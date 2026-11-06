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



## v11.2 — actual NESRecomp DMA event sidecar and paired verifier

**Source is committed and the pinned NESRecomp ABI compile job passes.**
No new NESRecomp gameplay run or Mesen DMA event capture is available yet.

The opt-in adapter chains the upstream cyc_trace_cycle_hook defined in pinned
runner/cyc/cyc_trace.h. It counts real DMA cycles while the original $FCDE
instruction sequence runs. Every paired call emits a separate DMA sidecar row:
guest entry/return clocks, native C instruction-cycle prediction, real DMA
cycles, halt/read/write subdivisions, conservative DMC source-address
advances and OAM $2004 writes. Nothing changes CPU or hardware cycles.
A Mesen +4 clock discrepancy is never silently normalized as DMA.

The host must be started with --hash-out FILE, which enables its real cycle
tracer, BEFORE the adapter's --collision-v11-dma-trace FILE option.
Without tracing, the adapter refuses to capture DMA instead of reporting
fake zero stalls. An existing trace callback is chained, not overwritten.

The following are arguments for the owner-built private
jp_akumajou_densetsu_modern executable (not a standalone command).
Supply a verified input schedule using --input FILE as well:

    --frames 2000
    --hash-out C:\NESDev\capture\native-hashes.txt
    --collision-v11-rom "C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes"
    --collision-v11-trace C:\NESDev\capture\collision-native.csv
    --collision-v11-dma-trace C:\NESDev\capture\collision-native-dma.csv
    --input C:\NESDev\capture\same-controls.txt

A genuine paired execution also requires matching reset, PPU phase, and
controller history. The original 128 Mesen rows contain neither the
per-frame inputs nor per-cycle bus/DMC activity. Do not infer they describe
the same execution as a future native trace.

Run the fail-closed verifier after capturing the native files:

    py -3 tools/instrumentation/verify_fcde_paired_dma_v11.py --rom "C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes" --mesen "C:\NESDev\capture\cv3dcjp-fcde-v11-20261103-211957.csv" --native "C:\NESDev\capture\collision-native.csv" --dma "C:\NESDev\capture\collision-native-dma.csv" --report "C:\NESDev\capture\collision-paired-v11.json"

The paired verifier rejects missing/out-of-order samples, different full
entry/return CPU state, native DMA not accounting for actual elapsed
cycles beyond independent ROM instruction timing, or mismatched
cross-engine clock totals. It does NOT establish Mesen per-bus-cycle
parity, authentic input/movie provenance, or DMC DMA causality from the
existing Mesen CSV. New native execution samples: 0. Retired PRG bytes: 0.


## v11.2 owner synchronized DMC and IRQ observations

A new MesenCE-labeled three-file synchronized capture was uploaded
(owner ZIP SHA-256 08d78c01b26921b72b7606c441331aa7862a4ff080d92c9225b661137b151d9a).
ROM-verified original bank-14 callers: $8429 (100), $96E4 (14),
$96ED (14), all 128 precisely matched to the DMC boundary sidecar.

The independent original-ROM 6502 reference matches all 128 returned
A/X/Y/SP/P and 127 of 128 complete 2-KiB RAM states. CPU instruction
time equals measured elapsed clocks in 112 samples. Fifteen samples
have +4 clocks each, and all fifteen show DMC source address +1 and
bytes remaining -1; no exact-cycle sample changes the DMC source
address. Sample #101 at $96ED has +246 clocks and one DMC reader
advance. Its eight extra stack writes exactly reproduce the original
Japanese ROM IRQ vector $FFFE -> $E0F5, handler's first 11 opcodes
(48 8A 48 98 48 A5 00 48 A5 01 48), saved return PC $FCEB,
and two original zero-page values. This explains why an uninterrupted
$FCDE-only interpreter cannot match the interrupt-overlap sample.

The controller trace has 9,749 input polls; a consecutive post-reset
sequence covers frames 2..9227. No NESRecomp session has been paired
against the same input, reset and PPU phase. Neither exact IRQ cycles
nor actual Mesen bus transactions are proven. The new fail-closed
ROM+ZIP analyzer is tools/instrumentation/verify_fcde_sync_dmc_irq_v11.py.
Aggregate evidence only: evidence/fcde_v11_2_sync_dmc_irq_owner_capture.json.
No raw private RAM/ROM/CSV in Git, no native substitution,
no original machine-code bytes retired.


## v11.3 — first real original-CPU NESRecomp collision capture

An owner-provided ZIP (SHA-256
eb2bb7e3631273243756cceaa06a9f01511460254c8d0572c9434b1dd73fbcf6)
from the exact pinned upstream engine demonstrates **actual**
headless original-ROM execution. The ROM-derived, original-CPU
observer recorded 212 $FCDE calls: $8429=128, $96E4=42 and
$96ED=42. Independent 6502 ROM instruction replay matched
**212/212 A/X/Y/SP/P and complete 2-KiB RAM**; original CPU
instruction-cycle predictions generated by Collision_FCDE_Timed
matched **212/212** ROM-reference predictions. The 212 native
elapsed-time windows equal ROM instruction clocks plus real native
DMA sidecar cycles. Native sample DMA: 193 zero, 16 four-cycle
and 3 three-cycle, total 19 DMC sample fetches. This checks the
behavior of the original CPU and C shadow prediction; it is NOT
substitution of native C into the game and no cycle interleaving
parity is established.

Original Mesen 128-call capture is a different execution phase:
**zero** exact full entry registers/flags plus 2KiB RAM matches across
all same-caller original/native pair combinations. Even where
entry CPU clock numbers coincide, internal RAM differs by 17–33
bytes (see evidence JSON). Native single-frame bus trace contains
the original ROM IRQ vector $FFFE->$E0F5 at CPU cycle 28,654,127
between collision calls; Mesen interrupt outlier sample 101
occurs inside $96ED at cycles 28,683,874..28,684,272.
Neither coincident CPU counts nor "same controller input schedule"
is proof of matched reset/PPU phase or IRQ timing.

Recorded live original-ROM instruction-address coverage
inside the $FCDE bounded interpreter: 60 distinct instructions.
No new live collision caller site beyond the previously observed
three. Coverage inventory remains 1,150/1,264 implemented;
114 identified open; **0 original bytes retired**.
Use tools/instrumentation/verify_fcde_native_zip_v11.py to
verify original ROM/DMA traces with arbitrary native row count:
the older paired v11 comparator was deliberately restricted to
128 owner Mesen calls and must NOT truncate a 212-row native run.


## v11.4 — 627 original-CPU native calls; shift-1 ROM effect alignment

Owner provided all three isolated headless native replay runs,
recording 212/209/206 original $FCDE CPU calls (627 total).
The authentic ROM interpreter verifies 627/627 bounded
register/flag and instruction-clock predictions, 625/625
uninterrupted full RAM and CPU+DMA elapsed-clock totals,
and two separate IRQ-overlap cases with the ROM vector
$FFFE->$E0F5 and eight stack writes.

Shift 1 is the first true same-ordinal cross-emulator
execution alignment. All first 128 Mesen and native caller
sites and CPU entry/return registers/flags match, as do
their authentic ROM $FCDE bounded sequential RAM write
effects and original 6502 instruction-cycle totals. Only
nine historical RAM addresses differ in the complete
capture; none is a read-before-write input on those
observed ROM paths. Full 2KiB byte identity and
machine-wide behavior are still NOT proved.

DMC phase diverges: 98 equal elapsed-time samples,
16 Mesen-only +4, 14 native-only +4. Sample 101 has
the identical CPU entry clock and ROM IRQ return site,
but Mesen observes one DMC sample advance and a +4
elapsed-time difference. No per-cycle Mesen DMC DMA
bus capture was provided; this is a *bounded semantic
match* with unresolved cross-engine bus-phase parity,
not hardware-perfect parity.

Translation ledger unchanged: 1,150/1,264 implemented;
114 open, 60 classified data, 0 executable bytes retired.
Public aggregate: evidence/fcde_v11_4_three_shift_owner_sync_rom_audit.json.


## Collision v12 DMC bus-event source instrumentation prepared (2026-11-05)

The native original-CPU collision observer now emits an optional
per-cycle DMA sidecar, recording actual CycTraceCycle halt/read/write
events and device-owned DMC address/byte transitions inside verified
$FCDE calls. A private source patcher targets the exact upstream
MesenCE commit 4b8a196e299b3de297b204fd1208d2981562c89b,
using the real NES debugger DmaRead and IsDmcDma branch to
identify DMC sample reads. This is necessary because the Lua Read
callback combines ordinary CPU and DMA reads. The MesenCE patch
is opt-in in a separate private source checkout, not a modification
to the installed emulator.

The shift-1 replay Lua now accepts a private deterministic input
schedule, and Windows scripts prepare/build/run both the private
MesenCE capture and the isolated NESRecomp original-CPU capture.
A strict v12 source-identified bus comparator and ROM-free synthetic
tests are committed; pinned MesenCE source-patch CI passed.

**These are prepared, source-checked tools, not new gameplay evidence.**
No new MesenCE debugger-DMC CSV or per-cycle native DMA CSV was
captured or compared in this environment. Source-level collision
translation: 1,150/1,264 executable bytes implemented, 114 open.
Original CPU is still active, retired original 6502 bytes = 0.
No whole-machine DMC/IRQ/PPU/APU/VRC6 bus parity acceptance.
See docs/rom-translation/COLLISION_V12_DMC_BUS_GATE.md.
