# Stage 0 synchronized CPU/PPU evidence — v3 foundation

**Purpose:** Resolve the central weakness of v2: CPU selector-writer events and PPU frame-end snapshots were from separate executions. v3 records both streams from **one new isolated original-ROM process**, with one run identifier, one verified ROM, a copied executable, CPU cycle brackets, file hashes and per-frame palette integrity checks.

**Not yet proven:** executable CPU hook participation until the runtime writes events; per-scanline palette commits, PRG palette-source addresses, physical adjacency, eight-room global geometry and independent pixel parity.

## Requirements

- Current `C:\NESDev\CV3DCJP-Repo` checkout, with this feature merged into `main`.
- Authenticated Japanese ROM SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`.
- Existing **isolated** original-game build at `C:\NESDev\CV3DCJP-Builds\5h8f-original\Release\jp_akumajou_densetsu_modern.exe`.
- The owner-local `C:\NESDev\nesrecomp\runner\cyc\hw_machine.c` must still contain the historical **5H8U instruction-fetch/write-cycle hook**. The script checks this source requirement. The running executable must actually contain the hook, confirmed by nonempty compatible CSV—not just by source inspection.
- No change is made to the ROM, shared engine source, original game installation, or current isolated build.

## Windows command

```powershell
Set-Location C:\NESDev\CV3DCJP-Repo
git switch main
git pull --ff-only origin main

$capture = '.\tools\windows\Capture-CV3DCJP-SynchronizedV3.ps1'
& $capture -ValidateOnly
if (-not $?) { throw 'Preflight failed; do not proceed' }

& $capture
if (-not $?) { throw 'Capture failed; preserve evidence and console output' }
```

The launcher creates a **new unique** directory under ignored `data/generated/stage0-sync-v3/<run-guid>/`, copies `Release` to `executable/`, launches *that copy* with `--edition original --room-view 0 --cviii-diag-mode frames`, sets `CVIII_SELECTOR_TRACE_5H8U` for the same process, and restores environment settings upon exit. It never calls or modifies the old destructive build launcher.

**During the game:** play through the complete Stage 0 room sequence, preferably including every actual transition. Exit normally. It writes:

```text
data/generated/stage0-sync-v3/<run-guid>/
├── executable/               # Private disposable executable copy
├── selector-cycle.csv        # CPU RAM $34-$36 writes with cycle/fetch epoch
├── frames.csv                # Frame IDs, CPU cycles, selectors and PPU digests
├── palette.bin               # Private 32-byte frame-end palette snapshots
├── cviii-evidence.events.csv # Original frame event sequence (additional)
├── cviii-evidence.oam.bin    # Original-game sprites (additional, private)
├── cviii-evidence.lines.bin  # Scanline scroll/PPU summary (additional)
├── session.json              # Same-process execution receipt, input/output SHA256
└── audit.json                # Source-safe analysis with unresolved fields
```

`audit.json` checks: authenticated ROM and copied executable hash, session-run ID, each input file's SHA-256, continuous frame IDs and monotonic CPU cycles, 40-byte palette records and per-frame FNV digest, sequenced original `5H8U` CPU writes with fetch epochs, target RAM addresses, register consistency, **same frame/cycle boundaries**, and resulting selector state at the next recorded frame. It rejects mismatches rather than guessing.

Only the **frame-end palette states** can be verified through this capture. A changed palette is reported with its frame and hash; the original ROM table source is `null` because **a write-level PPU palette event and its producing CPU instruction have not been captured**. The 5H8U hook traces selector RAM writes only, not writes to $2006/$2007 and the actual PPU palette bus. Thus v3 must not yet claim palette-source or per-scanline parity.

A single-session trace still **cannot** prove room displacement from selector order. The report leaves direction, global displacement and new world positions unresolved.

## Next proof stage (separate instrumentation)

Add a *development-only, opt-in* PPU event probe in a separately checked-out engine, sourcing the exact $2006/$2007 CPU instruction/fetch epoch and mapper bank, **actual committed palette RAM writes**, scanline/dot/cycle timing, and CHR bank switching. Verify callback timing with original hardware/independent emulator frames. Capture with 5H8U selector events in that **same run** and use CPU/PPU evidence to justify each global displacement. Do not claim these features already exist.

## Privacy

**Do not upload or commit the whole run directory, copied executable, ROM, OAM frames or palette bytes to GitHub.** The private capture can be shared in this conversation for analysis. GitHub holds only the exporter, validator, synthetic tests and source-safe findings. A successful offline test cannot replace Windows execution of this instrumentation.

See [Stage 0 evidence v2](STAGE0_EVIDENCE_V2.md) and [Issue #3](https://github.com/Hyperbog/CV3DCJP/issues/3).
