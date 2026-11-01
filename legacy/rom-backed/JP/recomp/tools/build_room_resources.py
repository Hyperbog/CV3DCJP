#!/usr/bin/env python3
"""Build a PRIVATE versioned whole-room pack from exact Japanese mapper24 ROM.

All tile bytes remain owner-local. No TAS/emulator dependency. Output is a
read-only geometry/CHR association asset, not a substitute for gameplay state.
"""
import argparse
import hashlib
import struct
from pathlib import Path

ROM_SHA = '958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
REGIONS = [(0x20011,0x20451),(0x20DF2,0x2141A),(0x219BF,0x22120),
           (0x22692,0x22C7D),(0x23321,0x23737),(0x1C011,0x1C459),
           (0x1CA0F,0x1CD94),(0x1D17F,0x1D78E),(0x1DDEE,0x1E3BE),
           (0x18011,0x182D0),(0x18820,0x18EDB),(0x1EA1E,0x1EDFA),
           (0x19FDB,0x1A2FB),(0x1A95B,0x1AC9A),(0x1B2C7,0x1B592)]
SIGNATURES = [(0x3C910,'a4344c0fc9'),
              (0x3D459,'a5340aa8b9f9d88508b9fad88509a5350aa8b108850ac8b108850ba5360a850ea8c8b10a854dc8b10a854e60'),
              (0x3D053,'a5340a850ca8b953d98508b954d98509a5350a850da8b108850ac8b108850ba436b10a8565'),
              (0x00023,'a5340aa8b95f808508b960808509a5350a0aa8b108850ac8b108850ba5360aa8b10a853dc8b10a853ea5350a0aa8c8c8b108850ac8b108850ba5360aa8b10a853ac8b10a853b202be34c47e3')]
MAGIC = b'CVROOM1\x00'
RECORDS = 135
RECORD_SIZE = 32
DATA_START = 64 + RECORDS * RECORD_SIZE


def word(raw, pos):
    assert 0 <= pos < len(raw)-1
    return raw[pos] | raw[pos+1] << 8


def fixed(cpu):
    assert 0xC000 <= cpu < 0xE000
    return 0x3C010 + cpu - 0xC000


def first(cpu):
    assert 0x8000 <= cpu < 0xA000
    return 0x10 + cpu - 0x8000


def switched(cpu, bank):
    assert 0x8000 <= cpu < 0xC000 and 0 <= bank < 16
    return 0x10 + 0x4000 * bank + cpu - 0x8000


def bank_for(stage, area, room, banks):
    if stage == 13:
        if area == 0 and room in (0,1): return banks[2]
        if (area,room) in ((2,2),(3,0)): return banks[5]
    if stage == 14 and area == 0 and room == 1: return banks[1]
    return banks[stage]


def row_major(data, screens, vertical):
    width, height = (8, screens*8) if vertical else (screens*8, 6)
    return bytes(data[((y//8)*64 + (y%8)*8+x) if vertical
                      else ((x//8)*48 + y*8+x%8)]
                 for y in range(height) for x in range(width))


def fnv(data):
    n = 14695981039346656037
    for b in data:
        n = ((n ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return n


def build(raw):
    if len(raw)!=393232 or raw[:4]!=b'NES\x1a' or hashlib.sha256(raw).hexdigest()!=ROM_SHA:
        raise ValueError('Expected authenticated JP Akumajou Densetsu ROM')
    if (raw[6] >> 4 | (raw[7]&0xF0))!=24:
        raise ValueError('Expected Japanese mapper 24')
    for off,sig in SIGNATURES:
        if raw[off:off+len(bytes.fromhex(sig))]!=bytes.fromhex(sig):
            raise ValueError(f'Japanese selector code differs at {off:05X}')
    banks=list(raw[0x3C95B:0x3C95B+15]); assert banks==[8]*5+[7]*4+[6]*2+[7]+[6]*3
    records=[]
    unique={}
    data=bytearray()
    for stage,(region_begin,region_end) in enumerate(REGIONS):
        main_bank=banks[stage]
        stage_root=switched(word(raw,0x3D909+stage*2),main_bank)
        if stage_root!=region_begin:
            raise ValueError(f'Stage {stage}: pointer origin mismatch')
        first_area=switched(word(raw,stage_root),main_bank)
        area_count=(first_area-stage_root)//2
        if not 1<=area_count<=16 or first_area-stage_root!=area_count*2:
            raise ValueError('Invalid area table')
        metadata_base=fixed(word(raw,0x3D963+stage*2))
        chr_area_root=first(word(raw,first(0x805F)+stage*2))
        for area in range(area_count):
            descriptor=switched(word(raw,stage_root+2*area),main_bank)
            rooms=raw[descriptor]+1
            if not 1<=rooms<=16: raise ValueError('Bad room count')
            meta_area=fixed(word(raw,metadata_base+2*area))
            bg_ptr=first(word(raw,chr_area_root+4*area))
            spr_ptr=first(word(raw,chr_area_root+4*area+2))
            for room in range(rooms):
                effective=bank_for(stage,area,room,banks)
                offset=switched(word(raw,descriptor+1+room*2),effective)
                if not any(begin<=offset<end for begin,end in REGIONS):
                    raise ValueError('Layout outside known ROM regions')
                metadata=raw[meta_area+room]
                vertical=bool(metadata&0x80)
                screens=raw[offset]+1
                if not 1<=screens<=16: raise ValueError('Invalid screen count')
                size=screens*(64 if vertical else 48)
                if offset+1+size>next(end for begin,end in REGIONS if begin<=offset<end):
                    raise ValueError('Layout exceeds data region')
                width,height=(8,screens*8) if vertical else (screens*8,6)
                cells=row_major(raw[offset+1:offset+1+size],screens,vertical)
                assert len(cells)==width*height
                if offset in unique:
                    at,prior=unique[offset]
                    if prior!=cells: raise ValueError('Aliased layout differs')
                else:
                    at=DATA_START+len(data)
                    unique[offset]=(at,cells)
                    data.extend(cells)
                chr_banks=[raw[bg_ptr+room*2],raw[bg_ptr+room*2+1],
                           raw[spr_ptr+room*2],raw[spr_ptr+room*2+1]]
                if any(x>=128 for x in chr_banks):
                    raise ValueError('CHR bank out of bounds')
                rec=bytearray(RECORD_SIZE)
                rec[:9]=bytes((stage,area,room,metadata,screens,*chr_banks))
                struct.pack_into('<HHHII',rec,10,width,height,len(cells),offset,at)
                records.append(rec)
    if len(records)!=135 or len(unique)!=130 or sum(1 for x in records if x[0]==0)!=8:
        raise ValueError('Japanese room selector population mismatch')
    # Never silently overwrite an original-source byte interval with invalid
    # dimensions; the three known trailing gaps remain outside each grid.
    if sum(len(cells) for _,cells in unique.values()) != len(data):
        raise ValueError('Non-unique room data allocation')
    payload=b''.join(records)+data
    header=bytearray(64)
    header[:8]=MAGIC
    struct.pack_into('<HHIII',header,8,1,RECORDS,64+len(payload),DATA_START,len(data))
    header[24:56]=bytes.fromhex(ROM_SHA)
    struct.pack_into('<Q',header,56,fnv(payload))
    return bytes(header)+payload


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    a=p.parse_args()
    packed=build(a.rom.read_bytes())
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_bytes(packed)
    print(f'PASS JP ROM: 135 room references, 130 distinct layouts; {len(packed)} private bytes')

if __name__=='__main__':main()