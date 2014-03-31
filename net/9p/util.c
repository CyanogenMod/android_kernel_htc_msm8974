/*
 *  net/9p/util.c
 *
 *  This file contains some helper functions
 *
 *  Copyright (C) 2007 by Latchesar Ionkov <lucho@ionkov.net>
 *  Copyright (C) 2004 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <net/9p/9p.h>


struct p9_idpool {
	spinlock_t lock;
	struct idr pool;
};


struct p9_idpool *p9_idpool_create(void)
{
	struct p9_idpool *p;

	p = kmalloc(sizeof(struct p9_idpool), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&p->lock);
	idr_init(&p->pool);

	return p;
}
EXPORT_SYMBOL(p9_idpool_create);


void p9_idpool_destroy(struct p9_idpool *p)
{
	idr_destroy(&p->pool);
	kfree(p);
}
EXPORT_SYMBOL(p9_idpool_destroy);


int p9_idpool_get(struct p9_idpool *p)
{
	int i = 0;
	int error;
	unsigned long flags;

retry:
	if (idr_pre_get(&p->pool, GFP_NOFS) == 0)
		return -1;

	spin_lock_irqsave(&p->lock, flags);

	
	error = idr_get_new(&p->pool, p, &i);
	spin_unlock_irqrestore(&p->lock, flags);

	if (error == -EAGAIN)
		goto retry;
	else if (error)
		return -1;

	p9_debug(P9_DEBUG_MUX, " id %d pool %p\n", i, p);
	return i;
}
EXPORT_SYMBOL(p9_idpool_get);


void p9_idpool_put(int id, struct p9_idpool *p)
{
	unsigned long flags;

	p9_debug(P9_DEBUG_MUX, " id %d pool %p\n", id, p);

	spin_lock_irqsave(&p->lock, flags);
	idr_remove(&p->pool, id);
	spin_unlock_irqrestore(&p->lock, flags);
}
EXPORT_SYMBOL(p9_idpool_put);


int p9_idpool_check(int id, struct p9_idpool *p)
{
	return idr_find(&p->pool, id) != NULL;
}
EXPORT_SYMBOL(p9_idpool_check);

