/*
 * Bestcomm FEC RX task microcode
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Automatically created based on BestCommAPI-2.2/code_dma/image_rtos1/dma_image.hex
 * on Tue Mar 22 11:19:38 2005 GMT
 */

#include <asm/types.h>


u32 bcom_fec_rx_task[] = {
	
	0x4243544b,
	0x18060709,
	0x00000000,
	0x00000000,

	
	0x808220e3, 
	0x10601010, 
	0xb8800264, 
	0x10001308, 
	0x60140002, 
	0x0cccfcca, 
	0x80004000, 
	0xb8c58029, 
	0x60000002, 
	0x088cf8cc, 
	0x991982f2, 
	0x006acf80, 
	0x80004000, 
	0x9999802d, 
	0x70000002, 
	0x034cfc4e, 
	0x00008868, 
	0x99198341, 
	0x007ecf80, 
	0x99198272, 
	0x046acf80, 
	0x9819002d, 
	0x0060c790, 
	0x000001f8, 

	
	0x40000000,
	0x7fff7fff,
	0x00000000,
	0x00000003,
	0x40000008,
	0x43ffffff,

	
	0x40000000,
	0xe0000000,
	0xe0000000,
	0xa0000008,
	0x20000000,
	0x00000000,
	0x4000ffff,
};

