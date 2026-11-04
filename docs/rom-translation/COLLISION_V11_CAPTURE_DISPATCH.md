# Collision v11 — MesenCE capture and NESRecomp read-only dispatch observation

## Authenticated ROM and scope

Original JP SHA-256: `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`.
The optional NESRecomp module checks the whole owner-local ROM using a pinned
64-bit FNV-1a fingerprint `76615b120049ad3f` (NOT a cryptographic signature).
CMake separately enforces the full SHA-256 on its supplied original ROM.

Upstream: mstan/nesrecomp commit `2b4fcdb98c9e702be3fb365cfc645003c50b5fc1`.
Real supported callback path: `runner/cyc/cyc_hooks.c`,
`runner/cyc/cyc_run.c`, `runner/include/mod_function_hooks.h`.
It operates before instruction fetch; returning **zero** leaves the original
instructions, all CPU bus accesses, and the PPU/APU/DMA/VRC6 clocks intact.

The first read-only hook watches `$FCDE` and four independently ROM-checked
caller-return paths: `$8345 -> $8348`, `$8429 -> $842C`,
`$96E4 -> $96E7`, and `$96ED -> $96F0`. The three additional callers
were actually observed in the owner's 128-row v11 capture.
The five instruction-hook keys are recorded in
`tools/instrumentation/collision_v11_hook_sites.toml`.
The fixed bank and original physical bank 14 are required. All other
`$FCDE` calls still execute unchanged without shadow comparison.

## Opt-in build (never the default)

1. Start from a **private copy** of the working `game.toml`; append the
   five `[[mod_function_hook]]` records exactly as in the snippet. Do not
   use the snippet as a complete game configuration.
2. In the existing compatibility CMake build set
   `-DJP_COLLISION_V11_SHADOW=ON`,
   `-DJP_GAME_CONFIG=<private-complete-game.toml>`,
   and the original `-DJP_ORIGINAL_JP_ROM=<private-Japanese-ROM>`.
3. Launch that isolated diagnostic build with
   `--collision-v11-rom <private-Japanese-ROM>` and
   `--collision-v11-trace <new-private-native-CSV-path>`.
   This loads a private verified image and enables the five read-only callbacks.
   Do not enable these callbacks in the shipped default executable.
4. The native CSV uses the same 15-column register/RAM/cycle format as MesenCE.
   The file is created only when the optional trace path is supplied; each
   completed entry/return is flushed immediately. No file is added to Git.
5. `CollisionDispatchV11_GetStats()` exposes:
   entry hits, shadow-ready, rejected probes, matched returns, mismatches,
   and abandoned nested entries. A match is an **instruction-boundary**
   comparison of A/X/Y/P/SP, 2-KiB RAM, PC and total guest CPU cycles.
   There is no bus-event comparison and no runtime CPU substitution.

The original ROM is never checked into this repository; diagnostics have no
CPU, PPU, mapper, or RAM write call. Hook registration must happen before
`cyc_hooks_validate()`, as option discovery does when the explicit option
is supplied. A build without the option keeps all callbacks disabled.

## Original MesenCE / Mesen2 GUI capture

MesenCE from nesdev-org/MesenCE supports `emu.getCpuCycleCount()`.
Open the **original** Japanese ROM in MesenCE. In Lua settings enable
**Allow access to I/O and OS functions**. Run
`tools/instrumentation/mesen2_collision_fcde_gate_v11.lua` from its Lua
console while playing the game or replaying your own input movie. End the script
to close the CSV, then inspect its logged absolute path in the script-data folder.
The capture is bounded to 128 rows. The owner uploaded one complete 128-row
CSV, independently replayed against the original ROM. MesenCE itself was
not executed in this environment.

The v11 CSV retains the strict 15-column v10 schema. Check it with:

```powershell
py -3 tools/instrumentation/verify_fcde_trace_gate_v11.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --trace 'PATH_TO_MESEN_SCRIPT_DATA\cv3dcjp-fcde-v11-TIMESTAMP.csv' `
  --report 'data/generated/collision-v11-mesen-verified.json'
```

The report is evidence of *reported* emulator register/cycle totals only.
CSV file SHA-256 cannot prove the capture was made by MesenCE. The native
CSV is a separate original-emulator execution observation; two files from
different runs are NOT a paired comparison without identical inputs and
replay state. We require
paired input/movie, emulator build identity, and native side trace before
accepting parity. No per-cycle PPU/APU/DMA/IRQ/NMI/VRC6 equivalence has been
established. **Zero original PRG bytes are retired.**

## Open gates

- The owner-side MesenCE capture is available: 128/128 structurally valid;
  110/128 match original-ROM instruction CPU-clock totals. All 18 timing
  residuals are +4 cycles, while all 128 register/flag/stack/RAM results match.
  DMA is a plausible hypothesis, NOT an observed bus-event fact.
- Run the isolated NESRecomp opt-in build and capture the stats.
- Compare the same deterministic input sequence and ROM in both machines.
- Add real per-cycle bus transaction observation and CPU/PPU/APU/DMA/VRC6
  interrupts comparison before authorizing *any* replacement.
- Expand native coverage only where the owner trace and original ROM establish
  the caller/return sites; the three observed sites are now instrumented.

### Replaying the supplied 128-row capture

The owner supplied `cv3dcjp-fcde-v11-20261103-211957.csv`, SHA-256
`ca19f8453f2b0909f51704e75b809eaeacf0243109a7a4e1efff3b5d1ebbaf19`.
The original ROM interpreter matched every recorded register, status-flag,
stack-pointer and 2-KiB RAM return; 110/128 clock totals agreed exactly;
the remaining 18 were **four more observed cycles** than 6502 instruction
cycles, spread across all three callers. The CSV did not log DMC/DMA events.
The timing discrepancy must not be silently normalized away. The original
runtime has not been replaced. The native C module has not been compared against
this particular CSV in the current execution environment.

Owner-side ROM opcode replay:

```powershell
py -3 tools/instrumentation/verify_fcde_rom_capture_v11.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --trace 'PATH_TO_CAPTURE\cv3dcjp-fcde-v11-20261103-211957.csv' `
  --report 'data/generated/collision-v11-rom-replay.json'
```

Privacy: the complete owner CSV and ROM are not committed. Public evidence is
aggregate only: `evidence/fcde_v11_mesen_128_capture_rom_replay.json`.


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
tracer. The pinned host applies the game's command-line options before it
turns tracing on. Consequently the sidecar registers during option processing
and checks cyc_trace_enabled at each $FCDE execution boundary. Without real
tracing it refuses to capture that interval, counts the rejected probe, and
prints a warning once. It never reports fake zero DMA stalls.
An existing trace callback is chained, not overwritten.

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


## v11.2 MesenCE synchronized capture (new recording, not historical data)

The previous 128-row owner capture cannot retroactively supply input history
or DMC reader state. The new optional, read-only Lua script is:

    tools/instrumentation/mesen2_collision_fcde_sync_v11.lua

Its MesenCE APIs and field names were checked against:
- Core/Debugger/LuaApi.cpp: getInput, getState, inputPolled
- Core/NES/Input/NesController.h: standard NES A/B/Select/Start/Up/Down/Left/Right
- Core/NES/APU/DeltaModulationChannel.cpp: currentAddr, bytesRemaining,
  bufferEmpty (and bundled NesDmcCapture.lua)
- the original Japanese ROM's pinned SHA-1, checked before file creation

Run it in MesenCE with the original ROM and Lua file access enabled.
**Reset the emulated console after the Lua script is active**, then replay
the intended input sequence. Stop the script to flush three files in the
script-data directory with one shared timestamp:

- cv3dcjp-fcde-sync-v11-TIMESTAMP.csv — same 15-column collision format;
- cv3dcjp-fcde-dmc-v11-TIMESTAMP.csv — aligned original Mesen DMC
  current-reader address/bytes-remaining/buffer-empty boundary states;
- cv3dcjp-controller-v11-TIMESTAMP.csv — one row per inputPoll event,
  recorded frame number, CPU clock, both controller masks and reset epoch.

Controller bit positions are source-verified: A=bit0, B=bit1,
Select=bit2, Start=bit3, Up=bit4, Down=bit5, Left=bit6, Right=bit7.
The raw input log is **not automatically equivalent** to NESRecomp's
--input FILE frame schedule. Confirm first-frame alignment and reset/PPU
phase before converting or claiming matched replay. The earlier capture's
18 four-cycle deltas remain unexplained until corroborating DMA data
are actually collected. Lua source correctness is static/source-verified;
no new Lua session has been executed here.

Retain the three original user-side CSV files outside public Git.


## v11.2 first synchronized owner ZIP: analyzed and archived as evidence

Strict ROM/JSR/stack preflight: PASS. Collision + DMC alignment:
128/128. CPU registers, stack pointer, flags: 128/128 exact.
Whole RAM: 127/128 exact. ROM instruction clocks: 112/128 exact.
The 15 +4 clock cases each coincide with DMC sample reader +1 /
remaining bytes -1. The 246-clock anomaly (sample 101) has the same
DMC change and an original-ROM IRQ handler stack signature, saved
return PC $FCEB and original vector $E0F5. No exact-cycle case has
a DMC reader advance. These Mesen DMC boundary observations
corroborate DMC sample fetching but do not give the individual DMA
bus-cycle timestamps. The 246-clock IRQ span cannot be normalized
or declared per-bus-cycle equivalent without an actual instruction trace.

Controller input-poll records: 9,749; continuous post-reset epoch:
9,226 records (frames 2..9227). The data does not yet establish
identical frame-zero and PPU phase for NESRecomp. The original
128-row Mesen v11 CSV remains a separate unsynchronized capture.

Run the public, fail-closed analyzer against a PRIVATE ZIP:
python3 tools/instrumentation/verify_fcde_sync_dmc_irq_v11.py
  --rom <private-Japanese-ROM> --zip <private-three-CSV-ZIP>
  --report <private-analysis.json> --residuals-csv <private-details.csv>

The ZIP SHA and aggregate results are committed in
docs/rom-translation/evidence/fcde_v11_2_sync_dmc_irq_owner_capture.json.
The original ROM, CSVs and complete 2-KiB RAM states remain untracked.
The next gate remains an actual matched-input NESRecomp native CSV and
hardware DMA-cycle sidecar, plus instruction/bus traces around the IRQ.
No native substitution, no retirement of original PRG code.
