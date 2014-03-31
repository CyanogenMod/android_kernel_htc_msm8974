#ifndef _ASM_IA64_AGP_H
#define _ASM_IA64_AGP_H

/*
 * IA-64 specific AGP definitions.
 *
 * Copyright (C) 2002-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#define map_page_into_agp(page)		
#define unmap_page_from_agp(page)	
#define flush_agp_cache()		mb()

#define alloc_gatt_pages(order)		\
	((char *)__get_free_pages(GFP_KERNEL, (order)))
#define free_gatt_pages(table, order)	\
	free_pages((unsigned long)(table), (order))

#endif 
