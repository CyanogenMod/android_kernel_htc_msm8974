/*
 * Ultra Wide Band
 * DRP availability management
 *
 * Copyright (C) 2005-2006 Intel Corporation
 * Reinette Chatre <reinette.chatre@intel.com>
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
 *
 *
 * Manage DRP Availability (the MAS available for DRP
 * reservations). Thus:
 *
 * - Handle DRP Availability Change notifications
 *
 * - Allow the reservation manager to indicate MAS reserved/released
 *   by local (owned by/targeted at the radio controller)
 *   reservations.
 *
 * - Based on the two sources above, generate a DRP Availability IE to
 *   be included in the beacon.
 *
 * See also the documentation for struct uwb_drp_avail.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/bitmap.h>
#include "uwb-internal.h"

void uwb_drp_avail_init(struct uwb_rc *rc)
{
	bitmap_fill(rc->drp_avail.global, UWB_NUM_MAS);
	bitmap_fill(rc->drp_avail.local, UWB_NUM_MAS);
	bitmap_fill(rc->drp_avail.pending, UWB_NUM_MAS);
}

void uwb_drp_available(struct uwb_rc *rc, struct uwb_mas_bm *avail)
{
	bitmap_and(avail->bm, rc->drp_avail.global, rc->drp_avail.local, UWB_NUM_MAS);
	bitmap_and(avail->bm, avail->bm, rc->drp_avail.pending, UWB_NUM_MAS);
}

int uwb_drp_avail_reserve_pending(struct uwb_rc *rc, struct uwb_mas_bm *mas)
{
	struct uwb_mas_bm avail;

	uwb_drp_available(rc, &avail);
	if (!bitmap_subset(mas->bm, avail.bm, UWB_NUM_MAS))
		return -EBUSY;

	bitmap_andnot(rc->drp_avail.pending, rc->drp_avail.pending, mas->bm, UWB_NUM_MAS);
	return 0;
}

void uwb_drp_avail_reserve(struct uwb_rc *rc, struct uwb_mas_bm *mas)
{
	bitmap_or(rc->drp_avail.pending, rc->drp_avail.pending, mas->bm, UWB_NUM_MAS);
	bitmap_andnot(rc->drp_avail.local, rc->drp_avail.local, mas->bm, UWB_NUM_MAS);
	rc->drp_avail.ie_valid = false;
}

void uwb_drp_avail_release(struct uwb_rc *rc, struct uwb_mas_bm *mas)
{
	bitmap_or(rc->drp_avail.local, rc->drp_avail.local, mas->bm, UWB_NUM_MAS);
	bitmap_or(rc->drp_avail.pending, rc->drp_avail.pending, mas->bm, UWB_NUM_MAS);
	rc->drp_avail.ie_valid = false;
	uwb_rsv_handle_drp_avail_change(rc);
}

void uwb_drp_avail_ie_update(struct uwb_rc *rc)
{
	struct uwb_mas_bm avail;

	bitmap_and(avail.bm, rc->drp_avail.global, rc->drp_avail.local, UWB_NUM_MAS);

	rc->drp_avail.ie.hdr.element_id = UWB_IE_DRP_AVAILABILITY;
	rc->drp_avail.ie.hdr.length = UWB_NUM_MAS / 8;
	uwb_mas_bm_copy_le(rc->drp_avail.ie.bmp, &avail);
	rc->drp_avail.ie_valid = true;
}

static
unsigned long get_val(u8 *array, size_t itr, size_t len)
{
	unsigned long val = 0;
	size_t top = itr + len;

	BUG_ON(len > sizeof(val));

	while (itr < top) {
		val <<= 8;
		val |= array[top - 1];
		top--;
	}
	val <<= 8 * (sizeof(val) - len); 
	return val;
}

static
void buffer_to_bmp(unsigned long *bmp_itr, void *_buffer,
		   size_t buffer_size)
{
	u8 *buffer = _buffer;
	size_t itr, len;
	unsigned long val;

	itr = 0;
	while (itr < buffer_size) {
		len = buffer_size - itr >= sizeof(val) ?
			sizeof(val) : buffer_size - itr;
		val = get_val(buffer, itr, len);
		bmp_itr[itr / sizeof(val)] = val;
		itr += sizeof(val);
	}
}


static
int uwbd_evt_get_drp_avail(struct uwb_event *evt, unsigned long *bmp)
{
	struct device *dev = &evt->rc->uwb_dev.dev;
	struct uwb_rc_evt_drp_avail *drp_evt;
	int result = -EINVAL;

	
	if (evt->notif.size < sizeof(*drp_evt)) {
		dev_err(dev, "DRP Availability Change: Not enough "
			"data to decode event [%zu bytes, %zu "
			"needed]\n", evt->notif.size, sizeof(*drp_evt));
		goto error;
	}
	drp_evt = container_of(evt->notif.rceb, struct uwb_rc_evt_drp_avail, rceb);
	buffer_to_bmp(bmp, drp_evt->bmp, UWB_NUM_MAS/8);
	result = 0;
error:
	return result;
}


int uwbd_evt_handle_rc_drp_avail(struct uwb_event *evt)
{
	int result;
	struct uwb_rc *rc = evt->rc;
	DECLARE_BITMAP(bmp, UWB_NUM_MAS);

	result = uwbd_evt_get_drp_avail(evt, bmp);
	if (result < 0)
		return result;

	mutex_lock(&rc->rsvs_mutex);
	bitmap_copy(rc->drp_avail.global, bmp, UWB_NUM_MAS);
	rc->drp_avail.ie_valid = false;
	uwb_rsv_handle_drp_avail_change(rc);
	mutex_unlock(&rc->rsvs_mutex);

	uwb_rsv_sched_update(rc);

	return 0;
}
