#ifndef X11_AUDIO_H
#define X11_AUDIO_H

#include "../../base.h"
#include "../../game.h"
#include "../_common/memory.h"
#include <stdbool.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
// 🔊 ALSA AUDIO DYNAMIC LOADING (Casey's DirectSound Pattern)
// ═══════════════════════════════════════════════════════════════

// ALSA Type Definitions
typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;

typedef enum {
  LINUX_SND_PCM_FORMAT_S16_LE = 2
} linux_snd_pcm_format_t;

typedef enum {
  LINUX_SND_PCM_ACCESS_RW_INTERLEAVED = 3
} linux_snd_pcm_access_t;

typedef enum {
  LINUX_SND_PCM_STREAM_PLAYBACK = 0
} linux_snd_pcm_stream_t;

// ═══════════════════════════════════════════════════════════════
// ALSA Function Signatures
// ═══════════════════════════════════════════════════════════════

#define SND_PCM_NONBLOCK 0x00000001

#define ALSA_SND_PCM_OPEN(name) \
  int name(snd_pcm_t **pcm, const char *device, int stream, int mode)
typedef ALSA_SND_PCM_OPEN(alsa_snd_pcm_open);

#define ALSA_SND_PCM_SET_PARAMS(name) \
  int name(snd_pcm_t *pcm, int format, int access, unsigned int channels, \
           unsigned int rate, int soft_resample, unsigned int latency)
typedef ALSA_SND_PCM_SET_PARAMS(alsa_snd_pcm_set_params);

#define ALSA_SND_PCM_WRITEI(name) \
  long name(snd_pcm_t *pcm, const void *buffer, unsigned long frames)
typedef ALSA_SND_PCM_WRITEI(alsa_snd_pcm_writei);

#define ALSA_SND_PCM_PREPARE(name) int name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_PREPARE(alsa_snd_pcm_prepare);

#define ALSA_SND_PCM_CLOSE(name) int name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_CLOSE(alsa_snd_pcm_close);

#define ALSA_SND_STRERROR(name) const char *name(int errnum)
typedef ALSA_SND_STRERROR(alsa_snd_strerror);

#define ALSA_SND_PCM_AVAIL(name) long name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_AVAIL(alsa_snd_pcm_avail);

#define ALSA_SND_PCM_RECOVER(name) int name(snd_pcm_t *pcm, int err, int silent)
typedef ALSA_SND_PCM_RECOVER(alsa_snd_pcm_recover);

#define ALSA_SND_PCM_DELAY(name) \
  int name(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
typedef ALSA_SND_PCM_DELAY(alsa_snd_pcm_delay);

#define ALSA_SND_PCM_GET_PARAMS(name) \
  int name(snd_pcm_t *pcm, snd_pcm_uframes_t *buffer_size, \
           snd_pcm_uframes_t *period_size)
typedef ALSA_SND_PCM_GET_PARAMS(alsa_snd_pcm_get_params);

#define ALSA_SND_PCM_START(name) int name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_START(alsa_snd_pcm_start);
#define ALSA_SND_PCM_DROP(name) int name(snd_pcm_t *pcm)
typedef ALSA_SND_PCM_DROP(alsa_snd_pcm_drop);

// Stub declarations
ALSA_SND_PCM_OPEN(AlsaSndPcmOpenStub);
ALSA_SND_PCM_SET_PARAMS(AlsaSndPcmSetParamsStub);
ALSA_SND_PCM_WRITEI(AlsaSndPcmWriteiStub);
ALSA_SND_PCM_PREPARE(AlsaSndPcmPrepareStub);
ALSA_SND_PCM_CLOSE(AlsaSndPcmCloseStub);
ALSA_SND_STRERROR(AlsaSndStrerrorStub);
ALSA_SND_PCM_AVAIL(AlsaSndPcmAvailStub);
ALSA_SND_PCM_RECOVER(AlsaSndPcmRecoverStub);
ALSA_SND_PCM_DELAY(AlsaSndPcmDelayStub);
ALSA_SND_PCM_GET_PARAMS(AlsaSndPcmGetParamsStub);
ALSA_SND_PCM_START(AlsaSndPcmStartStub);
ALSA_SND_PCM_DROP(AlsaSndPcmDropStub);

// Global function pointers
extern alsa_snd_pcm_open *SndPcmOpen_;
extern alsa_snd_pcm_set_params *SndPcmSetParams_;
extern alsa_snd_pcm_writei *SndPcmWritei_;
extern alsa_snd_pcm_prepare *SndPcmPrepare_;
extern alsa_snd_pcm_close *SndPcmClose_;
extern alsa_snd_strerror *SndStrerror_;
extern alsa_snd_pcm_avail *SndPcmAvail_;
extern alsa_snd_pcm_recover *SndPcmRecover_;
extern alsa_snd_pcm_delay *SndPcmDelay_;
extern alsa_snd_pcm_get_params *SndPcmGetParams_;
extern alsa_snd_pcm_start *SndPcmStart_;
extern alsa_snd_pcm_drop *SndPcmDrop_;

// Clean API names
#define SndPcmOpen SndPcmOpen_
#define SndPcmSetParams SndPcmSetParams_
#define SndPcmWritei SndPcmWritei_
#define SndPcmPrepare SndPcmPrepare_
#define SndPcmClose SndPcmClose_
#define SndStrerror SndStrerror_
#define SndPcmAvail SndPcmAvail_
#define SndPcmRecover SndPcmRecover_
#define SndPcmDelay SndPcmDelay_
#define SndPcmGetParams SndPcmGetParams_
#define SndPcmStart SndPcmStart_
#define SndPcmDrop SndPcmDrop_

// ═══════════════════════════════════════════════════════════════
// 🔊 LINUX SOUND OUTPUT STATE
// ═══════════════════════════════════════════════════════════════

typedef struct {
  snd_pcm_t *handle;
  void *alsa_library;
  
  uint32_t buffer_size;
  
  PlatformMemoryBlock sample_buffer;
  uint32_t sample_buffer_size;
  
  int32_t latency_sample_count;
  int32_t latency_microseconds;

  // Day 20DirectSound has SafetyBytes:
  // - Accounts for frame timing variance
  // - Prevents audio underruns
  // - Calculated as: (SamplesPerSecond * BytesPerSample / GameUpdateHz) / 3

  // ALSA needs safety_sample_count:
  // - Same purpose
  // - Same calculation (but in samples, not bytes)
  // - Formula: (samples_per_second / game_update_hz) / 3
  int32_t safety_sample_count;  // Safety margin (1/3 frame worth of samples)
} LinuxSoundOutput;

extern LinuxSoundOutput g_linux_sound_output;

// ═══════════════════════════════════════════════════════════════
// 📊 DAY 20: DEBUG AUDIO MARKER (X11/ALSA VERSION)
// ═══════════════════════════════════════════════════════════════
// This structure records audio timing data for visualization
// Equivalent to win32_debug_time_marker but adapted for ALSA
// ═══════════════════════════════════════════════════════════════

#if HANDMADE_INTERNAL

typedef struct {
    // ══════════════════════════════════════════════════════════
    // CAPTURED BEFORE AUDIO WRITE (Output State)
    // ══════════════════════════════════════════════════════════
    // These are "virtual cursors" calculated from ALSA state
    // Unlike DirectSound, ALSA doesn't give us cursors directly
    // We calculate them from running_sample_index and delay
    
    int64_t output_play_cursor;      // Virtual play cursor (RSI - delay)
    int64_t output_write_cursor;     // This is now the ACTUAL write cursor
    int64_t output_location;         // Where we started writing (RSI)
    int64_t output_sample_count;     // How many samples we wrote
    
    // ══════════════════════════════════════════════════════════
    // PREDICTION
    // ══════════════════════════════════════════════════════════
    // Where we PREDICT the play cursor will be at frame flip
    int64_t expected_flip_play_cursor;
    
    // ══════════════════════════════════════════════════════════
    // CAPTURED AFTER SCREEN FLIP (Flip State)
    // ══════════════════════════════════════════════════════════
    // Actual cursor positions after frame display
    int64_t flip_play_cursor;        // Actual play cursor after flip
    int64_t flip_write_cursor;       // Actual write cursor after flip
    
    // ══════════════════════════════════════════════════════════
    // ALSA-SPECIFIC DATA (for debugging ALSA behavior)
    // ══════════════════════════════════════════════════════════
    snd_pcm_sframes_t output_delay_frames;  // ALSA delay at output time
    snd_pcm_sframes_t output_avail_frames;  // ALSA avail at output time
    snd_pcm_sframes_t flip_delay_frames;    // ALSA delay at flip time
    snd_pcm_sframes_t flip_avail_frames;    // ALSA avail at flip time
                                            // 
    // ✅ NEW: Store safe write cursor for reference
    int64_t output_safe_write_cursor;  // ← ADD THIS
    
} LinuxDebugAudioMarker;

// How many frames of history to keep (0.5 seconds at 30 FPS)
#define MAX_DEBUG_AUDIO_MARKERS 15

// Global debug marker array
extern LinuxDebugAudioMarker g_debug_audio_markers[MAX_DEBUG_AUDIO_MARKERS];
extern int g_debug_marker_index;

// Capture flip state (called after frame display)
void linux_debug_capture_flip_state(GameSoundOutput *sound_output);

// Draw debug visualization (called every frame)
void linux_debug_sync_display(
    GameOffscreenBuffer *buffer,
    GameSoundOutput *sound_output,
    LinuxDebugAudioMarker *markers,
    int marker_count,
    int current_marker_index);

#endif // HANDMADE_INTERNAL

// ═══════════════════════════════════════════════════════════════
// 🔊 PUBLIC FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════════════

void linux_load_alsa(void);

bool linux_init_sound(
    GameSoundOutput *sound_output,
    int32_t samples_per_second,
    int32_t buffer_size_bytes,
    int32_t game_update_hz);

void linux_fill_sound_buffer(GameSoundOutput *sound_output);
void linux_debug_audio_latency(GameSoundOutput *sound_output);
void linux_unload_alsa(GameSoundOutput *sound_output);
void linux_audio_fps_change_handling(GameSoundOutput *sound_output);

#endif // X11_AUDIO_H
