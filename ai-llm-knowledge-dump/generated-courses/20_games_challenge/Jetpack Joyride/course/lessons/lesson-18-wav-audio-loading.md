# Lesson 18 -- WAV Audio Loading

## Observable Outcome
Jump, Fall, POP, and Dagger sounds play when the corresponding game events occur. Pressing SPACE triggers the jump sound. Colliding with an obstacle triggers either the fall sound (death) or the POP sound (shield absorb). Dagger obstacles emit a dagger whoosh. All audio is real-time mixed into the platform audio callback.

## New Concepts (max 3)
1. Hand-rolled RIFF/WAV parser: chunk-walking the binary format to find `fmt ` and `data` sections
2. int16 to float32 PCM conversion with mono-to-stereo duplication
3. Voice pool with steal-oldest eviction policy for concurrent sound effects

## Prerequisites
- Lesson 09 (AudioOutputBuffer, interleaved stereo layout, audio callback)
- Lesson 10 (ToneGenerator voice pool concept, SoundDef, steal-oldest)
- Working game with player physics and obstacle collisions (Lessons 01-17)

## Background

Until now, our audio system synthesized sounds procedurally using ToneGenerator oscillators. Real games use recorded audio files. The WAV format is the simplest uncompressed audio container -- a RIFF header followed by chunks of metadata and raw PCM samples. By writing our own parser instead of using a library, we understand exactly what those bytes mean and avoid any dependency beyond `<stdio.h>`.

A WAV file begins with a 12-byte RIFF header: the ASCII string "RIFF", a 4-byte little-endian file size, and the ASCII string "WAVE". After that come chunks, each starting with a 4-byte ID and a 4-byte size. We need two chunks: `fmt ` (note the trailing space) which describes the audio format, and `data` which contains the raw samples. Our parser walks through the file byte-by-byte looking for these two chunks, ignoring any others (like `LIST` metadata chunks that some editors insert).

The `fmt ` chunk tells us: audio_format (must be 1 for PCM -- we reject compressed formats), number of channels, sample rate, and bits per sample. We require 16-bit samples because that is what virtually all game WAV files use. The `data` chunk contains the raw int16 samples which we convert to float32 for our mixer. This conversion is straightforward: divide each int16 by 32768.0f to map the range [-32768, 32767] into approximately [-1.0, 1.0].

Our loaded-audio system stores everything as interleaved stereo float32, regardless of the source format. If a WAV file is mono, we duplicate each sample into both left and right channels during loading. This uniform internal format simplifies the mixer -- it always reads pairs of floats without checking channel count at mix time.

## Code Walkthrough

### Step 1: Little-endian byte readers (loaded-audio.c)

WAV files are always little-endian, but our CPU might not be. These two helper functions read multi-byte values safely from a byte buffer:

```c
static uint32_t read_u32_le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
```

By constructing values from individual bytes with explicit shifts, we avoid endianness bugs. This works correctly on both little-endian (x86) and big-endian (some ARM) architectures.

### Step 2: AudioClip and AudioVoice structs (loaded-audio.h)

An AudioClip holds decoded audio data ready for playback:

```c
typedef struct {
  float *samples;      /* Interleaved stereo float32 PCM data */
  int sample_count;    /* Total number of frames (L+R pairs) */
  int sample_rate;     /* Original sample rate (e.g. 44100) */
  int channels;        /* Always 2 after loading (stereo) */
} AudioClip;
```

An AudioVoice represents one active playback instance of a clip:

```c
typedef struct {
  AudioClip *clip;     /* Which clip is playing (NULL = inactive) */
  float position;      /* Current playback position (fractional frames) */
  float volume;        /* 0.0 - 1.0 */
  float pitch;         /* 1.0 = normal speed */
  float pan;           /* -1.0 = left, 0.0 = center, 1.0 = right */
  int active;          /* 1 = playing */
  int loop;            /* 1 = loop forever */
  int age;             /* For steal-oldest policy */
} AudioVoice;

#define MAX_AUDIO_VOICES 16
#define MAX_AUDIO_CLIPS 32
```

The `position` field is a float, not an int. This is crucial for pitch variation in the next lesson -- a fractional position lets us advance by non-integer amounts per output sample.

### Step 3: WAV loading (loaded-audio.c)

The `audioclip_load_wav` function reads the entire file into memory, validates the RIFF/WAVE header, then walks chunks to find `fmt ` and `data`:

```c
int audioclip_load_wav(const char *path, AudioClip *out) {
  memset(out, 0, sizeof(*out));

  FILE *f = fopen(path, "rb");
  /* ... read entire file into uint8_t *data ... */

  /* Validate RIFF/WAVE header */
  if (file_size < 44 ||
      memcmp(data, "RIFF", 4) != 0 ||
      memcmp(data + 8, "WAVE", 4) != 0) {
    /* Not a valid WAV file */
  }

  /* Walk chunks starting at offset 12 */
  size_t pos = 12;
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
```

The even-boundary padding (`chunk_size & 1`) is a RIFF requirement -- chunks with odd sizes are padded with a zero byte so the next chunk starts on an even offset.

### Step 4: fmt chunk parsing and format validation

```c
  uint16_t audio_format = read_u16_le(fmt_chunk);
  uint16_t channels = read_u16_le(fmt_chunk + 2);
  uint32_t sample_rate = read_u32_le(fmt_chunk + 4);
  uint16_t bits_per_sample = read_u16_le(fmt_chunk + 14);

  if (audio_format != 1) { /* 1 = PCM */
    fprintf(stderr, "not PCM format (%d)\n", audio_format);
    return -1;
  }
  if (bits_per_sample != 16) {
    fprintf(stderr, "not 16-bit (%d-bit)\n", bits_per_sample);
    return -1;
  }
```

We reject anything that is not PCM 16-bit. This keeps the parser simple and covers nearly all game sound effects.

### Step 5: int16 to float32 conversion with mono-to-stereo

```c
  int total_samples = (int)(data_size / (bits_per_sample / 8));
  int frame_count = total_samples / (int)channels;

  out->samples = (float *)malloc((size_t)frame_count * 2 * sizeof(float));

  int16_t *src = (int16_t *)data_chunk;
  for (int i = 0; i < frame_count; i++) {
    float left, right;
    if (channels == 1) {
      left = right = (float)src[i] / 32768.0f;
    } else {
      left  = (float)src[i * 2]     / 32768.0f;
      right = (float)src[i * 2 + 1] / 32768.0f;
    }
    out->samples[i * 2]     = left;
    out->samples[i * 2 + 1] = right;
  }

  out->sample_count = frame_count;
  out->sample_rate = (int)sample_rate;
  out->channels = 2; /* Always stored as stereo */
```

Dividing by 32768.0f (not 32767.0f) is intentional: int16 ranges from -32768 to +32767, so dividing by 32768 maps -32768 to exactly -1.0 and +32767 to slightly less than +1.0. This asymmetry is standard practice in audio engines.

### Step 6: Voice pool and steal-oldest policy

When playing a sound, we first search for a free voice. If none exists, we steal the voice with the lowest `age` counter:

```c
void loaded_audio_play(LoadedAudioState *state, int clip_idx,
                       float volume, float pitch, float pan) {
  AudioVoice *voice = NULL;
  for (int i = 0; i < MAX_AUDIO_VOICES; i++) {
    if (!state->voices[i].active) {
      voice = &state->voices[i];
      break;
    }
  }

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
```

The `age` counter is monotonically increasing. The oldest voice (lowest age) is the one playing the longest, and therefore the one whose interruption is least noticeable.

### Step 7: LoadedAudioState initialization

```c
void loaded_audio_init(LoadedAudioState *state) {
  memset(state, 0, sizeof(*state));
  state->master_volume = 0.6f;
  state->sfx_volume = 1.0f;
  state->music_volume = 0.5f;
}
```

The master volume starts at 0.6 to prevent clipping when multiple sounds play simultaneously. Music is quieter (0.5) so it sits underneath the sound effects.

## Common Mistakes

**Opening WAV files in text mode instead of binary mode.** Always use `"rb"` with `fopen`. On some platforms, text mode translates newline characters, which corrupts binary data. A WAV file that happens to contain byte 0x0A in its sample data would get silently mangled.

**Forgetting the RIFF chunk padding rule.** Chunks with odd sizes are padded to the next even byte. Without the `if (chunk_size & 1) pos++` check, the parser skips to the wrong offset and fails to find subsequent chunks. This manifests as "missing fmt/data chunks" errors on files that work fine in other players.

**Using 32767.0f instead of 32768.0f for the int16 conversion divisor.** While both work audibly, dividing by 32767 means the most negative int16 value (-32768) maps to a value slightly below -1.0. This can cause subtle clipping in downstream processing that expects the [-1.0, +1.0] range. The convention is 32768.0f.

**Allocating the output buffer with `channels` instead of hardcoded 2.** The output is always stereo regardless of input channel count. Allocating `frame_count * channels * sizeof(float)` when `channels == 1` gives you half the memory you need, causing a buffer overrun when writing stereo pairs.
