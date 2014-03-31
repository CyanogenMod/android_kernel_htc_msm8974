/*
 * Intel Wireless WiMAX Connection 2400m
 * USB-specific i2400m driver definitions
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 *  - Initial implementation
 *
 *
 * This driver implements the bus-specific part of the i2400m for
 * USB. Check i2400m.h for a generic driver description.
 *
 * ARCHITECTURE
 *
 * This driver listens to notifications sent from the notification
 * endpoint (in usb-notif.c); when data is ready to read, the code in
 * there schedules a read from the device (usb-rx.c) and then passes
 * the data to the generic RX code (rx.c).
 *
 * When the generic driver needs to send data (network or control), it
 * queues up in the TX FIFO (tx.c) and that will notify the driver
 * through the i2400m->bus_tx_kick() callback
 * (usb-tx.c:i2400mu_bus_tx_kick) which will send the items in the
 * FIFO queue.
 *
 * This driver, as well, implements the USB-specific ops for the generic
 * driver to be able to setup/teardown communication with the device
 * [i2400m_bus_dev_start() and i2400m_bus_dev_stop()], reseting the
 * device [i2400m_bus_reset()] and performing firmware upload
 * [i2400m_bus_bm_cmd() and i2400_bus_bm_wait_for_ack()].
 */

#ifndef __I2400M_USB_H__
#define __I2400M_USB_H__

#include "i2400m.h"
#include <linux/kthread.h>


enum {
	EDC_MAX_ERRORS = 10,
	EDC_ERROR_TIMEFRAME = HZ,
};

struct edc {
	unsigned long timestart;
	u16 errorcount;
};

struct i2400m_endpoint_cfg {
	unsigned char bulk_out;
	unsigned char notification;
	unsigned char reset_cold;
	unsigned char bulk_in;
};

static inline void edc_init(struct edc *edc)
{
	edc->timestart = jiffies;
}

static inline int edc_inc(struct edc *edc, u16 max_err, u16 timeframe)
{
	unsigned long now;

	now = jiffies;
	if (now - edc->timestart > timeframe) {
		edc->errorcount = 1;
		edc->timestart = now;
	} else if (++edc->errorcount > max_err) {
		edc->errorcount = 0;
		edc->timestart = now;
		return 1;
	}
	return 0;
}

enum {
	I2400M_USB_BOOT_RETRIES = 3,
	I2400MU_MAX_NOTIFICATION_LEN = 256,
	I2400MU_BLK_SIZE = 16,
	I2400MU_PL_SIZE_MAX = 0x3EFF,

	
	USB_DEVICE_ID_I6050 = 0x0186,
	USB_DEVICE_ID_I6050_2 = 0x0188,
	USB_DEVICE_ID_I6250 = 0x0187,
};


struct i2400mu {
	struct i2400m i2400m;		

	struct usb_device *usb_dev;
	struct usb_interface *usb_iface;
	struct edc urb_edc;		
	struct i2400m_endpoint_cfg endpoint_cfg;

	struct urb *notif_urb;
	struct task_struct *tx_kthread;
	wait_queue_head_t tx_wq;

	struct task_struct *rx_kthread;
	wait_queue_head_t rx_wq;
	atomic_t rx_pending_count;
	size_t rx_size, rx_size_acc, rx_size_cnt;
	atomic_t do_autopm;
	u8 rx_size_auto_shrink;

	struct dentry *debugfs_dentry;
	unsigned i6050:1;	
};


static inline
void i2400mu_init(struct i2400mu *i2400mu)
{
	i2400m_init(&i2400mu->i2400m);
	edc_init(&i2400mu->urb_edc);
	init_waitqueue_head(&i2400mu->tx_wq);
	atomic_set(&i2400mu->rx_pending_count, 0);
	init_waitqueue_head(&i2400mu->rx_wq);
	i2400mu->rx_size = PAGE_SIZE - sizeof(struct skb_shared_info);
	atomic_set(&i2400mu->do_autopm, 1);
	i2400mu->rx_size_auto_shrink = 1;
}

extern int i2400mu_notification_setup(struct i2400mu *);
extern void i2400mu_notification_release(struct i2400mu *);

extern int i2400mu_rx_setup(struct i2400mu *);
extern void i2400mu_rx_release(struct i2400mu *);
extern void i2400mu_rx_kick(struct i2400mu *);

extern int i2400mu_tx_setup(struct i2400mu *);
extern void i2400mu_tx_release(struct i2400mu *);
extern void i2400mu_bus_tx_kick(struct i2400m *);

extern ssize_t i2400mu_bus_bm_cmd_send(struct i2400m *,
				       const struct i2400m_bootrom_header *,
				       size_t, int);
extern ssize_t i2400mu_bus_bm_wait_for_ack(struct i2400m *,
					   struct i2400m_bootrom_header *,
					   size_t);
#endif 
