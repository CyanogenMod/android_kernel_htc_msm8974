/*
 *    Copyright IBM Corp. 1999,2012
 *    Author(s): Hartmut Penner <hp@de.ibm.com>,
 *		 Martin Schwidefsky <schwidefsky@de.ibm.com>,
 *		 Denis Joseph Barrow,
 */

#ifndef _ASM_S390_LOWCORE_H
#define _ASM_S390_LOWCORE_H

#include <linux/types.h>
#include <asm/ptrace.h>
#include <asm/cpu.h>

#ifdef CONFIG_32BIT

#define LC_ORDER 0
#define LC_PAGES 1

struct save_area {
	u32	ext_save;
	u64	timer;
	u64	clk_cmp;
	u8	pad1[24];
	u8	psw[8];
	u32	pref_reg;
	u8	pad2[20];
	u32	acc_regs[16];
	u64	fp_regs[4];
	u32	gp_regs[16];
	u32	ctrl_regs[16];
} __packed;

struct _lowcore {
	psw_t	restart_psw;			
	psw_t	restart_old_psw;		
	__u8	pad_0x0010[0x0014-0x0010];	
	__u32	ipl_parmblock_ptr;		
	psw_t	external_old_psw;		
	psw_t	svc_old_psw;			
	psw_t	program_old_psw;		
	psw_t	mcck_old_psw;			
	psw_t	io_old_psw;			
	__u8	pad_0x0040[0x0058-0x0040];	
	psw_t	external_new_psw;		
	psw_t	svc_new_psw;			
	psw_t	program_new_psw;		
	psw_t	mcck_new_psw;			
	psw_t	io_new_psw;			
	__u32	ext_params;			
	__u16	ext_cpu_addr;			
	__u16	ext_int_code;			
	__u16	svc_ilc;			
	__u16	svc_code;			
	__u16	pgm_ilc;			
	__u16	pgm_code;			
	__u32	trans_exc_code;			
	__u16	mon_class_num;			
	__u16	per_perc_atmid;			
	__u32	per_address;			
	__u32	monitor_code;			
	__u8	exc_access_id;			
	__u8	per_access_id;			
	__u8	op_access_id;			
	__u8	ar_access_id;			
	__u8	pad_0x00a4[0x00b8-0x00a4];	
	__u16	subchannel_id;			
	__u16	subchannel_nr;			
	__u32	io_int_parm;			
	__u32	io_int_word;			
	__u8	pad_0x00c4[0x00c8-0x00c4];	
	__u32	stfl_fac_list;			
	__u8	pad_0x00cc[0x00d4-0x00cc];	
	__u32	extended_save_area_addr;	
	__u32	cpu_timer_save_area[2];		
	__u32	clock_comp_save_area[2];	
	__u32	mcck_interruption_code[2];	
	__u8	pad_0x00f0[0x00f4-0x00f0];	
	__u32	external_damage_code;		
	__u32	failing_storage_address;	
	__u8	pad_0x00fc[0x0100-0x00fc];	
	psw_t	psw_save_area;			
	__u32	prefixreg_save_area;		
	__u8	pad_0x010c[0x0120-0x010c];	

	
	__u32	access_regs_save_area[16];	
	__u32	floating_pt_save_area[8];	
	__u32	gpregs_save_area[16];		
	__u32	cregs_save_area[16];		

	
	__u32	save_area_sync[8];		
	__u32	save_area_async[8];		
	__u32	save_area_restart[1];		
	__u8	pad_0x0244[0x0248-0x0244];	

	
	psw_t	return_psw;			
	psw_t	return_mcck_psw;		

	
	__u64	sync_enter_timer;		
	__u64	async_enter_timer;		
	__u64	mcck_enter_timer;		
	__u64	exit_timer;			
	__u64	user_timer;			
	__u64	system_timer;			
	__u64	steal_timer;			
	__u64	last_update_timer;		
	__u64	last_update_clock;		
	__u64	int_clock;			
	__u64	mcck_clock;			
	__u64	clock_comparator;		

	
	__u32	current_task;			
	__u32	thread_info;			
	__u32	kernel_stack;			

	
	__u32	async_stack;			
	__u32	panic_stack;			
	__u32	restart_stack;			

	
	__u32	restart_fn;			
	__u32	restart_data;			
	__u32	restart_source;			

	
	__u32	kernel_asce;			
	__u32	user_asce;			
	__u32	current_pid;			

	
	__u32	cpu_nr;				
	__u32	softirq_pending;		
	__u32	percpu_offset;			
	__u32	machine_flags;			
	__u32	ftrace_func;			
	__u8	pad_0x02fc[0x0300-0x02fc];	

	
	__u8	irb[64];			

	__u8	pad_0x0340[0x0e00-0x0340];	

	__u32	ipib;				
	__u32	ipib_checksum;			
	__u32	vmcore_info;			
	__u8	pad_0x0e0c[0x0e18-0x0e0c];	
	__u32	os_info;			
	__u8	pad_0x0e1c[0x0f00-0x0e1c];	

	
	__u64	stfle_fac_list[32];		
} __packed;

#else 

#define LC_ORDER 1
#define LC_PAGES 2

struct save_area {
	u64	fp_regs[16];
	u64	gp_regs[16];
	u8	psw[16];
	u8	pad1[8];
	u32	pref_reg;
	u32	fp_ctrl_reg;
	u8	pad2[4];
	u32	tod_reg;
	u64	timer;
	u64	clk_cmp;
	u8	pad3[8];
	u32	acc_regs[16];
	u64	ctrl_regs[16];
} __packed;

struct _lowcore {
	__u8	pad_0x0000[0x0014-0x0000];	
	__u32	ipl_parmblock_ptr;		
	__u8	pad_0x0018[0x0080-0x0018];	
	__u32	ext_params;			
	__u16	ext_cpu_addr;			
	__u16	ext_int_code;			
	__u16	svc_ilc;			
	__u16	svc_code;			
	__u16	pgm_ilc;			
	__u16	pgm_code;			
	__u32	data_exc_code;			
	__u16	mon_class_num;			
	__u16	per_perc_atmid;			
	__u64	per_address;			
	__u8	exc_access_id;			
	__u8	per_access_id;			
	__u8	op_access_id;			
	__u8	ar_access_id;			
	__u8	pad_0x00a4[0x00a8-0x00a4];	
	__u64	trans_exc_code;			
	__u64	monitor_code;			
	__u16	subchannel_id;			
	__u16	subchannel_nr;			
	__u32	io_int_parm;			
	__u32	io_int_word;			
	__u8	pad_0x00c4[0x00c8-0x00c4];	
	__u32	stfl_fac_list;			
	__u8	pad_0x00cc[0x00e8-0x00cc];	
	__u32	mcck_interruption_code[2];	
	__u8	pad_0x00f0[0x00f4-0x00f0];	
	__u32	external_damage_code;		
	__u64	failing_storage_address;	
	__u8	pad_0x0100[0x0110-0x0100];	
	__u64	breaking_event_addr;		
	__u8	pad_0x0118[0x0120-0x0118];	
	psw_t	restart_old_psw;		
	psw_t	external_old_psw;		
	psw_t	svc_old_psw;			
	psw_t	program_old_psw;		
	psw_t	mcck_old_psw;			
	psw_t	io_old_psw;			
	__u8	pad_0x0180[0x01a0-0x0180];	
	psw_t	restart_psw;			
	psw_t	external_new_psw;		
	psw_t	svc_new_psw;			
	psw_t	program_new_psw;		
	psw_t	mcck_new_psw;			
	psw_t	io_new_psw;			

	
	__u64	save_area_sync[8];		
	__u64	save_area_async[8];		
	__u64	save_area_restart[1];		
	__u8	pad_0x0288[0x0290-0x0288];	

	
	psw_t	return_psw;			
	psw_t	return_mcck_psw;		

	
	__u64	sync_enter_timer;		
	__u64	async_enter_timer;		
	__u64	mcck_enter_timer;		
	__u64	exit_timer;			
	__u64	user_timer;			
	__u64	system_timer;			
	__u64	steal_timer;			
	__u64	last_update_timer;		
	__u64	last_update_clock;		
	__u64	int_clock;			
	__u64	mcck_clock;			
	__u64	clock_comparator;		

	
	__u64	current_task;			
	__u64	thread_info;			
	__u64	kernel_stack;			

	
	__u64	async_stack;			
	__u64	panic_stack;			
	__u64	restart_stack;			

	
	__u64	restart_fn;			
	__u64	restart_data;			
	__u64	restart_source;			

	
	__u64	kernel_asce;			
	__u64	user_asce;			
	__u64	current_pid;			

	
	__u32	cpu_nr;				
	__u32	softirq_pending;		
	__u64	percpu_offset;			
	__u64	vdso_per_cpu_data;		
	__u64	machine_flags;			
	__u64	ftrace_func;			
	__u64	gmap;				
	__u8	pad_0x03a0[0x0400-0x03a0];	

	
	__u8	irb[64];			

	
	__u32	paste[16];			

	__u8	pad_0x0480[0x0e00-0x0480];	

	__u64	ipib;				
	__u32	ipib_checksum;			
	__u8	vmcore_info[8];			
	__u8	pad_0x0e14[0x0e18-0x0e14];	
	__u64	os_info;			
	__u8	pad_0x0e20[0x0f00-0x0e20];	

	
	__u64	stfle_fac_list[32];		
	__u8	pad_0x1000[0x11b8-0x1000];	

	
	__u64	ext_params2;			
	__u8	pad_0x11c0[0x1200-0x11C0];	

	
	__u64	floating_pt_save_area[16];	
	__u64	gpregs_save_area[16];		
	psw_t	psw_save_area;			
	__u8	pad_0x1310[0x1318-0x1310];	
	__u32	prefixreg_save_area;		
	__u32	fpt_creg_save_area;		
	__u8	pad_0x1320[0x1324-0x1320];	
	__u32	tod_progreg_save_area;		
	__u32	cpu_timer_save_area[2];		
	__u32	clock_comp_save_area[2];	
	__u8	pad_0x1338[0x1340-0x1338];	
	__u32	access_regs_save_area[16];	
	__u64	cregs_save_area[16];		

	
	__u8	pad_0x1400[0x2000-0x1400];	
} __packed;

#endif 

#define S390_lowcore (*((struct _lowcore *) 0))

extern struct _lowcore *lowcore_ptr[];

static inline void set_prefix(__u32 address)
{
	asm volatile("spx %0" : : "m" (address) : "memory");
}

static inline __u32 store_prefix(void)
{
	__u32 address;

	asm volatile("stpx %0" : "=m" (address));
	return address;
}

#endif 
