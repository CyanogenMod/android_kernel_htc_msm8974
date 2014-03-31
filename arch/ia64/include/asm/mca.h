/*
 * File:	mca.h
 * Purpose:	Machine check handling specific defines
 *
 * Copyright (C) 1999, 2004 Silicon Graphics, Inc.
 * Copyright (C) Vijay Chander <vijay@engr.sgi.com>
 * Copyright (C) Srinivasa Thirumalachar <sprasad@engr.sgi.com>
 * Copyright (C) Russ Anderson <rja@sgi.com>
 */

#ifndef _ASM_IA64_MCA_H
#define _ASM_IA64_MCA_H

#if !defined(__ASSEMBLY__)

#include <linux/interrupt.h>
#include <linux/types.h>

#include <asm/param.h>
#include <asm/sal.h>
#include <asm/processor.h>
#include <asm/mca_asm.h>

#define IA64_MCA_RENDEZ_TIMEOUT		(20 * 1000)	

typedef struct ia64_fptr {
	unsigned long fp;
	unsigned long gp;
} ia64_fptr_t;

typedef union cmcv_reg_u {
	u64	cmcv_regval;
	struct	{
		u64	cmcr_vector		: 8;
		u64	cmcr_reserved1		: 4;
		u64	cmcr_ignored1		: 1;
		u64	cmcr_reserved2		: 3;
		u64	cmcr_mask		: 1;
		u64	cmcr_ignored2		: 47;
	} cmcv_reg_s;

} cmcv_reg_t;

#define cmcv_mask		cmcv_reg_s.cmcr_mask
#define cmcv_vector		cmcv_reg_s.cmcr_vector

enum {
	IA64_MCA_RENDEZ_CHECKIN_NOTDONE	=	0x0,
	IA64_MCA_RENDEZ_CHECKIN_DONE	=	0x1,
	IA64_MCA_RENDEZ_CHECKIN_INIT	=	0x2,
	IA64_MCA_RENDEZ_CHECKIN_CONCURRENT_MCA	=	0x3,
};

typedef struct ia64_mc_info_s {
	u64		imi_mca_handler;
	size_t		imi_mca_handler_size;
	u64		imi_monarch_init_handler;
	size_t		imi_monarch_init_handler_size;
	u64		imi_slave_init_handler;
	size_t		imi_slave_init_handler_size;
	u8		imi_rendez_checkin[NR_CPUS];

} ia64_mc_info_t;


struct ia64_sal_os_state {

	
	unsigned long		os_gp;			
	unsigned long		pal_proc;		
	unsigned long		sal_proc;		
	unsigned long		rv_rc;			
	unsigned long		proc_state_param;	
	unsigned long		monarch;		

	
	unsigned long		sal_ra;			
	unsigned long		sal_gp;			
	pal_min_state_area_t	*pal_min_state;		
	unsigned long		prev_IA64_KR_CURRENT;	
	unsigned long		prev_IA64_KR_CURRENT_STACK;
	struct task_struct	*prev_task;		
	unsigned long		isr;
	unsigned long		ifa;
	unsigned long		itir;
	unsigned long		iipa;
	unsigned long		iim;
	unsigned long		iha;

	
	unsigned long		os_status;		
	unsigned long		context;		

	
	unsigned long		iip;
	unsigned long		ipsr;
	unsigned long		ifs;
};

enum {
	IA64_MCA_CORRECTED	=	0x0,	
	IA64_MCA_WARM_BOOT	=	-1,	
	IA64_MCA_COLD_BOOT	=	-2,	
	IA64_MCA_HALT		=	-3	
};

enum {
	IA64_INIT_RESUME	=	0x0,	
	IA64_INIT_WARM_BOOT	=	-1,	
};

enum {
	IA64_MCA_SAME_CONTEXT	=	0x0,	
	IA64_MCA_NEW_CONTEXT	=	-1	
};


struct ia64_mca_cpu {
	u64 mca_stack[KERNEL_STACK_SIZE/8];
	u64 init_stack[KERNEL_STACK_SIZE/8];
};

extern unsigned long __per_cpu_mca[NR_CPUS];

extern int cpe_vector;
extern int ia64_cpe_irq;
extern void ia64_mca_init(void);
extern void ia64_mca_cpu_init(void *);
extern void ia64_os_mca_dispatch(void);
extern void ia64_os_mca_dispatch_end(void);
extern void ia64_mca_ucmc_handler(struct pt_regs *, struct ia64_sal_os_state *);
extern void ia64_init_handler(struct pt_regs *,
			      struct switch_stack *,
			      struct ia64_sal_os_state *);
extern void ia64_os_init_on_kdump(void);
extern void ia64_monarch_init_handler(void);
extern void ia64_slave_init_handler(void);
extern void ia64_mca_cmc_vector_setup(void);
extern int  ia64_reg_MCA_extension(int (*fn)(void *, struct ia64_sal_os_state *));
extern void ia64_unreg_MCA_extension(void);
extern unsigned long ia64_get_rnat(unsigned long *);
extern void ia64_set_psr_mc(void);
extern void ia64_mca_printk(const char * fmt, ...)
	 __attribute__ ((format (printf, 1, 2)));

struct ia64_mca_notify_die {
	struct ia64_sal_os_state *sos;
	int *monarch_cpu;
	int *data;
};

DECLARE_PER_CPU(u64, ia64_mca_pal_base);

#else	

#define IA64_MCA_CORRECTED	0x0	
#define IA64_MCA_WARM_BOOT	-1	
#define IA64_MCA_COLD_BOOT	-2	
#define IA64_MCA_HALT		-3	

#define IA64_INIT_RESUME	0x0	
#define IA64_INIT_WARM_BOOT	-1	

#define IA64_MCA_SAME_CONTEXT	0x0	
#define IA64_MCA_NEW_CONTEXT	-1	

#endif 
#endif 
