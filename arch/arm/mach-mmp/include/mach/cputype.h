#ifndef __ASM_MACH_CPUTYPE_H
#define __ASM_MACH_CPUTYPE_H

#include <asm/cputype.h>


extern unsigned int mmp_chip_id;

#ifdef CONFIG_CPU_PXA168
static inline int cpu_is_pxa168(void)
{
	return (((read_cpuid_id() >> 8) & 0xff) == 0x84) &&
		((mmp_chip_id & 0xfff) == 0x168);
}
#else
#define cpu_is_pxa168()	(0)
#endif

#ifdef CONFIG_CPU_PXA910
static inline int cpu_is_pxa910(void)
{
	return (((read_cpuid_id() >> 8) & 0xff) == 0x84) &&
		(((mmp_chip_id & 0xfff) == 0x910) ||
		 ((mmp_chip_id & 0xfff) == 0x920));
}
#else
#define cpu_is_pxa910()	(0)
#endif

#ifdef CONFIG_CPU_MMP2
static inline int cpu_is_mmp2(void)
{
	return (((read_cpuid_id() >> 8) & 0xff) == 0x58);
}
#else
#define cpu_is_mmp2()	(0)
#endif

#endif 
