
#include <assert.h>
#include <stdint.h>
#include <list>
#include <unordered_map>

struct block_handle {
  const uint64_t block_num;
  const uint8_t* data;

  block_handle(uint64_t bn, uint8_t* dat) : block_num(bn), data(dat) {}
};

static size_t n_blocks = 0; 
static std::list<block_handle> lru_list;
static std::unordered_map<uint64_t, std::list<block_handle>::iterator> lru_map;

extern "C" void lru_init(size_t n_blocks_)
{
  n_blocks = n_blocks_;
}

// return evicted block, if any.
extern "C" uint8_t* lru_insert_block(uint64_t block, uint8_t* data)
{
  auto iter = lru_map.find(block);
  if (iter != lru_map.end()) {
    //assert(0);
    auto& list_iter = iter->second;
    list_iter->data = data;
    return nullptr;
  }

  if (lru_map.size() == n_blocks) {
    // evict the lru
    block_handle bh = lru_list.back();
    lru_map.erase(bh.block_num);
    lru_list.pop_back();
    return const_cast<uint8_t*>(bh.data);
  }

  lru_list.emplace_front(block, data);
  lru_map[block] = lru_list.begin();
  return nullptr;
}

extern "C" uint8_t* lru_get_block(uint64_t block)
{
  const auto& map_iter = lru_map.find(block);
  if (map_iter == lru_map.end()) {
    return nullptr;
  }

  auto& lru_iter = map_iter->second;
  block_handle bh = *lru_iter;

  lru_list.erase(lru_iter);
  lru_list.push_front(bh);
  lru_map[block] = lru_list.begin();
  return const_cast<uint8_t*>(bh.data);
}