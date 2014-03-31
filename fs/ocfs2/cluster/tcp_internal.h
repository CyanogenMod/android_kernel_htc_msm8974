/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * Copyright (C) 2005 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef O2CLUSTER_TCP_INTERNAL_H
#define O2CLUSTER_TCP_INTERNAL_H

#define O2NET_MSG_MAGIC           ((u16)0xfa55)
#define O2NET_MSG_STATUS_MAGIC    ((u16)0xfa56)
#define O2NET_MSG_KEEP_REQ_MAGIC  ((u16)0xfa57)
#define O2NET_MSG_KEEP_RESP_MAGIC ((u16)0xfa58)

#define O2NET_QUORUM_DELAY_MS	((o2hb_dead_threshold + 2) * O2HB_REGION_TIMEOUT_MS)

#define O2NET_PROTOCOL_VERSION 11ULL
struct o2net_handshake {
	__be64	protocol_version;
	__be64	connector_id;
	__be32  o2hb_heartbeat_timeout_ms;
	__be32  o2net_idle_timeout_ms;
	__be32  o2net_keepalive_delay_ms;
	__be32  o2net_reconnect_delay_ms;
};

struct o2net_node {
	
	spinlock_t			nn_lock;

	
	struct o2net_sock_container	*nn_sc;
	
	unsigned			nn_sc_valid:1;
	
	int				nn_persistent_error;
	
	atomic_t			nn_timeout;

	wait_queue_head_t		nn_sc_wq;

	struct idr			nn_status_idr;
	struct list_head		nn_status_list;

	struct delayed_work		nn_connect_work;
	unsigned long			nn_last_connect_attempt;

	struct delayed_work		nn_connect_expired;

	struct delayed_work		nn_still_up;
};

struct o2net_sock_container {
	struct kref		sc_kref;
	
	struct socket		*sc_sock;
	struct o2nm_node	*sc_node;


	struct work_struct	sc_rx_work;
	struct work_struct	sc_connect_work;
	struct work_struct	sc_shutdown_work;

	struct timer_list	sc_idle_timeout;
	struct delayed_work	sc_keepalive_work;

	unsigned		sc_handshake_ok:1;

	struct page 		*sc_page;
	size_t			sc_page_off;

	
	void			(*sc_state_change)(struct sock *sk);
	void			(*sc_data_ready)(struct sock *sk, int bytes);

	u32			sc_msg_key;
	u16			sc_msg_type;

#ifdef CONFIG_DEBUG_FS
	struct list_head        sc_net_debug_item;
	ktime_t			sc_tv_timer;
	ktime_t			sc_tv_data_ready;
	ktime_t			sc_tv_advance_start;
	ktime_t			sc_tv_advance_stop;
	ktime_t			sc_tv_func_start;
	ktime_t			sc_tv_func_stop;
#endif
#ifdef CONFIG_OCFS2_FS_STATS
	ktime_t			sc_tv_acquiry_total;
	ktime_t			sc_tv_send_total;
	ktime_t			sc_tv_status_total;
	u32			sc_send_count;
	u32			sc_recv_count;
	ktime_t			sc_tv_process_total;
#endif
	struct mutex		sc_send_lock;
};

struct o2net_msg_handler {
	struct rb_node		nh_node;
	u32			nh_max_len;
	u32			nh_msg_type;
	u32			nh_key;
	o2net_msg_handler_func	*nh_func;
	o2net_msg_handler_func	*nh_func_data;
	o2net_post_msg_handler_func
				*nh_post_func;
	struct kref		nh_kref;
	struct list_head	nh_unregister_item;
};

enum o2net_system_error {
	O2NET_ERR_NONE = 0,
	O2NET_ERR_NO_HNDLR,
	O2NET_ERR_OVERFLOW,
	O2NET_ERR_DIED,
	O2NET_ERR_MAX
};

struct o2net_status_wait {
	enum o2net_system_error	ns_sys_status;
	s32			ns_status;
	int			ns_id;
	wait_queue_head_t	ns_wq;
	struct list_head	ns_node_item;
};

#ifdef CONFIG_DEBUG_FS
struct o2net_send_tracking {
	struct list_head		st_net_debug_item;
	struct task_struct		*st_task;
	struct o2net_sock_container	*st_sc;
	u32				st_id;
	u32				st_msg_type;
	u32				st_msg_key;
	u8				st_node;
	ktime_t				st_sock_time;
	ktime_t				st_send_time;
	ktime_t				st_status_time;
};
#else
struct o2net_send_tracking {
	u32	dummy;
};
#endif	

#endif 
