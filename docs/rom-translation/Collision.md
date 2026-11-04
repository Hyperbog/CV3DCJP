# Collision — ROM-matched native translation ledger

## Canonical files

- `src/collision/Collision.c`: complete **currently implemented**, bounded C11 translations consolidated from v2–v9; v9 adds bounded native state effects for the 16 formerly branch-only caller entries.
- `src/collision/Collision.h`: stable, explicit register/RAM entry interface; `CollisionExit` indicates the original CPU boundary and whether the path is supported.
- `docs/rom-translation/translation_ledger.json`: authoritative physical-ROM-byte inventory; 499 nonoverlapping spans.
- `docs/rom-translation/translation_ledger.csv`: human-reviewable mirror of all spans, with PRG and file offsets.
- `tests/check_collision_ledger.py`: checks physical ROM offsets, code/data classifications, and status gates.

## Translation standard — near 1:1 in *behavior*, not syntax

Translate each known original 6502 instruction's observable effects into C: A/X/Y, flags C/Z/N/V where applicable, stack, NES internal RAM, bank switching, and explicit original-ROM control-flow boundaries. Native C conditionals may replace 6502 branch instructions, but results must agree with an independent ROM instruction reference. Never invent effects of unsupported bus accesses, missing handlers, DMA, IRQ, or NMI. Do not collapse path distinctions if they modify flags/RAM.

**Current status is partial.** Earlier offline test reports cover authentic ROM-backed bounded routines; no live substitution, cycle/timing proof, or all-branch closure. The library is not wired into the runtime. `Collision_Caller` now translates all 34 observed entry continuations up to their verified boundaries; downstream calls and table reads are not claimed as translated.

## Cumulative ROM inventory (v9)

| Measure | Bytes |
|---|---:|
| Identified collision-related executable code | 1,264 |
| Implemented in native C (bounded) | 1,150 |
| Identified/decoded, not implemented | 114 |
| Classified data | 60 |
| **Formally retired (live, timing-verified)** | **0** |

The authoritative cartridge contains 262,144 PRG bytes. A PRG byte is retired only when the native translation is substituted into the actual game and verified for cycle, hardware and interrupt behavior. `IMPLEMENTED` is never silently promoted to `VERIFIED` or `RETIRED`. 6502 routine ownership uses *physical* PRG offset and the bank mapping, not CPU PC alone.

## Known limitations

- All 34 caller `JSR` entries have bounded continuations, but many hand off to untranslated paths or external routines.
- `$86FB`: all 55 instruction addresses and 31/32 branch outcomes exercised in 5,007 ROM-differential states; the final $86AA not-taken edge is statically impossible.
- `$968A` composition matched 1,498 synthetic states, with two unsupported-bus cases; not a live verified gameplay hook.
- 16 prior branch-only callers now pass 8,000 ROM-differential states through bounded exits. Downstream targets (such as `$915E` table reads and calls `$97DB` / `$84E8`) remain open.
- No cycle-accurate, mapper-timing, DMA, or interrupt acceptance, and therefore no retired code.
- Collision routines should not be renamed into player physics, damage, or camera mechanics without ROM evidence.

The ROM is **not** committed to Git; tests require an owner-local authentic Japanese ROM.

## v9 acceptance: all recorded caller entries have bounded continuations

- Japanese ROM SHA-256 is unchanged; same fixed bank 14 physical offsets.
- 16 previously branch-only entries gain deterministic native register, flag and RAM operations up to the next known execution boundary. The remaining 18 entry implementations are unchanged.
- 86 **additional** distinct instruction spans cover 171 authentic PRG bytes; entries already in v3 are not double-counted.
- The C native module agrees with an independent 6502 interpreter on **8,000 caller input states**; all 16 tested with both Z-flag outcomes and value-dependent branches.
- `$86FB` independently compared for **5,007 states**: 55/55 instruction addresses visited, 31/32 branch directions observed, and `$86AA` not-taken infeasible by predecessor Z-flag condition.
- The CPU state comparisons include registers, P, 2 KiB RAM, stack pointer, mapped bank, and the mapper write counter. The exits are classified as call/query/RTS/jump/fallthrough.
- Exact scope excludes the subsequent execution of external `JSR`s, indirect/data-table continuation `$915E`, unresolved return targets, and live player physics. These must not be represented as retired code.
- Reproduce locally with `CV3DCJP_JP_ROM=<owner-ROM-path> python -m unittest discover -s tests -p test_collision_v9_rom.py -v`. Without the owner ROM, this test skips.
- Evidence: `docs/rom-translation/evidence/caller_continuations_v9.json` and `closure_86fb_v9.json`; 499 ledger entries, 1,264 identified code bytes, 1,150 implemented, 114 open, 60 data, **0 retired**.

## v10 — CPU-clock parity and guarded runtime substitution boundary

The canonical `$FCDE` C translation now reports **6502 CPU cycle totals** via
`Collision_FCDE_Timed()` without changing the ABI of `Collision_FCDE()`.
Its timing counts the original query instructions, branch-taken/page-cross
penalties, indexed page crossings, and RTS (six cycles); the caller's JSR is
already executed by the time the native routine would enter. The independent
ROM-opcode reference matched all 2,200 tested native states on A/X/Y/P/SP,
RAM (2 KiB), instruction counts, and CPU-cycle totals; the 121 decoded
instruction addresses were exercised.

The added `CollisionLive_ProbeFCDE` is deliberately **read-only**. It validates
original bank-14 `JSR $FCDE` bytes, original JSR return address on the CPU
stack, bank identity, absence of pending asynchronous hardware, and the
fixed-bank native code fingerprint. It calculates the post-RTS program counter,
SP increment, and CPU-cycle delta in an independent snapshot. It explicitly
sets `runtime_commit_authorized=0` and does NOT modify live CPU state.

The original NESRecomp host's `CycHostExtras` interface exposes frame/visual
callbacks, **not** an instruction-fetch CPU replacement callback. Therefore,
linking the native library to the host alone cannot accomplish a safe live
substitution. We have NOT installed a CPU hook into NESRecomp's generated
cycle dispatch or verified its bus cycle ordering. True gameplay substitution
is NOT complete; no ROM spans are retired. Existing launcher, UI, renderer,
CPU and audio baseline are untouched.

An opt-in Mesen2 Lua capture now records the original game at actual `$FCDE`
entry and caller-return boundaries, including all CPU registers, CPU-cycle
counters, and full 2 KiB RAM. The Python verifier checks each measured case
against the native shadow. **No such new capture was executed in v10**;
existing Stage 0 logs omit the full entry A/X/Y/SP/RAM needed to establish
one-to-one live timing for the recorded 37,494 calls.

The remaining gate requires an owner-side Mesen2 capture, a supported
cycle-dispatch hook in the exact NESRecomp revision, and cycle-interleaved
CPU/PPU/APU/DMA/VRC6 and interrupt comparison. Until then, use only the
read-only shadow path.

### Commands (owner-local)

```powershell
# Existing original-rom emulator session: run
# tools/instrumentation/mesen2_collision_fcde_gate_v10.lua
# from Mesen2's Lua console, then stop the script to close the CSV.

py -3 tools/instrumentation/verify_fcde_live_trace_v10.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --trace 'PATH_TO_MESEN_SCRIPT_DATA\cv3dcjp-fcde-v10-TIMESTAMP.csv' `
  --report 'data/generated/collision-v10-live-evidence.json'
```


## v11 — ROM-authenticated live-trace preflight (2026-11-03)

The new tools/instrumentation/verify_fcde_trace_gate_v11.py validates the
Japanese ROM SHA-256 and all 36 Mesen-v10 instrumented bank-14 JSR $FCDE sites
before running the native shadow comparison. It rejects malformed CSV,
non-decimal and out-of-range CPU states, nonpositive clock deltas,
invalid return-stack bytes, and non-bank-14 entry RAM. It records
the observed subset of instrumented callers and the CSV SHA-256. The capture
hash does NOT prove Mesen generated the file.

The collision translation ledger lists 34 observed caller continuations,
whereas the Mesen instrument script monitors 36 ROM-confirmed JSR sites.
These are distinct scopes and MUST NOT be reported as the same coverage
population. Other JSR-looking ROM byte sequences have NOT been established
as executed code.

No new Mesen2 live capture was run in v11. No CPU-bus, PPU, APU, DMA, VRC6,
IRQ or NMI cycle-interleaving parity has been established. No CPU instruction
dispatch hook has been installed in NESRecomp. Game runtime is untouched,
native runtime substitution remains prohibited, and zero PRG bytes are retired.

Owner-side run after capturing with the existing v10 Mesen Lua script:

    py -3 tools/instrumentation/verify_fcde_trace_gate_v11.py --rom ROM.nes --trace Mesen-v10.csv --report data/generated/collision-v11-preflight.json

This is a stricter read-only, ROM-checked preflight, NOT live substitution.

## v11 — CPU dispatch observer & owner-run MesenCE capture

The project contains the optional `src/collision/CollisionDispatchV11.c` NESRecomp
hook adapter. It compiles against pinned upstream `2b4fcdb98c9e702be3fb365cfc645003c50b5fc1`
and observes `$FCDE` plus the original bank-14 `$8345 -> $8348` path. Both
callbacks return zero to execute the original 6502 code and all its bus cycles.
The default build leaves it out (`JP_COLLISION_V11_SHADOW=OFF`). An opt-in
build needs two `[[mod_function_hook]]` entries in an owner-local game.toml.
An owner-private original-ROM file is loaded after full-ROM FNV64 checking;
CMake still verifies the Japanese ROM SHA-256. `--collision-v11-rom` enables
the observer, and `--collision-v11-trace` records an optional 15-column CSV.

`tools/instrumentation/mesen2_collision_fcde_gate_v11.lua` uses MesenCE's
`emu.getCpuCycleCount()` and retains the v11 strict verifier CSV contract.
Neither MesenCE nor the Windows game host is installed in this environment.
**Real owner-side capture rows: 0; observed native callbacks: 0; real cross-
engine matches: untested; hardware bus-cycle parity: untested; retired bytes: 0.**
See [COLLISION_V11_CAPTURE_DISPATCH.md](COLLISION_V11_CAPTURE_DISPATCH.md).

## v11 — owner-supplied 128-row Mesen capture and ROM replay

An owner-supplied `cv3dcjp-fcde-v11-20261103-211957.csv` (SHA-256
`ca19f8453f2b0909f51704e75b809eaeacf0243109a7a4e1efff3b5d1ebbaf19`)
passed the strict v11 CSV preflight: **128 calls** at authenticated original
bank-14 `$8429` (66), `$96E4` (31), and `$96ED` (31). Independent
original-ROM opcode replay reproduced **all 128** A/X/Y/P/SP and 2-KiB RAM
return states. CPU-clock totals matched exactly for **110**; the remaining
**18** recorded four extra cycles (+4 each; no other residuals). An APU DMC
DMA stall is a *hypothesis*, not verified from this CSV because bus/DMC events
were not captured. See `evidence/fcde_v11_mesen_128_capture_rom_replay.json`.

The opt-in NESRecomp dispatch observer now includes the three captured
callers plus original `$8345` (five hook sites including the shared entry).
It is read-only, still disabled by default. It has not been executed here.
No native C parity from this CSV, synchronized cross-engine replay or complete
hardware bus cycle timing has been established. **Zero PRG code bytes retired.**
