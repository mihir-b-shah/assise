
#ifndef _RCACHE_H_
#define _RCACHE_H_

#include "conf_client.h"
#include <filesystem/fs.h>

struct rcache_req {
  uint64_t block;
  struct inode* inode;
  off_t offset;
  uint32_t io_size;
  uint8_t* dst;
};

void init_rcache(void);
void init_ssd_q(void);

void enq_ssd_req(struct rcache_req req);
void fill_partials_until(struct inode* ip, off_t offs);

config_t* get_cache_conf();
void rcache_read(struct rcache_req req);
void emul_ssd_read(struct rcache_req* req);

enum fetch_res {
  NONE_SENT = 0,
  PART_SENT = 1,
  FULL_SENT = 2
};

enum fetch_res fetch_remote(struct rcache_req* req);

#endif
