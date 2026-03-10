#ifndef GAME_BASE_H
#define GAME_BASE_H

#include "audio.h"
#include <stdbool.h>

#define GRID_WIDTH 60
#define GRID_HEIGHT 20
#define CELL_SIZE 14
#define HEADER_ROWS 3

#define SCREEN_INITIAL_WIDTH (GRID_WIDTH * CELL_SIZE)
#define SCREEN_INITIAL_HEIGHT ((GRID_HEIGHT + HEADER_ROWS) * CELL_SIZE)
#define MAX_SNAKE (GRID_WIDTH * GRID_HEIGHT)

typedef enum {
  SNAKE_DIR_UP = 0,
  SNAKE_DIR_RIGHT = 1,
  SNAKE_DIR_DOWN = 2,
  SNAKE_DIR_LEFT = 3
} SNAKE_DIR;

#define DIRECTION_SNAKE_SIZE 4
static const int DIRECTION_SNAKE_X[DIRECTION_SNAKE_SIZE] = {0, 1, 0, -1};
static const int DIRECTION_SNAKE_Y[DIRECTION_SNAKE_SIZE] = {-1, 0, 1, 0};

typedef struct {
  float timer;
  float interval;
} GameActionRepeat;

#define COLOR_BLACK GAME_RGB(0, 0, 0)
#define COLOR_WHITE GAME_RGB(255, 255, 255)
#define COLOR_RED GAME_RGB(220, 50, 50)
#define COLOR_DARK_RED GAME_RGB(139, 0, 0)
#define COLOR_LIME_GREEN GAME_RGB(50, 205, 50)
#define COLOR_DARK_GREEN GAME_RGB(0, 128, 0)
#define COLOR_YELLOW GAME_RGB(255, 215, 0)
#define COLOR_DARK_GRAY GAME_RGB(64, 64, 64)
#define COLOR_GRAY GAME_RGB(128, 128, 128)
#define COLOR_GREEN GAME_RGB(50, 205, 50)

typedef struct {
  int x;
  int y;
} Segment;

/* ─── Input System ───────────────────────────────────────────── */

typedef struct {
  int half_transition_count;
  int ended_down;
} GameButtonState;

#define UPDATE_BUTTON(button, is_down)                                         \
  do {                                                                         \
    if ((button).ended_down != (is_down)) {                                    \
      (button).half_transition_count++;                                        \
      (button).ended_down = (is_down);                                         \
    }                                                                          \
  } while (0)

#define BUTTON_COUNT 2

typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState turn_left;
      GameButtonState turn_right;
    };
  };
  int restart;
  int quit;
} GameInput;

typedef struct {
  Segment segments[MAX_SNAKE];
  int head;
  int tail;
  int length;

  SNAKE_DIR direction;
  SNAKE_DIR next_direction;
  int grow_pending;

  GameActionRepeat move_repeat;
} Snake;

typedef struct {
  Snake snake;
  bool is_game_over;
  int score;
  int best_score;
  GameAudioState audio;
  struct {
    int x;
    int y;
  } food;
} GameState;

#endif // GAME_BASE_H
