/*
 * Copyright 2009-2010 Freescale Semiconductor, Inc
 *
 * QorIQ based Cache Controller Memory Mapped Registers
 *
 * Author: Vivek Mahajan <vivek.mahajan@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __FSL_85XX_CACHE_CTLR_H__
#define __FSL_85XX_CACHE_CTLR_H__

#define L2CR_L2FI		0x40000000	
#define L2CR_L2IO		0x00200000	
#define L2CR_SRAM_ZERO		0x00000000	
#define L2CR_SRAM_FULL		0x00010000	
#define L2CR_SRAM_HALF		0x00020000	
#define L2CR_SRAM_TWO_HALFS	0x00030000	
#define L2CR_SRAM_QUART		0x00040000	
#define L2CR_SRAM_TWO_QUARTS	0x00050000	
#define L2CR_SRAM_EIGHTH	0x00060000	
#define L2CR_SRAM_TWO_EIGHTH	0x00070000	

#define L2SRAM_OPTIMAL_SZ_SHIFT	0x00000003	

#define L2SRAM_BAR_MSK_LO18	0xFFFFC000	
#define L2SRAM_BARE_MSK_HI4	0x0000000F	

enum cache_sram_lock_ways {
	LOCK_WAYS_ZERO,
	LOCK_WAYS_EIGHTH,
	LOCK_WAYS_TWO_EIGHTH,
	LOCK_WAYS_HALF = 4,
	LOCK_WAYS_FULL = 8,
};

struct mpc85xx_l2ctlr {
	u32	ctl;		
	u8	res1[0xC];
	u32	ewar0;		
	u32	ewarea0;	
	u32	ewcr0;		
	u8	res2[4];
	u32	ewar1;		
	u32	ewarea1;	
	u32	ewcr1;		
	u8	res3[4];
	u32	ewar2;		
	u32	ewarea2;	
	u32	ewcr2;		
	u8	res4[4];
	u32	ewar3;		
	u32	ewarea3;	
	u32	ewcr3;		
	u8	res5[0xB4];
	u32	srbar0;		
	u32	srbarea0;	
	u32	srbar1;		
	u32	srbarea1;	
	u8	res6[0xCF0];
	u32	errinjhi;	
	u32	errinjlo;	
	u32	errinjctl;	
	u8	res7[0x14];
	u32	captdatahi;	
	u32	captdatalo;	
	u32	captecc;	
	u8	res8[0x14];
	u32	errdet;		
	u32	errdis;		
	u32	errinten;	
	u32	errattr;	
	u32	erradrrl;	
	u32	erradrrh;	
	u32	errctl;		
	u8	res9[0x1A4];
};

struct sram_parameters {
	unsigned int sram_size;
	uint64_t sram_offset;
};

extern int instantiate_cache_sram(struct platform_device *dev,
		struct sram_parameters sram_params);
extern void remove_cache_sram(struct platform_device *dev);

#endif 
