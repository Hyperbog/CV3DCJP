"""Fixture tests; the real ROM is neither stored nor distributed with CI."""
import csv
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "stage0_sync_v3", ROOT / "tools/extract/verify_stage0_synchronized_v3.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def csv_bytes(fields, records):
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(records)
    return output.getvalue().encode("utf-8")


def fixture(root, room2=1, bad_cycle=False, broken_palette=False):
    rom = bytes(393232)
    tool.ROM_SHA = tool.sha(rom)  # Test fixture only; no accepted ROM altered.
    run = root / "test-run"
    (run / "executable").mkdir(parents=True)
    exe = run / "executable/jp_akumajou_densetsu_modern.exe"
    exe.write_bytes(b"fixture executable")
    states = [(0, 0, 0), (0, 1, 0), (0, room2, 0)]
    frames = [{"frame": i + 1, "cycles": (i + 1) * 100,
               "stage": s, "area": a, "room": r,
               "palette_fnv": tool.fnv(bytes([i]) * 32)}
              for i, (s, a, r) in enumerate(states)]
    palettes = b"".join(struct.pack("<Q", i+1) + bytes([i])*32
                        for i in range(len(states)))
    if broken_palette:
        palettes = palettes[:-1] + b"\xff"
    frame_data = csv_bytes(list(frames[0]), frames)
    def event(seq, f, old, new):
        return dict(zip(tool.HOOK_COLS,
                        [f, seq, seq, f*100+1, (f+1)*100+1 if bad_cycle else f*100+50,
                         0xC000, 0, 53, 53, old, new,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, old, 0, 1]))
    writes = [event(1, 1, 0, 1)]
    if room2 == 2:
        writes.append(event(2, 2, 1, 2))
    blobs = {"frames.csv": frame_data, "palette.bin": palettes,
             "selector-cycle.csv": csv_bytes(tool.HOOK_COLS, writes)}
    for name, blob in blobs.items():
        (run / name).write_bytes(blob)
    receipt = {"schema": tool.SCHEMA, "rom_sha256": tool.ROM_SHA,
               "run_id": run.name, "exit_code": 0,
               "executable_sha256": tool.sha(exe.read_bytes()),
               "sha256": {name: tool.sha(blob) for name, blob in blobs.items()}}
    (run / "session.json").write_text(json.dumps(receipt), encoding="utf-8")
    return rom, run


class SingleRunTests(unittest.TestCase):
    def test_single_run_writer_frame_bracketing(self):
        with tempfile.TemporaryDirectory() as td:
            rom, run = fixture(Path(td), room2=2)
            result = tool.verify(run, rom)
            self.assertEqual(result["selector_write_events"], 2)
            self.assertEqual(len(result["observed_selector_changes"]), 2)
            self.assertEqual(result["new_rom_verified_world_placements"], 0)
            self.assertEqual(result["palette_source_rom_addresses_verified"], 0)

    def test_wrong_capture_hash_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            rom, run = fixture(Path(td))
            with (run / "palette.bin").open("ab") as f:
                f.write(b"\x00")
            with self.assertRaisesRegex(ValueError, "SHA mismatch"):
                tool.verify(run, rom)

    def test_wrong_cpu_cycle_interval_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            rom, run = fixture(Path(td), bad_cycle=True)
            with self.assertRaisesRegex(ValueError, "outside reported frame"):
                tool.verify(run, rom)

    def test_palette_bytes_are_independently_checked(self):
        with tempfile.TemporaryDirectory() as td:
            rom, run = fixture(Path(td), broken_palette=True)
            with self.assertRaisesRegex(ValueError, "PPU palette mismatch"):
                tool.verify(run, rom)

    def test_cross_run_receipt_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            rom, run = fixture(Path(td))
            receipt = json.loads((run / "session.json").read_text("utf-8"))
            receipt["run_id"] = "different-run"
            (run / "session.json").write_text(json.dumps(receipt))
            with self.assertRaisesRegex(ValueError, "single-process session"):
                tool.verify(run, rom)

    def test_observation_does_not_prove_palette_source_or_coordinates(self):
        with tempfile.TemporaryDirectory() as td:
            rom, run = fixture(Path(td), room2=1)
            result = tool.verify(run, rom)
            self.assertFalse(result["independent_ppu_oracle"])
            self.assertEqual(result["palette_source_rom_addresses_verified"], 0)
            self.assertTrue(all(v["global_displacement"] is None
                                for v in result["observed_selector_changes"]))


if __name__ == "__main__":
    unittest.main()
