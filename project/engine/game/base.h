#ifndef ENGINE_GAME_BASE_H
#define ENGINE_GAME_BASE_H

#include "audio.h"
#include "backbuffer.h"
#include "input.h"
#include "memory.h"
#include <stdbool.h>
#include <time.h>

extern bool is_game_running;
extern bool g_game_is_paused;
extern bool g_reload_requested;
extern uint32_t g_frame_counter;
extern uint32_t g_fps;
extern uint32_t g_reload_check_interval;
extern uint32_t g_reload_check_counter;

#define FRAME_LOG_EVERY_ONE_SECONDS_CHECK ((g_frame_counter % (g_fps)) == 0)
#define FRAME_LOG_EVERY_THREE_SECONDS_CHECK                                    \
  ((g_frame_counter % (g_fps * 3)) == 0)
#define FRAME_LOG_EVERY_FIVE_SECONDS_CHECK                                     \
  ((g_frame_counter % (g_fps * 5)) == 0)
#define FRAME_LOG_EVERY_TEN_SECONDS_CHECK                                      \
  ((g_frame_counter % (g_fps * 10)) == 0)

#define RELOAD_CHECK_EVERY_ONE_SECONDS_CHECK                                   \
  ((g_reload_check_counter % (g_reload_check_interval)) == 0)

#endif // ENGINE_GAME_BASE_H
