# Verified project status

**Last editorial update: 2026-10-31**

| Area | Current evidence | Status |
| --- | --- | --- |
| Repository | Newly initialized GitHub hub | Bootstrap |
| Working original gameplay | 156 original support files and authenticated pre-W8 5H8F ROM-backed host merged to `main` in PR #5 and PR #6 | **Source imported; external diagnostic tools and reproducible Windows build still pending** |
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

## Historical source handoff

The guarded importer and pre-W8 source hash manifest are documented in [BASELINE_IMPORT.md](BASELINE_IMPORT.md). Issue #2 remains open: original Git-tracked modules are on the import PR; the historical host and any non-Git tracked tools have not yet been committed, and the source has not been built or validated from this repository.

## Historical source post-import audit

PR #6 merged the authenticated 5H8F host. A fresh audit of the historical Windows launcher identified five absent diagnostic Python source files from the owner's local archive. They can be staged with the hash-pinned [diagnostic source importer](../tools/import_local_diagnostics.py) (see [handoff](DIAGNOSTIC_SOURCE_HANDOFF.md)). Neither a clean Windows/MSVC build nor original-ROM live gameplay has been verified from this repository. Do not run the old launcher with its existing build/UI modification defaults.
