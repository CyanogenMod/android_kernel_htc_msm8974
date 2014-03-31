/*
   This is part of rtl8180 OpenSource driver.
   Copyright (C) Andrea Merello 2004-2005  <andreamrl@tiscali.it>
   Released under the terms of GPL (General Public Licence)

   Parts of this driver are based on the GPL part of the
   official realtek driver

   Parts of this driver are based on the rtl8180 driver skeleton
   from Patric Schenke & Andres Salomon

   Parts of this driver are based on the Intel Pro Wireless 2100 GPL driver

   We want to tanks the Authors of those projects and the Ndiswrapper
   project Authors.
*/

#ifndef R8180H
#define R8180H

#include <linux/interrupt.h>

#define RTL8180_MODULE_NAME "r8180"
#define DMESG(x,a...) printk(KERN_INFO RTL8180_MODULE_NAME ": " x "\n", ## a)
#define DMESGW(x,a...) printk(KERN_WARNING RTL8180_MODULE_NAME ": WW:" x "\n", ## a)
#define DMESGE(x,a...) printk(KERN_WARNING RTL8180_MODULE_NAME ": EE:" x "\n", ## a)

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/rtnetlink.h>	
#include <linux/wireless.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>	
#include <linux/if_arp.h>
#include "ieee80211/ieee80211.h"
#include <asm/io.h>

#define EPROM_93c46 0
#define EPROM_93c56 1

#define RTL_IOCTL_WPA_SUPPLICANT		SIOCIWFIRSTPRIV+30

#define DEFAULT_FRAG_THRESHOLD 2342U
#define MIN_FRAG_THRESHOLD     256U
#define DEFAULT_RTS_THRESHOLD 2342U
#define MIN_RTS_THRESHOLD 0U
#define MAX_RTS_THRESHOLD 2342U
#define DEFAULT_BEACONINTERVAL 0x64U

#define DEFAULT_RETRY_RTS 7
#define DEFAULT_RETRY_DATA 7

#define BEACON_QUEUE					6

#define aSifsTime 	10

#define sCrcLng         4
#define sAckCtsLng	112		
#define RATE_ADAPTIVE_TIMER_PERIOD	300

typedef enum _WIRELESS_MODE {
	WIRELESS_MODE_UNKNOWN = 0x00,
	WIRELESS_MODE_A = 0x01,
	WIRELESS_MODE_B = 0x02,
	WIRELESS_MODE_G = 0x04,
	WIRELESS_MODE_AUTO = 0x08,
} WIRELESS_MODE;

typedef struct 	ChnlAccessSetting {
	u16 SIFS_Timer;
	u16 DIFS_Timer;
	u16 SlotTimeTimer;
	u16 EIFS_Timer;
	u16 CWminIndex;
	u16 CWmaxIndex;
}*PCHANNEL_ACCESS_SETTING,CHANNEL_ACCESS_SETTING;

typedef enum{
        NIC_8185 = 1,
        NIC_8185B
        } nic_t;

typedef u32 AC_CODING;
#define AC0_BE	0		
#define AC1_BK	1		
#define AC2_VI	2		
#define AC3_VO	3		
#define AC_MAX	4		

typedef	union _ECW{
	u8	charData;
	struct
	{
		u8	ECWmin:4;
		u8	ECWmax:4;
	}f;	
}ECW, *PECW;

typedef	union _ACI_AIFSN{
	u8	charData;

	struct
	{
		u8	AIFSN:4;
		u8	ACM:1;
		u8	ACI:2;
		u8	Reserved:1;
	}f;	
}ACI_AIFSN, *PACI_AIFSN;

typedef	union _AC_PARAM{
	u32	longData;
	u8	charData[4];

	struct
	{
		ACI_AIFSN	AciAifsn;
		ECW		Ecw;
		u16		TXOPLimit;
	}f;	
}AC_PARAM, *PAC_PARAM;


typedef	union _ThreeWire{
	struct _ThreeWireStruc{
		u16		data:1;
		u16		clk:1;
		u16		enableB:1;
		u16		read_write:1;
		u16		resv1:12;
	}struc;
	u16			longData;
}ThreeWireReg;


typedef struct buffer
{
	struct buffer *next;
	u32 *buf;
	dma_addr_t dma;
} buffer;

typedef struct Stats
{
	unsigned long txrdu;
	unsigned long rxrdu;
	unsigned long rxnolast;
	unsigned long rxnodata;
	unsigned long rxnopointer;
	unsigned long txnperr;
	unsigned long txresumed;
	unsigned long rxerr;
	unsigned long rxoverflow;
	unsigned long rxint;
	unsigned long txbkpokint;
	unsigned long txbepoking;
	unsigned long txbkperr;
	unsigned long txbeperr;
	unsigned long txnpokint;
	unsigned long txhpokint;
	unsigned long txhperr;
	unsigned long ints;
	unsigned long shints;
	unsigned long txoverflow;
	unsigned long rxdmafail;
	unsigned long txbeacon;
	unsigned long txbeaconerr;
	unsigned long txlpokint;
	unsigned long txlperr;
	unsigned long txretry;
	unsigned long rxcrcerrmin;
	unsigned long rxcrcerrmid;
	unsigned long rxcrcerrmax;
	unsigned long rxicverr;
} Stats;

#define MAX_LD_SLOT_NUM 10
#define KEEP_ALIVE_INTERVAL 				20 
#define CHECK_FOR_HANG_PERIOD			2 
#define DEFAULT_KEEP_ALIVE_LEVEL			1
#define DEFAULT_SLOT_NUM					2
#define POWER_PROFILE_AC					0
#define POWER_PROFILE_BATTERY			1

typedef struct _link_detect_t
{
	u32				RxFrameNum[MAX_LD_SLOT_NUM];	
	u16				SlotNum;	
	u16				SlotIndex;

	u32				NumTxOkInPeriod;  
	u32				NumRxOkInPeriod;  

	u8				IdleCount;     
	u32				LastNumTxUnicast;
	u32				LastNumRxUnicast;

	bool				bBusyTraffic;    
}link_detect_t, *plink_detect_t;



typedef	enum _LED_STRATEGY_8185{
	SW_LED_MODE0, 
	SW_LED_MODE1, 
	HW_LED, 
}LED_STRATEGY_8185, *PLED_STRATEGY_8185;
typedef	enum _LED_CTL_MODE{
	LED_CTL_POWER_ON = 1,
	LED_CTL_LINK = 2,
	LED_CTL_NO_LINK = 3,
	LED_CTL_TX = 4,
	LED_CTL_RX = 5,
	LED_CTL_SITE_SURVEY = 6,
	LED_CTL_POWER_OFF = 7
}LED_CTL_MODE;

typedef	enum _RT_RF_POWER_STATE
{
	eRfOn,
	eRfSleep,
	eRfOff
}RT_RF_POWER_STATE;

enum	_ReasonCode{
	unspec_reason	= 0x1,
	auth_not_valid	= 0x2,
	deauth_lv_ss	= 0x3,
	inactivity		= 0x4,
	ap_overload		= 0x5,
	class2_err		= 0x6,
	class3_err		= 0x7,
	disas_lv_ss		= 0x8,
	asoc_not_auth	= 0x9,

	
	mic_failure		= 0xe,
	

	
	invalid_IE		= 0x0d,
	four_way_tmout	= 0x0f,
	two_way_tmout	= 0x10,
	IE_dismatch		= 0x11,
	invalid_Gcipher	= 0x12,
	invalid_Pcipher	= 0x13,
	invalid_AKMP	= 0x14,
	unsup_RSNIEver = 0x15,
	invalid_RSNIE	= 0x16,
	auth_802_1x_fail= 0x17,
	ciper_reject		= 0x18,

	
	QoS_unspec		= 0x20,	
	QAP_bandwidth	= 0x21,	
	poor_condition	= 0x22,	
	no_facility		= 0x23,	
							
	req_declined	= 0x25,	
	invalid_param	= 0x26,	
	req_not_honored= 0x27,	
	TS_not_created	= 0x2F,	
	DL_not_allowed	= 0x30,	
	dest_not_exist	= 0x31,	
	dest_not_QSTA	= 0x32,	
};
typedef	enum _RT_PS_MODE
{
	eActive,	
	eMaxPs,		
	eFastPs		
}RT_PS_MODE;
typedef struct r8180_priv
{
	struct pci_dev *pdev;

	short epromtype;
	int irq;
	struct ieee80211_device *ieee80211;

	short phy_ver; 
	short enable_gpio0;
	short hw_plcp_len;
	short plcp_preamble_mode; 

	spinlock_t irq_lock;
	spinlock_t irq_th_lock;
	spinlock_t tx_lock;
	spinlock_t ps_lock;
	spinlock_t rf_ps_lock;

	u16 irq_mask;
	short irq_enabled;
	struct net_device *dev;
	short chan;
	short sens;
	short max_sens;
	u8 chtxpwr[15]; 
	u8 chtxpwr_ofdm[15]; 
	
	u8 channel_plan;  
	short up;
	short crcmon; 
	short prism_hdr;

	struct timer_list scan_timer;
	spinlock_t scan_lock;
	u8 active_probe;
	
	struct semaphore wx_sem;
	struct semaphore rf_state;
	short hw_wep;

	short digphy;
	short antb;
	short diversity;
	u8 cs_treshold;
	short rcr_csense;
	u32 key0[4];
	short (*rf_set_sens)(struct net_device *dev,short sens);
	void (*rf_set_chan)(struct net_device *dev,short ch);
	void (*rf_close)(struct net_device *dev);
	void (*rf_init)(struct net_device *dev);
	void (*rf_sleep)(struct net_device *dev);
	void (*rf_wakeup)(struct net_device *dev);
	
	short promisc;
	
	struct Stats stats;
	struct _link_detect_t link_detect;  
	struct iw_statistics wstats;
	struct proc_dir_entry *dir_dev;

	
	u32 *rxring;
	u32 *rxringtail;
	dma_addr_t rxringdma;
	struct buffer *rxbuffer;
	struct buffer *rxbufferhead;
	int rxringcount;
	u16 rxbuffersize;

	struct sk_buff *rx_skb;

	short rx_skb_complete;

	u32 rx_prevlen;

	
	u32 *txmapring;
	u32 *txbkpring;
	u32 *txbepring;
	u32 *txvipring;
	u32 *txvopring;
	u32 *txhpring;
	dma_addr_t txmapringdma;
	dma_addr_t txbkpringdma;
	dma_addr_t txbepringdma;
	dma_addr_t txvipringdma;
	dma_addr_t txvopringdma;
	dma_addr_t txhpringdma;
	u32 *txmapringtail;
	u32 *txbkpringtail;
	u32 *txbepringtail;
	u32 *txvipringtail;
	u32 *txvopringtail;
	u32 *txhpringtail;
	u32 *txmapringhead;
	u32 *txbkpringhead;
	u32 *txbepringhead;
	u32 *txvipringhead;
	u32 *txvopringhead;
	u32 *txhpringhead;
	struct buffer *txmapbufs;
	struct buffer *txbkpbufs;
	struct buffer *txbepbufs;
	struct buffer *txvipbufs;
	struct buffer *txvopbufs;
	struct buffer *txhpbufs;
	struct buffer *txmapbufstail;
	struct buffer *txbkpbufstail;
	struct buffer *txbepbufstail;
	struct buffer *txvipbufstail;
	struct buffer *txvopbufstail;
	struct buffer *txhpbufstail;

	int txringcount;
	int txbuffsize;
	
	
	struct tasklet_struct irq_rx_tasklet;
	u8 dma_poll_mask;
	

	
	u32 *txbeaconringtail;
	dma_addr_t txbeaconringdma;
	u32 *txbeaconring;
	int txbeaconcount;
	struct buffer *txbeaconbufs;
	struct buffer *txbeaconbufstail;
	
	
	
	

	u8 retry_data;
	u8 retry_rts;
	u16 rts;

	LED_STRATEGY_8185 LedStrategy;

	struct timer_list watch_dog_timer;
	bool bInactivePs;
	bool bSwRfProcessing;
	RT_RF_POWER_STATE	eInactivePowerState;
	RT_RF_POWER_STATE eRFPowerState;
	u32 RfOffReason;
	bool RFChangeInProgress;
	bool bInHctTest;
	bool SetRFPowerStateInProgress;
	u8   RFProgType;
	bool bLeisurePs;
	RT_PS_MODE dot11PowerSaveMode;
	
	
	u8   TxPollingTimes;

	bool	bApBufOurFrame;
	u8	WaitBufDataBcnCount;
	u8	WaitBufDataTimeOut;

	u8 EEPROMSwAntennaDiversity;
	bool EEPROMDefaultAntenna1;
	u8 RegSwAntennaDiversityMechanism;
	bool bSwAntennaDiverity;
	u8 RegDefaultAntenna;
	bool bDefaultAntenna1;
	u8 SignalStrength;
	long Stats_SignalStrength;
	long LastSignalStrengthInPercent; 
	u8	 SignalQuality; 
	long Stats_SignalQuality;
	long RecvSignalPower; 
	long Stats_RecvSignalPower;
	u8	 LastRxPktAntenna;	
	u32 AdRxOkCnt;
	long AdRxSignalStrength;
	u8 CurrAntennaIndex;			
	u8 AdTickCount;				
	u8 AdCheckPeriod;				
	u8 AdMinCheckPeriod;			
	u8 AdMaxCheckPeriod;			
	long AdRxSsThreshold;			
	long AdMaxRxSsThreshold;			
	bool bAdSwitchedChecking;		
	long AdRxSsBeforeSwitched;		
	struct timer_list SwAntennaDiversityTimer;
	
	
	
	bool		bXtalCalibration; 
	u8			XtalCal_Xin; 
	u8			XtalCal_Xout; 
	
	
	
	
	bool		bTxPowerTrack; 
	u8			ThermalMeter; 
	
	
	
	bool				bDigMechanism; 
	bool				bRegHighPowerMechanism; 
	u32					FalseAlarmRegValue;
	u8					RegDigOfdmFaUpTh; 
	u8					DIG_NumberFallbackVote;
	u8					DIG_NumberUpgradeVote;
	
	u32			AdMainAntennaRxOkCnt;		
	u32			AdAuxAntennaRxOkCnt;		
	bool		bHWAdSwitched;				
	
	u8					RegHiPwrUpperTh;
	u8					RegHiPwrLowerTh;
	
	u8					RegRSSIHiPwrUpperTh;
	u8					RegRSSIHiPwrLowerTh;
	
	u8			CurCCKRSSI;
	bool        bCurCCKPkt;
	
	
	
	bool					bToUpdateTxPwr;
	long					UndecoratedSmoothedSS;
	long					UndercorateSmoothedRxPower;
	u8						RSSI;
	char					RxPower;
	 u8 InitialGain;
	 
	u32				DozePeriodInPast2Sec;
	 
	u8					InitialGainBackUp;
	 u8 RegBModeGainStage;
    struct timer_list rateadapter_timer;
	u32    RateAdaptivePeriod;
	bool   bEnhanceTxPwr;
	bool   bUpdateARFR;
	int	   ForcedDataRate; 
	u32     NumTxUnicast; 
	u8      keepAliveLevel; 
	unsigned long 	NumTxOkTotal;
	u16                                 LastRetryCnt;
        u16                                     LastRetryRate;
        unsigned long       LastTxokCnt;
        unsigned long           LastRxokCnt;
        u16                                     CurrRetryCnt;
        unsigned long           LastTxOKBytes;
	unsigned long 		    NumTxOkBytesTotal;
        u8                          LastFailTxRate;
        long                        LastFailTxRateSS;
        u8                          FailTxRateCount;
        u32                         LastTxThroughput;
        
        unsigned short          bTryuping;
        u8                                      CurrTxRate;     
        u16                                     CurrRetryRate;
        u16                                     TryupingCount;
        u8                                      TryDownCountLowData;
        u8                                      TryupingCountNoData;

        u8                  CurrentOperaRate;
	struct work_struct reset_wq;
	struct work_struct watch_dog_wq;
	struct work_struct tx_irq_wq;
	short ack_tx_to_ieee;

	u8 PowerProfile;
	u32 CSMethod;
	u8 cck_txpwr_base;
	u8 ofdm_txpwr_base;
	u8 dma_poll_stop_mask;

	
	u8 MWIEnable;
	u16 ShortRetryLimit;
	u16 LongRetryLimit;
	u16 EarlyRxThreshold;
	u32 TransmitConfig;
	u32 ReceiveConfig;
	u32 IntrMask;

	struct 	ChnlAccessSetting  ChannelAccessSetting;
}r8180_priv;

#define MANAGE_PRIORITY 0
#define BK_PRIORITY 1
#define BE_PRIORITY 2
#define VI_PRIORITY 3
#define VO_PRIORITY 4
#define HI_PRIORITY 5
#define BEACON_PRIORITY 6

#define LOW_PRIORITY VI_PRIORITY
#define NORM_PRIORITY VO_PRIORITY
#define AC2Q(_ac) (((_ac) == WME_AC_VO) ? VO_PRIORITY : \
		((_ac) == WME_AC_VI) ? VI_PRIORITY : \
		((_ac) == WME_AC_BK) ? BK_PRIORITY : \
		BE_PRIORITY)

short rtl8180_tx(struct net_device *dev,u8* skbuf, int len,int priority,
	short morefrag,short fragdesc,int rate);

u8 read_nic_byte(struct net_device *dev, int x);
u32 read_nic_dword(struct net_device *dev, int x);
u16 read_nic_word(struct net_device *dev, int x) ;
void write_nic_byte(struct net_device *dev, int x,u8 y);
void write_nic_word(struct net_device *dev, int x,u16 y);
void write_nic_dword(struct net_device *dev, int x,u32 y);
void force_pci_posting(struct net_device *dev);

void rtl8180_rtx_disable(struct net_device *);
void rtl8180_rx_enable(struct net_device *);
void rtl8180_tx_enable(struct net_device *);
void rtl8180_start_scanning(struct net_device *dev);
void rtl8180_start_scanning_s(struct net_device *dev);
void rtl8180_stop_scanning(struct net_device *dev);
void rtl8180_disassociate(struct net_device *dev);
void rtl8180_set_anaparam(struct net_device *dev,u32 a);
void rtl8185_set_anaparam2(struct net_device *dev,u32 a);
void rtl8180_set_hw_wep(struct net_device *dev);
void rtl8180_no_hw_wep(struct net_device *dev);
void rtl8180_update_msr(struct net_device *dev);
void rtl8180_beacon_tx_disable(struct net_device *dev);
void rtl8180_beacon_rx_disable(struct net_device *dev);
void rtl8180_conttx_enable(struct net_device *dev);
void rtl8180_conttx_disable(struct net_device *dev);
int rtl8180_down(struct net_device *dev);
int rtl8180_up(struct net_device *dev);
void rtl8180_commit(struct net_device *dev);
void rtl8180_set_chan(struct net_device *dev,short ch);
void rtl8180_set_master_essid(struct net_device *dev,char *essid);
void rtl8180_update_beacon_security(struct net_device *dev);
void write_phy(struct net_device *dev, u8 adr, u8 data);
void write_phy_cck(struct net_device *dev, u8 adr, u32 data);
void write_phy_ofdm(struct net_device *dev, u8 adr, u32 data);
void rtl8185_tx_antenna(struct net_device *dev, u8 ant);
void rtl8185_rf_pins_enable(struct net_device *dev);
void IBSS_randomize_cell(struct net_device *dev);
void IPSEnter(struct net_device *dev);
void IPSLeave(struct net_device *dev);
int get_curr_tx_free_desc(struct net_device *dev, int priority);
void UpdateInitialGain(struct net_device *dev);
bool SetAntennaConfig87SE(struct net_device *dev, u8  DefaultAnt, bool bAntDiversity);

void rtl8185b_adapter_start(struct net_device *dev);
void rtl8185b_rx_enable(struct net_device *dev);
void rtl8185b_tx_enable(struct net_device *dev);
void rtl8180_reset(struct net_device *dev);
void rtl8185b_irq_enable(struct net_device *dev);
void fix_rx_fifo(struct net_device *dev);
void fix_tx_fifo(struct net_device *dev);
void rtl8225z2_SetTXPowerLevel(struct net_device *dev, short ch);
void rtl8180_rate_adapter(struct work_struct * work);
bool MgntActSet_RF_State(struct net_device *dev, RT_RF_POWER_STATE StateToSet, u32 ChangeSource);

#endif
