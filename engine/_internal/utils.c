#include "utils.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>

#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#else
// Unsupported OS – no includes
#endif

u32 get_monitor_refresh_hz(void) {
#if defined(_WIN32)

  HDC hdc = GetDC(NULL);
  int hz;

  if (!hdc)
    return 0;

  hz = GetDeviceCaps(hdc, VREFRESH);
  ReleaseDC(NULL, hdc);

  return (hz > 0) ? hz : 0;

#elif defined(__APPLE__)

  CGDisplayModeRef mode;
  double hz;

  mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
  if (!mode)
    return 0;

  hz = CGDisplayModeGetRefreshRate(mode);
  CGDisplayModeRelease(mode);

  /* ProMotion / VRR displays return 0 */
  return (hz > 0.0) ? (int)(hz + 0.5) : 0;

#elif defined(__linux__)

  Display *dpy;
  Window root;
  XRRScreenConfiguration *conf;
  short rate;

  dpy = XOpenDisplay(NULL);
  if (!dpy)
    return 0;

  root = DefaultRootWindow(dpy);
  conf = XRRGetScreenInfo(dpy, root);
  if (!conf) {
    XCloseDisplay(dpy);
    return 0;
  }

  rate = XRRConfigCurrentRate(conf);

  XRRFreeScreenConfigInfo(conf);
  XCloseDisplay(dpy);

  return (rate > 0) ? rate : 0;

#else

  /* BSD, Wayland-only, Android, iOS, WASM, etc. */
  return 0;

#endif
}