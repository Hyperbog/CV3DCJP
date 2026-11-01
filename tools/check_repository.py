#!/usr/bin/env python3
"""Fail CI on accidentally committed ROMs, private captures, or build outputs."""
from __future__ import annotations

import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]
FORBIDDEN_EXTENSIONS = {
    ".nes", ".fds", ".nsf", ".srm", ".sav", ".oam", ".chr", ".ciram",
    ".ppu", ".bin", ".sqlite", ".sqlite3", ".db", ".zip", ".7z",
    ".rar", ".bmp", ".exe", ".dll", ".obj", ".lib", ".pdb",
}
FORBIDDEN_DIRECTORIES = {
    "roms", "private", "captures", "generated", "build", "out",
    "cmakefiles", ".vs",
}
REQUIRED_FILES = {
    "README.md", "CONTRIBUTING.md", "docs/ROM_AUTHORITY.md",
    "docs/ARCHITECTURE.md", "docs/STATUS.md",
}
MAX_TRACKED_BYTES = 5 * 1024 * 1024


def inspect(path: str, data: bytes) -> list[str]:
    """Return repo-policy violations for a single tracked path and its raw bytes."""
    p = pathlib.PurePosixPath(path)
    problems: list[str] = []
    if p.suffix.lower() in FORBIDDEN_EXTENSIONS:
        problems.append(f"{path}: prohibited extension {p.suffix}")
    if any(part.lower() in FORBIDDEN_DIRECTORIES or part.lower().startswith("build-")
           for part in p.parts[:-1]):
        problems.append(f"{path}: private/generated/build directory")
    if len(data) > MAX_TRACKED_BYTES:
        problems.append(f"{path}: tracked file exceeds 5 MiB")
    if data.startswith((b"NES\\x1a", b"FDS\\x1a")):
        problems.append(f"{path}: original game cartridge header detected")
    return problems


def tracked_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"], cwd=REPO, check=True, capture_output=True
    )
    return [p.decode("utf-8") for p in result.stdout.split(b"\\0") if p]


def main() -> int:
    paths = tracked_files()
    failures: list[str] = []
    failures.extend(f"{name}: required file is missing" for name in
                    sorted(REQUIRED_FILES - set(paths)))
    for path in paths:
        absolute = REPO / path
        if not absolute.is_file():
            failures.append(f"{path}: tracked file missing from working tree")
            continue
        failures.extend(inspect(path, absolute.read_bytes()))
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    print(f"PASS: {len(paths)} tracked files; no forbidden ROM/capture/build content")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
