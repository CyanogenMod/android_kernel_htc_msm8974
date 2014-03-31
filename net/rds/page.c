/*
 * Copyright (c) 2006 Oracle.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/cpu.h>
#include <linux/export.h>

#include "rds.h"

struct rds_page_remainder {
	struct page	*r_page;
	unsigned long	r_offset;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct rds_page_remainder,
				     rds_page_remainders);

int rds_page_copy_user(struct page *page, unsigned long offset,
		       void __user *ptr, unsigned long bytes,
		       int to_user)
{
	unsigned long ret;
	void *addr;

	addr = kmap(page);
	if (to_user) {
		rds_stats_add(s_copy_to_user, bytes);
		ret = copy_to_user(ptr, addr + offset, bytes);
	} else {
		rds_stats_add(s_copy_from_user, bytes);
		ret = copy_from_user(addr + offset, ptr, bytes);
	}
	kunmap(page);

	return ret ? -EFAULT : 0;
}
EXPORT_SYMBOL_GPL(rds_page_copy_user);

int rds_page_remainder_alloc(struct scatterlist *scat, unsigned long bytes,
			     gfp_t gfp)
{
	struct rds_page_remainder *rem;
	unsigned long flags;
	struct page *page;
	int ret;

	gfp |= __GFP_HIGHMEM;

	
	if (bytes >= PAGE_SIZE) {
		page = alloc_page(gfp);
		if (!page) {
			ret = -ENOMEM;
		} else {
			sg_set_page(scat, page, PAGE_SIZE, 0);
			ret = 0;
		}
		goto out;
	}

	rem = &per_cpu(rds_page_remainders, get_cpu());
	local_irq_save(flags);

	while (1) {
		
		if (rem->r_page && bytes > (PAGE_SIZE - rem->r_offset)) {
			rds_stats_inc(s_page_remainder_miss);
			__free_page(rem->r_page);
			rem->r_page = NULL;
		}

		
		if (rem->r_page && bytes <= (PAGE_SIZE - rem->r_offset)) {
			sg_set_page(scat, rem->r_page, bytes, rem->r_offset);
			get_page(sg_page(scat));

			if (rem->r_offset != 0)
				rds_stats_inc(s_page_remainder_hit);

			rem->r_offset += bytes;
			if (rem->r_offset == PAGE_SIZE) {
				__free_page(rem->r_page);
				rem->r_page = NULL;
			}
			ret = 0;
			break;
		}

		
		local_irq_restore(flags);
		put_cpu();

		page = alloc_page(gfp);

		rem = &per_cpu(rds_page_remainders, get_cpu());
		local_irq_save(flags);

		if (!page) {
			ret = -ENOMEM;
			break;
		}

		
		if (rem->r_page) {
			__free_page(page);
			continue;
		}

		
		rem->r_page = page;
		rem->r_offset = 0;
	}

	local_irq_restore(flags);
	put_cpu();
out:
	rdsdebug("bytes %lu ret %d %p %u %u\n", bytes, ret,
		 ret ? NULL : sg_page(scat), ret ? 0 : scat->offset,
		 ret ? 0 : scat->length);
	return ret;
}
EXPORT_SYMBOL_GPL(rds_page_remainder_alloc);

static int rds_page_remainder_cpu_notify(struct notifier_block *self,
					 unsigned long action, void *hcpu)
{
	struct rds_page_remainder *rem;
	long cpu = (long)hcpu;

	rem = &per_cpu(rds_page_remainders, cpu);

	rdsdebug("cpu %ld action 0x%lx\n", cpu, action);

	switch (action) {
	case CPU_DEAD:
		if (rem->r_page)
			__free_page(rem->r_page);
		rem->r_page = NULL;
		break;
	}

	return 0;
}

static struct notifier_block rds_page_remainder_nb = {
	.notifier_call = rds_page_remainder_cpu_notify,
};

void rds_page_exit(void)
{
	int i;

	for_each_possible_cpu(i)
		rds_page_remainder_cpu_notify(&rds_page_remainder_nb,
					      (unsigned long)CPU_DEAD,
					      (void *)(long)i);
}
