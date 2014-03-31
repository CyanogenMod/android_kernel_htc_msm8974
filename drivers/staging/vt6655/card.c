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
 * File: card.c
 * Purpose: Provide functions to setup NIC operation mode
 * Functions:
 *      s_vSafeResetTx - Rest Tx
 *      CARDvSetRSPINF - Set RSPINF
 *      vUpdateIFS - Update slotTime,SIFS,DIFS, and EIFS
 *      CARDvUpdateBasicTopRate - Update BasicTopRate
 *      CARDbAddBasicRate - Add to BasicRateSet
 *      CARDbSetBasicRate - Set Basic Tx Rate
 *      CARDbIsOFDMinBasicRate - Check if any OFDM rate is in BasicRateSet
 *      CARDvSetLoopbackMode - Set Loopback mode
 *      CARDbSoftwareReset - Sortware reset NIC
 *      CARDqGetTSFOffset - Caculate TSFOffset
 *      CARDbGetCurrentTSF - Read Current NIC TSF counter
 *      CARDqGetNextTBTT - Caculate Next Beacon TSF counter
 *      CARDvSetFirstNextTBTT - Set NIC Beacon time
 *      CARDvUpdateNextTBTT - Sync. NIC Beacon time
 *      CARDbRadioPowerOff - Turn Off NIC Radio Power
 *      CARDbRadioPowerOn - Turn On NIC Radio Power
 *      CARDbSetWEPMode - Set NIC Wep mode
 *      CARDbSetTxPower - Set NIC tx power
 *
 * Revision History:
 *      06-10-2003 Bryan YC Fan:  Re-write codes to support VT3253 spec.
 *      08-26-2003 Kyle Hsu:      Modify the defination type of dwIoBase.
 *      09-01-2003 Bryan YC Fan:  Add vUpdateIFS().
 *
 */

#include "tmacro.h"
#include "card.h"
#include "baseband.h"
#include "mac.h"
#include "desc.h"
#include "rf.h"
#include "vntwifi.h"
#include "power.h"
#include "key.h"
#include "rc4.h"
#include "country.h"
#include "channel.h"


static int          msglevel                =MSG_LEVEL_INFO;

#define C_SIFS_A        16      
#define C_SIFS_BG       10

#define C_EIFS          80      


#define C_SLOT_SHORT    9       
#define C_SLOT_LONG     20

#define C_CWMIN_A       15      
#define C_CWMIN_B       31

#define C_CWMAX         1023    

#define WAIT_BEACON_TX_DOWN_TMO         3    

                                                              
static unsigned char abyDefaultSuppRatesG[] = {WLAN_EID_SUPP_RATES, 8, 0x02, 0x04, 0x0B, 0x16, 0x24, 0x30, 0x48, 0x6C};
                                                                    
static unsigned char abyDefaultExtSuppRatesG[] = {WLAN_EID_EXTSUPP_RATES, 4, 0x0C, 0x12, 0x18, 0x60};
                                                              
static unsigned char abyDefaultSuppRatesA[] = {WLAN_EID_SUPP_RATES, 8, 0x0C, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6C};
                                                              
static unsigned char abyDefaultSuppRatesB[] = {WLAN_EID_SUPP_RATES, 4, 0x02, 0x04, 0x0B, 0x16};




const unsigned short cwRXBCNTSFOff[MAX_RATE] =
{17, 17, 17, 17, 34, 23, 17, 11, 8, 5, 4, 3};



static
void
s_vCaculateOFDMRParameter(
    unsigned char byRate,
    CARD_PHY_TYPE ePHYType,
    unsigned char *pbyTxRate,
    unsigned char *pbyRsvTime
    );



static
void
s_vCaculateOFDMRParameter (
    unsigned char byRate,
    CARD_PHY_TYPE ePHYType,
    unsigned char *pbyTxRate,
    unsigned char *pbyRsvTime
    )
{
    switch (byRate) {
    case RATE_6M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x9B;
            *pbyRsvTime = 44;
        }
        else {
            *pbyTxRate = 0x8B;
            *pbyRsvTime = 50;
        }
        break;

    case RATE_9M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x9F;
            *pbyRsvTime = 36;
        }
        else {
            *pbyTxRate = 0x8F;
            *pbyRsvTime = 42;
        }
        break;

   case RATE_12M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x9A;
            *pbyRsvTime = 32;
        }
        else {
            *pbyTxRate = 0x8A;
            *pbyRsvTime = 38;
        }
        break;

   case RATE_18M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x9E;
            *pbyRsvTime = 28;
        }
        else {
            *pbyTxRate = 0x8E;
            *pbyRsvTime = 34;
        }
        break;

    case RATE_36M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x9D;
            *pbyRsvTime = 24;
        }
        else {
            *pbyTxRate = 0x8D;
            *pbyRsvTime = 30;
        }
        break;

    case RATE_48M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x98;
            *pbyRsvTime = 24;
        }
        else {
            *pbyTxRate = 0x88;
            *pbyRsvTime = 30;
        }
        break;

    case RATE_54M :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x9C;
            *pbyRsvTime = 24;
        }
        else {
            *pbyTxRate = 0x8C;
            *pbyRsvTime = 30;
        }
        break;

    case RATE_24M :
    default :
        if (ePHYType == PHY_TYPE_11A) {
            *pbyTxRate = 0x99;
            *pbyRsvTime = 28;
        }
        else {
            *pbyTxRate = 0x89;
            *pbyRsvTime = 34;
        }
        break;
    }
}



static
void
s_vSetRSPINF (PSDevice pDevice, CARD_PHY_TYPE ePHYType, void *pvSupportRateIEs, void *pvExtSupportRateIEs)
{
    unsigned char byServ = 0, bySignal = 0; 
    unsigned short wLen = 0;
    unsigned char byTxRate = 0, byRsvTime = 0;    

    
    MACvSelectPage1(pDevice->PortOffset);

    
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_1M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_1, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_2M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_2, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_5M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_5, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    BBvCaculateParameter(pDevice,
                         14,
                         VNTWIFIbyGetACKTxRate(RATE_11M, pvSupportRateIEs, pvExtSupportRateIEs),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_11, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    s_vCaculateOFDMRParameter(RATE_6M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_6, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_9M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_9, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_12M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_12, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_18M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_18, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_24M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_24, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(
                              VNTWIFIbyGetACKTxRate(RATE_36M, pvSupportRateIEs, pvExtSupportRateIEs),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_36, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(
                              VNTWIFIbyGetACKTxRate(RATE_48M, pvSupportRateIEs, pvExtSupportRateIEs),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_48, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(
                              VNTWIFIbyGetACKTxRate(RATE_54M, pvSupportRateIEs, pvExtSupportRateIEs),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_54, MAKEWORD(byTxRate,byRsvTime));
    
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_72, MAKEWORD(byTxRate,byRsvTime));
    
    MACvSelectPage0(pDevice->PortOffset);
}




bool CARDbIsShortPreamble (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    if (pDevice->byPreambleType == 0) {
        return(false);
    }
    return(true);
}

bool CARDbIsShorSlotTime (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    return(pDevice->bShortSlotTime);
}


bool CARDbSetPhyParameter (void *pDeviceHandler, CARD_PHY_TYPE ePHYType, unsigned short wCapInfo, unsigned char byERPField, void *pvSupportRateIEs, void *pvExtSupportRateIEs)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned char byCWMaxMin = 0;
    unsigned char bySlot = 0;
    unsigned char bySIFS = 0;
    unsigned char byDIFS = 0;
    unsigned char byData;
    PWLAN_IE_SUPP_RATES pSupportRates = (PWLAN_IE_SUPP_RATES) pvSupportRateIEs;
    PWLAN_IE_SUPP_RATES pExtSupportRates = (PWLAN_IE_SUPP_RATES) pvExtSupportRateIEs;


    
    if (ePHYType == PHY_TYPE_11A) {
        if (pSupportRates == NULL) {
            pSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultSuppRatesA;
        }
        if (pDevice->byRFType == RF_AIROHA7230) {
            
            MACvSetBBType(pDevice->PortOffset, BB_TYPE_11G);
            pDevice->abyBBVGA[0] = 0x20;
            pDevice->abyBBVGA[2] = 0x10;
            pDevice->abyBBVGA[3] = 0x10;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x1C) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
            }
        } else if (pDevice->byRFType == RF_UW2452) {
            MACvSetBBType(pDevice->PortOffset, BB_TYPE_11A);
            pDevice->abyBBVGA[0] = 0x18;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x14) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
                BBbWriteEmbeded(pDevice->PortOffset, 0xE1, 0x57);
            }
        } else {
            MACvSetBBType(pDevice->PortOffset, BB_TYPE_11A);
        }
        BBbWriteEmbeded(pDevice->PortOffset, 0x88, 0x03);
        bySlot = C_SLOT_SHORT;
        bySIFS = C_SIFS_A;
        byDIFS = C_SIFS_A + 2*C_SLOT_SHORT;
        byCWMaxMin = 0xA4;
    } else if (ePHYType == PHY_TYPE_11B) {
        if (pSupportRates == NULL) {
            pSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultSuppRatesB;
        }
        MACvSetBBType(pDevice->PortOffset, BB_TYPE_11B);
        if (pDevice->byRFType == RF_AIROHA7230) {
            pDevice->abyBBVGA[0] = 0x1C;
            pDevice->abyBBVGA[2] = 0x00;
            pDevice->abyBBVGA[3] = 0x00;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x20) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
            }
        } else if (pDevice->byRFType == RF_UW2452) {
            pDevice->abyBBVGA[0] = 0x14;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x18) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
                BBbWriteEmbeded(pDevice->PortOffset, 0xE1, 0xD3);
            }
        }
        BBbWriteEmbeded(pDevice->PortOffset, 0x88, 0x02);
        bySlot = C_SLOT_LONG;
        bySIFS = C_SIFS_BG;
        byDIFS = C_SIFS_BG + 2*C_SLOT_LONG;
        byCWMaxMin = 0xA5;
    } else {
        if (pSupportRates == NULL) {
            pSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultSuppRatesG;
            pExtSupportRates = (PWLAN_IE_SUPP_RATES) abyDefaultExtSuppRatesG;
        }
        MACvSetBBType(pDevice->PortOffset, BB_TYPE_11G);
        if (pDevice->byRFType == RF_AIROHA7230) {
            pDevice->abyBBVGA[0] = 0x1C;
            pDevice->abyBBVGA[2] = 0x00;
            pDevice->abyBBVGA[3] = 0x00;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x20) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
            }
        } else if (pDevice->byRFType == RF_UW2452) {
            pDevice->abyBBVGA[0] = 0x14;
            BBbReadEmbeded(pDevice->PortOffset, 0xE7, &byData);
            if (byData == 0x18) {
                BBbWriteEmbeded(pDevice->PortOffset, 0xE7, pDevice->abyBBVGA[0]);
                BBbWriteEmbeded(pDevice->PortOffset, 0xE1, 0xD3);
            }
        }
        BBbWriteEmbeded(pDevice->PortOffset, 0x88, 0x08);
        bySIFS = C_SIFS_BG;
        if(VNTWIFIbIsShortSlotTime(wCapInfo)) {
            bySlot = C_SLOT_SHORT;
            byDIFS = C_SIFS_BG + 2*C_SLOT_SHORT;
        } else {
            bySlot = C_SLOT_LONG;
            byDIFS = C_SIFS_BG + 2*C_SLOT_LONG;
	    }
        if (VNTWIFIbyGetMaxSupportRate(pSupportRates, pExtSupportRates) > RATE_11M) {
            byCWMaxMin = 0xA4;
        } else {
            byCWMaxMin = 0xA5;
        }
        if (pDevice->bProtectMode != VNTWIFIbIsProtectMode(byERPField)) {
            pDevice->bProtectMode = VNTWIFIbIsProtectMode(byERPField);
            if (pDevice->bProtectMode) {
                MACvEnableProtectMD(pDevice->PortOffset);
            } else {
                MACvDisableProtectMD(pDevice->PortOffset);
            }
        }
        if (pDevice->bBarkerPreambleMd != VNTWIFIbIsBarkerMode(byERPField)) {
            pDevice->bBarkerPreambleMd = VNTWIFIbIsBarkerMode(byERPField);
            if (pDevice->bBarkerPreambleMd) {
                MACvEnableBarkerPreambleMd(pDevice->PortOffset);
            } else {
                MACvDisableBarkerPreambleMd(pDevice->PortOffset);
            }
        }
    }

    if (pDevice->byRFType == RF_RFMD2959) {
        
        
        bySIFS -= 3;
        byDIFS -= 3;
    
    
    
    }

    if (pDevice->bySIFS != bySIFS) {
        pDevice->bySIFS = bySIFS;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SIFS, pDevice->bySIFS);
    }
    if (pDevice->byDIFS != byDIFS) {
        pDevice->byDIFS = byDIFS;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_DIFS, pDevice->byDIFS);
    }
    if (pDevice->byEIFS != C_EIFS) {
        pDevice->byEIFS = C_EIFS;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_EIFS, pDevice->byEIFS);
    }
    if (pDevice->bySlot != bySlot) {
        pDevice->bySlot = bySlot;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SLOT, pDevice->bySlot);
        if (pDevice->bySlot == C_SLOT_SHORT) {
            pDevice->bShortSlotTime = true;
        } else {
            pDevice->bShortSlotTime = false;
        }
        BBvSetShortSlotTime(pDevice);
    }
    if (pDevice->byCWMaxMin != byCWMaxMin) {
        pDevice->byCWMaxMin = byCWMaxMin;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_CWMAXMIN0, pDevice->byCWMaxMin);
    }
    if (VNTWIFIbIsShortPreamble(wCapInfo)) {
        pDevice->byPreambleType = pDevice->byShortPreamble;
    } else {
        pDevice->byPreambleType = 0;
    }
    s_vSetRSPINF(pDevice, ePHYType, pSupportRates, pExtSupportRates);
    pDevice->eCurrentPHYType = ePHYType;
    
    return (true);
}

bool CARDbUpdateTSF (void *pDeviceHandler, unsigned char byRxRate, QWORD qwBSSTimestamp, QWORD qwLocalTSF)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    QWORD       qwTSFOffset;

    HIDWORD(qwTSFOffset) = 0;
    LODWORD(qwTSFOffset) = 0;

    if ((HIDWORD(qwBSSTimestamp) != HIDWORD(qwLocalTSF)) ||
        (LODWORD(qwBSSTimestamp) != LODWORD(qwLocalTSF))) {
        qwTSFOffset = CARDqGetTSFOffset(byRxRate, qwBSSTimestamp, qwLocalTSF);
        
        
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_TSFOFST, LODWORD(qwTSFOffset));
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_TSFOFST + 4, HIDWORD(qwTSFOffset));
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TFTCTL, TFTCTL_TSFSYNCEN);
    }
    return(true);
}


bool CARDbSetBeaconPeriod (void *pDeviceHandler, unsigned short wBeaconInterval)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int uBeaconInterval = 0;
    unsigned int uLowNextTBTT = 0;
    unsigned int uHighRemain = 0;
    unsigned int uLowRemain = 0;
    QWORD       qwNextTBTT;

    HIDWORD(qwNextTBTT) = 0;
    LODWORD(qwNextTBTT) = 0;
    CARDbGetCurrentTSF(pDevice->PortOffset, &qwNextTBTT); 
    uBeaconInterval = wBeaconInterval * 1024;
    
    uLowNextTBTT = (LODWORD(qwNextTBTT) >> 10) << 10;
    uLowRemain = (uLowNextTBTT) % uBeaconInterval;
    
    uHighRemain = (((0xffffffff % uBeaconInterval) + 1) * HIDWORD(qwNextTBTT))
                  % uBeaconInterval;
    uLowRemain = (uHighRemain + uLowRemain) % uBeaconInterval;
    uLowRemain = uBeaconInterval - uLowRemain;

    
    if ((~uLowNextTBTT) < uLowRemain) {
        HIDWORD(qwNextTBTT) ++ ;
    }
    LODWORD(qwNextTBTT) = uLowNextTBTT + uLowRemain;

    
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_BI, wBeaconInterval);
    pDevice->wBeaconInterval = wBeaconInterval;
    
    VNSvOutPortD(pDevice->PortOffset + MAC_REG_NEXTTBTT, LODWORD(qwNextTBTT));
    VNSvOutPortD(pDevice->PortOffset + MAC_REG_NEXTTBTT + 4, HIDWORD(qwNextTBTT));
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TFTCTL, TFTCTL_TBTTSYNCEN);

    return(true);
}



bool CARDbStopTxPacket (void *pDeviceHandler, CARD_PKT_TYPE ePktType)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;


    if (ePktType == PKT_TYPE_802_11_ALL) {
        pDevice->bStopBeacon = true;
        pDevice->bStopTx0Pkt = true;
        pDevice->bStopDataPkt = true;
    } else if (ePktType == PKT_TYPE_802_11_BCN) {
        pDevice->bStopBeacon = true;
    } else if (ePktType == PKT_TYPE_802_11_MNG) {
        pDevice->bStopTx0Pkt = true;
    } else if (ePktType == PKT_TYPE_802_11_DATA) {
        pDevice->bStopDataPkt = true;
    }

    if (pDevice->bStopBeacon == true) {
        if (pDevice->bIsBeaconBufReadySet == true) {
            if (pDevice->cbBeaconBufReadySetCnt < WAIT_BEACON_TX_DOWN_TMO) {
                pDevice->cbBeaconBufReadySetCnt ++;
                return(false);
            }
        }
        pDevice->bIsBeaconBufReadySet = false;
        pDevice->cbBeaconBufReadySetCnt = 0;
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_TCR, TCR_AUTOBCNTX);
    }
    
    if (pDevice->bStopTx0Pkt == true) {
         if (pDevice->iTDUsed[TYPE_TXDMA0] != 0){
            return(false);
        }
    }
    
    if (pDevice->bStopDataPkt == true) {
        if (pDevice->iTDUsed[TYPE_AC0DMA] != 0){
            return(false);
        }
    }

    return(true);
}


bool CARDbStartTxPacket (void *pDeviceHandler, CARD_PKT_TYPE ePktType)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;


    if (ePktType == PKT_TYPE_802_11_ALL) {
        pDevice->bStopBeacon = false;
        pDevice->bStopTx0Pkt = false;
        pDevice->bStopDataPkt = false;
    } else if (ePktType == PKT_TYPE_802_11_BCN) {
        pDevice->bStopBeacon = false;
    } else if (ePktType == PKT_TYPE_802_11_MNG) {
        pDevice->bStopTx0Pkt = false;
    } else if (ePktType == PKT_TYPE_802_11_DATA) {
        pDevice->bStopDataPkt = false;
    }

    if ((pDevice->bStopBeacon == false) &&
        (pDevice->bBeaconBufReady == true) &&
        (pDevice->eOPMode == OP_MODE_ADHOC)) {
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TCR, TCR_AUTOBCNTX);
    }

    return(true);
}



bool CARDbSetBSSID(void *pDeviceHandler, unsigned char *pbyBSSID, CARD_OP_MODE eOPMode)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    MACvWriteBSSIDAddress(pDevice->PortOffset, pbyBSSID);
    memcpy(pDevice->abyBSSID, pbyBSSID, WLAN_BSSID_LEN);
    if (eOPMode == OP_MODE_ADHOC) {
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_ADHOC);
    } else {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_ADHOC);
    }
    if (eOPMode == OP_MODE_AP) {
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_AP);
    } else {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_AP);
    }
    if (eOPMode == OP_MODE_UNKNOWN) {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_RCR, RCR_BSSID);
        pDevice->bBSSIDFilter = false;
        pDevice->byRxMode &= ~RCR_BSSID;
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "wcmd: rx_mode = %x\n", pDevice->byRxMode );
    } else {
        if (is_zero_ether_addr(pDevice->abyBSSID) == false) {
            MACvRegBitsOn(pDevice->PortOffset, MAC_REG_RCR, RCR_BSSID);
            pDevice->bBSSIDFilter = true;
            pDevice->byRxMode |= RCR_BSSID;
	    }
	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "wmgr: rx_mode = %x\n", pDevice->byRxMode );
    }
    
    pDevice->eOPMode = eOPMode;
    return(true);
}






bool CARDbSetTxDataRate(
    void *pDeviceHandler,
    unsigned short wDataRate
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    pDevice->wCurrentRate = wDataRate;
    return(true);
}

bool
CARDbPowerDown(
    void *pDeviceHandler
    )
{
    PSDevice        pDevice = (PSDevice)pDeviceHandler;
    unsigned int uIdx;

    
    if (MACbIsRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PS))
        return true;

    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PSEN);

    

    for (uIdx = 0; uIdx < TYPE_MAXTD; uIdx ++) {
        if (pDevice->iTDUsed[uIdx] != 0)
            return false;
    }

    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_GO2DOZE);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Go to Doze ZZZZZZZZZZZZZZZ\n");
    return true;
}

bool CARDbRadioPowerOff (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    bool bResult = true;

    if (pDevice->bRadioOff == true)
        return true;


    switch (pDevice->byRFType) {

        case RF_RFMD2959:
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_TXPEINV);
            MACvWordRegBitsOn(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE1);
            break;

        case RF_AIROHA:
        case RF_AL2230S:
        case RF_AIROHA7230: 
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE2);
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE3);
            break;

    }

    MACvRegBitsOff(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_RXON);

    BBvSetDeepSleep(pDevice->PortOffset, pDevice->byLocalID);

    pDevice->bRadioOff = true;
     
printk("chester power off\n");
MACvRegBitsOn(pDevice->PortOffset, MAC_REG_GPIOCTL0, LED_ACTSET);  
    return bResult;
}


bool CARDbRadioPowerOn (void *pDeviceHandler)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    bool bResult = true;
printk("chester power on\n");
    if (pDevice->bRadioControlOff == true){
if (pDevice->bHWRadioOff == true) printk("chester bHWRadioOff\n");
if (pDevice->bRadioControlOff == true) printk("chester bRadioControlOff\n");
        return false;}

    if (pDevice->bRadioOff == false)
       {
printk("chester pbRadioOff\n");
return true;}

    BBvExitDeepSleep(pDevice->PortOffset, pDevice->byLocalID);

    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_HOSTCR, HOSTCR_RXON);

    switch (pDevice->byRFType) {

        case RF_RFMD2959:
            MACvWordRegBitsOn(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_TXPEINV);
            MACvWordRegBitsOff(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE1);
            break;

        case RF_AIROHA:
        case RF_AL2230S:
        case RF_AIROHA7230: 
            MACvWordRegBitsOn(pDevice->PortOffset, MAC_REG_SOFTPWRCTL, (SOFTPWRCTL_SWPE2 |
                                                                        SOFTPWRCTL_SWPE3));
            break;

    }

    pDevice->bRadioOff = false;
printk("chester power on\n");
MACvRegBitsOff(pDevice->PortOffset, MAC_REG_GPIOCTL0, LED_ACTSET); 
    return bResult;
}



bool CARDbRemoveKey (void *pDeviceHandler, unsigned char *pbyBSSID)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    KeybRemoveAllKey(&(pDevice->sKey), pbyBSSID, pDevice->PortOffset);
    return (true);
}


bool
CARDbAdd_PMKID_Candidate (
    void *pDeviceHandler,
    unsigned char *pbyBSSID,
    bool bRSNCapExist,
    unsigned short wRSNCap
    )
{
    PSDevice            pDevice = (PSDevice) pDeviceHandler;
    PPMKID_CANDIDATE    pCandidateList;
    unsigned int ii = 0;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"bAdd_PMKID_Candidate START: (%d)\n", (int)pDevice->gsPMKIDCandidate.NumCandidates);

    if (pDevice->gsPMKIDCandidate.NumCandidates >= MAX_PMKIDLIST) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"vFlush_PMKID_Candidate: 3\n");
        memset(&pDevice->gsPMKIDCandidate, 0, sizeof(SPMKIDCandidateEvent));
    }

    for (ii = 0; ii < 6; ii++) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"%02X ", *(pbyBSSID + ii));
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\n");


    
    for (ii = 0; ii < pDevice->gsPMKIDCandidate.NumCandidates; ii++) {
        pCandidateList = &pDevice->gsPMKIDCandidate.CandidateList[ii];
        if ( !memcmp(pCandidateList->BSSID, pbyBSSID, ETH_ALEN)) {
            if ((bRSNCapExist == true) && (wRSNCap & BIT0)) {
                pCandidateList->Flags |= NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED;
            } else {
                pCandidateList->Flags &= ~(NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED);
            }
            return true;
        }
    }

    
    pCandidateList = &pDevice->gsPMKIDCandidate.CandidateList[pDevice->gsPMKIDCandidate.NumCandidates];
    if ((bRSNCapExist == true) && (wRSNCap & BIT0)) {
        pCandidateList->Flags |= NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED;
    } else {
        pCandidateList->Flags &= ~(NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED);
    }
    memcpy(pCandidateList->BSSID, pbyBSSID, ETH_ALEN);
    pDevice->gsPMKIDCandidate.NumCandidates++;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"NumCandidates:%d\n", (int)pDevice->gsPMKIDCandidate.NumCandidates);
    return true;
}

void *
CARDpGetCurrentAddress (
    void *pDeviceHandler
    )
{
    PSDevice            pDevice = (PSDevice) pDeviceHandler;

    return (pDevice->abyCurrentNetAddr);
}

bool
CARDbStartMeasure (
    void *pDeviceHandler,
    void *pvMeasureEIDs,
    unsigned int uNumOfMeasureEIDs
    )
{
    PSDevice                pDevice = (PSDevice) pDeviceHandler;
    PWLAN_IE_MEASURE_REQ    pEID = (PWLAN_IE_MEASURE_REQ) pvMeasureEIDs;
    QWORD                   qwCurrTSF;
    QWORD                   qwStartTSF;
    bool bExpired = true;
    unsigned short wDuration = 0;

    if ((pEID == NULL) ||
        (uNumOfMeasureEIDs == 0)) {
        return (true);
    }
    CARDbGetCurrentTSF(pDevice->PortOffset, &qwCurrTSF);
    if (pDevice->bMeasureInProgress == true) {
        pDevice->bMeasureInProgress = false;
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_RCR, pDevice->byOrgRCR);
        MACvSelectPage1(pDevice->PortOffset);
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MAR0, pDevice->dwOrgMAR0);
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MAR4, pDevice->dwOrgMAR4);
        
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_MSRCTL, MSRCTL_EN);
        MACvSelectPage0(pDevice->PortOffset);
        set_channel(pDevice, pDevice->byOrgChannel);
        MACvSelectPage1(pDevice->PortOffset);
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL+1, MSRCTL1_TXPAUSE);
        MACvSelectPage0(pDevice->PortOffset);
    }
    pDevice->uNumOfMeasureEIDs = uNumOfMeasureEIDs;

    do {
        pDevice->pCurrMeasureEID = pEID;
        pEID++;
        pDevice->uNumOfMeasureEIDs--;

        if (pDevice->byLocalID > REV_ID_VT3253_B1) {
            HIDWORD(qwStartTSF) = HIDWORD(*((PQWORD) (pDevice->pCurrMeasureEID->sReq.abyStartTime)));
            LODWORD(qwStartTSF) = LODWORD(*((PQWORD) (pDevice->pCurrMeasureEID->sReq.abyStartTime)));
            wDuration = *((unsigned short *) (pDevice->pCurrMeasureEID->sReq.abyDuration));
            wDuration += 1; 

            if ((LODWORD(qwStartTSF) == 0) && (HIDWORD(qwStartTSF) == 0)) {
                
                LODWORD(qwStartTSF) = LODWORD(qwCurrTSF) + 2048;
                HIDWORD(qwStartTSF) = HIDWORD(qwCurrTSF);
                if (LODWORD(qwCurrTSF) > LODWORD(qwStartTSF)) {
                    HIDWORD(qwStartTSF)++;
                }
                bExpired = false;
                break;
            } else {
                
                if (LODWORD(qwStartTSF) < 1024) {
                    HIDWORD(qwStartTSF)--;
                }
                LODWORD(qwStartTSF) -= 1024;
            }

            if ((HIDWORD(qwCurrTSF) < HIDWORD(qwStartTSF)) ||
                ((HIDWORD(qwCurrTSF) == HIDWORD(qwStartTSF)) &&
                (LODWORD(qwCurrTSF) < LODWORD(qwStartTSF)))
                ) {
                bExpired = false;
                break;
            }
            VNTWIFIbMeasureReport(  pDevice->pMgmt,
                                    false,
                                    pDevice->pCurrMeasureEID,
                                    MEASURE_MODE_LATE,
                                    pDevice->byBasicMap,
                                    pDevice->byCCAFraction,
                                    pDevice->abyRPIs
                                    );
        } else {
            
            VNTWIFIbMeasureReport(  pDevice->pMgmt,
                                    false,
                                    pDevice->pCurrMeasureEID,
                                    MEASURE_MODE_INCAPABLE,
                                    pDevice->byBasicMap,
                                    pDevice->byCCAFraction,
                                    pDevice->abyRPIs
                                    );
        }
    } while (pDevice->uNumOfMeasureEIDs != 0);

    if (bExpired == false) {
        MACvSelectPage1(pDevice->PortOffset);
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MSRSTART, LODWORD(qwStartTSF));
        VNSvOutPortD(pDevice->PortOffset + MAC_REG_MSRSTART + 4, HIDWORD(qwStartTSF));
        VNSvOutPortW(pDevice->PortOffset + MAC_REG_MSRDURATION, wDuration);
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL, MSRCTL_EN);
        MACvSelectPage0(pDevice->PortOffset);
    } else {
        
        VNTWIFIbMeasureReport(  pDevice->pMgmt,
                                true,
                                NULL,
                                0,
                                pDevice->byBasicMap,
                                pDevice->byCCAFraction,
                                pDevice->abyRPIs
                                );
    }
    return (true);
}


bool
CARDbChannelSwitch (
    void *pDeviceHandler,
    unsigned char byMode,
    unsigned char byNewChannel,
    unsigned char byCount
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    bool bResult = true;

    if (byCount == 0) {
        bResult = set_channel(pDevice, byNewChannel);
        VNTWIFIbChannelSwitch(pDevice->pMgmt, byNewChannel);
        MACvSelectPage1(pDevice->PortOffset);
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL+1, MSRCTL1_TXPAUSE);
        MACvSelectPage0(pDevice->PortOffset);
        return(bResult);
    }
    pDevice->byChannelSwitchCount = byCount;
    pDevice->byNewChannel = byNewChannel;
    pDevice->bChannelSwitch = true;
    if (byMode == 1) {
        bResult=CARDbStopTxPacket(pDevice, PKT_TYPE_802_11_ALL);
    }
    return (bResult);
}


bool
CARDbSetQuiet (
    void *pDeviceHandler,
    bool bResetQuiet,
    unsigned char byQuietCount,
    unsigned char byQuietPeriod,
    unsigned short wQuietDuration,
    unsigned short wQuietOffset
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int ii = 0;

    if (bResetQuiet == true) {
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_MSRCTL, (MSRCTL_QUIETTXCHK | MSRCTL_QUIETEN));
        for(ii=0;ii<MAX_QUIET_COUNT;ii++) {
            pDevice->sQuiet[ii].bEnable = false;
        }
        pDevice->uQuietEnqueue = 0;
        pDevice->bEnableFirstQuiet = false;
        pDevice->bQuietEnable = false;
        pDevice->byQuietStartCount = byQuietCount;
    }
    if (pDevice->sQuiet[pDevice->uQuietEnqueue].bEnable == false) {
        pDevice->sQuiet[pDevice->uQuietEnqueue].bEnable = true;
        pDevice->sQuiet[pDevice->uQuietEnqueue].byPeriod = byQuietPeriod;
        pDevice->sQuiet[pDevice->uQuietEnqueue].wDuration = wQuietDuration;
        pDevice->sQuiet[pDevice->uQuietEnqueue].dwStartTime = (unsigned long) byQuietCount;
        pDevice->sQuiet[pDevice->uQuietEnqueue].dwStartTime *= pDevice->wBeaconInterval;
        pDevice->sQuiet[pDevice->uQuietEnqueue].dwStartTime += wQuietOffset;
        pDevice->uQuietEnqueue++;
        pDevice->uQuietEnqueue %= MAX_QUIET_COUNT;
        if (pDevice->byQuietStartCount < byQuietCount) {
            pDevice->byQuietStartCount = byQuietCount;
        }
    } else {
        
    }
    return (true);
}


bool
CARDbStartQuiet (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int ii = 0;
    unsigned long dwStartTime = 0xFFFFFFFF;
    unsigned int uCurrentQuietIndex = 0;
    unsigned long dwNextTime = 0;
    unsigned long dwGap = 0;
    unsigned long dwDuration = 0;

    for(ii=0;ii<MAX_QUIET_COUNT;ii++) {
        if ((pDevice->sQuiet[ii].bEnable == true) &&
            (dwStartTime > pDevice->sQuiet[ii].dwStartTime)) {
            dwStartTime = pDevice->sQuiet[ii].dwStartTime;
            uCurrentQuietIndex = ii;
        }
    }
    if (dwStartTime == 0xFFFFFFFF) {
        
        pDevice->bQuietEnable = false;
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_MSRCTL, (MSRCTL_QUIETTXCHK | MSRCTL_QUIETEN));
    } else {
        if (pDevice->bQuietEnable == false) {
            
            pDevice->byQuietStartCount--;
            dwNextTime = pDevice->sQuiet[uCurrentQuietIndex].dwStartTime;
            dwNextTime %= pDevice->wBeaconInterval;
            MACvSelectPage1(pDevice->PortOffset);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETINIT, (unsigned short) dwNextTime);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETDUR, (unsigned short) pDevice->sQuiet[uCurrentQuietIndex].wDuration);
            if (pDevice->byQuietStartCount == 0) {
                pDevice->bEnableFirstQuiet = false;
                MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL, (MSRCTL_QUIETTXCHK | MSRCTL_QUIETEN));
            } else {
                pDevice->bEnableFirstQuiet = true;
            }
            MACvSelectPage0(pDevice->PortOffset);
        } else {
            if (pDevice->dwCurrentQuietEndTime > pDevice->sQuiet[uCurrentQuietIndex].dwStartTime) {
                
                dwGap =  pDevice->dwCurrentQuietEndTime - pDevice->sQuiet[uCurrentQuietIndex].dwStartTime;
                if (dwGap >= pDevice->sQuiet[uCurrentQuietIndex].wDuration) {
                    
                    return (false);
                }
                dwDuration = pDevice->sQuiet[uCurrentQuietIndex].wDuration - dwGap;
                dwGap = 0;
            } else {
                dwGap = pDevice->sQuiet[uCurrentQuietIndex].dwStartTime - pDevice->dwCurrentQuietEndTime;
                dwDuration = pDevice->sQuiet[uCurrentQuietIndex].wDuration;
            }
            
            MACvSelectPage1(pDevice->PortOffset);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETGAP, (unsigned short) dwGap);
            VNSvOutPortW(pDevice->PortOffset + MAC_REG_QUIETDUR, (unsigned short) dwDuration);
            MACvRegBitsOn(pDevice->PortOffset, MAC_REG_MSRCTL, MSRCTL_QUIETRPT);
            MACvSelectPage0(pDevice->PortOffset);
        }
        pDevice->bQuietEnable = true;
        pDevice->dwCurrentQuietEndTime = pDevice->sQuiet[uCurrentQuietIndex].dwStartTime;
        pDevice->dwCurrentQuietEndTime += pDevice->sQuiet[uCurrentQuietIndex].wDuration;
        if (pDevice->sQuiet[uCurrentQuietIndex].byPeriod == 0) {
            
            pDevice->sQuiet[uCurrentQuietIndex].bEnable = false;
        } else {
            
            dwNextTime = (unsigned long) pDevice->sQuiet[uCurrentQuietIndex].byPeriod;
            dwNextTime *= pDevice->wBeaconInterval;
            pDevice->sQuiet[uCurrentQuietIndex].dwStartTime = dwNextTime;
        }
        if (pDevice->dwCurrentQuietEndTime > 0x80010000) {
            
            for(ii=0;ii<MAX_QUIET_COUNT;ii++) {
                if (pDevice->sQuiet[ii].bEnable == true) {
                    pDevice->sQuiet[ii].dwStartTime -= 0x80000000;
                }
            }
            pDevice->dwCurrentQuietEndTime -= 0x80000000;
        }
    }
    return (true);
}

void
CARDvSetPowerConstraint (
    void *pDeviceHandler,
    unsigned char byChannel,
    char byPower
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    if (byChannel > CB_MAX_CHANNEL_24G) {
        if (pDevice->bCountryInfo5G == true) {
            pDevice->abyLocalPwr[byChannel] = pDevice->abyRegPwr[byChannel] - byPower;
        }
    } else {
        if (pDevice->bCountryInfo24G == true) {
            pDevice->abyLocalPwr[byChannel] = pDevice->abyRegPwr[byChannel] - byPower;
        }
    }
}


void
CARDvGetPowerCapability (
    void *pDeviceHandler,
    unsigned char *pbyMinPower,
    unsigned char *pbyMaxPower
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned char byDec = 0;

    *pbyMaxPower = pDevice->abyOFDMDefaultPwr[pDevice->byCurrentCh];
    byDec = pDevice->abyOFDMPwrTbl[pDevice->byCurrentCh];
    if (pDevice->byRFType == RF_UW2452) {
        byDec *= 3;
        byDec >>= 1;
    } else {
        byDec <<= 1;
    }
    *pbyMinPower = pDevice->abyOFDMDefaultPwr[pDevice->byCurrentCh] - byDec;
}

char
CARDbyGetTransmitPower (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;

    return (pDevice->byCurPwrdBm);
}

void
CARDvSafeResetTx (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int uu;
    PSTxDesc    pCurrTD;

    
    pDevice->apTailTD[0] = pDevice->apCurrTD[0] = &(pDevice->apTD0Rings[0]);
    pDevice->apTailTD[1] = pDevice->apCurrTD[1] = &(pDevice->apTD1Rings[0]);

    for (uu = 0; uu < TYPE_MAXTD; uu ++)
        pDevice->iTDUsed[uu] = 0;

    for (uu = 0; uu < pDevice->sOpts.nTxDescs[0]; uu++) {
        pCurrTD = &(pDevice->apTD0Rings[uu]);
        pCurrTD->m_td0TD0.f1Owner = OWNED_BY_HOST;
        
    }
    for (uu = 0; uu < pDevice->sOpts.nTxDescs[1]; uu++) {
        pCurrTD = &(pDevice->apTD1Rings[uu]);
        pCurrTD->m_td0TD0.f1Owner = OWNED_BY_HOST;
        
    }

    
    MACvSetCurrTXDescAddr(TYPE_TXDMA0, pDevice->PortOffset,
                        (pDevice->td0_pool_dma));

    MACvSetCurrTXDescAddr(TYPE_AC0DMA, pDevice->PortOffset,
                        (pDevice->td1_pool_dma));

    
    MACvSetCurrBCNTxDescAddr(pDevice->PortOffset,
                        (pDevice->tx_beacon_dma));

}



void
CARDvSafeResetRx (
    void *pDeviceHandler
    )
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int uu;
    PSRxDesc    pDesc;



    
    pDevice->pCurrRD[0]=&(pDevice->aRD0Ring[0]);
    pDevice->pCurrRD[1]=&(pDevice->aRD1Ring[0]);

    
    for (uu = 0; uu < pDevice->sOpts.nRxDescs0; uu++) {
        pDesc =&(pDevice->aRD0Ring[uu]);
        pDesc->m_rd0RD0.wResCount = (unsigned short)(pDevice->rx_buf_sz);
        pDesc->m_rd0RD0.f1Owner=OWNED_BY_NIC;
        pDesc->m_rd1RD1.wReqCount = (unsigned short)(pDevice->rx_buf_sz);
    }

    
    for (uu = 0; uu < pDevice->sOpts.nRxDescs1; uu++) {
        pDesc =&(pDevice->aRD1Ring[uu]);
        pDesc->m_rd0RD0.wResCount = (unsigned short)(pDevice->rx_buf_sz);
        pDesc->m_rd0RD0.f1Owner=OWNED_BY_NIC;
        pDesc->m_rd1RD1.wReqCount = (unsigned short)(pDevice->rx_buf_sz);
    }

    pDevice->cbDFCB = CB_MAX_RX_FRAG;
    pDevice->cbFreeDFCB = pDevice->cbDFCB;

    
    MACvRx0PerPktMode(pDevice->PortOffset);
    MACvRx1PerPktMode(pDevice->PortOffset);
    
    MACvSetCurrRx0DescAddr(pDevice->PortOffset,
                            pDevice->rd0_pool_dma);

    MACvSetCurrRx1DescAddr(pDevice->PortOffset,
                            pDevice->rd1_pool_dma);
}




unsigned short CARDwGetCCKControlRate(void *pDeviceHandler, unsigned short wRateIdx)
{
    PSDevice    pDevice = (PSDevice) pDeviceHandler;
    unsigned int ui = (unsigned int) wRateIdx;

    while (ui > RATE_1M) {
        if (pDevice->wBasicRate & ((unsigned short)1 << ui)) {
            return (unsigned short)ui;
        }
        ui --;
    }
    return (unsigned short)RATE_1M;
}

unsigned short CARDwGetOFDMControlRate (void *pDeviceHandler, unsigned short wRateIdx)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    unsigned int ui = (unsigned int) wRateIdx;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"BASIC RATE: %X\n", pDevice->wBasicRate);

    if (!CARDbIsOFDMinBasicRate((void *)pDevice)) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CARDwGetOFDMControlRate:(NO OFDM) %d\n", wRateIdx);
        if (wRateIdx > RATE_24M)
            wRateIdx = RATE_24M;
        return wRateIdx;
    }
    while (ui > RATE_11M) {
        if (pDevice->wBasicRate & ((unsigned short)1 << ui)) {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CARDwGetOFDMControlRate : %d\n", ui);
            return (unsigned short)ui;
        }
        ui --;
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CARDwGetOFDMControlRate: 6M\n");
    return (unsigned short)RATE_24M;
}


void CARDvSetRSPINF (void *pDeviceHandler, CARD_PHY_TYPE ePHYType)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    unsigned char byServ = 0x00, bySignal = 0x00; 
    unsigned short wLen = 0x0000;
    unsigned char byTxRate, byRsvTime;             

    
    MACvSelectPage1(pDevice->PortOffset);

    
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_1M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_1, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_2M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_2, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_5M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_5, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    BBvCaculateParameter(pDevice,
                         14,
                         CARDwGetCCKControlRate((void *)pDevice, RATE_11M),
                         PK_TYPE_11B,
                         &wLen,
                         &byServ,
                         &bySignal
    );

    VNSvOutPortD(pDevice->PortOffset + MAC_REG_RSPINF_B_11, MAKEDWORD(wLen,MAKEWORD(bySignal,byServ)));
    
    s_vCaculateOFDMRParameter(RATE_6M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_6, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_9M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_9, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_12M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_12, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_18M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
   VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_18, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(RATE_24M,
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_24, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_36M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_36, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_48M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_48, MAKEWORD(byTxRate,byRsvTime));
    
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_54M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_54, MAKEWORD(byTxRate,byRsvTime));

    
    s_vCaculateOFDMRParameter(CARDwGetOFDMControlRate((void *)pDevice, RATE_54M),
                              ePHYType,
                              &byTxRate,
                              &byRsvTime);
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_RSPINF_A_72, MAKEWORD(byTxRate,byRsvTime));
    
    MACvSelectPage0(pDevice->PortOffset);
}

void vUpdateIFS (void *pDeviceHandler)
{
    
    PSDevice pDevice = (PSDevice) pDeviceHandler;

    unsigned char byMaxMin = 0;
    if (pDevice->byPacketType==PK_TYPE_11A) {
        pDevice->uSlot = C_SLOT_SHORT;
        pDevice->uSIFS = C_SIFS_A;
        pDevice->uDIFS = C_SIFS_A + 2*C_SLOT_SHORT;
        pDevice->uCwMin = C_CWMIN_A;
        byMaxMin = 4;
    }
    else if (pDevice->byPacketType==PK_TYPE_11B) {
        pDevice->uSlot = C_SLOT_LONG;
        pDevice->uSIFS = C_SIFS_BG;
        pDevice->uDIFS = C_SIFS_BG + 2*C_SLOT_LONG;
	    pDevice->uCwMin = C_CWMIN_B;
        byMaxMin = 5;
    }
    else { 
        pDevice->uSIFS = C_SIFS_BG;
        if (pDevice->bShortSlotTime) {
            pDevice->uSlot = C_SLOT_SHORT;
        } else {
	        pDevice->uSlot = C_SLOT_LONG;
	    }
	    pDevice->uDIFS = C_SIFS_BG + 2*pDevice->uSlot;
        if (pDevice->wBasicRate & 0x0150) { 
            pDevice->uCwMin = C_CWMIN_A;
            byMaxMin = 4;
        }
        else {
            pDevice->uCwMin = C_CWMIN_B;
            byMaxMin = 5;
        }
    }

    pDevice->uCwMax = C_CWMAX;
    pDevice->uEIFS = C_EIFS;
    if (pDevice->byRFType == RF_RFMD2959) {
        
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SIFS, (unsigned char)(pDevice->uSIFS - 3));
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_DIFS, (unsigned char)(pDevice->uDIFS - 3));
    } else {
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_SIFS, (unsigned char)pDevice->uSIFS);
        VNSvOutPortB(pDevice->PortOffset + MAC_REG_DIFS, (unsigned char)pDevice->uDIFS);
    }
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_EIFS, (unsigned char)pDevice->uEIFS);
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_SLOT, (unsigned char)pDevice->uSlot);
    byMaxMin |= 0xA0;
    VNSvOutPortB(pDevice->PortOffset + MAC_REG_CWMAXMIN0, (unsigned char)byMaxMin);
}

void CARDvUpdateBasicTopRate (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    unsigned char byTopOFDM = RATE_24M, byTopCCK = RATE_1M;
    unsigned char ii;

     
     for (ii = RATE_54M; ii >= RATE_6M; ii --) {
         if ( (pDevice->wBasicRate) & ((unsigned short)(1<<ii)) ) {
             byTopOFDM = ii;
             break;
         }
     }
     pDevice->byTopOFDMBasicRate = byTopOFDM;

     for (ii = RATE_11M;; ii --) {
         if ( (pDevice->wBasicRate) & ((unsigned short)(1<<ii)) ) {
             byTopCCK = ii;
             break;
         }
         if (ii == RATE_1M)
            break;
     }
     pDevice->byTopCCKBasicRate = byTopCCK;
}


bool CARDbAddBasicRate (void *pDeviceHandler, unsigned short wRateIdx)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    unsigned short wRate = (unsigned short)(1<<wRateIdx);

    pDevice->wBasicRate |= wRate;

    
    CARDvUpdateBasicTopRate((void *)pDevice);

    return(true);
}

bool CARDbIsOFDMinBasicRate (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;
    int ii;

    for (ii = RATE_54M; ii >= RATE_6M; ii --) {
        if ((pDevice->wBasicRate) & ((unsigned short)(1<<ii)))
            return true;
    }
    return false;
}

unsigned char CARDbyGetPktType (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;

    if (pDevice->byBBType == BB_TYPE_11A || pDevice->byBBType == BB_TYPE_11B) {
        return (unsigned char)pDevice->byBBType;
    }
    else if (CARDbIsOFDMinBasicRate((void *)pDevice)) {
        return PK_TYPE_11GA;
    }
    else {
    	return PK_TYPE_11GB;
    }
}

void CARDvSetLoopbackMode (unsigned long dwIoBase, unsigned short wLoopbackMode)
{
    switch(wLoopbackMode) {
    case CARD_LB_NONE:
    case CARD_LB_MAC:
    case CARD_LB_PHY:
        break;
    default:
        ASSERT(false);
        break;
    }
    
    MACvSetLoopbackMode(dwIoBase, LOBYTE(wLoopbackMode));
    
}


bool CARDbSoftwareReset (void *pDeviceHandler)
{
    PSDevice pDevice = (PSDevice) pDeviceHandler;

    
    if (!MACbSafeSoftwareReset(pDevice->PortOffset))
        return false;

    return true;
}


QWORD CARDqGetTSFOffset (unsigned char byRxRate, QWORD qwTSF1, QWORD qwTSF2)
{
    QWORD   qwTSFOffset;
    unsigned short wRxBcnTSFOffst= 0;

    HIDWORD(qwTSFOffset) = 0;
    LODWORD(qwTSFOffset) = 0;
    wRxBcnTSFOffst = cwRXBCNTSFOff[byRxRate%MAX_RATE];
    (qwTSF2).u.dwLowDword += (unsigned long)(wRxBcnTSFOffst);
    if ((qwTSF2).u.dwLowDword < (unsigned long)(wRxBcnTSFOffst)) {
        (qwTSF2).u.dwHighDword++;
    }
    LODWORD(qwTSFOffset) = LODWORD(qwTSF1) - LODWORD(qwTSF2);
    if (LODWORD(qwTSF1) < LODWORD(qwTSF2)) {
        
        HIDWORD(qwTSFOffset) = HIDWORD(qwTSF1) - HIDWORD(qwTSF2) - 1 ;
    }
    else {
        HIDWORD(qwTSFOffset) = HIDWORD(qwTSF1) - HIDWORD(qwTSF2);
    };
    return (qwTSFOffset);
}


bool CARDbGetCurrentTSF (unsigned long dwIoBase, PQWORD pqwCurrTSF)
{
    unsigned short ww;
    unsigned char byData;

    MACvRegBitsOn(dwIoBase, MAC_REG_TFTCTL, TFTCTL_TSFCNTRRD);
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_TFTCTL, &byData);
        if ( !(byData & TFTCTL_TSFCNTRRD))
            break;
    }
    if (ww == W_MAX_TIMEOUT)
        return(false);
    VNSvInPortD(dwIoBase + MAC_REG_TSFCNTR, &LODWORD(*pqwCurrTSF));
    VNSvInPortD(dwIoBase + MAC_REG_TSFCNTR + 4, &HIDWORD(*pqwCurrTSF));

    return(true);
}


QWORD CARDqGetNextTBTT (QWORD qwTSF, unsigned short wBeaconInterval)
{

    unsigned int uLowNextTBTT;
    unsigned int uHighRemain, uLowRemain;
    unsigned int uBeaconInterval;

    uBeaconInterval = wBeaconInterval * 1024;
    
    uLowNextTBTT = (LODWORD(qwTSF) >> 10) << 10;
    
    uLowRemain = (uLowNextTBTT) % uBeaconInterval;
    
    uHighRemain = (((0xffffffff % uBeaconInterval) + 1) * HIDWORD(qwTSF))
                  % uBeaconInterval;
    uLowRemain = (uHighRemain + uLowRemain) % uBeaconInterval;
    uLowRemain = uBeaconInterval - uLowRemain;

    
    if ((~uLowNextTBTT) < uLowRemain)
        HIDWORD(qwTSF) ++ ;

    LODWORD(qwTSF) = uLowNextTBTT + uLowRemain;

    return (qwTSF);
}


void CARDvSetFirstNextTBTT (unsigned long dwIoBase, unsigned short wBeaconInterval)
{

    QWORD   qwNextTBTT;

    HIDWORD(qwNextTBTT) = 0;
    LODWORD(qwNextTBTT) = 0;
    CARDbGetCurrentTSF(dwIoBase, &qwNextTBTT); 
    qwNextTBTT = CARDqGetNextTBTT(qwNextTBTT, wBeaconInterval);
    
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT, LODWORD(qwNextTBTT));
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT + 4, HIDWORD(qwNextTBTT));
    MACvRegBitsOn(dwIoBase, MAC_REG_TFTCTL, TFTCTL_TBTTSYNCEN);
    
    return;
}


void CARDvUpdateNextTBTT (unsigned long dwIoBase, QWORD qwTSF, unsigned short wBeaconInterval)
{

    qwTSF = CARDqGetNextTBTT(qwTSF, wBeaconInterval);
    
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT, LODWORD(qwTSF));
    VNSvOutPortD(dwIoBase + MAC_REG_NEXTTBTT + 4, HIDWORD(qwTSF));
    MACvRegBitsOn(dwIoBase, MAC_REG_TFTCTL, TFTCTL_TBTTSYNCEN);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Card:Update Next TBTT[%8xh:%8xh] \n",
		    (unsigned int) HIDWORD(qwTSF), (unsigned int) LODWORD(qwTSF));

    return;
}







