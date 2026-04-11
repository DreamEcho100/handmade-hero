#define _POSIX_C_SOURCE 200809L
#ifndef PLATFORMS_X11_BASE_H
#define PLATFORMS_X11_BASE_H

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  Display *display;
  Window window;
  GLXContext gl_context;
  int screen;
  int window_w;
  int window_h;
  Atom wm_delete_window;
  bool vsync_enabled;
  GLuint texture_id;
} X11State;

extern X11State g_x11;
#endif
