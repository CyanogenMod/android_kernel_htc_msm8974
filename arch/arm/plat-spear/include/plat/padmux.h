/*
 * arch/arm/plat-spear/include/plat/padmux.h
 *
 * SPEAr platform specific gpio pads muxing file
 *
 * Copyright (C) 2009 ST Microelectronics
 * Viresh Kumar<viresh.kumar@st.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __PLAT_PADMUX_H
#define __PLAT_PADMUX_H

#include <linux/types.h>

struct pmx_reg {
	u32 offset;
	u32 mask;
};

struct pmx_dev_mode {
	u32 ids;
	u32 mask;
};

struct pmx_mode {
	char *name;
	u32 id;
	u32 mask;
};

struct pmx_dev {
	char *name;
	struct pmx_dev_mode *modes;
	u8 mode_count;
	bool is_active;
	bool enb_on_reset;
};

struct pmx_driver {
	struct pmx_mode *mode;
	struct pmx_dev **devs;
	u8 devs_count;
	u32 *base;
	struct pmx_reg mode_reg;
	struct pmx_reg mux_reg;
};

int pmx_register(struct pmx_driver *driver);

#endif 
