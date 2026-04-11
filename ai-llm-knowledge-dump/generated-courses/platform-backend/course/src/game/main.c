#include "main.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../utils/draw-shapes.h"
#include "../utils/draw-text.h"
#include "../utils/perf.h"
#include "../utils/render.h"
#include "audio_demo.h"

#ifndef COURSE_FORCE_SCENE_INDEX
#define COURSE_FORCE_SCENE_INDEX (-1)
#endif

#ifndef COURSE_LOCK_SCENE
#define COURSE_LOCK_SCENE 0
#endif

#define GAME_POOL_CAPACITY 20
#define GAME_LEVEL_CARD_COUNT 12
#define GAME_LEVEL_FORMATION_COUNT 3
#define GAME_LEVEL_ENTITY_COUNT 18
#define GAME_POOL_BARRIER_COUNT 3
#define GAME_SLAB_MAX_EVENTS 24
#define GAME_SLAB_SLOTS_PER_PAGE 6
#define GAME_SLAB_TRANSPORT_COUNT 4
#define GAME_ARENA_CHECKPOINT_CAPACITY 3
#define GAME_ARENA_LOG_CAPACITY 6

#define GAME_EXERCISE_STEP_1 (1u << 0)
#define GAME_EXERCISE_STEP_2 (1u << 1)
#define GAME_EXERCISE_STEP_3 (1u << 2)
#define GAME_EXERCISE_STEP_4 (1u << 3)

typedef struct {
  unsigned int progress_mask;
  unsigned long long completed_frame;
} GameSceneExerciseState;

typedef struct {
  const char *title;
  const char *summary;
  float x;
  float y;
  float w;
  float h;
  float r;
  float g;
  float b;
  float a;
} ArenaLabBar;

typedef struct {
  const char *label;
  size_t used_bytes;
  float fill;
  float r;
  float g;
  float b;
} ArenaCheckpointViz;

typedef struct {
  GameCamera camera;
  ArenaLabBar *bars;
  int bar_count;
  unsigned int build_serial;
  ArenaCheckpointViz checkpoints[GAME_ARENA_CHECKPOINT_CAPACITY];
  int checkpoint_count;
  unsigned int phase_index;
  unsigned int cycle_count;
  unsigned int rewind_count;
  unsigned int manual_steps;
  int active_depth;
  float phase_timer;
  float beat;
  size_t simulated_temp_bytes;
  size_t peak_temp_bytes;
  const char *phase_name;
  const char *phase_summary;
} ArenaLabScene;

typedef struct {
  float x;
  float y;
  float w;
  float h;
  float r;
  float g;
  float b;
  int entity_start;
  int entity_count;
  char label[24];
} LevelFormation;

typedef struct {
  float x;
  float y;
  float w;
  float h;
  float heat;
  float r;
  float g;
  float b;
  int formation_index;
  unsigned int serial;
} LevelEntity;

typedef struct {
  GameCamera camera;
  LevelFormation *formations;
  int formation_count;
  LevelEntity *entities;
  int entity_count;
  unsigned int build_serial;
  unsigned int reload_seed;
  float pulse;
  float rebuild_flash;
  float scan_t;
  int highlighted_entity;
} LevelReloadScene;

typedef struct {
  float x;
  float y;
  float vx;
  float vy;
  float ttl;
  float size;
  float r;
  float g;
  float b;
  int slot_index;
  unsigned int generation;
} PoolProbe;

typedef struct {
  float x;
  float y;
  float w;
  float h;
  float energy;
} PoolBarrier;

typedef struct {
  GameCamera camera;
  PoolAllocator pool;
  PoolProbe *active[GAME_POOL_CAPACITY];
  int active_count;
  PoolProbe *last_freed;
  PoolBarrier barriers[GAME_POOL_BARRIER_COUNT];
  unsigned int slot_generations[GAME_POOL_CAPACITY];
  unsigned int slot_reuses[GAME_POOL_CAPACITY];
  unsigned int reuse_hits;
  unsigned int collision_count;
  unsigned int escaped_count;
  unsigned int burst_count;
  int hottest_slot;
  float spawn_accum;
} PoolLabScene;

typedef struct {
  float wx;
  float wy;
  float ttl;
  float size;
  float r;
  float g;
  float b;
  int lane;
  int page_index;
  int state_index;
  unsigned int sequence_id;
  char label[32];
} SlabEvent;

typedef struct {
  GameCamera camera;
  SlabAllocator slab;
  SlabEvent *events[GAME_SLAB_MAX_EVENTS];
  int event_count;
  float event_accum;
  float state_timer;
  unsigned int generated_count;
  unsigned int state_index;
  unsigned int phase_changes;
  size_t prev_page_count;
  unsigned int page_growth_count;
  unsigned int burst_count;
} SlabAudioScene;

typedef struct {
  char text[128];
} GameHudLogEntry;

typedef struct {
  GameSceneID scene_id;
  const char *scene_name;
  union {
    ArenaLabScene arena_lab;
    LevelReloadScene level_reload;
    PoolLabScene pool_lab;
    SlabAudioScene slab_audio_lab;
  } as;
} GameLevelState;

typedef struct {
  GameSceneID current;
  GameSceneID requested;
  GameSceneID previous;
  GameSceneID compile_time_scene;
  GameSceneID runtime_forced_scene;
  unsigned long long frame_entered;
  int compile_time_override_active;
  int compile_time_locked;
  int runtime_override_active;
  int runtime_override_locked;
} GameSceneMachine;

struct GameAppState {
  GameSceneMachine scene;
  GameAudioState audio;
  GameLevelState *level_state;
  GameSceneID rebuild_failure_scene;
  GameSceneExerciseState exercise[GAME_SCENE_COUNT];
  int hud_visible;
  int rebuild_failure_active;
  unsigned long long frame_index;
  unsigned long long rebuild_failure_frame;
  unsigned int scene_enter_count[GAME_SCENE_COUNT];
  unsigned int scene_reload_count[GAME_SCENE_COUNT];
  unsigned int transition_count;
  unsigned int rebuild_failure_count;
  GameHudLogEntry logs[GAME_ARENA_LOG_CAPACITY];
  int log_head;
  int log_count;
  float audio_warning_heat[GAME_SCENE_COUNT];
  float audio_warning_peak[GAME_SCENE_COUNT];
};

static void game_draw_world_label(Backbuffer *backbuffer,
                                  const RenderContext *ctx, float wx, float wy,
                                  const char *text, float r, float g, float b,
                                  float a);
static void game_log_eventf(GameAppState *game, const char *fmt, ...);
static const char *slab_audio_state_name(unsigned int state_index);

static const char *game_scene_name(GameSceneID scene_id) {
  switch (scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return "Arena Lifetimes Lab";
  case GAME_SCENE_LEVEL_RELOAD:
    return "Level Reload Lab";
  case GAME_SCENE_POOL_LAB:
    return "Pool Reuse Lab";
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return "Slab + Audio State Lab";
  case GAME_SCENE_COUNT:
  default:
    return "Unknown";
  }
}

static const char *game_scene_summary(GameSceneID scene_id) {
  switch (scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return "perm vs level vs scratch vs TempMemory";
  case GAME_SCENE_LEVEL_RELOAD:
    return "scene-local data gets rebuilt on reload";
  case GAME_SCENE_POOL_LAB:
    return "fixed-size reuse with a free-list pool";
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return "slab pages plus audio-state transitions";
  case GAME_SCENE_COUNT:
  default:
    return "n/a";
  }
}

static const char *game_scene_action_hint(GameSceneID scene_id) {
  switch (scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return "SPACE advance checkpoint stack and stack cue pressure";
  case GAME_SCENE_LEVEL_RELOAD:
    return "SPACE spike the cache scan and audio warning line";
  case GAME_SCENE_POOL_LAB:
    return "SPACE fire a collision wave and saturate the voice pool";
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return "SPACE inject a transport burst and crowd the sequencer";
  case GAME_SCENE_COUNT:
  default:
    return "SPACE scene action";
  }
}

static const char *game_scale_mode_name(WindowScaleMode mode) {
  switch (mode) {
  case WINDOW_SCALE_MODE_FIXED:
    return "FIXED";
  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    return "DYNAMIC_MATCH";
  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    return "DYNAMIC_ASPECT";
  default:
    return "UNKNOWN";
  }
}

static int game_active_voice_count(const GameAudioState *audio) {
  int active = 0;
  if (!audio)
    return 0;
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (audio->voices[i].active)
      active++;
  }
  return active;
}

static int game_free_voice_count(const GameAudioState *audio) {
  if (!audio)
    return 0;
  return MAX_SOUNDS - game_active_voice_count(audio);
}

static int game_oldest_active_voice_age(const GameAudioState *audio) {
  int oldest_age = -1;

  if (!audio)
    return -1;

  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (!audio->voices[i].active)
      continue;
    if (oldest_age < 0 || audio->voices[i].age < oldest_age)
      oldest_age = audio->voices[i].age;
  }

  return oldest_age;
}

static int game_hottest_voice_slot(const GameAudioState *audio) {
  int oldest_age = -1;
  int slot = -1;

  if (!audio)
    return -1;

  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (!audio->voices[i].active)
      continue;
    if (oldest_age < 0 || audio->voices[i].age < oldest_age) {
      oldest_age = audio->voices[i].age;
      slot = i;
    }
  }

  return slot;
}

static float game_audio_layer_level(const GameAudioState *audio) {
  if (!audio)
    return 0.0f;
  return audio->master_volume * (audio->music_volume + audio->sfx_volume +
                                 audio->sequencer.tone.current_volume);
}

static void game_play_audio_probe_sequence(GameAudioState *audio,
                                           const SoundID *ids, int count) {
  if (!audio || !ids || count <= 0)
    return;
  for (int i = 0; i < count; ++i)
    game_play_sound_at(audio, ids[i]);
}

static void game_fire_scene_warning_probe(GameAppState *game,
                                          GameSceneID scene_id) {
  static const SoundID arena_ids[] = {SOUND_TONE_MID, SOUND_TONE_HIGH,
                                      SOUND_TONE_LOW, SOUND_TONE_HIGH};
  static const SoundID level_ids[] = {SOUND_TONE_MID, SOUND_TONE_HIGH,
                                      SOUND_TONE_MID, SOUND_TONE_HIGH,
                                      SOUND_TONE_LOW};
  static const SoundID pool_ids[] = {SOUND_TONE_HIGH, SOUND_TONE_MID,
                                     SOUND_TONE_HIGH, SOUND_TONE_LOW,
                                     SOUND_TONE_HIGH, SOUND_TONE_MID};
  static const SoundID slab_ids[] = {
      SOUND_TONE_MID, SOUND_TONE_HIGH, SOUND_TONE_LOW, SOUND_TONE_HIGH,
      SOUND_TONE_MID, SOUND_TONE_HIGH, SOUND_TONE_LOW};

  if (!game)
    return;

  switch (scene_id) {
  case GAME_SCENE_ARENA_LAB:
    game_play_audio_probe_sequence(
        &game->audio, arena_ids,
        (int)(sizeof(arena_ids) / sizeof(arena_ids[0])));
    game_log_eventf(game, "audio probe -> nested checkpoint cues stacked");
    break;
  case GAME_SCENE_LEVEL_RELOAD:
    game_play_audio_probe_sequence(
        &game->audio, level_ids,
        (int)(sizeof(level_ids) / sizeof(level_ids[0])));
    game_log_eventf(game, "audio probe -> scan spike layered over rebuild");
    break;
  case GAME_SCENE_POOL_LAB:
    game_play_audio_probe_sequence(
        &game->audio, pool_ids, (int)(sizeof(pool_ids) / sizeof(pool_ids[0])));
    game_log_eventf(game, "audio probe -> collision burst stressed voices");
    break;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    game_play_audio_probe_sequence(
        &game->audio, slab_ids, (int)(sizeof(slab_ids) / sizeof(slab_ids[0])));
    game_log_eventf(
        game, "audio probe -> transport burst crowded sequencer headroom");
    break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }
}

static int game_scene_audio_anomaly_level(const GameAppState *game) {
  const GameAudioState *audio;
  int active_voices;
  int free_voices;
  float layer_level;

  if (!game || !game->level_state)
    return 0;

  audio = &game->audio;
  active_voices = game_active_voice_count(audio);
  free_voices = game_free_voice_count(audio);
  layer_level = game_audio_layer_level(audio);

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    const ArenaLabScene *scene = &game->level_state->as.arena_lab;
    if (free_voices == 0 || (scene->active_depth >= 3 && active_voices >= 6))
      return 2;
    if ((scene->active_depth >= 2 && active_voices >= 4) ||
        layer_level >= 1.05f)
      return 1;
  } break;
  case GAME_SCENE_LEVEL_RELOAD: {
    const LevelReloadScene *scene = &game->level_state->as.level_reload;
    if (free_voices == 0 ||
        (scene->rebuild_flash >= 0.65f && active_voices >= 6))
      return 2;
    if ((scene->rebuild_flash >= 0.30f && active_voices >= 4) ||
        layer_level >= 1.00f)
      return 1;
  } break;
  case GAME_SCENE_POOL_LAB: {
    const PoolLabScene *scene = &game->level_state->as.pool_lab;
    if (free_voices == 0 || (scene->burst_count > 0 && active_voices >= 7))
      return 2;
    if (free_voices <= 2 ||
        (scene->collision_count > 0 && active_voices >= 5) ||
        layer_level >= 1.18f)
      return 1;
  } break;
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    const SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    if (free_voices == 0 || (scene->burst_count > 0 &&
                             scene->event_count >= 10 && active_voices >= 7))
      return 2;
    if (free_voices <= 2 || scene->page_growth_count > 0 ||
        (scene->event_count > GAME_SLAB_SLOTS_PER_PAGE && layer_level >= 1.10f))
      return 1;
  } break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }

  if (free_voices == 0)
    return 2;
  if (free_voices <= 1 || active_voices >= MAX_SOUNDS - 1 ||
      layer_level >= 1.45f)
    return 1;
  return 0;
}

static float game_scene_audio_pressure_score(const GameAppState *game) {
  const GameAudioState *audio;
  float score = 0.0f;
  float layer_level;
  int active_voices;
  int free_voices;

  if (!game || !game->level_state)
    return 0.0f;

  audio = &game->audio;
  active_voices = game_active_voice_count(audio);
  free_voices = game_free_voice_count(audio);
  layer_level = game_audio_layer_level(audio);

  score += (float)active_voices / (float)MAX_SOUNDS * 0.45f;
  score += (1.0f - (float)free_voices / (float)MAX_SOUNDS) * 0.20f;

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    const ArenaLabScene *scene = &game->level_state->as.arena_lab;
    score += 0.12f * (float)scene->active_depth;
    score +=
        scene->phase_index >= 4u && scene->phase_index <= 6u ? 0.10f : 0.0f;
  } break;
  case GAME_SCENE_LEVEL_RELOAD: {
    const LevelReloadScene *scene = &game->level_state->as.level_reload;
    score += scene->rebuild_flash * 0.28f;
    score += scene->scan_t > 0.35f ? 0.12f : 0.0f;
  } break;
  case GAME_SCENE_POOL_LAB: {
    const PoolLabScene *scene = &game->level_state->as.pool_lab;
    score += scene->burst_count > 0 ? 0.18f : 0.0f;
    score += scene->collision_count > 0 ? 0.14f : 0.0f;
  } break;
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    const SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    score += (float)scene->event_count / (float)GAME_SLAB_MAX_EVENTS * 0.24f;
    score += scene->page_growth_count > 0 ? 0.16f : 0.0f;
    score += scene->burst_count > 0 ? 0.18f : 0.0f;
  } break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }

  if (layer_level > 0.90f)
    score += (layer_level - 0.90f) * 0.35f;
  if (score < 0.0f)
    score = 0.0f;
  if (score > 1.0f)
    score = 1.0f;
  return score;
}

static void game_update_audio_warning_decay(GameAppState *game, float dt) {
  float pressure;
  int scene_id;

  if (!game || !game->level_state)
    return;

  scene_id = (int)game->scene.current;
  if (scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return;

  pressure = game_scene_audio_pressure_score(game);
  game->audio_warning_heat[scene_id] -= dt * 0.22f;
  if (game->audio_warning_heat[scene_id] < 0.0f)
    game->audio_warning_heat[scene_id] = 0.0f;
  if (pressure > game->audio_warning_heat[scene_id])
    game->audio_warning_heat[scene_id] = pressure;
  if (game->audio_warning_heat[scene_id] > game->audio_warning_peak[scene_id])
    game->audio_warning_peak[scene_id] = game->audio_warning_heat[scene_id];
}

static const char *game_audio_warning_cause_label(const GameAppState *game) {
  if (!game || !game->level_state)
    return "unknown cause";

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    const ArenaLabScene *scene = &game->level_state->as.arena_lab;
    return scene->active_depth >= 2 ? "nested checkpoint cues"
                                    : "checkpoint cue stack";
  }
  case GAME_SCENE_LEVEL_RELOAD: {
    const LevelReloadScene *scene = &game->level_state->as.level_reload;
    return scene->rebuild_flash > 0.25f ? "rebuild bed + scan spike"
                                        : "scan accent over rebuild";
  }
  case GAME_SCENE_POOL_LAB:
    return "collision churn + low free voices";
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return "transport burst + sequencer layering";
  case GAME_SCENE_COUNT:
  default:
    return "mixed runtime pressure";
  }
}

static void game_describe_audio_warning_context(const GameAppState *game,
                                                char *buf, size_t buf_size) {
  int scene_id;
  float heat;
  int anomaly_level;

  if (!buf || buf_size == 0u)
    return;

  buf[0] = '\0';
  if (!game)
    return;

  scene_id = (int)game->scene.current;
  if (scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return;

  heat = game->audio_warning_heat[scene_id];
  anomaly_level = game_scene_audio_anomaly_level(game);

  if (anomaly_level >= 2) {
    snprintf(buf, buf_size, "cause:%s | severity:red live pressure",
             game_audio_warning_cause_label(game));
  } else if (anomaly_level == 1) {
    snprintf(buf, buf_size, "cause:%s | severity:yellow accumulating risk",
             game_audio_warning_cause_label(game));
  } else if (heat >= 0.45f) {
    snprintf(buf, buf_size, "cause:%s | severity:cooling after burst",
             game_audio_warning_cause_label(game));
  } else {
    snprintf(buf, buf_size, "cause:%s | severity:green readable",
             game_audio_warning_cause_label(game));
  }
}

static int game_audio_warning_recovered(const GameAppState *game) {
  int scene_id;

  if (!game)
    return 0;

  scene_id = (int)game->scene.current;
  if (scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return 0;

  return game->audio_warning_peak[scene_id] >= 0.45f &&
         game->audio_warning_heat[scene_id] <= 0.20f;
}

static void game_describe_audio_warning_meter(const GameAppState *game,
                                              char *buf, size_t buf_size) {
  int scene_id;
  float heat;
  float peak;

  if (!buf || buf_size == 0u)
    return;

  buf[0] = '\0';
  if (!game)
    return;

  scene_id = (int)game->scene.current;
  if (scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return;

  heat = game->audio_warning_heat[scene_id];
  peak = game->audio_warning_peak[scene_id];
  if (heat >= 0.85f) {
    snprintf(buf, buf_size, "pressure memory: red linger %.2f peak %.2f", heat,
             peak);
  } else if (heat >= 0.45f) {
    snprintf(buf, buf_size, "pressure memory: yellow cooling %.2f peak %.2f",
             heat, peak);
  } else {
    snprintf(buf, buf_size, "pressure memory: clear %.2f peak %.2f", heat,
             peak);
  }
}

static void game_draw_audio_warning_meter(const GameAppState *game,
                                          Backbuffer *backbuffer,
                                          const RenderContext *hud, float wx,
                                          float wy) {
  float heat;
  int scene_id;

  if (!game || !backbuffer || !hud)
    return;

  scene_id = (int)game->scene.current;
  if (scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return;

  heat = game->audio_warning_heat[scene_id];
  draw_rect(backbuffer, world_rect_px_x(hud, wx, 5.74f),
            world_rect_px_y(hud, wy, 0.16f), world_w(hud, 5.74f),
            world_h(hud, 0.16f), 0.16f, 0.19f, 0.24f, 1.0f);

  if (heat <= 0.0f)
    return;

  if (heat >= 0.85f) {
    draw_rect(backbuffer, world_rect_px_x(hud, wx, 5.74f * heat),
              world_rect_px_y(hud, wy, 0.16f), world_w(hud, 5.74f * heat),
              world_h(hud, 0.16f), COLOR_RED);
  } else if (heat >= 0.45f) {
    draw_rect(backbuffer, world_rect_px_x(hud, wx, 5.74f * heat),
              world_rect_px_y(hud, wy, 0.16f), world_w(hud, 5.74f * heat),
              world_h(hud, 0.16f), COLOR_YELLOW);
  } else {
    draw_rect(backbuffer, world_rect_px_x(hud, wx, 5.74f * heat),
              world_rect_px_y(hud, wy, 0.16f), world_w(hud, 5.74f * heat),
              world_h(hud, 0.16f), COLOR_GREEN);
  }
}

static void game_describe_audio_evidence(const GameAppState *game, char *buf,
                                         size_t buf_size) {
  const GameAudioState *audio;
  int active_voices;
  int free_voices;
  int hottest_slot;
  int oldest_age;

  if (!buf || buf_size == 0u) {
    return;
  }

  buf[0] = '\0';
  if (!game) {
    return;
  }

  audio = &game->audio;
  active_voices = game_active_voice_count(audio);
  free_voices = game_free_voice_count(audio);
  hottest_slot = game_hottest_voice_slot(audio);
  oldest_age = game_oldest_active_voice_age(audio);

  snprintf(buf, buf_size,
           "Audio live free:%d active:%d hottest:%d oldest:%d step_left:%d "
           "tone:%.2f freq:%.1f master:%.2f",
           free_voices, active_voices, hottest_slot,
           oldest_age >= 0 ? oldest_age : 0,
           audio->sequencer.step_samples_remaining,
           audio->sequencer.tone.current_volume,
           audio->sequencer.tone.frequency, audio->master_volume);
}

static void game_describe_audio_anomaly(const GameAppState *game, char *buf,
                                        size_t buf_size) {
  const GameAudioState *audio;
  int active_voices;
  int free_voices;
  float layer_level;
  int anomaly_level;

  if (!buf || buf_size == 0u)
    return;

  buf[0] = '\0';
  if (!game)
    return;

  audio = &game->audio;
  active_voices = game_active_voice_count(audio);
  free_voices = game_free_voice_count(audio);
  layer_level = game_audio_layer_level(audio);
  anomaly_level = game_scene_audio_anomaly_level(game);

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    const ArenaLabScene *scene = &game->level_state->as.arena_lab;
    if (anomaly_level >= 2) {
      snprintf(buf, buf_size,
               "warning: nested checkpoint cues saturated the pool at depth %d",
               scene->active_depth);
    } else if (anomaly_level == 1) {
      snprintf(buf, buf_size,
               "watch: nested temp cues are stacking with music (voices:%d "
               "gain:%.2f)",
               active_voices, layer_level);
    } else {
      snprintf(buf, buf_size,
               "status: checkpoint cues stay separated while scratch rewinds "
               "cleanly");
    }
  } break;
  case GAME_SCENE_LEVEL_RELOAD: {
    const LevelReloadScene *scene = &game->level_state->as.level_reload;
    if (anomaly_level >= 2) {
      snprintf(
          buf, buf_size,
          "warning: rebuild flash + scan spike saturated voices (flash:%.2f)",
          scene->rebuild_flash);
    } else if (anomaly_level == 1) {
      snprintf(
          buf, buf_size,
          "watch: scan spike is crowding the rebuild bed (voices:%d gain:%.2f)",
          active_voices, layer_level);
    } else {
      snprintf(buf, buf_size,
               "status: rebuild flash and scan motion stay readable as "
               "separate states");
    }
  } break;
  case GAME_SCENE_POOL_LAB: {
    const PoolLabScene *scene = &game->level_state->as.pool_lab;
    if (anomaly_level >= 2) {
      snprintf(
          buf, buf_size,
          "warning: collision burst maxed the voice pool (free:%d reuse:%u)",
          free_voices, scene->reuse_hits);
    } else if (anomaly_level == 1) {
      snprintf(buf, buf_size,
               "watch: churn is heating the pool; next hit may steal (free:%d)",
               free_voices);
    } else {
      snprintf(buf, buf_size,
               "status: collision churn is audible but the pool still has "
               "spare slots");
    }
  } break;
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    const SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    if (anomaly_level >= 2) {
      snprintf(buf, buf_size,
               "warning: transport burst crowded sequencer headroom during %s",
               slab_audio_state_name(scene->state_index));
    } else if (anomaly_level == 1) {
      snprintf(buf, buf_size,
               "watch: queue pressure is layering tightly with the sequencer "
               "(pages:%zu)",
               scene->slab.page_count);
    } else {
      snprintf(buf, buf_size,
               "status: transport pressure is visible without overwhelming the "
               "sequencer");
    }
  } break;
  case GAME_SCENE_COUNT:
  default:
    if (anomaly_level >= 2) {
      snprintf(buf, buf_size,
               "warning: voice cap reached, steals likely (active:%d/%d)",
               active_voices, MAX_SOUNDS);
    } else if (anomaly_level == 1) {
      snprintf(buf, buf_size,
               "watch: crowded mix, layered gain %.2f is nearing headroom",
               layer_level);
    } else {
      snprintf(
          buf, buf_size,
          "status: headroom stable, allocator and mix pressure look healthy");
    }
    break;
  }
}

static void game_draw_audio_anomaly_label(const GameAppState *game,
                                          Backbuffer *backbuffer,
                                          const RenderContext *hud, float wx,
                                          float wy, const char *text) {
  const GameAudioState *audio;
  int active_voices;
  int free_voices;
  float layer_level;

  if (!game || !backbuffer || !hud || !text)
    return;

  audio = &game->audio;
  active_voices = game_active_voice_count(audio);
  free_voices = game_free_voice_count(audio);
  layer_level = game_audio_layer_level(audio);

  if (game_scene_audio_anomaly_level(game) >= 2) {
    game_draw_world_label(backbuffer, hud, wx, wy, text, COLOR_RED);
  } else if (game_scene_audio_anomaly_level(game) == 1 || free_voices <= 1 ||
             layer_level >= 1.45f || active_voices >= MAX_SOUNDS - 1) {
    game_draw_world_label(backbuffer, hud, wx, wy, text, COLOR_YELLOW);
  } else {
    game_draw_world_label(backbuffer, hud, wx, wy, text, COLOR_GREEN);
  }
}

static void game_log_eventf(GameAppState *game, const char *fmt, ...);

static int game_count_bits(unsigned int mask) {
  int count = 0;
  while (mask) {
    count += (int)(mask & 1u);
    mask >>= 1u;
  }
  return count;
}

static int game_scene_exercise_focus_step(unsigned int progress_mask,
                                          unsigned int required_mask) {
  if ((required_mask & GAME_EXERCISE_STEP_1) != 0u &&
      (progress_mask & GAME_EXERCISE_STEP_1) == 0u)
    return 0;
  if ((required_mask & GAME_EXERCISE_STEP_2) != 0u &&
      (progress_mask & GAME_EXERCISE_STEP_2) == 0u)
    return 1;
  if ((required_mask & GAME_EXERCISE_STEP_3) != 0u &&
      (progress_mask & GAME_EXERCISE_STEP_3) == 0u)
    return 2;
  return 3;
}

static unsigned int game_scene_exercise_required_mask(GameSceneID scene_id) {
  switch (scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return GAME_EXERCISE_STEP_1 | GAME_EXERCISE_STEP_2 | GAME_EXERCISE_STEP_3 |
           GAME_EXERCISE_STEP_4;
  case GAME_SCENE_LEVEL_RELOAD:
    return GAME_EXERCISE_STEP_1 | GAME_EXERCISE_STEP_2 | GAME_EXERCISE_STEP_3 |
           GAME_EXERCISE_STEP_4;
  case GAME_SCENE_POOL_LAB:
    return GAME_EXERCISE_STEP_1 | GAME_EXERCISE_STEP_2 | GAME_EXERCISE_STEP_3 |
           GAME_EXERCISE_STEP_4;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return GAME_EXERCISE_STEP_1 | GAME_EXERCISE_STEP_2 | GAME_EXERCISE_STEP_3 |
           GAME_EXERCISE_STEP_4;
  case GAME_SCENE_COUNT:
  default:
    return 0u;
  }
}

static const char *game_scene_exercise_status_name(const GameAppState *game,
                                                   GameSceneID scene_id) {
  const GameSceneExerciseState *exercise;
  unsigned int required_mask;

  if (!game || scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return "n/a";

  exercise = &game->exercise[scene_id];
  required_mask = game_scene_exercise_required_mask(scene_id);
  if (required_mask == 0u)
    return "n/a";
  if ((exercise->progress_mask & required_mask) == required_mask)
    return "complete";
  if (exercise->progress_mask != 0u)
    return "in progress";
  return "ready";
}

static void game_mark_scene_exercise(GameAppState *game, GameSceneID scene_id,
                                     unsigned int step_mask,
                                     const char *step_name) {
  GameSceneExerciseState *exercise;
  unsigned int required_mask;

  if (!game || !step_mask || scene_id < 0 || scene_id >= GAME_SCENE_COUNT)
    return;

  exercise = &game->exercise[scene_id];
  if ((exercise->progress_mask & step_mask) == step_mask)
    return;

  exercise->progress_mask |= step_mask;
  if (step_name) {
    game_log_eventf(game, "exercise %s -> %s", game_scene_name(scene_id),
                    step_name);
  }

  required_mask = game_scene_exercise_required_mask(scene_id);
  if (required_mask != 0u &&
      (exercise->progress_mask & required_mask) == required_mask &&
      exercise->completed_frame == 0u) {
    exercise->completed_frame = game->frame_index;
    game_log_eventf(game, "exercise complete -> %s", game_scene_name(scene_id));
    game_play_sound_at(&game->audio, SOUND_TONE_HIGH);
  }
}

static void game_update_scene_exercise_progress(GameAppState *game) {
  if (!game || !game->level_state)
    return;

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    if (scene->active_depth >= 2) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_2,
                               "nested temp scopes visible");
    }
    if (scene->rewind_count > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_3,
                               "rewind observed");
    }
    if (scene->rewind_count > 0 && scene->active_depth == 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_4,
                               "scratch returned to baseline");
    }
  } break;
  case GAME_SCENE_LEVEL_RELOAD: {
    LevelReloadScene *scene = &game->level_state->as.level_reload;
    if (scene->build_serial > 1u && scene->rebuild_flash > 0.0f) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_2,
                               "rebuild flash observed");
    }
    if ((game->exercise[game->scene.current].progress_mask &
         GAME_EXERCISE_STEP_1) != 0u &&
        scene->scan_t > 0.35f) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_3,
                               "contiguous strip scanned");
    }
  } break;
  case GAME_SCENE_POOL_LAB: {
    PoolLabScene *scene = &game->level_state->as.pool_lab;
    if (scene->active_count > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_1,
                               "fixed pool slot allocated");
    }
    if (scene->collision_count > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_2,
                               "collision churn observed");
    }
    if (scene->last_freed) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_3,
                               "slot returned to free list");
    }
    if (scene->reuse_hits > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_4,
                               "returned slot reused");
    }
  } break;
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    if (scene->event_count > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_1,
                               "same-size slab events recorded");
    }
    if (scene->burst_count > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_2,
                               "queue pressure burst injected");
    }
    if (scene->page_growth_count > 0) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_3,
                               "slab page growth observed");
    }
    if (scene->phase_changes >= 3u && scene->state_index == 3u) {
      game_mark_scene_exercise(game, game->scene.current, GAME_EXERCISE_STEP_4,
                               "flush state reached after growth");
    }
  } break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }
}

static void game_log_eventf(GameAppState *game, const char *fmt, ...) {
  va_list args;
  GameHudLogEntry *entry;

  if (!game || !fmt)
    return;

  entry = &game->logs[game->log_head];
  va_start(args, fmt);
  vsnprintf(entry->text, sizeof(entry->text), fmt, args);
  va_end(args);

  game->log_head = (game->log_head + 1) % GAME_ARENA_LOG_CAPACITY;
  if (game->log_count < GAME_ARENA_LOG_CAPACITY)
    game->log_count++;
}

static GameSceneID game_wrap_scene(int value) {
  while (value < 0)
    value += GAME_SCENE_COUNT;
  return (GameSceneID)(value % GAME_SCENE_COUNT);
}

static GameCamera game_default_camera(void) {
  GameCamera camera;
  camera.x = 0.0f;
  camera.y = 0.0f;
  camera.zoom = 1.0f;
  return camera;
}

static GameCamera *game_level_camera(GameLevelState *level_state) {
  if (!level_state)
    return NULL;
  switch (level_state->scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return &level_state->as.arena_lab.camera;
  case GAME_SCENE_LEVEL_RELOAD:
    return &level_state->as.level_reload.camera;
  case GAME_SCENE_POOL_LAB:
    return &level_state->as.pool_lab.camera;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return &level_state->as.slab_audio_lab.camera;
  case GAME_SCENE_COUNT:
  default:
    return NULL;
  }
}

static void game_apply_camera_input(GameCamera *camera, const GameInput *input,
                                    float dt) {
  float cam_dx;
  float cam_dy;

  if (!camera || !input)
    return;

  if (button_just_pressed(input->buttons.cam_reset))
    *camera = game_default_camera();

  cam_dx = (input->buttons.cam_right.ended_down ? 3.0f : 0.0f) +
           (input->buttons.cam_left.ended_down ? -3.0f : 0.0f);
  cam_dy = (input->buttons.cam_up.ended_down ? 3.0f : 0.0f) +
           (input->buttons.cam_down.ended_down ? -3.0f : 0.0f);
  camera_pan(&camera->x, &camera->y, cam_dx, cam_dy, 1.0f, dt);
  if (input->buttons.zoom_in.ended_down)
    camera->zoom *= powf(1.05f, dt * 60.0f);
  if (input->buttons.zoom_out.ended_down)
    camera->zoom *= powf(0.95f, dt * 60.0f);
  if (camera->zoom < 0.1f)
    camera->zoom = 0.1f;
  if (camera->zoom > 20.0f)
    camera->zoom = 20.0f;
}

static const char *slab_audio_state_name(unsigned int state_index) {
  switch (state_index % GAME_SLAB_TRANSPORT_COUNT) {
  case 0:
    return "record";
  case 1:
    return "queue";
  case 2:
    return "playback";
  case 3:
  default:
    return "flush";
  }
}

static int slab_audio_page_index_for_ptr(const SlabAllocator *slab,
                                         const void *ptr) {
  const SlabPage *page;
  int page_index = 0;

  if (!slab || !ptr)
    return 0;

  for (page = slab->pages; page; page = page->next, ++page_index) {
    if (slab_page_contains(slab, page, ptr))
      return page_index;
  }
  return 0;
}

static int pool_lab_slot_index(const PoolLabScene *scene,
                               const PoolProbe *probe) {
  ptrdiff_t offset;
  if (!scene || !probe || !scene->pool.memory || scene->pool.slot_size == 0)
    return -1;
  offset = (const uint8_t *)probe - (const uint8_t *)scene->pool.memory;
  if (offset < 0)
    return -1;
  return (int)((size_t)offset / scene->pool.slot_size);
}

static int pool_lab_slot_is_active(const PoolLabScene *scene, int slot_index) {
  if (!scene || slot_index < 0)
    return 0;
  for (int i = 0; i < scene->active_count; ++i) {
    if (scene->active[i] && scene->active[i]->slot_index == slot_index)
      return 1;
  }
  return 0;
}

static void arena_lab_set_checkpoint(ArenaLabScene *scene, int index,
                                     const char *label, size_t used_bytes,
                                     float fill, float r, float g, float b) {
  if (!scene || index < 0 || index >= GAME_ARENA_CHECKPOINT_CAPACITY)
    return;
  scene->checkpoints[index].label = label;
  scene->checkpoints[index].used_bytes = used_bytes;
  scene->checkpoints[index].fill = fill;
  scene->checkpoints[index].r = r;
  scene->checkpoints[index].g = g;
  scene->checkpoints[index].b = b;
}

static void arena_lab_apply_phase(ArenaLabScene *scene) {
  if (!scene)
    return;

  scene->checkpoint_count = 0;
  scene->active_depth = 0;
  scene->simulated_temp_bytes = 0;

  switch (scene->phase_index % 8u) {
  case 0:
    scene->phase_name = "baseline";
    scene->phase_summary =
        "level owns the persistent scene; temp scopes are idle";
    break;
  case 1:
    scene->phase_name = "begin temp A";
    scene->phase_summary =
        "scratch opens the first checkpoint for transient planning";
    scene->checkpoint_count = 1;
    scene->active_depth = 1;
    scene->simulated_temp_bytes = 96;
    arena_lab_set_checkpoint(scene, 0, "temp A", 96, 0.28f, 0.94f, 0.77f,
                             0.24f);
    break;
  case 2:
    scene->phase_name = "nest temp B";
    scene->phase_summary = "a deeper checkpoint isolates riskier work";
    scene->checkpoint_count = 2;
    scene->active_depth = 2;
    scene->simulated_temp_bytes = 208;
    arena_lab_set_checkpoint(scene, 0, "temp A", 96, 0.28f, 0.94f, 0.77f,
                             0.24f);
    arena_lab_set_checkpoint(scene, 1, "temp B", 208, 0.58f, 0.98f, 0.56f,
                             0.26f);
    break;
  case 3:
    scene->phase_name = "fan out work";
    scene->phase_summary = "nested temps hold speculative packets and labels";
    scene->checkpoint_count = 3;
    scene->active_depth = 3;
    scene->simulated_temp_bytes = 352;
    arena_lab_set_checkpoint(scene, 0, "temp A", 96, 0.28f, 0.94f, 0.77f,
                             0.24f);
    arena_lab_set_checkpoint(scene, 1, "temp B", 208, 0.58f, 0.98f, 0.56f,
                             0.26f);
    arena_lab_set_checkpoint(scene, 2, "temp C", 352, 0.88f, 0.46f, 0.78f,
                             0.96f);
    break;
  case 4:
    scene->phase_name = "rewind inner";
    scene->phase_summary = "temp C disappears instantly; temp A and B remain";
    scene->checkpoint_count = 2;
    scene->active_depth = 2;
    scene->simulated_temp_bytes = 208;
    arena_lab_set_checkpoint(scene, 0, "temp A", 96, 0.28f, 0.94f, 0.77f,
                             0.24f);
    arena_lab_set_checkpoint(scene, 1, "temp B", 208, 0.58f, 0.98f, 0.56f,
                             0.26f);
    break;
  case 5:
    scene->phase_name = "refill branch";
    scene->phase_summary = "after rewind, the same depth can be reused cheaply";
    scene->checkpoint_count = 2;
    scene->active_depth = 2;
    scene->simulated_temp_bytes = 248;
    arena_lab_set_checkpoint(scene, 0, "temp A", 96, 0.28f, 0.94f, 0.77f,
                             0.24f);
    arena_lab_set_checkpoint(scene, 1, "temp B2", 248, 0.70f, 0.35f, 0.98f,
                             0.94f);
    break;
  case 6:
    scene->phase_name = "rewind outer";
    scene->phase_summary = "ending temp A returns scratch to the baseline mark";
    break;
  case 7:
    scene->phase_name = "handoff";
    scene->phase_summary = "perm and level survive; scratch work is gone";
    break;
  }

  if (scene->simulated_temp_bytes > scene->peak_temp_bytes)
    scene->peak_temp_bytes = scene->simulated_temp_bytes;
}

static void game_advance_arena_lab(ArenaLabScene *scene, GameAppState *game,
                                   GameAudioState *audio, int manual_step) {
  unsigned int next_phase;

  if (!scene)
    return;

  next_phase = (scene->phase_index + 1u) % 8u;
  if (next_phase == 0)
    scene->cycle_count++;
  scene->phase_index = next_phase;
  if (manual_step)
    scene->manual_steps++;
  if (scene->phase_index == 4u || scene->phase_index == 6u)
    scene->rewind_count++;

  arena_lab_apply_phase(scene);
  if (game) {
    game_log_eventf(game, "arena phase %u: %s", scene->phase_index,
                    scene->phase_name);
  }

  if (audio) {
    if (scene->phase_index == 4u || scene->phase_index == 6u)
      game_play_sound_at(audio, SOUND_TONE_LOW);
    else if (scene->active_depth >= 3)
      game_play_sound_at(audio, SOUND_TONE_HIGH);
    else
      game_play_sound_at(audio, SOUND_TONE_MID);
  }
}

static void game_fill_arena_lab(GameLevelState *level_state, Arena *level,
                                unsigned int reload_count) {
  ArenaLabScene *scene = &level_state->as.arena_lab;
  scene->camera = game_default_camera();
  scene->bar_count = 4;
  scene->bars = ARENA_PUSH_ZERO_ARRAY(level, scene->bar_count, ArenaLabBar);
  scene->build_serial = reload_count;
  if (!scene->bars)
    return;
  scene->bars[0] = (ArenaLabBar){"perm", "session owner", 1.0f,   4.2f,   3.0f,
                                 2.0f,   0.839f,          0.286f, 0.231f, 1.0f};
  scene->bars[1] =
      (ArenaLabBar){"level", "scene rebuilds", 4.5f,   4.2f,   3.0f,
                    2.0f,    0.122f,           0.565f, 0.565f, 1.0f};
  scene->bars[2] = (ArenaLabBar){"scratch", "frame-local work",
                                 8.0f,      4.2f,
                                 3.0f,      2.0f,
                                 0.208f,    0.616f,
                                 0.259f,    1.0f};
  scene->bars[3] = (ArenaLabBar){"temp", "nested checkpoint",
                                 11.5f,  4.2f,
                                 3.0f,   2.0f,
                                 0.447f, 0.349f,
                                 0.792f, 1.0f};
  scene->phase_index = 0;
  scene->cycle_count = 0;
  scene->rewind_count = 0;
  scene->manual_steps = 0;
  scene->phase_timer = 0.0f;
  scene->beat = 0.0f;
  scene->peak_temp_bytes = 0;
  arena_lab_apply_phase(scene);
}

static void game_fill_level_reload(GameLevelState *level_state, Arena *level,
                                   unsigned int reload_count) {
  LevelReloadScene *scene = &level_state->as.level_reload;
  scene->camera = game_default_camera();
  scene->formation_count = GAME_LEVEL_FORMATION_COUNT;
  scene->entity_count = GAME_LEVEL_ENTITY_COUNT;
  scene->formations =
      ARENA_PUSH_ZERO_ARRAY(level, scene->formation_count, LevelFormation);
  scene->entities =
      ARENA_PUSH_ZERO_ARRAY(level, scene->entity_count, LevelEntity);
  scene->build_serial = reload_count;
  scene->reload_seed = reload_count * 17u + 3u;
  scene->pulse = 0.0f;
  scene->rebuild_flash = 1.0f;
  scene->scan_t = 0.0f;
  scene->highlighted_entity = 0;
  if (!scene->formations || !scene->entities)
    return;

  for (int formation_index = 0; formation_index < scene->formation_count;
       ++formation_index) {
    LevelFormation *formation = &scene->formations[formation_index];
    float hue = fmodf((float)scene->reload_seed * 0.01f +
                          (float)formation_index * 0.19f,
                      1.0f);
    formation->x = 0.9f + (float)formation_index * 4.85f;
    formation->y = 1.9f + (float)(formation_index % 2) * 0.45f;
    formation->w = 4.05f;
    formation->h = 4.55f;
    formation->r = 0.20f + 0.50f * hue;
    formation->g = 0.28f + 0.42f * (1.0f - hue);
    formation->b = 0.34f + 0.30f * fabsf(0.5f - hue);
    formation->entity_start = formation_index * 6;
    formation->entity_count = 6;
    snprintf(formation->label, sizeof(formation->label), "cluster_%d",
             formation_index + 1);
  }

  for (int entity_index = 0; entity_index < scene->entity_count;
       ++entity_index) {
    int formation_index = entity_index / 6;
    int local_index = entity_index % 6;
    int col = local_index % 2;
    int row = local_index / 2;
    LevelFormation *formation = &scene->formations[formation_index];
    LevelEntity *entity = &scene->entities[entity_index];
    float jitter =
        (float)((scene->reload_seed + (unsigned int)entity_index * 5u) % 7u) *
        0.03f;
    entity->x = formation->x + 0.35f + (float)col * 1.85f + jitter;
    entity->y = formation->y + 0.55f + (float)row * 1.05f;
    entity->w = 1.25f;
    entity->h = 0.52f;
    entity->heat =
        0.25f +
        0.10f * (float)((scene->reload_seed + (unsigned int)entity_index) % 5u);
    entity->r = formation->r + 0.18f;
    entity->g = formation->g + 0.10f;
    entity->b = formation->b + 0.15f;
    if (entity->r > 1.0f)
      entity->r = 1.0f;
    if (entity->g > 1.0f)
      entity->g = 1.0f;
    if (entity->b > 1.0f)
      entity->b = 1.0f;
    entity->formation_index = formation_index;
    entity->serial = (unsigned int)entity_index + 1u + reload_count * 100u;
  }

  scene->highlighted_entity =
      (int)(scene->reload_seed % (unsigned int)scene->entity_count);
}

static void pool_lab_spawn(PoolLabScene *scene, int count) {
  if (!scene)
    return;
  for (int i = 0; i < count && scene->active_count < GAME_POOL_CAPACITY; ++i) {
    PoolProbe *probe = (PoolProbe *)pool_alloc(&scene->pool);
    int slot_index;
    if (!probe)
      break;
    if (probe == scene->last_freed)
      scene->reuse_hits++;
    slot_index = pool_lab_slot_index(scene, probe);
    probe->slot_index = slot_index;
    if (slot_index >= 0 && slot_index < GAME_POOL_CAPACITY) {
      scene->slot_generations[slot_index]++;
      probe->generation = scene->slot_generations[slot_index];
      if (probe->generation > 1)
        scene->slot_reuses[slot_index]++;
      if (scene->hottest_slot < 0 ||
          scene->slot_reuses[slot_index] >
              scene->slot_reuses[scene->hottest_slot]) {
        scene->hottest_slot = slot_index;
      }
    } else {
      probe->generation = 0;
    }
    probe->x = 1.0f;
    probe->y = 1.6f + (float)((scene->active_count + i) % 6) * 1.08f;
    probe->vx = 2.8f + 0.45f * (float)((scene->active_count + i) % 5);
    probe->vy = -1.1f + 0.38f * (float)((scene->active_count + i) % 7);
    probe->ttl = 3.2f + 0.16f * (float)((scene->active_count + i) % 6);
    probe->size = 0.30f + 0.03f * (float)((scene->active_count + i) % 4);
    probe->r = 0.40f + 0.03f * (float)((slot_index + GAME_POOL_CAPACITY) % 7);
    probe->g = 0.65f + 0.02f * (float)((slot_index + GAME_POOL_CAPACITY) % 5);
    probe->b = 0.18f + 0.05f * (float)((slot_index + GAME_POOL_CAPACITY) % 8);
    scene->active[scene->active_count++] = probe;
  }
}

static void game_fill_pool_lab(GameLevelState *level_state, Arena *level) {
  PoolLabScene *scene = &level_state->as.pool_lab;
  scene->camera = game_default_camera();
  scene->active_count = 0;
  scene->last_freed = NULL;
  scene->reuse_hits = 0;
  scene->collision_count = 0;
  scene->escaped_count = 0;
  scene->burst_count = 0;
  scene->hottest_slot = -1;
  scene->spawn_accum = 0.0f;
  if (pool_init_in_arena(&scene->pool, level, sizeof(PoolProbe),
                         GAME_POOL_CAPACITY, "pool_lab_pool") != 0)
    return;
  scene->barriers[0] = (PoolBarrier){4.8f, 2.1f, 0.35f, 2.2f, 0.0f};
  scene->barriers[1] = (PoolBarrier){7.4f, 4.1f, 0.35f, 2.2f, 0.0f};
  scene->barriers[2] = (PoolBarrier){10.1f, 2.8f, 0.35f, 2.2f, 0.0f};
  pool_lab_spawn(scene, 6);
}

static SlabEvent *slab_audio_push_event(SlabAudioScene *scene,
                                        const char *label, float wy) {
  SlabEvent *event;
  if (!scene)
    return NULL;
  if (scene->event_count >= GAME_SLAB_MAX_EVENTS && scene->events[0]) {
    slab_free(&scene->slab, scene->events[0]);
    memmove(scene->events, scene->events + 1,
            (size_t)(scene->event_count - 1) * sizeof(scene->events[0]));
    scene->event_count--;
  }
  event = (SlabEvent *)slab_alloc(&scene->slab);
  if (!event)
    return NULL;
  memset(event, 0, sizeof(*event));
  event->page_index = slab_audio_page_index_for_ptr(&scene->slab, event);
  event->lane = (int)(scene->generated_count % 4u);
  event->state_index = (int)scene->state_index;
  event->sequence_id = scene->generated_count + 1u;
  event->wx = 1.25f + (float)event->page_index * 3.35f +
              0.18f * (float)(event->sequence_id % 3u);
  event->wy = wy + (float)event->lane * 0.04f;
  event->ttl = 2.5f;
  event->size = 0.9f;
  switch (scene->state_index % GAME_SLAB_TRANSPORT_COUNT) {
  case 0:
    event->r = 0.36f;
    event->g = 0.82f;
    event->b = 0.58f;
    break;
  case 1:
    event->r = 0.84f;
    event->g = 0.68f;
    event->b = 0.26f;
    break;
  case 2:
    event->r = 0.40f;
    event->g = 0.64f;
    event->b = 0.95f;
    break;
  case 3:
  default:
    event->r = 0.92f;
    event->g = 0.42f;
    event->b = 0.52f;
    break;
  }
  snprintf(event->label, sizeof(event->label), "%s %u", label,
           scene->generated_count + 1);
  scene->events[scene->event_count++] = event;
  scene->generated_count++;
  return event;
}

static void game_fill_slab_audio(GameLevelState *level_state, Arena *level) {
  SlabAudioScene *scene = &level_state->as.slab_audio_lab;
  scene->camera = game_default_camera();
  scene->event_count = 0;
  scene->event_accum = 0.0f;
  scene->state_timer = 0.0f;
  scene->generated_count = 0;
  scene->state_index = 0;
  scene->phase_changes = 0;
  scene->prev_page_count = 0;
  scene->page_growth_count = 0;
  scene->burst_count = 0;
  if (slab_init_in_arena(&scene->slab, level, sizeof(SlabEvent),
                         GAME_SLAB_SLOTS_PER_PAGE,
                         "slab_audio_lab_events") != 0)
    return;
  scene->prev_page_count = scene->slab.page_count;
  for (int i = 0; i < 8; ++i)
    slab_audio_push_event(scene, "note", 1.6f + (float)(i % 4) * 1.15f);
  scene->prev_page_count = scene->slab.page_count;
}

static void game_rebuild_scene(GameAppState *game, Arena *level,
                               int entering_new_scene) {
  GameLevelState *level_state;
  GameSceneID current;

  if (!game || !level)
    return;

  current = game->scene.current;
  game->level_state = NULL;
  arena_reset(level);
  level_state = ARENA_PUSH_ZERO_STRUCT(level, GameLevelState);
  if (!level_state) {
    game->rebuild_failure_active = 1;
    game->rebuild_failure_scene = current;
    game->rebuild_failure_frame = game->frame_index;
    game->rebuild_failure_count++;
    game_log_eventf(game, "scene rebuild failed: %s", game_scene_name(current));
    return;
  }

  game->rebuild_failure_active = 0;

  game->audio_warning_heat[current] = 0.0f;
  game->audio_warning_peak[current] = 0.0f;

  game->scene_reload_count[current]++;
  if (entering_new_scene)
    game->scene_enter_count[current]++;

  level_state->scene_id = current;
  level_state->scene_name = game_scene_name(current);
  switch (current) {
  case GAME_SCENE_ARENA_LAB:
    game_fill_arena_lab(level_state, level, game->scene_reload_count[current]);
    break;
  case GAME_SCENE_LEVEL_RELOAD:
    game_fill_level_reload(level_state, level,
                           game->scene_reload_count[current]);
    break;
  case GAME_SCENE_POOL_LAB:
    game_fill_pool_lab(level_state, level);
    break;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    game_fill_slab_audio(level_state, level);
    break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }

  game->level_state = level_state;
  game_log_eventf(game, "%s %s", entering_new_scene ? "enter" : "reload",
                  game_scene_name(current));
  game_audio_apply_scene_profile(&game->audio, (int)current);
  game_audio_trigger_scene_enter(&game->audio, (int)current);
}

static int game_scene_navigation_delta(const GameInput *input) {
  if (!input)
    return 0;
  if (button_just_pressed(input->buttons.prev_scene))
    return -1;
  if (button_just_pressed(input->buttons.next_scene))
    return 1;
  return 0;
}

static void game_handle_scene_requests(GameAppState *game,
                                       const GameInput *input) {
  int nav_delta;
  GameSceneID candidate;

  if (!game || !input)
    return;

  if (button_just_pressed(input->buttons.toggle_debug_hud))
    game->hud_visible = !game->hud_visible;

  if (button_just_pressed(input->buttons.toggle_runtime_override)) {
    if (game->scene.runtime_override_active) {
      game->scene.runtime_override_active = 0;
      game->scene.runtime_override_locked = 0;
      game_log_eventf(game, "runtime override disabled");
    } else {
      game->scene.runtime_override_active = 1;
      game->scene.runtime_override_locked = 1;
      game->scene.runtime_forced_scene = game->scene.current;
      game_log_eventf(game, "runtime override locked to %s",
                      game_scene_name(game->scene.current));
    }
  }

  if (button_just_pressed(input->buttons.clear_runtime_override)) {
    game->scene.runtime_override_active = 0;
    game->scene.runtime_override_locked = 0;
    game_log_eventf(game, "runtime override cleared");
  }

  nav_delta = game_scene_navigation_delta(input);
  if (nav_delta != 0) {
    candidate = game_wrap_scene((int)game->scene.current + nav_delta);
    if (game->scene.runtime_override_active) {
      game->scene.runtime_forced_scene = candidate;
      game->scene.requested = candidate;
    } else {
      game->scene.requested = candidate;
    }
    game_log_eventf(game, "scene request -> %s", game_scene_name(candidate));
  }

  if (game->scene.compile_time_override_active &&
      game->scene.compile_time_locked) {
    game->scene.requested = game->scene.compile_time_scene;
  }
  if (game->scene.runtime_override_active &&
      game->scene.runtime_override_locked) {
    game->scene.requested = game->scene.runtime_forced_scene;
  }
}

static void game_update_arena_lab(ArenaLabScene *scene, const GameInput *input,
                                  float dt, GameAudioState *audio,
                                  GameAppState *game) {
  if (!scene || !input)
    return;

  scene->beat += dt;
  scene->phase_timer += dt;
  if (button_just_pressed(input->buttons.play_tone)) {
    scene->phase_timer = 0.0f;
    if (game) {
      game_mark_scene_exercise(game, GAME_SCENE_ARENA_LAB, GAME_EXERCISE_STEP_1,
                               "manual checkpoint advance");
    }
    game_advance_arena_lab(scene, game, audio, 1);
    if (game && scene->active_depth >= 2)
      game_fire_scene_warning_probe(game, GAME_SCENE_ARENA_LAB);
    return;
  }
  if (scene->phase_timer >= 0.9f) {
    scene->phase_timer = 0.0f;
    game_advance_arena_lab(scene, game, audio, 0);
  }
}

static void game_update_level_reload(LevelReloadScene *scene, float dt) {
  if (!scene)
    return;
  scene->pulse += dt;
  scene->scan_t += dt * 0.95f;
  scene->rebuild_flash -= dt * 0.7f;
  if (scene->rebuild_flash < 0.0f)
    scene->rebuild_flash = 0.0f;
  if (scene->entity_count > 0) {
    int active_index =
        ((int)(scene->pulse * 4.0f) + scene->highlighted_entity) %
        scene->entity_count;
    scene->highlighted_entity = active_index;
    for (int i = 0; i < scene->entity_count; ++i) {
      float wave = 0.5f + 0.5f * sinf(scene->pulse * 2.0f + (float)i * 0.4f);
      scene->entities[i].heat = 0.15f + 0.55f * wave;
    }
  }
}

static void game_update_pool_lab(PoolLabScene *scene, const GameInput *input,
                                 float dt, GameAudioState *audio,
                                 GameAppState *game) {
  int any_collision = 0;
  if (!scene)
    return;

  scene->spawn_accum += dt;
  if (scene->spawn_accum >= 0.28f) {
    pool_lab_spawn(scene, 1);
    scene->spawn_accum = 0.0f;
  }
  if (button_just_pressed(input->buttons.play_tone)) {
    pool_lab_spawn(scene, 4);
    scene->burst_count++;
    if (game)
      game_log_eventf(game, "pool burst -> active %d", scene->active_count);
    game_play_sound_at(audio, SOUND_TONE_HIGH);
    if (game)
      game_fire_scene_warning_probe(game, GAME_SCENE_POOL_LAB);
  }

  for (int i = 0; i < scene->active_count;) {
    PoolProbe *probe = scene->active[i];
    if (probe->y < 0.9f || probe->y > 8.0f)
      probe->vy = -probe->vy;
    probe->x += probe->vx * dt;
    probe->y += probe->vy * dt;
    probe->ttl -= dt;

    for (int barrier_index = 0; barrier_index < GAME_POOL_BARRIER_COUNT;
         ++barrier_index) {
      PoolBarrier *barrier = &scene->barriers[barrier_index];
      float overlaps_x = fabsf(probe->x - barrier->x) <
                         (probe->size * 0.5f + barrier->w * 0.5f + 0.05f);
      float overlaps_y = fabsf(probe->y - barrier->y) <
                         (probe->size * 0.5f + barrier->h * 0.5f + 0.05f);
      if (overlaps_x && overlaps_y) {
        probe->vx = -fabsf(probe->vx) * 0.92f;
        probe->vy = probe->vy >= 0.0f ? probe->vy + 0.35f : probe->vy - 0.35f;
        probe->ttl -= 0.28f;
        barrier->energy = 1.0f;
        scene->collision_count++;
        any_collision = 1;
      }
    }

    if (probe->ttl <= 0.0f || probe->x < 0.4f || probe->x > 15.5f) {
      if (probe->x > 15.0f)
        scene->escaped_count++;
      scene->last_freed = probe;
      pool_free(&scene->pool, probe);
      scene->active[i] = scene->active[scene->active_count - 1];
      scene->active_count--;
      continue;
    }
    ++i;
  }

  for (int i = 0; i < GAME_POOL_BARRIER_COUNT; ++i) {
    scene->barriers[i].energy -= dt * 2.2f;
    if (scene->barriers[i].energy < 0.0f)
      scene->barriers[i].energy = 0.0f;
  }

  if (any_collision && audio)
    game_play_sound_at(audio, SOUND_TONE_MID);
}

static void game_update_slab_audio(SlabAudioScene *scene,
                                   const GameInput *input, float dt,
                                   GameAudioState *audio, GameAppState *game) {
  float spawn_interval;
  float ttl_decay;
  if (!scene)
    return;

  scene->state_timer += dt;
  if (scene->state_timer >= 2.2f) {
    scene->state_timer = 0.0f;
    scene->state_index = (scene->state_index + 1u) % GAME_SLAB_TRANSPORT_COUNT;
    scene->phase_changes++;
    if (game) {
      game_log_eventf(game, "slab transport -> %s",
                      slab_audio_state_name(scene->state_index));
    }
    if (audio)
      game_play_sound_at(audio, SOUND_TONE_HIGH);
  }

  switch (scene->state_index % GAME_SLAB_TRANSPORT_COUNT) {
  case 0:
    spawn_interval = 0.28f;
    ttl_decay = 0.95f;
    break;
  case 1:
    spawn_interval = 0.18f;
    ttl_decay = 1.05f;
    break;
  case 2:
    spawn_interval = 0.12f;
    ttl_decay = 1.15f;
    break;
  case 3:
  default:
    spawn_interval = 0.42f;
    ttl_decay = 1.80f;
    break;
  }

  scene->event_accum += dt;
  if (scene->event_accum >= spawn_interval) {
    slab_audio_push_event(scene, slab_audio_state_name(scene->state_index),
                          1.6f + (float)(scene->generated_count % 4) * 1.15f);
    scene->event_accum = 0.0f;
  }
  if (button_just_pressed(input->buttons.play_tone)) {
    slab_audio_push_event(scene, "burst", 2.2f);
    slab_audio_push_event(scene, "burst", 3.4f);
    slab_audio_push_event(scene, "burst", 4.6f);
    scene->burst_count++;
    if (game)
      game_log_eventf(game, "slab burst -> events %d", scene->event_count);
    game_play_sound_at(audio, SOUND_TONE_MID);
    if (game)
      game_fire_scene_warning_probe(game, GAME_SCENE_SLAB_AUDIO_LAB);
  }

  if (scene->slab.page_count > scene->prev_page_count) {
    scene->page_growth_count +=
        (unsigned int)(scene->slab.page_count - scene->prev_page_count);
    scene->prev_page_count = scene->slab.page_count;
    if (game)
      game_log_eventf(game, "slab grew to %zu pages", scene->slab.page_count);
  }

  for (int i = 0; i < scene->event_count;) {
    SlabEvent *event = scene->events[i];
    event->ttl -= dt * ttl_decay;
    event->wx +=
        ((scene->state_index % GAME_SLAB_TRANSPORT_COUNT) == 2 ? 0.28f
                                                               : 0.08f) *
        dt;
    if (event->ttl <= 0.0f) {
      slab_free(&scene->slab, event);
      scene->events[i] = scene->events[scene->event_count - 1];
      scene->event_count--;
      continue;
    }
    ++i;
  }
}

int game_app_init(GameAppState **out_game, Arena *perm) {
  GameAppState *game;
  GameSceneID start_scene = GAME_SCENE_ARENA_LAB;

  if (!out_game || !perm)
    return -1;

  game = ARENA_PUSH_ZERO_STRUCT(perm, GameAppState);
  if (!game)
    return -1;

  if (COURSE_FORCE_SCENE_INDEX >= 0 &&
      COURSE_FORCE_SCENE_INDEX < GAME_SCENE_COUNT) {
    start_scene = (GameSceneID)COURSE_FORCE_SCENE_INDEX;
    game->scene.compile_time_override_active = 1;
    game->scene.compile_time_locked = COURSE_LOCK_SCENE ? 1 : 0;
    game->scene.compile_time_scene = start_scene;
  }

  game->scene.current = start_scene;
  game->scene.requested = start_scene;
  game->scene.previous = start_scene;
  game->scene.runtime_forced_scene = start_scene;
  game->hud_visible = 1;
  game_audio_init_demo(&game->audio);

  *out_game = game;
  return 0;
}

void game_app_update(GameAppState *game, GameInput *input, float dt,
                     Arena *level) {
  GameCamera *camera;
  int do_reload = 0;
  int play_tone_pressed;

  if (!game || !input || !level)
    return;

  game->frame_index++;
  game_handle_scene_requests(game, input);
  do_reload = button_just_pressed(input->buttons.reload_scene);
  play_tone_pressed = button_just_pressed(input->buttons.play_tone);

  if (!game->level_state) {
    game_rebuild_scene(game, level, 1);
  } else if (game->scene.requested != game->scene.current) {
    game->scene.previous = game->scene.current;
    game->scene.current = game->scene.requested;
    game->scene.frame_entered = game->frame_index;
    game->transition_count++;
    game_rebuild_scene(game, level, 1);
  } else if (do_reload) {
    game_rebuild_scene(game, level, 0);
  }

  if (!game->level_state) {
    game_update_audio_warning_decay(game, dt);
    game_audio_update(&game->audio, dt * 1000.0f);
    return;
  }

  camera = game_level_camera(game->level_state);
  if (camera)
    game_apply_camera_input(camera, input, dt);

  if (play_tone_pressed && game->scene.current == GAME_SCENE_ARENA_LAB) {
    game_play_sound_at(&game->audio, SOUND_TONE_LOW);
  }

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB:
    game_update_arena_lab(&game->level_state->as.arena_lab, input, dt,
                          &game->audio, game);
    break;
  case GAME_SCENE_LEVEL_RELOAD:
    game_update_level_reload(&game->level_state->as.level_reload, dt);
    if (play_tone_pressed) {
      LevelReloadScene *scene = &game->level_state->as.level_reload;
      if ((game->exercise[GAME_SCENE_LEVEL_RELOAD].progress_mask &
           GAME_EXERCISE_STEP_3) != 0u) {
        game_mark_scene_exercise(game, GAME_SCENE_LEVEL_RELOAD,
                                 GAME_EXERCISE_STEP_4,
                                 "scan spike without rebuild");
      }
      scene->pulse += 0.55f;
      scene->rebuild_flash = 1.0f;
      if (scene->entity_count > 0) {
        scene->highlighted_entity =
            (scene->highlighted_entity + 5) % scene->entity_count;
      }
      game_log_eventf(game, "level scan spike -> entity %d",
                      scene->highlighted_entity);
      game_play_sound_at(&game->audio, SOUND_TONE_MID);
      game_fire_scene_warning_probe(game, GAME_SCENE_LEVEL_RELOAD);
    }
    break;
  case GAME_SCENE_POOL_LAB:
    game_update_pool_lab(&game->level_state->as.pool_lab, input, dt,
                         &game->audio, game);
    break;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    game_update_slab_audio(&game->level_state->as.slab_audio_lab, input, dt,
                           &game->audio, game);
    break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }

  if (do_reload && game->scene.current == GAME_SCENE_LEVEL_RELOAD) {
    game_mark_scene_exercise(game, GAME_SCENE_LEVEL_RELOAD,
                             GAME_EXERCISE_STEP_1,
                             "manual level rebuild requested");
  }

  if (play_tone_pressed && game->scene.current == GAME_SCENE_POOL_LAB) {
    game_mark_scene_exercise(game, GAME_SCENE_POOL_LAB, GAME_EXERCISE_STEP_1,
                             "collision burst injected");
  }

  if (play_tone_pressed && game->scene.current == GAME_SCENE_SLAB_AUDIO_LAB) {
    game_mark_scene_exercise(game, GAME_SCENE_SLAB_AUDIO_LAB,
                             GAME_EXERCISE_STEP_2, "transport burst injected");
  }

  game_update_scene_exercise_progress(game);
  game_update_audio_warning_decay(game, dt);

  game_audio_update(&game->audio, dt * 1000.0f);
}

static RenderContext game_make_world_context(Backbuffer *backbuffer,
                                             GameWorldConfig world_config,
                                             const GameCamera *camera) {
  GameWorldConfig config = world_config;
  if (camera) {
    config.camera_x = camera->x;
    config.camera_y = camera->y;
    config.camera_zoom = camera->zoom;
  }
  return make_render_context(backbuffer, config);
}

static RenderContext game_make_hud_context(Backbuffer *backbuffer,
                                           GameWorldConfig world_config) {
  GameWorldConfig config = world_config;
  config.camera_x = 0.0f;
  config.camera_y = 0.0f;
  config.camera_zoom = 1.0f;
  return make_render_context(backbuffer, config);
}

static void game_draw_world_label(Backbuffer *backbuffer,
                                  const RenderContext *ctx, float wx, float wy,
                                  const char *text, float r, float g, float b,
                                  float a) {
  draw_text(backbuffer, world_x(ctx, wx), world_y(ctx, wy),
            (int)ctx->text_scale, text, r, g, b, a);
}

static void game_render_arena_lab(GameLevelState *level_state,
                                  Backbuffer *backbuffer,
                                  const RenderContext *ctx) {
  ArenaLabScene *scene = &level_state->as.arena_lab;
  float packet_flow = fmodf(scene->beat * 1.8f, 1.0f);

  for (int i = 0; i < scene->bar_count; ++i) {
    ArenaLabBar *bar = &scene->bars[i];
    draw_rect(backbuffer, world_rect_px_x(ctx, bar->x, bar->w),
              world_rect_px_y(ctx, bar->y, bar->h), world_w(ctx, bar->w),
              world_h(ctx, bar->h), bar->r, bar->g, bar->b, bar->a);
    game_draw_world_label(backbuffer, ctx, bar->x + 0.2f, bar->y + 0.35f,
                          bar->title, COLOR_WHITE);
    game_draw_world_label(backbuffer, ctx, bar->x + 0.2f, bar->y + 0.8f,
                          bar->summary, COLOR_BLACK);
  }

  draw_rect(backbuffer, world_rect_px_x(ctx, 1.0f, 13.6f),
            world_rect_px_y(ctx, 7.0f, 1.7f), world_w(ctx, 13.6f),
            world_h(ctx, 1.7f), 0.10f, 0.12f, 0.18f, 0.9f);
  game_draw_world_label(backbuffer, ctx, 1.2f, 7.18f, scene->phase_name,
                        COLOR_WHITE);
  game_draw_world_label(backbuffer, ctx, 1.2f, 7.52f, scene->phase_summary,
                        COLOR_CYAN);

  for (int i = 0; i < scene->checkpoint_count; ++i) {
    ArenaCheckpointViz *checkpoint = &scene->checkpoints[i];
    float x = 1.4f + (float)i * 2.55f;
    float h = 0.55f + checkpoint->fill;
    draw_rect(backbuffer, world_rect_px_x(ctx, x, 1.9f),
              world_rect_px_y(ctx, 8.45f, h), world_w(ctx, 1.9f),
              world_h(ctx, h), checkpoint->r, checkpoint->g, checkpoint->b,
              0.95f);
    game_draw_world_label(backbuffer, ctx, x + 0.12f, 7.92f, checkpoint->label,
                          COLOR_WHITE);
  }

  for (int i = 0; i < 3; ++i) {
    float meter_y = 8.6f - (float)i * 0.55f;
    int live = i < scene->active_depth;
    draw_rect(backbuffer, world_rect_px_x(ctx, 10.7f, 2.2f),
              world_rect_px_y(ctx, meter_y, 0.38f), world_w(ctx, 2.2f),
              world_h(ctx, 0.38f), live ? 0.98f : 0.18f,
              live ? 0.70f - 0.10f * (float)i : 0.18f,
              live ? 0.34f + 0.18f * (float)i : 0.22f, 1.0f);
  }
  game_draw_world_label(backbuffer, ctx, 10.85f, 7.88f, "temp depth",
                        COLOR_YELLOW);

  for (int i = 0; i < 6; ++i) {
    float x = 1.25f + (float)i * 1.9f + packet_flow * 0.55f;
    float y = 6.45f + 0.18f * sinf(scene->beat * 4.0f + (float)i * 0.7f);
    draw_rect(backbuffer, world_rect_px_x(ctx, x, 0.42f),
              world_rect_px_y(ctx, y, 0.24f), world_w(ctx, 0.42f),
              world_h(ctx, 0.24f), 0.90f, 0.88f - 0.06f * (float)i,
              0.30f + 0.06f * (float)i, 1.0f);
  }
  game_draw_world_label(backbuffer, ctx, 1.2f, 6.12f,
                        "scratch packets enter, nest, rewind, and vanish",
                        COLOR_YELLOW);
}

static void game_render_level_reload(GameLevelState *level_state,
                                     Backbuffer *backbuffer,
                                     const RenderContext *ctx) {
  LevelReloadScene *scene = &level_state->as.level_reload;
  float pulse = 0.12f * sinf(scene->pulse * 2.0f);
  int scan_index = scene->entity_count > 0
                       ? ((int)(scene->scan_t * 5.0f)) % scene->entity_count
                       : 0;

  for (int i = 0; i < scene->formation_count; ++i) {
    LevelFormation *formation = &scene->formations[i];
    float flash = scene->rebuild_flash * (0.08f + 0.02f * (float)i);
    draw_rect(backbuffer, world_rect_px_x(ctx, formation->x, formation->w),
              world_rect_px_y(ctx, formation->y, formation->h),
              world_w(ctx, formation->w), world_h(ctx, formation->h),
              formation->r + flash, formation->g + flash, formation->b + flash,
              0.92f);
    game_draw_world_label(backbuffer, ctx, formation->x + 0.18f,
                          formation->y + 0.22f, formation->label, COLOR_WHITE);
  }

  for (int i = 0; i < scene->entity_count; ++i) {
    LevelEntity *entity = &scene->entities[i];
    float highlight = i == scene->highlighted_entity ? 0.18f : 0.0f;
    float scanned = i == scan_index ? 0.12f : 0.0f;
    draw_rect(backbuffer, world_rect_px_x(ctx, entity->x, entity->w + pulse),
              world_rect_px_y(ctx, entity->y, entity->h + highlight),
              world_w(ctx, entity->w + pulse),
              world_h(ctx, entity->h + highlight), entity->r + entity->heat,
              entity->g + scanned, entity->b + highlight, 1.0f);
  }

  for (int i = 0; i < scene->entity_count; ++i) {
    float x = 0.95f + (float)i * 0.74f;
    float y = 8.55f;
    float alpha = i == scan_index ? 1.0f : 0.78f;
    LevelEntity *entity = &scene->entities[i];
    draw_rect(backbuffer, world_rect_px_x(ctx, x, 0.56f),
              world_rect_px_y(ctx, y, 0.34f), world_w(ctx, 0.56f),
              world_h(ctx, 0.34f), entity->r, entity->g, entity->b, alpha);
  }

  game_draw_world_label(
      backbuffer, ctx, 1.0f, 7.05f,
      "level arena rebuilds clusters into one contiguous entity strip",
      COLOR_YELLOW);
  game_draw_world_label(
      backbuffer, ctx, 1.0f, 7.45f,
      "R rebuilds the whole strip, SPACE spikes the scan without reallocating",
      COLOR_CYAN);
  game_draw_world_label(backbuffer, ctx, 1.0f, 8.05f, "entity memory strip",
                        COLOR_WHITE);
  game_draw_world_label(backbuffer, ctx, 11.0f, 8.05f, "scan head",
                        COLOR_WHITE);
  draw_rect(backbuffer,
            world_rect_px_x(ctx, 0.95f + (float)scan_index * 0.74f, 0.62f),
            world_rect_px_y(ctx, 8.45f, 0.44f), world_w(ctx, 0.62f),
            world_h(ctx, 0.44f), 0.98f, 0.95f, 0.40f, 0.24f);
}

static void game_render_pool_lab(GameLevelState *level_state,
                                 Backbuffer *backbuffer,
                                 const RenderContext *ctx) {
  PoolLabScene *scene = &level_state->as.pool_lab;

  for (int i = 0; i < GAME_POOL_BARRIER_COUNT; ++i) {
    PoolBarrier *barrier = &scene->barriers[i];
    float glow = barrier->energy * 0.30f;
    draw_rect(backbuffer, world_rect_px_x(ctx, barrier->x, barrier->w + glow),
              world_rect_px_y(ctx, barrier->y, barrier->h + glow),
              world_w(ctx, barrier->w + glow), world_h(ctx, barrier->h + glow),
              0.88f, 0.32f + glow, 0.22f + glow, 0.92f);
  }

  for (int i = 0; i < scene->active_count; ++i) {
    PoolProbe *probe = scene->active[i];
    draw_rect(backbuffer, world_rect_px_x(ctx, probe->x, probe->size),
              world_rect_px_y(ctx, probe->y, probe->size),
              world_w(ctx, probe->size), world_h(ctx, probe->size), probe->r,
              probe->g, probe->b, 1.0f);
    if ((i % 3) == 0) {
      char label[24];
      snprintf(label, sizeof(label), "s%d g%u", probe->slot_index,
               probe->generation);
      game_draw_world_label(backbuffer, ctx, probe->x + 0.12f, probe->y - 0.18f,
                            label, COLOR_WHITE);
    }
  }

  for (size_t i = 0; i < scene->pool.slot_count; ++i) {
    int col = (int)(i % 10u);
    int row = (int)(i / 10u);
    float x = 1.0f + (float)col * 1.35f;
    float y = 7.95f + (float)row * 0.62f;
    int used = pool_lab_slot_is_active(scene, (int)i);
    int hottest = scene->hottest_slot == (int)i;
    draw_rect(backbuffer, world_rect_px_x(ctx, x, 0.5f),
              world_rect_px_y(ctx, y, 0.4f), world_w(ctx, 0.5f),
              world_h(ctx, 0.4f), used ? 0.95f : 0.20f,
              used ? 0.55f + 0.04f * (float)(scene->slot_reuses[i] % 3u)
                   : 0.25f,
              hottest ? 0.92f : (used ? 0.18f : 0.25f), 1.0f);
  }

  game_draw_world_label(
      backbuffer, ctx, 1.0f, 6.95f,
      "barriers force churn so freed slots get reused instead of forgotten",
      COLOR_YELLOW);
  game_draw_world_label(backbuffer, ctx, 1.0f, 7.35f,
                        "slot board: active, freed, and hottest reused slot",
                        COLOR_CYAN);
}

static int slab_audio_count_events_for_page(const SlabAudioScene *scene,
                                            int page_index) {
  int count = 0;
  if (!scene)
    return 0;
  for (int i = 0; i < scene->event_count; ++i) {
    if (scene->events[i] && scene->events[i]->page_index == page_index)
      count++;
  }
  return count;
}

static void game_render_slab_audio(GameLevelState *level_state,
                                   Backbuffer *backbuffer,
                                   const RenderContext *ctx) {
  SlabAudioScene *scene = &level_state->as.slab_audio_lab;

  for (int i = 0; i < GAME_SLAB_TRANSPORT_COUNT; ++i) {
    float x = 1.0f + (float)i * 3.25f;
    int active = (int)scene->state_index == i;
    draw_rect(backbuffer, world_rect_px_x(ctx, x, 2.4f),
              world_rect_px_y(ctx, 1.15f, 0.72f), world_w(ctx, 2.4f),
              world_h(ctx, 0.72f), active ? 0.90f : 0.20f,
              active ? 0.70f : 0.24f, active ? 0.34f : 0.26f, 1.0f);
    game_draw_world_label(backbuffer, ctx, x + 0.18f, 1.35f,
                          slab_audio_state_name((unsigned int)i),
                          active ? COLOR_BLACK : COLOR_WHITE);
  }

  for (int i = 0; i < scene->event_count; ++i) {
    SlabEvent *event = scene->events[i];
    float alpha = 0.45f + 0.15f * event->ttl;
    if (alpha > 1.0f)
      alpha = 1.0f;
    draw_rect(backbuffer, world_rect_px_x(ctx, event->wx, event->size),
              world_rect_px_y(ctx, event->wy, 0.6f), world_w(ctx, event->size),
              world_h(ctx, 0.6f), event->r, event->g, event->b, alpha);
    game_draw_world_label(backbuffer, ctx, event->wx + 0.08f, event->wy + 0.16f,
                          event->label, COLOR_WHITE);
  }

  for (size_t i = 0; i < scene->slab.page_count; ++i) {
    float x = 1.0f + (float)i * 3.35f;
    int used_slots = slab_audio_count_events_for_page(scene, (int)i);
    draw_rect(backbuffer, world_rect_px_x(ctx, x, 2.4f),
              world_rect_px_y(ctx, 7.35f, 1.2f), world_w(ctx, 2.4f),
              world_h(ctx, 1.2f), 0.16f, 0.26f + 0.08f * (float)i,
              0.42f + 0.08f * (float)i, 0.95f);
    for (int slot = 0; slot < GAME_SLAB_SLOTS_PER_PAGE; ++slot) {
      float sx = x + 0.16f + (float)(slot % 3) * 0.72f;
      float sy = 7.58f + ((float)slot / 3.0f) * 0.42f;
      int filled = slot < used_slots;
      draw_rect(backbuffer, world_rect_px_x(ctx, sx, 0.5f),
                world_rect_px_y(ctx, sy, 0.24f), world_w(ctx, 0.5f),
                world_h(ctx, 0.24f), filled ? 0.90f : 0.24f,
                filled ? 0.72f : 0.28f, filled ? 0.36f : 0.30f, 1.0f);
    }
    game_draw_world_label(backbuffer, ctx, x + 0.15f, 7.18f, "page",
                          COLOR_WHITE);
  }

  game_draw_world_label(backbuffer, ctx, 1.0f, 6.65f,
                        "transport state changes push events across slab pages",
                        COLOR_YELLOW);
}

static void game_render_scene(GameAppState *game, Backbuffer *backbuffer,
                              const RenderContext *ctx) {
  if (!game || !game->level_state)
    return;
  switch (game->level_state->scene_id) {
  case GAME_SCENE_ARENA_LAB:
    game_render_arena_lab(game->level_state, backbuffer, ctx);
    break;
  case GAME_SCENE_LEVEL_RELOAD:
    game_render_level_reload(game->level_state, backbuffer, ctx);
    break;
  case GAME_SCENE_POOL_LAB:
    game_render_pool_lab(game->level_state, backbuffer, ctx);
    break;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    game_render_slab_audio(game->level_state, backbuffer, ctx);
    break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }
}

static void game_draw_trace_panel_box(Backbuffer *backbuffer,
                                      const RenderContext *hud,
                                      const char *title) {
  draw_rect(backbuffer, world_rect_px_x(hud, 9.0f, 6.2f),
            world_rect_px_y(hud, 8.85f, 4.55f), world_w(hud, 6.2f),
            world_h(hud, 4.55f), 0.08f, 0.10f, 0.16f, 0.92f);
  draw_rect(backbuffer, world_rect_px_x(hud, 9.0f, 6.2f),
            world_rect_px_y(hud, 8.85f, 0.55f), world_w(hud, 6.2f),
            world_h(hud, 0.55f), 0.18f, 0.24f, 0.36f, 0.96f);
  game_draw_world_label(backbuffer, hud, 9.22f, 8.63f, title, COLOR_WHITE);
}

static void game_draw_trace_step(Backbuffer *backbuffer,
                                 const RenderContext *hud, float x, float y,
                                 const char *label, int active, int completed) {
  float r = 0.26f;
  float g = 0.26f;
  float b = 0.30f;
  float text_r = 0.502f;
  float text_g = 0.502f;
  float text_b = 0.502f;

  if (completed) {
    r = 0.24f;
    g = 0.82f;
    b = 0.44f;
    text_r = 1.0f;
    text_g = 1.0f;
    text_b = 1.0f;
  } else if (active) {
    r = 0.96f;
    g = 0.80f;
    b = 0.34f;
    text_r = 1.0f;
    text_g = 1.0f;
    text_b = 1.0f;
  }

  draw_rect(backbuffer, world_rect_px_x(hud, x, 0.22f),
            world_rect_px_y(hud, y, 0.18f), world_w(hud, 0.22f),
            world_h(hud, 0.18f), r, g, b, 1.0f);
  game_draw_world_label(backbuffer, hud, x + 0.34f, y - 0.02f, label, text_r,
                        text_g, text_b, 1.0f);
}

static const char *game_scene_exercise_prompt(GameAppState *game) {
  unsigned int progress_mask;
  unsigned int required_mask;

  if (!game)
    return "Try now: exercise the active scene and compare the result with the "
           "HUD.";

  progress_mask = game->exercise[game->scene.current].progress_mask;
  required_mask = game_scene_exercise_required_mask(game->scene.current);
  if (required_mask != 0u && (progress_mask & required_mask) == required_mask) {
    if (game->audio_warning_peak[game->scene.current] >= 0.45f &&
        !game_audio_warning_recovered(game)) {
      return "Try now: stop pressing SPACE and watch severity cool while the "
             "cause label stays scene-specific.";
    }
    if (game_audio_warning_recovered(game)) {
      return "Complete: you drove pressure up, then watched severity recover "
             "back toward green.";
    }
  }

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB:
    if ((progress_mask & GAME_EXERCISE_STEP_1) == 0u)
      return "Try now: tap SPACE once to take manual control of the checkpoint "
             "stack.";
    if ((progress_mask & GAME_EXERCISE_STEP_2) == 0u)
      return "Try now: keep tapping SPACE until two temp scopes are live at "
             "once and the cue warning line warms up.";
    if ((progress_mask & GAME_EXERCISE_STEP_3) == 0u)
      return "Try now: continue stepping until the trace enters a rewind "
             "phase.";
    if ((progress_mask & GAME_EXERCISE_STEP_4) == 0u)
      return "Try now: wait for the rewind to finish and confirm depth falls "
             "back to zero.";
    return "Complete: you stepped scratch through nesting, rewind, and a clean "
           "return to baseline.";
  case GAME_SCENE_LEVEL_RELOAD:
    if ((progress_mask & GAME_EXERCISE_STEP_1) == 0u)
      return "Try now: press R once and watch the level arena rebuild the "
             "entity strip.";
    if ((progress_mask & GAME_EXERCISE_STEP_2) == 0u)
      return "Try now: watch the rebuild flash settle so you can separate "
             "reset from rebuild work.";
    if ((progress_mask & GAME_EXERCISE_STEP_3) == 0u)
      return "Try now: wait for the scan head to walk the rebuilt strip before "
             "you spike one entity.";
    if ((progress_mask & GAME_EXERCISE_STEP_4) == 0u)
      return "Try now: press SPACE once and compare the scan spike plus "
             "warning line against the last rebuild.";
    return "Complete: you separated reset, rebuild, scan, and scan-only spike "
           "behavior in one scene.";
  case GAME_SCENE_POOL_LAB:
    if ((progress_mask & GAME_EXERCISE_STEP_1) == 0u)
      return "Try now: let the scene allocate its first fixed slot and light "
             "the board.";
    if ((progress_mask & GAME_EXERCISE_STEP_2) == 0u)
      return "Try now: press SPACE to inject a collision burst and watch the "
             "warning line move toward yellow or red.";
    if ((progress_mask & GAME_EXERCISE_STEP_3) == 0u)
      return "Try now: keep watching until one probe dies and returns its slot "
             "to the free list.";
    if ((progress_mask & GAME_EXERCISE_STEP_4) == 0u)
      return "Try now: wait for the next spawn to reuse a returned slot on the "
             "board below.";
    return "Complete: you watched the full pool cycle from allocation to "
           "reuse.";
  case GAME_SCENE_SLAB_AUDIO_LAB:
    if ((progress_mask & GAME_EXERCISE_STEP_1) == 0u)
      return "Try now: let the scene record same-size events into the first "
             "slab page.";
    if ((progress_mask & GAME_EXERCISE_STEP_2) == 0u)
      return "Try now: press SPACE once to inject a burst of same-size "
             "transport events and crowd the warning line.";
    if ((progress_mask & GAME_EXERCISE_STEP_3) == 0u)
      return "Try now: keep pressure on the scene until the slab grows by "
             "another page.";
    if ((progress_mask & GAME_EXERCISE_STEP_4) == 0u)
      return "Try now: wait for the transport machine to reach flush so reuse "
             "follows the growth spike.";
    return "Complete: you saw record, queue pressure, growth, and a later "
           "flush/reuse phase.";
  case GAME_SCENE_COUNT:
  default:
    return "Try now: exercise the active scene controls and compare the trace "
           "with the HUD metrics.";
  }
}

static const char *game_scene_exercise_observation(GameAppState *game) {
  unsigned int progress_mask;

  if (!game || !game->level_state)
    return "Observe: the scene should expose one allocator decision clearly.";

  progress_mask = game->exercise[game->scene.current].progress_mask;
  if (game->audio_warning_peak[game->scene.current] >= 0.45f) {
    if (!game_audio_warning_recovered(game)) {
      return "Observe: the cause label names why pressure happened; severity "
             "and the meter show whether it is still live or cooling.";
    }
    return "Observe: the meter cooled even though the scene mechanics stayed "
           "the same, so recovery is separate from cause.";
  }

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    if ((progress_mask & GAME_EXERCISE_STEP_4) != 0u)
      return "Observe: scratch returned to baseline, which means the transient "
             "branch really was transient.";
    return scene->active_depth == 0
               ? "Observe: scratch is back at baseline, so no transient "
                 "checkpoint escaped."
               : "Observe: nested temp scopes are live; ending them should "
                 "drop depth, not perm state.";
  }
  case GAME_SCENE_LEVEL_RELOAD: {
    LevelReloadScene *scene = &game->level_state->as.level_reload;
    if ((progress_mask & GAME_EXERCISE_STEP_4) != 0u)
      return "Observe: rebuilds change the level-owned serial, but scan spikes "
             "reuse the same strip in place.";
    return scene->rebuild_flash > 0.0f
               ? "Observe: rebuild flash means the level arena was reset and "
                 "the entity strip was rebuilt; SPACE should add audio "
                 "pressure "
                 "without changing ownership."
               : "Observe: the scan head moves over stable level-owned data "
                 "without forcing a rebuild.";
  }
  case GAME_SCENE_POOL_LAB: {
    PoolLabScene *scene = &game->level_state->as.pool_lab;
    if ((progress_mask & GAME_EXERCISE_STEP_4) != 0u)
      return "Observe: the pool never moved its backing storage; only slot "
             "ownership changed.";
    return scene->reuse_hits > 0
               ? "Observe: a returned slot was reused; pool backing stayed "
                 "fixed while occupancy changed, even when the warning line "
                 "heated up."
               : "Observe: collisions should eventually free a slot, making "
                 "reuse visible on the board below.";
  }
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    if ((progress_mask & GAME_EXERCISE_STEP_4) != 0u)
      return "Observe: new pages appear under pressure, then later transport "
             "states keep reusing those same-size slots.";
    return scene->page_growth_count > 0
               ? "Observe: allocator pressure forced a new page, but later "
                 "states still reuse same-size slots while the warning line "
                 "tracks audio crowding separately."
               : "Observe: event pressure is still fitting in current pages; "
                 "the next burst may force growth.";
  }
  case GAME_SCENE_COUNT:
  default:
    return "Observe: the trace panel should match the event log and "
           "scene-specific metric line.";
  }
}

static void game_scene_trace_step_evidence(GameAppState *game,
                                           unsigned int step_mask, char *buf,
                                           size_t buf_size) {
  if (!buf || buf_size == 0u)
    return;

  buf[0] = '\0';
  if (!game || !game->level_state)
    return;

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    if (step_mask == GAME_EXERCISE_STEP_1)
      snprintf(buf, buf_size, "manual=%u", scene->manual_steps);
    else if (step_mask == GAME_EXERCISE_STEP_2)
      snprintf(buf, buf_size, "depth=%d", scene->active_depth);
    else if (step_mask == GAME_EXERCISE_STEP_3)
      snprintf(buf, buf_size, "rewinds=%u", scene->rewind_count);
    else if (step_mask == GAME_EXERCISE_STEP_4)
      snprintf(buf, buf_size, "depth=%d", scene->active_depth);
  } break;
  case GAME_SCENE_LEVEL_RELOAD: {
    LevelReloadScene *scene = &game->level_state->as.level_reload;
    if (step_mask == GAME_EXERCISE_STEP_1)
      snprintf(buf, buf_size, "serial=%u", scene->build_serial);
    else if (step_mask == GAME_EXERCISE_STEP_2)
      snprintf(buf, buf_size, "flash=%.2f", scene->rebuild_flash);
    else if (step_mask == GAME_EXERCISE_STEP_3)
      snprintf(buf, buf_size, "scan=%.2f", scene->scan_t);
    else if (step_mask == GAME_EXERCISE_STEP_4)
      snprintf(buf, buf_size, "focus=%d", scene->highlighted_entity);
  } break;
  case GAME_SCENE_POOL_LAB: {
    PoolLabScene *scene = &game->level_state->as.pool_lab;
    if (step_mask == GAME_EXERCISE_STEP_1)
      snprintf(buf, buf_size, "active=%d", scene->active_count);
    else if (step_mask == GAME_EXERCISE_STEP_2)
      snprintf(buf, buf_size, "hits=%u", scene->collision_count);
    else if (step_mask == GAME_EXERCISE_STEP_3)
      snprintf(buf, buf_size, "free=%zu", scene->pool.free_count);
    else if (step_mask == GAME_EXERCISE_STEP_4)
      snprintf(buf, buf_size, "reuse=%u", scene->reuse_hits);
  } break;
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    if (step_mask == GAME_EXERCISE_STEP_1)
      snprintf(buf, buf_size, "events=%d", scene->event_count);
    else if (step_mask == GAME_EXERCISE_STEP_2)
      snprintf(buf, buf_size, "bursts=%u", scene->burst_count);
    else if (step_mask == GAME_EXERCISE_STEP_3)
      snprintf(buf, buf_size, "pages=%zu", scene->slab.page_count);
    else if (step_mask == GAME_EXERCISE_STEP_4)
      snprintf(buf, buf_size, "mode=%s",
               slab_audio_state_name(scene->state_index));
  } break;
  case GAME_SCENE_COUNT:
  default:
    break;
  }
}

static void game_scene_trace_step_label(GameAppState *game,
                                        unsigned int step_mask,
                                        const char *base_label, char *buf,
                                        size_t buf_size) {
  char evidence[48];

  if (!buf || buf_size == 0u)
    return;

  if (!game || !base_label) {
    buf[0] = '\0';
    return;
  }

  if ((game->exercise[game->scene.current].progress_mask & step_mask) == 0u) {
    snprintf(buf, buf_size, "%s", base_label);
    return;
  }

  game_scene_trace_step_evidence(game, step_mask, evidence, sizeof(evidence));
  if (evidence[0] != '\0')
    snprintf(buf, buf_size, "%s [%s]", base_label, evidence);
  else
    snprintf(buf, buf_size, "%s", base_label);
}

static const char *game_scene_audio_trace_title(GameSceneID scene_id) {
  switch (scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return "Audio Trace: checkpoint cues";
  case GAME_SCENE_LEVEL_RELOAD:
    return "Audio Trace: scan vs rebuild";
  case GAME_SCENE_POOL_LAB:
    return "Audio Trace: voice churn";
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return "Audio Trace: transport + sequencer";
  case GAME_SCENE_COUNT:
  default:
    return "Audio Trace";
  }
}

static void game_describe_scene_audio_trace(GameAppState *game, char *line1,
                                            size_t line1_size, char *line2,
                                            size_t line2_size) {
  if (!line1 || line1_size == 0u || !line2 || line2_size == 0u) {
    return;
  }

  line1[0] = '\0';
  line2[0] = '\0';
  if (!game || !game->level_state)
    return;

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    snprintf(line1, line1_size, "voices:%d free:%d phase:%s cue_depth:%d",
             game_active_voice_count(&game->audio),
             game_free_voice_count(&game->audio), scene->phase_name,
             scene->active_depth);
    snprintf(line2, line2_size, "rewinds:%u music_step:%d tone:%.2f",
             scene->rewind_count, game->audio.sequencer.current_step,
             game->audio.sequencer.tone.current_volume);
  } break;
  case GAME_SCENE_LEVEL_RELOAD: {
    LevelReloadScene *scene = &game->level_state->as.level_reload;
    snprintf(line1, line1_size, "pattern:%d step:%d flash:%.2f scan:%.2f",
             game->audio.sequencer.current_pattern,
             game->audio.sequencer.current_step, scene->rebuild_flash,
             scene->scan_t);
    snprintf(line2, line2_size, "music:%.2f sfx:%.2f voices:%d focus:%d",
             game->audio.music_volume, game->audio.sfx_volume,
             game_active_voice_count(&game->audio), scene->highlighted_entity);
  } break;
  case GAME_SCENE_POOL_LAB: {
    PoolLabScene *scene = &game->level_state->as.pool_lab;
    snprintf(line1, line1_size, "voices:%d free:%d oldest:%d hottest_slot:%d",
             game_active_voice_count(&game->audio),
             game_free_voice_count(&game->audio),
             game_oldest_active_voice_age(&game->audio),
             game_hottest_voice_slot(&game->audio));
    snprintf(line2, line2_size, "collisions:%u bursts:%u reuse:%u sfx:%.2f",
             scene->collision_count, scene->burst_count, scene->reuse_hits,
             game->audio.sfx_volume);
  } break;
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    snprintf(line1, line1_size, "mode:%s pages:%zu growth:%u events:%d",
             slab_audio_state_name(scene->state_index), scene->slab.page_count,
             scene->page_growth_count, scene->event_count);
    snprintf(line2, line2_size, "pattern:%d step:%d step_left:%d tone:%.2f",
             game->audio.sequencer.current_pattern,
             game->audio.sequencer.current_step,
             game->audio.sequencer.step_samples_remaining,
             game->audio.sequencer.tone.current_volume);
  } break;
  case GAME_SCENE_COUNT:
  default:
    snprintf(line1, line1_size, "voices:%d free:%d master:%.2f",
             game_active_voice_count(&game->audio),
             game_free_voice_count(&game->audio), game->audio.master_volume);
    snprintf(line2, line2_size, "pattern:%d step:%d",
             game->audio.sequencer.current_pattern,
             game->audio.sequencer.current_step);
    break;
  }
}

static void game_draw_audio_trace_panel(GameAppState *game,
                                        Backbuffer *backbuffer,
                                        const RenderContext *hud) {
  char line1[112];
  char line2[112];
  char line3[128];
  char line4[96];
  char line5[112];

  if (!game || !game->level_state)
    return;

  draw_rect(backbuffer, world_rect_px_x(hud, 9.0f, 6.2f),
            world_rect_px_y(hud, 4.18f, 2.68f), world_w(hud, 6.2f),
            world_h(hud, 2.68f), 0.08f, 0.11f, 0.15f, 0.92f);
  draw_rect(backbuffer, world_rect_px_x(hud, 9.0f, 6.2f),
            world_rect_px_y(hud, 4.18f, 0.42f), world_w(hud, 6.2f),
            world_h(hud, 0.42f), 0.17f, 0.22f, 0.30f, 0.96f);
  game_draw_world_label(backbuffer, hud, 9.22f, 3.98f,
                        game_scene_audio_trace_title(game->scene.current),
                        COLOR_WHITE);

  game_describe_scene_audio_trace(game, line1, sizeof(line1), line2,
                                  sizeof(line2));
  game_describe_audio_anomaly(game, line3, sizeof(line3));
  game_describe_audio_warning_context(game, line4, sizeof(line4));
  game_describe_audio_warning_meter(game, line5, sizeof(line5));
  game_draw_world_label(backbuffer, hud, 9.22f, 3.54f, line1, COLOR_CYAN);
  game_draw_world_label(backbuffer, hud, 9.22f, 3.18f, line2, COLOR_YELLOW);
  game_draw_audio_anomaly_label(game, backbuffer, hud, 9.22f, 2.82f, line3);
  game_draw_world_label(backbuffer, hud, 9.22f, 2.48f, line4, COLOR_WHITE);
  game_draw_world_label(backbuffer, hud, 9.22f, 2.18f, line5, COLOR_WHITE);
  game_draw_audio_warning_meter(game, backbuffer, hud, 9.22f, 1.96f);
}

static void game_draw_scene_trace_panel(GameAppState *game,
                                        Backbuffer *backbuffer,
                                        const RenderContext *hud) {
  char detail[160];
  char step1[96];
  char step2[96];
  char step3[96];
  char step4[96];
  unsigned int progress_mask;
  unsigned int required_mask;
  int active_step = 0;
  float y = 8.05f;

  if (!game || !game->level_state)
    return;

  progress_mask = game->exercise[game->scene.current].progress_mask;
  required_mask = game_scene_exercise_required_mask(game->scene.current);
  active_step = game_scene_exercise_focus_step(progress_mask, required_mask);

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    game_draw_trace_panel_box(backbuffer, hud, "Arena Trace");
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_1,
                                "1 baseline owner stays in level", step1,
                                sizeof(step1));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_2,
                                "2 open and nest temp scopes", step2,
                                sizeof(step2));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_3,
                                "3 rewind risky branch work", step3,
                                sizeof(step3));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_4,
                                "4 hand off only persistent state", step4,
                                sizeof(step4));
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step1, active_step == 0,
                         (progress_mask & GAME_EXERCISE_STEP_1) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step2, active_step == 1,
                         (progress_mask & GAME_EXERCISE_STEP_2) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step3, active_step == 2,
                         (progress_mask & GAME_EXERCISE_STEP_3) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step4, active_step == 3,
                         (progress_mask & GAME_EXERCISE_STEP_4) != 0u);
    snprintf(detail, sizeof(detail),
             "manual:%u depth:%d rewinds:%u | exercise:%s %d/%d",
             scene->manual_steps, scene->active_depth, scene->rewind_count,
             game_scene_exercise_status_name(game, game->scene.current),
             game_count_bits(progress_mask & required_mask),
             game_count_bits(required_mask));
    break;
  }
  case GAME_SCENE_LEVEL_RELOAD: {
    LevelReloadScene *scene = &game->level_state->as.level_reload;
    game_draw_trace_panel_box(backbuffer, hud, "Level Trace");
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_1,
                                "1 reset the level arena", step1,
                                sizeof(step1));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_2,
                                "2 rebuild formation headers", step2,
                                sizeof(step2));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_3,
                                "3 scan one contiguous entity strip", step3,
                                sizeof(step3));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_4,
                                "4 spike one entity without realloc", step4,
                                sizeof(step4));
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step1, active_step == 0,
                         (progress_mask & GAME_EXERCISE_STEP_1) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step2, active_step == 1,
                         (progress_mask & GAME_EXERCISE_STEP_2) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step3, active_step == 2,
                         (progress_mask & GAME_EXERCISE_STEP_3) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step4, active_step == 3,
                         (progress_mask & GAME_EXERCISE_STEP_4) != 0u);
    snprintf(detail, sizeof(detail),
             "serial:%u focus:%d seed:%u | exercise:%s %d/%d",
             scene->build_serial, scene->highlighted_entity, scene->reload_seed,
             game_scene_exercise_status_name(game, game->scene.current),
             game_count_bits(progress_mask & required_mask),
             game_count_bits(required_mask));
    break;
  }
  case GAME_SCENE_POOL_LAB: {
    PoolLabScene *scene = &game->level_state->as.pool_lab;
    game_draw_trace_panel_box(backbuffer, hud, "Pool Trace");
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_1,
                                "1 allocate a fixed slot", step1,
                                sizeof(step1));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_2,
                                "2 collide and age out under churn", step2,
                                sizeof(step2));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_3,
                                "3 return slot to the free list", step3,
                                sizeof(step3));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_4,
                                "4 reuse the hottest returned slot", step4,
                                sizeof(step4));
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step1, active_step == 0,
                         (progress_mask & GAME_EXERCISE_STEP_1) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step2, active_step == 1,
                         (progress_mask & GAME_EXERCISE_STEP_2) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step3, active_step == 2,
                         (progress_mask & GAME_EXERCISE_STEP_3) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step4, active_step == 3,
                         (progress_mask & GAME_EXERCISE_STEP_4) != 0u);
    snprintf(detail, sizeof(detail),
             "reuse:%u hottest:%d free:%zu | exercise:%s %d/%d",
             scene->reuse_hits, scene->hottest_slot, scene->pool.free_count,
             game_scene_exercise_status_name(game, game->scene.current),
             game_count_bits(progress_mask & required_mask),
             game_count_bits(required_mask));
    break;
  }
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    game_draw_trace_panel_box(backbuffer, hud, "Slab Trace");
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_1,
                                "1 record new same-size events", step1,
                                sizeof(step1));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_2,
                                "2 queue pressure across pages", step2,
                                sizeof(step2));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_3,
                                "3 grow a page under queue pressure", step3,
                                sizeof(step3));
    game_scene_trace_step_label(game, GAME_EXERCISE_STEP_4,
                                "4 flush and reuse freed slots", step4,
                                sizeof(step4));
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step1, active_step == 0,
                         (progress_mask & GAME_EXERCISE_STEP_1) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step2, active_step == 1,
                         (progress_mask & GAME_EXERCISE_STEP_2) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step3, active_step == 2,
                         (progress_mask & GAME_EXERCISE_STEP_3) != 0u);
    y -= 0.55f;
    game_draw_trace_step(backbuffer, hud, 9.22f, y, step4, active_step == 3,
                         (progress_mask & GAME_EXERCISE_STEP_4) != 0u);
    snprintf(detail, sizeof(detail),
             "mode:%s pages:%zu growth:%u | exercise:%s %d/%d",
             slab_audio_state_name(scene->state_index), scene->slab.page_count,
             scene->page_growth_count,
             game_scene_exercise_status_name(game, game->scene.current),
             game_count_bits(progress_mask & required_mask),
             game_count_bits(required_mask));
    break;
  }
  case GAME_SCENE_COUNT:
  default:
    return;
  }

  game_draw_world_label(backbuffer, hud, 9.22f, 6.15f, detail, COLOR_YELLOW);
  game_draw_world_label(backbuffer, hud, 9.22f, 5.72f,
                        game_scene_exercise_prompt(game), COLOR_CYAN);
  game_draw_world_label(backbuffer, hud, 9.22f, 5.28f,
                        game_scene_exercise_observation(game), COLOR_WHITE);

  game_draw_audio_trace_panel(game, backbuffer, hud);
}

static void game_draw_hud(GameAppState *game, Backbuffer *backbuffer, int fps,
                          WindowScaleMode scale_mode, const RenderContext *hud,
                          Arena *perm, Arena *level, Arena *scratch) {
  TextCursor text;
  char *buf;
  const ArenaStats *perm_stats;
  const ArenaStats *level_stats;
  const ArenaStats *scratch_stats;

  if (!game || !hud || !game->hud_visible)
    return;

  perm_stats = arena_get_stats(perm);
  level_stats = arena_get_stats(level);
  scratch_stats = arena_get_stats(scratch);
  text = make_cursor(hud, 0.7f, 8.9f);

  draw_text(backbuffer, text.x, text.y, (int)(hud->text_scale * 2.0f),
            game_scene_name(game->scene.current), COLOR_WHITE);
  text.y += (float)GLYPH_H * hud->text_scale * 1.8f;

  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale,
            game_scene_summary(game->scene.current), COLOR_CYAN);
  cursor_newline(&text);

  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale,
            game_scene_action_hint(game->scene.current), COLOR_YELLOW);
  cursor_newline(&text);

  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale,
            "Controls: [ prev ] next  R reload  F1 HUD  F2 lock override  F3 "
            "clear  SPACE scene action",
            COLOR_GRAY);
  cursor_newline(&text);

  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale,
            "Camera: arrows pan  Q/E zoom  C reset", COLOR_GRAY);
  cursor_newline(&text);

  buf = ARENA_PUSH_ARRAY(scratch, 160, char);
  if (!buf)
    return;

  snprintf(buf, 160,
           "State current:%s requested:%s previous:%s transitions:%u scale:%s "
           "fps:%d",
           game_scene_name(game->scene.current),
           game_scene_name(game->scene.requested),
           game_scene_name(game->scene.previous), game->transition_count,
           game_scale_mode_name(scale_mode), fps);
  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf, COLOR_CYAN);
  cursor_newline(&text);

  snprintf(buf, 160, "Override compile:%s%s runtime:%s%s forced:%s frame:%llu",
           game->scene.compile_time_override_active ? "on" : "off",
           game->scene.compile_time_locked ? "(locked)" : "",
           game->scene.runtime_override_active ? "on" : "off",
           game->scene.runtime_override_locked ? "(locked)" : "",
           game_scene_name(game->scene.runtime_forced_scene),
           game->frame_index);
  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf, COLOR_CYAN);
  cursor_newline(&text);

  if (game->rebuild_failure_active) {
    snprintf(buf, 160,
             "Runtime warning: rebuild retry pending for %s | failures:%u "
             "last_frame:%llu",
             game_scene_name(game->rebuild_failure_scene),
             game->rebuild_failure_count, game->rebuild_failure_frame);
    draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf, COLOR_RED);
    cursor_newline(&text);
  }

  snprintf(buf, 160,
           "Arena used perm:%zu level:%zu scratch:%zu | live_scopes:%d "
           "reloads:%u enters:%u",
           perm_stats->used_bytes, level_stats->used_bytes,
           scratch_stats->used_bytes, scratch->temp_count,
           game->scene_reload_count[game->scene.current],
           game->scene_enter_count[game->scene.current]);
  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf, COLOR_CYAN);
  cursor_newline(&text);

  snprintf(
      buf, 160,
      "Audio music_step:%d pattern:%d voices:%d music_vol:%.2f sfx_vol:%.2f",
      game->audio.sequencer.current_step, game->audio.sequencer.current_pattern,
      game_active_voice_count(&game->audio), game->audio.music_volume,
      game->audio.sfx_volume);
  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf, COLOR_CYAN);
  cursor_newline(&text);

  game_describe_audio_evidence(game, buf, 160);
  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf, COLOR_CYAN);
  cursor_newline(&text);

  if (!game->level_state) {
    snprintf(buf, 160,
             "Scene state unavailable: waiting for rebuild of %s to succeed",
             game_scene_name(game->scene.current));
    draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf,
              COLOR_YELLOW);
    for (int i = 0; i < game->log_count; ++i) {
      int log_index = (game->log_head - 1 - i + GAME_ARENA_LOG_CAPACITY) %
                      GAME_ARENA_LOG_CAPACITY;
      text.y += (float)GLYPH_H * hud->text_scale * (i == 0 ? 1.4f : 1.1f);
      draw_text(backbuffer, text.x, text.y, (int)hud->text_scale,
                game->logs[log_index].text, i == 0 ? COLOR_WHITE : COLOR_GRAY);
    }
    return;
  }

  switch (game->scene.current) {
  case GAME_SCENE_ARENA_LAB: {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    snprintf(
        buf, 160,
        "Arena phase:%s depth:%d temp_bytes:%zu peak:%zu rewinds:%u cycles:%u",
        scene->phase_name, scene->active_depth, scene->simulated_temp_bytes,
        scene->peak_temp_bytes, scene->rewind_count, scene->cycle_count);
    break;
  }
  case GAME_SCENE_LEVEL_RELOAD: {
    LevelReloadScene *scene = &game->level_state->as.level_reload;
    snprintf(buf, 160,
             "Level formations:%d entities:%d seed:%u flash:%.2f scan:%.2f",
             scene->formation_count, scene->entity_count, scene->reload_seed,
             scene->rebuild_flash, scene->scan_t);
    break;
  }
  case GAME_SCENE_POOL_LAB: {
    PoolLabScene *scene = &game->level_state->as.pool_lab;
    snprintf(buf, 160,
             "Pool active:%d collisions:%u escapes:%u hottest:%d bursts:%u",
             scene->active_count, scene->collision_count, scene->escaped_count,
             scene->hottest_slot, scene->burst_count);
    break;
  }
  case GAME_SCENE_SLAB_AUDIO_LAB: {
    SlabAudioScene *scene = &game->level_state->as.slab_audio_lab;
    snprintf(
        buf, 160,
        "Slab mode:%s events:%d pages:%zu growth:%u bursts:%u phase_swaps:%u",
        slab_audio_state_name(scene->state_index), scene->event_count,
        scene->slab.page_count, scene->page_growth_count, scene->burst_count,
        scene->phase_changes);
    break;
  }
  default:
    snprintf(
        buf, 160, "Arena peak perm:%zu level:%zu scratch:%zu temp_scopes:%zu",
        perm_stats->peak_used_bytes, level_stats->peak_used_bytes,
        scratch_stats->peak_used_bytes, scratch_stats->peak_temp_scope_count);
    break;
  }
  draw_text(backbuffer, text.x, text.y, (int)hud->text_scale, buf,
            COLOR_YELLOW);

  for (int i = 0; i < game->log_count; ++i) {
    int log_index = (game->log_head - 1 - i + GAME_ARENA_LOG_CAPACITY) %
                    GAME_ARENA_LOG_CAPACITY;
    text.y += (float)GLYPH_H * hud->text_scale * (i == 0 ? 1.4f : 1.1f);
    draw_text(backbuffer, text.x, text.y, (int)hud->text_scale,
              game->logs[log_index].text, i == 0 ? COLOR_WHITE : COLOR_GRAY);
  }

  game_draw_scene_trace_panel(game, backbuffer, hud);
}

void game_app_render(GameAppState *game, Backbuffer *backbuffer, int fps,
                     WindowScaleMode scale_mode, GameWorldConfig world_config,
                     Arena *perm, Arena *level, Arena *scratch) {
  GameCamera fallback_camera = game_default_camera();
  GameCamera *camera;
  RenderContext world_ctx;
  RenderContext hud_ctx;
  TempMemory render_temp;
  TempMemory arena_temps[GAME_ARENA_CHECKPOINT_CAPACITY];
  int arena_temp_count = 0;

  if (!game || !backbuffer || !scratch)
    return;

  PERF_BEGIN_NAMED(game_app_render_total, "game_app/render_total");
  render_temp = arena_begin_temp(scratch);
  camera = game_level_camera(game->level_state);
  if (!camera)
    camera = &fallback_camera;

  world_ctx = game_make_world_context(backbuffer, world_config, camera);
  hud_ctx = game_make_hud_context(backbuffer, world_config);

  if (game->level_state && game->scene.current == GAME_SCENE_ARENA_LAB) {
    ArenaLabScene *scene = &game->level_state->as.arena_lab;
    for (int i = 0;
         i < scene->active_depth && i < GAME_ARENA_CHECKPOINT_CAPACITY; ++i) {
      size_t bytes = 48u + (size_t)i * 56u;
      arena_temps[arena_temp_count] = arena_begin_temp(scratch);
      if (arena_push_zero(scratch, bytes, 16))
        arena_temp_count++;
      else
        arena_end_temp(arena_temps[arena_temp_count]);
    }
  }

  draw_rect(
      backbuffer, 0, 0, (float)backbuffer->width, (float)backbuffer->height,
      game->scene.current == GAME_SCENE_SLAB_AUDIO_LAB ? 0.06f : 0.08f,
      game->scene.current == GAME_SCENE_POOL_LAB ? 0.10f : 0.08f, 0.12f, 1.0f);

  game_render_scene(game, backbuffer, &world_ctx);
  game_draw_hud(game, backbuffer, fps, scale_mode, &hud_ctx, perm, level,
                scratch);

  while (arena_temp_count > 0) {
    arena_temp_count--;
    arena_end_temp(arena_temps[arena_temp_count]);
  }

  arena_end_temp(render_temp);
  PERF_END(game_app_render_total);
}

void game_app_get_audio_samples(GameAppState *game, AudioOutputBuffer *buf,
                                int num_frames) {
  if (!game || !buf)
    return;
  game_get_audio_samples(&game->audio, buf, num_frames);
}