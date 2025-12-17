#include "audio.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_double_PI
#define M_double_PI (2.f * M_PI)
#endif // M_double_PI

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO STATE (Day 7-9)
// ═══════════════════════════════════════════════════════════════
// This mirrors your LinuxSoundOutput but simplified for Raylib
// ═══════════════════════════════════════════════════════════════

RaylibSoundOutput g_sound_output = {0};

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO CALLBACK (Raylib's equivalent of linux_fill_sound_buffer)
// ═══════════════════════════════════════════════════════════════
//
// Raylib calls this function AUTOMATICALLY when it needs audio data.
// This is cleaner than ALSA's manual SndPcmWritei() every frame!
//
// Parameters:
//   buffer - Pointer to audio buffer to fill
//   frames - Number of stereo frames to generate
//
// ═══════════════════════════════════════════════════════════════
void raylib_audio_callback(void *buffer, unsigned int frames) {
  if (!g_sound_output.is_initialized) {
    return;
  }

  int16_t *sample_out = (int16_t *)buffer;

  for (unsigned int i = 0; i < frames; ++i) {
    // ═══════════════════════════════════════════════════════════
    // Day 9: Generate sine wave sample
    // ═══════════════════════════════════════════════════════════
    // Casey's exact formula:
    //   SineValue = sinf(tSine);
    //   SampleValue = (int16)(SineValue * ToneVolume);
    //   tSine += 2π / WavePeriod;
    // ═══════════════════════════════════════════════════════════

    real32 sine_value = sinf(g_sound_output.t_sine);
    int16_t sample_value = (int16_t)(sine_value * g_sound_output.tone_volume);

    // Apply panning (your extension from X11 version)
    int left_gain = (100 - g_sound_output.pan_position);  // 0 to 200
    int right_gain = (100 + g_sound_output.pan_position); // 0 to 200

    *sample_out++ = (sample_value * left_gain) / 200;  // Left channel
    *sample_out++ = (sample_value * right_gain) / 200; // Right channel

    // Increment phase accumulator (Day 9)
    g_sound_output.t_sine +=
        (2.0f * M_PI * 1.0f) / (real32)g_sound_output.wave_period;

    // Wrap to [0, 2π) range to prevent overflow
    if (g_sound_output.t_sine >= 2.0f * M_PI) {
      g_sound_output.t_sine -= 2.0f * M_PI;
    }

    g_sound_output.running_sample_index++;
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

void raylib_init_audio(void) {
  printf("🔊 Initializing audio system...\n");

  // ═══════════════════════════════════════════════════════════
  // Step 1: Initialize audio device (Casey's DirectSoundCreate)
  // ═══════════════════════════════════════════════════════════
  InitAudioDevice();

  if (!IsAudioDeviceReady()) {
    fprintf(stderr, "❌ Audio: Device initialization failed\n");
    fprintf(stderr, "   Game will run without sound\n");
    g_sound_output.is_initialized = false;
    return;
  }

  printf("✅ Audio: Device initialized\n");

  // ═══════════════════════════════════════════════════════════
  // Step 2: Set audio parameters (Day 7-9)
  // ═══════════════════════════════════════════════════════════
  g_sound_output.samples_per_second = 48000;
  g_sound_output.bytes_per_sample = sizeof(int16_t) * 2; // 16-bit stereo

  // Day 8: Sound generation parameters
  g_sound_output.running_sample_index = 0;
  g_sound_output.tone_hz = 256;      // Middle C-ish
  g_sound_output.tone_volume = 6000; // Casey's Day 8 value
  g_sound_output.wave_period =
      g_sound_output.samples_per_second / g_sound_output.tone_hz;

  // Day 9: Sine wave phase accumulator
  g_sound_output.t_sine = 0.0f;

  // Day 9: Latency (1/15 second like Casey)
  g_sound_output.latency_sample_count = g_sound_output.samples_per_second / 15;

  g_sound_output.pan_position = 0; // Center

  // ═══════════════════════════════════════════════════════════
  // Step 3: Create audio stream (Casey's secondary buffer)
  // ═══════════════════════════════════════════════════════════
  //
  // LoadAudioStream parameters:
  //   - sampleRate:    48000 Hz (CD quality is 44100)
  //   - sampleSize:    16 bits per sample
  //   - channels:      2 (stereo)
  //
  // Raylib creates a ring buffer internally (like DirectSound)
  // ═══════════════════════════════════════════════════════════

  g_sound_output.stream =
      LoadAudioStream(g_sound_output.samples_per_second, // 48000 Hz
                      16,                                // 16-bit samples
                      2                                  // Stereo
      );

  // ═══════════════════════════════════════════════════════════
  // Step 4: Set buffer size (Casey's latency calculation)
  // ═══════════════════════════════════════════════════════════
  // 4096 frames = ~85ms at 48kHz (similar to Casey's 1/15 sec)
  SetAudioStreamBufferSizeDefault(4096);

  // ═══════════════════════════════════════════════════════════
  // Step 5: Attach callback (automatic audio generation!)
  // ═══════════════════════════════════════════════════════════
  // Raylib will call raylib_audio_callback() when buffer needs data
  // This replaces your manual linux_fill_sound_buffer() call
  SetAudioStreamCallback(g_sound_output.stream, raylib_audio_callback);

  // ═══════════════════════════════════════════════════════════
  // Step 6: Start playback (Casey's IDirectSoundBuffer->Play())
  // ═══════════════════════════════════════════════════════════
  PlayAudioStream(g_sound_output.stream);

  g_sound_output.is_initialized = true;

  printf("✅ Audio: Initialized with sine wave!\n");
  printf("   Sample rate:  %d Hz\n", g_sound_output.samples_per_second);
  printf("   Tone:         %d Hz (sine wave)\n", g_sound_output.tone_hz);
  printf("   Wave period:  %d samples\n", g_sound_output.wave_period);
  printf("   Latency:      %d samples (~%.1f ms)\n",
         g_sound_output.latency_sample_count,
         (float)g_sound_output.latency_sample_count /
             g_sound_output.samples_per_second * 1000.0f);
  printf("   Buffer:       4096 frames (~85 ms)\n");
}

// ═══════════════════════════════════════════════════════════════
// 🎵 DAY 8: SOUND CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════════════
// These mirror your X11 backend functions exactly
// ═══════════════════════════════════════════════════════════════

inline void set_tone_frequency(int hz) {
  if (!g_sound_output.is_initialized)
    return;

  g_sound_output.tone_hz = hz;
  g_sound_output.wave_period =
      g_sound_output.samples_per_second / g_sound_output.tone_hz;

  // Optional: reset phase to avoid clicks
  // g_sound_output.t_sine = 0.0f;  // Uncomment if you hear clicks
}

inline void handle_update_tone_frequency(int hz_to_add) {
  int new_hz = g_sound_output.tone_hz + hz_to_add;

  // Clamp to safe range
  if (new_hz < 60)
    new_hz = 60;
  if (new_hz > 1000)
    new_hz = 1000;

  set_tone_frequency(new_hz);

  printf("🎵 Tone frequency: %d Hz (period: %d samples)\n", new_hz,
         g_sound_output.wave_period);
}

inline void handle_increase_volume(int num) {
  printf("lol");
  int new_volume = g_sound_output.tone_volume + num;

  // Clamp to safe range
  if (new_volume < 0)
    new_volume = 0;
  if (new_volume > 15000)
    new_volume = 15000;

  g_sound_output.tone_volume = new_volume;
  printf("🔊 Volume: %d / %d (%.1f%%)\n", new_volume, 15000,
         (new_volume * 100.0f) / 15000);
}

inline void handle_musical_keypress(int key) {
  printf("lol");
  // Musical note frequencies (same as X11 version)
  switch (key) {
  case KEY_Z:
    set_tone_frequency(262);
    printf("🎵 Note: C4 (261.63 Hz)\n");
    break;
  case KEY_X:
    set_tone_frequency(294);
    printf("🎵 Note: D4 (293.66 Hz)\n");
    break;
  case KEY_C:
    set_tone_frequency(330);
    printf("🎵 Note: E4 (329.63 Hz)\n");
    break;
  case KEY_V:
    set_tone_frequency(349);
    printf("🎵 Note: F4 (349.23 Hz)\n");
    break;
  case KEY_B:
    set_tone_frequency(392);
    printf("🎵 Note: G4 (392.00 Hz)\n");
    break;
  case KEY_N:
    set_tone_frequency(440);
    printf("🎵 Note: A4 (440.00 Hz) - Concert Pitch\n");
    break;
  case KEY_M:
    set_tone_frequency(494);
    printf("🎵 Note: B4 (493.88 Hz)\n");
    break;
  case KEY_COMMA:
    set_tone_frequency(523);
    printf("🎵 Note: C5 (523.25 Hz)\n");
    break;
  }
}

inline void handle_increase_pan(int num) {
  printf("lol");
  int new_pan = g_sound_output.pan_position + num;
  if (new_pan < -100)
    new_pan = -100;
  if (new_pan > 100)
    new_pan = 100;

  g_sound_output.pan_position = new_pan;

  // Visual indicator (same as X11)
  char indicator[50] = {0};
  int pos = (g_sound_output.pan_position + 100) * 20 / 200;
  for (int i = 0; i < 21; i++) {
    indicator[i] = (i == pos) ? '*' : '-';
  }
  indicator[21] = '\0';

  printf("🎧 Pan: %s %+d\n", indicator, g_sound_output.pan_position);
  printf("    L ◀%s▶ R\n", indicator);
}

void raylib_debug_audio(void) {
  if (!g_sound_output.is_initialized) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 Raylib Audio Debug Info                              │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");
  printf("│ Mode: Callback-based (automatic latency control)       │\n");
  printf("│                                                         │\n");
  printf("│ Sample rate:       %d Hz                                 │\n",
         g_sound_output.samples_per_second);
  printf("│ Frequency:         %d Hz                                 │\n",
         g_sound_output.tone_hz);
  printf("│ Volume:            %d / 15000                            │\n",
         g_sound_output.tone_volume);
  printf("│ Pan:               %+d (L=%d, R=%d)                      │\n",
         g_sound_output.pan_position, 100 - g_sound_output.pan_position,
         100 + g_sound_output.pan_position);
  printf("└─────────────────────────────────────────────────────────┘\n");
}
