/**
 * @fileoverview
 *
 * # Handmade Hero - Day 7 & 8 Audio System Explained
 *
 * ## 🔊 What Day 7 Is Really About
 *
 * Casey introduces **audio output** using DirectSound. Here's the mental model:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    AUDIO PIPELINE (Day 7)                       │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Your Game          DirectSound           Sound Card            │
 * │  ┌─────────┐       ┌───────────┐         ┌──────────┐          │
 * │  │ Generate │  →   │ Secondary │    →    │ DAC      │  → 🔊    │
 * │  │ Samples  │      │ Buffer    │         │ (Digital │          │
 * │  │ (48kHz)  │      │ (Ring)    │         │  to      │          │
 * │  └─────────┘       └───────────┘         │ Analog)  │          │
 * │                          ↑               └──────────┘          │
 * │                          │                                      │
 * │                    ┌───────────┐                                │
 * │                    │ Primary   │ ← Sets format (48kHz, 16-bit) │
 * │                    │ Buffer    │                                │
 * │                    └───────────┘                                │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ### Key Concepts
 *
 * | Concept | What It Is | Web Analogy |
 * |---------|------------|-------------|
 * | Sample Rate | 48000 samples/second | Like FPS but for audio |
 * | Buffer Size | Ring buffer for audio data | Like a streaming buffer |
 * | Primary Buffer | Sets the audio format | Like `audioContext.sampleRate` |
 * | Secondary Buffer | Where you write samples | Like `AudioBuffer` in Web
 * Audio | | Cooperative Level | How you share the sound card | Like exclusive
 * fullscreen mode |
 *
 * ### Linux Audio System (ALSA)
 *
 * This is the BIG addition for Day 7. On Linux, we use **ALSA** (Advanced Linux
 * Sound Architecture) instead of DirectSound.
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              WINDOWS vs LINUX AUDIO                             │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Windows:                      Linux:                           │
 * │  ┌──────────────┐             ┌──────────────┐                 │
 * │  │ DirectSound  │             │ ALSA         │                 │
 * │  │ dsound.dll   │             │ libasound    │                 │
 * │  └──────────────┘             └──────────────┘                 │
 * │         ↓                            ↓                          │
 * │  LoadLibrary()               dlopen() or link directly          │
 * │  DirectSoundCreate()         snd_pcm_open()                    │
 * │  SetCooperativeLevel()       snd_pcm_set_params()              │
 * │  CreateSoundBuffer()         snd_pcm_writei()                  │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## 🔊 Day 8 - Understanding the Ring Buffer
 *
 * This is **THE core concept** of Day 8. Let me explain it visually:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      DIRECTSOUND RING BUFFER                            │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  The buffer is circular - when you reach the end, it wraps to start!   │
 * │                                                                         │
 * │  ┌────────────────────────────────────────────────────────────┐        │
 * │  │ 0                                              BufferSize  │        │
 * │  │ ├──────────────────────────────────────────────────────────┤        │
 * │  │ │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│        │
 * │  │ │              ↑                    ↑                      │        │
 * │  │ │         PlayCursor           WriteCursor                 │        │
 * │  │ │         (hardware             (safe to                   │        │
 * │  │ │          reading)              write here)               │        │
 * │  │ └──────────────────────────────────────────────────────────┘        │
 * │  │                                                                      │
 * │  │  ▓▓▓ = Audio being played (DON'T TOUCH!)                            │
 * │  │  ░░░ = Safe to write new audio data                                  │
 * │                                                                         │
 * │  WRAP-AROUND CASE:                                                      │
 * │  ┌────────────────────────────────────────────────────────────┐        │
 * │  │░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░│        │
 * │  │         ↑                                  ↑               │        │
 * │  │    PlayCursor                         ByteToLock           │        │
 * │  │                                                            │        │
 * │  │  Region2 ◄────────────────────────────────► Region1        │        │
 * │  │  (start of buffer)                    (end of buffer)      │        │
 * │  └────────────────────────────────────────────────────────────┘        │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ### Square Wave Generation
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      SQUARE WAVE (256 Hz)                               │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  ToneVolume (+3000)                                                     │
 * │      ┌────┐    ┌────┐    ┌────┐    ┌────┐                              │
 * │      │    │    │    │    │    │    │    │                              │
 * │  ────┘    └────┘    └────┘    └────┘    └────                          │
 * │                                                                         │
 * │  -ToneVolume (-3000)                                                    │
 * │                                                                         │
 * │  ◄──────────────────────────────────────────►                          │
 * │           One period = 48000/256 = 187.5 samples                        │
 * │                                                                         │
 * │  Casey's formula:                                                       │
 * │  SampleValue = ((RunningSampleIndex / HalfPeriod) % 2)                 │
 * │                ? ToneVolume : -ToneVolume                               │
 * │                                                                         │
 * │  Sample 0-93:   +3000  (first half)                                    │
 * │  Sample 94-187: -3000  (second half)                                   │
 * │  Sample 188:    +3000  (next period starts)                            │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 */
#include "audio.h"
#include "../base.h"
#include <dlfcn.h> // For dlopen, dlsym, dlclose (Casey's LoadLibrary equivalent)
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // M_PI

#ifndef M_double_PI
#define M_double_PI (2.f * M_PI)
#endif // M_double_PI

// ═══════════════════════════════════════════════════════════════
// 🔊 ALSA AUDIO DYNAMIC LOADING (Casey's DirectSound Pattern)
// ═══════════════════════════════════════════════════════════════
//
// Casey dynamically loads dsound.dll using LoadLibrary/GetProcAddress.
// We do the same with dlopen/dlsym for libasound.so.
//
// WHY DYNAMIC LOADING?
// 1. Program doesn't crash if ALSA isn't installed
// 2. We can gracefully fall back to no audio
// 3. No compile-time dependency on -lasound
// 4. Exactly mirrors Casey's Win32 approach
//
// ═══════════════════════════════════════════════════════════════

// ───────────────────────────────────────────────────────────────
// Stub Implementations (used when ALSA not available)
// ───────────────────────────────────────────────────────────────

ALSA_SND_PCM_OPEN(AlsaSndPcmOpenStub) {
  /*
   * The (void) casts for the parameters 'pcm', 'device', 'stream', and 'mode'
   * are used to explicitly mark them as unused in this stub implementation.
   * This prevents compiler warnings about unused parameters, making it clear
   * that their omission is intentional in this context.
   */
  (void)pcm;
  (void)device;
  (void)stream;
  (void)mode;
  return -1; // Error: no device
}

ALSA_SND_PCM_SET_PARAMS(AlsaSndPcmSetParamsStub) {
  (void)pcm;
  (void)format;
  (void)access;
  (void)channels;
  (void)rate;
  (void)soft_resample;
  (void)latency;
  return -1;
}

ALSA_SND_PCM_WRITEI(AlsaSndPcmWriteiStub) {
  (void)pcm;
  (void)buffer;
  (void)frames;
  return 0; // Pretend we wrote 0 frames
}

ALSA_SND_PCM_PREPARE(AlsaSndPcmPrepareStub) {
  (void)pcm;
  return -1;
}

ALSA_SND_PCM_CLOSE(AlsaSndPcmCloseStub) {
  (void)pcm;
  return 0;
}

ALSA_SND_STRERROR(AlsaSndStrerrorStub) {
  (void)errnum;
  return "ALSA not loaded";
}

ALSA_SND_PCM_AVAIL(AlsaSndPcmAvailStub) {
  (void)pcm;
  return 0;
}

ALSA_SND_PCM_RECOVER(AlsaSndPcmRecoverStub) {
  (void)pcm;
  (void)err;
  (void)silent;
  return -1;
}

ALSA_SND_PCM_DELAY(AlsaSndPcmDelayStub) {
  (void)pcm;
  (void)delayp;
  return -1; // Error: not available
}

// ───────────────────────────────────────────────────────────────
// Global Function Pointers (initially point to stubs)
// ───────────────────────────────────────────────────────────────

alsa_snd_pcm_open *SndPcmOpen_ = AlsaSndPcmOpenStub;
alsa_snd_pcm_set_params *SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
alsa_snd_pcm_writei *SndPcmWritei_ = AlsaSndPcmWriteiStub;
alsa_snd_pcm_prepare *SndPcmPrepare_ = AlsaSndPcmPrepareStub;
alsa_snd_pcm_close *SndPcmClose_ = AlsaSndPcmCloseStub;
alsa_snd_strerror *SndStrerror_ = AlsaSndStrerrorStub;
alsa_snd_pcm_avail *SndPcmAvail_ = AlsaSndPcmAvailStub;
alsa_snd_pcm_recover *SndPcmRecover_ = AlsaSndPcmRecoverStub;
alsa_snd_pcm_delay *SndPcmDelay_ = AlsaSndPcmDelayStub;

// ───────────────────────────────────────────────────────────────
// Sound Output State
// ───────────────────────────────────────────────────────────────
LinuxSoundOutput g_sound_output = {0};

// ═══════════════════════════════════════════════════════════════
// 🔊 Load ALSA Library (Casey's Win32LoadXInput equivalent)
// ═══════════════════════════════════════════════════════════════
//
// This mirrors Casey's Win32LoadXInput() function exactly:
// 1. Try to load the library
// 2. If found, get function pointers
// 3. If not found, stubs remain in place
//
// ═══════════════════════════════════════════════════════════════

void linux_load_alsa(void) {
  printf("Loading ALSA library...\n");

  // ───────────────────────────────────────────────────────────
  // Try to load libasound.so (Casey's LoadLibrary equivalent)
  // ───────────────────────────────────────────────────────────
  //
  // Try multiple names (like Casey tries xinput1_4.dll, then xinput1_3.dll)
  //
  // RTLD_LAZY = Only resolve symbols when called (faster load)
  // RTLD_NOW  = Resolve all symbols immediately (catches errors early)
  // ───────────────────────────────────────────────────────────

  void *alsa_lib = dlopen("libasound.so.2", RTLD_LAZY);
  if (!alsa_lib) {
    alsa_lib = dlopen("libasound.so", RTLD_LAZY);
  }

  if (!alsa_lib) {
    fprintf(stderr, "❌ ALSA: Could not load libasound.so: %s\n", dlerror());
    fprintf(stderr,
            "   Audio disabled. Install: sudo apt install libasound2\n");
    return; // Stubs remain in place - audio just won't work
  }

  printf("✅ ALSA: Loaded libasound.so\n");
  g_sound_output.alsa_library = alsa_lib;

  // ───────────────────────────────────────────────────────────
  // Get function pointers (Casey's GetProcAddress equivalent)
  // ───────────────────────────────────────────────────────────

// Helper macro to reduce repetition (like Casey does implicitly)
#define LOAD_ALSA_FN(fn_ptr, fn_name, type)                                    \
  do {                                                                         \
    fn_ptr = (type *)dlsym(alsa_lib, fn_name);                                 \
    if (!fn_ptr) {                                                             \
      fprintf(stderr, "❌ ALSA: Could not load %s\n", fn_name);                \
    } else {                                                                   \
      printf("   ✓ Loaded %s\n", fn_name);                                     \
    }                                                                          \
  } while (0)

  LOAD_ALSA_FN(SndPcmOpen_, "snd_pcm_open", alsa_snd_pcm_open);
  LOAD_ALSA_FN(SndPcmSetParams_, "snd_pcm_set_params", alsa_snd_pcm_set_params);
  LOAD_ALSA_FN(SndPcmWritei_, "snd_pcm_writei", alsa_snd_pcm_writei);
  LOAD_ALSA_FN(SndPcmPrepare_, "snd_pcm_prepare", alsa_snd_pcm_prepare);
  LOAD_ALSA_FN(SndPcmClose_, "snd_pcm_close", alsa_snd_pcm_close);
  LOAD_ALSA_FN(SndStrerror_, "snd_strerror", alsa_snd_strerror);
  LOAD_ALSA_FN(SndPcmAvail_, "snd_pcm_avail", alsa_snd_pcm_avail);
  LOAD_ALSA_FN(SndPcmRecover_, "snd_pcm_recover", alsa_snd_pcm_recover);
  LOAD_ALSA_FN(SndPcmDelay_, "snd_pcm_delay", alsa_snd_pcm_delay);
#undef LOAD_ALSA_FN

  // Verify critical functions loaded
  if (SndPcmOpen_ == AlsaSndPcmOpenStub ||
      SndPcmSetParams_ == AlsaSndPcmSetParamsStub
      // || SndPcmWritei_ == AlsaSndPcmWriteiStub
  ) {
    fprintf(stderr, "❌ ALSA: Missing critical functions, audio disabled\n");
    // Reset to stubs
    SndPcmOpen_ = AlsaSndPcmOpenStub;
    SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
    SndPcmWritei_ = AlsaSndPcmWriteiStub;
    dlclose(alsa_lib);
    g_sound_output.alsa_library = NULL;
  }

  // DAY 10: Check if latency measurement is available
  if (SndPcmDelay_ == AlsaSndPcmDelayStub) {
    printf("⚠️  ALSA: snd_pcm_delay not available\n");
    printf("    Day 10 latency measurement disabled\n");
    printf("    Falling back to Day 9 behavior\n");
  } else {
    printf("✓ ALSA: Day 10 latency measurement available\n");
  }

  printf("✓ ALSA library loaded successfully\n");
}

// ═══════════════════════════════════════════════════════════════
// 🔊 Initialize Sound (Casey's Win32InitDSound equivalent)
// ═══════════════════════════════════════════════════════════════
//
// Casey's DirectSound setup:
// 1. DirectSoundCreate()         → SndPcmOpen()
// 2. SetCooperativeLevel()       → (not needed in ALSA)
// 3. Create primary buffer       → (format set via snd_pcm_set_params)
// 4. Set primary format          → snd_pcm_set_params()
// 5. Create secondary buffer     → (ALSA manages internally)
//
// ═══════════════════════════════════════════════════════════════

void linux_init_sound(int32_t samples_per_second, int32_t buffer_size_bytes) {
  printf("Initializing sound output...\n");

  // ───────────────────────────────────────────────────────────
  // STEP 1: Open the PCM device
  // ───────────────────────────────────────────────────────────
  // Casey: DirectSoundCreate()
  // Linux: snd_pcm_open()
  //
  // "default" = system default audio device
  //             (PulseAudio will intercept this on most systems)
  // ───────────────────────────────────────────────────────────

  int err = SndPcmOpen(&g_sound_output.handle,
                       "default",                     // Device
                       LINUX_SND_PCM_STREAM_PLAYBACK, // Output
                       0);                            // Blocking mode

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot open audio device: %s\n",
            SndStrerror(err));
    g_sound_output.is_valid = false;
    return;
  }

  printf("✅ Sound: Opened audio device\n");

  // ───────────────────────────────────────────────────────────
  // STEP 2: Set format parameters
  // ───────────────────────────────────────────────────────────
  // Casey creates WAVEFORMATEX:
  //   wBitsPerSample = 16
  //   nChannels = 2
  //   nSamplesPerSec = 48000
  //   nBlockAlign = 4 (2 channels × 2 bytes)
  //
  // We use snd_pcm_set_params() which does it all in one call
  // ───────────────────────────────────────────────────────────

  // Latency calculation:
  // Day 8: Use shorter latency for better responsiveness
  // Casey uses ~66ms (1/15 second), we'll use 50ms
  unsigned int latency_us = 50000; // 50ms in microseconds

  err = SndPcmSetParams(g_sound_output.handle,
                        LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed
                        LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // L-R-L-R
                        2,                                   // Stereo
                        samples_per_second,                  // 48000 Hz
                        1,                                   // Allow resample
                        latency_us);                         // 100ms buffer

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot set parameters: %s\n", SndStrerror(err));
    SndPcmClose(g_sound_output.handle);
    g_sound_output.is_valid = false;
    return;
  }

  printf("✅ Sound: Format set to %d Hz, 16-bit stereo\n", samples_per_second);

  // ───────────────────────────────────────────────────────────
  // STEP 3: Store parameters for later use
  // ───────────────────────────────────────────────────────────

  g_sound_output.samples_per_second = samples_per_second;
  g_sound_output.bytes_per_sample =
      sizeof(int16_t) * 2; // 16-bit stereo = 4 bytes
  g_sound_output.buffer_size = buffer_size_bytes;

  // ═══════════════════════════════════════════════════════════
  // Day 8: Allocate sample buffer (Casey's secondary buffer)
  // ═══════════════════════════════════════════════════════════
  //
  // We need a buffer to generate samples into before writing to ALSA.
  // Size: 1/15 second of audio (like Casey's buffer)
  //
  // samples_per_second / 15 = frames per write
  // × 2 channels × 2 bytes = buffer size
  // ═══════════════════════════════════════════════════════════

  g_sound_output.sample_buffer_size = samples_per_second / 15; // ~3200 frames
  int sample_buffer_bytes =
      g_sound_output.sample_buffer_size * g_sound_output.bytes_per_sample;

  g_sound_output.sample_buffer =
      (int16_t *)mmap(NULL, sample_buffer_bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (g_sound_output.sample_buffer == MAP_FAILED) {
    fprintf(stderr, "❌ Sound: Cannot allocate sample buffer\n");
    SndPcmClose(g_sound_output.handle);
    g_sound_output.is_valid = false;
    return;
  }

  // ═══════════════════════════════════════════════════════════
  // Day 8: Initialize sound generation parameters
  // ═══════════════════════════════════════════════════════════
  //
  // Casey's Day 8 values:
  //   ToneHz = 256 (middle C-ish)
  //   ToneVolume = 3000 (amplitude)
  //   SquareWavePeriod = SamplesPerSecond / ToneHz
  // ═══════════════════════════════════════════════════════════
  g_sound_output.running_sample_index = 0;
  g_sound_output.tone_hz = 256;
  g_sound_output.tone_volume = 6000;
  g_sound_output.wave_period = samples_per_second / g_sound_output.tone_hz;
  // g_sound_output.half_wave_period = g_sound_output.wave_period / 2;

  // Latency calculation
  g_sound_output.t_sine = 0;

  // Latency (1/15 second like Casey)
  g_sound_output.latency_sample_count = samples_per_second / 15;

  g_sound_output.pan_position = 0;

  g_sound_output.is_valid = true;

  printf("✅ Sound: Initialized!\n");
  printf("   Sample rate:    %d Hz\n", samples_per_second);
  printf("   Buffer size:    %d frames (~%.1f ms)\n",
         g_sound_output.sample_buffer_size,
         (float)g_sound_output.sample_buffer_size / samples_per_second *
             1000.0f);
  printf("   Tone frequency: %d Hz\n", g_sound_output.tone_hz);
  printf("   Wave period:    %d samples\n", g_sound_output.wave_period);
  printf("   Sample rate:  %d Hz\n", samples_per_second);
  printf("   Buffer size:  %d bytes\n", buffer_size_bytes);
  printf("   Latency:      %.1f ms\n", latency_us / 1000.0f);
}

file_scoped_fn inline bool linux_audio_has_latency_measurement(void) {
  return SndPcmDelay_ != AlsaSndPcmDelayStub;
}

// ═══════════════════════════════════════════════════════════════
// Day 8: Fill Sound Buffer with Square Wave
// ═══════════════════════════════════════════════════════════════
//
// This is the Linux equivalent of Casey's Lock/Write/Unlock pattern.
//
// DIFFERENCE FROM DIRECTSOUND:
// - DirectSound: Lock buffer → write → unlock → hardware plays
// - ALSA: Fill our buffer → snd_pcm_writei() copies to hardware
//
// ALSA is simpler because it manages the ring buffer for us!
// ═══════════════════════════════════════════════════════════════
//
// ═══════════════════════════════════════════════════════════════
// 🔊 LINUX AUDIO BUFFER FILLING (Day 9 + Day 10)
// ═══════════════════════════════════════════════════════════════
//
// This function has TWO MODES:
//
// MODE 1 (Day 10 - Latency-Aware):
//   - IF snd_pcm_delay is available
//   - Measure current latency
//   - Calculate exactly how much to write to maintain target
//   - Write only what's needed
//
// MODE 2 (Day 9 - Availability-Based):
//   - IF snd_pcm_delay is NOT available
//   - Fill based on snd_pcm_avail() only
//   - Write as much as ALSA allows (up to our buffer size)
//
// This graceful degradation is Casey's pattern!
void linux_fill_sound_buffer(void) {
  if (!g_sound_output.is_valid) {
    return;
  }

  // ───────────────────────────────────────────────────────────
  // STEP 1: Query available frames (BOTH modes need this)
  // ───────────────────────────────────────────────────────────

  long frames_available = SndPcmAvail(g_sound_output.handle);

  if (frames_available < 0) {
    // Error (probably underrun)
    int err = SndPcmRecover(g_sound_output.handle, (int)frames_available, 1);
    if (err < 0) {
      fprintf(stderr, "❌ Sound: Recovery failed: %s\n", SndStrerror(err));
      return;
    }
    frames_available = SndPcmAvail(g_sound_output.handle);
    if (frames_available < 0) {
      return;
    }
  }

  // ───────────────────────────────────────────────────────────
  // STEP 2: Calculate how many frames to write
  // ───────────────────────────────────────────────────────────
  // This is where Day 9 and Day 10 diverge!
  // ───────────────────────────────────────────────────────────

  long frames_to_write = 0;

  if (linux_audio_has_latency_measurement()) {
    // ═══════════════════════════════════════════════════════
    // 🆕 MODE 1: DAY 10 - LATENCY-AWARE FILLING
    // ═══════════════════════════════════════════════════════

    snd_pcm_sframes_t delay_frames = 0;
    int delay_err = SndPcmDelay(g_sound_output.handle, &delay_frames);

    if (delay_err < 0) {
      // Delay query failed - device might be in bad state
      if (delay_err == -EPIPE) {
        // Underrun - recover and assume empty buffer
        SndPcmRecover(g_sound_output.handle, delay_err, 1);
        delay_frames = 0;
      } else {
        // Other error - skip this frame
        return;
      }
    }

    // Calculate how much we need to reach target latency
    long target_queued = g_sound_output.latency_sample_count;
    long current_queued = delay_frames;
    long frames_needed = target_queued - current_queued;

// Optional debug logging
#if 0
        static int frame_count = 0;
        if (frame_count++ % 60 == 0) {
            float actual_ms = (float)delay_frames / 
                             g_sound_output.samples_per_second * 1000.0f;
            float target_ms = (float)target_queued / 
                             g_sound_output.samples_per_second * 1000.0f;
            printf("🔊 [Day 10] Latency: %.1fms (target: %.1fms), "
                   "need %ld frames\n",
                   actual_ms, target_ms, frames_needed);
        }
#endif

    // Clamp to available space and buffer size
    if (frames_needed < 0) {
      frames_needed = 0; // Already at or above target
    }
    if (frames_needed > frames_available) {
      frames_needed = frames_available;
    }
    if (frames_needed > (long)g_sound_output.sample_buffer_size) {
      frames_needed = g_sound_output.sample_buffer_size;
    }

    frames_to_write = frames_needed;

  } else {
    // ═══════════════════════════════════════════════════════
    // MODE 2: DAY 9 - AVAILABILITY-BASED FILLING
    // ═══════════════════════════════════════════════════════

    static bool warned_once = false;
    if (!warned_once) {
      printf("ℹ️  [Day 9 Mode] Using availability-based filling\n");
      warned_once = true;
    }

    // Fill as much as available (up to our buffer size)
    frames_to_write = frames_available;

    if (frames_to_write > (long)g_sound_output.sample_buffer_size) {
      frames_to_write = g_sound_output.sample_buffer_size;
    }
  }

  // ───────────────────────────────────────────────────────────
  // STEP 3: Early exit if nothing to write
  // ───────────────────────────────────────────────────────────

  if (frames_to_write <= 0) {
    return;
  }

  // ───────────────────────────────────────────────────────────
  // STEP 4: Generate samples (SAME for both modes)
  // ───────────────────────────────────────────────────────────

  int16_t *sample_out = g_sound_output.sample_buffer;

  for (long i = 0; i < frames_to_write; ++i) {
    // Sine wave generation
    real32 sine_value = sinf(g_sound_output.t_sine);
    int16_t sample_value = (int16_t)(sine_value * g_sound_output.tone_volume);

    // Panning
    int left_gain = (100 - g_sound_output.pan_position);
    int right_gain = (100 + g_sound_output.pan_position);

    *sample_out++ = (sample_value * left_gain) / 200;  // Left
    *sample_out++ = (sample_value * right_gain) / 200; // Right

    // Phase increment
    g_sound_output.t_sine += M_double_PI / (float)g_sound_output.wave_period;

    if (g_sound_output.t_sine >= M_double_PI) {
      g_sound_output.t_sine -= M_double_PI;
    }

    g_sound_output.running_sample_index++;
  }

  // ───────────────────────────────────────────────────────────
  // STEP 5: Write to ALSA (SAME for both modes)
  // ───────────────────────────────────────────────────────────

  long frames_written = SndPcmWritei(
      g_sound_output.handle, g_sound_output.sample_buffer, frames_to_write);

  if (frames_written < 0) {
    frames_written =
        SndPcmRecover(g_sound_output.handle, (int)frames_written, 1);
    if (frames_written < 0) {
      fprintf(stderr, "❌ Sound: Write failed: %s\n",
              SndStrerror((int)frames_written));
    }
  }

// Optional verification
#if 0
    if (frames_written > 0 && frames_written != frames_to_write) {
        printf("⚠️ Partial write: wanted %ld, wrote %ld\n",
               frames_to_write, frames_written);
    }
#endif
}

// ═══════════════════════════════════════════════════════════════
// 🔊 DAY 10: Audio Latency Debug Helper
// ═══════════════════════════════════════════════════════════════
void linux_debug_audio_latency(void) {
  if (!g_sound_output.is_valid) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 Audio Debug Info                                     │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");

  // Check if Day 10 features available
  if (!linux_audio_has_latency_measurement()) {
    printf("│ ⚠️  Mode: Day 9 (Availability-Based)                    │\n");
    printf("│                                                         │\n");
    printf("│ snd_pcm_delay not available                             │\n");
    printf("│ Latency measurement disabled                            │\n");
    printf("│                                                         │\n");

    long frames_available = SndPcmAvail(g_sound_output.handle);

    printf("│ Frames available: %ld                                   │\n",
           frames_available);
    printf("│ Sample rate:     %d Hz                                 │\n",
           g_sound_output.samples_per_second);
    printf("│ Frequency:       %d Hz                                 │\n",
           g_sound_output.tone_hz);
    printf("│ Volume:          %d / 15000                            │\n",
           g_sound_output.tone_volume);
    printf("│ Pan:             %+d (L=%d, R=%d)                      │\n",
           g_sound_output.pan_position, 100 - g_sound_output.pan_position,
           100 + g_sound_output.pan_position);
    printf("└─────────────────────────────────────────────────────────┘\n");
    return;
  }

  // Day 10 mode - full latency stats
  printf("│ ✅ Mode: Day 10 (Latency-Aware)                          │\n");
  printf("│                                                         │\n");

  snd_pcm_sframes_t delay_frames = 0;
  int err = SndPcmDelay(g_sound_output.handle, &delay_frames);

  if (err < 0) {
    printf("│ ❌ Can't measure delay: %s                              │\n",
           SndStrerror(err));
    printf("└─────────────────────────────────────────────────────────┘\n");
    return;
  }

  long frames_available = SndPcmAvail(g_sound_output.handle);

  float actual_latency_ms =
      (float)delay_frames / g_sound_output.samples_per_second * 1000.0f;
  float target_latency_ms = (float)g_sound_output.latency_sample_count /
                            g_sound_output.samples_per_second * 1000.0f;

  printf("│ Target latency:  %.1f ms (%d frames)                 │\n",
         target_latency_ms, g_sound_output.latency_sample_count);
  printf("│ Actual latency:  %.1f ms (%ld frames)                │\n",
         actual_latency_ms, (long)delay_frames);

  // Color-code latency status
  float latency_diff = actual_latency_ms - target_latency_ms;
  if (fabs(latency_diff) < 5.0f) {
    printf("│ Status:          ✅ GOOD (±%.1fms)                       │\n",
           latency_diff);
  } else if (fabs(latency_diff) < 10.0f) {
    printf("│ Status:          ⚠️  OK (±%.1fms)                         │\n",
           latency_diff);
  } else {
    printf("│ Status:          ❌ BAD (±%.1fms)                         │\n",
           latency_diff);
  }

  printf("│                                                         │\n");
  printf("│ Frames available: %ld                                   │\n",
         frames_available);
  printf("│ Sample rate:     %d Hz                                 │\n",
         g_sound_output.samples_per_second);
  printf("│ Frequency:       %d Hz                                 │\n",
         g_sound_output.tone_hz);
  printf("│ Volume:          %d / 15000                            │\n",
         g_sound_output.tone_volume);
  printf("│ Pan:             %+d (L=%d, R=%d)                      │\n",
         g_sound_output.pan_position, 100 - g_sound_output.pan_position,
         100 + g_sound_output.pan_position);
  printf("└─────────────────────────────────────────────────────────┘\n");
}
