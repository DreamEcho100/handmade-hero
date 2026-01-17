#include "audio.h"

#include <math.h>
#include <stdio.h>

#include "../../game.h"

#ifndef M_PI_DOUBLED
#define M_PI_DOUBLED (2.f * M_PI)
#endif // M_PI_DOUBLED

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO STATE (Day 7-9)
// ═══════════════════════════════════════════════════════════════
// This mirrors your LinuxSoundOutput but simplified for Raylib
// ═══════════════════════════════════════════════════════════════

RaylibSoundOutput g_linux_sound_output = {0};

GameSoundOutput *temp_sound_output = NULL;

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO CALLBACK (Raylib's equivalent of linux_fill_sound_buffer)
// ═══════════════════════════════════════════════════════════════
//
// Raylib calls this function AUTOMATICALLY when it needs audio data.
// This is cleaner than ALSA's manual SndPcmWritei() every frame!
//
// Parameters:
//   backbuffer - Pointer to audio backbuffer to fill
//   frames - Number of stereo frames to generate
//
// ═══════════════════════════════════════════════════════════════
void raylib_audio_callback(void *backbuffer, unsigned int frames) {
  if (!temp_sound_output || !temp_sound_output->is_initialized) {
    return;
  }

  int16_t *sample_out = (int16_t *)backbuffer;

  for (unsigned int i = 0; i < frames; ++i) {
    // ═══════════════════════════════════════════════════════════
    // Day 9: Generate sine wave sample
    // ═══════════════════════════════════════════════════════════
    // Casey's exact formula:
    //   SineValue = sinf(tSine);
    //   SampleValue = (int16)(SineValue * ToneVolume);
    //   tSine += 2π / WavePeriod;
    // ═══════════════════════════════════════════════════════════

    real32 sine_value = sinf(temp_sound_output->t_sine);
    int16_t sample_value =
        (int16_t)(sine_value * temp_sound_output->tone_volume);

    // Apply panning (your extension from X11 version)
    int left_gain = (100 - temp_sound_output->pan_position);  // 0 to 200
    int right_gain = (100 + temp_sound_output->pan_position); // 0 to 200

    *sample_out++ = (sample_value * left_gain) / 200;  // Left channel
    *sample_out++ = (sample_value * right_gain) / 200; // Right channel

    // Increment phase accumulator (Day 9)
    temp_sound_output->t_sine +=
        (2.0f * M_PI * 1.0f) / (real32)temp_sound_output->wave_period;

    // Wrap to [0, 2π) range to prevent overflow
    if (temp_sound_output->t_sine >= 2.0f * M_PI) {
      temp_sound_output->t_sine -= 2.0f * M_PI;
    }

    temp_sound_output->running_sample_index++;
  }
}

// ═══════════════════════════════════════════════════════════════
// 🔊 INITIALIZE AUDIO SYSTEM (Day 7-9)
// ═══════════════════════════════════════════════════════════════
//
// Raylib equivalent of:
//   linux_load_alsa()      → InitAudioDevice()
//   linux_init_sound()     → LoadAudioStream() + SetAudioStreamCallback()
//
// This is MUCH simpler than ALSA because Raylib handles:
// - Device detection
// - Format negotiation
// - Buffer management
// - Error recovery
//
// ═══════════════════════════════════════════════════════════════

void raylib_init_audio(GameSoundOutput *sound_output) {
  printf("🔊 Initializing audio system...\n");

  // ═══════════════════════════════════════════════════════════
  // Step 1: Initialize audio device (Casey's DirectSoundCreate)
  // ═══════════════════════════════════════════════════════════
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "❌ Audio: Device initialization failed\n");
    fprintf(stderr, "   Game will run without sound\n");
    sound_output->is_initialized = false;
    return;
  }

  printf("✅ Audio: Device initialized\n");

  // ═══════════════════════════════════════════════════════════
  // Step 2: Set audio parameters (Day 7-9)
  // ═══════════════════════════════════════════════════════════
  sound_output->samples_per_second = 24000;
  sound_output->bytes_per_sample = sizeof(int16_t) * 2; // 16-bit stereo

  // Day 8: Sound generation parameters
  sound_output->running_sample_index = 0;
  sound_output->tone_hz = 256;      // Middle C-ish
  sound_output->tone_volume = 6000; // Casey's Day 8 value
  sound_output->wave_period =
      sound_output->samples_per_second / sound_output->tone_hz;

  // Day 9: Sine wave phase accumulator
  sound_output->t_sine = 0.0f;

  // Day 9: Latency (1/15 second like Casey)
  sound_output->latency_sample_count = sound_output->samples_per_second / 15;

  sound_output->pan_position = 0; // Center

  // ═══════════════════════════════════════════════════════════
  // Step 3: Create audio stream (Casey's secondary backbuffer)
  // ═══════════════════════════════════════════════════════════
  //
  // LoadAudioStream parameters:
  //   - sampleRate:    48000 Hz (CD quality is 44100)
  //   - sampleSize:    16 bits per sample
  //   - channels:      2 (stereo)
  //
  // Raylib creates a ring backbuffer internally (like DirectSound)
  // ═══════════════════════════════════════════════════════════

  g_linux_sound_output.stream =
      LoadAudioStream(sound_output->samples_per_second, // 48000 Hz
                      16,                               // 16-bit samples
                      2                                 // Stereo
      );

  // ═══════════════════════════════════════════════════════════
  // Step 4: Set backbuffer size (Casey's latency calculation)
  // ═══════════════════════════════════════════════════════════
  // 4096 frames = ~85ms at 48kHz (similar to Casey's 1/15 sec)
  SetAudioStreamBufferSizeDefault(4096);

  // ═══════════════════════════════════════════════════════════
  // Step 5: Attach callback (automatic audio generation!)
  // ═══════════════════════════════════════════════════════════
  // Raylib will call raylib_audio_callback() when backbuffer needs data
  // This replaces your manual linux_fill_sound_buffer() call
  temp_sound_output = sound_output;
  SetAudioStreamCallback(g_linux_sound_output.stream, raylib_audio_callback);

  // ═══════════════════════════════════════════════════════════
  // Step 6: Start playback (Casey's IDirectSoundBuffer->Play())
  // ═══════════════════════════════════════════════════════════
  PlayAudioStream(g_linux_sound_output.stream);

  sound_output->is_initialized = true;

  printf("✅ Audio: Initialized with sine wave!\n");
  printf("   Sample rate:  %d Hz\n", sound_output->samples_per_second);
  printf("   Tone:         %d Hz (sine wave)\n", sound_output->tone_hz);
  printf("   Wave period:  %d samples\n", sound_output->wave_period);
  printf("   Latency:      %d samples (~%.1f ms)\n",
         sound_output->latency_sample_count,
         (float)sound_output->latency_sample_count /
             sound_output->samples_per_second * 1000.0f);
  printf("   Buffer:       4096 frames (~85 ms)\n");
}

void raylib_shutdown_audio(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized) {
    return;
  }

  // Stop playback
  StopAudioStream(g_linux_sound_output.stream);

  // Unload audio stream
  UnloadAudioStream(g_linux_sound_output.stream);

  // Close audio device
  CloseAudioDevice();

  sound_output->is_initialized = false;

  printf("✅ Audio: Shutdown complete\n");
}

// ═══════════════════════════════════════════════════════════════
// 🎵 DAY 8: SOUND CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════════════
// These mirror your X11 backend functions exactly
// ═══════════════════════════════════════════════════════════════

void raylib_debug_audio(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 Raylib Audio Debug Info                              │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");
  printf("│ Mode: Callback-based (automatic latency control)       │\n");
  printf("│                                                         │\n");
  printf("│ Sample rate:       %d Hz                                 │\n",
         sound_output->samples_per_second);
  printf("│ Frequency:         %d Hz                                 │\n",
         sound_output->tone_hz);
  printf("│ Volume:            %d / 15000                            │\n",
         sound_output->tone_volume);
  printf("│ Pan:               %+d (L=%d, R=%d)                      │\n",
         sound_output->pan_position, 100 - sound_output->pan_position,
         100 + sound_output->pan_position);
  printf("└─────────────────────────────────────────────────────────┘\n");
}
