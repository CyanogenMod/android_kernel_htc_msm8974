#include <linux/init.h>
#include <linux/io.h>
#include <linux/mm.h>

#include <asm/processor-cyrix.h>
#include <asm/processor-flags.h>
#include <asm/mtrr.h>
#include <asm/msr.h>

#include "mtrr.h"

static void
cyrix_get_arr(unsigned int reg, unsigned long *base,
	      unsigned long *size, mtrr_type * type)
{
	unsigned char arr, ccr3, rcr, shift;
	unsigned long flags;

	arr = CX86_ARR_BASE + (reg << 1) + reg;	

	local_irq_save(flags);

	ccr3 = getCx86(CX86_CCR3);
	setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10);	
	((unsigned char *)base)[3] = getCx86(arr);
	((unsigned char *)base)[2] = getCx86(arr + 1);
	((unsigned char *)base)[1] = getCx86(arr + 2);
	rcr = getCx86(CX86_RCR_BASE + reg);
	setCx86(CX86_CCR3, ccr3);			

	local_irq_restore(flags);

	shift = ((unsigned char *) base)[1] & 0x0f;
	*base >>= PAGE_SHIFT;

	if (shift)
		*size = (reg < 7 ? 0x1UL : 0x40UL) << (shift - 1);
	else
		*size = 0;

	
	if (reg < 7) {
		switch (rcr) {
		case 1:
			*type = MTRR_TYPE_UNCACHABLE;
			break;
		case 8:
			*type = MTRR_TYPE_WRBACK;
			break;
		case 9:
			*type = MTRR_TYPE_WRCOMB;
			break;
		case 24:
		default:
			*type = MTRR_TYPE_WRTHROUGH;
			break;
		}
	} else {
		switch (rcr) {
		case 0:
			*type = MTRR_TYPE_UNCACHABLE;
			break;
		case 8:
			*type = MTRR_TYPE_WRCOMB;
			break;
		case 9:
			*type = MTRR_TYPE_WRBACK;
			break;
		case 25:
		default:
			*type = MTRR_TYPE_WRTHROUGH;
			break;
		}
	}
}

static int
cyrix_get_free_region(unsigned long base, unsigned long size, int replace_reg)
{
	unsigned long lbase, lsize;
	mtrr_type ltype;
	int i;

	switch (replace_reg) {
	case 7:
		if (size < 0x40)
			break;
	case 6:
	case 5:
	case 4:
		return replace_reg;
	case 3:
	case 2:
	case 1:
	case 0:
		return replace_reg;
	}
	
	if (size > 0x2000) {
		cyrix_get_arr(7, &lbase, &lsize, &ltype);
		if (lsize == 0)
			return 7;
		
	} else {
		for (i = 0; i < 7; i++) {
			cyrix_get_arr(i, &lbase, &lsize, &ltype);
			if (lsize == 0)
				return i;
		}
		cyrix_get_arr(i, &lbase, &lsize, &ltype);
		if ((lsize == 0) && (size >= 0x40))
			return i;
	}
	return -ENOSPC;
}

static u32 cr4, ccr3;

static void prepare_set(void)
{
	u32 cr0;

	
	if (cpu_has_pge) {
		cr4 = read_cr4();
		write_cr4(cr4 & ~X86_CR4_PGE);
	}

	cr0 = read_cr0() | X86_CR0_CD;
	wbinvd();
	write_cr0(cr0);
	wbinvd();

	
	ccr3 = getCx86(CX86_CCR3);

	
	setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10);
}

static void post_set(void)
{
	
	wbinvd();

	
	setCx86(CX86_CCR3, ccr3);

	
	write_cr0(read_cr0() & 0xbfffffff);

	
	if (cpu_has_pge)
		write_cr4(cr4);
}

static void cyrix_set_arr(unsigned int reg, unsigned long base,
			  unsigned long size, mtrr_type type)
{
	unsigned char arr, arr_type, arr_size;

	arr = CX86_ARR_BASE + (reg << 1) + reg;	

	
	if (reg >= 7)
		size >>= 6;

	size &= 0x7fff;		
	for (arr_size = 0; size; arr_size++, size >>= 1)
		;

	if (reg < 7) {
		switch (type) {
		case MTRR_TYPE_UNCACHABLE:
			arr_type = 1;
			break;
		case MTRR_TYPE_WRCOMB:
			arr_type = 9;
			break;
		case MTRR_TYPE_WRTHROUGH:
			arr_type = 24;
			break;
		default:
			arr_type = 8;
			break;
		}
	} else {
		switch (type) {
		case MTRR_TYPE_UNCACHABLE:
			arr_type = 0;
			break;
		case MTRR_TYPE_WRCOMB:
			arr_type = 8;
			break;
		case MTRR_TYPE_WRTHROUGH:
			arr_type = 25;
			break;
		default:
			arr_type = 9;
			break;
		}
	}

	prepare_set();

	base <<= PAGE_SHIFT;
	setCx86(arr + 0,  ((unsigned char *)&base)[3]);
	setCx86(arr + 1,  ((unsigned char *)&base)[2]);
	setCx86(arr + 2, (((unsigned char *)&base)[1]) | arr_size);
	setCx86(CX86_RCR_BASE + reg, arr_type);

	post_set();
}

typedef struct {
	unsigned long	base;
	unsigned long	size;
	mtrr_type	type;
} arr_state_t;

static arr_state_t arr_state[8] = {
	{0UL, 0UL, 0UL}, {0UL, 0UL, 0UL}, {0UL, 0UL, 0UL}, {0UL, 0UL, 0UL},
	{0UL, 0UL, 0UL}, {0UL, 0UL, 0UL}, {0UL, 0UL, 0UL}, {0UL, 0UL, 0UL}
};

static unsigned char ccr_state[7] = { 0, 0, 0, 0, 0, 0, 0 };

static void cyrix_set_all(void)
{
	int i;

	prepare_set();

	
	for (i = 0; i < 4; i++)
		setCx86(CX86_CCR0 + i, ccr_state[i]);
	for (; i < 7; i++)
		setCx86(CX86_CCR4 + i, ccr_state[i]);

	for (i = 0; i < 8; i++) {
		cyrix_set_arr(i, arr_state[i].base,
			      arr_state[i].size, arr_state[i].type);
	}

	post_set();
}

static const struct mtrr_ops cyrix_mtrr_ops = {
	.vendor            = X86_VENDOR_CYRIX,
	.set_all	   = cyrix_set_all,
	.set               = cyrix_set_arr,
	.get               = cyrix_get_arr,
	.get_free_region   = cyrix_get_free_region,
	.validate_add_page = generic_validate_add_page,
	.have_wrcomb       = positive_have_wrcomb,
};

int __init cyrix_init_mtrr(void)
{
	set_mtrr_ops(&cyrix_mtrr_ops);
	return 0;
}
