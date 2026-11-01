# Verified project status

**Last editorial update: 2026-10-31 (owner local time)**

| Area | Current evidence | Status |
| --- | --- | --- |
| Repository | Source import PRs #5, #6, #8; isolated build script PR #9 | **Source and build tooling on main** |
| Original Japanese ROM-backed gameplay | Owner reports on 2026-10-31: “Rom loads. Plays as natively intended.” after the isolated Windows build instructions | **Owner-observed functional gameplay**; full regression and ROM-frame parity not independently measured |
| Original ROM identity | Isolated build script checks size 393232 bytes and SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28` before running | **Identity-gated locally**; build logs not archived in repository |
| Recovered source | 156 pinned Japanese support sources, 76 host/launcher sources, and 5 diagnostic tools/tests imported | **Tracked in GitHub with provenance** |
| Stage 0 room-local graphics/data | Extracted in earlier private project artifacts | **Not independently verified by this repository** |
| Complete Stage 0 room placement | Prior map has unverified room-world origins | **Unverified** |
| W8–W13 enhanced rendering | Owner screenshots show scene seams and incorrect visuals | **Failed visual acceptance; not a baseline** |
| True native, resolution-independent renderer | Still separate from the ROM-backed original-mode gameplay | **Not yet accepted** |
| Repository CI | GitHub Actions source-policy and Python tests run on Windows and Linux | **Passing at last merge**; does not test live ROM gameplay |

## Evidence levels

- **Owner-observed:** The owner ran the isolated Windows workflow and reports that the ROM loads and plays as natively intended. This is meaningful functional acceptance of the current *original-ROM mode* on their machine.
- **Repository-verified:** Historical sources are present; import hashes, source safeguards, Python tests and GitHub Windows/Linux repository CI have been checked.
- **Not yet independently established:** Full-game playthrough, per-frame ROM parity, audio timing measurements, all stage transitions, a clean build log retained as evidence, and all enhanced native-renderer features.

**Do not infer native graphics parity, complete Stage 0 world correctness, or widescreen acceptance from the successful original-ROM play session.**

## Preserved reference baseline

The ROM-backed original-mode executable from [WINDOWS_BASELINE_BUILD.md](WINDOWS_BASELINE_BUILD.md) is now the **owner-confirmed functional reference**, not a certified frame-perfect reimplementation. Keep its original gameplay execution and original presentation unchanged while working on ROM-derived world data, camera independence and native rendering.

The protected isolated build path is `C:\NESDev\CV3DCJP-Builds\5h8f-original`; the older existing game installation is `C:\NESDev\AkumajouDensetsu\build-5h8f`. Do not overwrite either with experimental graphics builds.

## Next original-game verification work

1. Preserve and record the precise baseline commit, build log, ROM identity, CTest output, and a short reproducible launch/control/transition log, **without checking in the ROM or frame assets**.
2. Compile Stage 0 terrain and transitions from the exact original Japanese ROM with explicit provenance. Fail when a room origin or active graphics phase is unresolved.
3. Compare native output to **independently recorded original-ROM output**, not against W9's provisional assembled bitmap.
4. Connect the modern camera and renderer only after these independent checks; recheck playable gameplay in the same native window at multiple resolutions.

See [ROM_AUTHORITY.md](ROM_AUTHORITY.md), [ARCHITECTURE.md](ARCHITECTURE.md), [SOURCE_IMPORT_PROVENANCE.md](SOURCE_IMPORT_PROVENANCE.md), and [WINDOWS_BASELINE_BUILD.md](WINDOWS_BASELINE_BUILD.md).
