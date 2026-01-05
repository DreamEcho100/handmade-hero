#ifndef RAYLIB_AUDIO_H
#define RAYLIB_AUDIO_H

#include "../../base/base.h"
#include <raylib.h>
#include <stdlib.h>

#include "../../game.h"

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
void raylib_audio_callback(void *backbuffer, unsigned int frames);
void raylib_init_audio(GameSoundOutput *sound_output);


void raylib_debug_audio(GameSoundOutput *sound_output);

#endif // RAYLIB_AUDIO_H
#define RAYLIB_AUDIO_H
