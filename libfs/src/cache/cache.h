
#ifndef _RCACHE_H_
#define _RCACHE_H_

#include <filesystem/fs.h>

struct rcache_req {
  uint64_t block;
  struct inode* inode;
  off_t offset;
  uint32_t io_size;
  uint8_t* dst;
};

extern uint32_t ip_int;

void init_rcache(void);
void init_fetcher_helper(void);
void init_ssd_q(void);

void enq_ssd_req(struct rcache_req req);
void fill_partials_until(struct inode* ip, off_t offs);

extern volatile int res_rcache_VISIBLE;
extern volatile int id_rcache_VISIBLE;
extern volatile uint32_t offs_rcache_VISIBLE;

void rcache_read(struct rcache_req req);
void emul_ssd_read(struct rcache_req* req);

enum fetch_res {
  NONE_SENT = 0,
  PART_SENT = 1,
  FULL_SENT = 2
};

enum fetch_res fetch_remote(struct rcache_req* req);

#endif
