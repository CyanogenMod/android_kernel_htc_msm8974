#ifndef B43legacy_XMIT_H_
#define B43legacy_XMIT_H_

#include "main.h"


#define _b43legacy_declare_plcp_hdr(size)	\
	struct b43legacy_plcp_hdr##size {	\
		union {				\
			__le32 data;		\
			__u8 raw[size];		\
		} __packed;	\
	} __packed

_b43legacy_declare_plcp_hdr(4);
_b43legacy_declare_plcp_hdr(6);

#undef _b43legacy_declare_plcp_hdr


struct b43legacy_txhdr_fw3 {
	__le32 mac_ctl;				
	__le16 mac_frame_ctl;			
	__le16 tx_fes_time_norm;		
	__le16 phy_ctl;				
	__u8 iv[16];				
	__u8 tx_receiver[6];			
	__le16 tx_fes_time_fb;			
	struct b43legacy_plcp_hdr4 rts_plcp_fb;	
	__le16 rts_dur_fb;			
	struct b43legacy_plcp_hdr4 plcp_fb;	
	__le16 dur_fb;				
	PAD_BYTES(2);
	__le16 cookie;
	__le16 unknown_scb_stuff;
	struct b43legacy_plcp_hdr6 rts_plcp;	
	__u8 rts_frame[18];			
	struct b43legacy_plcp_hdr6 plcp;
} __packed;

#define B43legacy_TX4_MAC_KEYIDX	0x0FF00000 
#define B43legacy_TX4_MAC_KEYIDX_SHIFT	20
#define B43legacy_TX4_MAC_KEYALG	0x00070000 
#define B43legacy_TX4_MAC_KEYALG_SHIFT	16
#define B43legacy_TX4_MAC_LIFETIME	0x00001000
#define B43legacy_TX4_MAC_FRAMEBURST	0x00000800
#define B43legacy_TX4_MAC_SENDCTS	0x00000400
#define B43legacy_TX4_MAC_AMPDU		0x00000300
#define B43legacy_TX4_MAC_AMPDU_SHIFT	8
#define B43legacy_TX4_MAC_CTSFALLBACKOFDM	0x00000200
#define B43legacy_TX4_MAC_FALLBACKOFDM	0x00000100
#define B43legacy_TX4_MAC_5GHZ		0x00000080
#define B43legacy_TX4_MAC_IGNPMQ	0x00000020
#define B43legacy_TX4_MAC_HWSEQ		0x00000010 
#define B43legacy_TX4_MAC_STMSDU	0x00000008 
#define B43legacy_TX4_MAC_SENDRTS	0x00000004
#define B43legacy_TX4_MAC_LONGFRAME	0x00000002
#define B43legacy_TX4_MAC_ACK		0x00000001

#define B43legacy_TX4_EFT_FBOFDM	0x0001 
#define B43legacy_TX4_EFT_RTSOFDM	0x0004 
#define B43legacy_TX4_EFT_RTSFBOFDM	0x0010 

#define B43legacy_TX4_PHY_ENC		0x0003 
#define B43legacy_TX4_PHY_ENC_CCK	0x0000 
#define B43legacy_TX4_PHY_ENC_OFDM	0x0001 
#define B43legacy_TX4_PHY_SHORTPRMBL	0x0010 
#define B43legacy_TX4_PHY_ANT		0x03C0 
#define  B43legacy_TX4_PHY_ANT0		0x0000 
#define  B43legacy_TX4_PHY_ANT1		0x0100 
#define  B43legacy_TX4_PHY_ANTLAST	0x0300 



int b43legacy_generate_txhdr(struct b43legacy_wldev *dev,
			      u8 *txhdr,
			      const unsigned char *fragment_data,
			      unsigned int fragment_len,
			      struct ieee80211_tx_info *info,
			      u16 cookie);


struct b43legacy_txstatus {
	u16 cookie;	
	u16 seq;	
	u8 phy_stat;	
	u8 frame_count;	
	u8 rts_count;	
	u8 supp_reason;	
	
	u8 pm_indicated;
	u8 intermediate;
	u8 for_ampdu;	
	u8 acked;	
};

enum {
	B43legacy_TXST_SUPP_NONE,	
	B43legacy_TXST_SUPP_PMQ,	
	B43legacy_TXST_SUPP_FLUSH,	
	B43legacy_TXST_SUPP_PREV,	
	B43legacy_TXST_SUPP_CHAN,	
	B43legacy_TXST_SUPP_LIFE,	
	B43legacy_TXST_SUPP_UNDER,	
	B43legacy_TXST_SUPP_ABNACK,	
};

struct b43legacy_hwtxstatus {
	PAD_BYTES(4);
	__le16 cookie;
	u8 flags;
	u8 count;
	PAD_BYTES(2);
	__le16 seq;
	u8 phy_stat;
	PAD_BYTES(1);
} __packed;


struct b43legacy_rxhdr_fw3 {
	__le16 frame_len;	
	PAD_BYTES(2);
	__le16 phy_status0;	
	__u8 jssi;		
	__u8 sig_qual;		
	PAD_BYTES(2);		
	__le16 phy_status3;	
	__le16 mac_status;	
	__le16 mac_time;
	__le16 channel;
} __packed;


#define B43legacy_RX_PHYST0_GAINCTL	0x4000 
#define B43legacy_RX_PHYST0_PLCPHCF	0x0200
#define B43legacy_RX_PHYST0_PLCPFV	0x0100
#define B43legacy_RX_PHYST0_SHORTPRMBL	0x0080 
#define B43legacy_RX_PHYST0_LCRS	0x0040
#define B43legacy_RX_PHYST0_ANT		0x0020 
#define B43legacy_RX_PHYST0_UNSRATE	0x0010
#define B43legacy_RX_PHYST0_CLIP	0x000C
#define B43legacy_RX_PHYST0_CLIP_SHIFT	2
#define B43legacy_RX_PHYST0_FTYPE	0x0003 
#define  B43legacy_RX_PHYST0_CCK	0x0000 
#define  B43legacy_RX_PHYST0_OFDM	0x0001 
#define  B43legacy_RX_PHYST0_PRE_N	0x0002 
#define  B43legacy_RX_PHYST0_STD_N	0x0003 

#define B43legacy_RX_PHYST2_LNAG	0xC000 
#define B43legacy_RX_PHYST2_LNAG_SHIFT	14
#define B43legacy_RX_PHYST2_PNAG	0x3C00 
#define B43legacy_RX_PHYST2_PNAG_SHIFT	10
#define B43legacy_RX_PHYST2_FOFF	0x03FF 

#define B43legacy_RX_PHYST3_DIGG	0x1800 
#define B43legacy_RX_PHYST3_DIGG_SHIFT	11
#define B43legacy_RX_PHYST3_TRSTATE	0x0400 

#define B43legacy_RX_MAC_BEACONSENT	0x00008000 
#define B43legacy_RX_MAC_KEYIDX		0x000007E0 
#define B43legacy_RX_MAC_KEYIDX_SHIFT	5
#define B43legacy_RX_MAC_DECERR		0x00000010 
#define B43legacy_RX_MAC_DEC		0x00000008 
#define B43legacy_RX_MAC_PADDING	0x00000004 
#define B43legacy_RX_MAC_RESP		0x00000002 
#define B43legacy_RX_MAC_FCSERR		0x00000001 

#define B43legacy_RX_CHAN_GAIN		0xFC00 
#define B43legacy_RX_CHAN_GAIN_SHIFT	10
#define B43legacy_RX_CHAN_ID		0x03FC 
#define B43legacy_RX_CHAN_ID_SHIFT	2
#define B43legacy_RX_CHAN_PHYTYPE	0x0003 



u8 b43legacy_plcp_get_ratecode_cck(const u8 bitrate);
u8 b43legacy_plcp_get_ratecode_ofdm(const u8 bitrate);

void b43legacy_generate_plcp_hdr(struct b43legacy_plcp_hdr4 *plcp,
			       const u16 octets, const u8 bitrate);

void b43legacy_rx(struct b43legacy_wldev *dev,
		struct sk_buff *skb,
		const void *_rxhdr);

void b43legacy_handle_txstatus(struct b43legacy_wldev *dev,
			       const struct b43legacy_txstatus *status);

void b43legacy_handle_hwtxstatus(struct b43legacy_wldev *dev,
				 const struct b43legacy_hwtxstatus *hw);

void b43legacy_tx_suspend(struct b43legacy_wldev *dev);
void b43legacy_tx_resume(struct b43legacy_wldev *dev);


#define B43legacy_NR_QOSPARMS	22
enum {
	B43legacy_QOSPARM_TXOP = 0,
	B43legacy_QOSPARM_CWMIN,
	B43legacy_QOSPARM_CWMAX,
	B43legacy_QOSPARM_CWCUR,
	B43legacy_QOSPARM_AIFS,
	B43legacy_QOSPARM_BSLOTS,
	B43legacy_QOSPARM_REGGAP,
	B43legacy_QOSPARM_STATUS,
};

void b43legacy_qos_init(struct b43legacy_wldev *dev);


static inline
int b43legacy_new_kidx_api(struct b43legacy_wldev *dev)
{
	
	return (dev->fw.rev >= 351);
}
static inline
u8 b43legacy_kidx_to_fw(struct b43legacy_wldev *dev, u8 raw_kidx)
{
	u8 firmware_kidx;
	if (b43legacy_new_kidx_api(dev))
		firmware_kidx = raw_kidx;
	else {
		if (raw_kidx >= 4) 
			firmware_kidx = raw_kidx - 4;
		else
			firmware_kidx = raw_kidx; 
	}
	return firmware_kidx;
}
static inline
u8 b43legacy_kidx_to_raw(struct b43legacy_wldev *dev, u8 firmware_kidx)
{
	u8 raw_kidx;
	if (b43legacy_new_kidx_api(dev))
		raw_kidx = firmware_kidx;
	else
		
		raw_kidx = firmware_kidx + 4;
	return raw_kidx;
}

#endif 
