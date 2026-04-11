# Module 13: Slab + Audio Lab

This module teaches the allocator story that comes after fixed-slot pool reuse.

The previous module focused on one bounded backing store where individual slots return to a free list and later come back. This module widens the model again: same-size records are allocated into slab pages, new pages appear only when existing ones fill up, and later transport states keep reusing that expanded capacity instead of demanding endless growth.

---

## Observable Outcome

By the end of this module, you should be able to:

- explain the Slab + Audio scene as a page-backed same-size allocator lesson rather than as a generic audio visualizer
- trace one event from slab allocation, through transport pressure, into page growth, and then into later reuse inside the grown page set
- separate allocator growth from warning-line severity and sequencer crowding
- verify slab behavior across transport blocks, event cards, page boards, HUD metrics, the trace panel, event log, and audio warning context together

---

## Lesson Order

1. [lesson-64-slab-audio-scene-contract-and-page-backed-allocation.md](lesson-64-slab-audio-scene-contract-and-page-backed-allocation.md)
   - the scene contract, slab ownership layer, and why transport state is part of the lesson
2. [lesson-65-event-allocation-page-indexing-and-seeded-fill.md](lesson-65-event-allocation-page-indexing-and-seeded-fill.md)
   - how the first page is seeded and how each event is stamped with page and transport identity
3. [lesson-66-transport-state-machine-bursts-and-page-growth.md](lesson-66-transport-state-machine-bursts-and-page-growth.md)
   - how transport cadence, TTL decay, and bursts create page-growth pressure
4. [lesson-67-page-occupancy-render-and-hud-proof-surfaces.md](lesson-67-page-occupancy-render-and-hud-proof-surfaces.md)
   - how the scene renders page occupancy, transport state, and slab growth visibly
5. [lesson-68-transport-pressure-warning-lines-and-guided-interpretation.md](lesson-68-transport-pressure-warning-lines-and-guided-interpretation.md)
   - how prompts, trace evidence, and warning strings teach cause versus severity
6. [lesson-69-forced-scene-walkthrough-and-slab-growth-proof-checks.md](lesson-69-forced-scene-walkthrough-and-slab-growth-proof-checks.md)
   - the end-to-end operator workflow for forcing growth and verifying later reuse in isolation

---

## Why This Module Matters

Without this module, the learner could still think allocator design is only about rewinding memory, rebuilding whole payloads, or recycling one fixed pool of slots.

The Slab + Audio Lab introduces the next pattern: same-size traffic that can exceed one page, force a second page into existence, and then settle into reuse across that larger footprint. That is a very common systems pattern, and it has different proof surfaces from both arenas and pools.

---

## First Reading Strategy

Work through this module in a strict order:

1. understand why slab pages exist before focusing on audio behavior
2. separate scene-visible event pointers from the slab allocator underneath them
3. treat transport state as pressure generation, not as the final lesson by itself
4. keep growth proof separate from warning severity and recovery

If you only watch the warning line or the transport labels, the allocator story will blur. The module lands only when page count, page occupancy, event lifetime, and warning context are read together.

---

## Verification Before Module 14

Do not move on until you can explain, from memory:

- why slab pages are a better fit here than one fixed pool block
- why page growth is not the end of the lesson, only the pressure spike
- how the scene proves allocator growth separately from warning severity
- why the final exercise waits for a later flush state instead of stopping when `pages` first increases

If those answers are clear, you are ready for Module 14, where the course stops treating the scenes as separate lab islands and instead teaches the runtime as one integrated diagnostic system.
