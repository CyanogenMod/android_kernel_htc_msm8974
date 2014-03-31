/*
 * wm8990.h  --  audio driver for WM8990
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __WM8990REGISTERDEFS_H__
#define __WM8990REGISTERDEFS_H__

#define WM8990_RESET                            0x00
#define WM8990_POWER_MANAGEMENT_1               0x01
#define WM8990_POWER_MANAGEMENT_2               0x02
#define WM8990_POWER_MANAGEMENT_3               0x03
#define WM8990_AUDIO_INTERFACE_1                0x04
#define WM8990_AUDIO_INTERFACE_2                0x05
#define WM8990_CLOCKING_1                       0x06
#define WM8990_CLOCKING_2                       0x07
#define WM8990_AUDIO_INTERFACE_3                0x08
#define WM8990_AUDIO_INTERFACE_4                0x09
#define WM8990_DAC_CTRL                         0x0A
#define WM8990_LEFT_DAC_DIGITAL_VOLUME          0x0B
#define WM8990_RIGHT_DAC_DIGITAL_VOLUME         0x0C
#define WM8990_DIGITAL_SIDE_TONE                0x0D
#define WM8990_ADC_CTRL                         0x0E
#define WM8990_LEFT_ADC_DIGITAL_VOLUME          0x0F
#define WM8990_RIGHT_ADC_DIGITAL_VOLUME         0x10
#define WM8990_GPIO_CTRL_1                      0x12
#define WM8990_GPIO1_GPIO2                      0x13
#define WM8990_GPIO3_GPIO4                      0x14
#define WM8990_GPIO5_GPIO6                      0x15
#define WM8990_GPIOCTRL_2                       0x16
#define WM8990_GPIO_POL                         0x17
#define WM8990_LEFT_LINE_INPUT_1_2_VOLUME       0x18
#define WM8990_LEFT_LINE_INPUT_3_4_VOLUME       0x19
#define WM8990_RIGHT_LINE_INPUT_1_2_VOLUME      0x1A
#define WM8990_RIGHT_LINE_INPUT_3_4_VOLUME      0x1B
#define WM8990_LEFT_OUTPUT_VOLUME               0x1C
#define WM8990_RIGHT_OUTPUT_VOLUME              0x1D
#define WM8990_LINE_OUTPUTS_VOLUME              0x1E
#define WM8990_OUT3_4_VOLUME                    0x1F
#define WM8990_LEFT_OPGA_VOLUME                 0x20
#define WM8990_RIGHT_OPGA_VOLUME                0x21
#define WM8990_SPEAKER_VOLUME                   0x22
#define WM8990_CLASSD1                          0x23
#define WM8990_CLASSD3                          0x25
#define WM8990_CLASSD4                          0x26
#define WM8990_INPUT_MIXER1                     0x27
#define WM8990_INPUT_MIXER2                     0x28
#define WM8990_INPUT_MIXER3                     0x29
#define WM8990_INPUT_MIXER4                     0x2A
#define WM8990_INPUT_MIXER5                     0x2B
#define WM8990_INPUT_MIXER6                     0x2C
#define WM8990_OUTPUT_MIXER1                    0x2D
#define WM8990_OUTPUT_MIXER2                    0x2E
#define WM8990_OUTPUT_MIXER3                    0x2F
#define WM8990_OUTPUT_MIXER4                    0x30
#define WM8990_OUTPUT_MIXER5                    0x31
#define WM8990_OUTPUT_MIXER6                    0x32
#define WM8990_OUT3_4_MIXER                     0x33
#define WM8990_LINE_MIXER1                      0x34
#define WM8990_LINE_MIXER2                      0x35
#define WM8990_SPEAKER_MIXER                    0x36
#define WM8990_ADDITIONAL_CONTROL               0x37
#define WM8990_ANTIPOP1                         0x38
#define WM8990_ANTIPOP2                         0x39
#define WM8990_MICBIAS                          0x3A
#define WM8990_PLL1                             0x3C
#define WM8990_PLL2                             0x3D
#define WM8990_PLL3                             0x3E
#define WM8990_INTDRIVBITS			0x3F

#define WM8990_EXT_ACCESS_ENA			0x75
#define WM8990_EXT_CTL1				0x7a


#define WM8990_SW_RESET_CHIP_ID_MASK            0xFFFF  

#define WM8990_SPK_ENA                          0x1000  
#define WM8990_SPK_ENA_BIT			12
#define WM8990_OUT3_ENA                         0x0800  
#define WM8990_OUT3_ENA_BIT			11
#define WM8990_OUT4_ENA                         0x0400  
#define WM8990_OUT4_ENA_BIT			10
#define WM8990_LOUT_ENA                         0x0200  
#define WM8990_LOUT_ENA_BIT			9
#define WM8990_ROUT_ENA                         0x0100  
#define WM8990_ROUT_ENA_BIT			8
#define WM8990_MICBIAS_ENA                      0x0010  
#define WM8990_MICBIAS_ENA_BIT			4
#define WM8990_VMID_MODE_MASK                   0x0006  
#define WM8990_VREF_ENA                         0x0001  
#define WM8990_VREF_ENA_BIT			0

#define WM8990_PLL_ENA                          0x8000  
#define WM8990_PLL_ENA_BIT			15
#define WM8990_TSHUT_ENA                        0x4000  
#define WM8990_TSHUT_ENA_BIT			14
#define WM8990_TSHUT_OPDIS                      0x2000  
#define WM8990_TSHUT_OPDIS_BIT			13
#define WM8990_OPCLK_ENA                        0x0800  
#define WM8990_OPCLK_ENA_BIT			11
#define WM8990_AINL_ENA                         0x0200  
#define WM8990_AINL_ENA_BIT			9
#define WM8990_AINR_ENA                         0x0100  
#define WM8990_AINR_ENA_BIT			8
#define WM8990_LIN34_ENA                        0x0080  
#define WM8990_LIN34_ENA_BIT			7
#define WM8990_LIN12_ENA                        0x0040  
#define WM8990_LIN12_ENA_BIT			6
#define WM8990_RIN34_ENA                        0x0020  
#define WM8990_RIN34_ENA_BIT			5
#define WM8990_RIN12_ENA                        0x0010  
#define WM8990_RIN12_ENA_BIT			4
#define WM8990_ADCL_ENA                         0x0002  
#define WM8990_ADCL_ENA_BIT			1
#define WM8990_ADCR_ENA                         0x0001  
#define WM8990_ADCR_ENA_BIT			0

#define WM8990_LON_ENA                          0x2000  
#define WM8990_LON_ENA_BIT			13
#define WM8990_LOP_ENA                          0x1000  
#define WM8990_LOP_ENA_BIT			12
#define WM8990_RON_ENA                          0x0800  
#define WM8990_RON_ENA_BIT			11
#define WM8990_ROP_ENA                          0x0400  
#define WM8990_ROP_ENA_BIT			10
#define WM8990_LOPGA_ENA                        0x0080  
#define WM8990_LOPGA_ENA_BIT			7
#define WM8990_ROPGA_ENA                        0x0040  
#define WM8990_ROPGA_ENA_BIT			6
#define WM8990_LOMIX_ENA                        0x0020  
#define WM8990_LOMIX_ENA_BIT			5
#define WM8990_ROMIX_ENA                        0x0010  
#define WM8990_ROMIX_ENA_BIT			4
#define WM8990_DACL_ENA                         0x0002  
#define WM8990_DACL_ENA_BIT			1
#define WM8990_DACR_ENA                         0x0001  
#define WM8990_DACR_ENA_BIT			0

#define WM8990_AIFADCL_SRC                      0x8000  
#define WM8990_AIFADCR_SRC                      0x4000  
#define WM8990_AIFADC_TDM                       0x2000  
#define WM8990_AIFADC_TDM_CHAN                  0x1000  
#define WM8990_AIF_BCLK_INV                     0x0100  
#define WM8990_AIF_LRCLK_INV                    0x0080  
#define WM8990_AIF_WL_MASK                      0x0060  
#define WM8990_AIF_WL_16BITS			(0 << 5)
#define WM8990_AIF_WL_20BITS			(1 << 5)
#define WM8990_AIF_WL_24BITS			(2 << 5)
#define WM8990_AIF_WL_32BITS			(3 << 5)
#define WM8990_AIF_FMT_MASK                     0x0018  
#define WM8990_AIF_TMF_RIGHTJ			(0 << 3)
#define WM8990_AIF_TMF_LEFTJ			(1 << 3)
#define WM8990_AIF_TMF_I2S			(2 << 3)
#define WM8990_AIF_TMF_DSP			(3 << 3)

#define WM8990_DACL_SRC                         0x8000  
#define WM8990_DACR_SRC                         0x4000  
#define WM8990_AIFDAC_TDM                       0x2000  
#define WM8990_AIFDAC_TDM_CHAN                  0x1000  
#define WM8990_DAC_BOOST_MASK                   0x0C00  
#define WM8990_DAC_COMP                         0x0010  
#define WM8990_DAC_COMPMODE                     0x0008  
#define WM8990_ADC_COMP                         0x0004  
#define WM8990_ADC_COMPMODE                     0x0002  
#define WM8990_LOOPBACK                         0x0001  

#define WM8990_TOCLK_RATE                       0x8000  
#define WM8990_TOCLK_ENA                        0x4000  
#define WM8990_OPCLKDIV_MASK                    0x1E00  
#define WM8990_DCLKDIV_MASK                     0x01C0  
#define WM8990_BCLK_DIV_MASK                    0x001E  
#define WM8990_BCLK_DIV_1			(0x0 << 1)
#define WM8990_BCLK_DIV_1_5			(0x1 << 1)
#define WM8990_BCLK_DIV_2			(0x2 << 1)
#define WM8990_BCLK_DIV_3			(0x3 << 1)
#define WM8990_BCLK_DIV_4			(0x4 << 1)
#define WM8990_BCLK_DIV_5_5			(0x5 << 1)
#define WM8990_BCLK_DIV_6			(0x6 << 1)
#define WM8990_BCLK_DIV_8			(0x7 << 1)
#define WM8990_BCLK_DIV_11			(0x8 << 1)
#define WM8990_BCLK_DIV_12			(0x9 << 1)
#define WM8990_BCLK_DIV_16			(0xA << 1)
#define WM8990_BCLK_DIV_22			(0xB << 1)
#define WM8990_BCLK_DIV_24			(0xC << 1)
#define WM8990_BCLK_DIV_32			(0xD << 1)
#define WM8990_BCLK_DIV_44			(0xE << 1)
#define WM8990_BCLK_DIV_48			(0xF << 1)

#define WM8990_MCLK_SRC                         0x8000  
#define WM8990_SYSCLK_SRC                       0x4000  
#define WM8990_CLK_FORCE                        0x2000  
#define WM8990_MCLK_DIV_MASK                    0x1800  
#define WM8990_MCLK_DIV_1			(0 << 11)
#define WM8990_MCLK_DIV_2			(2 << 11)
#define WM8990_MCLK_INV                         0x0400  
#define WM8990_ADC_CLKDIV_MASK                  0x00E0  
#define WM8990_ADC_CLKDIV_1			(0 << 5)
#define WM8990_ADC_CLKDIV_1_5			(1 << 5)
#define WM8990_ADC_CLKDIV_2			(2 << 5)
#define WM8990_ADC_CLKDIV_3			(3 << 5)
#define WM8990_ADC_CLKDIV_4			(4 << 5)
#define WM8990_ADC_CLKDIV_5_5			(5 << 5)
#define WM8990_ADC_CLKDIV_6			(6 << 5)
#define WM8990_DAC_CLKDIV_MASK                  0x001C  
#define WM8990_DAC_CLKDIV_1			(0 << 2)
#define WM8990_DAC_CLKDIV_1_5			(1 << 2)
#define WM8990_DAC_CLKDIV_2			(2 << 2)
#define WM8990_DAC_CLKDIV_3			(3 << 2)
#define WM8990_DAC_CLKDIV_4			(4 << 2)
#define WM8990_DAC_CLKDIV_5_5			(5 << 2)
#define WM8990_DAC_CLKDIV_6			(6 << 2)

#define WM8990_AIF_MSTR1                        0x8000  
#define WM8990_AIF_MSTR2                        0x4000  
#define WM8990_AIF_SEL                          0x2000  
#define WM8990_ADCLRC_DIR                       0x0800  
#define WM8990_ADCLRC_RATE_MASK                 0x07FF  

#define WM8990_ALRCGPIO1                        0x8000  
#define WM8990_ALRCBGPIO6                       0x4000  
#define WM8990_AIF_TRIS                         0x2000  
#define WM8990_DACLRC_DIR                       0x0800  
#define WM8990_DACLRC_RATE_MASK                 0x07FF  

#define WM8990_AIF_LRCLKRATE                    0x0400  
#define WM8990_DAC_MONO                         0x0200  
#define WM8990_DAC_SB_FILT                      0x0100  
#define WM8990_DAC_MUTERATE                     0x0080  
#define WM8990_DAC_MUTEMODE                     0x0040  
#define WM8990_DEEMP_MASK                       0x0030  
#define WM8990_DAC_MUTE                         0x0004  
#define WM8990_DACL_DATINV                      0x0002  
#define WM8990_DACR_DATINV                      0x0001  

#define WM8990_DAC_VU                           0x0100  
#define WM8990_DACL_VOL_MASK                    0x00FF  
#define WM8990_DACL_VOL_SHIFT			0
#define WM8990_DAC_VU                           0x0100  
#define WM8990_DACR_VOL_MASK                    0x00FF  
#define WM8990_DACR_VOL_SHIFT			0
#define WM8990_ADCL_DAC_SVOL_MASK               0x0F  
#define WM8990_ADCL_DAC_SVOL_SHIFT		9
#define WM8990_ADCR_DAC_SVOL_MASK               0x0F  
#define WM8990_ADCR_DAC_SVOL_SHIFT		5
#define WM8990_ADC_TO_DACL_MASK                 0x03  
#define WM8990_ADC_TO_DACL_SHIFT		2
#define WM8990_ADC_TO_DACR_MASK                 0x03  
#define WM8990_ADC_TO_DACR_SHIFT		0

#define WM8990_ADC_HPF_ENA                      0x0100  
#define WM8990_ADC_HPF_ENA_BIT			8
#define WM8990_ADC_HPF_CUT_MASK                 0x03  
#define WM8990_ADC_HPF_CUT_SHIFT		5
#define WM8990_ADCL_DATINV                      0x0002  
#define WM8990_ADCL_DATINV_BIT			1
#define WM8990_ADCR_DATINV                      0x0001  
#define WM8990_ADCR_DATINV_BIT			0

#define WM8990_ADC_VU                           0x0100  
#define WM8990_ADCL_VOL_MASK                    0x00FF  
#define WM8990_ADCL_VOL_SHIFT			0

#define WM8990_ADC_VU                           0x0100  
#define WM8990_ADCR_VOL_MASK                    0x00FF  
#define WM8990_ADCR_VOL_SHIFT			0

#define WM8990_IRQ                              0x1000  
#define WM8990_TEMPOK                           0x0800  
#define WM8990_MICSHRT                          0x0400  
#define WM8990_MICDET                           0x0200  
#define WM8990_PLL_LCK                          0x0100  
#define WM8990_GPI8_STATUS                      0x0080  
#define WM8990_GPI7_STATUS                      0x0040  
#define WM8990_GPIO6_STATUS                     0x0020  
#define WM8990_GPIO5_STATUS                     0x0010  
#define WM8990_GPIO4_STATUS                     0x0008  
#define WM8990_GPIO3_STATUS                     0x0004  
#define WM8990_GPIO2_STATUS                     0x0002  
#define WM8990_GPIO1_STATUS                     0x0001  

#define WM8990_GPIO2_DEB_ENA                    0x8000  
#define WM8990_GPIO2_IRQ_ENA                    0x4000  
#define WM8990_GPIO2_PU                         0x2000  
#define WM8990_GPIO2_PD                         0x1000  
#define WM8990_GPIO2_SEL_MASK                   0x0F00  
#define WM8990_GPIO1_DEB_ENA                    0x0080  
#define WM8990_GPIO1_IRQ_ENA                    0x0040  
#define WM8990_GPIO1_PU                         0x0020  
#define WM8990_GPIO1_PD                         0x0010  
#define WM8990_GPIO1_SEL_MASK                   0x000F  

#define WM8990_GPIO4_DEB_ENA                    0x8000  
#define WM8990_GPIO4_IRQ_ENA                    0x4000  
#define WM8990_GPIO4_PU                         0x2000  
#define WM8990_GPIO4_PD                         0x1000  
#define WM8990_GPIO4_SEL_MASK                   0x0F00  
#define WM8990_GPIO3_DEB_ENA                    0x0080  
#define WM8990_GPIO3_IRQ_ENA                    0x0040  
#define WM8990_GPIO3_PU                         0x0020  
#define WM8990_GPIO3_PD                         0x0010  
#define WM8990_GPIO3_SEL_MASK                   0x000F  

#define WM8990_GPIO6_DEB_ENA                    0x8000  
#define WM8990_GPIO6_IRQ_ENA                    0x4000  
#define WM8990_GPIO6_PU                         0x2000  
#define WM8990_GPIO6_PD                         0x1000  
#define WM8990_GPIO6_SEL_MASK                   0x0F00  
#define WM8990_GPIO5_DEB_ENA                    0x0080  
#define WM8990_GPIO5_IRQ_ENA                    0x0040  
#define WM8990_GPIO5_PU                         0x0020  
#define WM8990_GPIO5_PD                         0x0010  
#define WM8990_GPIO5_SEL_MASK                   0x000F  

#define WM8990_RD_3W_ENA                        0x8000  
#define WM8990_MODE_3W4W                        0x4000  
#define WM8990_TEMPOK_IRQ_ENA                   0x0800  
#define WM8990_MICSHRT_IRQ_ENA                  0x0400  
#define WM8990_MICDET_IRQ_ENA                   0x0200  
#define WM8990_PLL_LCK_IRQ_ENA                  0x0100  
#define WM8990_GPI8_DEB_ENA                     0x0080  
#define WM8990_GPI8_IRQ_ENA                     0x0040  
#define WM8990_GPI8_ENA                         0x0010  
#define WM8990_GPI7_DEB_ENA                     0x0008  
#define WM8990_GPI7_IRQ_ENA                     0x0004  
#define WM8990_GPI7_ENA                         0x0001  

#define WM8990_IRQ_INV                          0x1000  
#define WM8990_TEMPOK_POL                       0x0800  
#define WM8990_MICSHRT_POL                      0x0400  
#define WM8990_MICDET_POL                       0x0200  
#define WM8990_PLL_LCK_POL                      0x0100  
#define WM8990_GPI8_POL                         0x0080  
#define WM8990_GPI7_POL                         0x0040  
#define WM8990_GPIO6_POL                        0x0020  
#define WM8990_GPIO5_POL                        0x0010  
#define WM8990_GPIO4_POL                        0x0008  
#define WM8990_GPIO3_POL                        0x0004  
#define WM8990_GPIO2_POL                        0x0002  
#define WM8990_GPIO1_POL                        0x0001  

#define WM8990_IPVU                             0x0100  
#define WM8990_LI12MUTE                         0x0080  
#define WM8990_LI12MUTE_BIT			7
#define WM8990_LI12ZC                           0x0040  
#define WM8990_LI12ZC_BIT			6
#define WM8990_LIN12VOL_MASK                    0x001F  
#define WM8990_LIN12VOL_SHIFT			0
#define WM8990_IPVU                             0x0100  
#define WM8990_LI34MUTE                         0x0080  
#define WM8990_LI34MUTE_BIT			7
#define WM8990_LI34ZC                           0x0040  
#define WM8990_LI34ZC_BIT			6
#define WM8990_LIN34VOL_MASK                    0x001F  
#define WM8990_LIN34VOL_SHIFT			0

#define WM8990_IPVU                             0x0100  
#define WM8990_RI12MUTE                         0x0080  
#define WM8990_RI12MUTE_BIT			7
#define WM8990_RI12ZC                           0x0040  
#define WM8990_RI12ZC_BIT			6
#define WM8990_RIN12VOL_MASK                    0x001F  
#define WM8990_RIN12VOL_SHIFT			0

#define WM8990_IPVU                             0x0100  
#define WM8990_RI34MUTE                         0x0080  
#define WM8990_RI34MUTE_BIT			7
#define WM8990_RI34ZC                           0x0040  
#define WM8990_RI34ZC_BIT			6
#define WM8990_RIN34VOL_MASK                    0x001F  
#define WM8990_RIN34VOL_SHIFT			0

#define WM8990_OPVU                             0x0100  
#define WM8990_LOZC                             0x0080  
#define WM8990_LOZC_BIT				7
#define WM8990_LOUTVOL_MASK                     0x007F  
#define WM8990_LOUTVOL_SHIFT			0
#define WM8990_OPVU                             0x0100  
#define WM8990_ROZC                             0x0080  
#define WM8990_ROZC_BIT				7
#define WM8990_ROUTVOL_MASK                     0x007F  
#define WM8990_ROUTVOL_SHIFT			0
#define WM8990_LONMUTE                          0x0040  
#define WM8990_LONMUTE_BIT			6
#define WM8990_LOPMUTE                          0x0020  
#define WM8990_LOPMUTE_BIT			5
#define WM8990_LOATTN                           0x0010  
#define WM8990_LOATTN_BIT			4
#define WM8990_RONMUTE                          0x0004  
#define WM8990_RONMUTE_BIT			2
#define WM8990_ROPMUTE                          0x0002  
#define WM8990_ROPMUTE_BIT			1
#define WM8990_ROATTN                           0x0001  
#define WM8990_ROATTN_BIT			0

#define WM8990_OUT3MUTE                         0x0020  
#define WM8990_OUT3MUTE_BIT			5
#define WM8990_OUT3ATTN                         0x0010  
#define WM8990_OUT3ATTN_BIT			4
#define WM8990_OUT4MUTE                         0x0002  
#define WM8990_OUT4MUTE_BIT			1
#define WM8990_OUT4ATTN                         0x0001  
#define WM8990_OUT4ATTN_BIT			0

#define WM8990_OPVU                             0x0100  
#define WM8990_LOPGAZC                          0x0080  
#define WM8990_LOPGAZC_BIT			7
#define WM8990_LOPGAVOL_MASK                    0x007F  
#define WM8990_LOPGAVOL_SHIFT			0

#define WM8990_OPVU                             0x0100  
#define WM8990_ROPGAZC                          0x0080  
#define WM8990_ROPGAZC_BIT			7
#define WM8990_ROPGAVOL_MASK                    0x007F  
#define WM8990_ROPGAVOL_SHIFT			0
#define WM8990_SPKATTN_MASK                      0x0003  
#define WM8990_SPKATTN_SHIFT			 0

#define WM8990_CDMODE                           0x0100  
#define WM8990_CDMODE_BIT			8

#define WM8990_DCGAIN_MASK                      0x0007  
#define WM8990_DCGAIN_SHIFT			3
#define WM8990_ACGAIN_MASK                      0x0007  
#define WM8990_ACGAIN_SHIFT			0

#define WM8990_SPKZC_MASK                       0x0001  
#define WM8990_SPKZC_SHIFT                           7  
#define WM8990_SPKVOL_MASK                      0x007F  
#define WM8990_SPKVOL_SHIFT                          0  

#define WM8990_AINLMODE_MASK                    0x000C  
#define WM8990_AINLMODE_SHIFT			2
#define WM8990_AINRMODE_MASK                    0x0003  
#define WM8990_AINRMODE_SHIFT			0

#define WM8990_LMP4				0x0080	
#define WM8990_LMP4_BIT                         7	
#define WM8990_LMN3                             0x0040  
#define WM8990_LMN3_BIT                         6       
#define WM8990_LMP2                             0x0020  
#define WM8990_LMP2_BIT                         5       
#define WM8990_LMN1                             0x0010  
#define WM8990_LMN1_BIT                         4       
#define WM8990_RMP4                             0x0008  
#define WM8990_RMP4_BIT                         3       
#define WM8990_RMN3                             0x0004  
#define WM8990_RMN3_BIT                         2       
#define WM8990_RMP2                             0x0002  
#define WM8990_RMP2_BIT                         1       
#define WM8990_RMN1                             0x0001  
#define WM8990_RMN1_BIT                         0       

#define WM8990_L34MNB                           0x0100  
#define WM8990_L34MNB_BIT			8
#define WM8990_L34MNBST                         0x0080  
#define WM8990_L34MNBST_BIT			7
#define WM8990_L12MNB                           0x0020  
#define WM8990_L12MNB_BIT			5
#define WM8990_L12MNBST                         0x0010  
#define WM8990_L12MNBST_BIT			4
#define WM8990_LDBVOL_MASK                      0x0007  
#define WM8990_LDBVOL_SHIFT			0

#define WM8990_R34MNB                           0x0100  
#define WM8990_R34MNB_BIT			8
#define WM8990_R34MNBST                         0x0080  
#define WM8990_R34MNBST_BIT			7
#define WM8990_R12MNB                           0x0020  
#define WM8990_R12MNB_BIT			5
#define WM8990_R12MNBST                         0x0010  
#define WM8990_R12MNBST_BIT			4
#define WM8990_RDBVOL_MASK                      0x0007  
#define WM8990_RDBVOL_SHIFT			0

#define WM8990_LI2BVOL_MASK                     0x07  
#define WM8990_LI2BVOL_SHIFT			6
#define WM8990_LR4BVOL_MASK                     0x07  
#define WM8990_LR4BVOL_SHIFT			3
#define WM8990_LL4BVOL_MASK                     0x07  
#define WM8990_LL4BVOL_SHIFT			0

#define WM8990_RI2BVOL_MASK                     0x07  
#define WM8990_RI2BVOL_SHIFT			6
#define WM8990_RL4BVOL_MASK                     0x07  
#define WM8990_RL4BVOL_SHIFT			3
#define WM8990_RR4BVOL_MASK                     0x07  
#define WM8990_RR4BVOL_SHIFT			0

#define WM8990_LRBLO                            0x0080  
#define WM8990_LRBLO_BIT			7
#define WM8990_LLBLO                            0x0040  
#define WM8990_LLBLO_BIT			6
#define WM8990_LRI3LO                           0x0020  
#define WM8990_LRI3LO_BIT			5
#define WM8990_LLI3LO                           0x0010  
#define WM8990_LLI3LO_BIT			4
#define WM8990_LR12LO                           0x0008  
#define WM8990_LR12LO_BIT			3
#define WM8990_LL12LO                           0x0004  
#define WM8990_LL12LO_BIT			2
#define WM8990_LDLO                             0x0001  
#define WM8990_LDLO_BIT				0

#define WM8990_RLBRO                            0x0080  
#define WM8990_RLBRO_BIT			7
#define WM8990_RRBRO                            0x0040  
#define WM8990_RRBRO_BIT			6
#define WM8990_RLI3RO                           0x0020  
#define WM8990_RLI3RO_BIT			5
#define WM8990_RRI3RO                           0x0010  
#define WM8990_RRI3RO_BIT			4
#define WM8990_RL12RO                           0x0008  
#define WM8990_RL12RO_BIT			3
#define WM8990_RR12RO                           0x0004  
#define WM8990_RR12RO_BIT			2
#define WM8990_RDRO                             0x0001  
#define WM8990_RDRO_BIT				0

#define WM8990_LLI3LOVOL_MASK                   0x07  
#define WM8990_LLI3LOVOL_SHIFT			6
#define WM8990_LR12LOVOL_MASK                   0x07  
#define WM8990_LR12LOVOL_SHIFT			3
#define WM8990_LL12LOVOL_MASK                   0x07  
#define WM8990_LL12LOVOL_SHIFT			0

#define WM8990_RRI3ROVOL_MASK                   0x07  
#define WM8990_RRI3ROVOL_SHIFT			6
#define WM8990_RL12ROVOL_MASK                   0x07  
#define WM8990_RL12ROVOL_SHIFT			3
#define WM8990_RR12ROVOL_MASK                   0x07  
#define WM8990_RR12ROVOL_SHIFT			0

#define WM8990_LRI3LOVOL_MASK                   0x07  
#define WM8990_LRI3LOVOL_SHIFT			6
#define WM8990_LRBLOVOL_MASK                    0x07  
#define WM8990_LRBLOVOL_SHIFT			3
#define WM8990_LLBLOVOL_MASK                    0x07  
#define WM8990_LLBLOVOL_SHIFT			0

#define WM8990_RLI3ROVOL_MASK                   0x07  
#define WM8990_RLI3ROVOL_SHIFT			6
#define WM8990_RLBROVOL_MASK                    0x07  
#define WM8990_RLBROVOL_SHIFT			3
#define WM8990_RRBROVOL_MASK                    0x07  
#define WM8990_RRBROVOL_SHIFT			0

#define WM8990_VSEL_MASK                        0x0180  
#define WM8990_LI4O3                            0x0020  
#define WM8990_LI4O3_BIT			5
#define WM8990_LPGAO3                           0x0010  
#define WM8990_LPGAO3_BIT			4
#define WM8990_RI4O4                            0x0002  
#define WM8990_RI4O4_BIT			1
#define WM8990_RPGAO4                           0x0001  
#define WM8990_RPGAO4_BIT			0
#define WM8990_LLOPGALON                        0x0040  
#define WM8990_LLOPGALON_BIT			6
#define WM8990_LROPGALON                        0x0020  
#define WM8990_LROPGALON_BIT			5
#define WM8990_LOPLON                           0x0010  
#define WM8990_LOPLON_BIT			4
#define WM8990_LR12LOP                          0x0004  
#define WM8990_LR12LOP_BIT			2
#define WM8990_LL12LOP                          0x0002  
#define WM8990_LL12LOP_BIT			1
#define WM8990_LLOPGALOP                        0x0001  
#define WM8990_LLOPGALOP_BIT			0
#define WM8990_RROPGARON                        0x0040  
#define WM8990_RROPGARON_BIT			6
#define WM8990_RLOPGARON                        0x0020  
#define WM8990_RLOPGARON_BIT			5
#define WM8990_ROPRON                           0x0010  
#define WM8990_ROPRON_BIT			4
#define WM8990_RL12ROP                          0x0004  
#define WM8990_RL12ROP_BIT			2
#define WM8990_RR12ROP                          0x0002  
#define WM8990_RR12ROP_BIT			1
#define WM8990_RROPGAROP                        0x0001  
#define WM8990_RROPGAROP_BIT			0

#define WM8990_LB2SPK                           0x0080  
#define WM8990_LB2SPK_BIT			7
#define WM8990_RB2SPK                           0x0040  
#define WM8990_RB2SPK_BIT			6
#define WM8990_LI2SPK                           0x0020  
#define WM8990_LI2SPK_BIT			5
#define WM8990_RI2SPK                           0x0010  
#define WM8990_RI2SPK_BIT			4
#define WM8990_LOPGASPK                         0x0008  
#define WM8990_LOPGASPK_BIT			3
#define WM8990_ROPGASPK                         0x0004  
#define WM8990_ROPGASPK_BIT			2
#define WM8990_LDSPK                            0x0002  
#define WM8990_LDSPK_BIT			1
#define WM8990_RDSPK                            0x0001  
#define WM8990_RDSPK_BIT			0

#define WM8990_VROI                             0x0001  

#define WM8990_DIS_LLINE                        0x0020  
#define WM8990_DIS_RLINE                        0x0010  
#define WM8990_DIS_OUT3                         0x0008  
#define WM8990_DIS_OUT4                         0x0004  
#define WM8990_DIS_LOUT                         0x0002  
#define WM8990_DIS_ROUT                         0x0001  

#define WM8990_SOFTST                           0x0040  
#define WM8990_BUFIOEN                          0x0008  
#define WM8990_BUFDCOPEN                        0x0004  
#define WM8990_POBCTRL                          0x0002  
#define WM8990_VMIDTOG                          0x0001  

#define WM8990_MCDSCTH_MASK                     0x00C0  
#define WM8990_MCDTHR_MASK                      0x0038  
#define WM8990_MCD                              0x0004  
#define WM8990_MBSEL                            0x0001  

#define WM8990_SDM                              0x0080  
#define WM8990_PRESCALE                         0x0040  
#define WM8990_PLLN_MASK                        0x000F  

#define WM8990_PLLK1_MASK                       0x00FF  

#define WM8990_PLLK2_MASK                       0x00FF  

#define WM8990_INMIXL_PWR_BIT			0
#define WM8990_AINLMUX_PWR_BIT			1
#define WM8990_INMIXR_PWR_BIT			2
#define WM8990_AINRMUX_PWR_BIT			3

#define WM8990_MCLK_DIV 0
#define WM8990_DACCLK_DIV 1
#define WM8990_ADCCLK_DIV 2
#define WM8990_BCLK_DIV 3

#endif	
