#ifndef _RAID1_H
#define _RAID1_H

struct mirror_info {
	struct md_rdev	*rdev;
	sector_t	head_position;
};


struct pool_info {
	struct mddev *mddev;
	int	raid_disks;
};

struct r1conf {
	struct mddev		*mddev;
	struct mirror_info	*mirrors;	
	int			raid_disks;

	int			last_used;
	sector_t		next_seq_sect;
	sector_t		next_resync;

	spinlock_t		device_lock;

	struct list_head	retry_list;

	
	struct bio_list		pending_bio_list;
	int			pending_count;

	wait_queue_head_t	wait_barrier;
	spinlock_t		resync_lock;
	int			nr_pending;
	int			nr_waiting;
	int			nr_queued;
	int			barrier;

	int			fullsync;

	int			recovery_disabled;


	struct pool_info	*poolinfo;
	mempool_t		*r1bio_pool;
	mempool_t		*r1buf_pool;

	struct page		*tmppage;


	struct md_thread	*thread;
};


struct r1bio {
	atomic_t		remaining; 
	atomic_t		behind_remaining; 
	sector_t		sector;
	int			sectors;
	unsigned long		state;
	struct mddev		*mddev;
	struct bio		*master_bio;
	int			read_disk;

	struct list_head	retry_list;
	
	struct bio_vec		*behind_bvecs;
	int			behind_page_count;
	struct bio		*bios[0];
	
};

#define IO_BLOCKED ((struct bio *)1)
#define IO_MADE_GOOD ((struct bio *)2)

#define BIO_SPECIAL(bio) ((unsigned long)bio <= 2)

#define	R1BIO_Uptodate	0
#define	R1BIO_IsSync	1
#define	R1BIO_Degraded	2
#define	R1BIO_BehindIO	3
#define R1BIO_ReadError 4
#define	R1BIO_Returned 6
#define	R1BIO_MadeGood 7
#define	R1BIO_WriteError 8

extern int md_raid1_congested(struct mddev *mddev, int bits);

#endif
