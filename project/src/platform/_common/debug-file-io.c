#if HANDMADE_INTERNAL
#include "debug-file-io.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h> // fprintf
#include <string.h>

uint32_t safe_truncate_uint64(long value) {
  Assert(value >= 0); // Negative = error!

  // TODO: Defines for maximum values UInt32Max
  Assert(value <= 0xFFFFFFFF); // Too large for uint32

  uint32_t result = (uint32_t)value;
  return (result);
}

DebugReadFileResult debug_platform_read_entire_file(char *filename) {
  DebugReadFileResult result = {};

  FILE *file = fopen(filename, "rb");
  if (file) {
    if (fseek(file, 0, SEEK_END) == 0) {
      long file_size = ftell(file);
      if (file_size > 0) {
        rewind(file);

        result.contents = platform_allocate_memory(
            NULL, file_size, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

        if (result.contents.base) {
          size_t bytes_read = fread(result.contents.base, 1, file_size, file);
          if (bytes_read == (size_t)file_size) {
            result.size = safe_truncate_uint64(file_size);
          } else {
            debug_platform_free_file_memory(&result.contents);
            result.contents.base = NULL;
          }
        } else {
          fprintf(
              stderr,
              "debug_platform_read_entire_file: Memory allocation failed\n");
        }
      }
    }
    fclose(file);
  } else {
    fprintf(stderr,
            "debug_platform_read_entire_file: Could not open file %s: %s\n",
            filename, strerror(errno));
  }

  return (result);
}

void debug_platform_free_file_memory(PlatformMemoryBlock *memory_block) {
  platform_free_memory(memory_block);
}

bool32 debug_platform_write_entire_file(char *filename, uint32_t memory_size,
                                        void *memory) {
  bool32 result = false;

  FILE *file = fopen(filename, "wb");
  if (file) {
    size_t bytes_written = fwrite(memory, 1, memory_size, file);
    if (bytes_written == memory_size) {
      result = true;
    } else {
      fprintf(
          stderr,
          "debug_platform_write_entire_file: Write failed for file %s: %s\n",
          filename, strerror(errno));
    }
    fclose(file);
  } else {
    fprintf(stderr,
            "debug_platform_write_entire_file: Could not open file %s for "
            "writing: %s\n",
            filename, strerror(errno));
  }

  return (result);
}
#endif