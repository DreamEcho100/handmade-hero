**Here is your fully customized, ultra-beginner-friendly Handmade Hero Roadmap**, tailored to:

* You’re new to **C**
* You’re new to **Linux / Ubuntu**
* You’re building **your own platform layer**
* You want **X11 backend** + **Raylib backend**
* You can put in ~**2 hours/day, 5 days/week** (≈10 hrs/week)
* You want to actually *understand* everything, not just copy Casey

This roadmap is designed to take about **2.5–3 years**, the realistic pace for what you want to do.

Let’s go.

---

# 🧭 **THE HANDMADE HERO ROADMAP**

### (Beginner → Engine Architect → Confident C Programmer)

---

# 📍 **PHASE 0 — Prep Phase (1–2 months)**

“Become dangerous in C and Ubuntu.”

### 🎯 Goals

* You can use the terminal confidently
* You can compile C programs with `clang` and `gcc`
* You know pointers, structs, memory basics
* You understand how Makefiles work
* You know how to debug a crash

### ✔️ Step-by-step

#### **1. Linux / Ubuntu Basics**

* Learn: `cd`, `ls`, `cp`, `mv`, `rm`, `apt`, `sudo`, `chmod`
* Learn how to:

  * Install packages
  * Compile from terminal
  * Work with files & directories
  * Use VS Code or Neovim efficiently

#### **2. C Language Basics**

Not a full course — just enough to follow Casey.

Focus on:

* `struct`, arrays, pointers
* `typedef` and function pointers
* manual memory (stack vs heap)
* header / source separation
* building with `clang main.c -o main`

Do NOT learn "modern OOP C" or "C++ patterns".
Stick to: **C89, simple, minimalistic.**

#### **3. Essential Tools**

* Install:

  * `clang`
  * `gdb`
  * `make`
  * `valgrind`
  * `lldb` (optional)
  * Raylib development headers

Test that **VSCode C/C++ extension** works OR **Neovim + clangd**.

---

# 📍 **PHASE 1 — Base Project & Platform Layer Skeleton (2–3 months)**

“You build Casey’s Linux layer *before* following him.”

### 🎯 Goals

* You understand "platform layer" boundaries
* You can create a window in X11
* You can receive input events
* You can do timing & large file reading
* Raylib backend is created in parallel, but minimal

### ✔️ You build **this structure**:

```
my_game/
│
├── build/
│
├── build.sh
├── run.sh
│
└── src/
    ├── main.c
    │
    ├── game/
    │   ├── game.h
    │   └── game.c
    │
    └── platform/
        ├── platform.h
        ├── platform_selector.h
        │
        ├── x11_backend.c
        ├── sdl_backend.c
        └── raylib_backend.c
```

### ✔️ Implement these low-level features:

#### **X11 Backend**

* Create window
* Handle:

  * Key input
  * Mouse input
  * Resize events
* Add `XShm` if brave (optional later)
* Software buffer + blitting to X11 window

#### **Raylib Backend**

* Create window
* Basic draw texture
* Basic input
* Match same API shape as X11 backend

### ❗ No audio, no threading, no OpenGL yet

Just the minimum platform “shell.”

---

# 📍 **PHASE 2 — Follow Handmade Hero (Platform Layer Weeks) (3–6 months)**

“You follow Casey’s platform layer episodes and port everything to X11.”

This is where real learning happens.

### 🎯 Goals

Implement:

* **Frame timing**
* **Audio output from your X11 layer**
* **File I/O**
* **Controller input (SDL2 optional)**
* **High-resolution timers**
* **Software rendering**
* **Back-buffer**

### ✔️ Recreate Casey’s Windows layer equivalent in Linux

These episodes include:

* Sound buffer generation → ALSA / Pulse
* X11 event loop refinements
* Multithreading
* Memory arenas
* Platform services

Raylib backend at this point:

* Just mirror the platform interface
* Raylib handles audio + rendering for you
* But game logic remains identical

---

# 📍 **PHASE 3 — Follow Casey’s Game Logic (6–12 months)**

“You’re now only writing gameplay code, not platform code.”

This is the LONGEST but most fun part.

Casey begins building:

* entity system
* hero movement
* collision system
* 2D world mapping
* renderer pipeline
* debug system
* memory arenas
* asset pipeline
* audio mixing
* DOS-style debug tools

### 🎯 Your job:

* Write EXACTLY the game logic he writes
* But create your OWN C code flow
* Keep both backends working

### ✔️ Learning outcomes:

* working **game loop**
* full **software renderer**
* entity/component management
* asset loading pipeline
* lighting
* audio mixing
* tools and debug overlays

This is the heart of the project.

---

# 📍 **PHASE 4 — Engine Engineer Mastery (Year 2–3)**

“You understand everything Casey did, and now can extend it.”

### 🎯 Optional but recommended:

* Implement OpenGL backend (instead of X11 software blit)
* Multi-threaded job system
* Vectorized SIMD routines
* ECS / hybrid ECS
* 2D animation system
* Custom tooling GUI (like imgui but handmade)

### ✨ This is when you REALLY become a low-level programmer.

---

# 📍 THE REALISTIC TIMELINE (W/ YOUR SCHEDULE)

| Phase                            | Duration    |
| -------------------------------- | ----------- |
| Phase 0 – Prep                   | 1–2 months  |
| Phase 1 – Base platform skeleton | 2–3 months  |
| Phase 2 – Platform episodes      | 3–6 months  |
| Phase 3 – Game logic             | 6–12 months |
| Phase 4 – Extensions             | 1–2 years   |

### ⭐ Total realistic time:

## **2.5–3 years**, not 7–8 years.

Why the big difference?
Because we **avoid rewriting everything twice** at the same time.

---

# 📁 Want me to generate this next?

### 💾 *A full Casey-style project template for Ubuntu:*

* `build/`
* `build.sh`
* `run.sh`
* `code/platform/x11`
* `code/platform/raylib`
* `game/`
* `handmadehero_main.c`
* minimal window + backbuffer + input

