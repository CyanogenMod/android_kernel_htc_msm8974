/*
 * Copyright 2010 Benjamin Herrenschmidt, IBM Corp
 *                <benh@kernel.crashing.org>
 *     and        David Gibson, IBM Corporation.
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _ASM_POWERPC_SCOM_H
#define _ASM_POWERPC_SCOM_H

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#ifdef CONFIG_PPC_SCOM


typedef void *scom_map_t;

#define SCOM_MAP_INVALID	(NULL)

struct scom_controller {
	scom_map_t (*map)(struct device_node *ctrl_dev, u64 reg, u64 count);
	void (*unmap)(scom_map_t map);

	u64 (*read)(scom_map_t map, u32 reg);
	void (*write)(scom_map_t map, u32 reg, u64 value);
};

extern const struct scom_controller *scom_controller;

static inline void scom_init(const struct scom_controller *controller)
{
	scom_controller = controller;
}

static inline int scom_map_ok(scom_map_t map)
{
	return map != SCOM_MAP_INVALID;
}


static inline scom_map_t scom_map(struct device_node *ctrl_dev,
				  u64 reg, u64 count)
{
	return scom_controller->map(ctrl_dev, reg, count);
}

struct device_node *scom_find_parent(struct device_node *dev);


extern scom_map_t scom_map_device(struct device_node *dev, int index);


static inline void scom_unmap(scom_map_t map)
{
	if (scom_map_ok(map))
		scom_controller->unmap(map);
}

static inline u64 scom_read(scom_map_t map, u32 reg)
{
	return scom_controller->read(map, reg);
}

static inline void scom_write(scom_map_t map, u32 reg, u64 value)
{
	scom_controller->write(map, reg, value);
}

#endif 
#endif 
#endif 
#endif 
