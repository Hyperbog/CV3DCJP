#!/usr/bin/env python3
"""Review 5H-4 camera qualification without inferring room topology or Mesen parity."""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

REQUIRED = ('frame','stage','area','room','transition','qualified','source',
            'gate_5h4','gate_label','best_tested','best_matched',
            'best_x','best_y','best_top','anchor','chr_resolved')
VERIFIED = ((0,0,0),(0,1,0))

def load(path):
    with Path(path).open(newline='',encoding='utf-8-sig') as handle:
        reader=csv.DictReader(handle)
        if not reader.fieldnames or not set(REQUIRED).issubset(reader.fieldnames):
            raise ValueError('Requires 5H-4 camera trace; 5H-3 CSV lacks reason codes')
        rows=[]
        for line,item in enumerate(reader,2):
            try:
                row={k:int(item[k]) for k in REQUIRED if k!='gate_label'}
                row['gate_label']=item['gate_label']
            except (TypeError,ValueError) as exc:
                raise ValueError(f'Malformed trace line {line}') from exc
            if row['best_matched']>row['best_tested'] or row['qualified'] not in (0,1):
                raise ValueError(f'Invalid sample at line {line}')
            if rows and row['frame']<=rows[-1]['frame']:
                raise ValueError('Frames out of order or duplicated')
            rows.append(row)
    if not rows:raise ValueError('Trace is empty')
    return rows

def report(rows,span=35):
    reasons=Counter(r['gate_label'] for r in rows)
    first=next((i for i in range(1,len(rows))
        if (rows[i-1]['stage'],rows[i-1]['area'],rows[i-1]['room'])==VERIFIED[0]
        and (rows[i]['stage'],rows[i]['area'],rows[i]['room'])==VERIFIED[1]),None)
    vicinity=[]
    if first is not None:
        frame=rows[first]['frame']
        for r in rows:
            if abs(r['frame']-frame)>span:continue
            tested=r['best_tested']
            vicinity.append({'frame':r['frame'], 'room':[r['stage'],r['area'],r['room']],
                'gate':r['gate_label'],'qualified':bool(r['qualified']),
                'match':f"{r['best_matched']}/{tested}",
                'match_pct':round(100*r['best_matched']/tested,2) if tested else None,
                'candidate':[r['best_x'],r['best_y'],r['best_top']],
                'anchor':r['anchor'], 'chr_resolved':bool(r['chr_resolved'])})
    return {'frames':len(rows),'frame_range':[rows[0]['frame'],rows[-1]['frame']],
            'reason_counts':dict(sorted(reasons.items())),
            'qualified_frames':sum(r['qualified'] for r in rows),
            'atlas_presented':sum(r['source']==5 for r in rows),
            'first_verified_transition_frame':rows[first]['frame'] if first is not None else None,
            'first_verified_transition_window':vicinity,
            'unobserved_room_edges_promoted':0,
            'independent_mesen_parity':'NOT_EXERCISED'}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--native',required=True,type=Path)
    ap.add_argument('--out',type=Path)
    ap.add_argument('--span',type=int,default=35)
    args=ap.parse_args()
    if args.span<0 or args.span>600:ap.error('--span must be 0..600')
    result=report(load(args.native),args.span)
    body=json.dumps(result,indent=2)+'\n'
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True)
        args.out.write_text(body,encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='first_verified_transition_window'},indent=2))

if __name__=='__main__':main()
