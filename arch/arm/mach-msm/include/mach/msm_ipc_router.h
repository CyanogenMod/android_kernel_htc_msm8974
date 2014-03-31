/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_IPC_ROUTER_H
#define _MSM_IPC_ROUTER_H

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/wakelock.h>
#include <linux/msm_ipc.h>

#define MAX_WAKELOCK_NAME_SZ 32

enum msm_ipc_router_event {
	MSM_IPC_ROUTER_READ_CB = 0,
	MSM_IPC_ROUTER_WRITE_DONE,
	MSM_IPC_ROUTER_RESUME_TX,
};

struct comm_mode_info {
	int mode;
	void *xprt_info;
};

struct msm_ipc_port {
	struct list_head list;

	struct msm_ipc_port_addr this_port;
	struct msm_ipc_port_name port_name;
	uint32_t type;
	unsigned flags;
	spinlock_t port_lock;
	struct comm_mode_info mode_info;

	struct list_head port_rx_q;
	struct mutex port_rx_q_lock_lhb3;
	char rx_wakelock_name[MAX_WAKELOCK_NAME_SZ];
	struct wake_lock port_rx_wake_lock;
	wait_queue_head_t port_rx_wait_q;

	int restart_state;
	spinlock_t restart_lock;
	wait_queue_head_t restart_wait;

	void *endpoint;
	void (*notify)(unsigned event, void *priv);
	int (*check_send_permissions)(void *data);

	uint32_t num_tx;
	uint32_t num_rx;
	unsigned long num_tx_bytes;
	unsigned long num_rx_bytes;
	void *priv;
};

#ifdef CONFIG_MSM_IPC_ROUTER
struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *priv),
	void *priv);

int msm_ipc_router_lookup_server_name(struct msm_ipc_port_name *srv_name,
				      struct msm_ipc_server_info *srv_info,
				      int num_entries_in_array,
				      uint32_t lookup_mask);

int msm_ipc_router_send_msg(struct msm_ipc_port *src,
			    struct msm_ipc_addr *dest,
			    void *data, unsigned int data_len);

int msm_ipc_router_get_curr_pkt_size(struct msm_ipc_port *port_ptr);

int msm_ipc_router_read_msg(struct msm_ipc_port *port_ptr,
			    struct msm_ipc_addr *src,
			    unsigned char **data,
			    unsigned int *len);

int msm_ipc_router_close_port(struct msm_ipc_port *port_ptr);

#else

struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *priv),
	void *priv)
{
	return NULL;
}

int msm_ipc_router_lookup_server_name(struct msm_ipc_port_name *srv_name,
				      struct msm_ipc_server_info *srv_info,
				      int num_entries_in_array,
				      uint32_t lookup_mask)
{
	return -ENODEV;
}

int msm_ipc_router_send_msg(struct msm_ipc_port *src,
			    struct msm_ipc_addr *dest,
			    void *data, unsigned int data_len)
{
	return -ENODEV;
}

int msm_ipc_router_get_curr_pkt_size(struct msm_ipc_port *port_ptr)
{
	return -ENODEV;
}

int msm_ipc_router_read_msg(struct msm_ipc_port *port_ptr,
			    struct msm_ipc_addr *src,
			    unsigned char **data,
			    unsigned int *len)
{
	return -ENODEV;
}

int msm_ipc_router_close_port(struct msm_ipc_port *port_ptr)
{
	return -ENODEV;
}

#endif

#endif
