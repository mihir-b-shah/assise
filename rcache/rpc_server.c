
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include "agent.h"
#include "conf_client.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>

volatile sig_atomic_t stop;

void inthand(int signum)
{	
	stop = 1;
}

uint8_t* base = NULL;
size_t MEM_SIZE = 6000000000;

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
  uint8_t* dst;
};

struct send_req {
  uint32_t repl_id;
  uint64_t block;
  uint8_t data[4096]; // zero length struct
};

#define MR_RCACHE 0
#define MR_DRAM_CACHE 2

#define BLOCK_SIZE 4096

void signal_callback(struct app_context *msg)
{
  if (msg->data[0] == 'R') {
    struct client_req req;
    memcpy(&req, 1+msg->data, sizeof(struct client_req));

    printf("received from client msg[%d] with the following repl_id: %u, blk: %lu, dst: %p\n",
      msg->id, req.node_num, req.block_num, req.dst);

    /* scatter gather is the sge- allows us to read/write to non-contigious memory locations in one go, into a single 
     * contigious buffer 
     */
    rdma_meta_t *meta = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));

    // base addr to copy from- TODO: pointing at a shared memory region is problematic- pin the block?
    meta->addr = (uintptr_t) req.dst;
    meta->length = BLOCK_SIZE;

    // set immediate to sequence number (TBD)
    meta->imm = msg->id;
    meta->sge_count = 1;
    meta->next = NULL;

    // take advantage of zero length array at end of rdma_meta_t
    meta->sge_entries[0].addr = (uintptr_t) base;
    meta->sge_entries[0].length = BLOCK_SIZE;

    IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(msg->sockfd, meta, MR_RCACHE, MR_DRAM_CACHE);
  } else if (msg->data[0] == 'W') {
    struct send_req req;
    memcpy(&req, 1+msg->data, sizeof(struct send_req));

    for (int i = 0; i<4096; ++i) {
      assert(req.data[i] == 'C');
    }
  } else {
    printf("Odd message received.\n");
    exit(0);
  }
}

void add_peer_socket(int sockfd)
{
	;
}

void remove_peer_socket(int sockfd)
{
	;
}

int main(int argc, char **argv)
{
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

	char *host;
	char *portno;
	struct mr_context *regions;
	int sockfd;
	int iters;
	int sync;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		return 1;
	}

	portno = argv[1];

	regions = (struct mr_context *) calloc(1, sizeof(struct mr_context));

	//allocate memory
	regions[0].type = 0;
	regions[0].addr = (uintptr_t) base;
  regions[0].length = MEM_SIZE;

  // send verifiable trash for now
  for (int i = 0; i<4096; ++i) {
    mem[i] = 'C';
  }
	
	init_rdma_agent(portno, regions, 1, 4200, CH_TYPE_REMOTE, add_peer_socket, remove_peer_socket, signal_callback);
 	
  signal(SIGINT, inthand);

  while (!stop) {
    sleep(1);
  }
  munmap(base);

	return 0;
}
