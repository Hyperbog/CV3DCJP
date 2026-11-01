# Verified project status

**Last editorial update: 2026-10-31**

| Area | Current evidence | Status |
| --- | --- | --- |
| Repository | Newly initialized GitHub hub | Bootstrap |
| Working original gameplay | User has demonstrated functioning ROM-backed gameplay locally | Observed externally; **not imported or built in this repository** |
| Stage 0 room-local graphics/data | Extracted in earlier private project artifacts | Available outside repository; **not independently verified by this repo** |
| Complete Stage 0 world placement | Several origins were provisional in previous mockups | **Unverified** |
| W8–W13 enhanced rendering | User screenshots show visible seams and graphics discontinuities | **Failed visual acceptance; not a baseline** |
| Native PC engine | Clean architecture described, components exist in local prototypes | **Not yet an accepted integrated build** |
| CI | Repository hygiene tests proposed on foundation PR | Pending merge |

## Immediate acceptance sequence

1. Import the minimal known-working ROM-backed gameplay source and its Windows build scripts without graphics modifications.
2. Establish exact ROM identity, reproducible local extraction and evidence manifest format.
3. Compile all original Stage 0 terrain from authenticated ROM data; resolve room origins from ROM transitions.
4. Compare the complete native Stage 0 output against independent original-ROM captures and report mismatches.
5. Attach the native renderer to the behavior engine only after Stage 0 geometry validation.
6. Verify gameplay, resolution changes, transitions, sprites, HUD, entities, drops, persistence, and Windows presentation in one executable.

**Rule:** No unverified W8–W13 branch or stage image is to be promoted to release or used as an authoritative fixture.
