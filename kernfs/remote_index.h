
#ifndef _REMOTE_INDEX_H_
#define _REMOTE_INDEX_H_

#include <stdint.h>
#include <assert.h>

#define REMOTE_BASE_ADDR ((void*) 0x600000000000ULL)

// deliberately packed into 64 bits since 64-bit R/W are atomic on x86
struct rindex_entry {
  uint8_t ridx;
  uint32_t blknum;
  uintptr_t raddr;
};

static inline void write_rindex_entry(uint64_t* entry, uint8_t ridx, uint32_t blknum, uintptr_t ptr)
{
  uint64_t _ridx = ridx;
  uint64_t _ptr = (ptr - (uintptr_t) REMOTE_BASE_ADDR) / (g_block_size_bytes * g_max_sge);
  uint64_t _blknum = blknum;
  assert(_ridx >= 0 && _ridx <= 0xff &&
         _ptr >= 0 && _ptr <= 0xffffff &&
         _blknum >= 0 && _blknum <= 0xffffffff);
  *entry = (_ridx << 56) | (_ptr << 32) | _blknum;
}

static inline struct rindex_entry read_rindex_entry(uint64_t* entry)
{
  uint64_t v = *entry;
  uint8_t _ridx = v >> 56;
  uint32_t _blknum = v & 0xffffffff;
  uintptr_t _ptr = (v >> 32) & 0xffffff;
  _ptr = ((uintptr_t) REMOTE_BASE_ADDR) + _ptr * (g_block_size_bytes * g_max_sge);
  return (struct rindex_entry) {.ridx = _ridx, .blknum = _blknum, .raddr = _ptr};
}

#endif
