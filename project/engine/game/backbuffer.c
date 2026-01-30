#include "backbuffer.h"
#include <stdio.h>

INIT_BACKBUFFER_STATUS init_backbuffer(GameBackBuffer *buffer, int width,
                                       int height, int bytes_per_pixel) {
  buffer->memory.base = NULL;
  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = bytes_per_pixel;
  buffer->pitch = width * bytes_per_pixel;
  int initial_initial_buffer_size = buffer->pitch * buffer->height;
  MemoryBlock memory_block =
      memory_alloc(NULL, initial_initial_buffer_size,
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(memory_block)) {
    fprintf(stderr,
            "memory_alloc failed: could not allocate %d "
            "bytes\n",
            initial_initial_buffer_size);
    fprintf(stderr, "   Code: %s\n", memory_error_str(memory_block.error_code));
    return INIT_BACKBUFFER_STATUS_MMAP_FAILED;
  }

  buffer->memory = memory_block;

  return INIT_BACKBUFFER_STATUS_SUCCESS;
}