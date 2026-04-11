# Lesson 19 -- OGG Music and Pitch Variation

## Observable Outcome
Background music loops continuously. Jump sounds have subtle pitch variation between 0.9x and 1.1x on each press. Pellet collect sounds are slightly higher-pitched (1.0-1.2x). Shield break (POP) sounds are deep and boomy (0.4-0.6x pitch). Missile sounds play at slow pitches (0.3-0.35x). The game feels alive and varied despite using the same audio clips repeatedly.

## New Concepts (max 3)
1. OGG Vorbis decoding with stb_vorbis (stb_vorbis_decode_filename) and the same int16-to-float32 pipeline
2. Dedicated music voice separate from the SFX pool, with looping support
3. Pitch variation via fractional position advancement and linear interpolation between adjacent samples

## Prerequisites
- Lesson 18 (WAV loading, AudioClip, AudioVoice, voice pool, loaded_audio_mix)
- Understanding of interleaved stereo float32 format

## Background

Compressed audio is essential for music tracks. A 3-minute stereo WAV at 44.1kHz occupies roughly 30 MB, while the same track as OGG Vorbis compresses to 2-4 MB. We use stb_vorbis -- a single-file public domain Vorbis decoder -- to decompress OGG files at load time into the same float32 stereo format our mixer already handles. The decoding happens once during loading; at runtime we stream from the decoded buffer, not from the compressed data.

The music voice is architecturally separate from the SFX pool. It has its own AudioVoice that is never stolen by sound effects. This prevents the common bug where a burst of overlapping SFX (e.g., multiple dagger hits in rapid succession) steals the music voice and cuts the background track. The music voice also sets `loop = 1`, which causes the mixer to reset the playback position to zero when it reaches the end of the clip instead of deactivating the voice.

Pitch variation is the single cheapest technique for making a game sound rich instead of robotic. Playing the same jump sound at exactly 1.0x pitch every time creates a mechanical, repetitive feel. Randomizing the pitch between 0.9x and 1.1x makes each jump sound subtly different. The implementation is elegant: instead of resampling the audio data, we simply advance the playback position by `pitch` samples per output sample. At pitch 1.0, we advance by 1.0 -- normal speed. At pitch 0.5, we advance by 0.5 -- half speed, one octave down. At pitch 2.0, we advance by 2.0 -- double speed, one octave up. Linear interpolation between adjacent samples prevents aliasing artifacts.

## Code Walkthrough

### Step 1: OGG Vorbis loading (loaded-audio.c)

stb_vorbis provides a single-call decode function that returns int16 samples. We convert them with the same pipeline as WAV:

```c
int audioclip_load_ogg(const char *path, AudioClip *out) {
  memset(out, 0, sizeof(*out));

  int channels, sample_rate;
  short *decoded;
  int frame_count = stb_vorbis_decode_filename(path, &channels,
                                                &sample_rate, &decoded);
  if (frame_count <= 0) {
    fprintf(stderr, "audioclip_load_ogg: failed to decode '%s'\n", path);
    return -1;
  }

  out->samples = (float *)malloc((size_t)frame_count * 2 * sizeof(float));

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

  free(decoded); /* stb_vorbis allocates with malloc */
  return 0;
}
```

The `stb_vorbis_decode_filename` function allocates the decoded buffer internally with `malloc`. We must `free(decoded)` after copying to our float32 buffer. The function returns the frame count (number of L+R pairs), with channels and sample_rate populated via output pointers.

### Step 2: Including stb_vorbis

stb_vorbis is compiled directly into loaded-audio.c:

```c
#define STB_VORBIS_NO_PUSHDATA_API
#include "../../vendor/stb_vorbis.c"
```

The `STB_VORBIS_NO_PUSHDATA_API` define disables the streaming push API we do not need, reducing code size. Note: Raylib also bundles stb_vorbis internally. The Raylib build uses `--allow-multiple-definition` (`-Wl,-z,muldefs`) to resolve duplicate symbols since both copies are identical.

### Step 3: Dedicated music voice (loaded-audio.c)

The music voice lives outside the SFX voice array and always loops:

```c
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
```

The LoadedAudioState struct keeps the music voice separate:

```c
typedef struct {
  AudioClip clips[MAX_AUDIO_CLIPS];
  int clip_count;
  AudioVoice voices[MAX_AUDIO_VOICES]; /* SFX pool */
  int next_age;
  AudioVoice music_voice;              /* Never stolen */
  float master_volume;
  float sfx_volume;
  float music_volume;
} LoadedAudioState;
```

### Step 4: The mixer with pitch-based interpolation (loaded-audio.c)

The `mix_voice` function is the heart of the audio system. It advances the fractional position by the voice's pitch rate and linearly interpolates between adjacent samples:

```c
static void mix_voice(AudioVoice *voice, float *output, int num_frames,
                      float vol_scale) {
  if (!voice->active || !voice->clip || !voice->clip->samples)
    return;

  AudioClip *clip = voice->clip;
  float pos = voice->position;
  float pitch = voice->pitch;
  float vol = voice->volume * vol_scale;

  float pan = voice->pan;
  float left_vol  = vol * (1.0f - pan) * 0.5f;
  float right_vol = vol * (1.0f + pan) * 0.5f;

  for (int i = 0; i < num_frames; i++) {
    int idx = (int)pos;
    float frac = pos - (float)idx;

    if (idx >= clip->sample_count - 1) {
      if (voice->loop) {
        pos = 0.0f; idx = 0; frac = 0.0f;
      } else {
        voice->active = 0;
        break;
      }
    }

    int next_idx = idx + 1;
    if (next_idx >= clip->sample_count) next_idx = 0;

    /* Linear interpolation */
    float left  = clip->samples[idx * 2]     +
                  (clip->samples[next_idx * 2]     - clip->samples[idx * 2]) * frac;
    float right = clip->samples[idx * 2 + 1] +
                  (clip->samples[next_idx * 2 + 1] - clip->samples[idx * 2 + 1]) * frac;

    output[i * 2]     += left  * left_vol;
    output[i * 2 + 1] += right * right_vol;

    pos += pitch; /* Advance by pitch samples per output sample */
  }

  voice->position = pos;
}
```

When `pitch = 1.0`, `pos` advances by 1.0 each output sample -- normal playback. When `pitch = 0.5`, `pos` advances by 0.5 -- the sound plays at half speed, one octave lower. The fractional part (`frac`) drives the linear interpolation: `sample[idx] + (sample[idx+1] - sample[idx]) * frac`. Without this interpolation, pitch-shifted audio would have audible stair-step artifacts.

### Step 5: Game-specific pitch and volume (game/audio.c)

The game layer applies per-clip pitch variation using a dedicated RNG:

```c
void game_audio_play_ex(GameAudio *audio, ClipID id,
                        float volume, float pitch) {
  float final_pitch = pitch;
  switch (id) {
  case CLIP_JUMP:
    final_pitch = rng_float_range(&s_audio_rng, 0.9f, 1.1f);
    break;
  case CLIP_PELLET:
    final_pitch = rng_float_range(&s_audio_rng, 1.0f, 1.2f);
    break;
  case CLIP_POP:
    final_pitch = rng_float_range(&s_audio_rng, 0.4f, 0.6f);
    break;
  case CLIP_MISSILE_MOVE:
    final_pitch = 0.35f;
    break;
  case CLIP_MISSILE_BOOM:
    final_pitch = 0.3f;
    break;
  default: break;
  }

  float final_volume = volume;
  switch (id) {
  case CLIP_FALL:    final_volume = 0.1f; break;
  case CLIP_DAGGER:  final_volume = 0.1f; break;
  case CLIP_WARNING: final_volume = 0.3f; break;
  case CLIP_SIGIL_CHARGE: final_volume = 0.3f; break;
  default: break;
  }

  loaded_audio_play(&audio->loaded, idx, final_volume, final_pitch, 0.0f);
}
```

The audio RNG (`s_audio_rng`) is seeded separately from the gameplay RNG. This isolation ensures that audio pitch variation does not alter the deterministic obstacle spawn sequence.

### Step 6: The main mixer (loaded-audio.c)

The top-level mix function clears the buffer, mixes all SFX voices, mixes the music voice, and clamps:

```c
void loaded_audio_mix(LoadedAudioState *state, AudioOutputBuffer *buf,
                      int num_frames) {
  memset(buf->samples, 0, (size_t)num_frames * AUDIO_BYTES_PER_FRAME);

  float master = state->master_volume;

  for (int i = 0; i < MAX_AUDIO_VOICES; i++) {
    mix_voice(&state->voices[i], buf->samples, num_frames,
              master * state->sfx_volume);
  }

  mix_voice(&state->music_voice, buf->samples, num_frames,
            master * state->music_volume);

  /* Clamp output to [-1, +1] */
  for (int i = 0; i < num_frames * 2; i++) {
    float s = buf->samples[i];
    if (s > 1.0f)  buf->samples[i] = 1.0f;
    if (s < -1.0f) buf->samples[i] = -1.0f;
  }
}
```

The output clamp prevents values outside [-1.0, +1.0] from causing distortion in the platform's audio driver. This is a hard limiter -- not ideal for production audio, but perfectly adequate for a game with a limited number of simultaneous voices.

## Common Mistakes

**Forgetting to free the stb_vorbis decoded buffer.** `stb_vorbis_decode_filename` allocates with `malloc`. If you only free your own float32 buffer and not the decoded int16 buffer, you leak the entire decompressed OGG every time you load a clip.

**Using the gameplay RNG for pitch variation.** If audio pitch calls consume random numbers from the same RNG as obstacle spawning, the game becomes non-deterministic -- playing more or fewer sounds changes which obstacles appear. Use a separate RNG instance seeded with a fixed value.

**Setting pitch to 0.0.** A pitch of zero means the playback position never advances. The voice plays the same sample forever, producing a DC offset that sounds like silence but wastes a voice slot permanently. Always clamp pitch to a minimum of 0.1 or similar.

**Mixing music at the same volume as SFX.** Music is continuous and fills the frequency spectrum. SFX are short bursts. If both play at full volume, the combined output clips constantly. The code sets music_volume to 0.5f and plays background music at 0.3f volume to keep it underneath the effects.
