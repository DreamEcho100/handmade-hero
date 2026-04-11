#define _POSIX_C_SOURCE 200809L
// lesson_05.c — Virtual Memory and Pages
// BUILD_DEPS: common/bench.c
//
// Demonstrates mmap/munmap, reserve vs commit, PROT_NONE guard pages,
// and mprotect.  Linux/macOS only (POSIX).
//
// Run:  ./build-dev.sh --lesson=05 -r
// Guard page crash: ./build-dev.sh --lesson=05 --no-asan -r  (pass "crash" arg)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/mman.h>
#include "common/print_utils.h"
#include "common/bench.h"

static jmp_buf jump_buf;
static volatile int got_signal = 0;

static void sigsegv_handler(int sig) {
  (void)sig;
  got_signal = 1;
  longjmp(jump_buf, 1);
}

int main(int argc, char *argv[]) {
  int do_crash = (argc > 1 && strcmp(argv[1], "crash") == 0);
  (void)do_crash;

  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 05 — Virtual Memory and Pages\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  long page_size = sysconf(_SC_PAGESIZE);
  printf("\n  System page size: %ld bytes (%ld KB)\n",
         page_size, page_size / 1024);

  /* ── 1. Basic mmap: anonymous memory ────────────────────────────────
   *
   * mmap with MAP_ANONYMOUS allocates virtual memory directly from the OS.
   * Unlike malloc, this memory is page-aligned and in page-sized chunks.
   * The OS zeroes it automatically.
   */
  print_section("mmap — Anonymous Memory Allocation");
  {
    size_t size = 4 * (size_t)page_size;  /* 4 pages */
    void *mem = mmap(NULL, size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);

    if (mem == MAP_FAILED) {
      perror("mmap");
      return 1;
    }

    printf("  mmap(%zu bytes) = %p\n", size, mem);
    printf("  Page-aligned: %p mod %ld = %ld\n",
           mem, page_size, (long)((uintptr_t)mem % (uintptr_t)page_size));

    /* The memory is zero-initialized */
    int all_zero = 1;
    for (size_t i = 0; i < size; i++) {
      if (((char *)mem)[i] != 0) { all_zero = 0; break; }
    }
    printf("  Zero-initialized: %s\n", all_zero ? "YES" : "NO");

    /* Write to it */
    memset(mem, 0xAB, size);
    printf("  Written 0xAB to all %zu bytes.\n", size);

    /* Release */
    munmap(mem, size);
    printf("  munmap() — memory returned to OS.\n");
  }

  /* ── 2. Reserve vs Commit ───────────────────────────────────────────
   *
   * Key insight from the Ryan Fleury transcript:
   *   "I'm going to reserve 256 GB of address space, but that's not a
   *    physical commit of memory... when I'm about to end that region,
   *    I call back to the OS and say 'commit more pages.'"
   *
   * On Linux:
   *   Reserve = mmap with PROT_NONE (no read, no write, no execute)
   *   Commit  = mprotect to PROT_READ|PROT_WRITE
   *
   * This is how production arena allocators work (Lesson 13).
   */
  print_section("Reserve vs Commit (Virtual Memory Trick)");
  {
    /* Reserve a large address range (64 MB) — no physical memory used */
    size_t reserve_size = 64 * 1024 * 1024;  /* 64 MB */
    void *reserved = mmap(NULL, reserve_size,
                          PROT_NONE,  /* NO access — just reserves addresses */
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);
    if (reserved == MAP_FAILED) {
      perror("mmap reserve");
      return 1;
    }
    printf("  Reserved %zu MB at %p (no physical memory yet!)\n",
           reserve_size / (1024 * 1024), reserved);

    /* Commit the first page — now it has physical backing */
    size_t commit_size = (size_t)page_size;
    if (mprotect(reserved, commit_size, PROT_READ | PROT_WRITE) != 0) {
      perror("mprotect commit");
      munmap(reserved, reserve_size);
      return 1;
    }
    printf("  Committed first page (%ld bytes) — now writable.\n", page_size);

    /* Write to the committed page */
    ((int *)reserved)[0] = 42;
    printf("  Wrote 42 to first int: %d\n", ((int *)reserved)[0]);

    /* Commit more pages as needed */
    size_t more = 4 * (size_t)page_size;
    if (mprotect(reserved, commit_size + more, PROT_READ | PROT_WRITE) != 0) {
      perror("mprotect grow");
    }
    printf("  Committed %zu more pages — total committed: %zu bytes.\n",
           more / (size_t)page_size,
           commit_size + more);

    /* Write to the newly committed region */
    ((int *)reserved)[page_size / sizeof(int)] = 99;
    printf("  Wrote 99 to second page: %d\n",
           ((int *)reserved)[page_size / sizeof(int)]);

    printf("\n  Summary:\n");
    printf("    Address space reserved: %zu MB\n",
           reserve_size / (1024 * 1024));
    printf("    Physical memory used:   %zu KB\n",
           (commit_size + more) / 1024);
    printf("    Efficiency: reserve once, commit incrementally.\n");

    munmap(reserved, reserve_size);
  }

  /* ── 3. Guard pages ─────────────────────────────────────────────────
   *
   * A guard page is a page marked PROT_NONE placed adjacent to usable
   * memory.  Any access to the guard page triggers an immediate SIGSEGV.
   *
   * This catches buffer overflows at the exact instruction that overflows,
   * rather than silently corrupting adjacent memory.
   */
  print_section("Guard Pages — Instant Overflow Detection");
  {
    /* Layout: [GUARD] [DATA] [GUARD] */
    size_t data_pages = 4;
    size_t data_size = data_pages * (size_t)page_size;
    size_t total_size = data_size + 2 * (size_t)page_size;  /* + 2 guard pages */

    void *region = mmap(NULL, total_size,
                        PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (region == MAP_FAILED) { perror("mmap"); return 1; }

    /* Make the middle region writable */
    void *data = (char *)region + page_size;
    if (mprotect(data, data_size, PROT_READ | PROT_WRITE) != 0) {
      perror("mprotect");
      munmap(region, total_size);
      return 1;
    }

    printf("  Memory layout:\n");
    printf("    Guard page (PROT_NONE): %p\n", region);
    printf("    Data (%zu pages):        %p — %p\n",
           data_pages, data, (char *)data + data_size - 1);
    printf("    Guard page (PROT_NONE): %p\n", (char *)data + data_size);

    /* Write safely within bounds */
    memset(data, 0x42, data_size);
    printf("\n  Wrote 0x42 to all %zu data bytes — OK.\n", data_size);

    /* Try to touch the guard page (will SIGSEGV) */
    printf("\n  Attempting to write 1 byte past the data region...\n");

    struct sigaction sa, old_sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigsegv_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old_sa);
    sigaction(SIGBUS, &sa, NULL);

    got_signal = 0;
    if (setjmp(jump_buf) == 0) {
      /* This write hits the guard page → SIGSEGV */
      volatile char *guard = (char *)data + data_size;
      *guard = 0xFF;
      printf("  ERROR: Should not reach here!\n");
    } else {
      printf("  CAUGHT SIGSEGV! Guard page detected the overflow.\n");
      printf("  Without guard pages, this would silently corrupt memory.\n");
    }

    /* Restore signal handler */
    sigaction(SIGSEGV, &old_sa, NULL);
    munmap(region, total_size);
  }

  /* ── 4. mmap vs malloc performance ──────────────────────────────────*/
  print_section("mmap vs malloc — When to Use Each");
  printf("  malloc:\n");
  printf("    - General purpose, any size\n");
  printf("    - Fast for small allocations (uses free lists internally)\n");
  printf("    - No page alignment guarantee\n");
  printf("    - Cannot control read/write/exec permissions\n\n");
  printf("  mmap:\n");
  printf("    - Page-granularity only (4 KB minimum)\n");
  printf("    - Good for large allocations (> 128 KB)\n");
  printf("    - Page-aligned, zero-initialized\n");
  printf("    - Can set permissions (PROT_NONE, guard pages)\n");
  printf("    - Can map files into memory (Lesson 06)\n");
  printf("    - Used internally by malloc for large allocations!\n\n");
  printf("  Arena allocators (Lesson 13) use mmap for backing memory,\n");
  printf("  then hand out small pieces via pointer bumping.\n");

  printf("\n");
  return 0;
}
