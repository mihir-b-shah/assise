
import sys
import subprocess
import itertools

'''
argv[1]: n > 0
argv[2]: rand < 100
argv[3]: skew < 100
argv[4]: evict_frac < 100

'''

n = int(sys.argv[1])
data = subprocess.check_output(['plg.exe', str(n), sys.argv[2], sys.argv[3]])

by_key = {}
ct = 0
for i, line in enumerate(data.decode('utf-8').splitlines()):
  if not (line in by_key):
    by_key[line] = []
  by_key[line].append(i)
  ct += 1

reuse_dists = {}
for k,arr_raw in by_key.items():
  arr = list(reversed(arr_raw+[ct]))
  sm = 0
  powf = 1.1
  mult = 1
  for i in range(len(arr)-1):
    sm += mult*abs(arr[i+1]-arr[i])
    mult /= powf
  sm *= (1-(1/powf))/(1-mult)
  reuse_dists[k] = (sm, len(arr))

'''
considering a sorted prefix of these values, is essentially lfu instead of lru
but in the macro case, they are the same. The primary difference of lru vs lfu is
when drift happens- i.e. a once-popular value hangs around in lfu, but lru kills it.
But our workload has no drift- its randomly generated.
'''

reuse_list = sorted(reuse_dists.values(), key=lambda p: p[0])
evict_frac = int((int(sys.argv[4])/100)*len(reuse_list))

ret_list = itertools.accumulate(sorted(map(lambda p: p[0], reuse_list[len(reuse_list)-evict_frac:])), lambda p,q: max(p,q))
for v in ret_list:
    print(v)

#frac = sum(map(lambda p: p[1], reuse_list[len(reuse_list)-evict_frac:]))
#ttl = sum(map(lambda p: p[1], reuse_list[0:]))
