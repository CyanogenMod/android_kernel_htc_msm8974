/*
 * File...........: linux/drivers/s390/block/dasd_int.h
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 *		    Horst Hummel <Horst.Hummel@de.ibm.com>
 *		    Martin Schwidefsky <schwidefsky@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * Copyright IBM Corp. 1999, 2009
 */

#ifndef DASD_INT_H
#define DASD_INT_H

#ifdef __KERNEL__

#define DASD_PER_MAJOR (1U << (MINORBITS - DASD_PARTN_BITS))
#define DASD_PARTN_MASK ((1 << DASD_PARTN_BITS) - 1)


#define DASD_STATE_NEW	  0
#define DASD_STATE_KNOWN  1
#define DASD_STATE_BASIC  2
#define DASD_STATE_UNFMT  3
#define DASD_STATE_READY  4
#define DASD_STATE_ONLINE 5

#include <linux/module.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/interrupt.h>
#include <linux/log2.h>
#include <asm/ccwdev.h>
#include <linux/workqueue.h>
#include <asm/debug.h>
#include <asm/dasd.h>
#include <asm/idals.h>

#define DASD_ECKD_MAGIC 0xC5C3D2C4
#define DASD_DIAG_MAGIC 0xC4C9C1C7
#define DASD_FBA_MAGIC 0xC6C2C140

struct dasd_device;
struct dasd_block;

#define DASD_SENSE_BIT_0 0x80
#define DASD_SENSE_BIT_1 0x40
#define DASD_SENSE_BIT_2 0x20
#define DASD_SENSE_BIT_3 0x10

#define DASD_SIM_SENSE 0x0F
#define DASD_SIM_MSG_TO_OP 0x03
#define DASD_SIM_LOG 0x0C

#define CDEV_NESTED_FIRST 1
#define CDEV_NESTED_SECOND 2

#define DBF_DEV_EVENT(d_level, d_device, d_str, d_data...) \
do { \
	debug_sprintf_event(d_device->debug_area, \
			    d_level, \
			    d_str "\n", \
			    d_data); \
} while(0)

#define DBF_DEV_EXC(d_level, d_device, d_str, d_data...) \
do { \
	debug_sprintf_exception(d_device->debug_area, \
				d_level, \
				d_str "\n", \
				d_data); \
} while(0)

#define DBF_EVENT(d_level, d_str, d_data...)\
do { \
	debug_sprintf_event(dasd_debug_area, \
			    d_level,\
			    d_str "\n", \
			    d_data); \
} while(0)

#define DBF_EVENT_DEVID(d_level, d_cdev, d_str, d_data...)	\
do { \
	struct ccw_dev_id __dev_id;			\
	ccw_device_get_id(d_cdev, &__dev_id);		\
	debug_sprintf_event(dasd_debug_area,		\
			    d_level,					\
			    "0.%x.%04x " d_str "\n",			\
			    __dev_id.ssid, __dev_id.devno, d_data);	\
} while (0)

#define DBF_EXC(d_level, d_str, d_data...)\
do { \
	debug_sprintf_exception(dasd_debug_area, \
				d_level,\
				d_str "\n", \
				d_data); \
} while(0)

#define ERRORLENGTH 30

#define	DBF_EMERG	0	
#define	DBF_ALERT	1	
#define	DBF_CRIT	2	
#define	DBF_ERR		3	
#define	DBF_WARNING	4	
#define	DBF_NOTICE	5	
#define	DBF_INFO	6	
#define	DBF_DEBUG	6	

/* messages to be written via klogd and dbf */
#define DEV_MESSAGE(d_loglevel,d_device,d_string,d_args...)\
do { \
	printk(d_loglevel PRINTK_HEADER " %s: " d_string "\n", \
	       dev_name(&d_device->cdev->dev), d_args); \
	DBF_DEV_EVENT(DBF_ALERT, d_device, d_string, d_args); \
} while(0)

#define MESSAGE(d_loglevel,d_string,d_args...)\
do { \
	printk(d_loglevel PRINTK_HEADER " " d_string "\n", d_args); \
	DBF_EVENT(DBF_ALERT, d_string, d_args); \
} while(0)

/* messages to be written via klogd only */
#define DEV_MESSAGE_LOG(d_loglevel,d_device,d_string,d_args...)\
do { \
	printk(d_loglevel PRINTK_HEADER " %s: " d_string "\n", \
	       dev_name(&d_device->cdev->dev), d_args); \
} while(0)

#define MESSAGE_LOG(d_loglevel,d_string,d_args...)\
do { \
	printk(d_loglevel PRINTK_HEADER " " d_string "\n", d_args); \
} while(0)

struct dasd_ccw_req {
	unsigned int magic;		
	struct list_head devlist;	
	struct list_head blocklist;	

	
	struct dasd_block *block;	
	struct dasd_device *memdev;	
	struct dasd_device *startdev;	
	void *cpaddr;			
	unsigned char cpmode;		
	char status;			
	short retries;			
	unsigned long flags;        	

	
	unsigned long starttime;	
	unsigned long expires;		
	char lpm;			
	void *data;			

	
	int intrc;			
	struct irb irb;			
	struct dasd_ccw_req *refers;	
	void *function; 		

	
	unsigned long long buildclk;	
	unsigned long long startclk;	
	unsigned long long stopclk;	
	unsigned long long endclk;	

        
	void (*callback)(struct dasd_ccw_req *, void *data);
	void *callback_data;
};

#define DASD_CQR_FILLED 	0x00	
#define DASD_CQR_DONE		0x01	
#define DASD_CQR_NEED_ERP	0x02	
#define DASD_CQR_IN_ERP 	0x03	
#define DASD_CQR_FAILED 	0x04	
#define DASD_CQR_TERMINATED	0x05	

#define DASD_CQR_QUEUED 	0x80	
#define DASD_CQR_IN_IO		0x81	
#define DASD_CQR_ERROR		0x82	
#define DASD_CQR_CLEAR_PENDING	0x83	
#define DASD_CQR_CLEARED	0x84	
#define DASD_CQR_SUCCESS	0x85	

#define DASD_EXPIRES	  300
#define DASD_EXPIRES_MAX  40000000

#define DASD_CQR_FLAGS_USE_ERP   0	
#define DASD_CQR_FLAGS_FAILFAST  1	
#define DASD_CQR_VERIFY_PATH	 2	
#define DASD_CQR_ALLOW_SLOCK	 3	

typedef struct dasd_ccw_req *(*dasd_erp_fn_t) (struct dasd_ccw_req *);

#define UA_NOT_CONFIGURED  0x00
#define UA_BASE_DEVICE	   0x01
#define UA_BASE_PAV_ALIAS  0x02
#define UA_HYPER_PAV_ALIAS 0x03

struct dasd_uid {
	__u8 type;
	char vendor[4];
	char serial[15];
	__u16 ssid;
	__u8 real_unit_addr;
	__u8 base_unit_addr;
	char vduit[33];
};

struct dasd_discipline {
	struct module *owner;
	char ebcname[8];	
	char name[8];		
	int max_blocks;		

	struct list_head list;	

	int (*check_device) (struct dasd_device *);
	void (*uncheck_device) (struct dasd_device *);

	int (*do_analysis) (struct dasd_block *);

	int (*verify_path)(struct dasd_device *, __u8);

	int (*ready_to_online) (struct dasd_device *);
	int (*online_to_ready) (struct dasd_device *);

	struct dasd_ccw_req *(*build_cp) (struct dasd_device *,
					  struct dasd_block *,
					  struct request *);
	int (*start_IO) (struct dasd_ccw_req *);
	int (*term_IO) (struct dasd_ccw_req *);
	void (*handle_terminated_request) (struct dasd_ccw_req *);
	struct dasd_ccw_req *(*format_device) (struct dasd_device *,
					       struct format_data_t *);
	int (*free_cp) (struct dasd_ccw_req *, struct request *);

	dasd_erp_fn_t(*erp_action) (struct dasd_ccw_req *);
	dasd_erp_fn_t(*erp_postaction) (struct dasd_ccw_req *);
	void (*dump_sense) (struct dasd_device *, struct dasd_ccw_req *,
			    struct irb *);
	void (*dump_sense_dbf) (struct dasd_device *, struct irb *, char *);
	void (*check_for_device_change) (struct dasd_device *,
					 struct dasd_ccw_req *,
					 struct irb *);

        
	int (*fill_geometry) (struct dasd_block *, struct hd_geometry *);
	int (*fill_info) (struct dasd_device *, struct dasd_information2_t *);
	int (*ioctl) (struct dasd_block *, unsigned int, void __user *);

	
	int (*freeze) (struct dasd_device *);
	int (*restore) (struct dasd_device *);

	
	int (*reload) (struct dasd_device *);

	int (*get_uid) (struct dasd_device *, struct dasd_uid *);
	void (*kick_validate) (struct dasd_device *);
};

extern struct dasd_discipline *dasd_diag_discipline_pointer;

#define DASD_EER_DISABLE 0
#define DASD_EER_TRIGGER 1

#define DASD_EER_FATALERROR  1
#define DASD_EER_NOPATH      2
#define DASD_EER_STATECHANGE 3
#define DASD_EER_PPRCSUSPEND 4

struct dasd_path {
	__u8 opm;
	__u8 tbvpm;
	__u8 ppm;
	__u8 npm;
};

struct dasd_profile_info {
	
	unsigned int dasd_io_reqs;	 
	unsigned int dasd_io_sects;	 
	unsigned int dasd_io_secs[32];	 
	unsigned int dasd_io_times[32];	 
	unsigned int dasd_io_timps[32];	 
	unsigned int dasd_io_time1[32];	 
	unsigned int dasd_io_time2[32];	 
	unsigned int dasd_io_time2ps[32]; 
	unsigned int dasd_io_time3[32];	 
	unsigned int dasd_io_nr_req[32]; 

	
	struct timespec starttod;	   
	unsigned int dasd_io_alias;	   
	unsigned int dasd_io_tpm;	   
	unsigned int dasd_read_reqs;	   
	unsigned int dasd_read_sects;	   
	unsigned int dasd_read_alias;	   
	unsigned int dasd_read_tpm;	   
	unsigned int dasd_read_secs[32];   
	unsigned int dasd_read_times[32];  
	unsigned int dasd_read_time1[32];  
	unsigned int dasd_read_time2[32];  
	unsigned int dasd_read_time3[32];  
	unsigned int dasd_read_nr_req[32]; 
};

struct dasd_profile {
	struct dentry *dentry;
	struct dasd_profile_info *data;
	spinlock_t lock;
};

struct dasd_device {
	
	struct dasd_block *block;

        unsigned int devindex;
	unsigned long flags;	   
	unsigned short features;   

	
	struct dasd_ccw_req *eer_cqr;

	
	struct dasd_discipline *discipline;
	struct dasd_discipline *base_discipline;
	char *private;
	struct dasd_path path_data;

	
	int state, target;
	struct mutex state_mutex;
	int stopped;		

	
        atomic_t ref_count;

	
	struct list_head ccw_queue;
	spinlock_t mem_lock;
	void *ccw_mem;
	void *erp_mem;
	struct list_head ccw_chunks;
	struct list_head erp_chunks;

	atomic_t tasklet_scheduled;
        struct tasklet_struct tasklet;
	struct work_struct kick_work;
	struct work_struct restore_device;
	struct work_struct reload_device;
	struct work_struct kick_validate;
	struct timer_list timer;

	debug_info_t *debug_area;

	struct ccw_device *cdev;

	
	struct list_head alias_list;

	
	unsigned long default_expires;

	struct dentry *debugfs_dentry;
	struct dasd_profile profile;
};

struct dasd_block {
	
	struct gendisk *gdp;
	struct request_queue *request_queue;
	spinlock_t request_queue_lock;
	struct block_device *bdev;
	atomic_t open_count;

	unsigned long long blocks; 
	unsigned int bp_block;	   
	unsigned int s2b_shift;	   

	struct dasd_device *base;
	struct list_head ccw_queue;
	spinlock_t queue_lock;

	atomic_t tasklet_scheduled;
	struct tasklet_struct tasklet;
	struct timer_list timer;

	struct dentry *debugfs_dentry;
	struct dasd_profile profile;
};



#define DASD_STOPPED_NOT_ACC 1         
#define DASD_STOPPED_QUIESCE 2         
#define DASD_STOPPED_PENDING 4         
#define DASD_STOPPED_DC_WAIT 8         
#define DASD_STOPPED_SU      16        
#define DASD_STOPPED_PM      32        
#define DASD_UNRESUMED_PM    64        

#define DASD_FLAG_OFFLINE	3	
#define DASD_FLAG_EER_SNSS	4	
#define DASD_FLAG_EER_IN_USE	5	
#define DASD_FLAG_DEVICE_RO	6	
#define DASD_FLAG_IS_RESERVED	7	
#define DASD_FLAG_LOCK_STOLEN	8	
#define DASD_FLAG_SUSPENDED	9	


void dasd_put_device_wake(struct dasd_device *);

static inline void
dasd_get_device(struct dasd_device *device)
{
	atomic_inc(&device->ref_count);
}

static inline void
dasd_put_device(struct dasd_device *device)
{
	if (atomic_dec_return(&device->ref_count) == 0)
		dasd_put_device_wake(device);
}

struct dasd_mchunk
{
	struct list_head list;
	unsigned long size;
} __attribute__ ((aligned(8)));

static inline void
dasd_init_chunklist(struct list_head *chunk_list, void *mem,
		    unsigned long size)
{
	struct dasd_mchunk *chunk;

	INIT_LIST_HEAD(chunk_list);
	chunk = (struct dasd_mchunk *) mem;
	chunk->size = size - sizeof(struct dasd_mchunk);
	list_add(&chunk->list, chunk_list);
}

static inline void *
dasd_alloc_chunk(struct list_head *chunk_list, unsigned long size)
{
	struct dasd_mchunk *chunk, *tmp;

	size = (size + 7L) & -8L;
	list_for_each_entry(chunk, chunk_list, list) {
		if (chunk->size < size)
			continue;
		if (chunk->size > size + sizeof(struct dasd_mchunk)) {
			char *endaddr = (char *) (chunk + 1) + chunk->size;
			tmp = (struct dasd_mchunk *) (endaddr - size) - 1;
			tmp->size = size;
			chunk->size -= size + sizeof(struct dasd_mchunk);
			chunk = tmp;
		} else
			list_del(&chunk->list);
		return (void *) (chunk + 1);
	}
	return NULL;
}

static inline void
dasd_free_chunk(struct list_head *chunk_list, void *mem)
{
	struct dasd_mchunk *chunk, *tmp;
	struct list_head *p, *left;

	chunk = (struct dasd_mchunk *)
		((char *) mem - sizeof(struct dasd_mchunk));
	
	left = chunk_list;
	list_for_each(p, chunk_list) {
		if (list_entry(p, struct dasd_mchunk, list) > chunk)
			break;
		left = p;
	}
	
	if (left->next != chunk_list) {
		tmp = list_entry(left->next, struct dasd_mchunk, list);
		if ((char *) (chunk + 1) + chunk->size == (char *) tmp) {
			list_del(&tmp->list);
			chunk->size += tmp->size + sizeof(struct dasd_mchunk);
		}
	}
	
	if (left != chunk_list) {
		tmp = list_entry(left, struct dasd_mchunk, list);
		if ((char *) (tmp + 1) + tmp->size == (char *) chunk) {
			tmp->size += chunk->size + sizeof(struct dasd_mchunk);
			return;
		}
	}
	__list_add(&chunk->list, left, left->next);
}

static inline int
dasd_check_blocksize(int bsize)
{
	if (bsize < 512 || bsize > 4096 || !is_power_of_2(bsize))
		return -EMEDIUMTYPE;
	return 0;
}

#define DASD_PROFILE_OFF	 0
#define DASD_PROFILE_ON 	 1
#define DASD_PROFILE_GLOBAL_ONLY 2

extern debug_info_t *dasd_debug_area;
extern struct dasd_profile_info dasd_global_profile_data;
extern unsigned int dasd_global_profile_level;
extern const struct block_device_operations dasd_device_operations;

extern struct kmem_cache *dasd_page_cache;

struct dasd_ccw_req *
dasd_kmalloc_request(int , int, int, struct dasd_device *);
struct dasd_ccw_req *
dasd_smalloc_request(int , int, int, struct dasd_device *);
void dasd_kfree_request(struct dasd_ccw_req *, struct dasd_device *);
void dasd_sfree_request(struct dasd_ccw_req *, struct dasd_device *);
void dasd_wakeup_cb(struct dasd_ccw_req *, void *);

static inline int
dasd_kmalloc_set_cda(struct ccw1 *ccw, void *cda, struct dasd_device *device)
{
	return set_normalized_cda(ccw, cda);
}

struct dasd_device *dasd_alloc_device(void);
void dasd_free_device(struct dasd_device *);

struct dasd_block *dasd_alloc_block(void);
void dasd_free_block(struct dasd_block *);

void dasd_enable_device(struct dasd_device *);
void dasd_set_target_state(struct dasd_device *, int);
void dasd_kick_device(struct dasd_device *);
void dasd_restore_device(struct dasd_device *);
void dasd_reload_device(struct dasd_device *);

void dasd_add_request_head(struct dasd_ccw_req *);
void dasd_add_request_tail(struct dasd_ccw_req *);
int  dasd_start_IO(struct dasd_ccw_req *);
int  dasd_term_IO(struct dasd_ccw_req *);
void dasd_schedule_device_bh(struct dasd_device *);
void dasd_schedule_block_bh(struct dasd_block *);
int  dasd_sleep_on(struct dasd_ccw_req *);
int  dasd_sleep_on_immediatly(struct dasd_ccw_req *);
int  dasd_sleep_on_interruptible(struct dasd_ccw_req *);
void dasd_device_set_timer(struct dasd_device *, int);
void dasd_device_clear_timer(struct dasd_device *);
void dasd_block_set_timer(struct dasd_block *, int);
void dasd_block_clear_timer(struct dasd_block *);
int  dasd_cancel_req(struct dasd_ccw_req *);
int dasd_flush_device_queue(struct dasd_device *);
int dasd_generic_probe (struct ccw_device *, struct dasd_discipline *);
void dasd_generic_remove (struct ccw_device *cdev);
int dasd_generic_set_online(struct ccw_device *, struct dasd_discipline *);
int dasd_generic_set_offline (struct ccw_device *cdev);
int dasd_generic_notify(struct ccw_device *, int);
int dasd_generic_last_path_gone(struct dasd_device *);
int dasd_generic_path_operational(struct dasd_device *);

void dasd_generic_handle_state_change(struct dasd_device *);
int dasd_generic_pm_freeze(struct ccw_device *);
int dasd_generic_restore_device(struct ccw_device *);
enum uc_todo dasd_generic_uc_handler(struct ccw_device *, struct irb *);
void dasd_generic_path_event(struct ccw_device *, int *);
int dasd_generic_verify_path(struct dasd_device *, __u8);

int dasd_generic_read_dev_chars(struct dasd_device *, int, void *, int);
char *dasd_get_sense(struct irb *);

void dasd_device_set_stop_bits(struct dasd_device *, int);
void dasd_device_remove_stop_bits(struct dasd_device *, int);

int dasd_device_is_ro(struct dasd_device *);

void dasd_profile_reset(struct dasd_profile *);
int dasd_profile_on(struct dasd_profile *);
void dasd_profile_off(struct dasd_profile *);
void dasd_global_profile_reset(void);
char *dasd_get_user_string(const char __user *, size_t);

extern int dasd_max_devindex;
extern int dasd_probeonly;
extern int dasd_autodetect;
extern int dasd_nopav;
extern int dasd_nofcx;

int dasd_devmap_init(void);
void dasd_devmap_exit(void);

struct dasd_device *dasd_create_device(struct ccw_device *);
void dasd_delete_device(struct dasd_device *);

int dasd_get_feature(struct ccw_device *, int);
int dasd_set_feature(struct ccw_device *, int, int);

int dasd_add_sysfs_files(struct ccw_device *);
void dasd_remove_sysfs_files(struct ccw_device *);

struct dasd_device *dasd_device_from_cdev(struct ccw_device *);
struct dasd_device *dasd_device_from_cdev_locked(struct ccw_device *);
struct dasd_device *dasd_device_from_devindex(int);

void dasd_add_link_to_gendisk(struct gendisk *, struct dasd_device *);
struct dasd_device *dasd_device_from_gendisk(struct gendisk *);

int dasd_parse(void);
int dasd_busid_known(const char *);

int  dasd_gendisk_init(void);
void dasd_gendisk_exit(void);
int dasd_gendisk_alloc(struct dasd_block *);
void dasd_gendisk_free(struct dasd_block *);
int dasd_scan_partitions(struct dasd_block *);
void dasd_destroy_partitions(struct dasd_block *);

int  dasd_ioctl(struct block_device *, fmode_t, unsigned int, unsigned long);

int dasd_proc_init(void);
void dasd_proc_exit(void);

struct dasd_ccw_req *dasd_default_erp_action(struct dasd_ccw_req *);
struct dasd_ccw_req *dasd_default_erp_postaction(struct dasd_ccw_req *);
struct dasd_ccw_req *dasd_alloc_erp_request(char *, int, int,
					    struct dasd_device *);
void dasd_free_erp_request(struct dasd_ccw_req *, struct dasd_device *);
void dasd_log_sense(struct dasd_ccw_req *, struct irb *);
void dasd_log_sense_dbf(struct dasd_ccw_req *cqr, struct irb *irb);

struct dasd_ccw_req *dasd_3990_erp_action(struct dasd_ccw_req *);
void dasd_3990_erp_handle_sim(struct dasd_device *, char *);

#ifdef CONFIG_DASD_EER
int dasd_eer_init(void);
void dasd_eer_exit(void);
int dasd_eer_enable(struct dasd_device *);
void dasd_eer_disable(struct dasd_device *);
void dasd_eer_write(struct dasd_device *, struct dasd_ccw_req *cqr,
		    unsigned int id);
void dasd_eer_snss(struct dasd_device *);

static inline int dasd_eer_enabled(struct dasd_device *device)
{
	return device->eer_cqr != NULL;
}
#else
#define dasd_eer_init()		(0)
#define dasd_eer_exit()		do { } while (0)
#define dasd_eer_enable(d)	(0)
#define dasd_eer_disable(d)	do { } while (0)
#define dasd_eer_write(d,c,i)	do { } while (0)
#define dasd_eer_snss(d)	do { } while (0)
#define dasd_eer_enabled(d)	(0)
#endif	

#endif				

#endif				
