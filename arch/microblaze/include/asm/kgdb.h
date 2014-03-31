#ifdef __KERNEL__
#ifndef __MICROBLAZE_KGDB_H__
#define __MICROBLAZE_KGDB_H__

#ifndef __ASSEMBLY__

#define CACHE_FLUSH_IS_SAFE	1
#define BUFMAX			2048

#define NUMREGBYTES	(57 * 4)

#define BREAK_INSTR_SIZE	4
static inline void arch_kgdb_breakpoint(void)
{
	__asm__ __volatile__("brki r16, 0x18;");
}

#endif 
#endif 
#endif 
