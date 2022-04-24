
#ifndef _PL_RAND_H_
#define _PL_RAND_H_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#define MED 24000
#define INTG 38398.91
#define OFFS 0.06
#define LIM 240000

// unused, just for reference
double dist_pdf(double x)
{
  return (exp(-x/MED)+OFFS)/INTG;
}

double dist_cdf(double x)
{
  return (MED*(1-exp(-x/MED))+OFFS*x)/INTG;
}

uint32_t pl_rand()
{
  double r = ((double) rand())/RAND_MAX;
  double l = 0;
  double h = LIM;
  double m;

  // 25 iterations- 18 to get to an int, and 7 more for safety?
  for (int i = 0; i<25; ++i) {
    assert(l < h);
    m = (l+h)/2;
    double dm = dist_cdf(m);
    if (dm < r) {
      l = m;
    } else if (dm > r) {
      h = m;
    } else {
      break;
    }
  }

  return (uint32_t) m;
}

#endif
