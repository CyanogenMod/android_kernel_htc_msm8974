#ifndef WM9081_H
#define WM9081_H

/*
 * wm9081.c  --  WM9081 ALSA SoC Audio driver
 *
 * Author: Mark Brown
 *
 * Copyright 2009 Wolfson Microelectronics plc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/soc.h>

#define WM9081_SYSCLK_MCLK      1   
#define WM9081_SYSCLK_FLL_MCLK  2   

#define WM9081_SOFTWARE_RESET                   0x00
#define WM9081_ANALOGUE_LINEOUT                 0x02
#define WM9081_ANALOGUE_SPEAKER_PGA             0x03
#define WM9081_VMID_CONTROL                     0x04
#define WM9081_BIAS_CONTROL_1                   0x05
#define WM9081_ANALOGUE_MIXER                   0x07
#define WM9081_ANTI_POP_CONTROL                 0x08
#define WM9081_ANALOGUE_SPEAKER_1               0x09
#define WM9081_ANALOGUE_SPEAKER_2               0x0A
#define WM9081_POWER_MANAGEMENT                 0x0B
#define WM9081_CLOCK_CONTROL_1                  0x0C
#define WM9081_CLOCK_CONTROL_2                  0x0D
#define WM9081_CLOCK_CONTROL_3                  0x0E
#define WM9081_FLL_CONTROL_1                    0x10
#define WM9081_FLL_CONTROL_2                    0x11
#define WM9081_FLL_CONTROL_3                    0x12
#define WM9081_FLL_CONTROL_4                    0x13
#define WM9081_FLL_CONTROL_5                    0x14
#define WM9081_AUDIO_INTERFACE_1                0x16
#define WM9081_AUDIO_INTERFACE_2                0x17
#define WM9081_AUDIO_INTERFACE_3                0x18
#define WM9081_AUDIO_INTERFACE_4                0x19
#define WM9081_INTERRUPT_STATUS                 0x1A
#define WM9081_INTERRUPT_STATUS_MASK            0x1B
#define WM9081_INTERRUPT_POLARITY               0x1C
#define WM9081_INTERRUPT_CONTROL                0x1D
#define WM9081_DAC_DIGITAL_1                    0x1E
#define WM9081_DAC_DIGITAL_2                    0x1F
#define WM9081_DRC_1                            0x20
#define WM9081_DRC_2                            0x21
#define WM9081_DRC_3                            0x22
#define WM9081_DRC_4                            0x23
#define WM9081_WRITE_SEQUENCER_1                0x26
#define WM9081_WRITE_SEQUENCER_2                0x27
#define WM9081_MW_SLAVE_1                       0x28
#define WM9081_EQ_1                             0x2A
#define WM9081_EQ_2                             0x2B
#define WM9081_EQ_3                             0x2C
#define WM9081_EQ_4                             0x2D
#define WM9081_EQ_5                             0x2E
#define WM9081_EQ_6                             0x2F
#define WM9081_EQ_7                             0x30
#define WM9081_EQ_8                             0x31
#define WM9081_EQ_9                             0x32
#define WM9081_EQ_10                            0x33
#define WM9081_EQ_11                            0x34
#define WM9081_EQ_12                            0x35
#define WM9081_EQ_13                            0x36
#define WM9081_EQ_14                            0x37
#define WM9081_EQ_15                            0x38
#define WM9081_EQ_16                            0x39
#define WM9081_EQ_17                            0x3A
#define WM9081_EQ_18                            0x3B
#define WM9081_EQ_19                            0x3C
#define WM9081_EQ_20                            0x3D

#define WM9081_REGISTER_COUNT                   55
#define WM9081_MAX_REGISTER                     0x3D


#define WM9081_SW_RST_DEV_ID1_MASK              0xFFFF  
#define WM9081_SW_RST_DEV_ID1_SHIFT                  0  
#define WM9081_SW_RST_DEV_ID1_WIDTH                 16  

#define WM9081_LINEOUT_MUTE                     0x0080  
#define WM9081_LINEOUT_MUTE_MASK                0x0080  
#define WM9081_LINEOUT_MUTE_SHIFT                    7  
#define WM9081_LINEOUT_MUTE_WIDTH                    1  
#define WM9081_LINEOUTZC                        0x0040  
#define WM9081_LINEOUTZC_MASK                   0x0040  
#define WM9081_LINEOUTZC_SHIFT                       6  
#define WM9081_LINEOUTZC_WIDTH                       1  
#define WM9081_LINEOUT_VOL_MASK                 0x003F  
#define WM9081_LINEOUT_VOL_SHIFT                     0  
#define WM9081_LINEOUT_VOL_WIDTH                     6  

#define WM9081_SPKPGA_MUTE                      0x0080  
#define WM9081_SPKPGA_MUTE_MASK                 0x0080  
#define WM9081_SPKPGA_MUTE_SHIFT                     7  
#define WM9081_SPKPGA_MUTE_WIDTH                     1  
#define WM9081_SPKPGAZC                         0x0040  
#define WM9081_SPKPGAZC_MASK                    0x0040  
#define WM9081_SPKPGAZC_SHIFT                        6  
#define WM9081_SPKPGAZC_WIDTH                        1  
#define WM9081_SPKPGA_VOL_MASK                  0x003F  
#define WM9081_SPKPGA_VOL_SHIFT                      0  
#define WM9081_SPKPGA_VOL_WIDTH                      6  

#define WM9081_VMID_BUF_ENA                     0x0020  
#define WM9081_VMID_BUF_ENA_MASK                0x0020  
#define WM9081_VMID_BUF_ENA_SHIFT                    5  
#define WM9081_VMID_BUF_ENA_WIDTH                    1  
#define WM9081_VMID_RAMP                        0x0008  
#define WM9081_VMID_RAMP_MASK                   0x0008  
#define WM9081_VMID_RAMP_SHIFT                       3  
#define WM9081_VMID_RAMP_WIDTH                       1  
#define WM9081_VMID_SEL_MASK                    0x0006  
#define WM9081_VMID_SEL_SHIFT                        1  
#define WM9081_VMID_SEL_WIDTH                        2  
#define WM9081_VMID_FAST_ST                     0x0001  
#define WM9081_VMID_FAST_ST_MASK                0x0001  
#define WM9081_VMID_FAST_ST_SHIFT                    0  
#define WM9081_VMID_FAST_ST_WIDTH                    1  

#define WM9081_BIAS_SRC                         0x0040  
#define WM9081_BIAS_SRC_MASK                    0x0040  
#define WM9081_BIAS_SRC_SHIFT                        6  
#define WM9081_BIAS_SRC_WIDTH                        1  
#define WM9081_STBY_BIAS_LVL                    0x0020  
#define WM9081_STBY_BIAS_LVL_MASK               0x0020  
#define WM9081_STBY_BIAS_LVL_SHIFT                   5  
#define WM9081_STBY_BIAS_LVL_WIDTH                   1  
#define WM9081_STBY_BIAS_ENA                    0x0010  
#define WM9081_STBY_BIAS_ENA_MASK               0x0010  
#define WM9081_STBY_BIAS_ENA_SHIFT                   4  
#define WM9081_STBY_BIAS_ENA_WIDTH                   1  
#define WM9081_BIAS_LVL_MASK                    0x000C  
#define WM9081_BIAS_LVL_SHIFT                        2  
#define WM9081_BIAS_LVL_WIDTH                        2  
#define WM9081_BIAS_ENA                         0x0002  
#define WM9081_BIAS_ENA_MASK                    0x0002  
#define WM9081_BIAS_ENA_SHIFT                        1  
#define WM9081_BIAS_ENA_WIDTH                        1  
#define WM9081_STARTUP_BIAS_ENA                 0x0001  
#define WM9081_STARTUP_BIAS_ENA_MASK            0x0001  
#define WM9081_STARTUP_BIAS_ENA_SHIFT                0  
#define WM9081_STARTUP_BIAS_ENA_WIDTH                1  

#define WM9081_DAC_SEL                          0x0010  
#define WM9081_DAC_SEL_MASK                     0x0010  
#define WM9081_DAC_SEL_SHIFT                         4  
#define WM9081_DAC_SEL_WIDTH                         1  
#define WM9081_IN2_VOL                          0x0008  
#define WM9081_IN2_VOL_MASK                     0x0008  
#define WM9081_IN2_VOL_SHIFT                         3  
#define WM9081_IN2_VOL_WIDTH                         1  
#define WM9081_IN2_ENA                          0x0004  
#define WM9081_IN2_ENA_MASK                     0x0004  
#define WM9081_IN2_ENA_SHIFT                         2  
#define WM9081_IN2_ENA_WIDTH                         1  
#define WM9081_IN1_VOL                          0x0002  
#define WM9081_IN1_VOL_MASK                     0x0002  
#define WM9081_IN1_VOL_SHIFT                         1  
#define WM9081_IN1_VOL_WIDTH                         1  
#define WM9081_IN1_ENA                          0x0001  
#define WM9081_IN1_ENA_MASK                     0x0001  
#define WM9081_IN1_ENA_SHIFT                         0  
#define WM9081_IN1_ENA_WIDTH                         1  

#define WM9081_LINEOUT_DISCH                    0x0004  
#define WM9081_LINEOUT_DISCH_MASK               0x0004  
#define WM9081_LINEOUT_DISCH_SHIFT                   2  
#define WM9081_LINEOUT_DISCH_WIDTH                   1  
#define WM9081_LINEOUT_VROI                     0x0002  
#define WM9081_LINEOUT_VROI_MASK                0x0002  
#define WM9081_LINEOUT_VROI_SHIFT                    1  
#define WM9081_LINEOUT_VROI_WIDTH                    1  
#define WM9081_LINEOUT_CLAMP                    0x0001  
#define WM9081_LINEOUT_CLAMP_MASK               0x0001  
#define WM9081_LINEOUT_CLAMP_SHIFT                   0  
#define WM9081_LINEOUT_CLAMP_WIDTH                   1  

#define WM9081_SPK_DCGAIN_MASK                  0x0038  
#define WM9081_SPK_DCGAIN_SHIFT                      3  
#define WM9081_SPK_DCGAIN_WIDTH                      3  
#define WM9081_SPK_ACGAIN_MASK                  0x0007  
#define WM9081_SPK_ACGAIN_SHIFT                      0  
#define WM9081_SPK_ACGAIN_WIDTH                      3  

#define WM9081_SPK_MODE                         0x0040  
#define WM9081_SPK_MODE_MASK                    0x0040  
#define WM9081_SPK_MODE_SHIFT                        6  
#define WM9081_SPK_MODE_WIDTH                        1  
#define WM9081_SPK_INV_MUTE                     0x0010  
#define WM9081_SPK_INV_MUTE_MASK                0x0010  
#define WM9081_SPK_INV_MUTE_SHIFT                    4  
#define WM9081_SPK_INV_MUTE_WIDTH                    1  
#define WM9081_OUT_SPK_CTRL                     0x0008  
#define WM9081_OUT_SPK_CTRL_MASK                0x0008  
#define WM9081_OUT_SPK_CTRL_SHIFT                    3  
#define WM9081_OUT_SPK_CTRL_WIDTH                    1  

#define WM9081_TSHUT_ENA                        0x0100  
#define WM9081_TSHUT_ENA_MASK                   0x0100  
#define WM9081_TSHUT_ENA_SHIFT                       8  
#define WM9081_TSHUT_ENA_WIDTH                       1  
#define WM9081_TSENSE_ENA                       0x0080  
#define WM9081_TSENSE_ENA_MASK                  0x0080  
#define WM9081_TSENSE_ENA_SHIFT                      7  
#define WM9081_TSENSE_ENA_WIDTH                      1  
#define WM9081_TEMP_SHUT                        0x0040  
#define WM9081_TEMP_SHUT_MASK                   0x0040  
#define WM9081_TEMP_SHUT_SHIFT                       6  
#define WM9081_TEMP_SHUT_WIDTH                       1  
#define WM9081_LINEOUT_ENA                      0x0010  
#define WM9081_LINEOUT_ENA_MASK                 0x0010  
#define WM9081_LINEOUT_ENA_SHIFT                     4  
#define WM9081_LINEOUT_ENA_WIDTH                     1  
#define WM9081_SPKPGA_ENA                       0x0004  
#define WM9081_SPKPGA_ENA_MASK                  0x0004  
#define WM9081_SPKPGA_ENA_SHIFT                      2  
#define WM9081_SPKPGA_ENA_WIDTH                      1  
#define WM9081_SPK_ENA                          0x0002  
#define WM9081_SPK_ENA_MASK                     0x0002  
#define WM9081_SPK_ENA_SHIFT                         1  
#define WM9081_SPK_ENA_WIDTH                         1  
#define WM9081_DAC_ENA                          0x0001  
#define WM9081_DAC_ENA_MASK                     0x0001  
#define WM9081_DAC_ENA_SHIFT                         0  
#define WM9081_DAC_ENA_WIDTH                         1  

#define WM9081_CLK_OP_DIV_MASK                  0x1C00  
#define WM9081_CLK_OP_DIV_SHIFT                     10  
#define WM9081_CLK_OP_DIV_WIDTH                      3  
#define WM9081_CLK_TO_DIV_MASK                  0x0300  
#define WM9081_CLK_TO_DIV_SHIFT                      8  
#define WM9081_CLK_TO_DIV_WIDTH                      2  
#define WM9081_MCLKDIV2                         0x0080  
#define WM9081_MCLKDIV2_MASK                    0x0080  
#define WM9081_MCLKDIV2_SHIFT                        7  
#define WM9081_MCLKDIV2_WIDTH                        1  

#define WM9081_CLK_SYS_RATE_MASK                0x00F0  
#define WM9081_CLK_SYS_RATE_SHIFT                    4  
#define WM9081_CLK_SYS_RATE_WIDTH                    4  
#define WM9081_SAMPLE_RATE_MASK                 0x000F  
#define WM9081_SAMPLE_RATE_SHIFT                     0  
#define WM9081_SAMPLE_RATE_WIDTH                     4  

#define WM9081_CLK_SRC_SEL                      0x2000  
#define WM9081_CLK_SRC_SEL_MASK                 0x2000  
#define WM9081_CLK_SRC_SEL_SHIFT                    13  
#define WM9081_CLK_SRC_SEL_WIDTH                     1  
#define WM9081_CLK_OP_ENA                       0x0020  
#define WM9081_CLK_OP_ENA_MASK                  0x0020  
#define WM9081_CLK_OP_ENA_SHIFT                      5  
#define WM9081_CLK_OP_ENA_WIDTH                      1  
#define WM9081_CLK_TO_ENA                       0x0004  
#define WM9081_CLK_TO_ENA_MASK                  0x0004  
#define WM9081_CLK_TO_ENA_SHIFT                      2  
#define WM9081_CLK_TO_ENA_WIDTH                      1  
#define WM9081_CLK_DSP_ENA                      0x0002  
#define WM9081_CLK_DSP_ENA_MASK                 0x0002  
#define WM9081_CLK_DSP_ENA_SHIFT                     1  
#define WM9081_CLK_DSP_ENA_WIDTH                     1  
#define WM9081_CLK_SYS_ENA                      0x0001  
#define WM9081_CLK_SYS_ENA_MASK                 0x0001  
#define WM9081_CLK_SYS_ENA_SHIFT                     0  
#define WM9081_CLK_SYS_ENA_WIDTH                     1  

#define WM9081_FLL_HOLD                         0x0008  
#define WM9081_FLL_HOLD_MASK                    0x0008  
#define WM9081_FLL_HOLD_SHIFT                        3  
#define WM9081_FLL_HOLD_WIDTH                        1  
#define WM9081_FLL_FRAC                         0x0004  
#define WM9081_FLL_FRAC_MASK                    0x0004  
#define WM9081_FLL_FRAC_SHIFT                        2  
#define WM9081_FLL_FRAC_WIDTH                        1  
#define WM9081_FLL_ENA                          0x0001  
#define WM9081_FLL_ENA_MASK                     0x0001  
#define WM9081_FLL_ENA_SHIFT                         0  
#define WM9081_FLL_ENA_WIDTH                         1  

#define WM9081_FLL_OUTDIV_MASK                  0x0700  
#define WM9081_FLL_OUTDIV_SHIFT                      8  
#define WM9081_FLL_OUTDIV_WIDTH                      3  
#define WM9081_FLL_CTRL_RATE_MASK               0x0070  
#define WM9081_FLL_CTRL_RATE_SHIFT                   4  
#define WM9081_FLL_CTRL_RATE_WIDTH                   3  
#define WM9081_FLL_FRATIO_MASK                  0x0007  
#define WM9081_FLL_FRATIO_SHIFT                      0  
#define WM9081_FLL_FRATIO_WIDTH                      3  

#define WM9081_FLL_K_MASK                       0xFFFF  
#define WM9081_FLL_K_SHIFT                           0  
#define WM9081_FLL_K_WIDTH                          16  

#define WM9081_FLL_N_MASK                       0x7FE0  
#define WM9081_FLL_N_SHIFT                           5  
#define WM9081_FLL_N_WIDTH                          10  
#define WM9081_FLL_GAIN_MASK                    0x000F  
#define WM9081_FLL_GAIN_SHIFT                        0  
#define WM9081_FLL_GAIN_WIDTH                        4  

#define WM9081_FLL_CLK_REF_DIV_MASK             0x0018  
#define WM9081_FLL_CLK_REF_DIV_SHIFT                 3  
#define WM9081_FLL_CLK_REF_DIV_WIDTH                 2  
#define WM9081_FLL_CLK_SRC_MASK                 0x0003  
#define WM9081_FLL_CLK_SRC_SHIFT                     0  
#define WM9081_FLL_CLK_SRC_WIDTH                     2  

#define WM9081_AIFDAC_CHAN                      0x0040  
#define WM9081_AIFDAC_CHAN_MASK                 0x0040  
#define WM9081_AIFDAC_CHAN_SHIFT                     6  
#define WM9081_AIFDAC_CHAN_WIDTH                     1  
#define WM9081_AIFDAC_TDM_SLOT_MASK             0x0030  
#define WM9081_AIFDAC_TDM_SLOT_SHIFT                 4  
#define WM9081_AIFDAC_TDM_SLOT_WIDTH                 2  
#define WM9081_AIFDAC_TDM_MODE_MASK             0x000C  
#define WM9081_AIFDAC_TDM_MODE_SHIFT                 2  
#define WM9081_AIFDAC_TDM_MODE_WIDTH                 2  
#define WM9081_DAC_COMP                         0x0002  
#define WM9081_DAC_COMP_MASK                    0x0002  
#define WM9081_DAC_COMP_SHIFT                        1  
#define WM9081_DAC_COMP_WIDTH                        1  
#define WM9081_DAC_COMPMODE                     0x0001  
#define WM9081_DAC_COMPMODE_MASK                0x0001  
#define WM9081_DAC_COMPMODE_SHIFT                    0  
#define WM9081_DAC_COMPMODE_WIDTH                    1  

#define WM9081_AIF_TRIS                         0x0200  
#define WM9081_AIF_TRIS_MASK                    0x0200  
#define WM9081_AIF_TRIS_SHIFT                        9  
#define WM9081_AIF_TRIS_WIDTH                        1  
#define WM9081_DAC_DAT_INV                      0x0100  
#define WM9081_DAC_DAT_INV_MASK                 0x0100  
#define WM9081_DAC_DAT_INV_SHIFT                     8  
#define WM9081_DAC_DAT_INV_WIDTH                     1  
#define WM9081_AIF_BCLK_INV                     0x0080  
#define WM9081_AIF_BCLK_INV_MASK                0x0080  
#define WM9081_AIF_BCLK_INV_SHIFT                    7  
#define WM9081_AIF_BCLK_INV_WIDTH                    1  
#define WM9081_BCLK_DIR                         0x0040  
#define WM9081_BCLK_DIR_MASK                    0x0040  
#define WM9081_BCLK_DIR_SHIFT                        6  
#define WM9081_BCLK_DIR_WIDTH                        1  
#define WM9081_LRCLK_DIR                        0x0020  
#define WM9081_LRCLK_DIR_MASK                   0x0020  
#define WM9081_LRCLK_DIR_SHIFT                       5  
#define WM9081_LRCLK_DIR_WIDTH                       1  
#define WM9081_AIF_LRCLK_INV                    0x0010  
#define WM9081_AIF_LRCLK_INV_MASK               0x0010  
#define WM9081_AIF_LRCLK_INV_SHIFT                   4  
#define WM9081_AIF_LRCLK_INV_WIDTH                   1  
#define WM9081_AIF_WL_MASK                      0x000C  
#define WM9081_AIF_WL_SHIFT                          2  
#define WM9081_AIF_WL_WIDTH                          2  
#define WM9081_AIF_FMT_MASK                     0x0003  
#define WM9081_AIF_FMT_SHIFT                         0  
#define WM9081_AIF_FMT_WIDTH                         2  

#define WM9081_BCLK_DIV_MASK                    0x001F  
#define WM9081_BCLK_DIV_SHIFT                        0  
#define WM9081_BCLK_DIV_WIDTH                        5  

#define WM9081_LRCLK_RATE_MASK                  0x07FF  
#define WM9081_LRCLK_RATE_SHIFT                      0  
#define WM9081_LRCLK_RATE_WIDTH                     11  

#define WM9081_WSEQ_BUSY_EINT                   0x0004  
#define WM9081_WSEQ_BUSY_EINT_MASK              0x0004  
#define WM9081_WSEQ_BUSY_EINT_SHIFT                  2  
#define WM9081_WSEQ_BUSY_EINT_WIDTH                  1  
#define WM9081_TSHUT_EINT                       0x0001  
#define WM9081_TSHUT_EINT_MASK                  0x0001  
#define WM9081_TSHUT_EINT_SHIFT                      0  
#define WM9081_TSHUT_EINT_WIDTH                      1  

#define WM9081_IM_WSEQ_BUSY_EINT                0x0004  
#define WM9081_IM_WSEQ_BUSY_EINT_MASK           0x0004  
#define WM9081_IM_WSEQ_BUSY_EINT_SHIFT               2  
#define WM9081_IM_WSEQ_BUSY_EINT_WIDTH               1  
#define WM9081_IM_TSHUT_EINT                    0x0001  
#define WM9081_IM_TSHUT_EINT_MASK               0x0001  
#define WM9081_IM_TSHUT_EINT_SHIFT                   0  
#define WM9081_IM_TSHUT_EINT_WIDTH                   1  

#define WM9081_TSHUT_INV                        0x0001  
#define WM9081_TSHUT_INV_MASK                   0x0001  
#define WM9081_TSHUT_INV_SHIFT                       0  
#define WM9081_TSHUT_INV_WIDTH                       1  

#define WM9081_IRQ_POL                          0x8000  
#define WM9081_IRQ_POL_MASK                     0x8000  
#define WM9081_IRQ_POL_SHIFT                        15  
#define WM9081_IRQ_POL_WIDTH                         1  
#define WM9081_IRQ_OP_CTRL                      0x0001  
#define WM9081_IRQ_OP_CTRL_MASK                 0x0001  
#define WM9081_IRQ_OP_CTRL_SHIFT                     0  
#define WM9081_IRQ_OP_CTRL_WIDTH                     1  

#define WM9081_DAC_VOL_MASK                     0x00FF  
#define WM9081_DAC_VOL_SHIFT                         0  
#define WM9081_DAC_VOL_WIDTH                         8  

#define WM9081_DAC_MUTERATE                     0x0400  
#define WM9081_DAC_MUTERATE_MASK                0x0400  
#define WM9081_DAC_MUTERATE_SHIFT                   10  
#define WM9081_DAC_MUTERATE_WIDTH                    1  
#define WM9081_DAC_MUTEMODE                     0x0200  
#define WM9081_DAC_MUTEMODE_MASK                0x0200  
#define WM9081_DAC_MUTEMODE_SHIFT                    9  
#define WM9081_DAC_MUTEMODE_WIDTH                    1  
#define WM9081_DAC_MUTE                         0x0008  
#define WM9081_DAC_MUTE_MASK                    0x0008  
#define WM9081_DAC_MUTE_SHIFT                        3  
#define WM9081_DAC_MUTE_WIDTH                        1  
#define WM9081_DEEMPH_MASK                      0x0006  
#define WM9081_DEEMPH_SHIFT                          1  
#define WM9081_DEEMPH_WIDTH                          2  

#define WM9081_DRC_ENA                          0x8000  
#define WM9081_DRC_ENA_MASK                     0x8000  
#define WM9081_DRC_ENA_SHIFT                        15  
#define WM9081_DRC_ENA_WIDTH                         1  
#define WM9081_DRC_STARTUP_GAIN_MASK            0x07C0  
#define WM9081_DRC_STARTUP_GAIN_SHIFT                6  
#define WM9081_DRC_STARTUP_GAIN_WIDTH                5  
#define WM9081_DRC_FF_DLY                       0x0020  
#define WM9081_DRC_FF_DLY_MASK                  0x0020  
#define WM9081_DRC_FF_DLY_SHIFT                      5  
#define WM9081_DRC_FF_DLY_WIDTH                      1  
#define WM9081_DRC_QR                           0x0004  
#define WM9081_DRC_QR_MASK                      0x0004  
#define WM9081_DRC_QR_SHIFT                          2  
#define WM9081_DRC_QR_WIDTH                          1  
#define WM9081_DRC_ANTICLIP                     0x0002  
#define WM9081_DRC_ANTICLIP_MASK                0x0002  
#define WM9081_DRC_ANTICLIP_SHIFT                    1  
#define WM9081_DRC_ANTICLIP_WIDTH                    1  

#define WM9081_DRC_ATK_MASK                     0xF000  
#define WM9081_DRC_ATK_SHIFT                        12  
#define WM9081_DRC_ATK_WIDTH                         4  
#define WM9081_DRC_DCY_MASK                     0x0F00  
#define WM9081_DRC_DCY_SHIFT                         8  
#define WM9081_DRC_DCY_WIDTH                         4  
#define WM9081_DRC_QR_THR_MASK                  0x00C0  
#define WM9081_DRC_QR_THR_SHIFT                      6  
#define WM9081_DRC_QR_THR_WIDTH                      2  
#define WM9081_DRC_QR_DCY_MASK                  0x0030  
#define WM9081_DRC_QR_DCY_SHIFT                      4  
#define WM9081_DRC_QR_DCY_WIDTH                      2  
#define WM9081_DRC_MINGAIN_MASK                 0x000C  
#define WM9081_DRC_MINGAIN_SHIFT                     2  
#define WM9081_DRC_MINGAIN_WIDTH                     2  
#define WM9081_DRC_MAXGAIN_MASK                 0x0003  
#define WM9081_DRC_MAXGAIN_SHIFT                     0  
#define WM9081_DRC_MAXGAIN_WIDTH                     2  

#define WM9081_DRC_HI_COMP_MASK                 0x0038  
#define WM9081_DRC_HI_COMP_SHIFT                     3  
#define WM9081_DRC_HI_COMP_WIDTH                     3  
#define WM9081_DRC_LO_COMP_MASK                 0x0007  
#define WM9081_DRC_LO_COMP_SHIFT                     0  
#define WM9081_DRC_LO_COMP_WIDTH                     3  

#define WM9081_DRC_KNEE_IP_MASK                 0x07E0  
#define WM9081_DRC_KNEE_IP_SHIFT                     5  
#define WM9081_DRC_KNEE_IP_WIDTH                     6  
#define WM9081_DRC_KNEE_OP_MASK                 0x001F  
#define WM9081_DRC_KNEE_OP_SHIFT                     0  
#define WM9081_DRC_KNEE_OP_WIDTH                     5  

#define WM9081_WSEQ_ENA                         0x8000  
#define WM9081_WSEQ_ENA_MASK                    0x8000  
#define WM9081_WSEQ_ENA_SHIFT                       15  
#define WM9081_WSEQ_ENA_WIDTH                        1  
#define WM9081_WSEQ_ABORT                       0x0200  
#define WM9081_WSEQ_ABORT_MASK                  0x0200  
#define WM9081_WSEQ_ABORT_SHIFT                      9  
#define WM9081_WSEQ_ABORT_WIDTH                      1  
#define WM9081_WSEQ_START                       0x0100  
#define WM9081_WSEQ_START_MASK                  0x0100  
#define WM9081_WSEQ_START_SHIFT                      8  
#define WM9081_WSEQ_START_WIDTH                      1  
#define WM9081_WSEQ_START_INDEX_MASK            0x007F  
#define WM9081_WSEQ_START_INDEX_SHIFT                0  
#define WM9081_WSEQ_START_INDEX_WIDTH                7  

#define WM9081_WSEQ_CURRENT_INDEX_MASK          0x07F0  
#define WM9081_WSEQ_CURRENT_INDEX_SHIFT              4  
#define WM9081_WSEQ_CURRENT_INDEX_WIDTH              7  
#define WM9081_WSEQ_BUSY                        0x0001  
#define WM9081_WSEQ_BUSY_MASK                   0x0001  
#define WM9081_WSEQ_BUSY_SHIFT                       0  
#define WM9081_WSEQ_BUSY_WIDTH                       1  

#define WM9081_SPI_CFG                          0x0020  
#define WM9081_SPI_CFG_MASK                     0x0020  
#define WM9081_SPI_CFG_SHIFT                         5  
#define WM9081_SPI_CFG_WIDTH                         1  
#define WM9081_SPI_4WIRE                        0x0010  
#define WM9081_SPI_4WIRE_MASK                   0x0010  
#define WM9081_SPI_4WIRE_SHIFT                       4  
#define WM9081_SPI_4WIRE_WIDTH                       1  
#define WM9081_ARA_ENA                          0x0008  
#define WM9081_ARA_ENA_MASK                     0x0008  
#define WM9081_ARA_ENA_SHIFT                         3  
#define WM9081_ARA_ENA_WIDTH                         1  
#define WM9081_AUTO_INC                         0x0002  
#define WM9081_AUTO_INC_MASK                    0x0002  
#define WM9081_AUTO_INC_SHIFT                        1  
#define WM9081_AUTO_INC_WIDTH                        1  

#define WM9081_EQ_B1_GAIN_MASK                  0xF800  
#define WM9081_EQ_B1_GAIN_SHIFT                     11  
#define WM9081_EQ_B1_GAIN_WIDTH                      5  
#define WM9081_EQ_B2_GAIN_MASK                  0x07C0  
#define WM9081_EQ_B2_GAIN_SHIFT                      6  
#define WM9081_EQ_B2_GAIN_WIDTH                      5  
#define WM9081_EQ_B4_GAIN_MASK                  0x003E  
#define WM9081_EQ_B4_GAIN_SHIFT                      1  
#define WM9081_EQ_B4_GAIN_WIDTH                      5  
#define WM9081_EQ_ENA                           0x0001  
#define WM9081_EQ_ENA_MASK                      0x0001  
#define WM9081_EQ_ENA_SHIFT                          0  
#define WM9081_EQ_ENA_WIDTH                          1  

#define WM9081_EQ_B3_GAIN_MASK                  0xF800  
#define WM9081_EQ_B3_GAIN_SHIFT                     11  
#define WM9081_EQ_B3_GAIN_WIDTH                      5  
#define WM9081_EQ_B5_GAIN_MASK                  0x07C0  
#define WM9081_EQ_B5_GAIN_SHIFT                      6  
#define WM9081_EQ_B5_GAIN_WIDTH                      5  

#define WM9081_EQ_B1_A_MASK                     0xFFFF  
#define WM9081_EQ_B1_A_SHIFT                         0  
#define WM9081_EQ_B1_A_WIDTH                        16  

#define WM9081_EQ_B1_B_MASK                     0xFFFF  
#define WM9081_EQ_B1_B_SHIFT                         0  
#define WM9081_EQ_B1_B_WIDTH                        16  

#define WM9081_EQ_B1_PG_MASK                    0xFFFF  
#define WM9081_EQ_B1_PG_SHIFT                        0  
#define WM9081_EQ_B1_PG_WIDTH                       16  

#define WM9081_EQ_B2_A_MASK                     0xFFFF  
#define WM9081_EQ_B2_A_SHIFT                         0  
#define WM9081_EQ_B2_A_WIDTH                        16  

#define WM9081_EQ_B2_B_MASK                     0xFFFF  
#define WM9081_EQ_B2_B_SHIFT                         0  
#define WM9081_EQ_B2_B_WIDTH                        16  

#define WM9081_EQ_B2_C_MASK                     0xFFFF  
#define WM9081_EQ_B2_C_SHIFT                         0  
#define WM9081_EQ_B2_C_WIDTH                        16  

#define WM9081_EQ_B2_PG_MASK                    0xFFFF  
#define WM9081_EQ_B2_PG_SHIFT                        0  
#define WM9081_EQ_B2_PG_WIDTH                       16  

#define WM9081_EQ_B4_A_MASK                     0xFFFF  
#define WM9081_EQ_B4_A_SHIFT                         0  
#define WM9081_EQ_B4_A_WIDTH                        16  

#define WM9081_EQ_B4_B_MASK                     0xFFFF  
#define WM9081_EQ_B4_B_SHIFT                         0  
#define WM9081_EQ_B4_B_WIDTH                        16  

#define WM9081_EQ_B4_C_MASK                     0xFFFF  
#define WM9081_EQ_B4_C_SHIFT                         0  
#define WM9081_EQ_B4_C_WIDTH                        16  

#define WM9081_EQ_B4_PG_MASK                    0xFFFF  
#define WM9081_EQ_B4_PG_SHIFT                        0  
#define WM9081_EQ_B4_PG_WIDTH                       16  

#define WM9081_EQ_B3_A_MASK                     0xFFFF  
#define WM9081_EQ_B3_A_SHIFT                         0  
#define WM9081_EQ_B3_A_WIDTH                        16  

#define WM9081_EQ_B3_B_MASK                     0xFFFF  
#define WM9081_EQ_B3_B_SHIFT                         0  
#define WM9081_EQ_B3_B_WIDTH                        16  

#define WM9081_EQ_B3_C_MASK                     0xFFFF  
#define WM9081_EQ_B3_C_SHIFT                         0  
#define WM9081_EQ_B3_C_WIDTH                        16  

#define WM9081_EQ_B3_PG_MASK                    0xFFFF  
#define WM9081_EQ_B3_PG_SHIFT                        0  
#define WM9081_EQ_B3_PG_WIDTH                       16  

#define WM9081_EQ_B5_A_MASK                     0xFFFF  
#define WM9081_EQ_B5_A_SHIFT                         0  
#define WM9081_EQ_B5_A_WIDTH                        16  

#define WM9081_EQ_B5_B_MASK                     0xFFFF  
#define WM9081_EQ_B5_B_SHIFT                         0  
#define WM9081_EQ_B5_B_WIDTH                        16  

#define WM9081_EQ_B5_PG_MASK                    0xFFFF  
#define WM9081_EQ_B5_PG_SHIFT                        0  
#define WM9081_EQ_B5_PG_WIDTH                       16  


#endif
