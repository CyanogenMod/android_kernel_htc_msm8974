#ifndef __CAN_PLATFORM_TI_HECC_H__
#define __CAN_PLATFORM_TI_HECC_H__

/*
 * TI HECC (High End CAN Controller) driver platform header
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed as is WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

struct ti_hecc_platform_data {
	u32 scc_hecc_offset;
	u32 scc_ram_offset;
	u32 hecc_ram_offset;
	u32 mbx_offset;
	u32 int_line;
	u32 version;
	void (*transceiver_switch) (int);
};
#endif
