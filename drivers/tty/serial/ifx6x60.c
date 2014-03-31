/****************************************************************************
 *
 * Driver for the IFX 6x60 spi modem.
 *
 * Copyright (C) 2008 Option International
 * Copyright (C) 2008 Filip Aben <f.aben@option.com>
 *		      Denis Joseph Barrow <d.barow@option.com>
 *		      Jan Dumon <j.dumon@option.com>
 *
 * Copyright (C) 2009, 2010 Intel Corp
 * Russ Gorby <russ.gorby@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA
 *
 * Driver modified by Intel from Option gtm501l_spi.c
 *
 * Notes
 * o	The driver currently assumes a single device only. If you need to
 *	change this then look for saved_ifx_dev and add a device lookup
 * o	The driver is intended to be big-endian safe but has never been
 *	tested that way (no suitable hardware). There are a couple of FIXME
 *	notes by areas that may need addressing
 * o	Some of the GPIO naming/setup assumptions may need revisiting if
 *	you need to use this driver for another platform.
 *
 *****************************************************************************/
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/termios.h>
#include <linux/tty.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/kfifo.h>
#include <linux/tty_flip.h>
#include <linux/timer.h>
#include <linux/serial.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/rfkill.h>
#include <linux/fs.h>
#include <linux/ip.h>
#include <linux/dmapool.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/spi/ifx_modem.h>
#include <linux/delay.h>

#include "ifx6x60.h"

#define IFX_SPI_MORE_MASK		0x10
#define IFX_SPI_MORE_BIT		12	
#define IFX_SPI_CTS_BIT			13	
#define IFX_SPI_MODE			SPI_MODE_1
#define IFX_SPI_TTY_ID			0
#define IFX_SPI_TIMEOUT_SEC		2
#define IFX_SPI_HEADER_0		(-1)
#define IFX_SPI_HEADER_F		(-2)

static void ifx_spi_handle_srdy(struct ifx_spi_device *ifx_dev);

static int spi_bpw = 16;		
static struct tty_driver *tty_drv;
static struct ifx_spi_device *saved_ifx_dev;
static struct lock_class_key ifx_spi_key;


static inline void mrdy_set_high(struct ifx_spi_device *ifx)
{
	gpio_set_value(ifx->gpio.mrdy, 1);
}

static inline void mrdy_set_low(struct ifx_spi_device *ifx)
{
	gpio_set_value(ifx->gpio.mrdy, 0);
}

static void
ifx_spi_power_state_set(struct ifx_spi_device *ifx_dev, unsigned char val)
{
	unsigned long flags;

	spin_lock_irqsave(&ifx_dev->power_lock, flags);

	if (!ifx_dev->power_status)
		pm_runtime_get(&ifx_dev->spi_dev->dev);
	ifx_dev->power_status |= val;

	spin_unlock_irqrestore(&ifx_dev->power_lock, flags);
}

static void
ifx_spi_power_state_clear(struct ifx_spi_device *ifx_dev, unsigned char val)
{
	unsigned long flags;

	spin_lock_irqsave(&ifx_dev->power_lock, flags);

	if (ifx_dev->power_status) {
		ifx_dev->power_status &= ~val;
		if (!ifx_dev->power_status)
			pm_runtime_put(&ifx_dev->spi_dev->dev);
	}

	spin_unlock_irqrestore(&ifx_dev->power_lock, flags);
}

static inline void swap_buf(u16 *buf, int len, void *end)
{
	int n;

	len = ((len + 1) >> 1);
	if ((void *)&buf[len] > end) {
		pr_err("swap_buf: swap exceeds boundary (%p > %p)!",
		       &buf[len], end);
		return;
	}
	for (n = 0; n < len; n++) {
		*buf = cpu_to_be16(*buf);
		buf++;
	}
}

static void mrdy_assert(struct ifx_spi_device *ifx_dev)
{
	int val = gpio_get_value(ifx_dev->gpio.srdy);
	if (!val) {
		if (!test_and_set_bit(IFX_SPI_STATE_TIMER_PENDING,
				      &ifx_dev->flags)) {
			ifx_dev->spi_timer.expires =
				jiffies + IFX_SPI_TIMEOUT_SEC*HZ;
			add_timer(&ifx_dev->spi_timer);

		}
	}
	ifx_spi_power_state_set(ifx_dev, IFX_SPI_POWER_DATA_PENDING);
	mrdy_set_high(ifx_dev);
}

static void ifx_spi_ttyhangup(struct ifx_spi_device *ifx_dev)
{
	struct tty_port *pport = &ifx_dev->tty_port;
	struct tty_struct *tty = tty_port_tty_get(pport);
	if (tty) {
		tty_hangup(tty);
		tty_kref_put(tty);
	}
}

static void ifx_spi_timeout(unsigned long arg)
{
	struct ifx_spi_device *ifx_dev = (struct ifx_spi_device *)arg;

	dev_warn(&ifx_dev->spi_dev->dev, "*** SPI Timeout ***");
	ifx_spi_ttyhangup(ifx_dev);
	mrdy_set_low(ifx_dev);
	clear_bit(IFX_SPI_STATE_TIMER_PENDING, &ifx_dev->flags);
}


static int ifx_spi_tiocmget(struct tty_struct *tty)
{
	unsigned int value;
	struct ifx_spi_device *ifx_dev = tty->driver_data;

	value =
	(test_bit(IFX_SPI_RTS, &ifx_dev->signal_state) ? TIOCM_RTS : 0) |
	(test_bit(IFX_SPI_DTR, &ifx_dev->signal_state) ? TIOCM_DTR : 0) |
	(test_bit(IFX_SPI_CTS, &ifx_dev->signal_state) ? TIOCM_CTS : 0) |
	(test_bit(IFX_SPI_DSR, &ifx_dev->signal_state) ? TIOCM_DSR : 0) |
	(test_bit(IFX_SPI_DCD, &ifx_dev->signal_state) ? TIOCM_CAR : 0) |
	(test_bit(IFX_SPI_RI, &ifx_dev->signal_state) ? TIOCM_RNG : 0);
	return value;
}

static int ifx_spi_tiocmset(struct tty_struct *tty,
			    unsigned int set, unsigned int clear)
{
	struct ifx_spi_device *ifx_dev = tty->driver_data;

	if (set & TIOCM_RTS)
		set_bit(IFX_SPI_RTS, &ifx_dev->signal_state);
	if (set & TIOCM_DTR)
		set_bit(IFX_SPI_DTR, &ifx_dev->signal_state);
	if (clear & TIOCM_RTS)
		clear_bit(IFX_SPI_RTS, &ifx_dev->signal_state);
	if (clear & TIOCM_DTR)
		clear_bit(IFX_SPI_DTR, &ifx_dev->signal_state);

	set_bit(IFX_SPI_UPDATE, &ifx_dev->signal_state);
	return 0;
}

static int ifx_spi_open(struct tty_struct *tty, struct file *filp)
{
	return tty_port_open(&saved_ifx_dev->tty_port, tty, filp);
}

static void ifx_spi_close(struct tty_struct *tty, struct file *filp)
{
	struct ifx_spi_device *ifx_dev = tty->driver_data;
	tty_port_close(&ifx_dev->tty_port, tty, filp);
	
}

static int ifx_spi_decode_spi_header(unsigned char *buffer, int *length,
			unsigned char *more, unsigned char *received_cts)
{
	u16 h1;
	u16 h2;
	u16 *in_buffer = (u16 *)buffer;

	h1 = *in_buffer;
	h2 = *(in_buffer+1);

	if (h1 == 0 && h2 == 0) {
		*received_cts = 0;
		return IFX_SPI_HEADER_0;
	} else if (h1 == 0xffff && h2 == 0xffff) {
		
		return IFX_SPI_HEADER_F;
	}

	*length = h1 & 0xfff;	
	*more = (buffer[1] >> IFX_SPI_MORE_BIT) & 1;
	*received_cts = (buffer[3] >> IFX_SPI_CTS_BIT) & 1;
	return 0;
}

static void ifx_spi_setup_spi_header(unsigned char *txbuffer, int tx_count,
					unsigned char more)
{
	*(u16 *)(txbuffer) = tx_count;
	*(u16 *)(txbuffer+2) = IFX_SPI_PAYLOAD_SIZE;
	txbuffer[1] |= (more << IFX_SPI_MORE_BIT) & IFX_SPI_MORE_MASK;
}

static void ifx_spi_wakeup_serial(struct ifx_spi_device *ifx_dev)
{
	struct tty_struct *tty;

	tty = tty_port_tty_get(&ifx_dev->tty_port);
	if (!tty)
		return;
	tty_wakeup(tty);
	tty_kref_put(tty);
}

static int ifx_spi_prepare_tx_buffer(struct ifx_spi_device *ifx_dev)
{
	int temp_count;
	int queue_length;
	int tx_count;
	unsigned char *tx_buffer;

	tx_buffer = ifx_dev->tx_buffer;
	memset(tx_buffer, 0, IFX_SPI_TRANSFER_SIZE);

	
	tx_buffer += IFX_SPI_HEADER_OVERHEAD;
	tx_count = IFX_SPI_HEADER_OVERHEAD;

	ifx_dev->spi_more = 0;

	
	if (!ifx_dev->spi_slave_cts) {
		
		queue_length = kfifo_len(&ifx_dev->tx_fifo);
		if (queue_length != 0) {
			
			temp_count = min(queue_length, IFX_SPI_PAYLOAD_SIZE);
			temp_count = kfifo_out_locked(&ifx_dev->tx_fifo,
					tx_buffer, temp_count,
					&ifx_dev->fifo_lock);

			
			tx_buffer += temp_count;
			tx_count += temp_count;
			if (temp_count == queue_length)
				
				ifx_spi_wakeup_serial(ifx_dev);
			else 
				ifx_dev->spi_more = 1;
		}
	}
	
	
	ifx_spi_setup_spi_header(ifx_dev->tx_buffer,
					tx_count-IFX_SPI_HEADER_OVERHEAD,
					ifx_dev->spi_more);
	
	swap_buf((u16 *)(ifx_dev->tx_buffer), tx_count,
		&ifx_dev->tx_buffer[IFX_SPI_TRANSFER_SIZE]);
	return tx_count;
}

static int ifx_spi_write(struct tty_struct *tty, const unsigned char *buf,
			 int count)
{
	struct ifx_spi_device *ifx_dev = tty->driver_data;
	unsigned char *tmp_buf = (unsigned char *)buf;
	int tx_count = kfifo_in_locked(&ifx_dev->tx_fifo, tmp_buf, count,
				   &ifx_dev->fifo_lock);
	mrdy_assert(ifx_dev);
	return tx_count;
}

static int ifx_spi_write_room(struct tty_struct *tty)
{
	struct ifx_spi_device *ifx_dev = tty->driver_data;
	return IFX_SPI_FIFO_SIZE - kfifo_len(&ifx_dev->tx_fifo);
}

static int ifx_spi_chars_in_buffer(struct tty_struct *tty)
{
	struct ifx_spi_device *ifx_dev = tty->driver_data;
	return kfifo_len(&ifx_dev->tx_fifo);
}

static void ifx_spi_hangup(struct tty_struct *tty)
{
	struct ifx_spi_device *ifx_dev = tty->driver_data;
	tty_port_hangup(&ifx_dev->tty_port);
}

static int ifx_port_activate(struct tty_port *port, struct tty_struct *tty)
{
	struct ifx_spi_device *ifx_dev =
		container_of(port, struct ifx_spi_device, tty_port);

	
	kfifo_reset(&ifx_dev->tx_fifo);

	
	tty->driver_data = ifx_dev;

	
	tty->low_latency = 1;

	return 0;
}

static void ifx_port_shutdown(struct tty_port *port)
{
	struct ifx_spi_device *ifx_dev =
		container_of(port, struct ifx_spi_device, tty_port);

	mrdy_set_low(ifx_dev);
	clear_bit(IFX_SPI_STATE_TIMER_PENDING, &ifx_dev->flags);
	tasklet_kill(&ifx_dev->io_work_tasklet);
}

static const struct tty_port_operations ifx_tty_port_ops = {
	.activate = ifx_port_activate,
	.shutdown = ifx_port_shutdown,
};

static const struct tty_operations ifx_spi_serial_ops = {
	.open = ifx_spi_open,
	.close = ifx_spi_close,
	.write = ifx_spi_write,
	.hangup = ifx_spi_hangup,
	.write_room = ifx_spi_write_room,
	.chars_in_buffer = ifx_spi_chars_in_buffer,
	.tiocmget = ifx_spi_tiocmget,
	.tiocmset = ifx_spi_tiocmset,
};

static void ifx_spi_insert_flip_string(struct ifx_spi_device *ifx_dev,
				    unsigned char *chars, size_t size)
{
	struct tty_struct *tty = tty_port_tty_get(&ifx_dev->tty_port);
	if (!tty)
		return;
	tty_insert_flip_string(tty, chars, size);
	tty_flip_buffer_push(tty);
	tty_kref_put(tty);
}

static void ifx_spi_complete(void *ctx)
{
	struct ifx_spi_device *ifx_dev = ctx;
	struct tty_struct *tty;
	struct tty_ldisc *ldisc = NULL;
	int length;
	int actual_length;
	unsigned char more;
	unsigned char cts;
	int local_write_pending = 0;
	int queue_length;
	int srdy;
	int decode_result;

	mrdy_set_low(ifx_dev);

	if (!ifx_dev->spi_msg.status) {
		
		swap_buf((u16 *)ifx_dev->rx_buffer, IFX_SPI_HEADER_OVERHEAD,
			&ifx_dev->rx_buffer[IFX_SPI_HEADER_OVERHEAD]);
		decode_result = ifx_spi_decode_spi_header(ifx_dev->rx_buffer,
				&length, &more, &cts);
		if (decode_result == IFX_SPI_HEADER_0) {
			dev_dbg(&ifx_dev->spi_dev->dev,
				"ignore input: invalid header 0");
			ifx_dev->spi_slave_cts = 0;
			goto complete_exit;
		} else if (decode_result == IFX_SPI_HEADER_F) {
			dev_dbg(&ifx_dev->spi_dev->dev,
				"ignore input: invalid header F");
			goto complete_exit;
		}

		ifx_dev->spi_slave_cts = cts;

		actual_length = min((unsigned int)length,
					ifx_dev->spi_msg.actual_length);
		swap_buf((u16 *)(ifx_dev->rx_buffer + IFX_SPI_HEADER_OVERHEAD),
			 actual_length,
			 &ifx_dev->rx_buffer[IFX_SPI_TRANSFER_SIZE]);
		ifx_spi_insert_flip_string(
			ifx_dev,
			ifx_dev->rx_buffer + IFX_SPI_HEADER_OVERHEAD,
			(size_t)actual_length);
	} else {
		dev_dbg(&ifx_dev->spi_dev->dev, "SPI transfer error %d",
		       ifx_dev->spi_msg.status);
	}

complete_exit:
	if (ifx_dev->write_pending) {
		ifx_dev->write_pending = 0;
		local_write_pending = 1;
	}

	clear_bit(IFX_SPI_STATE_IO_IN_PROGRESS, &(ifx_dev->flags));

	queue_length = kfifo_len(&ifx_dev->tx_fifo);
	srdy = gpio_get_value(ifx_dev->gpio.srdy);
	if (!srdy)
		ifx_spi_power_state_clear(ifx_dev, IFX_SPI_POWER_SRDY);

	
	if (test_and_clear_bit(IFX_SPI_STATE_IO_READY, &ifx_dev->flags))
		tasklet_schedule(&ifx_dev->io_work_tasklet);
	else {
		if (more || ifx_dev->spi_more || queue_length > 0 ||
			local_write_pending) {
			if (ifx_dev->spi_slave_cts) {
				if (more)
					mrdy_assert(ifx_dev);
			} else
				mrdy_assert(ifx_dev);
		} else {
			ifx_spi_power_state_clear(ifx_dev,
						  IFX_SPI_POWER_DATA_PENDING);
			tty = tty_port_tty_get(&ifx_dev->tty_port);
			if (tty) {
				ldisc = tty_ldisc_ref(tty);
				if (ldisc) {
					ldisc->ops->write_wakeup(tty);
					tty_ldisc_deref(ldisc);
				}
				tty_kref_put(tty);
			}
		}
	}
}

static void ifx_spi_io(unsigned long data)
{
	int retval;
	struct ifx_spi_device *ifx_dev = (struct ifx_spi_device *) data;

	if (!test_and_set_bit(IFX_SPI_STATE_IO_IN_PROGRESS, &ifx_dev->flags)) {
		if (ifx_dev->gpio.unack_srdy_int_nb > 0)
			ifx_dev->gpio.unack_srdy_int_nb--;

		ifx_spi_prepare_tx_buffer(ifx_dev);

		spi_message_init(&ifx_dev->spi_msg);
		INIT_LIST_HEAD(&ifx_dev->spi_msg.queue);

		ifx_dev->spi_msg.context = ifx_dev;
		ifx_dev->spi_msg.complete = ifx_spi_complete;

		
		
		ifx_dev->spi_xfer.len = IFX_SPI_TRANSFER_SIZE;
		ifx_dev->spi_xfer.cs_change = 0;
		ifx_dev->spi_xfer.speed_hz = ifx_dev->spi_dev->max_speed_hz;
		
		ifx_dev->spi_xfer.bits_per_word = spi_bpw;

		ifx_dev->spi_xfer.tx_buf = ifx_dev->tx_buffer;
		ifx_dev->spi_xfer.rx_buf = ifx_dev->rx_buffer;

		if (ifx_dev->use_dma) {
			ifx_dev->spi_msg.is_dma_mapped = 1;
			ifx_dev->tx_dma = ifx_dev->tx_bus;
			ifx_dev->rx_dma = ifx_dev->rx_bus;
			ifx_dev->spi_xfer.tx_dma = ifx_dev->tx_dma;
			ifx_dev->spi_xfer.rx_dma = ifx_dev->rx_dma;
		} else {
			ifx_dev->spi_msg.is_dma_mapped = 0;
			ifx_dev->tx_dma = (dma_addr_t)0;
			ifx_dev->rx_dma = (dma_addr_t)0;
			ifx_dev->spi_xfer.tx_dma = (dma_addr_t)0;
			ifx_dev->spi_xfer.rx_dma = (dma_addr_t)0;
		}

		spi_message_add_tail(&ifx_dev->spi_xfer, &ifx_dev->spi_msg);

		mrdy_assert(ifx_dev);

		retval = spi_async(ifx_dev->spi_dev, &ifx_dev->spi_msg);
		if (retval) {
			clear_bit(IFX_SPI_STATE_IO_IN_PROGRESS,
				  &ifx_dev->flags);
			tasklet_schedule(&ifx_dev->io_work_tasklet);
			return;
		}
	} else
		ifx_dev->write_pending = 1;
}

static void ifx_spi_free_port(struct ifx_spi_device *ifx_dev)
{
	if (ifx_dev->tty_dev)
		tty_unregister_device(tty_drv, ifx_dev->minor);
	kfifo_free(&ifx_dev->tx_fifo);
}

static int ifx_spi_create_port(struct ifx_spi_device *ifx_dev)
{
	int ret = 0;
	struct tty_port *pport = &ifx_dev->tty_port;

	spin_lock_init(&ifx_dev->fifo_lock);
	lockdep_set_class_and_subclass(&ifx_dev->fifo_lock,
		&ifx_spi_key, 0);

	if (kfifo_alloc(&ifx_dev->tx_fifo, IFX_SPI_FIFO_SIZE, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto error_ret;
	}

	tty_port_init(pport);
	pport->ops = &ifx_tty_port_ops;
	ifx_dev->minor = IFX_SPI_TTY_ID;
	ifx_dev->tty_dev = tty_register_device(tty_drv, ifx_dev->minor,
					       &ifx_dev->spi_dev->dev);
	if (IS_ERR(ifx_dev->tty_dev)) {
		dev_dbg(&ifx_dev->spi_dev->dev,
			"%s: registering tty device failed", __func__);
		ret = PTR_ERR(ifx_dev->tty_dev);
		goto error_ret;
	}
	return 0;

error_ret:
	ifx_spi_free_port(ifx_dev);
	return ret;
}

static void ifx_spi_handle_srdy(struct ifx_spi_device *ifx_dev)
{
	if (test_bit(IFX_SPI_STATE_TIMER_PENDING, &ifx_dev->flags)) {
		del_timer_sync(&ifx_dev->spi_timer);
		clear_bit(IFX_SPI_STATE_TIMER_PENDING, &ifx_dev->flags);
	}

	ifx_spi_power_state_set(ifx_dev, IFX_SPI_POWER_SRDY);

	if (!test_bit(IFX_SPI_STATE_IO_IN_PROGRESS, &ifx_dev->flags))
		tasklet_schedule(&ifx_dev->io_work_tasklet);
	else
		set_bit(IFX_SPI_STATE_IO_READY, &ifx_dev->flags);
}

static irqreturn_t ifx_spi_srdy_interrupt(int irq, void *dev)
{
	struct ifx_spi_device *ifx_dev = dev;
	ifx_dev->gpio.unack_srdy_int_nb++;
	ifx_spi_handle_srdy(ifx_dev);
	return IRQ_HANDLED;
}

static irqreturn_t ifx_spi_reset_interrupt(int irq, void *dev)
{
	struct ifx_spi_device *ifx_dev = dev;
	int val = gpio_get_value(ifx_dev->gpio.reset_out);
	int solreset = test_bit(MR_START, &ifx_dev->mdm_reset_state);

	if (val == 0) {
		
		set_bit(MR_INPROGRESS, &ifx_dev->mdm_reset_state);
		if (!solreset) {
			
			ifx_spi_ttyhangup(ifx_dev);
		}
	} else {
		
		clear_bit(MR_INPROGRESS, &ifx_dev->mdm_reset_state);
		if (solreset) {
			set_bit(MR_COMPLETE, &ifx_dev->mdm_reset_state);
			wake_up(&ifx_dev->mdm_reset_wait);
		}
	}
	return IRQ_HANDLED;
}

static void ifx_spi_free_device(struct ifx_spi_device *ifx_dev)
{
	ifx_spi_free_port(ifx_dev);
	dma_free_coherent(&ifx_dev->spi_dev->dev,
				IFX_SPI_TRANSFER_SIZE,
				ifx_dev->tx_buffer,
				ifx_dev->tx_bus);
	dma_free_coherent(&ifx_dev->spi_dev->dev,
				IFX_SPI_TRANSFER_SIZE,
				ifx_dev->rx_buffer,
				ifx_dev->rx_bus);
}

static int ifx_spi_reset(struct ifx_spi_device *ifx_dev)
{
	int ret;
	set_bit(MR_START, &ifx_dev->mdm_reset_state);
	gpio_set_value(ifx_dev->gpio.po, 0);
	gpio_set_value(ifx_dev->gpio.reset, 0);
	msleep(25);
	gpio_set_value(ifx_dev->gpio.reset, 1);
	msleep(1);
	gpio_set_value(ifx_dev->gpio.po, 1);
	msleep(1);
	gpio_set_value(ifx_dev->gpio.po, 0);
	ret = wait_event_timeout(ifx_dev->mdm_reset_wait,
				 test_bit(MR_COMPLETE,
					  &ifx_dev->mdm_reset_state),
				 IFX_RESET_TIMEOUT);
	if (!ret)
		dev_warn(&ifx_dev->spi_dev->dev, "Modem reset timeout: (state:%lx)",
			 ifx_dev->mdm_reset_state);

	ifx_dev->mdm_reset_state = 0;
	return ret;
}


static int ifx_spi_spi_probe(struct spi_device *spi)
{
	int ret;
	int srdy;
	struct ifx_modem_platform_data *pl_data;
	struct ifx_spi_device *ifx_dev;

	if (saved_ifx_dev) {
		dev_dbg(&spi->dev, "ignoring subsequent detection");
		return -ENODEV;
	}

	pl_data = (struct ifx_modem_platform_data *)spi->dev.platform_data;
	if (!pl_data) {
		dev_err(&spi->dev, "missing platform data!");
		return -ENODEV;
	}

	
	ifx_dev = kzalloc(sizeof(struct ifx_spi_device), GFP_KERNEL);
	if (!ifx_dev) {
		dev_err(&spi->dev, "spi device allocation failed");
		return -ENOMEM;
	}
	saved_ifx_dev = ifx_dev;
	ifx_dev->spi_dev = spi;
	clear_bit(IFX_SPI_STATE_IO_IN_PROGRESS, &ifx_dev->flags);
	spin_lock_init(&ifx_dev->write_lock);
	spin_lock_init(&ifx_dev->power_lock);
	ifx_dev->power_status = 0;
	init_timer(&ifx_dev->spi_timer);
	ifx_dev->spi_timer.function = ifx_spi_timeout;
	ifx_dev->spi_timer.data = (unsigned long)ifx_dev;
	ifx_dev->modem = pl_data->modem_type;
	ifx_dev->use_dma = pl_data->use_dma;
	ifx_dev->max_hz = pl_data->max_hz;
	
	spi->max_speed_hz = ifx_dev->max_hz;
	spi->mode = IFX_SPI_MODE | (SPI_LOOP & spi->mode);
	spi->bits_per_word = spi_bpw;
	ret = spi_setup(spi);
	if (ret) {
		dev_err(&spi->dev, "SPI setup wasn't successful %d", ret);
		return -ENODEV;
	}

	
	ifx_dev->spi_more = 0;
	ifx_dev->spi_slave_cts = 0;

	
	ifx_dev->tx_buffer = dma_alloc_coherent(ifx_dev->spi_dev->dev.parent,
				IFX_SPI_TRANSFER_SIZE,
				&ifx_dev->tx_bus,
				GFP_KERNEL);
	if (!ifx_dev->tx_buffer) {
		dev_err(&spi->dev, "DMA-TX buffer allocation failed");
		ret = -ENOMEM;
		goto error_ret;
	}
	ifx_dev->rx_buffer = dma_alloc_coherent(ifx_dev->spi_dev->dev.parent,
				IFX_SPI_TRANSFER_SIZE,
				&ifx_dev->rx_bus,
				GFP_KERNEL);
	if (!ifx_dev->rx_buffer) {
		dev_err(&spi->dev, "DMA-RX buffer allocation failed");
		ret = -ENOMEM;
		goto error_ret;
	}

	
	init_waitqueue_head(&ifx_dev->mdm_reset_wait);

	spi_set_drvdata(spi, ifx_dev);
	tasklet_init(&ifx_dev->io_work_tasklet, ifx_spi_io,
						(unsigned long)ifx_dev);

	set_bit(IFX_SPI_STATE_PRESENT, &ifx_dev->flags);

	
	ret = ifx_spi_create_port(ifx_dev);
	if (ret != 0) {
		dev_err(&spi->dev, "create default tty port failed");
		goto error_ret;
	}

	ifx_dev->gpio.reset = pl_data->rst_pmu;
	ifx_dev->gpio.po = pl_data->pwr_on;
	ifx_dev->gpio.mrdy = pl_data->mrdy;
	ifx_dev->gpio.srdy = pl_data->srdy;
	ifx_dev->gpio.reset_out = pl_data->rst_out;

	dev_info(&spi->dev, "gpios %d, %d, %d, %d, %d",
		 ifx_dev->gpio.reset, ifx_dev->gpio.po, ifx_dev->gpio.mrdy,
		 ifx_dev->gpio.srdy, ifx_dev->gpio.reset_out);

	
	ret = gpio_request(ifx_dev->gpio.reset, "ifxModem");
	if (ret < 0) {
		dev_err(&spi->dev, "Unable to allocate GPIO%d (RESET)",
			ifx_dev->gpio.reset);
		goto error_ret;
	}
	ret += gpio_direction_output(ifx_dev->gpio.reset, 0);
	ret += gpio_export(ifx_dev->gpio.reset, 1);
	if (ret) {
		dev_err(&spi->dev, "Unable to configure GPIO%d (RESET)",
			ifx_dev->gpio.reset);
		ret = -EBUSY;
		goto error_ret2;
	}

	ret = gpio_request(ifx_dev->gpio.po, "ifxModem");
	ret += gpio_direction_output(ifx_dev->gpio.po, 0);
	ret += gpio_export(ifx_dev->gpio.po, 1);
	if (ret) {
		dev_err(&spi->dev, "Unable to configure GPIO%d (ON)",
			ifx_dev->gpio.po);
		ret = -EBUSY;
		goto error_ret3;
	}

	ret = gpio_request(ifx_dev->gpio.mrdy, "ifxModem");
	if (ret < 0) {
		dev_err(&spi->dev, "Unable to allocate GPIO%d (MRDY)",
			ifx_dev->gpio.mrdy);
		goto error_ret3;
	}
	ret += gpio_export(ifx_dev->gpio.mrdy, 1);
	ret += gpio_direction_output(ifx_dev->gpio.mrdy, 0);
	if (ret) {
		dev_err(&spi->dev, "Unable to configure GPIO%d (MRDY)",
			ifx_dev->gpio.mrdy);
		ret = -EBUSY;
		goto error_ret4;
	}

	ret = gpio_request(ifx_dev->gpio.srdy, "ifxModem");
	if (ret < 0) {
		dev_err(&spi->dev, "Unable to allocate GPIO%d (SRDY)",
			ifx_dev->gpio.srdy);
		ret = -EBUSY;
		goto error_ret4;
	}
	ret += gpio_export(ifx_dev->gpio.srdy, 1);
	ret += gpio_direction_input(ifx_dev->gpio.srdy);
	if (ret) {
		dev_err(&spi->dev, "Unable to configure GPIO%d (SRDY)",
			ifx_dev->gpio.srdy);
		ret = -EBUSY;
		goto error_ret5;
	}

	ret = gpio_request(ifx_dev->gpio.reset_out, "ifxModem");
	if (ret < 0) {
		dev_err(&spi->dev, "Unable to allocate GPIO%d (RESET_OUT)",
			ifx_dev->gpio.reset_out);
		goto error_ret5;
	}
	ret += gpio_export(ifx_dev->gpio.reset_out, 1);
	ret += gpio_direction_input(ifx_dev->gpio.reset_out);
	if (ret) {
		dev_err(&spi->dev, "Unable to configure GPIO%d (RESET_OUT)",
			ifx_dev->gpio.reset_out);
		ret = -EBUSY;
		goto error_ret6;
	}

	ret = request_irq(gpio_to_irq(ifx_dev->gpio.reset_out),
			  ifx_spi_reset_interrupt,
			  IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, DRVNAME,
		(void *)ifx_dev);
	if (ret) {
		dev_err(&spi->dev, "Unable to get irq %x\n",
			gpio_to_irq(ifx_dev->gpio.reset_out));
		goto error_ret6;
	}

	ret = ifx_spi_reset(ifx_dev);

	ret = request_irq(gpio_to_irq(ifx_dev->gpio.srdy),
			  ifx_spi_srdy_interrupt,
			  IRQF_TRIGGER_RISING, DRVNAME,
			  (void *)ifx_dev);
	if (ret) {
		dev_err(&spi->dev, "Unable to get irq %x",
			gpio_to_irq(ifx_dev->gpio.srdy));
		goto error_ret7;
	}

	
	pm_runtime_set_active(&spi->dev);
	pm_runtime_enable(&spi->dev);

	
	srdy = gpio_get_value(ifx_dev->gpio.srdy);

	if (srdy) {
		mrdy_assert(ifx_dev);
		ifx_spi_handle_srdy(ifx_dev);
	} else
		mrdy_set_low(ifx_dev);
	return 0;

error_ret7:
	free_irq(gpio_to_irq(ifx_dev->gpio.reset_out), (void *)ifx_dev);
error_ret6:
	gpio_free(ifx_dev->gpio.srdy);
error_ret5:
	gpio_free(ifx_dev->gpio.mrdy);
error_ret4:
	gpio_free(ifx_dev->gpio.reset);
error_ret3:
	gpio_free(ifx_dev->gpio.po);
error_ret2:
	gpio_free(ifx_dev->gpio.reset_out);
error_ret:
	ifx_spi_free_device(ifx_dev);
	saved_ifx_dev = NULL;
	return ret;
}


static int ifx_spi_spi_remove(struct spi_device *spi)
{
	struct ifx_spi_device *ifx_dev = spi_get_drvdata(spi);
	
	tasklet_kill(&ifx_dev->io_work_tasklet);
	
	free_irq(gpio_to_irq(ifx_dev->gpio.reset_out), (void *)ifx_dev);
	free_irq(gpio_to_irq(ifx_dev->gpio.srdy), (void *)ifx_dev);

	gpio_free(ifx_dev->gpio.srdy);
	gpio_free(ifx_dev->gpio.mrdy);
	gpio_free(ifx_dev->gpio.reset);
	gpio_free(ifx_dev->gpio.po);
	gpio_free(ifx_dev->gpio.reset_out);

	
	ifx_spi_free_device(ifx_dev);

	saved_ifx_dev = NULL;
	return 0;
}


static void ifx_spi_spi_shutdown(struct spi_device *spi)
{
}


static int ifx_spi_spi_suspend(struct spi_device *spi, pm_message_t msg)
{
	return 0;
}

static int ifx_spi_spi_resume(struct spi_device *spi)
{
	return 0;
}

static int ifx_spi_pm_suspend(struct device *dev)
{
	return 0;
}

static int ifx_spi_pm_resume(struct device *dev)
{
	return 0;
}

static int ifx_spi_pm_runtime_resume(struct device *dev)
{
	return 0;
}

static int ifx_spi_pm_runtime_suspend(struct device *dev)
{
	return 0;
}

static int ifx_spi_pm_runtime_idle(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ifx_spi_device *ifx_dev = spi_get_drvdata(spi);

	if (!ifx_dev->power_status)
		pm_runtime_suspend(dev);

	return 0;
}

static const struct dev_pm_ops ifx_spi_pm = {
	.resume = ifx_spi_pm_resume,
	.suspend = ifx_spi_pm_suspend,
	.runtime_resume = ifx_spi_pm_runtime_resume,
	.runtime_suspend = ifx_spi_pm_runtime_suspend,
	.runtime_idle = ifx_spi_pm_runtime_idle
};

static const struct spi_device_id ifx_id_table[] = {
	{"ifx6160", 0},
	{"ifx6260", 0},
	{ }
};
MODULE_DEVICE_TABLE(spi, ifx_id_table);

static const struct spi_driver ifx_spi_driver = {
	.driver = {
		.name = DRVNAME,
		.pm = &ifx_spi_pm,
		.owner = THIS_MODULE},
	.probe = ifx_spi_spi_probe,
	.shutdown = ifx_spi_spi_shutdown,
	.remove = __devexit_p(ifx_spi_spi_remove),
	.suspend = ifx_spi_spi_suspend,
	.resume = ifx_spi_spi_resume,
	.id_table = ifx_id_table
};


static void __exit ifx_spi_exit(void)
{
	
	tty_unregister_driver(tty_drv);
	spi_unregister_driver((void *)&ifx_spi_driver);
}


static int __init ifx_spi_init(void)
{
	int result;

	tty_drv = alloc_tty_driver(1);
	if (!tty_drv) {
		pr_err("%s: alloc_tty_driver failed", DRVNAME);
		return -ENOMEM;
	}

	tty_drv->driver_name = DRVNAME;
	tty_drv->name = TTYNAME;
	tty_drv->minor_start = IFX_SPI_TTY_ID;
	tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	tty_drv->subtype = SERIAL_TYPE_NORMAL;
	tty_drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_drv->init_termios = tty_std_termios;

	tty_set_operations(tty_drv, &ifx_spi_serial_ops);

	result = tty_register_driver(tty_drv);
	if (result) {
		pr_err("%s: tty_register_driver failed(%d)",
			DRVNAME, result);
		put_tty_driver(tty_drv);
		return result;
	}

	result = spi_register_driver((void *)&ifx_spi_driver);
	if (result) {
		pr_err("%s: spi_register_driver failed(%d)",
			DRVNAME, result);
		tty_unregister_driver(tty_drv);
	}
	return result;
}

module_init(ifx_spi_init);
module_exit(ifx_spi_exit);

MODULE_AUTHOR("Intel");
MODULE_DESCRIPTION("IFX6x60 spi driver");
MODULE_LICENSE("GPL");
MODULE_INFO(Version, "0.1-IFX6x60");
