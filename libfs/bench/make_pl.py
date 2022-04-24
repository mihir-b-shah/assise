
import math
import random

MED = 24000
INTG = 38398.91
OFFS = 0.06
LIM = 240000

# unused, just for reference
def dist_pdf(x):
  return (math.exp(-x/MED)+OFFS)/INTG

def dist_cdf(x):
  return (MED*(1-math.exp(-x/MED))+OFFS*x)/INTG

def make_rand():
  r = random.random()
  # binary search.
  l = 0
  h = LIM

  # 25 iterations- 18 to get to an int, and 7 more for safety?
  for i in range(25):
    assert(l < h)
    m = (l+h)/2
    dm = dist_cdf(m)
    if dm < r:
      l = m
    elif dm > r:
      h = m
    else:
      break

  return int(m)

for i in range(100000):
  print(make_rand())
