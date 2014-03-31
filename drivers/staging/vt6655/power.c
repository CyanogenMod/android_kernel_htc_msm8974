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
 * File: power.c
 *
 * Purpose: Handles 802.11 power management  functions
 *
 * Author: Lyndon Chen
 *
 * Date: July 17, 2002
 *
 * Functions:
 *      PSvEnablePowerSaving - Enable Power Saving Mode
 *      PSvDiasblePowerSaving - Disable Power Saving Mode
 *      PSbConsiderPowerDown - Decide if we can Power Down
 *      PSvSendPSPOLL - Send PS-POLL packet
 *      PSbSendNullPacket - Send Null packet
 *      PSbIsNextTBTTWakeUp - Decide if we need to wake up at next Beacon
 *
 * Revision History:
 *
 */

#include "ttype.h"
#include "mac.h"
#include "device.h"
#include "wmgr.h"
#include "power.h"
#include "wcmd.h"
#include "rxtx.h"
#include "card.h"






static int          msglevel                =MSG_LEVEL_INFO;







void
PSvEnablePowerSaving(
    void *hDeviceContext,
    unsigned short wListenInterval
    )
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = pDevice->pMgmt;
    unsigned short wAID = pMgmt->wCurrAID | BIT14 | BIT15;

    
    VNSvOutPortW(pDevice->PortOffset + MAC_REG_PWBT, C_PWBT);
    if (pDevice->eOPMode != OP_MODE_ADHOC) {
        
        VNSvOutPortW(pDevice->PortOffset + MAC_REG_AIDATIM, wAID);
    } else {
    	
        MACvWriteATIMW(pDevice->PortOffset, pMgmt->wCurrATIMWindow);
    }
    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);
    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_TFTCTL, TFTCTL_HWUTSF);

    if (wListenInterval >= 2) {
        
        MACvRegBitsOff(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_ALBCN);
        
        
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_LNBCN);
        pMgmt->wCountToWakeUp = wListenInterval;
    }
    else {
        
        MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_ALBCN);
        
        pMgmt->wCountToWakeUp = 0;
    }

    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PSEN);
    pDevice->bEnablePSMode = true;

    if (pDevice->eOPMode == OP_MODE_ADHOC) {
    }
    
    else if (pDevice->eOPMode == OP_MODE_INFRASTRUCTURE) {
        PSbSendNullPacket(pDevice);
    }
    pDevice->bPWBitOn = true;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "PS:Power Saving Mode Enable... \n");
    return;
}







void
PSvDisablePowerSaving(
    void *hDeviceContext
    )
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;

    
    MACbPSWakeup(pDevice->PortOffset);
    
    MACvRegBitsOff(pDevice->PortOffset, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);
    
    MACvRegBitsOff(pDevice->PortOffset, MAC_REG_TFTCTL, TFTCTL_HWUTSF);
    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_ALBCN);

    pDevice->bEnablePSMode = false;

    if (pDevice->eOPMode == OP_MODE_INFRASTRUCTURE) {
        PSbSendNullPacket(pDevice);
    }
    pDevice->bPWBitOn = false;
    return;
}




bool
PSbConsiderPowerDown(
    void *hDeviceContext,
    bool bCheckRxDMA,
    bool bCheckCountToWakeUp
    )
{
    PSDevice        pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject    pMgmt = pDevice->pMgmt;
    unsigned int uIdx;

    
    if (MACbIsRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PS))
        return true;

    if (pMgmt->eCurrMode != WMAC_MODE_IBSS_STA) {
        
        if (pMgmt->bInTIMWake)
            return false;
    }

    
    if (pDevice->bCmdRunning)
        return false;

    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_PSEN);

    
    for (uIdx = 0; uIdx < TYPE_MAXTD; uIdx ++) {
        if (pDevice->iTDUsed[uIdx] != 0)
            return false;
    }

    
    if (bCheckRxDMA &&
        ((pDevice->dwIsr& ISR_RXDMA0) != 0) &&
        ((pDevice->dwIsr & ISR_RXDMA1) != 0)){
        return false;
    }

    if (pMgmt->eCurrMode != WMAC_MODE_IBSS_STA) {
        if (bCheckCountToWakeUp &&
           (pMgmt->wCountToWakeUp == 0 || pMgmt->wCountToWakeUp == 1)) {
             return false;
        }
    }

    
    MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_GO2DOZE);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Go to Doze ZZZZZZZZZZZZZZZ\n");
    return true;
}






void
PSvSendPSPOLL(
    void *hDeviceContext
    )
{
    PSDevice            pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject        pMgmt = pDevice->pMgmt;
    PSTxMgmtPacket      pTxPacket = NULL;


    memset(pMgmt->pbyPSPacketPool, 0, sizeof(STxMgmtPacket) + WLAN_HDR_ADDR2_LEN);
    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyPSPacketPool;
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((unsigned char *)pTxPacket + sizeof(STxMgmtPacket));
    pTxPacket->p80211Header->sA2.wFrameCtl = cpu_to_le16(
         (
         WLAN_SET_FC_FTYPE(WLAN_TYPE_CTL) |
         WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_PSPOLL) |
         WLAN_SET_FC_PWRMGT(0)
         ));
    pTxPacket->p80211Header->sA2.wDurationID = pMgmt->wCurrAID | BIT14 | BIT15;
    memcpy(pTxPacket->p80211Header->sA2.abyAddr1, pMgmt->abyCurrBSSID, WLAN_ADDR_LEN);
    memcpy(pTxPacket->p80211Header->sA2.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    pTxPacket->cbMPDULen = WLAN_HDR_ADDR2_LEN;
    pTxPacket->cbPayloadLen = 0;
    
    if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Send PS-Poll packet failed..\n");
    }
    else {
    };

    return;
}



bool
PSbSendNullPacket(
    void *hDeviceContext
    )
{
    PSDevice            pDevice = (PSDevice)hDeviceContext;
    PSTxMgmtPacket      pTxPacket = NULL;
    PSMgmtObject        pMgmt = pDevice->pMgmt;
    unsigned int uIdx;


    if (pDevice->bLinkPass == false) {
        return false;
    }
    #ifdef TxInSleep
     if ((pDevice->bEnablePSMode == false) &&
	  (pDevice->fTxDataInSleep == false)){
        return false;
    }
#else
    if (pDevice->bEnablePSMode == false) {
        return false;
    }
#endif
    if (pDevice->bEnablePSMode) {
        for (uIdx = 0; uIdx < TYPE_MAXTD; uIdx ++) {
            if (pDevice->iTDUsed[uIdx] != 0)
                return false;
        }
    }

    memset(pMgmt->pbyPSPacketPool, 0, sizeof(STxMgmtPacket) + WLAN_NULLDATA_FR_MAXLEN);
    pTxPacket = (PSTxMgmtPacket)pMgmt->pbyPSPacketPool;
    pTxPacket->p80211Header = (PUWLAN_80211HDR)((unsigned char *)pTxPacket + sizeof(STxMgmtPacket));

    if (pDevice->bEnablePSMode) {

        pTxPacket->p80211Header->sA3.wFrameCtl = cpu_to_le16(
             (
            WLAN_SET_FC_FTYPE(WLAN_TYPE_DATA) |
            WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_NULL) |
            WLAN_SET_FC_PWRMGT(1)
            ));
    }
    else {
        pTxPacket->p80211Header->sA3.wFrameCtl = cpu_to_le16(
             (
            WLAN_SET_FC_FTYPE(WLAN_TYPE_DATA) |
            WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_NULL) |
            WLAN_SET_FC_PWRMGT(0)
            ));
    }

    if(pMgmt->eCurrMode != WMAC_MODE_IBSS_STA) {
        pTxPacket->p80211Header->sA3.wFrameCtl |= cpu_to_le16((unsigned short)WLAN_SET_FC_TODS(1));
    }

    memcpy(pTxPacket->p80211Header->sA3.abyAddr1, pMgmt->abyCurrBSSID, WLAN_ADDR_LEN);
    memcpy(pTxPacket->p80211Header->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
    memcpy(pTxPacket->p80211Header->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
    pTxPacket->cbMPDULen = WLAN_HDR_ADDR3_LEN;
    pTxPacket->cbPayloadLen = 0;
    
    if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Send Null Packet failed !\n");
        return false;
    }
    else {

    }


    return true ;
}


bool
PSbIsNextTBTTWakeUp(
    void *hDeviceContext
    )
{

    PSDevice         pDevice = (PSDevice)hDeviceContext;
    PSMgmtObject        pMgmt = pDevice->pMgmt;
    bool bWakeUp = false;

    if (pMgmt->wListenInterval >= 2) {
        if (pMgmt->wCountToWakeUp == 0) {
            pMgmt->wCountToWakeUp = pMgmt->wListenInterval;
        }

        pMgmt->wCountToWakeUp --;

        if (pMgmt->wCountToWakeUp == 1) {
            
            MACvRegBitsOn(pDevice->PortOffset, MAC_REG_PSCTL, PSCTL_LNBCN);
            bWakeUp = true;
        }

    }

    return bWakeUp;
}

