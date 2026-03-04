# Lesson 08 — State Machine

## What you build

You implement a four-phase game state machine (`TITLE → PLAYING →
LEVEL_COMPLETE → PLAYING` / `FREEPLAY`) with a single `change_phase()`
function as the only legal transition point, a title screen with a 6×5 level
select grid, and per-phase update and render functions that share no cross-phase
logic.

## Concepts introduced

- `GAME_PHASE` enum and the `PHASE_COUNT` sentinel
- `change_phase()` — the sole gatekeeper for phase transitions
- `phase_timer` for duration-based auto-advance
- `switch(state->phase)` dispatch in both `game_update` and `game_render`
- Title-screen level grid: layout macros, hover detection, locked levels
- `BUTTON_PRESSED` vs `ended_down` — "pressed this frame" vs "held"
- `state->should_quit` — cooperative quit signalling to the platform layer

## JS → C analogy

| JavaScript                                                        | C                                                                  |
|-------------------------------------------------------------------|--------------------------------------------------------------------|
| Redux `switch(action.type)` in a reducer                          | `switch(state->phase)` in `game_update`                            |
| `dispatch({ type: 'CHANGE_PHASE', phase: 'PLAYING' })`            | `change_phase(state, PHASE_PLAYING)`                               |
| `useEffect(() => { playSound() }, [phase])`                       | Side effects (audio cues) go inside `change_phase()` only          |
| `const [phase, setPhase] = useState('TITLE')`                     | `state->phase` field + `change_phase()` mutates it                 |
| `Date.now() - phaseStartedAt > 2000`                              | `state->phase_timer > 2.0f`                                        |

## Step-by-step

### Step 1: The `GAME_PHASE` enum

Open `game.h`. The four phases are declared as an enum:

```c
typedef enum {
    PHASE_TITLE,          /* title screen + level select grid         */
    PHASE_PLAYING,        /* active puzzle: simulation + drawing       */
    PHASE_LEVEL_COMPLETE, /* brief celebration, then advance           */
    PHASE_FREEPLAY,       /* sandbox after all 30 levels completed     */
    PHASE_COUNT           /* sentinel — used to detect missing cases   */
} GAME_PHASE;
```

`PHASE_COUNT` is not a real phase. Its purpose is to make the `switch`
statement in `game_update` exhaustive: when you add a new phase and forget to
handle it, the compiler warns about a missing case — a free correctness check.

In `GameState`:

```c
typedef struct {
    GAME_PHASE phase;
    float      phase_timer;   /* seconds elapsed in the current phase */
    /* ... */
} GameState;
```

### Step 2: `change_phase()` — the single transition point

```c
static void change_phase(GameState *state, GAME_PHASE next) {
  state->phase       = next;
  state->phase_timer = 0.0f;

  /* Audio: trigger sounds/music appropriate for the new phase.
   * ALL phase-entry audio lives here — never scattered across update functions.
   * This gives you one place to audit "what plays when phase changes". */
  switch (next) {
    case PHASE_TITLE:
      game_music_stop(&state->audio);
      break;
    case PHASE_PLAYING:
      game_music_play(&state->audio);
      game_play_sound(&state->audio, SOUND_TITLE_SELECT);
      break;
    case PHASE_LEVEL_COMPLETE:
      game_music_stop(&state->audio);
      game_play_sound(&state->audio, SOUND_LEVEL_COMPLETE);
      break;
    case PHASE_FREEPLAY:
      game_music_play(&state->audio);
      break;
    case PHASE_COUNT:
      break;
  }
}
```

The architectural rule is: **`change_phase()` is the only place that fires
audio triggers or other phase-entry side effects**. Never play sounds directly
from `update_playing()` or `update_level_complete()`. This gives you one place
to audit "what plays when the game changes state" — essential when debugging
audio bugs.

> **JS analogy:** this is like a Redux reducer that also dispatches side effects
> — or a state machine where each transition fires its own `onEnter()` callback.

`game_init` calls `change_phase(state, PHASE_TITLE)` to start cleanly:

```c
void game_init(GameState *state, GameBackbuffer *backbuffer) {
    memset(state, 0, sizeof(*state));
    /* ... backbuffer setup, initial values ... */
    state->unlocked_count = 1;
    state->gravity_sign   = 1;
    state->title_hover    = -1;
    change_phase(state, PHASE_TITLE);
}
```

### Step 3: `game_update` dispatch

```c
void game_update(GameState *state, GameInput *input, float dt) {
    if (dt > 0.1f) dt = 0.1f;   /* cap: prevents explosion after debugger pause */

    switch (state->phase) {
    case PHASE_TITLE:
        update_title(state, input);
        break;
    case PHASE_PLAYING:
        update_playing(state, input, dt);
        break;
    case PHASE_LEVEL_COMPLETE:
        update_level_complete(state, input, dt);
        break;
    case PHASE_FREEPLAY:
        update_freeplay(state, input, dt);
        break;
    case PHASE_COUNT:
        break;   /* unreachable — satisfies the compiler */
    }
}
```

Notice that `dt` is passed to phases that need physics but **not** to
`update_title`, which has no simulation. This isn't just tidiness — if you
accidentally call physics code from the title screen, the compiler will remind
you that `dt` is not in scope.

The same pattern applies to `game_render`:

```c
void game_render(const GameState *state, GameBackbuffer *bb) {
    switch (state->phase) {
    case PHASE_TITLE:          render_title(state, bb);          break;
    case PHASE_PLAYING:        render_playing(state, bb);        break;
    case PHASE_LEVEL_COMPLETE: render_level_complete(state, bb); break;
    case PHASE_FREEPLAY:       render_freeplay(state, bb);       break;
    case PHASE_COUNT:          break;
    }
}
```

`game_render` takes `const GameState *` — it must never modify simulation
data. The `const` is enforced by the compiler; if you accidentally write
`state->phase = PHASE_TITLE` inside a render function, you get a compile error.

### Step 4: Title screen update

```c
static void update_title(GameState *state, GameInput *input) {
    int mx = input->mouse.x;
    int my = input->mouse.y;
    state->title_hover = -1;

    int grid_x = TITLE_GRID_X;
    int grid_y = TITLE_GRID_Y;

    for (int i = 0; i < TOTAL_LEVELS; i++) {
        int col = i % TITLE_COLS,  row = i / TITLE_COLS;
        int bx  = grid_x + col * (TITLE_BTN_W + TITLE_BTN_GAP);
        int by  = grid_y + row * (TITLE_BTN_H + TITLE_BTN_GAP);

        if (mx >= bx && mx < bx + TITLE_BTN_W &&
            my >= by && my < by + TITLE_BTN_H) {
            state->title_hover = i;
            if (BUTTON_PRESSED(input->mouse.left) && i < state->unlocked_count) {
                level_load(state, i);
                change_phase(state, PHASE_PLAYING);
            }
        }
    }

    if (BUTTON_PRESSED(input->escape))
        state->should_quit = 1;
}
```

The layout macros `TITLE_GRID_X`, `TITLE_GRID_Y`, `TITLE_BTN_W`, etc. are
defined *once* above `update_title`:

```c
#define TITLE_BTN_W  70
#define TITLE_BTN_H  40
#define TITLE_BTN_GAP 8
#define TITLE_COLS   6
#define TITLE_GRID_X \
    ((CANVAS_W - (TITLE_COLS*(TITLE_BTN_W+TITLE_BTN_GAP) - TITLE_BTN_GAP)) / 2)
#define TITLE_GRID_Y 160
```

`render_title` uses the **same macros** to draw the buttons. This is the
critical pattern: if you hard-code button positions in `update_title` and
different positions in `render_title`, hover detection stops matching what the
player sees. One set of macros, two users.

The grid is 6 columns × 5 rows = 30 levels. `i < state->unlocked_count`
prevents clicking a locked level. `state->unlocked_count` starts at 1 and is
incremented inside `update_playing` when the player completes a level.

### Step 5: `BUTTON_PRESSED` vs `ended_down`

```c
#define BUTTON_PRESSED(b)  ((b).half_transition_count > 0 && (b).ended_down)
#define BUTTON_RELEASED(b) ((b).half_transition_count > 0 && !(b).ended_down)
```

- `ended_down` is `1` while the key is held, `0` while released.
- `half_transition_count` counts how many times the state changed *this
  frame*.

The difference matters on the title screen:

```c
/* ✅ Fires once when the button is first pressed */
if (BUTTON_PRESSED(input->mouse.left)) { ... }

/* ❌ Fires every frame while held — level reloads repeatedly */
if (input->mouse.left.ended_down) { ... }
```

`prepare_input_frame()` resets `half_transition_count` to zero at the start of
each frame. `ended_down` is preserved — it represents the current hold state,
not a one-time event.

### Step 6: Level-complete phase

```c
static void update_level_complete(GameState *state, GameInput *input, float dt) {
    state->phase_timer += dt;

    int advance = (state->phase_timer > 2.0f)
               || BUTTON_PRESSED(input->mouse.left)
               || BUTTON_PRESSED(input->enter);

    if (advance) {
        int next = state->current_level + 1;
        if (next >= TOTAL_LEVELS) {
            change_phase(state, PHASE_FREEPLAY);
        } else {
            level_load(state, next);
            change_phase(state, PHASE_PLAYING);
        }
    }
}
```

`phase_timer` is zeroed inside `change_phase()` and incremented every frame.
After 2 seconds the level auto-advances. The player can also click or press
Enter/Space to skip the celebration immediately.

When `next >= TOTAL_LEVELS` (level 30 completed), the game transitions to
`PHASE_FREEPLAY` instead of loading another puzzle.

### Step 7: Freeplay phase

```c
static void update_freeplay(GameState *state, GameInput *input, float dt) {
    state->phase_timer += dt;
    /* Hover + click for Reset button */
    /* ... same UI code as PLAYING ... */
    spawn_grains(state, dt);
    update_grains(state, dt);
    /* No check_win() call here */
}
```

`PHASE_FREEPLAY` is identical to `PHASE_PLAYING` except it never calls
`check_win()`. The player can draw lines and watch sugar flow indefinitely.
`render_freeplay` just calls `render_playing` and adds a "FREEPLAY" banner:

```c
static void render_freeplay(const GameState *state, GameBackbuffer *bb) {
    render_playing(state, bb);
    draw_text(bb, 10, 8, "FREEPLAY - ESC for menu", COLOR_UI_TEXT);
}
```

### Step 8: `should_quit` — cooperative exit

```c
if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;
```

The game never calls `exit()` directly. Instead it sets a flag in `GameState`.
The platform loop checks the flag:

```c
/* In main_x11.c / main_raylib.c */
while (!state.should_quit) {
    /* ... get input, game_update, game_render, present ... */
}
```

This separation keeps the game layer portable — it does not need to know
which platform is hosting it, and the platform layer retains control of the
process lifetime (important on some platforms where `exit()` needs cleanup).

### Step 9: Level-complete overlay rendering

```c
static void render_level_complete(const GameState *state, GameBackbuffer *bb) {
    render_playing(state, bb);   /* last playing frame underneath */
    draw_rect_blend(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_COMPLETE, 80);
    /* "LEVEL COMPLETE!" centred at 2× scale */
    int tx = (CANVAS_W - 15 * 9 * 2) / 2;
    int ty = CANVAS_H / 2 - 16;
    draw_text_scaled(bb, tx,     ty,      "LEVEL COMPLETE!", COLOR_WHITE, 2);
    draw_text_scaled(bb, tx + 2, ty + 20, "LEVEL COMPLETE!", COLOR_COMPLETE, 2);
    /* hint text */
    draw_text(bb, /* ... */, "CLICK or press ENTER for next level", COLOR_WHITE);
}
```

Key pattern: `render_playing` is called first to produce the background, then
a semi-transparent overlay is drawn on top. The game sim is still frozen
(no `update_playing` runs during `PHASE_LEVEL_COMPLETE`) so the background
is a static snapshot of the filled cups.

## Common mistakes to prevent

- **Playing audio outside `change_phase()`**: if you scatter `play_sound()`
  calls throughout `update_playing` or `update_level_complete`, it becomes
  very hard to audit "what sounds play on state change" when debugging. Put
  all phase-entry side effects in `change_phase()`.
- **Using different layout constants for update and render**: if `update_title`
  computes button positions with one set of numbers and `render_title` uses
  different numbers, hover detection visually mismatches the drawn buttons.
  Define the layout constants once and share them.
- **Checking `ended_down` instead of `BUTTON_PRESSED` for one-shot actions**:
  level loads and phase transitions must happen once per click, not every frame
  the mouse is held. Use `BUTTON_PRESSED`.
- **Calling `check_win` in `PHASE_FREEPLAY`**: freeplay is explicitly the
  sandbox mode with no win condition. Adding a win check there would
  unexpectedly advance the level.
- **Calling `game_render` in const-incorrect ways**: `game_render` takes
  `const GameState *`. If you ever need to update state inside a render
  function, you have broken the read-only contract. Move that logic to
  `game_update`.

## What to run

```bash
./build-dev.sh
```

Test the state machine:

1. **Title screen**: window opens showing the "SUGAR, SUGAR" title and a 6×5
   level grid. Only level 1 is clickable (others appear greyed out).
2. **PLAYING**: click level 1 — simulation starts. Draw a line; watch sugar
   flow.
3. **LEVEL_COMPLETE**: fill the cup. A green overlay appears with "LEVEL
   COMPLETE!". After 2 seconds (or on click) it auto-advances to level 2.
4. **Title → quit**: return to the title screen (Escape while playing) and
   press Escape again — the window should close.
5. **FREEPLAY**: complete all 30 levels to reach freeplay mode (or temporarily
   set `state->unlocked_count = TOTAL_LEVELS` in `game_init` to test it
   quickly).

## Summary

The state machine gives the game a clear narrative arc — title → play →
celebrate → play — while isolating each phase's logic so changes to the
playing simulation never accidentally affect the title screen. The central
`change_phase()` function acts as the single authoritative transition point
where all phase-entry side effects (audio, timers, resets) belong. Layout
constants are defined once and shared between update and render so that UI
hit-boxes always match what the player sees, and the `BUTTON_PRESSED` macro
ensures one-shot actions like level selection trigger exactly once per click
regardless of how long the button is held.
