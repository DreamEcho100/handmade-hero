# Lesson 14 -- Arena TempMemory Checkpoints

> **What you will build:** A checkpoint system for arenas that saves a position,
> allocates freely, then rolls back -- giving you scoped lifetimes without
> individual `free` calls.

## What you'll learn

- Why arenas need a way to free *some* allocations without freeing all of them
- How the `TempMemory` struct saves a checkpoint (block + byte offset)
- How `arena_begin_temp` / `arena_end_temp` create scoped lifetimes
- How nested temp scopes work like nested function call frames
- How `arena_keep_temp` conditionally commits or rolls back trial allocations
- How `arena_check` catches orphaned temp scopes at boundary points

## Prerequisites

- Lessons 10-13 (arena push, reset, chained blocks, mmap)
- Understanding of the `ArenaBlock` linked list from Lesson 12

---

## Step 1 -- Understand the problem: permanent vs temporary allocations

Up to now, you have two options with an arena: push (allocate) and reset (free
everything). But real programs need something in between. Consider a game loop:

```
  Frame N:
  1. Load a permanent texture (must survive across frames)
  2. Compute temporary collision results (only needed this frame)
  3. Build a temporary debug string (only needed this frame)
  4. End of frame -- want to free #2 and #3, but KEEP #1
```

`arena_reset` would destroy the texture along with the scratch data. You need a
way to say "free everything *after this point*, but keep everything before it."

In JavaScript, the garbage collector handles this automatically. When nothing
references the collision results after the frame ends, the GC collects them. The
texture survives because the game still holds a reference. In C, you need an
explicit mechanism.

> **Why?** This is the fundamental limitation of the plain arena from Lessons
> 10-12. Without checkpoints, you can only free everything or nothing. It is
> like a database that only supports `DROP TABLE` -- no `ROLLBACK TO SAVEPOINT`.
> TempMemory adds the savepoint.

Start with the lesson skeleton:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

int main(void) {
    printf("Lesson 14 -- Arena TempMemory Checkpoints\n\n");

    Arena arena = {0};
    arena.min_block_size = 4096;

    return 0;
}
```

Build and run:

```bash
./build-dev.sh --lesson=14 -r
```

---

## Step 2 -- Read the TempMemory struct

Open `src/lib/arena.h` and find the `TempMemory` definition:

```c
typedef struct TempMemory {
    Arena      *arena;
    ArenaBlock *block;   // which block we were in at checkpoint time
    size_t      used;    // how far into that block
} TempMemory;
```

Three fields. That is all a checkpoint needs:

```
  TempMemory saves a snapshot:

  Arena at checkpoint time:
  Block chain:  [block A] <-- [block B (current)]
                                  used = 200

  TempMemory t = {
      .arena = &arena,
      .block = block_B,     // the block that was current
      .used  = 200           // how many bytes were used in that block
  };

  After more allocations:
  Block chain:  [block A] <-- [block B] <-- [block C (current)]
                                                 used = 500

  arena_end_temp(t):
  1. Free block C (it was created after the checkpoint)
  2. Set current back to block B
  3. Set block B's used back to 200
  Result: arena is exactly where it was at checkpoint time
```

> **Why?** In JavaScript, a `try/finally` block guarantees cleanup runs. In a
> database, `SAVEPOINT` + `ROLLBACK TO SAVEPOINT` restores state. `TempMemory`
> is the arena equivalent -- it records exactly where the arena was, and
> `arena_end_temp` restores it. The difference from GC: this is instant and
> deterministic. You control exactly when the cleanup happens.

---

## Step 3 -- Use arena_begin_temp and arena_end_temp

Add a basic temp scope demo. Push a permanent allocation first, then open a
temp scope, push scratch data, and close the scope:

```c
/* Permanent allocation -- survives the temp scope */
int *permanent = ARENA_PUSH_STRUCT(&arena, int);
*permanent = 42;
printf("  Permanent alloc: %p -> %d\n", (void *)permanent, *permanent);
printf("  Arena used: %zu bytes\n\n", arena_total_used(&arena));

/* Begin temp scope */
TempMemory temp = arena_begin_temp(&arena);
printf("  --- TempMemory BEGIN ---\n");

/* Temp allocations */
float *scratch_buf = ARENA_PUSH_ARRAY(&arena, 100, float);
for (int i = 0; i < 100; i++) scratch_buf[i] = (float)i;

char *temp_str = (char *)arena_push_size(&arena, 256, 1);
snprintf(temp_str, 256, "This string is temporary!");

printf("  Scratch buf: %p (100 floats)\n", (void *)scratch_buf);
printf("  Temp string: \"%s\"\n", temp_str);
printf("  Arena used: %zu bytes\n\n", arena_total_used(&arena));

/* End temp scope -- all temp allocations freed */
arena_end_temp(temp);
printf("  --- TempMemory END ---\n");
printf("  Arena used: %zu bytes (back to before temp scope)\n",
       arena_total_used(&arena));
printf("  Permanent still valid: %p -> %d\n",
       (void *)permanent, *permanent);
```

Build and run:

```bash
./build-dev.sh --lesson=14 -r
```

You should see the arena usage jump during the temp scope, then drop back to
the pre-checkpoint level. The permanent int survives. The scratch buffer and
temp string are gone.

```
  Timeline:

  1. Push permanent int:
     [  permanent (4 bytes)  |              free              ]
                              ^ used

  2. arena_begin_temp -> checkpoint saved at this offset

  3. Push scratch_buf (400 bytes) + temp_str (256 bytes):
     [  permanent  | scratch_buf...  | temp_str... |   free   ]
                                                     ^ used

  4. arena_end_temp -> restore checkpoint:
     [  permanent  |              free                        ]
                    ^ used (rolled back)

     scratch_buf and temp_str: gone. Memory is available for reuse.
     permanent: untouched.
```

> **Why?** This is like a JavaScript block scope where local variables are
> garbage-collected when the block ends, except here it is instant and
> deterministic. No GC pause. No reference counting. Just a pointer reset.

---

## Step 4 -- Understand how arena_end_temp works internally

Look at the implementation in `arena.h`:

```c
static inline void arena_end_temp(TempMemory t) {
    Arena *arena = t.arena;
    // Free any blocks allocated AFTER the checkpoint
    while (arena->current != t.block) {
        ArenaBlock *to_free = arena->current;
        arena->current = to_free->prev;
        free(to_free);
    }
    // Restore the used offset on the checkpoint block
    if (arena->current)
        arena->current->used = t.used;
    assert(arena->temp_count > 0);
    --arena->temp_count;
}
```

Two things happen:

1. **Block cleanup**: If the temp scope caused new blocks to be allocated
   (because the original block filled up), those overflow blocks are freed
   entirely. The `while` loop walks backward through the block chain until it
   reaches the checkpoint block.

2. **Offset restore**: The checkpoint block's `used` field is reset to what it
   was when `arena_begin_temp` was called. Everything pushed after the
   checkpoint is now logically gone -- the memory is available for reuse.

The `temp_count` field tracks how many checkpoints are outstanding. This is
bookkeeping for `arena_check` (Step 7).

> **Why?** The key insight: `arena_end_temp` does not zero the memory or walk
> through individual allocations. It just resets a single integer (`used`) and
> frees any overflow blocks. This is O(number of overflow blocks), which is
> almost always O(1) in practice. Compare that to a GC which must trace every
> live object.

---

## Step 5 -- Nest temp scopes like function call frames

Temp scopes nest. An inner scope can be opened and closed without affecting the
outer scope, just like nested function calls on the stack:

```c
arena_reset(&arena);

int *base = ARENA_PUSH_STRUCT(&arena, int);
*base = 1;
printf("  Level 0: base=%d  used=%zu\n", *base, arena_total_used(&arena));

TempMemory t1 = arena_begin_temp(&arena);
{
    int *level1 = ARENA_PUSH_ARRAY(&arena, 50, int);
    level1[0] = 2;
    printf("  Level 1: level1[0]=%d  used=%zu\n",
           level1[0], arena_total_used(&arena));

    TempMemory t2 = arena_begin_temp(&arena);
    {
        int *level2 = ARENA_PUSH_ARRAY(&arena, 100, int);
        level2[0] = 3;
        printf("  Level 2: level2[0]=%d  used=%zu\n",
               level2[0], arena_total_used(&arena));

        arena_end_temp(t2);   // level2 freed, level1 still alive
        printf("  After end_temp(t2): used=%zu\n",
               arena_total_used(&arena));
    }

    printf("  level1[0] still = %d\n", level1[0]);

    arena_end_temp(t1);       // level1 freed, base still alive
    printf("  After end_temp(t1): used=%zu\n",
           arena_total_used(&arena));
}

printf("  base still = %d\n", *base);
```

Build and run:

```bash
./build-dev.sh --lesson=14 -r
```

```
  Nesting diagram:

  ┌── base (permanent) ──────────────────────────────────────┐
  │                                                          │
  │  ┌── t1 scope ──────────────────────────────────────┐    │
  │  │                                                  │    │
  │  │  level1 (50 ints)                                │    │
  │  │                                                  │    │
  │  │  ┌── t2 scope ────────────────────────────┐      │    │
  │  │  │                                        │      │    │
  │  │  │  level2 (100 ints)                     │      │    │
  │  │  │                                        │      │    │
  │  │  └── end_temp(t2): level2 freed ──────────┘      │    │
  │  │                                                  │    │
  │  │  level1 still alive                              │    │
  │  │                                                  │    │
  │  └── end_temp(t1): level1 freed ────────────────────┘    │
  │                                                          │
  │  base still alive                                        │
  └──────────────────────────────────────────────────────────┘
```

**Critical rule**: scopes must be ended in reverse order (LIFO). Ending `t1`
before `t2` would corrupt the arena state. This is exactly like stack unwinding
-- inner scopes close before outer ones.

> **Why?** In JavaScript, nested `try/finally` blocks unwind in reverse order
> automatically. In C with arenas, you must enforce this discipline yourself.
> The `temp_count` field helps catch mistakes (see Step 7), but the LIFO rule
> is your responsibility.

---

## Step 6 -- Conditionally commit with arena_keep_temp

Sometimes you start a temp scope for speculative work -- try to load an asset,
parse a file, or validate input. If the work succeeds, you want to *keep* the
allocations. If it fails, you want to roll back.

`arena_keep_temp` handles the success case. It just decrements `temp_count`
without restoring the checkpoint. The memory stays allocated:

```c
arena_reset(&arena);
printf("  Try-load pattern: keep if valid, roll back if not.\n\n");

for (int trial = 0; trial < 3; trial++) {
    TempMemory t = arena_begin_temp(&arena);

    /* Simulate asset loading */
    int *asset = ARENA_PUSH_ARRAY(&arena, 256, int);
    for (int i = 0; i < 256; i++) asset[i] = i + trial;

    /* Simulate validation (fail on trial 1) */
    int valid = (trial != 1);

    if (valid) {
        arena_keep_temp(t);   // commit -- keep the allocation
        printf("  Trial %d: VALID -> kept (used=%zu)\n",
               trial, arena_total_used(&arena));
    } else {
        arena_end_temp(t);    // rollback -- free the allocation
        printf("  Trial %d: INVALID -> rolled back (used=%zu)\n",
               trial, arena_total_used(&arena));
    }
}
```

Build and run:

```bash
./build-dev.sh --lesson=14 -r
```

You should see the arena grow after trials 0 and 2 (valid) but snap back after
trial 1 (invalid).

```
  Trial flow:

  Trial 0:  [begin] -> [push 256 ints] -> VALID -> [keep_temp]
            Arena grows by 1024 bytes and stays.

  Trial 1:  [begin] -> [push 256 ints] -> INVALID -> [end_temp]
            Arena snaps back. The 1024 bytes are reclaimed.

  Trial 2:  [begin] -> [push 256 ints] -> VALID -> [keep_temp]
            Arena grows by 1024 bytes and stays.
```

> **Why?** This is the arena equivalent of an optimistic transaction. In
> TypeScript, you might do `try { JSON.parse(input) } catch { return null }`.
> With arenas, you do `begin_temp` / work / `keep_temp` or `end_temp`. The
> arena makes rollback free -- literally a pointer reset.

---

## Step 7 -- Catch mismatched scopes with arena_check

Every `arena_begin_temp` increments `arena->temp_count`. Every `arena_end_temp`
or `arena_keep_temp` decrements it. If you forget to close a temp scope,
`temp_count` will be non-zero at a boundary point.

`arena_check` asserts that `temp_count == 0`:

```c
arena_reset(&arena);

TempMemory t = arena_begin_temp(&arena);
arena_push_size(&arena, 100, 1);
arena_end_temp(t);

arena_check(&arena);   // should pass -- temp_count is 0
printf("  arena_check passed (no orphaned temp scopes).\n");
```

Build and run:

```bash
./build-dev.sh --lesson=14 -r
```

If you comment out the `arena_end_temp(t)` line and rebuild, the `arena_check`
assertion will fire. This catches the arena equivalent of a resource leak -- a
temp scope that was opened but never closed.

```
  arena_check catches:

  arena_begin_temp(&arena);     // temp_count = 1
  // ... forgot arena_end_temp ...
  arena_check(&arena);          // ASSERT: temp_count != 0 !!!
```

> **Why?** In JavaScript, you have linters like ESLint that catch unclosed
> resources (open file handles, event listeners). `arena_check` serves the same
> purpose for arena temp scopes. Call it at frame boundaries, request
> boundaries, or any natural "everything should be clean" point.

---

## Step 8 -- The complete per-frame scratch pattern

Put it all together. This is the pattern you will use in every game loop,
animation frame, or request handler:

```c
Arena scratch = {0};
scratch.min_block_size = 64 * 1024;

for (int frame = 0; frame < 5; frame++) {
    TempMemory f = arena_begin_temp(&scratch);

    /* Frame work -- allocate freely */
    char *msg = (char *)arena_push_size(&scratch, 64, 1);
    snprintf(msg, 64, "Frame %d: scratch used=%zu",
             frame, arena_total_used(&scratch));
    printf("    %s\n", msg);

    arena_end_temp(f);
    arena_check(&scratch);
}
printf("\n  Each frame cleans up perfectly.  Zero leaks by design.\n");

arena_free(&scratch);
```

Build and run:

```bash
./build-dev.sh --lesson=14 -r
```

Every frame: begin a temp scope, do all work, end the scope, check for
mismatches. The scratch arena reuses the same memory every frame. No leaks. No
GC. No individual frees.

```
  Per-frame pattern:

  Frame 0: [begin_temp] -> [push msg] -> [end_temp] -> [check] -> OK
  Frame 1: [begin_temp] -> [push msg] -> [end_temp] -> [check] -> OK
  Frame 2: [begin_temp] -> [push msg] -> [end_temp] -> [check] -> OK
  ...

  Memory usage: flat line. Same bytes reused every frame.
  Leak count: zero, by construction.
```

> **Why?** This pattern replaces the garbage collector for frame-scoped data.
> In React, the reconciler allocates temporary fiber trees each render and the
> GC cleans them up (eventually, unpredictably). With arenas, cleanup is instant
> and happens exactly when you want it -- at the frame boundary. This is why
> game engines written in C do not need garbage collectors.

---

## Build and run

```bash
./build-dev.sh --lesson=14 -r
```

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `arena_check` assertion fires | A `TempMemory` begun but never ended or kept | Add the missing `arena_end_temp` or `arena_keep_temp` |
| Use-after-free crash | Accessing data after `arena_end_temp` freed it | Move access before `arena_end_temp`, or use `arena_keep_temp` |
| Ending scopes in wrong order | Ending `t1` before `t2` | Temp scopes are LIFO -- always end inner scopes first |
| `arena_keep_temp` confusion | It does not "move" data anywhere | It just decrements `temp_count`. The data was already in the arena. |

## Exercises

1. **Triple nesting**: Create three levels of nesting (t1, t2, t3). Allocate
   data at each level, end them in reverse order, and verify each level's data
   is freed independently.

2. **Intentional mismatch**: Comment out an `arena_end_temp` call and verify
   that `arena_check` catches it. Then fix it. Get comfortable with the assert
   message.

3. **Keep-or-discard loop**: Write a loop that "loads" 10 assets, keeps the
   even-numbered ones, and rolls back the odd-numbered ones. Print the arena
   usage after each iteration to see the staircase pattern.

## Connection to your work

If you have ever written an Express.js middleware that allocates buffers for
request parsing, you know the cleanup problem. Each buffer needs a `finally`
block or a resource tracker. With arena temp scopes, you wrap the entire
request handler in `begin_temp` / `end_temp` and all allocations are freed
instantly when the response is sent. No `finally`, no resource tracker, no GC.

## What's next

Lesson 15 takes the per-frame pattern from Step 8 and shows three complete
arena usage patterns: per-frame (game loop), per-request (web server), and
per-thread (parallel work). These patterns replace the garbage collector in
real C systems.
