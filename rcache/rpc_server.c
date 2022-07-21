
#include <pthread.h>
#include "rcache.h"

#include <time.h>
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "agent.h"
#include "core_ch.h"
#include "conf_client.h"
#include "globals.h"

#include <fcntl.h>
#include <errno.h>
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

uint8_t* base = (uint8_t*) 0x600000000000;
size_t MEM_SIZE = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int mapping_ok(uint8_t* mem_base, size_t mem_size)
{
  size_t page_size = sysconf(_SC_PAGE_SIZE);
  uint8_t* arr = calloc((mem_size + page_size) / page_size, sizeof(uint8_t));
  int res = mincore(mem_base, mem_size, arr);
  free(arr);
  return res == -1 && errno == ENOMEM;
}

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

// prevent false-sharing.
struct fshare_safe_u64 {
  volatile uint32_t ct;
  char junk[64-sizeof(ct)];
};
struct fshare_safe_u64 buf_cts[MAX_CONNECTIONS] = {{NUM_ALLOC-1, {0}}};


// W + 2d
void refresh_appl_buffer(int sockfd, int force)
{
  static uint32_t seqn = 2;

  if (!force && ++buf_cts[sockfd].ct < NUM_ALLOC) {
    return;
  } else {
    assert(!force || buf_cts[sockfd].ct == 0);
    buf_cts[sockfd].ct = 0;
  }

  // send the application node its initial buffer.
  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  app->id = __atomic_fetch_add(&seqn, 1, __ATOMIC_SEQ_CST);
  app->data[0] = 'N';
  
  struct timespec t;
  assert(clock_gettime(CLOCK_REALTIME, &t) == 0);
  ((uint64_t*) (8+app->data))[0] = t.tv_sec;
  ((uint64_t*) (8+app->data))[1] = t.tv_nsec;

  for (size_t i = 0; i<NUM_ALLOC; ++i) {
    ((void**) (24+app->data))[i] = blk_alloc(sockfd);
  }

  MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
}

void signal_callback(struct app_context *msg)
{
  uint64_t stime = asm_rdtscp();

  // immediate notification from an application, or refresh notification
  if (!msg->data) {
    blk_free(msg->sockfd, (msg->id) >> 8);
    refresh_appl_buffer(msg->sockfd, false);
  } else if (msg->data[0] == 'R') {
    refresh_appl_buffer(msg->sockfd, true);
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
  MEM_SIZE = ALLOC_SIZE * strtoull(argv[2], NULL, 16);
  
  conf_cmd_t ccmd;
  init_cmd(&ccmd);
  start_cache_client(&ccmd);

  int fd = shm_open("rcache", O_CREAT | O_RDWR, ALLPERMS);
	if (fd < 0) {
		fprintf(stderr, "cannot open rcache");
    return 1;
	}

  ftruncate(fd, MEM_SIZE);
  if (!mapping_ok(base, MEM_SIZE)) {
    perror("Memory already taken.\n");
    return 1;
  }
  base = mmap(base, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);

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
