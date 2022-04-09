
#ifndef _SSD_EMULATION_H_
#define _SSD_EMULATION_H_

#include <stdint.h>
#include <time.h>

void init_ssd_emul(void);
uint64_t get_num_migrated(void);
void send_to_ssd(uint64_t blk);
int ssd_has_blk(uint64_t blk);
void destroy_ssd_emul(void);
void ssd_emul_latency(clock_t);

#endif
