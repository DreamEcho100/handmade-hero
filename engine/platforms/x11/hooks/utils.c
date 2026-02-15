#include "../../_common/hooks/utils.h"
#include "../../../game/base.h"
#include "../../_common/frame-timing.h"

void de100_set_target_fps(u32 fps) { g_fps = fps; }

f32 de100_get_frame_time(void) { return g_frame_timing.total_seconds; }

f64 de100_get_time(void) {
  return (de100_get_wall_clock() - g_initial_game_time_ms) / 1000.0f;
}

u32 de100_get_fps(void) { return g_fps; }
