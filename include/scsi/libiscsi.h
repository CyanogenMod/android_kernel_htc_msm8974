/*
 * iSCSI lib definitions
 *
 * Copyright (C) 2006 Red Hat, Inc.  All rights reserved.
 * Copyright (C) 2004 - 2006 Mike Christie
 * Copyright (C) 2004 - 2005 Dmitry Yusupov
 * Copyright (C) 2004 - 2005 Alex Aizman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef LIBISCSI_H
#define LIBISCSI_H

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <scsi/iscsi_proto.h>
#include <scsi/iscsi_if.h>
#include <scsi/scsi_transport_iscsi.h>

struct scsi_transport_template;
struct scsi_host_template;
struct scsi_device;
struct Scsi_Host;
struct scsi_target;
struct scsi_cmnd;
struct socket;
struct iscsi_transport;
struct iscsi_cls_session;
struct iscsi_cls_conn;
struct iscsi_session;
struct iscsi_nopin;
struct device;

#define ISCSI_DEF_XMIT_CMDS_MAX	128	
#define ISCSI_MGMT_CMDS_MAX	15

#define ISCSI_DEF_CMD_PER_LUN	32

enum {
	TMF_INITIAL,
	TMF_QUEUED,
	TMF_SUCCESS,
	TMF_FAILED,
	TMF_TIMEDOUT,
	TMF_NOT_FOUND,
};

#define ISCSI_SUSPEND_BIT		1

#define ISCSI_ITT_MASK			0x1fff
#define ISCSI_TOTAL_CMDS_MAX		4096
#define ISCSI_TOTAL_CMDS_MIN		16
#define ISCSI_AGE_SHIFT			28
#define ISCSI_AGE_MASK			0xf

#define ISCSI_ADDRESS_BUF_LEN		64

enum {
	
	ISCSI_MAX_AHS_SIZE = sizeof(struct iscsi_ecdb_ahdr) +
				sizeof(struct iscsi_rlength_ahdr),
	ISCSI_DIGEST_SIZE = sizeof(__u32),
};


enum {
	ISCSI_TASK_FREE,
	ISCSI_TASK_COMPLETED,
	ISCSI_TASK_PENDING,
	ISCSI_TASK_RUNNING,
	ISCSI_TASK_ABRT_TMF,		
	ISCSI_TASK_ABRT_SESS_RECOV,	
	ISCSI_TASK_REQUEUE_SCSIQ,	
};

struct iscsi_r2t_info {
	__be32			ttt;		
	__be32			exp_statsn;	
	uint32_t		data_length;	
	uint32_t		data_offset;	
	int			data_count;	
	int			datasn;
	
	int			sent;		
};

struct iscsi_task {
	struct iscsi_hdr	*hdr;
	unsigned short		hdr_max;
	unsigned short		hdr_len;	
	
	itt_t			hdr_itt;
	__be32			cmdsn;
	struct scsi_lun		lun;

	int			itt;		

	unsigned		imm_count;	
	
	struct iscsi_r2t_info	unsol_r2t;
	char			*data;		
	unsigned		data_count;
	struct scsi_cmnd	*sc;		
	struct iscsi_conn	*conn;		

	
	unsigned long		last_xfer;
	unsigned long		last_timeout;
	bool			have_checked_conn;
	
	int			state;
	atomic_t		refcount;
	struct list_head	running;	
	void			*dd_data;	
};

static inline int iscsi_task_has_unsol_data(struct iscsi_task *task)
{
	return task->unsol_r2t.data_length > task->unsol_r2t.sent;
}

static inline void* iscsi_next_hdr(struct iscsi_task *task)
{
	return (void*)task->hdr + task->hdr_len;
}

enum {
	ISCSI_CONN_INITIAL_STAGE,
	ISCSI_CONN_STARTED,
	ISCSI_CONN_STOPPED,
	ISCSI_CONN_CLEANUP_WAIT,
};

struct iscsi_conn {
	struct iscsi_cls_conn	*cls_conn;	
	void			*dd_data;	
	struct iscsi_session	*session;	
        int			stop_stage;
	struct timer_list	transport_timer;
	unsigned long		last_recv;
	unsigned long		last_ping;
	int			ping_timeout;
	int			recv_timeout;
	struct iscsi_task 	*ping_task;

	
	uint32_t		exp_statsn;

	
	int			id;		
	int			c_stage;	
	char			*data;
	struct iscsi_task 	*login_task;	
	struct iscsi_task	*task;		

	
	struct list_head	mgmtqueue;	
	struct list_head	cmdqueue;	
	struct list_head	requeue;	
	struct work_struct	xmitwork;	
	unsigned long		suspend_tx;	
	unsigned long		suspend_rx;	

	
	wait_queue_head_t	ehwait;		
	struct iscsi_tm		tmhdr;
	struct timer_list	tmf_timer;
	int			tmf_state;	

	
	unsigned		max_recv_dlength; 
	unsigned		max_xmit_dlength; 
	int			hdrdgst_en;
	int			datadgst_en;
	int			ifmarker_en;
	int			ofmarker_en;
	
	int			persistent_port;
	char			*persistent_address;

	
	uint64_t		txdata_octets;
	uint64_t		rxdata_octets;
	uint32_t		scsicmd_pdus_cnt;
	uint32_t		dataout_pdus_cnt;
	uint32_t		scsirsp_pdus_cnt;
	uint32_t		datain_pdus_cnt;
	uint32_t		r2t_pdus_cnt;
	uint32_t		tmfcmd_pdus_cnt;
	int32_t			tmfrsp_pdus_cnt;

	
	uint32_t		eh_abort_cnt;
	uint32_t		fmr_unalign_cnt;
};

struct iscsi_pool {
	struct kfifo		queue;		
	void			**pool;		
	int			max;		
};

enum {
	ISCSI_STATE_FREE = 1,
	ISCSI_STATE_LOGGED_IN,
	ISCSI_STATE_FAILED,
	ISCSI_STATE_TERMINATE,
	ISCSI_STATE_IN_RECOVERY,
	ISCSI_STATE_RECOVERY_FAILED,
	ISCSI_STATE_LOGGING_OUT,
};

struct iscsi_session {
	struct iscsi_cls_session *cls_session;
	struct mutex		eh_mutex;

	
	uint32_t		cmdsn;
	uint32_t		exp_cmdsn;
	uint32_t		max_cmdsn;

	
	uint32_t		queued_cmdsn;

	
	int			abort_timeout;
	int			lu_reset_timeout;
	int			tgt_reset_timeout;
	int			initial_r2t_en;
	unsigned short		max_r2t;
	int			imm_data_en;
	unsigned		first_burst;
	unsigned		max_burst;
	int			time2wait;
	int			time2retain;
	int			pdu_inorder_en;
	int			dataseq_inorder_en;
	int			erl;
	int			fast_abort;
	int			tpgt;
	char			*username;
	char			*username_in;
	char			*password;
	char			*password_in;
	char			*targetname;
	char			*targetalias;
	char			*ifacename;
	char			*initiatorname;
	
	struct iscsi_transport	*tt;
	struct Scsi_Host	*host;
	struct iscsi_conn	*leadconn;	
	spinlock_t		lock;		
	int			state;		
	int			age;		

	int			scsi_cmds_max; 	
	int			cmds_max;	
	struct iscsi_task	**cmds;		
	struct iscsi_pool	cmdpool;	
	void			*dd_data;	
};

enum {
	ISCSI_HOST_SETUP,
	ISCSI_HOST_REMOVED,
};

struct iscsi_host {
	char			*initiatorname;
	
	char			*hwaddress;
	char			*netdev;

	wait_queue_head_t	session_removal_wq;
	
	spinlock_t		lock;
	int			num_sessions;
	int			state;

	struct workqueue_struct	*workq;
	char			workq_name[20];
};

extern int iscsi_change_queue_depth(struct scsi_device *sdev, int depth,
				    int reason);
extern int iscsi_eh_abort(struct scsi_cmnd *sc);
extern int iscsi_eh_recover_target(struct scsi_cmnd *sc);
extern int iscsi_eh_session_reset(struct scsi_cmnd *sc);
extern int iscsi_eh_device_reset(struct scsi_cmnd *sc);
extern int iscsi_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *sc);

#define iscsi_host_priv(_shost) \
	(shost_priv(_shost) + sizeof(struct iscsi_host))

extern int iscsi_host_set_param(struct Scsi_Host *shost,
				enum iscsi_host_param param, char *buf,
				int buflen);
extern int iscsi_host_get_param(struct Scsi_Host *shost,
				enum iscsi_host_param param, char *buf);
extern int iscsi_host_add(struct Scsi_Host *shost, struct device *pdev);
extern struct Scsi_Host *iscsi_host_alloc(struct scsi_host_template *sht,
					  int dd_data_size,
					  bool xmit_can_sleep);
extern void iscsi_host_remove(struct Scsi_Host *shost);
extern void iscsi_host_free(struct Scsi_Host *shost);
extern int iscsi_target_alloc(struct scsi_target *starget);

extern struct iscsi_cls_session *
iscsi_session_setup(struct iscsi_transport *, struct Scsi_Host *shost,
		    uint16_t, int, int, uint32_t, unsigned int);
extern void iscsi_session_teardown(struct iscsi_cls_session *);
extern void iscsi_session_recovery_timedout(struct iscsi_cls_session *);
extern int iscsi_set_param(struct iscsi_cls_conn *cls_conn,
			   enum iscsi_param param, char *buf, int buflen);
extern int iscsi_session_get_param(struct iscsi_cls_session *cls_session,
				   enum iscsi_param param, char *buf);

#define iscsi_session_printk(prefix, _sess, fmt, a...)	\
	iscsi_cls_session_printk(prefix, _sess->cls_session, fmt, ##a)

extern struct iscsi_cls_conn *iscsi_conn_setup(struct iscsi_cls_session *,
					       int, uint32_t);
extern void iscsi_conn_teardown(struct iscsi_cls_conn *);
extern int iscsi_conn_start(struct iscsi_cls_conn *);
extern void iscsi_conn_stop(struct iscsi_cls_conn *, int);
extern int iscsi_conn_bind(struct iscsi_cls_session *, struct iscsi_cls_conn *,
			   int);
extern void iscsi_conn_failure(struct iscsi_conn *conn, enum iscsi_err err);
extern void iscsi_session_failure(struct iscsi_session *session,
				  enum iscsi_err err);
extern int iscsi_conn_get_param(struct iscsi_cls_conn *cls_conn,
				enum iscsi_param param, char *buf);
extern int iscsi_conn_get_addr_param(struct sockaddr_storage *addr,
				     enum iscsi_param param, char *buf);
extern void iscsi_suspend_tx(struct iscsi_conn *conn);
extern void iscsi_suspend_queue(struct iscsi_conn *conn);
extern void iscsi_conn_queue_work(struct iscsi_conn *conn);

#define iscsi_conn_printk(prefix, _c, fmt, a...) \
	iscsi_cls_conn_printk(prefix, ((struct iscsi_conn *)_c)->cls_conn, \
			      fmt, ##a)

extern void iscsi_update_cmdsn(struct iscsi_session *, struct iscsi_nopin *);
extern void iscsi_prep_data_out_pdu(struct iscsi_task *task,
				    struct iscsi_r2t_info *r2t,
				    struct iscsi_data *hdr);
extern int iscsi_conn_send_pdu(struct iscsi_cls_conn *, struct iscsi_hdr *,
				char *, uint32_t);
extern int iscsi_complete_pdu(struct iscsi_conn *, struct iscsi_hdr *,
			      char *, int);
extern int __iscsi_complete_pdu(struct iscsi_conn *, struct iscsi_hdr *,
				char *, int);
extern int iscsi_verify_itt(struct iscsi_conn *, itt_t);
extern struct iscsi_task *iscsi_itt_to_ctask(struct iscsi_conn *, itt_t);
extern struct iscsi_task *iscsi_itt_to_task(struct iscsi_conn *, itt_t);
extern void iscsi_requeue_task(struct iscsi_task *task);
extern void iscsi_put_task(struct iscsi_task *task);
extern void __iscsi_put_task(struct iscsi_task *task);
extern void __iscsi_get_task(struct iscsi_task *task);
extern void iscsi_complete_scsi_task(struct iscsi_task *task,
				     uint32_t exp_cmdsn, uint32_t max_cmdsn);

extern void iscsi_pool_free(struct iscsi_pool *);
extern int iscsi_pool_init(struct iscsi_pool *, int, void ***, int);

static inline unsigned int
iscsi_padded(unsigned int len)
{
	return (len + ISCSI_PAD_LEN - 1) & ~(ISCSI_PAD_LEN - 1);
}

static inline unsigned int
iscsi_padding(unsigned int len)
{
	len &= (ISCSI_PAD_LEN - 1);
	if (len)
		len = ISCSI_PAD_LEN - len;
	return len;
}

#endif
