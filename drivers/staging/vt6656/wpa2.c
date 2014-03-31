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
 * File: wpa2.c
 *
 * Purpose: Handles the Basic Service Set & Node Database functions
 *
 * Functions:
 *
 * Revision History:
 *
 * Author: Yiching Chen
 *
 * Date: Oct. 4, 2004
 *
 */

#include "wpa2.h"
#include "device.h"

static int          msglevel                =MSG_LEVEL_INFO;


const BYTE abyOUIGK[4]      = { 0x00, 0x0F, 0xAC, 0x00 };
const BYTE abyOUIWEP40[4]   = { 0x00, 0x0F, 0xAC, 0x01 };
const BYTE abyOUIWEP104[4]  = { 0x00, 0x0F, 0xAC, 0x05 };
const BYTE abyOUITKIP[4]    = { 0x00, 0x0F, 0xAC, 0x02 };
const BYTE abyOUICCMP[4]    = { 0x00, 0x0F, 0xAC, 0x04 };

const BYTE abyOUI8021X[4]   = { 0x00, 0x0F, 0xAC, 0x01 };
const BYTE abyOUIPSK[4]     = { 0x00, 0x0F, 0xAC, 0x02 };





void
WPA2_ClearRSN (
     PKnownBSS        pBSSNode
    )
{
    int ii;

    pBSSNode->bWPA2Valid = FALSE;

    pBSSNode->byCSSGK = WLAN_11i_CSS_CCMP;
    for (ii=0; ii < 4; ii ++)
        pBSSNode->abyCSSPK[ii] = WLAN_11i_CSS_CCMP;
    pBSSNode->wCSSPKCount = 1;
    for (ii=0; ii < 4; ii ++)
        pBSSNode->abyAKMSSAuthType[ii] = WLAN_11i_AKMSS_802_1X;
    pBSSNode->wAKMSSAuthCount = 1;
    pBSSNode->sRSNCapObj.bRSNCapExist = FALSE;
    pBSSNode->sRSNCapObj.wRSNCap = 0;
}

void
WPA2vParseRSN (
     PKnownBSS        pBSSNode,
     PWLAN_IE_RSN     pRSN
    )
{
    int                 i, j;
    WORD                m = 0, n = 0;
    PBYTE               pbyOUI;
    BOOL                bUseGK = FALSE;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"WPA2_ParseRSN: [%d]\n", pRSN->len);

    WPA2_ClearRSN(pBSSNode);

    if (pRSN->len == 2) { 
        if ((pRSN->byElementID == WLAN_EID_RSN) && (pRSN->wVersion == 1)) {
            pBSSNode->bWPA2Valid = TRUE;
        }
        return;
    }

    if (pRSN->len < 6) { 
        
        return;
    }

    
    if ((pRSN->byElementID == WLAN_EID_RSN) &&
        (pRSN->wVersion == 1)) {

        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Legal 802.11i RSN\n");

        pbyOUI = &(pRSN->abyRSN[0]);
        if ( !memcmp(pbyOUI, abyOUIWEP40, 4))
            pBSSNode->byCSSGK = WLAN_11i_CSS_WEP40;
        else if ( !memcmp(pbyOUI, abyOUITKIP, 4))
            pBSSNode->byCSSGK = WLAN_11i_CSS_TKIP;
        else if ( !memcmp(pbyOUI, abyOUICCMP, 4))
            pBSSNode->byCSSGK = WLAN_11i_CSS_CCMP;
        else if ( !memcmp(pbyOUI, abyOUIWEP104, 4))
            pBSSNode->byCSSGK = WLAN_11i_CSS_WEP104;
        else if ( !memcmp(pbyOUI, abyOUIGK, 4)) {
            
            return;
        } else
            
            pBSSNode->byCSSGK = WLAN_11i_CSS_UNKNOWN;

        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"802.11i CSS: %X\n", pBSSNode->byCSSGK);

        if (pRSN->len == 6) {
            pBSSNode->bWPA2Valid = TRUE;
            return;
        }

        if (pRSN->len >= 8) { 
            pBSSNode->wCSSPKCount = *((PWORD) &(pRSN->abyRSN[4]));
            j = 0;
            pbyOUI = &(pRSN->abyRSN[6]);

            for (i = 0; (i < pBSSNode->wCSSPKCount) && (j < sizeof(pBSSNode->abyCSSPK)/sizeof(BYTE)); i++) {

                if (pRSN->len >= 8+i*4+4) { 
                    if ( !memcmp(pbyOUI, abyOUIGK, 4)) {
                        pBSSNode->abyCSSPK[j++] = WLAN_11i_CSS_USE_GROUP;
                        bUseGK = TRUE;
                    } else if ( !memcmp(pbyOUI, abyOUIWEP40, 4)) {
                        
                    } else if ( !memcmp(pbyOUI, abyOUITKIP, 4)) {
                        if (pBSSNode->byCSSGK != WLAN_11i_CSS_CCMP)
                            pBSSNode->abyCSSPK[j++] = WLAN_11i_CSS_TKIP;
                        else
                            ; 
                    } else if ( !memcmp(pbyOUI, abyOUICCMP, 4)) {
                        pBSSNode->abyCSSPK[j++] = WLAN_11i_CSS_CCMP;
                    } else if ( !memcmp(pbyOUI, abyOUIWEP104, 4)) {
                        
                    } else {
                        
                        pBSSNode->abyCSSPK[j++] = WLAN_11i_CSS_UNKNOWN;
                    }
                    pbyOUI += 4;
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"abyCSSPK[%d]: %X\n", j-1, pBSSNode->abyCSSPK[j-1]);
                } else
                    break;
            } 

            if (bUseGK == TRUE) {
                if (j != 1) {
                    
                    return;
                }
                if (pBSSNode->byCSSGK == WLAN_11i_CSS_CCMP) {
                    
                    return;
                }
            }
            if ((pBSSNode->wCSSPKCount != 0) && (j == 0)) {
                
                return;
            }
            pBSSNode->wCSSPKCount = (WORD)j;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"wCSSPKCount: %d\n", pBSSNode->wCSSPKCount);
        }

        m = *((PWORD) &(pRSN->abyRSN[4]));

        if (pRSN->len >= 10+m*4) { 
            pBSSNode->wAKMSSAuthCount = *((PWORD) &(pRSN->abyRSN[6+4*m]));
            j = 0;
            pbyOUI = &(pRSN->abyRSN[8+4*m]);
            for (i = 0; (i < pBSSNode->wAKMSSAuthCount) && (j < sizeof(pBSSNode->abyAKMSSAuthType)/sizeof(BYTE)); i++) {
                if (pRSN->len >= 10+(m+i)*4+4) { 
                    if ( !memcmp(pbyOUI, abyOUI8021X, 4))
                        pBSSNode->abyAKMSSAuthType[j++] = WLAN_11i_AKMSS_802_1X;
                    else if ( !memcmp(pbyOUI, abyOUIPSK, 4))
                        pBSSNode->abyAKMSSAuthType[j++] = WLAN_11i_AKMSS_PSK;
                    else
                        
                        pBSSNode->abyAKMSSAuthType[j++] = WLAN_11i_AKMSS_UNKNOWN;
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"abyAKMSSAuthType[%d]: %X\n", j-1, pBSSNode->abyAKMSSAuthType[j-1]);
                } else
                    break;
            }
            pBSSNode->wAKMSSAuthCount = (WORD)j;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"wAKMSSAuthCount: %d\n", pBSSNode->wAKMSSAuthCount);

            n = *((PWORD) &(pRSN->abyRSN[6+4*m]));
            if (pRSN->len >= 12+4*m+4*n) { 
                pBSSNode->sRSNCapObj.bRSNCapExist = TRUE;
                pBSSNode->sRSNCapObj.wRSNCap = *((PWORD) &(pRSN->abyRSN[8+4*m+4*n]));
            }
        }
        
        pBSSNode->bWPA2Valid = TRUE;
    }
}


unsigned int
WPA2uSetIEs(void *pMgmtHandle,
     PWLAN_IE_RSN pRSNIEs
    )
{
    PSMgmtObject    pMgmt = (PSMgmtObject) pMgmtHandle;
    PBYTE           pbyBuffer = NULL;
    unsigned int            ii = 0;
    PWORD           pwPMKID = NULL;

    if (pRSNIEs == NULL) {
        return(0);
    }
    if (((pMgmt->eAuthenMode == WMAC_AUTH_WPA2) ||
         (pMgmt->eAuthenMode == WMAC_AUTH_WPA2PSK)) &&
        (pMgmt->pCurrBSS != NULL)) {
        
        pbyBuffer = (PBYTE) pRSNIEs;
        pRSNIEs->byElementID = WLAN_EID_RSN;
        pRSNIEs->len = 6; 
        pRSNIEs->wVersion = 1;
        
        pRSNIEs->abyRSN[0] = 0x00;
        pRSNIEs->abyRSN[1] = 0x0F;
        pRSNIEs->abyRSN[2] = 0xAC;
        if (pMgmt->byCSSGK == KEY_CTL_WEP) {
            pRSNIEs->abyRSN[3] = pMgmt->pCurrBSS->byCSSGK;
        } else if (pMgmt->byCSSGK == KEY_CTL_TKIP) {
            pRSNIEs->abyRSN[3] = WLAN_11i_CSS_TKIP;
        } else if (pMgmt->byCSSGK == KEY_CTL_CCMP) {
            pRSNIEs->abyRSN[3] = WLAN_11i_CSS_CCMP;
        } else {
            pRSNIEs->abyRSN[3] = WLAN_11i_CSS_UNKNOWN;
        }

        
        pRSNIEs->abyRSN[4] = 1;
        pRSNIEs->abyRSN[5] = 0;
        pRSNIEs->abyRSN[6] = 0x00;
        pRSNIEs->abyRSN[7] = 0x0F;
        pRSNIEs->abyRSN[8] = 0xAC;
        if (pMgmt->byCSSPK == KEY_CTL_TKIP) {
            pRSNIEs->abyRSN[9] = WLAN_11i_CSS_TKIP;
        } else if (pMgmt->byCSSPK == KEY_CTL_CCMP) {
            pRSNIEs->abyRSN[9] = WLAN_11i_CSS_CCMP;
        } else if (pMgmt->byCSSPK == KEY_CTL_NONE) {
            pRSNIEs->abyRSN[9] = WLAN_11i_CSS_USE_GROUP;
        } else {
            pRSNIEs->abyRSN[9] = WLAN_11i_CSS_UNKNOWN;
        }
        pRSNIEs->len += 6;

        
        pRSNIEs->abyRSN[10] = 1;
        pRSNIEs->abyRSN[11] = 0;
        pRSNIEs->abyRSN[12] = 0x00;
        pRSNIEs->abyRSN[13] = 0x0F;
        pRSNIEs->abyRSN[14] = 0xAC;
        if (pMgmt->eAuthenMode == WMAC_AUTH_WPA2PSK) {
            pRSNIEs->abyRSN[15] = WLAN_11i_AKMSS_PSK;
        } else if (pMgmt->eAuthenMode == WMAC_AUTH_WPA2) {
            pRSNIEs->abyRSN[15] = WLAN_11i_AKMSS_802_1X;
        } else {
            pRSNIEs->abyRSN[15] = WLAN_11i_AKMSS_UNKNOWN;
        }
        pRSNIEs->len +=6;

        
        if (pMgmt->pCurrBSS->sRSNCapObj.bRSNCapExist == TRUE) {
            memcpy(&pRSNIEs->abyRSN[16], &pMgmt->pCurrBSS->sRSNCapObj.wRSNCap, 2);
        } else {
            pRSNIEs->abyRSN[16] = 0;
            pRSNIEs->abyRSN[17] = 0;
        }
        pRSNIEs->len +=2;

	if ((pMgmt->gsPMKIDCache.BSSIDInfoCount > 0) &&
	    (pMgmt->bRoaming == TRUE) &&
            (pMgmt->eAuthenMode == WMAC_AUTH_WPA2)) {
		
		pwPMKID = (PWORD)(&pRSNIEs->abyRSN[18]);
		*pwPMKID = 0;                     
		pbyBuffer = &pRSNIEs->abyRSN[20];    
		for (ii = 0; ii < pMgmt->gsPMKIDCache.BSSIDInfoCount; ii++) {
			if (!memcmp(&pMgmt->
				    gsPMKIDCache.BSSIDInfo[ii].abyBSSID[0],
				    pMgmt->abyCurrBSSID,
				    ETH_ALEN)) {
				(*pwPMKID)++;
				memcpy(pbyBuffer,
			pMgmt->gsPMKIDCache.BSSIDInfo[ii].abyPMKID,
				       16);
				pbyBuffer += 16;
			}
		}
            if (*pwPMKID != 0) {
                pRSNIEs->len += (2 + (*pwPMKID)*16);
            } else {
                pbyBuffer = &pRSNIEs->abyRSN[18];
            }
        }
        return(pRSNIEs->len + WLAN_IEHDR_LEN);
    }
    return(0);
}
