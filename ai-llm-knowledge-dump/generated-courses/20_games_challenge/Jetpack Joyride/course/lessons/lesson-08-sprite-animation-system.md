# Lesson 08 -- Sprite Animation System

## Observable Outcome
On the ground, the slime plays a two-frame walk animation at 3 frames per second (frames 2-3 from the Slime.png spritesheet). In the air, the slime instantly switches between frame 0 (flying up, when virt_speed < 0) and frame 1 (falling, when virt_speed >= 0). The transitions between ground and air are seamless -- no flicker, no stuck frames.

## New Concepts (max 3)
1. SpriteAnimation struct -- elapsed-time accumulator driving frame advancement
2. Loop vs. play-once animation modes
3. Static frame selection as an alternative to animation (air frames)

## Prerequisites
- Lesson 03 (SpriteSheet and spritesheet_frame_rect)
- Lesson 04 (Player struct with is_on_floor flag)

## Background

Animation in a sprite-based game is fundamentally a timer problem. You have N frames in a spritesheet and you want to cycle through them at a fixed rate. The standard approach is to accumulate elapsed time and advance the frame index each time the accumulator exceeds the frame duration (1/fps). For looping animations, the frame wraps back to 0 when it passes the last frame. For play-once animations (like death), it stops at the last frame and sets a "finished" flag.

Our SpriteAnimation struct encapsulates this pattern. It stores a pointer to its spritesheet, the starting frame index, frame count, fps, elapsed time, and current frame. The caller is responsible for calling `anim_update(dt)` each frame and then drawing the result.

Not all visual states need animation. The slime's air behavior uses static frame selection: we look at virt_speed and pick frame 0 or 1 directly, with no timer involved. This is simpler and more responsive than running a separate air animation -- the frame changes instantly when the player's velocity direction changes.

The ground walk uses frames 2 and 3 from the Slime.png atlas (the bottom row). The animation is initialized with `start_frame=2, frame_count=2`, which means the system cycles through sheet frames 2 and 3. The fps is set to 3 -- slow enough to be readable at the 20x20 pixel scale.

## Code Walkthrough

### Step 1: SpriteAnimation struct

Defined in sprite.h:

```c
typedef struct {
  SpriteSheet *sheet;      /* which spritesheet to animate */
  int start_frame;         /* first frame index in the sheet */
  int frame_count;         /* number of frames in this anim */
  float fps;               /* playback speed (frames/second) */
  float elapsed;           /* accumulated time since last frame */
  int current_frame;       /* current frame index (0-based) */
  int loop;                /* 1 = loop, 0 = play-once */
  int finished;            /* 1 if play-once completed */
} SpriteAnimation;
```

### Step 2: anim_init

Sets up an animation with its sheet reference, frame range, speed, and loop mode:

```c
void anim_init(SpriteAnimation *anim, SpriteSheet *sheet,
               int start_frame, int frame_count, float fps,
               int loop) {
  anim->sheet = sheet;
  anim->start_frame = start_frame;
  anim->frame_count = frame_count;
  anim->fps = fps;
  anim->elapsed = 0.0f;
  anim->current_frame = 0;
  anim->loop = loop;
  anim->finished = 0;
}
```

In player_init, the ground walk animation is configured:

```c
/* Ground walk: frames 2-3, 3 fps, loop */
anim_init(&player->anim_ground, &player->sheet, 2, 2, 3.0f, 1);
```

And the death animation:

```c
/* Death: frames 0-3 of Dead.png sheet, 5fps, play once */
anim_init(&player->anim_dead, &player->sheet_dead, 0, 4, 5.0f, 0);
```

### Step 3: anim_update -- the time accumulator

Each frame, we add dt to the elapsed time. When it exceeds the frame duration, we advance the frame:

```c
void anim_update(SpriteAnimation *anim, float dt) {
  if (anim->finished || anim->frame_count <= 1 || anim->fps <= 0.0f)
    return;

  anim->elapsed += dt;
  float frame_duration = 1.0f / anim->fps;

  while (anim->elapsed >= frame_duration) {
    anim->elapsed -= frame_duration;
    anim->current_frame++;

    if (anim->current_frame >= anim->frame_count) {
      if (anim->loop) {
        anim->current_frame = 0;
      } else {
        anim->current_frame = anim->frame_count - 1;
        anim->finished = 1;
        anim->elapsed = 0.0f;
        return;
      }
    }
  }
}
```

Key design decisions:
- The `while` loop (not `if`) handles the case where dt is larger than frame_duration. At 3fps with frame_duration=0.333s, a single 0.5s frame skip would advance by 1 frame, not miss it.
- For play-once animations, finished=1 stops all future updates and the last frame stays visible.
- Single-frame or zero-fps animations short-circuit immediately.

### Step 4: anim_reset

Used when restarting the game or transitioning states:

```c
void anim_reset(SpriteAnimation *anim) {
  anim->current_frame = 0;
  anim->elapsed = 0.0f;
  anim->finished = 0;
}
```

### Step 5: anim_draw -- rendering the current frame

A convenience function that computes the sheet frame index and blits:

```c
void anim_draw(const SpriteAnimation *anim, Backbuffer *bb,
               int dst_x, int dst_y) {
  if (!anim->sheet || !anim->sheet->sprite.pixels)
    return;

  int frame = anim->start_frame + anim->current_frame;
  SpriteRect rect = spritesheet_frame_rect(anim->sheet, frame);
  sprite_blit(bb, &anim->sheet->sprite, rect, dst_x, dst_y);
}
```

The actual sheet frame is `start_frame + current_frame`. For the ground walk: start_frame=2, current_frame alternates 0/1, so we draw sheet frames 2 and 3.

### Step 6: Ground animation in player_update

The ground animation only ticks when the player is on the floor:

```c
/* In player_update, PLAYER_STATE_NORMAL: */
if (player->is_on_floor) {
  anim_update(&player->anim_ground, dt);
}
```

When airborne, the animation does not update. This means the walk cycle pauses mid-stride while in the air, which is fine because the renderer uses static frame selection for airborne states anyway.

### Step 7: Static frame selection for air in player_render

Air frames bypass the animation system entirely:

```c
void player_render(const Player *player, Backbuffer *bb,
                   float camera_x) {
  int screen_x = (int)(player->x - camera_x);
  int screen_y = (int)player->y;

  switch (player->state) {
  case PLAYER_STATE_NORMAL: {
    if (player->is_on_floor) {
      /* Ground walk animation */
      int frame = player->anim_ground.start_frame +
                  player->anim_ground.current_frame;
      SpriteRect rect = spritesheet_frame_rect(&player->sheet, frame);
      sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
    } else {
      /* Air: frame 0 if flying up, frame 1 if falling */
      int frame = (player->virt_speed < 0.0f) ? 0 : 1;
      SpriteRect rect = spritesheet_frame_rect(&player->sheet, frame);
      sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
    }
    break;
  }
  /* ... DEAD, GAMEOVER cases ... */
  }
}
```

The air frame selection is instant: the frame changes the exact frame that virt_speed crosses zero. No animation timing, no transition delay. This makes the visual feedback for "pressing action" and "releasing action" immediate and responsive.

### Step 8: Slime.png frame layout

The atlas layout drives everything:

```
Frame 0 (0,0):  Flying up (top-left)
Frame 1 (20,0): Falling   (top-right)
Frame 2 (0,20): Walk 1    (bottom-left)
Frame 3 (20,20): Walk 2   (bottom-right)
```

## Common Mistakes

**Confusing current_frame with the sheet frame index.** current_frame is 0-based relative to the animation's start_frame. To get the actual sheet index, you must add start_frame. Drawing `spritesheet_frame_rect(sheet, anim->current_frame)` instead of `spritesheet_frame_rect(sheet, anim->start_frame + anim->current_frame)` shows the wrong frame.

**Updating the ground animation while airborne.** If you call `anim_update(&anim_ground, dt)` every frame regardless of is_on_floor, the walk cycle advances while the player is in the air. When they land, the walk starts at whatever frame the timer happened to reach. Gating the update behind is_on_floor keeps the walk cycle paused mid-stride.

**Using an animation for air frames.** A two-frame animation oscillating at some fps would create a rhythmic flapping effect. The original game uses instant frame switching based on velocity direction. An animation would add latency to the visual feedback and feel wrong.

**Setting fps too high.** At 60fps game rate and 3fps animation rate, each walk frame displays for 20 game frames. At 30fps animation rate, each frame would display for only 2 game frames -- far too fast to read at 20x20 pixels. Pixel art animation works best at very low frame rates (2-8 fps).
