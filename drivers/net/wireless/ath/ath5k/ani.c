/*
 * Copyright (C) 2010 Bruno Randolf <br1@einfach.org>
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

#include "ath5k.h"
#include "reg.h"
#include "debug.h"
#include "ani.h"




void
ath5k_ani_set_noise_immunity_level(struct ath5k_hw *ah, int level)
{
#if 0
	static const s8 lo[] = { -52, -56, -60, -64, -70 };
	static const s8 hi[] = { -18, -18, -16, -14, -12 };
	static const s8 sz[] = { -34, -41, -48, -55, -62 };
	static const s8 fr[] = { -70, -72, -75, -78, -80 };
#else
	static const s8 lo[] = { -64, -70 };
	static const s8 hi[] = { -14, -12 };
	static const s8 sz[] = { -55, -62 };
	static const s8 fr[] = { -78, -80 };
#endif
	if (level < 0 || level >= ARRAY_SIZE(sz)) {
		ATH5K_ERR(ah, "noise immunity level %d out of range",
			  level);
		return;
	}

	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_DESIRED_SIZE,
				AR5K_PHY_DESIRED_SIZE_TOT, sz[level]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_AGCCOARSE,
				AR5K_PHY_AGCCOARSE_LO, lo[level]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_AGCCOARSE,
				AR5K_PHY_AGCCOARSE_HI, hi[level]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_SIG,
				AR5K_PHY_SIG_FIRPWR, fr[level]);

	ah->ani_state.noise_imm_level = level;
	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "new level %d", level);
}

void
ath5k_ani_set_spur_immunity_level(struct ath5k_hw *ah, int level)
{
	static const int val[] = { 2, 4, 6, 8, 10, 12, 14, 16 };

	if (level < 0 || level >= ARRAY_SIZE(val) ||
	    level > ah->ani_state.max_spur_level) {
		ATH5K_ERR(ah, "spur immunity level %d out of range",
			  level);
		return;
	}

	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_OFDM_SELFCORR,
		AR5K_PHY_OFDM_SELFCORR_CYPWR_THR1, val[level]);

	ah->ani_state.spur_level = level;
	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "new level %d", level);
}

void
ath5k_ani_set_firstep_level(struct ath5k_hw *ah, int level)
{
	static const int val[] = { 0, 4, 8 };

	if (level < 0 || level >= ARRAY_SIZE(val)) {
		ATH5K_ERR(ah, "firstep level %d out of range", level);
		return;
	}

	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_SIG,
				AR5K_PHY_SIG_FIRSTEP, val[level]);

	ah->ani_state.firstep_level = level;
	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "new level %d", level);
}

void
ath5k_ani_set_ofdm_weak_signal_detection(struct ath5k_hw *ah, bool on)
{
	static const int m1l[] = { 127, 50 };
	static const int m2l[] = { 127, 40 };
	static const int m1[] = { 127, 0x4d };
	static const int m2[] = { 127, 0x40 };
	static const int m2cnt[] = { 31, 16 };
	static const int m2lcnt[] = { 63, 48 };

	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_WEAK_OFDM_LOW_THR,
				AR5K_PHY_WEAK_OFDM_LOW_THR_M1, m1l[on]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_WEAK_OFDM_LOW_THR,
				AR5K_PHY_WEAK_OFDM_LOW_THR_M2, m2l[on]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_WEAK_OFDM_HIGH_THR,
				AR5K_PHY_WEAK_OFDM_HIGH_THR_M1, m1[on]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_WEAK_OFDM_HIGH_THR,
				AR5K_PHY_WEAK_OFDM_HIGH_THR_M2, m2[on]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_WEAK_OFDM_HIGH_THR,
			AR5K_PHY_WEAK_OFDM_HIGH_THR_M2_COUNT, m2cnt[on]);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_WEAK_OFDM_LOW_THR,
			AR5K_PHY_WEAK_OFDM_LOW_THR_M2_COUNT, m2lcnt[on]);

	if (on)
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_WEAK_OFDM_LOW_THR,
				AR5K_PHY_WEAK_OFDM_LOW_THR_SELFCOR_EN);
	else
		AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_WEAK_OFDM_LOW_THR,
				AR5K_PHY_WEAK_OFDM_LOW_THR_SELFCOR_EN);

	ah->ani_state.ofdm_weak_sig = on;
	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "turned %s",
			  on ? "on" : "off");
}

void
ath5k_ani_set_cck_weak_signal_detection(struct ath5k_hw *ah, bool on)
{
	static const int val[] = { 8, 6 };
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_CCK_CROSSCORR,
				AR5K_PHY_CCK_CROSSCORR_WEAK_SIG_THR, val[on]);
	ah->ani_state.cck_weak_sig = on;
	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "turned %s",
			  on ? "on" : "off");
}



static void
ath5k_ani_raise_immunity(struct ath5k_hw *ah, struct ath5k_ani_state *as,
			 bool ofdm_trigger)
{
	int rssi = ewma_read(&ah->ah_beacon_rssi_avg);

	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "raise immunity (%s)",
		ofdm_trigger ? "ODFM" : "CCK");

	
	if (as->noise_imm_level < ATH5K_ANI_MAX_NOISE_IMM_LVL) {
		ath5k_ani_set_noise_immunity_level(ah, as->noise_imm_level + 1);
		return;
	}

	
	if (ofdm_trigger &&
	    as->spur_level < ah->ani_state.max_spur_level) {
		ath5k_ani_set_spur_immunity_level(ah, as->spur_level + 1);
		return;
	}

	
	if (ah->opmode == NL80211_IFTYPE_AP) {
		if (as->firstep_level < ATH5K_ANI_MAX_FIRSTEP_LVL)
			ath5k_ani_set_firstep_level(ah, as->firstep_level + 1);
		return;
	}

	


	if (rssi > ATH5K_ANI_RSSI_THR_HIGH) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
				  "beacon RSSI high");
		if (ofdm_trigger && as->ofdm_weak_sig) {
			ath5k_ani_set_ofdm_weak_signal_detection(ah, false);
			ath5k_ani_set_spur_immunity_level(ah, 0);
			return;
		}
		
		if (as->firstep_level < ATH5K_ANI_MAX_FIRSTEP_LVL) {
			ath5k_ani_set_firstep_level(ah, as->firstep_level + 1);
			return;
		}
	} else if (rssi > ATH5K_ANI_RSSI_THR_LOW) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
				  "beacon RSSI mid");
		if (ofdm_trigger && !as->ofdm_weak_sig)
			ath5k_ani_set_ofdm_weak_signal_detection(ah, true);
		if (as->firstep_level < ATH5K_ANI_MAX_FIRSTEP_LVL)
			ath5k_ani_set_firstep_level(ah, as->firstep_level + 1);
		return;
	} else if (ah->ah_current_channel->band == IEEE80211_BAND_2GHZ) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
				  "beacon RSSI low, 2GHz");
		if (ofdm_trigger && as->ofdm_weak_sig)
			ath5k_ani_set_ofdm_weak_signal_detection(ah, false);
		if (as->firstep_level > 0)
			ath5k_ani_set_firstep_level(ah, 0);
		return;
	}

}

static void
ath5k_ani_lower_immunity(struct ath5k_hw *ah, struct ath5k_ani_state *as)
{
	int rssi = ewma_read(&ah->ah_beacon_rssi_avg);

	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "lower immunity");

	if (ah->opmode == NL80211_IFTYPE_AP) {
		
		if (as->firstep_level > 0) {
			ath5k_ani_set_firstep_level(ah, as->firstep_level - 1);
			return;
		}
	} else {
		
		if (rssi > ATH5K_ANI_RSSI_THR_HIGH) {
		} else if (rssi > ATH5K_ANI_RSSI_THR_LOW) {
			if (!as->ofdm_weak_sig) {
				ath5k_ani_set_ofdm_weak_signal_detection(ah,
									 true);
				return;
			}
			if (as->firstep_level > 0) {
				ath5k_ani_set_firstep_level(ah,
							as->firstep_level - 1);
				return;
			}
		} else {
			
			if (as->firstep_level > 0) {
				ath5k_ani_set_firstep_level(ah,
							as->firstep_level - 1);
				return;
			}
		}
	}

	
	if (as->spur_level > 0) {
		ath5k_ani_set_spur_immunity_level(ah, as->spur_level - 1);
		return;
	}

	
	if (as->noise_imm_level > 0) {
		ath5k_ani_set_noise_immunity_level(ah, as->noise_imm_level - 1);
		return;
	}
}

static int
ath5k_hw_ani_get_listen_time(struct ath5k_hw *ah, struct ath5k_ani_state *as)
{
	struct ath_common *common = ath5k_hw_common(ah);
	int listen;

	spin_lock_bh(&common->cc_lock);

	ath_hw_cycle_counters_update(common);
	memcpy(&as->last_cc, &common->cc_ani, sizeof(as->last_cc));

	
	listen = ath_hw_get_listen_time(common);

	spin_unlock_bh(&common->cc_lock);

	return listen;
}

static int
ath5k_ani_save_and_clear_phy_errors(struct ath5k_hw *ah,
				    struct ath5k_ani_state *as)
{
	unsigned int ofdm_err, cck_err;

	if (!ah->ah_capabilities.cap_has_phyerr_counters)
		return 0;

	ofdm_err = ath5k_hw_reg_read(ah, AR5K_PHYERR_CNT1);
	cck_err = ath5k_hw_reg_read(ah, AR5K_PHYERR_CNT2);

	
	ath5k_hw_reg_write(ah, ATH5K_PHYERR_CNT_MAX - ATH5K_ANI_OFDM_TRIG_HIGH,
			   AR5K_PHYERR_CNT1);
	ath5k_hw_reg_write(ah, ATH5K_PHYERR_CNT_MAX - ATH5K_ANI_CCK_TRIG_HIGH,
			   AR5K_PHYERR_CNT2);

	ofdm_err = ATH5K_ANI_OFDM_TRIG_HIGH - (ATH5K_PHYERR_CNT_MAX - ofdm_err);
	cck_err = ATH5K_ANI_CCK_TRIG_HIGH - (ATH5K_PHYERR_CNT_MAX - cck_err);

	if (ofdm_err <= 0 && cck_err <= 0)
		return 0;

	
	if (ofdm_err > 0) {
		as->ofdm_errors += ofdm_err;
		as->sum_ofdm_errors += ofdm_err;
	}
	if (cck_err > 0) {
		as->cck_errors += cck_err;
		as->sum_cck_errors += cck_err;
	}
	return 1;
}

static void
ath5k_ani_period_restart(struct ath5k_ani_state *as)
{
	
	as->last_ofdm_errors = as->ofdm_errors;
	as->last_cck_errors = as->cck_errors;
	as->last_listen = as->listen_time;

	as->ofdm_errors = 0;
	as->cck_errors = 0;
	as->listen_time = 0;
}

void
ath5k_ani_calibration(struct ath5k_hw *ah)
{
	struct ath5k_ani_state *as = &ah->ani_state;
	int listen, ofdm_high, ofdm_low, cck_high, cck_low;

	listen = ath5k_hw_ani_get_listen_time(ah, as);
	as->listen_time += listen;

	if (as->ani_mode != ATH5K_ANI_MODE_AUTO)
		return;

	ath5k_ani_save_and_clear_phy_errors(ah, as);

	ofdm_high = as->listen_time * ATH5K_ANI_OFDM_TRIG_HIGH / 1000;
	cck_high = as->listen_time * ATH5K_ANI_CCK_TRIG_HIGH / 1000;
	ofdm_low = as->listen_time * ATH5K_ANI_OFDM_TRIG_LOW / 1000;
	cck_low = as->listen_time * ATH5K_ANI_CCK_TRIG_LOW / 1000;

	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
		"listen %d (now %d)", as->listen_time, listen);
	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
		"check high ofdm %d/%d cck %d/%d",
		as->ofdm_errors, ofdm_high, as->cck_errors, cck_high);

	if (as->ofdm_errors > ofdm_high || as->cck_errors > cck_high) {
		
		bool ofdm_flag = as->ofdm_errors > ofdm_high ? true : false;
		ath5k_ani_raise_immunity(ah, as, ofdm_flag);
		ath5k_ani_period_restart(as);

	} else if (as->listen_time > 5 * ATH5K_ANI_LISTEN_PERIOD) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
			"check low ofdm %d/%d cck %d/%d",
			as->ofdm_errors, ofdm_low, as->cck_errors, cck_low);

		if (as->ofdm_errors <= ofdm_low && as->cck_errors <= cck_low)
			ath5k_ani_lower_immunity(ah, as);

		ath5k_ani_period_restart(as);
	}
}



void
ath5k_ani_mib_intr(struct ath5k_hw *ah)
{
	struct ath5k_ani_state *as = &ah->ani_state;

	if (!ah->ah_capabilities.cap_has_phyerr_counters)
		return;

	
	ath5k_hw_reg_write(ah, 0, AR5K_OFDM_FIL_CNT);
	ath5k_hw_reg_write(ah, 0, AR5K_CCK_FIL_CNT);

	if (ah->ani_state.ani_mode != ATH5K_ANI_MODE_AUTO)
		return;

	if (ath5k_ani_save_and_clear_phy_errors(ah, as) == 0)
		return;

	if (as->ofdm_errors > ATH5K_ANI_OFDM_TRIG_HIGH ||
	    as->cck_errors > ATH5K_ANI_CCK_TRIG_HIGH)
		tasklet_schedule(&ah->ani_tasklet);
}

void
ath5k_ani_phy_error_report(struct ath5k_hw *ah,
			   enum ath5k_phy_error_code phyerr)
{
	struct ath5k_ani_state *as = &ah->ani_state;

	if (phyerr == AR5K_RX_PHY_ERROR_OFDM_TIMING) {
		as->ofdm_errors++;
		if (as->ofdm_errors > ATH5K_ANI_OFDM_TRIG_HIGH)
			tasklet_schedule(&ah->ani_tasklet);
	} else if (phyerr == AR5K_RX_PHY_ERROR_CCK_TIMING) {
		as->cck_errors++;
		if (as->cck_errors > ATH5K_ANI_CCK_TRIG_HIGH)
			tasklet_schedule(&ah->ani_tasklet);
	}
}



static void
ath5k_enable_phy_err_counters(struct ath5k_hw *ah)
{
	ath5k_hw_reg_write(ah, ATH5K_PHYERR_CNT_MAX - ATH5K_ANI_OFDM_TRIG_HIGH,
			   AR5K_PHYERR_CNT1);
	ath5k_hw_reg_write(ah, ATH5K_PHYERR_CNT_MAX - ATH5K_ANI_CCK_TRIG_HIGH,
			   AR5K_PHYERR_CNT2);
	ath5k_hw_reg_write(ah, AR5K_PHY_ERR_FIL_OFDM, AR5K_PHYERR_CNT1_MASK);
	ath5k_hw_reg_write(ah, AR5K_PHY_ERR_FIL_CCK, AR5K_PHYERR_CNT2_MASK);

	
	ath5k_hw_reg_write(ah, 0, AR5K_OFDM_FIL_CNT);
	ath5k_hw_reg_write(ah, 0, AR5K_CCK_FIL_CNT);
}

static void
ath5k_disable_phy_err_counters(struct ath5k_hw *ah)
{
	ath5k_hw_reg_write(ah, 0, AR5K_PHYERR_CNT1);
	ath5k_hw_reg_write(ah, 0, AR5K_PHYERR_CNT2);
	ath5k_hw_reg_write(ah, 0, AR5K_PHYERR_CNT1_MASK);
	ath5k_hw_reg_write(ah, 0, AR5K_PHYERR_CNT2_MASK);

	
	ath5k_hw_reg_write(ah, 0, AR5K_OFDM_FIL_CNT);
	ath5k_hw_reg_write(ah, 0, AR5K_CCK_FIL_CNT);
}

void
ath5k_ani_init(struct ath5k_hw *ah, enum ath5k_ani_mode mode)
{
	
	if (ah->ah_version < AR5K_AR5212)
		return;

	if (mode < ATH5K_ANI_MODE_OFF || mode > ATH5K_ANI_MODE_AUTO) {
		ATH5K_ERR(ah, "ANI mode %d out of range", mode);
		return;
	}

	
	memset(&ah->ani_state, 0, sizeof(ah->ani_state));

	
	if (ah->ah_mac_srev < AR5K_SREV_AR2414)
		ah->ani_state.max_spur_level = 7;
	else
		ah->ani_state.max_spur_level = 2;

	
	if (mode == ATH5K_ANI_MODE_OFF) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "ANI off\n");
	} else if (mode == ATH5K_ANI_MODE_MANUAL_LOW) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
			"ANI manual low -> high sensitivity\n");
		ath5k_ani_set_noise_immunity_level(ah, 0);
		ath5k_ani_set_spur_immunity_level(ah, 0);
		ath5k_ani_set_firstep_level(ah, 0);
		ath5k_ani_set_ofdm_weak_signal_detection(ah, true);
		ath5k_ani_set_cck_weak_signal_detection(ah, true);
	} else if (mode == ATH5K_ANI_MODE_MANUAL_HIGH) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI,
			"ANI manual high -> low sensitivity\n");
		ath5k_ani_set_noise_immunity_level(ah,
					ATH5K_ANI_MAX_NOISE_IMM_LVL);
		ath5k_ani_set_spur_immunity_level(ah,
					ah->ani_state.max_spur_level);
		ath5k_ani_set_firstep_level(ah, ATH5K_ANI_MAX_FIRSTEP_LVL);
		ath5k_ani_set_ofdm_weak_signal_detection(ah, false);
		ath5k_ani_set_cck_weak_signal_detection(ah, false);
	} else if (mode == ATH5K_ANI_MODE_AUTO) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_ANI, "ANI auto\n");
		ath5k_ani_set_noise_immunity_level(ah, 0);
		ath5k_ani_set_spur_immunity_level(ah, 0);
		ath5k_ani_set_firstep_level(ah, 0);
		ath5k_ani_set_ofdm_weak_signal_detection(ah, true);
		ath5k_ani_set_cck_weak_signal_detection(ah, false);
	}

	if (mode == ATH5K_ANI_MODE_AUTO) {
		if (ah->ah_capabilities.cap_has_phyerr_counters)
			ath5k_enable_phy_err_counters(ah);
		else
			ath5k_hw_set_rx_filter(ah, ath5k_hw_get_rx_filter(ah) |
						   AR5K_RX_FILTER_PHYERR);
	} else {
		if (ah->ah_capabilities.cap_has_phyerr_counters)
			ath5k_disable_phy_err_counters(ah);
		else
			ath5k_hw_set_rx_filter(ah, ath5k_hw_get_rx_filter(ah) &
						   ~AR5K_RX_FILTER_PHYERR);
	}

	ah->ani_state.ani_mode = mode;
}



#ifdef CONFIG_ATH5K_DEBUG

void
ath5k_ani_print_counters(struct ath5k_hw *ah)
{
	
	printk(KERN_NOTICE "ACK fail\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_ACK_FAIL));
	printk(KERN_NOTICE "RTS fail\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_RTS_FAIL));
	printk(KERN_NOTICE "RTS success\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_RTS_OK));
	printk(KERN_NOTICE "FCS error\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_FCS_FAIL));

	
	printk(KERN_NOTICE "tx\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_PROFCNT_TX));
	printk(KERN_NOTICE "rx\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_PROFCNT_RX));
	printk(KERN_NOTICE "busy\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_PROFCNT_RXCLR));
	printk(KERN_NOTICE "cycles\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_PROFCNT_CYCLE));

	printk(KERN_NOTICE "AR5K_PHYERR_CNT1\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_PHYERR_CNT1));
	printk(KERN_NOTICE "AR5K_PHYERR_CNT2\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_PHYERR_CNT2));
	printk(KERN_NOTICE "AR5K_OFDM_FIL_CNT\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_OFDM_FIL_CNT));
	printk(KERN_NOTICE "AR5K_CCK_FIL_CNT\t%d\n",
		ath5k_hw_reg_read(ah, AR5K_CCK_FIL_CNT));
}

#endif
