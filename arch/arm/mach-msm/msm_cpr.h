/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CPR_H
#define __ARCH_ARM_MACH_MSM_CPR_H


#define RBCPR_GCNT_TARGET(n)	(0x60 + 4 * n)

#define RBCPR_TIMER_INTERVAL	0x44
#define RBIF_TIMER_ADJUST	0x4C

#define RBIF_LIMIT		0x48
#define RBCPR_STEP_QUOT		0X80
#define RBCPR_CTL		0x90
#define RBIF_SW_VLEVEL		0x94
#define RBIF_CONT_ACK_CMD	0x98
#define RBIF_CONT_NACK_CMD	0x9C

#define RBCPR_RESULT_0		0xA0
#define RBCPR_RESULT_1		0xA4
#define RBCPR_QUOT_AVG		0x118

#define RBCPR_DEBUG1		0x120

#define RBIF_IRQ_EN(n)		(0x100 + 4 * n)
#define RBIF_IRQ_CLEAR		0x110
#define RBIF_IRQ_STATUS		0x114

#define GCNT_M				0x003FF000
#define TARGET_M			0x00000FFF
#define SW_VLEVEL_M			0x0000003F
#define UP_FLAG_M			0x00000010
#define DOWN_FLAG_M			0x00000004
#define CEILING_M			0x00000FC0
#define FLOOR_M				0x0000003F
#define LOOP_EN_M			0x00000001
#define TIMER_M				0x00000008
#define SW_AUTO_CONT_ACK_EN_M		0x00000020
#define SW_AUTO_CONT_NACK_DN_EN_M	0x00000040
#define HW_TO_PMIC_EN_M			BIT(4)
#define BUSY_M				BIT(19)
#define QUOT_SLOW_M			0x00FFF000
#define UP_THRESHOLD_M			0x0F000000
#define DN_THRESHOLD_M			0xF0000000

#define ENABLE_CPR		BIT(0)
#define DISABLE_CPR		0x0
#define ENABLE_TIMER		BIT(3)
#define DISABLE_TIMER		0x0
#define SW_MODE			0x0
#define SW_AUTO_CONT_ACK_EN	BIT(5)
#define SW_AUTO_CONT_NACK_DN_EN	BIT(6)

#define RBIF_CONS_DN_SHIFT (0x4)

#define GNT_CNT			0xC0
#define TARGET			0xEFF

#define CEILING_V		0x30
#define FLOOR_V			0x15

#define SW_LEVEL		0x20

#define INT_MASK (MIN_INT | DOWN_INT | MID_INT | UP_INT | MAX_INT)

#define NUM_OSC 8

#define CPR_MODE 2

enum cpr_mode {
	NORMAL_MODE = 0,
	TURBO_MODE,
	SVS_MODE,
};

enum cpr_action {
	DOWN = 0,
	UP,
};

enum cpr_interrupt {
	DONE_INT	= BIT(0),
	MIN_INT		= BIT(1),
	DOWN_INT	= BIT(2),
	MID_INT		= BIT(3),
	UP_INT		= BIT(4),
	MAX_INT		= BIT(5),
};

struct msm_cpr_osc {
	int gcnt;
	uint32_t quot;
};

struct msm_cpr_mode {
	struct msm_cpr_osc ring_osc_data[NUM_OSC];
	int ring_osc;
	int32_t tgt_volt_offset;
	uint32_t step_quot;
	uint32_t turbo_Vmax;
	uint32_t turbo_Vmin;
	uint32_t nom_Vmax;
	uint32_t nom_Vmin;
	uint32_t calibrated_uV;
};

struct msm_cpr_config {
	unsigned long ref_clk_khz;
	unsigned long delay_us;
	int irq_line;
	struct msm_cpr_mode *cpr_mode_data;
	int min_down_step;
	uint32_t tgt_count_div_N; 
	uint32_t floor;
	uint32_t ceiling;
	uint32_t sw_vlevel;
	uint32_t up_threshold;
	uint32_t dn_threshold;
	uint32_t up_margin;
	uint32_t dn_margin;
	uint32_t max_nom_freq;
	uint32_t max_freq;
	uint32_t max_quot;
	bool disable_cpr;
	uint32_t step_size;
	uint32_t (*get_quot)(uint32_t max_quot, uint32_t max_freq,
				uint32_t new_freq);
	void (*clk_enable)(void);
};

struct msm_cpr_reg {
	uint32_t rbif_timer_interval;
	uint32_t rbif_int_en;
	uint32_t rbif_limit;
	uint32_t rbif_timer_adjust;
	uint32_t rbcpr_gcnt_target;
	uint32_t rbcpr_step_quot;
	uint32_t rbif_sw_level;
	uint32_t rbcpr_ctl;
};

#if defined(CONFIG_MSM_CPR) || defined(CONFIG_MSM_CPR_MODULE)
void msm_cpr_pm_resume(void);
void msm_cpr_pm_suspend(void);
void msm_cpr_enable(void);
void msm_cpr_disable(void);
#else
void msm_cpr_pm_resume(void) { }
void msm_cpr_pm_suspend(void) { }
void msm_cpr_enable(void) { }
void msm_cpr_disable(void) { }
#endif

#ifdef CONFIG_DEBUG_FS
int msm_cpr_debug_init(void *);
#else
static inline int msm_cpr_debug_init(void *) { return 0; }
#endif
#endif 
