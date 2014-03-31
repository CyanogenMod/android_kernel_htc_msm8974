/*
 * Copyright (c) 2006-2008 Chelsio, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _CXGB3_OFFLOAD_H
#define _CXGB3_OFFLOAD_H

#include <linux/list.h>
#include <linux/skbuff.h>

#include "l2t.h"

#include "t3cdev.h"
#include "t3_cpl.h"

struct adapter;

void cxgb3_offload_init(void);

void cxgb3_adapter_ofld(struct adapter *adapter);
void cxgb3_adapter_unofld(struct adapter *adapter);
int cxgb3_offload_activate(struct adapter *adapter);
void cxgb3_offload_deactivate(struct adapter *adapter);

void cxgb3_set_dummy_ops(struct t3cdev *dev);

struct t3cdev *dev2t3cdev(struct net_device *dev);


void cxgb3_register_client(struct cxgb3_client *client);
void cxgb3_unregister_client(struct cxgb3_client *client);
void cxgb3_add_clients(struct t3cdev *tdev);
void cxgb3_remove_clients(struct t3cdev *tdev);
void cxgb3_event_notify(struct t3cdev *tdev, u32 event, u32 port);

typedef int (*cxgb3_cpl_handler_func)(struct t3cdev *dev,
				      struct sk_buff *skb, void *ctx);

enum {
	OFFLOAD_STATUS_UP,
	OFFLOAD_STATUS_DOWN,
	OFFLOAD_PORT_DOWN,
	OFFLOAD_PORT_UP,
	OFFLOAD_DB_FULL,
	OFFLOAD_DB_EMPTY,
	OFFLOAD_DB_DROP
};

struct cxgb3_client {
	char *name;
	void (*add) (struct t3cdev *);
	void (*remove) (struct t3cdev *);
	cxgb3_cpl_handler_func *handlers;
	int (*redirect)(void *ctx, struct dst_entry *old,
			struct dst_entry *new, struct l2t_entry *l2t);
	struct list_head client_list;
	void (*event_handler)(struct t3cdev *tdev, u32 event, u32 port);
};

int cxgb3_alloc_atid(struct t3cdev *dev, struct cxgb3_client *client,
		     void *ctx);
int cxgb3_alloc_stid(struct t3cdev *dev, struct cxgb3_client *client,
		     void *ctx);
void *cxgb3_free_atid(struct t3cdev *dev, int atid);
void cxgb3_free_stid(struct t3cdev *dev, int stid);
void cxgb3_insert_tid(struct t3cdev *dev, struct cxgb3_client *client,
		      void *ctx, unsigned int tid);
void cxgb3_queue_tid_release(struct t3cdev *dev, unsigned int tid);
void cxgb3_remove_tid(struct t3cdev *dev, void *ctx, unsigned int tid);

struct t3c_tid_entry {
	struct cxgb3_client *client;
	void *ctx;
};

enum {
	CPL_PRIORITY_DATA = 0,	
	CPL_PRIORITY_SETUP = 1,	
	CPL_PRIORITY_TEARDOWN = 0,	
	CPL_PRIORITY_LISTEN = 1,	
	CPL_PRIORITY_ACK = 1,	
	CPL_PRIORITY_CONTROL = 1	
};

enum {
	CPL_RET_BUF_DONE = 1, 
	CPL_RET_BAD_MSG = 2,  
	CPL_RET_UNKNOWN_TID = 4	
};

typedef int (*cpl_handler_func)(struct t3cdev *dev, struct sk_buff *skb);

static inline void *cplhdr(struct sk_buff *skb)
{
	return skb->data;
}

void t3_register_cpl_handler(unsigned int opcode, cpl_handler_func h);

union listen_entry {
	struct t3c_tid_entry t3c_tid;
	union listen_entry *next;
};

union active_open_entry {
	struct t3c_tid_entry t3c_tid;
	union active_open_entry *next;
};

struct tid_info {
	struct t3c_tid_entry *tid_tab;
	unsigned int ntids;
	atomic_t tids_in_use;

	union listen_entry *stid_tab;
	unsigned int nstids;
	unsigned int stid_base;

	union active_open_entry *atid_tab;
	unsigned int natids;
	unsigned int atid_base;

	spinlock_t atid_lock ____cacheline_aligned_in_smp;
	union active_open_entry *afree;
	unsigned int atids_in_use;

	spinlock_t stid_lock ____cacheline_aligned;
	union listen_entry *sfree;
	unsigned int stids_in_use;
};

struct t3c_data {
	struct list_head list_node;
	struct t3cdev *dev;
	unsigned int tx_max_chunk;	
	unsigned int max_wrs;	
	unsigned int nmtus;
	const unsigned short *mtus;
	struct tid_info tid_maps;

	struct t3c_tid_entry *tid_release_list;
	spinlock_t tid_release_lock;
	struct work_struct tid_release_task;

	struct sk_buff *nofail_skb;
	unsigned int release_list_incomplete;
};

#define T3C_DATA(dev) (*(struct t3c_data **)&(dev)->l4opt)

#endif
