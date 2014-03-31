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


#define MAX_NODE_NUM             64
#define MAX_BSS_NUM              42
#define LOST_BEACON_COUNT      	 10   
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

#define MAX_RATE            12

#define MAX_WPA_IE_LEN      64







typedef enum _NDIS_802_11_NETWORK_TYPE
{
    Ndis802_11FH,
    Ndis802_11DS,
    Ndis802_11OFDM5,
    Ndis802_11OFDM24,
    Ndis802_11NetworkTypeMax    
} NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;


typedef struct tagSERPObject {
    bool bERPExist;
    unsigned char byERP;
}ERPObject, *PERPObject;


typedef struct tagSRSNCapObject {
    bool bRSNCapExist;
    unsigned short wRSNCap;
}SRSNCapObject, *PSRSNCapObject;

#pragma pack(1)
typedef struct tagKnownBSS {
    
    bool bActive;
    unsigned char abyBSSID[WLAN_BSSID_LEN];
    unsigned int	uChannel;
    unsigned char abySuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    unsigned char abyExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN + 1];
    unsigned int	uRSSI;
    unsigned char bySQ;
    unsigned short wBeaconInterval;
    unsigned short wCapInfo;
    unsigned char abySSID[WLAN_IEHDR_LEN + WLAN_SSID_MAXLEN + 1];
    unsigned char byRxRate;

    unsigned char byRSSIStatCnt;
    long            ldBmMAX;
    long            ldBmAverage[RSSI_STAT_COUNT];
    long            ldBmAverRange;
    
    bool bSelected;

    
    bool bWPAValid;
    unsigned char byGKType;
    unsigned char abyPKType[4];
    unsigned short wPKCount;
    unsigned char abyAuthType[4];
    unsigned short wAuthCount;
    unsigned char byDefaultK_as_PK;
    unsigned char byReplayIdx;
    

    
    bool bWPA2Valid;
    unsigned char byCSSGK;
    unsigned short wCSSPKCount;
    unsigned char abyCSSPK[4];
    unsigned short wAKMSSAuthCount;
    unsigned char abyAKMSSAuthType[4];

    
    unsigned char byWPAIE[MAX_WPA_IE_LEN];
    unsigned char byRSNIE[MAX_WPA_IE_LEN];
    unsigned short wWPALen;
    unsigned short wRSNLen;

    
    unsigned int	uClearCount;
    unsigned int	uIELength;
    QWORD           qwBSSTimestamp;
    QWORD           qwLocalTSF;     

    CARD_PHY_TYPE   eNetworkTypeInUse;

    ERPObject       sERP;
    SRSNCapObject   sRSNCapObj;
    unsigned char abyIEs[1024];   

}__attribute__ ((__packed__))
KnownBSS , *PKnownBSS;

#pragma pack()

typedef enum tagNODE_STATE {
    NODE_FREE,
    NODE_AGED,
    NODE_KNOWN,
    NODE_AUTH,
    NODE_ASSOC
} NODE_STATE, *PNODE_STATE;


typedef struct tagKnownNodeDB {
    
    bool bActive;
    unsigned char abyMACAddr[WLAN_ADDR_LEN];
    unsigned char abyCurrSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN];
    unsigned char abyCurrExtSuppRates[WLAN_IEHDR_LEN + WLAN_RATES_MAXLEN];
    unsigned short wTxDataRate;
    bool bShortPreamble;
    bool bERPExist;
    bool bShortSlotTime;
    unsigned int	uInActiveCount;
    unsigned short wMaxBasicRate;     
    unsigned short wMaxSuppRate;      
    unsigned short wSuppRate;
    unsigned char byTopOFDMBasicRate;
    unsigned char byTopCCKBasicRate; 

    
    struct sk_buff_head sTxPSQueue;
    unsigned short wCapInfo;
    unsigned short wListenInterval;
    unsigned short wAID;
    NODE_STATE      eNodeState;
    bool bPSEnable;
    bool bRxPSPoll;
    unsigned char byAuthSequence;
    unsigned long ulLastRxJiffer;
    unsigned char bySuppRate;
    unsigned long dwFlags;
    unsigned short wEnQueueCnt;

    bool bOnFly;
    unsigned long long       KeyRSC;
    unsigned char byKeyIndex;
    unsigned long dwKeyIndex;
    unsigned char byCipherSuite;
    unsigned long dwTSC47_16;
    unsigned short wTSC15_0;
    unsigned int	uWepKeyLength;
    unsigned char abyWepKey[WLAN_WEPMAX_KEYLEN];
    
    
    bool bIsInFallback;
    unsigned int	uAverageRSSI;
    unsigned int	uRateRecoveryTimeout;
    unsigned int	uRatePollTimeout;
    unsigned int	uTxFailures;
    unsigned int	uTxAttempts;

    unsigned int	uTxRetry;
    unsigned int	uFailureRatio;
    unsigned int	uRetryRatio;
    unsigned int	uTxOk[MAX_RATE+1];
    unsigned int	uTxFail[MAX_RATE+1];
    unsigned int	uTimeCount;

} KnownNodeDB, *PKnownNodeDB;





PKnownBSS
BSSpSearchBSSList(
    void *hDeviceContext,
    unsigned char *pbyDesireBSSID,
    unsigned char *pbyDesireSSID,
    CARD_PHY_TYPE ePhyType
    );

PKnownBSS
BSSpAddrIsInBSSList(
    void *hDeviceContext,
    unsigned char *abyBSSID,
    PWLAN_IE_SSID pSSID
    );

void
BSSvClearBSSList(
    void *hDeviceContext,
    bool bKeepCurrBSSID
    );

bool
BSSbInsertToBSSList(
    void *hDeviceContext,
    unsigned char *abyBSSIDAddr,
    QWORD qwTimestamp,
    unsigned short wBeaconInterval,
    unsigned short wCapInfo,
    unsigned char byCurrChannel,
    PWLAN_IE_SSID pSSID,
    PWLAN_IE_SUPP_RATES pSuppRates,
    PWLAN_IE_SUPP_RATES pExtSuppRates,
    PERPObject psERP,
    PWLAN_IE_RSN pRSN,
    PWLAN_IE_RSN_EXT pRSNWPA,
    PWLAN_IE_COUNTRY pIE_Country,
    PWLAN_IE_QUIET pIE_Quiet,
    unsigned int uIELength,
    unsigned char *pbyIEs,
    void *pRxPacketContext
    );


bool
BSSbUpdateToBSSList(
    void *hDeviceContext,
    QWORD qwTimestamp,
    unsigned short wBeaconInterval,
    unsigned short wCapInfo,
    unsigned char byCurrChannel,
    bool bChannelHit,
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
    unsigned char *pbyIEs,
    void *pRxPacketContext
    );


bool
BSSDBbIsSTAInNodeDB(void *hDeviceContext, unsigned char *abyDstAddr,
		unsigned int *puNodeIndex);

void
BSSvCreateOneNode(void *hDeviceContext, unsigned int *puNodeIndex);

void
BSSvUpdateAPNode(
    void *hDeviceContext,
    unsigned short *pwCapInfo,
    PWLAN_IE_SUPP_RATES pItemRates,
    PWLAN_IE_SUPP_RATES pExtSuppRates
    );


void
BSSvSecondCallBack(
    void *hDeviceContext
    );


void
BSSvUpdateNodeTxCounter(
    void *hDeviceContext,
    unsigned char byTsr0,
    unsigned char byTsr1,
    unsigned char *pbyBuffer,
    unsigned int uFIFOHeaderSize
    );

void
BSSvRemoveOneNode(
    void *hDeviceContext,
    unsigned int uNodeIndex
    );

void
BSSvAddMulticastNode(
    void *hDeviceContext
    );


void
BSSvClearNodeDBTable(
    void *hDeviceContext,
    unsigned int uStartIndex
    );

void
BSSvClearAnyBSSJoinRecord(
    void *hDeviceContext
    );

#endif 
