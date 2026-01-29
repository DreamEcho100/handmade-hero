#ifndef DE100_COMMON_DEBUG_FILE_IO_H
#define DE100_COMMON_DEBUG_FILE_IO_H
#include "base.h"
#include "memory.h"

#if DE100_INTERNAL
typedef struct {
  uint32 size;
  PlatformMemoryBlock contents;
} DebugReadFileResult;
DebugReadFileResult debug_platform_read_entire_file(char *filename);
void debug_platform_free_file_memory(PlatformMemoryBlock *memory_block);
bool32 debug_platform_write_entire_file(char *filename, uint32 memory_size,
                                        void *memory);

#endif // DE100_INTERNAL
       
#endif // DE100_COMMON_DEBUG_FILE_IO_H