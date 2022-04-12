#include <time.h>
#include <signal.h>
#include <stdint.h>
#include "agent.h"
#include "conf_client.h"

volatile sig_atomic_t stop;

void inthand(int signum)
{	
	stop = 1;
}

uint64_t LOG_SIZE =  268265456UL; //256 MB

struct client_req {
  uint32_t repl_id;
  uint64_t inum;
};

void signal_callback(struct app_context *msg)
{
  struct client_req req;
  memcpy(&req, msg->data, sizeof(struct client_req));

	printf("received from client msg[%d] with the following repl_id: %u, inum: %lu\n", msg->id, req.repl_id, req.inum);

	struct app_context *app;
	int buffer_id = MP_ACQUIRE_BUFFER(msg->sockfd, &app);
	app->id = msg->id; //set this to same id of received msg (to act as a response)
 
	// MP_SEND_MSG_ASYNC(msg->sockfd, buffer_id, 0); - change if problematic.

	rdma_meta_t *meta = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + sizeof(struct ibv_sge));
	meta->addr = 0;
	meta->length =0;
	meta->sge_count = 0;
  meta->next = NULL;
	meta->imm = msg->id; //set immediate to sequence number in order for requester to match it (in case of io wait)
	IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(msg->sockfd, meta, 0, 0);
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

	char *host;
	char *portno;
	struct mr_context *regions;
	void *mem;
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
	posix_memalign(&mem, sysconf(_SC_PAGESIZE), LOG_SIZE);
	regions[0].type = 0;
	regions[0].addr = (uintptr_t) mem;
        regions[0].length = LOG_SIZE;	
	
	init_rdma_agent(portno, regions, 1, 1500, CH_TYPE_REMOTE, add_peer_socket, remove_peer_socket, signal_callback);
 	
  signal(SIGINT, inthand);

  while (!stop) {
    sleep(1);
  }
	free(mem);

	return 0;
}
