/*
 * Bestcomm ATA task microcode
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Created based on bestcom/code_dma/image_rtos1/dma_image.hex
 */

#include <asm/types.h>


u32 bcom_ata_task[] = {
	
	0x4243544b,
	0x0e060709,
	0x00000000,
	0x00000000,

	
	0x8198009b, 
	0x13e00c08, 
	0xb8000264, 
	0x10000f00, 
	0x60140002, 
	0x0c8cfc8a, 
	0xd8988240, 
	0xf845e011, 
	0xb845e00a, 
	0x0bfecf90, 
	0x9898802d, 
	0x64000005, 
	0x0c0cf849, 
	0x000001f8, 

	
	0x40000000,
	0x7fff7fff,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,

	
	0x40000000,
	0xe0000000,
	0xe0000000,
	0xa000000c,
	0x20000000,
	0x00000000,
	0x00000000,
};

