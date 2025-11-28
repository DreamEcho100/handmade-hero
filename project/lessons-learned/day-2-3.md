# Handmade Hero Days 2-3: Back Buffers, Pixel Math, and the Art of Not Crashing

## Day 3: Back Buffer & Pixel Drawing

### Key Concepts

#### 1. **Double Buffering** (The "Back Buffer")

- **Problem**: Drawing directly to screen causes flickering
- **Solution**: Draw to memory buffer first, then copy to screen all at once
- **Analogy**: Like preparing a painting in your studio before hanging it on a gallery wall

```
Back Buffer (RAM)        XPutImage()      Window (Screen)
┌──────────────┐         ────────→        ┌──────────────┐
│ Draw here    │                          │ User sees    │
│ in memory    │         Copy once        │ this         │
└──────────────┘                          └──────────────┘
```

#### 2. **Pixel Addressing: 2D → 1D Mapping**

The most important formula in graphics programming:

```c
// 2D coordinates (x, y) → 1D array offset
offset = y * width + x
		 ↑           ↑
		Row #      Column #
```

**Visual Explanation:**

```
Screen (800×600):
	 0   1   2   3 ... 799
	 ┌─┬─┬─┬─┬─────────┬─┐
 0 │X│ │ │ │         │ │ ← Row 0 (pixels 0-799)
	 ├─┼─┼─┼─┼─────────┼─┤
 1 │ │X│ │ │         │ │ ← Row 1 (pixels 800-1599)
	 ├─┼─┼─┼─┼─────────┼─┤
 2 │ │ │X│ │         │ │ ← Row 2 (pixels 1600-2399)
	 └─┴─┴─┴─┴─────────┴─┘

Memory (1D array):
[0][1][2]...[799][800][801]...[1599][1600][1601]...
 ↑   ↑               ↑                  ↑
(0,0)(1,0)         (0,1)              (0,2)
```

**Examples:**

- Pixel `(0,0)` = offset `0 * 800 + 0 = 0`
- Pixel `(5,0)` = offset `0 * 800 + 5 = 5`
- Pixel `(0,1)` = offset `1 * 800 + 0 = 800` ← Start of row 1!
- Pixel `(5,3)` = offset `3 * 800 + 5 = 2405`

#### 3. **Common Bugs I Hit (And Fixed!)**

##### Bug #1: Uninitialized Memory

```c
// ❌ WRONG: Random garbage in memory
uint8_t *buffer = malloc(width * height * 4);

// ✅ CORRECT: Zero-initialized memory
uint8_t *buffer = calloc(width * height, 4);
// 8× FASTER than malloc + memset! (OS uses copy-on-write zero pages)
```

**Result**: Window showed random colored pixels until resized

##### Bug #2: Buffer Overflow (Segfault!)

```c
// ❌ WRONG: i * width + i treats 'i' as BOTH x AND y!
for (int i = 0; i < 1000; i++) {
	int offset = i * width + i;  // offset = i * (width + 1)
	// At i=1000: offset = 1000 * 801 = 801,000
	// But buffer only has 800×600 = 480,000 pixels!
	pixels[offset] = white;  // 💥 CRASH!
}

// ✅ CORRECT: Separate x and y coordinates
for (int i = 0; i < min(width, height); i++) {
	int x = i;
	int y = i;
	int offset = y * width + x;  // Now i is ONLY the loop counter
	if (offset >= 0 && offset < total_pixels) {  // Bounds check!
		pixels[offset] = white;
	}
}
```

**Result**: Segmentation fault at i=1000 (accessing memory way beyond buffer)

##### Bug #3: NULL Pointer Access

```c
// ❌ WRONG: Drawing before buffer exists!
XMapWindow(display, window);
// Event loop starts...
// User resizes → ConfigureNotify → resize_back_buffer() allocates buffer

// But if we try to draw BEFORE ConfigureNotify fires:
if (g_PixelData) {  // g_PixelData is NULL!
	// This check prevents crash, but nothing gets drawn
}

// ✅ CORRECT: Allocate buffer BEFORE event loop
XMapWindow(display, window);
resize_back_buffer(display, g_BufferWidth, g_BufferHeight);
// NOW g_PixelData exists!
```

**Result**: "ERROR: Tried to access pixel 0 (out of X)" messages

#### 4. **Casey's Philosophy: Resource Lifetimes in Waves**

**Wave 1: Process Lifetime**

- Resources that live for the entire program
- OS cleans them up when process exits
- **No manual cleanup needed!**

```c
// ❌ UNNECESSARY (Casey's point):
XDestroyImage(g_BackBuffer);  // OS does this!
XDestroyWindow(window);       // OS does this!
XCloseDisplay(display);       // OS does this!

// ✅ Just exit! OS handles it:
return 0;
```

**Wave 2: State Lifetime**

- Resources that change during runtime
- **DO need manual cleanup** before reallocation

```c
// ✅ NECESSARY:
void resize_back_buffer() {
	if (g_BackBuffer) {
		XDestroyImage(g_BackBuffer);  // Free old buffer
	}
	g_BackBuffer = XCreateImage(...);  // Allocate new buffer
}
```

**Wave 3: Frame Lifetime** (Day 4+)

- Temporary data for a single frame
- Cleared/reset every frame

### Web Dev Analogies

```javascript
// Canvas API (similar to our back buffer):
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const imageData = ctx.getImageData(0, 0, width, height);
const pixels = imageData.data; // Uint8ClampedArray (RGBA)

// Draw diagonal line
for (let i = 0; i < Math.min(width, height); i++) {
  const offset = (i * width + i) * 4; // *4 because RGBA (not ARGB)
  pixels[offset + 0] = 255; // R
  pixels[offset + 1] = 255; // G
  pixels[offset + 2] = 255; // B
  pixels[offset + 3] = 255; // A
}

ctx.putImageData(imageData, 0, 0); // Like our XPutImage!
```

### Performance Notes

#### calloc vs malloc + memset

```c
// Naive approach:
void* buffer = malloc(size);
memset(buffer, 0, size);  // Writes zeros to every byte

// Smart approach:
void* buffer = calloc(count, element_size);  // OS tricks!
```

**Why calloc is 8× faster:**

1. OS keeps a pool of zero pages
2. calloc just maps your buffer to these pages (copy-on-write)
3. No actual memory writes until you modify it!
4. memset forces immediate writes to every byte

**Benchmark** (1920000 bytes):

- malloc + memset: ~1.2 ms
- calloc: ~0.15 ms

### What's Next?

**Day 4: Proper Game Loop**

- Move drawing out of event handling
- Frame timing (target 60 FPS)
- Animated gradient (Casey's signature demo)
- Proper render function

**Current Issues:**

- Drawing happens once when window closes (wrong!)
- Need to draw every frame continuously
- Need to separate input handling from rendering

### Files Changed

- `project/src/platform/x11_backend.c`:

  - Implemented `resize_back_buffer()` (Wave 2 resource management)
  - Added `update_window()` (XPutImage blitting)
  - Added diagonal line drawing test (temporary, for learning)
  - Applied Casey's Wave 1 philosophy (removed exit cleanup)
  - Fixed buffer overflow bug
  - Fixed NULL pointer bug

- `llm.txt`:
  - Added 12 Casey Muratori philosophies
  - Added common Casey sayings
  - Comprehensive reference for AI assistant

### Commands Reference

```bash
# Build and run (X11 backend)
./build.sh x11 && ./build/game

# Build and run (Raylib backend - future)
./build.sh raylib && ./build/game
```

### Debugging Checklist

When you get a segfault in graphics code:

1. ✅ Is the buffer allocated?
2. ✅ Is the offset calculation correct? (offset = y \* width + x)
3. ✅ Is there bounds checking? (offset < total_pixels)
4. ✅ Are x and y within range? (x < width, y < height)
5. ✅ Is the pixel format correct? (uint32_t for ARGB, not uint8_t)

---

**Date**: 2025
**Casey Muratori's Handmade Hero**: https://handmadehero.org
**My Implementation**: Linux X11 backend (learning systems programming)
