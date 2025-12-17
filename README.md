# 📝 Handmade Hero: Learning Notes

> Following Casey Muratori's Handmade Hero series, implementing on **Linux** with X11 (low-level) + Raylib (cross-platform) backends.

---

## Resources

### Main Links

- [Episode Guide](https://guide.handmadehero.org/)

### Podcasts

- [CORECURSIVE #062 - Video Game Programming From Scratch - With Casey Muratori](https://corecursive.com/062-game-programming/)

### Blogs

- [Learn Game Engine Programming](https://engine-programming.net/)
- [How 99% of C Tutorials Get it Wrong](https://sbaziotis.com/uncat/how-c-tutorials-get-it-wrong.html)
- [Cache-Friendly Code](https://www.baeldung.com/cs/cache-friendly-code)

## YouTube Playlists

- [Beginner C Videos By - Jacob Sorber _(@JacobSorber)_](https://www.youtube.com/playlist?list=PL9IEJIKnBJjG5H0ylFAzpzs9gSmW_eICB)
- [Pitfalls of Object Oriented Programming, Revisited - Tony Albrecht (TGC 2017)](https://youtu.be/VAT9E-M-PoE?si=XPRMfNUkrZFs90C9)
- [Practical Optimizations](https://youtu.be/NAVbI1HIzCE?si=iuNdnk68oGFqV6IW)
- [Building a Data-Oriented Future - Mike Acton](https://youtu.be/u8B3j8rqYMw?si=oyLQUlNRpBr6yd61)

---

## 📅 Days Summary

---

### 📆 Day 1-2: Platform Setup & Window Creation

**Focus:** Basic window, pixel buffers, platform abstraction... I was just missing around the first day 😅

#### 🗓️ Commits

| Date      | Commit            | What Changed                                   |
| --------- | ----------------- | ---------------------------------------------- |
| Nov 19    | `1a318ec`         | Basic C setup, first compilation               |
| Nov 20    | `bb97195`         | Raylib from source, first window, linker flags |
| Nov 21    | `43be272`         | Platform layer separation (game vs platform)   |
| Nov 21    | `96147fc`         | Multiple backends: X11, MIT-SHM, GLX           |
| Nov 21    | `14933ba`         | Simplified to one X11 backend                  |
| Nov 23    | `7d2ff1e`         | Roadmap planning, Xft fonts                    |
| Nov 23-26 | `f647196→157f661` | Custom message box (modal dialogs)             |

#### 📁 Project Structure

```
project/src/
├── game/              # Platform-independent
│   ├── game.c
│   └── game.h
└── platform/          # OS-specific
    ├── x11_backend.c      # Linux low-level
    └── raylib_backend.c   # Cross-platform
```

#### 🎯 Core Concepts

| Concept            | Implementation                                            |
| ------------------ | --------------------------------------------------------- |
| **Platform Layer** | Dual backends (X11 + Raylib), same game code              |
| **Pixel Buffer**   | CPU memory → gradient animation                           |
| **X11 Basics**     | `XOpenDisplay()`, `XCreateWindow()`, `XImage`, event loop |
| **X11 Advanced**   | Xft fonts, Pixmap double-buffer, modal dialogs            |
| **Frame Timing**   | 60 FPS with `nanosleep()` (16.67ms/frame)                 |
| **Memory**         | `mmap()` for pixel buffers, Wave 1/2 cleanup              |

#### ✅ Skills Acquired

- ✅ C compilation + linking (`gcc`, `-lX11`, `-lGL`)
- ✅ Build scripts (`build.sh`, `run-dev.sh`)
- ✅ Platform abstraction architecture
- ✅ X11 window management + event loop
- ✅ Pixel formats: BGRA (X11) vs RGBA (Raylib)

#### 💻 Code Snippets with Explanations

**1. Basic X11 Window Creation**

```c
#include <X11/Xlib.h>

// Connect to X server
Display *display = XOpenDisplay(NULL);  // NULL = default display (:0)
if (!display) { /* error handling */ }

int screen = DefaultScreen(display);
Window root = RootWindow(display, screen);

// Create window
Window window = XCreateSimpleWindow(
    display, root,
    0, 0,           // x, y position
    1280, 720,      // width, height
    0,              // border width
    BlackPixel(display, screen),  // border color
    BlackPixel(display, screen)   // background color
);

// Select events we care about
XSelectInput(display, window,
    ExposureMask |           // Window exposed/needs redraw
    StructureNotifyMask |    // Resize, close, etc.
    KeyPressMask |           // Keyboard input
    KeyReleaseMask
);

XMapWindow(display, window);  // Make window visible
```

**2. X11 Event Loop (Casey's Pattern)**

```c
Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
XSetWMProtocols(display, window, &wm_delete, 1);

bool running = true;
while (running) {
    // Process ALL pending events (don't block on XNextEvent!)
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);

        switch (event.type) {
            case Expose:
                // Window needs redraw
                break;
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == wm_delete) {
                    running = false;  // Window close button clicked
                }
                break;
            case KeyPress:
                // Handle keyboard
                break;
        }
    }

    // Game update + render here (runs every frame, not just on events!)
    render_frame();

    // Frame timing (60 FPS = 16.67ms per frame)
    struct timespec sleep_time = {0, 16666667};  // ~16.67ms
    nanosleep(&sleep_time, NULL);
}
```

**3. Platform Layer Architecture**

```c
// game.h - Platform-independent interface
typedef struct {
    void *memory;
    int width, height, pitch;
} GameBuffer;

void game_update_and_render(GameBuffer *buffer);

// x11_backend.c - Linux implementation
#include "game.h"
// ... X11 code that calls game_update_and_render()

// raylib_backend.c - Cross-platform implementation
#include "game.h"
// ... Raylib code that calls game_update_and_render()

// Same game.c works with BOTH backends!
```

---

### 📆 Day 3-5: Back Buffer & Rendering

**Focus:** Double buffering, pixel math, memory allocation, struct organization

#### 🗓️ Commits

| Date         | Commits           | What Changed                             |
| ------------ | ----------------- | ---------------------------------------- |
| Nov 28       | `e7e6991`         | Wave 2 resources, OffscreenBuffer struct |
| Nov 28-29    | `59ddf6b→117f955` | Gradient rendering, pixel math bugs      |
| Nov 29-Dec 1 | `a6e564e→7915890` | Day 5 refactor, fixed buffer, GC reuse   |

#### 📊 Pixel Buffer Memory Layout

```
Memory Address:  0x1000   0x1001   0x1002   0x1003   0x1004   ...
                ┌────────┬────────┬────────┬────────┬────────┬────
                │   B    │   G    │   R    │   X    │   B    │ ...
                └────────┴────────┴────────┴────────┴────────┴────
                ◄───── Pixel 0 (BGRA) ─────►◄── Pixel 1 ──

2D → 1D Mapping:
┌───────────────────────────────────────────────┐
│ (0,0)   (1,0)   (2,0)   ...    (W-1,0)        │  Row 0
│ (0,1)   (1,1)   (2,1)   ...    (W-1,1)        │  Row 1
│   ↓       ↓       ↓              ↓            │
│   0       1       2    ...     W-1            │  Linear index
│   W      W+1     W+2   ...    2W-1            │
└───────────────────────────────────────────────┘

Formula: offset = y * width + x
```

#### 🎯 Core Concepts

| Concept             | Implementation                                           |
| ------------------- | -------------------------------------------------------- |
| **Back Buffer**     | `XImage` + `mmap()` (Wave 2 resource)                    |
| **Pixel Math**      | `offset = y * width + x` (2D→1D)                         |
| **Memory**          | `calloc()` (8× faster) → `mmap()` (Day 4)                |
| **Resource Waves**  | Wave 1 (process) vs Wave 2 (state)                       |
| **OffscreenBuffer** | Struct with `info`, `memory`, `width`, `height`, `pitch` |
| **Fixed Buffer**    | 1280×720, never resize (Day 5 philosophy)                |
| **GC Reuse**        | Create once, never free (Casey's `CS_OWNDC`)             |

#### 🔄 Code Evolution

**Before (Day 3-4):** Scattered globals

```c
global_var XImage *g_BackBuffer;
global_var void *g_PixelData;
global_var int g_BufferWidth, g_BufferHeight;
```

**After (Day 5):** Organized struct

```c
typedef struct {
    XImage *info;
    void *memory;
    int width, height, pitch, bytes_per_pixel;
} OffscreenBuffer;
```

#### 🐛 Bugs Fixed

| Bug                | Cause                     | Fix                     |
| ------------------ | ------------------------- | ----------------------- |
| Segfault at i=1000 | `offset = i*width + i`    | `offset = y*width + x`  |
| NULL pointer       | Drawing before allocation | Pre-allocate in init    |
| Use-after-free     | `XFreeGC()` then reuse    | Create GC once (Wave 1) |

#### 💻 Code Snippets with Explanations

**1. Memory Allocation with mmap (Casey's Day 4)**

```c
#include <sys/mman.h>

// Allocate pixel buffer using mmap (like Casey does)
int buffer_size = width * height * bytes_per_pixel;
void *memory = mmap(
    NULL,                    // Let OS choose address
    buffer_size,             // Size in bytes
    PROT_READ | PROT_WRITE,  // Read + write access
    MAP_PRIVATE | MAP_ANONYMOUS,  // Private, not backed by file
    -1,                      // No file descriptor (anonymous)
    0                        // Offset (ignored for anonymous)
);

// Why mmap over malloc?
// 1. Pages are zero-initialized by OS (like calloc)
// 2. Can be easily unmapped (munmap)
// 3. Casey's pattern - matches Win32 VirtualAlloc
```

**2. Creating XImage for Back Buffer**

```c
// Create XImage structure that wraps our pixel memory
buffer->info = XCreateImage(
    display,
    DefaultVisual(display, screen),  // Visual format
    DefaultDepth(display, screen),   // Bits per pixel (usually 24)
    ZPixmap,                         // Format: packed pixels
    0,                               // Offset (0 = start of data)
    (char *)buffer->memory,          // Our mmap'd pixel data
    width, height,
    32,                              // Bitmap pad (32-bit alignment)
    0                                // Bytes per line (0 = auto)
);
```

**3. Rendering the Weird Gradient**

```c
void render_weird_gradient(OffscreenBuffer *buffer, int x_offset, int y_offset) {
    uint8_t *row = (uint8_t *)buffer->memory;

    for (int y = 0; y < buffer->height; y++) {
        uint32_t *pixel = (uint32_t *)row;  // Cast row to 32-bit pixels

        for (int x = 0; x < buffer->width; x++) {
            // BGRA format (X11 native on most systems)
            uint8_t blue  = (uint8_t)(x + x_offset);
            uint8_t green = (uint8_t)(y + y_offset);
            uint8_t red   = 0;

            *pixel++ = (red << 16) | (green << 8) | blue;
            //         ↑ shift red to bits 16-23
            //                      ↑ shift green to bits 8-15
            //                                     ↑ blue in bits 0-7
        }
        row += buffer->pitch;  // Move to next row
        // ↑ pitch = width * bytes_per_pixel (handles alignment)
    }
}
```

**4. Blitting to Window**

```c
void update_window(OffscreenBuffer *buffer, Display *display,
                   Window window, GC gc, int window_width, int window_height) {
    // Blit our back buffer to the window
    XPutImage(
        display, window, gc,
        buffer->info,              // Our XImage
        0, 0,                      // Source x, y
        0, 0,                      // Dest x, y
        buffer->width, buffer->height
    );
    // Note: Could scale here if window size != buffer size
}
```

**5. GC Reuse Pattern (Wave 1 Resource)**

```c
// ❌ WRONG: Create/free every frame (causes use-after-free!)
void update_window_bad(...) {
    GC gc = XCreateGC(display, window, 0, NULL);
    XPutImage(..., gc, ...);
    XFreeGC(display, gc);  // 💥 Freed! Next frame crashes!
}

// ✅ CORRECT: Create once, reuse forever (Wave 1)
int main() {
    GC gc = XCreateGC(display, window, 0, NULL);  // Create once

    while (running) {
        update_window(..., gc, ...);  // Reuse every frame
    }
    // GC freed automatically when process exits (Wave 1)
}
```

#### ✅ Skills Acquired

- ✅ Double buffering (XImage + XPutImage)
- ✅ Pixel addressing (`y * width + x`)
- ✅ Memory optimization (`calloc` → `mmap`)
- ✅ Resource lifetime management
- ✅ Struct-based organization

---

### 📆 Day 6: Controller & Keyboard Input

**Focus:** Gamepad input (Linux joystick API), keyboard handling, input abstraction

#### 🎯 Core Concepts

| Concept               | Implementation                                    |
| --------------------- | ------------------------------------------------- |
| **Input Abstraction** | `GameControls` struct stores all button states    |
| **Linux Joystick**    | `/dev/input/js*`, `O_NONBLOCK`, `struct js_event` |
| **Non-blocking I/O**  | `open(..., O_NONBLOCK)` prevents game freezing    |
| **Bit Manipulation**  | `>> 12` for fast division, `& mask` for filtering |
| **Keyboard**          | `KeyPressMask`, `XLookupKeysym()`, track up/down  |

#### 🎮 Linux Joystick API Flow

```
┌─────────────────────────────────────────────────────────────┐
│                     GAME LOOP                               │
│  ┌─────────────┐    ┌──────────────┐    ┌───────────────┐  │
│  │ Poll Events │───►│ Read Joystick│───►│ Update State  │  │
│  │   (X11)     │    │ (O_NONBLOCK) │    │ (GameControls)│  │
│  └─────────────┘    └──────────────┘    └───────────────┘  │
│                              │                              │
│                              ▼                              │
│                     struct js_event {                       │
│                         time: u32,  // timestamp            │
│                         value: i16, // -32767 to +32767     │
│                         type: u8,   // JS_EVENT_BUTTON/AXIS │
│                         number: u8  // which button/axis    │
│                     }                                       │
└─────────────────────────────────────────────────────────────┘
```

#### 🔢 Bit Manipulation Explained

```
Analog Stick Value Conversion: >> 12
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Input:  -32767 ←─────────────┼────────────→ +32767  (stick range)
                             │
        >> 12 (divide by 4096)
                             │
                             ▼
Output:    -8 ←──────────────┼──────────────→ +8    (pixels/frame)

Why >> 12?
• 2^12 = 4096
• 32767 ÷ 4096 ≈ 8 pixels max speed
• 1 CPU cycle vs 20-40 for division!

Binary Example:
  16384 decimal = 0100 0000 0000 0000 binary
  >> 12         = 0000 0000 0000 0100 binary = 4 decimal
```

```
Event Type Filtering: & JS_EVENT_INIT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Event types:
  JS_EVENT_BUTTON = 0x01  (0000 0001)
  JS_EVENT_AXIS   = 0x02  (0000 0010)
  JS_EVENT_INIT   = 0x80  (1000 0000)  ← Synthetic startup event

Real button press:  type = 0x01         (0000 0001)
Init button event:  type = 0x81         (1000 0001)  ← Has INIT bit set!

Filtering:
  if (event.type & JS_EVENT_INIT) continue;  // Skip startup spam
     0x81 & 0x80 = 0x80 (non-zero = true, skip it!)
     0x01 & 0x80 = 0x00 (zero = false, process it!)
```

#### 🎮 PS4 Controller Mapping

```
┌──────────────────────────────────────────────────────────────┐
│                      PS4 CONTROLLER                          │
│                                                              │
│                    [L2=Axis3]  [R2=Axis4]                    │
│                    [L1=Btn4]  [R1=Btn5]                      │
│                                                              │
│    D-Pad                                         Buttons     │
│  (Axes 6-7)    [SHARE]  [PS=10]  [OPT]         ┌───┐        │
│    ┌───┐        [Btn8]           [Btn9]    [Y=2]│ △ │        │
│    │ ↑ │ Axis7=-1                          ┌───┼───┼───┐    │
│ ┌──┼───┼──┐                            [X=3]│ □ │   │ ○ │[B=1│
│ │← │   │ →│ Axis6=±1                       └───┼───┼───┘    │
│ └──┼───┼──┘                                    │ ✕ │[A=0]   │
│    │ ↓ │ Axis7=+1                              └───┘        │
│    └───┘                                                     │
│                                                              │
│      Left Stick              Right Stick                     │
│       (0,1)                    (2,5)  ← Note: Y is axis 5!   │
│        ○─────                  ─────○                        │
│       [L3=11]                 [R3=12]                        │
└──────────────────────────────────────────────────────────────┘

⚠️  PS4 D-pad is AXES (6-7), not buttons like Xbox!
⚠️  Right stick Y is axis 5, not 3 (triggers are 3-4)
```

| Component    | Type   | Number/Axis  | Notes           |
| ------------ | ------ | ------------ | --------------- |
| Cross (✕)    | Button | 0            | A equivalent    |
| Circle (○)   | Button | 1            | B equivalent    |
| Triangle (△) | Button | 2            | Y equivalent    |
| Square (□)   | Button | 3            | X equivalent    |
| L1/R1        | Button | 4, 5         | Bumpers         |
| L3/R3        | Button | 11, 12       | Stick clicks    |
| PS           | Button | 10           | Home button     |
| Left Stick   | Axes   | 0 (X), 1 (Y) |                 |
| Right Stick  | Axes   | 2 (X), 5 (Y) | ⚠️ Y is 5!      |
| L2/R2        | Axes   | 3, 4         | Triggers        |
| D-Pad        | Axes   | 6 (X), 7 (Y) | ⚠️ Not buttons! |

#### 🔄 Raylib vs X11 Gamepad Comparison

| Feature        | X11 (Raw)                    | Raylib                                      |
| -------------- | ---------------------------- | ------------------------------------------- |
| Detection      | Manual `/dev/input/js*` loop | `IsGamepadAvailable(0..3)`                  |
| Button names   | Raw numbers (0, 1, 2...)     | Semantic (`GAMEPAD_BUTTON_RIGHT_FACE_DOWN`) |
| Axis range     | -32767 to +32767             | -1.0 to +1.0                                |
| D-pad          | May be axes OR buttons       | Always buttons                              |
| Cross-platform | ❌ Linux only                | ✅ Windows/Mac/Linux                        |
| Learning value | ⭐⭐⭐ High                  | ⭐ Low (black box)                          |

#### 🐛 Common Pitfalls

| Issue              | Cause                      | Fix                                     |
| ------------------ | -------------------------- | --------------------------------------- |
| Keyboard ignored   | Missing `KeyPressMask`     | Add to `XSelectInput()`                 |
| D-pad doesn't work | PS4 uses axes, not buttons | Check axes 6-7 with threshold           |
| Game freezes       | Blocking `read()`          | Use `O_NONBLOCK` flag                   |
| Axis spam in logs  | Every tiny movement logs   | Only log buttons or significant changes |
| Wrong controller   | Virtual keyd device        | Skip devices with "virtual" in name     |

#### 💻 Code Snippets with Explanations

**1. Opening Joystick Device (Non-blocking)**

```c
#include <fcntl.h>      // O_RDONLY, O_NONBLOCK
#include <linux/joystick.h>  // struct js_event, JS_EVENT_*

int joystick_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
//                     ↑ device path      ↑ read    ↑ don't wait if no data
if (joystick_fd < 0) {
    // No controller connected - that's OK, continue without it
}
```

**2. Reading Joystick Events**

```c
struct js_event event;
while (read(joystick_fd, &event, sizeof(event)) == sizeof(event)) {
    // ↑ Non-blocking: returns immediately if no data (-1 with EAGAIN)

    // Skip synthetic init events (sent when device opens)
    if (event.type & JS_EVENT_INIT) continue;
    //            ↑ bitwise AND checks if INIT bit (0x80) is set

    if (event.type == JS_EVENT_BUTTON) {
        // event.number = which button (0, 1, 2...)
        // event.value  = 1 (pressed) or 0 (released)
        printf("Button %d: %s\n", event.number,
               event.value ? "pressed" : "released");
    }
    else if (event.type == JS_EVENT_AXIS) {
        // event.number = which axis (0=left X, 1=left Y, etc.)
        // event.value  = -32767 to +32767
        int pixels_per_frame = event.value >> 12;  // Fast divide by 4096
        //                                 ↑ converts to -8..+8 range
    }
}
```

**3. Keyboard Input with X11**

```c
// In XSelectInput() - MUST include these masks!
XSelectInput(display, window,
    ExposureMask | StructureNotifyMask |
    KeyPressMask | KeyReleaseMask);  // ← Required for keyboard!

// In event loop:
case KeyPress: {
    KeySym key = XLookupKeysym(&event.xkey, 0);
    //                                     ↑ shift state (0 = unshifted)
    switch (key) {
        case XK_w: case XK_Up:    state->controls.move_up = true; break;
        case XK_s: case XK_Down:  state->controls.move_down = true; break;
        case XK_a: case XK_Left:  state->controls.move_left = true; break;
        case XK_d: case XK_Right: state->controls.move_right = true; break;
        case XK_Escape: g_is_running = false; break;
    }
    break;
}
case KeyRelease: {
    KeySym key = XLookupKeysym(&event.xkey, 0);
    switch (key) {
        case XK_w: case XK_Up:    state->controls.move_up = false; break;
        // ... same pattern for other keys
    }
    break;
}
```

**4. Input State Struct (Casey's Pattern)**

```c
// Store input state, don't execute actions in event handlers!
typedef struct {
    // Digital inputs (keyboard/d-pad) - boolean
    bool move_up, move_down, move_left, move_right;
    bool action_a, action_b, action_x, action_y;

    // Analog inputs (sticks) - preserve full range
    int left_stick_x, left_stick_y;   // -32767 to +32767
    int right_stick_x, right_stick_y;

    // Triggers (analog on PS4/Xbox)
    int left_trigger, right_trigger;  // 0 to +32767
} GameControls;

// In game update (NOT in event handler):
void apply_controls(GameState *state) {
    int speed = 4;  // pixels per frame for digital input

    // Digital movement (keyboard/d-pad)
    if (state->controls.move_up)    state->gradient.y_offset -= speed;
    if (state->controls.move_down)  state->gradient.y_offset += speed;

    // Analog movement (sticks) - variable speed
    state->gradient.x_offset += state->controls.left_stick_x >> 12;
    //                          ↑ divide by 4096 for -8..+8 range
}
```

---

#### 🔧 Casey's Dynamic Loading Pattern (Day 6 Windows)

Casey uses macros to define function signatures once and generate typedefs, stubs, and function pointers. This pattern isn't needed for our Linux/Raylib implementation (we link statically), but understanding it is valuable.

**Why Dynamic Loading?**

```
Static Linking (what we do):
┌─────────────┐    compile    ┌─────────────┐
│ your_code.c │ ───────────► │  executable │ ← Contains Raylib code
└─────────────┘    time       └─────────────┘

Dynamic Loading (Casey's pattern):
┌─────────────┐    run        ┌─────────────┐    LoadLibrary()   ┌───────────┐
│ your_code.c │ ──────────►  │  executable │ ◄─────────────────► │ xinput.dll│
└─────────────┘    time       └─────────────┘                    └───────────┘
                                    │ GetProcAddress("XInputGetState")
                                    ▼
                              Function pointer
```

**The Macro Pattern:**

```c
// Step 1: Define function signature ONCE with a macro
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
//      ↑ macro name            ↑ return type   ↑ parameters

// Step 2: Create a typedef for function pointers
typedef X_INPUT_GET_STATE(x_input_get_state);
// Expands to: typedef DWORD WINAPI x_input_get_state(DWORD, XINPUT_STATE*);

// Step 3: Create a stub (fallback if DLL not found)
X_INPUT_GET_STATE(XInputGetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;  // Safe default
}
// Expands to: DWORD WINAPI XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE *pState)

// Step 4: Global function pointer (starts pointing to stub)
global_var x_input_get_state *XInputGetState_ = XInputGetStateStub;

// Step 5: At runtime, try to load real function
void load_xinput(void) {
    HMODULE lib = LoadLibraryA("xinput1_4.dll");
    if (lib) {
        XInputGetState_ = (x_input_get_state *)GetProcAddress(lib, "XInputGetState");
        if (!XInputGetState_) XInputGetState_ = XInputGetStateStub;  // Fallback
    }
}

// Step 6: Use it (works whether DLL loaded or not!)
XINPUT_STATE state;
XInputGetState_(0, &state);  // Calls real function OR stub
```

**Why This Pattern is Brilliant:**

| Benefit                     | Explanation                       |
| --------------------------- | --------------------------------- |
| **Graceful degradation**    | Game runs even without XInput DLL |
| **No link-time dependency** | Don't need xinput.lib to compile  |
| **Single source of truth**  | Change signature in ONE place     |
| **Runtime flexibility**     | Load different DLL versions       |
| **Testability**             | Easy to mock with stub functions  |

**Linux Equivalent (if we needed it):**

```c
#include <dlfcn.h>

void *lib = dlopen("libSDL2.so", RTLD_LAZY);
if (lib) {
    // Cast to function pointer type
    int (*SDL_Init)(int) = dlsym(lib, "SDL_Init");
}
```

**But we don't need it because:**

- Raylib is statically linked (compiled into our executable)
- Linux joystick uses file I/O (`open`/`read`), not a library

---

#### ✅ Skills Acquired

- ✅ Linux joystick API (`/dev/input/js*`)
- ✅ Non-blocking I/O (`O_NONBLOCK`)
- ✅ Bit manipulation (`>> 12`, `& mask`)
- ✅ Input state management (poll → store → apply)
- ✅ Cross-platform gamepad (Raylib)
- ✅ X11 keyboard handling (`XLookupKeysym`)
- ✅ Understanding Casey's macro/dynamic loading pattern

---

### 📆 Day 7-9: Audio System - From Silence to Sine Waves 🔊

**Focus:** Implement cross-platform audio output with ALSA (Linux) and Raylib, progressing from square waves to sine waves with real-time control.

---

#### 🗓️ Commits

| Date         | Commit    | What Changed                                                                                                                                                                                         |
| ------------ | --------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Dec 5, 2025  | `9aa90bf` | **Day 7: ALSA Audio Foundation**<br>• Dynamic library loading (`dlopen`)<br>• PCM device initialization<br>• Audio parameter setup (48kHz, 16-bit stereo)                                            |
| Dec 10, 2025 | `e3e9544` | **Day 8: Square Wave & Controls**<br>• Ring buffer implementation<br>• Square wave generation<br>• Musical keyboard (Z-X-C-V-B-N-M)<br>• Volume & pan control<br>• Analog stick frequency modulation |
| Dec 11, 2025 | `ed5f86c` | **Day 9: Sine Wave Synthesis**<br>• Phase accumulator system<br>• Replace square wave with `sinf()`<br>• Latency calculation (1/15 sec)<br>• Phase wrapping to prevent overflow                      |
| Dec 12, 2025 | `3d4b6eb` | **Day 9: Raylib Audio Port**<br>• Mirror X11 implementation to Raylib<br>• AudioStream callback system<br>• Cross-platform feature parity                                                            |

---

#### 📊 Audio Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        AUDIO SYSTEM OVERVIEW                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Casey's Windows (DirectSound)         Your Linux (ALSA)                    │
│  ──────────────────────────────         ────────────────────                │
│  1. LoadLibrary("dsound.dll")           dlopen("libasound.so")              │
│  2. DirectSoundCreate()                 snd_pcm_open()                      │
│  3. SetCooperativeLevel()               (not needed)                        │
│  4. CreateSoundBuffer()                 snd_pcm_set_params()                │
│     ├─ Primary Buffer (format)          ├─ Sets format directly             │
│     └─ Secondary Buffer (data)          └─ Internal ring buffer             │
│  5. Lock() → Write → Unlock()           snd_pcm_writei() (simpler!)        │
│                                                                             │
│  Your Raylib (Cross-Platform)                                               │
│  ─────────────────────────────                                              │
│  1. InitAudioDevice()                   ← Built-in!                         │
│  2. LoadAudioStream(48000, 16, 2)       ← One function call                 │
│  3. SetAudioStreamCallback()            ← Automatic filling                 │
│  4. PlayAudioStream()                   ← Start playback                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

#### 🎯 Core Concepts

| Concept             | Windows (Casey)                      | Linux (Your ALSA)      | Raylib (Your Port)           |
| ------------------- | ------------------------------------ | ---------------------- | ---------------------------- |
| **Library Loading** | `LoadLibrary()` + `GetProcAddress()` | `dlopen()` + `dlsym()` | Built-in                     |
| **Device Init**     | `DirectSoundCreate()`                | `snd_pcm_open()`       | `InitAudioDevice()`          |
| **Format Setup**    | `WAVEFORMATEX` struct                | `snd_pcm_set_params()` | `LoadAudioStream()` params   |
| **Buffer Model**    | Primary + Secondary                  | Single ring buffer     | Callback-based               |
| **Write Pattern**   | Lock → Copy → Unlock                 | `snd_pcm_writei()`     | Callback fills automatically |
| **Error Recovery**  | Manual state tracking                | `snd_pcm_recover()`    | Automatic                    |
| **Sample Rate**     | 48000 Hz                             | 48000 Hz               | 48000 Hz                     |
| **Bit Depth**       | 16-bit signed                        | 16-bit signed LE       | 16-bit signed                |
| **Channels**        | 2 (stereo)                           | 2 (interleaved L-R)    | 2 (stereo)                   |
| **Latency**         | ~66ms (1/15 sec)                     | ~50ms (configurable)   | ~85ms (4096 frames)          |

---

#### 🔊 Day 7: Audio Initialization (X11/ALSA)

**Challenge:** Initialize ALSA without crashing if library missing (Casey's philosophy: graceful degradation)

##### Visual: Dynamic Library Loading Pattern

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CASEY'S DYNAMIC LOADING PATTERN                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  WHY: Don't crash if audio library missing!                                 │
│                                                                             │
│  STEP 1: Define function signature macros                                   │
│  ──────────────────────────────────────                                     │
│  #define ALSA_SND_PCM_OPEN(name) \                                          │
│      int name(snd_pcm_t **pcm, const char *device, ...)                     │
│                                                                             │
│  STEP 2: Create typedef                                                     │
│  ──────────────────────                                                     │
│  typedef ALSA_SND_PCM_OPEN(alsa_snd_pcm_open);                              │
│                                                                             │
│  STEP 3: Stub implementation (fallback)                                     │
│  ───────────────────────────────────────                                    │
│  ALSA_SND_PCM_OPEN(AlsaSndPcmOpenStub) {                                    │
│      return -1; // Pretend device not found                                 │
│  }                                                                          │
│                                                                             │
│  STEP 4: Global function pointer (starts as stub)                           │
│  ─────────────────────────────────────────────                              │
│  alsa_snd_pcm_open *SndPcmOpen_ = AlsaSndPcmOpenStub;                       │
│                                                                             │
│  STEP 5: Try to load real function                                          │
│  ──────────────────────────────────                                         │
│  void *lib = dlopen("libasound.so.2", RTLD_LAZY);                           │
│  if (lib) {                                                                 │
│      SndPcmOpen_ = (alsa_snd_pcm_open*)dlsym(lib, "snd_pcm_open");         │
│  }                                                                          │
│  // If dlsym fails, stub remains! No crash! ✅                              │
│                                                                             │
│  STEP 6: Use clean API name                                                 │
│  ───────────────────────────                                                │
│  #define SndPcmOpen SndPcmOpen_                                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

##### Code Snippet 1: ALSA Initialization (audio.c)

```c
// filepath: project/src/platform/x11/audio.c

void linux_load_alsa(void) {
    printf("Loading ALSA library...\n");

    // ═══════════════════════════════════════════════════════════
    // Try multiple library names (Casey's pattern)
    // ═══════════════════════════════════════════════════════════
    // Like Casey tries xinput1_4.dll, then xinput1_3.dll
    void *alsa_lib = dlopen("libasound.so.2", RTLD_LAZY);
    if (!alsa_lib) {
        alsa_lib = dlopen("libasound.so", RTLD_LAZY);
    }

    if (!alsa_lib) {
        fprintf(stderr, "❌ ALSA: Could not load libasound.so: %s\n", dlerror());
        fprintf(stderr, "   Audio disabled. Install: sudo apt install libasound2\n");
        return; // Stubs remain - audio just won't work
    }

    // ═══════════════════════════════════════════════════════════
    // Load function pointers (Casey's GetProcAddress pattern)
    // ═══════════════════════════════════════════════════════════
    #define LOAD_ALSA_FN(fn_ptr, fn_name, type) \
        fn_ptr = (type*)dlsym(alsa_lib, fn_name); \
        if (!fn_ptr) { \
            fprintf(stderr, "❌ ALSA: Could not load %s\n", fn_name); \
        }

    LOAD_ALSA_FN(SndPcmOpen_, "snd_pcm_open", alsa_snd_pcm_open);
    LOAD_ALSA_FN(SndPcmSetParams_, "snd_pcm_set_params", alsa_snd_pcm_set_params);
    LOAD_ALSA_FN(SndPcmWritei_, "snd_pcm_writei", alsa_snd_pcm_writei);
    // ... more functions ...

    #undef LOAD_ALSA_FN
}

void linux_init_sound(int32_t samples_per_second, int32_t buffer_size_bytes) {
    // ═══════════════════════════════════════════════════════════
    // STEP 1: Open PCM device (Casey's DirectSoundCreate)
    // ═══════════════════════════════════════════════════════════
    int err = SndPcmOpen(&g_sound_output.handle,
                         "default",                     // System default device
                         LINUX_SND_PCM_STREAM_PLAYBACK, // Output
                         0);                            // Blocking mode

    if (err < 0) {
        fprintf(stderr, "❌ Sound: Cannot open audio device: %s\n", SndStrerror(err));
        g_sound_output.is_valid = false;
        return;
    }

    // ═══════════════════════════════════════════════════════════
    // STEP 2: Set format parameters (Casey's WAVEFORMATEX)
    // ═══════════════════════════════════════════════════════════
    // Casey's values:
    //   wBitsPerSample = 16
    //   nChannels = 2
    //   nSamplesPerSec = 48000
    //   nBlockAlign = 4 (2 channels × 2 bytes)
    //
    // ALSA does it all in one call!
    unsigned int latency_us = 50000; // 50ms buffer (Casey uses ~66ms)

    err = SndPcmSetParams(g_sound_output.handle,
                          LINUX_SND_PCM_FORMAT_S16_LE,         // 16-bit signed little-endian
                          LINUX_SND_PCM_ACCESS_RW_INTERLEAVED, // L-R-L-R format
                          2,                                   // Stereo
                          samples_per_second,                  // 48000 Hz
                          1,                                   // Allow resampling
                          latency_us);                         // 50ms latency

    if (err < 0) {
        fprintf(stderr, "❌ Sound: Cannot set parameters: %s\n", SndStrerror(err));
        SndPcmClose(g_sound_output.handle);
        g_sound_output.is_valid = false;
        return;
    }

    g_sound_output.is_valid = true;
    printf("✅ Sound: Initialized at %d Hz, 16-bit stereo\n", samples_per_second);
}
```

**Why This Works:**

- ✅ No compile-time dependency on `-lasound`
- ✅ Graceful degradation if ALSA missing
- ✅ Exact mirror of Casey's `Win32LoadXInput` pattern
- ✅ Function pointers allow hot-swapping implementations

---

#### 🎵 Day 8: Square Wave Generation

**Challenge:** Implement Casey's ring buffer pattern and generate square wave audio

##### Visual: Square Wave Mathematics

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      SQUARE WAVE (256 Hz at 48kHz)                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Casey's Formula:                                                           │
│  ───────────────                                                            │
│  SampleValue = ((RunningSampleIndex / HalfSquareWavePeriod) % 2)           │
│                ? ToneVolume : -ToneVolume                                   │
│                                                                             │
│  Breakdown:                                                                 │
│  ──────────                                                                 │
│  1. WavePeriod = 48000 / 256 = 187.5 samples per cycle                     │
│  2. HalfPeriod = 187.5 / 2 = 93.75 samples per half-cycle                  │
│  3. RunningSampleIndex / 93.75 gives which half we're in                   │
│  4. % 2 toggles between 0 and 1                                             │
│  5. Ternary picks +3000 or -3000                                            │
│                                                                             │
│  Visual Output:                                                             │
│  ──────────────                                                             │
│  +3000 ┌────────┐          ┌────────┐          ┌────────┐                  │
│        │        │          │        │          │        │                  │
│      0 ┤        └──────────┘        └──────────┘        └──                │
│        │                                                                    │
│  -3000 └────────────────────────────────────────────────────────           │
│                                                                             │
│        ◄────────────────────────────────────────────►                       │
│         One period = 187.5 samples ≈ 3.9ms at 48kHz                        │
│         Frequency = 1 / 3.9ms = 256 Hz ✅                                   │
│                                                                             │
│  Sample Timeline:                                                           │
│  ────────────────                                                           │
│  Sample 0-93:    +3000 (first half - HIGH)                                 │
│  Sample 94-187:  -3000 (second half - LOW)                                 │
│  Sample 188-281: +3000 (next period starts)                                │
│  ...                                                                        │
│                                                                             │
│  Why 48kHz?                                                                 │
│  ──────────                                                                 │
│  • Higher than CD quality (44.1kHz)                                         │
│  • Professional audio standard                                              │
│  • Allows frequencies up to 24kHz (Nyquist theorem: max = sample_rate/2)   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

##### Code Snippet 2: Square Wave Generation

```c
// filepath: project/src/platform/x11/audio.c

void linux_fill_sound_buffer(void) {
    if (!g_sound_output.is_valid) return;

    // ═══════════════════════════════════════════════════════════
    // STEP 1: Check how many frames ALSA can accept
    // ═══════════════════════════════════════════════════════════
    // Like Casey's GetCurrentPosition() in DirectSound
    long frames_available = SndPcmAvail(g_sound_output.handle);

    if (frames_available < 0) {
        // ALSA underrun occurred - recover!
        int err = SndPcmRecover(g_sound_output.handle, (int)frames_available, 1);
        if (err < 0) {
            fprintf(stderr, "❌ Sound: Recovery failed: %s\n", SndStrerror(err));
            return;
        }
        frames_available = SndPcmAvail(g_sound_output.handle);
    }

    // Don't write more than our buffer can hold
    if (frames_available > (long)g_sound_output.sample_buffer_size) {
        frames_available = g_sound_output.sample_buffer_size;
    }

    if (frames_available <= 0) return; // Buffer full

    // ═══════════════════════════════════════════════════════════
    // STEP 2: Generate square wave samples (Casey's Day 8 formula)
    // ═══════════════════════════════════════════════════════════
    int16_t *sample_out = g_sound_output.sample_buffer;

    for (long i = 0; i < frames_available; ++i) {
        // Casey's exact formula:
        int16_t sample_value = ((g_sound_output.running_sample_index /
                                 g_sound_output.half_wave_period) % 2)
                               ? g_sound_output.tone_volume
                               : -g_sound_output.tone_volume;

        // Apply panning (your extension!)
        int left_gain = (100 - g_sound_output.pan_position);   // 0 to 200
        int right_gain = (100 + g_sound_output.pan_position);  // 0 to 200

        *sample_out++ = (sample_value * left_gain) / 200;   // Left channel
        *sample_out++ = (sample_value * right_gain) / 200;  // Right channel

        // Why divide by 200?
        // Gains range from 0-200, we want 100% = 200/200 = 1.0

        g_sound_output.running_sample_index++;
    }

    // ═══════════════════════════════════════════════════════════
    // STEP 3: Write samples to ALSA (simpler than DirectSound!)
    // ═══════════════════════════════════════════════════════════
    long frames_written = SndPcmWritei(
        g_sound_output.handle,
        g_sound_output.sample_buffer,
        frames_available
    );

    if (frames_written < 0) {
        // Handle errors (underrun, etc.)
        SndPcmRecover(g_sound_output.handle, (int)frames_written, 1);
    }
}
```

**Key Difference from DirectSound:**

- ✅ **No Lock/Unlock needed** - ALSA copies data internally
- ✅ **No Region1/Region2 wrap-around** - ALSA handles ring buffer logic
- ✅ **Automatic error recovery** - `snd_pcm_recover()` fixes underruns
- ✅ **Simpler API** - One function call vs Casey's multi-step Lock/Unlock

##### Code Snippet 3: Musical Keyboard Control

```c
// filepath: project/src/platform/x11/backend.c

// ═══════════════════════════════════════════════════════════════
// 🎹 Musical note frequencies (Equal Temperament)
// ═══════════════════════════════════════════════════════════════
// Formula: f(n) = 440 * 2^((n-49)/12)
// Where n is the key number (A4 = 440Hz is key 49)
//
// C4 = 261.63 Hz (middle C)
// A4 = 440.00 Hz (concert pitch)
// C5 = 523.25 Hz (one octave above middle C)
// ═══════════════════════════════════════════════════════════════

inline file_scoped_fn void handle_musical_keypress(KeySym keysym) {
    switch (keysym) {
        case XK_z: set_tone_frequency(262); printf("🎵 Note: C4 (261.63 Hz)\n"); break;
        case XK_x: set_tone_frequency(294); printf("🎵 Note: D4 (293.66 Hz)\n"); break;
        case XK_c: set_tone_frequency(330); printf("🎵 Note: E4 (329.63 Hz)\n"); break;
        case XK_v: set_tone_frequency(349); printf("🎵 Note: F4 (349.23 Hz)\n"); break;
        case XK_b: set_tone_frequency(392); printf("🎵 Note: G4 (392.00 Hz)\n"); break;
        case XK_n: set_tone_frequency(440); printf("🎵 Note: A4 (440.00 Hz) - Concert Pitch\n"); break;
        case XK_m: set_tone_frequency(494); printf("🎵 Note: B4 (493.88 Hz)\n"); break;
        case XK_comma: set_tone_frequency(523); printf("🎵 Note: C5 (523.25 Hz)\n"); break;
    }
}

inline void set_tone_frequency(int hz) {
    g_sound_output.tone_hz = hz;
    g_sound_output.wave_period = g_sound_output.samples_per_second / hz;
    g_sound_output.half_wave_period = g_sound_output.wave_period / 2;

    // Optional: Reset phase to avoid clicks
    g_sound_output.running_sample_index = 0;
}
```

**Why This Keyboard Layout?**

- ✅ **Z-X-C-V-B-N-M-Comma** = Bottom row of keyboard
- ✅ **Matches piano white keys** (C-D-E-F-G-A-B-C)
- ✅ **No modifier keys** = instant response
- ✅ **Easy to play melodies** (e.g., Z-X-C = C-D-E major chord)

---

#### 🌊 Day 9: Sine Wave Synthesis

**Challenge:** Replace harsh square wave with smooth sine wave using phase accumulator

##### Visual: Phase Accumulator Explained

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        PHASE ACCUMULATOR (t_sine)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  What is it?                                                                │
│  ───────────                                                                │
│  A variable that tracks "where we are" in the sine wave cycle.              │
│  Like a clock hand rotating around a circle!                                │
│                                                                             │
│  Sine Wave Cycle:                                                           │
│  ────────────────                                                           │
│                                                                             │
│   t_sine = 0       → sin(0)      = 0.0      (start)                         │
│   t_sine = π/2     → sin(π/2)    = +1.0     (peak)                          │
│   t_sine = π       → sin(π)      = 0.0      (middle)                        │
│   t_sine = 3π/2    → sin(3π/2)   = -1.0     (trough)                        │
│   t_sine = 2π      → sin(2π)     = 0.0      (end = start of next cycle)    │
│                                                                             │
│  Visual Representation:                                                     │
│  ──────────────────                                                         │
│                                                                             │
│            π/2 (peak)                                                       │
│             ↑                                                               │
│             │                                                               │
│   π ←───────┼───────→ 0 / 2π                                               │
│   (middle)  │         (start/end)                                           │
│             │                                                               │
│             ↓                                                               │
│           3π/2 (trough)                                                     │
│                                                                             │
│  Casey's Increment Formula:                                                 │
│  ───────────────────────────                                                │
│  t_sine += 2π / WavePeriod                                                  │
│            ─────────────────                                                │
│            How much of a full cycle to advance per sample                   │
│                                                                             │
│  Example (256 Hz, 48000 Hz sample rate):                                    │
│  ───────────────────────────────────────────                                │
│  WavePeriod = 48000 / 256 = 187.5 samples per cycle                        │
│  Increment  = 2π / 187.5 ≈ 0.0335 radians per sample                       │
│                                                                             │
│  Sample Timeline:                                                           │
│  ────────────────                                                           │
│  Sample 0:    t_sine = 0          → sin(0)      = 0.0                      │
│  Sample 1:    t_sine = 0.0335     → sin(0.0335) ≈ 0.0335                   │
│  Sample 94:   t_sine ≈ π/2        → sin(π/2)    = 1.0    ← PEAK!           │
│  Sample 187:  t_sine ≈ 2π         → sin(2π)     = 0.0    ← Cycle done!     │
│  Sample 188:  t_sine = 0.0335     → Next cycle starts                       │
│                                                                             │
│  Why wrap at 2π?                                                            │
│  ───────────────                                                            │
│  • Prevents float overflow (sin() is periodic: sin(x) = sin(x + 2π))       │
│  • Keeps precision high (large floats lose accuracy)                        │
│  • Mathematically cleaner                                                   │
│                                                                             │
│  BUT: Casey doesn't wrap in Day 9! Why?                                     │
│  ───────────────────────────────────────────                                │
│  • Float can represent numbers up to ~3.4 × 10³⁸                            │
│  • At 48kHz, takes YEARS to overflow                                        │
│  • Modern CPUs handle sinf(huge_number) fine                                │
│  • Simpler code (no extra conditional per sample)                           │
│                                                                             │
│  Both approaches valid - choose based on philosophy:                        │
│  • Wrap = Mathematically pure, prevents precision loss                      │
│  • No wrap = Simpler, Casey's pragmatic approach                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

##### Code Snippet 4: Sine Wave Generation (The Bug Fix!)

```c
// filepath: project/src/platform/x11/audio.c

#include <math.h>  // For sinf()

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_double_PI
#define M_double_PI (2.0f * M_PI)
#endif

void linux_fill_sound_buffer(void) {
    // ...existing frame availability check...

    int16_t *sample_out = g_sound_output.sample_buffer;

    for (long i = 0; i < frames_available; ++i) {
        // ═══════════════════════════════════════════════════════════
        // 🆕 Day 9: Generate sine wave sample
        // ═══════════════════════════════════════════════════════════
        // Casey's exact formula:
        //   SineValue = sinf(tSine);
        //   SampleValue = (int16)(SineValue * ToneVolume);
        //   tSine += 2π / WavePeriod;
        // ═══════════════════════════════════════════════════════════

        real32 sine_value = sinf(g_sound_output.t_sine);
        int16_t sample_value = (int16_t)(sine_value * g_sound_output.tone_volume);

        // Apply panning
        int left_gain = (100 - g_sound_output.pan_position);
        int right_gain = (100 + g_sound_output.pan_position);

        *sample_out++ = (sample_value * left_gain) / 200;   // Left
        *sample_out++ = (sample_value * right_gain) / 200;  // Right

        // ═══════════════════════════════════════════════════════════
        // ⚠️ CRITICAL: Use += not = !!!
        // ═══════════════════════════════════════════════════════════
        // ❌ WRONG: g_sound_output.t_sine = (2.0f * M_PI) / ...
        //    This REPLACES t_sine with the same value every sample!
        //    Result: sin(0.0335) every time → nearly constant → silence!
        //
        // ✅ CORRECT: g_sound_output.t_sine += (2.0f * M_PI) / ...
        //    This ADDS to t_sine, making it grow over time
        //    Result: sin(0), sin(0.0335), sin(0.067), ... → wave! 🔊
        // ═══════════════════════════════════════════════════════════
        g_sound_output.t_sine += M_double_PI / (float)g_sound_output.wave_period;

        // Optional: Wrap to [0, 2π) range to prevent overflow
        if (g_sound_output.t_sine >= M_double_PI) {
            g_sound_output.t_sine -= M_double_PI;
        }

        g_sound_output.running_sample_index++;
    }

    // ...existing write code...
}
```

**The Most Common Bug:**

```c
// ❌ YOUR ORIGINAL BUG (the `=` vs `+=` mistake):
g_sound_output.t_sine = (2.0f * M_PI) / (float)g_sound_output.wave_period;
//                      ▲
//                      Assignment! Sets to SAME value every sample!

// ✅ CORRECT (accumulate over time):
g_sound_output.t_sine += (2.0f * M_PI) / (float)g_sound_output.wave_period;
//                      ▲▲
//                      Addition assignment! Grows over time!
```

**Why This Bug Causes Silence:**

```
With `=`: t_sine = 0.0335 → sin(0.0335) ≈ 0.0335 → sample ≈ 201
         t_sine = 0.0335 → sin(0.0335) ≈ 0.0335 → sample ≈ 201
         t_sine = 0.0335 → sin(0.0335) ≈ 0.0335 → sample ≈ 201
         ↑ Speaker moves to position 201 and STAYS THERE → no sound!

With `+=`: t_sine = 0.0000 → sin(0.0000) = 0.0000 → sample = 0
           t_sine = 0.0335 → sin(0.0335) ≈ 0.0335 → sample ≈ 201
           t_sine = 0.0670 → sin(0.0670) ≈ 0.0670 → sample ≈ 402
           ↑ Speaker OSCILLATES back and forth → audible tone! 🔊
```

---

#### 🎮 Day 9: Raylib Audio Port

**Challenge:** Mirror X11 implementation to Raylib with feature parity

##### Visual: Callback System Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     X11/ALSA vs RAYLIB AUDIO MODELS                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  X11/ALSA (Manual Polling):                                                 │
│  ──────────────────────────────                                             │
│  while (running) {                                                          │
│      handle_events();                                                       │
│      render_frame();                                                        │
│      linux_fill_sound_buffer();  ← YOU call this every frame               │
│  }                                                                          │
│                                                                             │
│  Pros: ✅ Full control over timing                                          │
│  Cons: ❌ Must call manually every frame                                    │
│        ❌ Risk underruns if frame takes too long                            │
│                                                                             │
│  ─────────────────────────────────────────────────────────────             │
│                                                                             │
│  Raylib (Callback System):                                                  │
│  ──────────────────────────                                                 │
│  SetAudioStreamCallback(stream, raylib_audio_callback);                     │
│  PlayAudioStream(stream);                                                   │
│                                                                             │
│  while (running) {                                                          │
│      handle_events();                                                       │
│      render_frame();                                                        │
│      UpdateAudioStream(stream, NULL, 0);  ← Just keep stream alive         │
│  }                                                                          │
│                                                                             │
│  // Raylib calls raylib_audio_callback() AUTOMATICALLY when buffer needs data!│
│  void raylib_audio_callback(void *buffer, unsigned int frames) {           │
│      // Fill buffer (same logic as linux_fill_sound_buffer)                 │
│  }                                                                          │
│                                                                             │
│  Pros: ✅ Automatic buffer filling                                          │
│        ✅ Lower latency (runs in audio thread)                              │
│        ✅ No underruns from slow frames                                     │
│  Cons: ❌ Less control over exact timing                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

##### Code Snippet 5: Raylib Audio Callback

```c
// filepath: project/src/platform/raylib/audio.c

void raylib_audio_callback(void *buffer, unsigned int frames) {
    if (!g_sound_output.is_initialized) return;

    int16_t *sample_out = (int16_t *)buffer;

    for (unsigned int i = 0; i < frames; ++i) {
        // ═══════════════════════════════════════════════════════════
        // SAME logic as X11 version - just in a callback!
        // ═══════════════════════════════════════════════════════════
        real32 sine_value = sinf(g_sound_output.t_sine);
        int16_t sample_value = (int16_t)(sine_value * g_sound_output.tone_volume);

        // Apply panning
        int left_gain = (100 - g_sound_output.pan_position);
        int right_gain = (100 + g_sound_output.pan_position);

        *sample_out++ = (sample_value * left_gain) / 200;   // Left
        *sample_out++ = (sample_value * right_gain) / 200;  // Right

        // Increment phase accumulator
        g_sound_output.t_sine += M_double_PI / (real32)g_sound_output.wave_period;

        // Wrap to prevent overflow
        if (g_sound_output.t_sine >= M_double_PI) {
            g_sound_output.t_sine -= M_double_PI;
        }

        g_sound_output.running_sample_index++;
    }
}

void raylib_init_audio(void) {
    InitAudioDevice();

    if (!IsAudioDeviceReady()) {
        fprintf(stderr, "❌ Audio: Device initialization failed\n");
        return;
    }

    // Setup parameters (same as X11)
    g_sound_output.samples_per_second = 48000;
    g_sound_output.tone_hz = 256;
    g_sound_output.tone_volume = 6000;
    g_sound_output.wave_period =
        g_sound_output.samples_per_second / g_sound_output.tone_hz;
    g_sound_output.t_sine = 0.0f;

    // Create audio stream
    g_sound_output.stream = LoadAudioStream(48000, 16, 2);

    // Attach callback (magic happens here!)
    SetAudioStreamCallback(g_sound_output.stream, raylib_audio_callback);

    // Start playback
    PlayAudioStream(g_sound_output.stream);

    g_sound_output.is_initialized = true;
}
```

**Lines of Code Comparison:**

- X11/ALSA: ~450 lines (dynamic loading + manual buffer management)
- Raylib: ~200 lines (built-in + callback system)
- **50% reduction** while maintaining identical audio quality!

---

#### 🐛 Common Pitfalls

| Issue                                | Cause                                    | Fix                                              | Days Affected |
| ------------------------------------ | ---------------------------------------- | ------------------------------------------------ | ------------- |
| **No sound output**                  | Used `=` instead of `+=` for `t_sine`    | Change to `t_sine +=` (accumulate!)              | Day 9         |
| **Audio underruns**                  | Buffer too small, frame takes too long   | Increase `SetAudioStreamBufferSizeDefault(8192)` | Day 7-9       |
| **Clicking when changing frequency** | Phase discontinuity                      | Reset `t_sine = 0` in `set_tone_frequency()`     | Day 8-9       |
| **Left/Right reversed**              | Swapped channel order                    | Check `*sample_out++` order (L then R)           | Day 8         |
| **Volume too loud (distortion)**     | `tone_volume > 10000` clips 16-bit range | Clamp to `[-10000, 10000]`                       | Day 8-9       |
| **Panning doesn't work**             | Forgot to divide gains by 200            | `(sample * gain) / 200`                          | Day 8         |
| **Frequency off-pitch**              | Used `int` division instead of `float`   | `(float)samples_per_second / frequency`          | Day 8-9       |
| **Sine sounds like square**          | Forgot `#include <math.h>`               | Add include, link with `-lm`                     | Day 9         |

---

#### ✅ Skills Acquired

**Day 7:**

- ✅ Dynamic library loading (`dlopen`/`dlsym` vs `LoadLibrary`/`GetProcAddress`)
- ✅ Function pointer patterns for graceful degradation
- ✅ ALSA PCM device initialization
- ✅ Audio format negotiation (sample rate, bit depth, channels)
- ✅ Ring buffer concepts (Casey's DirectSound model)

**Day 8:**

- ✅ Square wave generation with integer math
- ✅ Musical note frequency calculation (equal temperament)
- ✅ Stereo panning (linear gain model)
- ✅ Real-time frequency modulation (analog stick control)
- ✅ Keyboard input for musical notes
- ✅ Audio underrun detection and recovery

**Day 9:**

- ✅ Phase accumulator system for sine wave synthesis
- ✅ Understanding `sinf()` vs lookup tables (trade-offs)
- ✅ Float precision management (wrapping phase vs letting it grow)
- ✅ The critical difference between `=` and `+=`
- ✅ Latency calculation (samples ahead vs milliseconds)
- ✅ Cross-platform audio abstraction (Raylib port)
- ✅ Callback-based audio systems vs manual polling

**Casey's Core Philosophy Applied:**

- ✅ **"Make it work, then make it right, then make it fast"** - Square wave first, sine wave later
- ✅ **Graceful degradation** - Stub functions when libraries missing
- ✅ **Platform abstraction** - Same logic, different APIs
- ✅ **Incremental development** - Each day builds on previous

---

#### 📊 Implementation Comparison Matrix

| Feature           | Windows (Casey)       | X11/ALSA (Yours)       | Raylib (Yours)             | Complexity                   |
| ----------------- | --------------------- | ---------------------- | -------------------------- | ---------------------------- |
| Library Loading   | `LoadLibrary()`       | `dlopen()`             | Built-in                   | X11: Medium, Raylib: Easy    |
| Function Pointers | `GetProcAddress()`    | `dlsym()`              | N/A                        | X11: Medium, Raylib: N/A     |
| Device Init       | `DirectSoundCreate()` | `snd_pcm_open()`       | `InitAudioDevice()`        | All: Easy                    |
| Format Setup      | `WAVEFORMATEX` struct | `snd_pcm_set_params()` | `LoadAudioStream()` params | All: Easy                    |
| Buffer Model      | Primary + Secondary   | Single ring            | Callback-managed           | X11: Medium, Raylib: Easy    |
| Write Pattern     | Lock → Copy → Unlock  | `snd_pcm_writei()`     | Callback auto-fills        | X11: Medium, Raylib: Easiest |
| Error Recovery    | Manual state check    | `snd_pcm_recover()`    | Automatic                  | X11: Medium, Raylib: Auto    |
| Lines of Code     | ~300                  | ~450                   | ~200                       | Raylib wins!                 |
| Platform Support  | Windows only          | Linux only             | Cross-platform             | Raylib wins!                 |
| Learning Value    | ⭐⭐⭐⭐⭐            | ⭐⭐⭐⭐⭐             | ⭐⭐⭐                     | Casey wins!                  |

---

#### 🎓 Deep Dive: Why Casey Uses This Audio Model

**Casey's Design Decisions:**

1. **Why DirectSound?** (2014)

   - Low-level control (no audio engine overhead)
   - Predictable latency
   - Ring buffer model teaches fundamentals
   - **Modern equivalent:** WASAPI (Windows), ALSA (Linux), Core Audio (macOS)

2. **Why 48kHz instead of 44.1kHz?**

   - Professional audio standard
   - Better high-frequency response
   - Easier math (48000 / 256 = 187.5 vs 44100 / 256 = 172.265...)
   - **Trade-off:** Slightly more CPU (negligible on modern hardware)

3. **Why square wave before sine?**

   - Simpler math (integer only, no `sinf()`)
   - Teaches wave period calculation
   - Debugging easier (binary high/low vs continuous)
   - **Philosophy:** Start simple, add complexity

4. **Why 16-bit audio?**

   - CD quality (16-bit = 96dB dynamic range)
   - Games don't need 24-bit studio quality
   - Half the memory bandwidth of 32-bit float
   - **Casey's rule:** "Good enough" beats "perfect"

5. **Why phase accumulator?**
   - Smooth frequency changes (no clicks)
   - Simple to understand (just adding!)
   - Matches analog synthesizer design
   - **Alternative:** Lookup tables (faster but less flexible)

---

#### 📖 Further Reading

**Casey's Handmade Hero Days:**

- Day 7: "Initializing DirectSound" (~1 hour)
- Day 8: "Writing a Square Wave to DirectSound" (~1.5 hours)
- Day 9: "Variable-Pitch Sine Wave Output" (~1 hour)

**ALSA Documentation:**

- [ALSA PCM Tutorial](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html)
- [Understanding ALSA Ring Buffers](<https://www.alsa-project.org/main/index.php/Asynchronous_Playback_(Howto)>)

**Audio DSP Fundamentals:**

- [Sample Rate & Nyquist Theorem](https://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem)
- [Phase Accumulator Synthesis](https://ccrma.stanford.edu/~jos/pasp/)
- [Equal Temperament Tuning](https://pages.mtu.edu/~suits/NoteFreqCalcs.html)

**Raylib Audio:**

- [Raylib Audio Stream Examples](https://github.com/raysan5/raylib/blob/master/examples/audio/)
- [AudioStream API Reference](https://www.raylib.com/cheatsheet/cheatsheet.html)

---

### 📆 Day 10: Audio Latency Measurement and Performance Timing

**Focus:** Implement precise audio latency control using `snd_pcm_delay()`, add frame timing measurements, and create debugging tools for audio system monitoring.

---

#### 🗓️ Commits

| Date         | Commit    | What Changed                                                     |
| ------------ | --------- | ---------------------------------------------------------------- |
| Dec 13, 2025 | `31b5830` | X11: Implement audio latency measurement and debug functionality |
| Dec 17, 2025 | `73e224c` | Raylib: Add audio debugging and frame timing measurements        |

---

#### 🎯 Core Concepts

| Concept                   | Implementation                                         |
| ------------------------- | ------------------------------------------------------ |
| **Latency Measurement**   | `snd_pcm_delay()` queries frames queued in ALSA buffer |
| **Target Latency**        | Maintain stable ~66.7ms (3200 frames @ 48kHz)          |
| **Latency-Aware Filling** | Write exactly: `target - current` frames per update    |
| **Graceful Degradation**  | Fallback to Day 9 if `snd_pcm_delay` unavailable       |
| **Performance Timing**    | `clock_gettime()` measures frame duration              |
| **CPU Cycle Counting**    | `__rdtsc()` counts processor cycles per frame          |
| **Debug Overlay**         | F1 key displays audio stats in ASCII box               |

---

#### 📊 Audio Latency Control Flow

```
┌─────────────────────────────────────────────────────────────────┐
│              DAY 10: LATENCY-AWARE AUDIO                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Step 1: Query Current Latency                    │           │
│  │                                                   │           │
│  │  snd_pcm_delay(handle, &delay_frames)            │           │
│  │  Result: delay_frames = 3098                     │           │
│  │          (64.5ms @ 48kHz)                        │           │
│  └────────────────┬─────────────────────────────────┘           │
│                   ↓                                             │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Step 2: Calculate Frames Needed                  │           │
│  │                                                   │           │
│  │  target = 3200 frames (66.7ms)                   │           │
│  │  current = 3098 frames (from query)              │           │
│  │  needed = target - current                       │           │
│  │         = 3200 - 3098 = 102 frames               │           │
│  └────────────────┬─────────────────────────────────┘           │
│                   ↓                                             │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Step 3: Generate & Write Samples                 │           │
│  │                                                   │           │
│  │  for (i = 0; i < 102; i++) {                     │           │
│  │      sample = sin(t_sine) * volume;              │           │
│  │      buffer[i] = apply_pan(sample);              │           │
│  │  }                                                │           │
│  │  snd_pcm_writei(handle, buffer, 102);            │           │
│  └──────────────────────────────────────────────────┘           │
│                                                                 │
│  Result: Latency maintained at stable 66.7ms ✅                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 🎭 Day 9 vs Day 10: Comparison

```
┌─────────────────────────────────────────────────────────────────┐
│         DAY 9 (Availability-Based)                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  frames_available = snd_pcm_avail(handle);                      │
│  // "How much CAN I write?"                                     │
│                                                                 │
│  generate_samples(frames_available);                            │
│  snd_pcm_writei(handle, buffer, frames_available);              │
│                                                                 │
│  Problem: Latency fluctuates! 📊                                │
│    - Sometimes 50ms                                             │
│    - Sometimes 120ms                                            │
│    - No control over consistency                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│         DAY 10 (Latency-Aware)                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  delay_frames = snd_pcm_delay(handle);                          │
│  // "How much IS queued?"                                       │
│                                                                 │
│  frames_needed = target_latency - delay_frames;                 │
│  // "How much do I NEED to maintain target?"                    │
│                                                                 │
│  generate_samples(frames_needed);                               │
│  snd_pcm_writei(handle, buffer, frames_needed);                 │
│                                                                 │
│  Result: Stable 66.7ms latency! ✅                              │
│    - Always within ±5ms of target                               │
│    - Responsive audio feedback                                  │
│    - Professional game audio quality                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 💻 Code Snippets with Explanations

**1. Dynamic Loading of `snd_pcm_delay` (Graceful Degradation Pattern)**

```c
// audio.h - Step 1: Define function signature
#define ALSA_SND_PCM_DELAY(name) \
    int name(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)

typedef ALSA_SND_PCM_DELAY(alsa_snd_pcm_delay);

// Step 2: Declare stub (fallback when ALSA unavailable)
ALSA_SND_PCM_DELAY(AlsaSndPcmDelayStub);

// Step 3: Declare function pointer
extern alsa_snd_pcm_delay *SndPcmDelay_;

// Step 4: Create clean API alias
#define SndPcmDelay SndPcmDelay_

// ───────────────────────────────────────────────────────────────
// audio.c - Implementation
// ───────────────────────────────────────────────────────────────

// Stub implementation (returns error)
ALSA_SND_PCM_DELAY(AlsaSndPcmDelayStub) {
    (void)pcm;     // Suppress unused parameter warning
    (void)delayp;
    return -1;     // Error: function not available
}

// Initialize to stub (safe default)
alsa_snd_pcm_delay *SndPcmDelay_ = AlsaSndPcmDelayStub;

// Try to load real function
void linux_load_alsa(void) {
    // ... open ALSA library with dlopen ...

    // Attempt to load snd_pcm_delay
    LOAD_ALSA_FN(SndPcmDelay_, "snd_pcm_delay", alsa_snd_pcm_delay);

    // Check result
    if (SndPcmDelay_ == AlsaSndPcmDelayStub) {
        printf("⚠️  ALSA: snd_pcm_delay not available\n");
        printf("    Day 10 latency measurement disabled\n");
        printf("    Falling back to Day 9 behavior\n");
    } else {
        printf("✓ ALSA: Day 10 latency measurement available\n");
    }
}

// Helper to check availability
inline bool linux_audio_has_latency_measurement(void) {
    return SndPcmDelay_ != AlsaSndPcmDelayStub;
}
```

**Why this pattern?**

- **Portability:** Code compiles even if ALSA doesn't have `snd_pcm_delay`
- **Runtime flexibility:** Detects availability at runtime, not compile-time
- **Graceful degradation:** Falls back to Day 9 mode automatically
- **No crashes:** Stub prevents segfaults if function missing

---

**2. Latency-Aware Buffer Filling (The Core Algorithm)**

```c
void linux_fill_sound_buffer(void) {
    // Step 1: Query available space (both modes need this)
    long frames_available = SndPcmAvail(g_sound_output.handle);

    if (frames_available < 0) {
        // Handle underrun (buffer ran dry)
        SndPcmRecover(g_sound_output.handle, frames_available, 1);
        return;
    }

    // Step 2: Calculate frames to write (MODE-DEPENDENT)
    long frames_to_write = 0;

    if (linux_audio_has_latency_measurement()) {
        // ═══════════════════════════════════════════════════════
        // MODE 1: DAY 10 - LATENCY-AWARE
        // ═══════════════════════════════════════════════════════

        // Query current latency
        snd_pcm_sframes_t delay_frames = 0;
        int err = SndPcmDelay(g_sound_output.handle, &delay_frames);

        if (err < 0) {
            if (err == -EPIPE) {
                // Underrun - assume buffer empty
                SndPcmRecover(g_sound_output.handle, err, 1);
                delay_frames = 0;
            } else {
                return;  // Other error - skip frame
            }
        }

        // Calculate: how much to reach target?
        long target_queued = g_sound_output.latency_sample_count;  // 3200
        long current_queued = delay_frames;
        long frames_needed = target_queued - current_queued;

        // Clamp to valid range
        if (frames_needed < 0) {
            frames_needed = 0;  // Already at/above target
        }
        if (frames_needed > frames_available) {
            frames_needed = frames_available;  // Can't write more than available
        }
        if (frames_needed > g_sound_output.sample_buffer_size) {
            frames_needed = g_sound_output.sample_buffer_size;  // Buffer limit
        }

        frames_to_write = frames_needed;

    } else {
        // ═══════════════════════════════════════════════════════
        // MODE 2: DAY 9 - AVAILABILITY-BASED (FALLBACK)
        // ═══════════════════════════════════════════════════════

        // Just fill as much as available
        frames_to_write = frames_available;

        if (frames_to_write > g_sound_output.sample_buffer_size) {
            frames_to_write = g_sound_output.sample_buffer_size;
        }
    }

    // Step 3: Early exit if nothing to write
    if (frames_to_write <= 0) {
        return;
    }

    // Step 4: Generate samples (SAME for both modes)
    int16_t *sample_out = g_sound_output.sample_buffer;

    for (long i = 0; i < frames_to_write; ++i) {
        // Sine wave generation
        float sine_value = sinf(g_sound_output.t_sine);
        int16_t sample_value = (int16_t)(sine_value * g_sound_output.tone_volume);

        // Apply stereo panning
        int left_gain = (100 - g_sound_output.pan_position);
        int right_gain = (100 + g_sound_output.pan_position);

        *sample_out++ = (sample_value * left_gain) / 200;   // Left channel
        *sample_out++ = (sample_value * right_gain) / 200;  // Right channel

        // Increment phase
        g_sound_output.t_sine += M_double_PI / g_sound_output.wave_period;

        if (g_sound_output.t_sine >= M_double_PI) {
            g_sound_output.t_sine -= M_double_PI;
        }

        g_sound_output.running_sample_index++;
    }

    // Step 5: Write to ALSA
    long frames_written = SndPcmWritei(
        g_sound_output.handle,
        g_sound_output.sample_buffer,
        frames_to_write
    );

    if (frames_written < 0) {
        SndPcmRecover(g_sound_output.handle, frames_written, 1);
    }
}
```

**Casey's Philosophy:**

- **Feedback loop:** Measure → Calculate → Adjust → Measure again
- **Precise control:** Write exactly what's needed, not "as much as possible"
- **Stable latency:** Keep audio delay consistent for responsive gameplay
- **Graceful degradation:** Works even if measurement unavailable

---

**3. Performance Timing Measurements**

```c
// backend.c - Frame timing setup

// High-precision timers
struct timespec start, end;
uint64_t start_cycles, end_cycles;

// Before main loop
clock_gettime(CLOCK_MONOTONIC, &start);
start_cycles = __rdtsc();

while (game_running) {
    // ... handle events ...
    // ... update game ...
    // ... render frame ...

    // Measure frame time
    clock_gettime(CLOCK_MONOTONIC, &end);
    end_cycles = __rdtsc();

    // Calculate metrics
    double ms_per_frame =
        (end.tv_sec - start.tv_sec) * 1000.0 +
        (end.tv_nsec - start.tv_nsec) / 1000000.0;

    double fps = 1000.0 / ms_per_frame;

    double mcpf = (end_cycles - start_cycles) / 1000000.0;

    printf("%.2fms/f, %.2ff/s, %.2fmc/f\n", ms_per_frame, fps, mcpf);

    // Prepare for next frame
    start = end;
    start_cycles = end_cycles;
}
```

**Why three measurements?**

| Metric   | What It Measures       | Why It Matters                                 |
| -------- | ---------------------- | ---------------------------------------------- |
| **ms/f** | Milliseconds per frame | Direct frame time (target: 16.67ms for 60 FPS) |
| **f/s**  | Frames per second      | User-friendly metric (target: 60+ FPS)         |
| **mc/f** | Megacycles per frame   | CPU usage independent of clock speed           |

**Linux vs Windows:**

- **Linux:** `clock_gettime(CLOCK_MONOTONIC, ...)` - POSIX standard
- **Windows:** `QueryPerformanceCounter()` - Win32 API
- **Both:** High-precision, sub-microsecond accuracy

---

**4. Audio Debug Overlay (F1 Key)**

```c
void linux_debug_audio_latency(void) {
    if (!g_sound_output.is_valid) {
        printf("❌ Audio: Not initialized\n");
        return;
    }

    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│ 🔊 Audio Debug Info                                     │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");

    if (!linux_audio_has_latency_measurement()) {
        // Day 9 mode
        printf("│ ⚠️  Mode: Day 9 (Availability-Based)                    │\n");
        printf("│ snd_pcm_delay not available                             │\n");
        // ... show basic stats ...
        return;
    }

    // Day 10 mode - full stats
    printf("│ ✅ Mode: Day 10 (Latency-Aware)                          │\n");

    // Query current latency
    snd_pcm_sframes_t delay_frames = 0;
    int err = SndPcmDelay(g_sound_output.handle, &delay_frames);

    if (err < 0) {
        printf("│ ❌ Can't measure delay: %s                              │\n",
               SndStrerror(err));
        return;
    }

    // Calculate milliseconds
    float actual_latency_ms =
        (float)delay_frames / g_sound_output.samples_per_second * 1000.0f;
    float target_latency_ms =
        (float)g_sound_output.latency_sample_count /
        g_sound_output.samples_per_second * 1000.0f;

    // Display latency comparison
    printf("│ Target latency:  %.1f ms (%d frames)                 │\n",
           target_latency_ms, g_sound_output.latency_sample_count);
    printf("│ Actual latency:  %.1f ms (%ld frames)                │\n",
           actual_latency_ms, (long)delay_frames);

    // Color-coded status
    float diff = actual_latency_ms - target_latency_ms;
    if (fabs(diff) < 5.0f) {
        printf("│ Status:          ✅ GOOD (±%.1fms)                       │\n", diff);
    } else if (fabs(diff) < 10.0f) {
        printf("│ Status:          ⚠️  OK (±%.1fms)                         │\n", diff);
    } else {
        printf("│ Status:          ❌ BAD (±%.1fms)                         │\n", diff);
    }

    // Additional stats
    printf("│ Sample rate:     %d Hz                                 │\n",
           g_sound_output.samples_per_second);
    printf("│ Frequency:       %d Hz                                 │\n",
           g_sound_output.tone_hz);
    printf("│ Volume:          %d / 15000                            │\n",
           g_sound_output.tone_volume);
    printf("└─────────────────────────────────────────────────────────┘\n");
}
```

**F1 Key Handler:**

```c
// backend.c - Keyboard handling
case XK_F1: {
    printf("F1 pressed - showing audio debug\n");
    linux_debug_audio_latency();
    break;
}
```

---

**5. Raylib Implementation (Simplified)**

```c
// Raylib backend doesn't need Day 10 latency control
// (callback-based system handles it automatically)
// But we still add timing and debug features!

// backend.c (raylib)
struct timespec g_frame_start, g_frame_end;

// Main loop
clock_gettime(CLOCK_MONOTONIC, &g_frame_start);

while (!WindowShouldClose()) {
    // ... game logic ...

    // Measure frame time
    clock_gettime(CLOCK_MONOTONIC, &g_frame_end);

    double ms_per_frame =
        (g_frame_end.tv_sec - g_frame_start.tv_sec) * 1000.0 +
        (g_frame_end.tv_nsec - g_frame_start.tv_nsec) / 1000000.0;

    double fps = 1000.0 / ms_per_frame;

    printf("%.2fms/f, %.2ff/s\n", ms_per_frame, fps);

    g_frame_start = g_frame_end;
}

// F1 handler
if (IsKeyPressed(KEY_F1)) {
    raylib_debug_audio();
}
```

**Why Raylib is simpler:**

- No manual latency control needed (callback handles it)
- No `snd_pcm_delay` equivalent (miniaudio abstracts it)
- Still benefits from timing and debug features

---

#### 🐛 Common Pitfalls

| Issue                                                   | Cause                                           | Fix                                                           |
| ------------------------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------- |
| **Compile error: `undefined reference to SndPcmDelay`** | Forgot to initialize function pointer           | Add `SndPcmDelay_ = AlsaSndPcmDelayStub;` in audio.c          |
| **Segfault when calling `SndPcmDelay()`**               | Function pointer is NULL                        | Check if `LOAD_ALSA_FN()` succeeded before using              |
| **Latency measurement returns -EPIPE**                  | Audio underrun occurred                         | Call `SndPcmRecover()` and retry with `delay_frames = 0`      |
| **Timing shows 0.00ms/f**                               | Division by nanoseconds instead of microseconds | Use `/ 1000000.0` not `/ 1000.0` for ns→ms                    |
| **FPS fluctuates wildly**                               | Measuring wall clock instead of monotonic       | Use `CLOCK_MONOTONIC` not `CLOCK_REALTIME`                    |
| **Macro name collision**                                | `#define SndPcmDelay SndPcmDelay`               | Use underscore: `SndPcmDelay_` for variable, macro maps to it |
| **Day 10 mode never activates**                         | Forgot to load function in `linux_load_alsa()`  | Add `LOAD_ALSA_FN(SndPcmDelay_, ...)`                         |
| **Raylib: timing shows huge numbers**                   | Wrong conversion factor                         | Use `* 1000.0` for sec→ms, `/ 1000000.0` for ns→ms            |

---

#### ⚙️ Performance Analysis

```
┌─────────────────────────────────────────────────────────────────┐
│         Understanding Frame Timing Output                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Example output: "2.47ms/f, 404.43f/s, 5.22mc/f"                │
│                                                                 │
│  2.47ms/f:                                                      │
│    - This frame took 2.47 milliseconds                          │
│    - Target: 16.67ms (for 60 FPS)                               │
│    - Status: ✅ Excellent! Running at 400+ FPS                  │
│                                                                 │
│  404.43f/s:                                                     │
│    - Running at 404 frames per second                           │
│    - Formula: 1000ms / 2.47ms = 404.43                          │
│    - Way above 60 FPS target (game is simple right now)         │
│                                                                 │
│  5.22mc/f:                                                      │
│    - Used 5.22 million CPU cycles                               │
│    - On 3GHz CPU: 5.22M / 3000M = 0.17% CPU usage               │
│    - Very efficient! (Most time is sleeping/waiting)            │
│                                                                 │
│  Why does FPS vary?                                             │
│    - X11 event processing (0-10 events)                         │
│    - Audio buffer filling (0-1024 frames)                       │
│    - OS scheduling (context switches)                           │
│    - Cache misses                                               │
│    - This is NORMAL! Don't worry yet.                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 📊 X11 vs Raylib: Day 10 Comparison

| Feature                    | X11 Backend                        | Raylib Backend             |
| -------------------------- | ---------------------------------- | -------------------------- |
| **Latency Measurement**    | ✅ `snd_pcm_delay()`               | ❌ Not exposed (automatic) |
| **Manual Latency Control** | ✅ Calculate `target - current`    | ❌ Callback-based          |
| **Day 10 Mode Available**  | ✅ Yes (if ALSA supports it)       | ⚠️ Estimated only          |
| **Frame Timing**           | ✅ `clock_gettime()` + `__rdtsc()` | ✅ `clock_gettime()`       |
| **CPU Cycle Counting**     | ✅ `__rdtsc()`                     | ✅ Same                    |
| **F1 Debug Overlay**       | ✅ Full stats with latency         | ✅ Simplified stats        |
| **Graceful Degradation**   | ✅ Falls back to Day 9             | N/A (always "Day 10-like") |
| **Buffer Filling Logic**   | ✅ Two modes (Day 9/Day 10)        | ✅ Automatic (one mode)    |

---

#### 🎓 Skills Acquired

- ✅ **Audio Latency Control**

  - Query queued frames with `snd_pcm_delay()`
  - Calculate precise write amounts to maintain target
  - Implement feedback loop for stable latency

- ✅ **Graceful Degradation Pattern**

  - Dynamic loading with function pointers
  - Stub implementations for missing functions
  - Runtime detection of capabilities
  - Automatic fallback to simpler mode

- ✅ **Performance Measurement**

  - High-precision timing with `clock_gettime()`
  - CPU cycle counting with `__rdtsc()`
  - Calculate ms/frame, FPS, megacycles/frame
  - Understand frame time variance

- ✅ **Debug Tooling**

  - ASCII art debug overlays
  - Keyboard shortcuts (F1 for audio stats)
  - Color-coded status indicators
  - Real-time metrics display

- ✅ **Cross-Platform Abstraction**

  - Understand Raylib's callback model
  - Compare manual vs automatic latency control
  - Adapt concepts across backends

- ✅ **C Programming Patterns**
  - Function pointer pattern for dynamic loading
  - Macro hygiene (underscore suffix)
  - Inline helper functions
  - Static analysis warning suppression

---

#### 📚 Additional Resources

**ALSA Documentation:**

- `snd_pcm_delay()`: https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html#ga
- Latency tuning guide: https://alsa.opensrc.org/Latency

**Linux Timing:**

- `clock_gettime()` man page: `man 2 clock_gettime`
- POSIX timers: https://linux.die.net/man/2/clock_gettime

**CPU Cycle Counting:**

- `__rdtsc()` intrinsic: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=rdtsc

**Casey's Day 10 Stream:**

- Handmade Hero Day 10: https://hero.handmade.network/episode/code/day010/

---

#### 💡 Key Takeaways

1. **Latency control is a feedback loop:** Measure → Calculate → Write → Measure again

2. **Graceful degradation is professional:** Code should work even when ideal conditions aren't met

3. **Performance measurement is essential:** You can't optimize what you don't measure

4. **Platform differences matter:** X11 gives manual control, Raylib abstracts it away - both valid approaches

5. **Debug tools save time:** F1 overlay is faster than printf debugging

---

### 🔊 Audio Fundamentals: Understanding Sound in Computers

> **Before diving into Day 10's audio latency control, let's understand what audio actually IS and how operating systems handle it.**

---

#### What IS Audio? (For Visual Thinkers)

Sound is **vibrating air**. When you speak, your vocal cords vibrate, pushing air molecules:

```
┌─────────────────────────────────────────────────────────────────┐
│                    SOUND AS PHYSICAL WAVES                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Real World (Air Pressure Over Time):                           │
│                                                                 │
│      High    ╱╲      ╱╲      ╱╲      ╱╲                         │
│   Pressure  ╱  ╲    ╱  ╲    ╱  ╲    ╱  ╲                        │
│           ─╯────╲──╯────╲──╯────╲──╯────╲─── Time →            │
│                  ╲╱      ╲╱      ╲╱      ╲╱                      │
│      Low                                                        │
│   Pressure                                                      │
│                                                                 │
│  Properties:                                                    │
│  - Frequency (Hz): How fast it oscillates (pitch)               │
│  - Amplitude: How tall the wave is (volume)                     │
│  - Phase: Where in the cycle we are                             │
│                                                                 │
│  Example:                                                       │
│  - 256 Hz tone = 256 complete waves per second                  │
│  - Middle C = 261.63 Hz                                         │
│  - Human hearing: ~20 Hz to 20,000 Hz                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### How Computers Represent Sound

Computers can't store waves—they store **numbers**. We **sample** the wave:

```
┌─────────────────────────────────────────────────────────────────┐
│                    ANALOG → DIGITAL CONVERSION                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Step 1: Sample the Wave (Take measurements)                    │
│                                                                 │
│      ╱╲         Sampling Points (48,000 times per second)       │
│     ╱  ╲        ↓    ↓    ↓    ↓    ↓    ↓    ↓                │
│   ─╯────╲───────●────●────●────●────●────●────●─── Time →      │
│          ╲╱                                                     │
│                                                                 │
│  Step 2: Quantize (Convert to integers)                         │
│                                                                 │
│  Sample 1: 0      (silence)                                     │
│  Sample 2: 3000   (low volume)                                  │
│  Sample 3: 6000   (medium volume)                               │
│  Sample 4: 8000   (higher volume)                               │
│  Sample 5: 6000   (back down)                                   │
│  Sample 6: 3000   (lower)                                       │
│  Sample 7: 0      (silence again)                               │
│                                                                 │
│  These numbers are stored in memory!                            │
│                                                                 │
│  ┌──────────────────────────────────────────┐                  │
│  │ Memory (array of samples):               │                  │
│  │ [0, 3000, 6000, 8000, 6000, 3000, 0, ...] │                  │
│  └──────────────────────────────────────────┘                  │
│                                                                 │
│  Sample Rate: How many samples per second                       │
│  - CD Quality: 44,100 Hz (44,100 samples/sec)                   │
│  - Game Audio: 48,000 Hz (48,000 samples/sec)                   │
│  - Phone Calls: 8,000 Hz (lower quality, smaller size)          │
│                                                                 │
│  Bit Depth: Range of each sample                                │
│  - 8-bit: -128 to +127 (old games, lo-fi)                       │
│  - 16-bit: -32,768 to +32,767 (CD quality) ← WE USE THIS        │
│  - 24-bit: Professional audio                                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Key Insight:**
Sound in a computer is just an **array of numbers**. To play sound, we feed these numbers to the speakers at a specific rate (sample rate).

---

#### How the OS Plays Sound: The Audio Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│              OS AUDIO PIPELINE (Linux/ALSA)                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. YOUR GAME                                                   │
│  ┌────────────────────────────────────┐                         │
│  │ Generate samples:                  │                         │
│  │ sample[0] = 3000;                  │                         │
│  │ sample[1] = 6000;                  │                         │
│  │ sample[2] = 8000;                  │                         │
│  │ ...                                │                         │
│  └──────────────┬─────────────────────┘                         │
│                 │ Write samples                                 │
│                 ↓                                               │
│  2. ALSA (Audio Layer)                                          │
│  ┌────────────────────────────────────┐                         │
│  │ Ring Buffer (in kernel memory):    │                         │
│  │                                    │                         │
│  │  ┌──────────────────────────────┐  │                         │
│  │  │[▓▓▓▓▓░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓]│  │                         │
│  │  └──────────────────────────────┘  │                         │
│  │   ↑ Play    ↑ Empty  ↑ Queued      │                         │
│  │   Cursor    Space    Samples       │                         │
│  │                                    │                         │
│  │  Automatically feeds to hardware   │                         │
│  └──────────────┬─────────────────────┘                         │
│                 │ DMA (Direct Memory Access)                    │
│                 ↓                                               │
│  3. SOUND CARD (Hardware)                                       │
│  ┌────────────────────────────────────┐                         │
│  │ DAC (Digital-to-Analog Converter)  │                         │
│  │                                    │                         │
│  │ Numbers → Electrical signals       │                         │
│  │ 3000 → Low voltage                 │                         │
│  │ 6000 → Medium voltage              │                         │
│  │ 8000 → Higher voltage              │                         │
│  └──────────────┬─────────────────────┘                         │
│                 │ Analog signal                                 │
│                 ↓                                               │
│  4. SPEAKERS                                                    │
│  ┌────────────────────────────────────┐                         │
│  │  Voltage → Magnet movement         │                         │
│  │  Magnet → Speaker cone vibrates    │                         │
│  │  Cone → Pushes air                 │                         │
│  │  Air → SOUND! 🔊                    │                         │
│  └────────────────────────────────────┘                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Critical Point:**
The hardware is **constantly** consuming samples from the buffer. If you don't refill it fast enough → **silence/crackling**!

---

#### What IS Audio Latency?

Latency = **delay** between when you trigger a sound and when you hear it.

```
┌─────────────────────────────────────────────────────────────────┐
│                    AUDIO LATENCY EXPLAINED                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Timeline of a Sound Effect:                                    │
│                                                                 │
│  T=0ms: Player presses fire button 🎮                           │
│         ↓                                                       │
│  T=0.1ms: Game generates gunshot samples                        │
│         ↓                                                       │
│  T=0.2ms: Samples written to ALSA ring buffer                   │
│         │                                                       │
│         │  ┌─────────────────────────────────┐                 │
│         │  │ Ring Buffer:                    │                 │
│         │  │ [music...music...GUNSHOT...] │                 │
│         │  │  ↑ Play cursor (slowly moving) │                 │
│         │  └─────────────────────────────────┘                 │
│         │                                                       │
│         │  Hardware is still playing music!                    │
│         │  Gunshot is QUEUED but not playing yet               │
│         │                                                       │
│  T=66.7ms: Hardware cursor reaches gunshot samples              │
│         ↓                                                       │
│  T=66.7ms: BANG! Sound plays through speakers 🔊                │
│                                                                 │
│  ╔═══════════════════════════════════════════════════╗          │
│  ║ LATENCY = 66.7ms (time from button to sound)     ║          │
│  ╚═══════════════════════════════════════════════════╝          │
│                                                                 │
│  Problem: Player feels disconnected!                            │
│  - Button press at T=0                                          │
│  - Sound heard at T=66.7ms                                      │
│  - Feels "laggy" or "mushy"                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Analogy:**
Imagine texting someone who reads texts really slowly:

- You: "Hello!" (T=0)
- _66.7ms of waiting..._
- Them: _Sees "Hello!"_ (T=66.7ms)
- Frustrating delay!

---

#### The Latency Tradeoff

```
┌─────────────────────────────────────────────────────────────────┐
│              LATENCY vs UNDERRUN TRADEOFF                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Scenario A: LOW LATENCY (10ms buffer)                          │
│  ───────────────────────────────────────                        │
│                                                                 │
│  Ring Buffer (480 samples @ 48kHz = 10ms):                      │
│  ┌────────────────────────────────────────┐                     │
│  │[▓▓▓▓▓▓▓▓▓▓░░]                          │                     │
│  └────────────────────────────────────────┘                     │
│   ↑ Play    ↑ Empty                                             │
│                                                                 │
│  ✅ Pro: Very responsive (10ms delay)                           │
│  ❌ Con: If game lags for 11ms → UNDERRUN! → Crackling!        │
│                                                                 │
│  Game frame took 16ms (60 FPS):                                 │
│    0ms ──────────────────→ 16ms                                 │
│    [Game logic + rendering]                                     │
│                    ↑                                            │
│               At 10ms, buffer ran dry!                          │
│               💥 CRACKLE/POP                                    │
│                                                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                 │
│  Scenario B: HIGH LATENCY (200ms buffer)                        │
│  ────────────────────────────────────────                       │
│                                                                 │
│  Ring Buffer (9600 samples @ 48kHz = 200ms):                    │
│  ┌────────────────────────────────────────┐                     │
│  │[▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░]  │                     │
│  └────────────────────────────────────────┘                     │
│   ↑ Play                    ↑ Empty                             │
│                                                                 │
│  ✅ Pro: Very safe (can skip frames without underrun)          │
│  ❌ Con: Unresponsive (200ms delay is TERRIBLE for games!)     │
│                                                                 │
│  Player presses fire:                                           │
│    0ms ──────────────────────────────────────→ 200ms            │
│    [Waiting... waiting... waiting... BANG! 💥]                  │
│                                                                 │
│    Too slow! Feels broken!                                      │
│                                                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                 │
│  Scenario C: OPTIMAL LATENCY (66.7ms buffer) ← Casey's Choice   │
│  ──────────────────────────────────────────────────────────     │
│                                                                 │
│  Ring Buffer (3200 samples @ 48kHz = 66.7ms):                   │
│  ┌────────────────────────────────────────┐                     │
│  │[▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░]          │                     │
│  └────────────────────────────────────────┘                     │
│   ↑ Play          ↑ Empty                                       │
│                                                                 │
│  ✅ Pro: Responsive enough for gameplay (~4 frames @ 60 FPS)    │
│  ✅ Pro: Safe enough to tolerate frame drops                    │
│                                                                 │
│  Goldilocks Zone:                                               │
│  - Not too low (no crackling)                                   │
│  - Not too high (still responsive)                              │
│  - Just right! ✨                                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Key Formula:**

```
Latency (ms) = (Buffer Size in Samples / Sample Rate) × 1000

Example:
3200 samples / 48000 Hz × 1000 = 66.7ms
```

---

#### Why Can't We Just Use 0ms Latency?

```
┌─────────────────────────────────────────────────────────────────┐
│          WHY ZERO LATENCY IS IMPOSSIBLE                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Fundamental Problems:                                          │
│                                                                 │
│  1. Hardware Needs Time                                         │
│     ─────────────────────                                       │
│     DAC (Digital-to-Analog Converter) has physical limits       │
│     - Electrical circuits need time to settle                   │
│     - Typical minimum: ~3-5ms                                   │
│                                                                 │
│  2. CPU Scheduling Isn't Perfect                                │
│     ────────────────────────────────                            │
│     Your game doesn't run alone:                                │
│                                                                 │
│     Timeline (Linux scheduler):                                 │
│     0ms ─────────────────────────────────────→ 20ms             │
│     [Your game][Browser][OS task][Your game]                    │
│      ↑ Paused!           ↑ Resumed                              │
│                                                                 │
│     If buffer is too small:                                     │
│     - Paused for 5ms → Buffer runs dry → Crackle!              │
│                                                                 │
│  3. Frame Rate Varies                                           │
│     ─────────────────                                           │
│     Game frames take different times:                           │
│                                                                 │
│     Frame 1: 8ms  (fast)                                        │
│     Frame 2: 12ms (normal)                                      │
│     Frame 3: 25ms (spike! GC, loading, etc.)                    │
│     Frame 4: 10ms (back to normal)                              │
│                                                                 │
│     Small buffer can't handle frame 3's spike!                  │
│                                                                 │
│  4. USB Audio Adds More Delay                                   │
│     ──────────────────────────────                              │
│     USB protocol has its own buffering:                         │
│     - USB polls every 1ms (USB 2.0)                             │
│     - Adds 1-10ms minimum latency                               │
│                                                                 │
│  ╔═══════════════════════════════════════════════════╗          │
│  ║ Reality: ~20-70ms is the practical range         ║          │
│  ║ Casey picks 66.7ms as safe middle ground         ║          │
│  ╚═══════════════════════════════════════════════════╝          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### How Day 10 Controls Latency: The Feedback Loop

```
┌─────────────────────────────────────────────────────────────────┐
│              DAY 9 vs DAY 10: THE KEY DIFFERENCE                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  DAY 9 APPROACH (Availability-Based):                           │
│  ───────────────────────────────────────                        │
│                                                                 │
│  Every frame:                                                   │
│    1. Ask: "How much CAN I write?"                              │
│       frames_available = snd_pcm_avail()  → 1024 frames         │
│                                                                 │
│    2. Generate that much:                                       │
│       for (i = 0; i < 1024; i++) { generate_sample(); }         │
│                                                                 │
│    3. Write it all:                                             │
│       snd_pcm_writei(handle, buffer, 1024);                     │
│                                                                 │
│  Problem: Latency fluctuates wildly!                            │
│                                                                 │
│  Frame 1: Write 1024 samples → Latency = 70ms                   │
│  Frame 2: Write 512 samples  → Latency = 60ms                   │
│  Frame 3: Write 2048 samples → Latency = 90ms                   │
│  Frame 4: Write 256 samples  → Latency = 55ms                   │
│                                                                 │
│  Latency graph:                                                 │
│   90ms │    ╱╲                                                  │
│   70ms │ ╱╲╱  ╲                                                 │
│   55ms │╱      ╲─╱                                              │
│        └───────────── Time                                      │
│        Wobbly! ⚠️                                                │
│                                                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                 │
│  DAY 10 APPROACH (Latency-Aware):                               │
│  ────────────────────────────────────                           │
│                                                                 │
│  Every frame:                                                   │
│    1. Measure: "How much IS queued?"                            │
│       current_queued = snd_pcm_delay()  → 3098 frames (64.5ms) │
│                                                                 │
│    2. Calculate: "How much do I NEED?"                          │
│       target = 3200 frames (66.7ms)                             │
│       needed = target - current_queued                          │
│              = 3200 - 3098 = 102 frames                         │
│                                                                 │
│    3. Generate exactly that:                                    │
│       for (i = 0; i < 102; i++) { generate_sample(); }          │
│                                                                 │
│    4. Write it:                                                 │
│       snd_pcm_writei(handle, buffer, 102);                      │
│                                                                 │
│  Result: Stable latency!                                        │
│                                                                 │
│  Frame 1: Need 102 → Latency = 66.7ms                           │
│  Frame 2: Need 95  → Latency = 66.8ms                           │
│  Frame 3: Need 108 → Latency = 66.6ms                           │
│  Frame 4: Need 101 → Latency = 66.7ms                           │
│                                                                 │
│  Latency graph:                                                 │
│   70ms │                                                        │
│   66.7ms│─────────────────                                     │
│   60ms │                                                        │
│        └───────────────── Time                                  │
│        Stable! ✅                                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**This is a FEEDBACK LOOP** (like a thermostat):

```
┌─────────────────────────────────────────────────────────────────┐
│                    AUDIO FEEDBACK LOOP                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Thermostat Analogy:                                            │
│  ──────────────────                                             │
│                                                                 │
│  Target: 70°F                                                   │
│  Current: 68°F                                                  │
│  → Turn heater ON for 2 minutes                                 │
│                                                                 │
│  Current: 70°F                                                  │
│  → Turn heater OFF                                              │
│                                                                 │
│  Current: 71°F                                                  │
│  → Turn AC ON for 1 minute                                      │
│                                                                 │
│  Audio Equivalent:                                              │
│  ─────────────────                                              │
│                                                                 │
│  Target: 66.7ms latency                                         │
│  Current: 64.5ms                                                │
│  → Write 102 samples (add 2.1ms)                                │
│                                                                 │
│  Current: 66.7ms                                                │
│  → Write 0 samples (perfect!)                                   │
│                                                                 │
│  Current: 68.0ms                                                │
│  → Write 0 samples (let it drain)                               │
│                                                                 │
│  This keeps latency STABLE! ✨                                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### Why This Matters for Games

```
┌─────────────────────────────────────────────────────────────────┐
│            AUDIO LATENCY IN GAME SCENARIOS                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Scenario 1: Fighting Game 🥊                                   │
│  ───────────────────────────                                    │
│                                                                 │
│  Player presses punch button:                                   │
│                                                                 │
│  With 200ms latency:                                            │
│    T=0ms:   Button pressed 🎮                                   │
│    T=16ms:  Animation starts (visual feedback)                  │
│    T=200ms: *WHACK!* sound plays 🔊                             │
│             ↑ Feels WRONG! Sound too late!                      │
│                                                                 │
│  With 66.7ms latency (Day 10):                                  │
│    T=0ms:   Button pressed 🎮                                   │
│    T=16ms:  Animation starts (visual feedback)                  │
│    T=66.7ms: *WHACK!* sound plays 🔊                            │
│             ↑ Feels RIGHT! Barely noticeable delay              │
│                                                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                 │
│  Scenario 2: Music/Rhythm Game 🎵                               │
│  ────────────────────────────────                               │
│                                                                 │
│  Visual cue appears on screen:                                  │
│  [  ↓  ]  ← Player must press button when arrow reaches line    │
│  [     ]                                                        │
│  [─────]  ← Target line                                         │
│                                                                 │
│  T=0ms:   Arrow reaches line (visual)                           │
│  T=0ms:   Player presses button 🎮                              │
│  T=66.7ms: *DING!* sound plays 🔊                               │
│                                                                 │
│  Problem: Sound is 66.7ms behind visuals!                       │
│  Solution: Game compensates by playing sound 66.7ms EARLY       │
│           (queues sound before arrow reaches line)              │
│                                                                 │
│  This is why rhythm games need STABLE latency!                  │
│  Fluctuating latency = impossible to compensate!                │
│                                                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│                                                                 │
│  Scenario 3: FPS Game 🔫                                        │
│  ─────────────────────                                          │
│                                                                 │
│  Player fires gun:                                              │
│                                                                 │
│  T=0ms:    Click! 🖱️                                            │
│  T=16ms:   Muzzle flash appears 💥 (visual)                     │
│  T=66.7ms: *BANG!* 🔊 (audio)                                   │
│                                                                 │
│  66.7ms = ~4 frames @ 60 FPS                                    │
│  Acceptable! Brain doesn't notice < 80ms                        │
│                                                                 │
│  But if latency varies (50ms, 100ms, 70ms, 120ms):             │
│  → Feels "mushy" or "inconsistent"                              │
│  → Players subconsciously notice!                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Human Perception:**

- < 20ms: Imperceptible (feels instant)
- 20-50ms: Noticeable if you're looking for it
- 50-80ms: Acceptable for games (Day 10's target)
- 80-150ms: Noticeable lag
- \> 150ms: Feels broken

---

#### Summary: Audio in 5 Levels

```
┌─────────────────────────────────────────────────────────────────┐
│              AUDIO UNDERSTANDING LADDER                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Level 1 (5 years old): 🎵                                      │
│  ─────────────────────                                          │
│  "Sound is vibrating air. Computers turn numbers into sound."   │
│                                                                 │
│  Level 2 (Beginner programmer): 💻                              │
│  ──────────────────────────────                                 │
│  "Sound is an array of samples. We write samples to a buffer,   │
│   and hardware plays them at a fixed rate (sample rate)."       │
│                                                                 │
│  Level 3 (Game developer): 🎮                                   │
│  ──────────────────────────                                     │
│  "The OS maintains a ring buffer. We write samples to it        │
│   faster than hardware consumes them. The gap between write     │
│   and play is latency. Too small = crackling. Too big = lag."  │
│                                                                 │
│  Level 4 (Handmade Hero Day 10): 🎯                             │
│  ──────────────────────────────────                             │
│  "We measure current latency, compare to target, and write      │
│   exactly the right amount to maintain stable latency. This     │
│   is a feedback loop. Day 9 blindly fills, Day 10 measures."   │
│                                                                 │
│  Level 5 (Audio engineer): 🔬                                   │
│  ─────────────────────────                                      │
│  "We consider DMA timing, interrupt coalescing, ALSA period     │
│   sizes, resampling artifacts, and jitter correction to         │
│   minimize latency while maximizing reliability."               │
│                                                                 │
│  You're at Level 4 now! 🎉                                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---
