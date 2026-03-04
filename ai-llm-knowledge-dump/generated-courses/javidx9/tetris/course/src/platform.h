/* ═══════════════════════════════════════════════════════════════════════════
 * platform.h  —  Phase 2 Course Version
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * This header defines the CONTRACT between the game layer and the platform
 * layer.  It contains:
 *   • PlatformAudioConfig  — how the platform configures audio buffering
 *   • PlatformGameProps    — the complete platform runtime state
 *   • Helper functions     — shared init/free/swap logic used by all backends
 *
 * The game (game.c) never includes this file — it knows nothing about the
 * platform.  Only the platform entry points (main_x11.c, main_raylib.c) need it.
 *
 * JS analogy: think of platform.h as the "Node.js / browser adapter" that
 * sits between your pure-logic module (game.c) and the host environment.
 * Different backends (X11, Raylib, SDL) implement the same interface
 * declared here, just like React Native, React DOM, and React Test Renderer
 * all implement the same React reconciler interface for different hosts.
 */

#ifndef PLATFORM_H   /* Include guard — same as JS module-level dedup trick  */
#define PLATFORM_H

#include "game.h"      /* GameState, GameInput, Backbuffer, FIELD_* constants */

#include <stdbool.h>   /* bool, true, false */
#include <stdio.h>     /* fprintf, stderr   */
#include <stdlib.h>    /* malloc, free      */
#include <string.h>    /* memset            */

/* ═══════════════════════════════════════════════════════════════════════════
 * PlatformAudioConfig  —  Casey's Latency Model
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * CASEY'S LATENCY MODEL
 * ──────────────────────
 * Audio output works by filling a ring buffer in advance.  The OS/sound card
 * drains the buffer continuously at `samples_per_second` samples/second.
 * We must always stay ahead — if we're too slow, the driver underruns and you
 * hear a click or silence.
 *
 * The model targets writing ~2 frames of audio ahead ("latency_samples"):
 *
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │ Audio ring buffer (OS-managed)                                       │
 *   │                                                                      │
 *   │  [  already played  ] [ safety ] [  our write target  ] [ gap ]      │
 *   │                       ↑          ↑                                   │
 *   │              play cursor     write cursor                            │
 *   └──────────────────────────────────────────────────────────────────────┘
 *
 *   samples_per_frame  = samples_per_second / hz
 *                        How many samples one game frame produces.
 *                        Example: 44100 / 60 = 735 samples/frame.
 *
 *   latency_samples    = samples_per_frame * frames_of_latency
 *                        How far ahead we target writing.
 *                        Example: 735 * 2 = 1470 samples (~33 ms at 44100 Hz)
 *
 *   safety_samples     = samples_per_frame / 3
 *                        A small margin added to avoid cutting it too close.
 *                        If the play cursor advances faster than expected,
 *                        safety_samples keeps us from underrunning.
 *                        Example: 735 / 3 = 245 samples (~5.5 ms at 44100 Hz)
 *
 *   running_sample_index  = Total samples written since program start (ever).
 *                           Monotonically increasing int64.  Used to compute
 *                           the ABSOLUTE write position in the ring buffer:
 *                             write_pos = running_sample_index % buffer_size
 *                           Storing the absolute index (not just a ring offset)
 *                           prevents wrap-around arithmetic bugs when comparing
 *                           cursor positions.
 *
 * JS equivalent: Web Audio API AudioWorklet processes a fixed block size
 * automatically.  Here we implement the equivalent scheduling manually.
 */
typedef struct {
  int samples_per_second;   /* 44100 or 48000 — sample rate chosen at init     */
  int buffer_size_samples;  /* Total ring-buffer size in samples                */
  int is_initialized;       /* 1 once platform_audio_init() succeeds            */

  /* ── Casey's Latency Model fields ── */
  int latency_samples;      /* Target write-ahead in samples
                               = samples_per_frame * frames_of_latency
                               Example: 735 * 2 = 1470 @ 44100/60             */

  int safety_samples;       /* Safety margin to avoid underrun
                               = samples_per_frame / 3
                               Example: 735 / 3 = 245 @ 44100/60              */

  int samples_per_frame;    /* Samples per one game frame
                               = samples_per_second / hz
                               Example: 44100 / 60 = 735                      */

  int64_t running_sample_index; /* Monotonic total samples written (ever).
                                    Used to compute ring-buffer write position.
                                    JS: like a sequence number / version counter. */

  int frames_of_latency;    /* CONFIG: how many frames ahead to buffer audio.
                               Set before platform_audio_init().  Default: 2.  */

  int hz;                   /* CONFIG: game update rate in Hz (e.g., 60).
                               Set before platform_audio_init().               */
} PlatformAudioConfig;

/* ═══════════════════════════════════════════════════════════════════════════
 * PlatformGameProps  —  All Runtime State for One Platform Instance
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * One of these lives on the stack in main().  It holds everything the
 * platform needs to run: the window dimensions, the pixel backbuffer,
 * and the audio config.
 *
 * JS: like a `AppContext` object passed around between platform modules.
 * C:  typedef struct { ... } PlatformGameProps;
 *   — JS: interface PlatformGameProps { title: string; width: number; ... }
 */
typedef struct {           /* JS: like a plain configuration/context object  */
  const char *title;       /* Window title string — pointer to a string literal */
  int width;               /* Window width in pixels                            */
  int height;              /* Window height in pixels                           */
  Backbuffer backbuffer;   /* Pixel memory; malloc'd in platform_game_props_init */
  PlatformAudioConfig audio; /* Audio configuration (see above)                 */
  bool is_running;         /* Main loop continues while true                    */
} PlatformGameProps;

/* ═══════════════════════════════════════════════════════════════════════════
 * STRINGIFY / TOSTRING Macros  —  Compile-Time String Injection
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * STRINGIFY(x) turns ANY token x into the string literal "x".
 *   STRINGIFY(hello)  → "hello"
 *   STRINGIFY(42)     → "42"
 *
 * TOSTRING(x) expands MACROS first, THEN stringifies.
 *   #define BACKEND raylib
 *   STRINGIFY(BACKEND)  → "BACKEND"   ← macro not expanded (wrong!)
 *   TOSTRING(BACKEND)   → "raylib"    ← macro IS expanded first (correct!)
 *
 * WHY TWO MACROS?
 *   The C preprocessor evaluates # (stringify) before macro-expanding its
 *   argument.  Wrapping in a second macro forces one extra expansion pass.
 *
 * TITLE uses TOSTRING so the backend name (defined with -DBACKEND=raylib on
 * the compiler command line) is injected into the window title at compile time.
 *
 * JS equivalent: template literals — but resolved at compile time, not runtime.
 *   JS: `Tetris (${BACKEND}) - yt/@javidx9`  (runtime)
 *   C:  "Tetris (" TOSTRING(BACKEND) ") - yt/@javidx9"  (compile time)
 */
#define STRINGIFY(x)  #x                          /* Stringify token directly  */
#define TOSTRING(x)   STRINGIFY(x)                /* Expand macro, then stringify */
#define TITLE  ("Tetris (" TOSTRING(BACKEND) ") - yt/@javidx9")
/* Adjacent string literals are concatenated by the C compiler:
   "Tetris (" "raylib" ") - yt/@javidx9" → "Tetris (raylib) - yt/@javidx9"   */

/* ═══════════════════════════════════════════════════════════════════════════
 * platform_game_props_init  —  One-Time Setup
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Computes window dimensions from game constants, allocates the backbuffer,
 * and sets audio config defaults.  Returns 0 on success, 1 on failure.
 *
 * `static` makes this function private to any file that includes platform.h
 * (it won't be exported to the linker as a symbol).
 * `inline` hints the compiler to inline the body (no call overhead).
 *
 * JS: static inline ≈ a helper function inside a module, not exported.
 * C:  static int platform_game_props_init(...) — local to this TU.
 */
static int platform_game_props_init(PlatformGameProps *props) {
  /* Compute window dimensions from game-layer constants */
  int screen_width  = (FIELD_WIDTH * CELL_SIZE) + SIDEBAR_WIDTH;
  int screen_height = FIELD_HEIGHT * CELL_SIZE;

  props->is_running = true;
  props->title      = TITLE;
  props->width      = screen_width;
  props->height     = screen_height;

  /* ── Allocate Backbuffer ──
     The backbuffer is a flat array of uint32_t pixels: one 32-bit ARGB value
     per pixel.  We allocate it once here; game_render() writes into it every
     frame; the platform blits it to the screen.
     JS: like allocating an ImageData buffer: new Uint32Array(w * h).        */
  props->backbuffer.width           = props->width;
  props->backbuffer.height          = props->height;
  props->backbuffer.bytes_per_pixel = 4; /* ARGB: A=8bit, R=8bit, G=8bit, B=8bit */
  props->backbuffer.pitch           = props->width * props->backbuffer.bytes_per_pixel;
                                      /* Pitch = bytes per row (for row-stride access) */
  props->backbuffer.pixels =
      (uint32_t *)malloc((size_t)props->width * (size_t)props->height * sizeof(uint32_t));
      /* malloc: allocate heap memory.  Returns NULL on failure.
         JS: new Uint32Array(w * h) — automatic; C requires explicit malloc.   */

  if (!props->backbuffer.pixels) {
    fprintf(stderr, "Failed to allocate backbuffer\n"); /* stderr: error stream */
    return 1; /* Non-zero = error (Unix convention) */
  }

  /* ── Audio Defaults ──
     Set configuration values before calling platform_audio_init().
     platform_audio_init() reads these to compute latency_samples, etc.     */
  memset(&props->audio, 0, sizeof(PlatformAudioConfig)); /* Zero all fields   */
  props->audio.hz                = 60; /* Game runs at 60 Hz (frames/second) */
  props->audio.frames_of_latency = 2;  /* Target ~2 frames of audio ahead    */

  return 0; /* Success */
}

/* ── platform_game_props_free ── */
static void platform_game_props_free(PlatformGameProps *props) {
  free(props->backbuffer.pixels); /* Release the malloc'd pixel memory        */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Interface  —  Implemented per Backend
 * ═══════════════════════════════════════════════════════════════════════════
 * These functions are declared here and DEFINED in each backend file:
 *   main_x11.c    → X11 / Linux implementation
 *   main_raylib.c → Raylib (cross-platform) implementation
 *   etc.
 *
 * The game layer (game.c) never calls these — only main() does.
 */
int    platform_init(PlatformGameProps *props);
void   platform_render(GameState *state);
double platform_get_time(void);
void   platform_shutdown(void);
int    platform_audio_init(PlatformAudioConfig *config);
void   platform_audio_shutdown(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * platform_swap_input_buffers
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHY SWAP CONTENTS, NOT POINTERS
 * ─────────────────────────────────
 * The platform maintains two input structs: old_input and current_input.
 * Each frame we want current_input to become old_input, and old_input to
 * become the fresh slate for the new frame.
 *
 * WRONG APPROACH: swapping POINTERS
 *   GameInput *tmp = current_input;
 *   current_input  = old_input;
 *   old_input      = tmp;
 *   ⚠ This only swaps the local pointer variables — the caller's pointers
 *     are NOT changed because C passes pointers by value.  The caller still
 *     holds its original pointers, now pointing at swapped buffers.  This
 *     causes one-frame-behind bugs and is hard to debug.
 *
 * CORRECT APPROACH: swapping CONTENTS (struct copy via value)
 *   GameInput temp  = *current_input;   // copy current → temp
 *   *current_input  = *old_input;       // copy old → current
 *   *old_input      = temp;             // copy temp → old
 *   ✓ The memory locations (old_input, current_input) don't move.
 *     The DATA inside them is exchanged.  The caller's pointers remain valid.
 *
 * After the swap, prepare_input_frame() resets half_transition_count on
 * current_input so it starts clean for the next frame's key events.
 *
 * JS equivalent: like using structuredClone() to swap two objects' contents:
 *   const temp = structuredClone(currentInput);
 *   Object.assign(currentInput, oldInput);
 *   Object.assign(oldInput, temp);
 */
static inline void platform_swap_input_buffers(GameInput *old_input,
                                               GameInput *current_input) {
  /* Struct assignment in C copies ALL bytes of the struct — like a deep copy
     for plain data structs (no pointers inside GameInput).
     JS: structuredClone(obj)                                                 */
  GameInput temp   = *current_input; /* Step 1: save current frame state       */
  *current_input   = *old_input;     /* Step 2: load previous frame state       */
  *old_input       = temp;           /* Step 3: stash what was current → old   */
}

#endif /* PLATFORM_H */
