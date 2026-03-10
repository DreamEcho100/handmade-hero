/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Defines every audio type the Snake game uses.
 *
 * The platform (main_x11.c / main_raylib.c) fills an AudioOutputBuffer each
 * frame.  The game writes raw PCM samples into it through
 * game_get_audio_samples().
 *
 * This is a simplified version of the Tetris audio system — no music
 * sequencer, no ToneGenerator.  Just sound effects: food-eaten (spatially
 * panned) and game-over (centred).  Compare with the Tetris utils/audio.h to
 * see how the same foundation scales up to a full music system.
 *
 * JS background note: if you've used the Web Audio API, think of this file as
 * implementing a small software synth that replaces AudioContext,
 * OscillatorNode, GainNode, and StereoPannerNode — except instead of the
 * browser managing these for you, we write every sample ourselves in a tight
 * loop.
 *
 * COURSE NOTE: The reference source has no audio at all.  This system is
 * added as a course enhancement.  See Lesson 11 and
 * COURSE-BUILDER-IMPROVEMENTS.md.
 */

#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

#include <stdbool.h> /* (reserved for future use; kept for student experimentation) */
#include <stdint.h> /* int16_t, int64_t */

/* ══════ AudioOutputBuffer ══════════════════════════════════════════════════
 *
 * WHO FILLS WHAT
 * ──────────────
 *   Platform sets at init:    samples_buffer, samples_per_second,
 *                              max_sample_count, is_initialized
 *   Platform sets per frame:  sample_count  (≤ max_sample_count)
 *   Game writes into:         samples_buffer  (game_get_audio_samples)
 *
 * MEMORY LAYOUT — Interleaved Stereo
 * ────────────────────────────────────
 *   Index:  0     1     2     3     4     5  …
 *   Value:  L[0]  R[0]  L[1]  R[1]  L[2]  R[2] …
 *
 *   To write sample pair N:
 *     samples_buffer[N * 2 + 0] = left_sample;
 *     samples_buffer[N * 2 + 1] = right_sample;
 *
 * JS equivalent: the Float32Array from AudioContext.createScriptProcessor()
 * but as raw int16 instead of float32.
 *
 * EVOLUTION PATH (for future multi-backend support)
 * ────────────────────────────────────────────────────────────────────────
 * This design hard-codes int16_t (I16_LE) which works on Linux ALSA + Raylib.
 * CoreAudio (macOS) and WASAPI (Windows) natively produce float32 streams.
 * The engine-level solution adds:
 *   void          *samples_buffer;   -- opaque, format-agnostic pointer
 *   AudioSampleFormat format;        -- I16_LE, F32, etc.
 * ... plus an audio_write_sample() helper that dispatches on format.
 * Keep that upgrade path in mind; do NOT sprinkle int16_t casts elsewhere.
 */
typedef struct {
  int16_t *samples_buffer; /* Interleaved stereo PCM; platform allocates. */
  /* I16 hard-coded for simplicity — see EVOLUTION PATH */
  /* above when adding CoreAudio/WASAPI (F32) backends. */
  int samples_per_second; /* 44100 or 48000 — fixed at init */
  int sample_count; /* Stereo pairs to write THIS call (≤ max_sample_count) */
  int max_sample_count; /* Buffer capacity in stereo pairs.                   */
                        /* Allocated as: (samples_per_second/60)*8 pairs.     */
  /* game_get_audio_samples() MUST NOT exceed this.      */
  bool is_initialized; /* Set by platform after successful audio device init. */
                       /* Treat as an EVERY-FRAME edge, not a startup flag:   */
                       /* audio devices can disappear mid-session on some OS. */
} AudioOutputBuffer;

#endif // UTILS_AUDIO_H
