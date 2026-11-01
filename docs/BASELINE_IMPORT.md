# Importing the recovered ROM-backed source

**Current status: source recovery and import tooling; NOT YET a reproducing Windows game.**

The authenticated, archived pre-W8 source contains 75 host/CMake files. The owner-local original JP/enhanced C modules, JP/recomp asset tools, tests, external NESRecomp and recomp-ui sources, and the Japanese ROM are not included in the archived snapshot. W8–W13 enhanced compositor code is not an accepted baseline.

## Authenticated historical source hashes

| File | SHA-256 |
| --- | --- |
| CMakeLists.txt | beff0283313293b2984d62910935293eff92e7c8dc54e4a1dca204995497abcd |
| cviii_modern_host.c | fa1221b9349b2308e407efdcb892944a26e1a8d7b7719617b3faeb7dd4109a51 |
| cviii_room_diag.c | 1313e492f8b3ee8a7b89be77c8ec91865f2fd226805296b5791aa62d594d8b7e |
| Build-CVIII-Phase5H8F.ps1 | e1a92598a135a8cfb9da1b3680d560f137e76b1ca3ac2fd5ad225727084258fe |

Hash agreement proves source identity, not Windows runtime functionality.

## Import on the owner's Windows computer

1. Clone Hyperbog/CV3DCJP, create a branch import/5h8f-working-source, and leave the working game installation unchanged.
2. Download CVIII_5H8F_Source_Recovery_Only.zip from this project. It contains recovered source and no game ROM or extracted artwork.
3. In the Git checkout, execute the following PowerShell command (one line):

    py -3.12 tools/import_legacy_source.py --snapshot "$env:USERPROFILE\Downloads\CVIII_5H8F_Source_Recovery_Only.zip" --workspace "C:\NESDev\CVIII-Decomp" --destination "legacy/rom-backed"

The importer takes the historical host from the pinned archive rather than copying any W13-modified host from the live workspace. It fails before writing the destination if required owner-local sources are absent, the archived source hash differs, or unsafe/binary files appear. It refuses to overwrite existing files.

4. Examine legacy/rom-backed/SOURCE_MANIFEST.json and the staged source files. Run python tools/check_repository.py and python -m unittest discover -s tests -p test_*.py -v. Commit only source and its provenance on your topic branch; open a pull request.
5. Install the upstream NESRecomp and optional recomp-ui checkouts separately. The ROM stays private. Compile and test original-mode execution under Windows/MSVC with the exact matching Japanese retail ROM. Attach the actual build log and gameplay verification before marking the baseline accepted.

## Release conditions

Do not claim a working repository baseline until the clean checkout plus documented local external dependencies reproduces ROM-backed player movement, collisions, enemies, item drops, room transitions, sprites, HUD and audio. Do not commit private ROM bytes, extracted copyright-protected artwork, captures, generated assets, build products or user save files.

The GitHub import tool is an intentionally guarded handoff. It does not itself publish or validate a running game.
