#ifndef _ASM_X86_SEGMENT_H
#define _ASM_X86_SEGMENT_H

#include <linux/const.h>

#define GDT_ENTRY(flags, base, limit)			\
	((((base)  & _AC(0xff000000,ULL)) << (56-24)) |	\
	 (((flags) & _AC(0x0000f0ff,ULL)) << 40) |	\
	 (((limit) & _AC(0x000f0000,ULL)) << (48-16)) |	\
	 (((base)  & _AC(0x00ffffff,ULL)) << 16) |	\
	 (((limit) & _AC(0x0000ffff,ULL))))


#define GDT_ENTRY_BOOT_CS	2
#define __BOOT_CS		(GDT_ENTRY_BOOT_CS * 8)

#define GDT_ENTRY_BOOT_DS	(GDT_ENTRY_BOOT_CS + 1)
#define __BOOT_DS		(GDT_ENTRY_BOOT_DS * 8)

#define GDT_ENTRY_BOOT_TSS	(GDT_ENTRY_BOOT_CS + 2)
#define __BOOT_TSS		(GDT_ENTRY_BOOT_TSS * 8)

#ifdef CONFIG_X86_32
#define GDT_ENTRY_TLS_MIN	6
#define GDT_ENTRY_TLS_MAX 	(GDT_ENTRY_TLS_MIN + GDT_ENTRY_TLS_ENTRIES - 1)

#define GDT_ENTRY_DEFAULT_USER_CS	14

#define GDT_ENTRY_DEFAULT_USER_DS	15

#define GDT_ENTRY_KERNEL_BASE		(12)

#define GDT_ENTRY_KERNEL_CS		(GDT_ENTRY_KERNEL_BASE+0)

#define GDT_ENTRY_KERNEL_DS		(GDT_ENTRY_KERNEL_BASE+1)

#define GDT_ENTRY_TSS			(GDT_ENTRY_KERNEL_BASE+4)
#define GDT_ENTRY_LDT			(GDT_ENTRY_KERNEL_BASE+5)

#define GDT_ENTRY_PNPBIOS_BASE		(GDT_ENTRY_KERNEL_BASE+6)
#define GDT_ENTRY_APMBIOS_BASE		(GDT_ENTRY_KERNEL_BASE+11)

#define GDT_ENTRY_ESPFIX_SS		(GDT_ENTRY_KERNEL_BASE+14)
#define __ESPFIX_SS			(GDT_ENTRY_ESPFIX_SS*8)

#define GDT_ENTRY_PERCPU		(GDT_ENTRY_KERNEL_BASE+15)
#ifdef CONFIG_SMP
#define __KERNEL_PERCPU (GDT_ENTRY_PERCPU * 8)
#else
#define __KERNEL_PERCPU 0
#endif

#define GDT_ENTRY_STACK_CANARY		(GDT_ENTRY_KERNEL_BASE+16)
#ifdef CONFIG_CC_STACKPROTECTOR
#define __KERNEL_STACK_CANARY		(GDT_ENTRY_STACK_CANARY*8)
#else
#define __KERNEL_STACK_CANARY		0
#endif

#define GDT_ENTRY_DOUBLEFAULT_TSS	31

#define GDT_ENTRIES 32

#define GDT_ENTRY_PNPBIOS_CS32		(GDT_ENTRY_PNPBIOS_BASE + 0)
#define GDT_ENTRY_PNPBIOS_CS16		(GDT_ENTRY_PNPBIOS_BASE + 1)
#define GDT_ENTRY_PNPBIOS_DS		(GDT_ENTRY_PNPBIOS_BASE + 2)
#define GDT_ENTRY_PNPBIOS_TS1		(GDT_ENTRY_PNPBIOS_BASE + 3)
#define GDT_ENTRY_PNPBIOS_TS2		(GDT_ENTRY_PNPBIOS_BASE + 4)

#define PNP_CS32   (GDT_ENTRY_PNPBIOS_CS32 * 8)	
#define PNP_CS16   (GDT_ENTRY_PNPBIOS_CS16 * 8)	
#define PNP_DS     (GDT_ENTRY_PNPBIOS_DS * 8)	
#define PNP_TS1    (GDT_ENTRY_PNPBIOS_TS1 * 8)	
#define PNP_TS2    (GDT_ENTRY_PNPBIOS_TS2 * 8)	

#define SEGMENT_RPL_MASK	0x3
#define SEGMENT_TI_MASK		0x4

#define USER_RPL		0x3
#define SEGMENT_LDT		0x4
#define SEGMENT_GDT		0x0


#define SEGMENT_IS_PNP_CODE(x)   (((x) & 0xf4) == GDT_ENTRY_PNPBIOS_BASE * 8)


#else
#include <asm/cache.h>

#define GDT_ENTRY_KERNEL32_CS 1
#define GDT_ENTRY_KERNEL_CS 2
#define GDT_ENTRY_KERNEL_DS 3

#define __KERNEL32_CS   (GDT_ENTRY_KERNEL32_CS * 8)

#define GDT_ENTRY_DEFAULT_USER32_CS 4
#define GDT_ENTRY_DEFAULT_USER_DS 5
#define GDT_ENTRY_DEFAULT_USER_CS 6
#define __USER32_CS   (GDT_ENTRY_DEFAULT_USER32_CS*8+3)
#define __USER32_DS	__USER_DS

#define GDT_ENTRY_TSS 8	
#define GDT_ENTRY_LDT 10 
#define GDT_ENTRY_TLS_MIN 12
#define GDT_ENTRY_TLS_MAX 14

#define GDT_ENTRY_PER_CPU 15	
#define __PER_CPU_SEG	(GDT_ENTRY_PER_CPU * 8 + 3)

#define FS_TLS 0
#define GS_TLS 1

#define GS_TLS_SEL ((GDT_ENTRY_TLS_MIN+GS_TLS)*8 + 3)
#define FS_TLS_SEL ((GDT_ENTRY_TLS_MIN+FS_TLS)*8 + 3)

#define GDT_ENTRIES 16

#endif

#define __KERNEL_CS	(GDT_ENTRY_KERNEL_CS*8)
#define __KERNEL_DS	(GDT_ENTRY_KERNEL_DS*8)
#define __USER_DS	(GDT_ENTRY_DEFAULT_USER_DS*8+3)
#define __USER_CS	(GDT_ENTRY_DEFAULT_USER_CS*8+3)
#ifndef CONFIG_PARAVIRT
#define get_kernel_rpl()  0
#endif

#define USER_RPL		0x3
#define SEGMENT_LDT		0x4
#define SEGMENT_GDT		0x0

#define SEGMENT_RPL_MASK	0x3
#define SEGMENT_TI_MASK		0x4

#define IDT_ENTRIES 256
#define NUM_EXCEPTION_VECTORS 32
#define GDT_SIZE (GDT_ENTRIES * 8)
#define GDT_ENTRY_TLS_ENTRIES 3
#define TLS_SIZE (GDT_ENTRY_TLS_ENTRIES * 8)

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
extern const char early_idt_handlers[NUM_EXCEPTION_VECTORS][10];

#define loadsegment(seg, value)						\
do {									\
	unsigned short __val = (value);					\
									\
	asm volatile("						\n"	\
		     "1:	movl %k0,%%" #seg "		\n"	\
									\
		     ".section .fixup,\"ax\"			\n"	\
		     "2:	xorl %k0,%k0			\n"	\
		     "		jmp 1b				\n"	\
		     ".previous					\n"	\
									\
		     _ASM_EXTABLE(1b, 2b)				\
									\
		     : "+r" (__val) : : "memory");			\
} while (0)

#define savesegment(seg, value)				\
	asm("mov %%" #seg ",%0":"=r" (value) : : "memory")

#ifdef CONFIG_X86_32
#ifdef CONFIG_X86_32_LAZY_GS
#define get_user_gs(regs)	(u16)({unsigned long v; savesegment(gs, v); v;})
#define set_user_gs(regs, v)	loadsegment(gs, (unsigned long)(v))
#define task_user_gs(tsk)	((tsk)->thread.gs)
#define lazy_save_gs(v)		savesegment(gs, (v))
#define lazy_load_gs(v)		loadsegment(gs, (v))
#else	
#define get_user_gs(regs)	(u16)((regs)->gs)
#define set_user_gs(regs, v)	do { (regs)->gs = (v); } while (0)
#define task_user_gs(tsk)	(task_pt_regs(tsk)->gs)
#define lazy_save_gs(v)		do { } while (0)
#define lazy_load_gs(v)		do { } while (0)
#endif	
#endif	

static inline unsigned long get_limit(unsigned long segment)
{
	unsigned long __limit;
	asm("lsll %1,%0" : "=r" (__limit) : "r" (segment));
	return __limit + 1;
}

#endif 
#endif 

#endif 
