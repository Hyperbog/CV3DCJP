#!/usr/bin/env python3
"""Independently prove v3 selector writes came from mapped Japanese-ROM opcodes.

The v3 CPU trace uses header-EXCLUDED PRG offsets. Always add 16 before
indexing .nes bytes. Does not prove spatial adjacency or palette ROM sources.
"""
from __future__ import annotations

from collections import Counter, defaultdict
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import zipfile

ROM_SHA = "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decode_writer(rom: bytes, event: dict) -> tuple[int, str]:
    pc = int(event["pc"])
    offset = int(event["prg_offset"])
    if not 0x8000 <= pc <= 0xffff or not 0 <= offset < 262144:
        raise ValueError("Invalid CPU PC or PRG offset")
    if pc >= 0xc000:
        if offset != 0x3c000 + pc - 0xc000:
            raise ValueError("Fixed-bank PRG mapping mismatch")
    elif offset % 0x4000 != pc - 0x8000:
        raise ValueError("Switched-bank PRG mapping mismatch")
    op, operand = rom[offset+16:offset+18]
    address, old, new = (int(event[k]) for k in ("address", "old_value", "new_value"))
    a, x, y = (int(event[k]) for k in ("fetch_a", "fetch_x", "fetch_y"))
    if address not in (52, 53, 54) or not 0 <= old <= 255 or not 0 <= new <= 255:
        raise ValueError("Invalid selector write values")
    if op == 0x91 and operand == 0:
        name = "STA ($00),Y"
        effective = (int(event["zp00"]) + 256*int(event["zp01"]) + y) & 0xffff
        matched = effective == address and new == a
    elif op == 0x95:
        name = "STA zp,X"
        matched = ((operand + x) & 255) == address and new == a
    elif op == 0x85:
        name = "STA zp"
        matched = operand == address and new == a
    elif op in (0xe6, 0xc6):
        name = "INC zp" if op == 0xe6 else "DEC zp"
        expected = (old + (1 if op == 0xe6 else -1)) & 255
        matched = operand == address and new in (old, expected)
    else:
        raise ValueError("ROM opcode is not a supported selector writer")
    if not matched:
        raise ValueError("ROM operand/CPU register/write mismatch")
    return offset+16, name


def verify(rom: bytes, capture_zip: Path) -> dict:
    if len(rom) != 393232 or sha(rom) != ROM_SHA:
        raise ValueError("Authenticated Japanese ROM required")
    with zipfile.ZipFile(capture_zip) as archive:
        receipt = json.loads(archive.read("session.json"))
        previous = json.loads(archive.read("audit.json"))
        source = {name: archive.read(name) for name in
                  ("frames.csv", "selector-cycle.csv", "palette.bin")}
    if (receipt.get("rom_sha256") != ROM_SHA or
            receipt.get("run_id") != previous.get("run_id") or
            previous.get("status") != "SAME_RUN_CPU_SELECTOR_AND_FRAME_END_PPU_OBSERVED"):
        raise ValueError("Mismatched run receipt or incomplete original audit")
    for name, raw in source.items():
        if sha(raw) != receipt.get("sha256", {}).get(name):
            raise ValueError("Capture data SHA-256 mismatch: " + name)
    frames = list(csv.DictReader(io.StringIO(source["frames.csv"].decode("utf-8-sig"))))
    writes = list(csv.DictReader(io.StringIO(source["selector-cycle.csv"].decode("utf-8-sig"))))
    if not frames or len(source["palette.bin"]) != 40*len(frames):
        raise ValueError("Truncated frames or palette records")
    sites, grouped, rmw = Counter(), defaultdict(list), defaultdict(list)
    last_cycle, last_epoch = -1, -1
    for sequence, row in enumerate(writes, 1):
        frame = int(row["capture_frame"])
        cycle = int(row["write_cycle"])
        epoch = int(row["fetch_epoch"])
        if (int(row["sequence"]) != sequence or
                not 1 <= frame < len(frames) or
                int(frames[frame-1]["cycles"]) > cycle or
                cycle >= int(frames[frame]["cycles"]) or
                cycle < last_cycle or epoch < last_epoch or
                int(row["fetch_cycle"]) > cycle or
                int(row["epoch_open"]) != 1):
            raise ValueError("Unmatched CPU write cycle/epoch")
        offset, opcode = decode_writer(rom, row)
        sites[(int(row["pc"]), offset, opcode)] += 1
        grouped[frame].append(row)
        if opcode in ("INC zp", "DEC zp"):
            rmw[(epoch, int(row["pc"]), int(row["address"]))].append(row)
        last_cycle, last_epoch = cycle, epoch
    for key, pair in rmw.items():
        if len(pair) != 2:
            raise ValueError("Unpaired 6502 read-modify-write event")
        first, second = pair
        old = int(first["old_value"])
        amount = 1 if decode_writer(rom, first)[1] == "INC zp" else -1
        if (int(first["new_value"]) != old or
                int(second["old_value"]) != old or
                int(second["new_value"]) != ((old+amount) & 255) or
                int(second["write_cycle"]) != int(first["write_cycle"])+1):
            raise ValueError("Invalid read-modify-write bus writes")
    for frame, rows in grouped.items():
        state = [int(frames[frame-1][k]) for k in ("stage", "area", "room")]
        for row in rows:
            if state != [int(row[k]) for k in ("stage_before", "area_before", "room_before")]:
                raise ValueError("Writer before-state mismatch")
            index = int(row["address"])-52
            if int(row["old_value"]) != state[index]:
                raise ValueError("Old selector RAM value mismatch")
            state[index] = int(row["new_value"])
        if state != [int(frames[frame][k]) for k in ("stage", "area", "room")]:
            raise ValueError("Next-frame selector state mismatch")
    for i in range(1, len(frames)):
        if (any(frames[i-1][k] != frames[i][k] for k in ("stage", "area", "room"))
                and i not in grouped):
            raise ValueError("Selector changed without CPU writer event")
    return {
        "schema": "cv3dcjp.stage0-rom-opcode-audit/v3",
        "rom_sha256": ROM_SHA,
        "capture_zip_sha256": sha(capture_zip.read_bytes()),
        "run_id": receipt["run_id"], "frames": len(frames),
        "selector_writer_events": len(writes), "opcode_matched_events": len(writes),
        "write_groups": len(grouped), "rmw_pairs": len(rmw),
        "verified_writer_sites": [
            {"pc": hex(pc), "nes_file_offset": hex(offset),
             "instruction": mnemonic, "write_events": count}
            for (pc, offset, mnemonic), count in sorted(sites.items())
        ],
        "unexplained_selector_changes": 0, "new_verified_world_origins": 0,
        "palette_source_rom_addresses_verified": 0,
        "executable_sha_verified_from_uploaded_zip": False,
        "limitations": [
            "The uploaded ZIP contains a receipt but not the executable for independent SHA verification.",
            "Palette writes are observed only as frame-end RAM; source instructions untraced.",
            "Selector state changes do not establish global coordinates or adjacency."
        ]
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", required=True, type=Path)
    p.add_argument("--capture", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    args = p.parse_args()
    if args.out.exists():
        raise FileExistsError("Refusing overwrite: " + str(args.out))
    result = verify(args.rom.read_bytes(), args.capture)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print("PASS: {} ROM-matched CPU writes across {} instruction sites".format(
          result["opcode_matched_events"], len(result["verified_writer_sites"])))


if __name__ == "__main__":
    main()
