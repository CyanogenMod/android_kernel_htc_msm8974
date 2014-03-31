#ifndef _RAID10_H
#define _RAID10_H

struct mirror_info {
	struct md_rdev	*rdev, *replacement;
	sector_t	head_position;
	int		recovery_disabled;	
};

struct r10conf {
	struct mddev		*mddev;
	struct mirror_info	*mirrors;
	int			raid_disks;
	spinlock_t		device_lock;

	
	int			near_copies;  
	int 			far_copies;   
	int			far_offset;   
	int			copies;	      
	sector_t		stride;	      

	sector_t		dev_sectors;  

	int			chunk_shift; 
	sector_t		chunk_mask;

	struct list_head	retry_list;
	
	struct bio_list		pending_bio_list;
	int			pending_count;

	spinlock_t		resync_lock;
	int			nr_pending;
	int			nr_waiting;
	int			nr_queued;
	int			barrier;
	sector_t		next_resync;
	int			fullsync;  
	int			have_replacement; 
	wait_queue_head_t	wait_barrier;

	mempool_t		*r10bio_pool;
	mempool_t		*r10buf_pool;
	struct page		*tmppage;

	struct md_thread	*thread;
};


struct r10bio {
	atomic_t		remaining; 
	sector_t		sector;	
	int			sectors;
	unsigned long		state;
	struct mddev		*mddev;
	struct bio		*master_bio;
	int			read_slot;

	struct list_head	retry_list;
	struct {
		struct bio	*bio;
		union {
			struct bio	*repl_bio; 
			struct md_rdev	*rdev;	   
		};
		sector_t	addr;
		int		devnum;
	} devs[0];
};

#define IO_BLOCKED ((struct bio*)1)
#define IO_MADE_GOOD ((struct bio *)2)

#define BIO_SPECIAL(bio) ((unsigned long)bio <= 2)

enum r10bio_state {
	R10BIO_Uptodate,
	R10BIO_IsSync,
	R10BIO_IsRecover,
	R10BIO_Degraded,
	R10BIO_ReadError,
	R10BIO_MadeGood,
	R10BIO_WriteError,
};
#endif
