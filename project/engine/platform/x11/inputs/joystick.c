#include "../../../game/input.h"
#include <fcntl.h>
#include <linux/joystick.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  int fd;                // File descriptor for /dev/input/jsX
  char device_name[128]; // For debugging
} LinuxJoystickState;

de100_file_scoped_global_var LinuxJoystickState
    g_joysticks[MAX_JOYSTICK_COUNT] = {0};

// ═══════════════════════════════════════════════════════════════
// 🎮 JOYSTICK DYNAMIC LOADING (Casey's Pattern for Linux)
// ═══════════════════════════════════════════════════════════════
// Define function signature macro
#define LINUX_JOYSTICK_READ(name) ssize_t name(int fd, struct js_event *event)

// Create typedef
typedef LINUX_JOYSTICK_READ(linux_joystick_read);

// Stub implementation (no joystick available)
LINUX_JOYSTICK_READ(LinuxJoystickReadStub) {
  (void)fd;
  (void)event;
  // Return -1 = error/no device (like ERROR_DEVICE_NOT_CONNECTED)
  return -1;
}

// Global function pointer (initially stub)
de100_file_scoped_global_var linux_joystick_read *LinuxJoystickRead_ =
    LinuxJoystickReadStub;

// Redefine API name
#define LinuxJoystickRead LinuxJoystickRead_

// Real implementation (only used if joystick found)
de100_file_scoped_fn LINUX_JOYSTICK_READ(linux_joystick_read_impl) {
  // This is what actually reads from /dev/input/js*
  return read(fd, event, sizeof(*event));
}

void linux_init_joystick(GameControllerInput *controller_old_input,
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
      de100_mem_set(g_joysticks[g_joysticks_index].device_name, 0,
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

void linux_close_joysticks(void) {
  for (int i = 0; i < MAX_JOYSTICK_COUNT; i++) {
    if (g_joysticks[i].fd >= 0) {
      close(g_joysticks[i].fd);
      g_joysticks[i].fd = -1;
      de100_mem_set(g_joysticks[i].device_name, 0,
                    sizeof(g_joysticks[i].device_name));
    }
  }
}

void linux_poll_joystick(GameInput *new_input) {

  for (int controller_index = 0; controller_index < MAX_CONTROLLER_COUNT;
       controller_index++) {
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
    real32 stick_x = 0.0f;
    real32 dpad_x = 0.0f;
    real32 stick_y = 0.0f;
    real32 dpad_y = 0.0f;

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
        new_controller->is_analog = false;

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
          process_game_button_state(is_pressed, &new_controller->action_down);
          break;

        case 1: // O (circle) - Map to B button
          process_game_button_state(is_pressed, &new_controller->action_right);
          break;

        case 3: // □ (square) - Map to X button
          process_game_button_state(is_pressed, &new_controller->action_left);
          break;

        case 2: // △ (triangle) - Map to Y button
          process_game_button_state(is_pressed, &new_controller->action_up);
          break;

        // Shoulder buttons
        case 4: // L1
          process_game_button_state(is_pressed, &new_controller->left_shoulder);
          break;

        case 5: // R1
          process_game_button_state(is_pressed,
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

        // Menu buttons
        case 8: // Share/Create - Map to Back
          if (is_pressed)
            printf("Button Share/Create pressed\n");
          break;

        case 9: // Options - Map to Start
          // new_controller1->start = is_pressed;
          if (is_pressed)
            printf("Button Options pressed\n");

          process_game_button_state(is_pressed, &new_controller->start);
          break;

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
        new_controller->is_analog = true;
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
          stick_x = (real32)event.value / 32767.0f;
          break;
        }
        // ─────────────────────────────────────────────────────
        // LEFT STICK Y (Axis 1)
        // ─────────────────────────────────────────────────────
        case 1: {
          stick_y = (real32)event.value / 32767.0f;
          break;
        }

          // // ───────────────────────────────────────────────────────
          // // Right Stick (axes 2, 5) ← NOTE: Y is axis 5, not 3!
          // // ───────────────────────────────────────────────────────
          // case 3: // L2 trigger X
          //   printf("Right Stick X\n");
          //   break;
          // case 4: // R2 trigger Y
          //   printf("Right Stick Y\n");
          //   break;

          // ───────────────────────────────────────────────────────
          // Triggers  (analog, 0 to +32767)
          // ───────────────────────────────────────────────────────
          // case 2:
          //   printf("D-pad number: %d, value: %d\n", event.number,
          //   event.value);
          //   printf("L2 trigger\n");
          //   break;
          // case 5: // ← Right stick Y is axis 5!
          //   printf("D-pad number: %d, value: %d\n", event.number,
          //   event.value); printf("R2 trigger\n"); break;

          // case 3:
          //   printf("3 trigger\n");
          //   break;
          // case 4: // ← Right stick Y is axis 5!
          //   printf("4 trigger\n");
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
          dpad_x = (event.value < -16384)  ? -1.0f
                   : (event.value > 16384) ? 1.0f
                                           : 0.0f;
          break;
        }

        case 7: { // D-pad Y (up/down)
          dpad_y = (event.value < -16384)  ? -1.0f
                   : (event.value > 16384) ? 1.0f
                                           : 0.0f;
          break;
        }

        default:
          // printf("D-pad number: %d, value: %d\n", event.number, event.value);
          break;
        }
      }
    }

    if (new_controller->is_connected && new_controller->is_analog) {
      if (fabsf(stick_x) > BASE_JOYSTICK_DEADZONE) {
        new_controller->stick_avg_x = stick_x; // Stick wins!
      } else {
        new_controller->stick_avg_x = dpad_x; // D-pad fallback
      }

      if (fabsf(stick_y) > BASE_JOYSTICK_DEADZONE) {
        new_controller->stick_avg_y = stick_y; // Stick wins!
      } else {
        new_controller->stick_avg_y = dpad_y; // D-pad fallback
      }

      // Horizontal stick → move_left/move_right buttons
      process_game_button_state(
          (new_controller->stick_avg_x < -BASE_JOYSTICK_DEADZONE),
          &new_controller->move_left);

      process_game_button_state(
          (new_controller->stick_avg_x < -BASE_JOYSTICK_DEADZONE),
          &new_controller->move_right);

      // Vertical stick → move_up/move_down buttons
      process_game_button_state(
          (new_controller->stick_avg_y > BASE_JOYSTICK_DEADZONE),
          &new_controller->move_down // Y inverted!
      );

      process_game_button_state(
          (new_controller->stick_avg_y > BASE_JOYSTICK_DEADZONE),
          &new_controller->move_up);
    }
  }
}

void debug_joystick_state(GameInput *game_input) {
  (void)game_input;
  // printf("\n🎮 Controller States:\n");
  // for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
  //   GameControllerInput *c = &game_input->controllers[i];
  //   LinuxJoystickState *joystick_state = NULL;
  //   if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
  //     joystick_state = &g_joysticks[i - 1];
  //   }
  //   printf("  [%d] connected=%d analog=%d fd=%d stick_avg_x=%.2f "
  //          "stick_avg_y=%.2f\n",
  //          i, c->is_connected, c->is_analog,
  //          joystick_state ? joystick_state->fd : -1, c->stick_avg_x,
  //          c->stick_avg_y);
  // }
}