# COURSE UPGRADE SPECIFICATION

## Module: Infinite Growth Laboratory

---

# 0. PURPOSE

Introduce a new Advanced Laboratory Scene:

> Infinite Growth Lab

This laboratory demonstrates how real-time systems behave when
data continuously grows without artificial limits.

The simulation MUST:

    continuously create new entities forever

until the engine introduces strategies to remain stable.

The goal is to teach:

    scalable systems are defined by constraints, not capacity.

---

# 1. HIGH-LEVEL EDUCATIONAL GOALS

The Infinite Growth Lab MUST teach:

- why naive infinite systems fail
- memory exhaustion behavior
- CPU scaling limits
- long-session stability problems
- amortized work distribution
- budgeting systems
- entity lifetime management
- streaming and eviction concepts
- simulation scope control

Students must experience system collapse first,
then learn how engines prevent it.

---

# 2. BUILD INTEGRATION REQUIREMENTS

BUILD INTEGRATION:

Labs are ALWAYS compiled into the game binary.
No conditional compilation flags needed.
The student runs `./build-dev.sh --backend=raylib -r`
and ALL scenes + ALL labs are available.

This was changed from the original design (which used #ifdef guards)
because requiring build flags to access labs was bad UX.

KEY BINDING: Press [I] to enter Infinite Growth Lab. Press [Tab] to cycle through 10 modes. Press [1-9] to return.

---

# 3. SCENE OVERVIEW

Simulation continuously spawns entities.

Characteristics:

- no fixed entity cap initially
- entities move or simulate behavior
- world expands or scrolls infinitely
- spawning never stops

Examples:

- endless particles
- infinite enemies
- infinite world tiles
- continuous effects

The system MUST eventually degrade without controls.

---

# 4. REQUIRED MODES (10 total, covering the full real-engine progression)

REQUIRED MODES (10 total, covering the full real-engine progression):

Stage 1 — Allocation approaches:
  Mode 0: malloc/realloc unbounded (INTENTIONALLY BAD — shows degradation)
  Mode 1: Arena pre-allocation (4MB bump pointer, O(1) allocation, no realloc)
  Mode 2: Pool recycling (fixed slots, free list, zero allocation in game loop)

Stage 2 — Lifetime + budget strategies:
  Mode 3: Lifetime cleanup (entities expire via TTL, steady-state count)
  Mode 4: Budget cap (hard max active entities, spawner adapts)

Stage 3 — Spatial strategies:
  Mode 5: Spatial streaming (only simulate within radius of screen center)
  Mode 6: Simulation LOD (near: full update, far: every 4th frame)

Stage 4 — Work distribution:
  Mode 7: Amortized work (stagger 1/4 entities per frame, 4x dt compensation)
  Mode 8: Threaded update (split array in two halves, pthread per half, join before render)

Stage 5 — Production combined:
  Mode 9: Arena + pool + lifetime + budget + amortized — all strategies combined

User toggles modes LIVE.

---

# 5. THREADING REQUIREMENT (Mode 8)

The threaded update mode demonstrates basic CPU parallelism:
- Split entity array into two halves
- Create one pthread for the first half
- Main thread processes the second half
- pthread_join before rendering

This is safe because the two halves are DISJOINT — no shared mutable state.
No locks needed. This is WHY data-oriented design enables easy parallelism:
independent data = independent threads.

Threading is the LAST optimization in the real-engine progression, not the first.
The lesson explains: fixed pools → arenas → budget → amortized → THEN threading.

---

# 6. MEMORY HANDLING

Memory handling uses malloc/realloc for unbounded mode (intentionally bad)
and a minimal inline arena for arena/production modes (NOT the full platform
foundation arena.h — just a simple bump allocator for the demo).

---

# 7. RUNTIME TOGGLE SYSTEM

User must toggle strategies LIVE.

Requirements:

- no restart required
- growth continues during toggles
- performance changes observable instantly

---

# 8. PERFORMANCE OBSERVABILITY (MANDATORY)

Display:

- total entities created
- active entities
- memory usage
- spawn rate
- destruction rate
- frame time
- frame-time variance
- simulation radius

Students must observe stabilization emerging.

HUD RENDERING:

All metrics are rendered directly into the CPU pixel buffer using
the FONT_8X8 bitmap font (same as the Platform Foundation Course's
draw-text.c). Both backends (Raylib + X11) show identical output.

The game_hud() function builds the text string.
The game_render() function draws it onto the pixel buffer
after scene rendering, before GPU upload.

HUD format: two lines
  Line 1: Scene name + key hints
  Line 2: Scene-specific real-time stats

---

# 9. VISUAL DEBUG OVERLAY

Provide visualization:

- active simulation region
- streamed zones
- LOD levels
- recycled entities

Goal:
make invisible engine decisions visible.

---

# 10. FAILURE VISIBILITY (IMPORTANT)

The lab MUST allow system failure.

Students must see:

- runaway memory usage
- FPS collapse
- instability

Failure is part of the lesson.

---

# 11. DATA FLOW REQUIREMENTS

Growth control must rely on:

- explicit data decisions
- lifetime management
- budgeting logic

Forbidden:

- hidden automatic limits
- silent caps without visualization

---

# 12. COURSE DOCUMENTATION UPDATES

Update:

---

## build-large-arrays-of-things-course.md

Add module:

    Infinite Growth Laboratory

Explain:

real engines survive indefinitely,
not just short demos.

---

## PLAN.md

Insert as FINAL CAPSTONE LAB.

Progression becomes:

1. Large Arrays
2. Particle Lab
3. Data Layout Lab
4. Spatial Partition Lab
5. Memory Arena Lab
6. Infinite Growth Lab (Capstone)

---

## PLAN-TRACKER.md

Add milestones:

- unbounded baseline
- lifetime cleanup
- budget system
- streaming system
- LOD simulation
- recycling system
- amortized workload

---

## GLOBAL_STYLE_GUIDE.md

Add rules:

- scalability discussions required
- stability prioritized over peak performance
- systems must survive long runtimes

---

## README.md

Add section:

Why Infinite Growth Matters

Explain difference between demo code and engine code.

---

# 13. LESSON SYNCHRONIZATION

Update lessons to introduce:

- long-running program design
- stability engineering
- amortization concepts
- scalability mindset

Connect all previous labs into unified understanding.

---

# 14. TRANSCRIPT RE-AUDIT TASK

Re-review:

    _ignore/Avoiding Modern C++ Anton Mikhailov.txt

Ensure inclusion of:

- explicit control over systems
- avoiding hidden runtime costs
- predictable scalability
- simplicity enabling growth

Add missing concepts where needed.

---

# 15. ARCHITECTURAL CONSTRAINTS

The Infinite Growth Lab MUST:

- integrate prior labs together
- reuse allocators
- reuse spatial systems
- reinforce data-oriented philosophy

This lab represents real engine behavior.

---

# 16. COMPLETION REQUIREMENT

Do NOT complete until:

- infinite growth observable
- failure demonstrable
- stabilization strategies functional
- documentation updated
- lessons synchronized
- course internally consistent

This is the course capstone.

---

# END OF SPECIFICATION
