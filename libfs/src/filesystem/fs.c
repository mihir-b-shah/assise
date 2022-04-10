
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <numa.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "storage/storage.h"
#include "mlfs/mlfs_user.h"
#include "global/global.h"
#include "concurrency/synchronization.h"
#include "concurrency/thread.h"
#include "filesystem/fs.h"
#include "io/block_io.h"
#include "filesystem/file.h"
#include "log/log.h"
#include "mlfs/mlfs_interface.h"
#include "ds/bitmap.h"
#include "filesystem/slru.h"
#include "filesystem/ssd_emulation.h"

#include "distributed/rpc_interface.h"
#include "cache/cache.h"
#include "conf_client.h"


#define _min(a, b) ({\
		__typeof__(a) _a = a;\
		__typeof__(b) _b = b;\
		_a < _b ? _a : _b; })

int log_fd = 0;
int shm_fd = 0;

struct disk_superblock *disk_sb;
struct super_block *sb[g_n_devices + 1];
ncx_slab_pool_t *mlfs_slab_pool;
ncx_slab_pool_t *mlfs_slab_pool_shared;
uint8_t *shm_base;
uint8_t shm_slab_index = 0;

uint8_t initialized = 0;

// statistics
uint8_t enable_perf_stats;

#ifdef DISTRIBUTED
uint8_t *g_fcache_base;
unsigned long *g_fcache_bitmap;
#endif

struct lru g_fcache_head;

pthread_rwlock_t *icreate_rwlock;
pthread_rwlock_t *icache_rwlock;
pthread_rwlock_t *dlookup_rwlock;
pthread_rwlock_t *invalidate_rwlock;
pthread_rwlock_t *g_fcache_rwlock;

pthread_rwlock_t *shm_slab_rwlock;
pthread_rwlock_t *shm_lru_rwlock;

pthread_rwlock_t *g_debug_rwlock;

#if MLFS_LEASE
SharedTable *lease_table;
#endif

struct inode *inode_hash;
struct dlookup_data *dlookup_hash;

struct list_head *lru_heads;

libfs_stat_t g_perf_stats;
float clock_speed_mhz;

static inline float tsc_to_ms(uint64_t tsc)
{
	return (float)tsc / (clock_speed_mhz * 1000.0);
}

void show_libfs_stats(void)
{
	printf("\n");
	printf("----------------------- libfs statistics\n");
	printf("Log dev id	  : %d\n", g_log_dev);
	// For some reason, floating point operation causes segfault in filebench 
	// worker thread.
	//printf("posix rename	  : %.3f ms\n", tsc_to_ms(g_perf_stats.posix_rename_tsc));
	printf("wait on digest    : %.6f ms\n", tsc_to_ms(g_perf_stats.digest_wait_tsc));
	printf("inode allocation  : %.6f ms\n", tsc_to_ms(g_perf_stats.ialloc_tsc));
	printf("bcache search     : %.6f ms\n", tsc_to_ms(g_perf_stats.bcache_search_tsc));
	printf("search l0 tree    : %.6f ms\n", tsc_to_ms(g_perf_stats.l0_search_tsc));
	printf("search lsm tree   : %.6f ms\n", tsc_to_ms(g_perf_stats.tree_search_tsc));
	printf("log commit        : %.6f ms\n", tsc_to_ms(g_perf_stats.log_commit_tsc));
	printf("  log writes      : %.6f ms\n", tsc_to_ms(g_perf_stats.log_write_tsc));
	printf("  loghdr writes   : %.6f ms\n", tsc_to_ms(g_perf_stats.loghdr_write_tsc));
	printf("read data blocks  : %.6f ms\n", tsc_to_ms(g_perf_stats.read_data_tsc));
	printf("wait on read rpc  : %.6f ms\n", tsc_to_ms(g_perf_stats.read_rpc_wait_tsc));
	printf("directory search  : %.6f ms\n", tsc_to_ms(g_perf_stats.dir_search_tsc));
	printf("temp_debug        : %.6f ms\n", tsc_to_ms(g_perf_stats.tmp_tsc));
	printf("rsync coalesce    : %.6f ms\n", tsc_to_ms(g_perf_stats.coalescing_log_time_tsc));
	printf("rsync interval    : %.6f ms\n", tsc_to_ms(g_perf_stats.calculating_sync_interval_time_tsc));
	printf("rdma write        : %.6f ms\n", tsc_to_ms(g_perf_stats.rdma_write_time_tsc));
	printf("lease rpc wait    : %.6f ms\n", tsc_to_ms(g_perf_stats.lease_rpc_wait_tsc));
	printf("lease lpc wait    : %.6f ms\n", tsc_to_ms(g_perf_stats.lease_lpc_wait_tsc));
	printf("   contention     : %.6f ms\n", tsc_to_ms(g_perf_stats.local_contention_tsc));
	printf("   digestion      : %.6f ms\n", tsc_to_ms(g_perf_stats.local_digestion_tsc));
	printf("lease revoke wait : %.6f ms\n", tsc_to_ms(g_perf_stats.lease_revoke_wait_tsc));
	printf("n_rsync           : %u\n", g_perf_stats.n_rsync);
	printf("n_rsync_skip      : %u\n", g_perf_stats.n_rsync_skipped);
	printf("n_rsync_blks      : %u\n", g_perf_stats.n_rsync_blks);
	printf("n_rsync_blks_skip : %u\n", g_perf_stats.n_rsync_blks_skipped);
	printf("rsync ops         : %u\n", g_perf_stats.rsync_ops);
	printf("lease rpcs        : %u\n", g_perf_stats.lease_rpc_nr);
	printf("lease lpcs        : %u\n", g_perf_stats.lease_lpc_nr);
	printf("read cache hit    : %lu\n", g_perf_stats.read_cache_hit);
	printf("nvm hit ct        : %lu\n", g_perf_stats.nvm_hit);
	printf("ssd hit ct        : %lu\n", g_perf_stats.ssd_hit);
	printf("read cache evict  : %lu\n", g_perf_stats.read_cache_evict);
	printf("read cache add    : %lu\n", g_perf_stats.read_cache_add);


/*
printf("wait on digest  (tsc)  : %lu \n", g_perf_stats.digest_wait_tsc);
printf("inode allocation (tsc) : %lu \n", g_perf_stats.ialloc_tsc);
printf("bcache search (tsc)    : %lu \n", g_perf_stats.bcache_search_tsc);
printf("search l0 tree  (tsc)  : %lu \n", g_perf_stats.l0_search_tsc);
printf("search lsm tree (tsc)  : %lu \n", g_perf_stats.tree_search_tsc);
printf("log commit (tsc)       : %lu \n", g_perf_stats.log_commit_tsc);
printf("  log writes (tsc)     : %lu \n", g_perf_stats.log_write_tsc);
printf("  loghdr writes (tsc)  : %lu \n", g_perf_stats.loghdr_write_tsc);
printf("read data blocks (tsc) : %lu \n", g_perf_stats.read_data_tsc);
printf("directory search (tsc) : %lu \n", g_perf_stats.dir_search_tsc);
printf("temp_debug (tsc)       : %lu \n", g_perf_stats.tmp_tsc);
*/
#if 1
	printf("wait on digest (nr)   : %u \n", g_perf_stats.digest_wait_nr);
	printf("search lsm tree (nr)  : %u \n", g_perf_stats.tree_search_nr);
	printf("log writes (nr)       : %u \n", g_perf_stats.log_write_nr);
	printf("read data blocks (nr) : %u \n", g_perf_stats.read_data_nr);
	printf("read rpc (nr) : %u \n", g_perf_stats.read_rpc_nr);
	printf("directory search hit  (nr) : %u \n", g_perf_stats.dir_search_nr_hit);
	printf("directory search miss (nr) : %u \n", g_perf_stats.dir_search_nr_miss);
	printf("directory search notfound (nr) : %u \n", g_perf_stats.dir_search_nr_notfound);
#endif
	printf("--------------------------------------\n");
}

void signal_shutdown_fs(int signum)
{
	printf("received shutdown signal [signum:%d]\n", signum);
	shutdown_fs();
}

void shutdown_fs(void)
{
	int ret;
	int _enable_perf_stats = enable_perf_stats;

	if(!cmpxchg(&initialized, 1, 0)) {
		return ;
	}

	fflush(stdout);
	fflush(stderr);

	enable_perf_stats = 0;

	shutdown_log();

#ifdef DISTRIBUTED
	shutdown_rpc();

	//free read cache memory
	//free(g_fcache_base);

#if MLFS_LEASE
	shutdown_lease_protocol();
#endif
#endif

	enable_perf_stats = _enable_perf_stats;

	if (enable_perf_stats)
		show_libfs_stats();

	/*
	ret = munmap(mlfs_slab_pool_shared, SHM_SIZE);
	if (ret == -1)
		panic("cannot unmap shared memory\n");

	ret = close(shm_fd);
	if (ret == -1)
		panic("cannot close shared memory\n");
	*/

	return ;
}

#ifdef USE_SLAB
void mlfs_slab_init(uint64_t pool_size)
{
	uint8_t *pool_space;

	// MAP_SHARED is used to share memory in case of fork.
	pool_space = (uint8_t *)mmap(0, pool_size, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);

	mlfs_assert(pool_space);

	if (madvise(pool_space, pool_size, MADV_HUGEPAGE) < 0)
		panic("cannot do madvise for huge page\n");

	mlfs_slab_pool = (ncx_slab_pool_t *)pool_space;
	mlfs_slab_pool->addr = pool_space;
	mlfs_slab_pool->min_shift = 3;
	mlfs_slab_pool->end = pool_space + pool_size;

	ncx_slab_init(mlfs_slab_pool);
}
#endif

void debug_init(void)
{
#ifdef MLFS_LOG
	log_fd = open(LOG_PATH, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
#endif
}

void shared_slab_init(uint8_t _shm_slab_index)
{
	/* TODO: make the following statment work */
	/* shared memory is used for 2 slab regions.
	 * At the beginning, The first region is used for allocating lru list.
	 * After libfs makes a digest request or lru update request, libfs must free
	 * a current lru list and start build new one. Instead of iterating lru list,
	 * Libfs reintialize slab to the second region and initialize head of lru.
	 * This is because kernel FS might be still absorbing the LRU list
	 * in the first region.(kernel FS sends ack of digest and starts absoring
	 * the LRU list to reduce digest wait time.)
	 * Likewise, the second region is toggle to the first region
	 * when it needs to build a new list.
	 */
	mlfs_slab_pool_shared = (ncx_slab_pool_t *)(shm_base + 4096);

	mlfs_slab_pool_shared->addr = (shm_base + 4096) + _shm_slab_index * (SHM_SIZE / 2);
	mlfs_slab_pool_shared->min_shift = 3;
	mlfs_slab_pool_shared->end = mlfs_slab_pool_shared->addr + (SHM_SIZE / 2);

	ncx_slab_init(mlfs_slab_pool_shared);
}

static void shared_memory_init(void)
{
	shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
	if (shm_fd == -1)
		panic("cannot open shared memory\n");

	// the first 4096 is reserved for lru_head array.
	shm_base = (uint8_t *)mmap(SHM_START_ADDR,
			SHM_SIZE + 4096,
			PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED,
			shm_fd, 0);
	if (shm_base == MAP_FAILED)
		panic("cannot map shared memory\n");

	shm_slab_index = 0;
	shared_slab_init(shm_slab_index);

	bandwidth_consumption = (uint64_t *)shm_base;

	lru_heads = (struct list_head *)shm_base + 128;

	INIT_LIST_HEAD(&lru_heads[g_log_dev]);

#if MLFS_LEASE
	//shm_lease = (uint64_t *) shm_base + 4096;
#endif
}

static void cache_init(void)
{
	inode_hash = NULL;
	dlookup_hash = NULL;
	lru_hash = NULL;
#if MLFS_LEASE
	lease_table = SharedTable_mock();
	assert(lease_table);
#endif

#ifdef DISTRIBUTED
	int ret;

	//Note: we currently assign cache to pre-allocated memory; this allows us to
	//register the memory location to the RDMA NIC (useful for preventing
	//write amplification during remote reads)
	mlfs_debug("allocating %u blocks for DRAM read cache\n", g_max_read_cache_blocks);
	g_fcache_bitmap = (unsigned long *)
		mlfs_zalloc(BITS_TO_LONGS(g_max_read_cache_blocks) * sizeof(unsigned long));
	ret = posix_memalign((void **)&g_fcache_base, sysconf(_SC_PAGESIZE),
			(g_max_read_cache_blocks << g_block_size_shift));
	if(ret)
		panic("failed to allocate read cache memory\n");
#endif
	INIT_LIST_HEAD(&g_fcache_head.lru_head);
	g_fcache_head.n = 0;
}

static void locks_init(void)
{
	pthread_rwlockattr_t rwlattr;

	pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED);

	icache_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));
	icreate_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));
	//icache_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));

	dlookup_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));
	invalidate_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));
	g_fcache_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));

	shm_slab_rwlock = (pthread_rwlock_t *)mlfs_alloc(sizeof(pthread_rwlock_t));
	shm_lru_rwlock = (pthread_rwlock_t *)mlfs_alloc(sizeof(pthread_rwlock_t));

	g_debug_rwlock = (pthread_rwlock_t *)mlfs_zalloc(sizeof(pthread_rwlock_t));

	pthread_rwlock_init(icache_rwlock, &rwlattr);
	pthread_rwlock_init(icreate_rwlock, &rwlattr);
	pthread_rwlock_init(dlookup_rwlock, &rwlattr);
	pthread_rwlock_init(invalidate_rwlock, &rwlattr);
	pthread_rwlock_init(g_fcache_rwlock, &rwlattr);

	pthread_rwlock_init(shm_slab_rwlock, &rwlattr);
	pthread_rwlock_init(shm_lru_rwlock, &rwlattr);

	pthread_rwlock_init(g_debug_rwlock, &rwlattr);
}

// defined in distributed/rpc_interface.h
struct peer_id* hot_replicas = NULL;
struct peer_id* hot_backups = NULL;
struct peer_id* cold_backups = NULL;

#ifdef DISTRIBUTED
static void mlfs_rpc_init(void) {

	//set memory regions to be registered by rdma device
	int n_regions = 3;
	struct mr_context *mrs = (struct mr_context *) mlfs_zalloc(sizeof(struct mr_context) * n_regions);

	// TODO: Optimize registrations. current implementation registers unused and sometimes overlapping memory regions
	for(int i=0; i<n_regions; i++) {
		switch(i) {
			case MR_NVM_LOG: {
				mrs[i].type = MR_NVM_LOG;
				//mrs[i].addr = (uint64_t)g_bdev[g_log_dev]->map_base_addr;
				mrs[i].addr = (uint64_t)g_bdev[g_log_dev]->map_base_addr + (disk_sb[g_log_dev].log_start << g_block_size_shift);
				//mrs[i].length = dev_size[g_root_dev];
				mrs[i].length = ((disk_sb[g_log_dev].nlog) << g_block_size_shift);
				break;
			}
			case MR_NVM_SHARED: {
				mrs[i].type = MR_NVM_SHARED;
				mrs[i].addr = (uint64_t)g_bdev[g_root_dev]->map_base_addr;
				mrs[i].length = (disk_sb[g_root_dev].datablock_start - disk_sb[g_root_dev].inode_start << g_block_size_shift);
				break;
			}
			case MR_DRAM_CACHE: {
				mrs[i].type = MR_DRAM_CACHE;
				mrs[i].addr = (uint64_t) g_fcache_base;
				mrs[i].length = (g_max_read_cache_blocks << g_block_size_shift);
				break;
			}
			case MR_DRAM_BUFFER:
				//do nothing
			default:
				break;
		}
	}

	char *port = getenv("PORTNO");

	if(!port) {
		// initialize rpc module
		port = (char *)mlfs_zalloc(sizeof(char)*10);
		sprintf(port, "%d", 12345);
	}

  /* sig_cb defined in log/log.c */
	init_rpc(mrs, n_regions, port, signal_callback);
	init_log(g_self_id);
}

#endif

// declared in distributed/peer.h
config_t* get_cache_conf()
{
  static config_t cache_conf;
  return &cache_conf;
}

void init_fs(void)
{
  setup_replica_array(&hot_replicas, &hot_backups, &cold_backups);

  init_ssd_emul();
  printf("initialized ssd emulation on libfs.\n");

  init_rcache();
  printf("initialized rcache on libfs.\n");

#ifdef USE_SLAB
	unsigned long memsize_gb = 4;
#endif

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = signal_shutdown_fs;
	sigaction(SIGUSR1, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	if(!cmpxchg(&initialized, 0, 1)) {
		const char *perf_profile;
		int i;

#ifdef USE_SLAB
		mlfs_slab_init(memsize_gb << 30);
#endif

		// This is allocated from slab, which is shared
		// between parent and child processes.
		disk_sb = (struct disk_superblock *)mlfs_zalloc(
				sizeof(struct disk_superblock) * (g_n_devices + 1));

		for (i = 0; i < g_n_devices + 1; i++)
			sb[i] = (struct super_block *)mlfs_zalloc(sizeof(struct super_block));
    
		device_init();

		debug_init();

		cache_init();

		//shared_memory_init();

		locks_init();

		read_superblock(g_root_dev);
#ifdef USE_SSD
		read_superblock(g_ssd_dev);
#endif
#ifdef USE_HDD
		read_superblock(g_hdd_dev);
#endif

		// read superblock for log if it's on a separate device
		if(g_log_dev != g_root_dev)
			read_superblock(g_log_dev);

		mlfs_file_init();

#ifdef DISTRIBUTED
		mlfs_rpc_init();
#else
		init_log();
#endif
		read_root_inode();

		mlfs_info("LibFS is initialized on dev %d\n", g_log_dev);

		perf_profile = getenv("MLFS_PROFILE");

		//FIXME: only for testing
		//perf_profile = 1;

		if (perf_profile){
      printf("Enabled perf stats.\n");
			enable_perf_stats = 1;
		} else {
			enable_perf_stats = 0;
    }

		memset(&g_perf_stats, 0, sizeof(libfs_stat_t));

		clock_speed_mhz = get_cpu_clock_speed();

    // initialize cache configuration
    // a bad idea in libfs, but for simplicity it's probably fine.
    init_conf(get_cache_conf());
    start_appl_client(get_cache_conf());
	}
	else
		mlfs_printf("LibFS already initialized. Skipping..%s\n", "");
}

///////////////////////////////////////////////////////////////////////
// Physical block management

void read_superblock(uint8_t dev)
{
	uint32_t inum;
	int ret;
	struct buffer_head *bh;
	struct dinode dip;

	// 1 is superblock address
	bh = bh_get_sync_IO(dev, 1, BH_NO_DATA_ALLOC);

	bh->b_size = g_block_size_bytes;
	bh->b_data = mlfs_zalloc(g_block_size_bytes);

	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(dev, 1);

	if (!bh)
		panic("cannot read superblock\n");

	memmove(&disk_sb[dev], bh->b_data, sizeof(struct disk_superblock));

	mlfs_debug("[dev %d] superblock: size %lu nblocks %lu ninodes %u "
			"inodestart %lu bmap start %lu datablock_start %lu\n",
			dev,
			disk_sb[dev].size,
			disk_sb[dev].ndatablocks,
			disk_sb[dev].ninodes,
			disk_sb[dev].inode_start,
			disk_sb[dev].bmap_start,
			disk_sb[dev].datablock_start);

	sb[dev]->ondisk = &disk_sb[dev];

	sb[dev]->s_inode_bitmap = (unsigned long *)
		mlfs_zalloc(BITS_TO_LONGS(disk_sb[dev].ninodes) * sizeof(unsigned long));

	if (dev == g_root_dev) {
		// setup inode allocation bitmap.
		for (inum = 1; inum < disk_sb[dev].ninodes; inum++) {
			read_ondisk_inode(inum, &dip);

			if (dip.itype != 0)
				bitmap_set(sb[dev]->s_inode_bitmap, inum, 1);
		}
	}

	mlfs_free(bh->b_data);
	bh_release(bh);
}

void read_root_inode()
{
	struct dinode _dinode;
	struct inode *ip;

	read_ondisk_inode(ROOTINO, &_dinode);
	mlfs_assert(_dinode.itype == T_DIR);

	ip = ialloc(ROOTINO, &_dinode);
	iunlock(ip);
}

int read_ondisk_inode(uint32_t inum, struct dinode *dip)
{
	int ret;
	struct buffer_head *bh;
	addr_t inode_block;

	inode_block = get_inode_block(g_root_dev, inum);
	bh = bh_get_sync_IO(g_root_dev, inode_block, BH_NO_DATA_ALLOC);

	bh->b_size = sizeof(struct dinode);
	bh->b_data = (uint8_t *)dip;
	bh->b_offset = sizeof(struct dinode) * (inum % IPB);
	bh_submit_read_sync_IO(bh);
	mlfs_io_wait(g_root_dev, 1);

	return 0;
}

//called after a digest response to sync inodes from nvm shared area
int sync_all_inode_ext_trees()
{
	struct inode *inode, *tmp;

	invalidate_bh_cache(g_root_dev);
#ifdef USE_SSD
	invalidate_bh_cache(g_ssd_dev);
#endif
#ifdef USE_HDD
	invalidate_bh_cache(g_hdd_dev);
#endif

	// TODO: optimize this. Now sync all inodes in the inode_hash.
	// As the optimization, Kernfs sends inodes lists (via shared memory),
	// and Libfs syncs inodes based on the list.
	HASH_ITER(hash_handle, inode_hash, inode, tmp) {
		ilock(inode);
		if (!(inode->flags & I_DELETING)) {
			if (inode->itype == T_FILE || inode->itype == T_DIR)
				sync_inode_ext_tree(inode);
			else {
				//pthread_mutex_unlock(&inode->i_mutex);
				sync_inode_ext_tree(inode);
				//panic("unsupported inode type\n");
			}
		}
		iunlock(inode);
	}
	return 0;
}

int sync_inode_ext_tree(struct inode *inode)
{
	//if (inode->flags & I_RESYNC) {
		struct buffer_head *bh;
		struct dinode dinode;

		read_ondisk_inode(inode->inum, &dinode);

		memmove(inode->l1.addrs, dinode.l1_addrs, sizeof(addr_t) * (NDIRECT + 1));
#ifdef USE_SSD
		memmove(inode->l2.addrs, dinode.l2_addrs, sizeof(addr_t) * (NDIRECT + 1));
#endif
#ifdef USE_HDD
		memmove(inode->l3.addrs, dinode.l3_addrs, sizeof(addr_t) * (NDIRECT + 1));
#endif

		/*
		if (inode->itype == T_DIR)
			mlfs_info("resync inode (DIR) %u is done\n", inode->inum);
		else
			mlfs_info("resync inode %u is done\n", inode->inum);
		*/
	//}

	inode->flags &= ~I_RESYNC;
	inode->flags &= ~I_DIRTY;
	return 0;
}

// Allocate an "in-memory" inode. Returned inode is locked.
struct inode* ialloc(uint32_t inum, struct dinode *dip)
{
	int ret;
	struct inode *ip;
	pthread_rwlockattr_t rwlattr;

	ip = icache_find(inum);
	if (!ip)
		ip = icache_alloc_add(inum);

	ilock(ip);
	ip->_dinode = (struct dinode *)ip;

	if (ip->flags & I_DELETING) {
		// There is the case where unlink in the update log is not yet digested.
		// Then, ondisk inode does contain a stale information.
		// So, skip syncing with ondisk inode.
		memset(ip->_dinode, 0, sizeof(struct dinode));
		ip->inum = inum;
		mlfs_debug("reuse inode - inum %u\n", inum);
	} else {
		sync_inode_from_dinode(ip, dip);
		mlfs_debug("get inode - inum %u\n", inum);
	}

	ip->flags = 0;
	ip->flags |= I_VALID;
	ip->i_ref = 1;
	ip->i_dirty_dblock = RB_ROOT;
	ip->i_sb = sb;

	pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED);
	pthread_rwlock_init(&ip->fcache_rwlock, &rwlattr);

#ifdef KLIB_HASH
	ip->fcache_hash = kh_init(fcache);
	ip->de_cache_hash = kh_init(de_cache);
#endif

	ip->n_fcache_entries = 0;
	ip->n_de_cache_entry = 0;

	ip->fcache = NULL;
	ip->de_cache = NULL;

	pthread_rwlock_init(&ip->de_cache_rwlock, &rwlattr);

	INIT_LIST_HEAD(&ip->i_slru_head);

	bitmap_set(sb[g_root_dev]->s_inode_bitmap, inum, 1);

	return ip;
}

// Allocate a new on-disk inode with the given type on device dev.
// A free inode has a type of zero. Returned inode is locked.
struct inode* icreate(uint8_t type)
{
	uint32_t inum;
	int ret;
	struct dinode dip;
	struct inode *ip;
	pthread_rwlockattr_t rwlattr;

	pthread_rwlock_wrlock(icreate_rwlock);

#ifndef DISTRIBUTED
	// FIXME: hard coded. used for testing multiple applications.
	if (g_log_dev == 4)
		inum = find_next_zero_bit(sb[g_root_dev]->s_inode_bitmap,
				sb[g_root_dev]->ondisk->ninodes, 1);
	else
		inum = find_next_zero_bit(sb[g_root_dev]->s_inode_bitmap,
				sb[g_root_dev]->ondisk->ninodes, (g_log_dev-4)*NINODES/4);
#else
	assert(g_self_id >= 0);
	mlfs_debug("finding empty inode number in slice(start:%u size:%u)\n", (g_self_id)*NINODES/g_n_max_libfs, sb[g_root_dev]->ondisk->ninodes);
	inum = find_next_zero_bit(sb[g_root_dev]->s_inode_bitmap,
				sb[g_root_dev]->ondisk->ninodes, (g_self_id)*NINODES/g_n_max_libfs);
	mlfs_debug("creating inode with inum %u\n", inum);
#endif

	read_ondisk_inode(inum, &dip);

	// Clean (in-memory in block cache) ondisk inode.
	// At this point, storage and in-memory state diverges.
	// Libfs does not write dip directly, and kernFS will
	// update the dip on storage when digesting the inode.
	setup_ondisk_inode(&dip, type);

	ip = ialloc(inum, &dip);

	pthread_rwlock_unlock(icreate_rwlock);

	return ip;
}

/* Inode (in-memory) cannot be freed at this point.
 * If the inode is freed, libfs will read on-disk inode from
 * read-only area. This cause a problem since the on-disk deletion
 * is not applied yet in kernfs (before digest).
 * idealloc() marks the inode as deleted (I_DELETING). The inode is
 * removed from icache when digesting the inode.
 * from icache. libfs will free the in-memory inode after digesting
 * a log of deleting the inode.
 */
int idealloc(struct inode *inode)
{
	struct inode *_inode;
	lru_node_t *l, *tmp;

	ilock(inode);
	mlfs_assert(inode->i_ref < 2);

	inode->flags &= ~I_BUSY;
	inode->size = 0;
	inode->flags |= I_DELETING;
	inode->itype = 0;

	//fcache_del_all(inode);
	de_cache_del_all(inode);

	pthread_rwlock_destroy(&inode->de_cache_rwlock);
	pthread_rwlock_destroy(&inode->fcache_rwlock);

	iunlock(inode);

	mlfs_debug("dealloc inum %u\n", inode->inum);

#if 0 // TODO: do this in parallel by assigning a background thread.
	list_for_each_entry_safe(l, tmp, &inode->i_slru_head, list) {
		HASH_DEL(lru_hash_head, l);
		list_del(&l->list);
		mlfs_free_shared(l);
	}
#endif

	return 0;
}

// Copy a modified in-memory inode to log.
void iupdate(struct inode *ip)
{
	mlfs_assert(!(ip->flags & I_DELETING));

	if (!(ip->dinode_flags & DI_VALID))
		panic("embedded _dinode is invalid\n");

	if (ip->_dinode != (struct dinode *)ip)
		panic("_dinode pointer is incorrect\n");

	mlfs_get_time(&ip->mtime);
	ip->atime = ip->mtime;

	add_to_loghdr(L_TYPE_INODE_UPDATE, ip, 0,
			sizeof(struct dinode), NULL, 0);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
struct inode* iget(uint32_t inum)
{
	struct inode *ip;

	ip = icache_find(inum);

	if (ip) {
		if ((ip->flags & I_VALID) && (ip->flags & I_DELETING))
			return NULL;

		pthread_mutex_lock(&ip->i_mutex);
		ip->i_ref++;
		pthread_mutex_unlock(&ip->i_mutex);
	} else {
		struct dinode dip;
		// allocate new in-memory inode
		mlfs_debug("allocate new inode by iget %u\n", inum);
		read_ondisk_inode(inum, &dip);

		ip = ialloc(inum, &dip);

		iunlock(ip);
	}

	return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode* idup(struct inode *ip)
{
	panic("does not support idup yet\n");

	return ip;
}

void ilock(struct inode *ip)
{
	pthread_mutex_lock(&ip->i_mutex);
	ip->flags |= I_BUSY;
}

void iunlock(struct inode *ip)
{
	pthread_mutex_unlock(&ip->i_mutex);
	ip->flags &= ~I_BUSY;
}

/* iput does not deallocate inode. it just drops reference count.
 * An inode is explicitly deallocated by ideallc()
 */
void iput(struct inode *ip)
{
	pthread_mutex_lock(&ip->i_mutex);

	mlfs_muffled("iput num %u ref %u nlink %u\n",
			ip->inum, ip->ref, ip->nlink);

	ip->i_ref--;

	pthread_mutex_unlock(&ip->i_mutex);
}

// Common idiom: unlock, then put.
void iunlockput(struct inode *ip)
{
	iunlock(ip);
	iput(ip);
}

/* Get block addresses from extent trees.
 * return = 0, if all requested offsets are found.
 * return = -EAGAIN, if not all blocks are found.
 *
 */
int bmap(struct inode *ip, struct bmap_request *bmap_req)
{
	int ret = 0;
	handle_t handle;
	offset_t offset = bmap_req->start_offset;

	if (ip->itype == T_DIR) {
		/*
		bmap_req->block_no = ip->l1.addrs[(offset >> g_block_size_shift)];
		bmap_req->blk_count_found = 1;
		bmap_req->dev = ip->dev;
		*/

		struct mlfs_map_blocks map;
		map.m_lblk = (offset >> g_block_size_shift);
		map.m_len = bmap_req->blk_count;
		map.m_flags = 0;

		handle.dev = g_root_dev;
		mlfs_debug("mlfs_ext_get_blocks: %s\n", "start");
		ret = mlfs_ext_get_blocks(&handle, ip, &map, 0);
		mlfs_debug("mlfs_ext_get_blocks: ret %d\n", ret);
		bmap_req->blk_count_found = ret;
		bmap_req->dev = g_root_dev;
		bmap_req->block_no = map.m_pblk;

		return 0;
	}
	/*
	if (ip->itype == T_DIR) {
		handle.dev = g_root_dev;
		struct mlfs_map_blocks map;

		map.m_lblk = (offset >> g_block_size_shift);
		map.m_len = bmap_req->blk_count;

		ret = mlfs_ext_get_blocks(&handle, ip, &map, 0);

		if (ret == bmap_req->blk_count)
			bmap_req->blk_count_found = ret;
		else
			bmap_req->blk_count_found = 0;

		return 0;
	}
	*/
	else if (ip->itype == T_FILE) {
		struct mlfs_map_blocks map;

		map.m_lblk = (offset >> g_block_size_shift);
		map.m_len = bmap_req->blk_count;
		map.m_flags = 0;

		// L1 search
		handle.dev = g_root_dev;
		ret = mlfs_ext_get_blocks(&handle, ip, &map, 0);
		mlfs_debug("mlfs_ext_get_blocks: ret %d\n", ret);
		// all blocks are found in the L1 tree
		if (ret != 0) {
			bmap_req->blk_count_found = ret;
			bmap_req->dev = g_root_dev;
			bmap_req->block_no = map.m_pblk;

			if (ret == bmap_req->blk_count) {
				mlfs_debug("[dev %d] Get all offset %lx: blockno %lx from NVM\n",
						g_root_dev, offset, map.m_pblk);
				return 0;
			} else {
				mlfs_debug("[dev %d] Get partial offset %lx: blockno %lx from NVM\n",
						g_root_dev, offset, map.m_pblk);
				return -EAGAIN;
			}
		}

		// L2 search
#ifdef USE_SSD
		if (ret == 0) {
#if MLFS_MASTER && defined(SSD_READ_REDIRECT)
			// Optimization: since we redirect all ssd reads to remote node
			// we can skip search and simply pretend that all data is on SSD

			// FIXME: But what if we want to simultaneously read locally and
			// remotely (i.e. speculative read)?
			bmap_req->blk_count_found = bmap_req->blk_count;
			bmap_req->dev = g_ssd_dev;
			return 0;
#endif
			map.m_lblk = (offset >> g_block_size_shift);
			map.m_len = bmap_req->blk_count;
			map.m_flags = 0;

			handle.dev = g_ssd_dev;
			ret = mlfs_ext_get_blocks(&handle, ip, &map, 0);

			mlfs_debug("search l2 tree: ret %d\n", ret);

#ifndef USE_HDD
			/* No blocks are found in all trees */
			if (ret == 0)
				return -EIO;
#else
			/* To L3 tree search */
			if (ret == 0)
				goto L3_search;
#endif
			bmap_req->blk_count_found = ret;
			bmap_req->dev = g_ssd_dev;
			bmap_req->block_no = map.m_pblk;

			mlfs_debug("[dev %d] Get offset %lu: blockno %lu from SSD\n",
					g_ssd_dev, offset, map.m_pblk);

			if (ret != bmap_req->blk_count)
				return -EAGAIN;
			else
				return 0;
		}
#endif
		// L3 search
#ifdef USE_HDD
L3_search:
		if (ret == 0) {
#if MLFS_MASTER && defined(HDD_READ_REDIRECT)
			// Optimization: since we redirect all hdd reads to remote node
			// we can skip search and simply pretend that all data is on HDD

			// FIXME: But what if we want to simultaneously read locally and
			// remotely (i.e. speculative read)?
			bmap_req->blk_count_found = bmap_req->blk_count;
			bmap_req->dev = g_hdd_dev;
			return 0;
#endif

			map.m_lblk = (offset >> g_block_size_shift);
			map.m_len = bmap_req->blk_count;
			map.m_flags = 0;

			handle.dev = g_hdd_dev;
			ret = mlfs_ext_get_blocks(&handle, ip, &map, 0);

			mlfs_debug("search l3 tree: ret %d\n", ret);

			/* No blocks are found in all trees */
			if (ret == 0)
				return -EIO;

			bmap_req->blk_count_found = ret;
			bmap_req->dev = g_hdd_dev;
			bmap_req->block_no = map.m_pblk;

			mlfs_debug("[dev %d] Get offset %lx: blockno %lx from SSD\n",
					g_ssd_dev, offset, map.m_pblk);

			if (ret != bmap_req->blk_count)
				return -EAGAIN;
			else
				return 0;
		}
#endif
	}

	return -EIO;
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
int itrunc(struct inode *ip, offset_t length)
{
	int ret = 0;
	struct fcache_block *fc_block;
	offset_t key;

	mlfs_assert(ip);
	ip->size = length;
	iupdate(ip);

	if (length == 0) {
		fcache_del_all(ip);
	} else if (length < ip->size) {
		// for 0 to ip->size

		/* invalidate all data pointers for log block.
		 * If libfs only takes care of zero trucate case,
		 * dropping entire hash table is OK.
		 * It considers non-zero truncate */
		for (key = 0; key <= ip->size >> g_block_size_shift; key++) {
			fc_block = fcache_find(ip, key);
			if (fc_block && fc_block->is_data_cached) {
				list_del(&fc_block->l);
				fcache_del(ip, fc_block);
				mlfs_free(fc_block);
			}
		}
	}


	return ret;
}

void stati(struct inode *ip, struct stat *st)
{
	mlfs_assert(ip);

	st->st_dev = g_root_dev;
	st->st_ino = ip->inum;
	st->st_mode = 0;
			if(ip->itype == T_DIR)
		st->st_mode |= S_IFDIR;
	else
		st->st_mode |= S_IFREG;

	st->st_nlink = ip->nlink;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = ip->size;
	st->st_blksize = g_block_size_bytes;
	// This could be incorrect if there is file holes.
	st->st_blocks = ip->size / 512;

	st->st_mtime = (time_t)ip->mtime.tv_sec;
	st->st_ctime = (time_t)ip->ctime.tv_sec;
	st->st_atime = (time_t)ip->atime.tv_sec;
}

// TODO: Now, eviction is simply discarding. Extend this function
// to evict data to the update log.
static void evict_read_cache(struct inode *inode, uint32_t n_entries_to_evict)
{
	uint32_t i = 0, j = 0;
	struct fcache_block *_fcache_block, *tmp;

	list_for_each_entry_safe_reverse(_fcache_block, tmp,
			&g_fcache_head.lru_head, l) {
		if (i > n_entries_to_evict)
			break;

		if (_fcache_block->is_data_cached) {

			mlfs_debug("evicting inum:%u off:%lu from read cache [# blocks: used %lu free %lu]\n",
					_fcache_block->inum, _fcache_block->key, g_fcache_head.n,
								g_max_read_cache_blocks - g_fcache_head.n);

#ifdef DISTRIBUTED
			j = ((_fcache_block->data - g_fcache_base) >> g_block_size_shift);
			bitmap_clear(g_fcache_bitmap, j, 1);
#else
			mlfs_free(_fcache_block->data);
#endif

			list_del(&_fcache_block->l);

			// FIXME: passing inode here is meaningless
			// the fcache entry could belong to any inode
			// instead, get correct inode using _fcache_block->inum
			fcache_del(inode, _fcache_block);

			mlfs_free(_fcache_block);
			g_fcache_head.n--;
			i++;
      
      g_perf_stats.read_cache_evict++;
		}
	}

	//assert(g_fcache_head.n == bitmap_weight(g_fcache_bitmap, g_max_read_cache_blocks))
}


// add read cache entry and evict older entries if we exceed cache size
// if data pointer is provided, we copy data over to cache buffer;
// otherwise, if pointer is NULL, we simply return a dataless _fcache_block
static struct fcache_block *add_to_read_cache(struct inode *inode,
		offset_t off, uint8_t *data)
{
	struct fcache_block *_fcache_block;

	if (g_fcache_head.n + 1 > g_max_read_cache_blocks) {
		evict_read_cache(inode, g_fcache_head.n - g_max_read_cache_blocks);
	}

	_fcache_block = fcache_find(inode, (off >> g_block_size_shift));

	if (!_fcache_block) {
		_fcache_block = fcache_alloc_add(inode, (off >> g_block_size_shift));
		g_fcache_head.n++;
	} else {
		mlfs_assert(_fcache_block->is_data_cached == 0);
	}

	_fcache_block->is_data_cached = 1;

#ifdef DISTRIBUTED
	int j = find_next_zero_bit(g_fcache_bitmap, g_max_read_cache_blocks-1, 0);
	bitmap_set(g_fcache_bitmap, j, 1);
	_fcache_block->data = g_fcache_base + (j << g_block_size_shift);
	//mlfs_debug("found empty cache with base %p offset %d pos %d\n",
	//		g_fcache_base, (j << g_block_size_shift), j);
#else
	_fcache_block->data = mlfs_zalloc(g_block_size_bytes);
#endif

	mlfs_debug("adding inum:%u off:%lu to read cache at @ %p [# blocks: used %lu free %lu]\n",
			inode->inum, off, _fcache_block->data, g_fcache_head.n,
			g_max_read_cache_blocks - g_fcache_head.n);

	if(data)
		memcpy(_fcache_block->data, data, g_block_size_bytes);

	list_move(&_fcache_block->l, &g_fcache_head.lru_head);

  g_perf_stats.read_cache_add++;

	return _fcache_block;
}

int check_log_invalidation(struct fcache_block *_fcache_block)
{
	int ret = 0;
	int version_diff = g_fs_log->avail_version - _fcache_block->log_version;
	addr_t log_addr = 0;
	struct fcache_log *logblk, *tmp;

	mlfs_assert(version_diff >= 0);

	//fcache entry needs to have a reason to exist (sanity check)
	mlfs_assert(_fcache_block->log_entries || _fcache_block->is_data_cached);

	pthread_rwlock_wrlock(invalidate_rwlock);

	// fcache is used for log pointers (patching case).
	if(_fcache_block->log_entries) {
		if(version_diff > 1)
			fcache_log_del_all(_fcache_block);
		else {
			//only remove outdated entries
			list_for_each_entry_safe_reverse(logblk, tmp,
				&_fcache_block->log_head, l) {
				version_diff = g_fs_log->avail_version - logblk->version;
				if(version_diff > 1)
					fcache_log_del(_fcache_block, logblk);
				else if (version_diff == 1 && logblk->addr < g_fs_log->next_avail_header)
					fcache_log_del(_fcache_block, logblk);
			}
		}
	}

	if(_fcache_block->log_entries || _fcache_block->is_data_cached)
		ret = 0;
	else
		ret = 1;

#if 0
	if ((version_diff > 1) || (version_diff == 1 &&
			 _fcache_block->log_addr < g_fs_log->next_avail_header)) {
		mlfs_debug("invalidate: inum %u -> addr %lu\n",
				_fcache_block->inum, _fcache_block->log_addr);
		_fcache_block->log_addr = 0;

		// Delete fcache_block when it is not used for read cache.
		if (!_fcache_block->is_data_cached)
			ret = 1;
		else
			ret = 0;
	}
#endif

	pthread_rwlock_unlock(invalidate_rwlock);

	return ret;
}

int do_unaligned_read(struct inode *ip, struct mlfs_reply *reply, offset_t off, uint32_t io_size, char *path)
{
	int io_done = 0, log_patches = 0, ret;
	offset_t key, off_aligned, off_in_block;
	struct fcache_block *_fcache_block;
	struct fcache_log *fcl, *tmp;
	struct rpc_pending_io *rpc, *_rpc;
	struct rdma_meta_entry *resp, *_resp;
	uint64_t start_tsc;
	uintptr_t src_addr;
	struct buffer_head *bh, *_bh;
	struct list_head io_list_log, remote_io_list;
	struct mlfs_interval req_interval, log_interval, intersect;
	bmap_req_t bmap_req;

	INIT_LIST_HEAD(&io_list_log);
	INIT_LIST_HEAD(&remote_io_list);

	mlfs_debug("unaligned read for : inum %u, offset %lu, io_size %u type: %s\n",
						ip->inum, off, io_size, reply->remote?"remote":"local");

/*
#ifdef DISTRIBUTED
	mlfs_assert(!reply->remote); //remote read rpcs can only ask for 4kb aligned blocks
#endif
*/

	mlfs_assert(io_size < g_block_size_bytes);

	off_aligned = ALIGN_FLOOR(off, g_block_size_bytes);

	off_in_block = off - off_aligned;

	key = (off_aligned >> g_block_size_shift);

	req_interval.start = off_in_block;
	req_interval.size = io_size;

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	_fcache_block = fcache_find(ip, key);

	if (enable_perf_stats) {
		g_perf_stats.l0_search_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.l0_search_nr++;
	}

	if (_fcache_block) {
		ret = check_log_invalidation(_fcache_block);
		if (ret) {
			fcache_del(ip, _fcache_block);
			mlfs_free(_fcache_block);
			_fcache_block = NULL;
		}
	}

	if (_fcache_block) {
		// read cache hit
		if (_fcache_block->is_data_cached) {
        g_perf_stats.read_cache_hit++;
				mlfs_debug("%s\n", "read cache hit");

				if(reply->remote) {
#if MLFS_REPLICA
					resp = create_rdma_entry((uintptr_t)(_fcache_block->data + off_in_block),
							(uintptr_t)(reply->dst + off_in_block),
							io_size, MR_DRAM_CACHE, -1);
					rpc_remote_read_response(reply->sockfd, resp->meta, resp->local_mr, reply->seqn);
					list_del(&resp->head);
					mlfs_free(resp->meta);
					mlfs_free(resp);

					//also send the entire 4k block (but do it seperately to avoid delays)
					resp = create_rdma_entry((uintptr_t) _fcache_block->data, (uintptr_t)(reply->dst),
							g_block_size_bytes, MR_DRAM_CACHE, -1);
					rpc_remote_read_response(reply->sockfd, resp->meta, resp->local_mr, 0);

					list_del(&resp->head);
					mlfs_free(resp->meta);
					mlfs_free(resp);
#endif
				}
				else {
          // check if it is a partial result!
          fill_partials_until(ip, off);

					memmove(reply->dst, _fcache_block->data + off_in_block, io_size);

					// move the fcache entry to head of LRU
					list_move(&_fcache_block->l, &g_fcache_head.lru_head);
				}

			return io_size;
		}
		// still need to search update log in case of patches
		// iterate through all patches and apply them from oldest to newest
		if (_fcache_block->log_entries) {
			//TODO: see if we can coalesce log entries to prevent redundant writes
			mlfs_debug("found cached log pointer(s) for blk %lx [count:%u]\n",
					key, _fcache_block->log_entries);
			list_for_each_entry_safe_reverse(fcl, tmp,
					&_fcache_block->log_head, l) {
				log_interval.start = fcl->offset;
				log_interval.size = fcl->size;
				if(find_intersection(&req_interval, &log_interval, &intersect)) {
					// log entry has something in common with requested block
					// note: we only write common block fragments (to reduce write amplification)

					mlfs_debug("log cache hit [log entry: offset %u(0x%x) size %u]\n",
							fcl->offset, fcl->offset, fcl->size);

					if(reply->remote) {
						/*
						//create rdma response and add to pendings
						src_addr = (uintptr_t)((fcl->addr << g_block_size_shift)
											+ g_bdev[g_fs_log->dev]->map_base_addr);
						resp = create_rdma_entry(src_addr + fcl->offset,  (uintptr_t)(reply->dst)
											+ intersect.start - req_interval.start, fcl->size, MR_NVM_LOG, -1);
						list_move(&resp->head, &remote_io_list);
						*/
						panic("unsupported code path; remote reads shouldn't serve items from the log");
					}
					else {
						bh = bh_get_sync_IO(g_fs_log->dev, fcl->addr, BH_NO_DATA_ALLOC);
						bh->b_offset = intersect.start;
						bh->b_data = reply->dst + intersect.start - req_interval.start;
						bh->b_size = intersect.size;

						list_add_tail(&bh->b_io_list, &io_list_log);
					}

					mlfs_debug("GET from log: offset %u(0x%x) size %u from log_blk %lu\n",
							intersect.start, intersect.start, intersect.size, fcl->addr);
				}
				log_patches++;
			}
			mlfs_assert(log_patches == _fcache_block->log_entries);
		}
	}

	// global shared area search
	bmap_req.start_offset = off_aligned;
	bmap_req.blk_count_found = 0;
	bmap_req.blk_count = 1;

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	// Get block address from shared area.
	ret = bmap(ip, &bmap_req);
  int in_ssd = ssd_has_blk(bmap_req.block_no);

	if (enable_perf_stats) {
		g_perf_stats.tree_search_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.tree_search_nr++;
	}

	if (ret == -EIO)
		goto do_io_unaligned;

	bh = bh_get_sync_IO(bmap_req.dev, bmap_req.block_no, BH_NO_DATA_ALLOC);
	bh->b_size = (bmap_req.blk_count_found << g_block_size_shift);

  // we don't actually migrate things, so shouldn't be in ssd.
  assert(bmap_req.dev == g_root_dev);
  
  if (in_ssd && bmap_req.blk_count_found == 1) {
    g_perf_stats.ssd_hit++;
    /* update caches as if our block WAS in ssd.
     * what kinds of coherence issues can this present?
     *  - probably nothing -> I think, Assise cannot write a block that has been already evicted to ssd.
     * then, just place the (read-only) block in read cache. Invariant is that digestion will never hit it.
     */

		mlfs_assert(_fcache_block == NULL);
		_fcache_block = add_to_read_cache(ip, off_aligned, NULL);
		
    // TODO: mlfs_readahead(g_ssd_dev, bh->b_blocknr, (128 << 10));
    
    // only implement our cache in single-block case.

    struct rcache_req rreq = {.block = bmap_req.block_no, .inode = ip, .offset = off,
                              .io_size = io_size, .dst = _fcache_block->data};
    rcache_read(rreq); // important to pass this by value.

    memmove(reply->dst, _fcache_block->data + (off - off_aligned), io_size);
  } else {
    g_perf_stats.nvm_hit++;
    // regular nvm read.
    bh->b_offset = off - off_aligned;
    bh->b_data = reply->dst;
    bh->b_size = io_size;
    bh_submit_read_sync_IO(bh);
    bh_release(bh);
  }

do_io_unaligned:
	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	// Patch data from log (L0) if up-to-date blocks are in the update log.
	// This is required when partial updates are in the update log.
	list_for_each_entry_safe(bh, _bh, &io_list_log, b_io_list) {
		bh_submit_read_sync_IO(bh);
		bh_release(bh);
	}

	if (enable_perf_stats) {
		g_perf_stats.read_data_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.read_data_nr++;
	}

	return io_size;
}

int do_aligned_read(struct inode *ip, struct mlfs_reply *reply, offset_t off, uint32_t io_size, char *path)
{
	int io_to_be_done = 0, log_patches = 0, ret, i;
	offset_t key, _off, pos;
	struct fcache_block *_fcache_block;
	struct fcache_log *fcl, *tmp;
	uint64_t start_tsc;
	struct buffer_head *bh, *_bh;
	struct rpc_pending_io *rpc, *_rpc;
	struct rdma_meta_entry *resp, *_resp;
				uintptr_t src_addr;
	struct list_head io_list, io_list_log, remote_io_list;
	uint32_t bitmap_size = (io_size >> g_block_size_shift), bitmap_pos;
	struct cache_copy_list copy_list[bitmap_size];
	bmap_req_t bmap_req;

	mlfs_debug("aligned read for : offset %lu, io_size %u path: %s [%s]\n",
						off, io_size, path, reply->remote?"remote":"local");

	DECLARE_BITMAP(io_bitmap, bitmap_size);

	bitmap_set(io_bitmap, 0, bitmap_size);

	memset(copy_list, 0, sizeof(struct cache_copy_list) * bitmap_size);

	INIT_LIST_HEAD(&io_list);
	INIT_LIST_HEAD(&io_list_log);
	INIT_LIST_HEAD(&remote_io_list);

	mlfs_assert(io_size % g_block_size_bytes == 0);

	for (pos = 0, _off = off; pos < io_size;
			pos += g_block_size_bytes, _off += g_block_size_bytes) {
		key = (_off >> g_block_size_shift);

		log_patches = 0;

		if (enable_perf_stats)
			start_tsc = asm_rdtscp();

		_fcache_block = fcache_find(ip, key);

		if (enable_perf_stats) {
			g_perf_stats.l0_search_tsc += (asm_rdtscp() - start_tsc);
			g_perf_stats.l0_search_nr++;
		}

		if (_fcache_block) {
			ret = check_log_invalidation(_fcache_block);
			if (ret) {
				fcache_del(ip, _fcache_block);
				mlfs_free(_fcache_block);
				_fcache_block = NULL;
			}
		}

		if (_fcache_block) {
			// read cache hit
			if (_fcache_block->is_data_cached) {
        g_perf_stats.read_cache_hit++;
				mlfs_debug("read cache hit for block %lu\n", key);

				if(reply->remote) {
#ifdef DISTRIBUTED
					src_addr = (uintptr_t) _fcache_block->data;
					resp = create_rdma_entry(src_addr, (uintptr_t)(reply->dst) + pos,
							g_block_size_bytes, MR_DRAM_CACHE, -1);
					list_move(&resp->head, &remote_io_list);
#else
					panic("undefined codepath\n");
#endif
				}
				else {
          fill_partials_until(ip, off);

					copy_list[pos >> g_block_size_shift].dst_buffer = reply->dst + pos;
					copy_list[pos >> g_block_size_shift].cached_data = _fcache_block->data;
					copy_list[pos >> g_block_size_shift].size = g_block_size_bytes;
					// move the fcache entry to head of LRU
					list_move(&_fcache_block->l, &g_fcache_head.lru_head);
				}

				bitmap_clear(io_bitmap, (pos >> g_block_size_shift), 1);
				io_to_be_done++;
			}
			// still need to search update log in case of patches
			if (_fcache_block->log_entries) {

				addr_t tot_fcl_size = 0;

				//TODO: see if we can coalesce log entries to prevent redundant writes
				mlfs_debug("log cache hit[n:%u] for block %lu\n", _fcache_block->log_entries, key);

				list_for_each_entry_safe_reverse(fcl, tmp, &_fcache_block->log_head, l) {
					mlfs_debug("GET from update log: blockno %lx offset %u(0x%x) size %u\n",
							fcl->addr, fcl->offset, fcl->offset, fcl->size);

					if(reply->remote) {
#ifdef DISTRIBUTED
						//create rdma response and add to pendings
						src_addr = (uintptr_t)((fcl->addr << g_block_size_shift)
											+ g_bdev[g_fs_log->dev]->map_base_addr);
						resp = create_rdma_entry(src_addr + fcl->offset,  (uintptr_t)(reply->dst)
											+ pos + fcl->offset, fcl->size, MR_NVM_LOG, -1);
						list_move(&resp->head, &remote_io_list);
#else
						panic("undefined codepath\n");
#endif
					}
					else {
						bh = bh_get_sync_IO(g_fs_log->dev, fcl->addr, BH_NO_DATA_ALLOC);
						bh->b_offset = fcl->offset;
						bh->b_data = reply->dst + pos + fcl->offset;
						bh->b_size = fcl->size;
						list_add_tail(&bh->b_io_list, &io_list_log);

						tot_fcl_size += fcl->size;
					}

					log_patches++;
				}
				//mlfs_info("TEST log_patches: %u | fcache_block->log_entries: %u\n",
				//		log_patches, _fcache_block->log_entries);
				mlfs_assert(log_patches == _fcache_block->log_entries);

				//FIXME: we shouldn't clear bitmap as log patches can be partial
				//(i.e. less than g_block_size_bytes)
				//Should we compute if log patches have filled entire block?

				if(tot_fcl_size == g_block_size_bytes) {
					bitmap_clear(io_bitmap, (pos >> g_block_size_shift), 1);
					io_to_be_done++;
				}
			}
		}
	}

	//all data came from L0 or read cache; skip global search
	if(bitmap_weight(io_bitmap, bitmap_size) == 0)  {
		goto do_io_aligned;
	}

	/*
	// All data come from the update log.
	if (bitmap_weight(io_bitmap, bitmap_size) == 0)  {
		list_for_each_entry_safe(bh, _bh, &io_list_log, b_io_list) {
			bh_submit_read_sync_IO(bh);
			bh_release(bh);
		}
		return io_size;
	}
	*/

do_global_search:
	_off = off + (find_first_bit(io_bitmap, bitmap_size) << g_block_size_shift);
	pos = find_first_bit(io_bitmap, bitmap_size) << g_block_size_shift;
	bitmap_pos = find_first_bit(io_bitmap, bitmap_size);

	// global shared area search
	bmap_req.start_offset = _off;
	bmap_req.blk_count =
		find_next_zero_bit(io_bitmap, bitmap_size, bitmap_pos) - bitmap_pos;
	bmap_req.dev = 0;
	bmap_req.block_no = 0;
	bmap_req.blk_count_found = 0;

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	// Get block address from shared area.
	ret = bmap(ip, &bmap_req);
  int in_ssd = ssd_has_blk(bmap_req.block_no);

	if (enable_perf_stats) {
		g_perf_stats.tree_search_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.tree_search_nr++;
	}

	if (ret == -EIO) {
		if (bmap_req.blk_count_found != bmap_req.blk_count) {
			//panic("could not found blocks in any storage layers\n");
			mlfs_debug("inum %u - count not find block in any storage layer\n",
					ip->inum);
		}
		goto do_io_aligned;
	}
	
	// NVM case: no caching for local reads.
  //TODO: update shared device LRU to reflect block being touched by read

  mlfs_debug("Found in NVM: %s\n", path);

  // again, we don't actually migrate things.
  assert(bmap_req.dev == g_root_dev);

  // this is incorrect- assumes that first block not being in ssd implies others
  // but if we restrict ourselves to single-block queries at a time, it's fine.
  if (!in_ssd || bmap_req.blk_count_found != 1) {
    if (!in_ssd) {
      g_perf_stats.nvm_hit += bmap_req.blk_count_found;
    }

    bh = bh_get_sync_IO(bmap_req.dev, bmap_req.block_no, BH_NO_DATA_ALLOC);
    bh->b_size = (bmap_req.blk_count_found << g_block_size_shift);

    bh->b_offset = 0;
    bh->b_data = reply->dst + pos;
    bh->b_size = (bmap_req.blk_count_found << g_block_size_shift);

    list_add_tail(&bh->b_io_list, &io_list);

  } else {
    // ssd and block_ct =1!

    // TODO: readahead
		
    g_perf_stats.ssd_hit++;
    _fcache_block = add_to_read_cache(ip, _off, NULL);
    
    struct rcache_req rreq = {.block = bmap_req.block_no, .inode = ip, .offset = 0, 
                              .io_size = g_block_size_bytes, .dst = _fcache_block->data};
    rcache_read(rreq); // important to pass this by value.

    copy_list[0].dst_buffer = reply->dst;
    copy_list[0].cached_data = _fcache_block->data;
    copy_list[0].size = g_block_size_bytes;
  }

	/* EAGAIN happens in two cases:
	 * 1. A size of extent is smaller than bmap_req.blk_count. In this
	 * case, subsequent bmap call starts finding blocks in next extent.
	 * 2. A requested offset is not in the L1 tree. In this case,
	 * subsequent bmap call starts finding blocks in other lsm tree.
	 */
	if (ret == -EAGAIN) {
		bitmap_clear(io_bitmap, bitmap_pos, bmap_req.blk_count_found);
		io_to_be_done += bmap_req.blk_count_found;

		goto do_global_search;
	} else {
		bitmap_clear(io_bitmap, bitmap_pos, bmap_req.blk_count_found);
		io_to_be_done += bmap_req.blk_count_found;

		//mlfs_assert(bitmap_weight(io_bitmap, bitmap_size) == 0);
		if (bitmap_weight(io_bitmap, bitmap_size) != 0) {
			goto do_global_search;
		}
	}

	mlfs_assert(io_to_be_done == (io_size >> g_block_size_shift));

do_io_aligned:
	//mlfs_assert(bitmap_weight(io_bitmap, bitmap_size) == 0);

	if (enable_perf_stats)
		start_tsc = asm_rdtscp();

	// Read data from L1 ~ trees
	list_for_each_entry_safe(bh, _bh, &io_list, b_io_list) {
		mlfs_assert(bh->b_dev < g_n_devices);
		bh_submit_read_sync_IO(bh);
		bh_release(bh);
	}

	// mlfs_io_wait(g_ssd_dev, 1);
	// At this point, read cache entries are filled with data from SSD/HDD.

  // copying read cache data to user buffer.
  for (i = 0 ; i < bitmap_size; i++) {
    if (copy_list[i].dst_buffer != NULL) {
      memmove(copy_list[i].dst_buffer, copy_list[i].cached_data,
          copy_list[i].size);

      if (copy_list[i].dst_buffer + copy_list[i].size > reply->dst + io_size)
        panic("read request overruns the user buffer\n");
    }
  }

  // Patch data from log (L0) if up-to-date blocks are in the update log.
  // This is required when partial updates are in the update log.
  list_for_each_entry_safe(bh, _bh, &io_list_log, b_io_list) {
    bh_submit_read_sync_IO(bh);
    bh_release(bh);
  }

	if (enable_perf_stats) {
		g_perf_stats.read_data_tsc += (asm_rdtscp() - start_tsc);
		g_perf_stats.read_data_nr++;
	}

	return io_size;
}

int readi(struct inode *ip, struct mlfs_reply *reply, offset_t off, uint32_t io_size, char *path)
{
	int ret = 0;
	//uint8_t *_dst;
	offset_t _off, offset_end, offset_aligned, offset_small = 0;
	offset_t size_aligned = 0, size_prepended = 0, size_appended = 0, size_small = 0;
	int io_done;

	mlfs_assert(off < ip->size);

/*
	if(reply->remote) {
#if MLFS_REPLICA
		//only support 4k io sizes for received rpc reads
		mlfs_assert(io_size == g_block_size_bytes);
#else
		panic("Unsupported operation; only replicas can serve remote reads\n");
#endif
	}
*/

	if (off + io_size > ip->size)
		io_size = ip->size - off;

	//_dst = dst;
	//_off = off;

	offset_end = off + io_size;
	offset_aligned = ALIGN(off, g_block_size_bytes);

	// aligned read.
	if ((offset_aligned == off) &&
		(offset_end == ALIGN(offset_end, g_block_size_bytes))) {
		size_aligned = io_size;
	}
	// unaligned read.
	else {
		if ((offset_aligned == off && io_size < g_block_size_bytes) ||
				(offset_end < offset_aligned)) {
			offset_small = off - ALIGN_FLOOR(off, g_block_size_bytes);
			size_small = io_size;
		} else {
			if (off < offset_aligned) {
				size_prepended = offset_aligned - off;
			} else
				size_prepended = 0;

			size_appended = ALIGN(offset_end, g_block_size_bytes) - offset_end;
			if (size_appended > 0) {
				size_appended = g_block_size_bytes - size_appended;
			}

			size_aligned = io_size - size_prepended - size_appended;
		}
	}

	mlfs_debug("read stats: inode[inum %u isize %lu] size_small %lu size_prepended %lu size_aligned %lu size_appended %lu\n",
			ip->inum, ip->size, size_small, size_prepended, size_aligned, size_appended);
	if (size_small) {
		io_done = do_unaligned_read(ip, reply, off, size_small, path);

		mlfs_assert(size_small == io_done);

		reply->dst += io_done;
		off += io_done;
		ret += io_done;
	}

	if (size_prepended) {
		io_done = do_unaligned_read(ip, reply, off, size_prepended, path);

		mlfs_assert(size_prepended == io_done);

		reply->dst += io_done;
		off += io_done;
		ret += io_done;
	}

	if (size_aligned) {
		io_done = do_aligned_read(ip, reply, off, size_aligned, path);

		mlfs_assert(size_aligned == io_done);

		reply->dst += io_done;
		off += io_done;
		ret += io_done;
	}

	if (size_appended) {
		io_done = do_unaligned_read(ip, reply, off, size_appended, path);

		mlfs_assert(size_appended == io_done);

		reply->dst += io_done;
		off += io_done;
		ret += io_done;
	}

	mlfs_debug("finishing read. iodone: %d\n", ret);

	return ret;
}

// Write data to log
// add_to_log should handle the logging for both directory and file.
// 1. allocate blocks for log
// 2. add to log_header
int add_to_log(struct inode *ip, uint8_t *data, offset_t off, uint32_t size, uint8_t ltype)
{
	offset_t total;
	uint32_t io_size, nr_iovec = 0;
	addr_t block_no;
	struct logheader_meta *loghdr_meta;

	mlfs_assert(ip != NULL);

	mlfs_debug("add to log: inum %u offset %lu size %u\n", ip->inum, off, size);

	loghdr_meta = get_loghdr_meta();
	mlfs_assert(loghdr_meta);

	mlfs_assert(off + size > off);

	nr_iovec = loghdr_meta->nr_iovec;
	loghdr_meta->io_vec[nr_iovec].base = data;
	loghdr_meta->io_vec[nr_iovec].size = size;
	loghdr_meta->nr_iovec++;

	mlfs_debug("%s\n", "adding to loghdr");

	mlfs_assert(loghdr_meta->nr_iovec <= 9);
	add_to_loghdr(ltype, ip, off, size, NULL, 0);

	mlfs_debug("%s\n", "add to loghdr done");

	mlfs_debug("DEBUG off+size %lu ip->size %lu\n", off+size, ip->size);
	if (size > 0 && (off + size) > ip->size) {
		ip->size = off + size;
		mlfs_debug("DEBUG setting ip->size to %lu\n", ip->size);
	}

	return size;
}
