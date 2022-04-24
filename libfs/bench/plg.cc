
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define IO_SIZE 128
#define WR_SIZE 4096
#define WS_SIZE 1000000000

/*
 * Like search.c, this approximates the power-law distribution, but more conveniently- by giving consistent 
 * replays of working sets, not randomly varying ones. Still realistic, since working sets probably only
 * drift in the long-run.
 *
 * https://mathworld.wolfram.com/RandomNumber.html*
 */

static uint32_t X1pN = 0;
static double np = 0;

uint32_t pl_rand()
{
  double y = ((double) rand())/RAND_MAX;
  return (uint32_t) pow(X1pN * y, 1.0/(np+1));
}

int main(int argc, char** argv)
{
  uint32_t n = atoi(argv[1]);
  np = atof(argv[2]);
  X1pN = pow(WS_SIZE/4096, np);

  for (int i = 0; i<n; ++i) {
    uint32_t blk_offs = 4096 * pl_rand();
    printf("%u\n", blk_offs);
  }

  return 0;
}
