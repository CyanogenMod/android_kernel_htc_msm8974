/*
 * arch/arm/plat-orion/include/plat/addr-map.h
 *
 * Marvell Orion SoC address map handling.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __PLAT_ADDR_MAP_H
#define __PLAT_ADDR_MAP_H

extern struct mbus_dram_target_info orion_mbus_dram_info;

struct orion_addr_map_cfg {
	const int num_wins;	
	const int remappable_wins;
	const u32 bridge_virt_base;

	int (*cpu_win_can_remap) (const struct orion_addr_map_cfg *cfg,
				  const int win);
	void __iomem *(*win_cfg_base) (const struct orion_addr_map_cfg *cfg,
				 const int win);
};

struct orion_addr_map_info {
	const int win;
	const u32 base;
	const u32 size;
	const u8 target;
	const u8 attr;
	const int remap;
};

void __init orion_config_wins(struct orion_addr_map_cfg *cfg,
			      const struct orion_addr_map_info *info);

void __init orion_setup_cpu_win(const struct orion_addr_map_cfg *cfg,
				const int win, const u32 base,
				const u32 size, const u8 target,
				const u8 attr, const int remap);

void __init orion_setup_cpu_mbus_target(const struct orion_addr_map_cfg *cfg,
					const u32 ddr_window_cpu_base);
#endif
