# Module 14: Runtime Integration Capstone

This module closes the course by shifting from scene-by-scene proof to runtime-wide verification.

The previous four lab modules taught you how to isolate one scene, force one allocator or lifetime pattern, and prove that local behavior with trace panels, logs, HUD metrics, and audio cues. This final module asks a broader question: how do all of those teaching surfaces, control policies, render layers, build personalities, and extension rules hold together as one coherent runtime?

---

## Observable Outcome

By the end of this module, you should be able to:

- explain the runtime as one integrated diagnostic system rather than four disconnected scene labs
- measure the runtime honestly with bench builds and named profiler scopes
- explain how scene requests, overrides, rebuild retries, world rendering, HUD rendering, and scene isolation cooperate in one frame loop
- verify the current runtime across both backends, all four scenes, and the stable `game_app_*` facade
- describe the minimum honest handoff for adding another scene without breaking the diagnostic layer

---

## Lesson Order

1. [lesson-70-bench-builds-profiler-hooks-and-measured-runtime-claims.md](lesson-70-bench-builds-profiler-hooks-and-measured-runtime-claims.md)
   - how the runtime changes build personality for honest measurement
2. [lesson-71-rebuild-failures-retry-paths-and-override-locks.md](lesson-71-rebuild-failures-retry-paths-and-override-locks.md)
   - how requests, overrides, and rebuild failure stay explicit instead of implicit
3. [lesson-72-world-context-hud-context-and-zoom-stable-panels.md](lesson-72-world-context-hud-context-and-zoom-stable-panels.md)
   - how one frame keeps world evidence and diagnostic overlays readable at the same time
4. [lesson-73-hud-logs-trace-panels-and-diagnostic-composition.md](lesson-73-hud-logs-trace-panels-and-diagnostic-composition.md)
   - how the HUD becomes one integrated proof surface across all scenes
5. [lesson-74-scene-isolation-rebuild-boundaries-and-extension-rules.md](lesson-74-scene-isolation-rebuild-boundaries-and-extension-rules.md)
   - how scene isolation is enforced and what must change when the scene set grows
6. [lesson-75-final-verification-matrix-and-custom-scene-handoff.md](lesson-75-final-verification-matrix-and-custom-scene-handoff.md)
   - the final operator checklist and stable extension boundary after the course ends

---

## Why This Module Matters

Without this module, the learner could still understand each scene locally while missing the larger runtime contract that makes the whole course maintainable.

The capstone matters because real systems work only when control flow, render composition, instrumentation, failure handling, and extension boundaries all agree. This module turns those agreements into explicit, teachable rules.

---

## First Reading Strategy

Work through this capstone in a strict order:

1. start with build personality and measurement
2. then read control and rebuild policy
3. then read frame composition and HUD layering
4. then read the full diagnostic surface as one system
5. only after that, read scene-extension and final verification rules

If you jump directly to the final checklist, the runtime will feel like a pile of commands. The module lands only when the learner understands why those commands prove meaningful boundaries.

---

## Verification Before Signoff

Do not finish the course until you can explain, from memory:

- why a bench build is a different runtime personality instead of a dev build with extra prints
- why `requested` scene, current scene, and authoritative rebuilt scene are not the same thing
- why the HUD uses the same render system as the world but a different camera policy
- why adding a fifth scene means updating public text and diagnostic switches, not only update/render code
- why the stable handoff after the course is still the same `game_app_init`, `game_app_update`, `game_app_render`, and `game_app_get_audio_samples` facade

If those answers are clear, the course has done its job: you can now read, verify, and extend the runtime as one integrated platform foundation.
