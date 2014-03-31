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
#include "control.h"
#include "rndis.h"



static int msglevel = MSG_LEVEL_INFO;




void PSvEnablePowerSaving(void *hDeviceContext,
			  WORD wListenInterval)
{
	PSDevice pDevice = (PSDevice)hDeviceContext;
	PSMgmtObject pMgmt = &(pDevice->sMgmtObj);
	WORD wAID = pMgmt->wCurrAID | BIT14 | BIT15;

	
	MACvWriteWord(pDevice, MAC_REG_PWBT, C_PWBT);

	if (pDevice->eOPMode != OP_MODE_ADHOC) {
		
		MACvWriteWord(pDevice, MAC_REG_AIDATIM, wAID);
	} else {
		
		
	}

	
	
	MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_PSEN);

	
	MACvRegBitsOn(pDevice, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);

	
	MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_GO2DOZE);

	if (wListenInterval >= 2) {

		
		MACvRegBitsOff(pDevice, MAC_REG_PSCTL, PSCTL_ALBCN);

		
		MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_LNBCN);

		pMgmt->wCountToWakeUp = wListenInterval;

	} else {

		
		MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_ALBCN);

		pMgmt->wCountToWakeUp = 0;
	}

	pDevice->bEnablePSMode = TRUE;

	
	if (pDevice->eOPMode == OP_MODE_INFRASTRUCTURE)
		PSbSendNullPacket(pDevice);

	pDevice->bPWBitOn = TRUE;
	DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "PS:Power Saving Mode Enable...\n");
}


void PSvDisablePowerSaving(void *hDeviceContext)
{
	PSDevice pDevice = (PSDevice)hDeviceContext;
	

	
	CONTROLnsRequestOut(pDevice, MESSAGE_TYPE_DISABLE_PS, 0,
						0, 0, NULL);

	
	MACvRegBitsOff(pDevice, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);

	
	MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_ALBCN);
	pDevice->bEnablePSMode = FALSE;

	if (pDevice->eOPMode == OP_MODE_INFRASTRUCTURE)
		PSbSendNullPacket(pDevice);

	pDevice->bPWBitOn = FALSE;
}


BOOL PSbConsiderPowerDown(void *hDeviceContext,
			  BOOL bCheckRxDMA,
			  BOOL bCheckCountToWakeUp)
{
	PSDevice pDevice = (PSDevice)hDeviceContext;
	PSMgmtObject pMgmt = &(pDevice->sMgmtObj);
	BYTE byData;

	
	ControlvReadByte(pDevice, MESSAGE_REQUEST_MACREG,
					MAC_REG_PSCTL, &byData);

	if ((byData & PSCTL_PS) != 0)
		return TRUE;

	if (pMgmt->eCurrMode != WMAC_MODE_IBSS_STA) {
		
		if (pMgmt->bInTIMWake)
			return FALSE;
	}

	
	if (pDevice->bCmdRunning)
		return FALSE;

	
	if (pDevice->bPSModeTxBurst)
		return FALSE;

	
	MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_PSEN);

	if (pMgmt->eCurrMode != WMAC_MODE_IBSS_STA) {
		if (bCheckCountToWakeUp && (pMgmt->wCountToWakeUp == 0
			|| pMgmt->wCountToWakeUp == 1)) {
				return FALSE;
		}
	}

	pDevice->bPSRxBeacon = TRUE;

	
	MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_GO2DOZE);
	DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Go to Doze ZZZZZZZZZZZZZZZ\n");
	return TRUE;
}


void PSvSendPSPOLL(void *hDeviceContext)
{
	PSDevice pDevice = (PSDevice)hDeviceContext;
	PSMgmtObject pMgmt = &(pDevice->sMgmtObj);
	PSTxMgmtPacket pTxPacket = NULL;

	memset(pMgmt->pbyPSPacketPool, 0, sizeof(STxMgmtPacket) + WLAN_HDR_ADDR2_LEN);
	pTxPacket = (PSTxMgmtPacket)pMgmt->pbyPSPacketPool;
	pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));
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
}


BOOL PSbSendNullPacket(void *hDeviceContext)
{
	PSDevice pDevice = (PSDevice)hDeviceContext;
	PSTxMgmtPacket pTxPacket = NULL;
	PSMgmtObject pMgmt = &(pDevice->sMgmtObj);
	u16 flags = 0;

	if (pDevice->bLinkPass == FALSE)
		return FALSE;

	if ((pDevice->bEnablePSMode == FALSE) &&
		(pDevice->fTxDataInSleep == FALSE)) {
			return FALSE;
	}

	memset(pMgmt->pbyPSPacketPool, 0, sizeof(STxMgmtPacket) + WLAN_NULLDATA_FR_MAXLEN);
	pTxPacket = (PSTxMgmtPacket)pMgmt->pbyPSPacketPool;
	pTxPacket->p80211Header = (PUWLAN_80211HDR)((PBYTE)pTxPacket + sizeof(STxMgmtPacket));

	flags = WLAN_SET_FC_FTYPE(WLAN_TYPE_DATA) |
                        WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_NULL);

	if (pDevice->bEnablePSMode)
		flags |= WLAN_SET_FC_PWRMGT(1);
	else
		flags |= WLAN_SET_FC_PWRMGT(0);

	pTxPacket->p80211Header->sA3.wFrameCtl = cpu_to_le16(flags);

	if (pMgmt->eCurrMode != WMAC_MODE_IBSS_STA)
		pTxPacket->p80211Header->sA3.wFrameCtl |= cpu_to_le16((WORD)WLAN_SET_FC_TODS(1));

	memcpy(pTxPacket->p80211Header->sA3.abyAddr1, pMgmt->abyCurrBSSID, WLAN_ADDR_LEN);
	memcpy(pTxPacket->p80211Header->sA3.abyAddr2, pMgmt->abyMACAddr, WLAN_ADDR_LEN);
	memcpy(pTxPacket->p80211Header->sA3.abyAddr3, pMgmt->abyCurrBSSID, WLAN_BSSID_LEN);
	pTxPacket->cbMPDULen = WLAN_HDR_ADDR3_LEN;
	pTxPacket->cbPayloadLen = 0;
	
	if (csMgmt_xmit(pDevice, pTxPacket) != CMD_STATUS_PENDING) {
		DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "Send Null Packet failed !\n");
		return FALSE;
	}
	return TRUE;
}


BOOL PSbIsNextTBTTWakeUp(void *hDeviceContext)
{
	PSDevice pDevice = (PSDevice)hDeviceContext;
	PSMgmtObject pMgmt = &(pDevice->sMgmtObj);
	BOOL bWakeUp = FALSE;

	if (pMgmt->wListenInterval >= 2) {
		if (pMgmt->wCountToWakeUp == 0)
			pMgmt->wCountToWakeUp = pMgmt->wListenInterval;

		pMgmt->wCountToWakeUp--;

		if (pMgmt->wCountToWakeUp == 1) {
			
			MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_LNBCN);
			pDevice->bPSRxBeacon = FALSE;
			bWakeUp = TRUE;
		} else if (!pDevice->bPSRxBeacon) {
			
			MACvRegBitsOn(pDevice, MAC_REG_PSCTL, PSCTL_LNBCN);
		}
	}
	return bWakeUp;
}

