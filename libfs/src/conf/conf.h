
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "conf_client.h"
#include "rw_spinlock.h"
#include <time.h>
#include <stdint.h>
#include <global/global.h>

struct conn_obj {
  struct in_addr addr;
  size_t idx;
  int sockfd;
  volatile void* rblock_addr[g_max_meta];
  struct timespec ts;
};

struct conn_ctx {
  struct conn_obj* conn_ring;
  volatile int n;
  struct rw_spinlock lock;
};

uint32_t ip_int; // my ip, as an integer.
void setup_appl_conf();

struct conn_ctx* update_cache_conf();
void release_rd_lock();
config_t* get_cache_conf();
struct conn_obj* get_dest(uint64_t block_no);

#endif
