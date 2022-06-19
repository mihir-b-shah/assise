
#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>

struct send_req {
  uint32_t repl_id;
  uint64_t block;
  uint8_t data[]; // zero length struct
};

void init_cache();
void send_to_rcache(uint64_t block);

#endif
