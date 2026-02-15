#include "base.h"

bool is_game_running = true;
bool g_game_is_paused = false;
// bool g_window_is_active = true;
bool g_reload_requested = false;

f64 g_initial_game_time_ms = 0.0;

u32 g_frame_counter = 0;
u32 g_fps = 0;
