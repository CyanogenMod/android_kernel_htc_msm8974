/*
 * Static Memory Controller for AT32 chips
 *
 * Copyright (C) 2006 Atmel Corporation
 *
 * Inspired by the OMAP2 General-Purpose Memory Controller interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ARCH_AT32AP_SMC_H
#define __ARCH_AT32AP_SMC_H

struct smc_timing {
	
	int ncs_read_setup;
	int nrd_setup;
	int ncs_write_setup;
	int nwe_setup;

	
	int ncs_read_pulse;
	int nrd_pulse;
	int ncs_write_pulse;
	int nwe_pulse;

	
	int read_cycle;
	int write_cycle;

	
	int ncs_read_recover;
	int nrd_recover;
	int ncs_write_recover;
	int nwe_recover;
};

struct smc_config {

	
	u8		ncs_read_setup;
	u8		nrd_setup;
	u8		ncs_write_setup;
	u8		nwe_setup;

	
	u8		ncs_read_pulse;
	u8		nrd_pulse;
	u8		ncs_write_pulse;
	u8		nwe_pulse;

	
	u8		read_cycle;
	u8		write_cycle;

	
	u8		bus_width;

	unsigned int	nrd_controlled:1;

	unsigned int	nwe_controlled:1;

	unsigned int	nwait_mode:2;

	unsigned int	byte_write:1;

	unsigned int	tdf_cycles:4;

	unsigned int	tdf_mode:1;
};

extern void smc_set_timing(struct smc_config *config,
			   const struct smc_timing *timing);

extern int smc_set_configuration(int cs, const struct smc_config *config);
extern struct smc_config *smc_get_configuration(int cs);

#endif 
