# 🎯 FPS Implementation Guide

> A comprehensive, beginner-friendly guide to understanding and implementing Frame Rate (FPS) management in game engines.

## How This Guide is Organized

This guide follows a **logical learning path** where each section builds on the previous one:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    LEARNING PROGRESSION                                 │
│                                                                         │
│   FOUNDATION (Understanding the Problem)                                │
│   ├── Section 1: What is FPS? ──────────► Why do we care about timing? │
│   └── Section 2: Frame Budget ──────────► What are our constraints?    │
│                                                                         │
│   TOOLS (How to Measure)                                                │
│   └── Section 3: Measuring Time ────────► How do we know elapsed time? │
│                                                                         │
│   ARCHITECTURE (The Big Picture)                                        │
│   └── Section 4: Game Loop ─────────────► Where does timing fit in?    │
│                                                                         │
│   IMPLEMENTATION (Solving the Problem)                                  │
│   ├── Section 5: Sleep Strategies ──────► How to wait precisely?       │
│   └── Section 6: Adaptive FPS ──────────► What if we can't keep up?    │
│                                                                         │
│   REFERENCE (Looking Things Up)                                         │
│   ├── Section 7: Data Structures ───────► What data do we track?       │
│   └── Section 8: Key Functions ─────────► What functions do we use?    │
│                                                                         │
│   MASTERY (Avoiding Mistakes & Practice)                                │
│   ├── Section 9: Common Pitfalls ───────► What mistakes to avoid?      │
│   ├── Section 10: Exercises ────────────► Hands-on practice            │
│   └── Section 11: Resources ────────────► Further learning             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Reading Tip**: Read sections 1-6 in order. Sections 7-8 are references you can revisit. Sections 9-11 help you solidify your understanding.

## Table of Contents

1. [What is FPS and Why Does It Matter?](#1-what-is-fps-and-why-does-it-matter)
2. [The Frame Time Budget](#2-the-frame-time-budget)
3. [Measuring Time](#3-measuring-time)
4. [The Game Loop Structure](#4-the-game-loop-structure)
5. [Sleep Strategies](#5-sleep-strategies)
6. [Adaptive FPS](#6-adaptive-fps)
7. [Key Data Structures](#7-key-data-structures)
8. [Key Functions](#8-key-functions)
9. [Common Pitfalls](#9-common-pitfalls)
10. [Practical Exercises](#10-practical-exercises)
11. [Resources](#11-resources)

---

# PART 1: FOUNDATION

> _Before we can solve the timing problem, we need to understand what the problem actually is._

---

## 1. What is FPS and Why Does It Matter?

### Definition

**FPS (Frames Per Second)** is the number of times your game updates and renders its display each second.

```
60 FPS = 60 frames per second
       = game updates 60 times every second
       = each frame takes 16.67 milliseconds
```

### Why It Matters

| FPS | Frame Time | Experience                           |
| --- | ---------- | ------------------------------------ |
| 15  | 66.67ms    | Slideshow-like, uncomfortable        |
| 30  | 33.33ms    | Playable, but noticeable delay       |
| 60  | 16.67ms    | Smooth, responsive gameplay          |
| 120 | 8.33ms     | Silky smooth (high refresh monitors) |

### The V-Sync Connection

Your monitor refreshes at a fixed rate (typically 60Hz). If your game produces frames faster or slower than this rate, you get **visual artifacts**:

```
┌─────────────────────────────────────────────────────────┐
│  MONITOR REFRESH (60Hz = every 16.67ms)                 │
│                                                         │
│  Frame 1    Frame 2    Frame 3    Frame 4               │
│  ────────   ────────   ────────   ────────              │
│  |      |   |      |   |      |   |      |              │
│  16.67ms    16.67ms    16.67ms    16.67ms               │
│                                                         │
│  If your game runs FASTER (produces frame in 10ms):     │
│  → GPU waits for next refresh (wasted time)             │
│  → Or "tearing" if V-Sync off                           │
│                                                         │
│  If your game runs SLOWER (produces frame in 25ms):     │
│  → Missed frame! Same image shown twice                 │
│  → Player sees stutter                                  │
└─────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

Now we understand that:

- Games run in a loop, producing frames
- Each frame has a **time budget** (16.67ms for 60 FPS)
- Missing this budget causes visible problems

But what exactly happens **within** those 16.67ms? That's our **frame time budget**, which we'll explore next.

---

## 2. The Frame Time Budget

### 🔗 Why This Section Matters

In Section 1, we learned that 60 FPS means **16.67ms per frame**. But what does that actually mean for our code? This section breaks down what work must fit into that time window and introduces the concept of **sleeping** to fill remaining time.

### The 16.67ms Target

At 60 FPS, you have exactly **16.67 milliseconds** to:

```
┌──────────────────── 16.67ms Frame Budget ────────────────────┐
│                                                              │
│  ┌──────────────────────── WORK ────────────────────────┐    │
│  │                                                      │    │
│  │  Input Processing    ~0.1ms                          │    │
│  │  Game Logic Update   ~1-2ms                          │    │
│  │  Rendering (CPU)     ~3-5ms                          │    │
│  │  GPU Upload          ~1-2ms                          │    │
│  │  Audio Fill          ~0.5ms                          │    │
│  │                      ───────                         │    │
│  │  Total Work:         ~5-10ms                         │    │
│  └──────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌─────────── SLEEP (remaining time) ───────────┐            │
│  │                                              │            │
│  │  Target: 16.67ms - work_time                 │            │
│  │  Example: 16.67ms - 6ms = 10.67ms sleep      │            │
│  └──────────────────────────────────────────────┘            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Why Sleep?

Without sleep, your game would:

1. **Waste CPU cycles** spinning in a tight loop
2. **Generate heat** and drain batteries
3. **Starve other processes** of CPU time
4. **Potentially run faster than the monitor** (if work < 16.67ms)

### The Core Problem We Need to Solve

Here's the fundamental challenge:

```
┌─────────────────────────────────────────────────────────────┐
│                    THE TIMING PROBLEM                       │
│                                                             │
│  WHAT WE KNOW:                                              │
│  • Work takes variable time (5-10ms typically)              │
│  • We need EXACTLY 16.67ms per frame                        │
│  • Sleep functions are IMPRECISE                            │
│                                                             │
│  WHAT WE NEED:                                              │
│  1. A way to MEASURE elapsed time accurately                │
│  2. A way to WAIT for the right amount of time              │
│  3. A way to ADAPT if we can't keep up                      │
│                                                             │
│  THE SOLUTION (covered in upcoming sections):               │
│  • Section 3: How to measure time → clock_gettime()         │
│  • Section 5: How to wait → Two-phase sleep                 │
│  • Section 6: How to adapt → Adaptive FPS                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

We now understand:

- We have a 16.67ms budget per frame
- Work takes some portion of that time
- We need to **sleep** for the remaining time
- But how do we know how much time has passed?

To implement precise timing, we first need to **measure time accurately**. That's what Section 3 covers.

---

# PART 2: TOOLS

> _Now that we understand the problem, let's learn about the tools we'll use to solve it._

---

## 3. Measuring Time

### 🔗 Why This Section Matters

In Section 2, we established that we need to:

1. Measure how long our work takes
2. Calculate how long to sleep
3. Verify we hit our target frame time

All of these require **accurate time measurement**. This section introduces the clock sources available and explains which one to use and why.

### Clock Sources

Different clocks have different purposes:

| Clock             | Resolution   | Use Case                              | Why/Why Not for Frame Timing         |
| ----------------- | ------------ | ------------------------------------- | ------------------------------------ |
| `CLOCK_MONOTONIC` | ~nanoseconds | Frame timing (never jumps backward)   | ✅ **Best choice** - stable, precise |
| `CLOCK_REALTIME`  | ~nanoseconds | Wall clock time (can change with NTP) | ❌ Can jump forward/backward         |
| `__rdtsc()`       | CPU cycles   | Micro-profiling (not time-based)      | ❌ Varies with CPU frequency         |

### Understanding the Clock Choice

**Why not `CLOCK_REALTIME`?**

```c
// Imagine this scenario:
// Frame starts at 12:00:00.000 (REALTIME)
// NTP sync happens, clock jumps to 12:00:05.000
// You calculate: elapsed = 5 seconds!
// Your game thinks it missed 300 frames!
```

**Why not `__rdtsc()` (CPU cycles)?**

```c
// CPU cycles ≠ wall clock time
// Modern CPUs change frequency (power saving, turbo boost)
// 1 million cycles might be:
//   - 0.5ms at 2GHz (power saving)
//   - 0.25ms at 4GHz (turbo boost)
// Useless for consistent timing!
```

**Why `CLOCK_MONOTONIC` is perfect:**

- "Monotonic" means "always increasing" - never jumps backward
- Not affected by NTP or user changing system time
- Nanosecond precision (1 billionth of a second)

### Our Implementation

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 90-96

static inline real64 get_wall_clock(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (real64)ts.tv_sec + (real64)ts.tv_nsec / 1000000000.0;
}
```

**Why `CLOCK_MONOTONIC`?**

- ✅ **Monotonic**: Never goes backward (unlike `REALTIME` which can jump during NTP sync)
- ✅ **High resolution**: Nanosecond precision
- ✅ **Not affected by system time changes**: Perfect for measuring durations

### Calculating Elapsed Time

```c
// At frame start:
struct timespec frame_start_time;
clock_gettime(CLOCK_MONOTONIC, &frame_start_time);

// ... do work ...

// At any point, calculate elapsed:
struct timespec current_time;
clock_gettime(CLOCK_MONOTONIC, &current_time);

real32 elapsed_seconds =
    (current_time.tv_sec - frame_start_time.tv_sec) +
    (current_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;
```

### Practical Example: Measuring Work Time

Let's see how this applies to our frame timing:

```c
// At the START of our frame:
struct timespec frame_start;
clock_gettime(CLOCK_MONOTONIC, &frame_start);

// === DO ALL OUR WORK ===
process_input();           // ~0.1ms
update_game_logic();       // ~1-2ms
render_frame();            // ~3-5ms
upload_to_gpu();           // ~1-2ms

// MEASURE how long work took:
struct timespec work_end;
clock_gettime(CLOCK_MONOTONIC, &work_end);

real32 work_seconds =
    (work_end.tv_sec - frame_start.tv_sec) +
    (work_end.tv_nsec - frame_start.tv_nsec) / 1000000000.0f;

// Now we know: work_seconds ≈ 0.006 (6ms)
// Target frame time: 0.01667 (16.67ms)
// Time to sleep: 0.01667 - 0.006 = 0.01067 (10.67ms)
```

### 🔗 Connection to Next Section

Now we have the ability to:

- ✅ Get the current time with nanosecond precision
- ✅ Calculate how long operations take
- ✅ Determine how long we should sleep

But **where** in our code do we use this? We need to understand the overall structure of a game loop to see where timing fits in. That's Section 4.

---

# PART 3: ARCHITECTURE

> _Understanding where timing fits into the bigger picture of your game._

---

## 4. The Game Loop Structure

### 🔗 Why This Section Matters

We've learned:

- **What** we're trying to achieve (Section 1: consistent FPS)
- **Why** we need timing (Section 2: frame budget)
- **How** to measure time (Section 3: `clock_gettime()`)

Now we need to see **where** all of this fits together. The game loop is the beating heart of your game—every frame flows through it.

### The Relationship Between Sections

```
┌─────────────────────────────────────────────────────────────┐
│           HOW PREVIOUS SECTIONS FIT INTO THE LOOP           │
│                                                             │
│   Section 1 (FPS)           Section 2 (Budget)              │
│   "60 frames/sec"    →      "16.67ms per frame"             │
│         │                          │                        │
│         └──────────┬───────────────┘                        │
│                    │                                        │
│                    ▼                                        │
│   Section 3 (Measuring)                                     │
│   "Use clock_gettime to track time"                         │
│         │                                                   │
│         ▼                                                   │
│   ┌─────────────────────────────────────────────────────┐   │
│   │              THE GAME LOOP (This Section)           │   │
│   │                                                     │   │
│   │   while (running) {                                 │   │
│   │       start_time = clock_gettime();  ← Section 3    │   │
│   │       do_work();                                    │   │
│   │       work_time = clock_gettime() - start_time;     │   │
│   │       sleep(target - work_time);     ← Section 5    │   │
│   │   }                                                 │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────┐
│                     GAME LOOP                               │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                                                     │   │
│   │   ┌──────────────┐                                  │   │
│   │   │ 1. TIMESTAMP │  Record start time               │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          ▼                                          │   │
│   │   ┌──────────────┐                                  │   │
│   │   │  2. INPUT    │  Process keyboard/mouse/gamepad  │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          ▼                                          │   │
│   │   ┌──────────────┐                                  │   │
│   │   │  3. UPDATE   │  Run game logic                  │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          ▼                                          │   │
│   │   ┌──────────────┐                                  │   │
│   │   │  4. RENDER   │  Draw to backbuffer              │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          ▼                                          │   │
│   │   ┌──────────────┐                                  │   │
│   │   │  5. DISPLAY  │  Upload to GPU, swap buffers     │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          ▼                                          │   │
│   │   ┌──────────────┐                                  │   │
│   │   │  6. SLEEP    │  Wait for target frame time      │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          ▼                                          │   │
│   │   ┌──────────────┐                                  │   │
│   │   │  7. AUDIO    │  Fill audio buffer               │   │
│   │   └──────┬───────┘                                  │   │
│   │          │                                          │   │
│   │          └───────────────────────────► Loop back    │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Our 11-Step Implementation

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 487-723

while (is_game_running) {
    // STEP 1:  Timestamp frame start         ← Uses Section 3 (clock_gettime)
    // STEP 2:  Prepare input (swap buffers, copy button states)
    // STEP 3:  Process X11 events (keyboard, window)
    // STEP 4:  Poll joystick
    // STEP 5:  Update game + render (CPU side)
    // STEP 6:  Measure work time             ← Uses Section 3 (clock_gettime)
    // STEP 7:  Sleep (two-phase)             ← Uses Section 5 (sleep strategies)
    // STEP 8:  Fill audio buffer
    // STEP 9:  Measure total frame time      ← Uses Section 3 (clock_gettime)
    // STEP 10: Report missed frames          ← Uses Section 2 (frame budget)
    // STEP 11: Adaptive FPS evaluation       ← Uses Section 6 (adaptive FPS)
    // STEP 12: Swap input buffers
}
```

### Detailed Step-by-Step Breakdown

Let's trace through one complete frame to see how everything connects:

```c
// ═══════════════════════════════════════════════════════════════
// FRAME N BEGINS
// ═══════════════════════════════════════════════════════════════

// STEP 1: TIMESTAMP (Section 3 in action)
// We need this to measure everything else
struct timespec frame_start_time;
clock_gettime(CLOCK_MONOTONIC, &frame_start_time);  // Time: 0.000ms

// STEPS 2-5: DO THE ACTUAL WORK
// This is what takes time (Section 2's "work" portion)
prepare_input();                    // ~0.05ms
process_events();                   // ~0.1ms
poll_joystick();                    // ~0.1ms
game_update_and_render();           // ~4-5ms
upload_to_gpu();                    // ~1-2ms
// Total work: ~5-7ms

// STEP 6: MEASURE WORK TIME (Section 3 again)
struct timespec work_end_time;
clock_gettime(CLOCK_MONOTONIC, &work_end_time);     // Time: ~6.000ms
real32 work_seconds = /* calculate difference */;   // Result: 0.006

// STEP 7: SLEEP (Section 5's two-phase sleep)
// We have 16.67ms budget, used 6ms, need to wait 10.67ms
// This is THE CRITICAL PART - covered in detail in Section 5

// STEP 8: AUDIO
// Fill audio buffer (done after sleep to be closer to playback)

// STEP 9: FINAL MEASUREMENT
clock_gettime(CLOCK_MONOTONIC, &frame_end_time);    // Time: ~16.67ms
// Did we hit our target? (Section 2's frame budget)

// STEP 10: REPORT (Did we exceed Section 2's budget?)
if (frame_time > target_frame_time) {
    printf("MISSED FRAME!");
}

// STEP 11: ADAPTIVE (Section 6)
// Track miss rate, adjust target FPS if needed

// ═══════════════════════════════════════════════════════════════
// FRAME N ENDS, FRAME N+1 BEGINS
// ═══════════════════════════════════════════════════════════════
```

### Why This Order Matters

The step order isn't arbitrary. Here's why:

```
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
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

Now we understand:

- The overall structure of a game loop
- Where timing measurements happen (Steps 1, 6, 9)
- Where sleep happens (Step 7)
- Where adaptive logic runs (Step 11)

But we glossed over **Step 7: Sleep**. How do we actually sleep for the right amount of time? Simple `sleep()` calls are notoriously imprecise. Section 5 solves this problem with the **Two-Phase Sleep Strategy**.

---

# PART 4: IMPLEMENTATION

> _Now we implement the actual solutions to the timing problem._

---

## 5. Sleep Strategies

### 🔗 Why This Section Matters

In Section 4, we saw that Step 7 is where we wait for the remaining frame time. But there's a problem: **operating systems don't wake you up precisely when you ask.**

This section solves the most challenging part of frame timing: waiting accurately.

### The Problem with Simple Sleep

```c
// NAIVE APPROACH (DON'T DO THIS!)
real32 remaining = target_frame_time - work_time;
usleep(remaining * 1000000);  // Sleep for remaining microseconds
```

**Why this fails:**

- `usleep()` is **not precise** - OS might wake you up late
- Typical jitter: 1-10ms variance
- Result: Inconsistent frame times

**Let's see the problem in action:**

```
┌─────────────────────────────────────────────────────────────┐
│           NAIVE SLEEP: WHAT ACTUALLY HAPPENS                │
│                                                             │
│  Frame 1: Work=6ms, Sleep request=10.67ms                   │
│           Actual wakeup: 12ms later (OS was busy)           │
│           Total frame: 18ms ❌ MISSED!                      │
│                                                             │
│  Frame 2: Work=5ms, Sleep request=11.67ms                   │
│           Actual wakeup: 11ms later (got lucky)             │
│           Total frame: 16ms ✓ OK                            │
│                                                             │
│  Frame 3: Work=7ms, Sleep request=9.67ms                    │
│           Actual wakeup: 15ms later (system hiccup)         │
│           Total frame: 22ms ❌ BADLY MISSED!                │
│                                                             │
│  The OS scheduler doesn't guarantee wake-up precision!      │
└─────────────────────────────────────────────────────────────┘
```

### The Insight: Trade CPU for Precision

The key insight is:

- **Sleeping** saves CPU but is imprecise
- **Spinning** (busy-waiting) wastes CPU but is precise
- **Solution**: Sleep for most of the time, spin for the final bit

### Two-Phase Sleep (Casey Muratori's Day 18 Pattern)

```
┌─────────────────────────────────────────────────────────────┐
│              TWO-PHASE SLEEP STRATEGY                       │
│                                                             │
│  Target: 16.67ms                                            │
│  Work:   6.00ms                                             │
│  Need to wait: 10.67ms                                      │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ PHASE 1: COARSE SLEEP (Leave 3ms margin)           │    │
│  │                                                     │    │
│  │   Sleep target: 16.67ms - 3ms = 13.67ms            │    │
│  │   Already worked: 6.00ms                           │    │
│  │   Sleep until: 13.67ms - 6.00ms = 7.67ms           │    │
│  │                                                     │    │
│  │   Loop: sleep 1ms, check time, repeat              │    │
│  │                                                     │    │
│  │   6ms ──[1ms]── 7ms ──[1ms]── 8ms ... 13ms         │    │
│  │                                                     │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ PHASE 2: SPIN-WAIT (Precise timing)                │    │
│  │                                                     │    │
│  │   Tight loop checking clock until 16.67ms          │    │
│  │                                                     │    │
│  │   while (elapsed < target) {                       │    │
│  │       clock_gettime(...);  // ~50-100 nanoseconds  │    │
│  │       // No sleep! Actively checking               │    │
│  │   }                                                │    │
│  │                                                     │    │
│  │   13ms ─────spin────── 16.67ms ✓                   │    │
│  │                                                     │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Implementation

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 541-575

// Phase 1: Coarse sleep (1ms chunks, leave 3ms margin)
if (seconds_elapsed < target_seconds_per_frame) {
    real32 test_seconds = target_seconds_per_frame - 0.003f;  // 3ms margin

    while (seconds_elapsed < test_seconds) {
        struct timespec sleep_spec = {0, 1000000};  // 1ms
        nanosleep(&sleep_spec, NULL);

        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        seconds_elapsed = /* calculate elapsed */;
    }

    // Phase 2: Spin-wait for precise timing
    while (seconds_elapsed < target_seconds_per_frame) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        seconds_elapsed = /* calculate elapsed */;
    }
}
```

### Why 3ms Margin?

The 3ms margin isn't arbitrary:

```
┌─────────────────────────────────────────────────────────────┐
│              WHY 3MS MARGIN?                                │
│                                                             │
│  OS Sleep Jitter Analysis:                                  │
│  ─────────────────────────                                  │
│  • Request 1ms sleep → actual: 1-3ms (typical Linux)        │
│  • Request 1ms sleep → actual: 1-15ms (worst case, loaded)  │
│                                                             │
│  If we sleep until (target - 3ms):                          │
│  • Even if OS wakes us 3ms late, we haven't overshot        │
│  • We then spin for 0-6ms (worst case)                      │
│  • ~3ms of spinning is negligible CPU usage                 │
│                                                             │
│  If we used smaller margin (1ms):                           │
│  • OS jitter could overshoot target                         │
│  • Frame would be late, nothing we can do                   │
│                                                             │
│  If we used larger margin (10ms):                           │
│  • We'd spin for 10ms every frame                           │
│  • Wastes CPU, drains battery                               │
│                                                             │
│  3ms is the sweet spot: safe but not wasteful               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Complete Sleep Strategy Walkthrough

Let's trace through a real example:

```
Frame work time: 6.0ms
Target frame time: 16.67ms
Time remaining: 10.67ms

PHASE 1 (Coarse Sleep):
├── Sleep target: 16.67ms - 3ms = 13.67ms
├── Current time: 6.0ms
├── Sleep until: 13.67ms
│
├── Iteration 1: sleep(1ms), wake at 7.1ms, check: 7.1 < 13.67? YES, continue
├── Iteration 2: sleep(1ms), wake at 8.2ms, check: 8.2 < 13.67? YES, continue
├── Iteration 3: sleep(1ms), wake at 9.1ms, check: 9.1 < 13.67? YES, continue
├── Iteration 4: sleep(1ms), wake at 10.3ms, check: 10.3 < 13.67? YES, continue
├── Iteration 5: sleep(1ms), wake at 11.2ms, check: 11.2 < 13.67? YES, continue
├── Iteration 6: sleep(1ms), wake at 12.4ms, check: 12.4 < 13.67? YES, continue
├── Iteration 7: sleep(1ms), wake at 13.5ms, check: 13.5 < 13.67? YES, continue
└── Iteration 8: sleep(1ms), wake at 14.7ms, check: 14.7 < 13.67? NO, exit loop

PHASE 2 (Spin-Wait):
├── Current time: 14.7ms
├── Target: 16.67ms
├── Need to wait: 1.97ms more
│
├── Spin iteration 1: check clock → 14.71ms < 16.67? YES
├── Spin iteration 2: check clock → 14.72ms < 16.67? YES
├── ... (thousands of iterations, ~50ns each)
├── Spin iteration N: check clock → 16.67ms < 16.67? NO, exit!
└──
Final frame time: 16.67ms ✓ PERFECT!
```

### 🔗 Connection to Next Section

The two-phase sleep strategy works great **when your hardware can keep up**. But what happens when:

- Your game logic takes 20ms (exceeds 16.67ms budget)?
- You're running on slow hardware?
- Background processes steal CPU time?

In these cases, you'll miss frames no matter how precise your sleep is. Section 6 introduces **Adaptive FPS** - a system that automatically adjusts the target frame rate based on actual performance.

---

## 6. Adaptive FPS

### 🔗 Why This Section Matters

Section 5 gave us precise timing control. But precision doesn't help if the work itself takes longer than our budget. Consider:

```
┌─────────────────────────────────────────────────────────────┐
│           WHEN SLEEP STRATEGY ISN'T ENOUGH                  │
│                                                             │
│  Scenario: Slow hardware or complex scene                   │
│                                                             │
│  Frame 1: Work=18ms, Target=16.67ms                         │
│           Work ALONE exceeds target!                        │
│           Sleep time would be NEGATIVE                      │
│           Frame: 18ms ❌ MISSED                             │
│                                                             │
│  Frame 2: Work=20ms, Target=16.67ms                         │
│           Frame: 20ms ❌ MISSED                             │
│                                                             │
│  Frame 3: Work=19ms, Target=16.67ms                         │
│           Frame: 19ms ❌ MISSED                             │
│                                                             │
│  Every single frame misses! Game feels terrible.            │
│  Player experiences constant stuttering.                    │
│                                                             │
│  SOLUTION: Lower the target FPS!                            │
│  At 30 FPS, target=33.33ms, and 18-20ms work fits easily!   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Why Adaptive FPS?

Sometimes your game can't maintain 60 FPS consistently:

- Old/slow hardware
- Complex scenes
- Background processes competing for CPU

**Adaptive FPS** automatically reduces the target frame rate to maintain **consistent** timing.

```
┌─────────────────────────────────────────────────────────────┐
│            ADAPTIVE FPS STATE MACHINE                       │
│                                                             │
│          ┌──────────────────────┐                          │
│          │                      │                          │
│          ▼                      │                          │
│    ┌──────────┐   miss rate     │                          │
│    │  60 FPS  │ ───>5%────────┐ │  miss rate               │
│    └──────────┘               │ │  <1%                     │
│          ▲                    ▼ │                          │
│          │              ┌──────────┐                       │
│          │              │  30 FPS  │                       │
│          │              └──────────┘                       │
│          │                    │ │                          │
│          │   miss rate        │ │  miss rate               │
│          │   <1%              │ │  >5%                     │
│          │              ┌─────┘ │                          │
│          │              ▼       ▼                          │
│          │        ┌──────────┐                             │
│          │        │  20 FPS  │                             │
│          │        └──────────┘                             │
│          │              │                                  │
│          │              │  miss rate >5%                   │
│          │              ▼                                  │
│          │        ┌──────────┐                             │
│          └────────│  15 FPS  │ (minimum)                   │
│            <1%    └──────────┘                             │
│                                                            │
│   Evaluation: Every 300 frames (~5 seconds at 60fps)       │
│   Miss threshold: >5% → reduce FPS                         │
│   Recover threshold: <1% → increase FPS                    │
│                                                            │
└─────────────────────────────────────────────────────────────┘
```

### Our Implementation

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 39-48

typedef struct AdaptiveFPS {
    int target_fps;           // Current target (60, 30, 20, or 15)
    int monitor_hz;           // Monitor refresh rate (max target)
    u32_t frames_sampled;  // Frames counted in current window
    u32_t frames_missed;   // Frames that exceeded target time
    u32_t sample_window;   // Evaluate every N frames (300 = 5 sec)
    real32 miss_threshold;    // Reduce if > this % miss (0.05 = 5%)
    real32 recover_threshold; // Increase if < this % miss (0.01 = 1%)
} AdaptiveFPS;
```

### How Evaluation Works (Step 11 in the Game Loop)

```c
// This runs at the end of EVERY frame (Step 11 from Section 4):

adaptive.frames_sampled++;

// Did this frame miss?
if (frame_time > target_frame_time + 0.002f) {  // 2ms tolerance
    adaptive.frames_missed++;
}

// Time to evaluate? (Every 300 frames ≈ 5 seconds)
if (adaptive.frames_sampled >= adaptive.sample_window) {

    real32 miss_rate = (real32)adaptive.frames_missed /
                       (real32)adaptive.frames_sampled;

    if (miss_rate > 0.05f) {  // >5% missed
        // REDUCE FPS: 60→30→20→15
        reduce_target_fps(&adaptive);
    }
    else if (miss_rate < 0.01f) {  // <1% missed
        // TRY TO RECOVER: 15→20→30→60
        increase_target_fps(&adaptive);
    }

    // Reset counters for next evaluation window
    adaptive.frames_sampled = 0;
    adaptive.frames_missed = 0;
}
```

### Why These Specific Thresholds?

```
┌─────────────────────────────────────────────────────────────┐
│              THRESHOLD RATIONALE                            │
│                                                             │
│  SAMPLE WINDOW: 300 frames (≈5 seconds at 60fps)            │
│  ────────────────────────────────────────────               │
│  • Too small (30 frames): Reacts to momentary hiccups       │
│  • Too large (3000 frames): Takes too long to adapt         │
│  • 5 seconds: Long enough to filter noise, short enough     │
│    to respond to sustained problems                         │
│                                                             │
│  MISS THRESHOLD: 5% (15 out of 300 frames)                  │
│  ────────────────────────────────────────────               │
│  • 5% = noticeable stutter to players                       │
│  • 1-2% = might just be OS scheduler noise                  │
│  • We reduce FPS only when it's clearly a problem           │
│                                                             │
│  RECOVER THRESHOLD: 1% (3 out of 300 frames)                │
│  ────────────────────────────────────────────               │
│  • Must be MUCH lower than miss threshold (hysteresis)      │
│  • Prevents oscillating: 60→30→60→30→60...                  │
│  • Only increase when we're VERY confident we can handle it │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Sections 7-8

You now understand the complete FPS management system:

1. **Measuring time** (Section 3) to track frame duration
2. **Game loop structure** (Section 4) where timing logic lives
3. **Two-phase sleep** (Section 5) for precise waiting
4. **Adaptive FPS** (Section 6) for graceful degradation

The next two sections are **reference material** - quick lookups for the data structures and functions we've discussed.

---

# PART 5: REFERENCE

> _Quick reference for data structures and functions. Come back here when implementing._

---

## 7. Key Data Structures

### 🔗 How These Structures Fit Together

```
┌─────────────────────────────────────────────────────────────┐
│           DATA STRUCTURE RELATIONSHIPS                      │
│                                                             │
│   ┌─────────────────┐                                       │
│   │   AdaptiveFPS   │ ← Controls the target frame time      │
│   │                 │   (Used in Section 6)                 │
│   │   target_fps    │──────────────────────────┐            │
│   │   monitor_hz    │                          │            │
│   │   frames_missed │                          │            │
│   └─────────────────┘                          │            │
│                                                │            │
│   ┌─────────────────┐                          │            │
│   │   FrameStats    │ ← Debug/profiling info   │            │
│   │                 │   (Not used in logic)    │            │
│   │   frame_count   │                          │            │
│   │   missed_frames │◄─────────────────────────┘            │
│   │   min/max/avg   │   (Both track misses,                 │
│   └─────────────────┘    but for different purposes)        │
│                                                             │
│   AdaptiveFPS.frames_missed → Used for FPS adjustment       │
│   FrameStats.missed_frames  → Used for end-of-run report    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### AdaptiveFPS

Controls automatic frame rate adjustment. Used in the **main game loop** (Section 4, Step 11).

```c
typedef struct AdaptiveFPS {
    int target_fps;           // 60, 30, 20, or 15
    int monitor_hz;           // Max target (from XRandR)
    u32_t frames_sampled;  // Counter for current evaluation window
    u32_t frames_missed;   // Frames that took >target + 2ms
    u32_t sample_window;   // Frames per evaluation (300)
    real32 miss_threshold;    // 0.05 = 5%
    real32 recover_threshold; // 0.01 = 1%
} AdaptiveFPS;

// Field-by-field explanation:
//
// target_fps: Current FPS we're trying to achieve
//   → Starts at monitor_hz (usually 60)
//   → Reduced when miss_rate > miss_threshold
//   → Increased when miss_rate < recover_threshold
//
// monitor_hz: Maximum FPS we'll ever try
//   → Set once at startup from XRandR
//   → Acts as ceiling for target_fps recovery
//
// frames_sampled: How many frames in current evaluation window
//   → Incremented every frame
//   → Reset to 0 when we evaluate
//
// frames_missed: How many frames exceeded target time
//   → Incremented when frame_time > target + 2ms
//   → Reset to 0 when we evaluate
//
// sample_window: How many frames between evaluations
//   → 300 frames ≈ 5 seconds at 60fps
//   → Balances responsiveness vs stability
//
// miss_threshold: When to REDUCE fps (0.05 = 5%)
//   → If frames_missed/frames_sampled > 0.05, reduce
//
// recover_threshold: When to INCREASE fps (0.01 = 1%)
//   → If frames_missed/frames_sampled < 0.01, try higher fps
//   → Much lower than miss_threshold to prevent oscillation
```

### FrameStats

Debug statistics for performance monitoring. Used for **end-of-run reporting** only.

```c
typedef struct FrameStats {
    u32_t frame_count;        // Total frames rendered
    u32_t missed_frames;      // Frames that exceeded target
    real32 min_frame_time_ms;    // Best frame time
    real32 max_frame_time_ms;    // Worst frame time
    real32 avg_frame_time_ms;    // Running average (summed, divide at end)
} FrameStats;

// This struct is for DEBUGGING/PROFILING only.
// It does NOT affect game behavior.
//
// At program exit, we print:
//   "Total frames: 54000"
//   "Missed frames: 127 (0.24%)"
//   "Min frame time: 16.45ms"
//   "Max frame time: 45.23ms"
//   "Avg frame time: 16.71ms"
```

---

## 8. Key Functions

### 🔗 Where Each Function is Used

```
┌─────────────────────────────────────────────────────────────┐
│           FUNCTION USAGE MAP                                │
│                                                             │
│   GAME LOOP STEP          FUNCTIONS USED                    │
│   ───────────────         ──────────────                    │
│   Step 1 (timestamp)  →   clock_gettime(), get_wall_clock() │
│   Step 6 (work time)  →   clock_gettime()                   │
│   Step 7 (sleep)      →   nanosleep(), clock_gettime()      │
│   Step 9 (total time) →   clock_gettime()                   │
│   Startup             →   get_monitor_refresh_rate()        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### `get_wall_clock()`

Returns current time in seconds with nanosecond precision.

```c
static inline real64 get_wall_clock(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (real64)ts.tv_sec + (real64)ts.tv_nsec / 1000000000.0;
}
```

### `get_monitor_refresh_rate()`

Queries the monitor's refresh rate using XRandR.

```c
static int get_monitor_refresh_rate(Display *display) {
    // Uses XRRGetScreenResources + XRRGetCrtcInfo
    // Returns detected rate or 60 as fallback
}
```

### `nanosleep()`

OS function for sleeping with nanosecond granularity. Used in **two-phase sleep** (Section 5, Phase 1).

```c
struct timespec sleep_spec = {
    .tv_sec = 0,        // Seconds
    .tv_nsec = 1000000  // Nanoseconds (1ms = 1,000,000ns)
};
nanosleep(&sleep_spec, NULL);

// WHY 1ms INCREMENTS?
// ────────────────────
// We could request to sleep for 7.67ms all at once:
//   sleep_spec.tv_nsec = 7670000;  // 7.67ms
//   nanosleep(&sleep_spec, NULL);
//
// BUT: OS might wake us anywhere from 7ms to 20ms later!
//
// By sleeping in 1ms increments and checking time each iteration:
// - We never overshoot by more than ~1-3ms
// - We can exit the sleep loop as soon as we're close enough
// - More predictable behavior across different systems
```

### 🔗 Connection to Section 9

Now that you understand all the components, Section 9 shows you the most common mistakes people make when implementing this system—and how to avoid them.

---

# PART 6: MASTERY

> _Avoid common mistakes, practice with exercises, and continue learning._

---

## 9. Common Pitfalls

### 🔗 How Pitfalls Relate to Previous Sections

Each pitfall corresponds to a concept from earlier sections:

| Pitfall                      | Related Section              | What Goes Wrong       |
| ---------------------------- | ---------------------------- | --------------------- |
| Using `sleep()` directly     | Section 5 (Sleep Strategies) | Imprecise timing      |
| Not accounting for work time | Section 2 (Frame Budget)     | Frames too long       |
| Wall clock for physics       | Section 3 (Measuring Time)   | Inconsistent gameplay |
| Busy-waiting entire frame    | Section 5 (Sleep Strategies) | Wastes CPU            |

### ❌ Pitfall 1: Using `sleep()` or `usleep()` Directly

```c
// BAD: Imprecise, can overshoot significantly
sleep(1);  // Sleeps ~1 second, but might be 1.01s or more
usleep(16000);  // Sleeps ~16ms, but might be 20ms
```

### ❌ Pitfall 2: Not Accounting for Work Time

```c
// BAD: Sleeps full frame time, ignoring work done
while (running) {
    do_game_work();  // Takes 6ms
    usleep(16666);   // Sleeps 16.67ms
    // Total: 22.67ms! Wrong!
}
```

### ❌ Pitfall 3: Using Wall Clock for Frame-Independent Logic

```c
// BAD: Frame time varies, physics becomes inconsistent
position += velocity;  // Assumes fixed dt

// GOOD: Use delta time
position += velocity * delta_time;
```

### ❌ Pitfall 4: Busy-Waiting for the Entire Frame

```c
// BAD: Burns 100% CPU
while (elapsed < target) {
    elapsed = get_time() - start;
}
```

### ✅ Solution: Two-Phase Sleep

```c
// GOOD: Sleep for most of the time, spin for the last few ms
while (elapsed < target - 0.003) {
    nanosleep(&one_ms, NULL);  // Saves CPU
    elapsed = get_time();
}
while (elapsed < target) {
    elapsed = get_time();  // Precise final timing
}
```

### ❌ Pitfall 5: Not Using Adaptive FPS

```c
// BAD: Fixed target FPS regardless of performance
#define TARGET_FPS 60  // Never changes!

while (running) {
    // If work takes 20ms, EVERY frame misses
    // Player experiences constant stutter
}

// GOOD: Adjust target based on actual performance
if (miss_rate > 0.05f) {
    target_fps = target_fps / 2;  // 60→30→15
    // Now 33.33ms budget, 20ms work fits easily
}
```

_(See Section 6 for full Adaptive FPS implementation)_

---

## 10. Practical Exercises

### 🔗 Exercise Progression

These exercises are ordered by complexity. Each builds on skills from previous exercises:

```
Exercise 1: Basic Timer ────► Learn clock_gettime (Section 3)
     │
     ▼
Exercise 2: FPS Counter ────► Apply to running loop (Section 4)
     │
     ▼
Exercise 3: Adaptive FPS ───► Implement adjustment logic (Section 6)
     │
     ▼
Exercise 4: Visualization ──► Debug and visualize timing (All Sections)
```

### Exercise 1: Basic Frame Timer

**Goal**: Practice using `clock_gettime()` from Section 3.

**Concepts Practiced**:

- Getting current time with nanosecond precision
- Calculating elapsed time
- Understanding the frame time target

Create a simple program that measures frame times.

```c
// exercise_1.c
// Compile: gcc -o ex1 exercise_1.c -lrt
// Run: ./ex1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// From Section 3: Measuring Time
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main() {
    // From Section 1: FPS targets
    double target_fps = 60.0;
    double target_frame_time = 1.0 / target_fps;  // 16.67ms

    for (int frame = 0; frame < 100; frame++) {
        double frame_start = get_time();

        // Simulate variable work (1-5ms random)
        usleep((rand() % 5 + 1) * 1000);

        double work_end = get_time();
        double work_time = work_end - frame_start;

        // ════════════════════════════════════════════════════
        // TODO: Implement two-phase sleep here (Section 5)
        // ════════════════════════════════════════════════════
        // Hint 1: Calculate how much time remains
        // Hint 2: Sleep until (target_frame_time - 0.003)
        // Hint 3: Then spin-wait until target_frame_time
        // ════════════════════════════════════════════════════

        double frame_end = get_time();
        double total_time = frame_end - frame_start;

        // From Section 2: Did we hit our budget?
        printf("Frame %3d: work=%.2fms, total=%.2fms, target=%.2fms %s\n",
               frame, work_time * 1000, total_time * 1000,
               target_frame_time * 1000,
               total_time > target_frame_time ? "MISSED!" : "OK");
    }
    return 0;
}
```

**Success Criteria**:

- Frame times should be within 0.5ms of 16.67ms
- No more than 5% of frames should show "MISSED!"

### Exercise 2: FPS Counter

**Goal**: Track and display actual FPS in real-time.

**Concepts Practiced**:

- Counting frames over time (relates to Section 6's sampling window)
- Real-time performance monitoring
- Understanding the difference between frame time and FPS

**Builds on**: Exercise 1 (you need a working game loop)

Add an FPS counter that updates every second.

```c
// Add these variables before your game loop:
int frames_this_second = 0;
double second_start = get_time();

// Inside the loop, after frame completes:
frames_this_second++;

// Check if one second has passed
if (get_time() - second_start >= 1.0) {
    printf("FPS: %d\n", frames_this_second);

    // Reset for next second
    frames_this_second = 0;
    second_start = get_time();
}
```

**Challenge Extension**:

- Also track min/max/average frame time per second
- Display: "FPS: 60 (min: 16.2ms, max: 17.1ms, avg: 16.7ms)"

**Success Criteria**:

- FPS should display as 60 (or very close) with your two-phase sleep working
- FPS should drop if you increase simulated work time

### Exercise 3: Adaptive FPS

**Goal**: Implement the adaptive FPS system from Section 6.

**Concepts Practiced**:

- Tracking miss rates over time
- Hysteresis (different thresholds for reducing vs increasing)
- Graceful degradation

**Builds on**: Exercises 1 and 2 (need timing + FPS tracking)

Implement the adaptive FPS system:

```c
typedef struct {
    int target_fps;
    int frames_sampled;
    int frames_missed;
    int sample_window;      // e.g., 300 frames
    float miss_threshold;   // e.g., 0.05 (5%)
    float recover_threshold; // e.g., 0.01 (1%)
} AdaptiveFPS;

// Initialize
AdaptiveFPS adaptive = {
    .target_fps = 60,
    .sample_window = 300,
    .miss_threshold = 0.05f,
    .recover_threshold = 0.01f
};

// TODO: Implement evaluate_adaptive_fps()
void evaluate_adaptive_fps(AdaptiveFPS *a, double frame_time) {
    double target_time = 1.0 / a->target_fps;

    a->frames_sampled++;

    // Was this frame a miss?
    if (frame_time > target_time + 0.002) {  // 2ms tolerance
        a->frames_missed++;
    }

    // Time to evaluate?
    if (a->frames_sampled >= a->sample_window) {
        float miss_rate = (float)a->frames_missed / a->frames_sampled;

        // ════════════════════════════════════════════════════
        // TODO: Implement the state machine from Section 6
        // ════════════════════════════════════════════════════
        // If miss_rate > miss_threshold: reduce (60→30→20→15)
        // If miss_rate < recover_threshold: increase (15→20→30→60)
        // ════════════════════════════════════════════════════

        // Reset counters
        a->frames_sampled = 0;
        a->frames_missed = 0;
    }
}
```

**Testing Tip**: Vary your simulated work time to trigger FPS changes:

```c
// Simulate heavy load
usleep(25000);  // 25ms work → should trigger 60→30 reduction
// Later, reduce to:
usleep(5000);   // 5ms work → should eventually recover to 60
```

**Success Criteria**:

- System should drop to 30 FPS when work exceeds 16.67ms
- System should recover to 60 FPS when work becomes light again
- Should NOT oscillate rapidly between FPS levels

### Exercise 4: Frame Time Graph

**Goal**: Create a visual debugging tool for frame timing.

**Concepts Practiced**:

- Visualizing timing data
- Understanding frame time distribution
- Identifying patterns in performance

**Builds on**: All previous exercises

Create a visual representation of frame times:

```c
void print_frame_bar(double frame_time_ms, double target_ms) {
    int bar_length = (int)(frame_time_ms * 3);  // 3 chars per ms
    int target_pos = (int)(target_ms * 3);      // Where target line should be

    printf("[");
    for (int i = 0; i < 60; i++) {
        if (i == target_pos) {
            printf("|");  // Target marker
        } else if (i < bar_length) {
            printf(frame_time_ms > target_ms ? "!" : "=");  // Exceeded vs OK
        } else {
            printf(" ");
        }
    }
    printf("] %.2fms %s\n", frame_time_ms,
           frame_time_ms > target_ms ? "MISS" : "");
}

// Example output:
// [==================|=====                          ] 8.23ms
// [==================|=========                      ] 9.67ms
// [==================|=================              ] 12.45ms
// [==================|=========================      ] 16.12ms
// [==================|===========================    ] 16.67ms
// [==================|!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  ] 19.83ms MISS
```

**Challenge Extensions**:

1. Add color (ANSI escape codes): green for OK, red for MISS
2. Show a rolling window of the last 10 frames
3. Add markers for sleep time vs work time within the bar

**Success Criteria**:

- Can visually identify frame time spikes
- Can see the 16.67ms target line clearly
- Can distinguish missed frames at a glance

---

## 11. Resources

### 🔗 Resources Organized by Section

| Section                      | Recommended Resources                  |
| ---------------------------- | -------------------------------------- |
| Section 1 (FPS Basics)       | "Fix Your Timestep!" article           |
| Section 3 (Time Measurement) | Linux man pages                        |
| Section 4 (Game Loop)        | "Game Programming Patterns: Game Loop" |
| Section 5 (Sleep Strategies) | Handmade Hero Day 18                   |
| Section 6 (Adaptive FPS)     | Game Engine Architecture book          |

### Books

- **"Game Engine Architecture" by Jason Gregory** - Chapter on Game Loop and Real-Time Simulation
- **"Real-Time Rendering" by Tomas Akenine-Möller** - Chapter on Performance

### Videos

- **Handmade Hero Day 18** - Casey Muratori's frame timing implementation
- **"Fix Your Timestep!" by Glenn Fiedler** - [gafferongames.com/post/fix_your_timestep](https://gafferongames.com/post/fix_your_timestep/)

### Articles

- **"Game Programming Patterns: Game Loop"** by Robert Nystrom - [gameprogrammingpatterns.com/game-loop.html](https://gameprogrammingpatterns.com/game-loop.html)
- **"dewitters gameloop"** - Classic article on game loops - [dewitters.com/dewitters-gameloop](https://www.koonsolo.com/dewitters-gameloop/)

### Documentation

- **Linux `clock_gettime()` man page**: `man clock_gettime`
  - _Relevant to_: Section 3 (time measurement)
- **Linux `nanosleep()` man page**: `man nanosleep`
  - _Relevant to_: Section 5 (sleep strategies)
- **POSIX Timers**: Understanding CLOCK_MONOTONIC vs CLOCK_REALTIME
  - _Relevant to_: Section 3 (clock selection)

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│                FPS QUICK REFERENCE                          │
├─────────────────────────────────────────────────────────────┤
│  TARGET FPS    │  FRAME TIME  │  USE CASE                   │
│  ──────────────┼──────────────┼───────────────────────────  │
│  60 FPS        │  16.67ms     │  Standard gameplay          │
│  30 FPS        │  33.33ms     │  Cinematic / low-power      │
│  20 FPS        │  50.00ms     │  Minimum playable           │
│  15 FPS        │  66.67ms     │  Last resort                │
├─────────────────────────────────────────────────────────────┤
│  TIMING FUNCTIONS                                           │
│  ─────────────────────────────────────────────────────────  │
│  clock_gettime(CLOCK_MONOTONIC, &ts)  → High-res time       │
│  nanosleep(&ts, NULL)                 → Sleep with ns res   │
│  __rdtsc()                            → CPU cycles          │
├─────────────────────────────────────────────────────────────┤
│  TWO-PHASE SLEEP                                            │
│  ─────────────────────────────────────────────────────────  │
│  Phase 1: Sleep 1ms chunks until (target - 3ms)             │
│  Phase 2: Spin-wait until exactly target                    │
├─────────────────────────────────────────────────────────────┤
│  ADAPTIVE FPS THRESHOLDS                                    │
│  ─────────────────────────────────────────────────────────  │
│  Reduce FPS:   >5% frames missed in 300-frame window        │
│  Increase FPS: <1% frames missed in 300-frame window        │
├─────────────────────────────────────────────────────────────┤
│  SECTION QUICK LOOKUP                                       │
│  ─────────────────────────────────────────────────────────  │
│  "What FPS should I target?"      → Section 1               │
│  "How much time do I have?"       → Section 2               │
│  "How do I measure time?"         → Section 3               │
│  "Where does timing code go?"     → Section 4               │
│  "How do I sleep precisely?"      → Section 5               │
│  "What if I can't keep up?"       → Section 6               │
│  "What data do I need?"           → Section 7               │
│  "What functions do I call?"      → Section 8               │
│  "What mistakes should I avoid?"  → Section 9               │
└─────────────────────────────────────────────────────────────┘
```

---

## Summary: How It All Fits Together

```
┌─────────────────────────────────────────────────────────────┐
│           COMPLETE FPS MANAGEMENT SYSTEM                    │
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                  AT STARTUP                         │   │
│   │                                                     │   │
│   │  1. Query monitor refresh rate (60Hz typical)       │   │
│   │  2. Set target_fps = monitor_hz                     │   │
│   │  3. Calculate target_frame_time = 1/target_fps      │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ▼                                  │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                 EACH FRAME                          │   │
│   │                                                     │   │
│   │  ┌───────────────────────────────────────────┐      │   │
│   │  │ 1. START TIMER                            │      │   │
│   │  │    frame_start = clock_gettime()          │      │   │
│   │  └───────────────────────────────────────────┘      │   │
│   │                     │                               │   │
│   │                     ▼                               │   │
│   │  ┌───────────────────────────────────────────┐      │   │
│   │  │ 2. DO WORK                                │      │   │
│   │  │    Input → Update → Render → GPU Upload   │      │   │
│   │  └───────────────────────────────────────────┘      │   │
│   │                     │                               │   │
│   │                     ▼                               │   │
│   │  ┌───────────────────────────────────────────┐      │   │
│   │  │ 3. MEASURE WORK TIME                      │      │   │
│   │  │    work_time = clock_gettime() - start    │      │   │
│   │  └───────────────────────────────────────────┘      │   │
│   │                     │                               │   │
│   │                     ▼                               │   │
│   │  ┌───────────────────────────────────────────┐      │   │
│   │  │ 4. TWO-PHASE SLEEP                        │      │   │
│   │  │    Phase 1: nanosleep until target - 3ms  │      │   │
│   │  │    Phase 2: spin-wait until target        │      │   │
│   │  └───────────────────────────────────────────┘      │   │
│   │                     │                               │   │
│   │                     ▼                               │   │
│   │  ┌───────────────────────────────────────────┐      │   │
│   │  │ 5. MEASURE TOTAL FRAME TIME               │      │   │
│   │  │    total = clock_gettime() - frame_start  │      │   │
│   │  └───────────────────────────────────────────┘      │   │
│   │                     │                               │   │
│   │                     ▼                               │   │
│   │  ┌───────────────────────────────────────────┐      │   │
│   │  │ 6. ADAPTIVE FPS EVALUATION                │      │   │
│   │  │    Track miss rate over 300 frames        │      │   │
│   │  │    Adjust target_fps if needed            │      │   │
│   │  └───────────────────────────────────────────┘      │   │
│   │                     │                               │   │
│   │                     └──────────► NEXT FRAME         │   │
│   │                                                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

_Last updated: Based on Handmade Hero X11 OpenGL backend implementation_
