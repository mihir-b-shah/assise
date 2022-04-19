
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

#define IO_SIZE 128
#define SLEEP_INTV 1

int main()
{
  int fd = det_open("/mlfs/model", O_RDWR | O_CREAT | O_TRUNC, 0666);
  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));

  // table should be 2G in size, so 2^20 possible blocks.
  
  for (int i = 0; i<10; ++i) {
    for (int j = 0; j<200000; j+=4096) {
      int ret = det_pwrite(fd, buf, IO_SIZE);
    }
    sleep(SLEEP_INTV);
    det_lseek(fd, 0, SEEK_SET);
  }

  return 0;
}
