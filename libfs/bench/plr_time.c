
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

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>
#include <cache/cache.h>

#define IO_SIZE 128
#define WR_SIZE 4096
#define N_DUTY_CYCLE 1000000
#define WS_SIZE 1000000000

int main(int argc, char** argv)
{
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

  uint32_t* queries = calloc(N_DUTY_CYCLE, sizeof(uint32_t));
  int* hit_remote = calloc(N_DUTY_CYCLE, sizeof(int));
  int local_id = -1;
  
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
      
      if (K == 1) {
        queries[i] = blk_offs;

        if (id_rcache_VISIBLE > local_id && offs_rcache_VISIBLE == blk_offs) {
          // something happened.
          hit_remote[i] = res_rcache_VISIBLE;
          local_id = id_rcache_VISIBLE;
        }
      }
    }

    if (K == 1) {
      FILE* f = fopen("query_log.txt", "w");
      for (int i = 0; i<N_DUTY_CYCLE; ++i) {
        fprintf(f, "%u %d\n", queries[i], hit_remote[i]);
      }
      fclose(f);
    }

    pthread_barrier_wait(bar);
    // sleepy time for spark to run
  }

  pthread_barrier_wait(bar);
  det_close(fd);
  return 0;
}
