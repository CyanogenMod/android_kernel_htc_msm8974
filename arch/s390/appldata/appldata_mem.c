/*
 * arch/s390/appldata/appldata_mem.c
 *
 * Data gathering module for Linux-VM Monitor Stream, Stage 1.
 * Collects data related to memory management.
 *
 * Copyright (C) 2003,2006 IBM Corporation, IBM Deutschland Entwicklung GmbH.
 *
 * Author: Gerald Schaefer <gerald.schaefer@de.ibm.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel_stat.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <asm/io.h>

#include "appldata.h"


#define P2K(x) ((x) << (PAGE_SHIFT - 10))	

static struct appldata_mem_data {
	u64 timestamp;
	u32 sync_count_1;       
	u32 sync_count_2;	

	u64 pgpgin;		
	u64 pgpgout;		/* data written to disk */
	u64 pswpin;		
	u64 pswpout;		

	u64 sharedram;		

	u64 totalram;		
	u64 freeram;		
	u64 totalhigh;		
	u64 freehigh;		

	u64 bufferram;		
	u64 cached;		
	u64 totalswap;		
	u64 freeswap;		

	u64 pgalloc;		
	u64 pgfault;		
	u64 pgmajfault;		

} __attribute__((packed)) appldata_mem_data;


static void appldata_get_mem_data(void *data)
{
	static struct sysinfo val;
	unsigned long ev[NR_VM_EVENT_ITEMS];
	struct appldata_mem_data *mem_data;

	mem_data = data;
	mem_data->sync_count_1++;

	all_vm_events(ev);
	mem_data->pgpgin     = ev[PGPGIN] >> 1;
	mem_data->pgpgout    = ev[PGPGOUT] >> 1;
	mem_data->pswpin     = ev[PSWPIN];
	mem_data->pswpout    = ev[PSWPOUT];
	mem_data->pgalloc    = ev[PGALLOC_NORMAL];
	mem_data->pgalloc    += ev[PGALLOC_DMA];
	mem_data->pgfault    = ev[PGFAULT];
	mem_data->pgmajfault = ev[PGMAJFAULT];

	si_meminfo(&val);
	mem_data->sharedram = val.sharedram;
	mem_data->totalram  = P2K(val.totalram);
	mem_data->freeram   = P2K(val.freeram);
	mem_data->totalhigh = P2K(val.totalhigh);
	mem_data->freehigh  = P2K(val.freehigh);
	mem_data->bufferram = P2K(val.bufferram);
	mem_data->cached    = P2K(global_page_state(NR_FILE_PAGES)
				- val.bufferram);

	si_swapinfo(&val);
	mem_data->totalswap = P2K(val.totalswap);
	mem_data->freeswap  = P2K(val.freeswap);

	mem_data->timestamp = get_clock();
	mem_data->sync_count_2++;
}


static struct appldata_ops ops = {
	.name      = "mem",
	.record_nr = APPLDATA_RECORD_MEM_ID,
	.size	   = sizeof(struct appldata_mem_data),
	.callback  = &appldata_get_mem_data,
	.data      = &appldata_mem_data,
	.owner     = THIS_MODULE,
	.mod_lvl   = {0xF0, 0xF0},		
};


static int __init appldata_mem_init(void)
{
	return appldata_register_ops(&ops);
}

static void __exit appldata_mem_exit(void)
{
	appldata_unregister_ops(&ops);
}


module_init(appldata_mem_init);
module_exit(appldata_mem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gerald Schaefer");
MODULE_DESCRIPTION("Linux-VM Monitor Stream, MEMORY statistics");
