
#include "cache.h"
#include <conf/conf.h>

struct client_req {
  uint32_t node_num;
  uint64_t block_num;
  uint8_t* dst;
};

static volatile uint32_t glob_seqn = 2;

#define SEQN_MASK 0x0fffffffUL
#define PRESENT_MASK 0x80000000UL

// visible to application, deliberately.
volatile int res_rcache_VISIBLE = -1;
volatile int id_rcache_VISIBLE = -1;
volatile uint32_t offs_rcache_VISIBLE = 0;

enum fetch_res fetch_remote(struct rcache_req* req)
{
  struct conn_obj* dst_node = get_dest(req->block);
  if (dst_node == NULL) {
    return NONE_SENT;
  }

  int sockfd = dst_node->sockfd;

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

  ++id_rcache_VISIBLE;
  offs_rcache_VISIBLE = req->offset;

  if (imm & PRESENT_MASK) {
    res_rcache_VISIBLE = FULL_SENT;
    return FULL_SENT;
  } else {
    res_rcache_VISIBLE = NONE_SENT;
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
