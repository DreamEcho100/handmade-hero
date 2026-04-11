# Lesson 05 -- Flight Controls and Physics

## Observable Outcome
Holding SPACE (or clicking the mouse) makes the slime fly upward. Releasing causes it to fall back down under gravity. The flight feels smooth and responsive. The slime cannot fly above the ceiling at y=4 or fall below the floor at y=165. The vertical motion matches the original Godot game's feel exactly.

## New Concepts (max 3)
1. virtSpeed accumulator -- a velocity variable that ramps between PLAYER_JUMP_VEL and PLAYER_GRAVITY
2. Asymmetric input: action-held decreases virtSpeed (fly), released increases (fall)
3. Ceiling collision clamping

## Prerequisites
- Lesson 04 (Player struct, double-delta integration, floor collision)

## Background

The original SlimeEscape uses a continuous-press flight model similar to Jetpack Joyride or Flappy Bird (but smoother). The key insight is the `virtSpeed` accumulator. Rather than applying an instant impulse on button press, the game continuously adjusts a vertical speed variable at a fixed rate (PLAYER_VIRT_ACCEL = 12000 units/sec). When the player holds the action button, virtSpeed ramps toward PLAYER_JUMP_VEL (-4500, upward). When released, it ramps toward PLAYER_GRAVITY (+4500, downward). The accumulator is clamped to the range [-4500, +4500] at all times.

This creates a smooth, analog-feeling vertical motion from a binary digital input. The ramp rate is fast enough (12000/sec) that pressing the button gives an almost immediate upward response, but the clamping prevents the player from accelerating infinitely. The result feels like piloting a jetpack: immediate thrust on press, gradual deceleration and gravity reassertion on release.

The separation between "in air, not pressing" and "pressing" matters for the physics branching. When on the floor and not pressing, no vertical physics runs (the player stays grounded). When on the floor and pressing, virtSpeed resets to 0 before ramping upward, giving a clean launch.

## Code Walkthrough

### Step 1: Reading the action button

The action button maps to SPACE and left mouse click. We read it as a held state (not just pressed this frame):

```c
void player_update(Player *player, GameInput *input, float dt) {
  switch (player->state) {
  case PLAYER_STATE_NORMAL: {
    int action_pressed = input->buttons.action.ended_down;
```

`ended_down` is 1 as long as the button is physically held. This gives continuous thrust while holding.

### Step 2: Falling -- airborne without action

When the player is in the air and NOT pressing the action button, gravity takes over:

```c
    if (!player->is_on_floor && !action_pressed) {
      if (player->is_on_ceiling) {
        player->virt_speed = 0.0f;
      }
      player->virt_speed = clampf(
          player->virt_speed + PLAYER_VIRT_ACCEL * dt,
          PLAYER_JUMP_VEL, PLAYER_GRAVITY);
    }
```

`PLAYER_VIRT_ACCEL * dt` is added to virt_speed, pushing it toward the positive (downward) clamp. At 60fps: 12000 * 0.0167 = ~200 units per frame. From maximum upward velocity (-4500) to maximum downward (+4500) takes about 45 frames (0.75 seconds) -- a natural-feeling arc.

The ceiling check zeroes virt_speed when the player hits the top, preventing them from accumulating upward velocity while pressed against the ceiling.

### Step 3: Flying -- action button held

When the action button is held, virt_speed ramps downward (negative = upward in y-down coords):

```c
    if (action_pressed) {
      if (player->is_on_floor) {
        player->virt_speed = 0.0f;
      }
      player->virt_speed = clampf(
          player->virt_speed - PLAYER_VIRT_ACCEL * dt,
          PLAYER_JUMP_VEL, PLAYER_GRAVITY);
    }
```

When launching from the floor, virt_speed is reset to 0 first. This ensures a clean takeoff regardless of any residual velocity from a previous landing.

The subtraction (`- PLAYER_VIRT_ACCEL * dt`) drives virt_speed toward PLAYER_JUMP_VEL (-4500). The clamp prevents it from going below -4500.

### Step 4: Horizontal acceleration

The horizontal speed increases each frame, capped at PLAYER_MAX_SPEED:

```c
    player->speed = clampf(player->speed + PLAYER_ACCEL * dt,
                           PLAYER_MIN_SPEED, PLAYER_MAX_SPEED);
```

Starting at 2000, adding 250/sec, reaching 6000 after (6000-2000)/250 = 16 seconds. The game gets progressively faster, increasing difficulty.

### Step 5: Position integration and distance tracking

Both axes use the double-delta model:

```c
    player->x += player->speed * dt * dt;
    player->y += player->virt_speed * dt * dt;

    player->total_distance += player->speed * dt * dt;
```

### Step 6: Ceiling collision

After integration, we check and clamp at both boundaries:

```c
    if (player->y < CEILING_Y) {
      player->y = CEILING_Y;
      player->virt_speed = 0.0f;
      player->is_on_ceiling = 1;
    } else {
      player->is_on_ceiling = 0;
    }
```

The ceiling at y=4 prevents the player from flying off the top of the screen. Zeroing virt_speed on ceiling contact means the player starts falling immediately when they release the button at the ceiling, rather than having to overcome residual upward momentum.

### Step 7: The complete physics flow

Putting it all together, the per-frame vertical physics flow is:

1. Read action button state
2. If airborne and not pressing: ramp virt_speed toward +4500 (fall)
3. If pressing: ramp virt_speed toward -4500 (fly)
4. Integrate: `y += virt_speed * dt * dt`
5. Clamp to floor (y + PLAYER_H >= 165): snap, zero virt_speed, set is_on_floor
6. Clamp to ceiling (y < 4): snap, zero virt_speed, set is_on_ceiling

## Common Mistakes

**Applying gravity while on the floor.** Without the `!player->is_on_floor` guard, the falling branch runs every frame even when grounded. This means virt_speed accumulates to +4500 while standing, and when the player jumps, they have to overcome all that downward velocity first -- the jump feels sluggish and delayed.

**Forgetting to reset virt_speed when launching from floor.** If the player was on the floor with virt_speed=0 and hits action, the ramp starts from 0 (correct). But if they landed with residual virt_speed and the floor collision did not reset it properly, the launch will feel inconsistent.

**Using single-delta for virt_speed integration but double-delta for position.** The velocity accumulator update (`virt_speed += accel * dt`) is single-delta because it represents acceleration over time. Only the position integration uses double-delta. Mixing these up produces either sluggish or explosive vertical motion.

**Not clamping virt_speed.** Without the clamp to [-4500, +4500], the accumulator grows unbounded. Holding the action button for a long time would build enormous upward velocity, and the player would shoot through the ceiling check.
