
#ifndef _RING_BUF_H_
#define _RING_BUF_H_

#include <string.h>
#include <stdint.h>
#include "utils.h" // rdma
#include <pthread.h>
#include <global/global.h>

#define RING_BUF_SIZE 128

struct ring_buf_entry {
  uint8_t ridx;
  uint32_t seqn;
  uintptr_t raddr;
  uint32_t blknums[g_max_sge];
};

/*
 * Constraints:
 * 1) Never empty when poll() is called.
 * 2) It is MPMC- why not...
 * 3) Performance of this doesn't really matter- it is called per blocking call to 
 *    wakeup on CQ event, and every time we write 400 kB over the network. A little 
 *    latency in the ring buffer is nothing.
 *      -> just acquire a mutex -> no concurrency at all.
 */
struct ring_buf {
  volatile size_t head;
  volatile size_t tail;
  volatile size_t n;
  volatile struct ring_buf_entry buf[RING_BUF_SIZE];
  /* network round trips are less than 100 us, waiting for a Linux scheduling cycle by using
     a blocking lock (milliseconds) is not great. Plus we are on a high core-count processor. */
  pthread_spinlock_t lock;
};

static inline void ring_buf_init(struct ring_buf* rb)
{
  rb->head = 0;
  rb->tail = 0; 
  rb->n = 0;
  pthread_spin_init(&(rb->lock), 0);
}

static inline void ring_buf_push(struct ring_buf* rb, uint8_t ridx, uintptr_t raddr, uint32_t seqn, uint32_t* blknums)
{ 
  pthread_spin_lock(&(rb->lock));
  while (__atomic_load_n(&(rb->n), __ATOMIC_SEQ_CST) == RING_BUF_SIZE) {
    pthread_spin_unlock(&(rb->lock));
    ibw_cpu_relax();
    pthread_spin_lock(&(rb->lock));
  }
  size_t pos = rb->tail++ & (RING_BUF_SIZE-1);
  __atomic_add_fetch(&(rb->n), 1, __ATOMIC_SEQ_CST);
  pthread_spin_unlock(&(rb->lock));

  rb->buf[pos].ridx = ridx;
  rb->buf[pos].raddr = raddr;
  rb->buf[pos].seqn = seqn;

  memcpy(blknums, (uint32_t*) &(rb->buf[pos].blknums), sizeof(uint32_t) * g_max_sge);
}

static inline struct ring_buf_entry ring_buf_poll(struct ring_buf* rb)
{
  pthread_spin_lock(&(rb->lock));
  struct ring_buf_entry entry = rb->buf[rb->head];
  rb->head++;
  __atomic_add_fetch(&(rb->n), -1, __ATOMIC_SEQ_CST);
  pthread_spin_unlock(&(rb->lock));
  return entry;
}

#endif
