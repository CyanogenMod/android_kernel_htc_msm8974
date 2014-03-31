#ifndef __WINBOND_MDS_H
#define __WINBOND_MDS_H

#include <linux/timer.h>
#include <linux/types.h>
#include <linux/atomic.h>

#include "localpara.h"
#include "mac_structures.h"

enum {
	WLAN_PREAMBLE_TYPE_SHORT,
	WLAN_PREAMBLE_TYPE_LONG,
};

#define MAX_USB_TX_DESCRIPTOR		15	
#define MAX_USB_TX_BUFFER_NUMBER	4	
#define MAX_USB_TX_BUFFER		4096	

#define AUTH_REQUEST_PAIRWISE_ERROR	0	
#define AUTH_REQUEST_GROUP_ERROR	1	

#define CURRENT_FRAGMENT_THRESHOLD	(adapter->Mds.TxFragmentThreshold & ~0x1)
#define CURRENT_PREAMBLE_MODE		(psLOCAL->boShortPreamble ? WLAN_PREAMBLE_TYPE_SHORT : WLAN_PREAMBLE_TYPE_LONG)
#define CURRENT_TX_RATE_FOR_MNG		(adapter->sLocalPara.CurrentTxRateForMng)
#define CURRENT_PROTECT_MECHANISM	(psLOCAL->boProtectMechanism)
#define CURRENT_RTS_THRESHOLD		(adapter->Mds.TxRTSThreshold)

#define MIB_GS_XMIT_OK_INC		(adapter->sLocalPara.GS_XMIT_OK++)
#define MIB_GS_RCV_OK_INC		(adapter->sLocalPara.GS_RCV_OK++)
#define MIB_GS_XMIT_ERROR_INC		(adapter->sLocalPara.GS_XMIT_ERROR)

#define ETHERNET_TX_DESCRIPTORS         MAX_USB_TX_BUFFER_NUMBER

#define ETHERNET_RX_DESCRIPTORS		8	

#define DEFAULT_MULTICASTLISTMAX	32	
#define DEFAULT_TX_BURSTLENGTH		3	
#define DEFAULT_RX_BURSTLENGTH		3	
#define DEFAULT_TX_THRESHOLD		0	
#define DEFAULT_RX_THRESHOLD		0	
#define DEFAULT_MAXTXRATE		6	
#define DEFAULT_CHANNEL			3	
#define DEFAULT_RTSThreshold		2347	
#define DEFAULT_PME			0	
#define DEFAULT_SIFSTIME		10
#define DEFAULT_ACKTIME_1ML             304	
#define DEFAULT_ACKTIME_2ML             248	
#define DEFAULT_FRAGMENT_THRESHOLD      2346	
#define DEFAULT_PREAMBLE_LENGTH		72
#define DEFAULT_PLCPHEADERTIME_LENGTH	24

#define DEFAULT_MAX_RECEIVE_TIME	16440000

#define RX_BUF_SIZE			2352	
#define MAX_RX_DESCRIPTORS		18	

#define MDS_ID_IGNORE			ETHERNET_RX_DESCRIPTORS

#define PACKET_FREE_TO_USE		0
#define PACKET_COME_FROM_NDIS		0x08
#define PACKET_COME_FROM_MLME		0x80
#define PACKET_SEND_COMPLETE		0xff

struct wb35_mds {
	
	u8	TxOwner[((MAX_USB_TX_BUFFER_NUMBER + 3) & ~0x03)];
	u8	*pTxBuffer;
	u16	TxBufferSize[((MAX_USB_TX_BUFFER_NUMBER + 1) & ~0x01)];
	u8	TxDesFrom[((MAX_USB_TX_DESCRIPTOR + 3) & ~0x03)];
	u8	TxCountInBuffer[((MAX_USB_TX_DESCRIPTOR + 3) & ~0x03)];

	u8	TxFillIndex;	
	u8	TxDesIndex;	
	u8	ScanTxPause;	
	u8	TxPause;	

	atomic_t	TxThreadCount;	

	u16	TxResult[((MAX_USB_TX_DESCRIPTOR + 1) & ~0x01)];

	u8	MicRedundant[8]; 
	u8	*MicWriteAddress[2]; 

	u16	MicWriteSize[2];

	u16	MicAdd; 
	u16	MicWriteIndex; 

	u8	TxRate[((MAX_USB_TX_DESCRIPTOR + 1) & ~0x01)][2]; 
	u8	TxInfo[((MAX_USB_TX_DESCRIPTOR + 1) & ~0x01)]; 

	
	u16	TxFragmentThreshold;	
	u16	TxRTSThreshold;

	u32	MaxReceiveTime;

	
	u32	MulticastListNo;
	u32	PacketFilter; 
	u8	MulticastAddressesArray[DEFAULT_MULTICASTLISTMAX][MAC_ADDR_LENGTH];

	
	u8	bMICfailCount;
	u8	boCounterMeasureBlock;
	u8	reserved_4[2];

	u32	TxTsc;
	u32	TxTsc_2;
};

#endif
