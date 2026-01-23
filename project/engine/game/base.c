#include "base.h"

bool is_game_running = true;
bool g_game_is_paused = false;
bool g_reload_requested = false;
uint32_t g_frame_counter = 0;
uint32_t g_fps = 0;
uint32_t g_reload_check_interval = 0;
uint32_t g_reload_check_counter = 0;