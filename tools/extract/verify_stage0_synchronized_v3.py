#!/usr/bin/env python3
"""Fail-closed same-process CPU selector / frame-end PPU timeline audit.

Inputs are created by Capture-CV3DCJP-SynchronizedV3.ps1 in a new private
directory. This does NOT discover palette PRG source tables or prove geometry.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
from pathlib import Path
import struct

ROM_SHA = "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28"
SCHEMA = "cv3dcjp.synchronized-observations/v3"
FILES = ("frames.csv", "palette.bin", "selector-cycle.csv")
HOOK_COLS = ("capture_frame,sequence,fetch_epoch,fetch_cycle,write_cycle,"
             "pc,prg_offset,raw_address,address,old_value,new_value,"
             "fetch_a,fetch_x,fetch_y,write_a,write_x,write_y,zp00,zp01,"
             "stage_before,area_before,room_before,epoch_open").split(",")


def sha(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def fnv(raw: bytes) -> int:
    h = 2166136261
    for byte in raw:
        h = ((h ^ byte) * 16777619) & 0xffffffff
    return h


def parse_csv(data: bytes, required: list[str]) -> list[dict[str, int]]:
    reader = csv.DictReader(data.decode("utf-8-sig").splitlines())
    if reader.fieldnames is None or any(k not in reader.fieldnames for k in required):
        raise ValueError("Capture CSV does not have required fields")
    rows = []
    for record in reader:
        try:
            rows.append({k: int(v, 10) for k, v in record.items() if v is not None})
        except ValueError as exc:
            raise ValueError("Non-numeric capture field") from exc
    if not rows:
        raise ValueError("Empty capture CSV")
    return rows


def verify(directory: Path, rom: bytes) -> dict:
    if len(rom) != 393232 or sha(rom) != ROM_SHA:
        raise ValueError("Wrong Japanese ROM identity")
    receipt = json.loads((directory / "session.json").read_text("utf-8"))
    if (receipt.get("schema") != SCHEMA or receipt.get("rom_sha256") != ROM_SHA
            or receipt.get("exit_code") != 0 or receipt.get("run_id") != directory.name):
        raise ValueError("Invalid or incomplete single-process session")
    copied_exe = directory / "executable/jp_akumajou_densetsu_modern.exe"
    if (not copied_exe.is_file() or
            sha(copied_exe.read_bytes()) != receipt.get("executable_sha256")):
        raise ValueError("Runnable copied executable hash mismatch")
    blobs = {}
    for name in FILES:
        raw = (directory / name).read_bytes()
        if not raw or sha(raw) != receipt.get("sha256", {}).get(name):
            raise ValueError(f"Capture missing or SHA mismatch: {name}")
        blobs[name] = raw
    frames = parse_csv(blobs["frames.csv"],
                       ["frame", "cycles", "stage", "area", "room", "palette_fnv"])
    writes = parse_csv(blobs["selector-cycle.csv"], HOOK_COLS)
    if len(blobs["palette.bin"]) != 40 * len(frames):
        raise ValueError("PPU palette record length mismatch")
    frame_index = {}
    palette_hashes = []
    for i, row in enumerate(frames):
        frame = row["frame"]
        if frame != i + 1 or (i and row["cycles"] <= frames[i-1]["cycles"]):
            raise ValueError("Non-monotonic PPU frame/cycle timeline")
        stamp = struct.unpack_from("<Q", blobs["palette.bin"], 40*i)[0]
        palette = blobs["palette.bin"][40*i+8:40*i+40]
        if stamp != frame or fnv(palette) != row["palette_fnv"]:
            raise ValueError(f"PPU palette mismatch on frame {frame}")
        palette_hashes.append(sha(palette))
        frame_index[frame] = row
    grouped = defaultdict(list)
    last_cycle = -1
    last_epoch = 0
    for index, write in enumerate(writes, 1):
        f = write["capture_frame"]
        addr = write["address"]
        cycle = write["write_cycle"]
        offset = write["prg_offset"]
        if (write["sequence"] != index or f < 1 or f >= len(frames)
                or addr not in (52, 53, 54)
                or write["raw_address"] & 0x7ff != addr
                or not 0 <= write["new_value"] <= 255
                or write["epoch_open"] != 1
                or write["fetch_epoch"] < last_epoch
                or write["fetch_cycle"] > cycle or cycle < last_cycle
                or offset < 0 or offset >= 262144
                or write["fetch_a"] != write["write_a"]
                or write["fetch_x"] != write["write_x"]
                or write["fetch_y"] != write["write_y"]):
            raise ValueError(f"Invalid CPU writer event {index}")
        if not (frame_index[f]["cycles"] <= cycle < frame_index[f+1]["cycles"]):
            raise ValueError(f"CPU writer outside reported frame {f}")
        last_cycle, last_epoch = cycle, write["fetch_epoch"]
        grouped[f].append(write)
    consistent = []
    raw_transitions = []
    for f, group in sorted(grouped.items()):
        state = [frame_index[f][k] for k in ("stage", "area", "room")]
        for event in group:
            before = [event[k] for k in ("stage_before", "area_before", "room_before")]
            if before != state or event["old_value"] != state[event["address"] - 52]:
                raise ValueError(f"CPU selector RAM state mismatch after frame {f}")
            state[event["address"] - 52] = event["new_value"]
        actual = [frame_index[f+1][k] for k in ("stage", "area", "room")]
        if state != actual:
            raise ValueError(f"Selector writer trace does not explain frame {f+1}")
        consistent.append(f)
        old = [frame_index[f][k] for k in ("stage", "area", "room")]
        if old != actual:
            raw_transitions.append({"after_frame": f, "before": old, "after": actual,
                                    "writer_events": len(group),
                                    "spatial_direction": None,
                                    "global_displacement": None})
    unexplained = []
    for i in range(1, len(frames)):
        a, b = frames[i-1], frames[i]
        if any(a[k] != b[k] for k in ("stage", "area", "room")) and i not in grouped:
            unexplained.append(i + 1)
    palette_changes = [
        {"frame": i+1, "previous_sha256": palette_hashes[i-1],
         "current_sha256": palette_hashes[i], "rom_source_offset": None}
        for i in range(1, len(frames)) if palette_hashes[i] != palette_hashes[i-1]
    ]
    return {
        "schema": SCHEMA, "rom_sha256": ROM_SHA, "run_id": receipt["run_id"],
        "verified_single_process_receipt": True,
        "frame_count": len(frames), "selector_write_events": len(writes),
        "selector_writes_cycle_bracketed": len(writes),
        "selector_write_groups_state_consistent": len(consistent),
        "observed_selector_changes": raw_transitions,
        "unexplained_frame_selector_changes": unexplained,
        "palette_distinct_frame_end_states": len(set(palette_hashes)),
        "palette_change_count": len(palette_changes),
        "palette_changes_unattributed": palette_changes,
        "palette_source_rom_addresses_verified": 0,
        "new_rom_verified_world_placements": 0,
        "independent_ppu_oracle": False,
        "status": ("INCOMPLETE" if unexplained else
                   "SAME_RUN_CPU_SELECTOR_AND_FRAME_END_PPU_OBSERVED"),
        "limits": ["Frame-end palette snapshot is not palette write timing",
                   "No CPU/PPU producer attribution for palette writes",
                   "No ROM-proven direction, coordinates or independent pixel parity"]
    }


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--session", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    if args.out.exists():
        raise FileExistsError("Refusing to replace an existing audit")
    result = verify(args.session, args.rom.read_bytes())
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items()
                      if k not in ("palette_changes_unattributed",
                                   "observed_selector_changes")}, indent=2))
    if result["status"] == "INCOMPLETE":
        raise SystemExit(2)


if __name__ == "__main__":
    main()
