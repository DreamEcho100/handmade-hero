#include "../base/base.h"
#include <stdint.h>

#if defined(_WIN32)
/* ========================= WINDOWS IMPLEMENTATION ========================= */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

static size_t platform_get_page_size(void) {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (size_t)info.dwPageSize;
}

static DWORD platform_win32_protection_from_flags(PlatformMemoryFlags flags) {
  int r = (flags & PLATFORM_MEMORY_READ) != 0;
  int w = (flags & PLATFORM_MEMORY_WRITE) != 0;
  int x = (flags & PLATFORM_MEMORY_EXECUTE) != 0;

  if (x) {
    if (w)
      return PAGE_EXECUTE_READWRITE;
    if (r)
      return PAGE_EXECUTE_READ;
    return PAGE_EXECUTE;
  } else {
    if (w)
      return PAGE_READWRITE;
    if (r)
      return PAGE_READONLY;
  }

  return PAGE_NOACCESS;
}

PlatformMemoryBlock platform_allocate_memory(void *base_hint, size_t size,
                                             PlatformMemoryFlags flags) {
  size_t page_size = platform_get_page_size();
  size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
  size_t total_size = aligned_size + 2 * page_size;

  DWORD protect = platform_win32_protection_from_flags(flags);

  void *request_base = NULL;
  if (flags & (PLATFORM_MEMORY_BASE_FIXED | PLATFORM_MEMORY_BASE_HINT)) {
    request_base = base_hint;
  }

  void *reserved =
      VirtualAlloc(request_base, total_size, MEM_RESERVE, PAGE_NOACCESS);

  if (!reserved && (flags & PLATFORM_MEMORY_BASE_HINT)) {
    reserved = VirtualAlloc(NULL, total_size, MEM_RESERVE, PAGE_NOACCESS);
  }

  if (!reserved) {
    return (PlatformMemoryBlock){0};
  }

  void *committed = VirtualAlloc((uint8_t *)reserved + page_size, aligned_size,
                                 MEM_COMMIT, protect);

  if (!committed) {
    VirtualFree(reserved, 0, MEM_RELEASE);
    return (PlatformMemoryBlock){0};
  }

  if (flags & PLATFORM_MEMORY_ZEROED) {
    ZeroMemory(committed, aligned_size);
  }

  return (PlatformMemoryBlock){.base = committed,
                               .size = aligned_size,
                               .total_size = total_size,
                               .flags = flags};
}

/*
void platform_free_memory(void *base, size_t total_size) {
  if (base) {
    // uint8_t *reserved_base = (uint8_t *)base - (total_size - total_size) / 2;
    size_t page_size = platform_get_page_size();
    uint8_t *reserved_base = (uint8_t *)base - page_size; // ← FIX!

    VirtualFree(reserved_base, 0, MEM_RELEASE);
  }
}
*/
void platform_free_memory(PlatformMemoryBlock *block) {
  if (block && block->base) {
    uint8_t *reserved_base =
        (uint8_t *)block->base - (block->total_size - block->size) / 2;

    VirtualFree(reserved_base, 0, MEM_RELEASE);
    block->base = NULL;
  }
}

#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__) ||         \
    defined(__MACH__)
/* ========================= POSIX IMPLEMENTATION ========================= */

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t platform_get_page_size(void) {
  return (size_t)sysconf(_SC_PAGESIZE);
}

static int platform_posix_protection_from_flags(PlatformMemoryFlags flags) {
  int prot = PROT_NONE;

  if (flags & PLATFORM_MEMORY_READ)
    prot |= PROT_READ;
  if (flags & PLATFORM_MEMORY_WRITE)
    prot |= PROT_WRITE;
  if (flags & PLATFORM_MEMORY_EXECUTE)
    prot |= PROT_EXEC;

  return prot;
}

PlatformMemoryBlock platform_allocate_memory(void *base_hint, size_t size,
                                             PlatformMemoryFlags flags) {
  size_t page_size = platform_get_page_size();
  size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
  size_t total_size = aligned_size + 2 * page_size;

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (flags & PLATFORM_MEMORY_BASE_FIXED) {
    mmap_flags |= MAP_FIXED;
  }

  void *reserved = mmap(base_hint, total_size, PROT_NONE, mmap_flags, -1, 0);

  if (reserved == MAP_FAILED) {
    return (PlatformMemoryBlock){0};
  }

  int prot = platform_posix_protection_from_flags(flags);
  if (mprotect((uint8_t *)reserved + page_size, aligned_size, prot) != 0) {
    munmap(reserved, total_size);
    return (PlatformMemoryBlock){0};
  }

  // ═══════════════════════════════════════════════════════════════
  // 🔍 OPTIMIZATION NOTE:
  // ═══════════════════════════════════════════════════════════════
  // On Linux, mmap() with MAP_ANONYMOUS already returns zero-initialized
  // pages (guaranteed by POSIX). The OS uses copy-on-write zero pages,
  // so we don't need to manually memset() here.
  //
  // Casey doesn't need this on Windows either because VirtualAlloc()
  // with MEM_COMMIT also returns zero-initialized memory.
  //
  // However, we keep the flag for API compatibility and future platforms
  // that might not guarantee zeroed memory.
  // ═══════════════════════════════════════════════════════════════
  // if (flags & PLATFORM_MEMORY_ZEROED) {
  //   memset((uint8_t *)reserved + page_size, 0, aligned_size);
  // }
  // ═══════════════════════════════════════════════════════════════

  return (PlatformMemoryBlock){.base = (uint8_t *)reserved + page_size,
                               .size = aligned_size,
                               .total_size = total_size,
                               .flags = flags};
}

/*
void platform_free_memory(void *base, size_t total_size) {
  if (base) {
    // uint8_t *reserved_base = (uint8_t *)base - (total_size - total_size) / 2;
    size_t page_size = platform_get_page_size();
    uint8_t *reserved_base = (uint8_t *)base - page_size; // ← FIX!

    munmap(reserved_base, total_size);
  }
}
*/
void platform_free_memory(PlatformMemoryBlock *block) {
  if (block && block->base) {
    uint8_t *reserved_base =
        (uint8_t *)block->base - (block->total_size - block->size) / 2;

    munmap(reserved_base, block->total_size);
    block->base = NULL;
  }
}

#else
#error Unsupported platform
#endif
