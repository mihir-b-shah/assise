
#include <math.h>
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
#define WR_SIZE 4096
#define N_DUTY_CYCLE 1000000
#define WS_SIZE 1000000000

/*
 * Like search.c, this approximates the power-law distribution, but more conveniently- by giving consistent 
 * replays of working sets, not randomly varying ones. Still realistic, since working sets probably only
 * drift in the long-run.
 *
 * https://mathworld.wolfram.com/RandomNumber.html*
 */

static uint32_t X1pN = 0;
static uint32_t np = 0;

uint32_t pl_rand()
{
  double y = ((double) rand())/RAND_MAX;
  return (uint32_t) pow(X1pN * y, 1.0/(np+1));
}

int main(int argc, char** argv)
{
  np = atoi(argv[1]);
  X1pN = pow(WS_SIZE/4096, np);

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
      uint32_t blk_offs = 4096 * pl_rand();

      if (blk_offs + WR_SIZE < WS_SIZE) {
        int ret = det_pread64(fd, buf, IO_SIZE, blk_offs);
        det_lseek(fd, blk_offs, SEEK_SET);
        det_write(fd, buf, IO_SIZE);
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
