#ifndef _INC_MUSYCC_H_
#define _INC_MUSYCC_H_

/*-----------------------------------------------------------------------------
 * musycc.h - Multichannel Synchronous Communications Controller
 *            CN8778/8474A/8472A/8471A
 *
 * Copyright (C) 2002-2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * For further information, contact via email: support@sbei.com
 * SBE, Inc.  San Ramon, California  U.S.A.
 *-----------------------------------------------------------------------------
 */

#include <linux/types.h>

#define VINT8   volatile u_int8_t
#define VINT32  volatile u_int32_t

#include "pmcc4_defs.h"



#define PCI_VENDOR_ID_CONEXANT   0x14f1
#define PCI_DEVICE_ID_CN8471     0x8471
#define PCI_DEVICE_ID_CN8472     0x8472
#define PCI_DEVICE_ID_CN8474     0x8474
#define PCI_DEVICE_ID_CN8478     0x8478
#define PCI_DEVICE_ID_CN8500     0x8500
#define PCI_DEVICE_ID_CN8501     0x8501
#define PCI_DEVICE_ID_CN8502     0x8502
#define PCI_DEVICE_ID_CN8503     0x8503

#define INT_QUEUE_SIZE    MUSYCC_NIQD

    struct musycc_groupr
    {
        VINT32      thp[32];    
        VINT32      tmp[32];    
        VINT32      rhp[32];    
        VINT32      rmp[32];    
        VINT8       ttsm[128];  
        VINT8       tscm[256];  
        VINT32      tcct[32];   
        VINT8       rtsm[128];  
        VINT8       rscm[256];  
        VINT32      rcct[32];   
        VINT32      __glcd;     
        VINT32      __iqp;      
        VINT32      __iql;      
        VINT32      grcd;       
        VINT32      mpd;        
        VINT32      mld;        
        VINT32      pcd;        
    };

    struct musycc_globalr
    {
        VINT32      gbp;        
        VINT32      dacbp;      
        VINT32      srd;        
        VINT32      isd;        
        VINT32      __thp[32 - 4];      
        VINT32      __tmp[32];  
        VINT32      __rhp[32];  
        VINT32      __rmp[32];  
        VINT8       ttsm[128];  
        VINT8       tscm[256];  
        VINT32      tcct[32];   
        VINT8       rtsm[128];  
        VINT8       rscm[256];  
        VINT32      rcct[32];   
        VINT32      glcd;       
        VINT32      iqp;        
        VINT32      iql;        
        VINT32      grcd;       
        VINT32      mpd;        
        VINT32      mld;        
        VINT32      pcd;        
        VINT32      rbist;      
        VINT32      tbist;      
    };

#define MUSYCC_GCD_ECLK_ENABLE  0x00000800      
#define MUSYCC_GCD_INTEL_SELECT 0x00000400      
#define MUSYCC_GCD_INTA_DISABLE 0x00000008      
#define MUSYCC_GCD_INTB_DISABLE 0x00000004      
#define MUSYCC_GCD_BLAPSE       12      
#define MUSYCC_GCD_ALAPSE       8       
#define MUSYCC_GCD_ELAPSE       4       
#define MUSYCC_GCD_PORTMAP_3    3       
#define MUSYCC_GCD_PORTMAP_2    2       
#define MUSYCC_GCD_PORTMAP_1    1       
#define MUSYCC_GCD_PORTMAP_0    0       

#ifdef SBE_WAN256T3_ENABLE
#define BLAPSE_VAL      0
#define ALAPSE_VAL      0
#define ELAPSE_VAL      7
#define PORTMAP_VAL     MUSYCC_GCD_PORTMAP_2
#endif

#ifdef SBE_PMCC4_ENABLE
#define BLAPSE_VAL      7
#define ALAPSE_VAL      3
#define ELAPSE_VAL      7
#define PORTMAP_VAL     MUSYCC_GCD_PORTMAP_0
#endif

#define GCD_MAGIC   (((BLAPSE_VAL)<<(MUSYCC_GCD_BLAPSE)) | \
                     ((ALAPSE_VAL)<<(MUSYCC_GCD_ALAPSE)) | \
                     ((ELAPSE_VAL)<<(MUSYCC_GCD_ELAPSE)) | \
                     (MUSYCC_GCD_ECLK_ENABLE) | PORTMAP_VAL)

#define MUSYCC_GRCD_RX_ENABLE       0x00000001  
#define MUSYCC_GRCD_TX_ENABLE       0x00000002  
#define MUSYCC_GRCD_SUBCHAN_DISABLE 0x00000004  
#define MUSYCC_GRCD_OOFMP_DISABLE   0x00000008  
#define MUSYCC_GRCD_OOFIRQ_DISABLE  0x00000010  
#define MUSYCC_GRCD_COFAIRQ_DISABLE 0x00000020  
#define MUSYCC_GRCD_INHRBSD         0x00000100  
#define MUSYCC_GRCD_INHTBSD         0x00000200  
#define MUSYCC_GRCD_SF_ALIGN        0x00008000  
#define MUSYCC_GRCD_MC_ENABLE       0x00000040  
#define MUSYCC_GRCD_POLLTH_16       0x00000001  
#define MUSYCC_GRCD_POLLTH_32       0x00000002  
#define MUSYCC_GRCD_POLLTH_64       0x00000003  
#define MUSYCC_GRCD_POLLTH_SHIFT    10  
#define MUSYCC_GRCD_SUERM_THRESH_SHIFT 16       

#define MUSYCC_PCD_E1X2_MODE       2    
#define MUSYCC_PCD_E1X4_MODE       3    
#define MUSYCC_PCD_NX64_MODE       4
#define MUSYCC_PCD_TXDATA_RISING   0x00000010   
#define MUSYCC_PCD_TXSYNC_RISING   0x00000020   
#define MUSYCC_PCD_RXDATA_RISING   0x00000040   
#define MUSYCC_PCD_RXSYNC_RISING   0x00000080   
#define MUSYCC_PCD_ROOF_RISING     0x00000100   
#define MUSYCC_PCD_TX_DRIVEN       0x00000200   
#define MUSYCC_PCD_PORTMODE_MASK   0xfffffff8   

#define MUSYCC_TSD_MODE_64KBPS              4
#define MUSYCC_TSD_MODE_56KBPS              5
#define MUSYCC_TSD_SUBCHANNEL_WO_FIRST      6
#define MUSYCC_TSD_SUBCHANNEL_WITH_FIRST    7

#define MUSYCC_MDT_BASE03_ADDR     0x00006000

#define MUSYCC_CCD_BUFIRQ_DISABLE  0x00000002   
#define MUSYCC_CCD_EOMIRQ_DISABLE  0x00000004   
#define MUSYCC_CCD_MSGIRQ_DISABLE  0x00000008   
#define MUSYCC_CCD_IDLEIRQ_DISABLE 0x00000010   
#define MUSYCC_CCD_FILTIRQ_DISABLE 0x00000020   
#define MUSYCC_CCD_SDECIRQ_DISABLE 0x00000040   
#define MUSYCC_CCD_SINCIRQ_DISABLE 0x00000080   
#define MUSYCC_CCD_SUERIRQ_DISABLE 0x00000100   
#define MUSYCC_CCD_FCS_XFER        0x00000200   
#define MUSYCC_CCD_PROTO_SHIFT     12   
#define MUSYCC_CCD_TRANS           0    
#define MUSYCC_CCD_SS7             1
#define MUSYCC_CCD_HDLC_FCS16      2
#define MUSYCC_CCD_HDLC_FCS32      3
#define MUSYCC_CCD_EOPIRQ_DISABLE  0x00008000   
#define MUSYCC_CCD_INVERT_DATA     0x00800000   
#define MUSYCC_CCD_MAX_LENGTH      10   
#define MUSYCC_CCD_BUFFER_LENGTH   16   
#define MUSYCC_CCD_BUFFER_LOC      24   


#define INT_EMPTY_ENTRY     0xfeedface
#define INT_EMPTY_ENTRY2    0xdeadface


#define INTRPTS_NEXTINT_M      0x7FFF0000
#define INTRPTS_NEXTINT_S      16
#define INTRPTS_NEXTINT(x)     ((x & INTRPTS_NEXTINT_M) >> INTRPTS_NEXTINT_S)

#define INTRPTS_INTFULL_M      0x00008000
#define INTRPTS_INTFULL_S      15
#define INTRPTS_INTFULL(x)     ((x & INTRPTS_INTFULL_M) >> INTRPTS_INTFULL_S)

#define INTRPTS_INTCNT_M       0x00007FFF
#define INTRPTS_INTCNT_S       0
#define INTRPTS_INTCNT(x)      ((x & INTRPTS_INTCNT_M) >> INTRPTS_INTCNT_S)



#define INTRPT_DIR_M           0x80000000
#define INTRPT_DIR_S           31
#define INTRPT_DIR(x)          ((x & INTRPT_DIR_M) >> INTRPT_DIR_S)

#define INTRPT_GRP_M           0x60000000
#define INTRPT_GRP_MSB_M       0x00004000
#define INTRPT_GRP_S           29
#define INTRPT_GRP_MSB_S       12
#define INTRPT_GRP(x)          (((x & INTRPT_GRP_M) >> INTRPT_GRP_S) | \
                               ((x & INTRPT_GRP_MSB_M) >> INTRPT_GRP_MSB_S))

#define INTRPT_CH_M            0x1F000000
#define INTRPT_CH_S            24
#define INTRPT_CH(x)           ((x & INTRPT_CH_M) >> INTRPT_CH_S)

#define INTRPT_EVENT_M         0x00F00000
#define INTRPT_EVENT_S         20
#define INTRPT_EVENT(x)        ((x & INTRPT_EVENT_M) >> INTRPT_EVENT_S)

#define INTRPT_ERROR_M         0x000F0000
#define INTRPT_ERROR_S         16
#define INTRPT_ERROR(x)        ((x & INTRPT_ERROR_M) >> INTRPT_ERROR_S)

#define INTRPT_ILOST_M         0x00008000
#define INTRPT_ILOST_S         15
#define INTRPT_ILOST(x)        ((x & INTRPT_ILOST_M) >> INTRPT_ILOST_S)

#define INTRPT_PERR_M          0x00004000
#define INTRPT_PERR_S          14
#define INTRPT_PERR(x)         ((x & INTRPT_PERR_M) >> INTRPT_PERR_S)

#define INTRPT_BLEN_M          0x00003FFF
#define INTRPT_BLEN_S          0
#define INTRPT_BLEN(x)         ((x & INTRPT_BLEN_M) >> INTRPT_BLEN_S)


#define OWNER_BIT       0x80000000      
#define HOST_TX_OWNED   0x00000000      
#define MUSYCC_TX_OWNED 0x80000000      
#define HOST_RX_OWNED   0x80000000      
#define MUSYCC_RX_OWNED 0x00000000      

#define POLL_DISABLED   0x40000000      
#define EOMIRQ_ENABLE   0x20000000      
#define EOBIRQ_ENABLE   0x10000000      
#define PADFILL_ENABLE  0x01000000      
#define REPEAT_BIT      0x00008000      
#define LENGTH_MASK         0X3fff      
#define IDLE_CODE               25      
#define EXTRA_FLAGS             16      
#define IDLE_CODE_MASK        0x03      
#define EXTRA_FLAGS_MASK      0xff      
#define PCI_PERMUTED_OWNER_BIT  0x00000080      

#define SREQ  8                 
#define SR_NOOP                 (0<<(SREQ))     
#define SR_CHIP_RESET           (1<<(SREQ))     
#define SR_GROUP_RESET          (2<<(SREQ))     
#define SR_GLOBAL_INIT          (4<<(SREQ))     
#define SR_GROUP_INIT           (5<<(SREQ))     
#define SR_CHANNEL_ACTIVATE     (8<<(SREQ))     
#define SR_GCHANNEL_MASK        0x001F          
#define SR_CHANNEL_DEACTIVATE   (9<<(SREQ))     
#define SR_JUMP                 (10<<(SREQ))    
#define SR_CHANNEL_CONFIG       (11<<(SREQ))    
#define SR_GLOBAL_CONFIG        (16<<(SREQ))    
#define SR_INTERRUPT_Q          (17<<(SREQ))    
#define SR_GROUP_CONFIG         (18<<(SREQ))    
#define SR_MEMORY_PROTECT       (19<<(SREQ))    
#define SR_MESSAGE_LENGTH       (20<<(SREQ))    
#define SR_PORT_CONFIG          (21<<(SREQ))    
#define SR_TIMESLOT_MAP         (24<<(SREQ))    
#define SR_SUBCHANNEL_MAP       (25<<(SREQ))    
#define SR_CHAN_CONFIG_TABLE    (26<<(SREQ))    
#define SR_TX_DIRECTION         0x00000020      
#define SR_RX_DIRECTION         0x00000000

#define GROUP10                     29  
#define CHANNEL                     24  
#define INT_IQD_TX          0x80000000
#define INT_IQD_GRP         0x60000000
#define INT_IQD_CHAN        0x1f000000
#define INT_IQD_EVENT       0x00f00000
#define INT_IQD_ERROR       0x000f0000
#define INT_IQD_ILOST       0x00008000
#define INT_IQD_PERR        0x00004000
#define INT_IQD_BLEN        0x00003fff

#define EVE_EVENT               20      
#define EVE_NONE                0       
#define EVE_SACK                1       
#define EVE_EOB                 2       
#define EVE_EOM                 3       
#define EVE_EOP                 4       
#define EVE_CHABT               5       
#define EVE_CHIC                6       
#define EVE_FREC                7       
#define EVE_SINC                8       
#define EVE_SDEC                9       
#define EVE_SFILT               10      
#define ERR_ERRORS              16      
#define ERR_BUF                 1       
#define ERR_COFA                2       
#define ERR_ONR                 3       
#define ERR_PROT                4       
#define ERR_OOF                 8       
#define ERR_FCS                 9       
#define ERR_ALIGN               10      
#define ERR_ABT                 11      
#define ERR_LNG                 12      
#define ERR_SHT                 13      
#define ERR_SUERR               14      
#define ERR_PERR                15      
#define TRANSMIT_DIRECTION  0x80000000  
#define ILOST               0x00008000  
#define GROUPMSB            0x00004000  
#define SACK_IMAGE          0x00100000  
#define INITIAL_STATUS      0x10000     

#define SUERM_THRESHOLD     0x1f

#undef VINT32
#undef VINT8

#endif                          

