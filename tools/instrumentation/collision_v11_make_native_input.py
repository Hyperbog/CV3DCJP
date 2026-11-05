#!/usr/bin/env python3
"""Make ROM-checked, UNVERIFIED frame-offset NESRecomp input candidates from MesenCE.

Mesen Lua inputPolled is not proven to share NESRecomp host's frame origin
or PPU phase. Each shift is only a candidate for *measured* live alignment.
Keep the original controller history, RAM states and ROM outside public Git.
"""
from __future__ import annotations
import argparse
import bisect
import csv
import hashlib
import json
from pathlib import Path

EXPECTED_ROM='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
FIELDS=('reset_epoch','mesen_frame','cycle','port1_mask','port2_mask')
NAMES=('A','B','SELECT','START','UP','DOWN','LEFT','RIGHT')

def decimal(value, name, limit=(1<<63)-1):
    if not value or not value.isascii() or not value.isdecimal():
        raise ValueError(f'{name}: expected unsigned decimal')
    n=int(value)
    if n>limit:raise ValueError(f'{name}: value exceeds range')
    return n

def buttons(mask):
    if not 0<=mask<=255:raise ValueError('invalid 8-bit button mask')
    return '+'.join(x for i,x in enumerate(NAMES) if mask&(1<<i)) or '-'

def load_epoch(path, epoch):
    rows=[];counts={}
    with open(path,'r',newline='',encoding='utf-8-sig') as handle:
        reader=csv.DictReader(handle,strict=True)
        if tuple(reader.fieldnames or ())!=FIELDS:
            raise ValueError('not a MesenCE v11.2 controller CSV')
        for n,record in enumerate(reader,2):
            if None in record or any(record.get(k) is None for k in FIELDS):
                raise ValueError(f'malformed controller row {n}')
            row={k:decimal(record[k],k,255 if k.endswith('_mask') else (1<<63)-1)
                 for k in FIELDS}
            counts[row['reset_epoch']]=counts.get(row['reset_epoch'],0)+1
            if row['reset_epoch']==epoch:rows.append(row)
    if not rows or rows[0]['mesen_frame']!=2:
        raise ValueError('selected reset epoch missing first frame-2 input poll')
    if rows[0]['port1_mask'] or rows[0]['port2_mask']:
        raise ValueError('input begins with active buttons: prior state unknown')
    for prev,cur in zip(rows,rows[1:]):
        if cur['mesen_frame']!=prev['mesen_frame']+1:
            raise ValueError('missing input poll or non-contiguous frame sequence')
        if cur['cycle']<=prev['cycle']:
            raise ValueError('non-monotone CPU clock')
    return rows,counts

def changes(rows,shift):
    if not 0<=shift<=8:raise ValueError('invalid frame shift')
    answer=[];previous=None
    for row in rows:
        state=(row['port1_mask'],row['port2_mask'])
        if state!=previous:
            answer.append((row['mesen_frame']-rows[0]['mesen_frame']+shift,*state))
        previous=state
    return answer

def serialize(changes_list,shift):
    lines=[f'# UNVERIFIED frame mapping shift={shift}',
           f'# Mesen frame 2 inputPoll corresponds to NESRecomp frame {shift}',
           '# Controller identity is source-mapped, not copied as raw bit order.',
           '0 -']
    prev=(0,0)
    for frame,p1,p2 in changes_list:
        if frame==0 and (p1,p2)==(0,0):continue
        if p1!=prev[0]:lines.append(f'{frame} {buttons(p1)}')
        if p2!=prev[1]:lines.append(f'{frame} 2:{buttons(p2)}')
        prev=(p1,p2)
    return '\n'.join(lines)+'\n'

def collision_window(path,rows):
    timeline=[x['cycle'] for x in rows]
    count=0;first=last=first_frame=last_frame=None
    with open(path,'r',newline='',encoding='utf-8-sig') as handle:
        reader=csv.DictReader(handle,strict=True)
        if not {'caller_pc','entry_cycle','return_cycle'}.issubset(
                reader.fieldnames or []):
            raise ValueError('collision trace is missing clock columns')
        for n,rec in enumerate(reader,1):
            a=decimal(rec['entry_cycle'],f'entry cycle {n}')
            b=decimal(rec['return_cycle'],f'return cycle {n}')
            if b<a:raise ValueError('decreasing collision cycle')
            index=bisect.bisect_right(timeline,a)-1
            poll=rows[index]['mesen_frame'] if index>=0 else None
            if first is None:first=a;first_frame=poll
            last=b;last_frame=poll;count+=1
    if not count or first<timeline[0] or last>timeline[-1]:
        raise ValueError('collision window not contained in post-reset inputs')
    return {'samples':count,'first_entry_cycle':first,'last_return_cycle':last,
            'mesen_last_input_poll_before_first_collision':first_frame,
            'mesen_last_input_poll_before_last_collision':last_frame,
            'cross_emulator_frame_alignment_proven':False}

def export(rom,controller,collision,outdir,epoch=1,shifts=(0,1,2)):
    if rom.stat().st_size!=393232 or hashlib.sha256(rom.read_bytes()).hexdigest()!=EXPECTED_ROM:
        raise ValueError('wrong original Japanese ROM')
    rows,counts=load_epoch(controller,epoch)
    run_count=len(changes(rows,0))
    outdir.mkdir(parents=True,exist_ok=True)
    targets=[outdir/f'cv3dcjp-native-input-UNVERIFIED-shift-{x}.txt' for x in shifts]
    manifest_path=outdir/'cv3dcjp-native-input-manifest.json'
    if any(x.exists() for x in [*targets,manifest_path]):
        raise FileExistsError('refusing to overwrite existing replay evidence')
    result={'schema':'cv3dcjp-v11.2-native-input-unverified-candidates',
            'source_rom_sha256':EXPECTED_ROM,
            'controller_sha256':hashlib.sha256(controller.read_bytes()).hexdigest(),
            'collision_sha256':hashlib.sha256(collision.read_bytes()).hexdigest(),
            'selected_reset_epoch':epoch,'epoch_counts':counts,
            'input_poll_rows':len(rows),'state_runs':run_count,
            'first_mesen_frame':rows[0]['mesen_frame'],
            'last_mesen_frame':rows[-1]['mesen_frame'],
            'port2_nonzero_polls':sum(x['port2_mask']!=0 for x in rows),
            'collision_window':collision_window(collision,rows),
            'start_phase_alignment_proven':False,
            'candidate_schedules':[]}
    for shift,target in zip(shifts,targets):
        data=serialize(changes(rows,shift),shift)
        target.write_text(data,encoding='ascii')
        result['candidate_schedules'].append({'shift':shift,'name':target.name,
            'sha256':hashlib.sha256(data.encode('ascii')).hexdigest(),
            'non_comment_lines':sum(not x.startswith('#') for x in data.splitlines())})
    manifest_path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('rom','controller','collision','outdir'):
        p.add_argument('--'+name,required=True,type=Path)
    p.add_argument('--epoch',type=int,default=1)
    p.add_argument('--shifts',nargs='+',type=int,default=[0,1,2])
    args=p.parse_args()
    r=export(args.rom,args.controller,args.collision,args.outdir,args.epoch,tuple(args.shifts))
    print(f"Generated {len(r['candidate_schedules'])} UNVERIFIED replay candidates, "
          f"{r['input_poll_rows']} input polls, {r['state_runs']} runs")
    return 0

if __name__=='__main__':
    raise SystemExit(main())
