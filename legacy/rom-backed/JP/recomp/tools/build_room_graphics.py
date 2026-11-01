#!/usr/bin/env python3
"""Generate a private, ROM-authenticated Japanese VRC6 metatile/attribute pack.
Requires v1 whole-room pack and its importer alongside this script.
"""
import argparse
import hashlib
import importlib.util
import struct
from pathlib import Path

ROM_SHA='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
STAGES=15
ROOMS=135
BASE=64+STAGES*24+ROOMS*8
BANKS=[8]*5+[7]*4+[6]*2+[7]+[6]*3
TILES_CPU_TABLE=0xd917
ATTR_CPU_TABLE=0xd935
FIXED_BANK30=0x3c010

def u16(b,offset): return struct.unpack_from('<H',b,offset)[0]
def rom_offset(cpu,bank):
    if not 0x8000<=cpu<0xc000: raise ValueError('Invalid tile/attribute pointer')
    return 0x10+bank*0x4000+cpu-0x8000

def source_table_ptr(rom,cpu,stage):
    return rom_offset(u16(rom,FIXED_BANK30+cpu-0xc000+2*stage),BANKS[stage])

def effective_group(s,a,r):
    if s==13 and a==0 and r in (0,1): return 2
    if s==13 and (a,r) in ((2,2),(3,0)): return 5
    if s==14 and (a,r)==(0,1): return 1
    return s

def build(rom,room_pack):
    if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=ROM_SHA:
        raise ValueError('Not the authenticated Japanese mapper-24 ROM')
    if (rom[6]>>4 | rom[7]&0xf0)!=24: raise ValueError('Wrong mapper')
    if (rom[0x3d090:0x3d0a5].hex() != 'b917d9855ab918d9855bb935d9855cb936d9855d60'):
        raise ValueError('Japanese $D080 tileset/attribute pointer opcodes differ')
    if room_pack[:8]!=b'CVROOM1\x00' or u16(room_pack,10)!=ROOMS or room_pack[24:56]!=bytes.fromhex(ROM_SHA):
        raise ValueError('Whole-room pack identity mismatch')
    if len(room_pack)<64+ROOMS*32 or struct.unpack_from('<I',room_pack,12)[0]!=len(room_pack):
        raise ValueError('Malformed whole-room pack')
    # Regenerate independently: fail if source room data and selected pack differ.
    module=Path(__file__).with_name('build_room_resources.py')
    spec=importlib.util.spec_from_file_location('jp_room_private',module)
    if spec is None or spec.loader is None: raise ValueError('Missing geometry importer')
    import sys
    mod=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=mod
    spec.loader.exec_module(mod)
    if room_pack!=mod.build(rom): raise ValueError('Room pack is stale or modified')

    groups=bytearray()
    payload=bytearray()
    limits=[]
    for group in range(STAGES):
        tiles=source_table_ptr(rom,TILES_CPU_TABLE,group)
        attrs=source_table_ptr(rom,ATTR_CPU_TABLE,group)
        span=attrs-tiles
        if span<=0 or span%16: raise ValueError('Invalid 16-byte metatile stride')
        count=span//16
        if not 1<=count<=256 or attrs+count>len(rom):
            raise ValueError('Invalid metatile palette extent')
        tile_at=BASE+len(payload)
        payload.extend(rom[tiles:attrs])
        attr_at=BASE+len(payload)
        payload.extend(rom[attrs:attrs+count])
        groups.extend(struct.pack('<BBHIIIII',group,BANKS[group],count,
                                  tiles,attrs,tile_at,attr_at,0))
        limits.append(count)
    rooms=bytearray()
    for index in range(ROOMS):
        base=64+index*32
        stage,area,room=room_pack[base:base+3]
        offset=struct.unpack_from('<I',room_pack,base+16)[0]
        size=struct.unpack_from('<H',room_pack,base+14)[0]
        grid_off=struct.unpack_from('<I',room_pack,base+20)[0]
        group=effective_group(stage,area,room)
        if max(room_pack[grid_off:grid_off+size])>=limits[group]:
            raise ValueError(f'Out-of-range metatile in {stage}/{area}/{room}')
        rooms.extend(struct.pack('<BBBBI',stage,area,room,group,offset))
    if len(rooms)!=ROOMS*8 or len(groups)!=STAGES*24: raise ValueError('Wrong population')
    body=groups+rooms+payload
    fnv=14695981039346656037
    for value in body: fnv=((fnv^value)*1099511628211)&0xffffffffffffffff
    header=bytearray(64)
    header[:8]=b'CVGFX01\x00'
    struct.pack_into('<HHHHII',header,8,1,STAGES,ROOMS,0,64+len(body),BASE)
    header[24:56]=bytes.fromhex(ROM_SHA)
    struct.pack_into('<Q',header,56,fnv)
    return bytes(header)+body,limits

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True)
    parser.add_argument('--rooms',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    pack,counts=build(args.rom.read_bytes(),args.rooms.read_bytes())
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_bytes(pack)
    print(f'PASS: 15 metatile groups, {sum(counts)} definitions, 135 room bindings, {len(pack)} private bytes')
if __name__=='__main__':main()