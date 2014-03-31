/*
 * Copyright 2007-2010 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF549_H
#define _DEF_BF549_H

#include "defBF54x_base.h"

#include "defBF547.h"


#define                      MXVR_CONFIG  0xffc02700   
#define                     MXVR_STATE_0  0xffc02708   
#define                     MXVR_STATE_1  0xffc0270c   
#define                  MXVR_INT_STAT_0  0xffc02710   
#define                  MXVR_INT_STAT_1  0xffc02714   
#define                    MXVR_INT_EN_0  0xffc02718   
#define                    MXVR_INT_EN_1  0xffc0271c   
#define                    MXVR_POSITION  0xffc02720   
#define                MXVR_MAX_POSITION  0xffc02724   
#define                       MXVR_DELAY  0xffc02728   
#define                   MXVR_MAX_DELAY  0xffc0272c   
#define                       MXVR_LADDR  0xffc02730   
#define                       MXVR_GADDR  0xffc02734   
#define                       MXVR_AADDR  0xffc02738   


#define                     MXVR_ALLOC_0  0xffc0273c   
#define                     MXVR_ALLOC_1  0xffc02740   
#define                     MXVR_ALLOC_2  0xffc02744   
#define                     MXVR_ALLOC_3  0xffc02748   
#define                     MXVR_ALLOC_4  0xffc0274c   
#define                     MXVR_ALLOC_5  0xffc02750   
#define                     MXVR_ALLOC_6  0xffc02754   
#define                     MXVR_ALLOC_7  0xffc02758   
#define                     MXVR_ALLOC_8  0xffc0275c   
#define                     MXVR_ALLOC_9  0xffc02760   
#define                    MXVR_ALLOC_10  0xffc02764   
#define                    MXVR_ALLOC_11  0xffc02768   
#define                    MXVR_ALLOC_12  0xffc0276c   
#define                    MXVR_ALLOC_13  0xffc02770   
#define                    MXVR_ALLOC_14  0xffc02774   


#define                MXVR_SYNC_LCHAN_0  0xffc02778   
#define                MXVR_SYNC_LCHAN_1  0xffc0277c   
#define                MXVR_SYNC_LCHAN_2  0xffc02780   
#define                MXVR_SYNC_LCHAN_3  0xffc02784   
#define                MXVR_SYNC_LCHAN_4  0xffc02788   
#define                MXVR_SYNC_LCHAN_5  0xffc0278c   
#define                MXVR_SYNC_LCHAN_6  0xffc02790   
#define                MXVR_SYNC_LCHAN_7  0xffc02794   


#define                 MXVR_DMA0_CONFIG  0xffc02798   
#define             MXVR_DMA0_START_ADDR  0xffc0279c   
#define                  MXVR_DMA0_COUNT  0xffc027a0   
#define              MXVR_DMA0_CURR_ADDR  0xffc027a4   
#define             MXVR_DMA0_CURR_COUNT  0xffc027a8   


#define                 MXVR_DMA1_CONFIG  0xffc027ac   
#define             MXVR_DMA1_START_ADDR  0xffc027b0   
#define                  MXVR_DMA1_COUNT  0xffc027b4   
#define              MXVR_DMA1_CURR_ADDR  0xffc027b8   
#define             MXVR_DMA1_CURR_COUNT  0xffc027bc   


#define                 MXVR_DMA2_CONFIG  0xffc027c0   
#define             MXVR_DMA2_START_ADDR  0xffc027c4   
#define                  MXVR_DMA2_COUNT  0xffc027c8   
#define              MXVR_DMA2_CURR_ADDR  0xffc027cc   
#define             MXVR_DMA2_CURR_COUNT  0xffc027d0   


#define                 MXVR_DMA3_CONFIG  0xffc027d4   
#define             MXVR_DMA3_START_ADDR  0xffc027d8   
#define                  MXVR_DMA3_COUNT  0xffc027dc   
#define              MXVR_DMA3_CURR_ADDR  0xffc027e0   
#define             MXVR_DMA3_CURR_COUNT  0xffc027e4   


#define                 MXVR_DMA4_CONFIG  0xffc027e8   
#define             MXVR_DMA4_START_ADDR  0xffc027ec   
#define                  MXVR_DMA4_COUNT  0xffc027f0   
#define              MXVR_DMA4_CURR_ADDR  0xffc027f4   
#define             MXVR_DMA4_CURR_COUNT  0xffc027f8   


#define                 MXVR_DMA5_CONFIG  0xffc027fc   
#define             MXVR_DMA5_START_ADDR  0xffc02800   
#define                  MXVR_DMA5_COUNT  0xffc02804   
#define              MXVR_DMA5_CURR_ADDR  0xffc02808   
#define             MXVR_DMA5_CURR_COUNT  0xffc0280c   


#define                 MXVR_DMA6_CONFIG  0xffc02810   
#define             MXVR_DMA6_START_ADDR  0xffc02814   
#define                  MXVR_DMA6_COUNT  0xffc02818   
#define              MXVR_DMA6_CURR_ADDR  0xffc0281c   
#define             MXVR_DMA6_CURR_COUNT  0xffc02820   


#define                 MXVR_DMA7_CONFIG  0xffc02824   
#define             MXVR_DMA7_START_ADDR  0xffc02828   
#define                  MXVR_DMA7_COUNT  0xffc0282c   
#define              MXVR_DMA7_CURR_ADDR  0xffc02830   
#define             MXVR_DMA7_CURR_COUNT  0xffc02834   


#define                      MXVR_AP_CTL  0xffc02838   
#define             MXVR_APRB_START_ADDR  0xffc0283c   
#define              MXVR_APRB_CURR_ADDR  0xffc02840   
#define             MXVR_APTB_START_ADDR  0xffc02844   
#define              MXVR_APTB_CURR_ADDR  0xffc02848   


#define                      MXVR_CM_CTL  0xffc0284c   
#define             MXVR_CMRB_START_ADDR  0xffc02850   
#define              MXVR_CMRB_CURR_ADDR  0xffc02854   
#define             MXVR_CMTB_START_ADDR  0xffc02858   
#define              MXVR_CMTB_CURR_ADDR  0xffc0285c   


#define             MXVR_RRDB_START_ADDR  0xffc02860   
#define              MXVR_RRDB_CURR_ADDR  0xffc02864   


#define                  MXVR_PAT_DATA_0  0xffc02868   
#define                    MXVR_PAT_EN_0  0xffc0286c   
#define                  MXVR_PAT_DATA_1  0xffc02870   
#define                    MXVR_PAT_EN_1  0xffc02874   


#define                 MXVR_FRAME_CNT_0  0xffc02878   
#define                 MXVR_FRAME_CNT_1  0xffc0287c   


#define                   MXVR_ROUTING_0  0xffc02880   
#define                   MXVR_ROUTING_1  0xffc02884   
#define                   MXVR_ROUTING_2  0xffc02888   
#define                   MXVR_ROUTING_3  0xffc0288c   
#define                   MXVR_ROUTING_4  0xffc02890   
#define                   MXVR_ROUTING_5  0xffc02894   
#define                   MXVR_ROUTING_6  0xffc02898   
#define                   MXVR_ROUTING_7  0xffc0289c   
#define                   MXVR_ROUTING_8  0xffc028a0   
#define                   MXVR_ROUTING_9  0xffc028a4   
#define                  MXVR_ROUTING_10  0xffc028a8   
#define                  MXVR_ROUTING_11  0xffc028ac   
#define                  MXVR_ROUTING_12  0xffc028b0   
#define                  MXVR_ROUTING_13  0xffc028b4   
#define                  MXVR_ROUTING_14  0xffc028b8   


#define                   MXVR_BLOCK_CNT  0xffc028c0   
#define                     MXVR_CLK_CTL  0xffc028d0   
#define                  MXVR_CDRPLL_CTL  0xffc028d4   
#define                   MXVR_FMPLL_CTL  0xffc028d8   
#define                     MXVR_PIN_CTL  0xffc028dc   
#define                    MXVR_SCLK_CNT  0xffc028e0   

#endif 
