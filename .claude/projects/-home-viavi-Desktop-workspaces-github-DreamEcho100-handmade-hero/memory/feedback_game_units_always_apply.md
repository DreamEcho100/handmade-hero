---
name: GAME_UNITS always applies — no exceptions
description: The game-unit coordinate system (GAME_UNITS_W/H with units_to_pixels conversion) should be used everywhere — including grid games, HUD overlays, and any rendering. There is no "grid-based exception".
type: feedback
---

GAME_UNITS_W/H with units_to_pixels conversion at the call site ALWAYS applies — there is no "grid-based exception" for Tetris/tile games.

**Why:** Even grid games become blurry/pixelated or break layout when the backbuffer resolution changes if they use hardcoded pixel positions. A Tetris grid should be "10 units wide × 20 tall" not "300px × 600px". The pixel-level cell size is computed from `u2p = backbuffer->width / GAME_UNITS_W`.

**How to apply:** When evaluating whether code needs GAME_UNITS conversion:
- Scene rendering that writes pixels directly (e.g., raytracer `bb->pixels[j * stride + i] = ...`) has its own coordinate system (3D camera/NDC) — GAME_UNITS is for the 2D overlay/UI/draw API layer
- ALL draw_rect / draw_text calls should use game-unit coordinates with `* u2p_x/y` conversion at the call site
- HUD overlays, grid games, platformers, physics games — ALL benefit equally
- The course-builder.md's "Grid-based exception" guidance is WRONG and should be corrected
