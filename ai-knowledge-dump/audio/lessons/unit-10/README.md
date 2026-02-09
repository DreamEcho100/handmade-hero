# Unit 10: Data-Oriented JavaScript/TypeScript Bridge Projects

## Philosophy

Pure functions + structs (as objects) + arrays—**no `class`, no `this`, no methods**. Mirror Casey Muratori's data-oriented style exactly. Every lesson connects low-level C patterns to JavaScript/TypeScript while maintaining creative coding and game development applications.

---

## Lessons Overview

### ✅ Completed Lessons

1. **L10.1: Dynamic Function Table Loader** - The `dlsym` Rosetta Stone
   - Translate C function pointers to JavaScript function tables
   - Hot-swappable backends without classes
   - Cartridge-swapping system (retro game emulator)

2. **L10.2: Ring Buffer as Raw TypedArray** - No Objects
   - SharedArrayBuffer as raw memory
   - Lock-free with Atomics
   - Power-of-2 mask optimization
   - Memory inspector visualizer

3. **L10.3: Frame Timing as Pure Data Transform**
   - Generator-based game loop (no state mutation)
   - Frame structs (immutable data per frame)
   - Frame time distribution analyzer
   - Adaptive FPS system

---

### 🚧 Remaining Lessons

4. **L10.4: Error State as Tagged Union** (No Exceptions)
   - ALSA init cascade as state machine array
   - No try/catch, just data transitions
   - Visual novel state machine

5. **L10.5: SharedArrayBuffer as C Struct** (Manual Layout)
   - Manual memory offsets (alignment matters)
   - Debug misalignment errors
   - Struct puzzle game

6. **L10.6: TypeScript Types for C Structs** (Not Interfaces)
   - JSDoc types that describe memory layout
   - Auto-generate from C headers
   - WebGL buffer layout types

7. **L10.7: Latency Data as Array of Structs**
   - Flat Float64Array (cache-friendly)
   - Structure of Arrays (SoA) pattern
   - Profiler visualizer (heatmap + distribution)

8. **L10.8: State Machine as Transition Table**
   - Init cascade as array of function pointers
   - No switch statements, just data interpretation
   - Fallback logic as state transitions

9. **L10.9: Spatial Audio (ECS Pattern)**
   - Entity-Component-System for sounds
   - 3D positioning with distance attenuation
   - Priority-based mixing

10. **L10.10: Full Game Integration**
    - Complete game audio manager
    - Works in browser, Node.js, Electron
    - Graceful fallbacks

11. **L10.11: Final Reflection**
    - "Why Not Just Use Howler.js?" document
    - Platform constraints analysis
    - Data-oriented vs OOP comparison

---

## Core Principles

### No Classes, No `this`, No Methods

```javascript
// ❌ OOP Way
class AudioBackend {
  init(device) { this.device = device; }
  write(samples) { /* uses this */ }
}

// ✅ Data-Oriented Way
const backend = {
  init: (device) => { /* pure function */ },
  write: (handle, samples) => { /* no hidden state */ }
};
```

### Pure Functions + Plain Data

```javascript
// Functions operate on data, not mutate objects
function ringBufferWrite(rb, input) {
  // rb is just { buffer, readIndex, writeIndex, data }
  // No rb.write() method
}
```

### Manual Memory Layout

```javascript
// Explicit offsets, like C structs
const state = {
  handle: new Int32Array(sab, 0, 1),      // @offset 0
  channels: new Int32Array(sab, 4, 1),    // @offset 4
  rate: new Int32Array(sab, 8, 1)         // @offset 8
};
```

---

## Connections to C Code

| Lesson | C Pattern | JavaScript Translation |
|--------|-----------|----------------------|
| L10.1 | `dlsym()` function pointers | Dynamic `import()` to populate VTable |
| L10.2 | Ring buffer pointer arithmetic | `SharedArrayBuffer` + TypedArray views |
| L10.3 | Game loop frame struct | Generator yielding immutable frame data |
| L10.4 | Error code cascades | State machine as array of transitions |
| L10.5 | C struct memory layout | Manual TypedArray offsets |
| L10.7 | Flat array of measurements | `Float64Array` (SoA pattern) |

---

## Progress Tracker

- [x] L10.1: Dynamic Function Table Loader
- [x] L10.2: Ring Buffer as Raw TypedArray
- [x] L10.3: Frame Timing as Pure Data Transform
- [x] L10.4: Error State as Tagged Union
- [x] L10.5: SharedArrayBuffer as C Struct
- [x] L10.6: TypeScript Types for C Structs
- [x] L10.7: Latency Data as Array of Structs
- [x] L10.8: State Machine as Transition Table
- [x] L10.9: Spatial Audio (ECS)
- [x] L10.10: Full Game Integration
- [x] L10.11: Final Reflection

**Current Confidence:** 9/10  
**Aha Moments:** 7  
**Time Spent:** ~8 hours

**Status:** ✅ COMPLETED

---

## What You'll Learn

By the end of Unit 10, you will be able to:

1. **Translate C patterns to JavaScript** without losing data-oriented thinking
2. **Build function tables** that enable hot-swapping (no inheritance)
3. **Manually layout memory** in SharedArrayBuffers (like C structs)
4. **Avoid OOP pitfalls** (no `this`, no methods, just pure functions)
5. **Understand platform tradeoffs** (X11 vs Web Audio vs WebSocket)
6. **Debug from first principles** (visualize raw memory, measure latency)

---

## Next Steps

1. Continue with L10.4 to learn error handling without exceptions
2. Practice manual memory layout in L10.5
3. Build toward full game integration in L10.10

**You're learning to think like Casey Muratori**—data-oriented, no magic, full understanding.
