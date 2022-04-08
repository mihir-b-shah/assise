
#include "filesystem/ssd_emulation.h"
#include "io/block_io.h"
#include "storage/storage.h"
#include "global/global.h"
#include "global/util.h"

void rcache_read(uint64_t block, off_t offset, uint32_t io_size, uint8_t* dst)
{
	struct buffer_head* bh = bh_get_sync_IO(g_root_dev, block, BH_NO_DATA_ALLOC);
	bh->b_size = g_block_size_shift;

  bh->b_offset = ALIGN_FLOOR(offset, g_block_size_bytes);
  bh->b_data = dst;
  bh->b_size = g_block_size_bytes;
  bh_submit_read_sync_IO(bh);
  bh_release(bh);
    
  ssd_emul_latency();
}
    
