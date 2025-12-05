#include "./platform.h"

#ifndef BASE_H
#define BASE_H

#include <stdint.h>
#include <stdbool.h>

#define file_scoped_fn static
#define local_persist_var static
#define file_scoped_global_var static

typedef int32_t bool32;

#endif // BASE_H
