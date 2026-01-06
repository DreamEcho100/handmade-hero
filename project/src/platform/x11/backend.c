#define _POSIX_C_SOURCE 199309L // Enable POSIX functions like nanosleep, sleep;

#include "backend.h"
#include "../../base/base.h"
#include "../../game.h"
#include "audio.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>    // For sleep()
#include <x86intrin.h> // for __rdtsc() (CPU cycle counter)

typedef struct {
  int fd;                // File descriptor for /dev/input/jsX
  char device_name[128]; // For debugging
} LinuxJoystickState;

file_scoped_global_var LinuxJoystickState g_joysticks[MAX_JOYSTICK_COUNT] = {0};

// ═══════════════════════════════════════════════════════════════
// 🎮 JOYSTICK DYNAMIC LOADING (Casey's Pattern for Linux)
// ═══════════════════════════════════════════════════════════════
// Define function signature macro
#define LINUX_JOYSTICK_READ(name) ssize_t name(int fd, struct js_event *event)

// Create typedef
typedef LINUX_JOYSTICK_READ(linux_joystick_read);

// Stub implementation (no joystick available)
LINUX_JOYSTICK_READ(LinuxJoystickReadStub) {
  // Return -1 = error/no device (like ERROR_DEVICE_NOT_CONNECTED)
  return -1;
}

// Global function pointer (initially stub)
file_scoped_global_var linux_joystick_read *LinuxJoystickRead_ =
    LinuxJoystickReadStub;

// Redefine API name
#define LinuxJoystickRead LinuxJoystickRead_

// file_scoped_global_var GameOffscreenBuffer game_buffer;
file_scoped_global_var XImage *g_buffer_info = NULL;

// Real implementation (only used if joystick found)
file_scoped_fn LINUX_JOYSTICK_READ(linux_joystick_read_impl) {
  // This is what actually reads from /dev/input/js*
  return read(fd, event, sizeof(*event));
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Initialize joystick (Casey's Win32LoadXInput equivalent)
// ═══════════════════════════════════════════════════════════════
// Dynamic loader
file_scoped_fn void
linux_init_joystick(GameControllerInput *controller_old_input,
                    GameControllerInput *controller_new_input) {
  printf("Searching for gamepad...\n");

  const char *device_paths[] = {"/dev/input/js0", "/dev/input/js1",
                                "/dev/input/js2", "/dev/input/js3"};

  // Initialize ALL controllers FIRST
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    controller_old_input[i].controller_index = i;
    controller_old_input[i].is_connected = false;

    controller_new_input[i].controller_index = i;
    controller_new_input[i].is_connected = false;

    int g_joysticks_index = i - 1; // Adjust for keyboard at index 0
    if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
      g_joysticks[g_joysticks_index].fd = -1;
      memset(g_joysticks[g_joysticks_index].device_name, 0,
             sizeof(g_joysticks[g_joysticks_index].device_name));
    }
  }

  // Mark keyboard (slot 0) as connected AFTER the loop
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  for (int i = MAX_KEYBOARD_COUNT; i < MAX_JOYSTICK_COUNT; i++) {
    int fd = open(device_paths[i - MAX_KEYBOARD_COUNT], O_RDONLY | O_NONBLOCK);

    if (fd >= 0) {
      char name[128] = {0};
      if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) >= 0) {

        printf("i: %d\n", i);
        // Skip virtual devices
        if (strstr(name, "virtual") || strstr(name, "keyd")) {
          close(fd);
          continue;
        }

        // Store controller index AND file descriptor separately
        controller_old_input[i].controller_index = i;
        controller_old_input[i].is_connected = true;
        controller_old_input[i].is_analog = true; // Joysticks are analog

        controller_new_input[i].controller_index = i;
        controller_new_input[i].is_connected = true;
        controller_new_input[i].is_analog = true; // Joysticks are analog

        int g_joysticks_index =
            i - MAX_KEYBOARD_COUNT; // Adjust for keyboard at index 0
        if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
          g_joysticks[g_joysticks_index].fd = fd;
          strncpy(g_joysticks[g_joysticks_index].device_name, name,
                  sizeof(g_joysticks[g_joysticks_index].device_name) - 1);
        }

        // Create wrapper function that uses our fd
        // (Linux doesn't have DLLs, but we can still use the pattern!)
        LinuxJoystickRead_ = linux_joystick_read_impl; // Real implementation

        printf("✅ Joystick connected: %s\n", name);
        // return true;
        continue; //
      } else {
        close(fd);
      }
    }
  }
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Poll joystick state (Casey's XInputGetState equivalent)
// ═══════════════════════════════════════════════════════════════

file_scoped_fn void linux_poll_joystick(GameInput *old_input,
                                        GameInput *new_input) {

  for (int controller_index = 0; controller_index < MAX_CONTROLLER_COUNT;
       controller_index++) {

    // Get controller state
    GameControllerInput *old_controller =
        &old_input->controllers[controller_index];
    GameControllerInput *new_controller =
        &new_input->controllers[controller_index];

    int g_joysticks_index = controller_index - 1;
    if (g_joysticks_index < 0 || g_joysticks_index >= MAX_JOYSTICK_COUNT) {
      // Skip keyboard (index 0)
      continue;
    }
    LinuxJoystickState *joystick_state = &g_joysticks[g_joysticks_index];

    // printf("controller_index: %d, new_controller->is_connected: %d\n",
    //        controller_index, new_controller->is_connected);
    // Use continue instead of return
    if (!new_controller->is_connected || joystick_state->fd < 0) {
      continue; // Skip this controller, check the next one
    }

    struct js_event event;

    while (LinuxJoystickRead(joystick_state->fd, &event) == sizeof(event)) {

      if (event.type & JS_EVENT_INIT) {
        continue;
      }

      // NOTE: Ignore the joystick for now since I don't have them

      // printf("JS_EVENT_BUTTON: %d, JS_EVENT_AXIS: %d\n", JS_EVENT_BUTTON,
      //  JS_EVENT_AXIS);
      // printf("event.type: %d, JS_EVENT_AXIS: %d\n", event.type,
      // JS_EVENT_AXIS);
      // ═══════════════════════════════════════════════════════════
      // 🎮 BUTTON EVENTS
      // ═══════════════════════════════════════════════════════════
      if (event.type == JS_EVENT_BUTTON) {
        bool is_pressed = (event.value != 0);

        printf("event.number: %d\n", event.number);
        printf("is_pressed: %d\n", is_pressed);

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
          // // Face buttons (cross, circle, square, triangle)
          // case 0: // X (cross) - Map to A button
          //   new_controller1->a_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button X (cross) pressed\n");
          //   break;

          // case 1: // O (circle) - Map to B button
          //   new_controller1->b_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button O (circle) pressed\n");
          //   break;

          // case 3: // □ (square) - Map to X button
          //   new_controller1->x_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button □ (square) pressed\n");
          //   break;

          // case 2: // △ (triangle) - Map to Y button
          //   new_controller1->y_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button △ (triangle) pressed\n");
          //   break;

          // Shoulder buttons
        case 4: // L1
                // new_controller1->left_shoulder = is_pressed;
                // if (is_pressed)
                //   printf("Button L1 pressed\n");
          process_game_button_state(is_pressed, &old_controller->left_shoulder,
                                    &new_controller->left_shoulder);
          break;

        case 5: // R1
          // new_controller1->right_shoulder = is_pressed;
          // if (is_pressed)
          //   printf("Button R1 pressed\n");
          process_game_button_state(is_pressed, &old_controller->right_shoulder,
                                    &new_controller->right_shoulder);
          break;

          // // Trigger buttons (L2/R2 as digital buttons)
          // // Note: Analog values are on axes 3 and 4
          // case 6: // L2 button
          //   if (is_pressed)
          //     printf("Button L2 pressed\n");
          //   break;

          // case 7: // R2 button
          //   if (is_pressed)
          //     printf("Button R2 pressed\n");
          //   break;

          // // Menu buttons
          // case 8: // Share/Create - Map to Back
          //   new_controller1->back = is_pressed;
          //   if (is_pressed)
          //     printf("Button Share/Create pressed\n");
          //   break;

          // case 9: // Options - Map to Start
          //   new_controller1->start = is_pressed;
          //   if (is_pressed)
          //     printf("Button Options pressed\n");
          //   break;

          // // Stick buttons
          // case 11: // L3 (left stick click)
          //   if (is_pressed)
          //     printf("Button L3 (left stick) pressed\n");
          //   break;

          // case 12: // R3 (right stick click)
          //   if (is_pressed)
          //     printf("Button R3 (right stick) pressed\n");
          //   break;

          // // Special buttons
          // case 10: // PS button
          //   if (is_pressed)
          //     printf("Button PS (logo) pressed\n");
          //   break;

          // case 13: // Touchpad button
          //   if (is_pressed)
          //     printf("Button Touchpad pressed\n");
          //   break;

          // default:
          //   // Unknown button
          //   if (is_pressed) {
          //     printf("Unknown button %d pressed\n", event.number);
          //   }
          //   break;
        }
      }

      // ═══════════════════════════════════════════════════════════
      // 🎮 AXIS EVENTS (Analog Sticks + Triggers + D-Pad)
      // ═══════════════════════════════════════════════════════════
      else if (event.type == JS_EVENT_AXIS) {
        new_controller->is_analog = true;
        // printf("new_controller->is_analog: %d\n", new_controller->is_analog);

        new_controller->start_x = old_controller->end_x;
        new_controller->start_y = old_controller->end_y;

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
        case 0: {
          // ═══════════════════════════════════════════════════
          // 🔥 CRITICAL: Linux joystick normalization
          // ═══════════════════════════════════════════════════
          // Linux /dev/input/jsX range: -32767 to +32767
          // This is SYMMETRIC (unlike XInput's asymmetric range)!
          //
          // SO WE USE SINGLE DIVISOR:
          //   X = (real32)event.value / 32767.0f;
          //
          // Casey's XInput needs TWO divisors:
          //   if(Pad->sThumbLX < 0) X = value / 32768.0f;
          //   else X = value / 32767.0f;
          //
          // Because XInput has -32768 to +32767 (asymmetric)
          // ═══════════════════════════════════════════════════

          // new_controller1->left_stick_x = event.value;
          real32 x = (real32)event.value / 32767.0f;

          new_controller->end_x = x;
          // new_controller1->min_x = fminf(new_controller1->min_x, x);
          new_controller->min_x = x; // Day 13: just set to end_x
          new_controller->max_x = x; // Day 14+: track actual min/max

          break;
        }
        // ─────────────────────────────────────────────────────
        // LEFT STICK Y (Axis 1)
        // ─────────────────────────────────────────────────────
        case 1: {
          real32 y = (real32)event.value / 32767.0f;

          // NOTE: Joystick Y is often inverted!
          // Up = negative, Down = positive
          // Game might want to flip this:
          // y = -y;  // Uncomment if your game wants up = positive

          new_controller->end_y = y;
          new_controller->min_y = y;
          new_controller->max_y = y;

          break;
        }

          // // ───────────────────────────────────────────────────────
          // // Right Stick (axes 2, 5) ← NOTE: Y is axis 5, not 3!
          // // ───────────────────────────────────────────────────────
          // // TODO(Day 14+): Add right stick support if needed
          // case 3: // L2 trigger X
          //   printf("Right Stick X\n");
          //   // Optional: Store trigger pressure if you need it
          //   new_controller1->right_stick_x = event.value;
          //   break;
          // case 4: // R2 trigger Y
          //   printf("Right Stick Y\n");
          //   new_controller1->right_stick_y = event.value;
          //   break;

          // // ───────────────────────────────────────────────────────
          // // Triggers (analog, 0 to +32767)
          // // ───────────────────────────────────────────────────────
          // case 2:
          //   new_controller1->left_trigger = event.value;
          //   printf("L2 trigger\n");
          //   break;
          // case 5: // ← Right stick Y is axis 5!
          //   new_controller1->right_trigger = event.value;
          //   printf("R2 trigger\n");
          //   break;

          // ─────────────────────────────────────────────────────
          // RIGHT STICK (Axes 2-5 depending on controller)
          // ─────────────────────────────────────────────────────
          // TODO(Day 14+): Add right stick support if needed

          // ═══════════════════════════════════════════════════════════
          // 🎮 D-PAD (Axes 6-7 on PlayStation controllers)
          // ═══════════════════════════════════════════════════════════
          // D-pad is DIGITAL (4 directions) but reported as ANALOG axis!
          // We must set BOTH button states AND analog values.
          //
          // Range: -32767 (left/up) to +32767 (right/down)
          // Threshold: Use ±16384 (half of max) for digital detection
          // ═══════════════════════════════════════════════════════════

        case 6: { // D-pad X (left/right)

          // ✅ FIX: Set analog values for game layer!
          new_controller->start_x = old_controller->end_x;
          new_controller->start_y = old_controller->end_y;

          if (event.value < -16384) {
            // D-pad LEFT pressed
            process_game_button_state(true, &old_controller->left,
                                      &new_controller->left);
            process_game_button_state(false, &old_controller->right,
                                      &new_controller->right);

            // ✅ Set analog value for left (-1.0)
            new_controller->end_x = -1.0f;

          } else if (event.value > 16384) {
            // D-pad RIGHT pressed
            process_game_button_state(true, &old_controller->right,
                                      &new_controller->right);
            process_game_button_state(false, &old_controller->left,
                                      &new_controller->left);

            // ✅ Set analog value for right (+1.0)
            new_controller->end_x = +1.0f;

          } else {
            // D-pad X RELEASED (centered)
            process_game_button_state(false, &old_controller->left,
                                      &new_controller->left);
            process_game_button_state(false, &old_controller->right,
                                      &new_controller->right);

            // ✅ Set analog value to zero
            new_controller->end_x = 0.0f;
          }

          // Update min/max for Day 13 (just mirror end value)
          new_controller->min_x = new_controller->max_x = new_controller->end_x;

          break;
        }

        case 7: { // D-pad Y (up/down)

          // ✅ FIX: Set analog values for game layer!
          new_controller->start_x = old_controller->end_x;
          new_controller->start_y = old_controller->end_y;

          if (event.value < -16384) {
            // D-pad UP pressed
            process_game_button_state(true, &old_controller->up,
                                      &new_controller->up);
            process_game_button_state(false, &old_controller->down,
                                      &new_controller->down);

            // ✅ Set analog value for up (+1.0)
            // NOTE: You might need to invert this depending on your coordinate
            // system
            new_controller->end_y = -1.0f;

          } else if (event.value > 16384) {
            // D-pad DOWN pressed
            process_game_button_state(true, &old_controller->down,
                                      &new_controller->down);
            process_game_button_state(false, &old_controller->up,
                                      &new_controller->up);

            // ✅ Set analog value for down (-1.0)
            new_controller->end_y = 1.0f;

          } else {
            // D-pad Y RELEASED (centered)
            process_game_button_state(false, &old_controller->up,
                                      &new_controller->up);
            process_game_button_state(false, &old_controller->down,
                                      &new_controller->down);

            // ✅ Set analog value to zero
            new_controller->end_y = 0.0f;
          }

          // Update min/max for Day 13 (just mirror end value)
          new_controller->min_y = new_controller->max_y = new_controller->end_y;

          break;
        }

        default:
          //   // printf("D-pad number: %d, value: %d\n", event.number,
          //   event.value);
          break;
        }
      }
    }
  }
}

/**
 * RESIZE BACK BUFFER
 *
 * Allocates (or reallocates) the pixel backbuffer when window size changes.
 *
 * Casey's equivalent: Win32ResizeDIBSection()
 *
 * FLOW:
 * 1. Free old backbuffer if it exists
 * 2. Allocate new pixel memory
 * 3. Create XImage wrapper
 *
 * ═══════════════════════════════════════════════════════════════════════
 * 🌊 CASEY'S "WAVE 2" RESOURCE - STATE LIFETIME
 * ═══════════════════════════════════════════════════════════════════════
 *
 * This backbuffer is a WAVE 2 resource (state-lifetime, not process-lifetime).
 * It lives ONLY as long as the current window size stays the same.
 *
 * Why we clean up here (unlike process-lifetime resources):
 * - We're REPLACING the backbuffer with a new one (different size)
 * - This happens DURING program execution (not at exit)
 * - If we don't free, we leak 1-3MB on EVERY resize!
 *
 * Example: User resizes window 10 times:
 * ❌ Without cleanup: 10 buffers × 2MB = 20MB leaked! 💥
 * ✅ With cleanup: Always just 1 backbuffer = 2MB total ⚡
 *
 * Casey's Rule: "Think about creation and destruction in WAVES.
 *                This backbuffer changes with window size (state change),
 *                so we manage it when state changes."
 *
 * 🟡 COLD PATH: Only runs on window resize (maybe once per second)
 *    So malloc/free here is totally fine!
 */
inline file_scoped_fn void resize_back_buffer(GameOffscreenBuffer *backbuffer,
                                              XImage **backbuffer_info,
                                              Display *display, Visual *visual,
                                              int window_depth, int width,
                                              int height) {

  // Free old backbuffer if it exists
  // This is WAVE 2 cleanup - we're changing state (window size)!
  //
  // Visual: What happens on resize
  // ┌─────────────────────────────────────────┐
  // │ Before (800×600):                       │
  // │ backbuffer_info → [1.8 MB of pixels]       │
  // │                                         │
  // │ User resizes to 1920×1080               │
  // │ ↓                                       │
  // │ We MUST free the 1.8 MB backbuffer!         │
  // │ Otherwise it leaks forever! 💥          │
  // │                                         │
  // │ After cleanup:                          │
  // │ backbuffer_info → NULL                     │
  // │ backbuffer->memory → NULL                      │
  // │                                         │
  // │ Now allocate new 8.3 MB backbuffer          │
  // │ backbuffer_info → [8.3 MB of pixels] ✅    │
  // └─────────────────────────────────────────┘

  if ((*backbuffer_info)) {
    // Call XDestroyImage() to free it
    // This ALSO frees backbuffer->memory automatically!
    // (X11 owns the memory once XCreateImage is called)
    if (backbuffer->memory.base) {
      (*backbuffer_info)->data = NULL; // XDestroyImage should not free
      platform_free_memory(&backbuffer->memory);
      backbuffer->memory = (PlatformMemoryBlock){0};
    }
    XDestroyImage((*backbuffer_info));

    (*backbuffer_info) = NULL;
  }

  // Calculate how much memory we need
  // Each pixel is 4 bytes (RGBA), so:
  // Total bytes = width × height × 4
  backbuffer->pitch = width * backbuffer->bytes_per_pixel; // Bytes per row
  int buffer_size = backbuffer->pitch * height;            // Total bytes

  printf("Allocating back backbuffer: %dx%d (%d bytes = %.2f MB)\n", width,
         height, buffer_size, buffer_size / (1024.0 * 1024.0));

  PlatformMemoryBlock backbuffer_memory = platform_allocate_memory(
      NULL, buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!backbuffer_memory.base) {
    fprintf(stderr,
            "platform_allocate_memory failed: could not allocate %d bytes\n",
            buffer_size);
    return;
  }

  // Allocate pixel memory using mmap (Casey-style)
  backbuffer->memory = backbuffer_memory;

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

  *backbuffer_info = XCreateImage(
      // display,                                        // X11 connection
      // DefaultVisual(display, DefaultScreen(display)), // Color format
      // 24,                     // Depth (24-bit RGB, ignore alpha)
      // ZPixmap,                // Format (chunky pixels, not planar)
      // 0,                      // Offset in data
      // (char *)backbuffer->memory, // Our pixel backbuffer
      // width, height,          // Dimensions
      // 32,                     // Bitmap pad (align to 32-bit boundaries)
      // 0                       // Bytes per line (0 = auto-calculate)
      display,                         //
      visual,                          //
      window_depth,                    //
      ZPixmap,                         //
      0,                               //
      (char *)backbuffer->memory.base, //
      width,                           //
      height,                          //
      32,                              //
      0                                //
  );

  // Save the dimensions
  backbuffer->width = width;
  backbuffer->height = height;

  printf("Back backbuffer created successfully\n");
}

/**
 * UPDATE WINDOW (BLIT)
 *
 * Copies pixels from back backbuffer to screen.
 * "Blit" = BLock Image Transfer = fast pixel copy
 *
 * Casey's equivalent: Win32UpdateWindow() using StretchDIBits()
 *
 * 🔴 HOT PATH: Could be called 60 times/second!
 * XPutImage is hardware-accelerated, so it's fast.
 */
static void update_window(GameOffscreenBuffer *backbuffer,
                          XImage **backbuffer_info, Display *display,
                          Window window, GC gc, int x, int y, int width,
                          int height) {
  // Don't blit if no backbuffer exists!
  if (!(*backbuffer_info)) {
    printf("WARNING: Tried to blit, but no backbuffer exists!\n");
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
   *    backbuffer->memory                   The actual window
   * ```
   */
  // Copy pixels from back backbuffer to window
  // This is THE KEY FUNCTION for double buffering!
  XPutImage(display,            // X11 connection
            window,             // Destination (the actual window)
            gc,                 // Graphics context
            (*backbuffer_info), // Source (our pixel backbuffer)
            x, y,               // Source position (which part of backbuffer)
            x, y,               // Dest position (where on window)
            width, height       // How much to copy
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
inline file_scoped_fn void
handle_event(GameOffscreenBuffer *backbuffer, XImage **backbuffer_info,
             Display *display, Window window, GC gc, XEvent *event,
             GameSoundOutput *sound_output, GameInput *old_game_input,
             GameInput *new_game_input) {
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
    //  * **Why do we resize the backbuffer here?**
    //  *
    //  * Because the window size changed! Our old backbuffer is the wrong size.
    //  We
    //  * need to allocate a new backbuffer that matches the new window
    //  dimensions.
    //  */
    // // Only resize if dimensions ACTUALLY CHANGED!
    // if (new_width != backbuffer->width || new_height != backbuffer->height) {
    //   printf("Window resized: %dx%d → %dx%d\n", backbuffer->width,
    //   backbuffer->height,
    //          new_width, new_height);
    //   resize_back_buffer(backbuffer, backbuffer_info, display, new_width,
    //   new_height);
    // } else {
    //   printf("ConfigureNotify (same size, ignoring)\n");
    // }

    // ═══════════════════════════════════════════════════════════════
    // 🔇 COMMENTED OUT: Day 5 uses fixed backbuffer, no resize
    // ═══════════════════════════════════════════════════════════════
    // if (new_width != backbuffer->width || new_height != backbuffer->height) {
    //     printf("Window resized: %dx%d → %dx%d\n",
    //            backbuffer->width, backbuffer->height, new_width, new_height);
    //     resize_back_buffer(backbuffer, backbuffer_info, display, new_width,
    //     new_height);
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
      is_game_running = false; // Stop the main loop
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
    update_window(backbuffer, backbuffer_info, display, window, gc, 0, 0,
                  backbuffer->width, backbuffer->height);
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
    is_game_running = false;
    break;
  }

  /**
   * KEY PRESS = KEYBOARD KEY PRESSED DOWN
   *
   * Casey's WM_KEYDOWN equivalent
   */
  case KeyPress: {
    KeySym key = XLookupKeysym(&event->xkey, 0);
    // printf("pressed\n");

    GameControllerInput *old_controller1 =
        &old_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];
    GameControllerInput *new_controller1 =
        &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

    switch (key) {
    case XK_F1: {
      printf("F1 pressed - showing audio debug\n");
      linux_debug_audio_latency(sound_output);
      break;
    }
    case (XK_w):
    case (XK_W):
    case (XK_Up): {
      // W/Up = Move up = positive Y
      new_controller1->end_y = +1.0f;
      new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
      new_controller1->is_analog = false;

      // Also set button state for backward compatibility
      process_game_button_state(true, &old_controller1->up,
                                &new_controller1->up);
      break;
    }
    case (XK_a):
    case (XK_A):
    case (XK_Left): {
      // A/Left = Move left = negative X
      new_controller1->end_x = -1.0f;
      new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
      new_controller1->is_analog = false;

      process_game_button_state(true, &old_controller1->left,
                                &new_controller1->left);
      break;
    }
    case (XK_s):
    case (XK_S):
    case (XK_Down): {
      // S/Down = Move down = negative Y
      new_controller1->end_y = -1.0f;
      new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
      new_controller1->is_analog = false;

      process_game_button_state(true, &old_controller1->down,
                                &new_controller1->down);
      break;
    }
    case (XK_d):
    case (XK_D):
    case (XK_Right): {
      // D/Right = Move right = positive X
      new_controller1->end_x = +1.0f;
      new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
      new_controller1->is_analog = false;

      process_game_button_state(true, &old_controller1->right,
                                &new_controller1->right);
      break;
    }
    case (XK_space): {
      printf("SPACE pressed\n");
      break;
    }
    case (XK_Escape): {
      printf("ESCAPE pressed - exiting\n");
      is_game_running = false;
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

    GameControllerInput *old_controller1 =
        &old_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];
    GameControllerInput *new_controller1 =
        &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

    switch (key) {
    case (XK_w):
    case (XK_W):
    case (XK_Up): {
      new_controller1->end_y = 0.0f;
      new_controller1->min_y = new_controller1->max_y = 0.0f;
      new_controller1->is_analog = false;
      process_game_button_state(false, &old_controller1->up,
                                &new_controller1->up);
      break;
    }
    case (XK_a):
    case (XK_A):
    case (XK_Left): {
      new_controller1->end_x = 0.0f;
      new_controller1->min_x = new_controller1->max_x = 0.0f;
      new_controller1->is_analog = false;
      process_game_button_state(false, &old_controller1->left,
                                &new_controller1->left);
      break;
    }
    case (XK_s):
    case (XK_S):
    case (XK_Down): {
      new_controller1->end_y = 0.0f;
      new_controller1->min_y = new_controller1->max_y = 0.0f;
      new_controller1->is_analog = false;
      process_game_button_state(false, &old_controller1->down,
                                &new_controller1->down);
      break;
    }
    case (XK_d):
    case (XK_D):
    case (XK_Right): {
      new_controller1->end_x = 0.0f;
      new_controller1->min_x = new_controller1->max_x = 0.0f;
      new_controller1->is_analog = false;
      process_game_button_state(false, &old_controller1->right,
                                &new_controller1->right);
      break;
    }
    case (XK_space): {
      printf("SPACE released\n");
      break;
    }
    case (XK_Escape): {
      printf("ESCAPE released - exiting\n");
      is_game_running = false;
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

// X11 pixel composer (0xAARRGGBB format)
file_scoped_fn uint32_t compose_pixel_xrgb(uint8_t r, uint8_t g, uint8_t b,
                                           uint8_t a) {
  return ((a << 24) | (r << 16) | (g << 8) | (b));
}

// ═══════════════════════════════════════════════════════════
// Clear new input buttons to released state
// ═══════════════════════════════════════════════════════════
// X11 keyboard only sends events on press/release.
// If no event, button stays in old state (wrong!).
// So we must explicitly clear to "not pressed".
// ═══════════════════════════════════════════════════════════
file_scoped_fn void prepare_input_frame(GameInput *old_input,
                                        GameInput *new_input) {
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *old_ctrl = &old_input->controllers[i];
    GameControllerInput *new_ctrl = &new_input->controllers[i];

    // ═══════════════════════════════════════════════════════════
    // STEP 1: Copy connection state (doesn't change mid-frame)
    // ═══════════════════════════════════════════════════════════
    new_ctrl->is_connected = old_ctrl->is_connected;
    new_ctrl->is_analog = old_ctrl->is_analog;

    // ═══════════════════════════════════════════════════════════
    // STEP 2: Set start position = last frame's end position
    // ═══════════════════════════════════════════════════════════
    new_ctrl->start_x = old_ctrl->end_x;
    new_ctrl->start_y = old_ctrl->end_y;

    // ═══════════════════════════════════════════════════════════
    // STEP 3: Initialize analog values
    // ═══════════════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════════════
    // PRESERVE analog values for BOTH keyboard AND joystick!
    // ═══════════════════════════════════════════════════════════
    // Linux joystick only sends events on CHANGE, not while held.
    // X11 keyboard only sends events on press/release, not while held.
    // So we MUST preserve values for both!
    new_ctrl->end_x = old_ctrl->end_x;
    new_ctrl->end_y = old_ctrl->end_y;

    new_ctrl->min_x = new_ctrl->max_x = new_ctrl->end_x;
    new_ctrl->min_y = new_ctrl->max_y = new_ctrl->end_y;

    // ═══════════════════════════════════════════════════════════
    // STEP 4: Buttons - preserve state, clear transition count
    // ═══════════════════════════════════════════════════════════
    for (int btn = 0; btn < MAX_CONTROLLER_COUNT; btn++) {
      // Preserve ended_down (button still held until release event)
      new_ctrl->buttons[btn].ended_down = old_ctrl->buttons[btn].ended_down;
      // Clear transition count (will be set if event occurs)
      new_ctrl->buttons[btn].half_transition_count = 0;
    }
  }
}

// Helper to get current time in seconds
static inline double get_wall_clock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int platform_main() {
  double t_start = get_wall_clock();
  printf("[%.3fs] Starting platform_main\n", get_wall_clock() - t_start);
  fflush(stdout);

#if HANDMADE_INTERNAL
  // Debug/Development mode: Reserve 2TB of address space for debugging
  // What if your RAM is less than 2TB? No problem, we're just reserving
  // address space, not committing physical memory yet.
  // Which means no actual RAM is used until we commit it (with mmap or
  // similar).
  //
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL;
#endif

  uint64_t permanent_storage_size = MEGABYTES(64);
  uint64_t transient_storage_size = GIGABYTES(4);

  printf("[%.3fs] Allocating permanent storage (%lu MB)...\n",
         get_wall_clock() - t_start, permanent_storage_size / (1024 * 1024));
  fflush(stdout);
  PlatformMemoryBlock permanent_storage = platform_allocate_memory(
      base_address, permanent_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  printf("[%.3fs] Permanent storage allocated\n", get_wall_clock() - t_start);
  fflush(stdout);

  if (!permanent_storage.base) {
    fprintf(stderr, "ERROR: Could not allocate permanent storage\n");
    return 1;
  }

  // Calculate next address
  void *transient_base =
      (uint8_t *)permanent_storage.base + permanent_storage.size;

  printf("[%.3fs] Allocating transient storage (%lu GB)...\n",
         get_wall_clock() - t_start,
         transient_storage_size / (1024 * 1024 * 1024));
  fflush(stdout);

  PlatformMemoryBlock transient_storage = platform_allocate_memory(
      transient_base, transient_storage_size, // ← Actually allocate it!
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  printf("[%.3fs] Transient storage allocated\n", get_wall_clock() - t_start);
  fflush(stdout);

  if (!transient_storage.base) {
    fprintf(stderr, "ERROR: Could not allocate transient storage\n");
    platform_free_memory(&permanent_storage);
    return 1;
  }

  GameMemory game_memory = {0};
  game_memory.permanent_storage = permanent_storage;
  game_memory.transient_storage = transient_storage;
  game_memory.permanent_storage_size = permanent_storage.size;
  game_memory.transient_storage_size = transient_storage.size;

  printf("✅ Game memory allocated:\n");
  printf("   Permanent: %lu MB at %p\n",
         game_memory.permanent_storage.size / (1024 * 1024),
         game_memory.permanent_storage.base);
  printf("   Transient: %lu GB at %p\n",
         game_memory.transient_storage.size / (1024 * 1024 * 1024),
         game_memory.transient_storage.base);

  if (game_memory.permanent_storage.base &&
      game_memory.transient_storage.base) {
    static GameInput game_inputs[2] = {0}; // Static - survives across frames!
    GameInput *new_game_input = &game_inputs[0];
    GameInput *old_game_input = &game_inputs[1];

    GameSoundOutput game_sound_output = {0};
    GameOffscreenBuffer game_buffer = {0};

    // ═══════════════════════════════════════════════════════════
    // 🎮 Initialize joystick BEFORE main loop (Casey's pattern)
    // ═══════════════════════════════════════════════════════════
    printf("[%.3fs] Initializing joystick...\n", get_wall_clock() - t_start);
    fflush(stdout);
    linux_init_joystick(old_game_input->controllers,
                        new_game_input->controllers);
    printf("[%.3fs] Joystick initialized\n", get_wall_clock() - t_start);
    fflush(stdout);
    // ═══════════════════════════════════════════════════════════
    // 🔊 Load ALSA library (Casey's Win32LoadXInput pattern)
    // ═══════════════════════════════════════════════════════════
    // This MUST come before linux_init_sound()!
    // Just like Casey calls Win32LoadXInput() before using XInput.
    printf("[%.3fs] Loading ALSA library...\n", get_wall_clock() - t_start);
    fflush(stdout);
    linux_load_alsa();
    printf("[%.3fs] ALSA library loaded\n", get_wall_clock() - t_start);
    fflush(stdout);

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
    // NOTE: This is a SECONDARY backbuffer size (where we write audio).
    //       The PRIMARY backbuffer just sets the format.
    // ═══════════════════════════════════════════════════════════
    int samples_per_second = 48000;
    int bytes_per_sample = sizeof(int16_t) * 2; // 16-bit stereo
    int secondary_buffer_size = samples_per_second * bytes_per_sample;
    linux_init_sound(&game_sound_output, samples_per_second,
                     secondary_buffer_size);

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

    // Force 32-bit visual with alpha
    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) {
      fprintf(stderr, "❌ No 32-bit visual available\n");
      return 1;
    }

    printf("✅ Using 32-bit visual (depth: %d)\n", vinfo.depth);

    // Create colormap for 32-bit visual
    Colormap colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);

    g_buffer_info = NULL;

    int init_backbuffer_status =
        init_backbuffer(&game_buffer, 1280, 720, 4, compose_pixel_xrgb);
    if (init_backbuffer_status != 0) {
      fprintf(stderr, "Failed to initialize backbuffer\n");
      return init_backbuffer_status;
    }

    g_buffer_info = XCreateImage(
        // display,                                        // X11 connection
        // DefaultVisual(display, DefaultScreen(display)), // Color format
        // 24,                          // Depth (24-bit RGB, ignore alpha)
        // ZPixmap,                     // Format (chunky pixels, not planar)
        // 0,                           // Offset in data
        // (char *)game_buffer.memory, // Our pixel backbuffer
        // game_buffer.width, game_buffer.height, // Dimensions
        // 32, // Bitmap pad (align to 32-bit boundaries)
        // 0   // Bytes per line (0 = auto-calculate)
        display,                               //
        vinfo.visual,                          //
        vinfo.depth,                           //
        ZPixmap,                               //
        0,                                     //
        (char *)game_buffer.memory.base,       //
        game_buffer.width, game_buffer.height, //
        32,                                    //
        0                                      //
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
    // Window window = XCreateSimpleWindow(
    //     display,                                 //
    //     root,                                    //
    //     0, 0,                                    // x, y position
    //     game_buffer.width, game_buffer.height, // width, height
    //     1,                                       // border width
    //     BlackPixel(display, screen),             // border color
    //     WhitePixel(display, screen)              // background color
    // );
    XSetWindowAttributes attrs = {0};
    attrs.colormap = colormap;
    attrs.event_mask = ExposureMask | StructureNotifyMask | FocusChangeMask |
                       KeyPressMask | KeyReleaseMask;

    Window window =
        XCreateWindow(display,                                                //
                      root,                                                   //
                      0, 0,                                                   //
                      game_buffer.width,                                      //
                      game_buffer.height,                                     //
                      0,                                                      //
                      vinfo.depth,                                            //
                      InputOutput,                                            //
                      vinfo.visual,                                           //
                      CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, //
                      &attrs                                                  //
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
     * We need to create the back backbuffer BEFORE entering the event loop
     * so we have something to draw to!
     *
     * Note: ConfigureNotify will fire after XMapWindow, but we also
     * want to draw immediately, so we allocate here.
     */
    resize_back_buffer(&game_buffer, &g_buffer_info, display, vinfo.visual,
                       vinfo.depth, game_buffer.width, game_buffer.height);

    // int test_offset = 0;

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

    while (is_game_running) {
      // ═══════════════════════════════════════════════════════════
      // 🐛 DEBUG: Print controller states (TEMPORARY!)
      // ═══════════════════════════════════════════════════════════
      static int frame_count = 0;
      if (frame_count++ % 60 == 0) { // Print once per second (60 FPS)
        printf("\n🎮 Controller States:\n");
        for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
          GameControllerInput *c = &old_game_input->controllers[i];
          LinuxJoystickState *joystick_state = NULL;
          if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
            joystick_state = &g_joysticks[i - 1];
          }
          printf("  [%d] connected=%d analog=%d fd=%d end_x=%.2f end_y=%.2f\n",
                 i, c->is_connected, c->is_analog,
                 joystick_state ? joystick_state->fd : -1, c->end_x, c->end_y);
        }
      }

      // ═══════════════════════════════════════════════════════════
      // Clear new input buttons to released state
      // ═══════════════════════════════════════════════════════════
      // X11 keyboard only sends events on press/release.
      // If no event, button stays in old state (wrong!).
      // So we must explicitly clear to "not pressed".
      // ═══════════════════════════════════════════════════════════
      prepare_input_frame(old_game_input, new_game_input);

      XEvent event;

      // ═══════════════════════════════════════════════════════════
      // Process events, joystick, call game...
      // ═══════════════════════════════════════════════════════════
      while (XPending(display) > 0) {
        XNextEvent(display, &event);
        handle_event(&game_buffer, &g_buffer_info, display, window, gc, &event,
                     &game_sound_output, old_game_input, new_game_input);
      }
      linux_poll_joystick(old_game_input, new_game_input);
      // printf("new_game_input->controllers[1].is_analog: %d\n",
      //        new_game_input->controllers[1].is_analog);

      if (game_buffer.memory.base) {

        // Display the result
        update_window(&game_buffer, &g_buffer_info, display, window, gc, 0, 0,
                      game_buffer.width, game_buffer.height);

        game_update_and_render(&game_memory, new_game_input, &game_buffer,
                               &game_sound_output);
      }
      // ═══════════════════════════════════════════════════════════
      // Day 8: Fill and write audio backbuffer every frame
      // ═══════════════════════════════════════════════════════════
      //
      // We generate samples and write them to ALSA.
      //
      // NOTE: ALSA handles playback automatically once we start
      // writing. No explicit "Play()" call needed!
      // ═══════════════════════════════════════════════════════════
      linux_fill_sound_buffer(&game_sound_output);

      // ═══════════════════════════════════════════════════════════
      // Timing
      // ═══════════════════════════════════════════════════════════
      clock_gettime(CLOCK_MONOTONIC, &end);
      end_cycles = __rdtsc();

      double ms_per_frame = (end.tv_sec - start.tv_sec) * 1000.0 +
                            (end.tv_nsec - start.tv_nsec) / 1000000.0;
      double fps = 1000.0 / ms_per_frame;
      double mcpf = (end_cycles - start_cycles) / 1000000.0;

      // Show FPS every 60 frames to verify performance
      static int frame_counter = 0;
      if (++frame_counter >= 60) {
        printf("[X11] %.2fms/f, %.2ff/s, %.2fmc/f\n", ms_per_frame, fps, mcpf);
        frame_counter = 0;
      }

      start = end;
      start_cycles = end_cycles;

      // ═══════════════════════════════════════════════════════════
      // SWAP INPUT BUFFERS (THE CRITICAL STEP!)
      // ═══════════════════════════════════════════════════════════
      // This is what makes double buffering work!
      // ═══════════════════════════════════════════════════════════
      // Swap pointers (preserves previous frame!)
      GameInput *temp_game_input = new_game_input;
      new_game_input = old_game_input;
      old_game_input = temp_game_input;
    }
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
   * │ - g_Buffer (per window size)                              │
   * │                                                                │
   * │ ✅ Casey says: Clean up WHEN STATE CHANGES (in batches)        │
   * │    We DO clean this in resize_back_buffer() because:          │
   * │    1. We're REPLACING it with a new backbuffer                     │
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
   * JavaScript: const backbuffer = new Uint8Array(1000000);
   *             // When function ends, GC cleans up automatically
   *             // You don't manually delete it!
   *
   * C (Casey's way): void* backbuffer = malloc(1000000);
   *                  // When PROCESS ends, OS cleans up automatically
   *                  // You don't manually free it at exit!
   *
   * EXCEPTION - WHEN TO MANUALLY CLEAN:
   * Only clean up resources that are NOT process-lifetime:
   * - Switching levels → Free old level assets, load new ones
   * - Resizing window → Free old backbuffer, allocate new one ✅ (we do
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
  // - Destroy g_Buffer (XImage)
  // - Close window
  // - Close display connection
  // All in <1ms, automatically! ⚡

  return 0;
}
