# Lesson 19 -- Free List on Arena

> **What you'll build:** A free list overlay on an arena that enables individual deallocation of variable-sized blocks, demonstrated by building and modifying a linked list with add/remove operations.

## Observable outcome

Run the program and you see three blocks of different sizes allocated, one freed and its slot reused by a smaller allocation, a linked list (10-20-30-40-50) built with freelist_alloc and modified by removing nodes and adding new ones that reuse freed memory, a mixed-size allocation pattern showing reuse statistics, and (with `--bench`) freelist vs malloc performance.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| Free list overlay | Like adding a recycling bin on top of an arena -- reuse freed blocks before allocating fresh |
| First-fit search | Like `Array.find(block => block.size >= needed)` on the free list |
| Variable-size dealloc | Unlike the pool (fixed-size), the free list handles blocks of any size |
| `FreeListNode` header | Each freed block stores its size and a next-pointer in its own memory |
| Arena as fallback | If no free block fits, bump the arena as usual |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_19.c` | New | Free list usage, linked list operations, mixed-size patterns, benchmark |
| `src/lib/freelist.h` | Dependency | `FreeList`, `freelist_init`, `freelist_alloc`, `freelist_free` |
| `src/lib/freelist.c` | Dependency | First-fit search and free block management |
| `src/lib/arena.h` | Dependency | Arena backing store |

---

## Background -- when arenas and pools are not enough

A plain arena cannot free individual allocations -- only bulk reset. A pool allocator handles individual free, but only for fixed-size objects. What about variable-size objects with unpredictable lifetimes? Nodes in a graph, entities in a scene, cache entries with varying TTLs?

The free list overlay adds a recycling layer on top of the arena. Freed blocks go onto a linked list. On allocation, the free list is searched first. If a block fits, it is reused. If nothing fits, the arena bumps forward as usual.

```
  Free list on arena:

  Arena memory:
  [ A (32B) ][ B (64B) ][ C (128B) ][ D (48B) ][ ... free space ... ]
                 ↑ freed                  ↑ freed

  Free list:
  free_head ──> [ B: size=64, next ──> D: size=48, next ──> NULL ]

  freelist_alloc(50 bytes):
    Search: B has 64 bytes >= 50? Yes, and aligned? Yes.
    Remove B from free list, return it.
    (D stays on the list for future reuse.)
```

### The FreeListNode

```c
typedef struct FreeListNode {
    size_t               size;   // usable bytes (not including this header)
    struct FreeListNode *next;   // next free block
} FreeListNode;
```

When a block is freed, its memory is repurposed to store this node. The first bytes hold `size` and `next`. When the block is allocated again, the user data overwrites these fields.

This means every allocation must be at least `sizeof(FreeListNode)` bytes. The implementation enforces this:

```c
size_t min_size = sizeof(FreeListNode);
if (size < min_size) size = min_size;
```

### First-fit search (freelist.c lines 17-45)

```c
FreeListNode **prev_next = &fl->free_head;
FreeListNode  *node      = fl->free_head;

while (node) {
    if (node->size >= size && ((uintptr_t)node % align == 0)) {
        *prev_next = node->next;    // remove from list
        memset(node, 0, size);      // zero the returned memory
        return (void *)node;
    }
    prev_next = &node->next;
    node = node->next;
}

// Nothing fits -- bump the arena
return arena_push_zero(fl->arena, size, align);
```

The `prev_next` double-pointer trick lets us remove a node from the linked list without special-casing the head. If `node` is the head, `*prev_next` modifies `fl->free_head`. If `node` is in the middle, `*prev_next` modifies the previous node's `next` field.

The alignment check `(uintptr_t)node % align == 0` ensures the freed block meets the caller's alignment requirement. A block freed from a 4-aligned allocation might not work for an 8-aligned request.

### Free operation (freelist.c lines 47-59)

```c
void freelist_free(FreeList *fl, void *ptr, size_t size) {
    FreeListNode *node = (FreeListNode *)ptr;
    node->size = size;
    node->next = fl->free_head;
    fl->free_head = node;
}
```

Freeing is O(1): cast the block to a `FreeListNode`, store its size, prepend to the list. No search, no merge.

**Important**: you must pass the correct `size` when freeing. The free list has no way to know how large the block was. Getting this wrong corrupts the free list.

---

## Code walkthrough

### Section 1: basic free list usage (lines 46-70)

```c
FreeList fl;
freelist_init(&fl, &arena);

void *a = freelist_alloc(&fl, 32, 8);
void *b = freelist_alloc(&fl, 64, 8);
void *c = freelist_alloc(&fl, 128, 8);
```

Three blocks of different sizes, all allocated from the arena via the free list. Since the free list starts empty, all three come from the arena's bump pointer.

```c
freelist_free(&fl, b, 64);
void *d = freelist_alloc(&fl, 48, 8);
// d == b (reused the 64-byte slot for a 48-byte request)
```

Freeing `b` puts its 64-byte block on the free list. The next allocation (48 bytes) fits inside b's 64-byte block, so the free list returns it. The remaining 16 bytes are wasted (our implementation does not split blocks).

### Section 2: linked list with add/remove (lines 72-130)

This section builds a practical data structure using the free list:

```c
// Build list: 10 -> 20 -> 30 -> 40 -> 50
for (int v = 50; v >= 10; v -= 10) {
    ListNode *n = (ListNode *)freelist_alloc(&fl, sizeof(ListNode), _Alignof(ListNode));
    n->value = v;
    n->next = head;
    head = n;
}
```

Each node is allocated from the free list. Removing a node frees its memory back to the list:

```c
// Remove node with value 30
freelist_free(&fl, cur, sizeof(ListNode));
```

Adding node 15 afterward reuses the freed memory:

```c
ListNode *n15 = (ListNode *)freelist_alloc(&fl, sizeof(ListNode), _Alignof(ListNode));
n15->value = 15;
n15->next = head;
head = n15;
```

This is the natural add/remove pattern that a plain arena cannot support. The free list enables it while the arena still provides bulk cleanup at the end.

### Section 3: mixed-size allocation pattern (lines 132-168)

Twenty blocks of varying sizes (16 to 256 bytes) are allocated, even-indexed blocks are freed, then 10 new 64-byte blocks are allocated. The program reports how many reused addresses from the free list. This demonstrates real-world churn with variable sizes.

### Section 4: when to use a free list (lines 170-181)

The program prints decision guidance:

- **USE** free list: variable sizes, unpredictable lifetimes, individual deallocation needed
- **DON'T USE**: all same size (use pool), all die together (use plain arena), need O(1) guaranteed (free list search is O(n))

---

## Build and run

```bash
./build-dev.sh --lesson=19 -r
./build-dev.sh --lesson=19 --bench -r   # with benchmarks
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `freelist_free` corrupts data | Passed wrong `size` that does not match original allocation | Always free with the exact same size you allocated |
| Free list never reuses blocks | Requested size slightly larger than any free block, or alignment mismatch | First-fit only matches if block is large enough AND correctly aligned |
| Memory appears to "leak" | Freed blocks are on the free list but arena still holds them | Expected behavior -- `arena_free` reclaims everything at the end |
| Fragmentation over time | Many small free blocks, no coalescing | Production free lists merge adjacent blocks; ours does not (simplicity trade-off) |

---

## Key takeaways

1. The free list overlays recycling on top of an arena. Freed blocks are reused before bumping the arena.
2. First-fit search walks the free list looking for the first block that is large enough and correctly aligned. This is O(n) in the worst case.
3. `freelist_free` requires you to pass the correct size. The free list stores size inside the freed block itself.
4. Use a free list when you need variable-size individual deallocation. Use a pool for fixed-size. Use a plain arena when everything dies together.
5. The arena still owns all memory. `arena_free` releases everything, including free-list metadata.
