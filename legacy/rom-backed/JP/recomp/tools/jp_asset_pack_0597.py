#!/usr/bin/env python3
"""JP0597 versioned local CHR asset cache for owner-supplied JP Castlevania III.

This package is an OFFLINE asset index, not a replacement for the ROM input
required by the pinned NESRecomp compiler or runner. Generated BIN content is
copyrighted game data: keep it private and outside source/CI artifacts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import tempfile
import zipfile
from pathlib import Path

import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2] / 'decomp' / 'tools'))
from jp_password_stage_0593 import JP_SHA, JP_TABLES, load_tables

SCHEMA = "cviii.jp.assets/v1"
MAGIC_ROM = b"NES\x1a"
ROM_LENGTH = 393232
PRG_LENGTH = 256 * 1024
CHR_LENGTH = 128 * 1024
TILE_COUNT = CHR_LENGTH // 16
PAGE_COUNT = CHR_LENGTH // 1024
RAW_NAME = "chr/raw_2bpp.bin"
INDEXED_NAME = "chr/pixels_u8.bin"
MANIFEST_NAME = "manifest.json"
TIMESTAMP = (1980, 1, 1, 0, 0, 0)
SOURCE = "Akumajou Densetsu Japanese iNES mapper24 owner-supplied ROM"


def sha(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def decode_chr(data: bytes) -> bytes:
    """NES 2bpp planar CHR -> 64 row-major 0..3 indices per 16-byte tile.

    This does NOT select palettes, scroll registers or live mapper banks.
    """
    if len(data) != CHR_LENGTH:
        raise ValueError("Expected full 128 KiB CHR-ROM, not a PPU snapshot")
    out = bytearray(TILE_COUNT * 64)
    for tile in range(TILE_COUNT):
        base, pix = tile * 16, tile * 64
        for y in range(8):
            low, high = data[base + y], data[base + y + 8]
            for x in range(8):
                shift = 7 - x
                out[pix + y * 8 + x] = ((low >> shift) & 1) | (((high >> shift) & 1) << 1)
    return bytes(out)


def encode_chr(indexed: bytes) -> bytes:
    """Reconstruct original planar bytes to prove lossless tile extraction."""
    if len(indexed) != TILE_COUNT * 64 or any(x > 3 for x in indexed):
        raise ValueError("Invalid indexed 8x8 CHR tile array")
    out = bytearray(CHR_LENGTH)
    for tile in range(TILE_COUNT):
        base, pix = tile * 16, tile * 64
        for y in range(8):
            low = high = 0
            for x in range(8):
                symbol = indexed[pix + y * 8 + x]
                low |= (symbol & 1) << (7 - x)
                high |= ((symbol >> 1) & 1) << (7 - x)
            out[base + y], out[base + y + 8] = low, high
    return bytes(out)


def make_contents(chr_bytes: bytes, tables: dict[str, bytes], source_sha: str) -> tuple[dict, dict[str, bytes]]:
    """Game-specific extraction; archive writer/reader are format reusable."""
    pixels = decode_chr(chr_bytes)
    if encode_chr(pixels) != chr_bytes:
        raise AssertionError("CHR planar roundtrip mismatch")
    entries = {RAW_NAME: chr_bytes, INDEXED_NAME: pixels}
    bank_hashes = [sha(chr_bytes[i * 1024:(i + 1) * 1024]) for i in range(PAGE_COUNT)]
    types = {
        RAW_NAME: {"kind": "chr-2bpp-planar", "physical_page_bytes": 1024, "pages": PAGE_COUNT,
                   "tile_bytes": 16, "tiles_per_page": 64, "page_sha256": bank_hashes},
        INDEXED_NAME: {"kind": "chr-indexed-pixels", "tile_width": 8, "tile_height": 8,
                       "tile_pixels": 64, "pixel_values": [0, 1, 2, 3], "tile_count": TILE_COUNT,
                       "ordering": "same physical tile order as raw_2bpp"},
    }
    for name, (offset, length) in sorted(JP_TABLES.items()):
        data = tables[name]
        if len(data) != length:
            raise ValueError(f"Incorrect table size for {name}")
        path = f"tables/password/{name}.bin"
        entries[path] = data
        types[path] = {"kind": "jp-password-table", "prg_physical_offset": offset,
                       "prg_8k_bank": offset // 8192, "bank_offset": offset % 8192,
                       "mapping_note": "physical PRG address, not an unconditional CPU address"}
    manifest = {
        "schema": SCHEMA,
        "source": SOURCE,
        "rom_sha256": source_sha,
        "source_rom_size": ROM_LENGTH,
        "mapper": 24,
        "prg_bytes": PRG_LENGTH,
        "chr_bytes": CHR_LENGTH,
        "status": "offline-asset-cache-not-yet-loaded-by-native-runtime",
        "unresolved": ["PRG gameplay/data partition", "stage maps and metatiles", "collision",
                       "object animation compositions", "live palettes", "music/SFX driver sequences",
                       "text and scene scripts", "CHR bank timing and mapper PPU address translation"],
        "entries": {name: {"size": len(blob), "sha256": sha(blob), **types[name]}
                    for name, blob in sorted(entries.items())},
    }
    return manifest, entries


def json_bytes(payload: dict) -> bytes:
    return (json.dumps(payload, indent=2, sort_keys=True, ensure_ascii=True) + "\n").encode("utf-8")


def write_pack(path: Path, manifest: dict, entries: dict[str, bytes]) -> None:
    """Deterministic ZIP-under-.bin with atomic replacement; local use only."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=".cviii-assets-", suffix=".tmp", dir=path.parent)
    os.close(fd)
    try:
        with zipfile.ZipFile(temp_name, "w") as pack:
            for name, blob in sorted({**entries, MANIFEST_NAME: json_bytes(manifest)}.items()):
                info = zipfile.ZipInfo(name, date_time=TIMESTAMP)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o600 << 16
                pack.writestr(info, blob, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
        os.replace(temp_name, path)
    finally:
        if os.path.exists(temp_name):
            os.unlink(temp_name)


def read_pack(path: Path) -> tuple[dict, dict[str, bytes]]:
    """Verify every entry, schema, CHR decode, and per-page identities."""
    with zipfile.ZipFile(path, "r") as z:
        names = z.namelist()
        if len(names) != len(set(names)) or MANIFEST_NAME not in names or len(names) > 32:
            raise ValueError("Missing manifest, duplicate files or extra pack entries")
        info = {entry.filename: entry for entry in z.infolist()}
        if any(item.file_size > 1024 * 1024 for item in info.values()):
            raise ValueError("Asset entry exceeds package size limit")
        manifest = json.loads(z.read(MANIFEST_NAME))
        if (manifest.get("schema") != SCHEMA or
            manifest.get("rom_sha256") != JP_SHA or
            manifest.get("mapper") != 24 or
            manifest.get("source_rom_size") != ROM_LENGTH or
            manifest.get("prg_bytes") != PRG_LENGTH or
            manifest.get("chr_bytes") != CHR_LENGTH):
            raise ValueError("Wrong asset schema, ROM fingerprint or cartridge geometry")
        specs = manifest.get("entries", {})
        expected = {RAW_NAME, INDEXED_NAME} | {f"tables/password/{key}.bin" for key in JP_TABLES}
        if set(specs) != expected or set(names) != expected | {MANIFEST_NAME}:
            raise ValueError("Missing/extra asset entries")
        entries: dict[str, bytes] = {}
        for name, spec in specs.items():
            blob = z.read(name)
            if len(blob) != spec.get("size") or sha(blob) != spec.get("sha256"):
                raise ValueError(f"Tampered asset entry: {name}")
            entries[name] = blob
        raw, decoded = entries[RAW_NAME], entries[INDEXED_NAME]
        if len(raw) != CHR_LENGTH or len(decoded) != TILE_COUNT * 64 or encode_chr(decoded) != raw:
            raise ValueError("CHR tile decode does not reconstruct original bytes")
        expected_pages = [sha(raw[i * 1024:(i + 1) * 1024]) for i in range(PAGE_COUNT)]
        if specs[RAW_NAME].get("page_sha256") != expected_pages:
            raise ValueError("CHR page indexes inconsistent")
        for name, (offset, length) in JP_TABLES.items():
            spec = specs[f"tables/password/{name}.bin"]
            if (spec.get("prg_physical_offset"), spec.get("size")) != (offset, length):
                raise ValueError("Password table physical address/index mismatch")
        return manifest, entries


def authenticated_parts(rom: Path) -> tuple[bytes, dict[str, bytes]]:
    # load_tables already verifies exact ROM size, sha256 and all JP table values.
    tables = load_tables(rom)
    data = rom.read_bytes()
    if len(data) != ROM_LENGTH or data[:4] != MAGIC_ROM or data[4:6] != bytes([16, 16]):
        raise ValueError("Wrong pinned JP mapper24 PRG/CHR layout")
    return data[16 + PRG_LENGTH:], tables


def build(rom: Path, path: Path, rebuild: bool = False) -> dict:
    root = Path(__file__).resolve().parents[3]
    output = path.resolve()
    if output == root or root in output.parents:
        raise ValueError("Generated copyrighted assets must be outside the source repository")
    # Authentication is mandatory even if a cache already exists.
    chr_bytes, tables = authenticated_parts(rom)
    if output.exists() and not rebuild:
        old, cached = read_pack(output)
        # A self-consistent archive is not necessarily from the owner's ROM.
        # Compare all extracted source bytes before returning "reused".
        if (cached[RAW_NAME] != chr_bytes or
            any(cached[f"tables/password/{name}.bin"] != tables[name]
                for name in JP_TABLES)):
            raise ValueError("Cache differs from authenticated Japanese ROM; pass --rebuild")
        return {"result": "reused", "path": str(output), "sha256": sha(output.read_bytes()),
                "rom_sha256": old["rom_sha256"], "tiles": TILE_COUNT, "pages": PAGE_COUNT}
    manifest, entries = make_contents(chr_bytes, tables, JP_SHA)
    write_pack(output, manifest, entries)
    read_pack(output)
    return {"result": "rebuilt", "path": str(output), "sha256": sha(output.read_bytes()),
            "rom_sha256": JP_SHA, "tiles": TILE_COUNT, "pages": PAGE_COUNT}


def smoke() -> None:
    """No ROM in CI: pure invented bytes, plus later local retail test."""
    import shutil
    with tempfile.TemporaryDirectory(prefix="jp0597-assets-") as d:
        path = Path(d) / "CVIII-Assets.bin"
        # Synthetic 2bpp tiles: bit 7 of each plane is set, so first pixel = 3.
        raw = bytearray(CHR_LENGTH)
        for tile in range(TILE_COUNT):
            raw[tile * 16] = 0x80
            raw[tile * 16 + 8] = 0x80
        tables = {name: bytes((offset + i) & 255 for i in range(length))
                  for name, (offset, length) in JP_TABLES.items()}
        manifest, contents = make_contents(bytes(raw), tables, JP_SHA)
        assert contents[INDEXED_NAME][:8] == bytes([3, 0, 0, 0, 0, 0, 0, 0])
        write_pack(path, manifest, contents)
        reconstructed, unpacked = read_pack(path)
        assert reconstructed == manifest and unpacked == contents
        first = path.read_bytes()
        write_pack(path, manifest, contents)
        assert path.read_bytes() == first, "Nondeterministic package bytes"
        bad_manifest = json.loads(json.dumps(manifest))
        bad_manifest["entries"][RAW_NAME]["sha256"] = "0" * 64
        write_pack(path, bad_manifest, contents)
        try:
            read_pack(path)
        except ValueError:
            pass
        else:
            raise AssertionError("Corrupt manifest accepted")
        # Validate strict original-ROM gate (only this test supplies fake data).
        fake = Path(d) / "fake.nes"
        fake.write_bytes(MAGIC_ROM + bytes(100))
        try:
            authenticated_parts(fake)
        except ValueError:
            pass
        else:
            raise AssertionError("Unverified retail media accepted")
    print("PASS JP0597 asset pack: 128 CHR pages, 8192 tiles, deterministic, corrupt-cache rejected")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--build", action="store_true")
    action.add_argument("--verify", action="store_true")
    action.add_argument("--smoke", action="store_true")
    parser.add_argument("--rom", type=Path, help="Original authenticated JP ROM (required for build)")
    parser.add_argument("--out", type=Path, help="Private CVIII-Assets.bin path")
    parser.add_argument("--rebuild", action="store_true", help="Rebuild outdated or corrupt local pack")
    args = parser.parse_args()
    if args.smoke:
        smoke()
        return
    if not args.out:
        parser.error("--out required")
    if args.build:
        if not args.rom:
            parser.error("--rom required for --build")
        result = build(args.rom, args.out, args.rebuild)
    else:
        manifest, entries = read_pack(args.out)
        result = {"result": "verified", "path": str(args.out),
                  "sha256": sha(args.out.read_bytes()), "schema": manifest["schema"],
                  "chr_pages": PAGE_COUNT, "chr_tiles": TILE_COUNT,
                  "entries": sorted(entries), "runtime_uses_pack": False}
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
