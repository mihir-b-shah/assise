
#include <conf/conf.h>
#include <global/global.h>
#include <io/block_io.h>
#include <io/device.h>
#include <storage/storage.h>
#include "cache.h"
#include "agent.h"
#include <assert.h>

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
  g_bdev[g_root_dev]->storage_engine->read(g_root_dev, 1+sizeof(body)+app->data, block, g_block_size_bytes);

  // we are asking for a particular block.
  MP_SEND_MSG_SYNC(sockfd, buffer_id, 0);

  /* TODO: modify the callback in kernfs/fs.c to make sure we ignore the ACK */
}
