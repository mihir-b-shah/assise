
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
#define MAX_PRODUCERS 4

struct ring_buf_entry {
  uint8_t ridx;
  uint32_t seqn;
  uintptr_t raddr;
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

static void ring_buf_push(struct ring_buf* rb, uint8_t ridx, uintptr_t raddr, uint32_t seqn)
{ 
  while (1) {
    // rely on aligned, packed, and no member-reordering properties
    uint64_t snapshot = __atomic_load_n(&(rb->snapshot), __ATOMIC_SEQ_CST);
    // little endian.
    uint64_t snap_head = snapshot >> 32;
    uint64_t snap_tail = snapshot & 0xffffffffULL;

    if (RING_SUB(snap_tail, snap_head) < RING_BUF_SIZE - MAX_PRODUCERS + 1) {
      break;
    }

    asm volatile("pause\n": : :"memory");
  }

  size_t pos = __atomic_fetch_add(&(rb->tail), 1, __ATOMIC_SEQ_CST);
  pos &= RING_BUF_SIZE-1;

  rb->buf[pos].ridx = ridx;
  rb->buf[pos].raddr = raddr;
  rb->buf[pos].seqn = seqn;
}

static struct ring_buf_entry ring_buf_poll(struct ring_buf* rb)
{
  struct ring_buf_entry ret = rb->buf[__atomic_load_n(&(rb->head), __ATOMIC_SEQ_CST) & (RING_BUF_SIZE-1)];
  __atomic_add_fetch(&(rb->head), 1, __ATOMIC_SEQ_CST);
  return ret;
}

static size_t op_ctr;
static struct ring_buf rb;
static uint32_t incr = 0;

#define LIM 100000

static void* producer(void* arg)
{
  for (uint32_t i = 0; i<LIM; ++i) {
    ring_buf_push(&rb, 0, 0, __atomic_fetch_add(&incr, 1, __ATOMIC_SEQ_CST));
    __atomic_add_fetch(&op_ctr, 1, __ATOMIC_SEQ_CST);
  }
}

// single thread...
static void* consumer(void* arg)
{
  uint64_t sum = 0;
  for (uint32_t i = 0; i<4*LIM; ) {
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

  pthread_t prod1, prod2, prod3, prod4, cons;
  pthread_create(&prod1, NULL, producer, NULL);
  pthread_create(&prod2, NULL, producer, NULL);
  pthread_create(&prod3, NULL, producer, NULL);
  pthread_create(&prod4, NULL, producer, NULL);
  pthread_create(&cons, NULL, consumer, NULL);

  while (1) {
    sleep(1); 
  }
}
