
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>

#define BUCKET_TS_INTV_USECS 20000
#define LOG_SIZE 1024
#define RING_LEN 32
  
enum alloc_entry_type {
  REGISTER,
  RETURN,
};

struct __attribute__((packed, aligned(4))) alloc_entry {
  enum alloc_entry_type type : 8;
  uint32_t alloc_num : 24;
};

struct alloc_log {
  struct alloc_entry blocks[LOG_SIZE];
  size_t pos;
  struct alloc_log* prev;
};

union head_info {
  struct __attribute__((packed, aligned(8))) {
    uint64_t ts_usecs : 56;
    uint8_t head_idx : 8;
  };
  uint64_t bits;
};

struct bucket_ring {
  struct alloc_log* buckets[RING_LEN];
  pthread_spinlock_t locks[RING_LEN];
  uint64_t head;
};

static inline uint64_t to_usecs(struct timespec ts)
{
  return (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);
}

void bucket_ring_init(struct bucket_ring* ring, struct timespec ts)
{
  for (size_t i = 0; i<RING_LEN; ++i) {
    pthread_spin_init(&(ring->locks[i]), 0);
    struct alloc_log* log = malloc(sizeof(struct alloc_log));
    log->pos = 0;
    log->prev = NULL;
    ring->buckets[i] = log;
  }
  
  union head_info info;
  info.ts_usecs = to_usecs(ts);
  info.head_idx = 0;
  ring->head = info.bits;
}

static inline void bucket_ring_append(struct bucket_ring* ring, uint64_t ts, uint32_t alloc_num, enum alloc_entry_type type)
{
  union head_info info;
  info.bits = __atomic_load_n(&(ring->head), __ATOMIC_SEQ_CST);
  size_t rpos = (((ts - info.ts_usecs) / BUCKET_TS_INTV_USECS) + info.head_idx) & (RING_LEN-1);

  pthread_spin_lock(&(ring->locks[rpos]));
  struct alloc_log* log = ring->buckets[rpos];

  if (log->pos == LOG_SIZE) {
    struct alloc_log* new_log = malloc(sizeof(struct alloc_log));
    new_log->pos = 0;
    new_log->prev = log;
    ring->buckets[rpos] = new_log;
    log = new_log; 
  }

  struct alloc_entry* entry = &(log->blocks[log->pos++]);
  entry->type = type;
  entry->alloc_num = alloc_num;
  pthread_spin_unlock(&(ring->locks[rpos]));
}

void bucket_ring_register(struct bucket_ring* ring, uint64_t ts, uint32_t alloc_num)
{
  bucket_ring_append(ring, ts, alloc_num, REGISTER);
}

// get ts from the meta map, when sending in.
void bucket_ring_return(struct bucket_ring* ring, uint64_t ts, uint32_t alloc_num)
{
  bucket_ring_append(ring, ts, alloc_num, RETURN);
}

int cmp_alloc_entry(const void* a, const void* b)
{
  int va = (((const struct alloc_entry*) a)->alloc_num << 7)
    | ((const struct alloc_entry*) a)->type;
  int vb = (((const struct alloc_entry*) b)->alloc_num << 7)
    | ((const struct alloc_entry*) b)->type;
  return va - vb;
}

void bucket_ring_revoke(struct bucket_ring* ring, uint64_t ts_before)
{
  while (1) {
    union head_info info;
    info.bits = __atomic_load_n(&(ring->head), __ATOMIC_SEQ_CST);

    if (info.ts_usecs + BUCKET_TS_INTV_USECS < ts_before) {
      pthread_spin_lock(&(ring->locks[info.head_idx]));
      struct alloc_log* log = ring->buckets[info.head_idx];

      while (log != NULL) {
        if (log->pos > 0) {
          qsort(log->blocks, log->pos, sizeof(struct alloc_entry), cmp_alloc_entry);

          size_t lpos = 1;
          assert(log->blocks[0].type == REGISTER);
          while (lpos < LOG_SIZE) {
            if (log->blocks[lpos].type == RETURN && log->blocks[lpos-1].type == REGISTER) {
              lpos += 2;
            } else if (log->blocks[lpos].type == REGISTER) {
              // unmatched, we need to kill it ourself.
              lpos += 1;
            } else {
              assert(0 && "Insufficient pattern match.");
            }
          }
          log->pos = 0;
        }
        
        struct alloc_log* prev = log->prev;
        if (log != ring->buckets[info.head_idx]) {
          free(log);
        } else {
          log->prev = NULL;
        }
        log = prev;
      }
      
      pthread_spin_unlock(&(ring->locks[info.head_idx]));
      info.ts_usecs += BUCKET_TS_INTV_USECS;
      info.head_idx = (info.head_idx + 1) & (RING_LEN-1);
      __atomic_store_n(&(ring->head), info.bits, __ATOMIC_SEQ_CST);

    } else {
      break;
    }
  }

}
