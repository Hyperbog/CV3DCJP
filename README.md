# CV3DCJP

**Castlevania III: Dracula's Curse — Japanese ROM-authoritative PC-native reconstruction**

> **Status: ROM-backed original gameplay owner-confirmed on Windows (2026-10-31).** The owner reports that the original Japanese ROM loads and plays as natively intended using the recovered host. This does **not** establish complete frame parity or a verified native widescreen renderer; see [docs/STATUS.md](docs/STATUS.md).

## Non-negotiable source of truth

The **original Japanese game ROM** is the only authority for original tile layouts, graphics, palettes, collision, game rules, CPU behavior, object lifecycles, and room transitions. Evidence from a matching ROM execution may be used to verify those facts. Community maps and documentation are supporting references only; they cannot override ROM-derived results. **No guessed placements or behaviors may be labeled verified.**

## Technical direction

Build a modern, resolution-independent PC engine. Keep the game world, independent 2D camera, native graphics renderer, and gameplay state separate. Use ROM-backed execution for behavior until specific routines have been matched and replaced by verified native implementations. Do not reuse the experimental NES framebuffer/margin-stitching approach for the native display.

## Repository rules

- Submit changes on focused branches via reviewed pull requests.
- Keep verified original behavior separate from optional enhancements.
- Record evidence and verification scope for gameplay and graphics changes.
- Treat Windows builds **and** live visual/gameplay parity as acceptance gates; compilation and unit tests alone are not proof of correctness.
- Do **not** commit game ROMs, decrypted/extracted proprietary graphics, private captures, save data, or large generated asset databases.
- Track incomplete integration and failing parity in [docs/STATUS.md](docs/STATUS.md).

## Next action

Preserve the **owner-confirmed original-ROM gameplay baseline** and verify a ROM-accurate Stage 0 asset compiler, global room placement and original-frame parity before reconnecting the modern renderer. Do not promote W8–W13 experimental renderer outputs as verified world maps.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/ROM_AUTHORITY.md](docs/ROM_AUTHORITY.md), and [CONTRIBUTING.md](CONTRIBUTING.md) for the current verification and development contracts.
