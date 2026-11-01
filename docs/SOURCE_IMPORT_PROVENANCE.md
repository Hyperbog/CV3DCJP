# Imported Japanese ROM support source — provenance

This directory contains **historical original-game support source**, not an accepted game executable or native renderer.

## Pinned source revision

- Source repository: https://github.com/Hyperbog/CVIII-Decomp
- Original source revision: `f7f533bbcc563560835ce734dc922adbb8e18413`
- Source directories: `JP/recomp/native/enhanced/`, `JP/recomp/tools/`, `JP/recomp/tests/`
- Imported **156** UTF-8/C/PowerShell files, preserving each relative path and verifying its source Git blob SHA before import.
- Imported GitHub source is consistent with the `.git` checkout embedded in the owner's uploaded `NESDev.zip`; files were fetched from the exact revision, **not from the modified local working tree**.

## Historical ROM-backed host

The authenticated original pre-W8 5H8F host is preserved separately in `CVIII_5H8F_Source_Recovery_Only.zip` with **75 source/CMake files and one build launcher**. Its SHA-256 anchors are in [BASELINE_IMPORT.md](BASELINE_IMPORT.md) and enforced by `tools/import_legacy_source.py`.

The GitHub importer writes only previously absent host files and a manifest; it never overwrites the pinned enhanced modules. This archive is not in GitHub until the source-only import is completed.

## External execution dependency

The owner's uploaded `nesrecomp` directory is an independent upstream checkout:

- Upstream: https://github.com/mstan/nesrecomp
- Embedded checkout revision: `fc0bf3fa46a710464960c0a1f2d3b83b4d345600`
- Track as a pinned **external dependency**; do not vendor its build folders, binaries, `.git` internals, or proprietary data without explicit review.

The Japanese ROM remains local and has expected SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`. The original ROM is the sole authority for game data and behavior. No ROM data is included in this import.

## Verification limits

- Source provenance: pinned Git commit and matched Git blob identities.
- Repository hygiene: automated source/binary policy.
- Windows/MSVC ROM-backed game: **not yet built from the CV3DCJP checkout**.
- Native graphics/gameplay parity: **not established**.
- Later owner-local post-5H8F scripts absent from the original Git revision may still be needed; they must be reviewed as separate, explicitly identified imports.

See [Issue #2](https://github.com/Hyperbog/CV3DCJP/issues/2). No W8–W13 experimental compositor is promoted as verified.
