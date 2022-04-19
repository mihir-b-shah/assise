
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

#define WR_SIZE 4096

int main()
{
  int fd = det_open("/mlfs/table", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* fcont = calloc(WR_SIZE, sizeof(uint8_t));

  for (int i = 0; i<1000000000; i+=4096) {
    int ret = det_write(fd, fcont, WR_SIZE);
  }

  det_close(fd);
  return 0;
}
