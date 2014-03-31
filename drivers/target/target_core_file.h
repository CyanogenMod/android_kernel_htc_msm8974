#ifndef TARGET_CORE_FILE_H
#define TARGET_CORE_FILE_H

#define FD_VERSION		"4.0"

#define FD_MAX_DEV_NAME		256
#define FD_DEVICE_QUEUE_DEPTH	32
#define FD_MAX_DEVICE_QUEUE_DEPTH 128
#define FD_BLOCKSIZE		512
#define FD_MAX_SECTORS		1024

#define RRF_EMULATE_CDB		0x01
#define RRF_GOT_LBA		0x02

struct fd_request {
	struct se_task	fd_task;
};

#define FBDF_HAS_PATH		0x01
#define FBDF_HAS_SIZE		0x02
#define FDBD_USE_BUFFERED_IO	0x04

struct fd_dev {
	u32		fbd_flags;
	unsigned char	fd_dev_name[FD_MAX_DEV_NAME];
	
	u32		fd_dev_id;
	
	u32		fd_table_count;
	u32		fd_queue_depth;
	u32		fd_block_size;
	unsigned long long fd_dev_size;
	struct file	*fd_file;
	
	struct fd_host *fd_host;
} ____cacheline_aligned;

struct fd_host {
	u32		fd_host_dev_id_count;
	
	u32		fd_host_id;
} ____cacheline_aligned;

#endif 
