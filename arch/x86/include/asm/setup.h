#ifndef _ASM_X86_SETUP_H
#define _ASM_X86_SETUP_H

#ifdef __KERNEL__

#define COMMAND_LINE_SIZE 2048

#ifdef __i386__

#include <linux/pfn.h>
#define MAXMEM_PFN	PFN_DOWN(MAXMEM)
#define MAX_NONPAE_PFN	(1 << 20)

#endif 

#define PARAM_SIZE 4096		

#define OLD_CL_MAGIC		0xA33F
#define OLD_CL_ADDRESS		0x020	
#define NEW_CL_POINTER		0x228	

#ifndef __ASSEMBLY__
#include <asm/bootparam.h>
#include <asm/x86_init.h>

#ifdef CONFIG_X86_64
void vsmp_init(void);
#else
static inline void vsmp_init(void) { }
#endif

void setup_bios_corruption_check(void);

#ifdef CONFIG_X86_VISWS
extern void visws_early_detect(void);
#else
static inline void visws_early_detect(void) { }
#endif

extern unsigned long saved_video_mode;

extern void reserve_standard_io_resources(void);
extern void i386_reserve_resources(void);
extern void setup_default_timer_irq(void);

#ifdef CONFIG_X86_INTEL_MID
extern void x86_mrst_early_setup(void);
#else
static inline void x86_mrst_early_setup(void) { }
#endif

#ifdef CONFIG_X86_INTEL_CE
extern void x86_ce4100_early_setup(void);
#else
static inline void x86_ce4100_early_setup(void) { }
#endif

#ifndef _SETUP

extern struct boot_params boot_params;

#define LOWMEMSIZE()	(0x9f000)

extern unsigned long _brk_end;
void *extend_brk(size_t size, size_t align);

#define RESERVE_BRK(name,sz)						\
	static void __section(.discard.text) __used notrace		\
	__brk_reservation_fn_##name##__(void) {				\
		asm volatile (						\
			".pushsection .brk_reservation,\"aw\",@nobits;" \
			".brk." #name ":"				\
			" 1:.skip %c0;"					\
			" .size .brk." #name ", . - 1b;"		\
			" .popsection"					\
			: : "i" (sz));					\
	}

#define RESERVE_BRK_ARRAY(type, name, entries)		\
	type *name;					\
	RESERVE_BRK(name, sizeof(type) * entries)

extern void probe_roms(void);
#ifdef __i386__

void __init i386_start_kernel(void);

#else
void __init x86_64_start_kernel(char *real_mode);
void __init x86_64_start_reservations(char *real_mode_data);

#endif 
#endif 
#else
#define RESERVE_BRK(name,sz)				\
	.pushsection .brk_reservation,"aw",@nobits;	\
.brk.name:						\
1:	.skip sz;					\
	.size .brk.name,.-1b;				\
	.popsection
#endif 
#endif  

#endif 
