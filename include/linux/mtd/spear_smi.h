/*
 * Copyright © 2010 ST Microelectronics
 * Shiraz Hashim <shiraz.hashim@st.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __MTD_SPEAR_SMI_H
#define __MTD_SPEAR_SMI_H

#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define MAX_NUM_FLASH_CHIP	4

#define DEFINE_PARTS(n, of, s)		\
{					\
	.name = n,			\
	.offset = of,			\
	.size = s,			\
}


struct spear_smi_flash_info {
	char *name;
	unsigned long mem_base;
	unsigned long size;
	struct mtd_partition *partitions;
	int nr_partitions;
	u8 fast_mode;
};

struct spear_smi_plat_data {
	unsigned long clk_rate;
	int num_flashes;
	struct spear_smi_flash_info *board_flash_info;
	struct device_node *np[MAX_NUM_FLASH_CHIP];
};

#endif 
