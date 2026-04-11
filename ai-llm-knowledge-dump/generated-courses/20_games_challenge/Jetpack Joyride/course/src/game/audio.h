#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/audio.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Game-layer audio: clip IDs, init, and play functions.
 * Wraps the loaded-audio system with game-specific clip management.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../utils/loaded-audio.h"

/* ── Clip IDs ────────────────────────────────────────────────────────── */
typedef enum {
  CLIP_JUMP,
  CLIP_FALL,
  CLIP_POP,
  CLIP_DAGGER,
  CLIP_MISSILE_MOVE,
  CLIP_MISSILE_BOOM,
  CLIP_WARNING,
  CLIP_PELLET,
  CLIP_SHIELD,
  CLIP_SIGIL_CHARGE,
  CLIP_SIGIL_FIRE,
  CLIP_SIGIL_POWERDOWN,
  CLIP_NEW_HIGH_SCORE,
  CLIP_BG_MUSIC,
  CLIP_MENU_MUSIC,
  CLIP_COUNT
} ClipID;

/* Game audio state — wraps LoadedAudioState with clip index mapping. */
typedef struct {
  LoadedAudioState loaded;
  int clip_indices[CLIP_COUNT]; /* Map ClipID → loaded clip index */
  int initialized;
} GameAudio;

/* Initialize audio state (no clips loaded yet). */
void game_audio_init_state(GameAudio *audio);

/* Load all game audio clips at once (blocks). Returns 0 on success. */
int game_audio_init(GameAudio *audio);

/* Load one clip by index (0 to CLIP_COUNT-1). Returns 1 if all done. */
int game_audio_load_one_clip(GameAudio *audio, int clip_index);

/* Free all audio resources. */
void game_audio_free(GameAudio *audio);

/* Play a sound effect with default settings. */
void game_audio_play(GameAudio *audio, ClipID id);

/* Play a sound effect with volume and pitch variation. */
void game_audio_play_ex(GameAudio *audio, ClipID id,
                        float volume, float pitch);

/* Start background music. */
void game_audio_play_music(GameAudio *audio, ClipID id, float volume);

/* Stop music. */
void game_audio_stop_music(GameAudio *audio);

/* Mix audio into output buffer (called from audio callback). */
void game_audio_mix(GameAudio *audio, AudioOutputBuffer *buf, int num_frames);

#endif /* GAME_AUDIO_H */
