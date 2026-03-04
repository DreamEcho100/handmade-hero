# Flash Game Reverse Engineering & Planning Prompt

You are tasked with researching and constructing a comprehensive PLAN.md for reconstructing a legacy Flash game without access to its source code.

This document is used **alongside `course-builder.md`** — all architectural decisions must align with that document's patterns (Y-up coordinates, world units in meters, `GAME_PHASE` state machine, backbuffer rendering, etc.).

The output must be rigorous, implementation-oriented, and suitable for a low-level, data-oriented development approach (C-style architecture).

---

## 1. Research & Discovery Phase

### 1.1 Game Identification

- **Original title** (including regional variants)
- **Developer/Publisher** and release year
- **Platform history** (Newgrounds, Kongregate, Miniclip, Armor Games, etc.)
- **Sequels or spin-offs** (may reveal evolved mechanics)
- **Spiritual successors** (modern games inspired by it)

### 1.2 Source Material Collection

| Source Type       | Where to Find                  | What to Extract                           |
| ----------------- | ------------------------------ | ----------------------------------------- |
| Gameplay videos   | YouTube, archive.org           | Timing, mechanics, edge cases             |
| Let's Plays       | YouTube                        | Player strategies reveal hidden mechanics |
| Speedruns         | speedrun.com, YouTube          | Exploits reveal physics edge cases        |
| Original SWF      | Flashpoint, archive.org        | Asset extraction (if legal)               |
| Screenshots       | MobyGames, GameFAQs            | UI layout, color palette                  |
| Forum discussions | Original portal forums, Reddit | Undocumented mechanics                    |
| Walkthroughs      | GameFAQs, IGN                  | Level layouts, enemy patterns             |
| Reviews           | Metacritic, original portal    | Feature descriptions                      |

### 1.3 Community Knowledge Mining

Search for:

- "How does [mechanic] work in [game]?"
- "[Game] physics explained"
- "[Game] speedrun techniques"
- "[Game] hidden mechanics"
- "[Game] source code" (occasionally available)
- "[Game] remake" (learn from others' attempts)

### 1.4 Legal Considerations

Document clearly:

- [ ] Is the original game abandonware?
- [ ] Are assets being recreated or extracted?
- [ ] Is this for educational purposes only?
- [ ] Are there trademark concerns with the name?

**Recommendation:** Always recreate assets procedurally rather than extracting from SWF files.

---

## 2. Mechanics Extraction from Footage

### 2.1 Frame-by-Frame Analysis Setup

**Tools needed:**

- Video player with frame advance (VLC: press `E` to advance frame)
- Screen recording at 60 FPS (captures sub-frame timing even for 30 FPS games)
- Spreadsheet for timing data
- Image editor for pixel measurements (GIMP, Photoshop)

**Key technique:** Download highest quality footage available. YouTube's 60 FPS videos are ideal.

### 2.2 Determining Original Frame Rate

Flash games typically run at **30 FPS** (33.33ms per frame). Some run at 24 FPS or 60 FPS.

**Method:**

1. Find a known-duration event (1-second timer countdown, animation cycle)
2. Count frames in your 60 FPS recording
3. `Original FPS = counted_frames / 2` (if recording at 60 FPS)

**Common Flash frame rates:**
| FPS | Frame Duration | Common In |
|-----|----------------|-----------|
| 24 | 41.67ms | Cinematic games |
| 30 | 33.33ms | Most Flash games (default) |
| 60 | 16.67ms | Action games (rare) |

### 2.3 Timing Data Extraction

**Create a timing table for all observable events:**

```markdown
## Timing Data (at [X] FPS)

| Event               | Frames | Seconds | Notes                  |
| ------------------- | ------ | ------- | ---------------------- |
| Jump rise           |        |         | Apex frame number      |
| Jump fall           |        |         | Often faster than rise |
| Total jump          |        |         | Rise + fall            |
| Walk cycle          |        |         | Full animation loop    |
| Attack windup       |        |         | Vulnerable period      |
| Attack active       |        |         | Hitbox active          |
| Attack recovery     |        |         | Can cancel? When?      |
| Hurt stun           |        |         | Invincibility start    |
| Invincibility       |        |         | Flashing duration      |
| Enemy patrol cycle  |        |         | Full left-right-left   |
| Projectile lifetime |        |         | Until despawn          |
| Respawn delay       |        |         | Death to control       |
```

### 2.4 Physics Parameter Derivation

**From jump observation, derive gravity and initial velocity:**

```
Given (measured from footage):
- Jump height (h) in pixels
- Time to apex (t) in seconds (frames / FPS)

Derive:
- Gravity (g) = 2h / t²
- Jump velocity (v₀) = g × t

Example:
- Jump height = 96 pixels
- Time to apex = 18 frames at 30 FPS = 0.6 seconds
- Scale = 32 pixels/meter (estimated)
- Height in meters = 96 / 32 = 3.0 meters
- Gravity = 2 × 3.0 / 0.6² = 16.67 m/s²
- Jump velocity = 16.67 × 0.6 = 10.0 m/s
```

**Movement speed derivation:**

```
Given:
- Distance traveled in pixels over N frames
- Scale (pixels per meter)

Derive:
- Speed = (pixels / scale) / (frames / FPS)

Example:
- Player moves 64 pixels in 30 frames (1 second)
- Scale = 32 pixels/meter
- Speed = (64 / 32) / 1.0 = 2.0 m/s
```

### 2.5 Scale Estimation (PIXELS_PER_METER)

**Method 1: Character height**

- Assume humanoid ≈ 1.7 meters
- Measure character sprite height in pixels
- `Scale = sprite_height / 1.7`

**Method 2: Tile size**

- Many Flash games use 32×32 or 16×16 tiles
- Assume 1 tile = 1 meter (common convention)
- `Scale = tile_size`

**Method 3: Jump height reasonableness**

- Measure jump height in pixels
- Estimate reasonable real-world height (2-4 meters typical)
- `Scale = jump_pixels / estimated_meters`

**Verification:** Cross-check multiple methods. If character is 54px tall and tiles are 32px, then:

- Character = 54/32 = 1.69 tiles ≈ 1.7 meters ✓

**Document your scale:**

```markdown
## Scale Determination

| Method           | Measurement              | Result  |
| ---------------- | ------------------------ | ------- |
| Character height | 54 pixels / 1.7m assumed | 32 px/m |
| Tile size        | 32×32 pixel tiles        | 32 px/m |
| Jump height      | 96 pixels / 3m estimated | 32 px/m |

**PIXELS_PER_METER: 32** (all methods agree)
```

### 2.6 Input Behavior Analysis

**Observe and document these input characteristics:**

| Behavior          | How to Test                | What to Record                   |
| ----------------- | -------------------------- | -------------------------------- |
| Jump buffering    | Press jump before landing  | Buffer window in frames          |
| Coyote time       | Walk off ledge, then jump  | Grace period in frames           |
| Variable jump     | Tap vs hold jump           | Height difference, cut-off frame |
| Air control       | Change direction mid-air   | Acceleration, max speed change   |
| Attack canceling  | Press attack during attack | Can cancel? Into what?           |
| Dash cooldown     | Spam dash                  | Cooldown in frames               |
| Wall interactions | Jump at walls              | Slide? Wall jump? Cling?         |

**Input timing table:**

```markdown
## Input Timing

| Mechanic           | Frames | Behavior                                       |
| ------------------ | ------ | ---------------------------------------------- |
| Jump buffer        |        | Frames before landing input is remembered      |
| Coyote time        |        | Frames after leaving ground you can still jump |
| Jump cut threshold |        | Release before this frame = short jump         |
| Attack buffer      |        | Can buffer next attack during recovery?        |
| Dash cooldown      |        | Frames between dashes                          |
```

---

## 3. State Machine Mapping

### 3.1 Game Phase Identification

**Watch for screen transitions and map to `GAME_PHASE` enum (per `course-builder.md`):**

| Observed Screen/Behavior | GAME_PHASE       | Entry Trigger     | Exit Triggers           |
| ------------------------ | ---------------- | ----------------- | ----------------------- |
| Title screen             | MENU             | Game start        | Play clicked            |
| Active gameplay          | PLAYING          | From menu/unpause | Pause, death, level end |
| Pause overlay            | PAUSED           | Pause key         | Unpause, quit           |
| Death screen             | GAME_OVER        | Player death      | Retry, menu             |
| Level complete           | LEVEL_TRANSITION | Goal reached      | Next level starts       |
| Cutscene                 | CUTSCENE         | Triggered event   | Skip/complete           |

### 3.2 Player State Identification

**Observe player animations and behaviors:**

| Visual/Behavior    | State Name | Entry Conditions          | Exit Conditions          |
| ------------------ | ---------- | ------------------------- | ------------------------ |
| Standing still     | IDLE       | No input, on ground       | Move, jump, attack, hurt |
| Walking animation  | WALKING    | Move input, on ground     | Stop, jump, attack, hurt |
| Rising in air      | JUMPING    | Just jumped               | Apex reached, hurt       |
| Falling in air     | FALLING    | Apex passed or walked off | Land, hurt               |
| Attack animation   | ATTACKING  | Attack input              | Animation complete, hurt |
| Flashing/knockback | HURT       | Took damage               | Timer expires, death     |
| Death animation    | DEAD       | Health ≤ 0                | Triggers GAME_OVER       |

### 3.3 Enemy State Identification

**For each enemy type, document:**

```markdown
## Enemy: [Name]

### States

| State  | Behavior           | Duration         | Transitions            |
| ------ | ------------------ | ---------------- | ---------------------- |
| IDLE   | Standing still     | Until triggered  | → PATROL, CHASE        |
| PATROL | Walk left-right    | Continuous       | → CHASE (player seen)  |
| CHASE  | Move toward player | Until lost       | → PATROL (player lost) |
| ATTACK | Attack animation   | Animation length | → IDLE                 |
| HURT   | Stun/flash         | X frames         | → Previous state       |
| DEAD   | Death animation    | Animation length | Despawn                |

### Triggers

- Sees player: Within X meters, line of sight
- Loses player: Out of range for Y frames
- Attack range: Within Z meters
```

---

## 4. Entity Catalog

### 4.1 Player Entity

```markdown
## Player

### Dimensions

- Sprite size: [W] × [H] pixels
- Hitbox size: [W] × [H] pixels (often smaller than sprite)
- Hitbox offset: [X, Y] from sprite origin

### Abilities

| Ability     | Input        | Behavior                        |
| ----------- | ------------ | ------------------------------- |
| Walk        | ←/→          | Speed: X m/s                    |
| Jump        | Space/Z      | Height: X m, Duration: X s      |
| Double jump | Space in air | (if applicable)                 |
| Attack      | X/Ctrl       | Hitbox: X×Y, Active frames: X-Y |
| Dash        | Shift        | Distance: X m, Cooldown: X s    |

### Health System

- Max health: X
- Invincibility after hit: X frames
- Knockback: X m/s for Y frames
```

### 4.2 Enemy Catalog

**For each enemy type:**

```markdown
## Enemy: [Name]

### Dimensions

- Sprite: [W] × [H] pixels → [W] × [H] meters
- Hitbox: [W] × [H] pixels (if different)

### Behavior

- AI type: Patrol / Chase / Stationary / Flying
- Patrol range: X meters
- Detection range: X meters
- Movement speed: X m/s
- Attack pattern: [Description]

### Combat

- Health: X hits
- Damage to player: X
- Attack hitbox: [W] × [H], offset [X, Y]
- Attack timing: Windup X frames, Active Y frames

### Drops

- Score: X points
- Items: [List]
```

### 4.3 Projectile Catalog

```markdown
## Projectiles

| Name           | Size (px) | Size (m)  | Speed (m/s) | Lifetime (s) | Damage | Behavior            |
| -------------- | --------- | --------- | ----------- | ------------ | ------ | ------------------- |
| Player arrow   | 16×4      | 0.5×0.125 | 10.0        | 2.0          | 1      | Straight line       |
| Enemy fireball | 16×16     | 0.5×0.5   | 5.0         | 3.0          | 1      | Straight / homing   |
| Bomb           | 24×24     | 0.75×0.75 | Arc         | Until ground | 2      | Parabolic, explodes |
```

### 4.4 Pickup Catalog

```markdown
## Pickups

| Name    | Size (px) | Effect      | Respawns | Animation           |
| ------- | --------- | ----------- | -------- | ------------------- |
| Coin    | 16×16     | +10 score   | No       | 4 frames, 0.3s loop |
| Health  | 16×16     | +1 HP       | No       | Bobbing             |
| Key     | 16×24     | Opens doors | No       | Sparkle             |
| Powerup | 24×24     | [Effect]    | No       | Glow pulse          |
```

### 4.5 Environment Objects

```markdown
## Environment

| Object           | Size        | Behavior                  | Collision                         |
| ---------------- | ----------- | ------------------------- | --------------------------------- |
| Solid tile       | 32×32       | Static                    | Blocking                          |
| One-way platform | 32×16       | Static                    | Solid from above only             |
| Moving platform  | 64×16       | Horizontal/vertical cycle | Carries player                    |
| Spikes           | 32×16       | Static                    | Instant death                     |
| Spring           | 32×32       | Static                    | Launches player (velocity: X m/s) |
| Door             | 32×64       | Opens with key            | Blocking until opened             |
| Checkpoint       | 32×48       | Sets respawn              | Trigger (no physics)              |
| Ladder           | 32×variable | Enables climbing          | Trigger (climb state)             |
```

---

## 5. Collision Analysis

### 5.1 Collision Pairs

**Document what collides with what:**

```markdown
## Collision Matrix

| A                 | B                | Result                                    |
| ----------------- | ---------------- | ----------------------------------------- |
| Player            | Solid tile       | Block movement                            |
| Player            | One-way platform | Block if moving down, pass if moving up   |
| Player            | Enemy body       | Player takes damage, knockback            |
| Player            | Enemy projectile | Player takes damage, projectile destroyed |
| Player            | Pickup           | Collect, pickup destroyed                 |
| Player attack     | Enemy            | Enemy takes damage                        |
| Player projectile | Enemy            | Enemy takes damage, projectile destroyed  |
| Player projectile | Solid tile       | Projectile destroyed                      |
| Enemy             | Solid tile       | Block movement (AI turns around)          |
| Enemy             | Ledge            | AI turns around (if not flying)           |
```

### 5.2 Hitbox Details

**Document hitbox specifics:**

```markdown
## Hitbox Analysis

### Player Hitbox

- Sprite size: 32×54 pixels
- Collision hitbox: 24×48 pixels (smaller for fairness)
- Hitbox offset: 4 pixels from left, 6 from top

### Player Attack Hitbox

- Size: 32×32 pixels
- Offset: 24 pixels in facing direction, centered vertically
- Active frames: 7-10 (of 18 total attack frames)

### Enemy Hitboxes

| Enemy  | Body Hitbox | Attack Hitbox         | Attack Frames |
| ------ | ----------- | --------------------- | ------------- |
| Slime  | Full sprite | None (contact damage) | N/A           |
| Knight | 28×44       | 32×48, offset 20px    | 12-15         |
```

---

## 6. Asset Recreation Plan

### 6.1 Visual Asset Strategy

**Choose one approach:**

| Approach             | Pros                     | Cons           | Best For           |
| -------------------- | ------------------------ | -------------- | ------------------ |
| Procedural shapes    | Fast, legal, educational | Less authentic | Teaching courses   |
| Pixel art recreation | Authentic feel           | Time-consuming | Portfolio projects |
| Asset extraction     | Exact match              | Legal concerns | Personal use only  |

**Recommendation for courses:** Procedural shapes with matching proportions and colors.

### 6.2 Color Palette Extraction

**From screenshots, extract key colors:**

```markdown
## Color Palette

| Usage                | Hex     | RGB           | Sample |
| -------------------- | ------- | ------------- | ------ |
| Background (sky)     | #87CEEB | 135, 206, 235 | 🟦     |
| Ground (dirt)        | #8B4513 | 139, 69, 19   | 🟫     |
| Grass                | #228B22 | 34, 139, 34   | 🟩     |
| Player primary       | #4169E1 | 65, 105, 225  | 🔵     |
| Player secondary     | #FFD700 | 255, 215, 0   | 🟡     |
| Enemy (common)       | #32CD32 | 50, 205, 50   | 🟢     |
| Danger (spikes/lava) | #FF0000 | 255, 0, 0     | 🔴     |
| UI background        | #000000 | 0, 0, 0       | ⬛     |
| UI text              | #FFFFFF | 255, 255, 255 | ⬜     |
```

### 6.3 Animation Frame Counts

```markdown
## Animation Data

### Player

| Animation   | Frames | Duration  | Looping |
| ----------- | ------ | --------- | ------- |
| Idle        | 4      | 0.5s      | Yes     |
| Walk        | 6      | 0.4s      | Yes     |
| Jump (rise) | 2      | Hold last | No      |
| Jump (fall) | 2      | Hold last | No      |
| Attack      | 6      | 0.3s      | No      |
| Hurt        | 2      | 0.2s      | No      |
| Death       | 8      | 0.8s      | No      |

### Enemies

| Enemy | Animation | Frames | Duration |
| ----- | --------- | ------ | -------- |
| Slime | Idle      | 2      | 0.5s     |
| Slime | Move      | 4      | 0.4s     |
| Slime | Death     | 4      | 0.3s     |
```

### 6.4 Sound Design Plan

**Map game events to procedural sounds (implementation per `course-builder.md` audio section):**

```markdown
## Sound Effects

| Event       | Type         | Frequency Range  | Duration | Character         |
| ----------- | ------------ | ---------------- | -------- | ----------------- |
| Jump        | Rising sweep | 200→400 Hz       | 100ms    | Quick, light      |
| Land        | Thud         | 80→60 Hz         | 80ms     | Low impact        |
| Attack      | Swoosh       | 300→100 Hz       | 150ms    | Sharp, descending |
| Enemy hit   | Impact       | 400→200 Hz       | 100ms    | Satisfying        |
| Enemy death | Pop          | 600→100 Hz       | 200ms    | Descending sweep  |
| Player hurt | Buzz         | 150 Hz flat      | 200ms    | Harsh             |
| Coin        | Ding         | 800→1000 Hz      | 150ms    | Pleasant, rising  |
| Powerup     | Arpeggio     | 400→600→800 Hz   | 400ms    | Triumphant        |
| Checkpoint  | Chime        | 500, 600, 800 Hz | 300ms    | Reassuring        |
| Game over   | Descend      | 400→100 Hz       | 800ms    | Somber            |
| Victory     | Fanfare      | Arpeggio up      | 600ms    | Triumphant        |

## Music Tracks

| Track     | Phase                | Tempo   | Feel            |
| --------- | -------------------- | ------- | --------------- |
| Menu      | GAME_PHASE_MENU      | 90 BPM  | Calm, inviting  |
| Gameplay  | GAME_PHASE_PLAYING   | 120 BPM | Energetic       |
| Boss      | Boss encounter       | 140 BPM | Intense, urgent |
| Game Over | GAME_PHASE_GAME_OVER | 60 BPM  | Somber          |
| Victory   | Level complete       | 130 BPM | Triumphant      |
```

### 6.5 UI Layout

```markdown
## UI Elements

### HUD (GAME_PHASE_PLAYING)
```

┌────────────────────────────────────────┐
│ ♥♥♥ SCORE: 00000 │
│ ×3 │
│ 🔑×0 │
│ │
│ │
│ │
│ │
│ │
└────────────────────────────────────────┘

```
- Health: Top-left, heart icons
- Lives: Below health
- Keys: Below lives
- Score: Top-right

### Menu Screen
- Title: Large, centered top
- Buttons: Centered, stacked vertically
- Button order: PLAY, OPTIONS, CREDITS

### Pause Overlay
- Darken background (alpha 0.7)
- "PAUSED" centered
- RESUME / QUIT buttons

### Game Over Screen
- Darken background (alpha 0.8)
- "GAME OVER" top
- Final score centered
- RETRY / MENU buttons
```

---

## 7. Level Analysis

### 7.1 Level Structure

```markdown
## Level Data

### Dimensions

- Tile size: 32×32 pixels (1×1 meters)
- Level 1 size: 40×15 tiles (40×15 meters)
- Screen size: 20×15 tiles (visible area)

### Tile Types

| ID  | Type    | Collision     | Visual           |
| --- | ------- | ------------- | ---------------- |
| 0   | Empty   | None          | Transparent      |
| 1   | Solid   | Blocking      | Ground texture   |
| 2   | One-way | Top only      | Platform texture |
| 3   | Spikes  | Damage        | Spike sprite     |
| 4   | Ladder  | Climb trigger | Ladder sprite    |

### Entity Spawns

| ID  | Entity       | Behavior       |
| --- | ------------ | -------------- |
| P   | Player start | Spawn point    |
| E1  | Slime        | Patrol         |
| E2  | Bat          | Sine wave      |
| C   | Coin         | Pickup         |
| K   | Key          | Pickup         |
| D   | Door         | Requires key   |
| G   | Goal         | Level complete |
```

### 7.2 Level Progression

```markdown
## Level Sequence

| Level | New Mechanic     | Enemies      | Difficulty |
| ----- | ---------------- | ------------ | ---------- |
| 1     | Basic movement   | Slimes only  | Tutorial   |
| 2     | Jumping          | Slimes, gaps | Easy       |
| 3     | Combat           | Slimes, bats | Easy       |
| 4     | Keys/doors       | Mixed        | Medium     |
| 5     | Moving platforms | Mixed        | Medium     |
| ...   | ...              | ...          | ...        |
```

---

## 8. Flash-Specific Considerations

### 8.1 Flash Defaults

```markdown
## Flash Platform Defaults

| Property          | Default Value        | Notes                     |
| ----------------- | -------------------- | ------------------------- |
| Frame rate        | 30 FPS               | Verify from footage       |
| Stage size        | 550×400              | Newgrounds default        |
| Coordinate origin | Top-left             | We convert to bottom-left |
| Y direction       | Down = positive      | We use up = positive      |
| Collision         | AABB (hitTestObject) | Simple bounding box       |
```

### 8.2 Common Flash Patterns

```markdown
## Flash-Era Conventions

### Input

- Arrow keys primary (WASD secondary or absent)
- Space/Z for jump
- X/Ctrl for attack
- P for pause, M for mute

### Physics

- Often custom (not Box2D)
- Gravity applied per-frame, not per-second
- May need to multiply extracted values by FPS

### Timing

- enterFrame-based (tied to FPS)
- Timers in frames, not milliseconds
- Animation tied to game speed
```

### 8.3 Flash Bugs We Should Fix

```markdown
## Flash Bugs to NOT Recreate

| Bug                 | Cause                     | Our Fix                            |
| ------------------- | ------------------------- | ---------------------------------- |
| Fall through floors | No swept collision        | Use swept AABB or substep          |
| Stuck in walls      | Poor collision resolution | Proper depenetration               |
| Sound cutoff        | Too many sounds           | Sound pooling with limits          |
| Slowdown over time  | Memory leaks              | Proper object pooling              |
| Speed tied to FPS   | No delta time             | Delta time (per course-builder.md) |
```

---

## 9. Verification Strategy

### 9.1 Side-by-Side Comparison

**Method:**

1. Record original gameplay at consistent resolution
2. Record recreation gameplay at same resolution
3. Split-screen or overlay comparison
4. Check: proportions, timing, positions, feel

### 9.2 Timing Verification Checklist

```markdown
## Timing Verification

| Mechanic           | Original   | Recreation | Match | Notes |
| ------------------ | ---------- | ---------- | ----- | ----- |
| Jump duration      | X frames   |            | ☐     |       |
| Jump height        | X pixels   |            | ☐     |       |
| Walk speed         | X px/frame |            | ☐     |       |
| Attack duration    | X frames   |            | ☐     |       |
| Enemy patrol cycle | X frames   |            | ☐     |       |
| Invincibility      | X frames   |            | ☐     |       |
| Coyote time        | X frames   |            | ☐     |       |
| Jump buffer        | X frames   |            | ☐     |       |
```

### 9.3 Feel Verification

```markdown
## Feel Checklist

| Aspect                | Original | Recreation | Adjustment |
| --------------------- | -------- | ---------- | ---------- |
| Jump responsiveness   |          |            |            |
| Jump arc shape        |          |            |            |
| Air control amount    |          |            |            |
| Landing feel          |          |            |            |
| Attack feedback       |          |            |            |
| Hit feedback          |          |            |            |
| Movement acceleration |          |            |            |
| Movement deceleration |          |            |            |
```

### 9.4 Edge Case Testing

```markdown
## Edge Cases

### Movement

- [ ] Walking into wall (no jitter)
- [ ] Walking off ledge (coyote time)
- [ ] Landing on platform edge (no clip)
- [ ] Jumping into ceiling (stops cleanly)
- [ ] One-way platform (up = pass, down = solid)

### Combat

- [ ] Attack at ledge (no fall during attack)
- [ ] Hit during attack (interrupts correctly)
- [ ] Simultaneous hit (both take damage?)
- [ ] Kill with last HP (game over triggers)
- [ ] Invincibility prevents damage

### State Transitions

- [ ] Pause during attack
- [ ] Pause during jump
- [ ] Death during invincibility (shouldn't happen)
- [ ] Level transition during jump
- [ ] Restart resets everything

### Audio

- [ ] Sounds don't play when paused
- [ ] Music ducks when paused
- [ ] Rapid coin collection (sounds overlap correctly)
- [ ] Phase transitions trigger correct music
```

---

## 10. Risk Assessment

### 10.1 Research Risks

| Risk                    | Likelihood | Impact | Mitigation                                       |
| ----------------------- | ---------- | ------ | ------------------------------------------------ |
| No footage available    | Low        | High   | Check archive.org, Flashpoint, contact community |
| Low quality footage     | Medium     | Medium | Multiple sources, frame interpolation            |
| Undocumented mechanics  | High       | Medium | Community research, experimentation              |
| Conflicting information | Medium     | Low    | Primary source (footage) takes precedence        |

### 10.2 Extraction Risks

| Risk                   | Likelihood | Impact | Mitigation                                 |
| ---------------------- | ---------- | ------ | ------------------------------------------ |
| Incorrect timing       | Medium     | High   | Multiple measurements, cross-verification  |
| Wrong scale estimation | Medium     | Medium | Multiple methods, reasonableness check     |
| Missed mechanics       | High       | Medium | Thorough footage review, speedrun analysis |
| Hidden RNG             | Medium     | Low    | Fixed seed testing, pattern observation    |

### 10.3 Implementation Risks

| Risk                 | Likelihood | Impact | Mitigation                                |
| -------------------- | ---------- | ------ | ----------------------------------------- |
| Physics feel "off"   | High       | High   | Iterative tuning, side-by-side comparison |
| Scope creep          | Medium     | High   | Strict feature parity with original       |
| Platform differences | Low        | Medium | Test both backends continuously           |

---

## 11. PLAN.md Output Template

**The final deliverable is a PLAN.md file with this structure:**

```markdown
# [Game Name] Recreation — PLAN.md

## Overview

[One paragraph: what is this game, why recreate it, what's the teaching goal]

---

## Source Material

### Original Game

- **Title:** [Name]
- **Developer:** [Name]
- **Year:** [Year]
- **Platform:** [Newgrounds/Kongregate/etc.]

### Research Sources

- Gameplay footage: [URLs]
- Speedruns: [URLs]
- Community discussions: [URLs]
- Other remakes: [URLs]

---

## Extracted Parameters

### Scale

- **PIXELS_PER_METER:** [Value]
- Derivation: [Method used]

### Timing (at [X] FPS)

| Event         | Frames | Seconds |
| ------------- | ------ | ------- |
| Jump rise     |        |         |
| Jump fall     |        |         |
| Attack total  |        |         |
| Invincibility |        |         |
| [etc.]        |        |         |

### Physics

| Parameter     | Value | Unit |
| ------------- | ----- | ---- |
| Gravity       |       | m/s² |
| Jump velocity |       | m/s  |
| Walk speed    |       | m/s  |
| [etc.]        |       |      |

### Input Timing

| Mechanic    | Frames |
| ----------- | ------ |
| Jump buffer |        |
| Coyote time |        |
| [etc.]      |        |

---

## State Machines

### Game Phases

[Table mapping observed states to GAME_PHASE enum]

### Player States

[Table of player states with transitions]

### Enemy States

[Per-enemy state tables]

---

## Entity Catalog

### Player

[Dimensions, abilities, health system]

### Enemies

[Per-enemy specifications]

### Projectiles

[Projectile table]

### Pickups

[Pickup table]

### Environment

[Environment object table]

---

## Collision System

### Collision Matrix

[What collides with what]

### Hitbox Details

[Specific hitbox measurements]

---

## Asset Plan

### Visual Approach

[Procedural/recreated, color palette]

### Sound Effects

[Sound event table]

### Music

[Track list with phases]

### UI Layout

[HUD, menus, overlays]

---

## Level Structure

### Tile System

[Tile types and IDs]

### Level Progression

[Level sequence with new mechanics]

---

## Lesson Sequence

| Lesson | Title | Builds | Student Sees |
| ------ | ----- | ------ | ------------ |
| 01     |       |        |              |
| 02     |       |        |              |
| [etc.] |       |        |              |

---

## Verification Checklist

### Timing

- [ ] Jump duration matches
- [ ] Attack timing matches
- [ ] [etc.]

### Feel

- [ ] Jump arc feels right
- [ ] Movement feels right
- [ ] [etc.]

### Edge Cases

- [ ] [List from Section 9.4]

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
| ---- | ---------- | ------ | ---------- |
|      |            |        |            |

---

## Open Questions

- [Unresolved questions about original game]
- [Mechanics needing more research]
- [Design decisions to make during development]
```

---

## Quick Reference Checklist

**Before starting implementation, verify:**

### Research Complete

- [ ] Original game identified (title, developer, year)
- [ ] Gameplay footage collected (multiple sources)
- [ ] Community knowledge mined (forums, speedruns)
- [ ] Legal status documented

### Extraction Complete

- [ ] Frame rate determined
- [ ] Scale estimated (PIXELS_PER_METER)
- [ ] Timing data extracted (all major events)
- [ ] Physics parameters derived (gravity, speeds)
- [ ] Input behaviors documented (buffering, coyote time)

### State Machines Mapped

- [ ] Game phases identified → GAME_PHASE enum
- [ ] Player states identified with transitions
- [ ] Enemy states identified per enemy type

### Entities Cataloged

- [ ] Player fully specified
- [ ] All enemy types documented
- [ ] Projectiles listed
- [ ] Pickups listed
- [ ] Environment objects listed

### Collision Documented

- [ ] Collision matrix complete
- [ ] Hitbox sizes measured
- [ ] Special cases noted (one-way platforms, etc.)

### Assets Planned

- [ ] Visual approach chosen
- [ ] Color palette extracted
- [ ] Animation frame counts recorded
- [ ] Sound effects mapped to events
- [ ] Music tracks identified per phase
- [ ] UI layout documented

### Levels Analyzed

- [ ] Tile system documented
- [ ] Level progression outlined

### Verification Planned

- [ ] Timing checklist prepared
- [ ] Feel checklist prepared
- [ ] Edge cases listed

### Risks Identified

- [ ] Research risks documented
- [ ] Extraction risks documented
- [ ] Implementation risks documented

---

## Integration with course-builder.md

This document produces a **PLAN.md** that feeds directly into the `course-builder.md` workflow:

| This Document Produces | course-builder.md Uses For       |
| ---------------------- | -------------------------------- |
| PIXELS_PER_METER       | World unit system                |
| Physics parameters     | Game physics implementation      |
| State machine mapping  | GAME_PHASE enum, player states   |
| Entity catalog         | Entity structs, pools            |
| Collision matrix       | Collision system design          |
| Sound effects table    | Audio system (SOUND_ID enum)     |
| Music tracks           | MusicSequencer patterns          |
| Timing data            | Animation timing, input handling |
| Lesson sequence        | Course structure                 |

**Workflow:**

1. Use this document to research and extract → produces PLAN.md
2. Use PLAN.md + course-builder.md to implement → produces course lessons
