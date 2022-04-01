#include <time.h>
#include "agent.h"

int msg_sync = 1;

uint64_t LOG_SIZE =  268265456UL; //256 MB

void add_peer_socket(int sockfd)
{
	;
}

void remove_peer_socket(int sockfd)
{
	;
}

void signal_callback(struct app_context *msg)
{
	printf("received msg[%d] with the following id: %s\n", msg->id, msg->data);
}

int main(int argc, char **argv)
{
	char *host;
	char *portno;
	struct mr_context *regions;
	void *mem;
	int sockfd;
	int iters;
	
	portno = argv[2];
	iters = atoi(argv[3]);

	regions = (struct mr_context *) calloc(1, sizeof(struct mr_context));

	//allocate memory for log area
	posix_memalign(&mem, sysconf(_SC_PAGESIZE), LOG_SIZE);
	regions[0].type = 0;
	regions[0].addr = (uintptr_t) mem;
  regions[0].length = LOG_SIZE;	

	init_rdma_agent(NULL, regions, 1, 256, CH_TYPE_REMOTE, add_peer_socket, remove_peer_socket, signal_callback);
 	sockfd = add_connection(argv[1], portno, 0, 0, CH_TYPE_REMOTE, 1);

	while(!mp_is_channel_ready(sockfd)) {
    asm("");
	}

	struct app_context *app;
	int buffer_id;
	int seqn;

	for(int i=0; i<iters; i++) {
		seqn = 1 + i; //app_ids must start at 1
		buffer_id = MP_ACQUIRE_BUFFER(0, &app);
		app->id = seqn;

    struct client_req req = {.repl_id = 1, .inum = 1+2*i}; 
    memcpy(app->data, &req, sizeof(struct client_req));

		MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);

		if(msg_sync)
 			MP_AWAIT_RESPONSE(sockfd, seqn);
	}

	sleep(2);
	free(mem);

	return 0;
}
