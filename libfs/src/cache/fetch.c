
#include "cache.h"

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
};

static volatile uint32_t glob_seqn = 2;

enum fetch_res fetch_remote(struct rcache_req* req)
{
  printf("Hit fetch_remote.\n");

  struct conn_ctx* ctx = update_cache_conf();
  if (ctx->n == 0) {
    return NONE_SENT;
  }

  printf("Reached %d\n", __LINE__);

  int sockfd = ctx->conn_ring[0].sockfd;

  printf("sockfd: %d\n", sockfd);

  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  uint32_t seqn = glob_seqn;   // should only have one oustanding request at a time- we wait immediately.
  
  printf("Reached %d\n", __LINE__);

  glob_seqn += 2;

  struct client_req body = {.node_num = ip_int, .block_num = req->block};
  
  printf("Reached %d\n", __LINE__);

  app->id = seqn;
  memcpy(app->data, &body, sizeof(body));
  app->data[sizeof(body)] = '\0';

  MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
  printf("Reached %d\n", __LINE__);
  MP_AWAIT_RESPONSE(sockfd, seqn);
  printf("Reached %d\n", __LINE__);

  /*
  uint32_t imm = MP_AWAIT_RESPONSE_MASK(sockfd, seqn, 0x3);

  uint32_t result_type = (imm & 0xc) >> 2;
  uint32_t start_offs = (imm & 0xfff0) >> 4;
  uint32_t end_offs = (imm & 0xfff0000) >> 16;

  printf("Received result_type: %u, s: %u, e: %u\n", result_type, start_offs, end_offs);
  */

  // obviously change, but for preliminary experiments...
  return NONE_SENT;
}
