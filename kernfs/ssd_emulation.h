
#ifndef _SSD_EMULATION_H_
#define _SSD_EMULATION_H_

#include <stdint.h>

void init_ssd_emul(void);
void send_to_ssd(uint64_t blk);
int ssd_has_blk(uint64_t blk);
void destroy_ssd_emul(void);

#endif
