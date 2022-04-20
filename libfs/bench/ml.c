
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
#define MODEL_SIZE 200000000 // seems big, but can be equivalent of many large NN-models.

// tuning params
#define N_ITERS 2

int main()
{
  int fd = det_open("/mlfs/model", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  while (1) {
    for (int j = 0; j<MODEL_SIZE; j+=4096) {
      int ret = det_write(fd, buf, IO_SIZE);
    }
    det_lseek(fd, 0, SEEK_SET);
  }

  return 0;
}
