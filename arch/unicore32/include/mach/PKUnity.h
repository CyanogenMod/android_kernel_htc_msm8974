/*
 * linux/arch/unicore32/include/mach/PKUnity.h
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MACH_PUV3_HARDWARE_H__
#error You must include hardware.h not PKUnity.h
#endif

#include "bitfield.h"

#define PKUNITY_SDRAM_BASE		0x00000000 
#define PKUNITY_MMIO_BASE		0x80000000 

#define PKUNITY_PCI_BASE		io_p2v(0x80000000) 
#include "regs-pci.h"

#define PKUNITY_PCICFG_BASE		(PKUNITY_PCI_BASE + 0x0)
#define PKUNITY_PCIBRI_BASE		(PKUNITY_PCI_BASE + 0x00010000)
#define PKUNITY_PCILIO_BASE		(PKUNITY_PCI_BASE + 0x00030000)
#define PKUNITY_PCIMEM_BASE		(PKUNITY_PCI_BASE + 0x10000000)
#define PKUNITY_PCIAHB_BASE		(PKUNITY_PCI_BASE + 0x18000000)

#define PKUNITY_AHB_BASE		io_p2v(0xC0000000)

#define PKUNITY_ARBITER_BASE		(PKUNITY_AHB_BASE + 0x000000) 
#define PKUNITY_DDR2CTRL_BASE		(PKUNITY_AHB_BASE + 0x100000) 
#define PKUNITY_DMAC_BASE		(PKUNITY_AHB_BASE + 0x200000) 
#include "regs-dmac.h"
#define PKUNITY_UMAL_BASE		(PKUNITY_AHB_BASE + 0x300000) 
#include "regs-umal.h"
#define PKUNITY_USB_BASE		(PKUNITY_AHB_BASE + 0x400000) 
#define PKUNITY_SATA_BASE		(PKUNITY_AHB_BASE + 0x500000) 
#define PKUNITY_SMC_BASE		(PKUNITY_AHB_BASE + 0x600000) 
#define PKUNITY_MME_BASE		(PKUNITY_AHB_BASE + 0x700000) 
#define PKUNITY_UNIGFX_BASE		(PKUNITY_AHB_BASE + 0x800000) 
#include "regs-unigfx.h"
#define PKUNITY_NAND_BASE		(PKUNITY_AHB_BASE + 0x900000) 
#include "regs-nand.h"
#define PKUNITY_H264D_BASE		(PKUNITY_AHB_BASE + 0xA00000) 
#define PKUNITY_H264E_BASE		(PKUNITY_AHB_BASE + 0xB00000) 

#define PKUNITY_APB_BASE		io_p2v(0xEE000000)

#define PKUNITY_UART0_BASE		(PKUNITY_APB_BASE + 0x000000) 
#define PKUNITY_UART1_BASE		(PKUNITY_APB_BASE + 0x100000) 
#include "regs-uart.h"
#define PKUNITY_I2C_BASE		(PKUNITY_APB_BASE + 0x200000) 
#include "regs-i2c.h"
#define PKUNITY_SPI_BASE		(PKUNITY_APB_BASE + 0x300000) 
#include "regs-spi.h"
#define PKUNITY_AC97_BASE		(PKUNITY_APB_BASE + 0x400000) 
#include "regs-ac97.h"
#define PKUNITY_GPIO_BASE		(PKUNITY_APB_BASE + 0x500000) 
#include "regs-gpio.h"
#define PKUNITY_INTC_BASE		(PKUNITY_APB_BASE + 0x600000) 
#include "regs-intc.h"
#define PKUNITY_RTC_BASE		(PKUNITY_APB_BASE + 0x700000) 
#include "regs-rtc.h"
#define PKUNITY_OST_BASE		(PKUNITY_APB_BASE + 0x800000) 
#include "regs-ost.h"
#define PKUNITY_RESETC_BASE		(PKUNITY_APB_BASE + 0x900000) 
#include "regs-resetc.h"
#define PKUNITY_PM_BASE			(PKUNITY_APB_BASE + 0xA00000) 
#include "regs-pm.h"
#define PKUNITY_PS2_BASE		(PKUNITY_APB_BASE + 0xB00000) 
#include "regs-ps2.h"
#define PKUNITY_SDC_BASE		(PKUNITY_APB_BASE + 0xC00000) 
#include "regs-sdc.h"

