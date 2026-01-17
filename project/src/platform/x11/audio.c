// ═══════════════════════════════════════════════════════════════
// 🔊 HANDMADE HERO - AUDIO SYSTEM (Days 7-19)
// ═══════════════════════════════════════════════════════════════
//
// WHAT THIS FILE DOES:
// - Dynamically loads ALSA (Linux audio library)
// - Initializes audio output at 48kHz, 16-bit stereo
// - Generates sine wave test audio
// - Handles audio buffer synchronization (Day 19)
// - Provides debug visualization of audio timing
//
// WHY WE USE ALSA:
// - Linux equivalent of DirectSound (Windows audio API)
// - We load it dynamically (dlopen) so the game doesn't crash if audio fails
// - ALSA manages the ring buffer for us (simpler than DirectSound)
//
// KEY CONCEPTS:
// - Sample Rate: 48000 samples/second (like FPS but for audio)
// - Ring Buffer: Circular buffer that wraps around when full
// - Latency: Delay between writing audio and hearing it (~66ms)
// - Frame-Aligned: Audio writes sync with game logic (30 FPS)
//
// CASEY'S PATTERN:
// - Audio runs at FIXED rate (30 FPS game logic)
// - Rendering can adapt (30-120 FPS)
// - Humans tolerate visual stutter, but hate audio clicks!
// ═══════════════════════════════════════════════════════════════
#include "audio.h"
#include "../../base.h"
#include "../../game.h"
#include <dlfcn.h> // For dlopen, dlsym, dlclose (Casey's LoadLibrary equivalent)
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #include <alsa/asoundlib.h>

#if HANDMADE_INTERNAL
#include "../_common/debug.h"
#endif

#if HANDMADE_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 📊 DEBUG AUDIO MARKER TRACKING
// ═══════════════════════════════════════════════════════════════
// Casey stores 15 markers (0.5 seconds at 30 FPS)
// We'll do the same
// ═══════════════════════════════════════════════════════════════

#define MAX_DEBUG_AUDIO_MARKERS_30                                             \
  30 // 1 second at 30 FPS (or 0.5 sec at 60 FPS)
#define MAX_DEBUG_AUDIO_MARKERS_60                                             \
  60 // 1 second at 60 FPS (or 0.5 sec at 120 FPS)
#define MAX_DEBUG_AUDIO_MARKERS_120                                            \
  120 // 1 second at 120 FPS (or 0.5 sec at 240 FPS)

LinuxDebugAudioMarker g_debug_audio_markers[MAX_DEBUG_AUDIO_MARKERS] = {0};
int g_debug_marker_index = 0;

#endif // HANDMADE_INTERNAL

// ═══════════════════════════════════════════════════════════════
// ALSA DYNAMIC LOADING (Casey's Pattern)
// ═══════════════════════════════════════════════════════════════
// WHY: Game runs even if audio fails (graceful degradation)
// HOW: dlopen() to load library, dlsym() to get function pointers
// WHEN: At startup, before trying to play audio
// ═══════════════════════════════════════════════════════════════

// Stub functions return errors when ALSA isn't loaded

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

// Global audio state (ALSA handle, buffers, latency params)
LinuxSoundOutput g_linux_sound_output = {0};

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
  g_linux_sound_output.alsa_library = alsa_lib;

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
    g_linux_sound_output.alsa_library = NULL;
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

bool linux_init_sound(GameSoundOutput *sound_output, int32_t samples_per_second,
                      int32_t buffer_size_bytes, int32_t game_update_hz) {
  if (game_update_hz <= 0) {
    fprintf(stderr, "❌ AUDIO INIT: Invalid game_update_hz=%d\n",
            game_update_hz);
    return false;
  }

#if HANDMADE_INTERNAL
  for (int i = 0; i < 5; i++) {
    g_debug_audio_markers[i].flip_play_cursor = -1;  // Invalid marker
    g_debug_audio_markers[i].flip_write_cursor = -1; // Invalid marker
  }
#endif

  sound_output->game_update_hz = game_update_hz;

  int32_t samples_per_frame = samples_per_second / sound_output->game_update_hz;

  // ═══════════════════════════════════════════════════════════════
  // 🛡️ DAY 20: SAFETY MARGIN
  // ═══════════════════════════════════════════════════════════════
  // This is the buffer we stay ahead of the play cursor by
  // Casey uses 1/3 of a frame as safety margin
  sound_output->safety_sample_count = samples_per_frame / 3;

  printf("[AUDIO INIT] Safety margin: %d samples (%.1f ms)\n",
         sound_output->safety_sample_count,
         (float)sound_output->safety_sample_count / samples_per_second *
             1000.0f);

  // ═══════════════════════════════════════════════════════════════
  // 🎵 BUFFER SIZE: Use 2 seconds like Casey's DirectSound
  // ═══════════════════════════════════════════════════════════════
  // DirectSound uses 2 seconds = 192KB at 48kHz stereo
  // This gives us plenty of room for Day 20's write-ahead algorithm
  int32_t latency_sample_count =
      samples_per_second * 2; // 96000 samples at 48kHz
  int64_t latency_us =
      (((int64_t)latency_sample_count * 1000000) / samples_per_second);

  printf("[AUDIO INIT] Requesting ALSA buffer: %d samples (%.1f ms)\n",
         latency_sample_count, latency_us / 1000.0f);

  // Store requested latency (will be updated with actual value later)
  g_linux_sound_output.latency_sample_count = latency_sample_count;
  g_linux_sound_output.latency_microseconds = latency_us;

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
    err = SndPcmOpen(&g_linux_sound_output.handle, devices[i],
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
    sound_output->is_initialized = false;
    return false;
  }

  // ═══════════════════════════════════════════════════════════════
  // 🎛️ SET AUDIO FORMAT
  // ═══════════════════════════════════════════════════════════════
  // Format: 48kHz, 16-bit signed little-endian, stereo, interleaved
  // soft_resample = 1 allows ALSA to handle rate conversion if needed
  // latency_us = requested buffer size in microseconds
  err = SndPcmSetParams(
      g_linux_sound_output.handle,
      LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed little-endian
      LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // Interleaved (L-R-L-R)
      2,                                   // Stereo (2 channels)
      samples_per_second,                  // 48000 Hz
      1,         // CRITICAL: 1 = allow soft resample (more forgiving!)
      latency_us // 2000000 us = 2 seconds
  );

  if (err < 0) {
    fprintf(stderr, "❌ Sound: Cannot set parameters: %s\n", SndStrerror(err));
    SndPcmClose(g_linux_sound_output.handle);
    sound_output->is_initialized = false;
    return false;
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔍 VERIFY: What buffer size did ALSA actually give us?
  // ═══════════════════════════════════════════════════════════════
  // ALSA might not give us exactly what we asked for!
  // We MUST use the actual buffer size for cursor math to work.
  snd_pcm_uframes_t actual_buffer_size = 0;
  snd_pcm_uframes_t actual_period_size = 0;

  err = SndPcmGetParams(g_linux_sound_output.handle, &actual_buffer_size,
                        &actual_period_size);

  if (err >= 0) {
    printf("✅ ALSA actual buffer: %lu frames (requested: %d)\n",
           (unsigned long)actual_buffer_size, latency_sample_count);
    printf("   ALSA period size: %lu frames\n",
           (unsigned long)actual_period_size);

    // CRITICAL: Use ALSA's actual buffer size!
    // If we use our requested size but ALSA gave us something different,
    // the cursor math will be wrong and audio will click.
    g_linux_sound_output.latency_sample_count = (int32_t)actual_buffer_size;

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
  sound_output->samples_per_second = samples_per_second;
  sound_output->bytes_per_sample = sizeof(int16_t) * 2; // L+R channels
  g_linux_sound_output.buffer_size = buffer_size_bytes;

  // ═══════════════════════════════════════════════════════════════
  // 📦 ALLOCATE SAMPLE BUFFER
  // ═══════════════════════════════════════════════════════════════
  // This is our CPU-side buffer for generating audio before sending to ALSA
  // Size matches ALSA's buffer size (not our original request!)
  g_linux_sound_output.sample_buffer_size =
      g_linux_sound_output.latency_sample_count;
  int sample_buffer_bytes =
      g_linux_sound_output.sample_buffer_size * sound_output->bytes_per_sample;

  g_linux_sound_output.sample_buffer = platform_allocate_memory(
      NULL, sample_buffer_bytes,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!g_linux_sound_output.sample_buffer.base) {
    fprintf(stderr, "❌ Sound: Cannot allocate sample buffer\n");
    SndPcmClose(g_linux_sound_output.handle);
    sound_output->is_initialized = false;
    return false;
  }

  printf("✅ Sound: Allocated sample buffer (%d frames, %.1f KB)\n",
         g_linux_sound_output.sample_buffer_size,
         (float)sample_buffer_bytes / 1024.0f);

  // ═══════════════════════════════════════════════════════════════
  // 🎵 INITIALIZE TEST TONE PARAMETERS
  // ═══════════════════════════════════════════════════════════════
  sound_output->running_sample_index = 0;
  sound_output->tone_hz = 256;      // 256 Hz test tone
  sound_output->tone_volume = 6000; // Volume (max ~32767)
  sound_output->wave_period = samples_per_second / sound_output->tone_hz;
  sound_output->t_sine = 0.0f;    // Sine wave phase
  sound_output->pan_position = 0; // Center pan
  sound_output->is_initialized = true;

  printf("✅ Sound: Test tone initialized\n");
  printf("   Frequency:  %d Hz\n", sound_output->tone_hz);
  printf("   Volume:     %d / 32767\n", sound_output->tone_volume);
  printf("   Pan:        center (0)\n");

  // 🚀 PRE-FILL BUFFER TO START PLAYBACK
  // Mirror Casey's DirectSound prefill: Fill 1/8 of actual buffer with silence
  {
    printf("🔊 Sound: Pre-filling buffer...\n");

    // Prepare the PCM device (like DirectSound buffer start)
    int prep_err = SndPcmPrepare(g_linux_sound_output.handle);
    if (prep_err < 0) {
      printf("⚠️  snd_pcm_prepare failed: %s (continuing anyway)\n",
             SndStrerror(prep_err));
    }

    // Use actual buffer size for prefill (ALSA-specific: don't assume requested
    // size)
    int prefill_frames = g_linux_sound_output.latency_sample_count * 0.125;
    if (prefill_frames > (int)g_linux_sound_output.sample_buffer_size) {
      prefill_frames = (int)g_linux_sound_output.sample_buffer_size;
    }

    // Generate silence (zeros) - Use allocated buffer
    PlatformMemoryBlock samples_block = g_linux_sound_output.sample_buffer;

    // Write silence to ALSA (blocking mode ensures it waits)
    long written = SndPcmWritei(g_linux_sound_output.handle, samples_block.base,
                                prefill_frames);

    if (written > 0) {
      sound_output->running_sample_index = written;
      printf("✅ Sound: Pre-filled %ld frames of silence\n", written);
      printf("   Buffer position: %ld / %d (%.1f%%)\n", written,
             g_linux_sound_output.latency_sample_count,
             (float)written / g_linux_sound_output.latency_sample_count *
                 100.0f);

      // Explicitly start playback (ALSA-specific: some devices need this)
      int start_err = SndPcmStart(g_linux_sound_output.handle);
      if (start_err < 0 && start_err != -EBADFD) {
        printf("⚠️  snd_pcm_start: %s (continuing anyway)\n",
               SndStrerror(start_err));
      } else {
        printf("✅ Sound: Playback started\n");
      }
    } else if (written < 0) {
      // ALSA-specific: Recover from errors during prefill
      printf("⚠️  Pre-fill write failed: %s\n", SndStrerror((int)written));
      SndPcmRecover(g_linux_sound_output.handle, (int)written, 1);
      printf("    Audio may not start immediately\n");
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
         g_linux_sound_output.latency_sample_count,
         (float)g_linux_sound_output.latency_sample_count / samples_per_second *
             1000.0f);
  printf("Samples/frame:    %d (at %d Hz game logic)\n", samples_per_frame,
         game_update_hz);
  printf("Safety margin:    %d samples\n", sound_output->safety_sample_count);
  printf("Test tone:        %d Hz at volume %d\n", sound_output->tone_hz,
         sound_output->tone_volume);
  printf("═══════════════════════════════════════════════════════════\n");
  printf("\n");

  return true;
}

// Check if ALSA latency measurement is available (not all versions have it)
file_scoped_fn inline bool linux_audio_has_latency_measurement(void) {
  return SndPcmDelay_ != AlsaSndPcmDelayStub;
}

#if HANDMADE_INTERNAL

// #define WRAP_SAMPLE(v, w) (((v) % (w) + (w)) % (w))
// static inline int64_t WrapSample(int64_t value, int64_t window) {
//   int64_t result = value % window;
//   if (result < 0)
//     result += window;
//   return result;
// }

// ═══════════════════════════════════════════════════════════════
// 📊 DAY 20: DEBUG SYNC DISPLAY (Complete Rewrite)
// ═══════════════════════════════════════════════════════════════
//
// This function draws vertical lines showing audio buffer state.
//
// COORDINATE SYSTEM:
// - X axis = position in audio buffer (0 to buffer_size)
// - Y axis = different "tiers" of information
//
// TIERS (from top to bottom):
// 1. All frames: Flip cursors (white = play, red = write)
// 2. Current frame: Output cursors (before writing)
// 3. Current frame: Write region (where we wrote)
// 4. Current frame: Expected flip position (yellow)
//
// COLOR LEGEND:
// - White (0xFFFFFFFF): Play cursor positions
// - Red (0xFFFF0000): Write cursor positions
// - Yellow (0xFFFFFF00): Expected/predicted positions
// - Magenta (0xFFFF00FF): Play window (safety margin visualization)
//
// ═══════════════════════════════════════════════════════════════

// Helper: Draw a vertical line with bounds checking
file_scoped_fn void debug_draw_vertical_line(GameOffscreenBuffer *buffer, int x,
                                             int top, int bottom,
                                             uint32_t color) {
  // Bounds checking (prevents crashes!)
  if (x < 0 || x >= buffer->width)
    return;
  if (top < 0)
    top = 0;
  if (bottom > buffer->height)
    bottom = buffer->height;
  if (top >= bottom)
    return;

  // Calculate starting pixel address
  uint8_t *pixel = (uint8_t *)buffer->memory.base + top * buffer->pitch +
                   x * buffer->bytes_per_pixel;

  // Draw vertical line
  for (int y = top; y < bottom; y++) {
    *(uint32_t *)pixel = color;
    pixel += buffer->pitch;
  }
}

// Helper: Convert sample position to screen X coordinate
file_scoped_fn int sample_to_screen_x(int64_t sample_position,
                                      int64_t buffer_size_samples,
                                      int screen_width, int pad_x) {
  // Wrap sample position to buffer size
  // This handles the ring buffer nature of audio
  int64_t wrapped = sample_position % buffer_size_samples;
  if (wrapped < 0)
    wrapped += buffer_size_samples;

  // Scale to screen coordinates
  int drawable_width = screen_width - 2 * pad_x;
  real32 scale = (real32)drawable_width / (real32)buffer_size_samples;
  int x = pad_x + (int)(scale * (real32)wrapped);

  return x;
}

void linux_debug_sync_display(GameOffscreenBuffer *buffer,
                              GameSoundOutput *sound_output,
                              LinuxDebugAudioMarker *markers, int marker_count,
                              int current_marker_index) {
  // ═══════════════════════════════════════════════════════════════
  // LAYOUT CONSTANTS
  // ═══════════════════════════════════════════════════════════════

  int pad_x = 16;
  int pad_y = 16;
  int line_height = 64;

  // We visualize the ALSA buffer, not 1 second of audio
  // This matches Casey's approach with DirectSound's SecondaryBufferSize
  int64_t buffer_size_samples = g_linux_sound_output.latency_sample_count;
  printf("[VIZ] buffer_size_samples=%ld, screen_width=%d\n",
         buffer_size_samples, buffer->width);

  // If buffer size is invalid, use 1 second as fallback
  if (buffer_size_samples <= 0) {
    buffer_size_samples = sound_output->samples_per_second;
  }

  // ═══════════════════════════════════════════════════════════════
  // COLOR DEFINITIONS
  // ═══════════════════════════════════════════════════════════════

  uint32_t play_color = 0xFFFFFFFF;          // White
  uint32_t write_color = 0xFFFF0000;         // Red
  uint32_t expected_flip_color = 0xFFFFFF00; // Yellow
  uint32_t play_window_color = 0xFFFF00FF;   // Magenta

  // ═══════════════════════════════════════════════════════════════
  // DRAW ALL MARKERS
  // ═══════════════════════════════════════════════════════════════

  for (int marker_idx = 0; marker_idx < marker_count; marker_idx++) {
    LinuxDebugAudioMarker *marker = &markers[marker_idx];

    // Skip uninitialized markers
    if (marker->flip_play_cursor < 0) {
      continue;
    }

    // ═══════════════════════════════════════════════════════════
    // TIER 1: FLIP CURSORS (All Frames)
    // ═══════════════════════════════════════════════════════════
    // This shows where the play/write cursors were at each frame flip.
    // You should see them marching steadily across the screen.

    int tier1_top = pad_y;
    int tier1_bottom = pad_y + line_height;

    int flip_play_x = sample_to_screen_x(
        marker->flip_play_cursor, buffer_size_samples, buffer->width, pad_x);
    int flip_write_x = sample_to_screen_x(
        marker->flip_write_cursor, buffer_size_samples, buffer->width, pad_x);

    debug_draw_vertical_line(buffer, flip_play_x, tier1_top, tier1_bottom,
                             play_color);
    debug_draw_vertical_line(buffer, flip_write_x, tier1_top, tier1_bottom,
                             write_color);

    // Draw play window (480 samples after play cursor, like Casey)
    int play_window_samples = 480;
    int play_window_x =
        sample_to_screen_x(marker->flip_play_cursor + play_window_samples,
                           buffer_size_samples, buffer->width, pad_x);
    debug_draw_vertical_line(buffer, play_window_x, tier1_top, tier1_bottom,
                             play_window_color);

    // ═══════════════════════════════════════════════════════════
    // TIERS 2-4: CURRENT FRAME ONLY (Detailed View)
    // ═══════════════════════════════════════════════════════════

    if (marker_idx == current_marker_index) {
      // ─────────────────────────────────────────────────────────
      // TIER 2: OUTPUT CURSORS (Before Writing)
      // ─────────────────────────────────────────────────────────

      int tier2_top = tier1_bottom + pad_y;
      int tier2_bottom = tier2_top + line_height;

      int output_play_x =
          sample_to_screen_x(marker->output_play_cursor, buffer_size_samples,
                             buffer->width, pad_x);
      int output_write_x =
          sample_to_screen_x(marker->output_write_cursor, buffer_size_samples,
                             buffer->width, pad_x);

      debug_draw_vertical_line(buffer, output_play_x, tier2_top, tier2_bottom,
                               play_color);
      debug_draw_vertical_line(buffer, output_write_x, tier2_top, tier2_bottom,
                               write_color);

      // ─────────────────────────────────────────────────────────
      // TIER 3: WRITE REGION (Where We Actually Wrote)
      // ─────────────────────────────────────────────────────────

      int tier3_top = tier2_bottom + pad_y;
      int tier3_bottom = tier3_top + line_height;

      int write_start_x = sample_to_screen_x(
          marker->output_location, buffer_size_samples, buffer->width, pad_x);
      int write_end_x = sample_to_screen_x(
          marker->output_location + marker->output_sample_count,
          buffer_size_samples, buffer->width, pad_x);

      debug_draw_vertical_line(buffer, write_start_x, tier3_top, tier3_bottom,
                               play_color);
      debug_draw_vertical_line(buffer, write_end_x, tier3_top, tier3_bottom,
                               write_color);

      // ─────────────────────────────────────────────────────────
      // TIER 4: EXPECTED FLIP POSITION (Prediction)
      // ─────────────────────────────────────────────────────────
      // This tall yellow line shows where we PREDICTED the play
      // cursor would be at frame flip. Compare to actual flip
      // cursor in Tier 1 to see prediction accuracy.

      int expected_x =
          sample_to_screen_x(marker->expected_flip_play_cursor,
                             buffer_size_samples, buffer->width, pad_x);

      // Draw from tier 2 top to tier 3 bottom (spans multiple tiers)
      debug_draw_vertical_line(buffer, expected_x, tier2_top, tier3_bottom,
                               expected_flip_color);
    }
  }

  // ═══════════════════════════════════════════════════════════════
  // DEBUG: Draw buffer boundary markers
  // ═══════════════════════════════════════════════════════════════
  // These help you see where the buffer wraps around

  // Left edge (buffer position 0)
  debug_draw_vertical_line(buffer, pad_x, pad_y, pad_y + 10,
                           0xFF00FF00); // Green

  // Right edge (buffer position = buffer_size)
  int right_edge = buffer->width - pad_x;
  debug_draw_vertical_line(buffer, right_edge, pad_y, pad_y + 10,
                           0xFF00FF00); // Green
}

// ═══════════════════════════════════════════════════════════════
// 📸 DAY 20: CAPTURE FLIP STATE
// ═══════════════════════════════════════════════════════════════
//
// This function is called AFTER glXSwapBuffers() (screen flip).
// It captures the ACTUAL audio cursor positions at flip time.
//
// By comparing these to our predictions (expected_flip_play_cursor),
// we can see if our audio timing is correct.
//
// IMPORTANT: This is where we advance the marker index!
// Each frame gets ONE marker, filled in two stages:
// 1. Output* fields (before audio write)
// 2. Flip* fields (after screen flip) ← We are here
//
// ═══════════════════════════════════════════════════════════════

void linux_debug_capture_flip_state(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized || !g_linux_sound_output.handle) {
    return;
  }

  // Query ALSA state at flip time
  snd_pcm_sframes_t delay_frames = 0;
  int err = SndPcmDelay(g_linux_sound_output.handle, &delay_frames);

  if (err >= 0) {
    // Calculate virtual cursors at flip time
    int64_t flip_play_cursor =
        sound_output->running_sample_index - delay_frames;
    int64_t flip_write_cursor =
        sound_output->running_sample_index + sound_output->safety_sample_count;

    // Store in the CURRENT marker (same one we filled Output* fields in)
    LinuxDebugAudioMarker *marker =
        &g_debug_audio_markers[g_debug_marker_index];
    marker->flip_play_cursor = flip_play_cursor;
    marker->flip_write_cursor = flip_write_cursor;

    // Debug: Compare prediction vs reality
    static int comparison_count = 0;
    if (++comparison_count <= 10 || comparison_count % 300 == 0) {
      int64_t prediction_error =
          flip_play_cursor - marker->expected_flip_play_cursor;
      printf("[FLIP #%d] Expected=%u, Actual=%ld, Error=%ld samples (%.2fms)\n",
             comparison_count, marker->expected_flip_play_cursor,
             flip_play_cursor, prediction_error,
             (float)prediction_error / sound_output->samples_per_second *
                 1000.0f);
    }
  } else {
    // Query failed, mark as invalid
    LinuxDebugAudioMarker *marker =
        &g_debug_audio_markers[g_debug_marker_index];
    marker->flip_play_cursor = -1;
    marker->flip_write_cursor = -1;
  }

  // ═══════════════════════════════════════════════════════════════
  // NOW we advance to the next marker!
  // This ensures Output* and Flip* are in the SAME marker.
  // ═══════════════════════════════════════════════════════════════

  g_debug_marker_index = (g_debug_marker_index + 1) % MAX_DEBUG_AUDIO_MARKERS;

  static int advance_count = 0;
  printf("[MARKER ADVANCE #%d] index now = %d\n", ++advance_count,
         g_debug_marker_index);
}

#endif // HANDMADE_INTERNAL

// ═══════════════════════════════════════════════════════════════
// 🔊 DAY 20: FILL SOUND BUFFER (Complete Rewrite)
// ═══════════════════════════════════════════════════════════════
//
// This function implements Casey's Day 20 audio timing algorithm.
//
// KEY CONCEPTS:
// 1. We calculate WHERE the play cursor will be at next frame flip
// 2. We determine if we're in "low latency" or "high latency" mode
// 3. We write audio to stay ahead of the play cursor
// 4. We capture debug markers for visualization
//
// ALSA TRANSLATION:
// - DirectSound PlayCursor → running_sample_index - delay
// - DirectSound WriteCursor → running_sample_index (our write position)
// - DirectSound Lock/Unlock → snd_pcm_writei (ALSA handles internally)
//
// ═══════════════════════════════════════════════════════════════

void linux_fill_sound_buffer(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized || !g_linux_sound_output.handle) {
    return;
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔍 STEP 1: QUERY ALSA STATE
  // ═══════════════════════════════════════════════════════════════
  //
  // ALSA gives us:
  // - delay: How many frames are queued for playback
  // - avail: How many frames we can write
  //
  // From these, we calculate "virtual cursors" like DirectSound:
  // - PlayCursor = running_sample_index - delay
  // - WriteCursor = running_sample_index (where we'll write next)
  //
  // ═══════════════════════════════════════════════════════════════

  snd_pcm_sframes_t delay_frames = 0;
  snd_pcm_sframes_t avail_frames = 0;

  int delay_err = SndPcmDelay(g_linux_sound_output.handle, &delay_frames);
  if (delay_err < 0) {
    // If delay query fails, assume no delay (conservative)
    delay_frames = 0;
  }

  avail_frames = SndPcmAvail(g_linux_sound_output.handle);
  if (avail_frames < 0) {
    // Handle ALSA errors (underrun, etc.)
    int recover_err =
        SndPcmRecover(g_linux_sound_output.handle, (int)avail_frames, 1);
    if (recover_err < 0) {
      // Recovery failed, try to continue anyway
      avail_frames = 0;
    } else {
      // Recovery succeeded, query again
      avail_frames = SndPcmAvail(g_linux_sound_output.handle);
      if (avail_frames < 0) {
        avail_frames = 0;
      }
    }
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔢 STEP 2: CALCULATE VIRTUAL CURSORS
  // ═══════════════════════════════════════════════════════════════
  //
  // These are the ALSA equivalents of DirectSound's cursors.
  //
  // IMPORTANT: These are absolute sample positions, not buffer offsets!
  // They grow forever (well, until int64 overflow in ~6 million years).
  //
  // ═══════════════════════════════════════════════════════════════

  int64_t running_sample_index = sound_output->running_sample_index;

  // PlayCursor: Where hardware is currently playing
  // If delay is 1000 and we've written 5000 samples, play cursor is at 4000
  int64_t play_cursor = running_sample_index - delay_frames;

  // WriteCursor: Where we'll write next (our running index)
  int64_t write_cursor = running_sample_index;

  // ═══════════════════════════════════════════════════════════════
  // 🎯 STEP 3: CALCULATE FRAME TIMING
  // ═══════════════════════════════════════════════════════════════
  //
  // Casey's algorithm needs to know:
  // - How many samples per game frame
  // - Where play cursor will be at next frame flip
  //
  // ═══════════════════════════════════════════════════════════════

  int32_t samples_per_frame =
      sound_output->samples_per_second / sound_output->game_update_hz;

  // Expected play cursor position at next frame flip
  // (current play position + one frame's worth of samples)
  int64_t expected_frame_boundary = play_cursor + samples_per_frame;

  // ═══════════════════════════════════════════════════════════════
  // 🛡️ STEP 4: CALCULATE SAFE WRITE CURSOR
  // ═══════════════════════════════════════════════════════════════
  //
  // Casey adds a "safety margin" to the write cursor to account for
  // timing variance in the game loop.
  //
  // SafeWriteCursor = WriteCursor + SafetyBytes
  //
  // If SafeWriteCursor < ExpectedFrameBoundary, we're in "low latency"
  // mode and can achieve perfect sync.
  //
  // ═══════════════════════════════════════════════════════════════

  int64_t safe_write_cursor = write_cursor + sound_output->safety_sample_count;

  // Determine if we're in low latency mode
  // Low latency = we have time to write audio that will play at frame flip
  bool audio_card_is_low_latency =
      (safe_write_cursor < expected_frame_boundary);

  // ═══════════════════════════════════════════════════════════════
  // 🎯 STEP 5: CALCULATE TARGET CURSOR
  // ═══════════════════════════════════════════════════════════════
  //
  // This is WHERE we want to write audio TO.
  //
  // LOW LATENCY MODE:
  //   Target = ExpectedFrameBoundary + one more frame
  //   (We write audio for the NEXT frame, achieving perfect sync)
  //
  // HIGH LATENCY MODE:
  //   Target = WriteCursor + one frame + safety margin
  //   (We can't achieve perfect sync, so just stay ahead)
  //
  // ═══════════════════════════════════════════════════════════════

  int64_t target_cursor;

  if (audio_card_is_low_latency) {
    // Perfect sync possible!
    // Write audio that will play exactly when next frame displays
    target_cursor = expected_frame_boundary + samples_per_frame;

#if HANDMADE_INTERNAL
    static int low_latency_count = 0;
    if (++low_latency_count <= 5) {
      printf("🎯 LOW LATENCY MODE: target=%ld (frame boundary + 1 frame)\n",
             target_cursor);
    }
#endif
  } else {
    // Can't achieve perfect sync, just stay ahead
    target_cursor =
        write_cursor + samples_per_frame + sound_output->safety_sample_count;

#if HANDMADE_INTERNAL
    static int high_latency_count = 0;
    if (++high_latency_count <= 5) {
      printf("⚠️  HIGH LATENCY MODE: target=%ld (write + frame + safety)\n",
             target_cursor);
    }
#endif
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔢 STEP 6: CALCULATE SAMPLES TO WRITE
  // ═══════════════════════════════════════════════════════════════
  //
  // SamplesToWrite = TargetCursor - WriteCursor
  //
  // But we can't write more than ALSA has room for!
  //
  // ═══════════════════════════════════════════════════════════════

  int64_t samples_to_write = target_cursor - write_cursor;

  // Clamp to available space
  if (samples_to_write > avail_frames) {
    samples_to_write = avail_frames;
  }

  // Clamp to our buffer size
  if (samples_to_write > (int64_t)g_linux_sound_output.sample_buffer_size) {
    samples_to_write = g_linux_sound_output.sample_buffer_size;
  }

  // Don't write negative samples!
  if (samples_to_write <= 0) {
#if HANDMADE_INTERNAL
    static int skip_count = 0;
    if (++skip_count <= 5) {
      printf("⏭️  Skipping write: samples_to_write=%ld, avail=%ld\n",
             samples_to_write, (long)avail_frames);
    }
#endif
    return;
  }

  // ═══════════════════════════════════════════════════════════════
  // 📊 STEP 7: CAPTURE DEBUG MARKER (Output State)
  // ═══════════════════════════════════════════════════════════════
  //
  // IMPORTANT: We capture BEFORE writing, not after!
  // This matches Casey's pattern exactly.
  //
  // We do NOT advance the marker index here - that happens after
  // the screen flip when we capture Flip* state.
  //
  // ═══════════════════════════════════════════════════════════════

#if HANDMADE_INTERNAL
  {
    LinuxDebugAudioMarker *marker =
        &g_debug_audio_markers[g_debug_marker_index];

    // Output state (BEFORE writing)
    marker->output_play_cursor = play_cursor;
    marker->output_write_cursor = safe_write_cursor;
    marker->output_location = write_cursor; // Where we're about to write
    marker->output_sample_count = samples_to_write;

    // Prediction: where will play cursor be at frame flip?
    marker->expected_flip_play_cursor = expected_frame_boundary;

    // Initialize flip cursors to invalid values
    // They'll be filled in by linux_debug_capture_flip_state()
    marker->flip_play_cursor = -1;
    marker->flip_write_cursor = -1;
  }
#endif

  // ═══════════════════════════════════════════════════════════════
  // 🎵 STEP 8: GENERATE AUDIO SAMPLES
  // ═══════════════════════════════════════════════════════════════
  //
  // This is where we actually create the audio waveform.
  // Currently a simple sine wave for testing.
  //
  // In a real game, this would call GameGetSoundSamples().
  //
  // ═══════════════════════════════════════════════════════════════

  int16_t *sample_out = (int16_t *)g_linux_sound_output.sample_buffer.base;

  real32 samples_per_cycle =
      (real32)sound_output->samples_per_second / (real32)sound_output->tone_hz;
  real32 phase_increment = M_PI_DOUBLED / samples_per_cycle;

  for (int64_t i = 0; i < samples_to_write; ++i) {
    real32 sine_value = sinf(sound_output->t_sine);
    int16_t sample_value = (int16_t)(sine_value * sound_output->tone_volume);

    // Apply panning
    int32_t left_gain = (100 - sound_output->pan_position);
    int32_t right_gain = (100 + sound_output->pan_position);
    int32_t left_sample = ((int32_t)sample_value * left_gain) / 200;
    int32_t right_sample = ((int32_t)sample_value * right_gain) / 200;

    *sample_out++ = (int16_t)left_sample;
    *sample_out++ = (int16_t)right_sample;

    // Advance phase
    sound_output->t_sine += phase_increment;
    if (sound_output->t_sine >= M_PI_DOUBLED) {
      sound_output->t_sine -= M_PI_DOUBLED;
    }
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔊 STEP 9: WRITE TO ALSA
  // ═══════════════════════════════════════════════════════════════
  //
  // snd_pcm_writei() is the ALSA equivalent of DirectSound's
  // Lock/memcpy/Unlock pattern.
  //
  // ALSA handles the ring buffer internally - we just give it
  // samples and it figures out where to put them.
  //
  // ═══════════════════════════════════════════════════════════════

  snd_pcm_sframes_t frames_written = SndPcmWritei(
      g_linux_sound_output.handle, g_linux_sound_output.sample_buffer.base,
      (snd_pcm_uframes_t)samples_to_write);

  if (frames_written < 0) {
    // Handle errors
    if (frames_written == -EPIPE) {
      // Underrun! Buffer ran empty.
      // This causes audio glitches (clicks/pops).
#if HANDMADE_INTERNAL
      printf("⚠️  AUDIO UNDERRUN at sample %ld\n", running_sample_index);
#endif
      SndPcmPrepare(g_linux_sound_output.handle);
      // Try writing again
      frames_written = SndPcmWritei(g_linux_sound_output.handle,
                                    g_linux_sound_output.sample_buffer.base,
                                    (snd_pcm_uframes_t)samples_to_write);
    } else if (frames_written == -EAGAIN) {
      // Buffer full, try again later
#if HANDMADE_INTERNAL
      printf("⚠️  AUDIO EAGAIN at sample %ld\n", running_sample_index);
#endif
      frames_written = 0;
    } else {
      // Other error, try to recover
      int recover_err =
          SndPcmRecover(g_linux_sound_output.handle, (int)frames_written, 1);
      if (recover_err < 0) {
        fprintf(stderr, "❌ ALSA recovery failed: %s\n",
                SndStrerror(recover_err));
        return;
      }
      frames_written = 0;
    }
  }

  // ═══════════════════════════════════════════════════════════════
  // 📈 STEP 10: UPDATE RUNNING SAMPLE INDEX
  // ═══════════════════════════════════════════════════════════════
  //
  // This is our "virtual write cursor" - it tracks how many
  // samples we've written total.
  //
  // IMPORTANT: Only advance by what we ACTUALLY wrote!
  //
  // ═══════════════════════════════════════════════════════════════

  if (frames_written > 0) {
    sound_output->running_sample_index += frames_written;
  }

#if HANDMADE_INTERNAL
  // Debug logging (every 5 seconds)
  static int frame_count = 0;
  frame_count++;
  if (frame_count % (sound_output->game_update_hz * 5) == 0) {
    printf("[AUDIO] RSI=%u, wrote=%ld/%ld, play=%ld, delay=%ld, avail=%ld\n",
           sound_output->running_sample_index, (long)frames_written,
           samples_to_write, play_cursor, (long)delay_frames,
           (long)avail_frames);
  }
#endif

  // Add this in linux_fill_sound_buffer after writing:
  printf("[RSI] Before=%ld, After=%u, Wrote=%ld\n", running_sample_index,
         sound_output->running_sample_index, frames_written);
}

// Debug helper: Print current audio latency (Day 10)
void linux_debug_audio_latency(GameSoundOutput *sound_output) {
  if (!sound_output->is_initialized) {
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

    long frames_available = SndPcmAvail(g_linux_sound_output.handle);

    printf("│ Frames available: %ld                                   │\n",
           frames_available);
    printf("│ Sample rate:     %d Hz                                 │\n",
           sound_output->samples_per_second);
    printf("│ Frequency:       %d Hz                                 │\n",
           sound_output->tone_hz);
    printf("│ Volume:          %d / 15000                            │\n",
           sound_output->tone_volume);
    printf("│ Pan:             %+d (L=%d, R=%d)                      │\n",
           sound_output->pan_position, 100 - sound_output->pan_position,
           100 + sound_output->pan_position);
    printf("└─────────────────────────────────────────────────────────┘\n");
    return;
  }

  // Day 10 mode - full latency stats
  printf("│ Mode: Day 10 (Latency-Aware)                          │\n");
  printf("│                                                         │\n");

  snd_pcm_sframes_t delay_frames = 0;
  int err = SndPcmDelay(g_linux_sound_output.handle, &delay_frames);

  if (err < 0) {
    printf("│ ❌ Can't measure delay: %s                              │\n",
           SndStrerror(err));
    printf("└─────────────────────────────────────────────────────────┘\n");
    return;
  }

  long frames_available = SndPcmAvail(g_linux_sound_output.handle);

  float actual_latency_ms =
      (float)delay_frames / sound_output->samples_per_second * 1000.0f;
  float target_latency_ms = (float)sound_output->latency_sample_count /
                            sound_output->samples_per_second * 1000.0f;

  printf("│ Target latency:  %.1f ms (%d frames)                 │\n",
         target_latency_ms, sound_output->latency_sample_count);
  printf("│ Actual latency:  %.1f ms (%ld frames)                │\n",
         actual_latency_ms, (long)delay_frames);

  // Color-code latency status
  float latency_diff = actual_latency_ms - target_latency_ms;
  if (fabs(latency_diff) < 5.0f) {
    printf("│ Status:          GOOD (±%.1fms)                       │\n",
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
         sound_output->samples_per_second);
  printf("│ Frequency:       %d Hz                                 │\n",
         sound_output->tone_hz);
  printf("│ Volume:          %d / 15000                            │\n",
         sound_output->tone_volume);
  printf("│ Pan:             %+d (L=%d, R=%d)                      │\n",
         sound_output->pan_position, 100 - sound_output->pan_position,
         100 + sound_output->pan_position);
  printf("└─────────────────────────────────────────────────────────┘\n");
}

void linux_unload_alsa(GameSoundOutput *sound_output) {
  printf("Unloading ALSA audio...\n");

  // Free sample buffer
  platform_free_memory(&g_linux_sound_output.sample_buffer);

  // Close PCM device
  if (g_linux_sound_output.handle) {
    SndPcmClose(g_linux_sound_output.handle);
    g_linux_sound_output.handle = NULL;
  }

  // Unload ALSA library
  if (g_linux_sound_output.alsa_library) {
    dlclose(g_linux_sound_output.alsa_library);
    g_linux_sound_output.alsa_library = NULL;
  }

  sound_output->is_initialized = false;

  printf("✅ ALSA audio unloaded.\n");
}