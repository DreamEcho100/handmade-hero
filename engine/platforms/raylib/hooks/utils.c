#include "../../_common/hooks/utils.h"
#include "../../../game/base.h"
#include <raylib.h>

#if DE100_INTERNAL
#include <stdio.h>
#endif

void de100_set_target_fps(uint32 fps) {
  SetTargetFPS((int)fps);
  g_fps = fps;

#if DE100_INTERNAL
  printf("🎯 Raylib SetTargetFPS(%u)\n", fps);
#endif
}

f32 de100_get_frame_time(void) { return GetFrameTime(); }

f64 de100_get_time(void) { return (f64)GetTime(); }

uint32 de100_get_fps(void) { return (uint32)GetFPS(); }
