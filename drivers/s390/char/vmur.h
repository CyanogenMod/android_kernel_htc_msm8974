/*
 * Linux driver for System z and s390 unit record devices
 * (z/VM virtual punch, reader, printer)
 *
 * Copyright IBM Corp. 2001, 2007
 * Authors: Malcolm Beattie <beattiem@uk.ibm.com>
 *	    Michael Holzheu <holzheu@de.ibm.com>
 *	    Frank Munzert <munzert@de.ibm.com>
 */

#ifndef _VMUR_H_
#define _VMUR_H_

#define DEV_CLASS_UR_I 0x20 
#define DEV_CLASS_UR_O 0x10 
#define READER_PUNCH_DEVTYPE	0x2540
#define PRINTER_DEVTYPE		0x1403

struct file_control_block {
	char reserved_1[8];
	char user_owner[8];
	char user_orig[8];
	__s32 data_recs;
	__s16 rec_len;
	__s16 file_num;
	__u8  file_stat;
	__u8  dev_type;
	char  reserved_2[6];
	char  file_name[12];
	char  file_type[12];
	char  create_date[8];
	char  create_time[8];
	char  reserved_3[6];
	__u8  file_class;
	__u8  sfb_lok;
	__u64 distr_code;
	__u32 reserved_4;
	__u8  current_starting_copy_number;
	__u8  sfblock_cntrl_flags;
	__u8  reserved_5;
	__u8  more_status_flags;
	char  rest[200];
} __attribute__ ((packed));

#define FLG_SYSTEM_HOLD	0x04
#define FLG_CP_DUMP	0x10
#define FLG_USER_HOLD	0x20
#define FLG_IN_USE	0x80

struct urdev {
	struct ccw_device *cdev;	
	struct mutex io_mutex;		
	struct completion *io_done;	
	struct device *device;
	struct cdev *char_device;
	struct ccw_dev_id dev_id;	
	size_t reclen;			
	int class;			
	int io_request_rc;		
	atomic_t ref_count;		
	wait_queue_head_t wait;		
	int open_flag;			
	spinlock_t open_lock;		
};

struct urfile {
	struct urdev *urd;
	unsigned int flags;
	size_t dev_reclen;
	__u16 file_reclen;
};


#define UR_MAJOR 0	
#define NUM_MINORS 65536

#define MAX_RECS_PER_IO		511
#define WRITE_CCW_CMD		0x01

#define TRACE(x...) debug_sprintf_event(vmur_dbf, 1, x)
#define CCWDEV_CU_DI(cutype, di) \
		CCW_DEVICE(cutype, 0x00), .driver_info = (di)

#define FILE_RECLEN_OFFSET	4064 

#endif 
