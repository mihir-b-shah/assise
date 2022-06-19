
#include <pthread.h>
#include <assert.h>
#include "rw_spinlock.h"

volatile int x = 0;
volatile int y = 0;
volatile int z = 0;
int sum = 0;
struct rw_spinlock lock = RW_SPINLOCK_INITIALIZER;

void* reader1(void* arg)
{
  rw_spinlock_rd_lock(&lock);
  sum += x+y+z;
  rw_spinlock_rd_unlock(&lock);
}

void* reader2(void* arg)
{
  rw_spinlock_rd_lock(&lock);
  sum += x+y+z;
  rw_spinlock_rd_unlock(&lock);
}

int main()
{
  pthread_t r1,r2;
  pthread_create(&r1, NULL, reader1, NULL);
  pthread_create(&r2, NULL, reader2, NULL);
  int ctr;

  while (1) {
    rw_spinlock_wr_lock(&lock);

    x += 1;
    
    ctr = 0;
    while (ctr < 1000) { ++ctr; }

    y += 1;
    
    ctr = 0;
    while (ctr < 1000) { ++ctr; }

    z += 1;
    
    ctr = 0;
    while (ctr < 1000) { ++ctr; }

    rw_spinlock_wr_unlock(&lock);

    assert(sum % 3 == 0);
  }

  return 0;
}
