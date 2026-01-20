#ifndef ENGINE_GAME_BASE_H
#define ENGINE_GAME_BASE_H

#include "audio.h"
#include "backbuffer.h"
#include "input.h"
#include "memory.h"
#include <stdbool.h>

extern bool is_game_running;
extern bool g_game_is_paused;

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

#endif // ENGINE_GAME_BASE_H
