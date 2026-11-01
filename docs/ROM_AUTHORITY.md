# ROM authority and evidence contract

## Priority of sources

1. **The exact original Japanese Castlevania III ROM and its actual executable behavior**: sole authority for original data/rules.
2. ROM-authenticated, reproducible extractor output and trace captures tied to that ROM identity: derived evidence.
3. External maps, disassemblies, RAM maps and screenshots: discovery and independent visual aids only.
4. Provisional layouts, inference and mockups: explicitly *unverified* until proved from the ROM.

Nothing from tiers 2–4 may contradict the ROM. A tool's output is not considered verified merely because it is sourced from ROM bytes; its decoding, bank selection, address mapping, timing, and palette interpretation must also be checked.

## Required provenance

Every imported dataset must have metadata recording:
- ROM edition and SHA-256 (hash recorded; ROM itself not committed)
- Source address/bank and decoder revision when applicable
- Stage/area/room identity and room-local dimensions
- Coordinate transform into stage-global X/Y, with proof of any transitions used
- CHR/tile/metatile IDs and active palette information
- Confidence state: `verified`, `observed`, or `unresolved`
- Comparison to an independent original-ROM reference and identified mismatches

Do not silently replace missing information with zeros, neighboring-room guesses, color matching, or unrelated graphics phases.

## Original behavior vs. enhanced behavior

Original behavior must first be reproduced faithfully. Additional graphical, gameplay, accessibility, or camera features are separate, opt-in systems. They must not be represented as ROM-derived behavior and must not modify the verified reference fixtures.

## Release gate

No release may claim “accurate Stage 0” until the complete stage placement and rendered background have been checked against the original ROM (including transitions and dynamic graphics phases), with mismatch metrics published. An offline renderer matching a self-generated map does not pass this gate.
