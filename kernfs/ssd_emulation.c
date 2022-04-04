
#include "ssd_emulation.h"

KHASH_MAP_INIT_INT64(imap, lru_val_t)

khash_t(imap) *ssd_blks;

void init_ssd_emul(void){
  ssd_blks = kh_init(imap);
}

void send_to_ssd(lru_key_t key, lru_val_t val)
{
  int success = 0;
  khiter_t iter;
  while (!success) {
    iter = kh_put(imap, ssd_blks, key.block, &success);
  }
  kh_value(ssd_blks, iter) = val;
}

int ssd_has_blk(int64_t key, lru_val_t* val)
{
  khiter_t iter = kh_get(imap, ssd_blks, key);
  if (iter == kh_end(ssd_blks)) {
    return 0;
  } else {
    *val = kh_value(ssd_blks, iter);
    return 1;
  }
}
