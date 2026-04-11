# Jetpack Joyride (SlimeEscape Version)

A course that teaches you how to build a **Jetpack Joyride-style 2D endless side-scroller** from scratch in C, using a custom software renderer (backbuffer pipeline).

## What You'll Build

A fast-paced arcade game where a slime character auto-scrolls rightward through an obstacle course. Hold SPACE to fly upward with a jetpack; release to fall. Dodge spinning daggers, homing missiles, and magical beam attacks. Collect pellets for points, grab shields for protection, and compete for high scores.

## Game Features

- **Flight physics**: Hold-to-fly / release-to-fall with gravity and thrust
- **6 obstacle types**: Spinning daggers, sequential missiles, homing missiles, single sigils, multi-sigils, and dagger formations
- **Collectibles**: Pellets (+10 pts), shield pickups (absorb one hit), near-miss bonuses (+50 pts)
- **Scoring**: Distance + pellets + near-misses with persistent top-10 high scores
- **Audio**: 14 sound effects with pitch variation + 2 music tracks (WAV + OGG)
- **Particle effects**: Jetpack exhaust, death burst, shield break sparkles
- **Full game flow**: Main menu, gameplay, pause, death sequence, game over, high score entry

## Prerequisites

- Completed the [Platform Foundation Course](../../platform-backend/)
- C programming fundamentals
- Familiarity with the 20 pillars of Modern C Practices

## Technical Stack

- **Language**: C (compiled with clang)
- **Renderer**: Software backbuffer (320x180, ARGB pixel pipeline)
- **Backends**: Raylib (cross-platform) and X11/GLX + ALSA (Linux native)
- **Libraries**: stb_image.h (PNG loading), stb_vorbis.c (OGG decoding)
- **Resolution**: 320x180 native, upscaled 3x to 960x540 via GPU letterbox

## Course Structure

26 lessons organized into 8 sections:

1. **Foundation** (Lessons 1-3): Project setup, sprite loading, atlas blitting
2. **Player Physics** (Lessons 4-6): Gravity, flight controls, camera follow
3. **World Building** (Lessons 7-9): Parallax background, animation, ground detection
4. **Obstacles** (Lessons 10-14): Collision, daggers, spawning, missiles, sigils
5. **Collectibles & Scoring** (Lessons 15-17): Pellets, near-miss, shields
6. **Audio** (Lessons 18-19): WAV loading, OGG music, pitch variation
7. **Polish** (Lessons 20-22): Particles, death sequence, HUD
8. **Game Flow** (Lessons 23-26): Menus, high scores, threading, game feel

## Building

```bash
cd course

# Raylib backend (default)
./build-dev.sh --backend=raylib -r

# X11/GLX + ALSA backend (Linux)
./build-dev.sh --backend=x11 -r
```

## Credits

- **Original game**: SlimeEscape by TheStarHawk (Godot 4.3)
- **STB libraries**: stb_image.h v2.30, stb_vorbis.c v1.22 by Sean Barrett (MIT/Public Domain)
- **Engine template**: Platform Foundation Course
