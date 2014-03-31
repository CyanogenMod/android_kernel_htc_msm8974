/*
 * Ultra Wide Band
 * Information Element Handling
 *
 * Copyright (C) 2005-2006 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 * Reinette Chatre <reinette.chatre@intel.com>
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

#include <linux/slab.h>
#include <linux/export.h>
#include "uwb-internal.h"

struct uwb_ie_hdr *uwb_ie_next(void **ptr, size_t *len)
{
	struct uwb_ie_hdr *hdr;
	size_t ie_len;

	if (*len < sizeof(struct uwb_ie_hdr))
		return NULL;

	hdr = *ptr;
	ie_len = sizeof(struct uwb_ie_hdr) + hdr->length;

	if (*len < ie_len)
		return NULL;

	*ptr += ie_len;
	*len -= ie_len;

	return hdr;
}
EXPORT_SYMBOL_GPL(uwb_ie_next);

/**
 * uwb_ie_dump_hex - print IEs to a character buffer
 * @ies: the IEs to print.
 * @len: length of all the IEs.
 * @buf: the destination buffer.
 * @size: size of @buf.
 *
 * Returns the number of characters written.
 */
int uwb_ie_dump_hex(const struct uwb_ie_hdr *ies, size_t len,
		    char *buf, size_t size)
{
	void *ptr;
	const struct uwb_ie_hdr *ie;
	int r = 0;
	u8 *d;

	ptr = (void *)ies;
	for (;;) {
		ie = uwb_ie_next(&ptr, &len);
		if (!ie)
			break;

		r += scnprintf(buf + r, size - r, "%02x %02x",
			       (unsigned)ie->element_id,
			       (unsigned)ie->length);
		d = (uint8_t *)ie + sizeof(struct uwb_ie_hdr);
		while (d != ptr && r < size)
			r += scnprintf(buf + r, size - r, " %02x", (unsigned)*d++);
		if (r < size)
			buf[r++] = '\n';
	};

	return r;
}

static
ssize_t uwb_rc_get_ie(struct uwb_rc *uwb_rc, struct uwb_rc_evt_get_ie **pget_ie)
{
	ssize_t result;
	struct device *dev = &uwb_rc->uwb_dev.dev;
	struct uwb_rccb *cmd = NULL;
	struct uwb_rceb *reply = NULL;
	struct uwb_rc_evt_get_ie *get_ie;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;

	cmd->bCommandType = UWB_RC_CET_GENERAL;
	cmd->wCommand = cpu_to_le16(UWB_RC_CMD_GET_IE);
	result = uwb_rc_vcmd(uwb_rc, "GET_IE", cmd, sizeof(*cmd),
			     UWB_RC_CET_GENERAL, UWB_RC_CMD_GET_IE,
			     &reply);
	kfree(cmd);
	if (result < 0)
		return result;

	get_ie = container_of(reply, struct uwb_rc_evt_get_ie, rceb);
	if (result < sizeof(*get_ie)) {
		dev_err(dev, "not enough data returned for decoding GET IE "
			"(%zu bytes received vs %zu needed)\n",
			result, sizeof(*get_ie));
		return -EINVAL;
	} else if (result < sizeof(*get_ie) + le16_to_cpu(get_ie->wIELength)) {
		dev_err(dev, "not enough data returned for decoding GET IE "
			"payload (%zu bytes received vs %zu needed)\n", result,
			sizeof(*get_ie) + le16_to_cpu(get_ie->wIELength));
		return -EINVAL;
	}

	*pget_ie = get_ie;
	return result;
}


int uwb_rc_set_ie(struct uwb_rc *rc, struct uwb_rc_cmd_set_ie *cmd)
{
	int result;
	struct device *dev = &rc->uwb_dev.dev;
	struct uwb_rc_evt_set_ie reply;

	reply.rceb.bEventType = UWB_RC_CET_GENERAL;
	reply.rceb.wEvent = UWB_RC_CMD_SET_IE;
	result = uwb_rc_cmd(rc, "SET-IE", &cmd->rccb,
			    sizeof(*cmd) + le16_to_cpu(cmd->wIELength),
			    &reply.rceb, sizeof(reply));
	if (result < 0)
		goto error_cmd;
	else if (result != sizeof(reply)) {
		dev_err(dev, "SET-IE: not enough data to decode reply "
			"(%d bytes received vs %zu needed)\n",
			result, sizeof(reply));
		result = -EIO;
	} else if (reply.bResultCode != UWB_RC_RES_SUCCESS) {
		dev_err(dev, "SET-IE: command execution failed: %s (%d)\n",
			uwb_rc_strerror(reply.bResultCode), reply.bResultCode);
		result = -EIO;
	} else
		result = 0;
error_cmd:
	return result;
}

void uwb_rc_ie_init(struct uwb_rc *uwb_rc)
{
	mutex_init(&uwb_rc->ies_mutex);
}


int uwb_rc_ie_setup(struct uwb_rc *uwb_rc)
{
	struct uwb_rc_evt_get_ie *ie_info = NULL;
	int capacity;

	capacity = uwb_rc_get_ie(uwb_rc, &ie_info);
	if (capacity < 0)
		return capacity;

	mutex_lock(&uwb_rc->ies_mutex);

	uwb_rc->ies = (struct uwb_rc_cmd_set_ie *)ie_info;
	uwb_rc->ies->rccb.bCommandType = UWB_RC_CET_GENERAL;
	uwb_rc->ies->rccb.wCommand = cpu_to_le16(UWB_RC_CMD_SET_IE);
	uwb_rc->ies_capacity = capacity;

	mutex_unlock(&uwb_rc->ies_mutex);

	return 0;
}


void uwb_rc_ie_release(struct uwb_rc *uwb_rc)
{
	kfree(uwb_rc->ies);
	uwb_rc->ies = NULL;
	uwb_rc->ies_capacity = 0;
}


static int uwb_rc_ie_add_one(struct uwb_rc *rc, const struct uwb_ie_hdr *new_ie)
{
	struct uwb_rc_cmd_set_ie *new_ies;
	void *ptr, *prev_ie;
	struct uwb_ie_hdr *ie;
	size_t length, new_ie_len, new_capacity, size, prev_size;

	length = le16_to_cpu(rc->ies->wIELength);
	new_ie_len = sizeof(struct uwb_ie_hdr) + new_ie->length;
	new_capacity = sizeof(struct uwb_rc_cmd_set_ie) + length + new_ie_len;

	if (new_capacity > rc->ies_capacity) {
		new_ies = krealloc(rc->ies, new_capacity, GFP_KERNEL);
		if (!new_ies)
			return -ENOMEM;
		rc->ies = new_ies;
	}

	ptr = rc->ies->IEData;
	size = length;
	for (;;) {
		prev_ie = ptr;
		prev_size = size;
		ie = uwb_ie_next(&ptr, &size);
		if (!ie || ie->element_id > new_ie->element_id)
			break;
	}

	memmove(prev_ie + new_ie_len, prev_ie, prev_size);
	memcpy(prev_ie, new_ie, new_ie_len);
	rc->ies->wIELength = cpu_to_le16(length + new_ie_len);

	return 0;
}

int uwb_rc_ie_add(struct uwb_rc *uwb_rc,
		  const struct uwb_ie_hdr *ies, size_t size)
{
	int result = 0;
	void *ptr;
	const struct uwb_ie_hdr *ie;

	mutex_lock(&uwb_rc->ies_mutex);

	ptr = (void *)ies;
	for (;;) {
		ie = uwb_ie_next(&ptr, &size);
		if (!ie)
			break;

		result = uwb_rc_ie_add_one(uwb_rc, ie);
		if (result < 0)
			break;
	}
	if (result >= 0) {
		if (size == 0) {
			if (uwb_rc->beaconing != -1)
				result = uwb_rc_set_ie(uwb_rc, uwb_rc->ies);
		} else
			result = -EINVAL;
	}

	mutex_unlock(&uwb_rc->ies_mutex);

	return result;
}
EXPORT_SYMBOL_GPL(uwb_rc_ie_add);


static
void uwb_rc_ie_cache_rm(struct uwb_rc *uwb_rc, enum uwb_ie to_remove)
{
	struct uwb_ie_hdr *ie;
	size_t len = le16_to_cpu(uwb_rc->ies->wIELength);
	void *ptr;
	size_t size;

	ptr = uwb_rc->ies->IEData;
	size = len;
	for (;;) {
		ie = uwb_ie_next(&ptr, &size);
		if (!ie)
			break;
		if (ie->element_id == to_remove) {
			len -= sizeof(struct uwb_ie_hdr) + ie->length;
			memmove(ie, ptr, size);
			ptr = ie;
		}
	}
	uwb_rc->ies->wIELength = cpu_to_le16(len);
}


int uwb_rc_ie_rm(struct uwb_rc *uwb_rc, enum uwb_ie element_id)
{
	int result = 0;

	mutex_lock(&uwb_rc->ies_mutex);

	uwb_rc_ie_cache_rm(uwb_rc, element_id);

	if (uwb_rc->beaconing != -1)
		result = uwb_rc_set_ie(uwb_rc, uwb_rc->ies);

	mutex_unlock(&uwb_rc->ies_mutex);

	return result;
}
EXPORT_SYMBOL_GPL(uwb_rc_ie_rm);
