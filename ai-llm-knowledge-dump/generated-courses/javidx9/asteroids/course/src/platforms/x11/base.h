#define _POSIX_C_SOURCE 200809L

#ifndef PLATFORMS_X11_BASE_H
#define PLATFORMS_X11_BASE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/base.h — X11 Backend for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * X11State holds all X11/GLX windowing state in a single struct.
 * The global `g_x11` is declared here and defined in main.c.
 *
 * Fields:
 *   display          — X11 server connection (XOpenDisplay)
 *   window           — application window (XCreateWindow)
 *   gl_context       — OpenGL rendering context (glXCreateContext)
 *   screen           — default screen number
 *   window_w/h       — current window size (updated on ConfigureNotify)
 *   wm_delete_window — atom for close-button events
 *   vsync_enabled    — true if GLX swap control succeeded
 *   texture_id       — GPU texture for backbuffer upload (glTexSubImage2D)
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdbool.h>
#include <stdint.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

typedef struct {
  Display    *display;          /* X11 server connection (XOpenDisplay).    */
  Window      window;           /* The application window (XCreateWindow).  */
  GLXContext  gl_context;       /* OpenGL rendering context (glXCreateContext). */
  int         screen;           /* Default screen number.                   */

  int         window_w;         /* Current window width  (updated on ConfigureNotify). */
  int         window_h;         /* Current window height (updated on ConfigureNotify). */

  Atom        wm_delete_window; /* WM_DELETE_WINDOW atom (close-button event). */
  bool        vsync_enabled;    /* true if GLX_EXT/MESA swap control succeeded. */
  GLuint      texture_id;       /* GPU texture for the backbuffer upload.   */
} X11State;

extern X11State g_x11;

#endif /* PLATFORMS_X11_BASE_H */
