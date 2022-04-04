
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

#include <storage/storage.h>

static int shm_fd;
static const char* shm_path = "ssd_map";
static uint8_t* map_base;

// sort of correct, but fine for our purposes
static uint64_t round_up(uint64_t v, unsigned pow2)
{
  return (v << pow2 >> pow2) + (1 << pow2);
}

void init_ssd_emul(void)
{
  shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, ALLPERMS);
	if (shm_fd < 0) {
		fprintf(stderr, "cannot open ssd bitmap %s\n", shm_path);
		exit(-1);
	}
  
  size_t shm_size = round_up(dev_size[g_root_dev], g_block_size_shift);
  ftruncate(shm_fd, shm_size);

  map_base = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (map_base == MAP_FAILED) {
		perror("cannot map ssd_map file");
		exit(-1);
	}
}

void send_to_ssd(uint64_t blk)
{
  map_base[blk] = 1;
  /* synchronize? we can probably tolerate some staleness here,
   * since if we think something never went to ssd, it doesn't harm correctness of fs */
}

int ssd_has_blk(uint64_t blk)
{
  return map_base[blk];
}

void destroy_ssd_emul(void)
{
  shm_unlink(shm_path);
}
