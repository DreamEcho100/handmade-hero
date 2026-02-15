# 🎨 OpenGL Usage Guide

> A comprehensive, beginner-friendly guide to understanding and using OpenGL for 2D game rendering, specifically for CPU-rendered software buffers.

## How This Guide is Organized

This guide follows a **logical learning path** where each section builds on the previous one:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    LEARNING PROGRESSION                                 │
│                                                                         │
│   CONTEXT (Understanding Why)                                           │
│   ├── Section 1: What is OpenGL? ───────► What tool are we using?      │
│   └── Section 2: Why OpenGL? ───────────► Why not just use X11?        │
│                                                                         │
│   OVERVIEW (The Big Picture)                                            │
│   └── Section 3: Rendering Pipeline ────► How do all the pieces fit?   │
│                                                                         │
│   IMPLEMENTATION (Step-by-Step)                                         │
│   ├── Section 4: Initialization ────────► Setup (done once)            │
│   ├── Section 5: Texture Upload ────────► Send pixels to GPU           │
│   ├── Section 6: Drawing a Quad ────────► Display the texture          │
│   └── Section 7: Buffer Swapping ───────► Show the frame               │
│                                                                         │
│   REFERENCE (Looking Things Up)                                         │
│   ├── Section 8: Data Structures ───────► What data do we track?       │
│   └── Section 9: Key Functions ─────────► What functions do we call?   │
│                                                                         │
│   MASTERY (Avoiding Mistakes & Practice)                                │
│   ├── Section 10: Common Pitfalls ──────► What mistakes to avoid?      │
│   ├── Section 11: Exercises ────────────► Hands-on practice            │
│   └── Section 12: Resources ────────────► Further learning             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Prerequisites

Before reading this guide, you should understand:

- **C programming** - pointers, structs, function calls
- **Basic graphics concepts** - pixels, colors, buffers
- **FPS and frame timing** - see `fps-implementation.md` (companion guide)

### How OpenGL Relates to FPS Management

```
┌─────────────────────────────────────────────────────────────────────────┐
│       RELATIONSHIP TO fps-implementation.md                             │
│                                                                         │
│   fps-implementation.md covers:                                         │
│   ├── WHEN to display frames (timing)                                   │
│   ├── HOW LONG each frame should take (16.67ms)                         │
│   └── WHAT TO DO if frames take too long (adaptive FPS)                 │
│                                                                         │
│   THIS GUIDE covers:                                                    │
│   ├── HOW to display frames (OpenGL)                                    │
│   ├── WHERE pixels go (CPU buffer → GPU texture → screen)               │
│   └── WHAT FORMAT pixels should be in (RGBA)                            │
│                                                                         │
│   Together they form the complete rendering system:                     │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │  FPS Guide (timing) ──► "It's time to display a frame"          │   │
│   │          │                                                      │   │
│   │          ▼                                                      │   │
│   │  OpenGL Guide ──────► "Here's HOW to display that frame"        │   │
│   │          │                                                      │   │
│   │          ▼                                                      │   │
│   │  FPS Guide (measure) ► "That took X ms, adjust if needed"       │   │
│   └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Reading Tip**: Read sections 1-7 in order (they form a complete story). Sections 8-9 are references. Sections 10-12 help you master the material.

## Table of Contents

1. [What is OpenGL?](#1-what-is-opengl)
2. [Why OpenGL for a CPU Renderer?](#2-why-opengl-for-a-cpu-renderer)
3. [The Rendering Pipeline](#3-the-rendering-pipeline)
4. [Initialization](#4-initialization)
5. [Texture Upload](#5-texture-upload)
6. [Drawing a Fullscreen Quad](#6-drawing-a-fullscreen-quad)
7. [Buffer Swapping](#7-buffer-swapping)
8. [Key Data Structures](#8-key-data-structures)
9. [Key Functions](#9-key-functions)
10. [Common Pitfalls](#10-common-pitfalls)
11. [Practical Exercises](#11-practical-exercises)
12. [Resources](#12-resources)

---

# PART 1: CONTEXT

> _Before we dive into code, let's understand what OpenGL is and why we're using it._

---

## 1. What is OpenGL?

### The Core Question This Section Answers

**"What exactly is OpenGL, and where does it fit in the software stack?"**

### Definition

**OpenGL (Open Graphics Library)** is a cross-platform API for rendering 2D and 3D graphics. It provides a standardized interface to your GPU.

```
┌─────────────────────────────────────────────────────────────┐
│                  THE GRAPHICS STACK                         │
│                                                             │
│   ┌─────────────────┐                                       │
│   │   Your Game     │  Calls OpenGL functions               │
│   └────────┬────────┘                                       │
│            │                                                │
│            ▼                                                │
│   ┌─────────────────┐                                       │
│   │  OpenGL API     │  Standardized interface               │
│   └────────┬────────┘                                       │
│            │                                                │
│            ▼                                                │
│   ┌─────────────────┐                                       │
│   │  GPU Driver     │  NVIDIA/AMD/Intel implementation      │
│   └────────┬────────┘                                       │
│            │                                                │
│            ▼                                                │
│   ┌─────────────────┐                                       │
│   │      GPU        │  Hardware acceleration                │
│   └─────────────────┘                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### OpenGL vs Other APIs

| API         | Platform       | Use Case                   |
| ----------- | -------------- | -------------------------- |
| **OpenGL**  | Cross-platform | Classic, well-documented   |
| **Vulkan**  | Cross-platform | Modern, low-level, complex |
| **DirectX** | Windows        | Microsoft ecosystem        |
| **Metal**   | Apple          | macOS/iOS                  |

For a 2D software renderer like Handmade Hero, **OpenGL is perfect**: simple, fast enough, and works everywhere.

### Key Terminology

Before we continue, let's define some terms you'll see throughout this guide:

| Term        | Definition                                                 | Analogy                              |
| ----------- | ---------------------------------------------------------- | ------------------------------------ |
| **GPU**     | Graphics Processing Unit - specialized chip for graphics   | A factory specialized in painting    |
| **Texture** | An image stored in GPU memory                              | A photograph loaded into the factory |
| **Context** | OpenGL's state container (current texture, settings, etc.) | The factory's current configuration  |
| **Buffer**  | A region of memory holding data                            | A container/bucket for pixels        |
| **Swap**    | Exchange front and back buffers                            | Flip a double-sided canvas           |
| **Quad**    | A rectangle made of 4 vertices                             | A picture frame                      |

### 🔗 Connection to Next Section

Now you know **what** OpenGL is - a cross-platform graphics API that talks to your GPU. But why would we use it for a _software_ renderer that does everything on the CPU? Section 2 answers this question.

---

## 2. Why OpenGL for a CPU Renderer?

### The Core Question This Section Answers

**"If we render pixels on the CPU, why do we need OpenGL at all?"**

This is a crucial question! After all, Handmade Hero is a _software renderer_ - we calculate every pixel on the CPU. So why add the complexity of OpenGL?

### The Original Problem: XPutImage

Before OpenGL, we used X11's `XPutImage()` function to send pixel data to the screen. Here's what went wrong:

```
┌─────────────────────────────────────────────────────────────┐
│              XPUTIMAGE PROBLEMS                             │
│                                                             │
│  1. COLOR FORMAT MISMATCH                                   │
│     ─────────────────────                                   │
│     X11 wants: BGRA (Blue, Green, Red, Alpha)              │
│     Our game: RGBA (Red, Green, Blue, Alpha)               │
│     Result: Colors look wrong!                              │
│                                                             │
│     Expected:  🟥 Red                                        │
│     Got:       🟦 Blue (R and B swapped)                    │
│                                                             │
│  2. VARIABLE TIMING                                         │
│     ──────────────────                                      │
│     XPutImage timing depends on:                            │
│     - Window manager                                        │
│     - Compositor (Gnome, KDE)                               │
│     - GPU sync behavior                                     │
│     Result: Frame times vary wildly (1-50ms)                │
│                                                             │
│  3. NO VSYNC CONTROL                                        │
│     ─────────────────                                       │
│     X11 doesn't give us vsync info                          │
│     Result: Tearing or inconsistent frame pacing            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Why These Problems Matter

Let's connect each problem to concepts from the FPS guide:

```
┌─────────────────────────────────────────────────────────────┐
│         HOW X11 PROBLEMS AFFECT FRAME TIMING                │
│                                                             │
│  PROBLEM 1 (Color Format):                                  │
│  ─────────────────────────                                  │
│  Not a timing issue, but a correctness issue.               │
│  Your game looks wrong! Players see blue when you drew red. │
│                                                             │
│  PROBLEM 2 (Variable Timing):                               │
│  ───────────────────────────                                │
│  From fps-implementation.md, you know:                      │
│  • We need consistent 16.67ms frames                        │
│  • We use two-phase sleep to hit this target                │
│                                                             │
│  But if XPutImage itself takes 1-50ms randomly:             │
│  • Frame N: work=5ms, XPutImage=2ms, total work=7ms ✓       │
│  • Frame N+1: work=5ms, XPutImage=45ms, total work=50ms ❌   │
│                                                             │
│  Our careful timing is ruined by unpredictable display!     │
│                                                             │
│  PROBLEM 3 (No VSync):                                      │
│  ─────────────────────                                      │
│  Without vsync, we can't synchronize with monitor refresh.  │
│  Result: Tearing (top half shows frame N, bottom shows N+1) │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### The OpenGL Solution

```
┌─────────────────────────────────────────────────────────────┐
│              OPENGL ADVANTAGES                              │
│                                                             │
│  1. CONSISTENT RGBA FORMAT                                  │
│     ──────────────────────                                  │
│     OpenGL accepts RGBA directly                            │
│     No color swapping needed!                               │
│                                                             │
│  2. GPU-ACCELERATED UPLOAD                                  │
│     ─────────────────────────                               │
│     glTexImage2D → DMA transfer to GPU                      │
│     Much faster than X11's software path                    │
│                                                             │
│  3. VSYNC VIA GLX                                           │
│     ───────────────                                         │
│     glXSwapBuffers waits for vertical blank                 │
│     Consistent timing, no tearing                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Our Hybrid Approach

```
┌─────────────────────────────────────────────────────────────┐
│            CPU-RENDERED + GPU-DISPLAYED                     │
│                                                             │
│   CPU Side                          GPU Side                │
│   ────────                          ────────                │
│                                                             │
│   ┌─────────────────┐                                       │
│   │  Game Logic     │                                       │
│   │  (Handmade Hero)│                                       │
│   └────────┬────────┘                                       │
│            │                                                │
│            ▼                                                │
│   ┌─────────────────┐                                       │
│   │  CPU Rendering  │  Writes RGBA pixels                   │
│   │  (software)     │  to memory buffer                     │
│   └────────┬────────┘                                       │
│            │                                                │
│            │  glTexImage2D()                                │
│            │  (upload pixels)                               │
│            │                                                │
│            ▼                         ┌─────────────────┐    │
│   ┌─────────────────┐                │  GPU Texture    │    │
│   │  OpenGL Texture │ ────────────── │  Memory         │    │
│   └────────┬────────┘                └─────────────────┘    │
│            │                                                │
│            │  Draw fullscreen quad                          │
│            │                                                │
│            ▼                         ┌─────────────────┐    │
│   ┌─────────────────┐                │  Display        │    │
│   │  glXSwapBuffers │ ────────────── │  (Monitor)      │    │
│   └─────────────────┘                └─────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### The Key Insight

**We're NOT using the GPU to render our game.** We're using it as a fast, consistent way to display what we already rendered on the CPU.

Think of it like this:

- **CPU**: The artist who paints the picture (renders pixels)
- **GPU**: The gallery that displays the painting (shows pixels on screen)
- **OpenGL**: The delivery service between artist and gallery

### 🔗 Connection to Next Section

Now you understand:

- ✅ What OpenGL is (graphics API)
- ✅ Why we need it (X11 problems)
- ✅ Our hybrid approach (CPU renders, GPU displays)

But before we write code, we need to see the **big picture** - how all the OpenGL pieces fit together. That's Section 3.

---

# PART 2: OVERVIEW

> _Understanding the complete flow before diving into implementation details._

---

## 3. The Rendering Pipeline

### The Core Question This Section Answers

**"What are all the steps, and in what order do they happen?"**

This section gives you the mental model you need before implementing. Don't worry about the code yet - just understand the flow.

### Simplified OpenGL Pipeline for 2D

```
┌─────────────────────────────────────────────────────────────┐
│            OPENGL 2D TEXTURE DISPLAY PIPELINE               │
│                                                             │
│  STEP 1: INITIALIZATION (once at startup)                   │
│  ─────────────────────────────────────────                  │
│  These steps happen ONCE when your program starts.          │
│  They set up OpenGL so it's ready to display frames.        │
│                                                             │
│    ┌─────────────┐                                          │
│    │ Create      │  glXCreateContext()                      │
│    │ GL Context  │  "Initialize OpenGL"                     │
│    └──────┬──────┘                                          │
│           │                                                 │
│           ▼                                                 │
│    ┌─────────────┐                                          │
│    │ Create      │  glGenTextures()                         │
│    │ Texture     │  "Reserve GPU memory for our image"      │
│    └──────┬──────┘                                          │
│           │                                                 │
│           ▼                                                 │
│    ┌─────────────┐                                          │
│    │ Setup       │  glOrtho() for 2D                        │
│    │ Projection  │  "Tell GPU this is 2D, not 3D"           │
│    └─────────────┘                                          │
│                                                             │
│  ──────────────────────────────────────────────────────     │
│                                                             │
│  STEP 2: EACH FRAME (in game loop)                          │
│  ─────────────────────────────────                          │
│  These steps happen EVERY FRAME (60 times per second).      │
│  This is where the FPS guide's timing measurements apply.   │
│                                                             │
│    ┌─────────────┐                                          │
│    │ Upload      │  glTexImage2D()                          │
│    │ Pixels      │  "Copy CPU pixels → GPU texture"         │
│    └──────┬──────┘                                          │
│           │                                                 │
│           ▼                                                 │
│    ┌─────────────┐                                          │
│    │ Draw        │  glBegin(GL_QUADS)                       │
│    │ Quad        │  "Draw a rectangle with our texture"     │
│    └──────┬──────┘                                          │
│           │                                                 │
│           ▼                                                 │
│    ┌─────────────┐                                          │
│    │ Swap        │  glXSwapBuffers()                        │
│    │ Buffers     │  "Show this frame on the monitor"        │
│    └─────────────┘                                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### How This Maps to the Game Loop

Remember from the FPS guide, our game loop has 11 steps. Here's where OpenGL fits:

```
┌─────────────────────────────────────────────────────────────┐
│         GAME LOOP STEPS (from fps-implementation.md)        │
│         with OpenGL highlighted                             │
│                                                             │
│   Step 1:  Timestamp frame start                            │
│   Step 2:  Prepare input                                    │
│   Step 3:  Process X11 events                               │
│   Step 4:  Poll joystick                                    │
│   Step 5:  Update game + render (CPU fills pixel buffer)    │
│   ┌─────────────────────────────────────────────────────┐   │
│   │ Step 5b: glTexImage2D()     ← UPLOAD PIXELS         │   │
│   │ Step 5c: glBegin/glEnd      ← DRAW QUAD             │   │
│   │ Step 5d: glXSwapBuffers()   ← SWAP BUFFERS          │   │
│   │ Step 5e: XSync()            ← WAIT FOR GPU          │   │
│   └─────────────────────────────────────────────────────┘   │
│   Step 6:  Measure work time                                │
│   Step 7:  Sleep (two-phase)                                │
│   Step 8:  Fill audio buffer                                │
│   Step 9:  Measure total frame time                         │
│   Step 10: Report missed frames                             │
│   Step 11: Adaptive FPS evaluation                          │
│                                                             │
│   OpenGL work is measured as part of "work time" in Step 6  │
│   This affects whether we hit our 16.67ms target!           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

You now have the complete mental model:

- **Initialization** (once): Context → Texture → Projection
- **Per frame** (60x/second): Upload → Draw → Swap

Now let's implement each step. Section 4 covers **Initialization** - the one-time setup.

---

# PART 3: IMPLEMENTATION

> _Step-by-step implementation with detailed explanations._

---

## 4. Initialization

### The Core Question This Section Answers

**"What do I need to set up before I can display frames?"**

Initialization happens **once** at program startup. After this, you're ready to display frames in your game loop.

### Step 1: Choose a Visual

A "visual" in X11 terms is a description of the pixel format (color depth, double buffering, etc.)

**Why we need this**: X11 and OpenGL need to agree on how pixels are formatted. The visual ensures compatibility.

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 424-429

// Request OpenGL-capable visual with:
// - RGBA color (not color-indexed)
// - 24-bit depth buffer
// - Double buffering (for smooth animation)
int visual_attribs[] = {
    GLX_RGBA,           // Want true-color, not palette
    GLX_DEPTH_SIZE, 24, // 24-bit depth buffer
    GLX_DOUBLEBUFFER,   // Enable double buffering
    None                // Terminator (required!)
};

XVisualInfo *visual = glXChooseVisual(display, screen, visual_attribs);
```

**What each attribute means:**

| Attribute            | Purpose                              | Why We Need It                         |
| -------------------- | ------------------------------------ | -------------------------------------- |
| `GLX_RGBA`           | Use true color (not indexed palette) | We want to specify exact RGB values    |
| `GLX_DEPTH_SIZE, 24` | 24-bit depth buffer                  | Standard depth, future-proofing for 3D |
| `GLX_DOUBLEBUFFER`   | Enable double buffering              | Prevents flickering (see Section 7)    |
| `None`               | End of list                          | Required terminator                    |

### Step 2: Create OpenGL Context

The "context" is OpenGL's state container (textures, settings, etc.)

**Why we need this**: OpenGL is a state machine. The context holds all state: which texture is active, what color to use, etc. Without a context, OpenGL calls do nothing.

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 108-115

GLXContext gl_context = glXCreateContext(
    display,    // X11 display connection
    visual,     // Visual we chose above (Step 1 feeds into Step 2!)
    NULL,       // No sharing with other contexts
    True        // Direct rendering (use GPU directly)
);

// Make this context current (active for this thread)
glXMakeCurrent(display, window, gl_context);
```

**Understanding "Make Current":**

```
┌─────────────────────────────────────────────────────────────┐
│         WHY glXMakeCurrent() IS NECESSARY                   │
│                                                             │
│   You could have MULTIPLE OpenGL contexts:                  │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│   │ Context A   │  │ Context B   │  │ Context C   │         │
│   │ (texture 1) │  │ (texture 2) │  │ (texture 3) │         │
│   └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                             │
│   OpenGL needs to know: "Which context should this          │
│   glTexImage2D() call apply to?"                            │
│                                                             │
│   glXMakeCurrent() says: "All OpenGL calls on THIS thread   │
│   now apply to THIS context."                               │
│                                                             │
│   For our simple case: one context, make it current once.   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Step 3: Create Texture

Textures are images stored on the GPU. We use one texture to hold our CPU-rendered frame.

**Why we need this**: Our pixels live in CPU memory (RAM). The GPU can't display RAM directly - we need to copy pixels to GPU memory. A "texture" is GPU memory reserved for image data.

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 120-142

// Generate a texture ID (like reserving a slot)
GLuint texture_id;
glGenTextures(1, &texture_id);

// Bind (activate) this texture ("I want to work with THIS texture")
glBindTexture(GL_TEXTURE_2D, texture_id);

// Set texture filtering (what happens when scaling)
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
// GL_NEAREST = no interpolation (pixel-perfect, crisp)
// GL_LINEAR = smooth interpolation (blurry, good for photos)

// Set edge behavior (what happens at texture boundaries)
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

**Understanding Texture Filtering:**

```
┌─────────────────────────────────────────────────────────────┐
│         GL_NEAREST vs GL_LINEAR                             │
│                                                             │
│   Original 4x4 texture:       Displayed at 8x8:             │
│                                                             │
│   ┌───┬───┬───┬───┐                                         │
│   │ R │ G │ B │ W │           GL_NEAREST (pixel-perfect):   │
│   ├───┼───┼───┼───┤           ┌───┬───┬───┬───┬───┬───┬───┬───┐
│   │ G │ B │ W │ R │           │RR │RR │GG │GG │BB │BB │WW │WW │
│   ├───┼───┼───┼───┤           │RR │RR │GG │GG │BB │BB │WW │WW │
│   │ B │ W │ R │ G │           └───┴───┴───┴───┴───┴───┴───┴───┘
│   ├───┼───┼───┼───┤           Blocky but sharp               │
│   │ W │ R │ G │ B │                                          │
│   └───┴───┴───┴───┘           GL_LINEAR (smooth):            │
│                               Colors blend between pixels    │
│                               Smooth but blurry              │
│                                                             │
│   For retro pixel art: Use GL_NEAREST                       │
│   For photographs: Use GL_LINEAR                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Step 4: Setup Orthographic Projection

For 2D rendering, we use an **orthographic** projection (no perspective).

**Why we need this**: OpenGL defaults to 3D perspective rendering. In perspective, faraway things look smaller. For 2D games, we want pixel-perfect rendering where 1 unit = 1 pixel, regardless of "depth."

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 145-156

// Switch to projection matrix mode
glMatrixMode(GL_PROJECTION);
glLoadIdentity();  // Reset to identity matrix

// Setup 2D coordinate system:
// Origin (0,0) at top-left, (width,height) at bottom-right
glOrtho(
    0, width,     // Left, Right
    height, 0,    // Bottom, Top (flipped for top-left origin!)
    -1, 1         // Near, Far (not important for 2D)
);

// Switch back to modelview matrix for drawing
glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
```

**Understanding the Coordinate Flip:**

```
┌─────────────────────────────────────────────────────────────┐
│        WHY WE FLIP THE Y AXIS                               │
│                                                             │
│   OPENGL DEFAULT:              SCREEN COORDINATES:          │
│   (0,height)───(width,height)  (0,0)──────────(width,0)     │
│       │                  │         │                  │     │
│       │    Y increases   │         │    Y increases   │     │
│       │    UPWARD ↑      │         │    DOWNWARD ↓    │     │
│       │                  │         │                  │     │
│   (0,0)────────(width,0)   (0,height)──(width,height)       │
│                                                             │
│   OpenGL: Origin at BOTTOM-left, Y goes UP (math style)     │
│   Screen: Origin at TOP-left, Y goes DOWN (image style)     │
│                                                             │
│   By calling glOrtho(0, width, height, 0, ...)              │
│                              ^^^^^^  ^                      │
│                              bottom  top (swapped!)         │
│                                                             │
│   We tell OpenGL: "Flip your Y axis to match screen coords" │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Coordinate System Visualization:**

```
┌─────────────────────────────────────────────────────────────┐
│        OPENGL 2D COORDINATE SYSTEM (after glOrtho)          │
│                                                             │
│   (0,0) ────────────────────────────────────── (width,0)    │
│     │                                               │       │
│     │                                               │       │
│     │               YOUR GAME                       │       │
│     │               RENDERS                         │       │
│     │               HERE                            │       │
│     │                                               │       │
│     │                                               │       │
│   (0,height) ───────────────────────────── (width,height)   │
│                                                             │
│   Note: Y increases DOWNWARD (like screen coordinates)      │
│   This is set by glOrtho(0, width, height, 0, -1, 1)       │
│   The "height, 0" flips Y axis from OpenGL default          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Initialization Summary

```
┌─────────────────────────────────────────────────────────────┐
│         INITIALIZATION COMPLETE CHECKLIST                   │
│                                                             │
│  ✓ Step 1: Choose Visual (glXChooseVisual)                  │
│    └─► "What pixel format should we use?"                   │
│                                                             │
│  ✓ Step 2: Create Context (glXCreateContext + MakeCurrent)  │
│    └─► "Initialize OpenGL state machine"                    │
│                                                             │
│  ✓ Step 3: Create Texture (glGenTextures + parameters)      │
│    └─► "Reserve GPU memory for our image"                   │
│                                                             │
│  ✓ Step 4: Setup Projection (glOrtho)                       │
│    └─► "Tell OpenGL this is 2D with screen coordinates"     │
│                                                             │
│  NOW READY TO: Display frames in the game loop!             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

Initialization is complete. Now, every frame (60 times per second), we need to:

1. **Upload** new pixels to the texture (Section 5)
2. **Draw** the texture to the screen (Section 6)
3. **Swap** buffers to display it (Section 7)

Section 5 covers the first step: uploading pixels.

---

## 5. Texture Upload

### The Core Question This Section Answers

**"How do I get my CPU-rendered pixels onto the GPU?"**

This happens **every frame**. Your game renders pixels into a CPU buffer, then we upload them to the GPU texture.

Every frame, we upload our CPU-rendered pixels to the GPU texture.

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 162-182

static void update_window_opengl(GameBackBuffer *buffer) {
    // Bind our texture
    glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);

    // Upload pixel data from CPU buffer to GPU texture
    glTexImage2D(
        GL_TEXTURE_2D,      // Target (2D texture)
        0,                  // Mipmap level (0 = base level)
        GL_RGBA,            // Internal format (how GPU stores it)
        buffer->width,      // Width in pixels
        buffer->height,     // Height in pixels
        0,                  // Border (must be 0)
        GL_RGBA,            // Source format (our CPU buffer format)
        GL_UNSIGNED_BYTE,   // Source data type (8 bits per channel)
        buffer->memory.base // Pointer to pixel data
    );
}
```

### Understanding glTexImage2D Parameters

```
┌─────────────────────────────────────────────────────────────┐
│              glTexImage2D() BREAKDOWN                       │
│                                                             │
│  glTexImage2D(                                              │
│      GL_TEXTURE_2D,       // Target type                    │
│      0,                   // Mipmap level                   │
│      GL_RGBA,             // Internal format ─────┐         │
│      width,               // Texture width        │         │
│      height,              // Texture height       │         │
│      0,                   // Border (legacy)      │         │
│      GL_RGBA,             // Source format ───────┤         │
│      GL_UNSIGNED_BYTE,    // Data type ───────────┤         │
│      pixels               // Pixel data ──────────┘         │
│  );                                                         │
│                                                             │
│  PIXEL DATA LAYOUT (GL_RGBA, GL_UNSIGNED_BYTE):             │
│  ───────────────────────────────────────────────            │
│                                                             │
│  Each pixel = 4 bytes: [R][G][B][A]                         │
│                                                             │
│  Row 0: [RGBA][RGBA][RGBA]...[RGBA]  ← width pixels         │
│  Row 1: [RGBA][RGBA][RGBA]...[RGBA]                         │
│  ...                                                        │
│  Row N: [RGBA][RGBA][RGBA]...[RGBA]  ← height rows          │
│                                                             │
│  Total size: width × height × 4 bytes                       │
│  Example: 1280×720 = 3,686,400 bytes ≈ 3.5 MB               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Performance Consideration

**This is the slowest part of our OpenGL usage!**

```
┌─────────────────────────────────────────────────────────────┐
│         TEXTURE UPLOAD PERFORMANCE                          │
│                                                             │
│   glTexImage2D() must copy 3.5 MB of data from:             │
│   CPU RAM ──────────────────────────────► GPU VRAM          │
│                                                             │
│   This takes time! Typically 1-3ms per frame.               │
│                                                             │
│   From the FPS guide, our budget is 16.67ms:                │
│   • Game logic:      ~2ms                                   │
│   • CPU rendering:   ~4ms                                   │
│   • Texture upload:  ~2ms  ← We are here                    │
│   • Draw + Swap:     ~1ms                                   │
│   • Sleep:           ~7ms (remaining)                       │
│   ─────────────────────────                                 │
│   Total:             ~16.67ms ✓                             │
│                                                             │
│   If texture upload takes too long, we miss frames!         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

After uploading pixels to the texture, the data is now in GPU memory. But the GPU hasn't done anything with it yet - we need to **draw** the texture onto the screen. That's Section 6.

---

## 6. Drawing a Fullscreen Quad

### The Core Question This Section Answers

**"How do I display the texture on screen?"**

After uploading pixels to the texture, we draw a rectangle that fills the screen.

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 184-210

// Enable 2D texturing (tell OpenGL we're using textures)
glEnable(GL_TEXTURE_2D);

// Clear the screen (optional, we're filling it anyway)
glClear(GL_COLOR_BUFFER_BIT);

// Draw a textured quad (rectangle)
glBegin(GL_QUADS);
    // Each vertex needs TWO pieces of information:
    // 1. Where to sample from the texture (glTexCoord2f)
    // 2. Where to draw on screen (glVertex2f)

    // Bottom-left vertex
    glTexCoord2f(0.0f, 0.0f);           // Texture: top-left corner
    glVertex2f(0.0f, 0.0f);             // Screen: top-left corner

    // Bottom-right vertex
    glTexCoord2f(1.0f, 0.0f);           // Texture: top-right corner
    glVertex2f((float)width, 0.0f);     // Screen: top-right corner

    // Top-right vertex
    glTexCoord2f(1.0f, 1.0f);           // Texture: bottom-right corner
    glVertex2f((float)width, (float)height);  // Screen: bottom-right corner

    // Top-left vertex
    glTexCoord2f(0.0f, 1.0f);           // Texture: bottom-left corner
    glVertex2f(0.0f, (float)height);    // Screen: bottom-left corner
glEnd();
```

### Understanding the Vertex Specification

**Why do we need both `glTexCoord2f` and `glVertex2f`?**

### Texture Coordinate Mapping

```
┌─────────────────────────────────────────────────────────────┐
│         TEXTURE COORDINATES vs SCREEN POSITIONS             │
│                                                             │
│   TEXTURE SPACE (UV coordinates, 0-1 range)                 │
│   ──────────────────────────────────────────                │
│                                                             │
│   (0,0) ─────────────────────────────── (1,0)               │
│     │            TEXTURE                   │                │
│     │            IMAGE                     │                │
│     │                                      │                │
│   (0,1) ─────────────────────────────── (1,1)               │
│                                                             │
│                      ↕ MAPPING ↕                            │
│                                                             │
│   SCREEN SPACE (pixels)                                     │
│   ─────────────────────                                     │
│                                                             │
│   (0,0) ─────────────────────────── (width,0)               │
│     │            WINDOW                    │                │
│     │                                      │                │
│     │                                      │                │
│   (0,height) ─────────────────── (width,height)             │
│                                                             │
│   VERTEX SPECIFICATION:                                     │
│   ─────────────────────                                     │
│                                                             │
│   glTexCoord2f(u, v);  // Where to sample from texture      │
│   glVertex2f(x, y);    // Where to draw on screen           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Why 0-1 for Texture, 0-width for Screen?

```
┌─────────────────────────────────────────────────────────────┐
│         NORMALIZED vs PIXEL COORDINATES                     │
│                                                             │
│   TEXTURE COORDINATES (0 to 1):                             │
│   • Always 0-1 regardless of texture size                   │
│   • (0,0) = top-left, (1,1) = bottom-right                  │
│   • Makes code work for ANY texture resolution              │
│                                                             │
│   Example: 1280x720 texture                                 │
│   • (0.5, 0.5) = middle = pixel (640, 360)                  │
│   • (0.25, 0.75) = pixel (320, 540)                         │
│                                                             │
│   SCREEN COORDINATES (0 to width/height):                   │
│   • Pixel coordinates (after glOrtho setup)                 │
│   • (0,0) = top-left pixel                                  │
│   • (1279, 719) = bottom-right pixel                        │
│                                                             │
│   WHY DIFFERENT?                                            │
│   • Texture coords are resolution-independent               │
│   • Screen coords match actual pixel positions              │
│   • Allows scaling: draw 640x480 texture on 1280x720 screen │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

We've drawn the quad to the **back buffer**. But the user can't see it yet - it's hidden! We need to **swap** the back buffer with the front buffer to display it. That's Section 7.

---

## 7. Buffer Swapping

### The Core Question This Section Answers

**"Why don't I see what I drew, and how do I make it visible?"**

Everything we've drawn so far is in the **back buffer** - invisible to the user. We need to swap buffers to display our frame.

### Double Buffering Explained

```
┌─────────────────────────────────────────────────────────────┐
│               DOUBLE BUFFERING                              │
│                                                             │
│   WITHOUT DOUBLE BUFFERING:                                 │
│   ─────────────────────────                                 │
│                                                             │
│   Frame N rendering ──────┐                                 │
│   to display buffer       │ USER SEES                       │
│   ────────────────────────┤ PARTIAL                         │
│   Frame N+1 starts        │ FRAMES!                         │
│   overwriting...          │ (flickering)                    │
│                                                             │
│   WITH DOUBLE BUFFERING:                                    │
│   ──────────────────────                                    │
│                                                             │
│   ┌──────────────┐  ┌──────────────┐                        │
│   │ FRONT BUFFER │  │ BACK BUFFER  │                        │
│   │ (displayed)  │  │ (hidden)     │                        │
│   └──────────────┘  └──────────────┘                        │
│          │                  │                               │
│          │    Render to     │                               │
│          │    back buffer   │                               │
│          │         ↓        │                               │
│          │    ┌─────────┐   │                               │
│          │    │ SWAP!   │   │                               │
│          │    └─────────┘   │                               │
│          │         ↓        │                               │
│          └─────────┬────────┘                               │
│                    │                                        │
│   Front becomes back, back becomes front                    │
│   User sees complete frames only!                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### glXSwapBuffers

```c
// 📍 Location: project/src/platform/x11/backend.c, line 212

// Swap front and back buffers
glXSwapBuffers(display, window);

// This function:
// 1. Waits for vertical blanking interval (if vsync enabled)
// 2. Swaps buffer pointers
// 3. Returns when swap is complete
```

### GPU Synchronization

After swapping, we sync with the GPU to get accurate timing:

```c
// 📍 Location: project/src/platform/x11/backend.c, line 527

// Wait for all GPU commands to complete
XSync(display, False);

// Without this:
// - CPU continues immediately
// - Frame time measurement is wrong
// - GPU might still be processing

// With this:
// - CPU waits for GPU to finish
// - Accurate frame time measurement
// - Consistent timing
```

### Why XSync Matters for Frame Timing

```
┌─────────────────────────────────────────────────────────────┐
│         XSYNC AND THE FPS GUIDE CONNECTION                  │
│                                                             │
│   Remember from fps-implementation.md: accurate timing      │
│   requires knowing WHEN the frame is actually displayed.    │
│                                                             │
│   WITHOUT XSync:                                            │
│   ──────────────                                            │
│   CPU: "Done! Frame took 2ms"                               │
│   GPU: "Still processing... 5ms more..."                    │
│   Reality: Frame took 7ms, but CPU thinks 2ms!              │
│                                                             │
│   WITH XSync:                                               │
│   ────────────                                              │
│   CPU: "glXSwapBuffers()... waiting..."                     │
│   GPU: "Processing... done!"                                │
│   CPU: "Now I can measure: frame took 7ms"                  │
│                                                             │
│   This accuracy feeds into:                                 │
│   • Two-phase sleep calculation                             │
│   • Adaptive FPS decisions                                  │
│   • Frame miss detection                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

We've covered all the OpenGL operations needed to display frames. Section 8 summarizes the **data structures** that hold all this state together.

---

# PART 4: REFERENCE

> _Data structures, function reference, and debugging information._

---

## 8. Key Data Structures

### 🔗 Why This Section Matters

Understanding these structures shows how **initialization state persists** across frames. The `OpenGLState` struct connects to every operation we've discussed.

### OpenGLState

Holds all OpenGL-related state for our application.

```c
// 📍 Location: project/src/platform/x11/backend.c, lines 26-33

typedef struct OpenGLState {
    Display *display;       // X11 display connection
    Window window;          // X11 window handle
    GLXContext gl_context;  // OpenGL context
    GLuint texture_id;      // Our GPU texture
    int width;              // Window width
    int height;             // Window height
} OpenGLState;

// Global instance
static OpenGLState g_gl = {0};
```

**How This Connects to Previous Sections:**

```
┌─────────────────────────────────────────────────────────────┐
│         OPENGLSTATE FIELD ORIGINS                           │
│                                                             │
│   typedef struct OpenGLState {                              │
│       Display *display;      ← X11 setup (before Section 4) │
│       Window window;         ← X11 setup (before Section 4) │
│       GLXContext gl_context; ← Section 4, Step 2            │
│       GLuint texture_id;     ← Section 4, Step 3            │
│       int width, height;     ← Window dimensions            │
│   } OpenGLState;                                            │
│                                                             │
│   Each field represents an initialization step!             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### GameBackBuffer

The CPU-side pixel buffer that we render to.

```c
// Our game renders pixels here (in RAM)
typedef struct GameBackBuffer {
    PlatformMemoryBlock memory;  // Pixel data (RGBA)
    int width;                   // Buffer width
    int height;                  // Buffer height
    int pitch;                   // Bytes per row (width × 4)
    int bytes_per_pixel;         // Always 4 (RGBA)
} GameBackBuffer;
```

**How This Connects to OpenGL:**

```
┌─────────────────────────────────────────────────────────────┐
│         CPU BUFFER → GPU TEXTURE FLOW                       │
│                                                             │
│   GameBackBuffer                  OpenGLState          │
│   ┌─────────────────────┐              ┌─────────────────┐  │
│   │ memory.base ────────│──────────────│→ texture_id     │  │
│   │ width, height       │   glTexImage2D()               │  │
│   │ pitch               │              │                 │  │
│   │ bytes_per_pixel = 4 │              │ GL_RGBA format  │  │
│   └─────────────────────┘              └─────────────────┘  │
│                                                             │
│   The game renders to GameBackBuffer.memory.base,      │
│   then glTexImage2D() copies it to the GPU texture.         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

Now that you understand the data structures, Section 9 provides a **function reference** - the complete list of OpenGL and GLX functions we use.

---

## 9. Key Functions

### 🔗 Why This Section Matters

This is your **quick lookup** for every OpenGL/GLX function used in this guide. Each function maps to a specific step in our rendering pipeline.

### GLX Functions (X11 + OpenGL)

| Function              | Purpose                                 |
| --------------------- | --------------------------------------- |
| `glXChooseVisual()`   | Find a visual matching our requirements |
| `glXCreateContext()`  | Create an OpenGL context                |
| `glXMakeCurrent()`    | Activate a context for this thread      |
| `glXSwapBuffers()`    | Swap front/back buffers (display frame) |
| `glXDestroyContext()` | Clean up context                        |

### Core OpenGL Functions

| Function                  | Purpose                      |
| ------------------------- | ---------------------------- |
| `glGenTextures()`         | Generate texture IDs         |
| `glBindTexture()`         | Make a texture active        |
| `glTexImage2D()`          | Upload pixel data to texture |
| `glTexParameteri()`       | Set texture properties       |
| `glEnable(GL_TEXTURE_2D)` | Enable 2D texturing          |
| `glBegin(GL_QUADS)`       | Start drawing quads          |
| `glTexCoord2f()`          | Specify texture coordinate   |
| `glVertex2f()`            | Specify vertex position      |
| `glEnd()`                 | Finish drawing               |
| `glOrtho()`               | Set orthographic projection  |
| `glMatrixMode()`          | Switch matrix modes          |
| `glClear()`               | Clear the screen             |

### Complete Function Reference

```c
// ═══════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════

// Choose visual with specific attributes
XVisualInfo* glXChooseVisual(
    Display *display,      // X11 display
    int screen,            // Screen number
    int *attrib_list       // Attribute list (NULL-terminated)
);

// Create OpenGL rendering context
GLXContext glXCreateContext(
    Display *display,      // X11 display
    XVisualInfo *visual,   // Visual from glXChooseVisual
    GLXContext share_list, // Context to share with (or NULL)
    Bool direct            // True = direct rendering (faster)
);

// Make context current for this thread
Bool glXMakeCurrent(
    Display *display,      // X11 display
    GLXDrawable drawable,  // Window or pixmap
    GLXContext context     // Context to make current
);

// ═══════════════════════════════════════════════════════════
// TEXTURE MANAGEMENT
// ═══════════════════════════════════════════════════════════

// Generate texture object names
void glGenTextures(
    GLsizei n,             // Number of textures to generate
    GLuint *textures       // Array to store texture IDs
);

// Bind (activate) a texture
void glBindTexture(
    GLenum target,         // GL_TEXTURE_2D
    GLuint texture         // Texture ID to bind
);

// Upload pixel data to texture
void glTexImage2D(
    GLenum target,         // GL_TEXTURE_2D
    GLint level,           // Mipmap level (0 = base)
    GLint internal_format, // How GPU stores (GL_RGBA)
    GLsizei width,         // Width in pixels
    GLsizei height,        // Height in pixels
    GLint border,          // Must be 0
    GLenum format,         // Source format (GL_RGBA)
    GLenum type,           // Data type (GL_UNSIGNED_BYTE)
    const void *data       // Pixel data pointer
);

// Set texture parameters
void glTexParameteri(
    GLenum target,         // GL_TEXTURE_2D
    GLenum pname,          // Parameter name
    GLint param            // Parameter value
);

// ═══════════════════════════════════════════════════════════
// DRAWING
// ═══════════════════════════════════════════════════════════

// Begin primitive drawing
void glBegin(GLenum mode); // GL_QUADS, GL_TRIANGLES, etc.

// Specify texture coordinate for next vertex
void glTexCoord2f(GLfloat s, GLfloat t);

// Specify vertex position
void glVertex2f(GLfloat x, GLfloat y);

// End primitive drawing
void glEnd(void);

// ═══════════════════════════════════════════════════════════
// BUFFER OPERATIONS
// ═══════════════════════════════════════════════════════════

// Swap front and back buffers
void glXSwapBuffers(Display *display, GLXDrawable drawable);

// Clear buffers
void glClear(GLbitfield mask);  // GL_COLOR_BUFFER_BIT, etc.
```

### 🔗 Connection to Next Section

Knowing the functions is only half the battle. Section 10 covers **common pitfalls** - the mistakes you'll likely make and how to fix them.

---

# PART 5: DEBUGGING

> _Common mistakes and how to avoid them._

---

## 10. Common Pitfalls

### 🔗 Why This Section Matters

These are the bugs you **will** encounter. Every mistake listed here corresponds to a concept from earlier sections.

### ❌ Pitfall 1: Wrong Pixel Format

```c
// BAD: Format mismatch between CPU buffer and glTexImage2D
// CPU buffer has BGRA, but we tell OpenGL it's RGBA
glTexImage2D(..., GL_RGBA, ..., bgra_buffer);
// Result: Red and Blue are swapped!

// GOOD: Match format to actual data
glTexImage2D(..., GL_BGRA, ..., bgra_buffer);  // X11 style
// OR render in RGBA from the start
```

### ❌ Pitfall 2: Forgetting to Bind Texture

```c
// BAD: Upload without binding
glTexImage2D(..., data);  // Which texture?? Undefined!

// GOOD: Always bind first
glBindTexture(GL_TEXTURE_2D, my_texture);
glTexImage2D(..., data);  // Now it's clear
```

### ❌ Pitfall 3: Wrong Coordinate System

```c
// BAD: OpenGL default Y-axis (0 at bottom)
glOrtho(0, width, 0, height, -1, 1);
// Texture appears upside down!

// GOOD: Flip Y for screen coordinates (0 at top)
glOrtho(0, width, height, 0, -1, 1);
//                ^^^^^^^ ^^  flipped!
```

### ❌ Pitfall 4: Not Enabling Texturing

```c
// BAD: Draw textured quad without enabling textures
glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    // ... vertices
glEnd();
// Result: Solid color, no texture!

// GOOD: Enable texture mapping
glEnable(GL_TEXTURE_2D);  // <-- Don't forget this!
glBegin(GL_QUADS);
    // ...
glEnd();
```

### ❌ Pitfall 5: Leaking GPU Resources

```c
// BAD: Create textures every frame
while (running) {
    GLuint tex;
    glGenTextures(1, &tex);  // Leak!
    // ...
    // Never deleted!
}

// GOOD: Create once, reuse
GLuint tex;
glGenTextures(1, &tex);  // Once at startup

while (running) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(...);  // Update existing texture
}

// Cleanup at shutdown
glDeleteTextures(1, &tex);
```

### Pitfall-to-Section Reference

```
┌─────────────────────────────────────────────────────────────┐
│         WHICH SECTION EXPLAINS EACH PITFALL?                │
│                                                             │
│   Pitfall 1 (Wrong Pixel Format)   → Section 5 (Upload)     │
│   Pitfall 2 (Forgot Bind)          → Section 4 (Init Step 3)│
│   Pitfall 3 (Wrong Coordinates)    → Section 4 (Init Step 4)│
│   Pitfall 4 (No Texturing)         → Section 6 (Drawing)    │
│   Pitfall 5 (Resource Leak)        → Section 4 (Init Step 3)│
│                                                             │
│   If you hit a bug, the section reference tells you where   │
│   to review the correct implementation!                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 🔗 Connection to Next Section

Now you know the pitfalls. Section 11 provides **hands-on exercises** to practice and internalize everything you've learned.

---

# PART 6: PRACTICE

> _Hands-on exercises to solidify your understanding._

---

## 11. Practical Exercises

### 🔗 Why This Section Matters

Reading isn't enough - you need to **build** to truly understand. These exercises progress from simple to complex, reinforcing each section.

### Exercise Progression Map

```
┌─────────────────────────────────────────────────────────────┐
│         EXERCISE → SECTION MAPPING                          │
│                                                             │
│   Exercise 1: OpenGL Window                                 │
│   └─► Practices: Section 4 (Init) + Section 7 (Swap)        │
│       Skills: Context creation, buffer swapping             │
│                                                             │
│   Exercise 2: Checkerboard Pattern                          │
│   └─► Practices: Section 5 (Upload) + Section 6 (Draw)      │
│       Skills: CPU rendering, texture upload, quad drawing   │
│                                                             │
│   Exercise 3: Animated Gradient                             │
│   └─► Practices: All sections in game loop                  │
│       Skills: Per-frame updates, smooth animation           │
│                                                             │
│   Exercise 4: Performance Measurement                       │
│   └─► Practices: Section 7 (XSync) + fps-implementation.md  │
│       Skills: Timing, optimization, comparison              │
│                                                             │
│   RECOMMENDED ORDER: 1 → 2 → 3 → 4                          │
│   Each exercise builds on the previous!                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Exercise 1: Simple OpenGL Window

Create a window that clears to a color.

```c
// exercise_1_gl.c
// Compile: gcc -o ex1 exercise_1_gl.c -lX11 -lGL -lGLX

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>

int main() {
    // 1. Open display
    Display *display = XOpenDisplay(NULL);

    // 2. Choose visual
    int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(display, 0, attribs);

    // 3. Create window
    Window root = DefaultRootWindow(display);
    XSetWindowAttributes swa = {
        .colormap = XCreateColormap(display, root, vi->visual, AllocNone),
        .event_mask = ExposureMask | KeyPressMask
    };

    Window win = XCreateWindow(display, root, 0, 0, 640, 480, 0,
        vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    XMapWindow(display, win);
    XStoreName(display, win, "Exercise 1: OpenGL Clear Color");

    // 4. Create and bind GL context
    GLXContext ctx = glXCreateContext(display, vi, NULL, True);
    glXMakeCurrent(display, win, ctx);

    // 5. Main loop - cycle through colors
    float r = 0.0f;
    while (1) {
        // TODO: Handle events (press Q to quit)

        // Clear to changing color
        glClearColor(r, 0.3f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(display, win);

        r += 0.001f;
        if (r > 1.0f) r = 0.0f;
    }

    return 0;
}
```

**Goal**: See a smoothly changing background color.

### Exercise 2: Display a CPU-Generated Pattern

Create a checkerboard pattern in CPU memory and display it.

```c
// Create a simple checkerboard pattern
void generate_checkerboard(u8_t *pixels, int width, int height) {
    int tile_size = 32;  // 32x32 pixel tiles

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int tile_x = x / tile_size;
            int tile_y = y / tile_size;

            // Alternate colors
            int is_white = (tile_x + tile_y) % 2;

            int idx = (y * width + x) * 4;  // 4 bytes per pixel (RGBA)
            pixels[idx + 0] = is_white ? 255 : 0;    // R
            pixels[idx + 1] = is_white ? 255 : 0;    // G
            pixels[idx + 2] = is_white ? 255 : 0;    // B
            pixels[idx + 3] = 255;                   // A
        }
    }
}

// TODO: Upload this to a texture and display it
```

**Goal**: See a checkerboard pattern on screen.

### Exercise 3: Animated Gradient

Animate the gradient from Handmade Hero:

```c
void render_animated_gradient(u8_t *pixels, int width, int height, int frame) {
    int x_offset = frame * 2;  // Move 2 pixels per frame
    int y_offset = frame;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;

            // Animated gradient
            pixels[idx + 0] = (u8_t)(x + x_offset);  // R
            pixels[idx + 1] = (u8_t)(y + y_offset);  // G
            pixels[idx + 2] = 128;                       // B
            pixels[idx + 3] = 255;                       // A
        }
    }
}
```

**Goal**: See a smoothly scrolling gradient.

### Exercise 4: Performance Measurement

Add timing to compare XPutImage vs OpenGL:

```c
#include <time.h>

double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// In your render loop:
double start = get_time_ms();

// Option A: XPutImage
// XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);

// Option B: OpenGL
glTexImage2D(...);
glBegin(GL_QUADS); /* ... */ glEnd();
glXSwapBuffers(display, window);
XSync(display, False);

double end = get_time_ms();
printf("Frame time: %.2fms\n", end - start);
```

**Goal**: Measure and compare performance of both methods.

### 🔗 Connection to Next Section

After completing these exercises, you'll have hands-on experience with every concept. Section 12 provides **resources** for deeper learning.

---

## 12. Resources

### 🔗 Why This Section Matters

This guide covers the essentials. These resources help you go deeper into specific areas based on your needs.

### Official Documentation

- **OpenGL Registry**: [khronos.org/registry/OpenGL](https://www.khronos.org/registry/OpenGL/)
- **GLX Specification**: [khronos.org/registry/OpenGL/specs/gl](https://www.khronos.org/registry/OpenGL/specs/gl/)
- **OpenGL Reference Pages**: [docs.gl](https://docs.gl/)

### Tutorials

- **LearnOpenGL.com**: [learnopengl.com](https://learnopengl.com/) (Modern OpenGL, but concepts transfer)
- **OpenGL Tutorial**: [opengl-tutorial.org](http://www.opengl-tutorial.org/)
- **NeHe Productions**: [nehe.gamedev.net](http://nehe.gamedev.net/) (Classic, fixed-function pipeline)

### Books

- **"OpenGL Programming Guide"** (Red Book) - Comprehensive reference
- **"OpenGL SuperBible"** - Practical, example-driven

### Videos

- **Handmade Hero Day 237-239** - Casey implements OpenGL renderer
- **The Cherno's OpenGL Series**: YouTube playlist on modern OpenGL

### Linux-Specific

- **Xlib Programming Manual**: X11 fundamentals
- **GLX Documentation**: `man glXChooseVisual`, `man glXCreateContext`

### Learning Path Recommendation

```
┌─────────────────────────────────────────────────────────────┐
│         RECOMMENDED LEARNING PATH                           │
│                                                             │
│   BEGINNER (this guide level):                              │
│   1. Complete all exercises in this guide                   │
│   2. Read fps-implementation.md for timing context          │
│   3. Study the actual backend.c implementation              │
│                                                             │
│   INTERMEDIATE (deeper understanding):                      │
│   1. NeHe Tutorials - classic fixed-function pipeline       │
│   2. OpenGL Programming Guide (Red Book) chapters 1-5       │
│   3. Handmade Hero Days 237-239                             │
│                                                             │
│   ADVANCED (modern techniques):                             │
│   1. LearnOpenGL.com - modern OpenGL with shaders           │
│   2. OpenGL SuperBible - comprehensive modern coverage      │
│   3. GPU programming and shader optimization                │
│                                                             │
│   YOUR CURRENT POSITION: Beginner → Intermediate            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Quick Reference Card

### 🔗 Why This Section Matters

This is your **cheat sheet** for daily use. Print it or keep it open while coding.

```
┌─────────────────────────────────────────────────────────────┐
│                 OPENGL QUICK REFERENCE                      │
├─────────────────────────────────────────────────────────────┤
│  INITIALIZATION                                             │
│  ──────────────                                             │
│  glXChooseVisual()     → Find compatible visual             │
│  glXCreateContext()    → Create GL context                  │
│  glXMakeCurrent()      → Activate context                   │
│  glGenTextures()       → Create texture ID                  │
│  glBindTexture()       → Select texture                     │
│  glTexParameteri()     → Set texture options                │
│  glOrtho()             → Setup 2D projection                │
├─────────────────────────────────────────────────────────────┤
│  EACH FRAME                                                 │
│  ──────────                                                 │
│  glTexImage2D()        → Upload pixels to GPU               │
│  glEnable(GL_TEXTURE_2D)                                    │
│  glBegin(GL_QUADS)     → Start drawing                      │
│    glTexCoord2f()      → Texture coordinates                │
│    glVertex2f()        → Vertex positions                   │
│  glEnd()               → Finish drawing                     │
│  glXSwapBuffers()      → Display frame                      │
├─────────────────────────────────────────────────────────────┤
│  PIXEL FORMAT                                               │
│  ────────────                                               │
│  GL_RGBA + GL_UNSIGNED_BYTE = 4 bytes per pixel             │
│  [R][G][B][A] = [0-255][0-255][0-255][0-255]                │
├─────────────────────────────────────────────────────────────┤
│  TEXTURE FILTERING                                          │
│  ─────────────────                                          │
│  GL_NEAREST = Pixel-perfect (no blur)                       │
│  GL_LINEAR  = Smooth interpolation (blur)                   │
├─────────────────────────────────────────────────────────────┤
│  COMMON ERRORS                                              │
│  ─────────────                                              │
│  Upside down → Fix glOrtho Y parameters                     │
│  Wrong colors → Check pixel format (RGBA vs BGRA)           │
│  Black screen → Enable GL_TEXTURE_2D                        │
│  No texture → Call glBindTexture before upload              │
└─────────────────────────────────────────────────────────────┘
```

---

## 🎯 Final Summary: Your Learning Journey

```
┌─────────────────────────────────────────────────────────────┐
│         COMPLETE LEARNING PATH THROUGH THIS GUIDE           │
│                                                             │
│   PART 1: FOUNDATION (Sections 1-3)                         │
│   ├─► What OpenGL is and why we use it                      │
│   ├─► How it solves the XPutImage color format problem      │
│   └─► The simplified rendering pipeline mental model        │
│                                                             │
│   PART 2: CONCEPTS (Section 3 detailed)                     │
│   ├─► Init once vs per-frame operations                     │
│   └─► How OpenGL fits into the 11-step game loop            │
│                                                             │
│   PART 3: IMPLEMENTATION (Sections 4-7)                     │
│   ├─► Section 4: One-time initialization (4 steps)          │
│   ├─► Section 5: Per-frame texture upload                   │
│   ├─► Section 6: Drawing textured quads                     │
│   └─► Section 7: Buffer swapping and GPU sync               │
│                                                             │
│   PART 4: REFERENCE (Sections 8-9)                          │
│   ├─► Data structures that hold state                       │
│   └─► Complete function reference                           │
│                                                             │
│   PART 5: DEBUGGING (Section 10)                            │
│   └─► Common pitfalls and their fixes                       │
│                                                             │
│   PART 6: PRACTICE (Sections 11-12)                         │
│   ├─► Hands-on exercises                                    │
│   └─► Resources for continued learning                      │
│                                                             │
│   ═══════════════════════════════════════════════════════   │
│   RELATED GUIDE: fps-implementation.md                      │
│   └─► Frame timing, adaptive FPS, sleep strategies          │
│   ═══════════════════════════════════════════════════════   │
│                                                             │
│   You now understand how to display CPU-rendered frames     │
│   on Linux using OpenGL. Combined with the FPS guide,       │
│   you have all the knowledge for a smooth game engine!      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

_Last updated: Based on Handmade Hero X11 OpenGL backend implementation_
