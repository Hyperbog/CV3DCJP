#!/usr/bin/env python3
"""ROM-authoritative local content export (private generated outputs only).

Reuses the authenticated 135-room and 15-metatile-group Japanese ROM decoders.
Does not claim global positions, runtime palettes, music or gameplay semantics.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROM_SHA = "958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28"
SCHEMA = "cv3dcjp.rom-content/v1"
ROM_BYTES = 393232
STAGES = 15
ROOMS = 135


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decoder(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ValueError(f"Missing source decoder: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def json_file(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def extract(rom: bytes, tool_dir: Path, out: Path) -> None:
    if (len(rom) != ROM_BYTES or sha(rom) != ROM_SHA or
            rom[:4] != b"NES\x1a" or (rom[6] >> 4 | rom[7] & 0xf0) != 24):
        raise ValueError("Only the authenticated Japanese mapper-24 ROM is supported")
    if out.exists():
        raise FileExistsError(f"Refusing to overwrite content output: {out}")

    room_decoder = decoder(tool_dir / "build_room_resources.py", "rom_rooms_v1")
    gfx_decoder = decoder(tool_dir / "build_room_graphics.py", "rom_graphics_v1")
    rp = room_decoder.build(rom)
    gp, counts = gfx_decoder.build(rom, rp)
    if (rp[:8] != b"CVROOM1\x00" or gp[:8] != b"CVGFX01\x00" or
            len(counts) != STAGES or sum(counts) != 1563):
        raise ValueError("Unexpected versioned ROM-derived pack population")

    rooms = []
    for i in range(ROOMS):
        base = 64 + i * 32
        stage, area, room, meta, screens, bg0, bg1, sp0, sp1 = rp[base:base+9]
        w, h, length, rom_at, grid_at = struct.unpack_from("<HHHII", rp, base+10)
        cells = list(rp[grid_at:grid_at+length])
        vertical = bool(meta & 0x80)
        expected = (8, screens * 8) if vertical else (screens * 8, 6)
        group = gfx_decoder.effective_group(stage, area, room)
        if (stage >= STAGES or not 1 <= screens <= 16 or (w, h) != expected
                or w * h != length or len(cells) != length or
                any(v >= counts[group] for v in cells)):
            raise ValueError(f"Malformed room selector {i}")
        rooms.append((stage, area, room, meta, screens, w, h, rom_at,
                      [bg0, bg1, sp0, sp1], group, cells))
    if len({r[:3] for r in rooms}) != ROOMS:
        raise ValueError("Duplicate room selectors")

    chr_data = rom[16+262144:]
    if len(chr_data) != 131072:
        raise ValueError("Missing 128 KiB physical CHR-ROM")
    out.mkdir(parents=True)
    (out / "graphics").mkdir()
    (out / "graphics/chr-patterns.2bpp").write_bytes(chr_data)
    json_file(out / "graphics/chr-index.json", {
        "schema": SCHEMA, "rom_sha256": ROM_SHA,
        "file": "chr-patterns.2bpp", "format": "NES planar 2bpp 8x8",
        "bytes_per_tile": 16, "tile_count": 8192, "page_size": 1024,
        "page_count": 128,
        "pages": [{"index": i, "offset": i*1024,
                   "sha256": sha(chr_data[i*1024:(i+1)*1024])}
                  for i in range(128)],
        "note": "Raw physical CHR patterns. Active mapper, scroll and palette are runtime state."
    })

    for group in range(STAGES):
        gid, bank, count, tiles_rom, attrs_rom, tiles_at, attrs_at, reserved = (
            struct.unpack_from("<BBHIIIII", gp, 64 + group*24))
        if gid != group or count != counts[group] or reserved:
            raise ValueError(f"Invalid metatile group {group}")
        entries = []
        for tile_id in range(count):
            raw_ids = list(gp[tiles_at+tile_id*16:tiles_at+(tile_id+1)*16])
            if len(raw_ids) != 16 or attrs_at+tile_id >= len(gp):
                raise ValueError("Truncated metatile record")
            entries.append({"id": tile_id, "chr_tile_ids_4x4": raw_ids,
                            "attribute_byte": gp[attrs_at+tile_id]})
        json_file(out / f"graphics/metatiles/group-{group:02d}.json", {
            "schema": SCHEMA, "rom_sha256": ROM_SHA,
            "group_id": group, "selector_bank": bank,
            "source_offsets": {"tile_table": tiles_rom,
                               "attribute_table": attrs_rom},
            "records": entries,
            "note": "Raw ROM tile IDs and attribute bytes; dynamic display palette is unresolved."
        })

    stage_list = []
    for stage in range(STAGES):
        stage_rooms = [r for r in rooms if r[0] == stage]
        files = []
        for r in stage_rooms:
            s, area, room, meta, screens, width, height, offset, chr_pages, group, cells = r
            vertical = bool(meta & 0x80)
            sections = []
            for n in range(screens):
                x0, y0, sw, sh = (0, n*8, 8, 8) if vertical else (n*8, 0, 8, 6)
                matrix = [cells[y*width+x0:y*width+x0+sw]
                          for y in range(y0, y0+sh)]
                sections.append({"index": n, "grid_origin": [x0, y0],
                                 "grid_size": [sw, sh], "metatile_ids": matrix})
            rel = f"area-{area:02d}/room-{room:02d}.json"
            files.append(rel)
            json_file(out / f"levels/stage-{stage:02d}" / rel, {
                "schema": SCHEMA, "rom_sha256": ROM_SHA,
                "identity": {"stage_index": stage, "area_index": area,
                             "room_index": room},
                "source": {"layout_file_offset": offset, "metadata_byte": meta},
                "room_grid_size_metatiles": [width, height],
                "vertical_layout": vertical,
                "graphics": {
                    "metatile_group": f"graphics/metatiles/group-{group:02d}.json",
                    "chr_page_selectors": chr_pages
                },
                "sections": sections, "world_origin": None,
                "world_origin_status": "UNRESOLVED_ROM_TRANSITION_EVIDENCE",
                "runtime_palette_phase": "UNRESOLVED",
                "collision_binding": "UNRESOLVED", "transitions": []
            })
        json_file(out / f"levels/stage-{stage:02d}/stage.json", {
            "schema": SCHEMA, "rom_sha256": ROM_SHA, "stage_index": stage,
            "area_indices": sorted({r[1] for r in stage_rooms}),
            "room_count": len(stage_rooms), "room_files": files,
            "global_placement_complete": False,
            "note": "ROM stage selector; does not imply linear level order."
        })
        stage_list.append({"stage_index": stage, "id": f"stage-{stage:02d}",
                           "room_count": len(stage_rooms)})
    json_file(out / "levels/index.json", {
        "schema": SCHEMA, "rom_sha256": ROM_SHA,
        "stage_count": STAGES, "room_count": ROOMS, "stages": stage_list,
        "note": "Internal ROM stage selectors, not assumed chronological levels."
    })
    for category, note in (
        ("audio", "Songs/SFX/instruments and timing require verified sound routines and 2A03/VRC6 traces."),
        ("gameplay", "Collision, AI, entities and progression require ROM routine and RAM evidence."),
    ):
        json_file(out / category / "index.json", {
            "schema": SCHEMA, "rom_sha256": ROM_SHA,
            "status": "NOT_YET_DECODED", "entries": [], "note": note
        })
    json_file(out / "source/rom-index.json", {
        "schema": SCHEMA, "sha256": ROM_SHA, "size": ROM_BYTES, "mapper": 24,
        "prg": {"offset": 16, "bytes": 262144,
                "sha256": sha(rom[16:16+262144])},
        "chr": {"offset": 16+262144, "bytes": 131072, "sha256": sha(chr_data)},
        "source_decoders": ["build_room_resources.py", "build_room_graphics.py"],
        "decoded": {"stages": 15, "rooms": 135, "distinct_layouts": 130,
                    "metatile_definitions": 1563, "chr_tiles": 8192},
        "unresolved": ["global_room_positions", "room_transitions", "runtime_palettes",
                       "collision_semantics", "music", "sound_effects", "gameplay_rules"]
    })
    print("PASS: 15 stages, 135 room selectors, 130 layouts, 1563 metatiles, 8192 CHR tiles")
    print("NOT YET VERIFIED: room-global placements, runtime palettes, gameplay and audio")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    tool_dir = Path(__file__).resolve().parents[2] / (
        "legacy/rom-backed/JP/recomp/tools")
    extract(args.rom.read_bytes(), tool_dir, args.out)


if __name__ == "__main__":
    main()
