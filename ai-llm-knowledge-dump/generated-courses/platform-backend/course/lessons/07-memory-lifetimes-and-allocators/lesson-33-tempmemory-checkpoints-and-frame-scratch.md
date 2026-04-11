# Lesson 33: `TempMemory` Checkpoints and Frame Scratch

## Frontmatter

- Module: `07-memory-lifetimes-and-allocators`
- Lesson: `33`
- Status: Required
- Target files:
  - `src/utils/arena.h`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
- Target symbols:
  - `TempMemory`
  - `arena_begin_temp`
  - `arena_end_temp`
  - `arena_keep_temp`
  - `arena_check`

## Observable Outcome

By the end of this lesson, you should be able to explain how a temp checkpoint rewinds an arena, why the frame loop wraps scratch memory every frame, and how nested temp scopes inside rendering stay safe as long as they unwind in reverse order.

## Prerequisite Bridge

Lesson 32 showed how arenas allocate quickly by pushing forward.

That still leaves a crucial question:

```text
how do we cheaply throw away temporary allocations
without resetting the whole arena all the time?
```

The answer is `TempMemory`.

## Step 1: Read `TempMemory` as a Saved Rewind Point

In `src/utils/arena.h`:

```c
typedef struct TempMemory {
  Arena *arena;
  ArenaBlock *block;
  size_t used;
} TempMemory;
```

This struct stores exactly enough information to return the arena to an earlier state:

- which arena was checkpointed
- which block was current at that time
- how many bytes of that block had been used

That is why temp checkpoints are cheap.

They do not copy allocations. They only remember where to rewind.

## Step 2: Walk `arena_begin_temp(...)`

The begin helper does four jobs:

```c
t.arena = arena;
t.block = arena->current;
t.used = arena->current ? arena->current->used : 0;
++arena->temp_count;
```

It also updates peak temp-scope stats.

So the checkpoint cost is basically:

```text
snapshot a pointer, snapshot a used offset, increment bookkeeping
```

That is why it is reasonable to use temp scopes per frame and even nested inside a frame.

## Step 3: Read `arena_end_temp(...)` as "Rewind to Snapshot"

The end helper has two rewind jobs.

### Job 1: Free whole overflow blocks created after the checkpoint

```c
while (arena->current != t.block) {
  ArenaBlock *to_free = arena->current;
  arena->current = to_free->prev;
  ...
  free(to_free);
}
```

### Job 2: Rewind the surviving current block to the saved `used` value

```c
if (arena->current) {
  size_t freed_bytes = arena->current->used - t.used;
  ...
  arena->current->used = t.used;
}
```

That means the checkpoint semantics are:

```text
remove every allocation made after begin_temp,
including whole overflow blocks if the arena grew during that scope
```

## Step 4: Notice the Debug Fill on Rewind

In debug builds, the freed bytes are filled with `ARENA_DEBUG_FILL_FREE`.

That is important.

It means temp scopes are not only about convenience. They also make stale-memory mistakes louder during debugging.

## Step 5: `arena_keep_temp(...)` Means "Commit This Scope"

Sometimes you open a temp scope and later decide its allocations should stay.

The helper for that is:

```c
static inline void arena_keep_temp(TempMemory t) {
  assert(t.arena->temp_count > 0);
  --t.arena->temp_count;
}
```

This does not rewind anything.

It only tells the arena bookkeeping:

```text
this temp scope ended, but its allocations remain valid
```

That is subtler than `arena_end_temp(...)`, and it is worth keeping separate in your mental model.

## Step 6: `arena_check(...)` Verifies Scope Discipline

The helper is small but important:

```c
assert(arena->temp_count == 0 &&
       "arena_check: unmatched arena_begin_temp / arena_end_temp");
```

This catches orphaned temp scopes.

In other words, the runtime does not merely hope its scratch arena got rewound correctly. It asserts that every begin had a matching end or keep.

## Step 7: Watch the Per-Frame Pattern in Both Backends

Raylib and X11 both use the same scratch frame boundary:

```c
TempMemory frame_scratch = arena_begin_temp(&props.scratch);
game_app_render(..., &props.scratch);
arena_end_temp(frame_scratch);
arena_check(&props.scratch);
```

This is one of the most important runtime patterns in the whole course.

It means:

```text
the scratch arena starts each frame clean,
any temporary render-time allocations are allowed,
and the whole frame rewinds before the next iteration
```

That is what makes scratch truly frame-lifetime memory.

## Step 8: Study the Nested Temp Scopes in `game_app_render(...)`

In `src/game/main.c`, rendering does not use only one temp scope.

It first creates a top-level render scope:

```c
render_temp = arena_begin_temp(scratch);
```

Then, in the arena-lab path, it may create multiple nested checkpoints:

```c
arena_temps[arena_temp_count] = arena_begin_temp(scratch);
if (arena_push_zero(scratch, bytes, 16))
  arena_temp_count++;
else
  arena_end_temp(arena_temps[arena_temp_count]);
```

And later it unwinds them in reverse order:

```c
while (arena_temp_count > 0) {
  arena_temp_count--;
  arena_end_temp(arena_temps[arena_temp_count]);
}

arena_end_temp(render_temp);
```

That is proper stack discipline.

## Visual: Nested Temp Scopes

```text
begin frame_scratch
  begin render_temp
    begin nested temp 0
    end nested temp 0
    begin nested temp 1
    end nested temp 1
  end render_temp
end frame_scratch
check temp_count == 0
```

Last opened, first closed.

## Step 9: Why Scratch Memory Belongs to This Pattern

Scratch memory is ideal for:

- temporary strings
- temporary layout buffers
- one-frame work arrays
- temporary visualization data

It is a bad home for anything that must survive scene rebuilds or cross-frame simulation.

That is the lifetime rule you must enforce mentally even before the code enforces it physically.

## Practical Exercises

### Exercise 1: Explain Rewind Semantics

What does `arena_end_temp(...)` do if the arena allocated overflow blocks after the checkpoint?

Expected answer:

```text
it frees those later blocks entirely and then rewinds the surviving checkpoint block to the saved used offset
```

### Exercise 2: Explain `arena_check(...)`

Why is `arena_check(&props.scratch)` called after the frame temp ends?

Expected answer:

```text
to assert that no unmatched temp scopes remain and that the scratch arena's checkpoint discipline stayed balanced for the frame
```

### Exercise 3: Explain `arena_keep_temp(...)`

How is `arena_keep_temp(...)` different from `arena_end_temp(...)`?

Expected answer:

```text
keep_temp closes the bookkeeping scope without rewinding allocations, while end_temp actually rewinds the arena to the saved checkpoint
```

## Common Mistakes

### Mistake 1: Treating temp checkpoints as copies of memory

They are rewind markers, not snapshots of data.

### Mistake 2: Ending nested temp scopes in the wrong order

They must unwind like a stack.

### Mistake 3: Storing long-lived pointers into scratch allocations

Those pointers die when the temp scope ends.

## Troubleshooting Your Understanding

### "Why not just call `arena_reset(scratch)` every frame?"

Because temp scopes allow finer-grained nested rewinds inside a frame while still preserving the clean top-level frame boundary.

### "Why does `arena_end_temp(...)` care about overflow blocks?"

Because the scope might have grown the arena beyond the checkpoint block, and a real rewind must undo that growth too.

## Recap

You now understand the scratch-rewind model:

1. `arena_begin_temp(...)` records a rewind point
2. `arena_end_temp(...)` returns the arena to that point
3. `arena_keep_temp(...)` closes bookkeeping without rewinding
4. `arena_check(...)` enforces balanced temp-scope usage
5. both backends wrap every frame in scratch begin/end/check

## Next Lesson

Lesson 34 introduces a different allocator shape: the pool allocator, which trades variable-size push semantics for fixed-size slot reuse and immediate free-list recycling.
