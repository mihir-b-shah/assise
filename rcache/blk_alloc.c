
#include "rcache.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

static uint8_t* mem_base = NULL;
static uint8_t* free_lhead = NULL;
static size_t n_blocks = 0;

void blk_init(uint8_t* mem_base_, size_t mem_size_)
{
  n_blocks = mem_size_ / BLK_SIZE;
  mem_base = mem_base_;
  free_lhead = mem_base;

  // setup the free list
  // definition of mmap- the last pointer is NULL, and mmap has already set the last block to 0.
  for (size_t i = 0; i<n_blocks-1; ++i) {
    uint64_t* view = (uint64_t*) &mem_base[BLK_SIZE*i];
    *view = (uint64_t) &mem_base[BLK_SIZE*(i+1)];
  }
}

uint8_t* blk_alloc(void)
{
  if (!free_lhead) {
    return NULL;
  }

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
