
#define _GNU_SOURCE

#include "ssd_emulation.h"

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <storage/storage.h>

static int shm_fd;
static int mock_fd;
static const char* shm_path = "ssd_map";
static uint8_t* map_base;
static volatile uint64_t num_migrated;

// sort of correct, but fine for our purposes
static uint64_t round_up(uint64_t v, unsigned pow2)
{
  return (v << pow2 >> pow2) + (1 << pow2);
}

static char buf[4096];
void init_ssd_emul(void)
{
  mock_fd = open("/tmp/mihirs_bak/mock_ssd", O_CREAT | O_RDWR | O_DIRECT | O_SYNC, ALLPERMS);
  write(mock_fd, buf, 4096);

  #ifdef KERNFS
  shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, ALLPERMS);
  #else
  shm_fd = shm_open(shm_path, O_RDWR, ALLPERMS);
  #endif
	if (shm_fd < 0) {
		fprintf(stderr, "cannot open ssd bitmap %s\n", shm_path);
		exit(-1);
	}
  
  size_t shm_size = round_up(dev_size[g_root_dev], g_block_size_shift);

  #ifdef KERNFS
  ftruncate(shm_fd, shm_size);
  #endif

  map_base = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (map_base == MAP_FAILED) {
		perror("cannot map ssd_map file");
		exit(-1);
	}

  num_migrated = 0;
  printf("ssd bitmap initialized with size %lx\n", shm_size);
}

uint64_t get_num_migrated(void)
{
  return num_migrated;
}

void send_to_ssd(uint64_t blk)
{
  ++num_migrated;
  map_base[blk] = 1;
  /* synchronize? we can probably tolerate some staleness here,
   * since if we think something never went to ssd, it doesn't harm correctness of fs */
  // not calling ssd_emul_latency since cache latency is prob already high.
}

int ssd_has_blk(uint64_t blk)
{
  return map_base[blk];
}

void destroy_ssd_emul(void)
{
  shm_unlink(shm_path);
}

void ssd_emul_latency_read()
{
  pread64(mock_fd, buf, 4096, 0);
}
