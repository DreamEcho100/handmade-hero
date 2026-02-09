# Unit 10: Data-Oriented JavaScript/TypeScript Bridge Projects

**Location:** `./project/misc/audio/unit10-data-oriented/`

**Philosophy:** Pure functions + structs (as objects) + arrays—**no `class`, no `this`, no methods**. Mirror Casey Muratori's data-oriented style exactly. Every lesson connects low-level C patterns to JavaScript/TypeScript while maintaining creative coding and game development applications.

---

## Lesson 1: Dynamic Function Table Loader (The `dlsym` Rosetta Stone)

**Core Concept:** Build a **plain function table** that you **hot-load** at runtime—exactly like `audio.c:133-200`, but in JavaScript. This is your first step into data-oriented thinking.

### Project: Function Table Runtime Loader

```typescript
// function-table.mjs
/**
 * @typedef {Object} AudioBackendVTable
 * @property {function(string): number} init          // Returns handle or -1
 * @property {function(number, Float32Array, number): number} write
 * @property {function(number): {playCursor: number, writeCursor: number, delay: number}} getCursorInfo
 * @property {function(number): void} shutdown
 */

/** @type {AudioBackendVTable|null} */
let g_audioBackend = null;

/**
 * Load backend module and populate function table
 * @param {string} modulePath
 */
export async function loadBackend(modulePath) {
  // This mirrors audio.c:133-200 exactly
  const module = await import(pathToFileURL(modulePath).href);
  
  // JS version of: *(void **)(&SndPcmOpen) = dlsym(...);
  g_audioBackend = {
    init: module.init,
    write: module.write,
    getCursorInfo: module.getCursorInfo,
    shutdown: module.shutdown
  };
}
```

**Hard Requirements:**
- Use **only plain objects with function properties**—no `class AudioBackend { }`
- After loading, call `g_audioBackend.init()` directly—**just like calling a function pointer in C**
- No method binding, no `this` context

**Data-Oriented Test:**
```typescript
// backends/alsa-backend.mjs
export function init(deviceName) {
  console.log(`[ALSA] Initializing: ${deviceName}`);
  return 0; // Success
}

export function write(handle, samples, frameCount) {
  // Raw buffer manipulation - no objects
  return frameCount;
}
```

**Web API Mirror: "Web Audio Function Table"**

Build the same VTable for Web Audio API—but wrap it in a data-oriented interface:

```typescript
/** 
 * @typedef {Object} WebAudioVTable
 * @property {function(): AudioContext} createContext
 * @property {function(AudioContext, AudioNode): void} connectNode
 * @property {function(AudioContext): number} getCurrentTime
 */

const webAudioVTable = {
  createContext: () => new AudioContext(),
  connectNode: (ctx, node) => node.connect(ctx.destination),
  getCurrentTime: (ctx) => ctx.currentTime
};
```

**Creative Coding Challenge: Retro Game Console Emulator**

Make it a **cartridge-swapping system**—each "cartridge" (backend) has a **function table** at a fixed offset:

```typescript
/**
 * @typedef {Object} CartridgeVTable
 * @property {function(): void} onLoad
 * @property {function(Float32Array): void} fillAudioBuffer
 * @property {function(): void} onEject
 */

const cartridgeSlot = {
  current: null,
  swap(newCartridge) {
    if (this.current) this.current.onEject();
    this.current = newCartridge;
    this.current.onLoad();
  }
};
```

**Aha Moment:** When you realize `g_audioBackend.write()` is **exactly** the same as `(*SndPcmWritei)(handle, ...)`—no magic, just a function pointer in a struct.

**Connects to:**
- Unit 1-L1.2 (`dlsym` pattern)
- Unit 1-L1.4 (macro redirection)
- Unit 5-L5.1 (platform API)

**Enables:** Lesson 4 (hot-swappable backends), Lesson 8 (error simulation)

---

## Lesson 2: Ring Buffer as Raw TypedArray (No Objects)

**Core Concept:** Visualize your ring buffer as **raw memory**—just a `SharedArrayBuffer` with manual cursor arithmetic. **No classes, just data transforms**.

### Project: Lock-Free Ring Buffer

```typescript
// ring-buffer-raw.mjs
/**
 * @typedef {Object} RingBuffer
 * @property {SharedArrayBuffer} buffer
 * @property {Int32Array} readIndex  // First 4 bytes
 * @property {Int32Array} writeIndex // Next 4 bytes
 * @property {Float32Array} data     // Rest of buffer
 */

/**
 * Create ring buffer - mirrors L2.4: Ring Buffer Mental Model
 * @param {number} sizePowerOfTwo Must be power of 2 for mask optimization
 * @returns {RingBuffer}
 */
export function createRingBuffer(sizePowerOfTwo) {
  const buffer = new SharedArrayBuffer(8 + sizePowerOfTwo * 4);
  return {
    buffer,
    readIndex: new Int32Array(buffer, 0, 1),
    writeIndex: new Int32Array(buffer, 4, 1),
    data: new Float32Array(buffer, 8, sizePowerOfTwo)
  };
}

/**
 * Write samples - mirrors audio.c:470-603 sample writing loop
 * @param {RingBuffer} rb
 * @param {Float32Array} input
 * @returns {number} Frames written
 */
export function ringBufferWrite(rb, input) {
  const mask = rb.data.length - 1;
  let written = 0;
  
  for (let i = 0; i < input.length; i++) {
    const writePos = Atomics.load(rb.writeIndex, 0) & mask;
    const readPos = Atomics.load(rb.readIndex, 0);
    
    if (writePos === readPos) break; // Buffer full - like snd_pcm_avail() == 0
    
    rb.data[writePos] = input[i];
    Atomics.add(rb.writeIndex, 0, 1);
    written++;
  }
  return written;
}

/**
 * Read samples - consumer side
 * @param {RingBuffer} rb
 * @param {Float32Array} output
 * @returns {number} Frames read
 */
export function ringBufferRead(rb, output) {
  const mask = rb.data.length - 1;
  let read = 0;
  
  for (let i = 0; i < output.length; i++) {
    const readPos = Atomics.load(rb.readIndex, 0) & mask;
    const writePos = Atomics.load(rb.writeIndex, 0);
    
    if (readPos === writePos) break; // Buffer empty
    
    output[i] = rb.data[readPos];
    Atomics.add(rb.readIndex, 0, 1);
    read++;
  }
  return read;
}
```

**Debug Challenge:** **Force buffer wrap** by writing more than `sizePowerOfTwo`—watch the cursor arithmetic wrap thanks to `& mask`. This is the same math as your `running_index % buffer_size`.

**Data-Oriented Principle:** The **RingBuffer is just a bag of typed arrays**—no methods, just pure functions that operate on it.

**Web API Mirror: "AudioWorklet Raw Memory"**

In an AudioWorklet, allocate a `SharedArrayBuffer` and **manually fill it** without any `AudioBuffer` wrapper:

```javascript
// In AudioWorkletProcessor
class RawMemoryProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.buffer = new SharedArrayBuffer(128 * 4);
    this.output = new Float32Array(this.buffer);
    this.phase = 0;
  }
  
  process(inputs, outputs, parameters) {
    const sampleRate = 48000;
    for (let i = 0; i < 128; i++) {
      this.output[i] = Math.sin(2 * Math.PI * 440 * this.phase / sampleRate);
      this.phase++;
    }
    
    outputs[0][0].set(this.output);
    return true;
  }
}
```

**Creative Coding: Memory Inspector Visualizer**

Build a **memory inspector** that renders the raw `SharedArrayBuffer` as **hex dump + waveform side-by-side**:

```typescript
/**
 * @param {RingBuffer} rb
 * @param {CanvasRenderingContext2D} ctx
 */
function visualizeRingBuffer(rb, ctx) {
  const width = ctx.canvas.width;
  const height = ctx.canvas.height;
  
  // Top half: hex dump
  const bytes = new Uint8Array(rb.buffer);
  for (let i = 0; i < Math.min(256, bytes.length); i++) {
    const x = (i % 32) * 20;
    const y = Math.floor(i / 32) * 16;
    ctx.fillText(bytes[i].toString(16).padStart(2, '0'), x, y);
  }
  
  // Bottom half: waveform
  const readPos = Atomics.load(rb.readIndex, 0);
  const writePos = Atomics.load(rb.writeIndex, 0);
  
  ctx.strokeStyle = '#0f0';
  ctx.beginPath();
  for (let i = 0; i < rb.data.length; i++) {
    const x = (i / rb.data.length) * width;
    const y = height / 2 + rb.data[i] * height / 4;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  }
  ctx.stroke();
  
  // Cursor indicators
  ctx.fillStyle = '#f00';
  ctx.fillRect((readPos / rb.data.length) * width, 0, 2, height);
  ctx.fillStyle = '#00f';
  ctx.fillRect((writePos / rb.data.length) * width, 0, 2, height);
}
```

**Corrupted memory shows as visual glitches**—like debugging your C code with `xxd` and `audacity` simultaneously.

**Aha Moment:** When you see that **changing `rb.data[writePos]`** is **exactly** like `sample_buffer[write_index] = value`—it's just pointer arithmetic.

**Connects to:**
- Unit 1-L1.4 (ring buffer internals)
- Unit 1-L1.7 (profiling/visualization)
- Unit 5-L5.2 (custom debug probes)

**Enables:** Lesson 3 (frame timing), Lesson 5 (shared memory), Lesson 7 (latency arrays)

---

## Lesson 3: Frame Timing as Pure Data Transform

**Core Concept:** Simulate your **game loop** (`backend.c:423-899`) as a **generator function that yields plain structs**—no game state object, just data per frame.

### Project: Stateless Game Loop Simulator

```typescript
// game-loop-data.mjs
/**
 * @typedef {Object} GameFrame
 * @property {number} frameNumber
 * @property {number} deltaTimeMs Exact frame time
 * @property {number} targetFps
 * @property {number} samplesToWrite Derived from targetFps
 * @property {number} writeCursor Current sample write position
 * @property {Object} performanceMetrics
 * @property {number} performanceMetrics.cpuTime
 * @property {number} performanceMetrics.bufferAvailable
 */

/**
 * Generator-based game loop - mirrors backend.c:423-899
 * @param {number} targetFps
 * @generator
 * @yields {GameFrame}
 * @returns {Generator<GameFrame, void, number>}
 */
export function* gameLoopSimulator(targetFps) {
  let runningSampleIndex = 0;
  let frameNumber = 0;
  const sampleRate = 48000;
  
  while (true) {
    const samplesThisFrame = Math.floor(sampleRate / targetFps);
    
    const frame = {
      frameNumber,
      deltaTimeMs: 1000 / targetFps,
      targetFps,
      samplesToWrite: samplesThisFrame,
      writeCursor: runningSampleIndex,
      performanceMetrics: {
        cpuTime: 0,
        bufferAvailable: 0
      }
    };
    
    const actualDeltaTime = yield frame;
    
    runningSampleIndex += samplesThisFrame;
    frameNumber++;
  }
}
```

**Usage Pattern:**

```typescript
const loop = gameLoopSimulator(60);
let frame = loop.next().value;

while (playing) {
  const startTime = performance.now();
  
  // Simulate game logic
  processGameLogic(frame);
  
  // Fill audio buffer
  const audioData = new Float32Array(frame.samplesToWrite);
  fillAudioBuffer(audioData, frame.writeCursor);
  
  // Measure performance
  frame.performanceMetrics.cpuTime = performance.now() - startTime;
  frame.performanceMetrics.bufferAvailable = getBufferAvail();
  
  // Get next frame with actual measured delta
  const actualDelta = performance.now() - startTime;
  frame = loop.next(actualDelta).value;
}
```

**Data-Oriented Principle:** **No `class GameLoop { }`**—the state is in the generator's closure, but you treat it as **immutable data per frame**.

**Web API Mirror: "requestAnimationFrame Data Logger"**

```javascript
const frames = [];
let lastTimestamp = 0;

function logFrame(timestamp) {
  const deltaTime = timestamp - lastTimestamp;
  
  frames.push({
    timestamp,
    deltaTime,
    fps: deltaTime > 0 ? 1000 / deltaTime : 0,
    frameNumber: frames.length
  });
  
  lastTimestamp = timestamp;
  requestAnimationFrame(logFrame);
}

requestAnimationFrame(logFrame);
```

**Creative Coding: Frame Time Distribution Analyzer**

Build a **frame time distribution analyzer**—collect 1000 frames of data, then **render histograms** of deltaTime variance:

```typescript
/**
 * @typedef {Object} FrameStats
 * @property {number[]} deltaTimes
 * @property {number} avgDelta
 * @property {number} minDelta
 * @property {number} maxDelta
 * @property {number} stdDev
 */

/**
 * Analyze frame timing - mirrors perf tool (Unit 8-L8.1)
 * @param {GameFrame[]} frames
 * @returns {FrameStats}
 */
function analyzeFrameTiming(frames) {
  const deltaTimes = frames.map(f => f.deltaTimeMs);
  const sum = deltaTimes.reduce((a, b) => a + b, 0);
  const avg = sum / deltaTimes.length;
  
  const variance = deltaTimes
    .map(dt => Math.pow(dt - avg, 2))
    .reduce((a, b) => a + b, 0) / deltaTimes.length;
  
  return {
    deltaTimes,
    avgDelta: avg,
    minDelta: Math.min(...deltaTimes),
    maxDelta: Math.max(...deltaTimes),
    stdDev: Math.sqrt(variance)
  };
}

/**
 * Render histogram to canvas
 * @param {FrameStats} stats
 * @param {CanvasRenderingContext2D} ctx
 */
function renderHistogram(stats, ctx) {
  const buckets = new Array(60).fill(0);
  const bucketSize = (stats.maxDelta - stats.minDelta) / buckets.length;
  
  stats.deltaTimes.forEach(dt => {
    const bucket = Math.floor((dt - stats.minDelta) / bucketSize);
    buckets[Math.min(bucket, buckets.length - 1)]++;
  });
  
  const maxCount = Math.max(...buckets);
  const width = ctx.canvas.width;
  const height = ctx.canvas.height;
  
  buckets.forEach((count, i) => {
    const barHeight = (count / maxCount) * height;
    const x = (i / buckets.length) * width;
    const barWidth = width / buckets.length;
    
    ctx.fillStyle = '#0f0';
    ctx.fillRect(x, height - barHeight, barWidth, barHeight);
  });
}
```

**Visualize frame time as a racing line**—if jitter is high, the line wobbles. **Smooth 60 FPS = perfect straight line**. This is the browser equivalent of your `debug_audio_markers`.

**Aha Moment:** When you realize `yield frame` is **like returning from a game loop iteration**—the generator's internal state is your `runningSampleIndex`, but you **never mutate it directly**.

**Connects to:**
- Unit 1-L1.5 (event loop)
- Unit 3-L3.1 (game loop anatomy)
- Unit 8-L8.1 (`perf` profiling)

**Enables:** Lesson 7 (latency measurement), Lesson 10 (game audio manager)

---

## Lesson 4: Error State as Tagged Union (No Exceptions)

**Core Concept:** Model your **ALSA error cascade** (`Unit 4-L4.3`) as a **plain array of state transitions**—no try/catch, just data.

### Project: Init State Machine

```typescript
// error-states.mjs
/**
 * @typedef {Object} InitStep
 * @property {string} name
 * @property {function(): number} action Returns 0 or negative error code
 * @property {number} onSuccess Next step index
 * @property {number} onFailure Fallback step index (-1 = abort)
 */

/** @type {InitStep[]} */
const initSteps = [
  {
    name: 'dlopen_alsa',
    action: () => (Math.random() > 0.05 ? 0 : -1),
    onSuccess: 1,
    onFailure: 4 // Jump to PulseAudio fallback
  },
  {
    name: 'dlsym_snd_pcm_open',
    action: () => 0,
    onSuccess: 2,
    onFailure: -1
  },
  {
    name: 'snd_pcm_open',
    action: () => (Math.random() > 0.1 ? 0 : -2),
    onSuccess: 3,
    onFailure: 4
  },
  {
    name: 'snd_pcm_set_params',
    action: () => 0,
    onSuccess: -2, // Success terminal
    onFailure: 4
  },
  {
    name: 'try_pulse',
    action: () => 0,
    onSuccess: -2,
    onFailure: 5
  },
  {
    name: 'fallback_null',
    action: () => 0,
    onSuccess: -2,
    onFailure: -1
  }
];

/**
 * Run initialization state machine
 * @returns {('alsa'|'pulse'|'null'|'failed')}
 */
export function runInitMachine() {
  let stepIndex = 0;
  const executedSteps = [];
  
  while (stepIndex >= 0 && stepIndex < initSteps.length) {
    const step = initSteps[stepIndex];
    executedSteps.push(step.name);
    
    const result = step.action();
    
    if (result === 0) {
      stepIndex = step.onSuccess;
    } else {
      console.log(`[INIT] ${step.name} failed with ${result}`);
      stepIndex = step.onFailure;
    }
    
    // Terminal success
    if (stepIndex === -2) {
      if (executedSteps.includes('snd_pcm_set_params')) return 'alsa';
      if (executedSteps.includes('try_pulse')) return 'pulse';
      return 'null';
    }
  }
  
  return 'failed';
}
```

**Data-Oriented Principle:** **The state machine is just an array**—you **interpret** it, no methods, no switch statements.

**Web API Mirror: "Promise Chain as State Machine"**

```typescript
/**
 * @typedef {Object} FetchStep
 * @property {function(): Promise<any>} fn
 * @property {number} onSuccess
 * @property {number} onFailure
 */

/** @type {FetchStep[]} */
const fetchSteps = [
  {
    fn: () => fetch('/api/data'),
    onSuccess: 1,
    onFailure: 3 // Cached data fallback
  },
  {
    fn: (response) => response.json(),
    onSuccess: 2,
    onFailure: 3
  },
  {
    fn: (data) => processData(data),
    onSuccess: -2,
    onFailure: -1
  },
  {
    fn: () => getCachedData(),
    onSuccess: 2,
    onFailure: -1
  }
];
```

**Creative Coding: Visual Novel State Machine**

Build a **visual novel** where each step is a **dialogue node**:

```typescript
/**
 * @typedef {Object} DialogueNode
 * @property {string} text
 * @property {string[]} choices
 * @property {number[]} nextNodes Parallel to choices
 * @property {function(): number} condition Optional: auto-branching
 */

/** @type {DialogueNode[]} */
const story = [
  {
    text: "ALSA library loading...",
    choices: ["Continue"],
    nextNodes: [1],
    condition: () => (Math.random() > 0.9 ? 3 : 1) // Random failure
  },
  {
    text: "Successfully loaded ALSA!",
    choices: ["Open device", "Try PulseAudio instead"],
    nextNodes: [2, 4]
  },
  {
    text: "Device opened. Audio quality: Perfect",
    choices: ["Play sound"],
    nextNodes: [-2] // Success ending
  },
  {
    text: "ALSA failed! System message: Permission denied",
    choices: ["Retry", "Fallback to PulseAudio"],
    nextNodes: [1, 4]
  },
  {
    text: "PulseAudio active. Audio quality: Degraded",
    choices: ["Accept degraded audio"],
    nextNodes: [-2]
  }
];
```

**The player's choices (success/failure) drive the narrative**. **Wrong path = degraded audio UX**—you see what users experience when audio fails.

**Aha Moment:** When you realize **this is how Casey does game state**—no `switch` on enums, just **arrays of function pointers** (or step data).

**Connects to:**
- Unit 1-L1.6 (error handling)
- Unit 4-L4.3 (init cascade)
- Unit 2-L2.3 (WASAPI sessions)

**Enables:** Lesson 8 (fallback simulator), Lesson 10 (robust game audio)

---

## Lesson 5: SharedArrayBuffer as C Struct (Manual Layout)

**Core Concept:** Manually **layout memory** like a C struct—**offsets matter, alignment matters, no abstraction**.

### Project: ALSA State Struct in Memory

```typescript
// c-struct-layout.mjs
/**
 * Memory layout (32 bytes total):
 * @typedef {Object} AlsaState
 * @property {Int32Array} handle       @offset 0  (4 bytes)
 * @property {Int32Array} channels     @offset 4  (4 bytes)
 * @property {Int32Array} rate         @offset 8  (4 bytes)
 * @property {Int32Array} latency_ms   @offset 12 (4 bytes)
 * @property {BigInt64Array} writeCursor @offset 16 (8 bytes, aligned)
 * @property {BigInt64Array} runningIndex @offset 24 (8 bytes)
 */

const STATE_SIZE = 32;

/**
 * Create ALSA state struct from raw memory
 * @param {SharedArrayBuffer} sab
 * @returns {AlsaState}
 */
export function createAlsaState(sab) {
  // Manual offset calculation—like C struct padding
  return {
    handle: new Int32Array(sab, 0, 1),
    channels: new Int32Array(sab, 4, 1),
    rate: new Int32Array(sab, 8, 1),
    latency_ms: new Int32Array(sab, 12, 1),
    writeCursor: new BigInt64Array(sab, 16, 1),  // 8-byte aligned
    runningIndex: new BigInt64Array(sab, 24, 1)
  };
}

/**
 * Initialize state - like alsa_init()
 * @param {AlsaState} state
 * @param {number} handle
 * @param {number} channels
 * @param {number} rate
 */
export function initState(state, handle, channels, rate) {
  state.handle[0] = handle;
  state.channels[0] = channels;
  state.rate[0] = rate;
  state.latency_ms[0] = Math.floor((4096 * 1000) / rate);
  state.writeCursor[0] = 0n;
  state.runningIndex[0] = 0n;
}

/**
 * Update write cursor - like audio write loop
 * @param {AlsaState} state
 * @param {number} framesWritten
 */
export function updateCursors(state, framesWritten) {
  const newIndex = state.runningIndex[0] + BigInt(framesWritten);
  state.runningIndex[0] = newIndex;
  state.writeCursor[0] = newIndex % BigInt(state.rate[0]);
}
```

**Data-Oriented Principle:** **The "struct" is just a bag of TypedArrays**—you **manually access** `state.handle[0]`, not `state.handle`.

**Debug Challenge: Misalignment Test**

```typescript
/**
 * BROKEN VERSION - misaligned BigInt64Array
 */
function createBrokenState(sab) {
  return {
    handle: new Int32Array(sab, 0, 1),
    channels: new Int32Array(sab, 4, 1),
    rate: new Int32Array(sab, 8, 1),
    // BUG: BigInt64Array at offset 10 (not 8-byte aligned!)
    writeCursor: new BigInt64Array(sab, 10, 1), // CRASHES
    runningIndex: new BigInt64Array(sab, 18, 1)
  };
}
```

**Misalign** a field (put `writeCursor` at offset 10, not 16). **Watch the crash/corruption**—this teaches **why C struct alignment matters** (Unit 7-L7.2 cache effects).

**Web API Mirror: "AudioBuffer Manual Allocation"**

**Don't use `AudioBuffer`**—allocate raw memory and **format it yourself**:

```javascript
// 1 second of stereo audio at 48kHz
const sab = new SharedArrayBuffer(48000 * 4 * 2);
const leftChannel = new Float32Array(sab, 0, 48000);
const rightChannel = new Float32Array(sab, 48000 * 4, 48000);

// Manual interleaving
for (let i = 0; i < 48000; i++) {
  leftChannel[i] = Math.sin(2 * Math.PI * 440 * i / 48000);
  rightChannel[i] = Math.sin(2 * Math.PI * 880 * i / 48000);
}
```

**This is exactly what `snd_pcm_set_params` does internally**—you're manually interleaving samples.

**Creative Coding: Struct Puzzle Game**

Build a **struct puzzle game**—you're given a **hex dump** and a **struct definition with wrong offsets**:

```typescript
/**
 * @typedef {Object} PuzzleLevel
 * @property {Uint8Array} hexDump Raw memory
 * @property {string} secretMessage Hidden in struct
 * @property {Object} structHints Wrong offsets - you must fix them
 */

const level1 = {
  hexDump: new Uint8Array([
    0x48, 0x65, 0x6C, 0x6C, // "Hell"
    0x6F, 0x20, 0x00, 0x00, // "o "
    0x57, 0x6F, 0x72, 0x6C, // "Worl"
    0x64, 0x21, 0x00, 0x00  // "d!"
  ]),
  secretMessage: "Hello World!",
  structHints: {
    part1Offset: 0,  // Correct
    part2Offset: 6,  // WRONG! Should be 4
    part3Offset: 10, // WRONG! Should be 8
    part4Offset: 14  // WRONG! Should be 12
  }
};

function solvePuzzle(level) {
  // Player must find correct offsets to decode message
}
```

**Fix the offsets to decode the secret message** (like reverse engineering a binary format).

**Connects to:**
- Unit 1-L1.2 (X11 structs)
- Unit 7-L7.2 (cache alignment)
- Unit 3-L3.2 (ArrayBuffer emulation)

**Enables:** Lesson 6 (TypeScript types), Lesson 9 (WASM memory)

---

## Lesson 6: TypeScript Types for C Structs (Not Interfaces)

**Core Concept:** Write JSDoc types that **describe memory layout**, not behavior. Types should mirror C structs exactly.

### Project: Complete Type Definitions for Audio System

```typescript
// audio-structs.d.ts

/**
 * Mirrors GameSoundOutput from handmade hero
 * @typedef {Object} GameSoundOutput
 * @property {number} samples_per_second
 * @property {number} sample_count
 * @property {Float32Array} samples - Flat interleaved array
 * @property {bigint} write_cursor_sample
 */

/**
 * Platform memory block - mirrors platform.h
 * @typedef {Object} PlatformMemoryBlock
 * @property {ArrayBuffer} memory
 * @property {number} size Total allocated size
 * @property {number} used Currently used bytes
 * @property {PlatformMemoryBlock|null} next_free Linked list pointer
 */

/**
 * Audio backend vtable
 * @typedef {Object} AudioBackend
 * @property {function(string): number} init
 * @property {function(number, Float32Array, number): number} write_samples
 * @property {function(number): void} shutdown
 * @property {function(number): number} get_buffer_size
 */

/**
 * Ring buffer state
 * @typedef {Object} RingBufferState
 * @property {SharedArrayBuffer} buffer
 * @property {Uint32Array} read_cursor  @offset 0
 * @property {Uint32Array} write_cursor @offset 4
 * @property {Float32Array} data        @offset 8
 * @property {number} size_mask Power-of-2 mask for wrapping
 */

/**
 * Game state per frame
 * @typedef {Object} GameFrameState
 * @property {number} frame_number
 * @property {number} delta_time_ms
 * @property {number} samples_to_generate
 * @property {bigint} absolute_sample_index
 * @property {Object} metrics
 * @property {number} metrics.cpu_time_ms
 * @property {number} metrics.buffer_fill_percentage
 */
```

**Advanced: Function Pointer Types**

```typescript
/**
 * Callback signature - like X11 event handlers
 * @callback AudioCallback
 * @param {Float32Array} output_buffer
 * @param {number} frame_count
 * @param {GameFrameState} frame_state
 * @returns {void}
 */

/**
 * Error handler - like ALSA error callbacks
 * @callback ErrorHandler
 * @param {number} error_code
 * @param {string} error_message
 * @returns {boolean} true = retry, false = abort
 */

/**
 * Audio system configuration
 * @typedef {Object} AudioConfig
 * @property {number} sample_rate
 * @property {number} channels
 * @property {number} buffer_size_frames
 * @property {AudioCallback} fill_callback
 * @property {ErrorHandler} on_error
 */
```

**Data-Oriented Principle:** **Types describe memory, not methods**. `GameSoundOutput` is **plain data**—you pass it to functions, not call methods on it.

**Hard Mode: Auto-Generate from C Headers**

```typescript
// parse-c-header.mjs
import { readFileSync } from 'fs';

/**
 * Parse C struct to JSDoc typedef
 * @param {string} cHeaderPath
 * @returns {string} JSDoc typedef
 */
export function parseStructToJSDoc(cHeaderPath) {
  const content = readFileSync(cHeaderPath, 'utf8');
  const structMatch = content.match(/typedef struct\s+{([^}]+)}\s+(\w+);/);
  
  if (!structMatch) return '';
  
  const [, body, name] = structMatch;
  const fields = body.split(';').map(field => {
    const match = field.trim().match(/(\w+)\s+(\w+)/);
    if (!match) return null;
    const [, type, fieldName] = match;
    return { type: mapCTypeToJS(type), fieldName };
  }).filter(Boolean);
  
  let jsdoc = `/**\n * @typedef {Object} ${name}\n`;
  fields.forEach(({ type, fieldName }) => {
    jsdoc += ` * @property {${type}} ${fieldName}\n`;
  });
  jsdoc += ` */\n`;
  
  return jsdoc;
}

function mapCTypeToJS(cType) {
  const typeMap = {
    'int': 'number',
    'float': 'number',
    'double': 'number',
    'uint32_t': 'number',
    'int64_t': 'bigint',
    'void*': 'number', // Pointer as offset
    'char*': 'string'
  };
  return typeMap[cType] || 'any';
}
```

**Web API Mirror: "WebGL Buffer Layout Types"**

```typescript
/**
 * Vertex buffer layout - mirrors OpenGL vertex attributes
 * @typedef {Object} Vertex
 * @property {Float32Array} position @offset 0, length 3
 * @property {Float32Array} normal   @offset 12, length 3
 * @property {Float32Array} uv       @offset 24, length 2
 * @property {Uint8Array} color      @offset 32, length 4
 */

/** Total vertex size: 36 bytes */
const VERTEX_SIZE = 36;

/**
 * Create vertex buffer
 * @param {number} vertexCount
 * @returns {ArrayBuffer}
 */
function createVertexBuffer(vertexCount) {
  return new ArrayBuffer(vertexCount * VERTEX_SIZE);
}
```

**Aha Moment:** When VSCode autocompletes `sample.write()` and you **delete it**—because **writing is a function that takes the struct, not a method**.

**Connects to:**
- Unit 1-L1.2 (X11 structs)
- Unit 2-L2.10 (API evolution)
- Unit 3-L3.5 (type mapping)

**Enables:** All future projects, Unit 3-L3.10 (abstraction library)

---

## Lesson 7: Latency Data as Array of Structs

**Core Concept:** Collect **1000 latency measurements** as a **flat Float64Array**—not an array of objects. This is cache-friendly data-oriented design.

### Project: Latency Profiler

```typescript
// latency-struct-array.mjs

/**
 * Each measurement is 24 bytes (3 x Float64):
 * @typedef {Object} LatencySample
 * @property {number} timestamp      @offset 0
 * @property {number} keypressTime   @offset 8
 * @property {number} audioTime      @offset 16
 */

const SAMPLE_SIZE = 3; // 3 Float64 values
const MAX_SAMPLES = 1000;

/** Flat array: all samples in contiguous memory */
const samples = new Float64Array(MAX_SAMPLES * SAMPLE_SIZE);
let sampleCount = 0;

/**
 * Record latency sample - data-oriented write
 * @param {number} timestamp
 * @param {number} keypressTime
 * @param {number} audioTime
 */
export function recordSample(timestamp, keypressTime, audioTime) {
  if (sampleCount >= MAX_SAMPLES) return;
  
  const offset = sampleCount * SAMPLE_SIZE;
  samples[offset] = timestamp;
  samples[offset + 1] = keypressTime;
  samples[offset + 2] = audioTime;
  
  sampleCount++;
}

/**
 * Process all samples in one loop - cache-friendly
 * @returns {Object} Statistics
 */
export function analyzeSamples() {
  let totalLatency = 0;
  let minLatency = Infinity;
  let maxLatency = -Infinity;
  
  for (let i = 0; i < sampleCount * SAMPLE_SIZE; i += SAMPLE_SIZE) {
    const latency = samples[i + 2] - samples[i + 1];
    totalLatency += latency;
    minLatency = Math.min(minLatency, latency);
    maxLatency = Math.max(maxLatency, latency);
  }
  
  return {
    avgLatency: totalLatency / sampleCount,
    minLatency,
    maxLatency,
    sampleCount
  };
}

/**
 * Get percentile - requires sort
 * @param {number} percentile 0-100
 * @returns {number}
 */
export function getPercentile(percentile) {
  const latencies = new Float64Array(sampleCount);
  
  for (let i = 0; i < sampleCount; i++) {
    const offset = i * SAMPLE_SIZE;
    latencies[i] = samples[offset + 2] - samples[offset + 1];
  }
  
  latencies.sort();
  const index = Math.floor((percentile / 100) * sampleCount);
  return latencies[index];
}
```

**Data-Oriented Principle:** **Flat array = cache friendly**. No `samples[i].keypressTime`—just `samples[i*3 + 1]`. Sequential memory access is orders of magnitude faster.

**Real-World Measurement:**

```typescript
// latency-test.mjs
import { recordSample, analyzeSamples, getPercentile } from './latency-struct-array.mjs';

/**
 * Measure keypress → audio latency
 */
export async function runLatencyTest(audioBackend) {
  return new Promise((resolve) => {
    let testCount = 0;
    
    document.addEventListener('keydown', async (e) => {
      if (testCount >= 1000) return;
      
      const keypressTime = performance.now();
      
      // Simulate audio callback delay
      const timestamp = performance.now();
      const audioTime = await triggerSound(audioBackend);
      
      recordSample(timestamp, keypressTime, audioTime);
      testCount++;
      
      if (testCount >= 1000) {
        const stats = analyzeSamples();
        stats.p50 = getPercentile(50);
        stats.p95 = getPercentile(95);
        stats.p99 = getPercentile(99);
        resolve(stats);
      }
    });
  });
}
```

**Creative Coding: Profiler Visualizer**

Build a **profiler visualizer** that **renders the flat array as a heatmap**:

```typescript
/**
 * Render latency heatmap - each sample is a pixel
 * @param {CanvasRenderingContext2D} ctx
 */
export function renderLatencyHeatmap(ctx) {
  const width = ctx.canvas.width;
  const height = ctx.canvas.height;
  const pixelsPerSample = Math.floor(width / sampleCount);
  
  for (let i = 0; i < sampleCount; i++) {
    const offset = i * SAMPLE_SIZE;
    const latency = samples[offset + 2] - samples[offset + 1];
    
    // Color: green (fast) → red (slow)
    const normalizedLatency = (latency - 0) / (50 - 0); // 0-50ms range
    const hue = (1 - normalizedLatency) * 120; // 120=green, 0=red
    
    ctx.fillStyle = `hsl(${hue}, 100%, 50%)`;
    ctx.fillRect(
      i * pixelsPerSample,
      0,
      pixelsPerSample,
      height
    );
  }
}

/**
 * Render distribution curve
 * @param {CanvasRenderingContext2D} ctx
 */
export function renderDistributionCurve(ctx) {
  const buckets = new Array(100).fill(0);
  
  for (let i = 0; i < sampleCount; i++) {
    const offset = i * SAMPLE_SIZE;
    const latency = samples[offset + 2] - samples[offset + 1];
    const bucket = Math.floor(latency); // 1ms buckets
    if (bucket < buckets.length) buckets[bucket]++;
  }
  
  const maxCount = Math.max(...buckets);
  const width = ctx.canvas.width;
  const height = ctx.canvas.height;
  
  ctx.beginPath();
  buckets.forEach((count, ms) => {
    const x = (ms / buckets.length) * width;
    const y = height - (count / maxCount) * height;
    ms === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#0f0';
  ctx.stroke();
}
```

**Cache misses show as color shifts**—visual feedback for memory access patterns.

**Web API Mirror: "Web Audio Latency Comparison"**

```typescript
/**
 * Compare X11, Web Audio, WebSocket latencies
 */
export async function compareLatencies() {
  const tests = [
    { name: 'X11', backend: x11Backend },
    { name: 'Web Audio', backend: webAudioBackend },
    { name: 'WebSocket', backend: websocketBackend }
  ];
  
  for (const test of tests) {
    console.log(`Testing ${test.name}...`);
    const stats = await runLatencyTest(test.backend);
    console.log(`  Avg: ${stats.avgLatency.toFixed(2)}ms`);
    console.log(`  P50: ${stats.p50.toFixed(2)}ms`);
    console.log(`  P95: ${stats.p95.toFixed(2)}ms`);
    console.log(`  P99: ${stats.p99.toFixed(2)}ms`);
  }
}
```

**Aha Moment:** When you see Web Audio's "low latency" mode has the same tradeoffs as X11's buffer size tuning—**the laws of physics don't change**.

**Connects to:**
- Unit 1-L1.7 (profiling)
- Unit 1-L1.8 (buffer sizing)
- Unit 7-L7.2 (cache-friendly data)

**Enables:** Lesson 10 (game audio), Unit 3-L3.9 (latency compensation)

---

## Lesson 8: State Machine as Transition Table

**Core Concept:** Your init cascade (`Unit 4-L4.3`) as **array of structs** with function pointers. Logic becomes data.

### Project: Fallback Cascade Simulator

```typescript
// state-machine-table.mjs

/**
 * @typedef {Object} Transition
 * @property {number} fromState
 * @property {number} condition 0=success, -1=failure
 * @property {number} toState
 * @property {string} label For debugging
 */

/** State IDs */
const STATE = {
  INIT: 0,
  TRY_ALSA_DLOPEN: 1,
  TRY_ALSA_INIT: 2,
  ALSA_SUCCESS: 3,
  TRY_PULSE: 4,
  PULSE_SUCCESS: 5,
  TRY_NULL: 6,
  NULL_SUCCESS: 7,
  FAILED: -1
};

/** @type {Transition[]} */
const transitions = [
  // From INIT
  { fromState: STATE.INIT, condition: 0, toState: STATE.TRY_ALSA_DLOPEN, label: 'start' },
  
  // ALSA dlopen attempts
  { fromState: STATE.TRY_ALSA_DLOPEN, condition: 0, toState: STATE.TRY_ALSA_INIT, label: 'dlopen success' },
  { fromState: STATE.TRY_ALSA_DLOPEN, condition: -1, toState: STATE.TRY_PULSE, label: 'dlopen failed' },
  
  // ALSA init attempts
  { fromState: STATE.TRY_ALSA_INIT, condition: 0, toState: STATE.ALSA_SUCCESS, label: 'alsa init success' },
  { fromState: STATE.TRY_ALSA_INIT, condition: -1, toState: STATE.TRY_PULSE, label: 'alsa init failed' },
  
  // PulseAudio fallback
  { fromState: STATE.TRY_PULSE, condition: 0, toState: STATE.PULSE_SUCCESS, label: 'pulse success' },
  { fromState: STATE.TRY_PULSE, condition: -1, toState: STATE.TRY_NULL, label: 'pulse failed' },
  
  // Null backend (always succeeds)
  { fromState: STATE.TRY_NULL, condition: 0, toState: STATE.NULL_SUCCESS, label: 'null backend' }
];

/**
 * Find next state - data-oriented lookup
 * @param {number} currentState
 * @param {number} result
 * @returns {number} nextState
 */
function getNextState(currentState, result) {
  const condition = result === 0 ? 0 : -1;
  const transition = transitions.find(
    t => t.fromState === currentState && t.condition === condition
  );
  return transition ? transition.toState : STATE.FAILED;
}

/**
 * Simulate actions for each state
 * @type {Map<number, function(): number>}
 */
const stateActions = new Map([
  [STATE.TRY_ALSA_DLOPEN, () => Math.random() > 0.1 ? 0 : -1],
  [STATE.TRY_ALSA_INIT, () => Math.random() > 0.2 ? 0 : -1],
  [STATE.TRY_PULSE, () => Math.random() > 0.3 ? 0 : -1],
  [STATE.TRY_NULL, () => 0] // Always succeeds
]);

/**
 * Run state machine - mirrors audio init cascade
 * @param {boolean} verbose
 * @returns {string} Final backend
 */
export function runStateMachine(verbose = false) {
  let currentState = STATE.INIT;
  const executionPath = [];
  
  while (true) {
    if (verbose) console.log(`[STATE] ${getStateName(currentState)}`);
    executionPath.push(currentState);
    
    // Terminal states
    if (currentState === STATE.ALSA_SUCCESS) return 'alsa';
    if (currentState === STATE.PULSE_SUCCESS) return 'pulse';
    if (currentState === STATE.NULL_SUCCESS) return 'null';
    if (currentState === STATE.FAILED) return 'failed';
    
    // Execute action
    const action = stateActions.get(currentState);
    const result = action ? action() : 0;
    
    // Transition
    const nextState = getNextState(currentState, result);
    
    if (verbose && result !== 0) {
      console.log(`  [ERROR] Action failed with code ${result}`);
    }
    
    currentState = nextState;
  }
}

function getStateName(state) {
  return Object.keys(STATE).find(key => STATE[key] === state) || 'UNKNOWN';
}
```

**Data-Oriented Principle:** **Logic is data**—you **interpret** the table, no `if/else` cascade, no switch statements.

**Testable Failure Injection:**

```typescript
/**
 * Force specific failure paths for testing
 * @param {string} failureMode
 */
export function runWithFailure(failureMode) {
  const forcedActions = new Map([
    [STATE.TRY_ALSA_DLOPEN, () => failureMode === 'alsa_dlopen' ? -1 : 0],
    [STATE.TRY_ALSA_INIT, () => failureMode === 'alsa_init' ? -1 : 0],
    [STATE.TRY_PULSE, () => failureMode === 'pulse' ? -1 : 0],
    [STATE.TRY_NULL, () => 0]
  ]);
  
  // Temporarily replace actions
  const originalActions = stateActions;
  Object.setPrototypeOf(stateActions, forcedActions);
  
  const result = runStateMachine(true);
  
  // Restore
  Object.setPrototypeOf(stateActions, originalActions);
  
  return result;
}

// Test all failure paths
console.log('ALSA dlopen fails:', runWithFailure('alsa_dlopen'));
console.log('ALSA init fails:', runWithFailure('alsa_init'));
console.log('Pulse fails:', runWithFailure('pulse'));
```

**Creative Coding: API Request RPG**

```typescript
/**
 * @typedef {Object} QuestNode
 * @property {string} description
 * @property {function(): Promise<number>} action
 * @property {Map<number, number>} transitions result → nextState
 */

const apiQuestStates = new Map([
  [0, {
    description: "Fetch user data from primary server",
    action: () => fetch('/api/user').then(r => r.ok ? 0 : -1),
    transitions: new Map([[0, 1], [-1, 3]])
  }],
  [1, {
    description: "Parse JSON response",
    action: async () => {
      try {
        await response.json();
        return 0;
      } catch {
        return -1;
      }
    },
    transitions: new Map([[0, 2], [-1, 3]])
  }],
  [2, {
    description: "Quest complete! User data retrieved.",
    action: () => Promise.resolve(0),
    transitions: new Map()
  }],
  [3, {
    description: "Primary server failed. Trying cache...",
    action: () => getCachedData().then(() => 0).catch(() => -1),
    transitions: new Map([[0, 2], [-1, 4]])
  }],
  [4, {
    description: "Quest failed. No data available.",
    action: () => Promise.resolve(-1),
    transitions: new Map()
  }]
]);

async function runAPIQuest() {
  let state = 0;
  
  while (apiQuestStates.has(state)) {
    const quest = apiQuestStates.get(state);
    console.log(`[QUEST] ${quest.description}`);
    
    const result = await quest.action();
    const nextState = quest.transitions.get(result);
    
    if (nextState === undefined) break;
    state = nextState;
  }
}
```

**Each step is a quest. Network failure = quest failed**, fallback to cached data (like your audio stubs).

**Connects to:**
- Unit 1-L1.6 (error handling)
- Unit 2-L2.3 (WASAPI sessions)
- Unit 4-L4.3 (init cascade)

**Enables:** Lesson 10 (robust game audio), Unit 3-L3.8 (robust abstractions)

---

## Lesson 9: WASM Memory as Raw Byte Array

**Core Concept:** **Manually allocate** WASM memory for audio—no Emscripten helpers. Treat WASM linear memory as a giant `Uint8Array`.

### Project: Manual WASM Audio Processor

```typescript
// wasm-raw.mjs

const WASM_PAGE_SIZE = 65536;

/**
 * Allocate memory in WASM - manual malloc
 * @param {WebAssembly.Instance} instance
 * @param {number} sampleCount
 * @returns {number} Pointer to allocated buffer
 */
export function wasmAlloc(instance, sampleCount) {
  const bytesNeeded = sampleCount * 4; // Float32 = 4 bytes
  const mallocPtr = instance.exports.malloc(bytesNeeded);
  return mallocPtr;
}

/**
 * Write JS array to WASM memory - manual memcpy
 * @param {WebAssembly.Instance} instance
 * @param {number} ptr
 * @param {Float32Array} jsArray
 */
export function wasmWrite(instance, ptr, jsArray) {
  const wasmMemory = new Float32Array(
    instance.exports.memory.buffer,
    ptr,
    jsArray.length
  );
  wasmMemory.set(jsArray);
}

/**
 * Read WASM memory to JS array
 * @param {WebAssembly.Instance} instance
 * @param {number} ptr
 * @param {number} length
 * @returns {Float32Array}
 */
export function wasmRead(instance, ptr, length) {
  return new Float32Array(
    instance.exports.memory.buffer,
    ptr,
    length
  );
}

/**
 * Free WASM memory
 * @param {WebAssembly.Instance} instance
 * @param {number} ptr
 */
export function wasmFree(instance, ptr) {
  instance.exports.free(ptr);
}
```

**Corresponding C Code (compiled to WASM):**

```c
// audio-processor.c
#include <stdlib.h>
#include <math.h>

// Export malloc/free for JS
__attribute__((export_name("malloc")))
void* audio_malloc(size_t size) {
    return malloc(size);
}

__attribute__((export_name("free")))
void audio_free(void* ptr) {
    free(ptr);
}

// Process audio samples
__attribute__((export_name("process_audio")))
void process_audio(float* samples, int count, float volume) {
    for (int i = 0; i < count; i++) {
        samples[i] *= volume;
    }
}

// Generate sine wave
__attribute__((export_name("generate_sine")))
void generate_sine(float* output, int count, float frequency, float sample_rate) {
    for (int i = 0; i < count; i++) {
        output[i] = sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
}
```

**Usage from JavaScript:**

```typescript
// Load and use WASM module
async function initWASMAudio() {
  const response = await fetch('audio-processor.wasm');
  const buffer = await response.arrayBuffer();
  const module = await WebAssembly.instantiate(buffer);
  const instance = module.instance;
  
  // Allocate memory for 1024 samples
  const ptr = wasmAlloc(instance, 1024);
  
  // Generate sine wave in WASM
  instance.exports.generate_sine(ptr, 1024, 440, 48000);
  
  // Read result back to JS
  const samples = wasmRead(instance, ptr, 1024);
  console.log('Generated samples:', samples);
  
  // Process audio (volume control)
  instance.exports.process_audio(ptr, 1024, 0.5);
  
  // Read processed result
  const processed = wasmRead(instance, ptr, 1024);
  console.log('Processed samples:', processed);
  
  // Free memory
  wasmFree(instance, ptr);
}
```

**Data-Oriented Principle:** **You manage pointers**—no `instance.exports.generateSine()` that returns an object. You **pass a pointer**, get raw bytes back.

**Creative Coding: Memory Allocator Visualizer**

Build a **memory allocator visualizer**—show WASM memory as a **vertical tower of blocks**:

```typescript
/**
 * Track WASM allocations
 */
class WASMAllocTracker {
  constructor(instance) {
    this.instance = instance;
    this.allocations = new Map(); // ptr → size
    this.totalAllocated = 0;
  }
  
  malloc(size) {
    const ptr = this.instance.exports.malloc(size);
    this.allocations.set(ptr, size);
    this.totalAllocated += size;
    return ptr;
  }
  
  free(ptr) {
    const size = this.allocations.get(ptr);
    if (size) {
      this.instance.exports.free(ptr);
      this.allocations.delete(ptr);
      this.totalAllocated -= size;
    }
  }
  
  /**
   * Render memory layout
   * @param {CanvasRenderingContext2D} ctx
   */
  visualize(ctx) {
    const width = ctx.canvas.width;
    const height = ctx.canvas.height;
    const memorySize = this.instance.exports.memory.buffer.byteLength;
    
    // Draw free space
    ctx.fillStyle = '#222';
    ctx.fillRect(0, 0, width, height);
    
    // Draw allocations
    let currentY = 0;
    const sortedAllocs = Array.from(this.allocations.entries())
      .sort((a, b) => a[0] - b[0]);
    
    for (const [ptr, size] of sortedAllocs) {
      const blockHeight = (size / memorySize) * height;
      const hue = (ptr % 360);
      
      ctx.fillStyle = `hsl(${hue}, 70%, 50%)`;
      ctx.fillRect(0, currentY, width, blockHeight);
      
      // Label
      ctx.fillStyle = '#fff';
      ctx.font = '10px monospace';
      ctx.fillText(`${ptr}: ${size}B`, 5, currentY + 12);
      
      currentY += blockHeight;
    }
    
    // Stats
    ctx.fillStyle = '#fff';
    ctx.fillText(
      `Total: ${this.totalAllocated}/${memorySize} bytes`,
      5,
      height - 10
    );
  }
}
```

**Malloc = carve out block, free = return block. Fragmentation shows as Swiss cheese**.

**Web API Mirror: "AudioWorklet WASM Integration"**

```typescript
// audio-worklet-wasm.js
class WASMWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.wasmInstance = null;
    this.samplePtr = null;
    
    this.port.onmessage = (e) => {
      if (e.data.type === 'init') {
        this.initWASM(e.data.wasmModule);
      }
    };
  }
  
  async initWASM(wasmModule) {
    const instance = await WebAssembly.instantiate(wasmModule);
    this.wasmInstance = instance.instance;
    this.samplePtr = wasmAlloc(this.wasmInstance, 128);
  }
  
  process(inputs, outputs, parameters) {
    if (!this.wasmInstance) return true;
    
    const output = outputs[0][0];
    
    // Generate in WASM
    this.wasmInstance.exports.generate_sine(
      this.samplePtr,
      128,
      440,
      sampleRate
    );
    
    // Copy to output
    const wasmSamples = wasmRead(this.wasmInstance, this.samplePtr, 128);
    output.set(wasmSamples);
    
    return true;
  }
}

registerProcessor('wasm-processor', WASMWorkletProcessor);
```

**Aha Moment:** When you realize WASM's linear memory is just a giant `Uint8Array` that maps to C's heap—**it's the same memory model**.

**Connects to:**
- Unit 1-L1.10 (unit tests)
- Unit 3-L3.8 (WASM integration)
- Lesson 3 (worklets)

**Enables:** Lesson 10 (game audio), Unit 6-L6.4 (asset streaming)

---

## Lesson 10: ECS Audio Manager (Ultimate Data-Oriented)

**Core Concept:** **Entity-Component-System** for game audio—**no OOP, just arrays of structs**. This is the final synthesis of everything.

### Project: Data-Oriented Game Audio System

```typescript
// ecs-audio.mjs

/** Maximum concurrent sounds */
const MAX_SOUNDS = 256;

/** Component arrays - Structure of Arrays (SoA) */
const soundPositionsX = new Float32Array(MAX_SOUNDS);
const soundPositionsY = new Float32Array(MAX_SOUNDS);
const soundPositionsZ = new Float32Array(MAX_SOUNDS);
const soundPriorities = new Int32Array(MAX_SOUNDS);
const soundSampleOffsets = new Int32Array(MAX_SOUNDS);
const soundVolumes = new Float32Array(MAX_SOUNDS);
const soundIsActive = new Uint8Array(MAX_SOUNDS);

/** Sample data storage - like your sample_buffer */
const soundSampleData = new Array(MAX_SOUNDS);

/** Active sound count */
let activeSoundCount = 0;

/**
 * Spawn new sound entity
 * @param {Float32Array} sampleData
 * @param {number} x
 * @param {number} y
 * @param {number} z
 * @param {number} priority
 * @returns {number} Entity ID or -1 if full
 */
export function spawnSound(sampleData, x, y, z, priority) {
  if (activeSoundCount >= MAX_SOUNDS) {
    // Evict lowest priority sound
    let lowestPriority = Infinity;
    let lowestIndex = -1;
    
    for (let i = 0; i < activeSoundCount; i++) {
      if (soundPriorities[i] < lowestPriority) {
        lowestPriority = soundPriorities[i];
        lowestIndex = i;
      }
    }
    
    if (priority <= lowestPriority) return -1; // Can't evict
    
    // Evict and reuse slot
    const id = lowestIndex;
    soundSampleData[id] = sampleData;
    soundPositionsX[id] = x;
    soundPositionsY[id] = y;
    soundPositionsZ[id] = z;
    soundPriorities[id] = priority;
    soundSampleOffsets[id] = 0;
    soundVolumes[id] = 1.0;
    soundIsActive[id] = 1;
    
    return id;
  }
  
  // New sound
  const id = activeSoundCount++;
  soundSampleData[id] = sampleData;
  soundPositionsX[id] = x;
  soundPositionsY[id] = y;
  soundPositionsZ[id] = z;
  soundPriorities[id] = priority;
  soundSampleOffsets[id] = 0;
  soundVolumes[id] = 1.0;
  soundIsActive[id] = 1;
  
  return id;
}

/**
 * Mix all active sounds - mirrors L3.2: Pull System + L9.1: Mixer
 * @param {Float32Array} outputMix Output buffer
 * @param {number} listenerX
 * @param {number} listenerY
 * @param {number} listenerZ
 */
export function mixSounds(outputMix, listenerX, listenerY, listenerZ) {
  // Clear output
  outputMix.fill(0);
  
  // Mix all active sounds - data-oriented loop
  for (let i = 0; i < activeSoundCount; i++) {
    if (!soundIsActive[i]) continue;
    
    const sampleData = soundSampleData[i];
    if (!sampleData) continue;
    
    // Calculate 3D volume based on distance
    const dx = soundPositionsX[i] - listenerX;
    const dy = soundPositionsY[i] - listenerY;
    const dz = soundPositionsZ[i] - listenerZ;
    const distance = Math.sqrt(dx*dx + dy*dy + dz*dz);
    const volume = soundVolumes[i] / (1 + distance * 0.1);
    
    // Mix samples
    let offset = soundSampleOffsets[i];
    for (let j = 0; j < outputMix.length && offset < sampleData.length; j++, offset++) {
      outputMix[j] += sampleData[offset] * volume;
    }
    
    // Update offset
    soundSampleOffsets[i] = offset;
    
    // Deactivate if finished
    if (offset >= sampleData.length) {
      soundIsActive[i] = 0;
    }
  }
  
  // Compact active sounds (remove finished ones)
  compactSounds();
}

/**
 * Remove finished sounds - defragmentation
 */
function compactSounds() {
  let writeIndex = 0;
  
  for (let readIndex = 0; readIndex < activeSoundCount; readIndex++) {
    if (soundIsActive[readIndex]) {
      if (writeIndex !== readIndex) {
        // Move sound data
        soundSampleData[writeIndex] = soundSampleData[readIndex];
        soundPositionsX[writeIndex] = soundPositionsX[readIndex];
        soundPositionsY[writeIndex] = soundPositionsY[readIndex];
        soundPositionsZ[writeIndex] = soundPositionsZ[readIndex];
        soundPriorities[writeIndex] = soundPriorities[readIndex];
        soundSampleOffsets[writeIndex] = soundSampleOffsets[readIndex];
        soundVolumes[writeIndex] = soundVolumes[readIndex];
        soundIsActive[writeIndex] = soundIsActive[readIndex];
      }
      writeIndex++;
    }
  }
  
  activeSoundCount = writeIndex;
}

/**
 * Update listener position for dynamic mixing
 * @param {number} x
 * @param {number} y
 * @param {number} z
 */
export function updateListener(x, y, z) {
  // Store for next mix call
  this.listenerPosition = { x, y, z };
}
```

**Data-Oriented Principle:** **No `class Sound { }`**—sounds are **indices into parallel arrays**. **Cache-friendly**: `soundPositionsX`, `soundPositionsY`, `soundPositionsZ` are contiguous in memory.

**Game Integration Example:**

```typescript
// game.mjs
import { spawnSound, mixSounds } from './ecs-audio.mjs';
import { gameLoopSimulator } from './game-loop-data.mjs';
import { ringBufferWrite } from './ring-buffer-raw.mjs';

/**
 * Game with integrated audio
 */
export function runGame() {
  const loop = gameLoopSimulator(60);
  const outputBuffer = new Float32Array(800); // ~60fps at 48kHz
  
  let playerX = 0;
  let playerY = 0;
  let playerZ = 0;
  
  function gameFrame() {
    const frame = loop.next().value;
    
    // Game logic
    playerX += 0.1;
    
    // Spawn sound on keypress
    document.addEventListener('keydown', (e) => {
      if (e.key === ' ') {
        const gunshot = generateGunshot();
        spawn Sound(gunshot, playerX + 1, playerY, playerZ, 10);
      }
    });
    
    // Mix audio
    mixSounds(outputBuffer, playerX, playerY, playerZ);
    
    // Write to audio backend
    ringBufferWrite(audioRingBuffer, outputBuffer);
    
    requestAnimationFrame(gameFrame);
  }
  
  gameFrame();
}
```

**Creative Coding: Data Layout Inspector**

```typescript
/**
 * Visualize ECS data layout
 * @param {CanvasRenderingContext2D} ctx
 */
export function visualizeECSLayout(ctx) {
  const width = ctx.canvas.width;
  const height = ctx.canvas.height;
  const barHeight = height / 8;
  
  const arrays = [
    { name: 'posX', data: soundPositionsX, color: '#f00' },
    { name: 'posY', data: soundPositionsY, color: '#0f0' },
    { name: 'posZ', data: soundPositionsZ, color: '#00f' },
    { name: 'priority', data: soundPriorities, color: '#ff0' },
    { name: 'offset', data: soundSampleOffsets, color: '#f0f' },
    { name: 'volume', data: soundVolumes, color: '#0ff' },
    { name: 'active', data: soundIsActive, color: '#fff' }
  ];
  
  arrays.forEach((arr, idx) => {
    const y = idx * barHeight;
    
    // Draw bar background
    ctx.fillStyle = '#222';
    ctx.fillRect(0, y, width, barHeight);
    
    // Draw label
    ctx.fillStyle = arr.color;
    ctx.font = '12px monospace';
    ctx.fillText(arr.name, 5, y + 15);
    
    // Draw data cells
    for (let i = 0; i < activeSoundCount; i++) {
      const x = 100 + (i * 20);
      const value = arr.data[i];
      const normalizedValue = typeof value === 'number' ? value / 255 : 0;
      
      ctx.fillStyle = arr.color;
      ctx.globalAlpha = 0.3 + normalizedValue * 0.7;
      ctx.fillRect(x, y + 20, 18, barHeight - 25);
    }
    ctx.globalAlpha = 1.0;
  });
}
```

**Hover over index 5, see all properties**—like a **memory debugger** for your audio ECS.

**Final Synthesis:** **This is how your C code works**—`GameSoundOutput` is a struct, `active_sounds[]` is an array. **You've rebuilt it in JS, but data-oriented**.

**Web API Mirror: "Web Audio ECS"**

Instead of `new AudioBuffer()`, store **all audio data in one giant `Float32Array`**:

```typescript
// One big buffer, offset-based addressing
const audioHeap = new Float32Array(48000 * 60); // 1 minute of audio
const soundOffsets = new Uint32Array(256); // Each sound's offset in heap
const soundLengths = new Uint32Array(256);

/**
 * Load sound into heap
 * @param {Float32Array} sampleData
 * @returns {number} Sound ID
 */
function loadSoundToHeap(sampleData) {
  const id = nextSoundID++;
  const offset = currentHeapOffset;
  
  audioHeap.set(sampleData, offset);
  soundOffsets[id] = offset;
  soundLengths[id] = sampleData.length;
  
  currentHeapOffset += sampleData.length;
  
  return id;
}
```

**This is exactly how Unity's Burst compiler lays out data**—you're learning professional game dev patterns that bridge to low-level C.

**Connects to:**
- EVERYTHING—this is the synthesis project
- Unit 3-L3.2 (Pull System)
- Unit 6-L6.2 (3D positioning)
- Unit 9-L9.1 (Mixer Architecture)

**Enables:** Your independence as a game audio programmer

---

## Lesson 11: WebSocket Audio Bridge (Networking Reality Check)

**Core Concept:** Build a WebSocket server that wraps your audio system and streams to a browser. Experience the **same latency issues** as X11 over the network.

### Project: Real-Time Audio Streaming

```typescript
// websocket-audio-server.mjs
import { WebSocketServer } from 'ws';
import { createRingBuffer, ringBufferRead } from './ring-buffer-raw.mjs';

const audioRingBuffer = createRingBuffer(4096);
const wss = new WebSocketServer({ port: 8080 });

wss.on('connection', (ws) => {
  console.log('[WS] Client connected');
  
  // Stream audio at ~60fps
  const streamInterval = setInterval(() => {
    const samples = new Float32Array(800);
    const framesRead = ringBufferRead(audioRingBuffer, samples);
    
    if (framesRead > 0) {
      // Send as binary
      ws.send(samples.buffer);
    }
  }, 16.666); // ~60fps
  
  ws.on('close', () => {
    clearInterval(streamInterval);
    console.log('[WS] Client disconnected');
  });
  
  // Disable Nagle's algorithm - like Unit 1-L1.6
  ws._socket.setNoDelay(true);
});
```

**Client Side:**

```typescript
// websocket-audio-client.mjs
const ws = new WebSocket('ws://localhost:8080');
const audioContext = new AudioContext();
let scriptNode;

ws.binaryType = 'arraybuffer';

ws.onopen = () => {
  console.log('[WS] Connected to audio server');
  
  scriptNode = audioContext.createScriptProcessor(1024, 0, 2);
  let pendingBuffer = [];
  
  ws.onmessage = (event) => {
    const samples = new Float32Array(event.data);
    pendingBuffer.push(...samples);
  };
  
  scriptNode.onaudioprocess = (e) => {
    const output = e.outputBuffer.getChannelData(0);
    
    for (let i = 0; i < output.length; i++) {
      output[i] = pendingBuffer.shift() || 0;
    }
  };
  
  scriptNode.connect(audioContext.destination);
};
```

**Aha Moment:** When TCP Nagle's algorithm introduces the same latency as X11's buffering, and you fix it with `socket.setNoDelay()` (Unit 1-L1.6).

**Latency Measurement:**

```typescript
/**
 * Measure network latency
 */
function measureWebSocketLatency(ws) {
  const latencies = [];
  
  for (let i = 0; i < 100; i++) {
    const sendTime = performance.now();
    
    ws.send(JSON.stringify({ type: 'ping', timestamp: sendTime }));
    
    ws.once('message', (data) => {
      const msg = JSON.parse(data);
      if (msg.type === 'pong') {
        const latency = performance.now() - msg.timestamp;
        latencies.push(latency);
      }
    });
  }
  
  return latencies;
}
```

**Connects to:**
- Unit 1-L1.1 (pipeline)
- Unit 1-L1.6 (non-blocking I/O)
- Lesson 1 (function tables)
- Lesson 7 (latency measurement)

**Enables:** Lesson 12 (complete game audio)

---

## Lesson 12: Complete Game Audio Manager (Final Boss)

**Core Concept:** Build a TypeScript audio manager for a hypothetical game that **uses all patterns** from the entire unit.

### Project: Production-Ready Game Audio System

```typescript
// game-audio-manager.mjs
import { loadBackend } from './function-table.mjs';
import { createRingBuffer, ringBufferWrite } from './ring-buffer-raw.mjs';
import { gameLoopSimulator } from './game-loop-data.mjs';
import { runStateMachine } from './state-machine-table.mjs';
import { spawnSound, mixSounds } from './ecs-audio.mjs';
import { recordSample, analyzeSamples } from './latency-struct-array.mjs';

/**
 * @typedef {Object} AudioManagerConfig
 * @property {number} sampleRate
 * @property {number} channels
 * @property {number} bufferSize
 * @property {number} targetFps
 * @property {boolean} enableFallbacks
 */

/**
 * Complete game audio manager
 */
export class GameAudioManager {
  constructor() {
    /** @type {AudioBackendVTable|null} */
    this.backend = null;
    
    /** @type {RingBuffer|null} */
    this.ringBuffer = null;
    
    /** @type {Generator|null} */
    this.gameLoop = null;
    
    this.listenerPosition = { x: 0, y: 0, z: 0 };
  }
  
  /**
   * Initialize audio system with fallbacks
   * @param {AudioManagerConfig} config
   * @returns {Promise<string>} Backend used
   */
  async init(config) {
    console.log('[AudioManager] Initializing...');
    
    // Run state machine to select backend
    const backendName = config.enableFallbacks
      ? runStateMachine(true)
      : 'alsa';
    
    console.log(`[AudioManager] Selected backend: ${backendName}`);
    
    // Load backend function table
    await loadBackend(`./backends/${backendName}-backend.mjs`);
    this.backend = g_audioBackend;
    
    // Initialize backend
    const handle = this.backend.init('default');
    if (handle < 0) {
      throw new Error('Backend init failed');
    }
    
    // Create ring buffer
    this.ringBuffer = createRingBuffer(config.bufferSize);
    
    // Start game loop
    this.gameLoop = gameLoopSimulator(config.targetFps);
    
    return backendName;
  }
  
  /**
   * Main audio frame - call this every game frame
   */
  processFrame() {
    if (!this.gameLoop || !this.backend || !this.ringBuffer) return;
    
    const frame = this.gameLoop.next().value;
    const startTime = performance.now();
    
    // Allocate output buffer
    const outputMix = new Float32Array(frame.samplesToWrite);
    
    // Mix all active sounds (ECS)
    mixSounds(
      outputMix,
      this.listenerPosition.x,
      this.listenerPosition.y,
      this.listenerPosition.z
    );
    
    // Write to ring buffer
    const written = ringBufferWrite(this.ringBuffer, outputMix);
    
    // Record latency sample
    const audioTime = performance.now();
    recordSample(performance.now(), startTime, audioTime);
    
    // Update metrics
    frame.performanceMetrics.cpuTime = performance.now() - startTime;
    frame.performanceMetrics.bufferAvailable = written;
    
    // Get next frame
    const actualDelta = performance.now() - startTime;
    this.gameLoop.next(actualDelta);
  }
  
  /**
   * Play sound at position
   * @param {Float32Array} sampleData
   * @param {number} x
   * @param {number} y
   * @param {number} z
   * @param {number} priority
   */
  playSound(sampleData, x, y, z, priority = 5) {
    return spawnSound(sampleData, x, y, z, priority);
  }
  
  /**
   * Update listener (camera) position
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  setListenerPosition(x, y, z) {
    this.listenerPosition = { x, y, z };
  }
  
  /**
   * Get performance stats
   */
  getStats() {
    return analyzeSamples();
  }
  
  /**
   * Shutdown
   */
  shutdown() {
    if (this.backend) {
      this.backend.shutdown(0);
    }
  }
}
```

**Usage in Game:**

```typescript
// game.mjs
const audioManager = new GameAudioManager();

await audioManager.init({
  sampleRate: 48000,
  channels: 2,
  bufferSize: 4096,
  targetFps: 60,
  enableFallbacks: true
});

// Game loop
function gameLoop() {
  // Update player
  player.x += player.velocityX;
  player.y += player.velocityY;
  
  // Update audio listener
  audioManager.setListenerPosition(player.x, player.y, 0);
  
  // Handle input
  if (input.isKeyPressed('SPACE')) {
    const gunshot = loadSound('gunshot.wav');
    audioManager.playSound(gunshot, player.x + 1, player.y, 0, 10);
  }
  
  // Process audio frame
  audioManager.processFrame();
  
  requestAnimationFrame(gameLoop);
}

gameLoop();
```

**Success Metrics:**
- ✅ Works in browser (WebSocket), Node.js (direct), Electron (hybrid)
- ✅ Spatial audio with 3D positioning
- ✅ Dynamic mixing with priority channels
- ✅ Graceful fallbacks when backends fail
- ✅ Latency compensation with predictive scheduling
- ✅ **You can debug it without LLM help**

**Final Reflection Document:**

Write a technical document explaining why this implementation makes different tradeoffs than libraries like Howler.js:

1. **Buffer Management:** Manual ring buffer vs. Web Audio's internal buffering
2. **Latency:** Direct backend access vs. browser abstraction layers
3. **Fallbacks:** Explicit state machine vs. hidden fallback logic
4. **Memory Layout:** Structure of Arrays (SoA) vs. Array of Structures (AoS)
5. **Platform Constraints:** Why X11 ALSA differs from Web Audio API

**Cite specific code** from throughout the unit to support your arguments.

---

## Meta-Lesson: The "Why Not Just Use Howler.js?" Reckoning

**Culminating Reflection:**

After completing all lessons, you should be able to articulate:

1. **Why manual buffer management matters** (Lesson 2)
2. **How function tables enable hot-swapping** (Lesson 1)
3. **When data-oriented design improves performance** (Lessons 7, 10)
4. **What tradeoffs browser APIs hide from you** (Lessons 3, 11)
5. **How to debug audio issues from first principles** (All lessons)

When someone asks "why not just use X?", you can now explain the platform constraints, latency requirements, and architectural differences **with technical specificity**.

**You've learned to think like Casey Muratori**—data-oriented, no magic, full understanding.