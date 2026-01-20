#ifndef ENGINE_COMMON_DEBUG_FILE_IO_H
#define ENGINE_COMMON_DEBUG_FILE_IO_H
#include "base.h"
#include "memory.h"

#if HANDMADE_INTERNAL
typedef struct {
  uint32_t size;
  PlatformMemoryBlock contents;
} DebugReadFileResult;
DebugReadFileResult debug_platform_read_entire_file(char *filename);
void debug_platform_free_file_memory(PlatformMemoryBlock *memory_block);
bool32 debug_platform_write_entire_file(char *filename, uint32_t memory_size,
                                        void *memory);

#endif // HANDMADE_INTERNAL
       
#endif // ENGINE_COMMON_DEBUG_FILE_IO_H