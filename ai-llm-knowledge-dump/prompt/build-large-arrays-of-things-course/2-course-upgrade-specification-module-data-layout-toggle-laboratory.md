# COURSE UPGRADE SPECIFICATION

## Module: Data Layout Toggle Laboratory

---

# 0. PURPOSE

Introduce a new Advanced Laboratory Scene:

> Data Layout Toggle Lab

This laboratory visually and measurably demonstrates how
**memory layout affects performance**.

The goal is NOT gameplay.

The goal is for students to directly observe that:

    Same logic + Different data layout = Different performance.

This lab is the conceptual bridge between:

- Large Arrays Of Things
- Cache locality
- Data-Oriented Design
- ECS intuition
- Real engine architecture decisions

---

# 1. HIGH-LEVEL EDUCATIONAL GOALS

The Data Layout Toggle Lab MUST teach:

- Memory traversal cost
- Cache locality intuition
- Why Array-of-Structs exists
- Why Struct-of-Arrays exists
- Branch prediction influence
- Data grouping advantages
- Why engines reorganize data

Students must SEE performance change without changing gameplay.

---

# 2. BUILD INTEGRATION REQUIREMENTS

BUILD INTEGRATION:

Labs are ALWAYS compiled into the game binary.
No conditional compilation flags needed.
The student runs `./build-dev.sh --backend=raylib -r`
and ALL scenes + ALL labs are available.

This was changed from the original design (which used #ifdef guards)
because requiring build flags to access labs was bad UX.

KEY BINDING: Press [L] to enter Data Layout Lab. Press [Tab] to cycle layouts (AoS → SoA → Hybrid). Press [1-9] to return.

---

# 3. CORE CONCEPT REQUIREMENT (CRITICAL)

All layouts MUST execute:

- identical update logic
- identical movement rules
- identical visuals

Only the **data organization** may change.

If behavior differs, the lab fails.

---

# 4. SCENE OVERVIEW

Display a large number of entities (1000–5000 recommended).

Entities perform simple continuous updates:

- move
- update velocity
- interact with player or environment
- optional archetype variation

Simulation complexity must be large enough
for layout differences to become measurable.

---

# 5. REQUIRED DATA LAYOUT MODES

Implement runtime toggle between layouts.

Minimum modes:

---

## MODE 1 — Array of Structs (AoS)

Example conceptual form:

    struct Particle {
        vec2 position;
        vec2 velocity;
        float aggressiveness;
        int archetype;
    };

    Particle particles[N];

Characteristics:

- intuitive layout
- mixed data per entity
- poorer cache traversal for single-property passes

---

## MODE 2 — Struct of Arrays (SoA)

Example conceptual form:

    struct ParticleSoA {
        vec2 position[N];
        vec2 velocity[N];
        float aggressiveness[N];
        int archetype[N];
    };

Characteristics:

- contiguous property access
- cache-friendly iteration
- preferred data-oriented structure

---

## MODE 3 — Hybrid Layout (Recommended)

Example:

- hot data grouped together
- cold data separated

Teaches:
hot vs cold memory separation.

---

# 6. RUNTIME TOGGLE SYSTEM

User must be able to switch layouts LIVE.

Required:

- keyboard toggle
- instant layout rebuild
- simulation continues running

Rules:

- No restart required
- No behavior change allowed
- Only layout differs

Switching layout may rebuild data internally.

---

# 7. PERFORMANCE OBSERVABILITY (MANDATORY)

Display real-time metrics:

- FPS
- update time
- iteration time
- entity count
- layout name currently active

Optional advanced metrics:

- memory stride visualization
- cache-line approximation indicator
- branch divergence estimate

Students must directly correlate layout → performance.

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

# 8. VISUAL MEMORY ACCESS DEMONSTRATION

Add optional debug visualization:

Examples:

- highlight sequential access
- show stride distance
- display memory walk pattern

Goal:
Make invisible cache behavior visible.

---

# 9. UPDATE PIPELINE REQUIREMENTS

Each layout must use explicit iteration:

    for (int i = 0; i < count; ++i)

No containers.
No abstraction hiding iteration.

The loop must remain visible to learners.

---

# 10. COURSE DOCUMENTATION UPDATES

Update the following files:

---

## A. build-large-arrays-of-things-course.md

Add module:

    Data Layout Toggle Laboratory

Explain:

- why layout matters more than algorithms sometimes
- relation to cache locality
- relation to engine design

---

## B. PLAN.md

Insert lab AFTER Particle Laboratory.

Learning progression becomes:

1. Large Arrays
2. Particle Lab (scale)
3. Data Layout Lab (memory reality)

---

## C. PLAN-TRACKER.md

Add milestones:

- AoS implementation
- SoA implementation
- Hybrid layout
- Runtime switching
- Performance overlay
- Visualization tools

---

## D. GLOBAL_STYLE_GUIDE.md

Add teaching rules:

- performance differences must be measurable
- avoid premature abstraction
- data layout discussions must remain concrete
- diagrams encouraged for memory representation

---

## E. README.md

Add section:

Why Data Layout Matters

Explain this lab is the turning point toward
data-oriented engine thinking.

---

# 11. LESSON SYNCHRONIZATION

If new concepts appear, update lessons:

- memory layout diagrams
- cache explanation sections
- iteration examples
- performance discussions

Ensure terminology consistency across course.

---

# 12. TRANSCRIPT RE-AUDIT TASK

Re-review:

    _ignore/Avoiding Modern C++ Anton Mikhailov.txt

Verify inclusion of:

- data-oriented iteration
- simplicity preference
- layout over abstraction
- performance transparency
- predictable code behavior
- avoiding hidden costs

Add missing insights to lessons if needed.

---

# 13. ARCHITECTURAL CONSTRAINTS

The Data Layout Lab MUST:

- extend existing architecture
- NOT introduce OOP hierarchy
- NOT add abstraction layers hiding data
- reinforce course philosophy

Complexity must emerge from layout decisions only.

---

# 14. COMPLETION REQUIREMENT

Do NOT complete task until:

- all layouts implemented
- runtime switching works
- performance differences observable
- documentation updated
- lessons synchronized
- transcript audit completed
- course internally consistent

This is a full curriculum upgrade.

---

# END OF SPECIFICATION
