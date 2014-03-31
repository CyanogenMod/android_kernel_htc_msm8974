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
#include <net/mac80211.h>

#include "rate.h"
#include "scb.h"
#include "phy/phy_hal.h"
#include "antsel.h"
#include "main.h"
#include "ampdu.h"

#define AMPDU_MAX_MPDU			32
#define AMPDU_NUM_MPDU_LEGACY		16
#define AMPDU_TX_BA_MAX_WSIZE		64
#define AMPDU_TX_BA_DEF_WSIZE		64
#define AMPDU_RX_BA_DEF_WSIZE		64
#define AMPDU_RX_BA_MAX_WSIZE		64
#define	AMPDU_MAX_DUR			5
#define AMPDU_DEF_RETRY_LIMIT		5
#define AMPDU_DEF_RR_RETRY_LIMIT	2
#define AMPDU_DEF_TXPKT_WEIGHT		2
#define AMPDU_DEF_FFPLD_RSVD		2048
#define AMPDU_INI_FREE			10
#define	AMPDU_SCB_MAX_RELEASE		20

#define NUM_FFPLD_FIFO 4	
#define FFPLD_TX_MAX_UNFL   200	
#define FFPLD_MPDU_SIZE 1800	
#define FFPLD_MAX_MCS 23	
#define FFPLD_PLD_INCR 1000	
#define FFPLD_MAX_AMPDU_CNT 5000	

#define AMPDU_DELIMITER_LEN	4

#define AMPDU_NUM_MPDU		16

#define TX_SEQ_TO_INDEX(seq) ((seq) % AMPDU_TX_BA_MAX_WSIZE)

#define AMPDU_MAX_MPDU_OVERHEAD (FCS_LEN + DOT11_ICV_AES_LEN +\
	AMPDU_DELIMITER_LEN + 3\
	+ DOT11_A4_HDR_LEN + DOT11_QOS_LEN + DOT11_IV_MAX_LEN)

#define MODADD_POW2(x, y, bound) (((x) + (y)) & ((bound) - 1))
#define MODSUB_POW2(x, y, bound) (((x) - (y)) & ((bound) - 1))

struct brcms_fifo_info {
	u16 ampdu_pld_size;
	u8 mcs2ampdu_table[FFPLD_MAX_MCS + 1];
	u16 prev_txfunfl;
	u32 accum_txfunfl;
	u32 accum_txampdu;
	u32 prev_txampdu;
	u32 dmaxferrate;
};

struct ampdu_info {
	struct brcms_c_info *wlc;
	int scb_handle;
	u8 ini_enable[AMPDU_MAX_SCB_TID];
	u8 ba_tx_wsize;
	u8 ba_rx_wsize;
	u8 retry_limit;
	u8 rr_retry_limit;
	u8 retry_limit_tid[AMPDU_MAX_SCB_TID];
	u8 rr_retry_limit_tid[AMPDU_MAX_SCB_TID];
	u8 mpdu_density;
	s8 max_pdu;
	u8 dur;
	u8 txpkt_weight;
	u8 rx_factor;
	u32 ffpld_rsvd;
	u32 max_txlen[MCS_TABLE_SIZE][2][2];
	bool mfbr;
	u32 tx_max_funl;
	struct brcms_fifo_info fifo_tb[NUM_FFPLD_FIFO];
};

struct cb_del_ampdu_pars {
	struct ieee80211_sta *sta;
	u16 tid;
};

static void brcms_c_scb_ampdu_update_max_txlen(struct ampdu_info *ampdu, u8 dur)
{
	u32 rate, mcs;

	for (mcs = 0; mcs < MCS_TABLE_SIZE; mcs++) {
		
		
		rate = mcs_2_rate(mcs, false, false);
		ampdu->max_txlen[mcs][0][0] = (rate * dur) >> 3;
		
		rate = mcs_2_rate(mcs, true, false);
		ampdu->max_txlen[mcs][1][0] = (rate * dur) >> 3;
		
		rate = mcs_2_rate(mcs, false, true);
		ampdu->max_txlen[mcs][0][1] = (rate * dur) >> 3;
		
		rate = mcs_2_rate(mcs, true, true);
		ampdu->max_txlen[mcs][1][1] = (rate * dur) >> 3;
	}
}

static bool brcms_c_ampdu_cap(struct ampdu_info *ampdu)
{
	if (BRCMS_PHY_11N_CAP(ampdu->wlc->band))
		return true;
	else
		return false;
}

static int brcms_c_ampdu_set(struct ampdu_info *ampdu, bool on)
{
	struct brcms_c_info *wlc = ampdu->wlc;

	wlc->pub->_ampdu = false;

	if (on) {
		if (!(wlc->pub->_n_enab & SUPPORT_11N)) {
			wiphy_err(ampdu->wlc->wiphy, "wl%d: driver not "
				"nmode enabled\n", wlc->pub->unit);
			return -ENOTSUPP;
		}
		if (!brcms_c_ampdu_cap(ampdu)) {
			wiphy_err(ampdu->wlc->wiphy, "wl%d: device not "
				"ampdu capable\n", wlc->pub->unit);
			return -ENOTSUPP;
		}
		wlc->pub->_ampdu = on;
	}

	return 0;
}

static void brcms_c_ffpld_init(struct ampdu_info *ampdu)
{
	int i, j;
	struct brcms_fifo_info *fifo;

	for (j = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = (ampdu->fifo_tb + j);
		fifo->ampdu_pld_size = 0;
		for (i = 0; i <= FFPLD_MAX_MCS; i++)
			fifo->mcs2ampdu_table[i] = 255;
		fifo->dmaxferrate = 0;
		fifo->accum_txampdu = 0;
		fifo->prev_txfunfl = 0;
		fifo->accum_txfunfl = 0;

	}
}

struct ampdu_info *brcms_c_ampdu_attach(struct brcms_c_info *wlc)
{
	struct ampdu_info *ampdu;
	int i;

	ampdu = kzalloc(sizeof(struct ampdu_info), GFP_ATOMIC);
	if (!ampdu)
		return NULL;

	ampdu->wlc = wlc;

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++)
		ampdu->ini_enable[i] = true;
	
	ampdu->ini_enable[PRIO_8021D_VO] = false;
	ampdu->ini_enable[PRIO_8021D_NC] = false;

	
	ampdu->ini_enable[PRIO_8021D_NONE] = false;
	ampdu->ini_enable[PRIO_8021D_BK] = false;

	ampdu->ba_tx_wsize = AMPDU_TX_BA_DEF_WSIZE;
	ampdu->ba_rx_wsize = AMPDU_RX_BA_DEF_WSIZE;
	ampdu->mpdu_density = AMPDU_DEF_MPDU_DENSITY;
	ampdu->max_pdu = AUTO;
	ampdu->dur = AMPDU_MAX_DUR;
	ampdu->txpkt_weight = AMPDU_DEF_TXPKT_WEIGHT;

	ampdu->ffpld_rsvd = AMPDU_DEF_FFPLD_RSVD;
	if (BRCMS_ISNPHY(wlc->band) && NREV_LT(wlc->band->phyrev, 2))
		ampdu->rx_factor = IEEE80211_HT_MAX_AMPDU_32K;
	else
		ampdu->rx_factor = IEEE80211_HT_MAX_AMPDU_64K;
	ampdu->retry_limit = AMPDU_DEF_RETRY_LIMIT;
	ampdu->rr_retry_limit = AMPDU_DEF_RR_RETRY_LIMIT;

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		ampdu->retry_limit_tid[i] = ampdu->retry_limit;
		ampdu->rr_retry_limit_tid[i] = ampdu->rr_retry_limit;
	}

	brcms_c_scb_ampdu_update_max_txlen(ampdu, ampdu->dur);
	ampdu->mfbr = false;
	
	brcms_c_ampdu_set(ampdu, wlc->pub->_ampdu);

	ampdu->tx_max_funl = FFPLD_TX_MAX_UNFL;
	brcms_c_ffpld_init(ampdu);

	return ampdu;
}

void brcms_c_ampdu_detach(struct ampdu_info *ampdu)
{
	kfree(ampdu);
}

static void brcms_c_scb_ampdu_update_config(struct ampdu_info *ampdu,
					    struct scb *scb)
{
	struct scb_ampdu *scb_ampdu = &scb->scb_ampdu;
	int i;

	scb_ampdu->max_pdu = AMPDU_NUM_MPDU;

	
	for (i = 0; i < NUM_FFPLD_FIFO; i++) {
		if (ampdu->fifo_tb[i].ampdu_pld_size > FFPLD_PLD_INCR)
			scb_ampdu->max_pdu = AMPDU_NUM_MPDU_LEGACY;
	}

	
	if (ampdu->max_pdu != AUTO)
		scb_ampdu->max_pdu = (u8) ampdu->max_pdu;

	scb_ampdu->release = min_t(u8, scb_ampdu->max_pdu,
				   AMPDU_SCB_MAX_RELEASE);

	if (scb_ampdu->max_rx_ampdu_bytes)
		scb_ampdu->release = min_t(u8, scb_ampdu->release,
			scb_ampdu->max_rx_ampdu_bytes / 1600);

	scb_ampdu->release = min(scb_ampdu->release,
				 ampdu->fifo_tb[TX_AC_BE_FIFO].
				 mcs2ampdu_table[FFPLD_MAX_MCS]);
}

static void brcms_c_scb_ampdu_update_config_all(struct ampdu_info *ampdu)
{
	brcms_c_scb_ampdu_update_config(ampdu, &ampdu->wlc->pri_scb);
}

static void brcms_c_ffpld_calc_mcs2ampdu_table(struct ampdu_info *ampdu, int f)
{
	int i;
	u32 phy_rate, dma_rate, tmp;
	u8 max_mpdu;
	struct brcms_fifo_info *fifo = (ampdu->fifo_tb + f);

	
	
	max_mpdu = min_t(u8, fifo->mcs2ampdu_table[FFPLD_MAX_MCS],
			 AMPDU_NUM_MPDU_LEGACY);
	phy_rate = mcs_2_rate(FFPLD_MAX_MCS, true, false);
	dma_rate =
	    (((phy_rate / 100) *
	      (max_mpdu * FFPLD_MPDU_SIZE - fifo->ampdu_pld_size))
	     / (max_mpdu * FFPLD_MPDU_SIZE)) * 100;
	fifo->dmaxferrate = dma_rate;

	
	dma_rate = dma_rate >> 7;
	for (i = 0; i < FFPLD_MAX_MCS; i++) {
		
		phy_rate = mcs_2_rate(i, true, false) >> 7;
		if (phy_rate > dma_rate) {
			tmp = ((fifo->ampdu_pld_size * phy_rate) /
			       ((phy_rate - dma_rate) * FFPLD_MPDU_SIZE)) + 1;
			tmp = min_t(u32, tmp, 255);
			fifo->mcs2ampdu_table[i] = (u8) tmp;
		}
	}
}

static int brcms_c_ffpld_check_txfunfl(struct brcms_c_info *wlc, int fid)
{
	struct ampdu_info *ampdu = wlc->ampdu;
	u32 phy_rate = mcs_2_rate(FFPLD_MAX_MCS, true, false);
	u32 txunfl_ratio;
	u8 max_mpdu;
	u32 current_ampdu_cnt = 0;
	u16 max_pld_size;
	u32 new_txunfl;
	struct brcms_fifo_info *fifo = (ampdu->fifo_tb + fid);
	uint xmtfifo_sz;
	u16 cur_txunfl;

	
	cur_txunfl = brcms_b_read_shm(wlc->hw,
				      M_UCODE_MACSTAT +
				      offsetof(struct macstat, txfunfl[fid]));
	new_txunfl = (u16) (cur_txunfl - fifo->prev_txfunfl);
	if (new_txunfl == 0) {
		BCMMSG(wlc->wiphy, "TX status FRAG set but no tx underflows\n");
		return -1;
	}
	fifo->prev_txfunfl = cur_txunfl;

	if (!ampdu->tx_max_funl)
		return 1;

	
	if (brcms_b_xmtfifo_sz_get(wlc->hw, fid, &xmtfifo_sz))
		return -1;

	if ((TXFIFO_SIZE_UNIT * (u32) xmtfifo_sz) <= ampdu->ffpld_rsvd)
		return 1;

	max_pld_size = TXFIFO_SIZE_UNIT * xmtfifo_sz - ampdu->ffpld_rsvd;
	fifo->accum_txfunfl += new_txunfl;

	
	if (fifo->accum_txfunfl < 10)
		return 0;

	BCMMSG(wlc->wiphy, "ampdu_count %d  tx_underflows %d\n",
		current_ampdu_cnt, fifo->accum_txfunfl);


	txunfl_ratio = current_ampdu_cnt / fifo->accum_txfunfl;

	if (txunfl_ratio > ampdu->tx_max_funl) {
		if (current_ampdu_cnt >= FFPLD_MAX_AMPDU_CNT)
			fifo->accum_txfunfl = 0;

		return 0;
	}
	max_mpdu = min_t(u8, fifo->mcs2ampdu_table[FFPLD_MAX_MCS],
			 AMPDU_NUM_MPDU_LEGACY);


	if (fifo->ampdu_pld_size >= max_mpdu * FFPLD_MPDU_SIZE) {
		fifo->accum_txfunfl = 0;
		return 0;
	}

	if (fifo->ampdu_pld_size < max_pld_size) {

		
		fifo->ampdu_pld_size += FFPLD_PLD_INCR;
		if (fifo->ampdu_pld_size > max_pld_size)
			fifo->ampdu_pld_size = max_pld_size;

		
		brcms_c_scb_ampdu_update_config_all(ampdu);

		fifo->dmaxferrate =
		    (((phy_rate / 100) *
		      (max_mpdu * FFPLD_MPDU_SIZE - fifo->ampdu_pld_size))
		     / (max_mpdu * FFPLD_MPDU_SIZE)) * 100;

		BCMMSG(wlc->wiphy, "DMA estimated transfer rate %d; "
			"pre-load size %d\n",
			fifo->dmaxferrate, fifo->ampdu_pld_size);
	} else {

		
		if (fifo->mcs2ampdu_table[FFPLD_MAX_MCS] > 1) {
			if (fifo->mcs2ampdu_table[FFPLD_MAX_MCS] == 255)
				fifo->mcs2ampdu_table[FFPLD_MAX_MCS] =
				    AMPDU_NUM_MPDU_LEGACY - 1;
			else
				fifo->mcs2ampdu_table[FFPLD_MAX_MCS] -= 1;

			
			brcms_c_ffpld_calc_mcs2ampdu_table(ampdu, fid);

			
			brcms_c_scb_ampdu_update_config_all(ampdu);
		}
	}
	fifo->accum_txfunfl = 0;
	return 0;
}

void
brcms_c_ampdu_tx_operational(struct brcms_c_info *wlc, u8 tid,
	u8 ba_wsize,		
	uint max_rx_ampdu_bytes) 
{
	struct scb_ampdu *scb_ampdu;
	struct scb_ampdu_tid_ini *ini;
	struct ampdu_info *ampdu = wlc->ampdu;
	struct scb *scb = &wlc->pri_scb;
	scb_ampdu = &scb->scb_ampdu;

	if (!ampdu->ini_enable[tid]) {
		wiphy_err(ampdu->wlc->wiphy, "%s: Rejecting tid %d\n",
			  __func__, tid);
		return;
	}

	ini = &scb_ampdu->ini[tid];
	ini->tid = tid;
	ini->scb = scb_ampdu->scb;
	ini->ba_wsize = ba_wsize;
	scb_ampdu->max_rx_ampdu_bytes = max_rx_ampdu_bytes;
}

int
brcms_c_sendampdu(struct ampdu_info *ampdu, struct brcms_txq_info *qi,
	      struct sk_buff **pdu, int prec)
{
	struct brcms_c_info *wlc;
	struct sk_buff *p, *pkt[AMPDU_MAX_MPDU];
	u8 tid, ndelim;
	int err = 0;
	u8 preamble_type = BRCMS_GF_PREAMBLE;
	u8 fbr_preamble_type = BRCMS_GF_PREAMBLE;
	u8 rts_preamble_type = BRCMS_LONG_PREAMBLE;
	u8 rts_fbr_preamble_type = BRCMS_LONG_PREAMBLE;

	bool rr = true, fbr = false;
	uint i, count = 0, fifo, seg_cnt = 0;
	u16 plen, len, seq = 0, mcl, mch, index, frameid, dma_len = 0;
	u32 ampdu_len, max_ampdu_bytes = 0;
	struct d11txh *txh = NULL;
	u8 *plcp;
	struct ieee80211_hdr *h;
	struct scb *scb;
	struct scb_ampdu *scb_ampdu;
	struct scb_ampdu_tid_ini *ini;
	u8 mcs = 0;
	bool use_rts = false, use_cts = false;
	u32 rspec = 0, rspec_fallback = 0;
	u32 rts_rspec = 0, rts_rspec_fallback = 0;
	u16 mimo_ctlchbw = PHY_TXC1_BW_20MHZ;
	struct ieee80211_rts *rts;
	u8 rr_retry_limit;
	struct brcms_fifo_info *f;
	bool fbr_iscck;
	struct ieee80211_tx_info *tx_info;
	u16 qlen;
	struct wiphy *wiphy;

	wlc = ampdu->wlc;
	wiphy = wlc->wiphy;
	p = *pdu;

	tid = (u8) (p->priority);

	f = ampdu->fifo_tb + prio2fifo[tid];

	scb = &wlc->pri_scb;
	scb_ampdu = &scb->scb_ampdu;
	ini = &scb_ampdu->ini[tid];

	
	qlen = pktq_plen(&qi->q, prec);
	if (ini->tx_in_transit > 0 &&
	    qlen < min(scb_ampdu->max_pdu, ini->ba_wsize))
		
		return -EBUSY;

	
	rr_retry_limit = ampdu->rr_retry_limit_tid[tid];
	ampdu_len = 0;
	dma_len = 0;
	while (p) {
		struct ieee80211_tx_rate *txrate;

		tx_info = IEEE80211_SKB_CB(p);
		txrate = tx_info->status.rates;

		if (tx_info->flags & IEEE80211_TX_CTL_AMPDU) {
			err = brcms_c_prep_pdu(wlc, p, &fifo);
		} else {
			wiphy_err(wiphy, "%s: AMPDU flag is off!\n", __func__);
			*pdu = NULL;
			err = 0;
			break;
		}

		if (err) {
			if (err == -EBUSY) {
				wiphy_err(wiphy, "wl%d: sendampdu: "
					  "prep_xdu retry; seq 0x%x\n",
					  wlc->pub->unit, seq);
				*pdu = p;
				break;
			}

			
			wiphy_err(wiphy, "wl%d: sendampdu: prep_xdu "
				  "rejected; seq 0x%x\n", wlc->pub->unit, seq);
			*pdu = NULL;
			break;
		}

		
		txh = (struct d11txh *) p->data;
		plcp = (u8 *) (txh + 1);
		h = (struct ieee80211_hdr *)(plcp + D11_PHY_HDR_LEN);
		seq = le16_to_cpu(h->seq_ctrl) >> SEQNUM_SHIFT;
		index = TX_SEQ_TO_INDEX(seq);

		
		mcl = le16_to_cpu(txh->MacTxControlLow);
		mcl &= ~TXC_AMPDU_MASK;
		fbr_iscck = !(le16_to_cpu(txh->XtraFrameTypes) & 0x3);
		txh->PreloadSize = 0;	

		
		if (txrate[0].count <= rr_retry_limit) {
			txrate[0].count++;
			rr = true;
			fbr = false;
		} else {
			fbr = true;
			rr = false;
			txrate[1].count++;
		}

		
		len = fbr_iscck ? BRCMS_GET_CCK_PLCP_LEN(txh->FragPLCPFallback)
		    : BRCMS_GET_MIMO_PLCP_LEN(txh->FragPLCPFallback);

		
		ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
		seg_cnt += 1;

		BCMMSG(wlc->wiphy, "wl%d: mpdu %d plcp_len %d\n",
			wlc->pub->unit, count, len);

		if (count == 0) {
			mcl |= (TXC_AMPDU_FIRST << TXC_AMPDU_SHIFT);
			
			mcl |= TXC_STARTMSDU;
			rts = (struct ieee80211_rts *)&txh->rts_frame;

			if (ieee80211_is_rts(rts->frame_control)) {
				mcl |= TXC_SENDRTS;
				use_rts = true;
			}
			if (ieee80211_is_cts(rts->frame_control)) {
				mcl |= TXC_SENDCTS;
				use_cts = true;
			}
		} else {
			mcl |= (TXC_AMPDU_MIDDLE << TXC_AMPDU_SHIFT);
			mcl &= ~(TXC_STARTMSDU | TXC_SENDRTS | TXC_SENDCTS);
		}

		len = roundup(len, 4);
		ampdu_len += (len + (ndelim + 1) * AMPDU_DELIMITER_LEN);

		dma_len += (u16) p->len;

		BCMMSG(wlc->wiphy, "wl%d: ampdu_len %d"
			" seg_cnt %d null delim %d\n",
			wlc->pub->unit, ampdu_len, seg_cnt, ndelim);

		txh->MacTxControlLow = cpu_to_le16(mcl);

		
		pkt[count++] = p;

		
		if (count == 1) {
			u8 plcp0, plcp3, is40, sgi;
			struct ieee80211_sta *sta;

			sta = tx_info->control.sta;

			if (rr) {
				plcp0 = plcp[0];
				plcp3 = plcp[3];
			} else {
				plcp0 = txh->FragPLCPFallback[0];
				plcp3 = txh->FragPLCPFallback[3];

			}
			is40 = (plcp0 & MIMO_PLCP_40MHZ) ? 1 : 0;
			sgi = plcp3_issgi(plcp3) ? 1 : 0;
			mcs = plcp0 & ~MIMO_PLCP_40MHZ;
			max_ampdu_bytes =
			    min(scb_ampdu->max_rx_ampdu_bytes,
				ampdu->max_txlen[mcs][is40][sgi]);

			if (is40)
				mimo_ctlchbw =
				   CHSPEC_SB_UPPER(wlc_phy_chanspec_get(
								 wlc->band->pi))
				   ? PHY_TXC1_BW_20MHZ_UP : PHY_TXC1_BW_20MHZ;

			
			rspec = RSPEC_MIMORATE;
			rspec |= plcp[0] & ~MIMO_PLCP_40MHZ;
			if (plcp[0] & MIMO_PLCP_40MHZ)
				rspec |= (PHY_TXC1_BW_40MHZ << RSPEC_BW_SHIFT);

			if (fbr_iscck)	
				rspec_fallback = cck_rspec(cck_phy2mac_rate
						    (txh->FragPLCPFallback[0]));
			else {	
				rspec_fallback = RSPEC_MIMORATE;
				rspec_fallback |=
				    txh->FragPLCPFallback[0] & ~MIMO_PLCP_40MHZ;
				if (txh->FragPLCPFallback[0] & MIMO_PLCP_40MHZ)
					rspec_fallback |=
					    (PHY_TXC1_BW_40MHZ <<
					     RSPEC_BW_SHIFT);
			}

			if (use_rts || use_cts) {
				rts_rspec =
				    brcms_c_rspec_to_rts_rspec(wlc,
					rspec, false, mimo_ctlchbw);
				rts_rspec_fallback =
				    brcms_c_rspec_to_rts_rspec(wlc,
					rspec_fallback, false, mimo_ctlchbw);
			}
		}

		
		
		if ((mcs_2_rate(mcs, true, false) >= f->dmaxferrate) &&
		    (count == f->mcs2ampdu_table[mcs])) {
			BCMMSG(wlc->wiphy, "wl%d: PR 37644: stopping"
				" ampdu at %d for mcs %d\n",
				wlc->pub->unit, count, mcs);
			break;
		}

		if (count == scb_ampdu->max_pdu)
			break;

		p = pktq_ppeek(&qi->q, prec);
		
		tx_info = IEEE80211_SKB_CB(p);

		if (p) {
			if ((tx_info->flags & IEEE80211_TX_CTL_AMPDU) &&
			    ((u8) (p->priority) == tid)) {
				plen = p->len + AMPDU_MAX_MPDU_OVERHEAD;
				plen = max(scb_ampdu->min_len, plen);

				if ((plen + ampdu_len) > max_ampdu_bytes) {
					p = NULL;
					continue;
				}

				if (*wlc->core->txavail[fifo] <= seg_cnt + 1) {
					wiphy_err(wiphy, "%s: No fifo space  "
						  "!!\n", __func__);
					p = NULL;
					continue;
				}
				p = brcmu_pktq_pdeq(&qi->q, prec);
			} else {
				p = NULL;
			}
		}
	}			

	ini->tx_in_transit += count;

	if (count) {
		
		txh = (struct d11txh *) pkt[count - 1]->data;
		mcl = le16_to_cpu(txh->MacTxControlLow);
		mcl &= ~TXC_AMPDU_MASK;
		mcl |= (TXC_AMPDU_LAST << TXC_AMPDU_SHIFT);
		txh->MacTxControlLow = cpu_to_le16(mcl);

		
		ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
		txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM] = 0;
		ampdu_len -= ndelim * AMPDU_DELIMITER_LEN;

		
		fbr_iscck = ((le16_to_cpu(txh->XtraFrameTypes) & 0x3) == 0);
		len = fbr_iscck ? BRCMS_GET_CCK_PLCP_LEN(txh->FragPLCPFallback)
		    : BRCMS_GET_MIMO_PLCP_LEN(txh->FragPLCPFallback);
		ampdu_len -= roundup(len, 4) - len;

		
		txh = (struct d11txh *) pkt[0]->data;
		plcp = (u8 *) (txh + 1);

		BRCMS_SET_MIMO_PLCP_LEN(plcp, ampdu_len);
		
		BRCMS_SET_MIMO_PLCP_AMPDU(plcp);

		
		if (txh->MModeLen) {
			u16 mmodelen =
			    brcms_c_calc_lsig_len(wlc, rspec, ampdu_len);
			txh->MModeLen = cpu_to_le16(mmodelen);
			preamble_type = BRCMS_MM_PREAMBLE;
		}
		if (txh->MModeFbrLen) {
			u16 mmfbrlen =
			    brcms_c_calc_lsig_len(wlc, rspec_fallback,
						  ampdu_len);
			txh->MModeFbrLen = cpu_to_le16(mmfbrlen);
			fbr_preamble_type = BRCMS_MM_PREAMBLE;
		}

		
		if (mcs_2_rate(mcs, true, false) >= f->dmaxferrate) {
			dma_len = min(dma_len, f->ampdu_pld_size);
			txh->PreloadSize = cpu_to_le16(dma_len);
		} else
			txh->PreloadSize = 0;

		mch = le16_to_cpu(txh->MacTxControlHigh);

		
		if (use_rts || use_cts) {
			u16 durid;
			rts = (struct ieee80211_rts *)&txh->rts_frame;
			if ((mch & TXC_PREAMBLE_RTS_MAIN_SHORT) ==
			    TXC_PREAMBLE_RTS_MAIN_SHORT)
				rts_preamble_type = BRCMS_SHORT_PREAMBLE;

			if ((mch & TXC_PREAMBLE_RTS_FB_SHORT) ==
			    TXC_PREAMBLE_RTS_FB_SHORT)
				rts_fbr_preamble_type = BRCMS_SHORT_PREAMBLE;

			durid =
			    brcms_c_compute_rtscts_dur(wlc, use_cts, rts_rspec,
						   rspec, rts_preamble_type,
						   preamble_type, ampdu_len,
						   true);
			rts->duration = cpu_to_le16(durid);
			durid = brcms_c_compute_rtscts_dur(wlc, use_cts,
						       rts_rspec_fallback,
						       rspec_fallback,
						       rts_fbr_preamble_type,
						       fbr_preamble_type,
						       ampdu_len, true);
			txh->RTSDurFallback = cpu_to_le16(durid);
			
			txh->TxFesTimeNormal = rts->duration;
			
			txh->TxFesTimeFallback = txh->RTSDurFallback;
		}

		
		if (fbr) {
			mch |= TXC_AMPDU_FBR;
			txh->MacTxControlHigh = cpu_to_le16(mch);
			BRCMS_SET_MIMO_PLCP_AMPDU(plcp);
			BRCMS_SET_MIMO_PLCP_AMPDU(txh->FragPLCPFallback);
		}

		BCMMSG(wlc->wiphy, "wl%d: count %d ampdu_len %d\n",
			wlc->pub->unit, count, ampdu_len);

		
		frameid = le16_to_cpu(txh->TxFrameID);
		if (frameid & TXFID_RATE_PROBE_MASK)
			wiphy_err(wiphy, "%s: XXX what to do with "
				  "TXFID_RATE_PROBE_MASK!?\n", __func__);

		for (i = 0; i < count; i++)
			brcms_c_txfifo(wlc, fifo, pkt[i], i == (count - 1),
				   ampdu->txpkt_weight);

	}
	
	return err;
}

static void
brcms_c_ampdu_rate_status(struct brcms_c_info *wlc,
			  struct ieee80211_tx_info *tx_info,
			  struct tx_status *txs, u8 mcs)
{
	struct ieee80211_tx_rate *txrate = tx_info->status.rates;
	int i;

	
	for (i = 2; i < IEEE80211_TX_MAX_RATES; i++) {
		txrate[i].idx = -1;
		txrate[i].count = 0;
	}
}

static void
brcms_c_ampdu_dotxstatus_complete(struct ampdu_info *ampdu, struct scb *scb,
			      struct sk_buff *p, struct tx_status *txs,
			      u32 s1, u32 s2)
{
	struct scb_ampdu *scb_ampdu;
	struct brcms_c_info *wlc = ampdu->wlc;
	struct scb_ampdu_tid_ini *ini;
	u8 bitmap[8], queue, tid;
	struct d11txh *txh;
	u8 *plcp;
	struct ieee80211_hdr *h;
	u16 seq, start_seq = 0, bindex, index, mcl;
	u8 mcs = 0;
	bool ba_recd = false, ack_recd = false;
	u8 suc_mpdu = 0, tot_mpdu = 0;
	uint supr_status;
	bool update_rate = true, retry = true, tx_error = false;
	u16 mimoantsel = 0;
	u8 antselid = 0;
	u8 retry_limit, rr_retry_limit;
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(p);
	struct wiphy *wiphy = wlc->wiphy;

#ifdef DEBUG
	u8 hole[AMPDU_MAX_MPDU];
	memset(hole, 0, sizeof(hole));
#endif

	scb_ampdu = &scb->scb_ampdu;
	tid = (u8) (p->priority);

	ini = &scb_ampdu->ini[tid];
	retry_limit = ampdu->retry_limit_tid[tid];
	rr_retry_limit = ampdu->rr_retry_limit_tid[tid];
	memset(bitmap, 0, sizeof(bitmap));
	queue = txs->frameid & TXFID_QUEUE_MASK;
	supr_status = txs->status & TX_STATUS_SUPR_MASK;

	if (txs->status & TX_STATUS_ACK_RCV) {
		if (TX_STATUS_SUPR_UF == supr_status)
			update_rate = false;

		WARN_ON(!(txs->status & TX_STATUS_INTERMEDIATE));
		start_seq = txs->sequence >> SEQNUM_SHIFT;
		bitmap[0] = (txs->status & TX_STATUS_BA_BMAP03_MASK) >>
		    TX_STATUS_BA_BMAP03_SHIFT;

		WARN_ON(s1 & TX_STATUS_INTERMEDIATE);
		WARN_ON(!(s1 & TX_STATUS_AMPDU));

		bitmap[0] |=
		    (s1 & TX_STATUS_BA_BMAP47_MASK) <<
		    TX_STATUS_BA_BMAP47_SHIFT;
		bitmap[1] = (s1 >> 8) & 0xff;
		bitmap[2] = (s1 >> 16) & 0xff;
		bitmap[3] = (s1 >> 24) & 0xff;

		bitmap[4] = s2 & 0xff;
		bitmap[5] = (s2 >> 8) & 0xff;
		bitmap[6] = (s2 >> 16) & 0xff;
		bitmap[7] = (s2 >> 24) & 0xff;

		ba_recd = true;
	} else {
		if (supr_status) {
			update_rate = false;
			if (supr_status == TX_STATUS_SUPR_BADCH) {
				wiphy_err(wiphy,
					  "%s: Pkt tx suppressed, illegal channel possibly %d\n",
					  __func__, CHSPEC_CHANNEL(
					  wlc->default_bss->chanspec));
			} else {
				if (supr_status != TX_STATUS_SUPR_FRAG)
					wiphy_err(wiphy, "%s: supr_status 0x%x\n",
						  __func__, supr_status);
			}
			
			if (supr_status == TX_STATUS_SUPR_BADCH ||
			    supr_status == TX_STATUS_SUPR_EXPTIME) {
				retry = false;
			} else if (supr_status == TX_STATUS_SUPR_EXPTIME) {
			} else if (supr_status == TX_STATUS_SUPR_FRAG) {
				if (brcms_c_ffpld_check_txfunfl(wlc,
					prio2fifo[tid]) > 0)
					tx_error = true;
			}
		} else if (txs->phyerr) {
			update_rate = false;
			wiphy_err(wiphy, "%s: ampdu tx phy error (0x%x)\n",
				  __func__, txs->phyerr);

			if (brcm_msg_level & LOG_ERROR_VAL) {
				brcmu_prpkt("txpkt (AMPDU)", p);
				brcms_c_print_txdesc((struct d11txh *) p->data);
			}
			brcms_c_print_txstatus(txs);
		}
	}

	
	while (p) {
		tx_info = IEEE80211_SKB_CB(p);
		txh = (struct d11txh *) p->data;
		mcl = le16_to_cpu(txh->MacTxControlLow);
		plcp = (u8 *) (txh + 1);
		h = (struct ieee80211_hdr *)(plcp + D11_PHY_HDR_LEN);
		seq = le16_to_cpu(h->seq_ctrl) >> SEQNUM_SHIFT;

		if (tot_mpdu == 0) {
			mcs = plcp[0] & MIMO_PLCP_MCS_MASK;
			mimoantsel = le16_to_cpu(txh->ABI_MimoAntSel);
		}

		index = TX_SEQ_TO_INDEX(seq);
		ack_recd = false;
		if (ba_recd) {
			bindex = MODSUB_POW2(seq, start_seq, SEQNUM_MAX);
			BCMMSG(wiphy,
			       "tid %d seq %d, start_seq %d, bindex %d set %d, index %d\n",
			       tid, seq, start_seq, bindex,
			       isset(bitmap, bindex), index);
			
			if ((bindex < AMPDU_TX_BA_MAX_WSIZE)
			    && isset(bitmap, bindex)) {
				ini->tx_in_transit--;
				ini->txretry[index] = 0;

				
				brcms_c_ampdu_rate_status(wlc, tx_info, txs,
							  mcs);
				tx_info->flags |= IEEE80211_TX_STAT_ACK;
				tx_info->flags |= IEEE80211_TX_STAT_AMPDU;
				tx_info->status.ampdu_ack_len =
					tx_info->status.ampdu_len = 1;

				skb_pull(p, D11_PHY_HDR_LEN);
				skb_pull(p, D11_TXH_LEN);

				ieee80211_tx_status_irqsafe(wlc->pub->ieee_hw,
							    p);
				ack_recd = true;
				suc_mpdu++;
			}
		}
		
		if (!ack_recd) {
			if (retry && (ini->txretry[index] < (int)retry_limit)) {
				ini->txretry[index]++;
				ini->tx_in_transit--;
				brcms_c_txq_enq(wlc, scb, p,
						BRCMS_PRIO_TO_HI_PREC(tid));
			} else {
				
				ini->tx_in_transit--;
				ieee80211_tx_info_clear_status(tx_info);
				tx_info->status.ampdu_ack_len = 0;
				tx_info->status.ampdu_len = 1;
				tx_info->flags |=
				    IEEE80211_TX_STAT_AMPDU_NO_BACK;
				skb_pull(p, D11_PHY_HDR_LEN);
				skb_pull(p, D11_TXH_LEN);
				BCMMSG(wiphy,
				       "BA Timeout, seq %d, in_transit %d\n",
				       seq, ini->tx_in_transit);
				ieee80211_tx_status_irqsafe(wlc->pub->ieee_hw,
							    p);
			}
		}
		tot_mpdu++;

		
		if (((mcl & TXC_AMPDU_MASK) >> TXC_AMPDU_SHIFT) ==
		    TXC_AMPDU_LAST)
			break;

		p = dma_getnexttxp(wlc->hw->di[queue], DMA_RANGE_TRANSMITTED);
	}
	brcms_c_send_q(wlc);

	
	antselid = brcms_c_antsel_antsel2id(wlc->asi, mimoantsel);

	brcms_c_txfifo_complete(wlc, queue, ampdu->txpkt_weight);
}

void
brcms_c_ampdu_dotxstatus(struct ampdu_info *ampdu, struct scb *scb,
		     struct sk_buff *p, struct tx_status *txs)
{
	struct scb_ampdu *scb_ampdu;
	struct brcms_c_info *wlc = ampdu->wlc;
	struct scb_ampdu_tid_ini *ini;
	u32 s1 = 0, s2 = 0;
	struct ieee80211_tx_info *tx_info;

	tx_info = IEEE80211_SKB_CB(p);

	if (txs->status & TX_STATUS_ACK_RCV) {
		u8 status_delay = 0;

		
		s1 = bcma_read32(wlc->hw->d11core, D11REGOFFS(frmtxstatus));
		while ((s1 & TXS_V) == 0) {
			udelay(1);
			status_delay++;
			if (status_delay > 10)
				return; 
			s1 = bcma_read32(wlc->hw->d11core,
					 D11REGOFFS(frmtxstatus));
		}

		s2 = bcma_read32(wlc->hw->d11core, D11REGOFFS(frmtxstatus2));
	}

	if (scb) {
		scb_ampdu = &scb->scb_ampdu;
		ini = &scb_ampdu->ini[p->priority];
		brcms_c_ampdu_dotxstatus_complete(ampdu, scb, p, txs, s1, s2);
	} else {
		
		u8 queue = txs->frameid & TXFID_QUEUE_MASK;
		struct d11txh *txh;
		u16 mcl;
		while (p) {
			tx_info = IEEE80211_SKB_CB(p);
			txh = (struct d11txh *) p->data;
			mcl = le16_to_cpu(txh->MacTxControlLow);
			brcmu_pkt_buf_free_skb(p);
			
			if (((mcl & TXC_AMPDU_MASK) >> TXC_AMPDU_SHIFT) ==
			    TXC_AMPDU_LAST)
				break;
			p = dma_getnexttxp(wlc->hw->di[queue],
					   DMA_RANGE_TRANSMITTED);
		}
		brcms_c_txfifo_complete(wlc, queue, ampdu->txpkt_weight);
	}
}

void brcms_c_ampdu_macaddr_upd(struct brcms_c_info *wlc)
{
	char template[T_RAM_ACCESS_SZ * 2];

	
	memset(template, 0, sizeof(template));
	memcpy(template, wlc->pub->cur_etheraddr, ETH_ALEN);
	brcms_b_write_template_ram(wlc->hw, (T_BA_TPL_BASE + 16),
				  (T_RAM_ACCESS_SZ * 2),
				  template);
}

bool brcms_c_aggregatable(struct brcms_c_info *wlc, u8 tid)
{
	return wlc->ampdu->ini_enable[tid];
}

void brcms_c_ampdu_shm_upd(struct ampdu_info *ampdu)
{
	struct brcms_c_info *wlc = ampdu->wlc;

	if ((ampdu->rx_factor & IEEE80211_HT_AMPDU_PARM_FACTOR) ==
	    IEEE80211_HT_MAX_AMPDU_64K) {
		brcms_b_write_shm(wlc->hw, M_MIMO_MAXSYM, MIMO_MAXSYM_MAX);
		brcms_b_write_shm(wlc->hw, M_WATCHDOG_8TU, WATCHDOG_8TU_MAX);
	} else {
		brcms_b_write_shm(wlc->hw, M_MIMO_MAXSYM, MIMO_MAXSYM_DEF);
		brcms_b_write_shm(wlc->hw, M_WATCHDOG_8TU, WATCHDOG_8TU_DEF);
	}
}

static bool cb_del_ampdu_pkt(struct sk_buff *mpdu, void *arg_a)
{
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(mpdu);
	struct cb_del_ampdu_pars *ampdu_pars =
				 (struct cb_del_ampdu_pars *)arg_a;
	bool rc;

	rc = tx_info->flags & IEEE80211_TX_CTL_AMPDU ? true : false;
	rc = rc && (tx_info->control.sta == NULL || ampdu_pars->sta == NULL ||
		    tx_info->control.sta == ampdu_pars->sta);
	rc = rc && ((u8)(mpdu->priority) == ampdu_pars->tid);
	return rc;
}

static void dma_cb_fn_ampdu(void *txi, void *arg_a)
{
	struct ieee80211_sta *sta = arg_a;
	struct ieee80211_tx_info *tx_info = (struct ieee80211_tx_info *)txi;

	if ((tx_info->flags & IEEE80211_TX_CTL_AMPDU) &&
	    (tx_info->control.sta == sta || sta == NULL))
		tx_info->control.sta = NULL;
}

void brcms_c_ampdu_flush(struct brcms_c_info *wlc,
		     struct ieee80211_sta *sta, u16 tid)
{
	struct brcms_txq_info *qi = wlc->pkt_queue;
	struct pktq *pq = &qi->q;
	int prec;
	struct cb_del_ampdu_pars ampdu_pars;

	ampdu_pars.sta = sta;
	ampdu_pars.tid = tid;
	for (prec = 0; prec < pq->num_prec; prec++)
		brcmu_pktq_pflush(pq, prec, true, cb_del_ampdu_pkt,
			    (void *)&ampdu_pars);
	brcms_c_inval_dma_pkts(wlc->hw, sta, dma_cb_fn_ampdu);
}
