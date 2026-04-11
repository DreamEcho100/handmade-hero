# Lesson 06 -- Memory-Mapped File I/O

> **What you will build:** A program that creates a 4 MB test file, maps it
> into memory with `mmap`, reads it via pointer access (no `fread`, no buffer
> management), computes a checksum, and benchmarks mmap vs traditional `fread`
> to measure the zero-copy advantage.

## What you'll learn

- `mmap` for file-backed mappings -- a file becomes a pointer you can
  dereference
- The kernel's demand paging: only pages you touch are loaded from disk
- Zero-copy reads: your pointer reads directly from the kernel's page cache
- When mmap beats fread and when fread wins
- `MAP_PRIVATE` vs `MAP_SHARED` for file mappings
- Closing the file descriptor after mmap (the mapping survives)

## Prerequisites

- Lesson 05 (virtual memory -- you know mmap, pages, and PROT flags)
- Lesson 03 (heap allocation -- you understand the difference between OS-level
  and user-space allocation)

---

## Step 1 -- The file I/O model you know from Node.js

In Node.js, reading a file looks like this:

```js
const data = fs.readFileSync('big.bin');
// 'data' is a Buffer -- a COPY of the file contents in memory
```

Every `readFileSync` call performs a COPY: the OS reads the file from disk
into the kernel's **page cache** (an in-memory cache of recently accessed file
data), then copies that data from kernel space into your Node.js Buffer in
user space. For a 4 MB file, that is 4 MB of copying. Read the file 20 times
and you copy 80 MB.

```
Traditional file I/O (fread / readFileSync):

   Disk  ------read------>  Kernel page cache  ---copy--->  User buffer
                                                             ^
                                                             |
                                                      your data here
                                                      (a copy)
```

Now imagine you could get a pointer that reads directly from the kernel's page
cache -- no copying at all. That is exactly what `mmap` does:

```
Memory-mapped file I/O (mmap):

   Disk  ------read------>  Kernel page cache
                                  ^
                                  |
                            your pointer reads
                            DIRECTLY from here
                            (zero copies)
```

The difference: `fread` copies data OUT of the page cache into your buffer.
`mmap` lets you read the page cache directly through page table mappings. No
copy, no user-space buffer, no chunk management.

> **Why?** For a game engine loading a 500 MB level file, the difference
> between copying 500 MB into a buffer and just getting a pointer is enormous.
> With mmap, you only load the pages you actually access (demand paging), and
> if the file is already in the page cache from a previous load, access is
> nearly instant -- just pointer dereferences with no system call overhead.

---

## Step 2 -- Creating a test file

Before we can mmap a file, we need one. Let's create a 4 MB file with a
predictable byte pattern so we can verify our reads are correct:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "common/print_utils.h"
#include "common/bench.h"

static const char *TEST_FILE = "/tmp/lesson_06_test.bin";
static const size_t TEST_FILE_SIZE = 4 * 1024 * 1024;  /* 4 MB */

static int create_test_file(void) {
    FILE *f = fopen(TEST_FILE, "wb");
    if (!f) { perror("fopen"); return -1; }

    uint8_t buf[4096];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = (uint8_t)(i & 0xFF);
    }

    size_t remaining = TEST_FILE_SIZE;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
        fwrite(buf, 1, chunk, f);
        remaining -= chunk;
    }

    fclose(f);
    return 0;
}
```

Let's walk through this:

- `fopen(TEST_FILE, "wb")` -- Open for writing in binary mode. `"wb"` creates
  the file if it does not exist, truncates it if it does. In JS this is
  `fs.writeFileSync(path, data)`.

- The `buf[4096]` array holds a repeating pattern: bytes `0x00` through `0xFF`
  cycling. This gives a predictable checksum we can verify.

- The write loop writes 4 KB chunks until we reach 4 MB. In JavaScript,
  `fs.writeFileSync` writes everything at once. In C, you typically write in
  chunks because (a) you might not have enough stack space for a 4 MB buffer,
  and (b) it is more memory-efficient.

Add the call in `main`:

```c
int main(void) {
    printf("=== Lesson 06 -- Memory-Mapped File I/O ===\n");

    print_section("Creating Test File");
    if (create_test_file() != 0) {
        printf("  Failed to create test file.\n");
        return 1;
    }
    printf("  Created %s (%zu MB)\n", TEST_FILE,
           TEST_FILE_SIZE / (1024 * 1024));
```

### Build and run checkpoint

```bash
./build-dev.sh --lesson=06 -r
```

You should see the test file created at `/tmp/lesson_06_test.bin`.

---

## Step 3 -- Mapping the file into memory

This is the core of the lesson. We open the file, get its size, and map it:

```c
    print_section("mmap -- Map File Into Memory");
    {
        int fd = open(TEST_FILE, O_RDONLY);
        if (fd < 0) { perror("open"); return 1; }

        struct stat st;
        if (fstat(fd, &st) != 0) { perror("fstat"); close(fd); return 1; }
        size_t size = (size_t)st.st_size;

        void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);   /* Safe! The mapping keeps a reference to the file. */

        if (data == MAP_FAILED) { perror("mmap"); return 1; }

        printf("  File mapped at:    %p\n", data);
        printf("  File size:         %zu bytes\n", size);
        printf("  Page-aligned:      %s\n",
               ((uintptr_t)data % 4096 == 0) ? "YES" : "NO");
```

Let's break down every step:

**`open(TEST_FILE, O_RDONLY)`** -- Opens the file for reading. This is a
low-level POSIX call (not `fopen`). It returns a file descriptor (an integer),
not a `FILE *`. We need the raw fd for `mmap`.

**`fstat(fd, &st)`** -- Gets file metadata (size, permissions, timestamps)
into the `struct stat`. We need `st.st_size` to know how many bytes to map.
In JS, this is `fs.statSync(path).size`.

**`mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)`** -- The key call:
- `NULL` -- let the OS choose the virtual address
- `size` -- map the entire file
- `PROT_READ` -- read-only access. Trying to write would `SIGSEGV`.
- `MAP_PRIVATE` -- copy-on-write semantics. If we did write (after changing
  permissions), changes would not affect the file on disk.
- `fd` -- the file descriptor to map
- `0` -- offset from the start of the file (map from the beginning)

**`close(fd)`** -- This surprises people: we close the file descriptor
IMMEDIATELY after `mmap`. The mapping survives. The kernel keeps an internal
reference to the file (through the inode) even after you close the fd. This
is different from JavaScript where closing a file handle ends all access.

> **Why?** Closing the fd after mmap is standard practice and is important
> for resource management. File descriptors are limited (typically 1024 per
> process by default). If you mmap hundreds of files and forget to close the
> fds, you run out. The mapping does not need the fd -- it has its own
> reference to the file.

Now let's access the file through the mapping:

```c
        const uint8_t *bytes = (const uint8_t *)data;
        printf("  First 16 bytes:    ");
        for (int i = 0; i < 16; i++) printf("%02x ", bytes[i]);
        printf("\n");
        printf("  Expected:          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");
```

That is the key insight: **the file IS an array**. `bytes[0]` is the first
byte of the file. `bytes[size-1]` is the last byte. No `fread` calls, no
buffer management, no chunk sizing. Just pointer dereferences.

```
Traditional fread:                    mmap:

FILE *f = fopen(path, "rb");          const uint8_t *bytes = mmap(...);
uint8_t buf[4096];
size_t n;
while ((n = fread(buf, 1, 4096, f))   // Just use bytes[i]
       > 0) {                          // No loop needed
    for (int i = 0; i < n; i++)        // No buffer needed
        process(buf[i]);               // No chunk management
}
fclose(f);
```

Compute a checksum over the entire file:

```c
        long checksum = 0;
        for (size_t i = 0; i < size; i++) checksum += bytes[i];
        printf("  Checksum:          %ld\n", checksum);

        munmap(data, size);
        printf("  Unmapped file.\n");
    }
```

`munmap(data, size)` releases the mapping. Always pair `mmap` with `munmap`
using the exact same pointer and size.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=06 -r
```

You should see the first 16 bytes matching the expected pattern (`00 01 02 ...
0f`), proving the mmap read the file correctly.

---

## Step 4 -- How demand paging works

When you `mmap` a 4 MB file, the kernel does NOT read 4 MB from disk. It sets
up page table entries that POINT to the file on disk, but no data is loaded
yet. The first time you access `bytes[0]`, the CPU triggers a **page fault**:

```
First access to bytes[0]:

1. CPU: "I need to read virtual address 0x7ffff7fc0000"
2. MMU: "Page table says this page maps to file offset 0, but it's not in RAM"
3. MMU: triggers PAGE FAULT (not a crash -- a request)
4. Kernel: "I'll read page 0 of the file from disk into the page cache"
5. Kernel: updates page table to point to the page cache
6. CPU: retries the access -- now it succeeds
7. All subsequent accesses to bytes[0..4095] hit the page cache (fast!)
```

```
After accessing bytes[0] (page 0 faulted in):

Page 0:  [LOADED from disk] -- bytes[0..4095] accessible
Page 1:  [NOT LOADED yet]   -- will fault on first access
Page 2:  [NOT LOADED yet]
...
Page 1023:[NOT LOADED yet]

After accessing bytes[5000] (page 1 faulted in):

Page 0:  [LOADED]
Page 1:  [LOADED from disk] -- bytes[4096..8191] accessible
Page 2:  [NOT LOADED yet]
...
```

This is **demand paging** (or lazy loading). If you mmap a 1 GB file but only
read the first 4 KB, only ONE page (4 KB) is loaded from disk. The OS does
NOT read the entire file upfront.

In JavaScript, `fs.readFileSync` reads the ENTIRE file immediately. There is
no laziness. For a 1 GB file, that is 1 GB of I/O and 1 GB of memory, even
if you only need the first few bytes.

> **Why?** Demand paging is how game engines stream open-world level data. You
> mmap the entire level file (maybe 2 GB), but only the sections the player
> is currently near get loaded from disk. As the player moves, new pages are
> faulted in and old ones can be evicted from the page cache. No explicit
> streaming code needed -- the OS does it for you.

---

## Step 5 -- The fread approach for comparison

To benchmark mmap vs traditional I/O, we need both implementations. Write the
fread version:

```c
static long read_with_fread(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    long checksum = 0;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            checksum += buf[i];
        }
    }

    fclose(f);
    return checksum;
}
```

Every `fread` call:
1. Enters the kernel (system call overhead)
2. Copies data from the page cache into `buf` (memcpy overhead)
3. Returns to user space

For a 4 MB file with 4 KB chunks, that is 1024 system calls and 1024 copies.

Now the mmap version:

```c
static long read_with_mmap(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -1; }
    size_t size = (size_t)st.st_size;

    void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) return -1;

    long checksum = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++) {
        checksum += bytes[i];
    }

    munmap(data, size);
    return checksum;
}
```

The mmap version:
1. One `mmap` system call (setup)
2. Zero copies -- reads directly from the page cache via page table mappings
3. One `munmap` system call (cleanup)

For sequential reads of the entire file, page faults happen once per page
(1024 faults for 4 MB). On subsequent reads, all pages are already in the
page cache, so there are ZERO system calls -- just pointer dereferences.

Verify both produce the same result:

```c
    print_section("Correctness Check");
    {
        long fread_sum = read_with_fread(TEST_FILE);
        long mmap_sum  = read_with_mmap(TEST_FILE);
        printf("  fread checksum: %ld\n", fread_sum);
        printf("  mmap  checksum: %ld\n", mmap_sum);
        printf("  Match: %s\n", (fread_sum == mmap_sum) ? "YES" : "NO");
    }
```

### Build and run checkpoint

```bash
./build-dev.sh --lesson=06 -r
```

Both checksums should match.

---

## Step 6 -- MAP_PRIVATE vs MAP_SHARED

So far we have used `MAP_PRIVATE`. Let's understand the two mapping modes:

**`MAP_PRIVATE` (copy-on-write):**
- Reads: come from the shared page cache (efficient)
- Writes: create private copies of the affected pages (your writes do NOT
  modify the file on disk)
- Multiple processes mapping the same file with `MAP_PRIVATE` share the
  physical page cache pages for reads

**`MAP_SHARED`:**
- Reads: same as `MAP_PRIVATE` -- from the shared page cache
- Writes: go through to the file on disk (eventually, via the page cache)
- Multiple processes with `MAP_SHARED` see each other's writes
- This enables shared-memory IPC (inter-process communication)

```
MAP_PRIVATE:                      MAP_SHARED:

Process A  Process B              Process A  Process B
   |          |                      |          |
   v          v                      v          v
+-------+  +-------+              +-------+
| CoW   |  | CoW   |              | Shared|<---- both processes
| page  |  | page  |              | page  |      same physical page
+-------+  +-------+              +-------+
   |          |                      |
   v          v                      v
Original page in page cache       Same page in page cache
                                     |
                                     v
                                   File on disk
                                  (writes sync here)
```

For read-only file access (the most common game engine use case), the
distinction does not matter because you never write. Use `MAP_PRIVATE` as the
default.

`MAP_SHARED` is powerful for IPC. Two game processes (e.g., a game server and
a metrics dashboard) can share data through a memory-mapped file without any
serialization or socket overhead. Just write to memory in one process and read
in the other.

> **Why?** In JavaScript, the closest equivalent to `MAP_SHARED` is
> `SharedArrayBuffer` with `Atomics` for web workers. The concept is the same:
> multiple execution contexts sharing the same physical memory. In C with
> `MAP_SHARED`, you get this at the OS level, between separate processes, with
> files as the backing store. Game engine tools (editors, profilers, live
> reload systems) often use this for zero-copy data sharing.

---

## Step 7 -- Benchmarking mmap vs fread

The benchmark reads the entire 4 MB file 20 times with each method:

```c
#ifdef BENCH_MODE
    {
        BenchSuite suite;
        long N = 20;

        BENCH_SUITE(suite, "fread vs mmap (4 MB file, N full reads)") {
            volatile long cs = 0;
            BENCH_CASE(suite, "fread", i, N) {
                cs += read_with_fread(TEST_FILE);
            }
            BENCH_CASE_END(suite, N);
            (void)cs;

            cs = 0;
            BENCH_CASE(suite, "mmap", i, N) {
                cs += read_with_mmap(TEST_FILE);
            }
            BENCH_CASE_END(suite, N);
            (void)cs;
        }
    }
#else
    bench_skip_notice("lesson_06");
#endif
```

What to expect:

- **First run** (cold cache): mmap might be SLOWER than fread. Each page
  fault has per-page overhead, and fread uses sequential readahead (the kernel
  pre-loads the next pages because it sees a sequential access pattern). With
  mmap, the kernel does not know you will access the pages sequentially.

- **Subsequent runs** (warm cache): mmap should be FASTER because there are
  zero copies. fread still copies from the page cache to your buffer on every
  call. mmap reads the page cache directly.

- **Random access patterns**: mmap wins decisively. With fread, random access
  means seeking and re-reading. With mmap, it is just pointer arithmetic.

```
Performance comparison (rough, varies by system):

                  Cold cache    Warm cache    Random access
fread             ~10 ms        ~8 ms         ~50 ms (seek overhead)
mmap              ~12 ms        ~5 ms         ~5 ms (just pointer deref)
```

Clean up the test file:

```c
    unlink(TEST_FILE);
    printf("  Cleaned up test file.\n\n");
    return 0;
}
```

`unlink(path)` deletes the file. This is the POSIX equivalent of
`fs.unlinkSync(path)` in Node.js.

### Build and run checkpoint

Normal run:
```bash
./build-dev.sh --lesson=06 -r
```

With benchmarks:
```bash
./build-dev.sh --lesson=06 --bench -r
```

---

## Step 8 -- The trade-offs summary

Add this section to your `main`:

```c
    print_section("mmap vs fread -- Trade-offs");
    printf("  mmap advantages:\n");
    printf("    - No user-space buffer needed (OS pages ARE the buffer)\n");
    printf("    - Lazy loading: only touched pages are read from disk\n");
    printf("    - Multiple processes can share the same physical pages\n");
    printf("    - Random access without seeking (just pointer arithmetic)\n");
    printf("    - Less copying: zero-copy for read-only access\n\n");
    printf("  mmap disadvantages:\n");
    printf("    - Page-granularity overhead for small files\n");
    printf("    - TLB pressure for very large mappings\n");
    printf("    - Error handling is via SIGBUS (complex) vs errno\n");
    printf("    - Not portable to all embedded systems\n\n");
    printf("  fread advantages:\n");
    printf("    - Simple, portable, familiar API\n");
    printf("    - Fine-grained control over buffering\n");
    printf("    - Works with non-seekable streams (pipes, sockets)\n");
    printf("    - Error handling via return value\n");
```

The decision matrix:

```
Scenario                                  Best choice
----------------------------------------  -----------
Large file, read multiple times           mmap
Large file, random access                 mmap
Large file, read once sequentially        fread (readahead wins)
Small file (< 1 page)                    fread (mmap overhead not worth it)
Streaming from pipe/socket                fread (mmap needs a seekable file)
Multiple processes sharing file data      mmap with MAP_PRIVATE
Memory-mapped database                    mmap with MAP_SHARED
Portable code for embedded systems        fread
Game asset loading (textures, meshes)     mmap (lazy loading + zero-copy)
```

> **Why?** There is no single "best" file I/O method. `mmap` shines for large
> files with repeated or random access -- exactly the pattern game engines have
> with asset files. `fread` is better for one-shot sequential reads and
> non-seekable streams. Understanding the trade-offs lets you choose the right
> tool for each situation.

---

## Build and run

Normal run:

```bash
./build-dev.sh --lesson=06 -r
```

With benchmarks:

```bash
./build-dev.sh --lesson=06 --bench -r
```

---

## Expected output

```
=== Creating Test File ===
  Created /tmp/lesson_06_test.bin (4 MB)

=== mmap -- Map File Into Memory ===
  File mapped at:    0x7ffff7bb9000
  File size:         4194304 bytes
  Page-aligned:      YES
  First 16 bytes:    00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
  Expected:          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
  Checksum:          532676608
  Unmapped file.

=== Correctness Check ===
  fread checksum: 532676608
  mmap  checksum: 532676608
  Match: YES
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `mmap` returns `MAP_FAILED` | File does not exist, or `open` used wrong flags | Check `errno` after `mmap`; ensure the file was created first |
| `SIGBUS` when accessing mapped memory | File was truncated after mapping, or accessing past the end of file | Ensure file size is stable; only access within the mapped `size` |
| Slower mmap than fread on first run | mmap's per-page fault overhead; fread does sequential readahead | For one-shot sequential reads of small files, fread can win. mmap shines on repeated/random access |
| Memory not freed after `munmap` | Forgot to call `munmap`, or used wrong size | Always pair `mmap` with `munmap(ptr, size)` using the exact same size |
| Data corruption when writing to MAP_PRIVATE | You expected writes to reach the file | MAP_PRIVATE creates private copies. Use MAP_SHARED if you want writes to reach the file |
| File changes not visible after mapping | mmap'd with MAP_PRIVATE, so changes to the file are not reflected | Use MAP_SHARED, or munmap and re-mmap after the file changes |

---

## Exercises

1. **Map only part of a file.** Use the offset parameter of `mmap` to map
   just the second megabyte of the test file (offset = 1 MB, size = 1 MB).
   Verify the first byte matches what you expect based on the repeating
   pattern.

2. **Write through MAP_SHARED.** Create a file, mmap it with
   `MAP_SHARED | PROT_READ | PROT_WRITE`, write some data through the
   mapping, `munmap`, then `fread` the file and verify the writes landed on
   disk. This is how memory-mapped databases (LMDB, SQLite in WAL mode)
   work.

3. **mmap for string searching.** mmap a text file and use `memchr` or a
   custom loop to search for a string. Compare the performance with opening
   the file with `fread` and searching the buffer. mmap should be competitive
   or faster for warm-cache scenarios.

4. **Lazy loading proof.** mmap a large file (100+ MB if you can create one).
   Measure RSS (Resident Set Size) with `getrusage(RUSAGE_SELF, &usage)`
   before and after accessing the first page, then after accessing all pages.
   RSS should grow incrementally, proving pages are loaded on demand.

5. **Build a read-only asset loader.** Write a function
   `asset_open(path) -> Asset` that mmaps a file and returns a struct with
   the data pointer and size. Write `asset_close(Asset)` that munmaps.
   Use it to "load" multiple files and access them via pointers. This is the
   beginning of a game engine asset system.

---

## Connection to your work

In game engines:

- **Asset loading with mmap.** Texture files, mesh data, audio clips -- all
  can be memory-mapped. The OS handles caching and eviction. You just access
  the data through a pointer.

- **Streaming open worlds.** mmap the entire world data file. As the player
  moves, new regions cause page faults that load data from disk. Old regions
  are evicted from the page cache naturally. No explicit streaming code
  needed for the simple case.

- **Hot-reload with MAP_SHARED.** Map a shared library or data file with
  MAP_SHARED. When the file changes on disk (recompiled shader, updated
  texture), the changes become visible through the mapping. Some engines
  use this for live code reloading during development.

- **Save files.** mmap the save file with MAP_SHARED, write game state
  directly to it. The OS flushes to disk periodically. Call `msync` to force
  a flush at checkpoints. No serialization library needed for flat data
  structures.

- **This completes the foundation.** You now understand all five memory
  segments (Lesson 01), the stack (Lesson 02), the heap (Lesson 03),
  alignment (Lesson 04), virtual memory (Lesson 05), and file-mapped memory
  (this lesson). Starting from Lesson 07, we build on this foundation with
  ownership patterns, leak detection, and custom allocators.

---

## What's next

In Lesson 07, we tackle **pointer ownership and lifetime** -- the rules for
who is responsible for freeing allocated memory, how to prevent use-after-free
and double-free, and the patterns that make manual memory management
manageable. This is where we go from understanding the mechanisms to
mastering the discipline.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_06.c`. It contains the
test file creation, mmap demonstration, fread comparison, correctness
verification, and benchmark.
