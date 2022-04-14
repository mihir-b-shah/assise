
//#include "cache_impl.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define BLK_SIZE 4096
#define MEM_SIZE 0x200000ULL
#define N_BLOCKS (MEM_SIZE/BLK_SIZE)

static uint8_t* mem_base = NULL;
static uint8_t* free_lhead = NULL;

void init_blk_alloc(uint8_t* mem_base_)
{
  mem_base = mem_base_;
  free_lhead = mem_base;

  // setup the free list
  // definition of mmap- the last pointer is NULL, and mmap has already set the last block to 0.
  for (size_t i = 0; i<N_BLOCKS-1; ++i) {
    uint64_t* view = (uint64_t*) &mem_base[BLK_SIZE*i];
    *view = (uint64_t) &mem_base[BLK_SIZE*(i+1)];
  }
}

uint8_t* blk_alloc(void)
{
  uint8_t* to_give = free_lhead;
  uint64_t* view = (uint64_t*) to_give;
  free_lhead = (uint8_t*) *view;
  return to_give;
}

void blk_free(uint8_t* ptr)
{
  uint64_t* view = (uint64_t*) ptr;
  *view = (uint64_t) free_lhead;
  free_lhead = ptr;
}

int main()
{
  uint8_t* region = calloc(1, MEM_SIZE);
  init_blk_alloc(region);

  uint8_t* ptrs[10];
  ptrs[0] = blk_alloc();
  for (int i = 1; i<10; ++i) {
    ptrs[i] = blk_alloc();
    assert(ptrs[i] == region + i*BLK_SIZE);
  }
  blk_free(ptrs[1]);
  blk_free(ptrs[3]);
  blk_free(ptrs[7]);

  assert(blk_alloc() == ptrs[7]);
  assert(blk_alloc() == ptrs[3]);
  assert(blk_alloc() == ptrs[1]);
  assert(blk_alloc() == region + 10*BLK_SIZE);

  return 0;
}
