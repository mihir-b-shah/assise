
#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <string.h>
#include <stdint.h>
#include "utils.h" // rdma
#include <global/global.h>

// this HAS to be a power of 2, not just for bitwise AND, but for wrap-around design.
#define RING_BUF_SIZE 128
#define MAX_PRODUCERS 4

struct ring_buf_entry {
  uint8_t ridx;
  uint32_t seqn;
  uintptr_t raddr;
  uint32_t blknums[g_max_sge];
};

/*
 * Constraints:
 * 1) Never empty when poll() is called.
 * 2) It is MPSC.
 * 3) Perf doesn't really matter - but a good learning experience.
 */
struct __attribute__((packed, aligned(8))) ring_buf {
  volatile uint32_t head;
  volatile uint32_t tail;
  volatile struct ring_buf_entry buf[RING_BUF_SIZE];
};

static inline void ring_buf_init(struct ring_buf* rb)
{
  rb->head = 0;
  rb->tail = 0; 
}

#define RING_SUB(x,y) ((x)>(y)?((x)-(y)):((x)+(1ULL<<32)-(y)))

static inline void ring_buf_push(struct ring_buf* rb, uint8_t ridx, uintptr_t raddr, uint32_t seqn, uint32_t* blknums)
{ 
  while (1) {
    // rely on aligned, packed, and no member-reordering properties
    // THIS READS THE TAIL TOO
    uint64_t snapshot = __atomic_load_n((uint64_t*) &(rb->head), __ATOMIC_SEQ_CST);
    // little endian.
    uint64_t snap_tail = snapshot >> 32;
    uint64_t snap_head = snapshot & 0xffffffffULL;

    if (RING_SUB(snap_tail, snap_head) < RING_BUF_SIZE - MAX_PRODUCERS + 1) {
      break;
    }

    ibw_cpu_relax();
  }

  size_t pos = __atomic_fetch_add(&(rb->tail), 1, __ATOMIC_SEQ_CST);
  pos &= RING_BUF_SIZE-1;

  rb->buf[pos].ridx = ridx;
  rb->buf[pos].raddr = raddr;
  rb->buf[pos].seqn = seqn;

  memcpy(blknums, (uint32_t*) &(rb->buf[pos].blknums), sizeof(uint32_t) * g_max_sge);
}

static inline struct ring_buf_entry ring_buf_poll(struct ring_buf* rb)
{
  size_t pos = __atomic_fetch_add(&(rb->head), 1, __ATOMIC_SEQ_CST);
  return rb->buf[pos & (RING_BUF_SIZE-1)];
}

#endif
