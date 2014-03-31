/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>

#include <asm/cpu.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>
#include <mach/rpm-regulator.h>

#include "acpuclock.h"
#include "avs.h"

#define SHOT_SWITCH		4
#define HOP_SWITCH		5
#define SIMPLE_SLEW		6
#define COMPLEX_SLEW		7

#define L_VAL_SCPLL_CAL_MIN	0x08 

#define MAX_VDD_SC		1325000 
#define MAX_VDD_MEM		1325000 
#define MAX_VDD_DIG		1200000 
#define MAX_AXI			 310500 
#define SCPLL_LOW_VDD_FMAX	 594000 
#define SCPLL_LOW_VDD		1000000 
#define SCPLL_NOMINAL_VDD	1100000 

#define SCPLL_POWER_DOWN	0
#define SCPLL_BYPASS		1
#define SCPLL_STANDBY		2
#define SCPLL_FULL_CAL		4
#define SCPLL_HALF_CAL		5
#define SCPLL_STEP_CAL		6
#define SCPLL_NORMAL		7

#define SCPLL_DEBUG_NONE	0
#define SCPLL_DEBUG_FULL	3

#define SCPLL_DEBUG_OFFSET		0x0
#define SCPLL_CTL_OFFSET		0x4
#define SCPLL_CAL_OFFSET		0x8
#define SCPLL_STATUS_OFFSET		0x10
#define SCPLL_CFG_OFFSET		0x1C
#define SCPLL_FSM_CTL_EXT_OFFSET	0x24
#define SCPLL_LUT_OFFSET(l_val)		(0x38 + (((l_val) / 4) * 4))

#define SPSS0_CLK_CTL_ADDR		(MSM_ACC0_BASE + 0x04)
#define SPSS0_CLK_SEL_ADDR		(MSM_ACC0_BASE + 0x08)
#define SPSS1_CLK_CTL_ADDR		(MSM_ACC1_BASE + 0x04)
#define SPSS1_CLK_SEL_ADDR		(MSM_ACC1_BASE + 0x08)
#define SPSS_L2_CLK_SEL_ADDR		(MSM_GCC_BASE  + 0x38)

#define QFPROM_PTE_EFUSE_ADDR		(MSM_QFPROM_BASE + 0x00C0)

static const void * const clk_ctl_addr[] = {SPSS0_CLK_CTL_ADDR,
			SPSS1_CLK_CTL_ADDR};
static const void * const clk_sel_addr[] = {SPSS0_CLK_SEL_ADDR,
			SPSS1_CLK_SEL_ADDR, SPSS_L2_CLK_SEL_ADDR};

static const int rpm_vreg_voter[] = { RPM_VREG_VOTER1, RPM_VREG_VOTER2 };
static struct regulator *regulator_sc[NR_CPUS];

enum scplls {
	CPU0 = 0,
	CPU1,
	L2,
};

static const void * const sc_pll_base[] = {
	[CPU0]	= MSM_SCPLL_BASE + 0x200,
	[CPU1]	= MSM_SCPLL_BASE + 0x300,
	[L2]	= MSM_SCPLL_BASE + 0x400,
};

enum sc_src {
	ACPU_AFAB,
	ACPU_PLL_8,
	ACPU_SCPLL,
};

static struct clock_state {
	struct clkctl_acpu_speed	*current_speed[NR_CPUS];
	struct clkctl_l2_speed		*current_l2_speed;
	spinlock_t			l2_lock;
	struct mutex			lock;
} drv_state;

struct clkctl_l2_speed {
	unsigned int     khz;
	unsigned int     src_sel;
	unsigned int     l_val;
	unsigned int     vdd_dig;
	unsigned int     vdd_mem;
	unsigned int     bw_level;
};

static struct clkctl_l2_speed *l2_vote[NR_CPUS];

struct clkctl_acpu_speed {
	unsigned int     use_for_scaling[2]; 
	unsigned int     acpuclk_khz;
	int              pll;
	unsigned int     acpuclk_src_sel;
	unsigned int     acpuclk_src_div;
	unsigned int     core_src_sel;
	unsigned int     l_val;
	struct clkctl_l2_speed *l2_level;
	unsigned int     vdd_sc;
	unsigned int     avsdscr_setting;
};

#define BW_MBPS(_bw) \
	{ \
		.vectors = &(struct msm_bus_vectors){ \
			.src = MSM_BUS_MASTER_AMPSS_M0, \
			.dst = MSM_BUS_SLAVE_EBI_CH0, \
			.ib = (_bw) * 1000000UL, \
			.ab = 0, \
		}, \
		.num_paths = 1, \
	}
static struct msm_bus_paths bw_level_tbl[] = {
	[0] =  BW_MBPS(824), 
	[1] = BW_MBPS(1336), 
	[2] = BW_MBPS(2008), 
	[3] = BW_MBPS(2480), 
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclock",
};

static uint32_t bus_perf_client;

static struct clkctl_l2_speed l2_freq_tbl_v2[] = {
	[0]  = { MAX_AXI, 0, 0,    1000000, 1100000, 0},
	[1]  = { 432000,  1, 0x08, 1000000, 1100000, 0},
	[2]  = { 486000,  1, 0x09, 1000000, 1100000, 0},
	[3]  = { 540000,  1, 0x0A, 1000000, 1100000, 0},
	[4]  = { 594000,  1, 0x0B, 1000000, 1100000, 0},
	[5]  = { 648000,  1, 0x0C, 1000000, 1100000, 1},
	[6]  = { 702000,  1, 0x0D, 1100000, 1100000, 1},
	[7]  = { 756000,  1, 0x0E, 1100000, 1100000, 1},
	[8]  = { 810000,  1, 0x0F, 1100000, 1100000, 1},
	[9]  = { 864000,  1, 0x10, 1100000, 1100000, 1},
	[10] = { 918000,  1, 0x11, 1100000, 1100000, 2},
	[11] = { 972000,  1, 0x12, 1100000, 1100000, 2},
	[12] = {1026000,  1, 0x13, 1100000, 1100000, 2},
	[13] = {1080000,  1, 0x14, 1100000, 1200000, 2},
	[14] = {1134000,  1, 0x15, 1100000, 1200000, 2},
	[15] = {1188000,  1, 0x16, 1200000, 1200000, 3},
	[16] = {1242000,  1, 0x17, 1200000, 1212500, 3},
	[17] = {1296000,  1, 0x18, 1200000, 1225000, 3},
	[18] = {1350000,  1, 0x19, 1200000, 1225000, 3},
	[19] = {1404000,  1, 0x1A, 1200000, 1250000, 3},
};

#define L2(x) (&l2_freq_tbl_v2[(x)])
static struct clkctl_acpu_speed acpu_freq_tbl_1188mhz[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   812500, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   875000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   875000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   887500, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   912500, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   925000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   937500, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   950000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   975000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),  1000000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),  1012500, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),  1037500, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10), 1062500, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11), 1087500, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12), 1125000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1137500, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1162500, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1187500, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1512mhz_slow[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   800000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   825000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   825000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   850000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   850000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   875000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   875000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   900000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   900000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   925000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   975000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   975000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10), 1000000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11), 1025000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12), 1025000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1050000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1075000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1100000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1125000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1150000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1150000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1175000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1200000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1225000, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1512mhz_nom[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   800000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   825000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   825000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   850000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   850000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   875000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   875000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   900000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   900000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   925000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   950000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   975000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  975000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11), 1000000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12), 1000000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1025000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1025000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1050000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1075000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1100000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1125000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1150000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1150000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1175000, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1512mhz_fast[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   800000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   825000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   825000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   850000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   850000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   875000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   875000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   900000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   900000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   925000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   925000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   950000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  950000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11),  950000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12),  975000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13), 1000000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1000000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1025000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1050000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1075000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1100000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1100000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1100000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1125000, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1674mhz_slower[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   775000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   775000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   775000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   775000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   775000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   787500, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   800000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   825000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   837500, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   850000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   875000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   900000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  912500, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11),  937500, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12),  962500, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13),  987500, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14), 1012500, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1025000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1062500, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1087500, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1100000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1125000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1150000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1187500, 0x03006000},
  { {1, 1}, 1566000,  ACPU_SCPLL, 0, 0, 1, 0x1D, L2(19), 1225000, 0x03006000},
  { {1, 1}, 1620000,  ACPU_SCPLL, 0, 0, 1, 0x1E, L2(19), 1262500, 0x03006000},
  { {1, 1}, 1674000,  ACPU_SCPLL, 0, 0, 1, 0x1F, L2(19), 1300000, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1674mhz_slow[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   775000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   775000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   775000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   775000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   775000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   787500, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   800000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   825000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   837500, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   850000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   862500, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   887500, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  900000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11),  925000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12),  937500, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13),  962500, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14),  987500, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15), 1000000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16), 1025000, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1050000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1062500, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1087500, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1112500, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1150000, 0x03006000},
  { {1, 1}, 1566000,  ACPU_SCPLL, 0, 0, 1, 0x1D, L2(19), 1175000, 0x03006000},
  { {1, 1}, 1620000,  ACPU_SCPLL, 0, 0, 1, 0x1E, L2(19), 1212500, 0x03006000},
  { {1, 1}, 1674000,  ACPU_SCPLL, 0, 0, 1, 0x1F, L2(19), 1250000, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1674mhz_nom[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   775000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   775000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   775000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   775000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   775000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   787500, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   800000, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   812500, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   825000, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   837500, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   850000, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   875000, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  887500, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11),  900000, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12),  912500, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13),  937500, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14),  950000, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15),  975000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16),  987500, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17), 1012500, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1025000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1050000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1075000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1112500, 0x03006000},
  { {1, 1}, 1566000,  ACPU_SCPLL, 0, 0, 1, 0x1D, L2(19), 1137500, 0x03006000},
  { {1, 1}, 1620000,  ACPU_SCPLL, 0, 0, 1, 0x1E, L2(19), 1175000, 0x03006000},
  { {1, 1}, 1674000,  ACPU_SCPLL, 0, 0, 1, 0x1F, L2(19), 1200000, 0x03006000},
  { {0, 0}, 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_1674mhz_fast[] = {
  { {1, 1},  192000,  ACPU_PLL_8, 3, 1, 0, 0,    L2(1),   775000, 0x03006000},
  
  { {0, 0},  MAX_AXI, ACPU_AFAB,  1, 0, 0, 0,    L2(0),   775000, 0x03006000},
  { {1, 1},  384000,  ACPU_PLL_8, 3, 0, 0, 0,    L2(1),   775000, 0x03006000},
  { {1, 1},  432000,  ACPU_SCPLL, 0, 0, 1, 0x08, L2(1),   775000, 0x03006000},
  { {1, 1},  486000,  ACPU_SCPLL, 0, 0, 1, 0x09, L2(2),   775000, 0x03006000},
  { {1, 1},  540000,  ACPU_SCPLL, 0, 0, 1, 0x0A, L2(3),   775000, 0x03006000},
  { {1, 1},  594000,  ACPU_SCPLL, 0, 0, 1, 0x0B, L2(4),   787500, 0x03006000},
  { {1, 1},  648000,  ACPU_SCPLL, 0, 0, 1, 0x0C, L2(5),   800000, 0x03006000},
  { {1, 1},  702000,  ACPU_SCPLL, 0, 0, 1, 0x0D, L2(6),   812500, 0x03006000},
  { {1, 1},  756000,  ACPU_SCPLL, 0, 0, 1, 0x0E, L2(7),   825000, 0x03006000},
  { {1, 1},  810000,  ACPU_SCPLL, 0, 0, 1, 0x0F, L2(8),   837500, 0x03006000},
  { {1, 1},  864000,  ACPU_SCPLL, 0, 0, 1, 0x10, L2(9),   862500, 0x03006000},
  { {1, 1},  918000,  ACPU_SCPLL, 0, 0, 1, 0x11, L2(10),  875000, 0x03006000},
  { {1, 1},  972000,  ACPU_SCPLL, 0, 0, 1, 0x12, L2(11),  887500, 0x03006000},
  { {1, 1}, 1026000,  ACPU_SCPLL, 0, 0, 1, 0x13, L2(12),  900000, 0x03006000},
  { {1, 1}, 1080000,  ACPU_SCPLL, 0, 0, 1, 0x14, L2(13),  925000, 0x03006000},
  { {1, 1}, 1134000,  ACPU_SCPLL, 0, 0, 1, 0x15, L2(14),  937500, 0x03006000},
  { {1, 1}, 1188000,  ACPU_SCPLL, 0, 0, 1, 0x16, L2(15),  950000, 0x03006000},
  { {1, 1}, 1242000,  ACPU_SCPLL, 0, 0, 1, 0x17, L2(16),  962500, 0x03006000},
  { {1, 1}, 1296000,  ACPU_SCPLL, 0, 0, 1, 0x18, L2(17),  975000, 0x03006000},
  { {1, 1}, 1350000,  ACPU_SCPLL, 0, 0, 1, 0x19, L2(18), 1000000, 0x03006000},
  { {1, 1}, 1404000,  ACPU_SCPLL, 0, 0, 1, 0x1A, L2(19), 1025000, 0x03006000},
  { {1, 1}, 1458000,  ACPU_SCPLL, 0, 0, 1, 0x1B, L2(19), 1050000, 0x03006000},
  { {1, 1}, 1512000,  ACPU_SCPLL, 0, 0, 1, 0x1C, L2(19), 1075000, 0x03006000},
  { {1, 1}, 1566000,  ACPU_SCPLL, 0, 0, 1, 0x1D, L2(19), 1100000, 0x03006000},
  { {1, 1}, 1620000,  ACPU_SCPLL, 0, 0, 1, 0x1E, L2(19), 1125000, 0x03006000},
  { {1, 1}, 1674000,  ACPU_SCPLL, 0, 0, 1, 0x1F, L2(19), 1150000, 0x03006000},
  { {0, 0}, 0 },
};

#define CAL_IDX 1

static struct clkctl_acpu_speed *acpu_freq_tbl;
static struct clkctl_l2_speed *l2_freq_tbl = l2_freq_tbl_v2;
static unsigned int l2_freq_tbl_size = ARRAY_SIZE(l2_freq_tbl_v2);

static unsigned long acpuclk_8x60_get_rate(int cpu)
{
	return drv_state.current_speed[cpu]->acpuclk_khz;
}

static void select_core_source(unsigned int id, unsigned int src)
{
	uint32_t regval;
	int shift;

	shift = (id == L2) ? 0 : 1;
	regval = readl_relaxed(clk_sel_addr[id]);
	regval &= ~(0x3 << shift);
	regval |= (src << shift);
	writel_relaxed(regval, clk_sel_addr[id]);
}

static void select_clk_source_div(unsigned int id, struct clkctl_acpu_speed *s)
{
	uint32_t reg_clksel, reg_clkctl, src_sel;

	
	if (s->core_src_sel == 0) {

		reg_clksel = readl_relaxed(clk_sel_addr[id]);

		
		src_sel = reg_clksel & 1;

		
		reg_clkctl = readl_relaxed(clk_ctl_addr[id]);
		reg_clkctl &= ~(0xFF << (8 * src_sel));
		reg_clkctl |= s->acpuclk_src_sel << (4 + 8 * src_sel);
		reg_clkctl |= s->acpuclk_src_div << (0 + 8 * src_sel);
		writel_relaxed(reg_clkctl, clk_ctl_addr[id]);

		
		reg_clksel ^= 1;

		
		writel_relaxed(reg_clksel, clk_sel_addr[id]);
	}
}

static void scpll_enable(int sc_pll, uint32_t l_val)
{
	uint32_t regval;

	
	writel_relaxed(SCPLL_STANDBY, sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
	mb();
	udelay(10);

	
	regval = (l_val << 3) | SHOT_SWITCH;
	writel_relaxed(regval, sc_pll_base[sc_pll] + SCPLL_FSM_CTL_EXT_OFFSET);
	writel_relaxed(SCPLL_NORMAL, sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
	mb();
	udelay(20);
}

static void scpll_disable(int sc_pll)
{
	
	writel_relaxed(SCPLL_POWER_DOWN,
		       sc_pll_base[sc_pll] + SCPLL_CTL_OFFSET);
}

static void scpll_change_freq(int sc_pll, uint32_t l_val)
{
	uint32_t regval;
	const void *base_addr = sc_pll_base[sc_pll];

	
	regval = (l_val << 3) | COMPLEX_SLEW;
	writel_relaxed(regval, base_addr + SCPLL_FSM_CTL_EXT_OFFSET);
	writel_relaxed(SCPLL_NORMAL, base_addr + SCPLL_CTL_OFFSET);

	
	while (((readl_relaxed(base_addr + SCPLL_CTL_OFFSET) >> 3) & 0x3F)
			!= l_val)
		cpu_relax();
	
	while (readl_relaxed(base_addr + SCPLL_STATUS_OFFSET) & 0x1)
		cpu_relax();
}

static struct clkctl_l2_speed *compute_l2_speed(unsigned int voting_cpu,
						struct clkctl_l2_speed *tgt_s)
{
	struct clkctl_l2_speed *new_s;
	int cpu;

	
	BUG_ON(tgt_s >= (l2_freq_tbl + l2_freq_tbl_size));

	
	l2_vote[voting_cpu] = tgt_s;
	new_s = l2_freq_tbl;
	for_each_present_cpu(cpu)
		new_s = max(new_s, l2_vote[cpu]);

	return new_s;
}

static void set_l2_speed(struct clkctl_l2_speed *tgt_s)
{
	if (tgt_s == drv_state.current_l2_speed)
		return;

	if (drv_state.current_l2_speed->src_sel == 1
				&& tgt_s->src_sel == 1)
		scpll_change_freq(L2, tgt_s->l_val);
	else {
		if (tgt_s->src_sel == 1) {
			scpll_enable(L2, tgt_s->l_val);
			mb();
			select_core_source(L2, tgt_s->src_sel);
		} else {
			select_core_source(L2, tgt_s->src_sel);
			mb();
			scpll_disable(L2);
		}
	}
	drv_state.current_l2_speed = tgt_s;
}

static void set_bus_bw(unsigned int bw)
{
	int ret;

	
	if (bw >= ARRAY_SIZE(bw_level_tbl)) {
		pr_err("%s: invalid bandwidth request (%d)\n", __func__, bw);
		return;
	}

	
	ret = msm_bus_scale_client_update_request(bus_perf_client, bw);
	if (ret)
		pr_err("%s: bandwidth request failed (%d)\n", __func__, ret);

	return;
}

static int increase_vdd(int cpu, unsigned int vdd_sc, unsigned int vdd_mem,
			unsigned int vdd_dig, enum setrate_reason reason)
{
	int rc = 0;

	rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S0, rpm_vreg_voter[cpu],
				  vdd_mem, MAX_VDD_MEM, 0);
	if (rc) {
		pr_err("%s: vdd_mem (cpu%d) increase failed (%d)\n",
			__func__, cpu, rc);
		return rc;
	}

	
	rc = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S1, rpm_vreg_voter[cpu],
				  vdd_dig, MAX_VDD_DIG, 0);
	if (rc) {
		pr_err("%s: vdd_dig (cpu%d) increase failed (%d)\n",
			__func__, cpu, rc);
		return rc;
	}

	if (reason == SETRATE_HOTPLUG)
		return rc;

	
	rc = regulator_set_voltage(regulator_sc[cpu], vdd_sc, MAX_VDD_SC);
	if (rc) {
		pr_err("%s: vdd_sc (cpu%d) increase failed (%d)\n",
			__func__, cpu, rc);
		return rc;
	}

	return rc;
}

static void decrease_vdd(int cpu, unsigned int vdd_sc, unsigned int vdd_mem,
			 unsigned int vdd_dig, enum setrate_reason reason)
{
	int ret;

	if (reason != SETRATE_HOTPLUG) {
		ret = regulator_set_voltage(regulator_sc[cpu], vdd_sc,
					    MAX_VDD_SC);
		if (ret) {
			pr_err("%s: vdd_sc (cpu%d) decrease failed (%d)\n",
				__func__, cpu, ret);
			return;
		}
	}

	
	ret = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S1, rpm_vreg_voter[cpu],
				   vdd_dig, MAX_VDD_DIG, 0);
	if (ret) {
		pr_err("%s: vdd_dig (cpu%d) decrease failed (%d)\n",
			__func__, cpu, ret);
		return;
	}

	ret = rpm_vreg_set_voltage(RPM_VREG_ID_PM8058_S0, rpm_vreg_voter[cpu],
				   vdd_mem, MAX_VDD_MEM, 0);
	if (ret) {
		pr_err("%s: vdd_mem (cpu%d) decrease failed (%d)\n",
			__func__, cpu, ret);
		return;
	}
}

static void switch_sc_speed(int cpu, struct clkctl_acpu_speed *tgt_s)
{
	struct clkctl_acpu_speed *strt_s = drv_state.current_speed[cpu];

	if (strt_s->pll != ACPU_SCPLL && tgt_s->pll != ACPU_SCPLL) {
		select_clk_source_div(cpu, tgt_s);
		
		select_core_source(cpu, tgt_s->core_src_sel);
	} else if (strt_s->pll != ACPU_SCPLL && tgt_s->pll == ACPU_SCPLL) {
		scpll_enable(cpu, tgt_s->l_val);
		mb();
		select_core_source(cpu, tgt_s->core_src_sel);
	} else if (strt_s->pll == ACPU_SCPLL && tgt_s->pll != ACPU_SCPLL) {
		select_clk_source_div(cpu, tgt_s);
		select_core_source(cpu, tgt_s->core_src_sel);
		
		mb();
		udelay(1);
		scpll_disable(cpu);
	} else
		scpll_change_freq(cpu, tgt_s->l_val);

	
	drv_state.current_speed[cpu] = tgt_s;
}

static int acpuclk_8x60_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	struct clkctl_l2_speed *tgt_l2;
	unsigned int vdd_mem, vdd_dig, pll_vdd_dig;
	unsigned long flags;
	int rc = 0;

	if (cpu > num_possible_cpus())
		return -EINVAL;

	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed[cpu];

	
	if (rate == strt_s->acpuclk_khz)
		goto out;

	
	for (tgt_s = acpu_freq_tbl; tgt_s->acpuclk_khz != 0; tgt_s++)
		if (tgt_s->acpuclk_khz == rate)
			break;
	if (tgt_s->acpuclk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ)
		AVS_DISABLE(cpu);

	vdd_mem = max(tgt_s->vdd_sc, tgt_s->l2_level->vdd_mem);
	
	if ((tgt_s->l2_level->khz > SCPLL_LOW_VDD_FMAX) ||
	    (tgt_s->pll == ACPU_SCPLL
	     && tgt_s->acpuclk_khz > SCPLL_LOW_VDD_FMAX))
		pll_vdd_dig = SCPLL_NOMINAL_VDD;
	else
		pll_vdd_dig = SCPLL_LOW_VDD;
	vdd_dig = max(tgt_s->l2_level->vdd_dig, pll_vdd_dig);

	
	if ((reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG
	  || reason == SETRATE_INIT)
			&& (tgt_s->acpuclk_khz > strt_s->acpuclk_khz)) {
		rc = increase_vdd(cpu, tgt_s->vdd_sc, vdd_mem, vdd_dig, reason);
		if (rc)
			goto out;
	}

	pr_debug("Switching from ACPU%d rate %u KHz -> %u KHz\n",
		cpu, strt_s->acpuclk_khz, tgt_s->acpuclk_khz);

	
	switch_sc_speed(cpu, tgt_s);

	
	spin_lock_irqsave(&drv_state.l2_lock, flags);
	tgt_l2 = compute_l2_speed(cpu, tgt_s->l2_level);
	set_l2_speed(tgt_l2);
	spin_unlock_irqrestore(&drv_state.l2_lock, flags);

	
	if (reason == SETRATE_SWFI)
		goto out;

	
	if (reason == SETRATE_PC)
		goto out;

	
	set_bus_bw(tgt_l2->bw_level);

	
	if (tgt_s->acpuclk_khz < strt_s->acpuclk_khz)
		decrease_vdd(cpu, tgt_s->vdd_sc, vdd_mem, vdd_dig, reason);

	pr_debug("ACPU%d speed change complete\n", cpu);

	
	if (reason == SETRATE_CPUFREQ)
		AVS_ENABLE(cpu, tgt_s->avsdscr_setting);

out:
	if (reason == SETRATE_CPUFREQ || reason == SETRATE_HOTPLUG)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __init scpll_init(int pll, unsigned int max_l_val)
{
	uint32_t regval;

	pr_debug("Initializing SCPLL%d\n", pll);

	writel_relaxed(SCPLL_DEBUG_FULL, sc_pll_base[pll] + SCPLL_DEBUG_OFFSET);
	writel_relaxed(0x0, sc_pll_base[pll] + SCPLL_LUT_OFFSET(max_l_val));
	writel_relaxed(SCPLL_DEBUG_NONE, sc_pll_base[pll] + SCPLL_DEBUG_OFFSET);

	
	writel_relaxed(SCPLL_STANDBY, sc_pll_base[pll] + SCPLL_CTL_OFFSET);
	mb();
	udelay(10);

	
	regval = (max_l_val << 24) | (L_VAL_SCPLL_CAL_MIN << 16);
	writel_relaxed(regval, sc_pll_base[pll] + SCPLL_CAL_OFFSET);

	
	writel_relaxed(SCPLL_FULL_CAL, sc_pll_base[pll] + SCPLL_CTL_OFFSET);

	while (!readl_relaxed(sc_pll_base[pll] + SCPLL_LUT_OFFSET(max_l_val)))
		cpu_relax();

	
	while (readl_relaxed(sc_pll_base[pll] + SCPLL_STATUS_OFFSET) & 0x2)
		cpu_relax();

	
	scpll_disable(pll);
}

static void __init unselect_scplls(void)
{
	int cpu;

	
	BUG_ON(acpu_freq_tbl[CAL_IDX].core_src_sel != 0);
	BUG_ON(acpu_freq_tbl[CAL_IDX].l2_level->src_sel != 0);

	for_each_possible_cpu(cpu) {
		select_clk_source_div(cpu, &acpu_freq_tbl[CAL_IDX]);
		select_core_source(cpu, acpu_freq_tbl[CAL_IDX].core_src_sel);
		drv_state.current_speed[cpu] = &acpu_freq_tbl[CAL_IDX];
		l2_vote[cpu] = acpu_freq_tbl[CAL_IDX].l2_level;
	}

	select_core_source(L2, acpu_freq_tbl[CAL_IDX].l2_level->src_sel);
	drv_state.current_l2_speed = acpu_freq_tbl[CAL_IDX].l2_level;
}

static void __init scpll_set_refs(void)
{
	int cpu;
	uint32_t regval;

	
	for_each_possible_cpu(cpu) {
		regval = readl_relaxed(sc_pll_base[cpu] + SCPLL_CFG_OFFSET);
		regval &= ~BIT(4);
		writel_relaxed(regval, sc_pll_base[cpu] + SCPLL_CFG_OFFSET);
	}
	regval = readl_relaxed(sc_pll_base[L2] + SCPLL_CFG_OFFSET);
	regval &= ~BIT(4);
	writel_relaxed(regval, sc_pll_base[L2] + SCPLL_CFG_OFFSET);
}

static void __init regulator_init(void)
{
	struct clkctl_acpu_speed **freq = drv_state.current_speed;
	const char *regulator_sc_name[] = {"8901_s0", "8901_s1"};
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		
		regulator_sc[cpu] = regulator_get(NULL, regulator_sc_name[cpu]);
		if (IS_ERR(regulator_sc[cpu]))
			goto err;
		ret = regulator_set_voltage(regulator_sc[cpu],
				freq[cpu]->vdd_sc, MAX_VDD_SC);
		if (ret)
			goto err;
		ret = regulator_enable(regulator_sc[cpu]);
		if (ret)
			goto err;
	}

	return;

err:
	pr_err("%s: Failed to initialize voltage regulators\n", __func__);
	BUG();
}

static void __init bus_init(void)
{
	bus_perf_client = msm_bus_scale_register_client(&bus_client_pdata);
	if (!bus_perf_client) {
		pr_err("%s: unable register bus client\n", __func__);
		BUG();
	}
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[NR_CPUS][30];

static void __init cpufreq_table_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		int i, freq_cnt = 0;
		
		for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0
				&& freq_cnt < ARRAY_SIZE(*freq_table); i++) {
			if (acpu_freq_tbl[i].use_for_scaling[cpu]) {
				freq_table[cpu][freq_cnt].index = freq_cnt;
				freq_table[cpu][freq_cnt].frequency
					= acpu_freq_tbl[i].acpuclk_khz;
				freq_cnt++;
			}
		}
		
		BUG_ON(acpu_freq_tbl[i].acpuclk_khz != 0);

		freq_table[cpu][freq_cnt].index = freq_cnt;
		freq_table[cpu][freq_cnt].frequency = CPUFREQ_TABLE_END;

		pr_info("CPU%d: %d scaling frequencies supported.\n",
			cpu, freq_cnt);

		
		cpufreq_frequency_table_get_attr(freq_table[cpu], cpu);
	}
}
#else
static void __init cpufreq_table_init(void) {}
#endif

#define HOT_UNPLUG_KHZ MAX_AXI
static int __cpuinit acpuclock_cpu_callback(struct notifier_block *nfb,
					    unsigned long action, void *hcpu)
{
	static int prev_khz[NR_CPUS];
	int cpu = (int)hcpu;

	switch (action) {
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		prev_khz[cpu] = acpuclk_8x60_get_rate(cpu);
		
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		acpuclk_8x60_set_rate(cpu, HOT_UNPLUG_KHZ, SETRATE_HOTPLUG);
		break;
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		if (WARN_ON(!prev_khz[cpu]))
			return NOTIFY_BAD;
		acpuclk_8x60_set_rate(cpu, prev_khz[cpu], SETRATE_HOTPLUG);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata acpuclock_cpu_notifier = {
	.notifier_call = acpuclock_cpu_callback,
};

static __init struct clkctl_acpu_speed *select_freq_plan(void)
{
	uint32_t pte_efuse, speed_bin, pvs;
	struct clkctl_acpu_speed *f;

	pte_efuse = readl_relaxed(QFPROM_PTE_EFUSE_ADDR);

	speed_bin = pte_efuse & 0xF;
	if (speed_bin == 0xF)
		speed_bin = (pte_efuse >> 4) & 0xF;

	pvs = (pte_efuse >> 10) & 0x7;
	if (pvs == 0x7)
		pvs = (pte_efuse >> 13) & 0x7;

	if (speed_bin == 0x2) {
		switch (pvs) {
		case 0x7:
		case 0x4:
			acpu_freq_tbl = acpu_freq_tbl_1674mhz_slower;
			pr_info("ACPU PVS: Slower\n");
			break;
		case 0x0:
			acpu_freq_tbl = acpu_freq_tbl_1674mhz_slow;
			pr_info("ACPU PVS: Slow\n");
			break;
		case 0x1:
			acpu_freq_tbl = acpu_freq_tbl_1674mhz_nom;
			pr_info("ACPU PVS: Nominal\n");
			break;
		case 0x3:
			acpu_freq_tbl = acpu_freq_tbl_1674mhz_fast;
			pr_info("ACPU PVS: Fast\n");
			break;
		default:
			acpu_freq_tbl = acpu_freq_tbl_1674mhz_slower;
			pr_warn("ACPU PVS: Unknown. Defaulting to slower.\n");
			break;
		}
	} else if (speed_bin == 0x1) {
		switch (pvs) {
		case 0x0:
		case 0x7:
			acpu_freq_tbl = acpu_freq_tbl_1512mhz_slow;
			pr_info("ACPU PVS: Slow\n");
			break;
		case 0x1:
			acpu_freq_tbl = acpu_freq_tbl_1512mhz_nom;
			pr_info("ACPU PVS: Nominal\n");
			break;
		case 0x3:
			acpu_freq_tbl = acpu_freq_tbl_1512mhz_fast;
			pr_info("ACPU PVS: Fast\n");
			break;
		default:
			acpu_freq_tbl = acpu_freq_tbl_1512mhz_slow;
			pr_warn("ACPU PVS: Unknown. Defaulting to slow.\n");
			break;
		}
	} else {
		acpu_freq_tbl = acpu_freq_tbl_1188mhz;
	}

	for (f = acpu_freq_tbl; f->acpuclk_khz != 0; f++)
		;
	f--;
	pr_info("Max ACPU freq: %u KHz\n", f->acpuclk_khz);

	return f;
}

static struct acpuclk_data acpuclk_8x60_data = {
	.set_rate = acpuclk_8x60_set_rate,
	.get_rate = acpuclk_8x60_get_rate,
	.power_collapse_khz = MAX_AXI,
	.wait_for_irq_khz = MAX_AXI,
};

static int __init acpuclk_8x60_probe(struct platform_device *pdev)
{
	struct clkctl_acpu_speed *max_freq;
	int cpu;

	mutex_init(&drv_state.lock);
	spin_lock_init(&drv_state.l2_lock);

	
	max_freq = select_freq_plan();
	unselect_scplls();
	scpll_set_refs();
	for_each_possible_cpu(cpu)
		scpll_init(cpu, max_freq->l_val);
	scpll_init(L2, max_freq->l2_level->l_val);
	regulator_init();
	bus_init();

	
	for_each_online_cpu(cpu)
		acpuclk_8x60_set_rate(cpu, max_freq->acpuclk_khz, SETRATE_INIT);

	acpuclk_register(&acpuclk_8x60_data);
	cpufreq_table_init();
	register_hotcpu_notifier(&acpuclock_cpu_notifier);

	return 0;
}

static struct platform_driver acpuclk_8x60_driver = {
	.driver = {
		.name = "acpuclk-8x60",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_8x60_init(void)
{
	return platform_driver_probe(&acpuclk_8x60_driver, acpuclk_8x60_probe);
}
device_initcall(acpuclk_8x60_init);
