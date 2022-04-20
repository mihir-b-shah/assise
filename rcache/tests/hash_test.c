
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
uint64_t hash(uint64_t x)
{
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x = x ^ (x >> 31);
  return x;
}

// our node identifiers.
static uint64_t biases[5];

uint64_t rand64()
{
  size_t idx = rand() % 5;
  return (biases[idx] << 32) | rand();
}

int main(int argc, char** argv)
{
  uint32_t high_bias = rand() & 0xffff0000;
  for (int i = 0; i<5; ++i) {
    biases[i] = high_bias | (rand() & 0xffff);
  }

  int n_rands = atoi(argv[1]);
  int n_buckets = atoi(argv[2]);
  uint64_t vals_per = ((1ULL << 63) / n_buckets) * 2;

  // no need to free..
  uint64_t* buckets = calloc(n_buckets, sizeof(uint64_t));
  uint64_t* raw_buckets = calloc(n_buckets, sizeof(uint64_t));

  for (int i = 0; i<n_rands; ++i) {
    uint64_t v = rand64();
    uint64_t h = hash(v);
    ++raw_buckets[v / vals_per];
    ++buckets[h / vals_per];
  }

  for (int i = 0; i<n_buckets; ++i) {
    printf("%lu ", raw_buckets[i]);
  }
  printf("\n");
  for (int i = 0; i<n_buckets; ++i) {
    printf("%lu ", buckets[i]);
  }
  printf("\n");
  
  return 0;
}
