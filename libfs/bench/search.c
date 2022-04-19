
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

#define IO_SIZE 128

/*
 * Notes from this paper: https://www.benchcouncil.org/BigDataBench/files/huafengxi.pdf
 *
 * Web search: model workloads with a transition matrix. Nodes should probably follow
 * the power-law distribution (Zipf's law)
 * Rate of requests- typical workloads are quite bursty and focused. The paper demonstrates
 * a Poisson distribution isn't probably the best fit. Maybe uniform + a random spike
 * that occurs at some frequency.
 *
 * Simulating Zipf's distribution: we will produce a 32-bit number as follows:
 * ntz(X) represents an index into a degree table. ntz=0 has the highest degree
 *
 * Keep in mind basic locality rules i.e. the 80/20 
 *
 * We do not simulate how focused a burst is, since it is probably small enough to fit
 * well within NVM shared area.
 */

#define NNUMS 8

static const int max_nums[NNUMS] = {1 ,3 ,1 ,3 ,1 ,3 ,1 ,3 };
static const int min_nums[NNUMS] = {-2,-4,-2,-4,-2,-4,-2,-4};
static const int shifts[NNUMS]   = {2, 3, 2, 3, 2, 3, 2, 3 };

struct blkno {
  int nums[NNUMS];
  int idx;
};

void init(struct blkno* v)
{
  for (int i = 0; i<NNUMS; ++i) {
    v->nums[i] = 0;
  }
}

uint32_t make(struct blkno* v)
{
  int shift = 0;
  uint32_t res = 0;
  for (int i = 0; i<NNUMS; ++i) {
    res += (v->nums[i] + min_nums[i]) << shift;
    shift += shifts[i];
  }
  return res;
}

void evolve(struct blkno* v)
{
  int r = rand() % 64;
  if (r < 10) {
    if (v->idx > 0) {
      v->nums[v->idx] = 0;
      v->idx -= 1;
    }
  } else if (r < 29) {
    if (v->idx >= 0 && v->idx < NNUMS) {
      if (v->nums[v->idx] < max_nums[v->idx]) {
        v->nums[v->idx] += 1;
      }
    }
  } else if (r < 48) {
    if (v->idx >= 0 && v->idx < NNUMS) {
      if (v->nums[v->idx] > min_nums[v->idx]) {
        v->nums[v->idx] -= 1;
      }
    }
  } else {
    if (v->idx < NNUMS-1) {
      v->idx += 1;
      v->nums[v->idx] = 0;
    }
  }
}

int main()
{
  int fd = det_open("/mlfs/table", O_RDWR, 0666);
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  // table should be 2G in size, so 2^20 possible blocks.
  struct blkno blk;
  init(&blk);

  for (int i = 0; i<1000000000; ++i) {
    int ret = det_pread64(fd, buf, IO_SIZE, 4096*make(&blk));
    evolve(&blk);
  }

  return 0;
}
