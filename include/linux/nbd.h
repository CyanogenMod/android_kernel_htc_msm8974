/*
 * 1999 Copyright (C) Pavel Machek, pavel@ucw.cz. This code is GPL.
 * 1999/11/04 Copyright (C) 1999 VMware, Inc. (Regis "HPReg" Duchesne)
 *            Made nbd_end_request() use the io_request_lock
 * 2001 Copyright (C) Steven Whitehouse
 *            New nbd_end_request() for compatibility with new linux block
 *            layer code.
 * 2003/06/24 Louis D. Langholtz <ldl@aros.net>
 *            Removed unneeded blksize_bits field from nbd_device struct.
 *            Cleanup PARANOIA usage & code.
 * 2004/02/19 Paul Clements
 *            Removed PARANOIA, plus various cleanup and comments
 */

#ifndef LINUX_NBD_H
#define LINUX_NBD_H

#include <linux/types.h>

#define NBD_SET_SOCK	_IO( 0xab, 0 )
#define NBD_SET_BLKSIZE	_IO( 0xab, 1 )
#define NBD_SET_SIZE	_IO( 0xab, 2 )
#define NBD_DO_IT	_IO( 0xab, 3 )
#define NBD_CLEAR_SOCK	_IO( 0xab, 4 )
#define NBD_CLEAR_QUE	_IO( 0xab, 5 )
#define NBD_PRINT_DEBUG	_IO( 0xab, 6 )
#define NBD_SET_SIZE_BLOCKS	_IO( 0xab, 7 )
#define NBD_DISCONNECT  _IO( 0xab, 8 )
#define NBD_SET_TIMEOUT _IO( 0xab, 9 )

enum {
	NBD_CMD_READ = 0,
	NBD_CMD_WRITE = 1,
	NBD_CMD_DISC = 2
};

#define nbd_cmd(req) ((req)->cmd[0])

#ifdef __KERNEL__

#include <linux/wait.h>
#include <linux/mutex.h>

#define NBD_READ_ONLY 0x0001
#define NBD_WRITE_NOCHK 0x0002

struct request;

struct nbd_device {
	int flags;
	int harderror;		
	struct socket * sock;
	struct file * file; 	
	int magic;

	spinlock_t queue_lock;
	struct list_head queue_head;	
	struct request *active_req;
	wait_queue_head_t active_wq;
	struct list_head waiting_queue;	
	wait_queue_head_t waiting_wq;

	struct mutex tx_lock;
	struct gendisk *disk;
	int blksize;
	u64 bytesize;
	pid_t pid; 
	int xmit_timeout;
};

#endif


#define NBD_REQUEST_MAGIC 0x25609513
#define NBD_REPLY_MAGIC 0x67446698

struct nbd_request {
	__be32 magic;
	__be32 type;	
	char handle[8];
	__be64 from;
	__be32 len;
} __attribute__((packed));

struct nbd_reply {
	__be32 magic;
	__be32 error;		
	char handle[8];		
};
#endif
