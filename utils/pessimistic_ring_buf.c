
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// this HAS to be a power of 2, not just for bitwise AND, but for wrap-around design.
#define RING_BUF_SIZE 128
#define MAX_PRODUCERS 2

struct ring_buf_entry {
  uint32_t seqn;
};

/*
 * Constraints:
 * 1) Never empty when poll() is called.
 * 2) It is MPSC.
 */
struct __attribute__((packed, aligned(8))) ring_buf {
  union {
    struct {
      volatile uint32_t tail;
      volatile uint32_t head;
    };
    volatile uint64_t snapshot;
  };
  volatile struct ring_buf_entry buf[RING_BUF_SIZE];
};

static void ring_buf_init(struct ring_buf* rb)
{
  rb->tail = 0; 
  rb->head = 0;
}

#define RING_SUB(x,y) ((x)>=(y)?((x)-(y)):((x)+(1ULL<<32)-(y)))

static void ring_buf_push(struct ring_buf* rb, uint32_t seqn)
{
  size_t pos;
   
  while (1) {
    // rely on aligned, packed, and no member-reordering properties
    uint64_t snapshot = __atomic_load_n(&(rb->snapshot), __ATOMIC_SEQ_CST);
    // little endian.
    uint64_t snap_head = snapshot >> 32;
    uint64_t snap_tail = snapshot & 0xffffffffULL;

    if (RING_SUB(snap_tail, snap_head) < RING_BUF_SIZE - MAX_PRODUCERS + 1) {
      uint32_t exp = snap_tail;
      if (__atomic_compare_exchange_n(&(rb->tail), &exp, snap_tail+1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        pos = snap_tail;
        break;
      }
    }

    asm volatile("pause\n": : :"memory");
  }

  pos &= RING_BUF_SIZE-1;

  rb->buf[pos].seqn = seqn;
    
  asm volatile("sfence\n": : :"memory");
}

static struct ring_buf_entry ring_buf_poll(struct ring_buf* rb)
{
  struct ring_buf_entry ret = rb->buf[__atomic_load_n(&(rb->head), __ATOMIC_SEQ_CST) & (RING_BUF_SIZE-1)];
  __atomic_add_fetch(&(rb->head), 1, __ATOMIC_SEQ_CST);
  return ret;
}

static size_t op_ctr;
static struct ring_buf rb;

#define LIM 100000

static void* producer(void* arg)
{
  for (uint32_t i = 0; i<LIM; ++i) {
    ring_buf_push(&rb, i);
    __atomic_add_fetch(&op_ctr, 1, __ATOMIC_SEQ_CST);
  }
}

// single thread...
static void* consumer(void* arg)
{
  uint64_t sum = 0;
  for (uint32_t i = 0; i<MAX_PRODUCERS*LIM; ) {
    if (__atomic_load_n(&op_ctr, __ATOMIC_SEQ_CST) > 0) {
      struct ring_buf_entry e = ring_buf_poll(&rb);
      sum += e.seqn;
      __atomic_add_fetch(&op_ctr, -1, __ATOMIC_SEQ_CST);
      ++i;
    }
  }
  printf("Sum: %lu\n", sum);
  exit(0);
}

int main() {
  ring_buf_init(&rb);

  pthread_t prods[MAX_PRODUCERS];
  pthread_t cons;

  for (size_t i = 0; i<MAX_PRODUCERS; ++i) {
    pthread_create(&prods[i], NULL, producer, NULL);
  }
  pthread_create(&cons, NULL, consumer, NULL);

  while (1) {
    sleep(1); 
  }
}
