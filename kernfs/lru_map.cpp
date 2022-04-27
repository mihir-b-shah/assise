
#include <unordered_map>
#include <cstdlib>
#include "filesystem/slru.h"

/* A permissive map- the goal is to avoid false negatives that are occurring from
 * the lru map in migrate.c. */

static std::unordered_map<uint64_t, lru_node_t*> nodes;

extern "C" void lmap_insert(uint64_t blk, lru_node_t* node)
{
  nodes[blk] = node;
}

extern "C" lru_node_t* lmap_find(uint64_t blk)
{
  return nodes.find(blk) == nodes.end() ? NULL : nodes[blk];
}

extern "C" void lmap_erase(uint64_t blk)
{
  nodes.erase(blk);
}
