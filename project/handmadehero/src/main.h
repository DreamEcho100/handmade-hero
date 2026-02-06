#ifndef DE100_HERO_GAME_H
#define DE100_HERO_GAME_H
#include "./inputs.h"

#include "../../engine/_common/base.h"
#include "../../engine/_common/memory.h"
#include "../../engine/game/audio.h"
#include "../../engine/game/backbuffer.h"
#include "../../engine/game/base.h"
#include "../../engine/game/game-loader.h"
#include "../../engine/game/inputs.h"
#include "../../engine/game/memory.h"

GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

typedef struct {
  int32 offset_x;
  int32 offset_y;
} GradientState;

typedef struct {
  int32 offset_x;
  int32 offset_y;
} PixelState;

typedef struct {
  int32 x;
  int32 y;
  real32 t_jump;
} PlayerState;

typedef struct {
  // Audio state (embedded struct, not pointer)
  GameAudioState audio;
  // Future: More game-specific state
  // PlayerState player;
  // WorldState world;

  // // Audio state (matches Casey's game_state)
  // int32 tone_hz;
  // real32 t_sine; // Phase accumulator

  // Your other state
  GradientState gradient_state;
  PixelState pixel_state;
  PlayerState player_state;
  int32 speed;
} HandMadeHeroGameState;

#endif // DE100_HERO_GAME_H
