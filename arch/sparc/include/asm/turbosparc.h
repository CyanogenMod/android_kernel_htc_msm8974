/*
 * turbosparc.h:  Defines specific to the TurboSparc module.
 *            This is SRMMU stuff.
 *
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */
#ifndef _SPARC_TURBOSPARC_H
#define _SPARC_TURBOSPARC_H

#include <asm/asi.h>
#include <asm/pgtsrmmu.h>


#define TURBOSPARC_MMUENABLE    0x00000001
#define TURBOSPARC_NOFAULT      0x00000002
#define TURBOSPARC_ICSNOOP	0x00000004
#define TURBOSPARC_PSO          0x00000080
#define TURBOSPARC_DCENABLE     0x00000100   
#define TURBOSPARC_ICENABLE     0x00000200   
#define TURBOSPARC_BMODE        0x00004000   
#define TURBOSPARC_PARITYODD	0x00020000   
#define TURBOSPARC_PCENABLE	0x00040000   


#define TURBOSPARC_SCENABLE 0x00000008	 
#define TURBOSPARC_uS2	    0x00000010   
#define TURBOSPARC_WTENABLE 0x00000020	 
#define TURBOSPARC_SNENABLE 0x40000000	 

#ifndef __ASSEMBLY__

static inline void turbosparc_inv_insn_tag(unsigned long addr)
{
        __asm__ __volatile__("sta %%g0, [%0] %1\n\t"
			     : 
			     : "r" (addr), "i" (ASI_M_TXTC_TAG)
			     : "memory");
}

static inline void turbosparc_inv_data_tag(unsigned long addr)
{
        __asm__ __volatile__("sta %%g0, [%0] %1\n\t"
			     : 
			     : "r" (addr), "i" (ASI_M_DATAC_TAG)
			     : "memory");
}

static inline void turbosparc_flush_icache(void)
{
	unsigned long addr;

        for (addr = 0; addr < 0x4000; addr += 0x20)
                turbosparc_inv_insn_tag(addr);
}

static inline void turbosparc_flush_dcache(void)
{
	unsigned long addr;

        for (addr = 0; addr < 0x4000; addr += 0x20)
                turbosparc_inv_data_tag(addr);
}

static inline void turbosparc_idflash_clear(void)
{
	unsigned long addr;

        for (addr = 0; addr < 0x4000; addr += 0x20) {
                turbosparc_inv_insn_tag(addr);
                turbosparc_inv_data_tag(addr);
	}
}

static inline void turbosparc_set_ccreg(unsigned long regval)
{
	__asm__ __volatile__("sta %0, [%1] %2\n\t"
			     : 
			     : "r" (regval), "r" (0x600), "i" (ASI_M_MMUREGS)
			     : "memory");
}

static inline unsigned long turbosparc_get_ccreg(void)
{
	unsigned long regval;

	__asm__ __volatile__("lda [%1] %2, %0\n\t"
			     : "=r" (regval)
			     : "r" (0x600), "i" (ASI_M_MMUREGS));
	return regval;
}

#endif 

#endif 
