/*
 * Memory pre-allocations for Gaia boxes.
 *
 * Copyright (C) 2005-2009 Scientific-Atlanta, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Author:       David VomLehn
 */

#include <linux/init.h>
#include <asm/mach-powertv/asic.h>

struct resource dvr_gaia_resources[] __initdata = {
	{
		.name   = "ST231aImage",	
		.start  = 0x24000000,
		.end    = 0x241FFFFF,		
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ST231aMonitor",	
		.start  = 0x24200000,
		.end    = 0x24201FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "MediaMemory1",
		.start  = 0x24202000,
		.end    = 0x25FFFFFF, 
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ST231bImage",	
		.start  = 0x60000000,
		.end    = 0x601FFFFF,		
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "ST231bMonitor",	
		.start  = 0x60200000,
		.end    = 0x60201FFF,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "MediaMemory2",
		.start  = 0x60202000,
		.end    = 0x61FFFFFF, 
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "DSP_Image_Buff",
		.start  = 0x00000000,
		.end    = 0x000FFFFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ADSC_CPU_PCM_Buff",
		.start  = 0x00000000,
		.end    = 0x00009FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ADSC_AUX_Buff",
		.start  = 0x00000000,
		.end    = 0x00003FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ADSC_Main_Buff",
		.start  = 0x00000000,
		.end    = 0x00003FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "AVMEMPartition0",
		.start  = 0x63580000,
		.end    = 0x64180000 - 1,  
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "Docsis",
		.start  = 0x62000000,
		.end    = 0x62700000 - 1,	
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "GraphicsHeap",
		.start  = 0x62700000,
		.end    = 0x63500000 - 1,	
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "MulticomSHM",
		.start  = 0x26000000,
		.end    = 0x26020000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "BMM_Buffer",
		.start  = 0x00000000,
		.end    = 0x00280000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "DisplayBins0",
		.start  = 0x00000000,
		.end    = 0x00000FFF,		
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "DisplayBins1",
		.start  = 0x64AD4000,
		.end    = 0x64AD5000 - 1,  
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "ITFS",
		.start  = 0x64180000,
		
		.end    = 0x6430DFFF,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "AvfsDmaMem",
		.start  = 0x6430E000,
		
		.end    = 0x64AD0000 - 1,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "AvfsFileSys",
		.start  = 0x64AD0000,
		.end    = 0x64AD1000 - 1,  
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "SmartCardInfo",
		.start  = 0x64AD1000,
		.end    = 0x64AD3800 - 1,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "NP_Reset_Vector",
		.start  = 0x27c00000,
		.end    = 0x27c01000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "NP_Image",
		.start  = 0x27020000,
		.end    = 0x27060000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "NP_IPC",
		.start  = 0x63500000,
		.end    = 0x63580000 - 1,
		.flags  = IORESOURCE_IO,
	},
	{ },
};

struct resource non_dvr_gaia_resources[] __initdata = {
	{
		.name   = "ST231aImage",	
		.start  = 0x24000000,
		.end    = 0x241FFFFF,		
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ST231aMonitor",	
		.start  = 0x24200000,
		.end    = 0x24201FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "MediaMemory1",
		.start  = 0x24202000,
		.end    = 0x25FFFFFF, 
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ST231bImage",	
		.start  = 0x60000000,
		.end    = 0x601FFFFF,		
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "ST231bMonitor",	
		.start  = 0x60200000,
		.end    = 0x60201FFF,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "MediaMemory2",
		.start  = 0x60202000,
		.end    = 0x61FFFFFF, 
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "DSP_Image_Buff",
		.start  = 0x00000000,
		.end    = 0x000FFFFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ADSC_CPU_PCM_Buff",
		.start  = 0x00000000,
		.end    = 0x00009FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ADSC_AUX_Buff",
		.start  = 0x00000000,
		.end    = 0x00003FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "ADSC_Main_Buff",
		.start  = 0x00000000,
		.end    = 0x00003FFF,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "AVMEMPartition0",
		.start  = 0x63580000,
		.end    = 0x64180000 - 1,  
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "Docsis",
		.start  = 0x62000000,
		.end    = 0x62700000 - 1,	
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "GraphicsHeap",
		.start  = 0x62700000,
		.end    = 0x63500000 - 1,	
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "MulticomSHM",
		.start  = 0x26000000,
		.end    = 0x26020000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "BMM_Buffer",
		.start  = 0x00000000,
		.end    = 0x000AA000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "DisplayBins0",
		.start  = 0x00000000,
		.end    = 0x00000FFF,		
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "DisplayBins1",
		.start  = 0x64AD4000,
		.end    = 0x64AD5000 - 1,  
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "AvfsDmaMem",
		.start  = 0x6430E000,
		.end    = 0x645D2C00 - 1,  
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "DiagPersistentMemory",
		.start  = 0x00000000,
		.end    = 0x10000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "SmartCardInfo",
		.start  = 0x64AD1000,
		.end    = 0x64AD3800 - 1,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "NP_Reset_Vector",
		.start  = 0x27c00000,
		.end    = 0x27c01000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "NP_Image",
		.start  = 0x27020000,
		.end    = 0x27060000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "NP_IPC",
		.start  = 0x63500000,
		.end    = 0x63580000 - 1,
		.flags  = IORESOURCE_IO,
	},
	{ },
};
