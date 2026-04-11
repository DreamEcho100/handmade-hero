# Lesson 02 -- Sprite Loading with stb_image

## Observable Outcome
The Slime.png spritesheet (a 40x40 image containing four 20x20 frames) appears on screen. The slime character is visible against the dark purple background, confirming that PNG decoding, RGBA swizzling, and backbuffer blitting all work correctly.

## New Concepts (max 3)
1. stb_image single-header library integration (STB_IMAGE_STATIC, STB_IMAGE_IMPLEMENTATION)
2. Sprite struct -- owned pixel data with width/height metadata
3. RGBA byte-order swizzle -- converting stb's [R,G,B,A] bytes to our GAME_RGBA packed format

## Prerequisites
- Lesson 01 (working window with dark purple background)
- `vendor/stb_image.h` placed in the project's vendor directory

## Background

Loading images from disk is a solved problem. Sean Barrett's stb_image is a single-header C library that decodes PNG, BMP, TGA, and more. We include it in exactly one `.c` file with `STB_IMAGE_IMPLEMENTATION` defined, which expands the decoder implementation. Every other file that includes `stb_image.h` only gets the function declarations.

There is a critical subtlety: Raylib also bundles stb_image internally. If both our copy and Raylib's copy export the same function names, the linker will complain about duplicate symbols. We solve this by defining `STB_IMAGE_STATIC` before the implementation include, which marks all stb_image functions as `static`. They become file-local to sprite.c and cannot conflict with Raylib's copy.

The second challenge is byte order. stb_image always returns pixels as [R,G,B,A] bytes in memory (when requesting 4 channels). Our backbuffer uses a packed uint32_t format defined by `GAME_RGBA(r,g,b,a)`, which produces `(a<<24)|(b<<16)|(g<<8)|r`. On little-endian machines this means memory bytes are [R,G,B,A] -- but the integer bit layout is different from what stb gives us if we just cast the pointer. We must swizzle each pixel explicitly to match our GAME_RGBA packing.

## Code Walkthrough

### Step 1: stb_image integration in sprite.c

The implementation lives in a single compilation unit. We suppress unused-function warnings because `STB_IMAGE_STATIC` generates many internal helpers that our code does not call directly:

```c
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb_image.h"
#pragma GCC diagnostic pop
```

### Step 2: The Sprite struct

A Sprite is simply owned pixel data plus dimensions. The pixels are allocated by stb_image (via malloc internally) and freed with `stbi_image_free`:

```c
typedef struct {
  uint32_t *pixels;  /* RGBA pixel data (owned, from stb_image) */
  int width;         /* image width in pixels */
  int height;        /* image height in pixels */
} Sprite;
```

### Step 3: sprite_load -- decode and swizzle

`sprite_load` calls `stbi_load` requesting 4 channels (RGBA), then swizzles every pixel from stb's byte order to our GAME_RGBA format:

```c
int sprite_load(const char *path, Sprite *out) {
  int w, h, channels;
  unsigned char *data = stbi_load(path, &w, &h, &channels, 4);
  if (!data) {
    fprintf(stderr, "sprite_load: failed to load '%s': %s\n",
            path, stbi_failure_reason());
    return -1;
  }

  /* Swizzle RGBA bytes -> packed GAME_RGBA format.
   * stb gives us bytes: [R][G][B][A] per pixel.
   * We need uint32_t: (A<<24)|(B<<16)|(G<<8)|R. */
  int pixel_count = w * h;
  uint32_t *pixels = (uint32_t *)data;
  for (int i = 0; i < pixel_count; i++) {
    unsigned char *p = (unsigned char *)&pixels[i];
    unsigned char r = p[0];
    unsigned char g = p[1];
    unsigned char b = p[2];
    unsigned char a = p[3];
    pixels[i] = GAME_RGBA(r, g, b, a);
  }

  out->pixels = pixels;
  out->width = w;
  out->height = h;
  return 0;
}
```

Key points about the swizzle:
- We read byte-by-byte from the raw stb output (`p[0]` through `p[3]`)
- We then pack them using `GAME_RGBA` which places channels in the bit positions our backbuffer expects
- The cast to `(uint32_t *)data` lets us reuse the same buffer in-place -- no second allocation needed
- The `4` in `stbi_load(..., 4)` forces RGBA output regardless of the source format (even for images without alpha)

### Step 4: sprite_free

Cleanup must use `stbi_image_free` (not plain `free`), since stb_image may use a custom allocator:

```c
void sprite_free(Sprite *s) {
  if (s->pixels) {
    stbi_image_free(s->pixels);
    s->pixels = NULL;
  }
  s->width = 0;
  s->height = 0;
}
```

### Step 5: Loading in game_init

In `game/main.c`, the game loads sprites during initialization:

```c
void game_init(GameState *state, PlatformGameProps *props) {
  /* ... */
  if (player_init(&state->player) != 0) {
    fprintf(stderr, "Failed to initialize player\n");
  }
  /* ... */
}
```

And `player_init` in `game/player.c` loads the slime spritesheet:

```c
int player_init(Player *player) {
  memset(player, 0, sizeof(*player));
  /* ... position, physics init ... */

  if (sprite_load("assets/sprites/Slime.png", &player->sprite_slime) != 0) {
    fprintf(stderr, "Failed to load Slime.png\n");
    return -1;
  }
  /* ... */
  return 0;
}
```

### Step 6: Verifying the load

At this point, `player->sprite_slime` contains a 40x40 pixel array with all four slime frames packed into a grid. We can verify visually by blitting the entire sprite (all frames) onto the backbuffer in the next lesson.

## Common Mistakes

**Defining STB_IMAGE_IMPLEMENTATION in a header file.** If multiple `.c` files include that header, you get duplicate symbol errors. The implementation must live in exactly one compilation unit (sprite.c in our case).

**Forgetting STB_IMAGE_STATIC when using Raylib.** Without it, both Raylib and your code export `stbi_load`, `stbi_image_free`, etc. The linker picks one arbitrarily, which may differ in version or configuration. Making our copy static avoids the conflict entirely.

**Skipping the RGBA swizzle.** If you just cast the stb output to `uint32_t*` without swizzling, the red and blue channels will be swapped on screen (the slime will look blue instead of green). The swizzle loop is mandatory because GAME_RGBA's bit layout differs from stb's byte layout.

**Using `free()` instead of `stbi_image_free()`.** While these are often the same function, stb_image supports custom allocators via `STBI_MALLOC/STBI_FREE`. Always use the matching free to be safe.

**Not checking the return value of sprite_load.** If the asset path is wrong (e.g., running from the wrong working directory), `stbi_load` returns NULL. Without a check, you dereference a null pointer in the swizzle loop. The build script expects you to run from the `course/` directory where `assets/` lives.
