# CV3DCJP

**Castlevania III: Dracula's Curse — Japanese ROM-authoritative PC-native reconstruction**

> **Status: repository bootstrap.** No playable or visually verified native build has been committed here yet. Historical local prototypes and captures are not release baselines.

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

Import the **known-working ROM gameplay baseline** with reproducible build instructions, then implement and validate a ROM-accurate Stage 0 asset compiler before reconnecting the modern renderer. Do not promote W8–W13 experimental renderer outputs as verified world maps.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/ROM_AUTHORITY.md](docs/ROM_AUTHORITY.md), and [CONTRIBUTING.md](CONTRIBUTING.md) when the repository foundation PR is merged.
