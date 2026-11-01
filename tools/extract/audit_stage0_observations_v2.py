#!/usr/bin/env python3
"""Audit original-ROM-backed Stage 0 selector and frame-end PPU palette evidence.

No inferred map edges, palette ROM tables, or room placements are generated.
The runtime capture is a NESRecomp observation, not an independent PPU oracle.
All output is private: generated palettes are original-game-derived content.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile

ROM_SHA = "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28"
FRAMES_SHA = "c0cb6f83f33a9b9a201e439d1f2648769752c9e7ea0619eec3c41ae9fbc8240a"
EVENTS_SHA = "c44492ba681efc991cd8c16d8e4824ed48fef1fbbd4040c158f81b0ec683860e"
SCHEMA = "cv3dcjp.stage0-evidence/v2"


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def fnv32(raw: bytes) -> int:
    result = 2166136261
    for byte in raw:
        result = ((result ^ byte) * 16777619) & 0xffffffff
    return result


def dump(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def audit(rom: bytes, capture: Path, content: Path, destination: Path) -> dict:
    if sha(rom) != ROM_SHA or len(rom) != 393232 or rom[:4] != b"NES\x1a":
        raise ValueError("Authenticated Japanese ROM required")
    if destination.exists():
        raise FileExistsError("Refusing to overwrite existing evidence directory")
    with zipfile.ZipFile(capture) as z:
        frame_bytes = z.read("cviii-evidence-5h8f.frames.csv")
        event_bytes = z.read("cviii-evidence-5h8f.events.csv")
        palette_bytes = z.read("cviii-evidence-5h8f.palette.bin")
    if sha(frame_bytes) != FRAMES_SHA or sha(event_bytes) != EVENTS_SHA:
        raise ValueError("Historical frame or event evidence identity mismatch")
    frames = list(csv.DictReader(io.StringIO(frame_bytes.decode("utf-8-sig"))))
    events = list(csv.DictReader(io.StringIO(event_bytes.decode("utf-8-sig"))))
    if len(frames) != 13944 or len(palette_bytes) != len(frames)*40:
        raise ValueError("Unexpected capture frame count or palette record size")
    palette_states = {}
    room_counts = defaultdict(Counter)
    first = {}
    runs = []
    for index, frame in enumerate(frames):
        frame_id = int(frame["frame"])
        if frame_id != index + 1:
            raise ValueError("Frame discontinuity")
        stored_frame, = struct.unpack_from("<Q", palette_bytes, index*40)
        palette = palette_bytes[index*40+8:index*40+40]
        if stored_frame != frame_id or fnv32(palette) != int(frame["palette_fnv"]):
            raise ValueError(f"Palette frame/digest mismatch at {frame_id}")
        room = "/".join(frame[key] for key in ("stage", "area", "room"))
        key = sha(palette)[:16]
        if key not in palette_states:
            palette_states[key] = {"sha256": sha(palette), "bytes_hex": palette.hex(),
                                   "first_frame": frame_id}
        room_counts[room][key] += 1
        first.setdefault((room, key), frame_id)
        if not runs or runs[-1]["room"] != room:
            if runs:
                runs[-1]["last_frame"] = frame_id - 1
            runs.append({"room": room, "first_frame": frame_id})
    runs[-1]["last_frame"] = len(frames)
    for run in runs:
        run["frames"] = run["last_frame"] - run["first_frame"] + 1
    events_room = [{"room": e["detail"], "first_frame": int(e["frame"])}
                   for e in events if e["event"] == "ROOM"]
    if events_room != [{"room": r["room"], "first_frame": r["first_frame"]}
                       for r in runs]:
        raise ValueError("ROOM events conflict with frame selector sequence")

    stable = [r for r in runs if r["frames"] >= 3 and r["room"].startswith("0/")]
    edges = []
    for a, b in zip(stable, stable[1:]):
        if a["room"] != b["room"]:
            edges.append({"from": a["room"], "to": b["room"],
                          "from_last_frame": a["last_frame"],
                          "to_first_frame": b["first_frame"],
                          "classification": "ROM_BACKED_SELECTOR_SEQUENCE_ONLY",
                          "cpu_writer_site": None, "direction": None,
                          "global_displacement": None})
    with zipfile.ZipFile(content) as z:
        root = "cv3dcjp-content-v1/levels/stage-00/"
        stage = json.loads(z.read(root + "stage.json"))
        records = [json.loads(z.read(root + relative))
                   for relative in stage["room_files"]]
    if len(records) != 8 or {r["identity"]["stage_index"] for r in records} != {0}:
        raise ValueError("Expected exactly eight authenticated Stage 0 room records")
    ids = set()
    if destination.exists():
        raise FileExistsError("Refusing to overwrite output")
    destination.mkdir(parents=True)
    for room in records:
        identity = room["identity"]
        key = f'0/{identity["area_index"]}/{identity["room_index"]}'
        if key in ids:
            raise ValueError("Duplicate Stage 0 room")
        ids.add(key)
        dump(destination / "levels" / f'area-{identity["area_index"]:02d}' /
             f'room-{identity["room_index"]:02d}.json', {
                 "schema": SCHEMA, "selector": key,
                 "source_layout_offset": room["source"]["layout_file_offset"],
                 "metatile_grid_wh": room["room_grid_size_metatiles"],
                 "metatile_group": room["graphics"]["metatile_group"],
                 "world_origin": None,
                 "world_origin_status": "UNRESOLVED_CPU_PPU_TRANSITION_EVIDENCE",
                 "observed_palette_states": [
                     {"id": state, "frames": count,
                      "first_frame": first[(key, state)]}
                     for state, count in room_counts[key].most_common()
                 ]
             })
    if len(edges) != 7 or len(palette_states) != 24:
        raise ValueError("Historical evidence population differs from verified audit")
    dump(destination / "connections-observed.json", {
        "schema": SCHEMA, "rom_sha256": ROM_SHA, "runs": runs,
        "stable_threshold_frames": 3, "observed_edges": edges,
        "status": "NO_SPATIAL_EDGE_OR_DIRECTION_ESTABLISHED"
    })
    dump(destination / "palettes-frame-end-PRIVATE.json", {
        "schema": SCHEMA, "rom_sha256": ROM_SHA, "palette_states": palette_states,
        "source_sha256": sha(palette_bytes), "observed_frame_count": len(frames),
        "status": "FRAME_END_RAM_NOT_PER_SCANLINE_OR_ROM_TABLE"
    })
    result = {"schema": SCHEMA, "rom_sha256": ROM_SHA,
              "source_capture_zip_sha256": sha(capture.read_bytes()),
              "source_content_v1_zip_sha256": sha(content.read_bytes()),
              "frame_csv_sha256": sha(frame_bytes), "event_csv_sha256": sha(event_bytes),
              "frames": len(frames), "observed_selector_edges": len(edges),
              "palette_states": len(palette_states), "room_records": len(records),
              "new_rom_verified_global_placements": 0,
              "independent_ppu_frame_comparison": False,
              "cpu_writer_traces_synchronized": False}
    dump(destination / "audit-summary.json", result)
    return result


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom", type=Path, required=True)
    p.add_argument("--capture", type=Path, required=True)
    p.add_argument("--content-v1", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    print(json.dumps(audit(a.rom.read_bytes(), a.capture, a.content_v1, a.out), indent=2))


if __name__ == "__main__":
    main()
