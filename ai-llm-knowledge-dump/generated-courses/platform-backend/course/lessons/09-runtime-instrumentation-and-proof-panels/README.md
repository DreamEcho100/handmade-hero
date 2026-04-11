# Module 09: Runtime Instrumentation and Proof Panels

This module takes the live scene state from Module 08 and turns it into teaching evidence.

The runtime is no longer satisfied with "the scene works." It now wants to show the learner what has been proved, what still needs to be demonstrated, and which live metrics justify each claim.

This is the bridge from architecture into the first full lab modules. If the overlay still feels like decoration here, Modules 10-13 will be much harder to read correctly.

---

## Observable Outcome

By the end of this module, you should be able to:

- explain how scene exercise progress is stored and derived from bitmasks instead of hand-written UI state
- distinguish prompt text from observation text and explain why both are needed
- describe how trace-panel rows, evidence suffixes, and audio warning panels are assembled from live runtime data
- explain why the HUD is part of the course architecture rather than generic debug noise

---

## Lesson Order

1. [lesson-41-exercise-masks-focus-and-status.md](lesson-41-exercise-masks-focus-and-status.md)
   - the state model for scene proof progress
2. [lesson-42-prompt-and-observation-guidance.md](lesson-42-prompt-and-observation-guidance.md)
   - dynamic guidance derived from scene and warning state
3. [lesson-43-trace-panel-structure-and-evidence-suffixes.md](lesson-43-trace-panel-structure-and-evidence-suffixes.md)
   - the main proof checklist and label-upgrade system
4. [lesson-44-audio-trace-panels-and-warning-meter.md](lesson-44-audio-trace-panels-and-warning-meter.md)
   - scene-aware audio evidence, cause text, and severity display
5. [lesson-45-final-hud-composition-and-runtime-proof.md](lesson-45-final-hud-composition-and-runtime-proof.md)
   - the full overlay composition that unifies scene, allocator, audio, and proof state

---

## Why This Module Matters

Without this module, the lab scenes would just animate.

With this module, the learner can read the runtime as a proof system: each panel, row, suffix, warning label, and allocator line exists to justify a specific teaching claim with live evidence.

Module 10 depends on this shift. The Arena Lab is only readable if prompt text, observation text, trace rows, HUD summaries, and audio evidence already feel like one coordinated system instead of separate UI fragments.

---

## How To Work Through It

For each lesson in this module:

1. identify the underlying state model first
2. ask what evidence is derived from that state
3. ask how the HUD turns that evidence into a guided action or observation
4. compare one compact summary line against one detailed proof panel beneath it

If you only read the drawing helpers, the panel layout feels cosmetic. If you keep tying each helper back to the state it exposes, the instrumentation becomes architectural.

---

## Verification Before Module 10

Do not move on until you can explain, from memory:

- what `progress_mask` and `required_mask` represent
- how the runtime decides the next focus step
- why evidence suffixes are stronger than a plain completed checkbox
- why the HUD shows both top-level summaries and lower proof panels

If those answers are clear, you are ready for Module 10, where the course stops describing the proof system in the abstract and starts using it inside the first full lab.

If you still read the HUD as generic debug garnish, re-read lessons 41-45 before continuing. The next module assumes you can already separate proof surfaces from merely illustrative animation.
