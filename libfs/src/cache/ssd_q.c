
/* implement the ssd request queue
 * 
 * invariant: the only requests in it are those for which we hit in the remote cache, and move on (so ssd can resolve
 * asynchronously). This then implies, that none of these are necessary for correctness and can be thrown away
 * at convenience.
 *
 * flowchart of how this should work:
 *
 * rcache_read(...) 1. network operation. If succeeds, then async ssd op.
 * On a regular read, if the fcache says it has my block, check our pending queue to see if the cache has a partial
 * result (using a khash table). If so, find it in the queue. Then block until the clock timestamp for its ssd request
 * resolution has arrived, and fill it. If not there in the queue, refill the block from ssd, and add the corresponding
 * latency. (Not doing now). When filling based on an old request, note invalidation could have occurred, as such, do an
 * fcache_find. Simplest thing is probably avoid any locking of fcache - I think we're running single-threaded, and even
 * if multi-threaded, not planning on touching the same inodes.
 */ 

#include <stdio.h>
#include <limits.h>
#include <pthread.h>

#include <global/defs.h>
#include <ds/khash.h>
#include <filesystem/fs.h>
#include "cache.h"

#define QSIZE_SHIFT 6
#define QSIZE (1 << QSIZE_SHIFT)
#define QSIZE_MASK (QSIZE - 1)

struct q_obj {
  struct rcache_req req;
  clock_t ts;
};

/* queue */
static struct q_obj q[QSIZE];
static volatile int head = 0;
static volatile int tail = 0;
static volatile int size = 0;

/* hash map mapping to positions in queue */
KHASH_MAP_INIT_INT64(m64, struct q_obj*)
static khash_t(m64) *qmap;

/* mutex used for both */
static pthread_mutex_t mutex;

void init_ssd_q(void)
{
  pthread_mutex_init(&mutex, 0);
  qmap = kh_init(m64);
}

static inline uint64_t make_key(struct inode* ip, off_t offset)
{
  off_t block_no = offset >> g_block_size_shift;
  if (block_no >= UINT32_MAX) {
    panic("Inode bigger than bigger than 2^32 blocks. (17 TB)");
  }
  return ((block_no & 0xffffffffULL) << 32) | ip->inum;
}

static void enq(struct q_obj obj)
{
  pthread_mutex_lock(&mutex);

  if (size == QSIZE && head == tail) {
    pthread_mutex_unlock(&mutex);
    panic("SSD queue capacity reached- if this is a problem, we can drop and handle things.\n");
  }
  
  struct q_obj* slot = &q[tail++ & QSIZE_MASK];
  *slot = obj;
  ++size;
  
  uint64_t key = make_key(obj.req.inode, obj.req.offset);
  int map_ret;
  khiter_t iter = kh_put(m64, qmap, key, &map_ret);
  kh_value(qmap, iter) = slot;

  pthread_mutex_unlock(&mutex);
}

// think its ok as single threaded.
void enq_ssd_req(struct rcache_req req)
{
  struct q_obj obj = {.req = req, .ts = clock() + (CLOCKS_PER_SEC/100000)};
  enq(obj);
}

// fill the old object, and return the invalidated pointer.
static struct q_obj* deq(struct q_obj* fill)
{
  pthread_mutex_lock(&mutex);

  if (size == 0) {
    pthread_mutex_unlock(&mutex);
    panic("Dequeue on empty ssd queue.\n");
  } else {
    --size;
  }

  struct q_obj* ptr = &q[head++ & QSIZE_MASK];
  *fill = *ptr;
  uint64_t key = make_key(fill->req.inode, fill->req.offset);
  kh_del(m64, qmap, key);

  pthread_mutex_unlock(&mutex);
  return ptr;
}

static struct q_obj* get_qobj(struct inode* ip, off_t offs)
{
  uint64_t key = make_key(ip, offs);
	khiter_t iter = kh_get(m64, qmap, key);
  return iter == kh_end(qmap) ? NULL : kh_value(qmap, iter);
}

void emul_ssd_read(struct rcache_req* req)
{
	struct buffer_head* bh = bh_get_sync_IO(g_root_dev, req->block, BH_NO_DATA_ALLOC);
	bh->b_size = g_block_size_shift;

  bh->b_offset = ALIGN_FLOOR(req->offset, g_block_size_bytes);
  bh->b_data = req->dst;
  bh->b_size = g_block_size_bytes;
  bh_submit_read_sync_IO(bh);
  bh_release(bh);
}

static void fill_partial(struct q_obj* obj)
{
  while (clock() < obj->ts);

  struct fcache_block* dst = fcache_find(obj->req.inode, obj->req.offset);
  if (dst == NULL) {
    // if the block itself is gone, no need to fill it from ssd!
    return;
  }

  obj->req.dst = dst->data;
  emul_ssd_read(&(obj->req));
}

/* note, I am the only one that does deq() operations- as such,
 * it is safe to read the head ptr without synchronization
 */
void fill_partials_until(struct inode* ip, off_t offs)
{
  struct q_obj* to_fill = get_qobj(ip, offs);

  if (to_fill == NULL) {
    return;
  }

  struct q_obj obj;
  while(deq(&obj) != to_fill) {
    fill_partial(&obj);
  }
  // handle the final one in `obj`
  fill_partial(&obj);
}
