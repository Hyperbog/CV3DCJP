# Target PC-native architecture

This is a **modern Windows game engine**, not an NES PPU or widescreen-framebuffer extension. The ROM remains authoritative for *original behavior*; NES display hardware does **not** constrain native resolution or world representation.

```text
Owner-supplied Japanese ROM (local, never committed)
       |                  |
       v                  v
ROM asset compiler   ROM-backed behavior execution
       |                  |
       v                  v
Versioned StageData   Frame-authoritative WorldState
       |                  |
       +--------+---------+
                |
    Camera2D: independent X/Y, zoom, bounds
                |
                v
 Native renderer: terrain + objects + sprites + HUD
                |
                v
Resizable / fullscreen platform drawable
```

## Subsystem boundaries

- **Asset compiler**: deterministically extracts tile, metatile, palette, room, geometry, and animation data with provenance, per exact ROM identity.
- **StageData**: immutable preloaded world geometry in stage-global coordinates, plus explicit data for dynamic graphics phases. Missing placement fails verification rather than being guessed.
- **WorldState**: player, NPCs, items, candles, destructible geometry, and current room/cell gameplay state. ROM-backed execution remains the original behavior engine until individual native equivalents are verified.
- **Camera2D**: tracks the player and performs transition animation; changes no terrain or entity identity.
- **Renderer**: takes StageData, WorldState, and Camera2D and renders into current platform drawable dimensions. Terrain and entity transforms are identical; HUD is screen-space.
- **Platform**: input, audio, timing, resize, HiDPI, windowed and fullscreen modes.

## Display invariants

Never hard-code 960x540 or 256x240 as the output image size. Query the live drawable size. At constant zoom, same-aspect resolutions show the same world footprint; wider aspects expose more terrain when available. No background stitching from the original framebuffer. No camera fallback to room (0,0) after an unqualified observation.

## Behavior migration

Begin with the known-working ROM execution core (rather than rewriting it blindly). Move behavior components to native modules only with matched ROM routines, deterministic playback tests, and regression proof. Keep a faithful reference mode for comparisons.
