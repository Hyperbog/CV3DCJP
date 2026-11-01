#!/usr/bin/env python3
"""JP0598 read-only native renderer asset index from JP0597 owner-local pack.

The native index is for optional graphics/metadata access only. It never
replaces the original cartridge's CPU/PPU/memory/mapper behavior.
Only the exact authenticated Japanese game's CHR content is accepted.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path

from jp_asset_pack_0597 import CHR_LENGTH, INDEXED_NAME, RAW_NAME, decode_chr, read_pack

MAGIC = b"CVIII058"
RAW_HASH = "a29eea0ec8baaecac8a51c234a63b974ee1ee65c69985241d725f4b45116c89f"
PIXELS_HASH = "3ee2beb2fa4e118b5f179cd15c9a35e91be530ff0f7bac5937b70069882f16a5"
PIXELS_LENGTH = CHR_LENGTH * 4
INDEX_LENGTH = len(MAGIC) + CHR_LENGTH + PIXELS_LENGTH
FORMAT = "CVIII-JP-native-chr-index/v1"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def layout(raw: bytes, pixels: bytes) -> bytes:
    if len(raw) != CHR_LENGTH or len(pixels) != PIXELS_LENGTH:
        raise ValueError("Native index must contain complete physical Japanese CHR")
    if decode_chr(raw) != pixels:
        raise ValueError("Native index decoded pixels disagree with raw 2bpp source")
    return MAGIC + raw + pixels


def parse(data: bytes, authentic: bool = True) -> tuple[bytes, bytes]:
    if len(data) != INDEX_LENGTH or data[:8] != MAGIC:
        raise ValueError("Invalid JP0598 index size or magic")
    raw = data[8:8 + CHR_LENGTH]
    pixels = data[8 + CHR_LENGTH:]
    if authentic and (digest(raw) != RAW_HASH or digest(pixels) != PIXELS_HASH):
        raise ValueError("CHR index is not the authenticated original-Japanese content")
    if decode_chr(raw) != pixels:
        raise ValueError("CHR index parity failure")
    return raw, pixels


def export(archive: Path, output: Path, rebuild: bool = False) -> dict:
    manifest, entries = read_pack(archive)
    raw, pixels = entries[RAW_NAME], entries[INDEXED_NAME]
    image = layout(raw, pixels)
    parse(image, authentic=True)
    output = output.resolve()
    root = Path(__file__).resolve().parents[3]
    if output == root or root in output.parents:
        raise ValueError("Copyrighted native asset indexes must stay outside repository")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and not rebuild:
        if output.read_bytes() != image:
            raise ValueError("Existing index differs from authenticated pack; use --rebuild")
        action = "reused"
    else:
        fd, temporary = tempfile.mkstemp(prefix=".cviii-native-", dir=output.parent)
        try:
            with os.fdopen(fd, "wb") as dest:
                dest.write(image)
                dest.flush()
                os.fsync(dest.fileno())
            os.replace(temporary, output)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        action = "rebuilt"
    return {"result": action, "format": FORMAT, "index": str(output),
            "bytes": len(image), "sha256": digest(image), "chr_pages": 128,
            "tiles": 8192, "renderer_substitution_enabled": False,
            "rom_sha256": manifest["rom_sha256"]}


def smoke() -> None:
    import random
    import tempfile
    rng = random.Random(598)
    raw = bytes(rng.randrange(256) for _ in range(CHR_LENGTH))
    pixels = decode_chr(raw)
    encoded = layout(raw, pixels)
    assert len(encoded) == INDEX_LENGTH
    assert parse(encoded, authentic=False) == (raw, pixels)
    try:
        parse(encoded, authentic=True)
    except ValueError:
        pass
    else:
        raise AssertionError("Synthetic CHR accepted as original ROM asset data")
    for bad in (encoded[:-1], b"XXXXXXXX" + encoded[8:],
                encoded[:-1] + bytes([encoded[-1] ^ 1])):
        try:
            parse(bad, authentic=False)
        except ValueError:
            pass
        else:
            raise AssertionError("Invalid native index accepted")
    with tempfile.TemporaryDirectory(prefix="jp0598-index-") as d:
        path = Path(d) / "cache.bin"
        path.write_bytes(encoded)
        assert parse(path.read_bytes(), authentic=False) == (raw, pixels)
    print("PASS JP0598 native CHR index: 8192 tiles, lossless decode, size/magic/tamper gates")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument("--export", action="store_true")
    group.add_argument("--verify", action="store_true")
    group.add_argument("--smoke", action="store_true")
    p.add_argument("--assets", type=Path, help="Verified JP0597 CVIII-Assets.bin")
    p.add_argument("--out", type=Path, help="Owner-private native index .dat")
    p.add_argument("--rebuild", action="store_true")
    a = p.parse_args()
    if a.smoke:
        smoke()
        return
    if not a.out:
        p.error("--out is required")
    if a.export:
        if not a.assets:
            p.error("--assets required for export")
        result = export(a.assets, a.out, a.rebuild)
    else:
        raw, pixels = parse(a.out.read_bytes())
        result = {"result": "verified", "format": FORMAT, "bytes": INDEX_LENGTH,
                  "chr_sha256": digest(raw), "pixels_sha256": digest(pixels),
                  "renderer_substitution_enabled": False}
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
