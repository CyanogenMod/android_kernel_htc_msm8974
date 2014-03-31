/*
 * Ultra Wide Band
 * Dynamic Reservation Protocol handling
 *
 * Copyright (C) 2005-2006 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
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
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "uwb-internal.h"


enum uwb_drp_conflict_action {
	
	UWB_DRP_CONFLICT_MANTAIN = 0,
	
	UWB_DRP_CONFLICT_ACT1,
	
	
	UWB_DRP_CONFLICT_ACT2,

	UWB_DRP_CONFLICT_ACT3,
};


static void uwb_rc_set_drp_cmd_done(struct uwb_rc *rc, void *arg,
				    struct uwb_rceb *reply, ssize_t reply_size)
{
	struct uwb_rc_evt_set_drp_ie *r = (struct uwb_rc_evt_set_drp_ie *)reply;

	if (r != NULL) {
		if (r->bResultCode != UWB_RC_RES_SUCCESS)
			dev_err(&rc->uwb_dev.dev, "SET-DRP-IE failed: %s (%d)\n",
				uwb_rc_strerror(r->bResultCode), r->bResultCode);
	} else
		dev_err(&rc->uwb_dev.dev, "SET-DRP-IE: timeout\n");

	spin_lock_bh(&rc->rsvs_lock);
	if (rc->set_drp_ie_pending > 1) {
		rc->set_drp_ie_pending = 0;
		uwb_rsv_queue_update(rc);	
	} else {
		rc->set_drp_ie_pending = 0;	
	}
	spin_unlock_bh(&rc->rsvs_lock);
}

int uwb_rc_send_all_drp_ie(struct uwb_rc *rc)
{
	int result;
	struct uwb_rc_cmd_set_drp_ie *cmd;
	struct uwb_rsv *rsv;
	struct uwb_rsv_move *mv;
	int num_bytes = 0;
	u8 *IEDataptr;

	result = -ENOMEM;
	
	list_for_each_entry(rsv, &rc->reservations, rc_node) {
		if (rsv->drp_ie != NULL) {
			num_bytes += rsv->drp_ie->hdr.length + 2;
			if (uwb_rsv_has_two_drp_ies(rsv) &&
				(rsv->mv.companion_drp_ie != NULL)) {
				mv = &rsv->mv;
				num_bytes += mv->companion_drp_ie->hdr.length + 2;	
			}
		}
	}
	num_bytes += sizeof(rc->drp_avail.ie);
	cmd = kzalloc(sizeof(*cmd) + num_bytes, GFP_KERNEL);
	if (cmd == NULL)
		goto error;
	cmd->rccb.bCommandType = UWB_RC_CET_GENERAL;
	cmd->rccb.wCommand = cpu_to_le16(UWB_RC_CMD_SET_DRP_IE);
	cmd->wIELength = num_bytes;
	IEDataptr = (u8 *)&cmd->IEData[0];

	
	
	memcpy(IEDataptr, &rc->drp_avail.ie, sizeof(rc->drp_avail.ie));
	IEDataptr += sizeof(struct uwb_ie_drp_avail);

	
	list_for_each_entry(rsv, &rc->reservations, rc_node) {
		if (rsv->drp_ie != NULL) {
			memcpy(IEDataptr, rsv->drp_ie,
			       rsv->drp_ie->hdr.length + 2);
			IEDataptr += rsv->drp_ie->hdr.length + 2;
			
			if (uwb_rsv_has_two_drp_ies(rsv) &&
				(rsv->mv.companion_drp_ie != NULL)) {
				mv = &rsv->mv;
				memcpy(IEDataptr, mv->companion_drp_ie,
				       mv->companion_drp_ie->hdr.length + 2);
				IEDataptr += mv->companion_drp_ie->hdr.length + 2;	
			}
		}
	}

	result = uwb_rc_cmd_async(rc, "SET-DRP-IE", &cmd->rccb, sizeof(*cmd) + num_bytes,
				  UWB_RC_CET_GENERAL, UWB_RC_CMD_SET_DRP_IE,
				  uwb_rc_set_drp_cmd_done, NULL);
	
	rc->set_drp_ie_pending = 1;

	kfree(cmd);
error:
	return result;
}

static int evaluate_conflict_action(struct uwb_ie_drp *ext_drp_ie, int ext_beacon_slot,
				    struct uwb_rsv *rsv, int our_status)
{
	int our_tie_breaker = rsv->tiebreaker;
	int our_type        = rsv->type;
	int our_beacon_slot = rsv->rc->uwb_dev.beacon_slot;

	int ext_tie_breaker = uwb_ie_drp_tiebreaker(ext_drp_ie);
	int ext_status      = uwb_ie_drp_status(ext_drp_ie);
	int ext_type        = uwb_ie_drp_type(ext_drp_ie);
	
	
	
	if (ext_type == UWB_DRP_TYPE_PCA && our_type == UWB_DRP_TYPE_PCA) {
		return UWB_DRP_CONFLICT_MANTAIN;
	}

	
	if (our_type == UWB_DRP_TYPE_ALIEN_BP) {
		return UWB_DRP_CONFLICT_MANTAIN;
	}
	
	
	if (ext_type == UWB_DRP_TYPE_ALIEN_BP) {
		
		return UWB_DRP_CONFLICT_ACT1;
	}

	
	if (our_status == 0 && ext_status == 1) {
		return UWB_DRP_CONFLICT_ACT2;
	}

	
	if (our_status == 1 && ext_status == 0) {
		return UWB_DRP_CONFLICT_MANTAIN;
	}

	
	if (our_tie_breaker == ext_tie_breaker &&
	    our_beacon_slot <  ext_beacon_slot) {
		return UWB_DRP_CONFLICT_MANTAIN;
	}

	
	if (our_tie_breaker != ext_tie_breaker &&
	    our_beacon_slot >  ext_beacon_slot) {
		return UWB_DRP_CONFLICT_MANTAIN;
	}
	
	if (our_status == 0) {
		if (our_tie_breaker == ext_tie_breaker) {
			
			if (our_beacon_slot > ext_beacon_slot) {
				return UWB_DRP_CONFLICT_ACT2;
			}
		} else  {
			
			if (our_beacon_slot < ext_beacon_slot) {
				return UWB_DRP_CONFLICT_ACT2;
			}
		}
	} else {
		if (our_tie_breaker == ext_tie_breaker) {
			
			if (our_beacon_slot > ext_beacon_slot) {
				return UWB_DRP_CONFLICT_ACT3;
			}
		} else {
			
			if (our_beacon_slot < ext_beacon_slot) {
				return UWB_DRP_CONFLICT_ACT3;
			}
		}
	}
	return UWB_DRP_CONFLICT_MANTAIN;
}

static void handle_conflict_normal(struct uwb_ie_drp *drp_ie, 
				   int ext_beacon_slot, 
				   struct uwb_rsv *rsv, 
				   struct uwb_mas_bm *conflicting_mas)
{
	struct uwb_rc *rc = rsv->rc;
	struct uwb_rsv_move *mv = &rsv->mv;
	struct uwb_drp_backoff_win *bow = &rc->bow;
	int action;

	action = evaluate_conflict_action(drp_ie, ext_beacon_slot, rsv, uwb_rsv_status(rsv));

	if (uwb_rsv_is_owner(rsv)) {
		switch(action) {
		case UWB_DRP_CONFLICT_ACT2:
			
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_TO_BE_MOVED);
			if (bow->can_reserve_extra_mases == false)
				uwb_rsv_backoff_win_increment(rc);
			
			break;
		case UWB_DRP_CONFLICT_ACT3:
			uwb_rsv_backoff_win_increment(rc);
			
			
			bitmap_and(mv->companion_mas.bm, rsv->mas.bm, conflicting_mas->bm, UWB_NUM_MAS);
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MODIFIED);
		default:
			break;
		}
	} else {
		switch(action) {
		case UWB_DRP_CONFLICT_ACT2:
		case UWB_DRP_CONFLICT_ACT3:
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_CONFLICT);	
		default:
			break;
		}

	}
	
}

static void handle_conflict_expanding(struct uwb_ie_drp *drp_ie, int ext_beacon_slot,
				      struct uwb_rsv *rsv, bool companion_only,
				      struct uwb_mas_bm *conflicting_mas)
{
	struct uwb_rc *rc = rsv->rc;
	struct uwb_drp_backoff_win *bow = &rc->bow;
	struct uwb_rsv_move *mv = &rsv->mv;
	int action;
	
	if (companion_only) {
		
		action = evaluate_conflict_action(drp_ie, ext_beacon_slot, rsv, 0);
		if (uwb_rsv_is_owner(rsv)) {
			switch(action) {
			case UWB_DRP_CONFLICT_ACT2:
			case UWB_DRP_CONFLICT_ACT3:
				uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_ESTABLISHED);
				rsv->needs_release_companion_mas = false;
				if (bow->can_reserve_extra_mases == false)
					uwb_rsv_backoff_win_increment(rc);
				uwb_drp_avail_release(rsv->rc, &rsv->mv.companion_mas);
			}
		} else { 			
			switch(action) {
			case UWB_DRP_CONFLICT_ACT2:
			case UWB_DRP_CONFLICT_ACT3:
				uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_EXPANDING_CONFLICT);
                                
			}
		}
	} else { 		
		if (uwb_rsv_is_owner(rsv)) {
			uwb_rsv_backoff_win_increment(rc);
			
			uwb_drp_avail_release(rsv->rc, &rsv->mv.companion_mas);

			

			
			bitmap_andnot(mv->companion_mas.bm, rsv->mas.bm, conflicting_mas->bm, UWB_NUM_MAS);
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MODIFIED);
		} else { 
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_CONFLICT);
                        
		}
	}
}

static void uwb_drp_handle_conflict_rsv(struct uwb_rc *rc, struct uwb_rsv *rsv,
					struct uwb_rc_evt_drp *drp_evt, 
					struct uwb_ie_drp *drp_ie,
					struct uwb_mas_bm *conflicting_mas)
{
	struct uwb_rsv_move *mv;

	
	if (uwb_rsv_has_two_drp_ies(rsv)) {
		mv = &rsv->mv;
		if (bitmap_intersects(rsv->mas.bm, conflicting_mas->bm, UWB_NUM_MAS)) {
			handle_conflict_expanding(drp_ie, drp_evt->beacon_slot_number,
						  rsv, false, conflicting_mas);
		} else {
			if (bitmap_intersects(mv->companion_mas.bm, conflicting_mas->bm, UWB_NUM_MAS)) {
				handle_conflict_expanding(drp_ie, drp_evt->beacon_slot_number,
							  rsv, true, conflicting_mas);	
			}
		}
	} else if (bitmap_intersects(rsv->mas.bm, conflicting_mas->bm, UWB_NUM_MAS)) {
		handle_conflict_normal(drp_ie, drp_evt->beacon_slot_number, rsv, conflicting_mas);
	}
}

static void uwb_drp_handle_all_conflict_rsv(struct uwb_rc *rc,
					    struct uwb_rc_evt_drp *drp_evt, 
					    struct uwb_ie_drp *drp_ie,
					    struct uwb_mas_bm *conflicting_mas)
{
	struct uwb_rsv *rsv;
	
	list_for_each_entry(rsv, &rc->reservations, rc_node) {
		uwb_drp_handle_conflict_rsv(rc, rsv, drp_evt, drp_ie, conflicting_mas);	
	}
}
	
static void uwb_drp_process_target(struct uwb_rc *rc, struct uwb_rsv *rsv,
				   struct uwb_ie_drp *drp_ie, struct uwb_rc_evt_drp *drp_evt)
{
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_rsv_move *mv = &rsv->mv;
	int status;
	enum uwb_drp_reason reason_code;
	struct uwb_mas_bm mas;
	
	status = uwb_ie_drp_status(drp_ie);
	reason_code = uwb_ie_drp_reason_code(drp_ie);
	uwb_drp_ie_to_bm(&mas, drp_ie);

	switch (reason_code) {
	case UWB_DRP_REASON_ACCEPTED:

		if (rsv->state == UWB_RSV_STATE_T_CONFLICT) {
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_CONFLICT);
			break;
		}

		if (rsv->state == UWB_RSV_STATE_T_EXPANDING_ACCEPTED) {
			
			if (!bitmap_equal(rsv->mas.bm, mas.bm, UWB_NUM_MAS))
				
				uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_EXPANDING_ACCEPTED);	
		} else {
			if (!bitmap_equal(rsv->mas.bm, mas.bm, UWB_NUM_MAS)) {
				if (uwb_drp_avail_reserve_pending(rc, &mas) == -EBUSY) {
					uwb_drp_handle_all_conflict_rsv(rc, drp_evt, drp_ie, &mas);
				} else {
					
					bitmap_copy(mv->companion_mas.bm, mas.bm, UWB_NUM_MAS);
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_EXPANDING_ACCEPTED);
				}
			} else {
				if (status) {
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_ACCEPTED);
				}
			}
			
		}
		break;

	case UWB_DRP_REASON_MODIFIED:
		
		if (bitmap_equal(rsv->mas.bm, mas.bm, UWB_NUM_MAS)) {
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_ACCEPTED);
			break;
		}

		
		if (bitmap_subset(mas.bm, rsv->mas.bm, UWB_NUM_MAS)) {
			
			bitmap_andnot(mv->companion_mas.bm, rsv->mas.bm, mas.bm, UWB_NUM_MAS);
			uwb_drp_avail_release(rsv->rc, &mv->companion_mas);
		}

		bitmap_copy(rsv->mas.bm, mas.bm, UWB_NUM_MAS);
		uwb_rsv_set_state(rsv, UWB_RSV_STATE_T_RESIZED);
		break;
	default:
		dev_warn(dev, "ignoring invalid DRP IE state (%d/%d)\n",
			 reason_code, status);
	}
}

static void uwb_drp_process_owner(struct uwb_rc *rc, struct uwb_rsv *rsv,
				  struct uwb_dev *src, struct uwb_ie_drp *drp_ie,
				  struct uwb_rc_evt_drp *drp_evt)
{
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_rsv_move *mv = &rsv->mv;
	int status;
	enum uwb_drp_reason reason_code;
	struct uwb_mas_bm mas;

	status = uwb_ie_drp_status(drp_ie);
	reason_code = uwb_ie_drp_reason_code(drp_ie);
	uwb_drp_ie_to_bm(&mas, drp_ie);

	if (status) {
		switch (reason_code) {
		case UWB_DRP_REASON_ACCEPTED:
			switch (rsv->state) {
			case UWB_RSV_STATE_O_PENDING:
			case UWB_RSV_STATE_O_INITIATED:
			case UWB_RSV_STATE_O_ESTABLISHED:
				uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_ESTABLISHED);
				break;
			case UWB_RSV_STATE_O_MODIFIED:
				if (bitmap_equal(mas.bm, rsv->mas.bm, UWB_NUM_MAS)) {
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_ESTABLISHED);
				} else {
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MODIFIED);	
				}
				break;
				
			case UWB_RSV_STATE_O_MOVE_REDUCING: 
				if (bitmap_equal(mas.bm, rsv->mas.bm, UWB_NUM_MAS)) {
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_ESTABLISHED);
				} else {
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_REDUCING);	
				}
				break;
			case UWB_RSV_STATE_O_MOVE_EXPANDING:
				if (bitmap_equal(mas.bm, mv->companion_mas.bm, UWB_NUM_MAS)) {
					
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_COMBINING);
				} else {
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_EXPANDING);
				}
				break;
			case UWB_RSV_STATE_O_MOVE_COMBINING:
				if (bitmap_equal(mas.bm, rsv->mas.bm, UWB_NUM_MAS))
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_REDUCING);
				else
					uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_MOVE_COMBINING);
				break;
			default:
				break;	
			}
			break;
		default:
			dev_warn(dev, "ignoring invalid DRP IE state (%d/%d)\n",
				 reason_code, status);
		}
	} else {
		switch (reason_code) {
		case UWB_DRP_REASON_PENDING:
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_O_PENDING);
			break;
		case UWB_DRP_REASON_DENIED:
			uwb_rsv_set_state(rsv, UWB_RSV_STATE_NONE);
			break;
		case UWB_DRP_REASON_CONFLICT:
			
			bitmap_complement(mas.bm, src->last_availability_bm,
					  UWB_NUM_MAS);
			uwb_drp_handle_conflict_rsv(rc, rsv, drp_evt, drp_ie, &mas);
			break;
		default:
			dev_warn(dev, "ignoring invalid DRP IE state (%d/%d)\n",
				 reason_code, status);
		}
	}
}

static void uwb_cnflt_alien_stroke_timer(struct uwb_cnflt_alien *cnflt)
{
	unsigned timeout_us = UWB_MAX_LOST_BEACONS * UWB_SUPERFRAME_LENGTH_US;
	mod_timer(&cnflt->timer, jiffies + usecs_to_jiffies(timeout_us));
}

static void uwb_cnflt_update_work(struct work_struct *work)
{
	struct uwb_cnflt_alien *cnflt = container_of(work,
						     struct uwb_cnflt_alien,
						     cnflt_update_work);
	struct uwb_cnflt_alien *c;
	struct uwb_rc *rc = cnflt->rc;
	
	unsigned long delay_us = UWB_MAS_LENGTH_US * UWB_MAS_PER_ZONE;
	
	mutex_lock(&rc->rsvs_mutex);

	list_del(&cnflt->rc_node);

	
	bitmap_zero(rc->cnflt_alien_bitmap.bm, UWB_NUM_MAS);

	list_for_each_entry(c, &rc->cnflt_alien_list, rc_node) {
		bitmap_or(rc->cnflt_alien_bitmap.bm, rc->cnflt_alien_bitmap.bm, c->mas.bm, UWB_NUM_MAS);			
	}
	
	queue_delayed_work(rc->rsv_workq, &rc->rsv_alien_bp_work, usecs_to_jiffies(delay_us));

	kfree(cnflt);
	mutex_unlock(&rc->rsvs_mutex);
}

static void uwb_cnflt_timer(unsigned long arg)
{
	struct uwb_cnflt_alien *cnflt = (struct uwb_cnflt_alien *)arg;

	queue_work(cnflt->rc->rsv_workq, &cnflt->cnflt_update_work);
}

static void uwb_drp_handle_alien_drp(struct uwb_rc *rc, struct uwb_ie_drp *drp_ie)
{
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_mas_bm mas;
	struct uwb_cnflt_alien *cnflt;
	char buf[72];
	unsigned long delay_us = UWB_MAS_LENGTH_US * UWB_MAS_PER_ZONE;
	
	uwb_drp_ie_to_bm(&mas, drp_ie);
	bitmap_scnprintf(buf, sizeof(buf), mas.bm, UWB_NUM_MAS);
	
	list_for_each_entry(cnflt, &rc->cnflt_alien_list, rc_node) {
		if (bitmap_equal(cnflt->mas.bm, mas.bm, UWB_NUM_MAS)) {
			uwb_cnflt_alien_stroke_timer(cnflt);
			return;
		}
	}

	

	
	cnflt = kzalloc(sizeof(struct uwb_cnflt_alien), GFP_KERNEL);
	if (!cnflt)
		dev_err(dev, "failed to alloc uwb_cnflt_alien struct\n");
	INIT_LIST_HEAD(&cnflt->rc_node);
	init_timer(&cnflt->timer);
	cnflt->timer.function = uwb_cnflt_timer;
	cnflt->timer.data     = (unsigned long)cnflt;

	cnflt->rc = rc;
	INIT_WORK(&cnflt->cnflt_update_work, uwb_cnflt_update_work);
	
	bitmap_copy(cnflt->mas.bm, mas.bm, UWB_NUM_MAS);

	list_add_tail(&cnflt->rc_node, &rc->cnflt_alien_list);

	
	bitmap_or(rc->cnflt_alien_bitmap.bm, rc->cnflt_alien_bitmap.bm, mas.bm, UWB_NUM_MAS);

	queue_delayed_work(rc->rsv_workq, &rc->rsv_alien_bp_work, usecs_to_jiffies(delay_us));
	
	
	uwb_cnflt_alien_stroke_timer(cnflt);
}

static void uwb_drp_process_not_involved(struct uwb_rc *rc,
					 struct uwb_rc_evt_drp *drp_evt, 
					 struct uwb_ie_drp *drp_ie)
{
	struct uwb_mas_bm mas;
	
	uwb_drp_ie_to_bm(&mas, drp_ie);
	uwb_drp_handle_all_conflict_rsv(rc, drp_evt, drp_ie, &mas);
}

static void uwb_drp_process_involved(struct uwb_rc *rc, struct uwb_dev *src,
				     struct uwb_rc_evt_drp *drp_evt,
				     struct uwb_ie_drp *drp_ie)
{
	struct uwb_rsv *rsv;

	rsv = uwb_rsv_find(rc, src, drp_ie);
	if (!rsv) {
		return;
	}
	
	if (rsv->state == UWB_RSV_STATE_NONE) {
		uwb_rsv_set_state(rsv, UWB_RSV_STATE_NONE);
		return;
	}
			
	if (uwb_ie_drp_owner(drp_ie))
		uwb_drp_process_target(rc, rsv, drp_ie, drp_evt);
	else
		uwb_drp_process_owner(rc, rsv, src, drp_ie, drp_evt);
	
}


static bool uwb_drp_involves_us(struct uwb_rc *rc, struct uwb_ie_drp *drp_ie)
{
	return uwb_dev_addr_cmp(&rc->uwb_dev.dev_addr, &drp_ie->dev_addr) == 0;
}

static void uwb_drp_process(struct uwb_rc *rc, struct uwb_rc_evt_drp *drp_evt,
			    struct uwb_dev *src, struct uwb_ie_drp *drp_ie)
{
	if (uwb_ie_drp_type(drp_ie) == UWB_DRP_TYPE_ALIEN_BP)
		uwb_drp_handle_alien_drp(rc, drp_ie);
	else if (uwb_drp_involves_us(rc, drp_ie))
		uwb_drp_process_involved(rc, src, drp_evt, drp_ie);
	else
		uwb_drp_process_not_involved(rc, drp_evt, drp_ie);
}

static void uwb_drp_availability_process(struct uwb_rc *rc, struct uwb_dev *src,
					 struct uwb_ie_drp_avail *drp_availability_ie)
{
	bitmap_copy(src->last_availability_bm,
		    drp_availability_ie->bmp, UWB_NUM_MAS);
}

static
void uwb_drp_process_all(struct uwb_rc *rc, struct uwb_rc_evt_drp *drp_evt,
			 size_t ielen, struct uwb_dev *src_dev)
{
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_ie_hdr *ie_hdr;
	void *ptr;

	ptr = drp_evt->ie_data;
	for (;;) {
		ie_hdr = uwb_ie_next(&ptr, &ielen);
		if (!ie_hdr)
			break;

		switch (ie_hdr->element_id) {
		case UWB_IE_DRP_AVAILABILITY:
			uwb_drp_availability_process(rc, src_dev, (struct uwb_ie_drp_avail *)ie_hdr);
			break;
		case UWB_IE_DRP:
			uwb_drp_process(rc, drp_evt, src_dev, (struct uwb_ie_drp *)ie_hdr);
			break;
		default:
			dev_warn(dev, "unexpected IE in DRP notification\n");
			break;
		}
	}

	if (ielen > 0)
		dev_warn(dev, "%d octets remaining in DRP notification\n",
			 (int)ielen);
}

int uwbd_evt_handle_rc_drp(struct uwb_event *evt)
{
	struct device *dev = &evt->rc->uwb_dev.dev;
	struct uwb_rc *rc = evt->rc;
	struct uwb_rc_evt_drp *drp_evt;
	size_t ielength, bytes_left;
	struct uwb_dev_addr src_addr;
	struct uwb_dev *src_dev;

	if (evt->notif.size < sizeof(*drp_evt)) {
		dev_err(dev, "DRP event: Not enough data to decode event "
			"[%zu bytes left, %zu needed]\n",
			evt->notif.size, sizeof(*drp_evt));
		return 0;
	}
	bytes_left = evt->notif.size - sizeof(*drp_evt);
	drp_evt = container_of(evt->notif.rceb, struct uwb_rc_evt_drp, rceb);
	ielength = le16_to_cpu(drp_evt->ie_length);
	if (bytes_left != ielength) {
		dev_err(dev, "DRP event: Not enough data in payload [%zu"
			"bytes left, %zu declared in the event]\n",
			bytes_left, ielength);
		return 0;
	}

	memcpy(src_addr.data, &drp_evt->src_addr, sizeof(src_addr));
	src_dev = uwb_dev_get_by_devaddr(rc, &src_addr);
	if (!src_dev) {
		return 0;
	}

	mutex_lock(&rc->rsvs_mutex);

	
	uwb_drp_process_all(rc, drp_evt, ielength, src_dev);

	mutex_unlock(&rc->rsvs_mutex);

	uwb_dev_put(src_dev);
	return 0;
}
