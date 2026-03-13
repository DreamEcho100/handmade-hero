#ifndef GAME_BASE_H
#define GAME_BASE_H

#include <stdint.h>
#include <string.h>

/* ── Debug ────────────────────────────────────────────────────────────── */
#if defined(DEBUG)
#  if defined(__i386__) || defined(__x86_64__)
#    define DEBUG_TRAP()   __asm__ volatile("int3")
#  elif defined(__aarch64__)
#    define DEBUG_TRAP()   __asm__ volatile("brk #0")
#  else
#    include <signal.h>
#    define DEBUG_TRAP()   raise(SIGTRAP)
#  endif
#  define DEBUG_ASSERT(cond)  do { if (!(cond)) { DEBUG_TRAP(); } } while (0)
#else
#  define DEBUG_TRAP()         ((void)0)
#  define DEBUG_ASSERT(cond)   ((void)(cond))
#endif

/* ── Button state ─────────────────────────────────────────────────────── */
typedef struct {
  int ended_down;
  int half_transitions;
} GameButtonState;

static inline int button_just_pressed(GameButtonState b) {
  return b.ended_down && (b.half_transitions > 0);
}

/* ── Mouse state (like Three.js OrbitControls input) ──────────────────── */
typedef struct {
  float x, y;           /* current pixel position                          */
  float dx, dy;         /* delta since last frame (for drag)               */
  float scroll;         /* wheel scroll delta (+ = zoom in, - = zoom out)  */
  int   left_down;      /* left button held = orbit                        */
  int   right_down;     /* right button held = pan                         */
  int   middle_down;    /* middle button held = pan (alt)                  */
} MouseState;

/* ── GameInput ────────────────────────────────────────────────────────── */
typedef struct {
  union {
    GameButtonState all[26];
    struct {
      /* Navigation */
      GameButtonState quit;
      GameButtonState camera_left;
      GameButtonState camera_right;
      GameButtonState camera_up;
      GameButtonState camera_down;
      GameButtonState camera_forward;
      GameButtonState camera_backward;
      /* Exports */
      GameButtonState export_ppm;
      GameButtonState export_anaglyph;
      GameButtonState export_sidebyside;
      GameButtonState export_stereogram;
      GameButtonState export_stereogram_cross;
      GameButtonState export_glsl;          /* Shift+L (L21) */
      /* Feature toggles */
      GameButtonState toggle_voxels;      /* V */
      GameButtonState toggle_floor;       /* F */
      GameButtonState toggle_boxes;       /* B */
      GameButtonState toggle_meshes;      /* M */
      GameButtonState toggle_reflections; /* R */
      GameButtonState toggle_refractions; /* T */
      GameButtonState toggle_shadows;     /* H */
      GameButtonState toggle_envmap;      /* E */
      GameButtonState toggle_aa;          /* X */
      GameButtonState cycle_envmap_mode;  /* C (L20) */
      /* Render scale */
      GameButtonState scale_cycle;        /* Tab */
    };
  } buttons;
  MouseState mouse;
} GameInput;

#define UPDATE_BUTTON(btn_ptr, is_down_val)                                  \
  do {                                                                        \
    GameButtonState *_b = (btn_ptr);                                         \
    if (_b->ended_down != (is_down_val)) {                                   \
      _b->ended_down = (is_down_val);                                        \
      _b->half_transitions++;                                                 \
    }                                                                         \
  } while (0)

static inline void prepare_input_frame(GameInput *curr, const GameInput *prev) {
  int n = (int)(sizeof(curr->buttons.all) / sizeof(curr->buttons.all[0]));
  for (int i = 0; i < n; i++) {
    curr->buttons.all[i].ended_down       = prev->buttons.all[i].ended_down;
    curr->buttons.all[i].half_transitions = 0;
  }
  /* Carry forward mouse button state; reset deltas. */
  curr->mouse.left_down   = prev->mouse.left_down;
  curr->mouse.right_down  = prev->mouse.right_down;
  curr->mouse.middle_down = prev->mouse.middle_down;
  curr->mouse.x           = prev->mouse.x;
  curr->mouse.y           = prev->mouse.y;
  curr->mouse.dx          = 0.0f;
  curr->mouse.dy          = 0.0f;
  curr->mouse.scroll      = 0.0f;
}

static inline void platform_swap_input_buffers(GameInput **curr, GameInput **prev) {
  GameInput *tmp = *curr;
  *curr = *prev;
  *prev = tmp;
}

#endif /* GAME_BASE_H */
