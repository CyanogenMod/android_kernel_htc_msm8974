/*
   md.h : kernel internal structure of the Linux MD driver
          Copyright (C) 1996-98 Ingo Molnar, Gadi Oxman
	  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#ifndef _MD_MD_H
#define _MD_MD_H

#include <linux/blkdev.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#define MaxSector (~(sector_t)0)

#define MD_MAX_BADBLOCKS	(PAGE_SIZE/8)

struct md_rdev {
	struct list_head same_set;	

	sector_t sectors;		
	struct mddev *mddev;		
	int last_events;		

	struct block_device *meta_bdev;
	struct block_device *bdev;	

	struct page	*sb_page, *bb_page;
	int		sb_loaded;
	__u64		sb_events;
	sector_t	data_offset;	
	sector_t 	sb_start;	
	int		sb_size;	
	int		preferred_minor;	

	struct kobject	kobj;


	unsigned long	flags;	
	wait_queue_head_t blocked_wait;

	int desc_nr;			
	int raid_disk;			
	int new_raid_disk;		
	int saved_raid_disk;		
	sector_t	recovery_offset;

	atomic_t	nr_pending;	
	atomic_t	read_errors;	
	struct timespec last_read_error;	
	atomic_t	corrected_errors; 
	struct work_struct del_work;	

	struct sysfs_dirent *sysfs_state; 

	struct badblocks {
		int	count;		
		int	unacked_exist;	
		int	shift;		
		u64	*page;		
		int	changed;
		seqlock_t lock;

		sector_t sector;
		sector_t size;		
	} badblocks;
};
enum flag_bits {
	Faulty,			
	In_sync,		
	Unmerged,		
	WriteMostly,		
	AutoDetected,		
	Blocked,		
	WriteErrorSeen,		
	FaultRecorded,		
	BlockedBadBlocks,	
	WantReplacement,	
	Replacement,		
};

#define BB_LEN_MASK	(0x00000000000001FFULL)
#define BB_OFFSET_MASK	(0x7FFFFFFFFFFFFE00ULL)
#define BB_ACK_MASK	(0x8000000000000000ULL)
#define BB_MAX_LEN	512
#define BB_OFFSET(x)	(((x) & BB_OFFSET_MASK) >> 9)
#define BB_LEN(x)	(((x) & BB_LEN_MASK) + 1)
#define BB_ACK(x)	(!!((x) & BB_ACK_MASK))
#define BB_MAKE(a, l, ack) (((a)<<9) | ((l)-1) | ((u64)(!!(ack)) << 63))

extern int md_is_badblock(struct badblocks *bb, sector_t s, int sectors,
			  sector_t *first_bad, int *bad_sectors);
static inline int is_badblock(struct md_rdev *rdev, sector_t s, int sectors,
			      sector_t *first_bad, int *bad_sectors)
{
	if (unlikely(rdev->badblocks.count)) {
		int rv = md_is_badblock(&rdev->badblocks, rdev->data_offset + s,
					sectors,
					first_bad, bad_sectors);
		if (rv)
			*first_bad -= rdev->data_offset;
		return rv;
	}
	return 0;
}
extern int rdev_set_badblocks(struct md_rdev *rdev, sector_t s, int sectors,
			      int acknowledged);
extern int rdev_clear_badblocks(struct md_rdev *rdev, sector_t s, int sectors);
extern void md_ack_all_badblocks(struct badblocks *bb);

struct mddev {
	void				*private;
	struct md_personality		*pers;
	dev_t				unit;
	int				md_minor;
	struct list_head 		disks;
	unsigned long			flags;
#define MD_CHANGE_DEVS	0	
#define MD_CHANGE_CLEAN 1	
#define MD_CHANGE_PENDING 2	
#define MD_ARRAY_FIRST_USE 3    

	int				suspended;
	atomic_t			active_io;
	int				ro;
	int				sysfs_active; 
	int				ready; 
	struct gendisk			*gendisk;

	struct kobject			kobj;
	int				hold_active;
#define	UNTIL_IOCTL	1
#define	UNTIL_STOP	2

	
	int				major_version,
					minor_version,
					patch_version;
	int				persistent;
	int 				external;	
	char				metadata_type[17]; 
	int				chunk_sectors;
	time_t				ctime, utime;
	int				level, layout;
	char				clevel[16];
	int				raid_disks;
	int				max_disks;
	sector_t			dev_sectors; 	
	sector_t			array_sectors; 
	int				external_size; 
	__u64				events;
	int				can_decrease_events;

	char				uuid[16];

	/* If the array is being reshaped, we need to record the
	 * new shape and an indication of where we are up to.
	 * This is written to the superblock.
	 * If reshape_position is MaxSector, then no reshape is happening (yet).
	 */
	sector_t			reshape_position;
	int				delta_disks, new_level, new_layout;
	int				new_chunk_sectors;

	atomic_t			plug_cnt;	
	struct md_thread		*thread;	
	struct md_thread		*sync_thread;	
	sector_t			curr_resync;	
	sector_t			curr_resync_completed;
	unsigned long			resync_mark;	
	sector_t			resync_mark_cnt;/* blocks written at resync_mark */
	sector_t			curr_mark_cnt; 

	sector_t			resync_max_sectors; 

	sector_t			resync_mismatches; 

	
	sector_t			suspend_lo;
	sector_t			suspend_hi;
	
	int				sync_speed_min;
	int				sync_speed_max;

	
	int				parallel_resync;

	int				ok_start_degraded;
#define	MD_RECOVERY_RUNNING	0
#define	MD_RECOVERY_SYNC	1
#define	MD_RECOVERY_RECOVER	2
#define	MD_RECOVERY_INTR	3
#define	MD_RECOVERY_DONE	4
#define	MD_RECOVERY_NEEDED	5
#define	MD_RECOVERY_REQUESTED	6
#define	MD_RECOVERY_CHECK	7
#define MD_RECOVERY_RESHAPE	8
#define	MD_RECOVERY_FROZEN	9

	unsigned long			recovery;
	int				recovery_disabled;

	int				in_sync;	
	struct mutex			open_mutex;
	struct mutex			reconfig_mutex;
	atomic_t			active;		
	atomic_t			openers;	

	int				changed;	
	int				degraded;	
	int				merge_check_needed; 

	atomic_t			recovery_active; /* blocks scheduled, but not written */
	wait_queue_head_t		recovery_wait;
	sector_t			recovery_cp;
	sector_t			resync_min;	
	sector_t			resync_max;	

	struct sysfs_dirent		*sysfs_state;	
	struct sysfs_dirent		*sysfs_action;  

	struct work_struct del_work;	

	spinlock_t			write_lock;
	wait_queue_head_t		sb_wait;	
	atomic_t			pending_writes;	

	unsigned int			safemode;	
 
	unsigned int			safemode_delay;
	struct timer_list		safemode_timer;
	atomic_t			writes_pending; 
	struct request_queue		*queue;	

	struct bitmap                   *bitmap; 
	struct {
		struct file		*file; 
		loff_t			offset; 
		loff_t			default_offset; 
		struct mutex		mutex;
		unsigned long		chunksize;
		unsigned long		daemon_sleep; 
		unsigned long		max_write_behind; 
		int			external;
	} bitmap_info;

	atomic_t 			max_corr_read_errors; 
	struct list_head		all_mddevs;

	struct attribute_group		*to_remove;

	struct bio_set			*bio_set;

	struct bio *flush_bio;
	atomic_t flush_pending;
	struct work_struct flush_work;
	struct work_struct event_work;	
	void (*sync_super)(struct mddev *mddev, struct md_rdev *rdev);
};


static inline void rdev_dec_pending(struct md_rdev *rdev, struct mddev *mddev)
{
	int faulty = test_bit(Faulty, &rdev->flags);
	if (atomic_dec_and_test(&rdev->nr_pending) && faulty)
		set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
}

static inline void md_sync_acct(struct block_device *bdev, unsigned long nr_sectors)
{
        atomic_add(nr_sectors, &bdev->bd_contains->bd_disk->sync_io);
}

struct md_personality
{
	char *name;
	int level;
	struct list_head list;
	struct module *owner;
	void (*make_request)(struct mddev *mddev, struct bio *bio);
	int (*run)(struct mddev *mddev);
	int (*stop)(struct mddev *mddev);
	void (*status)(struct seq_file *seq, struct mddev *mddev);
	void (*error_handler)(struct mddev *mddev, struct md_rdev *rdev);
	int (*hot_add_disk) (struct mddev *mddev, struct md_rdev *rdev);
	int (*hot_remove_disk) (struct mddev *mddev, struct md_rdev *rdev);
	int (*spare_active) (struct mddev *mddev);
	sector_t (*sync_request)(struct mddev *mddev, sector_t sector_nr, int *skipped, int go_faster);
	int (*resize) (struct mddev *mddev, sector_t sectors);
	sector_t (*size) (struct mddev *mddev, sector_t sectors, int raid_disks);
	int (*check_reshape) (struct mddev *mddev);
	int (*start_reshape) (struct mddev *mddev);
	void (*finish_reshape) (struct mddev *mddev);
	void (*quiesce) (struct mddev *mddev, int state);
	void *(*takeover) (struct mddev *mddev);
};


struct md_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct mddev *, char *);
	ssize_t (*store)(struct mddev *, const char *, size_t);
};
extern struct attribute_group md_bitmap_group;

static inline struct sysfs_dirent *sysfs_get_dirent_safe(struct sysfs_dirent *sd, char *name)
{
	if (sd)
		return sysfs_get_dirent(sd, NULL, name);
	return sd;
}
static inline void sysfs_notify_dirent_safe(struct sysfs_dirent *sd)
{
	if (sd)
		sysfs_notify_dirent(sd);
}

static inline char * mdname (struct mddev * mddev)
{
	return mddev->gendisk ? mddev->gendisk->disk_name : "mdX";
}

static inline int sysfs_link_rdev(struct mddev *mddev, struct md_rdev *rdev)
{
	char nm[20];
	if (!test_bit(Replacement, &rdev->flags)) {
		sprintf(nm, "rd%d", rdev->raid_disk);
		return sysfs_create_link(&mddev->kobj, &rdev->kobj, nm);
	} else
		return 0;
}

static inline void sysfs_unlink_rdev(struct mddev *mddev, struct md_rdev *rdev)
{
	char nm[20];
	if (!test_bit(Replacement, &rdev->flags)) {
		sprintf(nm, "rd%d", rdev->raid_disk);
		sysfs_remove_link(&mddev->kobj, nm);
	}
}

#define rdev_for_each_list(rdev, tmp, head)				\
	list_for_each_entry_safe(rdev, tmp, head, same_set)

#define rdev_for_each(rdev, mddev)				\
	list_for_each_entry(rdev, &((mddev)->disks), same_set)

#define rdev_for_each_safe(rdev, tmp, mddev)				\
	list_for_each_entry_safe(rdev, tmp, &((mddev)->disks), same_set)

#define rdev_for_each_rcu(rdev, mddev)				\
	list_for_each_entry_rcu(rdev, &((mddev)->disks), same_set)

struct md_thread {
	void			(*run) (struct mddev *mddev);
	struct mddev		*mddev;
	wait_queue_head_t	wqueue;
	unsigned long           flags;
	struct task_struct	*tsk;
	unsigned long		timeout;
};

#define THREAD_WAKEUP  0

#define __wait_event_lock_irq(wq, condition, lock, cmd) 		\
do {									\
	wait_queue_t __wait;						\
	init_waitqueue_entry(&__wait, current);				\
									\
	add_wait_queue(&wq, &__wait);					\
	for (;;) {							\
		set_current_state(TASK_UNINTERRUPTIBLE);		\
		if (condition)						\
			break;						\
		spin_unlock_irq(&lock);					\
		cmd;							\
		schedule();						\
		spin_lock_irq(&lock);					\
	}								\
	current->state = TASK_RUNNING;					\
	remove_wait_queue(&wq, &__wait);				\
} while (0)

#define wait_event_lock_irq(wq, condition, lock, cmd) 			\
do {									\
	if (condition)	 						\
		break;							\
	__wait_event_lock_irq(wq, condition, lock, cmd);		\
} while (0)

static inline void safe_put_page(struct page *p)
{
	if (p) put_page(p);
}

extern int register_md_personality(struct md_personality *p);
extern int unregister_md_personality(struct md_personality *p);
extern struct md_thread *md_register_thread(
	void (*run)(struct mddev *mddev),
	struct mddev *mddev,
	const char *name);
extern void md_unregister_thread(struct md_thread **threadp);
extern void md_wakeup_thread(struct md_thread *thread);
extern void md_check_recovery(struct mddev *mddev);
extern void md_write_start(struct mddev *mddev, struct bio *bi);
extern void md_write_end(struct mddev *mddev);
extern void md_done_sync(struct mddev *mddev, int blocks, int ok);
extern void md_error(struct mddev *mddev, struct md_rdev *rdev);

extern int mddev_congested(struct mddev *mddev, int bits);
extern void md_flush_request(struct mddev *mddev, struct bio *bio);
extern void md_super_write(struct mddev *mddev, struct md_rdev *rdev,
			   sector_t sector, int size, struct page *page);
extern void md_super_wait(struct mddev *mddev);
extern int sync_page_io(struct md_rdev *rdev, sector_t sector, int size, 
			struct page *page, int rw, bool metadata_op);
extern void md_do_sync(struct mddev *mddev);
extern void md_new_event(struct mddev *mddev);
extern int md_allow_write(struct mddev *mddev);
extern void md_wait_for_blocked_rdev(struct md_rdev *rdev, struct mddev *mddev);
extern void md_set_array_sectors(struct mddev *mddev, sector_t array_sectors);
extern int md_check_no_bitmap(struct mddev *mddev);
extern int md_integrity_register(struct mddev *mddev);
extern void md_integrity_add_rdev(struct md_rdev *rdev, struct mddev *mddev);
extern int strict_strtoul_scaled(const char *cp, unsigned long *res, int scale);
extern void restore_bitmap_write_access(struct file *file);

extern void mddev_init(struct mddev *mddev);
extern int md_run(struct mddev *mddev);
extern void md_stop(struct mddev *mddev);
extern void md_stop_writes(struct mddev *mddev);
extern int md_rdev_init(struct md_rdev *rdev);

extern void mddev_suspend(struct mddev *mddev);
extern void mddev_resume(struct mddev *mddev);
extern struct bio *bio_clone_mddev(struct bio *bio, gfp_t gfp_mask,
				   struct mddev *mddev);
extern struct bio *bio_alloc_mddev(gfp_t gfp_mask, int nr_iovecs,
				   struct mddev *mddev);
extern int mddev_check_plugged(struct mddev *mddev);
extern void md_trim_bio(struct bio *bio, int offset, int size);
#endif 
