
#include <stdint.h>
#include <stdio.h>
#include <x86intrin.h>
#include <time.h>
  
#define WAIT_AMT (2500 * CLOCKS_PER_SEC / 100000)

int main()
{
  uint64_t start  = __rdtsc();
  while(__rdtsc() - start < WAIT_AMT);
  return 0;
}
