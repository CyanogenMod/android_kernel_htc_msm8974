#ifndef __ASM_ALPHA_FPU_H
#define __ASM_ALPHA_FPU_H

#include <asm/special_insns.h>

#define FPCR_DNOD	(1UL<<47)	
#define FPCR_DNZ	(1UL<<48)	
#define FPCR_INVD	(1UL<<49)	
#define FPCR_DZED	(1UL<<50)	
#define FPCR_OVFD	(1UL<<51)	
#define FPCR_INV	(1UL<<52)	
#define FPCR_DZE	(1UL<<53)	
#define FPCR_OVF	(1UL<<54)	
#define FPCR_UNF	(1UL<<55)	
#define FPCR_INE	(1UL<<56)	
#define FPCR_IOV	(1UL<<57)	
#define FPCR_UNDZ	(1UL<<60)	
#define FPCR_UNFD	(1UL<<61)	
#define FPCR_INED	(1UL<<62)	
#define FPCR_SUM	(1UL<<63)	

#define FPCR_DYN_SHIFT	58		
#define FPCR_DYN_CHOPPED (0x0UL << FPCR_DYN_SHIFT)	
#define FPCR_DYN_MINUS	 (0x1UL << FPCR_DYN_SHIFT)	
#define FPCR_DYN_NORMAL	 (0x2UL << FPCR_DYN_SHIFT)	
#define FPCR_DYN_PLUS	 (0x3UL << FPCR_DYN_SHIFT)	
#define FPCR_DYN_MASK	 (0x3UL << FPCR_DYN_SHIFT)

#define FPCR_MASK	0xffff800000000000L

#define IEEE_TRAP_ENABLE_INV	(1UL<<1)	
#define IEEE_TRAP_ENABLE_DZE	(1UL<<2)	
#define IEEE_TRAP_ENABLE_OVF	(1UL<<3)	
#define IEEE_TRAP_ENABLE_UNF	(1UL<<4)	
#define IEEE_TRAP_ENABLE_INE	(1UL<<5)	
#define IEEE_TRAP_ENABLE_DNO	(1UL<<6)	
#define IEEE_TRAP_ENABLE_MASK	(IEEE_TRAP_ENABLE_INV | IEEE_TRAP_ENABLE_DZE |\
				 IEEE_TRAP_ENABLE_OVF | IEEE_TRAP_ENABLE_UNF |\
				 IEEE_TRAP_ENABLE_INE | IEEE_TRAP_ENABLE_DNO)

#define IEEE_MAP_DMZ		(1UL<<12)	
#define IEEE_MAP_UMZ		(1UL<<13)	

#define IEEE_MAP_MASK		(IEEE_MAP_DMZ | IEEE_MAP_UMZ)

#define IEEE_STATUS_INV		(1UL<<17)
#define IEEE_STATUS_DZE		(1UL<<18)
#define IEEE_STATUS_OVF		(1UL<<19)
#define IEEE_STATUS_UNF		(1UL<<20)
#define IEEE_STATUS_INE		(1UL<<21)
#define IEEE_STATUS_DNO		(1UL<<22)

#define IEEE_STATUS_MASK	(IEEE_STATUS_INV | IEEE_STATUS_DZE |	\
				 IEEE_STATUS_OVF | IEEE_STATUS_UNF |	\
				 IEEE_STATUS_INE | IEEE_STATUS_DNO)

#define IEEE_SW_MASK		(IEEE_TRAP_ENABLE_MASK |		\
				 IEEE_STATUS_MASK | IEEE_MAP_MASK)

#define IEEE_CURRENT_RM_SHIFT	32
#define IEEE_CURRENT_RM_MASK	(3UL<<IEEE_CURRENT_RM_SHIFT)

#define IEEE_STATUS_TO_EXCSUM_SHIFT	16

#define IEEE_INHERIT    (1UL<<63)	


static inline unsigned long
ieee_swcr_to_fpcr(unsigned long sw)
{
	unsigned long fp;
	fp = (sw & IEEE_STATUS_MASK) << 35;
	fp |= (sw & IEEE_MAP_DMZ) << 36;
	fp |= (sw & IEEE_STATUS_MASK ? FPCR_SUM : 0);
	fp |= (~sw & (IEEE_TRAP_ENABLE_INV
		      | IEEE_TRAP_ENABLE_DZE
		      | IEEE_TRAP_ENABLE_OVF)) << 48;
	fp |= (~sw & (IEEE_TRAP_ENABLE_UNF | IEEE_TRAP_ENABLE_INE)) << 57;
	fp |= (sw & IEEE_MAP_UMZ ? FPCR_UNDZ | FPCR_UNFD : 0);
	fp |= (~sw & IEEE_TRAP_ENABLE_DNO) << 41;
	return fp;
}

static inline unsigned long
ieee_fpcr_to_swcr(unsigned long fp)
{
	unsigned long sw;
	sw = (fp >> 35) & IEEE_STATUS_MASK;
	sw |= (fp >> 36) & IEEE_MAP_DMZ;
	sw |= (~fp >> 48) & (IEEE_TRAP_ENABLE_INV
			     | IEEE_TRAP_ENABLE_DZE
			     | IEEE_TRAP_ENABLE_OVF);
	sw |= (~fp >> 57) & (IEEE_TRAP_ENABLE_UNF | IEEE_TRAP_ENABLE_INE);
	sw |= (fp >> 47) & IEEE_MAP_UMZ;
	sw |= (~fp >> 41) & IEEE_TRAP_ENABLE_DNO;
	return sw;
}

#ifdef __KERNEL__


static inline unsigned long
rdfpcr(void)
{
	unsigned long tmp, ret;

#if defined(CONFIG_ALPHA_EV6) || defined(CONFIG_ALPHA_EV67)
	__asm__ __volatile__ (
		"ftoit $f0,%0\n\t"
		"mf_fpcr $f0\n\t"
		"ftoit $f0,%1\n\t"
		"itoft %0,$f0"
		: "=r"(tmp), "=r"(ret));
#else
	__asm__ __volatile__ (
		"stt $f0,%0\n\t"
		"mf_fpcr $f0\n\t"
		"stt $f0,%1\n\t"
		"ldt $f0,%0"
		: "=m"(tmp), "=m"(ret));
#endif

	return ret;
}

static inline void
wrfpcr(unsigned long val)
{
	unsigned long tmp;

#if defined(CONFIG_ALPHA_EV6) || defined(CONFIG_ALPHA_EV67)
	__asm__ __volatile__ (
		"ftoit $f0,%0\n\t"
		"itoft %1,$f0\n\t"
		"mt_fpcr $f0\n\t"
		"itoft %0,$f0"
		: "=&r"(tmp) : "r"(val));
#else
	__asm__ __volatile__ (
		"stt $f0,%0\n\t"
		"ldt $f0,%1\n\t"
		"mt_fpcr $f0\n\t"
		"ldt $f0,%0"
		: "=m"(tmp) : "m"(val));
#endif
}

static inline unsigned long
swcr_update_status(unsigned long swcr, unsigned long fpcr)
{
	if (implver() == IMPLVER_EV6) {
		swcr &= ~IEEE_STATUS_MASK;
		swcr |= (fpcr >> 35) & IEEE_STATUS_MASK;
	}
	return swcr;
}

extern unsigned long alpha_read_fp_reg (unsigned long reg);
extern void alpha_write_fp_reg (unsigned long reg, unsigned long val);
extern unsigned long alpha_read_fp_reg_s (unsigned long reg);
extern void alpha_write_fp_reg_s (unsigned long reg, unsigned long val);

#endif 

#endif 
