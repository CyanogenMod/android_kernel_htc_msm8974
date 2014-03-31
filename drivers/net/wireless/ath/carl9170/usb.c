/*
 * Atheros CARL9170 driver
 *
 * USB - frontend
 *
 * Copyright 2008, Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2009, 2010, Christian Lamparter <chunkeey@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, see
 * http://www.gnu.org/licenses/.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *    Copyright (c) 2007-2008 Atheros Communications, Inc.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/firmware.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <net/mac80211.h>
#include "carl9170.h"
#include "cmd.h"
#include "hw.h"
#include "fwcmd.h"

MODULE_AUTHOR("Johannes Berg <johannes@sipsolutions.net>");
MODULE_AUTHOR("Christian Lamparter <chunkeey@googlemail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Atheros AR9170 802.11n USB wireless");
MODULE_FIRMWARE(CARL9170FW_NAME);
MODULE_ALIAS("ar9170usb");
MODULE_ALIAS("arusb_lnx");

static struct usb_device_id carl9170_usb_ids[] = {
	
	{ USB_DEVICE(0x0cf3, 0x9170) },
	
	{ USB_DEVICE(0x0cf3, 0x1001) },
	
	{ USB_DEVICE(0x0cf3, 0x1002), .driver_info = CARL9170_WPS_BUTTON |
		 CARL9170_ONE_LED },
	
	{ USB_DEVICE(0x0cf3, 0x1010) },
	
	{ USB_DEVICE(0x0cf3, 0x1011) },
	
	{ USB_DEVICE(0xcace, 0x0300) },
	
	{ USB_DEVICE(0x07d1, 0x3c10) },
	
	{ USB_DEVICE(0x07d1, 0x3a09) },
	
	{ USB_DEVICE(0x07d1, 0x3a0f) },
	
	{ USB_DEVICE(0x0846, 0x9040) },
	
	{ USB_DEVICE(0x0846, 0x9010) },
	
	{ USB_DEVICE(0x0846, 0x9001), .driver_info = CARL9170_ONE_LED },
	
	{ USB_DEVICE(0x0ace, 0x1221) },
	
	{ USB_DEVICE(0x1435, 0x0804) },
	
	{ USB_DEVICE(0x1435, 0x0326) },
	
	{ USB_DEVICE(0x0586, 0x3417) },
	
	{ USB_DEVICE(0x0cde, 0x0023) },
	
	{ USB_DEVICE(0x0cde, 0x0026) },
	
	{ USB_DEVICE(0x0cde, 0x0027) },
	
	{ USB_DEVICE(0x083a, 0xf522) },
	
	{ USB_DEVICE(0x2019, 0x5304) },
	
	{ USB_DEVICE(0x04bb, 0x093f) },
	
	{ USB_DEVICE(0x0409, 0x0249) },
	
	{ USB_DEVICE(0x0409, 0x02b4) },
	
	{ USB_DEVICE(0x057c, 0x8401) },
	
	{ USB_DEVICE(0x057c, 0x8402) },
	
	{ USB_DEVICE(0x1668, 0x1200) },
	
	{ USB_DEVICE(0x1b75, 0x9170) },

	
	{}
};
MODULE_DEVICE_TABLE(usb, carl9170_usb_ids);

static void carl9170_usb_submit_data_urb(struct ar9170 *ar)
{
	struct urb *urb;
	int err;

	if (atomic_inc_return(&ar->tx_anch_urbs) > AR9170_NUM_TX_URBS)
		goto err_acc;

	urb = usb_get_from_anchor(&ar->tx_wait);
	if (!urb)
		goto err_acc;

	usb_anchor_urb(urb, &ar->tx_anch);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (unlikely(err)) {
		if (net_ratelimit()) {
			dev_err(&ar->udev->dev, "tx submit failed (%d)\n",
				urb->status);
		}

		usb_unanchor_urb(urb);
		usb_anchor_urb(urb, &ar->tx_err);
	}

	usb_free_urb(urb);

	if (likely(err == 0))
		return;

err_acc:
	atomic_dec(&ar->tx_anch_urbs);
}

static void carl9170_usb_tx_data_complete(struct urb *urb)
{
	struct ar9170 *ar = usb_get_intfdata(usb_ifnum_to_if(urb->dev, 0));

	if (WARN_ON_ONCE(!ar)) {
		dev_kfree_skb_irq(urb->context);
		return;
	}

	atomic_dec(&ar->tx_anch_urbs);

	switch (urb->status) {
	
	case 0:
		carl9170_tx_callback(ar, (void *)urb->context);
		break;

	
	case -ENOENT:
	case -ECONNRESET:
	case -ENODEV:
	case -ESHUTDOWN:
		usb_anchor_urb(urb, &ar->tx_err);
		return;

	
	default:
		if (net_ratelimit()) {
			dev_err(&ar->udev->dev, "tx failed (%d)\n",
				urb->status);
		}

		usb_anchor_urb(urb, &ar->tx_err);
		break;
	}

	if (likely(IS_STARTED(ar)))
		carl9170_usb_submit_data_urb(ar);
}

static int carl9170_usb_submit_cmd_urb(struct ar9170 *ar)
{
	struct urb *urb;
	int err;

	if (atomic_inc_return(&ar->tx_cmd_urbs) != 1) {
		atomic_dec(&ar->tx_cmd_urbs);
		return 0;
	}

	urb = usb_get_from_anchor(&ar->tx_cmd);
	if (!urb) {
		atomic_dec(&ar->tx_cmd_urbs);
		return 0;
	}

	usb_anchor_urb(urb, &ar->tx_anch);
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (unlikely(err)) {
		usb_unanchor_urb(urb);
		atomic_dec(&ar->tx_cmd_urbs);
	}
	usb_free_urb(urb);

	return err;
}

static void carl9170_usb_cmd_complete(struct urb *urb)
{
	struct ar9170 *ar = urb->context;
	int err = 0;

	if (WARN_ON_ONCE(!ar))
		return;

	atomic_dec(&ar->tx_cmd_urbs);

	switch (urb->status) {
	
	case 0:
		break;

	
	case -ENOENT:
	case -ECONNRESET:
	case -ENODEV:
	case -ESHUTDOWN:
		return;

	default:
		err = urb->status;
		break;
	}

	if (!IS_INITIALIZED(ar))
		return;

	if (err)
		dev_err(&ar->udev->dev, "submit cmd cb failed (%d).\n", err);

	err = carl9170_usb_submit_cmd_urb(ar);
	if (err)
		dev_err(&ar->udev->dev, "submit cmd failed (%d).\n", err);
}

static void carl9170_usb_rx_irq_complete(struct urb *urb)
{
	struct ar9170 *ar = urb->context;

	if (WARN_ON_ONCE(!ar))
		return;

	switch (urb->status) {
	
	case 0:
		break;

	
	case -ENOENT:
	case -ECONNRESET:
	case -ENODEV:
	case -ESHUTDOWN:
		return;

	default:
		goto resubmit;
	}

	carl9170_handle_command_response(ar, urb->transfer_buffer,
					 urb->actual_length);

resubmit:
	usb_anchor_urb(urb, &ar->rx_anch);
	if (unlikely(usb_submit_urb(urb, GFP_ATOMIC)))
		usb_unanchor_urb(urb);
}

static int carl9170_usb_submit_rx_urb(struct ar9170 *ar, gfp_t gfp)
{
	struct urb *urb;
	int err = 0, runs = 0;

	while ((atomic_read(&ar->rx_anch_urbs) < AR9170_NUM_RX_URBS) &&
		(runs++ < AR9170_NUM_RX_URBS)) {
		err = -ENOSPC;
		urb = usb_get_from_anchor(&ar->rx_pool);
		if (urb) {
			usb_anchor_urb(urb, &ar->rx_anch);
			err = usb_submit_urb(urb, gfp);
			if (unlikely(err)) {
				usb_unanchor_urb(urb);
				usb_anchor_urb(urb, &ar->rx_pool);
			} else {
				atomic_dec(&ar->rx_pool_urbs);
				atomic_inc(&ar->rx_anch_urbs);
			}
			usb_free_urb(urb);
		}
	}

	return err;
}

static void carl9170_usb_rx_work(struct ar9170 *ar)
{
	struct urb *urb;
	int i;

	for (i = 0; i < AR9170_NUM_RX_URBS_POOL; i++) {
		urb = usb_get_from_anchor(&ar->rx_work);
		if (!urb)
			break;

		atomic_dec(&ar->rx_work_urbs);
		if (IS_INITIALIZED(ar)) {
			carl9170_rx(ar, urb->transfer_buffer,
				    urb->actual_length);
		}

		usb_anchor_urb(urb, &ar->rx_pool);
		atomic_inc(&ar->rx_pool_urbs);

		usb_free_urb(urb);

		carl9170_usb_submit_rx_urb(ar, GFP_ATOMIC);
	}
}

void carl9170_usb_handle_tx_err(struct ar9170 *ar)
{
	struct urb *urb;

	while ((urb = usb_get_from_anchor(&ar->tx_err))) {
		struct sk_buff *skb = (void *)urb->context;

		carl9170_tx_drop(ar, skb);
		carl9170_tx_callback(ar, skb);
		usb_free_urb(urb);
	}
}

static void carl9170_usb_tasklet(unsigned long data)
{
	struct ar9170 *ar = (struct ar9170 *) data;

	if (!IS_INITIALIZED(ar))
		return;

	carl9170_usb_rx_work(ar);

	if (IS_STARTED(ar))
		carl9170_tx_scheduler(ar);
}

static void carl9170_usb_rx_complete(struct urb *urb)
{
	struct ar9170 *ar = (struct ar9170 *)urb->context;
	int err;

	if (WARN_ON_ONCE(!ar))
		return;

	atomic_dec(&ar->rx_anch_urbs);

	switch (urb->status) {
	case 0:
		
		usb_anchor_urb(urb, &ar->rx_work);
		atomic_inc(&ar->rx_work_urbs);
		break;

	case -ENOENT:
	case -ECONNRESET:
	case -ENODEV:
	case -ESHUTDOWN:
		
		return;

	default:
		
		usb_anchor_urb(urb, &ar->rx_pool);
		atomic_inc(&ar->rx_pool_urbs);
		break;
	}

	err = carl9170_usb_submit_rx_urb(ar, GFP_ATOMIC);
	if (unlikely(err)) {
		tasklet_hi_schedule(&ar->usb_tasklet);

		if (atomic_read(&ar->rx_anch_urbs) == 0) {

			ieee80211_queue_work(ar->hw, &ar->ping_work);
		}
	} else {
		tasklet_hi_schedule(&ar->usb_tasklet);
	}
}

static struct urb *carl9170_usb_alloc_rx_urb(struct ar9170 *ar, gfp_t gfp)
{
	struct urb *urb;
	void *buf;

	buf = kmalloc(ar->fw.rx_size, gfp);
	if (!buf)
		return NULL;

	urb = usb_alloc_urb(0, gfp);
	if (!urb) {
		kfree(buf);
		return NULL;
	}

	usb_fill_bulk_urb(urb, ar->udev, usb_rcvbulkpipe(ar->udev,
			  AR9170_USB_EP_RX), buf, ar->fw.rx_size,
			  carl9170_usb_rx_complete, ar);

	urb->transfer_flags |= URB_FREE_BUFFER;

	return urb;
}

static int carl9170_usb_send_rx_irq_urb(struct ar9170 *ar)
{
	struct urb *urb = NULL;
	void *ibuf;
	int err = -ENOMEM;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb)
		goto out;

	ibuf = kmalloc(AR9170_USB_EP_CTRL_MAX, GFP_KERNEL);
	if (!ibuf)
		goto out;

	usb_fill_int_urb(urb, ar->udev, usb_rcvintpipe(ar->udev,
			 AR9170_USB_EP_IRQ), ibuf, AR9170_USB_EP_CTRL_MAX,
			 carl9170_usb_rx_irq_complete, ar, 1);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &ar->rx_anch);
	err = usb_submit_urb(urb, GFP_KERNEL);
	if (err)
		usb_unanchor_urb(urb);

out:
	usb_free_urb(urb);
	return err;
}

static int carl9170_usb_init_rx_bulk_urbs(struct ar9170 *ar)
{
	struct urb *urb;
	int i, err = -EINVAL;

	for (i = 0; i < AR9170_NUM_RX_URBS_POOL; i++) {
		urb = carl9170_usb_alloc_rx_urb(ar, GFP_KERNEL);
		if (!urb) {
			err = -ENOMEM;
			goto err_out;
		}

		usb_anchor_urb(urb, &ar->rx_pool);
		atomic_inc(&ar->rx_pool_urbs);
		usb_free_urb(urb);
	}

	err = carl9170_usb_submit_rx_urb(ar, GFP_KERNEL);
	if (err)
		goto err_out;

	
	carl9170_set_state_when(ar, CARL9170_STOPPED, CARL9170_IDLE);
	return 0;

err_out:

	usb_scuttle_anchored_urbs(&ar->rx_pool);
	usb_scuttle_anchored_urbs(&ar->rx_work);
	usb_kill_anchored_urbs(&ar->rx_anch);
	return err;
}

static int carl9170_usb_flush(struct ar9170 *ar)
{
	struct urb *urb;
	int ret, err = 0;

	while ((urb = usb_get_from_anchor(&ar->tx_wait))) {
		struct sk_buff *skb = (void *)urb->context;
		carl9170_tx_drop(ar, skb);
		carl9170_tx_callback(ar, skb);
		usb_free_urb(urb);
	}

	ret = usb_wait_anchor_empty_timeout(&ar->tx_cmd, 1000);
	if (ret == 0)
		err = -ETIMEDOUT;

	
	ret = usb_wait_anchor_empty_timeout(&ar->tx_anch, 1000);
	if (ret == 0)
		err = -ETIMEDOUT;

	usb_kill_anchored_urbs(&ar->tx_anch);
	carl9170_usb_handle_tx_err(ar);

	return err;
}

static void carl9170_usb_cancel_urbs(struct ar9170 *ar)
{
	int err;

	carl9170_set_state(ar, CARL9170_UNKNOWN_STATE);

	err = carl9170_usb_flush(ar);
	if (err)
		dev_err(&ar->udev->dev, "stuck tx urbs!\n");

	usb_poison_anchored_urbs(&ar->tx_anch);
	carl9170_usb_handle_tx_err(ar);
	usb_poison_anchored_urbs(&ar->rx_anch);

	tasklet_kill(&ar->usb_tasklet);

	usb_scuttle_anchored_urbs(&ar->rx_work);
	usb_scuttle_anchored_urbs(&ar->rx_pool);
	usb_scuttle_anchored_urbs(&ar->tx_cmd);
}

int __carl9170_exec_cmd(struct ar9170 *ar, struct carl9170_cmd *cmd,
			const bool free_buf)
{
	struct urb *urb;
	int err = 0;

	if (!IS_INITIALIZED(ar)) {
		err = -EPERM;
		goto err_free;
	}

	if (WARN_ON(cmd->hdr.len > CARL9170_MAX_CMD_LEN - 4)) {
		err = -EINVAL;
		goto err_free;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		err = -ENOMEM;
		goto err_free;
	}

	usb_fill_int_urb(urb, ar->udev, usb_sndintpipe(ar->udev,
		AR9170_USB_EP_CMD), cmd, cmd->hdr.len + 4,
		carl9170_usb_cmd_complete, ar, 1);

	if (free_buf)
		urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &ar->tx_cmd);
	usb_free_urb(urb);

	return carl9170_usb_submit_cmd_urb(ar);

err_free:
	if (free_buf)
		kfree(cmd);

	return err;
}

int carl9170_exec_cmd(struct ar9170 *ar, const enum carl9170_cmd_oids cmd,
	unsigned int plen, void *payload, unsigned int outlen, void *out)
{
	int err = -ENOMEM;

	if (!IS_ACCEPTING_CMD(ar))
		return -EIO;

	if (!(cmd & CARL9170_CMD_ASYNC_FLAG))
		might_sleep();

	ar->cmd.hdr.len = plen;
	ar->cmd.hdr.cmd = cmd;
	
	if (plen && payload != (u8 *)(ar->cmd.data))
		memcpy(ar->cmd.data, payload, plen);

	spin_lock_bh(&ar->cmd_lock);
	ar->readbuf = (u8 *)out;
	ar->readlen = outlen;
	spin_unlock_bh(&ar->cmd_lock);

	err = __carl9170_exec_cmd(ar, &ar->cmd, false);

	if (!(cmd & CARL9170_CMD_ASYNC_FLAG)) {
		err = wait_for_completion_timeout(&ar->cmd_wait, HZ);
		if (err == 0) {
			err = -ETIMEDOUT;
			goto err_unbuf;
		}

		if (ar->readlen != outlen) {
			err = -EMSGSIZE;
			goto err_unbuf;
		}
	}

	return 0;

err_unbuf:
	
	if (IS_STARTED(ar)) {
		dev_err(&ar->udev->dev, "no command feedback "
			"received (%d).\n", err);

		
		print_hex_dump_bytes("carl9170 cmd: ", DUMP_PREFIX_NONE,
				     &ar->cmd, plen + 4);

		carl9170_restart(ar, CARL9170_RR_COMMAND_TIMEOUT);
	}

	
	spin_lock_bh(&ar->cmd_lock);
	ar->readbuf = NULL;
	ar->readlen = 0;
	spin_unlock_bh(&ar->cmd_lock);

	return err;
}

void carl9170_usb_tx(struct ar9170 *ar, struct sk_buff *skb)
{
	struct urb *urb;
	struct ar9170_stream *tx_stream;
	void *data;
	unsigned int len;

	if (!IS_STARTED(ar))
		goto err_drop;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		goto err_drop;

	if (ar->fw.tx_stream) {
		tx_stream = (void *) (skb->data - sizeof(*tx_stream));

		len = skb->len + sizeof(*tx_stream);
		tx_stream->length = cpu_to_le16(len);
		tx_stream->tag = cpu_to_le16(AR9170_TX_STREAM_TAG);
		data = tx_stream;
	} else {
		data = skb->data;
		len = skb->len;
	}

	usb_fill_bulk_urb(urb, ar->udev, usb_sndbulkpipe(ar->udev,
		AR9170_USB_EP_TX), data, len,
		carl9170_usb_tx_data_complete, skb);

	urb->transfer_flags |= URB_ZERO_PACKET;

	usb_anchor_urb(urb, &ar->tx_wait);

	usb_free_urb(urb);

	carl9170_usb_submit_data_urb(ar);
	return;

err_drop:
	carl9170_tx_drop(ar, skb);
	carl9170_tx_callback(ar, skb);
}

static void carl9170_release_firmware(struct ar9170 *ar)
{
	if (ar->fw.fw) {
		release_firmware(ar->fw.fw);
		memset(&ar->fw, 0, sizeof(ar->fw));
	}
}

void carl9170_usb_stop(struct ar9170 *ar)
{
	int ret;

	carl9170_set_state_when(ar, CARL9170_IDLE, CARL9170_STOPPED);

	ret = carl9170_usb_flush(ar);
	if (ret)
		dev_err(&ar->udev->dev, "kill pending tx urbs.\n");

	usb_poison_anchored_urbs(&ar->tx_anch);
	carl9170_usb_handle_tx_err(ar);

	
	spin_lock_bh(&ar->cmd_lock);
	ar->readlen = 0;
	spin_unlock_bh(&ar->cmd_lock);
	complete_all(&ar->cmd_wait);

	
	INIT_COMPLETION(ar->cmd_wait);

}

int carl9170_usb_open(struct ar9170 *ar)
{
	usb_unpoison_anchored_urbs(&ar->tx_anch);

	carl9170_set_state_when(ar, CARL9170_STOPPED, CARL9170_IDLE);
	return 0;
}

static int carl9170_usb_load_firmware(struct ar9170 *ar)
{
	const u8 *data;
	u8 *buf;
	unsigned int transfer;
	size_t len;
	u32 addr;
	int err = 0;

	buf = kmalloc(4096, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto err_out;
	}

	data = ar->fw.fw->data;
	len = ar->fw.fw->size;
	addr = ar->fw.address;

	
	data += ar->fw.offset;
	len -= ar->fw.offset;

	while (len) {
		transfer = min_t(unsigned int, len, 4096u);
		memcpy(buf, data, transfer);

		err = usb_control_msg(ar->udev, usb_sndctrlpipe(ar->udev, 0),
				      0x30 , 0x40 | USB_DIR_OUT,
				      addr >> 8, 0, buf, transfer, 100);

		if (err < 0) {
			kfree(buf);
			goto err_out;
		}

		len -= transfer;
		data += transfer;
		addr += transfer;
	}
	kfree(buf);

	err = usb_control_msg(ar->udev, usb_sndctrlpipe(ar->udev, 0),
			      0x31 ,
			      0x40 | USB_DIR_OUT, 0, 0, NULL, 0, 200);

	if (wait_for_completion_timeout(&ar->fw_boot_wait, HZ) == 0) {
		err = -ETIMEDOUT;
		goto err_out;
	}

	err = carl9170_echo_test(ar, 0x4a110123);
	if (err)
		goto err_out;

	
	ar->cmd_seq = -1;

	return 0;

err_out:
	dev_err(&ar->udev->dev, "firmware upload failed (%d).\n", err);
	return err;
}

int carl9170_usb_restart(struct ar9170 *ar)
{
	int err = 0;

	if (ar->intf->condition != USB_INTERFACE_BOUND)
		return 0;

	ar->cmd_seq = -2;

	err = carl9170_reboot(ar);

	carl9170_usb_stop(ar);

	if (err)
		goto err_out;

	tasklet_schedule(&ar->usb_tasklet);

	
	msleep(1100);

	err = carl9170_usb_open(ar);
	if (err)
		goto err_out;

	err = carl9170_usb_load_firmware(ar);
	if (err)
		goto err_out;

	return 0;

err_out:
	carl9170_usb_cancel_urbs(ar);
	return err;
}

void carl9170_usb_reset(struct ar9170 *ar)
{
	carl9170_usb_cancel_urbs(ar);

	carl9170_usb_stop(ar);

	usb_queue_reset_device(ar->intf);
}

static int carl9170_usb_init_device(struct ar9170 *ar)
{
	int err;

	ar->cmd_seq = -2;

	err = carl9170_usb_send_rx_irq_urb(ar);
	if (err)
		goto err_out;

	err = carl9170_usb_init_rx_bulk_urbs(ar);
	if (err)
		goto err_unrx;

	err = carl9170_usb_open(ar);
	if (err)
		goto err_unrx;

	mutex_lock(&ar->mutex);
	err = carl9170_usb_load_firmware(ar);
	mutex_unlock(&ar->mutex);
	if (err)
		goto err_stop;

	return 0;

err_stop:
	carl9170_usb_stop(ar);

err_unrx:
	carl9170_usb_cancel_urbs(ar);

err_out:
	return err;
}

static void carl9170_usb_firmware_failed(struct ar9170 *ar)
{
	struct device *parent = ar->udev->dev.parent;
	struct usb_device *udev;

	udev = ar->udev;

	complete(&ar->fw_load_wait);

	
	if (parent)
		device_lock(parent);

	device_release_driver(&udev->dev);
	if (parent)
		device_unlock(parent);

	usb_put_dev(udev);
}

static void carl9170_usb_firmware_finish(struct ar9170 *ar)
{
	int err;

	err = carl9170_parse_firmware(ar);
	if (err)
		goto err_freefw;

	err = carl9170_usb_init_device(ar);
	if (err)
		goto err_freefw;

	err = carl9170_register(ar);

	carl9170_usb_stop(ar);
	if (err)
		goto err_unrx;

	complete(&ar->fw_load_wait);
	usb_put_dev(ar->udev);
	return;

err_unrx:
	carl9170_usb_cancel_urbs(ar);

err_freefw:
	carl9170_release_firmware(ar);
	carl9170_usb_firmware_failed(ar);
}

static void carl9170_usb_firmware_step2(const struct firmware *fw,
					void *context)
{
	struct ar9170 *ar = context;

	if (fw) {
		ar->fw.fw = fw;
		carl9170_usb_firmware_finish(ar);
		return;
	}

	dev_err(&ar->udev->dev, "firmware not found.\n");
	carl9170_usb_firmware_failed(ar);
}

static int carl9170_usb_probe(struct usb_interface *intf,
			      const struct usb_device_id *id)
{
	struct ar9170 *ar;
	struct usb_device *udev;
	int err;

	err = usb_reset_device(interface_to_usbdev(intf));
	if (err)
		return err;

	ar = carl9170_alloc(sizeof(*ar));
	if (IS_ERR(ar))
		return PTR_ERR(ar);

	udev = interface_to_usbdev(intf);
	usb_get_dev(udev);
	ar->udev = udev;
	ar->intf = intf;
	ar->features = id->driver_info;

	usb_set_intfdata(intf, ar);
	SET_IEEE80211_DEV(ar->hw, &intf->dev);

	init_usb_anchor(&ar->rx_anch);
	init_usb_anchor(&ar->rx_pool);
	init_usb_anchor(&ar->rx_work);
	init_usb_anchor(&ar->tx_wait);
	init_usb_anchor(&ar->tx_anch);
	init_usb_anchor(&ar->tx_cmd);
	init_usb_anchor(&ar->tx_err);
	init_completion(&ar->cmd_wait);
	init_completion(&ar->fw_boot_wait);
	init_completion(&ar->fw_load_wait);
	tasklet_init(&ar->usb_tasklet, carl9170_usb_tasklet,
		     (unsigned long)ar);

	atomic_set(&ar->tx_cmd_urbs, 0);
	atomic_set(&ar->tx_anch_urbs, 0);
	atomic_set(&ar->rx_work_urbs, 0);
	atomic_set(&ar->rx_anch_urbs, 0);
	atomic_set(&ar->rx_pool_urbs, 0);

	usb_get_dev(ar->udev);

	carl9170_set_state(ar, CARL9170_STOPPED);

	return request_firmware_nowait(THIS_MODULE, 1, CARL9170FW_NAME,
		&ar->udev->dev, GFP_KERNEL, ar, carl9170_usb_firmware_step2);
}

static void carl9170_usb_disconnect(struct usb_interface *intf)
{
	struct ar9170 *ar = usb_get_intfdata(intf);
	struct usb_device *udev;

	if (WARN_ON(!ar))
		return;

	udev = ar->udev;
	wait_for_completion(&ar->fw_load_wait);

	if (IS_INITIALIZED(ar)) {
		carl9170_reboot(ar);
		carl9170_usb_stop(ar);
	}

	carl9170_usb_cancel_urbs(ar);
	carl9170_unregister(ar);

	usb_set_intfdata(intf, NULL);

	carl9170_release_firmware(ar);
	carl9170_free(ar);
	usb_put_dev(udev);
}

#ifdef CONFIG_PM
static int carl9170_usb_suspend(struct usb_interface *intf,
				pm_message_t message)
{
	struct ar9170 *ar = usb_get_intfdata(intf);

	if (!ar)
		return -ENODEV;

	carl9170_usb_cancel_urbs(ar);

	return 0;
}

static int carl9170_usb_resume(struct usb_interface *intf)
{
	struct ar9170 *ar = usb_get_intfdata(intf);
	int err;

	if (!ar)
		return -ENODEV;

	usb_unpoison_anchored_urbs(&ar->rx_anch);
	carl9170_set_state(ar, CARL9170_STOPPED);

	msleep(1100);

	err = carl9170_usb_init_device(ar);
	if (err)
		goto err_unrx;

	return 0;

err_unrx:
	carl9170_usb_cancel_urbs(ar);

	return err;
}
#endif 

static struct usb_driver carl9170_driver = {
	.name = KBUILD_MODNAME,
	.probe = carl9170_usb_probe,
	.disconnect = carl9170_usb_disconnect,
	.id_table = carl9170_usb_ids,
	.soft_unbind = 1,
#ifdef CONFIG_PM
	.suspend = carl9170_usb_suspend,
	.resume = carl9170_usb_resume,
	.reset_resume = carl9170_usb_resume,
#endif 
};

module_usb_driver(carl9170_driver);
