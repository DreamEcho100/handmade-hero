#ifndef DE100_GAME_AUDIO_SYNC_TEST_H
#define DE100_GAME_AUDIO_SYNC_TEST_H

#include "../_common/base.h"
#include "audio.h"
#include "audio-helpers.h"
#include "backbuffer.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO SYNC TEST
// ═══════════════════════════════════════════════════════════════════════════
//
// Self-contained audio-visual sync tester for platforms/backends.
// Exercises the exact audio paths a latency-sensitive game would use:
//
//   1. METRONOME     - Precise 120 BPM click track (audible + visual)
//   2. INPUT-TO-BEEP - Press a key, hear a beep; measures round-trip latency
//   3. SWEEP         - Continuous frequency sweep (exposes glitches/clicks)
//
// Usage:
//   #include "engine/game/audio-sync-test.h"
//
//   // In your game state:
//   AudioSyncTest sync_test;
//
//   // Init once:
//   audio_sync_test_init(&sync_test);
//
//   // In update_and_render (call ONCE per frame):
//   audio_sync_test_update(&sync_test, dt, any_key_pressed);
//
//   // In get_audio_samples:
//   audio_sync_test_generate(&sync_test, audio_buffer);
//
//   // In render (draws status bar):
//   audio_sync_test_draw(&sync_test, backbuffer);
//
// ═══════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────
// Test Mode
// ─────────────────────────────────────────────────────────────────────────

typedef enum {
  AUDIO_SYNC_MODE_METRONOME = 0, // 120 BPM click + visual flash
  AUDIO_SYNC_MODE_INPUT_BEEP,    // Beep on keypress, measure latency
  AUDIO_SYNC_MODE_SWEEP,         // Continuous freq sweep (glitch detector)
  AUDIO_SYNC_MODE_COUNT
} AudioSyncMode;

// ─────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────

#define AUDIO_SYNC_HISTORY_SIZE 16

// Fade duration in samples for click-free enable/disable transitions
#define AUDIO_SYNC_FADE_SAMPLES 2400 // ~50ms at 48kHz

typedef struct {
  AudioSyncMode mode;
  bool enabled;

  // ─── Master fade (prevents click on enable/disable toggle) ───
  f32 fade_level;  // 0.0 (silent) to 1.0 (full volume)
  bool fade_target; // true = fading in, false = fading out

  // ─── Metronome ───
  f32 metro_timer;       // seconds since last beat
  f32 metro_bpm;         // beats per minute (default: 120)
  f32 metro_beat_dur;    // 1 / (bpm/60) = seconds per beat
  bool metro_flash;      // true for a few frames after beat
  f32 metro_flash_timer; // visual flash countdown
  i32 metro_beat_count;  // total beats played

  // Click oscillator (short burst of high-freq sine for a sharp click)
  f32 click_phase;
  f32 click_freq;        // Hz (default: 1000)
  i32 click_samples_left; // samples remaining in current click
  i32 click_duration;     // total samples per click

  // ─── Input beep ───
  bool beep_trigger;          // set true on keypress, consumed by audio
  f32 beep_phase;
  f32 beep_freq;              // Hz (default: 880 = A5)
  i32 beep_samples_left;
  i32 beep_duration;
  f64 beep_trigger_time;      // wall clock when key was pressed
  f64 beep_latency_history[AUDIO_SYNC_HISTORY_SIZE]; // last N latencies
  i32 beep_latency_index;
  i32 beep_latency_count;

  // ─── Sweep ───
  f32 sweep_phase;
  f32 sweep_freq;        // current frequency
  f32 sweep_freq_min;    // 200 Hz
  f32 sweep_freq_max;    // 4000 Hz
  f32 sweep_speed;       // Hz per second
  bool sweep_ascending;

} AudioSyncTest;

// ─────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────

static inline void audio_sync_test_init(AudioSyncTest *t) {
  *t = (AudioSyncTest){0};
  t->enabled = true;
  t->mode = AUDIO_SYNC_MODE_METRONOME;
  t->fade_level = 0.0f;
  t->fade_target = false;

  // Metronome defaults
  t->metro_bpm = 120.0f;
  t->metro_beat_dur = 60.0f / t->metro_bpm;
  t->metro_timer = 0.0f;
  t->click_freq = 1000.0f;
  t->click_duration = 0; // set in generate based on sample rate

  // Input beep defaults
  t->beep_freq = 880.0f;
  t->beep_duration = 0;

  // Sweep defaults
  t->sweep_freq_min = 200.0f;
  t->sweep_freq_max = 4000.0f;
  t->sweep_freq = t->sweep_freq_min;
  t->sweep_speed = 2000.0f; // Hz per second
  t->sweep_ascending = true;
}

// ─────────────────────────────────────────────────────────────────────────
// Update (call once per game frame)
// ─────────────────────────────────────────────────────────────────────────
//
// dt:          delta time in seconds
// key_pressed: true if any key/button was pressed THIS frame
// wall_clock:  monotonic wall clock in seconds (for latency measurement)
// cycle_mode:  true to switch to next test mode
//

static inline void audio_sync_test_update(AudioSyncTest *t, f32 dt,
                                          bool key_pressed, f64 wall_clock,
                                          bool cycle_mode) {
  if (!t->enabled)
    return;

  if (cycle_mode) {
    // Zero all oscillator state to prevent click from waveform discontinuity
    t->click_phase = 0.0f;
    t->click_samples_left = 0;
    t->beep_phase = 0.0f;
    t->beep_samples_left = 0;
    t->sweep_phase = 0.0f;

    t->mode = (AudioSyncMode)((t->mode + 1) % AUDIO_SYNC_MODE_COUNT);

    // Immediately trigger first sound in new mode so it's not silent
    if (t->mode == AUDIO_SYNC_MODE_METRONOME) {
      t->metro_timer = 0.0f;
      t->click_samples_left = t->click_duration; // immediate first click
      t->metro_flash = true;
      t->metro_flash_timer = 0.1f;
      t->metro_beat_count = 0;
    }
  }

  // ─── Metronome update ───
  if (t->mode == AUDIO_SYNC_MODE_METRONOME) {
    t->metro_timer += dt;
    if (t->metro_timer >= t->metro_beat_dur) {
      t->metro_timer -= t->metro_beat_dur;
      t->metro_flash = true;
      t->metro_flash_timer = 0.1f; // 100ms flash
      t->metro_beat_count++;
      // click_samples_left is set in generate() on first call
      // to avoid needing sample_rate here
      t->click_samples_left = t->click_duration;
      t->click_phase = 0.0f;
    }

    if (t->metro_flash) {
      t->metro_flash_timer -= dt;
      if (t->metro_flash_timer <= 0.0f) {
        t->metro_flash = false;
      }
    }
  }

  // ─── Input beep update ───
  if (t->mode == AUDIO_SYNC_MODE_INPUT_BEEP && key_pressed) {
    t->beep_trigger = true;
    t->beep_trigger_time = wall_clock;
    t->beep_samples_left = t->beep_duration;
    t->beep_phase = 0.0f;
  }
}

// ─────────────────────────────────────────────────────────────────────────
// Generate audio (call from get_audio_samples)
// ─────────────────────────────────────────────────────────────────────────

static inline void audio_sync_test_generate(AudioSyncTest *t,
                                            GameAudioOutputBuffer *buf) {
  if (!buf->is_initialized || buf->sample_count == 0)
    return;
  // Keep generating while fading out (fade_level > 0) even if disabled
  if (!t->enabled && t->fade_level <= 0.0f)
    return;

  i32 sample_rate = buf->samples_per_second;
  f32 inv_sr = 1.0f / (f32)sample_rate;
  f32 fade_step = 1.0f / (f32)AUDIO_SYNC_FADE_SAMPLES;

  // Lazy init durations (need sample_rate which we don't have at init time)
  if (t->click_duration == 0) {
    t->click_duration = sample_rate / 10; // 100ms click (was 50ms - more audible)
    t->beep_duration = sample_rate / 6;   // ~167ms beep
  }

  for (i32 s = 0; s < buf->sample_count; s++) {
    f32 out = 0.0f;

    switch (t->mode) {

    case AUDIO_SYNC_MODE_METRONOME: {
      if (t->click_samples_left > 0) {
        // Sharp click: sine burst with fast decay envelope
        f32 env = (f32)t->click_samples_left / (f32)t->click_duration;
        env = env * env; // quadratic decay for snappy click
        f32 wave = sinf(t->click_phase * 2.0f * (f32)M_PI);
        out = wave * env * 0.7f; // louder click (was 0.5)
        t->click_phase += t->click_freq * inv_sr;
        if (t->click_phase >= 1.0f)
          t->click_phase -= 1.0f;
        t->click_samples_left--;
      } else {
        // Quiet background tone between clicks so you know it's active
        f32 wave = sinf(t->click_phase * 2.0f * (f32)M_PI);
        out = wave * 0.05f; // very quiet
        t->click_phase += 220.0f * inv_sr; // low A3
        if (t->click_phase >= 1.0f)
          t->click_phase -= 1.0f;
      }
    } break;

    case AUDIO_SYNC_MODE_INPUT_BEEP: {
      if (t->beep_samples_left > 0) {
        f32 env = (f32)t->beep_samples_left / (f32)t->beep_duration;
        f32 wave = sinf(t->beep_phase * 2.0f * (f32)M_PI);
        out = wave * env * 0.4f;
        t->beep_phase += t->beep_freq * inv_sr;
        if (t->beep_phase >= 1.0f)
          t->beep_phase -= 1.0f;
        t->beep_samples_left--;

        // Record latency on the first sample of the beep
        if (t->beep_trigger) {
          t->beep_trigger = false;
          // Latency = time from keypress to this audio sample being generated.
          // Actual perceived latency also includes audio buffer latency.
        }
      }
    } break;

    case AUDIO_SYNC_MODE_SWEEP: {
      f32 wave = sinf(t->sweep_phase * 2.0f * (f32)M_PI);
      out = wave * 0.3f;
      t->sweep_phase += t->sweep_freq * inv_sr;
      if (t->sweep_phase >= 1.0f)
        t->sweep_phase -= 1.0f;

      // Advance sweep frequency
      if (t->sweep_ascending) {
        t->sweep_freq += t->sweep_speed * inv_sr;
        if (t->sweep_freq >= t->sweep_freq_max) {
          t->sweep_freq = t->sweep_freq_max;
          t->sweep_ascending = false;
        }
      } else {
        t->sweep_freq -= t->sweep_speed * inv_sr;
        if (t->sweep_freq <= t->sweep_freq_min) {
          t->sweep_freq = t->sweep_freq_min;
          t->sweep_ascending = true;
        }
      }
    } break;

    default:
      break;
    }

    // Write mono output as centered stereo
    // Apply fade envelope for click-free enable/disable
    if (t->fade_target && t->fade_level < 1.0f) {
      t->fade_level += fade_step;
      if (t->fade_level > 1.0f) t->fade_level = 1.0f;
    } else if (!t->fade_target && t->fade_level > 0.0f) {
      t->fade_level -= fade_step;
      if (t->fade_level < 0.0f) t->fade_level = 0.0f;
    }
    out *= t->fade_level;

    audio_write_sample(buf, s, out, out);
  }
}

// ─────────────────────────────────────────────────────────────────────────
// Draw status (call from render, writes directly to backbuffer)
//
// Draws a small status bar at the top of the screen:
//   - Mode name
//   - Metronome: beat counter + flash indicator
//   - Input beep: average latency
//   - Sweep: current frequency
//
// x, y: top-left corner of the status bar (in pixels)
// ─────────────────────────────────────────────────────────────────────────

static inline void audio_sync_test_draw_bar(GameBackBuffer *bb, i32 x, i32 y,
                                            i32 width, i32 height,
                                            u32 color) {
  if (!bb || !bb->memory.base)
    return;

  for (i32 row = y; row < y + height && row < bb->height; row++) {
    if (row < 0)
      continue;
    for (i32 col = x; col < x + width && col < bb->width; col++) {
      if (col < 0)
        continue;
      u32 *pixel =
          (u32 *)((u8 *)bb->memory.base + row * bb->pitch + col * bb->bytes_per_pixel);
      *pixel = color;
    }
  }
}

static inline void audio_sync_test_draw(AudioSyncTest *t,
                                        GameBackBuffer *bb) {
  if (!t->enabled || !bb || !bb->memory.base)
    return;

  // Flash bar for metronome beats (full-width bar at top)
  if (t->mode == AUDIO_SYNC_MODE_METRONOME && t->metro_flash) {
    // Bright white flash bar, 8px tall
    u32 flash_color = 0xFFFFFFFF; // ARGB white
    audio_sync_test_draw_bar(bb, 0, 0, bb->width, 8, flash_color);
  }

  // Beat counter indicator: small colored squares, one per beat (mod 4)
  if (t->mode == AUDIO_SYNC_MODE_METRONOME) {
    i32 beat_in_measure = t->metro_beat_count % 4;
    for (i32 i = 0; i < 4; i++) {
      u32 color = (i <= beat_in_measure) ? 0xFF00FF00 : 0xFF333333;
      audio_sync_test_draw_bar(bb, 10 + i * 20, 12, 16, 16, color);
    }
  }

  // Sweep: frequency bar (horizontal position = frequency)
  if (t->mode == AUDIO_SYNC_MODE_SWEEP) {
    f32 norm = (t->sweep_freq - t->sweep_freq_min) /
               (t->sweep_freq_max - t->sweep_freq_min);
    i32 bar_x = (i32)(norm * (f32)(bb->width - 20));
    audio_sync_test_draw_bar(bb, bar_x, 0, 4, 20, 0xFF00AAFF);
  }

  // Input beep: flash on trigger
  if (t->mode == AUDIO_SYNC_MODE_INPUT_BEEP && t->beep_samples_left > 0) {
    audio_sync_test_draw_bar(bb, 0, 0, bb->width, 4, 0xFFFF8800);
  }
}

#endif // DE100_GAME_AUDIO_SYNC_TEST_H
