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
 * File: mib.c
 *
 * Purpose: Implement MIB Data Structure
 *
 * Author: Tevin Chen
 *
 * Date: May 21, 1996
 *
 * Functions:
 *      STAvClearAllCounter - Clear All MIB Counter
 *      STAvUpdateIstStatCounter - Update ISR statistic counter
 *      STAvUpdateRDStatCounter - Update Rx statistic counter
 *      STAvUpdateRDStatCounterEx - Update Rx statistic counter and copy rcv data
 *      STAvUpdateTDStatCounter - Update Tx statistic counter
 *      STAvUpdateTDStatCounterEx - Update Tx statistic counter and copy tx data
 *      STAvUpdate802_11Counter - Update 802.11 mib counter
 *
 * Revision History:
 *
 */

#include "upc.h"
#include "mac.h"
#include "tether.h"
#include "mib.h"
#include "wctl.h"
#include "baseband.h"

static int          msglevel                =MSG_LEVEL_INFO;







void STAvClearAllCounter (PSStatCounter pStatistic)
{
    
	memset(pStatistic, 0, sizeof(SStatCounter));
}


void STAvUpdateIsrStatCounter (PSStatCounter pStatistic, unsigned long dwIsr)
{
    
    
    
    

    if (dwIsr == 0) {
        pStatistic->ISRStat.dwIsrUnknown++;
        return;
    }

    if (dwIsr & ISR_TXDMA0)               
        pStatistic->ISRStat.dwIsrTx0OK++;             

    if (dwIsr & ISR_AC0DMA)               
        pStatistic->ISRStat.dwIsrAC0TxOK++;           

    if (dwIsr & ISR_BNTX)                 
        pStatistic->ISRStat.dwIsrBeaconTxOK++;        

    if (dwIsr & ISR_RXDMA0)               
        pStatistic->ISRStat.dwIsrRx0OK++;             

    if (dwIsr & ISR_TBTT)                 
        pStatistic->ISRStat.dwIsrTBTTInt++;           

    if (dwIsr & ISR_SOFTTIMER)            
        pStatistic->ISRStat.dwIsrSTIMERInt++;

    if (dwIsr & ISR_WATCHDOG)             
        pStatistic->ISRStat.dwIsrWatchDog++;

    if (dwIsr & ISR_FETALERR)             
        pStatistic->ISRStat.dwIsrUnrecoverableError++;

    if (dwIsr & ISR_SOFTINT)              
        pStatistic->ISRStat.dwIsrSoftInterrupt++;     

    if (dwIsr & ISR_MIBNEARFULL)          
        pStatistic->ISRStat.dwIsrMIBNearfull++;

    if (dwIsr & ISR_RXNOBUF)              
        pStatistic->ISRStat.dwIsrRxNoBuf++;           

    if (dwIsr & ISR_RXDMA1)               
        pStatistic->ISRStat.dwIsrRx1OK++;             






    if (dwIsr & ISR_SOFTTIMER1)           
        pStatistic->ISRStat.dwIsrSTIMER1Int++;

}


void STAvUpdateRDStatCounter (PSStatCounter pStatistic,
                              unsigned char byRSR, unsigned char byNewRSR, unsigned char byRxRate,
                              unsigned char *pbyBuffer, unsigned int cbFrameLength)
{
    
    PS802_11Header pHeader = (PS802_11Header)pbyBuffer;

    if (byRSR & RSR_ADDROK)
        pStatistic->dwRsrADDROk++;
    if (byRSR & RSR_CRCOK) {
        pStatistic->dwRsrCRCOk++;

        pStatistic->ullRsrOK++;

        if (cbFrameLength >= ETH_ALEN) {
            
            if (byRSR & RSR_ADDRBROAD) {
                pStatistic->ullRxBroadcastFrames++;
                pStatistic->ullRxBroadcastBytes += (unsigned long long) cbFrameLength;
            }
            else if (byRSR & RSR_ADDRMULTI) {
                pStatistic->ullRxMulticastFrames++;
                pStatistic->ullRxMulticastBytes += (unsigned long long) cbFrameLength;
            }
            else {
                pStatistic->ullRxDirectedFrames++;
                pStatistic->ullRxDirectedBytes += (unsigned long long) cbFrameLength;
            }
        }
    }

    if(byRxRate==22) {
        pStatistic->CustomStat.ullRsr11M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr11MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"11M: ALL[%d], OK[%d]:[%02x]\n", (int)pStatistic->CustomStat.ullRsr11M, (int)pStatistic->CustomStat.ullRsr11MCRCOk, byRSR);
    }
    else if(byRxRate==11) {
        pStatistic->CustomStat.ullRsr5M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr5MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" 5M: ALL[%d], OK[%d]:[%02x]\n", (int)pStatistic->CustomStat.ullRsr5M, (int)pStatistic->CustomStat.ullRsr5MCRCOk, byRSR);
    }
    else if(byRxRate==4) {
        pStatistic->CustomStat.ullRsr2M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr2MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" 2M: ALL[%d], OK[%d]:[%02x]\n", (int)pStatistic->CustomStat.ullRsr2M, (int)pStatistic->CustomStat.ullRsr2MCRCOk, byRSR);
    }
    else if(byRxRate==2){
        pStatistic->CustomStat.ullRsr1M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr1MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" 1M: ALL[%d], OK[%d]:[%02x]\n", (int)pStatistic->CustomStat.ullRsr1M, (int)pStatistic->CustomStat.ullRsr1MCRCOk, byRSR);
    }
    else if(byRxRate==12){
        pStatistic->CustomStat.ullRsr6M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr6MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" 6M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr6M, (int)pStatistic->CustomStat.ullRsr6MCRCOk);
    }
    else if(byRxRate==18){
        pStatistic->CustomStat.ullRsr9M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr9MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" 9M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr9M, (int)pStatistic->CustomStat.ullRsr9MCRCOk);
    }
    else if(byRxRate==24){
        pStatistic->CustomStat.ullRsr12M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr12MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"12M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr12M, (int)pStatistic->CustomStat.ullRsr12MCRCOk);
    }
    else if(byRxRate==36){
        pStatistic->CustomStat.ullRsr18M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr18MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"18M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr18M, (int)pStatistic->CustomStat.ullRsr18MCRCOk);
    }
    else if(byRxRate==48){
        pStatistic->CustomStat.ullRsr24M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr24MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"24M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr24M, (int)pStatistic->CustomStat.ullRsr24MCRCOk);
    }
    else if(byRxRate==72){
        pStatistic->CustomStat.ullRsr36M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr36MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"36M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr36M, (int)pStatistic->CustomStat.ullRsr36MCRCOk);
    }
    else if(byRxRate==96){
        pStatistic->CustomStat.ullRsr48M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr48MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"48M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr48M, (int)pStatistic->CustomStat.ullRsr48MCRCOk);
    }
    else if(byRxRate==108){
        pStatistic->CustomStat.ullRsr54M++;
        if(byRSR & RSR_CRCOK) {
            pStatistic->CustomStat.ullRsr54MCRCOk++;
        }
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"54M: ALL[%d], OK[%d]\n", (int)pStatistic->CustomStat.ullRsr54M, (int)pStatistic->CustomStat.ullRsr54MCRCOk);
    }
    else {
    	DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Unknown: Total[%d], CRCOK[%d]\n", (int)pStatistic->dwRsrRxPacket+1, (int)pStatistic->dwRsrCRCOk);
    }

    if (byRSR & RSR_BSSIDOK)
        pStatistic->dwRsrBSSIDOk++;

    if (byRSR & RSR_BCNSSIDOK)
        pStatistic->dwRsrBCNSSIDOk++;
    if (byRSR & RSR_IVLDLEN)  
        pStatistic->dwRsrLENErr++;
    if (byRSR & RSR_IVLDTYP)  
        pStatistic->dwRsrTYPErr++;
    if (byRSR & (RSR_IVLDTYP | RSR_IVLDLEN))
        pStatistic->dwRsrErr++;

    if (byNewRSR & NEWRSR_DECRYPTOK)
        pStatistic->dwNewRsrDECRYPTOK++;
    if (byNewRSR & NEWRSR_CFPIND)
        pStatistic->dwNewRsrCFP++;
    if (byNewRSR & NEWRSR_HWUTSF)
        pStatistic->dwNewRsrUTSF++;
    if (byNewRSR & NEWRSR_BCNHITAID)
        pStatistic->dwNewRsrHITAID++;
    if (byNewRSR & NEWRSR_BCNHITAID0)
        pStatistic->dwNewRsrHITAID0++;

    
    pStatistic->dwRsrRxPacket++;
    pStatistic->dwRsrRxOctet += cbFrameLength;


    if (IS_TYPE_DATA(pbyBuffer)) {
        pStatistic->dwRsrRxData++;
    } else if (IS_TYPE_MGMT(pbyBuffer)){
        pStatistic->dwRsrRxManage++;
    } else if (IS_TYPE_CONTROL(pbyBuffer)){
        pStatistic->dwRsrRxControl++;
    }

    if (byRSR & RSR_ADDRBROAD)
        pStatistic->dwRsrBroadcast++;
    else if (byRSR & RSR_ADDRMULTI)
        pStatistic->dwRsrMulticast++;
    else
        pStatistic->dwRsrDirected++;

    if (WLAN_GET_FC_MOREFRAG(pHeader->wFrameCtl))
        pStatistic->dwRsrRxFragment++;

    if (cbFrameLength < ETH_ZLEN + 4) {
        pStatistic->dwRsrRunt++;
    }
    else if (cbFrameLength == ETH_ZLEN + 4) {
        pStatistic->dwRsrRxFrmLen64++;
    }
    else if ((65 <= cbFrameLength) && (cbFrameLength <= 127)) {
        pStatistic->dwRsrRxFrmLen65_127++;
    }
    else if ((128 <= cbFrameLength) && (cbFrameLength <= 255)) {
        pStatistic->dwRsrRxFrmLen128_255++;
    }
    else if ((256 <= cbFrameLength) && (cbFrameLength <= 511)) {
        pStatistic->dwRsrRxFrmLen256_511++;
    }
    else if ((512 <= cbFrameLength) && (cbFrameLength <= 1023)) {
        pStatistic->dwRsrRxFrmLen512_1023++;
    }
    else if ((1024 <= cbFrameLength) && (cbFrameLength <= ETH_FRAME_LEN + 4)) {
        pStatistic->dwRsrRxFrmLen1024_1518++;
    } else if (cbFrameLength > ETH_FRAME_LEN + 4) {
        pStatistic->dwRsrLong++;
    }

}




void
STAvUpdateRDStatCounterEx (
    PSStatCounter   pStatistic,
    unsigned char byRSR,
    unsigned char byNewRSR,
    unsigned char byRxRate,
    unsigned char *pbyBuffer,
    unsigned int cbFrameLength
    )
{
    STAvUpdateRDStatCounter(
                    pStatistic,
                    byRSR,
                    byNewRSR,
                    byRxRate,
                    pbyBuffer,
                    cbFrameLength
                    );

    
    pStatistic->dwCntRxFrmLength = cbFrameLength;
    
    memcpy(pStatistic->abyCntRxPattern, (unsigned char *)pbyBuffer, 10);
}


void
STAvUpdateTDStatCounter (
    PSStatCounter   pStatistic,
    unsigned char byTSR0,
    unsigned char byTSR1,
    unsigned char *pbyBuffer,
    unsigned int cbFrameLength,
    unsigned int uIdx
    )
{
    PWLAN_80211HDR_A4   pHeader;
    unsigned char *pbyDestAddr;
    unsigned char byTSR0_NCR = byTSR0 & TSR0_NCR;



    pHeader = (PWLAN_80211HDR_A4) pbyBuffer;
    if (WLAN_GET_FC_TODS(pHeader->wFrameCtl) == 0) {
        pbyDestAddr = &(pHeader->abyAddr1[0]);
    }
    else {
        pbyDestAddr = &(pHeader->abyAddr3[0]);
    }
    
    pStatistic->dwTsrTxPacket[uIdx]++;
    pStatistic->dwTsrTxOctet[uIdx] += cbFrameLength;

    if (byTSR0_NCR != 0) {
        pStatistic->dwTsrRetry[uIdx]++;
        pStatistic->dwTsrTotalRetry[uIdx] += byTSR0_NCR;

        if (byTSR0_NCR == 1)
            pStatistic->dwTsrOnceRetry[uIdx]++;
        else
            pStatistic->dwTsrMoreThanOnceRetry[uIdx]++;
    }

    if ((byTSR1&(TSR1_TERR|TSR1_RETRYTMO|TSR1_TMO|ACK_DATA)) == 0) {
        pStatistic->ullTsrOK[uIdx]++;
        pStatistic->CustomStat.ullTsrAllOK =
            (pStatistic->ullTsrOK[TYPE_AC0DMA] + pStatistic->ullTsrOK[TYPE_TXDMA0]);
        
        if (is_broadcast_ether_addr(pbyDestAddr)) {
            pStatistic->ullTxBroadcastFrames[uIdx]++;
            pStatistic->ullTxBroadcastBytes[uIdx] += (unsigned long long) cbFrameLength;
        }
        else if (is_multicast_ether_addr(pbyDestAddr)) {
            pStatistic->ullTxMulticastFrames[uIdx]++;
            pStatistic->ullTxMulticastBytes[uIdx] += (unsigned long long) cbFrameLength;
        }
        else {
            pStatistic->ullTxDirectedFrames[uIdx]++;
            pStatistic->ullTxDirectedBytes[uIdx] += (unsigned long long) cbFrameLength;
        }
    }
    else {
        if (byTSR1 & TSR1_TERR)
            pStatistic->dwTsrErr[uIdx]++;
        if (byTSR1 & TSR1_RETRYTMO)
            pStatistic->dwTsrRetryTimeout[uIdx]++;
        if (byTSR1 & TSR1_TMO)
            pStatistic->dwTsrTransmitTimeout[uIdx]++;
        if (byTSR1 & ACK_DATA)
            pStatistic->dwTsrACKData[uIdx]++;
    }

    if (is_broadcast_ether_addr(pbyDestAddr))
        pStatistic->dwTsrBroadcast[uIdx]++;
    else if (is_multicast_ether_addr(pbyDestAddr))
        pStatistic->dwTsrMulticast[uIdx]++;
    else
        pStatistic->dwTsrDirected[uIdx]++;

}


void
STAvUpdateTDStatCounterEx (
    PSStatCounter   pStatistic,
    unsigned char *pbyBuffer,
    unsigned long cbFrameLength
    )
{
    unsigned int uPktLength;

    uPktLength = (unsigned int)cbFrameLength;

    
    pStatistic->dwCntTxBufLength = uPktLength;
    
    memcpy(pStatistic->abyCntTxPattern, pbyBuffer, 16);
}


void
STAvUpdate802_11Counter(
    PSDot11Counters         p802_11Counter,
    PSStatCounter           pStatistic,
    unsigned long dwCounter
    )
{
    
    p802_11Counter->MulticastTransmittedFrameCount = (unsigned long long) (pStatistic->dwTsrBroadcast[TYPE_AC0DMA] +
                                                                  pStatistic->dwTsrBroadcast[TYPE_TXDMA0] +
                                                                  pStatistic->dwTsrMulticast[TYPE_AC0DMA] +
                                                                  pStatistic->dwTsrMulticast[TYPE_TXDMA0]);
    p802_11Counter->FailedCount = (unsigned long long) (pStatistic->dwTsrErr[TYPE_AC0DMA] + pStatistic->dwTsrErr[TYPE_TXDMA0]);
    p802_11Counter->RetryCount = (unsigned long long) (pStatistic->dwTsrRetry[TYPE_AC0DMA] + pStatistic->dwTsrRetry[TYPE_TXDMA0]);
    p802_11Counter->MultipleRetryCount = (unsigned long long) (pStatistic->dwTsrMoreThanOnceRetry[TYPE_AC0DMA] +
                                                          pStatistic->dwTsrMoreThanOnceRetry[TYPE_TXDMA0]);
    
    p802_11Counter->RTSSuccessCount += (unsigned long long)  (dwCounter & 0x000000ff);
    p802_11Counter->RTSFailureCount += (unsigned long long) ((dwCounter & 0x0000ff00) >> 8);
    p802_11Counter->ACKFailureCount += (unsigned long long) ((dwCounter & 0x00ff0000) >> 16);
    p802_11Counter->FCSErrorCount +=   (unsigned long long) ((dwCounter & 0xff000000) >> 24);
    
    p802_11Counter->MulticastReceivedFrameCount = (unsigned long long) (pStatistic->dwRsrBroadcast +
                                                               pStatistic->dwRsrMulticast);
}

void
STAvClear802_11Counter(PSDot11Counters p802_11Counter)
{
    
	memset(p802_11Counter, 0, sizeof(SDot11Counters));
}
