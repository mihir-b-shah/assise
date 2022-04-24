
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <assert.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

#define IO_SIZE 128
#define NNUMS 8
#define WR_SIZE 4096
#define N_DUTY_CYCLE 1000000
#define WS_SIZE 1000000000

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

static const int max_nums[NNUMS] = {1 ,3 ,1 ,3 ,1 ,1 ,1 ,1 };
static const int min_nums[NNUMS] = {-2,-4,-2,-4,-2,-2,-2,-2};
static const int shifts[NNUMS]   = {2, 3, 2, 3, 2, 2, 2, 2 };

enum {
  JMP_RAND,
  JMP_IN,
  JMP_OUT,
  JMP_LEFT,
  JMP_RIGHT,
  THR_LAST
};

struct blkno {
  int nums[NNUMS];
  int idx;
  int thrs[THR_LAST];
};

void set(struct blkno* v, uint32_t new_rand)
{
  for (int i = 0; i<NNUMS; ++i) {
    int raw = new_rand & ((1 << shifts[i]) - 1);
    v->nums[i] = raw+min_nums[i];
    // guaranteed rand() is non-negative.
    new_rand >>= shifts[i];
  }
}

uint32_t make(struct blkno* v)
{
  int shift = 0;
  uint32_t res = 0;
  for (int i = 0; i<NNUMS; ++i) {
    res += (v->nums[i] - min_nums[i]) << shift;
    shift += shifts[i];
  }
  return res;
}

// rand,skew should be between 0 and 100.
void init(struct blkno* v, int rand_amt, int skew_amt)
{
  uint32_t initial = rand();
  set(v, initial);

  v->idx = 0;

  v->thrs[JMP_RAND] = rand_amt;

  int remaining = 100-rand_amt;
  int jlr = remaining*3/5;
  int jud = remaining*2/5;

  v->thrs[JMP_IN] = jud*skew_amt/100;
  v->thrs[JMP_OUT] = jud*(100-skew_amt)/100;
  v->thrs[JMP_LEFT] = jlr/2;
  v->thrs[JMP_RIGHT] = jlr/2;

  for (int i = 1; i<THR_LAST; ++i) {
    v->thrs[i] += v->thrs[i-1];
  }
}

void evolve(struct blkno* v)
{
  int r = rand() % 100;
  if (r < v->thrs[JMP_RAND]) {
    //printf("Rand.\n");
    int new_rand = rand();
    set(v, new_rand);
  } else if (r < v->thrs[JMP_IN]) {
    //printf("Jmp in.\n");
    if (v->idx > 0) {
      v->nums[v->idx] = 0;
      v->idx -= 1;
    }
  } else if (r < v->thrs[JMP_OUT]) {
    //printf("Jmp out.\n");
    if (v->idx < NNUMS-1) {
      v->idx += 1;
      v->nums[v->idx] = 0;
    }
  } else if (r < v->thrs[JMP_LEFT]) {
    //printf("Jmp left.\n");
    if (v->idx >= 0 && v->idx < NNUMS) {
      if (v->nums[v->idx] > min_nums[v->idx]) {
        v->nums[v->idx] -= 1;
      }
    }
  } else if (r < v->thrs[JMP_RIGHT]) {
    //printf("Jmp right.\n");
    if (v->idx >= 0 && v->idx < NNUMS) {
      if (v->nums[v->idx] < max_nums[v->idx]) {
        v->nums[v->idx] += 1;
      }
    }
  } else {
    /* just skip this step- we could have prevented it, but it probably has a 
     * nice self-reference effect. */
  }
}

int main(int argc, char** argv)
{
  int rand_amt = atoi(argv[1]);
  int skew_amt = atoi(argv[2]);

  struct blkno blk;
  init(&blk, rand_amt, skew_amt);
  
  int barfd = shm_open("schedbar", O_RDWR, ALLPERMS);
 	pthread_barrier_t* bar = mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_POPULATE, barfd, 0);
  
  pthread_barrier_wait(bar);

  int fd = det_open("/mlfs/table", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* wbuf = calloc(WR_SIZE, sizeof(uint8_t));
  for (int i = 0; i<WS_SIZE; i+=WR_SIZE) {
    int ret = det_write(fd, wbuf, WR_SIZE);
  }

  printf("Finished creating table.\n");
  
  pthread_barrier_wait(bar);

  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));
  
  // table should be 1G in size, so 2^18 possible blocks.
  for (int K = 0; K<2; ++K) {
    pthread_barrier_wait(bar);

    printf("Started cycle %d of reads.\n", K);
    for (int i = 0; i<N_DUTY_CYCLE; ++i) {
      // allow to range from [0,800000000 + change]
      evolve(&blk);
      uint32_t vrand = make(&blk);
      uint32_t blk_offs = 4096 * vrand;

      if (blk_offs + WR_SIZE < WS_SIZE) {
        int ret = det_pread64(fd, buf, IO_SIZE, blk_offs);
        det_lseek(fd, blk_offs, SEEK_SET);
        det_write(fd, buf, WR_SIZE);
      }
    }
    printf("Finished cycle %d of reads.\n", K);
    pthread_barrier_wait(bar);
    // sleepy time for spark to run
  }

  pthread_barrier_wait(bar);
  det_close(fd);
  return 0;
}
