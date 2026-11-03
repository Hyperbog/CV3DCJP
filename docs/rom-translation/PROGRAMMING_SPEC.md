# CV3DCJP | ROM Translation and System Completion Protocol

**Version:** 1.0 (foundation)  **Date:** 2 November 2026  **Authority:** authenticated Japanese ROM

## 1. Objective and non-negotiable rule

Build the game one verified subsystem at a time **while translating the 6502 machine code belonging to that subsystem into readable, independently executable native code**. After a complete routine or control-flow region passes its acceptance gate, retire that physical ROM code from the untranslated work queue. A bounded opcode interpreter is a useful oracle, **not a translation**. Extraction, disassembly, observation, translation, and acceptance are different milestones. No diagnostics or experimental enhancements may be presented as finished gameplay.

## 2. ROM identity and address accounting

Authoritative cartridge: *Akumajou Densetsu (Japan).nes*, SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`. iNES header: 16 bytes; Mapper 24 (VRC6); PRG: **262,144 bytes**, organized as **16 physical 16-KiB banks**; CHR: **131,072 bytes**, organized as **16 8-KiB units**. The ROM file offset of PRG byte P is `0x10 + P`. CHR is graphics data, not untranslated CPU instructions. Record the physical PRG offset for every translated byte. CPU address `$8000` cannot identify a routine by itself because mapper banking changes its contents.

## 3. The permanent translation ledger

Maintain a version-controlled **byte-range ledger**. Each record must identify the ROM hash, physical PRG offset, exact original bytes, mapped CPU address and bank context, owning subsystem, execution evidence, incoming callers, outgoing branches/calls, RAM and hardware effects, native source, tests, and status. Shared routines have one canonical owner and may be referenced by many systems. Overlapping code/data interpretations must preserve both roles. Never count the same PRG byte twice.

**Statuses:** `IDENTIFIED` = authentic bytes and entry verified; `DECODED` = control-flow and data references described; `IMPLEMENTED` = readable high-level replacement exists; `VERIFIED` = differential and side-effect tests pass; `RETIRED` = integration, dependencies, and coverage accepted. Statuses are not interchangeable. An unvisited branch or unknown indirect target prevents retirement.

## 4. Completion gate for one assembly region

A region may become `RETIRED` only when (a) every reachable entry, branch, jump, call, return, and bank-switch path in the defined scope has been accounted for; (b) arithmetic, flags, stack, zero-page/CPU RAM, PPU/APU/VRC6/mapper I/O, ordering, and relevant cycle/interrupt behavior match; (c) ROM-derived tables are represented without invented contents; (d) independently generated regression vectors and captured original-emulator states agree; (e) callers and shared dependencies are integrated and tested; and (f) the source, tests, evidence, and reviewer sign-off are recorded. Incomplete or unexecuted paths remain open, not silently approximated.

## 5. Progress measurement without false completion

Partition every physical PRG byte into **retired code**, **known code still open**, **classified non-code data/filler**, or **unclassified**. When code and data overlap, require both roles before closing the byte. The final target is **zero open code and zero unclassified PRG bytes**; data must remain correctly represented. Show separate counts for known code retired and ROM bytes still unclassified. Discovering new code can increase the visible untranslated backlog, so never promise a strictly decreasing percentage before code/data classification is complete. Verified gameplay coverage is a separate metric.

## 6. One-system development cycle

1. Lock ROM identity and collect already available traces; identify entry points and callers by **physical PRG offsets**.
2. Construct a ROM-backed control-flow graph; classify code, tables, data, and unresolved indirect edges.
3. Translate a bounded, closed region into named native functions and typed state transitions; do not execute ROM opcodes in the replacement.
4. Differential-test CPU flags, RAM writes, mapped hardware accesses, events, and timing against original execution, including boundary states and negative cases.
5. Accept only the proven region; retire its exact byte spans in the ledger. Reuse verified shared functions; never duplicate ownership.
6. Advance to adjacent dependencies or the next subsystem, preserving the original game as an oracle until integration is independently qualified.

## 7. Work breakdown and integration safeguards

Proceed through **collision/physics**, **player input and actions**, **enemies and objects**, **room/level geometry and transitions**, **camera/PPU/sprites**, **audio/APU/VRC6**, then **system integration**. These are ownership areas, not isolated CPU banks; dependencies may cross systems. Preserve the original launcher, build, and settings. Enhancements such as widescreen are a separate layer and must not substitute for ROM-matched behavior or hide rendering failures.

## 8. Verified starting baseline — collision only

The attached initial ledger seeds **34 executed bank-14 `JSR $FCDE` sites**: 102 verified instruction bytes with **37,494 call/return observations**, plus the one-byte bank-15 `$FCDE` entry anchor (`PHA`). **103 PRG bytes are identified as code; zero are retired.** The other **262,041 PRG bytes remain unclassified in this deliberately conservative ledger**, not necessarily untranslated instructions. A prior bounded 6502 interpreter and immediate caller-decision tests exist; caller-side state mutation, complete function boundaries, and independent native replacements are not yet proven. **Collision is not complete.**

## 9. Release rule

A subsystem is complete only when every original machine-code region required for its stated scope has been retired **and** the subsystem behaves correctly in the native game across the accepted test domain. The entire ROM is complete only after all PRG bytes are classified and all executable paths and referenced data have faithful native equivalents. Each release must disclose open code, unknown bytes, missing behavioral evidence, and integration limitations.


## Amendment A — collision $FCDE translation, 2026-11-02

The original fixed-bank $FCDE collision query has a readable direct native C translation.
The statically reachable code interval is CPU $FC7D–$FD4C (PRG physical
$3FC7D–$3FD4C inclusive): **121 instructions, 208 code bytes**.
All 121 instructions ran in 2,300 seeded input tests (59 distinct paths).
Native code and the independent original-instruction executor agreed on all
CPU registers, preserved flags, full 2KiB RAM and instruction counts; the
wrong-ROM fingerprint rejection was also tested. Conditional branch coverage
was 26/28 edges; two unvisited edges have simple ROM-local infeasibility proofs.

**Ledger status: IMPLEMENTED, not RETIRED.** The replacement has not been
executed within the real game and cycle-accuracy/interrupt timing has not been
qualified. The 34 observed caller-side JSR sites remain IDENTIFIED. No bytes
were retired in this increment. The code inventory now identifies 310 bytes
(208 implemented in this region plus 102 call-site instruction bytes).

Refer to `collision_fcde_native_v2/` for native code, provenance, tests, and
limitations. Retire this region only after source-matched live integration,
CPU/PPU/mapper/timing acceptance, and independent review.

## Amendment B — bounded collision caller continuations, 2026-11-02

The bank-14 CPU return-side branch at **33 of 34 observed JSR $FCDE**
call sites has been translated as an immediate Z-flag decision.
The exception at `$839D` is a ROM-authenticated `CMP #$05` /
`CMP #$07` / `CLC; RTS` sequence, translated separately.

Eighteen caller continuations now have readable native C state transitions,
with explicit stop boundaries at the next collision JSR, an external call/jump,
an RTS, or an out-of-scope ROM block. They cover return sites in the
`$96CF–$9738` and `$9B3C–$9BA0` regions plus `$9212` and `$839D`.
Verified writes include RAM `$0509`, `$04F2`, and `$0565`; the physical
RAM effects are established, but gameplay names and upstream/downstream
object or movement semantics are **not** yet proven.

A separate 6502 instruction executor, interpreting authentic bank-14 ROM
bytes, matched the native translation on registers, flags, **all 2,048 RAM
bytes**, and exit-boundary classifications for **3,240 synthetic cases**.
The 33 native immediate-branch decisions were also compared against all
**37,494 recorded collision return states** (34 sites, of which one takes
the special compare path). This is an offline differential check, not a
substitute for integrating the translated native routines in the game.

**Additional distinct classified ROM code: 80 instructions / 169 bytes.**
Of those, 137 bytes belong to the 18 bounded continuations; 32 bytes are
first-branch-only translations at the other 16 sites. The updated byte
ledger counts each PRG offset exactly once.

**New total:** 479 identified PRG code bytes, 377 with readable native
implementations, 102 JSR instruction bytes still merely identified,
261,665 PRG bytes not yet classified, **zero bytes RETIRED**. The 16
first-branch-only post-call paths, cross-boundary native function calls,
interrupt/cycle timing, real-game substitution, and any complete
player movement effects remain open. The recorded tests cannot establish
those facts, and this release does not claim collision or physics complete.

Source: `src/cviii_collision_callers_native_v3.h`
Proof: `evidence/translation_coverage_v3.json`
Ledger: `translation_ledger.json`

---
## Collision v4 — external dependencies and RAM effects (2026-11-02)

**ROM:** Japanese Mapper 24 source SHA-256 as above. **All code accounting is keyed by unique physical PRG bytes.**

- Additional readable native translation: `$89EB` (31 bytes), `$9B5E` (25 bytes), and all 86 distinct instructions reachable from `$8723` (182 bytes), including a bounded post-`$E648` continuation: **238 new implemented bytes**.
- `$968A`, `$968D`, `$9690`, and `$9693` are **12 newly decoded bytes**, not an independently implemented controller sequence: external `$8ABD`, `$EF91`, `$86FB` remain pending.
- New differential evidence: **5,834 native-vs-ROM-interpreted state comparisons**, full register/flag/2 KiB CPU RAM, 113/113 decoded instruction addresses visited. Reference execution is independent of the handwritten C translation, but this is **offline**, not real game substitution.
- Native-implemented code across collision v2-v4: **615 PRG bytes**. Known collision-related code: **729 bytes**; 102 identified caller JSR bytes, 12 decoded orchestration bytes, 615 implemented; **0 verified for retirement, 0 retired**. **261,415 PRG bytes remain unclassified**, not necessarily executable.
- `$E648` at fixed CPU address is an **external real-code boundary**: do not fabricate its return or memory mutations. The translated `$8723` resumes only with externally supplied post-call CPU state.
- Remaining requirements: independently translate `$E648` and the `$968A`/`$968D` dependency chain, determine the 16 branch-only callers' remaining state changes, then instrument and validate actual integration including CPU stack and cycles.

The original launcher, protected game builds, ROM, and widescreen renderer are not changed. No code is retired in this increment.

## Amendment D — banked `$E648` translation (2026-11-02)

- ROM bank 15 `$E648` selects bank 7 through `$E2CA`, calls **bank 7** `$B437`, then restores the previous mapper state via `$E62D`/`$E2D0`. `$B437` in bank 14 is not the executable routine.
- Direct native C module: `src/cviii_collision_e648_native_v5.h`, integrated offline with the existing `$8723` implementation. It preserves A/X/Y/P, all 2 KiB RAM, CPU stack state, and two mapper writes; it reads authentic bank-7 ROM table bytes.
- New identified and implemented machine-code inventory: **90 instructions / 173 distinct PRG bytes**, plus **30 data bytes** in bank 7's `$B5F6` pointer table.
- Independent ROM-instruction differential tests: **4,000 random + 2,700 targeted + 1,500 integrated synthetic state tests**. All cases matched. All 77 bank-7 instruction addresses were exercised; **23/24 branch directions** were observed, with `$B476` not-taken statically infeasible after `$B474` falls through.
- Cumulative inventory: **902 identified code bytes, 788 implemented, 114 identified/decoded only, 30 classified data bytes, 261,212 unclassified PRG bytes, 0 retired**.
- **Not retired:** live game substitution, real CPU traces for this call, interrupts and precise CPU/PPU cycle timing remain unverified. The Python test validates the whole ROM hash, while the native C verifies only size and local ROM anchors; engine-side authentication is required.

---

# Programming specification — Collision v7 integration amendment

## Source and scope

The sole authority is the Japanese Mapper 24 ROM SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`. This increment changes **memory mapping in the existing translation**; it does **not** add executable ROM bytes or retire previous entries.

## First divergence and correction

The v6 `$968A` composition rejected synthetic case 359 during its `$8723 → $E648 → bank-7:$B437` dependency. An original-ROM instruction trace accesses `$E1D6`, the fixed final 8 KiB bank, through a pointer loaded by `$B437`. The native v5 `cv5_read` wrongly supported only RAM and the `$8000–$BFFF` switchable window. Its correction is a fixed-bank read at file `0x3E010 + CPU_address - 0xE000` (not the start of 16 KiB bank 15).

The corrected implementation is a high-level translation, **not a new 6502 emulator**. The original nested bank-switch sequence remains modeled.

## Acceptance evidence

- 1,400 `$8ABD` cases passed.
- 3,500 `$EF91` cases passed.
- 5,000 `$86FB` cases passed.
- 1,498 `$968A` compositions matched; 2 input states attempted bus reads outside modeled mappings and were rejected without modifying the state.
- CPU `$E1D6` maps to ROM file `$3E1E6`, value `$A9`, verified by a dedicated C helper test.
- The latest branch-closure reporter has **not** completed. It encountered a test harness reporting error (`ValueError: too many values to unpack (expected 2)`); its corrected source is included for a future clean run. It would be improper to mark the missing `$86FB` branches as verified today.

## Translation ledger

No new ROM code ranges added; the prior v6 total remains **1,093 identified**, **979 implemented**, **0 retired**, and **60 classified data bytes**. Retirement remains gated by full branch/control-flow verification, accurate CPU stack and cycle timing under integration, interrupts, real running-game comparisons, and higher-level acceptance.

## Do not conflate tests

Instruction-level synthetic-state equivalence does not imply live gameplay or widescreen acceptance. Existing game, launcher, and enhancements are unchanged.

---

## Collision v8 canonical source and Git policy

All existing bounded native collision translations from v2–v7 are now consolidated in `src/collision/Collision.c` and exported through `Collision.h`. New verified subsystem translations must be added to the matching canonical `.c` module and the permanent JSON/CSV ROM ledger in the **same Git commit or pull request**. Include independent test evidence and exact original machine-code byte ranges. Never report a 6502 byte as `RETIRED` until the live game executes the replacement with cycle/interrupt and memory/hardware equivalence proven.

A **near 1:1 translation** preserves every original instruction's observable effect and flow, not its assembly syntax or an artificial one-C-line-per-opcode ratio. 6502 flag, carry, overflow, stack, timing, bank/mapper, and memory changes must be accounted for; if an effect is not fully covered, keep the code `IMPLEMENTED` rather than `RETIRED`.

Git workflow: source, evidence, tests and ledger live on `collision/canonical-native-translation-v8` (draft PR #19); playable `main` remains unchanged pending review.

## Amendment v9 — `$86FB` branch closure and remaining caller entry continuations

The single canonical `src/collision/Collision.c` now contains bounded native continuations for all **34 captured `JSR $FCDE` caller entries**. This does not imply every transitive downstream call is translated. The additional 16 entry paths stop at authentic unresolved ROM call, table, or code targets and preserve the original registers, flags, stack, and RAM up to that point.

The `$86FB` closure passes 5,007 independent ROM instruction differential cases: **55/55 executed instructions, 31/32 conditional branch directions**, with the final direction statically impossible. The caller increment passes 8,000 distinct comparisons. The ledger adds 86 authentic instruction spans / 171 new bytes; current totals are **1,264 identified code bytes, 1,150 implemented, 114 open, 60 classified data, 0 retired**.

**Still incomplete:** external and table-boundary continuations, full collision/physics code discovery, hardware/timing/interrupt equivalence, and live runtime substitution. `IMPLEMENTED` does not imply retirement.

## Amendment v10 — native collision cycle clock and live integration safety gate

The original `$FCDE` routine now has native CPU-cycle accounting with independently
verified branch/page-cross penalties. 2,200 ROM-driven differential executions
passed including exact CPU clock totals; all 121 instruction addresses were
reached. This is NOT full hardware bus-cycle equivalence.

A `CollisionLive_ProbeFCDE` adapter demonstrates validated JSR/RTS stack
handoff, original ROM/bank/caller identity and transactional native execution.
It cannot alter the host's CPU and it refuses speculative mutation; the
runtime-commit gate is zero by construction. Mesen2 capture and comparison
sources exist but were not executed against live Windows game or Mesen2 during
this increment. CPU instruction-fetch hook, mapper/PPU/APU/DMA/interrupt
interleaving and original-ROM live parity remain open.

**Cumulative executable inventory unchanged:** 1,264 identified PRG code bytes,
1,150 implemented, 114 identified/decoded only, and zero retired. The new
code is adapter/timing verification, not newly translated original instructions.
