
#include <conf/conf.h>
#include <assert.h>
#include "cache.h"

#include "agent.h"
#include <global/global.h>

static volatile uint32_t glob_seqn = 2;

void send_to_rcache(uint64_t block)
{
  struct conn_ctx* ctx = update_cache_conf();
  if (ctx->n == 0) {
    // if there are no cache nodes, just ignore sending- don't queue the block, for now.
    return;
  }

  // hopefully when I use consistent hashing, common code can go in conf.h
  int sockfd = ctx->conn_ring[0].sockfd;

  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  uint32_t seqn = glob_seqn;   // should only have one oustanding request at a time- we wait immediately.
  glob_seqn += 2;
  
  struct send_req body = {.repl_id = ip_int, .block = block};
  
  // just send the block! dumb dumb
    
  app->id = seqn;
  app->data[0] = 'W';
  memcpy(1+app->data, &body, sizeof(body));

  for (int i = 0; i<g_block_size_bytes; ++i) {
    app->data[1+sizeof(body)+i] = 'C';
  }

  // we are asking for a particular block.
  MP_SEND_MSG_SYNC(sockfd, buffer_id, 0);

  /* TODO: modify the callback in kernfs/fs.c to make sure we ignore the ACK */
}
