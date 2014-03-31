/*
 * Author: Kevin Wells <kevin.wells@nxp.com>
 *
 * Copyright (C) 2010 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MACH_GPIO_LPC32XX_H
#define __MACH_GPIO_LPC32XX_H


#define LPC32XX_GPIO_P0_MAX 8
#define LPC32XX_GPIO_P1_MAX 24
#define LPC32XX_GPIO_P2_MAX 13
#define LPC32XX_GPIO_P3_MAX 6
#define LPC32XX_GPI_P3_MAX 28
#define LPC32XX_GPO_P3_MAX 24

#define LPC32XX_GPIO_P0_GRP 0
#define LPC32XX_GPIO_P1_GRP (LPC32XX_GPIO_P0_GRP + LPC32XX_GPIO_P0_MAX)
#define LPC32XX_GPIO_P2_GRP (LPC32XX_GPIO_P1_GRP + LPC32XX_GPIO_P1_MAX)
#define LPC32XX_GPIO_P3_GRP (LPC32XX_GPIO_P2_GRP + LPC32XX_GPIO_P2_MAX)
#define LPC32XX_GPI_P3_GRP (LPC32XX_GPIO_P3_GRP + LPC32XX_GPIO_P3_MAX)
#define LPC32XX_GPO_P3_GRP (LPC32XX_GPI_P3_GRP + LPC32XX_GPI_P3_MAX)

#define LPC32XX_GPIO(x, y) ((x) + (y))

#endif 
