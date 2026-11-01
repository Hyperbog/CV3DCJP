#!/usr/bin/env python3
"""JP0600: summarize private read-only physical CHR fetch audit CSVs.

This tool reads metadata from a private original-JP gameplay trace but
requires neither the retail ROM nor its copyrighted CHR bytes. Its public
self-test uses invented observations. Invalid/malformed traces fail closed.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import tempfile
from collections import Counter
from pathlib import Path

PAGE_COUNT = 8
PHYSICAL_PAGES = 128
SCHEMA = "CVIII-JP-CHR-fetch-audit/v1"
FIELDS = ("frame", "scanline", "fetches", "asset_mismatch", "invalid") + tuple(
    f"ppu{p}_{suffix}" for p in range(PAGE_COUNT)
    for suffix in ("first", "last", "changes")
)


def summarize(fp) -> dict:
    reader = csv.DictReader(fp)
    if tuple(reader.fieldnames or ()) != FIELDS:
        raise ValueError("Not a JP0600 CHR fetch CSV (unexpected column schema)")
    read_lines = 0
    frames: set[int] = set()
    seen: set[tuple[int, int]] = set()
    previous = (0, -1)
    totals = Counter()
    pages = [set() for _ in range(PAGE_COUNT)]
    changing_lines = 0
    failing_lines = 0
    for row in reader:
        if set(row) != set(FIELDS) or any(v is None or not v.strip() for v in row.values()):
            raise ValueError(f"Malformed CHR fetch row at data line {reader.line_num}")
        try:
            values = {field: int(row[field], 10) for field in FIELDS}
        except ValueError as exc:
            raise ValueError(f"Non-decimal CHR fetch field on line {reader.line_num}") from exc
        frame, scanline = values["frame"], values["scanline"]
        position = (frame, scanline)
        if (frame < 1 or (frame - 1) % 60 or not 0 <= scanline < 312
                or position <= previous or position in seen):
            raise ValueError(f"Invalid, duplicate, unordered or unsampled line: {position}")
        previous = position
        seen.add(position)
        fetches = values["fetches"]
        bad = values["asset_mismatch"]
        invalid = values["invalid"]
        if min(fetches, bad, invalid) < 0 or bad > fetches or fetches + invalid == 0:
            raise ValueError(f"Invalid fetch/mismatch counts on line {reader.line_num}")
        perline_changes = 0
        active = 0
        for page in range(PAGE_COUNT):
            first = values[f"ppu{page}_first"]
            last = values[f"ppu{page}_last"]
            changes = values[f"ppu{page}_changes"]
            if changes < 0 or changes > fetches:
                raise ValueError(f"Invalid bank-change count on line {reader.line_num}")
            if first == last == -1:
                if changes:
                    raise ValueError(f"Bank changes without a bank on line {reader.line_num}")
                continue
            if not (0 <= first < PHYSICAL_PAGES and
                    0 <= last < PHYSICAL_PAGES):
                raise ValueError(f"CHR physical bank out of range at line {reader.line_num}")
            if first != last and changes == 0:
                raise ValueError(f"Changed bank without transition at line {reader.line_num}")
            pages[page].update((first, last))
            active += 1
            perline_changes += changes
        if fetches and not active:
            raise ValueError(f"Pattern reads without an active PPU page at line {reader.line_num}")
        if not fetches and active:
            raise ValueError(f"Active PPU page without pattern reads at line {reader.line_num}")
        read_lines += 1
        frames.add(frame)
        totals["pattern_fetches"] += fetches
        totals["cache_mismatches"] += bad
        totals["invalid_fetches"] += invalid
        totals["bank_transitions"] += perline_changes
        if perline_changes:
            changing_lines += 1
        if bad or invalid:
            failing_lines += 1
    return {
        "schema": SCHEMA,
        "sampled_frames": len(frames),
        "observed_scanlines": read_lines,
        "pattern_fetches": totals["pattern_fetches"],
        "cache_mismatches": totals["cache_mismatches"],
        "invalid_fetches": totals["invalid_fetches"],
        "observed_bank_transitions": totals["bank_transitions"],
        "scanlines_with_bank_transitions": changing_lines,
        "scanlines_with_errors": failing_lines,
        "physical_chr_pages_by_ppu_slot": [sorted(p) for p in pages],
        "all_observed_physical_chr_pages": sorted(set().union(*pages)),
        "cache_parity_pass": not (totals["cache_mismatches"] or totals["invalid_fetches"]),
        "observation_scope": "sampled real pattern fetches, not final framebuffer or all frames",
    }


def invented_row(frame: int, line: int, page: int, first: int,
                 last: int, changes: int, count: int = 2,
                 bad: int = 0, invalid: int = 0) -> dict:
    result = {field: "0" for field in FIELDS}
    result.update(frame=str(frame), scanline=str(line), fetches=str(count),
                  asset_mismatch=str(bad), invalid=str(invalid))
    for p in range(PAGE_COUNT):
        result[f"ppu{p}_first"] = "-1"
        result[f"ppu{p}_last"] = "-1"
    result.update({f"ppu{page}_first": str(first),
                   f"ppu{page}_last": str(last),
                   f"ppu{page}_changes": str(changes)})
    return result


def smoke() -> None:
    def csvdata(rows):
        buffer = io.StringIO()
        writer = csv.DictWriter(buffer, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
        return buffer.getvalue()

    valid = [
        invented_row(1, 10, 0, 3, 5, 1, count=4),
        invented_row(1, 11, 1, 7, 7, 0),
        invented_row(61, 20, 7, 127, 127, 0),
    ]
    stats = summarize(io.StringIO(csvdata(valid)))
    assert stats["sampled_frames"] == 2
    assert stats["observed_scanlines"] == 3
    assert stats["pattern_fetches"] == 8
    assert stats["observed_bank_transitions"] == 1
    assert stats["cache_parity_pass"]
    assert stats["physical_chr_pages_by_ppu_slot"][0] == [3, 5]
    assert stats["all_observed_physical_chr_pages"] == [3, 5, 7, 127]
    cases = [
        valid + [valid[-1]],  # duplicate sample line
        [invented_row(2, 10, 0, 0, 0, 0)],  # sampling interval
        [invented_row(1, 312, 0, 0, 0, 0)],  # scanline limit
        [invented_row(1, 10, 0, 0, 128, 1)],  # physical bank invalid
        [invented_row(1, 10, 0, 0, 1, 0)],  # transition missing
        [invented_row(1, 10, 0, 0, 1, 1, count=1, bad=2)],
    ]
    for rows in cases:
        try:
            summarize(io.StringIO(csvdata(rows)))
        except ValueError:
            pass
        else:
            raise AssertionError("Malformed CHR observation was accepted")
    bad_csv = csvdata(valid).replace("ppu0_first", "wrong_first", 1)
    try:
        summarize(io.StringIO(bad_csv))
    except ValueError:
        pass
    else:
        raise AssertionError("Unexpected schema accepted")
    print("PASS JP0600 CHR scanline CSV validation and summary contract")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--smoke", action="store_true")
    group.add_argument("--csv", type=Path, help="Owner-private CVIII-CHR-Fetch.csv")
    ap.add_argument("--json", type=Path, help="Optional separate analysis JSON")
    ap.add_argument("--require-parity", action="store_true",
                    help="Exit nonzero if any observed physical byte mismatch")
    args = ap.parse_args()
    if args.smoke:
        smoke()
        return
    with args.csv.open(newline="", encoding="utf-8") as file:
        report = summarize(file)
    data = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.write_text(data, encoding="utf-8")
    else:
        print(data, end="")
    if args.require_parity and not report["cache_parity_pass"]:
        ap.exit(1, "JP0600 read-only CHR pattern fetches do not match cache\n")


if __name__ == "__main__":
    main()
