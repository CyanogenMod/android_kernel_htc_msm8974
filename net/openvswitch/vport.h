/*
 * Copyright (c) 2007-2011 Nicira Networks.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef VPORT_H
#define VPORT_H 1

#include <linux/list.h>
#include <linux/openvswitch.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/u64_stats_sync.h>

#include "datapath.h"

struct vport;
struct vport_parms;


int ovs_vport_init(void);
void ovs_vport_exit(void);

struct vport *ovs_vport_add(const struct vport_parms *);
void ovs_vport_del(struct vport *);

struct vport *ovs_vport_locate(const char *name);

void ovs_vport_get_stats(struct vport *, struct ovs_vport_stats *);

int ovs_vport_set_options(struct vport *, struct nlattr *options);
int ovs_vport_get_options(const struct vport *, struct sk_buff *);

int ovs_vport_send(struct vport *, struct sk_buff *);


struct vport_percpu_stats {
	u64 rx_bytes;
	u64 rx_packets;
	u64 tx_bytes;
	u64 tx_packets;
	struct u64_stats_sync sync;
};

struct vport_err_stats {
	u64 rx_dropped;
	u64 rx_errors;
	u64 tx_dropped;
	u64 tx_errors;
};

struct vport {
	struct rcu_head rcu;
	u16 port_no;
	struct datapath	*dp;
	struct list_head node;
	u32 upcall_pid;

	struct hlist_node hash_node;
	const struct vport_ops *ops;

	struct vport_percpu_stats __percpu *percpu_stats;

	spinlock_t stats_lock;
	struct vport_err_stats err_stats;
};

struct vport_parms {
	const char *name;
	enum ovs_vport_type type;
	struct nlattr *options;

	
	struct datapath *dp;
	u16 port_no;
	u32 upcall_pid;
};

struct vport_ops {
	enum ovs_vport_type type;

	
	struct vport *(*create)(const struct vport_parms *);
	void (*destroy)(struct vport *);

	int (*set_options)(struct vport *, struct nlattr *);
	int (*get_options)(const struct vport *, struct sk_buff *);

	
	const char *(*get_name)(const struct vport *);
	void (*get_config)(const struct vport *, void *);
	int (*get_ifindex)(const struct vport *);

	int (*send)(struct vport *, struct sk_buff *);
};

enum vport_err_type {
	VPORT_E_RX_DROPPED,
	VPORT_E_RX_ERROR,
	VPORT_E_TX_DROPPED,
	VPORT_E_TX_ERROR,
};

struct vport *ovs_vport_alloc(int priv_size, const struct vport_ops *,
			      const struct vport_parms *);
void ovs_vport_free(struct vport *);

#define VPORT_ALIGN 8

static inline void *vport_priv(const struct vport *vport)
{
	return (u8 *)vport + ALIGN(sizeof(struct vport), VPORT_ALIGN);
}

static inline struct vport *vport_from_priv(const void *priv)
{
	return (struct vport *)(priv - ALIGN(sizeof(struct vport), VPORT_ALIGN));
}

void ovs_vport_receive(struct vport *, struct sk_buff *);
void ovs_vport_record_error(struct vport *, enum vport_err_type err_type);

extern const struct vport_ops ovs_netdev_vport_ops;
extern const struct vport_ops ovs_internal_vport_ops;

#endif 
