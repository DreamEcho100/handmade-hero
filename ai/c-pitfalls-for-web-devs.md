# 🕳️ C Pitfalls for Web Developers: A Survival Guide

> **"In JavaScript, the runtime catches your mistakes. In C, your mistakes catch you."**

This guide documents the **exact traps** that will bite you as a web developer learning C.
Each pitfall includes:

- 🔴 The trap (what goes wrong)
- 🟢 The fix (what to do instead)
- 🧠 The mental model (why it works this way)
- 🌐 Web dev analogy (what you're used to)

---

## Table of Contents

1. [Logical vs Bitwise Operators](#1-logical-vs-bitwise-operators)
2. [Integer Division Truncation](#2-integer-division-truncation)
   - [Negative Modulo (The % Trap)](#-gotcha-negative-modulo-the--trap)
3. [Signed vs Unsigned Integers](#3-signed-vs-unsigned-integers)
   - [Unsigned Coordinates That Go Negative](#-real-world-trap-unsigned-coordinates-that-go-negative)
4. [Implicit Type Conversions (Coercion)](#4-implicit-type-conversions-coercion)
5. [Operator Precedence Surprises](#5-operator-precedence-surprises)
6. [Assignment vs Comparison](#6-assignment-vs-comparison)
7. [Uninitialized Variables](#7-uninitialized-variables)
8. [Array Bounds (No Runtime Checks)](#8-array-bounds-no-runtime-checks)
9. [String Handling Nightmares](#9-string-handling-nightmares)
10. [Pointer Arithmetic Mistakes](#10-pointer-arithmetic-mistakes)
    - [Pointer Type Determines Write Size](#-real-world-trap-pointer-type-determines-write-size)
11. [Memory Leaks and Use-After-Free](#11-memory-leaks-and-use-after-free)
12. [Macro Pitfalls](#12-macro-pitfalls)
13. [sizeof() Surprises](#13-sizeof-surprises)
14. [Floating Point Comparison](#14-floating-point-comparison)
15. [Short-Circuit Evaluation Side Effects](#15-short-circuit-evaluation-side-effects)
16. [Buffer Overflows](#16-buffer-overflows)
17. [Null Pointer Dereference](#17-null-pointer-dereference)
18. [Stack vs Heap Lifetime](#18-stack-vs-heap-lifetime)
19. [Undefined Behavior (The Silent Killer)](#19-undefined-behavior-the-silent-killer)
20. [Platform-Specific Gotchas](#20-platform-specific-gotchas)
21. [Macro Definition Order Across Compilation Units](#21-macro-definition-order-across-compilation-units)

---

## 1. Logical vs Bitwise Operators

### 🔴 THE TRAP (What You Just Hit!)

```c
// You wrote this thinking "give me the first non-zero value"
uint32 fps = config.initial_fps || config.target_fps;

// What you EXPECTED (JavaScript behavior):
// fps = 60 (the first truthy value)

// What you GOT:
// fps = 1 (boolean true!)
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    LOGICAL vs BITWISE OPERATORS                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  LOGICAL (||, &&, !)         BITWISE (|, &, ~, ^)                      │
│  ─────────────────           ────────────────────                      │
│  Returns: 0 or 1             Returns: computed bits                    │
│  Purpose: Boolean logic      Purpose: Bit manipulation                 │
│                                                                         │
│  60 || 0  →  1 (true)        60 | 0   →  60 (0011 1100 | 0000 0000)   │
│  60 && 30 →  1 (true)        60 & 30  →  28 (0011 1100 & 0001 1110)   │
│  !60      →  0 (false)       ~60      →  -61 (flip all bits)          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: || returns the actual value (short-circuit with value)
const fps = config.initialFps || config.targetFps || 60;
// If initialFps is 60, fps = 60

// C: || returns 0 or 1 (boolean only!)
// There is NO equivalent to JS's || in C
```

### 🟢 THE FIX

```c
// Option 1: Ternary operator (most common)
uint32 fps = (config.initial_fps != 0)
    ? config.initial_fps
    : config.target_fps;

// Option 2: Explicit if statement (clearer)
uint32 fps = config.target_fps;
if (config.initial_fps != 0) {
    fps = config.initial_fps;
}

// Option 3: Helper macro (if you do this a lot)
#define FIRST_NONZERO(a, b) ((a) != 0 ? (a) : (b))
uint32 fps = FIRST_NONZERO(config.initial_fps, config.target_fps);
```

### 📋 QUICK REFERENCE

| Expression   | JavaScript Result | C Result | Why                        |
| ------------ | ----------------- | -------- | -------------------------- |
| `60 \|\| 0`  | `60`              | `1`      | C: logical OR returns bool |
| `0 \|\| 30`  | `30`              | `1`      | C: 30 is truthy = 1        |
| `0 \|\| 0`   | `0`               | `0`      | Both false                 |
| `60 && 30`   | `30`              | `1`      | C: both truthy = 1         |
| `null ?? 30` | `30`              | N/A      | No nullish coalescing in C |

---

## 2. Integer Division Truncation

### 🔴 THE TRAP

```c
int frame_time_ms = 1000 / 60;
// You expect: 16.666...
// You get: 16 (truncated toward zero!)

float target_fps = 1000 / 60;
// You expect: 16.666...
// You get: 16.0 (division happens BEFORE conversion!)
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       INTEGER DIVISION IN C                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  int / int = int (ALWAYS)                                              │
│                                                                         │
│  1000 / 60 = 16  (remainder 40 is DISCARDED)                           │
│                                                                         │
│  The division happens FIRST, THEN the result is converted:             │
│                                                                         │
│  float x = 1000 / 60;                                                  │
│           └─────────┘                                                  │
│           int division = 16                                            │
│           └─────────────────┘                                          │
│           convert to float = 16.0                                      │
│                                                                         │
│  To get float division, at least ONE operand must be float:            │
│                                                                         │
│  float x = 1000.0f / 60;   // = 16.666...                              │
│  float x = 1000 / 60.0f;   // = 16.666...                              │
│  float x = (float)1000 / 60; // = 16.666...                            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: All numbers are floats (IEEE 754 doubles)
const frameTime = 1000 / 60; // 16.666666666666668

// To get integer division in JS, you must explicitly truncate:
const frameTimeInt = Math.floor(1000 / 60); // 16

// C is the OPPOSITE:
// You get integer division by default
// You must explicitly request float division
```

### 🟢 THE FIX

```c
// Method 1: Use float literals
float frame_time = 1000.0f / 60.0f;  // 16.666...

// Method 2: Cast one operand
float frame_time = (float)1000 / 60;  // 16.666...

// Method 3: Cast variables
int total_ms = 1000;
int fps = 60;
float frame_time = (float)total_ms / (float)fps;

// Method 4: For ratios, multiply first to preserve precision
// BAD:  int percent = (current / total) * 100;  // Always 0 if current < total!
// GOOD: int percent = (current * 100) / total;  // Works!
```

### ⚠️ GOTCHA: Negative Division and Modulo

```c
// C99+: Division truncates toward zero (NOT floor!)
-7 / 2 = -3  (not -4!)
7 / -2 = -3  (not -4!)

// This is DIFFERENT from Python's floor division:
// Python: -7 // 2 = -4 (floor toward negative infinity)
```

### ⚠️ GOTCHA: Negative Modulo (The % Trap)

```c
// C's % operator can return NEGATIVE results!
-5 % 3 = -2   // NOT 1 like in Python!
-7 % 4 = -3   // NOT 1!
 7 % -4 = 3   // Sign follows the dividend (left operand)

// Python vs C comparison:
//   Python: -5 % 3 = 1  (always positive, Euclidean modulo)
//   C:      -5 % 3 = -2 (sign matches dividend)
```

**Why This Matters (Screen Wrapping Example):**

```c
// Naive wrap function - BROKEN for negative inputs!
int wrap(int value, int max) {
    return value % max;  // Returns negative for negative value!
}

wrap(725, 720) =  5   ✓
wrap(-5, 720)  = -5   ✗ (wanted 715!)

// CORRECT wrap function for negative values:
int wrap(int value, int max) {
    return ((value % max) + max) % max;
}

// How it works for value = -5, max = 720:
// Step 1: -5 % 720  = -5    (C's negative modulo)
// Step 2: -5 + 720  = 715   (shift to positive)
// Step 3: 715 % 720 = 715   (keep in range if step 2 overshot)
```

---

## 3. Signed vs Unsigned Integers

### 🔴 THE TRAP

```c
unsigned int a = 5;
int b = -10;

if (a + b > 0) {
    printf("Positive!\n");  // This PRINTS! Why?!
}

// a + b = 5 + (-10) = -5... right?
// WRONG! b is converted to unsigned first!
// -10 as unsigned = 4294967286
// 5 + 4294967286 = 4294967291 (huge positive number!)
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SIGNED vs UNSIGNED CONVERSION                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  When signed meets unsigned, SIGNED CONVERTS TO UNSIGNED               │
│  (This is called "usual arithmetic conversions")                        │
│                                                                         │
│  int b = -10;                                                          │
│                                                                         │
│  Memory (32-bit):  1111 1111 1111 1111 1111 1111 1111 0110              │
│                    └─ Two's complement representation of -10           │
│                                                                         │
│  As signed int:    -10                                                 │
│  As unsigned int:  4294967286 (same bits, different interpretation!)   │
│                                                                         │
│  The bits don't change - only the INTERPRETATION changes!              │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: No unsigned integers (until BigInt/TypedArrays)
// All numbers are signed 64-bit floats
-10 + 5 = -5  // Always works as expected

// In C, you must be aware of signedness at all times
// There's no automatic "do the right thing"
```

### 🟢 THE FIX

```c
// Rule 1: Don't mix signed and unsigned in comparisons
// BAD:
unsigned int size = get_size();
int offset = -5;
if (offset < size) { ... }  // BROKEN if offset is negative!

// GOOD: Cast to same type explicitly
if ((int)size > offset) { ... }
// OR
if (offset >= 0 && (unsigned int)offset < size) { ... }

// Rule 2: Use size_t for sizes, but be careful
size_t len = strlen(str);
int i;
for (i = len - 1; i >= 0; i--) {  // INFINITE LOOP if len is 0!
    // When i = 0, i-- makes it wrap to SIZE_MAX (huge positive)
}

// GOOD: Use size_t consistently or check bounds
for (size_t i = len; i > 0; i--) {
    process(str[i - 1]);
}
```

### 📋 DANGER ZONES

| Operation                    | Danger              | Example                                    |
| ---------------------------- | ------------------- | ------------------------------------------ |
| Comparing signed to unsigned | Sign extension      | `if (-1 < 1u)` is FALSE!                   |
| Subtracting from unsigned    | Underflow           | `unsigned x = 0; x--;` = MAX_UINT          |
| Loop counters                | Infinite loops      | `for (unsigned i = 10; i >= 0; i--)`       |
| Array indexing               | Huge positive index | `arr[negative_int]` with unsigned arr size |

### 🔴 REAL-WORLD TRAP: Unsigned Coordinates That Go Negative

This trap is especially common in **game development** when handling positions:

```c
// Player position stored as unsigned (seems logical - pixels are positive!)
typedef struct {
    uint32 x;  // ← THE TRAP!
    uint32 y;
} PlayerState;

// Player at position (5, 10), moving up by 15 pixels
player.y = player.y - 15;

// You expect: player.y = -5 (to be wrapped later)
// Reality:    player.y = 4294967291 (HUGE positive number!)
//
// Why? uint32 can't represent -5, so it wraps around:
// 10 - 15 = -5  →  0xFFFFFFFB  →  4294967291
```

**The Visual Bug:**

```
Before (y=10):     After with uint32 (y=4294967291):
┌───────────┐      Player "teleports" to bottom of screen
│  █ player │      or causes out-of-bounds memory access!
│           │
│           │      ┌───────────────────────────────────────┐
│           │      │                                       │
└───────────┘      │                                  █    │
                   └───────────────────────────────────────┘
```

**Why This Trap Is Sneaky:**

1. **It "works" most of the time** - when coordinates stay positive
2. **Debug builds may mask it** - some compilers initialize memory differently
3. **Unsigned seems logical** - "positions can't be negative on screen!"
4. **Wrapping math breaks** - `((y % height) + height) % height` assumes signed input

```c
// Wrapping function expects SIGNED input
int32 wrap_y(int32 y, int32 height) {
    return ((y % height) + height) % height;
}

// With signed int32: wrap_y(-5, 720) = 715 ✓
// With unsigned:     The -5 never existed! You passed 4294967291
//                    4294967291 % 720 = 611 (WRONG!)
```

### 🟢 THE FIX: Use Signed Types for Coordinates

```c
// GOOD: Use signed integers for anything that might go negative
typedef struct {
    int32 x;  // Can handle negative during calculations
    int32 y;
} PlayerState;

// Now subtraction works correctly
player.y = player.y - 15;  // If y=10, result is -5 (correct!)

// And wrapping works
int32 wrapped_y = wrap_y(player.y, height);  // -5 → 715 ✓
```

### 📋 WHEN TO USE UNSIGNED vs SIGNED

| Use Case              | Type        | Why                              |
| --------------------- | ----------- | -------------------------------- |
| Positions/coordinates | `int32`     | Can go negative during math      |
| Velocities            | `int32`     | Can be negative (moving left/up) |
| Array indices         | `size_t`    | But careful with subtraction!    |
| Bit flags             | `uint32`    | Pure bit manipulation            |
| Colors (RGBA)         | `uint32`    | Always 0-255 per channel         |
| Byte counts/sizes     | `size_t`    | Can't have negative bytes        |
| Memory addresses      | `uintptr_t` | Addresses are unsigned           |

**Rule of Thumb:** If the value might **ever** become negative during calculations
(even temporarily), use a signed type.

---

## 4. Implicit Type Conversions (Coercion)

### 🔴 THE TRAP

```c
char c = 200;      // You think: c = 200
                   // Reality: c = -56 (on systems where char is signed!)

int x = 3.7f;      // You think: x = 3.7
                   // Reality: x = 3 (truncated, no warning!)

float f = 1e40;    // You think: f = 1e40
int i = f;         // Reality: UNDEFINED BEHAVIOR (overflow)
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    C's IMPLICIT CONVERSION HIERARCHY                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  PROMOTION (safe, automatic):                                          │
│  char → int → long → long long                                         │
│  float → double → long double                                          │
│                                                                         │
│  DEMOTION (dangerous, silent!):                                        │
│  double → float → int → short → char                                   │
│                                                                         │
│  What happens during demotion:                                         │
│  ┌────────────────────────────────────────────────────────┐            │
│  │ float → int:    Truncate (3.9 → 3)                     │            │
│  │ large → small:  Modulo wrap (256 in char → 0)          │            │
│  │ signed ↔ unsigned: Bit reinterpretation                │            │
│  └────────────────────────────────────────────────────────┘            │
│                                                                         │
│  THE COMPILER WILL NOT WARN BY DEFAULT!                                │
│  Use: -Wconversion -Wsign-conversion to catch these!                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: "Everything converts to everything" (type coercion madness)
"5" + 3 = "53"   // String concatenation
"5" - 3 = 2     // Numeric subtraction
"5" * "3" = 15  // Numeric multiplication

// C: No string coercion, but SILENT numeric truncation
// At least C is consistent, but it won't tell you when data is lost!
```

### 🟢 THE FIX

```c
// Fix 1: Enable compiler warnings
// Add to your build: -Wconversion -Wsign-conversion -Wfloat-conversion

// Fix 2: Use explicit casts to show intent
int frame = (int)floorf(time_seconds * 60.0f);  // Explicit truncation

// Fix 3: Use types that match your data
uint8_t byte_value = 200;  // unsigned char, holds 0-255
int16_t audio_sample;      // signed 16-bit for audio
uint32_t color_rgba;       // unsigned 32-bit for colors

// Fix 4: Range check before assignment
float volume = get_volume();  // Could be any float
if (volume < 0.0f) volume = 0.0f;
if (volume > 1.0f) volume = 1.0f;
uint8_t volume_byte = (uint8_t)(volume * 255.0f);  // Safe now
```

---

## 5. Operator Precedence Surprises

### 🔴 THE TRAP

```c
// Trap 1: Bitwise vs Comparison
if (flags & FLAG_ACTIVE == FLAG_ACTIVE) { ... }
// You think: (flags & FLAG_ACTIVE) == FLAG_ACTIVE
// Reality:   flags & (FLAG_ACTIVE == FLAG_ACTIVE) = flags & 1

// Trap 2: Shift vs Addition
int x = 1 << 4 + 1;
// You think: (1 << 4) + 1 = 17
// Reality:   1 << (4 + 1) = 32

// Trap 3: Ternary vs Assignment
int x = condition ? a : b = 5;
// This doesn't do what you think!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    OPERATOR PRECEDENCE (HIGH to LOW)                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1.  () [] -> .                    Postfix                             │
│  2.  ! ~ ++ -- + - * & (type) sizeof   Unary (right-to-left)          │
│  3.  * / %                         Multiplicative                      │
│  4.  + -                           Additive                            │
│  5.  << >>                         Shift         ← LOWER than + - !    │
│  6.  < <= > >=                     Relational                          │
│  7.  == !=                         Equality      ← HIGHER than & | !   │
│  8.  &                             Bitwise AND                          │
│  9.  ^                             Bitwise XOR                          │
│  10. |                             Bitwise OR                           │
│  11. &&                            Logical AND                          │
│  12. ||                            Logical OR                           │
│  13. ?:                            Ternary       ← VERY LOW!           │
│  14. = += -= etc.                  Assignment    ← LOWEST              │
│  15. ,                             Comma                                │
│                                                                         │
│  ⚠️  COMMON SURPRISE: == binds TIGHTER than & | ^                      │
│  ⚠️  COMMON SURPRISE: + - bind TIGHTER than << >>                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// RULE: When in doubt, use parentheses!

// Bitwise operations: ALWAYS parenthesize
if ((flags & FLAG_ACTIVE) == FLAG_ACTIVE) { ... }
if ((flags & (FLAG_A | FLAG_B)) != 0) { ... }

// Shift operations: ALWAYS parenthesize
int x = (1 << 4) + 1;        // 17
int mask = (1 << n) - 1;     // n bits set

// Ternary: Parenthesize the whole thing if assigning
int x = (condition ? a : b);

// Compound expressions: Break into multiple lines
// BAD:
result = base + offset << scale & mask;

// GOOD:
int shifted = offset << scale;
int masked = shifted & mask;
result = base + masked;
```

---

## 6. Assignment vs Comparison

### 🔴 THE TRAP

```c
int x = 5;

if (x = 0) {  // ASSIGNMENT, not comparison!
    printf("x is zero\n");  // Never prints
}
// Now x is 0, and the if condition evaluated to 0 (false)

// Even worse with pointers:
if (ptr = NULL) {  // Sets ptr to NULL, condition is false
    printf("ptr is null\n");  // Never prints, AND ptr is now NULL!
}
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ASSIGNMENT vs COMPARISON                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  =   Assignment: Changes the variable, returns the new value           │
│  ==  Comparison: Compares two values, returns 0 or 1                   │
│                                                                         │
│  if (x = 0)     →  x becomes 0, condition is 0 (false)                 │
│  if (x == 0)    →  x unchanged, condition is 0 or 1                    │
│                                                                         │
│  WHY C allows assignment in if:                                        │
│  ┌────────────────────────────────────────────────────────┐            │
│  │ // This is a VALID pattern:                            │            │
│  │ if ((ptr = malloc(100)) == NULL) {                     │            │
│  │     // Handle allocation failure                        │            │
│  │ }                                                       │            │
│  │ // ptr is assigned AND checked in one statement        │            │
│  └────────────────────────────────────────────────────────┘            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Same trap exists!
let x = 5;
if ((x = 0)) {
  // Assignment, not comparison
  console.log("zero"); // Never runs
}

// But JS linters catch this. C compilers might not by default.
// ESLint: "Expected a conditional expression and instead saw an assignment"
```

### 🟢 THE FIX

```c
// Fix 1: Enable -Wall (warns about assignment in condition)
// gcc -Wall -Werror main.c

// Fix 2: "Yoda conditions" (constant on left)
if (0 == x) { ... }  // If you typo =, compiler errors!
if (NULL == ptr) { ... }
// Some people hate this style, but it's foolproof

// Fix 3: Extra parentheses silence the warning when intentional
if ((ptr = malloc(100)) != NULL) {  // Intentional, parenthesized
    // Use ptr
}

// Fix 4: Separate assignment from condition
ptr = malloc(100);
if (ptr != NULL) {
    // Use ptr
}
```

---

## 7. Uninitialized Variables

### 🔴 THE TRAP

```c
int main() {
    int x;
    printf("%d\n", x);  // Could print ANYTHING! Undefined behavior!

    int* ptr;
    *ptr = 5;  // CRASH! ptr points to garbage address

    char buffer[100];
    printf("%s\n", buffer);  // Prints garbage until random null byte
}
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    UNINITIALIZED MEMORY IN C                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Stack memory contains WHATEVER WAS THERE BEFORE                        │
│                                                                         │
│  void foo() {                                                          │
│      int x;  // x has whatever bits were on the stack                  │
│  }           // Could be 0, could be 0xDEADBEEF, could be anything     │
│                                                                         │
│  ┌─────────────────┐                                                   │
│  │ Stack Frame     │                                                   │
│  ├─────────────────┤                                                   │
│  │ x: 0x????????   │ ← Garbage from previous function call            │
│  ├─────────────────┤                                                   │
│  │ return address  │                                                   │
│  └─────────────────┘                                                   │
│                                                                         │
│  IMPORTANT: Debug builds often zero-initialize for you.                │
│  Release builds don't. Bug appears only in production!                 │
│                                                                         │
│  Global/static variables ARE zero-initialized (by the loader).         │
│  int g_count;  // Guaranteed to be 0                                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Uninitialized variables are `undefined`
let x;
console.log(x);  // undefined (predictable, safe)

// TypeScript: Catches uninitialized variables at compile time
let x: number;
console.log(x);  // Error: Variable 'x' is used before being assigned

// C: No safety net. You get garbage. Might work in debug, crash in release.
```

### 🟢 THE FIX

```c
// Fix 1: Always initialize at declaration
int x = 0;
int* ptr = NULL;
char buffer[100] = {0};  // Zero-fills entire array

// Fix 2: Use compound literals for structs (C99+)
Player player = {0};  // All fields zeroed
// Or:
Player player = {.x = 100, .y = 200, .health = 100};

// Fix 3: Use memset for dynamic memory
void* mem = malloc(1024);
if (mem) {
    memset(mem, 0, 1024);
}

// Fix 4: calloc instead of malloc (zeros memory)
int* array = calloc(100, sizeof(int));  // 100 zeroed ints

// Fix 5: Compiler warnings
// -Wuninitialized -Wmaybe-uninitialized (gcc)
// -Wconditional-uninitialized (clang)
```

---

## 8. Array Bounds (No Runtime Checks)

### 🔴 THE TRAP

```c
int arr[10];

arr[10] = 5;   // OUT OF BOUNDS! No error, no warning, just corruption
arr[-1] = 3;   // Also valid syntax! Corrupts memory before array
arr[1000] = 0; // Might crash, might corrupt, might "work"

for (int i = 0; i <= 10; i++) {  // Off-by-one: 11 iterations!
    arr[i] = i;  // arr[10] corrupts memory
}
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ARRAY INDEXING IN C                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  arr[i] is LITERALLY just pointer arithmetic:                          │
│  arr[i] ≡ *(arr + i)                                                   │
│                                                                         │
│  int arr[10];                                                          │
│                                                                         │
│  Memory:                                                                │
│  Address:  0x1000  0x1004  0x1008  ...  0x1024  0x1028                 │
│  Index:    [0]     [1]     [2]    ...   [9]     [10]                   │
│            └─────────────────────────┘  └──────────────┘               │
│            Valid array (40 bytes)       BEYOND ARRAY!                  │
│                                                                         │
│  arr[10] = *(0x1000 + 10*4) = *(0x1028)                                │
│  This address belongs to something else (stack variable, return addr)  │
│                                                                         │
│  C DOES NOT CHECK. It just does the math and accesses memory.          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Out-of-bounds returns undefined, doesn't corrupt
const arr = [1, 2, 3];
console.log(arr[100]); // undefined (safe)
arr[100] = 5; // Creates sparse array (weird but safe)

// C: Out-of-bounds corrupts memory. No exceptions. No errors.
// This is how buffer overflow attacks work!
```

### 🟢 THE FIX

```c
// Fix 1: Use constants for array sizes
#define ARRAY_SIZE 10
int arr[ARRAY_SIZE];

for (int i = 0; i < ARRAY_SIZE; i++) {  // Note: < not <=
    arr[i] = i;
}

// Fix 2: sizeof trick for static arrays
int arr[10];
size_t len = sizeof(arr) / sizeof(arr[0]);  // = 10

// Fix 3: Always pass length with arrays
void process_array(int* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // Safe: i is always < len
    }
}

// Fix 4: Use AddressSanitizer during development
// Compile with: -fsanitize=address
// It will catch out-of-bounds at runtime!

// Fix 5: Wrap access in bounds-checking macro (debug builds)
#ifdef DEBUG
#define ARRAY_GET(arr, i, max) \
    ((i) < (max) ? (arr)[i] : (assert(0), (arr)[0]))
#else
#define ARRAY_GET(arr, i, max) ((arr)[i])
#endif
```

---

## 9. String Handling Nightmares

### 🔴 THE TRAP

```c
// Trap 1: Forgetting null terminator
char name[5] = "Hello";  // No room for '\0'!
printf("%s", name);      // Prints "Hello" + garbage until random '\0'

// Trap 2: Buffer overflow with strcpy
char buf[10];
strcpy(buf, "This is way too long for the buffer");  // OVERFLOW!

// Trap 3: Comparing strings with ==
char* a = "hello";
char* b = "hello";
if (a == b) { ... }      // Might work (string interning)

char c[10] = "hello";
if (a == c) { ... }      // ALWAYS FALSE! Comparing addresses!

// Trap 4: Modifying string literals
char* str = "hello";
str[0] = 'H';            // CRASH or undefined behavior!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    STRINGS IN C                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  "String" = array of chars ending with '\0' (null terminator)          │
│                                                                         │
│  char str[] = "Hi";                                                    │
│  Memory: ['H']['i']['\0']                                              │
│           0    1    2      (3 bytes total!)                            │
│                                                                         │
│  String literal vs array:                                              │
│  ┌─────────────────────────────────────────────────────────┐           │
│  │ char* ptr = "hello";    // Points to READ-ONLY memory   │           │
│  │ char arr[] = "hello";   // Copies to stack (writable)   │           │
│  └─────────────────────────────────────────────────────────┘           │
│                                                                         │
│  String functions DON'T know buffer size:                              │
│  strcpy(dst, src);  // Copies until '\0', ignores dst size!            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Strings are immutable, auto-managed
const str = "hello";
const str2 = str.toUpperCase(); // Creates new string
// No buffer sizes, no null terminators, no overflow possible

// C: You manage everything manually
// It's like having to manage your own StringBuilder
// with explicit capacity and null termination
```

### 🟢 THE FIX

```c
// Fix 1: Use strncpy with explicit size (still has gotchas!)
char buf[10];
strncpy(buf, "hello world", sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';  // strncpy might not null-terminate!

// Fix 2: Use snprintf (safer, always null-terminates)
char buf[10];
snprintf(buf, sizeof(buf), "%s", "hello world");  // Truncates safely

// Fix 3: Compare strings with strcmp
if (strcmp(a, b) == 0) {
    printf("Strings are equal\n");
}

// Fix 4: Use arrays for modifiable strings
char str[] = "hello";  // Writable copy
str[0] = 'H';          // OK!

// Fix 5: Calculate required size
const char* src = "hello";
size_t needed = strlen(src) + 1;  // +1 for '\0'
char* dst = malloc(needed);
if (dst) {
    memcpy(dst, src, needed);
}

// Fix 6: Use strlcpy if available (BSD/macOS, not standard C)
// Or implement your own safe string functions
```

---

## 10. Pointer Arithmetic Mistakes

### 🔴 THE TRAP

```c
int arr[10];
int* ptr = arr;

// Trap 1: Thinking ptr++ adds 1 byte
ptr++;  // Actually adds sizeof(int) = 4 bytes!

// Trap 2: Wrong pointer type
int* int_ptr = arr;
char* char_ptr = (char*)arr;

int_ptr++;   // Moves 4 bytes
char_ptr++;  // Moves 1 byte
// They're now pointing at different places!

// Trap 3: Pointer difference confusion
int* a = &arr[2];
int* b = &arr[5];
ptrdiff_t diff = b - a;  // = 3 (elements), not 12 (bytes)!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    POINTER ARITHMETIC                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ptr + n  =  (address of ptr) + n * sizeof(*ptr)                       │
│                                                                         │
│  int arr[5];     // Each int is 4 bytes                                │
│  int* ptr = arr; // ptr = 0x1000                                       │
│                                                                         │
│  Address:  0x1000  0x1004  0x1008  0x100C  0x1010                      │
│  Index:    [0]     [1]     [2]     [3]     [4]                         │
│                                                                         │
│  ptr + 1 = 0x1000 + 1*4 = 0x1004  (points to arr[1])                   │
│  ptr + 3 = 0x1000 + 3*4 = 0x100C  (points to arr[3])                   │
│                                                                         │
│  Pointer subtraction returns ELEMENT count, not byte count:            │
│  &arr[3] - &arr[0] = 3  (not 12!)                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// Fix 1: Use array indexing when possible (clearer)
for (int i = 0; i < 10; i++) {
    arr[i] = i;  // Clearer than *(arr + i)
}

// Fix 2: Be explicit about byte offsets
char* byte_ptr = (char*)arr;
byte_ptr += sizeof(int);  // Explicitly move by sizeof

// Fix 3: Use ptrdiff_t for pointer differences
int* start = arr;
int* end = arr + 10;
ptrdiff_t count = end - start;  // 10 elements

// Fix 4: Use uintptr_t for raw address math (rare, low-level)
#include <stdint.h>
uintptr_t addr = (uintptr_t)ptr;
addr += 4;  // Raw byte offset
ptr = (int*)addr;
```

### 🔴 REAL-WORLD TRAP: Pointer Type Determines Write Size

This trap is especially common in **graphics/pixel manipulation**:

```c
// You have a pixel buffer and want to write a 32-bit RGBA color
uint8 *pixel_pos = buffer + offset;  // Points to correct byte
uint32 color = 0xFFFFFFFF;           // White (RGBA)

// THE TRAP: Dereferencing through wrong pointer type!
*pixel_pos = color;  // ❌ WRONG! Only writes 1 byte!

// What happens:
// color = 0xFFFFFFFF (4 bytes: FF FF FF FF)
// *pixel_pos = 0xFF (truncated to 1 byte!)
// Result: Only the first byte is written, color is corrupted!
```

**Memory Visualization:**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    POINTER TYPE DETERMINES WRITE SIZE                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  color = 0xFF00FF00 (Green in RGBA)                                    │
│                                                                         │
│  WRONG: *pixel_pos = color; (pixel_pos is uint8*)                      │
│  ┌────────────────────────────────────────┐                            │
│  │ Before: [??][??][??][??]               │                            │
│  │ After:  [00][??][??][??]  ← Only 1 byte written (truncated!)       │
│  └────────────────────────────────────────┘                            │
│                                                                         │
│  RIGHT: *(uint32*)pixel_pos = color;                                   │
│  ┌────────────────────────────────────────┐                            │
│  │ Before: [??][??][??][??]               │                            │
│  │ After:  [00][FF][00][FF]  ← All 4 bytes written correctly          │
│  └────────────────────────────────────────┘                            │
│                                                                         │
│  The POINTER TYPE tells C how many bytes to read/write!                │
│  uint8*  → 1 byte                                                      │
│  uint16* → 2 bytes                                                     │
│  uint32* → 4 bytes                                                     │
│  uint64* → 8 bytes                                                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX: Cast Before Dereferencing

```c
// Method 1: Cast when dereferencing (most common in pixel loops)
uint8 *pixel_pos = buffer + (y * pitch) + (x * bytes_per_pixel);
*(uint32 *)pixel_pos = color;  // Cast to uint32* THEN dereference

// Method 2: Use the correct pointer type from the start
uint32 *pixels = (uint32 *)buffer;
pixels[y * width + x] = color;  // Direct 32-bit access

// Method 3: For row-by-row iteration (Handmade Hero style)
uint8 *row = (uint8 *)buffer;
for (int y = 0; y < height; ++y) {
    uint32 *pixel = (uint32 *)row;  // Cast row to uint32*
    for (int x = 0; x < width; ++x) {
        *pixel++ = color;  // Write 4 bytes, advance by 4
    }
    row += pitch;  // Advance by pitch (may include padding)
}
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: TypedArrays enforce the view size
const buffer = new ArrayBuffer(16);
const uint8View = new Uint8Array(buffer);
const uint32View = new Uint32Array(buffer);

uint8View[0] = 0xffffffff; // Truncated to 0xFF (1 byte)
uint32View[0] = 0xffffffff; // Full 4 bytes written

// In C, you must EXPLICITLY cast to get the right behavior
// There's no separate "view" - the pointer type IS the view
```

### 📋 COMMON SCENARIOS

| Pointer Type | Dereference Writes | Use Case                |
| ------------ | ------------------ | ----------------------- |
| `uint8 *`    | 1 byte             | Raw byte manipulation   |
| `int16 *`    | 2 bytes            | Audio samples (16-bit)  |
| `uint32 *`   | 4 bytes            | Pixels (RGBA), colors   |
| `float *`    | 4 bytes            | Single-precision floats |
| `double *`   | 8 bytes            | Double-precision floats |

**Rule:** When writing multi-byte values through a byte pointer,
**always cast to the appropriate pointer type first!**

---

## 11. Memory Leaks and Use-After-Free

### 🔴 THE TRAP

```c
// Trap 1: Memory leak (forgetting to free)
void process() {
    char* data = malloc(1024);
    if (error_condition) {
        return;  // LEAK! data never freed
    }
    free(data);
}

// Trap 2: Use-after-free
char* get_name() {
    char* name = malloc(100);
    strcpy(name, "Alice");
    free(name);
    return name;  // DANGLING POINTER! Memory is freed!
}

// Trap 3: Double-free
char* ptr = malloc(100);
free(ptr);
free(ptr);  // CRASH or heap corruption!

// Trap 4: Freeing stack memory
void bad() {
    char buffer[100];
    free(buffer);  // CRASH! buffer is on stack, not heap!
}
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MEMORY LIFECYCLE IN C                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  malloc(size)  →  Returns pointer to heap memory                       │
│       │            Memory is UNINITIALIZED (garbage)                   │
│       ↓                                                                │
│  Use the memory                                                        │
│       │                                                                │
│       ↓                                                                │
│  free(ptr)     →  Memory returned to heap                              │
│       │            Pointer is now DANGLING (points to freed memory)    │
│       ↓                                                                │
│  ptr = NULL    →  Explicitly invalidate (good practice)                │
│                                                                         │
│  RULES:                                                                │
│  • Every malloc MUST have exactly ONE free                             │
│  • Never use memory after free                                         │
│  • Never free memory twice                                             │
│  • Only free what malloc/calloc/realloc returned                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Garbage collector handles everything
function process() {
  const data = new Array(1000);
  if (errorCondition) {
    return; // data is automatically garbage collected
  }
}
// No leaks possible (in theory)

// C: YOU are the garbage collector
// Forget to free? Memory leaks.
// Free twice? Heap corruption.
// Use after free? Security vulnerability!
```

### 🟢 THE FIX

```c
// Fix 1: Single exit point pattern
void process() {
    char* data = malloc(1024);
    if (!data) return;

    int result = 0;

    if (condition1) {
        result = -1;
        goto cleanup;  // Yes, goto is OK for cleanup!
    }

    if (condition2) {
        result = -2;
        goto cleanup;
    }

    // Normal processing...

cleanup:
    free(data);
    return result;
}

// Fix 2: Set pointer to NULL after free
free(ptr);
ptr = NULL;  // Now if you accidentally use it, you'll crash immediately

// Fix 3: Use wrapper functions
void safe_free(void** ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

// Fix 4: Use AddressSanitizer
// Compile with: -fsanitize=address
// Detects: leaks, use-after-free, double-free, buffer overflow

// Fix 5: Use Valgrind during testing
// valgrind --leak-check=full ./your_program
```

---

## 12. Macro Pitfalls

### 🔴 THE TRAP

```c
// Trap 1: Missing parentheses
#define SQUARE(x) x * x
int a = SQUARE(1 + 2);  // Expands to: 1 + 2 * 1 + 2 = 5, not 9!

// Trap 2: Multiple evaluation
#define MAX(a, b) ((a) > (b) ? (a) : (b))
int x = MAX(i++, j++);  // i or j gets incremented TWICE!

// Trap 3: Semicolon after macro
#define LOG(msg) printf("%s\n", msg);
if (debug)
    LOG("test");  // Fine
else
    LOG("other"); // SYNTAX ERROR due to double semicolon!

// Trap 4: Macros in unexpected contexts
#define BEGIN {
#define END }
// Breaks syntax highlighting, confuses debuggers
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MACROS = TEXT SUBSTITUTION                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Preprocessor does DUMB text replacement before compilation:           │
│                                                                         │
│  #define SQUARE(x) x * x                                               │
│                                                                         │
│  SQUARE(1 + 2)                                                         │
│     ↓ text substitution                                                │
│  1 + 2 * 1 + 2                                                         │
│     ↓ normal precedence                                                │
│  1 + (2 * 1) + 2 = 5                                                   │
│                                                                         │
│  No type checking, no scoping, no debugging visibility                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// Fix 1: ALWAYS parenthesize parameters AND result
#define SQUARE(x) ((x) * (x))
int a = SQUARE(1 + 2);  // ((1 + 2) * (1 + 2)) = 9 ✓

// Fix 2: Use do-while(0) for multi-statement macros
#define LOG(msg) do { \
    printf("[LOG] %s\n", msg); \
} while(0)

if (debug)
    LOG("test");  // Works correctly
else
    LOG("other"); // Also works!

// Fix 3: Use inline functions instead (when possible)
static inline int square(int x) {
    return x * x;
}
// Type-safe, single evaluation, debuggable

// Fix 4: Use _Generic for type-generic functions (C11)
#define MAX(a, b) _Generic((a), \
    int: max_int, \
    float: max_float, \
    double: max_double \
)(a, b)

// Fix 5: Use GCC's statement expressions for complex macros
#define MAX_SAFE(a, b) ({ \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b; \
})
// Evaluates each argument exactly once
```

---

## 13. sizeof() Surprises

### 🔴 THE TRAP

```c
// Trap 1: sizeof array parameter
void func(int arr[10]) {
    printf("%zu\n", sizeof(arr));  // Prints 8 (pointer size), not 40!
}

// Trap 2: sizeof pointer vs array
int arr[10];
int* ptr = arr;
sizeof(arr);  // 40 (10 * 4 bytes)
sizeof(ptr);  // 8 (pointer size on 64-bit)

// Trap 3: sizeof struct has padding
struct Bad {
    char a;    // 1 byte
    int b;     // 4 bytes
};
// sizeof(Bad) = 8, not 5! (3 bytes padding)

// Trap 4: sizeof string literal
sizeof("hello");  // 6, includes null terminator
strlen("hello");  // 5, does NOT include null terminator
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    sizeof() BEHAVIOR                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  sizeof is a COMPILE-TIME operator (mostly)                            │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │ int arr[10];                                                      │ │
│  │ sizeof(arr) = 40  (compiler knows array size)                     │ │
│  │                                                                   │ │
│  │ void func(int arr[10]) {  // The [10] is a LIE!                   │ │
│  │     sizeof(arr) = 8   // It's actually a pointer                  │ │
│  │ }                                                                 │ │
│  │                                                                   │ │
│  │ When array is passed to function, it DECAYS to pointer           │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                                                         │
│  Struct padding:                                                       │
│  struct { char a; int b; }                                             │
│  Memory: [a][pad][pad][pad][b][b][b][b]                                │
│          1  + 3 padding  + 4 = 8 bytes                                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// Fix 1: Pass array size explicitly
void func(int* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // Use arr[i]
    }
}

// Fix 2: Use macro for array length (only works on actual arrays!)
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

int arr[10];
size_t len = ARRAY_LEN(arr);  // 10 ✓

// But this FAILS:
void func(int arr[10]) {
    size_t len = ARRAY_LEN(arr);  // WRONG! Gets pointer size / int size
}

// Fix 3: Pack structs when needed (careful: impacts performance)
#pragma pack(push, 1)
struct Packed {
    char a;
    int b;
};
#pragma pack(pop)
// sizeof(Packed) = 5, but unaligned access is slow!

// Fix 4: Use offsetof to understand padding
#include <stddef.h>
offsetof(struct Bad, a);  // 0
offsetof(struct Bad, b);  // 4 (not 1!)
```

---

## 14. Floating Point Comparison

### 🔴 THE TRAP

```c
float a = 0.1f + 0.2f;
if (a == 0.3f) {
    printf("Equal!\n");  // NEVER PRINTS!
}
// a is actually 0.30000001192092896...

// Trap 2: Accumulation errors
float sum = 0.0f;
for (int i = 0; i < 1000000; i++) {
    sum += 0.0001f;
}
// sum is NOT 100.0, it's something like 100.13278...
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    FLOATING POINT REPRESENTATION                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Floats store numbers in binary scientific notation:                   │
│  value = sign × mantissa × 2^exponent                                  │
│                                                                         │
│  Problem: Many decimal numbers can't be represented exactly!           │
│                                                                         │
│  0.1 in decimal = 0.0001100110011001100110011... in binary (repeating) │
│                                                                         │
│  float (32-bit):  ~7 decimal digits precision                          │
│  double (64-bit): ~15 decimal digits precision                         │
│                                                                         │
│  0.1f = 0.100000001490116119384765625                                  │
│  0.2f = 0.200000002980232238769531250                                  │
│  0.1f + 0.2f = 0.300000011920928955078125                              │
│  0.3f = 0.300000011920928955078125... wait, same? NO!                  │
│  0.3f = 0.300000011920928955078125 (different rounding!)               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Same problem exists!
0.1 + 0.2 === 0.3; // false!
0.1 + 0.2; // 0.30000000000000004

// You've probably hit this before in JS
// The solution in C is similar: use epsilon comparison
```

### 🟢 THE FIX

```c
#include <math.h>

// Fix 1: Epsilon comparison
#define EPSILON 0.00001f

bool float_equal(float a, float b) {
    return fabsf(a - b) < EPSILON;
}

if (float_equal(a, 0.3f)) {
    printf("Equal!\n");  // Now works
}

// Fix 2: Relative epsilon (for numbers of varying magnitude)
bool float_equal_relative(float a, float b, float epsilon) {
    float diff = fabsf(a - b);
    float largest = fmaxf(fabsf(a), fabsf(b));
    return diff <= largest * epsilon;
}

// Fix 3: Use integers when possible
// Instead of: float price = 19.99f;
// Use:        int price_cents = 1999;

// Fix 4: Use double for more precision
double a = 0.1 + 0.2;
// Still not exact, but more precise

// Fix 5: For exact decimal math, use fixed-point or decimal libraries
// No standard C solution - consider libdecimal or manual fixed-point
```

---

## 15. Short-Circuit Evaluation Side Effects

### 🔴 THE TRAP

```c
int i = 0;

// Trap 1: Side effect not executed
if (false && i++) {
    // i++ NEVER executes because && short-circuits!
}
// i is still 0

// Trap 2: Order of evaluation in function calls
int x = 0;
printf("%d %d\n", x++, x++);
// UNDEFINED BEHAVIOR! Order of argument evaluation is unspecified

// Trap 3: Relying on evaluation order in expressions
int arr[3] = {1, 2, 3};
int i = 0;
int value = arr[i++] + arr[i++];  // UNDEFINED!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SHORT-CIRCUIT EVALUATION                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  && (AND): If left side is false, right side NEVER evaluates           │
│  || (OR):  If left side is true, right side NEVER evaluates            │
│                                                                         │
│  This is USEFUL for null checks:                                       │
│  if (ptr && ptr->value > 0)  // Safe: ptr->value only if ptr != NULL  │
│                                                                         │
│  But DANGEROUS with side effects:                                      │
│  if (condition && count++)   // count++ might not execute!             │
│                                                                         │
│  EVALUATION ORDER:                                                      │
│  • && and || guarantee left-to-right                                   │
│  • Function arguments: UNSPECIFIED order                               │
│  • Most operators: UNSPECIFIED order                                   │
│  • Between sequence points: modifying twice is UNDEFINED               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// Fix 1: Don't put side effects in conditions
int i = 0;
if (condition) {
    // work
}
i++;  // Side effect separate from condition

// Fix 2: Use separate statements for function arguments
int a = x++;
int b = x++;
printf("%d %d\n", a, b);  // Well-defined

// Fix 3: One modification per statement
int arr[3] = {1, 2, 3};
int i = 0;
int val1 = arr[i];
i++;
int val2 = arr[i];
i++;
int value = val1 + val2;  // Well-defined

// Fix 4: Use the comma operator carefully (has defined order)
int i = 0;
int a = (i++, i++, i);  // i = 2, a = 2 (but please don't write this!)
```

---

## 16. Buffer Overflows

### 🔴 THE TRAP

```c
// Trap 1: gets() - NEVER USE THIS
char buffer[10];
gets(buffer);  // No size limit! Removed from C11 standard

// Trap 2: sprintf without size
char buf[20];
sprintf(buf, "User: %s, Score: %d", username, score);
// If username is long, OVERFLOW!

// Trap 3: scanf with %s
char name[10];
scanf("%s", name);  // No size limit!

// Trap 4: Forgetting space for null terminator
char buf[5];
strncpy(buf, "hello", 5);  // No room for '\0'!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    BUFFER OVERFLOW                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Stack layout:                                                          │
│  ┌─────────────────┐                                                   │
│  │ buffer[10]      │ ← Your data goes here                             │
│  ├─────────────────┤                                                   │
│  │ saved_ebp       │ ← Overflow corrupts this                          │
│  ├─────────────────┤                                                   │
│  │ return address  │ ← Attacker controls this = arbitrary code exec!  │
│  └─────────────────┘                                                   │
│                                                                         │
│  Writing past buffer end:                                              │
│  1. Corrupts adjacent variables                                         │
│  2. Corrupts saved frame pointer                                        │
│  3. Corrupts return address (security vulnerability!)                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// Fix 1: Use fgets instead of gets
char buffer[100];
fgets(buffer, sizeof(buffer), stdin);

// Fix 2: Use snprintf instead of sprintf
char buf[20];
snprintf(buf, sizeof(buf), "User: %s", username);

// Fix 3: Use width specifier with scanf
char name[10];
scanf("%9s", name);  // Max 9 chars + null terminator

// Fix 4: Always account for null terminator
char buf[6];  // 5 chars + 1 null
strncpy(buf, "hello", sizeof(buf) - 1);
buf[sizeof(buf) - 1] = '\0';

// Fix 5: Use safer functions where available
// strlcpy, strlcat (BSD)
// strcpy_s, strcat_s (C11 Annex K, but poorly supported)

// Fix 6: Calculate sizes explicitly
size_t needed = strlen(prefix) + strlen(suffix) + 1;
char* result = malloc(needed);
if (result) {
    snprintf(result, needed, "%s%s", prefix, suffix);
}

// Fix 7: Compile with stack protectors
// -fstack-protector-strong (GCC/Clang)
```

---

## 17. Null Pointer Dereference

### 🔴 THE TRAP

```c
// Trap 1: Not checking malloc return
int* arr = malloc(1000000000 * sizeof(int));
arr[0] = 5;  // CRASH if malloc returned NULL!

// Trap 2: Assuming function success
FILE* f = fopen("nonexistent.txt", "r");
fscanf(f, "%d", &x);  // CRASH if file doesn't exist!

// Trap 3: Returning pointer to freed memory
Node* find_node(List* list, int value) {
    // ... search ...
    if (not_found) {
        return NULL;
    }
    return found_node;
}

Node* n = find_node(list, 42);
n->value = 100;  // CRASH if not found!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    NULL POINTER DEREFERENCE                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  NULL = (void*)0 = address zero                                        │
│                                                                         │
│  Memory address 0 is typically:                                        │
│  • Unmapped (protected by OS)                                          │
│  • Accessing it causes SIGSEGV (segmentation fault)                    │
│                                                                         │
│  WHY functions return NULL:                                            │
│  • malloc: Out of memory                                               │
│  • fopen: File doesn't exist                                           │
│  • find/search: Element not found                                       │
│  • strstr: Substring not found                                         │
│                                                                         │
│  In C, you MUST check every pointer that could be NULL!                │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript/TypeScript: null/undefined is a common error source
const user = findUser(id);
console.log(user.name); // TypeError: Cannot read property 'name' of null

// TypeScript helps: user: User | null
// Optional chaining: user?.name
// Nullish coalescing: user?.name ?? 'Unknown'

// C: No such safety. You must check explicitly.
```

### 🟢 THE FIX

```c
// Fix 1: ALWAYS check malloc
int* arr = malloc(size);
if (!arr) {
    fprintf(stderr, "Out of memory!\n");
    return -1;  // Or handle error appropriately
}

// Fix 2: Check file operations
FILE* f = fopen(filename, "r");
if (!f) {
    perror("fopen");  // Prints: "fopen: No such file or directory"
    return -1;
}

// Fix 3: Check function returns before use
Node* n = find_node(list, 42);
if (n) {
    n->value = 100;  // Safe
} else {
    // Handle not found
}

// Fix 4: Use assert for programming errors (debug only)
#include <assert.h>
void process(Data* data) {
    assert(data != NULL);  // Crashes in debug if NULL
    // ... use data ...
}

// Fix 5: Establish conventions
// Document: "Returns NULL if not found"
// Or: Use error codes + output parameters

// Fix 6: Defensive programming with early return
void process_data(Data* data) {
    if (!data) return;  // Guard clause

    if (!data->buffer) return;

    // Now safe to use data and data->buffer
}
```

---

## 18. Stack vs Heap Lifetime

### 🔴 THE TRAP

```c
// Trap 1: Returning pointer to local variable
char* get_greeting() {
    char buffer[100];
    strcpy(buffer, "Hello, World!");
    return buffer;  // DANGLING POINTER! buffer dies when function returns!
}

// Trap 2: Storing pointer to temporary
int* bad_array() {
    int arr[] = {1, 2, 3, 4, 5};
    return arr;  // arr is on stack, dies after return!
}

// Trap 3: Escaping reference to inner scope
int* ptr;
{
    int x = 42;
    ptr = &x;
}
// x is out of scope here, ptr is dangling
*ptr = 10;  // UNDEFINED BEHAVIOR
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    STACK vs HEAP LIFETIME                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  STACK: Automatic storage duration                                     │
│  ───────────────────────────────────                                   │
│  • Lives until function returns                                        │
│  • Local variables, function parameters                                │
│  • CANNOT outlive function scope                                       │
│                                                                         │
│  void foo() {                                                          │
│      int x = 42;      ← Born when foo() called                         │
│      // use x                                                          │
│  }                    ← x DIES when foo() returns                      │
│                                                                         │
│  HEAP: Dynamic storage duration                                        │
│  ──────────────────────────────                                        │
│  • Lives until explicitly freed                                        │
│  • malloc/calloc/realloc allocated                                     │
│  • CAN outlive function scope                                          │
│                                                                         │
│  int* create_int() {                                                   │
│      int* p = malloc(sizeof(int));  ← Born when malloc called          │
│      *p = 42;                                                          │
│      return p;  ← p (local) dies, but *p (heap memory) lives!          │
│  }                                                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Everything is on the "heap" (managed by GC)
function getGreeting() {
  const buffer = "Hello, World!"; // Object lives as long as referenced
  return buffer; // Safe! GC keeps it alive
}

// C: You must know WHERE memory lives and HOW LONG it lives
```

### 🟢 THE FIX

```c
// Fix 1: Allocate on heap if it needs to outlive function
char* get_greeting() {
    char* buffer = malloc(100);
    if (buffer) {
        strcpy(buffer, "Hello, World!");
    }
    return buffer;  // Caller must free!
}

// Fix 2: Use static for persistent local storage (careful: not thread-safe!)
const char* get_greeting() {
    static char buffer[100];  // Lives for program lifetime
    strcpy(buffer, "Hello, World!");
    return buffer;  // Valid, but will be overwritten on next call!
}

// Fix 3: Pass buffer from caller (best practice)
void get_greeting(char* buffer, size_t size) {
    snprintf(buffer, size, "Hello, World!");
}

// Usage:
char my_buffer[100];
get_greeting(my_buffer, sizeof(my_buffer));

// Fix 4: Use strdup for strings (caller must free)
#define _POSIX_C_SOURCE 200809L  // For strdup
#include <string.h>

char* get_greeting() {
    return strdup("Hello, World!");  // malloc's and copies
}

// Fix 5: Document ownership clearly
/**
 * Creates a new greeting string.
 * @return Heap-allocated string. Caller must free().
 */
char* create_greeting(void);
```

---

## 19. Undefined Behavior (The Silent Killer)

### 🔴 THE TRAP

```c
// These are all UNDEFINED BEHAVIOR:

int x = INT_MAX + 1;           // Signed overflow
int arr[10]; arr[10] = 5;      // Out of bounds
int y = 1 / 0;                 // Division by zero
int* p = NULL; *p = 5;         // Null dereference
int a = i++ + i++;             // Multiple modifications without sequence point
free(ptr); *ptr = 5;           // Use after free
int b = (int)3.5e20;           // Float-to-int overflow
int c = -1 << 1;               // Left shift of negative value
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    UNDEFINED BEHAVIOR (UB)                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  UB means: "The C standard says anything can happen"                   │
│                                                                         │
│  Common misconceptions:                                                │
│  ❌ "It will crash" - Maybe, maybe not                                  │
│  ❌ "It will wrap around" - Only unsigned, not signed                   │
│  ❌ "It works on my machine" - Might fail on different compiler/OS     │
│                                                                         │
│  What compilers do with UB:                                            │
│  • Assume it never happens                                             │
│  • Optimize based on that assumption                                   │
│  • Generate code that does ANYTHING                                    │
│                                                                         │
│  Example: Compiler sees (x + 1 > x) for signed int x                   │
│  Compiler thinks: "This is always true (UB if overflow)"               │
│  Compiler optimizes to: always true                                    │
│  Your overflow check disappears!                                       │
│                                                                         │
│  UB can:                                                               │
│  • Work perfectly (until it doesn't)                                   │
│  • Crash                                                               │
│  • Corrupt data silently                                               │
│  • Execute arbitrary code (security vulnerability)                     │
│  • Make code behave differently with optimization levels               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```javascript
// JavaScript: Very few UBs, most errors are well-defined
const x = 1 / 0; // Infinity (defined!)
const y = 0 / 0; // NaN (defined!)
const z = null.foo; // TypeError (defined exception!)

// C: Many operations are UB
// The language spec says "we don't know what happens"
// It's not "implementation-defined" (where each compiler documents behavior)
// It's "literally anything can happen"
```

### 🟢 THE FIX

```c
// Fix 1: Use compiler sanitizers to detect UB
// -fsanitize=undefined
// -fsanitize=integer (for integer overflow)
// -fsanitize=address (for memory errors)

// Fix 2: Use safe arithmetic functions
#include <stdckdint.h>  // C23
bool overflow = ckd_add(&result, a, b);
if (overflow) {
    // Handle overflow
}

// Or manual checks:
if (b > 0 && a > INT_MAX - b) {
    // Would overflow
}

// Fix 3: Use unsigned for modular arithmetic
unsigned int x = UINT_MAX;
x++;  // x = 0 (defined behavior: wraps around)

// Fix 4: Initialize everything
int x = 0;  // Not: int x;

// Fix 5: Enable all warnings
// -Wall -Wextra -Wpedantic -Werror

// Fix 6: Use static analyzers
// clang-tidy, cppcheck, PVS-Studio

// Fix 7: List of common UB to avoid:
// • Signed integer overflow
// • Null pointer dereference
// • Out-of-bounds array access
// • Use after free
// • Double free
// • Division by zero
// • Shift by negative or >= width
// • Reading uninitialized variables
// • Violating strict aliasing
```

---

## 20. Platform-Specific Gotchas

### 🔴 THE TRAP

```c
// Trap 1: Assuming int size
int x = 0xDEADBEEF12345678;  // Truncated on 32-bit!

// Trap 2: Assuming pointer size
printf("%d", ptr);  // Wrong format specifier, UB

// Trap 3: Assuming endianness
int x = 0x01020304;
char* bytes = (char*)&x;
// bytes[0] could be 0x01 (big-endian) or 0x04 (little-endian)

// Trap 4: Assuming char signedness
char c = 200;
if (c > 0) { ... }  // Might be false! (char could be signed)

// Trap 5: Alignment assumptions
char buffer[100];
int* p = (int*)(buffer + 1);  // Unaligned! Crash on some architectures!

// Trap 6: Assuming path separators
const char* path = "folder\\file.txt";  // Windows-only!
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TYPE SIZES ARE NOT FIXED!                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  C only guarantees MINIMUM sizes:                                      │
│                                                                         │
│  Type          Minimum    32-bit Linux   64-bit Linux   64-bit Windows │
│  ──────────────────────────────────────────────────────────────────────│
│  char          8 bits     8 bits         8 bits         8 bits         │
│  short         16 bits    16 bits        16 bits        16 bits        │
│  int           16 bits    32 bits        32 bits        32 bits        │
│  long          32 bits    32 bits        64 bits        32 bits  ← !!  │
│  long long     64 bits    64 bits        64 bits        64 bits        │
│  void*         N/A        32 bits        64 bits        64 bits        │
│                                                                         │
│  ENDIANNESS:                                                           │
│  Big-endian:    Most significant byte first (network order)            │
│  Little-endian: Least significant byte first (x86, ARM)                │
│                                                                         │
│  0x01020304 in memory:                                                 │
│  Big-endian:    [01][02][03][04]                                       │
│  Little-endian: [04][03][02][01]                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🟢 THE FIX

```c
// Fix 1: Use fixed-width types for known sizes
#include <stdint.h>
int32_t x = 0x12345678;    // Always 32 bits
int64_t y = 0xDEADBEEF12345678LL;  // Always 64 bits
uint8_t byte = 200;        // Always unsigned 8 bits

// Fix 2: Use correct format specifiers
#include <inttypes.h>
printf("%" PRId32 "\n", x);        // int32_t
printf("%" PRIx64 "\n", y);        // int64_t as hex
printf("%p\n", (void*)ptr);        // Pointer
printf("%zu\n", sizeof(x));        // size_t

// Fix 3: Use standard types for sizes/differences
size_t size = sizeof(buffer);      // Unsigned, fits any object size
ptrdiff_t diff = ptr2 - ptr1;      // Signed, pointer difference
intptr_t addr = (intptr_t)ptr;     // Integer that can hold pointer

// Fix 4: Handle endianness explicitly
#include <arpa/inet.h>  // POSIX
uint32_t network_order = htonl(host_value);  // Host to network
uint32_t host_order = ntohl(network_value);  // Network to host

// Fix 5: Use explicit signedness
signed char sc = -50;      // Explicitly signed
unsigned char uc = 200;    // Explicitly unsigned

// Fix 6: Align properly
#include <stdalign.h>
alignas(int) char buffer[100];
int* p = (int*)buffer;  // Now properly aligned

// Fix 7: Use portable path handling
#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif
```

---

## Add new section at the end (before Summary):

````markdown:ai/c-pitfalls-for-web-devs.md
---

## 21. Macro Definition Order Across Compilation Units

### 🔴 THE TRAP

```c
// engine/input.h - Engine provides defaults
#ifndef DE100_GAME_BUTTON_COUNT
#define DE100_GAME_BUTTON_COUNT 0
#endif

#ifndef DE100_GAME_BUTTON_FIELDS
#define DE100_GAME_BUTTON_FIELDS GameButtonState ___DUMMY_NAME_TO_AVOID_WARNING;
#endif

typedef struct {
  // ... other fields ...
  union {
    GameButtonState buttons[DE100_GAME_BUTTON_COUNT];
    struct { DE100_GAME_BUTTON_FIELDS };
  };
  // ... other fields ...
} GameControllerInput;
````

```c
// game/inputs.h - Game defines its buttons
#define DE100_GAME_BUTTON_COUNT 12
#define DE100_GAME_BUTTON_FIELDS \
  GameButtonState move_up; \
  GameButtonState jump;    \
  // ... 10 more ...
```

```c
// game/main.c - Works! Includes game inputs first
#include "inputs.h"      // DE100_GAME_BUTTON_COUNT = 12 ✓
#include "engine/input.h" // #ifndef skipped ✓
```

```c
// engine/input.c - BROKEN! Never sees game's definitions
#include "input.h"  // DE100_GAME_BUTTON_COUNT = 0 ✗
// Array has 0 elements, game has 12 buttons = MEMORY CORRUPTION!
```

**Runtime output:**

```
new_ctrl_btns_size: 0
DE100_GAME_BUTTON_COUNT: 0
ASSERTION FAILED
```

### 🧠 THE MENTAL MODEL

```
┌─────────────────────────────────────────────────────────────────────────┐
│              SEPARATE COMPILATION = SEPARATE MACRO WORLDS               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Each .c file is compiled INDEPENDENTLY                                │
│  Macros defined in one .c file DON'T exist in another!                 │
│                                                                         │
│  Game Library Build:                                                   │
│  ┌─────────────────────────────────────────────────────────┐           │
│  │ main.c                                                  │           │
│  │   #include "inputs.h"  → DE100_GAME_BUTTON_COUNT = 12        │           │
│  │   #include "input.h"   → #ifndef skipped! ✓            │           │
│  └─────────────────────────────────────────────────────────┘           │
│                                                                         │
│  Platform/Engine Build (SEPARATE!):                                    │
│  ┌─────────────────────────────────────────────────────────┐           │
│  │ input.c                                                 │           │
│  │   #include "input.h"   → DE100_GAME_BUTTON_COUNT = 0 ✗       │           │
│  │   // Never saw inputs.h!                                │           │
│  └─────────────────────────────────────────────────────────┘           │
│                                                                         │
│  The game .so and platform executable have DIFFERENT struct sizes!     │
│                                                                         │
│  Game thinks:     GameControllerInput = 12 buttons + fields = ~104 bytes│
│  Platform thinks: GameControllerInput = 0 buttons + fields = ~24 bytes │
│                                                                         │
│  Result: Memory corruption, wrong button states, crashes               │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 🌐 WEB DEV ANALOGY

```typescript
// TypeScript/JavaScript: Imports are resolved at bundle time
// Everything sees the same definitions

// constants.ts
export const BUTTON_COUNT = 12;

// engine.ts
import { BUTTON_COUNT } from "./constants"; // Always 12!

// game.ts
import { BUTTON_COUNT } from "./constants"; // Always 12!

// Bundler ensures consistency. C has NO bundler!
```

```c
// C: Each file compiled separately
// No automatic sharing of #defines across .c files

// You MUST ensure each compilation unit sees the same definitions
// Either via:
//   1. Force-include (-include flag)
//   2. Shared header included everywhere
//   3. Build system coordination
```

### 🟢 THE FIX

**Option 1: Force-include via compiler flag (Recommended for engine/game separation)**

```bash
# Build script passes game's config to ALL compilation units
GAME_INPUT_HEADER="${GAME_DIR}/src/inputs.h"

# -include forces this header BEFORE any source processing
CFLAGS="${CFLAGS} -include ${GAME_INPUT_HEADER}"

# Now engine/input.c sees:
#   1. inputs.h (force-included) → DE100_GAME_BUTTON_COUNT = 12
#   2. input.h → #ifndef DE100_GAME_BUTTON_COUNT skipped!
```

```bash:build-common.sh
# Add to your build script where CFLAGS are set:
if [ -n "${GAME_INPUT_HEADER:-}" ] && [ -f "${GAME_INPUT_HEADER}" ]; then
    echo "Using game input header: ${GAME_INPUT_HEADER}"
    CFLAGS="${CFLAGS} -include ${GAME_INPUT_HEADER}"
fi
```

```bash:game/build-dev.sh
# Export before calling engine build:
export GAME_INPUT_HEADER="${SCRIPT_DIR}/src/inputs.h"
source "${ENGINE_DIR}/build-common.sh"
```

**Option 2: Compile-time size validation**

```c
// engine/input.h - Add static assert to catch mismatches
typedef struct {
  union {
    GameButtonState buttons[DE100_GAME_BUTTON_COUNT];
    struct { DE100_GAME_BUTTON_FIELDS };
  };
  // ...
} GameControllerInput;

// Verify struct size at compile time
_Static_assert(DE100_GAME_BUTTON_COUNT > 0,
    "DE100_GAME_BUTTON_COUNT not defined! Include game's inputs.h first.");
```

**Option 3: Auto-count buttons (no manual sync)**

```c
// game/inputs.h
#define DE100_GAME_BUTTON_FIELDS         \
  GameButtonState move_up;         \
  GameButtonState move_down;       \
  GameButtonState jump;            \
  GameButtonState attack;

// Auto-count using sizeof trick
typedef struct { DE100_GAME_BUTTON_FIELDS } _GameButtonsCounter;
#define DE100_GAME_BUTTON_COUNT (sizeof(_GameButtonsCounter) / sizeof(GameButtonState))

// Now count is ALWAYS correct - no manual "12" to maintain!
```

### 📋 SYMPTOMS OF THIS BUG

| Symptom                                 | Cause                                         |
| --------------------------------------- | --------------------------------------------- |
| `DE100_GAME_BUTTON_COUNT: 0` at runtime | Engine compiled without game's defines        |
| Some buttons work, others don't         | Struct size mismatch, memory overlap          |
| Buttons "stick" (never release)         | Wrong memory being read for button state      |
| Crash in input loop                     | Array bounds violation (0-size array)         |
| Works in game lib, fails in platform    | Different compilation units, different macros |

### 🔍 HOW TO DEBUG

```c
// Add to both game AND platform code:
printf("DE100_GAME_BUTTON_COUNT: %d\n", DE100_GAME_BUTTON_COUNT);
printf("sizeof(GameControllerInput): %zu\n", sizeof(GameControllerInput));

// If these print DIFFERENT values, you have the bug!
```

```bash
# Check what the compiler actually sees:
clang -E -dM input.c | grep GAME_BUTTON

# Should show your game's values, not engine defaults
```

### ⚠️ RELATED TRAP: Union Size Mismatch

```c
// If DE100_GAME_BUTTON_COUNT = 0 but DE100_GAME_BUTTON_FIELDS has 12 fields:
union {
  GameButtonState buttons[0];   // 0 bytes!
  struct { /* 12 fields */ };   // ~96 bytes
};

// Union takes size of LARGEST member = 96 bytes
// But buttons[0] means array access is ALWAYS out of bounds!

for (int i = 0; i < 12; i++) {
    buttons[i] = ...;  // Writing past array! Undefined behavior!
}
```

---

## 🎓 Summary: Top Rules for Web Devs Learning C

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    THE 10 COMMANDMENTS OF C                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. || returns 0 or 1, not the actual value                            │
│                                                                         │
│  2. int/int = int (truncated), not float                               │
│                                                                         │
│  3. signed + unsigned = unsigned (dangerous!)                          │
│                                                                         │
│  4. Arrays don't know their size when passed to functions              │
│                                                                         │
│  5. Strings need null terminators AND space for them                   │
│                                                                         │
│  6. Local variables die when the function returns                      │
│                                                                         │
│  7. malloc can fail - ALWAYS check the return value                    │
│                                                                         │
│  8. Every malloc needs exactly one free                                │
│                                                                         │
│  9. Uninitialized variables contain garbage                            │
│                                                                         │
│  10. Undefined behavior can make your program do ANYTHING              │
│                                                                         │
│  11. Macros don't cross compilation units - use -include flag          │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🛠️ Compiler Flags to Save Your Life

```bash
# Debug build - catch all the errors
gcc -Wall -Wextra -Wpedantic -Werror \
    -Wconversion -Wsign-conversion \
    -fsanitize=address,undefined \
    -g -O0 \
    main.c -o main

# Explanation:
# -Wall           : Enable most warnings
# -Wextra         : Enable even more warnings
# -Wpedantic      : Strict ISO C compliance
# -Werror         : Treat warnings as errors
# -Wconversion    : Warn on implicit conversions
# -Wsign-conversion : Warn on signed/unsigned conversions
# -fsanitize=address : Detect memory errors at runtime
# -fsanitize=undefined : Detect UB at runtime
# -g              : Include debug symbols
# -O0             : No optimization (easier debugging)
```

---

## 📚 Additional Resources

- [C FAQ](http://c-faq.com/) - Classic collection of C gotchas
- [SEI CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/) - Security-focused guidelines
- [What Every C Programmer Should Know About Undefined Behavior](https://blog.llvm.org/2011/05/what-every-c-programmer-should-know.html)
- [Compiler Explorer](https://godbolt.org/) - See what your C code compiles to

---

_Remember: In C, the compiler trusts you. Don't betray that trust._ 🤝
