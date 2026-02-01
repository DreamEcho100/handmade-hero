// project/engine/platform/x11/audio.c
//
// ═══════════════════════════════════════════════════════════════════════════
// 🔊 ALSA AUDIO IMPLEMENTATION (Casey's DirectSound Pattern for Linux)
// ═══════════════════════════════════════════════════════════════════════════
//
// This is the Linux/ALSA equivalent of Casey's DirectSound audio code.
//
// KEY CONCEPTS:
// ─────────────────────────────────────────────────────────────────────────────
// 1. DYNAMIC LOADING: We load ALSA at runtime (like Casey loads DirectSound)
//    - Allows building without ALSA development headers
//    - Graceful fallback if ALSA isn't available
//
// 2. RING BUFFER: ALSA uses a ring buffer model
//    - Play cursor: Where hardware is currently playing
//    - Write cursor: Where we can safely write new data
//    - We calculate these from snd_pcm_delay() and snd_pcm_avail()
//
// 3. LOW LATENCY: We target ~2 frames of audio latency
//    - Safety margin prevents underruns
//    - Matches Casey's DirectSound approach
//
// ALSA vs DirectSound Comparison:
// ┌────────────────────────────┬────────────────────────────────────────────┐
// │ DirectSound (Windows)      │ ALSA (Linux)                               │
// ├────────────────────────────┼────────────────────────────────────────────┤
// │ IDirectSoundBuffer         │ snd_pcm_t*                                 │
// │ GetCurrentPosition()       │ snd_pcm_delay() + snd_pcm_avail()          │
// │ Lock()/Unlock()            │ snd_pcm_writei() (direct write)            │
// │ Play()/Stop()              │ snd_pcm_start()/snd_pcm_drop()             │
// │ SetFormat()                │ snd_pcm_set_params()                       │
// └────────────────────────────┴────────────────────────────────────────────┘
//
// ═══════════════════════════════════════════════════════════════════════════

#include "audio.h"

#include "../../_common/base.h"
#include "../../_common/dll.h"
#include "../../_common/memory.h"
#include "../../game/audio.h"
#include "../_common/config.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

LinuxSoundOutput g_linux_audio_output = {0};

#if DE100_INTERNAL
LinuxDebugAudioMarker g_debug_audio_markers[MAX_DEBUG_AUDIO_MARKERS] = {0};
int g_debug_marker_index = 0;
AudioDebugDisplayMode g_audio_debug_display_mode =
    AUDIO_DEBUG_DISPLAY_SEMI_TRANSPARENT;

// // Cycle through modes
// g_audio_debug_display_mode = (g_audio_debug_display_mode + 1) % 3;

// // Or set directly
// g_audio_debug_display_mode = AUDIO_DEBUG_DISPLAY_NONE;
// g_audio_debug_display_mode = AUDIO_DEBUG_DISPLAY_SEMI_TRANSPARENT;
// g_audio_debug_display_mode = AUDIO_DEBUG_DISPLAY_FULL;
#endif

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 ALSA FUNCTION POINTER GLOBALS (Initialized to stubs)
// ═══════════════════════════════════════════════════════════════════════════
//
// These are the actual function pointers that will either:
//   1. Point to stub functions (if ALSA fails to load)
//   2. Point to real ALSA functions (after successful dynamic load)
//
// The stubs ensure the program doesn't crash if ALSA is unavailable.
//
// ═══════════════════════════════════════════════════════════════════════════

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

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 STUB FUNCTIONS (Used when ALSA is unavailable)
// ═══════════════════════════════════════════════════════════════════════════
//
// These do nothing but return safe values.
// Allows the game to run without audio if ALSA isn't present.
//
// ═══════════════════════════════════════════════════════════════════════════

ALSA_SND_PCM_OPEN(AlsaSndPcmOpenStub) {
  (void)pcm;
  (void)device;
  (void)stream;
  (void)mode;
  return -1; // Failure
}

ALSA_SND_PCM_SET_PARAMS(AlsaSndPcmSetParamsStub) {
  (void)pcm;
  (void)format;
  (void)access;
  (void)channels;
  (void)rate;
  (void)soft_resample;
  (void)latency;
  return -1; // Failure
}

ALSA_SND_PCM_WRITEI(AlsaSndPcmWriteiStub) {
  (void)pcm;
  (void)buffer;
  (void)frames;
  return 0; // No frames written
}

ALSA_SND_PCM_PREPARE(AlsaSndPcmPrepareStub) {
  (void)pcm;
  return -1; // Failure
}

ALSA_SND_PCM_CLOSE(AlsaSndPcmCloseStub) {
  (void)pcm;
  return 0; // Success (nothing to close)
}

ALSA_SND_STRERROR(AlsaSndStrerrorStub) {
  (void)errnum;
  return "ALSA not loaded";
}

ALSA_SND_PCM_AVAIL(AlsaSndPcmAvailStub) {
  (void)pcm;
  return 0; // No samples available
}

ALSA_SND_PCM_RECOVER(AlsaSndPcmRecoverStub) {
  (void)pcm;
  (void)err;
  (void)silent;
  return -1; // Failure
}

ALSA_SND_PCM_DELAY(AlsaSndPcmDelayStub) {
  (void)pcm;
  (void)delayp;
  return -1; // Failure
}

ALSA_SND_PCM_GET_PARAMS(AlsaSndPcmGetParamsStub) {
  (void)pcm;
  (void)buffer_size;
  (void)period_size;
  return -1; // Failure
}

ALSA_SND_PCM_START(AlsaSndPcmStartStub) {
  (void)pcm;
  return -1; // Failure
}

ALSA_SND_PCM_DROP(AlsaSndPcmDropStub) {
  (void)pcm;
  return 0; // Success (nothing to drop)
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 DYNAMIC LOADING OF ALSA LIBRARY
// ═══════════════════════════════════════════════════════════════════════════
//
// This is Casey's pattern: load the audio library at runtime.
//
// WHY DYNAMIC LOADING?
// 1. No need for ALSA development headers at compile time
// 2. Game can run (without audio) if ALSA isn't installed
// 3. More control over initialization and error handling
//
// MEMORY LAYOUT:
// ┌───────────────────────────────────────────────────────────────────────┐
// │ Process Address Space                                                 │
// ├───────────────────────────────────────────────────────────────────────┤
// │ [Our Code]                                                            │
// │    ↓ dlopen("libasound.so.2")                                         │
// │ [libasound.so.2 mapped here]                                          │
// │    ↓ dlsym("snd_pcm_open")                                            │
// │ SndPcmOpen_ now points to real function in libasound                  │
// └───────────────────────────────────────────────────────────────────────┘
//
// ═══════════════════════════════════════════════════════════════════════════

void linux_load_alsa(void) {
  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 LOADING ALSA LIBRARY\n");
  printf("═══════════════════════════════════════════════════════════\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 1: Try to load the ALSA shared library
  // ─────────────────────────────────────────────────────────────────────
  //
  // libasound.so.2 is the main ALSA library on Linux.
  // RTLD_NOW: Resolve all symbols immediately (fail fast if missing)
  // RTLD_LOCAL: Don't export symbols to other loaded libraries
  //
  // ─────────────────────────────────────────────────────────────────────

  void *alsa_lib = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);

  if (!alsa_lib) {
    fprintf(stderr, "❌ Audio: Failed to load libasound.so.2: %s\n", dlerror());
    fprintf(stderr, "   Audio will be disabled.\n");
    fprintf(stderr, "   Install ALSA: sudo apt install libasound2\n");
    return;
  }

  g_linux_audio_output.alsa_library = alsa_lib;
  printf("✅ Audio: Loaded libasound.so.2\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 2: Load all required ALSA functions
  // ─────────────────────────────────────────────────────────────────────
  //
  // MACRO PATTERN:
  //   Name_ = (type*)dlsym(lib, "function_name");
  //   if (!Name_) Name_ = StubFunction;  // Fallback to stub
  //
  // This ensures we never have NULL function pointers.
  //
  // ─────────────────────────────────────────────────────────────────────

  // Load each function (order doesn't matter)
  // We use explicit casts since typeof() isn't portable

  SndPcmOpen_ = (alsa_snd_pcm_open *)dlsym(alsa_lib, "snd_pcm_open");
  if (!SndPcmOpen_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_open' not found\n");
    SndPcmOpen_ = AlsaSndPcmOpenStub;
  }

  SndPcmSetParams_ =
      (alsa_snd_pcm_set_params *)dlsym(alsa_lib, "snd_pcm_set_params");
  if (!SndPcmSetParams_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_set_params' not found\n");
    SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
  }

  SndPcmWritei_ = (alsa_snd_pcm_writei *)dlsym(alsa_lib, "snd_pcm_writei");
  if (!SndPcmWritei_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_writei' not found\n");
    SndPcmWritei_ = AlsaSndPcmWriteiStub;
  }

  SndPcmPrepare_ = (alsa_snd_pcm_prepare *)dlsym(alsa_lib, "snd_pcm_prepare");
  if (!SndPcmPrepare_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_prepare' not found\n");
    SndPcmPrepare_ = AlsaSndPcmPrepareStub;
  }

  SndPcmClose_ = (alsa_snd_pcm_close *)dlsym(alsa_lib, "snd_pcm_close");
  if (!SndPcmClose_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_close' not found\n");
    SndPcmClose_ = AlsaSndPcmCloseStub;
  }

  SndStrerror_ = (alsa_snd_strerror *)dlsym(alsa_lib, "snd_strerror");
  if (!SndStrerror_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_strerror' not found\n");
    SndStrerror_ = AlsaSndStrerrorStub;
  }

  SndPcmAvail_ = (alsa_snd_pcm_avail *)dlsym(alsa_lib, "snd_pcm_avail");
  if (!SndPcmAvail_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_avail' not found\n");
    SndPcmAvail_ = AlsaSndPcmAvailStub;
  }

  SndPcmRecover_ = (alsa_snd_pcm_recover *)dlsym(alsa_lib, "snd_pcm_recover");
  if (!SndPcmRecover_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_recover' not found\n");
    SndPcmRecover_ = AlsaSndPcmRecoverStub;
  }

  SndPcmDelay_ = (alsa_snd_pcm_delay *)dlsym(alsa_lib, "snd_pcm_delay");
  if (!SndPcmDelay_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_delay' not found\n");
    SndPcmDelay_ = AlsaSndPcmDelayStub;
  }

  SndPcmGetParams_ =
      (alsa_snd_pcm_get_params *)dlsym(alsa_lib, "snd_pcm_get_params");
  if (!SndPcmGetParams_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_get_params' not found\n");
    SndPcmGetParams_ = AlsaSndPcmGetParamsStub;
  }

  SndPcmStart_ = (alsa_snd_pcm_start *)dlsym(alsa_lib, "snd_pcm_start");
  if (!SndPcmStart_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_start' not found\n");
    SndPcmStart_ = AlsaSndPcmStartStub;
  }

  SndPcmDrop_ = (alsa_snd_pcm_drop *)dlsym(alsa_lib, "snd_pcm_drop");
  if (!SndPcmDrop_) {
    fprintf(stderr, "⚠️  Audio: Symbol 'snd_pcm_drop' not found\n");
    SndPcmDrop_ = AlsaSndPcmDropStub;
  }

  printf("✅ Audio: All ALSA functions loaded\n");
  printf("═══════════════════════════════════════════════════════════\n\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 INITIALIZE AUDIO SYSTEM
// ═══════════════════════════════════════════════════════════════════════════
//
// Opens the ALSA PCM device and configures it for low-latency playback.
//
// ALSA CONFIGURATION:
// ─────────────────────────────────────────────────────────────────────────────
// - Device: "default" (system default output)
// - Format: S16_LE (16-bit signed little-endian, like DirectSound)
// - Access: RW_INTERLEAVED (LRLRLR... sample layout)
// - Channels: 2 (stereo)
// - Rate: 48000 Hz (or as specified)
//
// LATENCY CALCULATION (Casey's Day 20):
// ─────────────────────────────────────────────────────────────────────────────
// Target: ~2 frames of audio latency
//
// latency_samples = (samples_per_second / game_update_hz) * FRAMES_OF_LATENCY
//
// Example at 48kHz, 30fps:
//   samples_per_frame = 48000 / 30 = 1600
//   latency_samples = 1600 * 2 = 3200 samples (~67ms)
//
// Safety margin: 1/3 of a frame to prevent underruns
//
// ═══════════════════════════════════════════════════════════════════════════

bool linux_init_audio(GameAudioOutputBuffer *audio_output,
                      PlatformAudioConfig *audio_config,
                      int32 samples_per_second, int32 buffer_size_bytes,
                      int32 game_update_hz) {
  (void)buffer_size_bytes; // We calculate our own buffer size

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 ALSA AUDIO INITIALIZATION\n");
  printf("═══════════════════════════════════════════════════════════\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 1: Check if ALSA was loaded
  // ─────────────────────────────────────────────────────────────────────

  if (!g_linux_audio_output.alsa_library) {
    fprintf(stderr, "❌ Audio: ALSA library not loaded\n");
    audio_config->is_initialized = false;
    return false;
  }

  // ─────────────────────────────────────────────────────────────────────
  // STEP 2: Open PCM device
  // ─────────────────────────────────────────────────────────────────────
  //
  // "default" = system default audio output
  // LINUX_SND_PCM_STREAM_PLAYBACK = output (not recording)
  // 0 = blocking mode (non-blocking is SND_PCM_NONBLOCK)
  //
  // ─────────────────────────────────────────────────────────────────────

  int err = SndPcmOpen(&g_linux_audio_output.pcm_handle, "default",
                       LINUX_SND_PCM_STREAM_PLAYBACK, 0);

  if (err < 0) {
    fprintf(stderr, "❌ Audio: Cannot open PCM device: %s\n", SndStrerror(err));
    audio_config->is_initialized = false;
    return false;
  }

  printf("✅ Audio: Opened PCM device 'default'\n");

  // ─────────────────────────────────────────────────────────────────────
  // STEP 3: Calculate latency and buffer sizes
  // ─────────────────────────────────────────────────────────────────────
  //
  // Casey's Day 20 latency model:
  //
  //   ┌─────────────────────────────────────────────────────────────────┐
  //   │ Frame N           │ Frame N+1         │ Frame N+2              │
  //   │ [game logic]      │ [game logic]      │ [game logic]           │
  //   │     ↓             │     ↓             │     ↓                  │
  //   │ [generate audio]  │ [generate audio]  │ [generate audio]       │
  //   │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ← audio buffer │
  //   │ ↑ play cursor     │              ↑ write cursor                │
  //   └─────────────────────────────────────────────────────────────────┘
  //
  // We need enough buffered audio so hardware never runs out!
  //
  // ─────────────────────────────────────────────────────────────────────

  int32 samples_per_frame = samples_per_second / game_update_hz;
  int32 latency_sample_count = samples_per_frame * FRAMES_OF_AUDIO_LATENCY;

  // Safety margin: 1/3 of a frame (prevents underruns due to timing variance)
  int32 safety_sample_count = samples_per_frame / 3;

  // Convert to microseconds for ALSA (they use µs for latency parameter)
  int32 latency_microseconds = (int32)((real64)latency_sample_count /
                                       (real64)samples_per_second * 1000000.0);

  printf("[AUDIO] Samples per frame: %d (at %d Hz game logic)\n",
         samples_per_frame, game_update_hz);
  printf("[AUDIO] Latency: %d samples (%.1f ms)\n", latency_sample_count,
         (float)latency_sample_count / samples_per_second * 1000.0f);
  printf("[AUDIO] Safety margin: %d samples (%.1f ms)\n", safety_sample_count,
         (float)safety_sample_count / samples_per_second * 1000.0f);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 4: Configure PCM parameters
  // ─────────────────────────────────────────────────────────────────────
  //
  // snd_pcm_set_params() is the "simple" API that sets all params at once.
  // For more control, you'd use snd_pcm_hw_params_*() functions.
  //
  // Parameters:
  //   - format: S16_LE (16-bit signed, little-endian)
  //   - access: RW_INTERLEAVED (samples are LRLRLR...)
  //   - channels: 2 (stereo)
  //   - rate: 48000 (or as specified)
  //   - soft_resample: 1 (allow ALSA to resample if hardware doesn't support)
  //   - latency: in microseconds
  //
  // ─────────────────────────────────────────────────────────────────────

  err = SndPcmSetParams(
      g_linux_audio_output.pcm_handle,
      LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed little-endian
      LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // Interleaved stereo (LRLRLR)
      2,                                   // Stereo
      (unsigned int)samples_per_second,    // Sample rate
      1,                                   // Allow soft resampling
      (unsigned int)latency_microseconds   // Target latency
  );

  if (err < 0) {
    fprintf(stderr, "❌ Audio: Cannot set PCM parameters: %s\n",
            SndStrerror(err));
    SndPcmClose(g_linux_audio_output.pcm_handle);
    g_linux_audio_output.pcm_handle = NULL;
    audio_config->is_initialized = false;
    return false;
  }

  printf("✅ Audio: PCM configured (%d Hz, 16-bit stereo)\n",
         samples_per_second);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 5: Query actual buffer/period sizes from ALSA
  // ─────────────────────────────────────────────────────────────────────
  //
  // ALSA may have adjusted our requested latency.
  // Query what we actually got.
  //
  // ─────────────────────────────────────────────────────────────────────

  snd_pcm_uframes_t actual_buffer_size = 0;
  snd_pcm_uframes_t actual_period_size = 0;

  err = SndPcmGetParams(g_linux_audio_output.pcm_handle, &actual_buffer_size,
                        &actual_period_size);

  if (err < 0) {
    fprintf(stderr, "⚠️  Audio: Cannot query params: %s\n", SndStrerror(err));
    // Continue anyway with our calculated values
    actual_buffer_size = (snd_pcm_uframes_t)latency_sample_count;
    actual_period_size = (snd_pcm_uframes_t)samples_per_frame;
  }

  printf("[AUDIO] ALSA buffer: %lu frames (%.1f ms)\n",
         (unsigned long)actual_buffer_size,
         (float)actual_buffer_size / samples_per_second * 1000.0f);
  printf("[AUDIO] ALSA period: %lu frames (%.1f ms)\n",
         (unsigned long)actual_period_size,
         (float)actual_period_size / samples_per_second * 1000.0f);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 6: Store configuration
  // ─────────────────────────────────────────────────────────────────────

  // Platform audio config (read by the whole platform layer)
  audio_config->samples_per_second = samples_per_second;
  audio_config->bytes_per_sample = sizeof(int16) * 2; // 16-bit stereo
  audio_config->running_sample_index = 0;
  audio_config->game_update_hz = game_update_hz;
  audio_config->latency_samples = latency_sample_count;
  audio_config->safety_samples = safety_sample_count;
  audio_config->buffer_size_bytes =
      (int32)actual_buffer_size * audio_config->bytes_per_sample;

  // Linux-specific state
  g_linux_audio_output.buffer_size = (uint32)actual_buffer_size;
  g_linux_audio_output.latency_sample_count = latency_sample_count;
  g_linux_audio_output.latency_microseconds = latency_microseconds;
  g_linux_audio_output.safety_sample_count = safety_sample_count;

  // ─────────────────────────────────────────────────────────────────────
  // STEP 7: Allocate sample buffer for game to fill
  // ─────────────────────────────────────────────────────────────────────
  //
  // This is where the game writes samples before we send them to ALSA.
  // We allocate enough for several frames worth (handles timing variance).
  //
  // ─────────────────────────────────────────────────────────────────────

  // Allocate enough for max_samples_per_call
  uint32 sample_buffer_size =
      audio_config->max_samples_per_call * audio_config->bytes_per_sample;

  g_linux_audio_output.sample_buffer =
      de100_memory_alloc(NULL, sample_buffer_size,
                         De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE |
                             De100_MEMORY_FLAG_ZEROED);

  if (!de100_memory_is_valid(g_linux_audio_output.sample_buffer)) {
    fprintf(stderr, "❌ Audio: Failed to allocate sample buffer (%d bytes)\n",
            sample_buffer_size);
    fprintf(
        stderr, "   Error: %s\n",
        de100_memory_error_str(g_linux_audio_output.sample_buffer.error_code));
    SndPcmClose(g_linux_audio_output.pcm_handle);
    g_linux_audio_output.pcm_handle = NULL;
    audio_config->is_initialized = false;
    return false;
  }

  g_linux_audio_output.sample_buffer_size = sample_buffer_size;

  printf("✅ Audio: Sample buffer allocated (%d bytes)\n", sample_buffer_size);

  // ─────────────────────────────────────────────────────────────────────
  // STEP 8: Configure game audio output buffer
  // ─────────────────────────────────────────────────────────────────────

  audio_output->samples_per_second = samples_per_second;
  audio_output->sample_count = 0; // Will be set each frame
  audio_output->samples = g_linux_audio_output.sample_buffer.base;

  // ─────────────────────────────────────────────────────────────────────
  // STEP 9: Pre-fill buffer with silence and start playback
  // ─────────────────────────────────────────────────────────────────────
  //
  // ALSA needs data BEFORE you call snd_pcm_start().
  // We fill with silence to prevent initial clicks/pops.
  //
  // ─────────────────────────────────────────────────────────────────────

  // Zero the sample buffer (silence)
  de100_mem_set(g_linux_audio_output.sample_buffer.base, 0,
                g_linux_audio_output.sample_buffer_size);

  // Write silence to prime the buffer
  snd_pcm_sframes_t frames_written = SndPcmWritei(
      g_linux_audio_output.pcm_handle, g_linux_audio_output.sample_buffer.base,
      (snd_pcm_uframes_t)latency_sample_count);

  if (frames_written < 0) {
    fprintf(stderr, "⚠️  Audio: Initial write failed: %s\n",
            SndStrerror((int)frames_written));
    // Try to recover
    SndPcmPrepare(g_linux_audio_output.pcm_handle);
  } else {
    printf("[AUDIO] Pre-filled buffer with %ld frames of silence\n",
           (long)frames_written);
  }

  // Start playback
  err = SndPcmStart(g_linux_audio_output.pcm_handle);
  if (err < 0) {
    fprintf(stderr, "⚠️  Audio: Cannot start PCM: %s\n", SndStrerror(err));
    // Non-fatal: ALSA might auto-start on first write
  }

  audio_config->is_initialized = true;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 AUDIO SYSTEM INITIALIZED\n");
  printf("═══════════════════════════════════════════════════════════\n\n");

  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 CALCULATE SAMPLES TO WRITE (Casey's Day 20 Pattern)
// ═══════════════════════════════════════════════════════════════════════════
//
// This is the CORE of Casey's audio synchronization approach.
//
// GOAL: Write enough samples to reach "target cursor" but not more.
//
// THE ALGORITHM:
// ─────────────────────────────────────────────────────────────────────────────
// 1. Get play cursor position (where hardware is currently playing)
// 2. Calculate where we've written up to (running_sample_index)
// 3. Calculate "target cursor" = play cursor + latency + safety
// 4. samples_to_write = target_cursor - where_we_are
//
// VISUALIZATION:
// ─────────────────────────────────────────────────────────────────────────────
//
//   Audio Buffer (Ring):
//   ┌─────────────────────────────────────────────────────────────────────┐
//   │░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░│
//   └─────────────────────────────────────────────────────────────────────┘
//                 ↑ Play Cursor                        ↑ Our Write Position
//                 (where hardware plays)               (running_sample_index)
//
//   We need to write to HERE:
//   ┌─────────────────────────────────────────────────────────────────────┐
//   │░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
//   └─────────────────────────────────────────────────────────────────────┘
//                 ↑                                                       ↑
//              Play Cursor                                    Target Cursor
//                          |←────── latency + safety ────────→|
//
// ═══════════════════════════════════════════════════════════════════════════

int32 linux_get_samples_to_write(PlatformAudioConfig *audio_config,
                                 GameAudioOutputBuffer *audio_output) {
  (void)audio_output;

  if (!audio_config->is_initialized || !g_linux_audio_output.pcm_handle) {
    return 0;
  }

  // ─────────────────────────────────────────────────────────────────────
  // STEP 1: Query ALSA for delay and available space
  // ─────────────────────────────────────────────────────────────────────
  //
  // snd_pcm_delay(): How many frames are in the buffer waiting to be played
  // snd_pcm_avail(): How many frames we can write without blocking
  //
  // From these we can calculate virtual "cursors":
  //   play_cursor = running_sample_index - delay
  //   write_cursor = play_cursor + (buffer_size - avail)
  //
  // ─────────────────────────────────────────────────────────────────────

  snd_pcm_sframes_t delay_frames = 0;
  int err = SndPcmDelay(g_linux_audio_output.pcm_handle, &delay_frames);

  if (err < 0) {
    // Underrun or error - try to recover
    err = SndPcmRecover(g_linux_audio_output.pcm_handle, err, 1);
    if (err < 0) {
      fprintf(stderr, "⚠️  Audio: Recovery failed: %s\n", SndStrerror(err));
      return 0;
    }
    delay_frames = 0;
  }

  snd_pcm_sframes_t avail_frames = SndPcmAvail(g_linux_audio_output.pcm_handle);

  if (avail_frames < 0) {
    // Error - try to recover
    err = SndPcmRecover(g_linux_audio_output.pcm_handle, (int)avail_frames, 1);
    if (err < 0) {
      return 0;
    }
    avail_frames = (snd_pcm_sframes_t)g_linux_audio_output.buffer_size;
  }

  // ─────────────────────────────────────────────────────────────────────
  // STEP 2: Calculate how many samples to write
  // ─────────────────────────────────────────────────────────────────────
  //
  // Casey's approach (Day 20):
  //   - Calculate expected play cursor position at frame flip time
  //   - Write samples up to that position + safety margin
  //
  // For ALSA, we use avail_frames directly:
  //   - avail_frames tells us how much space is in the buffer
  //   - We want to keep the buffer reasonably full (latency worth)
  //   - But not overfull (increases latency)
  //
  // ─────────────────────────────────────────────────────────────────────

  // Target: keep (latency + safety) samples in the buffer
  int32 target_buffered = g_linux_audio_output.latency_sample_count +
                          g_linux_audio_output.safety_sample_count;

  // Current buffered amount is approximately: buffer_size - avail
  int32 current_buffered =
      (int32)g_linux_audio_output.buffer_size - (int32)avail_frames;

  // We need to write enough to reach target
  int32 samples_to_write = target_buffered - current_buffered;

  // Clamp to available space
  if (samples_to_write > (int32)avail_frames) {
    samples_to_write = (int32)avail_frames;
  }

  // Don't write negative samples!
  if (samples_to_write < 0) {
    samples_to_write = 0;
  }

  // Clamp to our buffer capacity
  int32 max_samples = (int32)(g_linux_audio_output.sample_buffer_size /
                              audio_config->bytes_per_sample);
  if (samples_to_write > max_samples) {
    samples_to_write = max_samples;
  }

#if DE100_INTERNAL
  // ═══════════════════════════════════════════════════════════════════════
  // STORE DEBUG MARKERS (Casey's Day 23 Pattern - adapted for ALSA)
  // ═══════════════════════════════════════════════════════════════════════
  //
  // KEY DIFFERENCE from DirectSound:
  // - DirectSound: GetCurrentPosition() returns byte offsets within buffer
  // - ALSA: We get delay/avail, must calculate positions ourselves
  //
  // We store positions as BYTE OFFSETS within the buffer (0 to
  // buffer_size_bytes) This matches Casey's approach where assertions check:
  // cursor < buffer_size
  //
  // ═══════════════════════════════════════════════════════════════════════

  LinuxDebugAudioMarker *marker = &g_debug_audio_markers[g_debug_marker_index];

  uint32 buffer_size_bytes =
      g_linux_audio_output.buffer_size * audio_config->bytes_per_sample;

  // Convert running_sample_index to byte position within buffer (wrapped)
  // This is like Casey's ByteToLock calculation
  uint32 byte_to_lock = (uint32)((audio_config->running_sample_index *
                                  audio_config->bytes_per_sample) %
                                 buffer_size_bytes);

  // Calculate play cursor position within buffer
  // play_cursor_bytes = where hardware is currently playing
  // In ALSA: play_pos = (write_pos - delay) wrapped to buffer
  uint32 delay_bytes = (uint32)delay_frames * audio_config->bytes_per_sample;
  uint32 play_cursor_bytes;
  if (byte_to_lock >= delay_bytes) {
    play_cursor_bytes = byte_to_lock - delay_bytes;
  } else {
    // Handle wrap-around
    play_cursor_bytes = buffer_size_bytes - (delay_bytes - byte_to_lock);
  }

  // Write cursor = where we CAN write = play + (buffer - avail)
  // In DirectSound terms: WriteCursor is ahead of PlayCursor
  uint32 avail_bytes = (uint32)avail_frames * audio_config->bytes_per_sample;
  uint32 write_cursor_bytes =
      (play_cursor_bytes + (buffer_size_bytes - avail_bytes)) %
      buffer_size_bytes;

  // Expected flip play cursor = play cursor + one frame of samples
  int32 samples_per_frame =
      audio_config->samples_per_second / audio_config->game_update_hz;
  uint32 frame_bytes =
      (uint32)samples_per_frame * audio_config->bytes_per_sample;
  uint32 expected_flip_cursor =
      (play_cursor_bytes + frame_bytes) % buffer_size_bytes;

  // Bytes we're about to write
  uint32 bytes_to_write =
      (uint32)samples_to_write * audio_config->bytes_per_sample;

  // Store in marker (all values are now within [0, buffer_size_bytes))
  marker->output_play_cursor = play_cursor_bytes;
  marker->output_write_cursor = write_cursor_bytes;
  marker->output_location = byte_to_lock;       // Where we START writing
  marker->output_sample_count = bytes_to_write; // Now in BYTES to match Casey
  marker->output_delay_frames = delay_frames;
  marker->output_avail_frames = avail_frames;
  marker->expected_flip_play_cursor = expected_flip_cursor;

  // Safe write cursor (for visualization of target zone)
  uint32 safety_bytes = (uint32)g_linux_audio_output.safety_sample_count *
                        audio_config->bytes_per_sample;
  marker->output_safe_write_cursor =
      (write_cursor_bytes + safety_bytes) % buffer_size_bytes;
#endif

  return samples_to_write;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 SEND SAMPLES TO ALSA
// ═══════════════════════════════════════════════════════════════════════════
//
// Writes audio samples to the ALSA buffer.
// This is equivalent to DirectSound's Lock()/Unlock() pattern.
//
// ═══════════════════════════════════════════════════════════════════════════

void linux_send_samples_to_alsa(PlatformAudioConfig *audio_config,
                                GameAudioOutputBuffer *source) {
  if (!audio_config->is_initialized || !g_linux_audio_output.pcm_handle) {
    return;
  }

  if (!source->samples || source->sample_count <= 0) {
    return;
  }

  // ─────────────────────────────────────────────────────────────────────
  // Write samples to ALSA
  // ─────────────────────────────────────────────────────────────────────
  //
  // snd_pcm_writei() is the interleaved write function.
  // It can return:
  //   > 0: Number of frames written
  //   -EPIPE: Underrun occurred
  //   -EBADFD: Device in bad state
  //   -ESTRPIPE: Suspended
  //
  // ─────────────────────────────────────────────────────────────────────

  snd_pcm_sframes_t frames_written =
      SndPcmWritei(g_linux_audio_output.pcm_handle, source->samples,
                   (snd_pcm_uframes_t)source->sample_count);

  if (frames_written < 0) {
    // Error occurred - try to recover
    int err =
        SndPcmRecover(g_linux_audio_output.pcm_handle, (int)frames_written, 0);

    if (err < 0) {
      fprintf(stderr, "⚠️  Audio: Write recovery failed: %s\n",
              SndStrerror(err));
      return;
    }

    // Retry the write after recovery
    frames_written =
        SndPcmWritei(g_linux_audio_output.pcm_handle, source->samples,
                     (snd_pcm_uframes_t)source->sample_count);

    if (frames_written < 0) {
      fprintf(stderr, "⚠️  Audio: Write still failing after recovery\n");
      return;
    }
  }

  // Update running sample index
  audio_config->running_sample_index += frames_written;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 CLEAR AUDIO BUFFER (Send Silence)
// ═══════════════════════════════════════════════════════════════════════════

void linux_clear_audio_buffer(PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized || !g_linux_audio_output.pcm_handle) {
    return;
  }

  // Fill our buffer with silence
  de100_mem_set(g_linux_audio_output.sample_buffer.base, 0,
                g_linux_audio_output.sample_buffer_size);

  // Calculate how many samples to write (fill the buffer)
  int32 samples_to_clear = (int32)(g_linux_audio_output.sample_buffer_size /
                                   audio_config->bytes_per_sample);

  // Write silence
  SndPcmWritei(g_linux_audio_output.pcm_handle,
               g_linux_audio_output.sample_buffer.base,
               (snd_pcm_uframes_t)samples_to_clear);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 DEBUG AUDIO LATENCY
// ═══════════════════════════════════════════════════════════════════════════

void linux_debug_audio_latency(PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized) {
    printf("❌ Audio: Not initialized\n");
    return;
  }

  // Query current ALSA state
  snd_pcm_sframes_t delay_frames = 0;
  SndPcmDelay(g_linux_audio_output.pcm_handle, &delay_frames);

  snd_pcm_sframes_t avail_frames = SndPcmAvail(g_linux_audio_output.pcm_handle);

  float runtime_seconds = (float)audio_config->running_sample_index /
                          (float)audio_config->samples_per_second;

  float current_latency_ms =
      (float)delay_frames / audio_config->samples_per_second * 1000.0f;

  printf("┌─────────────────────────────────────────────────────────────┐\n");
  printf("│ 🔊 ALSA AUDIO DEBUG INFO                                    │\n");
  printf("├─────────────────────────────────────────────────────────────┤\n");
  printf("│ Mode: Ring buffer with snd_pcm_writei()                     │\n");
  printf("│                                                             │\n");
  printf("│ Sample rate:        %6d Hz                               │\n",
         audio_config->samples_per_second);
  printf("│ Bytes per sample:   %6d (16-bit stereo)                  │\n",
         audio_config->bytes_per_sample);
  printf("│ Buffer size:        %6u frames (%.1f ms)                 │\n",
         g_linux_audio_output.buffer_size,
         (float)g_linux_audio_output.buffer_size /
             audio_config->samples_per_second * 1000.0f);
  printf("│ Target latency:     %6d frames (%.1f ms)                 │\n",
         g_linux_audio_output.latency_sample_count,
         (float)g_linux_audio_output.latency_sample_count /
             audio_config->samples_per_second * 1000.0f);
  printf("│ Safety margin:      %6d frames (%.1f ms)                 │\n",
         g_linux_audio_output.safety_sample_count,
         (float)g_linux_audio_output.safety_sample_count /
             audio_config->samples_per_second * 1000.0f);
  printf("│ Game update rate:   %6d Hz                               │\n",
         audio_config->game_update_hz);
  printf("│                                                             │\n");
  printf("│ Running samples:    %10lld                              │\n",
         (long long)audio_config->running_sample_index);
  printf("│ Runtime:            %10.2f seconds                      │\n",
         runtime_seconds);
  printf("│                                                             │\n");
  printf("│ Current delay:      %6ld frames (%.1f ms latency)        │\n",
         (long)delay_frames, current_latency_ms);
  printf("│ Available space:    %6ld frames                          │\n",
         (long)avail_frames);
  printf("└─────────────────────────────────────────────────────────────┘\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 UNLOAD ALSA
// ═══════════════════════════════════════════════════════════════════════════

void linux_unload_alsa(PlatformAudioConfig *audio_config) {
  printf("🔊 Shutting down ALSA audio...\n");

  // Close PCM device
  if (g_linux_audio_output.pcm_handle) {
    SndPcmDrop(g_linux_audio_output.pcm_handle);
    SndPcmClose(g_linux_audio_output.pcm_handle);
    g_linux_audio_output.pcm_handle = NULL;
  }

  // Free sample buffer
  if (de100_memory_is_valid(g_linux_audio_output.sample_buffer)) {
    de100_memory_free(&g_linux_audio_output.sample_buffer);
  }

  // Unload ALSA library
  if (g_linux_audio_output.alsa_library) {
    dlclose(g_linux_audio_output.alsa_library);
    g_linux_audio_output.alsa_library = NULL;

    // Reset function pointers to stubs
    SndPcmOpen_ = AlsaSndPcmOpenStub;
    SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
    SndPcmWritei_ = AlsaSndPcmWriteiStub;
    SndPcmPrepare_ = AlsaSndPcmPrepareStub;
    SndPcmClose_ = AlsaSndPcmCloseStub;
    SndStrerror_ = AlsaSndStrerrorStub;
    SndPcmAvail_ = AlsaSndPcmAvailStub;
    SndPcmRecover_ = AlsaSndPcmRecoverStub;
    SndPcmDelay_ = AlsaSndPcmDelayStub;
    SndPcmGetParams_ = AlsaSndPcmGetParamsStub;
    SndPcmStart_ = AlsaSndPcmStartStub;
    SndPcmDrop_ = AlsaSndPcmDropStub;
  }

  audio_config->is_initialized = false;

  printf("✅ Audio: Shutdown complete\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 HANDLE FPS CHANGE
// ═══════════════════════════════════════════════════════════════════════════
//
// When the game changes FPS, we need to recalculate audio latency.
// Casey handles this implicitly through his latency calculations each frame.
// For us, we update the config values.
//
// ═══════════════════════════════════════════════════════════════════════════

void linux_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                     PlatformAudioConfig *audio_config) {
  (void)audio_output;

  if (!audio_config->is_initialized) {
    return;
  }

  // Recalculate latency values based on new game_update_hz
  int32 samples_per_frame =
      audio_config->samples_per_second / audio_config->game_update_hz;

  g_linux_audio_output.latency_sample_count =
      samples_per_frame * FRAMES_OF_AUDIO_LATENCY;

  g_linux_audio_output.safety_sample_count = samples_per_frame / 3;

  audio_config->latency_samples = g_linux_audio_output.latency_sample_count;
  audio_config->safety_samples = g_linux_audio_output.safety_sample_count;

  printf("[AUDIO] FPS changed: new latency=%d samples, safety=%d samples\n",
         g_linux_audio_output.latency_sample_count,
         g_linux_audio_output.safety_sample_count);
}

// ═══════════════════════════════════════════════════════════════════════════
// 📊 DEBUG: CAPTURE FLIP STATE (Called after screen flip)
// ═══════════════════════════════════════════════════════════════════════════
//
// Casey's Day 20: Record audio state AFTER the frame is displayed.
// This lets us compare predicted vs actual cursor positions.
//
// ═══════════════════════════════════════════════════════════════════════════

#if DE100_INTERNAL

void linux_debug_capture_flip_state(PlatformAudioConfig *audio_config) {
  if (!audio_config->is_initialized || !g_linux_audio_output.pcm_handle) {
    return;
  }

  LinuxDebugAudioMarker *marker = &g_debug_audio_markers[g_debug_marker_index];

  // Query current ALSA state
  snd_pcm_sframes_t delay_frames = 0;
  SndPcmDelay(g_linux_audio_output.pcm_handle, &delay_frames);

  snd_pcm_sframes_t avail_frames = SndPcmAvail(g_linux_audio_output.pcm_handle);
  if (avail_frames < 0)
    avail_frames = 0;

  // ═══════════════════════════════════════════════════════════════════════
  // Calculate byte positions within buffer (matching Casey's approach)
  // ═══════════════════════════════════════════════════════════════════════

  uint32 buffer_size_bytes =
      g_linux_audio_output.buffer_size * audio_config->bytes_per_sample;

  // Current write position within buffer
  uint32 current_byte_pos = (uint32)((audio_config->running_sample_index *
                                      audio_config->bytes_per_sample) %
                                     buffer_size_bytes);

  // Play cursor = write position - delay (wrapped)
  uint32 delay_bytes = (uint32)delay_frames * audio_config->bytes_per_sample;
  uint32 play_cursor_bytes;
  if (current_byte_pos >= delay_bytes) {
    play_cursor_bytes = current_byte_pos - delay_bytes;
  } else {
    play_cursor_bytes = buffer_size_bytes - (delay_bytes - current_byte_pos);
  }

  // Write cursor = play + (buffer - avail)
  uint32 avail_bytes = (uint32)avail_frames * audio_config->bytes_per_sample;
  uint32 write_cursor_bytes =
      (play_cursor_bytes + (buffer_size_bytes - avail_bytes)) %
      buffer_size_bytes;

  // Store flip state (byte positions within buffer)
  marker->flip_play_cursor = play_cursor_bytes;
  marker->flip_write_cursor = write_cursor_bytes;
  marker->flip_delay_frames = delay_frames;
  marker->flip_avail_frames = avail_frames;

  // Move to next marker slot
  g_debug_marker_index = (g_debug_marker_index + 1) % MAX_DEBUG_AUDIO_MARKERS;
}

// ═══════════════════════════════════════════════════════════════════════════
// 📊 DEBUG: DRAW AUDIO SYNC VISUALIZATION (ALSA-Native Version)
// ═══════════════════════════════════════════════════════════════════════════
//
// This visualization is adapted for ALSA's model which differs from
// DirectSound:
//
// DIRECTSOUND vs ALSA:
// ─────────────────────────────────────────────────────────────────────────────
// DirectSound:                    ALSA:
// - Ring buffer with cursors      - Write-and-forget model
// - PlayCursor/WriteCursor        - delay (queued frames) / avail (free space)
// - Cursors wrap at buffer size   - No exposed cursor positions
//
// WHAT WE VISUALIZE (ALSA semantics):
// ─────────────────────────────────────────────────────────────────────────────
// Screen width represents the ALSA buffer capacity (buffer_size frames).
//
// For each marker (frame), we show:
//   - White line:   How much audio is queued (delay_frames) - left side
//   - Red line:     How much space is available (avail_frames) - right side
//   - The gap between them shows buffer occupancy
//
// This is similar in SPIRIT to Casey's visualization:
//   - You can see if the buffer is too full (high latency) or too empty (risk
//   underrun)
//   - Lines should march steadily if audio timing is stable
//
// ═══════════════════════════════════════════════════════════════════════════

// Helper: Blend a color with the existing pixel (for semi-transparency)
de100_file_scoped_fn uint32 linux_debug_blend_color(uint32 existing,
                                                    uint32 color,
                                                    uint32 alpha) {
  // alpha: 0 = fully transparent, 255 = fully opaque
  uint32 inv_alpha = 255 - alpha;

  uint32 r_exist = (existing >> 16) & 0xFF;
  uint32 g_exist = (existing >> 8) & 0xFF;
  uint32 b_exist = existing & 0xFF;

  uint32 r_new = (color >> 16) & 0xFF;
  uint32 g_new = (color >> 8) & 0xFF;
  uint32 b_new = color & 0xFF;

  uint32 r = (r_new * alpha + r_exist * inv_alpha) / 255;
  uint32 g = (g_new * alpha + g_exist * inv_alpha) / 255;
  uint32 b = (b_new * alpha + b_exist * inv_alpha) / 255;

  return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Helper: Draw a horizontal region (bar) between two X positions
de100_file_scoped_fn void linux_debug_draw_bar(GameBackBuffer *buffer, int32 x1,
                                               int32 x2, int32 top,
                                               int32 bottom, uint32 color,
                                               uint32 alpha) {
  if (x1 > x2) {
    int32 tmp = x1;
    x1 = x2;
    x2 = tmp;
  }
  if (x1 < 0)
    x1 = 0;
  if (x2 > buffer->width)
    x2 = buffer->width;
  if (top < 0)
    top = 0;
  if (bottom > buffer->height)
    bottom = buffer->height;

  for (int32 y = top; y < bottom; y++) {
    uint32 *row_start = (uint32 *)((uint8 *)buffer->memory + y * buffer->pitch);
    for (int32 x = x1; x < x2; x++) {
      if (alpha >= 255) {
        row_start[x] = color;
      } else {
        row_start[x] = linux_debug_blend_color(row_start[x], color, alpha);
      }
    }
  }
}

void linux_debug_sync_display(GameBackBuffer *buffer,
                              GameAudioOutputBuffer *audio_output,
                              PlatformAudioConfig *audio_config,
                              LinuxDebugAudioMarker *markers, int marker_count,
                              int current_marker_index) {
  (void)audio_output;

  // Check display mode - skip if hidden
  if (g_audio_debug_display_mode == AUDIO_DEBUG_DISPLAY_NONE) {
    return;
  }

  if (!audio_config->is_initialized) {
    return;
  }

  // Alpha based on display mode
  uint32 alpha =
      (g_audio_debug_display_mode == AUDIO_DEBUG_DISPLAY_SEMI_TRANSPARENT)
          ? 128  // ~50% transparent
          : 255; // Fully opaque

  // Layout constants (smaller for less intrusive display)
  int32 pad_x = 8;
  int32 pad_y = 4;
  int32 row_height = 3;  // Height of each marker row (smaller)
  int32 row_spacing = 1; // Gap between rows

  // ALSA buffer size in frames
  uint32 buffer_size_frames = g_linux_audio_output.buffer_size;
  if (buffer_size_frames == 0)
    return;

  // Scale: map buffer frames to screen pixels
  int32 drawable_width = buffer->width - 2 * pad_x;
  real32 scale = (real32)drawable_width / (real32)buffer_size_frames;

  // Colors
  uint32 delay_color = 0xFFFFFFFF;   // White: queued/playing audio
  uint32 avail_color = 0xFF404040;   // Dark gray: available space (background)
  uint32 written_color = 0xFF00FF00; // Green: samples we just wrote
  uint32 target_color = 0xFFFFFF00;  // Yellow: target latency marker
  uint32 safety_color = 0xFFFF00FF;  // Magenta: safety margin

  // Target latency in frames (for reference line)
  int32 target_latency_frames = g_linux_audio_output.latency_sample_count;
  int32 safety_frames = g_linux_audio_output.safety_sample_count;

  // ═══════════════════════════════════════════════════════════════════════
  // ROW 0: Reference bar showing ideal buffer state
  // ═══════════════════════════════════════════════════════════════════════
  {
    int32 top = pad_y;
    int32 bottom = top + row_height;

    // Draw full buffer as dark background
    linux_debug_draw_bar(buffer, pad_x, pad_x + drawable_width, top, bottom,
                         avail_color, alpha);

    // Draw target latency zone (yellow)
    int32 target_x = pad_x + (int32)(scale * target_latency_frames);
    linux_debug_draw_bar(buffer, target_x, target_x + 2, top, bottom,
                         target_color, alpha);

    // Draw safety margin (magenta)
    int32 safety_x = pad_x + (int32)(scale * safety_frames);
    linux_debug_draw_bar(buffer, safety_x, safety_x + 2, top, bottom,
                         safety_color, alpha);
  }

  // ═══════════════════════════════════════════════════════════════════════
  // MARKER ROWS: Each frame gets one row showing buffer state
  // ═══════════════════════════════════════════════════════════════════════
  //
  // We draw markers from oldest to newest (bottom to top order visually)
  // This way the most recent marker is at the top, closest to row 0
  //
  // For each marker:
  //   - Bar from 0 to delay_frames = audio currently queued (white)
  //   - Bar from delay_frames to buffer_size = available space (dark)
  //   - Line at where we wrote = write position (green)
  //
  // ═══════════════════════════════════════════════════════════════════════

  for (int i = 0; i < marker_count; i++) {
    // Calculate which marker to draw (circular buffer indexing)
    // Start from oldest marker and go to newest
    int marker_idx = (current_marker_index + 1 + i) % marker_count;
    LinuxDebugAudioMarker *marker = &markers[marker_idx];

    // Skip uninitialized markers (delay == 0 and avail == 0 means unused)
    if (marker->flip_delay_frames == 0 && marker->flip_avail_frames == 0 &&
        marker->output_delay_frames == 0) {
      continue;
    }

    // Row position: newest at top (row 1), oldest at bottom
    int row_from_top = marker_count - 1 - i;
    int32 top = pad_y + (row_height + row_spacing) * (1 + row_from_top);
    int32 bottom = top + row_height;

    // Clamp to screen
    if (bottom > buffer->height - pad_y)
      continue;

    // Get ALSA state at flip time (this is the "actual" state when frame was
    // displayed)
    snd_pcm_sframes_t delay = marker->flip_delay_frames;
    snd_pcm_sframes_t avail = marker->flip_avail_frames;

    // Clamp to valid range
    if (delay < 0)
      delay = 0;
    if (delay > (snd_pcm_sframes_t)buffer_size_frames)
      delay = buffer_size_frames;
    if (avail < 0)
      avail = 0;

    // Draw available space (dark background for full bar)
    linux_debug_draw_bar(buffer, pad_x, pad_x + drawable_width, top, bottom,
                         avail_color, alpha);

    // Draw queued audio (white bar from left edge to delay position)
    // This represents audio that's buffered and waiting to play
    int32 delay_x = pad_x + (int32)(scale * delay);
    if (delay_x > pad_x) {
      linux_debug_draw_bar(buffer, pad_x, delay_x, top, bottom, delay_color,
                           alpha);
    }

    // For the CURRENT marker, also show output-time state
    if (i == marker_count - 1) {
      // Show where we wrote (green line at output_location, but in frames)
      // output_sample_count is in bytes, convert to frames
      int32 bytes_per_frame = audio_config->bytes_per_sample;
      int32 written_frames =
          (int32)marker->output_sample_count / bytes_per_frame;
      if (written_frames > 0) {
        int32 write_start_x = delay_x; // We write at the end of queued data
        int32 write_end_x = delay_x + (int32)(scale * written_frames);
        linux_debug_draw_bar(buffer, write_start_x, write_end_x, top, bottom,
                             written_color, alpha);
      }
    }
  }
}

#endif // DE100_INTERNAL
