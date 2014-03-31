/*
 * Copyright (c) 2008-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/dma-mapping.h>
#include "ath9k.h"

#define FUDGE 2

static void ath9k_reset_beacon_status(struct ath_softc *sc)
{
	sc->beacon.tx_processed = false;
	sc->beacon.tx_last = false;
}

int ath_beaconq_config(struct ath_softc *sc)
{
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath9k_tx_queue_info qi, qi_be;
	struct ath_txq *txq;

	ath9k_hw_get_txq_props(ah, sc->beacon.beaconq, &qi);
	if (sc->sc_ah->opmode == NL80211_IFTYPE_AP) {
		
		qi.tqi_aifs = 1;
		qi.tqi_cwmin = 0;
		qi.tqi_cwmax = 0;
	} else {
		
		txq = sc->tx.txq_map[WME_AC_BE];
		ath9k_hw_get_txq_props(ah, txq->axq_qnum, &qi_be);
		qi.tqi_aifs = qi_be.tqi_aifs;
		qi.tqi_cwmin = 4*qi_be.tqi_cwmin;
		qi.tqi_cwmax = qi_be.tqi_cwmax;
	}

	if (!ath9k_hw_set_txq_props(ah, sc->beacon.beaconq, &qi)) {
		ath_err(common,
			"Unable to update h/w beacon queue parameters\n");
		return 0;
	} else {
		ath9k_hw_resettxqueue(ah, sc->beacon.beaconq);
		return 1;
	}
}

static void ath_beacon_setup(struct ath_softc *sc, struct ieee80211_vif *vif,
			     struct ath_buf *bf, int rateidx)
{
	struct sk_buff *skb = bf->bf_mpdu;
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath_tx_info info;
	struct ieee80211_supported_band *sband;
	u8 chainmask = ah->txchainmask;
	u8 rate = 0;

	ath9k_reset_beacon_status(sc);

	sband = &sc->sbands[common->hw->conf.channel->band];
	rate = sband->bitrates[rateidx].hw_value;
	if (vif->bss_conf.use_short_preamble)
		rate |= sband->bitrates[rateidx].hw_value_short;

	memset(&info, 0, sizeof(info));
	info.pkt_len = skb->len + FCS_LEN;
	info.type = ATH9K_PKT_TYPE_BEACON;
	info.txpower = MAX_RATE_POWER;
	info.keyix = ATH9K_TXKEYIX_INVALID;
	info.keytype = ATH9K_KEY_TYPE_CLEAR;
	info.flags = ATH9K_TXDESC_NOACK | ATH9K_TXDESC_INTREQ;

	info.buf_addr[0] = bf->bf_buf_addr;
	info.buf_len[0] = roundup(skb->len, 4);

	info.is_first = true;
	info.is_last = true;

	info.qcu = sc->beacon.beaconq;

	info.rates[0].Tries = 1;
	info.rates[0].Rate = rate;
	info.rates[0].ChSel = ath_txchainmask_reduction(sc, chainmask, rate);

	ath9k_hw_set_txdesc(ah, bf->bf_desc, &info);
}

static void ath_tx_cabq(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ath_softc *sc = hw->priv;
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_tx_control txctl;

	memset(&txctl, 0, sizeof(struct ath_tx_control));
	txctl.txq = sc->beacon.cabq;

	ath_dbg(common, XMIT, "transmitting CABQ packet, skb: %p\n", skb);

	if (ath_tx_start(hw, skb, &txctl) != 0) {
		ath_dbg(common, XMIT, "CABQ TX failed\n");
		dev_kfree_skb_any(skb);
	}
}

static struct ath_buf *ath_beacon_generate(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif)
{
	struct ath_softc *sc = hw->priv;
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_buf *bf;
	struct ath_vif *avp;
	struct sk_buff *skb;
	struct ath_txq *cabq;
	struct ieee80211_tx_info *info;
	int cabq_depth;

	ath9k_reset_beacon_status(sc);

	avp = (void *)vif->drv_priv;
	cabq = sc->beacon.cabq;

	if ((avp->av_bcbuf == NULL) || !avp->is_bslot_active)
		return NULL;

	

	bf = avp->av_bcbuf;
	skb = bf->bf_mpdu;
	if (skb) {
		dma_unmap_single(sc->dev, bf->bf_buf_addr,
				 skb->len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb);
		bf->bf_buf_addr = 0;
	}

	

	skb = ieee80211_beacon_get(hw, vif);
	bf->bf_mpdu = skb;
	if (skb == NULL)
		return NULL;
	((struct ieee80211_mgmt *)skb->data)->u.beacon.timestamp =
		avp->tsf_adjust;

	info = IEEE80211_SKB_CB(skb);
	if (info->flags & IEEE80211_TX_CTL_ASSIGN_SEQ) {
		struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
		sc->tx.seq_no += 0x10;
		hdr->seq_ctrl &= cpu_to_le16(IEEE80211_SCTL_FRAG);
		hdr->seq_ctrl |= cpu_to_le16(sc->tx.seq_no);
	}

	bf->bf_buf_addr = dma_map_single(sc->dev, skb->data,
					 skb->len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(sc->dev, bf->bf_buf_addr))) {
		dev_kfree_skb_any(skb);
		bf->bf_mpdu = NULL;
		bf->bf_buf_addr = 0;
		ath_err(common, "dma_mapping_error on beaconing\n");
		return NULL;
	}

	skb = ieee80211_get_buffered_bc(hw, vif);

	spin_lock_bh(&cabq->axq_lock);
	cabq_depth = cabq->axq_depth;
	spin_unlock_bh(&cabq->axq_lock);

	if (skb && cabq_depth) {
		if (sc->nvifs > 1) {
			ath_dbg(common, BEACON,
				"Flushing previous cabq traffic\n");
			ath_draintxq(sc, cabq, false);
		}
	}

	ath_beacon_setup(sc, vif, bf, info->control.rates[0].idx);

	while (skb) {
		ath_tx_cabq(hw, skb);
		skb = ieee80211_get_buffered_bc(hw, vif);
	}

	return bf;
}

int ath_beacon_alloc(struct ath_softc *sc, struct ieee80211_vif *vif)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_vif *avp;
	struct ath_buf *bf;
	struct sk_buff *skb;
	struct ath_beacon_config *cur_conf = &sc->cur_beacon_conf;
	__le64 tstamp;

	avp = (void *)vif->drv_priv;

	
	if (!avp->av_bcbuf) {
		avp->av_bcbuf = list_first_entry(&sc->beacon.bbuf,
						 struct ath_buf, list);
		list_del(&avp->av_bcbuf->list);

		if (ath9k_uses_beacons(vif->type)) {
			int slot;
			avp->av_bslot = 0;
			for (slot = 0; slot < ATH_BCBUF; slot++)
				if (sc->beacon.bslot[slot] == NULL) {
					avp->av_bslot = slot;
					avp->is_bslot_active = false;

					
					if (slot == 0 || !sc->beacon.bslot[slot-1])
						break;
				}
			BUG_ON(sc->beacon.bslot[avp->av_bslot] != NULL);
			sc->beacon.bslot[avp->av_bslot] = vif;
			sc->nbcnvifs++;
		}
	}

	
	bf = avp->av_bcbuf;
	if (bf->bf_mpdu != NULL) {
		skb = bf->bf_mpdu;
		dma_unmap_single(sc->dev, bf->bf_buf_addr,
				 skb->len, DMA_TO_DEVICE);
		dev_kfree_skb_any(skb);
		bf->bf_mpdu = NULL;
		bf->bf_buf_addr = 0;
	}

	
	skb = ieee80211_beacon_get(sc->hw, vif);
	if (skb == NULL)
		return -ENOMEM;

	tstamp = ((struct ieee80211_mgmt *)skb->data)->u.beacon.timestamp;
	sc->beacon.bc_tstamp = (u32) le64_to_cpu(tstamp);
	
	if (avp->av_bslot > 0) {
		u64 tsfadjust;
		int intval;

		intval = cur_conf->beacon_interval ? : ATH_DEFAULT_BINTVAL;

		tsfadjust = TU_TO_USEC(intval * avp->av_bslot) / ATH_BCBUF;
		avp->tsf_adjust = cpu_to_le64(tsfadjust);

		ath_dbg(common, BEACON,
			"stagger beacons, bslot %d intval %u tsfadjust %llu\n",
			avp->av_bslot, intval, (unsigned long long)tsfadjust);

		((struct ieee80211_mgmt *)skb->data)->u.beacon.timestamp =
			avp->tsf_adjust;
	} else
		avp->tsf_adjust = cpu_to_le64(0);

	bf->bf_mpdu = skb;
	bf->bf_buf_addr = dma_map_single(sc->dev, skb->data,
					 skb->len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(sc->dev, bf->bf_buf_addr))) {
		dev_kfree_skb_any(skb);
		bf->bf_mpdu = NULL;
		bf->bf_buf_addr = 0;
		ath_err(common, "dma_mapping_error on beacon alloc\n");
		return -ENOMEM;
	}
	avp->is_bslot_active = true;

	return 0;
}

void ath_beacon_return(struct ath_softc *sc, struct ath_vif *avp)
{
	if (avp->av_bcbuf != NULL) {
		struct ath_buf *bf;

		avp->is_bslot_active = false;
		if (avp->av_bslot != -1) {
			sc->beacon.bslot[avp->av_bslot] = NULL;
			sc->nbcnvifs--;
			avp->av_bslot = -1;
		}

		bf = avp->av_bcbuf;
		if (bf->bf_mpdu != NULL) {
			struct sk_buff *skb = bf->bf_mpdu;
			dma_unmap_single(sc->dev, bf->bf_buf_addr,
					 skb->len, DMA_TO_DEVICE);
			dev_kfree_skb_any(skb);
			bf->bf_mpdu = NULL;
			bf->bf_buf_addr = 0;
		}
		list_add_tail(&bf->list, &sc->beacon.bbuf);

		avp->av_bcbuf = NULL;
	}
}

void ath_beacon_tasklet(unsigned long data)
{
	struct ath_softc *sc = (struct ath_softc *)data;
	struct ath_beacon_config *cur_conf = &sc->cur_beacon_conf;
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath_buf *bf = NULL;
	struct ieee80211_vif *vif;
	bool edma = !!(ah->caps.hw_caps & ATH9K_HW_CAP_EDMA);
	int slot;
	u32 bfaddr, bc = 0;

	if (ath9k_hw_numtxpending(ah, sc->beacon.beaconq) != 0) {
		sc->beacon.bmisscnt++;

		if (sc->beacon.bmisscnt < BSTUCK_THRESH * sc->nbcnvifs) {
			ath_dbg(common, BSTUCK,
				"missed %u consecutive beacons\n",
				sc->beacon.bmisscnt);
			ath9k_hw_stop_dma_queue(ah, sc->beacon.beaconq);
			if (sc->beacon.bmisscnt > 3)
				ath9k_hw_bstuck_nfcal(ah);
		} else if (sc->beacon.bmisscnt >= BSTUCK_THRESH) {
			ath_dbg(common, BSTUCK, "beacon is officially stuck\n");
			sc->sc_flags |= SC_OP_TSF_RESET;
			ieee80211_queue_work(sc->hw, &sc->hw_reset_work);
		}

		return;
	}



	if (ah->opmode == NL80211_IFTYPE_AP) {
		u16 intval;
		u32 tsftu;
		u64 tsf;

		intval = cur_conf->beacon_interval ? : ATH_DEFAULT_BINTVAL;
		tsf = ath9k_hw_gettsf64(ah);
		tsf += TU_TO_USEC(ah->config.sw_beacon_response_time);
		tsftu = TSF_TO_TU((tsf * ATH_BCBUF) >>32, tsf * ATH_BCBUF);
		slot = (tsftu % (intval * ATH_BCBUF)) / intval;
		vif = sc->beacon.bslot[slot];

		ath_dbg(common, BEACON,
			"slot %d [tsf %llu tsftu %u intval %u] vif %p\n",
			slot, tsf, tsftu / ATH_BCBUF, intval, vif);
	} else {
		slot = 0;
		vif = sc->beacon.bslot[slot];
	}


	bfaddr = 0;
	if (vif) {
		bf = ath_beacon_generate(sc->hw, vif);
		if (bf != NULL) {
			bfaddr = bf->bf_daddr;
			bc = 1;
		}

		if (sc->beacon.bmisscnt != 0) {
			ath_dbg(common, BSTUCK,
				"resume beacon xmit after %u misses\n",
				sc->beacon.bmisscnt);
			sc->beacon.bmisscnt = 0;
		}
	}

	if (sc->beacon.updateslot == UPDATE) {
		sc->beacon.updateslot = COMMIT; 
		sc->beacon.slotupdate = slot;
	} else if (sc->beacon.updateslot == COMMIT && sc->beacon.slotupdate == slot) {
		ah->slottime = sc->beacon.slottime;
		ath9k_hw_init_global_settings(ah);
		sc->beacon.updateslot = OK;
	}
	if (bfaddr != 0) {
		
		ath9k_hw_puttxbuf(ah, sc->beacon.beaconq, bfaddr);

		if (!edma)
			ath9k_hw_txstart(ah, sc->beacon.beaconq);

		sc->beacon.ast_be_xmit += bc;     
	}
}

static void ath9k_beacon_init(struct ath_softc *sc,
			      u32 next_beacon,
			      u32 beacon_period)
{
	if (sc->sc_flags & SC_OP_TSF_RESET) {
		ath9k_ps_wakeup(sc);
		ath9k_hw_reset_tsf(sc->sc_ah);
	}

	ath9k_hw_beaconinit(sc->sc_ah, next_beacon, beacon_period);

	if (sc->sc_flags & SC_OP_TSF_RESET) {
		ath9k_ps_restore(sc);
		sc->sc_flags &= ~SC_OP_TSF_RESET;
	}
}

static void ath_beacon_config_ap(struct ath_softc *sc,
				 struct ath_beacon_config *conf)
{
	struct ath_hw *ah = sc->sc_ah;
	u32 nexttbtt, intval;

	
	intval = TU_TO_USEC(conf->beacon_interval);
	intval /= ATH_BCBUF;    
	nexttbtt = intval;

	ah->imask |= ATH9K_INT_SWBA;
	ath_beaconq_config(sc);

	

	ath9k_hw_disable_interrupts(ah);
	sc->sc_flags |= SC_OP_TSF_RESET;
	ath9k_beacon_init(sc, nexttbtt, intval);
	sc->beacon.bmisscnt = 0;
	ath9k_hw_set_interrupts(ah);
	ath9k_hw_enable_interrupts(ah);
}

static void ath_beacon_config_sta(struct ath_softc *sc,
				  struct ath_beacon_config *conf)
{
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath9k_beacon_state bs;
	int dtimperiod, dtimcount, sleepduration;
	int cfpperiod, cfpcount;
	u32 nexttbtt = 0, intval, tsftu;
	u64 tsf;
	int num_beacons, offset, dtim_dec_count, cfp_dec_count;

	
	if (!common->curaid) {
		ath_dbg(common, BEACON,
			"STA is not yet associated..skipping beacon config\n");
		return;
	}

	memset(&bs, 0, sizeof(bs));
	intval = conf->beacon_interval;

	dtimperiod = conf->dtim_period;
	dtimcount = conf->dtim_count;
	if (dtimcount >= dtimperiod)	
		dtimcount = 0;
	cfpperiod = 1;			
	cfpcount = 0;

	sleepduration = conf->listen_interval * intval;

	tsf = ath9k_hw_gettsf64(ah);
	tsftu = TSF_TO_TU(tsf>>32, tsf) + FUDGE;

	num_beacons = tsftu / intval + 1;
	offset = tsftu % intval;
	nexttbtt = tsftu - offset;
	if (offset)
		nexttbtt += intval;

	
	dtim_dec_count = num_beacons % dtimperiod;
	
	cfp_dec_count = (num_beacons / dtimperiod) % cfpperiod;
	if (dtim_dec_count)
		cfp_dec_count++;

	dtimcount -= dtim_dec_count;
	if (dtimcount < 0)
		dtimcount += dtimperiod;

	cfpcount -= cfp_dec_count;
	if (cfpcount < 0)
		cfpcount += cfpperiod;

	bs.bs_intval = intval;
	bs.bs_nexttbtt = nexttbtt;
	bs.bs_dtimperiod = dtimperiod*intval;
	bs.bs_nextdtim = bs.bs_nexttbtt + dtimcount*intval;
	bs.bs_cfpperiod = cfpperiod*bs.bs_dtimperiod;
	bs.bs_cfpnext = bs.bs_nextdtim + cfpcount*bs.bs_dtimperiod;
	bs.bs_cfpmaxduration = 0;

	if (sleepduration > intval) {
		bs.bs_bmissthreshold = conf->listen_interval *
			ATH_DEFAULT_BMISS_LIMIT / 2;
	} else {
		bs.bs_bmissthreshold = DIV_ROUND_UP(conf->bmiss_timeout, intval);
		if (bs.bs_bmissthreshold > 15)
			bs.bs_bmissthreshold = 15;
		else if (bs.bs_bmissthreshold <= 0)
			bs.bs_bmissthreshold = 1;
	}


	bs.bs_sleepduration = roundup(IEEE80211_MS_TO_TU(100), sleepduration);
	if (bs.bs_sleepduration > bs.bs_dtimperiod)
		bs.bs_sleepduration = bs.bs_dtimperiod;

	
	bs.bs_tsfoor_threshold = ATH9K_TSFOOR_THRESHOLD;

	ath_dbg(common, BEACON, "tsf: %llu tsftu: %u\n", tsf, tsftu);
	ath_dbg(common, BEACON,
		"bmiss: %u sleep: %u cfp-period: %u maxdur: %u next: %u\n",
		bs.bs_bmissthreshold, bs.bs_sleepduration,
		bs.bs_cfpperiod, bs.bs_cfpmaxduration, bs.bs_cfpnext);

	

	ath9k_hw_disable_interrupts(ah);
	ath9k_hw_set_sta_beacon_timers(ah, &bs);
	ah->imask |= ATH9K_INT_BMISS;

	ath9k_hw_set_interrupts(ah);
	ath9k_hw_enable_interrupts(ah);
}

static void ath_beacon_config_adhoc(struct ath_softc *sc,
				    struct ath_beacon_config *conf)
{
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);
	u32 tsf, intval, nexttbtt;

	ath9k_reset_beacon_status(sc);

	intval = TU_TO_USEC(conf->beacon_interval);
	tsf = roundup(ath9k_hw_gettsf32(ah) + TU_TO_USEC(FUDGE), intval);
	nexttbtt = tsf + intval;

	ath_dbg(common, BEACON, "IBSS nexttbtt %u intval %u (%u)\n",
		nexttbtt, intval, conf->beacon_interval);

	ah->imask |= ATH9K_INT_SWBA;

	ath_beaconq_config(sc);

	

	ath9k_hw_disable_interrupts(ah);
	ath9k_beacon_init(sc, nexttbtt, intval);
	sc->beacon.bmisscnt = 0;

	ath9k_hw_set_interrupts(ah);
	ath9k_hw_enable_interrupts(ah);
}

static bool ath9k_allow_beacon_config(struct ath_softc *sc,
				      struct ieee80211_vif *vif)
{
	struct ath_beacon_config *cur_conf = &sc->cur_beacon_conf;
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ieee80211_bss_conf *bss_conf = &vif->bss_conf;
	struct ath_vif *avp = (void *)vif->drv_priv;

	if ((sc->sc_ah->opmode == NL80211_IFTYPE_AP) &&
	    (sc->nbcnvifs > 1) &&
	    (vif->type == NL80211_IFTYPE_AP) &&
	    (cur_conf->beacon_interval != bss_conf->beacon_int)) {
		ath_dbg(common, CONFIG,
			"Changing beacon interval of multiple AP interfaces !\n");
		return false;
	}
	if ((sc->sc_ah->opmode == NL80211_IFTYPE_AP) &&
	    (vif->type != NL80211_IFTYPE_AP)) {
		ath_dbg(common, CONFIG,
			"STA vif's beacon not allowed on AP mode\n");
		return false;
	}
	if ((sc->sc_ah->opmode == NL80211_IFTYPE_STATION) &&
	    (vif->type == NL80211_IFTYPE_STATION) &&
	    (sc->sc_flags & SC_OP_BEACONS) &&
	    !avp->primary_sta_vif) {
		ath_dbg(common, CONFIG,
			"Beacon already configured for a station interface\n");
		return false;
	}
	return true;
}

void ath_beacon_config(struct ath_softc *sc, struct ieee80211_vif *vif)
{
	struct ath_beacon_config *cur_conf = &sc->cur_beacon_conf;
	struct ieee80211_bss_conf *bss_conf = &vif->bss_conf;

	if (!ath9k_allow_beacon_config(sc, vif))
		return;

	
	cur_conf->beacon_interval = bss_conf->beacon_int;
	cur_conf->dtim_period = bss_conf->dtim_period;
	cur_conf->listen_interval = 1;
	cur_conf->dtim_count = 1;
	cur_conf->bmiss_timeout =
		ATH_DEFAULT_BMISS_LIMIT * cur_conf->beacon_interval;

	if (cur_conf->beacon_interval == 0)
		cur_conf->beacon_interval = 100;

	if (cur_conf->dtim_period == 0)
		cur_conf->dtim_period = 1;

	ath_set_beacon(sc);
}

static bool ath_has_valid_bslot(struct ath_softc *sc)
{
	struct ath_vif *avp;
	int slot;
	bool found = false;

	for (slot = 0; slot < ATH_BCBUF; slot++) {
		if (sc->beacon.bslot[slot]) {
			avp = (void *)sc->beacon.bslot[slot]->drv_priv;
			if (avp->is_bslot_active) {
				found = true;
				break;
			}
		}
	}
	return found;
}


void ath_set_beacon(struct ath_softc *sc)
{
	struct ath_common *common = ath9k_hw_common(sc->sc_ah);
	struct ath_beacon_config *cur_conf = &sc->cur_beacon_conf;

	switch (sc->sc_ah->opmode) {
	case NL80211_IFTYPE_AP:
		if (ath_has_valid_bslot(sc))
			ath_beacon_config_ap(sc, cur_conf);
		break;
	case NL80211_IFTYPE_ADHOC:
	case NL80211_IFTYPE_MESH_POINT:
		ath_beacon_config_adhoc(sc, cur_conf);
		break;
	case NL80211_IFTYPE_STATION:
		ath_beacon_config_sta(sc, cur_conf);
		break;
	default:
		ath_dbg(common, CONFIG, "Unsupported beaconing mode\n");
		return;
	}

	sc->sc_flags |= SC_OP_BEACONS;
}

void ath9k_set_beaconing_status(struct ath_softc *sc, bool status)
{
	struct ath_hw *ah = sc->sc_ah;

	if (!ath_has_valid_bslot(sc))
		return;

	ath9k_ps_wakeup(sc);
	if (status) {
		
		ah->imask |= ATH9K_INT_SWBA;
		ath9k_hw_set_interrupts(ah);
	} else {
		
		ah->imask &= ~ATH9K_INT_SWBA;
		ath9k_hw_set_interrupts(ah);
		tasklet_kill(&sc->bcon_tasklet);
		ath9k_hw_stop_dma_queue(ah, sc->beacon.beaconq);
	}
	ath9k_ps_restore(sc);
}
