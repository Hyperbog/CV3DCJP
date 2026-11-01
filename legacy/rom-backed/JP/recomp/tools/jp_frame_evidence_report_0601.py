#!/usr/bin/env python3
"""JP0601: cross-validate actual PPU fetch CSV against ORIGINAL frame evidence.

This compares observed facts from the *same* original Japanese gameplay
frames, not a synthesized renderer. No ROM, pixel image, or game instructions
are required. Public CI uses invented records only.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import re
from collections import defaultdict
from pathlib import Path

from jp_chr_fetch_report_0600 import FIELDS as FETCH_FIELDS
from jp_chr_fetch_report_0600 import summarize as summarize_fetch

EVIDENCE_FIELDS = (
    "frame", "pattern_reads", "cache_mismatches", "invalid_reads",
    "bank_changes", "ppu_color_fnv64", "bg_opaque_fnv64",
    "scanline_state_fnv64", "nonzero_bg_pixels", "visible_bg_lines",
    "visible_sprite_lines",
)
HEX16 = re.compile(r"^[0-9a-f]{16}$")
HASH_FIELDS = ("ppu_color_fnv64", "bg_opaque_fnv64", "scanline_state_fnv64")
NUM_FIELDS = tuple(field for field in EVIDENCE_FIELDS if field not in HASH_FIELDS)


def read_evidence(text: str) -> list[dict]:
    reader = csv.DictReader(io.StringIO(text))
    if tuple(reader.fieldnames or ()) != EVIDENCE_FIELDS:
        raise ValueError("Wrong JP0601 frame evidence CSV schema")
    frames: list[dict] = []
    last = 0
    for row in reader:
        if set(row) != set(EVIDENCE_FIELDS):
            raise ValueError("Malformed JP0601 frame evidence record")
        if any(v is None or v.strip() != v or not v for v in row.values()):
            raise ValueError("Missing or malformed frame evidence value")
        try:
            numbers = {field: int(row[field], 10) for field in NUM_FIELDS}
        except ValueError as exc:
            raise ValueError("Non-decimal frame evidence counter") from exc
        f = numbers["frame"]
        if f <= last or f < 1 or (f - 1) % 60 != 0:
            raise ValueError("Frame IDs must be increasing sampled 1-in-60 frames")
        if any(value < 0 for value in numbers.values()):
            raise ValueError("Negative frame evidence counter")
        if (numbers["cache_mismatches"] + numbers["invalid_reads"] >
                numbers["pattern_reads"] or
                numbers["bank_changes"] > numbers["pattern_reads"] or
                numbers["nonzero_bg_pixels"] > 256 * 240 or
                numbers["visible_bg_lines"] > 240 or
                numbers["visible_sprite_lines"] > 240):
            raise ValueError("Physically impossible frame evidence counters")
        if any(not HEX16.fullmatch(row[name]) for name in HASH_FIELDS):
            raise ValueError("Unexpected fingerprint encoding")
        frames.append({**numbers, **{name: row[name] for name in HASH_FIELDS}})
        last = f
    if not frames:
        raise ValueError("No sampled gameplay frames in frame evidence")
    return frames


def analyze(fetch_text: str, evidence_text: str) -> dict:
    # Validate JP0600 schema/ranges before aggregating its per-scanline rows.
    fetch_summary = summarize_fetch(io.StringIO(fetch_text))
    frames = read_evidence(evidence_text)
    fetch_reader = csv.DictReader(io.StringIO(fetch_text))
    grouped = defaultdict(lambda: dict(pattern_reads=0, cache_mismatches=0,
                                       invalid_reads=0, bank_changes=0))
    for row in fetch_reader:
        frame = int(row["frame"])
        aggregate = grouped[frame]
        # JP0600 'fetches' counts valid reads; invalid attempts are separate.
        aggregate["pattern_reads"] += int(row["fetches"]) + int(row["invalid"])
        aggregate["cache_mismatches"] += int(row["asset_mismatch"])
        aggregate["invalid_reads"] += int(row["invalid"])
        aggregate["bank_changes"] += sum(
            int(row[f"ppu{page}_changes"]) for page in range(8)
        )
    joined = []
    for frame in frames:
        expected = grouped.pop(frame["frame"], dict(pattern_reads=0,
            cache_mismatches=0, invalid_reads=0, bank_changes=0))
        for field in ("pattern_reads", "cache_mismatches", "invalid_reads",
                      "bank_changes"):
            if frame[field] != expected[field]:
                raise ValueError(
                    f"Frame {frame['frame']} {field}: original PPU report "
                    f"{frame[field]} != sampled fetch report {expected[field]}"
                )
        joined.append(frame)
    if grouped:
        raise ValueError("Actual PPU fetch frames missing original-PPU evidence")
    return {
        "schema": "CVIII-JP-frame-evidence/v1",
        "sampled_original_ppu_frames": len(frames),
        "sampled_fetch_frames": fetch_summary["sampled_frames"],
        "pattern_fetches": sum(f["pattern_reads"] for f in frames),
        "cache_mismatches": sum(f["cache_mismatches"] for f in frames),
        "invalid_fetches": sum(f["invalid_reads"] for f in frames),
        "bank_transitions": sum(f["bank_changes"] for f in frames),
        "frames_with_background": sum(f["visible_bg_lines"] > 0 for f in frames),
        "frames_with_sprites": sum(f["visible_sprite_lines"] > 0 for f in frames),
        "fingerprints": [{
            "frame": f["frame"], "ppu_color_fnv64": f["ppu_color_fnv64"],
            "bg_opaque_fnv64": f["bg_opaque_fnv64"],
            "scanline_state_fnv64": f["scanline_state_fnv64"]
        } for f in frames],
        "fetch_source_byte_parity": fetch_summary["cache_parity_pass"] and
                                    all(f["invalid_reads"] == 0 for f in frames),
        "rendering_parity_proven": False,
        "limitation": "Original PPU frame fingerprints only; no independent "
                      "cache-rendered framebuffer or retail test oracle",
    }


def smoke() -> None:
    from jp_chr_fetch_report_0600 import invented_row

    def output(fields, rows):
        sio = io.StringIO()
        writer = csv.DictWriter(sio, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
        return sio.getvalue()

    row = invented_row(1, 12, 3, 6, 7, 1, count=5)
    fetch = output(FETCH_FIELDS, [row])
    evidence = {
        "frame": "1", "pattern_reads": "5", "cache_mismatches": "0",
        "invalid_reads": "0", "bank_changes": "1",
        "ppu_color_fnv64": "0123456789abcdef",
        "bg_opaque_fnv64": "fedcba9876543210",
        "scanline_state_fnv64": "1111111111111111",
        "nonzero_bg_pixels": "3", "visible_bg_lines": "20",
        "visible_sprite_lines": "10",
    }
    frame_csv = output(EVIDENCE_FIELDS, [evidence])
    parsed = analyze(fetch, frame_csv)
    assert parsed["sampled_original_ppu_frames"] == 1
    assert parsed["pattern_fetches"] == 5
    assert parsed["bank_transitions"] == 1
    assert parsed["fetch_source_byte_parity"]
    assert not parsed["rendering_parity_proven"]

    # A completely blank sample still gets a frame record (no CHR fetch rows).
    blank = dict(evidence, frame="61", pattern_reads="0", bank_changes="0")
    assert analyze(fetch, output(EVIDENCE_FIELDS, [evidence, blank]))[
        "sampled_original_ppu_frames"] == 2

    invalid = [
        (fetch, output(EVIDENCE_FIELDS, [dict(evidence, pattern_reads="6")])),
        (fetch, output(EVIDENCE_FIELDS, [dict(evidence, frame="2")])),
        (fetch, output(EVIDENCE_FIELDS, [dict(evidence, bg_opaque_fnv64="xx")])),
        (fetch, output(EVIDENCE_FIELDS, [dict(evidence, nonzero_bg_pixels="61441")])),
        (fetch, output(EVIDENCE_FIELDS, [dict(evidence, bank_changes="9")])),
        (fetch, output(EVIDENCE_FIELDS, [evidence, evidence])),
        (fetch, output(EVIDENCE_FIELDS, [dict(evidence, frame="61")])),
    ]
    for fcsv, ecsv in invalid:
        try:
            analyze(fcsv, ecsv)
        except ValueError:
            pass
        else:
            raise AssertionError("Inconsistent original-PPU evidence was accepted")
    print("PASS JP0601 paired fetch/PPU fingerprint CSV contract (ROM-free)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--smoke", action="store_true")
    action.add_argument("--fetch", type=Path, help="JP0600 original PPU fetch CSV")
    parser.add_argument("--evidence", type=Path, help="JP0601 original PPU frame CSV")
    parser.add_argument("--json", type=Path, help="Optional owner-private report output")
    parser.add_argument("--require-parity", action="store_true",
                        help="Fail on any cache-vs-original CHR-byte mismatch")
    args = parser.parse_args()
    if args.smoke:
        smoke()
        return
    if not args.evidence:
        parser.error("--evidence required with --fetch")
    result = analyze(args.fetch.read_text(encoding="utf-8"),
                     args.evidence.read_text(encoding="utf-8"))
    output = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    if args.require_parity and not result["fetch_source_byte_parity"]:
        parser.exit(1, "JP0601 detected mismatched original PPU CHR bytes\n")


if __name__ == "__main__":
    main()
