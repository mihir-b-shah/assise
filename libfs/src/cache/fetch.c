
#include "conf_client.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static volatile int loc_version = -1;

struct conn_obj {
  struct in_addr addr;
  int sockfd;
  // more metadata?
};

struct conn_ctx {
  struct conn_obj* conn_ring;
  volatile int n;
  // more metadata?
};

static struct conn_ctx ctx = {.conn_ring = NULL, .n = 0};

int conf_cmp(const void* e1, const void* e2)
{
  const unsigned long v1 = ((struct in_addr*) e1)->s_addr;
  const unsigned long v2 = ((struct in_addr*) e2)->s_addr;

  return v1 == v2 ? 0 : v1 < v2 ? -1 : 1;
}

static struct conn_obj make_conn(struct in_addr* addr)
{
  printf("Open conn %s\n", inet_ntoa(*addr));
  return (struct conn_obj) {.addr = *addr, .sockfd = -1};
}

static void kill_conn(struct conn_obj* obj)
{
  printf("Close conn %s\n", inet_ntoa(obj->addr));
  obj->sockfd = -1;
}

/* TODO: make this robust to changes. Right now,
 * let's just make sure our single connection is the same.
 *
 * 1. Sort the connections in some order.
 * 2. If there are new connections, add them, and if old, kill them.
 */
static void adjust_loc_conf(config_t* conf)
{
  if (ctx.conn_ring == NULL) {
    ctx.conn_ring = malloc(sizeof(struct conn_obj));
    ctx.conn_ring[0].addr.s_addr = 0xffffffffUL;
    ctx.conn_ring[0].sockfd = -1;
  }

  // compute a diff between this conf 
  qsort(conf->ips, 1+conf->n, sizeof(struct in_addr), conf_cmp);
  // invariant, the current ctx has stuff sorted by s_addr too.

  struct conn_obj* loc_ring = malloc((1+conf->n) * sizeof(struct conn_obj));
  loc_ring[conf->n].addr.s_addr = 0xffffffffUL;
  loc_ring[conf->n].sockfd = -1;
  
  int ptr_ring = 0;
  int ptr_conf = 0;
  
  while (ptr_ring < ctx.n || ptr_conf < conf->n) {
    if (ctx.conn_ring[ptr_ring].addr.s_addr < conf->ips[ptr_conf].s_addr) {
      kill_conn(&(ctx.conn_ring[ptr_ring]));
      ++ptr_ring;
    } else if (ctx.conn_ring[ptr_ring].addr.s_addr > conf->ips[ptr_conf].s_addr) {
      loc_ring[ptr_conf] = make_conn(&(conf->ips[ptr_conf]));
      ++ptr_conf;
    } else {
      loc_ring[ptr_conf] = ctx.conn_ring[ptr_ring];
      ++ptr_ring;
      ++ptr_conf;
    }
  }

  free(ctx.conn_ring);
  ctx.conn_ring = loc_ring;
  ctx.n = conf->n;
}

enum fetch_res fetch_remote(struct rcache_req* req)
{
  config_t* conf = get_cache_conf();

  // i hope double checked locking is fine here (not broken)?
  if (conf->version == loc_version) {
    // keep using our current configuration
  } else {
    pthread_mutex_lock(&(conf->mutex));
    loc_version = conf->version;
    // new conf! find the diffs and adjust our connections.
    adjust_loc_conf(conf);
    pthread_mutex_unlock(&(conf->mutex));
  }
}
