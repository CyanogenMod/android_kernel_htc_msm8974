/*
 * File:	mca_drv.c
 * Purpose:	Generic MCA handling layer
 *
 * Copyright (C) 2004 FUJITSU LIMITED
 * Copyright (C) 2004 Hidetoshi Seto <seto.hidetoshi@jp.fujitsu.com>
 * Copyright (C) 2005 Silicon Graphics, Inc
 * Copyright (C) 2005 Keith Owens <kaos@sgi.com>
 * Copyright (C) 2006 Russ Anderson <rja@sgi.com>
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kallsyms.h>
#include <linux/bootmem.h>
#include <linux/acpi.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/delay.h>
#include <asm/machvec.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/mca.h>

#include <asm/irq.h>
#include <asm/hw_irq.h>

#include "mca_drv.h"

static int sal_rec_max = 10000;

extern void *mca_handler_bhhook(void);

static DEFINE_SPINLOCK(mca_bh_lock);

typedef enum {
	MCA_IS_LOCAL  = 0,
	MCA_IS_GLOBAL = 1
} mca_type_t;

#define MAX_PAGE_ISOLATE 1024

static struct page *page_isolate[MAX_PAGE_ISOLATE];
static int num_page_isolate = 0;

typedef enum {
	ISOLATE_NG,
	ISOLATE_OK,
	ISOLATE_NONE
} isolate_status_t;

typedef enum {
	MCA_NOT_RECOVERED = 0,
	MCA_RECOVERED	  = 1
} recovery_status_t;

static struct {
	slidx_list_t *buffer; 
	int	     cur_idx; 
	int	     max_idx; 
} slidx_pool;

static int
fatal_mca(const char *fmt, ...)
{
	va_list args;
	char buf[256];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	ia64_mca_printk(KERN_ALERT "MCA: %s\n", buf);

	return MCA_NOT_RECOVERED;
}

static int
mca_recovered(const char *fmt, ...)
{
	va_list args;
	char buf[256];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	ia64_mca_printk(KERN_INFO "MCA: %s\n", buf);

	return MCA_RECOVERED;
}


static isolate_status_t
mca_page_isolate(unsigned long paddr)
{
	int i;
	struct page *p;

	
	if (!ia64_phys_addr_valid(paddr))
		return ISOLATE_NONE;

	if (!pfn_valid(paddr >> PAGE_SHIFT))
		return ISOLATE_NONE;

	
	p = pfn_to_page(paddr>>PAGE_SHIFT);

	
	for (i = 0; i < num_page_isolate; i++)
		if (page_isolate[i] == p)
			return ISOLATE_OK; 

	
	if (num_page_isolate == MAX_PAGE_ISOLATE)
		return ISOLATE_NG;

	
	if (PageSlab(p) || PageReserved(p))
		return ISOLATE_NG;

	
	get_page(p);
	SetPageReserved(p);
	page_isolate[num_page_isolate++] = p;

	return ISOLATE_OK;
}


void
mca_handler_bh(unsigned long paddr, void *iip, unsigned long ipsr)
{
	ia64_mlogbuf_dump();
	printk(KERN_ERR "OS_MCA: process [cpu %d, pid: %d, uid: %d, "
		"iip: %p, psr: 0x%lx,paddr: 0x%lx](%s) encounters MCA.\n",
	       raw_smp_processor_id(), current->pid, current_uid(),
		iip, ipsr, paddr, current->comm);

	spin_lock(&mca_bh_lock);
	switch (mca_page_isolate(paddr)) {
	case ISOLATE_OK:
		printk(KERN_DEBUG "Page isolation: ( %lx ) success.\n", paddr);
		break;
	case ISOLATE_NG:
		printk(KERN_CRIT "Page isolation: ( %lx ) failure.\n", paddr);
		break;
	default:
		break;
	}
	spin_unlock(&mca_bh_lock);

	
	do_exit(SIGKILL);
}


static void
mca_make_peidx(sal_log_processor_info_t *slpi, peidx_table_t *peidx)
{
	u64 total_check_num = slpi->valid.num_cache_check
				+ slpi->valid.num_tlb_check
				+ slpi->valid.num_bus_check
				+ slpi->valid.num_reg_file_check
				+ slpi->valid.num_ms_check;
	u64 head_size =	sizeof(sal_log_mod_error_info_t) * total_check_num
			+ sizeof(sal_log_processor_info_t);
	u64 mid_size  = slpi->valid.cpuid_info * sizeof(struct sal_cpuid_info);

	peidx_head(peidx)   = slpi;
	peidx_mid(peidx)    = (struct sal_cpuid_info *)
		(slpi->valid.cpuid_info ? ((char*)slpi + head_size) : NULL);
	peidx_bottom(peidx) = (sal_processor_static_info_t *)
		(slpi->valid.psi_static_struct ?
			((char*)slpi + head_size + mid_size) : NULL);
}

#define LOG_INDEX_ADD_SECT_PTR(sect, ptr) \
	{slidx_list_t *hl = &slidx_pool.buffer[slidx_pool.cur_idx]; \
	hl->hdr = ptr; \
	list_add(&hl->list, &(sect)); \
	slidx_pool.cur_idx = (slidx_pool.cur_idx + 1)%slidx_pool.max_idx; }

static int
mca_make_slidx(void *buffer, slidx_table_t *slidx)
{
	int platform_err = 0;
	int record_len = ((sal_log_record_header_t*)buffer)->len;
	u32 ercd_pos;
	int sects;
	sal_log_section_hdr_t *sp;

	INIT_LIST_HEAD(&(slidx->proc_err));
	INIT_LIST_HEAD(&(slidx->mem_dev_err));
	INIT_LIST_HEAD(&(slidx->sel_dev_err));
	INIT_LIST_HEAD(&(slidx->pci_bus_err));
	INIT_LIST_HEAD(&(slidx->smbios_dev_err));
	INIT_LIST_HEAD(&(slidx->pci_comp_err));
	INIT_LIST_HEAD(&(slidx->plat_specific_err));
	INIT_LIST_HEAD(&(slidx->host_ctlr_err));
	INIT_LIST_HEAD(&(slidx->plat_bus_err));
	INIT_LIST_HEAD(&(slidx->unsupported));

	slidx->header = buffer;

	for (ercd_pos = sizeof(sal_log_record_header_t), sects = 0;
		ercd_pos < record_len; ercd_pos += sp->len, sects++) {
		sp = (sal_log_section_hdr_t *)((char*)buffer + ercd_pos);
		if (!efi_guidcmp(sp->guid, SAL_PROC_DEV_ERR_SECT_GUID)) {
			LOG_INDEX_ADD_SECT_PTR(slidx->proc_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_MEM_DEV_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->mem_dev_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_SEL_DEV_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->sel_dev_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_PCI_BUS_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->pci_bus_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_SMBIOS_DEV_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->smbios_dev_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_PCI_COMP_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->pci_comp_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_SPECIFIC_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->plat_specific_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_HOST_CTLR_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->host_ctlr_err, sp);
		} else if (!efi_guidcmp(sp->guid,
				SAL_PLAT_BUS_ERR_SECT_GUID)) {
			platform_err = 1;
			LOG_INDEX_ADD_SECT_PTR(slidx->plat_bus_err, sp);
		} else {
			LOG_INDEX_ADD_SECT_PTR(slidx->unsupported, sp);
		}
	}
	slidx->n_sections = sects;

	return platform_err;
}

static int
init_record_index_pools(void)
{
	int i;
	int rec_max_size;  
	int sect_min_size; 
	
	static int sal_log_sect_min_sizes[] = {
		sizeof(sal_log_processor_info_t)
		+ sizeof(sal_processor_static_info_t),
		sizeof(sal_log_mem_dev_err_info_t),
		sizeof(sal_log_sel_dev_err_info_t),
		sizeof(sal_log_pci_bus_err_info_t),
		sizeof(sal_log_smbios_dev_err_info_t),
		sizeof(sal_log_pci_comp_err_info_t),
		sizeof(sal_log_plat_specific_err_info_t),
		sizeof(sal_log_host_ctlr_err_info_t),
		sizeof(sal_log_plat_bus_err_info_t),
	};


	
	rec_max_size = sal_rec_max;

	
	sect_min_size = sal_log_sect_min_sizes[0];
	for (i = 1; i < sizeof sal_log_sect_min_sizes/sizeof(size_t); i++)
		if (sect_min_size > sal_log_sect_min_sizes[i])
			sect_min_size = sal_log_sect_min_sizes[i];

	
	slidx_pool.max_idx = (rec_max_size/sect_min_size) * 2 + 1;
	slidx_pool.buffer = (slidx_list_t *)
		kmalloc(slidx_pool.max_idx * sizeof(slidx_list_t), GFP_KERNEL);

	return slidx_pool.buffer ? 0 : -ENOMEM;
}




static mca_type_t
is_mca_global(peidx_table_t *peidx, pal_bus_check_info_t *pbci,
	      struct ia64_sal_os_state *sos)
{
	pal_processor_state_info_t *psp =
		(pal_processor_state_info_t*)peidx_psp(peidx);

	switch (sos->rv_rc) {
		case -1: 
			return MCA_IS_GLOBAL;
		case  0: 
			return MCA_IS_LOCAL;
		case  1: 
		case  2: 
		default:
			break;
	}

	if (psp->tc || psp->cc || psp->rc || psp->uc)
		return MCA_IS_LOCAL;
	
	if (!pbci || pbci->ib)
		return MCA_IS_GLOBAL;

	if (pbci->eb)
		switch (pbci->bsi) {
			case 0:
				
				return MCA_IS_LOCAL;
			case 1:
			case 2:
			case 3:
				return MCA_IS_GLOBAL;
		}

	return MCA_IS_GLOBAL;
}

static u64
get_target_identifier(peidx_table_t *peidx)
{
	u64 target_address = 0;
	sal_log_mod_error_info_t *smei;
	pal_cache_check_info_t *pcci;
	int i, level = 9;

	for (i = 0; i < peidx_cache_check_num(peidx); i++) {
		smei = (sal_log_mod_error_info_t *)peidx_cache_check(peidx, i);
		if (smei->valid.target_identifier && smei->target_identifier) {
			pcci = (pal_cache_check_info_t *)&(smei->check_info);
			if (!target_address || (pcci->level < level)) {
				target_address = smei->target_identifier;
				level = pcci->level;
				continue;
			}
		}
	}
	if (target_address)
		return target_address;

	smei = peidx_bus_check(peidx, 0);
	if (smei && smei->valid.target_identifier)
		return smei->target_identifier;

	return 0;
}


static int
recover_from_read_error(slidx_table_t *slidx,
			peidx_table_t *peidx, pal_bus_check_info_t *pbci,
			struct ia64_sal_os_state *sos)
{
	u64 target_identifier;
	pal_min_state_area_t *pmsa;
	struct ia64_psr *psr1, *psr2;
	ia64_fptr_t *mca_hdlr_bh = (ia64_fptr_t*)mca_handler_bhhook;

	
	target_identifier = get_target_identifier(peidx);
	if (!target_identifier)
		return fatal_mca("target address not valid");


	
	if (!peidx_bottom(peidx) || !(peidx_bottom(peidx)->valid.minstate))
		return fatal_mca("minstate not valid");
	psr1 =(struct ia64_psr *)&(peidx_minstate_area(peidx)->pmsa_ipsr);
	psr2 =(struct ia64_psr *)&(peidx_minstate_area(peidx)->pmsa_xpsr);


	pmsa = sos->pal_min_state;
	if (psr1->cpl != 0 ||
	   ((psr2->cpl != 0) && mca_recover_range(pmsa->pmsa_iip))) {
		
		pmsa->pmsa_gr[8-1] = target_identifier;
		pmsa->pmsa_gr[9-1] = pmsa->pmsa_iip;
		pmsa->pmsa_gr[10-1] = pmsa->pmsa_ipsr;
		
		pmsa->pmsa_br0 = pmsa->pmsa_iip;
		
		pmsa->pmsa_iip = mca_hdlr_bh->fp;
		pmsa->pmsa_gr[1-1] = mca_hdlr_bh->gp;
		
		psr2 = (struct ia64_psr *)&pmsa->pmsa_ipsr;
		psr2->cpl = 0;
		psr2->ri  = 0;
		psr2->bn  = 1;
		psr2->i  = 0;

		return mca_recovered("user memory corruption. "
				"kill affected process - recovered.");
	}

	return fatal_mca("kernel context not recovered, iip 0x%lx\n",
			 pmsa->pmsa_iip);
}


static int
recover_from_platform_error(slidx_table_t *slidx, peidx_table_t *peidx,
			    pal_bus_check_info_t *pbci,
			    struct ia64_sal_os_state *sos)
{
	int status = 0;
	pal_processor_state_info_t *psp =
		(pal_processor_state_info_t*)peidx_psp(peidx);

	if (psp->bc && pbci->eb && pbci->bsi == 0) {
		switch(pbci->type) {
		case 1: 
		case 3: 
		case 9: 
			status = recover_from_read_error(slidx, peidx, pbci,
							 sos);
			break;
		case 0: 
		case 2: 
		case 4: 
		case 5: 
		case 6: 
		case 7: 
		case 8: 
		case 10: 
		case 11: 
		case 12: 
		default:
			break;
		}
	} else if (psp->cc && !psp->bc) {	
		status = recover_from_read_error(slidx, peidx, pbci, sos);
	}

	return status;
}

static int
recover_from_tlb_check(peidx_table_t *peidx)
{
	sal_log_mod_error_info_t *smei;
	pal_tlb_check_info_t *ptci;

	smei = (sal_log_mod_error_info_t *)peidx_tlb_check(peidx, 0);
	ptci = (pal_tlb_check_info_t *)&(smei->check_info);

	if (ptci->op == PAL_TLB_CHECK_OP_PURGE
	    && !(ptci->itr || ptci->dtc || ptci->itc))
		return fatal_mca("Duplicate TLB entry");

	return mca_recovered("TLB check recovered");
}


static int
recover_from_processor_error(int platform, slidx_table_t *slidx,
			     peidx_table_t *peidx, pal_bus_check_info_t *pbci,
			     struct ia64_sal_os_state *sos)
{
	pal_processor_state_info_t *psp =
		(pal_processor_state_info_t*)peidx_psp(peidx);


	if (psp->cm == 1)
		return mca_recovered("machine check is already corrected.");

	if (psp->us || psp->ci == 0)
		return fatal_mca("error not contained");

	if (psp->tc && !(psp->cc || psp->bc || psp->rc || psp->uc))
		return recover_from_tlb_check(peidx);

	if (psp->cc == 0 && (psp->bc == 0 || pbci == NULL))
		return fatal_mca("No cache or bus check");

	if (peidx_bus_check_num(peidx) > 1)
		return fatal_mca("Too many bus checks");

	if (pbci->ib)
		return fatal_mca("Internal Bus error");
	if (pbci->eb && pbci->bsi > 0)
		return fatal_mca("External bus check fatal status");

	if (platform)
		return recover_from_platform_error(slidx, peidx, pbci, sos);

	return fatal_mca("Strange SAL record");
}


static int
mca_try_to_recover(void *rec, struct ia64_sal_os_state *sos)
{
	int platform_err;
	int n_proc_err;
	slidx_table_t slidx;
	peidx_table_t peidx;
	pal_bus_check_info_t pbci;

	
	platform_err = mca_make_slidx(rec, &slidx);

	
	n_proc_err = slidx_count(&slidx, proc_err);

	 
	if (n_proc_err > 1)
		return fatal_mca("Too Many Errors");
	else if (n_proc_err == 0)
		
		return fatal_mca("Weird SAL record");

	
	mca_make_peidx((sal_log_processor_info_t*)
		slidx_first_entry(&slidx.proc_err)->hdr, &peidx);

	
	*((u64*)&pbci) = peidx_check_info(&peidx, bus_check, 0);

	
	if (is_mca_global(&peidx, &pbci, sos))
		return fatal_mca("global MCA");
	
	
	return recover_from_processor_error(platform_err, &slidx, &peidx,
					    &pbci, sos);
}


int __init mca_external_handler_init(void)
{
	if (init_record_index_pools())
		return -ENOMEM;

	
	if (ia64_reg_MCA_extension(mca_try_to_recover)) {	
		printk(KERN_ERR "ia64_reg_MCA_extension failed.\n");
		kfree(slidx_pool.buffer);
		return -EFAULT;
	}
	return 0;
}

void __exit mca_external_handler_exit(void)
{
	
	ia64_unreg_MCA_extension();
	kfree(slidx_pool.buffer);
}

module_init(mca_external_handler_init);
module_exit(mca_external_handler_exit);

module_param(sal_rec_max, int, 0644);
MODULE_PARM_DESC(sal_rec_max, "Max size of SAL error record");

MODULE_DESCRIPTION("ia64 platform dependent mca handler driver");
MODULE_LICENSE("GPL");
