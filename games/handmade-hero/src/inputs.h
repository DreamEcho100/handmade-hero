#ifndef HANDMADE_HERO_GAME_INPUTS_H
#define HANDMADE_HERO_GAME_INPUTS_H

// Only include the base (for GameButtonState type)
#include "../../../engine/game/inputs-base.h"

#ifndef DE100_GAME_BUTTON_FIELDS
#define DE100_GAME_BUTTON_FIELDS                                                     \
  GameButtonState move_up;                                                     \
  GameButtonState move_down;                                                   \
  GameButtonState move_left;                                                   \
  GameButtonState move_right;                                                  \
                                                                               \
  GameButtonState action_up;                                                   \
  GameButtonState action_down;                                                 \
  GameButtonState action_left;                                                 \
  GameButtonState action_right;                                                \
                                                                               \
  GameButtonState left_shoulder;                                               \
  GameButtonState right_shoulder;                                              \
                                                                               \
  GameButtonState back;                                                        \
  GameButtonState start;
#endif

// Auto-count using sizeof
typedef struct { DE100_GAME_BUTTON_FIELDS } _GameButtonsCounter;
#ifndef DE100_GAME_BUTTON_COUNT
#define DE100_GAME_BUTTON_COUNT (uint32)(sizeof(_GameButtonsCounter) / sizeof(GameButtonState))
#endif

#endif
