/* ═══════════════════════════════════════════════════════════════════════════
 * utils/loaded-audio.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WAV/OGG audio loading and voice-pool mixer.
 *
 * WAV parser: Hand-rolled minimal RIFF/WAV decoder supporting PCM 16-bit
 * mono and stereo at any sample rate. Converts int16 → float32.
 *
 * OGG decoder: Uses stb_vorbis for Vorbis decompression.
 *
 * Mixer: Iterates all active voices, advances fractional position by
 * pitch rate, linearly interpolates between samples, mixes into output.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./loaded-audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── stb_vorbis — compiled in loaded-audio.c ─────────────────────────── */
/* Raylib also bundles stb_vorbis internally. To avoid duplicate symbol
 * errors, the Raylib build uses --allow-multiple-definition (both copies
 * are identical). The X11 build has no conflict.                        */
#define STB_VORBIS_NO_PUSHDATA_API
#include "../../vendor/stb_vorbis.c"

/* ═══════════════════════════════════════════════════════════════════════════
 * WAV file format structures
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * RIFF WAVE format:
 *   "RIFF" [file_size-8] "WAVE"
 *   "fmt " [chunk_size] [audio_format] [channels] [sample_rate] ...
 *   "data" [data_size] [samples...]
 */

/* Read a 32-bit little-endian value from a byte buffer. */
static uint32_t read_u32_le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int audioclip_load_wav(const char *path, AudioClip *out) {
  memset(out, 0, sizeof(*out));

  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "audioclip_load_wav: cannot open '%s'\n", path);
    return -1;
  }

  /* Read entire file into memory */
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint8_t *data = (uint8_t *)malloc((size_t)file_size);
  if (!data) {
    fclose(f);
    return -1;
  }
  if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
    free(data);
    fclose(f);
    return -1;
  }
  fclose(f);

  /* Validate RIFF/WAVE header */
  if (file_size < 44 ||
      memcmp(data, "RIFF", 4) != 0 ||
      memcmp(data + 8, "WAVE", 4) != 0) {
    fprintf(stderr, "audioclip_load_wav: '%s' is not a valid WAV file\n", path);
    free(data);
    return -1;
  }

  /* Find fmt chunk */
  uint8_t *fmt_chunk = NULL;
  uint8_t *data_chunk = NULL;
  uint32_t data_size = 0;

  size_t pos = 12; /* After "RIFF" + size + "WAVE" */
  while (pos + 8 <= (size_t)file_size) {
    uint32_t chunk_size = read_u32_le(data + pos + 4);
    if (memcmp(data + pos, "fmt ", 4) == 0) {
      fmt_chunk = data + pos + 8;
    } else if (memcmp(data + pos, "data", 4) == 0) {
      data_chunk = data + pos + 8;
      data_size = chunk_size;
    }
    pos += 8 + chunk_size;
    if (chunk_size & 1) pos++; /* Pad to even boundary */
  }

  if (!fmt_chunk || !data_chunk) {
    fprintf(stderr, "audioclip_load_wav: '%s' missing fmt/data chunks\n", path);
    free(data);
    return -1;
  }

  /* Parse fmt chunk */
  uint16_t audio_format = read_u16_le(fmt_chunk);
  uint16_t channels = read_u16_le(fmt_chunk + 2);
  uint32_t sample_rate = read_u32_le(fmt_chunk + 4);
  uint16_t bits_per_sample = read_u16_le(fmt_chunk + 14);

  if (audio_format != 1) { /* 1 = PCM */
    fprintf(stderr, "audioclip_load_wav: '%s' not PCM format (%d)\n",
            path, audio_format);
    free(data);
    return -1;
  }

  if (bits_per_sample != 16) {
    fprintf(stderr, "audioclip_load_wav: '%s' not 16-bit (%d-bit)\n",
            path, bits_per_sample);
    free(data);
    return -1;
  }

  /* Convert int16 samples to float32 interleaved stereo */
  int total_samples = (int)(data_size / (bits_per_sample / 8));
  int frame_count = total_samples / (int)channels;

  /* Allocate stereo output (always 2 channels) */
  out->samples = (float *)malloc((size_t)frame_count * 2 * sizeof(float));
  if (!out->samples) {
    free(data);
    return -1;
  }

  int16_t *src = (int16_t *)data_chunk;
  for (int i = 0; i < frame_count; i++) {
    float left, right;
    if (channels == 1) {
      /* Mono → duplicate to both channels */
      left = right = (float)src[i] / 32768.0f;
    } else {
      /* Stereo */
      left  = (float)src[i * 2]     / 32768.0f;
      right = (float)src[i * 2 + 1] / 32768.0f;
    }
    out->samples[i * 2]     = left;
    out->samples[i * 2 + 1] = right;
  }

  out->sample_count = frame_count;
  out->sample_rate = (int)sample_rate;
  out->channels = 2; /* Always stored as stereo */

  free(data);
  return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * audioclip_load_ogg — OGG Vorbis via stb_vorbis
 * ═══════════════════════════════════════════════════════════════════════════
 */
int audioclip_load_ogg(const char *path, AudioClip *out) {
  memset(out, 0, sizeof(*out));

  int channels, sample_rate;
  short *decoded;
  int frame_count = stb_vorbis_decode_filename(path, &channels, &sample_rate,
                                                &decoded);
  if (frame_count <= 0) {
    fprintf(stderr, "audioclip_load_ogg: failed to decode '%s'\n", path);
    return -1;
  }

  /* Convert int16 to float32 interleaved stereo */
  out->samples = (float *)malloc((size_t)frame_count * 2 * sizeof(float));
  if (!out->samples) {
    free(decoded);
    return -1;
  }

  for (int i = 0; i < frame_count; i++) {
    float left, right;
    if (channels == 1) {
      left = right = (float)decoded[i] / 32768.0f;
    } else {
      left  = (float)decoded[i * 2]     / 32768.0f;
      right = (float)decoded[i * 2 + 1] / 32768.0f;
    }
    out->samples[i * 2]     = left;
    out->samples[i * 2 + 1] = right;
  }

  out->sample_count = frame_count;
  out->sample_rate = sample_rate;
  out->channels = 2;

  free(decoded);
  return 0;
}

void audioclip_free(AudioClip *clip) {
  if (clip->samples) {
    free(clip->samples);
    clip->samples = NULL;
  }
  clip->sample_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LoadedAudioState — voice pool management
 * ═══════════════════════════════════════════════════════════════════════════
 */
void loaded_audio_init(LoadedAudioState *state) {
  memset(state, 0, sizeof(*state));
  state->master_volume = 0.6f;
  state->sfx_volume = 1.0f;
  state->music_volume = 0.5f;
}

int loaded_audio_load_wav(LoadedAudioState *state, const char *path) {
  if (state->clip_count >= MAX_AUDIO_CLIPS) return -1;
  int idx = state->clip_count;
  if (audioclip_load_wav(path, &state->clips[idx]) != 0) return -1;
  state->clip_count++;
  return idx;
}

int loaded_audio_load_ogg(LoadedAudioState *state, const char *path) {
  if (state->clip_count >= MAX_AUDIO_CLIPS) return -1;
  int idx = state->clip_count;
  if (audioclip_load_ogg(path, &state->clips[idx]) != 0) return -1;
  state->clip_count++;
  return idx;
}

void loaded_audio_play(LoadedAudioState *state, int clip_idx,
                       float volume, float pitch, float pan) {
  if (clip_idx < 0 || clip_idx >= state->clip_count) return;

  /* Find free voice */
  AudioVoice *voice = NULL;
  for (int i = 0; i < MAX_AUDIO_VOICES; i++) {
    if (!state->voices[i].active) {
      voice = &state->voices[i];
      break;
    }
  }

  /* Steal oldest if no free voice */
  if (!voice) {
    int oldest_age = state->next_age;
    int oldest_idx = 0;
    for (int i = 0; i < MAX_AUDIO_VOICES; i++) {
      if (state->voices[i].age < oldest_age) {
        oldest_age = state->voices[i].age;
        oldest_idx = i;
      }
    }
    voice = &state->voices[oldest_idx];
  }

  voice->clip = &state->clips[clip_idx];
  voice->position = 0.0f;
  voice->volume = volume;
  voice->pitch = pitch;
  voice->pan = pan;
  voice->active = 1;
  voice->loop = 0;
  voice->age = state->next_age++;
}

void loaded_audio_play_music(LoadedAudioState *state, int clip_idx,
                             float volume) {
  if (clip_idx < 0 || clip_idx >= state->clip_count) return;

  state->music_voice.clip = &state->clips[clip_idx];
  state->music_voice.position = 0.0f;
  state->music_voice.volume = volume;
  state->music_voice.pitch = 1.0f;
  state->music_voice.pan = 0.0f;
  state->music_voice.active = 1;
  state->music_voice.loop = 1;
}

void loaded_audio_stop_music(LoadedAudioState *state) {
  state->music_voice.active = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * loaded_audio_mix — mix all voices into output buffer
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * For each output frame, iterate all active voices:
 *   1. Compute fractional source position (voice->position)
 *   2. Linear interpolate between adjacent source samples
 *   3. Apply volume and pan
 *   4. Advance position by pitch rate
 *   5. Deactivate voice when it reaches the end (or loop)
 */
static void mix_voice(AudioVoice *voice, float *output, int num_frames,
                      float vol_scale) {
  if (!voice->active || !voice->clip || !voice->clip->samples)
    return;

  AudioClip *clip = voice->clip;
  float pos = voice->position;
  float pitch = voice->pitch;
  float vol = voice->volume * vol_scale;

  /* Pan: linear equal-amplitude */
  float pan = voice->pan;
  float left_vol  = vol * (1.0f - pan) * 0.5f;
  float right_vol = vol * (1.0f + pan) * 0.5f;
  if (left_vol < 0.0f) left_vol = 0.0f;
  if (right_vol < 0.0f) right_vol = 0.0f;

  for (int i = 0; i < num_frames; i++) {
    int idx = (int)pos;
    float frac = pos - (float)idx;

    if (idx >= clip->sample_count - 1) {
      if (voice->loop) {
        pos = 0.0f;
        idx = 0;
        frac = 0.0f;
      } else {
        voice->active = 0;
        break;
      }
    }

    /* Linear interpolation between samples */
    int next_idx = idx + 1;
    if (next_idx >= clip->sample_count) next_idx = 0;

    float l0 = clip->samples[idx * 2];
    float r0 = clip->samples[idx * 2 + 1];
    float l1 = clip->samples[next_idx * 2];
    float r1 = clip->samples[next_idx * 2 + 1];

    float left  = l0 + (l1 - l0) * frac;
    float right = r0 + (r1 - r0) * frac;

    /* Mix into output (additive) */
    output[i * 2]     += left  * left_vol;
    output[i * 2 + 1] += right * right_vol;

    pos += pitch;
  }

  voice->position = pos;
}

void loaded_audio_mix(LoadedAudioState *state, AudioOutputBuffer *buf,
                      int num_frames) {
  /* Clear buffer */
  memset(buf->samples, 0, (size_t)num_frames * AUDIO_BYTES_PER_FRAME);

  float master = state->master_volume;

  /* Mix SFX voices */
  for (int i = 0; i < MAX_AUDIO_VOICES; i++) {
    mix_voice(&state->voices[i], buf->samples, num_frames,
              master * state->sfx_volume);
  }

  /* Mix music voice */
  mix_voice(&state->music_voice, buf->samples, num_frames,
            master * state->music_volume);

  /* Clamp output to [-1, +1] */
  for (int i = 0; i < num_frames * 2; i++) {
    float s = buf->samples[i];
    if (s > 1.0f) buf->samples[i] = 1.0f;
    else if (s < -1.0f) buf->samples[i] = -1.0f;
  }
}
