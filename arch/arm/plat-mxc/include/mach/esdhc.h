/*
 * Copyright 2010 Wolfram Sang <w.sang@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef __ASM_ARCH_IMX_ESDHC_H
#define __ASM_ARCH_IMX_ESDHC_H

enum wp_types {
	ESDHC_WP_NONE,		
	ESDHC_WP_CONTROLLER,	
	ESDHC_WP_GPIO,		
};

enum cd_types {
	ESDHC_CD_NONE,		
	ESDHC_CD_CONTROLLER,	
	ESDHC_CD_GPIO,		
	ESDHC_CD_PERMANENT,	
};


struct esdhc_platform_data {
	unsigned int wp_gpio;
	unsigned int cd_gpio;
	enum wp_types wp_type;
	enum cd_types cd_type;
};
#endif 
