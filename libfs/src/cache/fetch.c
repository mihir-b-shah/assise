
#include "cache.h"

// not going to have 2 billion outstanding requests...
static volatile int req_num = 5; // just something random, not 0.

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
};

enum fetch_res fetch_remote(struct rcache_req* req)
{
  struct conn_ctx* ctx = update_cache_conf();
  if (ctx->n == 0) {
    return NONE_SENT;
  }

  int sockfd = ctx->conn_ring[0].sockfd;
  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  int seqn = req_num++;

  struct client_req body = {.node_num = ip_int, .block_num = req->block};

  app->id = seqn;
  memcpy(app->data, &body, sizeof(body));

  MP_SEND_MSG_ASYNC(sockfd, buffer_id, 0);
  MP_AWAIT_RESPONSE(sockfd, seqn);

  // obviously change, but for preliminary experiments...
  return NONE_SENT;
}
