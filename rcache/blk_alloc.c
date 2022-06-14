
#include "rcache.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

static volatile uint8_t* free_lhead = NULL;
static uint8_t* mem_base = NULL;
static size_t n_blocks = 0;

static void set_lhead(uint8_t* p)
{
  __atomic_store_n(&free_lhead, p, __ATOMIC_SEQ_CST);
}

static volatile uint8_t* get_lhead()
{
  return __atomic_load_n(&free_lhead, __ATOMIC_SEQ_CST);
}

void blk_init(uint8_t* mem_base_, size_t mem_size_)
{
  n_blocks = mem_size_ / BLK_SIZE;
  mem_base = mem_base_;

  set_lhead(mem_base);

  // setup the free list
  // definition of mmap- the last pointer is NULL, and mmap has already set the last block to 0.
  for (size_t i = 0; i<n_blocks-1; ++i) {
    uint64_t* view = (uint64_t*) &mem_base[BLK_SIZE*i];
    *view = (uint64_t) &mem_base[BLK_SIZE*(i+1)];
  }
}

uint8_t* blk_alloc(void)
{
  if (!get_lhead()) {
    assert(0 && "Could not alloc a block.\n");
    return NULL;
  }

  volatile uint8_t* to_give = get_lhead();
  volatile uint64_t* view = (volatile uint64_t*) to_give;
  set_lhead((uint8_t*) *view);
  return (uint8_t*) to_give;
}

void blk_free(uint8_t* ptr)
{
  uint64_t* view = (uint64_t*) ptr;
  *view = (uint64_t) get_lhead();
  set_lhead(ptr);
}
