# C Preprocessor & Macros: Complete Crash Course

## Table of Contents

1. [Basics](#1-basics)
2. [Object-like Macros](#2-object-like-macros)
3. [Function-like Macros](#3-function-like-macros)
4. [Stringification (#)](#4-stringification-)
5. [Token Pasting (##)](#5-token-pasting-)
6. [Variadic Macros](#6-variadic-macros)
7. [Conditional Compilation](#7-conditional-compilation)
8. [Predefined Macros](#8-predefined-macros)
9. [Include Guards](#9-include-guards)
10. [Pragma](#10-pragma)
11. [Advanced Techniques](#11-advanced-techniques)
12. [Common Patterns](#12-common-patterns)
13. [Dangers & Best Practices](#13-dangers--best-practices)

---

## 1. Basics

The C preprocessor runs **before** compilation. It's a text substitution system.

```c
// Preprocessor directives start with #
#define NAME value
#include <file.h>
#ifdef SOMETHING
#endif
```

**Compilation phases:**

```
Source Code → Preprocessor → Compiler → Assembler → Linker → Executable
    .c            .i           .s          .o
```

**See preprocessor output:**

```bash
# Output preprocessed code to file
clang -E main.c -o main.i

# Or to stdout
clang -E main.c
```

---

## 2. Object-like Macros

Simple text replacement.

### Basic Definition

```c
#define PI 3.14159265359
#define MAX_BUFFER_SIZE 1024
#define GAME_NAME "Handmade Hero"

// Usage
float circumference = 2 * PI * radius;
char buffer[MAX_BUFFER_SIZE];
printf("Welcome to %s\n", GAME_NAME);

// After preprocessing:
float circumference = 2 * 3.14159265359 * radius;
char buffer[1024];
printf("Welcome to %s\n", "Handmade Hero");
```

### Multi-line Macros

```c
// Use backslash to continue on next line
#define LONG_STRING "This is a very long string that \
spans multiple lines in the source code \
but is actually one continuous string"

// For code blocks
#define SWAP(a, b) \
    do { \
        int temp = (a); \
        (a) = (b); \
        (b) = temp; \
    } while(0)
```

### Empty Macros

```c
#define UNUSED
#define DEPRECATED

// Used for documentation or conditional compilation
DEPRECATED void old_function(void);
```

### Undefining Macros

```c
#define DEBUG 1

// ... code ...

#undef DEBUG  // Now DEBUG is undefined

#define DEBUG 0  // Can redefine
```

---

## 3. Function-like Macros

Macros that take arguments.

### Basic Syntax

```c
#define SQUARE(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Usage
int result = SQUARE(5);      // ((5) * (5)) = 25
int bigger = MAX(10, 20);    // ((10) > (20) ? (10) : (20)) = 20

// After preprocessing:
int result = ((5) * (5));
int bigger = ((10) > (20) ? (10) : (20));
```

### Why All The Parentheses?

```c
// BAD - no parentheses
#define SQUARE_BAD(x) x * x
#define DOUBLE_BAD(x) x + x

int a = SQUARE_BAD(2 + 3);   // 2 + 3 * 2 + 3 = 2 + 6 + 3 = 11 (wrong!)
int b = 10 * DOUBLE_BAD(5);  // 10 * 5 + 5 = 55 (wrong!)

// GOOD - with parentheses
#define SQUARE(x) ((x) * (x))
#define DOUBLE(x) ((x) + (x))

int a = SQUARE(2 + 3);       // ((2 + 3) * (2 + 3)) = 25 (correct!)
int b = 10 * DOUBLE(5);      // 10 * ((5) + (5)) = 100 (correct!)
```

### Multiple Statements

```c
// BAD - breaks with if/else
#define LOG_BAD(msg) printf("LOG: "); printf("%s\n", msg)

if (error)
    LOG_BAD("error!");  // Only first printf is in if!
else
    do_something();     // Compiler error: else without if

// GOOD - use do-while(0)
#define LOG(msg) \
    do { \
        printf("LOG: "); \
        printf("%s\n", msg); \
    } while(0)

if (error)
    LOG("error!");  // Works correctly
else
    do_something();
```

### Macros with No Arguments vs Object-like

```c
// Function-like (note the parentheses touching the name)
#define GET_VALUE() some_global_value

// Object-like (space before anything)
#define GET_VALUE some_global_value

// The difference:
GET_VALUE()  // Function-like: expands to some_global_value
GET_VALUE    // Object-like: expands to some_global_value
```

---

## 4. Stringification (#)

The `#` operator converts a macro argument to a string literal.

### Basic Usage

```c
#define STRINGIFY(x) #x

printf("%s\n", STRINGIFY(hello));      // prints: hello
printf("%s\n", STRINGIFY(123));        // prints: 123
printf("%s\n", STRINGIFY(a + b));      // prints: a + b
```

### Practical Examples

```c
// Debug printing
#define PRINT_VAR(var) printf(#var " = %d\n", var)

int count = 42;
PRINT_VAR(count);  // prints: count = 42

// Expands to:
printf("count" " = %d\n", count);  // String concatenation!
printf("count = %d\n", count);     // Same thing
```

### Assert with Expression

```c
#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "Assertion failed: %s\n", #expr); \
            fprintf(stderr, "  File: %s, Line: %d\n", __FILE__, __LINE__); \
            abort(); \
        } \
    } while(0)

ASSERT(x > 0);
// If x <= 0, prints: Assertion failed: x > 0
```

### Stringify Expanded Value

```c
#define VALUE 42
#define STRINGIFY(x) #x
#define STRINGIFY_VALUE(x) STRINGIFY(x)

printf("%s\n", STRINGIFY(VALUE));        // prints: VALUE (not expanded!)
printf("%s\n", STRINGIFY_VALUE(VALUE));  // prints: 42 (expanded first!)

// Why? # prevents expansion. Two-level macro forces expansion first.
```

---

## 5. Token Pasting (##)

The `##` operator concatenates two tokens into one.

### Basic Usage

```c
#define CONCAT(a, b) a ## b

int CONCAT(my, Variable) = 10;  // Creates: int myVariable = 10;
CONCAT(my, Function)();         // Calls: myFunction();
```

### Creating Identifiers

```c
#define MAKE_GETTER(name) get_ ## name
#define MAKE_SETTER(name) set_ ## name

int MAKE_GETTER(value)(void);   // int get_value(void);
void MAKE_SETTER(value)(int v); // void set_value(int v);
```

### Enum and String Tables

```c
// Define once, use everywhere
#define FOREACH_ERROR(E) \
    E(ERROR_NONE) \
    E(ERROR_FILE_NOT_FOUND) \
    E(ERROR_ACCESS_DENIED) \
    E(ERROR_OUT_OF_MEMORY)

// Generate enum
#define GENERATE_ENUM(name) name,
enum ErrorCode {
    FOREACH_ERROR(GENERATE_ENUM)
};
// Expands to:
// enum ErrorCode { ERROR_NONE, ERROR_FILE_NOT_FOUND, ... };

// Generate string table
#define GENERATE_STRING(name) #name,
const char *error_strings[] = {
    FOREACH_ERROR(GENERATE_STRING)
};
// Expands to:
// const char *error_strings[] = { "ERROR_NONE", "ERROR_FILE_NOT_FOUND", ... };

// Usage
printf("Error: %s\n", error_strings[ERROR_FILE_NOT_FOUND]);
```

### Unique Variable Names

```c
#define UNIQUE_NAME(prefix) CONCAT(prefix, __LINE__)
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b

int UNIQUE_NAME(temp) = 1;  // On line 42: int temp42 = 1;
int UNIQUE_NAME(temp) = 2;  // On line 43: int temp43 = 2;
```

---

## 6. Variadic Macros

Macros that accept variable number of arguments.

### Basic Syntax

```c
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)

LOG("Value: %d\n", 42);           // printf("Value: %d\n", 42);
LOG("x=%d, y=%d\n", x, y);        // printf("x=%d, y=%d\n", x, y);
```

### The Trailing Comma Problem

```c
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)

LOG("Hello\n");  // printf("Hello\n", );  // ERROR! Trailing comma!
```

### Solutions

**GNU Extension (##**VA_ARGS**):**

```c
#define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)

LOG("Hello\n");        // printf("Hello\n");  // Comma removed!
LOG("x=%d\n", 42);     // printf("x=%d\n", 42);
```

**C23 **VA_OPT**:**

```c
#define LOG(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)

LOG("Hello\n");        // printf("Hello\n");
LOG("x=%d\n", 42);     // printf("x=%d\n", 42);
```

**Workaround for older standards:**

```c
#define LOG(...) printf(__VA_ARGS__)

LOG("Hello\n");        // printf("Hello\n");
LOG("x=%d\n", 42);     // printf("x=%d\n", 42);
```

### Practical Debug Macro

```c
#ifdef DEBUG
    #define DEBUG_LOG(fmt, ...) \
        fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) ((void)0)
#endif

DEBUG_LOG("Starting");
DEBUG_LOG("Value is %d", value);
// [DEBUG] main.c:42: Starting
// [DEBUG] main.c:43: Value is 42
```

### Counting Arguments

```c
// Count up to 10 arguments
#define COUNT_ARGS(...) COUNT_ARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define COUNT_ARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

int count1 = COUNT_ARGS(a);           // 1
int count3 = COUNT_ARGS(a, b, c);     // 3
int count5 = COUNT_ARGS(a, b, c, d, e); // 5
```

---

## 7. Conditional Compilation

### #if, #elif, #else, #endif

```c
#define VERSION 2

#if VERSION == 1
    void feature_v1(void);
#elif VERSION == 2
    void feature_v2(void);
#else
    void feature_default(void);
#endif
```

### #ifdef, #ifndef

```c
// If defined
#ifdef DEBUG
    printf("Debug mode\n");
#endif

// If not defined
#ifndef BUFFER_SIZE
    #define BUFFER_SIZE 1024
#endif

// Equivalent to:
#if defined(DEBUG)
#if !defined(BUFFER_SIZE)
```

### defined() Operator

```c
// Complex conditions
#if defined(WINDOWS) && !defined(CONSOLE_APP)
    // Windows GUI app
#elif defined(LINUX) || defined(MACOS)
    // Unix-like
#endif

// Can combine with values
#if defined(VERSION) && VERSION >= 2
    // Version 2 or higher
#endif
```

### Checking Compiler/Platform

```c
// Compiler detection
#if defined(__clang__)
    #define COMPILER "Clang"
#elif defined(__GNUC__)
    #define COMPILER "GCC"
#elif defined(_MSC_VER)
    #define COMPILER "MSVC"
#endif

// Platform detection
#if defined(_WIN32)
    #include <windows.h>
    #define PLATFORM "Windows"
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define PLATFORM "macOS"
    #endif
#elif defined(__linux__)
    #define PLATFORM "Linux"
#elif defined(__FreeBSD__)
    #define PLATFORM "FreeBSD"
#endif

// Architecture
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define ARCH "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH "ARM64"
#endif
```

### #error and #warning

```c
#ifndef REQUIRED_DEFINE
    #error "REQUIRED_DEFINE must be defined!"
#endif

#if BUFFER_SIZE < 256
    #warning "BUFFER_SIZE is very small, consider increasing"
#endif

// Compiler will stop with error message or show warning
```

---

## 8. Predefined Macros

### Standard C Macros

```c
__FILE__      // Current filename as string: "main.c"
__LINE__      // Current line number as int: 42
__func__      // Current function name (C99): "main"
__DATE__      // Compilation date: "Jan  1 2024"
__TIME__      // Compilation time: "12:34:56"
__STDC__      // 1 if standard C compliant
__STDC_VERSION__  // C standard version:
                  // 199901L = C99
                  // 201112L = C11
                  // 201710L = C17
                  // 202311L = C23
```

### Usage Examples

```c
// Logging with location
#define LOG(msg) printf("[%s:%d] %s\n", __FILE__, __LINE__, msg)

// Build info
printf("Built on %s at %s\n", __DATE__, __TIME__);

// Function name in errors
void process_data(void) {
    if (error) {
        fprintf(stderr, "Error in %s\n", __func__);
    }
}

// Check C version
#if __STDC_VERSION__ >= 201112L
    // C11 features available
    _Static_assert(sizeof(int) == 4, "int must be 4 bytes");
#endif
```

### Compiler-Specific Macros

```c
// Clang
__clang__
__clang_major__
__clang_minor__

// GCC
__GNUC__
__GNUC_MINOR__
__GNUC_PATCHLEVEL__

// MSVC
_MSC_VER        // e.g., 1920 for VS 2019
_MSC_FULL_VER

// Example
#if defined(__clang__)
    printf("Clang %d.%d\n", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
    printf("GCC %d.%d\n", __GNUC__, __GNUC_MINOR__);
#endif
```

### Platform Macros

```c
// Windows
_WIN32          // 32-bit or 64-bit Windows
_WIN64          // 64-bit Windows only
__MINGW32__     // MinGW 32-bit
__MINGW64__     // MinGW 64-bit
__CYGWIN__      // Cygwin

// Apple
__APPLE__       // Any Apple platform
__MACH__        // Mach kernel (macOS, iOS)

// Linux
__linux__       // Linux kernel
__gnu_linux__   // GNU/Linux

// BSD
__FreeBSD__
__NetBSD__
__OpenBSD__

// Unix-like
__unix__
_POSIX_VERSION
```

---

## 9. Include Guards

Prevent multiple inclusion of header files.

### Traditional Include Guard

```c
// file: myheader.h
#ifndef MYHEADER_H
#define MYHEADER_H

// Header contents here
struct MyStruct {
    int value;
};

void my_function(void);

#endif // MYHEADER_H
```

### #pragma once (Non-standard but widely supported)

```c
// file: myheader.h
#pragma once

// Header contents here
struct MyStruct {
    int value;
};
```

### Comparison

```c
// Include guard:
// + Standard C
// + Works everywhere
// - More typing
// - Must ensure unique names

// #pragma once:
// + Simple
// + Faster compilation (sometimes)
// - Non-standard (but works on all major compilers)
// - Can fail with symlinks/network drives
```

---

## 10. Pragma

Compiler-specific directives.

### Common Pragmas

```c
// Disable specific warning
#pragma warning(disable: 4996)  // MSVC
#pragma GCC diagnostic ignored "-Wunused-variable"  // GCC/Clang

// Push/pop warning state
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
// ... code with deprecated stuff ...
#pragma GCC diagnostic pop

// Structure packing
#pragma pack(push, 1)
struct PackedStruct {
    char a;    // 1 byte
    int b;     // 4 bytes, no padding!
};             // Total: 5 bytes
#pragma pack(pop)

// Mark function as used (prevent dead code elimination)
#pragma GCC poison printf  // Error if printf is used

// Region markers (IDE support)
#pragma region MySection
// ... code ...
#pragma endregion
```

### \_Pragma Operator (C99)

```c
// Allows pragma in macros
#define DISABLE_WARNING(warning) \
    _Pragma(STRINGIFY(GCC diagnostic ignored warning))

#define STRINGIFY(x) #x

DISABLE_WARNING("-Wunused-variable")
// Expands to: _Pragma("GCC diagnostic ignored \"-Wunused-variable\"")
```

---

## 11. Advanced Techniques

### X-Macros

```c
// Define data once, generate multiple things
#define COLORS \
    X(RED,   0xFF0000) \
    X(GREEN, 0x00FF00) \
    X(BLUE,  0x0000FF) \
    X(WHITE, 0xFFFFFF) \
    X(BLACK, 0x000000)

// Generate enum
#define X(name, value) COLOR_##name,
enum Color {
    COLORS
    COLOR_COUNT
};
#undef X

// Generate value array
#define X(name, value) value,
u32_t color_values[] = {
    COLORS
};
#undef X

// Generate name strings
#define X(name, value) #name,
const char *color_names[] = {
    COLORS
};
#undef X

// Usage
printf("Color %s = 0x%06X\n",
       color_names[COLOR_RED],
       color_values[COLOR_RED]);
// Output: Color RED = 0xFF0000
```

### Type-Generic Macros (C11 \_Generic)

```c
#define print_value(x) _Generic((x), \
    int:     print_int, \
    float:   print_float, \
    double:  print_double, \
    char*:   print_string, \
    default: print_unknown \
)(x)

void print_int(int x)        { printf("%d\n", x); }
void print_float(float x)    { printf("%f\n", x); }
void print_double(double x)  { printf("%lf\n", x); }
void print_string(char *x)   { printf("%s\n", x); }
void print_unknown(void *x)  { printf("unknown\n"); }

// Usage
print_value(42);        // Calls print_int
print_value(3.14f);     // Calls print_float
print_value("hello");   // Calls print_string
```

### Compile-Time Type Checking

```c
#define STATIC_ASSERT_EXPR(expr) \
    (sizeof(struct { int:-!(expr); }))

#define TYPE_CHECK(ptr, type) \
    ((void)STATIC_ASSERT_EXPR(__builtin_types_compatible_p(typeof(*(ptr)), type)))

#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) + \
     STATIC_ASSERT_EXPR(!__builtin_types_compatible_p(typeof(arr), typeof(&(arr)[0]))))

int arr[10];
int *ptr = arr;

size_t s1 = ARRAY_SIZE(arr);  // OK: 10
size_t s2 = ARRAY_SIZE(ptr);  // Compile error! ptr is not an array
```

### Defer/Cleanup (GCC/Clang extension)

```c
#define DEFER(fn) __attribute__((cleanup(fn)))

void free_ptr(void **ptr) {
    if (*ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

void close_file(FILE **fp) {
    if (*fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

void example(void) {
    DEFER(free_ptr) char *buffer = malloc(1024);
    DEFER(close_file) FILE *file = fopen("test.txt", "r");

    // ... use buffer and file ...

    // Automatically freed/closed when function returns!
}
```

### Computed Includes

```c
#define PLATFORM_HEADER(name) STRINGIFY(platform/PLATFORM/name.h)
#define STRINGIFY(x) STRINGIFY_INNER(x)
#define STRINGIFY_INNER(x) #x

#if defined(_WIN32)
    #define PLATFORM win32
#elif defined(__linux__)
    #define PLATFORM linux
#elif defined(__APPLE__)
    #define PLATFORM macos
#endif

#include PLATFORM_HEADER(audio)    // "platform/linux/audio.h"
#include PLATFORM_HEADER(window)   // "platform/linux/window.h"
```

### Macro Overloading by Argument Count

```c
// Get the right macro based on argument count
#define GET_MACRO(_1, _2, _3, NAME, ...) NAME

#define FOO(...) GET_MACRO(__VA_ARGS__, FOO3, FOO2, FOO1)(__VA_ARGS__)

#define FOO1(a)       printf("one: %d\n", a)
#define FOO2(a, b)    printf("two: %d, %d\n", a, b)
#define FOO3(a, b, c) printf("three: %d, %d, %d\n", a, b, c)

FOO(1);        // Calls FOO1(1)
FOO(1, 2);     // Calls FOO2(1, 2)
FOO(1, 2, 3);  // Calls FOO3(1, 2, 3)
```

### Recursive-like Macros (with limits)

```c
// Macros can't be truly recursive, but can simulate with multiple levels
#define REPEAT_0(x)
#define REPEAT_1(x) x
#define REPEAT_2(x) x x
#define REPEAT_3(x) x x x
#define REPEAT_4(x) x x x x
#define REPEAT_5(x) x x x x x

#define REPEAT_N(n, x) REPEAT_##n(x)

REPEAT_N(3, printf("hello\n");)
// Expands to: printf("hello\n"); printf("hello\n"); printf("hello\n");
```

---

## 12. Common Patterns

### Safe Min/Max

```c
// Type-safe, evaluates arguments only once
#define MIN(a, b) ({ \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b; \
})

#define MAX(a, b) ({ \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b; \
})

// Usage - safe even with side effects
int x = MIN(expensive_function(), another_function());
```

### Container Of

```c
// Get pointer to containing structure from member pointer
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct list_node {
    struct list_node *next;
    struct list_node *prev;
};

struct my_data {
    int value;
    char name[32];
    struct list_node node;  // Embedded at offset
};

// Given a list_node pointer, get the my_data pointer
struct list_node *node_ptr = /* ... */;
struct my_data *data = container_of(node_ptr, struct my_data, node);
```

### Likely/Unlikely Branch Hints

```c
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

if (UNLIKELY(error_occurred)) {
    handle_error();
}

if (LIKELY(buffer != NULL)) {
    process_buffer(buffer);
}
```

### Compile-Time Assertions

```c
// C11 way
#define STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)

// Pre-C11 way
#define STATIC_ASSERT(expr, msg) \
    typedef char static_assertion_##msg[(expr) ? 1 : -1]

STATIC_ASSERT(sizeof(int) == 4, int_must_be_4_bytes);
STATIC_ASSERT(sizeof(void*) == 8, must_be_64_bit);
```

### Unused Parameter

```c
#define UNUSED(x) (void)(x)

void callback(int event, void *data) {
    UNUSED(data);  // Suppress "unused parameter" warning
    printf("Event: %d\n", event);
}
```

### Array Size

```c
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int numbers[] = {1, 2, 3, 4, 5};
for (size_t i = 0; i < ARRAY_SIZE(numbers); i++) {
    printf("%d\n", numbers[i]);
}
```

### Memory Size Helpers

```c
#define KB(x) ((x) * 1024ULL)
#define MB(x) (KB(x) * 1024ULL)
#define GB(x) (MB(x) * 1024ULL)

size_t buffer_size = MB(16);  // 16 megabytes
size_t cache_size = KB(64);   // 64 kilobytes
```

### Bit Manipulation

```c
#define BIT(n)              (1ULL << (n))
#define SET_BIT(x, n)       ((x) |= BIT(n))
#define CLEAR_BIT(x, n)     ((x) &= ~BIT(n))
#define TOGGLE_BIT(x, n)    ((x) ^= BIT(n))
#define CHECK_BIT(x, n)     (((x) >> (n)) & 1)

#define SET_BITS(x, mask)   ((x) |= (mask))
#define CLEAR_BITS(x, mask) ((x) &= ~(mask))

u32_t flags = 0;
SET_BIT(flags, 3);      // Set bit 3
if (CHECK_BIT(flags, 3)) {
    printf("Bit 3 is set\n");
}
```

### Alignment

```c
#define ALIGN_UP(x, align)   (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

size_t aligned = ALIGN_UP(100, 16);  // 112 (next multiple of 16)
```

### Stringify Enum

```c
#define ENUM_TO_STRING(e) case e: return #e

const char *error_to_string(enum Error e) {
    switch (e) {
        ENUM_TO_STRING(ERROR_NONE);
        ENUM_TO_STRING(ERROR_FILE_NOT_FOUND);
        ENUM_TO_STRING(ERROR_ACCESS_DENIED);
        default: return "UNKNOWN";
    }
}
```

---

## 13. Dangers & Best Practices

### Multiple Evaluation Problem

```c
// DANGEROUS
#define SQUARE(x) ((x) * (x))

int i = 5;
int result = SQUARE(i++);  // ((i++) * (i++)) - undefined behavior!

// SAFE - use inline function or statement expression
static inline int square(int x) { return x * x; }

// Or GCC/Clang extension
#define SQUARE_SAFE(x) ({ \
    __typeof__(x) _x = (x); \
    _x * _x; \
})
```

### Operator Precedence

```c
// DANGEROUS
#define ADD(a, b) a + b

int result = ADD(1, 2) * 3;  // 1 + 2 * 3 = 7, not 9!

// SAFE
#define ADD(a, b) ((a) + (b))

int result = ADD(1, 2) * 3;  // ((1) + (2)) * 3 = 9
```

### Semicolon Issues

```c
// DANGEROUS
#define DO_SOMETHING() func1(); func2()

if (condition)
    DO_SOMETHING();  // Only func1() is in the if!
else
    other();         // Error: else without if

// SAFE - use do-while(0)
#define DO_SOMETHING() \
    do { \
        func1(); \
        func2(); \
    } while(0)
```

### Naming Conventions

```c
// Use ALL_CAPS for macros to distinguish from functions
#define MAX_BUFFER_SIZE 1024    // Good
#define CalculateSum(a, b)      // Bad - looks like a function

// Prefix with project/module name to avoid collisions
#define DE100_ASSERT(x)         // Good
#define ASSERT(x)               // Risky - might conflict

// Use underscores for internal/helper macros
#define _INTERNAL_HELPER(x)     // Convention for "private"
#define INTERNAL_HELPER_(x)     // Alternative convention
```

### Macro vs Inline Function

```c
// PREFER inline functions when possible:

// Macro - no type safety, multiple evaluation
#define SQUARE(x) ((x) * (x))

// Inline function - type safe, single evaluation, debuggable
static inline int square_int(int x) { return x * x; }
static inline float square_float(float x) { return x * x; }

// Use macros when you NEED:
// 1. Token pasting (##)
// 2. Stringification (#)
// 3. __FILE__, __LINE__ at call site
// 4. Type-generic behavior (or use _Generic)
// 5. Conditional compilation
// 6. Code that must work in C89
```

### Debugging Macros

```c
// Problem: Can't step into macros with debugger
// Solution: Provide debug-friendly alternatives

#ifdef DEBUG
    // Expand to actual function calls for debugging
    #define VECTOR_ADD(a, b) vector_add_debug(a, b, __FILE__, __LINE__)
#else
    // Fast macro version for release
    #define VECTOR_ADD(a, b) ((Vec3){(a).x+(b).x, (a).y+(b).y, (a).z+(b).z})
#endif
```

### Documentation

```c
/**
 * @brief Clamps a value between min and max.
 *
 * @param val The value to clamp
 * @param min Minimum bound (inclusive)
 * @param max Maximum bound (inclusive)
 * @return The clamped value
 *
 * @warning Arguments are evaluated multiple times!
 * @note For side-effect-free expressions only.
 *
 * Example:
 *   int x = CLAMP(value, 0, 100);
 */
#define CLAMP(val, min, max) \
    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
```

---

## 14. Complete Real-World Examples

### Example 1: Cross-Platform Types

```c
// platform_types.h
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Fixed-size types with cleaner names
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

typedef u8_t   u8;
typedef u16_t  u16;
typedef u32_t  u32;
typedef u64_t  u64;

typedef float     f32;
typedef double    f64;

// Size types
typedef size_t    usize;
typedef ptrdiff_t isize;

// Boolean
typedef int32_t   b32;  // 32-bit bool for alignment

// Byte
typedef u8_t   byte;

// Memory sizes
#define KB(n) ((usize)(n) * 1024)
#define MB(n) (KB(n) * 1024)
#define GB(n) (MB(n) * 1024)

// Limits
#define I8_MIN  INT8_MIN
#define I8_MAX  INT8_MAX
#define I16_MIN INT16_MIN
#define I16_MAX INT16_MAX
#define I32_MIN INT32_MIN
#define I32_MAX INT32_MAX
#define I64_MIN INT64_MIN
#define I64_MAX INT64_MAX

#define U8_MAX  u8_MAX
#define U16_MAX u16_MAX
#define U32_MAX u32_MAX
#define U64_MAX u64_MAX

#endif // PLATFORM_TYPES_H
```

### Example 2: Comprehensive Assert System

```c
// assert.h
#ifndef DE100_ASSERT_H
#define DE100_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

// Configuration
#ifndef DE100_ASSERT_ENABLED
    #ifdef NDEBUG
        #define DE100_ASSERT_ENABLED 0
    #else
        #define DE100_ASSERT_ENABLED 1
    #endif
#endif

// Debug break
#if defined(_MSC_VER)
    #define DE100_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
    #define DE100_DEBUG_BREAK() __builtin_trap()
#else
    #define DE100_DEBUG_BREAK() abort()
#endif

// Assert handler (can be overridden)
#ifndef DE100_ASSERT_HANDLER
    #define DE100_ASSERT_HANDLER(expr, file, line, func, msg) \
        de100_default_assert_handler(expr, file, line, func, msg)
#endif

static inline void de100_default_assert_handler(
    const char *expr,
    const char *file,
    int line,
    const char *func,
    const char *msg
) {
    fprintf(stderr,
        "\n"
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║                     ASSERTION FAILED                         ║\n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║ Expression: %-48s ║\n"
        "║ File:       %-48s ║\n"
        "║ Line:       %-48d ║\n"
        "║ Function:   %-48s ║\n",
        expr, file, line, func);

    if (msg && msg[0]) {
        fprintf(stderr,
        "║ Message:    %-48s ║\n", msg);
    }

    fprintf(stderr,
        "╚══════════════════════════════════════════════════════════════╝\n\n");

    fflush(stderr);
}

// Main assert macros
#if DE100_ASSERT_ENABLED

    #define DE100_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                DE100_ASSERT_HANDLER(#expr, __FILE__, __LINE__, __func__, ""); \
                DE100_DEBUG_BREAK(); \
            } \
        } while(0)

    #define DE100_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                DE100_ASSERT_HANDLER(#expr, __FILE__, __LINE__, __func__, msg); \
                DE100_DEBUG_BREAK(); \
            } \
        } while(0)

    #define DE100_ASSERT_FMT(expr, fmt, ...) \
        do { \
            if (!(expr)) { \
                char _msg[256]; \
                snprintf(_msg, sizeof(_msg), fmt, ##__VA_ARGS__); \
                DE100_ASSERT_HANDLER(#expr, __FILE__, __LINE__, __func__, _msg); \
                DE100_DEBUG_BREAK(); \
            } \
        } while(0)

    #define DE100_UNREACHABLE() \
        do { \
            DE100_ASSERT_HANDLER("UNREACHABLE", __FILE__, __LINE__, __func__, \
                                 "Code path should never be reached"); \
            DE100_DEBUG_BREAK(); \
        } while(0)

    #define DE100_NOT_IMPLEMENTED() \
        do { \
            DE100_ASSERT_HANDLER("NOT_IMPLEMENTED", __FILE__, __LINE__, __func__, \
                                 "Feature not yet implemented"); \
            DE100_DEBUG_BREAK(); \
        } while(0)

#else
    #define DE100_ASSERT(expr)              ((void)0)
    #define DE100_ASSERT_MSG(expr, msg)     ((void)0)
    #define DE100_ASSERT_FMT(expr, fmt, ...) ((void)0)
    #define DE100_UNREACHABLE()             ((void)0)
    #define DE100_NOT_IMPLEMENTED()         ((void)0)
#endif

// Static assert (compile-time)
#if __STDC_VERSION__ >= 201112L
    #define DE100_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#else
    #define DE100_STATIC_ASSERT(expr, msg) \
        typedef char static_assert_##__LINE__[(expr) ? 1 : -1]
#endif

// Usage examples:
// DE100_ASSERT(ptr != NULL);
// DE100_ASSERT_MSG(count > 0, "Count must be positive");
// DE100_ASSERT_FMT(index < size, "Index %d out of bounds (size=%d)", index, size);
// DE100_STATIC_ASSERT(sizeof(int) == 4, "int must be 4 bytes");

#endif // DE100_ASSERT_H
```

### Example 3: Logging System

```c
// log.h
#ifndef DE100_LOG_H
#define DE100_LOG_H

#include <stdio.h>
#include <time.h>

// Log levels
#define DE100_LOG_LEVEL_TRACE 0
#define DE100_LOG_LEVEL_DEBUG 1
#define DE100_LOG_LEVEL_INFO  2
#define DE100_LOG_LEVEL_WARN  3
#define DE100_LOG_LEVEL_ERROR 4
#define DE100_LOG_LEVEL_FATAL 5
#define DE100_LOG_LEVEL_OFF   6

// Default log level
#ifndef DE100_LOG_LEVEL
    #ifdef NDEBUG
        #define DE100_LOG_LEVEL DE100_LOG_LEVEL_INFO
    #else
        #define DE100_LOG_LEVEL DE100_LOG_LEVEL_TRACE
    #endif
#endif

// ANSI colors
#define DE100_LOG_COLOR_RESET   "\033[0m"
#define DE100_LOG_COLOR_TRACE   "\033[90m"      // Gray
#define DE100_LOG_COLOR_DEBUG   "\033[36m"      // Cyan
#define DE100_LOG_COLOR_INFO    "\033[32m"      // Green
#define DE100_LOG_COLOR_WARN    "\033[33m"      // Yellow
#define DE100_LOG_COLOR_ERROR   "\033[31m"      // Red
#define DE100_LOG_COLOR_FATAL   "\033[35m"      // Magenta

// Enable/disable colors
#ifndef DE100_LOG_USE_COLOR
    #if defined(_WIN32) && !defined(__MINGW32__)
        #define DE100_LOG_USE_COLOR 0
    #else
        #define DE100_LOG_USE_COLOR 1
    #endif
#endif

// Log output stream
#ifndef DE100_LOG_STREAM
    #define DE100_LOG_STREAM stderr
#endif

// Internal log implementation
#define DE100_LOG_IMPL(level, level_str, color, fmt, ...) \
    do { \
        time_t _t = time(NULL); \
        struct tm *_tm = localtime(&_t); \
        char _time_buf[16]; \
        strftime(_time_buf, sizeof(_time_buf), "%H:%M:%S", _tm); \
        if (DE100_LOG_USE_COLOR) { \
            fprintf(DE100_LOG_STREAM, \
                "%s[%s]%s %s%-5s%s %s:%d: " fmt "\n", \
                "\033[90m", _time_buf, DE100_LOG_COLOR_RESET, \
                color, level_str, DE100_LOG_COLOR_RESET, \
                __FILE__, __LINE__, ##__VA_ARGS__); \
        } else { \
            fprintf(DE100_LOG_STREAM, \
                "[%s] %-5s %s:%d: " fmt "\n", \
                _time_buf, level_str, \
                __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
        fflush(DE100_LOG_STREAM); \
    } while(0)

// Public log macros
#if DE100_LOG_LEVEL <= DE100_LOG_LEVEL_TRACE
    #define LOG_TRACE(fmt, ...) \
        DE100_LOG_IMPL(DE100_LOG_LEVEL_TRACE, "TRACE", DE100_LOG_COLOR_TRACE, fmt, ##__VA_ARGS__)
#else
    #define LOG_TRACE(fmt, ...) ((void)0)
#endif

#if DE100_LOG_LEVEL <= DE100_LOG_LEVEL_DEBUG
    #define LOG_DEBUG(fmt, ...) \
        DE100_LOG_IMPL(DE100_LOG_LEVEL_DEBUG, "DEBUG", DE100_LOG_COLOR_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if DE100_LOG_LEVEL <= DE100_LOG_LEVEL_INFO
    #define LOG_INFO(fmt, ...) \
        DE100_LOG_IMPL(DE100_LOG_LEVEL_INFO, "INFO", DE100_LOG_COLOR_INFO, fmt, ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

#if DE100_LOG_LEVEL <= DE100_LOG_LEVEL_WARN
    #define LOG_WARN(fmt, ...) \
        DE100_LOG_IMPL(DE100_LOG_LEVEL_WARN, "WARN", DE100_LOG_COLOR_WARN, fmt, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif

#if DE100_LOG_LEVEL <= DE100_LOG_LEVEL_ERROR
    #define LOG_ERROR(fmt, ...) \
        DE100_LOG_IMPL(DE100_LOG_LEVEL_ERROR, "ERROR", DE100_LOG_COLOR_ERROR, fmt, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if DE100_LOG_LEVEL <= DE100_LOG_LEVEL_FATAL
    #define LOG_FATAL(fmt, ...) \
        DE100_LOG_IMPL(DE100_LOG_LEVEL_FATAL, "FATAL", DE100_LOG_COLOR_FATAL, fmt, ##__VA_ARGS__)
#else
    #define LOG_FATAL(fmt, ...) ((void)0)
#endif

// Usage:
// LOG_TRACE("Entering function");
// LOG_DEBUG("Variable x = %d", x);
// LOG_INFO("Server started on port %d", port);
// LOG_WARN("Memory usage at %d%%", usage);
// LOG_ERROR("Failed to open file: %s", filename);
// LOG_FATAL("Out of memory!");

#endif // DE100_LOG_H
```

### Example 4: Dynamic Array (Generic Container)

```c
// dynarray.h
#ifndef DE100_DYNARRAY_H
#define DE100_DYNARRAY_H

#include <stdlib.h>
#include <string.h>

// Dynamic array header (stored before the data)
typedef struct {
    size_t length;
    size_t capacity;
    size_t elem_size;
} DynArrayHeader;

// Get header from array pointer
#define da_header(a) \
    ((DynArrayHeader *)((char *)(a) - sizeof(DynArrayHeader)))

// Get length
#define da_len(a) \
    ((a) ? da_header(a)->length : 0)

// Get capacity
#define da_cap(a) \
    ((a) ? da_header(a)->capacity : 0)

// Free array
#define da_free(a) \
    do { \
        if (a) { \
            free(da_header(a)); \
            (a) = NULL; \
        } \
    } while(0)

// Ensure capacity (internal)
#define da_grow(a, n) \
    do { \
        size_t _cap = da_cap(a); \
        size_t _needed = da_len(a) + (n); \
        if (_needed > _cap) { \
            size_t _new_cap = _cap ? _cap * 2 : 8; \
            while (_new_cap < _needed) _new_cap *= 2; \
            size_t _elem_size = (a) ? da_header(a)->elem_size : sizeof(*(a)); \
            size_t _size = sizeof(DynArrayHeader) + _new_cap * _elem_size; \
            DynArrayHeader *_h = (a) \
                ? realloc(da_header(a), _size) \
                : malloc(_size); \
            _h->capacity = _new_cap; \
            _h->elem_size = _elem_size; \
            if (!(a)) _h->length = 0; \
            (a) = (void *)((char *)_h + sizeof(DynArrayHeader)); \
        } \
    } while(0)

// Push element
#define da_push(a, v) \
    do { \
        da_grow(a, 1); \
        (a)[da_header(a)->length++] = (v); \
    } while(0)

// Pop element
#define da_pop(a) \
    ((a)[--da_header(a)->length])

// Last element
#define da_last(a) \
    ((a)[da_len(a) - 1])

// Clear (keep capacity)
#define da_clear(a) \
    do { if (a) da_header(a)->length = 0; } while(0)

// Reserve capacity
#define da_reserve(a, n) \
    da_grow(a, (n) - da_len(a))

// Insert at index
#define da_insert(a, i, v) \
    do { \
        da_grow(a, 1); \
        size_t _i = (i); \
        size_t _len = da_header(a)->length++; \
        memmove(&(a)[_i + 1], &(a)[_i], (_len - _i) * sizeof(*(a))); \
        (a)[_i] = (v); \
    } while(0)

// Remove at index
#define da_remove(a, i) \
    do { \
        size_t _i = (i); \
        size_t _len = --da_header(a)->length; \
        memmove(&(a)[_i], &(a)[_i + 1], (_len - _i) * sizeof(*(a))); \
    } while(0)

// For-each loop
#define da_foreach(a, it) \
    for (typeof(*(a)) *it = (a); it < (a) + da_len(a); ++it)

// Usage example:
/*
    int *numbers = NULL;

    da_push(numbers, 10);
    da_push(numbers, 20);
    da_push(numbers, 30);

    printf("Length: %zu\n", da_len(numbers));  // 3

    da_foreach(numbers, n) {
        printf("%d\n", *n);  // 10, 20, 30
    }

    int last = da_pop(numbers);  // 30

    da_insert(numbers, 0, 5);  // [5, 10, 20]
    da_remove(numbers, 1);     // [5, 20]

    da_free(numbers);
*/

#endif // DE100_DYNARRAY_H
```

### Example 5: Platform Detection Header

```c
// platform.h
#ifndef DE100_PLATFORM_H
#define DE100_PLATFORM_H

// ═══════════════════════════════════════════════════════════════════════════
// COMPILER DETECTION
// ═══════════════════════════════════════════════════════════════════════════

#if defined(__clang__)
    #define DE100_COMPILER_CLANG 1
    #define DE100_COMPILER_NAME "Clang"
    #define DE100_COMPILER_VERSION \
        (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define DE100_COMPILER_GCC 1
    #define DE100_COMPILER_NAME "GCC"
    #define DE100_COMPILER_VERSION \
        (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
    #define DE100_COMPILER_MSVC 1
    #define DE100_COMPILER_NAME "MSVC"
    #define DE100_COMPILER_VERSION _MSC_VER
#else
    #define DE100_COMPILER_UNKNOWN 1
    #define DE100_COMPILER_NAME "Unknown"
    #define DE100_COMPILER_VERSION 0
#endif

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM DETECTION
// ═══════════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
    #define DE100_PLATFORM_WINDOWS 1
    #define DE100_PLATFORM_NAME "Windows"
    #if defined(_WIN64)
        #define DE100_PLATFORM_WIN64 1
    #else
        #define DE100_PLATFORM_WIN32 1
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #define DE100_PLATFORM_APPLE 1
    #if TARGET_OS_MAC
        #define DE100_PLATFORM_MACOS 1
        #define DE100_PLATFORM_NAME "macOS"
    #elif TARGET_OS_IPHONE
        #define DE100_PLATFORM_IOS 1
        #define DE100_PLATFORM_NAME "iOS"
    #endif
#elif defined(__linux__)
    #define DE100_PLATFORM_LINUX 1
    #define DE100_PLATFORM_NAME "Linux"
    #define DE100_PLATFORM_POSIX 1
#elif defined(__FreeBSD__)
    #define DE100_PLATFORM_FREEBSD 1
    #define DE100_PLATFORM_NAME "FreeBSD"
    #define DE100_PLATFORM_POSIX 1
#elif defined(__unix__)
    #define DE100_PLATFORM_UNIX 1
    #define DE100_PLATFORM_NAME "Unix"
    #define DE100_PLATFORM_POSIX 1
#else
    #define DE100_PLATFORM_UNKNOWN 1
    #define DE100_PLATFORM_NAME "Unknown"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ARCHITECTURE DETECTION
// ═══════════════════════════════════════════════════════════════════════════

#if defined(__x86_64__) || defined(_M_X64)
    #define DE100_ARCH_X64 1
    #define DE100_ARCH_NAME "x86_64"
    #define DE100_ARCH_64BIT 1
#elif defined(__i386__) || defined(_M_IX86)
    #define DE100_ARCH_X86 1
    #define DE100_ARCH_NAME "x86"
    #define DE100_ARCH_32BIT 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define DE100_ARCH_ARM64 1
    #define DE100_ARCH_NAME "ARM64"
    #define DE100_ARCH_64BIT 1
#elif defined(__arm__) || defined(_M_ARM)
    #define DE100_ARCH_ARM 1
    #define DE100_ARCH_NAME "ARM"
    #define DE100_ARCH_32BIT 1
#else
    #define DE100_ARCH_UNKNOWN 1
    #define DE100_ARCH_NAME "Unknown"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// ENDIANNESS
// ═══════════════════════════════════════════════════════════════════════════

#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define DE100_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define DE100_BIG_ENDIAN 1
    #endif
#elif defined(_WIN32)
    #define DE100_LITTLE_ENDIAN 1
#endif

// ═══════════════════════════════════════════════════════════════════════════
// COMPILER ATTRIBUTES
// ═══════════════════════════════════════════════════════════════════════════

// Force inline
#if defined(DE100_COMPILER_MSVC)
    #define DE100_FORCE_INLINE __forceinline
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define DE100_FORCE_INLINE inline
#endif

// No inline
#if defined(DE100_COMPILER_MSVC)
    #define DE100_NO_INLINE __declspec(noinline)
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_NO_INLINE __attribute__((noinline))
#else
    #define DE100_NO_INLINE
#endif

// Unused
#if defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_UNUSED __attribute__((unused))
#else
    #define DE100_UNUSED
#endif

// Deprecated
#if defined(DE100_COMPILER_MSVC)
    #define DE100_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define DE100_DEPRECATED(msg)
#endif

// No return
#if defined(DE100_COMPILER_MSVC)
    #define DE100_NORETURN __declspec(noreturn)
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_NORETURN __attribute__((noreturn))
#else
    #define DE100_NORETURN
#endif

// Printf format checking
#if defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_PRINTF_FORMAT(fmt_idx, args_idx) \
        __attribute__((format(printf, fmt_idx, args_idx)))
#else
    #define DE100_PRINTF_FORMAT(fmt_idx, args_idx)
#endif

// Likely/Unlikely
#if defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define DE100_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define DE100_LIKELY(x)   (x)
    #define DE100_UNLIKELY(x) (x)
#endif

// Alignment
#if defined(DE100_COMPILER_MSVC)
    #define DE100_ALIGN(n) __declspec(align(n))
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_ALIGN(n) __attribute__((aligned(n)))
#else
    #define DE100_ALIGN(n)
#endif

// Packed struct
#if defined(DE100_COMPILER_MSVC)
    #define DE100_PACKED_BEGIN __pragma(pack(push, 1))
    #define DE100_PACKED_END   __pragma(pack(pop))
    #define DE100_PACKED
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_PACKED_BEGIN
    #define DE100_PACKED_END
    #define DE100_PACKED __attribute__((packed))
#else
    #define DE100_PACKED_BEGIN
    #define DE100_PACKED_END
    #define DE100_PACKED
#endif

// DLL export/import
#if defined(DE100_PLATFORM_WINDOWS)
    #define DE100_DLL_EXPORT __declspec(dllexport)
    #define DE100_DLL_IMPORT __declspec(dllimport)
#elif defined(DE100_COMPILER_GCC) || defined(DE100_COMPILER_CLANG)
    #define DE100_DLL_EXPORT __attribute__((visibility("default")))
    #define DE100_DLL_IMPORT
#else
    #define DE100_DLL_EXPORT
    #define DE100_DLL_IMPORT
#endif

// API macro (use in headers)
#ifdef DE100_BUILD_DLL
    #define DE100_API DE100_DLL_EXPORT
#elif defined(DE100_USE_DLL)
    #define DE100_API DE100_DLL_IMPORT
#else
    #define DE100_API
#endif

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY MACROS
// ═══════════════════════════════════════════════════════════════════════════

// Stringify
#define DE100_STRINGIFY(x) DE100_STRINGIFY_IMPL(x)
#define DE100_STRINGIFY_IMPL(x) #x

// Concatenate
#define DE100_CONCAT(a, b) DE100_CONCAT_IMPL(a, b)
#define DE100_CONCAT_IMPL(a, b) a##b

// Unique name
#define DE100_UNIQUE_NAME(prefix) DE100_CONCAT(prefix, __LINE__)

// Array size
#define DE100_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Offset of
#ifndef offsetof
    #define offsetof(type, member) ((size_t)&((type *)0)->member)
#endif

// Container of
#define DE100_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Min/Max
#define DE100_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DE100_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DE100_CLAMP(x, lo, hi) DE100_MIN(DE100_MAX(x, lo), hi)

// Swap
#define DE100_SWAP(a, b) \
    do { \
        typeof(a) _tmp = (a); \
        (a) = (b); \
        (b) = _tmp; \
    } while(0)

// Unused parameter
#define DE100_UNUSED_PARAM(x) (void)(x)

// Print platform info
#define DE100_PRINT_PLATFORM_INFO() \
    do { \
        printf("Platform: %s\n", DE100_PLATFORM_NAME); \
        printf("Compiler: %s (version %d)\n", DE100_COMPILER_NAME, DE100_COMPILER_VERSION); \
        printf("Architecture: %s\n", DE100_ARCH_NAME); \
        printf("Pointer size: %zu bytes\n", sizeof(void*)); \
    } while(0)

#endif // DE100_PLATFORM_H
```

### Example 6: Unit Testing Framework

```c
// test.h
#ifndef DE100_TEST_H
#define DE100_TEST_H

#include <stdio.h>
#include <string.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════════════════
// TEST FRAMEWORK STATE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    int assertions;
    int assertions_failed;
    const char *current_test;
    const char *current_file;
    int current_line;
} TestState;

static TestState _test_state = {0};

// ═══════════════════════════════════════════════════════════════════════════
// COLORS
// ═══════════════════════════════════════════════════════════════════════════

#define TEST_COLOR_RESET  "\033[0m"
#define TEST_COLOR_RED    "\033[31m"
#define TEST_COLOR_GREEN  "\033[32m"
#define TEST_COLOR_YELLOW "\033[33m"
#define TEST_COLOR_CYAN   "\033[36m"

// ═══════════════════════════════════════════════════════════════════════════
// TEST DEFINITION
// ═══════════════════════════════════════════════════════════════════════════

#define TEST(name) \
    static void test_##name(void); \
    static void test_##name##_wrapper(void) { \
        _test_state.current_test = #name; \
        _test_state.tests_run++; \
        int _failed_before = _test_state.assertions_failed; \
        printf("  " TEST_COLOR_CYAN "RUN" TEST_COLOR_RESET "  %s\n", #name); \
        test_##name(); \
        if (_test_state.assertions_failed == _failed_before) { \
            _test_state.tests_passed++; \
            printf("  " TEST_COLOR_GREEN "PASS" TEST_COLOR_RESET " %s\n", #name); \
        } else { \
            _test_state.tests_failed++; \
            printf("  " TEST_COLOR_RED "FAIL" TEST_COLOR_RESET " %s\n", #name); \
        } \
    } \
    static void test_##name(void)

// ═══════════════════════════════════════════════════════════════════════════
// ASSERTIONS
// ═══════════════════════════════════════════════════════════════════════════

#define TEST_ASSERT_BASE(expr, msg) \
    do { \
        _test_state.assertions++; \
        _test_state.current_file = __FILE__; \
        _test_state.current_line = __LINE__; \
        if (!(expr)) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Expression: %s\n", #expr); \
            if (msg[0]) printf("       Message: %s\n", msg); \
        } \
    } while(0)

// Basic assertions
#define ASSERT_TRUE(expr) \
    TEST_ASSERT_BASE(expr, "Expected true")

#define ASSERT_FALSE(expr) \
    TEST_ASSERT_BASE(!(expr), "Expected false")

#define ASSERT_NULL(ptr) \
    TEST_ASSERT_BASE((ptr) == NULL, "Expected NULL")

#define ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT_BASE((ptr) != NULL, "Expected non-NULL")

// Equality assertions
#define ASSERT_EQ(expected, actual) \
    do { \
        _test_state.assertions++; \
        if ((expected) != (actual)) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Expected: %lld\n", (long long)(expected)); \
            printf("       Actual:   %lld\n", (long long)(actual)); \
        } \
    } while(0)

#define ASSERT_NE(expected, actual) \
    do { \
        _test_state.assertions++; \
        if ((expected) == (actual)) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Values should not be equal: %lld\n", (long long)(expected)); \
        } \
    } while(0)

// Comparison assertions
#define ASSERT_LT(a, b) \
    TEST_ASSERT_BASE((a) < (b), "Expected " #a " < " #b)

#define ASSERT_LE(a, b) \
    TEST_ASSERT_BASE((a) <= (b), "Expected " #a " <= " #b)

#define ASSERT_GT(a, b) \
    TEST_ASSERT_BASE((a) > (b), "Expected " #a " > " #b)

#define ASSERT_GE(a, b) \
    TEST_ASSERT_BASE((a) >= (b), "Expected " #a " >= " #b)

// Float assertions
#define ASSERT_FLOAT_EQ(expected, actual, epsilon) \
    do { \
        _test_state.assertions++; \
        double _diff = fabs((double)(expected) - (double)(actual)); \
        if (_diff > (epsilon)) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Expected: %f\n", (double)(expected)); \
            printf("       Actual:   %f\n", (double)(actual)); \
            printf("       Diff:     %f (epsilon: %f)\n", _diff, (double)(epsilon)); \
        } \
    } while(0)

// String assertions
#define ASSERT_STR_EQ(expected, actual) \
    do { \
        _test_state.assertions++; \
        if (strcmp((expected), (actual)) != 0) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Expected: \"%s\"\n", (expected)); \
            printf("       Actual:   \"%s\"\n", (actual)); \
        } \
    } while(0)

#define ASSERT_STR_NE(expected, actual) \
    do { \
        _test_state.assertions++; \
        if (strcmp((expected), (actual)) == 0) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Strings should not be equal: \"%s\"\n", (expected)); \
        } \
    } while(0)

// Memory assertions
#define ASSERT_MEM_EQ(expected, actual, size) \
    do { \
        _test_state.assertions++; \
        if (memcmp((expected), (actual), (size)) != 0) { \
            _test_state.assertions_failed++; \
            printf("       " TEST_COLOR_RED "ASSERTION FAILED" TEST_COLOR_RESET \
                   " at %s:%d\n", __FILE__, __LINE__); \
            printf("       Memory blocks differ (size: %zu)\n", (size_t)(size)); \
        } \
    } while(0)

// ═══════════════════════════════════════════════════════════════════════════
// TEST SUITE
// ═══════════════════════════════════════════════════════════════════════════

#define TEST_SUITE_BEGIN(name) \
    static void run_test_suite_##name(void) { \
        printf("\n" TEST_COLOR_YELLOW "═══ Test Suite: %s ═══" TEST_COLOR_RESET "\n\n", #name);

#define TEST_SUITE_END() \
    }

#define RUN_TEST(name) \
    test_##name##_wrapper()

// ═══════════════════════════════════════════════════════════════════════════
// TEST RUNNER
// ═══════════════════════════════════════════════════════════════════════════

#define RUN_TEST_SUITE(name) \
    run_test_suite_##name()

#define TEST_REPORT() \
    do { \
        printf("\n" TEST_COLOR_YELLOW "═══ Test Results ═══" TEST_COLOR_RESET "\n"); \
        printf("Tests:      %d run, ", _test_state.tests_run); \
        if (_test_state.tests_passed > 0) \
            printf(TEST_COLOR_GREEN "%d passed" TEST_COLOR_RESET ", ", _test_state.tests_passed); \
        if (_test_state.tests_failed > 0) \
            printf(TEST_COLOR_RED "%d failed" TEST_COLOR_RESET, _test_state.tests_failed); \
        else \
            printf("0 failed"); \
        printf("\n"); \
        printf("Assertions: %d total, %d failed\n", \
               _test_state.assertions, _test_state.assertions_failed); \
        printf("\n"); \
        if (_test_state.tests_failed == 0) { \
            printf(TEST_COLOR_GREEN "ALL TESTS PASSED!" TEST_COLOR_RESET "\n\n"); \
        } else { \
            printf(TEST_COLOR_RED "SOME TESTS FAILED!" TEST_COLOR_RESET "\n\n"); \
        } \
    } while(0)

#define TEST_EXIT_CODE() \
    (_test_state.tests_failed > 0 ? 1 : 0)

// ═══════════════════════════════════════════════════════════════════════════
// USAGE EXAMPLE
// ═══════════════════════════════════════════════════════════════════════════

/*
// test_math.c

#include "test.h"

// Define tests
TEST(addition) {
    ASSERT_EQ(4, 2 + 2);
    ASSERT_EQ(0, -1 + 1);
    ASSERT_EQ(100, 50 + 50);
}

TEST(subtraction) {
    ASSERT_EQ(0, 2 - 2);
    ASSERT_EQ(-2, 0 - 2);
}

TEST(strings) {
    ASSERT_STR_EQ("hello", "hello");
    ASSERT_STR_NE("hello", "world");
}

TEST(floats) {
    ASSERT_FLOAT_EQ(3.14159, 3.14159, 0.00001);
    ASSERT_FLOAT_EQ(1.0/3.0, 0.333333, 0.0001);
}

// Define test suite
TEST_SUITE_BEGIN(math)
    RUN_TEST(addition);
    RUN_TEST(subtraction);
    RUN_TEST(strings);
    RUN_TEST(floats);
TEST_SUITE_END()

// Main
int main(void) {
    RUN_TEST_SUITE(math);
    TEST_REPORT();
    return TEST_EXIT_CODE();
}
*/

#endif // DE100_TEST_H
```

### Example 7: State Machine Generator

```c
// statemachine.h
#ifndef DE100_STATEMACHINE_H
#define DE100_STATEMACHINE_H

// ═══════════════════════════════════════════════════════════════════════════
// STATE MACHINE GENERATOR MACROS
// ═══════════════════════════════════════════════════════════════════════════

// Define states using X-macro pattern
// Usage:
//   #define PLAYER_STATES(X) \
//       X(IDLE) \
//       X(WALKING) \
//       X(RUNNING) \
//       X(JUMPING) \
//       X(FALLING)

// Generate state enum
#define SM_GENERATE_ENUM(state) STATE_##state,
#define SM_DEFINE_STATES(name, states) \
    typedef enum { \
        states(SM_GENERATE_ENUM) \
        name##_STATE_COUNT \
    } name##State;

// Generate state names array
#define SM_GENERATE_NAME(state) #state,
#define SM_DEFINE_STATE_NAMES(name, states) \
    static const char *name##_state_names[] = { \
        states(SM_GENERATE_NAME) \
    };

// State machine structure
#define SM_DEFINE_MACHINE(name, states) \
    SM_DEFINE_STATES(name, states) \
    SM_DEFINE_STATE_NAMES(name, states) \
    \
    typedef struct name##StateMachine { \
        name##State current; \
        name##State previous; \
        void (*on_enter[name##_STATE_COUNT])(struct name##StateMachine *sm); \
        void (*on_update[name##_STATE_COUNT])(struct name##StateMachine *sm, float dt); \
        void (*on_exit[name##_STATE_COUNT])(struct name##StateMachine *sm); \
        void *user_data; \
    } name##StateMachine; \
    \
    static inline void name##_sm_init(name##StateMachine *sm) { \
        memset(sm, 0, sizeof(*sm)); \
    } \
    \
    static inline const char *name##_sm_state_name(name##State state) { \
        if (state >= 0 && state < name##_STATE_COUNT) \
            return name##_state_names[state]; \
        return "UNKNOWN"; \
    } \
    \
    static inline void name##_sm_transition(name##StateMachine *sm, name##State new_state) { \
        if (sm->current == new_state) return; \
        if (sm->on_exit[sm->current]) \
            sm->on_exit[sm->current](sm); \
        sm->previous = sm->current; \
        sm->current = new_state; \
        if (sm->on_enter[sm->current]) \
            sm->on_enter[sm->current](sm); \
    } \
    \
    static inline void name##_sm_update(name##StateMachine *sm, float dt) { \
        if (sm->on_update[sm->current]) \
            sm->on_update[sm->current](sm, dt); \
    }

// ═══════════════════════════════════════════════════════════════════════════
// USAGE EXAMPLE
// ═══════════════════════════════════════════════════════════════════════════

/*
// player_states.h

#include "statemachine.h"
#include <string.h>

// Define player states
#define PLAYER_STATES(X) \
    X(IDLE) \
    X(WALKING) \
    X(RUNNING) \
    X(JUMPING) \
    X(FALLING) \
    X(DEAD)

// Generate state machine
SM_DEFINE_MACHINE(Player, PLAYER_STATES)

// State handlers
void player_idle_enter(PlayerStateMachine *sm) {
    printf("Entering IDLE state\n");
}

void player_idle_update(PlayerStateMachine *sm, float dt) {
    // Check for input, transition if needed
    // Player_sm_transition(sm, STATE_WALKING);
}

void player_idle_exit(PlayerStateMachine *sm) {
    printf("Exiting IDLE state\n");
}

// Setup
void setup_player_sm(PlayerStateMachine *sm) {
    Player_sm_init(sm);

    sm->on_enter[STATE_IDLE] = player_idle_enter;
    sm->on_update[STATE_IDLE] = player_idle_update;
    sm->on_exit[STATE_IDLE] = player_idle_exit;

    // ... setup other states ...
}

// Usage
int main(void) {
    PlayerStateMachine player_sm;
    setup_player_sm(&player_sm);

    printf("Current state: %s\n", Player_sm_state_name(player_sm.current));

    Player_sm_transition(&player_sm, STATE_WALKING);
    Player_sm_update(&player_sm, 0.016f);

    return 0;
}
*/

#endif // DE100_STATEMACHINE_H
```

### Example 8: Memory Arena with Macros

```c
// arena.h
#ifndef DE100_ARENA_H
#define DE100_ARENA_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════════════
// ARENA ALLOCATOR
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    u8_t *base;
    size_t size;
    size_t used;
    size_t peak;
} Arena;

// Initialize arena with existing memory
#define arena_init(arena, memory, mem_size) \
    do { \
        (arena)->base = (u8_t *)(memory); \
        (arena)->size = (mem_size); \
        (arena)->used = 0; \
        (arena)->peak = 0; \
    } while(0)

// Alignment helper
#define ARENA_ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

// Allocate from arena (returns NULL if out of memory)
#define arena_alloc(arena, alloc_size) \
    arena_alloc_aligned(arena, alloc_size, sizeof(void*))

static inline void *arena_alloc_aligned(Arena *arena, size_t size, size_t align) {
    size_t aligned_used = ARENA_ALIGN_UP(arena->used, align);
    if (aligned_used + size > arena->size) {
        return NULL;  // Out of memory
    }
    void *ptr = arena->base + aligned_used;
    arena->used = aligned_used + size;
    if (arena->used > arena->peak) {
        arena->peak = arena->used;
    }
    return ptr;
}

// Allocate and zero
#define arena_alloc_zero(arena, alloc_size) \
    ({ \
        void *_ptr = arena_alloc(arena, alloc_size); \
        if (_ptr) memset(_ptr, 0, alloc_size); \
        _ptr; \
    })

// Allocate typed
#define arena_alloc_type(arena, type) \
    ((type *)arena_alloc_aligned(arena, sizeof(type), _Alignof(type)))

#define arena_alloc_array(arena, type, count) \
    ((type *)arena_alloc_aligned(arena, sizeof(type) * (count), _Alignof(type)))

// Push/pop for temporary allocations
typedef struct {
    size_t saved_used;
} ArenaMarker;

#define arena_mark(arena) \
    ((ArenaMarker){ .saved_used = (arena)->used })

#define arena_restore(arena, marker) \
    do { (arena)->used = (marker).saved_used; } while(0)

// Temporary arena scope
#define arena_temp_scope(arena) \
    for (ArenaMarker _marker = arena_mark(arena), *_once = &_marker; \
         _once; \
         arena_restore(arena, _marker), _once = NULL)

// Reset arena (keep memory)
#define arena_reset(arena) \
    do { (arena)->used = 0; } while(0)

// Get remaining space
#define arena_remaining(arena) \
    ((arena)->size - (arena)->used)

// ═══════════════════════════════════════════════════════════════════════════
// USAGE EXAMPLE
// ═══════════════════════════════════════════════════════════════════════════

/*
#include "arena.h"
#include <stdio.h>

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 position;
    Vec3 velocity;
    int health;
} Entity;

int main(void) {
    // Create arena with 1MB of memory
    u8_t memory[1024 * 1024];
    Arena arena;
    arena_init(&arena, memory, sizeof(memory));

    // Allocate single entity
    Entity *player = arena_alloc_type(&arena, Entity);
    player->health = 100;

    // Allocate array of entities
    Entity *enemies = arena_alloc_array(&arena, Entity, 10);
    for (int i = 0; i < 10; i++) {
        enemies[i].health = 50;
    }

    // Temporary allocation scope
    arena_temp_scope(&arena) {
        char *temp_buffer = arena_alloc(&arena, 4096);
        // Use temp_buffer...
        // Automatically freed when scope ends
    }

    printf("Arena used: %zu / %zu bytes\n", arena.used, arena.size);
    printf("Peak usage: %zu bytes\n", arena.peak);

    return 0;
}
*/

#endif // DE100_ARENA_H
```

---

## 15. Quick Reference Card

```c
// ═══════════════════════════════════════════════════════════════════════════
// C PREPROCESSOR QUICK REFERENCE
// ═══════════════════════════════════════════════════════════════════════════

// ─── DEFINITION ───
#define NAME value                    // Object-like macro
#define FUNC(a, b) ((a) + (b))       // Function-like macro
#define MULTI_LINE \                  // Multi-line (backslash)
    line1 \
    line2
#undef NAME                           // Undefine

// ─── OPERATORS ───
#define STR(x) #x                     // Stringify: STR(hello) → "hello"
#define CAT(a,b) a##b                 // Concatenate: CAT(foo,bar) → foobar
#define VA(...) __VA_ARGS__           // Variadic arguments
#define VA_OPT(...) __VA_OPT__(,)     // C23: comma only if args present

// ─── CONDITIONALS ───
#if EXPR                              // If expression is true
#elif EXPR                            // Else if
#else                                 // Else
#endif                                // End if

#ifdef NAME                           // If defined (same as #if defined(NAME))
#ifndef NAME                          // If not defined
#if defined(A) && !defined(B)         // Complex conditions

#error "message"                      // Compilation error
#warning "message"                    // Compilation warning (non-standard)

// ─── INCLUDES ───
#include <file.h>                     // System include path
#include "file.h"                     // Local include path
#include MACRO                        // Computed include

// ─── PREDEFINED MACROS ───
__FILE__                              // Current file: "main.c"
__LINE__                              // Current line: 42
__func__                              // Current function: "main"
__DATE__                              // Compile date: "Jan  1 2024"
__TIME__                              // Compile time: "12:34:56"
__STDC__                              // 1 if standard C
__STDC_VERSION__                      // C version (199901L, 201112L, etc.)

// ─── PRAGMA ───
#pragma once                          // Include guard (non-standard)
#pragma pack(push, 1)                 // Struct packing
#pragma pack(pop)
_Pragma("message \"hello\"")          // Pragma in macro (C99)

// ─── COMMON PATTERNS ───

// Safe multi-statement macro
#define SAFE_MACRO(x) \
    do { \
        stmt1; \
        stmt2; \
    } while(0)

// Parenthesize everything
#define ADD(a, b) ((a) + (b))

// Avoid multiple evaluation (GCC/Clang)
#define MAX(a, b) ({ \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b; \
})

// Two-level stringify (expand first)
#define STR(x) STR_IMPL(x)
#define STR_IMPL(x) #x

// X-macro pattern
#define LIST(X) X(A) X(B) X(C)
#define GEN_ENUM(x) x,
enum { LIST(GEN_ENUM) };

// Include guard
#ifndef HEADER_H
#define HEADER_H
// ... content ...
#endif

// ─── COMPILER DETECTION ───
#if defined(__clang__)
#elif defined(__GNUC__)
#elif defined(_MSC_VER)
#endif

// ─── PLATFORM DETECTION ───
#if defined(_WIN32)
#elif defined(__APPLE__)
#elif defined(__linux__)
#elif defined(__FreeBSD__)
#endif

// ─── ARCHITECTURE DETECTION ───
#if defined(__x86_64__) || defined(_M_X64)
#elif defined(__i386__) || defined(_M_IX86)
#elif defined(__aarch64__) || defined(_M_ARM64)
#endif
```

---

## Summary

The C preprocessor is a powerful text-processing tool that runs before compilation. Key capabilities:

| Feature         | Syntax                | Use Case                 |
| --------------- | --------------------- | ------------------------ |
| Object macros   | `#define NAME value`  | Constants, configuration |
| Function macros | `#define F(x) (x)`    | Inline code, generics    |
| Stringification | `#x`                  | Debug output, logging    |
| Token pasting   | `a##b`                | Generate identifiers     |
| Variadic        | `...` / `__VA_ARGS__` | Printf-like macros       |
| Conditionals    | `#if` / `#ifdef`      | Platform code, features  |
| Includes        | `#include`            | Header files             |

**Best Practices:**

1. Use ALL_CAPS for macro names, why? Because it improves readability and helps distinguish macros from functions and variables.
2. Parenthesize all arguments and the whole expression, why? Because it ensures the macro is evaluated correctly and consistently, avoiding potential issues with operator precedence and evaluation order.
3. Use `do { } while(0)` for multi-statement macros, why? Because it ensures the macro behaves like a single statement, allowing it to be used safely in conditional statements and other control structures without causing syntax errors or unexpected behavior.
4. Prefer `static inline` functions when possible, why? Because it avoids the overhead of function calls, making the code faster and more efficient.
5. Document macros that have side effects or evaluate arguments multiple times, why? Because it helps users understand the behavior of the macro and how to use it correctly.
6. Use include guards or `#pragma once`, why? Because it prevents the same header file from being included multiple times, avoiding errors and inefficiencies.
7. Prefix macros with project name to avoid collisions, why? Because it prevents naming conflicts with macros from other libraries or parts of the codebase.

**When to use macros vs functions:**

- **Use macros for:** `__FILE__`/`__LINE__`, token pasting, stringification, conditional compilation, type-generic code
- **Use functions for:** Everything else (type safety, debugging, no multiple evaluation issues)
- **Use inline functions for:** Performance-critical code that benefits from inlining without the downsides of macros
