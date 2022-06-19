
#include "conf.h"
#include "conf_client.h"
#include "agent.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <distributed/rpc_interface.h>

static volatile int loc_version = -1;

uint32_t ip_int = 0; // declared in the header file.

void setup_appl_conf()
{
  struct in_addr addr;
  inet_aton(g_self_ip, &addr);
  ip_int = addr.s_addr;
}

static struct conn_ctx ctx = {.conn_ring = NULL, .n = 0, .lock = RW_SPINLOCK_INITIALIZER};

static int conf_cmp(const void* e1, const void* e2)
{
  const unsigned long v1 = ((struct in_addr*) e1)->s_addr;
  const unsigned long v2 = ((struct in_addr*) e2)->s_addr;

  return v1 == v2 ? 0 : v1 < v2 ? -1 : 1;
}

#define CACHE_PORT_MIN 11300
#define CACHE_PORT_MAX 11500

static volatile int conn_ct = 0;

static struct conn_obj make_conn(struct in_addr addr)
{
  printf("Open conn %s\n", inet_ntoa(addr));

  char port_buf[10];
  sprintf(port_buf, "%d", CACHE_PORT_MIN);
  int sockfd = add_connection(inet_ntoa(addr), port_buf, 3, getpid(), CH_TYPE_REMOTE, 1);

  while(!mp_is_channel_ready(sockfd)) {
    asm("");
  }
  ++conn_ct;

  return (struct conn_obj) {.addr = addr, .sockfd = sockfd, .rblock_addr = {NULL}};
}

static void kill_conn(struct conn_obj* obj)
{
  perror("Not supporting dynamic connections yet.\n");
  assert(0);

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
      loc_ring[ptr_conf] = make_conn(conf->ips[ptr_conf]);
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

struct conn_ctx* update_cache_conf()
{
  config_t* conf = get_cache_conf();

  // TODO: is this correct?
  int old_loc_version;
  __atomic_exchange(&loc_version, &(conf->version), &old_loc_version, __ATOMIC_SEQ_CST);
  if (old_loc_version != __atomic_load_n(&loc_version, __ATOMIC_SEQ_CST)) {
    pthread_mutex_lock(&(conf->mutex));
    /* no chance of deadlock since we're a single thread and the only one that can ever
       hold these two locks concurrently. */
    rw_spinlock_wr_lock(&(ctx.lock));

    // new conf! find the diffs and adjust our connections.
    loc_version = conf->version;
    adjust_loc_conf(conf);

    rw_spinlock_wr_unlock(&(ctx.lock));
    pthread_mutex_unlock(&(conf->mutex));
  }
  
  rw_spinlock_rd_lock(&(ctx.lock));
  return &ctx;
}

void release_rd_lock()
{
  rw_spinlock_rd_unlock(&(ctx.lock));
}
