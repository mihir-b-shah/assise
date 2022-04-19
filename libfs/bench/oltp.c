
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

#define WR_SIZE 4096
#define IO_SIZE 128

int main()
{
  int fd = det_open("/mlfs/table", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* fcont = calloc(WR_SIZE, sizeof(uint8_t));
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  // table should be 2G in size, so 2^20 possible blocks.
  for (int i = 0; i<1500000000; i+=4096) {
    int ret = det_write(fd, fcont, WR_SIZE);
  }

  // table should be 2G in size, so 2^20 possible blocks.
  for (int i = 0; i<1000000; ++i) {
    uint32_t blk_offs = 4096 * (rand() % 1000000);
    int ret = det_pread64(fd, buf, IO_SIZE, blk_offs);
  }

  det_close(fd);
  return 0;
}
