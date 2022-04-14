#include "fs.h"
#include "ds/list.h"
#include "ds/khash.h"
#include "global/util.h"
#include "migrate.h"
#include "extents.h"
#include "filesystem/slru.h"
#include "ssd_emulation.h"

#include "cache/cache.h"

lru_node_t *g_lru_hash[g_n_devices + 1];
struct lru g_lru[g_n_devices + 1];
struct lru g_stage_lru[g_n_devices + 1];
struct lru g_swap_lru[g_n_devices + 1];
pthread_spinlock_t lru_spinlock;

// 0: not used
// 1: g_root_dev
// 2: g_ssd_dev
// 3: g_hdd_dev (not used)
// 4~ unused
int wb_threshold[g_n_devices + 1] = {0, 60, 80, 100};
int migrate_threshold[g_n_devices + 1] = {0, 90, 95, 100};

static inline uint8_t get_lower_dev(uint8_t dev)
{
	uint8_t lower_dev;

	if (dev == g_root_dev) {
#ifdef USE_SSD
		lower_dev = g_ssd_dev;
#else
		lower_dev = 0;
#endif
	} else if (dev == g_ssd_dev) {
#ifdef USE_HDD
		lower_dev = g_hdd_dev;
#else
		lower_dev = 0;
#endif
	} else if (dev == g_hdd_dev) {
		lower_dev = 0;
	}

	return lower_dev;
}

static void evict_slru_lists(void)
{
  uint8_t dev;

  for(dev = g_root_dev; get_lower_dev(dev) != 0; dev = get_lower_dev(dev)) {
    // Evict last one if over limit, move to next lower LRU
    while(g_lru[dev].n >= (disk_sb[dev].ndatablocks * migrate_threshold[dev]) / 100) {
      lru_node_t *n = list_last_entry(&g_lru[dev].lru_head, lru_node_t, list);
      uint8_t ndev = get_lower_dev(dev);

      /* mlfs_info("Evicting %u -> %u.\n", dev, ndev); */

      list_move(&n->list, &g_lru[ndev].lru_head);
      HASH_DEL(g_lru_hash[dev], n);
      HASH_ADD(hh, g_lru_hash[ndev], key, sizeof(lru_key_t), n);

      g_lru[dev].n--;
      g_lru[ndev].n++;
    }
  }
}

static void update_staging_slru_list(uint8_t dev, lru_node_t *node)
{
#if MLFS_REPLICA

#else
	panic("unexpected code path\n");
#endif

}	

int update_slru_list_from_digest(uint8_t dev, lru_key_t k, lru_val_t v)
{
	struct inode *inode;
	lru_node_t *node;
	struct lru *lru;

	if(dev != g_root_dev)
		panic("invalid code path; digesting to non-NVM device");

	pthread_spin_lock(&lru_spinlock);

#if MLFS_REPLICA
	uint64_t used_blocks = sb[dev]->used_blocks;
	uint64_t datablocks = disk_sb[dev].ndatablocks;

	if (used_blocks > (migrate_threshold[dev] * datablocks) / 100)
		lru = &g_stage_lru[dev];
	else
		lru = &g_lru[dev];
#else
	lru = &g_lru[dev];
#endif

	HASH_FIND(hh, g_lru_hash[dev], &k, sizeof(lru_key_t), node);

	if (node) {
		list_del_init(&node->list);
		//node->access_freq[(ALIGN_FLOOR(search_key.offset, g_block_size_bytes)) >> g_block_size_shift]++;

		list_add(&node->list, &lru->lru_head);
	} else {
		node = (lru_node_t *)mlfs_zalloc(sizeof(lru_node_t));

		node->key = k;
		node->val = v;
		//memset(&node->access_freq, 0, LRU_ENTRY_SIZE >> g_block_size_shift);
		INIT_LIST_HEAD(&node->list);

		HASH_ADD(hh, g_lru_hash[dev], key, sizeof(lru_key_t), node);
		node->sync = 0;
		list_add(&node->list, &lru->lru_head);
		lru->n++;

		//evict_slru_lists();
	}

	//printf("Adding LRU inum: %u lblk: %lu\n", node->val.inum, node->val.lblock);

	pthread_spin_unlock(&lru_spinlock);

#if 0
	inode = icache_find(k.inum);
	
	if (inode) {
		// update per-inode lru list.
		if (!is_del_entry(&node->per_inode_list))
			list_del(&node->per_inode_list);

		list_add(&node->per_inode_list, &inode->i_slru_head);
	}
#endif
}

int migrate_blocks(uint8_t from_dev, uint8_t to_dev, isolated_list_t *migrate_list, int swap)
{
  static int sampl_intv = 0;

	int ret;
	int migrate_down = from_dev < to_dev;
	int migrate_up = !migrate_down;
	lru_node_t *l, *tmp;
	struct lru *from_lru, *to_lru;
	mlfs_fsblk_t blknr;
	struct inode *file_inode;
	struct mlfs_map_blocks map;
	offset_t cur_lblk;
	uint32_t nr_blocks, nr_done = 0;
	uint32_t migrated_success = 0;
	uint8_t lower_dev, upper_dev;
	struct list_head migrate_success_list;
	handle_t handle = {.dev = from_dev};

  from_lru = &g_lru[from_dev];

	list_for_each_entry_safe_reverse(l, tmp, &migrate_list->head, list) {
		from_lru->n--;
		list_del_init(&l->list);
		migrated_success++;
		HASH_DEL(g_lru_hash[from_dev], l);

    send_to_ssd(l->key.block);
    send_to_rcache(l->key.block);
	}

  /*
	// Wait for finishing all outstanding IO.
	if (to_dev == g_ssd_dev)
		mlfs_io_wait(g_ssd_dev, 1);
  */

	g_perf_stats.total_migrated_mb += 
		((migrated_success * LRU_ENTRY_SIZE) >> 20);

	return migrated_success;
}

/* nr_blocks: minimum amount of blocks to migrate */
int try_migrate_blocks(uint8_t from_dev, uint8_t to_dev, uint32_t nr_blocks, uint8_t force, int swap)
{
#ifdef MIGRATION
	int migrate_down = from_dev < to_dev;
	int migrate_up = !migrate_down;
	uint32_t n_entries = 0, i = 0, ret = 0, do_migrate = 0;
	uint64_t used_blocks, datablocks;
	struct isolated_list migrate_list;
	lru_node_t *node, *tmp;

#ifndef USE_SSD
	return 0;
#endif

	if (force) {
		do_migrate = 1;
		goto do_force_migration;
	}

	used_blocks = sb[from_dev]->used_blocks;
	datablocks = disk_sb[from_dev].ndatablocks;
	
  /*
  printf("*** used_blocks: %lu, migrate_thr: %lu, db: %lu, total: %lu\n", used_blocks, migrate_threshold[from_dev], datablocks/4,
    (migrate_threshold[from_dev] * datablocks / 4) / 100);
  */

  uint64_t num_migr = get_num_migrated();
	if (used_blocks - num_migr > (migrate_threshold[from_dev] * datablocks / 4) / 100) {
		n_entries = BLOCKS_TO_LRU_ENTRIES(
				used_blocks - num_migr - ((migrate_threshold[from_dev] * datablocks / 4) / 100));
		do_migrate = 1;
	} else {
		return 0;
	}

do_force_migration:
	if (nr_blocks == 0) 
		nr_blocks = BLOCKS_PER_LRU_ENTRY * MIN_MIGRATE_ENTRY;

	INIT_LIST_HEAD(&migrate_list.head);
	INIT_LIST_HEAD(&migrate_list.fail_head);

	n_entries = max(n_entries, BLOCKS_TO_LRU_ENTRIES(nr_blocks));
	mlfs_debug("migration: %d->%d n_entries %u\n", from_dev, to_dev, n_entries);

	migrate_list.n = 0;
	struct lru *from_lru = &g_lru[from_dev];
  
  //printf("*** from_lru size: %lu\n", from_lru->n);

  // TODO: number of tries to migrate- right now, just once
  while(ret < nr_blocks){
    //printf("*** ran inside loop.\n");
    list_for_each_entry_safe_reverse(node, tmp, &from_lru->lru_head, list) {
      // isolate list from per-device lru list.
      list_del_init(&node->list);
        
      if (node->val.inum == ROOTINO)
        continue;

      //printf("*** added to list head.\n");
      list_add(&node->list, &migrate_list.head);
      migrate_list.n++;

      i++;

      mlfs_debug("try migrate (%d->%d): iter %d inum %d offset %lu(0x%lx) phys %lu\n", 
          i, from_dev, to_dev, node->val.inum, node->val.lblock, 
          node->val.lblock, node->key.block);

      if (i >= n_entries)
        break;
    }
    // ensure forward progress
    int liveliness_factor = 1;
	  if (used_blocks - num_migr > datablocks / 4) {
      liveliness_factor = 0;
    }
    ret += liveliness_factor + migrate_blocks(from_dev, to_dev, &migrate_list, swap);
  }

	return 0;
#endif // MIGRATION
}

