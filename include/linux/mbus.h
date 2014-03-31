/*
 * Marvell MBUS common definitions.
 *
 * Copyright (C) 2008 Marvell Semiconductor
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __LINUX_MBUS_H
#define __LINUX_MBUS_H

struct mbus_dram_target_info
{
	u8		mbus_dram_target_id;

	int		num_cs;
	struct mbus_dram_window {
		u8	cs_index;
		u8	mbus_attr;
		u32	base;
		u32	size;
	} cs[4];
};

#ifdef CONFIG_PLAT_ORION
extern const struct mbus_dram_target_info *mv_mbus_dram_info(void);
#else
static inline const struct mbus_dram_target_info *mv_mbus_dram_info(void)
{
	return NULL;
}
#endif
#endif
