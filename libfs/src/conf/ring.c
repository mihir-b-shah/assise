
#include "conf.h"

#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>

/* we verified this hash has good distibution properties, even for a small number of random node_id's
 * with high order bits the same, in the rcache/tests/hash_test.c file */
static uint64_t hash(uint64_t x)
{
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x = x ^ (x >> 31);
  return x;
}

static uint32_t hash32(uint32_t x)
{
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x;
}

//#define TOY

#ifndef TOY
#define NUM_MASKS 10
#define NUM_TOTAL 5
#define NUM_COLOR 3
static uint32_t masks[NUM_MASKS] = {7, 11, 13, 14, 19, 21, 22, 25, 26, 28};
#else
#define NUM_MASKS 3
#define NUM_TOTAL 3
#define NUM_COLOR 2
static uint32_t masks[NUM_MASKS] = {3, 5, 6};
#endif

// implement hash ring partitioning
static int get_index(uint64_t hash, int n)
{
  if (n == 0) {
    return -1;
  } else if (n == 1) {
    return 0;
  } else {
    // right now, just a hack for experimentation.
    assert(n == NUM_TOTAL);

    uint32_t unif32 = hash32(ip_int);
    uint32_t mask = masks[unif32 % NUM_MASKS];
    
    /* technically not correct- this is (2^64 - 1)/n instead of (2^64/n)
     * but for any reasonable n, who cares? */
    int region = hash / (UINT64_MAX / (NUM_COLOR));

    int ctr = 0;
    for (int i = 0; i<NUM_TOTAL; ++i) {
      if ((mask & (1 << i)) && ctr++ == region) {
        return i;
      }
    }

    printf("region: %d, mask: %u\n", region, mask);
    assert(0);
    return -1;
  }
}

struct conn_obj* get_dest(uint64_t block_no)
{
  assert(block_no < UINT32_MAX);
  uint64_t key = (((uint64_t) ip_int) << 32) | block_no;

  // should be cheap, since the version numbers are usually same.
  struct conn_ctx* ctx = update_cache_conf();
  int idx;

  if (ctx->n == 0) {
    return NULL;
  }
  
  idx = get_index(hash(key), ctx->n);
  assert(idx >= 0 && idx < ctx->n);

  /* FIXME: dangerous- if this escapes the lifetime of the backing array.
   * make sure to only call update_cache_conf from ONE application thread */
  return &(ctx->conn_ring[idx]);
}
