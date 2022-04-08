
#ifndef _RCACHE_H_
#define _RCACHE_H_

#include "conf_client.h"

config_t* get_cache_conf();
void rcache_read(uint64_t block, off_t offset, uint32_t io_size, uint8_t* dst);

#endif
