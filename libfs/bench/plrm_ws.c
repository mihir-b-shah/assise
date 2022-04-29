
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
#include <time.h>
#include <sys/time.h>

#include <mlfs/mlfs_interface.h>	
#include <intf/fcall_api.h>

#define RDD_SIZE 1000000000
#define N_DUTY_CYCLE 1000000
#define IO_SIZE 128
#define WR_SIZE 4096
#define WS_SIZE 1000000000

void spin_until_tv(time_t ref_time, uint32_t offset)
{
  time_t tgt = ref_time+offset;
  struct timeval tv;
  do {
    gettimeofday(&tv, NULL);
  } while(tv.tv_sec < tgt);
}

void run_oltp(int fd, char* buf)
{
  for (int i = 0; i<N_DUTY_CYCLE; ++i) {
    uint32_t blk_offs = 4096 * pl_rand();

    if (blk_offs + WR_SIZE < WS_SIZE) {
      int ret = det_pread64(fd, buf, IO_SIZE, blk_offs);
      det_lseek(fd, blk_offs, SEEK_SET);
      det_write(fd, buf, IO_SIZE);
    }
  }
  det_lseek(fd, 0, SEEK_SET);
}

int main(int argc, char** argv)
{
  time_t ref_time = atoll(argv[1]);
  int node_id = atoi(argv[2]);
  assert(node_id >= 1);
  
  spin_until_tv(ref_time, 30);
  printf("Ended round 0.\n");

  /**************************************************
   *    END OF ROUND 0 - START OF WORKLOAD SETUP
   **************************************************/

  int rfd = det_open("/mlfs/rdd", O_RDWR | O_CREAT | O_TRUNC, 0666);
  int tfd = det_open("/mlfs/table", O_RDWR | O_CREAT | O_TRUNC, 0666);

  uint8_t* buf = calloc(IO_SIZE, sizeof(uint8_t));
  uint8_t* wbuf = calloc(WR_SIZE, sizeof(uint8_t));

  for (int i = 0; i<WS_SIZE; i+=WR_SIZE) {
    int ret = det_write(tfd, wbuf, WR_SIZE);
  }
    
  /* technically, running in same process with different access patterns should work too.
     just simplifies logistics of running the experiment. Caching behaviors are slightly
     different, but since OLAP only operates in NVM, should be fine. */

  // setup our working set - time: 33 secs
  run_oltp(tfd, buf);

  /* 90 secs for this period, so 30+90=120. Then, since we want each node to enter olap
   * staggered, we add 60 (our upper bound for round 2) successively.
   */
  spin_until_tv(ref_time, 120 + 60*(node_id-1));
  printf("Ended round 1.\n");

  /**************************************************
   *    END OF ROUND 1 - START OF OLAP PERIOD       
   **************************************************/

  for (int j = 0; j<RDD_SIZE; j+=4096) {
    int ret = det_write(rfd, buf, WR_SIZE);
  }
  det_lseek(rfd, 0, SEEK_SET);
  
  spin_until_tv(ref_time, 120 + 60*node_id);
  printf("Ended round 2.\n");
  
  /**************************************************
   *    END OF ROUND 2 - START OF OLTP PERIOD
   **************************************************/

  /* takes 288 seconds- so 20% is roughly 60 seconds- thus, we can only recover 20% of our working set
     in isolation, before the next node starts using. */
  run_oltp(tfd, buf);
  printf("Ended round 3.\n");

  /**************************************************
   *    END OF ALL ROUNDS
   **************************************************/

  det_close(rfd);
  det_close(tfd);
  return 0;
}
