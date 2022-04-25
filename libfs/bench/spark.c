
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <pthread.h>
#include <time.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

// conv params
#define IO_SIZE 4096
#define RDD_SIZE 1000000000

int main()
{
  int barfd = shm_open("schedbar", O_RDWR, ALLPERMS);
 	pthread_barrier_t* bar = mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_POPULATE, barfd, 0);
  
  pthread_barrier_wait(bar);

  int fd = det_open("/mlfs/rdd", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));
  
  pthread_barrier_wait(bar);

  for (int K = 0; K<2; ++K) {
    pthread_barrier_wait(bar);
    pthread_barrier_wait(bar);
    
    // spark would typically perform lots of computation and then checkpoint- can just ignore this part.

    clock_t t = clock();
    printf("Starting cycle %d of writes.\n", K);
    for (int j = 0; j<RDD_SIZE; j+=4096) {
      int ret = det_write(fd, buf, IO_SIZE);
    }
    printf("Ending cycle %d of writes, time=%f.\n", K, ((double) (clock()-t))/CLOCKS_PER_SEC);
    det_lseek(fd, 0, SEEK_SET);
  }

  pthread_barrier_wait(bar);
  det_close(fd);
  return 0;
}
