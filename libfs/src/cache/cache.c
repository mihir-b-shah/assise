
#include "cache.h"

#include "filesystem/ssd_emulation.h"
#include "io/block_io.h"
#include "storage/storage.h"
#include "global/global.h"
#include "global/util.h"

/*
 * We want to emulate resolving the ssd and remote operations in parallel.
 * Traditionally, this is kind of tricky and could probably require some
 * non-blocking IO or threading. However, since we are EMULATING the ssd
 * latency, we can choose where to place it. Furthermore, main memory
 * access in the worst case (which is effectively what POSIX shared memory is),
 * is around ~200 ns. According to Assise paper, a round trip in the network is
 * around 3 us- roughly an order of magnitude slower. Thus, applying the ssd
 * latency intelligently is a good approximation to this parallel resolution.
 *
 * How to handle parallel resolution- i.e. when to fill our cache with the "ssd" data.
 * The simplest approach, rather than a "resolution thread", is to maintain a queue
 * of updates, with a clock_t. How do we actually do the fill? Do a fcache_find to see
 * if that data still exists in the cache (remember, eviction could have happened after
 * we returned). Beforehand, acquire a read latch. fcache_find already will acquire it, 
 * but reentrancy is not an issue here, since we can acquire arbitrary numbers of read
 * latches. 
 *
 * Note, I think (not sure though) that Assise has a bug, where it sends the part we want as a perf opt for latency,
 * and then the whole thing. But the fcache thinks it has the whole thing, for the first part.
 * This can get problematic if a new read happens, before the rest gets here. I want to extend
 * this optimization, as I discussed in the design doc.
 */ 

#define FULL_SENT 2
#define PART_SENT 1
#define NONE_SENT 0

static int fetch_remote(struct rcache_req* req)
{
  return NONE_SENT;
}

static int decide_ask_remote(struct rcache_req* req)
{
  return 0;
}

/*
 * 1. Decide whether to ask the remote node.
 * 2. If so, make the request, and if it succeeds, then enqueue
 */
void rcache_read(struct rcache_req req)
{
  printf("Hit rcache_read. %s:%d\n", __FILE__, __LINE__);

  clock_t start = clock();

  int ask = decide_ask_remote(&req);
  if (ask) {
    // perform remote cache read
    int code = fetch_remote(&req);

    switch (code) {
    case NONE_SENT: break;
    case FULL_SENT: return;
    case PART_SENT:
      enq_ssd_req(req);
      break;
    }
  }

  printf("Hit emul_ssd_read. %s:%d\n", __FILE__, __LINE__);
  emul_ssd_read(&req);
  
  printf("Hit ssd_emul_latency. %s:%d\n", __FILE__, __LINE__);
  // ssd_emul_latency(start);
}

