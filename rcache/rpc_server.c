
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
#include "globals.h"

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

#define BLOCK_DEPTH 10

static uint64_t hit_ctr = 0;
static uint64_t miss_ctr = 0;
static uint64_t w_ctr = 0;
static uint64_t w_lat = 0;

float get_cpu_clock_speed(void)
{
	FILE* fp;
	char buffer[1024], dummy[64];
	size_t bytes_read;
	char* match;
	float clock_speed;

	/* Read the entire contents of /proc/cpuinfo into the buffer.  */
	fp = fopen ("/proc/cpuinfo", "r");
	bytes_read = fread (buffer, 1, sizeof (buffer), fp);
	fclose (fp);

	/* Bail if read failed or if buffer isn't big enough.  */
	if (bytes_read == 0)
		return 0;

	/* NUL-terminate the text.  */
	buffer[bytes_read] = '\0';

	/* Locate the line that starts with "cpu MHz".  */
	match = strstr(buffer, "cpu MHz");
	if (match == NULL) 
		return 0;

	match = strstr(match, ":");

	/* Parse the line to extrace the clock speed.  */
	sscanf (match, ": %f", &clock_speed);
	return clock_speed;
}

// only called once, so fine.
static float tsc_to_ms(uint64_t tsc)
{
    float clock_speed_mhz = get_cpu_clock_speed();
    return (float)tsc / (clock_speed_mhz * 1000.0);
}

static unsigned long long asm_rdtscp(void)
{
    unsigned hi, lo;
      __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi)::"rcx");
        return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

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

struct fshare_safe_u64 {
  uint32_t ct;
  char junk[60];
};
struct fshare_safe_u64 buf_cts[MAX_CONNECTIONS] = {{BLOCK_DEPTH-1, {0}}};

void refresh_appl_buffer(int sockfd)
{
  static uint32_t seqn = 2;

  if (++buf_cts[sockfd].ct < BLOCK_DEPTH) {
    return;
  } else {
    buf_cts[sockfd].ct = 0;
  }

  printf("Sending msg.\n");
  // send the application node its initial buffer.
  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  app->id = __atomic_fetch_add(&seqn, 1, __ATOMIC_SEQ_CST);
  app->data[0] = 'N';

  for (size_t i = 0; i<BLOCK_DEPTH; ++i) {
    ((void**) (8+app->data))[i] = blk_alloc();
  }

  MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
}

void signal_callback(struct app_context *msg)
{
  uint64_t stime = asm_rdtscp();
  //pthread_mutex_lock(&lock);

  if (!msg->data) {
    // immediate notification from an application
    uint32_t appl_ip = msg->id;
    printf("Received msg from ip: %d.%d.%d.%d.\n", appl_ip & 0xff, (appl_ip >> 8) & 0xff,
      (appl_ip >> 16) & 0xff, (appl_ip >> 24) & 0xff);
    refresh_appl_buffer(msg->sockfd);
  }

  //pthread_mutex_unlock(&lock);
}

void add_peer_socket(int sockfd)
{
  refresh_appl_buffer(sockfd);
}

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
  MEM_SIZE = BLK_SIZE * strtoull(argv[2], NULL, 16);
  
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

	regions = (struct mr_context *) calloc(1, sizeof(struct mr_context));

	//allocate memory
	regions[0].type = MR_RCACHE;
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
  printf("w_lat: %.6f\n", tsc_to_ms(w_lat));

	return 0;
}
