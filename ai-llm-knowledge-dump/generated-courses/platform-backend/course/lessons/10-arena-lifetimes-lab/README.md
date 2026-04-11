# Module 10: Arena Lifetimes Lab

This module is the first full lab module.

Everything before it prepared the learner to understand the runtime. This module starts using that understanding in a guided scene where allocator ideas are no longer only explained in prose or helper functions, but made visible as an interactive proof surface.

---

## Observable Outcome

By the end of this module, you should be able to:

- explain the Arena Lifetimes Lab as one complete scene contract rather than a pile of helper functions
- follow the scripted checkpoint phases from baseline, to nesting, to rewind, to clean handoff
- distinguish automatic scene motion from deliberate learner-driven proof events
- verify the allocator lesson through scene graphics, HUD metrics, trace steps, prompts, observations, event logs, and audio cues together

---

## Lesson Order

1. [lesson-46-arena-lab-scene-contract-and-teaching-surface.md](lesson-46-arena-lab-scene-contract-and-teaching-surface.md)
   - the scene contract, scene payload, and why this lab comes first
2. [lesson-47-arena-phase-machine-and-checkpoint-stack.md](lesson-47-arena-phase-machine-and-checkpoint-stack.md)
   - the scripted checkpoint story the scene repeats
3. [lesson-48-manual-step-auto-loop-and-audio-cues.md](lesson-48-manual-step-auto-loop-and-audio-cues.md)
   - how the learner and the runtime each move the lab forward
4. [lesson-49-exercise-flow-prompts-observations-and-log-evidence.md](lesson-49-exercise-flow-prompts-observations-and-log-evidence.md)
   - how this one scene plugs into the generic proof system
5. [lesson-50-render-surface-depth-meters-and-hud-metrics.md](lesson-50-render-surface-depth-meters-and-hud-metrics.md)
   - how the world render and HUD metrics make lifetime behavior visible
6. [lesson-51-forced-scene-walkthrough-and-proof-checks.md](lesson-51-forced-scene-walkthrough-and-proof-checks.md)
   - how to run the lab in isolation and verify the full proof path end to end

---

## Why This Module Matters

Without a first full lab, the learner would know the runtime's architecture but not yet know how to operate it as an experiment surface.

The Arena Lab is the right first lab because it teaches the cleanest lifetime story in the course: nested transient work appears, rewinds, and vanishes while persistent ownership remains stable.

---

## How To Work Through It

For each lesson in this module:

1. identify the stable teaching surface first
2. follow one specific learner action through the scene update path
3. compare at least two proof surfaces for the same claim
4. keep asking which evidence is authoritative and which evidence is illustrative

If you only read the arena lab as animation, you will miss its real design. The point of the lab is not motion alone. The point is that each motion has explicit proof attached to it.

---

## Verification Before Module 11

Do not move on until you can explain, from memory:

- what the Arena Lab is teaching in each major phase
- how a manual `SPACE` press differs from the auto-loop phase change
- which proof surfaces confirm nested temp depth and rewind behavior
- why the forced-scene walkthrough is useful for learning, not just debugging

If those answers are clear, you are ready for the next lab, where the lifetime question shifts from nested scratch rewinds to full scene teardown and rebuild.
