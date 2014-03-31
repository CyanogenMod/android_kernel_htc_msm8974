/* arch/arm/plat-samsung/include/plat/regs-spi.h
 *
 * Copyright (c) 2004 Fetron GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2410 SPI register definition
*/

#ifndef __ASM_ARCH_REGS_SPI_H
#define __ASM_ARCH_REGS_SPI_H

#define S3C2410_SPI1		(0x20)
#define S3C2412_SPI1		(0x100)

#define S3C2410_SPCON		(0x00)

#define S3C2410_SPCON_SMOD_DMA	(2 << 5)	
#define S3C2410_SPCON_SMOD_INT	(1 << 5)	
#define S3C2410_SPCON_SMOD_POLL	(0 << 5)	
#define S3C2410_SPCON_ENSCK	(1 << 4)	
#define S3C2410_SPCON_MSTR	(1 << 3)	
#define S3C2410_SPCON_CPOL_HIGH	(1 << 2)	
#define S3C2410_SPCON_CPOL_LOW	(0 << 2)	

#define S3C2410_SPCON_CPHA_FMTB	(1 << 1)	
#define S3C2410_SPCON_CPHA_FMTA	(0 << 1)	

#define S3C2410_SPSTA		(0x04)

#define S3C2410_SPSTA_DCOL	(1 << 2)	
#define S3C2410_SPSTA_MULD	(1 << 1)	
#define S3C2410_SPSTA_READY	(1 << 0)	
#define S3C2412_SPSTA_READY_ORG	(1 << 3)

#define S3C2410_SPPIN		(0x08)

#define S3C2410_SPPIN_ENMUL	(1 << 2)	
#define S3C2410_SPPIN_RESERVED	(1 << 1)
#define S3C2410_SPPIN_KEEP	(1 << 0)	

#define S3C2410_SPPRE		(0x0C)
#define S3C2410_SPTDAT		(0x10)
#define S3C2410_SPRDAT		(0x14)

#endif 
