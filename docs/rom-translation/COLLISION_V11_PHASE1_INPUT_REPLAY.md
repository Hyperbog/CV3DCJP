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
