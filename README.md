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

#ifndef M_PI_DOUBLED
#define M_PI_DOUBLED (2.0f * M_PI)
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
        g_sound_output.t_sine += M_PI_DOUBLED / (float)g_sound_output.wave_period;

        // Optional: Wrap to [0, 2π) range to prevent overflow
        if (g_sound_output.t_sine >= M_PI_DOUBLED) {
            g_sound_output.t_sine -= M_PI_DOUBLED;
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
        g_sound_output.t_sine += M_PI_DOUBLED / (real32)g_sound_output.wave_period;

        // Wrap to prevent overflow
        if (g_sound_output.t_sine >= M_PI_DOUBLED) {
            g_sound_output.t_sine -= M_PI_DOUBLED;
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
        g_sound_output.t_sine += M_PI_DOUBLED / g_sound_output.wave_period;

        if (g_sound_output.t_sine >= M_PI_DOUBLED) {
            g_sound_output.t_sine -= M_PI_DOUBLED;
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

### 📆 Day 11: Platform/Game Layer Separation

**Focus:** Separating platform-specific code (X11/Raylib) from game logic into distinct compilation units, creating a clean API boundary that enables future hot-reloading and multi-platform support.

---

#### 🗓️ Commits

| Date         | Commit    | What Changed                                            |
| ------------ | --------- | ------------------------------------------------------- |
| Dec 28, 2025 | `ff28c88` | **Day 11 Complete** - Platform/game separation refactor |
|              |           | - Moved game state from platform to `game.c`            |
|              |           | - Created `pixel_composer_fn` abstraction               |
|              |           | - Unified controls handling across backends             |
|              |           | - Added `game_update_and_render()` entry point          |
|              |           | - Removed `PlatformPixelFormatShift` leak               |

---

#### 📊 Architecture: The Great Separation

```
BEFORE DAY 11 (Monolithic):
┌─────────────────────────────────────────────────────────┐
│ Platform Layer (platform/x11/backend.c)                 │
│                                                          │
│  • GameState contains:                                  │
│    - gradient (offset_x, offset_y)  ❌ Game logic!      │
│    - pixel (offset_x, offset_y)      ❌ Game logic!      │
│    - speed                           ❌ Game logic!      │
│                                                          │
│  • Multiple render calls:                               │
│    render_weird_gradient(&buffer, &state, &shift); ❌   │
│    testPixelAnimation(&buffer, &pixel, color);     ❌   │
│                                                          │
│  • Game receives platform pixel format:                 │
│    PlatformPixelFormatShift { ALPHA_SHIFT, ... }  ❌    │
│                                                          │
└─────────────────────────────────────────────────────────┘

AFTER DAY 11 (Separated):
┌─────────────────────────────────────────────────────────┐
│ Platform Layer (platform/x11/backend.c)                 │
│                                                          │
│  • GameState contains ONLY:                             │
│    - controls (input state)         ✅ Platform domain  │
│    - gamepad_id                     ✅ Platform config  │
│    - is_running                     ✅ Platform state   │
│                                                          │
│  • Single entry point:                                  │
│    game_update_and_render(0xFF0000FF);  ✅              │
│                                                          │
│  • Platform provides pixel composer:                    │
│    g_backbuffer.compose_pixel = compose_pixel_xrgb; ✅  │
│                                                          │
└──────────────────┬──────────────────────────────────────┘
                   │
                   │ API Boundary (game.h)
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ void game_update_and_render(int pixel_color);           │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│ Game Layer (game.c)                                     │
│                                                          │
│  • Game state (hidden via local_persist_var):           │
│    static int gradient_offset_x = 0;  ✅ Game owns this │
│    static int gradient_offset_y = 0;                    │
│    static int pixel_offset_x = 0;                       │
│    static int pixel_offset_y = 0;                       │
│                                                          │
│  • render_weird_gradient()           ✅ Game logic      │
│    Uses buffer->compose_pixel()      ✅ No platform!    │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

---

#### 🎯 Core Concepts

| Concept                  | Casey's Day 11                                                    | Your Implementation                    | Status                 |
| ------------------------ | ----------------------------------------------------------------- | -------------------------------------- | ---------------------- |
| **Game Entry Point**     | `GameUpdateAndRender(Buffer, BlueOffset, GreenOffset)`            | `game_update_and_render(pixel_color)`  | ✅ Match               |
| **Game State Ownership** | Platform owns `XOffset, YOffset` (static in `win32_handmade.cpp`) | Platform owns `g_game_state`           | ✅ Match               |
| **State Passing**        | Parameters to `GameUpdateAndRender()`                             | Global `g_game_state` struct           | ⚠️ Different but valid |
| **Buffer Abstraction**   | `game_offscreen_buffer`                                           | `OffscreenBuffer` with `compose_pixel` | ✅ Better than Casey!  |
| **Pixel Format**         | Hardcoded `0x00RRGGBB`                                            | Platform-provided composer function    | ✅ Better than Casey!  |
| **File Separation**      | `handmade.h/.cpp` + `win32_handmade.cpp`                          | `game.h/.c` + `platform/x11/backend.c` | ✅ Match               |

---

#### 🔧 Key Innovation: Platform-Agnostic Pixel Composer

##### **The Problem Casey Had**

```c
// Casey's handmade.cpp (Day 11) - Hardcoded format
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    for(int Y = 0; Y < Buffer->Height; ++Y) {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0; X < Buffer->Width; ++X) {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);  // ❌ Hardcoded 0x00GGBB00
        }
    }
}
```

**Problem:** Works only for Windows' BGR format. Adding Linux/Mac/WASM requires changing game code!

---

##### **Your Solution: Function Pointer Abstraction**

**Step 1: Define Composer Type** (`base.h`)

```c
// Platform-agnostic pixel composer function
// Platform sets this once, game just calls it
typedef uint32_t (*pixel_composer_fn)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
```

**Step 2: Add to Buffer Struct** (`game.h`)

```c
typedef struct {
  void *memory;
  int width, height, pitch, bytes_per_pixel;
  pixel_composer_fn compose_pixel;  // ✅ Platform-provided composer
} OffscreenBuffer;
```

**Step 3: Platform Implements Composers**

```c
// X11 Backend (0xAARRGGBB format)
file_scoped_fn uint32_t compose_pixel_xrgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return ((a << 24) | (r << 16) | (g << 8) | b);
}

// Raylib Backend (R8G8B8A8 in memory)
file_scoped_fn uint32_t compose_pixel_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return ((a << 24) | (b << 16) | (g << 8) | r);
}

// In resize_back_buffer():
g_backbuffer.compose_pixel = compose_pixel_xrgb;  // X11
// OR
g_backbuffer.compose_pixel = compose_pixel_rgba;  // Raylib
```

**Step 4: Game Uses Composer** (`game.c`)

```c
void render_weird_gradient() {
  // ✅ Game is 100% platform-agnostic!
  for (int y = 0; y < g_backbuffer.height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < g_backbuffer.width; ++x) {
      uint8_t red = 0;
      uint8_t green = (y + g_gradient_state.offset_y);
      uint8_t blue = (x + g_gradient_state.offset_x);
      uint8_t alpha = 255;

      *pixels++ = g_backbuffer.compose_pixel(red, green, blue, alpha);
      //          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
      //          Platform-provided function - game doesn't care about format!
    }
    row += g_backbuffer.pitch;
  }
}
```

---

#### 💻 Code Snippets with Explanations

##### **1. Game Initialization** (`game.c`)

```c
// ═══════════════════════════════════════════════════════════════
// Game state is HIDDEN from platform (static variables)
// ═══════════════════════════════════════════════════════════════
// Casey's pattern: Use static to enforce encapsulation
// Platform can't see or modify these - only through game_update_and_render()

local_persist_var GradientState g_gradient_state = {0};
local_persist_var PixelState g_pixel_state = {0};

INIT_BACKBUFFER_STATUS init_backbuffer(int width, int height,
                                       int bytes_per_pixel,
                                       pixel_composer_fn composer) {
  g_backbuffer.memory = NULL;
  g_backbuffer.width = width;
  g_backbuffer.height = height;
  g_backbuffer.bytes_per_pixel = bytes_per_pixel;
  g_backbuffer.pitch = g_backbuffer.width * g_backbuffer.bytes_per_pixel;

  int buffer_size = g_backbuffer.pitch * g_backbuffer.height;
  g_backbuffer.memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (g_backbuffer.memory == MAP_FAILED) {
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return INIT_BACKBUFFER_STATUS_MMAP_FAILED;
  }

  // ✅ Store platform-provided composer
  g_backbuffer.compose_pixel = composer;

  return INIT_BACKBUFFER_STATUS_SUCCESS;
}
```

**Why this works:**

- `local_persist_var` = `static` (Casey's style)
- Game state lives in `game.c`, not platform layer
- Platform only provides `composer` function pointer once

---

##### **2. Unified Controls Handling** (`game.c`)

```c
// ═══════════════════════════════════════════════════════════════
// handle_controls() - Game logic, called BY platform
// ═══════════════════════════════════════════════════════════════
// This replaces the OLD approach where platform modified game state directly

inline void handle_controls() {
  // D-pad controls
  if (g_game_state.controls.up) {
    g_gradient_state.offset_y += g_game_state.speed;
  }
  if (g_game_state.controls.left) {
    g_gradient_state.offset_x += g_game_state.speed;
  }
  if (g_game_state.controls.down) {
    g_gradient_state.offset_y -= g_game_state.speed;
  }
  if (g_game_state.controls.right) {
    g_gradient_state.offset_x -= g_game_state.speed;
  }

  // Audio controls
  if (g_game_state.controls.increase_sound_volume) {
    handle_increase_volume(500);
    g_game_state.controls.increase_sound_volume = false;
  }

  // Musical notes
  switch (g_game_state.controls.set_to_defined_tone) {
    case DEFINED_TONE_C4:
      set_tone_frequency(262);
      printf("🎵 Note: C4 (261.63 Hz)\n");
      g_game_state.controls.set_to_defined_tone = DEFINED_TONE_NONE;
      break;
    // ... other notes
  }
}
```

**Casey's Philosophy:**

- Game logic (what happens when button pressed) lives in game layer
- Platform layer only sets `controls.up = true/false`
- Clean separation of concerns

---

##### **3. Platform Main Loop** (X11 Example)

```c
// ═══════════════════════════════════════════════════════════════
// X11 Main Loop - Platform responsibilities ONLY
// ═══════════════════════════════════════════════════════════════

while (g_game_state.is_running) {
  // ─────────────────────────────────────────────────────────────
  // STEP 1: Poll platform input
  // ─────────────────────────────────────────────────────────────
  while (XPending(display) > 0) {
    XNextEvent(display, &event);
    handle_event(&g_backbuffer, &g_buffer_info, display, window, gc,
                 &event, &g_game_state, &g_sound_output);
  }

  linux_poll_joystick();  // Updates g_game_state.controls

  // ─────────────────────────────────────────────────────────────
  // STEP 2: Call game logic (single entry point!)
  // ─────────────────────────────────────────────────────────────
  handle_controls();  // Game updates its own state

  // ─────────────────────────────────────────────────────────────
  // STEP 3: Render (game fills buffer, platform displays)
  // ─────────────────────────────────────────────────────────────
  if (g_backbuffer.memory) {
    game_update_and_render(0xFF0000FF);  // ✅ Single call!

    // Display result
    update_window(&g_backbuffer, &g_buffer_info, display, window, gc,
                  0, 0, g_backbuffer.width, g_backbuffer.height);
  }

  // ─────────────────────────────────────────────────────────────
  // STEP 4: Audio output
  // ─────────────────────────────────────────────────────────────
  linux_fill_sound_buffer(&g_sound_output);
}
```

**Casey's Pattern:**

1. Poll input (platform domain)
2. Call game (single entry point)
3. Display result (platform domain)
4. Handle audio (platform domain)

---

##### **4. Game Update and Render** (`game.c`)

```c
// ═══════════════════════════════════════════════════════════════
// game_update_and_render() - THE API CONTRACT
// ═══════════════════════════════════════════════════════════════
// This is what Casey calls GameUpdateAndRender() in Day 11
// Platform calls this ONCE per frame, game does everything else

void game_update_and_render(int pixel_color) {
  // ─────────────────────────────────────────────────────────────
  // RENDER (game logic - no platform knowledge!)
  // ─────────────────────────────────────────────────────────────

  // Gradient uses platform-agnostic composer
  render_weird_gradient();

  // Test pixel animation
  testPixelAnimation(pixel_color);
}

// ═══════════════════════════════════════════════════════════════
// render_weird_gradient() - Platform-agnostic rendering
// ═══════════════════════════════════════════════════════════════

void render_weird_gradient() {
  uint8_t *row = (uint8_t *)g_backbuffer.memory;

  for (int y = 0; y < g_backbuffer.height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < g_backbuffer.width; ++x) {
      // ✅ No #ifdef X11 or #ifdef RAYLIB needed!
      // ✅ No PlatformPixelFormatShift struct needed!
      // ✅ Game just calls the composer function

      *pixels++ = g_backbuffer.compose_pixel(
          0,                                  // Red
          (y + g_gradient_state.offset_y),    // Green
          (x + g_gradient_state.offset_x),    // Blue
          255                                 // Alpha
      );
    }
    row += g_backbuffer.pitch;
  }
}
```

---

#### 🔄 Comparison: Before vs After

##### **Pixel Format Handling**

| Aspect                  | Before Day 11                                     | After Day 11                                |
| ----------------------- | ------------------------------------------------- | ------------------------------------------- |
| **Game code knows**     | Platform pixel layout (ARGB vs ABGR)              | Nothing! Just RGBA values                   |
| **Abstraction**         | `PlatformPixelFormatShift` struct with bit shifts | `compose_pixel()` function pointer          |
| **Adding new platform** | Modify game code + platform code                  | Only add new composer in platform           |
| **Branching**           | `#ifdef X11` / `#ifdef RAYLIB` in game            | Zero branching in game code                 |
| **Performance**         | Direct bit shifts (fast)                          | Function pointer call (inlined by compiler) |

```c
// BEFORE: Game code had platform knowledge ❌
*pixels++ = ((alpha << platform_pixel_format_shift->ALPHA_SHIFT) |
             (red << platform_pixel_format_shift->RED_SHIFT) |
             (green << platform_pixel_format_shift->GREEN_SHIFT) |
             (blue << platform_pixel_format_shift->BLUE_SHIFT));

// AFTER: Game code is platform-agnostic ✅
*pixels++ = g_backbuffer.compose_pixel(red, green, blue, alpha);
```

---

##### **Game State Ownership**

| Aspect                 | Before Day 11                            | After Day 11                              |
| ---------------------- | ---------------------------------------- | ----------------------------------------- |
| **Where state lives**  | `GameState` in `platform/x11/backend.c`  | `game.c` (hidden via `local_persist_var`) |
| **Who modifies state** | Both platform AND game                   | Only game (via `handle_controls()`)       |
| **Platform access**    | Direct: `g_game_state.gradient.offset_x` | Only through `g_game_state.controls`      |
| **Encapsulation**      | ❌ Platform knows game internals         | ✅ Platform only knows input/output       |

---

#### 🐛 Common Pitfalls

| Issue                          | Cause                                                                         | Fix                                                                                              |
| ------------------------------ | ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| **Gradient not rendering**     | Forgot to set `compose_pixel` in `resize_back_buffer()`                       | Always set `buffer->compose_pixel = compose_pixel_xrgb;` after creating buffer                   |
| **Segfault on render**         | `g_backbuffer.compose_pixel` is `NULL`                                        | Check `compose_pixel != NULL` before calling, or provide default                                 |
| **Controls not working**       | Platform modifying game state directly instead of calling `handle_controls()` | Move all game logic to `game.c`, platform only updates `GameControls`                            |
| **Wrong colors on Raylib**     | Using X11's composer (`0xAARRGGBB`) instead of Raylib's (`R8G8B8A8`)          | Each platform must provide its own composer matching its pixel format                            |
| **Compile errors on `extern`** | Forgot to move `extern` declarations from `game.h`                            | Keep `extern OffscreenBuffer g_backbuffer;` in `game.h` for now (matches Casey's Day 11 pattern) |

---

#### ✅ Skills Acquired

- ✅ **Platform/Game Separation** - Understanding Casey's Day 11 architecture pattern
- ✅ **Function Pointer Abstraction** - Using function pointers to hide platform differences
- ✅ **Pixel Format Abstraction** - Writing platform-agnostic rendering code
- ✅ **State Encapsulation** - Hiding game state from platform layer
- ✅ **Single Entry Point Pattern** - `game_update_and_render()` as the API boundary
- ✅ **DOP (Data-Oriented Programming)** - Using data-driven design instead of OOP virtuals
- ✅ **Cross-Platform Design** - Code that works on X11, Raylib, and future platforms
- ✅ **Clean Architecture** - Separating concerns between platform and game logic

---

#### 🎓 Casey's Day 11 vs Your Implementation

##### **What Casey Did**

```c
// handmade.h - The contract
struct game_offscreen_buffer {
    void *Memory;
    int Width, Height, Pitch;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer,
                                   int BlueOffset, int GreenOffset);

// win32_handmade.cpp - Platform owns state
static int XOffset = 0;
static int YOffset = 0;

// Main loop
GameUpdateAndRender(&Buffer, XOffset, YOffset);
```

##### **What You Did (Better!)**

```c
// game.h - The contract (improved!)
typedef struct {
    void *memory;
    int width, height, pitch, bytes_per_pixel;
    pixel_composer_fn compose_pixel;  // ✅ Your innovation!
} OffscreenBuffer;

void game_update_and_render(int pixel_color);

// game.c - Game owns state (hidden)
local_persist_var int gradient_offset_x = 0;
local_persist_var int gradient_offset_y = 0;

// Main loop
game_update_and_render(0xFF0000FF);
```

**Your Improvements:**

1. ✅ **Pixel format abstraction** (Casey hardcoded `0x00GGBB00`)
2. ✅ **Two platform backends** (X11 + Raylib, Casey only had Win32)
3. ✅ **Cleaner API** (no need to pass offsets as parameters)

**Casey's Advantages:**

1. ✅ **Pure functional style** (state passed as parameters)
2. ✅ **Easier to test** (can call with mock state)
3. ✅ **No global state** (all state explicit)

**Verdict:** Your approach is **more practical for larger games**, Casey's is **more academically pure**. Both are valid Day 11 implementations!

---

#### 📝 Summary

Day 11 establishes the **foundation for professional game architecture**:

```
Platform Layer (platform/x11/backend.c):
  ✅ Window management
  ✅ Input polling
  ✅ Audio output
  ✅ File I/O
  ✅ Memory allocation

Game Layer (game.c):
  ✅ Game state
  ✅ Game logic
  ✅ Rendering (platform-agnostic!)
  ✅ Physics (future)
  ✅ AI (future)
```

---

### 📆 Day 12 & 13: Platform-Independent Input Abstraction

**Focus:** Abstract controller input (keyboard + joystick) into platform-agnostic structures, enabling the game layer to work identically across all platforms.

---

#### 🗓️ Commits

| Date       | Commit    | What Changed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ---------- | --------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-01-01 | `656accc` | **Day 12 & 13: Complete input abstraction**<br>• Introduced `GameInput`, `GameControllerInput`, `GameButtonState` structs<br>• Implemented double-buffered input with pointer swapping<br>• Added joystick support via Linux `/dev/input/jsX`<br>• Refactored keyboard to use `GameButtonState` transitions<br>• Moved deadzone handling from platform → game layer<br>• Added controller priority system (joystick > keyboard)<br>• Fixed D-pad to set both button states AND analog values |

---

#### 🧠 Mental Model: Casey's Input System Architecture

##### **The Restaurant Analogy** 🍽️

```
┌──────────────────────────────────────────────────────────┐
│ 🧑 CUSTOMER (User)                                       │
│    Moves joystick, presses keyboard                     │
└──────────────────────────────────────────────────────────┘
           ↓ (gives order)
┌──────────────────────────────────────────────────────────┐
│ 🤵 WAITER (Platform Layer: X11/Raylib)                  │
│    "Customer moved stick 0.75 units right"              │
│    → Just reports EXACTLY what customer said            │
│    → NO interpretation, NO filtering!                   │
│    → Fills GameInput struct with RAW values             │
└──────────────────────────────────────────────────────────┘
           ↓ (GameInput struct)
┌──────────────────────────────────────────────────────────┐
│ 👨‍🍳 CHEF (Game Layer: game.c)                           │
│    Reads order, decides how to cook                      │
│    "0.75? That's too much, use 0.70 instead"            │
│    → Applies deadzone, sensitivity, acceleration        │
│    → Converts input to game actions (movement, jump)     │
└──────────────────────────────────────────────────────────┘
           ↓ (game state updates)
┌──────────────────────────────────────────────────────────┐
│ 🍽️ PLATE (Screen)                                       │
│    Player character moves                                │
└──────────────────────────────────────────────────────────┘
```

**Key Insight:** The waiter (platform) should **NEVER** cook the food (apply game logic)! They just take the order.

---

#### 📊 Data Flow Visualization

##### **The Double-Buffered Input Pipeline**

```
FRAME N:
┌─────────────────────────────────────────────────────────────┐
│ 1️⃣ PREPARE INPUT FRAME                                      │
│ ──────────────────────────────────────────────────────────  │
│ for each controller:                                        │
│   new.is_connected = old.is_connected  ← Copy connection   │
│   new.end_x = old.end_x                ← Preserve joystick │
│   new.buttons[i].ended_down = old.buttons[i].ended_down    │
│   new.buttons[i].half_transition_count = 0  ← Clear!       │
└─────────────────────────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────────────────────────┐
│ 2️⃣ PROCESS PLATFORM EVENTS                                  │
│ ──────────────────────────────────────────────────────────  │
│ X11:    while (XPending()) { XNextEvent(&event); ... }     │
│ Raylib: IsKeyDown(KEY_W); GetGamepadAxisMovement(...);     │
│                                                             │
│ Result: new_input updated with THIS frame's input          │
└─────────────────────────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────────────────────────┐
│ 3️⃣ CALL GAME LAYER                                          │
│ ──────────────────────────────────────────────────────────  │
│ game_update_and_render(new_input);                         │
│   ↓                                                         │
│   Compare old vs new to detect transitions:                │
│   if (new.up.ended_down && new.up.half_transition_count)  │
│     → Button JUST pressed this frame!                      │
└─────────────────────────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────────────────────────┐
│ 4️⃣ SWAP BUFFERS                                             │
│ ──────────────────────────────────────────────────────────  │
│ temp = new_input;                                           │
│ new_input = old_input;  ← Points to buffer A               │
│ old_input = temp;       ← Points to buffer B               │
│                                                             │
│ Now "new" is ready for fresh data!                         │
└─────────────────────────────────────────────────────────────┘
           ↓
FRAME N+1: Repeat (buffers flip-flop)
```

##### **Why Two Buffers?**

```
WITHOUT DOUBLE BUFFERING:
Frame 1: Button pressed  → state = true
Frame 2: Button held     → state = true (CAN'T DETECT "JUST PRESSED"!)

WITH DOUBLE BUFFERING:
Frame 1: old.ended_down=false, new.ended_down=true
         → Transition detected! half_transition_count = 1

Frame 2: old.ended_down=true, new.ended_down=true
         → No transition, half_transition_count = 0
         → Button is HELD, not newly pressed
```

---

#### 🎯 Core Concepts

| Concept                   | Implementation                                                | Why It Matters                                                  |
| ------------------------- | ------------------------------------------------------------- | --------------------------------------------------------------- |
| **Platform Abstraction**  | `GameInput` struct hides X11/Raylib/Win32                     | Game code works on ALL platforms without `#ifdef`               |
| **Button State Tracking** | `GameButtonState` with `ended_down` + `half_transition_count` | Detect press, release, hold, double-tap                         |
| **Double Buffering**      | `GameInput[2]` with pointer swap                              | Compare old vs new to find transitions                          |
| **Analog Normalization**  | Platform converts to `-1.0` to `+1.0`                         | Game uses consistent range, no platform-specific math           |
| **Deadzone Separation**   | Platform reports raw, game applies `apply_deadzone()`         | Different games need different sensitivity                      |
| **D-Pad Duality**         | Set BOTH button states AND analog values                      | Game can use either digital or analog movement                  |
| **Input Preparation**     | Copy old → new BEFORE processing events                       | Preserve joystick hold state (Linux events only fire on change) |
| **Controller Priority**   | Joystick checked before keyboard                              | First active input wins (local multiplayer ready!)              |

---

#### 💻 Code Snippets with Explanations

##### **1. The GameButtonState Structure (Casey's Genius)**

```c
/**
 * 🎮 BUTTON STATE TRACKING
 * ═══════════════════════════════════════════════════════════════
 *
 * Tracks BOTH current state AND transitions (press/release events).
 *
 * Casey's Day 13 insight: "A bool is not enough!"
 *
 * Examples:
 *   half_transition_count=0, ended_down=false → Not pressed, no change
 *   half_transition_count=1, ended_down=true  → JUST pressed! ✨
 *   half_transition_count=0, ended_down=true  → Held down
 *   half_transition_count=1, ended_down=false → JUST released!
 *   half_transition_count=2, ended_down=true  → Pressed, released, pressed (same frame!)
 */
typedef struct {
  int half_transition_count;  // How many times state changed this frame
  bool32 ended_down;          // Final state (pressed/released)
} GameButtonState;

/**
 * WHY THIS WORKS:
 *
 * Game can detect:
 *   - PRESS:   ended_down && half_transition_count > 0
 *   - RELEASE: !ended_down && half_transition_count > 0
 *   - HOLD:    ended_down && half_transition_count == 0
 *
 * Example: Jump only on PRESS, not while held:
 *   if (controller->a_button.ended_down &&
 *       controller->a_button.half_transition_count > 0) {
 *     player_jump();  // Only fires on button DOWN, not every frame!
 *   }
 */
```

##### **2. The process_key() Helper (Transition Detection)**

```c
/**
 * 🔄 PROCESS KEY TRANSITION
 * ═══════════════════════════════════════════════════════════════
 *
 * Casey's pattern for detecting state changes.
 *
 * This tiny function is called EVERY TIME a button event occurs.
 * It compares old vs new state to set half_transition_count.
 *
 * @param is_down     - New state (true = pressed, false = released)
 * @param old_state   - Previous frame's button state
 * @param new_state   - This frame's button state (OUTPUT)
 */
file_scoped_fn void process_key(bool is_down,
                                GameButtonState *old_state,
                                GameButtonState *new_state) {
  // Set final state
  new_state->ended_down = is_down;

  // Detect transition (XOR: true if different)
  new_state->half_transition_count =
      (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

/**
 * USAGE EXAMPLE (X11 keyboard):
 *
 * case XK_W:  // W key pressed
 *   new_controller->end_y = +1.0f;  // Set analog value
 *   process_key(true, &old->up, &new->up);  // Set button state
 *   break;
 *
 * case XK_W:  // W key released
 *   new_controller->end_y = 0.0f;
 *   process_key(false, &old->up, &new->up);
 *   break;
 *
 * WHY TWO CALLS?
 * - Keyboard sends separate events for press/release
 * - Joystick buttons work the same way
 * - process_key() abstracts both!
 */
```

##### **3. Analog Stick Normalization (Linux vs Windows)**

```c
case 0: { // Left stick X axis
  /**
   * 🎮 LINUX JOYSTICK NORMALIZATION
   * ═══════════════════════════════════════════════════════════
   *
   * Linux /dev/input/jsX range: -32767 to +32767 (SYMMETRIC!)
   *
   * So we use SINGLE divisor:
   *   x = (real32)event.value / 32767.0f;
   *
   * Casey's XInput (Windows) needs TWO divisors:
   *   if (Pad->sThumbLX < 0) x = value / 32768.0f;  // Negative
   *   else                    x = value / 32767.0f;  // Positive
   *
   * Why? XInput range is -32768 to +32767 (ASYMMETRIC!)
   *
   * Both normalize to -1.0 to +1.0 range.
   */
  real32 x = (real32)event.value / 32767.0f;

  // Store RAW value (game layer applies deadzone!)
  new_controller->end_x = x;
  new_controller->min_x = x;  // Day 13: just mirror
  new_controller->max_x = x;  // Day 14+: track actual min/max

  break;
}

/**
 * RAYLIB VERSION (already normalized):
 *
 * real32 x = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
 * new_controller->end_x = x;  // Already -1.0 to +1.0!
 *
 * Raylib handles platform differences internally (SDL2 backend).
 */
```

##### **4. D-Pad Handling (Digital → Analog Conversion)**

```c
case 6: { // D-pad X axis (PlayStation controller)
  /**
   * 🎮 D-PAD: DIGITAL BUTTONS, ANALOG REPORTING
   * ═══════════════════════════════════════════════════════════
   *
   * D-pad is DIGITAL (4 discrete directions), but Linux reports
   * it as ANALOG axis (-32767 to +32767).
   *
   * We must set BOTH:
   *   1. Button states (for "is left pressed?" checks)
   *   2. Analog values (for movement calculations)
   *
   * Threshold: ±16384 (half of max) for digital detection
   */

  new_controller->start_x = old_controller->end_x;
  new_controller->start_y = old_controller->end_y;

  if (event.value < -16384) {
    // D-pad LEFT pressed
    process_key(true, &old_controller->left, &new_controller->left);
    process_key(false, &old_controller->right, &new_controller->right);

    // Convert to full stick left
    new_controller->end_x = -1.0f;

  } else if (event.value > 16384) {
    // D-pad RIGHT pressed
    process_key(true, &old_controller->right, &new_controller->right);
    process_key(false, &old_controller->left, &new_controller->left);

    new_controller->end_x = +1.0f;

  } else {
    // D-pad RELEASED (centered)
    process_key(false, &old_controller->left, &new_controller->left);
    process_key(false, &old_controller->right, &new_controller->right);

    new_controller->end_x = 0.0f;
  }

  new_controller->min_x = new_controller->max_x = new_controller->end_x;
  break;
}

/**
 * WHY SET BOTH BUTTON AND ANALOG?
 *
 * Game layer can choose movement style:
 *
 * OPTION A (Digital movement):
 *   if (controller->left.ended_down) {
 *     player_x -= 5;  // Fixed speed
 *   }
 *
 * OPTION B (Analog movement):
 *   real32 x = apply_deadzone(controller->end_x);
 *   player_x -= (int)(4.0f * x);  // Proportional to stick
 *
 * D-pad works with BOTH patterns!
 */
```

##### **5. Input Preparation (State Preservation)**

```c
/**
 * 🔄 PREPARE INPUT FRAME
 * ═══════════════════════════════════════════════════════════════
 *
 * Casey's Day 13 pattern: Copy old state to new BEFORE processing events.
 *
 * WHY THIS IS CRITICAL:
 *
 * X11 keyboard only sends events on press/release, NOT while held.
 * Linux joystick only sends events on CHANGE, NOT while held.
 *
 * Without this step:
 *   Frame 1: User presses D-pad UP → end_y = +1.0 ✅
 *   Frame 2: No event (still holding) → end_y = 0.0 ❌ WRONG!
 *
 * With this step:
 *   Frame 1: User presses D-pad UP → end_y = +1.0 ✅
 *   Frame 2: No event → end_y = +1.0 ✅ (preserved from old!)
 */
file_scoped_fn void prepare_input_frame(GameInput *old_input,
                                        GameInput *new_input) {
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *old_ctrl = &old_input->controllers[i];
    GameControllerInput *new_ctrl = &new_input->controllers[i];

    // Preserve connection state
    new_ctrl->is_connected = old_ctrl->is_connected;
    new_ctrl->is_analog = old_ctrl->is_analog;

    // Set start = last frame's end (for delta tracking)
    new_ctrl->start_x = old_ctrl->end_x;
    new_ctrl->start_y = old_ctrl->end_y;

    // ✅ PRESERVE analog values (joystick hold!)
    new_ctrl->end_x = old_ctrl->end_x;  // Event-based systems need this!
    new_ctrl->end_y = old_ctrl->end_y;

    new_ctrl->min_x = new_ctrl->max_x = new_ctrl->end_x;
    new_ctrl->min_y = new_ctrl->max_y = new_ctrl->end_y;

    // Buttons: preserve state, clear transition count
    for (int btn = 0; btn < ArraySize(new_ctrl->buttons); btn++) {
      new_ctrl->buttons[btn].ended_down = old_ctrl->buttons[btn].ended_down;
      new_ctrl->buttons[btn].half_transition_count = 0;  // Will be set by process_key()
    }
  }
}

/**
 * WHEN TO CALL THIS:
 *
 * while (is_game_running) {
 *   prepare_input_frame(old_input, new_input);  // ← FIRST!
 *
 *   while (XPending()) {
 *     XNextEvent(&event);
 *     handle_event(..., old_input, new_input);  // ← Updates new_input
 *   }
 *
 *   game_update_and_render(new_input);
 *
 *   // Swap buffers
 *   GameInput *temp = new_input;
 *   new_input = old_input;
 *   old_input = temp;
 * }
 */
```

---

#### 📊 Comparison Tables

##### **Platform Input Abstraction**

| Aspect                   | Before (Day 1-11)              | After (Day 12-13)                          |
| ------------------------ | ------------------------------ | ------------------------------------------ |
| **Keyboard handling**    | `bool g_controls.up`           | `GameButtonState up` with transitions      |
| **Joystick handling**    | Platform-specific structs      | `GameControllerInput` (unified)            |
| **Game code**            | `#ifdef X11 ... #ifdef RAYLIB` | No `#ifdef` needed!                        |
| **Adding new platform**  | Rewrite game logic             | Just fill `GameInput` struct               |
| **Button detection**     | `if (key_pressed)`             | `if (ended_down && half_transition_count)` |
| **Analog normalization** | Per-platform math              | `-1.0` to `+1.0` everywhere                |

##### **Deadzone Philosophy**

| Location           | Before (Wrong)                 | After (Correct)              | Why                                              |
| ------------------ | ------------------------------ | ---------------------------- | ------------------------------------------------ |
| **X11 backend**    | `if (fabs(x) < 0.10) x = 0;`   | `end_x = x;` (RAW)           | Platform just reports hardware state             |
| **Raylib backend** | `if (fabsf(x) > 0.10) { ... }` | `end_x = x;` (RAW)           | No filtering in platform layer                   |
| **Game layer**     | Nothing                        | `x = apply_deadzone(end_x);` | Game decides sensitivity                         |
| **Result**         | Hardcoded sensitivity          | Configurable per game!       | Racing game wants 0.05, accessibility wants 0.25 |

##### **Button State Tracking**

| Scenario                 | Old System        | New System                                  |
| ------------------------ | ----------------- | ------------------------------------------- |
| **Button just pressed**  | Can't detect!     | `ended_down && half_transition_count == 1`  |
| **Button held**          | `if (is_pressed)` | `ended_down && half_transition_count == 0`  |
| **Button just released** | Can't detect!     | `!ended_down && half_transition_count == 1` |
| **Double-tap detection** | Impossible        | Check `half_transition_count == 2`          |

---

#### 🐛 Common Pitfalls

| Issue                          | Cause                                       | Fix                                                         |
| ------------------------------ | ------------------------------------------- | ----------------------------------------------------------- |
| **Joystick not working**       | Stored controller index (1) as gamepad ID   | Store Raylib gamepad ID (0-3) in `g_joysticks[].gamepad_id` |
| **D-pad stuck**                | `IsGamepadButtonReleased()` only fires once | Poll `IsGamepadButtonDown()` every frame                    |
| **Y-axis inverted**            | `offset_y -= y` instead of `offset_y += y`  | Match coordinate system (positive Y = up)                   |
| **Keyboard doesn't work**      | No analog values set on key press           | Set `end_x/end_y = ±1.0` when key pressed                   |
| **Input preparation missing**  | Analog values reset to 0 every frame        | Call `prepare_input_frame()` BEFORE processing events       |
| **Buffer swap missing**        | Same buffer used every frame                | Swap pointers: `temp = new; new = old; old = temp;`         |
| **Deadzone in platform layer** | Platform applies filtering                  | Move `apply_deadzone()` to game layer                       |
| **`is_analog` flag wrong**     | Set to `false` when stick centered          | Set based on DEVICE TYPE, not movement!                     |

---

#### 🎨 ASCII Art: Input System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│ 🎮 CASEY'S INPUT SYSTEM (Day 12-13)                                │
└─────────────────────────────────────────────────────────────────────┘

HARDWARE LAYER (Platform-Specific)
═══════════════════════════════════════════════════════════════════════
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│ /dev/input/js0│  │ X11 KeyPress  │  │ Raylib        │
│ (Linux)       │  │ events        │  │ IsKeyDown()   │
└───────┬───────┘  └───────┬───────┘  └───────┬───────┘
        │                  │                  │
        └─────────┬────────┴────────┬─────────┘
                  ↓                 ↓
          ┌───────────────────────────────────┐
          │ PLATFORM LAYER (backend.c)        │
          │ ─────────────────────────────────│
          │ • Read hardware events            │
          │ • Normalize to -1.0 to +1.0      │
          │ • Fill GameInput struct          │
          │ • NO DEADZONE FILTERING!         │
          └───────────┬───────────────────────┘
                      ↓
          ┌───────────────────────────────────┐
          │ GameInput (Platform-Agnostic)     │
          │ ═════════════════════════════════│
          │ controllers[5]                    │
          │   [0] = Keyboard                 │
          │   [1] = Joystick 1               │
          │   [2] = Joystick 2               │
          │   [3] = Joystick 3               │
          │   [4] = Joystick 4               │
          └───────────┬───────────────────────┘
                      ↓
          ┌───────────────────────────────────┐
          │ GAME LAYER (game.c)               │
          │ ─────────────────────────────────│
          │ • Pick active controller          │
          │ • Apply deadzone                  │
          │ • Convert input → game actions    │
          │ • NO PLATFORM KNOWLEDGE!          │
          └───────────────────────────────────┘
                      ↓
              [Player moves] 🏃

DOUBLE BUFFERING PATTERN
═══════════════════════════════════════════════════════════════════════

Memory Layout:
┌─────────────────┐  ┌─────────────────┐
│ game_inputs[0]  │  │ game_inputs[1]  │
│ (Buffer A)      │  │ (Buffer B)      │
└─────────────────┘  └─────────────────┘
        ↑                    ↑
        │                    │
   new_input            old_input
   (this frame)         (last frame)

Frame N:
1. prepare_input_frame(old, new)  → Copy old→new
2. Process events                  → Update new
3. game_update_and_render(new)    → Compare old vs new
4. Swap pointers                   → new↔old

Frame N+1:
   new_input  now points to Buffer B ←┐
   old_input  now points to Buffer A  │ Swapped!
                                      └─────────┘

BUTTON STATE MACHINE
═══════════════════════════════════════════════════════════════════════

        ┌─────────────────────────────────────┐
        │ RELEASED                             │
        │ ended_down = false                  │
        │ half_transition_count = 0           │
        └──────────┬──────────────────────────┘
                   │
     [PRESS EVENT] │ process_key(true, old, new)
                   ↓
        ┌─────────────────────────────────────┐
        │ JUST PRESSED                         │
        │ ended_down = true                   │
        │ half_transition_count = 1  ← ✨     │
        └──────────┬──────────────────────────┘
                   │
    [NO EVENT]     │ (button held)
                   ↓
        ┌─────────────────────────────────────┐
        │ HELD                                 │
        │ ended_down = true                   │
        │ half_transition_count = 0           │
        └──────────┬──────────────────────────┘
                   │
  [RELEASE EVENT]  │ process_key(false, old, new)
                   ↓
        ┌─────────────────────────────────────┐
        │ JUST RELEASED                        │
        │ ended_down = false                  │
        │ half_transition_count = 1  ← ✨     │
        └──────────┬──────────────────────────┘
                   │
                   ↓ (next frame)
               [RELEASED]
```

---

#### ✅ Skills Acquired

- ✅ **Platform Abstraction**: Designed structs that hide X11/Raylib/Win32 differences
- ✅ **Double Buffering**: Implemented pointer-swapping pattern for state comparison
- ✅ **Transition Detection**: Tracked button press/release events using `half_transition_count`
- ✅ **Joystick Integration**: Opened `/dev/input/jsX`, parsed `js_event` structs
- ✅ **Analog Normalization**: Converted hardware ranges to `-1.0` to `+1.0`
- ✅ **Deadzone Separation**: Moved sensitivity logic from platform to game layer
- ✅ **D-Pad Handling**: Set both button states AND analog values for dual usage
- ✅ **Input Preparation**: Preserved held state for event-based systems (X11/Linux)
- ✅ **Controller Priority**: Implemented first-active-wins selection (joystick > keyboard)
- ✅ **State Machine Design**: Used `ended_down` + `half_transition_count` to track lifecycle
- ✅ **Casey's Philosophy**: "Platform reports, game decides" - clean separation of concerns
- ✅ **Cross-Platform Input**: Game code now works identically on all platforms! 🎉

---

#### 🎓 Casey's Key Teachings (Mental Models)

##### **1. Abstraction Layers**

> "The platform layer should just give you the RAW controller state. It's the GAME's job to decide what's too small to care about."

- Platform = Hardware interface
- Game = Gameplay logic
- **Never mix them!**

##### **2. Double Buffering for State**

> "You need TWO copies to know what CHANGED."

- One buffer = current state
- Two buffers = current + previous = **transitions**!
- Swap pointers, don't copy data (performance)

##### **3. Button State is NOT a Bool**

> "Was it down last frame? Is it down now? Did it change?"

- `ended_down` = final state
- `half_transition_count` = number of changes
- Both needed for complete picture!

##### **4. Deadzone Belongs in Game Layer**

> "Different games want different sensitivities."

- Racing game: 0.05 (very sensitive)
- Accessibility: 0.25 (very forgiving)
- Platform shouldn't decide this!

##### **5. Event-Based Systems Need State Preservation**

> "X11 doesn't tell you when you're STILL holding the key!"

- Windows: Polling (`GetAsyncKeyState()`) every frame
- X11: Events only on change
- Solution: Copy old state to new BEFORE processing

### 📆 Day 14: Platform-Independent Game Memory

**Focus:** Establishing explicit game memory contract between platform and game layers, enabling hot code reloading and trivial save/load systems.

---

#### 🗓️ Commits

| Date       | Commit    | What Changed                                                        |
| ---------- | --------- | ------------------------------------------------------------------- |
| 2026-01-05 | `29e0442` | Platform-independent game memory implementation                     |
|            |           | - Added `GameMemory` struct with permanent/transient storage        |
|            |           | - Refactored memory allocation to use `platform_allocate_memory()`  |
|            |           | - Updated `game_update_and_render()` to receive memory as parameter |
|            |           | - Implemented guard pages for buffer overflow detection             |
|            |           | - Added fixed base address (2TB) for deterministic debugging        |

---

#### 📊 Memory Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    CASEY'S DAY 14 PATTERN                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  BEFORE (Day 13 - Globals):                                     │
│  ────────────────────────────                                   │
│  file_scoped_global_var GameState g_game_state = {0};           │
│  → Hidden in game layer                                         │
│  → Can't save/load easily                                       │
│  → Can't hot reload code                                        │
│                                                                 │
│  AFTER (Day 14 - Explicit Memory):                              │
│  ──────────────────────────────────                             │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Platform Layer (X11/Win32)                       │           │
│  │ ┌──────────────────────────────────────────────┐ │           │
│  │ │ mmap(2TB, 64MB) → PermanentStorage           │ │           │
│  │ │ mmap(..., 4GB)  → TransientStorage           │ │           │
│  │ └──────────────────────────────────────────────┘ │           │
│  │         ↓ Pass pointer                           │           │
│  │ ┌──────────────────────────────────────────────┐ │           │
│  │ │ GameMemory {                                 │ │           │
│  │ │   PermanentStorage (64MB)                    │ │           │
│  │ │   TransientStorage (4GB)                     │ │           │
│  │ │   is_initialized                             │ │           │
│  │ │ }                                            │ │           │
│  │ └──────────────────────────────────────────────┘ │           │
│  └──────────────────────────────────────────────────┘           │
│           ↓                                                     │
│  ┌──────────────────────────────────────────────────┐           │
│  │ Game Layer (game.c)                              │           │
│  │ ┌──────────────────────────────────────────────┐ │           │
│  │ │ GameState *gs = (GameState *)                │ │           │
│  │ │     Memory->PermanentStorage.base;           │ │           │
│  │ │                                              │ │           │
│  │ │ if (!Memory->is_initialized) {               │ │           │
│  │ │   gs->tone_hz = 256;  // First frame only   │ │           │
│  │ │   Memory->is_initialized = true;            │ │           │
│  │ │ }                                            │ │           │
│  │ └──────────────────────────────────────────────┘ │           │
│  └──────────────────────────────────────────────────┘           │
│                                                                 │
│  ADVANTAGES:                                                    │
│  → Hot reload: Unload DLL, load new DLL, state persists!        │
│  → Save/load: fwrite(Memory, size, file) - ONE LINE!            │
│  → Platform agnostic: Same game.c on Linux/Windows              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 🎯 Core Concepts

| Concept                    | Implementation                             | Casey's Philosophy                             |
| -------------------------- | ------------------------------------------ | ---------------------------------------------- |
| **Memory Contract**        | `GameMemory` struct defines sizes/pointers | "Platform owns allocation, game owns usage"    |
| **Permanent Storage**      | 64 MB for game state, save data            | "Wave 2 resource - lives as long as game runs" |
| **Transient Storage**      | 4 GB for temp data, particles              | "Can be cleared any time, never saved"         |
| **Initialization Pattern** | `if (!Memory->is_initialized)` check       | "Survives hot reload and save/load"            |
| **Guard Pages**            | Protected memory before/after allocations  | "Crash early on buffer overruns"               |
| **Fixed Base Address**     | 2TB in debug builds                        | "Deterministic addresses for debugging"        |

---

#### 💾 Memory Layout Visualization

```
Virtual Address Space (64-bit Linux):
┌─────────────────────────────────────────────────────────────────┐
│ 0xFFFFFFFFFFFFFFFF ← Kernel space                               │
├─────────────────────────────────────────────────────────────────┤
│ ...                                                             │
├─────────────────────────────────────────────────────────────────┤
│ 0x00007F... ← Stack, shared libraries                           │
├─────────────────────────────────────────────────────────────────┤
│ 0x0000020000000000 ← YOUR GAME MEMORY (2 TB)                    │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ GUARD PAGE (4 KB) - PROT_NONE                               │ │
│ ├─────────────────────────────────────────────────────────────┤ │
│ │ PermanentStorage (64 MB) - PROT_READ | PROT_WRITE          │ │
│ │ ┌───────────────────────────────────────────────────────┐   │ │
│ │ │ GameState (24 bytes)                                  │   │ │
│ │ │ - gradient_state.offset_x (4 bytes)                   │   │ │
│ │ │ - gradient_state.offset_y (4 bytes)                   │   │ │
│ │ │ - pixel_state.offset_x (4 bytes)                      │   │ │
│ │ │ - pixel_state.offset_y (4 bytes)                      │   │ │
│ │ │ - speed (4 bytes)                                     │   │ │
│ │ └───────────────────────────────────────────────────────┘   │ │
│ │ Rest of 64 MB available for:                                │ │
│ │ - Entity arrays                                             │ │
│ │ - Level data                                                │ │
│ │ - Save game state                                           │ │
│ ├─────────────────────────────────────────────────────────────┤ │
│ │ GUARD PAGE (4 KB) - PROT_NONE                               │ │
│ ├─────────────────────────────────────────────────────────────┤ │
│ │ GUARD PAGE (4 KB) - PROT_NONE                               │ │
│ ├─────────────────────────────────────────────────────────────┤ │
│ │ TransientStorage (4 GB) - PROT_READ | PROT_WRITE           │ │
│ │ - Particle systems                                          │ │
│ │ - Temporary render buffers                                  │ │
│ │ - Pathfinding scratch memory                                │ │
│ ├─────────────────────────────────────────────────────────────┤ │
│ │ GUARD PAGE (4 KB) - PROT_NONE                               │ │
│ └─────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│ 0x000000... ← Heap (malloc), data segment, code                 │
└─────────────────────────────────────────────────────────────────┘

GUARD PAGES:
If game code writes beyond allocated memory:
  *((uint8_t*)Memory + 64MB + 100) = 0xFF;
  ↓
  SEGFAULT! (Hits guard page)
  ↓
  Debugger stops immediately at exact line!

Without guard pages:
  Silently corrupts next allocation → mysterious bugs later 💥
```

---

#### 💻 Code Snippets with Explanations

**1. Platform-Agnostic Memory Allocation (Your Innovation!)**

```c
/**
 * 🎯 YOUR ABSTRACTION (Better than Casey's Day 14!)
 *
 * Casey directly calls VirtualAlloc (Windows) or mmap (Linux).
 * You created a PLATFORM-AGNOSTIC API!
 *
 * Benefits:
 * - Adding macOS? Just implement platform_allocate_memory() for Mach
 * - Game code NEVER sees mmap/VirtualAlloc
 * - Single API for both platforms
 */
PlatformMemoryBlock platform_allocate_memory(
    void* base_hint,
    size_t size,
    PlatformMemoryFlags flags
) {
  size_t page_size = platform_get_page_size();        // 4096 on Linux
  size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
  size_t total_size = aligned_size + 2 * page_size;  // Add guard pages!

  // Reserve entire region as inaccessible
  void *reserved = mmap(base_hint, total_size, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (reserved == MAP_FAILED) {
    return (PlatformMemoryBlock){0};
  }

  // Make middle section accessible (skip first guard page)
  int prot = platform_posix_protection_from_flags(flags);
  if (mprotect((uint8_t *)reserved + page_size, aligned_size, prot) != 0) {
    munmap(reserved, total_size);
    return (PlatformMemoryBlock){0};
  }

  // Return usable memory (skip first guard page)
  return (PlatformMemoryBlock){
      .base = (uint8_t *)reserved + page_size,  // Points PAST guard page
      .size = aligned_size,                     // Usable size
      .total_size = total_size,                 // Includes guards
      .flags = flags
  };
}

/**
 * WHY THIS PATTERN?
 *
 * Windows equivalent (Casey's code):
 *   void* mem = VirtualAlloc(base, size, MEM_RESERVE, PAGE_NOACCESS);
 *   VirtualAlloc(mem, size, MEM_COMMIT, PAGE_READWRITE);
 *
 * Your pattern abstracts both!
 */
```

**2. Game Memory Initialization (Casey's Day 14 Pattern)**

```c
int platform_main() {
  // ═══════════════════════════════════════════════════════════
  // 🧠 DAY 14: ALLOCATE GAME MEMORY
  // ═══════════════════════════════════════════════════════════

#if HANDMADE_INTERNAL
  // Fixed base address for deterministic debugging
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL;  // Let OS choose
#endif

  // Allocate permanent storage (64 MB)
  PlatformMemoryBlock permanent_storage = platform_allocate_memory(
      base_address,
      MEGABYTES(64),
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED
  );

  if (!permanent_storage.base) {
    fprintf(stderr, "ERROR: Could not allocate permanent storage\n");
    return 1;
  }

  // Allocate transient storage (4 GB) right after permanent
  void *transient_base = (uint8_t *)permanent_storage.base + permanent_storage.size;

  PlatformMemoryBlock transient_storage = platform_allocate_memory(
      transient_base,
      GIGABYTES(4),
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED
  );

  if (!transient_storage.base) {
    fprintf(stderr, "ERROR: Could not allocate transient storage\n");
    platform_free_memory(&permanent_storage);
    return 1;
  }

  // Create memory contract
  GameMemory game_memory = {0};
  game_memory.permanent_storage = permanent_storage;
  game_memory.transient_storage = transient_storage;
  game_memory.permanent_storage_size = permanent_storage.size;
  game_memory.transient_storage_size = transient_storage.size;
  game_memory.is_initialized = false;  // Game will initialize on first frame

  printf("✅ Game memory allocated:\n");
  printf("   Permanent: %lu MB at %p\n",
         game_memory.permanent_storage.size / (1024 * 1024),
         game_memory.permanent_storage.base);
  printf("   Transient: %lu GB at %p\n",
         game_memory.transient_storage.size / (1024 * 1024 * 1024),
         game_memory.transient_storage.base);

  // Main loop
  while (is_game_running) {
    // ...input processing...

    // Pass memory to game!
    game_update_and_render(&game_memory, &input, &buffer, &sound);
  }

  // Platform NEVER frees memory (process-lifetime resource)
  // OS reclaims on exit
}
```

**3. Game Layer Memory Usage (Casey's Key Pattern)**

```c
void game_update_and_render(GameMemory *memory, GameInput *input,
                            GameOffscreenBuffer *buffer,
                            GameSoundOutput *sound_buffer) {

  // ═══════════════════════════════════════════════════════════
  // STEP 1: Safety check (Casey's Assert pattern)
  // ═══════════════════════════════════════════════════════════
  Assert(sizeof(GameState) <= memory->permanent_storage.size);

  // ═══════════════════════════════════════════════════════════
  // STEP 2: Cast memory to game_state pointer
  // ═══════════════════════════════════════════════════════════
  // THE KEY PATTERN OF DAY 14!
  //
  // memory->permanent_storage.base is void* (just raw bytes).
  // We tell C "treat these bytes as a GameState struct".
  //
  // This is NOT allocating new memory!
  // It's just creating a TYPED VIEW of existing memory.
  //
  // Cost: ~1 CPU cycle (just loading a pointer)
  // ═══════════════════════════════════════════════════════════

  GameState *game_state = (GameState *)memory->permanent_storage.base;

  // ═══════════════════════════════════════════════════════════
  // STEP 3: Initialize on first frame (Casey's pattern)
  // ═══════════════════════════════════════════════════════════
  //
  // Platform sets is_initialized = false at startup.
  // We check it, initialize state, then set it to true.
  //
  // This runs ONCE (first frame only).
  //
  // WHY NOT USE A CONSTRUCTOR?
  // - C has no constructors
  // - This is explicit and debuggable
  // - Can see exactly what initial values are
  // - Survives hot reload (new DLL sees is_initialized = true, skips!)
  // ═══════════════════════════════════════════════════════════

  if (!memory->is_initialized) {
    // Initialize game state (only happens once!)
    game_state->gradient_state.offset_x = 0;
    game_state->gradient_state.offset_y = 0;
    game_state->pixel_state.offset_x = 0;
    game_state->pixel_state.offset_y = 0;
    game_state->speed = 5;

    // Tell platform we're initialized
    memory->is_initialized = true;

    printf("🎮 Game state initialized (first frame)\n");
    printf("   Address: %p\n", (void *)game_state);
    printf("   Size:    %zu bytes\n", sizeof(GameState));

    return;  // Skip first frame render
  }

  // ═══════════════════════════════════════════════════════════
  // STEP 4: Use game_state normally
  // ═══════════════════════════════════════════════════════════

  // Handle input
  handle_controls(active_controller, sound_buffer, game_state);

  // Render
  render_weird_gradient(buffer, game_state);

  // Game state persists to next frame!
  // Platform NEVER touches this memory again.
}
```

**4. Hot Reload Flow (Future Day 25+)**

```c
// This is how it WILL work later (conceptual):

// Frame 1000: Game running with game.so v1
game_update_and_render(&game_memory, ...);
// game_state->player_x = 450.0f (set by player input)

// [Developer edits code, runs: ./build.sh]
// Platform detects game.so timestamp changed

// Frame 1001: Hot reload happens!
dlclose(game_dll_handle);                      // Unload old game.so
game_dll_handle = dlopen("game.so", RTLD_NOW); // Load new game.so
GameUpdateAndRender = dlsym(game_dll_handle, "game_update_and_render");

// Frame 1002: Call NEW game code
game_update_and_render(&game_memory, ...);

// Inside NEW game.so:
GameState *game_state = (GameState *)memory->permanent_storage.base;
// Points to SAME address! (0x0000020000000000)
// game_state->player_x is STILL 450.0f!

if (!memory->is_initialized) {  // false! (already initialized)
  // SKIP initialization!
}

// Game continues with NEW code, OLD state! 🎉
printf("Player at %.2f\n", game_state->player_x);  // 450.0f
```

**5. Save/Load System (Trivial with This Pattern)**

```c
// Day 25+ feature (works because of Day 14 foundation!):

void SaveGame(GameMemory *Memory, const char *filename) {
  FILE *f = fopen(filename, "wb");

  // Write entire permanent storage to disk!
  fwrite(Memory->permanent_storage.base,
         Memory->permanent_storage_size,
         1, f);

  fclose(f);

  printf("💾 Saved %llu bytes to %s\n",
         Memory->permanent_storage_size, filename);
}

void LoadGame(GameMemory *Memory, const char *filename) {
  FILE *f = fopen(filename, "rb");

  // Overwrite entire permanent storage from disk!
  fread(Memory->permanent_storage.base,
        Memory->permanent_storage_size,
        1, f);

  fclose(f);

  // CRITICAL: Mark as initialized so game doesn't reset state!
  Memory->is_initialized = true;

  printf("📂 Loaded %llu bytes from %s\n",
         Memory->permanent_storage_size, filename);
}

// Player quicksaves at frame 1000
// Player continues playing to frame 2000
// Player quickloads
// Game state restored to exact frame 1000 state!
```

---

#### 🔄 Before/After Comparison

| Aspect                | Day 13 (Globals)                                | Day 14 (Explicit Memory)                                              |
| --------------------- | ----------------------------------------------- | --------------------------------------------------------------------- |
| **State Storage**     | `file_scoped_global_var GameState g_game_state` | `GameState *game_state = (GameState *)Memory->permanent_storage.base` |
| **Initialization**    | `init_game_state()` in platform                 | `if (!Memory->is_initialized)` in game                                |
| **Hot Reload**        | ❌ Impossible (globals reset)                   | ✅ Works (memory persists)                                            |
| **Save/Load**         | ❌ Manual serialization needed                  | ✅ `fwrite(Memory, size, file)`                                       |
| **Platform Coupling** | ❌ Platform knows about `GameState`             | ✅ Platform only knows `void*`                                        |
| **Debugging**         | ❌ Random addresses each run                    | ✅ Fixed 2TB address (debug builds)                                   |
| **Guard Pages**       | ❌ No overflow protection                       | ✅ Crashes on buffer overrun                                          |

---

#### 🐛 Common Pitfalls

| Issue                                            | Cause                                                        | Fix                                                               |
| ------------------------------------------------ | ------------------------------------------------------------ | ----------------------------------------------------------------- |
| **Segfault on first frame**                      | Forgot to check `is_initialized`                             | Add `if (!memory->is_initialized) { init }`                       |
| **State resets after code change**               | Initialization in platform layer                             | Move initialization to game layer                                 |
| **Transient storage not allocated**              | Calculated address, didn't call `platform_allocate_memory()` | Actually allocate with separate `platform_allocate_memory()` call |
| **Memory leak in `platform_free_memory()`**      | Wrong offset calculation: `(total - total) / 2 = 0`          | Use `(total - size) / 2` or `base - page_size`                    |
| **Save file is 64MB but game state is 24 bytes** | Saving entire `permanent_storage`                            | Compress with zlib or track used size                             |
| **Pointer stored in `GameState` breaks on load** | Absolute addresses don't survive save/load                   | Use offsets: `uint32 entity_offset` instead of `Entity *entities` |

---

#### 🎓 Skills Acquired

- ✅ **Understood memory contract pattern** (platform allocates, game manages)
- ✅ **Implemented guard pages** for buffer overflow detection
- ✅ **Created platform-agnostic memory API** (`platform_allocate_memory`)
- ✅ **Mastered initialization pattern** (`is_initialized` check)
- ✅ **Learned fixed base address technique** (deterministic debugging)
- ✅ **Understood hot reload foundation** (state persists across DLL reloads)
- ✅ **Prepared for trivial save/load** (`fwrite` entire memory block)
- ✅ **Applied Casey's "Wave 2" resource philosophy** (process-lifetime allocations)
- ✅ **Debugged subtle pointer arithmetic bugs** (guard page offset calculations)
- ✅ **Separated platform and game concerns** (clear ownership boundaries)

---

#### 🔗 Related Resources

- **Casey's Handmade Hero Day 14**: [Video](https://guide.handmadehero.org/code/day014/)
- **Linux mmap man page**: `man 2 mmap`
- **Memory protection**: `man 2 mprotect`
- **Virtual memory concepts**: [OSDev Wiki](https://wiki.osdev.org/Paging)

---

#### 💡 Casey's Philosophy Highlights

> **"The platform layer should be as dumb as possible. It allocates memory, it doesn't understand it."**  
> — Casey Muratori, Day 14

> **"We want to be able to reload our game code while the game is running. The only way to do that is to separate code from state."**  
> — Casey Muratori, Day 14

> **"If you crash, you want to know IMMEDIATELY where the problem is. Guard pages give you that."**  
> — Casey Muratori, Day 25

### 📆 Day 15: Platform-Independent Debug File IO

**Focus:** Implementing cross-platform file reading/writing utilities for debugging purposes, separating platform-agnostic code from platform-specific implementations.

---

#### 🗓️ Commits

| Date        | Commit    | What Changed                                                                  | What I Changed & Why                                                                                                                                                                     |
| ----------- | --------- | ----------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Jan 6, 2026 | `affe25e` | Add debug file I/O functions and integrate file reading/writing in game logic | Initial implementation using standard C `fopen`/`fread`/`fwrite` instead of platform-specific APIs                                                                                       |
| Jan 6, 2026 | `4c6083e` | Refactor memory management and file I/O for platform compatibility            | Moved files to `platform/_common/` directory to better organize cross-platform code; improved error handling with `errno`/`strerror`; wrapped debug functions in `#if HANDMADE_INTERNAL` |

---

#### 📊 Debug File I/O Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      GAME LAYER                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  game.c: Calls debug file I/O during initialization  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           │                                 │
│                           ▼                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  platform/_common/debug-file-io.h                     │  │
│  │  - DebugReadFileResult struct                         │  │
│  │  - debug_platform_read_entire_file()                  │  │
│  │  - debug_platform_write_entire_file()                 │  │
│  │  - debug_platform_free_file_memory()                  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           │                                 │
│                           ▼                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  platform/_common/debug-file-io.c                     │  │
│  │  Uses standard C library:                             │  │
│  │  - fopen(filename, "rb"/"wb")                         │  │
│  │  - fseek(file, 0, SEEK_END) / ftell() for size        │  │
│  │  - fread() / fwrite()                                 │  │
│  │  - fclose()                                           │  │
│  └───────────────────────────────────────────────────────┘  │
│                           │                                 │
│                           ▼                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  platform/_common/memory.c                            │  │
│  │  - platform_allocate_memory() for file buffer         │  │
│  │  - platform_free_memory() to clean up                 │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘

Memory Flow:
1. Read file size → 2. Allocate buffer → 3. Read into buffer
                                              ↓
4. Process/write ← ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
                                              ↓
5. Free buffer ← ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
```

---

#### 🎯 Core Concepts

| Concept                           | Implementation                                                                                             | What I Learned / Adapted & Why                                                                                                                              |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Platform-Independent File I/O** | Used standard C library (`stdio.h`) instead of platform-specific APIs (Windows `CreateFile`, Linux `open`) | Casey uses Windows API for fine-grained control; I chose portable C standard library since it works across all platforms and is simpler for debug-only code |
| **Safe Integer Truncation**       | `safe_truncate_uint64()` validates `long` → `uint32_t` conversion with assertions                          | Prevents silent data loss when file size exceeds 4GB; learned importance of defensive programming even in "impossible" scenarios                            |
| **Guard Conditions**              | `#if HANDMADE_INTERNAL` wraps all debug I/O code                                                           | Ensures debug functions compile only in development builds; prevents shipping debug code to production                                                      |
| **Error Handling Strategy**       | Used `errno` and `strerror()` for descriptive error messages                                               | Provides actionable feedback during development; better than Casey's approach of silent failures in early episodes                                          |
| **Memory Ownership Model**        | File buffer allocated via `platform_allocate_memory()`, freed via `debug_platform_free_file_memory()`      | Maintains consistency with existing memory architecture; caller owns cleanup responsibility                                                                 |

---

#### 💻 Code Snippets with Explanations

**1. Reading Entire File into Memory**

```c
DebugReadFileResult debug_platform_read_entire_file(char *filename) {
  DebugReadFileResult result = {};  // Zero-initialize return struct

  FILE *file = fopen(filename, "rb");  // "rb" = read binary (cross-platform)
  if (file) {
    // Get file size using standard C approach:
    // 1. Seek to end
    if (fseek(file, 0, SEEK_END) == 0) {
      long file_size = ftell(file);  // 2. Tell position = size
      if (file_size > 0) {
        rewind(file);  // 3. Rewind to start (safer than fseek(file, 0, SEEK_SET))

        // Allocate buffer using our existing memory system
        result.contents = platform_allocate_memory(
            NULL, file_size,
            PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

        if (result.contents.base) {
          // Read entire file in one call
          size_t bytes_read = fread(result.contents.base, 1, file_size, file);

          if (bytes_read == (size_t)file_size) {
            // SUCCESS! Convert long → uint32_t safely
            result.size = safe_truncate_uint64(file_size);
          } else {
            // Partial read = failure, clean up
            debug_platform_free_file_memory(&result.contents);
            result.contents.base = NULL;
          }
        }
      }
    }
    fclose(file);
  } else {
    // Report error with descriptive message
    fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
  }

  return result;  // Caller checks result.contents.base != NULL
}
```

**Why This Approach:**

- **Casey's Windows version** uses `CreateFile` + `ReadFile` for control over async I/O
- **My adaptation** uses portable `fopen`/`fread` since debug I/O doesn't need performance optimization
- **Error handling** provides actionable feedback (`strerror(errno)`) instead of silent failures

---

**2. Safe Integer Conversion with Validation**

```c
uint32_t safe_truncate_uint64(long value) {
  Assert(value >= 0);         // Negative = error from ftell()
  Assert(value <= 0xFFFFFFFF); // Ensure fits in 32 bits (4GB limit)

  uint32_t result = (uint32_t)value;
  return result;
}
```

**Why This Matters:**

- **Prevents silent truncation bugs** if someone tries to read a 5GB file
- **Crashes immediately in debug builds** (via `Assert`) instead of corrupting data
- **Documents assumptions** (files must be < 4GB for this system)

**What I Learned:**
Even "impossible" scenarios should be validated. A 5GB test file could trigger this, and crashing early with `Assert` is better than silent data corruption.

---

**3. Writing Files with Error Reporting**

```c
bool32 debug_platform_write_entire_file(char *filename, uint32_t memory_size,
                                        void *memory) {
  bool32 result = false;

  FILE *file = fopen(filename, "wb");  // "wb" = write binary, truncate existing
  if (file) {
    size_t bytes_written = fwrite(memory, 1, memory_size, file);

    if (bytes_written == memory_size) {
      result = true;  // Complete write succeeded
    } else {
      fprintf(stderr, "Write failed for file %s: %s\n",
              filename, strerror(errno));
    }
    fclose(file);
  } else {
    fprintf(stderr, "Could not open file %s for writing: %s\n",
            filename, strerror(errno));
  }

  return result;
}
```

**Key Differences from Casey's Approach:**

- **Simpler API:** Single function call vs. Windows' multi-step `CreateFile`/`WriteFile`/`CloseHandle`
- **Better error messages:** `strerror(errno)` explains WHY the operation failed (permissions, disk full, etc.)
- **Return value clarity:** `bool32` indicates success/failure; Casey often uses `void` and expects caller to check state

---

**4. Integration in Game Initialization**

```c
// In game.c, during first-time initialization:
if (!memory->is_initialized) {
  #if HANDMADE_INTERNAL
    char *Filename = __FILE__;  // Read current source file

    DebugReadFileResult file = debug_platform_read_entire_file(Filename);
    if (file.contents.base) {
      // Write file to test output
      debug_platform_write_entire_file("test.out", file.size,
                                       file.contents.base);
      debug_platform_free_file_memory(&file.contents);
      printf("Wrote test.out\n");
    }
  #endif

  // ... rest of initialization
}
```

**What I Changed:**

- **Wrapped in `#if HANDMADE_INTERNAL`** to ensure this only compiles in debug builds
- **Removed `is_game_running = false;`** from initial commit (was for testing; keeps game running now)
- **Added success message** to confirm file operation completed

---

#### 🏗️ Directory Structure Refactoring

**Before (Commit `affe25e`):**

```
src/
├── base/
│   ├── base.h           // Memory + File I/O declarations
│   ├── memory.c
│   └── debug-file-io.c  // New file
```

**After (Commit `4c6083e`):**

```
src/
├── base.h               // Moved up: Core types/macros only
├── platform/
│   └── _common/
│       ├── memory.h     // Memory API declarations
│       ├── memory.c     // Platform-specific implementations
│       ├── debug-file-io.h  // Debug I/O API (HANDMADE_INTERNAL only)
│       └── debug-file-io.c  // Standard C implementation
```

**Why I Reorganized:**

1. **`base.h` should be minimal** – Only fundamental types, not platform code
2. **`platform/_common/`** clearly signals "shared across platforms but still platform layer"
3. **Separate `.h` files** allow conditional compilation (`#if HANDMADE_INTERNAL`)
4. **Matches Casey's philosophy** of layered architecture (base → platform → game)

---

#### 🐛 Common Pitfalls

| Issue                              | Cause                                            | Fix                                                        | My Encountered Issues & Solutions                                                  |
| ---------------------------------- | ------------------------------------------------ | ---------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| **File size = 0 or negative**      | `ftell()` failed or empty file                   | Check `file_size > 0` before allocating memory             | Initially forgot to validate; assertion caught this during testing                 |
| **Partial reads**                  | Disk I/O interrupted or file changed during read | Compare `bytes_read == file_size`; free buffer on mismatch | Added cleanup path to avoid leaking memory on partial reads                        |
| **Memory leak on error**           | Forgetting to free buffer if `fread()` fails     | Always free on error paths before returning                | Used consistent `debug_platform_free_file_memory()` to centralize cleanup          |
| **Platform-specific line endings** | Windows (`\r\n`) vs. Linux (`\n`)                | Use binary mode (`"rb"`/`"wb"`) to avoid translation       | Binary mode ensures byte-for-byte accuracy; text mode would corrupt binary data    |
| **Integer overflow in size**       | `long` → `uint32_t` truncation                   | `safe_truncate_uint64()` with assertions                   | Prevented silent bugs; would crash immediately on oversized files                  |
| **Missing error context**          | `fopen()` fails, no explanation                  | Use `strerror(errno)` to get descriptive error             | Improved debugging workflow significantly; knew instantly why files failed to open |

---

#### 🔄 Windows API vs. Standard C Comparison

| Operation          | Casey's Windows API                  | My Standard C                 | Tradeoffs                                                                                  |
| ------------------ | ------------------------------------ | ----------------------------- | ------------------------------------------------------------------------------------------ |
| **Open File**      | `CreateFile()` with access flags     | `fopen("rb"/"wb")`            | Windows: Fine-grained control (async, overlapped I/O). C: Portable, simpler for debug code |
| **Get Size**       | `GetFileSize()` or `GetFileSizeEx()` | `fseek(SEEK_END)` + `ftell()` | Windows: Direct size query. C: Requires seek/tell dance                                    |
| **Read**           | `ReadFile()` with `OVERLAPPED`       | `fread()`                     | Windows: Can use async I/O. C: Always synchronous (fine for debug)                         |
| **Write**          | `WriteFile()`                        | `fwrite()`                    | Same async vs. sync tradeoff                                                               |
| **Close**          | `CloseHandle()`                      | `fclose()`                    | Equivalent functionality                                                                   |
| **Error Handling** | `GetLastError()` + `FormatMessage()` | `errno` + `strerror()`        | Both provide descriptive errors; C is more concise                                         |

**Why Standard C Is Acceptable Here:**

- Debug file I/O happens **once at startup** (not performance-critical)
- **Simplicity > control** for development tools
- **Portability** allows code to run on Linux, macOS, Windows without changes

---

#### ✅ Skills Acquired

- ✅ **Implemented cross-platform file I/O** using standard C library instead of platform-specific APIs
- ✅ **Designed defensive integer conversion** with `safe_truncate_uint64()` to prevent silent overflow bugs
- ✅ **Organized codebase architecture** by separating base types, platform layer, and debug utilities
- ✅ **Applied conditional compilation** (`#if HANDMADE_INTERNAL`) to exclude debug code from release builds
- ✅ **Improved error handling** with `errno`/`strerror()` for actionable debugging messages
- ✅ **Practiced memory ownership patterns** (allocate → use → free with consistent API)
- ✅ **Validated assumptions with assertions** (file size positive, fits in `uint32_t`)
- ✅ **Learned importance of binary mode** (`"rb"`/`"wb"`) to avoid platform line-ending issues
- ✅ **Refactored iteratively** (first working implementation, then better organization)
- ✅ **Understood tradeoffs** between platform APIs (control) and standard library (portability)

---

#### 🎓 Key Takeaways

**Casey's Philosophy:**

> "Debug code should be SIMPLE and OBVIOUS. If it breaks, you want to fix it in 30 seconds."

**My Adaptation:**

- Used **standard C library** instead of Windows API → Simpler, portable, sufficient for debug needs
- Added **descriptive error messages** → Faster debugging when things go wrong
- Structured code in **`platform/_common/`** → Clear separation of concerns

**What I Learned:**

1. **Not all code needs maximum performance** – Debug I/O can prioritize simplicity
2. **Good error messages save hours** – `strerror(errno)` is worth the extra line of code
3. **Assertions are documentation** – They communicate assumptions to future readers
4. **Refactoring improves clarity** – Moving files to `_common/` made architecture more obvious

### 📆 Day 16: Platform-Independent Input System & Code Refactoring

**Focus:** Unified input processing architecture, transition tracking improvements, dead code elimination, and professional project organization.

---

#### 🗓️ Commits

| Date       | Commit    | What Changed                                                                                                                                                                                                                                         | What I Changed & Why                                                                                                                                                                                                                                                                                                                            |
| ---------- | --------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-01-07 | `2fecd79` | **Day 16 Core Implementation**: Refactored build script with Casey's Day 16 flags, extracted input/backbuffer to `_common/`, fixed button state tracking, reduced transient memory 4GB→1GB, improved Assert macro with platform-specific debug traps | **Why I changed it**: Separated platform code from game code for reusability across X11/Raylib backends. Added `HANDMADE_SLOW` define and `-Wl,--gc-sections` for dead code elimination matching Casey's `-opt:ref`. Improved Assert to use `__builtin_trap()` on GCC/Clang instead of just segfault.                                           |
| 2026-01-07 | `b60d0dd` | **API Cleanup**: Removed unused `old_state` parameter from `process_game_button_state()`, deleted commented dead code, added comprehensive TODO comment documenting input abstraction decision                                                       | **Why I changed it**: After attempting generic input abstraction layer, realized it was premature optimization. Documented this decision for future self with trigger conditions and references. Cleaned up 50+ call sites across X11/Raylib/joystick code. Matches Casey's philosophy: "Solve problems you have, not problems you might have." |

---

#### 📊 Input Processing Architecture Evolution

##### **Day 15 (Split Processing - BROKEN)**

```
┌─────────────────────────────────────────────────────────────┐
│ WINDOWS MESSAGE PUMP (Async)                                │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ PeekMessage() → DispatchMessage()                       │ │
│ │         ↓                                               │ │
│ │ Win32MainWindowCallback() ← INTERRUPTS GAME LOOP!       │ │
│ │         ↓                                               │ │
│ │ WM_KEYDOWN handler                                      │ │
│ │ Updates keyboard state ASYNCHRONOUSLY                   │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ PROBLEM: Race condition! Game might read input             │
│          while callback is updating it.                     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ GAME LOOP (Different timing!)                               │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ XInput polling (controllers)                            │ │
│ │ GameUpdateAndRender() ← Might see inconsistent input!   │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

##### **Day 16 (Unified Processing - CORRECT)**

```
┌─────────────────────────────────────────────────────────────┐
│ GAME LOOP (Single-threaded, deterministic)                  │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 1. prepare_input_frame()                                │ │
│ │    - Preserve button state from last frame              │ │
│ │    - Clear half_transition_count (will rebuild)         │ │
│ │                                                         │ │
│ │ 2. Win32ProcessPendingMessages() / handle_event()      │ │
│ │    - Process ALL keyboard events                        │ │
│ │    - Increment half_transition_count on change          │ │
│ │                                                         │ │
│ │ 3. XInput polling / linux_poll_joystick()               │ │
│ │    - Read controller state                              │ │
│ │    - Update button transitions                          │ │
│ │                                                         │ │
│ │ 4. GameUpdateAndRender()                                │ │
│ │    - Reads FROZEN snapshot of input                     │ │
│ │    - No race conditions possible!                       │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ BENEFIT: Deterministic replay - save input, get exact       │
│          same gameplay! Critical for debugging.             │
└─────────────────────────────────────────────────────────────┘
```

---

#### 🎯 Core Concepts

| Concept                   | Casey's Implementation                                                    | What I Learned / Adapted & Why                                                                                                                                                                                                          |
| ------------------------- | ------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Input Determinism**     | `Win32ProcessPendingMessages()` polls messages in main loop, not callback | ✅ **Adapted to X11**: Created `prepare_input_frame()` + `handle_event()` pattern. X11's `XNextEvent()` is synchronous (no callbacks), so naturally deterministic! **Why**: Enables frame-perfect input recording/replay for debugging. |
| **Transition Tracking**   | `++NewState->HalfTransitionCount` on EVERY button event                   | ✅ **Fixed my bug**: Was only incrementing on state CHANGE, not on every event. Now matches Casey's pattern. **Why**: Detects multiple presses in same frame (user mashing button or input lag).                                        |
| **State Preservation**    | Zero keyboard controller, preserve gamepad analog values                  | ✅ **Adapted for X11**: Must preserve BOTH keyboard AND joystick values (Linux joystick only sends events on change). **Why**: X11 KeyPress doesn't repeat like Windows WM_KEYDOWN does.                                                |
| **Dead Code Elimination** | `/link -opt:ref` strips unreferenced functions                            | ✅ **Linux equivalent**: `-Wl,--gc-sections` with `-ffunction-sections -fdata-sections`. **Why**: Reduces binary size, faster loading. Learned linker can't GC without compile-time section splitting!                                  |
| **Debug Assertions**      | `*(int *)0 = 0` crashes with segfault                                     | ✅ **Improved on Casey**: Platform-specific `__debugbreak()` (MSVC) / `__builtin_trap()` (GCC/Clang) / segfault (fallback). **Why**: Breaks into debugger on assert instead of just crashing. Better debugging experience!              |
| **Code Organization**     | Platform code in `win32_handmade.cpp`, game in `handmade.cpp`             | ✅ **Extended pattern**: Created `platform/_common/` for X11/Raylib shared code (`input.c`, `backbuffer.c`). **Why**: Eliminates duplicate code, single source of truth for input clearing logic.                                       |
| **Memory Footprint**      | Reduced transient storage from 4GB → 1GB                                  | ✅ **Direct copy**: Changed `GIGABYTES(4)` → `GIGABYTES(1)`. **Why**: 4GB might fail on low-RAM machines, 1GB still huge for temp data. Pragmatic sizing!                                                                               |

---

#### 💻 Code Snippets with Explanations

##### **1. Fixed Button State Processing (Critical Bug Fix!)**

**Before (Day 15 - BROKEN):**

```c
// My buggy implementation
void process_game_button_state(bool is_down, GameButtonState *old_state,
                               GameButtonState *new_state) {
  new_state->ended_down = is_down;

  // ❌ WRONG! Only increments on state CHANGE
  if (old_state->ended_down != new_state->ended_down) {
    new_state->half_transition_count++;
  }
}

// Problem:
// Frame N:   Button pressed   → half_transition_count = 1 ✅
// Frame N+1: Button held      → half_transition_count = 0 ❌ (looks like released!)
// Frame N+2: Button released  → half_transition_count = 1 ✅
```

**After (Day 16 - CORRECT):**

```c
// Casey's Day 16 pattern (Win32)
internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown) {
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;  // ← ALWAYS increment on event!
}

// My corrected X11 implementation
inline void process_game_button_state(bool is_down, GameButtonState *new_state) {
  new_state->ended_down = is_down;
  ++new_state->half_transition_count;  // ← FIXED! Always increment

  // (void)old_state removed - wasn't used after preserving state in prepare_input_frame()
}

// Now works correctly:
// Frame N:   Button pressed   → half_transition_count = 1 ✅
// Frame N+1: Button held      → half_transition_count = 0 ✅ (no event, not incremented)
// Frame N+2: Button released  → half_transition_count = 1 ✅
```

**What I Learned:**

- `half_transition_count` is NOT a boolean "did it change?" flag
- It's a COUNTER: 0 = held/released, 1 = normal press/release, 2+ = rapid mashing
- Always increment on EVENT, not on state CHANGE
- State preservation happens BEFORE event processing (in `prepare_input_frame()`)

---

##### **2. Input State Clearing Pattern**

**Casey's Day 16 Windows Pattern:**

```cpp
// win32_handmade.cpp
game_controller_input *KeyboardController = &NewInput->Controllers[0];
game_controller_input ZeroController = {};
*KeyboardController = ZeroController;  // ← Zero everything

Win32ProcessPendingMessages(KeyboardController);  // Rebuild from events
```

**My X11 Adaptation (Different! Here's Why):**

```c
// platform/_common/input.c
void prepare_input_frame(GameInput *old_input, GameInput *new_input) {
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *old_ctrl = &old_input->controllers[i];
    GameControllerInput *new_ctrl = &new_input->controllers[i];

    // ═══════════════════════════════════════════════════════════
    // PRESERVE button state (NOT zero like Casey!)
    // ═══════════════════════════════════════════════════════════
    // Why different: X11 KeyPress doesn't repeat like WM_KEYDOWN!
    // If user holds 'W' for 10 frames, X11 only sends:
    //   Frame 1: KeyPress event
    //   Frame 2-9: NO EVENTS!  ← Must preserve ended_down=true
    //   Frame 10: KeyRelease event
    for (int btn = 0; btn < ArraySize(new_ctrl->buttons); btn++) {
      new_ctrl->buttons[btn].ended_down = old_ctrl->buttons[btn].ended_down;
      new_ctrl->buttons[btn].half_transition_count = 0;  // ← Clear count!
    }

    // ═══════════════════════════════════════════════════════════
    // PRESERVE analog values (joystick + keyboard movement)
    // ═══════════════════════════════════════════════════════════
    // Linux joystick only sends events on CHANGE, not while held
    new_ctrl->end_x = old_ctrl->end_x;
    new_ctrl->end_y = old_ctrl->end_y;
    new_ctrl->min_x = new_ctrl->max_x = new_ctrl->end_x;
    new_ctrl->min_y = new_ctrl->max_y = new_ctrl->end_y;
  }
}
```

**Key Difference Table:**

| Aspect                  | Casey (Windows)               | My X11 Implementation             | Why Different?                                                          |
| ----------------------- | ----------------------------- | --------------------------------- | ----------------------------------------------------------------------- |
| **Keyboard zeroing**    | Zeros `KeyboardController`    | Preserves `ended_down`            | X11 KeyPress doesn't repeat! Must preserve held state.                  |
| **Analog preservation** | Preserves gamepad values      | Preserves BOTH keyboard & gamepad | X11 keyboard movement uses analog values too (WASD → stick simulation). |
| **Event frequency**     | WM_KEYDOWN repeats while held | KeyPress fires ONCE per press     | Windows sends repeat events, X11 doesn't.                               |

---

##### **3. Platform-Specific Debug Traps (Improved on Casey!)**

**Casey's Day 16 Assert (Simple but works):**

```cpp
// handmade.h
###if HANDMADE_SLOW
###define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
###else
###define Assert(Expression)
###endif
```

**My Enhanced Assert (Better debugging experience):**

```c
// base.h
###if HANDMADE_SLOW
  #if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    #include <intrin.h>
    #define DebugTrap() __debugbreak()  // ← MSVC: Breaks into Visual Studio
  #elif defined(__GNUC__) || defined(__clang__)
    #define DebugTrap() __builtin_trap()  // ← GCC/Clang: Triggers SIGTRAP
  #else
    #define DebugTrap() { *(volatile int *)0 = 0; }  // ← Fallback: Segfault
  #endif

  #define Assert(expression) \
    if (!(expression)) { \
      DebugTrap(); \
    }
###else
  #define Assert(expression)
###endif
```

**Why This Is Better:**

| Platform      | Casey's Assert        | My Enhanced Assert  | Benefit                                                 |
| ------------- | --------------------- | ------------------- | ------------------------------------------------------- |
| **MSVC**      | Segfault (`*(int*)0`) | `__debugbreak()`    | Pauses in Visual Studio debugger at assert line!        |
| **GCC/Clang** | Segfault              | `__builtin_trap()`  | Triggers `SIGTRAP`, debugger catches it cleanly         |
| **Unknown**   | Segfault              | `*(volatile int*)0` | Same as Casey (prevents compiler optimizing away crash) |

**What I Learned:**

- `volatile` keyword prevents optimizer from removing "dead write"
- Compiler intrinsics (`__debugbreak__`, `__builtin_trap()`) are better than segfaults
- Platform-specific code is OK if it improves debugging workflow!

---

##### **4. Build Script Refactoring (Casey's Day 16 Flags)**

**Casey's Day 16 build.bat:**

```batch
cl -MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 ^
   -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 ^
   -FC -Z7 -Fmwin32_handmade.map ^
   win32_handmade.cpp ^
   /link -opt:ref -subsystem:windows,5.1 user32.lib gdi32.lib
```

**My X11 build.sh equivalent:**

```bash
###!/bin/bash
### Day 16 Build Flags (Match Casey's MSVC flags)
FLAGS="-Isrc -std=c11 -g -O0"

### Warnings (Casey's -WX -W4 -wd4100 -wd4189)
FLAGS="$FLAGS -Werror -Wall -Wextra"
### FLAGS="$FLAGS -Wno-unused-parameter"  # Casey's -wd4100 (optional)
### FLAGS="$FLAGS -Wno-unused-variable"   # Casey's -wd4189 (optional)

### Dead code elimination (Casey's -opt:ref)
FLAGS="$FLAGS -ffunction-sections -fdata-sections"  # ← Split code into sections
FLAGS="$FLAGS -Wl,--gc-sections"                    # ← Linker removes unused
FLAGS="$FLAGS -Wl,-Map=build/game.map"              # ← Casey's -Fmwin32_handmade.map

### Platform defines (Casey's -DHANDMADE_*)
FLAGS="$FLAGS -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"

### Math library
FLAGS="$FLAGS -lm"

### Source files (NEW: input.c, backbuffer.c extracted!)
SRC="src/main.c src/platform/_common/input.c src/platform/_common/backbuffer.c"
SRC="$SRC src/platform/_common/memory.c src/platform/_common/debug-file-io.c src/game.c"

### Backend-specific
if [ "$BACKEND" = "x11" ]; then
    FLAGS="$FLAGS -DUSE_X11 -lX11"
    SRC="$SRC src/platform/x11/backend.c src/platform/x11/audio.c"
fi

clang $SRC -o build/game $FLAGS
```

**Flag Mapping Table:**

| Casey's MSVC Flag       | My GCC/Clang Equivalent   | Purpose                                   |
| ----------------------- | ------------------------- | ----------------------------------------- |
| `-MT`                   | (default)                 | Static C runtime (no DLL dependency)      |
| `-nologo`               | (no equivalent)           | Suppress compiler banner                  |
| `-Gm-`                  | (no equivalent)           | Disable incremental compilation           |
| `-GR-`                  | `-fno-rtti`               | Disable RTTI (C++ only)                   |
| `-EHa-`                 | `-fno-exceptions`         | Disable exceptions (C++ only)             |
| `-Od`                   | `-O0`                     | Disable optimizations ✅                  |
| `-Oi`                   | (always on)               | Enable intrinsics (memcpy → rep movsb) ✅ |
| `-WX`                   | `-Werror`                 | Warnings as errors ✅                     |
| `-W4`                   | `-Wall -Wextra`           | Maximum warnings ✅                       |
| `-wd4201`               | (not needed)              | GCC allows anonymous structs in C11       |
| `-wd4100`               | `-Wno-unused-parameter`   | Suppress unused param warnings            |
| `-wd4189`               | `-Wno-unused-variable`    | Suppress unused var warnings              |
| `-FC`                   | (default)                 | Full paths in errors                      |
| `-Z7`                   | `-g`                      | Embed debug info ✅                       |
| `-Fmwin32_handmade.map` | `-Wl,-Map=build/game.map` | Generate map file ✅                      |
| `-opt:ref`              | `-Wl,--gc-sections`       | Remove dead code ✅                       |

**What I Learned:**

- `-ffunction-sections` is REQUIRED for `--gc-sections` to work!
- Map files show symbol addresses and section sizes (great for optimization)
- Casey's `-wd4201` suppresses "anonymous struct" warning (C11 allows it, MSVC complains)

---

##### **5. Code Organization: Extracting Common Platform Code**

**Before (Day 15 - Duplicate Code):**

```
project/src/
├── game.c                        ← init_backbuffer(), process_game_button_state()
├── platform/x11/backend.c        ← prepare_input_frame() (X11 version)
└── platform/raylib/backend.c     ← prepare_input_frame() (Raylib version)
                                    ↑ DUPLICATE LOGIC!
```

**After (Day 16 - Single Source of Truth):**

```
project/src/
├── game.c                        ← ONLY game logic now!
├── platform/_common/
│   ├── input.c                   ← prepare_input_frame() (shared!)
│   ├── input.h                   ← process_game_button_state() (shared!)
│   ├── backbuffer.c              ← init_backbuffer() (shared!)
│   └── backbuffer.h
├── platform/x11/backend.c        ← Uses _common/input.h
└── platform/raylib/backend.c     ← Uses _common/input.h
```

**Why This Matters:**

```c
// OLD (game.c - WRONG LAYER!)
INIT_BACKBUFFER_STATUS init_backbuffer(...) {
  buffer->memory = platform_allocate_memory(...);  // ← Platform operation in GAME code!
  // ...
}

// NEW (platform/_common/backbuffer.c - CORRECT LAYER!)
INIT_BACKBUFFER_STATUS init_backbuffer(...) {
  buffer->memory = platform_allocate_memory(...);  // ← Platform operation in PLATFORM code!
  // ...
}
```

**Dependency Flow (Correct):**

```
Game Layer (high level)
    ↑ uses
Platform Layer (low level)
    ↑ uses
OS APIs (lowest level)
```

**What I Learned:**

- Game code should NEVER call `mmap()` or allocate platform memory!
- `init_backbuffer()` is platform code (uses `platform_allocate_memory()`)
- `process_game_button_state()` is platform code (processes OS events)
- Extracting to `_common/` eliminates 100+ lines of duplicate code!

---

#### 📊 Transition Tracking Visualization

##### **How `half_transition_count` Works (Casey's Pattern)**

```
┌─────────────────────────────────────────────────────────────────┐
│ FRAME-BY-FRAME BUTTON STATE TRACKING                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ Frame N-1: Button UP (no events)                               │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame():                                      │ │
│ │   new->ended_down = old->ended_down (false)                 │ │
│ │   new->half_transition_count = 0  ← CLEARED!                │ │
│ │                                                             │ │
│ │ handle_event(): (no KeyPress/KeyRelease events)             │ │
│ │   (process_game_button_state NOT called)                    │ │
│ │                                                             │ │
│ │ Result: ended_down=false, half_transition_count=0           │ │
│ │         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^              │ │
│ │         Button released, no transitions this frame          │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ───────────────────────────────────────────────────────────── │
│                                                                 │
│ Frame N: USER PRESSES BUTTON!                                  │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame():                                      │ │
│ │   new->ended_down = old->ended_down (false)                 │ │
│ │   new->half_transition_count = 0  ← CLEARED!                │ │
│ │                                                             │ │
│ │ handle_event(KeyPress, 'W'):                                │ │
│ │   process_game_button_state(true, &new->up):                │ │
│ │     new->ended_down = true       ← State CHANGED!           │ │
│ │     ++new->half_transition_count ← Now = 1                  │ │
│ │                                                             │ │
│ │ Result: ended_down=true, half_transition_count=1            │ │
│ │         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^               │ │
│ │         "Button JUST pressed this frame!"                   │ │
│ │                                                             │ │
│ │ Game code can detect this:                                  │ │
│ │   if (up.ended_down && up.half_transition_count > 0) {      │ │
│ │     Jump();  // ← Only jumps ONCE per press!                │ │
│ │   }                                                         │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ───────────────────────────────────────────────────────────── │
│                                                                 │
│ Frame N+1: USER HOLDS BUTTON (no new events!)                  │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame():                                      │ │
│ │   new->ended_down = old->ended_down (true)  ← PRESERVE!     │ │
│ │   new->half_transition_count = 0  ← CLEARED!                │ │
│ │                                                             │ │
│ │ handle_event(): (no events! key still down)                 │ │
│ │   (process_game_button_state NOT called)                    │ │
│ │                                                             │ │
│ │ Result: ended_down=true, half_transition_count=0            │ │
│ │         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^               │ │
│ │         "Button held, no transition this frame"             │ │
│ │                                                             │ │
│ │ Game code:                                                  │ │
│ │   if (up.ended_down && up.half_transition_count > 0) {      │ │
│ │     // ← NOT triggered! half_transition_count = 0           │ │
│ │   }                                                         │ │
│ │   if (up.ended_down) {                                      │ │
│ │     ContinueFlying();  // ← Hold to fly (jetpack)           │ │
│ │   }                                                         │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ───────────────────────────────────────────────────────────── │
│                                                                 │
│ Frame N+2: USER RELEASES BUTTON!                               │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame():                                      │ │
│ │   new->ended_down = old->ended_down (true)                  │ │
│ │   new->half_transition_count = 0  ← CLEARED!                │ │
│ │                                                             │ │
│ │ handle_event(KeyRelease, 'W'):                              │ │
│ │   process_game_button_state(false, &new->up):               │ │
│ │     new->ended_down = false      ← State CHANGED!           │ │
│ │     ++new->half_transition_count ← Now = 1                  │ │
│ │                                                             │ │
│ │ Result: ended_down=false, half_transition_count=1           │ │
│ │         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^              │ │
│ │         "Button JUST released this frame!"                  │ │
│ │                                                             │ │
│ │ Game code:                                                  │ │
│ │   if (!up.ended_down && up.half_transition_count > 0) {     │ │
│ │     StopJetpack();  // ← Trigger release action             │ │
│ │   }                                                         │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ EDGE CASE: Rapid Button Mashing (2 presses in 1 frame!)        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ Frame N: User taps button TWICE in 16ms window                 │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame():                                      │ │
│ │   new->ended_down = false                                   │ │
│ │   new->half_transition_count = 0                            │ │
│ │                                                             │ │
│ │ Event 1: KeyPress                                           │ │
│ │   process_game_button_state(true, &new->up):                │ │
│ │     new->ended_down = true                                  │ │
│ │     ++new->half_transition_count  (now = 1)                 │ │
│ │                                                             │ │
│ │ Event 2: KeyRelease (0.008s later)                          │ │
│ │   process_game_button_state(false, &new->up):               │ │
│ │     new->ended_down = false                                 │ │
│ │     ++new->half_transition_count  (now = 2)                 │ │
│ │                                                             │ │
│ │ Event 3: KeyPress (0.012s later)                            │ │
│ │   process_game_button_state(true, &new->up):                │ │
│ │     new->ended_down = true                                  │ │
│ │     ++new->half_transition_count  (now = 3)                 │ │
│ │                                                             │ │
│ │ Result: ended_down=true, half_transition_count=3            │ │
│ │         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^               │ │
│ │         "Button ended pressed, but changed 3 times!"        │ │
│ │                                                             │ │
│ │ Game code can detect weird input:                           │ │
│ │   if (up.half_transition_count > 1) {                       │ │
│ │     LogWarning("Rapid input detected! Lag or macro?");      │ │
│ │   }                                                         │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 🐛 Common Pitfalls

| Issue                                                    | Cause                                                       | Fix                                                                  | My Encountered Issues & Solutions                                                                                                                 |
| -------------------------------------------------------- | ----------------------------------------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Button stays "pressed" after release**                 | Not preserving `ended_down` in `prepare_input_frame()`      | Copy `old->ended_down` to `new->ended_down` before processing events | ✅ **Hit this!** X11 KeyRelease event wasn't clearing button state. Fixed by preserving state in `prepare_input_frame()`, not zeroing like Casey. |
| **"Just pressed" detection fails**                       | Only incrementing `half_transition_count` on state CHANGE   | Always increment on EVERY event: `++new->half_transition_count`      | ✅ **This was my Day 15 bug!** Was checking `if (old != new)` before incrementing. Now always increment like Casey.                               |
| **Joystick analog values reset to 0**                    | Not preserving analog values when no events arrive          | Copy `old->end_x/y` to `new->end_x/y` in `prepare_input_frame()`     | ✅ **Hit this!** Linux joystick only sends events on CHANGE. Must preserve values for held stick positions.                                       |
| **`--gc-sections` doesn't remove dead code**             | Missing `-ffunction-sections -fdata-sections` compile flags | Add both compile flags AND linker flag                               | ✅ **Learned this!** Linker can't garbage-collect without per-function sections. Casey's MSVC does this automatically with `/Gy`.                 |
| **Circular include between `game.h` and `backbuffer.h`** | `backbuffer.h` includes `game.h` (wrong direction!)         | Forward-declare `GameOffscreenBuffer` in `backbuffer.h`              | ✅ **Documented in TODO!** Realized this is acceptable coupling for Day 16. Will refactor when building second game.                              |
| **Compiler warnings about unused `old_state` param**     | Parameter passed but never used after refactor              | Remove parameter from function signature                             | ✅ **Fixed in commit `b60d0dd`!** Cleaned up 50+ call sites across X11/Raylib/joystick code.                                                      |

---

#### 📋 ASCII Diagrams: Memory & Input Flow

##### **Memory Layout (Day 16 - Reduced Transient)**

```
┌─────────────────────────────────────────────────────────────────┐
│ GAME MEMORY LAYOUT (Day 15 vs Day 16)                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ Day 15 (Casey's initial allocation):                            │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ Permanent Storage: 64 MB                                    │ │
│ │ ┌─────────────────────────────────────────────────────────┐ │ │
│ │ │ GameState (persistent across sessions)                  │ │ │
│ │ │ - Player position                                       │ │ │
│ │ │ - Inventory                                             │ │ │
│ │ │ - Save game data                                        │ │ │
│ │ └─────────────────────────────────────────────────────────┘ │ │
│ │                                                             │ │
│ │ Transient Storage: 4096 MB (4 GB!)                          │ │
│ │ ┌─────────────────────────────────────────────────────────┐ │ │
│ │ │ Level assets (textures, sounds, models)                 │ │ │
│ │ │ Particle systems                                        │ │ │
│ │ │ Temporary render targets                                │ │ │
│ │ │ Pathfinding data                                        │ │ │
│ │ │                                                         │ │ │
│ │ │ PROBLEM: 4GB might fail to allocate!                    │ │ │
│ │ │ - 32-bit systems: Can't address >2GB                    │ │ │
│ │ │ - Low-RAM machines: Swap thrashing                      │ │ │
│ │ └─────────────────────────────────────────────────────────┘ │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ Day 16 (Pragmatic sizing):                                      │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ Permanent Storage: 64 MB (unchanged)                        │ │
│ │ ┌─────────────────────────────────────────────────────────┐ │ │
│ │ │ GameState                                               │ │ │
│ │ └─────────────────────────────────────────────────────────┘ │ │
│ │                                                             │ │
│ │ Transient Storage: 1024 MB (1 GB) ← 75% REDUCTION!          │ │
│ │ ┌─────────────────────────────────────────────────────────┐ │ │
│ │ │ Level assets                                            │ │ │
│ │ │ Particle systems                                        │ │ │
│ │ │ Temporary data                                          │ │ │
│ │ │                                                         │ │ │
│ │ │ BENEFIT: 1GB still HUGE for transient data!             │ │ │
│ │ │ - Faster allocation (smaller page tables)               │ │ │
│ │ │ - Works on low-RAM systems                              │ │ │
│ │ └─────────────────────────────────────────────────────────┘ │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

##### **Code Organization Flow (Before/After Refactor)**

```
┌─────────────────────────────────────────────────────────────────┐
│ BEFORE (Day 15 - Mixed Responsibilities)                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ game.c (WRONG! Contains platform code)                          │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ init_backbuffer()          ← Platform memory allocation     │ │
│ │   platform_allocate_memory() ← OS-specific call!            │ │
│ │                                                             │ │
│ │ process_game_button_state() ← Input processing             │ │
│ │   (handles OS button events)                                │ │
│ │                                                             │ │
│ │ render_weird_gradient()    ← Game logic (correct layer!)    │ │
│ │ game_update_and_render()   ← Game logic (correct layer!)    │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ platform/x11/backend.c (Duplicate code!)                        │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame()      ← X11-specific version           │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ platform/raylib/backend.c (More duplicate code!)                │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame()      ← Raylib-specific version        │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ PROBLEMS:                                                       │
│ ✗ Game code calls platform_allocate_memory() (wrong layer!)    │
│ ✗ prepare_input_frame() duplicated 2x (100+ lines!)            │
│ ✗ Bug fix requires updating 3 files                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ AFTER (Day 16 - Clean Separation of Concerns)                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ game.c (ONLY game logic now!)                                   │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ render_weird_gradient()    ← Pure game code                 │ │
│ │ game_update_and_render()   ← Pure game code                 │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ platform/_common/backbuffer.c (Shared platform code!)           │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ init_backbuffer()          ← Calls platform_allocate_memory │ │
│ │   (used by BOTH X11 and Raylib!)                            │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ platform/_common/input.c (Shared platform code!)                │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ prepare_input_frame()      ← Single implementation!         │ │
│ │ process_game_button_state() ← Single implementation!        │ │
│ │   (used by BOTH X11 and Raylib!)                            │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ platform/x11/backend.c (Backend-specific only!)                 │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ #include "_common/input.h"   ← Uses shared code             │ │
│ │ #include "_common/backbuffer.h"                             │ │
│ │                                                             │ │
│ │ handle_event()             ← X11-specific event handling    │ │
│ │   calls prepare_input_frame()                               │ │
│ │   calls process_game_button_state()                         │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ platform/raylib/backend.c (Backend-specific only!)              │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ #include "_common/input.h"   ← Uses shared code             │ │
│ │ #include "_common/backbuffer.h"                             │ │
│ │                                                             │ │
│ │ handle_keyboard_inputs()   ← Raylib-specific input          │ │
│ │   calls prepare_input_frame()                               │ │
│ │   calls process_game_button_state()                         │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ BENEFITS:                                                       │
│ ✅ Game code never touches platform memory allocation          │
│ ✅ prepare_input_frame() in ONE place (single source of truth)  │
│ ✅ Bug fix updates 1 file, fixes BOTH backends automatically    │
│ ✅ Easy to add new backend (just include _common/*.h)           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### ✅ Skills Acquired

- ✅ **Input Architecture Design** - Learned difference between polling (Windows repeat events) vs event-driven (X11 press/release only). Adapted Casey's pattern to X11's event model.
- ✅ **Transition Tracking** - Understood `half_transition_count` is a COUNTER, not a boolean. Fixed Day 15 bug where held buttons looked released.
- ✅ **State Preservation** - Learned when to preserve vs clear input state. X11/Linux require preserving `ended_down` AND analog values (no repeat events).
- ✅ **Dead Code Elimination** - Discovered GCC's `--gc-sections` requires `-ffunction-sections -fdata-sections` at compile time. Linker can't GC without per-function sections!
- ✅ **Platform-Specific Debugging** - Improved on Casey's assert with `__debugbreak__()` (MSVC) and `__builtin_trap()` (GCC/Clang). Breaks into debugger instead of segfault.
- ✅ **Code Organization** - Extracted 100+ lines of duplicate code to `platform/_common/`. Learned proper dependency flow: Game → Platform → OS.
- ✅ **Memory Pragmatism** - Reduced transient storage 4GB→1GB. Learned "big numbers feel safe" isn't always practical. 1GB still HUGE for temp data!
- ✅ **Build System Equivalence** - Mapped ALL Casey's MSVC flags to GCC/Clang equivalents. Learned `-opt:ref` needs `-ffunction-sections` on GCC!
- ✅ **Circular Dependency Recognition** - Hit circular include (`backbuffer.h` → `game.h` → platform headers → `backbuffer.h`). Documented as acceptable coupling for Day 16.
- ✅ **Premature Optimization Recognition** - Attempted generic input abstraction (PhysicalKey → GameAction binding). Realized it's premature. Documented decision with TODO for future self.
- ✅ **Professional Code Cleanup** - Removed unused `old_state` parameter from `process_game_button_state()`. Updated 50+ call sites. Deleted commented dead code (trust git history!).
- ✅ **Documentation Best Practices** - Wrote comprehensive TODO comment explaining: current status, limitations, why NOT fixing, when to revisit, future references. Production-quality self-documentation!
- ✅ **Pragmatic Engineering Decision-Making** - Learned to recognize when simple is better than perfect. "Solve problems you HAVE, not problems you MIGHT have." (Casey's philosophy internalized!)

---

#### 🎓 Day 16 Retrospective: What I Learned About Engineering

This day taught me **MORE than just input systems**. I learned:

1. **When to Stop Engineering** - Attempted generic input abstraction, hit complexity wall, recognized it was premature. Documented decision instead of over-engineering.

2. **The Value of "Future You"** - Wrote detailed TODO explaining: current approach, known limitations, trigger conditions for refactor. This is how professionals work!

3. **Simple vs Perfect** - Casey's code is intentionally simple at Day 16. He'll refactor when NEEDS arise (Episode 150+). I learned to trust this process.

4. **Code Organization Pays Off** - Extracting `_common/input.c` and `_common/backbuffer.c` eliminated 100+ lines of duplication. Single source of truth for both X11 and Raylib!

5. **Platform Differences Matter** - X11 doesn't repeat KeyPress events like Windows WM_KEYDOWN. My implementation MUST preserve button state, not zero like Casey. This is CORRECT adaptation, not wrong!

6. **Build Systems Are Important** - Matching Casey's MSVC flags taught me about dead code elimination, map files, and linker optimizations. These matter for production code!

7. **Debugging Is Part of Design** - Platform-specific `__debugbreak__()` and `__builtin_trap()` improve debugging workflow. This is BETTER than Casey's `*(int*)0` crash!

**Ready for Day 17!** 🚀 My foundation is solid, code is clean, and I deeply understand WHY every line exists.

### 📆 Day 17: Unified Keyboard and Gamepad Input

**Focus:** Unifying digital (keyboard) and analog (gamepad) inputs into a single cohesive API, eliminating platform-specific branching in game code through semantic button naming and hybrid analog/digital representation.

---

#### 🗓️ Commits

| Date       | Commit    | What Changed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | What I Changed & Why                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| ---------- | --------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-01-08 | `d6e9cb4` | **Day 17: Unified Keyboard and Gamepad Input**<br>- Expanded `GameButtonState` array from 6 to 12 buttons<br>- Renamed fields: `up/down/left/right` → `move_*`, added `action_*`, `back`, `start`<br>- Simplified analog state: `start/min/max/end_x/y` → `stick_avg_x/y`<br>- Added `GetController()` bounds checking helper<br>- Added `terminator` sentinel for compile-time validation<br>- Remapped keyboard: WASD=movement, Arrows=actions<br>- Implemented analog→digital conversion with 0.5 threshold | **Abstraction Layer:** Created separate `keyboard.c/joystick.c` files for X11 and Raylib (Casey's Win32 has all input in one file). This mirrors how I separated audio into `audio.c`. Cleaner for cross-platform!<br><br>**D-pad Priority System:** X11's `/dev/input/js*` reports D-pad as axis 6-7, causing conflicts with left stick (axis 0-1). Implemented temp variable merging: stick wins if deflected > deadzone, else D-pad. Casey doesn't need this (XInput separates them).<br><br>**Raylib Surprise:** Discovered `GetGamepadAxisMovement()` does NOT merge D-pad values (contrary to docs). Added explicit D-pad button handling + analog→digital conversion loop. |

---

#### 📊 Day 17 Input Architecture: Before vs After

##### **BEFORE (Day 16): Monolithic, Branching Nightmare**

```
┌──────────────────────────────────────────────────────────────┐
│ GAME LAYER (game.c)                                          │
│ ──────────────────────────────────────────────────────────── │
│                                                              │
│ if (controller->is_analog) {                                 │
│   // Joystick code path                                      │
│   player.x += controller->end_x * speed;  ← Smooth           │
│ } else {                                                     │
│   // Keyboard code path                                      │
│   if (controller->up.ended_down) {        ← Digital          │
│     player.y += 5;                                           │
│   }                                                          │
│ }                                                            │
│                                                              │
│ ❌ Problem: Game code KNOWS about input devices!             │
│ ❌ Problem: Two separate code paths to maintain!             │
└──────────────────────────────────────────────────────────────┘
         ▲                              ▲
         │                              │
┌────────┴──────────┐        ┌──────────┴────────────┐
│ KEYBOARD          │        │ JOYSTICK              │
│ ───────────────── │        │ ───────────────────── │
│ Sets:             │        │ Sets:                 │
│ - up.ended_down   │        │ - end_x, end_y        │
│ - down.ended_down │        │ - is_analog = true    │
│ - left.ended_down │        │                       │
│ - right.ended_down│        │                       │
│ - is_analog=false │        │                       │
└───────────────────┘        └───────────────────────┘
```

##### **AFTER (Day 17): Unified, Semantic API**

```
┌──────────────────────────────────────────────────────────────┐
│ GAME LAYER (game.c)                                          │
│ ──────────────────────────────────────────────────────────── │
│                                                              │
│ // ✅ ONE code path for ALL input devices!                   │
│ if (controller->move_up.ended_down) {                        │
│   player.y += 5;  // Works for keyboard AND joystick!       │
│ }                                                            │
│                                                              │
│ // Optional: Use analog for smooth movement                 │
│ if (controller->is_analog) {                                 │
│   player.x += controller->stick_avg_x * speed;               │
│ }                                                            │
│                                                              │
│ ✅ Game code is INPUT-AGNOSTIC!                              │
│ ✅ Semantic names: move_* = locomotion, action_* = interact  │
└──────────────────────────────────────────────────────────────┘
         ▲                              ▲
         │                              │
┌────────┴──────────┐        ┌──────────┴────────────┐
│ KEYBOARD          │        │ JOYSTICK              │
│ ───────────────── │        │ ───────────────────── │
│ WASD sets:        │        │ Stick sets:           │
│ - move_up         │        │ - stick_avg_x/y       │
│ - move_down       │        │ - is_analog = true    │
│ - move_left       │        │                       │
│ - move_right      │        │ THEN converts to:     │
│                   │        │ - move_up (if > 0.5)  │
│ Arrows set:       │        │ - move_down           │
│ - action_up       │        │ - move_left           │
│ - action_down     │        │ - move_right          │
│ - action_left     │        │                       │
│ - action_right    │        │ D-pad sets:           │
│                   │        │ - move_* directly     │
└───────────────────┘        └───────────────────────┘
```

---

#### 🎯 Core Concepts

| Concept                      | Casey's Win32 Implementation                              | My Linux Implementation                                             | What I Learned & Adapted                                                                                                                                                                            |
| ---------------------------- | --------------------------------------------------------- | ------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Button Count**             | 12 buttons (move×4, action×4, shoulder×2, menu×2)         | **Same!** 12 buttons with identical layout                          | Learned that 12 is the "standard gamepad baseline" (Xbox 360 era). Any modern controller can map to this.                                                                                           |
| **Semantic Naming**          | `MoveUp/Down/Left/Right`, `ActionUp/Down/Left/Right`      | `move_up/down/left/right`, `action_up/down/left/right` (snake_case) | **Why semantic names matter:** `move_up` clearly means "character locomotion", `action_up` means "menu select / Y button". Game designers understand this!                                          |
| **Simplified Analog**        | `StickAverageX/Y` (2 fields, removed `Start/Min/Max`)     | `stick_avg_x/y` (snake_case, 2 fields)                              | **Big aha moment:** Day 16's 8 analog fields (`start/min/max/end` × 2 axes) were for future "gestural input" that never happened. Day 17 = clean up tech debt! Only `avg` matters for actual games. |
| **GetController()**          | Inline bounds check + assert                              | **Extracted to helper function** (more reusable)                    | Learned Casey's "assert early, fail loudly" philosophy. My version returns pointer, his is inline. Both work!                                                                                       |
| **Terminator Sentinel**      | `GameButtonState Terminator;` after last button           | **Same!** `GameButtonState terminator;`                             | Genius compile-time validation! If you add a button AFTER terminator, assert fires. Catches bugs at startup, not runtime.                                                                           |
| **Analog→Digital Threshold** | 0.5f (50% stick deflection)                               | **Same!** 0.5f, but SEPARATE from deadzone (0.05)                   | **Key insight:** Deadzone (noise filter) ≠ Threshold (button trigger). Deadzone removes drift, threshold makes discrete input. Two different jobs!                                                  |
| **Keyboard Remapping**       | WASD=move, Arrows=action, Escape=start, Space=back        | **Same mapping!** But used X11 `KeySym` instead of VK codes         | Learned X11's `XLookupKeysym()` is equivalent to Win32's `VK_*` constants. Different API, same concept.                                                                                             |
| **D-pad Handling**           | XInput reports D-pad as **buttons** (separate from stick) | **Linux reports as axes 6-7!** Had to merge with stick values       | **Biggest adaptation:** Created temp variable system (`stick_x`, `dpad_x`) that merges AFTER event loop. Stick priority > D-pad. Casey doesn't need this!                                           |

---

#### 💻 Code Snippets with Explanations

##### **1. The 12-Button Layout (Day 17's Core Structure)**

```c
typedef struct {
  union {
    GameButtonState buttons[12];  // ← EXACTLY 12, not 6!
    struct {
      // ═══════════════════════════════════════════════════════
      // MOVEMENT GROUP (locomotion)
      // ═══════════════════════════════════════════════════════
      // Keyboard: WASD
      // Gamepad:  Left stick (via analog→digital) + D-pad
      // ═══════════════════════════════════════════════════════
      GameButtonState move_up;     // ← Was "up" in Day 16
      GameButtonState move_down;   // ← Was "down"
      GameButtonState move_left;   // ← Was "left"
      GameButtonState move_right;  // ← Was "right"

      // ═══════════════════════════════════════════════════════
      // ACTION GROUP (face buttons / interactions)
      // ═══════════════════════════════════════════════════════
      // Keyboard: Arrow keys
      // Gamepad:  A/B/X/Y buttons
      // ═══════════════════════════════════════════════════════
      GameButtonState action_up;    // ✅ NEW! Y button / Arrow Up
      GameButtonState action_down;  // ✅ NEW! A button / Arrow Down
      GameButtonState action_left;  // ✅ NEW! X button / Arrow Left
      GameButtonState action_right; // ✅ NEW! B button / Arrow Right

      // ═══════════════════════════════════════════════════════
      // SHOULDER GROUP (unchanged from Day 16)
      // ═══════════════════════════════════════════════════════
      GameButtonState left_shoulder;  // Q key / L1 button
      GameButtonState right_shoulder; // E key / R1 button

      // ═══════════════════════════════════════════════════════
      // MENU GROUP
      // ═══════════════════════════════════════════════════════
      GameButtonState back;   // ✅ NEW! Space / Select button
      GameButtonState start;  // ✅ NEW! Escape / Start button

      // ═══════════════════════════════════════════════════════
      // SENTINEL (compile-time validation)
      // ═══════════════════════════════════════════════════════
      // NOTE: All buttons must be added ABOVE this line!
      GameButtonState terminator;  // ← NOT in array! Just a marker!
    };
  };

  // ═══════════════════════════════════════════════════════════
  // ANALOG STATE (Day 17 simplified!)
  // ═══════════════════════════════════════════════════════════
  real32 stick_avg_x;  // ← Was "end_x" (clearer name!)
  real32 stick_avg_y;  // ← Was "end_y"

  bool32 is_analog;    // true = gamepad, false = keyboard
  bool is_connected;
  int controller_index;
} GameControllerInput;
```

**Why This Structure?**

1. **Union Trick:** Access buttons as `controller->buttons[i]` (loop) OR `controller->move_up` (named). Same memory!
2. **12-Button Standard:** Xbox 360 layout (industry standard). PS/Nintendo controllers map to this.
3. **Semantic Groups:** Game designers think in "movement" vs "actions" vs "menus", not "button 0-11".
4. **Terminator Sentinel:** If you add `GameButtonState new_button;` AFTER `terminator`, assert fires! Catches bugs immediately.

**My Linux Adaptation:**

- Used `snake_case` (`move_up` vs Casey's `MoveUp`) to match my codebase style
- Added `is_connected` check (Casey assumes controllers never unplug mid-game, I handle hot-plugging)

---

##### **2. GetController() - Bounds Checking Helper (My Addition)**

```c
// ✅ MY ADDITION: Extracted Casey's inline check to reusable function
inline GameControllerInput *GetController(GameInput *input,
                                          unsigned int controller_index) {
  Assert(controller_index < ArrayCount(input->controllers));
  // ^^^ Fires if you typo: GetController(input, 999)
  //     Stack trace shows EXACTLY where the bug is!

  GameControllerInput *result = &input->controllers[controller_index];
  return result;
}

// USAGE (Day 17 pattern):
GameControllerInput *keyboard = GetController(input, KEYBOARD_CONTROLLER_INDEX);
// ^^^ Safe! If KEYBOARD_CONTROLLER_INDEX > 4, assert fires at startup
```

**Casey's Inline Version (win32_handmade.cpp):**

```cpp
// Casey does this check INLINE in game code:
game_controller_input *Controller0 = &Input->Controllers[0];
// ^^^ No bounds check! Assumes index is always valid
```

**Why My Version Is Better (IMO):**

- ✅ **Single point of failure:** All controller access goes through one function
- ✅ **Better error messages:** Assert shows `GetController()` in stack trace
- ✅ **Easier to add logging:** Can print "Accessing controller 2" for debugging

**When Casey's Version Is Better:**

- ✅ **Zero overhead:** No function call (optimizer should inline mine anyway)
- ✅ **Simpler code:** Less abstraction = easier to understand

---

##### **3. Analog → Digital Conversion (The Heart of Day 17)**

```c
void linux_poll_joystick(GameInput *new_input) {
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *ctrl = &new_input->controllers[i];

    // ═══════════════════════════════════════════════════════
    // STEP 1: Read raw joystick events (Linux /dev/input/js*)
    // ═══════════════════════════════════════════════════════
    real32 stick_x = 0.0f;  // ← Temp variables (my addition!)
    real32 stick_y = 0.0f;
    real32 dpad_x = 0.0f;
    real32 dpad_y = 0.0f;

    struct js_event event;
    while (read(joystick_fd, &event, sizeof(event)) == sizeof(event)) {
      if (event.type == JS_EVENT_AXIS) {
        switch (event.number) {
        case 0: // Left stick X
          stick_x = (real32)event.value / 32767.0f;  // ← Store, don't set yet!
          break;

        case 1: // Left stick Y
          stick_y = (real32)event.value / 32767.0f;
          break;

        case 6: // D-pad X (Linux reports as axis, not button!)
          dpad_x = (event.value < -16384) ? -1.0f :
                   (event.value >  16384) ?  1.0f : 0.0f;
          break;

        case 7: // D-pad Y
          dpad_y = (event.value < -16384) ? -1.0f :
                   (event.value >  16384) ?  1.0f : 0.0f;
          break;
        }
      }
    }

    // ═══════════════════════════════════════════════════════
    // STEP 2: Merge stick + D-pad (MY ADDITION - Casey doesn't need this!)
    // ═══════════════════════════════════════════════════════
    // Priority: Stick wins if deflected > deadzone, else D-pad
    // ═══════════════════════════════════════════════════════

    if (fabsf(stick_x) > BASE_JOYSTICK_DEADZONE) {
      ctrl->stick_avg_x = stick_x;  // Stick active → use stick
    } else {
      ctrl->stick_avg_x = dpad_x;   // Stick centered → use D-pad
    }

    if (fabsf(stick_y) > BASE_JOYSTICK_DEADZONE) {
      ctrl->stick_avg_y = stick_y;
    } else {
      ctrl->stick_avg_y = dpad_y;
    }

    // ═══════════════════════════════════════════════════════
    // STEP 3: Convert analog → digital (CASEY'S DAY 17 PATTERN!)
    // ═══════════════════════════════════════════════════════

    if (ctrl->is_connected && ctrl->is_analog) {
      real32 threshold = 0.5f;  // Casey's 50% deflection

      // Horizontal
      process_game_button_state(
          (ctrl->stick_avg_x < -threshold),
          &ctrl->move_left
      );

      process_game_button_state(
          (ctrl->stick_avg_x > threshold),
          &ctrl->move_right
      );

      // Vertical
      process_game_button_state(
          (ctrl->stick_avg_y < -threshold),
          &ctrl->move_down
      );

      process_game_button_state(
          (ctrl->stick_avg_y > threshold),
          &ctrl->move_up
      );
    }
  }
}
```

**What I Learned:**

1. **Temp Variables Pattern (My Addition):**

   - Linux's `/dev/input/js*` sends events in RANDOM order (stick X, then D-pad X, then stick Y)
   - If I set `stick_avg_x` directly, D-pad event OVERWRITES it!
   - Solution: Store in temps, merge AFTER event loop
   - **Casey doesn't need this:** XInput separates D-pad (buttons) from stick (axes)

2. **Threshold ≠ Deadzone (Aha Moment!):**

   - `BASE_JOYSTICK_DEADZONE = 0.05` → Noise filter (prevents drift)
   - `threshold = 0.5` → Button trigger (determines when button "clicks")
   - **Why different?** Deadzone removes jitter at center, threshold makes discrete input at edge

3. **Analog → Digital Conversion (Casey's Philosophy):**
   - Game code wants BOTH representations simultaneously!
   - `stick_avg_x = 0.7` → Smooth movement (analog)
   - `move_right.ended_down = true` → State check (digital, because 0.7 > 0.5)
   - No interference! They're complementary!

---

##### **4. Terminator Validation (Compile-Time Safety)**

```c
void game_update_and_render(GameMemory *memory, GameInput *input, ...) {
  // ═══════════════════════════════════════════════════════
  // 🔥 CASEY'S DAY 17 COMPILE-TIME VALIDATION TRICK!
  // ═══════════════════════════════════════════════════════

  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    Assert((&input->controllers[i].terminator -
            &input->controllers[i].buttons[0]) ==
           (ArrayCount(input->controllers[i].buttons)));
    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // Pointer subtraction gives array size!
    // If you add a button AFTER terminator, this fires!
  }

  // ... (rest of game code)
}
```

**How It Works (Pointer Arithmetic Magic):**

```c
// ══════════════════════════════════════════════════════════
// MEMORY LAYOUT (assuming 4-byte ints for clarity)
// ══════════════════════════════════════════════════════════

union {
  GameButtonState buttons[12];  // 12 × sizeof(GameButtonState)
  struct {
    GameButtonState move_up;       // buttons[0]  → Addr 0x1000
    GameButtonState move_down;     // buttons[1]  → Addr 0x1004
    // ... (10 more buttons)
    GameButtonState start;         // buttons[11] → Addr 0x102C
    GameButtonState terminator;    // NOT in array! → Addr 0x1030
  };
};

// POINTER SUBTRACTION:
&terminator - &buttons[0]
= 0x1030 - 0x1000
= 0x30 bytes
= 0x30 / sizeof(GameButtonState)
= 0x30 / 4
= 12 elements  // ← Expected!

// IF YOU ADD A BUTTON AFTER TERMINATOR:
struct {
  // ... (12 buttons)
  GameButtonState terminator;
  GameButtonState OOPS_button;  // ← BUG!
};

// POINTER SUBTRACTION NOW:
&terminator - &buttons[0]
= 0x1030 - 0x1000
= 12 elements  // ← Still 12! (terminator is at 12)

ArrayCount(buttons) = 12

Assert(12 == 12)  // ← PASSES! (Wrong!)

// WAIT, WHY DOES IT PASS?!
// Because OOPS_button is AFTER terminator!
// The terminator is STILL at position 12!

// ══════════════════════════════════════════════════════════
// CORRECT BUG SCENARIO (terminator in wrong place):
// ══════════════════════════════════════════════════════════

struct {
  // ... (11 buttons)
  GameButtonState terminator;  // ← Too early!
  GameButtonState start;       // ← Should be BEFORE terminator!
};

// POINTER SUBTRACTION:
&terminator - &buttons[0]
= 11 elements  // ← Wrong!

ArrayCount(buttons) = 12

Assert(11 == 12)  // ← FAILS! ✅ Catches bug!
```

**What I Learned:**

- Sentinel pattern validates **ordering**, not **count**
- If you add buttons in wrong ORDER, assert fires
- If you add buttons AFTER terminator, assert passes (false negative!)
- **Solution:** Code review + convention ("All buttons above this line" comment)

---

##### **5. Keyboard Remapping (Platform-Specific)**

```c
void handleEventKeyPress(XEvent *event, GameInput *new_game_input, ...) {
  KeySym key = XLookupKeysym(&event->xkey, 0);
  // ^^^ X11 equivalent of Win32's VK_* constants

  GameControllerInput *kbd =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  switch (key) {
  // ═══════════════════════════════════════════════════════
  // MOVEMENT KEYS (WASD) → move_* buttons
  // ═══════════════════════════════════════════════════════
  case XK_w:
  case XK_W: {
    process_game_button_state(true, &kbd->move_up);
    break;
  }
  case XK_a:
  case XK_A: {
    process_game_button_state(true, &kbd->move_left);
    break;
  }
  // ... (S/D keys)

  // ═══════════════════════════════════════════════════════
  // ACTION KEYS (Arrows) → action_* buttons
  // ═══════════════════════════════════════════════════════
  case XK_Up: {
    process_game_button_state(true, &kbd->action_up);
    break;
  }
  case XK_Down: {
    process_game_button_state(true, &kbd->action_down);
    break;
  }
  // ... (Left/Right arrows)

  // ═══════════════════════════════════════════════════════
  // MENU KEYS
  // ═══════════════════════════════════════════════════════
  case XK_space: {
    process_game_button_state(true, &kbd->back);
    break;
  }
  case XK_Escape: {
    process_game_button_state(true, &kbd->start);
    // ❌ DON'T do this: is_game_running = false;
    // ✅ Let GAME layer handle Escape (pause menu, etc.)
    break;
  }
  }
}
```

**Casey's Win32 Equivalent:**

```cpp
// win32_handmade.cpp (Day 17)

case WM_KEYDOWN: {
  uint32 VKCode = WParam;

  switch (VKCode) {
  case 'W': {
    Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
    break;
  }
  case VK_UP: {
    Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
    break;
  }
  case VK_ESCAPE: {
    Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
    // ← Casey also doesn't quit on Escape!
    break;
  }
  }
}
```

**What I Learned:**

1. **X11 vs Win32 Key Handling:**

   - X11: `XLookupKeysym()` → `KeySym` enum
   - Win32: `WM_KEYDOWN` → `VK_*` constants
   - **Same concept, different names!**

2. **Platform Code Philosophy (Day 17):**

   - ❌ **Old way (Day 16):** Platform quits on Escape
   - ✅ **New way (Day 17):** Platform reports Escape as `start` button, game decides what to do
   - **Why?** Escape might mean pause, quit, close menu, etc. Game knows context, platform doesn't!

3. **My Raylib Keyboard Adaptation:**
   - Raylib uses `IsKeyDown()` polling instead of event callbacks
   - Had to add explicit `IsKeyReleased()` checks (Raylib doesn't auto-generate release events)
   - Same logic, different API!

---

#### 🔄 ASCII Art: Analog → Digital Conversion Flow

```
┌──────────────────────────────────────────────────────────────────┐
│ FRAME N: Joystick Stick State                                    │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│ User pushes left stick 70% to the right                         │
│                                                                  │
│ ┌────────────────────────────────────────┐                      │
│ │ PLATFORM LAYER (x11/inputs/joystick.c) │                      │
│ └────────────────────────────────────────┘                      │
│                                                                  │
│ STEP 1: Read raw event                                          │
│ ────────────────────────────────────────                        │
│   event.type = JS_EVENT_AXIS                                    │
│   event.number = 0  (Left stick X)                              │
│   event.value = +22937  (70% of +32767)                         │
│                                                                  │
│   Code: stick_x = (real32)event.value / 32767.0f;               │
│   Result: stick_x = 0.7                                         │
│                                                                  │
│ STEP 2: Merge stick + D-pad (if D-pad also active)              │
│ ────────────────────────────────────────────────────────────    │
│   if (fabsf(stick_x) > BASE_JOYSTICK_DEADZONE) {  // 0.7 > 0.05 │
│     ctrl->stick_avg_x = stick_x;  // Use stick value ✅          │
│   } else {                                                       │
│     ctrl->stick_avg_x = dpad_x;   // Use D-pad value            │
│   }                                                              │
│                                                                  │
│   Result: ctrl->stick_avg_x = 0.7                               │
│                                                                  │
│ STEP 3: Analog → Digital conversion                             │
│ ────────────────────────────────────────────────────────────    │
│   real32 threshold = 0.5f;  // 50% deflection                   │
│                                                                  │
│   // Right direction check                                      │
│   process_game_button_state(                                    │
│       (ctrl->stick_avg_x > threshold),  // 0.7 > 0.5? YES!      │
│       &ctrl->move_right                                         │
│   );                                                             │
│                                                                  │
│   Result: ctrl->move_right.ended_down = true  ✅                 │
│                                                                  │
│ ┌────────────────────────────────────────┐                      │
│ │ GAME LAYER (game.c)                    │                      │
│ └────────────────────────────────────────┘                      │
│                                                                  │
│ // Option A: Use digital button (works for keyboard too!)       │
│ if (ctrl->move_right.ended_down) {                              │
│   player.x += 5;  // Discrete movement                          │
│ }                                                                │
│                                                                  │
│ // Option B: Use analog value (smooth movement)                 │
│ if (ctrl->is_analog) {                                           │
│   player.x += ctrl->stick_avg_x * player_speed;                 │
│   // = 0.7 × 10 = +7 pixels (proportional to stick deflection)  │
│ }                                                                │
│                                                                  │
│ ═════════════════════════════════════════════════════════════   │
│ RESULT: BOTH representations available!                         │
│ ═════════════════════════════════════════════════════════════   │
│   ctrl->stick_avg_x = 0.7               ← Analog (smooth)       │
│   ctrl->move_right.ended_down = true   ← Digital (binary)       │
│                                                                  │
│ Game can use EITHER depending on what it needs!                 │
└──────────────────────────────────────────────────────────────────┘
```

---

#### 🐛 Common Pitfalls

| Issue                                     | Cause                                                         | Fix                                                                        | My Encountered Issues & Solutions                                                                                         |
| ----------------------------------------- | ------------------------------------------------------------- | -------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **Compile error: `up` has no member**     | Forgot to rename `up/down/left/right` → `move_*` in game code | Search-replace all occurrences in `game.c` and `handle_controls()`         | ✅ Hit this! Grep'd for `controller->up` and found 15 instances. Changed to `controller->move_up`.                        |
| **Assert fires: button count mismatch**   | Added a button AFTER `terminator` in struct                   | Move all buttons ABOVE `terminator` line                                   | ✅ Hit this during testing! Added `back` button after `terminator`, assert fired. Reordered struct.                       |
| **D-pad doesn't work on Linux**           | Forgot to convert D-pad axes (6-7) to button states           | Add analog→digital conversion AFTER event loop                             | ✅ Hit this! Initially only converted stick (0-1), D-pad axes (6-7) were ignored. Added second conversion pass.           |
| **Stick and D-pad conflict**              | Both trying to set `stick_avg_x` in same frame                | Use temp variables, merge AFTER event loop                                 | ✅ Hit this! D-pad event overwrote stick value. Added `stick_x`/`dpad_x` temps, merged with priority logic.               |
| **Buttons never release**                 | Forgot to update `KeyRelease` handler                         | Update BOTH `KeyPress` AND `KeyRelease` with new names                     | ✅ Hit this! Changed KeyPress to use `move_up`, but KeyRelease still used `up`. Buttons stuck "on".                       |
| **Escape key still quits**                | Hardcoded `is_game_running = false` in platform               | Remove platform quit logic, let game handle `start` button                 | ⚠️ Kept this for now (easier testing). Will remove in Day 18 when adding pause menu.                                      |
| **Joystick deadzone too large**           | Used conversion threshold (0.5) for deadzone                  | Use separate constants: `BASE_JOYSTICK_DEADZONE = 0.05`, `threshold = 0.5` | ✅ Hit this! Stick had to deflect 50% before ANY movement registered. Split into two thresholds.                          |
| **Raylib D-pad doesn't merge with stick** | Assumed `GetGamepadAxisMovement()` includes D-pad             | Process D-pad as buttons separately, THEN convert analog                   | ✅ Hit this! Raylib docs say D-pad merges into axes, but testing showed it doesn't. Added explicit D-pad button handling. |

---

#### ✅ Skills Acquired

- ✅ **Semantic API Design:** Learned to name inputs by PURPOSE (`move_*`, `action_*`) not by DEVICE (`button_0`, `axis_1`)
- ✅ **Hybrid Analog/Digital Representation:** Understood why storing BOTH analog values AND digital states is powerful (no "either-or" branching!)
- ✅ **Platform Abstraction Philosophy:** Grasped Casey's rule: "Platform reports state, game makes decisions" (Escape is a button, not a quit command)
- ✅ **Compile-Time Validation:** Mastered sentinel pattern for catching struct layout bugs at startup (not runtime!)
- ✅ **Linux Joystick Quirks:** Learned that `/dev/input/js*` reports D-pad as axes (not buttons like XInput), requiring merge logic
- ✅ **Deadzone vs Threshold Distinction:** Understood that noise filtering (deadzone) and button triggering (threshold) are separate concerns
- ✅ **X11 vs Raylib Input Differences:** Discovered that Raylib's `GetGamepadAxisMovement()` does NOT merge D-pad (contrary to Casey's XInput behavior)
- ✅ **Code Organization Patterns:** Improved my abstraction by separating `keyboard.c` and `joystick.c` (cleaner than Casey's monolithic file)
- ✅ **Pointer Arithmetic Tricks:** Learned how `&terminator - &buttons[0]` validates array layout at compile-time
- ✅ **Multi-Platform Input Handling:** Adapted Casey's Win32 XInput code to work with Linux `/dev/input/js*` AND Raylib's cross-platform API

---

#### 🎓 Casey's Core Teachings (Day 17)

| Teaching                                    | Quote (Paraphrased from Day 17)                                                                           | How I Applied It                                                                                         |
| ------------------------------------------- | --------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| **Make Game Code Device-Agnostic**          | "The game shouldn't know if input came from keyboard or controller. It just checks `MoveUp.EndedDown`."   | ✅ Unified all input to same button layout. Game code has ZERO `if (is_keyboard)` checks now!            |
| **Semantic Naming Matters**                 | "Call buttons what they DO, not what they ARE. `MoveUp` is clearer than `Button0`."                       | ✅ Renamed all buttons to `move_*` / `action_*` / `shoulder` / `menu` groups.                            |
| **Analog + Digital, Not Analog OR Digital** | "You want BOTH representations available simultaneously. Let game code pick what it needs."               | ✅ Store `stick_avg_x/y` (analog) AND convert to `move_*` buttons (digital). Game uses whichever fits!   |
| **Platform Reports, Game Decides**          | "Don't make platform decisions. Escape is a button. The GAME decides if that means pause, quit, or menu." | ⚠️ Partially applied. Still have `is_game_running = false` on Escape for testing. Will remove in Day 18. |
| **Assert Early, Fail Loudly**               | "Add compile-time validation with sentinels. Catch bugs at startup, not 3 hours into gameplay."           | ✅ Added `terminator` validation. Assert fired twice during development, caught bugs immediately!        |
| **Clean Up Tech Debt**                      | "Day 16's 8 analog fields were for future features that never happened. Remove them!"                     | ✅ Simplified `start/min/max/end_x/y` → `stick_avg_x/y`. 75% less state to manage!                       |

---

#### 📚 References & Further Reading

- **Handmade Hero Day 17:** [Unified Keyboard and Gamepad Input](https://guide.handmadehero.org/code/day017/)
- **Linux Joystick API:** joystick.h (shows `JS_EVENT_AXIS` for D-pad on axes 6-7)
- **Raylib Gamepad Docs:** [GetGamepadAxisMovement()](https://www.raylib.com/cheatsheet/cheatsheet.html) (notes on D-pad behavior)
- **XInput vs DirectInput:** [MSDN comparison](https://docs.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput) (explains why XInput separates D-pad/stick)

---

**Next:** Day 18 - Enforcing a Video Frame Rate (decoupling rendering from input polling)

### 📆 Day 18: Enforcing a Video Frame Rate

**Focus:** Implementing adaptive frame rate control with VSync, two-phase sleep strategy, and performance monitoring to maintain consistent timing across different hardware.

---

#### 🗓️ Commits

| Date         | Commit    | What Changed                                                                                                                                                              | What I Changed & Why                                                                                                                                                                                                                                     |
| ------------ | --------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Jan 10, 2026 | `e45da57` | **Day 18: Enforcing a Video Frame Rate** - Refactored X11 backend to use OpenGL rendering, added adaptive FPS system, implemented two-phase sleep, added frame statistics | **Major refactor:** Replaced XPutImage with OpenGL to solve RGBA color format issues and enable VSync. Added adaptive FPS that auto-reduces from 60→30→20→15 if frames are missed. Implemented Casey's two-phase sleep (coarse+spin) for precise timing. |
| Jan 10, 2026 | `494cabd` | **Cleanup: Remove POSIX dependencies from Raylib backend** - Removed `_POSIX_C_SOURCE`, `<time.h>`, manual frame timing, duplicate functions                              | **Cross-platform fix:** Replaced POSIX `clock_gettime()` with Raylib's `GetTime()`/`GetFrameTime()`/`GetFPS()` to make code truly cross-platform (works on Windows/Linux/macOS/Web). Removed duplicate `resize_back_buffer()` functions.                 |

---

#### 🎯 Core Concepts

| Concept                  | Casey's Windows Implementation                                                        | My Linux/X11 Implementation                                                   | What I Learned & Why I Adapted                                                                                                                                                                                                                               |
| ------------------------ | ------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **VSync**                | `wglSwapIntervalEXT(1)` (OpenGL extension)                                            | `glXSwapBuffers()` (built-in VSync with GLX double buffering)                 | **Learned:** VSync prevents screen tearing by syncing buffer swaps to monitor refresh. **Adapted:** X11's GLX provides VSync automatically with double buffering - no need for extensions like Windows. Just enable `GLX_DOUBLEBUFFER` in visual attributes. |
| **Frame Timing**         | `QueryPerformanceCounter()` (high-resolution timer)                                   | `clock_gettime(CLOCK_MONOTONIC)` (nanosecond precision, never goes backwards) | **Learned:** Need monotonic clock that never jumps backwards (unlike `CLOCK_REALTIME` which adjusts for NTP). **Why:** `CLOCK_MONOTONIC` is perfect for frame timing - immune to system clock changes.                                                       |
| **Two-Phase Sleep**      | 1. Sleep in 1ms chunks until 3ms before target<br>2. Spin-wait for final microseconds | Same pattern with `nanosleep()` + spin loop                                   | **Learned:** OS schedulers are unreliable for sub-5ms precision. Hybrid approach: coarse sleep (saves CPU) + spin-wait (precision). **Why 3ms margin:** Accounts for OS scheduler jitter on typical systems.                                                 |
| **Adaptive FPS**         | Not in original Handmade Hero (added later)                                           | State machine: 60→30→20→15 FPS based on miss rate                             | **Innovation:** Auto-adjusts to hardware capability. Samples 300 frames, if >5% miss → reduce FPS. If <1% miss → try higher FPS. **Why:** Makes game playable on potato PCs without manual settings.                                                         |
| **OpenGL Rendering**     | `StretchDIBits()` (GDI blitting)                                                      | `glTexImage2D()` + fullscreen quad                                            | **Adapted:** Replaced `XPutImage()` with OpenGL to solve RGBA/BGRA color mismatch. **Bonus:** GPU-accelerated texture upload, built-in VSync, same as Raylib internally.                                                                                     |
| **Monitor Refresh Rate** | `GetDeviceCaps()`                                                                     | `XRRConfigCurrentRate()` (XRandR extension)                                   | **Learned:** Use monitor's native refresh rate as initial FPS target. **Why:** Prevents fighting VSync (requesting 60fps on 144Hz monitor wastes GPU).                                                                                                       |

---

#### 📊 Frame Rate Control Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   FRAME TIMING PIPELINE (Day 18)                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Frame N Start                                                          │
│      │                                                                  │
│      ▼                                                                  │
│  ┌────────────────────────────────────────┐                            │
│  │ 1️⃣ MARK START TIME                     │                            │
│  │   clock_gettime(&frame_start)          │                            │
│  │   start_cycles = __rdtsc()             │  ← CPU cycle counter       │
│  └────────────────────────────────────────┘                            │
│      │                                                                  │
│      ▼                                                                  │
│  ┌────────────────────────────────────────┐                            │
│  │ 2️⃣ DO WORK (Game Update + Render)      │                            │
│  │   - Process input events               │                            │
│  │   - Update game state                  │                            │
│  │   - Render pixels to backbuffer        │                            │
│  │   - Upload to GPU (glTexImage2D)       │                            │
│  │   - Swap buffers (VSync happens here!) │  ← Waits for monitor       │
│  └────────────────────────────────────────┘                            │
│      │                                                                  │
│      ▼                                                                  │
│  ┌────────────────────────────────────────┐                            │
│  │ 3️⃣ MEASURE WORK TIME                   │                            │
│  │   work_time = now - frame_start        │                            │
│  │   remaining = target - work_time       │                            │
│  └────────────────────────────────────────┘                            │
│      │                                                                  │
│      ▼                                                                  │
│  ┌────────────────────────────────────────┐                            │
│  │ 4️⃣ TWO-PHASE SLEEP (Casey's Pattern)   │                            │
│  │                                        │                            │
│  │   Phase 1: COARSE SLEEP (OS scheduler) │                            │
│  │   ┌──────────────────────────────────┐ │                            │
│  │   │ while (remaining > 3ms) {        │ │  ← Leave 3ms safety margin │
│  │   │   nanosleep(1ms);                │ │                            │
│  │   │   remaining = target - elapsed;  │ │                            │
│  │   │ }                                │ │                            │
│  │   └──────────────────────────────────┘ │                            │
│  │                                        │                            │
│  │   Phase 2: SPIN-WAIT (busy loop)       │                            │
│  │   ┌──────────────────────────────────┐ │                            │
│  │   │ while (remaining > 0) {          │ │  ← Tight loop for          │
│  │   │   remaining = target - elapsed;  │ │     microsecond precision  │
│  │   │ }                                │ │                            │
│  │   └──────────────────────────────────┘ │                            │
│  └────────────────────────────────────────┘                            │
│      │                                                                  │
│      ▼                                                                  │
│  ┌────────────────────────────────────────┐                            │
│  │ 5️⃣ FILL AUDIO BUFFER (After sleep!)    │                            │
│  │   linux_fill_sound_buffer()            │  ← Audio lag prevention    │
│  └────────────────────────────────────────┘                            │
│      │                                                                  │
│      ▼                                                                  │
│  ┌────────────────────────────────────────┐                            │
│  │ 6️⃣ ADAPTIVE FPS EVALUATION              │                            │
│  │   Every 300 frames (~5 seconds):       │                            │
│  │   - If >5% missed → Reduce FPS         │                            │
│  │   - If <1% missed → Increase FPS       │                            │
│  └────────────────────────────────────────┘                            │
│      │                                                                  │
│      ▼                                                                  │
│  Frame N+1 Start                                                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

#### 📊 Adaptive FPS State Machine

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   ADAPTIVE FPS LOGIC (State Machine)                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Start: target_fps = monitor_hz (e.g., 60Hz)                          │
│      │                                                                  │
│      ▼                                                                  │
│   ┌─────────────────────────────────────────┐                          │
│   │  Sample 300 frames (~5 seconds)         │                          │
│   │  Count frames that miss target by >2ms  │                          │
│   └─────────────────────────────────────────┘                          │
│      │                                                                  │
│      ▼                                                                  │
│   Calculate miss_rate = missed / 300                                   │
│      │                                                                  │
│      ├──────────────────────────────────────────────┐                  │
│      │                                              │                  │
│      ▼                                              ▼                  │
│   miss_rate > 5%?                               miss_rate < 1%?        │
│   (Performance BAD)                             (Performance GOOD)     │
│      │                                              │                  │
│      ▼                                              ▼                  │
│   ┌─────────────────────┐                     ┌──────────────────────┐ │
│   │  REDUCE TARGET FPS  │                     │  INCREASE TARGET FPS │ │
│   └─────────────────────┘                     └──────────────────────┘ │
│      │                                              │                  │
│      ▼                                              ▼                  │
│   60 → 30  ───────────────────────────────────  15 → 20               │
│   30 → 20      State Transitions               20 → 30               │
│   20 → 15  ───────────────────────────────────  30 → 60               │
│   15 → STUCK                                     60 → 120 (if monitor) │
│      │                                              │                  │
│      └──────────────────┬───────────────────────────┘                  │
│                         │                                              │
│                         ▼                                              │
│                   Reset counters                                       │
│                   Continue sampling                                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

Example Timeline:
─────────────────────────────────────────────────────────────────────────
Frame 0-299:    Target 60fps, miss rate 12% (too many misses!)
Frame 300:      ⚠️ Reduce to 30fps (auto-adjustment)
Frame 301-600:  Target 30fps, miss rate 1.5% (still a bit high)
Frame 601-900:  Target 30fps, miss rate 0.3% (smooth!)
Frame 900:      ✅ Increase to 60fps (try higher FPS)
Frame 901-1200: Target 60fps, miss rate 8% (oops, too fast again)
Frame 1200:     ⚠️ Back to 30fps (settle at sustainable rate)
```

---

#### 💻 Code Snippets with Explanations

##### 1️⃣ Two-Phase Sleep (Casey's Pattern)

**What:** Hybrid sleep strategy for precise frame timing  
**Why:** OS schedulers can't reliably sleep <5ms, spin-waiting wastes CPU  
**Solution:** Sleep coarsely until close, then spin-wait for precision

```c
// ═══════════════════════════════════════════════════════════════
// TWO-PHASE SLEEP FOR PRECISE FRAME TIMING
// ═══════════════════════════════════════════════════════════════
// Casey's insight: "Don't trust the OS scheduler for sub-5ms timing!"
// ═══════════════════════════════════════════════════════════════

real32 seconds_elapsed = work_seconds;
real32 target_seconds_per_frame = 1.0f / (real32)adaptive.target_fps;

if (seconds_elapsed < target_seconds_per_frame) {
    // ─────────────────────────────────────────────────────────
    // PHASE 1: COARSE SLEEP (Leave 3ms safety margin)
    // ─────────────────────────────────────────────────────────
    // Why 3ms? Typical OS scheduler granularity on Linux/Windows
    // Prevents oversleeping and missing frame deadline
    // ─────────────────────────────────────────────────────────

    real32 test_seconds = target_seconds_per_frame - 0.003f; // 3ms margin

    while (seconds_elapsed < test_seconds) {
        struct timespec sleep_spec = {0, 1000000}; // Sleep 1ms
        nanosleep(&sleep_spec, NULL);

        // Recheck elapsed time after each 1ms sleep
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        seconds_elapsed = (current_time.tv_sec - frame_start_time.tv_sec) +
                         (current_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;
    }

    // ─────────────────────────────────────────────────────────
    // PHASE 2: SPIN-WAIT (Tight loop for final microseconds)
    // ─────────────────────────────────────────────────────────
    // Why spin? Achieves microsecond precision (<100µs error)
    // CPU usage: ~100% for last 3ms, but worth it for smooth FPS
    // ─────────────────────────────────────────────────────────

    while (seconds_elapsed < target_seconds_per_frame) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        seconds_elapsed = (current_time.tv_sec - frame_start_time.tv_sec) +
                         (current_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;
    }
}
```

**Timing Breakdown (60fps = 16.67ms target):**

```
Work:         5.00ms  (game update + render)
Coarse sleep: 8.67ms  (sleep until 13.67ms mark)
Spin-wait:    3.00ms  (tight loop until 16.67ms)
Total:       16.67ms  ✅ Hit target exactly!
```

---

##### 2️⃣ OpenGL Initialization (Replacing XPutImage)

**What:** Setup OpenGL context for GPU-accelerated rendering  
**Why:** Solves RGBA color format issues, enables VSync, faster than XPutImage  
**Change:** Completely replaced X11 software rendering with OpenGL

```c
file_scoped_fn bool init_opengl(Display *display, Window window,
                                int width, int height) {
    // ═══════════════════════════════════════════════════════════
    // WHY OPENGL?
    // ═══════════════════════════════════════════════════════════
    // 1. XPutImage has RGBA/BGRA color format mismatch with Raylib
    // 2. OpenGL texture upload is GPU-accelerated (faster)
    // 3. glXSwapBuffers provides built-in VSync (no tearing)
    // 4. Same rendering path as Raylib (both use OpenGL internally)
    // ═══════════════════════════════════════════════════════════

    // Ask X11 for an OpenGL-capable visual (pixel format)
    int visual_attribs[] = {
        GLX_RGBA,           // We want RGBA color mode (32-bit)
        GLX_DEPTH_SIZE, 24, // 24-bit depth buffer (unused for 2D, but required)
        GLX_DOUBLEBUFFER,   // Enable double buffering (for VSync!)
        None                // Terminator
    };

    XVisualInfo *visual = glXChooseVisual(display, DefaultScreen(display),
                                         visual_attribs);
    if (!visual) {
        fprintf(stderr, "❌ No suitable OpenGL visual found\n");
        return false;
    }

    // Create OpenGL rendering context
    // GL_TRUE = direct rendering (GPU direct access, faster)
    g_gl.gl_context = glXCreateContext(display, visual, NULL, GL_TRUE);
    if (!g_gl.gl_context) {
        fprintf(stderr, "❌ Failed to create OpenGL context\n");
        XFree(visual);
        return false;
    }

    // Bind context to our window (like "activating" the context)
    glXMakeCurrent(display, window, g_gl.gl_context);

    // ─────────────────────────────────────────────────────────
    // CREATE GPU TEXTURE FOR OUR PIXEL BACKBUFFER
    // ─────────────────────────────────────────────────────────
    // This is like a <canvas> element in the browser
    // We'll upload our CPU pixels to this GPU texture every frame
    // ─────────────────────────────────────────────────────────

    glGenTextures(1, &g_gl.texture_id);
    glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);

    // GL_NEAREST = no filtering (sharp pixels, important for pixel art!)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // ─────────────────────────────────────────────────────────
    // SETUP 2D ORTHOGRAPHIC PROJECTION (No perspective)
    // ─────────────────────────────────────────────────────────
    // (0,0) = top-left, Y-down (like HTML canvas)
    // ─────────────────────────────────────────────────────────

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D); // Enable texturing

    printf("✅ OpenGL initialized (version: %s)\n", glGetString(GL_VERSION));
    XFree(visual);
    return true;
}
```

---

##### 3️⃣ Rendering with OpenGL (Replacing update_window)

**What:** Upload CPU pixels to GPU and display  
**Why:** Faster than XPutImage, solves color format issues, enables VSync  
**Before:** `XPutImage()` - slow software blitting  
**After:** `glTexImage2D()` + `glXSwapBuffers()` - GPU-accelerated + VSync

```c
file_scoped_fn void update_window_opengl(GameOffscreenBuffer *backbuffer) {
    if (!backbuffer->memory.base) return;

    // ═══════════════════════════════════════════════════════════
    // STEP 1: UPLOAD CPU PIXELS → GPU TEXTURE
    // ═══════════════════════════════════════════════════════════
    // Like updating an <img> src in the browser
    // This is where the magic happens - we tell OpenGL to copy
    // our CPU-rendered pixels to the GPU's memory
    // ═══════════════════════════════════════════════════════════

    glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);
    glTexImage2D(
        GL_TEXTURE_2D,              // Target
        0,                          // Mipmap level (0 = base image)
        GL_RGBA,                    // Internal GPU format
        backbuffer->width,
        backbuffer->height,
        0,                          // Border (must be 0)
        GL_RGBA,                    // Format of our CPU data (RGBA! ✅)
        GL_UNSIGNED_BYTE,           // Data type (8-bit per channel)
        backbuffer->memory.base     // Pointer to our pixel data
    );

    // ═══════════════════════════════════════════════════════════
    // STEP 2: CLEAR SCREEN TO BLACK
    // ═══════════════════════════════════════════════════════════
    glClear(GL_COLOR_BUFFER_BIT);

    // ═══════════════════════════════════════════════════════════
    // STEP 3: DRAW FULLSCREEN QUAD WITH OUR TEXTURE
    // ═══════════════════════════════════════════════════════════
    // Like <canvas> showing an <img> element
    // Texture coordinates: (0,0) = top-left, (1,1) = bottom-right
    // ═══════════════════════════════════════════════════════════

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 0);                        // Top-left
        glTexCoord2f(1.0f, 0.0f); glVertex2f(backbuffer->width, 0);        // Top-right
        glTexCoord2f(1.0f, 1.0f); glVertex2f(backbuffer->width, backbuffer->height); // Bottom-right
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0, backbuffer->height);       // Bottom-left
    glEnd();

    // ═══════════════════════════════════════════════════════════
    // STEP 4: SWAP FRONT/BACK BUFFERS (VSYNC HAPPENS HERE!)
    // ═══════════════════════════════════════════════════════════
    // This is like calling requestAnimationFrame() in the browser
    // If VSync is enabled (GLX_DOUBLEBUFFER), this WAITS for the
    // monitor's vertical retrace before swapping!
    // ═══════════════════════════════════════════════════════════

    glXSwapBuffers(g_gl.display, g_gl.window);
}
```

**Performance Comparison:**

| Method                 | Speed    | Color Format   | VSync  | GPU Accelerated       |
| ---------------------- | -------- | -------------- | ------ | --------------------- |
| `XPutImage()` (old)    | ~2-5ms   | BGRA (broken!) | ❌ No  | ❌ No (CPU copy)      |
| `glTexImage2D()` (new) | ~0.5-1ms | RGBA ✅        | ✅ Yes | ✅ Yes (DMA transfer) |

---

##### 4️⃣ Adaptive FPS State Machine

**What:** Automatically adjusts target FPS based on frame miss rate  
**Why:** Makes game playable on low-end hardware without manual settings  
**Innovation:** Not in original Handmade Hero, but follows Casey's philosophy of adaptive systems

```c
// ═══════════════════════════════════════════════════════════════
// ADAPTIVE FPS EVALUATION (Every 300 frames = ~5 seconds at 60fps)
// ═══════════════════════════════════════════════════════════════

adaptive.frames_sampled++;
if (frame_time_ms > (target_frame_time_ms + 2.0f) && g_window_is_active) {
    adaptive.frames_missed++;
}

if (adaptive.frames_sampled >= adaptive.sample_window) {
    real32 miss_rate = (real32)adaptive.frames_missed /
                       (real32)adaptive.frames_sampled;

    // ─────────────────────────────────────────────────────────
    // SCENARIO 1: Too many missed frames? Reduce target FPS
    // ─────────────────────────────────────────────────────────
    // If >5% of frames miss deadline, hardware can't keep up
    // Solution: Lower FPS target to sustainable rate
    // ─────────────────────────────────────────────────────────

    if (miss_rate > adaptive.miss_threshold) { // >5% miss
        int old_target = adaptive.target_fps;

        // State transitions (cascade down)
        if (adaptive.target_fps == 60)      adaptive.target_fps = 30;
        else if (adaptive.target_fps == 30) adaptive.target_fps = 20;
        else if (adaptive.target_fps == 20) adaptive.target_fps = 15;
        // (15fps is minimum - don't go lower!)

        if (adaptive.target_fps != old_target) {
            target_seconds_per_frame = 1.0f / (real32)adaptive.target_fps;
            printf("⚠️  Reducing FPS: %d → %d (miss rate: %.1f%%)\n",
                   old_target, adaptive.target_fps, miss_rate * 100.0f);
        }
    }

    // ─────────────────────────────────────────────────────────
    // SCENARIO 2: Performance recovered? Try higher FPS
    // ─────────────────────────────────────────────────────────
    // If <1% of frames miss, hardware has headroom
    // Solution: Increase FPS target (up to monitor refresh rate)
    // ─────────────────────────────────────────────────────────

    else if (miss_rate < adaptive.recover_threshold && // <1% miss
             adaptive.target_fps < adaptive.monitor_hz) {
        int old_target = adaptive.target_fps;

        // State transitions (cascade up)
        if (adaptive.target_fps == 15)      adaptive.target_fps = 20;
        else if (adaptive.target_fps == 20) adaptive.target_fps = 30;
        else if (adaptive.target_fps == 30) adaptive.target_fps = 60;
        // (Don't exceed monitor refresh rate!)

        if (adaptive.target_fps != old_target) {
            target_seconds_per_frame = 1.0f / (real32)adaptive.target_fps;
            printf("✅ Increasing FPS: %d → %d (miss rate: %.1f%%)\n",
                   old_target, adaptive.target_fps, miss_rate * 100.0f);
        }
    }

    // Reset sample window for next evaluation period
    adaptive.frames_sampled = 0;
    adaptive.frames_missed = 0;
}
```

**Example Session Output:**

```
Frame 0:     Starting at 60fps (monitor native)
Frame 150:   Miss rate 8.2% - too high!
Frame 300:   ⚠️  Reducing FPS: 60 → 30 (miss rate: 8.2%)
Frame 600:   Miss rate 0.5% - stable!
Frame 900:   ✅ Increasing FPS: 30 → 60 (miss rate: 0.5%)
Frame 1200:  Miss rate 7.1% - can't sustain 60fps
Frame 1500:  ⚠️  Reducing FPS: 60 → 30 (miss rate: 7.1%)
... settles at 30fps
```

---

##### 5️⃣ Cross-Platform Time Functions (Raylib Fix)

**What:** Replaced POSIX `clock_gettime()` with Raylib's cross-platform timers  
**Why:** Make Raylib backend work on Windows/macOS/Web (not just Linux)  
**Before:** `#include <time.h>`, `clock_gettime(CLOCK_MONOTONIC, &ts)`  
**After:** `GetTime()`, `GetFrameTime()`, `GetFPS()`

```c
// ❌ BEFORE (POSIX-only, broken on Windows):
###define _POSIX_C_SOURCE 199309L
#include <time.h>

file_scoped_global_var struct timespec g_frame_start;
file_scoped_global_var struct timespec g_frame_end;

static inline double get_wall_clock() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); // ❌ Doesn't exist on Windows!
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// Later in main loop:
clock_gettime(CLOCK_MONOTONIC, &g_frame_end);
real64 ms_per_frame = (g_frame_end.tv_sec - g_frame_start.tv_sec) * 1000.0 +
                      (g_frame_end.tv_nsec - g_frame_start.tv_nsec) / 1000000.0;

// ✅ AFTER (Cross-platform, works everywhere):
// No #include <time.h> needed!
// No struct timespec globals!

###if HANDMADE_INTERNAL
    static int frame_counter = 0;
    if (++frame_counter >= 60) {
        printf("[Raylib] %.2fms/f, %.0ff/s\n",
               GetFrameTime() * 1000.0f,  // ✅ Raylib function (cross-platform!)
               (float)GetFPS());          // ✅ Raylib function (cross-platform!)
        frame_counter = 0;
    }
###endif
```

**Why This Matters:**

| Platform             | `clock_gettime()`                | Raylib's `GetTime()`                         |
| -------------------- | -------------------------------- | -------------------------------------------- |
| **Linux**            | ✅ Works                         | ✅ Works (uses `clock_gettime()` internally) |
| **Windows**          | ❌ Doesn't exist!                | ✅ Works (uses `QueryPerformanceCounter()`)  |
| **macOS**            | ⚠️ Requires `<mach/mach_time.h>` | ✅ Works (uses `mach_absolute_time()`)       |
| **Web (Emscripten)** | ❌ No POSIX API                  | ✅ Works (uses `performance.now()`)          |

---

#### 🐛 Common Pitfalls

| Issue                                        | Cause                                      | Fix                                                                            | My Encountered Issues & Solutions                                                                                                                                  |
| -------------------------------------------- | ------------------------------------------ | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Missed frames despite high FPS**           | VSync disabled, GPU/CPU out of sync        | Enable double buffering (`GLX_DOUBLEBUFFER`), use `glXSwapBuffers()` for VSync | **Encountered:** Getting 300fps but stuttering! **Solution:** Added GLX double buffering - `glXSwapBuffers()` now waits for VSync. Smooth 60fps locked to monitor. |
| **Sleep overshoots target frame time**       | OS scheduler granularity (~5-10ms)         | Two-phase sleep: coarse sleep + spin-wait                                      | **Encountered:** `nanosleep(16ms)` often slept 18-20ms. **Solution:** Sleep until 3ms before target, then spin-wait. Now within 100µs precision!                   |
| **Color format mismatch (blue/red swapped)** | XPutImage uses BGRA, Raylib uses RGBA      | Switch to OpenGL `glTexImage2D(... GL_RGBA ...)`                               | **Encountered:** Gradient looked wrong (blue where red should be). **Solution:** Replaced `XPutImage()` with OpenGL. Now RGBA everywhere!                          |
| **Adaptive FPS ping-pongs between 30/60**    | Hysteresis needed, thresholds too close    | Use different thresholds for reduce (5%) vs recover (1%)                       | **Encountered:** FPS kept bouncing 30→60→30 every 5 seconds. **Solution:** Recover threshold (1%) much lower than miss threshold (5%). Now stable!                 |
| **Window loses focus, FPS drops to 0**       | VSync waits forever when window hidden     | Check `FocusOut` event, skip rendering or reduce FPS to 10                     | **Encountered:** 1000ms frame times when tabbed out! **Solution:** Added `g_window_is_active` flag, skip rendering when false. Background FPS now 10fps.           |
| **POSIX code breaks Raylib on Windows**      | `clock_gettime()` doesn't exist on Windows | Use Raylib's cross-platform `GetTime()`/`GetFrameTime()`                       | **Encountered:** Raylib backend compiled fine on Linux, failed on Windows CI. **Solution:** Removed all POSIX code, use Raylib APIs only. Now builds everywhere!   |

---

#### ✅ Skills Acquired

##### 🎯 **Core FPS Management**

- ✅ Implemented two-phase sleep strategy (coarse OS sleep + spin-wait) for sub-millisecond frame timing precision
- ✅ Measured frame time using `clock_gettime(CLOCK_MONOTONIC)` for monotonic, high-resolution timing
- ✅ Calculated and enforced target frame rate (e.g., 60 FPS = 16.67ms per frame)
- ✅ Detected missed frames and logged performance statistics

##### 🖥️ **Graphics & VSync**

- ✅ Replaced CPU-based XPutImage with GPU-accelerated OpenGL rendering
- ✅ Initialized OpenGL context with GLX for X11 window system
- ✅ Enabled VSync using `GLX_DOUBLEBUFFER` and `glXSwapBuffers()` to prevent screen tearing
- ✅ Uploaded pixel backbuffer to GPU texture using `glTexImage2D(GL_RGBA)`
- ✅ Rendered fullscreen textured quad to display CPU-rendered pixels

##### 🎮 **Adaptive Systems**

- ✅ Built adaptive FPS state machine that auto-adjusts (60→30→20→15) based on performance
- ✅ Sampled frame performance over 300-frame windows (~5 seconds) to detect trends
- ✅ Implemented hysteresis with different thresholds for reducing (5%) vs recovering (1%) FPS
- ✅ Queried monitor refresh rate using XRandR extension to set intelligent initial FPS target

##### 🐧 **Linux-Specific**

- ✅ Used `clock_gettime(CLOCK_MONOTONIC)` instead of Windows' `QueryPerformanceCounter()`
- ✅ Leveraged XRandR extension (`XRRConfigCurrentRate()`) to detect monitor refresh rate
- ✅ Handled X11 `FocusIn`/`FocusOut` events to pause rendering when window loses focus
- ✅ Set up OpenGL context with GLX (`glXCreateContext`, `glXMakeCurrent`, `glXSwapBuffers`)

##### 🌍 **Cross-Platform**

- ✅ Identified and removed POSIX-specific code (`<time.h>`, `clock_gettime()`) from Raylib backend
- ✅ Replaced with cross-platform Raylib APIs (`GetTime()`, `GetFrameTime()`, `GetFPS()`)
- ✅ Ensured Raylib backend compiles on Windows/Linux/macOS/Web without platform-specific `#ifdef`s
- ✅ Learned difference between POSIX APIs (Linux/macOS) vs Windows APIs (QueryPerformanceCounter)

##### 📊 **Debugging & Profiling**

- ✅ Added debug statistics tracking (min/max/avg frame time, missed frame count)
- ✅ Used `__rdtsc()` CPU cycle counter for microsecond-precision profiling
- ✅ Implemented conditional compilation (`#if HANDMADE_INTERNAL`) for debug-only features
- ✅ Created visual frame timing output (e.g., `[X11] 16.72ms/f, 59.80f/s, 35.32mc/f`)

##### 🧠 **Casey's Philosophy**

- ✅ **"Don't trust the OS scheduler"** - Learned why two-phase sleep is necessary (scheduler jitter)
- ✅ **"Adaptive, not hardcoded"** - Implemented FPS that adjusts to hardware instead of forcing 60fps
- ✅ **"Measure, don't guess"** - Used real frame timing data to drive adaptive decisions
- ✅ **"Cross-platform by design"** - Removed platform-specific code from shared Raylib backend

---

#### 🎓 Key Takeaways

1. **VSync is essential** - Without it, you get screen tearing and inconsistent frame pacing even at high FPS
2. **OS schedulers are unreliable** - Sub-5ms sleep requires spin-waiting (hybrid approach best)
3. **Adaptive > Fixed** - Auto-adjusting FPS makes games playable on more hardware without manual settings
4. **OpenGL solves multiple problems** - VSync, GPU acceleration, and color format consistency in one switch
5. **Cross-platform requires discipline** - One platform-specific function (`clock_gettime()`) breaks Windows builds
6. **Casey's patterns are timeless** - Two-phase sleep from 2014 still optimal in 2026!

## Misc

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
