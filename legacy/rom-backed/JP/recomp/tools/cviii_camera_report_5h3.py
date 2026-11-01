#!/usr/bin/env python3
"""Summarize owner-local CVIII room-camera observations; never infer topology.

Optional --mesen-reference accepts a deliberately matched CSV schema from an
independent Mesen capture. No reference means NO independent parity claim.
"""
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

REQUIRED = ("frame", "stage", "area", "room", "transition", "camera_ram",
            "ppu_x", "ppu_y", "ppu_bg_table", "qualified", "camera_x",
            "camera_y", "top", "tested", "matched", "world_placed", "source")
CAMERA_FIELDS = ("stage", "area", "room", "camera_ram", "ppu_x", "ppu_y", "ppu_bg_table")


def load(path):
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or not set(REQUIRED).issubset(reader.fieldnames):
            raise ValueError(f"Missing 5H3 camera-trace columns: {path}")
        records = []
        seen = set()
        for i, data in enumerate(reader, start=2):
            try:
                row = {k: int(data[k]) for k in REQUIRED}
            except (TypeError, ValueError) as ex:
                raise ValueError(f"Invalid camera trace record at {path}:{i}") from ex
            if row["frame"] in seen:
                raise ValueError(f"Duplicate frame {row['frame']} at {path}:{i}")
            seen.add(row["frame"])
            if row["matched"] > row["tested"] or row["qualified"] not in (0, 1):
                raise ValueError(f"Invalid camera match counts at {path}:{i}")
            records.append(row)
        if not records:
            raise ValueError(f"No camera observations in {path}")
        if any(a["frame"] >= b["frame"] for a, b in zip(records, records[1:])):
            raise ValueError(f"Frame order not strictly increasing: {path}")
        return records


def key(row):
    return (row["stage"], row["area"], row["room"])


def analyze(rows, reference=None):
    transitions = Counter()
    unverified = []
    for old, new in zip(rows, rows[1:]):
        if key(old) == key(new):
            if new["transition"]:
                raise ValueError(f"Spurious transition at frame {new['frame']}")
            continue
        if not new["transition"]:
            raise ValueError(f"Missing transition marker at frame {new['frame']}")
        a, b = key(old), key(new)
        transitions[(a, b)] += 1
    # Only the original independently substantiated JP0803 edge is verified.
    KNOWN = {((0, 0, 0), (0, 1, 0)), ((0, 1, 0), (0, 0, 0))}
    for (a, b), count in sorted(transitions.items()):
        if (a, b) not in KNOWN:
            unverified.append({"from": list(a), "to": list(b),
                               "observed_transitions": count,
                               "topology": "UNVERIFIED; DO NOT PLACE"})
    output = {
        "frames": len(rows),
        "frame_range": [rows[0]["frame"], rows[-1]["frame"]],
        "qualified_frames": sum(row["qualified"] for row in rows),
        "atlas_presented": sum(row["source"] == 5 for row in rows),
        "observed_connections": [
            {"from": list(a), "to": list(b), "count": count,
             "status": "SOURCE_VERIFIED_JP0803" if (a, b) in KNOWN
                       else "OBSERVED_ONLY_NOT_PROVEN"}
            for (a, b), count in sorted(transitions.items())
        ],
        "unverified_connections": unverified,
        "independent_mesen_parity": "NOT_EXERCISED",
    }
    if reference is not None:
        native = {r["frame"]: r for r in rows}
        ref = {r["frame"]: r for r in reference}
        overlap = sorted(native.keys() & ref.keys())
        mismatches = []
        for frame in overlap:
            different = [f for f in CAMERA_FIELDS if native[frame][f] != ref[frame][f]]
            if different:
                mismatches.append({"frame": frame, "fields": different})
                if len(mismatches) >= 20:
                    break
        output["independent_mesen_parity"] = (
            "NO_SHARED_FRAMES" if not overlap else
            "MISMATCH" if mismatches else "PASS_MATCHED_COLUMNS_ONLY")
        output["matched_reference_frames"] = len(overlap)
        output["camera_mismatches_first20"] = mismatches
        output["reference_requires_independent_capture"] = True
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--mesen-reference", type=Path,
                        help="Independently captured Mesen CSV in identical 5H3 format")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    native = load(args.native)
    other = load(args.mesen_reference) if args.mesen_reference else None
    result = analyze(native, other)
    body = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(body, encoding="utf-8")
    print(body, end="")
    if other and result["independent_mesen_parity"] != "PASS_MATCHED_COLUMNS_ONLY":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
