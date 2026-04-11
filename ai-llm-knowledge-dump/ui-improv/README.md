## Summary: What Each Utility Does

| Category                  | Utilities                                                                       | Purpose                              |
| ------------------------- | ------------------------------------------------------------------------------- | ------------------------------------ |
| **Coordinate Conversion** | `world_to_screen_*`, `ref_to_screen_*`, `norm_to_screen_*`, `screen_to_world_*` | Convert between coordinate spaces    |
| **Anchors**               | `get_anchor_pos`, `anchor_offset`, `anchor_pivot`                               | Position UI relative to screen edges |
| **Aspect Ratio**          | `fit_aspect` with `SCALE_FIT/FILL/STRETCH`                                      | Handle different screen ratios       |
| **Safe Area**             | `set_safe_area_insets`, `set_safe_area_percent`                                 | Account for notches, system UI       |
| **Camera**                | `camera_set_pos`, `camera_move`, `camera_follow`, `camera_clamp_to_world`       | Pan, zoom, follow targets            |
| **Screen Shake**          | `shake_make`, `shake_update`, `camera_apply_shake`                              | Impact effects                       |
| **Text Layout**           | `TextCursor`, `cursor_newline`, `cursor_gap`, `text_width`                      | Flowing text positioning             |
| **Layout**                | `grid_pos`, `HStack`, `VStack`                                                  | Grid and stack layouts               |
| **Animation**             | `lerp`, `ease_*` functions                                                      | Smooth transitions                   |
| **Culling**               | `is_visible_world`, `get_visible_world_bounds`                                  | Skip off-screen objects              |
| **Debug**                 | `debug_draw_world_grid`, `debug_draw_viewport`, `debug_draw_anchors`            | Visualize coordinate systems         |

---

## Key Design Principles

1. **Single Source of Truth**: `RenderContext` holds all scaling info
2. **Convert Once**: Convert coordinates at render time, not during game logic
3. **Explicit Coordinate Spaces**: Functions clearly indicate which space they work in
4. **Composable**: Small utilities combine for complex layouts
5. **Debug-Friendly**: Visual helpers for understanding coordinate systems

This architecture scales from simple demos to full games while keeping the mental model clear!
