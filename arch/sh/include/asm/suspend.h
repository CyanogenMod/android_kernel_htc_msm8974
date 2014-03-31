#ifndef _ASM_SH_SUSPEND_H
#define _ASM_SH_SUSPEND_H

#ifndef __ASSEMBLY__
#include <linux/notifier.h>

#include <asm/ptrace.h>

struct swsusp_arch_regs {
	struct pt_regs user_regs;
	unsigned long bank1_regs[8];
};

void sh_mobile_call_standby(unsigned long mode);

#ifdef CONFIG_CPU_IDLE
void sh_mobile_setup_cpuidle(void);
#else
static inline void sh_mobile_setup_cpuidle(void) {}
#endif

extern struct atomic_notifier_head sh_mobile_pre_sleep_notifier_list;
extern struct atomic_notifier_head sh_mobile_post_sleep_notifier_list;

#define SH_MOBILE_SLEEP_BOARD	0
#define SH_MOBILE_SLEEP_CPU	1
#define SH_MOBILE_PRE(x)	(x)
#define SH_MOBILE_POST(x)	(-(x))

void sh_mobile_register_self_refresh(unsigned long flags,
				     void *pre_start, void *pre_end,
				     void *post_start, void *post_end);

struct sh_sleep_regs {
	unsigned long stbcr;
	unsigned long bar;

	
	unsigned long pteh;
	unsigned long ptel;
	unsigned long ttb;
	unsigned long tea;
	unsigned long mmucr;
	unsigned long ptea;
	unsigned long pascr;
	unsigned long irmcr;

	
	unsigned long ccr;
	unsigned long ramcr;
};

struct sh_sleep_data {
	
	unsigned long mode;

	
	unsigned long sf_pre;
	unsigned long sf_post;

	
	unsigned long resume;

	
	unsigned long vbr;
	unsigned long spc;
	unsigned long sr;
	unsigned long sp;

	
	struct sh_sleep_regs addr;

	
	struct sh_sleep_regs data;
};

extern unsigned long sh_mobile_sleep_supported;

#endif

#define SUSP_SH_SLEEP		(1 << 0) 
#define SUSP_SH_STANDBY		(1 << 1) 
#define SUSP_SH_RSTANDBY	(1 << 2) 
#define SUSP_SH_USTANDBY	(1 << 3) 
#define SUSP_SH_SF		(1 << 4) 
#define SUSP_SH_MMU		(1 << 5) 
#define SUSP_SH_REGS		(1 << 6) 

#endif 
