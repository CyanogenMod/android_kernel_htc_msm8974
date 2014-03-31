/*
 * Copyright 2008-2009 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF518_H
#define _DEF_BF518_H

#include "defBF516.h"


#define EMAC_PTP_CTL                   0xFFC030A0 
#define EMAC_PTP_IE                    0xFFC030A4 
#define EMAC_PTP_ISTAT                 0xFFC030A8 
#define EMAC_PTP_FOFF                  0xFFC030AC 
#define EMAC_PTP_FV1                   0xFFC030B0 
#define EMAC_PTP_FV2                   0xFFC030B4 
#define EMAC_PTP_FV3                   0xFFC030B8 
#define EMAC_PTP_ADDEND                0xFFC030BC 
#define EMAC_PTP_ACCR                  0xFFC030C0 
#define EMAC_PTP_OFFSET                0xFFC030C4 
#define EMAC_PTP_TIMELO                0xFFC030C8 
#define EMAC_PTP_TIMEHI                0xFFC030CC 
#define EMAC_PTP_RXSNAPLO              0xFFC030D0 
#define EMAC_PTP_RXSNAPHI              0xFFC030D4 
#define EMAC_PTP_TXSNAPLO              0xFFC030D8 
#define EMAC_PTP_TXSNAPHI              0xFFC030DC 
#define EMAC_PTP_ALARMLO               0xFFC030E0 
#define EMAC_PTP_ALARMHI               0xFFC030E4 
#define EMAC_PTP_ID_OFF                0xFFC030E8 
#define EMAC_PTP_ID_SNAP               0xFFC030EC 
#define EMAC_PTP_PPS_STARTLO           0xFFC030F0 
#define EMAC_PTP_PPS_STARTHI           0xFFC030F4 
#define EMAC_PTP_PPS_PERIOD            0xFFC030F8 


#define                    PTP_EN  0x1        
#define                        TL  0x2        
#define                      ASEN  0x10       
#define                     PPSEN  0x80       
#define                     CKOEN  0x2000     


#define                      ALIE  0x1        
#define                     RXEIE  0x2        
#define                     RXGIE  0x4        
#define                      TXIE  0x8        
#define                     RXOVE  0x10       
#define                     TXOVE  0x20       
#define                      ASIE  0x40       


#define                       ALS  0x1        
#define                      RXEL  0x2        
#define                      RXGL  0x4        
#define                      TXTL  0x8        
#define                      RXOV  0x10       
#define                      TXOV  0x20       
#define                       ASL  0x40       

#endif 
