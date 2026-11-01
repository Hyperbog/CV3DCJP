# Stage 0 ROM transition and palette evidence — v2

**Status:** ROM-backed observations collected and authenticated; **full Stage 0 spatial placement and original-frame pixel parity are not yet proven**. This is the evidence boundary for [Issue #3](https://github.com/Hyperbog/CV3DCJP/issues/3).

## Evidence sources

- Private authenticated Japanese ROM: 393232 bytes, SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`.
- Historical `CVIII-Phase5H8F-Private-Evidence.zip`: owner-recorded original-ROM-backed NESRecomp PPU/CPU frame snapshots. Not a separate Mesen2 oracle.
- `cviii-evidence-5h8f.frames.csv`: SHA-256 `c0cb6f83f33a9b9a201e439d1f2648769752c9e7ea0619eec3c41ae9fbc8240a`.
- `cviii-evidence-5h8f.events.csv`: SHA-256 `c44492ba681efc991cd8c16d8e4824ed48fef1fbbd4040c158f81b0ec683860e`.
- Previous private `CV3DCJP_ROM_Content_Extraction_v1_PRIVATE.zip`: authenticated room-local selectors and layouts.
- Previous 5H8Q ROM static writer-site inventory, verified against actual original ROM instruction bytes; 5H8S CPU hook results recorded separately.

## Measured results

| Dimension | Evidence | Verified scope |
| --- | --- | --- |
| Stage 0 rooms | 8 v1 ROM records | ROM-local layout identity, offsets, size |
| Frame coverage | 13,944 frames | Continuous recorded frame IDs, frame-end PPU palette record digest checked per frame |
| Palette RAM states | 24 distinct 32-byte values (includes initial/boot states) | ROM-backed original-game *frame-end* palette RAM, not immutable ROM palette tables |
| Selector changes | 10 raw selector runs (including initial and one-frame transient states), 7 stable edges | Recorded selector sequence only; no proof of global adjacency/direction |
| ROM static selector writer sites | 16 opcode + zero-page-target matches | Actual original ROM instruction bytes; not proof of execution at observed frame |
| Separate 5H8S writer trace | 43 selector-target stores, 1 attributed static site | ROM-backed native CPU hook in a separate capture; not synchronized to this frame sequence |
| Stage 0 global room placements | Previously established relative placements of 0/0/0 and 0/1/0 only | 5H8M previous evidence with arbitrary origin at (0,0); no new placement |
| New ROM-verified global placements | **0** | Six additional Stage 0 room origins remain unresolved |

### Observed stable selector sequence

`0/0/0 → 0/1/0 → 0/1/1 → 0/1/3 → 0/1/2 → 0/2/1 → 0/2/0 → 0/3/0`

Between `0/1/2` and `0/2/1`, the raw capture temporarily reports `0/1/1` for exactly one frame. The v2 output **retains** this as a transient record rather than pretending it defines physical adjacency.

Only the previous two-room horizontal relative observation (0/0/0 at [0,0], 0/1/0 at [768,0]) is carried forward, labeled `PRIOR_5H8M_TWO_ROOM_OBSERVATION_ONLY`. This does not prove an absolute origin encoded in the ROM, and no additional room positions are inferred from selector order.

## Versioned private output

`CV3DCJP_Stage0_ROM_TransitionPalette_Evidence_v2_PRIVATE.zip` contains:

- `levels/stage-00/area-*/room-*.json`: ROM-local provenance plus observed palette references and separate previous-placement metadata.
- `levels/stage-00/connections-observed.json`: exact selector runs and seven stable sequences, with spatial direction/delta set to `null`.
- `graphics/palettes-frame-end.json`: actual privately retained palette bytes, hash identities and room occurrence counts.
- `gameplay/rom-selector-writer-sites.json`: 16 ROM-verified static instruction-site identifiers.
- `source/evidence-manifest.json`: SHA-256 for all external evidence sources and unresolved checks.

**Keep this package private.** Game-derived palette bytes are not committed into this repository.

## Reproducible validation

First regenerate the v1 content ZIP using `tools/extract/export_content_v1.py` and the authenticated ROM. Then validate a historical ROM-backed capture:

```powershell
py -3.12 tools/extract/audit_stage0_observations_v2.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --capture 'C:\PrivateEvidence\CVIII-Phase5H8F-Private-Evidence.zip' `
  --content-v1 'C:\PrivateEvidence\CV3DCJP_ROM_Content_Extraction_v1_PRIVATE.zip' `
  --out 'data/generated/stage0-evidence-v2'
```

This public script checks the original ROM and historical frame/event CSV identities, verifies each 40-byte frame/palette record (8-byte little-endian frame + 32 bytes palette) against the frame digest, and refuses overwrites. It emits eight stage room observation records, the selector run graph and private palette states; the separate historical two-room placement is not silently promoted by this exporter.

Verify the 5H8Q static ROM-writer site audit independently:

```powershell
py -3.12 tools/extract/verify_selector_writers_v2.py `
  --rom 'C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes' `
  --sites 'C:\PrivateEvidence\verified-writer-sites.csv'
```

The local owner's file paths may differ. Neither tool edits an existing ROM or the working game executable.

## Next proof required for spatial completion

Record a **single synchronized** replay containing CPU instruction fetch PC + mapper bank, writes to selector RAM $0034-$0036, PPU scroll and palette events, and original frame IDs. The CPU writer must be attributed to a concrete ROM offset on the same transition. For world placement, also prove directional displacement from original camera/PPU behavior or direct stage script semantics. Independently compare original emulator pixels with the native renderer at matching frames, including palette timing and dynamic CHR banking. Until then, unresolved positions must remain `null` and neither W8–W13 images nor previous atlas renderings are authoritative fixtures.
