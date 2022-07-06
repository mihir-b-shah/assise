
#ifndef _REMOTE_INDEX_H_
#define _REMOTE_INDEX_H_

#include <storage/storage.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define REMOTE_BASE_ADDR ((void*) 0x600000000000ULL)
// messy beyond this point
#define CHUNKS 1

static inline uint64_t* map_rindex()
{
  #ifdef KERNFS
  int shm_fd = shm_open("cache_index", O_CREAT | O_RDWR, ALLPERMS);
  #else
  int shm_fd = shm_open("cache_index", O_RDWR, ALLPERMS);
  #endif

	if (shm_fd < 0) {
		fprintf(stderr, "cannot open cache index.");
		exit(-1);
	}
  
  size_t shm_size = sizeof(uint64_t) * ((dev_size[g_root_dev] + g_block_size_bytes) >> g_block_size_shift);

  #ifdef KERNFS
  ftruncate(shm_fd, shm_size);
  #endif

  uint64_t* map_base = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (map_base == MAP_FAILED) {
		perror("cannot map cache_index file");
		exit(-1);
	}

  return map_base;
}

// deliberately packed into 64 bits since 64-bit R/W are atomic on x86
struct rindex_entry {
  bool _info_valid;
  uint8_t ridx[CHUNKS];
  uintptr_t raddr[CHUNKS];
};

static inline void write_rindex_entry(uint64_t* entry, uint8_t ridx, uintptr_t ptr)
{
  uint64_t _ridx = ridx;
  uint64_t _ptr = (ptr - (uintptr_t) REMOTE_BASE_ADDR) / (g_block_size_bytes * g_max_sge);
  assert(_ridx >= 0 && _ridx <= 0xff &&
         _ptr >= 0 && _ptr <= 0xffffff);
  *entry = (_ridx << 24) | (_ptr << 0);
}

static inline struct rindex_entry read_rindex_entry(uint64_t* entry)
{
  uint64_t v = *entry;
  uint8_t _ridx = v >> 24;
  uintptr_t _ptr = (v >> 0) & 0xffffff;
  _ptr = ((uintptr_t) REMOTE_BASE_ADDR) + _ptr * (g_block_size_bytes * g_max_sge);
  return (struct rindex_entry) {._info_valid = (v == 0), .ridx = {_ridx}, .raddr = {_ptr}};
}

#endif
