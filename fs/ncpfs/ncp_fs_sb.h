/*
 *  ncp_fs_sb.h
 *
 *  Copyright (C) 1995, 1996 by Volker Lendecke
 *
 */

#ifndef _NCP_FS_SB
#define _NCP_FS_SB

#include <linux/types.h>
#include <linux/ncp_mount.h>
#include <linux/net.h>
#include <linux/mutex.h>
#include <linux/backing-dev.h>
#include <linux/workqueue.h>

#define NCP_DEFAULT_OPTIONS 0		

struct sock;

struct ncp_mount_data_kernel {
	unsigned long    flags;		
	unsigned int	 int_flags;	
#define NCP_IMOUNT_LOGGEDIN_POSSIBLE	0x0001
	__kernel_uid32_t mounted_uid;	
	struct pid      *wdog_pid;	
	unsigned int     ncp_fd;	
	unsigned int     time_out;	
	unsigned int     retry_count;	
	unsigned char	 mounted_vol[NCP_VOLNAME_LEN + 1];
	__kernel_uid32_t uid;
	__kernel_gid32_t gid;
	__kernel_mode_t  file_mode;
	__kernel_mode_t  dir_mode;
	int		 info_fd;
};

struct ncp_server {

	struct ncp_mount_data_kernel m;	

	__u8 name_space[NCP_NUMBER_OF_VOLUMES + 2];

	struct file *ncp_filp;	
	struct socket *ncp_sock;
	struct file *info_filp;
	struct socket *info_sock;

	u8 sequence;
	u8 task;
	u16 connection;		

	u8 completion;		
	u8 conn_status;		

	int buffer_size;	

	int reply_size;		

	int packet_size;
	unsigned char *packet;	
	unsigned char *txbuf;	
	unsigned char *rxbuf;	

	int lock;		
	struct mutex mutex;

	int current_size;	
	int has_subfunction;
	int ncp_reply_size;

	int root_setuped;
	struct mutex root_setup_lock;

	
	int sign_wanted;	
	int sign_active;	
	char sign_root[8];	
	char sign_last[16];	

	
	struct {
		int	auth_type;
		size_t	object_name_len;
		void*	object_name;
		int	object_type;
	} auth;
	
	struct {
		size_t	len;
		void*	data;
	} priv;
	struct rw_semaphore auth_rwsem;

	
	struct nls_table *nls_vol;
	struct nls_table *nls_io;

	
	atomic_t dentry_ttl;

	
	unsigned int flags;

	spinlock_t requests_lock;	

	void (*data_ready)(struct sock* sk, int len);
	void (*error_report)(struct sock* sk);
	void (*write_space)(struct sock* sk);	
	struct {
		struct work_struct tq;		
		struct ncp_request_reply* creq;	
		struct mutex creq_mutex;	

		unsigned int state;		
		struct {
			__u32 magic __packed;
			__u32 len __packed;
			__u16 type __packed;
			__u16 p1 __packed;
			__u16 p2 __packed;
			__u16 p3 __packed;
			__u16 type2 __packed;
		} buf;				
		unsigned char* ptr;		
		size_t len;			
	} rcv;
	struct {
		struct list_head requests;	
		struct work_struct tq;		
		struct ncp_request_reply* creq;	
	} tx;
	struct timer_list timeout_tm;		
	struct work_struct timeout_tq;		
	int timeout_last;			
	int timeout_retries;			
	struct {
		size_t len;
		__u8 data[128];
	} unexpected_packet;
	struct backing_dev_info bdi;
};

extern void ncp_tcp_rcv_proc(struct work_struct *work);
extern void ncp_tcp_tx_proc(struct work_struct *work);
extern void ncpdgram_rcv_proc(struct work_struct *work);
extern void ncpdgram_timeout_proc(struct work_struct *work);
extern void ncpdgram_timeout_call(unsigned long server);
extern void ncp_tcp_data_ready(struct sock* sk, int len);
extern void ncp_tcp_write_space(struct sock* sk);
extern void ncp_tcp_error_report(struct sock* sk);

#define NCP_FLAG_UTF8	1

#define NCP_CLR_FLAG(server, flag)	((server)->flags &= ~(flag))
#define NCP_SET_FLAG(server, flag)	((server)->flags |= (flag))
#define NCP_IS_FLAG(server, flag)	((server)->flags & (flag))

static inline int ncp_conn_valid(struct ncp_server *server)
{
	return ((server->conn_status & 0x11) == 0);
}

static inline void ncp_invalidate_conn(struct ncp_server *server)
{
	server->conn_status |= 0x01;
}

#endif
