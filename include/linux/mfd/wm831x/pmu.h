/*
 * include/linux/mfd/wm831x/pmu.h -- PMU for WM831x
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

#ifndef __MFD_WM831X_PMU_H__
#define __MFD_WM831X_PMU_H__

#define WM831X_CHIP_ON                          0x8000  
#define WM831X_CHIP_ON_MASK                     0x8000  
#define WM831X_CHIP_ON_SHIFT                        15  
#define WM831X_CHIP_ON_WIDTH                         1  
#define WM831X_CHIP_SLP                         0x4000  
#define WM831X_CHIP_SLP_MASK                    0x4000  
#define WM831X_CHIP_SLP_SHIFT                       14  
#define WM831X_CHIP_SLP_WIDTH                        1  
#define WM831X_REF_LP                           0x1000  
#define WM831X_REF_LP_MASK                      0x1000  
#define WM831X_REF_LP_SHIFT                         12  
#define WM831X_REF_LP_WIDTH                          1  
#define WM831X_PWRSTATE_DLY_MASK                0x0C00  
#define WM831X_PWRSTATE_DLY_SHIFT                   10  
#define WM831X_PWRSTATE_DLY_WIDTH                    2  
#define WM831X_SWRST_DLY                        0x0200  
#define WM831X_SWRST_DLY_MASK                   0x0200  
#define WM831X_SWRST_DLY_SHIFT                       9  
#define WM831X_SWRST_DLY_WIDTH                       1  
#define WM831X_USB100MA_STARTUP_MASK            0x0030  
#define WM831X_USB100MA_STARTUP_SHIFT                4  
#define WM831X_USB100MA_STARTUP_WIDTH                2  
#define WM831X_USB_CURR_STS                     0x0008  
#define WM831X_USB_CURR_STS_MASK                0x0008  
#define WM831X_USB_CURR_STS_SHIFT                    3  
#define WM831X_USB_CURR_STS_WIDTH                    1  
#define WM831X_USB_ILIM_MASK                    0x0007  
#define WM831X_USB_ILIM_SHIFT                        0  
#define WM831X_USB_ILIM_WIDTH                        3  

#define WM831X_THW_STS                          0x8000  
#define WM831X_THW_STS_MASK                     0x8000  
#define WM831X_THW_STS_SHIFT                        15  
#define WM831X_THW_STS_WIDTH                         1  
#define WM831X_PWR_SRC_BATT                     0x0400  
#define WM831X_PWR_SRC_BATT_MASK                0x0400  
#define WM831X_PWR_SRC_BATT_SHIFT                   10  
#define WM831X_PWR_SRC_BATT_WIDTH                    1  
#define WM831X_PWR_WALL                         0x0200  
#define WM831X_PWR_WALL_MASK                    0x0200  
#define WM831X_PWR_WALL_SHIFT                        9  
#define WM831X_PWR_WALL_WIDTH                        1  
#define WM831X_PWR_USB                          0x0100  
#define WM831X_PWR_USB_MASK                     0x0100  
#define WM831X_PWR_USB_SHIFT                         8  
#define WM831X_PWR_USB_WIDTH                         1  
#define WM831X_MAIN_STATE_MASK                  0x001F  
#define WM831X_MAIN_STATE_SHIFT                      0  
#define WM831X_MAIN_STATE_WIDTH                      5  

#define WM831X_CHG_ENA                          0x8000  
#define WM831X_CHG_ENA_MASK                     0x8000  
#define WM831X_CHG_ENA_SHIFT                        15  
#define WM831X_CHG_ENA_WIDTH                         1  
#define WM831X_CHG_FRC                          0x4000  
#define WM831X_CHG_FRC_MASK                     0x4000  
#define WM831X_CHG_FRC_SHIFT                        14  
#define WM831X_CHG_FRC_WIDTH                         1  
#define WM831X_CHG_ITERM_MASK                   0x1C00  
#define WM831X_CHG_ITERM_SHIFT                      10  
#define WM831X_CHG_ITERM_WIDTH                       3  
#define WM831X_CHG_FAST                         0x0020  
#define WM831X_CHG_FAST_MASK                    0x0020  
#define WM831X_CHG_FAST_SHIFT                        5  
#define WM831X_CHG_FAST_WIDTH                        1  
#define WM831X_CHG_IMON_ENA                     0x0002  
#define WM831X_CHG_IMON_ENA_MASK                0x0002  
#define WM831X_CHG_IMON_ENA_SHIFT                    1  
#define WM831X_CHG_IMON_ENA_WIDTH                    1  
#define WM831X_CHG_CHIP_TEMP_MON                0x0001  
#define WM831X_CHG_CHIP_TEMP_MON_MASK           0x0001  
#define WM831X_CHG_CHIP_TEMP_MON_SHIFT               0  
#define WM831X_CHG_CHIP_TEMP_MON_WIDTH               1  

#define WM831X_CHG_OFF_MSK                      0x4000  
#define WM831X_CHG_OFF_MSK_MASK                 0x4000  
#define WM831X_CHG_OFF_MSK_SHIFT                    14  
#define WM831X_CHG_OFF_MSK_WIDTH                     1  
#define WM831X_CHG_TIME_MASK                    0x0F00  
#define WM831X_CHG_TIME_SHIFT                        8  
#define WM831X_CHG_TIME_WIDTH                        4  
#define WM831X_CHG_TRKL_ILIM_MASK               0x00C0  
#define WM831X_CHG_TRKL_ILIM_SHIFT                   6  
#define WM831X_CHG_TRKL_ILIM_WIDTH                   2  
#define WM831X_CHG_VSEL_MASK                    0x0030  
#define WM831X_CHG_VSEL_SHIFT                        4  
#define WM831X_CHG_VSEL_WIDTH                        2  
#define WM831X_CHG_FAST_ILIM_MASK               0x000F  
#define WM831X_CHG_FAST_ILIM_SHIFT                   0  
#define WM831X_CHG_FAST_ILIM_WIDTH                   4  

#define WM831X_BATT_OV_STS                      0x8000  
#define WM831X_BATT_OV_STS_MASK                 0x8000  
#define WM831X_BATT_OV_STS_SHIFT                    15  
#define WM831X_BATT_OV_STS_WIDTH                     1  
#define WM831X_CHG_STATE_MASK                   0x7000  
#define WM831X_CHG_STATE_SHIFT                      12  
#define WM831X_CHG_STATE_WIDTH                       3  
#define WM831X_BATT_HOT_STS                     0x0800  
#define WM831X_BATT_HOT_STS_MASK                0x0800  
#define WM831X_BATT_HOT_STS_SHIFT                   11  
#define WM831X_BATT_HOT_STS_WIDTH                    1  
#define WM831X_BATT_COLD_STS                    0x0400  
#define WM831X_BATT_COLD_STS_MASK               0x0400  
#define WM831X_BATT_COLD_STS_SHIFT                  10  
#define WM831X_BATT_COLD_STS_WIDTH                   1  
#define WM831X_CHG_TOPOFF                       0x0200  
#define WM831X_CHG_TOPOFF_MASK                  0x0200  
#define WM831X_CHG_TOPOFF_SHIFT                      9  
#define WM831X_CHG_TOPOFF_WIDTH                      1  
#define WM831X_CHG_ACTIVE                       0x0100  
#define WM831X_CHG_ACTIVE_MASK                  0x0100  
#define WM831X_CHG_ACTIVE_SHIFT                      8  
#define WM831X_CHG_ACTIVE_WIDTH                      1  
#define WM831X_CHG_TIME_ELAPSED_MASK            0x00FF  
#define WM831X_CHG_TIME_ELAPSED_SHIFT                0  
#define WM831X_CHG_TIME_ELAPSED_WIDTH                8  

#define WM831X_CHG_STATE_OFF         (0 << WM831X_CHG_STATE_SHIFT)
#define WM831X_CHG_STATE_TRICKLE     (1 << WM831X_CHG_STATE_SHIFT)
#define WM831X_CHG_STATE_FAST        (2 << WM831X_CHG_STATE_SHIFT)
#define WM831X_CHG_STATE_TRICKLE_OT  (3 << WM831X_CHG_STATE_SHIFT)
#define WM831X_CHG_STATE_FAST_OT     (4 << WM831X_CHG_STATE_SHIFT)
#define WM831X_CHG_STATE_DEFECTIVE   (5 << WM831X_CHG_STATE_SHIFT)

#define WM831X_BKUP_CHG_ENA                     0x8000  
#define WM831X_BKUP_CHG_ENA_MASK                0x8000  
#define WM831X_BKUP_CHG_ENA_SHIFT                   15  
#define WM831X_BKUP_CHG_ENA_WIDTH                    1  
#define WM831X_BKUP_CHG_STS                     0x4000  
#define WM831X_BKUP_CHG_STS_MASK                0x4000  
#define WM831X_BKUP_CHG_STS_SHIFT                   14  
#define WM831X_BKUP_CHG_STS_WIDTH                    1  
#define WM831X_BKUP_CHG_MODE                    0x1000  
#define WM831X_BKUP_CHG_MODE_MASK               0x1000  
#define WM831X_BKUP_CHG_MODE_SHIFT                  12  
#define WM831X_BKUP_CHG_MODE_WIDTH                   1  
#define WM831X_BKUP_BATT_DET_ENA                0x0800  
#define WM831X_BKUP_BATT_DET_ENA_MASK           0x0800  
#define WM831X_BKUP_BATT_DET_ENA_SHIFT              11  
#define WM831X_BKUP_BATT_DET_ENA_WIDTH               1  
#define WM831X_BKUP_BATT_STS                    0x0400  
#define WM831X_BKUP_BATT_STS_MASK               0x0400  
#define WM831X_BKUP_BATT_STS_SHIFT                  10  
#define WM831X_BKUP_BATT_STS_WIDTH                   1  
#define WM831X_BKUP_CHG_VLIM                    0x0010  
#define WM831X_BKUP_CHG_VLIM_MASK               0x0010  
#define WM831X_BKUP_CHG_VLIM_SHIFT                   4  
#define WM831X_BKUP_CHG_VLIM_WIDTH                   1  
#define WM831X_BKUP_CHG_ILIM_MASK               0x0003  
#define WM831X_BKUP_CHG_ILIM_SHIFT                   0  
#define WM831X_BKUP_CHG_ILIM_WIDTH                   2  

#endif
