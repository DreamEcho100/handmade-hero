# Lesson 01 -- Project Setup and Colored Window

## Observable Outcome
A 960x540 window opens with a dark purple background. Both the Raylib and X11 backends compile and display an identical solid-color canvas. Pressing ESC closes the window cleanly.

## New Concepts (max 3)
1. Adapting the Platform Foundation template for a new game (base.h, platform.h, game/base.h)
2. COORD_ORIGIN_LEFT_TOP -- y-down screen convention matching the original Godot source
3. Build script with dual backends (Raylib and X11/GLX)

## Prerequisites
- Completed Platform Foundation Course (or equivalent understanding of the backbuffer, draw_rect, and platform contract)
- Working C toolchain (clang), Raylib installed, X11/GLX/ALSA dev headers

## Background

Every new game in this engine starts by forking the platform-backend template and customising four files. The template gives us a working window, backbuffer, audio pipeline, and input system. Our job in this lesson is to wire those pieces up for Jetpack Joyride's specific requirements.

The original SlimeEscape game (our Godot 4.3 reference) uses a 320x180 viewport -- a popular pixel-art resolution that scales cleanly by integer multiples. At 3x we get 960x540, which fits comfortably on modern displays while preserving the chunky pixel aesthetic. We choose COORD_ORIGIN_LEFT_TOP because the original Godot project uses y-down screen coordinates: y=0 is the top of the screen and y increases downward. This means all position constants from the GDScript transfer directly without sign flipping.

The game-unit system uses a 1:1 ratio (one game unit = one pixel at native resolution). GAME_UNITS_W is 320.0f, matching GAME_W. This trivial mapping keeps the mental model simple -- every constant you read in the source is literally a pixel count at native res. Despite the 1:1 ratio, we still route everything through units_to_pixels. GAME_UNITS always applies, even when the ratio is trivial, because it ensures resolution independence if you ever resize the canvas.

## Code Walkthrough

### Step 1: utils/base.h -- canvas dimensions and coordinate system

This file defines the global constants every other file depends on:

```c
#define GAME_W 320
#define GAME_H 180

#define GAME_UNITS_W 320.0f
#define GAME_UNITS_H 180.0f
```

The coordinate origin is set to LEFT_TOP in the GameWorldConfig struct. The HUD helper macros make it easy to position UI elements relative to screen edges:

```c
#define HUD_TOP_Y(offset_from_top) ((float)(offset_from_top))
#define HUD_BOTTOM_Y(offset_from_bottom) \
  ((float)GAME_UNITS_H - (float)(offset_from_bottom))
```

The GameWorldConfig struct carries camera state and coordinate origin. It is built once per frame and passed into make_render_context():

```c
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  float camera_x;
  float camera_y;
  float camera_zoom;
  /* ... custom fields for COORD_ORIGIN_CUSTOM ... */
} GameWorldConfig;
```

### Step 2: platform.h -- game title and shared props

platform.h is the contract between game code and platform backends. We set the title:

```c
#ifndef GAME_TITLE
#define GAME_TITLE "Jetpack Joyride"
#endif
```

The PlatformGameProps struct is allocated identically by every backend. It bundles the backbuffer, audio buffer, arenas, and world config:

```c
static inline int platform_game_props_init(PlatformGameProps *props) {
  props->scale_mode = WINDOW_SCALE_MODE_FIXED;
  props->world_config.coord_origin = COORD_ORIGIN_LEFT_TOP;
  props->world_config.camera_zoom = 1.0f;
  /* ... allocate backbuffer, audio buffer, arenas ... */
}
```

### Step 3: game/base.h -- input buttons

We define 6 buttons for this game. Each button is a GameButtonState with ended_down and half_transitions for frame-accurate press/release detection:

```c
#define GAME_BUTTON_COUNT 6

typedef struct {
  union {
    GameButtonState all[GAME_BUTTON_COUNT];
    struct {
      GameButtonState action;   /* SPACE / Click -- fly / select */
      GameButtonState quit;     /* ESC -- close / back */
      GameButtonState pause;    /* P -- toggle pause */
      GameButtonState restart;  /* R -- restart game */
      GameButtonState left;     /* LEFT / A -- menu navigation */
      GameButtonState right;    /* RIGHT / D -- menu navigation */
    };
  } buttons;
  float mouse_x;
  float mouse_y;
  int mouse_down;
} GameInput;
```

### Step 4: build-dev.sh -- compilation

The build script supports both backends via a `--backend` flag:

```bash
SOURCES="src/utils/backbuffer.c src/utils/draw-shapes.c src/utils/draw-text.c \
  src/utils/sprite.c src/game/player.c src/game/main.c ..."

case "$BACKEND" in
    x11)
        BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX -lpthread"
        SOURCES="$SOURCES src/platforms/x11/main.c ..."
    ;;
    raylib)
        BACKEND_LIBS="-lm -lraylib -lpthread -ldl -Wl,-z,muldefs"
        SOURCES="$SOURCES src/platforms/raylib/main.c"
    ;;
esac

clang $BASE_FLAGS $INCLUDE_FLAGS -o "$BINARY" $SOURCES $BACKEND_LIBS
```

Build and run with:

```bash
./build-dev.sh --backend=raylib -r
./build-dev.sh --backend=x11 -r
```

### Step 5: Raylib backend opens the window

In `platforms/raylib/main.c`, the window opens at 3x native resolution:

```c
InitWindow(GAME_W * 3, GAME_H * 3, TITLE);  /* 960x540 */
SetTargetFPS(TARGET_FPS);                     /* 60 fps */
SetWindowState(FLAG_WINDOW_RESIZABLE);
```

### Step 6: Dark purple background

In `game/main.c`, the render_background function fills with a dark purple when no background sprite is loaded:

```c
static void render_background(GameState *state, Backbuffer *bb) {
  if (!state->bg_sprite.pixels) {
    draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
              0.078f, 0.047f, 0.110f, 1.0f);
    return;
  }
  /* ... parallax rendering ... */
}
```

The color (0.078, 0.047, 0.110) is a muted dark purple -- chosen to match the underground cavern feel of the original game.

## Common Mistakes

**Using COORD_ORIGIN_LEFT_BOTTOM for a Godot port.** Godot uses y-down (screen convention). If you use LEFT_BOTTOM (y-up), every vertical position will be mirror-flipped. The player will spawn near the ceiling instead of the floor, and gravity will push them upward on screen.

**Hardcoding pixel values instead of using GAME_UNITS.** Even with a 1:1 ratio, writing `draw_rect(bb, 80.0f, ...)` instead of converting through the unit system means your code breaks silently if anyone changes GAME_W or GAME_UNITS_W. Always go through the conversion path.

**Forgetting `-Wl,-z,muldefs` for the Raylib backend.** Raylib bundles its own stb_image and stb_vorbis. When we also compile our own copy, the linker sees duplicate symbols. The `-z,muldefs` flag tells it to use the first definition and ignore duplicates.

**Not capping dt.** The Raylib backend caps `dt` to 0.05f (50ms). Without this cap, a single long frame (e.g., from window drag) can cause the physics to explode, launching the player off-screen in one step.
