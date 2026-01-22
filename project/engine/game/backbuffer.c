#include "backbuffer.h"
#include <stdio.h>

INIT_BACKBUFFER_STATUS init_backbuffer(GameOffscreenBuffer *buffer, int width,
                                       int height, int bytes_per_pixel) {
  buffer->memory.base = NULL;
  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = bytes_per_pixel;
  buffer->pitch = buffer->width * buffer->bytes_per_pixel;
  int initial_initial_buffer_size = buffer->pitch * buffer->height;
  PlatformMemoryBlock memory_block = platform_allocate_memory(
      NULL, initial_initial_buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(memory_block)) {
    fprintf(stderr,
            "platform_allocate_memory failed: could not allocate %d "
            "bytes\n",
            initial_initial_buffer_size);
    fprintf(stderr, "   Error: %s\n", memory_block.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(memory_block.error_code));
    return INIT_BACKBUFFER_STATUS_MMAP_FAILED;
  }

  buffer->memory = memory_block;

  return INIT_BACKBUFFER_STATUS_SUCCESS;
}