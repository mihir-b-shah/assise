
#include "conf.h"

#include <stdint.h>
#include <assert.h>
#include <limits.h>

/* we verified this hash has good distibution properties, even for a small number of random node_id's
 * with high order bits the same, in the rcache/tests/hash_test.c file */
static uint64_t hash(uint64_t x)
{
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x = x ^ (x >> 31);
  return x;
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
  
  /* technically not correct- this is (2^64 - 1)/n instead of (2^64/n)
   * but for any reasonable n, who cares? */
  idx = hash(key) / (UINT64_MAX / (ctx->n));
  assert(idx >= 0 && idx < ctx->n);

  /* FIXME: dangerous- if this escapes the lifetime of the backing array.
   * make sure to only call update_cache_conf from ONE application thread */
  return &(ctx->conn_ring[idx]);
}
