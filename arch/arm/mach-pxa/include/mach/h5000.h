/*
 * Hardware definitions for HP iPAQ h5xxx Handheld Computers
 *
 * Copyright(20)02 Hewlett-Packard Company.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks
 */

#ifndef __ASM_ARCH_H5000_H
#define __ASM_ARCH_H5000_H

#include <mach/mfp-pxa25x.h>


#define H5000_GPIO_POWER_BUTTON   (0)
#define H5000_GPIO_RESET_BUTTON_N (1)
#define H5000_GPIO_OPT_INT        (2)
#define H5000_GPIO_BACKUP_POWER   (3)
#define H5000_GPIO_ACTION_BUTTON  (4)
#define H5000_GPIO_COM_DCD_SOMETHING  (5) 
#define H5000_GPIO_RESET_BUTTON_AGAIN_N (7) 
#define H5000_GPIO_RSO_N          (9)       
#define H5000_GPIO_ASIC_INT_N   (10)       
#define H5000_GPIO_BT_ENV_0     (11)       
#define H5000_GPIO_BT_ENV_1     (13)       
#define H5000_GPIO_BT_WU        (14)       
#define H5000_GPIO_OE_RD_NWR	(21)       
#define H5000_GPIO_OPT_SPI_CLK  (23)       
#define H5000_GPIO_OPT_SPI_CS_N (24)       
#define H5000_GPIO_OPT_SPI_DOUT (25)       
#define H5000_GPIO_OPT_SPI_DIN  (26)       
#define H5000_GPIO_I2S_BITCLK   (28)       
#define H5000_GPIO_I2S_DATAOUT  (29)       
#define H5000_GPIO_I2S_DATAIN   (30)       
#define H5000_GPIO_I2S_LRCLK    (31)       
#define H5000_GPIO_I2S_SYSCLK   (32)       
#define H5000_GPIO_COM_RXD      (34)       
#define H5000_GPIO_COM_CTS      (35)       
#define H5000_GPIO_COM_DCD      (36)       
#define H5000_GPIO_COM_DSR      (37)       
#define H5000_GPIO_COM_RI       (38)       
#define H5000_GPIO_COM_TXD      (39)       
#define H5000_GPIO_COM_DTR      (40)       
#define H5000_GPIO_COM_RTS      (41)       

#define H5000_GPIO_BT_RXD       (42)       
#define H5000_GPIO_BT_TXD       (43)       
#define H5000_GPIO_BT_CTS       (44)       
#define H5000_GPIO_BT_RTS       (45)       

#define H5000_GPIO_IRDA_RXD     (46)
#define H5000_GPIO_IRDA_TXD     (47)

#define H5000_GPIO_POE_N        (48)       
#define H5000_GPIO_PWE_N        (49)       
#define H5000_GPIO_PIOR_N       (50)       
#define H5000_GPIO_PIOW_N       (51)       
#define H5000_GPIO_PCE1_N       (52)       
#define H5000_GPIO_PCE2_N       (53)       
#define H5000_GPIO_PSKTSEL      (54)       
#define H5000_GPIO_PREG_N       (55)       
#define H5000_GPIO_PWAIT_N      (56)       
#define H5000_GPIO_IOIS16_N     (57)       

#define H5000_GPIO_IRDA_SD      (58)       
#define H5000_GPIO_POWER_SD_N   (60)       
#define H5000_GPIO_POWER_RS232_N	(61)       
#define H5000_GPIO_POWER_ACCEL_N	(62)       
#define H5000_GPIO_OPT_NVRAM    (64)       
#define H5000_GPIO_CHG_EN       (65)       
#define H5000_GPIO_USB_PULLUP   (66)       
#define H5000_GPIO_BT_2V8_N     (67)       
#define H5000_GPIO_EXT_CHG_RATE (68)       
#define H5000_GPIO_CIR_RESET    (70)       
#define H5000_GPIO_POWER_LIGHT_SENSOR_N	(71)
#define H5000_GPIO_BT_M_RESET   (72)
#define H5000_GPIO_STD_CHG_RATE (73)
#define H5000_GPIO_SD_WP_N      (74)
#define H5000_GPIO_MOTOR_ON_N   (75)       
#define H5000_GPIO_HEADPHONE_DETECT	(76)
#define H5000_GPIO_USB_CHG_RATE (77)       

#endif 
