
#ifndef _LRU_MAP_H_
#define _LRU_MAP_H_

#include "filesystem/slru.h"
#include <stdint.h>

void lmap_insert(uint64_t blk, lru_node_t* node);
lru_node_t* lmap_find(uint64_t blk);
void lmap_erase(uint64_t blk);

#endif
