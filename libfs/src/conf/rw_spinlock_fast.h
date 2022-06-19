
#ifndef _RW_SPINLOCK_H_
#define _RW_SPINLOCK_H_

// Taken from http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.156.7879&rep=rep1&type=pdf

#include <stdint.h>
#include "utils.h" // rdma

struct rw_spinlock {
  volatile uint32_t val;
};

#define RW_SPINLOCK_INITIALIZER {.val = 0}
#define RW_SPINLOCK_WFLAG 0x1
#define RW_SPINLOCK_RINCR 0x2

static inline void rw_spinlock_wr_lock(struct rw_spinlock* lock)
{
  uint32_t exp = 0;
  while (__atomic_compare_exchange_n(&(lock->val), &exp, RW_SPINLOCK_WFLAG, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    ibw_cpu_relax();
  }
}

static inline void rw_spinlock_wr_unlock(struct rw_spinlock* lock)
{
  __atomic_add_fetch(&(lock->val), -RW_SPINLOCK_WFLAG, __ATOMIC_SEQ_CST);
}

static inline void rw_spinlock_rd_lock(struct rw_spinlock* lock)
{
  __atomic_add_fetch(&(lock->val), RW_SPINLOCK_RINCR, __ATOMIC_SEQ_CST);
  while (__atomic_load_n(&(lock->val), __ATOMIC_SEQ_CST) & RW_SPINLOCK_WFLAG) {
    ibw_cpu_relax();
  }
}

static inline void rw_spinlock_rd_unlock(struct rw_spinlock* lock)
{
  __atomic_add_fetch(&(lock->val), -RW_SPINLOCK_RINCR, __ATOMIC_SEQ_CST);
}

#endif
