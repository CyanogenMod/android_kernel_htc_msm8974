/*
 * WUSB Wire Adapter: Control/Data Streaming Interface (WUSB[8])
 * MMC (Microscheduled Management Command) handling
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
 * WUIEs and MMC IEs...well, they are almost the same at the end. MMC
 * IEs are Wireless USB IEs that go into the MMC period...[what is
 * that? look in Design-overview.txt].
 *
 *
 * This is a simple subsystem to keep track of which IEs are being
 * sent by the host in the MMC period.
 *
 * For each WUIE we ask to send, we keep it in an array, so we can
 * request its removal later, or replace the content. They are tracked
 * by pointer, so be sure to use the same pointer if you want to
 * remove it or update the contents.
 *
 * FIXME:
 *  - add timers that autoremove intervalled IEs?
 */
#include <linux/usb/wusb.h>
#include <linux/slab.h>
#include <linux/export.h>
#include "wusbhc.h"

int wusbhc_mmcie_create(struct wusbhc *wusbhc)
{
	u8 mmcies = wusbhc->mmcies_max;
	wusbhc->mmcie = kcalloc(mmcies, sizeof(wusbhc->mmcie[0]), GFP_KERNEL);
	if (wusbhc->mmcie == NULL)
		return -ENOMEM;
	mutex_init(&wusbhc->mmcie_mutex);
	return 0;
}

void wusbhc_mmcie_destroy(struct wusbhc *wusbhc)
{
	kfree(wusbhc->mmcie);
}

int wusbhc_mmcie_set(struct wusbhc *wusbhc, u8 interval, u8 repeat_cnt,
		     struct wuie_hdr *wuie)
{
	int result = -ENOBUFS;
	unsigned handle, itr;

	
	mutex_lock(&wusbhc->mmcie_mutex);
	switch (wuie->bIEIdentifier) {
	case WUIE_ID_HOST_INFO:
		
		handle = wusbhc->mmcies_max - 1;
		break;
	case WUIE_ID_ISOCH_DISCARD:
		dev_err(wusbhc->dev, "Special ordering case for WUIE ID 0x%x "
			"unimplemented\n", wuie->bIEIdentifier);
		result = -ENOSYS;
		goto error_unlock;
	default:
		
		handle = ~0;
		for (itr = 0; itr < wusbhc->mmcies_max - 1; itr++) {
			if (wusbhc->mmcie[itr] == wuie) {
				handle = itr;
				break;
			}
			if (wusbhc->mmcie[itr] == NULL)
				handle = itr;
		}
		if (handle == ~0)
			goto error_unlock;
	}
	result = (wusbhc->mmcie_add)(wusbhc, interval, repeat_cnt, handle,
				     wuie);
	if (result >= 0)
		wusbhc->mmcie[handle] = wuie;
error_unlock:
	mutex_unlock(&wusbhc->mmcie_mutex);
	return result;
}
EXPORT_SYMBOL_GPL(wusbhc_mmcie_set);

void wusbhc_mmcie_rm(struct wusbhc *wusbhc, struct wuie_hdr *wuie)
{
	int result;
	unsigned handle, itr;

	mutex_lock(&wusbhc->mmcie_mutex);
	for (itr = 0; itr < wusbhc->mmcies_max; itr++) {
		if (wusbhc->mmcie[itr] == wuie) {
			handle = itr;
			goto found;
		}
	}
	mutex_unlock(&wusbhc->mmcie_mutex);
	return;

found:
	result = (wusbhc->mmcie_rm)(wusbhc, handle);
	if (result == 0)
		wusbhc->mmcie[itr] = NULL;
	mutex_unlock(&wusbhc->mmcie_mutex);
}
EXPORT_SYMBOL_GPL(wusbhc_mmcie_rm);

static int wusbhc_mmc_start(struct wusbhc *wusbhc)
{
	int ret;

	mutex_lock(&wusbhc->mutex);
	ret = wusbhc->start(wusbhc);
	if (ret >= 0)
		wusbhc->active = 1;
	mutex_unlock(&wusbhc->mutex);

	return ret;
}

static void wusbhc_mmc_stop(struct wusbhc *wusbhc)
{
	mutex_lock(&wusbhc->mutex);
	wusbhc->active = 0;
	wusbhc->stop(wusbhc, WUSB_CHANNEL_STOP_DELAY_MS);
	mutex_unlock(&wusbhc->mutex);
}

int wusbhc_start(struct wusbhc *wusbhc)
{
	int result;
	struct device *dev = wusbhc->dev;

	WARN_ON(wusbhc->wuie_host_info != NULL);

	result = wusbhc_rsv_establish(wusbhc);
	if (result < 0) {
		dev_err(dev, "cannot establish cluster reservation: %d\n",
			result);
		goto error_rsv_establish;
	}

	result = wusbhc_devconnect_start(wusbhc);
	if (result < 0) {
		dev_err(dev, "error enabling device connections: %d\n", result);
		goto error_devconnect_start;
	}

	result = wusbhc_sec_start(wusbhc);
	if (result < 0) {
		dev_err(dev, "error starting security in the HC: %d\n", result);
		goto error_sec_start;
	}
	result = wusbhc->set_num_dnts(wusbhc, 0, 15);
	if (result < 0) {
		dev_err(dev, "Cannot set DNTS parameters: %d\n", result);
		goto error_set_num_dnts;
	}
	result = wusbhc_mmc_start(wusbhc);
	if (result < 0) {
		dev_err(dev, "error starting wusbch: %d\n", result);
		goto error_wusbhc_start;
	}

	return 0;

error_wusbhc_start:
	wusbhc_sec_stop(wusbhc);
error_set_num_dnts:
error_sec_start:
	wusbhc_devconnect_stop(wusbhc);
error_devconnect_start:
	wusbhc_rsv_terminate(wusbhc);
error_rsv_establish:
	return result;
}

void wusbhc_stop(struct wusbhc *wusbhc)
{
	wusbhc_mmc_stop(wusbhc);
	wusbhc_sec_stop(wusbhc);
	wusbhc_devconnect_stop(wusbhc);
	wusbhc_rsv_terminate(wusbhc);
}

int wusbhc_chid_set(struct wusbhc *wusbhc, const struct wusb_ckhdid *chid)
{
	int result = 0;

	if (memcmp(chid, &wusb_ckhdid_zero, sizeof(*chid)) == 0)
		chid = NULL;

	mutex_lock(&wusbhc->mutex);
	if (chid) {
		if (wusbhc->active) {
			mutex_unlock(&wusbhc->mutex);
			return -EBUSY;
		}
		wusbhc->chid = *chid;
	}
	mutex_unlock(&wusbhc->mutex);

	if (chid)
		result = uwb_radio_start(&wusbhc->pal);
	else
		uwb_radio_stop(&wusbhc->pal);
	return result;
}
EXPORT_SYMBOL_GPL(wusbhc_chid_set);
