# COURSE UPGRADE SPECIFICATION

## Module: Spatial Partition Laboratory

---

# 0. PURPOSE

Introduce a new Advanced Laboratory Scene:

> Spatial Partition Lab

This laboratory demonstrates how spatial data structures transform
simulation performance by reducing search complexity.

The lab must clearly show:

    O(N²) brute force → structured spatial queries → scalable simulation

This is NOT a gameplay feature.

It is a controlled experiment allowing students to observe
algorithmic complexity in real time.

---

# 1. HIGH-LEVEL EDUCATIONAL GOALS

The Spatial Partition Lab MUST teach:

- Why naive collision detection fails at scale
- Difference between algorithmic cost and micro-optimization
- Spatial locality concepts
- Query acceleration structures
- Broadphase vs narrowphase thinking
- Data grouping enabling faster queries
- Engine-level reasoning about world organization

Students must visually and numerically experience performance collapse
and performance recovery.

---

# 2. BUILD INTEGRATION REQUIREMENTS

BUILD INTEGRATION:

Labs are ALWAYS compiled into the game binary.
No conditional compilation flags needed.
The student runs `./build-dev.sh --backend=raylib -r`
and ALL scenes + ALL labs are available.

This was changed from the original design (which used #ifdef guards)
because requiring build flags to access labs was bad UX.

KEY BINDING: Press [G] to enter Spatial Partition Lab. Press [Tab] to cycle modes (Naive → Grid → Hash → Quadtree). Press [1-9] to return.

---

# 3. SCENE OVERVIEW

Simulation contains:

- Large number of moving entities (1000–5000 recommended)
- Continuous motion
- Collision or proximity interaction
- Optional player-controlled entity

The goal is to force interaction queries between entities.

---

# 4. BASELINE MODE — NAIVE APPROACH (MANDATORY)

Implement baseline algorithm:

    for each entity A
        for each entity B
            test interaction

Characteristics:

- O(N²) checks
- intentionally inefficient
- becomes slow at scale

Display live:

- number of interaction checks
- frame time

This mode is REQUIRED as a teaching baseline.

---

# 5. REQUIRED SPATIAL PARTITION MODES

The user must toggle between multiple query strategies.

---

## MODE 1 — Naive All-Pairs

Purpose:
Expose scaling failure.

Must remain selectable at all times.

---

## MODE 2 — Uniform Grid Partition

Divide world into fixed-size cells.

Entities assigned to cells based on position.

Queries only check:

- same cell
- neighboring cells

Teaches:

- spatial locality
- constant-time neighborhood approximation

---

## MODE 3 — Spatial Hash Grid

Grid without fixed bounds.

Hash function maps position → bucket.

Teaches:

- scalable world design
- memory-efficient partitioning

---

## MODE 4 — Quadtree (Optional Advanced)

Hierarchical subdivision of space.

Teaches:

- adaptive spatial resolution
- uneven density handling

If implemented, must remain simple and educational.

---

# 6. RUNTIME TOGGLE SYSTEM

User must switch modes LIVE:

Required controls:

- keyboard toggle between partition modes
- immediate behavior change
- simulation continues running

No restart allowed.

Students must instantly observe performance differences.

---

# 7. PERFORMANCE OBSERVABILITY (MANDATORY)

Display real-time metrics:

- entity count
- number of interaction checks
- frame time
- update time
- active partition method

Students must clearly see:

    same simulation + different algorithm = massive performance change

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

# 8. VISUAL DEBUG OVERLAY (VERY IMPORTANT)

Render visualization of partitions:

Examples:

Uniform Grid:

- draw grid lines
- highlight active cells

Spatial Hash:

- highlight occupied buckets

Quadtree:

- draw subdivision boxes

Goal:
Make spatial organization visible.

---

# 9. QUERY PIPELINE REQUIREMENTS

All modes must share:

- identical entity behavior
- identical interaction rules
- identical visuals

ONLY query strategy changes.

This isolates algorithmic impact.

---

# 10. DATA FLOW REQUIREMENTS

Spatial structures MUST:

- rebuild or update each frame
- operate on existing arrays
- avoid per-entity heap allocation
- avoid hidden abstractions

Prefer explicit loops and simple data structures.

---

# 11. COURSE DOCUMENTATION UPDATES

Update the following files:

---

## A. build-large-arrays-of-things-course.md

Add module:

    Spatial Partition Laboratory

Explain:

- why brute force fails
- why engines partition space
- relationship to physics and rendering systems

---

## B. PLAN.md

Insert after Data Layout Lab.

Progression becomes:

1. Large Arrays
2. Particle Lab (scale)
3. Data Layout Lab (memory)
4. Spatial Partition Lab (algorithms)

---

## C. PLAN-TRACKER.md

Add milestones:

- naive collision system
- uniform grid implementation
- spatial hash implementation
- runtime toggle system
- debug visualization
- performance overlay

---

## D. GLOBAL_STYLE_GUIDE.md

Add rules:

- algorithms must remain visible
- avoid abstraction hiding complexity
- educational clarity preferred over optimization tricks
- diagrams encouraged

---

## E. README.md

Add explanation:

This lab demonstrates that scalable engines depend on
smart query organization rather than faster CPUs.

---

# 12. LESSON SYNCHRONIZATION

Update lessons if necessary:

- add explanation of Big-O intuition
- introduce broadphase vs narrowphase concepts
- connect spatial partitioning to real engines
- include diagrams and step-by-step walkthroughs

Ensure conceptual continuity with previous labs.

---

# 13. TRANSCRIPT RE-AUDIT TASK

Re-review:

    _ignore/Avoiding Modern C++ Anton Mikhailov.txt

Verify inclusion of:

- simplicity over abstraction
- algorithmic clarity
- explicit data iteration
- avoiding unnecessary complexity
- predictable performance reasoning

Add missing ideas where required.

---

# 14. ARCHITECTURAL CONSTRAINTS

The Spatial Partition Lab MUST:

- extend existing architecture
- reuse entity data model
- avoid OOP hierarchies
- avoid hidden containers
- reinforce data-oriented philosophy

Complexity must arise from algorithm choice only.

---

# 15. COMPLETION REQUIREMENT

Do NOT finish until:

- all partition modes implemented
- runtime switching works
- visual overlays exist
- performance differences measurable
- documentation updated
- lessons synchronized
- transcript audit completed
- course remains internally consistent

This is a full curriculum upgrade.

---

# END OF SPECIFICATION
