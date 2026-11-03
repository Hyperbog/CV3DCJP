#!/usr/bin/env python3
"""Run the original v7 physical-ROM span validator on canonical files."""
from pathlib import Path
import argparse, json, hashlib, sys

EXPECTED='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'

def verify(rom,ledger,root):
    if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=EXPECTED:raise ValueError('ROM identity mismatch')
    assert ledger['source']['sha256']==EXPECTED
    seen=bytearray(262144);status={};classified={'code':0,'data':0,'filler':0,'dual':0}
    for rec in ledger['spans']:
        pos=rec['prg_offset'];n=rec['length']
        if n<1 or pos<0 or pos+n>len(seen) or pos//16384 != rec['bank_16k'] or (pos+n-1)//16384!=rec['bank_16k']:raise ValueError('out of bounds/bank: '+rec['id'])
        if any(seen[pos:pos+n]):raise ValueError('overlap: '+rec['id'])
        if rom[16+pos:16+pos+n].hex()!=rec['expected_bytes_hex']:raise ValueError('ROM mismatch: '+rec['id'])
        seen[pos:pos+n]=b'\x01'*n;status[rec['status']]=status.get(rec['status'],0)+n
        role=rec['role'];classified[role]+=n
        if rec['status'] in ('IMPLEMENTED','VERIFIED','RETIRED'):
            for k in ('translation_file','test_report'):
                v=rec.get(k)
                if not v or not (root/v).is_file():raise ValueError('missing '+k+': '+rec['id'])
        if rec['status']=='RETIRED' and not (rec.get('approval') and rec.get('timing_proven') and rec.get('side_effects_proven') and rec.get('integration_proven') and not rec.get('unresolved_successors')):
            raise ValueError('not eligible for retirement: '+rec['id'])
    return {'spans':len(ledger['spans']),'classified':classified,'status_bytes':status,'retired':status.get('RETIRED',0),'total_classified':sum(seen)}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--rom',required=True,type=Path)
    a=parser.parse_args();root=Path(__file__).resolve().parents[1];ledger=json.loads((root/'docs/rom-translation/translation_ledger.json').read_text())
    try:print(json.dumps(verify(a.rom.read_bytes(),ledger,root),indent=2))
    except (AssertionError,ValueError,OSError) as e:print('FAIL:',e,file=sys.stderr);sys.exit(1)
