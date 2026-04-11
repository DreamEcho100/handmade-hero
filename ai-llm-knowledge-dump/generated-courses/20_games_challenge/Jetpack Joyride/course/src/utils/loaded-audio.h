#ifndef UTILS_LOADED_AUDIO_H
#define UTILS_LOADED_AUDIO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/loaded-audio.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Loaded audio system: WAV/OGG file loading and playback via voice pool.
 * Separate from the procedural ToneGenerator system in audio.h — both
 * coexist in the audio callback.
 *
 * LESSON 18 — WAV loading, voice pool, mixer.
 * LESSON 19 — OGG loading via stb_vorbis, pitch variation, music voice.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include "./audio.h"

/* ── AudioClip ───────────────────────────────────────────────────────── */
/* A decoded audio clip stored as float32 interleaved stereo.             */
typedef struct {
  float *samples;      /* Interleaved stereo float32 PCM data */
  int sample_count;    /* Total number of frames (L+R pairs) */
  int sample_rate;     /* Original sample rate (e.g. 44100) */
  int channels;        /* 1=mono, 2=stereo */
} AudioClip;

/* ── AudioVoice ──────────────────────────────────────────────────────── */
/* One active playback instance of a clip.                                */
typedef struct {
  AudioClip *clip;     /* Which clip is playing (NULL = inactive) */
  float position;      /* Current playback position (fractional frames) */
  float volume;        /* 0.0 - 1.0 */
  float pitch;         /* 1.0 = normal speed; 0.9-1.1 for variation */
  float pan;           /* -1.0 = left, 0.0 = center, 1.0 = right */
  int active;          /* 1 = playing */
  int loop;            /* 1 = loop forever */
  int age;             /* For steal-oldest policy */
} AudioVoice;

#define MAX_AUDIO_VOICES 16
#define MAX_AUDIO_CLIPS 32

/* ── LoadedAudioState ────────────────────────────────────────────────── */
typedef struct {
  AudioClip clips[MAX_AUDIO_CLIPS];
  int clip_count;

  AudioVoice voices[MAX_AUDIO_VOICES];
  int next_age;

  /* Dedicated music voice (never stolen by SFX) */
  AudioVoice music_voice;

  /* Volume controls */
  float master_volume;
  float sfx_volume;
  float music_volume;
} LoadedAudioState;

/* ── Loading ─────────────────────────────────────────────────────────── */

/* Load a WAV file into an AudioClip. Supports PCM 16-bit mono/stereo.
 * Returns 0 on success, -1 on failure. */
int audioclip_load_wav(const char *path, AudioClip *out);

/* Load an OGG Vorbis file into an AudioClip via stb_vorbis.
 * Returns 0 on success, -1 on failure. */
int audioclip_load_ogg(const char *path, AudioClip *out);

/* Free an AudioClip's sample data. */
void audioclip_free(AudioClip *clip);

/* ── Playback ────────────────────────────────────────────────────────── */

/* Initialize the loaded audio state. */
void loaded_audio_init(LoadedAudioState *state);

/* Load a clip and register it. Returns clip index, or -1 on failure. */
int loaded_audio_load_wav(LoadedAudioState *state, const char *path);
int loaded_audio_load_ogg(LoadedAudioState *state, const char *path);

/* Play a sound effect. Uses free voice or steals oldest.
 * clip_idx: index returned by loaded_audio_load_*
 * volume: 0.0-1.0, pitch: playback rate, pan: -1 to +1 */
void loaded_audio_play(LoadedAudioState *state, int clip_idx,
                       float volume, float pitch, float pan);

/* Play music (dedicated voice, loops). */
void loaded_audio_play_music(LoadedAudioState *state, int clip_idx,
                             float volume);

/* Stop music. */
void loaded_audio_stop_music(LoadedAudioState *state);

/* Mix all active voices into the audio output buffer.
 * Called from the audio callback. */
void loaded_audio_mix(LoadedAudioState *state, AudioOutputBuffer *buf,
                      int num_frames);

#endif /* UTILS_LOADED_AUDIO_H */
