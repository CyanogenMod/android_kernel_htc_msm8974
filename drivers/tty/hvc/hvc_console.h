/*
 * hvc_console.h
 * Copyright (C) 2005 IBM Corporation
 *
 * Author(s):
 * 	Ryan S. Arnold <rsa@us.ibm.com>
 *
 * hvc_console header information:
 *      moved here from arch/powerpc/include/asm/hvconsole.h
 *      and drivers/char/hvc_console.c
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

#ifndef HVC_CONSOLE_H
#define HVC_CONSOLE_H
#include <linux/kref.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#define MAX_NR_HVC_CONSOLES	16

#define HVC_ALLOC_TTY_ADAPTERS	8

struct hvc_struct {
	spinlock_t lock;
	int index;
	struct tty_struct *tty;
	int count;
	int do_wakeup;
	char *outbuf;
	int outbuf_size;
	int n_outbuf;
	uint32_t vtermno;
	const struct hv_ops *ops;
	int irq_requested;
	int data;
	struct winsize ws;
	struct work_struct tty_resize;
	struct list_head next;
	struct kref kref; 
};

struct hv_ops {
	int (*get_chars)(uint32_t vtermno, char *buf, int count);
	int (*put_chars)(uint32_t vtermno, const char *buf, int count);

	
	int (*notifier_add)(struct hvc_struct *hp, int irq);
	void (*notifier_del)(struct hvc_struct *hp, int irq);
	void (*notifier_hangup)(struct hvc_struct *hp, int irq);

	
	int (*tiocmget)(struct hvc_struct *hp);
	int (*tiocmset)(struct hvc_struct *hp, unsigned int set, unsigned int clear);
};

extern int hvc_instantiate(uint32_t vtermno, int index,
			   const struct hv_ops *ops);

extern struct hvc_struct * hvc_alloc(uint32_t vtermno, int data,
				     const struct hv_ops *ops, int outbuf_size);
extern int hvc_remove(struct hvc_struct *hp);

int hvc_poll(struct hvc_struct *hp);
void hvc_kick(void);

extern void __hvc_resize(struct hvc_struct *hp, struct winsize ws);

static inline void hvc_resize(struct hvc_struct *hp, struct winsize ws)
{
	unsigned long flags;

	spin_lock_irqsave(&hp->lock, flags);
	__hvc_resize(hp, ws);
	spin_unlock_irqrestore(&hp->lock, flags);
}

extern int notifier_add_irq(struct hvc_struct *hp, int data);
extern void notifier_del_irq(struct hvc_struct *hp, int data);
extern void notifier_hangup_irq(struct hvc_struct *hp, int data);


#if defined(CONFIG_XMON) && defined(CONFIG_SMP)
#include <asm/xmon.h>
#else
static inline int cpus_are_in_xmon(void)
{
	return 0;
}
#endif

#endif 
