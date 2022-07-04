
#include <unistd.h>
#include <conf/conf.h>
#include <global/global.h>
#include <remote_index.h>
#include <io/block_io.h>
#include <io/device.h>
#include <fs.h>
#include <storage/storage.h>
#include <global/util.h>
#include "cache.h"
#include "agent.h"
#include <assert.h>
#include "utils.h" // rdma
#include <pthread.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <ds/khash.h>

#define MR_RCACHE 0

#define ARR_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

static uint32_t blk_queue[g_max_meta * g_max_sge];
static volatile uint32_t blk_queue_pos = 0;
static pthread_spinlock_t lock;
static pthread_spinlock_t map_lock;

struct seqn_state_t {
  uint8_t ridx;
  uintptr_t raddr;
  uint32_t blknums[g_max_sge];
};

static volatile uint32_t seqn = 0;
KHASH_MAP_INIT_INT(seqn_map_t, struct seqn_state_t)
static khash_t(seqn_map_t) *seqn_states;

static int shm_fd;
static const char* shm_path = "cache_index";
static uint64_t* map_base;

void update_index(uint32_t imm)
{
  int present;
  pthread_spin_lock(&map_lock);
  khint_t k = kh_get(seqn_map_t, seqn_states, imm);
  assert(k != kh_end(seqn_states));
  
  struct seqn_state_t e = kh_value(seqn_states, k);
  kh_del(seqn_map_t, seqn_states, k);
  pthread_spin_unlock(&map_lock);

  for (size_t i = 0; i<g_max_sge; ++i) {
    write_rindex_entry(&map_base[e.blknums[i]], e.ridx, (uintptr_t) (e.raddr + g_block_size_bytes * i));
  }
}

void init_cache()
{
  pthread_spin_init(&lock, 0);
  pthread_spin_init(&map_lock, 0);

  shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, ALLPERMS);
	if (shm_fd < 0) {
		fprintf(stderr, "cannot open cache index %s\n", shm_path);
		exit(-1);
	}
  
  size_t shm_size = sizeof(uint64_t) * ((dev_size[g_root_dev] + g_block_size_bytes) >> g_block_size_shift);
  ftruncate(shm_fd, shm_size);
  map_base = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (map_base == MAP_FAILED) {
		perror("cannot map cache_index file");
		exit(-1);
	}

  seqn_states = kh_init(seqn_map_t);
  set_cq_cb_fn(update_index);
}

// notice we deliberately de-queue from the evict list in blocks of 960 (instead of 1024).
void send_to_rcache(uint64_t block)
{
  pthread_spin_lock(&lock);

  blk_queue[blk_queue_pos++] = block;
  if (blk_queue_pos < ARR_SIZE(blk_queue)) {
    pthread_spin_unlock(&lock);
    return;    
  }

  uint64_t stime = asm_rdtscp();

  struct conn_obj* dst_node = get_dest(block);
  if (dst_node == NULL) {
    pthread_spin_unlock(&lock);
    release_rd_lock();
    return;
  }

  volatile void* rblock_addr[g_max_meta] = {NULL};
  rdma_meta_t* metas[1+g_max_meta] = {NULL};

  for (size_t i = 0; i<g_max_meta; ++i) {
    //printf("Reading from %p\n", &(dst_node->rblock_addr[i]));
    uint64_t ctr = 0;
    while ((rblock_addr[i] = __atomic_exchange_n(
      &(dst_node->rblock_addr[i]), NULL, __ATOMIC_SEQ_CST)) == NULL) {
      ++ctr;
      ibw_cpu_relax();
    }
    g_perf_stats.n_send_wait_m2[i] += ctr*ctr;
    g_perf_stats.n_send_wait[i] += ctr;
    g_perf_stats.n_sends[i]++;
    metas[i] = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + g_max_sge * sizeof(struct ibv_sge));
  }

  for (size_t i = 0; i<g_max_meta; ++i) {
    uint32_t my_seqn = __atomic_add_fetch(&seqn, 1, __ATOMIC_SEQ_CST);

    metas[i]->addr = (uintptr_t) rblock_addr[i];
    metas[i]->imm = my_seqn;
    metas[i]->length = g_block_size_bytes * g_max_sge;
    metas[i]->sge_count = g_max_sge;
    metas[i]->next = metas[1+i];

    for (size_t j = 0; j<g_max_sge; ++j) {
      uint64_t blknum = blk_queue[j + i * g_max_sge];
      metas[i]->sge_entries[j].addr = (uintptr_t) g_bdev[g_root_dev]->storage_engine
        ->get_addr(g_root_dev, blknum);
      metas[i]->sge_entries[j].length = g_block_size_bytes;
    }
    
    // pass in offset in cache node array, not the sockfd (utterly worthless here)
    int absent;
    pthread_spin_lock(&map_lock);
    khint_t k = kh_put(seqn_map_t, seqn_states, my_seqn, &absent);
    kh_value(seqn_states, k).ridx = dst_node->idx;
    kh_value(seqn_states, k).raddr = metas[i]->addr;
    for (size_t j = 0; j<g_max_sge; ++j) {
      kh_value(seqn_states, k).blknums[j] = blk_queue[i * g_max_sge + j];
    }
    pthread_spin_unlock(&map_lock);
  }

  IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(dst_node->sockfd, metas[0], MR_NVM_SHARED, MR_RCACHE);

  // TODO: modify the callback in kernfs/fs.c to make sure we ignore the ACK
	g_perf_stats.rcache_send_tsc += asm_rdtscp() - stime;

  pthread_spin_unlock(&lock);
  //release_rd_lock();
  blk_queue_pos = 0;
}
