#!/usr/bin/env python3
"""JP0614: verify owner-local audio capture identity and save/load epoch boundaries.

Never joins requests to registers or treats proximity as source ownership.
This derives epochs from exact cumulative CSV row indices, not cycle order.
"""
from __future__ import annotations
import argparse
import csv
from hashlib import sha256
import io
import json
from pathlib import Path
import re
import jp_audio_bus_report_0610 as chip
import tempfile

REQUESTS = ('cycle','pc','command','x','y','s','p','tracked_prg_bank','stack_next_lo','stack_next_hi')
REGISTERS = ('cycle','chip','voice','register','value','previous','delta_cycles','rapid_rewrite')
EPOCHS = ('session_id','epoch','event','cycle','request_rows','register_rows')
HEX = re.compile(r'[a-f0-9]{32}\Z')
DIGEST = re.compile(r'[a-f0-9]{64}\Z')
DEC = re.compile(r'[0-9]+\Z')
BYTE = re.compile(r'[A-Fa-f0-9]{2}\Z')
MAX_ROWS = 1_000_000


def rows(payload: bytes, header: tuple[str,...]) -> list[dict[str,str]]:
    try: text = payload.decode('utf-8-sig')
    except UnicodeError as exc: raise ValueError('Invalid UTF-8 CSV') from exc
    rd=csv.DictReader(io.StringIO(text))
    if tuple(rd.fieldnames or ()) != header: raise ValueError('CSV schema mismatch')
    out=[]
    for row in rd:
        if len(out) >= MAX_ROWS or set(row)!=set(header) or any(
            v is None or not v or v.strip()!=v for v in row.values()
        ): raise ValueError('Malformed/overlong capture CSV')
        out.append(row)
    return out


def number(raw: str, max_value: int=(1<<64)-1) -> int:
    if not DEC.fullmatch(raw): raise ValueError('Non-decimal capture value')
    v=int(raw)
    if v > max_value: raise ValueError('Capture value outside bounds')
    return v


def verify(folder: Path) -> dict:
    manifest_path=folder/'CVIII-Audio-Session.json'
    obj=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    if obj.get('schema')!='CVIII-JP-audio-session/v1' or not HEX.fullmatch(obj.get('session_id','')):
        raise ValueError('Invalid run manifest identity')
    if set(obj)!= {'schema','session_id','files'} or set(obj['files'])!={'requests','registers','epochs'}:
        raise ValueError('Incomplete/private run manifest')
    names={'requests':'CVIII-Sound-Requests.csv', 'registers':'CVIII-Audio-Registers.csv',
           'epochs':'CVIII-Audio-Epochs.csv'}
    headers={'requests':REQUESTS,'registers':REGISTERS,'epochs':EPOCHS}
    data={}
    for key,name in names.items():
        desc=obj['files'][key]
        if set(desc)!={'name','sha256'} or desc['name']!=name or not DIGEST.fullmatch(desc['sha256']):
            raise ValueError('Unexpected capture path/hash field')
        buf=(folder/name).read_bytes()
        if sha256(buf).hexdigest()!=desc['sha256']:
            raise ValueError('Cross-file session digest mismatch: '+key)
        data[key]=rows(buf,headers[key])
    epochs=data['epochs']
    if not epochs or len(epochs)>10000: raise ValueError('Missing/excessive epoch markers')
    previous=(0,0)
    boundaries=[]
    for i,row in enumerate(epochs):
        if (row['session_id']!=obj['session_id'] or number(row['epoch'],10000)!=i or
            row['event'] not in ('power_on','state_loaded') or
            (i==0 and row['event']!='power_on')):
            raise ValueError('Missing/misordered session epoch')
        r=number(row['request_rows'],MAX_ROWS)
        w=number(row['register_rows'],MAX_ROWS)
        clock=number(row['cycle'])
        if r<previous[0] or w<previous[1] or r>len(data['requests']) or w>len(data['registers']):
            raise ValueError('Epoch row indices out of bounds/order')
        if i==0 and (r,w)!=(0,0): raise ValueError('Missing initial empty epoch')
        boundaries.append((clock,r,w,row['event']))
        previous=(r,w)
    segments=[]
    for i,(clock,r,w,event) in enumerate(boundaries):
        rn=boundaries[i+1][1] if i+1<len(boundaries) else len(data['requests'])
        wn=boundaries[i+1][2] if i+1<len(boundaries) else len(data['registers'])
        for key,start,stop in (('requests',r,rn),('registers',w,wn)):
            last=clock
            for record in data[key][start:stop]:
                tick=number(record['cycle'])
                if tick<last: raise ValueError('Nonmonotonic cycle within epoch: '+key)
                last=tick
                if key=='requests':
                    if record['pc'].upper()!='E249' or any(not BYTE.fullmatch(record[field]) for field in REQUESTS[2:]):
                        raise ValueError('Bad original request PC/register field')
                else:
                    # Register is four hexadecimal digits, never decimal.
                    if not re.fullmatch(r'[0-9A-Fa-f]{4}',record['register']):
                        raise ValueError('Malformed APU/VRC6 register address')
                    addr=int(record['register'],16)
                    if chip.expected(addr)!=(record['chip'],record['voice']):
                        raise ValueError('Invalid audio chip or voice label')
                    if (not BYTE.fullmatch(record['value']) or not BYTE.fullmatch(record['previous']) or
                        record['chip'] not in ('2a03','vrc6') or record['rapid_rewrite'] not in ('0','1')):
                        raise ValueError('Malformed register observation')
                    number(record['delta_cycles'])
        segments.append({'epoch':i,'event':event,'first_cycle':clock,
                         'request_rows':rn-r,'register_rows':wn-w})
    return {'schema':'CVIII-JP-audio-epoch-verification/v1',
            'session_id':obj['session_id'],'epochs':segments,
            'validated_requests':len(data['requests']),
            'validated_register_writes':len(data['registers']),
            'file_digests_verified':True,
            'row_boundary_integrity_verified':True,
            'causal_audio_ownership_proven':False,
            'reference_mesen_parity_verified':False}


def smoke() -> None:
    with tempfile.TemporaryDirectory(prefix='jp0614-') as root:
        path=Path(root)
        files={
            'requests':('CVIII-Sound-Requests.csv', REQUESTS, [
                ('100','E249','6B','00','00','FD','24','0A','10','B5'),
                ('260','E249','10','00','00','FD','24','0A','10','B5'),
                ('20','E249','6F','00','00','FD','24','0A','10','B5')]),
            'registers':('CVIII-Audio-Registers.csv', REGISTERS, [
                ('170','2a03','pulse1','4000','30','00','0','0'),
                ('25','vrc6','pulse1','9000','00','00','0','0')]),
            'epochs':('CVIII-Audio-Epochs.csv', EPOCHS, [
                ('0123456789abcdef0123456789abcdef','0','power_on','0','0','0'),
                ('0123456789abcdef0123456789abcdef','1','state_loaded','10','2','1')]),
        }
        manifest={'schema':'CVIII-JP-audio-session/v1',
                  'session_id':'0123456789abcdef0123456789abcdef','files':{}}
        for key,(name,header,values) in files.items():
            with (path/name).open('w',encoding='utf-8',newline='') as f:
                writer=csv.writer(f,lineterminator='\n');writer.writerow(header);writer.writerows(values)
            manifest['files'][key]={'name':name,'sha256':sha256((path/name).read_bytes()).hexdigest()}
        manifest_path=path/'CVIII-Audio-Session.json'
        manifest_path.write_text(json.dumps(manifest),encoding='utf-8')
        result=verify(path)
        assert len(result['epochs'])==2 and result['validated_requests']==3
        assert result['epochs'][1]['request_rows']==1
        old=(path/'CVIII-Sound-Requests.csv').read_bytes()
        (path/'CVIII-Sound-Requests.csv').write_bytes(old+b'\n')
        try: verify(path)
        except ValueError: pass
        else: raise AssertionError('Accepted stale or edited capture')
        (path/'CVIII-Sound-Requests.csv').write_bytes(old)
        tampered=manifest['session_id'];manifest['session_id']='f'*32
        manifest_path.write_text(json.dumps(manifest),encoding='utf-8')
        try: verify(path)
        except ValueError: pass
        else: raise AssertionError('Accepted mismatched session identity')
        manifest['session_id']=tampered
        print('PASS JP0614 2 epochs (including cycle rollback), hashes and rejected capture mixing')


def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--smoke',action='store_true')
    ap.add_argument('--dir',type=Path)
    ap.add_argument('--json',type=Path)
    args=ap.parse_args()
    if args.smoke: smoke();return
    if args.dir is None: ap.error('--dir required')
    out=verify(args.dir)
    if args.json: args.json.write_text(json.dumps(out,indent=2,sort_keys=True)+'\n',encoding='utf-8')
    print(json.dumps(out,indent=2,sort_keys=True))

if __name__=='__main__': main()
