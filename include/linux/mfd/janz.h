/*
 * Common Definitions for Janz MODULbus devices
 *
 * Copyright (c) 2010 Ira W. Snyder <iws@ovro.caltech.edu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef JANZ_H
#define JANZ_H

struct janz_platform_data {
	
	unsigned int modno;
};

struct janz_cmodio_onboard_regs {
	u8 unused1;

	u8 int_disable;
	u8 unused2;

	u8 int_enable;
	u8 unused3;

	
	u8 reset_assert;
	u8 unused4;

	
	u8 reset_deassert;
	u8 unused5;

	
	u8 eep;
	u8 unused6;

	
	u8 enid;
};

#endif 
