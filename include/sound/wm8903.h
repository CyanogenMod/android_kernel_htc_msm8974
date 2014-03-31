/*
 * linux/sound/wm8903.h -- Platform data for WM8903
 *
 * Copyright 2010 Wolfson Microelectronics. PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SND_WM8903_H
#define __LINUX_SND_WM8903_H

#define WM8903_GPIO_CONFIG_ZERO 0x8000

#define WM8903_MICDET_THR_MASK                  0x0030  
#define WM8903_MICDET_THR_SHIFT                      4  
#define WM8903_MICDET_THR_WIDTH                      2  
#define WM8903_MICSHORT_THR_MASK                0x000C  
#define WM8903_MICSHORT_THR_SHIFT                    2  
#define WM8903_MICSHORT_THR_WIDTH                    2  
#define WM8903_MICDET_ENA                       0x0002  
#define WM8903_MICDET_ENA_MASK                  0x0002  
#define WM8903_MICDET_ENA_SHIFT                      1  
#define WM8903_MICDET_ENA_WIDTH                      1  
#define WM8903_MICBIAS_ENA                      0x0001  
#define WM8903_MICBIAS_ENA_MASK                 0x0001  
#define WM8903_MICBIAS_ENA_SHIFT                     0  
#define WM8903_MICBIAS_ENA_WIDTH                     1  

#define WM8903_GPn_FN_GPIO_OUTPUT                    0
#define WM8903_GPn_FN_BCLK                           1
#define WM8903_GPn_FN_IRQ_OUTPT                      2
#define WM8903_GPn_FN_GPIO_INPUT                     3
#define WM8903_GPn_FN_MICBIAS_CURRENT_DETECT         4
#define WM8903_GPn_FN_MICBIAS_SHORT_DETECT           5
#define WM8903_GPn_FN_DMIC_LR_CLK_OUTPUT             6
#define WM8903_GPn_FN_FLL_LOCK_OUTPUT                8
#define WM8903_GPn_FN_FLL_CLOCK_OUTPUT               9

#define WM8903_GP1_FN_MASK                      0x1F00  
#define WM8903_GP1_FN_SHIFT                          8  
#define WM8903_GP1_FN_WIDTH                          5  
#define WM8903_GP1_DIR                          0x0080  
#define WM8903_GP1_DIR_MASK                     0x0080  
#define WM8903_GP1_DIR_SHIFT                         7  
#define WM8903_GP1_DIR_WIDTH                         1  
#define WM8903_GP1_OP_CFG                       0x0040  
#define WM8903_GP1_OP_CFG_MASK                  0x0040  
#define WM8903_GP1_OP_CFG_SHIFT                      6  
#define WM8903_GP1_OP_CFG_WIDTH                      1  
#define WM8903_GP1_IP_CFG                       0x0020  
#define WM8903_GP1_IP_CFG_MASK                  0x0020  
#define WM8903_GP1_IP_CFG_SHIFT                      5  
#define WM8903_GP1_IP_CFG_WIDTH                      1  
#define WM8903_GP1_LVL                          0x0010  
#define WM8903_GP1_LVL_MASK                     0x0010  
#define WM8903_GP1_LVL_SHIFT                         4  
#define WM8903_GP1_LVL_WIDTH                         1  
#define WM8903_GP1_PD                           0x0008  
#define WM8903_GP1_PD_MASK                      0x0008  
#define WM8903_GP1_PD_SHIFT                          3  
#define WM8903_GP1_PD_WIDTH                          1  
#define WM8903_GP1_PU                           0x0004  
#define WM8903_GP1_PU_MASK                      0x0004  
#define WM8903_GP1_PU_SHIFT                          2  
#define WM8903_GP1_PU_WIDTH                          1  
#define WM8903_GP1_INTMODE                      0x0002  
#define WM8903_GP1_INTMODE_MASK                 0x0002  
#define WM8903_GP1_INTMODE_SHIFT                     1  
#define WM8903_GP1_INTMODE_WIDTH                     1  
#define WM8903_GP1_DB                           0x0001  
#define WM8903_GP1_DB_MASK                      0x0001  
#define WM8903_GP1_DB_SHIFT                          0  
#define WM8903_GP1_DB_WIDTH                          1  

#define WM8903_GP2_FN_MASK                      0x1F00  
#define WM8903_GP2_FN_SHIFT                          8  
#define WM8903_GP2_FN_WIDTH                          5  
#define WM8903_GP2_DIR                          0x0080  
#define WM8903_GP2_DIR_MASK                     0x0080  
#define WM8903_GP2_DIR_SHIFT                         7  
#define WM8903_GP2_DIR_WIDTH                         1  
#define WM8903_GP2_OP_CFG                       0x0040  
#define WM8903_GP2_OP_CFG_MASK                  0x0040  
#define WM8903_GP2_OP_CFG_SHIFT                      6  
#define WM8903_GP2_OP_CFG_WIDTH                      1  
#define WM8903_GP2_IP_CFG                       0x0020  
#define WM8903_GP2_IP_CFG_MASK                  0x0020  
#define WM8903_GP2_IP_CFG_SHIFT                      5  
#define WM8903_GP2_IP_CFG_WIDTH                      1  
#define WM8903_GP2_LVL                          0x0010  
#define WM8903_GP2_LVL_MASK                     0x0010  
#define WM8903_GP2_LVL_SHIFT                         4  
#define WM8903_GP2_LVL_WIDTH                         1  
#define WM8903_GP2_PD                           0x0008  
#define WM8903_GP2_PD_MASK                      0x0008  
#define WM8903_GP2_PD_SHIFT                          3  
#define WM8903_GP2_PD_WIDTH                          1  
#define WM8903_GP2_PU                           0x0004  
#define WM8903_GP2_PU_MASK                      0x0004  
#define WM8903_GP2_PU_SHIFT                          2  
#define WM8903_GP2_PU_WIDTH                          1  
#define WM8903_GP2_INTMODE                      0x0002  
#define WM8903_GP2_INTMODE_MASK                 0x0002  
#define WM8903_GP2_INTMODE_SHIFT                     1  
#define WM8903_GP2_INTMODE_WIDTH                     1  
#define WM8903_GP2_DB                           0x0001  
#define WM8903_GP2_DB_MASK                      0x0001  
#define WM8903_GP2_DB_SHIFT                          0  
#define WM8903_GP2_DB_WIDTH                          1  

#define WM8903_GP3_FN_MASK                      0x1F00  
#define WM8903_GP3_FN_SHIFT                          8  
#define WM8903_GP3_FN_WIDTH                          5  
#define WM8903_GP3_DIR                          0x0080  
#define WM8903_GP3_DIR_MASK                     0x0080  
#define WM8903_GP3_DIR_SHIFT                         7  
#define WM8903_GP3_DIR_WIDTH                         1  
#define WM8903_GP3_OP_CFG                       0x0040  
#define WM8903_GP3_OP_CFG_MASK                  0x0040  
#define WM8903_GP3_OP_CFG_SHIFT                      6  
#define WM8903_GP3_OP_CFG_WIDTH                      1  
#define WM8903_GP3_IP_CFG                       0x0020  
#define WM8903_GP3_IP_CFG_MASK                  0x0020  
#define WM8903_GP3_IP_CFG_SHIFT                      5  
#define WM8903_GP3_IP_CFG_WIDTH                      1  
#define WM8903_GP3_LVL                          0x0010  
#define WM8903_GP3_LVL_MASK                     0x0010  
#define WM8903_GP3_LVL_SHIFT                         4  
#define WM8903_GP3_LVL_WIDTH                         1  
#define WM8903_GP3_PD                           0x0008  
#define WM8903_GP3_PD_MASK                      0x0008  
#define WM8903_GP3_PD_SHIFT                          3  
#define WM8903_GP3_PD_WIDTH                          1  
#define WM8903_GP3_PU                           0x0004  
#define WM8903_GP3_PU_MASK                      0x0004  
#define WM8903_GP3_PU_SHIFT                          2  
#define WM8903_GP3_PU_WIDTH                          1  
#define WM8903_GP3_INTMODE                      0x0002  
#define WM8903_GP3_INTMODE_MASK                 0x0002  
#define WM8903_GP3_INTMODE_SHIFT                     1  
#define WM8903_GP3_INTMODE_WIDTH                     1  
#define WM8903_GP3_DB                           0x0001  
#define WM8903_GP3_DB_MASK                      0x0001  
#define WM8903_GP3_DB_SHIFT                          0  
#define WM8903_GP3_DB_WIDTH                          1  

#define WM8903_GP4_FN_MASK                      0x1F00  
#define WM8903_GP4_FN_SHIFT                          8  
#define WM8903_GP4_FN_WIDTH                          5  
#define WM8903_GP4_DIR                          0x0080  
#define WM8903_GP4_DIR_MASK                     0x0080  
#define WM8903_GP4_DIR_SHIFT                         7  
#define WM8903_GP4_DIR_WIDTH                         1  
#define WM8903_GP4_OP_CFG                       0x0040  
#define WM8903_GP4_OP_CFG_MASK                  0x0040  
#define WM8903_GP4_OP_CFG_SHIFT                      6  
#define WM8903_GP4_OP_CFG_WIDTH                      1  
#define WM8903_GP4_IP_CFG                       0x0020  
#define WM8903_GP4_IP_CFG_MASK                  0x0020  
#define WM8903_GP4_IP_CFG_SHIFT                      5  
#define WM8903_GP4_IP_CFG_WIDTH                      1  
#define WM8903_GP4_LVL                          0x0010  
#define WM8903_GP4_LVL_MASK                     0x0010  
#define WM8903_GP4_LVL_SHIFT                         4  
#define WM8903_GP4_LVL_WIDTH                         1  
#define WM8903_GP4_PD                           0x0008  
#define WM8903_GP4_PD_MASK                      0x0008  
#define WM8903_GP4_PD_SHIFT                          3  
#define WM8903_GP4_PD_WIDTH                          1  
#define WM8903_GP4_PU                           0x0004  
#define WM8903_GP4_PU_MASK                      0x0004  
#define WM8903_GP4_PU_SHIFT                          2  
#define WM8903_GP4_PU_WIDTH                          1  
#define WM8903_GP4_INTMODE                      0x0002  
#define WM8903_GP4_INTMODE_MASK                 0x0002  
#define WM8903_GP4_INTMODE_SHIFT                     1  
#define WM8903_GP4_INTMODE_WIDTH                     1  
#define WM8903_GP4_DB                           0x0001  
#define WM8903_GP4_DB_MASK                      0x0001  
#define WM8903_GP4_DB_SHIFT                          0  
#define WM8903_GP4_DB_WIDTH                          1  

#define WM8903_GP5_FN_MASK                      0x1F00  
#define WM8903_GP5_FN_SHIFT                          8  
#define WM8903_GP5_FN_WIDTH                          5  
#define WM8903_GP5_DIR                          0x0080  
#define WM8903_GP5_DIR_MASK                     0x0080  
#define WM8903_GP5_DIR_SHIFT                         7  
#define WM8903_GP5_DIR_WIDTH                         1  
#define WM8903_GP5_OP_CFG                       0x0040  
#define WM8903_GP5_OP_CFG_MASK                  0x0040  
#define WM8903_GP5_OP_CFG_SHIFT                      6  
#define WM8903_GP5_OP_CFG_WIDTH                      1  
#define WM8903_GP5_IP_CFG                       0x0020  
#define WM8903_GP5_IP_CFG_MASK                  0x0020  
#define WM8903_GP5_IP_CFG_SHIFT                      5  
#define WM8903_GP5_IP_CFG_WIDTH                      1  
#define WM8903_GP5_LVL                          0x0010  
#define WM8903_GP5_LVL_MASK                     0x0010  
#define WM8903_GP5_LVL_SHIFT                         4  
#define WM8903_GP5_LVL_WIDTH                         1  
#define WM8903_GP5_PD                           0x0008  
#define WM8903_GP5_PD_MASK                      0x0008  
#define WM8903_GP5_PD_SHIFT                          3  
#define WM8903_GP5_PD_WIDTH                          1  
#define WM8903_GP5_PU                           0x0004  
#define WM8903_GP5_PU_MASK                      0x0004  
#define WM8903_GP5_PU_SHIFT                          2  
#define WM8903_GP5_PU_WIDTH                          1  
#define WM8903_GP5_INTMODE                      0x0002  
#define WM8903_GP5_INTMODE_MASK                 0x0002  
#define WM8903_GP5_INTMODE_SHIFT                     1  
#define WM8903_GP5_INTMODE_WIDTH                     1  
#define WM8903_GP5_DB                           0x0001  
#define WM8903_GP5_DB_MASK                      0x0001  
#define WM8903_GP5_DB_SHIFT                          0  
#define WM8903_GP5_DB_WIDTH                          1  

#define WM8903_NUM_GPIO 5

struct wm8903_platform_data {
	bool irq_active_low;   

	u16 micdet_cfg;

	int micdet_delay;      

	int gpio_base;
	u32 gpio_cfg[WM8903_NUM_GPIO]; 
};

#endif
