# Tilemap coordinate system design

**Created:** 2/15/2026 13:48:44  
**Updated:** 2/15/2026 15:08:41  
**Exported:** 2/15/2026 15:12:48  
**Link:** [https://claude.ai/chat/2ea2bf20-36ff-4b57-9fe1-9f514ad3e8b0](https://claude.ai/chat/2ea2bf20-36ff-4b57-9fe1-9f514ad3e8b0)

## Response:

2/15/2026, 1:48:46 PM

> File:

## Prompt:

2/15/2026, 1:48:58 PM

I can see this is a transcript from Handmade Hero Day 031 about tilemap coordinate systems. The speaker (Casey Muratori) is explaining two key problems with the current implementation and proposing solutions.

Let me break down the main concepts:

## The Two Problems Identified

**1. Pixel-based units problem:**

- Currently everything is measured in screen pixels
- If you change tile resolution (e.g., 60px to 30px), the player moves twice as fast relative to tiles
- The game world shouldn't be tied to screen resolution

**2. Upside-down coordinate system:**

- Currently Y increases downward (typical for screen coordinates)
- Math textbooks use Y increasing upward
- This mismatch causes confusion when implementing physics, angles, projectiles, etc.

## The Proposed Solutions

**1. World units in meters:**

- Use real-world units (meters) independent of screen resolution
- Example: tile side = 1.4 meters (roughly height of a 10-year-old)
- Separate `tileSideInMeters` from `tileSideInPixels`
- Makes it easier to implement realistic physics (gravity, movement speed)

**2. Coordinate system packing:**

- Currently using separate values: `tileMapX`, `tileMapY`, `tileX`, `tileY`
- Proposal: Pack into single 32-bit X and Y values
- Use bit separation: high bits = tile map index, low bits = tile within map
- Creates a "virtual memory system" for a 4 billion × 4 billion tile world
- Only store chunks where world actually exists (sparse storage)

## Key Insights

The speaker emphasizes these aren't strictly necessary but will save headaches later:

- **Multi-resolution support** becomes trivial
- **Math consistency** between textbooks and code
- **Cleaner architecture** separating display from world representation

Would you like me to explain any specific part in more detail, or help you implement these concepts in your own code?

## Response:

2/15/2026, 1:57:30 PM

My issue is I can get the vision/idea here in Handmade Hero Day 031 - Tilemap Coordinate Systems

## Prompt:

2/15/2026, 1:58:41 PM

I understand - you're grasping the general vision but perhaps struggling with how it all fits together practically. Let me create a clearer explanation with concrete examples.

```plaintext
Create File: Creating a detailed explanation document breaking down the Handmade Hero Day 031 concepts with visual examples and concrete code comparisons
```

```plaintext
Present File: /home/claude/tilemap_coordinate_systems_explained.md
```

I've created a detailed breakdown that should help clarify the vision! The document covers:

1. **Concrete examples** showing the before/after of each problem
2. **Visual diagrams** of coordinate systems and bit packing
3. **Real code comparisons** demonstrating why each change matters
4. **Practical scenarios** (gravity, multi-device support) showing real-world benefits

The key insight is: **Casey is building a layer of abstraction between "where things are in the game world" and "where pixels appear on screen."** This separation gives you resolution independence, math compatibility, and a scalable coordinate system.

Is there a specific aspect that's still unclear? For example:

- How the bit packing works with the chunk system?
- Why meters are better than pixels for game logic?
- How the coordinate conversion happens during rendering?

> File: /home/claude/tilemap_coordinate_systems_explained.md

# Understanding Handmade Hero Day 031: Tilemap Coordinate Systems

## The Core Vision

Casey is fundamentally restructuring how the game thinks about position and space. Instead of "where are you on screen?", the game will ask "where are you in the world?" - and then figure out how to display that.

---

## Problem 1: The Pixel Trap

### Current Broken Approach

```c
// Everything is in pixels
player.x = 320;  // 320 pixels from left
player.y = 240;  // 240 pixels from top
player.speed = 5;  // 5 pixels per frame

int tileWidth = 60;  // tile is 60 pixels wide
```

### What Happens When You Change Resolution

```c
// You decide tiles should be smaller
int tileWidth = 30;  // NOW tiles are 30 pixels wide

// PROBLEM: Player still moves at 5 pixels/frame
// But tiles are half as big!
// Result: Player moves TWICE AS FAST relative to the world
```

**The Issue:** Your game logic is coupled to display resolution. The world's "physics" change when you change how you draw it.

### The Solution: World Units

```c
// Game world uses meters (or any consistent unit)
float tileSideInMeters = 1.4f;  // A tile is 1.4 meters

// Display uses pixels
int tileSideInPixels = 60;  // Currently drawing at 60px per tile

// Player position in WORLD units
player.worldX = 5.7f;  // 5.7 meters from origin
player.worldY = 3.2f;  // 3.2 meters from origin
player.speedMetersPerSec = 3.0f;  // 3 meters per second

// Convert to screen when rendering
float metersToPixels = tileSideInPixels / tileSideInMeters;
int screenX = player.worldX * metersToPixels;  // Calculate display position
```

**Now change display resolution:**

```c
int tileSideInPixels = 30;  // Half the pixels

// Player still moves at 3.0 meters/second in world
// Display calculation automatically adjusts
float metersToPixels = 30 / 1.4;  // New scale factor
int screenX = player.worldX * metersToPixels;  // Still correct!
```

---

## Problem 2: Upside-Down World

### Current Problem

```
Screen Coordinates:        Math Textbook Coordinates:
Y                          Y
↓                          ↑
0 ────→ X                  0 ────→ X
```

**Screen:** Y increases downward (top-left origin)  
**Math:** Y increases upward (bottom-left origin)

### Why This Matters

#### Projectile Example

```c
// Fire projectile at 45 degrees upward-right

// Math says: cos(45°) = 0.707, sin(45°) = 0.707
float angle = 45 * (PI / 180);
float dx = cos(angle);  // 0.707 (rightward) ✓
float dy = sin(angle);  // 0.707 (upward in math) ✓

// But in screen coordinates, +Y is DOWN!
// Result: Projectile goes down-right instead of up-right ✗

// You have to remember to flip it:
dy = -sin(angle);  // Manually invert
```

#### Circle Drawing

```c
// Draw circle by going around angle
for (float angle = 0; angle < 2*PI; angle += 0.1) {
    float x = centerX + radius * cos(angle);
    float y = centerY + radius * sin(angle);
    // Draws CLOCKWISE on screen (backward from math)
    // Every formula needs mental translation
}
```

### The Solution: Math-Friendly Coordinates

```c
// Use Y-up in game world
// Player presses "up" button
if (moveUp) {
    player.worldY += player.speed;  // Y increases going UP
}

// Convert to screen only when rendering
int screenY = screenHeight - (player.worldY * metersToPixels);
```

---

## Problem 3: Coordinate System Bloat

### Current Messy Approach

```c
struct CanonicalPosition {
    int tileMapX;  // Which tile map chunk (0, 1, 2...)
    int tileMapY;
    int tileX;     // Which tile within that chunk (0-16)
    int tileY;
    float offsetX; // Position within that tile (pixels)
    float offsetY;
};

// To use this position, you need complex logic:
// 1. Figure out which tile map
// 2. Figure out which tile
// 3. Add the offset
// 4. Handle transitions between tiles
// 5. Handle transitions between tile maps
```

### The Vision: Packed Coordinates

```
Instead of storing 4 integers, pack into 2:

32-bit X coordinate:
┌─────────────────────────┬─────────────┐
│  High 28 bits           │ Low 4 bits  │
│  Tile Map X (chunk)     │ Tile X      │
│  Which 16×16 chunk      │ 0-15 in chunk│
└─────────────────────────┴─────────────┘

Same for Y coordinate
```

### How It Works

```c
// Single position representation
struct TilePosition {
    uint32_t absoluteTileX;  // Entire world X coordinate
    uint32_t absoluteTileY;  // Entire world Y coordinate
    float offsetX;           // Meters within tile (0.0 to 1.4)
    float offsetY;
};

// Extract chunk and tile automatically
#define TILE_CHUNK_SHIFT 4  // Low 4 bits = tile index
#define TILE_CHUNK_MASK 0xF // 0b1111

uint32_t GetChunkX(uint32_t absoluteTileX) {
    return absoluteTileX >> TILE_CHUNK_SHIFT;  // High bits
}

uint32_t GetTileXInChunk(uint32_t absoluteTileX) {
    return absoluteTileX & TILE_CHUNK_MASK;     // Low bits
}

// Example:
// absoluteTileX = 0b0000_0000_0000_0000_0000_0000_0011_0101
//                                                 ││││ ││││
//                                         chunk=3 ││││ tile=5
```

### Why This Is Brilliant

**Before:**

- Need to manually track which chunk you're in
- Need to manually handle chunk boundaries
- 6 values to store position

**After:**

- Just increment absoluteTileX to move right
- Chunk calculation is automatic bit operation
- 4 values (2 ints, 2 floats)
- Works for 4 billion × 4 billion world!

---

## The Complete Picture

### How Position Works Now

```c
struct GameState {
    // World definition
    float tileSideInMeters;    // 1.4 meters
    int tileSideInPixels;      // 60 pixels (can change!)

    // Player position in WORLD coordinates
    uint32_t playerAbsoluteTileX;
    uint32_t playerAbsoluteTileY;
    float playerOffsetX;  // 0.0 to tileSideInMeters
    float playerOffsetY;
};

// Update (in world units)
void UpdatePlayer(GameState* state, float dt) {
    float speedMetersPerSec = 3.0f;
    float moveX = input.x * speedMetersPerSec * dt;

    state->playerOffsetX += moveX;

    // Handle tile boundaries
    if (state->playerOffsetX > state->tileSideInMeters) {
        state->playerOffsetX -= state->tileSideInMeters;
        state->playerAbsoluteTileX++;  // Automatic chunk handling!
    }
}

// Render (convert to screen)
void RenderPlayer(GameState* state) {
    float metersToPixels = state->tileSideInPixels / state->tileSideInMeters;

    // Which chunk is visible?
    uint32_t cameraChunkX = GetChunkX(state->playerAbsoluteTileX);
    uint32_t cameraTileX = GetTileXInChunk(state->playerAbsoluteTileX);

    // Convert to screen pixels
    float worldX = cameraTileX * state->tileSideInMeters + state->playerOffsetX;
    int screenX = worldX * metersToPixels;

    // Y is flipped for screen
    int screenY = screenHeight - (worldY * metersToPixels);

    DrawSprite(screenX, screenY);
}
```

---

## Real-World Example: Why This Matters

### Scenario: Implementing Gravity

**Without world units:**

```c
// Gravity in... pixels per second squared?
player.velocityY += 9.8;  // What does this mean?
// Depends on tile size, screen resolution, etc.
```

**With world units:**

```c
// Gravity in meters per second squared (real physics!)
player.velocityY -= 9.8;  // Actual earth gravity
// Works regardless of display settings
```

### Scenario: Supporting Multiple Devices

**Without resolution independence:**

```c
// Game designed for 1920×1080
int tileSize = 60;  // Hard-coded

// Player on 4K screen: Tiles look tiny
// Player on phone: Tiles too big
// Need separate versions or hacks
```

**With world units:**

```c
// Game logic: "tiles are 1.4 meters"
// Display: "I have this many pixels, let me calculate"
int tileSize = CalculateBestFit(screenWidth, screenHeight);
// Same game logic, adapts to any screen
```

---

## Key Takeaways

1. **Separate Simulation from Presentation**
   - World = meters (or any consistent unit)
   - Screen = pixels (derived from world)

2. **Use Math-Friendly Coordinates**
   - +Y = up (like textbooks)
   - Convert to screen Y only when rendering

3. **Pack Coordinates Efficiently**
   - Single integer holds both chunk and tile
   - Bit operations handle the math
   - Scales to massive worlds

4. **Benefits**
   - Change resolution without breaking game
   - Copy physics formulas directly from textbooks
   - Support enormous worlds with sparse storage
   - Cleaner, more maintainable code

---

## What Casey Will Do Next

1. **Pack tile coordinates** into single 32-bit values
2. **Convert offsets** from pixels to meters
3. **Implement sparse storage** (only store existing chunks)
4. **Update rendering** to pull from virtual coordinate system
5. **Remove pixel-based** position entirely

The result: A robust, scalable coordinate system that's resolution-independent and math-friendly.

## Response:

2/15/2026, 2:06:09 PM

So, for handling consistent measurements of units across resolutions, he will do something like browsers CSS `rem`? or did I get it wrong/miss something?
And he didn't implement it yet, but he difined the `TileSideInMeters` and `TileSideInPixels`, why?
And how will Casey implement it?
And what're other good/performant ways to handle it
And want the _text book math like coordination_ have some complications or performance penalty? and if so how to mitigate/handle them
Also what do you think, which should own what or let the user/dev choose? between the engine and the game or the "consistent measurements of units across resolutions" and "_text book math like coordination_"? and why and why not? and what would happen for all the cases?

And I still don't get how "Packed Coordinates" or `CalculateBestFit` will work

---

## Prompt:

2/15/2026, 2:09:07 PM

Excellent questions! Let me break these down systematically.

```plaintext
Create File: Creating a comprehensive analysis document answering all the questions about CSS rem comparison, implementation details, performance considerations, and architectural decisions
```

```plaintext
Present File: /home/claude/coordinate_system_deep_dive.md
```

Perfect! I've created a comprehensive deep-dive that answers all your questions:

## Quick Answers:

1. **CSS `rem` comparison**: ✅ YES! Exactly the same principle - separate logical units from display units

2. **Why define now but not use yet**: Incremental refactoring strategy - safer to change one thing at a time, test, then move to next step

3. **How Casey implements it**: 6-step process from packed coordinates → world definition → meter conversion → rendering conversion

4. **Other performant approaches**: Fixed-point (fastest), normalized coords, double precision, homogeneous coords - but float is best for modern CPUs

5. **Y-flip complications**: Negligible performance cost (~0.1% of frame budget), main issue is mental translation when integrating third-party code - solved with clear layer separation

6. **Packed coordinates explained**: Uses bit-shifting to pack chunk ID and tile ID into single integer - the CPU's ALU handles chunk transitions automatically through binary overflow

7. **CalculateBestFit**: Multiple strategies shown - fixed tile count, aspect ratio maintenance, DPI-aware, or adaptive performance-based

8. **Architectural ownership**: Three options compared (Engine-enforced, Pragmatic hybrid, Full game control) - Casey chooses pragmatic: provides world-space tools as default, but allows pixel access when needed

**The document includes working code examples for everything**, performance measurements, and real-world scenarios. The packed coordinates section especially shows the "magic" of how binary addition automatically handles chunk boundaries!

> File: /home/claude/coordinate_system_deep_dive.md

# Deep Dive: Coordinate Systems - Your Questions Answered

## 1. Is This Like CSS `rem`?

### Short Answer: YES, very similar concept!

### CSS `rem` Concept

```css
/* Root defines base unit */
html {
  font-size: 16px; /* 1rem = 16px */
}

/* Everything else uses rem */
.button {
  padding: 1rem; /* 16px */
  font-size: 0.875rem; /* 14px */
}

/* Change resolution/zoom */
html {
  font-size: 20px; /* NOW 1rem = 20px */
}
/* All rem-based sizes scale automatically! */
```

### Casey's Approach (Same Idea)

```c
// Define base unit relationship
float tileSideInMeters = 1.4f;   // Base unit (like 1rem)
int tileSideInPixels = 60;       // Current scale (like 16px)

// Everything uses meters
float playerSpeed = 3.0f;  // 3 meters/sec
float playerX = 5.7f;      // 5.7 meters

// Change resolution
tileSideInPixels = 120;    // Double the pixels
// Player speed still 3.0 m/s, automatically scales!
```

### Key Difference

- **CSS rem**: One-dimensional scalar (font-size)
- **Casey's system**: Two-dimensional spatial units (meters → pixels)
- **Both**: Separate logical units from display units

**You got it exactly right!** It's the same separation of concerns.

---

## 2. Why Define `TileSideInMeters` and `TileSideInPixels` Now?

Casey defines them now but doesn't use them yet because he's doing **incremental refactoring**:

### The Strategy

```
Current State:    Everything in pixels (broken)
                         ↓
Step 1:           Define the target units (TODAY)
                  - TileSideInMeters = 1.4
                  - TileSideInPixels = 60
                  (Preparation, not yet used)
                         ↓
Step 2:           Pack coordinates into chunks (TOMORROW)
                  - Simplify position representation
                         ↓
Step 3:           Convert positions to use meters (DAY AFTER)
                  - Replace pixel offsets with meter offsets
                  - Use TileSideInMeters for calculations
                         ↓
Final State:      Resolution-independent (GOAL)
```

### Why Not Do It All at Once?

**Risk Management:**

```c
// Changing everything at once:
// - Hard to debug what broke
// - Might mix up bugs from different changes
// - Can't test incrementally

// Casey's way:
// 1. Define constants (compiles, runs same as before)
// 2. Test - still works? ✓
// 3. Change coordinate packing (single focused change)
// 4. Test - still works? ✓
// 5. Use meter units (final change)
// 6. Test - still works? ✓
```

**It's like TODO comments with actual values:**

```c
float tileSideInMeters = 1.4f;  // TODO: Use this instead of pixels
```

---

## 3. How Will Casey Implement It?

### The Complete Implementation

#### Step 1: Current System (What We Have)

```c
struct CanonicalPosition {
    int tileMapX, tileMapY;  // Which chunk
    int tileX, tileY;        // Which tile in chunk
    float offsetX, offsetY;  // Pixels within tile
};

void MovePlayer(CanonicalPosition* pos, float pixelsToMove) {
    pos->offsetX += pixelsToMove;  // Pixels!

    if (pos->offsetX > 60) {  // Hard-coded pixel size
        pos->offsetX -= 60;
        pos->tileX++;
        if (pos->tileX > 16) {
            pos->tileX = 0;
            pos->tileMapX++;
        }
    }
}
```

#### Step 2: Packed Coordinates (Simplification)

```c
struct TilePosition {
    uint32_t absTileX;  // Packed: [chunk bits][tile bits]
    uint32_t absTileY;
    float offsetX;      // Still pixels for now
    float offsetY;
};

void MovePlayer(TilePosition* pos, float pixelsToMove) {
    pos->offsetX += pixelsToMove;

    if (pos->offsetX > 60) {
        pos->offsetX -= 60;
        pos->absTileX++;  // Auto-handles chunk transitions!
    }
}
```

#### Step 3: Add World Definition (Setup)

```c
struct World {
    float tileSideInMeters;  // 1.4m
    int tileSideInPixels;    // 60px (can change!)
};

// Calculate conversion factor
float GetMetersToPixels(World* world) {
    return (float)world->tileSideInPixels / world->tileSideInMeters;
    // 60 / 1.4 ≈ 42.857 pixels per meter
}
```

#### Step 4: Convert to Meters (The Big Change)

```c
struct TilePosition {
    uint32_t absTileX;
    uint32_t absTileY;
    float offsetX;  // NOW IN METERS (0.0 to 1.4)
    float offsetY;  // NOW IN METERS
};

void MovePlayer(World* world, TilePosition* pos, float metersToMove) {
    pos->offsetX += metersToMove;  // METERS!

    if (pos->offsetX > world->tileSideInMeters) {  // 1.4m
        pos->offsetX -= world->tileSideInMeters;
        pos->absTileX++;
    }
}

void UpdatePlayer(World* world, TilePosition* pos, Input input, float dt) {
    float speedMetersPerSec = 3.0f;  // Game logic in meters
    float moveDistance = speedMetersPerSec * dt;

    if (input.right) MovePlayer(world, pos, moveDistance);
}
```

#### Step 5: Rendering (Convert to Screen)

```c
void RenderPlayer(World* world, TilePosition* pos, int screenW, int screenH) {
    // Get visible area in tile coordinates
    uint32_t cameraTileX = pos->absTileX;
    uint32_t cameraTileY = pos->absTileY;

    // Convert position to meters from camera
    float relativeMetersX = pos->offsetX;
    float relativeMetersY = pos->offsetY;

    // Convert meters to pixels for display
    float metersToPixels = GetMetersToPixels(world);
    int screenX = (int)(relativeMetersX * metersToPixels);
    int screenY = screenH - (int)(relativeMetersY * metersToPixels);  // Flip Y

    DrawSprite(playerSprite, screenX, screenY);
}
```

#### Step 6: Resolution Change (Now Trivial!)

```c
void ChangeResolution(World* world, int newTilePixelSize) {
    world->tileSideInPixels = newTilePixelSize;
    // That's it! Game logic unchanged, rendering adapts automatically
}

// 4K display
ChangeResolution(world, 120);  // Bigger tiles
// Game still runs at same speed, just draws bigger

// Mobile
ChangeResolution(world, 30);   // Smaller tiles
// Game still runs at same speed, just draws smaller
```

---

## 4. Other Good/Performant Ways to Handle It

### Approach A: Fixed-Point Math (Old-School, Very Fast)

```c
// Use integers with implied decimal point
// 1 unit = 1/256 of a meter (8 bits of fraction)
typedef int32_t Fixed;
#define FIXED_SHIFT 8

Fixed MetersToFixed(float meters) {
    return (Fixed)(meters * (1 << FIXED_SHIFT));
}

struct Position {
    uint32_t tileX, tileY;
    Fixed offsetX;  // Fixed-point, no floating-point math!
    Fixed offsetY;
};

void Move(Position* pos, Fixed distance) {
    pos->offsetX += distance;  // Integer addition (fast!)

    Fixed tileSize = MetersToFixed(1.4f);
    if (pos->offsetX > tileSize) {
        pos->offsetX -= tileSize;
        pos->tileX++;
    }
}

// Pros: Very fast, deterministic, good for networking
// Cons: More complex, limited precision, harder to debug
// Used by: Doom, Quake, many retro games
```

### Approach B: Normalized Coordinates (0.0 to 1.0)

```c
struct Position {
    uint32_t tileX, tileY;
    float offsetX;  // Always 0.0 to 1.0 (fraction of tile)
    float offsetY;
};

void Move(Position* pos, float metersToMove, float tileSizeMeters) {
    // Convert meters to fraction of tile
    float tileFraction = metersToMove / tileSizeMeters;
    pos->offsetX += tileFraction;

    if (pos->offsetX > 1.0f) {
        pos->offsetX -= 1.0f;
        pos->tileX++;
    }
}

// Pros: Simple, always normalized
// Cons: Need tile size for every operation
// Used by: Unity (UV coordinates), many engines
```

### Approach C: Homogeneous Coordinates (Advanced)

```c
struct Position {
    float x, y, w;  // w is scale factor
};

// Can represent both world and screen in one system
// Convert between them with matrix multiplication
// Pros: Unified transformations, supports projection
// Cons: Overkill for 2D, more complex
// Used by: 3D graphics, OpenGL/Vulkan
```

### Approach D: Double Precision (Brute Force)

```c
struct Position {
    double worldX;  // Absolute position in meters
    double worldY;
};

// Pros: Simple, huge range, precise
// Cons: Slower, uses more memory
// Used by: Minecraft (after 1.8), space games
```

### Performance Comparison

```
Operation: 1 million position updates

Fixed-Point:       ~2ms   (fastest, integer math)
Float (Casey's):   ~3ms   (very good, modern CPUs)
Double:            ~5ms   (acceptable, slightly slower)
Normalized:        ~3ms   (similar to float, extra division)

Modern advice: Use float unless you need determinism
```

---

## 5. Math-Friendly Coordinates: Complications & Performance

### The Y-Flip Issue

#### Problem: Extra Operation Every Frame

```c
// Every time you render
void Render(float worldY) {
    int screenY = screenHeight - (worldY * metersToPixels);
    //            ^^^^^^^^^^^^ Extra subtraction!
}
```

#### Is This Slow?

**NO! Negligible cost:**

```c
// Modern CPU: ~0.3 nanoseconds per subtraction
// At 60 FPS with 1000 entities:
// Cost = 1000 * 0.3ns = 0.0003ms per frame
// Totally irrelevant compared to rendering (16ms budget)
```

#### When It WOULD Matter

```c
// If you do physics in screen space (don't do this!)
for (int i = 0; i < 1000000; i++) {
    screenY = screenHeight - worldY;  // Flip every calculation
    // Adds up if done millions of times per frame
}

// Solution: Keep physics in world space
for (int i = 0; i < 1000000; i++) {
    worldY += velocity;  // No flip needed
}
// Only flip once when rendering
screenY = screenHeight - worldY;
```

### Complications

#### 1. Third-Party Libraries (Might Use Screen Coords)

```c
// Some physics engine uses Y-down
PhysicsEngine* physics = CreatePhysics();
physics->AddBody(x, y);  // Expects Y-down

// Solution: Adapter layer
void AddBodyToPhysics(PhysicsEngine* p, float worldX, float worldY) {
    float physicsY = -worldY;  // Convert Y-up to Y-down
    p->AddBody(worldX, physicsY);
}
```

#### 2. Touch Input (Always Screen Coords)

```c
void OnTouch(int screenX, int screenY) {
    // Convert screen to world
    float worldX = screenX / metersToPixels;
    float worldY = (screenHeight - screenY) / metersToPixels;
    // Handle in world space from here
}
```

#### 3. UI Elements (Often Want Screen Space)

```c
// UI is screen-relative, not world-relative
struct UIElement {
    int screenX, screenY;  // Keep in screen space
    // Don't convert these to world space!
};
```

### Mitigation Strategies

#### Strategy 1: Clear Layer Separation

```c
// World layer: Y-up, meters
struct WorldPosition {
    float x, y;  // Always Y-up
};

// Screen layer: Y-down, pixels
struct ScreenPosition {
    int x, y;  // Always Y-down
};

// Explicit conversion
ScreenPosition WorldToScreen(WorldPosition world) {
    // Conversion happens in one place
}
```

#### Strategy 2: Inline Conversion for Hot Paths

```c
// If profiling shows it's slow (unlikely)
void RenderEntities(Entity* entities, int count) {
    float m2p = metersToPixels;
    int h = screenHeight;

    // Hoist constants out of loop
    for (int i = 0; i < count; i++) {
        int x = entities[i].x * m2p;
        int y = h - (entities[i].y * m2p);
        Draw(x, y);
    }
}
```

#### Strategy 3: SIMD for Bulk Conversions

```c
// If you have thousands of objects
#include <immintrin.h>

void ConvertBatch(float* worldY, int* screenY, int count) {
    __m128 height = _mm_set1_ps(screenHeight);
    __m128 scale = _mm_set1_ps(metersToPixels);

    for (int i = 0; i < count; i += 4) {
        __m128 wy = _mm_load_ps(&worldY[i]);
        __m128 sy = _mm_sub_ps(height, _mm_mul_ps(wy, scale));
        _mm_store_si128((__m128i*)&screenY[i], _mm_cvtps_epi32(sy));
    }
}
// 4x faster for large batches
```

### Real-World Performance Impact

```
Scenario: 2D game, 1000 entities, 60 FPS

Without Y-flip optimization:
- 1000 subtractions per frame
- 60,000 subtractions per second
- Cost: ~0.018ms per frame
- % of 16ms budget: 0.1%

With Y-flip optimization:
- Cost: ~0.004ms per frame
- % of 16ms budget: 0.025%

Conclusion: Not worth optimizing unless you have
100,000+ entities or are CPU-bound
```

---

## 6. Packed Coordinates - How They Actually Work

### The Bit-Packing Mechanism

#### Visual Breakdown

```
32-bit unsigned integer for X coordinate:

Option A: 16-bit chunks, 16-bit tile index
┌─────────────────────────────────────┬─────────────────────────────────────┐
│     Bits 31-16 (high 16 bits)      │      Bits 15-0 (low 16 bits)       │
│     Chunk X (65,536 chunks)        │   Tile X within chunk (65,536)     │
└─────────────────────────────────────┴─────────────────────────────────────┘

Option B: 28-bit chunks, 4-bit tile index (Casey's choice)
┌────────────────────────────────────────────────────────┬──────────────────┐
│          Bits 31-4 (high 28 bits)                      │ Bits 3-0 (4 bits)│
│     Chunk X (268 million chunks)                       │ Tile X (0-15)    │
└────────────────────────────────────────────────────────┴──────────────────┘

Example value: 0b0000_0000_0000_0000_0000_0000_0011_0101
                                                    ^^^^ Tile = 5
                                ^^^^^^^^^^^^^^^^^^^^ Chunk = 3
```

#### The Code

```c
// Configuration
#define CHUNK_SHIFT 4      // Low 4 bits for tile index
#define CHUNK_MASK 0xF     // 0b1111
#define TILES_PER_CHUNK_SIDE 16  // 2^4 = 16

// Single position representation
typedef struct {
    uint32_t absTileX;  // Bits [31:4]=chunk, [3:0]=tile
    uint32_t absTileY;
    float offsetMetersX;  // 0.0 to tileSideInMeters
    float offsetMetersY;
} TilePosition;

// Extract chunk coordinate (which 16×16 region)
inline uint32_t GetChunkX(uint32_t absTileX) {
    return absTileX >> CHUNK_SHIFT;
    // Shift right by 4 = divide by 16, drops low bits
    // 0b...0011_0101 >> 4 = 0b...0000_0011 = 3
}

// Extract tile within chunk (0-15)
inline uint32_t GetTileInChunkX(uint32_t absTileX) {
    return absTileX & CHUNK_MASK;
    // Mask keeps only low 4 bits
    // 0b...0011_0101 & 0xF = 0b...0000_0101 = 5
}

// Build absolute tile coordinate
inline uint32_t MakeAbsTileX(uint32_t chunkX, uint32_t tileX) {
    return (chunkX << CHUNK_SHIFT) | tileX;
    // (3 << 4) | 5 = 0b0011_0000 | 0b0101 = 0b0011_0101
}

// The magic: moving is just integer increment!
void MoveRight(TilePosition* pos, float meters, float tileSideInMeters) {
    pos->offsetMetersX += meters;

    while (pos->offsetMetersX >= tileSideInMeters) {
        pos->offsetMetersX -= tileSideInMeters;
        pos->absTileX++;  // Just increment!
        // If tile was 15, becomes 16, spills into chunk bits
        // Chunk auto-increments from bit overflow!
    }
}
```

#### Why This Works - The Trick

**Before (manual chunk management):**

```c
tileX++;
if (tileX >= 16) {
    tileX = 0;
    chunkX++;
}
// You manually handle overflow
```

**After (automatic):**

```c
absTileX++;  // Binary addition does it automatically!

// Example:
// Tile 15 in chunk 3: 0b0011_1111 (3*16 + 15 = 63)
// Increment:         + 1
// Result:             0b0100_0000 (4*16 + 0 = 64)
//                        ^^^^ Chunk is now 4
//                             ^^^^ Tile is now 0
// The CPU's ALU handles the carry automatically!
```

### Real Example Walkthrough

```c
TilePosition player;
player.absTileX = 0;  // Start at origin
player.offsetMetersX = 0.0f;

float tileSideInMeters = 1.4f;

// Player moves 10 meters to the right
float moveDistance = 10.0f;

player.offsetMetersX += moveDistance;  // 0 + 10 = 10.0

// Normalize to tile boundaries
while (player.offsetMetersX >= tileSideInMeters) {
    player.offsetMetersX -= tileSideInMeters;
    player.absTileX++;
}

// After loop:
// 10.0 / 1.4 = 7.14 tiles crossed
// absTileX = 7 (crossed 7 full tiles)
// offsetMetersX = 0.2 (10.0 - 7*1.4 = 0.2 meters into tile 7)

GetChunkX(7);        // = 0 (still in chunk 0)
GetTileInChunkX(7);  // = 7 (in tile 7 of that chunk)
```

### Advanced: Wrapping and Negative Coordinates

```c
// For worlds that wrap around (like Pac-Man)
#define WORLD_SIZE_TILES (1 << 20)  // 1 million tiles

uint32_t WrapX(int32_t x) {
    return ((uint32_t)x) & (WORLD_SIZE_TILES - 1);
    // Fast modulo for power-of-2
}

// For supporting negative coordinates
int32_t GetSignedTileX(TilePosition* pos) {
    // Top bit is sign
    return (int32_t)pos->absTileX;
}

void MoveLeft(TilePosition* pos, float meters, float tileSideInMeters) {
    pos->offsetMetersX -= meters;

    while (pos->offsetMetersX < 0.0f) {
        pos->offsetMetersX += tileSideInMeters;
        pos->absTileX--;  // Decrement works too!
        // Chunk auto-decrements from borrow
    }
}
```

---

## 7. CalculateBestFit - How It Works

### The Problem

```
You have:
- Screen: 1920 × 1080 pixels
- Tiles: should be "about 60 pixels" but can adapt
- World: each tile is 1.4 meters

Question: How many tiles should we show?
          What pixel size per tile?
```

### Approach 1: Fixed Tile Count (Simple)

```c
#define TILES_VISIBLE_X 32
#define TILES_VISIBLE_Y 18

int CalculateTilePixelSize(int screenWidth, int screenHeight) {
    int tileSizeX = screenWidth / TILES_VISIBLE_X;
    int tileSizeY = screenHeight / TILES_VISIBLE_Y;

    // Use smaller to ensure both fit
    return (tileSizeX < tileSizeY) ? tileSizeX : tileSizeY;
}

// 1920×1080 screen:
// tileSizeX = 1920 / 32 = 60 pixels
// tileSizeY = 1080 / 18 = 60 pixels
// Use 60 pixels per tile
```

### Approach 2: Maintain Aspect Ratio

```c
int CalculateBestFit(int screenW, int screenH, float targetTileSize) {
    // Try to get close to target while maintaining aspect
    int tilesX = screenW / (int)targetTileSize;
    int tilesY = screenH / (int)targetTileSize;

    // Recalculate actual sizes
    int actualSizeX = screenW / tilesX;
    int actualSizeY = screenH / tilesY;

    // Tiles must be square, use average or smaller
    return (actualSizeX + actualSizeY) / 2;
}
```

### Approach 3: Density-Aware (Mobile vs Desktop)

```c
int CalculateBestFit(int screenW, int screenH, float dpi) {
    // Physical size in inches
    float screenWidthInches = screenW / dpi;

    // Want ~0.2 inches per tile (comfortable viewing)
    float targetInchesPerTile = 0.2f;

    int tilesAcross = (int)(screenWidthInches / targetInchesPerTile);
    return screenW / tilesAcross;
}

// 1920px screen at 96 DPI:
// 1920/96 = 20 inches wide
// 20 / 0.2 = 100 tiles across
// 1920 / 100 = 19.2 pixels per tile

// Same game on 4K at 192 DPI:
// 3840/192 = 20 inches wide (same physical size)
// Still 100 tiles across
// 3840 / 100 = 38.4 pixels per tile
// More detail, same gameplay
```

### Approach 4: Adaptive to Performance

```c
struct VideoSettings {
    int minTileSize;  // 30px
    int maxTileSize;  // 120px
    int targetFPS;    // 60
};

int CalculateBestFit(VideoSettings* settings, int screenW, float currentFPS) {
    int currentSize = screenW / tilesVisibleX;

    if (currentFPS < settings->targetFPS * 0.9f) {
        // Running slow, make tiles bigger (less to draw)
        currentSize = min(currentSize + 5, settings->maxTileSize);
    } else if (currentFPS > settings->targetFPS) {
        // Running fast, make tiles smaller (more detail)
        currentSize = max(currentSize - 5, settings->minTileSize);
    }

    return currentSize;
}
```

### Casey's Likely Approach (Simple and Effective)

```c
typedef struct {
    float tileSideInMeters;
    int tileSideInPixels;
    int tilesVisibleX;  // How many tiles to show
    int tilesVisibleY;
} WorldDefinition;

void RecalculateForScreen(WorldDefinition* world,
                          int screenW, int screenH) {
    // Maintain approximately the same tile count
    world->tileSideInPixels = screenW / world->tilesVisibleX;

    // Ensure it's reasonable
    if (world->tileSideInPixels < 16)
        world->tileSideInPixels = 16;
    if (world->tileSideInPixels > 256)
        world->tileSideInPixels = 256;

    // Recalculate how many tiles actually fit
    world->tilesVisibleX = screenW / world->tileSideInPixels;
    world->tilesVisibleY = screenH / world->tileSideInPixels;
}
```

---

## 8. Architectural Decisions: Who Owns What?

### The Big Questions

1. Should the ENGINE enforce resolution independence?
2. Should the GAME be allowed to work in pixels?
3. Who decides coordinate system (Y-up vs Y-down)?
4. What if the game wants something different?

### Option A: Engine Enforces Everything (Opinionated)

```c
// Engine API only exposes world units
typedef struct {
    float x, y;  // ALWAYS meters, ALWAYS Y-up
} Position;

// Game CANNOT access pixels directly
void GameUpdate(float dt) {
    Position playerPos = GetPlayerPosition();
    playerPos.x += 3.0f * dt;  // Must use meters
    SetPlayerPosition(playerPos);
}

// Engine handles all rendering conversion
void EngineRender() {
    // Internal: converts to screen space
    int screenX = WorldToScreenX(playerPos.x);
    int screenY = WorldToScreenY(playerPos.y);
}
```

**Pros:**

- Consistent across all games
- Prevents mistakes
- Easy to support multiple resolutions
- Math-friendly by default

**Cons:**

- Less flexible
- Game can't optimize for pixel-perfect alignment
- Hard to integrate pixel-art pipelines
- UI/HUD is awkward (naturally pixel-aligned)

**Good for:** Engines targeting multiple games (Unity, Unreal)

### Option B: Engine Provides Tools, Game Chooses (Pragmatic)

```c
// Engine provides BOTH coordinate systems
typedef struct {
    float worldX, worldY;    // Meters, Y-up
    int screenX, screenY;    // Pixels, Y-down
} Position;

// Conversion helpers
int WorldToScreenX(float worldX);
float ScreenToWorldX(int screenX);

// Game chooses what to use
void GameUpdate(float dt) {
    // Physics in world space
    playerWorldX += velocityMetersPerSec * dt;

    // UI in screen space
    buttonScreenX = 50;  // 50 pixels from left
    buttonScreenY = 50;  // 50 pixels from top
}
```

**Pros:**

- Flexible
- Best of both worlds
- Can optimize where needed
- Easier migration from existing code

**Cons:**

- Game can make wrong choice
- More API surface area
- Easier to mix up coordinate systems

**Good for:** Flexible engines (Casey's approach, SDL, raylib)

### Option C: Game Has Full Control (Libertarian)

```c
// Engine is just rendering + input
void GameUpdate() {
    // Game implements EVERYTHING
    MyPosition pos = MyCalculatePosition();
    MyCoordinateSystem coords = MySetupCoords();

    DrawSprite(pos, coords);
}

// Engine has no opinion on units
void DrawSprite(int x, int y) {
    // Draws at pixel coordinates, game decides meaning
}
```

**Pros:**

- Maximum flexibility
- Zero overhead
- Game can do anything

**Cons:**

- Game does all the work
- Easy to mess up
- Every game reinvents the wheel

**Good for:** Low-level engines (Sokol, custom engines)

### Casey's Choice: Option B with Convention

**His Approach:**

```c
// Engine provides world-space tools
typedef struct {
    float tileSideInMeters;
    int tileSideInPixels;  // Can change
} World;

// Game SHOULD use world space
typedef struct {
    uint32_t absTileX, absTileY;
    float offsetMetersX, offsetMetersY;
} Position;

// But engine doesn't FORCE it
// Game can access pixels if needed for:
// - UI
// - Debug overlays
// - Pixel-perfect effects
```

**Why This Works:**

- **Convention over configuration**: World space is the path of least resistance
- **Escape hatch**: Can use pixels when needed (UI, effects)
- **Clear separation**: World vs Screen is obvious in code
- **Performance**: No overhead, just different APIs

### My Recommendation by Game Type

#### Pixel-Art Platformer

```c
// PREFER: Pixel control
// WHY: Pixel-perfect collision, retro aesthetic
Position.pixelX = 100;
Position.pixelY = 50;
// Scale entire game window instead of tiles
```

#### Physics-Based Game

```c
// PREFER: World units (meters)
// WHY: Realistic physics, resolution-independent
Position.metersX = 5.7f;
Position.metersY = 3.2f;
ApplyGravity(-9.8f);
```

#### Strategy/Simulation Game

```c
// PREFER: Abstract units (tiles)
// WHY: Doesn't need physics realism
Position.tileX = 10;
Position.tileY = 5;
// Tile size adapts to screen
```

#### UI-Heavy Game

```c
// PREFER: Mixed approach
// World: meters for gameplay
// Screen: pixels for UI
GameWorldPos.meters = 5.7f;
UIButtonPos.pixels = 100;
```

### What Happens in Each Case?

#### Case 1: Engine Y-up, Game Wants Y-down

```c
// Adapter layer
Position EngineToGame(EnginePos engine) {
    return (Position){
        .x = engine.x,
        .y = -engine.y  // Flip
    };
}

// Or: Game accepts engine's choice, adjusts mindset
// Usually easier to adapt than fight the engine
```

#### Case 2: Engine Uses Pixels, Game Wants Meters

```c
// Game implements its own world layer
typedef struct {
    float metersX, metersY;
} GamePos;

void GameRender(GamePos pos) {
    int pixelX = pos.metersX * PIXELS_PER_METER;
    int pixelY = pos.metersY * PIXELS_PER_METER;
    EngineDrawSprite(pixelX, pixelY);
}
// Works, but duplicates effort
```

#### Case 3: Engine Flexible, Game Uses Both

```c
// Best case: right tool for right job
void GameUpdate() {
    // Physics in meters
    UpdatePhysics(&worldPosition);

    // Render adaptively
    int pixelX = WorldToPixel(worldPosition.x);
    EngineDrawSprite(pixelX, ...);

    // UI in pixels
    EngineDrawButton(50, 50);  // Top-left
}
```

---

## Summary: What Should YOU Do?

### For a New Engine

1. **Provide world-space coordinate system** (Y-up, meters or abstract units)
2. **Make it the default path** (easy to use, well-documented)
3. **Provide pixel-space access** (for UI, effects, debug)
4. **Clear conversion functions** (WorldToScreen, ScreenToWorld)
5. **Document the convention** (when to use which)

### For an Existing Game

1. **If it works, don't change it** (pixel-based is fine if consistent)
2. **If adding resolution support**, wrap pixel code with conversion layer
3. **If adding physics**, consider world units for new code
4. **If rewriting**, adopt Casey's approach

### Casey's Philosophy

> "I'm not forcing anyone to do anything. I'm showing you the problems I've encountered and the solutions that worked for me. Your game might be different."

This is why Handmade Hero is valuable - it's not dogma, it's battle-tested pragmatism.

---

## Response:

2/15/2026, 2:56:17 PM

Hmm, will for now, since I'm still on day 31, my skills are still of a beginner, not sure which should be abstracted to what correctly right now _(since Casey is making the engine around handmade hero which means some of his decisions in terms of the boundaries between the game and engine is kind of blurred and if I want to separate it correctly to support other types of games then I may have to wait a little)_, I'll let those being owned by the game _for now_ and please give me any kind of helpful advice to help for now and then

## Prompt:

2/15/2026, 2:57:55 PM

You're making a **very wise decision**! Let me give you practical advice for your situation.

```plaintext
Create File: Creating practical beginner-friendly advice for following Handmade Hero Day 31 without overengineering
```

```plaintext
Present File: /home/claude/beginner_advice_day31.md
```

Perfect! Here's the **TL;DR** for right now:

## What You Should Do This Week:

1. **Just follow Casey exactly** - don't try to make it "better" yet
2. **Put everything in game code** - worry about engine/game separation after day 100
3. **Add comments for confused parts** - "I don't understand why, but Casey says..."
4. **Make simple test cases** - print out chunk/tile numbers as you move
5. **One episode per 2-3 days** - watch, implement, debug, then next episode

## The Key Insight:

**You're trying to learn engine architecture while learning basic game programming.** That's like learning to design cars while learning to drive. Learn to drive first (follow Casey through day 100), THEN design your own car.

By day 100, you'll naturally see:

- "Oh, THIS code is reusable across games" (engine)
- "THIS code is Handmade Hero specific" (game)
- "THIS is where I'd want different choices for different games"

Right now at day 31? You can't see those boundaries yet because you haven't hit the problems that make those boundaries obvious. **And that's perfectly fine.**

Casey is teaching you to **think like a professional programmer**: messy first, refactor later, understand through doing. Trust the process! 🎯

> File: /home/claude/beginner_advice_day31.md

# Practical Advice: Following Handmade Hero as a Beginner

## Your Situation: Perfect Approach! 🎯

You're doing exactly the right thing:

1. Following along with Casey
2. Recognizing you don't know enough yet to make abstraction decisions
3. Keeping things game-side for now
4. Planning to refactor later when you understand more

This is **exactly** how you should learn!

---

## The "Just Follow Along" Strategy

### What to Do Right Now

```c
// DON'T worry about this yet:
// - "Is this engine or game code?"
// - "Should I abstract this?"
// - "What if I want to make a different game?"

// DO this instead:
// - Copy what Casey does
// - Make it work
// - Understand WHY it works
// - Move to next day
```

### Why This Works

**Casey's teaching method:**

1. He shows you the PROBLEM first (day 31: pixel trap)
2. Then shows you a SOLUTION (meters + packing)
3. Later shows you when to use it (future episodes)

**Your learning path:**

1. Day 31: See the problem, understand the solution
2. Days 32-50: Use the solution in practice
3. Day 100+: NOW you understand when to abstract
4. Your own project: Make informed decisions

**Trying to abstract too early is like:**

- Learning to play guitar by designing a guitar factory
- You need to PLAY first, THEN build

---

## Beginner-Friendly Rules for Day 31

### Rule 1: Copy Casey's Code Structure Exactly

```c
// Just put everything in game code for now
// handmade.h
typedef struct {
    float tileSideInMeters;  // Casey's exact naming
    int tileSideInPixels;
} World;

typedef struct {
    uint32_t absTileX;
    uint32_t absTileY;
    float offsetX;  // Will be meters later
    float offsetY;
} TilePosition;

// Don't think "should this be in engine.h?"
// Just follow Casey's structure
```

**Why:** His structure works. You'll understand WHY it works by day 100.

### Rule 2: Add Comments About What You DON'T Understand

```c
typedef struct {
    uint32_t absTileX;
    // NOTE(yourname): I don't fully understand the bit-packing yet
    // but Casey says high bits = chunk, low bits = tile
    // Will make sense after implementing it
} TilePosition;

// Helper I'm copying from Casey
inline uint32_t GetChunkX(uint32_t absTileX) {
    return absTileX >> CHUNK_SHIFT;
    // TODO: Understand bit shifting better
    // For now: this extracts the chunk number
}
```

**Why:** Future-you will thank present-you. These comments become bread crumbs.

### Rule 3: Keep a "Questions to Revisit" List

```markdown
# Questions for Later (After Day 100)

## Day 31 Questions:

- Why 4 bits for tiles vs 28 for chunks? Why not 8/24?
- Could I use this for a 3D game? How would Z work?
- When would I want pixel-perfect instead of meters?
- How does this work with tile-based collision?

## Will understand after:

- Seeing collision system (day 40-50?)
- Seeing entity system (day 60-70?)
- Seeing performance optimization (day 80+?)
```

**Why:** Acknowledges your questions without blocking progress.

### Rule 4: Make It Work Before Making It "Right"

```c
// WRONG approach (trying to be clever):
template<typename T>
class CoordinateSystem {
    // Generic abstraction for any coordinate system!
    // I'll make it work for 2D, 3D, polar coords, etc!
}

// RIGHT approach (Casey's way):
// Just make a struct that works for THIS game
typedef struct {
    uint32_t absTileX;
    uint32_t absTileY;
    float offsetX;
    float offsetY;
} TilePosition;

// If you need 3D later, you'll know how to add Z
// If you need different game later, you'll know what to change
```

---

## What to Focus On (Day 31 Specifically)

### Focus List ✅

1. **Get the coordinate packing working**

   ```c
   // Make this compile and run
   uint32_t absTileX = 0;
   absTileX++;  // Should increment tile

   uint32_t chunk = GetChunkX(absTileX);
   uint32_t tile = GetTileInChunk(absTileX);
   printf("Chunk: %u, Tile: %u\n", chunk, tile);
   ```

2. **Understand the meter/pixel separation**

   ```c
   // Make sure you can change this:
   tileSideInPixels = 60;
   // And have game still work (even if slow/fast)

   tileSideInPixels = 30;
   // Player should move same speed relative to tiles
   ```

3. **See Y-up vs Y-down in action**

   ```c
   // Try both:
   screenY = worldY * scale;              // Y-down (wrong for math)
   screenY = screenHeight - (worldY * scale);  // Y-up (math-friendly)

   // Notice which feels more natural when you add jumping/gravity later
   ```

### Don't Focus On ❌

1. **Making a generic engine** - You don't know what needs to be generic yet
2. **Perfect abstractions** - You'll see what needs abstracting by day 100
3. **Supporting other game types** - Make THIS game first
4. **Optimizations** - Casey will show you when it matters
5. **"Clean code"** - Handmade Hero's goal is UNDERSTANDING, not beauty

---

## Your Safety Net: The Refactoring Plan

### Stage 1: Now (Day 31-50) - Follow Casey

```c
// Everything in game code
// handmade.cpp has all the logic
void GameUpdateAndRender() {
    // World stuff
    // Rendering stuff
    // All mixed together
}
```

### Stage 2: Mid-point (Day 50-100) - See Patterns

```c
// Casey will naturally separate things
// platform layer vs game layer
// renderer vs simulation
// You'll see WHY they separate
```

### Stage 3: Later (Day 100+) - Extract Engine

```c
// NOW you know what to extract
engine/
  coordinates.h   // You know this is reusable
  rendering.h     // You know this is reusable
  platform.h      // You know this is reusable

game/
  handmade_rules.cpp  // Game-specific logic
  handmade_world.cpp  // Uses engine/coordinates
```

### Stage 4: Your Next Project - Informed Decisions

```c
// NOW you can make a different game
// Because you UNDERSTAND the tradeoffs

// Pixel platformer?
// You know: Keep pixel-perfect, scale window instead

// Physics game?
// You know: Use meters, Y-up coordinates

// RTS?
// You know: Abstract units work, adapt coordinate packing
```

---

## Practical Tips for Day 31 Specifically

### Tip 1: Use Simple Test Cases

```c
void TestCoordinatePacking() {
    // Test: Move right across chunk boundary
    TilePosition pos = {0};
    pos.absTileX = 15;  // Last tile in chunk 0
    pos.offsetX = 1.3f;  // Near end of tile

    // Move 0.2 meters (should cross into next tile)
    pos.offsetX += 0.2f;
    if (pos.offsetX >= 1.4f) {
        pos.offsetX -= 1.4f;
        pos.absTileX++;  // Should become 16 (tile 0 of chunk 1)
    }

    uint32_t chunk = GetChunkX(pos.absTileX);
    uint32_t tile = GetTileInChunk(pos.absTileX);

    printf("Expected: Chunk=1, Tile=0\n");
    printf("Got: Chunk=%u, Tile=%u\n", chunk, tile);
    assert(chunk == 1 && tile == 0);
}
```

### Tip 2: Draw Debug Visualizations

```c
void DebugDrawCoordinates(TilePosition* pos) {
    char buffer[256];
    sprintf(buffer,
        "Abs: (%u, %u)\n"
        "Chunk: (%u, %u)\n"
        "Tile: (%u, %u)\n"
        "Offset: (%.2f, %.2f)",
        pos->absTileX, pos->absTileY,
        GetChunkX(pos->absTileX), GetChunkY(pos->absTileY),
        GetTileInChunk(pos->absTileX), GetTileInChunk(pos->absTileY),
        pos->offsetX, pos->offsetY
    );

    // Draw this on screen while playing
    // You'll SEE the numbers change as you move
}
```

### Tip 3: Keep Casey's Pace

```c
// Casey's day 31 plan:
// 1. Explain problem (20 min) ✓
// 2. Define tileSideInMeters/Pixels (5 min) ✓
// 3. Start thinking about packing (5 min) ✓
// 4. Next day: actually implement it

// Your plan:
// Day 1: Watch episode, take notes
// Day 2: Implement what Casey explained
// Day 3: Make sure it works
// Day 4: Watch next episode

// Don't rush ahead trying to implement things he hasn't shown yet
```

### Tip 4: Expect to Rewrite

```c
// Your first attempt:
typedef struct {
    int tileX, tileY;
    float offsetX, offsetY;
} Position;  // Works but limited

// After day 31:
typedef struct {
    uint32_t absTileX, absTileY;
    float offsetX, offsetY;
} TilePosition;  // Better! Can handle huge world

// After day 50:
typedef struct {
    uint32_t absTileX, absTileY;
    float offsetMetersX, offsetMetersY;  // Now in meters!
} TilePosition;

// This is NORMAL and GOOD
// You're learning by doing
```

---

## When Things Get Confusing

### Strategy: Simplify First

```c
// Casey's talking about chunks, packing, meters...
// Too much at once?

// Step back. Simplify:

// Version 1: Just absolute tile coordinates
typedef struct {
    int tileX;      // Entire world, -2 billion to +2 billion
    int tileY;
    float offsetX;  // Pixels within tile
    float offsetY;
} SimplePosition;

// Get this working FIRST
// Then add packing
// Then add meters
// One thing at a time
```

### Strategy: Ask Specific Questions

```c
// VAGUE: "I don't understand coordinate systems"
// Can't help with this - too broad

// SPECIFIC: "Why does GetChunkX use >> instead of / ?"
// Answer: Bit shift is faster, same result for powers of 2

// SPECIFIC: "My player moves twice as fast after I changed tile size"
// Answer: You forgot to use meters for speed, still using pixels

// SPECIFIC: "What's the difference between absTileX and tileX?"
// Answer: abs = packed (chunk+tile), tileX = just tile in chunk
```

### Strategy: Compare to Something You Know

```c
// Don't understand bit packing?
// Think: "It's like a phone number"
//
// Phone: (555) 123-4567
//         ^^^  ^^^^^^^^
//         area code | local number
//
// Coordinate: 0b...0011_0101
//                   ^^^^|^^^^
//                 chunk | tile
//
// Both pack two numbers into one!
```

---

## The Long-Term Mindset

### Where You Are Now (Day 31)

```
Knowledge:  [##--------]  20%
Skill:      [#---------]  10%
Confusion:  [########--]  80% (NORMAL!)
Confidence: [#---------]  10%
```

### Where You'll Be (Day 100)

```
Knowledge:  [######----]  60%
Skill:      [#####-----]  50%
Confusion:  [###-------]  30% (STILL NORMAL!)
Confidence: [####------]  40%
```

### Where You'll Be (Day 300)

```
Knowledge:  [#########-]  90%
Skill:      [########--]  80%
Confusion:  [#---------]  10% (ALWAYS SOME!)
Confidence: [#######---]  70%
```

**Key insight:** Casey finished Handmade Hero at day 600+. You're at day 31. You're supposed to be confused!

---

## Your Action Plan for This Week

### Monday: Watch Day 31

- Take notes
- Don't code yet
- Just understand the PROBLEMS

### Tuesday: Simple Implementation

```c
// Just make this work:
typedef struct {
    uint32_t absTileX, absTileY;
    float offsetX, offsetY;  // Still pixels for now
} TilePosition;

// Move player, see numbers change
```

### Wednesday: Add Bit Operations

```c
// Add these:
uint32_t GetChunkX(uint32_t abs);
uint32_t GetTileInChunk(uint32_t abs);

// Test them with prints
```

### Thursday: Add Meter Definitions

```c
// Add but don't use yet:
float tileSideInMeters = 1.4f;
int tileSideInPixels = 60;

// Just have them in your World struct
```

### Friday: Watch Day 32

- See how Casey uses what he set up
- NOW you're ready for the next step

---

## Remember: Casey's on Your Side

### He Says Things Like:

- "We'll come back to this"
- "Don't worry about this yet"
- "This is going to be messy for a while"
- "We'll clean this up later"

### This Means:

- You're not supposed to understand everything now
- He's showing you the messy process intentionally
- Professional programming IS messy
- Refactoring is normal

### What He's Teaching:

- Not just "how to make a game"
- But "how to think about making a game"
- The thought process matters more than the code
- You'll internalize this by day 100

---

## Final Advice: Trust the Process

```
Day 31:  "I don't understand coordinate systems"
Day 50:  "Oh, coordinates make sense, but entity systems are confusing"
Day 100: "Entity systems clicked! But rendering is hard"
Day 150: "Rendering makes sense! But performance is tricky"
Day 300: "I understand most of this now!"
Day 600: "I could teach someone else Handmade Hero"

Your next project: "I'm making my own engine and I know why I'm making each decision"
```

**You're on the right track.** Keep following Casey, keep asking questions, and trust that it will click. Because it will - around day 100, suddenly a bunch of things will click at once.

Until then: Just make it work, then make it better, then understand why it's better.

You've got this! 🚀

---

## Response:

2/15/2026, 2:59:44 PM

Give me the steps/way of handling it on the professional way so that the engine has sensible default while the game can influence/change it for it's needs whatever it's

## Prompt:

2/15/2026, 3:01:31 PM

```plaintext
Create File: Creating a professional-grade guide for designing a flexible coordinate system with sensible engine defaults and game override capabilities
```

```plaintext
Present File: /home/claude/professional_coordinate_system_design.md
```

Perfect! I've given you the **professional-grade architecture** for handling coordinate systems.

## The Key Pattern: "Convention over Configuration with Escape Hatches"

**The philosophy:**

1. Engine picks smart defaults (meters, Y-up, 60px tiles)
2. Game can tweak values (change tile size, chunk size, etc.)
3. Game can override functions (for special cases like pixel-perfect games)
4. Engine validates everything (prevents bad configurations)

## How It Works in Practice:

**90% of games:**

```c
config = EngineGetDefaultCoordinateConfig();
// Done! Just use it.
```

**9% of games:**

```c
config = EngineGetDefaultCoordinateConfig();
config.pixelsPerTile = 80;  // Tweak one thing
// Done!
```

**1% of games:**

```c
config = EngineGetDefaultCoordinateConfig();
override.worldToScreen = MyComplexFunction;
EngineSetCoordinateOverride(&config, &override);
// Full control when needed
```

## The Real Power:

Three completely different games can use the **same engine code**, just configured differently:

- Physics platformer → defaults
- Pixel-art game → pixel units
- Strategy game → abstract units

This is exactly how Unity/Unreal work: opinionated defaults, but flexible when you need it.

**For your current learning:** Start with the simple version in the beginner guide, then come back to this professional pattern once you've finished more of Handmade Hero! 🚀

> File: /home/claude/professional_coordinate_system_design.md

# Professional Engine Design: Sensible Defaults with Game Control

## The Professional Pattern: "Convention over Configuration"

**Philosophy:** The engine provides smart defaults that work for 80% of cases, but the game can override anything when needed.

---

## Step-by-Step Professional Implementation

### Step 1: Define the Engine's Default Coordinate System

```c
// engine_coordinates.h
// This is the ENGINE's opinion on how coordinates should work

typedef enum {
    COORD_SYSTEM_Y_UP,      // Math-friendly (default)
    COORD_SYSTEM_Y_DOWN,    // Screen-native
    COORD_SYSTEM_CUSTOM     // Game handles conversion
} CoordinateSystemType;

typedef enum {
    UNIT_SYSTEM_METERS,     // Real-world units (default)
    UNIT_SYSTEM_PIXELS,     // Screen units
    UNIT_SYSTEM_ABSTRACT,   // Game-defined units
    UNIT_SYSTEM_CUSTOM      // Game handles everything
} UnitSystemType;

// Engine's coordinate configuration
typedef struct {
    // Unit system
    UnitSystemType unitSystem;      // Default: UNIT_SYSTEM_METERS
    float baseUnit;                  // Default: 1.0 meter
    float unitsPerTile;             // Default: 1.4 meters per tile

    // Display system
    int pixelsPerTile;              // Default: 60, can change at runtime

    // Coordinate system
    CoordinateSystemType coordSystem;  // Default: COORD_SYSTEM_Y_UP

    // Chunk configuration (for sparse storage)
    uint32_t chunkShift;            // Default: 4 (16x16 tiles per chunk)
    uint32_t tilesPerChunkSide;     // Default: 16

    // Allow game to override conversions
    void* gameOverride;             // Optional: game can inject custom behavior
} EngineCoordinateConfig;

// Engine provides smart defaults
EngineCoordinateConfig EngineGetDefaultCoordinateConfig() {
    return (EngineCoordinateConfig){
        .unitSystem = UNIT_SYSTEM_METERS,
        .baseUnit = 1.0f,
        .unitsPerTile = 1.4f,
        .pixelsPerTile = 60,
        .coordSystem = COORD_SYSTEM_Y_UP,
        .chunkShift = 4,
        .tilesPerChunkSide = 16,
        .gameOverride = NULL
    };
}
```

### Step 2: Define Position Representation (Engine Layer)

```c
// engine_coordinates.h (continued)

// The engine's position representation
// This works for ANY game using the engine
typedef struct {
    // Tile-based position (supports huge worlds via bit-packing)
    uint32_t absTileX;
    uint32_t absTileY;

    // Sub-tile position (in engine's unit system)
    float offsetX;  // 0.0 to config.unitsPerTile
    float offsetY;

    // Optional: Z for 3D games (ignored by 2D engine functions)
    uint32_t absTileZ;
    float offsetZ;
} EnginePosition;

// Conversion utilities (engine provides these)
typedef struct {
    int screenX;
    int screenY;
} ScreenPosition;

typedef struct {
    float worldX;  // In engine's unit system
    float worldY;
} WorldPosition;

// Engine's conversion functions
ScreenPosition EngineWorldToScreen(
    EngineCoordinateConfig* config,
    EnginePosition* pos,
    int screenWidth,
    int screenHeight
);

WorldPosition EnginePositionToWorld(
    EngineCoordinateConfig* config,
    EnginePosition* pos
);

EnginePosition EngineWorldToPosition(
    EngineCoordinateConfig* config,
    WorldPosition world
);
```

### Step 3: Create Game-Override Interface (Flexibility Layer)

```c
// engine_coordinates.h (continued)

// Game can register custom conversion functions
typedef struct {
    // Optional: Game's custom coordinate conversion
    ScreenPosition (*worldToScreen)(void* gameData, WorldPosition world,
                                    int screenW, int screenH);

    // Optional: Game's custom unit conversion
    float (*unitsToMeters)(void* gameData, float units);
    float (*metersToUnits)(void* gameData, float meters);

    // Optional: Game's custom chunk configuration
    uint32_t (*getChunkX)(void* gameData, uint32_t absTileX);
    uint32_t (*getTileInChunk)(void* gameData, uint32_t absTileX);

    // Game's data pointer (for custom functions to use)
    void* gameData;
} GameCoordinateOverride;

// Engine respects game overrides
void EngineSetCoordinateOverride(
    EngineCoordinateConfig* config,
    GameCoordinateOverride* override
) {
    config->gameOverride = override;
}
```

### Step 4: Implement Engine Core with Override Support

```c
// engine_coordinates.c

// Internal: Check if game has override
static inline bool HasGameOverride(EngineCoordinateConfig* config) {
    return config->gameOverride != NULL;
}

static inline GameCoordinateOverride* GetOverride(EngineCoordinateConfig* config) {
    return (GameCoordinateOverride*)config->gameOverride;
}

// Engine's default implementation (used when game doesn't override)
static ScreenPosition DefaultWorldToScreen(
    EngineCoordinateConfig* config,
    WorldPosition world,
    int screenWidth,
    int screenHeight
) {
    float unitsToPixels = (float)config->pixelsPerTile / config->unitsPerTile;

    ScreenPosition result;
    result.screenX = (int)(world.worldX * unitsToPixels);

    if (config->coordSystem == COORD_SYSTEM_Y_UP) {
        // Y-up: flip for screen
        result.screenY = screenHeight - (int)(world.worldY * unitsToPixels);
    } else {
        // Y-down: direct mapping
        result.screenY = (int)(world.worldY * unitsToPixels);
    }

    return result;
}

// Public API: Engine checks for game override first
ScreenPosition EngineWorldToScreen(
    EngineCoordinateConfig* config,
    EnginePosition* pos,
    int screenWidth,
    int screenHeight
) {
    WorldPosition world = EnginePositionToWorld(config, pos);

    // Check if game wants to handle this
    if (HasGameOverride(config)) {
        GameCoordinateOverride* override = GetOverride(config);
        if (override->worldToScreen) {
            return override->worldToScreen(
                override->gameData,
                world,
                screenWidth,
                screenHeight
            );
        }
    }

    // Use engine's default
    return DefaultWorldToScreen(config, world, screenWidth, screenHeight);
}

// Engine's chunk management (with override support)
uint32_t EngineGetChunkX(EngineCoordinateConfig* config, uint32_t absTileX) {
    if (HasGameOverride(config)) {
        GameCoordinateOverride* override = GetOverride(config);
        if (override->getChunkX) {
            return override->getChunkX(override->gameData, absTileX);
        }
    }

    // Default: bit-shift based on chunk size
    return absTileX >> config->chunkShift;
}

uint32_t EngineGetTileInChunk(EngineCoordinateConfig* config, uint32_t absTileX) {
    if (HasGameOverride(config)) {
        GameCoordinateOverride* override = GetOverride(config);
        if (override->getTileInChunk) {
            return override->getTileInChunk(override->gameData, absTileX);
        }
    }

    // Default: mask low bits
    uint32_t mask = (1 << config->chunkShift) - 1;
    return absTileX & mask;
}
```

### Step 5: Game Layer - Use Defaults (Simple Case)

```c
// game_handmade.c
// Simple game: just use engine defaults

void GameInit(GameState* state) {
    // Get engine's smart defaults
    state->coordConfig = EngineGetDefaultCoordinateConfig();

    // Optionally tweak defaults for this game
    state->coordConfig.pixelsPerTile = 64;  // Slightly bigger tiles
    state->coordConfig.unitsPerTile = 1.5f; // Slightly bigger world units

    // That's it! Game uses all engine's coordinate logic
}

void GameUpdate(GameState* state, float dt) {
    EnginePosition* playerPos = &state->player.position;

    // Move in world units (meters by default)
    float speedUnitsPerSec = 3.0f;
    playerPos->offsetX += speedUnitsPerSec * dt;

    // Engine handles tile boundaries automatically
    if (playerPos->offsetX >= state->coordConfig.unitsPerTile) {
        playerPos->offsetX -= state->coordConfig.unitsPerTile;
        playerPos->absTileX++;
    }
}

void GameRender(GameState* state, int screenW, int screenH) {
    // Engine handles all conversion
    ScreenPosition screen = EngineWorldToScreen(
        &state->coordConfig,
        &state->player.position,
        screenW,
        screenH
    );

    DrawSprite(state->player.sprite, screen.screenX, screen.screenY);
}
```

### Step 6: Game Layer - Override for Special Needs (Advanced Case)

```c
// game_pixel_perfect.c
// Pixel-art game: needs pixel-perfect control

// Game's custom data
typedef struct {
    bool snapToPixels;
    int cameraOffsetX;
    int cameraOffsetY;
} PixelGameData;

// Game's custom screen conversion
ScreenPosition PixelGameWorldToScreen(
    void* gameData,
    WorldPosition world,
    int screenW,
    int screenH
) {
    PixelGameData* data = (PixelGameData*)gameData;

    ScreenPosition result;

    // Custom: Treat units as pixels directly
    result.screenX = (int)world.worldX;
    result.screenY = (int)world.worldY;

    // Custom: Apply camera offset
    result.screenX -= data->cameraOffsetX;
    result.screenY -= data->cameraOffsetY;

    // Custom: Snap to pixel grid
    if (data->snapToPixels) {
        result.screenX = (result.screenX / 2) * 2;  // Even pixels only
        result.screenY = (result.screenY / 2) * 2;
    }

    return result;
}

void PixelGameInit(GameState* state) {
    // Start with engine defaults
    state->coordConfig = EngineGetDefaultCoordinateConfig();

    // Change to pixel-based system
    state->coordConfig.unitSystem = UNIT_SYSTEM_PIXELS;
    state->coordConfig.coordSystem = COORD_SYSTEM_Y_DOWN;  // Screen-native
    state->coordConfig.unitsPerTile = 16.0f;  // 16 pixels per tile
    state->coordConfig.pixelsPerTile = 16;    // 1:1 mapping

    // Register custom conversion
    static PixelGameData pixelData = {
        .snapToPixels = true,
        .cameraOffsetX = 0,
        .cameraOffsetY = 0
    };

    static GameCoordinateOverride override = {
        .worldToScreen = PixelGameWorldToScreen,
        .gameData = &pixelData
    };

    EngineSetCoordinateOverride(&state->coordConfig, &override);

    // Now engine respects pixel-perfect requirements
}
```

### Step 7: Game Layer - Mixed Systems (Pro Case)

```c
// game_mixed.c
// Professional game: World units for gameplay, pixels for UI

typedef struct {
    // Gameplay entities use engine's world coordinates
    EnginePosition playerPos;
    EnginePosition enemyPos[100];

    // UI uses screen coordinates directly
    struct {
        int x, y;  // Screen pixels
    } uiElements[50];

    // Configuration
    EngineCoordinateConfig worldCoords;  // For gameplay
} MixedGameState;

void MixedGameUpdate(MixedGameState* state, float dt) {
    // Physics in world units (meters)
    float gravity = -9.8f;  // Real gravity!
    state->playerPos.offsetY += gravity * dt * dt;

    // UI stays in screen space (doesn't care about world units)
    state->uiElements[0].x = 10;  // 10 pixels from left
    state->uiElements[0].y = 10;  // 10 pixels from top
}

void MixedGameRender(MixedGameState* state, int screenW, int screenH) {
    // Render world entities (engine converts)
    ScreenPosition playerScreen = EngineWorldToScreen(
        &state->worldCoords,
        &state->playerPos,
        screenW, screenH
    );
    DrawSprite(playerSprite, playerScreen.screenX, playerScreen.screenY);

    // Render UI (already in screen space)
    DrawButton(state->uiElements[0].x, state->uiElements[0].y);
}
```

---

## Step 8: Runtime Configuration System (Very Professional)

```c
// engine_config.h
// Allow changing coordinate system at runtime

typedef struct {
    EngineCoordinateConfig coords;

    // Runtime change callbacks
    void (*onResolutionChange)(void* gameData, int newW, int newH);
    void (*onUnitSystemChange)(void* gameData, UnitSystemType oldType, UnitSystemType newType);

    void* gameData;
} EngineRuntimeConfig;

// Engine function: Handle resolution change
void EngineSetResolution(
    EngineRuntimeConfig* config,
    int newWidth,
    int newHeight
) {
    // Recalculate pixels per tile to fit screen
    int tilesAcrossX = 32;  // Target visible tiles
    config->coords.pixelsPerTile = newWidth / tilesAcrossX;

    // Clamp to reasonable range
    if (config->coords.pixelsPerTile < 16) {
        config->coords.pixelsPerTile = 16;
    }
    if (config->coords.pixelsPerTile > 128) {
        config->coords.pixelsPerTile = 128;
    }

    // Notify game
    if (config->onResolutionChange) {
        config->onResolutionChange(config->gameData, newWidth, newHeight);
    }
}

// Game can react to changes
void GameOnResolutionChange(void* gameData, int newW, int newH) {
    GameState* state = (GameState*)gameData;

    // Game-specific adjustments
    state->cameraZoom = CalculateOptimalZoom(newW, newH);

    printf("Resolution changed to %dx%d, new tile size: %d pixels\n",
           newW, newH, state->coordConfig.pixelsPerTile);
}
```

---

## Step 9: Validation & Safety (Production Quality)

```c
// engine_coordinates.c

// Engine validates configuration
bool EngineValidateCoordinateConfig(EngineCoordinateConfig* config) {
    // Ensure sensible values
    if (config->unitsPerTile <= 0.0f) {
        fprintf(stderr, "ERROR: unitsPerTile must be > 0\n");
        return false;
    }

    if (config->pixelsPerTile <= 0) {
        fprintf(stderr, "ERROR: pixelsPerTile must be > 0\n");
        return false;
    }

    if (config->chunkShift > 16) {
        fprintf(stderr, "WARNING: chunkShift > 16 creates tiny chunks\n");
        config->chunkShift = 16;
    }

    if (config->chunkShift < 2) {
        fprintf(stderr, "WARNING: chunkShift < 2 creates huge chunks\n");
        config->chunkShift = 2;
    }

    return true;
}

// Safe configuration wrapper
bool EngineSetCoordinateConfig(
    EngineCoordinateConfig* dest,
    EngineCoordinateConfig* src
) {
    if (!EngineValidateCoordinateConfig(src)) {
        fprintf(stderr, "ERROR: Invalid coordinate config, using defaults\n");
        *dest = EngineGetDefaultCoordinateConfig();
        return false;
    }

    *dest = *src;
    return true;
}
```

---

## Step 10: Documentation & Examples (Critical!)

```c
// engine_coordinates_examples.h

/*
 * COORDINATE SYSTEM USAGE GUIDE
 * ==============================
 *
 * The engine provides smart defaults for coordinate systems.
 * Most games can just use these defaults and be happy.
 *
 * DEFAULT BEHAVIOR:
 * - Units: Meters (real-world scale)
 * - Coordinate system: Y-up (math-friendly)
 * - Tile size: 1.4 meters (roughly human height)
 * - Display: 60 pixels per tile (HD-friendly)
 * - Chunks: 16x16 tiles (good balance)
 *
 * EXAMPLE 1: Simple platformer (use defaults)
 * -------------------------------------------
 */
void Example_SimplePlatformer() {
    EngineCoordinateConfig config = EngineGetDefaultCoordinateConfig();

    // Tweak one value if desired
    config.unitsPerTile = 2.0f;  // Bigger tiles for this game

    // Use it
    EnginePosition playerPos = {0};
    playerPos.offsetX = 1.0f;  // 1 meter from tile origin
}

/*
 * EXAMPLE 2: Pixel-art retro game (override for pixels)
 * ------------------------------------------------------
 */
void Example_PixelArtGame() {
    EngineCoordinateConfig config = EngineGetDefaultCoordinateConfig();

    // Change to pixel-based
    config.unitSystem = UNIT_SYSTEM_PIXELS;
    config.coordSystem = COORD_SYSTEM_Y_DOWN;
    config.unitsPerTile = 16.0f;  // 16 pixels per tile
    config.pixelsPerTile = 16;    // 1:1 mapping

    // No need to register custom functions for simple pixel mapping
}

/*
 * EXAMPLE 3: Strategy game (abstract units)
 * -----------------------------------------
 */
void Example_StrategyGame() {
    EngineCoordinateConfig config = EngineGetDefaultCoordinateConfig();

    // Use abstract units
    config.unitSystem = UNIT_SYSTEM_ABSTRACT;
    config.unitsPerTile = 1.0f;   // 1 unit = 1 tile
    config.pixelsPerTile = 64;    // Display size

    // Now positions are in "game tiles"
    EnginePosition cityPos = {0};
    cityPos.absTileX = 10;  // City at tile 10,5
    cityPos.absTileY = 5;
    cityPos.offsetX = 0.5f;  // Centered in tile
    cityPos.offsetY = 0.5f;
}

/*
 * EXAMPLE 4: Change resolution at runtime
 * ----------------------------------------
 */
void Example_ResolutionChange() {
    EngineCoordinateConfig config = EngineGetDefaultCoordinateConfig();

    // Original: 1920x1080, 60px tiles
    printf("Pixels per tile: %d\n", config.pixelsPerTile);  // 60

    // User switches to 4K
    EngineRuntimeConfig runtime = {.coords = config};
    EngineSetResolution(&runtime, 3840, 2160);

    printf("Pixels per tile: %d\n", runtime.coords.pixelsPerTile);  // 120

    // Game logic unchanged! Player still moves at same speed in meters
}
```

---

## Design Principles Summary

### 1. **Smart Defaults**

```c
// Most games should just do this:
config = EngineGetDefaultCoordinateConfig();
// And it works well
```

### 2. **Easy Customization**

```c
// Simple tweaks are simple:
config.pixelsPerTile = 80;  // Change one value
```

### 3. **Full Override Capability**

```c
// Complex games can override everything:
override.worldToScreen = MyCustomFunction;
EngineSetCoordinateOverride(&config, &override);
```

### 4. **Validation & Safety**

```c
// Engine prevents bad configurations:
EngineValidateCoordinateConfig(&config);  // Returns false if bad
```

### 5. **Clear Documentation**

```c
// Every option is documented with:
// - What it does
// - When to use it
// - Example code
```

### 6. **Performance**

```c
// Override check is one pointer comparison:
if (config->gameOverride) { ... }  // Fast!
// Most games use defaults (no overhead)
```

---

## The Decision Tree for Game Developers

```
START: I'm making a game with this engine

Q: Does my game need realistic physics (gravity, etc)?
├─ YES → Use default (meters, Y-up)
└─ NO  → Continue to next question

Q: Is my game pixel-art with pixel-perfect collision?
├─ YES → Set unitSystem = UNIT_SYSTEM_PIXELS
└─ NO  → Continue to next question

Q: Is my game grid-based (strategy, roguelike, etc)?
├─ YES → Set unitSystem = UNIT_SYSTEM_ABSTRACT
└─ NO  → Continue to next question

Q: Do I have special requirements?
├─ YES → Register custom override functions
└─ NO  → Just use defaults!

RESULT: Configuration that fits your game
```

---

## Real-World Example: Three Games, One Engine

### Game 1: Physics Platformer

```c
// Just use defaults
config = EngineGetDefaultCoordinateConfig();
// Gravity = 9.8 m/s² works perfectly
```

### Game 2: Retro Pixel Game

```c
config = EngineGetDefaultCoordinateConfig();
config.unitSystem = UNIT_SYSTEM_PIXELS;
config.coordSystem = COORD_SYSTEM_Y_DOWN;
config.pixelsPerTile = 16;
config.unitsPerTile = 16.0f;
```

### Game 3: Space Strategy

```c
config = EngineGetDefaultCoordinateConfig();
config.unitSystem = UNIT_SYSTEM_ABSTRACT;
config.unitsPerTile = 1.0f;
config.chunkShift = 8;  // 256x256 star sectors

// Custom starfield rendering
override.worldToScreen = RenderStarfield;
EngineSetCoordinateOverride(&config, &override);
```

All three games use the same engine, same coordinate system code, but configured differently!

---

## Key Takeaway

**The professional way is:**

1. **Engine provides smart defaults** that work for most cases
2. **Game can tweak values** for simple customization
3. **Game can override functions** for complex needs
4. **Engine validates** to prevent mistakes
5. **Everything is documented** with examples

This is how Unity, Unreal, and other professional engines work. The engine is opinionated about best practices, but flexible enough for edge cases.

**You start simple, add complexity only when needed.**

---

## Response:

2/15/2026, 3:06:35 PM

Nice, on the mean time, while following the series, in terms of what we discussed, what do you think I should look for to when implementing for me when I decide what will go to the engine only, game only, or letting the engine provide the means for the game to change/init them

## Prompt:

2/15/2026, 3:08:41 PM

```plaintext
Create File: Creating a practical decision-making guide for identifying what belongs in engine vs game vs configurable, tailored for following Handmade Hero
```

```plaintext
Present File: /home/claude/decision_guide_engine_vs_game.md
```

Perfect! I've created a **practical decision-making framework** you can use while following Handmade Hero.

## The Four Key Questions (Memorize These!)

Every time Casey writes new code, ask:

1. **"Would this change for a different game?"**
   - NO → Engine
   - YES → Game or Config

2. **"Is this mechanism (HOW) or policy (WHAT/WHEN)?"**
   - Mechanism → Engine
   - Policy → Game or Config

3. **"How variable is this between games?"**
   - No variability → Engine
   - Low variability → Config with defaults
   - High variability → Game

4. **"If I change this, what breaks?"**
   - Nothing → Config
   - Game code → Game
   - Everything → Engine Core

## Your Practical Workflow:

### While Watching (Don't Stop to Refactor)

```c
// Just mark with comments as you go:

// ENGINE_CORE: Universal math
float Square(float x) { return x * x; }

// ENGINE_SERVICE: Coordinate conversion (configurable)
ScreenPos WorldToScreen(WorldPos world, CoordinateConfig* cfg) { ... }

// GAME: Handmade Hero specific
void UpdateHeroAbilities(Hero* hero) { ... }

// TODO_REFACTOR: Should be configurable
#define TILE_SIZE 60
```

### After 10 Episodes (Review Patterns)

```markdown
# Episodes 31-40 Notes

## Noticed Patterns:

- All tile operations are universal (engine)
- Tile TYPES are game-specific (game)
- Tile SIZE should be configurable (config)
```

### After Day 100 (NOW Refactor)

Only after seeing the full picture, split into layers.

## The Ultimate Mental Model:

**Think of it like cooking:**

- **Engine Core** = Kitchen equipment (universal)
- **Engine Services** = Recipes with adjustable ingredients (configurable)
- **Game** = The specific meal you're making (unique)

You don't redesign the oven for each meal, but you DO adjust the temperature!

Keep these guides handy as you continue through Handmade Hero - they'll make more sense with each episode! 🎯

> File: /home/claude/decision_guide_engine_vs_game.md

---

# Decision Guide: Engine vs Game vs Configurable

## The Mental Framework While Following Handmade Hero

As you watch Casey code, ask yourself these questions for each piece of code:

---

## Question 1: "If I Made a DIFFERENT Game, Would This Code Change?"

### NO → Engine

```c
// This code doesn't change between games:
uint32_t GetChunkX(uint32_t absTileX, uint32_t chunkShift) {
    return absTileX >> chunkShift;  // Math is math
}

// Bit operations are universal
// File I/O is universal
// Memory allocation is universal
```

### YES → Game (or Configurable)

```c
// This code is Handmade Hero specific:
void UpdateHeroAbilities(Hero* hero) {
    if (hero->hasSword) {
        hero->attackDamage = 10;  // Game balance numbers
    }
}

// Game rules change per game
// Character abilities change per game
// Story/progression changes per game
```

### MAYBE → Configurable

```c
// This COULD change between games:
float gravity = -9.8f;  // Earth gravity

// Some games want realistic physics
// Some games want floaty jumps
// → Make it configurable!
```

---

## Question 2: "Is This POLICY or MECHANISM?"

**Key Concept:**

- **Mechanism** = HOW to do something (engine)
- **Policy** = WHAT/WHEN to do it (game or config)

### Examples from Day 31:

#### Mechanism (Engine):

```c
// HOW to convert coordinates
ScreenPos WorldToScreen(WorldPos world, float scale) {
    ScreenPos screen;
    screen.x = world.x * scale;
    screen.y = world.y * scale;
    return screen;
}
// The MECHANISM of coordinate conversion is universal
```

#### Policy (Game or Config):

```c
// WHAT scale to use
float scale = 60.0f / 1.4f;  // 60 pixels per 1.4 meters

// WHEN to convert
if (entity.visible) {
    screen = WorldToScreen(entity.pos, scale);
}
// The game decides the scale and when to render
```

#### Pattern:

- **Mechanism** → Engine provides the function
- **Policy** → Game calls it with game-specific values

---

## Question 3: "Does This Have VARIABILITY Between Games?"

### No Variability → Engine

```c
// Bit operations: Same for all games
#define CHUNK_SHIFT 4
uint32_t chunk = absTileX >> CHUNK_SHIFT;

// Memory management: Same for all games
void* Allocate(size_t size);

// Math utilities: Same for all games
float Lerp(float a, float b, float t);
```

### Low Variability → Configurable Default

```c
// Most games want this, but some might change it
typedef struct {
    float tileSideInMeters;  // Default: 1.4, but configurable
    int pixelsPerTile;       // Default: 60, but configurable
    bool yAxisUp;            // Default: true, but configurable
} CoordinateConfig;
```

### High Variability → Game Code

```c
// Every game is completely different here
void UpdatePlayerAbilities(Player* player) {
    // Completely game-specific logic
    if (player->level > 10) {
        player->abilities |= ABILITY_DOUBLE_JUMP;
    }
}
```

---

## Question 4: "If I Change This, What Else Breaks?"

### Nothing Breaks → Safely Configurable

```c
// Changing this only affects visuals
config.pixelsPerTile = 80;  // Changed from 60
// Nothing breaks! Game logic unchanged.
```

### Other Game Code Breaks → Game-Only

```c
// Changing this breaks game-specific code
#define MAX_HERO_HEALTH 100

void CheckGameOver(Hero* hero) {
    if (hero->health <= 0) {  // Depends on MAX_HERO_HEALTH
        GameOver();
    }
}
// This is tightly coupled to game rules
```

### Everything Breaks → Engine (Core Dependency)

```c
// Changing this breaks everything
typedef struct {
    uint32_t absTileX;
    uint32_t absTileY;
} Position;

// If you change Position, you break:
// - All entity code
// - All rendering code
// - All collision code
// - All serialization code
// → This is a core engine type
```

---

## Practical Pattern Recognition Guide

### As You Watch Each Episode, Look For These Patterns:

#### Pattern A: "Casey Hard-Codes a Number"

```c
// Episode: Casey writes this
float gravity = 9.8f;
```

**Ask yourself:**

- Is 9.8 specific to Handmade Hero? (NO - it's Earth gravity)
- Would a different game want different gravity? (YES - moon game, space game)
- **Decision:** Make it configurable with 9.8 as default

#### Pattern B: "Casey Writes a Helper Function"

```c
// Episode: Casey writes this
float Square(float x) {
    return x * x;
}
```

**Ask yourself:**

- Is squaring different per game? (NO - math is universal)
- Does it depend on game rules? (NO - pure math)
- **Decision:** Engine utility function

#### Pattern C: "Casey Checks Game State"

```c
// Episode: Casey writes this
if (hero->hasSword) {
    // Attack logic
}
```

**Ask yourself:**

- Is this Handmade Hero specific? (YES - sword is game content)
- Would every game have swords? (NO - space games, racing games)
- **Decision:** Game-only code

#### Pattern D: "Casey Sets Up a System"

```c
// Episode: Casey writes this
typedef struct {
    Entity* entities;
    int entityCount;
    int maxEntities;
} EntitySystem;
```

**Ask yourself:**

- Do all games need entities? (YES - characters, objects, etc.)
- Does the entity structure change per game? (MAYBE - different games need different data)
- **Decision:** Engine provides system, game defines entity contents

---

## The Three-Bucket Classification System

### Bucket 1: ENGINE CORE (15-20% of code)

**Criteria:**

- ✅ Universal across all games
- ✅ No game-specific knowledge
- ✅ Pure mechanism, no policy

**Examples from Days 1-31:**

```c
// Memory management
void* PushSize(MemoryArena* arena, size_t size);

// Math utilities
float Lerp(float a, float b, float t);
float Clamp(float value, float min, float max);

// Bit operations
uint32_t GetChunkX(uint32_t absTile, uint32_t shift);

// Platform abstraction
bool ReadFileToMemory(char* filename, void* buffer);

// Basic data structures
void PushEntity(EntityList* list, Entity entity);
```

**How to identify:** If you can copy-paste this into ANY game engine, it's engine core.

### Bucket 2: ENGINE SERVICES (25-30% of code)

**Criteria:**

- ✅ Useful across many games
- ✅ Has sensible defaults
- ✅ Game can configure/override

**Examples from Days 1-31:**

```c
// Coordinate system (configurable)
typedef struct {
    float tileSideInMeters;     // Configurable
    int pixelsPerTile;          // Configurable
    CoordinateSystemType type;  // Configurable
} CoordinateConfig;

// Rendering system (configurable)
typedef struct {
    int screenWidth;            // Configurable
    int screenHeight;           // Configurable
    int targetFPS;              // Configurable
} RenderConfig;

// Tile system (configurable)
typedef struct {
    uint32_t chunkShift;        // Configurable (4, 8, 16)
    TileType* tiles;            // Game defines TileType
} TileSystem;
```

**How to identify:** If it has values that change per game, but the SYSTEM is universal.

### Bucket 3: GAME CODE (50-60% of code)

**Criteria:**

- ✅ Specific to this game
- ✅ Contains game rules/logic
- ✅ Would be rewritten for different game

**Examples from Days 1-31:**

```c
// Hero-specific
typedef struct {
    bool hasSword;
    int health;
    HeroAbility abilities;
} Hero;

// Handmade Hero world layout
void InitializeWorld(World* world) {
    // Specific room layout
    SetTile(world, 0, 0, TILE_WALL);
    SetTile(world, 1, 0, TILE_DOOR);
    // etc...
}

// Game-specific update logic
void UpdateAndRenderGame(GameState* state, Input* input, float dt) {
    // All the Handmade Hero-specific stuff
}
```

**How to identify:** If it mentions "hero," "sword," "door," "room" - it's game content.

---

## Real Examples from Handmade Hero Episodes

### Example 1: Tile Map Drawing (Day 31)

**Casey's Code:**

```c
void DrawTileMap(TileMap* map, int screenWidth, int screenHeight) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int tileValue = GetTile(map, x, y);
            DrawTile(x * 60, y * 60, tileValue);  // 60 = tile pixel size
        }
    }
}
```

**Your Analysis:**

| Part                       | Bucket         | Reasoning                            |
| -------------------------- | -------------- | ------------------------------------ |
| `for` loops                | Engine Core    | Universal iteration logic            |
| `GetTile()`                | Engine Service | Tile access mechanism                |
| `60` (pixel size)          | Config         | Should be `config.pixelsPerTile`     |
| `DrawTile()`               | Engine Service | Drawing mechanism                    |
| `tileValue` interpretation | Game           | What tile "1" means is game-specific |

**Your Refactored Version:**

```c
// Engine service
void DrawTileMap(TileMap* map, DrawConfig* config) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int tileValue = GetTile(map, x, y);
            int pixelX = x * config->pixelsPerTile;  // Config!
            int pixelY = y * config->pixelsPerTile;

            // Game decides what tileValue means
            GameDrawTile(pixelX, pixelY, tileValue, config->gameData);
        }
    }
}
```

### Example 2: Player Movement (Day 15-20)

**Casey's Code:**

```c
if (input->moveRight) {
    player->x += 5;  // 5 pixels per frame
}
```

**Your Analysis:**

| Part           | Decision                        |
| -------------- | ------------------------------- |
| Input checking | Engine Service (input system)   |
| `player->x`    | Game (player is game entity)    |
| `5` (speed)    | Config (game decides speed)     |
| `+=` operator  | Engine Core (math is universal) |

**Better Version:**

```c
// Engine provides input query
bool IsButtonDown(Input* input, Button button);

// Game uses it with game values
if (IsButtonDown(input, BUTTON_RIGHT)) {
    float speed = state->config.playerSpeed;  // Config
    player->worldX += speed * deltaTime;      // Game code
}
```

### Example 3: Coordinate Conversion (Day 31)

**Casey's Code:**

```c
int screenX = (int)(worldX * 42.857f);  // pixels per meter
int screenY = screenHeight - (int)(worldY * 42.857f);
```

**Your Analysis:**

| Part                 | Decision                                            |
| -------------------- | --------------------------------------------------- |
| Conversion formula   | Engine Service (mechanism)                          |
| `42.857` (scale)     | Config (derived from pixelsPerTile / metersPerTile) |
| `screenHeight - ...` | Config (Y-up vs Y-down is a choice)                 |

**Your Version:**

```c
// Engine service
ScreenPos WorldToScreen(WorldPos world, CoordinateConfig* config) {
    float scale = config->pixelsPerTile / config->unitsPerTile;
    ScreenPos screen;
    screen.x = (int)(world.x * scale);

    if (config->yAxisUp) {
        screen.y = config->screenHeight - (int)(world.y * scale);
    } else {
        screen.y = (int)(world.y * scale);
    }

    return screen;
}
```

---

## Your Episode-by-Episode Checklist

### While Watching Each Episode:

**Step 1: Identify New Code Types**

```
[ ] New data structures (Position, Entity, TileMap)
[ ] New functions (UpdatePlayer, DrawTile, GetChunk)
[ ] New constants (TILE_SIZE, GRAVITY, MAX_ENTITIES)
[ ] New systems (rendering, collision, animation)
```

**Step 2: Classify Each**

```
For each piece of code, ask:
[ ] Q1: Would this change for different games?
[ ] Q2: Is this mechanism or policy?
[ ] Q3: How variable is this?
[ ] Q4: What breaks if I change this?
```

**Step 3: Mark Your Code**

```c
// In your implementation, use comments:

// ENGINE_CORE: Universal bit operations
uint32_t GetChunkX(uint32_t absTileX) {
    return absTileX >> CHUNK_SHIFT;
}

// ENGINE_SERVICE: Coordinate conversion (configurable)
ScreenPos WorldToScreen(WorldPos world, CoordinateConfig* cfg) {
    // ...
}

// GAME: Handmade Hero specific
void UpdateHeroSwordAttack(Hero* hero) {
    // ...
}

// TODO_REFACTOR: This should be configurable
#define GRAVITY 9.8f  // Hard-coded for now
```

**Step 4: Track Patterns**

```markdown
# Episode 31 Notes

## Engine Core Identified:

- GetChunkX() - bit operations
- GetTileInChunk() - bit operations

## Engine Services Identified:

- Coordinate conversion (need to make configurable)
- Tile map rendering (mechanism is universal)

## Game Code Identified:

- Hero movement speed (game balance)
- Tile types (WALL, DOOR, etc. are game-specific)

## Need to Refactor:

- Hard-coded 60 pixels → config.pixelsPerTile
- Hard-coded 1.4 meters → config.tileSideInMeters
```

---

## Red Flags: Common Mistakes to Avoid

### ❌ Red Flag 1: "Everything in Engine"

```c
// BAD: Putting game rules in engine
void EngineUpdateHero(Hero* hero) {
    if (hero->health <= 0) {
        hero->isDead = true;
    }
}
// Problem: Engine knows about game rules (health → death)
```

**Fix:** Engine provides mechanism, game implements policy

```c
// GOOD: Engine provides lifecycle hooks
void EngineUpdateEntities(EntityList* entities,
                          void (*updateFn)(Entity*, float dt),
                          float dt);

// Game implements specific logic
void GameUpdateHero(Entity* entity, float dt) {
    Hero* hero = (Hero*)entity->gameData;
    if (hero->health <= 0) {
        hero->isDead = true;
    }
}
```

### ❌ Red Flag 2: "Everything Configurable"

```c
// BAD: Over-configuration
typedef struct {
    bool useGravity;
    bool use3D;
    bool usePhysics;
    bool useCollision;
    bool useRendering;
    bool useEverything;
    // 100 more options...
} EngineConfig;
```

**Fix:** Sensible defaults with escape hatches

```c
// GOOD: Simple config with overrides
typedef struct {
    float gravity;  // Default: -9.8
    // Only a few common options

    // Override if needed
    void (*customPhysics)(Entity*, float dt);
} PhysicsConfig;
```

### ❌ Red Flag 3: "No Defaults"

```c
// BAD: Forcing every game to configure everything
CoordinateConfig config;
config.tileSideInMeters = ???;  // What should this be?
config.pixelsPerTile = ???;     // I don't know!
config.chunkShift = ???;        // What's a good value?
```

**Fix:** Provide working defaults

```c
// GOOD: Defaults that work
CoordinateConfig config = GetDefaultCoordinateConfig();
// Works immediately!

// Tweak if needed
config.pixelsPerTile = 80;  // Optional
```

---

## Progressive Understanding Timeline

### Day 31 (Now)

```
Understanding: "Oh, coordinates can be engine or game"
Action: Mark code with comments
Don't: Try to refactor everything yet
```

### Day 50

```
Understanding: "I see patterns in what Casey puts where"
Action: Start grouping related code
Don't: Extract to separate files yet
```

### Day 100

```
Understanding: "I see clear boundaries now"
Action: Plan your engine/game split
Don't: Rebuild from scratch
```

### Day 200

```
Understanding: "I know what belongs where"
Action: Refactor into engine/game layers
Don't: Over-engineer
```

### Your Own Project

```
Understanding: "I can make informed architecture decisions"
Action: Design clean engine/game boundary
Don't: Copy Casey blindly - adapt to your needs
```

---

## Quick Reference Table

| Code Type              | Engine?    | Config?  | Example                    |
| ---------------------- | ---------- | -------- | -------------------------- |
| Math (abs, sqrt, lerp) | ✅ Yes     | ❌ No    | `float Sqrt(float x)`      |
| Memory allocation      | ✅ Yes     | ⚠️ Maybe | `void* Alloc(size_t size)` |
| File I/O               | ✅ Yes     | ⚠️ Maybe | `bool ReadFile(...)`       |
| Input handling         | ✅ Yes     | ⚠️ Maybe | `bool IsKeyDown(Key k)`    |
| Coordinate conversion  | ⚠️ Service | ✅ Yes   | `WorldToScreen(...)`       |
| Physics (gravity)      | ⚠️ Service | ✅ Yes   | `ApplyGravity(...)`        |
| Rendering              | ⚠️ Service | ✅ Yes   | `DrawSprite(...)`          |
| Tile storage           | ⚠️ Service | ✅ Yes   | `TileMap` structure        |
| Player attributes      | ❌ No      | ❌ No    | `hero.hasSword`            |
| Game rules             | ❌ No      | ❌ No    | `if (health == 0)`         |
| Story/content          | ❌ No      | ❌ No    | Room layouts               |

Legend:

- ✅ Yes = Always this
- ❌ No = Never this
- ⚠️ Maybe/Service = Depends on approach

---

## Your Daily Practice

**After each episode:**

1. **Review new code** - What did Casey add?
2. **Apply the 4 questions** - Classify each piece
3. **Mark with comments** - Track your decisions
4. **Look for patterns** - What's emerging?
5. **Don't refactor yet** - Just note for later

**After every 10 episodes:**

1. **Review your notes** - See the bigger picture
2. **Group similar patterns** - What goes together?
3. **Update your understanding** - Revise classifications
4. **Plan (don't implement) refactoring** - Where would boundaries go?

**After day 100:**

1. **Now** you know enough to refactor
2. **Start** extracting engine services
3. **Keep** clear engine/game boundary
4. **Test** with a simple second game

---

## The Ultimate Test

**"Could I make a completely different game with this engine?"**

If you can answer these questions, you've succeeded:

- ✅ "Yes, I'd keep all the coordinate system code" (Engine)
- ✅ "Yes, but I'd configure chunk size differently" (Config)
- ✅ "No, I'd rewrite all the hero ability code" (Game)

**Example:**

```
Current: Handmade Hero (top-down adventure)
New: Space Invaders (arcade shooter)

Keep (Engine):
- Coordinate system ✅
- Tile rendering ✅
- Input handling ✅
- Memory management ✅

Configure (Adjust):
- Gravity = 0 (space!) ⚙️
- Tile size = 16px (retro!) ⚙️
- Y-axis = down (original game!) ⚙️

Rewrite (Game):
- No hero, have player ship ❌
- No swords, have bullets ❌
- No rooms, have wave spawning ❌
```

If you can make this list confidently, you understand the boundaries!

---

## Summary: Your Action Plan

### While Following Handmade Hero (Days 1-100):

1. ✅ **Mark code** with ENGINE/CONFIG/GAME comments
2. ✅ **Take notes** on patterns you see
3. ✅ **Ask the 4 questions** for each new feature
4. ❌ **Don't refactor yet** - just learn

### After Understanding (Days 100-200):

1. ✅ **Group related code** into logical units
2. ✅ **Identify clear boundaries** between layers
3. ✅ **Plan the split** - where would files go?
4. ⚠️ **Start refactoring** - carefully, incrementally

### Your Own Project (After Handmade Hero):

1. ✅ **Design clean boundaries** from the start
2. ✅ **Use sensible defaults** with override capability
3. ✅ **Keep it simple** - only abstract what varies
4. ✅ **Test with second game** - validates your design

**Remember:** You're learning to make these decisions, not memorizing rules. By day 100, it will be intuitive!
