
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

static uint64_t x = 0;

static void* setter(void* arg)
{
  int ctr = 0;
  for (size_t i = 0; i<UINT64_MAX; ++i) {
    __atomic_store_n(&x, i, __ATOMIC_SEQ_CST);
  }
  return NULL;
}

int main()
{
  srand(time(NULL));

  pthread_t thr;
  pthread_create(&thr, NULL, setter, NULL);

  uint64_t v = 0;
  while (1) {
    while ((v = __atomic_exchange_n(&x, NULL, __ATOMIC_SEQ_CST)) == 0);
    printf("Received %d\n", v);
  }

  void* ret;
  pthread_join(thr, &ret);

  return 0;
}
