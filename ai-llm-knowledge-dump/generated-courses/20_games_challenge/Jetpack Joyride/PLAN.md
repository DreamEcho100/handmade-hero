# Jetpack Joyride (SlimeEscape) — Course Plan

## Overview

| Parameter | Value |
|-----------|-------|
| coord_mode | explicit |
| COORD_ORIGIN | COORD_ORIGIN_LEFT_TOP |
| GAME_W / GAME_H | 320 x 180 |
| GAME_UNITS_W / GAME_UNITS_H | 320.0f x 180.0f |
| Window size | 960 x 540 (3x upscale) |
| Backends | Raylib + X11/GLX/ALSA |

## Skill Inventory

| Skill | Lesson | Category |
|-------|--------|----------|
| Platform template adaptation | 01 | Foundation |
| stb_image.h header-only integration | 02 | Graphics |
| PNG decode to RGBA pixels | 02 | Graphics |
| Sprite struct and load/free API | 02 | Graphics |
| Spritesheet atlas layout | 03 | Graphics |
| Per-pixel alpha blitting | 03 | Graphics |
| Player struct with position/velocity | 04 | Physics |
| Gravity integration (v += g*dt) | 04 | Physics |
| Floor/ceiling collision clamping | 04-05 | Physics |
| Thrust vs gravity input model | 05 | Physics |
| Camera X tracking | 06 | Camera |
| Auto-scroll acceleration | 06 | Camera |
| RenderContext camera integration | 06 | Camera |
| Parallax scrolling | 07 | Graphics |
| Seamless background tiling | 07 | Graphics |
| SpriteAnimation (elapsed, fps, loop) | 08 | Animation |
| Animation state switching | 08 | Animation |
| Ground detection and state machine | 09 | Physics |
| AABB collision detection | 10 | Collision |
| Debug collision overlay | 10 | Collision |
| Sprite rotation (3-shear Paeth) | 11 | Graphics |
| Obstacle entity struct | 11 | Entities |
| Xorshift32 RNG | 12 | Math |
| Obstacle spawning/lifecycle | 12 | Entities |
| Per-entity state machine | 13 | Entities |
| Homing/tracking behavior | 13 | AI |
| Complex multi-state machine | 14 | Entities |
| Screen-width beam hitbox | 14 | Collision |
| Collectible entity pool | 15 | Entities |
| Score accumulation system | 15 | Gameplay |
| Near-miss detection (aabb margin) | 16 | Collision |
| Floating text indicator | 16 | UI |
| Shield pickup with bounce physics | 17 | Physics |
| Damage pipeline (shield/die) | 17 | Gameplay |
| WAV format parsing (RIFF header) | 18 | Audio |
| int16-to-float32 conversion | 18 | Audio |
| Voice pool for loaded clips | 18 | Audio |
| stb_vorbis OGG decoding | 19 | Audio |
| Pitch variation via playback rate | 19 | Audio |
| Dedicated music voice (loop) | 19 | Audio |
| Ring-buffer particle pool | 20 | Effects |
| Particle lifetime + alpha fade | 20 | Effects |
| Burst emission patterns | 20 | Effects |
| Multi-stage death sequence | 21 | Gameplay |
| Timer-based state transitions | 21 | Gameplay |
| HUD with separate RenderContext | 22 | UI |
| Game state machine (menu/play/pause) | 23 | Architecture |
| Menu rendering and navigation | 23 | UI |
| File I/O (fopen/fprintf/fscanf) | 24 | Persistence |
| Insertion sort for score table | 24 | Algorithms |
| Text input capture | 24 | Input |
| pthread_create/join | 25 | Threading |
| Atomic progress counter | 25 | Threading |
| Screen shake (camera offset+decay) | 26 | Polish |
| Damage flash overlay | 26 | Polish |
| Game feel tuning | 26 | Polish |

## Planned File Structure

```
course/
  build-dev.sh
  assets/sprites/   (PNG files)
  assets/audio/     (WAV + OGG files)
  vendor/stb_image.h, stb_vorbis.c
  src/
    platform.h
    utils/
      arena.h, audio.h, audio-helpers.h
      backbuffer.h/c, base.h, collision.h
      draw-shapes.h/c, draw-text.h/c
      loaded-audio.h/c, math.h
      particles.h/c, render.h, render_explicit.h
      rng.h, sprite.h/c, vec3.h, perf.h/c
    game/
      base.h, types.h, main.h, main.c
      player.h/c, obstacles.h/c
      audio.h/c, hud.c, menu.c
      high_scores.h/c
    platforms/
      raylib/main.c
      x11/base.h, base.c, main.c, audio.c
```

## Lesson Breakdown (26 lessons)

### Foundation (1-3)
- 01: Project setup and colored window
- 02: Sprite loading with stb_image
- 03: Sprite blitting and atlas frames

### Player Physics (4-6)
- 04: Player entity and gravity
- 05: Flight controls and physics
- 06: Camera follow and scrolling

### World Building (7-9)
- 07: Parallax background
- 08: Sprite animation system
- 09: Ground detection and walk cycle

### Obstacles (10-14)
- 10: AABB collision detection
- 11: Dagger obstacles and rotation
- 12: Obstacle spawning and variety
- 13: Missile obstacles and state machines
- 14: Sigil obstacles and beam attacks

### Collectibles & Scoring (15-17)
- 15: Pellet collectibles and scoring
- 16: Near-miss bonus system
- 17: Shield pickup and damage

### Audio (18-19)
- 18: WAV audio loading
- 19: OGG music and pitch variation

### Polish (20-22)
- 20: Particle effects
- 21: Death sequence and game over
- 22: HUD and score display

### Game Flow (23-26)
- 23: Main menu and pause
- 24: High scores and file I/O
- 25: Background asset loading
- 26: Polish and game feel

## Dependency Order

Level 0: Platform template (copied from platform-backend)
Level 1: sprite.h/c, loaded-audio.h/c, collision.h, rng.h, particles.h/c
Level 2: game/types.h, player.c, obstacles.c, audio.c, hud.c, menu.c, high_scores.c, main.c
