
#ifndef X11_AUDIO_H
#define X11_AUDIO_H

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
// ALSA Type Definitions (from <alsa/asoundlib.h>)
// ───────────────────────────────────────────────────────────────
// We define these ourselves so we don't need the ALSA headers!
// This is how Casey avoids needing DirectSound headers.

typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;

// ALSA format enum (we only need S16_LE = signed 16-bit little-endian)
typedef enum {
  LINUX_SND_PCM_FORMAT_S16_LE = 2 // Signed 16-bit Little Endian
} linux_snd_pcm_format_t;

// ALSA access enum (we only need interleaved = L-R-L-R)
typedef enum {
  LINUX_SND_PCM_ACCESS_RW_INTERLEAVED = 3 //
} linux_snd_pcm_access_t;

// ALSA stream direction (playback = output)
typedef enum {
  LINUX_SND_PCM_STREAM_PLAYBACK = 0 //
} linux_snd_pcm_stream_t;


// ───────────────────────────────────────────────────────────────
// ALSA Function Signatures (Casey's macro pattern)
// ───────────────────────────────────────────────────────────────

// snd_pcm_open - Opens a PCM device
#define ALSA_SND_PCM_OPEN(name)                                                \
  int name(snd_pcm_t **pcm, const char *device, int stream, int mode)
typedef ALSA_SND_PCM_OPEN(alsa_snd_pcm_open);

// snd_pcm_set_params - Sets all parameters in one call
#define ALSA_SND_PCM_SET_PARAMS(name)                                          \
  int name(snd_pcm_t *pcm, int format, int access, unsigned int channels,      \
           unsigned int rate, int soft_resample, unsigned int latency)
typedef ALSA_SND_PCM_SET_PARAMS(alsa_snd_pcm_set_params);

// snd_pcm_writei - Write interleaved samples
#define ALSA_SND_PCM_WRITEI(name)                                              \
  long name(snd_pcm_t *pcm, const void *buffer, unsigned long frames)
typedef ALSA_SND_PCM_WRITEI(alsa_snd_pcm_writei);

// snd_pcm_prepare - Prepare PCM for use
#define ALSA_SND_PCM_PREPARE(name) int name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_PREPARE(alsa_snd_pcm_prepare);

// snd_pcm_close - Close PCM device
#define ALSA_SND_PCM_CLOSE(name) int name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_CLOSE(alsa_snd_pcm_close);

// snd_strerror - Get error string
#define ALSA_SND_STRERROR(name) const char *name(int errnum)
typedef ALSA_SND_STRERROR(alsa_snd_strerror);

// snd_pcm_avail - Get available frames in buffer
#define ALSA_SND_PCM_AVAIL(name) long name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_AVAIL(alsa_snd_pcm_avail);

// snd_pcm_recover - Recover from errors (underrun, etc.)
#define ALSA_SND_PCM_RECOVER(name) int name(snd_pcm_t *pcm, int err, int silent)
typedef ALSA_SND_PCM_RECOVER(alsa_snd_pcm_recover);

// ───────────────────────────────────────────────────────────────
// Stub Implementations (used when ALSA not available)
// ───────────────────────────────────────────────────────────────

ALSA_SND_PCM_OPEN(AlsaSndPcmOpenStub);

ALSA_SND_PCM_SET_PARAMS(AlsaSndPcmSetParamsStub);

ALSA_SND_PCM_WRITEI(AlsaSndPcmWriteiStub);

ALSA_SND_PCM_PREPARE(AlsaSndPcmPrepareStub);

ALSA_SND_PCM_CLOSE(AlsaSndPcmCloseStub) ;

ALSA_SND_STRERROR(AlsaSndStrerrorStub);

ALSA_SND_PCM_AVAIL(AlsaSndPcmAvailStub);

ALSA_SND_PCM_RECOVER(AlsaSndPcmRecoverStub) ;
 
/*
 * The keyword 'extern' is used here to declare a variable or function that is defined elsewhere,
 * typically in a corresponding .c (or .cpp) file. This tells the compiler that the actual storage
 * and initialization of the variable/function is not in this header, but will be provided at link time.
 *
 * Why not use 'static'?
 * - 'static' gives the variable/function internal linkage, meaning it is only visible within the file
 *   where it is defined. If you use 'static' in a header, each source file including the header gets
 *   its own private copy, which is usually not what you want for shared variables/functions.
 *
 * How does the definition work in both .c and .h files?
 * - In the header (.h), you use 'extern' to declare the existence of the variable/function.
 * - In one source (.c) file, you provide the actual definition (without 'extern').
 *   Example:
 *     // In audio.h
 *     extern int audioVolume;
 *
 *     // In audio.c
 *     int audioVolume = 100;
 *
 * This way, all files including audio.h know about 'audioVolume', but only audio.c allocates storage.
 *
 * 'extern' works in both C and C++ for this purpose.
 */
extern alsa_snd_pcm_open *SndPcmOpen_;
extern alsa_snd_pcm_set_params *SndPcmSetParams_;
extern alsa_snd_pcm_writei *SndPcmWritei_;
extern alsa_snd_pcm_prepare *SndPcmPrepare_;
extern alsa_snd_pcm_close *SndPcmClose_;
extern alsa_snd_strerror *SndStrerror_;
extern alsa_snd_pcm_avail *SndPcmAvail_;
extern alsa_snd_pcm_recover *SndPcmRecover_;

// Redefine names for clean API (Casey's pattern)
#define SndPcmOpen SndPcmOpen_
#define SndPcmSetParams SndPcmSetParams_
#define SndPcmWritei SndPcmWritei_
#define SndPcmPrepare SndPcmPrepare_
#define SndPcmClose SndPcmClose_
#define SndStrerror SndStrerror_
#define SndPcmAvail SndPcmAvail_
#define SndPcmRecover SndPcmRecover_


// ───────────────────────────────────────────────────────────────
// Sound Output State
// ───────────────────────────────────────────────────────────────
typedef struct {
  snd_pcm_t *handle;          // ALSA PCM handle
  void *alsa_library;         // dlopen handle (for cleanup if needed)
  int32_t samples_per_second; // 48000 Hz
  int32_t bytes_per_sample;   // 4 (16-bit stereo)
  uint32_t buffer_size;       // Secondary buffer size in bytes
  bool is_valid;              // Did initialization succeed?
} LinuxSoundOutput;

extern LinuxSoundOutput g_sound_output;

void linux_load_alsa(void);
void linux_init_sound(int32_t samples_per_second, int32_t buffer_size_bytes);

#endif // X11_AUDIO_H
