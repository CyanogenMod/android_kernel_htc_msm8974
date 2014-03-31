#ifndef _ASM_POWERPC_KEXEC_H
#define _ASM_POWERPC_KEXEC_H
#ifdef __KERNEL__

#if defined(CONFIG_FSL_BOOKE) || defined(CONFIG_44x)

#define KEXEC_SOURCE_MEMORY_LIMIT	(2 * 1024 * 1024 * 1024UL - 1)
#define KEXEC_DESTINATION_MEMORY_LIMIT	(2 * 1024 * 1024 * 1024UL - 1)
#define KEXEC_CONTROL_MEMORY_LIMIT	(2 * 1024 * 1024 * 1024UL - 1)

#else

#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)

#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)

#ifdef __powerpc64__
#define KEXEC_CONTROL_MEMORY_LIMIT (-1UL)
#else
#define KEXEC_CONTROL_MEMORY_LIMIT TASK_SIZE
#endif
#endif

#define KEXEC_CONTROL_PAGE_SIZE 4096

#ifdef __powerpc64__
#define KEXEC_ARCH KEXEC_ARCH_PPC64
#else
#define KEXEC_ARCH KEXEC_ARCH_PPC
#endif

#define KEXEC_STATE_NONE 0
#define KEXEC_STATE_IRQS_OFF 1
#define KEXEC_STATE_REAL_MODE 2

#ifndef __ASSEMBLY__
#include <asm/reg.h>

typedef void (*crash_shutdown_t)(void);

#ifdef CONFIG_KEXEC

static inline void crash_setup_regs(struct pt_regs *newregs,
					struct pt_regs *oldregs)
{
	if (oldregs)
		memcpy(newregs, oldregs, sizeof(*newregs));
	else
		ppc_save_regs(newregs);
}

extern void kexec_smp_wait(void);	
extern int crashing_cpu;
extern void crash_send_ipi(void (*crash_ipi_callback)(struct pt_regs *));

struct kimage;
struct pt_regs;
extern void default_machine_kexec(struct kimage *image);
extern int default_machine_kexec_prepare(struct kimage *image);
extern void default_machine_crash_shutdown(struct pt_regs *regs);
extern int crash_shutdown_register(crash_shutdown_t handler);
extern int crash_shutdown_unregister(crash_shutdown_t handler);

extern void machine_kexec_simple(struct kimage *image);
extern void crash_kexec_secondary(struct pt_regs *regs);
extern int overlaps_crashkernel(unsigned long start, unsigned long size);
extern void reserve_crashkernel(void);
extern void machine_kexec_mask_interrupts(void);

#else 
static inline void crash_kexec_secondary(struct pt_regs *regs) { }

static inline int overlaps_crashkernel(unsigned long start, unsigned long size)
{
	return 0;
}

static inline void reserve_crashkernel(void) { ; }

static inline int crash_shutdown_register(crash_shutdown_t handler)
{
	return 0;
}

static inline int crash_shutdown_unregister(crash_shutdown_t handler)
{
	return 0;
}

#endif 
#endif 
#endif 
#endif 
