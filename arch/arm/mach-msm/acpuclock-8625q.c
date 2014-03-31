/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sort.h>
#include <linux/regulator/consumer.h>
#include <linux/smp.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/clk-provider.h>

#include <asm/cpu.h>

#include "acpuclock.h"
#include "acpuclock-8625q.h"

#define A11S_CLK_CNTL_ADDR	(MSM_CSR_BASE + 0x100)
#define A11S_CLK_SEL_ADDR	(MSM_CSR_BASE + 0x104)

#define PLL4_L_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x378)
#define PLL4_M_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x37C)
#define PLL4_N_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x380)

#define POWER_COLLAPSE_KHZ 19200

#define MAX_WAIT_FOR_IRQ_KHZ 128000

enum {
	ACPU_PLL_0	= 0,
	ACPU_PLL_1,
	ACPU_PLL_2,
	ACPU_PLL_3,
	ACPU_PLL_4,
	ACPU_PLL_TCXO,
	ACPU_PLL_END,
};

struct acpu_clk_src {
	struct clk *clk;
	const char *name;
};

struct pll_config {
	unsigned int l;
	unsigned int m;
	unsigned int n;
};

static struct acpu_clk_src pll_clk[ACPU_PLL_END] = {
	[ACPU_PLL_0] = { .name = "pll0_clk" },
	[ACPU_PLL_1] = { .name = "pll1_clk" },
	[ACPU_PLL_2] = { .name = "pll2_clk" },
	[ACPU_PLL_4] = { .name = "pll4_clk" },
};

static struct pll_config pll4_cfg_tbl[] = {
	[0] = {  36, 1, 2 }, 
	[1] = {  52, 1, 2 }, 
	[2] = {  63, 0, 1 }, 
	[3] = {  73, 0, 1 }, 
};

struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	uint32_t			max_speed_delta_khz;
	struct clk			*ebi1_clk;
	struct regulator		*vreg_cpu;
};

struct clkctl_acpu_speed {
	unsigned int	use_for_scaling;
	unsigned int	a11clk_khz;
	int		pll;
	unsigned int	a11clk_src_sel;
	unsigned int	a11clk_src_div;
	unsigned int	ahbclk_khz;
	unsigned int	ahbclk_div;
	int		vdd;
	unsigned int	axiclk_khz;
	struct pll_config *pll_rate;
	unsigned long   lpj;
};

static struct clock_state drv_state = { 0 };


# define MAX_14GHZ_VOLTAGE 1350000
# define MAX_12GHZ_VOLTAGE 1275000
# define MAX_1GHZ_VOLTAGE 1175000
# define MAX_NOMINAL_VOLTAGE 1150000

# define DELTA_LEVEL_1_UV 0
# define DELTA_LEVEL_2_UV 75000
# define DELTA_LEVEL_3_UV 150000

static struct clkctl_acpu_speed acpu_freq_tbl_cmn[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 2400, 3, 0, 30720 },
	{ 1, 245760, ACPU_PLL_1, 1, 0, 30720, 3, MAX_NOMINAL_VOLTAGE, 61440 },
	{ 1, 320000, ACPU_PLL_0, 4, 2, 40000, 3, MAX_NOMINAL_VOLTAGE, 122880 },
	{ 1, 480000, ACPU_PLL_0, 4, 1, 60000, 3, MAX_NOMINAL_VOLTAGE, 122880 },
	{ 0, 600000, ACPU_PLL_2, 2, 1, 75000, 3, 0, 160000 },
	{ 1, 700800, ACPU_PLL_4, 6, 0, 87500, 3, MAX_NOMINAL_VOLTAGE, 160000,
						&pll4_cfg_tbl[0]},
	{ 1, 1008000, ACPU_PLL_4, 6, 0, 126000, 3, MAX_1GHZ_VOLTAGE, 200000,
						&pll4_cfg_tbl[1]},
};

static struct clkctl_acpu_speed acpu_freq_tbl_1209[] = {
	{ 1, 1209600, ACPU_PLL_4, 6, 0, 151200, 3, MAX_12GHZ_VOLTAGE, 200000,
						&pll4_cfg_tbl[2]},
};

static struct clkctl_acpu_speed acpu_freq_tbl_1401[] = {
	{ 1, 1401600, ACPU_PLL_4, 6, 0, 175000, 3, MAX_14GHZ_VOLTAGE, 200000,
						&pll4_cfg_tbl[3]},
};

static struct clkctl_acpu_speed acpu_freq_tbl_196608[] = {
	{ 1, 196608, ACPU_PLL_1, 1, 0,  65536, 2, MAX_NOMINAL_VOLTAGE, 98304 },
};

static struct clkctl_acpu_speed acpu_freq_tbl_null[] = {
	{ 0 },
};

static struct clkctl_acpu_speed acpu_freq_tbl[ARRAY_SIZE(acpu_freq_tbl_cmn)
	+ ARRAY_SIZE(acpu_freq_tbl_1209)
	+ ARRAY_SIZE(acpu_freq_tbl_1401)
	+ ARRAY_SIZE(acpu_freq_tbl_null)];

static struct clkctl_acpu_speed *backup_s;

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[NR_CPUS][20];

static void __devinit cpufreq_table_init(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		unsigned int i, freq_cnt = 0;

		for (i = 0; acpu_freq_tbl[i].a11clk_khz != 0
				&& freq_cnt < ARRAY_SIZE(*freq_table)-1; i++) {
			if (acpu_freq_tbl[i].use_for_scaling) {
				freq_table[cpu][freq_cnt].index = freq_cnt;
				freq_table[cpu][freq_cnt].frequency
					= acpu_freq_tbl[i].a11clk_khz;
				freq_cnt++;
			}
		}

		
		BUG_ON(acpu_freq_tbl[i].a11clk_khz != 0);

		freq_table[cpu][freq_cnt].index = freq_cnt;
		freq_table[cpu][freq_cnt].frequency = CPUFREQ_TABLE_END;
		
		cpufreq_frequency_table_get_attr(freq_table[cpu], cpu);
		pr_info("CPU%d: %d scaling frequencies supported.\n",
			cpu, freq_cnt);
	}
}
#else
static void __devinit cpufreq_table_init(void) { }
#endif

static void update_jiffies(int cpu, unsigned long loops)
{
#ifdef CONFIG_SMP
	for_each_possible_cpu(cpu) {
		per_cpu(cpu_data, cpu).loops_per_jiffy =
						loops;
	}
#endif
	
	loops_per_jiffy = loops;
}

static void acpuclk_config_pll4(struct pll_config *pll)
{
	mb();
	writel_relaxed(pll->l, PLL4_L_VAL_ADDR);
	writel_relaxed(pll->m, PLL4_M_VAL_ADDR);
	writel_relaxed(pll->n, PLL4_N_VAL_ADDR);
	
	mb();
}

static void acpuclk_set_div(const struct clkctl_acpu_speed *hunt_s)
{
	uint32_t reg_clkctl, reg_clksel, clk_div, src_sel;

	reg_clksel = readl_relaxed(A11S_CLK_SEL_ADDR);

	
	clk_div = (reg_clksel >> 1) & 0x03;
	
	src_sel = reg_clksel & 1;

	if (hunt_s->ahbclk_div > clk_div) {
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);
	}

	
	reg_clkctl = readl_relaxed(A11S_CLK_CNTL_ADDR);
	reg_clkctl &= ~(0xFF << (8 * src_sel));
	reg_clkctl |= hunt_s->a11clk_src_sel << (4 + 8 * src_sel);
	reg_clkctl |= hunt_s->a11clk_src_div << (0 + 8 * src_sel);
	writel_relaxed(reg_clkctl, A11S_CLK_CNTL_ADDR);

	
	reg_clksel ^= 1;
	writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);

	
	mb();
	udelay(50);

	if (hunt_s->ahbclk_div < clk_div) {
		reg_clksel &= ~(0x3 << 1);
		reg_clksel |= (hunt_s->ahbclk_div << 1);
		writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);
	}
}

static int acpuclk_set_vdd_level(int vdd)
{
	int rc;

	rc = regulator_set_voltage(drv_state.vreg_cpu, vdd, vdd);
	if (rc) {
		pr_err("failed to set vdd=%d uV\n", vdd);
		return rc;
	}

	return 0;
}

static int acpuclk_8625q_set_rate(int cpu, unsigned long rate,
					enum setrate_reason reason)
{
	uint32_t reg_clkctl;
	struct clkctl_acpu_speed *cur_s, *tgt_s, *strt_s;
	int res, rc = 0;
	unsigned int plls_enabled = 0, pll;
	int delta;


	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = cur_s = drv_state.current_speed;

	WARN_ONCE(cur_s == NULL, "%s: not initialized\n", __func__);
	if (cur_s == NULL) {
		rc = -ENOENT;
		goto out;
	}

	cur_s->vdd = regulator_get_voltage(drv_state.vreg_cpu);
	if (cur_s->vdd <= 0)
		goto out;

	pr_debug("current freq=%dKhz vdd=%duV\n",
			cur_s->a11clk_khz, cur_s->vdd);

	if (rate == cur_s->a11clk_khz)
		goto out;

	for (tgt_s = acpu_freq_tbl; tgt_s->a11clk_khz != 0; tgt_s++) {
		if (tgt_s->a11clk_khz == rate)
			break;
	}

	if (tgt_s->a11clk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	
	if (reason != SETRATE_CPUFREQ
		&& tgt_s->a11clk_khz < cur_s->a11clk_khz) {
		while (tgt_s->pll != ACPU_PLL_TCXO &&
					tgt_s->pll != cur_s->pll) {
			pr_debug("Intermediate frequency changes: %u\n",
					tgt_s->a11clk_khz);
			tgt_s--;
		}
	}

	if (strt_s->pll != ACPU_PLL_TCXO)
		plls_enabled |= 1 << strt_s->pll;

	if (reason == SETRATE_CPUFREQ || reason == SETRATE_PC) {
		
		if (tgt_s->vdd > cur_s->vdd) {
			rc = acpuclk_set_vdd_level(tgt_s->vdd);
			if (rc < 0) {
				pr_err("Unable to switch ACPU vdd (%d)\n", rc);
				goto out;
			}
			pr_debug("Increased Vdd to %duV\n", tgt_s->vdd);
		}
	}

	
	reg_clkctl = readl_relaxed(A11S_CLK_CNTL_ADDR);
	reg_clkctl |= (100 << 16); 
	writel_relaxed(reg_clkctl, A11S_CLK_CNTL_ADDR);

	pr_debug("Switching from ACPU rate %u KHz -> %u KHz\n",
			strt_s->a11clk_khz, tgt_s->a11clk_khz);

	delta = abs((int)(strt_s->a11clk_khz - tgt_s->a11clk_khz));

	if (tgt_s->pll == ACPU_PLL_4) {
		if (strt_s->pll == ACPU_PLL_4 ||
				delta > drv_state.max_speed_delta_khz) {
			clk_enable(pll_clk[backup_s->pll].clk);
			acpuclk_set_div(backup_s);
			update_jiffies(cpu, backup_s->lpj);
		}
		
		if ((plls_enabled & (1 << tgt_s->pll))) {
			clk_disable(pll_clk[tgt_s->pll].clk);
			plls_enabled &= ~(1 << tgt_s->pll);
		}
		acpuclk_config_pll4(tgt_s->pll_rate);
		pll_clk[tgt_s->pll].clk->rate = tgt_s->a11clk_khz*1000;

	} else if (strt_s->pll == ACPU_PLL_4) {
		if (delta > drv_state.max_speed_delta_khz) {
			clk_enable(pll_clk[backup_s->pll].clk);
			acpuclk_set_div(backup_s);
			update_jiffies(cpu, backup_s->lpj);
		}
	}

	if ((tgt_s->pll != ACPU_PLL_TCXO) &&
			!(plls_enabled & (1 << tgt_s->pll))) {
		rc = clk_enable(pll_clk[tgt_s->pll].clk);
		if (rc < 0) {
			pr_err("PLL%d enable failed (%d)\n",
					tgt_s->pll, rc);
			goto out;
		}
		plls_enabled |= 1 << tgt_s->pll;
	}
	acpuclk_set_div(tgt_s);
	drv_state.current_speed = tgt_s;
	pr_debug("The new clock speed is %u\n", tgt_s->a11clk_khz);
	
	update_jiffies(cpu, tgt_s->lpj);

	
	if ((delta > drv_state.max_speed_delta_khz)
			|| (strt_s->pll == ACPU_PLL_4 &&
				tgt_s->pll == ACPU_PLL_4))
		clk_disable(pll_clk[backup_s->pll].clk);

	
	if (reason == SETRATE_SWFI)
		goto out;

	
	if (reason != SETRATE_PC &&
		strt_s->axiclk_khz != tgt_s->axiclk_khz) {
		res = clk_set_rate(drv_state.ebi1_clk,
				tgt_s->axiclk_khz * 1000);
		pr_debug("AXI bus set freq %d\n",
				tgt_s->axiclk_khz * 1000);
		if (res < 0)
			pr_warning("Setting AXI min rate failed (%d)\n", res);
	}

	
	if (tgt_s->pll != ACPU_PLL_TCXO)
		plls_enabled &= ~(1 << tgt_s->pll);
	for (pll = ACPU_PLL_0; pll < ACPU_PLL_END; pll++)
		if (plls_enabled & (1 << pll))
			clk_disable(pll_clk[pll].clk);

	
	if (reason == SETRATE_PC)
		goto out;

	
	if (tgt_s->vdd < strt_s->vdd) {
		res = acpuclk_set_vdd_level(tgt_s->vdd);
		if (res < 0)
			pr_warning("Unable to drop ACPU vdd (%d)\n", res);
		pr_debug("Decreased Vdd to %duV\n", tgt_s->vdd);
	}

	pr_debug("ACPU speed change complete\n");
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);

	return rc;
}

static int __devinit acpuclk_hw_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel, reg_clksel;
	int res;

	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_0].clk));
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_1].clk));
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_2].clk));
	BUG_ON(clk_prepare(pll_clk[ACPU_PLL_4].clk));
	BUG_ON(clk_prepare(drv_state.ebi1_clk));


	if (!(readl_relaxed(A11S_CLK_SEL_ADDR) & 0x01)) { 
		
		sel = (readl_relaxed(A11S_CLK_CNTL_ADDR) >> 12) & 0x7;
		
		div = (readl_relaxed(A11S_CLK_CNTL_ADDR) >> 8) & 0x0f;
	} else {
		
		sel = (readl_relaxed(A11S_CLK_CNTL_ADDR) >> 4) & 0x07;
		
		div = readl_relaxed(A11S_CLK_CNTL_ADDR) & 0x0f;
	}

	for (speed = acpu_freq_tbl; speed->a11clk_khz != 0; speed++) {
		if (speed->a11clk_src_sel == sel
			&& (speed->a11clk_src_div == div))
			break;
	}
	if (speed->a11clk_khz == 0) {
		pr_err("Error - ACPU clock reports invalid speed\n");
		return -EINVAL;
	}

	drv_state.current_speed = speed;
	if (speed->pll != ACPU_PLL_TCXO) {
		if (clk_enable(pll_clk[speed->pll].clk)) {
			pr_warning("Failed to vote for boot PLL\n");
			return -ENODEV;
		}
	}

	reg_clksel = readl_relaxed(A11S_CLK_SEL_ADDR);
	reg_clksel &= ~(0x3 << 14);
	reg_clksel |= (0x1 << 14);
	writel_relaxed(reg_clksel, A11S_CLK_SEL_ADDR);

	res = clk_set_rate(drv_state.ebi1_clk, speed->axiclk_khz * 1000);
	if (res < 0) {
		pr_warning("Setting AXI min rate failed (%d)\n", res);
		return -ENODEV;
	}
	res = clk_enable(drv_state.ebi1_clk);
	if (res < 0) {
		pr_warning("Enabling AXI clock failed (%d)\n", res);
		return -ENODEV;
	}

	drv_state.vreg_cpu = regulator_get(NULL, "vddx_cx");
	if (IS_ERR(drv_state.vreg_cpu)) {
		res = PTR_ERR(drv_state.vreg_cpu);
		pr_err("could not get regulator: %d\n", res);
	}

	pr_info("ACPU running at %d KHz\n", speed->a11clk_khz);
	return 0;
}

static unsigned long acpuclk_8625q_get_rate(int cpu)
{
	WARN_ONCE(drv_state.current_speed == NULL,
			"%s: not initialized\n", __func__);
	if (drv_state.current_speed)
		return drv_state.current_speed->a11clk_khz;
	else
		return 0;
}

static int reinitialize_freq_table(bool target_select)
{
	if (target_select) {
		struct clkctl_acpu_speed *tbl;
		for (tbl = acpu_freq_tbl; tbl->a11clk_khz; tbl++) {

			if (tbl->a11clk_khz >= 1008000) {
				tbl->axiclk_khz = 300000;
				if (tbl->a11clk_khz == 1209600)
					tbl->vdd = 0;
			} else {
				if (tbl->a11clk_khz != 600000
					&& tbl->a11clk_khz != 19200)
					tbl->vdd = 1050000;
				if (tbl->a11clk_khz == 700800)
					tbl->axiclk_khz = 245000;
			}
		}

	}
	return 0;
}

#define MHZ 1000000

static void __devinit select_freq_plan(unsigned int pvs_voltage,
							bool target_sel)
{
	unsigned long pll_mhz[ACPU_PLL_END];
	int i;
	int size;
	int delta[3] = {DELTA_LEVEL_1_UV, DELTA_LEVEL_2_UV, DELTA_LEVEL_3_UV};
	struct clkctl_acpu_speed *tbl;

	
	for (i = 0; i < ACPU_PLL_END; i++) {
		if (pll_clk[i].name) {
			pll_clk[i].clk = clk_get_sys("acpu", pll_clk[i].name);
			if (IS_ERR(pll_clk[i].clk)) {
				pll_mhz[i] = 0;
				continue;
			}
			
			pll_mhz[i] = clk_get_rate(pll_clk[i].clk)/MHZ;
		}
	}

	memcpy(acpu_freq_tbl, acpu_freq_tbl_cmn, sizeof(acpu_freq_tbl_cmn));
	size = ARRAY_SIZE(acpu_freq_tbl_cmn);

	i = 0;		
	
	if (pll_mhz[ACPU_PLL_4] == 1209) {
		memcpy(acpu_freq_tbl + size, acpu_freq_tbl_1209,
						sizeof(acpu_freq_tbl_1209));
		size += sizeof(acpu_freq_tbl_1209);
		i = 1;		
	}
	
	if (pll_mhz[ACPU_PLL_4] == 1401) {
		memcpy(acpu_freq_tbl + size, acpu_freq_tbl_1209,
						sizeof(acpu_freq_tbl_1209));
		size += ARRAY_SIZE(acpu_freq_tbl_1209);
		memcpy(acpu_freq_tbl + size, acpu_freq_tbl_1401,
						sizeof(acpu_freq_tbl_1401));
		size += ARRAY_SIZE(acpu_freq_tbl_1401);
		i = 2;		
	}

	memcpy(acpu_freq_tbl + size, acpu_freq_tbl_null,
						sizeof(acpu_freq_tbl_null));
	size += sizeof(acpu_freq_tbl_null);

	
	if (pll_mhz[ACPU_PLL_1] == 196) {

		for (tbl = acpu_freq_tbl; tbl->a11clk_khz; tbl++) {
			if (tbl->a11clk_khz == 245760 &&
					tbl->pll == ACPU_PLL_1) {
				pr_debug("Upgrading pll1 freq to 196  Mhz\n");
				memcpy(tbl, acpu_freq_tbl_196608,
						sizeof(acpu_freq_tbl_196608));
				break;
			}
		}
	}

	reinitialize_freq_table(target_sel);

	for (tbl = acpu_freq_tbl; tbl->a11clk_khz; tbl++) {
		if (tbl->a11clk_khz >= 1008000) {

			tbl->vdd = max((int)(pvs_voltage - delta[i]), tbl->vdd);
			i--;
		}
	}


	
	for (tbl = acpu_freq_tbl; tbl->a11clk_khz; tbl++) {
		if (tbl->pll == ACPU_PLL_2 &&
				tbl->a11clk_src_div == 1) {
			backup_s = tbl;
			break;
		}
	}

	BUG_ON(!backup_s);

}

static unsigned long __devinit find_wait_for_irq_khz(void)
{
	unsigned long found_khz = 0;
	int i;

	for (i = 0; acpu_freq_tbl[i].a11clk_khz &&
		acpu_freq_tbl[i].a11clk_khz <= MAX_WAIT_FOR_IRQ_KHZ; i++)
		found_khz = acpu_freq_tbl[i].a11clk_khz;

	return found_khz;
}

static void __devinit lpj_init(void)
{
	int i = 0, cpu;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;
	unsigned long loops;

	for_each_possible_cpu(cpu) {
#ifdef CONFIG_SMP
		loops = per_cpu(cpu_data, cpu).loops_per_jiffy;
#else
		loops = loops_per_jiffy;
#endif
		for (i = 0; acpu_freq_tbl[i].a11clk_khz; i++) {
			acpu_freq_tbl[i].lpj = cpufreq_scale(
				loops,
				base_clk->a11clk_khz,
				acpu_freq_tbl[i].a11clk_khz);
		}

	}

}

static struct acpuclk_data acpuclk_8625q_data = {
	.set_rate = acpuclk_8625q_set_rate,
	.get_rate = acpuclk_8625q_get_rate,
	.power_collapse_khz = POWER_COLLAPSE_KHZ,
	.switch_time_us = 50,
};

static void __devinit print_acpu_freq_tbl(void)
{
	struct clkctl_acpu_speed *t;
	int i;

	pr_info("Id CPU-KHz PLL DIV AHB-KHz ADIV AXI-KHz Vdd\n");

	t = &acpu_freq_tbl[0];
	for (i = 0; t->a11clk_khz != 0; i++) {
		pr_info("%2d %7d %3d %3d %7d %4d %7d %3d\n",
			i, t->a11clk_khz, t->pll, t->a11clk_src_div + 1,
			t->ahbclk_khz, t->ahbclk_div + 1, t->axiclk_khz,
			t->vdd);
		t++;
	}
}

static int __devinit acpuclk_8625q_probe(struct platform_device *pdev)
{
	const struct acpuclk_pdata_8625q *pdata = pdev->dev.platform_data;
	unsigned int pvs_voltage = pdata->pvs_voltage_uv;
	bool target_sel = pdata->flag;

	drv_state.max_speed_delta_khz = pdata->acpu_clk_data->
						max_speed_delta_khz;

	drv_state.ebi1_clk = clk_get(NULL, "ebi1_acpu_clk");
	BUG_ON(IS_ERR(drv_state.ebi1_clk));

	mutex_init(&drv_state.lock);
	select_freq_plan(pvs_voltage, target_sel);
	acpuclk_8625q_data.wait_for_irq_khz = find_wait_for_irq_khz();

	if (acpuclk_hw_init() < 0)
		pr_err("acpuclk_hw_init not successful.\n");

	print_acpu_freq_tbl();
	lpj_init();
	acpuclk_register(&acpuclk_8625q_data);

	cpufreq_table_init();

	return 0;
}

static struct platform_driver acpuclk_8625q_driver = {
	.probe = acpuclk_8625q_probe,
	.driver = {
		.name = "acpuclock-8625q",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_8625q_init(void)
{

	return platform_driver_register(&acpuclk_8625q_driver);
}
postcore_initcall(acpuclk_8625q_init);
