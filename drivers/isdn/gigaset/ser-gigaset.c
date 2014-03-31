/* This is the serial hardware link layer (HLL) for the Gigaset 307x isdn
 * DECT base (aka Sinus 45 isdn) using the RS232 DECT data module M101,
 * written as a line discipline.
 *
 * =====================================================================
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * =====================================================================
 */

#include "gigaset.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/completion.h>

#define DRIVER_AUTHOR "Tilman Schmidt"
#define DRIVER_DESC "Serial Driver for Gigaset 307x using Siemens M101"

#define GIGASET_MINORS     1
#define GIGASET_MINOR      0
#define GIGASET_MODULENAME "ser_gigaset"
#define GIGASET_DEVNAME    "ttyGS"

#define IF_WRITEBUF 264

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_GIGASET_M101);

static int startmode = SM_ISDN;
module_param(startmode, int, S_IRUGO);
MODULE_PARM_DESC(startmode, "initial operation mode");
static int cidmode = 1;
module_param(cidmode, int, S_IRUGO);
MODULE_PARM_DESC(cidmode, "stay in CID mode when idle");

static struct gigaset_driver *driver;

struct ser_cardstate {
	struct platform_device	dev;
	struct tty_struct	*tty;
	atomic_t		refcnt;
	struct completion	dead_cmp;
};

static struct platform_driver device_driver = {
	.driver = {
		.name = GIGASET_MODULENAME,
	},
};

static void flush_send_queue(struct cardstate *);

static int write_modem(struct cardstate *cs)
{
	struct tty_struct *tty = cs->hw.ser->tty;
	struct bc_state *bcs = &cs->bcs[0];	
	struct sk_buff *skb = bcs->tx_skb;
	int sent = -EOPNOTSUPP;

	if (!tty || !tty->driver || !skb)
		return -EINVAL;

	if (!skb->len) {
		dev_kfree_skb_any(skb);
		bcs->tx_skb = NULL;
		return -EINVAL;
	}

	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	if (tty->ops->write)
		sent = tty->ops->write(tty, skb->data, skb->len);
	gig_dbg(DEBUG_OUTPUT, "write_modem: sent %d", sent);
	if (sent < 0) {
		
		flush_send_queue(cs);
		return sent;
	}
	skb_pull(skb, sent);
	if (!skb->len) {
		
		gigaset_skb_sent(bcs, skb);

		gig_dbg(DEBUG_INTR, "kfree skb (Adr: %lx)!",
			(unsigned long) skb);
		dev_kfree_skb_any(skb);
		bcs->tx_skb = NULL;
	}
	return sent;
}

static int send_cb(struct cardstate *cs)
{
	struct tty_struct *tty = cs->hw.ser->tty;
	struct cmdbuf_t *cb, *tcb;
	unsigned long flags;
	int sent = 0;

	if (!tty || !tty->driver)
		return -EFAULT;

	cb = cs->cmdbuf;
	if (!cb)
		return 0;	

	if (cb->len) {
		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		sent = tty->ops->write(tty, cb->buf + cb->offset, cb->len);
		if (sent < 0) {
			
			gig_dbg(DEBUG_OUTPUT, "send_cb: write error %d", sent);
			flush_send_queue(cs);
			return sent;
		}
		cb->offset += sent;
		cb->len -= sent;
		gig_dbg(DEBUG_OUTPUT, "send_cb: sent %d, left %u, queued %u",
			sent, cb->len, cs->cmdbytes);
	}

	while (cb && !cb->len) {
		spin_lock_irqsave(&cs->cmdlock, flags);
		cs->cmdbytes -= cs->curlen;
		tcb = cb;
		cs->cmdbuf = cb = cb->next;
		if (cb) {
			cb->prev = NULL;
			cs->curlen = cb->len;
		} else {
			cs->lastcmdbuf = NULL;
			cs->curlen = 0;
		}
		spin_unlock_irqrestore(&cs->cmdlock, flags);

		if (tcb->wake_tasklet)
			tasklet_schedule(tcb->wake_tasklet);
		kfree(tcb);
	}
	return sent;
}

static void gigaset_modem_fill(unsigned long data)
{
	struct cardstate *cs = (struct cardstate *) data;
	struct bc_state *bcs;
	struct sk_buff *nextskb;
	int sent = 0;

	if (!cs) {
		gig_dbg(DEBUG_OUTPUT, "%s: no cardstate", __func__);
		return;
	}
	bcs = cs->bcs;
	if (!bcs) {
		gig_dbg(DEBUG_OUTPUT, "%s: no cardstate", __func__);
		return;
	}
	if (!bcs->tx_skb) {
		
		sent = send_cb(cs);
		gig_dbg(DEBUG_OUTPUT, "%s: send_cb -> %d", __func__, sent);
		if (sent)
			
			return;

		
		nextskb = skb_dequeue(&bcs->squeue);
		if (!nextskb)
			
			return;
		bcs->tx_skb = nextskb;

		gig_dbg(DEBUG_INTR, "Dequeued skb (Adr: %lx)",
			(unsigned long) bcs->tx_skb);
	}

	
	gig_dbg(DEBUG_OUTPUT, "%s: tx_skb", __func__);
	if (write_modem(cs) < 0)
		gig_dbg(DEBUG_OUTPUT, "%s: write_modem failed", __func__);
}

static void flush_send_queue(struct cardstate *cs)
{
	struct sk_buff *skb;
	struct cmdbuf_t *cb;
	unsigned long flags;

	
	spin_lock_irqsave(&cs->cmdlock, flags);
	while ((cb = cs->cmdbuf) != NULL) {
		cs->cmdbuf = cb->next;
		if (cb->wake_tasklet)
			tasklet_schedule(cb->wake_tasklet);
		kfree(cb);
	}
	cs->cmdbuf = cs->lastcmdbuf = NULL;
	cs->cmdbytes = cs->curlen = 0;
	spin_unlock_irqrestore(&cs->cmdlock, flags);

	
	if (cs->bcs->tx_skb)
		dev_kfree_skb_any(cs->bcs->tx_skb);
	while ((skb = skb_dequeue(&cs->bcs->squeue)) != NULL)
		dev_kfree_skb_any(skb);
}



static int gigaset_write_cmd(struct cardstate *cs, struct cmdbuf_t *cb)
{
	unsigned long flags;

	gigaset_dbg_buffer(cs->mstate != MS_LOCKED ?
			   DEBUG_TRANSCMD : DEBUG_LOCKCMD,
			   "CMD Transmit", cb->len, cb->buf);

	spin_lock_irqsave(&cs->cmdlock, flags);
	cb->prev = cs->lastcmdbuf;
	if (cs->lastcmdbuf)
		cs->lastcmdbuf->next = cb;
	else {
		cs->cmdbuf = cb;
		cs->curlen = cb->len;
	}
	cs->cmdbytes += cb->len;
	cs->lastcmdbuf = cb;
	spin_unlock_irqrestore(&cs->cmdlock, flags);

	spin_lock_irqsave(&cs->lock, flags);
	if (cs->connected)
		tasklet_schedule(&cs->write_tasklet);
	spin_unlock_irqrestore(&cs->lock, flags);
	return cb->len;
}

/*
 * tty_driver.write_room interface routine
 * return number of characters the driver will accept to be written
 * parameter:
 *	controller state structure
 * return value:
 *	number of characters
 */
static int gigaset_write_room(struct cardstate *cs)
{
	unsigned bytes;

	bytes = cs->cmdbytes;
	return bytes < IF_WRITEBUF ? IF_WRITEBUF - bytes : 0;
}

static int gigaset_chars_in_buffer(struct cardstate *cs)
{
	return cs->cmdbytes;
}

static int gigaset_brkchars(struct cardstate *cs, const unsigned char buf[6])
{
	
	return -EINVAL;
}

static int gigaset_init_bchannel(struct bc_state *bcs)
{
	
	gigaset_bchannel_up(bcs);
	return 0;
}

static int gigaset_close_bchannel(struct bc_state *bcs)
{
	
	gigaset_bchannel_down(bcs);
	return 0;
}

static int gigaset_initbcshw(struct bc_state *bcs)
{
	
	bcs->hw.ser = NULL;
	return 1;
}

static int gigaset_freebcshw(struct bc_state *bcs)
{
	
	return 1;
}

static void gigaset_reinitbcshw(struct bc_state *bcs)
{
	
}

static void gigaset_freecshw(struct cardstate *cs)
{
	tasklet_kill(&cs->write_tasklet);
	if (!cs->hw.ser)
		return;
	dev_set_drvdata(&cs->hw.ser->dev.dev, NULL);
	platform_device_unregister(&cs->hw.ser->dev);
	kfree(cs->hw.ser);
	cs->hw.ser = NULL;
}

static void gigaset_device_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	
	kfree(dev->platform_data);
	kfree(pdev->resource);
}

static int gigaset_initcshw(struct cardstate *cs)
{
	int rc;
	struct ser_cardstate *scs;

	scs = kzalloc(sizeof(struct ser_cardstate), GFP_KERNEL);
	if (!scs) {
		pr_err("out of memory\n");
		return 0;
	}
	cs->hw.ser = scs;

	cs->hw.ser->dev.name = GIGASET_MODULENAME;
	cs->hw.ser->dev.id = cs->minor_index;
	cs->hw.ser->dev.dev.release = gigaset_device_release;
	rc = platform_device_register(&cs->hw.ser->dev);
	if (rc != 0) {
		pr_err("error %d registering platform device\n", rc);
		kfree(cs->hw.ser);
		cs->hw.ser = NULL;
		return 0;
	}
	dev_set_drvdata(&cs->hw.ser->dev.dev, cs);

	tasklet_init(&cs->write_tasklet,
		     gigaset_modem_fill, (unsigned long) cs);
	return 1;
}

static int gigaset_set_modem_ctrl(struct cardstate *cs, unsigned old_state,
				  unsigned new_state)
{
	struct tty_struct *tty = cs->hw.ser->tty;
	unsigned int set, clear;

	if (!tty || !tty->driver || !tty->ops->tiocmset)
		return -EINVAL;
	set = new_state & ~old_state;
	clear = old_state & ~new_state;
	if (!set && !clear)
		return 0;
	gig_dbg(DEBUG_IF, "tiocmset set %x clear %x", set, clear);
	return tty->ops->tiocmset(tty, set, clear);
}

static int gigaset_baud_rate(struct cardstate *cs, unsigned cflag)
{
	return -EINVAL;
}

static int gigaset_set_line_ctrl(struct cardstate *cs, unsigned cflag)
{
	return -EINVAL;
}

static const struct gigaset_ops ops = {
	gigaset_write_cmd,
	gigaset_write_room,
	gigaset_chars_in_buffer,
	gigaset_brkchars,
	gigaset_init_bchannel,
	gigaset_close_bchannel,
	gigaset_initbcshw,
	gigaset_freebcshw,
	gigaset_reinitbcshw,
	gigaset_initcshw,
	gigaset_freecshw,
	gigaset_set_modem_ctrl,
	gigaset_baud_rate,
	gigaset_set_line_ctrl,
	gigaset_m10x_send_skb,	
	gigaset_m10x_input,	
};



static struct cardstate *cs_get(struct tty_struct *tty)
{
	struct cardstate *cs = tty->disc_data;

	if (!cs || !cs->hw.ser) {
		gig_dbg(DEBUG_ANY, "%s: no cardstate", __func__);
		return NULL;
	}
	atomic_inc(&cs->hw.ser->refcnt);
	return cs;
}

static void cs_put(struct cardstate *cs)
{
	if (atomic_dec_and_test(&cs->hw.ser->refcnt))
		complete(&cs->hw.ser->dead_cmp);
}

static int
gigaset_tty_open(struct tty_struct *tty)
{
	struct cardstate *cs;

	gig_dbg(DEBUG_INIT, "Starting HLL for Gigaset M101");

	pr_info(DRIVER_DESC "\n");

	if (!driver) {
		pr_err("%s: no driver structure\n", __func__);
		return -ENODEV;
	}

	
	cs = gigaset_initcs(driver, 1, 1, 0, cidmode, GIGASET_MODULENAME);
	if (!cs)
		goto error;

	cs->dev = &cs->hw.ser->dev.dev;
	cs->hw.ser->tty = tty;
	atomic_set(&cs->hw.ser->refcnt, 1);
	init_completion(&cs->hw.ser->dead_cmp);

	tty->disc_data = cs;

	if (startmode == SM_LOCKED)
		cs->mstate = MS_LOCKED;
	if (!gigaset_start(cs)) {
		tasklet_kill(&cs->write_tasklet);
		goto error;
	}

	gig_dbg(DEBUG_INIT, "Startup of HLL done");
	return 0;

error:
	gig_dbg(DEBUG_INIT, "Startup of HLL failed");
	tty->disc_data = NULL;
	gigaset_freecs(cs);
	return -ENODEV;
}

static void
gigaset_tty_close(struct tty_struct *tty)
{
	struct cardstate *cs = tty->disc_data;

	gig_dbg(DEBUG_INIT, "Stopping HLL for Gigaset M101");

	if (!cs) {
		gig_dbg(DEBUG_INIT, "%s: no cardstate", __func__);
		return;
	}

	
	tty->disc_data = NULL;

	if (!cs->hw.ser)
		pr_err("%s: no hw cardstate\n", __func__);
	else {
		
		if (!atomic_dec_and_test(&cs->hw.ser->refcnt))
			wait_for_completion(&cs->hw.ser->dead_cmp);
	}

	
	gigaset_stop(cs);
	tasklet_kill(&cs->write_tasklet);
	flush_send_queue(cs);
	cs->dev = NULL;
	gigaset_freecs(cs);

	gig_dbg(DEBUG_INIT, "Shutdown of HLL done");
}

static int gigaset_tty_hangup(struct tty_struct *tty)
{
	gigaset_tty_close(tty);
	return 0;
}

static ssize_t
gigaset_tty_read(struct tty_struct *tty, struct file *file,
		 unsigned char __user *buf, size_t count)
{
	return -EAGAIN;
}

static ssize_t
gigaset_tty_write(struct tty_struct *tty, struct file *file,
		  const unsigned char *buf, size_t count)
{
	return -EAGAIN;
}

static int
gigaset_tty_ioctl(struct tty_struct *tty, struct file *file,
		  unsigned int cmd, unsigned long arg)
{
	struct cardstate *cs = cs_get(tty);
	int rc, val;
	int __user *p = (int __user *)arg;

	if (!cs)
		return -ENXIO;

	switch (cmd) {

	case FIONREAD:
		
		val = 0;
		rc = put_user(val, p);
		break;

	case TCFLSH:
		
		switch (arg) {
		case TCIFLUSH:
			
			break;
		case TCIOFLUSH:
		case TCOFLUSH:
			flush_send_queue(cs);
			break;
		}
		

	default:
		
		rc = n_tty_ioctl_helper(tty, file, cmd, arg);
		break;
	}
	cs_put(cs);
	return rc;
}

static void
gigaset_tty_receive(struct tty_struct *tty, const unsigned char *buf,
		    char *cflags, int count)
{
	struct cardstate *cs = cs_get(tty);
	unsigned tail, head, n;
	struct inbuf_t *inbuf;

	if (!cs)
		return;
	inbuf = cs->inbuf;
	if (!inbuf) {
		dev_err(cs->dev, "%s: no inbuf\n", __func__);
		cs_put(cs);
		return;
	}

	tail = inbuf->tail;
	head = inbuf->head;
	gig_dbg(DEBUG_INTR, "buffer state: %u -> %u, receive %u bytes",
		head, tail, count);

	if (head <= tail) {
		
		n = min_t(unsigned, count, RBUFSIZE - tail);
		memcpy(inbuf->data + tail, buf, n);
		tail = (tail + n) % RBUFSIZE;
		buf += n;
		count -= n;
	}

	if (count > 0) {
		
		n = head - tail - 1;
		if (count > n) {
			dev_err(cs->dev,
				"inbuf overflow, discarding %d bytes\n",
				count - n);
			count = n;
		}
		memcpy(inbuf->data + tail, buf, count);
		tail += count;
	}

	gig_dbg(DEBUG_INTR, "setting tail to %u", tail);
	inbuf->tail = tail;

	
	gig_dbg(DEBUG_INTR, "%s-->BH", __func__);
	gigaset_schedule_event(cs);
	cs_put(cs);
}

static void
gigaset_tty_wakeup(struct tty_struct *tty)
{
	struct cardstate *cs = cs_get(tty);

	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	if (!cs)
		return;
	tasklet_schedule(&cs->write_tasklet);
	cs_put(cs);
}

static struct tty_ldisc_ops gigaset_ldisc = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= "ser_gigaset",
	.open		= gigaset_tty_open,
	.close		= gigaset_tty_close,
	.hangup		= gigaset_tty_hangup,
	.read		= gigaset_tty_read,
	.write		= gigaset_tty_write,
	.ioctl		= gigaset_tty_ioctl,
	.receive_buf	= gigaset_tty_receive,
	.write_wakeup	= gigaset_tty_wakeup,
};



static int __init ser_gigaset_init(void)
{
	int rc;

	gig_dbg(DEBUG_INIT, "%s", __func__);
	rc = platform_driver_register(&device_driver);
	if (rc != 0) {
		pr_err("error %d registering platform driver\n", rc);
		return rc;
	}

	
	driver = gigaset_initdriver(GIGASET_MINOR, GIGASET_MINORS,
				    GIGASET_MODULENAME, GIGASET_DEVNAME,
				    &ops, THIS_MODULE);
	if (!driver)
		goto error;

	rc = tty_register_ldisc(N_GIGASET_M101, &gigaset_ldisc);
	if (rc != 0) {
		pr_err("error %d registering line discipline\n", rc);
		goto error;
	}

	return 0;

error:
	if (driver) {
		gigaset_freedriver(driver);
		driver = NULL;
	}
	platform_driver_unregister(&device_driver);
	return rc;
}

static void __exit ser_gigaset_exit(void)
{
	int rc;

	gig_dbg(DEBUG_INIT, "%s", __func__);

	if (driver) {
		gigaset_freedriver(driver);
		driver = NULL;
	}

	rc = tty_unregister_ldisc(N_GIGASET_M101);
	if (rc != 0)
		pr_err("error %d unregistering line discipline\n", rc);

	platform_driver_unregister(&device_driver);
}

module_init(ser_gigaset_init);
module_exit(ser_gigaset_exit);
