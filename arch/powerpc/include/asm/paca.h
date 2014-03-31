/*
 * This control block defines the PACA which defines the processor
 * specific data for each logical processor on the system.
 * There are some pointers defined that are utilized by PLIC.
 *
 * C 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_POWERPC_PACA_H
#define _ASM_POWERPC_PACA_H
#ifdef __KERNEL__

#ifdef CONFIG_PPC64

#include <linux/init.h>
#include <asm/types.h>
#include <asm/lppaca.h>
#include <asm/mmu.h>
#include <asm/page.h>
#include <asm/exception-64e.h>
#ifdef CONFIG_KVM_BOOK3S_64_HANDLER
#include <asm/kvm_book3s_asm.h>
#endif

register struct paca_struct *local_paca asm("r13");

#if defined(CONFIG_DEBUG_PREEMPT) && defined(CONFIG_SMP)
extern unsigned int debug_smp_processor_id(void); 
#define get_paca()	((void) debug_smp_processor_id(), local_paca)
#else
#define get_paca()	local_paca
#endif

#define get_lppaca()	(get_paca()->lppaca_ptr)
#define get_slb_shadow()	(get_paca()->slb_shadow_ptr)

struct task_struct;
struct opal_machine_check_event;

struct paca_struct {
#ifdef CONFIG_PPC_BOOK3S

	struct lppaca *lppaca_ptr;	
#endif 
	u16 lock_token;			
	u16 paca_index;			

	u64 kernel_toc;			
	u64 kernelbase;			
	u64 kernel_msr;			
#ifdef CONFIG_PPC_STD_MMU_64
	u64 stab_real;			
	u64 stab_addr;			
#endif 
	void *emergency_sp;		
	u64 data_offset;		
	s16 hw_cpu_id;			
	u8 cpu_start;			
					
	u8 kexec_state;		
#ifdef CONFIG_PPC_STD_MMU_64
	struct slb_shadow *slb_shadow_ptr;
	struct dtl_entry *dispatch_log;
	struct dtl_entry *dispatch_log_end;

	
	u64 exgen[11] __attribute__((aligned(0x80)));
	u64 exmc[11];		
	u64 exslb[11];		
	
	u16 vmalloc_sllp;
	u16 slb_cache_ptr;
	u16 slb_cache[SLB_CACHE_ENTRIES];
#endif 

#ifdef CONFIG_PPC_BOOK3E
	u64 exgen[8] __attribute__((aligned(0x80)));
	
	pgd_t *pgd __attribute__((aligned(0x80))); 
	pgd_t *kernel_pgd;		
	
	u64 extlb[3][EX_TLB_SIZE / sizeof(u64)];
	u64 exmc[8];		
	u64 excrit[8];		
	u64 exdbg[8];		

	
	void *mc_kstack;
	void *crit_kstack;
	void *dbg_kstack;
#endif 

	mm_context_t context;

	struct task_struct *__current;	
	u64 kstack;			
	u64 stab_rr;			
	u64 saved_r1;			
	u64 saved_msr;			
	u16 trap_save;			
	u8 soft_enabled;		
	u8 irq_happened;		
	u8 io_sync;			
	u8 irq_work_pending;		
	u8 nap_state_lost;		

#ifdef CONFIG_PPC_POWERNV
	struct opal_machine_check_event *opal_mc_evt;
#endif

	
	u64 user_time;			
	u64 system_time;		
	u64 user_time_scaled;		
	u64 starttime;			
	u64 starttime_user;		
	u64 startspurr;			
	u64 utime_sspurr;		
	u64 stolen_time;		
	u64 dtl_ridx;			
	struct dtl_entry *dtl_curr;	

#ifdef CONFIG_KVM_BOOK3S_HANDLER
#ifdef CONFIG_KVM_BOOK3S_PR
	
	struct kvmppc_book3s_shadow_vcpu shadow_vcpu;
#endif
	struct kvmppc_host_state kvm_hstate;
#endif
};

extern struct paca_struct *paca;
extern __initdata struct paca_struct boot_paca;
extern void initialise_paca(struct paca_struct *new_paca, int cpu);
extern void setup_paca(struct paca_struct *new_paca);
extern void allocate_pacas(void);
extern void free_unused_pacas(void);

#else 

static inline void allocate_pacas(void) { };
static inline void free_unused_pacas(void) { };

#endif 

#endif 
#endif 
