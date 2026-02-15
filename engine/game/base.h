#ifndef DE100_GAME_BASE_H
#define DE100_GAME_BASE_H

#include "audio.h"
#include "backbuffer.h"
#include "inputs.h"
#include "memory.h"
#include <stdbool.h>

extern bool is_game_running;
extern bool g_game_is_paused;
// extern bool g_window_is_active;
extern bool g_reload_requested;

extern f64 g_initial_game_time_ms;

extern uint32 g_frame_counter;
extern uint32 g_fps;

#define FRAME_LOG_EVERY_ONE_SECONDS_CHECK ((g_frame_counter % (g_fps)) == 0)
#define FRAME_LOG_EVERY_THREE_SECONDS_CHECK                                    \
  ((g_frame_counter % (g_fps * 3)) == 0)
#define FRAME_LOG_EVERY_FIVE_SECONDS_CHECK                                     \
  ((g_frame_counter % (g_fps * 5)) == 0)
#define FRAME_LOG_EVERY_TEN_SECONDS_CHECK                                      \
  ((g_frame_counter % (g_fps * 10)) == 0)

#endif // DE100_GAME_BASE_H
