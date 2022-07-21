
#ifndef _RCACHE_H_
#define _RCACHE_H_

#include <stdint.h>
#include <stddef.h>

#define MAX_SGE 96
#define BLK_SIZE 4096
#define NUM_ALLOC 10
#define ALLOC_SIZE (4096*96)

// in blk_alloc.c
void blk_init(uint8_t* mem_base_, size_t mem_size_);
uint8_t* blk_alloc(void);
void blk_free(uint8_t*);

#endif
