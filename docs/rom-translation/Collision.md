# Collision — ROM-matched native translation ledger

## Canonical files

- `src/collision/Collision.c`: complete **currently implemented**, bounded C11 translations consolidated from v2–v7; the helper code is unchanged except moving it into one C compilation unit.
- `src/collision/Collision.h`: stable, explicit register/RAM entry interface; `CollisionExit` indicates the original CPU boundary and whether the path is supported.
- `docs/rom-translation/translation_ledger.json`: authoritative physical-ROM-byte inventory; 413 nonoverlapping spans.
- `docs/rom-translation/translation_ledger.csv`: human-reviewable mirror of all spans, with PRG and file offsets.
- `tests/check_collision_ledger.py`: checks physical ROM offsets, code/data classifications, and status gates.

## Translation standard — near 1:1 in *behavior*, not syntax

Translate each known original 6502 instruction's observable effects into C: A/X/Y, flags C/Z/N/V where applicable, stack, NES internal RAM, bank switching, and explicit original-ROM control-flow boundaries. Native C conditionals may replace 6502 branch instructions, but results must agree with an independent ROM instruction reference. Never invent effects of unsupported bus accesses, missing handlers, DMA, IRQ, or NMI. Do not collapse path distinctions if they modify flags/RAM.

**Current status is partial.** Earlier offline test reports cover authentic ROM-backed bounded routines; no live substitution, cycle/timing proof, or all-branch closure. The library is not wired into the runtime. `Collision_Caller` can return immediate branch decisions for identified but untranslated caller tails; they are not counted as high-level translated bytes.

## Cumulative ROM inventory (v7 → canonical v8)

| Measure | Bytes |
|---|---:|
| Identified collision-related executable code | 1,093 |
| Implemented in native C (bounded) | 979 |
| Identified/decoded, not implemented | 114 |
| Classified data | 60 |
| **Formally retired (live, timing-verified)** | **0** |

The authoritative cartridge contains 262,144 PRG bytes. A PRG byte is retired only when the native translation is substituted into the actual game and verified for cycle, hardware and interrupt behavior. `IMPLEMENTED` is never silently promoted to `VERIFIED` or `RETIRED`. 6502 routine ownership uses *physical* PRG offset and the bank mapping, not CPU PC alone.

## Known limitations

- 34 caller `JSR` instructions are identified; not all continuations are translated.
- `$86FB` has 52/55 previously executed instructions and 25/32 observed conditional branch edges; closure remains open.
- `$968A` composition matched 1,498 synthetic states, with two unsupported-bus cases; not a live verified gameplay hook.
- No cycle-accurate, mapper-timing, DMA, or interrupt acceptance, and therefore no retired code.
- Collision routines should not be renamed into player physics, damage, or camera mechanics without ROM evidence.

The ROM is **not** committed to Git; tests require an owner-local authentic Japanese ROM.
