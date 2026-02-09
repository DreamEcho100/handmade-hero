#include "./platforms/_common/backend.h"
#include "_common/path.h"

#if DE100_INTERNAL
#include "./_common/base.h"
#include "_common/time.h"
#include <stdio.h>
#endif

int main(int argc, char **argv) {
  path_on_init(argc, argv);

  (void)argc;
  (void)argv;

#if DE100_INTERNAL
  real64 main_start = get_wall_clock();
  printf("[MAIN ENTRY] %.6f seconds since boot\n", main_start);
#endif

  return platform_main();
  //
}
