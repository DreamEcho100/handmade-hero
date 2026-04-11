# Lesson 05: X11, GLX, and the Explicit Window Boot Path

## Frontmatter

- Module: `02-windowing-and-boot`
- Lesson: `05`
- Status: Required
- Target files:
  - `src/platforms/x11/base.h`
  - `src/platforms/x11/base.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `X11State`
  - `g_x11`
  - `init_window`
  - `setup_vsync`
  - `shutdown_window`
  - `main`

## Observable Outcome

By the end of this lesson, you should be able to read the X11 backend without getting lost in API volume and explain:

- which resources are plain X11 resources
- which resources are GLX or OpenGL resources
- which values are backend bookkeeping only
- how the explicit X11 path still wraps the same shared runtime story you already saw in Raylib

The goal is not to memorize every X11 function. The goal is to classify what each stage is trying to own.

## Prerequisite Bridge

Lesson 04 showed the full backend shell shape using Raylib.

This lesson keeps the architecture but removes most of the convenience layer.

So the question changes from:

```text
what is the backend doing?
```

to:

```text
what exact resources does the backend have to claim by hand?
```

## Why This Lesson Exists

Raylib showed the platform shell with less noise.

Now you need to see what that convenience was hiding:

- opening the display connection
- choosing a visual
- creating the window
- creating the GL context
- creating the GPU texture
- wiring close handling
- probing vsync support
- destroying everything explicitly

That explicitness is exactly why the shared contract matters so much. Without it, this backend could easily swallow responsibilities that belong to shared startup or the game facade.

## Mini Glossary Before the Code

Keep these resource categories straight:

- `Display` = connection to the X server
- `Window` = the actual X11 window resource
- `GLXContext` = the OpenGL context bound to the window
- `Atom` = X11's interned identifier type, used here for close-window protocol handling
- `texture_id` = GPU texture used to present the software-rendered backbuffer

You do not need to memorize every API name yet. You do need to know what kind of resource each API is trying to acquire.

## Step 1: Read `X11State` as an Ownership Ledger

The backend-owned state is:

```c
typedef struct {
  Display *display;
  Window window;
  GLXContext gl_context;
  int screen;

  int window_w;
  int window_h;

  Atom wm_delete_window;
  bool vsync_enabled;
  bool window_focused;
  GLuint texture_id;
} X11State;

extern X11State g_x11;
```

And `base.c` defines it as:

```c
X11State g_x11 = {0};
```

### Read it by category

```text
X11 window-system resources
  -> display, window, screen, wm_delete_window

graphics resources
  -> gl_context, texture_id

backend bookkeeping
  -> window_w, window_h, vsync_enabled, window_focused
```

That category view is much easier to remember than the raw declaration order.

## Visual: What X11 Personally Owns

```text
g_x11
  +-- display           connection to X server
  +-- window            real X11 window handle
  +-- gl_context        OpenGL context for drawing into that window
  +-- screen            chosen screen number
  +-- window_w/h        current window dimensions
  +-- wm_delete_window  close-button protocol atom
  +-- vsync_enabled     swap-control capability result
  +-- window_focused    focus bookkeeping
  +-- texture_id        GPU texture for backbuffer upload
```

This is the explicit backend ledger that Raylib mostly hid from you.

## Step 2: Read `init_window()` as Stages, Not Noise

The function looks large because it performs several distinct boot stages in one place.

If you split those stages apart mentally, it becomes much easier to follow.

## Stage A: Open the Display Connection

```c
g_x11.display = XOpenDisplay(NULL);
if (!g_x11.display) {
  fprintf(stderr, "XOpenDisplay failed\n");
  return -1;
}
```

This is the outermost platform resource of the X11 backend.

Without a display connection, nothing else in the X11 path can happen.

## Stage B: Improve Later Input Behavior

```c
Bool ok;
XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);
if (!ok)
  fprintf(stderr, "XkbSetDetectableAutoRepeat failed\n");
```

This is a good early example of boot code shaping later system behavior.

The function is not rendering anything yet, but it is already influencing how later key handling will feel.

## Stage C: Choose Screen and Visual

```c
g_x11.screen = DefaultScreen(g_x11.display);
g_x11.window_w = GAME_W;
g_x11.window_h = GAME_H;

int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
if (!visual) {
  fprintf(stderr, "glXChooseVisual: no matching visual\n");
  return -1;
}
```

At this stage the backend is choosing the rendering-compatible visual information it needs before it can create the real window and GL context.

This is where the X11 and GL stories begin to overlap.

## Stage D: Create the Real X11 Window

```c
Colormap cmap =
    XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                    visual->visual, AllocNone);

XSetWindowAttributes wa = {
    .colormap = cmap,
    .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                  StructureNotifyMask | FocusChangeMask,
};

g_x11.window =
    XCreateWindow(g_x11.display, RootWindow(g_x11.display, g_x11.screen), 100,
                  100, GAME_W, GAME_H, 0, visual->depth, InputOutput,
                  visual->visual, CWColormap | CWEventMask, &wa);
```

This is the actual window-system acquisition step.

Notice how event categories are part of boot here. The backend is declaring early what kinds of platform messages it plans to handle later.

## Stage E: Create the GL Context

```c
g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
XFree(visual);

if (!g_x11.gl_context) {
  fprintf(stderr, "glXCreateContext failed\n");
  return -1;
}
```

This is a critical distinction:

```text
the X11 window existing does not automatically mean OpenGL can render into it
```

The GLX context is a separate resource.

That is why `X11State` contains both `window` and `gl_context`.

## Stage F: Make the Window Visible and Wire Close Handling

```c
XStoreName(g_x11.display, g_x11.window, TITLE);
XMapWindow(g_x11.display, g_x11.window);
g_x11.window_focused = true;

g_x11.wm_delete_window =
    XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);
```

This block turns the just-created window into a visible, well-behaved application window.

It also shows another small but important shared-contract detail: the title still comes from `TITLE`, not a backend-local string.

## Stage G: Bind GL and Create the Texture Bridge

```c
glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

glDisable(GL_DEPTH_TEST);
glEnable(GL_TEXTURE_2D);

glGenTextures(1, &g_x11.texture_id);
glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GAME_W, GAME_H, 0, GL_RGBA,
             GL_UNSIGNED_BYTE, NULL);
```

This is the X11 equivalent of Raylib's texture setup, but you can see the underlying pieces much more directly.

### What this means

```text
bind GL context to the window
create texture object
configure nearest-neighbor filtering
allocate GPU storage for the backbuffer image
```

The texture has no meaningful pixel contents yet. It only has storage reserved.

Later frames use `glTexSubImage2D(...)` to upload the current CPU pixels into that texture.

## Visual: What Raylib Was Hiding

```text
Raylib path
  InitWindow(...)
  LoadTextureFromImage(...)

X11 + GLX path
  XOpenDisplay(...)
  glXChooseVisual(...)
  XCreateWindow(...)
  glXCreateContext(...)
  glXMakeCurrent(...)
  glGenTextures(...)
  glTexImage2D(...)
```

The architecture is still the same. The explicit backend simply shows more of the underlying machinery.

## Comparison Table: Same Boot Story, Different Visibility

| Boot concern                   | Raylib view                 | X11/GLX view                                    | Why the explicit path matters                                         |
| ------------------------------ | --------------------------- | ----------------------------------------------- | --------------------------------------------------------------------- |
| open platform window           | `InitWindow(...)`           | `XOpenDisplay(...)` + `XCreateWindow(...)`      | separates server connection from real window ownership                |
| choose render-compatible setup | mostly hidden               | `glXChooseVisual(...)`                          | shows that not every window configuration is OpenGL-compatible        |
| create rendering context       | mostly hidden               | `glXCreateContext(...)` + `glXMakeCurrent(...)` | teaches that a visible window is not yet a valid GL target            |
| create presentation texture    | `LoadTextureFromImage(...)` | `glGenTextures(...)` + `glTexImage2D(...)`      | exposes the real GPU storage step behind the shared backbuffer bridge |
| handle close/focus protocol    | mostly hidden               | `XInternAtom(...)` + `XSetWMProtocols(...)`     | makes window-manager behavior visible instead of magical              |

If the X11 path feels noisy, return to this table. It compresses many API names back into the same few ownership jobs you already learned from Raylib.

## Step 3: `setup_vsync()` Is Capability Probing, Not Core Ownership

After `init_window()` succeeds, `main()` calls:

```c
setup_vsync();
```

That helper probes extension support and tries to enable swap control.

This is a good example of a secondary backend responsibility:

- important for frame pacing behavior
- not required to define the core window/context ownership model

In other words, it is a backend capability layer that sits after the essential boot resources exist.

## Step 4: Shared Startup Still Owns the Common Runtime Bundle

Once the explicit X11 window path is ready, `main()` does the same shared startup call you saw in Raylib:

```c
PlatformGameProps props = {0};
if (platform_game_props_init(&props) != 0) {
  fprintf(stderr, "Out of memory\n");
  return 1;
}
```

This is one of the most important lines in the whole module.

Even in the very explicit backend, the common runtime bundle is still claimed through shared startup rather than being reinvented inside `init_window()`.

That is how the course avoids turning into two separate runtimes.

## Step 5: X11 `main()` Keeps the Same High-Level Shape as Raylib

The start of `main()` is:

```c
if (init_window() != 0)
  return 1;
setup_vsync();

PlatformGameProps props = {0};
if (platform_game_props_init(&props) != 0) {
  fprintf(stderr, "Out of memory\n");
  return 1;
}

if (platform_audio_init(&props.audio_config) != 0) {
  fprintf(stderr, "Audio init failed - continuing without audio\n");
}

GameAppState *game = NULL;
if (game_app_init(&game, &props.perm) != 0) {
  ...
}
```

So even though the explicit backend is much more verbose, the architectural shape is still familiar:

```text
platform boot
  -> shared bundle boot
  -> optional audio boot
  -> game facade boot
  -> frame loop
```

That continuity is the whole point.

## Step 6: `shutdown_window()` Shows Explicit Reverse Cleanup

The cleanup helper is:

```c
static void shutdown_window(void) {
  if (g_x11.texture_id)
    glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)
    XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display)
    XCloseDisplay(g_x11.display);
}
```

This is the reverse direction of the resource story you just studied.

The backend releases:

1. GPU texture
2. GL context
3. X11 window
4. display connection

That order is not arbitrary. Later resources depended on earlier ones.

## Worked Example: Why `window` and `gl_context` Must Be Separate in Your Head

Suppose someone says:

```text
X11 already created the window, so rendering is ready.
```

That statement is incomplete.

The window resource and the GL context are different claims.

Until `glXCreateContext(...)` and `glXMakeCurrent(...)` succeed, the backend still does not have a valid OpenGL rendering context bound to the window.

This is exactly the kind of conceptual separation that the explicit backend is supposed to teach you.

## Common Beginner Mistakes

### Mistake 1: Treating X11 and GLX as the same subsystem

They interact, but they are not the same resource layer.

### Mistake 2: Forgetting that `X11State` is still just backend-private state

It is not the runtime's shared ownership bundle.

### Mistake 3: Reading the explicit backend as if it defines a different engine architecture

It does not.

It exposes the same shell with fewer abstractions hiding the details.

## Practice

Answer these before moving on:

1. Which fields in `X11State` are window-system resources?
2. Which fields are graphics resources?
3. Why does `glTexImage2D(..., NULL)` make sense during startup?
4. Why is `shutdown_window()` a meaningful architecture lesson rather than just cleanup trivia?

## Mini Exercise

Without looking back, write the X11 boot path as seven short stages.

If you cannot separate them yet, revisit Stages A through G.

## Troubleshooting Your Understanding

### "The X11 file feels much larger than Raylib"

That is normal.

The fix is not memorization. The fix is categorization:

```text
display
visual
window
context
texture
shared runtime bundle
game facade
```

### "I keep forgetting what belongs to shared startup"

If the resource is part of the common runtime surface seen by both backends, it probably belongs in `PlatformGameProps`, not in `X11State`.

## Recap

This lesson showed that the explicit X11 backend still follows the same architecture you already learned:

1. Claim platform resources by hand.
2. Claim the shared runtime bundle through shared startup.
3. Attach a GPU texture presentation path.
4. Initialize the shared game facade.
5. Clean up in reverse order.

The difference is not architectural. The difference is how much of the underlying platform work is visible.

## Next Lesson

Lesson 06 reconnects both backend shells at the ownership layer.

The central question becomes:

```text
if Raylib and X11 look so different,
what keeps them from drifting into two different runtimes?
```
