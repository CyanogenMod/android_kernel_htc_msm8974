/*
 * asm-s390/kvm_host.h - definition for kernel virtual machines on s390
 *
 * Copyright IBM Corp. 2008,2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2 only)
 * as published by the Free Software Foundation.
 *
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 */


#ifndef ASM_KVM_HOST_H
#define ASM_KVM_HOST_H
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kvm_host.h>
#include <asm/debug.h>
#include <asm/cpu.h>

#define KVM_MAX_VCPUS 64
#define KVM_MEMORY_SLOTS 32
#define KVM_PRIVATE_MEM_SLOTS 4

struct sca_entry {
	atomic_t scn;
	__u32	reserved;
	__u64	sda;
	__u64	reserved2[2];
} __attribute__((packed));


struct sca_block {
	__u64	ipte_control;
	__u64	reserved[5];
	__u64	mcn;
	__u64	reserved2;
	struct sca_entry cpu[64];
} __attribute__((packed));

#define KVM_NR_PAGE_SIZES 2
#define KVM_HPAGE_GFN_SHIFT(x) (((x) - 1) * 8)
#define KVM_HPAGE_SHIFT(x) (PAGE_SHIFT + KVM_HPAGE_GFN_SHIFT(x))
#define KVM_HPAGE_SIZE(x) (1UL << KVM_HPAGE_SHIFT(x))
#define KVM_HPAGE_MASK(x)	(~(KVM_HPAGE_SIZE(x) - 1))
#define KVM_PAGES_PER_HPAGE(x)	(KVM_HPAGE_SIZE(x) / PAGE_SIZE)

#define CPUSTAT_STOPPED    0x80000000
#define CPUSTAT_WAIT       0x10000000
#define CPUSTAT_ECALL_PEND 0x08000000
#define CPUSTAT_STOP_INT   0x04000000
#define CPUSTAT_IO_INT     0x02000000
#define CPUSTAT_EXT_INT    0x01000000
#define CPUSTAT_RUNNING    0x00800000
#define CPUSTAT_RETAINED   0x00400000
#define CPUSTAT_TIMING_SUB 0x00020000
#define CPUSTAT_SIE_SUB    0x00010000
#define CPUSTAT_RRF        0x00008000
#define CPUSTAT_SLSV       0x00004000
#define CPUSTAT_SLSR       0x00002000
#define CPUSTAT_ZARCH      0x00000800
#define CPUSTAT_MCDS       0x00000100
#define CPUSTAT_SM         0x00000080
#define CPUSTAT_G          0x00000008
#define CPUSTAT_J          0x00000002
#define CPUSTAT_P          0x00000001

struct kvm_s390_sie_block {
	atomic_t cpuflags;		
	__u32	prefix;			
	__u8	reserved8[32];		
	__u64	cputm;			
	__u64	ckc;			
	__u64	epoch;			
	__u8	reserved40[4];		
#define LCTL_CR0	0x8000
	__u16   lctl;			
	__s16	icpua;			
	__u32	ictl;			
	__u32	eca;			
	__u8	icptcode;		
	__u8	reserved51;		
	__u16	ihcpu;			
	__u8	reserved54[2];		
	__u16	ipa;			
	__u32	ipb;			
	__u32	scaoh;			
	__u8	reserved60;		
	__u8	ecb;			
	__u8	reserved62[2];		
	__u32	scaol;			
	__u8	reserved68[4];		
	__u32	todpr;			
	__u8	reserved70[32];		
	psw_t	gpsw;			
	__u64	gg14;			
	__u64	gg15;			
	__u8	reservedb0[30];		
	__u16   iprcc;			
	__u8	reservedd0[48];		
	__u64	gcr[16];		
	__u64	gbea;			
	__u8	reserved188[24];	
	__u32	fac;			
	__u8	reserved1a4[92];	
} __attribute__((packed));

struct kvm_vcpu_stat {
	u32 exit_userspace;
	u32 exit_null;
	u32 exit_external_request;
	u32 exit_external_interrupt;
	u32 exit_stop_request;
	u32 exit_validity;
	u32 exit_instruction;
	u32 instruction_lctl;
	u32 instruction_lctlg;
	u32 exit_program_interruption;
	u32 exit_instr_and_program;
	u32 deliver_external_call;
	u32 deliver_emergency_signal;
	u32 deliver_service_signal;
	u32 deliver_virtio_interrupt;
	u32 deliver_stop_signal;
	u32 deliver_prefix_signal;
	u32 deliver_restart_signal;
	u32 deliver_program_int;
	u32 exit_wait_state;
	u32 instruction_stidp;
	u32 instruction_spx;
	u32 instruction_stpx;
	u32 instruction_stap;
	u32 instruction_storage_key;
	u32 instruction_stsch;
	u32 instruction_chsc;
	u32 instruction_stsi;
	u32 instruction_stfl;
	u32 instruction_tprot;
	u32 instruction_sigp_sense;
	u32 instruction_sigp_sense_running;
	u32 instruction_sigp_external_call;
	u32 instruction_sigp_emergency;
	u32 instruction_sigp_stop;
	u32 instruction_sigp_arch;
	u32 instruction_sigp_prefix;
	u32 instruction_sigp_restart;
	u32 diagnose_10;
	u32 diagnose_44;
};

struct kvm_s390_io_info {
	__u16        subchannel_id;            
	__u16        subchannel_nr;            
	__u32        io_int_parm;              
	__u32        io_int_word;              
};

struct kvm_s390_ext_info {
	__u32 ext_params;
	__u64 ext_params2;
};

#define PGM_OPERATION            0x01
#define PGM_PRIVILEGED_OPERATION 0x02
#define PGM_EXECUTE              0x03
#define PGM_PROTECTION           0x04
#define PGM_ADDRESSING           0x05
#define PGM_SPECIFICATION        0x06
#define PGM_DATA                 0x07

struct kvm_s390_pgm_info {
	__u16 code;
};

struct kvm_s390_prefix_info {
	__u32 address;
};

struct kvm_s390_extcall_info {
	__u16 code;
};

struct kvm_s390_emerg_info {
	__u16 code;
};

struct kvm_s390_interrupt_info {
	struct list_head list;
	u64	type;
	union {
		struct kvm_s390_io_info io;
		struct kvm_s390_ext_info ext;
		struct kvm_s390_pgm_info pgm;
		struct kvm_s390_emerg_info emerg;
		struct kvm_s390_extcall_info extcall;
		struct kvm_s390_prefix_info prefix;
	};
};

#define ACTION_STORE_ON_STOP		(1<<0)
#define ACTION_STOP_ON_STOP		(1<<1)
#define ACTION_RELOADVCPU_ON_STOP	(1<<2)

struct kvm_s390_local_interrupt {
	spinlock_t lock;
	struct list_head list;
	atomic_t active;
	struct kvm_s390_float_interrupt *float_int;
	int timer_due; 
	wait_queue_head_t wq;
	atomic_t *cpuflags;
	unsigned int action_bits;
};

struct kvm_s390_float_interrupt {
	spinlock_t lock;
	struct list_head list;
	atomic_t active;
	int next_rr_cpu;
	unsigned long idle_mask[(KVM_MAX_VCPUS + sizeof(long) - 1)
				/ sizeof(long)];
	struct kvm_s390_local_interrupt *local_int[KVM_MAX_VCPUS];
};


struct kvm_vcpu_arch {
	struct kvm_s390_sie_block *sie_block;
	s390_fp_regs      host_fpregs;
	unsigned int      host_acrs[NUM_ACRS];
	s390_fp_regs      guest_fpregs;
	struct kvm_s390_local_interrupt local_int;
	struct hrtimer    ckc_timer;
	struct tasklet_struct tasklet;
	union  {
		struct cpuid	cpu_id;
		u64		stidp_data;
	};
	struct gmap *gmap;
};

struct kvm_vm_stat {
	u32 remote_tlb_flush;
};

struct kvm_arch_memory_slot {
};

struct kvm_arch{
	struct sca_block *sca;
	debug_info_t *dbf;
	struct kvm_s390_float_interrupt float_int;
	struct gmap *gmap;
};

extern int sie64a(struct kvm_s390_sie_block *, u64 *);
#endif
