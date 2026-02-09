// @ts-check
/*
// Implementation of a 60fps for a game loop
const STEP = 1000 / 60;
const MAX_DELTA = 250;

/** @type {number|undefined} * /
let lastTime;
let accumulator = 0;

/** @param {DOMHighResTimeStamp} time * /
function animate(time) {
  if (lastTime === undefined) {
    lastTime = time;
    requestAnimationFrame(animate);
    return;
  }

  const elapsedTime = time - lastTime;
  lastTime = time;

  if (elapsedTime > 1000) {
    // Tab was likely inactive
    accumulator = 0;
    requestAnimationFrame(animate);
    return;
  }

  accumulator += Math.min(elapsedTime, MAX_DELTA);

  while (accumulator >= STEP) {
    update(STEP);
    accumulator -= STEP;
  }

  render();
  requestAnimationFrame(animate);
}

/** @param {number} dt  * /
function update(dt) {
  // Game logic using fixed dt (16.67ms)
}

function render() {
  // Draw
}

// accumulator += time - (lastTime ?? time);
*/

// OR

/*
function frame(time) {
  const delta = time - lastTime;
  lastTime = time;

  accumulator += Math.min(delta, MAX_DELTA);

  while (accumulator >= STEP) {
    update(STEP);
    accumulator -= STEP;
  }

  render(interpolation = accumulator / STEP);
  requestAnimationFrame(frame);
}

// 3. Adaptive FPS = skip renders, not sleeps
// To reduce load:

// Keep update rate fixed
// Render every Nth frame
// Or dynamically increase STEP
*/
// The above is more or less the correct way of doing a fixed timestep game loop.
// However, the following is just an implementation of a _low-level_ mirrored just to bridge/deepen my understanding of the concepts as a web dev in C.
// As JavaScript code: ⚠️ Fundamentally incompatible with the browser execution model.
// So, we do this as a learning exercise and as web programmer learning the  C programming language and low level

let is_running = true;
let target_seconds_per_frame = 1 / 60; // 16.67ms per frame

/** @param {number} timeInMS */
const sleep = async (timeInMS) =>
  await new Promise((resolve) => setTimeout(resolve, timeInMS));
/** @param {(rate: number) => void} callback */
function getRefreshRate(callback) {
  let frameCount = 0;
  let startTime = performance.now();

  function measureFrame() {
    frameCount++;
    let currentTime = performance.now();
    if (currentTime - startTime >= 1000) {
      const refreshRate = frameCount;
      console.log("Refresh Rate: " + refreshRate + " Hz");
      callback(refreshRate);
      return;
    }
    requestAnimationFrame(measureFrame);
  }

  requestAnimationFrame(measureFrame);
}

/** @type {number} */
const monitor_refresh_hz = await new Promise((res) =>
  getRefreshRate((rate) => res(rate))
);

const adaptive = {
  target_fps: monitor_refresh_hz,
  monitor_hz: monitor_refresh_hz,
  frames_sampled: 0,
  frames_missed: 0,
  sample_window: 300, // Evaluate every 5 seconds (at 60fps)
  miss_threshold: 0.05, // If >5% frames miss → reduce FPS
  recover_threshold: 0.01, // If <1% frames miss → try higher FPS
};

/**
 *
 * Note: Let's imagine that
 * - `performance.now()` is our `CLOCK_MONOTONIC`
 *  - So, we don't need to consider nanoseconds in consideration
 * - `sleep(ms)` is our `nanosleep()` or `usleep`
 *  - Which means we still be acting as the OS provides it and the OS scheduler doesn't guarantee wake-up precision anyway, so we still need to handle it
 */

while (is_running) {
  const start_time = performance.now();

  // Do some work - start
  // // === DO ALL OUR WORK ===
  // process_input();           // ~0.1ms
  // update_game_logic();       // ~1-2ms
  // render_frame();            // ~3-5ms
  // upload_to_gpu();           // ~1-2ms
  for (let i = 0; i < 1000; i++) {
    // ...
  }
  // Do some work - end

  const work_seconds = (performance.now() - start_time) / 1000;

  // REPORT (Did we exceed Section 2's budget?)
  if (work_seconds > target_seconds_per_frame) {
    console.log("MISSED FRAME!");
  }

  // Now we know: work_seconds ≈ 0.006 (6ms)
  // Target frame time: 0.01667 (16.67ms)
  // Time to sleep: 0.01667 - 0.006 = 0.01067 (10.67ms)

  // WOW, I didn't know it's valid in JavaScript
  // to call `await` on a `while` loop to be honest

  /*
  `const seconds_elapsed = target_seconds_per_frame - work_seconds;`
  `await sleep(time_to_sleep_seconds);`
┌─────────────────────────────────────────────────────────────┐
│           NAIVE SLEEP: WHAT ACTUALLY HAPPENS                │
│                                                             │
│  Frame 1: Work=6ms, Sleep request=10.67ms                   │
│           Actual wakeup: 12ms later (OS was busy)           │
│           Total frame: 18ms ❌ MISSED!                       │
│                                                             │
│  Frame 2: Work=5ms, Sleep request=11.67ms                   │
│           Actual wakeup: 11ms later (got lucky)             │
│           Total frame: 16ms ✓ OK                            │
│                                                             │
│  Frame 3: Work=7ms, Sleep request=9.67ms                    │
│           Actual wakeup: 15ms later (system hiccup)         │
│           Total frame: 22ms ❌ BADLY MISSED!                  │
│                                                             │
│  The OS scheduler doesn't guarantee wake-up precision!      │
└─────────────────────────────────────────────────────────────┘
  */

  let seconds_elapsed = work_seconds;
  if (seconds_elapsed < target_seconds_per_frame) {
    const rest_seconds = target_seconds_per_frame - 0.003;

    // Coarse sleep - using `sleep`
    while (seconds_elapsed < rest_seconds) {
      sleep(0.1);
      seconds_elapsed = (performance.now() - start_time) / 1000;
    }

    // Busy-wait sleep - CPU for precise timing after coarsening with `sleep` for an _almost_ `target_seconds_per_frame`
    while (seconds_elapsed < target_seconds_per_frame) {
      seconds_elapsed = (performance.now() - start_time) / 1000;
    }
  }

  const total_frame_time = (performance.now() - start_time) / 1000;
  const frame_time_ms = total_frame_time * 1000.0;
  const target_frame_time_ms = target_seconds_per_frame * 1000.0;

  // ─────────────────────────────────────────────────────────────
  // STEP 10: ADAPTIVE FPS EVALUATION
  // ─────────────────────────────────────────────────────────────
  // Only runs every 300 frames (~5 seconds)
  // Adjusts target FPS based on performance
  // ─────────────────────────────────────────────────────────────

  adaptive.frames_sampled++;
  if (frame_time_ms > target_frame_time_ms + 2) {
    adaptive.frames_missed++;
  }

  // Time to evaluate? (Every 300 frames = 5 seconds at 60fps)
  if (adaptive.frames_sampled >= adaptive.sample_window) {
    const miss_rate = adaptive.frames_missed / adaptive.frames_sampled;

    if (miss_rate > adaptive.miss_threshold) {
      const old_target = adaptive.target_fps;

      if (adaptive.target_fps === adaptive.monitor_hz) {
        // adaptive.target_fps =
        //     adaptive.monitor_hz === 120
        //         ? (adaptive.monitor_hz === 100
        //                ? (adaptive.monitor_hz === 60 ? 30 : 60)
        //                : 100)
        //         : 100;
        switch (adaptive.monitor_hz) {
          case 120:
            adaptive.target_fps = 100;
            break;
          case 100:
            adaptive.target_fps = 60;
            break;
          case 60:
            adaptive.target_fps = 30;
            break;
          default:
            adaptive.target_fps = 60;
            break;
        }
      } else if (adaptive.target_fps === 120) adaptive.target_fps = 100;
      else if (adaptive.target_fps === 100) adaptive.target_fps = 60;
      else if (adaptive.target_fps === 60) adaptive.target_fps = 30;
      else if (adaptive.target_fps === 30) adaptive.target_fps = 20;
      else if (adaptive.target_fps === 20) adaptive.target_fps = 15;

      if (adaptive.target_fps !== old_target) {
        target_seconds_per_frame = 1.0 / adaptive.target_fps;
        console.log(
          `\n⚠️  ADAPTIVE FPS: REDUCING TARGET ${old_target} → ${
            adaptive.target_fps
          } FPS (miss rate: ""${(miss_rate * 100.0).toFixed(1)}%)\n\n`
        );
      }
    }
    // Performance recovered? Try higher FPS
    else if (
      miss_rate < adaptive.recover_threshold &&
      adaptive.target_fps < adaptive.monitor_hz
    ) {
      const old_target = adaptive.target_fps;

      if (adaptive.target_fps === 15) adaptive.target_fps = 20;
      else if (adaptive.target_fps === 20) adaptive.target_fps = 30;
      else if (adaptive.target_fps === 30) adaptive.target_fps = 60;
      else if (adaptive.target_fps === 60) adaptive.target_fps = 100;
      else if (adaptive.target_fps === 100) adaptive.target_fps = 120;
      else if (adaptive.target_fps === 120)
        adaptive.target_fps = adaptive.monitor_hz;

      if (adaptive.target_fps !== old_target) {
        target_seconds_per_frame = 1.0 / adaptive.target_fps;
        console.log(
          `\n✅ ADAPTIVE FPS: INCREASING TARGET ${old_target} → ${old_target} FPS (miss rate: ""${(adaptive.target_fps,
          miss_rate * 100.0).toFixed(1)}%)\n\n`
        );
      }
    }

    // Reset sample window
    adaptive.frames_sampled = 0;
    adaptive.frames_missed = 0;
  }
}

/*
┌─────────────────────────────────────────────────────────────┐
│              WHY STEPS ARE IN THIS ORDER                    │
│                                                             │
│  INPUT before UPDATE:                                       │
│  └─► Game logic needs current input state                   │
│                                                             │
│  UPDATE before RENDER:                                      │
│  └─► We render what we just calculated                      │
│                                                             │
│  GPU UPLOAD before SLEEP:                                   │
│  └─► GPU works while we sleep (parallelism!)                │
│                                                             │
│  SLEEP before AUDIO:                                        │
│  └─► Fill audio buffer as close to playback as possible     │
│      (reduces latency and buffer underruns)                 │
│                                                             │
│  ADAPTIVE EVAL at END:                                      │
│  └─► We need the full frame time to evaluate performance    │
│                                                             │
└────────────────────────────────────────────────────────────
*/

export {};
