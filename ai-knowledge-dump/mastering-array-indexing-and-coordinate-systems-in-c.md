# 🎯 Mastering Array Indexing & Coordinate Systems in C

## A Complete Course for Game Developers

---

## Table of Contents

1. [The Core Problem: Why This Is Confusing](#1-the-core-problem-why-this-is-confusing)
2. [Mental Models: How to Think About 2D Arrays](#2-mental-models-how-to-think-about-2d-arrays)
3. [The Three Coordinate Systems You'll Encounter](#3-the-three-coordinate-systems-youll-encounter)
4. [Naming Conventions That Prevent Bugs](#4-naming-conventions-that-prevent-bugs)
5. [Debugging Techniques](#5-debugging-techniques)
6. [Common Bug Patterns & How to Spot Them](#6-common-bug-patterns--how-to-spot-them)
7. [Defensive Programming Techniques](#7-defensive-programming-techniques)
8. [Exercises: Spot the Bug](#8-exercises-spot-the-bug)
9. [Exercises: Fix the Code](#9-exercises-fix-the-code)
10. [Exercises: Design From Scratch](#10-exercises-design-from-scratch)
11. [Quick Reference Cheat Sheet](#11-quick-reference-cheat-sheet)

---

## 1. The Core Problem: Why This Is Confusing

### 1.1 The Fundamental Mismatch

There's a fundamental mismatch between how we **think** about 2D space and how C **stores** 2D arrays:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     HOW WE THINK (Cartesian)                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│       Y ↑                                                               │
│         │                                                               │
│       3 │  ·  ·  ·  ·                                                   │
│       2 │  ·  ·  X  ·    ← Point at (2, 2)                              │
│       1 │  ·  ·  ·  ·                                                   │
│       0 │  ·  ·  ·  ·                                                   │
│         └──────────────→ X                                              │
│            0  1  2  3                                                   │
│                                                                         │
│   Access: point(x, y) = point(2, 2)                                     │
│   X comes FIRST, Y comes SECOND                                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                     HOW C STORES (Row-Major)                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Row 0 →  [0][0] [0][1] [0][2] [0][3]                                  │
│   Row 1 →  [1][0] [1][1] [1][2] [1][3]                                  │
│   Row 2 →  [2][0] [2][1] [2][2] [2][3]  ← Element at [2][2]             │
│   Row 3 →  [3][0] [3][1] [3][2] [3][3]                                  │
│                ↓                                                        │
│              Col 0  Col 1  Col 2  Col 3                                 │
│                                                                         │
│   Access: array[row][col] = array[2][2]                                 │
│   ROW comes FIRST, COL comes SECOND                                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 The Translation Problem

When you have a point at screen position `(x=150, y=90)` and want to find which tile it's in:

```
Screen Position:  x = 150, y = 90
Tile Size:        width = 60, height = 60
Tile Offset:      upper_left_x = -30, upper_left_y = 0

Calculation:
  tile_x = (150 - (-30)) / 60 = 180 / 60 = 3  (column 3)
  tile_y = (90 - 0) / 60 = 90 / 60 = 1        (row 1)

Array Access:
  ❌ WRONG: map[tile_x][tile_y] = map[3][1]  (row 3, col 1)
  ✅ RIGHT: map[tile_y][tile_x] = map[1][3]  (row 1, col 3)
```

### 1.3 Why Your Brain Gets It Wrong

Your brain naturally thinks:

- "I calculated X first, so X goes first in the array"
- "The variable is called `tile_x`, so it's the first index"

But C arrays don't care about your variable names. They care about:

- **First index** = which row (vertical position, Y)
- **Second index** = which column (horizontal position, X)

---

## 2. Mental Models: How to Think About 2D Arrays

### 2.1 Mental Model #1: The Spreadsheet

Think of a 2D array as a spreadsheet:

```
              Col 0   Col 1   Col 2   Col 3   Col 4
            ┌───────┬───────┬───────┬───────┬───────┐
    Row 0   │ [0][0]│ [0][1]│ [0][2]│ [0][3]│ [0][4]│
            ├───────┼───────┼───────┼───────┼───────┤
    Row 1   │ [1][0]│ [1][1]│ [1][2]│ [1][3]│ [1][4]│
            ├───────┼───────┼───────┼───────┼───────┤
    Row 2   │ [2][0]│ [2][1]│ [2][2]│ [2][3]│ [2][4]│
            └───────┴───────┴───────┴───────┴───────┘

To access cell in Row 1, Column 3:
  spreadsheet[1][3]  ← Row first, then Column
```

**Key insight:** You say "Row 1, Column 3" — the row comes first in speech AND in code.

### 2.2 Mental Model #2: The Address System

Think of it like a building address:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         BUILDING ADDRESS                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   "Apartment 3B" means:                                                 │
│     - Floor 3 (vertical, like row/Y)                                    │
│     - Unit B (horizontal, like column/X)                                │
│                                                                         │
│   building[floor][unit] = building[3][B]                                │
│                                                                         │
│   Similarly:                                                            │
│   map[row][col] = map[y][x]                                             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Mental Model #3: The Memory Layout

C stores 2D arrays in **row-major order** — each row is contiguous in memory:

```
Declaration: int map[3][4];  // 3 rows, 4 columns

Memory Layout (linear):
┌─────────────────────────────────────────────────────────────────────────┐
│ [0][0] [0][1] [0][2] [0][3] │ [1][0] [1][1] [1][2] [1][3] │ [2][0] ... │
└─────────────────────────────────────────────────────────────────────────┘
│←────── Row 0 ──────────────→│←────── Row 1 ──────────────→│←── Row 2 ──

Address calculation:
  &map[row][col] = base_address + (row * num_cols + col) * sizeof(int)
  &map[1][2]     = base_address + (1 * 4 + 2) * 4 = base_address + 24
```

**Key insight:** The first index (row) determines which "chunk" of memory, the second index (col) is the offset within that chunk.

### 2.4 Mental Model #4: The "Y First" Rule

Create a simple rule and ALWAYS follow it:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         THE "Y FIRST" RULE                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   When accessing a 2D array with screen coordinates:                    │
│                                                                         │
│   ┌─────────────────────────────────────────┐                           │
│   │  array[ Y ][ X ]                        │                           │
│   │        ↑     ↑                          │                           │
│   │       row   col                         │                           │
│   │     vertical horizontal                 │                           │
│   │       first  second                     │                           │
│   └─────────────────────────────────────────┘                           │
│                                                                         │
│   Mnemonic: "Why (Y) do I always forget? Because Y comes first!"        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. The Three Coordinate Systems You'll Encounter

### 3.1 Screen Coordinates (Pixels)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       SCREEN COORDINATES                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   (0,0)────────────────────────────→ X (increases right)                │
│     │                                                                   │
│     │    Player at (150, 90)                                            │
│     │         ↓                                                         │
│     │    ┌────────┐                                                     │
│     │    │ Player │                                                     │
│     │    └────────┘                                                     │
│     │                                                                   │
│     ↓                                                                   │
│     Y (increases DOWN)                                                  │
│                                                                         │
│   NOTE: Y increases DOWNWARD (opposite of math class!)                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Tile/Grid Coordinates

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        TILE COORDINATES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   tile_x (column): 0, 1, 2, 3, ...                                      │
│   tile_y (row):    0, 1, 2, 3, ...                                      │
│                                                                         │
│        col 0   col 1   col 2   col 3                                    │
│       ┌───────┬───────┬───────┬───────┐                                 │
│ row 0 │ (0,0) │ (1,0) │ (2,0) │ (3,0) │  ← tile_y = 0                   │
│       ├───────┼───────┼───────┼───────┤                                 │
│ row 1 │ (0,1) │ (1,1) │ (2,1) │ (3,1) │  ← tile_y = 1                   │
│       ├───────┼───────┼───────┼───────┤                                 │
│ row 2 │ (0,2) │ (1,2) │ (2,2) │ (3,2) │  ← tile_y = 2                   │
│       └───────┴───────┴───────┴───────┘                                 │
│          ↑       ↑       ↑       ↑                                      │
│       tile_x=0  =1      =2      =3                                      │
│                                                                         │
│   Conversion from screen:                                               │
│     tile_x = (screen_x - offset_x) / tile_width                         │
│     tile_y = (screen_y - offset_y) / tile_height                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Array Indices

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ARRAY INDICES                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Declaration: u32 map[ROW_COUNT][COL_COUNT];                        │
│                           ↑           ↑                                 │
│                        first dim   second dim                           │
│                         (rows)     (columns)                            │
│                                                                         │
│   Access: map[row_index][col_index]                                     │
│               ↑           ↑                                             │
│            tile_y      tile_x                                           │
│                                                                         │
│   THE CRITICAL TRANSLATION:                                             │
│   ┌─────────────────────────────────────────┐                           │
│   │  map[tile_y][tile_x]                    │                           │
│   │      ↑         ↑                        │                           │
│   │     row       col                       │                           │
│   │   (Y coord) (X coord)                   │                           │
│   └─────────────────────────────────────────┘                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Complete Translation Chain

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    COMPLETE TRANSLATION CHAIN                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   STEP 1: Screen Position                                               │
│   ─────────────────────────                                             │
│   player.x = 150  (pixels from left)                                    │
│   player.y = 90   (pixels from top)                                     │
│                                                                         │
│   STEP 2: Convert to Tile Coordinates                                   │
│   ────────────────────────────────────                                  │
│   tile_x = floor((player.x - offset_x) / tile_width)                    │
│   tile_x = floor((150 - (-30)) / 60) = floor(3.0) = 3                   │
│                                                                         │
│   tile_y = floor((player.y - offset_y) / tile_height)                   │
│   tile_y = floor((90 - 0) / 60) = floor(1.5) = 1                        │
│                                                                         │
│   STEP 3: Access Array                                                  │
│   ────────────────────                                                  │
│   tile_value = map[tile_y][tile_x]                                      │
│   tile_value = map[1][3]  ← Row 1, Column 3                             │
│                                                                         │
│   COMMON MISTAKE:                                                       │
│   ❌ map[tile_x][tile_y] = map[3][1]  ← WRONG! Row 3, Column 1          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Naming Conventions That Prevent Bugs

### 4.1 Strategy #1: Use Row/Col Instead of X/Y

```c
// ❌ CONFUSING: x and y can be swapped easily
int tile_x = calculate_tile_x(player.x);
int tile_y = calculate_tile_y(player.y);
int value = map[tile_x][tile_y];  // Is this right? Hard to tell!

// ✅ CLEAR: row and col match array semantics
int tile_col = calculate_tile_col(player.x);  // X → column
int tile_row = calculate_tile_row(player.y);  // Y → row
int value = map[tile_row][tile_col];  // Obviously correct!
```

### 4.2 Strategy #2: Name Arrays with Dimensions

```c
// ❌ AMBIGUOUS: What's the first dimension?
#define MAP_WIDTH 17
#define MAP_HEIGHT 9
u32 map[MAP_WIDTH][MAP_HEIGHT];  // Is this right?

// ✅ EXPLICIT: Dimensions are clear
#define MAP_ROW_COUNT 9    // Number of rows (Y direction)
#define MAP_COL_COUNT 17   // Number of columns (X direction)
u32 map[MAP_ROW_COUNT][MAP_COL_COUNT];  // [rows][cols] - obviously correct!
```

### 4.3 Strategy #3: Create Type Aliases

```c
// Define semantic types
typedef i32 TileRow;  // Y coordinate in tile space
typedef i32 TileCol;  // X coordinate in tile space

// Function signatures become self-documenting
u32 get_tile_value(TileRow row, TileCol col) {
    return map[row][col];  // Parameter order matches array order!
}

// Usage is clear
TileCol col = screen_x_to_tile_col(player.x);
TileRow row = screen_y_to_tile_row(player.y);
u32 value = get_tile_value(row, col);  // Can't mess this up!
```

### 4.4 Strategy #4: Create Helper Functions

```c
// Encapsulate the translation in one place
typedef struct {
    i32 row;
    i32 col;
} TileCoord;

TileCoord screen_to_tile(f32 screen_x, f32 screen_y, TileState* tiles) {
    TileCoord result;
    result.col = floor_f32_to_int32(
        (screen_x - tiles->upper_left_x) / tiles->width);
    result.row = floor_f32_to_int32(
        (screen_y - tiles->upper_left_y) / tiles->height);
    return result;
}

bool32 is_tile_walkable(TileState* tiles, TileCoord coord) {
    if (coord.row < 0 || coord.row >= MAP_ROW_COUNT ||
        coord.col < 0 || coord.col >= MAP_COL_COUNT) {
        return false;  // Out of bounds
    }
    return tiles->map[coord.row][coord.col] == 0;
}

// Usage - impossible to get wrong!
TileCoord player_tile = screen_to_tile(player.x, player.y, &tile_state);
if (is_tile_walkable(&tile_state, player_tile)) {
    // Move player
}
```

### 4.5 Strategy #5: Comment the Array Declaration

```c
typedef struct {
    // map[row][col] where:
    //   row = Y direction (0 = top, increases downward)
    //   col = X direction (0 = left, increases rightward)
    // Access: map[tile_y][tile_x]
    u32 map[MAP_ROW_COUNT][MAP_COL_COUNT];

    f32 upper_left_x;  // Screen X of tile [0][0]'s left edge
    f32 upper_left_y;  // Screen Y of tile [0][0]'s top edge
    f32 width;         // Width of each tile in pixels
    f32 height;        // Height of each tile in pixels
} TileState;
```

---

## 5. Debugging Techniques

### 5.1 Technique #1: Print Everything

When tile collision isn't working, add comprehensive logging:

```c
void debug_tile_check(f32 screen_x, f32 screen_y, TileState* tiles) {
    f32 relative_x = screen_x - tiles->upper_left_x;
    f32 relative_y = screen_y - tiles->upper_left_y;

    i32 tile_col = floor_f32_to_int32(relative_x / tiles->width);
    i32 tile_row = floor_f32_to_int32(relative_y / tiles->height);

    printf("═══════════════════════════════════════\n");
    printf("TILE DEBUG:\n");
    printf("  Screen pos:    (%.1f, %.1f)\n", screen_x, screen_y);
    printf("  Tile offset:   (%.1f, %.1f)\n", tiles->upper_left_x, tiles->upper_left_y);
    printf("  Relative pos:  (%.1f, %.1f)\n", relative_x, relative_y);
    printf("  Tile size:     (%.1f, %.1f)\n", tiles->width, tiles->height);
    printf("  Tile coord:    col=%d, row=%d\n", tile_col, tile_row);
    printf("  Array bounds:  cols=%d, rows=%d\n", MAP_COL_COUNT, MAP_ROW_COUNT);

    if (tile_row >= 0 && tile_row < MAP_ROW_COUNT &&
        tile_col >= 0 && tile_col < MAP_COL_COUNT) {
        printf("  Array access:  map[%d][%d] = %u\n",
               tile_row, tile_col, tiles->map[tile_row][tile_col]);
    } else {
        printf("  Array access:  OUT OF BOUNDS!\n");
    }
    printf("═══════════════════════════════════════\n");
}
```

### 5.2 Technique #2: Visual Debug Overlay

Draw the tile grid and highlight the player's current tile:

```c
void draw_debug_overlay(GameBackBuffer* buffer, TileState* tiles,
                        f32 player_x, f32 player_y) {
    // Calculate player's tile
    i32 player_col = floor_f32_to_int32(
        (player_x - tiles->upper_left_x) / tiles->width);
    i32 player_row = floor_f32_to_int32(
        (player_y - tiles->upper_left_y) / tiles->height);

    // Draw all tiles with their coordinates
    for (i32 row = 0; row < MAP_ROW_COUNT; ++row) {
        for (i32 col = 0; col < MAP_COL_COUNT; ++col) {
            f32 min_x = tiles->upper_left_x + col * tiles->width;
            f32 min_y = tiles->upper_left_y + row * tiles->height;

            // Highlight player's tile in a different color
            if (row == player_row && col == player_col) {
                // Draw bright green border for player's tile
                draw_rect_outline(buffer, min_x, min_y,
                                  min_x + tiles->width, min_y + tiles->height,
                                  0.0f, 1.0f, 0.0f, 1.0f);  // Green
            }

            // Draw tile value as color
            u32 tile_value = tiles->map[row][col];
            if (tile_value == 1) {
                // Solid tile - draw red X
                draw_x_mark(buffer, min_x, min_y,
                           min_x + tiles->width, min_y + tiles->height,
                           1.0f, 0.0f, 0.0f, 0.5f);  // Red
            }
        }
    }

    // Draw crosshair at exact player position
    draw_crosshair(buffer, player_x, player_y, 5, 1.0f, 1.0f, 0.0f, 1.0f);
}
```

### 5.3 Technique #3: Assertion Checking

Add assertions that catch the bug immediately:

```c
u32 get_tile_at(TileState* tiles, i32 row, i32 col) {
    // These assertions will fire if you swap row/col
    DEV_ASSERT_MSG(row >= 0 && row < MAP_ROW_COUNT,
        "Row %d out of bounds [0, %d)", row, MAP_ROW_COUNT);
    DEV_ASSERT_MSG(col >= 0 && col < MAP_COL_COUNT,
        "Col %d out of bounds [0, %d)", col, MAP_COL_COUNT);

    return tiles->map[row][col];
}

// If you accidentally call get_tile_at(tile_x, tile_y) with x=15, y=3
// and MAP_ROW_COUNT=9, you'll get:
// "Row 15 out of bounds [0, 9)"
// This immediately tells you the arguments are swapped!
```

### 5.4 Technique #4: Boundary Testing

Test at known positions to verify your math:

```c
void test_tile_coordinates(TileState* tiles) {
    printf("=== TILE COORDINATE TESTS ===\n");

    // Test: Upper-left corner of tile [0][0]
    {
        f32 test_x = tiles->upper_left_x + 1;  // Just inside
        f32 test_y = tiles->upper_left_y + 1;
        i32 col = floor_f32_to_int32((test_x - tiles->upper_left_x) / tiles->width);
        i32 row = floor_f32_to_int32((test_y - tiles->upper_left_y) / tiles->height);
        printf("Test 1: pos(%.1f, %.1f) -> tile[%d][%d] (expected [0][0]): %s\n",
               test_x, test_y, row, col,
               (row == 0 && col == 0) ? "PASS" : "FAIL");
    }

    // Test: Upper-left corner of tile [1][2]
    {
        f32 test_x = tiles->upper_left_x + 2 * tiles->width + 1;
        f32 test_y = tiles->upper_left_y + 1 * tiles->height + 1;
        i32 col = floor_f32_to_int32((test_x - tiles->upper_left_x) / tiles->width);
        i32 row = floor_f32_to_int32((test_y - tiles->upper_left_y) / tiles->height);
        printf("Test 2: pos(%.1f, %.1f) -> tile[%d][%d] (expected [1][2]): %s\n",
               test_x, test_y, row, col,
               (row == 1 && col == 2) ? "PASS" : "FAIL");
    }

    // Test: Last valid tile
    {
        f32 test_x = tiles->upper_left_x + (MAP_COL_COUNT - 1) * tiles->width + 1;
        f32 test_y = tiles->upper_left_y + (MAP_ROW_COUNT - 1) * tiles->height + 1;
        i32 col = floor_f32_to_int32((test_x - tiles->upper_left_x) / tiles->width);
        i32 row = floor_f32_to_int32((test_y - tiles->upper_left_y) / tiles->height);
        printf("Test 3: pos(%.1f, %.1f) -> tile[%d][%d] (expected [%d][%d]): %s\n",
               test_x, test_y, row, col,
               MAP_ROW_COUNT - 1, MAP_COL_COUNT - 1,
               (row == MAP_ROW_COUNT - 1 && col == MAP_COL_COUNT - 1) ? "PASS" : "FAIL");
    }

    printf("=============================\n");
}
```

### 5.5 Technique #5: Trace the Data Flow

When debugging, trace the ENTIRE data flow from input to output:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         DATA FLOW TRACE                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   1. INPUT: Player presses RIGHT arrow                                  │
│      └─> d_player_x = 1.0 * speed = 64.0                                │
│                                                                         │
│   2. NEW POSITION CALCULATION:                                          │
│      └─> new_x = 150 + (64.0 * 0.033) = 152.1                           │
│      └─> new_y = 90 (unchanged)                                         │
│                                                                         │
│   3. TILE COORDINATE CALCULATION:                                       │
│      └─> tile_col = floor((152.1 - (-30)) / 60) = floor(3.035) = 3      │
│      └─> tile_row = floor((90 - 0) / 60) = floor(1.5) = 1               │
│                                                                         │
│   4. BOUNDS CHECK:                                                      │
│      └─> tile_col (3) >= 0? YES                                         │
│      └─> tile_col (3) < 17? YES                                         │
│      └─> tile_row (1) >= 0? YES                                         │
│      └─> tile_row (1) < 9? YES                                          │
│      └─> All bounds OK!                                                 │
│                                                                         │
│   5. ARRAY ACCESS:                                                      │
│      └─> map[tile_row][tile_col] = map[1][3]                            │
│      └─> Value at map[1][3] = ???                                       │
│                                                                         │
│   6. VERIFY MAP DATA:                                                   │
│      Row 0: {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1}          │
│      Row 1: {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1}          │
│                     ↑                                                   │
│                  col 3 = 0 (walkable!)                                  │
│                                                                         │
│   7. RESULT: is_valid = (0 == 0) = true → Player can move               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Common Bug Patterns & How to Spot Them

### 6.1 Bug Pattern #1: Swapped Indices

**The Bug:**

```c
// Calculating tile coordinates correctly...
i32 tile_x = floor((player_x - offset_x) / tile_width);  // Column
i32 tile_y = floor((player_y - offset_y) / tile_height); // Row

// ...but accessing array incorrectly
i32 value = map[tile_x][tile_y];  // ❌ WRONG! Should be [tile_y][tile_x]
```

**How to Spot It:**

- Player can walk through some walls but not others
- Collision works in one direction but not another
- The bug appears "random" but is actually consistent in a rotated/mirrored way

**The Fix:**

```c
i32 value = map[tile_y][tile_x];  // ✅ [row][col] = [y][x]
```

**Prevention:**

```c
// Rename variables to match array semantics
i32 tile_col = floor((player_x - offset_x) / tile_width);
i32 tile_row = floor((player_y - offset_y) / tile_height);
i32 value = map[tile_row][tile_col];  // Now it's obvious!
```

---

### 6.2 Bug Pattern #2: Mismatched Bounds Check

**The Bug:**

```c
#define MAP_WIDTH 17   // Columns (X)
#define MAP_HEIGHT 9   // Rows (Y)
u32 map[MAP_HEIGHT][MAP_WIDTH];  // [9][17]

// Later...
if (tile_x >= 0 && tile_x < MAP_HEIGHT &&  // ❌ Using HEIGHT for X!
    tile_y >= 0 && tile_y < MAP_WIDTH) {   // ❌ Using WIDTH for Y!
    // ...
}
```

**How to Spot It:**

- Player can go out of bounds on one axis
- Crashes or weird behavior near map edges
- Works fine in the middle of the map

**The Fix:**

```c
if (tile_x >= 0 && tile_x < MAP_WIDTH &&   // ✅ X uses WIDTH (columns)
    tile_y >= 0 && tile_y < MAP_HEIGHT) {  // ✅ Y uses HEIGHT (rows)
    // ...
}
```

**Prevention:**

```c
// Use unambiguous names
#define MAP_COL_COUNT 17  // X direction
#define MAP_ROW_COUNT 9   // Y direction
u32 map[MAP_ROW_COUNT][MAP_COL_COUNT];

if (tile_col >= 0 && tile_col < MAP_COL_COUNT &&
    tile_row >= 0 && tile_row < MAP_ROW_COUNT) {
    // Now it's impossible to confuse!
}
```

---

### 6.3 Bug Pattern #3: Inconsistent Initialization vs Access

**The Bug:**

```c
// In init.c - initializing as [row][col]
u32 temp_map[9][17] = {
    {1, 1, 1, ...},  // Row 0
    {1, 0, 0, ...},  // Row 1
    // ...
};

// In main.h - declaring with swapped dimensions
#define MAP_X_COUNT 9
#define MAP_Y_COUNT 17
u32 map[MAP_X_COUNT][MAP_Y_COUNT];  // ❌ [9][17] but semantics are wrong!

// In main.c - accessing based on wrong mental model
value = map[tile_x][tile_y];  // Thinks first index is X
```

**How to Spot It:**

- Map appears rotated or mirrored
- Walls appear in wrong places
- The visual map doesn't match the data

**The Fix:**
Ensure ALL three locations use consistent semantics:

```c
// main.h
#define MAP_ROW_COUNT 9
#define MAP_COL_COUNT 17
u32 map[MAP_ROW_COUNT][MAP_COL_COUNT];  // [rows][cols]

// init.c
u32 temp_map[MAP_ROW_COUNT][MAP_COL_COUNT] = {
    {1, 1, 1, ...},  // Row 0 (top)
    {1, 0, 0, ...},  // Row 1
    // ...
};

// main.c
value = map[tile_row][tile_col];  // [row][col]
```

---

### 6.4 Bug Pattern #4: Truncation vs Floor for Coordinate Conversion

**Understanding the Difference:**

```
┌─────────────────────────────────────────────────────────────────────────┐
│              TRUNC vs FLOOR: WHEN TO USE EACH                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  NUMBER LINE VISUALIZATION:                                             │
│                                                                         │
│        -4      -3      -2      -1       0       1       2       3       │
│  ───────┼───────┼───────┼───────┼───────┼───────┼───────┼───────┼────  │
│                                                                         │
│  Value: -3.7                                                            │
│         ↓                                                               │
│  floor: -4  (toward -∞)                                                 │
│  trunc: -3  (toward 0)                                                  │
│                                                                         │
│  Value: +3.7                                                            │
│                                         ↓                               │
│  floor: +3  (toward -∞)  ← SAME!                                        │
│  trunc: +3  (toward 0)   ← SAME!                                        │
│                                                                         │
│  ═══════════════════════════════════════════════════════════════════   │
│  KEY INSIGHT: They only differ for NEGATIVE numbers!                    │
│  ═══════════════════════════════════════════════════════════════════   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**When It Matters - Tile Mapping with Negative Coordinates:**

```
┌─────────────────────────────────────────────────────────────────────────┐
│              TILE MAPPING WITH NEGATIVE COORDINATES                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Tile size = 32 pixels                                                  │
│  World origin (0,0) is at CENTER of map                                 │
│                                                                         │
│  Tile indices:    -2      -1       0       1       2                    │
│                ┌───────┬───────┬───────┬───────┬───────┐               │
│  Pixel range:  │-64→-33│-32→-1 │ 0→31  │32→63  │64→95  │               │
│                └───────┴───────┴───────┴───────┴───────┘               │
│                                                                         │
│  Test: pixel_x = -1 (should be tile -1)                                │
│    (int)(-1 / 32.0f) = (int)(-0.03125) = 0      ❌ WRONG!               │
│    (int)floorf(-1 / 32.0f) = (int)(-1.0) = -1   ✅ CORRECT!             │
│                                                                         │
│  Test: pixel_x = -33 (should be tile -2)                               │
│    (int)(-33 / 32.0f) = (int)(-1.03125) = -1    ❌ WRONG!               │
│    (int)floorf(-33 / 32.0f) = (int)(-2.0) = -2  ✅ CORRECT!             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**How to Spot It:**

- Player can enter tiles they shouldn't near the origin
- Collision breaks when player position goes negative
- Works fine for positive coordinates only

**The Decision Tree:**

```c
// CASE 1: Coordinates are ALWAYS non-negative (most tile games)
// → Use (int) cast or integer division — simpler, faster, same result
i32 tile_col = (i32)(pixel_x / tile_width);        // ✅ Fine for positive coords!
i32 tile_row = (i32)(pixel_y / tile_height);

// CASE 2: Coordinates CAN be negative (infinite world, centered origin)
// → Use floorf() to get consistent tile boundaries
i32 tile_col = (i32)floorf(pixel_x / tile_width);  // ✅ Correct for negatives
i32 tile_row = (i32)floorf(pixel_y / tile_height);
```

**The Fix for Handmade Hero:**

```c
// Early days: tile maps use non-negative coordinates
// Simple cast is appropriate and faster:
i32 tile_col = (i32)(relative_x / tile_width);
i32 tile_row = (i32)(relative_y / tile_height);

// Later: world coordinates may go negative
// Use floor for correct tile boundaries:
de100_file_scoped_fn inline i32 floor_f32_to_int32(f32 value) {
    return (i32)floorf(value);
}

i32 tile_col = floor_f32_to_int32(relative_x / tile_width);
i32 tile_row = floor_f32_to_int32(relative_y / tile_height);
```

**Summary Table:**

| Scenario                            | Use                         | Why                          |
| ----------------------------------- | --------------------------- | ---------------------------- |
| Coordinates always ≥ 0              | `(int)x` or `x / tile_size` | Faster, same result as floor |
| Coordinates can be negative         | `(int)floorf(x / size)`     | Correct tile boundaries      |
| Screen/pixel coordinates            | `(int)x`                    | Always positive              |
| World coordinates (centered origin) | `(int)floorf(x / size)`     | Handles negative quadrants   |

---

### 6.5 Bug Pattern #5: Off-by-One in Bounds

**The Bug:**

```c
#define MAP_COL_COUNT 17

// Using <= instead of <
if (tile_col >= 0 && tile_col <= MAP_COL_COUNT) {  // ❌ Allows col 17!
    value = map[tile_row][tile_col];  // Buffer overflow when col = 17!
}
```

**How to Spot It:**

- Crashes near right/bottom edge of map
- Valgrind reports buffer overflow
- Weird values read from memory

**The Fix:**

```c
if (tile_col >= 0 && tile_col < MAP_COL_COUNT) {  // ✅ Strictly less than
    value = map[tile_row][tile_col];
}
```

**Prevention:**

```c
// Use a helper function with built-in bounds checking
bool32 is_valid_tile(i32 row, i32 col) {
    return (row >= 0 && row < MAP_ROW_COUNT &&
            col >= 0 && col < MAP_COL_COUNT);
}

u32 get_tile_safe(TileState* tiles, i32 row, i32 col) {
    if (!is_valid_tile(row, col)) {
        return 1;  // Treat out-of-bounds as solid
    }
    return tiles->map[row][col];
}
```

---

### 6.6 Bug Pattern #6: Render Loop vs Collision Loop Mismatch

**The Bug:**

```c
// Rendering - correct order
for (u32 row = 0; row < MAP_ROW_COUNT; ++row) {
    for (u32 col = 0; col < MAP_COL_COUNT; ++col) {
        u32 tile_id = map[row][col];  // ✅ Correct
        // Draw at position based on row/col...
    }
}

// Collision - wrong order!
i32 tile_value = map[tile_x][tile_y];  // ❌ Swapped!
```

**How to Spot It:**

- Map LOOKS correct (walls in right places)
- But collision doesn't match what you see
- Player blocked by invisible walls, walks through visible walls

**The Fix:**
Ensure both use the same access pattern:

```c
// Rendering
u32 tile_id = map[row][col];

// Collision
u32 tile_value = map[tile_row][tile_col];  // Same pattern!
```

---

## 7. Defensive Programming Techniques

### 7.1 Technique #1: Encapsulate Array Access

Never access the map array directly. Always go through a function:

```c
// tile_map.h
typedef struct {
    u32 data[MAP_ROW_COUNT][MAP_COL_COUNT];
    f32 origin_x;
    f32 origin_y;
    f32 tile_width;
    f32 tile_height;
} TileMap;

// Get tile value with bounds checking
u32 tilemap_get(TileMap* map, i32 row, i32 col);

// Check if a tile is walkable
bool32 tilemap_is_walkable(TileMap* map, i32 row, i32 col);

// Convert screen position to tile coordinates
void tilemap_screen_to_tile(TileMap* map, f32 screen_x, f32 screen_y,
                            i32* out_row, i32* out_col);

// tile_map.c
u32 tilemap_get(TileMap* map, i32 row, i32 col) {
    DEV_ASSERT(row >= 0 && row < MAP_ROW_COUNT);
    DEV_ASSERT(col >= 0 && col < MAP_COL_COUNT);
    return map->data[row][col];
}

bool32 tilemap_is_walkable(TileMap* map, i32 row, i32 col) {
    if (row < 0 || row >= MAP_ROW_COUNT ||
        col < 0 || col >= MAP_COL_COUNT) {
        return false;  // Out of bounds = not walkable
    }
    return map->data[row][col] == 0;
}

void tilemap_screen_to_tile(TileMap* map, f32 screen_x, f32 screen_y,
                            i32* out_row, i32* out_col) {
    *out_col = (i32)floorf((screen_x - map->origin_x) / map->tile_width);
    *out_row = (i32)floorf((screen_y - map->origin_y) / map->tile_height);
}
```

**Usage:**

```c
// Now it's impossible to get wrong!
i32 row, col;
tilemap_screen_to_tile(&tile_map, player.x, player.y, &row, &col);

if (tilemap_is_walkable(&tile_map, row, col)) {
    // Move player
}
```

---

### 7.2 Technique #2: Use Structs for Coordinates

```c
typedef struct {
    i32 row;
    i32 col;
} TileCoord;

typedef struct {
    f32 x;
    f32 y;
} ScreenPos;

// Functions use these types - can't mix them up!
TileCoord screen_to_tile(ScreenPos pos, TileMap* map) {
    TileCoord result;
    result.col = (i32)floorf((pos.x - map->origin_x) / map->tile_width);
    result.row = (i32)floorf((pos.y - map->origin_y) / map->tile_height);
    return result;
}

u32 get_tile_at(TileMap* map, TileCoord coord) {
    // Can't accidentally swap row/col - they're in a struct!
    return map->data[coord.row][coord.col];
}

// Usage
ScreenPos player_pos = { player.x, player.y };
TileCoord player_tile = screen_to_tile(player_pos, &tile_map);
u32 tile_value = get_tile_at(&tile_map, player_tile);
```

---

### 7.3 Technique #3: Compile-Time Assertions

```c
// Ensure map dimensions match between declaration and initialization
#define MAP_ROW_COUNT 9
#define MAP_COL_COUNT 17

// In init.c
u32 temp_map[MAP_ROW_COUNT][MAP_COL_COUNT] = { ... };

// Compile-time check that dimensions match
_Static_assert(sizeof(temp_map) == sizeof(u32) * MAP_ROW_COUNT * MAP_COL_COUNT,
               "Map dimensions mismatch!");

// Also verify the struct matches
_Static_assert(sizeof(((TileState*)0)->map) == sizeof(temp_map),
               "TileState.map size doesn't match temp_map!");
```

---

### 7.4 Technique #4: Debug Visualization Mode

```c
#if DE100_INTERNAL
#define DEBUG_TILE_COLLISION 1
#endif

void handle_controls(...) {
    // ... calculate new position ...

    i32 tile_col = floor_div(new_x - offset_x, tile_width);
    i32 tile_row = floor_div(new_y - offset_y, tile_height);

#if DEBUG_TILE_COLLISION
    // Store for debug rendering
    g_debug.last_checked_row = tile_row;
    g_debug.last_checked_col = tile_col;
    g_debug.last_check_result = is_walkable;
#endif

    // ... rest of collision logic ...
}

void render_debug_overlay(GameBackBuffer* buffer, TileState* tiles) {
#if DEBUG_TILE_COLLISION
    // Highlight the last checked tile
    f32 min_x = tiles->origin_x + g_debug.last_checked_col * tiles->tile_width;
    f32 min_y = tiles->origin_y + g_debug.last_checked_row * tiles->tile_height;

    // Green = walkable, Red = blocked
    f32 r = g_debug.last_check_result ? 0.0f : 1.0f;
    f32 g = g_debug.last_check_result ? 1.0f : 0.0f;

    draw_rect_outline(buffer, min_x, min_y,
                      min_x + tiles->tile_width,
                      min_y + tiles->tile_height,
                      r, g, 0.0f, 1.0f);

    // Draw text showing coordinates
    char debug_text[64];
    snprintf(debug_text, sizeof(debug_text), "[%d][%d]=%s",
             g_debug.last_checked_row, g_debug.last_checked_col,
             g_debug.last_check_result ? "OK" : "BLOCKED");
    draw_debug_text(buffer, 10, 10, debug_text);
#endif
}
```

---

### 7.5 Technique #5: Unit Tests for Coordinate Math

```c
void run_tile_coordinate_tests(void) {
    TileMap test_map = {
        .origin_x = -30.0f,
        .origin_y = 0.0f,
        .tile_width = 60.0f,
        .tile_height = 60.0f
    };

    typedef struct {
        f32 screen_x, screen_y;
        i32 expected_row, expected_col;
    } TestCase;

    TestCase tests[] = {
        // Screen position -> Expected tile
        { -29.0f,   1.0f, 0, 0 },  // Just inside first tile
        { -30.0f,   0.0f, 0, 0 },  // Exactly at origin
        {  30.0f,   1.0f, 0, 1 },  // Second column
        {   1.0f,  60.0f, 1, 0 },  // Second row
        { 150.0f,  90.0f, 1, 3 },  // Middle of map
        { -31.0f,   1.0f, 0, -1 }, // Left of map (out of bounds)
        {   1.0f,  -1.0f, -1, 0 }, // Above map (out of bounds)
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    printf("Running %d tile coordinate tests...\n", num_tests);

    for (int i = 0; i < num_tests; i++) {
        TestCase* t = &tests[i];
        i32 row, col;
        tilemap_screen_to_tile(&test_map, t->screen_x, t->screen_y, &row, &col);

        bool32 pass = (row == t->expected_row && col == t->expected_col);

        printf("  Test %d: screen(%.1f, %.1f) -> [%d][%d] (expected [%d][%d]): %s\n",
               i + 1, t->screen_x, t->screen_y,
               row, col, t->expected_row, t->expected_col,
               pass ? "PASS" : "FAIL");

        if (pass) passed++;
    }

    printf("Results: %d/%d passed\n", passed, num_tests);
    DEV_ASSERT(passed == num_tests);
}
```

---

## 8. Exercises: Spot the Bug

### Exercise 8.1: The Invisible Wall

**Difficulty:** ⭐⭐☆☆☆  
**Time Estimate:** 5-10 minutes

The player can walk through some walls but gets blocked by empty space. Find the bug:

```c
#define TILE_ROWS 5
#define TILE_COLS 8
u32 map[TILE_ROWS][TILE_COLS] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1},
};

void check_collision(f32 player_x, f32 player_y) {
    i32 tile_x = (i32)(player_x / 32.0f);
    i32 tile_y = (i32)(player_y / 32.0f);

    if (tile_x >= 0 && tile_x < TILE_ROWS &&
        tile_y >= 0 && tile_y < TILE_COLS) {

        if (map[tile_x][tile_y] == 1) {
            printf("Blocked!\n");
        }
    }
}
```

<details>
<summary>💡 Hint 1 (Direction)</summary>

Look at the bounds check. What are you comparing `tile_x` and `tile_y` against?

</details>

<details>
<summary>💡 Hint 2 (Concept)</summary>

`tile_x` is calculated from `player_x` (horizontal position). Should it be compared against ROWS or COLS?

</details>

<details>
<summary>💡 Hint 3 (Specific)</summary>

There are TWO bugs:

1. Bounds check uses wrong constants
2. Array access has swapped indices

</details>

<details>
<summary>🔓 Complete Solution</summary>

**Bug 1:** Bounds check is backwards

```c
// ❌ Wrong
tile_x < TILE_ROWS  // X should use COLS
tile_y < TILE_COLS  // Y should use ROWS

// ✅ Correct
tile_x < TILE_COLS  // X = column = COLS
tile_y < TILE_ROWS  // Y = row = ROWS
```

**Bug 2:** Array access is backwards

```c
// ❌ Wrong
map[tile_x][tile_y]  // [col][row]

// ✅ Correct
map[tile_y][tile_x]  // [row][col]
```

**Fixed Code:**

```c
void check_collision(f32 player_x, f32 player_y) {
    i32 tile_col = (i32)(player_x / 32.0f);
    i32 tile_row = (i32)(player_y / 32.0f);

    if (tile_col >= 0 && tile_col < TILE_COLS &&
        tile_row >= 0 && tile_row < TILE_ROWS) {

        if (map[tile_row][tile_col] == 1) {
            printf("Blocked!\n");
        }
    }
}
```

</details>

---

### Exercise 8.2: The Teleporting Player

**Difficulty:** ⭐⭐⭐☆☆  
**Time Estimate:** 10-15 minutes

The player sometimes teleports when walking near the left edge of the map. Find the bug:

```c
#define MAP_OFFSET_X -64.0f
#define MAP_OFFSET_Y 0.0f
#define TILE_SIZE 32.0f

i32 get_tile_col(f32 screen_x) {
    f32 relative_x = screen_x - MAP_OFFSET_X;
    return (i32)(relative_x / TILE_SIZE);
}

void update_player(Player* player, f32 dx) {
    f32 new_x = player->x + dx;
    i32 tile_col = get_tile_col(new_x);

    // Check if tile is valid
    if (tile_col >= 0 && tile_col < MAP_COLS) {
        if (map[player->tile_row][tile_col] == 0) {
            player->x = new_x;
        }
    }
}
```

Test case that fails:

- Player at x = -60 (tile_col should be 0)
- Player moves left by 10 pixels to x = -70
- Expected: blocked (out of bounds)
- Actual: player position becomes weird

<details>
<summary>💡 Hint 1 (Direction)</summary>

What happens when `relative_x` is negative? Try calculating `get_tile_col(-70)` by hand.

</details>

<details>
<summary>💡 Hint 2 (Concept)</summary>

When you cast a negative float to int, C truncates toward zero, not toward negative infinity.

```
(i32)(-0.1875) = 0   // Truncates toward zero
floor(-0.1875) = -1    // Rounds toward negative infinity
```

</details>

<details>
<summary>💡 Hint 3 (Specific)</summary>

```
screen_x = -70
relative_x = -70 - (-64) = -6
-6 / 32 = -0.1875
(i32)(-0.1875) = 0  // BUG! Should be -1
```

The player at x=-70 is calculated as being in tile 0, but they're actually outside the map!

</details>

<details>
<summary>🔓 Complete Solution</summary>

**The Bug:** Using `(i32)` cast instead of `floor()` for negative numbers.

**Why It Matters:**

```
Position x = -70, offset = -64, tile_size = 32

relative_x = -70 - (-64) = -6
-6 / 32 = -0.1875

(i32)(-0.1875) = 0   ❌ Truncates toward zero
floorf(-0.1875) = -1   ✅ Correct tile index
```

**Fixed Code:**

```c
i32 get_tile_col(f32 screen_x) {
    f32 relative_x = screen_x - MAP_OFFSET_X;
    return (i32)floorf(relative_x / TILE_SIZE);  // Use floor for negative coords!
}
```

**General Rule:** Use `floorf()` when converting screen coordinates to tile indices if coordinates can be negative (world space with centered origin). For non-negative coordinates (screen pixels with top-left origin), a simple `(i32)` cast works fine and is faster.

</details>

---

### Exercise 8.3: The Mirrored Map

**Difficulty:** ⭐⭐⭐☆☆  
**Time Estimate:** 10-15 minutes

The map renders correctly, but collision detection seems mirrored. Walls on the left block movement on the right, and vice versa. Find the bug:

```c
// main.h
#define MAP_WIDTH 10
#define MAP_HEIGHT 8
typedef struct {
    u32 tiles[MAP_WIDTH][MAP_HEIGHT];
} TileMap;

// init.c
void init_map(TileMap* map) {
    u32 data[8][10] = {
        {1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,0,0,0,0,1},
        {1,0,1,1,0,0,0,0,0,1},
        {1,0,0,0,0,0,1,1,0,1},
        {1,0,0,0,0,0,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1},
    };
    memcpy(map->tiles, data, sizeof(data));
}

// main.c - Rendering (works correctly)
for (int row = 0; row < MAP_HEIGHT; row++) {
    for (int col = 0; col < MAP_WIDTH; col++) {
        u32 tile = map->tiles[col][row];  // Note the order
        draw_tile(col * TILE_SIZE, row * TILE_SIZE, tile);
    }
}

// main.c - Collision (broken)
i32 check_col = (i32)(player_x / TILE_SIZE);
i32 check_row = (i32)(player_y / TILE_SIZE);
if (map->tiles[check_row][check_col] == 1) {
    // Block movement
}
```

<details>
<summary>💡 Hint 1 (Direction)</summary>

Compare how the array is declared in the struct vs how the data is initialized.

</details>

<details>
<summary>💡 Hint 2 (Concept)</summary>

The struct declares `tiles[MAP_WIDTH][MAP_HEIGHT]` which is `[10][8]`.
The initialization data is `data[8][10]`.

These have the same total size but different shapes!

</details>

<details>
<summary>💡 Hint 3 (Specific)</summary>

There are THREE places with inconsistencies:

1. Struct declaration: `tiles[WIDTH][HEIGHT]` = `[10][8]`
2. Init data: `data[8][10]` = `[HEIGHT][WIDTH]`
3. Render access: `tiles[col][row]`
4. Collision access: `tiles[row][col]`

The render and collision use DIFFERENT access patterns!

</details>

<details>
<summary>🔓 Complete Solution</summary>

**The Bugs:**

1. **Struct declaration is backwards:**

```c
// ❌ Wrong - [width][height] = [cols][rows]
u32 tiles[MAP_WIDTH][MAP_HEIGHT];

// ✅ Correct - [height][width] = [rows][cols]
u32 tiles[MAP_HEIGHT][MAP_WIDTH];
```

2. **Render loop uses wrong access pattern:**

```c
// ❌ Wrong - [col][row]
u32 tile = map->tiles[col][row];

// ✅ Correct - [row][col]
u32 tile = map->tiles[row][col];
```

3. **Collision happens to be correct but for wrong reasons!**

**Fixed Code:**

```c
// main.h
#define MAP_COLS 10
#define MAP_ROWS 8
typedef struct {
    u32 tiles[MAP_ROWS][MAP_COLS];  // [rows][cols]
} TileMap;

// init.c
void init_map(TileMap* map) {
    u32 data[MAP_ROWS][MAP_COLS] = {
        {1,1,1,1,1,1,1,1,1,1},  // Row 0
        // ...
    };
    memcpy(map->tiles, data, sizeof(data));
}

// main.c - Rendering
for (int row = 0; row < MAP_ROWS; row++) {
    for (int col = 0; col < MAP_COLS; col++) {
        u32 tile = map->tiles[row][col];  // [row][col]
        draw_tile(col * TILE_SIZE, row * TILE_SIZE, tile);
    }
}

// main.c - Collision
i32 check_col = (i32)(player_x / TILE_SIZE);
i32 check_row = (i32)(player_y / TILE_SIZE);
if (map->tiles[check_row][check_col] == 1) {  // [row][col]
    // Block movement
}
```

**Key Lesson:** Use consistent naming (ROWS/COLS not WIDTH/HEIGHT) and consistent access patterns EVERYWHERE.

</details>

---

### Exercise 8.4: The Edge Case Crash

**Difficulty:** ⭐⭐⭐⭐☆  
**Time Estimate:** 15-20 minutes

The game crashes when the player reaches the bottom-right corner of the map. Find the bug:

```c
#define MAP_ROWS 9
#define MAP_COLS 17
u32 map[MAP_ROWS][MAP_COLS];

bool32 can_move_to(f32 x, f32 y) {
    i32 col = (i32)floorf(x / TILE_SIZE);
    i32 row = (i32)floorf(y / TILE_SIZE);

    // Bounds check
    if (col < 0 || col > MAP_COLS ||
        row < 0 || row > MAP_ROWS) {
        return false;
    }

    return map[row][col] == 0;
}
```

The crash happens when:

- Player is at position (1019, 539) with TILE_SIZE = 60
- Map is 17 columns × 9 rows

<details>
<summary>💡 Hint 1 (Direction)</summary>

Calculate the tile coordinates for position (1019, 539):

- col = floor(1019 / 60) = ?
- row = floor(539 / 60) = ?

</details>

<details>
<summary>💡 Hint 2 (Concept)</summary>

```
col = floor(1019 / 60) = floor(16.98) = 16
row = floor(539 / 60) = floor(8.98) = 8
```

Now check: is `col > MAP_COLS` true? Is `row > MAP_ROWS` true?

</details>

<details>
<summary>💡 Hint 3 (Specific)</summary>

The bounds check uses `>` instead of `>=`:

```c
col > MAP_COLS  // False when col = 16, MAP_COLS = 17
row > MAP_ROWS  // False when row = 8, MAP_ROWS = 9
```

But valid indices are 0 to 16 (for cols) and 0 to 8 (for rows).
When col = 16 and row = 8, the check passes, but...

Wait, that's actually valid! Let me recalculate...

Actually, the bug is `>` should be `>=`:

- `col > 17` is false when col = 17
- But col = 17 is OUT OF BOUNDS
</details>

<details>
<summary>🔓 Complete Solution</summary>

**The Bug:** Using `>` instead of `>=` in bounds check.

**Analysis:**

```
Position: (1019, 539), TILE_SIZE = 60

col = floor(1019 / 60) = floor(16.98) = 16  ✅ Valid (0-16)
row = floor(539 / 60) = floor(8.98) = 8     ✅ Valid (0-8)
```

Wait, this should work! Let me reconsider...

Actually, the crash happens at the EXACT edge. Let's try position (1020, 540):

```
col = floor(1020 / 60) = floor(17.0) = 17   ❌ Invalid! (max is 16)
row = floor(540 / 60) = floor(9.0) = 9      ❌ Invalid! (max is 8)
```

Now check the bounds:

```c
col > MAP_COLS  →  17 > 17  →  false  // BUG! Should reject 17
row > MAP_ROWS  →  9 > 9    →  false  // BUG! Should reject 9
```

The check passes, then `map[9][17]` causes a buffer overflow!

**Fixed Code:**

```c
bool32 can_move_to(f32 x, f32 y) {
    i32 col = (i32)floorf(x / TILE_SIZE);
    i32 row = (i32)floorf(y / TILE_SIZE);

    // Bounds check - use >= not >
    if (col < 0 || col >= MAP_COLS ||    // ✅ >= instead of >
        row < 0 || row >= MAP_ROWS) {    // ✅ >= instead of >
        return false;
    }

    return map[row][col] == 0;
}
```

**Memory Diagram:**

```
Valid indices:     [0] [1] [2] ... [7] [8]    (MAP_ROWS = 9)
                    ↑                   ↑
                  first              last valid

Index 9 is OUT OF BOUNDS but passes "row > 9" check!
```

**Key Lesson:** Array bounds checks should ALWAYS use `< COUNT` or `>= COUNT`, never `<= COUNT` or `> COUNT`.

</details>

---

### Exercise 8.5: The Phantom Collision

**Difficulty:** ⭐⭐⭐⭐☆  
**Time Estimate:** 15-20 minutes

The player gets blocked in the middle of empty tiles. The blocking seems to happen at regular intervals. Find the bug:

```c
#define TILE_SIZE 64

typedef struct {
    f32 x, y;           // Center position
    f32 width, height;  // Player is 48x48
} Player;

bool32 is_blocked(Player* p, TileMap* map) {
    // Check all four corners of the player
    f32 left = p->x - p->width / 2;
    f32 right = p->x + p->width / 2;
    f32 top = p->y - p->height / 2;
    f32 bottom = p->y + p->height / 2;

    // Convert to tile coordinates
    i32 left_col = (i32)(left / TILE_SIZE);
    i32 right_col = (i32)(right / TILE_SIZE);
    i32 top_row = (i32)(top / TILE_SIZE);
    i32 bottom_row = (i32)(bottom / TILE_SIZE);

    // Check each corner
    if (get_tile(map, top_row, left_col) == 1) return true;
    if (get_tile(map, top_row, right_col) == 1) return true;
    if (get_tile(map, bottom_row, left_col) == 1) return true;
    if (get_tile(map, bottom_row, right_col) == 1) return true;

    return false;
}
```

The player (48x48) gets blocked when their center is at positions like (96, 96), (160, 160), etc.

<details>
<summary>💡 Hint 1 (Direction)</summary>

Calculate the corner positions when the player center is at (96, 96):

- left = 96 - 24 = 72
- right = 96 + 24 = 120
- top = 96 - 24 = 72
- bottom = 96 + 24 = 120

Now convert to tile coordinates...

</details>

<details>
<summary>💡 Hint 2 (Concept)</summary>

```
left_col = (i32)(72 / 64) = (i32)(1.125) = 1
right_col = (i32)(120 / 64) = (i32)(1.875) = 1
top_row = (i32)(72 / 64) = 1
bottom_row = (i32)(120 / 64) = 1
```

All corners are in tile [1][1]. But what if the player is at (128, 128)?

```
left = 128 - 24 = 104   → col = (i32)(104/64) = 1
right = 128 + 24 = 152  → col = (i32)(152/64) = 2
```

The player spans TWO tiles horizontally! But wait, that's expected...

The bug is elsewhere. What about negative positions?

</details>

<details>
<summary>💡 Hint 3 (Specific)</summary>

The bug is using `(i32)` cast instead of `floor()`.

When player is at (32, 32):

```
left = 32 - 24 = 8     → (i32)(8/64) = 0    ✅
right = 32 + 24 = 56   → (i32)(56/64) = 0   ✅
```

When player is at (24, 24):

```
left = 24 - 24 = 0     → (i32)(0/64) = 0    ✅
right = 24 + 24 = 48   → (i32)(48/64) = 0   ✅
```

When player is at (20, 20):

```
left = 20 - 24 = -4    → (i32)(-4/64) = (i32)(-0.0625) = 0  ❌
                          Should be -1!
```

The player's left edge is in tile -1 (off map), but it's calculated as tile 0!

</details>

<details>
<summary>🔓 Complete Solution</summary>

**The Bug:** Using `(i32)` cast instead of `floorf()` for tile coordinate conversion.

**The Problem:**

```c
i32 left_col = (i32)(left / TILE_SIZE);
```

When `left` is negative (player near left edge):

```
left = -4
-4 / 64 = -0.0625
(i32)(-0.0625) = 0    ❌ Truncates toward zero!
floorf(-0.0625) = -1    ✅ Correct tile index
```

This causes the player to be checked against tile 0 when they're actually outside the map, leading to phantom collisions if tile 0 happens to be solid.

**But wait - the question says blocking happens at (96, 96), (160, 160)...**

Let me reconsider. These are positions where `right` or `bottom` lands exactly on a tile boundary:

```
Player at (96, 96):
right = 96 + 24 = 120
120 / 64 = 1.875 → col 1

Player at (128, 128):
right = 128 + 24 = 152
152 / 64 = 2.375 → col 2

Player at (160, 160):
right = 160 + 24 = 184
184 / 64 = 2.875 → col 2
```

Hmm, that's not showing the pattern either. Let me look at exact tile boundaries:

```
Tile boundaries: 0, 64, 128, 192, 256...

Player at center (64, 64):
right = 64 + 24 = 88 → col 1
bottom = 64 + 24 = 88 → row 1

Player at center (128, 128):
right = 128 + 24 = 152 → col 2
bottom = 128 + 24 = 152 → row 2
```

**Actually, the real bug might be checking the exact boundary pixel:**

When `right = 128` exactly:

```
right_col = (i32)(128 / 64) = 2
```

The player's rightmost pixel is AT x=128, which is the START of tile 2. But the player doesn't actually occupy tile 2 - they occupy up to but not including x=128.

**Fixed Code:**

```c
bool32 is_blocked(Player* p, TileMap* map) {
    f32 left = p->x - p->width / 2;
    f32 right = p->x + p->width / 2 - 0.001f;  // Subtract epsilon
    f32 top = p->y - p->height / 2;
    f32 bottom = p->y + p->height / 2 - 0.001f;  // Subtract epsilon

    // Use floor for negative coordinate safety
    i32 left_col = (i32)floorf(left / TILE_SIZE);
    i32 right_col = (i32)floorf(right / TILE_SIZE);
    i32 top_row = (i32)floorf(top / TILE_SIZE);
    i32 bottom_row = (i32)floorf(bottom / TILE_SIZE);

    // ... rest of function
}
```

**Alternative Fix (cleaner):**

```c
// Define player bounds as [min, max) - max is exclusive
f32 right = p->x + p->width / 2;  // This pixel is NOT part of player

// When checking tiles, the rightmost tile is:
i32 right_col = (i32)floorf((right - 0.001f) / TILE_SIZE);
// Or equivalently, if right is exactly on boundary, use previous tile
```

**Key Lesson:** Be explicit about whether bounds are inclusive or exclusive, and handle tile boundaries consistently.

</details>

---

## 9. Exercises: Fix the Code

### Exercise 9.1: Complete the Helper Function

**Difficulty:** ⭐⭐☆☆☆  
**Time Estimate:** 10-15 minutes

Complete this helper function to safely get a tile value:

```c
#define MAP_ROWS 9
#define MAP_COLS 17
#define TILE_SOLID 1
#define TILE_EMPTY 0

typedef struct {
    u32 data[MAP_ROWS][MAP_COLS];
    f32 origin_x;
    f32 origin_y;
    f32 tile_size;
} TileMap;

// TODO: Complete this function
// - Convert screen position to tile coordinates
// - Return TILE_SOLID if out of bounds
// - Return the tile value if in bounds
u32 get_tile_at_position(TileMap* map, f32 screen_x, f32 screen_y) {
    // Your code here
}
```

<details>
<summary>💡 Hint 1</summary>

First calculate the relative position by subtracting the origin.

</details>

<details>
<summary>💡 Hint 2</summary>

For coordinate-to-tile conversion: use `(i32)` cast if coordinates are always non-negative, or `floorf()` if they can be negative (handles negative coordinates correctly by rounding toward -∞).

</details>

<details>
<summary>💡 Hint 3</summary>

Check bounds BEFORE accessing the array. Remember: `row < MAP_ROWS` and `col < MAP_COLS`.

</details>

<details>
<summary>🔓 Complete Solution</summary>

```c
u32 get_tile_at_position(TileMap* map, f32 screen_x, f32 screen_y) {
    // Step 1: Calculate relative position
    f32 relative_x = screen_x - map->origin_x;
    f32 relative_y = screen_y - map->origin_y;

    // Step 2: Convert to tile coordinates using floor (handles negatives)
    i32 col = (i32)floorf(relative_x / map->tile_size);
    i32 row = (i32)floorf(relative_y / map->tile_size);

    // Step 3: Bounds check - treat out of bounds as solid
    if (col < 0 || col >= MAP_COLS ||
        row < 0 || row >= MAP_ROWS) {
        return TILE_SOLID;
    }

    // Step 4: Return tile value (note: [row][col] order!)
    return map->data[row][col];
}
```

**Key Points:**

1. `floorf()` ensures correct behavior for negative coordinates
2. Bounds check uses `>=` not `>`
3. Array access is `[row][col]` not `[col][row]`
4. Out of bounds returns SOLID (safe default for collision)

</details>

---

### Exercise 9.2: Fix the Rendering Loop

**Difficulty:** ⭐⭐⭐☆☆  
**Time Estimate:** 10-15 minutes

This rendering code draws the map incorrectly (rotated 90 degrees). Fix it:

```c
#define MAP_ROWS 5
#define MAP_COLS 8
#define TILE_SIZE 32

u32 map[MAP_ROWS][MAP_COLS] = {
    {1,1,1,1,1,1,1,1},  // Row 0 - should be TOP
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1},  // Row 4 - should be BOTTOM
};

void render_map(void) {
    for (int x = 0; x < MAP_COLS; x++) {
        for (int y = 0; y < MAP_ROWS; y++) {
            u32 tile = map[x][y];

            f32 screen_x = x * TILE_SIZE;
            f32 screen_y = y * TILE_SIZE;

            draw_tile(screen_x, screen_y, tile);
        }
    }
}
```

<details>
<summary>💡 Hint 1</summary>

The loop variables are named `x` and `y`, but what do they actually iterate over?

</details>

<details>
<summary>💡 Hint 2</summary>

`x` goes from 0 to MAP_COLS, so it's iterating over columns.
`y` goes from 0 to MAP_ROWS, so it's iterating over rows.

But the array access is `map[x][y]`... is that correct?

</details>

<details>
<summary>🔓 Complete Solution</summary>

**The Bug:** Loop variables named misleadingly, and array access is backwards.

**Fixed Code (Option 1 - Rename variables):**

```c
void render_map(void) {
    for (int col = 0; col < MAP_COLS; col++) {
        for (int row = 0; row < MAP_ROWS; row++) {
            u32 tile = map[row][col];  // [row][col] order

            f32 screen_x = col * TILE_SIZE;  // col determines X
            f32 screen_y = row * TILE_SIZE;  // row determines Y

            draw_tile(screen_x, screen_y, tile);
        }
    }
}
```

**Fixed Code (Option 2 - Row-major iteration, more cache-friendly):**

```c
void render_map(void) {
    for (int row = 0; row < MAP_ROWS; row++) {
        for (int col = 0; col < MAP_COLS; col++) {
            u32 tile = map[row][col];

            f32 screen_x = col * TILE_SIZE;
            f32 screen_y = row * TILE_SIZE;

            draw_tile(screen_x, screen_y, tile);
        }
    }
}
```

**Why Option 2 is Better:**

- Iterates in row-major order (how C stores arrays)
- Better cache performance (sequential memory access)
- More natural: "for each row, for each column in that row"

</details>

---

### Exercise 9.3: Fix the Collision System

**Difficulty:** ⭐⭐⭐⭐☆  
**Time Estimate:** 20-25 minutes

This collision system has multiple bugs. Find and fix all of them:

```c
#define WORLD_ROWS 10
#define WORLD_COLS 15
#define TILE_WIDTH 50
#define TILE_HEIGHT 50

typedef struct {
    u32 tiles[WORLD_COLS][WORLD_ROWS];  // Bug #1 is here
    int origin_x, origin_y;
} World;

typedef struct {
    float x, y;
    float radius;
} Player;

int is_position_valid(World* world, float x, float y) {
    // Convert to tile space
    int tile_x = (int)((x - world->origin_x) / TILE_WIDTH);
    int tile_y = (int)((y - world->origin_y) / TILE_HEIGHT);

    // Bounds check
    if (tile_x < 0 || tile_x > WORLD_COLS) return 0;  // Bug #2
    if (tile_y < 0 || tile_y > WORLD_ROWS) return 0;  // Bug #3

    // Check tile
    return world->tiles[tile_x][tile_y] == 0;  // Bug #4
}

void move_player(World* world, Player* player, float dx, float dy) {
    float new_x = player->x + dx;
    float new_y = player->y + dy;

    // Only check center point (Bug #5 - should check multiple points)
    if (is_position_valid(world, new_x, new_y)) {
        player->x = new_x;
        player->y = new_y;
    }
}
```

<details>
<summary>💡 Hint 1 (Bug #1)</summary>

Look at the array declaration. What should the dimensions be for `[ROWS][COLS]`?

</details>

<details>
<summary>💡 Hint 2 (Bugs #2 and #3)</summary>

The bounds check uses `>` instead of `>=`. What happens when `tile_x == WORLD_COLS`?

</details>

<details>
<summary>💡 Hint 3 (Bug #4)</summary>

The array access uses `[tile_x][tile_y]`. Given that tile_x is the column and tile_y is the row, is this correct?

</details>

<details>
<summary>💡 Hint 4 (Bug #5)</summary>

The player has a radius, but only the center point is checked. What if the player's edge overlaps a wall?

</details>

<details>
<summary>🔓 Complete Solution</summary>

**All 5 Bugs Fixed:**

```c
#define WORLD_ROWS 10
#define WORLD_COLS 15
#define TILE_WIDTH 50
#define TILE_HEIGHT 50

typedef struct {
    // Bug #1 FIXED: [ROWS][COLS] not [COLS][ROWS]
    u32 tiles[WORLD_ROWS][WORLD_COLS];
    int origin_x, origin_y;
} World;

typedef struct {
    float x, y;
    float radius;
} Player;

int is_position_valid(World* world, float x, float y) {
    // Convert to tile space - use floor for negative coordinates
    int tile_col = (int)floorf((x - world->origin_x) / TILE_WIDTH);
    int tile_row = (int)floorf((y - world->origin_y) / TILE_HEIGHT);

    // Bug #2 and #3 FIXED: Use >= not >
    if (tile_col < 0 || tile_col >= WORLD_COLS) return 0;
    if (tile_row < 0 || tile_row >= WORLD_ROWS) return 0;

    // Bug #4 FIXED: [row][col] not [col][row]
    return world->tiles[tile_row][tile_col] == 0;
}

// Bug #5 FIXED: Check multiple points around the player
int is_player_position_valid(World* world, Player* player, float x, float y) {
    // Check center
    if (!is_position_valid(world, x, y)) return 0;

    // Check four cardinal points at radius distance
    if (!is_position_valid(world, x - player->radius, y)) return 0;  // Left
    if (!is_position_valid(world, x + player->radius, y)) return 0;  // Right
    if (!is_position_valid(world, x, y - player->radius)) return 0;  // Top
    if (!is_position_valid(world, x, y + player->radius)) return 0;  // Bottom

    // Optionally check diagonal corners too
    float diag = player->radius * 0.707f;  // radius * cos(45°)
    if (!is_position_valid(world, x - diag, y - diag)) return 0;
    if (!is_position_valid(world, x + diag, y - diag)) return 0;
    if (!is_position_valid(world, x - diag, y + diag)) return 0;
    if (!is_position_valid(world, x + diag, y + diag)) return 0;

    return 1;
}

void move_player(World* world, Player* player, float dx, float dy) {
    float new_x = player->x + dx;
    float new_y = player->y + dy;

    if (is_player_position_valid(world, player, new_x, new_y)) {
        player->x = new_x;
        player->y = new_y;
    }
}
```

**Summary of Bugs:**
| Bug | Location | Problem | Fix |
|-----|----------|---------|-----|
| #1 | Array declaration | `[COLS][ROWS]` | `[ROWS][COLS]` |
| #2 | Bounds check | `> WORLD_COLS` | `>= WORLD_COLS` |
| #3 | Bounds check | `> WORLD_ROWS` | `>= WORLD_ROWS` |
| #4 | Array access | `[tile_x][tile_y]` | `[tile_row][tile_col]` |
| #5 | Collision check | Only center point | Multiple points around radius |

</details>

---

## 10. Exercises: Design From Scratch

### Exercise 10.1: Design a Tile Coordinate System

**Difficulty:** ⭐⭐⭐☆☆  
**Time Estimate:** 20-30 minutes

Design a complete tile coordinate system from scratch. Your design should:

1. Define clear types for screen coordinates and tile coordinates
2. Provide conversion functions between them
3. Handle negative coordinates correctly
4. Include bounds checking
5. Be impossible to accidentally swap row/col

**Requirements:**

```c
// Your design should support these operations:

// Convert screen position to tile coordinate
TileCoord screen_to_tile(ScreenPos pos, TileMap* map);

// Check if a tile coordinate is within the map
bool32 is_tile_in_bounds(TileCoord coord, TileMap* map);

// Get the tile value at a coordinate (returns SOLID if out of bounds)
u32 get_tile(TileMap* map, TileCoord coord);

// Check if a screen position is walkable
bool32 is_walkable(ScreenPos pos, TileMap* map);
```

<details>
<summary>💡 Hint 1 (Types)</summary>

Create separate struct types for screen and tile coordinates. This prevents accidentally passing one where the other is expected.

</details>

<details>
<summary>💡 Hint 2 (Naming)</summary>

Use `row` and `col` in your TileCoord struct, not `x` and `y`. This makes the array access obvious.

</details>

<details>
<summary>💡 Hint 3 (Conversion)</summary>

The conversion function should:

1. Subtract the map origin
2. Divide by tile size
3. Use `floorf()` if coordinates can be negative, or `(i32)` cast if always non-negative

</details>

<details>
<summary>🔓 Complete Solution</summary>

```c
// ═══════════════════════════════════════════════════════════════════════════
// TYPES
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    f32 x;
    f32 y;
} ScreenPos;

typedef struct {
    i32 row;  // Y direction (vertical)
    i32 col;  // X direction (horizontal)
} TileCoord;

#define MAP_ROW_COUNT 9
#define MAP_COL_COUNT 17
#define TILE_SOLID 1
#define TILE_EMPTY 0

typedef struct {
    // Array is [row][col] - comment makes it explicit
    u32 data[MAP_ROW_COUNT][MAP_COL_COUNT];

    // Screen position of tile [0][0]'s top-left corner
    f32 origin_x;
    f32 origin_y;

    // Size of each tile in pixels
    f32 tile_width;
    f32 tile_height;
} TileMap;

// ═══════════════════════════════════════════════════════════════════════════
// CONVERSION FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

TileCoord screen_to_tile(ScreenPos pos, TileMap* map) {
    TileCoord result;

    // Calculate position relative to map origin
    f32 relative_x = pos.x - map->origin_x;
    f32 relative_y = pos.y - map->origin_y;

    // Convert to tile indices using floor (handles negatives correctly)
    result.col = (i32)floorf(relative_x / map->tile_width);
    result.row = (i32)floorf(relative_y / map->tile_height);

    return result;
}

ScreenPos tile_to_screen(TileCoord coord, TileMap* map) {
    ScreenPos result;

    // Calculate top-left corner of the tile in screen space
    result.x = map->origin_x + (f32)coord.col * map->tile_width;
    result.y = map->origin_y + (f32)coord.row * map->tile_height;

    return result;
}

ScreenPos tile_center_to_screen(TileCoord coord, TileMap* map) {
    ScreenPos result;

    result.x = map->origin_x + ((f32)coord.col + 0.5f) * map->tile_width;
    result.y = map->origin_y + ((f32)coord.row + 0.5f) * map->tile_height;

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// BOUNDS CHECKING
// ═══════════════════════════════════════════════════════════════════════════

bool32 is_tile_in_bounds(TileCoord coord, TileMap* map) {
    return (coord.row >= 0 && coord.row < MAP_ROW_COUNT &&
            coord.col >= 0 && coord.col < MAP_COL_COUNT);
}

// ═══════════════════════════════════════════════════════════════════════════
// TILE ACCESS
// ═══════════════════════════════════════════════════════════════════════════

u32 get_tile(TileMap* map, TileCoord coord) {
    // Out of bounds = solid (safe default)
    if (!is_tile_in_bounds(coord, map)) {
        return TILE_SOLID;
    }

    // Access array with [row][col] - matches struct field order!
    return map->data[coord.row][coord.col];
}

void set_tile(TileMap* map, TileCoord coord, u32 value) {
    if (!is_tile_in_bounds(coord, map)) {
        return;  // Silently ignore out-of-bounds writes
    }

    map->data[coord.row][coord.col] = value;
}

// ═══════════════════════════════════════════════════════════════════════════
// HIGH-LEVEL QUERIES
// ═══════════════════════════════════════════════════════════════════════════

bool32 is_walkable(ScreenPos pos, TileMap* map) {
    TileCoord coord = screen_to_tile(pos, map);
    u32 tile = get_tile(map, coord);
    return (tile == TILE_EMPTY);
}

bool32 is_rect_walkable(ScreenPos top_left, ScreenPos bottom_right, TileMap* map) {
    // Get tile range that the rect covers
    TileCoord min_tile = screen_to_tile(top_left, map);

    // Subtract small epsilon from bottom_right to handle exact boundaries
    ScreenPos adjusted_br = {
        bottom_right.x - 0.001f,
        bottom_right.y - 0.001f
    };
    TileCoord max_tile = screen_to_tile(adjusted_br, map);

    // Check all tiles in the range
    for (i32 row = min_tile.row; row <= max_tile.row; row++) {
        for (i32 col = min_tile.col; col <= max_tile.col; col++) {
            TileCoord coord = { row, col };
            if (get_tile(map, coord) != TILE_EMPTY) {
                return false;
            }
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void debug_print_tile_coord(TileCoord coord) {
    printf("TileCoord { row: %d, col: %d }\n", coord.row, coord.col);
}

void debug_print_screen_pos(ScreenPos pos) {
    printf("ScreenPos { x: %.2f, y: %.2f }\n", pos.x, pos.y);
}

void debug_tile_at_screen(ScreenPos pos, TileMap* map) {
    TileCoord coord = screen_to_tile(pos, map);
    u32 tile = get_tile(map, coord);

    printf("Screen (%.1f, %.1f) -> Tile [row=%d, col=%d] = %u (%s)\n",
           pos.x, pos.y, coord.row, coord.col, tile,
           tile == TILE_EMPTY ? "empty" : "solid");
}
```

**Why This Design Works:**

1. **Type Safety:** `ScreenPos` and `TileCoord` are different types - can't accidentally mix them
2. **Clear Naming:** `row` and `col` in TileCoord match array access order
3. **Encapsulation:** All array access goes through `get_tile()` which handles bounds
4. **Negative Handling:** `floorf()` used in conversion
5. **Self-Documenting:** Function names describe exactly what they do

</details>

---

## 11. Summary: The Golden Rules

After all this analysis, here are the **absolute rules** to follow for 2D tile-based collision:

### 11.1 The Five Golden Rules

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     THE FIVE GOLDEN RULES                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  RULE 1: Arrays are [ROW][COL], always                                 │
│  ─────────────────────────────────────                                  │
│  u32 map[ROW_COUNT][COL_COUNT];                                     │
│  value = map[row][col];                                                │
│                                                                         │
│  RULE 2: ROW = Y, COL = X                                              │
│  ────────────────────────                                               │
│  row = f(screen_y)                                                     │
│  col = f(screen_x)                                                     │
│                                                                         │
│  RULE 3: Use floor() for negative coords, cast for non-negative        │
│  ───────────────────────────────────────────────────────────            │
│  // If coords can be negative (world space):                           │
│  col = (i32)floorf(relative_x / tile_width);                         │
│  // If coords always non-negative (screen pixels):                     │
│  col = (i32)(relative_x / tile_width);  // Faster!                   │
│                                                                         │
│  RULE 4: Bounds check with < COUNT, not <= COUNT                       │
│  ───────────────────────────────────────────────                        │
│  if (row >= 0 && row < ROW_COUNT &&                                    │
│      col >= 0 && col < COL_COUNT)                                      │
│                                                                         │
│  RULE 5: Encapsulate array access                                      │
│  ────────────────────────────────                                       │
│  u32 get_tile(TileMap* map, i32 row, i32 col);                  │
│  // Never access map->data directly outside this function              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 11.2 The Naming Convention

**Always use these names consistently:**

| Concept                    | Name                    | Description       |
| -------------------------- | ----------------------- | ----------------- |
| Horizontal screen position | `x`, `screen_x`         | Pixels from left  |
| Vertical screen position   | `y`, `screen_y`         | Pixels from top   |
| Horizontal tile index      | `col`, `tile_col`       | Column in array   |
| Vertical tile index        | `row`, `tile_row`       | Row in array      |
| Array horizontal size      | `COL_COUNT`, `MAP_COLS` | Number of columns |
| Array vertical size        | `ROW_COUNT`, `MAP_ROWS` | Number of rows    |

**Never use:**

- `tile_x` / `tile_y` (ambiguous - is x the row or column?)
- `MAP_WIDTH` / `MAP_HEIGHT` (confusing with pixel dimensions)
- `w` / `h` for tile counts

### 11.3 The Checklist

Before committing any tile-related code, verify:

```
□ Array declaration is [ROW_COUNT][COL_COUNT]
□ Initialization data matches declaration dimensions
□ All array accesses are [row][col] order
□ Screen-to-tile conversion uses floorf() for negative coords or (int) for non-negative
□ Bounds checks use < COUNT (not <= COUNT)
□ Row is calculated from Y, col from X
□ Render loop and collision use same access pattern
□ Out-of-bounds returns safe default (solid)
□ Variable names clearly indicate row vs col
□ No direct array access outside helper functions
```

### 11.4 Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TILE COLLISION QUICK REFERENCE                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  DECLARATION:                                                           │
│    #define MAP_ROWS 9                                                  │
│    #define MAP_COLS 17                                                 │
│    u32 map[MAP_ROWS][MAP_COLS];                                     │
│                                                                         │
│  SCREEN TO TILE:                                                        │
│    // If coords can be negative:                                       │
│    i32 col = (i32)floorf((screen_x - origin_x) / tile_width);      │
│    i32 row = (i32)floorf((screen_y - origin_y) / tile_height);     │
│    // If coords always non-negative:                                   │
│    i32 col = (i32)((screen_x - origin_x) / tile_width);            │
│    i32 row = (i32)((screen_y - origin_y) / tile_height);           │
│                                                                         │
│  BOUNDS CHECK:                                                          │
│    bool valid = (row >= 0 && row < MAP_ROWS &&                         │
│                  col >= 0 && col < MAP_COLS);                          │
│                                                                         │
│  ARRAY ACCESS:                                                          │
│    u32 tile = map[row][col];  // [row][col] = [y][x]                │
│                                                                         │
│  TILE TO SCREEN:                                                        │
│    f32 screen_x = origin_x + col * tile_width;                      │
│    f32 screen_y = origin_y + row * tile_height;                     │
│                                                                         │
│  MEMORY LAYOUT:                                                         │
│    Row 0: [0][0] [0][1] [0][2] ... [0][16]  ← First in memory          │
│    Row 1: [1][0] [1][1] [1][2] ... [1][16]                             │
│    ...                                                                  │
│    Row 8: [8][0] [8][1] [8][2] ... [8][16]  ← Last in memory           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 12. Applying This to Your Code

Now let's look at your actual code and identify what needs to be fixed:

### 12.1 Current State Analysis

**In `main.h`:**

```c
#define TILE_MAP_COLUMN_COUNT 9
#define TILE_MAP_ROW_COUNT 17
typedef struct {
  u32 map[TILE_MAP_COLUMN_COUNT][TILE_MAP_ROW_COUNT];  // [9][17]
  // ...
} TileState;
```

**Problem:** The naming suggests X=columns and Y=rows, but the array is declared `[X][Y]` which would be `[cols][rows]`. This is backwards from the C convention of `[rows][cols]`.

**In `init.c`:**

```c
u32 temp_map[9][17] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},  // 17 elements
    // ... 9 rows total
};
```

**Problem:** The initialization is `[9][17]` which is 9 rows of 17 columns. This is correct for the visual layout, but doesn't match the struct declaration semantics.

**In `main.c`:**

```c
i32 tile_map_value = game_state->tile_state.map[player_tile_x][player_tile_y];
```

**Problem:** Accessing as `[x][y]` when it should be `[row][col]` = `[y][x]`.

### 12.2 The Fix

Here's exactly what you need to change:

**Step 1: Fix `main.h`**

```c
#define TILE_MAP_ROW_COUNT 9   // Renamed from Y_COUNT
#define TILE_MAP_COL_COUNT 17  // Renamed from X_COUNT

typedef struct {
  // Array is [rows][cols] - matches C convention
  u32 map[TILE_MAP_ROW_COUNT][TILE_MAP_COL_COUNT];
  f32 upper_left_x;
  f32 upper_left_y;
  f32 width;   // Width of each tile in pixels
  f32 height;  // Height of each tile in pixels
} TileState;
```

**Step 2: Fix `init.c`**

```c
// The visual layout is correct - 9 rows, 17 columns
// Just update the declaration to use new constant names
u32 temp_map[TILE_MAP_ROW_COUNT][TILE_MAP_COL_COUNT] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},  // Row 0 (top)
    {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},  // Row 1
    {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},  // Row 2
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},  // Row 3
    {0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},  // Row 4 (middle, has openings on sides)
    {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},  // Row 5
    {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},  // Row 6
    {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},  // Row 7
    {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},  // Row 8 (bottom)
};
```

**Step 3: Fix `main.c` - Collision**

```c
// In handle_controls():

// Calculate tile coordinates - use CLEAR variable names
i32 player_tile_col = truncate_f32_to_int32(
    (new_player_state_x - game_state->tile_state.upper_left_x) /
    game_state->tile_state.width);

i32 player_tile_row = truncate_f32_to_int32(
    (new_player_state_y - game_state->tile_state.upper_left_y) /
    game_state->tile_state.height);

bool32 is_valid_player_pos = false;

// Bounds check - col against COL_COUNT, row against ROW_COUNT
if (player_tile_col >= 0 && player_tile_col < TILE_MAP_COL_COUNT &&
    player_tile_row >= 0 && player_tile_row < TILE_MAP_ROW_COUNT) {

    // Array access - [row][col] order!
    i32 tile_map_value =
        game_state->tile_state.map[player_tile_row][player_tile_col];

    is_valid_player_pos = (tile_map_value == 0);

    if (!is_valid_player_pos) {
#if DE100_INTERNAL
        printf("Blocked by tile at [row=%d, col=%d] with value %d\n",
               player_tile_row, player_tile_col, tile_map_value);
#endif
    }
}
```

**Step 4: Fix `main.c` - Rendering**

```c
// In game_update_and_render():

TileState *tile_state = &game_state->tile_state;

// Iterate in row-major order (better cache performance)
for (u32 row = 0; row < TILE_MAP_ROW_COUNT; ++row) {
    for (u32 col = 0; col < TILE_MAP_COL_COUNT; ++col) {
        // Access array as [row][col]
        u32 tile_id = tile_state->map[row][col];
        f32 gray = tile_id == 1 ? 1.0f : 0.5f;

        // Screen position: col determines X, row determines Y
        f32 min_x = tile_state->upper_left_x + (f32)col * tile_state->width;
        f32 min_y = tile_state->upper_left_y + (f32)row * tile_state->height;
        f32 max_x = min_x + tile_state->width;
        f32 max_y = min_y + tile_state->height;

        draw_rect(buffer, min_x, min_y, max_x, max_y, gray, gray, gray, 1.0f);
    }
}
```

### 12.3 Complete Fixed Files

Here are the complete corrected versions:

```c
#ifndef DE100_HERO_GAME_H
#define DE100_HERO_GAME_H
#include "./inputs.h"

#include "../../../engine/_common/base.h"
#include "../../../engine/_common/memory.h"
#include "../../../engine/game/audio.h"
#include "../../../engine/game/backbuffer.h"
#include "../../../engine/game/base.h"
#include "../../../engine/game/game-loader.h"
#include "../../../engine/game/inputs.h"
#include "../../../engine/game/memory.h"

GameControllerInput *GetController(GameInput *Input,
                                   unsigned int ControllerIndex);

// Tile map dimensions - use ROW/COL naming for clarity
#define TILE_MAP_ROW_COUNT 9   // Number of rows (Y direction)
#define TILE_MAP_COL_COUNT 17  // Number of columns (X direction)

typedef struct {
  // Array layout: [row][col] - standard C convention
  // Row 0 is the TOP of the map, Row 8 is the BOTTOM
  // Col 0 is the LEFT of the map, Col 16 is the RIGHT
  u32 map[TILE_MAP_ROW_COUNT][TILE_MAP_COL_COUNT];

  // Screen position of tile [0][0]'s top-left corner
  f32 upper_left_x;
  f32 upper_left_y;

  // Size of each tile in pixels
  f32 width;
  f32 height;
} TileState;

typedef struct {
  i32 x;
  i32 y;
  f32 width;
  f32 height;
  f32 t_jump;
  f32 color_r;
  f32 color_g;
  f32 color_b;
  f32 color_a;
} PlayerState;

typedef struct {
  GameAudioState audio;
  TileState tile_state;
  PlayerState player_state;
  i32 speed;
} HandMadeHeroGameState;

#endif // DE100_HERO_GAME_H
```

```c
// ... (includes remain the same)

GAME_INIT(game_init) {
  (void)thread_context;
  (void)inputs;
  (void)buffer;

  DEV_ASSERT_MSG(sizeof(_GameButtonsCounter) == sizeof(GameButtonState) * 12,
                 "Button struct size mismatch");

  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage;

  if (!memory->is_initialized) {
#if DE100_INTERNAL
    printf("[GAME] First-time init\n");
#endif

    // ... (debug file I/O code remains the same)

    printf("[GAME INIT] game_update_and_render from build id %d\n", 1);

    // Initialize audio state
    game_state->audio.tone.frequency = 256;
    game_state->audio.tone.phase = 0.0f;
    game_state->audio.tone.volume = 1.0f;
    game_state->audio.tone.pan_position = 0.0f;
    game_state->audio.tone.is_playing = true;
    game_state->audio.master_volume = 1.0f;

    game_state->speed = 64;

    // ═══════════════════════════════════════════════════════════════════
    // TILE MAP INITIALIZATION
    // ═══════════════════════════════════════════════════════════════════
    // Array is [ROW][COL] = [9][17]
    // Each row has 17 columns
    // Row 0 = top of screen, Row 8 = bottom of screen
    // Col 0 = left of screen, Col 16 = right of screen
    //
    // Visual representation (what you see on screen):
    //   Col: 0 1 2 3 4 5 6 7 8 9 ...
    // Row 0: 1 1 1 1 1 1 1 1 0 1 ...  <- TOP
    // Row 1: 1 1 0 0 0 1 0 0 0 0 ...
    // ...
    // Row 8: 1 1 1 1 1 1 1 1 0 1 ...  <- BOTTOM
    // ═══════════════════════════════════════════════════════════════════

    u32 temp_map[TILE_MAP_ROW_COUNT][TILE_MAP_COL_COUNT] = {
        //   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16  <- Column indices
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},  // Row 0 (top)
        {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},  // Row 1
        {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},  // Row 2
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},  // Row 3
        {0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},  // Row 4 (openings left & right)
        {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},  // Row 5
        {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},  // Row 6
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},  // Row 7
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},  // Row 8 (bottom)
    };

    de100_mem_copy(game_state->tile_state.map, temp_map, sizeof(temp_map));

    game_state->tile_state.upper_left_x = -30;
    game_state->tile_state.upper_left_y = 0;
    game_state->tile_state.width = 60;
    game_state->tile_state.height = 60;

    // Player starts in a walkable area
    game_state->player_state.x = 150;
    game_state->player_state.y = 150;
    game_state->player_state.color_r = 1.0f;
    game_state->player_state.color_g = 1.0f;
    game_state->player_state.color_b = 0.0f;
    game_state->player_state.color_a = 1.0f;
    game_state->player_state.width = 0.75f * game_state->tile_state.width;
    game_state->player_state.height = game_state->tile_state.height;

    memory->is_initialized = true;

#if DE100_INTERNAL
    printf("[GAME] Init complete\n");
    printf("[GAME] Map size: %d rows x %d cols\n",
           TILE_MAP_ROW_COUNT, TILE_MAP_COL_COUNT);
    printf("[GAME] Tile size: %.0f x %.0f pixels\n",
           game_state->tile_state.width, game_state->tile_state.height);
#endif
    return;
  }
}
```

```c
// ... (includes and helper functions remain the same)

void handle_controls(GameControllerInput *inputs,
                     HandMadeHeroGameState *game_state, f32 frame_time) {
  if (inputs->is_analog) {
    // NOTE: Analog movement (currently disabled)
  } else {
    // NOTE: Use digital movement tuning
    f32 d_player_x = 0.0f;
    f32 d_player_y = 0.0f;

    if (inputs->move_up.ended_down) {
      d_player_y = -1.0f;
    }
    if (inputs->move_down.ended_down) {
      d_player_y = 1.0f;
    }
    if (inputs->move_left.ended_down) {
      d_player_x = -1.0f;
    }
    if (inputs->move_right.ended_down) {
      d_player_x = 1.0f;
    }

    d_player_x *= game_state->speed;
    d_player_y *= game_state->speed;

    // Calculate new position
    i32 new_player_x = game_state->player_state.x + (i32)(d_player_x * frame_time);
    i32 new_player_y = game_state->player_state.y + (i32)(d_player_y * frame_time);

    // ═══════════════════════════════════════════════════════════════════
    // TILE COLLISION
    // ═══════════════════════════════════════════════════════════════════
    // Convert screen position to tile coordinates
    // col = X direction, row = Y direction
    // ═══════════════════════════════════════════════════════════════════

    TileState *tiles = &game_state->tile_state;

    // Calculate tile coordinates
    // Note: Using truncate for now, should use floor for negative coords
    i32 player_tile_col = truncate_f32_to_int32(
        (new_player_x - tiles->upper_left_x) / tiles->width);
    i32 player_tile_row = truncate_f32_to_int32(
        (new_player_y - tiles->upper_left_y) / tiles->height);

    bool32 is_valid_player_pos = false;

    // Bounds check: col against COL_COUNT, row against ROW_COUNT
    if (player_tile_col >= 0 && player_tile_col < TILE_MAP_COL_COUNT &&
        player_tile_row >= 0 && player_tile_row < TILE_MAP_ROW_COUNT) {

        // Array access: [row][col] - THIS IS THE KEY FIX!
        u32 tile_map_value = tiles->map[player_tile_row][player_tile_col];

        is_valid_player_pos = (tile_map_value == 0);

#if DE100_INTERNAL
        if (!is_valid_player_pos) {
            printf("Blocked at [row=%d, col=%d] = %u\n",
                   player_tile_row, player_tile_col, tile_map_value);
        }
#endif
    }

    if (is_valid_player_pos) {
      game_state->player_state.x = new_player_x;
      game_state->player_state.y = new_player_y;
    }
  }
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {
  (void)thread_context;

  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage;

  // ... (controller selection code remains the same)

  f32 frame_time = de100_get_frame_time();
  handle_controls(active_controller, game_state, frame_time);

  // Clear background
  draw_rect(buffer, 0.0f, 0.0f, (f32)buffer->width, (f32)buffer->height,
            1.0f, 0.0f, 1.0f, 1.0f);

  // ═══════════════════════════════════════════════════════════════════════
  // RENDER TILE MAP
  // ═══════════════════════════════════════════════════════════════════════
  // Iterate in row-major order for cache efficiency
  // Access array as [row][col]
  // Screen position: col → X, row → Y
  // ═══════════════════════════════════════════════════════════════════════

  TileState *tile_state = &game_state->tile_state;

  for (u32 row = 0; row < TILE_MAP_ROW_COUNT; ++row) {
    for (u32 col = 0; col < TILE_MAP_COL_COUNT; ++col) {
      // Array access: [row][col]
      u32 tile_id = tile_state->map[row][col];
      f32 gray = tile_id == 1 ? 1.0f : 0.5f;

      // Screen position: col determines X, row determines Y
      f32 min_x = tile_state->upper_left_x + (f32)col * tile_state->width;
      f32 min_y = tile_state->upper_left_y + (f32)row * tile_state->height;
      f32 max_x = min_x + tile_state->width;
      f32 max_y = min_y + tile_state->height;

      draw_rect(buffer, min_x, min_y, max_x, max_y, gray, gray, gray, 1.0f);
    }
  }

  // Render player
  f32 player_left = game_state->player_state.x - game_state->player_state.width * 0.5f;
  f32 player_top = game_state->player_state.y - game_state->player_state.height * 0.5f;

  draw_rect(buffer, player_left, player_top,
            player_left + game_state->player_state.width,
            player_top + game_state->player_state.height,
            game_state->player_state.color_r,
            game_state->player_state.color_g,
            game_state->player_state.color_b,
            game_state->player_state.color_a);

  // Render mouse button indicators
  for (int button_index = 0; button_index < 5; ++button_index) {
    if (inputs->mouse_buttons[button_index].ended_down) {
      draw_rect(buffer, inputs->mouse_x, inputs->mouse_y,
                inputs->mouse_x + 10, inputs->mouse_y + 10,
                0.0f, 1.0f, 1.0f, 1.0f);
    }
  }
}

// ... (audio code remains the same)
```

---

## 13. Final Reflection Questions

Before moving on, answer these questions to verify your understanding:

### 13.1 Conceptual Understanding

1. **Why is `map[row][col]` the correct order in C?**
   - Think about how C stores 2D arrays in memory
   - What does "row-major order" mean?

2. **Why use `floorf()` vs `(i32)` cast for tile coordinates?**
   - **For non-negative coordinates (screen pixels, early tile maps):** `(i32)` cast is fine and faster
     - Both give the same result: `(int)3.7 = 3` and `floor(3.7) = 3`
   - **For potentially negative coordinates (world space):** Must use `floorf()`
     - They differ for negatives: `(int)(-3.7) = -3` but `floor(-3.7) = -4`
     - Example: `pixel_x = -1, tile_size = 32`
       - Wrong: `(int)(-1/32) = (int)(-0.03) = 0` (should be tile -1!)
       - Right: `floor(-1/32) = floor(-0.03) = -1` (correct tile boundary)
   - **Rule of thumb:** If your world origin is at (0,0) top-left and coordinates never go negative, simple cast is fine. If your world has a centered origin or infinite scrolling, use `floorf()`.

3. **Why is `< COUNT` correct but `<= COUNT` wrong for bounds checking?**
   - If you have 9 rows, what are the valid indices?
   - What happens if you access index 9?

### 13.2 Practical Application

4. **If you add a new tile type (e.g., water = 2), what code needs to change?**
   - Initialization
   - Collision checking
   - Rendering

5. **If you want the player to check collision at multiple points (not just center), what changes?**
   - How many points should you check?
   - How do you calculate those points?

6. **If you want to add a second tile map (for a different room), what's the cleanest way?**
   - Should you duplicate the TileState struct?
   - How would you switch between maps?

### 13.3 Debugging Skills

7. **If collision seems "mirrored" (walls on left block right movement), what's likely wrong?**
   - Which indices are probably swapped?
   - How would you verify this with printf?

8. **If the player can walk through walls near the edges but not in the middle, what's likely wrong?**
   - What's special about edge tiles?
   - What bounds check might be incorrect?

9. **If the map renders correctly but collision is offset by one tile, what's likely wrong?**
   - Are you using the same origin for both?
   - Is there an off-by-one in the coordinate calculation?

---

## 14. Conclusion

You've now completed a deep dive into one of the most common bugs in game development: the row/column confusion in 2D arrays.

**Key Takeaways:**

1. **C arrays are row-major**: `array[row][col]` where row is the first index
2. **Row = Y, Col = X**: The vertical axis maps to rows, horizontal to columns
3. **Naming matters**: Use `row`/`col` not `x`/`y` for tile indices
4. **Encapsulate access**: Never access the array directly; use helper functions
5. **Test systematically**: Check corners, edges, and negative coordinates

This knowledge will serve you well not just in Handmade Hero, but in any 2D game or application that uses grid-based data structures.

**Next Steps:**

1. Apply the fixes to your codebase
2. Test thoroughly by walking the player around all edges of the map
3. Verify that the visual map matches the collision map
4. Add debug visualization to show which tile the player is currently in

---

## 15. Bonus: Debug Visualization Helper

Here's a helper function you can add to visualize the current tile during development:

```c
#if DE100_INTERNAL
// Debug: Highlight the tile the player is currently in
de100_file_scoped_fn void debug_draw_player_tile(
    GameBackBuffer *buffer,
    TileState *tile_state,
    i32 player_x,
    i32 player_y) {

    // Calculate which tile the player center is in
    i32 col = truncate_f32_to_int32(
        (player_x - tile_state->upper_left_x) / tile_state->width);
    i32 row = truncate_f32_to_int32(
        (player_y - tile_state->upper_left_y) / tile_state->height);

    // Only draw if in bounds
    if (col >= 0 && col < TILE_MAP_COL_COUNT &&
        row >= 0 && row < TILE_MAP_ROW_COUNT) {

        f32 min_x = tile_state->upper_left_x + (f32)col * tile_state->width;
        f32 min_y = tile_state->upper_left_y + (f32)row * tile_state->height;
        f32 max_x = min_x + tile_state->width;
        f32 max_y = min_y + tile_state->height;

        // Draw a semi-transparent overlay on the current tile
        // Using a different color to distinguish from regular tiles
        // Red = solid tile, Green = empty tile
        u32 tile_value = tile_state->map[row][col];
        if (tile_value == 0) {
            // Green tint for walkable
            draw_rect(buffer, min_x + 2, min_y + 2, max_x - 2, max_y - 2,
                      0.0f, 1.0f, 0.0f, 0.3f);
        } else {
            // Red tint for solid
            draw_rect(buffer, min_x + 2, min_y + 2, max_x - 2, max_y - 2,
                      1.0f, 0.0f, 0.0f, 0.3f);
        }
    }

    // Print debug info
    static i32 debug_frame_counter = 0;
    if (++debug_frame_counter % 60 == 0) {
        printf("[DEBUG] Player at (%d, %d) -> Tile [row=%d, col=%d]\n",
               player_x, player_y, row, col);
        if (col >= 0 && col < TILE_MAP_COL_COUNT &&
            row >= 0 && row < TILE_MAP_ROW_COUNT) {
            printf("[DEBUG] Tile value: %u\n", tile_state->map[row][col]);
        }
    }
}
#endif
```

Then call it in your render function:

```c
GAME_UPDATE_AND_RENDER(game_update_and_render) {
    // ... existing code ...

    // Render tile map
    // ... existing tile rendering code ...

#if DE100_INTERNAL
    // Debug: Show which tile the player is in
    debug_draw_player_tile(buffer, tile_state,
                           game_state->player_state.x,
                           game_state->player_state.y);
#endif

    // Render player (after debug so player draws on top)
    // ... existing player rendering code ...
}
```

---

## 16. Quick Diagnostic Checklist

If you're still having issues after applying the fixes, run through this checklist:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TILE COLLISION DIAGNOSTIC                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  STEP 1: Verify Array Declaration                                       │
│  ─────────────────────────────────                                      │
│  □ main.h defines TILE_MAP_ROW_COUNT and TILE_MAP_COL_COUNT            │
│  □ TileState.map is declared as [ROW_COUNT][COL_COUNT]                 │
│  □ ROW_COUNT = 9, COL_COUNT = 17 (for your map)                        │
│                                                                         │
│  STEP 2: Verify Initialization                                          │
│  ─────────────────────────────                                          │
│  □ temp_map in init.c is [9][17] (9 rows, 17 elements each)            │
│  □ Each row in the initializer has exactly 17 values                   │
│  □ Visual layout matches expected (row 0 = top)                        │
│                                                                         │
│  STEP 3: Verify Collision Code                                          │
│  ─────────────────────────────                                          │
│  □ player_tile_col calculated from X position                          │
│  □ player_tile_row calculated from Y position                          │
│  □ Bounds check: col < COL_COUNT, row < ROW_COUNT                      │
│  □ Array access: map[row][col] (row FIRST)                             │
│                                                                         │
│  STEP 4: Verify Render Code                                             │
│  ──────────────────────────                                             │
│  □ Outer loop iterates over rows (0 to ROW_COUNT)                      │
│  □ Inner loop iterates over cols (0 to COL_COUNT)                      │
│  □ Array access: map[row][col] (row FIRST)                             │
│  □ Screen X calculated from col                                        │
│  □ Screen Y calculated from row                                        │
│                                                                         │
│  STEP 5: Test Cases                                                     │
│  ─────────────────                                                      │
│  □ Player can walk through opening at top (row 0, col 8)               │
│  □ Player can walk through opening at bottom (row 8, col 8)            │
│  □ Player blocked by walls on left edge (col 0)                        │
│  □ Player blocked by walls on right edge (col 16)                      │
│  □ Collision matches visual (no offset or mirroring)                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 17. The Mental Model to Remember

Whenever you work with 2D arrays in C, visualize this:

```
                    COLUMNS (X direction)
                    0   1   2   3   4   ...  16
                  ┌───┬───┬───┬───┬───┬───┬───┐
           ROW 0  │   │   │   │   │   │...│   │  ← First in memory
                  ├───┼───┼───┼───┼───┼───┼───┤
    R      ROW 1  │   │   │   │   │   │...│   │
    O             ├───┼───┼───┼───┼───┼───┼───┤
    W      ROW 2  │   │   │   │   │   │...│   │
    S             ├───┼───┼───┼───┼───┼───┼───┤
           ...    │   │   │   │   │   │...│   │
   (Y)            ├───┼───┼───┼───┼───┼───┼───┤
           ROW 8  │   │   │   │   │   │...│   │  ← Last in memory
                  └───┴───┴───┴───┴───┴───┴───┘

    Memory layout (linear):
    [0][0] [0][1] [0][2] ... [0][16] [1][0] [1][1] ... [8][16]
    ←────── Row 0 ──────────→←────── Row 1 ──────→ ...

    Access pattern:
    map[row][col]  =  map[Y][X]  =  map[vertical][horizontal]

    Conversion:
    col = f(screen_x)  →  horizontal position → second index
    row = f(screen_y)  →  vertical position   → first index
```

**The mantra:**

> "Rows go down, columns go across. Array is `[row][col]`. Row comes from Y, col comes from X."

---

## 18. Congratulations! 🎉

You've completed a comprehensive deep-dive into 2D array indexing and tile-based collision. This is one of those fundamental concepts that, once truly understood, will save you countless hours of debugging in the future.

**What you learned:**

- How C stores 2D arrays in memory (row-major order)
- Why `[row][col]` is the correct access pattern
- How to convert between screen coordinates and tile coordinates
- Common bugs and how to identify them
- Best practices for naming and code organization
- Debug techniques for tile-based systems

**Casey's Philosophy Applied:**

- We didn't just fix the bug; we understood WHY it happened
- We created mental models that will prevent future bugs
- We wrote code that's self-documenting through good naming
- We added debug tools to make future issues easier to diagnose

Now go apply these fixes and watch your player collide correctly with the walls! 🎮

---

## 🧮 2D Arrays as Flat Memory: Pointer Arithmetic for Grids

**Focus:** Understanding how 2D data (images, tilemaps, grids) is stored as contiguous 1D memory and accessed via index calculation.

---

### 🎯 Core Concept

In C, there's no "true" 2D array at runtime—everything is flat memory. When you declare `int grid[3][4]`, the compiler stores 12 integers contiguously and calculates offsets for you. When using `malloc` or `mmap`, YOU do the calculation.

```

┌─────────────────────────────────────────────────────────────────────────┐
│ 2D ARRAY → FLAT MEMORY MAPPING │
├─────────────────────────────────────────────────────────────────────────┤
│ │
│ LOGICAL VIEW (what you think): PHYSICAL VIEW (what exists): │
│ │
│ grid[y][x] or grid[row][col] Contiguous bytes in RAM │
│ │
│ ┌─────┬─────┬─────┬─────┐ ┌───┬───┬───┬───┬───┬───┬... │
│ │[0,0]│[0,1]│[0,2]│[0,3]│ row 0 │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ │
│ ├─────┼─────┼─────┼─────┤ └───┴───┴───┴───┴───┴───┴... │
│ │[1,0]│[1,1]│[1,2]│[1,3]│ row 1 ↑row 0↑ ↑row 1↑ ↑row 2 │
│ ├─────┼─────┼─────┼─────┤ │
│ │[2,0]│[2,1]│[2,2]│[2,3]│ row 2 │
│ └─────┴─────┴─────┴─────┘ │
│ │
│ HEIGHT = 3 rows │
│ WIDTH = 4 columns │
│ TOTAL = 12 elements │
│ │
│ ═══════════════════════════════════════════════════════════════════ │
│ THE FORMULA (Row-Major Order): │
│ ═══════════════════════════════════════════════════════════════════ │
│ │
│ index = (y _ WIDTH) + x │
│ │
│ OR with named variables: │
│ │
│ index = (row _ num_columns) + column │
│ │
│ ═══════════════════════════════════════════════════════════════════ │
│ EXAMPLES: │
│ ═══════════════════════════════════════════════════════════════════ │
│ │
│ grid[0][0] → index = 0 _ 4 + 0 = 0 │
│ grid[0][3] → index = 0 _ 4 + 3 = 3 │
│ grid[1][0] → index = 1 _ 4 + 0 = 4 ← Start of row 1 │
│ grid[1][2] → index = 1 _ 4 + 2 = 6 │
│ grid[2][3] → index = 2 \* 4 + 3 = 11 ← Last element │
│ │
└─────────────────────────────────────────────────────────────────────────┘

```

---

### 💻 Code Patterns

#### **1. Basic Flat Array Access**

```c
// Allocate flat buffer for 2D data
int width = 1280;
int height = 720;
u32 *pixels = (u32 *)malloc(width * height * sizeof(u32));

// ❌ WRONG: Can't use 2D syntax with pointer
// pixels[y][x] = color;  // Compiler error!

// ✅ CORRECT: Calculate index manually
pixels[y * width + x] = color;

// ✅ CORRECT: Read a pixel
u32 color = pixels[y * width + x];
```

#### **2. Macro for Clean 2D Access**

```c
// ═══════════════════════════════════════════════════════════════════════
// GENERIC 2D ACCESS MACRO
// ═══════════════════════════════════════════════════════════════════════
// Makes flat arrays feel like 2D arrays
// Works for reading AND writing (it's an lvalue!)

#define AT_2D(ptr, x, y, width) ((ptr)[(y) * (width) + (x)])

// Usage examples:
u32 *pixels = (u32 *)buffer->memory;

// Read pixel at (100, 50)
u32 color = AT_2D(pixels, 100, 50, buffer->width);

// Write pixel at (100, 50)
AT_2D(pixels, 100, 50, buffer->width) = 0xFF00FF00;

// Use in loops
for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        AT_2D(pixels, x, y, width) = (x ^ y) * 0x010101;  // Pattern
    }
}
```

#### **3. Pitch/Stride-Aware Access (For Aligned Memory)**

```c
// ═══════════════════════════════════════════════════════════════════════
// PITCH VS WIDTH
// ═══════════════════════════════════════════════════════════════════════
//
// Sometimes rows have PADDING for memory alignment:
//
//   Width = 1280 pixels × 4 bytes = 5120 bytes per row (logical)
//   Pitch = 5120 bytes, rounded up to 5632 for 512-byte alignment
//
//   Memory layout:
//   ┌──────────────────────────────────────┬─────────┐
//   │ Row 0: 1280 pixels (5120 bytes)      │ PADDING │
//   ├──────────────────────────────────────┼─────────┤
//   │ Row 1: 1280 pixels (5120 bytes)      │ PADDING │
//   └──────────────────────────────────────┴─────────┘
//
// If you use width instead of pitch, you'll read into padding garbage!
// ═══════════════════════════════════════════════════════════════════════

// Pitch-aware macro (ptr is void* or u8*, pitch is in BYTES)
#define AT_2D_PITCH(base, x, y, pitch, type) \
    (((type *)((u8 *)(base) + (y) * (pitch)))[(x)])

// Usage:
u32 color = AT_2D_PITCH(buffer->memory, x, y, buffer->pitch, u32);
AT_2D_PITCH(buffer->memory, x, y, buffer->pitch, u32) = 0xFFFF0000;

// ═══════════════════════════════════════════════════════════════════════
// ALTERNATIVE: Row pointer approach (Casey's pattern)
// ═══════════════════════════════════════════════════════════════════════

void render_gradient(GameOffscreenBuffer *buffer) {
    u8 *row = (u8 *)buffer->memory;  // Start at first row

    for (int y = 0; y < buffer->height; y++) {
        u32 *pixel = (u32 *)row;  // Cast row to pixel pointer

        for (int x = 0; x < buffer->width; x++) {
            u8 blue = (u8)(x);
            u8 green = (u8)(y);
            *pixel++ = (green << 8) | blue;  // Write and advance
        }

        row += buffer->pitch;  // Move to next row (handles padding!)
    }
}
```

#### **4. Tilemap Access Pattern**

```c
// ═══════════════════════════════════════════════════════════════════════
// TILEMAP EXAMPLE
// ═══════════════════════════════════════════════════════════════════════

#define TILE_MAP_WIDTH 17
#define TILE_MAP_HEIGHT 9

// Store as flat array
u32 tilemap[TILE_MAP_HEIGHT * TILE_MAP_WIDTH] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  // Row 0
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,  // Row 1
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,  // Row 2
    // ... etc
};

// Access macro for this specific tilemap
#define TILE_AT(x, y) (tilemap[(y) * TILE_MAP_WIDTH + (x)])

// Check if tile is solid
bool is_solid = (TILE_AT(player_tile_x, player_tile_y) == 1);

// Set a tile
TILE_AT(5, 3) = 2;  // Place tile type 2 at (5, 3)
```

#### **5. Generic Multi-Array Macro System**

```c
// ═══════════════════════════════════════════════════════════════════════
// REUSABLE MACRO GENERATOR FOR MULTIPLE 2D ARRAYS
// ═══════════════════════════════════════════════════════════════════════

// Define accessor for a specific array
#define DEFINE_2D_ACCESSOR(name, array, width) \
    static inline u32 name##_get(int x, int y) { \
        return (array)[(y) * (width) + (x)]; \
    } \
    static inline void name##_set(int x, int y, u32 value) { \
        (array)[(y) * (width) + (x)] = value; \
    }

// Usage:
u32 g_world_tiles[100 * 100];
u32 g_collision_map[50 * 50];

DEFINE_2D_ACCESSOR(world, g_world_tiles, 100)
DEFINE_2D_ACCESSOR(collision, g_collision_map, 50)

// Now you have type-safe functions:
u32 tile = world_get(10, 20);
world_set(10, 20, TILE_GRASS);

bool blocked = (collision_get(px, py) != 0);
```

---

### 🐛 Common Pitfalls

| Issue                                      | Cause                                                | Fix                                                                  |
| ------------------------------------------ | ---------------------------------------------------- | -------------------------------------------------------------------- |
| **Garbage pixels / wrong colors**          | Used `width` instead of `pitch` for row advancement  | Always use `pitch` (bytes per row) not `width * bytes_per_pixel`     |
| **Segfault at edge of image**              | Off-by-one: accessed `pixels[y * width + width]`     | Ensure `x < width` and `y < height` (not `<=`)                       |
| **Image appears sheared/skewed**           | Pitch != width × bytes_per_pixel (alignment padding) | Use pitch-aware access pattern                                       |
| **X and Y swapped**                        | Used `x * width + y` instead of `y * width + x`      | Remember: Y selects ROW, X selects COLUMN within row                 |
| **Vertical stripes instead of horizontal** | Iterating X in outer loop, Y in inner                | Outer loop = Y (rows), inner loop = X (columns) for cache efficiency |
| **Only top-left corner renders**           | Forgot to multiply by `sizeof(type)` in byte offset  | Use typed pointer (`u32*`) so compiler handles size                  |

---

### 📊 Row-Major vs Column-Major

```
┌─────────────────────────────────────────────────────────────────────────┐
│              ROW-MAJOR (C, C++, Python) vs COLUMN-MAJOR (Fortran, MATLAB)│
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Logical 2D Array:                                                      │
│  ┌─────┬─────┬─────┐                                                   │
│  │  A  │  B  │  C  │  row 0                                            │
│  ├─────┼─────┼─────┤                                                   │
│  │  D  │  E  │  F  │  row 1                                            │
│  └─────┴─────┴─────┘                                                   │
│                                                                         │
│  ROW-MAJOR (C uses this):          COLUMN-MAJOR (Fortran):             │
│  Memory: [A][B][C][D][E][F]        Memory: [A][D][B][E][C][F]          │
│  Index:   0  1  2  3  4  5         Index:   0  1  2  3  4  5           │
│                                                                         │
│  Formula: index = y * WIDTH + x    Formula: index = x * HEIGHT + y     │
│                                                                         │
│  ═══════════════════════════════════════════════════════════════════   │
│  WHY IT MATTERS: Cache Efficiency                                       │
│  ═══════════════════════════════════════════════════════════════════   │
│                                                                         │
│  ✅ GOOD (row-major, iterate X in inner loop):                          │
│  for (y = 0; y < height; y++) {                                        │
│      for (x = 0; x < width; x++) {                                     │
│          pixels[y * width + x] = color;  // Sequential memory access   │
│      }                                                                  │
│  }                                                                      │
│  Access pattern: [0][1][2][3][4][5]... → Cache friendly! 🚀            │
│                                                                         │
│  ❌ BAD (column-major iteration on row-major data):                     │
│  for (x = 0; x < width; x++) {                                         │
│      for (y = 0; y < height; y++) {                                    │
│          pixels[y * width + x] = color;  // Jumping around memory!     │
│      }                                                                  │
│  }                                                                      │
│  Access pattern: [0][3][6][1][4][7]... → Cache thrashing! 🐌           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

### 🎮 Real-World Example: Handmade Hero Gradient

```c
// From Day 3-5: Casey's gradient rendering pattern
void render_weird_gradient(GameOffscreenBuffer *buffer, int x_offset, int y_offset) {
    // Start at first byte of buffer
    u8 *row = (u8 *)buffer->memory;

    for (int y = 0; y < buffer->height; y++) {
        // Cast current row to 32-bit pixels
        u32 *pixel = (u32 *)row;

        for (int x = 0; x < buffer->width; x++) {
            // Calculate color based on position + offset
            u8 blue = (u8)(x + x_offset);
            u8 green = (u8)(y + y_offset);
            u8 red = 0;

            // Pack into 32-bit ARGB (or BGRA depending on platform)
            *pixel++ = (red << 16) | (green << 8) | blue;
            //  ↑ Write to current pixel, then advance pointer
        }

        // Move to next row using PITCH (not width!)
        row += buffer->pitch;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// WHY CASEY USES THIS PATTERN:
// ═══════════════════════════════════════════════════════════════════════
//
// 1. `row` is u8* so we can add pitch (bytes) directly
// 2. `pixel` is u32* so *pixel++ advances by 4 bytes automatically
// 3. Handles pitch != width * 4 (memory alignment padding)
// 4. Sequential memory access = cache efficient
// 5. No multiplication per pixel (just pointer increment)
//
// EQUIVALENT USING MACRO:
// for (int y = 0; y < buffer->height; y++) {
//     for (int x = 0; x < buffer->width; x++) {
//         AT_2D_PITCH(buffer->memory, x, y, buffer->pitch, u32) = color;
//     }
// }
// ═══════════════════════════════════════════════════════════════════════
```

---

### ✅ Quick Reference

```c
// ═══════════════════════════════════════════════════════════════════════
// COPY-PASTE READY MACROS
// ═══════════════════════════════════════════════════════════════════════

// Simple 2D access (when pitch == width * element_size)
#define AT_2D(ptr, x, y, width) \
    ((ptr)[(y) * (width) + (x)])

// Pitch-aware 2D access (handles alignment padding)
#define AT_2D_PITCH(base, x, y, pitch, type) \
    (((type *)((u8 *)(base) + (y) * (pitch)))[(x)])

// Bounds-checked access (debug builds)
#define AT_2D_SAFE(ptr, x, y, width, height) \
    (Assert((x) >= 0 && (x) < (width) && (y) >= 0 && (y) < (height)), \
     (ptr)[(y) * (width) + (x)])

// Convert 2D coords to 1D index
#define INDEX_2D(x, y, width) ((y) * (width) + (x))

// Convert 1D index back to 2D coords
#define INDEX_TO_X(index, width) ((index) % (width))
#define INDEX_TO_Y(index, width) ((index) / (width))

// ═══════════════════════════════════════════════════════════════════════
// USAGE EXAMPLES
// ═══════════════════════════════════════════════════════════════════════

// Pixel buffer
u32 *pixels = buffer->memory;
AT_2D(pixels, 100, 50, buffer->width) = 0xFF0000FF;

// Tilemap
u32 tile = AT_2D(tilemap, tile_x, tile_y, MAP_WIDTH);

// With pitch (image with alignment)
u32 color = AT_2D_PITCH(image->data, x, y, image->pitch, u32);

// Get index for external use
int idx = INDEX_2D(x, y, width);
some_function(idx);

// Reverse: index to coordinates
int x = INDEX_TO_X(clicked_index, grid_width);
int y = INDEX_TO_Y(clicked_index, grid_width);
```

---

### 🎓 Key Takeaways

1. **C has no runtime 2D arrays** — Everything is flat memory with calculated offsets
2. **Formula: `index = y * width + x`** — Y selects row, X selects column within row
3. **Pitch ≠ Width** — Pitch includes alignment padding, always use pitch for row advancement
4. **Macros make it readable** — `AT_2D(pixels, x, y, width)` is clearer than `pixels[y * width + x]`
5. **Cache efficiency matters** — Iterate X in inner loop (sequential memory access)
6. **Pointer arithmetic is your friend** — `*pixel++` is faster than `pixels[y * width + x]` in tight loops

```

This section covers:
- Visual explanation of 2D→1D mapping
- Multiple macro patterns (simple, pitch-aware, bounds-checked)
- Real-world Handmade Hero example
- Cache efficiency considerations
- Common pitfalls table
- Copy-paste ready code
```
