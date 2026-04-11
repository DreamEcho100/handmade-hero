/* ═══════════════════════════════════════════════════════════════════════════
 * game/audio.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Loads all 15 audio clips (13 WAV + 2 OGG) and provides game-layer
 * play functions with pitch variation matching the original.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "./audio.h"
#include "../utils/rng.h"

#include <stdio.h>
#include <string.h>

/* RNG for pitch variation — isolated from gameplay RNG */
static RNG s_audio_rng;

/* Audio file paths */
static const char *CLIP_PATHS[CLIP_COUNT] = {
  [CLIP_JUMP]           = "assets/audio/Jump.wav",
  [CLIP_FALL]           = "assets/audio/Fall.wav",
  [CLIP_POP]            = "assets/audio/POP.wav",
  [CLIP_DAGGER]         = "assets/audio/Dagger.wav",
  [CLIP_MISSILE_MOVE]   = "assets/audio/MissileMove.wav",
  [CLIP_MISSILE_BOOM]   = "assets/audio/MissileBOOM.wav",
  [CLIP_WARNING]        = "assets/audio/Warning.wav",
  [CLIP_PELLET]         = "assets/audio/Pellet.wav",
  [CLIP_SHIELD]         = "assets/audio/Shield.wav",
  [CLIP_SIGIL_CHARGE]   = "assets/audio/SigilCharge.wav",
  [CLIP_SIGIL_FIRE]     = "assets/audio/SigilFire.wav",
  [CLIP_SIGIL_POWERDOWN]= "assets/audio/SigilPowerDown.wav",
  [CLIP_NEW_HIGH_SCORE] = "assets/audio/NewHighScore.wav",
  [CLIP_BG_MUSIC]       = "assets/audio/BackgroundMusic.ogg",
  [CLIP_MENU_MUSIC]     = "assets/audio/MenuMusic.ogg",
};

/* Initialize audio state without loading any clips. */
void game_audio_init_state(GameAudio *audio) {
  memset(audio, 0, sizeof(*audio));
  loaded_audio_init(&audio->loaded);
  rng_seed(&s_audio_rng, 9876);
  for (int i = 0; i < CLIP_COUNT; i++) {
    audio->clip_indices[i] = -1;
  }
}

/* Load ONE clip by index. Returns 1 when all clips are loaded. */
int game_audio_load_one_clip(GameAudio *audio, int clip_index) {
  if (clip_index < 0 || clip_index >= CLIP_COUNT) return 1;

  const char *path = CLIP_PATHS[clip_index];
  int idx;

  if (clip_index == CLIP_BG_MUSIC || clip_index == CLIP_MENU_MUSIC) {
    idx = loaded_audio_load_ogg(&audio->loaded, path);
  } else {
    idx = loaded_audio_load_wav(&audio->loaded, path);
  }

  if (idx >= 0) {
    audio->clip_indices[clip_index] = idx;
  } else {
    fprintf(stderr, "WARNING: Failed to load audio '%s'\n", path);
  }

  /* Check if this was the last clip */
  if (clip_index >= CLIP_COUNT - 1) {
    int loaded = 0;
    for (int i = 0; i < CLIP_COUNT; i++) {
      if (audio->clip_indices[i] >= 0) loaded++;
    }
    printf("Game audio: loaded %d/%d clips\n", loaded, CLIP_COUNT);
    audio->initialized = (loaded > 0) ? 1 : 0;
    return 1; /* All done */
  }
  return 0; /* More clips to load */
}

/* Load all clips at once (blocking). Used for simplicity where needed. */
int game_audio_init(GameAudio *audio) {
  game_audio_init_state(audio);
  for (int i = 0; i < CLIP_COUNT; i++) {
    game_audio_load_one_clip(audio, i);
  }
  return 0;
}

void game_audio_free(GameAudio *audio) {
  if (!audio || audio->loaded.clip_count <= 0) return;
#ifdef DEBUG
  printf("game_audio_free: freeing %d clips\n", audio->loaded.clip_count);
#endif
  for (int i = 0; i < audio->loaded.clip_count; i++) {
    audioclip_free(&audio->loaded.clips[i]);
  }
  audio->loaded.clip_count = 0;
  audio->initialized = 0;
}

void game_audio_play(GameAudio *audio, ClipID id) {
  game_audio_play_ex(audio, id, 1.0f, 1.0f);
}

void game_audio_play_ex(GameAudio *audio, ClipID id,
                        float volume, float pitch) {
  if (!audio->initialized) return;
  if (id < 0 || id >= CLIP_COUNT) return;

  int idx = audio->clip_indices[id];
  if (idx < 0) return;

  /* Apply game-specific pitch variation */
  float final_pitch = pitch;
  switch (id) {
  case CLIP_JUMP:
    /* Original: randi_range(9, 11) / 10 → 0.9-1.1 */
    final_pitch = rng_float_range(&s_audio_rng, 0.9f, 1.1f);
    break;
  case CLIP_PELLET:
    /* Original: pitch 1.0-1.2 */
    final_pitch = rng_float_range(&s_audio_rng, 1.0f, 1.2f);
    break;
  case CLIP_POP:
    /* Shield break: pitch 0.4-0.6 */
    final_pitch = rng_float_range(&s_audio_rng, 0.4f, 0.6f);
    break;
  case CLIP_MISSILE_MOVE:
    final_pitch = 0.35f;
    break;
  case CLIP_MISSILE_BOOM:
    final_pitch = 0.3f;
    break;
  default:
    break;
  }

  /* Volume adjustments matching original */
  float final_volume = volume;
  switch (id) {
  case CLIP_FALL:
    final_volume = 0.1f; /* -20dB equivalent */
    break;
  case CLIP_DAGGER:
    final_volume = 0.1f;
    break;
  case CLIP_WARNING:
    final_volume = 0.3f;
    break;
  case CLIP_SIGIL_CHARGE:
    final_volume = 0.3f;
    break;
  default:
    break;
  }

  loaded_audio_play(&audio->loaded, idx, final_volume, final_pitch, 0.0f);
}

void game_audio_play_music(GameAudio *audio, ClipID id, float volume) {
  if (!audio->initialized) return;
  int idx = audio->clip_indices[id];
  if (idx < 0) return;
  loaded_audio_play_music(&audio->loaded, idx, volume);
}

void game_audio_stop_music(GameAudio *audio) {
  loaded_audio_stop_music(&audio->loaded);
}

void game_audio_mix(GameAudio *audio, AudioOutputBuffer *buf, int num_frames) {
  if (!audio->initialized) {
    memset(buf->samples, 0, (size_t)num_frames * AUDIO_BYTES_PER_FRAME);
    return;
  }
  loaded_audio_mix(&audio->loaded, buf, num_frames);
}
