
#ifndef _REMOTE_INDEX_H_
#define _REMOTE_INDEX_H_

#include <stdint.h>
#include <assert.h>

#define REMOTE_BASE_ADDR ((void*) 0x600000000000ULL)
// messy beyond this point
#define CHUNKS 1

// deliberately packed into 64 bits since 64-bit R/W are atomic on x86
struct rindex_entry {
  uint8_t ridx[CHUNKS];
  uintptr_t raddr[CHUNKS];
};

static inline void write_rindex_entry(uint64_t* entry, uint8_t ridx, uintptr_t ptr)
{
  uint64_t _ridx = ridx;
  uint64_t _ptr = (ptr - (uintptr_t) REMOTE_BASE_ADDR) / (g_block_size_bytes * g_max_sge);
  assert(_ridx >= 0 && _ridx <= 0xff &&
         _ptr >= 0 && _ptr <= 0xffffff);
  *entry = (_ridx << 24) | (_ptr << 0);
}

static inline struct rindex_entry read_rindex_entry(uint64_t* entry)
{
  uint64_t v = *entry;
  uint8_t _ridx = v >> 24;
  uintptr_t _ptr = (v >> 0) & 0xffffff;
  _ptr = ((uintptr_t) REMOTE_BASE_ADDR) + _ptr * (g_block_size_bytes * g_max_sge);
  return (struct rindex_entry) {.ridx = {_ridx}, .raddr = {_ptr}};
}

#endif
