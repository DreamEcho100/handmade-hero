# PLAN — Tetris Build-Along Course

## What the final game does

A complete, playable Tetris clone built entirely in C without any game-engine
framework. Seven tetrominoes fall from the top of a 12×18 grid; the player
moves, rotates, and soft-drops them using the keyboard. When one or more rows
are completely filled, they flash white briefly, then collapse, awarding points.
The game speeds up every 25 pieces. A sidebar shows the score, level, piece
count, and the next piece preview. Procedural sound effects (move, rotate, drop,
line-clear, Tetris, level-up, game-over, restart) are rendered in stereo with
spatial panning tied to the piece's horizontal position. A four-pattern MIDI
sequencer plays the classic Korobeiniki theme using a sine-less square-wave
synthesiser. The game can be built against two independent backends — Raylib
(all platforms) and X11/ALSA (Linux) — without changing a single line of game
logic.

---

## Lesson sequence

| #  | Title                        | What gets built                                                                          | What the student runs/sees                                           |
|----|------------------------------|------------------------------------------------------------------------------------------|----------------------------------------------------------------------|
| 01 | Window & Backbuffer          | Open a window, allocate a pixel buffer, fill it with a solid colour, display it          | A solid-coloured window at the correct game resolution               |
| 02 | Drawing Primitives           | `draw_rect()` and `draw_rect_blend()` with clipping; colour macros                       | Coloured rectangles on screen, one semi-transparent overlay          |
| 03 | Input                        | `GameButtonState`, `UPDATE_BUTTON`, `prepare_input_frame`, double-buffering              | A coloured square that moves with arrow/WASD keys                    |
| 04 | Game Structure               | `GameState`, `game_init()`, `game_update()`, `game_render()` skeleton                   | Same moving square but code now lives in the 3-layer architecture    |
| 05 | Tetromino Data               | `TETROMINOES[]` char bitmap, `tetromino_pos_value()`, render one fixed piece             | A single tetromino drawn in the centre of the field                  |
| 06 | Tetromino Movement           | Keyboard-driven move + rotate, `tetromino_does_piece_fit()`, `RepeatInterval` / DAS-ARR | Fully controllable, collision-checked tetromino                      |
| 07 | The Playfield                | `TETRIS_FIELD_CELL` enum, wall init, piece locking on landing                            | Pieces stack up; walls visible; game never crashes on landing        |
| 08 | Line Clearing                | Detect full rows, `TETRIS_FIELD_TMP_FLASH`, flash timer, collapse logic                 | Completed rows flash white and disappear                             |
| 09 | Scoring & Levels             | Score formula, `pieces_count`, `level`, drop-interval scaling                           | Score and level update in sidebar; game visibly speeds up            |
| 10 | UI — Sidebar & Text          | `draw_text()`, bitmap font, score/level/pieces/next-piece sidebar                       | Complete HUD: score, level, piece count, next-piece preview          |
| 11 | Audio — Sound Effects        | `AudioOutputBuffer`, `SoundInstance`, `game_play_sound_at()`, stereo pan, fade-in/out   | Audible sound on every game event with spatial panning               |
| 12 | Audio — Background Music     | `MusicSequencer`, `ToneGenerator`, MIDI→frequency, volume ramping                       | Korobeiniki theme loops in the background                            |
| 13 | Polish — Ghost & Game Over   | Ghost piece, game-over overlay, restart; final complete game                            | Ghost piece projection; GAME OVER screen; R restarts cleanly         |

---

## JS → C concept mapping per lesson

| Lesson | New C concepts introduced                                              | Nearest JS equivalent                                              |
|--------|------------------------------------------------------------------------|---------------------------------------------------------------------|
| 01     | `malloc`/`free`, `uint32_t *` pixel buffer, `typedef struct`           | `new ArrayBuffer(w*h*4)`, typed arrays                             |
| 02     | Pointer arithmetic (`py * pitch/4 + px`), bit shifting, `uint8_t`     | `Uint32Array` index math, `(r << 16) \| (g << 8) \| b`           |
| 03     | `union`, `#define` macros with `do { } while(0)`, `int ended_down`     | Event listeners, `event.repeat`, `keydown`/`keyup` tracking        |
| 04     | C file/header split, `extern`, forward declarations, `= {0}` init     | ES module exports/imports, class constructor                        |
| 05     | `const char *` strings as bitmaps, 2D-in-1D indexing, `enum` values   | `const PIECES = [[...]]`, array indexing, `Object.freeze`          |
| 06     | `static` functions (file-scope privacy), `int *` output params         | Private methods, callback return values                             |
| 07     | `unsigned char` array as grid, `memset`, enum-as-colour-key           | `new Array(W*H).fill(0)`, switch-on-enum                           |
| 08     | Timer-based state (`float timer`), bottom-to-top row processing        | `setTimeout` simulation via `deltaTime`, array splice               |
| 09     | Integer bit-shift scoring `(1 << n) * 100`, modulo counters           | `Math.pow(2, n)`, `% 25 === 0` milestone checks                    |
| 10     | Bit-packed bitmap font (`bitmap[row] & (0x10 >> col)`), `snprintf`    | Canvas `fillText`, string formatting                                |
| 11     | `int16_t` PCM samples, phase accumulator, frequency slide             | `AudioContext`, `OscillatorNode`, `GainNode`                        |
| 12     | MIDI-to-Hz formula (`440 * pow(2, (n-69)/12)`), volume ramp           | `AudioContext.currentTime` scheduling, MIDI events                  |
| 13     | Multiple render passes, overlay with `draw_rect_blend`                 | Canvas `globalAlpha`, `z-index` layering                            |

---

## Final file structure

```
ai-llm-knowledge-dump/generated-courses/javidx9/tetris/
├── PLAN.md
├── README.md
├── PLAN-TRACKER.md
├── COURSE-BUILDER-IMPROVEMENTS.md   (written last)
└── course/
    ├── build-dev.sh
    ├── src/
    │   ├── utils/
    │   │   ├── backbuffer.h
    │   │   ├── draw-shapes.c
    │   │   ├── draw-shapes.h
    │   │   ├── draw-text.c
    │   │   ├── draw-text.h
    │   │   └── math.h
    │   ├── game.h
    │   ├── game.c
    │   ├── audio.c
    │   ├── platform.h
    │   ├── main_x11.c
    │   └── main_raylib.c
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives.md
        ├── 03-input.md
        ├── 04-game-structure.md
        ├── 05-tetromino-data.md
        ├── 06-tetromino-movement.md
        ├── 07-the-playfield.md
        ├── 08-line-clearing.md
        ├── 09-scoring-and-levels.md
        ├── 10-ui-sidebar-and-text.md
        ├── 11-audio-sound-effects.md
        ├── 12-audio-background-music.md
        └── 13-polish-ghost-and-game-over.md
```
