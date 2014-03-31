/*
 * bitmap.h: Copyright (C) Peter T. Breuer (ptb@ot.uc3m.es) 2003
 *
 * additions: Copyright (C) 2003-2004, Paul Clements, SteelEye Technology, Inc.
 */
#ifndef BITMAP_H
#define BITMAP_H 1

#define BITMAP_MAJOR_LO 3
#define BITMAP_MAJOR_HI 4
#define	BITMAP_MAJOR_HOSTENDIAN 3


#ifdef __KERNEL__

#define PAGE_BITS (PAGE_SIZE << 3)
#define PAGE_BIT_SHIFT (PAGE_SHIFT + 3)

typedef __u16 bitmap_counter_t;
#define COUNTER_BITS 16
#define COUNTER_BIT_SHIFT 4
#define COUNTER_BYTE_SHIFT (COUNTER_BIT_SHIFT - 3)

#define NEEDED_MASK ((bitmap_counter_t) (1 << (COUNTER_BITS - 1)))
#define RESYNC_MASK ((bitmap_counter_t) (1 << (COUNTER_BITS - 2)))
#define COUNTER_MAX ((bitmap_counter_t) RESYNC_MASK - 1)
#define NEEDED(x) (((bitmap_counter_t) x) & NEEDED_MASK)
#define RESYNC(x) (((bitmap_counter_t) x) & RESYNC_MASK)
#define COUNTER(x) (((bitmap_counter_t) x) & COUNTER_MAX)

#define PAGE_COUNTER_RATIO (PAGE_BITS / COUNTER_BITS)
#define PAGE_COUNTER_SHIFT (PAGE_BIT_SHIFT - COUNTER_BIT_SHIFT)
#define PAGE_COUNTER_MASK  (PAGE_COUNTER_RATIO - 1)

#define BITMAP_BLOCK_SHIFT 9

#endif


#define BITMAP_MAGIC 0x6d746962

enum bitmap_state {
	BITMAP_STALE  = 0x002,  
	BITMAP_WRITE_ERROR = 0x004, 
	BITMAP_HOSTENDIAN = 0x8000,
};

typedef struct bitmap_super_s {
	__le32 magic;        
	__le32 version;      
	__u8  uuid[16];      
	__le64 events;       
	__le64 events_cleared;
	__le64 sync_size;    
	__le32 state;        
	__le32 chunksize;    
	__le32 daemon_sleep; 
	__le32 write_behind; 

	__u8  pad[256 - 64]; 
} bitmap_super_t;


#ifdef __KERNEL__

struct bitmap_page {
	char *map;
	unsigned int hijacked:1;
	unsigned int  count:31;
};

struct bitmap {
	struct bitmap_page *bp;
	unsigned long pages; 
	unsigned long missing_pages; 

	struct mddev *mddev; 

	
	unsigned long chunkshift; 
	unsigned long chunks; 

	__u64	events_cleared;
	int need_sync;

	
	spinlock_t lock;

	struct file *file; 
	struct page *sb_page; 
	struct page **filemap; 
	unsigned long *filemap_attr; 
	unsigned long file_pages; 
	int last_page_size; 

	unsigned long flags;

	int allclean;

	atomic_t behind_writes;
	unsigned long behind_writes_used; 

	unsigned long daemon_lastrun; 
	unsigned long last_end_sync; 

	atomic_t pending_writes; 
	wait_queue_head_t write_wait;
	wait_queue_head_t overflow_wait;
	wait_queue_head_t behind_wait;

	struct sysfs_dirent *sysfs_can_clear;
};


int  bitmap_create(struct mddev *mddev);
int bitmap_load(struct mddev *mddev);
void bitmap_flush(struct mddev *mddev);
void bitmap_destroy(struct mddev *mddev);

void bitmap_print_sb(struct bitmap *bitmap);
void bitmap_update_sb(struct bitmap *bitmap);
void bitmap_status(struct seq_file *seq, struct bitmap *bitmap);

int  bitmap_setallbits(struct bitmap *bitmap);
void bitmap_write_all(struct bitmap *bitmap);

void bitmap_dirty_bits(struct bitmap *bitmap, unsigned long s, unsigned long e);

int bitmap_startwrite(struct bitmap *bitmap, sector_t offset,
			unsigned long sectors, int behind);
void bitmap_endwrite(struct bitmap *bitmap, sector_t offset,
			unsigned long sectors, int success, int behind);
int bitmap_start_sync(struct bitmap *bitmap, sector_t offset, sector_t *blocks, int degraded);
void bitmap_end_sync(struct bitmap *bitmap, sector_t offset, sector_t *blocks, int aborted);
void bitmap_close_sync(struct bitmap *bitmap);
void bitmap_cond_end_sync(struct bitmap *bitmap, sector_t sector);

void bitmap_unplug(struct bitmap *bitmap);
void bitmap_daemon_work(struct mddev *mddev);
#endif

#endif
