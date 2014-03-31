/*
 * include/linux/mfd/wm831x/auxadc.h -- Auxiliary ADC interface for WM831x
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM831X_AUXADC_H__
#define __MFD_WM831X_AUXADC_H__

#define WM831X_AUX_DATA_SRC_MASK                0xF000  
#define WM831X_AUX_DATA_SRC_SHIFT                   12  
#define WM831X_AUX_DATA_SRC_WIDTH                    4  
#define WM831X_AUX_DATA_MASK                    0x0FFF  
#define WM831X_AUX_DATA_SHIFT                        0  
#define WM831X_AUX_DATA_WIDTH                       12  

#define WM831X_AUX_ENA                          0x8000  
#define WM831X_AUX_ENA_MASK                     0x8000  
#define WM831X_AUX_ENA_SHIFT                        15  
#define WM831X_AUX_ENA_WIDTH                         1  
#define WM831X_AUX_CVT_ENA                      0x4000  
#define WM831X_AUX_CVT_ENA_MASK                 0x4000  
#define WM831X_AUX_CVT_ENA_SHIFT                    14  
#define WM831X_AUX_CVT_ENA_WIDTH                     1  
#define WM831X_AUX_SLPENA                       0x1000  
#define WM831X_AUX_SLPENA_MASK                  0x1000  
#define WM831X_AUX_SLPENA_SHIFT                     12  
#define WM831X_AUX_SLPENA_WIDTH                      1  
#define WM831X_AUX_FRC_ENA                      0x0800  
#define WM831X_AUX_FRC_ENA_MASK                 0x0800  
#define WM831X_AUX_FRC_ENA_SHIFT                    11  
#define WM831X_AUX_FRC_ENA_WIDTH                     1  
#define WM831X_AUX_RATE_MASK                    0x003F  
#define WM831X_AUX_RATE_SHIFT                        0  
#define WM831X_AUX_RATE_WIDTH                        6  

#define WM831X_AUX_CAL_SEL                      0x8000  
#define WM831X_AUX_CAL_SEL_MASK                 0x8000  
#define WM831X_AUX_CAL_SEL_SHIFT                    15  
#define WM831X_AUX_CAL_SEL_WIDTH                     1  
#define WM831X_AUX_BKUP_BATT_SEL                0x0400  
#define WM831X_AUX_BKUP_BATT_SEL_MASK           0x0400  
#define WM831X_AUX_BKUP_BATT_SEL_SHIFT              10  
#define WM831X_AUX_BKUP_BATT_SEL_WIDTH               1  
#define WM831X_AUX_WALL_SEL                     0x0200  
#define WM831X_AUX_WALL_SEL_MASK                0x0200  
#define WM831X_AUX_WALL_SEL_SHIFT                    9  
#define WM831X_AUX_WALL_SEL_WIDTH                    1  
#define WM831X_AUX_BATT_SEL                     0x0100  
#define WM831X_AUX_BATT_SEL_MASK                0x0100  
#define WM831X_AUX_BATT_SEL_SHIFT                    8  
#define WM831X_AUX_BATT_SEL_WIDTH                    1  
#define WM831X_AUX_USB_SEL                      0x0080  
#define WM831X_AUX_USB_SEL_MASK                 0x0080  
#define WM831X_AUX_USB_SEL_SHIFT                     7  
#define WM831X_AUX_USB_SEL_WIDTH                     1  
#define WM831X_AUX_SYSVDD_SEL                   0x0040  
#define WM831X_AUX_SYSVDD_SEL_MASK              0x0040  
#define WM831X_AUX_SYSVDD_SEL_SHIFT                  6  
#define WM831X_AUX_SYSVDD_SEL_WIDTH                  1  
#define WM831X_AUX_BATT_TEMP_SEL                0x0020  
#define WM831X_AUX_BATT_TEMP_SEL_MASK           0x0020  
#define WM831X_AUX_BATT_TEMP_SEL_SHIFT               5  
#define WM831X_AUX_BATT_TEMP_SEL_WIDTH               1  
#define WM831X_AUX_CHIP_TEMP_SEL                0x0010  
#define WM831X_AUX_CHIP_TEMP_SEL_MASK           0x0010  
#define WM831X_AUX_CHIP_TEMP_SEL_SHIFT               4  
#define WM831X_AUX_CHIP_TEMP_SEL_WIDTH               1  
#define WM831X_AUX_AUX4_SEL                     0x0008  
#define WM831X_AUX_AUX4_SEL_MASK                0x0008  
#define WM831X_AUX_AUX4_SEL_SHIFT                    3  
#define WM831X_AUX_AUX4_SEL_WIDTH                    1  
#define WM831X_AUX_AUX3_SEL                     0x0004  
#define WM831X_AUX_AUX3_SEL_MASK                0x0004  
#define WM831X_AUX_AUX3_SEL_SHIFT                    2  
#define WM831X_AUX_AUX3_SEL_WIDTH                    1  
#define WM831X_AUX_AUX2_SEL                     0x0002  
#define WM831X_AUX_AUX2_SEL_MASK                0x0002  
#define WM831X_AUX_AUX2_SEL_SHIFT                    1  
#define WM831X_AUX_AUX2_SEL_WIDTH                    1  
#define WM831X_AUX_AUX1_SEL                     0x0001  
#define WM831X_AUX_AUX1_SEL_MASK                0x0001  
#define WM831X_AUX_AUX1_SEL_SHIFT                    0  
#define WM831X_AUX_AUX1_SEL_WIDTH                    1  

#define WM831X_DCOMP4_STS                       0x0800  
#define WM831X_DCOMP4_STS_MASK                  0x0800  
#define WM831X_DCOMP4_STS_SHIFT                     11  
#define WM831X_DCOMP4_STS_WIDTH                      1  
#define WM831X_DCOMP3_STS                       0x0400  
#define WM831X_DCOMP3_STS_MASK                  0x0400  
#define WM831X_DCOMP3_STS_SHIFT                     10  
#define WM831X_DCOMP3_STS_WIDTH                      1  
#define WM831X_DCOMP2_STS                       0x0200  
#define WM831X_DCOMP2_STS_MASK                  0x0200  
#define WM831X_DCOMP2_STS_SHIFT                      9  
#define WM831X_DCOMP2_STS_WIDTH                      1  
#define WM831X_DCOMP1_STS                       0x0100  
#define WM831X_DCOMP1_STS_MASK                  0x0100  
#define WM831X_DCOMP1_STS_SHIFT                      8  
#define WM831X_DCOMP1_STS_WIDTH                      1  
#define WM831X_DCMP4_ENA                        0x0008  
#define WM831X_DCMP4_ENA_MASK                   0x0008  
#define WM831X_DCMP4_ENA_SHIFT                       3  
#define WM831X_DCMP4_ENA_WIDTH                       1  
#define WM831X_DCMP3_ENA                        0x0004  
#define WM831X_DCMP3_ENA_MASK                   0x0004  
#define WM831X_DCMP3_ENA_SHIFT                       2  
#define WM831X_DCMP3_ENA_WIDTH                       1  
#define WM831X_DCMP2_ENA                        0x0002  
#define WM831X_DCMP2_ENA_MASK                   0x0002  
#define WM831X_DCMP2_ENA_SHIFT                       1  
#define WM831X_DCMP2_ENA_WIDTH                       1  
#define WM831X_DCMP1_ENA                        0x0001  
#define WM831X_DCMP1_ENA_MASK                   0x0001  
#define WM831X_DCMP1_ENA_SHIFT                       0  
#define WM831X_DCMP1_ENA_WIDTH                       1  

#define WM831X_DCMP1_SRC_MASK                   0xE000  
#define WM831X_DCMP1_SRC_SHIFT                      13  
#define WM831X_DCMP1_SRC_WIDTH                       3  
#define WM831X_DCMP1_GT                         0x1000  
#define WM831X_DCMP1_GT_MASK                    0x1000  
#define WM831X_DCMP1_GT_SHIFT                       12  
#define WM831X_DCMP1_GT_WIDTH                        1  
#define WM831X_DCMP1_THR_MASK                   0x0FFF  
#define WM831X_DCMP1_THR_SHIFT                       0  
#define WM831X_DCMP1_THR_WIDTH                      12  

#define WM831X_DCMP2_SRC_MASK                   0xE000  
#define WM831X_DCMP2_SRC_SHIFT                      13  
#define WM831X_DCMP2_SRC_WIDTH                       3  
#define WM831X_DCMP2_GT                         0x1000  
#define WM831X_DCMP2_GT_MASK                    0x1000  
#define WM831X_DCMP2_GT_SHIFT                       12  
#define WM831X_DCMP2_GT_WIDTH                        1  
#define WM831X_DCMP2_THR_MASK                   0x0FFF  
#define WM831X_DCMP2_THR_SHIFT                       0  
#define WM831X_DCMP2_THR_WIDTH                      12  

#define WM831X_DCMP3_SRC_MASK                   0xE000  
#define WM831X_DCMP3_SRC_SHIFT                      13  
#define WM831X_DCMP3_SRC_WIDTH                       3  
#define WM831X_DCMP3_GT                         0x1000  
#define WM831X_DCMP3_GT_MASK                    0x1000  
#define WM831X_DCMP3_GT_SHIFT                       12  
#define WM831X_DCMP3_GT_WIDTH                        1  
#define WM831X_DCMP3_THR_MASK                   0x0FFF  
#define WM831X_DCMP3_THR_SHIFT                       0  
#define WM831X_DCMP3_THR_WIDTH                      12  

#define WM831X_DCMP4_SRC_MASK                   0xE000  
#define WM831X_DCMP4_SRC_SHIFT                      13  
#define WM831X_DCMP4_SRC_WIDTH                       3  
#define WM831X_DCMP4_GT                         0x1000  
#define WM831X_DCMP4_GT_MASK                    0x1000  
#define WM831X_DCMP4_GT_SHIFT                       12  
#define WM831X_DCMP4_GT_WIDTH                        1  
#define WM831X_DCMP4_THR_MASK                   0x0FFF  
#define WM831X_DCMP4_THR_SHIFT                       0  
#define WM831X_DCMP4_THR_WIDTH                      12  

#define WM831X_AUX_CAL_FACTOR  0xfff
#define WM831X_AUX_CAL_NOMINAL 0x222

enum wm831x_auxadc {
	WM831X_AUX_CAL = 15,
	WM831X_AUX_BKUP_BATT = 10,
	WM831X_AUX_WALL = 9,
	WM831X_AUX_BATT = 8,
	WM831X_AUX_USB = 7,
	WM831X_AUX_SYSVDD = 6,
	WM831X_AUX_BATT_TEMP = 5,
	WM831X_AUX_CHIP_TEMP = 4,
	WM831X_AUX_AUX4 = 3,
	WM831X_AUX_AUX3 = 2,
	WM831X_AUX_AUX2 = 1,
	WM831X_AUX_AUX1 = 0,
};

int wm831x_auxadc_read(struct wm831x *wm831x, enum wm831x_auxadc input);
int wm831x_auxadc_read_uv(struct wm831x *wm831x, enum wm831x_auxadc input);

#endif
