/*********************************************************************
 *
 * Filename:      ircomm_tty.c
 * Version:       1.0
 * Description:   IrCOMM serial TTY driver
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun Jun  6 21:00:56 1999
 * Modified at:   Wed Feb 23 00:09:02 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Sources:       serial.c and previous IrCOMM work by Takahide Higuchi
 *
 *     Copyright (c) 1999-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2003 Jean Tourrilhes <jt@hpl.hp.com>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/termios.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/interrupt.h>
#include <linux/device.h>		

#include <asm/uaccess.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>

#include <net/irda/ircomm_core.h>
#include <net/irda/ircomm_param.h>
#include <net/irda/ircomm_tty_attach.h>
#include <net/irda/ircomm_tty.h>

static int  ircomm_tty_open(struct tty_struct *tty, struct file *filp);
static void ircomm_tty_close(struct tty_struct * tty, struct file *filp);
static int  ircomm_tty_write(struct tty_struct * tty,
			     const unsigned char *buf, int count);
static int  ircomm_tty_write_room(struct tty_struct *tty);
static void ircomm_tty_throttle(struct tty_struct *tty);
static void ircomm_tty_unthrottle(struct tty_struct *tty);
static int  ircomm_tty_chars_in_buffer(struct tty_struct *tty);
static void ircomm_tty_flush_buffer(struct tty_struct *tty);
static void ircomm_tty_send_xchar(struct tty_struct *tty, char ch);
static void ircomm_tty_wait_until_sent(struct tty_struct *tty, int timeout);
static void ircomm_tty_hangup(struct tty_struct *tty);
static void ircomm_tty_do_softint(struct work_struct *work);
static void ircomm_tty_shutdown(struct ircomm_tty_cb *self);
static void ircomm_tty_stop(struct tty_struct *tty);

static int ircomm_tty_data_indication(void *instance, void *sap,
				      struct sk_buff *skb);
static int ircomm_tty_control_indication(void *instance, void *sap,
					 struct sk_buff *skb);
static void ircomm_tty_flow_indication(void *instance, void *sap,
				       LOCAL_FLOW cmd);
#ifdef CONFIG_PROC_FS
static const struct file_operations ircomm_tty_proc_fops;
#endif 
static struct tty_driver *driver;

static hashbin_t *ircomm_tty = NULL;

static const struct tty_operations ops = {
	.open            = ircomm_tty_open,
	.close           = ircomm_tty_close,
	.write           = ircomm_tty_write,
	.write_room      = ircomm_tty_write_room,
	.chars_in_buffer = ircomm_tty_chars_in_buffer,
	.flush_buffer    = ircomm_tty_flush_buffer,
	.ioctl           = ircomm_tty_ioctl,	
	.tiocmget        = ircomm_tty_tiocmget,	
	.tiocmset        = ircomm_tty_tiocmset,	
	.throttle        = ircomm_tty_throttle,
	.unthrottle      = ircomm_tty_unthrottle,
	.send_xchar      = ircomm_tty_send_xchar,
	.set_termios     = ircomm_tty_set_termios,
	.stop            = ircomm_tty_stop,
	.start           = ircomm_tty_start,
	.hangup          = ircomm_tty_hangup,
	.wait_until_sent = ircomm_tty_wait_until_sent,
#ifdef CONFIG_PROC_FS
	.proc_fops       = &ircomm_tty_proc_fops,
#endif 
};

static int __init ircomm_tty_init(void)
{
	driver = alloc_tty_driver(IRCOMM_TTY_PORTS);
	if (!driver)
		return -ENOMEM;
	ircomm_tty = hashbin_new(HB_LOCK);
	if (ircomm_tty == NULL) {
		IRDA_ERROR("%s(), can't allocate hashbin!\n", __func__);
		put_tty_driver(driver);
		return -ENOMEM;
	}

	driver->driver_name     = "ircomm";
	driver->name            = "ircomm";
	driver->major           = IRCOMM_TTY_MAJOR;
	driver->minor_start     = IRCOMM_TTY_MINOR;
	driver->type            = TTY_DRIVER_TYPE_SERIAL;
	driver->subtype         = SERIAL_TYPE_NORMAL;
	driver->init_termios    = tty_std_termios;
	driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	driver->flags           = TTY_DRIVER_REAL_RAW;
	tty_set_operations(driver, &ops);
	if (tty_register_driver(driver)) {
		IRDA_ERROR("%s(): Couldn't register serial driver\n",
			   __func__);
		put_tty_driver(driver);
		return -1;
	}
	return 0;
}

static void __exit __ircomm_tty_cleanup(struct ircomm_tty_cb *self)
{
	IRDA_DEBUG(0, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	ircomm_tty_shutdown(self);

	self->magic = 0;
	kfree(self);
}

static void __exit ircomm_tty_cleanup(void)
{
	int ret;

	IRDA_DEBUG(4, "%s()\n", __func__ );

	ret = tty_unregister_driver(driver);
	if (ret) {
		IRDA_ERROR("%s(), failed to unregister driver\n",
			   __func__);
		return;
	}

	hashbin_delete(ircomm_tty, (FREE_FUNC) __ircomm_tty_cleanup);
	put_tty_driver(driver);
}

static int ircomm_tty_startup(struct ircomm_tty_cb *self)
{
	notify_t notify;
	int ret = -ENODEV;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return -1;);

	
	if (test_and_set_bit(ASYNC_B_INITIALIZED, &self->flags)) {
		IRDA_DEBUG(2, "%s(), already open so break out!\n", __func__ );
		return 0;
	}

	
	irda_notify_init(&notify);
	
	notify.data_indication       = ircomm_tty_data_indication;
	notify.udata_indication      = ircomm_tty_control_indication;
	notify.flow_indication       = ircomm_tty_flow_indication;

	
	notify.disconnect_indication = ircomm_tty_disconnect_indication;
	notify.connect_confirm       = ircomm_tty_connect_confirm;
	notify.connect_indication    = ircomm_tty_connect_indication;
	strlcpy(notify.name, "ircomm_tty", sizeof(notify.name));
	notify.instance = self;

	if (!self->ircomm) {
		self->ircomm = ircomm_open(&notify, self->service_type,
					   self->line);
	}
	if (!self->ircomm)
		goto err;

	self->slsap_sel = self->ircomm->slsap_sel;

	
	ret = ircomm_tty_attach_cable(self);
	if (ret < 0) {
		IRDA_ERROR("%s(), error attaching cable!\n", __func__);
		goto err;
	}

	return 0;
err:
	clear_bit(ASYNC_B_INITIALIZED, &self->flags);
	return ret;
}

static int ircomm_tty_block_til_ready(struct ircomm_tty_cb *self,
				      struct file *filp)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	int		do_clocal = 0, extra_count = 0;
	unsigned long	flags;
	struct tty_struct *tty;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	tty = self->tty;

	if (filp->f_flags & O_NONBLOCK || tty->flags & (1 << TTY_IO_ERROR)){
		
		self->flags |= ASYNC_NORMAL_ACTIVE;
		IRDA_DEBUG(1, "%s(), O_NONBLOCK requested!\n", __func__ );
		return 0;
	}

	if (tty->termios->c_cflag & CLOCAL) {
		IRDA_DEBUG(1, "%s(), doing CLOCAL!\n", __func__ );
		do_clocal = 1;
	}


	retval = 0;
	add_wait_queue(&self->open_wait, &wait);

	IRDA_DEBUG(2, "%s(%d):block_til_ready before block on %s open_count=%d\n",
	      __FILE__,__LINE__, tty->driver->name, self->open_count );

	
	spin_lock_irqsave(&self->spinlock, flags);
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		self->open_count--;
	}
	spin_unlock_irqrestore(&self->spinlock, flags);
	self->blocked_open++;

	while (1) {
		if (tty->termios->c_cflag & CBAUD) {
			self->settings.dte |= IRCOMM_RTS + IRCOMM_DTR;

			ircomm_param_request(self, IRCOMM_DTE, TRUE);
		}

		current->state = TASK_INTERRUPTIBLE;

		if (tty_hung_up_p(filp) ||
		    !test_bit(ASYNC_B_INITIALIZED, &self->flags)) {
			retval = (self->flags & ASYNC_HUP_NOTIFY) ?
					-EAGAIN : -ERESTARTSYS;
			break;
		}

		if (!test_bit(ASYNC_B_CLOSING, &self->flags) &&
		    (do_clocal || (self->settings.dce & IRCOMM_CD)) &&
		    self->state == IRCOMM_TTY_READY)
		{
			break;
		}

		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}

		IRDA_DEBUG(1, "%s(%d):block_til_ready blocking on %s open_count=%d\n",
		      __FILE__,__LINE__, tty->driver->name, self->open_count );

		schedule();
	}

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&self->open_wait, &wait);

	if (extra_count) {
		
		spin_lock_irqsave(&self->spinlock, flags);
		self->open_count++;
		spin_unlock_irqrestore(&self->spinlock, flags);
	}
	self->blocked_open--;

	IRDA_DEBUG(1, "%s(%d):block_til_ready after blocking on %s open_count=%d\n",
	      __FILE__,__LINE__, tty->driver->name, self->open_count);

	if (!retval)
		self->flags |= ASYNC_NORMAL_ACTIVE;

	return retval;
}

static int ircomm_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct ircomm_tty_cb *self;
	unsigned int line = tty->index;
	unsigned long	flags;
	int ret;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	
	self = hashbin_lock_find(ircomm_tty, line, NULL);
	if (!self) {
		
		self = kzalloc(sizeof(struct ircomm_tty_cb), GFP_KERNEL);
		if (self == NULL) {
			IRDA_ERROR("%s(), kmalloc failed!\n", __func__);
			return -ENOMEM;
		}

		self->magic = IRCOMM_TTY_MAGIC;
		self->flow = FLOW_STOP;

		self->line = line;
		INIT_WORK(&self->tqueue, ircomm_tty_do_softint);
		self->max_header_size = IRCOMM_TTY_HDR_UNINITIALISED;
		self->max_data_size = IRCOMM_TTY_DATA_UNINITIALISED;
		self->close_delay = 5*HZ/10;
		self->closing_wait = 30*HZ;

		
		init_timer(&self->watchdog_timer);
		init_waitqueue_head(&self->open_wait);
		init_waitqueue_head(&self->close_wait);
		spin_lock_init(&self->spinlock);

		tty->termios->c_iflag = 0;
		tty->termios->c_oflag = 0;

		
		hashbin_insert(ircomm_tty, (irda_queue_t *) self, line, NULL);
	}
	
	spin_lock_irqsave(&self->spinlock, flags);
	self->open_count++;

	tty->driver_data = self;
	self->tty = tty;
	spin_unlock_irqrestore(&self->spinlock, flags);

	IRDA_DEBUG(1, "%s(), %s%d, count = %d\n", __func__ , tty->driver->name,
		   self->line, self->open_count);

	
	self->tty->low_latency = (self->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	if (tty_hung_up_p(filp) ||
	    test_bit(ASYNC_B_CLOSING, &self->flags)) {


		if (wait_event_interruptible(self->close_wait, !test_bit(ASYNC_B_CLOSING, &self->flags))) {
			IRDA_WARNING("%s - got signal while blocking on ASYNC_CLOSING!\n",
				     __func__);
			return -ERESTARTSYS;
		}

#ifdef SERIAL_DO_RESTART
		return (self->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS;
#else
		return -EAGAIN;
#endif
	}

	
	if (line < 0x10) {
		self->service_type = IRCOMM_3_WIRE | IRCOMM_9_WIRE;
		self->settings.service_type = IRCOMM_9_WIRE; 
		
		self->settings.dce = IRCOMM_CTS | IRCOMM_CD | IRCOMM_DSR | IRCOMM_RI; 
		IRDA_DEBUG(2, "%s(), IrCOMM device\n", __func__ );
	} else {
		IRDA_DEBUG(2, "%s(), IrLPT device\n", __func__ );
		self->service_type = IRCOMM_3_WIRE_RAW;
		self->settings.service_type = IRCOMM_3_WIRE_RAW; 
	}

	ret = ircomm_tty_startup(self);
	if (ret)
		return ret;

	ret = ircomm_tty_block_til_ready(self, filp);
	if (ret) {
		IRDA_DEBUG(2,
		      "%s(), returning after block_til_ready with %d\n", __func__ ,
		      ret);

		return ret;
	}
	return 0;
}

static void ircomm_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;
	unsigned long flags;

	IRDA_DEBUG(0, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	spin_lock_irqsave(&self->spinlock, flags);

	if (tty_hung_up_p(filp)) {
		spin_unlock_irqrestore(&self->spinlock, flags);

		IRDA_DEBUG(0, "%s(), returning 1\n", __func__ );
		return;
	}

	if ((tty->count == 1) && (self->open_count != 1)) {
		IRDA_DEBUG(0, "%s(), bad serial port count; "
			   "tty->count is 1, state->count is %d\n", __func__ ,
			   self->open_count);
		self->open_count = 1;
	}

	if (--self->open_count < 0) {
		IRDA_ERROR("%s(), bad serial port count for ttys%d: %d\n",
			   __func__, self->line, self->open_count);
		self->open_count = 0;
	}
	if (self->open_count) {
		spin_unlock_irqrestore(&self->spinlock, flags);

		IRDA_DEBUG(0, "%s(), open count > 0\n", __func__ );
		return;
	}

	
	set_bit(ASYNC_B_CLOSING, &self->flags);

	spin_unlock_irqrestore(&self->spinlock, flags);

	tty->closing = 1;
	if (self->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent_from_close(tty, self->closing_wait);

	ircomm_tty_shutdown(self);

	tty_driver_flush_buffer(tty);
	tty_ldisc_flush(tty);

	tty->closing = 0;
	self->tty = NULL;

	if (self->blocked_open) {
		if (self->close_delay)
			schedule_timeout_interruptible(self->close_delay);
		wake_up_interruptible(&self->open_wait);
	}

	self->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CLOSING);
	wake_up_interruptible(&self->close_wait);
}

static void ircomm_tty_flush_buffer(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	schedule_work(&self->tqueue);
}

static void ircomm_tty_do_softint(struct work_struct *work)
{
	struct ircomm_tty_cb *self =
		container_of(work, struct ircomm_tty_cb, tqueue);
	struct tty_struct *tty;
	unsigned long flags;
	struct sk_buff *skb, *ctrl_skb;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	if (!self || self->magic != IRCOMM_TTY_MAGIC)
		return;

	tty = self->tty;
	if (!tty)
		return;

	
	spin_lock_irqsave(&self->spinlock, flags);

	ctrl_skb = self->ctrl_skb;
	self->ctrl_skb = NULL;

	spin_unlock_irqrestore(&self->spinlock, flags);

	
	if(ctrl_skb) {
		if(self->flow == FLOW_START)
			ircomm_control_request(self->ircomm, ctrl_skb);
		
		dev_kfree_skb(ctrl_skb);
	}

	if (tty->hw_stopped)
		return;

	
	spin_lock_irqsave(&self->spinlock, flags);

	skb = self->tx_skb;
	self->tx_skb = NULL;

	spin_unlock_irqrestore(&self->spinlock, flags);

	
	if (skb) {
		ircomm_tty_do_event(self, IRCOMM_TTY_DATA_REQUEST, skb, NULL);
		
		dev_kfree_skb(skb);
	}

	
	tty_wakeup(tty);
}

static int ircomm_tty_write(struct tty_struct *tty,
			    const unsigned char *buf, int count)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;
	unsigned long flags;
	struct sk_buff *skb;
	int tailroom = 0;
	int len = 0;
	int size;

	IRDA_DEBUG(2, "%s(), count=%d, hw_stopped=%d\n", __func__ , count,
		   tty->hw_stopped);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return -1;);

	if (self->max_header_size == IRCOMM_TTY_HDR_UNINITIALISED) {
		IRDA_DEBUG(1, "%s() : not initialised\n", __func__);
#ifdef IRCOMM_NO_TX_BEFORE_INIT
		
		return 0;
#endif
	}

	if (count < 1)
		return 0;

	
	spin_lock_irqsave(&self->spinlock, flags);

	
	skb = self->tx_skb;

	while (count) {
		size = count;

		
		if (size > self->max_data_size)
			size = self->max_data_size;

		if (skb) {
			if ((tailroom = (self->tx_data_size - skb->len)) > 0) {
				
				if (size > tailroom)
					size = tailroom;
			} else {
				break;
			}
		} else {
			
			skb = alloc_skb(self->max_data_size+
					self->max_header_size,
					GFP_ATOMIC);
			if (!skb) {
				spin_unlock_irqrestore(&self->spinlock, flags);
				return -ENOBUFS;
			}
			skb_reserve(skb, self->max_header_size);
			self->tx_skb = skb;
			self->tx_data_size = self->max_data_size;
		}

		
		memcpy(skb_put(skb,size), buf + len, size);

		count -= size;
		len += size;
	}

	spin_unlock_irqrestore(&self->spinlock, flags);

	schedule_work(&self->tqueue);

	return len;
}

/*
 * Function ircomm_tty_write_room (tty)
 *
 *    This routine returns the numbers of characters the tty driver will
 *    accept for queuing to be written. This number is subject to change as
 *    output buffers get emptied, or if the output flow control is acted.
 */
static int ircomm_tty_write_room(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;
	unsigned long flags;
	int ret;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return -1;);

#ifdef IRCOMM_NO_TX_BEFORE_INIT
	
	if (self->max_header_size == IRCOMM_TTY_HDR_UNINITIALISED)
		
		return 0;
#endif

	if (tty->hw_stopped)
		ret = 0;
	else {
		spin_lock_irqsave(&self->spinlock, flags);
		if (self->tx_skb)
			ret = self->tx_data_size - self->tx_skb->len;
		else
			ret = self->max_data_size;
		spin_unlock_irqrestore(&self->spinlock, flags);
	}
	IRDA_DEBUG(2, "%s(), ret=%d\n", __func__ , ret);

	return ret;
}

/*
 * Function ircomm_tty_wait_until_sent (tty, timeout)
 *
 *    This routine waits until the device has written out all of the
 *    characters in its transmitter FIFO.
 */
static void ircomm_tty_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;
	unsigned long orig_jiffies, poll_time;
	unsigned long flags;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	orig_jiffies = jiffies;

	
	poll_time = IRDA_MIN(timeout, msecs_to_jiffies(200));

	spin_lock_irqsave(&self->spinlock, flags);
	while (self->tx_skb && self->tx_skb->len) {
		spin_unlock_irqrestore(&self->spinlock, flags);
		schedule_timeout_interruptible(poll_time);
		spin_lock_irqsave(&self->spinlock, flags);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	spin_unlock_irqrestore(&self->spinlock, flags);
	current->state = TASK_RUNNING;
}

static void ircomm_tty_throttle(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	
	if (I_IXOFF(tty))
		ircomm_tty_send_xchar(tty, STOP_CHAR(tty));

	
	if (tty->termios->c_cflag & CRTSCTS) {
		self->settings.dte &= ~IRCOMM_RTS;
		self->settings.dte |= IRCOMM_DELTA_RTS;

		ircomm_param_request(self, IRCOMM_DTE, TRUE);
	}

	ircomm_flow_request(self->ircomm, FLOW_STOP);
}

static void ircomm_tty_unthrottle(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	
	if (I_IXOFF(tty)) {
		ircomm_tty_send_xchar(tty, START_CHAR(tty));
	}

	
	if (tty->termios->c_cflag & CRTSCTS) {
		self->settings.dte |= (IRCOMM_RTS|IRCOMM_DELTA_RTS);

		ircomm_param_request(self, IRCOMM_DTE, TRUE);
		IRDA_DEBUG(1, "%s(), FLOW_START\n", __func__ );
	}
	ircomm_flow_request(self->ircomm, FLOW_START);
}

static int ircomm_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;
	unsigned long flags;
	int len = 0;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return -1;);

	spin_lock_irqsave(&self->spinlock, flags);

	if (self->tx_skb)
		len = self->tx_skb->len;

	spin_unlock_irqrestore(&self->spinlock, flags);

	return len;
}

static void ircomm_tty_shutdown(struct ircomm_tty_cb *self)
{
	unsigned long flags;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	IRDA_DEBUG(0, "%s()\n", __func__ );

	if (!test_and_clear_bit(ASYNC_B_INITIALIZED, &self->flags))
		return;

	ircomm_tty_detach_cable(self);

	spin_lock_irqsave(&self->spinlock, flags);

	del_timer(&self->watchdog_timer);

	
	if (self->ctrl_skb) {
		dev_kfree_skb(self->ctrl_skb);
		self->ctrl_skb = NULL;
	}

	
	if (self->tx_skb) {
		dev_kfree_skb(self->tx_skb);
		self->tx_skb = NULL;
	}

	if (self->ircomm) {
		ircomm_close(self->ircomm);
		self->ircomm = NULL;
	}

	spin_unlock_irqrestore(&self->spinlock, flags);
}

static void ircomm_tty_hangup(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;
	unsigned long	flags;

	IRDA_DEBUG(0, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	
	ircomm_tty_shutdown(self);

	
	spin_lock_irqsave(&self->spinlock, flags);
	self->flags &= ~ASYNC_NORMAL_ACTIVE;
	self->tty = NULL;
	self->open_count = 0;
	spin_unlock_irqrestore(&self->spinlock, flags);

	wake_up_interruptible(&self->open_wait);
}

static void ircomm_tty_send_xchar(struct tty_struct *tty, char ch)
{
	IRDA_DEBUG(0, "%s(), not impl\n", __func__ );
}

void ircomm_tty_start(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;

	ircomm_flow_request(self->ircomm, FLOW_START);
}

static void ircomm_tty_stop(struct tty_struct *tty)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) tty->driver_data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	ircomm_flow_request(self->ircomm, FLOW_STOP);
}

void ircomm_tty_check_modem_status(struct ircomm_tty_cb *self)
{
	struct tty_struct *tty;
	int status;

	IRDA_DEBUG(0, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	tty = self->tty;

	status = self->settings.dce;

	if (status & IRCOMM_DCE_DELTA_ANY) {
		
	}
	if ((self->flags & ASYNC_CHECK_CD) && (status & IRCOMM_DELTA_CD)) {
		IRDA_DEBUG(2,
			   "%s(), ircomm%d CD now %s...\n", __func__ , self->line,
			   (status & IRCOMM_CD) ? "on" : "off");

		if (status & IRCOMM_CD) {
			wake_up_interruptible(&self->open_wait);
		} else {
			IRDA_DEBUG(2,
				   "%s(), Doing serial hangup..\n", __func__ );
			if (tty)
				tty_hangup(tty);

			
			return;
		}
	}
	if (self->flags & ASYNC_CTS_FLOW) {
		if (tty->hw_stopped) {
			if (status & IRCOMM_CTS) {
				IRDA_DEBUG(2,
					   "%s(), CTS tx start...\n", __func__ );
				tty->hw_stopped = 0;

				
				wake_up_interruptible(&self->open_wait);

				schedule_work(&self->tqueue);
				return;
			}
		} else {
			if (!(status & IRCOMM_CTS)) {
				IRDA_DEBUG(2,
					   "%s(), CTS tx stop...\n", __func__ );
				tty->hw_stopped = 1;
			}
		}
	}
}

static int ircomm_tty_data_indication(void *instance, void *sap,
				      struct sk_buff *skb)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) instance;

	IRDA_DEBUG(2, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return -1;);
	IRDA_ASSERT(skb != NULL, return -1;);

	if (!self->tty) {
		IRDA_DEBUG(0, "%s(), no tty!\n", __func__ );
		return 0;
	}

	if (self->tty->hw_stopped && (self->flow == FLOW_START)) {
		IRDA_DEBUG(0, "%s(), polling for line settings!\n", __func__ );
		ircomm_param_request(self, IRCOMM_POLL, TRUE);

		
		ircomm_tty_send_initial_parameters(self);
		ircomm_tty_link_established(self);
	}

	tty_insert_flip_string(self->tty, skb->data, skb->len);
	tty_flip_buffer_push(self->tty);

	

	return 0;
}

static int ircomm_tty_control_indication(void *instance, void *sap,
					 struct sk_buff *skb)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) instance;
	int clen;

	IRDA_DEBUG(4, "%s()\n", __func__ );

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return -1;);
	IRDA_ASSERT(skb != NULL, return -1;);

	clen = skb->data[0];

	irda_param_extract_all(self, skb->data+1, IRDA_MIN(skb->len-1, clen),
			       &ircomm_param_info);

	

	return 0;
}

static void ircomm_tty_flow_indication(void *instance, void *sap,
				       LOCAL_FLOW cmd)
{
	struct ircomm_tty_cb *self = (struct ircomm_tty_cb *) instance;
	struct tty_struct *tty;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == IRCOMM_TTY_MAGIC, return;);

	tty = self->tty;

	switch (cmd) {
	case FLOW_START:
		IRDA_DEBUG(2, "%s(), hw start!\n", __func__ );
		tty->hw_stopped = 0;

		
		schedule_work(&self->tqueue);
		break;
	default:  
	case FLOW_STOP:
		IRDA_DEBUG(2, "%s(), hw stopped!\n", __func__ );
		tty->hw_stopped = 1;
		break;
	}
	self->flow = cmd;
}

#ifdef CONFIG_PROC_FS
static void ircomm_tty_line_info(struct ircomm_tty_cb *self, struct seq_file *m)
{
	char sep;

	seq_printf(m, "State: %s\n", ircomm_tty_state[self->state]);

	seq_puts(m, "Service type: ");
	if (self->service_type & IRCOMM_9_WIRE)
		seq_puts(m, "9_WIRE");
	else if (self->service_type & IRCOMM_3_WIRE)
		seq_puts(m, "3_WIRE");
	else if (self->service_type & IRCOMM_3_WIRE_RAW)
		seq_puts(m, "3_WIRE_RAW");
	else
		seq_puts(m, "No common service type!\n");
	seq_putc(m, '\n');

	seq_printf(m, "Port name: %s\n", self->settings.port_name);

	seq_printf(m, "DTE status:");
	sep = ' ';
	if (self->settings.dte & IRCOMM_RTS) {
		seq_printf(m, "%cRTS", sep);
		sep = '|';
	}
	if (self->settings.dte & IRCOMM_DTR) {
		seq_printf(m, "%cDTR", sep);
		sep = '|';
	}
	seq_putc(m, '\n');

	seq_puts(m, "DCE status:");
	sep = ' ';
	if (self->settings.dce & IRCOMM_CTS) {
		seq_printf(m, "%cCTS", sep);
		sep = '|';
	}
	if (self->settings.dce & IRCOMM_DSR) {
		seq_printf(m, "%cDSR", sep);
		sep = '|';
	}
	if (self->settings.dce & IRCOMM_CD) {
		seq_printf(m, "%cCD", sep);
		sep = '|';
	}
	if (self->settings.dce & IRCOMM_RI) {
		seq_printf(m, "%cRI", sep);
		sep = '|';
	}
	seq_putc(m, '\n');

	seq_puts(m, "Configuration: ");
	if (!self->settings.null_modem)
		seq_puts(m, "DTE <-> DCE\n");
	else
		seq_puts(m, "DTE <-> DTE (null modem emulation)\n");

	seq_printf(m, "Data rate: %d\n", self->settings.data_rate);

	seq_puts(m, "Flow control:");
	sep = ' ';
	if (self->settings.flow_control & IRCOMM_XON_XOFF_IN) {
		seq_printf(m, "%cXON_XOFF_IN", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_XON_XOFF_OUT) {
		seq_printf(m, "%cXON_XOFF_OUT", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_RTS_CTS_IN) {
		seq_printf(m, "%cRTS_CTS_IN", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_RTS_CTS_OUT) {
		seq_printf(m, "%cRTS_CTS_OUT", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_DSR_DTR_IN) {
		seq_printf(m, "%cDSR_DTR_IN", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_DSR_DTR_OUT) {
		seq_printf(m, "%cDSR_DTR_OUT", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_ENQ_ACK_IN) {
		seq_printf(m, "%cENQ_ACK_IN", sep);
		sep = '|';
	}
	if (self->settings.flow_control & IRCOMM_ENQ_ACK_OUT) {
		seq_printf(m, "%cENQ_ACK_OUT", sep);
		sep = '|';
	}
	seq_putc(m, '\n');

	seq_puts(m, "Flags:");
	sep = ' ';
	if (self->flags & ASYNC_CTS_FLOW) {
		seq_printf(m, "%cASYNC_CTS_FLOW", sep);
		sep = '|';
	}
	if (self->flags & ASYNC_CHECK_CD) {
		seq_printf(m, "%cASYNC_CHECK_CD", sep);
		sep = '|';
	}
	if (self->flags & ASYNC_INITIALIZED) {
		seq_printf(m, "%cASYNC_INITIALIZED", sep);
		sep = '|';
	}
	if (self->flags & ASYNC_LOW_LATENCY) {
		seq_printf(m, "%cASYNC_LOW_LATENCY", sep);
		sep = '|';
	}
	if (self->flags & ASYNC_CLOSING) {
		seq_printf(m, "%cASYNC_CLOSING", sep);
		sep = '|';
	}
	if (self->flags & ASYNC_NORMAL_ACTIVE) {
		seq_printf(m, "%cASYNC_NORMAL_ACTIVE", sep);
		sep = '|';
	}
	seq_putc(m, '\n');

	seq_printf(m, "Role: %s\n", self->client ? "client" : "server");
	seq_printf(m, "Open count: %d\n", self->open_count);
	seq_printf(m, "Max data size: %d\n", self->max_data_size);
	seq_printf(m, "Max header size: %d\n", self->max_header_size);

	if (self->tty)
		seq_printf(m, "Hardware: %s\n",
			       self->tty->hw_stopped ? "Stopped" : "Running");
}

static int ircomm_tty_proc_show(struct seq_file *m, void *v)
{
	struct ircomm_tty_cb *self;
	unsigned long flags;

	spin_lock_irqsave(&ircomm_tty->hb_spinlock, flags);

	self = (struct ircomm_tty_cb *) hashbin_get_first(ircomm_tty);
	while (self != NULL) {
		if (self->magic != IRCOMM_TTY_MAGIC)
			break;

		ircomm_tty_line_info(self, m);
		self = (struct ircomm_tty_cb *) hashbin_get_next(ircomm_tty);
	}
	spin_unlock_irqrestore(&ircomm_tty->hb_spinlock, flags);
	return 0;
}

static int ircomm_tty_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ircomm_tty_proc_show, NULL);
}

static const struct file_operations ircomm_tty_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= ircomm_tty_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif 

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("IrCOMM serial TTY driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(IRCOMM_TTY_MAJOR);

module_init(ircomm_tty_init);
module_exit(ircomm_tty_cleanup);
