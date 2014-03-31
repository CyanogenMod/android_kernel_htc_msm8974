/*
 * Suspend support specific for s390.
 *
 * Copyright IBM Corp. 2009
 *
 * Author(s): Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 */

#include <linux/pfn.h>
#include <linux/suspend.h>
#include <linux/mm.h>
#include <asm/ctl_reg.h>

extern const void __nosave_begin, __nosave_end;


struct page_key_data {
	struct page_key_data *next;
	unsigned char data[];
};

#define PAGE_KEY_DATA_SIZE	(PAGE_SIZE - sizeof(struct page_key_data *))

static struct page_key_data *page_key_data;
static struct page_key_data *page_key_rp, *page_key_wp;
static unsigned long page_key_rx, page_key_wx;

unsigned long page_key_additional_pages(unsigned long pages)
{
	return DIV_ROUND_UP(pages, PAGE_KEY_DATA_SIZE);
}

void page_key_free(void)
{
	struct page_key_data *pkd;

	while (page_key_data) {
		pkd = page_key_data;
		page_key_data = pkd->next;
		free_page((unsigned long) pkd);
	}
}

int page_key_alloc(unsigned long pages)
{
	struct page_key_data *pk;
	unsigned long size;

	size = DIV_ROUND_UP(pages, PAGE_KEY_DATA_SIZE);
	while (size--) {
		pk = (struct page_key_data *) get_zeroed_page(GFP_KERNEL);
		if (!pk) {
			page_key_free();
			return -ENOMEM;
		}
		pk->next = page_key_data;
		page_key_data = pk;
	}
	page_key_rp = page_key_wp = page_key_data;
	page_key_rx = page_key_wx = 0;
	return 0;
}

void page_key_read(unsigned long *pfn)
{
	unsigned long addr;

	addr = (unsigned long) page_address(pfn_to_page(*pfn));
	*(unsigned char *) pfn = (unsigned char) page_get_storage_key(addr);
}

void page_key_memorize(unsigned long *pfn)
{
	page_key_wp->data[page_key_wx] = *(unsigned char *) pfn;
	*(unsigned char *) pfn = 0;
	if (++page_key_wx < PAGE_KEY_DATA_SIZE)
		return;
	page_key_wp = page_key_wp->next;
	page_key_wx = 0;
}

void page_key_write(void *address)
{
	page_set_storage_key((unsigned long) address,
			     page_key_rp->data[page_key_rx], 0);
	if (++page_key_rx >= PAGE_KEY_DATA_SIZE)
		return;
	page_key_rp = page_key_rp->next;
	page_key_rx = 0;
}

int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = PFN_DOWN(__pa(&__nosave_begin));
	unsigned long nosave_end_pfn = PFN_DOWN(__pa(&__nosave_end));

	
	if (pfn <= LC_PAGES)
		return 0;
	if (pfn >= nosave_begin_pfn && pfn < nosave_end_pfn)
		return 1;
	
	if (tprot(PFN_PHYS(pfn)))
		return 1;
	return 0;
}

void save_processor_state(void)
{
	local_mcck_disable();
	
	__ctl_clear_bit(0,28);
	S390_lowcore.external_new_psw.mask &= ~PSW_MASK_MCHECK;
	S390_lowcore.svc_new_psw.mask &= ~PSW_MASK_MCHECK;
	S390_lowcore.io_new_psw.mask &= ~PSW_MASK_MCHECK;
	S390_lowcore.program_new_psw.mask &= ~PSW_MASK_MCHECK;
}

void restore_processor_state(void)
{
	S390_lowcore.external_new_psw.mask |= PSW_MASK_MCHECK;
	S390_lowcore.svc_new_psw.mask |= PSW_MASK_MCHECK;
	S390_lowcore.io_new_psw.mask |= PSW_MASK_MCHECK;
	S390_lowcore.program_new_psw.mask |= PSW_MASK_MCHECK;
	
	__ctl_set_bit(0,28);
	local_mcck_enable();
}
