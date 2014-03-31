
#ifndef _ST_H
#define _ST_H

#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <scsi/scsi_cmnd.h>

struct st_cmdstatus {
	int midlevel_result;
	struct scsi_sense_hdr sense_hdr;
	int have_sense;
	int residual;
	u64 uremainder64;
	u8 flags;
	u8 remainder_valid;
	u8 fixed_format;
	u8 deferred;
};

struct scsi_tape;

struct st_request {
	unsigned char cmd[MAX_COMMAND_SIZE];
	unsigned char sense[SCSI_SENSE_BUFFERSIZE];
	int result;
	struct scsi_tape *stp;
	struct completion *waiting;
	struct bio *bio;
};

struct st_buffer {
	unsigned char dma;	
	unsigned char do_dio;   
	unsigned char cleared;  
	int buffer_size;
	int buffer_blocks;
	int buffer_bytes;
	int read_pointer;
	int writing;
	int syscall_result;
	struct st_request *last_SRpnt;
	struct st_cmdstatus cmdstat;
	struct page **reserved_pages;
	int reserved_page_order;
	struct page **mapped_pages;
	struct rq_map_data map_data;
	unsigned char *b_data;
	unsigned short use_sg;	
	unsigned short sg_segs;		
	unsigned short frp_segs;	
};

struct st_modedef {
	unsigned char defined;
	unsigned char sysv;	
	unsigned char do_async_writes;
	unsigned char do_buffer_writes;
	unsigned char do_read_ahead;
	unsigned char defaults_for_writes;
	unsigned char default_compression;	
	short default_density;	
	int default_blksize;	
	struct cdev *cdevs[2];  
};

#define ST_NBR_MODE_BITS 2
#define ST_NBR_MODES (1 << ST_NBR_MODE_BITS)
#define ST_MODE_SHIFT (7 - ST_NBR_MODE_BITS)
#define ST_MODE_MASK ((ST_NBR_MODES - 1) << ST_MODE_SHIFT)

#define ST_MAX_TAPES 128
#define ST_MAX_TAPE_ENTRIES  (ST_MAX_TAPES << (ST_NBR_MODE_BITS + 1))

struct st_partstat {
	unsigned char rw;
	unsigned char eof;
	unsigned char at_sm;
	unsigned char last_block_valid;
	u32 last_block_visited;
	int drv_block;		
	int drv_file;
};

#define ST_NBR_PARTITIONS 4

struct scsi_tape {
	struct scsi_driver *driver;
	struct scsi_device *device;
	struct mutex lock;	
	struct completion wait;	
	struct st_buffer *buffer;

	
	unsigned char omit_blklims;
	unsigned char do_auto_lock;
	unsigned char can_bsr;
	unsigned char can_partitions;
	unsigned char two_fm;
	unsigned char fast_mteom;
	unsigned char immediate;
	unsigned char restr_dma;
	unsigned char scsi2_logical;
	unsigned char default_drvbuffer;	
	unsigned char cln_mode;			
	unsigned char cln_sense_value;
	unsigned char cln_sense_mask;
	unsigned char use_pf;			
	unsigned char try_dio;			
	unsigned char try_dio_now;		
	unsigned char c_algo;			
	unsigned char pos_unknown;			
	unsigned char sili;			
	unsigned char immediate_filemark;	
	int tape_type;
	int long_timeout;	

	unsigned long max_pfn;	

	
	struct st_modedef modes[ST_NBR_MODES];
	int current_mode;

	
	int partition;
	int new_partition;
	int nbr_partitions;	
	struct st_partstat ps[ST_NBR_PARTITIONS];
	unsigned char dirty;
	unsigned char ready;
	unsigned char write_prot;
	unsigned char drv_write_prot;
	unsigned char in_use;
	unsigned char blksize_changed;
	unsigned char density_changed;
	unsigned char compression_changed;
	unsigned char drv_buffer;
	unsigned char density;
	unsigned char door_locked;
	unsigned char autorew_dev;   
	unsigned char rew_at_close;  
	unsigned char inited;
	unsigned char cleaning_req;  
	int block_size;
	int min_block;
	int max_block;
	int recover_count;     
	int recover_reg;       

#if DEBUG
	unsigned char write_pending;
	int nbr_finished;
	int nbr_waits;
	int nbr_requests;
	int nbr_dio;
	int nbr_pages;
	unsigned char last_cmnd[6];
	unsigned char last_sense[16];
#endif
	struct gendisk *disk;
	struct kref     kref;
};

#define USE_PF      1
#define PF_TESTED   2

#define	ST_NOEOF	0
#define ST_FM_HIT       1
#define ST_FM           2
#define ST_EOM_OK       3
#define ST_EOM_ERROR	4
#define	ST_EOD_1        5
#define ST_EOD_2        6
#define ST_EOD		7

#define	ST_IDLE		0
#define	ST_READING	1
#define	ST_WRITING	2

#define ST_READY	0
#define ST_NOT_READY	1
#define ST_NO_TAPE	2

#define ST_UNLOCKED	0
#define ST_LOCKED_EXPLICIT 1
#define ST_LOCKED_AUTO  2
#define ST_LOCK_FAILS   3

#define	QFA_REQUEST_BLOCK	0x02
#define	QFA_SEEK_BLOCK		0x0c

#define ST_DONT_TOUCH  0
#define ST_NO          1
#define ST_YES         2

#define EXTENDED_SENSE_START  18

#define SENSE_FMK   0x80
#define SENSE_EOM   0x40
#define SENSE_ILI   0x20

#endif
