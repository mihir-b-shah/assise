
'''
1. first, the remote cache is infinite capacity in this experiment.
as such, everything is accessible at the end. then, we can work backwards.


'''

offset_map = {}
replay = []
ttl = 0

with open("query_log.txt") as f:
  for i, line in enumerate(f):
    vals = line.split(' ')
    if not (vals[0] in offset_map):
      offset_map[vals[0]] = []
    v1 = int(vals[1].rstrip('\n'))
    offset_map[vals[0]].append((i, v1))
    replay.append((vals[0], v1))

  for key, rcache_ev in replay:
    if key in offset_map and rcache_ev == 2:
      ttl += len(offset_map[key])
      del offset_map[key]
    print(ttl)
