
#include <unistd.h>
#include <conf/conf.h>
#include <global/global.h>
#include <io/block_io.h>
#include <io/device.h>
#include <fs.h>
#include <storage/storage.h>
#include <global/util.h>
#include "cache.h"
#include "agent.h"
#include <assert.h>
#include <pthread.h>
#include "utils.h" // rdma

#define MR_RCACHE 0

#define ARR_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

uint64_t blk_queue[g_max_meta * g_max_sge];
size_t blk_queue_pos = 0;

// notice we deliberately de-queue from the evict list in blocks of 960 (instead of 1024).
void send_to_rcache(uint64_t block)
{
  // lets make life simple here- just acquire the mutex, if its slow we'll fix it.
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  pthread_mutex_lock(&mutex);

  blk_queue[blk_queue_pos++] = block;
  if (blk_queue_pos < ARR_SIZE(blk_queue)) {
    pthread_mutex_unlock(&mutex);
    return;    
  }
  assert(blk_queue_pos == ARR_SIZE(blk_queue));
  blk_queue_pos = 0;

  uint64_t stime = asm_rdtscp();

  struct conn_obj* dst_node = get_dest(block);
  if (dst_node == NULL) {
    pthread_mutex_unlock(&mutex);
    return;
  }

  void* rblock_addr[g_max_meta] = {NULL};
  rdma_meta_t* metas[1+g_max_meta] = {NULL};

  for (size_t i = 0; i<g_max_meta; ++i) {
    while ((rblock_addr[i] = __atomic_exchange_n(
      &(dst_node->rblock_addr[i]), NULL, __ATOMIC_SEQ_CST)) == NULL) {
      ibw_cpu_relax();
    }
    metas[i] = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + g_max_sge * sizeof(struct ibv_sge));
  }

  for (size_t i = 0; i<g_max_meta; ++i) {
    metas[i]->addr = (uintptr_t) rblock_addr[i];
    metas[i]->imm = ip_int;
    metas[i]->length = g_block_size_bytes * g_max_sge;
    metas[i]->sge_count = g_max_sge;
    metas[i]->next = metas[1+i];

    for (size_t j = 0; j<g_max_sge; ++j) {
      metas[i]->sge_entries[j].addr = (uintptr_t) g_bdev[g_root_dev]->storage_engine
        ->get_addr(g_root_dev, blk_queue[j + i * g_max_sge]);
      metas[i]->sge_entries[j].length = g_block_size_bytes;
    }
  }

  IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(dst_node->sockfd, metas[0], MR_NVM_SHARED, MR_RCACHE);

  // TODO: modify the callback in kernfs/fs.c to make sure we ignore the ACK
	g_perf_stats.rcache_send_tsc += asm_rdtscp() - stime;
  pthread_mutex_unlock(&mutex);
}
