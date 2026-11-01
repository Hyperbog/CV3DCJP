# Contributing to CV3DCJP

## Workflow

1. Create a focused branch: `feat/<scope>`, `fix/<scope>`, `docs/<scope>`, or `chore/<scope>`.
2. Keep each pull request small and include its purpose, changed behavior, risks, rollback path and evidence.
3. Run the repository checks and relevant native/Windows tests.
4. Provide the exact ROM revision/hash used in a **local-only** provenance record; do not upload the ROM.
5. Obtain review and passing CI before merging. Do not force-push shared branches or use destructive history rewriting.

## Original-game evidence

Original behavior is authoritative **only when supported by the matching Japanese ROM or its correctly instrumented execution**. Supply machine-readable evidence for each gameplay or tile claim: ROM hash, program/code location or extraction routine, room identity, dimensions, source coordinate system, palette mapping, and expected result. Clearly label observations, hypotheses, and verified conclusions.

Do not invent coordinates, object identities, timing, palette mappings, or missing content. If evidence is incomplete, mark it unresolved and keep it out of verified runtime data. External maps and wikis are guides, **not** overrides.

## Visual acceptance

A test build is not visually correct merely because it compiles or recreates a provisional asset. Require independent comparison against original-ROM captures for static world geometry, pixel/CHR phase, palette state, sprites/HUD, transitions, and determinism. Document frame numbers, observed mismatches, and whether all required rooms were covered. Windows/MSVC execution and actual gameplay must be checked separately from offline tests.

## Distribution

Do not commit or publish ROM bytes, extracted copyrighted artwork, CHR dumps, game-derived framebuffers, private trace archives, or save files. Implement import tools so owners can generate the required assets locally from their own legally obtained ROM. The repository includes scripts and schemas, not the source game content.

## Commit style

Use `type(scope): concise imperative subject`, for example `fix(camera): prevent unqualified world-origin fallback`. Update `docs/STATUS.md` when changing a project's verified/unverified status.
