
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
  uint32_t pos = __atomic_fetch_add(&(rb->tail), 1, __ATOMIC_SEQ_CST);
  // wait until my position is available.
  
  while (1) {
    if (RING_SUB(pos, __atomic_load_n(&(rb->head), __ATOMIC_SEQ_CST)) < RING_BUF_SIZE) {
      break;
    }

    asm volatile("pause\n": : :"memory");
  }

  pos &= RING_BUF_SIZE-1;
  rb->buf[pos].seqn = seqn;
}

static struct ring_buf_entry ring_buf_poll(struct ring_buf* rb)
{
  uint32_t loc_head = __atomic_load_n(&(rb->head), __ATOMIC_SEQ_CST);
  struct ring_buf_entry ret = rb->buf[loc_head & (RING_BUF_SIZE-1)];
  __atomic_store_n(&(rb->head), 1+loc_head, __ATOMIC_SEQ_CST);
  return ret;
}

static volatile size_t op_ctr;
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
  uint32_t* nums = malloc(sizeof(uint32_t) * MAX_PRODUCERS * LIM);
  uint32_t* op_cts = malloc(sizeof(uint32_t) * MAX_PRODUCERS * LIM);

  uint64_t sum = 0;
  for (uint32_t i = 0; i<MAX_PRODUCERS*LIM; ) {
    if (__atomic_load_n(&op_ctr, __ATOMIC_SEQ_CST) > 0) {
      struct ring_buf_entry e = ring_buf_poll(&rb);
      sum += e.seqn;
      nums[i] = e.seqn;
      op_cts[i] = __atomic_fetch_add(&op_ctr, -1, __ATOMIC_SEQ_CST);
      ++i;
    }
  }
  
  for (size_t i = 0; i<MAX_PRODUCERS*LIM; ++i) {
    printf("%u %u\n", nums[i], op_cts[i]);
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
