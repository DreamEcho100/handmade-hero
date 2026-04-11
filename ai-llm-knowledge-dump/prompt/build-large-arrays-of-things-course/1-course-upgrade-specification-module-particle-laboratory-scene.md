# COURSE UPGRADE SPECIFICATION

## Module: Particle Laboratory Scene

---

# 0. PURPOSE

Introduce an **Advanced Optional Scene** called:

> Particle Laboratory

This scene exists to stress-test and visually demonstrate the principles taught in the **Build Large Arrays of Things** course.

This is NOT a gameplay feature.
This is NOT a replacement scene.

It is a **Learning Laboratory** designed to make data-oriented design,
entity scaling, archetype mutation, and performance behavior observable.

The original scene MUST remain unchanged.

---

# 1. HIGH-LEVEL EDUCATIONAL GOALS

The Particle Laboratory MUST teach:

- Large-scale entity iteration
- Behavior defined as DATA (not objects)
- Archetype switching via data mutation
- Emergent behavior from simple rules
- Performance scaling visibility
- Cache and branching intuition
- Simulation complexity growth

Students must _feel_ why Large Arrays Of Things matter.

---

# 2. BUILD INTEGRATION REQUIREMENTS

BUILD INTEGRATION:

Labs are ALWAYS compiled into the game binary.
No conditional compilation flags needed.
The student runs `./build-dev.sh --backend=raylib -r`
and ALL scenes + ALL labs are available.

This was changed from the original design (which used #ifdef guards)
because requiring build flags to access labs was bad UX.

KEY BINDING: Press [P] to enter Particle Lab. Press [Tab] to cycle modes. Press [1-9] to return to lesson scenes.

---

# 3. SCENE OVERVIEW

The Particle Laboratory contains:

### Player

- Rectangle controlled via keyboard
- Constant movement speed
- Cannot outrun all particles
- Serves as interaction anchor

### Particle System

Large pool of particles:

- Minimum: 1000 particles
- Scalable to stress performance
- Stored in contiguous arrays
- NO per-entity allocation allowed

Particles demonstrate different behavioral archetypes.

---

# 4. PARTICLE DATA MODEL (MANDATORY)

Particles MUST be pure data.

Required fields:

- position
- velocity
- archetype_id
- aggressiveness
- prediction_error
- value_score
- active flag

ABSOLUTELY FORBIDDEN:

- virtual functions
- inheritance
- heap allocation per particle
- polymorphic objects

Behavior is selected by archetype_id only.

---

# 5. REQUIRED ARCHETYPES

Implement AT LEAST six archetypes:

1. Bounce
   - simple reflection movement

2. Wander
   - random directional drift

3. Seek Player
   - moves toward player position

4. Predictive Seek
   - predicts future player position

5. Orbit
   - circular motion around player

6. Drift Noise
   - noisy velocity variation

Archetypes differ ONLY by update logic.

---

# 6. PREDICTION SYSTEM

Predictive particles estimate:

```
future_position = player_pos + player_velocity * prediction_factor
```

Add controlled imperfection:

- each particle has prediction_error
- introduce randomized offset
- behavior remains imperfect and dynamic

Goal:
Demonstrate data-driven intelligence without complexity explosion.

---

# 7. ARCHETYPE MUTATION RULES (CRITICAL)

On collision with player:

Particle MAY:

- change archetype_id
- modify aggressiveness
- modify prediction_error
- change value_score

Mutation MUST be implemented as:

DATA REWRITE ONLY.

Forbidden:

- spawning new objects
- deleting entities
- reallocating memory

This demonstrates:
→ mutation over inheritance.

---

# 8. PLAYER INTERACTION

Player abilities:

### Dodge

Movement avoids incoming particles.

### Collect

Collision collects particles.

Effects:

- score increase
- optional temporary modifiers

Collected particles are recycled inside the existing array.

NO allocation allowed.

---

# 9. PERFORMANCE OBSERVABILITY (MANDATORY)

Scene MUST expose live metrics:

- entity count
- frame time
- update time
- collision checks
- archetype distribution

Students must visually connect:
behavior complexity ↔ performance cost.

Optional debug overlays encouraged.

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

# 10. UPDATE PIPELINE REQUIREMENTS

All particles updated using:

```
for (i = 0; i < particle_count; ++i)
```

No containers.
No hidden iterators.

The loop must remain explicit for teaching clarity.

---

# 11. COURSE DOCUMENTATION UPDATES

Update the following files:

---

## A. build-large-arrays-of-things-course.md

Add new module:

> Advanced Module — Particle Laboratory

Explain:

- purpose
- learning outcomes
- why complexity is introduced later

---

## B. PLAN.md

Insert Particle Laboratory after core iteration lessons.

Mark as:

```
Advanced Visualization Module
```

---

## C. PLAN-TRACKER.md

Add milestones:

- Particle data model
- Archetype system
- Mutation system
- Prediction system
- Performance overlay
- Final lab integration

---

## D. GLOBAL_STYLE_GUIDE.md

Add rules:

- behavior expressed as data
- avoid abstraction layers
- prioritize iteration clarity
- mutation preferred over hierarchy

---

## E. README.md

Explain why the Particle Laboratory exists:

It transforms theory into observable system behavior.

---

# 12. LESSON + CODE SYNCHRONIZATION

If Particle Laboratory introduces new concepts:

You MUST:

- update affected lessons
- update examples
- ensure terminology consistency
- add explanatory diagrams where required

Course and code must remain aligned.

---

# 13. TRANSCRIPT RE-AUDIT TASK

Re-audit:

```
_ignore/Avoiding Modern C++ Anton Mikhailov.txt
```

Verify inclusion of:

- data-oriented iteration
- simplicity over abstraction
- ownership clarity
- mutation vs inheritance
- debugging advantages of simple data
- compile-time vs runtime reasoning
- avoiding unnecessary indirection

If any concept is missing:

Update lessons and prompts accordingly.

---

# 14. ARCHITECTURAL CONSTRAINTS

Particle Laboratory MUST:

- extend existing architecture
- not fork the engine design
- reinforce course philosophy
- remain understandable step-by-step

Complexity must emerge gradually.

---

# 15. COMPLETION REQUIREMENT

DO NOT finish until:

- all listed files updated
- lessons synchronized
- code updated
- transcript audit completed
- Particle Laboratory integrated cleanly
- course remains internally consistent

This is a full course upgrade, not a partial edit.

---

# END OF SPECIFICATION
