
#include "cache.h"
#include <filesystem/fs.h>
#include "remote_index.h"
#include <conf/conf.h>
#include <global/global.h>
#include <global/util.h>
#include <storage/storage.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "common.h" // rdma
#include "verbs.h" // rdma
#include <distributed/peer.h>

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
  uint8_t* dst;
};

// visible to application, deliberately.
volatile int res_rcache_VISIBLE = -1;
volatile int id_rcache_VISIBLE = -1;
volatile uint32_t offs_rcache_VISIBLE = 0;

static uint64_t* map_base;

#define MR_RCACHE 0

void init_fetcher_helper()
{
  map_base = map_rindex();
}

enum fetch_res fetch_remote(struct rcache_req* req)
{
	uint64_t start_tsc = asm_rdtscp();

  /* 
   * This is a 64-bit atomic read
   * However, this is NOT synchronized with flow control- 
   * We could get the "time to release resource" message from the cache, and then
   *  mark the entry in the cache_index as untaken (ridx=0, raddr=0)
   *  Meanwhile, libfs launches a read before marked. Simultaneously, I ACK the request.
   *  Now, the cache is free to kill my data when it gets my message, and my read reads trash.
   *
   *  How to fix this? Kernfs will agree that invariant time_of_set_index < time_of_send_ack. Then,
   *  before returning FULL_SENT, just check the index again. If the value is still valid, it is
   *  guaranteed that kernfs has NOT sent an ACK yet, and our read was safe. Else (very low probability),
   *  out of caution, trash the read and use the SSD.
   *
   *  How to guarantee the time guarantee?
   *  https://blog.the-pans.com/std-atomic-from-bottom-up/
   *
   *  I think an sfence is sufficient, and forcing a read to happen for the second check
   *  (otherwise compiler may use the old read).
   *  note, mfence does the globally-available property.
   */

  struct rindex_entry entry = read_rindex_entry(&map_base[req->block]);
  // TODO: fix for more chunks
  if (!entry._info_valid) {
    // miss
    g_perf_stats.rcache_miss_tsc += (asm_rdtscp()-start_tsc);
    g_perf_stats.rcache_miss++;
    res_rcache_VISIBLE = NONE_SENT;
    return NONE_SENT;
  }

  struct conn_ctx* nodes = update_cache_conf();
  if (nodes->n == 0) {
    return NONE_SENT;
  }
  
  struct conn_obj* dst_node = &(nodes->conn_ring[entry.ridx[0]]);
  int sockfd = dst_node->sockfd;
  
  rdma_meta_t* meta = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
  meta->addr = entry.raddr[0];
  meta->length = g_block_size_bytes;
  meta->sge_count = 1;
  meta->next = NULL;
  meta->sge_entries[0].addr = req->dst;
  meta->sge_entries[0].length = g_block_size_bytes;

  uint32_t wr_id = IBV_WRAPPER_READ_ASYNC(sockfd, meta, MR_DRAM_CACHE, MR_RCACHE);
  MP_AWAIT_WORK_COMPLETION(sockfd, wr_id);

  ++id_rcache_VISIBLE;
  offs_rcache_VISIBLE = req->offset;
    
  g_perf_stats.rcache_hit_tsc += (asm_rdtscp()-start_tsc);
  g_perf_stats.rcache_hit++;
  res_rcache_VISIBLE = FULL_SENT;
  return FULL_SENT;
}
