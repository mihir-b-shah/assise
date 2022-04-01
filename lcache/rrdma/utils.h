#ifndef RDMA_UTILS_H
#define RDMA_UTILS_H

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define get_tid() syscall(__NR_gettid)
//DEBUG macros
#ifdef DEBUG
 #define debug_print(fmt, args...) fprintf(stderr, "DEBUG[tid:%lu][%s:%d]: " fmt, \
		     	 	get_tid(), __FILE__, __LINE__, ##args)
#else
 #define debug_print(fmt, args...) /*  Don't do anything in release builds */
#endif

/*
 * min()/max()/clamp() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(a, b) ({\
		__typeof__(a) _a = a;\
		__typeof__(b) _b = b;\
		_a < _b ? _a : _b; })

#define max(a, b) ({\
		__typeof__(a) _a = a;\
		__typeof__(b) _b = b;\
		_a > _b ? _a : _b; })

#define ibw_cpu_relax() asm volatile("pause\n": : :"memory")

#define ibw_cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))

#define ibw_unused(expr) do { (void)(expr); } while (0)

extern unsigned int g_seed;

void set_seed(int seed);
int fastrand(int seed);
int cmp_counters(uint32_t a, uint32_t b);
int diff_counters(uint32_t a, uint32_t b);
int find_first_empty_bit_and_set(int bitmap[], int n);
int find_first_empty_bit(int bitmap[], int n);
int find_next_empty_bit(int idx, int bitmap[], int n);
int find_first_set_bit_and_empty(int bitmap[], int n);
int find_first_set_bit(int bitmap[], int n);
int find_next_set_bit(int idx, int bitmap[], int n);
int find_bitmap_weight(int bitmap[], int n);

struct sockaddr_in * copy_ipv4_sockaddr(struct sockaddr_storage *in);
void* mp_create_shm(char* path, size_t size);
void mp_destroy_shm(char* path, void *addr, size_t size);
void split_char(const char *text, char *text1, char *text2);
void mp_die(const char *reason);

#endif
