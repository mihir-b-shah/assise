
#include <conf/conf.h>
#include <global/global.h>
#include <io/block_io.h>
#include <io/device.h>
#include <storage/storage.h>
#include "cache.h"
#include "agent.h"
#include <assert.h>

struct send_req {
  uint32_t repl_id;
  uint64_t block;
  uint8_t data[]; // zero length struct
};

void send_evicted_to_rcache(uint64_t block, uint8_t* data)
{
  struct conn_obj* dst_node = get_dest(block);
  if (dst_node == NULL) {
    return;
  }

  int sockfd = dst_node->sockfd;

  struct app_context* app;
  int buffer_id = MP_ACQUIRE_BUFFER(sockfd, &app);
  uint32_t seqn = glob_seqn;   // should only have one oustanding request at a time- we wait immediately.
  glob_seqn += 2;
  
  struct send_req body = {.repl_id = ip_int, .block = block};
  
  // just send the block! dumb dumb
  
  app->id = seqn;
  app->data[0] = 'W';
  memcpy(1+app->data, &body, sizeof(body));
  memcpy(1+sizeof(body)+app->data, data, g_block_size_bytes);

  // we are asking for a particular block.
  MP_SEND_MSG_SYNC(sockfd, buffer_id, 0);
}
