#ifndef HANDMADE_HERO_GAME_H
#define HANDMADE_HERO_GAME_H

#include "../engine/_common/base.h"
#include "../engine/_common/memory.h"
#include "../engine/game/audio.h"
#include "../engine/game/backbuffer.h"
#include "../engine/game/base.h"
#include "../engine/game/input.h"
#include "../engine/game/memory.h"

GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

typedef struct {
  // Audio state (embedded struct, not pointer)
  GameAudioState audio;
  // Future: More game-specific state
  // PlayerState player;
  // WorldState world;

  // // Audio state (matches Casey's game_state)
  // int tone_hz;
  // real32 t_sine; // Phase accumulator
  
  // Your other state
  GradientState gradient_state;
  PixelState pixel_state;
  int speed;
} HandMadeHeroGameState;

#endif // HANDMADE_HERO_GAME_H
