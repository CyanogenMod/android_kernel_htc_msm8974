/*
 * include/linux/fsl_devices.h
 *
 * Definitions for any platform device related flags or structures for
 * Freescale processor devices
 *
 * Maintainer: Kumar Gala <galak@kernel.crashing.org>
 *
 * Copyright 2004 Freescale Semiconductor, Inc
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _FSL_DEVICE_H_
#define _FSL_DEVICE_H_

#include <linux/types.h>


enum fsl_usb2_operating_modes {
	FSL_USB2_MPH_HOST,
	FSL_USB2_DR_HOST,
	FSL_USB2_DR_DEVICE,
	FSL_USB2_DR_OTG,
};

enum fsl_usb2_phy_modes {
	FSL_USB2_PHY_NONE,
	FSL_USB2_PHY_ULPI,
	FSL_USB2_PHY_UTMI,
	FSL_USB2_PHY_UTMI_WIDE,
	FSL_USB2_PHY_SERIAL,
};

struct clk;
struct platform_device;

struct fsl_usb2_platform_data {
	
	enum fsl_usb2_operating_modes	operating_mode;
	enum fsl_usb2_phy_modes		phy_mode;
	unsigned int			port_enables;
	unsigned int			workaround;

	int		(*init)(struct platform_device *);
	void		(*exit)(struct platform_device *);
	void __iomem	*regs;		
	struct clk	*clk;
	unsigned	power_budget;	
	unsigned	big_endian_mmio:1;
	unsigned	big_endian_desc:1;
	unsigned	es:1;		
	unsigned	le_setup_buf:1;
	unsigned	have_sysif_regs:1;
	unsigned	invert_drvvbus:1;
	unsigned	invert_pwr_fault:1;

	unsigned	suspended:1;
	unsigned	already_suspended:1;

	
	u32		pm_command;
	u32		pm_status;
	u32		pm_intr_enable;
	u32		pm_frame_index;
	u32		pm_segment;
	u32		pm_frame_list;
	u32		pm_async_next;
	u32		pm_configured_flag;
	u32		pm_portsc;
	u32		pm_usbgenctrl;
};

#define FSL_USB2_PORT0_ENABLED	0x00000001
#define FSL_USB2_PORT1_ENABLED	0x00000002

#define FLS_USB2_WORKAROUND_ENGCM09152	(1 << 0)

struct spi_device;

struct fsl_spi_platform_data {
	u32 	initial_spmode;	
	s16	bus_num;
	unsigned int flags;
#define SPI_QE_CPU_MODE		(1 << 0) 
#define SPI_CPM_MODE		(1 << 1) 
#define SPI_CPM1		(1 << 2) 
#define SPI_CPM2		(1 << 3) 
#define SPI_QE			(1 << 4) 
	
	u16	max_chipselect;
	void	(*cs_control)(struct spi_device *spi, bool on);
	u32	sysclk;
};

struct mpc8xx_pcmcia_ops {
	void(*hw_ctrl)(int slot, int enable);
	int(*voltage_set)(int slot, int vcc, int vpp);
};

#if defined(CONFIG_PPC_83xx) && defined(CONFIG_SUSPEND)
int fsl_deep_sleep(void);
#else
static inline int fsl_deep_sleep(void) { return 0; }
#endif

#endif 
