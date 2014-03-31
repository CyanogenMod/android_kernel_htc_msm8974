/*
 * Copyright IBM Corporation, 2010
 * Author Venkateswararao Jujjuri <jvrao@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <net/9p/9p.h>
#include <net/9p/client.h>
#include <linux/scatterlist.h>
#include "trans_common.h"

void p9_release_pages(struct page **pages, int nr_pages)
{
	int i = 0;
	while (pages[i] && nr_pages--) {
		put_page(pages[i]);
		i++;
	}
}
EXPORT_SYMBOL(p9_release_pages);

int p9_nr_pages(char *data, int len)
{
	unsigned long start_page, end_page;
	start_page =  (unsigned long)data >> PAGE_SHIFT;
	end_page = ((unsigned long)data + len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	return end_page - start_page;
}
EXPORT_SYMBOL(p9_nr_pages);


int p9_payload_gup(char *data, int *nr_pages, struct page **pages, int write)
{
	int nr_mapped_pages;

	nr_mapped_pages = get_user_pages_fast((unsigned long)data,
					      *nr_pages, write, pages);
	if (nr_mapped_pages <= 0)
		return nr_mapped_pages;

	*nr_pages = nr_mapped_pages;
	return 0;
}
EXPORT_SYMBOL(p9_payload_gup);
