#ifndef HANDMADE_HERO_GAME_H
#define HANDMADE_HERO_GAME_H

#include "../engine/_common/base.h"
#include "../engine/game/backbuffer.h"
#include "../engine/_common/memory.h"
#include "../engine/game/input.h"
#include "../engine/game/audio.h"
#include "../engine/game/memory.h"


GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

typedef struct {
  GradientState gradient_state;
  PixelState pixel_state;
  int speed;
} GameState;

/**
 * 🎮 DAY 13: Updated Game Entry Point
 * ═══════════════════════════════════════════════════════════════
 *
 * New signature takes abstract input (not pixel_color parameter).
 *
 * Casey's Day 13 change:
 *   OLD: GameUpdateAndRender(Buffer, XOffset, YOffset, Sound, ToneHz)
 *   NEW: GameUpdateAndRender(Input, Buffer, Sound)
 *
 * We add:
 *   game_input *input  ← Platform-agnostic input state
 *
 * Game state (offsets, tone_hz, etc.) now lives in game.c as
 * local_persist variables (hidden from platform!).
 *
 * ═══════════════════════════════════════════════════════════════
 */
void game_update_and_render(GameMemory *memory, GameInput *input,
                            GameOffscreenBuffer *buffer,
                            GameSoundOutput *sound_buffer);

#endif // HANDMADE_HERO_GAME_H
