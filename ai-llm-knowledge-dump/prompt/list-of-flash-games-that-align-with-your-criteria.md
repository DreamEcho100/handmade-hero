Based on your profile from `Untitled-9`, I'll create a curated list of Flash games that align with your criteria:

1. **Clear State Machine Structure**
2. **Spatial/Mathematical Depth**
3. **Deterministic Simulation Core**
4. **Mechanical Decomposition Potential**
5. **Forces Architectural Decisions**

---

## Flash Game Recommendations

### Tier 1: High Priority (Satisfies 4-5 Criteria)

These games force the most growth and align perfectly with your trajectory.

---

#### 1. **N (N-Game / N+)**

**Developer:** Metanet Software (2004)

| Criterion           | Score | Why                                                                   |
| ------------------- | ----- | --------------------------------------------------------------------- |
| State Machine       | ★★★★★ | Player states (running, jumping, wall-sliding, dead), level states    |
| Spatial/Math        | ★★★★★ | Swept AABB collision, momentum physics, slope handling, ragdoll death |
| Deterministic       | ★★★★★ | Frame-perfect speedruns exist — fully deterministic                   |
| Decomposition       | ★★★★★ | Input → Physics → Collision → Enemies → Rendering                     |
| Forces Architecture | ★★★★★ | Naive collision breaks immediately; requires proper swept collision   |

**What it forces you to learn:**

- Swept AABB collision (tunneling prevention)
- Momentum-based movement (not instant velocity)
- Slope collision and response
- Enemy AI state machines (homing missiles, turrets, chasers)
- Tile-based level with continuous physics

**Mathematical exposure:**

- Vector projection (wall sliding)
- Line-segment intersection (laser turrets)
- Quadratic solving (homing missile prediction)

**Why it's perfect for you:** N is the gold standard for "simple concept, deep implementation." The physics feel requires precise math. Speedrunners have documented every frame of behavior.

---

#### 2. **Sugar, Sugar**

**Developer:** Bart Bonte (2010)

| Criterion           | Score | Why                                                            |
| ------------------- | ----- | -------------------------------------------------------------- |
| State Machine       | ★★★★☆ | Drawing → Simulation → Win/Lose                                |
| Spatial/Math        | ★★★★★ | Particle simulation, line intersection, gravity, collision     |
| Deterministic       | ★★★★★ | Same lines = same result every time                            |
| Decomposition       | ★★★★★ | Input → Line storage → Particle update → Collision → Rendering |
| Forces Architecture | ★★★★★ | Particle pooling, spatial queries, line-particle collision     |

**What it forces you to learn:**

- Particle system architecture
- Line-segment vs point collision
- Spatial partitioning (for many particles)
- Fixed-timestep simulation
- Drawing input to geometry conversion

**Mathematical exposure:**

- Point-to-line distance
- Line segment intersection
- Particle physics (gravity, bounce)
- Flood fill (color regions)

**Why it's perfect for you:** Pure simulation puzzle. No enemies, no combat — just physics and spatial reasoning. Forces you to think about particle systems properly.

---

#### 3. **Canabalt**

**Developer:** Adam Atomic (2009)

| Criterion           | Score | Why                                                           |
| ------------------- | ----- | ------------------------------------------------------------- |
| State Machine       | ★★★★☆ | Running → Jumping → Falling → Crashing → Dead                 |
| Spatial/Math        | ★★★★☆ | Procedural generation, AABB collision, momentum               |
| Deterministic       | ★★★★★ | Seeded RNG — same seed = same run                             |
| Decomposition       | ★★★★★ | Input → Physics → Generation → Collision → Camera → Rendering |
| Forces Architecture | ★★★★☆ | Procedural chunk generation, camera following, parallax       |

**What it forces you to learn:**

- Procedural level generation
- Camera systems (smooth follow, look-ahead)
- Parallax scrolling (multiple layers)
- Momentum-based auto-runner physics
- Obstacle spawning and pooling

**Mathematical exposure:**

- Procedural generation algorithms
- Camera interpolation (lerp, smooth damp)
- Parallax math (layer depth ratios)

**Why it's perfect for you:** Minimal input (one button), maximum systems. Forces you to build proper camera, generation, and physics systems.

---

#### 4. **VVVVVV**

**Developer:** Terry Cavanagh (2010)

| Criterion           | Score | Why                                                                  |
| ------------------- | ----- | -------------------------------------------------------------------- |
| State Machine       | ★★★★★ | Gravity states, room transitions, checkpoint system                  |
| Spatial/Math        | ★★★★☆ | Grid-based but with continuous movement, gravity flipping            |
| Deterministic       | ★★★★★ | Speedruns are frame-perfect                                          |
| Decomposition       | ★★★★★ | Input → Gravity → Movement → Collision → Room transition → Rendering |
| Forces Architecture | ★★★★★ | Room-based level loading, checkpoint system, screen wrapping         |

**What it forces you to learn:**

- Gravity inversion mechanics
- Room/screen transition systems
- Checkpoint and respawn architecture
- Moving platform synchronization
- Screen-wrap collision

**Mathematical exposure:**

- Coordinate system flipping
- Modular arithmetic (screen wrapping)
- Platform-relative movement

**Why it's perfect for you:** Simple mechanic (flip gravity), complex implications. Forces clean state management for gravity direction, room transitions, and death/respawn.

---

#### 5. **World's Hardest Game**

**Developer:** Stephen Critoph (2008)

| Criterion           | Score | Why                                                            |
| ------------------- | ----- | -------------------------------------------------------------- |
| State Machine       | ★★★★☆ | Playing → Dead → Respawn, level progression                    |
| Spatial/Math        | ★★★★★ | Circle-rectangle collision, sine wave patterns, precise timing |
| Deterministic       | ★★★★★ | Patterns are 100% deterministic                                |
| Decomposition       | ★★★★★ | Input → Movement → Enemy patterns → Collision → Rendering      |
| Forces Architecture | ★★★★☆ | Enemy pattern system, precise collision, level state           |

**What it forces you to learn:**

- Circle vs rectangle collision
- Parametric enemy movement (sine waves, circles, lines)
- Precise hitbox management
- Pattern timing systems
- Death counter / statistics tracking

**Mathematical exposure:**

- Trigonometric movement patterns
- Circle-AABB intersection
- Parametric curves

**Why it's perfect for you:** Pure collision and timing. No combat system, no complex AI — just precise movement and collision. Great for drilling collision math.

---

#### 6. **Bloons Tower Defense (1 or 3)**

**Developer:** Ninja Kiwi (2007)

| Criterion           | Score | Why                                                             |
| ------------------- | ----- | --------------------------------------------------------------- |
| State Machine       | ★★★★★ | Build phase → Wave phase → Win/Lose, tower states, bloon states |
| Spatial/Math        | ★★★★★ | Path following, range detection, projectile targeting, AOE      |
| Deterministic       | ★★★★★ | Same placement = same result                                    |
| Decomposition       | ★★★★★ | Input → Placement → Targeting → Projectiles → Bloons → Economy  |
| Forces Architecture | ★★★★★ | Entity pooling, spatial queries, targeting priorities           |

**What it forces you to learn:**

- Path-following AI (spline or waypoint)
- Range queries (circle intersection)
- Targeting priority systems
- Projectile prediction (leading targets)
- Wave spawning systems
- Economy/upgrade systems

**Mathematical exposure:**

- Distance calculations (many entities)
- Projectile intercept math
- AOE (area of effect) calculations
- Spatial partitioning for range queries

**Why it's perfect for you:** Forces you to handle many entities efficiently. Naive O(n²) targeting breaks immediately. Requires proper spatial partitioning.

---

#### 7. **Fancy Pants Adventure**

**Developer:** Brad Borne (2006)

| Criterion           | Score | Why                                                             |
| ------------------- | ----- | --------------------------------------------------------------- |
| State Machine       | ★★★★★ | Complex player states (running, sliding, wall-jumping, rolling) |
| Spatial/Math        | ★★★★★ | Momentum physics, slope handling, curved surfaces               |
| Deterministic       | ★★★★☆ | Mostly deterministic (some enemy RNG)                           |
| Decomposition       | ★★★★★ | Input → Physics → Collision → Animation → Camera → Rendering    |
| Forces Architecture | ★★★★★ | Slope collision is notoriously hard to get right                |

**What it forces you to learn:**

- Momentum-based platformer physics
- Slope collision and response
- Wall jumping mechanics
- Animation state machine (many states)
- Curved surface collision (advanced)

**Mathematical exposure:**

- Surface normal calculation
- Slope angle → velocity conversion
- Momentum conservation
- Bezier curves (level geometry)

**Why it's perfect for you:** The "feel" of Fancy Pants is legendary. Recreating it forces you to understand momentum physics deeply. Slopes alone will teach you more than most games.

---

#### 8. **Portal: The Flash Version**

**Developer:** We Create Stuff (2007)

| Criterion           | Score | Why                                                             |
| ------------------- | ----- | --------------------------------------------------------------- |
| State Machine       | ★★★★☆ | Player states, portal states, object states                     |
| Spatial/Math        | ★★★★★ | Portal coordinate transforms, raycasting, momentum preservation |
| Deterministic       | ★★★★★ | Puzzle game — must be deterministic                             |
| Decomposition       | ★★★★★ | Input → Physics → Portal logic → Collision → Rendering          |
| Forces Architecture | ★★★★★ | Portal math is non-trivial; forces proper transform thinking    |

**What it forces you to learn:**

- Coordinate system transforms (portal entry → exit)
- Raycasting (portal gun)
- Momentum preservation through portals
- Recursive rendering (portal in portal)
- Physics object management

**Mathematical exposure:**

- Matrix transforms (rotation, translation)
- Ray-plane intersection
- Velocity vector transformation
- Coordinate space conversion

**Why it's perfect for you:** You mentioned Portal specifically because it forces spatial transforms and topology thinking. This is exactly what you need for your mathematical gaps.

---

#### 9. **Meat Boy (Flash version)**

**Developer:** Edmund McMillen (2008)

| Criterion           | Score | Why                                                               |
| ------------------- | ----- | ----------------------------------------------------------------- |
| State Machine       | ★★★★★ | Running, jumping, wall-sliding, dead, replay system               |
| Spatial/Math        | ★★★★★ | Tight platformer physics, wall jumping, sawblade collision        |
| Deterministic       | ★★★★★ | Replay system proves determinism                                  |
| Decomposition       | ★★★★★ | Input → Physics → Collision → Death → Replay → Rendering          |
| Forces Architecture | ★★★★★ | Replay system requires input recording and deterministic playback |

**What it forces you to learn:**

- Tight platformer physics (instant response)
- Wall sliding and jumping
- Replay system architecture
- Circular sawblade collision
- Death and instant respawn

**Mathematical exposure:**

- Circle collision (sawblades)
- Wall normal detection
- Input recording/playback

**Why it's perfect for you:** The replay system alone is worth studying. Forces you to think about determinism and input recording.

---

#### 10. **Gravitee / Gravitee Wars**

**Developer:** FunkyPear (2008)

| Criterion           | Score | Why                                                                   |
| ------------------- | ----- | --------------------------------------------------------------------- |
| State Machine       | ★★★★☆ | Aiming → Firing → Simulation → Result                                 |
| Spatial/Math        | ★★★★★ | N-body gravity simulation, trajectory prediction, orbital mechanics   |
| Deterministic       | ★★★★★ | Same shot = same result                                               |
| Decomposition       | ★★★★★ | Input → Trajectory preview → Physics simulation → Collision → Scoring |
| Forces Architecture | ★★★★☆ | Gravity calculation for multiple bodies                               |

**What it forces you to learn:**

- N-body gravitational simulation
- Trajectory prediction (drawing the arc)
- Orbital mechanics basics
- Turn-based game flow
- Aiming UI (angle + power)

**Mathematical exposure:**

- Gravitational force: F = G(m1\*m2)/r²
- Vector summation (multiple gravity sources)
- Numerical integration (Euler, RK4)
- Trajectory prediction

**Why it's perfect for you:** Pure physics simulation. Forces you to implement real gravitational math, not fake "gravity = constant downward."

---

### Tier 2: Strong Candidates (Satisfies 3-4 Criteria)

These are excellent but slightly less forcing than Tier 1.

| Game                      | State Machine | Spatial/Math | Deterministic | Decomposition | Forces Architecture |
| ------------------------- | ------------- | ------------ | ------------- | ------------- | ------------------- |
| **Bubble Tanks**          | ★★★★☆         | ★★★★☆        | ★★★★☆         | ★★★★☆         | ★★★☆☆               |
| **The Impossible Quiz**   | ★★★☆☆         | ★★☆☆☆        | ★★★★★         | ★★★☆☆         | ★★☆☆☆               |
| **Learn to Fly**          | ★★★★☆         | ★★★★☆        | ★★★★★         | ★★★★☆         | ★★★☆☆               |
| **Bowman**                | ★★★☆☆         | ★★★★★        | ★★★★★         | ★★★★☆         | ★★★☆☆               |
| **Bloxorz**               | ★★★★☆         | ★★★★☆        | ★★★★★         | ★★★★☆         | ★★★★☆               |
| **Cursor\*10**            | ★★★★★         | ★★★☆☆        | ★★★★★         | ★★★★☆         | ★★★★☆               |
| **Line Rider**            | ★★★☆☆         | ★★★★★        | ★★★★★         | ★★★★☆         | ★★★★☆               |
| **Pandemic 2**            | ★★★★★         | ★★★☆☆        | ★★★★☆         | ★★★★★         | ★★★★☆               |
| **Desktop Tower Defense** | ★★★★★         | ★★★★☆        | ★★★★★         | ★★★★★         | ★★★★☆               |
| **Crush the Castle**      | ★★★☆☆         | ★★★★★        | ★★★★★         | ★★★★☆         | ★★★★☆               |

---

### Tier 3: Supplementary (Satisfies 2-3 Criteria)

Good for specific skill drilling, not full course material.

| Game                    | Best For                                 |
| ----------------------- | ---------------------------------------- |
| **Helicopter Game**     | Simple physics, procedural generation    |
| **Stick RPG**           | State management, economy systems        |
| **Interactive Buddy**   | Physics sandbox, ragdoll basics          |
| **Madness Interactive** | Combat systems, entity management        |
| **Thing Thing**         | Shooter mechanics, weapon systems        |
| **Electric Box**        | Puzzle logic, signal propagation         |
| **Grow series**         | State machines, combinatorial logic      |
| **Motherload**          | Resource management, terrain destruction |

---

## Recommended Learning Sequence

Based on your trajectory (Snake → Asteroids → Frogger → Flash games), here's the optimal order:

### Phase 1: Core Physics Mastery

| Order | Game                     | Primary Learning                                |
| ----- | ------------------------ | ----------------------------------------------- |
| 1     | **World's Hardest Game** | Precise collision, parametric patterns          |
| 2     | **Canabalt**             | Momentum physics, procedural generation, camera |
| 3     | **N (N-Game)**           | Swept collision, slopes, momentum               |

### Phase 2: Simulation & Particles

| Order | Game             | Primary Learning                        |
| ----- | ---------------- | --------------------------------------- |
| 4     | **Sugar, Sugar** | Particle systems, line collision        |
| 5     | **Gravitee**     | N-body physics, trajectory prediction   |
| 6     | **Line Rider**   | Physics simulation, user-drawn geometry |

### Phase 3: Complex State & Systems

| Order | Game            | Primary Learning                |
| ----- | --------------- | ------------------------------- |
| 7     | **VVVVVV**      | Gravity mechanics, room systems |
| 8     | **Meat Boy**    | Tight controls, replay system   |
| 9     | **Fancy Pants** | Advanced slopes, momentum feel  |

### Phase 4: Entity Management & AI

| Order | Game           | Primary Learning                          |
| ----- | -------------- | ----------------------------------------- |
| 10    | **Bloons TD**  | Many entities, spatial queries, targeting |
| 11    | **Desktop TD** | Pathfinding, tower placement              |

### Phase 5: Advanced Spatial Reasoning

| Order | Game             | Primary Learning                |
| ----- | ---------------- | ------------------------------- |
| 12    | **Portal Flash** | Coordinate transforms, topology |
| 13    | **Bloxorz**      | 3D-in-2D, rotation states       |

---

## Quick Selection Matrix

**"I want to learn X, which game?"**

| I Want To Learn       | Best Game               | Why                     |
| --------------------- | ----------------------- | ----------------------- |
| Swept collision       | N                       | Requires it or breaks   |
| Particle systems      | Sugar, Sugar            | Core mechanic           |
| Momentum physics      | Fancy Pants, Canabalt   | Defines the feel        |
| Procedural generation | Canabalt                | Infinite runner         |
| Gravity simulation    | Gravitee                | N-body physics          |
| Slope collision       | N, Fancy Pants          | Central mechanic        |
| State machines        | VVVVVV, Meat Boy        | Complex player states   |
| Entity pooling        | Bloons TD               | Many entities           |
| Spatial partitioning  | Bloons TD, Sugar Sugar  | Performance requirement |
| Replay systems        | Meat Boy                | Built-in feature        |
| Coordinate transforms | Portal Flash            | Portal mechanics        |
| Pathfinding           | Bloons TD, Desktop TD   | Enemy movement          |
| Camera systems        | Canabalt, Fancy Pants   | Smooth follow           |
| Trajectory prediction | Gravitee, Bowman        | Aiming systems          |
| Line intersection     | Sugar Sugar, Line Rider | Drawing mechanics       |

---

## Games to AVOID (Per Your Criteria)

These fail your filters:

| Game                      | Why Avoid                               |
| ------------------------- | --------------------------------------- |
| **Farmville-style**       | No simulation, just timers and clicking |
| **Dress-up games**        | Pure UI, no systems                     |
| **Quiz games**            | No spatial reasoning                    |
| **Idle/clicker games**    | No real-time simulation                 |
| **Pure narrative games**  | No mechanical depth                     |
| **Multiplayer-dependent** | Can't recreate without server           |
| **Heavy RNG games**       | Non-deterministic core                  |

---

## Final Recommendation

**Start with these 5 in order:**

1. **World's Hardest Game** — Drill collision math
2. **Canabalt** — Build camera + procedural systems
3. **N** — Master swept collision and momentum
4. **Sugar, Sugar** — Particle systems and line math
5. **Portal Flash** — Coordinate transforms (your stated weakness)

This sequence builds:

- Collision → Physics → Particles → Transforms

Each game forces the next level of mathematical and architectural sophistication.

---

Would you like me to:

1. **Create a detailed PLAN.md** for any of these games using the reverse engineering template?
2. **Expand on the mathematical requirements** for a specific game?
3. **Map out the full lesson sequence** for one of these as a course?
