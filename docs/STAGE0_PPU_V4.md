# Stage 0 — source-matched PPU palette and VRC6 CHR observer v4

**Status: instrumentation foundation only.** The original Japanese ROM is authoritative; no ROM palette-producer addresses or global room coordinates are claimed verified.

## Exact NESRecomp source paths

The owner's NESDev.zip contains these verified mutation paths:

| Event | Source function | Meaning |
| --- | --- | --- |
| CPU_PPU_REQUEST | hw_machine.c, hw_bus_write | CPU $2000–$3FFF register request with CPU fetch/PRG context, not a completed VRAM write |
| PALETTE_COMMIT | hw_ppu.c, vram_store | Actual six-bit palette RAM store, correct mirrored palette slot, previous/new value, CPU cycle, PPU scanline/dot |
| PALETTE_CORRUPTION | hw_ppu.c, corrupt_palettes | Separate rendering-induced palette RAM changes |
| CPU_VRC6_REQUEST | hw_machine.c, hw_bus_write | CPU mapper 24 write request, not proof of CHR change |
| VRC6_CHR_MAP | hw_vrc6.inc, vrc6_apply | Changed 1 KiB CHR page mapping after map_chr1, including mapped offset before/after |

The observer records sequence, kind, CPU cycles, PPU scanline/dot, fetch epoch, fetch cycle, fetch PC and header-excluded PRG offset, CPU/PPU address, slot, previous/new value and source-binding classification.

Only synchronous CPU register *requests* get DIRECT_CPU_BUS_REQUEST. The later palette commits, corruption and CHR remaps get LAST_FETCH_CONTEXT_ONLY: the most recently fetched instruction does **not** prove the originating ROM instruction for an asynchronous PPU commit. A PRG ROM file offset is fetch_prg_offset + 16 bytes (iNES header), but only when that fetch context is verified. Power-on palette initializations and state loads are separate paths, not counted as palette commits.

## Immutable source identity

- hw_machine.c SHA-256: e5f2f1a72788f6c027c2215ce8db76cd3a9a631f02d591b15231ff05d2a4a925
- hw_ppu.c SHA-256: 25f3dafc80928b7e5b93cc687d6f9fdeb2b426bf34bafaac6d5698b9b0547471
- hw_vrc6.inc SHA-256: f65e06e94230f8ef500ca9f48430fd24a34d67e991209da8133119cc1c663817

The fail-closed Python patcher checks all three SHA-256 digests and exact source anchors **before creating a new destination**. It refuses an existing destination or one within the original engine checkout. The locally extracted owner source passed the patch and GCC C11 syntax checks for hw_machine.c, hw_ppu.c and hw_mapper.c. Actual Windows MSVC compilation and runtime capture have NOT yet been performed.

## Windows execution

Run in a current main checkout:

    cd C:\NESDev\CV3DCJP-Repo
    git switch main
    git pull --ff-only origin main
    $probe = '.\tools\instrumentation\Capture-Stage0-PPUV4.ps1'
    & $probe -Action Validate
    & $probe -Action Prepare
    & $probe -Action Build
    & $probe -Action Capture

Validate: checks authenticated Japanese ROM and three source SHA-256 values without any mutation.

Prepare: creates a completely new external engine copy at C:\NESDev\nesrecomp-ppu-v4. Only the *copy* is patched. Never force source changes on hash mismatch.

Build: invokes the existing isolated original-ROM MSVC/CTest script, with the copied engine and separate C:\NESDev\CV3DCJP-Builds\stage0-ppu-v4 output.

Capture: runs a disposable copy of that Release directory; enables CVIII_PPU_PROBE_V4, CVIII_SELECTOR_TRACE_5H8U and the original frame-end PPU recorder in one process. Evidence is created in a unique folder beneath ignored data/generated/stage0-ppu-v4. The files include ppu-v4-events.csv, selector-cycle.csv, the frame/line/palette sidecars and a SHA-256 receipt. Nothing overwrites the working game or the confirmed original-ROM baseline.

**Never upload these files to public GitHub:** ROM, raw CHR data, original palettes, private recordings, executable or private generated assets. The private evidence may be provided in this conversation.

## Next proof stage

Run the instrumented Windows build and record a full Stage 0 traversal. Independently reconcile event order and palette/CHR state, attribute *causally proven* CPU requests/ROM fetches to actual PPU changes, then compare with an independent original emulator. Until this succeeds, palette source PRG offsets, per-scanline rendering parity and six unresolved global room origins remain unverified. This is a foundation for Issues #16 and #3, not their completion.
