/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * File: desc.h
 *
 * Purpose:The header file of descriptor
 *
 * Revision History:
 *
 * Author: Tevin Chen
 *
 * Date: May 21, 1996
 *
 */

#ifndef __DESC_H__
#define __DESC_H__

#include <linux/types.h>
#include <linux/mm.h>
#include "ttype.h"
#include "tether.h"


#define CB_MAX_BUF_SIZE     2900U       
                                        

#define CB_MAX_TX_BUF_SIZE          CB_MAX_BUF_SIZE 
#define CB_MAX_RX_BUF_SIZE_NORMAL   CB_MAX_BUF_SIZE 

#define CB_BEACON_BUF_SIZE  512U        

#define MAX_TOTAL_SIZE_WITH_ALL_HEADERS CB_MAX_BUF_SIZE

#define MAX_INTERRUPT_SIZE              32

#define RX_BLOCKS           64          
#define TX_BLOCKS           32          

#define CB_MAX_RX_DESC      128         
#define CB_MIN_RX_DESC      16          
#define CB_MAX_TX_DESC      128         
#define CB_MIN_TX_DESC      16          

#define CB_RD_NUM           64          
#define CB_TD_NUM           64          

#define RSR_ADDRBROAD       0x80        
#define RSR_ADDRMULTI       0x40        
#define RSR_ADDRUNI         0x00        
#define RSR_IVLDTYP         0x20        
#define RSR_IVLDLEN         0x10        
#define RSR_BSSIDOK         0x08        
#define RSR_CRCOK           0x04        
#define RSR_BCNSSIDOK       0x02        
#define RSR_ADDROK          0x01        

#define NEWRSR_DECRYPTOK    0x10        
#define NEWRSR_CFPIND       0x08        
#define NEWRSR_HWUTSF       0x04        
#define NEWRSR_BCNHITAID    0x02        
#define NEWRSR_BCNHITAID0   0x01        

#define TSR_RETRYTMO        0x08        
#define TSR_TMO             0x04        
#define TSR_ACKDATA         0x02        
#define TSR_VALID           0x01        

#define CB_PROTOCOL_RESERVED_SECTION    16

#define CB_MAX_TX_ABORT_RETRY   3

#define FIFOCTL_AUTO_FB_1   0x1000 
#define FIFOCTL_AUTO_FB_0   0x0800 
#define FIFOCTL_GRPACK      0x0400 
#define FIFOCTL_11GA        0x0300 
#define FIFOCTL_11GB        0x0200 
#define FIFOCTL_11B         0x0100 
#define FIFOCTL_11A         0x0000 
#define FIFOCTL_RTS         0x0080 
#define FIFOCTL_ISDMA0      0x0040 
#define FIFOCTL_GENINT      0x0020 
#define FIFOCTL_TMOEN       0x0010 
#define FIFOCTL_LRETRY      0x0008 
#define FIFOCTL_CRCDIS      0x0004 
#define FIFOCTL_NEEDACK     0x0002 
#define FIFOCTL_LHEAD       0x0001 

#define FRAGCTL_AES         0x0300 
#define FRAGCTL_TKIP        0x0200 
#define FRAGCTL_LEGACY      0x0100 
#define FRAGCTL_NONENCRYPT  0x0000 
#define FRAGCTL_ENDFRAG     0x0003 
#define FRAGCTL_MIDFRAG     0x0002 
#define FRAGCTL_STAFRAG     0x0001 
#define FRAGCTL_NONFRAG     0x0000 

#define TYPE_TXDMA0     0
#define TYPE_AC0DMA     1
#define TYPE_ATIMDMA    2
#define TYPE_SYNCDMA    3
#define TYPE_MAXTD      2

#define TYPE_BEACONDMA  4

#define TYPE_RXDMA0     0
#define TYPE_RXDMA1     1
#define TYPE_MAXRD      2

#define TD_FLAGS_NETIF_SKB               0x01       
#define TD_FLAGS_PRIV_SKB                0x02       
#define TD_FLAGS_PS_RETRY                0x04       


typedef struct tagSRrvTime_gRTS {
    WORD        wRTSTxRrvTime_ba;
    WORD        wRTSTxRrvTime_aa;
    WORD        wRTSTxRrvTime_bb;
    WORD        wReserved;
    WORD        wTxRrvTime_b;
    WORD        wTxRrvTime_a;
} __attribute__ ((__packed__))
SRrvTime_gRTS, *PSRrvTime_gRTS;

typedef const SRrvTime_gRTS *PCSRrvTime_gRTS;

typedef struct tagSRrvTime_gCTS {
    WORD        wCTSTxRrvTime_ba;
    WORD        wReserved;
    WORD        wTxRrvTime_b;
    WORD        wTxRrvTime_a;
} __attribute__ ((__packed__))
SRrvTime_gCTS, *PSRrvTime_gCTS;

typedef const SRrvTime_gCTS *PCSRrvTime_gCTS;

typedef struct tagSRrvTime_ab {
    WORD        wRTSTxRrvTime;
    WORD        wTxRrvTime;
} __attribute__ ((__packed__))
SRrvTime_ab, *PSRrvTime_ab;

typedef const SRrvTime_ab *PCSRrvTime_ab;

typedef struct tagSRrvTime_atim {
    WORD        wCTSTxRrvTime_ba;
    WORD        wTxRrvTime_a;
} __attribute__ ((__packed__))
SRrvTime_atim, *PSRrvTime_atim;

typedef const SRrvTime_atim *PCSRrvTime_atim;

typedef struct tagSRTSData {
    WORD    wFrameControl;
    WORD    wDurationID;
    BYTE    abyRA[ETH_ALEN];
    BYTE    abyTA[ETH_ALEN];
} __attribute__ ((__packed__))
SRTSData, *PSRTSData;

typedef const SRTSData *PCSRTSData;

typedef struct tagSRTS_g {
    BYTE        bySignalField_b;
    BYTE        byServiceField_b;
    WORD        wTransmitLength_b;
    BYTE        bySignalField_a;
    BYTE        byServiceField_a;
    WORD        wTransmitLength_a;
    WORD        wDuration_ba;
    WORD        wDuration_aa;
    WORD        wDuration_bb;
    WORD        wReserved;
    SRTSData    Data;
} __attribute__ ((__packed__))
SRTS_g, *PSRTS_g;
typedef const SRTS_g *PCSRTS_g;

typedef struct tagSRTS_g_FB {
    BYTE        bySignalField_b;
    BYTE        byServiceField_b;
    WORD        wTransmitLength_b;
    BYTE        bySignalField_a;
    BYTE        byServiceField_a;
    WORD        wTransmitLength_a;
    WORD        wDuration_ba;
    WORD        wDuration_aa;
    WORD        wDuration_bb;
    WORD        wReserved;
    WORD        wRTSDuration_ba_f0;
    WORD        wRTSDuration_aa_f0;
    WORD        wRTSDuration_ba_f1;
    WORD        wRTSDuration_aa_f1;
    SRTSData    Data;
} __attribute__ ((__packed__))
SRTS_g_FB, *PSRTS_g_FB;

typedef const SRTS_g_FB *PCSRTS_g_FB;

typedef struct tagSRTS_ab {
    BYTE        bySignalField;
    BYTE        byServiceField;
    WORD        wTransmitLength;
    WORD        wDuration;
    WORD        wReserved;
    SRTSData    Data;
} __attribute__ ((__packed__))
SRTS_ab, *PSRTS_ab;

typedef const SRTS_ab *PCSRTS_ab;

typedef struct tagSRTS_a_FB {
    BYTE        bySignalField;
    BYTE        byServiceField;
    WORD        wTransmitLength;
    WORD        wDuration;
    WORD        wReserved;
    WORD        wRTSDuration_f0;
    WORD        wRTSDuration_f1;
    SRTSData    Data;
} __attribute__ ((__packed__))
SRTS_a_FB, *PSRTS_a_FB;

typedef const SRTS_a_FB *PCSRTS_a_FB;


typedef struct tagSCTSData {
    WORD    wFrameControl;
    WORD    wDurationID;
    BYTE    abyRA[ETH_ALEN];
    WORD    wReserved;
} __attribute__ ((__packed__))
SCTSData, *PSCTSData;

typedef struct tagSCTS {
    BYTE        bySignalField_b;
    BYTE        byServiceField_b;
    WORD        wTransmitLength_b;
    WORD        wDuration_ba;
    WORD        wReserved;
    SCTSData    Data;
} __attribute__ ((__packed__))
SCTS, *PSCTS;

typedef const SCTS *PCSCTS;

typedef struct tagSCTS_FB {
    BYTE        bySignalField_b;
    BYTE        byServiceField_b;
    WORD        wTransmitLength_b;
    WORD        wDuration_ba;
    WORD        wReserved;
    WORD        wCTSDuration_ba_f0;
    WORD        wCTSDuration_ba_f1;
    SCTSData    Data;
} __attribute__ ((__packed__))
SCTS_FB, *PSCTS_FB;

typedef const SCTS_FB *PCSCTS_FB;

typedef struct tagSTxBufHead {
    DWORD   adwTxKey[4];
    WORD    wFIFOCtl;
    WORD    wTimeStamp;
    WORD    wFragCtl;
    WORD    wReserved;
} __attribute__ ((__packed__))
STxBufHead, *PSTxBufHead;
typedef const STxBufHead *PCSTxBufHead;

typedef struct tagSTxShortBufHead {
    WORD    wFIFOCtl;
    WORD    wTimeStamp;
} __attribute__ ((__packed__))
STxShortBufHead, *PSTxShortBufHead;
typedef const STxShortBufHead *PCSTxShortBufHead;

typedef struct tagSTxDataHead_g {
    BYTE    bySignalField_b;
    BYTE    byServiceField_b;
    WORD    wTransmitLength_b;
    BYTE    bySignalField_a;
    BYTE    byServiceField_a;
    WORD    wTransmitLength_a;
    WORD    wDuration_b;
    WORD    wDuration_a;
    WORD    wTimeStampOff_b;
    WORD    wTimeStampOff_a;
} __attribute__ ((__packed__))
STxDataHead_g, *PSTxDataHead_g;

typedef const STxDataHead_g *PCSTxDataHead_g;

typedef struct tagSTxDataHead_g_FB {
    BYTE    bySignalField_b;
    BYTE    byServiceField_b;
    WORD    wTransmitLength_b;
    BYTE    bySignalField_a;
    BYTE    byServiceField_a;
    WORD    wTransmitLength_a;
    WORD    wDuration_b;
    WORD    wDuration_a;
    WORD    wDuration_a_f0;
    WORD    wDuration_a_f1;
    WORD    wTimeStampOff_b;
    WORD    wTimeStampOff_a;
} __attribute__ ((__packed__))
STxDataHead_g_FB, *PSTxDataHead_g_FB;
typedef const STxDataHead_g_FB *PCSTxDataHead_g_FB;

typedef struct tagSTxDataHead_ab {
    BYTE    bySignalField;
    BYTE    byServiceField;
    WORD    wTransmitLength;
    WORD    wDuration;
    WORD    wTimeStampOff;
} __attribute__ ((__packed__))
STxDataHead_ab, *PSTxDataHead_ab;
typedef const STxDataHead_ab *PCSTxDataHead_ab;

typedef struct tagSTxDataHead_a_FB {
    BYTE    bySignalField;
    BYTE    byServiceField;
    WORD    wTransmitLength;
    WORD    wDuration;
    WORD    wTimeStampOff;
    WORD    wDuration_f0;
    WORD    wDuration_f1;
} __attribute__ ((__packed__))
STxDataHead_a_FB, *PSTxDataHead_a_FB;
typedef const STxDataHead_a_FB *PCSTxDataHead_a_FB;

typedef struct tagSMICHDRHead {
    DWORD   adwHDR0[4];
    DWORD   adwHDR1[4];
    DWORD   adwHDR2[4];
} __attribute__ ((__packed__))
SMICHDRHead, *PSMICHDRHead;

typedef const SMICHDRHead *PCSMICHDRHead;

typedef struct tagSBEACONCtl {
    DWORD   BufReady : 1;
    DWORD   TSF : 15;
    DWORD   BufLen : 11;
    DWORD   Reserved : 5;
} __attribute__ ((__packed__))
SBEACONCtl;

typedef struct tagSSecretKey {
    DWORD   dwLowDword;
    BYTE    byHighByte;
} __attribute__ ((__packed__))
SSecretKey;

typedef struct tagSKeyEntry {
    BYTE  abyAddrHi[2];
    WORD  wKCTL;
    BYTE  abyAddrLo[4];
    DWORD dwKey0[4];
    DWORD dwKey1[4];
    DWORD dwKey2[4];
    DWORD dwKey3[4];
    DWORD dwKey4[4];
} __attribute__ ((__packed__))
SKeyEntry;




#endif 
