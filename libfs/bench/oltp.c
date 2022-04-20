
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <pthread.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

// conv params
#define WR_SIZE 4096
#define IO_SIZE 128

// tuning params
#define N_READ_DUTY_CYCLE 1000000
#define OFF_TIME 2

int main()
{
  int barfd = shm_open("schedbar", O_RDWR, ALLPERMS);
 	pthread_barrier_t* bar = mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_POPULATE, barfd, 0);
  
  pthread_barrier_wait(bar);

  int fd = det_open("/mlfs/table", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* wbuf = calloc(WR_SIZE, sizeof(uint8_t));
  for (int i = 0; i<1000000000; i+=WR_SIZE) {
    int ret = det_write(fd, wbuf, WR_SIZE);
  }
  
  pthread_barrier_wait(bar);

  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  // table should be 2G in size, so 2^20 possible blocks.
  for (int K = 0; K<2; ++K) {
    pthread_barrier_wait(bar);
    for (int i = 0; i<N_READ_DUTY_CYCLE; ++i) {
      // allow to range from [0,800000000]
      uint32_t blk_offs = 4096 * (rand() % 200000);
      int ret = det_pread64(fd, buf, IO_SIZE, blk_offs);

      det_lseek(fd, 0, blk_offs);
      det_write(fd, buf, IO_SIZE);
    }
    pthread_barrier_wait(bar);
    // sleepy time for spark to run
  }

  pthread_barrier_wait(bar);
  det_close(fd);
  return 0;
}
