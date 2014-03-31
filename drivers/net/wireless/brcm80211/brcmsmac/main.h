/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BRCM_MAIN_H_
#define _BRCM_MAIN_H_

#include <linux/etherdevice.h>

#include <brcmu_utils.h>
#include "types.h"
#include "d11.h"
#include "scb.h"

#define	INVCHANNEL		255	

#define BRCMS_MAXMODULES	22

#define SEQNUM_SHIFT		4
#define SEQNUM_MAX		0x1000

#define NTXRATE			64	

#define	BRCMS_MAX_MAC_SUSPEND	83000

#define BRCMS_PRB_RESP_TIMEOUT	0	

#define TXOFF (D11_TXH_LEN + D11_PHY_HDR_LEN)

#define BITFIELD_MASK(width) \
		(((unsigned)1 << (width)) - 1)
#define GFIELD(val, field) \
		(((val) >> field ## _S) & field ## _M)
#define SFIELD(val, field, bits) \
		(((val) & (~(field ## _M << field ## _S))) | \
		 ((unsigned)(bits) << field ## _S))

#define	SW_TIMER_MAC_STAT_UPD		30	

#define	MAXCOREREV		28

#if CONF_MSK(D11CONF, 0x4f) || CONF_GE(D11CONF, MAXCOREREV)
#error "Configuration for D11CONF includes unsupported versions."
#endif				

#define BRCMS_SHORTSLOT_AUTO	-1 
#define BRCMS_SHORTSLOT_OFF	0  
#define BRCMS_SHORTSLOT_ON	1  

#define BRCMS_LONG_PREAMBLE	(0)
#define BRCMS_SHORT_PREAMBLE	(1 << 0)
#define BRCMS_GF_PREAMBLE		(1 << 1)
#define BRCMS_MM_PREAMBLE		(1 << 2)
#define BRCMS_IS_MIMO_PREAMBLE(_pre) (((_pre) == BRCMS_GF_PREAMBLE) || \
				      ((_pre) == BRCMS_MM_PREAMBLE))

#define TXFID_QUEUE_MASK	0x0007	
#define TXFID_SEQ_MASK		0x7FE0	
#define TXFID_SEQ_SHIFT		5	
#define	TXFID_RATE_PROBE_MASK	0x8000	
#define TXFID_RATE_MASK		0x0018	
#define TXFID_RATE_SHIFT	3	

#define BOARDREV_PROMOTABLE	0xFF	
#define BOARDREV_PROMOTED	1	

#define DATA_BLOCK_TX_SUPR	(1 << 4)

extern const u8 prio2fifo[];

#define BRCMS_WAKE_OVERRIDE_CLKCTL	0x01
#define BRCMS_WAKE_OVERRIDE_PHYREG	0x02
#define BRCMS_WAKE_OVERRIDE_MACSUSPEND	0x04
#define BRCMS_WAKE_OVERRIDE_TXFIFO	0x08
#define BRCMS_WAKE_OVERRIDE_FORCEFAST	0x10


#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RO | I_XU)
#define	DEF_RXINTMASK	(I_RI)	
#define	DEF_MACINTMASK	(MI_TXSTOP | MI_TBTT | MI_ATIMWINEND | MI_PMQ | \
			 MI_PHYTXERR | MI_DMAINT | MI_TFS | MI_BG_NOISE | \
			 MI_CCA | MI_TO | MI_GP0 | MI_RFDISABLE | MI_PWRUP)

#define	MAXTXPKTS		6	

#define	MAXTXFRAMEBURST		8 
#define	MAXFRAMEBURST_TXOP	10000	

#define	NFIFO			6	


#define BRCMS_PLLREQ_SHARED	0x1
#define BRCMS_PLLREQ_RADIO_MON	0x2
#define BRCMS_PLLREQ_FLIP		0x4

#define	CHANNEL_BANDUNIT(wlc, ch) \
	(((ch) <= CH_MAX_2G_CHANNEL) ? BAND_2G_INDEX : BAND_5G_INDEX)

#define	OTHERBANDUNIT(wlc) \
	((uint)((wlc)->band->bandunit ? BAND_2G_INDEX : BAND_5G_INDEX))

struct brcms_protection {
	bool _g;
	s8 g_override;
	u8 gmode_user;
	s8 overlap;
	s8 nmode_user;
	s8 n_cfg;
	s8 n_cfg_override;
	bool nongf;
	s8 nongf_override;
	s8 n_pam_override;
	bool n_obss;
};

struct brcms_stf {
	u8 hw_txchain;
	u8 txchain;
	u8 txstreams;
	u8 hw_rxchain;
	u8 rxchain;
	u8 rxstreams;
	u8 ant_rx_ovr;
	s8 txant;
	u16 phytxant;
	u8 ss_opmode;
	bool ss_algosel_auto;
	u16 ss_algo_channel;
	u8 rxchain_restore_delay;
	s8 ldpc;
	u8 txcore[MAX_STREAMS_SUPPORTED + 1];
	s8 spatial_policy;
};

#define BRCMS_STF_SS_STBC_TX(wlc, scb) \
	(((wlc)->stf->txstreams > 1) && (((wlc)->band->band_stf_stbc_tx == ON) \
	 || (((scb)->flags & SCB_STBCCAP) && \
	     (wlc)->band->band_stf_stbc_tx == AUTO && \
	     isset(&((wlc)->stf->ss_algo_channel), PHY_TXC1_MODE_STBC))))

#define BRCMS_STBC_CAP_PHY(wlc) (BRCMS_ISNPHY(wlc->band) && \
				 NREV_GE(wlc->band->phyrev, 3))

#define BRCMS_SGI_CAP_PHY(wlc) ((BRCMS_ISNPHY(wlc->band) && \
				 NREV_GE(wlc->band->phyrev, 3)) || \
				BRCMS_ISLCNPHY(wlc->band))

#define BRCMS_CHAN_PHYTYPE(x)     (((x) & RXS_CHAN_PHYTYPE_MASK) \
				   >> RXS_CHAN_PHYTYPE_SHIFT)
#define BRCMS_CHAN_CHANNEL(x)     (((x) & RXS_CHAN_ID_MASK) \
				   >> RXS_CHAN_ID_SHIFT)

struct brcms_core {
	uint coreidx;		

	
	uint *txavail[NFIFO];	
	s16 txpktpend[NFIFO];	

	struct macstat *macstat_snapshot;	
};

struct brcms_band {
	int bandtype;		
	uint bandunit;		

	u16 phytype;		
	u16 phyrev;
	u16 radioid;
	u16 radiorev;
	struct brcms_phy_pub *pi; 
	bool abgphy_encore;

	u8 gmode;		

	struct scb *hwrs_scb;	

	
	struct brcms_c_rateset defrateset;

	u8 band_stf_ss_mode;	
	s8 band_stf_stbc_tx;	
	
	struct brcms_c_rateset hw_rateset;
	u8 basic_rate[BRCM_MAXRATE + 1]; 
	bool mimo_cap_40;	
	s8 antgain;		

	u16 CWmin; 
	u16 CWmax; 
	struct ieee80211_supported_band band;
};

struct modulecb {
	
	char name[32];
	
	struct brcms_info *hdl;

	int (*down_fn)(void *handle); 

};

struct brcms_hw_band {
	int bandtype;		
	uint bandunit;		
	u16 mhfs[MHFMAX];	
	u8 bandhw_stf_ss_mode;	
	u16 CWmin;
	u16 CWmax;
	u32 core_flags;

	u16 phytype;		
	u16 phyrev;
	u16 radioid;
	u16 radiorev;
	struct brcms_phy_pub *pi; 
	bool abgphy_encore;
};

struct brcms_hardware {
	bool _piomode;		
	struct brcms_c_info *wlc;

	
	struct dma_pub *di[NFIFO];	

	uint unit;		

	
	u16 vendorid;	
	u16 deviceid;	
	uint corerev;		
	u8 sromrev;		
	u16 boardrev;	
	u32 boardflags;	
	u32 boardflags2;	
	u32 machwcap;	
	u32 machwcap_backup;	

	struct si_pub *sih;	
	struct bcma_device *d11core;	
	struct phy_shim_info *physhim; 
	struct shared_phy *phy_sh;	
	struct brcms_hw_band *band;
	
	struct brcms_hw_band *bandstate[MAXBANDS];
	u16 bmac_phytxant;	
	bool shortslot;		
	u16 SRL;		
	u16 LRL;		
	u16 SFBL;		
	u16 LFBL;		

	bool up;		
	uint now;		
	uint _nbands;		
	u16 chanspec;	

	uint *txavail[NFIFO];	
	const u16 *xmtfifo_sz;	

	u32 pllreq;		

	u8 suspended_fifos;	
	u32 maccontrol;	
	uint mac_suspend_depth;	
	u32 wake_override;	
	u32 mute_override;	
	u8 etheraddr[ETH_ALEN];	
	bool noreset;		
	bool forcefastclk;	
	bool clk;		
	bool sbclk;		
	bool phyclk;		

	bool ucode_loaded;	


	u8 hw_stf_ss_opmode;	
	u8 antsel_type;	
	u32 antsel_avail;	
};

struct brcms_txq_info {
	struct brcms_txq_info *next;
	struct pktq q;
	uint stopped;		
};

struct brcms_c_info {
	struct brcms_pub *pub;
	struct brcms_info *wl;
	struct brcms_hardware *hw;

	
	u16 fastpwrup_dly;

	
	u32 macintstatus;
	u32 macintmask;
	u32 defmacintmask;

	bool clk;

	
	struct brcms_core *core;
	struct brcms_band *band;
	struct brcms_core *corestate;
	struct brcms_band *bandstate[MAXBANDS];

	
	uint qvalid;

	struct ampdu_info *ampdu;
	struct antsel_info *asi;
	struct brcms_cm_info *cmi;

	u16 vendorid;
	u16 deviceid;
	uint ucode_rev;

	u8 perm_etheraddr[ETH_ALEN];

	bool bandlocked;
	bool bandinit_pending;

	bool radio_monitor;
	bool going_down;

	struct brcms_timer *wdtimer;
	struct brcms_timer *radio_timer;

	
	uint filter_flags;

	
	bool _rifs;

	
	u8 bcn_li_bcn;
	u8 bcn_li_dtim;

	bool WDarmed;
	u32 WDlast;

	
	u16 edcf_txop[IEEE80211_NUM_ACS];

	u16 wme_retries[IEEE80211_NUM_ACS];
	u16 tx_prec_map;
	u16 fifo2prec_map[NFIFO];

	struct brcms_bss_cfg *bsscfg;

	
	struct brcms_txq_info *tx_queues;

	struct modulecb *modulecb;

	u8 mimoft;
	s8 cck_40txbw;
	s8 ofdm_40txbw;
	s8 mimo_40txbw;

	struct brcms_bss_info *default_bss;

	u16 mc_fid_counter;

	char country_default[BRCM_CNTRY_BUF_SZ];
	char autocountry_default[BRCM_CNTRY_BUF_SZ];
	u16 prb_resp_timeout;

	u16 home_chanspec;

	
	u16 chanspec;
	u16 usr_fragthresh;
	u16 fragthresh[NFIFO];
	u16 RTSThresh;
	u16 SRL;
	u16 LRL;
	u16 SFBL;
	u16 LFBL;

	
	bool shortslot;
	s8 shortslot_override;
	bool include_legacy_erp;

	struct brcms_protection *protection;
	s8 PLCPHdr_override;

	struct brcms_stf *stf;

	u32 bcn_rspec;

	uint tempsense_lasttime;

	u16 tx_duty_cycle_ofdm;
	u16 tx_duty_cycle_cck;

	struct brcms_txq_info *pkt_queue;
	struct wiphy *wiphy;
	struct scb pri_scb;
};

struct antsel_info {
	struct brcms_c_info *wlc;	
	struct brcms_pub *pub;		
	u8 antsel_type;	
	u8 antsel_antswitch;	
	bool antsel_avail;	
	struct brcms_antselcfg antcfg_11n; 
	struct brcms_antselcfg antcfg_cur; 
};

struct brcms_bss_cfg {
	struct brcms_c_info *wlc;
	bool up;
	bool enable;
	bool associated;
	bool BSS;
	u8 SSID_len;
	u8 SSID[IEEE80211_MAX_SSID_LEN];
	u8 BSSID[ETH_ALEN];
	u8 cur_etheraddr[ETH_ALEN];
	struct brcms_bss_info *current_bss;
};

extern void brcms_c_txfifo(struct brcms_c_info *wlc, uint fifo,
			   struct sk_buff *p,
			   bool commit, s8 txpktpend);
extern void brcms_c_txfifo_complete(struct brcms_c_info *wlc, uint fifo,
				    s8 txpktpend);
extern void brcms_c_txq_enq(struct brcms_c_info *wlc, struct scb *scb,
			    struct sk_buff *sdu, uint prec);
extern void brcms_c_print_txstatus(struct tx_status *txs);
extern int brcms_b_xmtfifo_sz_get(struct brcms_hardware *wlc_hw, uint fifo,
		   uint *blocks);

#if defined(DEBUG)
extern void brcms_c_print_txdesc(struct d11txh *txh);
#else
static inline void brcms_c_print_txdesc(struct d11txh *txh)
{
}
#endif

extern int brcms_c_set_gmode(struct brcms_c_info *wlc, u8 gmode, bool config);
extern void brcms_c_mac_promisc(struct brcms_c_info *wlc, uint filter_flags);
extern void brcms_c_send_q(struct brcms_c_info *wlc);
extern int brcms_c_prep_pdu(struct brcms_c_info *wlc, struct sk_buff *pdu,
			    uint *fifo);
extern u16 brcms_c_calc_lsig_len(struct brcms_c_info *wlc, u32 ratespec,
				uint mac_len);
extern u32 brcms_c_rspec_to_rts_rspec(struct brcms_c_info *wlc,
					     u32 rspec,
					     bool use_rspec, u16 mimo_ctlchbw);
extern u16 brcms_c_compute_rtscts_dur(struct brcms_c_info *wlc, bool cts_only,
				      u32 rts_rate,
				      u32 frame_rate,
				      u8 rts_preamble_type,
				      u8 frame_preamble_type, uint frame_len,
				      bool ba);
extern void brcms_c_inval_dma_pkts(struct brcms_hardware *hw,
			       struct ieee80211_sta *sta,
			       void (*dma_callback_fn));
extern void brcms_c_update_beacon(struct brcms_c_info *wlc);
extern void brcms_c_update_probe_resp(struct brcms_c_info *wlc, bool suspend);
extern int brcms_c_set_nmode(struct brcms_c_info *wlc);
extern void brcms_c_beacon_phytxctl_txant_upd(struct brcms_c_info *wlc,
					  u32 bcn_rate);
extern void brcms_b_antsel_type_set(struct brcms_hardware *wlc_hw,
				     u8 antsel_type);
extern void brcms_b_set_chanspec(struct brcms_hardware *wlc_hw,
				  u16 chanspec,
				  bool mute, struct txpwr_limits *txpwr);
extern void brcms_b_write_shm(struct brcms_hardware *wlc_hw, uint offset,
			      u16 v);
extern u16 brcms_b_read_shm(struct brcms_hardware *wlc_hw, uint offset);
extern void brcms_b_mhf(struct brcms_hardware *wlc_hw, u8 idx, u16 mask,
			u16 val, int bands);
extern void brcms_b_corereset(struct brcms_hardware *wlc_hw, u32 flags);
extern void brcms_b_mctrl(struct brcms_hardware *wlc_hw, u32 mask, u32 val);
extern void brcms_b_phy_reset(struct brcms_hardware *wlc_hw);
extern void brcms_b_bw_set(struct brcms_hardware *wlc_hw, u16 bw);
extern void brcms_b_core_phypll_reset(struct brcms_hardware *wlc_hw);
extern void brcms_c_ucode_wake_override_set(struct brcms_hardware *wlc_hw,
					u32 override_bit);
extern void brcms_c_ucode_wake_override_clear(struct brcms_hardware *wlc_hw,
					  u32 override_bit);
extern void brcms_b_write_template_ram(struct brcms_hardware *wlc_hw,
				       int offset, int len, void *buf);
extern u16 brcms_b_rate_shm_offset(struct brcms_hardware *wlc_hw, u8 rate);
extern void brcms_b_copyto_objmem(struct brcms_hardware *wlc_hw,
				   uint offset, const void *buf, int len,
				   u32 sel);
extern void brcms_b_copyfrom_objmem(struct brcms_hardware *wlc_hw, uint offset,
				     void *buf, int len, u32 sel);
extern void brcms_b_switch_macfreq(struct brcms_hardware *wlc_hw, u8 spurmode);
extern u16 brcms_b_get_txant(struct brcms_hardware *wlc_hw);
extern void brcms_b_phyclk_fgc(struct brcms_hardware *wlc_hw, bool clk);
extern void brcms_b_macphyclk_set(struct brcms_hardware *wlc_hw, bool clk);
extern void brcms_b_core_phypll_ctl(struct brcms_hardware *wlc_hw, bool on);
extern void brcms_b_txant_set(struct brcms_hardware *wlc_hw, u16 phytxant);
extern void brcms_b_band_stf_ss_set(struct brcms_hardware *wlc_hw,
				    u8 stf_mode);
extern void brcms_c_init_scb(struct scb *scb);

#endif				
