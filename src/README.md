# Native subsystem sources

`src/collision/Collision.c` and `Collision.h` hold the canonical original-Japanese-ROM collision translation, currently **offline only**. They are not linked into the gameplay runtime and do not constitute fully translated physics.

Future verified translations go in one canonical directory per system (`src/player`, `src/enemies`, `src/world`, `src/graphics`, `src/audio`). Each has a permanent physical-ROM ledger and tests. Do not bring failed experimental margin-stitching renderer code into production by renaming it.

See [programming specification](../docs/rom-translation/PROGRAMMING_SPEC.md) and [current status](../docs/STATUS.md).
