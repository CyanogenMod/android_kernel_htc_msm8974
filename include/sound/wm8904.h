/*
 * Platform data for WM8904
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

#ifndef __MFD_WM8994_PDATA_H__
#define __MFD_WM8994_PDATA_H__

#define WM8904_GPIO_NO_CONFIG 0x8000

#define WM8904_MICDET_THR_MASK                  0x0070  
#define WM8904_MICDET_THR_SHIFT                      4  
#define WM8904_MICDET_THR_WIDTH                      3  
#define WM8904_MICSHORT_THR_MASK                0x000C  
#define WM8904_MICSHORT_THR_SHIFT                    2  
#define WM8904_MICSHORT_THR_WIDTH                    2  
#define WM8904_MICDET_ENA                       0x0002  
#define WM8904_MICDET_ENA_MASK                  0x0002  
#define WM8904_MICDET_ENA_SHIFT                      1  
#define WM8904_MICDET_ENA_WIDTH                      1  
#define WM8904_MICBIAS_ENA                      0x0001  
#define WM8904_MICBIAS_ENA_MASK                 0x0001  
#define WM8904_MICBIAS_ENA_SHIFT                     0  
#define WM8904_MICBIAS_ENA_WIDTH                     1  

#define WM8904_MIC_DET_FILTER_ENA               0x8000  
#define WM8904_MIC_DET_FILTER_ENA_MASK          0x8000  
#define WM8904_MIC_DET_FILTER_ENA_SHIFT             15  
#define WM8904_MIC_DET_FILTER_ENA_WIDTH              1  
#define WM8904_MIC_SHORT_FILTER_ENA             0x4000  
#define WM8904_MIC_SHORT_FILTER_ENA_MASK        0x4000  
#define WM8904_MIC_SHORT_FILTER_ENA_SHIFT           14  
#define WM8904_MIC_SHORT_FILTER_ENA_WIDTH            1  
#define WM8904_MICBIAS_SEL_MASK                 0x0007  
#define WM8904_MICBIAS_SEL_SHIFT                     0  
#define WM8904_MICBIAS_SEL_WIDTH                     3  


#define WM8904_GPIO1_PU                         0x0020  
#define WM8904_GPIO1_PU_MASK                    0x0020  
#define WM8904_GPIO1_PU_SHIFT                        5  
#define WM8904_GPIO1_PU_WIDTH                        1  
#define WM8904_GPIO1_PD                         0x0010  
#define WM8904_GPIO1_PD_MASK                    0x0010  
#define WM8904_GPIO1_PD_SHIFT                        4  
#define WM8904_GPIO1_PD_WIDTH                        1  
#define WM8904_GPIO1_SEL_MASK                   0x000F  
#define WM8904_GPIO1_SEL_SHIFT                       0  
#define WM8904_GPIO1_SEL_WIDTH                       4  

#define WM8904_GPIO2_PU                         0x0020  
#define WM8904_GPIO2_PU_MASK                    0x0020  
#define WM8904_GPIO2_PU_SHIFT                        5  
#define WM8904_GPIO2_PU_WIDTH                        1  
#define WM8904_GPIO2_PD                         0x0010  
#define WM8904_GPIO2_PD_MASK                    0x0010  
#define WM8904_GPIO2_PD_SHIFT                        4  
#define WM8904_GPIO2_PD_WIDTH                        1  
#define WM8904_GPIO2_SEL_MASK                   0x000F  
#define WM8904_GPIO2_SEL_SHIFT                       0  
#define WM8904_GPIO2_SEL_WIDTH                       4  

#define WM8904_GPIO3_PU                         0x0020  
#define WM8904_GPIO3_PU_MASK                    0x0020  
#define WM8904_GPIO3_PU_SHIFT                        5  
#define WM8904_GPIO3_PU_WIDTH                        1  
#define WM8904_GPIO3_PD                         0x0010  
#define WM8904_GPIO3_PD_MASK                    0x0010  
#define WM8904_GPIO3_PD_SHIFT                        4  
#define WM8904_GPIO3_PD_WIDTH                        1  
#define WM8904_GPIO3_SEL_MASK                   0x000F  
#define WM8904_GPIO3_SEL_SHIFT                       0  
#define WM8904_GPIO3_SEL_WIDTH                       4  

#define WM8904_GPI7_ENA                         0x0200  
#define WM8904_GPI7_ENA_MASK                    0x0200  
#define WM8904_GPI7_ENA_SHIFT                        9  
#define WM8904_GPI7_ENA_WIDTH                        1  
#define WM8904_GPI8_ENA                         0x0100  
#define WM8904_GPI8_ENA_MASK                    0x0100  
#define WM8904_GPI8_ENA_SHIFT                        8  
#define WM8904_GPI8_ENA_WIDTH                        1  
#define WM8904_GPIO_BCLK_MODE_ENA               0x0080  
#define WM8904_GPIO_BCLK_MODE_ENA_MASK          0x0080  
#define WM8904_GPIO_BCLK_MODE_ENA_SHIFT              7  
#define WM8904_GPIO_BCLK_MODE_ENA_WIDTH              1  
#define WM8904_GPIO_BCLK_SEL_MASK               0x000F  
#define WM8904_GPIO_BCLK_SEL_SHIFT                   0  
#define WM8904_GPIO_BCLK_SEL_WIDTH                   4  

#define WM8904_MIC_REGS  2
#define WM8904_GPIO_REGS 4
#define WM8904_DRC_REGS  4
#define WM8904_EQ_REGS   25

struct wm8904_drc_cfg {
	const char *name;
	u16 regs[WM8904_DRC_REGS];
};

struct wm8904_retune_mobile_cfg {
	const char *name;
	unsigned int rate;
	u16 regs[WM8904_EQ_REGS];
};

struct wm8904_pdata {
	int num_drc_cfgs;
	struct wm8904_drc_cfg *drc_cfgs;

	int num_retune_mobile_cfgs;
	struct wm8904_retune_mobile_cfg *retune_mobile_cfgs;

	u32 gpio_cfg[WM8904_GPIO_REGS];
	u32 mic_cfg[WM8904_MIC_REGS];
};

#endif
