
#include "pl_rand.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#define RDD_SIZE 1000000000
#define IO_SIZE 128
#define WR_SIZE 4096
#define WS_SIZE 1000000000

double secs_since(clock_t base)
{
  return ((double) (clock()-base))/CLOCKS_PER_SEC;
}

int main(int argc, char** argv)
{
  clock_t init_time = clock();

  time_t ref_time = atoll(argv[1]);
  time_t tgt = ref_time+30;

  struct timeval tv;
  do {
    gettimeofday(&tv, NULL);
  } while(tv.tv_sec < tgt);

  int node_id = atoi(argv[2]);
  assert(node_id >= 1);

  return 0;
}
