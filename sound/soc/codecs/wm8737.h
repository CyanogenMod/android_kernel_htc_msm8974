#ifndef _WM8737_H
#define _WM8737_H

/*
 * wm8737.c  --  WM8523 ALSA SoC Audio driver
 *
 * Copyright 2010 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define WM8737_LEFT_PGA_VOLUME                  0x00
#define WM8737_RIGHT_PGA_VOLUME                 0x01
#define WM8737_AUDIO_PATH_L                     0x02
#define WM8737_AUDIO_PATH_R                     0x03
#define WM8737_3D_ENHANCE                       0x04
#define WM8737_ADC_CONTROL                      0x05
#define WM8737_POWER_MANAGEMENT                 0x06
#define WM8737_AUDIO_FORMAT                     0x07
#define WM8737_CLOCKING                         0x08
#define WM8737_MIC_PREAMP_CONTROL               0x09
#define WM8737_MISC_BIAS_CONTROL                0x0A
#define WM8737_NOISE_GATE                       0x0B
#define WM8737_ALC1                             0x0C
#define WM8737_ALC2                             0x0D
#define WM8737_ALC3                             0x0E
#define WM8737_RESET                            0x0F

#define WM8737_REGISTER_COUNT                   16
#define WM8737_MAX_REGISTER                     0x0F


#define WM8737_LVU                              0x0100  
#define WM8737_LVU_MASK                         0x0100  
#define WM8737_LVU_SHIFT                             8  
#define WM8737_LVU_WIDTH                             1  
#define WM8737_LINVOL_MASK                      0x00FF  
#define WM8737_LINVOL_SHIFT                          0  
#define WM8737_LINVOL_WIDTH                          8  

#define WM8737_RVU                              0x0100  
#define WM8737_RVU_MASK                         0x0100  
#define WM8737_RVU_SHIFT                             8  
#define WM8737_RVU_WIDTH                             1  
#define WM8737_RINVOL_MASK                      0x00FF  
#define WM8737_RINVOL_SHIFT                          0  
#define WM8737_RINVOL_WIDTH                          8  

#define WM8737_LINSEL_MASK                      0x0180  
#define WM8737_LINSEL_SHIFT                          7  
#define WM8737_LINSEL_WIDTH                          2  
#define WM8737_LMICBOOST_MASK                   0x0060  
#define WM8737_LMICBOOST_SHIFT                       5  
#define WM8737_LMICBOOST_WIDTH                       2  
#define WM8737_LMBE                             0x0010  
#define WM8737_LMBE_MASK                        0x0010  
#define WM8737_LMBE_SHIFT                            4  
#define WM8737_LMBE_WIDTH                            1  
#define WM8737_LMZC                             0x0008  
#define WM8737_LMZC_MASK                        0x0008  
#define WM8737_LMZC_SHIFT                            3  
#define WM8737_LMZC_WIDTH                            1  
#define WM8737_LPZC                             0x0004  
#define WM8737_LPZC_MASK                        0x0004  
#define WM8737_LPZC_SHIFT                            2  
#define WM8737_LPZC_WIDTH                            1  
#define WM8737_LZCTO_MASK                       0x0003  
#define WM8737_LZCTO_SHIFT                           0  
#define WM8737_LZCTO_WIDTH                           2  

#define WM8737_RINSEL_MASK                      0x0180  
#define WM8737_RINSEL_SHIFT                          7  
#define WM8737_RINSEL_WIDTH                          2  
#define WM8737_RMICBOOST_MASK                   0x0060  
#define WM8737_RMICBOOST_SHIFT                       5  
#define WM8737_RMICBOOST_WIDTH                       2  
#define WM8737_RMBE                             0x0010  
#define WM8737_RMBE_MASK                        0x0010  
#define WM8737_RMBE_SHIFT                            4  
#define WM8737_RMBE_WIDTH                            1  
#define WM8737_RMZC                             0x0008  
#define WM8737_RMZC_MASK                        0x0008  
#define WM8737_RMZC_SHIFT                            3  
#define WM8737_RMZC_WIDTH                            1  
#define WM8737_RPZC                             0x0004  
#define WM8737_RPZC_MASK                        0x0004  
#define WM8737_RPZC_SHIFT                            2  
#define WM8737_RPZC_WIDTH                            1  
#define WM8737_RZCTO_MASK                       0x0003  
#define WM8737_RZCTO_SHIFT                           0  
#define WM8737_RZCTO_WIDTH                           2  

#define WM8737_DIV2                             0x0080  
#define WM8737_DIV2_MASK                        0x0080  
#define WM8737_DIV2_SHIFT                            7  
#define WM8737_DIV2_WIDTH                            1  
#define WM8737_3DLC                             0x0040  
#define WM8737_3DLC_MASK                        0x0040  
#define WM8737_3DLC_SHIFT                            6  
#define WM8737_3DLC_WIDTH                            1  
#define WM8737_3DUC                             0x0020  
#define WM8737_3DUC_MASK                        0x0020  
#define WM8737_3DUC_SHIFT                            5  
#define WM8737_3DUC_WIDTH                            1  
#define WM8737_3DDEPTH_MASK                     0x001E  
#define WM8737_3DDEPTH_SHIFT                         1  
#define WM8737_3DDEPTH_WIDTH                         4  
#define WM8737_3DE                              0x0001  
#define WM8737_3DE_MASK                         0x0001  
#define WM8737_3DE_SHIFT                             0  
#define WM8737_3DE_WIDTH                             1  

#define WM8737_MONOMIX_MASK                     0x0180  
#define WM8737_MONOMIX_SHIFT                         7  
#define WM8737_MONOMIX_WIDTH                         2  
#define WM8737_POLARITY_MASK                    0x0060  
#define WM8737_POLARITY_SHIFT                        5  
#define WM8737_POLARITY_WIDTH                        2  
#define WM8737_HPOR                             0x0010  
#define WM8737_HPOR_MASK                        0x0010  
#define WM8737_HPOR_SHIFT                            4  
#define WM8737_HPOR_WIDTH                            1  
#define WM8737_LP                               0x0004  
#define WM8737_LP_MASK                          0x0004  
#define WM8737_LP_SHIFT                              2  
#define WM8737_LP_WIDTH                              1  
#define WM8737_MONOUT                           0x0002  
#define WM8737_MONOUT_MASK                      0x0002  
#define WM8737_MONOUT_SHIFT                          1  
#define WM8737_MONOUT_WIDTH                          1  
#define WM8737_ADCHPD                           0x0001  
#define WM8737_ADCHPD_MASK                      0x0001  
#define WM8737_ADCHPD_SHIFT                          0  
#define WM8737_ADCHPD_WIDTH                          1  

#define WM8737_VMID                             0x0100  
#define WM8737_VMID_MASK                        0x0100  
#define WM8737_VMID_SHIFT                            8  
#define WM8737_VMID_WIDTH                            1  
#define WM8737_VREF                             0x0080  
#define WM8737_VREF_MASK                        0x0080  
#define WM8737_VREF_SHIFT                            7  
#define WM8737_VREF_WIDTH                            1  
#define WM8737_AI                               0x0040  
#define WM8737_AI_MASK                          0x0040  
#define WM8737_AI_SHIFT                              6  
#define WM8737_AI_WIDTH                              1  
#define WM8737_PGL                              0x0020  
#define WM8737_PGL_MASK                         0x0020  
#define WM8737_PGL_SHIFT                             5  
#define WM8737_PGL_WIDTH                             1  
#define WM8737_PGR                              0x0010  
#define WM8737_PGR_MASK                         0x0010  
#define WM8737_PGR_SHIFT                             4  
#define WM8737_PGR_WIDTH                             1  
#define WM8737_ADL                              0x0008  
#define WM8737_ADL_MASK                         0x0008  
#define WM8737_ADL_SHIFT                             3  
#define WM8737_ADL_WIDTH                             1  
#define WM8737_ADR                              0x0004  
#define WM8737_ADR_MASK                         0x0004  
#define WM8737_ADR_SHIFT                             2  
#define WM8737_ADR_WIDTH                             1  
#define WM8737_MICBIAS_MASK                     0x0003  
#define WM8737_MICBIAS_SHIFT                         0  
#define WM8737_MICBIAS_WIDTH                         2  

#define WM8737_SDODIS                           0x0080  
#define WM8737_SDODIS_MASK                      0x0080  
#define WM8737_SDODIS_SHIFT                          7  
#define WM8737_SDODIS_WIDTH                          1  
#define WM8737_MS                               0x0040  
#define WM8737_MS_MASK                          0x0040  
#define WM8737_MS_SHIFT                              6  
#define WM8737_MS_WIDTH                              1  
#define WM8737_LRP                              0x0010  
#define WM8737_LRP_MASK                         0x0010  
#define WM8737_LRP_SHIFT                             4  
#define WM8737_LRP_WIDTH                             1  
#define WM8737_WL_MASK                          0x000C  
#define WM8737_WL_SHIFT                              2  
#define WM8737_WL_WIDTH                              2  
#define WM8737_FORMAT_MASK                      0x0003  
#define WM8737_FORMAT_SHIFT                          0  
#define WM8737_FORMAT_WIDTH                          2  

#define WM8737_AUTODETECT                       0x0080  
#define WM8737_AUTODETECT_MASK                  0x0080  
#define WM8737_AUTODETECT_SHIFT                      7  
#define WM8737_AUTODETECT_WIDTH                      1  
#define WM8737_CLKDIV2                          0x0040  
#define WM8737_CLKDIV2_MASK                     0x0040  
#define WM8737_CLKDIV2_SHIFT                         6  
#define WM8737_CLKDIV2_WIDTH                         1  
#define WM8737_SR_MASK                          0x003E  
#define WM8737_SR_SHIFT                              1  
#define WM8737_SR_WIDTH                              5  
#define WM8737_USB_MODE                         0x0001  
#define WM8737_USB_MODE_MASK                    0x0001  
#define WM8737_USB_MODE_SHIFT                        0  
#define WM8737_USB_MODE_WIDTH                        1  

#define WM8737_RBYPEN                           0x0008  
#define WM8737_RBYPEN_MASK                      0x0008  
#define WM8737_RBYPEN_SHIFT                          3  
#define WM8737_RBYPEN_WIDTH                          1  
#define WM8737_LBYPEN                           0x0004  
#define WM8737_LBYPEN_MASK                      0x0004  
#define WM8737_LBYPEN_SHIFT                          2  
#define WM8737_LBYPEN_WIDTH                          1  
#define WM8737_MBCTRL_MASK                      0x0003  
#define WM8737_MBCTRL_SHIFT                          0  
#define WM8737_MBCTRL_WIDTH                          2  

#define WM8737_VMIDSEL_MASK                     0x000C  
#define WM8737_VMIDSEL_SHIFT                         2  
#define WM8737_VMIDSEL_WIDTH                         2  
#define WM8737_LINPUT1_DC_BIAS_ENABLE           0x0002  
#define WM8737_LINPUT1_DC_BIAS_ENABLE_MASK      0x0002  
#define WM8737_LINPUT1_DC_BIAS_ENABLE_SHIFT          1  
#define WM8737_LINPUT1_DC_BIAS_ENABLE_WIDTH          1  
#define WM8737_RINPUT1_DC_BIAS_ENABLE           0x0001  
#define WM8737_RINPUT1_DC_BIAS_ENABLE_MASK      0x0001  
#define WM8737_RINPUT1_DC_BIAS_ENABLE_SHIFT          0  
#define WM8737_RINPUT1_DC_BIAS_ENABLE_WIDTH          1  

#define WM8737_NGTH_MASK                        0x001C  
#define WM8737_NGTH_SHIFT                            2  
#define WM8737_NGTH_WIDTH                            3  
#define WM8737_NGAT                             0x0001  
#define WM8737_NGAT_MASK                        0x0001  
#define WM8737_NGAT_SHIFT                            0  
#define WM8737_NGAT_WIDTH                            1  

#define WM8737_ALCSEL_MASK                      0x0180  
#define WM8737_ALCSEL_SHIFT                          7  
#define WM8737_ALCSEL_WIDTH                          2  
#define WM8737_MAX_GAIN_MASK                    0x0070  
#define WM8737_MAX_GAIN_SHIFT                        4  
#define WM8737_MAX_GAIN_WIDTH                        3  
#define WM8737_ALCL_MASK                        0x000F  
#define WM8737_ALCL_SHIFT                            0  
#define WM8737_ALCL_WIDTH                            4  

#define WM8737_ALCZCE                           0x0010  
#define WM8737_ALCZCE_MASK                      0x0010  
#define WM8737_ALCZCE_SHIFT                          4  
#define WM8737_ALCZCE_WIDTH                          1  
#define WM8737_HLD_MASK                         0x000F  
#define WM8737_HLD_SHIFT                             0  
#define WM8737_HLD_WIDTH                             4  

#define WM8737_DCY_MASK                         0x00F0  
#define WM8737_DCY_SHIFT                             4  
#define WM8737_DCY_WIDTH                             4  
#define WM8737_ATK_MASK                         0x000F  
#define WM8737_ATK_SHIFT                             0  
#define WM8737_ATK_WIDTH                             4  

#define WM8737_RESET_MASK                       0x01FF  
#define WM8737_RESET_SHIFT                           0  
#define WM8737_RESET_WIDTH                           9  

#endif
