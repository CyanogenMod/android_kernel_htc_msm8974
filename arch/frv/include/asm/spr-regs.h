/* spr-regs.h: special-purpose registers on the FRV
 *
 * Copyright (C) 2003, 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_SPR_REGS_H
#define _ASM_SPR_REGS_H

#define PSR_ET			0x00000001	
#define PSR_PS			0x00000002	
#define PSR_S			0x00000004	
#define PSR_PIL			0x00000078	
#define PSR_PIL_0		0x00000000	
#define PSR_PIL_13		0x00000068	
#define PSR_PIL_14		0x00000070	
#define PSR_PIL_15		0x00000078	
#define PSR_EM			0x00000080	
#define PSR_EF			0x00000100	
#define PSR_BE			0x00001000	
#define PSR_BE_LE		0x00000000	
#define PSR_BE_BE		0x00001000	
#define PSR_CM			0x00002000	
#define PSR_NEM			0x00004000	
#define PSR_ICE			0x00010000	
#define PSR_VERSION_SHIFT	24		
#define PSR_IMPLE_SHIFT		28		

#define PSR_VERSION(psr)	(((psr) >> PSR_VERSION_SHIFT) & 0xf)
#define PSR_IMPLE(psr)		(((psr) >> PSR_IMPLE_SHIFT) & 0xf)

#define PSR_IMPLE_FR401		0x2
#define PSR_VERSION_FR401_MB93401	0x0
#define PSR_VERSION_FR401_MB93401A	0x1
#define PSR_VERSION_FR401_MB93403	0x2

#define PSR_IMPLE_FR405		0x4
#define PSR_VERSION_FR405_MB93405	0x0

#define PSR_IMPLE_FR451		0x5
#define PSR_VERSION_FR451_MB93451	0x0

#define PSR_IMPLE_FR501		0x1
#define PSR_VERSION_FR501_MB93501	0x1
#define PSR_VERSION_FR501_MB93501A	0x2

#define PSR_IMPLE_FR551		0x3
#define PSR_VERSION_FR551_MB93555	0x1

#define __get_PSR()	({ unsigned long x; asm volatile("movsg psr,%0" : "=r"(x)); x; })
#define __set_PSR(V)	do { asm volatile("movgs %0,psr" : : "r"(V)); } while(0)

#define TBR_TT			0x00000ff0
#define TBR_TT_INSTR_MMU_MISS	(0x01 << 4)
#define TBR_TT_INSTR_ACC_ERROR	(0x02 << 4)
#define TBR_TT_INSTR_ACC_EXCEP	(0x03 << 4)
#define TBR_TT_PRIV_INSTR	(0x06 << 4)
#define TBR_TT_ILLEGAL_INSTR	(0x07 << 4)
#define TBR_TT_FP_EXCEPTION	(0x0d << 4)
#define TBR_TT_MP_EXCEPTION	(0x0e << 4)
#define TBR_TT_DATA_ACC_ERROR	(0x11 << 4)
#define TBR_TT_DATA_MMU_MISS	(0x12 << 4)
#define TBR_TT_DATA_ACC_EXCEP	(0x13 << 4)
#define TBR_TT_DATA_STR_ERROR	(0x14 << 4)
#define TBR_TT_DIVISION_EXCEP	(0x17 << 4)
#define TBR_TT_COMMIT_EXCEP	(0x19 << 4)
#define TBR_TT_INSTR_TLB_MISS	(0x1a << 4)
#define TBR_TT_DATA_TLB_MISS	(0x1b << 4)
#define TBR_TT_DATA_DAT_EXCEP	(0x1d << 4)
#define TBR_TT_DECREMENT_TIMER	(0x1f << 4)
#define TBR_TT_COMPOUND_EXCEP	(0x20 << 4)
#define TBR_TT_INTERRUPT_1	(0x21 << 4)
#define TBR_TT_INTERRUPT_2	(0x22 << 4)
#define TBR_TT_INTERRUPT_3	(0x23 << 4)
#define TBR_TT_INTERRUPT_4	(0x24 << 4)
#define TBR_TT_INTERRUPT_5	(0x25 << 4)
#define TBR_TT_INTERRUPT_6	(0x26 << 4)
#define TBR_TT_INTERRUPT_7	(0x27 << 4)
#define TBR_TT_INTERRUPT_8	(0x28 << 4)
#define TBR_TT_INTERRUPT_9	(0x29 << 4)
#define TBR_TT_INTERRUPT_10	(0x2a << 4)
#define TBR_TT_INTERRUPT_11	(0x2b << 4)
#define TBR_TT_INTERRUPT_12	(0x2c << 4)
#define TBR_TT_INTERRUPT_13	(0x2d << 4)
#define TBR_TT_INTERRUPT_14	(0x2e << 4)
#define TBR_TT_INTERRUPT_15	(0x2f << 4)
#define TBR_TT_TRAP0		(0x80 << 4)
#define TBR_TT_TRAP1		(0x81 << 4)
#define TBR_TT_TRAP2		(0x82 << 4)
#define TBR_TT_TRAP3		(0x83 << 4)
#define TBR_TT_TRAP120		(0xf8 << 4)
#define TBR_TT_TRAP121		(0xf9 << 4)
#define TBR_TT_TRAP122		(0xfa << 4)
#define TBR_TT_TRAP123		(0xfb << 4)
#define TBR_TT_TRAP124		(0xfc << 4)
#define TBR_TT_TRAP125		(0xfd << 4)
#define TBR_TT_TRAP126		(0xfe << 4)
#define TBR_TT_BREAK		(0xff << 4)

#define TBR_TT_ATOMIC_CMPXCHG32	TBR_TT_TRAP120
#define TBR_TT_ATOMIC_XCHG32	TBR_TT_TRAP121
#define TBR_TT_ATOMIC_XOR	TBR_TT_TRAP122
#define TBR_TT_ATOMIC_OR	TBR_TT_TRAP123
#define TBR_TT_ATOMIC_AND	TBR_TT_TRAP124
#define TBR_TT_ATOMIC_SUB	TBR_TT_TRAP125
#define TBR_TT_ATOMIC_ADD	TBR_TT_TRAP126

#define __get_TBR()	({ unsigned long x; asm volatile("movsg tbr,%0" : "=r"(x)); x; })

#define HSR0_PDM		0x00000007	
#define HSR0_PDM_NORMAL		0x00000000	
#define HSR0_PDM_CORE_SLEEP	0x00000001	
#define HSR0_PDM_BUS_SLEEP	0x00000003	
#define HSR0_PDM_PLL_RUN	0x00000005	
#define HSR0_PDM_PLL_STOP	0x00000007	
#define HSR0_GRLE		0x00000040	
#define HSR0_GRHE		0x00000080	
#define HSR0_FRLE		0x00000100	
#define HSR0_FRHE		0x00000200	
#define HSR0_GRN		0x00000400	
#define HSR0_GRN_64		0x00000000	
#define HSR0_GRN_32		0x00000400	
#define HSR0_FRN		0x00000800	
#define HSR0_FRN_64		0x00000000	
#define HSR0_FRN_32		0x00000800	
#define HSR0_SA			0x00001000	
#define HSR0_ETMI		0x00008000	
#define HSR0_ETMD		0x00004000	
#define HSR0_PEDAT		0x00010000	
#define HSR0_XEDAT		0x00020000	
#define HSR0_EDAT		0x00080000	
#define HSR0_RME		0x00400000	
#define HSR0_EMEM		0x00800000	
#define HSR0_EXMMU		0x01000000	
#define HSR0_EDMMU		0x02000000	
#define HSR0_EIMMU		0x04000000	
#define HSR0_CBM		0x08000000	
#define HSR0_CBM_WRITE_THRU	0x00000000	
#define HSR0_CBM_COPY_BACK	0x08000000	
#define HSR0_NWA		0x10000000	
#define HSR0_DCE		0x40000000	
#define HSR0_ICE		0x80000000	

#define __get_HSR(R)	({ unsigned long x; asm volatile("movsg hsr"#R",%0" : "=r"(x)); x; })
#define __set_HSR(R,V)	do { asm volatile("movgs %0,hsr"#R : : "r"(V)); } while(0)

#define CCR_FCC0		0x0000000f	
#define CCR_FCC1		0x000000f0	
#define CCR_FCC2		0x00000f00	
#define CCR_FCC3		0x0000f000	
#define CCR_ICC0		0x000f0000	
#define CCR_ICC0_C		0x00010000	
#define CCR_ICC0_V		0x00020000	
#define CCR_ICC0_Z		0x00040000	
#define CCR_ICC0_N		0x00080000	
#define CCR_ICC1		0x00f00000	
#define CCR_ICC2		0x0f000000	
#define CCR_ICC3		0xf0000000	

#define CCCR_CC0		0x00000003	
#define CCCR_CC0_FALSE		0x00000002	
#define CCCR_CC0_TRUE		0x00000003	
#define CCCR_CC1		0x0000000c	
#define CCCR_CC2		0x00000030	
#define CCCR_CC3		0x000000c0	
#define CCCR_CC4		0x00000300	
#define CCCR_CC5		0x00000c00	
#define CCCR_CC6		0x00003000	
#define CCCR_CC7		0x0000c000	

#define ISR_EMAM		0x00000001	
#define ISR_EMAM_EXCEPTION	0x00000000	
#define ISR_EMAM_FUDGE		0x00000001	
#define ISR_AEXC		0x00000004	
#define ISR_DTT			0x00000018	
#define ISR_DTT_IGNORE		0x00000000	
#define ISR_DTT_DIVBYZERO	0x00000008	
#define ISR_DTT_OVERFLOW	0x00000010	
#define ISR_EDE			0x00000020	
#define ISR_PLI			0x20000000	
#define ISR_QI			0x80000000	

#define EPCR0_V			0x00000001	
#define EPCR0_PC		0xfffffffc	

#define ESRx_VALID		0x00000001	
#define ESRx_EC			0x0000003e	
#define ESRx_EC_DATA_STORE	0x00000000	
#define ESRx_EC_INSN_ACCESS	0x00000006	
#define ESRx_EC_PRIV_INSN	0x00000008	
#define ESRx_EC_ILL_INSN	0x0000000a	
#define ESRx_EC_MP_EXCEP	0x0000001c	
#define ESRx_EC_DATA_ACCESS	0x00000020	
#define ESRx_EC_DIVISION	0x00000026	
#define ESRx_EC_ITLB_MISS	0x00000034	
#define ESRx_EC_DTLB_MISS	0x00000036	
#define ESRx_EC_DATA_ACCESS_DAT	0x0000003a	

#define ESR0_IAEC		0x00000100	
#define ESR0_IAEC_RESV		0x00000000	
#define ESR0_IAEC_PROT_VIOL	0x00000100	

#define ESR0_ATXC		0x00f00000	
#define ESR0_ATXC_MMU_MISS	0x00000000	
#define ESR0_ATXC_MULTI_DAT	0x00800000	
#define ESR0_ATXC_MULTI_SAT	0x00900000	
#define ESR0_ATXC_AMRTLB_MISS	0x00a00000	
#define ESR0_ATXC_PRIV_EXCEP	0x00c00000	
#define ESR0_ATXC_WP_EXCEP	0x00d00000	

#define ESR0_EAV		0x00000800	
#define ESR15_EAV		0x00000800	

#define ESFR1_ESR0		0x00000001	
#define ESFR1_ESR14		0x00004000	
#define ESFR1_ESR15		0x00008000	

#define MSR0_AOVF		0x00000001	
#define MSRx_OVF		0x00000002	
#define MSRx_SIE		0x0000003c	
#define MSRx_SIE_NONE		0x00000000	
#define MSRx_SIE_FRkHI_ACCk	0x00000020	
#define MSRx_SIE_FRkLO_ACCk1	0x00000010	
#define MSRx_SIE_FRk1HI_ACCk2	0x00000008	
#define MSRx_SIE_FRk1LO_ACCk3	0x00000004	
#define MSR0_MTT		0x00007000	
#define MSR0_MTT_NONE		0x00000000	
#define MSR0_MTT_OVERFLOW	0x00001000	
#define MSR0_HI			0x00c00000	
#define MSR0_HI_ROUNDING	0x00000000	
#define MSR0_HI_NONROUNDING	0x00c00000	
#define MSR0_EMCI		0x01000000	
#define MSR0_SRDAV		0x10000000	
#define MSR0_SRDAV_RDAV		0x00000000	
#define MSR0_SRDAV_RD		0x10000000	
#define MSR0_RDAV		0x20000000	
#define MSR0_RDAV_NEAREST_MI	0x00000000	
#define MSR0_RDAV_NEAREST_PL	0x20000000	
#define MSR0_RD			0xc0000000	
#define MSR0_RD_NEAREST		0x00000000	
#define MSR0_RD_ZERO		0x40000000	
#define MSR0_RD_POS_INF		0x80000000	
#define MSR0_RD_NEG_INF		0xc0000000	

#define xAMPRx_V		0x00000001	
#define DAMPRx_WP		0x00000002	
#define DAMPRx_WP_RW		0x00000000	
#define DAMPRx_WP_RO		0x00000002	
#define xAMPRx_C		0x00000004	
#define xAMPRx_C_CACHED		0x00000000	
#define xAMPRx_C_UNCACHED	0x00000004	
#define xAMPRx_S		0x00000008	
#define xAMPRx_S_USER		0x00000000	
#define xAMPRx_S_KERNEL		0x00000008	
#define xAMPRx_SS		0x000000f0	
#define xAMPRx_SS_16Kb		0x00000000	
#define xAMPRx_SS_64Kb		0x00000010	
#define xAMPRx_SS_256Kb		0x00000020	
#define xAMPRx_SS_1Mb		0x00000030	
#define xAMPRx_SS_2Mb		0x00000040	
#define xAMPRx_SS_4Mb		0x00000050	
#define xAMPRx_SS_8Mb		0x00000060	
#define xAMPRx_SS_16Mb		0x00000070	
#define xAMPRx_SS_32Mb		0x00000080	
#define xAMPRx_SS_64Mb		0x00000090	
#define xAMPRx_SS_128Mb		0x000000a0	
#define xAMPRx_SS_256Mb		0x000000b0	
#define xAMPRx_SS_512Mb		0x000000c0	
#define xAMPRx_RESERVED8	0x00000100	
#define xAMPRx_NG		0x00000200	
#define xAMPRx_L		0x00000400	
#define xAMPRx_M		0x00000800	
#define xAMPRx_D		0x00001000	
#define xAMPRx_RESERVED13	0x00002000	
#define xAMPRx_PPFN		0xfff00000	

#define xAMPRx_V_BIT		0
#define DAMPRx_WP_BIT		1
#define xAMPRx_C_BIT		2
#define xAMPRx_S_BIT		3
#define xAMPRx_RESERVED8_BIT	8
#define xAMPRx_NG_BIT		9
#define xAMPRx_L_BIT		10
#define xAMPRx_M_BIT		11
#define xAMPRx_D_BIT		12
#define xAMPRx_RESERVED13_BIT	13

#define __get_IAMPR(R) ({ unsigned long x; asm volatile("movsg iampr"#R",%0" : "=r"(x)); x; })
#define __get_DAMPR(R) ({ unsigned long x; asm volatile("movsg dampr"#R",%0" : "=r"(x)); x; })

#define __get_IAMLR(R) ({ unsigned long x; asm volatile("movsg iamlr"#R",%0" : "=r"(x)); x; })
#define __get_DAMLR(R) ({ unsigned long x; asm volatile("movsg damlr"#R",%0" : "=r"(x)); x; })

#define __set_IAMPR(R,V) 	do { asm volatile("movgs %0,iampr"#R : : "r"(V)); } while(0)
#define __set_DAMPR(R,V)  	do { asm volatile("movgs %0,dampr"#R : : "r"(V)); } while(0)

#define __set_IAMLR(R,V) 	do { asm volatile("movgs %0,iamlr"#R : : "r"(V)); } while(0)
#define __set_DAMLR(R,V)  	do { asm volatile("movgs %0,damlr"#R : : "r"(V)); } while(0)

#define save_dampr(R, _dampr)					\
do {								\
	asm volatile("movsg dampr"R",%0" : "=r"(_dampr));	\
} while(0)

#define restore_dampr(R, _dampr)			\
do {							\
	asm volatile("movgs %0,dampr"R :: "r"(_dampr));	\
} while(0)

#define AMCR_IAMRN		0x000000ff	
#define AMCR_DAMRN		0x0000ff00	

#define __get_TTBR()		({ unsigned long x; asm volatile("movsg ttbr,%0" : "=r"(x)); x; })

#define TPXR_E			0x00000001
#define TPXR_LMAX_SHIFT		20
#define TPXR_LMAX_SMASK		0xf
#define TPXR_WMAX_SHIFT		24
#define TPXR_WMAX_SMASK		0xf
#define TPXR_WAY_SHIFT		28
#define TPXR_WAY_SMASK		0xf

#define DCR_IBCE3		0x00000001	
#define DCR_IBE3		0x00000002	
#define DCR_IBCE1		0x00000004	
#define DCR_IBE1		0x00000008	
#define DCR_IBCE2		0x00000010	
#define DCR_IBE2		0x00000020	
#define DCR_IBCE0		0x00000040	
#define DCR_IBE0		0x00000080	

#define DCR_DDBE1		0x00004000	
#define DCR_DWBE1		0x00008000	
#define DCR_DRBE1		0x00010000	
#define DCR_DDBE0		0x00020000	
#define DCR_DWBE0		0x00040000	
#define DCR_DRBE0		0x00080000	

#define DCR_EIM			0x0c000000	
#define DCR_IBM			0x10000000	
#define DCR_SE			0x20000000	
#define DCR_EBE			0x40000000	

#define BRR_ST			0x00000001	
#define BRR_SB			0x00000002	
#define BRR_BB			0x00000004	
#define BRR_CBB			0x00000008	
#define BRR_IBx			0x000000f0	
#define BRR_DBx			0x00000f00	
#define BRR_DBNEx		0x0000f000	
#define BRR_EBTT		0x00ff0000	
#define BRR_TB			0x10000000	
#define BRR_CB			0x20000000	
#define BRR_EB			0x40000000	

#define BPSR_BET		0x00000001	
#define BPSR_BS			0x00001000	

#endif 
