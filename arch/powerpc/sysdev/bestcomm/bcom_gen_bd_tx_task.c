/*
 * Bestcomm GenBD TX task microcode
 *
 * Copyright (C) 2006 AppSpec Computer Technologies Corp.
 *                    Jeff Gibbons <jeff.gibbons@appspec.com>
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Based on BestCommAPI-2.2/code_dma/image_rtos1/dma_image.hex
 * on Tue Mar 4 10:14:12 2006 GMT
 *
 */

#include <asm/types.h>


u32 bcom_gen_bd_tx_task[] = {
	
	0x4243544b,
	0x0f040609,
	0x00000000,
	0x00000000,

	
	0x800220e3, 
	0x13e01010, 
	0xb8808264, 
	0x10001308, 
	0x60140002, 
	0x0cccfcca, 
	0xd9190300, 
	0xb8c5e009, 
	0x03fec398, 
	0x9919826a, 
	0x0feac398, 
	0x99190036, 
	0x60000005, 
	0x0c4cf889, 
	0x000001f8, 

	
	0x40000000,
	0x7fff7fff,
	0x00000000,
	0x40000004,

	
	0x40000000,
	0xe0000000,
	0xe0000000,
	0xa0000008,
	0x20000000,
	0x4000ffff,
};

