# COURSE UPGRADE SPECIFICATION

## Module: Memory Arena Laboratory

---

# 0. PURPOSE

Introduce a new Advanced Laboratory Scene:

> Memory Arena Lab

This laboratory demonstrates how allocation strategy affects:

- performance stability
- fragmentation
- frame-time spikes
- data lifetime reasoning

The goal is for students to experience:

    malloc/free mindset → lifetime-based allocation mindset

This is NOT a gameplay feature.

It is a controlled experiment exposing hidden runtime costs.

---

# 1. HIGH-LEVEL EDUCATIONAL GOALS

The Memory Arena Lab MUST teach:

- allocation cost awareness
- fragmentation problems
- lifetime grouping
- temporary vs permanent memory
- deterministic memory usage
- frame stability importance
- why engines use arenas and pools

Students must SEE allocation decisions affecting runtime behavior.

---

# 2. BUILD INTEGRATION REQUIREMENTS

BUILD INTEGRATION:

Labs are ALWAYS compiled into the game binary.
No conditional compilation flags needed.
The student runs `./build-dev.sh --backend=raylib -r`
and ALL scenes + ALL labs are available.

This was changed from the original design (which used #ifdef guards)
because requiring build flags to access labs was bad UX.

KEY BINDING: Press [M] to enter Memory Arena Lab. Press [Tab] to cycle modes (malloc → arena → pool → scratch). Press [1-9] to return.

---

# 3. SCENE OVERVIEW

Simulation repeatedly performs:

- spawning entities
- destroying entities
- temporary allocations
- burst creation events

Goal:
Force allocator pressure.

Recommended features:

- particles spawning in waves
- short-lived effects
- temporary query buffers
- continuous allocation churn

---

# 4. REQUIRED MEMORY MODES

User must toggle allocation strategy LIVE.

---

## MODE 1 — Standard malloc/free (Baseline)

Each entity:

- allocated individually
- freed individually

Purpose:

Expose:

- fragmentation
- allocation overhead
- frame-time spikes

Display allocation count per frame.

This mode intentionally performs poorly.

---

## MODE 2 — Linear Memory Arena

Single contiguous memory block.

Behavior:

- allocations advance pointer
- no individual frees
- reset arena at defined lifetime boundary

Teaches:

- lifetime grouping
- constant-time allocation
- predictable performance

Required feature:
arena reset button or automatic frame reset.

---

## MODE 3 — Free List / Pool Allocator

Fixed-size slots reused.

Behavior:

- entities recycled
- minimal fragmentation
- stable allocation cost

Teaches:

- reuse vs allocation
- object lifetime control

---

## MODE 4 — Temporary Scratch Arena (CRITICAL)

Introduce transient allocations:

Examples:

- collision query buffers
- sorting arrays
- temporary simulation data

Reset every frame.

Teaches:
difference between transient and persistent memory.

---

# 5. RUNTIME TOGGLE SYSTEM

User must switch allocation mode LIVE.

Requirements:

- keyboard toggle
- simulation continues running
- allocator swaps safely
- internal rebuild allowed

No restart allowed.

---

# 6. PERFORMANCE OBSERVABILITY (MANDATORY)

Display live metrics:

- allocations per frame
- frees per frame
- memory used
- peak memory usage
- frame time
- frame-time variance

Frame-time stability visualization REQUIRED.

Students must see spikes disappear when arenas are used.

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

# 7. MEMORY VISUALIZATION (VERY IMPORTANT)

Add optional debug visualization:

Examples:

Linear Arena:

- growing allocation bar

Free List:

- occupied vs free slots

malloc/free:

- fragmented block visualization

Goal:
Make invisible memory behavior visible.

---

# 8. LIFETIME DEMONSTRATION SYSTEM

Explicitly categorize memory:

- Permanent Memory
- Level Memory
- Frame Memory
- Temporary Scratch Memory

Display categories on screen.

Students must understand:

Memory organization follows lifetime, not type.

---

# 9. ALLOCATION API REQUIREMENTS

Expose simple allocation interface.

Example conceptual operations:

- allocate_permanent(size)
- allocate_level(size)
- allocate_frame(size)
- arena_reset()

Avoid complex abstractions.

Allocation intent must remain obvious.

---

# 10. DATA FLOW REQUIREMENTS

Allocators MUST:

- operate on large contiguous blocks
- avoid per-allocation OS calls in arena modes
- avoid hidden containers
- remain inspectable and debuggable

Prefer explicit pointer arithmetic where educational.

---

# 11. COURSE DOCUMENTATION UPDATES

Update the following files:

---

## A. build-large-arrays-of-things-course.md

Add module:

    Memory Arena Laboratory

Explain:

- why malloc/free is insufficient for games
- lifetime-based memory management
- relation to Handmade Hero memory model

---

## B. PLAN.md

Insert after Spatial Partition Lab.

Learning progression becomes:

1. Large Arrays
2. Particle Lab (scale)
3. Data Layout Lab (memory layout)
4. Spatial Partition Lab (algorithms)
5. Memory Arena Lab (lifetime management)

---

## C. PLAN-TRACKER.md

Add milestones:

- malloc/free baseline
- linear arena implementation
- free list allocator
- scratch arena system
- visualization overlay
- runtime switching

---

## D. GLOBAL_STYLE_GUIDE.md

Add rules:

- allocation intent must be explicit
- lifetimes prioritized over ownership abstractions
- clarity preferred over allocator cleverness
- debugging friendliness required

---

## E. README.md

Add section:

Why Game Engines Use Memory Arenas

Explain how stable frame times depend on predictable allocation.

---

# 12. LESSON SYNCHRONIZATION

Update lessons if needed:

- explain fragmentation
- introduce lifetime thinking
- explain transient vs persistent memory
- connect to future engine architecture lessons

Add diagrams for arena growth and reset behavior.

---

# 13. TRANSCRIPT RE-AUDIT TASK

Re-review:

    _ignore/Avoiding Modern C++ Anton Mikhailov.txt

Verify inclusion of:

- ownership clarity
- explicit lifetime management
- avoiding hidden allocation
- predictable performance
- simplicity in systems code

Integrate any missing insights into lessons.

---

# 14. ARCHITECTURAL CONSTRAINTS

The Memory Arena Lab MUST:

- reinforce data-oriented philosophy
- avoid complex allocator hierarchies
- remain educational first
- emphasize predictability over flexibility

Complexity must arise from lifetime reasoning.

---

# 15. COMPLETION REQUIREMENT

Do NOT complete task until:

- all allocation modes implemented
- runtime switching functional
- memory visualization present
- performance differences observable
- documentation updated
- lessons synchronized
- transcript audit completed
- course internally consistent

This is a full curriculum upgrade.

---

# END OF SPECIFICATION
