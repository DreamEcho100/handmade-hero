# Audio Learning Units Tracker

> **Started:** 2026-01-14  
> **Goal:** Transform from cargo-culting to competent systems audio programmer  
> **Progress:** 0/9 units completed

---

## Unit 1: Dynamic Library Loading [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 3 (Can modify)

### Lessons

- [x] L1.1: Dynamic Linker Basics
  - Understand dlopen, dlsym, dlclose
  - Compare to LoadLibrary/GetProcAddress (Windows)
  - When/why to use dynamic vs static linking
- [x] L1.2: Symbol Tables & Resolution
  - What's a symbol table?
  - How dlsym finds functions by name
  - Why `*(void **)(&...)` cast is needed
- [x] L1.3: Stub Functions Pattern
  - Why stubs return errors instead of crashing
  - Graceful degradation strategy
  - Testing without real library
- [x] L1.4: Macro Pattern (Casey's Style)
  - `#define ALSA_SND_PCM_OPEN(name)` pattern
  - Why macros over function pointers directly
  - Code hygiene benefits

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- None yet

---

## Unit 2: ALSA Architecture [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 3 (Can modify)

### Lessons

- [x] L2.1: Kernel vs Userspace Audio
  - Why audio lives in kernel
  - What ALSA actually IS
  - System call overhead
- [x] L2.2: PCM Audio Basics
  - What's PCM (Pulse Code Modulation)?
  - Sample rate, bit depth, channels
  - Why 48kHz? Why 16-bit?
- [x] L2.3: ALSA Initialization
  - `snd_pcm_open()` parameters
  - `snd_pcm_set_params()` all-in-one config
  - Device strings ("default", "hw:0,0")
- [x] L2.4: Ring Buffers (ALSA's, not yours)
  - ALSA manages the ring buffer
  - Period size vs buffer size
  - Where your data goes after writei()
- [x] L2.5: Sample Formats
  - S16_LE (signed 16-bit little-endian)
  - Interleaved (L-R-L-R) vs non-interleaved
  - Why this matters for debugging
- [x] L2.6: Latency Calculation
  - Why `game_update_hz * FRAMES_OF_AUDIO_LATENCY`?
  - Microseconds vs frames conversion
  - Tuning latency for different machines

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Depends on Unit 1 completion

---

## Unit 3: Frame-Aligned Audio (Day 19 Pattern) [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 4 (Can debug)

### Lessons

- [x] L3.1: Game Loop Timing
  - Fixed timestep for game logic
  - Variable timestep for rendering
  - Why audio follows game logic, not rendering
- [x] L3.2: Pull System Explained
  - Game pulls audio (not audio callback pushing)
  - Why Casey prefers pull over push
  - Timing guarantees
- [x] L3.3: Underrun & Overrun
  - What's EPIPE? (-32 error code)
  - Detecting underruns (snd_pcm_recover)
  - Click/pop causes
- [x] L3.4: Avail vs Delay
  - `snd_pcm_avail()` = writeable frames
  - `snd_pcm_delay()` = queued frames
  - Mapping to DirectSound PlayCursor/WriteCursor
- [x] L3.5: Adaptive FPS Integration
  - Audio latency changes with FPS
  - 60 FPS = 33ms/frame = 66ms audio buffer
  - 120 FPS = 8ms/frame = 16ms audio buffer

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Depends on Unit 2 completion

---

## Unit 4: Debugging Audio [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 4 (Can debug)

### Lessons

- [x] L4.1: Error Code Decoding
  - errno values (EPIPE, EAGAIN, EBADFD)
  - SndStrerror() usage
  - Logging strategies
- [x] L4.2: Day 9 vs Day 10 Modes
  - Availability-only mode (Day 9)
  - Latency-aware mode (Day 10)
  - Feature detection at runtime
- [x] L4.3: Initialization Failures
  - "No such device" debugging
  - PulseAudio conflicts
  - Permissions issues
- [x] L4.4: Click Detection
  - 50% write size change heuristic
  - Frame time irregularities
  - Buffer underrun patterns
- [x] L4.5: Debug Visualization
  - Audio marker system
  - Play cursor vs write cursor
  - Expected vs actual flip timing

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Depends on Unit 3 completion

---

## Unit 5: Audio Content Generation [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 5 (Can reimplement)

### Lessons

- [x] L5.1: Platform Abstraction API
  - GameSoundOutput structure design
  - Why separate platform from game
  - Testing game code without platform
- [x] L5.2: Buffer Design Patterns
  - Linear vs ring buffer (yours vs ALSA's)
  - sample_buffer allocation strategy
  - Frame-aligned sizing
- [x] L5.3: Sine Wave Math
  - `sin(2π * frequency * time)`
  - Phase accumulation vs recalculation
  - Sample-accurate frequency
- [x] L5.4: Stereo Panning
  - L/R channel mixing
  - Pan position (-100 to +100)
  - Volume scaling per channel
- [x] L5.5: Memory Management
  - PlatformMemoryBlock pattern
  - platform_allocate_memory
  - Alignment requirements (ALSA doesn't care, but CPU does)

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Depends on Unit 4 completion

---

## Unit 6: Cross-Platform Audio [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 5 (Can reimplement)

### Lessons

- [x] L6.1: Audio Backend Comparison
  - ALSA vs PulseAudio vs JACK
  - DirectSound vs WASAPI vs XAudio2
  - CoreAudio (macOS)
  - Web Audio API
- [x] L6.2: PulseAudio Port Exercise
  - Replace snd*pcm*_ with pa*simple*_
  - Latency handling differences
  - Testing both backends
- [x] L6.3: Web Audio Port (Emscripten)
  - Callback-based (not pull-based)
  - AudioWorklet vs ScriptProcessor
  - Latency constraints in browser
- [x] L6.4: Architectural Tradeoffs
  - When to use each backend
  - Latency vs complexity
  - Platform availability
- [x] L6.5: Real Debugging Session
  - Fix actual audio bug in your code
  - Use tools: strace, perf, gdb
  - Document root cause

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Depends on Unit 5 completion

---

## Unit 7: Real-Time Systems (Advanced) [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 4 (Can debug)

### Lessons

- [ ] L7.1: Linux Scheduling Classes
  - SCHED_FIFO vs SCHED_OTHER
  - When real-time scheduling helps
  - Permissions required
- [ ] L7.2: CPU Affinity
  - Pinning threads to cores
  - Cache coherency issues
  - When this matters (spoiler: rarely)
- [ ] L7.3: Priority Inversion
  - Classic RT problem
  - How it affects audio
  - Mitigation strategies

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Optional unit (not required for competence)

---

## Unit 8: Performance Optimization [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 4 (Can debug)

### Lessons

- [ ] L8.1: Profiling with perf
  - Flamegraphs for audio code
  - Finding hotspots
  - Cache miss analysis
- [ ] L8.2: SIMD for Audio
  - SSE/AVX for sample mixing
  - Auto-vectorization hints
  - When premature optimization hurts
- [ ] L8.3: Lock-Free Ring Buffers
  - Single-producer/single-consumer
  - Memory barriers
  - When needed (not for your current code)

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Optional unit (not required for competence)

---

## Unit 9: Production Audio [NOT STARTED]

**Status:** 🔴 Not Started  
**Target Competence:** Level 5 (Can reimplement)

### Lessons

- [ ] L9.1: Mixer Architecture
  - Multiple sound sources
  - Volume per source
  - Summing without clipping
- [ ] L9.2: WAV File Loading
  - RIFF format parsing
  - Supporting different formats
  - Memory mapping large files
- [ ] L9.3: Event System Integration
  - Triggering sounds from game events
  - Positional audio basics
  - Audio/video sync

### Metrics

- **Confidence:** 0/10
- **Time Spent:** 0 hours
- **Aha Moments:** None yet
- **Code Modified:** None

### Blockers

- Depends on Unit 5 completion

---

## Overall Progress

### Timeline

- **Week 1-2:** Units 1-2 (Foundation)
- **Week 3-4:** Unit 3 (Frame timing)
- **Week 5-6:** Unit 4 (Debugging)
- **Week 7-8:** Unit 5 (Content generation)
- **Week 9-10:** Unit 6 (Cross-platform)
- **Week 11-12:** Units 7-9 (Advanced topics, optional)

### Key Milestones

- [ ] **Milestone 1:** Can explain every line in audio.c
- [ ] **Milestone 2:** Can modify latency without breaking audio
- [ ] **Milestone 3:** Can debug audio clicks independently
- [ ] **Milestone 4:** Can implement PulseAudio backend from scratch
- [ ] **Milestone 5:** Can design audio system for new platform

### Study Log

#### Session 1 (2026-01-14)

- Created UNITS-TRACKER.md
- Next: Read Unit 1, Lesson 1 content

---

## Notes & Reflections

### Web Dev → Systems Programming Mindset Shifts

1. **Error Handling:** Not exceptions, but return codes
2. **Memory:** Not garbage collected, but manually managed
3. **I/O:** Not async/await, but blocking/polling
4. **Imports:** Not compile-time, but runtime (dlsym)
5. **Buffers:** Not auto-resizing, but fixed-size

### Questions to Answer Later

- Why do some Linux systems need PulseAudio emulation?
- What's the actual CPU cost of dlsym() calls?
- How does ALSA ring buffer differ from mine?
- Why does Casey prefer pull over push?
- What's the minimum safe latency on modern hardware?

### Resources Collected

- ALSA project documentation
- Casey's Handmade Hero Days 7-20 videos
- Linux Audio Developers Guide
- Web Audio API spec (for comparison)

---

**Next Action:** Start Unit 1, Lesson 1 when ready!
