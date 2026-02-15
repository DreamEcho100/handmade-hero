#ifndef DE100_HOOKS_MAIN_H
#define DE100_HOOKS_MAIN_H

#include "../../../_common/base.h"

// ═══════════════════════════════════════════════════════════════════════
// PLATFORM HOOKS - Main Loop Timing
// ═══════════════════════════════════════════════════════════════════════

void de100_set_target_fps(u32 fps);
f32 de100_get_frame_time(void);
f64 de100_get_time(void);
u32 de100_get_fps(void);

#endif // DE100_HOOKS_MAIN_H
