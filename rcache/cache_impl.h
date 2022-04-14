
#ifndef _CACHE_IMPL_H_
#define _CACHE_IMPL_H_

#include <stdint.h>

// in blk_alloc.c
void init_blk_alloc(uint8_t* mem_base_);
uint8_t* blk_alloc(void);
void blk_free(uint8_t*);

// in cache_impl.c
void init_cache(uint8_t* mem_base_);
void insert_block(uint64_t block, uint8_t* data);
uint8_t* get_block(uint64_t block);

#endif
