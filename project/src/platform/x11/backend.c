#define _POSIX_C_SOURCE 199309L // Enable POSIX functions like nanosleep, sleep
#include "../base.h"
#include "audio.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>    // For sleep()
#include <x86intrin.h> // for __rdtsc() (CPU cycle counter)

// ═══════════════════════════════════════════════════════════════
// 🎮 JOYSTICK DYNAMIC LOADING (Casey's Pattern for Linux)
// ═══════════════════════════════════════════════════════════════
// STEP 1: Define function signature macro
#define LINUX_JOYSTICK_READ(name) ssize_t name(int fd, struct js_event *event)

// STEP 2: Create typedef
typedef LINUX_JOYSTICK_READ(linux_joystick_read);

// STEP 3: Stub implementation (no joystick available)
LINUX_JOYSTICK_READ(LinuxJoystickReadStub) {
  // Return -1 = error/no device (like ERROR_DEVICE_NOT_CONNECTED)
  return -1;
}

// STEP 4: Global function pointer (initially stub)
file_scoped_global_var linux_joystick_read *LinuxJoystickRead_ =
    LinuxJoystickReadStub;

// STEP 5: Redefine API name
#define LinuxJoystickRead LinuxJoystickRead_

/**
 * GLOBAL STATE
 *
 * In C, we often use global variables for things that need to persist
 * across function calls. Think of these like module-level variables in JS.
 *
 * Casey uses globals to avoid passing everything as parameters.
 * This is a design choice - simpler but less "pure".
 */

typedef struct {
  XImage *info; // X11 image wrapper
  void *memory; // Raw pixel memory (our canvas!)
  int width;    // Current buffer dimensions
  int height;
  int pitch;
  int bytes_per_pixel;
} OffscreenBuffer;

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;
typedef struct {
  int offset_x;
  int offset_y;
} PixelState;
// Button states (mirrors Casey's XInput button layout)
typedef struct {
  bool up;
  bool down;
  bool left;
  bool right;

  bool start;
  bool back;
  bool a_button;
  bool b_button;
  bool x_button;
  bool y_button;
  bool left_shoulder;
  bool right_shoulder;

  // Analog sticks (16-bit signed, like XInput)
  int16_t left_stick_x;
  int16_t left_stick_y;
  int16_t right_stick_x;
  int16_t right_stick_y;
  int16_t left_trigger;
  int16_t right_trigger;
} GameControls;

typedef struct {
  GameControls controls;
  GradientState gradient;
  PixelState pixel;
  int speed;
  int gamepad_id;
  bool is_running;
} GameState;

file_scoped_global_var GameState g_game_state = {0}; // Zero-initialized struct

/*
Will be added when needed
typedef struct {
    int width;
    int height;
} X11WindowDimension;

file_scoped_fn X11WindowDimension
get_window_dimension(Display *display, Window window) {
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);
    return (X11WindowDimension){attrs.width, attrs.height};
}
*/

file_scoped_global_var OffscreenBuffer g_backbuffer;

// Real implementation (only used if joystick found)
file_scoped_fn LINUX_JOYSTICK_READ(linux_joystick_read_impl) {
  // This is what actually reads from /dev/input/js*
  return read(fd, event, sizeof(*event));
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Initialize joystick (Casey's Win32LoadXInput equivalent)
// ═══════════════════════════════════════════════════════════════
// Dynamic loader
file_scoped_fn bool linux_init_joystick(GameState *game_state) {
  printf("Searching for gamepad...\n");

  const char *device_paths[] = {"/dev/input/js0", "/dev/input/js1",
                                "/dev/input/js2", "/dev/input/js3"};

  for (int i = 0; i < 4; i++) {
    int fd = open(device_paths[i], O_RDONLY | O_NONBLOCK);

    if (fd >= 0) {
      char name[128] = {0};
      if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) >= 0) {

        // Skip virtual devices
        if (strstr(name, "virtual") || strstr(name, "keyd")) {
          close(fd);
          continue;
        }

        // Found real gamepad! Replace stub with real function
        game_state->gamepad_id = fd;

        // Create wrapper function that uses our fd
        // (Linux doesn't have DLLs, but we can still use the pattern!)
        LinuxJoystickRead_ = linux_joystick_read_impl; // Real implementation

        printf("✅ Joystick connected: %s\n", name);
        return true;
      }
      close(fd);
    }
  }

  // No joystick found - LinuxJoystickRead_ stays pointing to stub
  printf("❌ No gamepad found - using stub\n");
  game_state->gamepad_id = -1;
  return false;
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Poll joystick state (Casey's XInputGetState equivalent)
// ═══════════════════════════════════════════════════════════════

file_scoped_fn void linux_poll_joystick(GameState *game_state) {
  if (game_state->gamepad_id < 0) {
    return;
  }

  struct js_event event;

  while (LinuxJoystickRead(game_state->gamepad_id, &event) == sizeof(event)) {

    if (event.type & JS_EVENT_INIT) {
      continue;
    }

    // NOTE: Ignore the joystick for now since I don't have them

    // ═══════════════════════════════════════════════════════════
    // 🎮 BUTTON EVENTS
    // ═══════════════════════════════════════════════════════════
    if (event.type == JS_EVENT_BUTTON) {
      bool is_pressed = (event.value != 0);

      // ───────────────────────────────────────────────────────
      // PS4/PS5 "Wireless Controller" Button Mapping
      // ───────────────────────────────────────────────────────
      // Button layout (PlayStation naming):
      //   0 = X (cross)     - South button
      //   1 = O (circle)    - East button
      //   3 = □ (square)    - West button
      //   2 = △ (triangle)  - North button
      //   4 = L1 (left bumper)
      //   5 = R1 (right bumper)
      //   6 = L2 (left trigger button, not analog value!)
      //   7 = R2 (right trigger button, not analog value!)
      //   8 = Share (PS4) / Create (PS5)
      //   9 = Options (PS4/PS5)
      //  10 = L3 (left stick button)
      //  11 = R3 (right stick button)
      //  12 = PS button (PlayStation logo)
      //  13 = Touchpad button (PS4/PS5)
      // ───────────────────────────────────────────────────────

      switch (event.number) {
      // Face buttons (cross, circle, square, triangle)
      case 0: // X (cross) - Map to A button
        game_state->controls.a_button = is_pressed;
        if (is_pressed)
          printf("Button X (cross) pressed\n");
        break;

      case 1: // O (circle) - Map to B button
        game_state->controls.b_button = is_pressed;
        if (is_pressed)
          printf("Button O (circle) pressed\n");
        break;

      case 3: // □ (square) - Map to X button
        game_state->controls.x_button = is_pressed;
        if (is_pressed)
          printf("Button □ (square) pressed\n");
        break;

      case 2: // △ (triangle) - Map to Y button
        game_state->controls.y_button = is_pressed;
        if (is_pressed)
          printf("Button △ (triangle) pressed\n");
        break;

      // Shoulder buttons
      case 4: // L1
        game_state->controls.left_shoulder = is_pressed;
        if (is_pressed)
          printf("Button L1 pressed\n");
        break;

      case 5: // R1
        game_state->controls.right_shoulder = is_pressed;
        if (is_pressed)
          printf("Button R1 pressed\n");
        break;

      // Trigger buttons (L2/R2 as digital buttons)
      // Note: Analog values are on axes 3 and 4
      case 6: // L2 button
        if (is_pressed)
          printf("Button L2 pressed\n");
        break;

      case 7: // R2 button
        if (is_pressed)
          printf("Button R2 pressed\n");
        break;

      // Menu buttons
      case 8: // Share/Create - Map to Back
        game_state->controls.back = is_pressed;
        if (is_pressed)
          printf("Button Share/Create pressed\n");
        break;

      case 9: // Options - Map to Start
        game_state->controls.start = is_pressed;
        if (is_pressed)
          printf("Button Options pressed\n");
        break;

      // Stick buttons
      case 11: // L3 (left stick click)
        if (is_pressed)
          printf("Button L3 (left stick) pressed\n");
        break;

      case 12: // R3 (right stick click)
        if (is_pressed)
          printf("Button R3 (right stick) pressed\n");
        break;

      // Special buttons
      case 10: // PS button
        if (is_pressed)
          printf("Button PS (logo) pressed\n");
        break;

      case 13: // Touchpad button
        if (is_pressed)
          printf("Button Touchpad pressed\n");
        break;

      default:
        // Unknown button
        if (is_pressed) {
          printf("Unknown button %d pressed\n", event.number);
        }
        break;
      }
    }

    // ═══════════════════════════════════════════════════════════
    // 🎮 AXIS EVENTS (Analog Sticks + Triggers + D-Pad)
    // ═══════════════════════════════════════════════════════════
    else if (event.type == JS_EVENT_AXIS) {

      // ───────────────────────────────────────────────────────
      // PS4/PS5 "Wireless Controller" Axis Mapping
      // ───────────────────────────────────────────────────────
      // Axes:
      //   0 = Left stick X    (-32767 left, +32767 right)
      //   1 = Left stick Y    (-32767 up,   +32767 down)
      //   2 = Right stick X   (-32767 left, +32767 right)
      //   3 = L2 trigger      (0 released, +32767 pressed)
      //   4 = R2 trigger      (0 released, +32767 pressed)
      //   5 = Right stick Y   (-32767 up,   +32767 down)
      //   6 = D-pad X         (-32767 left, +32767 right)
      //   7 = D-pad Y         (-32767 up,   +32767 down)
      // ───────────────────────────────────────────────────────

      switch (event.number) {
      // ───────────────────────────────────────────────────────
      // Left Stick (axes 0-1)
      // ───────────────────────────────────────────────────────
      case 0:
        game_state->controls.left_stick_x = event.value;
        printf("Left Stick X\n");
        break;
      case 1:
        game_state->controls.left_stick_y = event.value;
        printf("Left Stick Y\n");
        break;

      // ───────────────────────────────────────────────────────
      // Right Stick (axes 2, 5) ← NOTE: Y is axis 5, not 3!
      // ───────────────────────────────────────────────────────
      case 3: // L2 trigger X
        printf("Right Stick X\n");
        // Optional: Store trigger pressure if you need it
        game_state->controls.right_stick_x = event.value;
        break;
      case 4: // R2 trigger Y
        printf("Right Stick Y\n");
        game_state->controls.right_stick_y = event.value;
        break;

      // ───────────────────────────────────────────────────────
      // Triggers (analog, 0 to +32767)
      // ───────────────────────────────────────────────────────
      case 2:
        game_state->controls.left_trigger = event.value;
        printf("L2 trigger\n");
        break;
      case 5: // ← Right stick Y is axis 5!
        game_state->controls.right_trigger = event.value;
        printf("R2 trigger\n");
        break;

      // ───────────────────────────────────────────────────────
      // D-PAD (axes 6-7)
      // ───────────────────────────────────────────────────────
      case 6: { // D-pad horizontal
        if (event.value < -16384) {
          game_state->controls.left = true;
          game_state->controls.right = false;
          printf("D-pad LEFT\n");
        } else if (event.value > 16384) {
          game_state->controls.right = true;
          game_state->controls.left = false;
          printf("D-pad RIGHT\n");
        } else {
          game_state->controls.left = false;
          game_state->controls.right = false;
        }
        break;
      }

      case 7: { // D-pad vertical
        if (event.value < -16384) {
          game_state->controls.up = true;
          game_state->controls.down = false;
          printf("D-pad UP\n");
        } else if (event.value > 16384) {
          game_state->controls.down = true;
          game_state->controls.up = false;
          printf("D-pad DOWN\n");
        } else {
          game_state->controls.up = false;
          game_state->controls.down = false;
        }
        break;
      }
      default:
        printf("D-pad number: %d, value: %d\n", event.number, event.value);
      }
    }
  }
}

// NEW: Helper function to change frequency
inline void set_tone_frequency(int hz) {
  g_sound_output.tone_hz = (int)hz;
  g_sound_output.wave_period =
      g_sound_output.samples_per_second / g_sound_output.tone_hz;
  // g_sound_output.half_wave_period = g_sound_output.wave_period / 2;

  // Optional: reset phase to avoid clicks
  g_sound_output.running_sample_index = 0;
}

// Fun test
inline file_scoped_fn void handle_update_tone_frequency(int hz_to_add) {
  int new_hz = g_sound_output.tone_hz + hz_to_add;
  // Clamp to safe range
  if (new_hz < 60)
    new_hz = 60;
  if (new_hz > 1000)
    new_hz = 1000;

  set_tone_frequency((float)new_hz);

  printf("🎵 Tone frequency: %d Hz (period: %d samples)\n", new_hz,
         g_sound_output.wave_period);
}

inline file_scoped_fn void handle_increase_volume(int num) {
  int new_volume = g_sound_output.tone_volume + num;

  // Clamp to safe range
  if (new_volume < 0)
    new_volume = 0;
  if (new_volume > 15000)
    new_volume = 15000;

  g_sound_output.tone_volume = new_volume;
  printf("🔊 Volume: %d / %d (%.1f%%)\n", new_volume, 15000,
         (new_volume * 100.0f) / 15000);
}

// Fun test
inline file_scoped_fn void handleMusicalkeypress(KeySym keysym) {
  switch (keysym) {
  case XK_F1: {
    printf("F1 pressed - showing audio debug\n");
    linux_debug_audio_latency();
    break;
  }
  case XK_z:
    set_tone_frequency((int)261.63f);
    printf("🎵 Note: C4 (261.63 Hz)\n");
    break;
  case XK_x:
    set_tone_frequency((int)293.66f);
    printf("🎵 Note: D4 (293.66 Hz)\n");
    break;
  case XK_c:
    set_tone_frequency((int)329.63f);
    printf("🎵 Note: E4 (329.63 Hz)\n");
    break;
  case XK_v:
    set_tone_frequency((int)349.23f);
    printf("🎵 Note: F4 (349.23 Hz)\n");
    break;
  case XK_b:
    set_tone_frequency((int)392.00f);
    printf("🎵 Note: G4 (392.00 Hz)\n");
    break;
  case XK_n:
    set_tone_frequency((int)440.00f);
    printf("🎵 Note: A4 (440.00 Hz) - Concert Pitch\n");
    break;
  case XK_m:
    set_tone_frequency((int)493.88f);
    printf("🎵 Note: B4 (493.88 Hz)\n");
    break;
  case XK_comma:
    set_tone_frequency((int)523.25f);
    printf("🎵 Note: C5 (523.25 Hz)\n");
    break;
  }
}

// Fun test

/**
 * **Linear vs Equal-Power Panning:**
 *
 *
 * ```
 * Linear Panning (simple):
 *
 * ────────────────────────────
 *
 * Pan center: both channels at 50%
 * Problem: Sounds QUIETER in center (50% + 50% ≠ 100% perceived volume)
 *
 * Math: L = (100-pan)/200, R = (100+pan)/200
 *
 *
 * Equal-Power Panning (better):
 *
 * ─────────────────────────────
 *
 * Pan center: both channels at 70.7% (√2/2)
 * Result: Constant perceived loudness across pan range
 *
 *
 * Math: L = cos(angle), R = sin(angle)
 *       where angle = (pan+1) * π/4
 * ```
 *
 *
 * **Why the "center dip" happens with linear:**
 * ```
 * Human hearing perceives power, not amplitude
 * Power ∝ amplitude²
 *
 * Linear at center:
 *     L = 0.5, R = 0.5
 *     Total power = 0.5² + 0.5² = 0.25 + 0.25 = 0.5  ← Only 50%!
 *
 * Equal-power at center:
 *     L = 0.707, R = 0.707
 *     Total power = 0.707² + 0.707² = 0.5 + 0.5 = 1.0  ← Full 100%!
 * ```
 *
 *
 * **Visual comparison:**
 * ```
 * Linear Pan (has dip):
 * Perceived  │     ╱╲
 * Loudness   │   ╱    ╲
 *            │ ╱        ╲
 *            └──────────────
 *            L    C      R
 *
 * Equal-Power (flat):
 * Perceived  │ ──────────
 * Loudness   │
 *            │
 *            └──────────────
 *            L    C      R
 * ```
 *
 */
inline file_scoped_fn void handle_increase_pan(int num) {
  int new_pan = g_sound_output.pan_position + num;
  if (new_pan < -100)
    new_pan = -100;
  if (new_pan > 100)
    new_pan = 100;

  g_sound_output.pan_position = new_pan;

  // Visual indicator
  char indicator[50] = {0};
  int pos =
      (g_sound_output.pan_position + 100) * 20 / 200; // Map -100..100 to 0..20
  for (int i = 0; i < 21; i++) {
    indicator[i] = (i == pos) ? '*' : '-';
  }
  indicator[21] = '\0';

  printf("🎧 Pan: %s %+d\n", indicator, g_sound_output.pan_position);
  printf("    L ◀%s▶ R\n", indicator);
}

file_scoped_fn void linux_handle_controls(GameState *game_state) {

  if (game_state->controls.up) {
    game_state->gradient.offset_y += game_state->speed;
  }
  if (game_state->controls.left) {
    game_state->gradient.offset_x += game_state->speed;
  }
  if (game_state->controls.down) {
    game_state->gradient.offset_y -= game_state->speed;
  }
  if (game_state->controls.right) {
    game_state->gradient.offset_x -= game_state->speed;
  }

  if (game_state->gamepad_id >= 0) {
    // Same math as Casey! (>> 12 = divide by 4096)
    // This converts -32767..+32767 to about -8..+8 pixels/frame
    int normalized_left_stick_x = game_state->controls.left_stick_x / 4096;
    int normalized_left_stick_y = game_state->controls.left_stick_y / 4096;

    game_state->gradient.offset_x -= normalized_left_stick_x;
    game_state->gradient.offset_y -= normalized_left_stick_y;

    // set_tone_frequency(
    //     512 +
    //     (int)(256.0f * ((real32)game_state->controls.left_stick_y /
    //     30000.0f)));
    if (normalized_left_stick_y != 0) {
      handle_update_tone_frequency(normalized_left_stick_y);
    }
    if (normalized_left_stick_x != 0) {
      handle_update_tone_frequency(normalized_left_stick_x);
    }
    // Optional: Start button resets
    if (game_state->controls.start) {
      game_state->gradient.offset_x = 0;
      game_state->gradient.offset_y = 0;
      printf("START pressed - reset offsets\n");
    }
  }
}

file_scoped_fn void render_weird_gradient(OffscreenBuffer *buffer,

                                          GradientState *gradient_state) {
  uint8_t *row = (uint8_t *)buffer->memory;

  for (int y = 0; y < buffer->height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < buffer->width; ++x) {
      uint8_t blue = (x + gradient_state->offset_x);
      uint8_t green = (y + gradient_state->offset_y);

      *pixels++ = ((green << 8) | blue);
    }
    row += buffer->pitch;
  }
}

/**
 * RESIZE BACK BUFFER
 *
 * Allocates (or reallocates) the pixel buffer when window size changes.
 *
 * Casey's equivalent: Win32ResizeDIBSection()
 *
 * FLOW:
 * 1. Free old buffer if it exists
 * 2. Allocate new pixel memory
 * 3. Create XImage wrapper
 *
 * ═══════════════════════════════════════════════════════════════════════
 * 🌊 CASEY'S "WAVE 2" RESOURCE - STATE LIFETIME
 * ═══════════════════════════════════════════════════════════════════════
 *
 * This buffer is a WAVE 2 resource (state-lifetime, not process-lifetime).
 * It lives ONLY as long as the current window size stays the same.
 *
 * Why we clean up here (unlike process-lifetime resources):
 * - We're REPLACING the buffer with a new one (different size)
 * - This happens DURING program execution (not at exit)
 * - If we don't free, we leak 1-3MB on EVERY resize!
 *
 * Example: User resizes window 10 times:
 * ❌ Without cleanup: 10 buffers × 2MB = 20MB leaked! 💥
 * ✅ With cleanup: Always just 1 buffer = 2MB total ⚡
 *
 * Casey's Rule: "Think about creation and destruction in WAVES.
 *                This buffer changes with window size (state change),
 *                so we manage it when state changes."
 *
 * 🟡 COLD PATH: Only runs on window resize (maybe once per second)
 *    So malloc/free here is totally fine!
 */
inline file_scoped_fn void resize_back_buffer(OffscreenBuffer *buffer,
                                              Display *display, int width,
                                              int height) {

  // Free old buffer if it exists
  // This is WAVE 2 cleanup - we're changing state (window size)!
  //
  // Visual: What happens on resize
  // ┌─────────────────────────────────────────┐
  // │ Before (800×600):                       │
  // │ buffer->info → [1.8 MB of pixels]       │
  // │                                         │
  // │ User resizes to 1920×1080               │
  // │ ↓                                       │
  // │ We MUST free the 1.8 MB buffer!         │
  // │ Otherwise it leaks forever! 💥          │
  // │                                         │
  // │ After cleanup:                          │
  // │ buffer->info → NULL                     │
  // │ buffer->memory → NULL                      │
  // │                                         │
  // │ Now allocate new 8.3 MB buffer          │
  // │ buffer->info → [8.3 MB of pixels] ✅    │
  // └─────────────────────────────────────────┘

  if (buffer->info) {
    // Call XDestroyImage() to free it
    // This ALSO frees buffer->memory automatically!
    // (X11 owns the memory once XCreateImage is called)
    if (buffer->memory) {
      buffer->info->data = NULL; // XDestroyImage should not free
      munmap(buffer->memory, buffer->width * buffer->height * 4);
      buffer->memory = NULL;
    }
    XDestroyImage(buffer->info);

    buffer->info = NULL;
    buffer->memory = NULL;
  }

  // Calculate how much memory we need
  // Each pixel is 4 bytes (RGBA), so:
  // Total bytes = width × height × 4
  buffer->pitch = width * buffer->bytes_per_pixel; // Bytes per row
  int buffer_size = buffer->pitch * height;        // Total bytes

  printf("Allocating back buffer: %dx%d (%d bytes = %.2f MB)\n", width, height,
         buffer_size, buffer_size / (1024.0 * 1024.0));

  // Allocate pixel memory using mmap (Casey-style)
  buffer->memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (buffer->memory == MAP_FAILED) {
    buffer->memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // NOTE: mmap gives you ZEROED pages automatically (like calloc), no memset
  // needed.

  //
  // TODO(Day 25+): Replace with mmap() when building memory system
  // TODO(Day 25+): Add debug mode mprotect() traps for use-after-free
  // TODO(Day 25+): Consider reserve-once-commit-as-needed pattern
  //
  // References:
  // - Casey Day 4:  VirtualAlloc basics
  // - Casey Day 25: Memory system architecture
  // - man mmap(2):  Linux virtual memory API
  // - man mprotect(2): Memory protection changes
  // Create XImage wrapper
  // XImage is like ImageData - it describes the pixel format

  buffer->info = XCreateImage(
      display,                                        // X11 connection
      DefaultVisual(display, DefaultScreen(display)), // Color format
      24,                     // Depth (24-bit RGB, ignore alpha)
      ZPixmap,                // Format (chunky pixels, not planar)
      0,                      // Offset in data
      (char *)buffer->memory, // Our pixel buffer
      width, height,          // Dimensions
      32,                     // Bitmap pad (align to 32-bit boundaries)
      0                       // Bytes per line (0 = auto-calculate)
  );

  // Save the dimensions
  buffer->width = width;
  buffer->height = height;

  printf("Back buffer created successfully\n");
}

/**
 * UPDATE WINDOW (BLIT)
 *
 * Copies pixels from back buffer to screen.
 * "Blit" = BLock Image Transfer = fast pixel copy
 *
 * Casey's equivalent: Win32UpdateWindow() using StretchDIBits()
 *
 * 🔴 HOT PATH: Could be called 60 times/second!
 * XPutImage is hardware-accelerated, so it's fast.
 */
static void update_window(OffscreenBuffer *buffer, Display *display,
                          Window window, GC gc, int x, int y, int width,
                          int height) {
  // Don't blit if no buffer exists!
  if (!buffer->info) {
    printf("WARNING: Tried to blit, but no buffer exists!\n");
    return;
  }

  /*
   * ```
   * Back Buffer (in RAM)          Window (on screen)
   * ┌─────────────────────┐      ┌─────────────────────┐
   * │ [Pixels we drew]    │      │                     │
   * │                     │      │                     │
   * │  800 × 600 pixels   │ ───→ │   Visible to user   │
   * │                     │ XPut │                     │
   * │                     │Image │                     │
   * └─────────────────────┘      └─────────────────────┘
   *    buffer->memory                   The actual window
   * ```
   */
  // Copy pixels from back buffer to window
  // This is THE KEY FUNCTION for double buffering!
  XPutImage(display,      // X11 connection
            window,       // Destination (the actual window)
            gc,           // Graphics context
            buffer->info, // Source (our pixel buffer)
            x, y,         // Source position (which part of buffer)
            x, y,         // Dest position (where on window)
            width, height // How much to copy
  );
}

/**
 * HANDLE WINDOW EVENTS
 *
 * This is like your event handlers in JavaScript:
 * - window.addEventListener('resize', handleResize)
 * - window.addEventListener('close', handleClose)
 *
 * Casey's Windows version has MainWindowCallback() - this is our equivalent.
 *
 * X11 DIFFERENCE:
 * - Windows: OS calls your callback function for each event (push model)
 * - X11: You pull events from a queue with XNextEvent() (pull model)
 *
 * We check the event.type and handle each case, just like:
 * switch(event.type) { case 'click': ..., case 'resize': ... }
 */
inline file_scoped_fn void handle_event(OffscreenBuffer *buffer,
                                        Display *display, Window window, GC gc,
                                        XEvent *event, GameState *game_state) {
  switch (event->type) {

  /**
   * CONFIGURE NOTIFY = WINDOW RESIZED
   *
   * Like window.addEventListener('resize', ...)
   *
   * Fires when window size changes. Casey logs "WM_SIZE" in Windows.
   * We'll just print to console (like console.log())
   */
  case ConfigureNotify: {
    int new_width = event->xconfigure.width;
    int new_height = event->xconfigure.height;
    printf("Window resized to: %dx%d\n", new_width, new_height);
    // /**
    //  * **Why do we resize the buffer here?**
    //  *
    //  * Because the window size changed! Our old buffer is the wrong size. We
    //  * need to allocate a new buffer that matches the new window dimensions.
    //  */
    // // Only resize if dimensions ACTUALLY CHANGED!
    // if (new_width != buffer->width || new_height != buffer->height) {
    //   printf("Window resized: %dx%d → %dx%d\n", buffer->width,
    //   buffer->height,
    //          new_width, new_height);
    //   resize_back_buffer(buffer, display, new_width, new_height);
    // } else {
    //   printf("ConfigureNotify (same size, ignoring)\n");
    // }

    // ═══════════════════════════════════════════════════════════════
    // 🔇 COMMENTED OUT: Day 5 uses fixed buffer, no resize
    // ═══════════════════════════════════════════════════════════════
    // if (new_width != buffer->width || new_height != buffer->height) {
    //     printf("Window resized: %dx%d → %dx%d\n",
    //            buffer->width, buffer->height, new_width, new_height);
    //     resize_back_buffer(buffer, display, new_width, new_height);
    // }

    break;
  }

  /**
   * CLIENT MESSAGE = WINDOW CLOSE BUTTON CLICKED
   *
   * Like window.addEventListener('beforeunload', ...)
   *
   * When user clicks the X button, we receive this event.
   * Casey's WM_CLOSE equivalent.
   *
   * X11 QUIRK: Close isn't automatic - we must check if it's actually
   * the close button message (WM_DELETE_WINDOW protocol)
   */
  case ClientMessage: {
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if ((Atom)event->xclient.data.l[0] == wmDelete) {
      printf("Window close requested\n");
      game_state->is_running = false; // Stop the main loop
    }
    break;
  }

  /**
   * EXPOSE = WINDOW NEEDS REPAINTING
   *
   * Like canvas.redraw() or React re-render
   *
   * Fires when window is uncovered, moved, or resized.
   * Casey's WM_PAINT equivalent.
   *
   * We'll toggle between white and black (Casey uses PatBlt with
   * WHITENESS/BLACKNESS)
   *
   * X11 CONCEPT - Graphics Context (GC):
   * Like setting strokeStyle/fillStyle on a canvas context.
   * GC holds drawing settings (color, line width, etc.)
   */
  case Expose: {
    // Only process the last expose event (count == 0)
    // X11 can send multiple expose events for different regions
    if (event->xexpose.count != 0)
      break;
    printf("Repainting window");
    update_window(buffer, display, window, gc, 0, 0, buffer->width,
                  buffer->height);
    break;
  }

  /**
   * FOCUS IN = WINDOW GAINED FOCUS
   *
   * Like window.addEventListener('focus', ...)
   *
   * Casey's WM_ACTIVATEAPP equivalent.
   */
  case FocusIn: {
    printf("Window gained focus\n");
    break;
  }

  /**
   * DESTROY NOTIFY = WINDOW DESTROYED
   *
   * Like window being removed from DOM
   *
   * Casey's WM_DESTROY equivalent.
   * This means the window is being destroyed by the window manager.
   */
  case DestroyNotify: {
    printf("Window destroyed\n");
    game_state->is_running = false;
    break;
  }

  /**
   * KEY PRESS = KEYBOARD KEY PRESSED DOWN
   *
   * Casey's WM_KEYDOWN equivalent
   */
  case KeyPress: {
    KeySym key = XLookupKeysym(&event->xkey, 0);
    printf("pressed\n");

    handleMusicalkeypress(key);

    switch (key) {
    case XK_braceleft: // Sometimes not detected!{ // [ key (Shift for {)
    {
      printf("{ or [ pressed\n");
      handle_increase_volume(-500);
      break;
    }
    case XK_bracketleft: // [ key (Shift for {)
    {
      if (event->xkey.state & ShiftMask) {
        printf("{ pressed (Shift + [)\n");
        handle_increase_volume(-500);
      } else {
        handle_increase_pan(-10);
      }
      break;
    }
    case XK_braceright: // Sometimes not detected!{ // ] key (Shift for })
    {
      printf("} or ] pressed\n");
      handle_increase_volume(500);
      break;
    }
    case XK_bracketright: // [ key (Shift for {)
    {
      if (event->xkey.state & ShiftMask) {
        printf("{ pressed (Shift + ])\n");
        handle_increase_volume(500);
      } else {
        handle_increase_pan(10);
      }
      break;
    }
    case (XK_w):
    case (XK_W):
    case (XK_Up): {
      printf("W pressed\n");
      game_state->controls.up = true;
      break;
    }
    case (XK_a):
    case (XK_A):
    case (XK_Left): {
      printf("A pressed\n");
      game_state->controls.left = true;
      break;
    }
    case (XK_s):
    case (XK_S):
    case (XK_Down): {
      printf("S pressed\n");
      game_state->controls.down = true;
      break;
    }
    case (XK_d):
    case (XK_D):
    case (XK_Right): {
      printf("D pressed\n");
      game_state->controls.right = true;
      break;
    }
    case (XK_space): {
      printf("SPACE pressed\n");
      break;
    }
    case (XK_Escape): {
      printf("ESCAPE pressed - exiting\n");
      game_state->is_running = false;
      break;
    }
    }

    break;
  }

  /**
   * KEY RELEASE = KEYBOARD KEY RELEASED
   *
   * Casey's WM_KEYUP equivalent
   */
  case KeyRelease: {
    KeySym key = XLookupKeysym(&event->xkey, 0);

    switch (key) {
    case (XK_w):
    case (XK_W):
    case (XK_Up): {
      printf("W released\n");
      game_state->controls.up = false;
      break;
    }
    case (XK_a):
    case (XK_A):
    case (XK_Left): {
      printf("A released\n");
      game_state->controls.left = false;
      break;
    }
    case (XK_s):
    case (XK_S):
    case (XK_Down): {
      printf("S released\n");
      game_state->controls.down = false;
      break;
    }
    case (XK_d):
    case (XK_D):
    case (XK_Right): {
      printf("D released\n");
      game_state->controls.right = false;
      break;
    }
    case (XK_space): {
      printf("SPACE released\n");
      break;
    }
    case (XK_Escape): {
      printf("ESCAPE released - exiting\n");
      game_state->is_running = false;
      break;
    }
    }

    break;
  }

  /**
   * DEFAULT CASE
   *
   * Casey has DefWindowProc() for unhandled messages.
   * In X11, we just ignore events we don't care about.
   * X11 doesn't need a "default handler" like Windows.
   */
  default: {
    // Uncomment to see all other events (lots of noise!)
    // printf("Unhandled event type: %d\n", event->type);
    break;
  }
  }
}

/**
 * MAIN FUNCTION
 *
 * Casey's WinMain equivalent. This is the entry point of our program.
 *
 * FLOW:
 * 1. Connect to X server (like connecting to a WebSocket server)
 * 2. Create a window (like document.createElement())
 * 3. Set up event handling (like addEventListener())
 * 4. Run event loop (like while(true) in a game loop)
 * 5. Clean up (like componentWillUnmount in React)
 */
int platform_main() {
  // ═══════════════════════════════════════════════════════════
  // 🎮 Initialize joystick BEFORE main loop (Casey's pattern)
  // ═══════════════════════════════════════════════════════════
  linux_init_joystick(&g_game_state);
  // ═══════════════════════════════════════════════════════════
  // 🔊 Load ALSA library (Casey's Win32LoadXInput pattern)
  // ═══════════════════════════════════════════════════════════
  // This MUST come before linux_init_sound()!
  // Just like Casey calls Win32LoadXInput() before using XInput.
  linux_load_alsa();

  // ═══════════════════════════════════════════════════════════
  // 🔊 Initialize sound (Casey's Win32InitDSound equivalent)
  // ═══════════════════════════════════════════════════════════
  //
  // Casey's Day 7 call:
  //   Win32InitDSound(Window, 48000, 48000*sizeof(int16)*2);
  //
  // Parameters breakdown:
  //   48000 = samples per second (CD quality is 44100, we use higher)
  //   48000 * sizeof(int16_t) * 2 = 1 second of stereo 16-bit audio
  //                                = 48000 * 2 * 2 = 192,000 bytes
  //
  // NOTE: This is a SECONDARY buffer size (where we write audio).
  //       The PRIMARY buffer just sets the format.
  // ═══════════════════════════════════════════════════════════
  int samples_per_second = 48000;
  int bytes_per_sample = sizeof(int16_t) * 2; // 16-bit stereo
  int secondary_buffer_size = samples_per_second * bytes_per_sample;
  linux_init_sound(samples_per_second, secondary_buffer_size);

  /**
   * CONNECT TO X SERVER
   *
   * XOpenDisplay(NULL) connects to the default display.
   * Display is like a connection to the windowing system.
   *
   * WEB ANALOGY: Like opening a WebSocket connection to a server
   *
   * NULL means "use the DISPLAY environment variable"
   * (usually ":0" for the first monitor)
   */
  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "ERROR: Cannot connect to X server\n");
    return 1;
  }
  printf("Connected to X server\n");

  /**
   * GET SCREEN INFO
   *
   * X11 supports multiple screens (monitors).
   * We'll use the default screen.
   *
   * Like getting window.screen.width/height in JavaScript
   */
  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);

  // g_game_state.controls = {0};
  // g_game_state.gradient = {0};
  // g_game_state.speed = 1;

  g_game_state.controls = (GameControls){0};
  g_game_state.gradient = (GradientState){0};
  g_game_state.pixel = (PixelState){0};
  g_game_state.speed = 1;
  g_game_state.is_running = true;

  g_backbuffer.info = NULL;
  // g_backbuffer.memory = NULL;
  g_backbuffer.width = 1280;
  g_backbuffer.height = 720;
  g_backbuffer.bytes_per_pixel = 4;
  g_backbuffer.pitch = g_backbuffer.width * g_backbuffer.bytes_per_pixel;
  int initial_buffer_size = g_backbuffer.pitch * g_backbuffer.height;
  g_backbuffer.memory = mmap(NULL, initial_buffer_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (g_backbuffer.memory == MAP_FAILED) {
    g_backbuffer.memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n",
            initial_buffer_size);
    return 1;
  }

  g_backbuffer.info = XCreateImage(
      display,                                        // X11 connection
      DefaultVisual(display, DefaultScreen(display)), // Color format
      24,                          // Depth (24-bit RGB, ignore alpha)
      ZPixmap,                     // Format (chunky pixels, not planar)
      0,                           // Offset in data
      (char *)g_backbuffer.memory, // Our pixel buffer
      g_backbuffer.width, g_backbuffer.height, // Dimensions
      32, // Bitmap pad (align to 32-bit boundaries)
      0   // Bytes per line (0 = auto-calculate)
  );

  /**
   * CREATE THE WINDOW
   *
   * This is like:
   * const div = document.createElement('div');
   * div.style.width = '800px';
   * div.style.height = '600px';
   * document.body.appendChild(div);
   *
   * XCreateSimpleWindow parameters:
   * - display: Our connection to X server
   * - root: Parent window (desktop)
   * - x, y: Position (0, 0 = top-left)
   * - width, height: Size in pixels
   * - border_width: Border size
   * - border: Border color
   * - background: Background color
   *
   * Casey uses CreateWindowExA() in Windows with WS_OVERLAPPEDWINDOW
   */
  Window window = XCreateSimpleWindow(
      display,                                 //
      root,                                    //
      0, 0,                                    // x, y position
      g_backbuffer.width, g_backbuffer.height, // width, height
      1,                                       // border width
      BlackPixel(display, screen),             // border color
      WhitePixel(display, screen)              // background color
  );
  printf("Created window\n");

  /**
   * SET WINDOW TITLE
   *
   * Like document.title = "Handmade Hero"
   *
   * Casey sets this in CreateWindowExA() as the window name parameter
   */
  XStoreName(display, window, "Handmade Hero");

  /**
   * REGISTER FOR WINDOW CLOSE EVENT
   *
   * By default, clicking X just closes the window without notifying us.
   * We need to tell X11 we want to handle the close event ourselves.
   *
   * This is like:
   * window.addEventListener('beforeunload', (e) => {
   *   e.preventDefault(); // We handle it ourselves
   * });
   *
   * WM_DELETE_WINDOW is a protocol that says "let me know when user wants
   * to close"
   */
  Atom wmDeleteMsg = XInternAtom(display, "WM_DELETE_WINDOW", false);
  XSetWMProtocols(display, window, &wmDeleteMsg, 1);
  printf("Registered close event handler\n");

  /**
   * SELECT EVENTS WE WANT TO RECEIVE
   *
   * Like calling addEventListener() for specific events.
   * We tell X11 which events we care about.
   *
   * Think of this like:
   * element.addEventListener('click', handler);
   * element.addEventListener('resize', handler);
   * element.addEventListener('focus', handler);
   *
   * Event masks are bit flags (like MB_OK|MB_ICONINFORMATION in Windows)
   */
  XSelectInput(display, window,
               ExposureMask |            // Repaint events (WM_PAINT)
                   StructureNotifyMask | // Resize events (WM_SIZE)
                   FocusChangeMask |     // Focus events (WM_ACTIVATEAPP)
                   KeyPressMask |        // Key press events
                   KeyReleaseMask        // Key release events
  );
  printf("Registered event listeners\n");

  /**
   * SHOW THE WINDOW
   *
   * Like element.style.display = 'block'
   * or element.classList.remove('hidden')
   *
   * Window is created but hidden by default. XMapWindow makes it visible.
   * Casey uses WS_VISIBLE flag to show window immediately.
   */
  XMapWindow(display, window);
  printf("Window shown\n");

  // Create GC (graphics context)
  // Like ctx = canvas.getContext('2d')
  GC gc = XCreateGC(display, window, 0, NULL);

  /**
   * : ALLOCATE INITIAL BACK BUFFER
   *
   * We need to create the back buffer BEFORE entering the event loop
   * so we have something to draw to!
   *
   * Note: ConfigureNotify will fire after XMapWindow, but we also
   * want to draw immediately, so we allocate here.
   */
  resize_back_buffer(&g_backbuffer, display, g_backbuffer.width,
                     g_backbuffer.height);

  int test_offset = 0;

  /**
   * EVENT LOOP (THE HEART OF THE PROGRAM)
   *
   * This is like:
   * while (true) {
   *   const event = await waitForEvent();
   *   handleEvent(event);
   * }
   *
   * Casey's version:
   * for(;;) {
   *   GetMessageA(&Message, ...);
   *   TranslateMessage(&Message);
   *   DispatchMessageA(&Message);
   * }
   *
   * DIFFERENCES:
   * - Windows: GetMessageA() blocks until message arrives (synchronous)
   * - X11: XNextEvent() blocks until event arrives (synchronous)
   *
   * Both are essentially: "Wait for next event, then handle it"
   *
   * This loop runs forever until g_Running becomes false
   * (when user closes the window)
   */
  printf("Entering event loop...\n");

  struct timespec start, end;
  uint64_t start_cycles, end_cycles;

  clock_gettime(CLOCK_MONOTONIC, &start);
  start_cycles = __rdtsc();

  while (g_game_state.is_running) {
    XEvent event;

    // /**
    //  * WAIT FOR NEXT EVENT
    //  *
    //  * XNextEvent() blocks (waits) until an event arrives.
    //  * Like await in JavaScript - execution stops here until event.
    //  *
    //  * When an event arrives, it's stored in the 'event' variable.
    //  */
    // XNextEvent(display, &event);

    while (XPending(display) > 0) {
      XNextEvent(display, &event);
      handle_event(&g_backbuffer, display, window, gc, &event, &g_game_state);
    }
    // ═══════════════════════════════════════════════════════
    // 🎮 Poll joystick (Casey's XInputGetState pattern)
    // ═══════════════════════════════════════════════════════
    linux_poll_joystick(&g_game_state);

    // // ═══════════════════════════════════════════════════════
    // // 🎮 Use joystick input to control gradient
    // // ═══════════════════════════════════════════════════════
    linux_handle_controls(&g_game_state);

    // /**
    //  * HANDLE THE EVENT
    //  *
    //  * This is like calling your event handler function.
    //  * Our handle_event() is like Casey's MainWindowCallback()
    //  */
    // handle_event(display, window, &event);

    // ═══════════════════════════════════════════════════════════════════
    // 🎨 DRAW DIAGONAL LINE (Learning Exercise - Day 3)
    // ═══════════════════════════════════════════════════════════════════
    //
    // This draws a white diagonal line every frame.
    //
    // PIXEL ADDRESSING: offset = y * width + x
    //
    // In Day 4+, we'll do proper rendering with frame timing.
    // For now, this demonstrates how to write pixels to memory.
    //
    if (g_backbuffer.memory) {
      render_weird_gradient(&g_backbuffer, &g_game_state.gradient);

      // Test pixel animation
      uint32_t *pixels = (uint32_t *)g_backbuffer.memory;
      int total_pixels = g_backbuffer.width * g_backbuffer.height;

      test_offset = g_game_state.pixel.offset_y * g_backbuffer.width +
                    g_game_state.pixel.offset_x;

      if (test_offset < total_pixels)
        pixels[test_offset] = 0xFF0000;

      // Move the red dot every frame
      if (g_game_state.pixel.offset_x + 1 < g_backbuffer.width - 1) {
        g_game_state.pixel.offset_x += 1;
      } else {
        g_game_state.pixel.offset_x = 0;
        if (g_game_state.pixel.offset_y + 75 < g_backbuffer.height - 1) {
          g_game_state.pixel.offset_y += 75;
        } else {
          g_game_state.pixel.offset_y = 0;
        }
      }
      // Display the result
      update_window(&g_backbuffer, display, window, gc, 0, 0,
                    g_backbuffer.width, g_backbuffer.height);

      // offset_x++;

      clock_gettime(CLOCK_MONOTONIC, &end);
      end_cycles = __rdtsc();

      double ms_per_frame = (end.tv_sec - start.tv_sec) * 1000.0 +
                            (end.tv_nsec - start.tv_nsec) / 1000000.0;
      double fps = 1000.0 / ms_per_frame;
      double mcpf = (end_cycles - start_cycles) / 1000000.0;

      printf("%.2fms/f, %.2ff/s, %.2fmc/f\n", ms_per_frame, fps, mcpf);

      start = end;
      start_cycles = end_cycles;
    }

    // ═══════════════════════════════════════════════════════════
    // Day 8: Fill and write audio buffer every frame
    // ═══════════════════════════════════════════════════════════
    //
    // Casey calls this in the main loop after rendering.
    // We generate samples and write them to ALSA.
    //
    // NOTE: ALSA handles playback automatically once we start
    // writing. No explicit "Play()" call needed!
    // ═══════════════════════════════════════════════════════════
    linux_fill_sound_buffer();
  }

  /**
   * CLEANUP - CASEY'S "RESOURCE LIFETIMES IN WAVES" PHILOSOPHY
   *
   * ═══════════════════════════════════════════════════════════════════════
   * IMPORTANT: Read Casey's Day 3 explanation about resource management!
   * ═══════════════════════════════════════════════════════════════════════
   *
   * Casey's Rule: "Don't be myopic about individual resource cleanup.
   *                Think in WAVES based on resource LIFETIME!"
   *
   * WAVE CLASSIFICATION FOR OUR RESOURCES:
   *
   * ┌────────────────────────────────────────────────────────────────┐
   * │ WAVE 1: Process Lifetime (Lives entire program)               │
   * │ ────────────────────────────────────────────                  │
   * │ - Display (X11 connection)                                     │
   * │ - Window                                                       │
   * │                                                                │
   * │ ✅ Casey says: DON'T manually clean these up!                  │
   * │    The OS does it INSTANTLY when process exits (<1ms)          │
   * │                                                                │
   * │ ❌ Bad (OOP way): Manually clean each resource (17ms wasted)   │
   * │    XDestroyImage(backBuffer);   // 2ms                         │
   * │    XDestroyWindow(window);      // 5ms                         │
   * │    XCloseDisplay(display);      // 10ms                        │
   * │                                                                │
   * │ ✅ Good (Casey's way): Just exit! (<1ms)                       │
   * │    return 0;  // OS bulk-cleans ALL resources instantly!       │
   * └────────────────────────────────────────────────────────────────┘
   *
   * ┌────────────────────────────────────────────────────────────────┐
   * │ WAVE 2: State Lifetime (Changes during program)               │
   * │ ────────────────────────────────────────────                  │
   * │ - g_BackBuffer (per window size)                              │
   * │                                                                │
   * │ ✅ Casey says: Clean up WHEN STATE CHANGES (in batches)        │
   * │    We DO clean this in resize_back_buffer() because:          │
   * │    1. We're REPLACING it with a new buffer                     │
   * │    2. This happens DURING program execution                    │
   * │    3. If we don't free, we leak memory on every resize!        │
   * │                                                                │
   * │ This is CORRECT Wave 2 management! ✅                          │
   * └────────────────────────────────────────────────────────────────┘
   *
   * REAL-WORLD IMPACT:
   *
   * Without manual cleanup (Casey's way):
   * ┌─────────────────────────────────────────┐
   * │ User clicks close button                │
   * │ ↓                                       │
   * │ return 0;  // Program exits             │
   * │ ↓                                       │
   * │ OS: "Process died, bulk cleanup!"       │
   * │   - Frees ALL memory in one operation   │
   * │   - Closes ALL X11 resources at once    │
   * │   - Destroys ALL windows instantly      │
   * │ ↓                                       │
   * │ Window closes in <1ms ⚡                 │
   * │ User: "Wow, instant close!" 😊          │
   * └─────────────────────────────────────────┘
   *
   * With manual cleanup (OOP way):
   * ┌─────────────────────────────────────────┐
   * │ User clicks close button                │
   * │ ↓                                       │
   * │ XDestroyImage()... wait 2ms             │
   * │ XDestroyWindow()... wait 5ms            │
   * │ XCloseDisplay()... wait 10ms            │
   * │ ↓                                       │
   * │ Window closes in 17ms 🐌                │
   * │ User: "Why is it laggy?" 😤             │
   * └─────────────────────────────────────────┘
   *
   * CASEY'S QUOTE (Day 3, ~9:20):
   * "If we actually put in code that closes our window before we exit,
   *  we are WASTING THE USER'S TIME. When you exit, Windows will bulk
   *  clean up all of our Windows, all of our handles, all of our memory -
   *  everything gets cleaned up by Windows. If you've ever had one of
   * those applications where you try to close it and it takes a while to
   * close down... honestly, a big cause of that is this sort of thing."
   *
   * WEB DEV ANALOGY:
   * JavaScript: const buffer = new Uint8Array(1000000);
   *             // When function ends, GC cleans up automatically
   *             // You don't manually delete it!
   *
   * C (Casey's way): void* buffer = malloc(1000000);
   *                  // When PROCESS ends, OS cleans up automatically
   *                  // You don't manually free it at exit!
   *
   * EXCEPTION - WHEN TO MANUALLY CLEAN:
   * Only clean up resources that are NOT process-lifetime:
   * - Switching levels → Free old level assets, load new ones
   * - Resizing window → Free old buffer, allocate new one ✅ (we do
   * this!)
   * - Closing modal → Free modal resources, keep main window
   *
   * THE BOTTOM LINE:
   * We COULD add cleanup here, but Casey teaches us it's:
   * 1. ❌ Slower (17× slower!)
   * 2. ❌ More code to maintain
   * 3. ❌ More places for bugs
   * 4. ❌ Wastes user's time
   * 5. ✅ OS does it better and faster
   *
   * So we follow Casey's philosophy: Just exit cleanly!
   */
  printf("Goodbye!\n");

  // ✅ NO MANUAL CLEANUP - OS handles it faster and better!
  // The OS will:
  // - Free g_PixelData (and all malloc'd memory)
  // - Destroy g_BackBuffer (XImage)
  // - Close window
  // - Close display connection
  // All in <1ms, automatically! ⚡

  return 0;
}
