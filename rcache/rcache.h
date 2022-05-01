
#ifndef _RCACHE_H_
#define _RCACHE_H_

#include <stdint.h>
#include <stddef.h>

#define BLK_SIZE 4096

// in blk_alloc.c
void blk_init(uint8_t* mem_base_, size_t mem_size_);
uint8_t* blk_alloc(void);
void blk_free(uint8_t*);

// in lru_impl.cc
void lru_init(size_t n_blocks);
uint8_t* lru_try_evict();
void lru_insert_block(uint64_t block, uint8_t* data);
uint8_t* lru_get_block(uint64_t block);
uint8_t* lru_get_block_mru(uint64_t block);
size_t lru_size();

#endif
