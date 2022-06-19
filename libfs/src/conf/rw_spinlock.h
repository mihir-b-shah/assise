
#ifndef _RW_SPINLOCK_H_
#define _RW_SPINLOCK_H_

#include <pthread.h>

struct rw_spinlock {
  pthread_rwlock_t lock;
};

#define RW_SPINLOCK_INITIALIZER {.lock = PTHREAD_RWLOCK_INITIALIZER}

static inline void rw_spinlock_wr_lock(struct rw_spinlock* lock)
{
  pthread_rwlock_wrlock(&(lock->lock));
}

static inline void rw_spinlock_wr_unlock(struct rw_spinlock* lock)
{
  pthread_rwlock_unlock(&(lock->lock));
}

static inline void rw_spinlock_rd_lock(struct rw_spinlock* lock)
{
  pthread_rwlock_rdlock(&(lock->lock));
}

static inline void rw_spinlock_rd_unlock(struct rw_spinlock* lock)
{
  pthread_rwlock_unlock(&(lock->lock));
}

#endif
