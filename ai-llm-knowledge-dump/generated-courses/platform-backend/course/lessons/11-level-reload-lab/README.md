# Module 11: Level Reload Lab

This module teaches a different allocator story from the Arena Lab.

The previous lab focused on nested transient work and rewind. This one focuses on a rebuildable scene-owned payload: headers plus one contiguous strip of entities that are torn down and regenerated together.

---

## Observable Outcome

By the end of this module, you should be able to:

- explain the Level Reload Lab as a scene-local rebuild proof rather than a generic animation
- distinguish real rebuild events from scan-only spikes that reuse the same payload in place
- explain why the scene stores grouped headers plus one contiguous entity strip
- verify rebuild, scan traversal, and spike behavior across the world scene, HUD, trace panel, event log, and audio trace together

---

## Lesson Order

1. [lesson-52-level-reload-scene-contract-and-lifetime-boundary.md](lesson-52-level-reload-scene-contract-and-lifetime-boundary.md)
   - the scene contract and the ownership boundary it is teaching
2. [lesson-53-level-arena-reset-rebuild-and-scene-enter-rules.md](lesson-53-level-arena-reset-rebuild-and-scene-enter-rules.md)
   - the real reset-and-rebuild path in the runtime
3. [lesson-54-formations-entities-and-one-contiguous-strip.md](lesson-54-formations-entities-and-one-contiguous-strip.md)
   - the rebuilt payload shape: headers plus contiguous entity storage
4. [lesson-55-scan-loop-spike-action-and-exercise-progression.md](lesson-55-scan-loop-spike-action-and-exercise-progression.md)
   - passive scan traversal versus manual spike pressure
5. [lesson-56-render-audio-trace-and-proof-surfaces.md](lesson-56-render-audio-trace-and-proof-surfaces.md)
   - the proof surfaces that keep rebuild and spike from being confused
6. [lesson-57-forced-scene-walkthrough-and-reload-proof-checks.md](lesson-57-forced-scene-walkthrough-and-reload-proof-checks.md)
   - the end-to-end operator workflow for verifying the scene in isolation

---

## Why This Module Matters

Without this module, the learner could still think of arenas only as a scratch-memory trick.

The Level Reload Lab widens that model: arenas are also useful when one scene owns one whole payload that can be discarded and rebuilt together while the surrounding app state survives.

---

## How To Work Through It

For each lesson in this module:

1. separate what is rebuilt from what only animates
2. trace one proof claim through at least two different evidence surfaces
3. keep header-versus-payload structure explicit
4. compare `R` and `SPACE` repeatedly until the ownership difference feels obvious

If you only watch the flash effect, this lab is easy to misread. The real lesson appears when you keep checking serial, seed, scan progress, trace evidence, and warning context together.

---

## Verification Before Module 12

Do not move on until you can explain, from memory:

- what exactly gets rebuilt on `R`
- what exactly does not get rebuilt on `SPACE`
- why formations are headers while entities are the real contiguous payload
- which proof surfaces confirm stable traversal over already rebuilt data

If those answers are clear, you are ready for the Pool Reuse Lab, where the next allocator story shifts from rebuildable ownership to free-list return and reuse.
