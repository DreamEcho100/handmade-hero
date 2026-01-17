#ifndef HANDMADE_COMMON_H
#define HANDMADE_COMMON_H
#include "../../base.h"

extern int g_frame_log_counter;
extern int g_debug_fps;

#define FRAME_LOG_EVERY_ONE_SECONDS_CHECK ((g_frame_log_counter % (g_debug_fps)) == 0)
#define FRAME_LOG_EVERY_FIVE_SECONDS_CHECK ((g_frame_log_counter % (g_debug_fps * 5)) == 0)
#define FRAME_LOG_EVERY_TEN_SECONDS_CHECK ((g_frame_log_counter % (g_debug_fps * 10)) == 0)

#endif // HANDMADE_COMMON_H