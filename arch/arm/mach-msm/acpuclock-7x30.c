/*
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <linux/platform_device.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>

#include "smd_private.h"
#include "acpuclock.h"
#include "spm.h"

#define SCSS_CLK_CTL_ADDR	(MSM_ACC0_BASE + 0x04)
#define SCSS_CLK_SEL_ADDR	(MSM_ACC0_BASE + 0x08)

#define PLL2_L_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x33C)
#define PLL2_M_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x340)
#define PLL2_N_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x344)
#define PLL2_CONFIG_ADDR	(MSM_CLK_CTL_BASE + 0x34C)

#define VREF_SEL     1	
#define V_STEP       (25 * (2 - VREF_SEL)) 
#define VREG_DATA    (VREG_CONFIG | (VREF_SEL << 5))
#define VREG_CONFIG  (BIT(7) | BIT(6)) 
#define MV(mv)      ((mv) / (!((mv) % V_STEP)))
#define VDD_RAW(mv) (((MV(mv) / V_STEP) - 30) | VREG_DATA)

#define MAX_AXI_KHZ 192000

struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	struct clk			*ebi1_clk;
};

struct pll {
	unsigned int l;
	unsigned int m;
	unsigned int n;
	unsigned int pre_div;
};

struct clkctl_acpu_speed {
	unsigned int	use_for_scaling;
	unsigned int	acpu_clk_khz;
	int		src;
	unsigned int	acpu_src_sel;
	unsigned int	acpu_src_div;
	unsigned int	axi_clk_hz;
	unsigned int	vdd_mv;
	unsigned int	vdd_raw;
	struct pll	*pll_rate;
	unsigned long	lpj; 
};

static struct clock_state drv_state = { 0 };

static struct clkctl_acpu_speed *backup_s;

static struct pll pll2_tbl[] = {
	{  42, 0, 1, 0 }, 
	{  53, 1, 3, 0 }, 
	{ 125, 0, 1, 1 }, 
	{  73, 0, 1, 0 }, 
};


enum acpuclk_source {
	LPXO	= -2,
	AXI	= -1,
	PLL_0	=  0,
	PLL_1,
	PLL_2,
	PLL_3,
	MAX_SOURCE
};

static struct clk *acpuclk_sources[MAX_SOURCE];

static struct clkctl_acpu_speed acpu_freq_tbl[] = {
	{ 0, 24576,  LPXO, 0, 0,  30720000,  900, VDD_RAW(900) },
	{ 0, 61440,  PLL_3,    5, 11, 61440000,  900, VDD_RAW(900) },
	{ 1, 122880, PLL_3,    5, 5,  61440000,  900, VDD_RAW(900) },
	{ 0, 184320, PLL_3,    5, 4,  61440000,  900, VDD_RAW(900) },
	{ 0, MAX_AXI_KHZ, AXI, 1, 0, 61440000, 900, VDD_RAW(900) },
	{ 1, 245760, PLL_3,    5, 2,  61440000,  900, VDD_RAW(900) },
	{ 1, 368640, PLL_3,    5, 1,  122800000, 900, VDD_RAW(900) },
	
	{ 1, 768000, PLL_1,    2, 0,  153600000, 1050, VDD_RAW(1050) },
	{ 1, 806400,  PLL_2, 3, 0, UINT_MAX, 1100, VDD_RAW(1100), &pll2_tbl[0]},
	{ 1, 1024000, PLL_2, 3, 0, UINT_MAX, 1200, VDD_RAW(1200), &pll2_tbl[1]},
	{ 1, 1200000, PLL_2, 3, 0, UINT_MAX, 1200, VDD_RAW(1200), &pll2_tbl[2]},
	{ 1, 1401600, PLL_2, 3, 0, UINT_MAX, 1250, VDD_RAW(1250), &pll2_tbl[3]},
	{ 0 }
};

static int acpuclk_set_acpu_vdd(struct clkctl_acpu_speed *s)
{
	int ret = msm_spm_set_vdd(0, s->vdd_raw);
	if (ret)
		return ret;

	
	udelay(62);
	return 0;
}

static void acpuclk_config_pll2(struct pll *pll)
{
	uint32_t config = readl_relaxed(PLL2_CONFIG_ADDR);

	mb();
	writel_relaxed(pll->l, PLL2_L_VAL_ADDR);
	writel_relaxed(pll->m, PLL2_M_VAL_ADDR);
	writel_relaxed(pll->n, PLL2_N_VAL_ADDR);
	if (pll->pre_div)
		config |= BIT(15);
	else
		config &= ~BIT(15);
	writel_relaxed(config, PLL2_CONFIG_ADDR);
	
	mb();
}

static void acpuclk_set_src(const struct clkctl_acpu_speed *s)
{
	uint32_t reg_clksel, reg_clkctl, src_sel;

	reg_clksel = readl_relaxed(SCSS_CLK_SEL_ADDR);

	
	src_sel = reg_clksel & 1;

	
	reg_clkctl = readl_relaxed(SCSS_CLK_CTL_ADDR);
	reg_clkctl &= ~(0xFF << (8 * src_sel));
	reg_clkctl |= s->acpu_src_sel << (4 + 8 * src_sel);
	reg_clkctl |= s->acpu_src_div << (0 + 8 * src_sel);
	writel_relaxed(reg_clkctl, SCSS_CLK_CTL_ADDR);

	
	reg_clksel ^= 1;

	
	writel_relaxed(reg_clksel, SCSS_CLK_SEL_ADDR);

	
	mb();
}

static int acpuclk_7x30_set_rate(int cpu, unsigned long rate,
				 enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int res, rc = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed;

	if (rate == strt_s->acpu_clk_khz)
		goto out;

	for (tgt_s = acpu_freq_tbl; tgt_s->acpu_clk_khz != 0; tgt_s++) {
		if (tgt_s->acpu_clk_khz == rate)
			break;
	}
	if (tgt_s->acpu_clk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ) {
		
		if (tgt_s->vdd_mv > strt_s->vdd_mv) {
			rc = acpuclk_set_acpu_vdd(tgt_s);
			if (rc < 0) {
				pr_err("ACPU VDD increase to %d mV failed "
					"(%d)\n", tgt_s->vdd_mv, rc);
				goto out;
			}
		}
	}

	pr_debug("Switching from ACPU rate %u KHz -> %u KHz\n",
	       strt_s->acpu_clk_khz, tgt_s->acpu_clk_khz);

	if (tgt_s->axi_clk_hz > strt_s->axi_clk_hz) {
		rc = clk_set_rate(drv_state.ebi1_clk, tgt_s->axi_clk_hz);
		if (rc < 0) {
			pr_err("Setting AXI min rate failed (%d)\n", rc);
			goto out;
		}
	}

	
	if (tgt_s->src == PLL_2 && strt_s->src == PLL_2) {
		clk_enable(acpuclk_sources[backup_s->src]);
		acpuclk_set_src(backup_s);
		clk_disable(acpuclk_sources[strt_s->src]);
	}

	
	if (tgt_s->src == PLL_2)
		acpuclk_config_pll2(tgt_s->pll_rate);

	
	if ((strt_s->src != tgt_s->src && tgt_s->src >= 0) ||
	    (tgt_s->src == PLL_2 && strt_s->src == PLL_2)) {
		pr_debug("Enabling PLL %d\n", tgt_s->src);
		clk_enable(acpuclk_sources[tgt_s->src]);
	}

	
	acpuclk_set_src(tgt_s);
	drv_state.current_speed = tgt_s;
	loops_per_jiffy = tgt_s->lpj;

	if (tgt_s->src == PLL_2 && strt_s->src == PLL_2)
		clk_disable(acpuclk_sources[backup_s->src]);

	
	if (reason == SETRATE_SWFI)
		goto out;

	
	if (strt_s->src != tgt_s->src && strt_s->src >= 0) {
		pr_debug("Disabling PLL %d\n", strt_s->src);
		clk_disable(acpuclk_sources[strt_s->src]);
	}

	
	if (tgt_s->axi_clk_hz < strt_s->axi_clk_hz) {
		res = clk_set_rate(drv_state.ebi1_clk, tgt_s->axi_clk_hz);
		if (res < 0)
			pr_warning("Setting AXI min rate failed (%d)\n", res);
	}

	
	if (reason == SETRATE_PC)
		goto out;

	
	if (tgt_s->vdd_mv < strt_s->vdd_mv) {
		res = acpuclk_set_acpu_vdd(tgt_s);
		if (res)
			pr_warning("ACPU VDD decrease to %d mV failed (%d)\n",
					tgt_s->vdd_mv, res);
	}

	pr_debug("ACPU speed change complete\n");
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);

	return rc;
}

static unsigned long acpuclk_7x30_get_rate(int cpu)
{
	WARN_ONCE(drv_state.current_speed == NULL,
		  "acpuclk_get_rate: not initialized\n");
	if (drv_state.current_speed)
		return drv_state.current_speed->acpu_clk_khz;
	else
		return 0;
}


static void __devinit acpuclk_hw_init(void)
{
	struct clkctl_acpu_speed *s;
	uint32_t div, sel, src_num;
	uint32_t reg_clksel, reg_clkctl;
	int res;
	u8 pll2_l = readl_relaxed(PLL2_L_VAL_ADDR) & 0xFF;

	drv_state.ebi1_clk = clk_get(NULL, "ebi1_clk");
	BUG_ON(IS_ERR(drv_state.ebi1_clk));

	reg_clksel = readl_relaxed(SCSS_CLK_SEL_ADDR);

	
	switch ((reg_clksel >> 1) & 0x3) {
	case 0:	
		reg_clkctl = readl_relaxed(SCSS_CLK_CTL_ADDR);
		src_num = reg_clksel & 0x1;
		sel = (reg_clkctl >> (12 - (8 * src_num))) & 0x7;
		div = (reg_clkctl >> (8 -  (8 * src_num))) & 0xF;

		
		for (s = acpu_freq_tbl; s->acpu_clk_khz != 0; s++) {
			if (s->acpu_src_sel == sel && s->acpu_src_div == div)
				break;
		}
		if (s->acpu_clk_khz == 0) {
			pr_err("Error - ACPU clock reports invalid speed\n");
			return;
		}
		break;
	case 2:	
		for (s = acpu_freq_tbl; s->acpu_clk_khz != 0
			&& s->src != PLL_2 && s->acpu_src_div == 0; s++)
			;
		if (s->acpu_clk_khz != 0) {
			
			acpuclk_set_src(s);

			
			reg_clksel = readl_relaxed(SCSS_CLK_SEL_ADDR);
			reg_clksel &= ~(0x3 << 1);
			writel_relaxed(reg_clksel, SCSS_CLK_SEL_ADDR);
			break;
		}
		
	default:
		pr_err("Error - ACPU clock reports invalid source\n");
		return;
	}

	
	if (s->src == PLL_2)
		for ( ; s->acpu_clk_khz; s++)
			if (s->pll_rate && s->pll_rate->l == pll2_l)
				break;

	
	acpuclk_set_acpu_vdd(s);

	drv_state.current_speed = s;

	
	if (s->src >= 0)
		clk_enable(acpuclk_sources[s->src]);

	res = clk_set_rate(drv_state.ebi1_clk, s->axi_clk_hz);
	if (res < 0)
		pr_warning("Setting AXI min rate failed!\n");

	pr_info("ACPU running at %d KHz\n", s->acpu_clk_khz);

	return;
}

static void __devinit lpj_init(void)
{
	int i;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;

	for (i = 0; acpu_freq_tbl[i].acpu_clk_khz; i++) {
		acpu_freq_tbl[i].lpj = cpufreq_scale(loops_per_jiffy,
						base_clk->acpu_clk_khz,
						acpu_freq_tbl[i].acpu_clk_khz);
	}
}

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table cpufreq_tbl[ARRAY_SIZE(acpu_freq_tbl)];

static void setup_cpufreq_table(void)
{
	unsigned i = 0;
	const struct clkctl_acpu_speed *speed;

	for (speed = acpu_freq_tbl; speed->acpu_clk_khz; speed++)
		if (speed->use_for_scaling) {
			cpufreq_tbl[i].index = i;
			cpufreq_tbl[i].frequency = speed->acpu_clk_khz;
			i++;
		}
	cpufreq_tbl[i].frequency = CPUFREQ_TABLE_END;

	cpufreq_frequency_table_get_attr(cpufreq_tbl, smp_processor_id());
}
#else
static inline void setup_cpufreq_table(void) { }
#endif

void __devinit pll2_fixup(void)
{
	struct clkctl_acpu_speed *speed = acpu_freq_tbl;
	u8 pll2_l = readl_relaxed(PLL2_L_VAL_ADDR) & 0xFF;

	for ( ; speed->acpu_clk_khz; speed++) {
		if (speed->src != PLL_2)
			backup_s = speed;
		if (speed->pll_rate && speed->pll_rate->l == pll2_l) {
			speed++;
			speed->acpu_clk_khz = 0;
			return;
		}
	}

	pr_err("Unknown PLL2 lval %d\n", pll2_l);
	BUG();
}

#define RPM_BYPASS_MASK	(1 << 3)
#define PMIC_MODE_MASK	(1 << 4)

static void __devinit populate_plls(void)
{
	acpuclk_sources[PLL_1] = clk_get_sys("acpu", "pll1_clk");
	BUG_ON(IS_ERR(acpuclk_sources[PLL_1]));
	acpuclk_sources[PLL_2] = clk_get_sys("acpu", "pll2_clk");
	BUG_ON(IS_ERR(acpuclk_sources[PLL_2]));
	acpuclk_sources[PLL_3] = clk_get_sys("acpu", "pll3_clk");
	BUG_ON(IS_ERR(acpuclk_sources[PLL_3]));
	BUG_ON(clk_prepare(acpuclk_sources[PLL_1]));
	BUG_ON(clk_prepare(acpuclk_sources[PLL_2]));
	BUG_ON(clk_prepare(acpuclk_sources[PLL_3]));
}

static struct acpuclk_data acpuclk_7x30_data = {
	.set_rate = acpuclk_7x30_set_rate,
	.get_rate = acpuclk_7x30_get_rate,
	.power_collapse_khz = MAX_AXI_KHZ,
	.wait_for_irq_khz = MAX_AXI_KHZ,
	.switch_time_us = 50,
};

static int __devinit acpuclk_7x30_probe(struct platform_device *pdev)
{
	pr_info("%s()\n", __func__);

	mutex_init(&drv_state.lock);
	pll2_fixup();
	populate_plls();
	acpuclk_hw_init();
	lpj_init();
	setup_cpufreq_table();
	acpuclk_register(&acpuclk_7x30_data);

	return 0;
}

static struct platform_driver acpuclk_7x30_driver = {
	.probe = acpuclk_7x30_probe,
	.driver = {
		.name = "acpuclk-7x30",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_7x30_init(void)
{
	return platform_driver_register(&acpuclk_7x30_driver);
}
postcore_initcall(acpuclk_7x30_init);
