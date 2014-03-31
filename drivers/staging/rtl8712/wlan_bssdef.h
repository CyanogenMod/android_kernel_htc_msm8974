/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Modifications for inclusion into the Linux staging tree are
 * Copyright(c) 2010 Larry Finger. All rights reserved.
 *
 * Contact information:
 * WLAN FAE <wlanfae@realtek.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/
#ifndef __WLAN_BSSDEF_H__
#define __WLAN_BSSDEF_H__

#define MAX_IE_SZ	768

#define NDIS_802_11_LENGTH_SSID         32
#define NDIS_802_11_LENGTH_RATES        8
#define NDIS_802_11_LENGTH_RATES_EX     16

typedef unsigned char   NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];
typedef unsigned char   NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];

struct ndis_802_11_ssid {
	u32 SsidLength;
	u8  Ssid[32];
};

enum NDIS_802_11_NETWORK_TYPE {
	Ndis802_11FH,
	Ndis802_11DS,
	Ndis802_11OFDM5,
	Ndis802_11OFDM24,
	Ndis802_11NetworkTypeMax 
};

struct NDIS_802_11_CONFIGURATION_FH {
	u32 Length;             
	u32 HopPattern;         
	u32 HopSet;             
	u32 DwellTime;          
};

struct NDIS_802_11_CONFIGURATION {
	u32 Length;             
	u32 BeaconPeriod;       
	u32 ATIMWindow;         
	u32 DSConfig;           
	struct NDIS_802_11_CONFIGURATION_FH FHConfig;
};

enum NDIS_802_11_NETWORK_INFRASTRUCTURE {
	Ndis802_11IBSS,
	Ndis802_11Infrastructure,
	Ndis802_11AutoUnknown,
	Ndis802_11InfrastructureMax, 
	Ndis802_11APMode
};

struct NDIS_802_11_FIXED_IEs {
	u8  Timestamp[8];
	u16 BeaconInterval;
	u16 Capabilities;
};


struct ndis_wlan_bssid_ex {
	u32 Length;
	unsigned char  MacAddress[6];
	u8  Reserved[2];
	struct ndis_802_11_ssid  Ssid;
	u32 Privacy;
	s32 Rssi;
	enum NDIS_802_11_NETWORK_TYPE  NetworkTypeInUse;
	struct NDIS_802_11_CONFIGURATION  Configuration;
	enum NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
	NDIS_802_11_RATES_EX  SupportedRates;
	u32 IELength;
	
	u8 IEs[MAX_IE_SZ];
};

enum NDIS_802_11_AUTHENTICATION_MODE {
	Ndis802_11AuthModeOpen,
	Ndis802_11AuthModeShared,
	Ndis802_11AuthModeAutoSwitch,
	Ndis802_11AuthModeWPA,
	Ndis802_11AuthModeWPAPSK,
	Ndis802_11AuthModeWPANone,
	Ndis802_11AuthModeMax      
};

enum {
	Ndis802_11WEPEnabled,
	Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
	Ndis802_11WEPDisabled,
	Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
	Ndis802_11WEPKeyAbsent,
	Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
	Ndis802_11WEPNotSupported,
	Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
	Ndis802_11Encryption2Enabled,
	Ndis802_11Encryption2KeyAbsent,
	Ndis802_11Encryption3Enabled,
	Ndis802_11Encryption3KeyAbsent
};

#define NDIS_802_11_AI_REQFI_CAPABILITIES      1
#define NDIS_802_11_AI_REQFI_LISTENINTERVAL    2
#define NDIS_802_11_AI_REQFI_CURRENTAPADDRESS  4

#define NDIS_802_11_AI_RESFI_CAPABILITIES      1
#define NDIS_802_11_AI_RESFI_STATUSCODE        2
#define NDIS_802_11_AI_RESFI_ASSOCIATIONID     4

struct NDIS_802_11_AI_REQFI {
	u16 Capabilities;
	u16 ListenInterval;
	unsigned char CurrentAPAddress[6];
};

struct NDIS_802_11_AI_RESFI {
	u16 Capabilities;
	u16 StatusCode;
	u16 AssociationId;
};

struct NDIS_802_11_ASSOCIATION_INFORMATION {
	u32 Length;
	u16 AvailableRequestFixedIEs;
	struct NDIS_802_11_AI_REQFI RequestFixedIEs;
	u32 RequestIELength;
	u32 OffsetRequestIEs;
	u16 AvailableResponseFixedIEs;
	struct NDIS_802_11_AI_RESFI ResponseFixedIEs;
	u32 ResponseIELength;
	u32 OffsetResponseIEs;
};

struct NDIS_802_11_KEY {
	u32 Length;			
	u32 KeyIndex;
	u32 KeyLength;			
	unsigned char BSSID[6];
	unsigned long long KeyRSC;
	u8  KeyMaterial[32];		
};

struct NDIS_802_11_REMOVE_KEY {
	u32 Length;			
	u32 KeyIndex;
	unsigned char BSSID[6];
};

struct NDIS_802_11_WEP {
	u32 Length;		  
	u32 KeyIndex;		  
	u32 KeyLength;		  
	u8  KeyMaterial[16];      
};

#define NDIS_802_11_AUTH_REQUEST_AUTH_FIELDS        0x0f
#define NDIS_802_11_AUTH_REQUEST_REAUTH			0x01
#define NDIS_802_11_AUTH_REQUEST_KEYUPDATE		0x02
#define NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR		0x06
#define NDIS_802_11_AUTH_REQUEST_GROUP_ERROR		0x0E

#define MIC_CHECK_TIME	60000000

#ifndef Ndis802_11APMode
#define Ndis802_11APMode (Ndis802_11InfrastructureMax+1)
#endif

struct	wlan_network {
	struct list_head list;
	int	network_type;	
	int	fixed;		
	unsigned int	last_scanned; 
	int	aid;		
	int	join_res;
	struct ndis_wlan_bssid_ex network; 
};

enum VRTL_CARRIER_SENSE {
	DISABLE_VCS,
	ENABLE_VCS,
	AUTO_VCS
};

enum VCS_TYPE {
	NONE_VCS,
	RTS_CTS,
	CTS_TO_SELF
};

#define PWR_CAM 0
#define PWR_MINPS 1
#define PWR_MAXPS 2
#define PWR_UAPSD 3
#define PWR_VOIP 4

enum UAPSD_MAX_SP {
	NO_LIMIT,
	TWO_MSDU,
	FOUR_MSDU,
	SIX_MSDU
};

#define NUM_PRE_AUTH_KEY 16
#define NUM_PMKID_CACHE NUM_PRE_AUTH_KEY

struct wlan_bssid_ex {
	u32 Length;
	unsigned char  MacAddress[6];
	u8  Reserved[2];
	struct ndis_802_11_ssid  Ssid;
	u32 Privacy;
	s32 Rssi;
	enum NDIS_802_11_NETWORK_TYPE  NetworkTypeInUse;
	struct NDIS_802_11_CONFIGURATION  Configuration;
	enum NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
	NDIS_802_11_RATES_EX  SupportedRates;
	u32 IELength;
	u8  IEs[MAX_IE_SZ];	
};

#endif 

