#!/usr/bin/env python3
"""JP0609: explain owner-local overlapping sprites and rotating hardware OAM.

Parses *observed* Japanese RAM $0400..$04C3 and all 64 PPU OAM slots.
Detects same-pair physical OAM priority reversals across adjacent frames
and compares their stable logical PRG26 emission order. No ROM data here.
"""
from __future__ import annotations
import argparse
import csv
import io
import json
from pathlib import Path

FIELDS = ("frame", "phase", "phase_delta", "active_objects",
          "visible_oam", "frame_offsets", "base_y", "base_x",
          "attributes", "flags", "selector", "mirror", "ppu_oam")
OBJECT_FIELDS = ("frame_offsets", "base_y", "base_x", "attributes",
                 "flags", "selector", "mirror")


def _read_hex(value: str, count: int, name: str) -> bytes:
    if len(value) != count * 2:
        raise ValueError(f"{name}: expected {count} bytes, got {len(value) // 2}")
    try:
        return bytes.fromhex(value)
    except ValueError as exc:
        raise ValueError(f"{name}: invalid hex") from exc


def analyze(text: str) -> dict:
    reader = csv.DictReader(io.StringIO(text))
    if tuple(reader.fieldnames or ()) != FIELDS:
        raise ValueError("Not a JP0609 object-to-OAM trace")
    frames = 0
    prev_phase = None
    prev_pairs: dict[tuple, tuple] = {}
    total_objects = total_sprites = full = rotation = motion = 0
    overlaps = flips = stable_flips = 0
    observed_pairs = 0
    present_active: set[int] = set()
    for row in reader:
        if set(row) != set(FIELDS) or any(v is None for v in row.values()):
            raise ValueError("Malformed JP0609 object trace row")
        frames += 1
        if frames > 120000:
            raise ValueError("Trace too long; capture shorter gameplay session")
        try:
            frame = int(row["frame"])
            phase = int(row["phase"])
            delta = int(row["phase_delta"])
            active = int(row["active_objects"])
            visible = int(row["visible_oam"])
        except ValueError as exc:
            raise ValueError("Non-decimal object/OAM trace counters") from exc
        if (frame != frames or not 0 <= phase <= 255 or phase % 4 or
                not 0 <= active <= 28 or not 0 <= visible <= 64 or
                delta != (-1 if prev_phase is None else (phase-prev_phase)&255)):
            raise ValueError(f"Invalid frame, phase or OAM count on row {frames}")
        rotation += int(delta == 68)
        motion += int(delta > 0)
        prev_phase = phase
        arrays = {k: _read_hex(row[k], 28, k) for k in OBJECT_FIELDS}
        calculated = 0
        for idx in range(28):
            if arrays["frame_offsets"][idx] and not arrays["flags"][idx] & 0x80:
                calculated += 1
                present_active.add(idx)
        if active != calculated:
            raise ValueError(f"Game object population mismatch frame {frames}")
        oam = _read_hex(row["ppu_oam"], 256, "ppu_oam")
        sprites = []
        for slot in range(64):
            y, tile, attr, x = oam[slot*4: slot*4+4]
            if y >= 0xEF:
                continue
            logical = ((slot - phase // 4) * 17) & 63  # inverse of -15 mod 64
            key = (y, tile, attr, x)
            sprites.append((key, slot, logical, x, y+1))
        if visible != len(sprites):
            raise ValueError(f"PPU OAM sprite population mismatch frame {frames}")
        total_objects += active
        total_sprites += visible
        full += int(visible == 64)
        signatures = [s[0] for s in sprites]
        if len(set(signatures)) != len(signatures):
            # Identical pieces have no safely observable individual identity.
            prev_pairs = {}
            continue
        current_pairs = {}
        for i, a in enumerate(sprites):
            for b in sprites[i+1:]:
                if abs(a[3]-b[3]) >= 8 or abs(a[4]-b[4]) >= 16:
                    continue
                overlaps += 1
                ids = tuple(sorted((a[0],b[0])))
                front_key = a[0] if a[1] < b[1] else b[0]
                logical_front = a[0] if a[2] < b[2] else b[0]
                current_pairs[ids] = (front_key, logical_front)
                if ids in prev_pairs:
                    observed_pairs += 1
                    old_phys, old_logical = prev_pairs[ids]
                    if old_phys != front_key:
                        flips += 1
                        stable_flips += int(old_logical == logical_front)
        prev_pairs = current_pairs
    if not frames:
        raise ValueError("Empty JP0609 trace")
    return {
        "schema": "CVIII-JP-object-to-OAM/v1",
        "frames": frames,
        "active_object_samples": total_objects,
        "oam_sprite_samples": total_sprites,
        "full_64_sprite_frames": full,
        "frame_phase_plus44_events": rotation,
        "frames_with_phase_changes": motion,
        "active_object_indices_seen": sorted(present_active),
        "overlapping_sprite_pairs_seen": overlaps,
        "same_pair_consecutive_observations": observed_pairs,
        "same_pair_physical_oam_priority_reversals": flips,
        "reversals_with_logical_emission_priority_stable": stable_flips,
        "scope": "Observed OAM and game object records; does not prove individual actor identity or pixel accuracy",
        "native_sprite_flicker_fixed": False,
    }


def _fixture(frame: int, phase: int, slots: tuple[int, int], delta: int) -> dict:
    def hex28(set_first: int = 0) -> str:
        return bytes([set_first] + [0]*27).hex()
    oam = bytearray([0xF4] * 256)
    for slot, tile in zip(slots, (1, 2)):
        oam[slot*4:slot*4+4] = bytes((90, tile, 0, 40))
    row = {k: hex28() for k in OBJECT_FIELDS}
    row["frame_offsets"] = hex28(2)
    row.update(frame=str(frame), phase=str(phase), phase_delta=str(delta),
               active_objects="1", visible_oam="2", ppu_oam=oam.hex())
    return row


def _csv(rows: list[dict]) -> str:
    out = io.StringIO()
    w = csv.DictWriter(out, fieldnames=FIELDS, lineterminator="\n")
    w.writeheader()
    w.writerows(rows)
    return out.getvalue()


def smoke() -> None:
    a = _fixture(1, 0, (0, 49), -1)
    b = _fixture(2, 68, (17, 2), 68)
    report = analyze(_csv([a, b]))
    assert report["frames"] == 2 and report["frame_phase_plus44_events"] == 1
    assert report["same_pair_physical_oam_priority_reversals"] == 1
    assert report["reversals_with_logical_emission_priority_stable"] == 1
    assert report["oam_sprite_samples"] == 4
    for bad in (
        _csv([a, dict(b, phase_delta="0")]),
        _csv([a, dict(b, frame="4")]),
        _csv([dict(a, active_objects="2"), b]),
        _csv([dict(a, visible_oam="1"), b]),
        _csv([dict(a, ppu_oam="0"), b]),
        _csv([dict(a, phase="1"), b]),
        _csv([dict(a, flags="zz"*28), b]),
    ):
        try:
            analyze(bad)
        except ValueError:
            continue
        raise AssertionError("Malformed object/OAM trace incorrectly accepted")
    print("PASS JP0609 authentic 28-object format and physically rotating OAM priority analyzer")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--csv", type=Path)
    g.add_argument("--smoke", action="store_true")
    p.add_argument("--json", type=Path)
    args = p.parse_args()
    if args.smoke:
        smoke()
        return
    result = analyze(args.csv.read_text(encoding="utf-8"))
    if args.json:
        args.json.write_text(json.dumps(result, indent=2, sort_keys=True)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
