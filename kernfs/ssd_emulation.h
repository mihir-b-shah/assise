
#ifndef _SSD_EMULATION_H_
#define _SSD_EMULATION_H_

#include "filesystem/slru.h"
#include "ds/khash.h"
#include <stdint.h>

void init_ssd_emul(void);
void send_to_ssd(lru_key_t, lru_val_t);
int ssd_has_blk(int64_t, lru_val_t*);

#endif
