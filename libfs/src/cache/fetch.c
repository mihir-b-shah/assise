
#include "cache.h"
#include <conf/conf.h>

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
  uint8_t* dst;
};

static volatile uint32_t glob_seqn = 2;

#define SEQN_MASK 0x000fffffUL
#define PRESENT_MASK 0x80000000UL

enum fetch_res fetch_remote(struct rcache_req* req)
{
  struct conn_ctx* ctx = update_cache_conf();
  if (ctx->n == 0) {
    return NONE_SENT;
  }

  int sockfd = ctx->conn_ring[0].sockfd;

  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  uint32_t seqn = glob_seqn;   // should only have one oustanding request at a time- we wait immediately.
  
  glob_seqn = (glob_seqn + 2) & SEQN_MASK;

  struct client_req body = {.node_num = ip_int, .block_num = req->block, .dst = req->dst};
  
  app->id = seqn;
  app->data[0] = 'R';
  memcpy(1+app->data, &body, sizeof(body));

  MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
  
  uint32_t imm = MP_AWAIT_RESPONSE_MASK(sockfd, seqn, SEQN_MASK);

  printf("inum: %u, offs: %lx, blk: %lu\n", req->inode, req->offset, req->block);

  if (imm & PRESENT_MASK) {
    return FULL_SENT;
  } else {
    return NONE_SENT;
  }

  /*
  uint32_t imm = MP_AWAIT_RESPONSE_MASK(sockfd, seqn, 0x3);

  uint32_t result_type = (imm & 0xc) >> 2;
  uint32_t start_offs = (imm & 0xfff0) >> 4;
  uint32_t end_offs = (imm & 0xfff0000) >> 16;

  printf("Received result_type: %u, s: %u, e: %u\n", result_type, start_offs, end_offs);
  */

  // obviously change, but for preliminary experiments...
}
