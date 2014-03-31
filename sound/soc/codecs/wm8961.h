/*
 * wm8961.h  --  WM8961 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8961_H
#define _WM8961_H

#include <sound/soc.h>

#define WM8961_BCLK  1
#define WM8961_LRCLK 2

#define WM8961_BCLK_DIV_1    0
#define WM8961_BCLK_DIV_1_5  1
#define WM8961_BCLK_DIV_2    2
#define WM8961_BCLK_DIV_3    3
#define WM8961_BCLK_DIV_4    4
#define WM8961_BCLK_DIV_5_5  5
#define WM8961_BCLK_DIV_6    6
#define WM8961_BCLK_DIV_8    7
#define WM8961_BCLK_DIV_11   8
#define WM8961_BCLK_DIV_12   9
#define WM8961_BCLK_DIV_16  10
#define WM8961_BCLK_DIV_24  11
#define WM8961_BCLK_DIV_32  13


#define WM8961_LEFT_INPUT_VOLUME                0x00
#define WM8961_RIGHT_INPUT_VOLUME               0x01
#define WM8961_LOUT1_VOLUME                     0x02
#define WM8961_ROUT1_VOLUME                     0x03
#define WM8961_CLOCKING1                        0x04
#define WM8961_ADC_DAC_CONTROL_1                0x05
#define WM8961_ADC_DAC_CONTROL_2                0x06
#define WM8961_AUDIO_INTERFACE_0                0x07
#define WM8961_CLOCKING2                        0x08
#define WM8961_AUDIO_INTERFACE_1                0x09
#define WM8961_LEFT_DAC_VOLUME                  0x0A
#define WM8961_RIGHT_DAC_VOLUME                 0x0B
#define WM8961_AUDIO_INTERFACE_2                0x0E
#define WM8961_SOFTWARE_RESET                   0x0F
#define WM8961_ALC1                             0x11
#define WM8961_ALC2                             0x12
#define WM8961_ALC3                             0x13
#define WM8961_NOISE_GATE                       0x14
#define WM8961_LEFT_ADC_VOLUME                  0x15
#define WM8961_RIGHT_ADC_VOLUME                 0x16
#define WM8961_ADDITIONAL_CONTROL_1             0x17
#define WM8961_ADDITIONAL_CONTROL_2             0x18
#define WM8961_PWR_MGMT_1                       0x19
#define WM8961_PWR_MGMT_2                       0x1A
#define WM8961_ADDITIONAL_CONTROL_3             0x1B
#define WM8961_ANTI_POP                         0x1C
#define WM8961_CLOCKING_3                       0x1E
#define WM8961_ADCL_SIGNAL_PATH                 0x20
#define WM8961_ADCR_SIGNAL_PATH                 0x21
#define WM8961_LOUT2_VOLUME                     0x28
#define WM8961_ROUT2_VOLUME                     0x29
#define WM8961_PWR_MGMT_3                       0x2F
#define WM8961_ADDITIONAL_CONTROL_4             0x30
#define WM8961_CLASS_D_CONTROL_1                0x31
#define WM8961_CLASS_D_CONTROL_2                0x33
#define WM8961_CLOCKING_4                       0x38
#define WM8961_DSP_SIDETONE_0                   0x39
#define WM8961_DSP_SIDETONE_1                   0x3A
#define WM8961_DC_SERVO_0                       0x3C
#define WM8961_DC_SERVO_1                       0x3D
#define WM8961_DC_SERVO_3                       0x3F
#define WM8961_DC_SERVO_5                       0x41
#define WM8961_ANALOGUE_PGA_BIAS                0x44
#define WM8961_ANALOGUE_HP_0                    0x45
#define WM8961_ANALOGUE_HP_2                    0x47
#define WM8961_CHARGE_PUMP_1                    0x48
#define WM8961_CHARGE_PUMP_B                    0x52
#define WM8961_WRITE_SEQUENCER_1                0x57
#define WM8961_WRITE_SEQUENCER_2                0x58
#define WM8961_WRITE_SEQUENCER_3                0x59
#define WM8961_WRITE_SEQUENCER_4                0x5A
#define WM8961_WRITE_SEQUENCER_5                0x5B
#define WM8961_WRITE_SEQUENCER_6                0x5C
#define WM8961_WRITE_SEQUENCER_7                0x5D
#define WM8961_GENERAL_TEST_1                   0xFC



#define WM8961_IPVU                             0x0100  
#define WM8961_IPVU_MASK                        0x0100  
#define WM8961_IPVU_SHIFT                            8  
#define WM8961_IPVU_WIDTH                            1  
#define WM8961_LINMUTE                          0x0080  
#define WM8961_LINMUTE_MASK                     0x0080  
#define WM8961_LINMUTE_SHIFT                         7  
#define WM8961_LINMUTE_WIDTH                         1  
#define WM8961_LIZC                             0x0040  
#define WM8961_LIZC_MASK                        0x0040  
#define WM8961_LIZC_SHIFT                            6  
#define WM8961_LIZC_WIDTH                            1  
#define WM8961_LINVOL_MASK                      0x003F  
#define WM8961_LINVOL_SHIFT                          0  
#define WM8961_LINVOL_WIDTH                          6  

#define WM8961_DEVICE_ID_MASK                   0xF000  
#define WM8961_DEVICE_ID_SHIFT                      12  
#define WM8961_DEVICE_ID_WIDTH                       4  
#define WM8961_CHIP_REV_MASK                    0x0E00  
#define WM8961_CHIP_REV_SHIFT                        9  
#define WM8961_CHIP_REV_WIDTH                        3  
#define WM8961_IPVU                             0x0100  
#define WM8961_IPVU_MASK                        0x0100  
#define WM8961_IPVU_SHIFT                            8  
#define WM8961_IPVU_WIDTH                            1  
#define WM8961_RINMUTE                          0x0080  
#define WM8961_RINMUTE_MASK                     0x0080  
#define WM8961_RINMUTE_SHIFT                         7  
#define WM8961_RINMUTE_WIDTH                         1  
#define WM8961_RIZC                             0x0040  
#define WM8961_RIZC_MASK                        0x0040  
#define WM8961_RIZC_SHIFT                            6  
#define WM8961_RIZC_WIDTH                            1  
#define WM8961_RINVOL_MASK                      0x003F  
#define WM8961_RINVOL_SHIFT                          0  
#define WM8961_RINVOL_WIDTH                          6  

#define WM8961_OUT1VU                           0x0100  
#define WM8961_OUT1VU_MASK                      0x0100  
#define WM8961_OUT1VU_SHIFT                          8  
#define WM8961_OUT1VU_WIDTH                          1  
#define WM8961_LO1ZC                            0x0080  
#define WM8961_LO1ZC_MASK                       0x0080  
#define WM8961_LO1ZC_SHIFT                           7  
#define WM8961_LO1ZC_WIDTH                           1  
#define WM8961_LOUT1VOL_MASK                    0x007F  
#define WM8961_LOUT1VOL_SHIFT                        0  
#define WM8961_LOUT1VOL_WIDTH                        7  

#define WM8961_OUT1VU                           0x0100  
#define WM8961_OUT1VU_MASK                      0x0100  
#define WM8961_OUT1VU_SHIFT                          8  
#define WM8961_OUT1VU_WIDTH                          1  
#define WM8961_RO1ZC                            0x0080  
#define WM8961_RO1ZC_MASK                       0x0080  
#define WM8961_RO1ZC_SHIFT                           7  
#define WM8961_RO1ZC_WIDTH                           1  
#define WM8961_ROUT1VOL_MASK                    0x007F  
#define WM8961_ROUT1VOL_SHIFT                        0  
#define WM8961_ROUT1VOL_WIDTH                        7  

#define WM8961_ADCDIV_MASK                      0x01C0  
#define WM8961_ADCDIV_SHIFT                          6  
#define WM8961_ADCDIV_WIDTH                          3  
#define WM8961_DACDIV_MASK                      0x0038  
#define WM8961_DACDIV_SHIFT                          3  
#define WM8961_DACDIV_WIDTH                          3  
#define WM8961_MCLKDIV                          0x0004  
#define WM8961_MCLKDIV_MASK                     0x0004  
#define WM8961_MCLKDIV_SHIFT                         2  
#define WM8961_MCLKDIV_WIDTH                         1  

#define WM8961_ADCPOL_MASK                      0x0060  
#define WM8961_ADCPOL_SHIFT                          5  
#define WM8961_ADCPOL_WIDTH                          2  
#define WM8961_DACMU                            0x0008  
#define WM8961_DACMU_MASK                       0x0008  
#define WM8961_DACMU_SHIFT                           3  
#define WM8961_DACMU_WIDTH                           1  
#define WM8961_DEEMPH_MASK                      0x0006  
#define WM8961_DEEMPH_SHIFT                          1  
#define WM8961_DEEMPH_WIDTH                          2  
#define WM8961_ADCHPD                           0x0001  
#define WM8961_ADCHPD_MASK                      0x0001  
#define WM8961_ADCHPD_SHIFT                          0  
#define WM8961_ADCHPD_WIDTH                          1  

#define WM8961_ADC_HPF_CUT_MASK                 0x0180  
#define WM8961_ADC_HPF_CUT_SHIFT                     7  
#define WM8961_ADC_HPF_CUT_WIDTH                     2  
#define WM8961_DACPOL_MASK                      0x0060  
#define WM8961_DACPOL_SHIFT                          5  
#define WM8961_DACPOL_WIDTH                          2  
#define WM8961_DACSMM                           0x0008  
#define WM8961_DACSMM_MASK                      0x0008  
#define WM8961_DACSMM_SHIFT                          3  
#define WM8961_DACSMM_WIDTH                          1  
#define WM8961_DACMR                            0x0004  
#define WM8961_DACMR_MASK                       0x0004  
#define WM8961_DACMR_SHIFT                           2  
#define WM8961_DACMR_WIDTH                           1  
#define WM8961_DACSLOPE                         0x0002  
#define WM8961_DACSLOPE_MASK                    0x0002  
#define WM8961_DACSLOPE_SHIFT                        1  
#define WM8961_DACSLOPE_WIDTH                        1  
#define WM8961_DAC_OSR128                       0x0001  
#define WM8961_DAC_OSR128_MASK                  0x0001  
#define WM8961_DAC_OSR128_SHIFT                      0  
#define WM8961_DAC_OSR128_WIDTH                      1  

#define WM8961_ALRSWAP                          0x0100  
#define WM8961_ALRSWAP_MASK                     0x0100  
#define WM8961_ALRSWAP_SHIFT                         8  
#define WM8961_ALRSWAP_WIDTH                         1  
#define WM8961_BCLKINV                          0x0080  
#define WM8961_BCLKINV_MASK                     0x0080  
#define WM8961_BCLKINV_SHIFT                         7  
#define WM8961_BCLKINV_WIDTH                         1  
#define WM8961_MS                               0x0040  
#define WM8961_MS_MASK                          0x0040  
#define WM8961_MS_SHIFT                              6  
#define WM8961_MS_WIDTH                              1  
#define WM8961_DLRSWAP                          0x0020  
#define WM8961_DLRSWAP_MASK                     0x0020  
#define WM8961_DLRSWAP_SHIFT                         5  
#define WM8961_DLRSWAP_WIDTH                         1  
#define WM8961_LRP                              0x0010  
#define WM8961_LRP_MASK                         0x0010  
#define WM8961_LRP_SHIFT                             4  
#define WM8961_LRP_WIDTH                             1  
#define WM8961_WL_MASK                          0x000C  
#define WM8961_WL_SHIFT                              2  
#define WM8961_WL_WIDTH                              2  
#define WM8961_FORMAT_MASK                      0x0003  
#define WM8961_FORMAT_SHIFT                          0  
#define WM8961_FORMAT_WIDTH                          2  

#define WM8961_DCLKDIV_MASK                     0x01C0  
#define WM8961_DCLKDIV_SHIFT                         6  
#define WM8961_DCLKDIV_WIDTH                         3  
#define WM8961_CLK_SYS_ENA                      0x0020  
#define WM8961_CLK_SYS_ENA_MASK                 0x0020  
#define WM8961_CLK_SYS_ENA_SHIFT                     5  
#define WM8961_CLK_SYS_ENA_WIDTH                     1  
#define WM8961_CLK_DSP_ENA                      0x0010  
#define WM8961_CLK_DSP_ENA_MASK                 0x0010  
#define WM8961_CLK_DSP_ENA_SHIFT                     4  
#define WM8961_CLK_DSP_ENA_WIDTH                     1  
#define WM8961_BCLKDIV_MASK                     0x000F  
#define WM8961_BCLKDIV_SHIFT                         0  
#define WM8961_BCLKDIV_WIDTH                         4  

#define WM8961_DACCOMP_MASK                     0x0018  
#define WM8961_DACCOMP_SHIFT                         3  
#define WM8961_DACCOMP_WIDTH                         2  
#define WM8961_ADCCOMP_MASK                     0x0006  
#define WM8961_ADCCOMP_SHIFT                         1  
#define WM8961_ADCCOMP_WIDTH                         2  
#define WM8961_LOOPBACK                         0x0001  
#define WM8961_LOOPBACK_MASK                    0x0001  
#define WM8961_LOOPBACK_SHIFT                        0  
#define WM8961_LOOPBACK_WIDTH                        1  

#define WM8961_DACVU                            0x0100  
#define WM8961_DACVU_MASK                       0x0100  
#define WM8961_DACVU_SHIFT                           8  
#define WM8961_DACVU_WIDTH                           1  
#define WM8961_LDACVOL_MASK                     0x00FF  
#define WM8961_LDACVOL_SHIFT                         0  
#define WM8961_LDACVOL_WIDTH                         8  

#define WM8961_DACVU                            0x0100  
#define WM8961_DACVU_MASK                       0x0100  
#define WM8961_DACVU_SHIFT                           8  
#define WM8961_DACVU_WIDTH                           1  
#define WM8961_RDACVOL_MASK                     0x00FF  
#define WM8961_RDACVOL_SHIFT                         0  
#define WM8961_RDACVOL_WIDTH                         8  

#define WM8961_LRCLK_RATE_MASK                  0x01FF  
#define WM8961_LRCLK_RATE_SHIFT                      0  
#define WM8961_LRCLK_RATE_WIDTH                      9  

#define WM8961_SW_RST_DEV_ID1_MASK              0xFFFF  
#define WM8961_SW_RST_DEV_ID1_SHIFT                  0  
#define WM8961_SW_RST_DEV_ID1_WIDTH                 16  

#define WM8961_ALCSEL_MASK                      0x0180  
#define WM8961_ALCSEL_SHIFT                          7  
#define WM8961_ALCSEL_WIDTH                          2  
#define WM8961_MAXGAIN_MASK                     0x0070  
#define WM8961_MAXGAIN_SHIFT                         4  
#define WM8961_MAXGAIN_WIDTH                         3  
#define WM8961_ALCL_MASK                        0x000F  
#define WM8961_ALCL_SHIFT                            0  
#define WM8961_ALCL_WIDTH                            4  

#define WM8961_ALCZC                            0x0080  
#define WM8961_ALCZC_MASK                       0x0080  
#define WM8961_ALCZC_SHIFT                           7  
#define WM8961_ALCZC_WIDTH                           1  
#define WM8961_MINGAIN_MASK                     0x0070  
#define WM8961_MINGAIN_SHIFT                         4  
#define WM8961_MINGAIN_WIDTH                         3  
#define WM8961_HLD_MASK                         0x000F  
#define WM8961_HLD_SHIFT                             0  
#define WM8961_HLD_WIDTH                             4  

#define WM8961_ALCMODE                          0x0100  
#define WM8961_ALCMODE_MASK                     0x0100  
#define WM8961_ALCMODE_SHIFT                         8  
#define WM8961_ALCMODE_WIDTH                         1  
#define WM8961_DCY_MASK                         0x00F0  
#define WM8961_DCY_SHIFT                             4  
#define WM8961_DCY_WIDTH                             4  
#define WM8961_ATK_MASK                         0x000F  
#define WM8961_ATK_SHIFT                             0  
#define WM8961_ATK_WIDTH                             4  

#define WM8961_NGTH_MASK                        0x00F8  
#define WM8961_NGTH_SHIFT                            3  
#define WM8961_NGTH_WIDTH                            5  
#define WM8961_NGG                              0x0002  
#define WM8961_NGG_MASK                         0x0002  
#define WM8961_NGG_SHIFT                             1  
#define WM8961_NGG_WIDTH                             1  
#define WM8961_NGAT                             0x0001  
#define WM8961_NGAT_MASK                        0x0001  
#define WM8961_NGAT_SHIFT                            0  
#define WM8961_NGAT_WIDTH                            1  

#define WM8961_ADCVU                            0x0100  
#define WM8961_ADCVU_MASK                       0x0100  
#define WM8961_ADCVU_SHIFT                           8  
#define WM8961_ADCVU_WIDTH                           1  
#define WM8961_LADCVOL_MASK                     0x00FF  
#define WM8961_LADCVOL_SHIFT                         0  
#define WM8961_LADCVOL_WIDTH                         8  

#define WM8961_ADCVU                            0x0100  
#define WM8961_ADCVU_MASK                       0x0100  
#define WM8961_ADCVU_SHIFT                           8  
#define WM8961_ADCVU_WIDTH                           1  
#define WM8961_RADCVOL_MASK                     0x00FF  
#define WM8961_RADCVOL_SHIFT                         0  
#define WM8961_RADCVOL_WIDTH                         8  

#define WM8961_TSDEN                            0x0100  
#define WM8961_TSDEN_MASK                       0x0100  
#define WM8961_TSDEN_SHIFT                           8  
#define WM8961_TSDEN_WIDTH                           1  
#define WM8961_DMONOMIX                         0x0010  
#define WM8961_DMONOMIX_MASK                    0x0010  
#define WM8961_DMONOMIX_SHIFT                        4  
#define WM8961_DMONOMIX_WIDTH                        1  
#define WM8961_TOEN                             0x0001  
#define WM8961_TOEN_MASK                        0x0001  
#define WM8961_TOEN_SHIFT                            0  
#define WM8961_TOEN_WIDTH                            1  

#define WM8961_TRIS                             0x0008  
#define WM8961_TRIS_MASK                        0x0008  
#define WM8961_TRIS_SHIFT                            3  
#define WM8961_TRIS_WIDTH                            1  

#define WM8961_VMIDSEL_MASK                     0x0180  
#define WM8961_VMIDSEL_SHIFT                         7  
#define WM8961_VMIDSEL_WIDTH                         2  
#define WM8961_VREF                             0x0040  
#define WM8961_VREF_MASK                        0x0040  
#define WM8961_VREF_SHIFT                            6  
#define WM8961_VREF_WIDTH                            1  
#define WM8961_AINL                             0x0020  
#define WM8961_AINL_MASK                        0x0020  
#define WM8961_AINL_SHIFT                            5  
#define WM8961_AINL_WIDTH                            1  
#define WM8961_AINR                             0x0010  
#define WM8961_AINR_MASK                        0x0010  
#define WM8961_AINR_SHIFT                            4  
#define WM8961_AINR_WIDTH                            1  
#define WM8961_ADCL                             0x0008  
#define WM8961_ADCL_MASK                        0x0008  
#define WM8961_ADCL_SHIFT                            3  
#define WM8961_ADCL_WIDTH                            1  
#define WM8961_ADCR                             0x0004  
#define WM8961_ADCR_MASK                        0x0004  
#define WM8961_ADCR_SHIFT                            2  
#define WM8961_ADCR_WIDTH                            1  
#define WM8961_MICB                             0x0002  
#define WM8961_MICB_MASK                        0x0002  
#define WM8961_MICB_SHIFT                            1  
#define WM8961_MICB_WIDTH                            1  

#define WM8961_DACL                             0x0100  
#define WM8961_DACL_MASK                        0x0100  
#define WM8961_DACL_SHIFT                            8  
#define WM8961_DACL_WIDTH                            1  
#define WM8961_DACR                             0x0080  
#define WM8961_DACR_MASK                        0x0080  
#define WM8961_DACR_SHIFT                            7  
#define WM8961_DACR_WIDTH                            1  
#define WM8961_LOUT1_PGA                        0x0040  
#define WM8961_LOUT1_PGA_MASK                   0x0040  
#define WM8961_LOUT1_PGA_SHIFT                       6  
#define WM8961_LOUT1_PGA_WIDTH                       1  
#define WM8961_ROUT1_PGA                        0x0020  
#define WM8961_ROUT1_PGA_MASK                   0x0020  
#define WM8961_ROUT1_PGA_SHIFT                       5  
#define WM8961_ROUT1_PGA_WIDTH                       1  
#define WM8961_SPKL_PGA                         0x0010  
#define WM8961_SPKL_PGA_MASK                    0x0010  
#define WM8961_SPKL_PGA_SHIFT                        4  
#define WM8961_SPKL_PGA_WIDTH                        1  
#define WM8961_SPKR_PGA                         0x0008  
#define WM8961_SPKR_PGA_MASK                    0x0008  
#define WM8961_SPKR_PGA_SHIFT                        3  
#define WM8961_SPKR_PGA_WIDTH                        1  

#define WM8961_SAMPLE_RATE_MASK                 0x0007  
#define WM8961_SAMPLE_RATE_SHIFT                     0  
#define WM8961_SAMPLE_RATE_WIDTH                     3  

#define WM8961_BUFDCOPEN                        0x0010  
#define WM8961_BUFDCOPEN_MASK                   0x0010  
#define WM8961_BUFDCOPEN_SHIFT                       4  
#define WM8961_BUFDCOPEN_WIDTH                       1  
#define WM8961_BUFIOEN                          0x0008  
#define WM8961_BUFIOEN_MASK                     0x0008  
#define WM8961_BUFIOEN_SHIFT                         3  
#define WM8961_BUFIOEN_WIDTH                         1  
#define WM8961_SOFT_ST                          0x0004  
#define WM8961_SOFT_ST_MASK                     0x0004  
#define WM8961_SOFT_ST_SHIFT                         2  
#define WM8961_SOFT_ST_WIDTH                         1  

#define WM8961_CLK_TO_DIV_MASK                  0x0180  
#define WM8961_CLK_TO_DIV_SHIFT                      7  
#define WM8961_CLK_TO_DIV_WIDTH                      2  
#define WM8961_CLK_256K_DIV_MASK                0x007E  
#define WM8961_CLK_256K_DIV_SHIFT                    1  
#define WM8961_CLK_256K_DIV_WIDTH                    6  
#define WM8961_MANUAL_MODE                      0x0001  
#define WM8961_MANUAL_MODE_MASK                 0x0001  
#define WM8961_MANUAL_MODE_SHIFT                     0  
#define WM8961_MANUAL_MODE_WIDTH                     1  

#define WM8961_LMICBOOST_MASK                   0x0030  
#define WM8961_LMICBOOST_SHIFT                       4  
#define WM8961_LMICBOOST_WIDTH                       2  

#define WM8961_RMICBOOST_MASK                   0x0030  
#define WM8961_RMICBOOST_SHIFT                       4  
#define WM8961_RMICBOOST_WIDTH                       2  

#define WM8961_SPKVU                            0x0100  
#define WM8961_SPKVU_MASK                       0x0100  
#define WM8961_SPKVU_SHIFT                           8  
#define WM8961_SPKVU_WIDTH                           1  
#define WM8961_SPKLZC                           0x0080  
#define WM8961_SPKLZC_MASK                      0x0080  
#define WM8961_SPKLZC_SHIFT                          7  
#define WM8961_SPKLZC_WIDTH                          1  
#define WM8961_SPKLVOL_MASK                     0x007F  
#define WM8961_SPKLVOL_SHIFT                         0  
#define WM8961_SPKLVOL_WIDTH                         7  

#define WM8961_SPKVU                            0x0100  
#define WM8961_SPKVU_MASK                       0x0100  
#define WM8961_SPKVU_SHIFT                           8  
#define WM8961_SPKVU_WIDTH                           1  
#define WM8961_SPKRZC                           0x0080  
#define WM8961_SPKRZC_MASK                      0x0080  
#define WM8961_SPKRZC_SHIFT                          7  
#define WM8961_SPKRZC_WIDTH                          1  
#define WM8961_SPKRVOL_MASK                     0x007F  
#define WM8961_SPKRVOL_SHIFT                         0  
#define WM8961_SPKRVOL_WIDTH                         7  

#define WM8961_TEMP_SHUT                        0x0002  
#define WM8961_TEMP_SHUT_MASK                   0x0002  
#define WM8961_TEMP_SHUT_SHIFT                       1  
#define WM8961_TEMP_SHUT_WIDTH                       1  
#define WM8961_TEMP_WARN                        0x0001  
#define WM8961_TEMP_WARN_MASK                   0x0001  
#define WM8961_TEMP_WARN_SHIFT                       0  
#define WM8961_TEMP_WARN_WIDTH                       1  

#define WM8961_TSENSEN                          0x0002  
#define WM8961_TSENSEN_MASK                     0x0002  
#define WM8961_TSENSEN_SHIFT                         1  
#define WM8961_TSENSEN_WIDTH                         1  
#define WM8961_MBSEL                            0x0001  
#define WM8961_MBSEL_MASK                       0x0001  
#define WM8961_MBSEL_SHIFT                           0  
#define WM8961_MBSEL_WIDTH                           1  

#define WM8961_SPKR_ENA                         0x0080  
#define WM8961_SPKR_ENA_MASK                    0x0080  
#define WM8961_SPKR_ENA_SHIFT                        7  
#define WM8961_SPKR_ENA_WIDTH                        1  
#define WM8961_SPKL_ENA                         0x0040  
#define WM8961_SPKL_ENA_MASK                    0x0040  
#define WM8961_SPKL_ENA_SHIFT                        6  
#define WM8961_SPKL_ENA_WIDTH                        1  

#define WM8961_CLASSD_ACGAIN_MASK               0x0007  
#define WM8961_CLASSD_ACGAIN_SHIFT                   0  
#define WM8961_CLASSD_ACGAIN_WIDTH                   3  

#define WM8961_CLK_DCS_DIV_MASK                 0x01E0  
#define WM8961_CLK_DCS_DIV_SHIFT                     5  
#define WM8961_CLK_DCS_DIV_WIDTH                     4  
#define WM8961_CLK_SYS_RATE_MASK                0x001E  
#define WM8961_CLK_SYS_RATE_SHIFT                    1  
#define WM8961_CLK_SYS_RATE_WIDTH                    4  

#define WM8961_ADCR_DAC_SVOL_MASK               0x00F0  
#define WM8961_ADCR_DAC_SVOL_SHIFT                   4  
#define WM8961_ADCR_DAC_SVOL_WIDTH                   4  
#define WM8961_ADC_TO_DACR_MASK                 0x000C  
#define WM8961_ADC_TO_DACR_SHIFT                     2  
#define WM8961_ADC_TO_DACR_WIDTH                     2  

#define WM8961_ADCL_DAC_SVOL_MASK               0x00F0  
#define WM8961_ADCL_DAC_SVOL_SHIFT                   4  
#define WM8961_ADCL_DAC_SVOL_WIDTH                   4  
#define WM8961_ADC_TO_DACL_MASK                 0x000C  
#define WM8961_ADC_TO_DACL_SHIFT                     2  
#define WM8961_ADC_TO_DACL_WIDTH                     2  

#define WM8961_DCS_ENA_CHAN_INL                 0x0080  
#define WM8961_DCS_ENA_CHAN_INL_MASK            0x0080  
#define WM8961_DCS_ENA_CHAN_INL_SHIFT                7  
#define WM8961_DCS_ENA_CHAN_INL_WIDTH                1  
#define WM8961_DCS_TRIG_STARTUP_INL             0x0040  
#define WM8961_DCS_TRIG_STARTUP_INL_MASK        0x0040  
#define WM8961_DCS_TRIG_STARTUP_INL_SHIFT            6  
#define WM8961_DCS_TRIG_STARTUP_INL_WIDTH            1  
#define WM8961_DCS_TRIG_SERIES_INL              0x0010  
#define WM8961_DCS_TRIG_SERIES_INL_MASK         0x0010  
#define WM8961_DCS_TRIG_SERIES_INL_SHIFT             4  
#define WM8961_DCS_TRIG_SERIES_INL_WIDTH             1  
#define WM8961_DCS_ENA_CHAN_INR                 0x0008  
#define WM8961_DCS_ENA_CHAN_INR_MASK            0x0008  
#define WM8961_DCS_ENA_CHAN_INR_SHIFT                3  
#define WM8961_DCS_ENA_CHAN_INR_WIDTH                1  
#define WM8961_DCS_TRIG_STARTUP_INR             0x0004  
#define WM8961_DCS_TRIG_STARTUP_INR_MASK        0x0004  
#define WM8961_DCS_TRIG_STARTUP_INR_SHIFT            2  
#define WM8961_DCS_TRIG_STARTUP_INR_WIDTH            1  
#define WM8961_DCS_TRIG_SERIES_INR              0x0001  
#define WM8961_DCS_TRIG_SERIES_INR_MASK         0x0001  
#define WM8961_DCS_TRIG_SERIES_INR_SHIFT             0  
#define WM8961_DCS_TRIG_SERIES_INR_WIDTH             1  

#define WM8961_DCS_ENA_CHAN_HPL                 0x0080  
#define WM8961_DCS_ENA_CHAN_HPL_MASK            0x0080  
#define WM8961_DCS_ENA_CHAN_HPL_SHIFT                7  
#define WM8961_DCS_ENA_CHAN_HPL_WIDTH                1  
#define WM8961_DCS_TRIG_STARTUP_HPL             0x0040  
#define WM8961_DCS_TRIG_STARTUP_HPL_MASK        0x0040  
#define WM8961_DCS_TRIG_STARTUP_HPL_SHIFT            6  
#define WM8961_DCS_TRIG_STARTUP_HPL_WIDTH            1  
#define WM8961_DCS_TRIG_SERIES_HPL              0x0010  
#define WM8961_DCS_TRIG_SERIES_HPL_MASK         0x0010  
#define WM8961_DCS_TRIG_SERIES_HPL_SHIFT             4  
#define WM8961_DCS_TRIG_SERIES_HPL_WIDTH             1  
#define WM8961_DCS_ENA_CHAN_HPR                 0x0008  
#define WM8961_DCS_ENA_CHAN_HPR_MASK            0x0008  
#define WM8961_DCS_ENA_CHAN_HPR_SHIFT                3  
#define WM8961_DCS_ENA_CHAN_HPR_WIDTH                1  
#define WM8961_DCS_TRIG_STARTUP_HPR             0x0004  
#define WM8961_DCS_TRIG_STARTUP_HPR_MASK        0x0004  
#define WM8961_DCS_TRIG_STARTUP_HPR_SHIFT            2  
#define WM8961_DCS_TRIG_STARTUP_HPR_WIDTH            1  
#define WM8961_DCS_TRIG_SERIES_HPR              0x0001  
#define WM8961_DCS_TRIG_SERIES_HPR_MASK         0x0001  
#define WM8961_DCS_TRIG_SERIES_HPR_SHIFT             0  
#define WM8961_DCS_TRIG_SERIES_HPR_WIDTH             1  

#define WM8961_DCS_FILT_BW_SERIES_MASK          0x0030  
#define WM8961_DCS_FILT_BW_SERIES_SHIFT              4  
#define WM8961_DCS_FILT_BW_SERIES_WIDTH              2  

#define WM8961_DCS_SERIES_NO_HP_MASK            0x007F  
#define WM8961_DCS_SERIES_NO_HP_SHIFT                0  
#define WM8961_DCS_SERIES_NO_HP_WIDTH                7  

#define WM8961_HP_PGAS_BIAS_MASK                0x0007  
#define WM8961_HP_PGAS_BIAS_SHIFT                    0  
#define WM8961_HP_PGAS_BIAS_WIDTH                    3  

#define WM8961_HPL_RMV_SHORT                    0x0080  
#define WM8961_HPL_RMV_SHORT_MASK               0x0080  
#define WM8961_HPL_RMV_SHORT_SHIFT                   7  
#define WM8961_HPL_RMV_SHORT_WIDTH                   1  
#define WM8961_HPL_ENA_OUTP                     0x0040  
#define WM8961_HPL_ENA_OUTP_MASK                0x0040  
#define WM8961_HPL_ENA_OUTP_SHIFT                    6  
#define WM8961_HPL_ENA_OUTP_WIDTH                    1  
#define WM8961_HPL_ENA_DLY                      0x0020  
#define WM8961_HPL_ENA_DLY_MASK                 0x0020  
#define WM8961_HPL_ENA_DLY_SHIFT                     5  
#define WM8961_HPL_ENA_DLY_WIDTH                     1  
#define WM8961_HPL_ENA                          0x0010  
#define WM8961_HPL_ENA_MASK                     0x0010  
#define WM8961_HPL_ENA_SHIFT                         4  
#define WM8961_HPL_ENA_WIDTH                         1  
#define WM8961_HPR_RMV_SHORT                    0x0008  
#define WM8961_HPR_RMV_SHORT_MASK               0x0008  
#define WM8961_HPR_RMV_SHORT_SHIFT                   3  
#define WM8961_HPR_RMV_SHORT_WIDTH                   1  
#define WM8961_HPR_ENA_OUTP                     0x0004  
#define WM8961_HPR_ENA_OUTP_MASK                0x0004  
#define WM8961_HPR_ENA_OUTP_SHIFT                    2  
#define WM8961_HPR_ENA_OUTP_WIDTH                    1  
#define WM8961_HPR_ENA_DLY                      0x0002  
#define WM8961_HPR_ENA_DLY_MASK                 0x0002  
#define WM8961_HPR_ENA_DLY_SHIFT                     1  
#define WM8961_HPR_ENA_DLY_WIDTH                     1  
#define WM8961_HPR_ENA                          0x0001  
#define WM8961_HPR_ENA_MASK                     0x0001  
#define WM8961_HPR_ENA_SHIFT                         0  
#define WM8961_HPR_ENA_WIDTH                         1  

#define WM8961_HPL_VOL_MASK                     0x01C0  
#define WM8961_HPL_VOL_SHIFT                         6  
#define WM8961_HPL_VOL_WIDTH                         3  
#define WM8961_HPR_VOL_MASK                     0x0038  
#define WM8961_HPR_VOL_SHIFT                         3  
#define WM8961_HPR_VOL_WIDTH                         3  
#define WM8961_HP_BIAS_BOOST_MASK               0x0007  
#define WM8961_HP_BIAS_BOOST_SHIFT                   0  
#define WM8961_HP_BIAS_BOOST_WIDTH                   3  

#define WM8961_CP_ENA                           0x0001  
#define WM8961_CP_ENA_MASK                      0x0001  
#define WM8961_CP_ENA_SHIFT                          0  
#define WM8961_CP_ENA_WIDTH                          1  

#define WM8961_CP_DYN_PWR_MASK                  0x0003  
#define WM8961_CP_DYN_PWR_SHIFT                      0  
#define WM8961_CP_DYN_PWR_WIDTH                      2  

#define WM8961_WSEQ_ENA                         0x0020  
#define WM8961_WSEQ_ENA_MASK                    0x0020  
#define WM8961_WSEQ_ENA_SHIFT                        5  
#define WM8961_WSEQ_ENA_WIDTH                        1  
#define WM8961_WSEQ_WRITE_INDEX_MASK            0x001F  
#define WM8961_WSEQ_WRITE_INDEX_SHIFT                0  
#define WM8961_WSEQ_WRITE_INDEX_WIDTH                5  

#define WM8961_WSEQ_EOS                         0x0100  
#define WM8961_WSEQ_EOS_MASK                    0x0100  
#define WM8961_WSEQ_EOS_SHIFT                        8  
#define WM8961_WSEQ_EOS_WIDTH                        1  
#define WM8961_WSEQ_ADDR_MASK                   0x00FF  
#define WM8961_WSEQ_ADDR_SHIFT                       0  
#define WM8961_WSEQ_ADDR_WIDTH                       8  

#define WM8961_WSEQ_DATA_MASK                   0x00FF  
#define WM8961_WSEQ_DATA_SHIFT                       0  
#define WM8961_WSEQ_DATA_WIDTH                       8  

#define WM8961_WSEQ_ABORT                       0x0100  
#define WM8961_WSEQ_ABORT_MASK                  0x0100  
#define WM8961_WSEQ_ABORT_SHIFT                      8  
#define WM8961_WSEQ_ABORT_WIDTH                      1  
#define WM8961_WSEQ_START                       0x0080  
#define WM8961_WSEQ_START_MASK                  0x0080  
#define WM8961_WSEQ_START_SHIFT                      7  
#define WM8961_WSEQ_START_WIDTH                      1  
#define WM8961_WSEQ_START_INDEX_MASK            0x003F  
#define WM8961_WSEQ_START_INDEX_SHIFT                0  
#define WM8961_WSEQ_START_INDEX_WIDTH                6  

#define WM8961_WSEQ_DATA_WIDTH_MASK             0x0070  
#define WM8961_WSEQ_DATA_WIDTH_SHIFT                 4  
#define WM8961_WSEQ_DATA_WIDTH_WIDTH                 3  
#define WM8961_WSEQ_DATA_START_MASK             0x000F  
#define WM8961_WSEQ_DATA_START_SHIFT                 0  
#define WM8961_WSEQ_DATA_START_WIDTH                 4  

#define WM8961_WSEQ_DELAY_MASK                  0x000F  
#define WM8961_WSEQ_DELAY_SHIFT                      0  
#define WM8961_WSEQ_DELAY_WIDTH                      4  

#define WM8961_WSEQ_BUSY                        0x0001  
#define WM8961_WSEQ_BUSY_MASK                   0x0001  
#define WM8961_WSEQ_BUSY_SHIFT                       0  
#define WM8961_WSEQ_BUSY_WIDTH                       1  

#define WM8961_ARA_ENA                          0x0002  
#define WM8961_ARA_ENA_MASK                     0x0002  
#define WM8961_ARA_ENA_SHIFT                         1  
#define WM8961_ARA_ENA_WIDTH                         1  
#define WM8961_AUTO_INC                         0x0001  
#define WM8961_AUTO_INC_MASK                    0x0001  
#define WM8961_AUTO_INC_SHIFT                        0  
#define WM8961_AUTO_INC_WIDTH                        1  

#endif
