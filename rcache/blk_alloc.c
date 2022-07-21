
#include "rcache.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include "globals.h"

#define META_NULL 0xffffffffU
struct meta_entry {
  volatile uint32_t next;
};

struct alloc {
  struct block {
    // TODO: add tag
    volatile uint8_t bytes[BLK_SIZE];
  };
  volatile struct block blocks[MAX_SGE];
};

static struct alloc* mem;
static struct meta_entry* meta;
static volatile size_t head;
static volatile size_t tail;
static size_t n_blocks;
static size_t n_blocks_shift;
static pthread_spin_lock lock;

// for simplicity, assume no sockfd disconnections.
static volatile uint32_t alloc_cts[MAX_CONNECTIONS];

static inline int is_pow_2(size_t v)
{
  return (v & -v) == v;
}

void blk_init(uint8_t* mem_base, size_t mem_size)
{
  n_blocks = mem_size / BLK_SIZE;
  assert(is_pow_2(n_blocks));
  n_blocks_shift = __builtin_ctz(n_blocks);
  mem = (struct alloc*) mem_base;
  head = 0;
  tail = n_blocks - 1;
  meta = calloc(n_blocks, sizeof(struct meta_entry));
  pthread_spin_init(&lock, 0);

  for (size_t i = 0; i<n_blocks; ++i) {
    meta[i].next = i+1;
  }
  meta[n_blocks-1].next = META_NULL;

  assert(sizeof(alloc) == ALLOC_SIZE);
}

uint8_t* blk_alloc(int sockfd)
{
  uint32_t ip_id = mp_channel_ipaddr(sockfd);

  pthread_spin_lock(&lock);
  // invariant: no one points to head.
  struct alloc* ret = &mem[head];
  /* TODO
  for (size_t i = 0; i<MAX_SGE; ++i) {
    // an atomic operation.
    ret->blocks[i].tag = (((uint64_t) ip_id) << 32) | alloc_cts[sockfd];
  }
  */
  uint32_t old_head = head;
  head = meta[head].next;
  /* Invariant- we should always have one block left, to simplify things. 
   * If there is ONLY one left, assert we're dead. This ensures head and tail
   * both point to something. */
  assert(head != META_NULL);
  meta[old_head].next = META_NULL;

  /* TODO alloc_cts[sockfd] += 1; */
  pthread_spin_unlock(&lock);
  /* no need for mfence- pthread spin enter-exit uses lock prefix,
   * which on x86 is a full fence, ensuring these writes are seen by all cores */
  return (uint8_t*) ret;
}

void blk_free(uint32_t blk_num)
{
  pthread_spin_lock(&lock);

  assert(meta[tail].next == META_NULL);
  meta[tail].next = blk_num;
  tail = blk_num;

  pthread_spin_unlock(&lock);
}
