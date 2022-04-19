
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

// conv params
#define IO_SIZE 128

// tuning params
#define N_ITERS 10
#define N_READ_DUTY_CYCLE 1000000
#define OFF_TIME 2

int main()
{
  int fd = det_open("/mlfs/table", O_RDWR, 0666);
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  // table should be 2G in size, so 2^20 possible blocks.
  for (int j = 0; j<N_ITERS; ++j) {
    for (int i = 0; i<N_READ_DUTY_CYCLE; ++i) {
      // allow to range from [0,800000]
      uint32_t blk_offs = 4096 * (rand() % 200000);
      int ret = det_pread64(fd, buf, IO_SIZE, blk_offs);

      det_lseek(fd, 0, blk_offs);
      det_write(fd, buf, IO_SIZE);
    }

    sleep(OFF_TIME);
  }

  det_close(fd);
  return 0;
}
