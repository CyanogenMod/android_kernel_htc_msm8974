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

#ifndef _BRCM_PUB_H_
#define _BRCM_PUB_H_

#include <linux/bcma/bcma.h>
#include <brcmu_wifi.h>
#include "types.h"
#include "defs.h"

enum brcms_srom_id {
	BRCMS_SROM_NULL,
	BRCMS_SROM_CONT,
	BRCMS_SROM_AA2G,
	BRCMS_SROM_AA5G,
	BRCMS_SROM_AG0,
	BRCMS_SROM_AG1,
	BRCMS_SROM_AG2,
	BRCMS_SROM_AG3,
	BRCMS_SROM_ANTSWCTL2G,
	BRCMS_SROM_ANTSWCTL5G,
	BRCMS_SROM_ANTSWITCH,
	BRCMS_SROM_BOARDFLAGS2,
	BRCMS_SROM_BOARDFLAGS,
	BRCMS_SROM_BOARDNUM,
	BRCMS_SROM_BOARDREV,
	BRCMS_SROM_BOARDTYPE,
	BRCMS_SROM_BW40PO,
	BRCMS_SROM_BWDUPPO,
	BRCMS_SROM_BXA2G,
	BRCMS_SROM_BXA5G,
	BRCMS_SROM_CC,
	BRCMS_SROM_CCK2GPO,
	BRCMS_SROM_CCKBW202GPO,
	BRCMS_SROM_CCKBW20UL2GPO,
	BRCMS_SROM_CCODE,
	BRCMS_SROM_CDDPO,
	BRCMS_SROM_DEVID,
	BRCMS_SROM_ET1MACADDR,
	BRCMS_SROM_EXTPAGAIN2G,
	BRCMS_SROM_EXTPAGAIN5G,
	BRCMS_SROM_FREQOFFSET_CORR,
	BRCMS_SROM_HW_IQCAL_EN,
	BRCMS_SROM_IL0MACADDR,
	BRCMS_SROM_IQCAL_SWP_DIS,
	BRCMS_SROM_LEDBH0,
	BRCMS_SROM_LEDBH1,
	BRCMS_SROM_LEDBH2,
	BRCMS_SROM_LEDBH3,
	BRCMS_SROM_LEDDC,
	BRCMS_SROM_LEGOFDM40DUPPO,
	BRCMS_SROM_LEGOFDMBW202GPO,
	BRCMS_SROM_LEGOFDMBW205GHPO,
	BRCMS_SROM_LEGOFDMBW205GLPO,
	BRCMS_SROM_LEGOFDMBW205GMPO,
	BRCMS_SROM_LEGOFDMBW20UL2GPO,
	BRCMS_SROM_LEGOFDMBW20UL5GHPO,
	BRCMS_SROM_LEGOFDMBW20UL5GLPO,
	BRCMS_SROM_LEGOFDMBW20UL5GMPO,
	BRCMS_SROM_MACADDR,
	BRCMS_SROM_MCS2GPO0,
	BRCMS_SROM_MCS2GPO1,
	BRCMS_SROM_MCS2GPO2,
	BRCMS_SROM_MCS2GPO3,
	BRCMS_SROM_MCS2GPO4,
	BRCMS_SROM_MCS2GPO5,
	BRCMS_SROM_MCS2GPO6,
	BRCMS_SROM_MCS2GPO7,
	BRCMS_SROM_MCS32PO,
	BRCMS_SROM_MCS5GHPO0,
	BRCMS_SROM_MCS5GHPO1,
	BRCMS_SROM_MCS5GHPO2,
	BRCMS_SROM_MCS5GHPO3,
	BRCMS_SROM_MCS5GHPO4,
	BRCMS_SROM_MCS5GHPO5,
	BRCMS_SROM_MCS5GHPO6,
	BRCMS_SROM_MCS5GHPO7,
	BRCMS_SROM_MCS5GLPO0,
	BRCMS_SROM_MCS5GLPO1,
	BRCMS_SROM_MCS5GLPO2,
	BRCMS_SROM_MCS5GLPO3,
	BRCMS_SROM_MCS5GLPO4,
	BRCMS_SROM_MCS5GLPO5,
	BRCMS_SROM_MCS5GLPO6,
	BRCMS_SROM_MCS5GLPO7,
	BRCMS_SROM_MCS5GPO0,
	BRCMS_SROM_MCS5GPO1,
	BRCMS_SROM_MCS5GPO2,
	BRCMS_SROM_MCS5GPO3,
	BRCMS_SROM_MCS5GPO4,
	BRCMS_SROM_MCS5GPO5,
	BRCMS_SROM_MCS5GPO6,
	BRCMS_SROM_MCS5GPO7,
	BRCMS_SROM_MCSBW202GPO,
	BRCMS_SROM_MCSBW205GHPO,
	BRCMS_SROM_MCSBW205GLPO,
	BRCMS_SROM_MCSBW205GMPO,
	BRCMS_SROM_MCSBW20UL2GPO,
	BRCMS_SROM_MCSBW20UL5GHPO,
	BRCMS_SROM_MCSBW20UL5GLPO,
	BRCMS_SROM_MCSBW20UL5GMPO,
	BRCMS_SROM_MCSBW402GPO,
	BRCMS_SROM_MCSBW405GHPO,
	BRCMS_SROM_MCSBW405GLPO,
	BRCMS_SROM_MCSBW405GMPO,
	BRCMS_SROM_MEASPOWER,
	BRCMS_SROM_OFDM2GPO,
	BRCMS_SROM_OFDM5GHPO,
	BRCMS_SROM_OFDM5GLPO,
	BRCMS_SROM_OFDM5GPO,
	BRCMS_SROM_OPO,
	BRCMS_SROM_PA0B0,
	BRCMS_SROM_PA0B1,
	BRCMS_SROM_PA0B2,
	BRCMS_SROM_PA0ITSSIT,
	BRCMS_SROM_PA0MAXPWR,
	BRCMS_SROM_PA1B0,
	BRCMS_SROM_PA1B1,
	BRCMS_SROM_PA1B2,
	BRCMS_SROM_PA1HIB0,
	BRCMS_SROM_PA1HIB1,
	BRCMS_SROM_PA1HIB2,
	BRCMS_SROM_PA1HIMAXPWR,
	BRCMS_SROM_PA1ITSSIT,
	BRCMS_SROM_PA1LOB0,
	BRCMS_SROM_PA1LOB1,
	BRCMS_SROM_PA1LOB2,
	BRCMS_SROM_PA1LOMAXPWR,
	BRCMS_SROM_PA1MAXPWR,
	BRCMS_SROM_PDETRANGE2G,
	BRCMS_SROM_PDETRANGE5G,
	BRCMS_SROM_PHYCAL_TEMPDELTA,
	BRCMS_SROM_RAWTEMPSENSE,
	BRCMS_SROM_REGREV,
	BRCMS_SROM_REV,
	BRCMS_SROM_RSSISAV2G,
	BRCMS_SROM_RSSISAV5G,
	BRCMS_SROM_RSSISMC2G,
	BRCMS_SROM_RSSISMC5G,
	BRCMS_SROM_RSSISMF2G,
	BRCMS_SROM_RSSISMF5G,
	BRCMS_SROM_RXCHAIN,
	BRCMS_SROM_RXPO2G,
	BRCMS_SROM_RXPO5G,
	BRCMS_SROM_STBCPO,
	BRCMS_SROM_TEMPCORRX,
	BRCMS_SROM_TEMPOFFSET,
	BRCMS_SROM_TEMPSENSE_OPTION,
	BRCMS_SROM_TEMPSENSE_SLOPE,
	BRCMS_SROM_TEMPTHRESH,
	BRCMS_SROM_TRI2G,
	BRCMS_SROM_TRI5GH,
	BRCMS_SROM_TRI5GL,
	BRCMS_SROM_TRI5G,
	BRCMS_SROM_TRISO2G,
	BRCMS_SROM_TRISO5G,
	BRCMS_SROM_TSSIPOS2G,
	BRCMS_SROM_TSSIPOS5G,
	BRCMS_SROM_TXCHAIN,
	BRCMS_SROM_ITT2GA0,
	BRCMS_SROM_ITT2GA1,
	BRCMS_SROM_ITT2GA2,
	BRCMS_SROM_ITT2GA3,
	BRCMS_SROM_ITT5GA0,
	BRCMS_SROM_ITT5GA1,
	BRCMS_SROM_ITT5GA2,
	BRCMS_SROM_ITT5GA3,
	BRCMS_SROM_MAXP2GA0,
	BRCMS_SROM_MAXP2GA1,
	BRCMS_SROM_MAXP2GA2,
	BRCMS_SROM_MAXP2GA3,
	BRCMS_SROM_MAXP5GA0,
	BRCMS_SROM_MAXP5GA1,
	BRCMS_SROM_MAXP5GA2,
	BRCMS_SROM_MAXP5GA3,
	BRCMS_SROM_MAXP5GHA0,
	BRCMS_SROM_MAXP5GHA1,
	BRCMS_SROM_MAXP5GHA2,
	BRCMS_SROM_MAXP5GHA3,
	BRCMS_SROM_MAXP5GLA0,
	BRCMS_SROM_MAXP5GLA1,
	BRCMS_SROM_MAXP5GLA2,
	BRCMS_SROM_MAXP5GLA3,
	BRCMS_SROM_PA2GW0A0,
	BRCMS_SROM_PA2GW0A1,
	BRCMS_SROM_PA2GW0A2,
	BRCMS_SROM_PA2GW0A3,
	BRCMS_SROM_PA2GW1A0,
	BRCMS_SROM_PA2GW1A1,
	BRCMS_SROM_PA2GW1A2,
	BRCMS_SROM_PA2GW1A3,
	BRCMS_SROM_PA2GW2A0,
	BRCMS_SROM_PA2GW2A1,
	BRCMS_SROM_PA2GW2A2,
	BRCMS_SROM_PA2GW2A3,
	BRCMS_SROM_PA5GHW0A0,
	BRCMS_SROM_PA5GHW0A1,
	BRCMS_SROM_PA5GHW0A2,
	BRCMS_SROM_PA5GHW0A3,
	BRCMS_SROM_PA5GHW1A0,
	BRCMS_SROM_PA5GHW1A1,
	BRCMS_SROM_PA5GHW1A2,
	BRCMS_SROM_PA5GHW1A3,
	BRCMS_SROM_PA5GHW2A0,
	BRCMS_SROM_PA5GHW2A1,
	BRCMS_SROM_PA5GHW2A2,
	BRCMS_SROM_PA5GHW2A3,
	BRCMS_SROM_PA5GLW0A0,
	BRCMS_SROM_PA5GLW0A1,
	BRCMS_SROM_PA5GLW0A2,
	BRCMS_SROM_PA5GLW0A3,
	BRCMS_SROM_PA5GLW1A0,
	BRCMS_SROM_PA5GLW1A1,
	BRCMS_SROM_PA5GLW1A2,
	BRCMS_SROM_PA5GLW1A3,
	BRCMS_SROM_PA5GLW2A0,
	BRCMS_SROM_PA5GLW2A1,
	BRCMS_SROM_PA5GLW2A2,
	BRCMS_SROM_PA5GLW2A3,
	BRCMS_SROM_PA5GW0A0,
	BRCMS_SROM_PA5GW0A1,
	BRCMS_SROM_PA5GW0A2,
	BRCMS_SROM_PA5GW0A3,
	BRCMS_SROM_PA5GW1A0,
	BRCMS_SROM_PA5GW1A1,
	BRCMS_SROM_PA5GW1A2,
	BRCMS_SROM_PA5GW1A3,
	BRCMS_SROM_PA5GW2A0,
	BRCMS_SROM_PA5GW2A1,
	BRCMS_SROM_PA5GW2A2,
	BRCMS_SROM_PA5GW2A3,
};

#define	BRCMS_NUMRATES	16	

#define	PHY_TYPE_A	0	
#define	PHY_TYPE_G	2	
#define	PHY_TYPE_N	4	
#define	PHY_TYPE_LP	5	
#define	PHY_TYPE_SSN	6	
#define	PHY_TYPE_LCN	8	
#define	PHY_TYPE_LCNXN	9	
#define	PHY_TYPE_HT	7	

#define BRCMS_10_MHZ	10	
#define BRCMS_20_MHZ	20	
#define BRCMS_40_MHZ	40	

#define	BRCMS_RSSI_MINVAL	-200	
#define	BRCMS_RSSI_NO_SIGNAL	-91	
#define	BRCMS_RSSI_VERY_LOW	-80	
#define	BRCMS_RSSI_LOW		-70	
#define	BRCMS_RSSI_GOOD		-68	
#define	BRCMS_RSSI_VERY_GOOD	-58	
#define	BRCMS_RSSI_EXCELLENT	-57	

#define BRCMS_TXPWR_MAX		(127)	

#define	BRCMS_RATE_FLAG	0x80	
#define	BRCMS_RATE_MASK	0x7f	

#define	ANT_RX_DIV_FORCE_0	0	
#define	ANT_RX_DIV_FORCE_1	1	
#define	ANT_RX_DIV_START_1	2	
#define	ANT_RX_DIV_START_0	3	
#define	ANT_RX_DIV_ENABLE	3	
#define ANT_RX_DIV_DEF		ANT_RX_DIV_START_0

#define ANT_TX_FORCE_0		0
#define ANT_TX_FORCE_1		1
#define ANT_TX_LAST_RX		3
#define ANT_TX_DEF		3

#define TXCHAIN_DEF		0x1
#define TXCHAIN_DEF_NPHY	0x3
#define TXCHAIN_DEF_HTPHY	0x7
#define RXCHAIN_DEF		0x1
#define RXCHAIN_DEF_NPHY	0x3
#define RXCHAIN_DEF_HTPHY	0x7
#define ANTSWITCH_NONE		0
#define ANTSWITCH_TYPE_1	1
#define ANTSWITCH_TYPE_2	2
#define ANTSWITCH_TYPE_3	3

#define RXBUFSZ		PKTBUFSZ

#define MAX_STREAMS_SUPPORTED	4	

struct brcm_rateset {
	
	u32 count;
	
	u8 rates[WL_NUMRATES];
};

struct brcms_c_rateset {
	uint count;		
	 
	u8 rates[BRCMS_NUMRATES];
	u8 htphy_membership;	
	u8 mcs[MCSSET_LEN];	
};

#define AMPDU_DEF_MPDU_DENSITY	6	

struct brcms_bss_info {
	u8 BSSID[ETH_ALEN];	
	u16 flags;		
	u8 SSID_len;		
	u8 SSID[32];		
	s16 RSSI;		
	s16 SNR;		
	u16 beacon_period;	
	u16 chanspec;	
	struct brcms_c_rateset rateset;	
};

#define MAC80211_PROMISC_BCNS	(1 << 0)
#define MAC80211_SCAN		(1 << 1)

struct brcms_pub {
	struct brcms_c_info *wlc;
	struct ieee80211_hw *ieee_hw;
	struct scb_ampdu *global_ampdu;
	uint mac80211_state;
	uint unit;		
	uint corerev;		
	struct si_pub *sih;	
	bool up;		
	bool hw_off;		
	bool hw_up;		
	bool _piomode;		
	uint _nbands;		
	uint now;		

	bool delayed_down;	
	bool associated;	
	
	bool _ampdu;		
	u8 _n_enab;		

	u8 cur_etheraddr[ETH_ALEN];	

	int bcmerror;		

	u32 radio_disabled;	

	u16 boardrev;	
	u8 sromrev;		
	char srom_ccode[BRCM_CNTRY_BUF_SZ];	
	u32 boardflags;	
	u32 boardflags2;	
	bool phy_11ncapable;	

	struct wl_cnt *_cnt;	
};

enum wlc_par_id {
	IOV_MPC = 1,
	IOV_RTSTHRESH,
	IOV_QTXPOWER,
	IOV_BCN_LI_BCN		
};


#define ENAB_1x1	0x01
#define ENAB_2x2	0x02
#define ENAB_3x3	0x04
#define ENAB_4x4	0x08
#define SUPPORT_11N	(ENAB_1x1|ENAB_2x2)
#define SUPPORT_HT	(ENAB_1x1|ENAB_2x2|ENAB_3x3)

#define AMPDU_AGG_HOST	1

extern const u8 wlc_prio2prec_map[];
#define BRCMS_PRIO_TO_PREC(pri)	wlc_prio2prec_map[(pri) & 7]

#define	BRCMS_PREC_COUNT	16	

#define BRCMS_PREC_BMP_ALL		MAXBITVAL(BRCMS_PREC_COUNT)

#define BRCMS_PRIO_TO_HI_PREC(pri)	min(BRCMS_PRIO_TO_PREC(pri) + 1,\
					    BRCMS_PREC_COUNT - 1)

#define BRCMS_PREC_BMP_AC_BE	(NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_BE)) | \
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_BE)) |	\
			NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_EE)) |	\
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_EE)))
#define BRCMS_PREC_BMP_AC_BK	(NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_BK)) | \
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_BK)) |	\
			NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_NONE)) |	\
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_NONE)))
#define BRCMS_PREC_BMP_AC_VI	(NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_CL)) | \
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_CL)) |	\
			NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_VI)) |	\
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_VI)))
#define BRCMS_PREC_BMP_AC_VO	(NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_VO)) | \
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_VO)) |	\
			NBITVAL(BRCMS_PRIO_TO_PREC(PRIO_8021D_NC)) |	\
			NBITVAL(BRCMS_PRIO_TO_HI_PREC(PRIO_8021D_NC)))

#define	BRCMS_PROT_G_SPEC		1	
#define	BRCMS_PROT_G_OVR		2	
#define	BRCMS_PROT_G_USER		3	
#define	BRCMS_PROT_OVERLAP	4	
#define	BRCMS_PROT_N_USER		10	
#define	BRCMS_PROT_N_CFG		11	
#define	BRCMS_PROT_N_CFG_OVR	12	
#define	BRCMS_PROT_N_NONGF	13	
#define	BRCMS_PROT_N_NONGF_OVR	14	
#define	BRCMS_PROT_N_PAM_OVR	15	
#define	BRCMS_PROT_N_OBSS		16	

#define GMODE_LEGACY_B		0
#define GMODE_AUTO		1
#define GMODE_ONLY		2
#define GMODE_B_DEFERRED	3
#define GMODE_PERFORMANCE	4
#define GMODE_LRS		5
#define GMODE_MAX		6

#define HIGHEST_SINGLE_STREAM_MCS	7

#define	MAXBANDS		2	

#define ANT_SELCFG_MAX		4

struct brcms_antselcfg {
	u8 ant_config[ANT_SELCFG_MAX];	
	u8 num_antcfg;	
};

extern struct brcms_c_info *
brcms_c_attach(struct brcms_info *wl, struct bcma_device *core, uint unit,
	       bool piomode, uint *perr);
extern uint brcms_c_detach(struct brcms_c_info *wlc);
extern int brcms_c_up(struct brcms_c_info *wlc);
extern uint brcms_c_down(struct brcms_c_info *wlc);

extern bool brcms_c_chipmatch(u16 vendor, u16 device);
extern void brcms_c_init(struct brcms_c_info *wlc, bool mute_tx);
extern void brcms_c_reset(struct brcms_c_info *wlc);

extern void brcms_c_intrson(struct brcms_c_info *wlc);
extern u32 brcms_c_intrsoff(struct brcms_c_info *wlc);
extern void brcms_c_intrsrestore(struct brcms_c_info *wlc, u32 macintmask);
extern bool brcms_c_intrsupd(struct brcms_c_info *wlc);
extern bool brcms_c_isr(struct brcms_c_info *wlc, bool *wantdpc);
extern bool brcms_c_dpc(struct brcms_c_info *wlc, bool bounded);
extern void brcms_c_sendpkt_mac80211(struct brcms_c_info *wlc,
				     struct sk_buff *sdu,
				     struct ieee80211_hw *hw);
extern bool brcms_c_aggregatable(struct brcms_c_info *wlc, u8 tid);
extern void brcms_c_protection_upd(struct brcms_c_info *wlc, uint idx,
				   int val);
extern int brcms_c_get_header_len(void);
extern void brcms_c_set_addrmatch(struct brcms_c_info *wlc,
				  int match_reg_offset,
				  const u8 *addr);
extern void brcms_c_wme_setparams(struct brcms_c_info *wlc, u16 aci,
			      const struct ieee80211_tx_queue_params *arg,
			      bool suspend);
extern struct brcms_pub *brcms_c_pub(struct brcms_c_info *wlc);
extern void brcms_c_ampdu_flush(struct brcms_c_info *wlc,
			    struct ieee80211_sta *sta, u16 tid);
extern void brcms_c_ampdu_tx_operational(struct brcms_c_info *wlc, u8 tid,
					 u8 ba_wsize, uint max_rx_ampdu_bytes);
extern char *getvar(struct si_pub *sih, enum brcms_srom_id id);
extern int getintvar(struct si_pub *sih, enum brcms_srom_id id);
extern int brcms_c_module_register(struct brcms_pub *pub,
				   const char *name, struct brcms_info *hdl,
				   int (*down_fn)(void *handle));
extern int brcms_c_module_unregister(struct brcms_pub *pub, const char *name,
				     struct brcms_info *hdl);
extern void brcms_c_suspend_mac_and_wait(struct brcms_c_info *wlc);
extern void brcms_c_enable_mac(struct brcms_c_info *wlc);
extern void brcms_c_associate_upd(struct brcms_c_info *wlc, bool state);
extern void brcms_c_scan_start(struct brcms_c_info *wlc);
extern void brcms_c_scan_stop(struct brcms_c_info *wlc);
extern int brcms_c_get_curband(struct brcms_c_info *wlc);
extern void brcms_c_wait_for_tx_completion(struct brcms_c_info *wlc,
					   bool drop);
extern int brcms_c_set_channel(struct brcms_c_info *wlc, u16 channel);
extern int brcms_c_set_rate_limit(struct brcms_c_info *wlc, u16 srl, u16 lrl);
extern void brcms_c_get_current_rateset(struct brcms_c_info *wlc,
				 struct brcm_rateset *currs);
extern int brcms_c_set_rateset(struct brcms_c_info *wlc,
					struct brcm_rateset *rs);
extern int brcms_c_set_beacon_period(struct brcms_c_info *wlc, u16 period);
extern u16 brcms_c_get_phy_type(struct brcms_c_info *wlc, int phyidx);
extern void brcms_c_set_shortslot_override(struct brcms_c_info *wlc,
				    s8 sslot_override);
extern void brcms_c_set_beacon_listen_interval(struct brcms_c_info *wlc,
					u8 interval);
extern int brcms_c_set_tx_power(struct brcms_c_info *wlc, int txpwr);
extern int brcms_c_get_tx_power(struct brcms_c_info *wlc);
extern bool brcms_c_check_radio_disabled(struct brcms_c_info *wlc);
extern void brcms_c_mute(struct brcms_c_info *wlc, bool on);

#endif				
