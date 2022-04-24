
import sys
import subprocess
import itertools

'''
argv[1]: n > 0
argv[2]: np
argv[3]: evict_frac < 100
'''

n = int(sys.argv[1])
data = subprocess.check_output(['plg.exe', str(n), sys.argv[2]])

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
evict_frac = int((int(sys.argv[3])/100)*len(reuse_list))

evict_list = reuse_list[len(reuse_list)-evict_frac:]
ret_list = itertools.accumulate(sorted(evict_list, key=lambda p:p[0]), lambda p,q: (max(p[0],q[0]), p[1]+q[1]))
for v1,v2 in ret_list:
    print('%s %s'%(v1, v2/ct))
