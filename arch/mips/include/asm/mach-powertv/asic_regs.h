/*
 * Copyright (C) 2009  Cisco Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __ASM_MACH_POWERTV_ASIC_H_
#define __ASM_MACH_POWERTV_ASIC_H_
#include <linux/io.h>

enum asic_type {
	ASIC_UNKNOWN,
	ASIC_ZEUS,
	ASIC_CALLIOPE,
	ASIC_CRONUS,
	ASIC_CRONUSLITE,
	ASIC_GAIA,
	ASICS			
};

#define CRONUS_10	0x0B4C1C20
#define CRONUS_11	0x0B4C1C21
#define CRONUSLITE_10	0x0B4C1C40

#define NAND_FLASH_BASE		0x03000000
#define CALLIOPE_IO_BASE	0x08000000
#define GAIA_IO_BASE		0x09000000
#define CRONUS_IO_BASE		0x09000000
#define ZEUS_IO_BASE		0x09000000

#define ASIC_IO_SIZE		0x01000000

#define UART1_INTSTAT	uart1_intstat
#define UART1_INTEN	uart1_inten
#define UART1_CONFIG1	uart1_config1
#define UART1_CONFIG2	uart1_config2
#define UART1_DIVISORHI	uart1_divisorhi
#define UART1_DIVISORLO	uart1_divisorlo
#define UART1_DATA	uart1_data
#define UART1_STATUS	uart1_status

union register_map_entry {
	unsigned long phys;
	u32 *virt;
};

#define REGISTER_MAP_ELEMENT(x) union register_map_entry x;
struct register_map {
#include <asm/mach-powertv/asic_reg_map.h>
};
#undef REGISTER_MAP_ELEMENT

static inline void register_map_offset_phys(struct register_map *map,
	unsigned long offset)
{
#define REGISTER_MAP_ELEMENT(x)		do {				\
		if (map->x.phys != 0)					\
			map->x.phys += offset;				\
	} while (false);

#include <asm/mach-powertv/asic_reg_map.h>
#undef REGISTER_MAP_ELEMENT
}

static inline void register_map_virtualize(struct register_map *map)
{
#define REGISTER_MAP_ELEMENT(x)		do {				\
		map->x.virt = (!map->x.phys) ? NULL :			\
			UNCAC_ADDR(phys_to_virt(map->x.phys));		\
	} while (false);

#include <asm/mach-powertv/asic_reg_map.h>
#undef REGISTER_MAP_ELEMENT
}

extern struct register_map _asic_register_map;
extern unsigned long asic_phy_base;

#define asic_reg_addr(x)	(_asic_register_map.x.virt)
#define asic_reg_phys_addr(x)	(virt_to_phys((void *) CAC_ADDR(	\
					(unsigned long) asic_reg_addr(x))))


#define asic_read(x)		readl(asic_reg_addr(x))
#define asic_write(v, x)	writel(v, asic_reg_addr(x))

extern void asic_irq_init(void);
#endif
