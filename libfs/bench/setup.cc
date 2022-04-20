
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <pthread.h>

int main()
{
  int barfd = shm_open("schedbar", O_CREAT | O_RDWR, ALLPERMS);
  ftruncate(barfd, sizeof(pthread_barrier_t));

  pthread_barrierattr_t attr;
  pthread_barrierattr_init(&attr);
  pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

 	pthread_barrier_t* bar = (pthread_barrier_t*) mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE,
    MAP_SHARED | MAP_POPULATE, barfd, 0);
  pthread_barrier_init(bar, &attr, 2);

  return 0;
}
