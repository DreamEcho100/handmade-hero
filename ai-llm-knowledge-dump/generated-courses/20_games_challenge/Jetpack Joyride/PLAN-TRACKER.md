# Jetpack Joyride — Plan Tracker

## Phase 0 — Planning
- [x] PLAN.md created
- [x] PLAN-TRACKER.md created
- [x] README.md created

## Phase 1 — Build Complete Working Source Code

### Level 0 — Platform Template
- [x] Copy and adapt utils/ from platform-backend
- [x] Adapt base.h (GAME_W=320, GAME_H=180, COORD_ORIGIN_LEFT_TOP)
- [x] Adapt platform.h (GAME_TITLE, includes)
- [x] Adapt game/base.h (new button names)
- [x] Adapt build-dev.sh
- [x] Adapt platforms/raylib/main.c
- [x] Adapt platforms/x11/main.c
- [x] Create game/main.h + game/main.c stubs
- [x] BUILD + TEST: both backends compile, window opens

### Level 1 — New Engine Utils
- [x] utils/sprite.h + sprite.c (stb_image integration, STB_IMAGE_STATIC)
- [x] BUILD + TEST: PNG renders on screen
- [x] Sprite atlas frame extraction + alpha blitting
- [x] BUILD + TEST: single frame with transparency
- [x] utils/rng.h (xorshift32)
- [x] utils/collision.h (AABB overlap, near-miss)
- [x] utils/loaded-audio.h + loaded-audio.c (WAV parser, OGG via stb_vorbis)
- [x] BUILD + TEST: WAV sound plays
- [x] utils/particles.h + particles.c (ring-buffer particle pool)
- [x] BUILD + TEST: particles visible on screen

### Level 2 — Game Layer
- [x] game/types.h (Player, Obstacle, Pellet, ShieldPickup, enums)
- [x] game/player.h + player.c (gravity, flight, ground detection)
- [x] BUILD + TEST: player falls, flies, lands
- [x] Camera follow + auto-scroll (dt*dt model matching Godot)
- [x] BUILD + TEST: world scrolls, player at 80px offset
- [x] Parallax background
- [x] BUILD + TEST: background tiles seamlessly
- [x] Sprite animation system
- [x] BUILD + TEST: walk + air animations correct
- [x] game/obstacles.h + obstacles.c (daggers, missiles, sigils)
- [x] Dagger blocks with original positions from .tscn files
- [x] BUILD + TEST: spinning daggers in cluster patterns
- [x] Obstacle spawning (blocks visible from game start, proper spacing)
- [x] BUILD + TEST: random obstacles spawn, no repeats
- [x] Missiles (sequential spawning, WARNING → FIRE → BOOM)
- [x] BUILD + TEST: missiles appear one at a time
- [x] Sigils (coordinated fire_order sequence for block 2)
- [x] BUILD + TEST: sigil beams fire in pairs
- [x] Pellet collectibles + scoring
- [x] BUILD + TEST: pellets collect, score increments
- [x] Near-miss bonus
- [x] BUILD + TEST: +50 indicator appears
- [x] Shield pickup + damage pipeline
- [x] BUILD + TEST: shields absorb hits
- [x] game/audio.h + audio.c (all SFX + music, pthread background loading)
- [x] BUILD + TEST: all sounds play correctly
- [x] Particle effects (exhaust, death, shield)
- [x] BUILD + TEST: particles on all events
- [x] Death sequence + game over transition
- [x] BUILD + TEST: full death flow
- [x] game/hud.c (distance, pellets, score) — integrated in main.c
- [x] BUILD + TEST: HUD visible and updating
- [x] Game over + restart (preserves loaded resources)
- [x] BUILD + TEST: complete game flow cycle
- [x] game/high_scores.h + high_scores.c
- [x] BUILD + TEST: scores persist to file
- [x] Background asset loading (pthread for audio)
- [x] BUILD + TEST: game starts immediately, audio loads in background

## Phase 1.5 — Freeze & Reconcile
- [x] Buildability audit: both backends compile with zero game-code warnings
- [x] Source file inventory: all .c files in build-dev.sh SOURCES
- [x] Debug output gated behind DEBUG flag
- [x] No UBSan violations
- [x] Gameplay verified via debug output (speed, dt, position, spawn timing)
- [x] Restart preserves resources (no memset of pointers)
- [x] Namespace collision check (Raylib types — uses -Wl,-z,muldefs for stb_vorbis)
- [x] Performance budget: verify >15 fps at 320x180 (60fps confirmed)
- [x] Update PLAN.md to match actual code

## Phase 2 — Build Course Lessons
- [x] lesson-01-project-setup-and-colored-window.md
- [x] lesson-02-sprite-loading-with-stb-image.md
- [x] lesson-03-sprite-blitting-and-atlas.md
- [x] lesson-04-player-entity-and-gravity.md
- [x] lesson-05-flight-controls-and-physics.md
- [x] lesson-06-camera-follow-and-scrolling.md
- [x] lesson-07-parallax-background.md
- [x] lesson-08-sprite-animation-system.md
- [x] lesson-09-ground-detection-and-walk-cycle.md
- [x] lesson-10-aabb-collision-detection.md
- [x] lesson-11-dagger-obstacles-and-rotation.md
- [x] lesson-12-obstacle-spawning-and-variety.md
- [x] lesson-13-missile-obstacles-and-state-machines.md
- [x] lesson-14-sigil-obstacles-and-beam-attacks.md
- [x] lesson-15-pellet-collectibles-and-scoring.md
- [x] lesson-16-near-miss-bonus-system.md
- [x] lesson-17-shield-pickup-and-damage.md
- [x] lesson-18-wav-audio-loading.md
- [x] lesson-19-ogg-music-and-pitch-variation.md
- [x] lesson-20-particle-effects.md
- [x] lesson-21-death-sequence-and-game-over.md
- [x] lesson-22-hud-and-score-display.md
- [x] lesson-23-main-menu-and-pause.md
- [x] lesson-24-high-scores-and-file-io.md
- [x] lesson-25-background-asset-loading.md
- [x] lesson-26-polish-and-game-feel.md

## ALL PHASES COMPLETE
