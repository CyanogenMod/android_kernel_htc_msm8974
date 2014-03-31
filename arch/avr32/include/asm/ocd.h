/*
 * AVR32 OCD Interface and register definitions
 *
 * Copyright (C) 2004-2007 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_OCD_H
#define __ASM_AVR32_OCD_H

#define OCD_DID				0x0000  
#define OCD_DC				0x0008  
#define OCD_DS				0x0010  
#define OCD_RWCS			0x001c  
#define OCD_RWA				0x0024  
#define OCD_RWD				0x0028  
#define OCD_WT				0x002c  
#define OCD_DTC				0x0034  
#define OCD_DTSA0			0x0038  
#define OCD_DTSA1			0x003c  
#define OCD_DTEA0			0x0048  
#define OCD_DTEA1			0x004c  
#define OCD_BWC0A			0x0058  
#define OCD_BWC0B			0x005c  
#define OCD_BWC1A			0x0060  
#define OCD_BWC1B			0x0064  
#define OCD_BWC2A			0x0068  
#define OCD_BWC2B			0x006c  
#define OCD_BWC3A			0x0070  
#define OCD_BWC3B			0x0074  
#define OCD_BWA0A			0x0078  
#define OCD_BWA0B			0x007c  
#define OCD_BWA1A			0x0080  
#define OCD_BWA1B			0x0084  
#define OCD_BWA2A			0x0088  
#define OCD_BWA2B			0x008c  
#define OCD_BWA3A			0x0090  
#define OCD_BWA3B			0x0094  
#define OCD_NXCFG			0x0100  
#define OCD_DINST			0x0104  
#define OCD_DPC				0x0108  
#define OCD_CPUCM			0x010c  
#define OCD_DCCPU			0x0110  
#define OCD_DCEMU			0x0114  
#define OCD_DCSR			0x0118  
#define OCD_PID				0x011c  
#define OCD_EPC0			0x0120  
#define OCD_EPC1			0x0124  
#define OCD_EPC2			0x0128  
#define OCD_EPC3			0x012c  
#define OCD_AXC				0x0130  

#define OCD_DID_MID_START		1
#define OCD_DID_MID_SIZE		11
#define OCD_DID_PN_START		12
#define OCD_DID_PN_SIZE			16
#define OCD_DID_RN_START		28
#define OCD_DID_RN_SIZE			4

#define OCD_DC_TM_START			0
#define OCD_DC_TM_SIZE			2
#define OCD_DC_EIC_START		3
#define OCD_DC_EIC_SIZE			2
#define OCD_DC_OVC_START		5
#define OCD_DC_OVC_SIZE			3
#define OCD_DC_SS_BIT			8
#define OCD_DC_DBR_BIT			12
#define OCD_DC_DBE_BIT			13
#define OCD_DC_EOS_START		20
#define OCD_DC_EOS_SIZE			2
#define OCD_DC_SQA_BIT			22
#define OCD_DC_IRP_BIT			23
#define OCD_DC_IFM_BIT			24
#define OCD_DC_TOZ_BIT			25
#define OCD_DC_TSR_BIT			26
#define OCD_DC_RID_BIT			27
#define OCD_DC_ORP_BIT			28
#define OCD_DC_MM_BIT			29
#define OCD_DC_RES_BIT			30
#define OCD_DC_ABORT_BIT		31

#define OCD_DS_SSS_BIT			0
#define OCD_DS_SWB_BIT			1
#define OCD_DS_HWB_BIT			2
#define OCD_DS_HWE_BIT			3
#define OCD_DS_STP_BIT			4
#define OCD_DS_DBS_BIT			5
#define OCD_DS_BP_START			8
#define OCD_DS_BP_SIZE			8
#define OCD_DS_INC_BIT			24
#define OCD_DS_BOZ_BIT			25
#define OCD_DS_DBA_BIT			26
#define OCD_DS_EXB_BIT			27
#define OCD_DS_NTBF_BIT			28

#define OCD_RWCS_DV_BIT			0
#define OCD_RWCS_ERR_BIT		1
#define OCD_RWCS_CNT_START		2
#define OCD_RWCS_CNT_SIZE		14
#define OCD_RWCS_CRC_BIT		19
#define OCD_RWCS_NTBC_START		20
#define OCD_RWCS_NTBC_SIZE		2
#define OCD_RWCS_NTE_BIT		22
#define OCD_RWCS_NTAP_BIT		23
#define OCD_RWCS_WRAPPED_BIT		24
#define OCD_RWCS_CCTRL_START		25
#define OCD_RWCS_CCTRL_SIZE		2
#define OCD_RWCS_SZ_START		27
#define OCD_RWCS_SZ_SIZE		3
#define OCD_RWCS_RW_BIT			30
#define OCD_RWCS_AC_BIT			31

#define OCD_RWA_RWA_START		0
#define OCD_RWA_RWA_SIZE		32

#define OCD_RWD_RWD_START		0
#define OCD_RWD_RWD_SIZE		32

#define OCD_WT_DTE_START		20
#define OCD_WT_DTE_SIZE			3
#define OCD_WT_DTS_START		23
#define OCD_WT_DTS_SIZE			3
#define OCD_WT_PTE_START		26
#define OCD_WT_PTE_SIZE			3
#define OCD_WT_PTS_START		29
#define OCD_WT_PTS_SIZE			3

#define OCD_DTC_T0WP_BIT		0
#define OCD_DTC_T1WP_BIT		1
#define OCD_DTC_ASID0EN_BIT		2
#define OCD_DTC_ASID0_START		3
#define OCD_DTC_ASID0_SIZE		8
#define OCD_DTC_ASID1EN_BIT		11
#define OCD_DTC_ASID1_START		12
#define OCD_DTC_ASID1_SIZE		8
#define OCD_DTC_RWT1_START		28
#define OCD_DTC_RWT1_SIZE		2
#define OCD_DTC_RWT0_START		30
#define OCD_DTC_RWT0_SIZE		2

#define OCD_DTSA0_DTSA_START		0
#define OCD_DTSA0_DTSA_SIZE		32

#define OCD_DTSA1_DTSA_START		0
#define OCD_DTSA1_DTSA_SIZE		32

#define OCD_DTEA0_DTEA_START		0
#define OCD_DTEA0_DTEA_SIZE		32

#define OCD_DTEA1_DTEA_START		0
#define OCD_DTEA1_DTEA_SIZE		32

#define OCD_BWC0A_ASIDEN_BIT		0
#define OCD_BWC0A_ASID_START		1
#define OCD_BWC0A_ASID_SIZE		8
#define OCD_BWC0A_EOC_BIT		14
#define OCD_BWC0A_AME_BIT		25
#define OCD_BWC0A_BWE_START		30
#define OCD_BWC0A_BWE_SIZE		2

#define OCD_BWC0B_ASIDEN_BIT		0
#define OCD_BWC0B_ASID_START		1
#define OCD_BWC0B_ASID_SIZE		8
#define OCD_BWC0B_EOC_BIT		14
#define OCD_BWC0B_AME_BIT		25
#define OCD_BWC0B_BWE_START		30
#define OCD_BWC0B_BWE_SIZE		2

#define OCD_BWC1A_ASIDEN_BIT		0
#define OCD_BWC1A_ASID_START		1
#define OCD_BWC1A_ASID_SIZE		8
#define OCD_BWC1A_EOC_BIT		14
#define OCD_BWC1A_AME_BIT		25
#define OCD_BWC1A_BWE_START		30
#define OCD_BWC1A_BWE_SIZE		2

#define OCD_BWC1B_ASIDEN_BIT		0
#define OCD_BWC1B_ASID_START		1
#define OCD_BWC1B_ASID_SIZE		8
#define OCD_BWC1B_EOC_BIT		14
#define OCD_BWC1B_AME_BIT		25
#define OCD_BWC1B_BWE_START		30
#define OCD_BWC1B_BWE_SIZE		2

#define OCD_BWC2A_ASIDEN_BIT		0
#define OCD_BWC2A_ASID_START		1
#define OCD_BWC2A_ASID_SIZE		8
#define OCD_BWC2A_EOC_BIT		14
#define OCD_BWC2A_AMB_START		20
#define OCD_BWC2A_AMB_SIZE		5
#define OCD_BWC2A_AME_BIT		25
#define OCD_BWC2A_BWE_START		30
#define OCD_BWC2A_BWE_SIZE		2

#define OCD_BWC2B_ASIDEN_BIT		0
#define OCD_BWC2B_ASID_START		1
#define OCD_BWC2B_ASID_SIZE		8
#define OCD_BWC2B_EOC_BIT		14
#define OCD_BWC2B_AME_BIT		25
#define OCD_BWC2B_BWE_START		30
#define OCD_BWC2B_BWE_SIZE		2

#define OCD_BWC3A_ASIDEN_BIT		0
#define OCD_BWC3A_ASID_START		1
#define OCD_BWC3A_ASID_SIZE		8
#define OCD_BWC3A_SIZE_START		9
#define OCD_BWC3A_SIZE_SIZE		3
#define OCD_BWC3A_EOC_BIT		14
#define OCD_BWC3A_BWO_START		16
#define OCD_BWC3A_BWO_SIZE		2
#define OCD_BWC3A_BME_START		20
#define OCD_BWC3A_BME_SIZE		4
#define OCD_BWC3A_BRW_START		28
#define OCD_BWC3A_BRW_SIZE		2
#define OCD_BWC3A_BWE_START		30
#define OCD_BWC3A_BWE_SIZE		2

#define OCD_BWC3B_ASIDEN_BIT		0
#define OCD_BWC3B_ASID_START		1
#define OCD_BWC3B_ASID_SIZE		8
#define OCD_BWC3B_SIZE_START		9
#define OCD_BWC3B_SIZE_SIZE		3
#define OCD_BWC3B_EOC_BIT		14
#define OCD_BWC3B_BWO_START		16
#define OCD_BWC3B_BWO_SIZE		2
#define OCD_BWC3B_BME_START		20
#define OCD_BWC3B_BME_SIZE		4
#define OCD_BWC3B_BRW_START		28
#define OCD_BWC3B_BRW_SIZE		2
#define OCD_BWC3B_BWE_START		30
#define OCD_BWC3B_BWE_SIZE		2

#define OCD_BWA0A_BWA_START		0
#define OCD_BWA0A_BWA_SIZE		32

#define OCD_BWA0B_BWA_START		0
#define OCD_BWA0B_BWA_SIZE		32

#define OCD_BWA1A_BWA_START		0
#define OCD_BWA1A_BWA_SIZE		32

#define OCD_BWA1B_BWA_START		0
#define OCD_BWA1B_BWA_SIZE		32

#define OCD_BWA2A_BWA_START		0
#define OCD_BWA2A_BWA_SIZE		32

#define OCD_BWA2B_BWA_START		0
#define OCD_BWA2B_BWA_SIZE		32

#define OCD_BWA3A_BWA_START		0
#define OCD_BWA3A_BWA_SIZE		32

#define OCD_BWA3B_BWA_START		0
#define OCD_BWA3B_BWA_SIZE		32

#define OCD_NXCFG_NXARCH_START		0
#define OCD_NXCFG_NXARCH_SIZE		4
#define OCD_NXCFG_NXOCD_START		4
#define OCD_NXCFG_NXOCD_SIZE		4
#define OCD_NXCFG_NXPCB_START		8
#define OCD_NXCFG_NXPCB_SIZE		4
#define OCD_NXCFG_NXDB_START		12
#define OCD_NXCFG_NXDB_SIZE		4
#define OCD_NXCFG_MXMSEO_BIT		16
#define OCD_NXCFG_NXMDO_START		17
#define OCD_NXCFG_NXMDO_SIZE		4
#define OCD_NXCFG_NXPT_BIT		21
#define OCD_NXCFG_NXOT_BIT		22
#define OCD_NXCFG_NXDWT_BIT		23
#define OCD_NXCFG_NXDRT_BIT		24
#define OCD_NXCFG_NXDTC_START		25
#define OCD_NXCFG_NXDTC_SIZE		3
#define OCD_NXCFG_NXDMA_BIT		28

#define OCD_DINST_DINST_START		0
#define OCD_DINST_DINST_SIZE		32

#define OCD_CPUCM_BEM_BIT		1
#define OCD_CPUCM_FEM_BIT		2
#define OCD_CPUCM_REM_BIT		3
#define OCD_CPUCM_IBEM_BIT		4
#define OCD_CPUCM_IEEM_BIT		5

#define OCD_DCCPU_DATA_START		0
#define OCD_DCCPU_DATA_SIZE		32

#define OCD_DCEMU_DATA_START		0
#define OCD_DCEMU_DATA_SIZE		32

#define OCD_DCSR_CPUD_BIT		0
#define OCD_DCSR_EMUD_BIT		1

#define OCD_PID_PROCESS_START		0
#define OCD_PID_PROCESS_SIZE		32

#define OCD_EPC0_RNG_START		0
#define OCD_EPC0_RNG_SIZE		2
#define OCD_EPC0_CE_BIT			4
#define OCD_EPC0_ECNT_START		16
#define OCD_EPC0_ECNT_SIZE		16

#define OCD_EPC1_RNG_START		0
#define OCD_EPC1_RNG_SIZE		2
#define OCD_EPC1_ATB_BIT		5
#define OCD_EPC1_AM_BIT			6

#define OCD_EPC2_RNG_START		0
#define OCD_EPC2_RNG_SIZE		2
#define OCD_EPC2_DB_START		2
#define OCD_EPC2_DB_SIZE		2

#define OCD_EPC3_RNG_START		0
#define OCD_EPC3_RNG_SIZE		2
#define OCD_EPC3_DWE_BIT		2

#define OCD_AXC_DIV_START		0
#define OCD_AXC_DIV_SIZE		4
#define OCD_AXC_AXE_BIT			8
#define OCD_AXC_AXS_BIT			9
#define OCD_AXC_DDR_BIT			10
#define OCD_AXC_LS_BIT			11
#define OCD_AXC_REX_BIT			12
#define OCD_AXC_REXTEN_BIT		13

#define OCD_EIC_PROGRAM_AND_DATA_TRACE	0
#define OCD_EIC_BREAKPOINT		1
#define OCD_EIC_NOP			2

#define OCD_OVC_OVERRUN			0
#define OCD_OVC_DELAY_CPU_BTM		1
#define OCD_OVC_DELAY_CPU_DTM		2
#define OCD_OVC_DELAY_CPU_BTM_DTM	3

#define OCD_EOS_NOP			0
#define OCD_EOS_DEBUG_MODE		1
#define OCD_EOS_BREAKPOINT_WATCHPOINT	2
#define OCD_EOS_THQ			3

#define OCD_NTBC_OVERWRITE		0
#define OCD_NTBC_DISABLE		1
#define OCD_NTBC_BREAKPOINT		2

#define OCD_CCTRL_AUTO			0
#define OCD_CCTRL_CACHED		1
#define OCD_CCTRL_UNCACHED		2

#define OCD_SZ_BYTE			0
#define OCD_SZ_HALFWORD			1
#define OCD_SZ_WORD			2

#define OCD_PTS_DISABLED		0
#define OCD_PTS_PROGRAM_0B		1
#define OCD_PTS_PROGRAM_1A		2
#define OCD_PTS_PROGRAM_1B		3
#define OCD_PTS_PROGRAM_2A		4
#define OCD_PTS_PROGRAM_2B		5
#define OCD_PTS_DATA_3A			6
#define OCD_PTS_DATA_3B			7

#define OCD_RWT1_NO_TRACE		0
#define OCD_RWT1_DATA_READ		1
#define OCD_RWT1_DATA_WRITE		2
#define OCD_RWT1_DATA_READ_WRITE	3

#define OCD_RWT0_NO_TRACE		0
#define OCD_RWT0_DATA_READ		1
#define OCD_RWT0_DATA_WRITE		2
#define OCD_RWT0_DATA_READ_WRITE	3

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_SIZE_BYTE_ACCESS		4
#define OCD_SIZE_HALFWORD_ACCESS	5
#define OCD_SIZE_WORD_ACCESS		6
#define OCD_SIZE_DOUBLE_WORD_ACCESS	7

#define OCD_BRW_READ_BREAK		0
#define OCD_BRW_WRITE_BREAK		1
#define OCD_BRW_ANY_ACCES_BREAK		2

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_SIZE_BYTE_ACCESS		4
#define OCD_SIZE_HALFWORD_ACCESS	5
#define OCD_SIZE_WORD_ACCESS		6
#define OCD_SIZE_DOUBLE_WORD_ACCESS	7

#define OCD_BRW_READ_BREAK		0
#define OCD_BRW_WRITE_BREAK		1
#define OCD_BRW_ANY_ACCES_BREAK		2

#define OCD_BWE_DISABLED		0
#define OCD_BWE_BREAKPOINT_ENABLED	1
#define OCD_BWE_WATCHPOINT_ENABLED	3

#define OCD_RNG_DISABLED		0
#define OCD_RNG_EXCLUSIVE		1
#define OCD_RNG_INCLUSIVE		2

#define OCD_RNG_DISABLED		0
#define OCD_RNG_EXCLUSIVE		1
#define OCD_RNG_INCLUSIVE		2

#define OCD_RNG_DISABLED		0
#define OCD_RNG_EXCLUSIVE		1
#define OCD_RNG_INCLUSIVE		2

#define OCD_DB_DISABLED			0
#define OCD_DB_CHAINED_B		1
#define OCD_DB_CHAINED_A		2
#define OCD_DB_AHAINED_A_AND_B		3

#define OCD_RNG_DISABLED		0
#define OCD_RNG_EXCLUSIVE		1
#define OCD_RNG_INCLUSIVE		2

#ifndef __ASSEMBLER__

static inline unsigned long __ocd_read(unsigned int reg)
{
	return __builtin_mfdr(reg);
}

static inline void __ocd_write(unsigned int reg, unsigned long value)
{
	__builtin_mtdr(reg, value);
}

#define ocd_read(reg)			__ocd_read(OCD_##reg)
#define ocd_write(reg, value)		__ocd_write(OCD_##reg, value)

struct task_struct;

void ocd_enable(struct task_struct *child);
void ocd_disable(struct task_struct *child);

#endif 

#endif 
