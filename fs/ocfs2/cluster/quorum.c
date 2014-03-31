/* -*- mode: c; c-basic-offset: 8; -*-
 *
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

#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>

#include "heartbeat.h"
#include "nodemanager.h"
#define MLOG_MASK_PREFIX ML_QUORUM
#include "masklog.h"
#include "quorum.h"

static struct o2quo_state {
	spinlock_t		qs_lock;
	struct work_struct	qs_work;
	int			qs_pending;
	int			qs_heartbeating;
	unsigned long		qs_hb_bm[BITS_TO_LONGS(O2NM_MAX_NODES)];
	int			qs_connected;
	unsigned long		qs_conn_bm[BITS_TO_LONGS(O2NM_MAX_NODES)];
	int			qs_holds;
	unsigned long		qs_hold_bm[BITS_TO_LONGS(O2NM_MAX_NODES)];
} o2quo_state;

static void o2quo_fence_self(void)
{
	o2hb_stop_all_regions();

	switch (o2nm_single_cluster->cl_fence_method) {
	case O2NM_FENCE_PANIC:
		panic("*** ocfs2 is very sorry to be fencing this system by "
		      "panicing ***\n");
		break;
	default:
		WARN_ON(o2nm_single_cluster->cl_fence_method >=
			O2NM_FENCE_METHODS);
	case O2NM_FENCE_RESET:
		printk(KERN_ERR "*** ocfs2 is very sorry to be fencing this "
		       "system by restarting ***\n");
		emergency_restart();
		break;
	};
}

void o2quo_disk_timeout(void)
{
	o2quo_fence_self();
}

static void o2quo_make_decision(struct work_struct *work)
{
	int quorum;
	int lowest_hb, lowest_reachable = 0, fence = 0;
	struct o2quo_state *qs = &o2quo_state;

	spin_lock(&qs->qs_lock);

	lowest_hb = find_first_bit(qs->qs_hb_bm, O2NM_MAX_NODES);
	if (lowest_hb != O2NM_MAX_NODES)
		lowest_reachable = test_bit(lowest_hb, qs->qs_conn_bm);

	mlog(0, "heartbeating: %d, connected: %d, "
	     "lowest: %d (%sreachable)\n", qs->qs_heartbeating,
	     qs->qs_connected, lowest_hb, lowest_reachable ? "" : "un");

	if (!test_bit(o2nm_this_node(), qs->qs_hb_bm) ||
	    qs->qs_heartbeating == 1)
		goto out;

	if (qs->qs_heartbeating & 1) {
		quorum = (qs->qs_heartbeating + 1)/2;
		if (qs->qs_connected < quorum) {
			mlog(ML_ERROR, "fencing this node because it is "
			     "only connected to %u nodes and %u is needed "
			     "to make a quorum out of %u heartbeating nodes\n",
			     qs->qs_connected, quorum,
			     qs->qs_heartbeating);
			fence = 1;
		}
	} else {
		quorum = qs->qs_heartbeating / 2;
		if (qs->qs_connected < quorum) {
			mlog(ML_ERROR, "fencing this node because it is "
			     "only connected to %u nodes and %u is needed "
			     "to make a quorum out of %u heartbeating nodes\n",
			     qs->qs_connected, quorum,
			     qs->qs_heartbeating);
			fence = 1;
		}
		else if ((qs->qs_connected == quorum) &&
			 !lowest_reachable) {
			mlog(ML_ERROR, "fencing this node because it is "
			     "connected to a half-quorum of %u out of %u "
			     "nodes which doesn't include the lowest active "
			     "node %u\n", quorum, qs->qs_heartbeating,
			     lowest_hb);
			fence = 1;
		}
	}

out:
	spin_unlock(&qs->qs_lock);
	if (fence)
		o2quo_fence_self();
}

static void o2quo_set_hold(struct o2quo_state *qs, u8 node)
{
	assert_spin_locked(&qs->qs_lock);

	if (!test_and_set_bit(node, qs->qs_hold_bm)) {
		qs->qs_holds++;
		mlog_bug_on_msg(qs->qs_holds == O2NM_MAX_NODES,
			        "node %u\n", node);
		mlog(0, "node %u, %d total\n", node, qs->qs_holds);
	}
}

static void o2quo_clear_hold(struct o2quo_state *qs, u8 node)
{
	assert_spin_locked(&qs->qs_lock);

	if (test_and_clear_bit(node, qs->qs_hold_bm)) {
		mlog(0, "node %u, %d total\n", node, qs->qs_holds - 1);
		if (--qs->qs_holds == 0) {
			if (qs->qs_pending) {
				qs->qs_pending = 0;
				schedule_work(&qs->qs_work);
			}
		}
		mlog_bug_on_msg(qs->qs_holds < 0, "node %u, holds %d\n",
				node, qs->qs_holds);
	}
}

void o2quo_hb_up(u8 node)
{
	struct o2quo_state *qs = &o2quo_state;

	spin_lock(&qs->qs_lock);

	qs->qs_heartbeating++;
	mlog_bug_on_msg(qs->qs_heartbeating == O2NM_MAX_NODES,
		        "node %u\n", node);
	mlog_bug_on_msg(test_bit(node, qs->qs_hb_bm), "node %u\n", node);
	set_bit(node, qs->qs_hb_bm);

	mlog(0, "node %u, %d total\n", node, qs->qs_heartbeating);

	if (!test_bit(node, qs->qs_conn_bm))
		o2quo_set_hold(qs, node);
	else
		o2quo_clear_hold(qs, node);

	spin_unlock(&qs->qs_lock);
}

void o2quo_hb_down(u8 node)
{
	struct o2quo_state *qs = &o2quo_state;

	spin_lock(&qs->qs_lock);

	qs->qs_heartbeating--;
	mlog_bug_on_msg(qs->qs_heartbeating < 0,
			"node %u, %d heartbeating\n",
			node, qs->qs_heartbeating);
	mlog_bug_on_msg(!test_bit(node, qs->qs_hb_bm), "node %u\n", node);
	clear_bit(node, qs->qs_hb_bm);

	mlog(0, "node %u, %d total\n", node, qs->qs_heartbeating);

	o2quo_clear_hold(qs, node);

	spin_unlock(&qs->qs_lock);
}

void o2quo_hb_still_up(u8 node)
{
	struct o2quo_state *qs = &o2quo_state;

	spin_lock(&qs->qs_lock);

	mlog(0, "node %u\n", node);

	qs->qs_pending = 1;
	o2quo_clear_hold(qs, node);

	spin_unlock(&qs->qs_lock);
}

void o2quo_conn_up(u8 node)
{
	struct o2quo_state *qs = &o2quo_state;

	spin_lock(&qs->qs_lock);

	qs->qs_connected++;
	mlog_bug_on_msg(qs->qs_connected == O2NM_MAX_NODES,
		        "node %u\n", node);
	mlog_bug_on_msg(test_bit(node, qs->qs_conn_bm), "node %u\n", node);
	set_bit(node, qs->qs_conn_bm);

	mlog(0, "node %u, %d total\n", node, qs->qs_connected);

	if (!test_bit(node, qs->qs_hb_bm))
		o2quo_set_hold(qs, node);
	else
		o2quo_clear_hold(qs, node);

	spin_unlock(&qs->qs_lock);
}

void o2quo_conn_err(u8 node)
{
	struct o2quo_state *qs = &o2quo_state;

	spin_lock(&qs->qs_lock);

	if (test_bit(node, qs->qs_conn_bm)) {
		qs->qs_connected--;
		mlog_bug_on_msg(qs->qs_connected < 0,
				"node %u, connected %d\n",
				node, qs->qs_connected);

		clear_bit(node, qs->qs_conn_bm);
	}

	mlog(0, "node %u, %d total\n", node, qs->qs_connected);

	if (test_bit(node, qs->qs_hb_bm))
		o2quo_set_hold(qs, node);

	spin_unlock(&qs->qs_lock);
}

void o2quo_init(void)
{
	struct o2quo_state *qs = &o2quo_state;

	spin_lock_init(&qs->qs_lock);
	INIT_WORK(&qs->qs_work, o2quo_make_decision);
}

void o2quo_exit(void)
{
	struct o2quo_state *qs = &o2quo_state;

	flush_work_sync(&qs->qs_work);
}
