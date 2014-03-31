/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Copyright IBM Corp. 2007
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 */

#ifndef __LINUX_KVM_POWERPC_H
#define __LINUX_KVM_POWERPC_H

#include <linux/types.h>

#define __KVM_HAVE_SPAPR_TCE
#define __KVM_HAVE_PPC_SMT

struct kvm_regs {
	__u64 pc;
	__u64 cr;
	__u64 ctr;
	__u64 lr;
	__u64 xer;
	__u64 msr;
	__u64 srr0;
	__u64 srr1;
	__u64 pid;

	__u64 sprg0;
	__u64 sprg1;
	__u64 sprg2;
	__u64 sprg3;
	__u64 sprg4;
	__u64 sprg5;
	__u64 sprg6;
	__u64 sprg7;

	__u64 gpr[32];
};

#define KVM_SREGS_E_IMPL_NONE	0
#define KVM_SREGS_E_IMPL_FSL	1

#define KVM_SREGS_E_FSL_PIDn	(1 << 0) 

#define KVM_SREGS_E_BASE		(1 << 0)

#define KVM_SREGS_E_ARCH206		(1 << 1)

#define KVM_SREGS_E_64			(1 << 2)

#define KVM_SREGS_E_SPRG8		(1 << 3)
#define KVM_SREGS_E_MCIVPR		(1 << 4)

#define KVM_SREGS_E_IVOR		(1 << 5)

#define KVM_SREGS_E_ARCH206_MMU		(1 << 6)

#define KVM_SREGS_E_DEBUG		(1 << 7)

#define KVM_SREGS_E_ED			(1 << 8)

#define KVM_SREGS_E_SPE			(1 << 9)

#define KVM_SREGS_EXP			(1 << 10)

#define KVM_SREGS_E_PD			(1 << 11)

#define KVM_SREGS_E_PC			(1 << 12)

#define KVM_SREGS_E_PT			(1 << 13)

#define KVM_SREGS_E_PM			(1 << 14)

#define KVM_SREGS_E_UPDATE_MCSR		(1 << 0)
#define KVM_SREGS_E_UPDATE_TSR		(1 << 1)
#define KVM_SREGS_E_UPDATE_DEC		(1 << 2)
#define KVM_SREGS_E_UPDATE_DBSR		(1 << 3)

struct kvm_sregs {
	__u32 pvr;
	union {
		struct {
			__u64 sdr1;
			struct {
				struct {
					__u64 slbe;
					__u64 slbv;
				} slb[64];
			} ppc64;
			struct {
				__u32 sr[16];
				__u64 ibat[8];
				__u64 dbat[8];
			} ppc32;
		} s;
		struct {
			union {
				struct { 
					__u32 features; 
					__u32 svr;
					__u64 mcar;
					__u32 hid0;

					
					__u32 pid1, pid2;
				} fsl;
				__u8 pad[256];
			} impl;

			__u32 features; 
			__u32 impl_id;	
			__u32 update_special; 
			__u32 pir;	
			__u64 sprg8;
			__u64 sprg9;	
			__u64 csrr0;
			__u64 dsrr0;	
			__u64 mcsrr0;
			__u32 csrr1;
			__u32 dsrr1;	
			__u32 mcsrr1;
			__u32 esr;
			__u64 dear;
			__u64 ivpr;
			__u64 mcivpr;
			__u64 mcsr;	

			__u32 tsr;	
			__u32 tcr;
			__u32 decar;
			__u32 dec;	

			__u64 tb;

			__u32 dbsr;	
			__u32 dbcr[3];
			__u32 iac[4];
			__u32 dac[2];
			__u32 dvc[2];
			__u8 num_iac;	
			__u8 num_dac;	
			__u8 num_dvc;	
			__u8 pad;

			__u32 epr;	
			__u32 vrsave;	
			__u32 epcr;	

			__u32 mas0;
			__u32 mas1;
			__u64 mas2;
			__u64 mas7_3;
			__u32 mas4;
			__u32 mas6;

			__u32 ivor_low[16]; 
			__u32 ivor_high[18]; 

			__u32 mmucfg;	
			__u32 eptcfg;	
			__u32 tlbcfg[4];
			__u32 tlbps[4]; 

			__u32 eplc, epsc; 
		} e;
		__u8 pad[1020];
	} u;
};

struct kvm_fpu {
	__u64 fpr[32];
};

struct kvm_debug_exit_arch {
};

struct kvm_guest_debug_arch {
};

struct kvm_sync_regs {
};

#define KVM_INTERRUPT_SET	-1U
#define KVM_INTERRUPT_UNSET	-2U
#define KVM_INTERRUPT_SET_LEVEL	-3U

#define KVM_CPU_440		1
#define KVM_CPU_E500V2		2
#define KVM_CPU_3S_32		3
#define KVM_CPU_3S_64		4

struct kvm_create_spapr_tce {
	__u64 liobn;
	__u32 window_size;
};

struct kvm_allocate_rma {
	__u64 rma_size;
};

struct kvm_book3e_206_tlb_entry {
	__u32 mas8;
	__u32 mas1;
	__u64 mas2;
	__u64 mas7_3;
};

struct kvm_book3e_206_tlb_params {
	__u32 tlb_sizes[4];
	__u32 tlb_ways[4];
	__u32 reserved[8];
};

#define KVM_REG_PPC_HIOR	(KVM_REG_PPC | KVM_REG_SIZE_U64 | 0x1)

#endif 
