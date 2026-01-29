/**
 * @file audio.c
 * @brief ALSA Audio System for Linux (Handmade Hero Days 7-20)
 *
 * This file implements the Linux audio subsystem using ALSA (Advanced Linux
 * Sound Architecture), following Casey Muratori's Handmade Hero architecture.
 *
 * @section overview Overview
 *
 * The audio system provides low-latency sound output synchronized with game
 * logic. Unlike DirectSound on Windows which exposes play/write cursors
 * directly, ALSA uses a different model where we query delay and available
 * space, then calculate virtual cursor positions.
 *
 * @section architecture Architecture
 *
 * **Dynamic Loading (Day 7 pattern):**
 * ALSA is loaded at runtime via dlopen/dlsym, mirroring Casey's LoadLibrary
 * pattern for DirectSound. This allows graceful degradation if audio fails.
 *
 * **Ring Buffer Model:**
 * - ALSA manages a 2-second circular buffer (96000 samples at 48kHz)
 * - We write ahead of playback to prevent underruns
 * - Virtual cursors are calculated from running_sample_index and delay
 *
 * **Frame-Aligned Audio (Day 19-20):**
 * - Audio writes sync with game update rate (not render rate)
 * - Target: maintain 2 frames + safety margin buffered ahead
 * - Safety margin = 1/3 frame (~533 samples at 30Hz)
 *
 * @section cursors Virtual Cursor Calculation
 *
 * DirectSound provides GetCurrentPosition() returning play/write cursors.
 * ALSA provides snd_pcm_delay() and snd_pcm_avail() instead:
 *
 * ```
 * play_cursor  = running_sample_index - delay_frames
 * write_cursor = running_sample_index
 * avail_frames = how much space available for writing
 * ```
 *
 * @section debug Debug Visualization (Day 20)
 *
 * The debug system records 15 frames of audio timing data:
 * - Output cursors: captured before each audio write
 * - Flip cursors: captured after frame display
 * - Allows comparing predicted vs actual playback position
 *
 * @section differences Key Differences from DirectSound
 *
 * | DirectSound                  | ALSA                              |
 * |------------------------------|-----------------------------------|
 * | Lock/Unlock buffer regions   | Single snd_pcm_writei() call      |
 * | GetCurrentPosition()         | Calculate from delay + avail      |
 * | Explicit ring buffer math    | ALSA handles wrap-around          |
 * | DSBCAPS_GETCURRENTPOSITION2  | snd_pcm_delay() for latency       |
 *
 * @section gotchas GOTCHAS & LESSONS LEARNED
 *
 * **1. Buffer Reallocation Bug:**
 * DO NOT call platform_allocate_memory() twice for the same buffer!
 * The second call replaces the pointer, causing the original large buffer
 * to be lost. If the second allocation is smaller, you get buffer overflow.
 * FIX: Allocate once with correct size, use memset() to zero portions.
 *
 * **2. Debug Marker Index Management:**
 * g_debug_marker_index points to the NEXT slot to fill, not the current one.
 * When displaying, use (g_debug_marker_index - 1 + MAX) % MAX to get the
 * most recently filled marker. Casey does this in Win32DebugSyncDisplay.
 *
 * **3. Audio Rate vs Render Rate:**
 * Audio should run at a FIXED rate (e.g., 30Hz) independent of rendering FPS.
 * When adaptive FPS changes, DON'T reset the audio buffer - just adjust
 * samples_per_frame calculation. Resetting causes audio clicks.
 *
 * **4. Power Save Mode Variance:**
 * Laptops in power save can have 25-80ms frame times instead of 16ms.
 * Solution: Buffer 100ms+ of audio to absorb jitter, not just 2 frames.
 *
 * **5. ALSA Blocking vs Non-Blocking:**
 * DON'T use SND_PCM_NONBLOCK - it causes random -EAGAIN errors.
 * Use blocking mode (0) for reliable writes.
 *
 * **6. Linux mmap() Already Zeros Memory:**
 * MAP_ANONYMOUS guarantees zero-initialized pages on Linux.
 * PLATFORM_MEMORY_ZEROED flag is mainly for documentation/Windows compat.
 *
 * @see _ignore/handmade_hero/.../handmade_hero_day_020_source/ for reference
 * @see https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html
 */

#include "audio.h"
#include <dlfcn.h> // For dlopen, dlsym, dlclose (Casey's LoadLibrary equivalent)
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #include <alsa/asoundlib.h>

#if DE100_INTERNAL
#include "../../game/base.h"
#endif

#if DE100_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 📊 DEBUG AUDIO MARKER TRACKING
// ═══════════════════════════════════════════════════════════════
LinuxDebugAudioMarker g_debug_audio_markers[MAX_DEBUG_AUDIO_MARKERS] = {0};
int g_debug_marker_index = 0;
#endif

// #if DE100_INTERNAL
// // ═══════════════════════════════════════════════════════════════
// 📊 DEBUG AUDIO MARKER TRACKING
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

ALSA_SND_PCM_GET_PARAMS(AlsaSndPcmGetParamsStub) {
  (void)pcm;
  (void)buffer_size;
  (void)period_size;
  return -1; // Error: not available
}

ALSA_SND_PCM_START(AlsaSndPcmStartStub) {
  (void)pcm;
  return -1;
}

ALSA_SND_PCM_DROP(AlsaSndPcmDropStub) {
  (void)pcm;
  return -1;
}

// Global function pointers (start as stubs, replaced if ALSA loads
// successfully)

alsa_snd_pcm_open *SndPcmOpen_ = AlsaSndPcmOpenStub;
alsa_snd_pcm_set_params *SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
alsa_snd_pcm_writei *SndPcmWritei_ = AlsaSndPcmWriteiStub;
alsa_snd_pcm_prepare *SndPcmPrepare_ = AlsaSndPcmPrepareStub;
alsa_snd_pcm_close *SndPcmClose_ = AlsaSndPcmCloseStub;
alsa_snd_strerror *SndStrerror_ = AlsaSndStrerrorStub;
alsa_snd_pcm_avail *SndPcmAvail_ = AlsaSndPcmAvailStub;
alsa_snd_pcm_recover *SndPcmRecover_ = AlsaSndPcmRecoverStub;
alsa_snd_pcm_delay *SndPcmDelay_ = AlsaSndPcmDelayStub;
alsa_snd_pcm_get_params *SndPcmGetParams_ = AlsaSndPcmGetParamsStub;
alsa_snd_pcm_start *SndPcmStart_ = AlsaSndPcmStartStub;
alsa_snd_pcm_drop *SndPcmDrop_ = AlsaSndPcmDropStub;

// Global audio state (ALSA handle, buffers, latency params)
LinuxSoundOutput g_linux_audio_output = {0};

// ═══════════════════════════════════════════════════════════════
// Load ALSA Library (Casey's Win32LoadXInput pattern)
// ═══════════════════════════════════════════════════════════════
// WHY: Graceful degradation if ALSA missing
// HOW: Try loading libasound.so.2, fallback to libasound.so
// WHEN: Called once at startup
// ═══════════════════════════════════════════════════════════════

void linux_load_alsa(void) {
  printf("Loading ALSA library...\n");

  // Try versioned library first (more specific), then fallback
  // RTLD_LAZY = resolve symbols when first called (faster startup)

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
  g_linux_audio_output.alsa_library = alsa_lib;

  // Get function pointers (like Casey's GetProcAddress)
  // Macro reduces repetition - each function loaded the same way
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
  LOAD_ALSA_FN(SndPcmGetParams_, "snd_pcm_get_params", alsa_snd_pcm_get_params);
  LOAD_ALSA_FN(SndPcmStart_, "snd_pcm_start", alsa_snd_pcm_start);
  LOAD_ALSA_FN(SndPcmDrop_, "snd_pcm_drop", alsa_snd_pcm_drop);
#undef LOAD_ALSA_FN

  // Sanity check: did we get the core functions?
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
    g_linux_audio_output.alsa_library = NULL;
  }

  // Day 10: Optional latency measurement (not all ALSA versions have it)
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
// Initialize Sound System - FULLY FIXED VERSION
// ═══════════════════════════════════════════════════════════════
// WHAT: Opens ALSA device, sets format, calculates latency
// WHY: Game needs audio output synchronized to game logic
// WHEN: Called once at startup after loading ALSA library
//
// KEY FIXES:
// 1. Use 2-second buffer like Casey's DirectSound
// 2. Remove SND_PCM_NONBLOCK (causes random -EAGAIN errors)
// 3. Enable soft_resample (more forgiving with hardware)
// 4. Use ALSA's actual buffer size (not our requested size)
// 5. Simplified prefill with silence
// ═══════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════
// Initialize Sound System - FULLY FIXED VERSION
// ═══════════════════════════════════════════════════════════════
// WHAT: Opens ALSA device, sets format, calculates latency
// WHY: Game needs audio output synchronized to game logic
// WHEN: Called once at startup after loading ALSA library
//
// KEY FIXES:
// 1. Use 2-second buffer like Casey's DirectSound
// 2. Remove SND_PCM_NONBLOCK (causes random -EAGAIN errors)
// 3. Enable soft_resample (more forgiving with hardware)
// 4. Use ALSA's actual buffer size (not our requested size)
// 5. Simplified prefill with silence
// ═══════════════════════════════════════════════════════════════

bool linux_init_audio(GameAudioOutputBuffer *audio_output,
                      PlatformAudioConfig *audio_config,
                      int32 samples_per_second, int32 buffer_size_bytes,
                      int32 game_update_hz) {
  if (game_update_hz <= 0) {
    fprintf(stderr, "❌ AUDIO INIT: Invalid game_update_hz=%d\n",
            game_update_hz);
    return false;
  }

  audio_config->game_update_hz = game_update_hz;
  int32 samples_per_frame = samples_per_second / audio_config->game_update_hz;

  // ═══════════════════════════════════════════════════════════════
  // 🛡️ DAY 20: SAFETY MARGIN CALCULATION
  // ═══════════════════════════════════════════════════════════════
  // Calculate safety margin (1/3 of a frame worth of samples)
  // This accounts for frame timing variance
  //
  // Example at 30 FPS, 48000 Hz:
  // - samples_per_frame = 48000 / 30 = 1600 samples
  // - safety_sample_count = 1600 / 3 = 533 samples
  // - This gives us ~11ms of safety margin
  //
  // Why 1/3 frame?
  // - Frame timing can vary by ±10% (30ms → 27-33ms)
  // - 1/3 frame = 33% extra buffer
  // - Covers variance with plenty of headroom
  // ═══════════════════════════════════════════════════════════════

  g_linux_audio_output.safety_sample_count = samples_per_frame / 3;
  audio_config->safety_sample_count = g_linux_audio_output.safety_sample_count;

  printf("[AUDIO INIT] Safety margin: %d samples (%.1f ms)\n",
         g_linux_audio_output.safety_sample_count,
         (float)g_linux_audio_output.safety_sample_count / samples_per_second *
             1000.0f);

  // ═════════════════════════════════════════════════════════════════
  // 🎵 BUFFER SIZE: Use 2 seconds like Casey's DirectSound
  // ═══════════════════════════════════════════════════════════════
  // DirectSound uses 2 seconds = 192KB at 48kHz stereo
  // This gives us plenty of room for Day 20's write-ahead algorithm
  int32 latency_sample_count = samples_per_second * 2; // 96000 samples at 48kHz
  int64 latency_us =
      (((int64)latency_sample_count * 1000000) / samples_per_second);

  printf("[AUDIO INIT] Requesting ALSA buffer: %d samples (%.1f ms)\n",
         latency_sample_count, latency_us / 1000.0f);

  // Store requested latency (will be updated with actual value later)
  g_linux_audio_output.latency_sample_count = latency_sample_count;
  g_linux_audio_output.latency_microseconds = latency_us;

  // ═══════════════════════════════════════════════════════════════
  // 🔌 OPEN AUDIO DEVICE
  // ═══════════════════════════════════════════════════════════════
  // Try devices in order of preference:
  // 1. "default" - PulseAudio (best compatibility, auto-start)
  // 2. "plughw:0,0" - Direct hardware with ALSA conversions
  // 3. "hw:0,0" - Raw hardware (lowest latency, may be locked)
  //
  // CRITICAL: Don't use SND_PCM_NONBLOCK! It causes random -EAGAIN errors.
  const char *devices[] = {"default", "plughw:0,0", "hw:0,0"};
  int err = -1;

  for (int i = 0; i < 3 && err < 0; i++) {
    err = SndPcmOpen(&g_linux_audio_output.pcm_handle, devices[i],
                     LINUX_SND_PCM_STREAM_PLAYBACK,
                     0); // 0 = blocking mode (not SND_PCM_NONBLOCK!)
    if (err >= 0) {
      printf("✅ Sound: Opened device '%s'\n", devices[i]);
      break;
    }
    printf("   Device '%s' failed: %s\n", devices[i], SndStrerror(err));
  }

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot open any audio device\n");
    audio_config->is_initialized = false;
    return false;
  }

  // ═══════════════════════════════════════════════════════════════
  // 🎛️ SET AUDIO FORMAT
  // ═══════════════════════════════════════════════════════════════
  // Format: 48kHz, 16-bit signed little-endian, stereo, interleaved
  // soft_resample = 1 allows ALSA to handle rate conversion if needed
  // latency_us = requested buffer size in microseconds
  err = SndPcmSetParams(
      g_linux_audio_output.pcm_handle,
      LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed little-endian
      LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // Interleaved (L-R-L-R)
      2,                                   // Stereo (2 channels)
      samples_per_second,                  // 48000 Hz
      1,         // CRITICAL: 1 = allow soft resample (more forgiving!)
      latency_us // 2000000 us = 2 seconds
  );

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot set parameters: %s\n", SndStrerror(err));
    SndPcmClose(g_linux_audio_output.pcm_handle);
    audio_config->is_initialized = false;
    return false;
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔍 VERIFY: What buffer size did ALSA actually give us?
  // ═══════════════════════════════════════════════════════════════
  // ALSA might not give us exactly what we asked for!
  // We MUST use the actual buffer size for cursor math to work.
  snd_pcm_uframes_t actual_buffer_size = 0;
  snd_pcm_uframes_t actual_period_size = 0;

  err = SndPcmGetParams(g_linux_audio_output.pcm_handle, &actual_buffer_size,
                        &actual_period_size);

  if (err >= 0) {
    printf("✅ ALSA actual buffer: %lu frames (requested: %d)\n",
           (unsigned long)actual_buffer_size, latency_sample_count);
    printf("   ALSA period size: %lu frames\n",
           (unsigned long)actual_period_size);

    // CRITICAL: Use ALSA's actual buffer size!
    // If we use our requested size but ALSA gave us something different,
    // the cursor math will be wrong and audio will click.
    g_linux_audio_output.latency_sample_count = (int32)actual_buffer_size;

    float actual_latency_ms =
        (float)actual_buffer_size / samples_per_second * 1000.0f;
    printf("   Actual latency: %.1f ms\n", actual_latency_ms);

    if (actual_buffer_size < (snd_pcm_uframes_t)latency_sample_count) {
      printf("⚠️  ALSA gave smaller buffer than requested\n");
      printf("    This is OK, we'll use the actual size\n");
    }
  } else {
    fprintf(stderr, "⚠️  Could not query buffer params: %s\n", SndStrerror(err));
    fprintf(stderr, "    Using requested size (may cause timing issues)\n");
    // Keep the requested size we stored earlier
  }

  printf("✅ Sound: Format set to %d Hz, 16-bit stereo\n", samples_per_second);

  // ═══════════════════════════════════════════════════════════════
  // 💾 STORE AUDIO PARAMETERS
  // ═══════════════════════════════════════════════════════════════
  audio_output->samples_per_second = samples_per_second;
  audio_config->bytes_per_sample = sizeof(int16) * 2; // L+R channels
  g_linux_audio_output.buffer_size = buffer_size_bytes;

  // ═══════════════════════════════════════════════════════════════
  // 📦 ALLOCATE SAMPLE BUFFER
  // ═══════════════════════════════════════════════════════════════
  g_linux_audio_output.sample_buffer_size =
      g_linux_audio_output.latency_sample_count;
  int sample_buffer_bytes =
      g_linux_audio_output.sample_buffer_size * audio_config->bytes_per_sample;

  // LATFORM_MEMORY_ZEROED here if you want it pre-zeroed
  g_linux_audio_output.sample_buffer = platform_allocate_memory(
      NULL, sample_buffer_bytes,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(g_linux_audio_output.sample_buffer)) {
    fprintf(stderr, "❌ Sound: Cannot allocate sample buffer\n");
    fprintf(stderr, "   Error: %s\n",
            g_linux_audio_output.sample_buffer.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(
                g_linux_audio_output.sample_buffer.error_code));
    SndPcmClose(g_linux_audio_output.pcm_handle);
    audio_config->is_initialized = false;
    return false;
  }

  printf("✅ Sound: Allocated sample buffer (%d frames, %.1f KB)\n",
         g_linux_audio_output.sample_buffer_size,
         (float)sample_buffer_bytes / 1024.0f);

  // ═══════════════════════════════════════════════════════════════
  // 🎵 INITIALIZE TEST TONE PARAMETERS
  // ═══════════════════════════════════════════════════════════════
  audio_config->running_sample_index = 0;
  // Can't init `game_audio_state` here, it should be on the game loop
  // game_audio_state->tone.frequency = 256; // 256 Hz test tone
  // game_audio_state->tone.volume = 6000;   // Volume (max ~32767)
  // audio_config->wave_period =
  //     samples_per_second / game_audio_state->tone.frequency;
  // game_audio_state->tone.phase = 0.0f;     // Sine wave phase
  // game_audio_state->tone.pan_position = 0; // Center pan
  audio_config->is_initialized = true;

  // printf("✅ Sound: Test tone initialized\n");
  // printf("   Frequency:  %0.2f Hz\n", game_audio_state->tone.frequency);
  // printf("   Volume:     %0.2f\n", game_audio_state->tone.volume);
  // printf("   Pan:        center (0)\n");

  // 🚀 PRE-FILL BUFFER TO START PLAYBACK
  {
    printf("🔊 Sound: Pre-filling buffer...\n");

    int prep_err = SndPcmPrepare(g_linux_audio_output.pcm_handle);
    if (prep_err < 0) {
      printf("⚠️  snd_pcm_prepare failed: %s (continuing anyway)\n",
             SndStrerror(prep_err));
    }

    // Use 2 frames worth of samples, not 12.5% of buffer!
    // At 60 FPS, 48kHz: 2 * 800 = 1600 samples
    int32 samples_per_frame_init = samples_per_second / game_update_hz;
    int prefill_frames = samples_per_frame_init * 2;

    if (prefill_frames > (int)g_linux_audio_output.sample_buffer_size) {
      prefill_frames = (int)g_linux_audio_output.sample_buffer_size;
    }

    long written =
        SndPcmWritei(g_linux_audio_output.pcm_handle,
                     g_linux_audio_output.sample_buffer.base, prefill_frames);

    if (written > 0) {
      audio_config->running_sample_index = written;
      printf("✅ Sound: Pre-filled %ld frames of silence\n", written);

      // ✅ CRITICAL: Explicitly start playback
      int start_err = SndPcmStart(g_linux_audio_output.pcm_handle);
      if (start_err < 0 && start_err != -EBADFD) {
        printf("⚠️  snd_pcm_start: %s (continuing anyway)\n",
               SndStrerror(start_err));
      } else {
        printf("✅ Sound: Playback started\n");
      }
    } else if (written < 0) {
      printf("⚠️  Pre-fill write failed: %s\n", SndStrerror((int)written));
      SndPcmRecover(g_linux_audio_output.pcm_handle, (int)written, 1);
    }
  }

  // ═══════════════════════════════════════════════════════════════
  // 📊 FINAL STATUS REPORT
  // ═══════════════════════════════════════════════════════════════
  printf("\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 AUDIO SYSTEM INITIALIZED\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Sample rate:      %d Hz\n", samples_per_second);
  printf("Buffer size:      %d frames (%.1f ms)\n",
         g_linux_audio_output.latency_sample_count,
         (float)g_linux_audio_output.latency_sample_count / samples_per_second *
             1000.0f);
  printf("Samples/frame:    %d (at %d Hz game logic)\n", samples_per_frame,
         game_update_hz);
  printf("Safety margin:    %d samples\n", audio_config->safety_sample_count);
  // printf("Test tone:        %0.2f Hz at volume %0.2f\n",
  //        game_audio_state->tone.frequency, game_audio_state->tone.volume);
  printf("═══════════════════════════════════════════════════════════\n");
  printf("\n");

  return true;
}

void linux_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                     PlatformAudioConfig *audio_config) {
  // ═══════════════════════════════════════════════════════════════
  // IMPORTANT: Audio rate should NOT change with rendering FPS!
  // ═══════════════════════════════════════════════════════════════
  //
  // Casey's architecture:
  // - game_update_hz = GAME LOGIC rate (fixed, e.g., 30 Hz)
  // - Rendering FPS = adaptive (30-120 Hz)
  // - Audio syncs to game logic, NOT rendering
  //
  // So when rendering FPS changes, we do NOTHING to audio!
  // The audio system continues at its fixed rate.
  //
  // If you DO want to change audio rate (rare), just update the
  // parameters without resetting the buffer:
  // ═══════════════════════════════════════════════════════════════

  if (!audio_config->is_initialized) {
    return;
  }

  // Recalculate derived values based on new game_update_hz
  int32 samples_per_frame =
      audio_output->samples_per_second / audio_config->game_update_hz;

  g_linux_audio_output.safety_sample_count = samples_per_frame / 3;
  audio_config->safety_sample_count = g_linux_audio_output.safety_sample_count;

#if DE100_INTERNAL
  printf("[AUDIO] Rate updated: %d Hz, samples/frame=%d, safety=%d\n",
         audio_config->game_update_hz, samples_per_frame,
         audio_config->safety_sample_count);
#endif

  // NOTE: We do NOT call SndPcmDrop/SndPcmPrepare!
  // That would cause audio clicks. The buffer continues playing
  // and we just adjust how much we write each frame.
}

// ═══════════════════════════════════════════════════════════════════════════
// QUERY SAMPLES TO WRITE
// ═══════════════════════════════════════════════════════════════════════════
// Tells the game how many samples to generate this frame.
//
// Casey's approach (Day 20):
// - Query ALSA delay (how much is buffered)
// - Calculate target buffer level
// - Return difference
// ═══════════════════════════════════════════════════════════════════════════
int32 linux_get_samples_to_write(PlatformAudioConfig *audio_config,
                                 GameAudioOutputBuffer *audio_output) {
  if (!audio_config->is_initialized || !g_linux_audio_output.pcm_handle) {
    return 0;
  }

  snd_pcm_sframes_t delay = 0;
  int err = SndPcmDelay(g_linux_audio_output.pcm_handle, &delay);
  if (err < 0) {
    SndPcmRecover(g_linux_audio_output.pcm_handle, err, 1);
    delay = 0;
  }

  snd_pcm_sframes_t avail = SndPcmAvail(g_linux_audio_output.pcm_handle);
  if (avail < 0) {
    SndPcmRecover(g_linux_audio_output.pcm_handle, (int)avail, 1);
    avail = SndPcmAvail(g_linux_audio_output.pcm_handle);
    if (avail < 0)
      avail = 0;
  }

  int32 samples_per_frame =
      audio_output->samples_per_second / audio_config->game_update_hz;

  // Target: 150ms of audio buffered
  int32 target_buffered = (audio_output->samples_per_second * 150) / 1000;

  // ✅ FIX: Define a "comfortable" range around target
  // Low water mark: 100ms - below this, we need to write more
  // High water mark: 200ms - above this, let it drain
  int32 low_water = (audio_output->samples_per_second * 100) / 1000;
  int32 high_water = (audio_output->samples_per_second * 200) / 1000;

  int32 samples_needed = 0;

  if (delay < low_water) {
    // Buffer is getting low - write up to target
    samples_needed = target_buffered - (int32)delay;
  } else if (delay < high_water) {
    // Buffer is in comfortable range - write 1 frame to maintain
    samples_needed = samples_per_frame;
  } else {
    // Buffer is overfull - let it drain
    samples_needed = 0;
  }

  // Clamp to available space
  if (samples_needed > (int32)avail) {
    samples_needed = (int32)avail;
  }

  // Don't generate more than 3 frames at once
  int32 max_per_call = samples_per_frame * 3;
  if (samples_needed > max_per_call) {
    samples_needed = max_per_call;
  }

#if DE100_INTERNAL
  local_persist_var int log_count = 0;
  if (++log_count % 300 == 1) {
    printf("[AUDIO] delay=%ld, low=%d, high=%d, needed=%d, avail=%ld\n",
           (long)delay, low_water, high_water, samples_needed, (long)avail);
  }
#endif

  return samples_needed;
}

// Debug helper: Print current audio latency (Day 10)
void linux_debug_audio_latency(
    // GameAudioOutputBuffer *audio_output,
    PlatformAudioConfig *audio_config
    //  GameAudioState *game_audio_state
) {
  if (!audio_config->is_initialized) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  printf("┌─────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 Audio Debug Info (Platform Layer)                   │\n");
  printf("├─────────────────────────────────────────────────────────┤\n");

  snd_pcm_sframes_t delay_frames = 0;
  int err = SndPcmDelay(g_linux_audio_output.pcm_handle, &delay_frames);

  if (err < 0) {
    printf("│ ❌ Can't measure delay: %s\n", SndStrerror(err));
  } else {
    float latency_ms =
        (float)delay_frames / audio_config->samples_per_second * 1000.0f;
    printf("│ Latency:         %.1f ms (%ld frames)\n", latency_ms,
           (long)delay_frames);
  }

  snd_pcm_sframes_t avail = SndPcmAvail(g_linux_audio_output.pcm_handle);
  printf("│ Available:       %ld frames\n", (long)avail);
  printf("│ Sample rate:     %d Hz\n", audio_config->samples_per_second);
  printf("│ Running index:   %ld\n", (long)audio_config->running_sample_index);
  printf("│ Game update Hz:  %d\n", audio_config->game_update_hz);
  printf("└─────────────────────────────────────────────────────────┘\n");
}

void linux_unload_alsa(GameAudioOutputBuffer *audio_output,
                       PlatformAudioConfig *audio_config) {
  printf("Unloading ALSA audio...\n");

  // Free sample buffer
  platform_free_memory(&g_linux_audio_output.sample_buffer);
  platform_free_memory(&audio_output->samples_block);

  // Close PCM device
  if (g_linux_audio_output.pcm_handle) {
    SndPcmClose(g_linux_audio_output.pcm_handle);
    g_linux_audio_output.pcm_handle = NULL;
  }

  // Unload ALSA library
  if (g_linux_audio_output.alsa_library) {
    dlclose(g_linux_audio_output.alsa_library);
    g_linux_audio_output.alsa_library = NULL;
  }

  audio_config->is_initialized = false;

  printf("✅ ALSA audio unloaded.\n");
}

void linux_send_samples_to_alsa(PlatformAudioConfig *audio_config,
                                GameAudioOutputBuffer *source) {
  if (!audio_config->is_initialized || !source->samples_block.is_valid ||
      source->sample_count <= 0) {
    return;
  }

#if DE100_INTERNAL
  snd_pcm_sframes_t delay_frames = 0;
  snd_pcm_sframes_t avail_frames = 0;

  int delay_err = SndPcmDelay(g_linux_audio_output.pcm_handle, &delay_frames);
  if (delay_err < 0) {
    delay_frames = 0;
  }

  avail_frames = SndPcmAvail(g_linux_audio_output.pcm_handle);
  if (avail_frames < 0) {
    avail_frames = 0;
  }

  int32 buffer_size = g_linux_audio_output.latency_sample_count;
  int64 running_sample_index = audio_config->running_sample_index;

  // Calculate virtual cursors BEFORE the write
  int64 play_cursor = running_sample_index - delay_frames;
  if (play_cursor < 0)
    play_cursor = 0;
  int64 write_cursor = running_sample_index;

  // Calculate expected flip position
  int32 samples_per_frame =
      audio_config->samples_per_second / audio_config->game_update_hz;
  int64 expected_flip_play_cursor = play_cursor + samples_per_frame;
  int64 safe_write_cursor = write_cursor + source->sample_count;
#endif

  if (source->sample_count > (int32)g_linux_audio_output.sample_buffer_size) {
    source->sample_count = (int32)g_linux_audio_output.sample_buffer_size;
  }

  snd_pcm_sframes_t frames_written =
      SndPcmWritei(g_linux_audio_output.pcm_handle,
                   (int16 *)(source->samples_block.base), source->sample_count);

  if (frames_written < 0) {
    if (frames_written == -EPIPE) {
      int recover_err = SndPcmPrepare(g_linux_audio_output.pcm_handle);
      if (recover_err < 0) {
        return;
      }
      frames_written = SndPcmWritei(g_linux_audio_output.pcm_handle,
                                    (int16 *)(source->samples_block.base),
                                    source->sample_count);
      if (frames_written < 0) {
        frames_written = 0;
      } else {
        SndPcmStart(g_linux_audio_output.pcm_handle);
      }
    } else {
      SndPcmRecover(g_linux_audio_output.pcm_handle, (int)frames_written, 1);
      frames_written = 0;
    }
  }

  if (frames_written > 0) {
    audio_config->running_sample_index += frames_written;
  }

#if DE100_INTERNAL
  // ═══════════════════════════════════════════════════════════════
  // RECORD DEBUG MARKER
  // ═══════════════════════════════════════════════════════════════
  if (buffer_size > 0) {
    // ✅ FIX: Bounds check BEFORE accessing array
    int marker_index = g_debug_marker_index;
    if (marker_index < 0 || marker_index >= MAX_DEBUG_AUDIO_MARKERS) {
      marker_index = 0;
      g_debug_marker_index = 0;
    }

    LinuxDebugAudioMarker *marker = &g_debug_audio_markers[marker_index];

    marker->output_play_cursor = play_cursor % buffer_size;
    marker->output_write_cursor = write_cursor % buffer_size;
    marker->output_safe_write_cursor = safe_write_cursor % buffer_size;
    marker->output_location = write_cursor % buffer_size;
    marker->output_sample_count = frames_written > 0 ? frames_written : 0;
    marker->expected_flip_play_cursor = expected_flip_play_cursor % buffer_size;
    marker->output_delay_frames = delay_frames;
    marker->output_avail_frames = avail_frames;

    // ✅ FIX: Advance index AFTER recording
    g_debug_marker_index = (g_debug_marker_index + 1) % MAX_DEBUG_AUDIO_MARKERS;
  }
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// CLEAR SOUND BUFFER (Fill with silence)
// ═══════════════════════════════════════════════════════════════════════════

void linux_clear_audio_buffer(PlatformAudioConfig *audio_config) {
  if (!g_linux_audio_output.pcm_handle || !audio_config->is_initialized) {
    return;
  }

  // Calculate buffer size in frames
  int frames =
      audio_config->secondary_buffer_size / audio_config->bytes_per_sample;

  // Allocate temporary silence buffer
  PlatformMemoryBlock silence_block = platform_allocate_memory(
      NULL, audio_config->secondary_buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(silence_block)) {
    fprintf(stderr, "❌ Cannot allocate silence buffer: %s\n",
            silence_block.error_message);
    return;
  }

  // Write silence
  SndPcmWritei(g_linux_audio_output.pcm_handle, silence_block.base, frames);
  platform_free_memory(&silence_block);
}

#if DE100_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 📊 CAPTURE FLIP STATE (Day 20)
// ═══════════════════════════════════════════════════════════════
//
// Called AFTER frame display to record actual cursor positions.
// This allows us to compare prediction vs reality.
//
// DirectSound equivalent: GetCurrentPosition() after flip
// ALSA equivalent: Calculate virtual cursors from delay/avail
//
// ═══════════════════════════════════════════════════════════════

void linux_debug_capture_flip_state(PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized || !g_linux_audio_output.pcm_handle) {
    return;
  }

  int32 buffer_size = g_linux_audio_output.latency_sample_count;
  if (buffer_size <= 0) {
    return;
  }

  // ✅ FIX: Get the PREVIOUS marker (the one we just filled in send_samples)
  int marker_index = (g_debug_marker_index - 1 + MAX_DEBUG_AUDIO_MARKERS) %
                     MAX_DEBUG_AUDIO_MARKERS;

  // ✅ FIX: Bounds check
  if (marker_index < 0 || marker_index >= MAX_DEBUG_AUDIO_MARKERS) {
    return;
  }

  snd_pcm_sframes_t delay_frames = 0;
  snd_pcm_sframes_t avail_frames = 0;

  int delay_err = SndPcmDelay(g_linux_audio_output.pcm_handle, &delay_frames);
  if (delay_err < 0) {
    delay_frames = 0;
  }

  avail_frames = SndPcmAvail(g_linux_audio_output.pcm_handle);
  if (avail_frames < 0) {
    avail_frames = 0;
  }

  int64 total_written = audio_config->running_sample_index;
  int64 flip_write_cursor = total_written % buffer_size;
  int64 play_absolute = total_written - delay_frames;
  if (play_absolute < 0)
    play_absolute = 0;
  int64 flip_play_cursor = play_absolute % buffer_size;

  LinuxDebugAudioMarker *marker = &g_debug_audio_markers[marker_index];
  marker->flip_play_cursor = flip_play_cursor;
  marker->flip_write_cursor = flip_write_cursor;
  marker->flip_delay_frames = delay_frames;
  marker->flip_avail_frames = avail_frames;
}

#endif // DE100_INTERNAL

#if DE100_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 🎨 DRAW VERTICAL LINE (Helper for Visualization)
// ═══════════════════════════════════════════════════════════════
//
// Draws a vertical line in the pixel buffer.
// Used to mark cursor positions in the audio visualization.
//
// DirectSound equivalent: Win32DebugDrawVertical()
// X11 equivalent: Direct pixel manipulation in RGBA buffer
//
// ═══════════════════════════════════════════════════════════════

file_scoped_fn void linux_debug_draw_vertical(GameBackBuffer *buffer, int x,
                                              int top, int bottom,
                                              uint32 color) {
  // Bounds checking
  if (x < 0 || x >= buffer->width) {
    return;
  }

  if (top < 0) {
    top = 0;
  }

  if (bottom > buffer->height) {
    bottom = buffer->height;
  }

  // Draw vertical line
  uint8 *pixel = (uint8 *)buffer->memory.base +
                 x * 4 + // 4 bytes per pixel (RGBA)
                 top * buffer->pitch;

  for (int y = top; y < bottom; ++y) {
    *(uint32 *)pixel = color;
    pixel += buffer->pitch;
  }
}

// ═══════════════════════════════════════════════════════════════
// 🎨 DRAW SOUND BUFFER MARKER (Helper for Visualization)
// ═══════════════════════════════════════════════════════════════
//
// Converts a sample position to screen X coordinate and draws a line.
//
// DirectSound equivalent: Win32DrawSoundBufferMarker()
// Key difference: We work with absolute sample positions, not buffer offsets
//
// ═══════════════════════════════════════════════════════════════

file_scoped_fn void
linux_draw_audio_buffer_marker(GameBackBuffer *buffer,
                               GameAudioOutputBuffer *audio_output,
                               real32 coefficient, int pad_x, int top,
                               int bottom, int64 value, uint32 color) {
  (void)(audio_output);

  int32 buffer_size = g_linux_audio_output.latency_sample_count;

  // ✅ SAFETY CHECK
  if (buffer_size <= 0) {
    return;
  }

  // Handle negative values
  int64 buffer_offset = value;
  if (buffer_offset < 0) {
    buffer_offset = 0;
  }
  buffer_offset = buffer_offset % buffer_size;

  real32 x_real = coefficient * (real32)buffer_offset;
  int x = pad_x + (int)x_real;

  // ✅ STRICT BOUNDS CHECK before drawing
  if (x < 0 || x >= buffer->width) {
    return; // Silently skip out-of-bounds draws
  }

  if (top < 0)
    top = 0;
  if (bottom > buffer->height)
    bottom = buffer->height;
  if (top >= bottom)
    return;

  linux_debug_draw_vertical(buffer, x, top, bottom, color);
}

// ═══════════════════════════════════════════════════════════════
// 🎨 DEBUG SYNC DISPLAY (Day 20 Visualization)
// ═══════════════════════════════════════════════════════════════
//
// Draws the complete audio timing visualization.
// Shows last 15 frames compressed, current frame expanded.
//
// DirectSound equivalent: Win32DebugSyncDisplay()
// Key differences:
// 1. We work with absolute sample positions (modulo for display)
// 2. We show ALSA-specific data (delay, avail)
// 3. We handle RGBA pixel format (not BGRA)
//
// ═══════════════════════════════════════════════════════════════

void linux_debug_sync_display(GameBackBuffer *buffer,
                              GameAudioOutputBuffer *audio_output,
                              PlatformAudioConfig *audio_config,
                              LinuxDebugAudioMarker *markers, int marker_count,
                              int current_marker_index) {
  if (!audio_config->is_initialized || !buffer || !buffer->memory.base ||
      !markers || marker_count <= 0) {
    return;
  }

  if (current_marker_index < 0 || current_marker_index >= marker_count) {
    current_marker_index = 0;
  }

  int32 buffer_size = g_linux_audio_output.latency_sample_count;
  if (buffer_size <= 0) {
    return;
  }

#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
    LinuxDebugAudioMarker *m = &markers[current_marker_index];

    // ✅ FIX: Calculate actual difference handling wrap-around
    int64 write_ahead =
        (int64)m->output_write_cursor - (int64)m->output_play_cursor;
    if (write_ahead < 0)
      write_ahead += buffer_size;

    printf("[VIZ] play=%ld write=%ld ahead=%ld (%.1fms) delay=%ld\n",
           m->output_play_cursor, m->output_write_cursor, write_ahead,
           (float)write_ahead / audio_config->samples_per_second * 1000.0f,
           m->output_delay_frames);
  }
#endif

  // ═══════════════════════════════════════════════════════════════
  // LAYOUT CONSTANTS
  // ═══════════════════════════════════════════════════════════════

  int pad_x = 16;
  int pad_y = 16;
  int line_height = 64;

  real32 coefficient =
      (real32)(buffer->width - 2 * pad_x) / (real32)buffer_size;

  // ═══════════════════════════════════════════════════════════════
  // COLORS (RGBA format, not BGRA!)
  // ═══════════════════════════════════════════════════════════════

  uint32 play_color = 0xFFFFFFFF;          // White (play cursor)
  uint32 write_color = 0xFF0000FF;         // Red (write cursor)
  uint32 expected_flip_color = 0xFF00FFFF; // Yellow (prediction)
  uint32 play_window_color = 0xFFFF00FF;   // Magenta (uncertainty)

  // ═══════════════════════════════════════════════════════════════
  // DRAW ALL MARKERS
  // ═══════════════════════════════════════════════════════════════

  for (int marker_index = 0; marker_index < marker_count; ++marker_index) {
    // ✅ BOUNDS CHECK
    if (marker_index < 0 || marker_index >= MAX_DEBUG_AUDIO_MARKERS) {
      continue;
    }

    LinuxDebugAudioMarker *this_marker = &markers[marker_index];

    int top = pad_y;
    int bottom = pad_y + line_height;

    // ═══════════════════════════════════════════════════════════
    // CURRENT FRAME: Expanded view (4 lines)
    // ═══════════════════════════════════════════════════════════

    if (marker_index == current_marker_index) {
      top += line_height + pad_y;
      bottom += line_height + pad_y;

      int first_top = top;

      // LINE 1: Output Cursors (at frame start)
      linux_draw_audio_buffer_marker(
          buffer, audio_output, coefficient, pad_x, top, bottom,
          this_marker->output_play_cursor, play_color); // White

      linux_draw_audio_buffer_marker(buffer, audio_output, coefficient, pad_x,
                                     top, bottom,
                                     this_marker->output_write_cursor,
                                     write_color); // Red (actual write cursor)

      // ✅ NEW: Draw safe write cursor in a different color
      linux_draw_audio_buffer_marker(buffer, audio_output, coefficient, pad_x,
                                     top, bottom,
                                     this_marker->output_safe_write_cursor,
                                     0xFFFFFF00); // Yellow (safe cursor)

      top += line_height + pad_y;
      bottom += line_height + pad_y;

      // LINE 2: Write Region (what we wrote)
      linux_draw_audio_buffer_marker(buffer, audio_output, coefficient, pad_x,
                                     top, bottom, this_marker->output_location,
                                     play_color);
      linux_draw_audio_buffer_marker(
          buffer, audio_output, coefficient, pad_x, top, bottom,
          this_marker->output_location + this_marker->output_sample_count,
          write_color);

      top += line_height + pad_y;
      bottom += line_height + pad_y;

      // LINE 3: Expected Flip Cursor (prediction)
      // This is a TALL line spanning all 3 lines above
      linux_draw_audio_buffer_marker(
          buffer, audio_output, coefficient, pad_x, first_top, bottom,
          this_marker->expected_flip_play_cursor, expected_flip_color);
    }

    // ═══════════════════════════════════════════════════════════
    // ALL FRAMES: Flip Cursors (compressed view)
    // ═══════════════════════════════════════════════════════════

    // LINE 4: Actual cursors after flip
    linux_draw_audio_buffer_marker(buffer, audio_output, coefficient, pad_x,
                                   top, bottom, this_marker->flip_play_cursor,
                                   play_color);

    // Play window (480-sample uncertainty)
    // ALSA equivalent: We use a fixed window size for consistency
    int32 play_window_samples = 480; // Same as DirectSound
    linux_draw_audio_buffer_marker(
        buffer, audio_output, coefficient, pad_x, top, bottom,
        this_marker->flip_play_cursor + play_window_samples, play_window_color);

    linux_draw_audio_buffer_marker(buffer, audio_output, coefficient, pad_x,
                                   top, bottom, this_marker->flip_write_cursor,
                                   write_color);
  }
}
#endif // DE100_INTERNAL