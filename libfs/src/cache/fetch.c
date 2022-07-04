
#include "cache.h"
#include <filesystem/fs.h>
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

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
  uint8_t* dst;
};

static volatile uint32_t glob_seqn = 2;

#define SEQN_MASK 0x0fffffffUL
#define PRESENT_MASK 0x80000000UL

// visible to application, deliberately.
volatile int res_rcache_VISIBLE = -1;
volatile int id_rcache_VISIBLE = -1;
volatile uint32_t offs_rcache_VISIBLE = 0;

static int shm_fd;
static const char* shm_path = "cache_index";
static uint64_t* map_base;

void init_fetcher_helper()
{
  shm_fd = shm_open(shm_path, O_RDWR, ALLPERMS);
	if (shm_fd < 0) {
		fprintf(stderr, "cannot open cache index %s\n", shm_path);
		exit(-1);
	}
  map_base = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (map_base == MAP_FAILED) {
		perror("cannot map cache_index file");
		exit(-1);
	}
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

  struct conn_obj* dst_node = update_cache_conf()->conn_ring[req];
  if (dst_node == NULL) {
    return NONE_SENT;
  }

  int sockfd = dst_node->sockfd;

  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  uint32_t seqn = glob_seqn;   // should only have one oustanding request at a time- we wait immediately.
  
  glob_seqn = (glob_seqn + 2) & SEQN_MASK;

  struct client_req body = {.node_num = ip_int, .block_num = req->block, .dst = req->dst};
  
  app->id = seqn;
  app->data[0] = 'R';
  memcpy(1+app->data, &body, sizeof(body));

  MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
  
  uint32_t imm = MP_AWAIT_RESPONSE_MASK(sockfd, seqn, SEQN_MASK);

  ++id_rcache_VISIBLE;
  offs_rcache_VISIBLE = req->offset;

  if (imm & PRESENT_MASK) {
    g_perf_stats.rcache_hit_tsc += (asm_rdtscp()-start_tsc);
    g_perf_stats.rcache_hit++;
    res_rcache_VISIBLE = FULL_SENT;
    return FULL_SENT;
  } else {
    g_perf_stats.rcache_miss_tsc += (asm_rdtscp()-start_tsc);
    g_perf_stats.rcache_miss++;
    res_rcache_VISIBLE = NONE_SENT;
    return NONE_SENT;
  }

  /*
  uint32_t imm = MP_AWAIT_RESPONSE_MASK(sockfd, seqn, 0x3);

  uint32_t result_type = (imm & 0xc) >> 2;
  uint32_t start_offs = (imm & 0xfff0) >> 4;
  uint32_t end_offs = (imm & 0xfff0000) >> 16;

  printf("Received result_type: %u, s: %u, e: %u\n", result_type, start_offs, end_offs);
  */
}
