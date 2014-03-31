/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 * Copyright © 2004-2010 David Woodhouse <dwmw2@infradead.org>
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#ifndef _JFFS2_FS_SB
#define _JFFS2_FS_SB

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/rwsem.h>

#define JFFS2_SB_FLAG_RO 1
#define JFFS2_SB_FLAG_SCANNING 2 
#define JFFS2_SB_FLAG_BUILDING 4 

struct jffs2_inodirty;

struct jffs2_mount_opts {
	bool override_compr;
	unsigned int compr;
};

struct jffs2_sb_info {
	struct mtd_info *mtd;

	uint32_t highest_ino;
	uint32_t checked_ino;

	unsigned int flags;

	struct task_struct *gc_task;	
	struct completion gc_thread_start; 
	struct completion gc_thread_exit; 

	struct mutex alloc_sem;		
	uint32_t cleanmarker_size;	

	uint32_t flash_size;
	uint32_t used_size;
	uint32_t dirty_size;
	uint32_t wasted_size;
	uint32_t free_size;
	uint32_t erasing_size;
	uint32_t bad_size;
	uint32_t sector_size;
	uint32_t unchecked_size;

	uint32_t nr_free_blocks;
	uint32_t nr_erasing_blocks;

	
	uint8_t resv_blocks_write;	
	uint8_t resv_blocks_deletion;	
	uint8_t resv_blocks_gctrigger;	
	uint8_t resv_blocks_gcbad;	
	uint8_t resv_blocks_gcmerge;	
	
	uint8_t vdirty_blocks_gctrigger;

	uint32_t nospc_dirty_size;

	uint32_t nr_blocks;
	struct jffs2_eraseblock *blocks;	
	struct jffs2_eraseblock *nextblock;	

	struct jffs2_eraseblock *gcblock;	

	struct list_head clean_list;		
	struct list_head very_dirty_list;	
	struct list_head dirty_list;		
	struct list_head erasable_list;		
	struct list_head erasable_pending_wbuf_list;	
	struct list_head erasing_list;		
	struct list_head erase_checking_list;	
	struct list_head erase_pending_list;	
	struct list_head erase_complete_list;	/* Blocks which are erased and need the clean marker written to them */
	struct list_head free_list;		
	struct list_head bad_list;		
	struct list_head bad_used_list;		

	spinlock_t erase_completion_lock;	
	wait_queue_head_t erase_wait;		

	wait_queue_head_t inocache_wq;
	int inocache_hashsize;
	struct jffs2_inode_cache **inocache_list;
	spinlock_t inocache_lock;

	struct mutex erase_free_sem;

	uint32_t wbuf_pagesize; 

#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	unsigned char *wbuf_verify; 
#endif
#ifdef CONFIG_JFFS2_FS_WRITEBUFFER
	unsigned char *wbuf; 
	uint32_t wbuf_ofs;
	uint32_t wbuf_len;
	struct jffs2_inodirty *wbuf_inodes;
	struct rw_semaphore wbuf_sem;	

	unsigned char *oobbuf;
	int oobavail; 
#endif

	struct jffs2_summary *summary;		
	struct jffs2_mount_opts mount_opts;

#ifdef CONFIG_JFFS2_FS_XATTR
#define XATTRINDEX_HASHSIZE	(57)
	uint32_t highest_xid;
	uint32_t highest_xseqno;
	struct list_head xattrindex[XATTRINDEX_HASHSIZE];
	struct list_head xattr_unchecked;
	struct list_head xattr_dead_list;
	struct jffs2_xattr_ref *xref_dead_list;
	struct jffs2_xattr_ref *xref_temp;
	struct rw_semaphore xattr_sem;
	uint32_t xdatum_mem_usage;
	uint32_t xdatum_mem_threshold;
#endif
	
	void *os_priv;
};

#endif 
