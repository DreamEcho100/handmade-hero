# Module 06: Coordinate Systems and Camera

This module teaches how the runtime turns gameplay reasoning into pixel placement without forcing the game layer to think directly in raw screen coordinates.

This is the bridge from software rendering fundamentals into scene-space reasoning.

---

## Observable Outcome

By the end of Module 06, you should be able to:

- explain the difference between backbuffer pixels and world units
- describe what `GameWorldConfig` means before looking at any draw helper
- explain how HUD anchoring, origin choice, and camera motion all depend on the same coordinate policy
- trace how a world-space position eventually becomes pixel coordinates through a render context

---

## Lesson Order

1. [lesson-24-world-units-and-gameworldconfig.md](lesson-24-world-units-and-gameworldconfig.md)
   - define the logical world space first
2. [lesson-25-coordinate-origins-and-axis-signs.md](lesson-25-coordinate-origins-and-axis-signs.md)
   - make origin and axis meaning explicit
3. [lesson-26-rendercontext-and-baked-world-to-pixel-math.md](lesson-26-rendercontext-and-baked-world-to-pixel-math.md)
   - bake the coordinate policy into a concrete render context
4. [lesson-27-textcursor-and-hud-layout.md](lesson-27-textcursor-and-hud-layout.md)
   - apply the same rules to readable overlay layout
5. [lesson-28-explicit-world-to-pixel-helpers.md](lesson-28-explicit-world-to-pixel-helpers.md)
   - turn the coordinate policy into reusable helper functions
6. [lesson-29-camera-motion-and-visibility-culling.md](lesson-29-camera-motion-and-visibility-culling.md)
   - move the camera and reason about what stays visible

---

## Why This Module Matters

Without this module, later scene labs would collapse back into pixel guessing.

This module keeps three layers separate:

- logical world space
- camera and origin policy
- final raster output in pixels

That separation is what makes the runtime teachable instead of magical.

---

## How To Work Through It

For each lesson in this module:

1. write down which space a number lives in before doing any math
2. keep camera state separate from object positions in your notes
3. test your understanding with one concrete conversion example
4. verify whether you are reasoning about world space, HUD space, or pixel space

If you keep the spaces separate, the camera lessons stay simple.

---

## Verification Before Module 07

Do not move on until you can explain, from memory:

- why the runtime uses both pixels and world units
- what fields in `GameWorldConfig` define coordinate meaning
- how scene camera state can override the default platform world config
- why HUD anchoring needs a bridge such as `HUD_TOP_Y(...)`

If those answers are clear, you are ready for the allocator and scene-lifetime modules.
