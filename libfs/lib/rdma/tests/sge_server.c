#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include "agent.h"
#include "rdma_ch.h"
#include "common.h"

volatile sig_atomic_t stop;

#define MAX_SGE 16

void* rmem;
int BUFFER_COUNT = 5;
uint64_t LOG_SIZE =  268265456UL; //256 MB
uint64_t BUFFER_SIZE = 8388608UL; //8 MB  

void inthand(int signum)
{	
	stop = 1;
}

// call this function to start a nanosecond-resolution timer
struct timespec timer_start(){
	struct timespec start_time;
	clock_gettime(CLOCK_REALTIME, &start_time);
	return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long timer_end(struct timespec start_time) {
	struct timespec end_time;
	long sec_diff, nsec_diff, nanoseconds_elapsed;

	clock_gettime(CLOCK_REALTIME, &end_time);

	sec_diff =  end_time.tv_sec - start_time.tv_sec;
	nsec_diff = end_time.tv_nsec - start_time.tv_nsec;

	if(nsec_diff < 0) {
		sec_diff--;
		nsec_diff += (long)1e9;
	}

	nanoseconds_elapsed = sec_diff * (long)1e9 + nsec_diff;

	return nanoseconds_elapsed;
}

double test(struct timespec start)
{
	struct timespec finish;
	clock_gettime(CLOCK_REALTIME, &finish);
 	long seconds = finish.tv_sec - start.tv_sec; 
     	long ns = finish.tv_nsec - start.tv_nsec; 
         
         if (start.tv_nsec > finish.tv_nsec) { // clock underflow 
	 	--seconds; 
	 	ns += 1000000000; 
	     }
	return (double)seconds + (double)ns/(double)1e9;
}

void signal_callback(struct app_context *msg)
{
  struct ibv_device_attr attrs;
  printf("get_context(): %p\n", get_context());
  ibv_query_device(get_context()->ctx[0], &attrs);
  printf("MAX_SGE: %d, MAX_SGE_RD: %d\n", attrs.max_sge, attrs.max_sge_rd);

	printf("received msg[%d] with the following body: %s\n", msg->id, msg->data);
#if 0
	struct app_context *app;
	int buffer_id = MP_ACQUIRE_BUFFER(msg->sockfd, &app);
	app->id = msg->id; //set this to same id of received msg (to act as a response)
	char* data = "answering your message";
	snprintf(app->data, msg_size, "%s", data);
	MP_SEND_MSG_ASYNC(msg->sockfd, buffer_id, 0);
#else
  printf("Line %d\n", __LINE__);
	rdma_meta_t *meta = (rdma_meta_t*) malloc(sizeof(rdma_meta_t) + MAX_SGE*sizeof(struct ibv_sge));
	meta->addr = strtoull(msg->data, NULL, 16);
	meta->length = 32*MAX_SGE;
	meta->sge_count = MAX_SGE;
  for (int i = 0; i<MAX_SGE; ++i) {
    meta->sge_entries[i].addr = (uintptr_t) rmem+64*i;
    meta->sge_entries[i].length = 32;
  }
	meta->imm = msg->id;
  printf("Line %d\n", __LINE__);
	IBV_WRAPPER_WRITE_WITH_IMM_ASYNC(msg->sockfd, meta, 0, 0);
  printf("Line %d\n", __LINE__);
#endif
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
  rmem = mem;
	
	/*
	for(int i=1; i<BUFFER_COUNT+1; i++) {
		posix_memalign(&mem, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
		regions[i].type = i;
		regions[i].addr = (uintptr_t) mem;
        	regions[i].length = BUFFER_SIZE;	
	}
	*/
		/*typedef struct rdma_metadata {
			addr_t address;
			addr_t total_len;
			int sge_count;
			struct ibv_sge sge_entries[];
		} rdma_meta_t;*/	

	init_rdma_agent(portno, regions, 1, 256, CH_TYPE_REMOTE, add_peer_socket, remove_peer_socket, signal_callback);
 	signal(SIGINT, inthand);

 	while(!stop) {
		sleep(1);
	}
	//sleep(1);
	free(mem);

	return 0;
}
