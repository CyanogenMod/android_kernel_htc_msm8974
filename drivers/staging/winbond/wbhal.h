#ifndef __WINBOND_WBHAL_S_H
#define __WINBOND_WBHAL_S_H

#include <linux/types.h>
#include <linux/if_ether.h> 

#define HAL_LED_SET_MASK	0x001c
#define HAL_LED_SET_SHIFT	2

#define RF_MAXIM_2825		0
#define RF_MAXIM_2827		1
#define RF_MAXIM_2828		2
#define RF_MAXIM_2829		3
#define RF_MAXIM_V1		15
#define RF_AIROHA_2230		16
#define RF_AIROHA_7230		17
#define RF_AIROHA_2230S		18
#define RF_WB_242		33
#define RF_WB_242_1		34
#define RF_DECIDE_BY_INF	255


#define HAL_USB_MODE_BURST(_H)			(_H->SoftwareSet & 0x20)

#define SCAN_MAX_CHNL_TIME			(50)

#define FRAME_TYPE_802_3_DATA			0
#define FRAME_TYPE_802_11_MANAGEMENT		1
#define FRAME_TYPE_802_11_MANAGEMENT_CHALLENGE	2
#define FRAME_TYPE_802_11_CONTROL		3
#define FRAME_TYPE_802_11_DATA			4
#define FRAME_TYPE_PROMISCUOUS			5

#define DOT_11_SEQUENCE_OFFSET			22 
#define DOT_3_TYPE_OFFSET			12
#define DOT_11_MAC_HEADER_SIZE			24
#define DOT_11_SNAP_SIZE			6
#define DOT_11_TYPE_OFFSET			30 
#define DEFAULT_SIFSTIME			10
#define DEFAULT_FRAGMENT_THRESHOLD		2346 
#define DEFAULT_MSDU_LIFE_TIME			0xffff

#define LONG_PREAMBLE_PLUS_PLCPHEADER_TIME		(144 + 48)
#define SHORT_PREAMBLE_PLUS_PLCPHEADER_TIME		(72 + 24)
#define PREAMBLE_PLUS_SIGNAL_PLUS_SIGNALEXTENSION	(16 + 4 + 6)
#define Tsym						4

#define MAC_TYPE_MANAGEMENT			0x00
#define MAC_TYPE_CONTROL			0x04
#define MAC_TYPE_DATA				0x08
#define MASK_FRAGMENT_NUMBER			0x000F
#define SEQUENCE_NUMBER_SHIFT			4

#define  HAL_WOL_TYPE_WAKEUP_FRAME		0x01
#define  HAL_WOL_TYPE_MAGIC_PACKET		0x02

#define HAL_KEYTYPE_WEP40			0
#define HAL_KEYTYPE_WEP104			1
#define HAL_KEYTYPE_TKIP			2 
#define HAL_KEYTYPE_AES_CCMP			3 

enum {
	VM_STOP = 0,
	VM_RUNNING,
	VM_COMPLETED
};


#define MAX_KEY_TABLE			24 
#define GROUP_KEY_START_INDEX		4
#define MAPPING_KEY_START_INDEX		8

#define MAX_DESCRIPTOR_BUFFER_INDEX	8 
#define FLAG_ERROR_TX_MASK		0x000000bf
#define FLAG_ERROR_RX_MASK		0x0000083f

#define FLAG_BAND_RX_MASK		0x10000000 

struct R00_descriptor {
	union {
		u32	value;
#ifdef _BIG_ENDIAN_
		struct {
			u32	R00_packet_or_buffer_status:1;
			u32	R00_packet_in_fifo:1;
			u32	R00_RESERVED:2;
			u32	R00_receive_byte_count:12;
			u32	R00_receive_time_index:16;
		};
#else
		struct {
			u32	R00_receive_time_index:16;
			u32	R00_receive_byte_count:12;
			u32	R00_RESERVED:2;
			u32	R00_packet_in_fifo:1;
			u32	R00_packet_or_buffer_status:1;
		};
#endif
	};
};

struct T00_descriptor {
	union {
		u32	value;
#ifdef _BIG_ENDIAN_
		struct {
			u32	T00_first_mpdu:1; 
			u32	T00_last_mpdu:1; 
			u32	T00_IsLastMpdu:1;
			u32	T00_IgnoreResult:1;
			u32	T00_RESERVED_ID:2;
			u32	T00_tx_packet_id:4;
			u32	T00_RESERVED:4;
			u32	T00_header_length:6;
			u32	T00_frame_length:12;
		};
#else
		struct {
			u32	T00_frame_length:12;
			u32	T00_header_length:6;
			u32	T00_RESERVED:4;
			u32	T00_tx_packet_id:4;
			u32	T00_RESERVED_ID:2; 
			u32	T00_IgnoreResult:1; 
			u32	T00_IsLastMpdu:1; 
			u32	T00_last_mpdu:1; 
			u32	T00_first_mpdu:1; 
		};
#endif
	};
};

struct R01_descriptor {
	union {
		u32	value;
#ifdef _BIG_ENDIAN_
		struct {
			u32	R01_RESERVED:3;
			u32	R01_mod_type:1;
			u32	R01_pre_type:1;
			u32	R01_data_rate:3;
			u32	R01_AGC_state:8;
			u32	R01_LNA_state:2;
			u32	R01_decryption_method:2;
			u32	R01_mic_error:1;
			u32	R01_replay:1;
			u32	R01_broadcast_frame:1;
			u32	R01_multicast_frame:1;
			u32	R01_directed_frame:1;
			u32	R01_receive_frame_antenna_selection:1;
			u32	R01_frame_receive_during_atim_window:1;
			u32	R01_protocol_version_error:1;
			u32	R01_authentication_frame_icv_error:1;
			u32	R01_null_key_to_authentication_frame:1;
			u32	R01_icv_error:1;
			u32	R01_crc_error:1;
		};
#else
		struct {
			u32	R01_crc_error:1;
			u32	R01_icv_error:1;
			u32	R01_null_key_to_authentication_frame:1;
			u32	R01_authentication_frame_icv_error:1;
			u32	R01_protocol_version_error:1;
			u32	R01_frame_receive_during_atim_window:1;
			u32	R01_receive_frame_antenna_selection:1;
			u32	R01_directed_frame:1;
			u32	R01_multicast_frame:1;
			u32	R01_broadcast_frame:1;
			u32	R01_replay:1;
			u32	R01_mic_error:1;
			u32	R01_decryption_method:2;
			u32	R01_LNA_state:2;
			u32	R01_AGC_state:8;
			u32	R01_data_rate:3;
			u32	R01_pre_type:1;
			u32	R01_mod_type:1;
			u32	R01_RESERVED:3;
		};
#endif
	};
};

struct T01_descriptor {
	union {
		u32	value;
#ifdef _BIG_ENDIAN_
		struct {
			u32	T01_rts_cts_duration:16;
			u32	T01_fall_back_rate:3;
			u32	T01_add_rts:1;
			u32	T01_add_cts:1;
			u32	T01_modulation_type:1;
			u32	T01_plcp_header_length:1;
			u32	T01_transmit_rate:3;
			u32	T01_wep_id:2;
			u32	T01_add_challenge_text:1;
			u32	T01_inhibit_crc:1;
			u32	T01_loop_back_wep_mode:1;
			u32	T01_retry_abort_ebable:1;
		};
#else
		struct {
			u32	T01_retry_abort_ebable:1;
			u32	T01_loop_back_wep_mode:1;
			u32	T01_inhibit_crc:1;
			u32	T01_add_challenge_text:1;
			u32	T01_wep_id:2;
			u32	T01_transmit_rate:3;
			u32	T01_plcp_header_length:1;
			u32	T01_modulation_type:1;
			u32	T01_add_cts:1;
			u32	T01_add_rts:1;
			u32	T01_fall_back_rate:3;
			u32	T01_rts_cts_duration:16;
		};
#endif
	};
};

struct T02_descriptor {
	union {
		u32	value;
#ifdef _BIG_ENDIAN_
		struct {
			u32	T02_IsLastMpdu:1; 
			u32	T02_IgnoreResult:1; 
			u32	T02_RESERVED_ID:2; 
			u32	T02_Tx_PktID:4;
			u32	T02_MPDU_Cnt:4;
			u32	T02_RTS_Cnt:4;
			u32	T02_RESERVED:7;
			u32	T02_transmit_complete:1;
			u32	T02_transmit_abort_due_to_TBTT:1;
			u32	T02_effective_transmission_rate:1;
			u32	T02_transmit_without_encryption_due_to_wep_on_false:1;
			u32	T02_discard_due_to_null_wep_key:1;
			u32	T02_RESERVED_1:1;
			u32	T02_out_of_MaxTxMSDULiftTime:1;
			u32	T02_transmit_abort:1;
			u32	T02_transmit_fail:1;
		};
#else
		struct {
			u32	T02_transmit_fail:1;
			u32	T02_transmit_abort:1;
			u32	T02_out_of_MaxTxMSDULiftTime:1;
			u32	T02_RESERVED_1:1;
			u32	T02_discard_due_to_null_wep_key:1;
			u32	T02_transmit_without_encryption_due_to_wep_on_false:1;
			u32	T02_effective_transmission_rate:1;
			u32	T02_transmit_abort_due_to_TBTT:1;
			u32	T02_transmit_complete:1;
			u32	T02_RESERVED:7;
			u32	T02_RTS_Cnt:4;
			u32	T02_MPDU_Cnt:4;
			u32	T02_Tx_PktID:4;
			u32	T02_RESERVED_ID:2; 
			u32	T02_IgnoreResult:1; 
			u32	T02_IsLastMpdu:1; 
		};
#endif
	};
};

struct wb35_descriptor { 
	
	u8	Descriptor_ID;
	
	u8	RESERVED[3];

	u16	FragmentThreshold;
	u8	InternalUsed; 
	u8	Type; 

	u8	PreambleMode;
	u8	TxRate;
	u8	FragmentCount;
	u8	EapFix; 

	
	union {
		struct R00_descriptor	R00;
		struct T00_descriptor	T00;
	};

	
	union {
		struct R01_descriptor	R01;
		struct T01_descriptor	T01;
	};

	
	union {
		u32		R02;
		struct T02_descriptor	T02;
	};

	
	
	union {
		u32	R03;
		u32	T03;
		struct {
			u8	buffer_number;
			u8	buffer_start_index;
			u16	buffer_total_size;
		};
	};

	
	u16	buffer_size[MAX_DESCRIPTOR_BUFFER_INDEX];
	void	*buffer_address[MAX_DESCRIPTOR_BUFFER_INDEX];
};

#define MAX_TXVGA_EEPROM		9	
#define MAX_RF_PARAMETER		32

struct txvga_for_50 {
	u8	ChanNo;
	u8	TxVgaValue;
};


#include "wb35reg_s.h"
#include "wb35tx_s.h"
#include "wb35rx_s.h"

struct hw_data {
	
	u32	revision;
	u32	BB3c_cal; 
	u32	BB54_cal; 

	
	u32	SurpriseRemove; 
	u8	IsKeyPreSet;
	u8	CalOneTime;

	u8	VCO_trim;

	u32	FragCount;
	u32	DMAFix; 

	u8	PermanentMacAddress[ETH_ALEN + 2]; 
	u8	CurrentMacAddress[ETH_ALEN + 2]; 

	u8	*bssid_pointer; 
	u8	bssid[8]; 
	u8	ssid[32]; 

	u16	AID;
	u8	ssid_length;
	u8	Channel;

	u16	ListenInterval;
	u16	CapabilityInformation;

	u16	BeaconPeriod;
	u16	ProbeDelay;

	u8	bss_type;
	u8	preamble;
	u8	slot_time_select; 
	u8	phy_type; 

	u32	phy_para[MAX_RF_PARAMETER];
	u32	phy_number;

	u32	CurrentRadioSw; 
	u32	CurrentRadioHw; 

	u8	*power_save_point; 
	u8	cwmin;
	u8	desired_power_save;
	u8	dtim; 
	u8	mapping_key_replace_index; 

	u16	MaxReceiveLifeTime;
	u16	FragmentThreshold;
	u16	FragmentThreshold_tmp;
	u16	cwmax;

	u8	Key_slot[MAX_KEY_TABLE][8]; 
	u32	Key_content[MAX_KEY_TABLE][12]; 
	u8	CurrentDefaultKeyIndex;
	u32	CurrentDefaultKeyLength;

	struct usb_device	*udev;
	struct wb35_reg		reg;	
	struct wb35_tx		Wb35Tx; 
	struct wb35_rx		Wb35Rx; 

	struct timer_list	LEDTimer; 

	u32			LEDpoint; 

	u32			dto_tx_retry_count;
	u32			dto_tx_frag_count;
	u32			rx_ok_count[13]; 
	u32			rx_err_count[13]; 

	
	u32			tx_TBTT_start_count;
	u32			tx_ETR_count;
	u32			tx_WepOn_false_count;
	u32			tx_Null_key_count;
	u32			tx_retry_count[8];

	u8			PowerIndexFromEEPROM; 
	u8			power_index;
	u8			IsWaitJoinComplete; 
	u8			band;

	u16			SoftwareSet;
	u16			Reserved_s;

	u32			IsInitOK; 

	
	s32			iq_rsdl_gain_tx_d2;
	s32			iq_rsdl_phase_tx_d2;
	u32			txvga_setting_for_cal;

	u8			TxVgaSettingInEEPROM[(((MAX_TXVGA_EEPROM * 2) + 3) & ~0x03)]; 
	u8			TxVgaFor24[16]; 
	struct txvga_for_50		TxVgaFor50[36];	

	u16			Scan_Interval;
	u16			RESERVED6;

	
	u32		LED_control;
	u32		LED_LinkOn;	
	u32		LED_Scanning;	
	u32		LED_Blinking;	
	u32		RxByteCountLast;
	u32		TxByteCountLast;

	
	u32		time_count;	
};

#endif
