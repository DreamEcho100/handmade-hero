# Todos

| Extension                    | What to add                                                                                                | New concept                                                                                                                  |
| ---------------------------- | ---------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| **Ghost piece**              | Draw a faded preview of where the piece will land.                                                         | Raycast downward: keep calling `DoesPieceFit` with `y+1`, `y+2`, ... until it fails. The last valid Y is the ghost position. |
| **Wall kicks**               | When rotating fails, try shifting 1 cell left or right before giving up.                                   | Retry logic: `DoesPieceFit(piece, rot, x-1, y)` and `DoesPieceFit(piece, rot, x+1, y)` as fallbacks.                         |
| **Hard drop**                | Space bar instantly drops the piece to the lowest valid position.                                          | Same raycast as the ghost piece — find the drop Y, then lock immediately.                                                    |
| **7-bag randomizer**         | Shuffle all 7 pieces into a "bag," deal them out one at a time, then reshuffle.                            | Fisher-Yates shuffle. Guarantees you see each piece exactly once per 7.                                                      |
| **Hold piece**               | Press C to save the current piece; press again to swap it back.                                            | One more field in `GameState`: `int hold_piece`. Swap `current_piece` and `hold_piece` on C press.                           |
| **High score table**         | Write the top 10 scores to a file on disk.                                                                 | `fopen`, `fprintf`, `fscanf`. Good first file I/O exercise.                                                                  |
| **Sound effects**            | Play a sound when a piece locks, when lines clear, and on game over.                                       | Raylib: `PlaySound()`. X11: link against a sound library or use the PC speaker.                                              |
| **SDL3 backend**             | Write `src/main_sdl3.c` implementing the same 6 functions.                                                 | SDL3 runs on Linux, macOS, and Windows. The game logic compiles unchanged.                                                   |
| **DAS (Delayed Auto-Shift)** | When holding left/right, piece starts moving after a short delay, then moves repeatedly at a set interval. | Track how long the button has been held. After an initial delay (e.g. 200ms), start auto-moving every 50ms until released.   |

---

## Optional: Further Improvements

If you want to enhance the audio further in the future:

| Improvement                 | Description                                  | Difficulty |
| --------------------------- | -------------------------------------------- | ---------- |
| **Pulse width modulation**  | Vary square wave duty cycle for richer sound | Easy       |
| **Triangle/sawtooth waves** | More waveform variety                        | Easy       |
| **ADSR envelopes**          | Attack-Decay-Sustain-Release for SFX         | Medium     |
| **Vibrato**                 | Slight pitch oscillation on held notes       | Medium     |
| **Reverb/delay**            | Echo effects                                 | Hard       |
| **Multiple channels**       | Separate music tracks (bass, melody)         | Medium     |

---

## Migrating Tetris to Use Engine Audio (Future)

When you're ready to integrate Tetris with the engine:

```c:games/tetris/src/audio.c
// Before: Tetris-specific types in utils/audio.h
// After: Use engine helpers + game-specific extensions

#include "../../../engine/game/audio.h"
#include "../../../engine/game/audio-helpers.h"

// Tetris-specific sound IDs (game layer)
typedef enum {
  TETRIS_SOUND_NONE = 0,
  TETRIS_SOUND_MOVE,
  TETRIS_SOUND_ROTATE,
  // ... etc
} TetrisSoundID;

// Tetris audio state (uses engine types)
typedef struct {
  De100SoundPlayer sfx;
  De100MusicSequencer music;
  f32 master_volume;
  f32 sfx_volume;
  f32 music_volume;
} TetrisAudioState;

// game_get_audio_samples uses engine helpers internally
// Same pattern as handmade-hero but with Tetris-specific sounds
```
