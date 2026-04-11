# Module 12: Pool Reuse Lab

This module teaches the next allocator story after rebuildable scene ownership.

The previous module showed how one scene-owned payload can be torn down and rebuilt together. This module narrows the scope and asks a different question: when one fixed-size slot is returned, how does the runtime prove that the same bounded backing storage is being reused instead of silently growing?

---

## Observable Outcome

By the end of this module, you should be able to:

- explain the Pool Reuse Lab as a fixed-size free-list proof rather than a generic projectile scene
- trace one probe from allocation, through churn, back to the free list, and into later reuse
- explain the difference between current occupancy, returned capacity, immediate reuse hits, and long-term hottest-slot concentration
- verify pool behavior across the world scene, slot board, HUD, trace panel, event log, and audio warning line together

---

## Lesson Order

1. [lesson-58-pool-reuse-scene-contract-and-free-list-foundation.md](lesson-58-pool-reuse-scene-contract-and-free-list-foundation.md)
   - the scene contract, pool ownership layer, and why barriers exist
2. [lesson-59-slot-indexing-generations-and-reuse-counters.md](lesson-59-slot-indexing-generations-and-reuse-counters.md)
   - how a returned pointer becomes a stable slot identity with reuse history
3. [lesson-60-collision-churn-free-returns-and-lifo-reuse.md](lesson-60-collision-churn-free-returns-and-lifo-reuse.md)
   - how collisions, TTL, and exits drive the free-list cycle
4. [lesson-61-slot-board-probe-labels-and-hud-evidence.md](lesson-61-slot-board-probe-labels-and-hud-evidence.md)
   - how the scene renders fixed slot identity instead of asking you to infer it
5. [lesson-62-burst-pressure-audio-warning-and-guided-proof.md](lesson-62-burst-pressure-audio-warning-and-guided-proof.md)
   - how prompts, trace evidence, and warning logic teach delayed reuse under pressure
6. [lesson-63-forced-scene-walkthrough-and-free-list-proof-checks.md](lesson-63-forced-scene-walkthrough-and-free-list-proof-checks.md)
   - the end-to-end operator workflow for proving free-list reuse in isolation

---

## Why This Module Matters

Without this module, the learner could still think allocator design is only about rewinding scratch memory or rebuilding whole payloads.

The Pool Reuse Lab widens that model again. Some systems keep one fixed backing region alive, return same-size slots to a free list, and hand those slots back out later. The cost model, evidence, and failure modes are different, so the teaching surface has to be different too.

---

## First Reading Strategy

Work through this module in a strict order:

1. identify the bounded backing store before you look at moving probes
2. separate current occupancy from reuse history
3. treat collisions and bursts as churn generators, not as the main lesson
4. keep checking the slot board and trace evidence whenever a probe disappears or reappears

If you only watch the world-space motion, the scene is easy to misread. The real allocator lesson appears when you keep pointer identity, free capacity, generation labels, and warning pressure in the same mental model.

---

## Verification Before Module 13

Do not move on until you can explain, from memory:

- why the pool still lives inside level-owned arena memory
- why `last_freed`, `reuse_hits`, `slot_generations`, and `slot_reuses` are all needed at once
- how the slot board proves fixed backing storage more clearly than moving probes alone
- why the pool scene's warning language is about churn and low free voices instead of rebuilds or transport mode

If those answers are clear, you are ready for Module 13, where the allocator story shifts again from returned fixed slots to same-size slab pages and audio-event pressure.
