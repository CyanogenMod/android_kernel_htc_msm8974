/*
 * Copyright (C) 2001 Anton Blanchard <anton@au.ibm.com>, IBM
 * Copyright (C) 2001 Paul Mackerras <paulus@au.ibm.com>, IBM
 * Copyright (C) 2004 Benjamin Herrenschmidt <benh@kernel.crashing.org>, IBM Corp.
 * Copyright (C) 2004 IBM Corporation
 *
 * Additional Author(s):
 *  Ryan S. Arnold <rsa@us.ibm.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/console.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/kbd_kern.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/sysrq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/serial_core.h>

#include <asm/uaccess.h>

#include "hvc_console.h"

#define HVC_MAJOR	229
#define HVC_MINOR	0

#define HVC_CLOSE_WAIT (HZ/100) 

#define N_OUTBUF	16
#define N_INBUF		16

#define __ALIGNED__ __attribute__((__aligned__(sizeof(long))))

static struct tty_driver *hvc_driver;
static struct task_struct *hvc_task;

static int hvc_kicked;

static int hvc_init(void);

#ifdef CONFIG_MAGIC_SYSRQ
static int sysrq_pressed;
#endif

static LIST_HEAD(hvc_structs);

static DEFINE_SPINLOCK(hvc_structs_lock);

static int last_hvc = -1;

static struct hvc_struct *hvc_get_by_index(int index)
{
	struct hvc_struct *hp;
	unsigned long flags;

	spin_lock(&hvc_structs_lock);

	list_for_each_entry(hp, &hvc_structs, next) {
		spin_lock_irqsave(&hp->lock, flags);
		if (hp->index == index) {
			kref_get(&hp->kref);
			spin_unlock_irqrestore(&hp->lock, flags);
			spin_unlock(&hvc_structs_lock);
			return hp;
		}
		spin_unlock_irqrestore(&hp->lock, flags);
	}
	hp = NULL;

	spin_unlock(&hvc_structs_lock);
	return hp;
}


static const struct hv_ops *cons_ops[MAX_NR_HVC_CONSOLES];
static uint32_t vtermnos[MAX_NR_HVC_CONSOLES] =
	{[0 ... MAX_NR_HVC_CONSOLES - 1] = -1};


static void hvc_console_print(struct console *co, const char *b,
			      unsigned count)
{
	char c[N_OUTBUF] __ALIGNED__;
	unsigned i = 0, n = 0;
	int r, donecr = 0, index = co->index;

	
	if (index >= MAX_NR_HVC_CONSOLES)
		return;

	
	if (vtermnos[index] == -1)
		return;

	while (count > 0 || i > 0) {
		if (count > 0 && i < sizeof(c)) {
			if (b[n] == '\n' && !donecr) {
				c[i++] = '\r';
				donecr = 1;
			} else {
				c[i++] = b[n++];
				donecr = 0;
				--count;
			}
		} else {
			r = cons_ops[index]->put_chars(vtermnos[index], c, i);
			if (r <= 0) {
				if (r != -EAGAIN)
					i = 0;
			} else if (r > 0) {
				i -= r;
				if (i > 0)
					memmove(c, c+r, i);
			}
		}
	}
}

static struct tty_driver *hvc_console_device(struct console *c, int *index)
{
	if (vtermnos[c->index] == -1)
		return NULL;

	*index = c->index;
	return hvc_driver;
}

static int __init hvc_console_setup(struct console *co, char *options)
{	
	if (co->index < 0 || co->index >= MAX_NR_HVC_CONSOLES)
		return -ENODEV;

	if (vtermnos[co->index] == -1)
		return -ENODEV;

	return 0;
}

static struct console hvc_console = {
	.name		= "hvc",
	.write		= hvc_console_print,
	.device		= hvc_console_device,
	.setup		= hvc_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

static int __init hvc_console_init(void)
{
	register_console(&hvc_console);
	return 0;
}
console_initcall(hvc_console_init);

static void destroy_hvc_struct(struct kref *kref)
{
	struct hvc_struct *hp = container_of(kref, struct hvc_struct, kref);
	unsigned long flags;

	spin_lock(&hvc_structs_lock);

	spin_lock_irqsave(&hp->lock, flags);
	list_del(&(hp->next));
	spin_unlock_irqrestore(&hp->lock, flags);

	spin_unlock(&hvc_structs_lock);

	kfree(hp);
}

int hvc_instantiate(uint32_t vtermno, int index, const struct hv_ops *ops)
{
	struct hvc_struct *hp;

	if (index < 0 || index >= MAX_NR_HVC_CONSOLES)
		return -1;

	if (vtermnos[index] != -1)
		return -1;

	
	hp = hvc_get_by_index(index);
	if (hp) {
		kref_put(&hp->kref, destroy_hvc_struct);
		return -1;
	}

	vtermnos[index] = vtermno;
	cons_ops[index] = ops;

	
	if (last_hvc < index)
		last_hvc = index;

	if (index == hvc_console.index)
		register_console(&hvc_console);

	return 0;
}
EXPORT_SYMBOL_GPL(hvc_instantiate);

void hvc_kick(void)
{
	hvc_kicked = 1;
	wake_up_process(hvc_task);
}
EXPORT_SYMBOL_GPL(hvc_kick);

static void hvc_unthrottle(struct tty_struct *tty)
{
	hvc_kick();
}

static int hvc_open(struct tty_struct *tty, struct file * filp)
{
	struct hvc_struct *hp;
	unsigned long flags;
	int rc = 0;

	
	if (!(hp = hvc_get_by_index(tty->index)))
		return -ENODEV;

	spin_lock_irqsave(&hp->lock, flags);
	
	if (hp->count++ > 0) {
		tty_kref_get(tty);
		spin_unlock_irqrestore(&hp->lock, flags);
		hvc_kick();
		return 0;
	} 

	tty->driver_data = hp;

	hp->tty = tty_kref_get(tty);

	spin_unlock_irqrestore(&hp->lock, flags);

	if (hp->ops->notifier_add)
		rc = hp->ops->notifier_add(hp, hp->data);

	if (rc) {
		spin_lock_irqsave(&hp->lock, flags);
		hp->tty = NULL;
		spin_unlock_irqrestore(&hp->lock, flags);
		tty_kref_put(tty);
		tty->driver_data = NULL;
		kref_put(&hp->kref, destroy_hvc_struct);
		printk(KERN_ERR "hvc_open: request_irq failed with rc %d.\n", rc);
	}
	
	hvc_kick();

	return rc;
}

static void hvc_close(struct tty_struct *tty, struct file * filp)
{
	struct hvc_struct *hp;
	unsigned long flags;

	if (tty_hung_up_p(filp))
		return;

	if (!tty->driver_data)
		return;

	hp = tty->driver_data;

	spin_lock_irqsave(&hp->lock, flags);

	if (--hp->count == 0) {
		
		hp->tty = NULL;
		spin_unlock_irqrestore(&hp->lock, flags);

		if (hp->ops->notifier_del)
			hp->ops->notifier_del(hp, hp->data);

		
		cancel_work_sync(&hp->tty_resize);

		tty_wait_until_sent_from_close(tty, HVC_CLOSE_WAIT);
	} else {
		if (hp->count < 0)
			printk(KERN_ERR "hvc_close %X: oops, count is %d\n",
				hp->vtermno, hp->count);
		spin_unlock_irqrestore(&hp->lock, flags);
	}

	tty_kref_put(tty);
	kref_put(&hp->kref, destroy_hvc_struct);
}

static void hvc_hangup(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;
	unsigned long flags;
	int temp_open_count;

	if (!hp)
		return;

	
	cancel_work_sync(&hp->tty_resize);

	spin_lock_irqsave(&hp->lock, flags);

	if (hp->count <= 0) {
		spin_unlock_irqrestore(&hp->lock, flags);
		return;
	}

	temp_open_count = hp->count;
	hp->count = 0;
	hp->n_outbuf = 0;
	hp->tty = NULL;

	spin_unlock_irqrestore(&hp->lock, flags);

	if (hp->ops->notifier_hangup)
		hp->ops->notifier_hangup(hp, hp->data);

	while(temp_open_count) {
		--temp_open_count;
		tty_kref_put(tty);
		kref_put(&hp->kref, destroy_hvc_struct);
	}
}

static int hvc_push(struct hvc_struct *hp)
{
	int n;

	n = hp->ops->put_chars(hp->vtermno, hp->outbuf, hp->n_outbuf);
	if (n <= 0) {
		if (n == 0 || n == -EAGAIN) {
			hp->do_wakeup = 1;
			return 0;
		}
		hp->n_outbuf = 0;
	} else
		hp->n_outbuf -= n;
	if (hp->n_outbuf > 0)
		memmove(hp->outbuf, hp->outbuf + n, hp->n_outbuf);
	else
		hp->do_wakeup = 1;

	return n;
}

static int hvc_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct hvc_struct *hp = tty->driver_data;
	unsigned long flags;
	int rsize, written = 0;

	
	if (!hp)
		return -EPIPE;

	if (hp->count <= 0)
		return -EIO;

	spin_lock_irqsave(&hp->lock, flags);

	
	if (hp->n_outbuf > 0)
		hvc_push(hp);

	while (count > 0 && (rsize = hp->outbuf_size - hp->n_outbuf) > 0) {
		if (rsize > count)
			rsize = count;
		memcpy(hp->outbuf + hp->n_outbuf, buf, rsize);
		count -= rsize;
		buf += rsize;
		hp->n_outbuf += rsize;
		written += rsize;
		hvc_push(hp);
	}
	spin_unlock_irqrestore(&hp->lock, flags);

	if (hp->n_outbuf)
		hvc_kick();

	return written;
}

static void hvc_set_winsz(struct work_struct *work)
{
	struct hvc_struct *hp;
	unsigned long hvc_flags;
	struct tty_struct *tty;
	struct winsize ws;

	hp = container_of(work, struct hvc_struct, tty_resize);

	spin_lock_irqsave(&hp->lock, hvc_flags);
	if (!hp->tty) {
		spin_unlock_irqrestore(&hp->lock, hvc_flags);
		return;
	}
	ws  = hp->ws;
	tty = tty_kref_get(hp->tty);
	spin_unlock_irqrestore(&hp->lock, hvc_flags);

	tty_do_resize(tty, &ws);
	tty_kref_put(tty);
}

static int hvc_write_room(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;

	if (!hp)
		return -1;

	return hp->outbuf_size - hp->n_outbuf;
}

static int hvc_chars_in_buffer(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;

	if (!hp)
		return 0;
	return hp->n_outbuf;
}

#define MIN_TIMEOUT		(10)
#define MAX_TIMEOUT		(2000)
static u32 timeout = MIN_TIMEOUT;

#define HVC_POLL_READ	0x00000001
#define HVC_POLL_WRITE	0x00000002

int hvc_poll(struct hvc_struct *hp)
{
	struct tty_struct *tty;
	int i, n, poll_mask = 0;
	char buf[N_INBUF] __ALIGNED__;
	unsigned long flags;
	int read_total = 0;
	int written_total = 0;

	spin_lock_irqsave(&hp->lock, flags);

	
	if (hp->n_outbuf > 0)
		written_total = hvc_push(hp);

	
	if (hp->n_outbuf > 0) {
		poll_mask |= HVC_POLL_WRITE;
		
		timeout = (written_total) ? 0 : MIN_TIMEOUT;
	}

	
	tty = tty_kref_get(hp->tty);
	if (tty == NULL)
		goto bail;

	
	if (test_bit(TTY_THROTTLED, &tty->flags))
		goto throttled;

	if (!hp->irq_requested)
		poll_mask |= HVC_POLL_READ;

	
	for (;;) {
		int count = tty_buffer_request_room(tty, N_INBUF);

		
		if (count == 0) {
			poll_mask |= HVC_POLL_READ;
			break;
		}

		n = hp->ops->get_chars(hp->vtermno, buf, count);
		if (n <= 0) {
			
			if (n == -EPIPE) {
				spin_unlock_irqrestore(&hp->lock, flags);
				tty_hangup(tty);
				spin_lock_irqsave(&hp->lock, flags);
			} else if ( n == -EAGAIN ) {
				poll_mask |= HVC_POLL_READ;
			}
			break;
		}
		for (i = 0; i < n; ++i) {
#ifdef CONFIG_MAGIC_SYSRQ
			if (hp->index == hvc_console.index) {
				
				
				if (buf[i] == '\x0f') {	
					sysrq_pressed = !sysrq_pressed;
					if (sysrq_pressed)
						continue;
				} else if (sysrq_pressed) {
					handle_sysrq(buf[i]);
					sysrq_pressed = 0;
					continue;
				}
			}
#endif 
			tty_insert_flip_char(tty, buf[i], 0);
		}

		read_total += n;
	}
 throttled:
	
	if (hp->do_wakeup) {
		hp->do_wakeup = 0;
		tty_wakeup(tty);
	}
 bail:
	spin_unlock_irqrestore(&hp->lock, flags);

	if (read_total) {
		timeout = MIN_TIMEOUT;

		tty_flip_buffer_push(tty);
	}
	if (tty)
		tty_kref_put(tty);

	return poll_mask;
}
EXPORT_SYMBOL_GPL(hvc_poll);

void __hvc_resize(struct hvc_struct *hp, struct winsize ws)
{
	hp->ws = ws;
	schedule_work(&hp->tty_resize);
}
EXPORT_SYMBOL_GPL(__hvc_resize);

static int khvcd(void *unused)
{
	int poll_mask;
	struct hvc_struct *hp;

	set_freezable();
	do {
		poll_mask = 0;
		hvc_kicked = 0;
		try_to_freeze();
		wmb();
		if (!cpus_are_in_xmon()) {
			spin_lock(&hvc_structs_lock);
			list_for_each_entry(hp, &hvc_structs, next) {
				poll_mask |= hvc_poll(hp);
			}
			spin_unlock(&hvc_structs_lock);
		} else
			poll_mask |= HVC_POLL_READ;
		if (hvc_kicked)
			continue;
		set_current_state(TASK_INTERRUPTIBLE);
		if (!hvc_kicked) {
			if (poll_mask == 0)
				schedule();
			else {
				if (timeout < MAX_TIMEOUT)
					timeout += (timeout >> 6) + 1;

				msleep_interruptible(timeout);
			}
		}
		__set_current_state(TASK_RUNNING);
	} while (!kthread_should_stop());

	return 0;
}

static int hvc_tiocmget(struct tty_struct *tty)
{
	struct hvc_struct *hp = tty->driver_data;

	if (!hp || !hp->ops->tiocmget)
		return -EINVAL;
	return hp->ops->tiocmget(hp);
}

static int hvc_tiocmset(struct tty_struct *tty,
			unsigned int set, unsigned int clear)
{
	struct hvc_struct *hp = tty->driver_data;

	if (!hp || !hp->ops->tiocmset)
		return -EINVAL;
	return hp->ops->tiocmset(hp, set, clear);
}

#ifdef CONFIG_CONSOLE_POLL
int hvc_poll_init(struct tty_driver *driver, int line, char *options)
{
	return 0;
}

static int hvc_poll_get_char(struct tty_driver *driver, int line)
{
	struct tty_struct *tty = driver->ttys[0];
	struct hvc_struct *hp = tty->driver_data;
	int n;
	char ch;

	n = hp->ops->get_chars(hp->vtermno, &ch, 1);

	if (n == 0)
		return NO_POLL_CHAR;

	return ch;
}

static void hvc_poll_put_char(struct tty_driver *driver, int line, char ch)
{
	struct tty_struct *tty = driver->ttys[0];
	struct hvc_struct *hp = tty->driver_data;
	int n;

	do {
		n = hp->ops->put_chars(hp->vtermno, &ch, 1);
	} while (n <= 0);
}
#endif

static const struct tty_operations hvc_ops = {
	.open = hvc_open,
	.close = hvc_close,
	.write = hvc_write,
	.hangup = hvc_hangup,
	.unthrottle = hvc_unthrottle,
	.write_room = hvc_write_room,
	.chars_in_buffer = hvc_chars_in_buffer,
	.tiocmget = hvc_tiocmget,
	.tiocmset = hvc_tiocmset,
#ifdef CONFIG_CONSOLE_POLL
	.poll_init = hvc_poll_init,
	.poll_get_char = hvc_poll_get_char,
	.poll_put_char = hvc_poll_put_char,
#endif
};

struct hvc_struct *hvc_alloc(uint32_t vtermno, int data,
			     const struct hv_ops *ops,
			     int outbuf_size)
{
	struct hvc_struct *hp;
	int i;

	
	if (!hvc_driver) {
		int err = hvc_init();
		if (err)
			return ERR_PTR(err);
	}

	hp = kzalloc(ALIGN(sizeof(*hp), sizeof(long)) + outbuf_size,
			GFP_KERNEL);
	if (!hp)
		return ERR_PTR(-ENOMEM);

	hp->vtermno = vtermno;
	hp->data = data;
	hp->ops = ops;
	hp->outbuf_size = outbuf_size;
	hp->outbuf = &((char *)hp)[ALIGN(sizeof(*hp), sizeof(long))];

	kref_init(&hp->kref);

	INIT_WORK(&hp->tty_resize, hvc_set_winsz);
	spin_lock_init(&hp->lock);
	spin_lock(&hvc_structs_lock);

	for (i=0; i < MAX_NR_HVC_CONSOLES; i++)
		if (vtermnos[i] == hp->vtermno &&
		    cons_ops[i] == hp->ops)
			break;

	
	if (i >= MAX_NR_HVC_CONSOLES)
		i = ++last_hvc;

	hp->index = i;

	list_add_tail(&(hp->next), &hvc_structs);
	spin_unlock(&hvc_structs_lock);

	return hp;
}
EXPORT_SYMBOL_GPL(hvc_alloc);

int hvc_remove(struct hvc_struct *hp)
{
	unsigned long flags;
	struct tty_struct *tty;

	spin_lock_irqsave(&hp->lock, flags);
	tty = tty_kref_get(hp->tty);

	if (hp->index < MAX_NR_HVC_CONSOLES)
		vtermnos[hp->index] = -1;

	

	spin_unlock_irqrestore(&hp->lock, flags);

	kref_put(&hp->kref, destroy_hvc_struct);

	if (tty) {
		tty_vhangup(tty);
		tty_kref_put(tty);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(hvc_remove);

static int hvc_init(void)
{
	struct tty_driver *drv;
	int err;

	
	drv = alloc_tty_driver(HVC_ALLOC_TTY_ADAPTERS);
	if (!drv) {
		err = -ENOMEM;
		goto out;
	}

	drv->driver_name = "hvc";
	drv->name = "hvc";
	drv->major = HVC_MAJOR;
	drv->minor_start = HVC_MINOR;
	drv->type = TTY_DRIVER_TYPE_SYSTEM;
	drv->init_termios = tty_std_termios;
	drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_RESET_TERMIOS;
	tty_set_operations(drv, &hvc_ops);

	hvc_task = kthread_run(khvcd, NULL, "khvcd");
	if (IS_ERR(hvc_task)) {
		printk(KERN_ERR "Couldn't create kthread for console.\n");
		err = PTR_ERR(hvc_task);
		goto put_tty;
	}

	err = tty_register_driver(drv);
	if (err) {
		printk(KERN_ERR "Couldn't register hvc console driver\n");
		goto stop_thread;
	}

	smp_mb();
	hvc_driver = drv;
	return 0;

stop_thread:
	kthread_stop(hvc_task);
	hvc_task = NULL;
put_tty:
	put_tty_driver(drv);
out:
	return err;
}

static void __exit hvc_exit(void)
{
	if (hvc_driver) {
		kthread_stop(hvc_task);

		tty_unregister_driver(hvc_driver);
		
		put_tty_driver(hvc_driver);
		unregister_console(&hvc_console);
	}
}
module_exit(hvc_exit);
