/*
 * cbe_regs.h
 *
 * This file is intended to hold the various register definitions for CBE
 * on-chip system devices (memory controller, IO controller, etc...)
 *
 * (C) Copyright IBM Corporation 2001,2006
 *
 * Authors: Maximino Aguilar (maguilar@us.ibm.com)
 *          David J. Erb (djerb@us.ibm.com)
 *
 * (c) 2006 Benjamin Herrenschmidt <benh@kernel.crashing.org>, IBM Corp.
 */

#ifndef CBE_REGS_H
#define CBE_REGS_H

#include <asm/cell-pmu.h>


#define HID0_CBE_THERM_WAKEUP	0x0000020000000000ul
#define HID0_CBE_SYSERR_WAKEUP	0x0000008000000000ul
#define HID0_CBE_THERM_INT_EN	0x0000000400000000ul
#define HID0_CBE_SYSERR_INT_EN	0x0000000200000000ul

#define MAX_CBE		2


union spe_reg {
	u64 val;
	u8 spe[8];
};

union ppe_spe_reg {
	u64 val;
	struct {
		u32 ppe;
		u32 spe;
	};
};


struct cbe_pmd_regs {
	
	u64	pad_0x0000;					

	u64	group_control;					

	u8	pad_0x0010_0x00a8 [0x00a8 - 0x0010];		

	u64	debug_bus_control;				

	u8	pad_0x00b0_0x0100 [0x0100 - 0x00b0];		

	u64	trace_aux_data;					
	u64	trace_buffer_0_63;				
	u64	trace_buffer_64_127;				
	u64	trace_address;					
	u64	ext_tr_timer;					

	u8	pad_0x0128_0x0400 [0x0400 - 0x0128];		

	
	u64	pm_status;					
	u64	pm_control;					
	u64	pm_interval;					
	u64	pm_ctr[4];					
	u64	pm_start_stop;					
	u64	pm07_control[8];				

	u8	pad_0x0480_0x0800 [0x0800 - 0x0480];		

	
	union	spe_reg	ts_ctsr1;				
	u64	ts_ctsr2;					
	union	spe_reg	ts_mtsr1;				
	u64	ts_mtsr2;					
	union	spe_reg	ts_itr1;				
	u64	ts_itr2;					
	u64	ts_gitr;					
	u64	ts_isr;						
	u64	ts_imr;						
	union	spe_reg	tm_cr1;					
	u64	tm_cr2;						
	u64	tm_simr;					
	union	ppe_spe_reg tm_tpr;				
	union	spe_reg	tm_str1;				
	u64	tm_str2;					
	union	ppe_spe_reg tm_tsr;				

	
	u64	pmcr;						
#define CBE_PMD_PAUSE_ZERO_CONTROL	0x10000
	u64	pmsr;						

	
	u64	tbr;						

	u8	pad_0x0898_0x0c00 [0x0c00 - 0x0898];		

	
	u64	checkstop_fir;					
	u64	recoverable_fir;				
	u64	spec_att_mchk_fir;				
	u32	fir_mode_reg;					
	u8	pad_0x0c1c_0x0c20 [4];				
#define CBE_PMD_FIR_MODE_M8		0x00800
	u64	fir_enable_mask;				

	u8	pad_0x0c28_0x0ca8 [0x0ca8 - 0x0c28];		
	u64	ras_esc_0;					
	u8	pad_0x0cb0_0x1000 [0x1000 - 0x0cb0];		
};

extern struct cbe_pmd_regs __iomem *cbe_get_pmd_regs(struct device_node *np);
extern struct cbe_pmd_regs __iomem *cbe_get_cpu_pmd_regs(int cpu);

/*
 * PMU shadow registers
 *
 * Many of the registers in the performance monitoring unit are write-only,
 * so we need to save a copy of what we write to those registers.
 *
 * The actual data counters are read/write. However, writing to the counters
 * only takes effect if the PMU is enabled. Otherwise the value is stored in
 * a hardware latch until the next time the PMU is enabled. So we save a copy
 * of the counter values if we need to read them back while the PMU is
 * disabled. The counter_value_in_latch field is a bitmap indicating which
 * counters currently have a value waiting to be written.
 */

struct cbe_pmd_shadow_regs {
	u32 group_control;
	u32 debug_bus_control;
	u32 trace_address;
	u32 ext_tr_timer;
	u32 pm_status;
	u32 pm_control;
	u32 pm_interval;
	u32 pm_start_stop;
	u32 pm07_control[NR_CTRS];

	u32 pm_ctr[NR_PHYS_CTRS];
	u32 counter_value_in_latch;
};

extern struct cbe_pmd_shadow_regs *cbe_get_pmd_shadow_regs(struct device_node *np);
extern struct cbe_pmd_shadow_regs *cbe_get_cpu_pmd_shadow_regs(int cpu);


struct cbe_iic_pending_bits {
	u32 data;
	u8 flags;
	u8 class;
	u8 source;
	u8 prio;
};

#define CBE_IIC_IRQ_VALID	0x80
#define CBE_IIC_IRQ_IPI		0x40

struct cbe_iic_thread_regs {
	struct cbe_iic_pending_bits pending;
	struct cbe_iic_pending_bits pending_destr;
	u64 generate;
	u64 prio;
};

struct cbe_iic_regs {
	u8	pad_0x0000_0x0400[0x0400 - 0x0000];		

	
	struct	cbe_iic_thread_regs thread[2];			

	u64	iic_ir;						
#define CBE_IIC_IR_PRIO(x)      (((x) & 0xf) << 12)
#define CBE_IIC_IR_DEST_NODE(x) (((x) & 0xf) << 4)
#define CBE_IIC_IR_DEST_UNIT(x) ((x) & 0xf)
#define CBE_IIC_IR_IOC_0        0x0
#define CBE_IIC_IR_IOC_1S       0xb
#define CBE_IIC_IR_PT_0         0xe
#define CBE_IIC_IR_PT_1         0xf

	u64	iic_is;						
#define CBE_IIC_IS_PMI		0x2

	u8	pad_0x0450_0x0500[0x0500 - 0x0450];		

	
	u64	ioc_fir_reset;					
	u64	ioc_fir_set;					
	u64	ioc_checkstop_enable;				
	u64	ioc_fir_error_mask;				
	u64	ioc_syserr_enable;				
	u64	ioc_fir;					

	u8	pad_0x0530_0x1000[0x1000 - 0x0530];		
};

extern struct cbe_iic_regs __iomem *cbe_get_iic_regs(struct device_node *np);
extern struct cbe_iic_regs __iomem *cbe_get_cpu_iic_regs(int cpu);


struct cbe_mic_tm_regs {
	u8	pad_0x0000_0x0040[0x0040 - 0x0000];		

	u64	mic_ctl_cnfg2;					
#define CBE_MIC_ENABLE_AUX_TRC		0x8000000000000000LL
#define CBE_MIC_DISABLE_PWR_SAV_2	0x0200000000000000LL
#define CBE_MIC_DISABLE_AUX_TRC_WRAP	0x0100000000000000LL
#define CBE_MIC_ENABLE_AUX_TRC_INT	0x0080000000000000LL

	u64	pad_0x0048;					

	u64	mic_aux_trc_base;				
	u64	mic_aux_trc_max_addr;				
	u64	mic_aux_trc_cur_addr;				
	u64	mic_aux_trc_grf_addr;				
	u64	mic_aux_trc_grf_data;				

	u64	pad_0x0078;					

	u64	mic_ctl_cnfg_0;					
#define CBE_MIC_DISABLE_PWR_SAV_0	0x8000000000000000LL

	u64	pad_0x0088;					

	u64	slow_fast_timer_0;				
	u64	slow_next_timer_0;				

	u8	pad_0x00a0_0x00f8[0x00f8 - 0x00a0];		
	u64    	mic_df_ecc_address_0;				

	u8	pad_0x0100_0x01b8[0x01b8 - 0x0100];		
	u64    	mic_df_ecc_address_1;				

	u64	mic_ctl_cnfg_1;					
#define CBE_MIC_DISABLE_PWR_SAV_1	0x8000000000000000LL

	u64	pad_0x01c8;					

	u64	slow_fast_timer_1;				
	u64	slow_next_timer_1;				

	u8	pad_0x01e0_0x0208[0x0208 - 0x01e0];		
	u64	mic_exc;					
#define CBE_MIC_EXC_BLOCK_SCRUB		0x0800000000000000ULL
#define CBE_MIC_EXC_FAST_SCRUB		0x0100000000000000ULL

	u64	mic_mnt_cfg;					
#define CBE_MIC_MNT_CFG_CHAN_0_POP	0x0002000000000000ULL
#define CBE_MIC_MNT_CFG_CHAN_1_POP	0x0004000000000000ULL

	u64	mic_df_config;					
#define CBE_MIC_ECC_DISABLE_0		0x4000000000000000ULL
#define CBE_MIC_ECC_REP_SINGLE_0	0x2000000000000000ULL
#define CBE_MIC_ECC_DISABLE_1		0x0080000000000000ULL
#define CBE_MIC_ECC_REP_SINGLE_1	0x0040000000000000ULL

	u8	pad_0x0220_0x0230[0x0230 - 0x0220];		
	u64	mic_fir;					
#define CBE_MIC_FIR_ECC_SINGLE_0_ERR	0x0200000000000000ULL
#define CBE_MIC_FIR_ECC_MULTI_0_ERR	0x0100000000000000ULL
#define CBE_MIC_FIR_ECC_SINGLE_1_ERR	0x0080000000000000ULL
#define CBE_MIC_FIR_ECC_MULTI_1_ERR	0x0040000000000000ULL
#define CBE_MIC_FIR_ECC_ERR_MASK	0xffff000000000000ULL
#define CBE_MIC_FIR_ECC_SINGLE_0_CTE	0x0000020000000000ULL
#define CBE_MIC_FIR_ECC_MULTI_0_CTE	0x0000010000000000ULL
#define CBE_MIC_FIR_ECC_SINGLE_1_CTE	0x0000008000000000ULL
#define CBE_MIC_FIR_ECC_MULTI_1_CTE	0x0000004000000000ULL
#define CBE_MIC_FIR_ECC_CTE_MASK	0x0000ffff00000000ULL
#define CBE_MIC_FIR_ECC_SINGLE_0_RESET	0x0000000002000000ULL
#define CBE_MIC_FIR_ECC_MULTI_0_RESET	0x0000000001000000ULL
#define CBE_MIC_FIR_ECC_SINGLE_1_RESET	0x0000000000800000ULL
#define CBE_MIC_FIR_ECC_MULTI_1_RESET	0x0000000000400000ULL
#define CBE_MIC_FIR_ECC_RESET_MASK	0x00000000ffff0000ULL
#define CBE_MIC_FIR_ECC_SINGLE_0_SET	0x0000000000000200ULL
#define CBE_MIC_FIR_ECC_MULTI_0_SET	0x0000000000000100ULL
#define CBE_MIC_FIR_ECC_SINGLE_1_SET	0x0000000000000080ULL
#define CBE_MIC_FIR_ECC_MULTI_1_SET	0x0000000000000040ULL
#define CBE_MIC_FIR_ECC_SET_MASK	0x000000000000ffffULL
	u64	mic_fir_debug;					

	u8	pad_0x0240_0x1000[0x1000 - 0x0240];		
};

extern struct cbe_mic_tm_regs __iomem *cbe_get_mic_tm_regs(struct device_node *np);
extern struct cbe_mic_tm_regs __iomem *cbe_get_cpu_mic_tm_regs(int cpu);


#define CBE_IOPTE_PP_W		0x8000000000000000ul 
#define CBE_IOPTE_PP_R		0x4000000000000000ul 
#define CBE_IOPTE_M		0x2000000000000000ul 
#define CBE_IOPTE_SO_R		0x1000000000000000ul 
#define CBE_IOPTE_SO_RW		0x1800000000000000ul 
#define CBE_IOPTE_RPN_Mask	0x07fffffffffff000ul 
#define CBE_IOPTE_H		0x0000000000000800ul 
#define CBE_IOPTE_IOID_Mask	0x00000000000007fful 

extern u32 cbe_get_hw_thread_id(int cpu);
extern u32 cbe_cpu_to_node(int cpu);
extern u32 cbe_node_to_cpu(int node);

extern void cbe_regs_init(void);


#endif 
