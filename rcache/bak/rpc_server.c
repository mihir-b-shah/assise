
#include <pthread.h>
#include "rcache.h"

#include <time.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "agent.h"
#include "conf_client.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>

volatile sig_atomic_t stop;

void inthand(int signum)
{	
	stop = 1;
}

uint8_t* base = NULL;
size_t MEM_SIZE = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
  uint8_t* dst;
};

struct send_req {
  uint32_t node_num;
  uint64_t block_num;
};

static uint64_t make_block_num(uint32_t node, uint64_t block)
{
  assert(block >> 32 == 0); // makes sense- 32 bit block number allows >8 TB of memory.
  return ((uint64_t) node << 32) | block;
}

#define MR_RCACHE 0
#define MR_DRAM_CACHE 2

static uint64_t hit_ctr = 0;
static uint64_t miss_ctr = 0;
static uint64_t w_ctr = 0;

void handle_segfault(int sig)
{
  char **strings;
  size_t i, size;
  enum Constexpr { MAX_SIZE = 1024 };
  void *array[MAX_SIZE];
  size = backtrace(array, MAX_SIZE);
  strings = backtrace_symbols(array, size);
  for (i = 0; i < size; i++) {
    printf("%s\n", strings[i]);
  }
  puts("");
  free(strings);
  abort();
}

void signal_callback(struct app_context *msg)
{
  pthread_mutex_lock(&lock);
  size_t lsize = lru_size();

  if (msg->data[0] == 'R') {
    struct client_req req;
    memcpy(&req, 1+msg->data, sizeof(struct client_req));

    //printf("received from client msg[%d] with the following node_num: %u, blk: %lu, dst: %p\n", msg->id, req.node_num, req.block_num, req.dst);

    /* scatter gather is the sge- allows us to read/write to non-contigious memory locations in one go, into a single 
     * contigious buffer 
     */
    rdma_meta_t *meta = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));

    // base addr to copy from- TODO: pointing at a shared memory region is problematic- pin the block?
    meta->addr = (uintptr_t) req.dst;

    // find our block.
    uint8_t* blk = lru_get_block(make_block_num(req.node_num, req.block_num));
    // set immediate to sequence number and a present bit
    if (blk != NULL) {
      meta->imm = msg->id | 0x80000000UL;
      meta->length = BLK_SIZE;
      ++hit_ctr;
    } else {
      blk = NULL;
      meta->imm = msg->id;
      meta->length = 0;
      ++miss_ctr;
    }

    //printf("sending block %lu to node %lx\n", req.block_num, req.node_num);

    meta->sge_count = 1;
    meta->next = NULL;

    // take advantage of zero length array at end of rdma_meta_t
    meta->sge_entries[0].addr = (uintptr_t) (blk == NULL ? base : blk);
    meta->sge_entries[0].length = blk == NULL ? 0 : BLK_SIZE;

    IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(msg->sockfd, meta, MR_RCACHE, MR_DRAM_CACHE);
    assert(lsize == lru_size());
  } else if (msg->data[0] == 'W') {
    struct send_req req;
    memcpy(&req, 1+msg->data, sizeof(struct send_req));

    uint64_t key = make_block_num(req.node_num, req.block_num);
    if (!lru_get_block(key)) {
      uint8_t* evicted = lru_try_evict();
      uint8_t* blk = evicted != NULL ? evicted : blk_alloc();
      assert(blk != NULL);
      memcpy(blk, 1+msg->data+sizeof(struct send_req), BLK_SIZE);
      
      lru_insert_block(key, blk);

      //printf("received block %lu from node %lx\n", req.block_num, req.node_num);
      ++w_ctr;
      assert(evicted ? lsize == lru_size() : (1+lsize) == lru_size());
    }
  } else {
    printf("Odd message received.\n");
    exit(0);
  }
  pthread_mutex_unlock(&lock);
}

void add_peer_socket(int sockfd) {}
void remove_peer_socket(int sockfd) {}

int main(int argc, char **argv)
{
  signal(SIGSEGV, handle_segfault);

	char *host;
	char *portno;
	struct mr_context *regions;
	int sockfd;
	int iters;
	int sync;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <port> <cache_size>\n", argv[0]);
		return 1;
	}

	portno = argv[1];
  MEM_SIZE = strtoull(argv[2], NULL, 16);
  
  conf_cmd_t ccmd;
  init_cmd(&ccmd);
  start_cache_client(&ccmd);

  int fd = shm_open("rcache", O_CREAT | O_RDWR, ALLPERMS);
	if (fd < 0) {
		fprintf(stderr, "cannot open rcache");
		exit(-1);
	}

  ftruncate(fd, MEM_SIZE);
  base = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);

  blk_init(base, MEM_SIZE);
  lru_init(MEM_SIZE / BLK_SIZE);

	regions = (struct mr_context *) calloc(1, sizeof(struct mr_context));

	//allocate memory
	regions[0].type = 0;
	regions[0].addr = (uintptr_t) base;
  regions[0].length = MEM_SIZE;

	init_rdma_agent(portno, regions, 1, 4200, CH_TYPE_REMOTE, add_peer_socket, remove_peer_socket, signal_callback);
  printf("Set up with size %llu\n", MEM_SIZE);
 	
  signal(SIGINT, inthand);

  while (!stop) {
    sleep(1);
  }
  munmap(base, MEM_SIZE);
  printf("hit_ct: %lu, miss_ct: %lu, w_ct: %lu\n", hit_ctr, miss_ctr, w_ctr);

	return 0;
}
