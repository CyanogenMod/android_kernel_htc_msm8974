/*
 * Copyright (c) 2003-2008 Chelsio, Inc. All rights reserved.
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
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/jhash.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <net/neighbour.h>
#include "common.h"
#include "t3cdev.h"
#include "cxgb3_defs.h"
#include "l2t.h"
#include "t3_cpl.h"
#include "firmware_exports.h"

#define VLAN_NONE 0xfff


static inline unsigned int vlan_prio(const struct l2t_entry *e)
{
	return e->vlan >> 13;
}

static inline unsigned int arp_hash(u32 key, int ifindex,
				    const struct l2t_data *d)
{
	return jhash_2words(key, ifindex, 0) & (d->nentries - 1);
}

static inline void neigh_replace(struct l2t_entry *e, struct neighbour *n)
{
	neigh_hold(n);
	if (e->neigh)
		neigh_release(e->neigh);
	e->neigh = n;
}

static int setup_l2e_send_pending(struct t3cdev *dev, struct sk_buff *skb,
				  struct l2t_entry *e)
{
	struct cpl_l2t_write_req *req;
	struct sk_buff *tmp;

	if (!skb) {
		skb = alloc_skb(sizeof(*req), GFP_ATOMIC);
		if (!skb)
			return -ENOMEM;
	}

	req = (struct cpl_l2t_write_req *)__skb_put(skb, sizeof(*req));
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_L2T_WRITE_REQ, e->idx));
	req->params = htonl(V_L2T_W_IDX(e->idx) | V_L2T_W_IFF(e->smt_idx) |
			    V_L2T_W_VLAN(e->vlan & VLAN_VID_MASK) |
			    V_L2T_W_PRIO(vlan_prio(e)));
	memcpy(e->dmac, e->neigh->ha, sizeof(e->dmac));
	memcpy(req->dst_mac, e->dmac, sizeof(req->dst_mac));
	skb->priority = CPL_PRIORITY_CONTROL;
	cxgb3_ofld_send(dev, skb);

	skb_queue_walk_safe(&e->arpq, skb, tmp) {
		__skb_unlink(skb, &e->arpq);
		cxgb3_ofld_send(dev, skb);
	}
	e->state = L2T_STATE_VALID;

	return 0;
}

static inline void arpq_enqueue(struct l2t_entry *e, struct sk_buff *skb)
{
	__skb_queue_tail(&e->arpq, skb);
}

int t3_l2t_send_slow(struct t3cdev *dev, struct sk_buff *skb,
		     struct l2t_entry *e)
{
again:
	switch (e->state) {
	case L2T_STATE_STALE:	
		neigh_event_send(e->neigh, NULL);
		spin_lock_bh(&e->lock);
		if (e->state == L2T_STATE_STALE)
			e->state = L2T_STATE_VALID;
		spin_unlock_bh(&e->lock);
	case L2T_STATE_VALID:	
		return cxgb3_ofld_send(dev, skb);
	case L2T_STATE_RESOLVING:
		spin_lock_bh(&e->lock);
		if (e->state != L2T_STATE_RESOLVING) {
			
			spin_unlock_bh(&e->lock);
			goto again;
		}
		arpq_enqueue(e, skb);
		spin_unlock_bh(&e->lock);

		if (!neigh_event_send(e->neigh, NULL)) {
			skb = alloc_skb(sizeof(struct cpl_l2t_write_req),
					GFP_ATOMIC);
			if (!skb)
				break;

			spin_lock_bh(&e->lock);
			if (!skb_queue_empty(&e->arpq))
				setup_l2e_send_pending(dev, skb, e);
			else	
				__kfree_skb(skb);
			spin_unlock_bh(&e->lock);
		}
	}
	return 0;
}

EXPORT_SYMBOL(t3_l2t_send_slow);

void t3_l2t_send_event(struct t3cdev *dev, struct l2t_entry *e)
{
again:
	switch (e->state) {
	case L2T_STATE_STALE:	
		neigh_event_send(e->neigh, NULL);
		spin_lock_bh(&e->lock);
		if (e->state == L2T_STATE_STALE) {
			e->state = L2T_STATE_VALID;
		}
		spin_unlock_bh(&e->lock);
		return;
	case L2T_STATE_VALID:	
		return;
	case L2T_STATE_RESOLVING:
		spin_lock_bh(&e->lock);
		if (e->state != L2T_STATE_RESOLVING) {
			
			spin_unlock_bh(&e->lock);
			goto again;
		}
		spin_unlock_bh(&e->lock);

		neigh_event_send(e->neigh, NULL);
	}
}

EXPORT_SYMBOL(t3_l2t_send_event);

static struct l2t_entry *alloc_l2e(struct l2t_data *d)
{
	struct l2t_entry *end, *e, **p;

	if (!atomic_read(&d->nfree))
		return NULL;

	
	for (e = d->rover, end = &d->l2tab[d->nentries]; e != end; ++e)
		if (atomic_read(&e->refcnt) == 0)
			goto found;

	for (e = &d->l2tab[1]; atomic_read(&e->refcnt); ++e) ;
found:
	d->rover = e + 1;
	atomic_dec(&d->nfree);

	if (e->state != L2T_STATE_UNUSED) {
		int hash = arp_hash(e->addr, e->ifindex, d);

		for (p = &d->l2tab[hash].first; *p; p = &(*p)->next)
			if (*p == e) {
				*p = e->next;
				break;
			}
		e->state = L2T_STATE_UNUSED;
	}
	return e;
}

void t3_l2e_free(struct l2t_data *d, struct l2t_entry *e)
{
	spin_lock_bh(&e->lock);
	if (atomic_read(&e->refcnt) == 0) {	
		if (e->neigh) {
			neigh_release(e->neigh);
			e->neigh = NULL;
		}
	}
	spin_unlock_bh(&e->lock);
	atomic_inc(&d->nfree);
}

EXPORT_SYMBOL(t3_l2e_free);

static inline void reuse_entry(struct l2t_entry *e, struct neighbour *neigh)
{
	unsigned int nud_state;

	spin_lock(&e->lock);	

	if (neigh != e->neigh)
		neigh_replace(e, neigh);
	nud_state = neigh->nud_state;
	if (memcmp(e->dmac, neigh->ha, sizeof(e->dmac)) ||
	    !(nud_state & NUD_VALID))
		e->state = L2T_STATE_RESOLVING;
	else if (nud_state & NUD_CONNECTED)
		e->state = L2T_STATE_VALID;
	else
		e->state = L2T_STATE_STALE;
	spin_unlock(&e->lock);
}

struct l2t_entry *t3_l2t_get(struct t3cdev *cdev, struct dst_entry *dst,
			     struct net_device *dev)
{
	struct l2t_entry *e = NULL;
	struct neighbour *neigh;
	struct port_info *p;
	struct l2t_data *d;
	int hash;
	u32 addr;
	int ifidx;
	int smt_idx;

	rcu_read_lock();
	neigh = dst_get_neighbour_noref(dst);
	if (!neigh)
		goto done_rcu;

	addr = *(u32 *) neigh->primary_key;
	ifidx = neigh->dev->ifindex;

	if (!dev)
		dev = neigh->dev;
	p = netdev_priv(dev);
	smt_idx = p->port_id;

	d = L2DATA(cdev);
	if (!d)
		goto done_rcu;

	hash = arp_hash(addr, ifidx, d);

	write_lock_bh(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next)
		if (e->addr == addr && e->ifindex == ifidx &&
		    e->smt_idx == smt_idx) {
			l2t_hold(d, e);
			if (atomic_read(&e->refcnt) == 1)
				reuse_entry(e, neigh);
			goto done_unlock;
		}

	
	e = alloc_l2e(d);
	if (e) {
		spin_lock(&e->lock);	
		e->next = d->l2tab[hash].first;
		d->l2tab[hash].first = e;
		e->state = L2T_STATE_RESOLVING;
		e->addr = addr;
		e->ifindex = ifidx;
		e->smt_idx = smt_idx;
		atomic_set(&e->refcnt, 1);
		neigh_replace(e, neigh);
		if (neigh->dev->priv_flags & IFF_802_1Q_VLAN)
			e->vlan = vlan_dev_vlan_id(neigh->dev);
		else
			e->vlan = VLAN_NONE;
		spin_unlock(&e->lock);
	}
done_unlock:
	write_unlock_bh(&d->lock);
done_rcu:
	rcu_read_unlock();
	return e;
}

EXPORT_SYMBOL(t3_l2t_get);

static void handle_failed_resolution(struct t3cdev *dev, struct sk_buff_head *arpq)
{
	struct sk_buff *skb, *tmp;

	skb_queue_walk_safe(arpq, skb, tmp) {
		struct l2t_skb_cb *cb = L2T_SKB_CB(skb);

		__skb_unlink(skb, arpq);
		if (cb->arp_failure_handler)
			cb->arp_failure_handler(dev, skb);
		else
			cxgb3_ofld_send(dev, skb);
	}
}

void t3_l2t_update(struct t3cdev *dev, struct neighbour *neigh)
{
	struct sk_buff_head arpq;
	struct l2t_entry *e;
	struct l2t_data *d = L2DATA(dev);
	u32 addr = *(u32 *) neigh->primary_key;
	int ifidx = neigh->dev->ifindex;
	int hash = arp_hash(addr, ifidx, d);

	read_lock_bh(&d->lock);
	for (e = d->l2tab[hash].first; e; e = e->next)
		if (e->addr == addr && e->ifindex == ifidx) {
			spin_lock(&e->lock);
			goto found;
		}
	read_unlock_bh(&d->lock);
	return;

found:
	__skb_queue_head_init(&arpq);

	read_unlock(&d->lock);
	if (atomic_read(&e->refcnt)) {
		if (neigh != e->neigh)
			neigh_replace(e, neigh);

		if (e->state == L2T_STATE_RESOLVING) {
			if (neigh->nud_state & NUD_FAILED) {
				skb_queue_splice_init(&e->arpq, &arpq);
			} else if (neigh->nud_state & (NUD_CONNECTED|NUD_STALE))
				setup_l2e_send_pending(dev, NULL, e);
		} else {
			e->state = neigh->nud_state & NUD_CONNECTED ?
			    L2T_STATE_VALID : L2T_STATE_STALE;
			if (memcmp(e->dmac, neigh->ha, 6))
				setup_l2e_send_pending(dev, NULL, e);
		}
	}
	spin_unlock_bh(&e->lock);

	if (!skb_queue_empty(&arpq))
		handle_failed_resolution(dev, &arpq);
}

struct l2t_data *t3_init_l2t(unsigned int l2t_capacity)
{
	struct l2t_data *d;
	int i, size = sizeof(*d) + l2t_capacity * sizeof(struct l2t_entry);

	d = cxgb_alloc_mem(size);
	if (!d)
		return NULL;

	d->nentries = l2t_capacity;
	d->rover = &d->l2tab[1];	
	atomic_set(&d->nfree, l2t_capacity - 1);
	rwlock_init(&d->lock);

	for (i = 0; i < l2t_capacity; ++i) {
		d->l2tab[i].idx = i;
		d->l2tab[i].state = L2T_STATE_UNUSED;
		__skb_queue_head_init(&d->l2tab[i].arpq);
		spin_lock_init(&d->l2tab[i].lock);
		atomic_set(&d->l2tab[i].refcnt, 0);
	}
	return d;
}

void t3_free_l2t(struct l2t_data *d)
{
	cxgb_free_mem(d);
}

