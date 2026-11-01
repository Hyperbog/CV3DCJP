#!/usr/bin/env python3
"""JP0606 validate owner-private PPU nametable and attribute fetch samples.

The trace captures the original PPU's actual /RD-returned bytes. It has no
replacement-renderer output and cannot by itself prove pixel parity.
Synthetic CI requires no copyrighted media.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import re
from collections import defaultdict
from pathlib import Path

FIELDS = ("frame", "scanline", "tile_reads", "attribute_reads",
          "ciram_reads", "chrrom_reads", "invalid", "first_tile",
          "first_attribute", "bus_fnv64")
HEX16 = re.compile(r"^[a-f0-9]{16}$")


def summarize(contents: str, frame_evidence: str | None = None) -> dict:
    reader = csv.DictReader(io.StringIO(contents))
    if tuple(reader.fieldnames or ()) != FIELDS:
        raise ValueError("Unrecognized JP0606 original-PPU nametable trace")
    frame_sums = defaultdict(lambda: {"tile_reads": 0, "attribute_reads": 0,
                                     "ciram_reads": 0, "chrrom_reads": 0,
                                     "invalid": 0, "scanlines": 0})
    last = (0, -1)
    rows = 0
    empty_frames: set[int] = set()
    for row in reader:
        if set(row) != set(FIELDS) or any(v is None or not v or v.strip() != v for v in row.values()):
            raise ValueError("Incomplete JP0606 nametable audit record")
        try:
            numbers = {k: int(row[k], 10) for k in FIELDS if k != "bus_fnv64"}
        except ValueError as exc:
            raise ValueError("Non-decimal PPU nametable fetch field") from exc
        frame, line = numbers["frame"], numbers["scanline"]
        if frame < 1 or (frame - 1) % 60 or not 0 <= line < 312 or (frame, line) <= last:
            raise ValueError("Nametable samples must be strictly ordered 1-in-60 frames")
        last = (frame, line)
        total = numbers["tile_reads"] + numbers["attribute_reads"]
        if any(numbers[k] < 0 for k in ("tile_reads", "attribute_reads", "ciram_reads",
                                       "chrrom_reads", "invalid")):
            raise ValueError("Negative nametable audit counters")
        if total != numbers["ciram_reads"] + numbers["chrrom_reads"]:
            raise ValueError("Source counter mismatch in nametable PPU trace")
        if not total and not numbers["invalid"]:
            if line != 0:
                raise ValueError("Empty PPU sample placeholder must be on scanline 0")
            empty_frames.add(frame)
        if (not -1 <= numbers["first_tile"] <= 255 or
            not -1 <= numbers["first_attribute"] <= 255 or
            (numbers["first_tile"] == -1) != (numbers["tile_reads"] == 0) or
            (numbers["first_attribute"] == -1) != (numbers["attribute_reads"] == 0) or
            not HEX16.fullmatch(row["bus_fnv64"])):
            raise ValueError("Invalid original PPU tile/attribute value or hash")
        aggregate = frame_sums[frame]
        for key in ("tile_reads", "attribute_reads", "ciram_reads",
                    "chrrom_reads", "invalid"):
            aggregate[key] += numbers[key]
        aggregate["scanlines"] += 1
        rows += 1
        if rows > 100000:
            raise ValueError("Too many PPU trace rows")
    if not rows:
        raise ValueError("No sampled nametable or attribute reads")
    frames = sorted(frame_sums)
    if any(frame_sums[f]["scanlines"] != 1 for f in empty_frames):
        raise ValueError("Empty frame marker may not accompany actual PPU fetches")
    if frame_evidence is not None:
        from jp_frame_evidence_report_0601 import read_evidence
        ref_frames = [r["frame"] for r in read_evidence(frame_evidence)]
        if frames != ref_frames:
            raise ValueError("Nametable trace frames do not match original PPU evidence frames")
    totals = {key: sum(frame_sums[f][key] for f in frames) for key in
              ("tile_reads", "attribute_reads", "ciram_reads", "chrrom_reads", "invalid")}
    return {
        "schema": "CVIII-JP-NT-bus-fetch/v1",
        "sampled_frames": len(frames),
        "sampled_frame_ids": frames,
        "scanline_records": rows,
        **totals,
        "ppu_fetch_counters_consistent": True,
        "all_reads_from_ciram": totals["chrrom_reads"] == 0,
        "no_invalid_bus_records": totals["invalid"] == 0,
        "frame_totals": {str(f): frame_sums[f] for f in frames},
        "pixel_parity_proven": False,
        "note": "Sampled nametable/attribute bus values only; frame pixels remain authoritative",
    }


def fixture(frame: int = 1, line: int = 40, rom: int = 0) -> dict[str, str]:
    return {"frame": str(frame), "scanline": str(line),
            "tile_reads": "3", "attribute_reads": "1",
            "ciram_reads": str(4 - rom), "chrrom_reads": str(rom),
            "invalid": "0", "first_tile": "17",
            "first_attribute": "240", "bus_fnv64": "a3" * 8}


def encode(rows: list[dict[str, str]]) -> str:
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=FIELDS, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue()


def smoke() -> None:
    good = encode([fixture(), fixture(61, 41, 1)])
    result = summarize(good)
    empty = fixture(121, 0)
    empty.update(tile_reads="0", attribute_reads="0",
                 ciram_reads="0", chrrom_reads="0",
                 first_tile="-1", first_attribute="-1")
    assert summarize(encode([fixture(), empty]))["sampled_frames"] == 2
    assert result["sampled_frames"] == 2 and result["tile_reads"] == 6
    assert result["attribute_reads"] == 2 and result["chrrom_reads"] == 1
    assert not result["all_reads_from_ciram"] and not result["pixel_parity_proven"]
    broken = (
        encode([fixture(61), fixture(1)]),
        encode([fixture(), fixture()]),
        encode([fixture(2)]),
        encode([dict(fixture(), ciram_reads="5")]),
        encode([dict(fixture(), first_attribute="-1")]),
        encode([dict(fixture(), first_tile="256")]),
        encode([dict(fixture(), bus_fnv64="not-a-hash")]),
        encode([dict(fixture(), scanline="340")]),
        encode([dict(empty, scanline="41")]),
        encode([empty, fixture(121, 1)]),
        good.replace("chrrom_reads", "surprise_field", 1),
    )
    for text in broken:
        try:
            summarize(text)
        except ValueError:
            continue
        raise AssertionError("Malformed nametable trace accepted")
    print("PASS JP0606 NT/attribute traces: frames, sources, hashes and integrity gates")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--smoke", action="store_true")
    g.add_argument("--csv", type=Path)
    p.add_argument("--evidence", type=Path, help="Cross-check JP0601 original PPU sampled frame IDs")
    p.add_argument("--json", type=Path)
    a = p.parse_args()
    if a.smoke:
        smoke()
        return
    evidence = a.evidence.read_text(encoding="utf-8") if a.evidence else None
    result = summarize(a.csv.read_text(encoding="utf-8"), evidence)
    if a.json:
        a.json.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k != "frame_totals"}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
