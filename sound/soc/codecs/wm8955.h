/*
 * wm8955.h  --  WM8904 ASoC driver
 *
 * Copyright 2009 Wolfson Microelectronics, plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8955_H
#define _WM8955_H

#define WM8955_CLK_MCLK 1

#define WM8955_LOUT1_VOLUME                     0x02
#define WM8955_ROUT1_VOLUME                     0x03
#define WM8955_DAC_CONTROL                      0x05
#define WM8955_AUDIO_INTERFACE                  0x07
#define WM8955_SAMPLE_RATE                      0x08
#define WM8955_LEFT_DAC_VOLUME                  0x0A
#define WM8955_RIGHT_DAC_VOLUME                 0x0B
#define WM8955_BASS_CONTROL                     0x0C
#define WM8955_TREBLE_CONTROL                   0x0D
#define WM8955_RESET                            0x0F
#define WM8955_ADDITIONAL_CONTROL_1             0x17
#define WM8955_ADDITIONAL_CONTROL_2             0x18
#define WM8955_POWER_MANAGEMENT_1               0x19
#define WM8955_POWER_MANAGEMENT_2               0x1A
#define WM8955_ADDITIONAL_CONTROL_3             0x1B
#define WM8955_LEFT_OUT_MIX_1                   0x22
#define WM8955_LEFT_OUT_MIX_2                   0x23
#define WM8955_RIGHT_OUT_MIX_1                  0x24
#define WM8955_RIGHT_OUT_MIX_2                  0x25
#define WM8955_MONO_OUT_MIX_1                   0x26
#define WM8955_MONO_OUT_MIX_2                   0x27
#define WM8955_LOUT2_VOLUME                     0x28
#define WM8955_ROUT2_VOLUME                     0x29
#define WM8955_MONOOUT_VOLUME                   0x2A
#define WM8955_CLOCKING_PLL                     0x2B
#define WM8955_PLL_CONTROL_1                    0x2C
#define WM8955_PLL_CONTROL_2                    0x2D
#define WM8955_PLL_CONTROL_3                    0x2E
#define WM8955_PLL_CONTROL_4                    0x3B

#define WM8955_REGISTER_COUNT                   29
#define WM8955_MAX_REGISTER                     0x3B


#define WM8955_LO1VU                            0x0100  
#define WM8955_LO1VU_MASK                       0x0100  
#define WM8955_LO1VU_SHIFT                           8  
#define WM8955_LO1VU_WIDTH                           1  
#define WM8955_LO1ZC                            0x0080  
#define WM8955_LO1ZC_MASK                       0x0080  
#define WM8955_LO1ZC_SHIFT                           7  
#define WM8955_LO1ZC_WIDTH                           1  
#define WM8955_LOUTVOL_MASK                     0x007F  
#define WM8955_LOUTVOL_SHIFT                         0  
#define WM8955_LOUTVOL_WIDTH                         7  

#define WM8955_RO1VU                            0x0100  
#define WM8955_RO1VU_MASK                       0x0100  
#define WM8955_RO1VU_SHIFT                           8  
#define WM8955_RO1VU_WIDTH                           1  
#define WM8955_RO1ZC                            0x0080  
#define WM8955_RO1ZC_MASK                       0x0080  
#define WM8955_RO1ZC_SHIFT                           7  
#define WM8955_RO1ZC_WIDTH                           1  
#define WM8955_ROUTVOL_MASK                     0x007F  
#define WM8955_ROUTVOL_SHIFT                         0  
#define WM8955_ROUTVOL_WIDTH                         7  

#define WM8955_DAT                              0x0080  
#define WM8955_DAT_MASK                         0x0080  
#define WM8955_DAT_SHIFT                             7  
#define WM8955_DAT_WIDTH                             1  
#define WM8955_DACMU                            0x0008  
#define WM8955_DACMU_MASK                       0x0008  
#define WM8955_DACMU_SHIFT                           3  
#define WM8955_DACMU_WIDTH                           1  
#define WM8955_DEEMPH_MASK                      0x0006  
#define WM8955_DEEMPH_SHIFT                          1  
#define WM8955_DEEMPH_WIDTH                          2  

#define WM8955_BCLKINV                          0x0080  
#define WM8955_BCLKINV_MASK                     0x0080  
#define WM8955_BCLKINV_SHIFT                         7  
#define WM8955_BCLKINV_WIDTH                         1  
#define WM8955_MS                               0x0040  
#define WM8955_MS_MASK                          0x0040  
#define WM8955_MS_SHIFT                              6  
#define WM8955_MS_WIDTH                              1  
#define WM8955_LRSWAP                           0x0020  
#define WM8955_LRSWAP_MASK                      0x0020  
#define WM8955_LRSWAP_SHIFT                          5  
#define WM8955_LRSWAP_WIDTH                          1  
#define WM8955_LRP                              0x0010  
#define WM8955_LRP_MASK                         0x0010  
#define WM8955_LRP_SHIFT                             4  
#define WM8955_LRP_WIDTH                             1  
#define WM8955_WL_MASK                          0x000C  
#define WM8955_WL_SHIFT                              2  
#define WM8955_WL_WIDTH                              2  
#define WM8955_FORMAT_MASK                      0x0003  
#define WM8955_FORMAT_SHIFT                          0  
#define WM8955_FORMAT_WIDTH                          2  

#define WM8955_BCLKDIV2                         0x0080  
#define WM8955_BCLKDIV2_MASK                    0x0080  
#define WM8955_BCLKDIV2_SHIFT                        7  
#define WM8955_BCLKDIV2_WIDTH                        1  
#define WM8955_MCLKDIV2                         0x0040  
#define WM8955_MCLKDIV2_MASK                    0x0040  
#define WM8955_MCLKDIV2_SHIFT                        6  
#define WM8955_MCLKDIV2_WIDTH                        1  
#define WM8955_SR_MASK                          0x003E  
#define WM8955_SR_SHIFT                              1  
#define WM8955_SR_WIDTH                              5  
#define WM8955_USB                              0x0001  
#define WM8955_USB_MASK                         0x0001  
#define WM8955_USB_SHIFT                             0  
#define WM8955_USB_WIDTH                             1  

#define WM8955_LDVU                             0x0100  
#define WM8955_LDVU_MASK                        0x0100  
#define WM8955_LDVU_SHIFT                            8  
#define WM8955_LDVU_WIDTH                            1  
#define WM8955_LDACVOL_MASK                     0x00FF  
#define WM8955_LDACVOL_SHIFT                         0  
#define WM8955_LDACVOL_WIDTH                         8  

#define WM8955_RDVU                             0x0100  
#define WM8955_RDVU_MASK                        0x0100  
#define WM8955_RDVU_SHIFT                            8  
#define WM8955_RDVU_WIDTH                            1  
#define WM8955_RDACVOL_MASK                     0x00FF  
#define WM8955_RDACVOL_SHIFT                         0  
#define WM8955_RDACVOL_WIDTH                         8  

#define WM8955_BB                               0x0080  
#define WM8955_BB_MASK                          0x0080  
#define WM8955_BB_SHIFT                              7  
#define WM8955_BB_WIDTH                              1  
#define WM8955_BC                               0x0040  
#define WM8955_BC_MASK                          0x0040  
#define WM8955_BC_SHIFT                              6  
#define WM8955_BC_WIDTH                              1  
#define WM8955_BASS_MASK                        0x000F  
#define WM8955_BASS_SHIFT                            0  
#define WM8955_BASS_WIDTH                            4  

#define WM8955_TC                               0x0040  
#define WM8955_TC_MASK                          0x0040  
#define WM8955_TC_SHIFT                              6  
#define WM8955_TC_WIDTH                              1  
#define WM8955_TRBL_MASK                        0x000F  
#define WM8955_TRBL_SHIFT                            0  
#define WM8955_TRBL_WIDTH                            4  

#define WM8955_RESET_MASK                       0x01FF  
#define WM8955_RESET_SHIFT                           0  
#define WM8955_RESET_WIDTH                           9  

#define WM8955_TSDEN                            0x0100  
#define WM8955_TSDEN_MASK                       0x0100  
#define WM8955_TSDEN_SHIFT                           8  
#define WM8955_TSDEN_WIDTH                           1  
#define WM8955_VSEL_MASK                        0x00C0  
#define WM8955_VSEL_SHIFT                            6  
#define WM8955_VSEL_WIDTH                            2  
#define WM8955_DMONOMIX_MASK                    0x0030  
#define WM8955_DMONOMIX_SHIFT                        4  
#define WM8955_DMONOMIX_WIDTH                        2  
#define WM8955_DACINV                           0x0002  
#define WM8955_DACINV_MASK                      0x0002  
#define WM8955_DACINV_SHIFT                          1  
#define WM8955_DACINV_WIDTH                          1  
#define WM8955_TOEN                             0x0001  
#define WM8955_TOEN_MASK                        0x0001  
#define WM8955_TOEN_SHIFT                            0  
#define WM8955_TOEN_WIDTH                            1  

#define WM8955_OUT3SW_MASK                      0x0180  
#define WM8955_OUT3SW_SHIFT                          7  
#define WM8955_OUT3SW_WIDTH                          2  
#define WM8955_ROUT2INV                         0x0010  
#define WM8955_ROUT2INV_MASK                    0x0010  
#define WM8955_ROUT2INV_SHIFT                        4  
#define WM8955_ROUT2INV_WIDTH                        1  
#define WM8955_DACOSR                           0x0001  
#define WM8955_DACOSR_MASK                      0x0001  
#define WM8955_DACOSR_SHIFT                          0  
#define WM8955_DACOSR_WIDTH                          1  

#define WM8955_VMIDSEL_MASK                     0x0180  
#define WM8955_VMIDSEL_SHIFT                         7  
#define WM8955_VMIDSEL_WIDTH                         2  
#define WM8955_VREF                             0x0040  
#define WM8955_VREF_MASK                        0x0040  
#define WM8955_VREF_SHIFT                            6  
#define WM8955_VREF_WIDTH                            1  
#define WM8955_DIGENB                           0x0001  
#define WM8955_DIGENB_MASK                      0x0001  
#define WM8955_DIGENB_SHIFT                          0  
#define WM8955_DIGENB_WIDTH                          1  

#define WM8955_DACL                             0x0100  
#define WM8955_DACL_MASK                        0x0100  
#define WM8955_DACL_SHIFT                            8  
#define WM8955_DACL_WIDTH                            1  
#define WM8955_DACR                             0x0080  
#define WM8955_DACR_MASK                        0x0080  
#define WM8955_DACR_SHIFT                            7  
#define WM8955_DACR_WIDTH                            1  
#define WM8955_LOUT1                            0x0040  
#define WM8955_LOUT1_MASK                       0x0040  
#define WM8955_LOUT1_SHIFT                           6  
#define WM8955_LOUT1_WIDTH                           1  
#define WM8955_ROUT1                            0x0020  
#define WM8955_ROUT1_MASK                       0x0020  
#define WM8955_ROUT1_SHIFT                           5  
#define WM8955_ROUT1_WIDTH                           1  
#define WM8955_LOUT2                            0x0010  
#define WM8955_LOUT2_MASK                       0x0010  
#define WM8955_LOUT2_SHIFT                           4  
#define WM8955_LOUT2_WIDTH                           1  
#define WM8955_ROUT2                            0x0008  
#define WM8955_ROUT2_MASK                       0x0008  
#define WM8955_ROUT2_SHIFT                           3  
#define WM8955_ROUT2_WIDTH                           1  
#define WM8955_MONO                             0x0004  
#define WM8955_MONO_MASK                        0x0004  
#define WM8955_MONO_SHIFT                            2  
#define WM8955_MONO_WIDTH                            1  
#define WM8955_OUT3                             0x0002  
#define WM8955_OUT3_MASK                        0x0002  
#define WM8955_OUT3_SHIFT                            1  
#define WM8955_OUT3_WIDTH                            1  

#define WM8955_VROI                             0x0040  
#define WM8955_VROI_MASK                        0x0040  
#define WM8955_VROI_SHIFT                            6  
#define WM8955_VROI_WIDTH                            1  

#define WM8955_LD2LO                            0x0100  
#define WM8955_LD2LO_MASK                       0x0100  
#define WM8955_LD2LO_SHIFT                           8  
#define WM8955_LD2LO_WIDTH                           1  
#define WM8955_LI2LO                            0x0080  
#define WM8955_LI2LO_MASK                       0x0080  
#define WM8955_LI2LO_SHIFT                           7  
#define WM8955_LI2LO_WIDTH                           1  
#define WM8955_LI2LOVOL_MASK                    0x0070  
#define WM8955_LI2LOVOL_SHIFT                        4  
#define WM8955_LI2LOVOL_WIDTH                        3  

#define WM8955_RD2LO                            0x0100  
#define WM8955_RD2LO_MASK                       0x0100  
#define WM8955_RD2LO_SHIFT                           8  
#define WM8955_RD2LO_WIDTH                           1  
#define WM8955_RI2LO                            0x0080  
#define WM8955_RI2LO_MASK                       0x0080  
#define WM8955_RI2LO_SHIFT                           7  
#define WM8955_RI2LO_WIDTH                           1  
#define WM8955_RI2LOVOL_MASK                    0x0070  
#define WM8955_RI2LOVOL_SHIFT                        4  
#define WM8955_RI2LOVOL_WIDTH                        3  

#define WM8955_LD2RO                            0x0100  
#define WM8955_LD2RO_MASK                       0x0100  
#define WM8955_LD2RO_SHIFT                           8  
#define WM8955_LD2RO_WIDTH                           1  
#define WM8955_LI2RO                            0x0080  
#define WM8955_LI2RO_MASK                       0x0080  
#define WM8955_LI2RO_SHIFT                           7  
#define WM8955_LI2RO_WIDTH                           1  
#define WM8955_LI2ROVOL_MASK                    0x0070  
#define WM8955_LI2ROVOL_SHIFT                        4  
#define WM8955_LI2ROVOL_WIDTH                        3  

#define WM8955_RD2RO                            0x0100  
#define WM8955_RD2RO_MASK                       0x0100  
#define WM8955_RD2RO_SHIFT                           8  
#define WM8955_RD2RO_WIDTH                           1  
#define WM8955_RI2RO                            0x0080  
#define WM8955_RI2RO_MASK                       0x0080  
#define WM8955_RI2RO_SHIFT                           7  
#define WM8955_RI2RO_WIDTH                           1  
#define WM8955_RI2ROVOL_MASK                    0x0070  
#define WM8955_RI2ROVOL_SHIFT                        4  
#define WM8955_RI2ROVOL_WIDTH                        3  

#define WM8955_LD2MO                            0x0100  
#define WM8955_LD2MO_MASK                       0x0100  
#define WM8955_LD2MO_SHIFT                           8  
#define WM8955_LD2MO_WIDTH                           1  
#define WM8955_LI2MO                            0x0080  
#define WM8955_LI2MO_MASK                       0x0080  
#define WM8955_LI2MO_SHIFT                           7  
#define WM8955_LI2MO_WIDTH                           1  
#define WM8955_LI2MOVOL_MASK                    0x0070  
#define WM8955_LI2MOVOL_SHIFT                        4  
#define WM8955_LI2MOVOL_WIDTH                        3  
#define WM8955_DMEN                             0x0001  
#define WM8955_DMEN_MASK                        0x0001  
#define WM8955_DMEN_SHIFT                            0  
#define WM8955_DMEN_WIDTH                            1  

#define WM8955_RD2MO                            0x0100  
#define WM8955_RD2MO_MASK                       0x0100  
#define WM8955_RD2MO_SHIFT                           8  
#define WM8955_RD2MO_WIDTH                           1  
#define WM8955_RI2MO                            0x0080  
#define WM8955_RI2MO_MASK                       0x0080  
#define WM8955_RI2MO_SHIFT                           7  
#define WM8955_RI2MO_WIDTH                           1  
#define WM8955_RI2MOVOL_MASK                    0x0070  
#define WM8955_RI2MOVOL_SHIFT                        4  
#define WM8955_RI2MOVOL_WIDTH                        3  

#define WM8955_LO2VU                            0x0100  
#define WM8955_LO2VU_MASK                       0x0100  
#define WM8955_LO2VU_SHIFT                           8  
#define WM8955_LO2VU_WIDTH                           1  
#define WM8955_LO2ZC                            0x0080  
#define WM8955_LO2ZC_MASK                       0x0080  
#define WM8955_LO2ZC_SHIFT                           7  
#define WM8955_LO2ZC_WIDTH                           1  
#define WM8955_LOUT2VOL_MASK                    0x007F  
#define WM8955_LOUT2VOL_SHIFT                        0  
#define WM8955_LOUT2VOL_WIDTH                        7  

#define WM8955_RO2VU                            0x0100  
#define WM8955_RO2VU_MASK                       0x0100  
#define WM8955_RO2VU_SHIFT                           8  
#define WM8955_RO2VU_WIDTH                           1  
#define WM8955_RO2ZC                            0x0080  
#define WM8955_RO2ZC_MASK                       0x0080  
#define WM8955_RO2ZC_SHIFT                           7  
#define WM8955_RO2ZC_WIDTH                           1  
#define WM8955_ROUT2VOL_MASK                    0x007F  
#define WM8955_ROUT2VOL_SHIFT                        0  
#define WM8955_ROUT2VOL_WIDTH                        7  

#define WM8955_MOZC                             0x0080  
#define WM8955_MOZC_MASK                        0x0080  
#define WM8955_MOZC_SHIFT                            7  
#define WM8955_MOZC_WIDTH                            1  
#define WM8955_MOUTVOL_MASK                     0x007F  
#define WM8955_MOUTVOL_SHIFT                         0  
#define WM8955_MOUTVOL_WIDTH                         7  

#define WM8955_MCLKSEL                          0x0100  
#define WM8955_MCLKSEL_MASK                     0x0100  
#define WM8955_MCLKSEL_SHIFT                         8  
#define WM8955_MCLKSEL_WIDTH                         1  
#define WM8955_PLLOUTDIV2                       0x0020  
#define WM8955_PLLOUTDIV2_MASK                  0x0020  
#define WM8955_PLLOUTDIV2_SHIFT                      5  
#define WM8955_PLLOUTDIV2_WIDTH                      1  
#define WM8955_PLL_RB                           0x0010  
#define WM8955_PLL_RB_MASK                      0x0010  
#define WM8955_PLL_RB_SHIFT                          4  
#define WM8955_PLL_RB_WIDTH                          1  
#define WM8955_PLLEN                            0x0008  
#define WM8955_PLLEN_MASK                       0x0008  
#define WM8955_PLLEN_SHIFT                           3  
#define WM8955_PLLEN_WIDTH                           1  

#define WM8955_N_MASK                           0x01E0  
#define WM8955_N_SHIFT                               5  
#define WM8955_N_WIDTH                               4  
#define WM8955_K_21_18_MASK                     0x000F  
#define WM8955_K_21_18_SHIFT                         0  
#define WM8955_K_21_18_WIDTH                         4  

#define WM8955_K_17_9_MASK                      0x01FF  
#define WM8955_K_17_9_SHIFT                          0  
#define WM8955_K_17_9_WIDTH                          9  

#define WM8955_K_8_0_MASK                       0x01FF  
#define WM8955_K_8_0_SHIFT                           0  
#define WM8955_K_8_0_WIDTH                           9  

#define WM8955_KEN                              0x0080  
#define WM8955_KEN_MASK                         0x0080  
#define WM8955_KEN_SHIFT                             7  
#define WM8955_KEN_WIDTH                             1  

#endif
