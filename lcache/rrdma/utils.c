#include "utils.h"

unsigned int g_seed;

void mp_die(const char *reason)
{
	fprintf(stderr, "%s [error code: %d]\n", reason, errno);
	exit(EXIT_FAILURE);
}

void set_seed(int seed) {
	g_seed = seed;
}

int fastrand(int seed) { 
	seed = (214013*seed+2531011); 
	return (seed>>16)&0x7FFF; 
}

int cmp_counters(uint32_t a, uint32_t b) {
	if (a == b)
		return 0;
	else if((a - b) < UINT32_MAX/2)
		return 1;
	else
		return -1;
}

int diff_counters(uint32_t a, uint32_t b) {
	if (a >= b)
		return a - b;
	else
		return b - a;
}

int find_first_empty_bit_and_set(int bitmap[], int n)
{
	for(int i=0; i<n; i++) {
		if(!ibw_cmpxchg(&bitmap[i], 0, 1))
			return i;
	}
	return -1;					
}

int find_first_empty_bit(int bitmap[], int n)
{
	for(int i=0; i<n; i++) {
		if(!bitmap[i])
			return i;
	}
	return -1;
}

int find_next_empty_bit(int idx, int bitmap[], int n)
{
	for(int i=idx+1; i<n; i++) {
		if(!bitmap[i])
			return i;
	}
	return -1;
}

int find_first_set_bit_and_empty(int bitmap[], int n)
{
	for(int i=0; i<n; i++) {
		if(ibw_cmpxchg(&bitmap[i], 1, 0))
			return i;
	}
	return -1;							
}

int find_first_set_bit(int bitmap[], int n)
{
	for(int i=0; i<n; i++) {
		if(bitmap[i])
			return i;
	}
	return -1;
}

int find_next_set_bit(int idx, int bitmap[], int n)
{
	for(int i=idx+1; i<n; i++) {
		if(bitmap[i])
			return i;
	}
	return -1;
}

int find_bitmap_weight(int bitmap[], int n)
{
	int weight = 0;
	for(int i=0; i<n; i++) {
		weight += bitmap[i];
	}
	return weight;
}

struct sockaddr_in * copy_ipv4_sockaddr(struct sockaddr_storage *in)
{
	if(in->ss_family == AF_INET) {
		struct sockaddr_in *out = (struct sockaddr_in *) calloc(1, sizeof(struct sockaddr_in));
		memcpy(out, in, sizeof(struct sockaddr_in));
		return out;
	}
	else {
		return NULL;
	}
}

void* mp_create_shm(char* path, size_t size) {
	void * addr;

	debug_print("mp_create_shm: %s\n", path);	
	int fd = shm_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		perror("shm_open failed.\n");
		exit(-1);
	}

	int res = ftruncate(fd, size);
	if (res < 0)
	{
		perror("ftruncate error.\n");
		exit(-1);
	}

	addr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED){
		perror("mmap failed.\n");
		exit(-1);
	}

	return addr;
}

void mp_destroy_shm(char* path, void *addr, size_t size) {
	int ret, fd;

	debug_print("mp_destroy_shm: %s\n", path);	
	ret = munmap(addr, size);
	if (ret < 0)
	{
		perror("munmap error.\n");
		exit(-1);
	}

	fd = shm_unlink(path);
	if (fd < 0) {
		perror("shm_unlink failed.\n");
		exit(-1);
	}
}

void split_char(const char *text, char *text1, char *text2)
{
	int len = (strchr(text,'|')-text)*sizeof(char);
	strncpy(text1, text, len);
	strcpy(text2, text+len+1);
}
