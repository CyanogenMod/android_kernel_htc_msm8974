/*
 * Ultra Wide Band
 * Beacon management
 *
 * Copyright (C) 2005-2006 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * FIXME: docs
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>

#include "uwb-internal.h"

struct uwb_rc_cmd_start_beacon {
	struct uwb_rccb rccb;
	__le16 wBPSTOffset;
	u8 bChannelNumber;
} __attribute__((packed));


static int uwb_rc_start_beacon(struct uwb_rc *rc, u16 bpst_offset, u8 channel)
{
	int result;
	struct uwb_rc_cmd_start_beacon *cmd;
	struct uwb_rc_evt_confirm reply;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;
	cmd->rccb.bCommandType = UWB_RC_CET_GENERAL;
	cmd->rccb.wCommand = cpu_to_le16(UWB_RC_CMD_START_BEACON);
	cmd->wBPSTOffset = cpu_to_le16(bpst_offset);
	cmd->bChannelNumber = channel;
	reply.rceb.bEventType = UWB_RC_CET_GENERAL;
	reply.rceb.wEvent = UWB_RC_CMD_START_BEACON;
	result = uwb_rc_cmd(rc, "START-BEACON", &cmd->rccb, sizeof(*cmd),
			    &reply.rceb, sizeof(reply));
	if (result < 0)
		goto error_cmd;
	if (reply.bResultCode != UWB_RC_RES_SUCCESS) {
		dev_err(&rc->uwb_dev.dev,
			"START-BEACON: command execution failed: %s (%d)\n",
			uwb_rc_strerror(reply.bResultCode), reply.bResultCode);
		result = -EIO;
	}
error_cmd:
	kfree(cmd);
	return result;
}

static int uwb_rc_stop_beacon(struct uwb_rc *rc)
{
	int result;
	struct uwb_rccb *cmd;
	struct uwb_rc_evt_confirm reply;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;
	cmd->bCommandType = UWB_RC_CET_GENERAL;
	cmd->wCommand = cpu_to_le16(UWB_RC_CMD_STOP_BEACON);
	reply.rceb.bEventType = UWB_RC_CET_GENERAL;
	reply.rceb.wEvent = UWB_RC_CMD_STOP_BEACON;
	result = uwb_rc_cmd(rc, "STOP-BEACON", cmd, sizeof(*cmd),
			    &reply.rceb, sizeof(reply));
	if (result < 0)
		goto error_cmd;
	if (reply.bResultCode != UWB_RC_RES_SUCCESS) {
		dev_err(&rc->uwb_dev.dev,
			"STOP-BEACON: command execution failed: %s (%d)\n",
			uwb_rc_strerror(reply.bResultCode), reply.bResultCode);
		result = -EIO;
	}
error_cmd:
	kfree(cmd);
	return result;
}

int uwb_rc_beacon(struct uwb_rc *rc, int channel, unsigned bpst_offset)
{
	int result;
	struct device *dev = &rc->uwb_dev.dev;

	if (channel < 0)
		channel = -1;
	if (channel == -1)
		result = uwb_rc_stop_beacon(rc);
	else {
		
		result = uwb_rc_start_beacon(rc, bpst_offset, channel);
		if (result < 0)
			return result;
		if (le16_to_cpu(rc->ies->wIELength) > 0) {
			result = uwb_rc_set_ie(rc, rc->ies);
			if (result < 0) {
				dev_err(dev, "Cannot set new IE on device: "
					"%d\n", result);
				result = uwb_rc_stop_beacon(rc);
				channel = -1;
				bpst_offset = 0;
			}
		}
	}

	if (result >= 0)
		rc->beaconing = channel;
	return result;
}


void uwb_bce_kfree(struct kref *_bce)
{
	struct uwb_beca_e *bce = container_of(_bce, struct uwb_beca_e, refcnt);

	kfree(bce->be);
	kfree(bce);
}


static
struct uwb_beca_e *__uwb_beca_find_bydev(struct uwb_rc *rc,
					 const struct uwb_dev_addr *dev_addr)
{
	struct uwb_beca_e *bce, *next;
	list_for_each_entry_safe(bce, next, &rc->uwb_beca.list, node) {
		if (!memcmp(&bce->dev_addr, dev_addr, sizeof(bce->dev_addr)))
			goto out;
	}
	bce = NULL;
out:
	return bce;
}

static
struct uwb_beca_e *__uwb_beca_find_bymac(struct uwb_rc *rc, 
					 const struct uwb_mac_addr *mac_addr)
{
	struct uwb_beca_e *bce, *next;
	list_for_each_entry_safe(bce, next, &rc->uwb_beca.list, node) {
		if (!memcmp(bce->mac_addr, mac_addr->data,
			    sizeof(struct uwb_mac_addr)))
			goto out;
	}
	bce = NULL;
out:
	return bce;
}

struct uwb_dev *uwb_dev_get_by_devaddr(struct uwb_rc *rc,
				       const struct uwb_dev_addr *devaddr)
{
	struct uwb_dev *found = NULL;
	struct uwb_beca_e *bce;

	mutex_lock(&rc->uwb_beca.mutex);
	bce = __uwb_beca_find_bydev(rc, devaddr);
	if (bce)
		found = uwb_dev_try_get(rc, bce->uwb_dev);
	mutex_unlock(&rc->uwb_beca.mutex);

	return found;
}

struct uwb_dev *uwb_dev_get_by_macaddr(struct uwb_rc *rc,
				       const struct uwb_mac_addr *macaddr)
{
	struct uwb_dev *found = NULL;
	struct uwb_beca_e *bce;

	mutex_lock(&rc->uwb_beca.mutex);
	bce = __uwb_beca_find_bymac(rc, macaddr);
	if (bce)
		found = uwb_dev_try_get(rc, bce->uwb_dev);
	mutex_unlock(&rc->uwb_beca.mutex);

	return found;
}

static void uwb_beca_e_init(struct uwb_beca_e *bce)
{
	mutex_init(&bce->mutex);
	kref_init(&bce->refcnt);
	stats_init(&bce->lqe_stats);
	stats_init(&bce->rssi_stats);
}

static
struct uwb_beca_e *__uwb_beca_add(struct uwb_rc *rc,
				  struct uwb_rc_evt_beacon *be,
				  struct uwb_beacon_frame *bf,
				  unsigned long ts_jiffies)
{
	struct uwb_beca_e *bce;

	bce = kzalloc(sizeof(*bce), GFP_KERNEL);
	if (bce == NULL)
		return NULL;
	uwb_beca_e_init(bce);
	bce->ts_jiffies = ts_jiffies;
	bce->uwb_dev = NULL;
	list_add(&bce->node, &rc->uwb_beca.list);
	return bce;
}

void uwb_beca_purge(struct uwb_rc *rc)
{
	struct uwb_beca_e *bce, *next;
	unsigned long expires;

	mutex_lock(&rc->uwb_beca.mutex);
	list_for_each_entry_safe(bce, next, &rc->uwb_beca.list, node) {
		expires = bce->ts_jiffies + msecs_to_jiffies(beacon_timeout_ms);
		if (time_after(jiffies, expires)) {
			uwbd_dev_offair(bce);
		}
	}
	mutex_unlock(&rc->uwb_beca.mutex);
}

void uwb_beca_release(struct uwb_rc *rc)
{
	struct uwb_beca_e *bce, *next;

	mutex_lock(&rc->uwb_beca.mutex);
	list_for_each_entry_safe(bce, next, &rc->uwb_beca.list, node) {
		list_del(&bce->node);
		uwb_bce_put(bce);
	}
	mutex_unlock(&rc->uwb_beca.mutex);
}

static void uwb_beacon_print(struct uwb_rc *rc, struct uwb_rc_evt_beacon *be,
			     struct uwb_beacon_frame *bf)
{
	char macbuf[UWB_ADDR_STRSIZE];
	char devbuf[UWB_ADDR_STRSIZE];
	char dstbuf[UWB_ADDR_STRSIZE];

	uwb_mac_addr_print(macbuf, sizeof(macbuf), &bf->Device_Identifier);
	uwb_dev_addr_print(devbuf, sizeof(devbuf), &bf->hdr.SrcAddr);
	uwb_dev_addr_print(dstbuf, sizeof(dstbuf), &bf->hdr.DestAddr);
	dev_info(&rc->uwb_dev.dev,
		 "BEACON from %s to %s (ch%u offset %u slot %u MAC %s)\n",
		 devbuf, dstbuf, be->bChannelNumber, be->wBPSTOffset,
		 bf->Beacon_Slot_Number, macbuf);
}

ssize_t uwb_bce_print_IEs(struct uwb_dev *uwb_dev, struct uwb_beca_e *bce,
			  char *buf, size_t size)
{
	ssize_t result = 0;
	struct uwb_rc_evt_beacon *be;
	struct uwb_beacon_frame *bf;
	int ies_len;
	struct uwb_ie_hdr *ies;

	mutex_lock(&bce->mutex);

	be = bce->be;
	if (be) {
		bf = (struct uwb_beacon_frame *)bce->be->BeaconInfo;
		ies_len = be->wBeaconInfoLength - sizeof(struct uwb_beacon_frame);
		ies = (struct uwb_ie_hdr *)bf->IEData;

		result = uwb_ie_dump_hex(ies, ies_len, buf, size);
	}

	mutex_unlock(&bce->mutex);

	return result;
}

static int uwb_verify_beacon(struct uwb_rc *rc, struct uwb_event *evt,
			     struct uwb_rc_evt_beacon *be)
{
	int result = -EINVAL;
	struct uwb_beacon_frame *bf;
	struct device *dev = &rc->uwb_dev.dev;

	
	if (evt->notif.size < sizeof(*be) + sizeof(*bf)) {
		dev_err(dev, "BEACON event: Not enough data to decode "
			"(%zu vs %zu bytes needed)\n", evt->notif.size,
			sizeof(*be) + sizeof(*bf));
		goto error;
	}
	result = 0;
error:
	return result;
}

int uwbd_evt_handle_rc_beacon(struct uwb_event *evt)
{
	int result = -EINVAL;
	struct uwb_rc *rc;
	struct uwb_rc_evt_beacon *be;
	struct uwb_beacon_frame *bf;
	struct uwb_beca_e *bce;
	unsigned long last_ts;

	rc = evt->rc;
	be = container_of(evt->notif.rceb, struct uwb_rc_evt_beacon, rceb);
	result = uwb_verify_beacon(rc, evt, be);
	if (result < 0)
		return result;

	
	if (be->bBeaconType == UWB_RC_BEACON_TYPE_OL_ALIEN ||
	    be->bBeaconType == UWB_RC_BEACON_TYPE_NOL_ALIEN) {
		return -ENOSYS;
	}

	bf = (struct uwb_beacon_frame *) be->BeaconInfo;

	if (uwb_mac_addr_bcast(&bf->Device_Identifier))
		return 0;

	mutex_lock(&rc->uwb_beca.mutex);
	bce = __uwb_beca_find_bymac(rc, &bf->Device_Identifier);
	if (bce == NULL) {
		
		uwb_beacon_print(evt->rc, be, bf);
		bce = __uwb_beca_add(rc, be, bf, evt->ts_jiffies);
		if (bce == NULL) {
			mutex_unlock(&rc->uwb_beca.mutex);
			return -ENOMEM;
		}
	}
	mutex_unlock(&rc->uwb_beca.mutex);

	mutex_lock(&bce->mutex);
	
	kfree(bce->be);

	last_ts = bce->ts_jiffies;

	
	bce->ts_jiffies = evt->ts_jiffies;
	bce->be = be;
	bce->dev_addr = bf->hdr.SrcAddr;
	bce->mac_addr = &bf->Device_Identifier;
	be->wBPSTOffset = le16_to_cpu(be->wBPSTOffset);
	be->wBeaconInfoLength = le16_to_cpu(be->wBeaconInfoLength);
	stats_add_sample(&bce->lqe_stats, be->bLQI - 7);
	stats_add_sample(&bce->rssi_stats, be->bRSSI + 18);

	if (bce->uwb_dev == NULL)
		uwbd_dev_onair(evt->rc, bce);

	mutex_unlock(&bce->mutex);

	return 1; 
}

int uwbd_evt_handle_rc_beacon_size(struct uwb_event *evt)
{
	int result = -EINVAL;
	struct device *dev = &evt->rc->uwb_dev.dev;
	struct uwb_rc_evt_beacon_size *bs;

	
	if (evt->notif.size < sizeof(*bs)) {
		dev_err(dev, "BEACON SIZE notification: Not enough data to "
			"decode (%zu vs %zu bytes needed)\n",
			evt->notif.size, sizeof(*bs));
		goto error;
	}
	bs = container_of(evt->notif.rceb, struct uwb_rc_evt_beacon_size, rceb);
	if (0)
		dev_info(dev, "Beacon size changed to %u bytes "
			"(FIXME: action?)\n", le16_to_cpu(bs->wNewBeaconSize));
	else {
		
		static unsigned count;
		if (++count % 1000 == 0)
			dev_info(dev, "Beacon size changed %u times "
				"(FIXME: action?)\n", count);
	}
	result = 0;
error:
	return result;
}

int uwbd_evt_handle_rc_bp_slot_change(struct uwb_event *evt)
{
	struct uwb_rc *rc = evt->rc;
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_rc_evt_bp_slot_change *bpsc;

	if (evt->notif.size < sizeof(*bpsc)) {
		dev_err(dev, "BP SLOT CHANGE event: Not enough data\n");
		return -EINVAL;
	}
	bpsc = container_of(evt->notif.rceb, struct uwb_rc_evt_bp_slot_change, rceb);

	mutex_lock(&rc->uwb_dev.mutex);
	if (uwb_rc_evt_bp_slot_change_no_slot(bpsc)) {
		dev_info(dev, "stopped beaconing: No free slots in BP\n");
		rc->beaconing = -1;
	} else
		rc->uwb_dev.beacon_slot = uwb_rc_evt_bp_slot_change_slot_num(bpsc);
	mutex_unlock(&rc->uwb_dev.mutex);

	return 0;
}

struct uwb_ie_bpo {
	struct uwb_ie_hdr hdr;
	u8                bp_length;
	u8                data[];
} __attribute__((packed));

int uwbd_evt_handle_rc_bpoie_change(struct uwb_event *evt)
{
	int result = -EINVAL;
	struct device *dev = &evt->rc->uwb_dev.dev;
	struct uwb_rc_evt_bpoie_change *bpoiec;
	struct uwb_ie_bpo *bpoie;
	static unsigned count;	
	size_t iesize;

	
	if (evt->notif.size < sizeof(*bpoiec)) {
		dev_err(dev, "BPOIEC notification: Not enough data to "
			"decode (%zu vs %zu bytes needed)\n",
			evt->notif.size, sizeof(*bpoiec));
		goto error;
	}
	bpoiec = container_of(evt->notif.rceb, struct uwb_rc_evt_bpoie_change, rceb);
	iesize = le16_to_cpu(bpoiec->wBPOIELength);
	if (iesize < sizeof(*bpoie)) {
		dev_err(dev, "BPOIEC notification: Not enough IE data to "
			"decode (%zu vs %zu bytes needed)\n",
			iesize, sizeof(*bpoie));
		goto error;
	}
	if (++count % 1000 == 0)	
		dev_info(dev, "BPOIE: %u changes received\n", count);
	result = 0;
error:
	return result;
}

static ssize_t uwb_rc_beacon_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct uwb_dev *uwb_dev = to_uwb_dev(dev);
	struct uwb_rc *rc = uwb_dev->rc;
	ssize_t result;

	mutex_lock(&rc->uwb_dev.mutex);
	result = sprintf(buf, "%d\n", rc->beaconing);
	mutex_unlock(&rc->uwb_dev.mutex);
	return result;
}

static ssize_t uwb_rc_beacon_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct uwb_dev *uwb_dev = to_uwb_dev(dev);
	struct uwb_rc *rc = uwb_dev->rc;
	int channel;
	ssize_t result = -EINVAL;

	result = sscanf(buf, "%d", &channel);
	if (result >= 1)
		result = uwb_radio_force_channel(rc, channel);

	return result < 0 ? result : size;
}
DEVICE_ATTR(beacon, S_IRUGO | S_IWUSR, uwb_rc_beacon_show, uwb_rc_beacon_store);
