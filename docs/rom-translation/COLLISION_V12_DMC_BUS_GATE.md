# Collision v12 — authentic DMC DMA bus-event acceptance gate

## Evidence boundary

Original Japanese ROM SHA-256:
958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28.
Original guest code always executes. This diagnostic does NOT enable any
native C substitution; original PRG executable bytes retired = 0.

The previously captured shift-1 128-call comparison matches original
$FCDE register/flag and ROM-ordered write effects, but has 30
differences in elapsed clock duration (16 Mesen-only DMC +4, 14
NESRecomp-only DMC +4). That does not establish per-cycle equivalence.

### What is actually observable

Pinned NESRecomp commit 2b4fcdb98c9e702be3fb365cfc645003c50b5fc1
exports genuine CycTraceCycle records including CPU-cycle index, DMA,
halt, address, value, direction and instruction end. The new
--collision-v11-bus-trace option logs all original guest DMA cycles
inside each verified $FCDE call, with old/new ApuDmcView source address
and remaining-byte count. A source-verified DMC read requires a
non-halted DMA read at the DMC source address and simultaneous source
address +1 / remaining bytes -1. These are stricter than the prior
boundary-only sample-address observations.

Pinned MesenCE commit 4b8a196e299b3de297b204fd1208d2981562c89b
Core/NES/Debugger/NesDebugger.cpp identifies DMC DMA at an actual
MemoryOperationType::DmaRead using NesCpu::IsDmcDma(). Lua
addMemoryCallback(read) does not expose MemoryOperationType::DmaRead:
ScriptManager.h merges ordinary and DMA reads into the same Read
callback. Consequently, DO NOT label a Lua read as a DMC bus event.
A fail-closed private source patch emits the true debugger-DMC flag,
CPU clock, DMA read/write, address and value into a CSV without
changing the emulated CPU or memory.

Files:
- src/collision/CollisionDispatchV11.c/h
- legacy/rom-backed/JP/recomp/native/nesrecomp_modern/cviii_modern_host.c
- tools/windows/Invoke-CV3DCJP-CollisionV11Native.ps1
- tools/instrumentation/prepare_mesen_dmc_bus_v12.py
- tools/instrumentation/mesen2_collision_fcde_sync_dmc_bus_v12.lua
- tools/windows/Invoke-CV3DCJP-CollisionV12Mesen.ps1
- tools/instrumentation/verify_fcde_dmc_bus_v12.py
- tests/test_collision_dmc_bus_v12.py

The pinned MesenCE source patch is compiled only in a separate private
copy. It does not modify the user's original installed MesenCE.
No original ROMs, private captured RAM, input schedules, or binaries
are placed on GitHub.

## Owner Windows collection

From C:\NESDev\CV3DCJP-Repo:

    git switch main
    git pull --ff-only origin main

Rebuild the existing isolated NESRecomp diagnostic on input shift 1.
Do NOT use -Action Run alone: the C object needs recompilation to add
the actual per-DMA-cycle CSV.

    & .\tools\windows\Invoke-CV3DCJP-CollisionV11Native.ps1 -Action BuildAndRun -Engine "C:\NESDev\nesrecomp-collision-v11-pin" -GameConfig "$env:USERPROFILE\Downloads\game-cv3dcjp-original.toml" -InputDir "$env:USERPROFILE\Downloads\CV3DCJP-Collision-v11-Replay" -Shift 1 -Frames 1000 -Align 0

Keep the newest private capture directory:
C:\NESDev\CV3DCJP-Builds\collision-v11-paired\captures\shift-1-...
It must include native-fcde.csv, native-dma.csv and
native-dma-bus-v12.csv. Preserve the other native logs as well.

Prepare a second, isolated MesenCE source copy, source-patch it, build
and run it:

    & .\tools\windows\Invoke-CV3DCJP-CollisionV12Mesen.ps1 -Action Prepare
    & .\tools\windows\Invoke-CV3DCJP-CollisionV12Mesen.ps1 -Action Build
    & .\tools\windows\Invoke-CV3DCJP-CollisionV12Mesen.ps1 -Action Run

In the PRIVATE MesenCE window only, open the original Japanese ROM,
enable "Allow access to I/O and OS functions" under Lua settings,
start tools/instrumentation/mesen2_collision_fcde_sync_dmc_bus_v12.lua,
then reset the ROM AFTER the script starts. The Lua replays the
same original-recorded shift-1 controller changes on inputPolled; it
does not change original CPU/PPU/APU code. Continue to 128 collision
calls, stop the Lua script to flush/close its CPU/DMC/controller CSVs,
and exit that private emulator. The source patch writes the fourth
CSV, mesen-real-dma-bus-v12.csv, to the same private directory.

The patched MesenCE debugger is explicitly ARMED by the v12 Lua reset
event via the private CV3DCJP_MESEN_ARM_FILE marker. Prior boot or
title-screen DMC events are discarded, preventing an unrelated pre-reset
CPU-clock epoch from contaminating the synchronized log. The owner-side
runner requires both the arm marker and a nonempty debugger-produced DMC
CSV before declaring capture completion. This marker is a host file
only and changes no emulated CPU or memory state.

The MesenCE private directory:
C:\NESDev\CV3DCJP-Builds\collision-v12-mesen\session-...
preserves the Mesen ROM-checked 128 calls, boundary DMC states,
replayed controller inputs and actual CPU-side DMC DMA transactions.
Never commit these full capture files.

Upload the two NEW capture directories as ZIPs for independent
original-ROM analysis.

## Verification gate

Use the exact original-ROM image and private data with:

    py -3 tools/instrumentation/verify_fcde_dmc_bus_v12.py --rom "<original Japanese ROM>" --mesen-cpu "<v12 Mesen CPU.csv>" --mesen-bus "<mesen-real-dma-bus-v12.csv>" --native-cpu "<native-fcde.csv>" --native-bus "<native-dma-bus-v12.csv>" --native-dma "<native-dma.csv>" --report "<private collision-v12-evidence.json>"

The verifier rejects wrong ROM, malformed event schemas, misaligned
caller/ordinal samples, impossible DMC labels, mismatched DMA cycle
counts and exact DMC-read cycle/address discrepancies. It never
silently shifts clock counts or marks historical 2KiB RAM differences
as globally irrelevant. All 128 original-ROM call boundaries and
register/flags are compared.

A PASS for DMC-read event identity is *not* a whole-machine,
per-bus-cycle, IRQ/NMI/PPU/APU/VRC6 mapper parity claim.
Full collision hardware timing is NOT accepted until actual
interrupt source, all CPU/DMA halt/read/write cycles and relevant
hardware interleavings are independently compared. Original code
remains active; original 6502 code bytes retired = 0.


## MesenCE owner-build failure diagnostics (2026-11-06)

The owner reported `Lua.vcxproj -> .../Lua.lib` followed by the wrapper's
generic `MesenCE patched Release x64 build failed: 1`. The Lua line
means the Lua static library was built; it is NOT an error message.
The actual first failed compiler, MSBuild, NuGet or .NET project
diagnostic was not included in that report. The subsequent
`Patched Mesen.exe missing` is expected if the full solution fails
to link/build and must not be treated as a second independent failure.

The pinned MesenCE UI project `UI/UI.csproj` has
`TargetFramework=net10.0`, `RuntimeIdentifier=win-x64`,
`AssemblyName=Mesen`, and outputs to
`bin/win-x64/Release/Mesen.exe`. The native Core is a Visual Studio
v143 C++ project. Neither the installed toolset nor the actual
failing component may be inferred from the last success line.

The Windows diagnostic launcher now retains an isolated MSBuild
diagnostic file at:
`C:\NESDev\CV3DCJP-Builds\collision-v12-mesen\build-logs\MesenCE-v12-Release-x64-*.log`
and prints the first identifiable build errors before aborting.
The original installed MesenCE, original ROM and playable build are
not altered. The owner should pull `main`, run only
`-Action Build`, and provide the first compiler/SDK error lines
from the log. Do not run `-Action Prepare` again or clean/reclone
the pinned private checkout. Hardware parity remains open.
