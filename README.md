# CV3DCJP

**Castlevania III: Dracula's Curse — Japanese ROM-authoritative PC-native reconstruction.**

**Status:** ROM-backed original gameplay has been observed running on Windows. Full native game reimplementation, widescreen, original-frame parity and all-stage timing correctness are **not complete**. See [verified project status](docs/STATUS.md).

## Source of truth

The authenticated original Japanese ROM is authoritative for original geometry, graphics, collision, player and enemy behaviors, audio, camera transitions and hardware behavior. Only ROM-derived or independently observed and reproduced behavior may be described as verified. Unknown data remains unknown.

## Current development workflow: one permanent branch

**`main` is the only permanent development branch.** Each 6502 subsystem is translated into a canonical C module, e.g. [`src/collision/Collision.c`](src/collision/Collision.c), and tracked with an exact physical-PRG-byte [translation ledger](docs/rom-translation/translation_ledger.json). A routine is **not retired** merely because its C source compiles or passes offline tests: live substitution, timing and side-effect parity are required.

Temporary review branches must be merged and removed promptly. See [repository maintenance audit](docs/repository/GIT_AUDIT_2026-11-02.md) and [branch cleanup](tools/git/Prune-Merged-Branches.ps1). Never replace the protected ROM-backed gameplay build or launcher with a diagnostic test application.

## Repository layout

- `src/`: native subsystem translations. Collision is implemented in isolation; not connected to live gameplay.
- `legacy/rom-backed/`: original ROM-backed C source, launcher interfaces, historic presentation and extraction tools; preserved pending reviewed migration.
- `tools/`: owner-side ROM extraction, regression scripts, guarded Git maintenance.
- `tests/`: public ROM-free CI and owner-local ROM-verified tests.
- `docs/`: programming specification, status, evidence and audit records.
- `data/`: public schemas and non-proprietary metadata. Private generated content stays outside Git.

Read [architecture](docs/ARCHITECTURE.md), [ROM authority](docs/ROM_AUTHORITY.md), [status](docs/STATUS.md), and [contributing policy](CONTRIBUTING.md). Never commit the commercial game ROM, ripped CHR/audio assets, private captures, save files or build artifacts.
