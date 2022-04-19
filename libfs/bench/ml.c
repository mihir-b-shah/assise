
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

// conv params
#define IO_SIZE 4096
#define MODEL_SIZE 300000000 // seems big, but can be equivalent of many large NN-models.

// tuning params
#define SCHED_INTV 5
#define N_ITERS 2

int main()
{
  int fd = det_open("/mlfs/model", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  for (int i = 0; i<N_ITERS; ++i) {
    for (int j = 0; j<MODEL_SIZE; j+=4096) {
      int ret = det_write(fd, buf, IO_SIZE);
    }
    det_lseek(fd, 0, SEEK_SET);

    sleep(SCHED_INTV);
  }

  return 0;
}
