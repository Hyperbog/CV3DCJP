# ROM-derived content contracts

This directory tracks **schemas and source-safe documentation only**. All generated original game content belongs in ignored `data/generated/`.

- `schemas/rom-room-v1.schema.json`: a ROM-local stage/area/room record with explicit unresolved world position.
- `../tools/extract/export_content_v1.py`: deterministic original Japanese ROM inventory/export, using the authenticated room and graphics extractors.
- `../docs/CONTENT_ARCHITECTURE.md`: repository versus final-release layout, domain separation, legal boundaries and validation tiers.

The current private exporter creates **15 stage selectors, 135 room files, 15 metatile groups and all physical CHR patterns**. Palette phase, transitions, collision semantics, audio and gameplay decoders are not yet accepted. A successful ROM data export does not establish independent visual frame parity.

Never commit raw original ROM, CHR patterns, generated room art, audio, copyrighted asset packs or private captures.
