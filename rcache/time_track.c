
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#define BUCKET_TS_INTV_USECS 15000
#define LOG_SIZE 2048
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
  uint64_t head;
};

static inline uint64_t to_usecs(struct timespec ts)
{
  return (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);
}

void bucket_ring_init(struct bucket_ring* ring, struct timespec ts)
{
  for (size_t i = 0; i<RING_LEN; ++i) {
    struct alloc_log* log = malloc(sizeof(struct alloc_log));
    log->pos = 0;
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

  struct alloc_log* log = ring->buckets[rpos];
  size_t pos = __atomic_fetch_add(&(log->pos), 1, __ATOMIC_SEQ_CST);

  assert(pos < LOG_SIZE);

  struct alloc_entry* entry = &(log->blocks[pos]);
  entry->type = type;
  entry->alloc_num = alloc_num;
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

    if (info.ts_usecs + BUCKET_TS_INTV_USECS >= ts_before) {
      break;
    }

    struct alloc_log* log = ring->buckets[info.head_idx];

    if (log->pos > 0) {
      qsort(log->blocks, log->pos, sizeof(struct alloc_entry), cmp_alloc_entry);

      size_t lpos = 1;
      assert(log->blocks[0].type == REGISTER);
      while (lpos < log->pos) {
        if (log->blocks[lpos].type == RETURN && log->blocks[lpos-1].type == REGISTER) {
          lpos += 2;
        } else if (log->blocks[lpos].type == REGISTER) {
          printf("Kill lpos: %lu\n", lpos-1);
          // unmatched, we need to kill it ourself.
          lpos += 1;
        } else {
          assert(0 && "Insufficient pattern match.");
        }
      }

      if (log->blocks[LOG_SIZE-1].type == REGISTER) {
        //printf("Kill lpos: %lu\n", LOG_SIZE-1);
      }

      log->pos = 0;
    }
    
    info.ts_usecs += BUCKET_TS_INTV_USECS;
    info.head_idx = (info.head_idx + 1) & (RING_LEN-1);
    __atomic_store_n(&(ring->head), info.bits, __ATOMIC_SEQ_CST);
  }
}

struct bucket_ring m_ring;

void* revoker(void* arg)
{
  while (1) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    bucket_ring_revoke(&m_ring, to_usecs(ts));
  }
  return NULL;
}

int* rands;

void* appender(void* arg)
{
  static uint32_t ctr = 0;

  while (1) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    bucket_ring_register(&m_ring, to_usecs(ts), ctr);
    if (rands[ctr & (LOG_SIZE-1)]) {
      bucket_ring_return(&m_ring, to_usecs(ts), ctr);
    }
    ++ctr;

    struct timespec mts_start, mts_check;
    clock_gettime(CLOCK_MONOTONIC, &mts_start);
    while (1) {
      clock_gettime(CLOCK_MONOTONIC, &mts_check);
      if (to_usecs(mts_check) - to_usecs(mts_start) >= 64) {
        break;
      }
    }
  }
  return NULL;
}

int main()
{
  srand(time(NULL));
  rands = malloc(sizeof(int) * LOG_SIZE);
  for (size_t i = 0; i<LOG_SIZE; ++i) {
    rands[i] = rand() % 2;
  }

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  bucket_ring_init(&m_ring, ts);

  pthread_t appendthr, revokethr;
  pthread_create(&appendthr, NULL, appender, NULL);
  pthread_create(&revokethr, NULL, revoker, NULL);

  while (1) {}
}
