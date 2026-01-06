
#include "base.h"
#include <stdint.h>
#include <stdio.h> // fopen, fclose, fseek, ftell, fread
// #include <stdlib.h> // malloc, free

uint32_t safe_truncate_uint64(uint64_t value) {
  // TODO: Defines for maximum values UInt32Max
  Assert(value <= 0xFFFFFFFF);
  uint32_t result = (uint32_t)value;
  return (result);
}

DebugReadFileResult debug_platform_read_entire_file(char *filename) {
  DebugReadFileResult result = {};

  // Open file for reading (binary mode)
  FILE *file = fopen(filename, "rb");

  if (file) {
    // Get file size
    fseek(file, 0, SEEK_END);    // Seek to end
    long FileSize = ftell(file); // Get position (= file size)
    fseek(file, 0, SEEK_SET);    // Seek back to start

    if (FileSize > 0) {
      // Allocate memory
      // Result.Contents = malloc(FileSize);
      result.contents = platform_allocate_memory(
          NULL, FileSize, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

      if (result.contents.base) {
        // Read entire file
        size_t bytes_read = fread(result.contents.base, 1, FileSize, file);

        if (bytes_read == FileSize) {
          // SUCCESS!
          result.size = safe_truncate_uint64(FileSize);
        } else {
          // Read failed, clean up
          debug_platform_free_file_memory(&result.contents);
          result.contents.base = NULL;
        }
      } else {
        // TODO: Logging
        fprintf(stderr,
                "debug_platform_read_entire_file: Memory allocation failed\n");
      }
    }

    fclose(file);
  } else {
    // TODO: Logging - file not found
    fprintf(stderr, "debug_platform_read_entire_file: Could not open file %s\n",
            filename);
  }

  return (result);
}

void debug_platform_free_file_memory(PlatformMemoryBlock *memory_block) {
  platform_free_memory(memory_block);
}

bool32 debug_platform_write_entire_file(char *filename, uint32_t memory_size,
                                        void *memory) {
  bool32 result = false;

  // Open file for writing (binary mode, create/truncate)
  FILE *file = fopen(filename, "wb");

  if (file) {
    // Write entire memory block
    size_t bytes_written = fwrite(memory, 1, memory_size, file);
    if (bytes_written == memory_size) {
      // SUCCESS!
      result = true;
    } else {
      // TODO: Logging - write failed
      fprintf(stderr,
              "debug_platform_write_entire_file: Write failed for file %s\n",
              filename);
    }

    fclose(file);
  } else {
    // TODO: Logging - couldn't create file
    fprintf(stderr,
            "debug_platform_write_entire_file: Could not open file %s for "
            "writing\n",
            filename);
  }

  return (result);
}
