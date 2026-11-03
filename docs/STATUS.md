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

## ROM content catalog v1 — 2026-10-31

The exact Japanese owner ROM was decoded locally using the already-audited whole-room and metatile extractors: **15 ROM stage selectors**, **135 room references**, **130 distinct layout payloads**, **1,563 metatile definitions**, and **8,192 physical CHR tiles**. A 172-file **private** archive contains the stage/area/room JSON, room-local screen sections, shared metatile tables and physical CHR patterns. No original graphic data is committed to GitHub.

The public ROM-authenticated generator is [tools/extract/export_content_v1.py](../tools/extract/export_content_v1.py); the release-oriented content layout is [CONTENT_ARCHITECTURE.md](CONTENT_ARCHITECTURE.md). These figures are **ROM-local data extraction only**, not independent room-world placement or renderer pixel parity. Unknown coordinates remain null; real-time palettes, camera transitions, audio and native gameplay decoders remain unresolved. Issue #3 remains open.

## Stage 0 room and palette observation audit v2

The authenticated 5H8F playback captures contain **13,944 continuous frames**, **24 distinct frame-end PPU palette-RAM states**, and **7 stable observed Stage 0 room selector sequences** (10 total room-ID runs, including startup and a one-frame transient). Historical source confirms **16 static ROM instruction sites** that write selectors; the separate 5H8S execution capture contains 43 writes but only one attributed to one of those sites, without frame synchronization to the 5H8F PPU session.

The private, owner-only Stage 0 v2 evidence pack retains eight room audit records, observed selector runs, 32-byte per-frame palette states, exact source hashes, and ROM static writer offsets. **No new ROM-verified world placements, palette ROM tables, or physical room directions have been established.** Two previously observed relative room placements are preserved separately with their historical 5H8M provenance; remaining six Stage 0 room positions stay unresolved. Independent emulator-frame pixel parity remains pending. See [STAGE0_EVIDENCE_V2.md](STAGE0_EVIDENCE_V2.md) and Issue #3.

## Stage 0 synchronized capture v3 — owner execution analyzed

Owner supplied a private single-run v3 capture (SHA-256 `e4896489df9fa6bbc6f5a86952fee0e16c32b350962756cc6e73830ea589b5cc`). Independent checks against the exact Japanese ROM confirm 14,847 frame-end palette records and 14,847 scanline records with correct digests, **28 of 28 CPU selector writes attributed to original ROM 6502 instructions across eight unique executed sites**, 15 consistent write/frame groups, nine recorded selector changes (including bootstrap and one-frame transient), and zero unexplained selector changes. The trace includes **26 distinct frame-end palettes / 29 changes**, but cannot yet identify original PRG palette producers. The copied executable itself is absent from the uploaded ZIP, so this follow-up did not independently rehash its executable receipt. **No new verified global coordinates or independent original-emulator pixel parity**. Full proof boundaries and exact byte offsets: [STAGE0_SYNCHRONIZED_V3_RESULTS.md](STAGE0_SYNCHRONIZED_V3_RESULTS.md). Issue #3 remains open.

## Collision native CPU-cycle shadow — v10 (2026-11-03)

The native C translation of original JP $FCDE now reports exact NMOS 6502 CPU-cycle totals for 2,200 synthetic ROM-reference comparisons (all 121 decoded instructions exercised), matching registers, flags, stack state and 2 KiB RAM. A read-only, fail-closed substitution probe validates mapper, caller JSR, return stack and ROM fingerprint. The original NESRecomp host currently has no validated CPU instruction-fetch/bus-cycle interception hook. **No live native substitution was executed; no original machine-code bytes have been retired.** Mesen2 source capture and offline verifier are included but have not been run as a new live capture. Collision inventory unchanged: 1,264 known executable bytes, 1,150 natively implemented, 114 open, 0 retired. See [Collision.md](rom-translation/Collision.md).
