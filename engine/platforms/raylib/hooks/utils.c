#include "../../../game/base.h"
#include "../../_common/hooks/utils.h"
#include <raylib.h>

void de100_set_target_fps(uint32 fps) {
  SetTargetFPS((int)fps);
  g_fps = fps;

#if DE100_INTERNAL
  printf("🎯 Raylib SetTargetFPS(%u)\n", fps);
#endif
}

real32 de100_get_frame_time(void) { return GetFrameTime(); }

real64 de100_get_time(void) { return (real64)GetTime(); }

uint32 de100_get_fps(void) { return (uint32)GetFPS(); }