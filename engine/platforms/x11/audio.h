#ifndef DE100_PLATFORMS_X11_AUDIO_H
#define DE100_PLATFORMS_X11_AUDIO_H

#include "../../_common/base.h"
#include "../../_common/memory.h"
#include "../../game/audio.h"
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

typedef enum { LINUX_SND_PCM_FORMAT_S16_LE = 2 } linux_snd_pcm_format_t;

typedef enum { LINUX_SND_PCM_ACCESS_RW_INTERLEAVED = 3 } linux_snd_pcm_access_t;

typedef enum { LINUX_SND_PCM_STREAM_PLAYBACK = 0 } linux_snd_pcm_stream_t;

// ═══════════════════════════════════════════════════════════════
// ALSA Function Signatures
// ═══════════════════════════════════════════════════════════════

#define SND_PCM_NONBLOCK 0x00000001

#define ALSA_SND_PCM_OPEN(name)                                                \
  int name(snd_pcm_t **pcm, const char *device, int stream, int mode)
typedef ALSA_SND_PCM_OPEN(alsa_snd_pcm_open);

#define ALSA_SND_PCM_SET_PARAMS(name)                                          \
  int name(snd_pcm_t *pcm, int format, int access, unsigned int channels,      \
           unsigned int rate, int soft_resample, unsigned int latency)
typedef ALSA_SND_PCM_SET_PARAMS(alsa_snd_pcm_set_params);

#define ALSA_SND_PCM_WRITEI(name)                                              \
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

#define ALSA_SND_PCM_DELAY(name)                                               \
  int name(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
typedef ALSA_SND_PCM_DELAY(alsa_snd_pcm_delay);

#define ALSA_SND_PCM_GET_PARAMS(name)                                          \
  int name(snd_pcm_t *pcm, snd_pcm_uframes_t *buffer_size,                     \
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

// ═══════════════════════════════════════════════════════════════// 🔊 LINUX /
// ALSA PRIVATE AUDIO CONFIG
//
// Owned entirely by the X11 backend. Not visible to shared engine/ layer.
// Compare with PlatformAudioConfig (removed): all fields below are
// ALSA-specific ring-buffer accounting that means nothing to Raylib or any
// other backend.
// ══════════════════════════════════════════════════════════════

typedef struct {
  i32 samples_per_second;   /* Hardware sample rate (e.g. 48000) */
  i32 bytes_per_sample;     /* 4 for 16-bit stereo */
  i32 game_update_hz;       /* Game loop rate used to size latency (e.g. 60) */
  i32 latency_samples;      /* Write-ahead target: samples_per_frame * 2 */
  i32 safety_samples;       /* Guard margin: ~1/3 frame */
  i64 running_sample_index; /* Total samples written to ALSA — write cursor */
  bool is_initialized;      /* True after successful ALSA init */
} LinuxAudioConfig;

// ══════════════════════════════════════════════════════════════// 🔊 LINUX
// SOUND OUTPUT STATE
// ═══════════════════════════════════════════════════════════════

typedef struct {
  snd_pcm_t *pcm_handle;
  void *alsa_library;

  u32 buffer_size;

  De100MemoryBlock sample_buffer;
  u32 sample_buffer_size;

  i32 latency_sample_count;
  i32 latency_microseconds;
  i32 safety_sample_count;

  // Platform-level master volume (0.0 to 1.0, default 1.0)
  f32 master_volume;

  // Pause state
  bool is_paused;
} LinuxSoundOutput;

extern LinuxSoundOutput g_linux_audio_output;

// ═══════════════════════════════════════════════════════════════
// 📊 DEBUG AUDIO DISPLAY MODE
// ═══════════════════════════════════════════════════════════════
// Toggle between: hidden, semi-transparent, or fully visible
// ═══════════════════════════════════════════════════════════════

typedef enum {
  AUDIO_DEBUG_DISPLAY_NONE = 0,         // Hidden - no visualization
  AUDIO_DEBUG_DISPLAY_SEMI_TRANSPARENT, // Semi-transparent overlay
  AUDIO_DEBUG_DISPLAY_FULL              // Fully opaque (normal)
} AudioDebugDisplayMode;

extern AudioDebugDisplayMode g_audio_debug_display_mode;

// ═══════════════════════════════════════════════════════════════
// 📊 DAY 20: DEBUG AUDIO MARKER (X11/ALSA VERSION)
// ═══════════════════════════════════════════════════════════════
// This structure records audio timing data for visualization
// Equivalent to win32_debug_time_marker but adapted for ALSA
// ═══════════════════════════════════════════════════════════════

#if DE100_INTERNAL
#include "../../game/backbuffer.h"

typedef struct {
  // ══════════════════════════════════════════════════════════
  // CAPTURED BEFORE AUDIO WRITE (Output State)
  // ══════════════════════════════════════════════════════════
  // These are "virtual cursors" calculated from ALSA state
  // Unlike DirectSound, ALSA doesn't give us cursors directly
  // We calculate them from running_sample_index and delay

  i64 output_play_cursor;  // Virtual play cursor (RSI - delay)
  i64 output_write_cursor; // This is now the ACTUAL write cursor
  i64 output_location;     // Where we started writing (RSI)
  i64 output_sample_count; // How many samples we wrote

  // ══════════════════════════════════════════════════════════
  // PREDICTION
  // ══════════════════════════════════════════════════════════
  // Where we PREDICT the play cursor will be at frame flip
  i64 expected_flip_play_cursor;

  // ══════════════════════════════════════════════════════════
  // CAPTURED AFTER SCREEN FLIP (Flip State)
  // ══════════════════════════════════════════════════════════
  // Actual cursor positions after frame display
  i64 flip_play_cursor;  // Actual play cursor after flip
  i64 flip_write_cursor; // Actual write cursor after flip

  // ══════════════════════════════════════════════════════════
  // ALSA-SPECIFIC DATA (for debugging ALSA behavior)
  // ══════════════════════════════════════════════════════════
  snd_pcm_sframes_t output_delay_frames; // ALSA delay at output time
  snd_pcm_sframes_t output_avail_frames; // ALSA avail at output time
  snd_pcm_sframes_t flip_delay_frames;   // ALSA delay at flip time
  snd_pcm_sframes_t flip_avail_frames;   // ALSA avail at flip time
                                         //
  // ✅ NEW: Store safe write cursor for reference
  i64 output_safe_write_cursor; // ← ADD THIS

} LinuxDebugAudioMarker;

// How many frames of history to keep (0.5 seconds at 30 FPS)
#define MAX_DEBUG_AUDIO_MARKERS 15

// Global debug marker array
extern LinuxDebugAudioMarker g_debug_audio_markers[MAX_DEBUG_AUDIO_MARKERS];
extern int g_debug_marker_index;

// Capture flip state (called after frame display)
void linux_debug_capture_flip_state(LinuxAudioConfig *audio_config);

// Draw debug visualization (called every frame)
void linux_debug_sync_display(GameBackBuffer *buffer,
                              GameAudioOutputBuffer *audio_output,
                              LinuxAudioConfig *audio_config,
                              LinuxDebugAudioMarker *markers, int marker_count,
                              int current_marker_index);

#endif // DE100_INTERNAL

// ═══════════════════════════════════════════════════════════════
// 🔊 PUBLIC FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════════════

void linux_load_alsa(void);

/**
 * Initialize ALSA audio. On success sets audio_config->is_initialized = true
 * AND audio_output->is_initialized = true so the game can optionally check.
 */
bool linux_init_audio(LinuxAudioConfig *audio_config,
                      GameAudioOutputBuffer *audio_output,
                      i32 samples_per_second, i32 game_update_hz);

u32 linux_get_samples_to_write(LinuxAudioConfig *audio_config,
                               GameAudioOutputBuffer *audio_output);
void linux_debug_audio_latency(LinuxAudioConfig *audio_config);
void linux_unload_alsa(LinuxAudioConfig *audio_config);
void linux_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                     LinuxAudioConfig *audio_config);

void linux_send_samples_to_alsa(LinuxAudioConfig *audio_config,
                                GameAudioOutputBuffer *source);
void linux_clear_audio_buffer(LinuxAudioConfig *audio_config);

/** Pause ALSA playback (e.g., on focus loss). */
void linux_pause_audio(LinuxAudioConfig *audio_config);

/** Resume ALSA playback (e.g., on focus gain). */
void linux_resume_audio(LinuxAudioConfig *audio_config);

/** Reset debug statistics. */
void linux_audio_reset_stats(LinuxAudioConfig *audio_config);

/** Set platform-level master volume (0.0 to 1.0). */
void linux_set_master_volume(f32 volume);

// ═══════════════════════════════════════════════════════════════════════════
// TODO: FUTURE FEATURES
// ═══════════════════════════════════════════════════════════════════════════
//
// Multi-stream support:
//   ALSA uses a single snd_pcm_t device. For separate music + SFX streams,
//   either open multiple PCM devices (may not be supported by all hardware)
//   or do software mixing: two internal buffers mixed before snd_pcm_writei.
//
// Audio file loading:
//   X11/ALSA has no built-in file loading. Would need stb_vorbis, dr_wav,
//   or similar for WAV/OGG decoding. Pre-decode into PCM buffers in
//   GameMemory, then mix during get_audio_samples.
//
// ═══════════════════════════════════════════════════════════════════════════

#endif // DE100_PLATFORMS_X11_AUDIO_H
