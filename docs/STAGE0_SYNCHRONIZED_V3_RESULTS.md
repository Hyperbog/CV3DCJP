# Stage 0 synchronized v3 capture — ROM-instruction verified

**Result:** Real owner Windows capture analyzed with the exact original Japanese ROM. The 5H8U CPU selector-write hook and frame-end 5H8F PPU recorder produced a consistent time-ordered original-ROM execution record. This is still **not** full native Stage 0 spatial or palette-source parity.

## Authenticated sources

- Japanese ROM SHA-256: `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`
- Private owner capture ZIP SHA-256: `e4896489df9fa6bbc6f5a86952fee0e16c32b350962756cc6e73830ea589b5cc`
- Session receipt run ID: `d08abec4afd54a05a5575ccad31644fb`
- Frames CSV SHA-256: `f6c72c352e595009198734ba0cd34dc8bc2eb46615b2578b7e8d5a2e8aef34d4`
- Selector cycles CSV SHA-256: `992874e7384ab24c423b84d2ce410d38237a02cb47dfde3417230227a885c49c`
- Palette snapshot binary SHA-256: `c3bb6e8986ba9770c720b227c34b5cf0d247d5a87c033c054b147784fe940fb2`
- Source-safe machine-readable summary: [synchronized-v3-verified.json](../data/evidence/stage0/synchronized-v3-verified.json)

## Verified runtime facts

| Criterion | Result |
| --- | ---: |
| Continuous original-game frames | 14,847 |
| Frame-end 32-byte palette records with matching FNV and IDs | 14,847 |
| 240-line records with matching FNV and IDs | 14,847 |
| CPU selector RAM $34–$36 write events | 28 |
| CPU writer events matched to actual ROM instruction opcode/operand | 28/28 |
| Distinct executed selector writer instruction sites | 8 |
| Same-run CPU write groups reconciled to subsequent frame selectors | 15 |
| Unexplained selector changes | 0 |
| ROOM events consistent with frame selector changes | 10 including initialization |
| Distinct frame-end PPU palette states | 26 |
| Palette state changes observed between frame boundaries | 29 |
| CHR_CHANGED event rows | 21 |
| New ROM-proven global room coordinates | **0** |
| Palette-producing ROM addresses proven | **0** |

### Exact source mapping

**Critical ROM-offset rule:** `prg_offset` in the CPU trace is an offset from the start of PRG **after the 16-byte iNES header**. To validate an instruction using the original `.nes`, fetch from `rom[prg_offset + 16]`.

| CPU instruction PC | .nes file byte offset | Original instruction | Hooked write events |
| --- | --- | --- | ---: |
| $B494 | 0x1F4A4 | STA $36 | 3 |
| $B682 | 0x37692 | STA $36 | 1 |
| $D3E9 | 0x3D3F9 | DEC $36 | 4 |
| $D3F6 | 0x3D406 | STA $36 | 1 |
| $D409 | 0x3D419 | STA $36 | 1 |
| $E02A | 0x3E03A | STA ($00),Y | 3 |
| $E7BA | 0x3E7CA | STA $00,X | 9 |
| $F97E | 0x3F98E | INC $35 | 6 |

The six `INC $35` writes are three matched 6502 **dummy-old/final-increment** pairs, and four `DEC $36` writes are two matched **dummy-old/final-decrement** pairs. The PC mappings, zero-page effective addresses, register values and resulting selector state all match the captured writes. This gives ROM instruction-site attribution stronger than the v2 static-site enumeration.

### Observed selector run boundaries

- Frame 3: initialize 15/15/240 → 0/0/0
- Frame 2174: 0/0/0 → 0/1/0
- Frame 3216: 0/1/0 → 0/1/1
- Frame 4637: 0/1/1 → 0/1/3
- Frame 5703: 0/1/3 → 0/1/2
- Frame 7440: 0/1/2 → 0/1/1 (**one frame only**)
- Frame 7441: 0/1/1 → 0/2/1
- Frame 8807: 0/2/1 → 0/2/0
- Frame 10360: 0/2/0 → 0/3/0

The one-frame 0/1/1 selector is retained and **not** treated as an independently visited room or an additional geometric adjacency. Selector order alone still cannot establish direction or displacement.

## Important remaining evidence boundaries

1. **Executable receipt:** the ZIP includes the session receipt and an executable SHA-256, but **does not include the copied executable**. The original local v3 audit checked it while present; this independent analysis can verify capture integrity and ROM opcodes but cannot independently rehash the running executable from the ZIP.
2. **Palette source:** the 26 observed states/29 changes are frame-end palette RAM only. Source PRG table/routines, actual palette commit instruction, scanline/dot write timing and mid-frame palette changes remain unknown. No source addresses are fabricated.
3. **Geometry:** all eight Stage 0 selector identities occur, but no new globally verified room origins/directions are derivable from the available fields. The prior two-room relative placement is separate evidence, not proven by this capture.
4. **Independent emulator:** no independent original-emulator pixel trace is included. PPU line digests establish recorded data integrity, **not pixel parity**.

## Repeatable source-only opcode verification

```powershell
py -3.12 tools/extract/verify_stage0_v3_rom_writers.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --capture "$env:USERPROFILE\Downloads\CV3DCJP_Stage0_Synchronized_v3_PRIVATE.zip" `
  --out 'data/generated/stage0-v3-rom-opcode-audit.json'
```

This verifier checks the private ZIP source hashes and selector cycle/state integrity against original ROM opcode bytes, including 6502 RMW two-write semantics. The full uploaded raw PPU sidecar integrity was independently checked during this analysis; the on-repository original v3 validator remains responsible for frame-end PPU digests in a complete owner-private run directory.

**Next proof:** [Issue #16](https://github.com/Hyperbog/CV3DCJP/issues/16) requires opt-in PPU palette-write and CHR mapping event instrumentation with the same CPU fetch/cycle context. [Issue #3](https://github.com/Hyperbog/CV3DCJP/issues/3) stays open for global room placement and independent frame parity. No game-engine source modifications are part of this acceptance report.
