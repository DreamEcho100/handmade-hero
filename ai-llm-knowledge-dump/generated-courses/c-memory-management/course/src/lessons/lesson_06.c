/* lesson_06.c — Memory-Mapped File I/O
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates mmap for file I/O: map a file into memory and read it
 * by pointer access instead of fread.  Benchmarks mmap vs fread.
 *
 * Run:   ./build-dev.sh --lesson=06 -r
 * Bench: ./build-dev.sh --lesson=06 --bench -r
 */

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

/* ── Create a test file for demonstrations ────────────────────────────────*/

static const char *TEST_FILE = "/tmp/lesson_06_test.bin";
static const size_t TEST_FILE_SIZE = 4 * 1024 * 1024;  /* 4 MB */

static int create_test_file(void) {
  FILE *f = fopen(TEST_FILE, "wb");
  if (!f) { perror("fopen"); return -1; }

  /* Write a pattern: repeating 0-255 bytes */
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

/* ── Read file with fread (traditional approach) ──────────────────────────*/

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

/* ── Read file with mmap ──────────────────────────────────────────────────*/

static long read_with_mmap(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;

  struct stat st;
  if (fstat(fd, &st) != 0) { close(fd); return -1; }
  size_t size = (size_t)st.st_size;

  void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);  /* fd can be closed after mmap */
  if (data == MAP_FAILED) return -1;

  /* Read through the mapping */
  long checksum = 0;
  const uint8_t *bytes = (const uint8_t *)data;
  for (size_t i = 0; i < size; i++) {
    checksum += bytes[i];
  }

  munmap(data, size);
  return checksum;
}

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 06 — Memory-Mapped File I/O\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Create test file ────────────────────────────────────────────*/
  print_section("Creating Test File");
  if (create_test_file() != 0) {
    printf("  Failed to create test file.\n");
    return 1;
  }
  printf("  Created %s (%zu MB)\n", TEST_FILE,
         TEST_FILE_SIZE / (1024 * 1024));

  /* ── 2. Demonstrate mmap for file reading ───────────────────────────
   *
   * mmap maps a file directly into your process's address space.
   * Instead of read() copying data from kernel to user buffer,
   * mmap gives you a pointer that IS the file contents.
   *
   * The OS loads pages on demand — only pages you touch are read from disk.
   */
  print_section("mmap — Map File Into Memory");
  {
    int fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct stat st;
    if (fstat(fd, &st) != 0) { perror("fstat"); close(fd); return 1; }
    size_t size = (size_t)st.st_size;

    /* Map the entire file read-only */
    void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) { perror("mmap"); return 1; }

    printf("  File mapped at:    %p\n", data);
    printf("  File size:         %zu bytes\n", size);
    printf("  Page-aligned:      %s\n",
           ((uintptr_t)data % 4096 == 0) ? "YES" : "NO");

    /* Access it like a regular array */
    const uint8_t *bytes = (const uint8_t *)data;
    printf("  First 16 bytes:    ");
    for (int i = 0; i < 16; i++) printf("%02x ", bytes[i]);
    printf("\n");
    printf("  Expected:          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");

    /* Compute checksum over entire file via pointer access */
    long checksum = 0;
    for (size_t i = 0; i < size; i++) checksum += bytes[i];
    printf("  Checksum:          %ld\n", checksum);

    munmap(data, size);
    printf("  Unmapped file.\n");
  }

  /* ── 3. mmap advantages ─────────────────────────────────────────────*/
  print_section("mmap vs fread — Trade-offs");
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

  /* ── 4. Verify both methods produce same result ─────────────────────*/
  print_section("Correctness Check");
  {
    long fread_sum = read_with_fread(TEST_FILE);
    long mmap_sum  = read_with_mmap(TEST_FILE);
    printf("  fread checksum: %ld\n", fread_sum);
    printf("  mmap  checksum: %ld\n", mmap_sum);
    printf("  Match: %s\n", (fread_sum == mmap_sum) ? "YES" : "NO");
  }

  /* ── 5. Benchmark ───────────────────────────────────────────────────*/
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

  /* Cleanup */
  unlink(TEST_FILE);
  printf("  Cleaned up test file.\n\n");

  return 0;
}
