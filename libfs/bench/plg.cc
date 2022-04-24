
#include "pl_rand.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define IO_SIZE 128
#define WR_SIZE 4096
#define WS_SIZE 1000000000

int main(int argc, char** argv)
{
  uint32_t n = atoi(argv[1]);
  for (int i = 0; i<n; ++i) {
    uint32_t blk_offs = 4096 * pl_rand();
    printf("%u\n", blk_offs);
  }

  return 0;
}
