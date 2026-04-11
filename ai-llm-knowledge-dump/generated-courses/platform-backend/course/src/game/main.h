#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include "../game/base.h"
#include "../utils/arena.h"
#include "../utils/audio.h"
#include "../utils/backbuffer.h"
#include "../utils/base.h"

typedef enum {
  GAME_SCENE_ARENA_LAB = 0,
  GAME_SCENE_LEVEL_RELOAD,
  GAME_SCENE_POOL_LAB,
  GAME_SCENE_SLAB_AUDIO_LAB,
  GAME_SCENE_COUNT
} GameSceneID;

typedef struct GameAppState GameAppState;

int game_app_init(GameAppState **out_game, Arena *perm);
void game_app_update(GameAppState *game, GameInput *input, float dt,
                     Arena *level);
void game_app_render(GameAppState *game, Backbuffer *backbuffer, int fps,
                     WindowScaleMode scale_mode, GameWorldConfig world_config,
                     Arena *perm, Arena *level, Arena *scratch);
void game_app_get_audio_samples(GameAppState *game, AudioOutputBuffer *buf,
                                int num_frames);

#endif /* GAME_MAIN_H */