/*
 * wm8741.h  --  WM8423 ASoC driver
 *
 * Copyright 2010 Wolfson Microelectronics, plc
 *
 * Author: Ian Lartey <ian@opensource.wolfsonmicro.com>
 *
 * Based on wm8753.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8741_H
#define _WM8741_H

#define WM8741_DACLLSB_ATTENUATION              0x00
#define WM8741_DACLMSB_ATTENUATION              0x01
#define WM8741_DACRLSB_ATTENUATION              0x02
#define WM8741_DACRMSB_ATTENUATION              0x03
#define WM8741_VOLUME_CONTROL                   0x04
#define WM8741_FORMAT_CONTROL                   0x05
#define WM8741_FILTER_CONTROL                   0x06
#define WM8741_MODE_CONTROL_1                   0x07
#define WM8741_MODE_CONTROL_2                   0x08
#define WM8741_RESET                            0x09
#define WM8741_ADDITIONAL_CONTROL_1             0x20

#define WM8741_REGISTER_COUNT                   11
#define WM8741_MAX_REGISTER                     0x20


#define WM8741_UPDATELL                         0x0020  
#define WM8741_UPDATELL_MASK                    0x0020  
#define WM8741_UPDATELL_SHIFT                        5  
#define WM8741_UPDATELL_WIDTH                        1  
#define WM8741_LAT_4_0_MASK                     0x001F  
#define WM8741_LAT_4_0_SHIFT                         0  
#define WM8741_LAT_4_0_WIDTH                         5  

#define WM8741_UPDATELM                         0x0020  
#define WM8741_UPDATELM_MASK                    0x0020  
#define WM8741_UPDATELM_SHIFT                        5  
#define WM8741_UPDATELM_WIDTH                        1  
#define WM8741_LAT_9_5_0_MASK                   0x001F  
#define WM8741_LAT_9_5_0_SHIFT                       0  
#define WM8741_LAT_9_5_0_WIDTH                       5  

#define WM8741_UPDATERL                         0x0020  
#define WM8741_UPDATERL_MASK                    0x0020  
#define WM8741_UPDATERL_SHIFT                        5  
#define WM8741_UPDATERL_WIDTH                        1  
#define WM8741_RAT_4_0_MASK                     0x001F  
#define WM8741_RAT_4_0_SHIFT                         0  
#define WM8741_RAT_4_0_WIDTH                         5  

#define WM8741_UPDATERM                         0x0020  
#define WM8741_UPDATERM_MASK                    0x0020  
#define WM8741_UPDATERM_SHIFT                        5  
#define WM8741_UPDATERM_WIDTH                        1  
#define WM8741_RAT_9_5_0_MASK                   0x001F  
#define WM8741_RAT_9_5_0_SHIFT                       0  
#define WM8741_RAT_9_5_0_WIDTH                       5  

#define WM8741_AMUTE                            0x0080  
#define WM8741_AMUTE_MASK                       0x0080  
#define WM8741_AMUTE_SHIFT                           7  
#define WM8741_AMUTE_WIDTH                           1  
#define WM8741_ZFLAG_MASK                       0x0060  
#define WM8741_ZFLAG_SHIFT                           5  
#define WM8741_ZFLAG_WIDTH                           2  
#define WM8741_IZD                              0x0010  
#define WM8741_IZD_MASK                         0x0010  
#define WM8741_IZD_SHIFT                             4  
#define WM8741_IZD_WIDTH                             1  
#define WM8741_SOFT                             0x0008  
#define WM8741_SOFT_MASK                        0x0008  
#define WM8741_SOFT_SHIFT                            3  
#define WM8741_SOFT_WIDTH                            1  
#define WM8741_ATC                              0x0004  
#define WM8741_ATC_MASK                         0x0004  
#define WM8741_ATC_SHIFT                             2  
#define WM8741_ATC_WIDTH                             1  
#define WM8741_ATT2DB                           0x0002  
#define WM8741_ATT2DB_MASK                      0x0002  
#define WM8741_ATT2DB_SHIFT                          1  
#define WM8741_ATT2DB_WIDTH                          1  
#define WM8741_VOL_RAMP                         0x0001  
#define WM8741_VOL_RAMP_MASK                    0x0001  
#define WM8741_VOL_RAMP_SHIFT                        0  
#define WM8741_VOL_RAMP_WIDTH                        1  

#define WM8741_PWDN                             0x0080  
#define WM8741_PWDN_MASK                        0x0080  
#define WM8741_PWDN_SHIFT                            7  
#define WM8741_PWDN_WIDTH                            1  
#define WM8741_REV                              0x0040  
#define WM8741_REV_MASK                         0x0040  
#define WM8741_REV_SHIFT                             6  
#define WM8741_REV_WIDTH                             1  
#define WM8741_BCP                              0x0020  
#define WM8741_BCP_MASK                         0x0020  
#define WM8741_BCP_SHIFT                             5  
#define WM8741_BCP_WIDTH                             1  
#define WM8741_LRP                              0x0010  
#define WM8741_LRP_MASK                         0x0010  
#define WM8741_LRP_SHIFT                             4  
#define WM8741_LRP_WIDTH                             1  
#define WM8741_FMT_MASK                         0x000C  
#define WM8741_FMT_SHIFT                             2  
#define WM8741_FMT_WIDTH                             2  
#define WM8741_IWL_MASK                         0x0003  
#define WM8741_IWL_SHIFT                             0  
#define WM8741_IWL_WIDTH                             2  

#define WM8741_ZFLAG_HI                         0x0080  
#define WM8741_ZFLAG_HI_MASK                    0x0080  
#define WM8741_ZFLAG_HI_SHIFT                        7  
#define WM8741_ZFLAG_HI_WIDTH                        1  
#define WM8741_DEEMPH_MASK                      0x0060  
#define WM8741_DEEMPH_SHIFT                          5  
#define WM8741_DEEMPH_WIDTH                          2  
#define WM8741_DSDFILT_MASK                     0x0018  
#define WM8741_DSDFILT_SHIFT                         3  
#define WM8741_DSDFILT_WIDTH                         2  
#define WM8741_FIRSEL_MASK                      0x0007  
#define WM8741_FIRSEL_SHIFT                          0  
#define WM8741_FIRSEL_WIDTH                          3  

#define WM8741_MODE8X                           0x0080  
#define WM8741_MODE8X_MASK                      0x0080  
#define WM8741_MODE8X_SHIFT                          7  
#define WM8741_MODE8X_WIDTH                          1  
#define WM8741_OSR_MASK                         0x0060  
#define WM8741_OSR_SHIFT                             5  
#define WM8741_OSR_WIDTH                             2  
#define WM8741_SR_MASK                          0x001C  
#define WM8741_SR_SHIFT                              2  
#define WM8741_SR_WIDTH                              3  
#define WM8741_MODESEL_MASK                     0x0003  
#define WM8741_MODESEL_SHIFT                         0  
#define WM8741_MODESEL_WIDTH                         2  

#define WM8741_DSD_GAIN                         0x0040  
#define WM8741_DSD_GAIN_MASK                    0x0040  
#define WM8741_DSD_GAIN_SHIFT                        6  
#define WM8741_DSD_GAIN_WIDTH                        1  
#define WM8741_SDOUT                            0x0020  
#define WM8741_SDOUT_MASK                       0x0020  
#define WM8741_SDOUT_SHIFT                           5  
#define WM8741_SDOUT_WIDTH                           1  
#define WM8741_DOUT                             0x0010  
#define WM8741_DOUT_MASK                        0x0010  
#define WM8741_DOUT_SHIFT                            4  
#define WM8741_DOUT_WIDTH                            1  
#define WM8741_DIFF_MASK                        0x000C  
#define WM8741_DIFF_SHIFT                            2  
#define WM8741_DIFF_WIDTH                            2  
#define WM8741_DITHER_MASK                      0x0003  
#define WM8741_DITHER_SHIFT                          0  
#define WM8741_DITHER_WIDTH                          2  

#define WM8741_DSD_LEVEL                        0x0002  
#define WM8741_DSD_LEVEL_MASK                   0x0002  
#define WM8741_DSD_LEVEL_SHIFT                       1  
#define WM8741_DSD_LEVEL_WIDTH                       1  
#define WM8741_DSD_NO_NOTCH                     0x0001  
#define WM8741_DSD_NO_NOTCH_MASK                0x0001  
#define WM8741_DSD_NO_NOTCH_SHIFT                    0  
#define WM8741_DSD_NO_NOTCH_WIDTH                    1  

#define  WM8741_SYSCLK 0

#endif
