# Collision v11 Phase 1 — native replay synchronization gate

## Source-grounded controller evidence

The owner MesenCE v11.2 ZIP has 128 collision pairs, 128 DMC boundary
records and 9,749 controller input-poll records. Reset epoch 1 supplies
9,226 consecutive input-poll records (Mesen frames 2..9227), containing
23 controller-state runs including the initial released state.
Controller 2 stays inactive.

The first collision entry was at CPU clock 26,000,844; the final return
at 28,950,229. The nearest preceding Mesen controller polls occurred
at frames 874 and 973, respectively. This is temporal localization,
NOT proven equivalence of NESRecomp and Mesen frame numbering or PPU phase.

Original Japanese ROM SHA-256:
958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28.

Pinned NESRecomp upstream:
mstan/nesrecomp 2b4fcdb98c9e702be3fb365cfc645003c50b5fc1.
Relevant verified upstream functions: runner/cyc/cyc_host.c,
parse_buttons, load_input, input_tick and the zero-based frame loop.

IMPORTANT: Mesen controller data encodes bit0=A, bit1=B, bit2=Select,
bit3=Start, bit4=Up, bit5=Down, bit6=Left, bit7=Right.
NESRecomp host input uses NAMED buttons, and maps A to its internal
0x80 bit, B to 0x40, and Right to 0x01. Never copy the raw Mesen
button mask into the native engine.

## Deliverables

- tools/instrumentation/collision_v11_make_native_input.py:
  authentic original ROM SHA-256 check; isolates contiguous reset
  epoch, rejects missing polls or invalid cycles, converts button
  semantics, writes three UNVERIFIED frame-offset input schedules
  without overwriting existing evidence.
- tests/test_collision_v11_input_replay.py:
  seven synthetic fail-closed contracts, not hardware parity evidence.
- tools/windows/Invoke-CV3DCJP-CollisionV11Native.ps1:
  guarded isolated owner-side CMake build, exact source/engine ROM
  verification and owner-private complete game.toml copy. Installs
  the five ROM-checked read-only hook declarations in the COPY only.
  Builds with JP_COLLISION_V11_SHADOW=ON, JP_CYCLE_HEADLESS=ON, and
  JP_MODERN_ENABLE_UI=OFF. Runs native CTest and actual original guest
  CPU with --input, --hash-out, --collision-v11-trace and
  --collision-v11-dma-trace. The original protected 5h8f build is
  not modified. Writes a run receipt and frame-specific cycle trace.
- tools/instrumentation/verify_fcde_paired_dma_v11.py:
  ROM-gated entry/return, CPU/RAM and real DMA comparison.
  ROM-authenticated IRQ overlap is classified separately from a
  pure CPU instruction plus DMC clock formula; IRQ bus-cycle timing
  remains an open acceptance condition.

The three candidate input mappings are Mesen frame 2 to native
frame 0, 1 or 2. The correct offset is NOT YET VERIFIED.
The initial run should use shift 0; follow-up experiments can test
shift 1 or 2, preserving previous results instead of overwriting.
First estimated collision span is Mesen frames 874..973.

## Owner-side first native run

From the repository checkout, after pulling the current main branch,
with your original ROM, exact pinned NESRecomp engine and existing
COMPLETE PRIVATE game.toml:

    & .\tools\windows\Invoke-CV3DCJP-CollisionV11Native.ps1 -Action BuildAndRun -GameConfig "<path to complete private game.toml>" -InputDir "<folder containing the generated input schedule and manifest>" -Shift 0 -Frames 1000 -Align 0

To create input candidates from your synchronized owner CSVs:

    py -3 tools/instrumentation/collision_v11_make_native_input.py --rom "<private Japanese ROM>" --controller "<owner controller CSV>" --collision "<owner synchronized collision CSV>" --outdir "<owner-private output directory>"

The separately delivered diagnostic ZIP already includes candidate
schedules, manifest, launcher and instructions; it includes no original
ROM and no full RAM snapshots.

The Windows runner tests the frame estimated to contain IRQ sample
#101: Mesen frame 964 maps to native 962+Shift ONLY as a probe.
The original ROM IRQ vector at $FFFE is $E0F5, with the saved
interrupt return PC $FCEB. The recorded DMC+IRQ case has 246
extra elapsed CPU clocks. That interval must not be normalized to
a four-cycle DMA without the actual interrupt/bus trace.

This environment has no owner-complete private TOML and no runnable
Windows diagnostic game executable. No native gameplay execution
has occurred here. The observable original game path is not replaced
and 0 executable PRG bytes have been retired.

## Acceptance gates

1. Windows owner-native run must produce nonempty CPU and DMA sidecars.
2. Input/reset/PPU alignment must be established with real entry
   A/X/Y/SP/P and 2-KiB RAM checks, not schedule proximity alone.
3. Ordinary +4 Mesen DMC-correlated cases must match actual native DMA
   cycle count, and return RAM/status, without masking any unexpected
   bus activity.
4. IRQ-overlap timing, NMI/IRQ source, handler effects and per-cycle
   execution require distinct verification. No native substitution
   until full hardware behavior is proven.
5. The collision translation ledger retains 1,150 of 1,264 identified
   executable bytes implemented, 114 open, 0 retired.


## Pinned-engine 5H8S/5H8U linker compatibility (2026-11-05)

Owner Windows collision diagnostic compile reached LNK2019 for two
historical selector-write globals: cyc_cviii_selector_write_5h8s and
cyc_cviii_selector_write_5h8u. These two callbacks are declared by
cviii_selector_native_5h8s.h and cviii_selector_native_5h8u.h, but
they were introduced in a separately instrumented cycle-engine
checkout, not the clean pinned upstream mstan/nesrecomp revision
2b4fcdb98c9e702be3fb365cfc645003c50b5fc1.

Correct scope: in CMake when BOTH JP_CYCLE_HEADLESS and
JP_COLLISION_V11_SHADOW are ON, omit the two legacy selector
instrumentation modules and instead link
cviii_selector_unavailable_collision_v11.c, which defines only the
HOST selector power-on entry points and does not define engine hook
pointers. Any attempt to enable the old selector instrumentation by
setting CVIII_SELECTOR_TRACE_5H8S or CVIII_SELECTOR_TRACE_5H8U
fails closed. The original game target and normal selector
diagnostics stay unchanged.

This compatibility fix does NOT create selector-write observations,
and no CPU, mapper or PPU behavior is fabricated. Its ROM-free
compiler/linker smoke test is part of GitHub Actions. A full private
Windows ROM-backed link remains an owner-side acceptance test.

After pulling main, rerun the SAME diagnostic command with the
separate pinned engine worktree. CMake regenerates the existing
marked collision-v11-paired build tree and drops the obsolete
selector objects from the target link. Do not reset or patch either
engine worktree. The protected original game build is untouched.


## v11.3 first actual native owner-run acceptance outcome

The owner Windows diagnostic successfully executed the original
Japanese ROM under pinned NESRecomp for 1,000 frames. Its 212
collision CPU rows and 212 DMA rows pass original-ROM callsite,
stack, mapped bank, independent ROM opcode return-state/RAM and
instruction-cycle/DMA accounting for **212/212** observed calls.
The DMA sidecar reports 16 four-cycle and three three-cycle
DMC fetch stalls; zero unexplained native cycle residuals.

However, the native shift=0, align=0 run does **NOT** pass the
cross-emulator same-session gate: no full entry CPU+2KiB RAM
state matches any of the 128 Mesen entries, even though 66
Mesen entries have a same-caller native entry clock within 100.
Native IRQ $E0F5 at CPU clock 28,654,127 in traced frame 962
is outside captured collision calls. Mesen IRQ-overlap sample
101 spans 28,683,874..28,684,272 *inside* caller $96ED.
Thus original CPU and native-side timing accounting are verified
locally, but **same-input PPU/IRQ alignment and hardware
bus-cycle parity remain unproven**.

To reproduce the ROM-backed audit on a private machine:

    py -3 tools/instrumentation/verify_fcde_native_zip_v11.py --rom "<private-JP-ROM>" --zip "<owner-20261105-native-ZIP>" --mesen "<owner-v11.2-Mesen-CPU-CSV>" --report "<private-native-audit.json>"

Next test two distinct candidate input phase shifts in the
already built, protected headless diagnostic directory;
do not select an offset without state equality:

    & .\tools\windows\Invoke-CV3DCJP-CollisionV11Native.ps1 -Action Run -Engine "C:\NESDev\nesrecomp-collision-v11-pin" -GameConfig "$env:USERPROFILE\Downloads\game-cv3dcjp-original.toml" -InputDir "$env:USERPROFILE\Downloads\CV3DCJP-Collision-v11-Replay" -Shift 1 -Frames 1000 -Align 0
    & .\tools\windows\Invoke-CV3DCJP-CollisionV11Native.ps1 -Action Run -Engine "C:\NESDev\nesrecomp-collision-v11-pin" -GameConfig "$env:USERPROFILE\Downloads\game-cv3dcjp-original.toml" -InputDir "$env:USERPROFILE\Downloads\CV3DCJP-Collision-v11-Replay" -Shift 2 -Frames 1000 -Align 0

Keep both new private capture directories. No ROM, raw input
history or RAM snapshots committed. Aggregate receipt:
docs/rom-translation/evidence/fcde_v11_3_native_212_call_rom_dma_owner_capture.json.
Original CPU still executes, 0 PRG bytes retired.


## v11.4 owner replay: three offset candidates audited against exact JP ROM

The owner uploaded the entire protected, isolated diagnostic build
and three separately recorded 1,000-frame runs. ZIP SHA-256:
851c6d9d662e7f45fa62093527f9e3ff0dc08de3813d056c61bd6b24b46df694.
The three receipts agree with the uploaded binary SHA-256:
dcd4619c3f034379ec7435dd6d7943f547c3a7b0eeb0ac4ae929fe6fc2658e91.
Source ROM SHA-256, pinned engine SHA, ZIP CRC, caller JSR/stack,
mapped bank, 15-column CPU and DMA sidecar boundaries were checked.

Native observations: shift 0 = 212 calls, shift 1 = 209,
shift 2 = 206; all three executed the original ROM CPU path
with read-only C shadow, yielding **627 total observations**.
Original-ROM bounded opcode replay matched A/X/Y/SP/P and C-predicted
instruction clocks on **627/627**. Uninterrupted original ROM
instruction execution and full 2KiB RAM plus native DMA timing
matched **625/625**. The other two (shift 1 and shift 2 sample
101) each contain a separately ROM-vector-verified IRQ inside $FCDE,
adding 242 observed clocks and eight stack writes. Do not mistake
those interrupt spans for failed C instruction prediction or for
an ordinary DMC DMA stall.

Best Mesen call-phase alignment is **shift 1**:
all 128 Mesen caller sites match native callers in sample order;
all 128 entry/return CPU registers and meaningful status flags
match; median absolute CPU entry clock difference = 2.
32 calls enter at exactly the same CPU clock. Shift 0 has 66
same-caller calls within 100 clocks (median nearest delta 97);
shift 2 has 63 (median 142). Shift 1 has all 128 within
100 clocks. The other shifts are not suitable parity baselines.

No shift achieves exact full RAM snapshot equality. For the
128 shift-1 pairs, differences are restricted to nine historical
addresses: $0004/$0005/$0020, $01ED-$01EF, and $01FC-$01FE.
Original Japanese $FCDE instruction-path replay demonstrates
that none of these *specific differing bytes* can influence
these bounded executions via a read before first write. In
particular, the $01EF stack byte is overwritten on the observed
path before the original CPU can read it. The 128 bounded
original-ROM ordered writes, output CPU register sets and
instruction-cycle totals match exactly. This is **not**
permission to ignore these RAM locations globally, and it
is not whole-machine per-bus-cycle parity.

Of the 128 shift-1 durations, **98 match exactly** (neither
emulator observes DMC progress inside the collision window),
**16 have Mesen +4** with a Mesen DMC sample-reader advance
but no native DMC DMA stall, and **14 have NESRecomp +4**
with a native DMC DMA stall but no Mesen sample-reader advance.
These 30 mismatches are a DMA **phase difference**, not
a collision instruction-cycle discrepancy. The boundary
Mesen data does NOT include per-bus-cycle DMC timestamps,
so full DMA equivalence is unproved.

The key synchronized interrupt example is sample 101:
both machines enter $FCDE at CPU clock 28,683,874, caller
$96ED. NESRecomp's real instruction/bus trace fetches the
ROM $FFFE/$FFFF IRQ vector at 28,683,903/904 and enters
the original $E0F5 handler at 28,683,905, saving the
original return PC $FCEB. Native returns at 28,684,268:
152 uninterrupted collision instruction clocks plus a
separately observed 242-clock interrupt span, no native DMC
fetch. Mesen returns at 28,684,272: the same interrupt
signature plus a DMC sample-reader advance and four
more elapsed clocks. The IRQ source and full DMC bus-cycle
timing are NOT established by these captures.

The complete 824-entry owner ZIP includes private build
outputs and therefore stays outside Git. Only the JSON
aggregate is committed:
evidence/fcde_v11_4_three_shift_owner_sync_rom_audit.json.
No original CPU code was substituted or retired. The next
acceptance step is a genuine synchronized DMC DMA bus event
trace (and interrupt-source attribution), not more shifts of
the same controller schedule.
