#!/usr/bin/env python3
"""Verify historical Stage 0 selector writer sites against exact ROM bytes.

This verifies *static instructions only*. It does not prove a site executed during
a particular room transition, or spatial adjacency/direction.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
from pathlib import Path
import re

ROM_SHA = "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28"
OPCODES = {"INC": 0xE6, "DEC": 0xC6, "STA": 0x85}
PATTERN = re.compile(r"(INC|DEC|STA) \$(34|35|36)\Z")


def verify(rom: bytes, csv_text: str) -> dict:
    if len(rom) != 393232 or hashlib.sha256(rom).hexdigest() != ROM_SHA:
        raise ValueError("Only authenticated Japanese ROM is supported")
    seen = set()
    rows = list(csv.DictReader(csv_text.splitlines()))
    if len(rows) != 16:
        raise ValueError("Expected exactly 16 verified historical writer sites")
    for row in rows:
        if row["confidence"] != "VERIFIED_ROM_BYTES_NOT_EXECUTION":
            raise ValueError("Only historical static ROM evidence is accepted")
        offset = int(row["rom_file_offset"], 16)
        match = PATTERN.fullmatch(row["instruction"])
        if match is None or offset in seen:
            raise ValueError("Invalid or duplicate selector instruction")
        expected = bytes((OPCODES[match.group(1)], int(match.group(2), 16)))
        if rom[offset:offset+2] != expected:
            raise ValueError(f"ROM instruction mismatch at offset {offset:#x}")
        seen.add(offset)
    return {
        "rom_sha256": ROM_SHA,
        "static_instruction_sites_verified": len(seen),
        "runtime_executions_verified_here": 0,
        "new_world_placements": 0,
        "classification": "ROM_BYTES_ONLY_NOT_RUNTIME_TRANSITION_PROOF"
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--sites", type=Path, required=True)
    args = parser.parse_args()
    from json import dumps
    print(dumps(verify(args.rom.read_bytes(),
                       args.sites.read_text(encoding="utf-8-sig")), indent=2))


if __name__ == "__main__":
    main()
