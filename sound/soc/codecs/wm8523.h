/*
 * wm8523.h  --  WM8423 ASoC driver
 *
 * Copyright 2009 Wolfson Microelectronics, plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * Based on wm8753.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8523_H
#define _WM8523_H

#define WM8523_DEVICE_ID                        0x00
#define WM8523_REVISION                         0x01
#define WM8523_PSCTRL1                          0x02
#define WM8523_AIF_CTRL1                        0x03
#define WM8523_AIF_CTRL2                        0x04
#define WM8523_DAC_CTRL3                        0x05
#define WM8523_DAC_GAINL                        0x06
#define WM8523_DAC_GAINR                        0x07
#define WM8523_ZERO_DETECT                      0x08

#define WM8523_REGISTER_COUNT                   9
#define WM8523_MAX_REGISTER                     0x08


#define WM8523_CHIP_ID_MASK                     0xFFFF  
#define WM8523_CHIP_ID_SHIFT                         0  
#define WM8523_CHIP_ID_WIDTH                        16  

#define WM8523_CHIP_REV_MASK                    0x0007  
#define WM8523_CHIP_REV_SHIFT                        0  
#define WM8523_CHIP_REV_WIDTH                        3  

#define WM8523_SYS_ENA_MASK                     0x0003  
#define WM8523_SYS_ENA_SHIFT                         0  
#define WM8523_SYS_ENA_WIDTH                         2  

#define WM8523_TDM_MODE_MASK                    0x1800  
#define WM8523_TDM_MODE_SHIFT                       11  
#define WM8523_TDM_MODE_WIDTH                        2  
#define WM8523_TDM_SLOT_MASK                    0x0600  
#define WM8523_TDM_SLOT_SHIFT                        9  
#define WM8523_TDM_SLOT_WIDTH                        2  
#define WM8523_DEEMPH                           0x0100  
#define WM8523_DEEMPH_MASK                      0x0100  
#define WM8523_DEEMPH_SHIFT                          8  
#define WM8523_DEEMPH_WIDTH                          1  
#define WM8523_AIF_MSTR                         0x0080  
#define WM8523_AIF_MSTR_MASK                    0x0080  
#define WM8523_AIF_MSTR_SHIFT                        7  
#define WM8523_AIF_MSTR_WIDTH                        1  
#define WM8523_LRCLK_INV                        0x0040  
#define WM8523_LRCLK_INV_MASK                   0x0040  
#define WM8523_LRCLK_INV_SHIFT                       6  
#define WM8523_LRCLK_INV_WIDTH                       1  
#define WM8523_BCLK_INV                         0x0020  
#define WM8523_BCLK_INV_MASK                    0x0020  
#define WM8523_BCLK_INV_SHIFT                        5  
#define WM8523_BCLK_INV_WIDTH                        1  
#define WM8523_WL_MASK                          0x0018  
#define WM8523_WL_SHIFT                              3  
#define WM8523_WL_WIDTH                              2  
#define WM8523_FMT_MASK                         0x0007  
#define WM8523_FMT_SHIFT                             0  
#define WM8523_FMT_WIDTH                             3  

#define WM8523_DAC_OP_MUX_MASK                  0x00C0  
#define WM8523_DAC_OP_MUX_SHIFT                      6  
#define WM8523_DAC_OP_MUX_WIDTH                      2  
#define WM8523_BCLKDIV_MASK                     0x0038  
#define WM8523_BCLKDIV_SHIFT                         3  
#define WM8523_BCLKDIV_WIDTH                         3  
#define WM8523_SR_MASK                          0x0007  
#define WM8523_SR_SHIFT                              0  
#define WM8523_SR_WIDTH                              3  

#define WM8523_ZC                               0x0010  
#define WM8523_ZC_MASK                          0x0010  
#define WM8523_ZC_SHIFT                              4  
#define WM8523_ZC_WIDTH                              1  
#define WM8523_DACR                             0x0008  
#define WM8523_DACR_MASK                        0x0008  
#define WM8523_DACR_SHIFT                            3  
#define WM8523_DACR_WIDTH                            1  
#define WM8523_DACL                             0x0004  
#define WM8523_DACL_MASK                        0x0004  
#define WM8523_DACL_SHIFT                            2  
#define WM8523_DACL_WIDTH                            1  
#define WM8523_VOL_UP_RAMP                      0x0002  
#define WM8523_VOL_UP_RAMP_MASK                 0x0002  
#define WM8523_VOL_UP_RAMP_SHIFT                     1  
#define WM8523_VOL_UP_RAMP_WIDTH                     1  
#define WM8523_VOL_DOWN_RAMP                    0x0001  
#define WM8523_VOL_DOWN_RAMP_MASK               0x0001  
#define WM8523_VOL_DOWN_RAMP_SHIFT                   0  
#define WM8523_VOL_DOWN_RAMP_WIDTH                   1  

#define WM8523_DACL_VU                          0x0200  
#define WM8523_DACL_VU_MASK                     0x0200  
#define WM8523_DACL_VU_SHIFT                         9  
#define WM8523_DACL_VU_WIDTH                         1  
#define WM8523_DACL_VOL_MASK                    0x01FF  
#define WM8523_DACL_VOL_SHIFT                        0  
#define WM8523_DACL_VOL_WIDTH                        9  

#define WM8523_DACR_VU                          0x0200  
#define WM8523_DACR_VU_MASK                     0x0200  
#define WM8523_DACR_VU_SHIFT                         9  
#define WM8523_DACR_VU_WIDTH                         1  
#define WM8523_DACR_VOL_MASK                    0x01FF  
#define WM8523_DACR_VOL_SHIFT                        0  
#define WM8523_DACR_VOL_WIDTH                        9  

#define WM8523_ZD_COUNT_MASK                    0x0003  
#define WM8523_ZD_COUNT_SHIFT                        0  
#define WM8523_ZD_COUNT_WIDTH                        2  

#endif
