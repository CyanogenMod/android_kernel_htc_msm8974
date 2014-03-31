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
 * File: bssdb.h
 *
 * Purpose: Handles the Basic Service Set & Node Database functions
 *
 * Author: Lyndon Chen
 *
 * Date: July 16, 2002
 *
 */

#ifndef __BSSDB_H__
#define __BSSDB_H__

#include <linux/skbuff.h>
#include "80211hdr.h"
#include "80211mgr.h"
#include "card.h"
#include "mib.h"


#define MAX_NODE_NUM             64
#define MAX_BSS_NUM              42
#define LOST_BEACON_COUNT        10   
#define MAX_PS_TX_BUF            32   
#define ADHOC_LOST_BEACON_COUNT  30   
#define MAX_INACTIVE_COUNT       300  

#define USE_PROTECT_PERIOD       10   
#define ERP_RECOVER_COUNT        30   
#define BSS_CLEAR_COUNT           1

#define RSSI_STAT_COUNT          10
#define MAX_CHECK_RSSI_COUNT     8

#define WLAN_STA_AUTH            BIT0
#define WLAN_STA_ASSOC           BIT1
#define WLAN_STA_PS              BIT2
#define WLAN_STA_TIM             BIT3
#define WLAN_STA_PERM            BIT4
#define WLAN_STA_AUTHORIZED      BIT5


#define MAX_WPA_IE_LEN      64







typedef struct tagSERPObject {
    BOOL    bERPExist;
    BYTE    byERP;
} ERPObject, *PERPObject;


typedef struct tagSRSNCapObject {
    BOOL    bRSNCapExist;
    WORD    wRSNCap;
} SRSNCapObject, *PSRSNCapObject;

#pragma pack(1)
typedef struct tagKnownBSS {
    
    BOOL            bActive;
    BYTE            abyBSSID[WLAN_BSSID_LEN];
    unsigned int            uChannel;
    BYTE            abySuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    BYTE            abyExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    unsigned int            uRSSI;
    BYTE            bySQ;
    WORD            wBeaconInterval;
    WORD            wCapInfo;
    BYTE            abySSID[WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1];
    BYTE            byRxRate;

    BYTE            byRSSIStatCnt;
    signed long            ldBmMAX;
    signed long            ldBmAverage[RSSI_STAT_COUNT];
    signed long            ldBmAverRange;
    
    BOOL            bSelected;

    
    BOOL            bWPAValid;
    BYTE            byGKType;
    BYTE            abyPKType[4];
    WORD            wPKCount;
    BYTE            abyAuthType[4];
    WORD            wAuthCount;
    BYTE            byDefaultK_as_PK;
    BYTE            byReplayIdx;
    

    
    BOOL            bWPA2Valid;
    BYTE            byCSSGK;
    WORD            wCSSPKCount;
    BYTE            abyCSSPK[4];
    WORD            wAKMSSAuthCount;
    BYTE            abyAKMSSAuthType[4];

    
    BYTE            byWPAIE[MAX_WPA_IE_LEN];
    BYTE            byRSNIE[MAX_WPA_IE_LEN];
    WORD            wWPALen;
    WORD            wRSNLen;

    
    unsigned int            uClearCount;
    unsigned int            uIELength;
    QWORD           qwBSSTimestamp;
    QWORD           qwLocalTSF;     

    CARD_PHY_TYPE   eNetworkTypeInUse;

    ERPObject       sERP;
    SRSNCapObject   sRSNCapObj;
    BYTE            abyIEs[1024];   

} __attribute__ ((__packed__))
KnownBSS , *PKnownBSS;



typedef enum tagNODE_STATE {
    NODE_FREE,
    NODE_AGED,
    NODE_KNOWN,
    NODE_AUTH,
    NODE_ASSOC
} NODE_STATE, *PNODE_STATE;


typedef struct tagKnownNodeDB {
    
    BOOL            bActive;
    BYTE            abyMACAddr[WLAN_ADDR_LEN];
    BYTE            abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN];
    BYTE            abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN];
    WORD            wTxDataRate;
    BOOL            bShortPreamble;
    BOOL            bERPExist;
    BOOL            bShortSlotTime;
    unsigned int            uInActiveCount;
    WORD            wMaxBasicRate;     
    WORD            wMaxSuppRate;      
    WORD            wSuppRate;
    BYTE            byTopOFDMBasicRate;
    BYTE            byTopCCKBasicRate; 

    
    struct sk_buff_head sTxPSQueue;
    WORD            wCapInfo;
    WORD            wListenInterval;
    WORD            wAID;
    NODE_STATE      eNodeState;
    BOOL            bPSEnable;
    BOOL            bRxPSPoll;
    BYTE            byAuthSequence;
    unsigned long           ulLastRxJiffer;
    BYTE            bySuppRate;
    DWORD           dwFlags;
    WORD            wEnQueueCnt;

    BOOL            bOnFly;
    unsigned long long       KeyRSC;
    BYTE            byKeyIndex;
    DWORD           dwKeyIndex;
    BYTE            byCipherSuite;
    DWORD           dwTSC47_16;
    WORD            wTSC15_0;
    unsigned int            uWepKeyLength;
    BYTE            abyWepKey[WLAN_WEPMAX_KEYLEN];
    
    
    BOOL            bIsInFallback;
    unsigned int            uAverageRSSI;
    unsigned int            uRateRecoveryTimeout;
    unsigned int            uRatePollTimeout;
    unsigned int            uTxFailures;
    unsigned int            uTxAttempts;

    unsigned int            uTxRetry;
    unsigned int            uFailureRatio;
    unsigned int            uRetryRatio;
    unsigned int            uTxOk[MAX_RATE+1];
    unsigned int            uTxFail[MAX_RATE+1];
    unsigned int            uTimeCount;

} KnownNodeDB, *PKnownNodeDB;


PKnownBSS BSSpSearchBSSList(void *hDeviceContext,
			    PBYTE pbyDesireBSSID,
			    PBYTE pbyDesireSSID,
			    CARD_PHY_TYPE ePhyType);

PKnownBSS BSSpAddrIsInBSSList(void *hDeviceContext,
			      PBYTE abyBSSID,
			      PWLAN_IE_SSID pSSID);

void BSSvClearBSSList(void *hDeviceContext, BOOL bKeepCurrBSSID);

BOOL BSSbInsertToBSSList(void *hDeviceContext,
			 PBYTE abyBSSIDAddr,
			 QWORD qwTimestamp,
			 WORD wBeaconInterval,
			 WORD wCapInfo,
			 BYTE byCurrChannel,
			 PWLAN_IE_SSID pSSID,
			 PWLAN_IE_SUPP_RATES pSuppRates,
			 PWLAN_IE_SUPP_RATES pExtSuppRates,
			 PERPObject psERP,
			 PWLAN_IE_RSN pRSN,
			 PWLAN_IE_RSN_EXT pRSNWPA,
			 PWLAN_IE_COUNTRY pIE_Country,
			 PWLAN_IE_QUIET pIE_Quiet,
			 unsigned int uIELength,
			 PBYTE pbyIEs,
			 void *pRxPacketContext);

BOOL BSSbUpdateToBSSList(void *hDeviceContext,
			 QWORD qwTimestamp,
			 WORD wBeaconInterval,
			 WORD wCapInfo,
			 BYTE byCurrChannel,
			 BOOL bChannelHit,
			 PWLAN_IE_SSID pSSID,
			 PWLAN_IE_SUPP_RATES pSuppRates,
			 PWLAN_IE_SUPP_RATES pExtSuppRates,
			 PERPObject psERP,
			 PWLAN_IE_RSN pRSN,
			 PWLAN_IE_RSN_EXT pRSNWPA,
			 PWLAN_IE_COUNTRY pIE_Country,
			 PWLAN_IE_QUIET pIE_Quiet,
			 PKnownBSS pBSSList,
			 unsigned int uIELength,
			 PBYTE pbyIEs,
			 void *pRxPacketContext);

BOOL BSSbIsSTAInNodeDB(void *hDeviceContext,
		       PBYTE abyDstAddr,
		       unsigned int *puNodeIndex);

void BSSvCreateOneNode(void *hDeviceContext, unsigned int *puNodeIndex);

void BSSvUpdateAPNode(void *hDeviceContext,
		      PWORD pwCapInfo,
		      PWLAN_IE_SUPP_RATES pItemRates,
		      PWLAN_IE_SUPP_RATES pExtSuppRates);

void BSSvSecondCallBack(void *hDeviceContext);

void BSSvUpdateNodeTxCounter(void *hDeviceContext,
			     PSStatCounter pStatistic,
			     BYTE byTSR,
			     BYTE byPktNO);

void BSSvRemoveOneNode(void *hDeviceContext,
		       unsigned int uNodeIndex);

void BSSvAddMulticastNode(void *hDeviceContext);

void BSSvClearNodeDBTable(void *hDeviceContext,
			  unsigned int uStartIndex);

void BSSvClearAnyBSSJoinRecord(void *hDeviceContext);

#endif 
