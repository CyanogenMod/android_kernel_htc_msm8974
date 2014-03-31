/*
 * isochronous resources helper functions
 *
 * Copyright (c) Clemens Ladisch <clemens@ladisch.de>
 * Licensed under the terms of the GNU General Public License, version 2.
 */

#include <linux/device.h>
#include <linux/firewire.h>
#include <linux/firewire-constants.h>
#include <linux/export.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "iso-resources.h"

int fw_iso_resources_init(struct fw_iso_resources *r, struct fw_unit *unit)
{
	r->channels_mask = ~0uLL;
	r->unit = fw_unit_get(unit);
	mutex_init(&r->mutex);
	r->allocated = false;

	return 0;
}
EXPORT_SYMBOL(fw_iso_resources_init);

void fw_iso_resources_destroy(struct fw_iso_resources *r)
{
	WARN_ON(r->allocated);
	mutex_destroy(&r->mutex);
	fw_unit_put(r->unit);
}
EXPORT_SYMBOL(fw_iso_resources_destroy);

static unsigned int packet_bandwidth(unsigned int max_payload_bytes, int speed)
{
	unsigned int bytes, s400_bytes;

	
	bytes = 3 * 4 + ALIGN(max_payload_bytes, 4);

	
	if (speed <= SCODE_400)
		s400_bytes = bytes * (1 << (SCODE_400 - speed));
	else
		s400_bytes = DIV_ROUND_UP(bytes, 1 << (speed - SCODE_400));

	return s400_bytes;
}

static int current_bandwidth_overhead(struct fw_card *card)
{
	return card->gap_count < 63 ? card->gap_count * 97 / 10 + 89 : 512;
}

static int wait_isoch_resource_delay_after_bus_reset(struct fw_card *card)
{
	for (;;) {
		s64 delay = (card->reset_jiffies + HZ) - get_jiffies_64();
		if (delay <= 0)
			return 0;
		if (schedule_timeout_interruptible(delay) > 0)
			return -ERESTARTSYS;
	}
}

int fw_iso_resources_allocate(struct fw_iso_resources *r,
			      unsigned int max_payload_bytes, int speed)
{
	struct fw_card *card = fw_parent_device(r->unit)->card;
	int bandwidth, channel, err;

	if (WARN_ON(r->allocated))
		return -EBADFD;

	r->bandwidth = packet_bandwidth(max_payload_bytes, speed);

retry_after_bus_reset:
	spin_lock_irq(&card->lock);
	r->generation = card->generation;
	r->bandwidth_overhead = current_bandwidth_overhead(card);
	spin_unlock_irq(&card->lock);

	err = wait_isoch_resource_delay_after_bus_reset(card);
	if (err < 0)
		return err;

	mutex_lock(&r->mutex);

	bandwidth = r->bandwidth + r->bandwidth_overhead;
	fw_iso_resource_manage(card, r->generation, r->channels_mask,
			       &channel, &bandwidth, true);
	if (channel == -EAGAIN) {
		mutex_unlock(&r->mutex);
		goto retry_after_bus_reset;
	}
	if (channel >= 0) {
		r->channel = channel;
		r->allocated = true;
	} else {
		if (channel == -EBUSY)
			dev_err(&r->unit->device,
				"isochronous resources exhausted\n");
		else
			dev_err(&r->unit->device,
				"isochronous resource allocation failed\n");
	}

	mutex_unlock(&r->mutex);

	return channel;
}
EXPORT_SYMBOL(fw_iso_resources_allocate);

int fw_iso_resources_update(struct fw_iso_resources *r)
{
	struct fw_card *card = fw_parent_device(r->unit)->card;
	int bandwidth, channel;

	mutex_lock(&r->mutex);

	if (!r->allocated) {
		mutex_unlock(&r->mutex);
		return 0;
	}

	spin_lock_irq(&card->lock);
	r->generation = card->generation;
	r->bandwidth_overhead = current_bandwidth_overhead(card);
	spin_unlock_irq(&card->lock);

	bandwidth = r->bandwidth + r->bandwidth_overhead;

	fw_iso_resource_manage(card, r->generation, 1uLL << r->channel,
			       &channel, &bandwidth, true);
	if (channel < 0 && channel != -EAGAIN) {
		r->allocated = false;
		if (channel == -EBUSY)
			dev_err(&r->unit->device,
				"isochronous resources exhausted\n");
		else
			dev_err(&r->unit->device,
				"isochronous resource allocation failed\n");
	}

	mutex_unlock(&r->mutex);

	return channel;
}
EXPORT_SYMBOL(fw_iso_resources_update);

void fw_iso_resources_free(struct fw_iso_resources *r)
{
	struct fw_card *card = fw_parent_device(r->unit)->card;
	int bandwidth, channel;

	mutex_lock(&r->mutex);

	if (r->allocated) {
		bandwidth = r->bandwidth + r->bandwidth_overhead;
		fw_iso_resource_manage(card, r->generation, 1uLL << r->channel,
				       &channel, &bandwidth, false);
		if (channel < 0)
			dev_err(&r->unit->device,
				"isochronous resource deallocation failed\n");

		r->allocated = false;
	}

	mutex_unlock(&r->mutex);
}
EXPORT_SYMBOL(fw_iso_resources_free);
