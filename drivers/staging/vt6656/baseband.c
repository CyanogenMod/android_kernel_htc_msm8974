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
 *
 * File: baseband.c
 *
 * Purpose: Implement functions to access baseband
 *
 * Author: Jerry Chen
 *
 * Date: Jun. 5, 2002
 *
 * Functions:
 *      BBuGetFrameTime        - Calculate data frame transmitting time
 *      BBvCaculateParameter   - Caculate PhyLength, PhyService and Phy Signal parameter for baseband Tx
 *      BBbVT3184Init          - VIA VT3184 baseband chip init code
 *      BBvLoopbackOn          - Turn on BaseBand Loopback mode
 *      BBvLoopbackOff         - Turn off BaseBand Loopback mode
 *
 * Revision History:
 *
 *
 */

#include "tmacro.h"
#include "tether.h"
#include "mac.h"
#include "baseband.h"
#include "rf.h"
#include "srom.h"
#include "control.h"
#include "datarate.h"
#include "rndis.h"

static int          msglevel                =MSG_LEVEL_INFO;









BYTE abyVT3184_AGC[] = {
    0x00,   
    0x00,   
    0x02,   
    0x02,   
    0x04,   
    0x04,   
    0x06,   
    0x06,   
    0x08,   
    0x08,   
    0x0A,   
    0x0A,   
    0x0C,   
    0x0C,   
    0x0E,   
    0x0E,   
    0x10,   
    0x10,   
    0x12,   
    0x12,   
    0x14,   
    0x14,   
    0x16,   
    0x16,   
    0x18,   
    0x18,   
    0x1A,   
    0x1A,   
    0x1C,   
    0x1C,   
    0x1E,   
    0x1E,   
    0x20,   
    0x20,   
    0x22,   
    0x22,   
    0x24,   
    0x24,   
    0x26,   
    0x26,   
    0x28,   
    0x28,   
    0x2A,   
    0x2A,   
    0x2C,   
    0x2C,   
    0x2E,   
    0x2E,   
    0x30,   
    0x30,   
    0x32,   
    0x32,   
    0x34,   
    0x34,   
    0x36,   
    0x36,   
    0x38,   
    0x38,   
    0x3A,   
    0x3A,   
    0x3C,   
    0x3C,   
    0x3E,   
    0x3E    
};


BYTE abyVT3184_AL2230[] = {
        0x31,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x00,
        0x00,
        0x70,
        0x45,
        0x2A,
        0x76,
        0x00,
        0x00,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8e,       
        0x0a,       
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x4a,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x4a,
        0x00,
        0x0c,       
        0x26,
        0x5b,
        0x00,
        0x00,
        0x00,
        0x00,
        0xaa,
        0xaa,
        0xff,
        0xff,
        0x79,
        0x00,
        0x00,
        0x0b,
        0x48,
        0x04,
        0x00,
        0x08,
        0x00,
        0x08,
        0x08,
        0x14,
        0x05,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x09,
        0x73,
        0x00,
        0xc5,
        0x00,
        0x19,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xd0,       
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xe4,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x98,
        0x0a,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,       
        0x03,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8c,
        0x01,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x1f,       
        0xb7,
        0x88,
        0x47,
        0xaa,
        0x00,       
        0x20,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xeb,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x18,
        0x00,
        0x00,
        0x00,
        0x00,
        0x15,       
        0x00,
        0x18,
        0x38,
        0x30,
        0x00,
        0x00,
        0xff,
        0x0f,
        0xe4,
        0xe2,
        0x00,
        0x00,
        0x00,
        0x03,
        0x01,
        0x00,
        0x00,
        0x00,
        0x18,
        0x20,
        0x07,
        0x18,
        0xff,
        0xff,       
        0x0e,       
        0x0a,
        0x0e,
        0x00,       
        0x82,       
        0xa7,
        0x3c,
        0x10,
        0x30,       
        0x05,       
        0x40,
        0x12,
        0x00,
        0x00,
        0x10,
        0x28,
        0x80,
        0x2A,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xf3,       
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x12,       
        0x00,
        0xf4,
        0x00,
        0xff,
        0x79,
        0x20,
        0x30,
        0x05,       
        0x00,
        0x3e,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
};



BYTE abyVT3184_VT3226D0[] = {
        0x31,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x00,
        0x00,
        0x70,
        0x45,
        0x2A,
        0x76,
        0x00,
        0x00,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8e,       
        0x0a,       
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x4a,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x4a,
        0x00,
        0x0c,       
        0x26,
        0x5b,
        0x00,
        0x00,
        0x00,
        0x00,
        0xaa,
        0xaa,
        0xff,
        0xff,
        0x79,
        0x00,
        0x00,
        0x0b,
        0x48,
        0x04,
        0x00,
        0x08,
        0x00,
        0x08,
        0x08,
        0x14,
        0x05,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x09,
        0x73,
        0x00,
        0xc5,
        0x00,
        0x19,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xd0,       
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xe4,
        0x80,
        0x00,
        0x00,
        0x00,
        0x00,
        0x98,
        0x0a,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,       
        0x03,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8c,
        0x01,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x08,
        0x00,
        0x1f,       
        0xb7,
        0x88,
        0x47,
        0xaa,
        0x00,       
        0x20,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xeb,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x18,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x18,
        0x38,
        0x30,
        0x00,
        0x00,
        0xff,
        0x0f,
        0xe4,
        0xe2,
        0x00,
        0x00,
        0x00,
        0x03,
        0x01,
        0x00,
        0x00,
        0x00,
        0x18,
        0x20,
        0x07,
        0x18,
        0xff,
        0xff,       
        0x10,       
        0x0a,
        0x0e,
        0x00,       
        0x84,       
        0xa7,
        0x3c,
        0x10,
        0x24,       
        0x05,       
        0x40,
        0x12,
        0x00,
        0x00,
        0x10,
        0x28,
        0x80,
        0x2A,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xf3,       
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x10,       
        0x00,
        0xf4,
        0x00,
        0xff,
        0x79,
        0x20,
        0x30,
        0x08,       
        0x00,
        0x3e,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
};

const WORD awcFrameTime[MAX_RATE] =
{10, 20, 55, 110, 24, 36, 48, 72, 96, 144, 192, 216};



unsigned int
BBuGetFrameTime (
     BYTE byPreambleType,
     BYTE byPktType,
     unsigned int cbFrameLength,
     WORD wRate
    )
{
    unsigned int uFrameTime;
    unsigned int uPreamble;
    unsigned int uTmp;
    unsigned int uRateIdx = (unsigned int)wRate;
    unsigned int uRate = 0;


    if (uRateIdx > RATE_54M) {
        ASSERT(0);
        return 0;
    }

    uRate = (unsigned int)awcFrameTime[uRateIdx];

    if (uRateIdx <= 3) {          

        if (byPreambleType == 1) {
            uPreamble = 96;
        } else {
            uPreamble = 192;
        }
        uFrameTime = (cbFrameLength * 80) / uRate;  
        uTmp = (uFrameTime * uRate) / 80;
        if (cbFrameLength != uTmp) {
            uFrameTime ++;
        }

        return (uPreamble + uFrameTime);
    }
    else {
        uFrameTime = (cbFrameLength * 8 + 22) / uRate;   
        uTmp = ((uFrameTime * uRate) - 22) / 8;
        if(cbFrameLength != uTmp) {
            uFrameTime ++;
        }
        uFrameTime = uFrameTime * 4;    
        if(byPktType != PK_TYPE_11A) {
            uFrameTime += 6;
        }
        return (20 + uFrameTime); 
    }
}

void
BBvCaculateParameter (
      PSDevice pDevice,
      unsigned int cbFrameLength,
      WORD wRate,
      BYTE byPacketType,
     PWORD pwPhyLen,
     PBYTE pbyPhySrv,
     PBYTE pbyPhySgn
    )
{
    unsigned int cbBitCount;
    unsigned int cbUsCount = 0;
    unsigned int cbTmp;
    BOOL bExtBit;
    BYTE byPreambleType = pDevice->byPreambleType;
    BOOL bCCK = pDevice->bCCK;

    cbBitCount = cbFrameLength * 8;
    bExtBit = FALSE;

    switch (wRate) {
    case RATE_1M :
        cbUsCount = cbBitCount;
        *pbyPhySgn = 0x00;
        break;

    case RATE_2M :
        cbUsCount = cbBitCount / 2;
        if (byPreambleType == 1)
            *pbyPhySgn = 0x09;
        else 
            *pbyPhySgn = 0x01;
        break;

    case RATE_5M :
        if (bCCK == FALSE)
            cbBitCount ++;
        cbUsCount = (cbBitCount * 10) / 55;
        cbTmp = (cbUsCount * 55) / 10;
        if (cbTmp != cbBitCount)
            cbUsCount ++;
        if (byPreambleType == 1)
            *pbyPhySgn = 0x0a;
        else 
            *pbyPhySgn = 0x02;
        break;

    case RATE_11M :

        if (bCCK == FALSE)
            cbBitCount ++;
        cbUsCount = cbBitCount / 11;
        cbTmp = cbUsCount * 11;
        if (cbTmp != cbBitCount) {
            cbUsCount ++;
            if ((cbBitCount - cbTmp) <= 3)
                bExtBit = TRUE;
        }
        if (byPreambleType == 1)
            *pbyPhySgn = 0x0b;
        else 
            *pbyPhySgn = 0x03;
        break;

    case RATE_6M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9B; 
        }
        else {
            *pbyPhySgn = 0x8B; 
        }
        break;

    case RATE_9M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9F; 
        }
        else {
            *pbyPhySgn = 0x8F; 
        }
        break;

    case RATE_12M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9A; 
        }
        else {
            *pbyPhySgn = 0x8A; 
        }
        break;

    case RATE_18M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9E; 
        }
        else {
            *pbyPhySgn = 0x8E; 
        }
        break;

    case RATE_24M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x99; 
        }
        else {
            *pbyPhySgn = 0x89; 
        }
        break;

    case RATE_36M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9D; 
        }
        else {
            *pbyPhySgn = 0x8D; 
        }
        break;

    case RATE_48M :
        if(byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x98; 
        }
        else {
            *pbyPhySgn = 0x88; 
        }
        break;

    case RATE_54M :
        if (byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9C; 
        }
        else {
            *pbyPhySgn = 0x8C; 
        }
        break;

    default :
        if (byPacketType == PK_TYPE_11A) {
            *pbyPhySgn = 0x9C; 
        }
        else {
            *pbyPhySgn = 0x8C; 
        }
        break;
    }

    if (byPacketType == PK_TYPE_11B) {
        *pbyPhySrv = 0x00;
        if (bExtBit)
            *pbyPhySrv = *pbyPhySrv | 0x80;
        *pwPhyLen = (WORD) cbUsCount;
    }
    else {
        *pbyPhySrv = 0x00;
        *pwPhyLen = (WORD)cbFrameLength;
    }
}


void
BBvSetAntennaMode (PSDevice pDevice, BYTE byAntennaMode)
{
    switch (byAntennaMode) {
        case ANT_TXA:
        case ANT_TXB:
            break;
        case ANT_RXA:
            pDevice->byBBRxConf &= 0xFC;
            break;
        case ANT_RXB:
            pDevice->byBBRxConf &= 0xFE;
            pDevice->byBBRxConf |= 0x02;
            break;
    }


    CONTROLnsRequestOut(pDevice,
                    MESSAGE_TYPE_SET_ANTMD,
                    (WORD) byAntennaMode,
                    0,
                    0,
                    NULL);
}


BOOL BBbVT3184Init(PSDevice pDevice)
{
	int ntStatus;
    WORD                    wLength;
    PBYTE                   pbyAddr;
    PBYTE                   pbyAgc;
    WORD                    wLengthAgc;
    BYTE                    abyArray[256];

    ntStatus = CONTROLnsRequestIn(pDevice,
                                  MESSAGE_TYPE_READ,
                                  0,
                                  MESSAGE_REQUEST_EEPROM,
                                  EEP_MAX_CONTEXT_SIZE,
                                  pDevice->abyEEPROM);
    if (ntStatus != STATUS_SUCCESS) {
        return FALSE;
    }



 pDevice->byOriginalZonetype = pDevice->abyEEPROM[EEP_OFS_ZONETYPE];
 if(pDevice->config_file.ZoneType >= 0) {         
  if ((pDevice->config_file.ZoneType == 0)&&
        (pDevice->abyEEPROM[EEP_OFS_ZONETYPE] !=0x00)){          
    pDevice->abyEEPROM[EEP_OFS_ZONETYPE] = 0;
    pDevice->abyEEPROM[EEP_OFS_MAXCHANNEL] = 0x0B;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Init Zone Type :USA\n");
  }
 else if((pDevice->config_file.ZoneType == 1)&&
 	     (pDevice->abyEEPROM[EEP_OFS_ZONETYPE]!=0x01)){   
    pDevice->abyEEPROM[EEP_OFS_ZONETYPE] = 0x01;
    pDevice->abyEEPROM[EEP_OFS_MAXCHANNEL] = 0x0D;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Init Zone Type :Japan\n");
  }
 else if((pDevice->config_file.ZoneType == 2)&&
 	     (pDevice->abyEEPROM[EEP_OFS_ZONETYPE]!=0x02)){   
    pDevice->abyEEPROM[EEP_OFS_ZONETYPE] = 0x02;
    pDevice->abyEEPROM[EEP_OFS_MAXCHANNEL] = 0x0D;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Init Zone Type :Europe\n");
  }
else {
   if(pDevice->config_file.ZoneType !=pDevice->abyEEPROM[EEP_OFS_ZONETYPE])
      printk("zonetype in file[%02x] mismatch with in EEPROM[%02x]\n",pDevice->config_file.ZoneType,pDevice->abyEEPROM[EEP_OFS_ZONETYPE]);
   else
      printk("Read Zonetype file success,use default zonetype setting[%02x]\n",pDevice->config_file.ZoneType);
 }
}

    if ( !pDevice->bZoneRegExist ) {
        pDevice->byZoneType = pDevice->abyEEPROM[EEP_OFS_ZONETYPE];
    }
    pDevice->byRFType = pDevice->abyEEPROM[EEP_OFS_RFTYPE];

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Zone Type %x\n", pDevice->byZoneType);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"RF Type %d\n", pDevice->byRFType);

    if ((pDevice->byRFType == RF_AL2230) || (pDevice->byRFType == RF_AL2230S)) {
        pDevice->byBBRxConf = abyVT3184_AL2230[10];
        wLength = sizeof(abyVT3184_AL2230);
        pbyAddr = abyVT3184_AL2230;
        pbyAgc = abyVT3184_AGC;
        wLengthAgc = sizeof(abyVT3184_AGC);

        pDevice->abyBBVGA[0] = 0x1C;
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    }
    else if (pDevice->byRFType == RF_AIROHA7230) {
        pDevice->byBBRxConf = abyVT3184_AL2230[10];
        wLength = sizeof(abyVT3184_AL2230);
        pbyAddr = abyVT3184_AL2230;
        pbyAgc = abyVT3184_AGC;
        wLengthAgc = sizeof(abyVT3184_AGC);

        
        
        
        
        
        pbyAddr[0xd7] = 0x06;

        pDevice->abyBBVGA[0] = 0x1C;
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
    }
    else if ( (pDevice->byRFType == RF_VT3226) || (pDevice->byRFType == RF_VT3226D0) ) {
        pDevice->byBBRxConf = abyVT3184_VT3226D0[10];   
        wLength = sizeof(abyVT3184_VT3226D0);           
        pbyAddr = abyVT3184_VT3226D0;                   
        pbyAgc = abyVT3184_AGC;
        wLengthAgc = sizeof(abyVT3184_AGC);

        pDevice->abyBBVGA[0] = 0x20; 
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
        
        MACvRegBitsOn(pDevice, MAC_REG_SOFTPWRCTL2, SOFTPWRCTL_RFLEOPT);
    
    
    } else if ( (pDevice->byRFType == RF_VT3342A0) ) {
        pDevice->byBBRxConf = abyVT3184_VT3226D0[10];
        wLength = sizeof(abyVT3184_VT3226D0);
        pbyAddr = abyVT3184_VT3226D0;
        pbyAgc = abyVT3184_AGC;
        wLengthAgc = sizeof(abyVT3184_AGC);

        pDevice->abyBBVGA[0] = 0x20;
        pDevice->abyBBVGA[1] = 0x10;
        pDevice->abyBBVGA[2] = 0x0;
        pDevice->abyBBVGA[3] = 0x0;
        pDevice->ldBmThreshold[0] = -70;
        pDevice->ldBmThreshold[1] = -48;
        pDevice->ldBmThreshold[2] = 0;
        pDevice->ldBmThreshold[3] = 0;
        
        MACvRegBitsOn(pDevice, MAC_REG_SOFTPWRCTL2, SOFTPWRCTL_RFLEOPT);
    
    } else {
        return TRUE;
    }

   memcpy(abyArray, pbyAddr, wLength);
   CONTROLnsRequestOut(pDevice,
                    MESSAGE_TYPE_WRITE,
                    0,
                    MESSAGE_REQUEST_BBREG,
                    wLength,
                    abyArray
                    );

   memcpy(abyArray, pbyAgc, wLengthAgc);
   CONTROLnsRequestOut(pDevice,
                    MESSAGE_TYPE_WRITE,
                    0,
                    MESSAGE_REQUEST_BBAGC,
                    wLengthAgc,
                    abyArray
                    );


    if ((pDevice->byRFType == RF_VT3226) || 
         (pDevice->byRFType == RF_VT3342A0)  
         ) {
        ControlvWriteByte(pDevice,MESSAGE_REQUEST_MACREG,MAC_REG_ITRTMSET,0x23);
        MACvRegBitsOn(pDevice,MAC_REG_PAPEDELAY,0x01);
    }
    else if (pDevice->byRFType == RF_VT3226D0)
    {
        ControlvWriteByte(pDevice,MESSAGE_REQUEST_MACREG,MAC_REG_ITRTMSET,0x11);
        MACvRegBitsOn(pDevice,MAC_REG_PAPEDELAY,0x01);
    }


    ControlvWriteByte(pDevice,MESSAGE_REQUEST_BBREG,0x04,0x7F);
    ControlvWriteByte(pDevice,MESSAGE_REQUEST_BBREG,0x0D,0x01);

    RFbRFTableDownload(pDevice);
    return TRUE;
}


void BBvLoopbackOn (PSDevice pDevice)
{
    BYTE      byData;

    
    ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0xC9, &pDevice->byBBCRc9);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0);
    ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x4D, &pDevice->byBBCR4d);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x4D, 0x90);

    
    ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x88, &pDevice->byBBCR88);

    if (pDevice->wCurrentRate <= RATE_11M) { 
        
        ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x21, &byData);
        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x21, (BYTE)(byData | 0x01));
        
        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x9A, 0);   

        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x88, 0x02);
    }
    else { 
        
        ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x9A, &byData);
        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x9A, (BYTE)(byData | 0x01));
        
        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x21, 0);   

        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x88, 0x03);
    }

    
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0E, 0);

    
    ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x09, &pDevice->byBBCR09);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x09, (BYTE)(pDevice->byBBCR09 & 0xDE));
}

void BBvLoopbackOff (PSDevice pDevice)
{
    BYTE      byData;

    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, pDevice->byBBCRc9);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x88, pDevice->byBBCR88);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x09, pDevice->byBBCR09);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x4D, pDevice->byBBCR4d);

    if (pDevice->wCurrentRate <= RATE_11M) { 
        
        ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x21, &byData);
        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x21, (BYTE)(byData & 0xFE));
	} else { 
        ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x9A, &byData);
        ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x9A, (BYTE)(byData & 0xFE));
    }
    ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0x0E, &byData);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0E, (BYTE)(byData | 0x80));

}


void
BBvSetShortSlotTime (PSDevice pDevice)
{
    BYTE byBBVGA=0;

	if (pDevice->bShortSlotTime)
        pDevice->byBBRxConf &= 0xDF;
	else
        pDevice->byBBRxConf |= 0x20;

    ControlvReadByte (pDevice, MESSAGE_REQUEST_BBREG, 0xE7, &byBBVGA);
	if (byBBVGA == pDevice->abyBBVGA[0])
        pDevice->byBBRxConf |= 0x20;

    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0A, pDevice->byBBRxConf);
}


void BBvSetVGAGainOffset(PSDevice pDevice, BYTE byData)
{

    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xE7, byData);

    
	if (pDevice->bShortSlotTime)
		pDevice->byBBRxConf &= 0xDF; 
	else
		pDevice->byBBRxConf |= 0x20; 

    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0A, pDevice->byBBRxConf);
}


void
BBvSoftwareReset (PSDevice pDevice)
{
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x50, 0x40);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x50, 0);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x9C, 0x01);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x9C, 0);
}

void
BBvSetDeepSleep (PSDevice pDevice)
{
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0c, 0x17);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0D, 0xB9);
}

void
BBvExitDeepSleep (PSDevice pDevice)
{
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0C, 0x00);
    ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0x0D, 0x01);
}


static unsigned long s_ulGetLowSQ3(PSDevice pDevice)
{
	int ii;
	unsigned long ulSQ3 = 0;
	unsigned long ulMaxPacket;

    ulMaxPacket = pDevice->aulPktNum[RATE_54M];
	if (pDevice->aulPktNum[RATE_54M] != 0)
        ulSQ3 = pDevice->aulSQ3Val[RATE_54M] / pDevice->aulPktNum[RATE_54M];

	for (ii = RATE_48M; ii >= RATE_6M; ii--)
		if (pDevice->aulPktNum[ii] > ulMaxPacket) {
            ulMaxPacket = pDevice->aulPktNum[ii];
            ulSQ3 = pDevice->aulSQ3Val[ii] / pDevice->aulPktNum[ii];
        }

    return ulSQ3;
}

static unsigned long s_ulGetRatio(PSDevice pDevice)
{
	int ii, jj;
	unsigned long ulRatio = 0;
	unsigned long ulMaxPacket;
	unsigned long ulPacketNum;

    
    ulMaxPacket = pDevice->aulPktNum[RATE_54M];
    if ( pDevice->aulPktNum[RATE_54M] != 0 ) {
        ulPacketNum = pDevice->aulPktNum[RATE_54M];
        ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
        ulRatio += TOP_RATE_54M;
    }
	for (ii = RATE_48M; ii >= RATE_1M; ii--)
        if ( pDevice->aulPktNum[ii] > ulMaxPacket ) {
            ulPacketNum = 0;
            for ( jj=RATE_54M;jj>=ii;jj--)
                ulPacketNum += pDevice->aulPktNum[jj];
            ulRatio = (ulPacketNum * 1000 / pDevice->uDiversityCnt);
            ulRatio += TOP_RATE_48M;
            ulMaxPacket = pDevice->aulPktNum[ii];
        }

    return ulRatio;
}


static
void
s_vClearSQ3Value (PSDevice pDevice)
{
    int ii;
    pDevice->uDiversityCnt = 0;

    for ( ii=RATE_1M;ii<MAX_RATE;ii++) {
        pDevice->aulPktNum[ii] = 0;
        pDevice->aulSQ3Val[ii] = 0;
    }
}



void
BBvAntennaDiversity (PSDevice pDevice, BYTE byRxRate, BYTE bySQ3)
{

    pDevice->uDiversityCnt++;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"pDevice->uDiversityCnt = %d\n", (int)pDevice->uDiversityCnt);

    if (byRxRate == 2) {
        pDevice->aulPktNum[RATE_1M]++;
    }
    else if (byRxRate==4) {
        pDevice->aulPktNum[RATE_2M]++;
    }
    else if (byRxRate==11) {
        pDevice->aulPktNum[RATE_5M]++;
    }
    else if (byRxRate==22) {
        pDevice->aulPktNum[RATE_11M]++;
    }
    else if(byRxRate==12){
        pDevice->aulPktNum[RATE_6M]++;
        pDevice->aulSQ3Val[RATE_6M] += bySQ3;
    }
    else if(byRxRate==18){
        pDevice->aulPktNum[RATE_9M]++;
        pDevice->aulSQ3Val[RATE_9M] += bySQ3;
    }
    else if(byRxRate==24){
        pDevice->aulPktNum[RATE_12M]++;
        pDevice->aulSQ3Val[RATE_12M] += bySQ3;
    }
    else if(byRxRate==36){
        pDevice->aulPktNum[RATE_18M]++;
        pDevice->aulSQ3Val[RATE_18M] += bySQ3;
    }
    else if(byRxRate==48){
        pDevice->aulPktNum[RATE_24M]++;
        pDevice->aulSQ3Val[RATE_24M] += bySQ3;
    }
    else if(byRxRate==72){
        pDevice->aulPktNum[RATE_36M]++;
        pDevice->aulSQ3Val[RATE_36M] += bySQ3;
    }
    else if(byRxRate==96){
        pDevice->aulPktNum[RATE_48M]++;
        pDevice->aulSQ3Val[RATE_48M] += bySQ3;
    }
    else if(byRxRate==108){
        pDevice->aulPktNum[RATE_54M]++;
        pDevice->aulSQ3Val[RATE_54M] += bySQ3;
    }

    if (pDevice->byAntennaState == 0) {

        if (pDevice->uDiversityCnt > pDevice->ulDiversityNValue) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ulDiversityNValue=[%d],54M-[%d]\n",(int)pDevice->ulDiversityNValue, (int)pDevice->aulPktNum[RATE_54M]);

            pDevice->ulSQ3_State0 = s_ulGetLowSQ3(pDevice);
            pDevice->ulRatio_State0 = s_ulGetRatio(pDevice);
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"SQ3_State0, SQ3= [%08x] rate = [%08x]\n",(int)pDevice->ulSQ3_State0,(int)pDevice->ulRatio_State0);

            if ( ((pDevice->aulPktNum[RATE_54M] < pDevice->ulDiversityNValue/2) &&
                  (pDevice->ulSQ3_State0 > pDevice->ulSQ3TH) ) ||
                 (pDevice->ulSQ3_State0 == 0 ) )  {

                if ( pDevice->byTMax == 0 )
                    return;

		bScheduleCommand((void *) pDevice,
				 WLAN_CMD_CHANGE_ANTENNA,
				 NULL);

                pDevice->byAntennaState = 1;

                del_timer(&pDevice->TimerSQ3Tmax3);
                del_timer(&pDevice->TimerSQ3Tmax2);
                pDevice->TimerSQ3Tmax1.expires =  RUN_AT(pDevice->byTMax * HZ);
                add_timer(&pDevice->TimerSQ3Tmax1);

            } else {
                pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
                add_timer(&pDevice->TimerSQ3Tmax3);
            }
            s_vClearSQ3Value(pDevice);

        }
    } else { 

        if (pDevice->uDiversityCnt > pDevice->ulDiversityMValue) {

            del_timer(&pDevice->TimerSQ3Tmax1);
            pDevice->ulSQ3_State1 = s_ulGetLowSQ3(pDevice);
            pDevice->ulRatio_State1 = s_ulGetRatio(pDevice);
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"SQ3_State1, rate0 = %08x,rate1 = %08x\n",(int)pDevice->ulRatio_State0,(int)pDevice->ulRatio_State1);

            if ( ((pDevice->ulSQ3_State1 == 0) && (pDevice->ulSQ3_State0 != 0)) ||
                 ((pDevice->ulSQ3_State1 == 0) && (pDevice->ulSQ3_State0 == 0) && (pDevice->ulRatio_State1 < pDevice->ulRatio_State0)) ||
                 ((pDevice->ulSQ3_State1 != 0) && (pDevice->ulSQ3_State0 != 0) && (pDevice->ulSQ3_State0 < pDevice->ulSQ3_State1))
               ) {

		bScheduleCommand((void *) pDevice,
				 WLAN_CMD_CHANGE_ANTENNA,
				 NULL);

                pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
                pDevice->TimerSQ3Tmax2.expires =  RUN_AT(pDevice->byTMax2 * HZ);
                add_timer(&pDevice->TimerSQ3Tmax3);
                add_timer(&pDevice->TimerSQ3Tmax2);

            }
            pDevice->byAntennaState = 0;
            s_vClearSQ3Value(pDevice);
        }
    } 
}



void TimerSQ3CallBack(void *hDeviceContext)
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TimerSQ3CallBack...");
    spin_lock_irq(&pDevice->lock);

    bScheduleCommand((void *) pDevice, WLAN_CMD_CHANGE_ANTENNA, NULL);
    pDevice->byAntennaState = 0;
    s_vClearSQ3Value(pDevice);
    pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
    pDevice->TimerSQ3Tmax2.expires =  RUN_AT(pDevice->byTMax2 * HZ);
    add_timer(&pDevice->TimerSQ3Tmax3);
    add_timer(&pDevice->TimerSQ3Tmax2);


    spin_unlock_irq(&pDevice->lock);
}



void TimerSQ3Tmax3CallBack(void *hDeviceContext)
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TimerSQ3Tmax3CallBack...");
    spin_lock_irq(&pDevice->lock);

    pDevice->ulRatio_State0 = s_ulGetRatio(pDevice);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"SQ3_State0 = [%08x]\n",(int)pDevice->ulRatio_State0);

    s_vClearSQ3Value(pDevice);
    if ( pDevice->byTMax == 0 ) {
        pDevice->TimerSQ3Tmax3.expires =  RUN_AT(pDevice->byTMax3 * HZ);
        add_timer(&pDevice->TimerSQ3Tmax3);
        spin_unlock_irq(&pDevice->lock);
        return;
    }

    bScheduleCommand((void *) pDevice, WLAN_CMD_CHANGE_ANTENNA, NULL);
    pDevice->byAntennaState = 1;
    del_timer(&pDevice->TimerSQ3Tmax3);
    del_timer(&pDevice->TimerSQ3Tmax2);
    pDevice->TimerSQ3Tmax1.expires =  RUN_AT(pDevice->byTMax * HZ);
    add_timer(&pDevice->TimerSQ3Tmax1);

    spin_unlock_irq(&pDevice->lock);
}

void
BBvUpdatePreEDThreshold(
      PSDevice    pDevice,
      BOOL        bScanning)
{


    switch(pDevice->byRFType)
    {
        case RF_AL2230:
        case RF_AL2230S:
        case RF_AIROHA7230:
            

            if( bScanning )
            {   
                if(pDevice->byBBPreEDIndex == 0) break;
                pDevice->byBBPreEDIndex = 0;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x30); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -69, -70, -71,...\n");
                break;
            }

            if(pDevice->byBBPreEDRSSI <= 45) { 
                if(pDevice->byBBPreEDIndex == 20) break;
                pDevice->byBBPreEDIndex = 20;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0xFF); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI 0, -1,-2,..-45\n");
            } else if(pDevice->byBBPreEDRSSI <= 46)  { 
                if(pDevice->byBBPreEDIndex == 19) break;
                pDevice->byBBPreEDIndex = 19;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x1A); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -46\n");
            } else if(pDevice->byBBPreEDRSSI <= 47)  { 
                if(pDevice->byBBPreEDIndex == 18) break;
                pDevice->byBBPreEDIndex = 18;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x15); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -47\n");
            } else if(pDevice->byBBPreEDRSSI <= 49)  { 
                if(pDevice->byBBPreEDIndex == 17) break;
                pDevice->byBBPreEDIndex = 17;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x0E); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -48,-49\n");
            } else if(pDevice->byBBPreEDRSSI <= 51)  { 
                if(pDevice->byBBPreEDIndex == 16) break;
                pDevice->byBBPreEDIndex = 16;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x09); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -50,-51\n");
            } else if(pDevice->byBBPreEDRSSI <= 53)  { 
                if(pDevice->byBBPreEDIndex == 15) break;
                pDevice->byBBPreEDIndex = 15;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x06); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -52,-53\n");
            } else if(pDevice->byBBPreEDRSSI <= 55)  { 
                if(pDevice->byBBPreEDIndex == 14) break;
                pDevice->byBBPreEDIndex = 14;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x03); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -54,-55\n");
            } else if(pDevice->byBBPreEDRSSI <= 56)  { 
                if(pDevice->byBBPreEDIndex == 13) break;
                pDevice->byBBPreEDIndex = 13;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x02); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xA0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -56\n");
            } else if(pDevice->byBBPreEDRSSI <= 57)  { 
                if(pDevice->byBBPreEDIndex == 12) break;
                pDevice->byBBPreEDIndex = 12;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x02); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x20); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -57\n");
            } else if(pDevice->byBBPreEDRSSI <= 58)  { 
                if(pDevice->byBBPreEDIndex == 11) break;
                pDevice->byBBPreEDIndex = 11;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xA0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -58\n");
            } else if(pDevice->byBBPreEDRSSI <= 59)  { 
                if(pDevice->byBBPreEDIndex == 10) break;
                pDevice->byBBPreEDIndex = 10;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x54); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -59\n");
            } else if(pDevice->byBBPreEDRSSI <= 60)  { 
                if(pDevice->byBBPreEDIndex == 9) break;
                pDevice->byBBPreEDIndex = 9;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x18); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -60\n");
            } else if(pDevice->byBBPreEDRSSI <= 61)  { 
                if(pDevice->byBBPreEDIndex == 8) break;
                pDevice->byBBPreEDIndex = 8;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xE3); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -61\n");
            } else if(pDevice->byBBPreEDRSSI <= 62)  { 
                if(pDevice->byBBPreEDIndex == 7) break;
                pDevice->byBBPreEDIndex = 7;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xB9); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -62\n");
            } else if(pDevice->byBBPreEDRSSI <= 63)  { 
                if(pDevice->byBBPreEDIndex == 6) break;
                pDevice->byBBPreEDIndex = 6;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x93); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -63\n");
            } else if(pDevice->byBBPreEDRSSI <= 64)  { 
                if(pDevice->byBBPreEDIndex == 5) break;
                pDevice->byBBPreEDIndex = 5;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x79); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -64\n");
            } else if(pDevice->byBBPreEDRSSI <= 65)  { 
                if(pDevice->byBBPreEDIndex == 4) break;
                pDevice->byBBPreEDIndex = 4;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x62); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -65\n");
            } else if(pDevice->byBBPreEDRSSI <= 66)  { 
                if(pDevice->byBBPreEDIndex == 3) break;
                pDevice->byBBPreEDIndex = 3;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x51); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -66\n");
            } else if(pDevice->byBBPreEDRSSI <= 67)  { 
                if(pDevice->byBBPreEDIndex == 2) break;
                pDevice->byBBPreEDIndex = 2;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x43); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -67\n");
            } else if(pDevice->byBBPreEDRSSI <= 68)  { 
                if(pDevice->byBBPreEDIndex == 1) break;
                pDevice->byBBPreEDIndex = 1;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x36); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -68\n");
            } else { 
                if(pDevice->byBBPreEDIndex == 0) break;
                pDevice->byBBPreEDIndex = 0;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x30); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -69, -70,...\n");
            }
            break;

        case RF_VT3226:
        case RF_VT3226D0:
            

            if( bScanning )
            {   
                if(pDevice->byBBPreEDIndex == 0) break;
                pDevice->byBBPreEDIndex = 0;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x24); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -69, -70,..\n");
                break;
            }

            if(pDevice->byBBPreEDRSSI <= 41) { 
                if(pDevice->byBBPreEDIndex == 22) break;
                pDevice->byBBPreEDIndex = 22;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0xFF); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI 0, -1,-2,..-41\n");
            } else if(pDevice->byBBPreEDRSSI <= 42)  { 
                if(pDevice->byBBPreEDIndex == 21) break;
                pDevice->byBBPreEDIndex = 21;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x36); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -42\n");
            } else if(pDevice->byBBPreEDRSSI <= 43)  { 
                if(pDevice->byBBPreEDIndex == 20) break;
                pDevice->byBBPreEDIndex = 20;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x26); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -43\n");
            } else if(pDevice->byBBPreEDRSSI <= 45)  { 
                if(pDevice->byBBPreEDIndex == 19) break;
                pDevice->byBBPreEDIndex = 19;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x18); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -44,-45\n");
            } else if(pDevice->byBBPreEDRSSI <= 47)  { 
                if(pDevice->byBBPreEDIndex == 18) break;
                pDevice->byBBPreEDIndex = 18;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x11); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -46,-47\n");
            } else if(pDevice->byBBPreEDRSSI <= 49)  { 
                if(pDevice->byBBPreEDIndex == 17) break;
                pDevice->byBBPreEDIndex = 17;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x0a); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -48,-49\n");
            } else if(pDevice->byBBPreEDRSSI <= 51)  { 
                if(pDevice->byBBPreEDIndex == 16) break;
                pDevice->byBBPreEDIndex = 16;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x07); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -50,-51\n");
            } else if(pDevice->byBBPreEDRSSI <= 53)  { 
                if(pDevice->byBBPreEDIndex == 15) break;
                pDevice->byBBPreEDIndex = 15;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x04); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -52,-53\n");
            } else if(pDevice->byBBPreEDRSSI <= 55)  { 
                if(pDevice->byBBPreEDIndex == 14) break;
                pDevice->byBBPreEDIndex = 14;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x02); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xC0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -54,-55\n");
            } else if(pDevice->byBBPreEDRSSI <= 56)  { 
                if(pDevice->byBBPreEDIndex == 13) break;
                pDevice->byBBPreEDIndex = 13;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x02); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x30); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -56\n");
            } else if(pDevice->byBBPreEDRSSI <= 57)  { 
                if(pDevice->byBBPreEDIndex == 12) break;
                pDevice->byBBPreEDIndex = 12;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xB0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -57\n");
            } else if(pDevice->byBBPreEDRSSI <= 58)  { 
                if(pDevice->byBBPreEDIndex == 11) break;
                pDevice->byBBPreEDIndex = 11;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x70); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -58\n");
            } else if(pDevice->byBBPreEDRSSI <= 59)  { 
                if(pDevice->byBBPreEDIndex == 10) break;
                pDevice->byBBPreEDIndex = 10;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x30); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -59\n");
            } else if(pDevice->byBBPreEDRSSI <= 60)  { 
                if(pDevice->byBBPreEDIndex == 9) break;
                pDevice->byBBPreEDIndex = 9;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xEA); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -60\n");
            } else if(pDevice->byBBPreEDRSSI <= 61)  { 
                if(pDevice->byBBPreEDIndex == 8) break;
                pDevice->byBBPreEDIndex = 8;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xC0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -61\n");
            } else if(pDevice->byBBPreEDRSSI <= 62)  { 
                if(pDevice->byBBPreEDIndex == 7) break;
                pDevice->byBBPreEDIndex = 7;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x9C); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -62\n");
            } else if(pDevice->byBBPreEDRSSI <= 63)  { 
                if(pDevice->byBBPreEDIndex == 6) break;
                pDevice->byBBPreEDIndex = 6;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x80); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -63\n");
            } else if(pDevice->byBBPreEDRSSI <= 64)  { 
                if(pDevice->byBBPreEDIndex == 5) break;
                pDevice->byBBPreEDIndex = 5;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x68); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -64\n");
            } else if(pDevice->byBBPreEDRSSI <= 65)  { 
                if(pDevice->byBBPreEDIndex == 4) break;
                pDevice->byBBPreEDIndex = 4;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x52); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -65\n");
            } else if(pDevice->byBBPreEDRSSI <= 66)  { 
                if(pDevice->byBBPreEDIndex == 3) break;
                pDevice->byBBPreEDIndex = 3;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x43); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -66\n");
            } else if(pDevice->byBBPreEDRSSI <= 67)  { 
                if(pDevice->byBBPreEDIndex == 2) break;
                pDevice->byBBPreEDIndex = 2;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x36); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -67\n");
            } else if(pDevice->byBBPreEDRSSI <= 68)  { 
                if(pDevice->byBBPreEDIndex == 1) break;
                pDevice->byBBPreEDIndex = 1;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x2D); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -68\n");
            } else { 
                if(pDevice->byBBPreEDIndex == 0) break;
                pDevice->byBBPreEDIndex = 0;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x24); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -69, -70,..\n");
            }
            break;

        case RF_VT3342A0: 
            if( bScanning )
            {   
                if(pDevice->byBBPreEDIndex == 0) break;
                pDevice->byBBPreEDIndex = 0;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x38); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -67, -68,..\n");
                break;
            }

            if(pDevice->byBBPreEDRSSI <= 41) { 
                if(pDevice->byBBPreEDIndex == 20) break;
                pDevice->byBBPreEDIndex = 20;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0xFF); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI 0, -1,-2,..-41\n");
            } else if(pDevice->byBBPreEDRSSI <= 42)  { 
                if(pDevice->byBBPreEDIndex == 19) break;
                pDevice->byBBPreEDIndex = 19;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x36); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -42\n");
            } else if(pDevice->byBBPreEDRSSI <= 43)  { 
                if(pDevice->byBBPreEDIndex == 18) break;
                pDevice->byBBPreEDIndex = 18;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x26); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -43\n");
            } else if(pDevice->byBBPreEDRSSI <= 45)  { 
                if(pDevice->byBBPreEDIndex == 17) break;
                pDevice->byBBPreEDIndex = 17;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x18); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -44,-45\n");
            } else if(pDevice->byBBPreEDRSSI <= 47)  { 
                if(pDevice->byBBPreEDIndex == 16) break;
                pDevice->byBBPreEDIndex = 16;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x11); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -46,-47\n");
            } else if(pDevice->byBBPreEDRSSI <= 49)  { 
                if(pDevice->byBBPreEDIndex == 15) break;
                pDevice->byBBPreEDIndex = 15;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x0a); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -48,-49\n");
            } else if(pDevice->byBBPreEDRSSI <= 51)  { 
                if(pDevice->byBBPreEDIndex == 14) break;
                pDevice->byBBPreEDIndex = 14;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x07); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -50,-51\n");
            } else if(pDevice->byBBPreEDRSSI <= 53)  { 
                if(pDevice->byBBPreEDIndex == 13) break;
                pDevice->byBBPreEDIndex = 13;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x04); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x00); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -52,-53\n");
            } else if(pDevice->byBBPreEDRSSI <= 55)  { 
                if(pDevice->byBBPreEDIndex == 12) break;
                pDevice->byBBPreEDIndex = 12;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x02); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xC0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -54,-55\n");
            } else if(pDevice->byBBPreEDRSSI <= 56)  { 
                if(pDevice->byBBPreEDIndex == 11) break;
                pDevice->byBBPreEDIndex = 11;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x02); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x30); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -56\n");
            } else if(pDevice->byBBPreEDRSSI <= 57)  { 
                if(pDevice->byBBPreEDIndex == 10) break;
                pDevice->byBBPreEDIndex = 10;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xB0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -57\n");
            } else if(pDevice->byBBPreEDRSSI <= 58)  { 
                if(pDevice->byBBPreEDIndex == 9) break;
                pDevice->byBBPreEDIndex = 9;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x70); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -58\n");
            } else if(pDevice->byBBPreEDRSSI <= 59)  { 
                if(pDevice->byBBPreEDIndex == 8) break;
                pDevice->byBBPreEDIndex = 8;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x01); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x30); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -59\n");
            } else if(pDevice->byBBPreEDRSSI <= 60)  { 
                if(pDevice->byBBPreEDIndex == 7) break;
                pDevice->byBBPreEDIndex = 7;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xEA); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -60\n");
            } else if(pDevice->byBBPreEDRSSI <= 61)  { 
                if(pDevice->byBBPreEDIndex == 6) break;
                pDevice->byBBPreEDIndex = 6;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0xC0); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -61\n");
            } else if(pDevice->byBBPreEDRSSI <= 62)  { 
                if(pDevice->byBBPreEDIndex == 5) break;
                pDevice->byBBPreEDIndex = 5;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x9C); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -62\n");
            } else if(pDevice->byBBPreEDRSSI <= 63)  { 
                if(pDevice->byBBPreEDIndex == 4) break;
                pDevice->byBBPreEDIndex = 4;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x80); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -63\n");
            } else if(pDevice->byBBPreEDRSSI <= 64)  { 
                if(pDevice->byBBPreEDIndex == 3) break;
                pDevice->byBBPreEDIndex = 3;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x68); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -64\n");
            } else if(pDevice->byBBPreEDRSSI <= 65)  { 
                if(pDevice->byBBPreEDIndex == 2) break;
                pDevice->byBBPreEDIndex = 2;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x52); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -65\n");
            } else if(pDevice->byBBPreEDRSSI <= 66)  { 
                if(pDevice->byBBPreEDIndex == 1) break;
                pDevice->byBBPreEDIndex = 1;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x43); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -66\n");
            } else { 
                if(pDevice->byBBPreEDIndex == 0) break;
                pDevice->byBBPreEDIndex = 0;
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xC9, 0x00); 
                ControlvWriteByte(pDevice, MESSAGE_REQUEST_BBREG, 0xCE, 0x38); 
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"            pDevice->byBBPreEDRSSI -67, -68,..\n");
            }
            break;

    }

}

