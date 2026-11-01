"""Inspect owner-local 5H-8 captures; do not distribute ROM-derived CHR sidecar."""
import argparse
import csv
import hashlib
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path


def check_fixed(path, frame_bytes, frame_ids):
    raw = path.read_bytes()
    size = 8 + frame_bytes
    if len(raw) != len(frame_ids) * size:
        raise ValueError(f"{path.name}: length mismatch {len(raw)} vs {len(frame_ids)*size}")
    for i, expected in enumerate(frame_ids):
        got = struct.unpack_from('<Q', raw, i*size)[0]
        if got != expected:
            raise ValueError(f"{path.name}: frame #{i} expected {expected}, got {got}")
    return len(raw)


def report(prefix):
    frames_path = Path(str(prefix)+'.frames.csv')
    with frames_path.open(newline='', encoding='utf-8-sig') as stream:
        frames = list(csv.DictReader(stream))
    if not frames:
        raise ValueError('Capture has no frames')
    ids = [int(row['frame']) for row in frames]
    if len(ids) != len(set(ids)) or ids != sorted(ids):
        raise ValueError('Frame IDs must be unique and ordered')
    files = {}
    for suffix, width in (('.oam.bin',256),('.lines.bin',1200),('.palette.bin',32)):
        p = Path(str(prefix)+suffix)
        files[p.name] = check_fixed(p,width,ids)
    rooms = defaultdict(lambda: {'frames':0,'first_frame':None,'last_frame':None})
    cpu_pc = Counter()
    for row in frames:
        label = f"{int(row['stage'])}/{int(row['area'])}/{int(row['room'])}"
        item = rooms[label]
        item['frames'] += 1
        item['first_frame'] = item['first_frame'] if item['first_frame'] is not None else int(row['frame'])
        item['last_frame'] = int(row['frame'])
        cpu_pc[int(row['pc'])] += 1
    ev_path = Path(str(prefix)+'.events.csv')
    with ev_path.open(newline='', encoding='utf-8-sig') as stream:
        events = list(csv.DictReader(stream))
    ev_counts = dict(Counter(e['event'] for e in events))
    gaps = [(a,b) for a,b in zip(ids,ids[1:]) if b!=a+1]
    scanline_flags = Counter()
    line_path = Path(str(prefix)+'.lines.bin')
    with line_path.open('rb') as stream:
        for _ in ids:
            block = stream.read(1208)
            if len(block)!=1208: raise ValueError('Incomplete PPU line block')
            for y in (0,32,47,48,64,119,120,121,239):
                v, fine, ctrl, mask = struct.unpack_from('<HBBB',block,8+5*y)
                scanline_flags[f'line_{y}_ctrl_{ctrl:02x}_mask_{mask:02x}'] += 1
    for suffix in ('.ram.bin','.chr-changes.bin','.ciram.bin'):
        p = Path(str(prefix)+suffix)
        if p.exists(): files[p.name] = p.stat().st_size
    provenance={}
    for path in [frames_path,ev_path]:
        provenance[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return {
        'format':'CVIII-5H8-EVIDENCE-v1',
        'frame_count':len(ids),'first_frame':ids[0],'last_frame':ids[-1],
        'missing_frame_ranges':gaps[:50], 'room_coverage':dict(rooms),
        'event_counts':ev_counts,'frequent_cpu_pc':cpu_pc.most_common(12),
        'scanline_ctrl_mask_samples':dict(scanline_flags),
        'binary_files_bytes':files,'sha256_manifest':provenance,
        'limitations':[
            'CPU registers sampled after each machine frame, not instruction traces',
            'PPU scanline descriptors sampled at dot 1; no PPU register write events',
            'CHR bytes are mapped at frame end, NOT historical per-scanline banks',
            'Full mode private CHR dumps are original-game-derived and should not be publicly redistributed',
            'No Mesen oracle comparison in this capture'
        ]
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--prefix',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    args=p.parse_args()
    result=report(args.prefix)
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(f"PASS: {result['frame_count']} frames; {len(result['room_coverage'])} rooms; "
          f"{len(result['missing_frame_ranges'])} recording gaps")
    print(args.out)


if __name__=='__main__':
    main()
