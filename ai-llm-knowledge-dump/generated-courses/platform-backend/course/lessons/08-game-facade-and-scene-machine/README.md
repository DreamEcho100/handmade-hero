# Module 08: Game Facade and Scene Machine

This module takes the allocator runway and turns it into actual game-runtime structure.

Up to now, the course has taught the learner how memory, rendering, audio, and world-space systems work. This module shows how the current branch composes those systems behind one narrow game boundary and one scene-machine control layer.

It is also the last architecture-first module before the course starts spending whole modules inside individual lab scenes. That means the ownership tree and transition policy here need to feel boringly explicit before the later proof modules can feel easy to follow.

---

## Observable Outcome

By the end of this module, you should be able to:

- explain what the backends are allowed to know about the game layer and what stays hidden behind `src/game/main.h`
- describe the difference between session-lifetime app state, scene-machine control state, and level-arena scene payload
- follow scene requests from raw input through wrapping, override policy, and rebuild decisions
- explain why scene enter is a coordinated event that touches memory, camera ownership, logs, counters, and audio profile state

---

## Lesson Order

1. [lesson-36-game-main-surface-and-scene-ids.md](lesson-36-game-main-surface-and-scene-ids.md)
   - the narrow public game facade and the scene-id vocabulary
2. [lesson-37-gameappstate-levelstate-and-scenemachine.md](lesson-37-gameappstate-levelstate-and-scenemachine.md)
   - the internal ownership tree behind the opaque facade pointer
3. [lesson-38-scene-requests-wrapping-and-override-policy.md](lesson-38-scene-requests-wrapping-and-override-policy.md)
   - how input and override rules choose the requested scene
4. [lesson-39-scene-rebuild-flow-and-level-arena-teardown.md](lesson-39-scene-rebuild-flow-and-level-arena-teardown.md)
   - how requested scene changes become grouped teardown and rebuild
5. [lesson-40-scene-enter-side-effects-camera-and-audio.md](lesson-40-scene-enter-side-effects-camera-and-audio.md)
   - the cross-system side effects that make scene enter a real runtime event

---

## Why This Module Matters

Without this module, the later lab scenes would feel like isolated demos instead of one coherent runtime.

The scene machine is the layer that explains why the labs share one game facade, one update/render/audio surface, one scene-request policy, and one rebuild path. It is the architectural hinge between "systems exist" and "systems cooperate."

Module 09 assumes this contract is already clear, and Modules 10-13 keep building on it without re-explaining the ownership split from scratch.

---

## How To Work Through It

For each lesson in this module:

1. separate control metadata from scene payload data
2. ask which lifetime bucket owns the state being discussed
3. trace one scene transition from input to rebuild to side effects
4. keep comparing what the backend sees against what the game layer keeps private

If you only read the code as control flow, the module feels like switching logic. If you keep reading it as ownership plus policy, the structure stays much clearer.

---

## Verification Before Module 09

Do not move on until you can explain, from memory:

- why `GameAppState` is opaque in `main.h`
- why `GameSceneMachine` is not the same thing as `GameLevelState`
- why `requested` and `current` must be separate
- why scene enter is more than assigning a new enum value

If those answers are clear, you are ready for the next module, where the runtime turns scene state into guided proof and instrumentation.

If any answer still collapses into "the backend calls some game code and scenes just switch," re-read lessons 38-40 before moving on. That confusion will make the proof-panel modules much harder than they need to be.
