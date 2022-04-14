

#include "cache_impl.h"

static uint8_t* mem_base = NULL;
static size_t mem_size = 0;

void init_cache(uint8_t* mem_base_, size_t mem_size_)
{
  mem_base = mem_base_;
  mem_size = mem_size_;
}

void insert_block(uint64_t block, uint8_t* data)
{
}

uint8_t* get_block(uint64_t block)
{
}
