#pragma once

#if defined(USE_X11)
#include "x11_backend.c"

#elif defined(USE_SDL)
#include "sdl_backend.c"

#elif defined(USE_RAYLIB)
#include "raylib_backend.c"

#else
#error "No backend selected!"
#endif
