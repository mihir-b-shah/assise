
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "conf_client.h"

struct conn_obj {
  struct in_addr addr;
  int sockfd;
};

struct conn_ctx {
  struct conn_obj* conn_ring;
  volatile int n;
};

uint32_t ip_int; // my ip, as an integer.
void setup_appl_conf();

struct conn_ctx* update_cache_conf();
config_t* get_cache_conf();

#endif
