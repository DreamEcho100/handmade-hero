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

## 🔮 Next: Day 7 (Audio)

Casey adds **DirectSound** on Windows. Linux equivalents:

| Windows     | Linux                      |
| ----------- | -------------------------- |
| DirectSound | ALSA (low-level)           |
| XAudio2     | PulseAudio (high-level)    |
| -           | Raylib `InitAudioDevice()` |

Key concepts coming:

- Ring buffer for audio samples
- Sample rate (44100 Hz typical)
- Channels (stereo = 2)
- Latency management
