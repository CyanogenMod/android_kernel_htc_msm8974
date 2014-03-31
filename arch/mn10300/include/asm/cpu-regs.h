/* MN10300 Core system registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_CPU_REGS_H
#define _ASM_CPU_REGS_H

#ifndef __ASSEMBLY__
#include <linux/types.h>
#endif

#ifndef __ASSEMBLY__
asm(" .am33_2\n");
#else
.am33_2
#endif

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#define __SYSREG(ADDR, TYPE) (*(volatile TYPE *)(ADDR))
#define __SYSREGC(ADDR, TYPE) (*(const volatile TYPE *)(ADDR))
#else
#define __SYSREG(ADDR, TYPE) ADDR
#define __SYSREGC(ADDR, TYPE) ADDR
#endif

#define EPSW_FLAG_Z		0x00000001	
#define EPSW_FLAG_N		0x00000002	
#define EPSW_FLAG_C		0x00000004	
#define EPSW_FLAG_V		0x00000008	
#define EPSW_IM			0x00000700	
#define EPSW_IM_0		0x00000000	
#define EPSW_IM_1		0x00000100	
#define EPSW_IM_2		0x00000200	
#define EPSW_IM_3		0x00000300	
#define EPSW_IM_4		0x00000400	
#define EPSW_IM_5		0x00000500	
#define EPSW_IM_6		0x00000600	
#define EPSW_IM_7		0x00000700	
#define EPSW_IE			0x00000800	
#define EPSW_S			0x00003000	
#define EPSW_T			0x00008000	
#define EPSW_nSL		0x00010000	
#define EPSW_NMID		0x00020000	
#define EPSW_nAR		0x00040000	
#define EPSW_ML			0x00080000	
#define EPSW_FE			0x00100000	
#define EPSW_IM_SHIFT		8		

#define NUM2EPSW_IM(num)	((num) << EPSW_IM_SHIFT)

#define FPCR_EF_I		0x00000001	
#define FPCR_EF_U		0x00000002	
#define FPCR_EF_O		0x00000004	
#define FPCR_EF_Z		0x00000008	
#define FPCR_EF_V		0x00000010	
#define FPCR_EE_I		0x00000020	
#define FPCR_EE_U		0x00000040	
#define FPCR_EE_O		0x00000080	
#define FPCR_EE_Z		0x00000100	
#define FPCR_EE_V		0x00000200	
#define FPCR_EC_I		0x00000400	
#define FPCR_EC_U		0x00000800	
#define FPCR_EC_O		0x00001000	
#define FPCR_EC_Z		0x00002000	
#define FPCR_EC_V		0x00004000	
#define FPCR_RM			0x00030000	
#define FPCR_RM_NEAREST		0x00000000	
#define FPCR_FCC_U		0x00040000	
#define FPCR_FCC_E		0x00080000	
#define FPCR_FCC_G		0x00100000	
#define FPCR_FCC_L		0x00200000	
#define FPCR_INIT		0x00000000	

#define CPUP			__SYSREG(0xc0000020, u16)	
#define CPUP_DWBD		0x0020		
#define CPUP_IPFD		0x0040		
#define CPUP_EXM		0x0080		
#define CPUP_EXM_AM33V1		0x0000		
#define CPUP_EXM_AM33V2		0x0080		

#define CPUM			__SYSREG(0xc0000040, u16)	
#define CPUM_SLEEP		0x0004		
#define CPUM_HALT		0x0008		
#define CPUM_STOP		0x0010		

#define CPUREV			__SYSREGC(0xc0000050, u32)	
#define CPUREV_TYPE		0x0000000f	
#define CPUREV_TYPE_S		0
#define CPUREV_TYPE_AM33_1	0x00000000	
#define CPUREV_TYPE_AM33_2	0x00000001	
#define CPUREV_TYPE_AM34_1	0x00000002	
#define CPUREV_TYPE_AM33_3	0x00000003	
#define CPUREV_TYPE_AM34_2	0x00000004	
#define CPUREV_REVISION		0x000000f0	
#define CPUREV_REVISION_S	4
#define CPUREV_ICWAY		0x00000f00	
#define CPUREV_ICWAY_S		8
#define CPUREV_ICSIZE		0x0000f000	
#define CPUREV_ICSIZE_S		12
#define CPUREV_DCWAY		0x000f0000	
#define CPUREV_DCWAY_S		16
#define CPUREV_DCSIZE		0x00f00000	
#define CPUREV_DCSIZE_S		20
#define CPUREV_FPUTYPE		0x0f000000	
#define CPUREV_FPUTYPE_NONE	0x00000000	
#define CPUREV_OCDCTG		0xf0000000	

#define DCR			__SYSREG(0xc0000030, u16)	

#define IVAR0			__SYSREG(0xc0000000, u16)	
#define IVAR1			__SYSREG(0xc0000004, u16)	
#define IVAR2			__SYSREG(0xc0000008, u16)	
#define IVAR3			__SYSREG(0xc000000c, u16)	
#define IVAR4			__SYSREG(0xc0000010, u16)	
#define IVAR5			__SYSREG(0xc0000014, u16)	
#define IVAR6			__SYSREG(0xc0000018, u16)	

#define TBR			__SYSREG(0xc0000024, u32)	
#define TBR_TB			0xff000000	
#define TBR_INT_CODE		0x00ffffff	

#define DEAR			__SYSREG(0xc0000038, u32)	

#define sISR			__SYSREG(0xc0000044, u32)	
#define	sISR_IRQICE		0x00000001	
#define	sISR_ISTEP		0x00000002	
#define	sISR_MISSA		0x00000004	
#define	sISR_UNIMP		0x00000008	
#define	sISR_PIEXE		0x00000010	
#define	sISR_MEMERR		0x00000020	
#define	sISR_IBREAK		0x00000040	
#define	sISR_DBSRL		0x00000080	
#define	sISR_PERIDB		0x00000100	
#define	sISR_EXUNIMP		0x00000200	
#define	sISR_OBREAK		0x00000400	
#define	sISR_PRIV		0x00000800	
#define	sISR_BUSERR		0x00001000	
#define	sISR_DBLFT		0x00002000	
#define	sISR_DBG		0x00008000	
#define sISR_ITMISS		0x00010000	
#define sISR_DTMISS		0x00020000	
#define sISR_ITEX		0x00040000	
#define sISR_DTEX		0x00080000	
#define sISR_ILGIA		0x00100000	
#define sISR_ILGDA		0x00200000	
#define sISR_IOIA		0x00400000	
#define sISR_PRIVA		0x00800000	
#define sISR_PRIDA		0x01000000	
#define sISR_DISA		0x02000000	
#define sISR_SYSC		0x04000000	
#define sISR_FPUD		0x08000000	
#define sISR_FPUUI		0x10000000	
#define sISR_FPUOP		0x20000000	
#define sISR_NE			0x80000000	

#define CHCTR			__SYSREG(0xc0000070, u16)	
#define CHCTR_ICEN		0x0001		
#define CHCTR_DCEN		0x0002		
#define CHCTR_ICBUSY		0x0004		
#define CHCTR_DCBUSY		0x0008		
#define CHCTR_ICINV		0x0010		
#define CHCTR_DCINV		0x0020		
#define CHCTR_DCWTMD		0x0040		
#define CHCTR_DCWTMD_WRBACK	0x0000		
#define CHCTR_DCWTMD_WRTHROUGH	0x0040		
#define CHCTR_DCALMD		0x0080		
#define CHCTR_ICWMD		0x0f00		
#define CHCTR_DCWMD		0xf000		

#ifdef CONFIG_AM34_2
#define ICIVCR			__SYSREG(0xc0000c00, u32)	
#define ICIVCR_ICIVBSY		0x00000008			
#define ICIVCR_ICI		0x00000001			

#define ICIVMR			__SYSREG(0xc0000c04, u32)	

#define	DCPGCR			__SYSREG(0xc0000c10, u32)	
#define	DCPGCR_DCPGBSY		0x00000008			
#define	DCPGCR_DCP		0x00000002			
#define	DCPGCR_DCI		0x00000001			

#define	DCPGMR			__SYSREG(0xc0000c14, u32)	
#endif 

#define MMUCTR			__SYSREG(0xc0000090, u32)	
#define MMUCTR_IRP		0x0000003f	
#define MMUCTR_ITE		0x00000040	
#define MMUCTR_IIV		0x00000080	
#define MMUCTR_ITL		0x00000700	
#define MMUCTR_ITL_NOLOCK	0x00000000	
#define MMUCTR_ITL_LOCK0	0x00000100	
#define MMUCTR_ITL_LOCK0_1	0x00000200	
#define MMUCTR_ITL_LOCK0_3	0x00000300	
#define MMUCTR_ITL_LOCK0_7	0x00000400	
#define MMUCTR_ITL_LOCK0_15	0x00000500	
#define MMUCTR_CE		0x00008000	
#define MMUCTR_DRP		0x003f0000	
#define MMUCTR_DTE		0x00400000	
#define MMUCTR_DIV		0x00800000	
#define MMUCTR_DTL		0x07000000	
#define MMUCTR_DTL_NOLOCK	0x00000000	
#define MMUCTR_DTL_LOCK0	0x01000000	
#define MMUCTR_DTL_LOCK0_1	0x02000000	
#define MMUCTR_DTL_LOCK0_3	0x03000000	
#define MMUCTR_DTL_LOCK0_7	0x04000000	
#define MMUCTR_DTL_LOCK0_15	0x05000000	
#ifdef CONFIG_AM34_2
#define MMUCTR_WTE		0x80000000	
#endif

#define PIDR			__SYSREG(0xc0000094, u16)	
#define PIDR_PID		0x00ff		

#define PTBR			__SYSREG(0xc0000098, unsigned long) 

#define IPTEL			__SYSREG(0xc00000a0, u32)	
#define DPTEL			__SYSREG(0xc00000b0, u32)	
#define xPTEL_V			0x00000001	
#define xPTEL_UNUSED1		0x00000002	
#define xPTEL_UNUSED2		0x00000004	
#define xPTEL_C			0x00000008	
#define xPTEL_PV		0x00000010	
#define xPTEL_D			0x00000020	
#define xPTEL_PR		0x000001c0	
#define xPTEL_PR_ROK		0x00000000	
#define xPTEL_PR_RWK		0x00000100	
#define xPTEL_PR_ROK_ROU	0x00000080	
#define xPTEL_PR_RWK_ROU	0x00000180	
#define xPTEL_PR_RWK_RWU	0x000001c0	
#define xPTEL_G			0x00000200	
#define xPTEL_PS		0x00000c00	
#define xPTEL_PS_4Kb		0x00000000	
#define xPTEL_PS_128Kb		0x00000400	
#define xPTEL_PS_1Kb		0x00000800	
#define xPTEL_PS_4Mb		0x00000c00	
#define xPTEL_PPN		0xfffff006	

#define IPTEU			__SYSREG(0xc00000a4, u32)	
#define DPTEU			__SYSREG(0xc00000b4, u32)	
#define xPTEU_VPN		0xfffffc00	
#define xPTEU_PID		0x000000ff	

#define IPTEL2			__SYSREG(0xc00000a8, u32)	
#define DPTEL2			__SYSREG(0xc00000b8, u32)	
#define xPTEL2_V		0x00000001	
#define xPTEL2_C		0x00000002	
#define xPTEL2_PV		0x00000004	
#define xPTEL2_D		0x00000008	
#define xPTEL2_PR		0x00000070	
#define xPTEL2_PR_ROK		0x00000000	
#define xPTEL2_PR_RWK		0x00000040	
#define xPTEL2_PR_ROK_ROU	0x00000020	
#define xPTEL2_PR_RWK_ROU	0x00000060	
#define xPTEL2_PR_RWK_RWU	0x00000070	
#define xPTEL2_G		0x00000080	
#define xPTEL2_PS		0x00000300	
#define xPTEL2_PS_4Kb		0x00000000	
#define xPTEL2_PS_128Kb		0x00000100	
#define xPTEL2_PS_1Kb		0x00000200	
#define xPTEL2_PS_4Mb		0x00000300	
#define xPTEL2_CWT		0x00000400	
#define xPTEL2_UNUSED1		0x00000800	
#define xPTEL2_PPN		0xfffff000	

#define xPTEL2_V_BIT		0	
#define xPTEL2_C_BIT		1
#define xPTEL2_PV_BIT		2
#define xPTEL2_D_BIT		3
#define xPTEL2_G_BIT		7
#define xPTEL2_UNUSED1_BIT	11

#define MMUFCR			__SYSREGC(0xc000009c, u32)	
#define MMUFCR_IFC		__SYSREGC(0xc000009c, u16)	
#define MMUFCR_DFC		__SYSREGC(0xc000009e, u16)	
#define MMUFCR_xFC_TLBMISS	0x0001		
#define MMUFCR_xFC_INITWR	0x0002		
#define MMUFCR_xFC_PGINVAL	0x0004		
#define MMUFCR_xFC_PROTVIOL	0x0008		
#define MMUFCR_xFC_ACCESS	0x0010		
#define MMUFCR_xFC_ACCESS_USR	0x0000		
#define MMUFCR_xFC_ACCESS_SR	0x0010		
#define MMUFCR_xFC_TYPE		0x0020		
#define MMUFCR_xFC_TYPE_READ	0x0000		
#define MMUFCR_xFC_TYPE_WRITE	0x0020		
#define MMUFCR_xFC_PR		0x01c0		
#define MMUFCR_xFC_PR_ROK	0x0000		
#define MMUFCR_xFC_PR_RWK	0x0100		
#define MMUFCR_xFC_PR_ROK_ROU	0x0080		
#define MMUFCR_xFC_PR_RWK_ROU	0x0180		
#define MMUFCR_xFC_PR_RWK_RWU	0x01c0		
#define MMUFCR_xFC_ILLADDR	0x0200		

#ifdef CONFIG_MN10300_HAS_ATOMIC_OPS_UNIT
#define AAR		__SYSREG(0xc0000a00, u32)	
#define AAR2		__SYSREG(0xc0000a04, u32)	
#define ADR		__SYSREG(0xc0000a08, u32)	
#define ASR		__SYSREG(0xc0000a0c, u32)	
#define AARU		__SYSREG(0xd400aa00, u32)	
#define ADRU		__SYSREG(0xd400aa08, u32)	
#define ASRU		__SYSREG(0xd400aa0c, u32)	

#define ASR_RW		0x00000008	
#define ASR_BW		0x00000004	
#define ASR_IW		0x00000002	
#define ASR_LW		0x00000001	

#define ASRU_RW		ASR_RW		
#define ASRU_BW		ASR_BW		
#define ASRU_IW		ASR_IW		
#define ASRU_LW		ASR_LW		

#define ATOMIC_OPS_BASE_ADDR 0xc0000a00
#ifndef __ASSEMBLY__
asm(
	"_AAR	= 0\n"
	"_AAR2	= 4\n"
	"_ADR	= 8\n"
	"_ASR	= 12\n");
#else
#define _AAR		0
#define _AAR2		4
#define _ADR		8
#define _ASR		12
#endif

#define USER_ATOMIC_OPS_PAGE_ADDR  0xd400a000

#endif 

#endif 

#endif 
