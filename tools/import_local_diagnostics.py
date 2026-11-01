#!/usr/bin/env python3
"""Import exactly the post-5H8F diagnostic helpers recovered from NESDev.zip.

The owner's working CVIII-Decomp tree is read-only. All source identities were
captured from the uploaded NESDev.zip, not inferred from filename similarity.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

SOURCE_HASHES = {
    "JP/recomp/tools/cviii_camera_report_5h3.py":
        "8e0724fec5ec4d63d6e6679baddae91766e5b7681979ce4672c27afdb9085843",
    "JP/recomp/tools/cviii_camera_report_5h4.py":
        "806db543f62e5051c9fab70fb8212830714f279df8f611c2938e54e851ad403d",
    "JP/recomp/tools/cviii_diagnostic_report_5h8.py":
        "cd41acf5478e4d1b9549f3c99b8e63535589ed0a7c4e3165502fcf06a4a7930a",
    "JP/recomp/tests/test_cviii_camera_report_5h3.py":
        "0dbdf7bff5c46f8944bfb7c8e78024f08ca895272fefc077bfe463c9c8272ae3",
    "JP/recomp/tests/test_cviii_camera_report_5h4.py":
        "953c38148995c1575658b7eb91f7c225e881dad6efa449dd89e37d0ff66ca475",
}


def check_sources(workspace: Path, dest: Path) -> list[tuple[Path, bytes]]:
    """Preflight every dependency before modifying the repository."""
    checked = []
    for relative, expected in SOURCE_HASHES.items():
        src = workspace / relative
        target = dest / relative
        if not src.is_file() or src.is_symlink():
            raise FileNotFoundError(f"missing original source: {src}")
        if target.exists() or target.is_symlink():
            raise FileExistsError(f"refusing to overwrite: {target}")
        data = src.read_bytes()
        if not data or len(data) > 1_048_576 or b"\0" in data:
            raise ValueError(f"not a small text source: {src}")
        data.decode("utf-8")
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected:
            raise ValueError(
                f"source SHA-256 mismatch: {relative}\n"
                f"expected {expected}\nactual   {actual}"
            )
        checked.append((target, data))
    return checked


def import_checked(checked: list[tuple[Path, bytes]]) -> None:
    written = []
    try:
        for target, data in checked:
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as output:
                output.write(data)
            written.append(target)
    except BaseException:
        for path in written:
            path.unlink(missing_ok=True)
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument(
        "--destination", type=Path,
        default=Path(__file__).resolve().parents[1] / "legacy/rom-backed"
    )
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    checked = check_sources(args.workspace, args.destination)
    print(f"PASS: {len(checked)} pinned diagnostic source files checked")
    if not args.check_only:
        import_checked(checked)
        print("STAGED: five verified Python source files; no ROM or assets imported")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, UnicodeError) as exc:
        print(f"IMPORT BLOCKED: {exc}", file=sys.stderr)
        raise SystemExit(1)
