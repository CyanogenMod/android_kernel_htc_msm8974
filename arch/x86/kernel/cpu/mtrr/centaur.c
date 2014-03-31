#include <linux/init.h>
#include <linux/mm.h>

#include <asm/mtrr.h>
#include <asm/msr.h>

#include "mtrr.h"

static struct {
	unsigned long high;
	unsigned long low;
} centaur_mcr[8];

static u8 centaur_mcr_reserved;
static u8 centaur_mcr_type;	

static int
centaur_get_free_region(unsigned long base, unsigned long size, int replace_reg)
{
	unsigned long lbase, lsize;
	mtrr_type ltype;
	int i, max;

	max = num_var_ranges;
	if (replace_reg >= 0 && replace_reg < max)
		return replace_reg;

	for (i = 0; i < max; ++i) {
		if (centaur_mcr_reserved & (1 << i))
			continue;
		mtrr_if->get(i, &lbase, &lsize, &ltype);
		if (lsize == 0)
			return i;
	}

	return -ENOSPC;
}

void mtrr_centaur_report_mcr(int mcr, u32 lo, u32 hi)
{
	centaur_mcr[mcr].low = lo;
	centaur_mcr[mcr].high = hi;
}

static void
centaur_get_mcr(unsigned int reg, unsigned long *base,
		unsigned long *size, mtrr_type * type)
{
	*base = centaur_mcr[reg].high >> PAGE_SHIFT;
	*size = -(centaur_mcr[reg].low & 0xfffff000) >> PAGE_SHIFT;
	*type = MTRR_TYPE_WRCOMB;		

	if (centaur_mcr_type == 1 && ((centaur_mcr[reg].low & 31) & 2))
		*type = MTRR_TYPE_UNCACHABLE;
	if (centaur_mcr_type == 1 && (centaur_mcr[reg].low & 31) == 25)
		*type = MTRR_TYPE_WRBACK;
	if (centaur_mcr_type == 0 && (centaur_mcr[reg].low & 31) == 31)
		*type = MTRR_TYPE_WRBACK;
}

static void
centaur_set_mcr(unsigned int reg, unsigned long base,
		unsigned long size, mtrr_type type)
{
	unsigned long low, high;

	if (size == 0) {
		
		high = low = 0;
	} else {
		high = base << PAGE_SHIFT;
		if (centaur_mcr_type == 0) {
			
			low = -size << PAGE_SHIFT | 0x1f;
		} else {
			if (type == MTRR_TYPE_UNCACHABLE)
				low = -size << PAGE_SHIFT | 0x02; 
			else
				low = -size << PAGE_SHIFT | 0x09; 
		}
	}
	centaur_mcr[reg].high = high;
	centaur_mcr[reg].low = low;
	wrmsr(MSR_IDT_MCR0 + reg, low, high);
}

static int
centaur_validate_add_page(unsigned long base, unsigned long size, unsigned int type)
{
	if (type != MTRR_TYPE_WRCOMB &&
	    (centaur_mcr_type == 0 || type != MTRR_TYPE_UNCACHABLE)) {
		pr_warning("mtrr: only write-combining%s supported\n",
			   centaur_mcr_type ? " and uncacheable are" : " is");
		return -EINVAL;
	}
	return 0;
}

static const struct mtrr_ops centaur_mtrr_ops = {
	.vendor            = X86_VENDOR_CENTAUR,
	.set               = centaur_set_mcr,
	.get               = centaur_get_mcr,
	.get_free_region   = centaur_get_free_region,
	.validate_add_page = centaur_validate_add_page,
	.have_wrcomb       = positive_have_wrcomb,
};

int __init centaur_init_mtrr(void)
{
	set_mtrr_ops(&centaur_mtrr_ops);
	return 0;
}
