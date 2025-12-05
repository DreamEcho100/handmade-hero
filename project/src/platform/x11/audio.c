#include "audio.h"
#include "../base.h"
#include <dlfcn.h> // 🆕 For dlopen, dlsym, dlclose (Casey's LoadLibrary equivalent)
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
#undef LOAD_ALSA_FN

  // Verify critical functions loaded
  if (!SndPcmOpen_ || !SndPcmSetParams_ || !SndPcmWritei_) {
    fprintf(stderr, "❌ ALSA: Missing critical functions, audio disabled\n");
    // Reset to stubs
    SndPcmOpen_ = AlsaSndPcmOpenStub;
    SndPcmSetParams_ = AlsaSndPcmSetParamsStub;
    SndPcmWritei_ = AlsaSndPcmWriteiStub;
    dlclose(alsa_lib);
    g_sound_output.alsa_library = NULL;
  }
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
  // Casey uses ~1/15th second buffer (~66ms) in Day 7
  // We'll use 100000 microseconds (100ms) for safety
  unsigned int latency_us = 100000; // 100ms in microseconds

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
  g_sound_output.is_valid = true;

  printf("✅ Sound: Initialized!\n");
  printf("   Sample rate:  %d Hz\n", samples_per_second);
  printf("   Buffer size:  %d bytes\n", buffer_size_bytes);
  printf("   Latency:      %.1f ms\n", latency_us / 1000.0f);
}
