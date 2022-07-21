
#define BUCKET_TS_INTV_USECS 20000
#define LOG_SIZE 1024
#define RING_LEN 32
  
enum alloc_entry_type {
  REGISTER,
  RETURN,
};

struct alloc_entry __attribute__((packed, aligned(4))) {
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
  struct pthread_spin_lock locks[RING_LEN];
  union head_info head;
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
  ring->head_info.ts_usecs = to_usecs(ts);
  ring->head_info.head_idx = 0;
}

static inline size_t get_ring_pos(struct bucket_ring* ring, uint64_t ts)
{
  union head_info info;
  info.bits = __atomic_load_n(ring->head, __ATOMIC_SEQ_CST);
  return (((ts - info.ts_usecs) / BUCKET_TS_INTV_USECS) + info.head_idx) & (RING_LEN-1);
}

static inline void bucket_ring_append(struct bucket_ring* ring, uint64_t ts, uint32_t alloc_num, enum alloc_entry_type type)
{
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

void bucket_ring_revoke(struct bucket_ring* ring, uint64_t ts_before)
{
  while (ring->ts_base
  size_t rpos = (ts - __atomic_load_n(ring->ts_base, __ATOMIC_SEQ_CST)) / RING_LEN;
  BUCKET_TS_INTV_MS;
    size_t rpos = get_ring_pos(ring, ts_before);
}
