/*
 * Driver for the NVIDIA Tegra pinmux
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __PINMUX_TEGRA_H__
#define __PINMUX_TEGRA_H__

struct tegra_function {
	const char *name;
	const char * const *groups;
	unsigned ngroups;
};

struct tegra_pingroup {
	const char *name;
	const unsigned *pins;
	unsigned npins;
	unsigned funcs[4];
	unsigned func_safe;
	s16 mux_reg;
	s16 pupd_reg;
	s16 tri_reg;
	s16 einput_reg;
	s16 odrain_reg;
	s16 lock_reg;
	s16 ioreset_reg;
	s16 drv_reg;
	u32 mux_bank:2;
	u32 pupd_bank:2;
	u32 tri_bank:2;
	u32 einput_bank:2;
	u32 odrain_bank:2;
	u32 ioreset_bank:2;
	u32 lock_bank:2;
	u32 drv_bank:2;
	u32 mux_bit:5;
	u32 pupd_bit:5;
	u32 tri_bit:5;
	u32 einput_bit:5;
	u32 odrain_bit:5;
	u32 lock_bit:5;
	u32 ioreset_bit:5;
	u32 hsm_bit:5;
	u32 schmitt_bit:5;
	u32 lpmd_bit:5;
	u32 drvdn_bit:5;
	u32 drvup_bit:5;
	u32 slwr_bit:5;
	u32 slwf_bit:5;
	u32 drvdn_width:6;
	u32 drvup_width:6;
	u32 slwr_width:6;
	u32 slwf_width:6;
};

struct tegra_pinctrl_soc_data {
	unsigned ngpios;
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct tegra_function *functions;
	unsigned nfunctions;
	const struct tegra_pingroup *groups;
	unsigned ngroups;
};

typedef void (*tegra_pinctrl_soc_initf)(
			const struct tegra_pinctrl_soc_data **soc_data);

void tegra20_pinctrl_init(const struct tegra_pinctrl_soc_data **soc_data);
void tegra30_pinctrl_init(const struct tegra_pinctrl_soc_data **soc_data);

#endif
