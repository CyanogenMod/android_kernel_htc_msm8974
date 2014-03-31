/*
 * This file is part of wl12xx
 *
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "wl1251.h"
#include "reg.h"
#include "io.h"

static enum wl12xx_acx_int_reg wl1251_io_reg_table[ACX_REG_TABLE_LEN] = {
	[ACX_REG_INTERRUPT_TRIG]     = (REGISTERS_BASE + 0x0474),
	[ACX_REG_INTERRUPT_TRIG_H]   = (REGISTERS_BASE + 0x0478),
	[ACX_REG_INTERRUPT_MASK]     = (REGISTERS_BASE + 0x0494),
	[ACX_REG_HINT_MASK_SET]      = (REGISTERS_BASE + 0x0498),
	[ACX_REG_HINT_MASK_CLR]      = (REGISTERS_BASE + 0x049C),
	[ACX_REG_INTERRUPT_NO_CLEAR] = (REGISTERS_BASE + 0x04B0),
	[ACX_REG_INTERRUPT_CLEAR]    = (REGISTERS_BASE + 0x04A4),
	[ACX_REG_INTERRUPT_ACK]      = (REGISTERS_BASE + 0x04A8),
	[ACX_REG_SLV_SOFT_RESET]     = (REGISTERS_BASE + 0x0000),
	[ACX_REG_EE_START]           = (REGISTERS_BASE + 0x080C),
	[ACX_REG_ECPU_CONTROL]       = (REGISTERS_BASE + 0x0804)
};

static int wl1251_translate_reg_addr(struct wl1251 *wl, int addr)
{
	if (addr < REGISTERS_BASE) {
		
		if (addr >= ACX_REG_TABLE_LEN) {
			wl1251_error("address out of range (%d)", addr);
			return -EINVAL;
		}
		addr = wl1251_io_reg_table[addr];
	}

	return addr - wl->physical_reg_addr + wl->virtual_reg_addr;
}

static int wl1251_translate_mem_addr(struct wl1251 *wl, int addr)
{
	return addr - wl->physical_mem_addr + wl->virtual_mem_addr;
}

void wl1251_mem_read(struct wl1251 *wl, int addr, void *buf, size_t len)
{
	int physical;

	physical = wl1251_translate_mem_addr(wl, addr);

	wl->if_ops->read(wl, physical, buf, len);
}

void wl1251_mem_write(struct wl1251 *wl, int addr, void *buf, size_t len)
{
	int physical;

	physical = wl1251_translate_mem_addr(wl, addr);

	wl->if_ops->write(wl, physical, buf, len);
}

u32 wl1251_mem_read32(struct wl1251 *wl, int addr)
{
	return wl1251_read32(wl, wl1251_translate_mem_addr(wl, addr));
}

void wl1251_mem_write32(struct wl1251 *wl, int addr, u32 val)
{
	wl1251_write32(wl, wl1251_translate_mem_addr(wl, addr), val);
}

u32 wl1251_reg_read32(struct wl1251 *wl, int addr)
{
	return wl1251_read32(wl, wl1251_translate_reg_addr(wl, addr));
}

void wl1251_reg_write32(struct wl1251 *wl, int addr, u32 val)
{
	wl1251_write32(wl, wl1251_translate_reg_addr(wl, addr), val);
}

void wl1251_set_partition(struct wl1251 *wl,
			  u32 mem_start, u32 mem_size,
			  u32 reg_start, u32 reg_size)
{
	struct wl1251_partition partition[2];

	wl1251_debug(DEBUG_SPI, "mem_start %08X mem_size %08X",
		     mem_start, mem_size);
	wl1251_debug(DEBUG_SPI, "reg_start %08X reg_size %08X",
		     reg_start, reg_size);

	if ((mem_size + reg_size) > HW_ACCESS_MEMORY_MAX_RANGE) {
		wl1251_debug(DEBUG_SPI, "Total size exceeds maximum virtual"
			     " address range.  Truncating partition[0].");
		mem_size = HW_ACCESS_MEMORY_MAX_RANGE - reg_size;
		wl1251_debug(DEBUG_SPI, "mem_start %08X mem_size %08X",
			     mem_start, mem_size);
		wl1251_debug(DEBUG_SPI, "reg_start %08X reg_size %08X",
			     reg_start, reg_size);
	}

	if ((mem_start < reg_start) &&
	    ((mem_start + mem_size) > reg_start)) {
		wl1251_debug(DEBUG_SPI, "End of partition[0] is "
			     "overlapping partition[1].  Adjusted.");
		mem_size = reg_start - mem_start;
		wl1251_debug(DEBUG_SPI, "mem_start %08X mem_size %08X",
			     mem_start, mem_size);
		wl1251_debug(DEBUG_SPI, "reg_start %08X reg_size %08X",
			     reg_start, reg_size);
	} else if ((reg_start < mem_start) &&
		   ((reg_start + reg_size) > mem_start)) {
		wl1251_debug(DEBUG_SPI, "End of partition[1] is"
			     " overlapping partition[0].  Adjusted.");
		reg_size = mem_start - reg_start;
		wl1251_debug(DEBUG_SPI, "mem_start %08X mem_size %08X",
			     mem_start, mem_size);
		wl1251_debug(DEBUG_SPI, "reg_start %08X reg_size %08X",
			     reg_start, reg_size);
	}

	partition[0].start = mem_start;
	partition[0].size  = mem_size;
	partition[1].start = reg_start;
	partition[1].size  = reg_size;

	wl->physical_mem_addr = mem_start;
	wl->physical_reg_addr = reg_start;

	wl->virtual_mem_addr = 0;
	wl->virtual_reg_addr = mem_size;

	wl->if_ops->write(wl, HW_ACCESS_PART0_SIZE_ADDR, partition,
		sizeof(partition));
}
