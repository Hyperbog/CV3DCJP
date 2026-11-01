#!/usr/bin/env python3
"""Privately derive a tiny original-JP-only $0A:$B7F7 native oracle pack."""
import argparse
import hashlib
import struct
from pathlib import Path

SOURCE_SHA = '958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
FRAGMENT_FNV = 0xCC5C6802DEB10072

def fnv64(data):
    h = 14695981039346656037
    for b in data:
        h = ((h ^ b) * 1099511628211) & 0xffffffffffffffff
    return h

def build(rom):
    if len(rom) != 393232 or rom[:4] != b'NES\x1a' or hashlib.sha256(rom).hexdigest() != SOURCE_SHA:
        raise ValueError('Not the authenticated Japanese mapper-24 ROM')
    bank0a = 0x10 + 0x0a * 0x4000
    bank0e = 0x10 + 0x0e * 0x4000
    data = rom[bank0a + 0x38c2:bank0a + 0x38cc] + rom[bank0e + 0x3e8b:bank0e + 0x3f3a]
    if len(data) != 185 or fnv64(data) != FRAGMENT_FNV:
        raise ValueError('Original source selector/records disagree')
    header = bytearray(64)
    header[:8] = b'CVN0660\x00'
    struct.pack_into('<HHQ', header, 8, 1, len(data), FRAGMENT_FNV)
    header[20:52] = bytes.fromhex(SOURCE_SHA)
    return bytes(header) + data

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    data = build(a.rom.read_bytes())
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_bytes(data)
    print('PASS: private 249-byte native verification pack from authenticated JP ROM')

if __name__ == '__main__':
    main()