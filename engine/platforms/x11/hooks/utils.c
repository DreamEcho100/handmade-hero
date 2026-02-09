#include "../../_common/hooks/utils.h"
#include "../../../game/base.h"
#include "../../_common/frame-timing.h"

void de100_set_target_fps(uint32 fps) { g_fps = fps; }

real32 de100_get_frame_time(void) { return frame_timing_get_ms() / 1000.0f; }

real64 de100_get_time(void) {
  return (get_wall_clock() - g_initial_game_time) / 1000.0f;
}

uint32 de100_get_fps(void) { return g_fps; }
