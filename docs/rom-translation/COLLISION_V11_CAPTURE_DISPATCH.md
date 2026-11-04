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

Initial slice: verified bank 14 `JSR $FCDE` at `$8345`, return at `$8348`.
The two hook-site bytes are recorded in
`tools/instrumentation/collision_v11_hook_sites.toml`.
Only the fixed bank and original physical bank 14 are accepted. Other
`$FCDE` callers are counted but NOT claimed as measured in this slice.

## Opt-in build (never the default)

1. Start from a **private copy** of the working `game.toml`; append the
   two `[[mod_function_hook]]` records exactly as in the snippet. Do not
   use the snippet as a complete game configuration.
2. In the existing compatibility CMake build set
   `-DJP_COLLISION_V11_SHADOW=ON`,
   `-DJP_GAME_CONFIG=<private-complete-game.toml>`,
   and the original `-DJP_ORIGINAL_JP_ROM=<private-Japanese-ROM>`.
3. Launch that isolated diagnostic build with
   `--collision-v11-rom <private-Japanese-ROM>`.
   This loads a private verified image and enables the two read-only callbacks.
   Do not enable these callbacks in the shipped default executable.
4. `CollisionDispatchV11_GetStats()` exposes:
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
The capture is bounded to 128 rows. No capture has been executed in this
environment: MesenCE is not installed here.

The v11 CSV retains the strict 15-column v10 schema. Check it with:

```powershell
py -3 tools/instrumentation/verify_fcde_trace_gate_v11.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --trace 'PATH_TO_MESEN_SCRIPT_DATA\cv3dcjp-fcde-v11-TIMESTAMP.csv' `
  --report 'data/generated/collision-v11-mesen-verified.json'
```

The report is evidence of *reported* emulator register/cycle totals only.
CSV file SHA-256 cannot prove the capture was made by MesenCE. We require
paired input/movie, emulator build identity, and native side trace before
accepting parity. No per-cycle PPU/APU/DMA/IRQ/NMI/VRC6 equivalence has been
established. **Zero original PRG bytes are retired.**

## Open gates

- Run the owner-side MesenCE script and verify its produced CSV.
- Run the isolated NESRecomp opt-in build and capture the stats.
- Compare the same deterministic input sequence and ROM in both machines.
- Add real per-cycle bus transaction observation and CPU/PPU/APU/DMA/VRC6
  interrupts comparison before authorizing *any* replacement.
- Expand beyond the one verified `$8345` caller only when live evidence exists.
