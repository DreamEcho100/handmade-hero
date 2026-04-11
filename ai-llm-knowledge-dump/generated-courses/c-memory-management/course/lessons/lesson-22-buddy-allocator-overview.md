# Lesson 22 -- Buddy Allocator Overview

> **What you'll build:** A self-contained buddy allocator that splits power-of-two blocks on allocation and coalesces them on free, with visual demonstrations of splitting, merging, and internal fragmentation analysis.

## Observable outcome

The program prints ASCII diagrams of how a 1024-byte block splits down to serve a 100-byte request. It allocates and frees blocks, printing free list state at each step and showing full coalescing back to the original block. A fragmentation table shows wasted bytes for various request sizes.

## New concepts

- Buddy system: memory divided into power-of-two blocks that split and merge in pairs
- Splitting: halving a block too large for a request, putting the unused half on a free list
- Coalescing: merging a freed block with its "buddy" (sibling) when both are free
- Buddy address calculation via XOR
- Internal fragmentation: wasted bytes from rounding to the next power of two

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_22.c` | New | Self-contained buddy allocator implementation and demos |

---

## Background -- the problem with free lists

In previous lessons, we saw pool and slab allocators for fixed-size objects. But what if you need variable-size allocations *and* want to avoid external fragmentation?

A naive free list accumulates freed blocks of different sizes. Over time, you might have plenty of total free memory but no single block large enough for a new request -- this is external fragmentation. Merging adjacent free blocks is hard because you do not know which blocks are neighbors.

### How the buddy system solves it

The buddy allocator constrains all block sizes to powers of two. This seemingly wasteful restriction buys a powerful property: every block has exactly one "buddy" at a computable address.

```
JS analogy:

  // Imagine a typed array you can only slice into powers of 2
  const memory = new ArrayBuffer(1024);

  // Need 100 bytes? Round up to 128.
  // To get 128 from 1024: split 1024->512->256->128
  // Each split creates a "buddy" that goes on a free list
```

### The splitting and coalescing process

```
  Request: 100 bytes (rounds up to 128 = 2^7)

  Step 1: Start with one 1024-byte block (order 10)
  [====================== 1024 ======================]

  Step 2: Split 1024 -> two 512s (order 9)
  [========= 512 =========][======= 512 free ========]

  Step 3: Split 512 -> two 256s (order 8)
  [==== 256 ====][= 256 free =][===== 512 free ======]

  Step 4: Split 256 -> two 128s (order 7)
  [128 USED][128 free][ 256 free  ][   512 free      ]

  Free lists after allocation:
    Order 7 (128B): 1 block
    Order 8 (256B): 1 block
    Order 9 (512B): 1 block
```

On free, the process reverses. The allocator checks: is my buddy also free? If yes, merge and check the next level up, all the way to the top.

### The XOR trick for finding buddies

Every block's buddy is at a predictable address computed with XOR:

```
  buddy_offset = block_offset XOR block_size

  Example: block at offset 0x100, size 128 (0x80)
    buddy = 0x100 XOR 0x080 = 0x180

  Example: block at offset 0x180, size 128
    buddy = 0x180 XOR 0x080 = 0x100    (same pair!)
```

This works because power-of-two alignment means the buddy differs by exactly one bit. XOR flips that bit, toggling between the left and right halves of the parent block.

---

## Code walkthrough

### Key structures

The buddy allocator is entirely self-contained in `lesson_22.c` (no separate library). The core structure:

```c
typedef struct BuddyAlloc {
  uint8_t   *memory;                        /* backing memory             */
  size_t     total_size;                     /* total bytes (power of 2)  */
  int        max_order;                      /* log2(total_size)          */
  BuddyNode *free_lists[BUDDY_NUM_ORDERS];  /* one free list per order   */
  uint8_t   *block_orders;                   /* order of each allocation  */
} BuddyAlloc;
```

`free_lists` is an array of linked lists, one per order (power of two). Order 5 = 32 bytes (minimum), up to `max_order`. The `block_orders` array remembers what order each allocation was made at, so `buddy_free` knows how large the block is.

Free list nodes (`BuddyNode`) are stored inside the free blocks themselves -- a doubly-linked list with `next` and `prev` pointers. This is the same intrusive-list trick the slab allocator uses.

### buddy_init

```c
static void buddy_init(BuddyAlloc *ba, size_t size) {
  int order = buddy_log2(size);          /* round up to power of two */
  ba->total_size = buddy_order_size(order);
  ba->memory = (uint8_t *)malloc(ba->total_size);

  /* The entire memory starts as one big free block */
  BuddyNode *node = (BuddyNode *)ba->memory;
  node->next = NULL;
  node->prev = NULL;
  ba->free_lists[order - BUDDY_MIN_ORDER] = node;
}
```

After init, there is exactly one free block (the entire memory) on the highest-order free list. All other free lists are empty.

### buddy_alloc -- splitting down

```c
static void *buddy_alloc(BuddyAlloc *ba, size_t size) {
  int order = buddy_log2(size);          /* smallest order that fits */

  /* Find a free block (may need to go to a higher order) */
  int found_order = -1;
  for (int o = order; o <= ba->max_order; o++) {
    if (ba->free_lists[o - BUDDY_MIN_ORDER]) {
      found_order = o;
      break;
    }
  }
  if (found_order < 0) return NULL;      /* out of memory */

  /* Pop from found order's free list */
  BuddyNode *block = ba->free_lists[found_idx];
  buddy_list_remove(&ba->free_lists[found_idx], block);

  /* Split down to target order */
  while (found_order > order) {
    found_order--;
    BuddyNode *buddy = (BuddyNode *)((uint8_t *)block +
                                       buddy_order_size(found_order));
    buddy_list_push(&ba->free_lists[split_idx], buddy);
  }
  return (void *)block;
}
```

The search walks up from the requested order until it finds a free block. Then it splits that block repeatedly, putting each right-half buddy on its order's free list, until it reaches the target size.

### buddy_free -- coalescing up

```c
static void buddy_free(BuddyAlloc *ba, void *ptr) {
  int order = ba->block_orders[buddy_block_index(ba, ptr)];

  while (order < ba->max_order) {
    size_t boff = buddy_offset(ba, ptr, order);
    BuddyNode *buddy_node = (BuddyNode *)(ba->memory + boff);

    /* Is the buddy free at this order? */
    if (!buddy_list_contains(ba->free_lists[list_idx], buddy_node))
      break;                              /* buddy is allocated, stop */

    /* Merge: remove buddy from free list, combine */
    buddy_list_remove(&ba->free_lists[list_idx], buddy_node);
    if ((uint8_t *)buddy_node < (uint8_t *)ptr)
      ptr = (void *)buddy_node;           /* merged block starts at lower addr */
    order++;
  }

  /* Add the (possibly merged) block to its free list */
  buddy_list_push(&ba->free_lists[list_idx], (BuddyNode *)ptr);
}
```

Free tries to merge upward. At each level, it XORs to find the buddy, checks if the buddy is on the free list for that order, and if so, removes the buddy and moves up one order. This continues until the buddy is not free or we reach the maximum order.

### Internal fragmentation table

The demo computes waste for various request sizes:

```
  +-------------+-------------+-------------------+
  | Requested   | Allocated   | Waste             |
  +-------------+-------------+-------------------+
  |      33 B   |      64 B   |    31 B (48%)     |
  |      65 B   |     128 B   |    63 B (49%)     |
  |     100 B   |     128 B   |    28 B (21%)     |
  |     200 B   |     256 B   |    56 B (21%)     |
  |     500 B   |     512 B   |    12 B  (2%)     |
  +-------------+-------------+-------------------+
```

Average waste is roughly 25%. This is the cost of fast O(log n) splitting and coalescing.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Coalescing never happens | Buddy is allocated (not free), so merge is blocked | This is correct behavior; both siblings must be free to merge |
| `buddy_alloc` returns NULL unexpectedly | All blocks are allocated or fragmented into small orders | The backing memory is finite; request a larger initial size |
| Buddy address calculation wrong | Using addition instead of XOR | The buddy's offset is `block_offset XOR block_size`, never addition |
| Performance worse than malloc | `buddy_list_contains` is O(n); real implementations use bitmaps | This is a teaching implementation; production buddy allocators track free/allocated state in O(1) with bitmaps |

---

## Build and run

```bash
./build-dev.sh --lesson=22 -r              # splitting and coalescing demos
./build-dev.sh --lesson=22 --bench -r      # buddy vs malloc benchmark
```

---

## Key takeaways

1. The buddy system trades internal fragmentation (~25% average waste) for guaranteed O(log n) allocation, O(log n) free, and zero external fragmentation.
2. The XOR trick for finding buddies only works because block sizes are powers of two and blocks are naturally aligned. This is the key insight that makes the whole algorithm work.
3. The Linux kernel uses buddy allocation for physical page management. User-space allocators like jemalloc layer slab allocators on top of buddy-managed pages -- exactly the layering we have been building across these lessons.
4. In JS terms, imagine `ArrayBuffer.transfer()` but where the runtime can automatically merge adjacent freed buffers back into larger ones. That is what coalescing gives you -- and JavaScript's GC does this for you, while in C you build it yourself.
