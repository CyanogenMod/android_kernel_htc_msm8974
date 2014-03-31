/*
 * UWB reservation management.
 *
 * Copyright (C) 2008 Cambridge Silicon Radio Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/uwb.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/export.h>

#include "uwb-internal.h"

static void uwb_rsv_timer(unsigned long arg);

static const char *rsv_states[] = {
	[UWB_RSV_STATE_NONE]                 = "none            ",
	[UWB_RSV_STATE_O_INITIATED]          = "o initiated     ",
	[UWB_RSV_STATE_O_PENDING]            = "o pending       ",
	[UWB_RSV_STATE_O_MODIFIED]           = "o modified      ",
	[UWB_RSV_STATE_O_ESTABLISHED]        = "o established   ",
	[UWB_RSV_STATE_O_TO_BE_MOVED]        = "o to be moved   ",
	[UWB_RSV_STATE_O_MOVE_EXPANDING]     = "o move expanding",
	[UWB_RSV_STATE_O_MOVE_COMBINING]     = "o move combining",
	[UWB_RSV_STATE_O_MOVE_REDUCING]      = "o move reducing ",
	[UWB_RSV_STATE_T_ACCEPTED]           = "t accepted      ",
	[UWB_RSV_STATE_T_CONFLICT]           = "t conflict      ",
	[UWB_RSV_STATE_T_PENDING]            = "t pending       ",
	[UWB_RSV_STATE_T_DENIED]             = "t denied        ",
	[UWB_RSV_STATE_T_RESIZED]            = "t resized       ",
	[UWB_RSV_STATE_T_EXPANDING_ACCEPTED] = "t expanding acc ",
	[UWB_RSV_STATE_T_EXPANDING_CONFLICT] = "t expanding conf",
	[UWB_RSV_STATE_T_EXPANDING_PENDING]  = "t expanding pend",
	[UWB_RSV_STATE_T_EXPANDING_DENIED]   = "t expanding den ",
};

static const char *rsv_types[] = {
	[UWB_DRP_TYPE_ALIEN_BP] = "alien-bp",
	[UWB_DRP_TYPE_HARD]     = "hard",
	[UWB_DRP_TYPE_SOFT]     = "soft",
	[UWB_DRP_TYPE_PRIVATE]  = "private",
	[UWB_DRP_TYPE_PCA]      = "pca",
};

bool uwb_rsv_has_two_drp_ies(struct uwb_rsv *rsv)
{
	static const bool has_two_drp_ies[] = {
		[UWB_RSV_STATE_O_INITIATED]               = false,
		[UWB_RSV_STATE_O_PENDING]                 = false,
		[UWB_RSV_STATE_O_MODIFIED]                = false,
		[UWB_RSV_STATE_O_ESTABLISHED]             = false,
		[UWB_RSV_STATE_O_TO_BE_MOVED]             = false,
		[UWB_RSV_STATE_O_MOVE_COMBINING]          = false,
		[UWB_RSV_STATE_O_MOVE_REDUCING]           = false,
		[UWB_RSV_STATE_O_MOVE_EXPANDING]          = true,
		[UWB_RSV_STATE_T_ACCEPTED]                = false,
		[UWB_RSV_STATE_T_CONFLICT]                = false,
		[UWB_RSV_STATE_T_PENDING]                 = false,
		[UWB_RSV_STATE_T_DENIED]                  = false,
		[UWB_RSV_STATE_T_RESIZED]                 = false,
		[UWB_RSV_STATE_T_EXPANDING_ACCEPTED]      = true,
		[UWB_RSV_STATE_T_EXPANDING_CONFLICT]      = true,
		[UWB_RSV_STATE_T_EXPANDING_PENDING]       = true,
		[UWB_RSV_STATE_T_EXPANDING_DENIED]        = true,
	};

	return has_two_drp_ies[rsv->state];
}

const char *uwb_rsv_state_str(enum uwb_rsv_state state)
{
	if (state < UWB_RSV_STATE_NONE || state >= UWB_RSV_STATE_LAST)
		return "unknown";
	return rsv_states[state];
}
EXPORT_SYMBOL_GPL(uwb_rsv_state_str);

const char *uwb_rsv_type_str(enum uwb_drp_type type)
{
	if (type < UWB_DRP_TYPE_ALIEN_BP || type > UWB_DRP_TYPE_PCA)
		return "invalid";
	return rsv_types[type];
}
EXPORT_SYMBOL_GPL(uwb_rsv_type_str);

void uwb_rsv_dump(char *text, struct uwb_rsv *rsv)
{
	struct device *dev = &rsv->rc->uwb_dev.dev;
	struct uwb_dev_addr devaddr;
	char owner[UWB_ADDR_STRSIZE], target[UWB_ADDR_STRSIZE];

	uwb_dev_addr_print(owner, sizeof(owner), &rsv->owner->dev_addr);
	if (rsv->target.type == UWB_RSV_TARGET_DEV)
		devaddr = rsv->target.dev->dev_addr;
	else
		devaddr = rsv->target.devaddr;
	uwb_dev_addr_print(target, sizeof(target), &devaddr);

	dev_dbg(dev, "rsv %s %s -> %s: %s\n",
		text, owner, target, uwb_rsv_state_str(rsv->state));
}

static void uwb_rsv_release(struct kref *kref)
{
	struct uwb_rsv *rsv = container_of(kref, struct uwb_rsv, kref);

	kfree(rsv);
}

void uwb_rsv_get(struct uwb_rsv *rsv)
{
	kref_get(&rsv->kref);
}

void uwb_rsv_put(struct uwb_rsv *rsv)
{
	kref_put(&rsv->kref, uwb_rsv_release);
}

static int uwb_rsv_get_stream(struct uwb_rsv *rsv)
{
	struct uwb_rc *rc = rsv->rc;
	struct device *dev = &rc->uwb_dev.dev;
	unsigned long *streams_bm;
	int stream;

	switch (rsv->target.type) {
	case UWB_RSV_TARGET_DEV:
		streams_bm = rsv->target.dev->streams;
		break;
	case UWB_RSV_TARGET_DEVADDR:
		streams_bm = rc->uwb_dev.streams;
		break;
	default:
		return -EINVAL;
	}

	stream = find_first_zero_bit(streams_bm, UWB_NUM_STREAMS);
	if (stream >= UWB_NUM_STREAMS)
		return -EBUSY;

	rsv->stream = stream;
	set_bit(stream, streams_bm);

	dev_dbg(dev, "get stream %d\n", rsv->stream);

	return 0;
}

static void uwb_rsv_put_stream(struct uwb_rsv *rsv)
{
	struct uwb_rc *rc = rsv->rc;
	struct device *dev = &rc->uwb_dev.dev;
	unsigned long *streams_bm;

	switch (rsv->target.type) {
	case UWB_RSV_TARGET_DEV:
		streams_bm = rsv->target.dev->streams;
		break;
	case UWB_RSV_TARGET_DEVADDR:
		streams_bm = rc->uwb_dev.streams;
		break;
	default:
		return;
	}

	clear_bit(rsv->stream, streams_bm);

	dev_dbg(dev, "put stream %d\n", rsv->stream);
}

void uwb_rsv_backoff_win_timer(unsigned long arg)
{
	struct uwb_drp_backoff_win *bow = (struct uwb_drp_backoff_win *)arg;
	struct uwb_rc *rc = container_of(bow, struct uwb_rc, bow);
	struct device *dev = &rc->uwb_dev.dev;

	bow->can_reserve_extra_mases = true;
	if (bow->total_expired <= 4) {
		bow->total_expired++;
	} else {
		bow->total_expired = 0;
		bow->window = UWB_DRP_BACKOFF_WIN_MIN >> 1;
	}
	dev_dbg(dev, "backoff_win_timer total_expired=%d, n=%d\n: ", bow->total_expired, bow->n);

	
	uwb_rsv_handle_drp_avail_change(rc);
}

void uwb_rsv_backoff_win_increment(struct uwb_rc *rc)
{
	struct uwb_drp_backoff_win *bow = &rc->bow;
	struct device *dev = &rc->uwb_dev.dev;
	unsigned timeout_us;

	dev_dbg(dev, "backoff_win_increment: window=%d\n", bow->window);

	bow->can_reserve_extra_mases = false;

	if((bow->window << 1) == UWB_DRP_BACKOFF_WIN_MAX)
		return;

	bow->window <<= 1;
	bow->n = random32() & (bow->window - 1);
	dev_dbg(dev, "new_window=%d, n=%d\n: ", bow->window, bow->n);

	
	timeout_us = bow->n * UWB_SUPERFRAME_LENGTH_US;
	bow->total_expired = 0;
	mod_timer(&bow->timer, jiffies + usecs_to_jiffies(timeout_us));		
}

static void uwb_rsv_stroke_timer(struct uwb_rsv *rsv)
{
	int sframes = UWB_MAX_LOST_BEACONS;

	if (rsv->is_multicast) {
		if (rsv->state == UWB_RSV_STATE_O_INITIATED
		    || rsv->state == UWB_RSV_STATE_O_MOVE_EXPANDING
		    || rsv->state == UWB_RSV_STATE_O_MOVE_COMBINING
		    || rsv->state == UWB_RSV_STATE_O_MOVE_REDUCING)
			sframes = 1;
		if (rsv->state == UWB_RSV_STATE_O_ESTABLISHED)
			sframes = 0;
		
	}

	if (sframes > 0) {
		unsigned timeout_us = (sframes + 2) * UWB_SUPERFRAME_LENGTH_US;
		mod_timer(&rsv->timer, jiffies + usecs_to_jiffies(timeout_us));
	} else
		del_timer(&rsv->timer);
}

static void uwb_rsv_state_update(struct uwb_rsv *rsv,
				 enum uwb_rsv_state new_state)
{
	rsv->state = new_state;
	rsv->ie_valid = false;

	uwb_rsv_dump("SU", rsv);

	uwb_rsv_stroke_timer(rsv);
	uwb_rsv_sched_update(rsv->rc);
}

static void uwb_rsv_callback(struct uwb_rsv *rsv)
{
	if (rsv->callback)
		rsv->callback(rsv);
}

void uwb_rsv_set_state(struct uwb_rsv *rsv, enum uwb_rsv_state new_state)
{
	struct uwb_rsv_move *mv = &rsv->mv;

	if (rsv->state == new_state) {
		switch (rsv->state) {
		case UWB_RSV_STATE_O_ESTABLISHED:
		case UWB_RSV_STATE_O_MOVE_EXPANDING:
		case UWB_RSV_STATE_O_MOVE_COMBINING:
		case UWB_RSV_STATE_O_MOVE_REDUCING:
		case UWB_RSV_STATE_T_ACCEPTED:
		case UWB_RSV_STATE_T_EXPANDING_ACCEPTED:
		case UWB_RSV_STATE_T_RESIZED:
		case UWB_RSV_STATE_NONE:
			uwb_rsv_stroke_timer(rsv);
			break;
		default:
			break;
		}
		return;
	}

	uwb_rsv_dump("SC", rsv);

	switch (new_state) {
	case UWB_RSV_STATE_NONE:
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_NONE);
		uwb_rsv_callback(rsv);
		break;
	case UWB_RSV_STATE_O_INITIATED:
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_INITIATED);
		break;
	case UWB_RSV_STATE_O_PENDING:
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_PENDING);
		break;
	case UWB_RSV_STATE_O_MODIFIED:
		
		bitmap_andnot(rsv->mas.bm, rsv->mas.bm, mv->companion_mas.bm, UWB_NUM_MAS);
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_MODIFIED);
		break;
	case UWB_RSV_STATE_O_ESTABLISHED:
		if (rsv->state == UWB_RSV_STATE_O_MODIFIED
		    || rsv->state == UWB_RSV_STATE_O_MOVE_REDUCING) {
			uwb_drp_avail_release(rsv->rc, &mv->companion_mas);
			rsv->needs_release_companion_mas = false;
		}
		uwb_drp_avail_reserve(rsv->rc, &rsv->mas);
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_ESTABLISHED);
		uwb_rsv_callback(rsv);
		break;
	case UWB_RSV_STATE_O_MOVE_EXPANDING:
		rsv->needs_release_companion_mas = true;
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_MOVE_EXPANDING);
		break;
	case UWB_RSV_STATE_O_MOVE_COMBINING:
		rsv->needs_release_companion_mas = false;
		uwb_drp_avail_reserve(rsv->rc, &mv->companion_mas);
		bitmap_or(rsv->mas.bm, rsv->mas.bm, mv->companion_mas.bm, UWB_NUM_MAS);
		rsv->mas.safe   += mv->companion_mas.safe;
		rsv->mas.unsafe += mv->companion_mas.unsafe;
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_MOVE_COMBINING);
		break;
	case UWB_RSV_STATE_O_MOVE_REDUCING:
		bitmap_andnot(mv->companion_mas.bm, rsv->mas.bm, mv->final_mas.bm, UWB_NUM_MAS);
		rsv->needs_release_companion_mas = true;
		rsv->mas.safe   = mv->final_mas.safe;
		rsv->mas.unsafe = mv->final_mas.unsafe;
		bitmap_copy(rsv->mas.bm, mv->final_mas.bm, UWB_NUM_MAS);
		bitmap_copy(rsv->mas.unsafe_bm, mv->final_mas.unsafe_bm, UWB_NUM_MAS);
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_O_MOVE_REDUCING);
		break;
	case UWB_RSV_STATE_T_ACCEPTED:
	case UWB_RSV_STATE_T_RESIZED:
		rsv->needs_release_companion_mas = false;
		uwb_drp_avail_reserve(rsv->rc, &rsv->mas);
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_T_ACCEPTED);
		uwb_rsv_callback(rsv);
		break;
	case UWB_RSV_STATE_T_DENIED:
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_T_DENIED);
		break;
	case UWB_RSV_STATE_T_CONFLICT:
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_T_CONFLICT);
		break;
	case UWB_RSV_STATE_T_PENDING:
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_T_PENDING);
		break;
	case UWB_RSV_STATE_T_EXPANDING_ACCEPTED:
		rsv->needs_release_companion_mas = true;
		uwb_drp_avail_reserve(rsv->rc, &mv->companion_mas);
		uwb_rsv_state_update(rsv, UWB_RSV_STATE_T_EXPANDING_ACCEPTED);
		break;
	default:
		dev_err(&rsv->rc->uwb_dev.dev, "unhandled state: %s (%d)\n",
			uwb_rsv_state_str(new_state), new_state);
	}
}

static void uwb_rsv_handle_timeout_work(struct work_struct *work)
{
	struct uwb_rsv *rsv = container_of(work, struct uwb_rsv,
					   handle_timeout_work);
	struct uwb_rc *rc = rsv->rc;

	mutex_lock(&rc->rsvs_mutex);

	uwb_rsv_dump("TO", rsv);

	switch (rsv->state) {
	case UWB_RSV_STATE_O_INITIATED:
		if (rsv->is_multicast) {
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_ESTABLISHED);
			goto unlock;
		}
		break;
	case UWB_RSV_STATE_O_MOVE_EXPANDING:
		if (rsv->is_multicast) {
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_COMBINING);
			goto unlock;
		}
		break;
	case UWB_RSV_STATE_O_MOVE_COMBINING:
		if (rsv->is_multicast) {
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_REDUCING);
			goto unlock;
		}
		break;
	case UWB_RSV_STATE_O_MOVE_REDUCING:
		if (rsv->is_multicast) {
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_ESTABLISHED);
			goto unlock;
		}
		break;
	case UWB_RSV_STATE_O_ESTABLISHED:
		if (rsv->is_multicast)
			goto unlock;
		break;
	case UWB_RSV_STATE_T_EXPANDING_ACCEPTED:
		uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_ACCEPTED);
		uwb_drp_avail_release(rsv->rc, &rsv->mv.companion_mas);
		goto unlock;
	default:
		break;
	}

	uwb_rsv_remove(rsv);

unlock:
	mutex_unlock(&rc->rsvs_mutex);
}

static struct uwb_rsv *uwb_rsv_alloc(struct uwb_rc *rc)
{
	struct uwb_rsv *rsv;

	rsv = kzalloc(sizeof(struct uwb_rsv), GFP_KERNEL);
	if (!rsv)
		return NULL;

	INIT_LIST_HEAD(&rsv->rc_node);
	INIT_LIST_HEAD(&rsv->pal_node);
	kref_init(&rsv->kref);
	init_timer(&rsv->timer);
	rsv->timer.function = uwb_rsv_timer;
	rsv->timer.data     = (unsigned long)rsv;

	rsv->rc = rc;
	INIT_WORK(&rsv->handle_timeout_work, uwb_rsv_handle_timeout_work);

	return rsv;
}

struct uwb_rsv *uwb_rsv_create(struct uwb_rc *rc, uwb_rsv_cb_f cb, void *pal_priv)
{
	struct uwb_rsv *rsv;

	rsv = uwb_rsv_alloc(rc);
	if (!rsv)
		return NULL;

	rsv->callback = cb;
	rsv->pal_priv = pal_priv;

	return rsv;
}
EXPORT_SYMBOL_GPL(uwb_rsv_create);

void uwb_rsv_remove(struct uwb_rsv *rsv)
{
	uwb_rsv_dump("RM", rsv);

	if (rsv->state != UWB_RSV_STATE_NONE)
		uwb_rsv_set_state(rsv, UWB_RSV_STATE_NONE);

	if (rsv->needs_release_companion_mas)
		uwb_drp_avail_release(rsv->rc, &rsv->mv.companion_mas);
	uwb_drp_avail_release(rsv->rc, &rsv->mas);

	if (uwb_rsv_is_owner(rsv))
		uwb_rsv_put_stream(rsv);

	uwb_dev_put(rsv->owner);
	if (rsv->target.type == UWB_RSV_TARGET_DEV)
		uwb_dev_put(rsv->target.dev);

	list_del_init(&rsv->rc_node);
	uwb_rsv_put(rsv);
}

void uwb_rsv_destroy(struct uwb_rsv *rsv)
{
	uwb_rsv_put(rsv);
}
EXPORT_SYMBOL_GPL(uwb_rsv_destroy);

int uwb_rsv_establish(struct uwb_rsv *rsv)
{
	struct uwb_rc *rc = rsv->rc;
	struct uwb_mas_bm available;
	int ret;

	mutex_lock(&rc->rsvs_mutex);
	ret = uwb_rsv_get_stream(rsv);
	if (ret)
		goto out;

	rsv->tiebreaker = random32() & 1;
	
	uwb_drp_available(rc, &available);

	ret = uwb_rsv_find_best_allocation(rsv, &available, &rsv->mas);
	if (ret == UWB_RSV_ALLOC_NOT_FOUND) {
		ret = -EBUSY;
		uwb_rsv_put_stream(rsv);
		goto out;
	}

	ret = uwb_drp_avail_reserve_pending(rc, &rsv->mas);
	if (ret != 0) {
		uwb_rsv_put_stream(rsv);
		goto out;
	}

	uwb_rsv_get(rsv);
	list_add_tail(&rsv->rc_node, &rc->reservations);
	rsv->owner = &rc->uwb_dev;
	uwb_dev_get(rsv->owner);
	uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_INITIATED);
out:
	mutex_unlock(&rc->rsvs_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(uwb_rsv_establish);

int uwb_rsv_modify(struct uwb_rsv *rsv, int max_mas, int min_mas, int max_interval)
{
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(uwb_rsv_modify);

int uwb_rsv_try_move(struct uwb_rsv *rsv, struct uwb_mas_bm *available)
{
	struct uwb_rc *rc = rsv->rc;
	struct uwb_drp_backoff_win *bow = &rc->bow;
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_rsv_move *mv;
	int ret = 0;
 
	if (bow->can_reserve_extra_mases == false)
		return -EBUSY;

	mv = &rsv->mv;

	if (uwb_rsv_find_best_allocation(rsv, available, &mv->final_mas) == UWB_RSV_ALLOC_FOUND) {

		if (!bitmap_equal(rsv->mas.bm, mv->final_mas.bm, UWB_NUM_MAS)) {
			
			bitmap_andnot(mv->companion_mas.bm, mv->final_mas.bm, rsv->mas.bm, UWB_NUM_MAS);
			uwb_drp_avail_reserve_pending(rc, &mv->companion_mas);
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_EXPANDING);
		}
	} else {
		dev_dbg(dev, "new allocation not found\n");
	}
	
	return ret;
}

void uwb_rsv_handle_drp_avail_change(struct uwb_rc *rc)
{
	struct uwb_drp_backoff_win *bow = &rc->bow;
	struct uwb_rsv *rsv;
	struct uwb_mas_bm mas;
	
	if (bow->can_reserve_extra_mases == false)
		return;

	list_for_each_entry(rsv, &rc->reservations, rc_node) {
		if (rsv->state == UWB_RSV_STATE_O_ESTABLISHED ||
		    rsv->state == UWB_RSV_STATE_O_TO_BE_MOVED) {
			uwb_drp_available(rc, &mas);
			bitmap_or(mas.bm, mas.bm, rsv->mas.bm, UWB_NUM_MAS);
			uwb_rsv_try_move(rsv, &mas);
		}
	}
	
}

void uwb_rsv_terminate(struct uwb_rsv *rsv)
{
	struct uwb_rc *rc = rsv->rc;

	mutex_lock(&rc->rsvs_mutex);

	if (rsv->state != UWB_RSV_STATE_NONE)
		uwb_rsv_set_state(rsv, UWB_RSV_STATE_NONE);

	mutex_unlock(&rc->rsvs_mutex);
}
EXPORT_SYMBOL_GPL(uwb_rsv_terminate);

void uwb_rsv_accept(struct uwb_rsv *rsv, uwb_rsv_cb_f cb, void *pal_priv)
{
	uwb_rsv_get(rsv);

	rsv->callback = cb;
	rsv->pal_priv = pal_priv;
	rsv->state    = UWB_RSV_STATE_T_ACCEPTED;
}
EXPORT_SYMBOL_GPL(uwb_rsv_accept);

static bool uwb_rsv_match(struct uwb_rsv *rsv, struct uwb_dev *src,
			  struct uwb_ie_drp *drp_ie)
{
	struct uwb_dev_addr *rsv_src;
	int stream;

	stream = uwb_ie_drp_stream_index(drp_ie);

	if (rsv->stream != stream)
		return false;

	switch (rsv->target.type) {
	case UWB_RSV_TARGET_DEVADDR:
		return rsv->stream == stream;
	case UWB_RSV_TARGET_DEV:
		if (uwb_ie_drp_owner(drp_ie))
			rsv_src = &rsv->owner->dev_addr;
		else
			rsv_src = &rsv->target.dev->dev_addr;
		return uwb_dev_addr_cmp(&src->dev_addr, rsv_src) == 0;
	}
	return false;
}

static struct uwb_rsv *uwb_rsv_new_target(struct uwb_rc *rc,
					  struct uwb_dev *src,
					  struct uwb_ie_drp *drp_ie)
{
	struct uwb_rsv *rsv;
	struct uwb_pal *pal;
	enum uwb_rsv_state state;

	rsv = uwb_rsv_alloc(rc);
	if (!rsv)
		return NULL;

	rsv->rc          = rc;
	rsv->owner       = src;
	uwb_dev_get(rsv->owner);
	rsv->target.type = UWB_RSV_TARGET_DEV;
	rsv->target.dev  = &rc->uwb_dev;
	uwb_dev_get(&rc->uwb_dev);
	rsv->type        = uwb_ie_drp_type(drp_ie);
	rsv->stream      = uwb_ie_drp_stream_index(drp_ie);
	uwb_drp_ie_to_bm(&rsv->mas, drp_ie);

	rsv->state = UWB_RSV_STATE_T_DENIED;
	mutex_lock(&rc->uwb_dev.mutex);
	list_for_each_entry(pal, &rc->pals, node) {
		if (pal->new_rsv)
			pal->new_rsv(pal, rsv);
		if (rsv->state == UWB_RSV_STATE_T_ACCEPTED)
			break;
	}
	mutex_unlock(&rc->uwb_dev.mutex);

	list_add_tail(&rsv->rc_node, &rc->reservations);
	state = rsv->state;
	rsv->state = UWB_RSV_STATE_NONE;

	
	if (state == UWB_RSV_STATE_T_ACCEPTED
	    && uwb_drp_avail_reserve_pending(rc, &rsv->mas) == -EBUSY) {
		
	} else {
		uwb_rsv_set_state(rsv, state);
	}

	return rsv;
}

void uwb_rsv_get_usable_mas(struct uwb_rsv *rsv, struct uwb_mas_bm *mas)
{
	bitmap_zero(mas->bm, UWB_NUM_MAS);
	bitmap_andnot(mas->bm, rsv->mas.bm, rsv->rc->cnflt_alien_bitmap.bm, UWB_NUM_MAS);
}
EXPORT_SYMBOL_GPL(uwb_rsv_get_usable_mas);

struct uwb_rsv *uwb_rsv_find(struct uwb_rc *rc, struct uwb_dev *src,
			     struct uwb_ie_drp *drp_ie)
{
	struct uwb_rsv *rsv;

	list_for_each_entry(rsv, &rc->reservations, rc_node) {
		if (uwb_rsv_match(rsv, src, drp_ie))
			return rsv;
	}

	if (uwb_ie_drp_owner(drp_ie))
		return uwb_rsv_new_target(rc, src, drp_ie);

	return NULL;
}

static bool uwb_rsv_update_all(struct uwb_rc *rc)
{
	struct uwb_rsv *rsv, *t;
	bool ie_updated = false;

	list_for_each_entry_safe(rsv, t, &rc->reservations, rc_node) {
		if (!rsv->ie_valid) {
			uwb_drp_ie_update(rsv);
			ie_updated = true;
		}
	}

	return ie_updated;
}

void uwb_rsv_queue_update(struct uwb_rc *rc)
{
	unsigned long delay_us = UWB_MAS_LENGTH_US * UWB_MAS_PER_ZONE;

	queue_delayed_work(rc->rsv_workq, &rc->rsv_update_work, usecs_to_jiffies(delay_us));
}

void uwb_rsv_sched_update(struct uwb_rc *rc)
{
	spin_lock_bh(&rc->rsvs_lock);
	if (!delayed_work_pending(&rc->rsv_update_work)) {
		if (rc->set_drp_ie_pending > 0) {
			rc->set_drp_ie_pending++;
			goto unlock;
		}
		uwb_rsv_queue_update(rc);
	}
unlock:
	spin_unlock_bh(&rc->rsvs_lock);
}

static void uwb_rsv_update_work(struct work_struct *work)
{
	struct uwb_rc *rc = container_of(work, struct uwb_rc,
					 rsv_update_work.work);
	bool ie_updated;

	mutex_lock(&rc->rsvs_mutex);

	ie_updated = uwb_rsv_update_all(rc);

	if (!rc->drp_avail.ie_valid) {
		uwb_drp_avail_ie_update(rc);
		ie_updated = true;
	}

	if (ie_updated && (rc->set_drp_ie_pending == 0))
		uwb_rc_send_all_drp_ie(rc);

	mutex_unlock(&rc->rsvs_mutex);
}

static void uwb_rsv_alien_bp_work(struct work_struct *work)
{
	struct uwb_rc *rc = container_of(work, struct uwb_rc,
					 rsv_alien_bp_work.work);
	struct uwb_rsv *rsv;

	mutex_lock(&rc->rsvs_mutex);
	
	list_for_each_entry(rsv, &rc->reservations, rc_node) {
		if (rsv->type != UWB_DRP_TYPE_ALIEN_BP) {
			rsv->callback(rsv);
		}
	}

	mutex_unlock(&rc->rsvs_mutex);
}

static void uwb_rsv_timer(unsigned long arg)
{
	struct uwb_rsv *rsv = (struct uwb_rsv *)arg;

	queue_work(rsv->rc->rsv_workq, &rsv->handle_timeout_work);
}

void uwb_rsv_remove_all(struct uwb_rc *rc)
{
	struct uwb_rsv *rsv, *t;

	mutex_lock(&rc->rsvs_mutex);
	list_for_each_entry_safe(rsv, t, &rc->reservations, rc_node) {
		if (rsv->state != UWB_RSV_STATE_NONE)
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_NONE);
		del_timer_sync(&rsv->timer);
	}
	
	rc->set_drp_ie_pending = 0;
	mutex_unlock(&rc->rsvs_mutex);

	cancel_delayed_work_sync(&rc->rsv_update_work);
	flush_workqueue(rc->rsv_workq);

	mutex_lock(&rc->rsvs_mutex);
	list_for_each_entry_safe(rsv, t, &rc->reservations, rc_node) {
		uwb_rsv_remove(rsv);
	}
	mutex_unlock(&rc->rsvs_mutex);
}

void uwb_rsv_init(struct uwb_rc *rc)
{
	INIT_LIST_HEAD(&rc->reservations);
	INIT_LIST_HEAD(&rc->cnflt_alien_list);
	mutex_init(&rc->rsvs_mutex);
	spin_lock_init(&rc->rsvs_lock);
	INIT_DELAYED_WORK(&rc->rsv_update_work, uwb_rsv_update_work);
	INIT_DELAYED_WORK(&rc->rsv_alien_bp_work, uwb_rsv_alien_bp_work);
	rc->bow.can_reserve_extra_mases = true;
	rc->bow.total_expired = 0;
	rc->bow.window = UWB_DRP_BACKOFF_WIN_MIN >> 1;
	init_timer(&rc->bow.timer);
	rc->bow.timer.function = uwb_rsv_backoff_win_timer;
	rc->bow.timer.data     = (unsigned long)&rc->bow;

	bitmap_complement(rc->uwb_dev.streams, rc->uwb_dev.streams, UWB_NUM_STREAMS);
}

int uwb_rsv_setup(struct uwb_rc *rc)
{
	char name[16];

	snprintf(name, sizeof(name), "%s_rsvd", dev_name(&rc->uwb_dev.dev));
	rc->rsv_workq = create_singlethread_workqueue(name);
	if (rc->rsv_workq == NULL)
		return -ENOMEM;

	return 0;
}

void uwb_rsv_cleanup(struct uwb_rc *rc)
{
	uwb_rsv_remove_all(rc);
	destroy_workqueue(rc->rsv_workq);
}
