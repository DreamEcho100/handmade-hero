#ifndef ENGINE_PLATFORM_RAYLIB_AUDIO_H
#define ENGINE_PLATFORM_RAYLIB_AUDIO_H

#include "../../_common/base.h"
#include <raylib.h>
#include <stdlib.h>

#include "../../game/audio.h"

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO STATE (Day 7-9)
// ═══════════════════════════════════════════════════════════════
// This mirrors your LinuxSoundOutput but simplified for Raylib
// ═══════════════════════════════════════════════════════════════

typedef struct {
  // Raylib audio stream handle
  AudioStream stream;

} RaylibSoundOutput;

extern RaylibSoundOutput g_linux_sound_output;
void raylib_init_audio(GameSoundOutput *sound_output);
void raylib_shutdown_audio(GameSoundOutput *sound_output);
void raylib_debug_audio(GameSoundOutput *sound_output);
void raylib_fill_sound_buffer(GameSoundOutput *sound_output);

#endif // ENGINE_PLATFORM_RAYLIB_AUDIO_H
#define ENGINE_PLATFORM_RAYLIB_AUDIO_H
