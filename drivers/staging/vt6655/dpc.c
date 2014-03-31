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
 * File: dpc.c
 *
 * Purpose: handle dpc rx functions
 *
 * Author: Lyndon Chen
 *
 * Date: May 20, 2003
 *
 * Functions:
 *      device_receive_frame - Rcv 802.11 frame function
 *      s_bAPModeRxCtl- AP Rcv frame filer Ctl.
 *      s_bAPModeRxData- AP Rcv data frame handle
 *      s_bHandleRxEncryption- Rcv decrypted data via on-fly
 *      s_bHostWepRxEncryption- Rcv encrypted data via host
 *      s_byGetRateIdx- get rate index
 *      s_vGetDASA- get data offset
 *      s_vProcessRxMACHeader- Rcv 802.11 and translate to 802.3
 *
 * Revision History:
 *
 */

#include "device.h"
#include "rxtx.h"
#include "tether.h"
#include "card.h"
#include "bssdb.h"
#include "mac.h"
#include "baseband.h"
#include "michael.h"
#include "tkip.h"
#include "tcrc.h"
#include "wctl.h"
#include "wroute.h"
#include "hostap.h"
#include "rf.h"
#include "iowpa.h"
#include "aes_ccmp.h"





static int          msglevel                =MSG_LEVEL_INFO;

const unsigned char acbyRxRate[MAX_RATE] =
{2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108};





static unsigned char s_byGetRateIdx(unsigned char byRate);


static void
s_vGetDASA(unsigned char *pbyRxBufferAddr, unsigned int *pcbHeaderSize,
		PSEthernetHeader psEthHeader);

static void
s_vProcessRxMACHeader(PSDevice pDevice, unsigned char *pbyRxBufferAddr,
		unsigned int cbPacketSize, bool bIsWEP, bool bExtIV,
		unsigned int *pcbHeadSize);

static bool s_bAPModeRxCtl(
    PSDevice pDevice,
    unsigned char *pbyFrame,
    int      iSANodeIndex
    );



static bool s_bAPModeRxData (
    PSDevice pDevice,
    struct sk_buff* skb,
    unsigned int FrameSize,
    unsigned int cbHeaderOffset,
    int      iSANodeIndex,
    int      iDANodeIndex
    );


static bool s_bHandleRxEncryption(
    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    unsigned char *pbyNewRsr,
    PSKeyItem   *pKeyOut,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16
    );

static bool s_bHostWepRxEncryption(

    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    bool bOnFly,
    PSKeyItem    pKey,
    unsigned char *pbyNewRsr,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16

    );


static void
s_vProcessRxMACHeader(PSDevice pDevice, unsigned char *pbyRxBufferAddr,
		unsigned int cbPacketSize, bool bIsWEP, bool bExtIV,
		unsigned int *pcbHeadSize)
{
    unsigned char *pbyRxBuffer;
    unsigned int cbHeaderSize = 0;
    unsigned short *pwType;
    PS802_11Header  pMACHeader;
    int             ii;


    pMACHeader = (PS802_11Header) (pbyRxBufferAddr + cbHeaderSize);

    s_vGetDASA((unsigned char *)pMACHeader, &cbHeaderSize, &pDevice->sRxEthHeader);

    if (bIsWEP) {
        if (bExtIV) {
            
            cbHeaderSize += (WLAN_HDR_ADDR3_LEN + 8);
        } else {
            
            cbHeaderSize += (WLAN_HDR_ADDR3_LEN + 4);
        }
    }
    else {
        cbHeaderSize += WLAN_HDR_ADDR3_LEN;
    };

    pbyRxBuffer = (unsigned char *) (pbyRxBufferAddr + cbHeaderSize);
    if (!compare_ether_addr(pbyRxBuffer, &pDevice->abySNAP_Bridgetunnel[0])) {
        cbHeaderSize += 6;
    }
    else if (!compare_ether_addr(pbyRxBuffer, &pDevice->abySNAP_RFC1042[0])) {
        cbHeaderSize += 6;
        pwType = (unsigned short *) (pbyRxBufferAddr + cbHeaderSize);
        if ((*pwType!= TYPE_PKT_IPX) && (*pwType != cpu_to_le16(0xF380))) {
        }
        else {
            cbHeaderSize -= 8;
            pwType = (unsigned short *) (pbyRxBufferAddr + cbHeaderSize);
            if (bIsWEP) {
                if (bExtIV) {
                    *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 8);    
                } else {
                    *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 4);    
                }
            }
            else {
                *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN);
            }
        }
    }
    else {
        cbHeaderSize -= 2;
        pwType = (unsigned short *) (pbyRxBufferAddr + cbHeaderSize);
        if (bIsWEP) {
            if (bExtIV) {
                *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 8);    
            } else {
                *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN - 4);    
            }
        }
        else {
            *pwType = htons(cbPacketSize - WLAN_HDR_ADDR3_LEN);
        }
    }

    cbHeaderSize -= (ETH_ALEN * 2);
    pbyRxBuffer = (unsigned char *) (pbyRxBufferAddr + cbHeaderSize);
    for(ii=0;ii<ETH_ALEN;ii++)
        *pbyRxBuffer++ = pDevice->sRxEthHeader.abyDstAddr[ii];
    for(ii=0;ii<ETH_ALEN;ii++)
        *pbyRxBuffer++ = pDevice->sRxEthHeader.abySrcAddr[ii];

    *pcbHeadSize = cbHeaderSize;
}




static unsigned char s_byGetRateIdx (unsigned char byRate)
{
    unsigned char byRateIdx;

    for (byRateIdx = 0; byRateIdx <MAX_RATE ; byRateIdx++) {
        if (acbyRxRate[byRateIdx%MAX_RATE] == byRate)
            return byRateIdx;
    }
    return 0;
}


static void
s_vGetDASA(unsigned char *pbyRxBufferAddr, unsigned int *pcbHeaderSize,
	PSEthernetHeader psEthHeader)
{
    unsigned int cbHeaderSize = 0;
    PS802_11Header  pMACHeader;
    int             ii;

    pMACHeader = (PS802_11Header) (pbyRxBufferAddr + cbHeaderSize);

    if ((pMACHeader->wFrameCtl & FC_TODS) == 0) {
        if (pMACHeader->wFrameCtl & FC_FROMDS) {
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr1[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr3[ii];
            }
        }
        else {
            
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr1[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr2[ii];
            }
        }
    }
    else {
        
        if (pMACHeader->wFrameCtl & FC_FROMDS) {
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr3[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr4[ii];
                cbHeaderSize += 6;
            }
        }
        else {
            for(ii=0;ii<ETH_ALEN;ii++) {
                psEthHeader->abyDstAddr[ii] = pMACHeader->abyAddr3[ii];
                psEthHeader->abySrcAddr[ii] = pMACHeader->abyAddr2[ii];
            }
        }
    };
    *pcbHeaderSize = cbHeaderSize;
}





void	MngWorkItem(void *Context)
{
	PSRxMgmtPacket			pRxMgmtPacket;
	PSDevice	pDevice =  (PSDevice) Context;
	
	spin_lock_irq(&pDevice->lock);
	 while(pDevice->rxManeQueue.packet_num != 0)
	 {
		 pRxMgmtPacket =  DeQueue(pDevice);
        		vMgrRxManagePacket(pDevice, pDevice->pMgmt, pRxMgmtPacket);
	}
	spin_unlock_irq(&pDevice->lock);
}





bool
device_receive_frame (
    PSDevice pDevice,
    PSRxDesc pCurrRD
    )
{

    PDEVICE_RD_INFO  pRDInfo = pCurrRD->pRDInfo;
#ifdef	PLICE_DEBUG
	
#endif
    struct net_device_stats* pStats=&pDevice->stats;
    struct sk_buff* skb;
    PSMgmtObject    pMgmt = pDevice->pMgmt;
    PSRxMgmtPacket  pRxPacket = &(pDevice->pMgmt->sRxPacket);
    PS802_11Header  p802_11Header;
    unsigned char *pbyRsr;
    unsigned char *pbyNewRsr;
    unsigned char *pbyRSSI;
    PQWORD          pqwTSFTime;
    unsigned short *pwFrameSize;
    unsigned char *pbyFrame;
    bool bDeFragRx = false;
    bool bIsWEP = false;
    unsigned int cbHeaderOffset;
    unsigned int FrameSize;
    unsigned short wEtherType = 0;
    int             iSANodeIndex = -1;
    int             iDANodeIndex = -1;
    unsigned int ii;
    unsigned int cbIVOffset;
    bool bExtIV = false;
    unsigned char *pbyRxSts;
    unsigned char *pbyRxRate;
    unsigned char *pbySQ;
    unsigned int cbHeaderSize;
    PSKeyItem       pKey = NULL;
    unsigned short wRxTSC15_0 = 0;
    unsigned long dwRxTSC47_16 = 0;
    SKeyItem        STempKey;
    
    unsigned long dwDuration = 0;
    long            ldBm = 0;
    long            ldBmThreshold = 0;
    PS802_11Header pMACHeader;
 bool bRxeapol_key = false;


    skb = pRDInfo->skb;


#if 1
	pci_unmap_single(pDevice->pcid, pRDInfo->skb_dma,
                     pDevice->rx_buf_sz, PCI_DMA_FROMDEVICE);
#endif
    pwFrameSize = (unsigned short *)(skb->data + 2);
    FrameSize = cpu_to_le16(pCurrRD->m_rd1RD1.wReqCount) - cpu_to_le16(pCurrRD->m_rd0RD0.wResCount);

    
    
    if ((FrameSize > 2364)||(FrameSize <= 32)) {
        
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---------- WRONG Length 1 \n");
        return false;
    }

    pbyRxSts = (unsigned char *) (skb->data);
    pbyRxRate = (unsigned char *) (skb->data + 1);
    pbyRsr = (unsigned char *) (skb->data + FrameSize - 1);
    pbyRSSI = (unsigned char *) (skb->data + FrameSize - 2);
    pbyNewRsr = (unsigned char *) (skb->data + FrameSize - 3);
    pbySQ = (unsigned char *) (skb->data + FrameSize - 4);
    pqwTSFTime = (PQWORD) (skb->data + FrameSize - 12);
    pbyFrame = (unsigned char *)(skb->data + 4);

    
    FrameSize = cpu_to_le16(*pwFrameSize);

    if ((FrameSize > 2346)|(FrameSize < 14)) { 
                                               
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---------- WRONG Length 2 \n");
        return false;
    }
#if 1
	
    STAvUpdateRDStatCounter(&pDevice->scStatistic,
                            *pbyRsr,
                            *pbyNewRsr,
                            *pbyRxRate,
                            pbyFrame,
                            FrameSize);

#endif

  pMACHeader=(PS802_11Header)((unsigned char *) (skb->data)+8);
	if (pDevice->bMeasureInProgress == true) {
        if ((*pbyRsr & RSR_CRCOK) != 0) {
            pDevice->byBasicMap |= 0x01;
        }
        dwDuration = (FrameSize << 4);
        dwDuration /= acbyRxRate[*pbyRxRate%MAX_RATE];
        if (*pbyRxRate <= RATE_11M) {
            if (*pbyRxSts & 0x01) {
                
                dwDuration += 192;
            } else {
                
                dwDuration += 96;
            }
        } else {
            dwDuration += 16;
        }
        RFvRSSITodBm(pDevice, *pbyRSSI, &ldBm);
        ldBmThreshold = -57;
        for (ii = 7; ii > 0;) {
            if (ldBm > ldBmThreshold) {
                break;
            }
            ldBmThreshold -= 5;
            ii--;
        }
        pDevice->dwRPIs[ii] += dwDuration;
        return false;
    }

    if (!is_multicast_ether_addr(pbyFrame)) {
        if (WCTLbIsDuplicate(&(pDevice->sDupRxCache), (PS802_11Header) (skb->data + 4))) {
            pDevice->s802_11Counter.FrameDuplicateCount++;
            return false;
        }
    }


    
    s_vGetDASA(skb->data+4, &cbHeaderSize, &pDevice->sRxEthHeader);

    
    if (!compare_ether_addr((unsigned char *)&(pDevice->sRxEthHeader.abySrcAddr[0]), pDevice->abyCurrentNetAddr))
        return false;

    if ((pMgmt->eCurrMode == WMAC_MODE_ESS_AP) || (pMgmt->eCurrMode == WMAC_MODE_IBSS_STA)) {
        if (IS_CTL_PSPOLL(pbyFrame) || !IS_TYPE_CONTROL(pbyFrame)) {
            p802_11Header = (PS802_11Header) (pbyFrame);
            
            if (BSSDBbIsSTAInNodeDB(pMgmt, (unsigned char *)(p802_11Header->abyAddr2), &iSANodeIndex)) {
                pMgmt->sNodeDBTable[iSANodeIndex].ulLastRxJiffer = jiffies;
                pMgmt->sNodeDBTable[iSANodeIndex].uInActiveCount = 0;
            }
        }
    }

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        if (s_bAPModeRxCtl(pDevice, pbyFrame, iSANodeIndex) == true) {
            return false;
        }
    }


    if (IS_FC_WEP(pbyFrame)) {
        bool bRxDecryOK = false;

        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"rx WEP pkt\n");
        bIsWEP = true;
        if ((pDevice->bEnableHostWEP) && (iSANodeIndex >= 0)) {
            pKey = &STempKey;
            pKey->byCipherSuite = pMgmt->sNodeDBTable[iSANodeIndex].byCipherSuite;
            pKey->dwKeyIndex = pMgmt->sNodeDBTable[iSANodeIndex].dwKeyIndex;
            pKey->uKeyLength = pMgmt->sNodeDBTable[iSANodeIndex].uWepKeyLength;
            pKey->dwTSC47_16 = pMgmt->sNodeDBTable[iSANodeIndex].dwTSC47_16;
            pKey->wTSC15_0 = pMgmt->sNodeDBTable[iSANodeIndex].wTSC15_0;
            memcpy(pKey->abyKey,
                &pMgmt->sNodeDBTable[iSANodeIndex].abyWepKey[0],
                pKey->uKeyLength
                );

            bRxDecryOK = s_bHostWepRxEncryption(pDevice,
                                                pbyFrame,
                                                FrameSize,
                                                pbyRsr,
                                                pMgmt->sNodeDBTable[iSANodeIndex].bOnFly,
                                                pKey,
                                                pbyNewRsr,
                                                &bExtIV,
                                                &wRxTSC15_0,
                                                &dwRxTSC47_16);
        } else {
            bRxDecryOK = s_bHandleRxEncryption(pDevice,
                                                pbyFrame,
                                                FrameSize,
                                                pbyRsr,
                                                pbyNewRsr,
                                                &pKey,
                                                &bExtIV,
                                                &wRxTSC15_0,
                                                &dwRxTSC47_16);
        }

        if (bRxDecryOK) {
            if ((*pbyNewRsr & NEWRSR_DECRYPTOK) == 0) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV Fail\n");
                if ( (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPA) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPAPSK) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPA2) ||
                    (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPA2PSK)) {

                    if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_TKIP)) {
                        pDevice->s802_11Counter.TKIPICVErrors++;
                    } else if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_CCMP)) {
                        pDevice->s802_11Counter.CCMPDecryptErrors++;
                    } else if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_WEP)) {
                    }
                }
                return false;
            }
        } else {
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"WEP Func Fail\n");
            return false;
        }
        if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_CCMP))
            FrameSize -= 8;         
        else
            FrameSize -= 4;         
    }


    
    
    
    
    FrameSize -= ETH_FCS_LEN;

    if (( !(*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI))) && 
        (IS_FRAGMENT_PKT((skb->data+4)))
        ) {
        
        bDeFragRx = WCTLbHandleFragment(pDevice, (PS802_11Header) (skb->data+4), FrameSize, bIsWEP, bExtIV);
        pDevice->s802_11Counter.ReceivedFragmentCount++;
        if (bDeFragRx) {
            
            skb = pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx].skb;
            FrameSize = pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx].cbFrameLength;

        }
        else {
            return false;
        }
    }


    if ((IS_TYPE_DATA((skb->data+4))) == false) {
        

        if (IS_TYPE_MGMT((skb->data+4))) {
            unsigned char *pbyData1;
            unsigned char *pbyData2;

            pRxPacket->p80211Header = (PUWLAN_80211HDR)(skb->data+4);
            pRxPacket->cbMPDULen = FrameSize;
            pRxPacket->uRSSI = *pbyRSSI;
            pRxPacket->bySQ = *pbySQ;
            HIDWORD(pRxPacket->qwLocalTSF) = cpu_to_le32(HIDWORD(*pqwTSFTime));
            LODWORD(pRxPacket->qwLocalTSF) = cpu_to_le32(LODWORD(*pqwTSFTime));
            if (bIsWEP) {
                
                pbyData1 = WLAN_HDR_A3_DATA_PTR(skb->data+4);
                pbyData2 = WLAN_HDR_A3_DATA_PTR(skb->data+4) + 4;
                for (ii = 0; ii < (FrameSize - 4); ii++) {
                    *pbyData1 = *pbyData2;
                     pbyData1++;
                     pbyData2++;
                }
            }
            pRxPacket->byRxRate = s_byGetRateIdx(*pbyRxRate);
            pRxPacket->byRxChannel = (*pbyRxSts) >> 2;

#ifdef	THREAD
		EnQueue(pDevice,pRxPacket);

		
		
			
#else

#ifdef	TASK_LET
		EnQueue(pDevice,pRxPacket);
		tasklet_schedule(&pDevice->RxMngWorkItem);
#else
	vMgrRxManagePacket((void *)pDevice, pDevice->pMgmt, pRxPacket);
           
#endif

#endif
			
            
            if (pDevice->bEnableHostapd) {
	            skb->dev = pDevice->apdev;
	            skb->data += 4;
	            skb->tail += 4;
                     skb_put(skb, FrameSize);
		skb_reset_mac_header(skb);
	            skb->pkt_type = PACKET_OTHERHOST;
    	        skb->protocol = htons(ETH_P_802_2);
	            memset(skb->cb, 0, sizeof(skb->cb));
	            netif_rx(skb);
                return true;
	        }
        }
        else {
            
        };
        return false;
    }
    else {
        if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
            
            if ( !(*pbyRsr & RSR_BSSIDOK)) {
                if (bDeFragRx) {
                    if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                        DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                        pDevice->dev->name);
                    }
                }
                return false;
            }
        }
        else {
            
            if ((pDevice->bLinkPass == false) ||
                !(*pbyRsr & RSR_BSSIDOK)) {
                if (bDeFragRx) {
                    if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                        DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                        pDevice->dev->name);
                    }
                }
                return false;
            }
   
   	  {
   	    unsigned char Protocol_Version;    
	    unsigned char Packet_Type;           
              if (bIsWEP)
                  cbIVOffset = 8;
              else
                  cbIVOffset = 0;
              wEtherType = (skb->data[cbIVOffset + 8 + 24 + 6] << 8) |
                          skb->data[cbIVOffset + 8 + 24 + 6 + 1];
	      Protocol_Version = skb->data[cbIVOffset + 8 + 24 + 6 + 1 +1];
	      Packet_Type = skb->data[cbIVOffset + 8 + 24 + 6 + 1 +1+1];
	     if (wEtherType == ETH_P_PAE) {         
                  if(((Protocol_Version==1) ||(Protocol_Version==2)) &&
		     (Packet_Type==3)) {  
                        bRxeapol_key = true;
                  }
	      }
   	  }
    
        }
    }




    if (pDevice->bEnablePSMode) {
        if (IS_FC_MOREDATA((skb->data+4))) {
            if (*pbyRsr & RSR_ADDROK) {
                
            }
        }
        else {
            if (pDevice->pMgmt->bInTIMWake == true) {
                pDevice->pMgmt->bInTIMWake = false;
            }
        }
    }

    
    if (pDevice->bDiversityEnable && (FrameSize>50) &&
        (pDevice->eOPMode == OP_MODE_INFRASTRUCTURE) &&
        (pDevice->bLinkPass == true)) {
	
		BBvAntennaDiversity(pDevice, s_byGetRateIdx(*pbyRxRate), 0);
    }


    if (pDevice->byLocalID != REV_ID_VT3253_B1) {
        pDevice->uCurrRSSI = *pbyRSSI;
    }
    pDevice->byCurrSQ = *pbySQ;

    if ((*pbyRSSI != 0) &&
        (pMgmt->pCurrBSS!=NULL)) {
        RFvRSSITodBm(pDevice, *pbyRSSI, &ldBm);
        
        pMgmt->pCurrBSS->byRSSIStatCnt++;
        pMgmt->pCurrBSS->byRSSIStatCnt %= RSSI_STAT_COUNT;
        pMgmt->pCurrBSS->ldBmAverage[pMgmt->pCurrBSS->byRSSIStatCnt] = ldBm;
        for(ii=0;ii<RSSI_STAT_COUNT;ii++) {
            if (pMgmt->pCurrBSS->ldBmAverage[ii] != 0) {
            pMgmt->pCurrBSS->ldBmMAX = max(pMgmt->pCurrBSS->ldBmAverage[ii], ldBm);
            }
        }
    }

    

    if ((pMgmt->eCurrMode == WMAC_MODE_ESS_AP) && (pDevice->bEnable8021x == true)){
        unsigned char abyMacHdr[24];

        
        if (bIsWEP)
            cbIVOffset = 8;
        else
            cbIVOffset = 0;
        wEtherType = (skb->data[cbIVOffset + 4 + 24 + 6] << 8) |
                    skb->data[cbIVOffset + 4 + 24 + 6 + 1];

	    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"wEtherType = %04x \n", wEtherType);
        if (wEtherType == ETH_P_PAE) {
            skb->dev = pDevice->apdev;

            if (bIsWEP == true) {
                
                memcpy(&abyMacHdr[0], (skb->data + 4), 24);
                memcpy((skb->data + 4 + cbIVOffset), &abyMacHdr[0], 24);
            }
            skb->data +=  (cbIVOffset + 4);
            skb->tail +=  (cbIVOffset + 4);
            skb_put(skb, FrameSize);
	    skb_reset_mac_header(skb);

	skb->pkt_type = PACKET_OTHERHOST;
            skb->protocol = htons(ETH_P_802_2);
            memset(skb->cb, 0, sizeof(skb->cb));
            netif_rx(skb);
            return true;

}
        
        if (!(pMgmt->sNodeDBTable[iSANodeIndex].dwFlags & WLAN_STA_AUTHORIZED))
            return false;
    }


    if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_TKIP)) {
        if (bIsWEP) {
            FrameSize -= 8;  
        }
    }

    
    
    if ((pKey != NULL) && (pKey->byCipherSuite == KEY_CTL_TKIP)) {
        if (bIsWEP) {
            unsigned long *pdwMIC_L;
            unsigned long *pdwMIC_R;
            unsigned long dwMIC_Priority;
            unsigned long dwMICKey0 = 0, dwMICKey1 = 0;
            unsigned long dwLocalMIC_L = 0;
            unsigned long dwLocalMIC_R = 0;
            viawget_wpa_header *wpahdr;


            if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
                dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[24]));
                dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[28]));
            }
            else {
                if (pDevice->pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) {
                    dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[16]));
                    dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[20]));
                } else if ((pKey->dwKeyIndex & BIT28) == 0) {
                    dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[16]));
                    dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[20]));
                } else {
                    dwMICKey0 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[24]));
                    dwMICKey1 = cpu_to_le32(*(unsigned long *)(&pKey->abyKey[28]));
                }
            }

            MIC_vInit(dwMICKey0, dwMICKey1);
            MIC_vAppend((unsigned char *)&(pDevice->sRxEthHeader.abyDstAddr[0]), 12);
            dwMIC_Priority = 0;
            MIC_vAppend((unsigned char *)&dwMIC_Priority, 4);
            
            MIC_vAppend((unsigned char *)(skb->data + 4 + WLAN_HDR_ADDR3_LEN + 8),
                        FrameSize - WLAN_HDR_ADDR3_LEN - 8);
            MIC_vGetMIC(&dwLocalMIC_L, &dwLocalMIC_R);
            MIC_vUnInit();

            pdwMIC_L = (unsigned long *)(skb->data + 4 + FrameSize);
            pdwMIC_R = (unsigned long *)(skb->data + 4 + FrameSize + 4);
            
            
            


            if ((cpu_to_le32(*pdwMIC_L) != dwLocalMIC_L) || (cpu_to_le32(*pdwMIC_R) != dwLocalMIC_R) ||
                (pDevice->bRxMICFail == true)) {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MIC comparison is fail!\n");
                pDevice->bRxMICFail = false;
                
                pDevice->s802_11Counter.TKIPLocalMICFailures++;
                if (bDeFragRx) {
                    if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                        DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                            pDevice->dev->name);
                    }
                }
               
       #ifdef WPA_SUPPLICANT_DRIVER_WEXT_SUPPORT
				
				
				{
					union iwreq_data wrqu;
					struct iw_michaelmicfailure ev;
					int keyidx = pbyFrame[cbHeaderSize+3] >> 6; 
					memset(&ev, 0, sizeof(ev));
					ev.flags = keyidx & IW_MICFAILURE_KEY_ID;
					if ((pMgmt->eCurrMode == WMAC_MODE_ESS_STA) &&
							(pMgmt->eCurrState == WMAC_STATE_ASSOC) &&
								(*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI)) == 0) {
						ev.flags |= IW_MICFAILURE_PAIRWISE;
					} else {
						ev.flags |= IW_MICFAILURE_GROUP;
					}

					ev.src_addr.sa_family = ARPHRD_ETHER;
					memcpy(ev.src_addr.sa_data, pMACHeader->abyAddr2, ETH_ALEN);
					memset(&wrqu, 0, sizeof(wrqu));
					wrqu.data.length = sizeof(ev);
					wireless_send_event(pDevice->dev, IWEVMICHAELMICFAILURE, &wrqu, (char *)&ev);

				}
         #endif


                if ((pDevice->bWPADEVUp) && (pDevice->skb != NULL)) {
                     wpahdr = (viawget_wpa_header *)pDevice->skb->data;
                     if ((pDevice->pMgmt->eCurrMode == WMAC_MODE_ESS_STA) &&
                         (pDevice->pMgmt->eCurrState == WMAC_STATE_ASSOC) &&
                         (*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI)) == 0) {
                         
                         wpahdr->type = VIAWGET_PTK_MIC_MSG;
                     } else {
                         
                         wpahdr->type = VIAWGET_GTK_MIC_MSG;
                     }
                     wpahdr->resp_ie_len = 0;
                     wpahdr->req_ie_len = 0;
                     skb_put(pDevice->skb, sizeof(viawget_wpa_header));
                     pDevice->skb->dev = pDevice->wpadev;
		     skb_reset_mac_header(pDevice->skb);
                     pDevice->skb->pkt_type = PACKET_HOST;
                     pDevice->skb->protocol = htons(ETH_P_802_2);
                     memset(pDevice->skb->cb, 0, sizeof(pDevice->skb->cb));
                     netif_rx(pDevice->skb);
                     pDevice->skb = dev_alloc_skb((int)pDevice->rx_buf_sz);
                 }

                return false;

            }
        }
    } 

    

    if ((pKey != NULL) && ((pKey->byCipherSuite == KEY_CTL_TKIP) ||
                           (pKey->byCipherSuite == KEY_CTL_CCMP))) {
        if (bIsWEP) {
            unsigned short wLocalTSC15_0 = 0;
            unsigned long dwLocalTSC47_16 = 0;
            unsigned long long       RSC = 0;
            
            RSC = *((unsigned long long *) &(pKey->KeyRSC));
            wLocalTSC15_0 = (unsigned short) RSC;
            dwLocalTSC47_16 = (unsigned long) (RSC>>16);

            RSC = dwRxTSC47_16;
            RSC <<= 16;
            RSC += wRxTSC15_0;
            memcpy(&(pKey->KeyRSC), &RSC,  sizeof(QWORD));

            if ( (pDevice->sMgmtObj.eCurrMode == WMAC_MODE_ESS_STA) &&
                 (pDevice->sMgmtObj.eCurrState == WMAC_STATE_ASSOC)) {
                
                if ( (wRxTSC15_0 < wLocalTSC15_0) &&
                     (dwRxTSC47_16 <= dwLocalTSC47_16) &&
                     !((dwRxTSC47_16 == 0) && (dwLocalTSC47_16 == 0xFFFFFFFF))) {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TSC is illegal~~!\n ");
                    if (pKey->byCipherSuite == KEY_CTL_TKIP)
                        
                        pDevice->s802_11Counter.TKIPReplays++;
                    else
                        
                        pDevice->s802_11Counter.CCMPReplays++;

                    if (bDeFragRx) {
                        if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                            DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                                pDevice->dev->name);
                        }
                    }
                    return false;
                }
            }
        }
    } 



    if ((pKey != NULL) && (bIsWEP)) {
    }


    s_vProcessRxMACHeader(pDevice, (unsigned char *)(skb->data+4), FrameSize, bIsWEP, bExtIV, &cbHeaderOffset);
    FrameSize -= cbHeaderOffset;
    cbHeaderOffset += 4;        

    
    if (FrameSize < 15)
        return false;

    if (pMgmt->eCurrMode == WMAC_MODE_ESS_AP) {
        if (s_bAPModeRxData(pDevice,
                            skb,
                            FrameSize,
                            cbHeaderOffset,
                            iSANodeIndex,
                            iDANodeIndex
                            ) == false) {

            if (bDeFragRx) {
                if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
                    DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                    pDevice->dev->name);
                }
            }
            return false;
        }


    }

	skb->data += cbHeaderOffset;
	skb->tail += cbHeaderOffset;
    skb_put(skb, FrameSize);
    skb->protocol=eth_type_trans(skb, skb->dev);


	

    skb->ip_summed=CHECKSUM_NONE;
    pStats->rx_bytes +=skb->len;
    pStats->rx_packets++;
    netif_rx(skb);

    if (bDeFragRx) {
        if (!device_alloc_frag_buf(pDevice, &pDevice->sRxDFCB[pDevice->uCurrentDFCBIdx])) {
            DBG_PRT(MSG_LEVEL_ERR,KERN_ERR "%s: can not alloc more frag bufs\n",
                pDevice->dev->name);
        }
        return false;
    }

    return true;
}


static bool s_bAPModeRxCtl (
    PSDevice pDevice,
    unsigned char *pbyFrame,
    int      iSANodeIndex
    )
{
    PS802_11Header      p802_11Header;
    CMD_STATUS          Status;
    PSMgmtObject        pMgmt = pDevice->pMgmt;


    if (IS_CTL_PSPOLL(pbyFrame) || !IS_TYPE_CONTROL(pbyFrame)) {

        p802_11Header = (PS802_11Header) (pbyFrame);
        if (!IS_TYPE_MGMT(pbyFrame)) {

            
            
            if (iSANodeIndex > 0) {
                
                if (pMgmt->sNodeDBTable[iSANodeIndex].eNodeState < NODE_AUTH) {
                    
                    
                    vMgrDeAuthenBeginSta(pDevice,
                                         pMgmt,
                                         (unsigned char *)(p802_11Header->abyAddr2),
                                         (WLAN_MGMT_REASON_CLASS2_NONAUTH),
                                         &Status
                                         );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: send vMgrDeAuthenBeginSta 1\n");
                    return true;
                }
                if (pMgmt->sNodeDBTable[iSANodeIndex].eNodeState < NODE_ASSOC) {
                    
                    
                    vMgrDisassocBeginSta(pDevice,
                                         pMgmt,
                                         (unsigned char *)(p802_11Header->abyAddr2),
                                         (WLAN_MGMT_REASON_CLASS3_NONASSOC),
                                         &Status
                                         );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: send vMgrDisassocBeginSta 2\n");
                    return true;
                }

                if (pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable) {
                    
                    if (IS_CTL_PSPOLL(pbyFrame)) {
                        pMgmt->sNodeDBTable[iSANodeIndex].bRxPSPoll = true;
                        bScheduleCommand((void *)pDevice, WLAN_CMD_RX_PSPOLL, NULL);
                        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: WLAN_CMD_RX_PSPOLL 1\n");
                    }
                    else {
                        
                        
                        if (!IS_FC_POWERMGT(pbyFrame)) {
                            pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable = false;
                            pMgmt->sNodeDBTable[iSANodeIndex].bRxPSPoll = true;
                            bScheduleCommand((void *)pDevice, WLAN_CMD_RX_PSPOLL, NULL);
                            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: WLAN_CMD_RX_PSPOLL 2\n");
                        }
                    }
                }
                else {
                   if (IS_FC_POWERMGT(pbyFrame)) {
                       pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable = true;
                       
                       pMgmt->sNodeDBTable[0].bPSEnable = true;
                   }
                   else {
                      
                      if (pMgmt->sNodeDBTable[iSANodeIndex].wEnQueueCnt > 0) {
                          pMgmt->sNodeDBTable[iSANodeIndex].bPSEnable = false;
                          pMgmt->sNodeDBTable[iSANodeIndex].bRxPSPoll = true;
                          bScheduleCommand((void *)pDevice, WLAN_CMD_RX_PSPOLL, NULL);
                         DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: WLAN_CMD_RX_PSPOLL 3\n");

                      }
                   }
                }
            }
            else {
                  vMgrDeAuthenBeginSta(pDevice,
                                       pMgmt,
                                       (unsigned char *)(p802_11Header->abyAddr2),
                                       (WLAN_MGMT_REASON_CLASS2_NONAUTH),
                                       &Status
                                       );
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: send vMgrDeAuthenBeginSta 3\n");
			DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "BSSID:%pM\n",
				p802_11Header->abyAddr3);
			DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "ADDR2:%pM\n",
				p802_11Header->abyAddr2);
			DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "ADDR1:%pM\n",
				p802_11Header->abyAddr1);
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc: wFrameCtl= %x\n", p802_11Header->wFrameCtl );
                    VNSvInPortB(pDevice->PortOffset + MAC_REG_RCR, &(pDevice->byRxMode));
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "dpc:pDevice->byRxMode = %x\n", pDevice->byRxMode );
                    return true;
            }
        }
    }
    return false;

}

static bool s_bHandleRxEncryption (
    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    unsigned char *pbyNewRsr,
    PSKeyItem   *pKeyOut,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16
    )
{
    unsigned int PayloadLen = FrameSize;
    unsigned char *pbyIV;
    unsigned char byKeyIdx;
    PSKeyItem       pKey = NULL;
    unsigned char byDecMode = KEY_CTL_WEP;
    PSMgmtObject    pMgmt = pDevice->pMgmt;


    *pwRxTSC15_0 = 0;
    *pdwRxTSC47_16 = 0;

    pbyIV = pbyFrame + WLAN_HDR_ADDR3_LEN;
    if ( WLAN_GET_FC_TODS(*(unsigned short *)pbyFrame) &&
         WLAN_GET_FC_FROMDS(*(unsigned short *)pbyFrame) ) {
         pbyIV += 6;             
         PayloadLen -= 6;
    }
    byKeyIdx = (*(pbyIV+3) & 0xc0);
    byKeyIdx >>= 6;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\nKeyIdx: %d\n", byKeyIdx);

    if ((pMgmt->eAuthenMode == WMAC_AUTH_WPA) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPAPSK) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPANONE) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPA2) ||
        (pMgmt->eAuthenMode == WMAC_AUTH_WPA2PSK)) {
        if (((*pbyRsr & (RSR_ADDRBROAD | RSR_ADDRMULTI)) == 0) &&
            (pDevice->pMgmt->byCSSPK != KEY_CTL_NONE)) {
            
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"unicast pkt\n");
            if (KeybGetKey(&(pDevice->sKey), pDevice->abyBSSID, 0xFFFFFFFF, &pKey) == true) {
                if (pDevice->pMgmt->byCSSPK == KEY_CTL_TKIP)
                    byDecMode = KEY_CTL_TKIP;
                else if (pDevice->pMgmt->byCSSPK == KEY_CTL_CCMP)
                    byDecMode = KEY_CTL_CCMP;
            }
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"unicast pkt: %d, %p\n", byDecMode, pKey);
        } else {
            
            KeybGetKey(&(pDevice->sKey), pDevice->abyBSSID, byKeyIdx, &pKey);
            if (pDevice->pMgmt->byCSSGK == KEY_CTL_TKIP)
                byDecMode = KEY_CTL_TKIP;
            else if (pDevice->pMgmt->byCSSGK == KEY_CTL_CCMP)
                byDecMode = KEY_CTL_CCMP;
            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"group pkt: %d, %d, %p\n", byKeyIdx, byDecMode, pKey);
        }
    }
    
    if (pKey == NULL) {
        
        KeybGetKey(&(pDevice->sKey), pDevice->abyBroadcastAddr, byKeyIdx, &pKey);
        if (pDevice->pMgmt->byCSSGK == KEY_CTL_TKIP)
            byDecMode = KEY_CTL_TKIP;
        else if (pDevice->pMgmt->byCSSGK == KEY_CTL_CCMP)
            byDecMode = KEY_CTL_CCMP;
    }
    *pKeyOut = pKey;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"AES:%d %d %d\n", pDevice->pMgmt->byCSSPK, pDevice->pMgmt->byCSSGK, byDecMode);

    if (pKey == NULL) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"pKey == NULL\n");
        if (byDecMode == KEY_CTL_WEP) {
        } else if (pDevice->bLinkPass == true) {
        }
        return false;
    }
    if (byDecMode != pKey->byCipherSuite) {
        if (byDecMode == KEY_CTL_WEP) {
        } else if (pDevice->bLinkPass == true) {
        }
        *pKeyOut = NULL;
        return false;
    }
    if (byDecMode == KEY_CTL_WEP) {
        
        if ((pDevice->byLocalID <= REV_ID_VT3253_A1) ||
            (((PSKeyTable)(pKey->pvKeyTable))->bSoftWEP == true)) {
            
            
            

            PayloadLen -= (WLAN_HDR_ADDR3_LEN + 4 + 4); 
            memcpy(pDevice->abyPRNG, pbyIV, 3);
            memcpy(pDevice->abyPRNG + 3, pKey->abyKey, pKey->uKeyLength);
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, pKey->uKeyLength + 3);
            rc4_encrypt(&pDevice->SBox, pbyIV+4, pbyIV+4, PayloadLen);

            if (ETHbIsBufferCrc32Ok(pbyIV+4, PayloadLen)) {
                *pbyNewRsr |= NEWRSR_DECRYPTOK;
            }
        }
    } else if ((byDecMode == KEY_CTL_TKIP) ||
               (byDecMode == KEY_CTL_CCMP)) {
        

        PayloadLen -= (WLAN_HDR_ADDR3_LEN + 8 + 4); 
        *pdwRxTSC47_16 = cpu_to_le32(*(unsigned long *)(pbyIV + 4));
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ExtIV: %lx\n",*pdwRxTSC47_16);
        if (byDecMode == KEY_CTL_TKIP) {
            *pwRxTSC15_0 = cpu_to_le16(MAKEWORD(*(pbyIV+2), *pbyIV));
        } else {
            *pwRxTSC15_0 = cpu_to_le16(*(unsigned short *)pbyIV);
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TSC0_15: %x\n", *pwRxTSC15_0);

        if ((byDecMode == KEY_CTL_TKIP) &&
            (pDevice->byLocalID <= REV_ID_VT3253_A1)) {
            
            
            PS802_11Header  pMACHeader = (PS802_11Header) (pbyFrame);
            TKIPvMixKey(pKey->abyKey, pMACHeader->abyAddr2, *pwRxTSC15_0, *pdwRxTSC47_16, pDevice->abyPRNG);
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, TKIP_KEY_LEN);
            rc4_encrypt(&pDevice->SBox, pbyIV+8, pbyIV+8, PayloadLen);
            if (ETHbIsBufferCrc32Ok(pbyIV+8, PayloadLen)) {
                *pbyNewRsr |= NEWRSR_DECRYPTOK;
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV OK!\n");
            } else {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV FAIL!!!\n");
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"PayloadLen = %d\n", PayloadLen);
            }
        }
    }

    if ((*(pbyIV+3) & 0x20) != 0)
        *pbExtIV = true;
    return true;
}


static bool s_bHostWepRxEncryption (
    PSDevice     pDevice,
    unsigned char *pbyFrame,
    unsigned int FrameSize,
    unsigned char *pbyRsr,
    bool bOnFly,
    PSKeyItem    pKey,
    unsigned char *pbyNewRsr,
    bool *pbExtIV,
    unsigned short *pwRxTSC15_0,
    unsigned long *pdwRxTSC47_16
    )
{
    unsigned int PayloadLen = FrameSize;
    unsigned char *pbyIV;
    unsigned char byKeyIdx;
    unsigned char byDecMode = KEY_CTL_WEP;
    PS802_11Header  pMACHeader;



    *pwRxTSC15_0 = 0;
    *pdwRxTSC47_16 = 0;

    pbyIV = pbyFrame + WLAN_HDR_ADDR3_LEN;
    if ( WLAN_GET_FC_TODS(*(unsigned short *)pbyFrame) &&
         WLAN_GET_FC_FROMDS(*(unsigned short *)pbyFrame) ) {
         pbyIV += 6;             
         PayloadLen -= 6;
    }
    byKeyIdx = (*(pbyIV+3) & 0xc0);
    byKeyIdx >>= 6;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"\nKeyIdx: %d\n", byKeyIdx);


    if (pDevice->pMgmt->byCSSGK == KEY_CTL_TKIP)
        byDecMode = KEY_CTL_TKIP;
    else if (pDevice->pMgmt->byCSSGK == KEY_CTL_CCMP)
        byDecMode = KEY_CTL_CCMP;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"AES:%d %d %d\n", pDevice->pMgmt->byCSSPK, pDevice->pMgmt->byCSSGK, byDecMode);

    if (byDecMode != pKey->byCipherSuite) {
        if (byDecMode == KEY_CTL_WEP) {
        } else if (pDevice->bLinkPass == true) {
        }
        return false;
    }

    if (byDecMode == KEY_CTL_WEP) {
        
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"byDecMode == KEY_CTL_WEP \n");
        if ((pDevice->byLocalID <= REV_ID_VT3253_A1) ||
            (((PSKeyTable)(pKey->pvKeyTable))->bSoftWEP == true) ||
            (bOnFly == false)) {
            
            
            
            

            PayloadLen -= (WLAN_HDR_ADDR3_LEN + 4 + 4); 
            memcpy(pDevice->abyPRNG, pbyIV, 3);
            memcpy(pDevice->abyPRNG + 3, pKey->abyKey, pKey->uKeyLength);
            rc4_init(&pDevice->SBox, pDevice->abyPRNG, pKey->uKeyLength + 3);
            rc4_encrypt(&pDevice->SBox, pbyIV+4, pbyIV+4, PayloadLen);

            if (ETHbIsBufferCrc32Ok(pbyIV+4, PayloadLen)) {
                *pbyNewRsr |= NEWRSR_DECRYPTOK;
            }
        }
    } else if ((byDecMode == KEY_CTL_TKIP) ||
               (byDecMode == KEY_CTL_CCMP)) {
        

        PayloadLen -= (WLAN_HDR_ADDR3_LEN + 8 + 4); 
        *pdwRxTSC47_16 = cpu_to_le32(*(unsigned long *)(pbyIV + 4));
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ExtIV: %lx\n",*pdwRxTSC47_16);

        if (byDecMode == KEY_CTL_TKIP) {
            *pwRxTSC15_0 = cpu_to_le16(MAKEWORD(*(pbyIV+2), *pbyIV));
        } else {
            *pwRxTSC15_0 = cpu_to_le16(*(unsigned short *)pbyIV);
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"TSC0_15: %x\n", *pwRxTSC15_0);

        if (byDecMode == KEY_CTL_TKIP) {

            if ((pDevice->byLocalID <= REV_ID_VT3253_A1) || (bOnFly == false)) {
                
                
                
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"soft KEY_CTL_TKIP \n");
                pMACHeader = (PS802_11Header) (pbyFrame);
                TKIPvMixKey(pKey->abyKey, pMACHeader->abyAddr2, *pwRxTSC15_0, *pdwRxTSC47_16, pDevice->abyPRNG);
                rc4_init(&pDevice->SBox, pDevice->abyPRNG, TKIP_KEY_LEN);
                rc4_encrypt(&pDevice->SBox, pbyIV+8, pbyIV+8, PayloadLen);
                if (ETHbIsBufferCrc32Ok(pbyIV+8, PayloadLen)) {
                    *pbyNewRsr |= NEWRSR_DECRYPTOK;
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV OK!\n");
                } else {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"ICV FAIL!!!\n");
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"PayloadLen = %d\n", PayloadLen);
                }
            }
        }

        if (byDecMode == KEY_CTL_CCMP) {
            if (bOnFly == false) {
                
                
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"soft KEY_CTL_CCMP\n");
                if (AESbGenCCMP(pKey->abyKey, pbyFrame, FrameSize)) {
                    *pbyNewRsr |= NEWRSR_DECRYPTOK;
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CCMP MIC compare OK!\n");
                } else {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"CCMP MIC fail!\n");
                }
            }
        }

    }

    if ((*(pbyIV+3) & 0x20) != 0)
        *pbExtIV = true;
    return true;
}



static bool s_bAPModeRxData (
    PSDevice pDevice,
    struct sk_buff* skb,
    unsigned int FrameSize,
    unsigned int cbHeaderOffset,
    int      iSANodeIndex,
    int      iDANodeIndex
    )
{
    PSMgmtObject        pMgmt = pDevice->pMgmt;
    bool bRelayAndForward = false;
    bool bRelayOnly = false;
    unsigned char byMask[8] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80};
    unsigned short wAID;


    struct sk_buff* skbcpy = NULL;

    if (FrameSize > CB_MAX_BUF_SIZE)
        return false;
    
    if(is_multicast_ether_addr((unsigned char *)(skb->data+cbHeaderOffset))) {
       if (pMgmt->sNodeDBTable[0].bPSEnable) {

           skbcpy = dev_alloc_skb((int)pDevice->rx_buf_sz);

        
           if (skbcpy == NULL) {
               DBG_PRT(MSG_LEVEL_NOTICE, KERN_INFO "relay multicast no skb available \n");
           }
           else {
               skbcpy->dev = pDevice->dev;
               skbcpy->len = FrameSize;
               memcpy(skbcpy->data, skb->data+cbHeaderOffset, FrameSize);
               skb_queue_tail(&(pMgmt->sNodeDBTable[0].sTxPSQueue), skbcpy);

               pMgmt->sNodeDBTable[0].wEnQueueCnt++;
               
               pMgmt->abyPSTxMap[0] |= byMask[0];
           }
       }
       else {
           bRelayAndForward = true;
       }
    }
    else {
        
        if (BSSDBbIsSTAInNodeDB(pMgmt, (unsigned char *)(skb->data+cbHeaderOffset), &iDANodeIndex)) {
            if (pMgmt->sNodeDBTable[iDANodeIndex].eNodeState >= NODE_ASSOC) {
                if (pMgmt->sNodeDBTable[iDANodeIndex].bPSEnable) {
                    

	                skb->data += cbHeaderOffset;
	                skb->tail += cbHeaderOffset;
                    skb_put(skb, FrameSize);
                    skb_queue_tail(&pMgmt->sNodeDBTable[iDANodeIndex].sTxPSQueue, skb);
                    pMgmt->sNodeDBTable[iDANodeIndex].wEnQueueCnt++;
                    wAID = pMgmt->sNodeDBTable[iDANodeIndex].wAID;
                    pMgmt->abyPSTxMap[wAID >> 3] |=  byMask[wAID & 7];
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO "relay: index= %d, pMgmt->abyPSTxMap[%d]= %d\n",
                               iDANodeIndex, (wAID >> 3), pMgmt->abyPSTxMap[wAID >> 3]);
                    return true;
                }
                else {
                    bRelayOnly = true;
                }
            }
        }
    }

    if (bRelayOnly || bRelayAndForward) {
        
        if (bRelayAndForward)
            iDANodeIndex = 0;

        if ((pDevice->uAssocCount > 1) && (iDANodeIndex >= 0)) {
            ROUTEbRelay(pDevice, (unsigned char *)(skb->data + cbHeaderOffset), FrameSize, (unsigned int)iDANodeIndex);
        }

        if (bRelayOnly)
            return false;
    }
    
    if (pDevice->uAssocCount == 0)
        return false;

    return true;
}

