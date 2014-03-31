/*
 * Copyright (c) 2004-2007 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2009 Nick Kossifidis <mickflemm@gmail.com>
 * Copyright (c) 2007-2008 Jiri Slaby <jirislaby@gmail.com>
 * Copyright (c) 2008-2009 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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
 *
 */


#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include "ath5k.h"
#include "reg.h"
#include "rfbuffer.h"
#include "rfgain.h"
#include "../regd.h"





u16
ath5k_hw_radio_revision(struct ath5k_hw *ah, enum ieee80211_band band)
{
	unsigned int i;
	u32 srev;
	u16 ret;

	switch (band) {
	case IEEE80211_BAND_2GHZ:
		ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_2GHZ, AR5K_PHY(0));
		break;
	case IEEE80211_BAND_5GHZ:
		ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_5GHZ, AR5K_PHY(0));
		break;
	default:
		return 0;
	}

	usleep_range(2000, 2500);

	
	ath5k_hw_reg_write(ah, 0x00001c16, AR5K_PHY(0x34));

	for (i = 0; i < 8; i++)
		ath5k_hw_reg_write(ah, 0x00010000, AR5K_PHY(0x20));

	if (ah->ah_version == AR5K_AR5210) {
		srev = ath5k_hw_reg_read(ah, AR5K_PHY(256) >> 28) & 0xf;
		ret = (u16)ath5k_hw_bitswap(srev, 4) + 1;
	} else {
		srev = (ath5k_hw_reg_read(ah, AR5K_PHY(0x100)) >> 24) & 0xff;
		ret = (u16)ath5k_hw_bitswap(((srev & 0xf0) >> 4) |
				((srev & 0x0f) << 4), 8);
	}

	
	ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_5GHZ, AR5K_PHY(0));

	return ret;
}

bool
ath5k_channel_ok(struct ath5k_hw *ah, struct ieee80211_channel *channel)
{
	u16 freq = channel->center_freq;

	
	if (channel->band == IEEE80211_BAND_2GHZ) {
		if ((freq >= ah->ah_capabilities.cap_range.range_2ghz_min) &&
		    (freq <= ah->ah_capabilities.cap_range.range_2ghz_max))
			return true;
	} else if (channel->band == IEEE80211_BAND_5GHZ)
		if ((freq >= ah->ah_capabilities.cap_range.range_5ghz_min) &&
		    (freq <= ah->ah_capabilities.cap_range.range_5ghz_max))
			return true;

	return false;
}

bool
ath5k_hw_chan_has_spur_noise(struct ath5k_hw *ah,
				struct ieee80211_channel *channel)
{
	u8 refclk_freq;

	if ((ah->ah_radio == AR5K_RF5112) ||
	(ah->ah_radio == AR5K_RF5413) ||
	(ah->ah_radio == AR5K_RF2413) ||
	(ah->ah_mac_version == (AR5K_SREV_AR2417 >> 4)))
		refclk_freq = 40;
	else
		refclk_freq = 32;

	if ((channel->center_freq % refclk_freq != 0) &&
	((channel->center_freq % refclk_freq < 10) ||
	(channel->center_freq % refclk_freq > 22)))
		return true;
	else
		return false;
}

static unsigned int
ath5k_hw_rfb_op(struct ath5k_hw *ah, const struct ath5k_rf_reg *rf_regs,
					u32 val, u8 reg_id, bool set)
{
	const struct ath5k_rf_reg *rfreg = NULL;
	u8 offset, bank, num_bits, col, position;
	u16 entry;
	u32 mask, data, last_bit, bits_shifted, first_bit;
	u32 *rfb;
	s32 bits_left;
	int i;

	data = 0;
	rfb = ah->ah_rf_banks;

	for (i = 0; i < ah->ah_rf_regs_count; i++) {
		if (rf_regs[i].index == reg_id) {
			rfreg = &rf_regs[i];
			break;
		}
	}

	if (rfb == NULL || rfreg == NULL) {
		ATH5K_PRINTF("Rf register not found!\n");
		
		return 0;
	}

	bank = rfreg->bank;
	num_bits = rfreg->field.len;
	first_bit = rfreg->field.pos;
	col = rfreg->field.col;

	offset = ah->ah_offset[bank];

	
	if (!(col <= 3 && num_bits <= 32 && first_bit + num_bits <= 319)) {
		ATH5K_PRINTF("invalid values at offset %u\n", offset);
		return 0;
	}

	entry = ((first_bit - 1) / 8) + offset;
	position = (first_bit - 1) % 8;

	if (set)
		data = ath5k_hw_bitswap(val, num_bits);

	for (bits_shifted = 0, bits_left = num_bits; bits_left > 0;
	     position = 0, entry++) {

		last_bit = (position + bits_left > 8) ? 8 :
					position + bits_left;

		mask = (((1 << last_bit) - 1) ^ ((1 << position) - 1)) <<
								(col * 8);

		if (set) {
			rfb[entry] &= ~mask;
			rfb[entry] |= ((data << position) << (col * 8)) & mask;
			data >>= (8 - position);
		} else {
			data |= (((rfb[entry] & mask) >> (col * 8)) >> position)
				<< bits_shifted;
			bits_shifted += last_bit - position;
		}

		bits_left -= 8 - position;
	}

	data = set ? 1 : ath5k_hw_bitswap(data, num_bits);

	return data;
}

static inline int
ath5k_hw_write_ofdm_timings(struct ath5k_hw *ah,
				struct ieee80211_channel *channel)
{
	
	u32 coef_scaled, coef_exp, coef_man,
		ds_coef_exp, ds_coef_man, clock;

	BUG_ON(!(ah->ah_version == AR5K_AR5212) ||
		(channel->hw_value == AR5K_MODE_11B));

	switch (ah->ah_bwmode) {
	case AR5K_BWMODE_40MHZ:
		clock = 40 * 2;
		break;
	case AR5K_BWMODE_10MHZ:
		clock = 40 / 2;
		break;
	case AR5K_BWMODE_5MHZ:
		clock = 40 / 4;
		break;
	default:
		clock = 40;
		break;
	}
	coef_scaled = ((5 * (clock << 24)) / 2) / channel->center_freq;

	coef_exp = ilog2(coef_scaled);

	
	if (!coef_scaled || !coef_exp)
		return -EINVAL;

	
	coef_exp = 14 - (coef_exp - 24);


	coef_man = coef_scaled +
		(1 << (24 - coef_exp - 1));

	ds_coef_man = coef_man >> (24 - coef_exp);
	ds_coef_exp = coef_exp - 16;

	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_TIMING_3,
		AR5K_PHY_TIMING_3_DSC_MAN, ds_coef_man);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_TIMING_3,
		AR5K_PHY_TIMING_3_DSC_EXP, ds_coef_exp);

	return 0;
}

int ath5k_hw_phy_disable(struct ath5k_hw *ah)
{
	
	ath5k_hw_reg_write(ah, AR5K_PHY_ACT_DISABLE, AR5K_PHY_ACT);

	return 0;
}

static void
ath5k_hw_wait_for_synth(struct ath5k_hw *ah,
			struct ieee80211_channel *channel)
{
	if (ah->ah_version != AR5K_AR5210) {
		u32 delay;
		delay = ath5k_hw_reg_read(ah, AR5K_PHY_RX_DELAY) &
			AR5K_PHY_RX_DELAY_M;
		delay = (channel->hw_value == AR5K_MODE_11B) ?
			((delay << 2) / 22) : (delay / 10);
		if (ah->ah_bwmode == AR5K_BWMODE_10MHZ)
			delay = delay << 1;
		if (ah->ah_bwmode == AR5K_BWMODE_5MHZ)
			delay = delay << 2;
		usleep_range(100 + delay, 100 + (2 * delay));
	} else {
		usleep_range(1000, 1500);
	}
}




int ath5k_hw_rfgain_opt_init(struct ath5k_hw *ah)
{
	
	switch (ah->ah_radio) {
	case AR5K_RF5111:
		ah->ah_gain.g_step_idx = rfgain_opt_5111.go_default;
		ah->ah_gain.g_low = 20;
		ah->ah_gain.g_high = 35;
		ah->ah_gain.g_state = AR5K_RFGAIN_ACTIVE;
		break;
	case AR5K_RF5112:
		ah->ah_gain.g_step_idx = rfgain_opt_5112.go_default;
		ah->ah_gain.g_low = 20;
		ah->ah_gain.g_high = 85;
		ah->ah_gain.g_state = AR5K_RFGAIN_ACTIVE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void
ath5k_hw_request_rfgain_probe(struct ath5k_hw *ah)
{

	if (ah->ah_gain.g_state != AR5K_RFGAIN_ACTIVE)
		return;

	ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txpower.txp_ofdm - 4,
			AR5K_PHY_PAPD_PROBE_TXPOWER) |
			AR5K_PHY_PAPD_PROBE_TX_NEXT, AR5K_PHY_PAPD_PROBE);

	ah->ah_gain.g_state = AR5K_RFGAIN_READ_REQUESTED;

}

static u32
ath5k_hw_rf_gainf_corr(struct ath5k_hw *ah)
{
	u32 mix, step;
	u32 *rf;
	const struct ath5k_gain_opt *go;
	const struct ath5k_gain_opt_step *g_step;
	const struct ath5k_rf_reg *rf_regs;

	
	if ((ah->ah_radio != AR5K_RF5112) ||
	(ah->ah_radio_5ghz_revision <= AR5K_SREV_RAD_5112A))
		return 0;

	go = &rfgain_opt_5112;
	rf_regs = rf_regs_5112a;
	ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5112a);

	g_step = &go->go_step[ah->ah_gain.g_step_idx];

	if (ah->ah_rf_banks == NULL)
		return 0;

	rf = ah->ah_rf_banks;
	ah->ah_gain.g_f_corr = 0;

	
	if (ath5k_hw_rfb_op(ah, rf_regs, 0, AR5K_RF_MIXVGA_OVR, false) != 1)
		return 0;

	
	step = ath5k_hw_rfb_op(ah, rf_regs, 0, AR5K_RF_MIXGAIN_STEP, false);

	
	mix = g_step->gos_param[0];

	switch (mix) {
	case 3:
		ah->ah_gain.g_f_corr = step * 2;
		break;
	case 2:
		ah->ah_gain.g_f_corr = (step - 5) * 2;
		break;
	case 1:
		ah->ah_gain.g_f_corr = step;
		break;
	default:
		ah->ah_gain.g_f_corr = 0;
		break;
	}

	return ah->ah_gain.g_f_corr;
}

static bool
ath5k_hw_rf_check_gainf_readback(struct ath5k_hw *ah)
{
	const struct ath5k_rf_reg *rf_regs;
	u32 step, mix_ovr, level[4];
	u32 *rf;

	if (ah->ah_rf_banks == NULL)
		return false;

	rf = ah->ah_rf_banks;

	if (ah->ah_radio == AR5K_RF5111) {

		rf_regs = rf_regs_5111;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5111);

		step = ath5k_hw_rfb_op(ah, rf_regs, 0, AR5K_RF_RFGAIN_STEP,
			false);

		level[0] = 0;
		level[1] = (step == 63) ? 50 : step + 4;
		level[2] = (step != 63) ? 64 : level[0];
		level[3] = level[2] + 50;

		ah->ah_gain.g_high = level[3] -
			(step == 63 ? AR5K_GAIN_DYN_ADJUST_HI_MARGIN : -5);
		ah->ah_gain.g_low = level[0] +
			(step == 63 ? AR5K_GAIN_DYN_ADJUST_LO_MARGIN : 0);
	} else {

		rf_regs = rf_regs_5112;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5112);

		mix_ovr = ath5k_hw_rfb_op(ah, rf_regs, 0, AR5K_RF_MIXVGA_OVR,
			false);

		level[0] = level[2] = 0;

		if (mix_ovr == 1) {
			level[1] = level[3] = 83;
		} else {
			level[1] = level[3] = 107;
			ah->ah_gain.g_high = 55;
		}
	}

	return (ah->ah_gain.g_current >= level[0] &&
			ah->ah_gain.g_current <= level[1]) ||
		(ah->ah_gain.g_current >= level[2] &&
			ah->ah_gain.g_current <= level[3]);
}

static s8
ath5k_hw_rf_gainf_adjust(struct ath5k_hw *ah)
{
	const struct ath5k_gain_opt *go;
	const struct ath5k_gain_opt_step *g_step;
	int ret = 0;

	switch (ah->ah_radio) {
	case AR5K_RF5111:
		go = &rfgain_opt_5111;
		break;
	case AR5K_RF5112:
		go = &rfgain_opt_5112;
		break;
	default:
		return 0;
	}

	g_step = &go->go_step[ah->ah_gain.g_step_idx];

	if (ah->ah_gain.g_current >= ah->ah_gain.g_high) {

		
		if (ah->ah_gain.g_step_idx == 0)
			return -1;

		for (ah->ah_gain.g_target = ah->ah_gain.g_current;
				ah->ah_gain.g_target >=  ah->ah_gain.g_high &&
				ah->ah_gain.g_step_idx > 0;
				g_step = &go->go_step[ah->ah_gain.g_step_idx])
			ah->ah_gain.g_target -= 2 *
			    (go->go_step[--(ah->ah_gain.g_step_idx)].gos_gain -
			    g_step->gos_gain);

		ret = 1;
		goto done;
	}

	if (ah->ah_gain.g_current <= ah->ah_gain.g_low) {

		
		if (ah->ah_gain.g_step_idx == (go->go_steps_count - 1))
			return -2;

		for (ah->ah_gain.g_target = ah->ah_gain.g_current;
				ah->ah_gain.g_target <= ah->ah_gain.g_low &&
				ah->ah_gain.g_step_idx < go->go_steps_count - 1;
				g_step = &go->go_step[ah->ah_gain.g_step_idx])
			ah->ah_gain.g_target -= 2 *
			    (go->go_step[++ah->ah_gain.g_step_idx].gos_gain -
			    g_step->gos_gain);

		ret = 2;
		goto done;
	}

done:
	ATH5K_DBG(ah, ATH5K_DEBUG_CALIBRATE,
		"ret %d, gain step %u, current gain %u, target gain %u\n",
		ret, ah->ah_gain.g_step_idx, ah->ah_gain.g_current,
		ah->ah_gain.g_target);

	return ret;
}

enum ath5k_rfgain
ath5k_hw_gainf_calibrate(struct ath5k_hw *ah)
{
	u32 data, type;
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;

	if (ah->ah_rf_banks == NULL ||
	ah->ah_gain.g_state == AR5K_RFGAIN_INACTIVE)
		return AR5K_RFGAIN_INACTIVE;

	if (ah->ah_gain.g_state != AR5K_RFGAIN_READ_REQUESTED)
		goto done;

	data = ath5k_hw_reg_read(ah, AR5K_PHY_PAPD_PROBE);

	
	if (!(data & AR5K_PHY_PAPD_PROBE_TX_NEXT)) {
		ah->ah_gain.g_current = data >> AR5K_PHY_PAPD_PROBE_GAINF_S;
		type = AR5K_REG_MS(data, AR5K_PHY_PAPD_PROBE_TYPE);

		if (type == AR5K_PHY_PAPD_PROBE_TYPE_CCK) {
			if (ah->ah_radio_5ghz_revision >= AR5K_SREV_RAD_5112A)
				ah->ah_gain.g_current +=
					ee->ee_cck_ofdm_gain_delta;
			else
				ah->ah_gain.g_current +=
					AR5K_GAIN_CCK_PROBE_CORR;
		}

		if (ah->ah_radio_5ghz_revision >= AR5K_SREV_RAD_5112A) {
			ath5k_hw_rf_gainf_corr(ah);
			ah->ah_gain.g_current =
				ah->ah_gain.g_current >= ah->ah_gain.g_f_corr ?
				(ah->ah_gain.g_current - ah->ah_gain.g_f_corr) :
				0;
		}

		if (ath5k_hw_rf_check_gainf_readback(ah) &&
		AR5K_GAIN_CHECK_ADJUST(&ah->ah_gain) &&
		ath5k_hw_rf_gainf_adjust(ah)) {
			ah->ah_gain.g_state = AR5K_RFGAIN_NEED_CHANGE;
		} else {
			ah->ah_gain.g_state = AR5K_RFGAIN_ACTIVE;
		}
	}

done:
	return ah->ah_gain.g_state;
}

static int
ath5k_hw_rfgain_init(struct ath5k_hw *ah, enum ieee80211_band band)
{
	const struct ath5k_ini_rfgain *ath5k_rfg;
	unsigned int i, size, index;

	switch (ah->ah_radio) {
	case AR5K_RF5111:
		ath5k_rfg = rfgain_5111;
		size = ARRAY_SIZE(rfgain_5111);
		break;
	case AR5K_RF5112:
		ath5k_rfg = rfgain_5112;
		size = ARRAY_SIZE(rfgain_5112);
		break;
	case AR5K_RF2413:
		ath5k_rfg = rfgain_2413;
		size = ARRAY_SIZE(rfgain_2413);
		break;
	case AR5K_RF2316:
		ath5k_rfg = rfgain_2316;
		size = ARRAY_SIZE(rfgain_2316);
		break;
	case AR5K_RF5413:
		ath5k_rfg = rfgain_5413;
		size = ARRAY_SIZE(rfgain_5413);
		break;
	case AR5K_RF2317:
	case AR5K_RF2425:
		ath5k_rfg = rfgain_2425;
		size = ARRAY_SIZE(rfgain_2425);
		break;
	default:
		return -EINVAL;
	}

	index = (band == IEEE80211_BAND_2GHZ) ? 1 : 0;

	for (i = 0; i < size; i++) {
		AR5K_REG_WAIT(i);
		ath5k_hw_reg_write(ah, ath5k_rfg[i].rfg_value[index],
			(u32)ath5k_rfg[i].rfg_register);
	}

	return 0;
}



static int
ath5k_hw_rfregs_init(struct ath5k_hw *ah,
			struct ieee80211_channel *channel,
			unsigned int mode)
{
	const struct ath5k_rf_reg *rf_regs;
	const struct ath5k_ini_rfbuffer *ini_rfb;
	const struct ath5k_gain_opt *go = NULL;
	const struct ath5k_gain_opt_step *g_step;
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u8 ee_mode = 0;
	u32 *rfb;
	int i, obdb = -1, bank = -1;

	switch (ah->ah_radio) {
	case AR5K_RF5111:
		rf_regs = rf_regs_5111;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5111);
		ini_rfb = rfb_5111;
		ah->ah_rf_banks_size = ARRAY_SIZE(rfb_5111);
		go = &rfgain_opt_5111;
		break;
	case AR5K_RF5112:
		if (ah->ah_radio_5ghz_revision >= AR5K_SREV_RAD_5112A) {
			rf_regs = rf_regs_5112a;
			ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5112a);
			ini_rfb = rfb_5112a;
			ah->ah_rf_banks_size = ARRAY_SIZE(rfb_5112a);
		} else {
			rf_regs = rf_regs_5112;
			ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5112);
			ini_rfb = rfb_5112;
			ah->ah_rf_banks_size = ARRAY_SIZE(rfb_5112);
		}
		go = &rfgain_opt_5112;
		break;
	case AR5K_RF2413:
		rf_regs = rf_regs_2413;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_2413);
		ini_rfb = rfb_2413;
		ah->ah_rf_banks_size = ARRAY_SIZE(rfb_2413);
		break;
	case AR5K_RF2316:
		rf_regs = rf_regs_2316;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_2316);
		ini_rfb = rfb_2316;
		ah->ah_rf_banks_size = ARRAY_SIZE(rfb_2316);
		break;
	case AR5K_RF5413:
		rf_regs = rf_regs_5413;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_5413);
		ini_rfb = rfb_5413;
		ah->ah_rf_banks_size = ARRAY_SIZE(rfb_5413);
		break;
	case AR5K_RF2317:
		rf_regs = rf_regs_2425;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_2425);
		ini_rfb = rfb_2317;
		ah->ah_rf_banks_size = ARRAY_SIZE(rfb_2317);
		break;
	case AR5K_RF2425:
		rf_regs = rf_regs_2425;
		ah->ah_rf_regs_count = ARRAY_SIZE(rf_regs_2425);
		if (ah->ah_mac_srev < AR5K_SREV_AR2417) {
			ini_rfb = rfb_2425;
			ah->ah_rf_banks_size = ARRAY_SIZE(rfb_2425);
		} else {
			ini_rfb = rfb_2417;
			ah->ah_rf_banks_size = ARRAY_SIZE(rfb_2417);
		}
		break;
	default:
		return -EINVAL;
	}

	if (ah->ah_rf_banks == NULL) {
		ah->ah_rf_banks = kmalloc(sizeof(u32) * ah->ah_rf_banks_size,
								GFP_KERNEL);
		if (ah->ah_rf_banks == NULL) {
			ATH5K_ERR(ah, "out of memory\n");
			return -ENOMEM;
		}
	}

	
	rfb = ah->ah_rf_banks;

	for (i = 0; i < ah->ah_rf_banks_size; i++) {
		if (ini_rfb[i].rfb_bank >= AR5K_MAX_RF_BANKS) {
			ATH5K_ERR(ah, "invalid bank\n");
			return -EINVAL;
		}

		
		if (bank != ini_rfb[i].rfb_bank) {
			bank = ini_rfb[i].rfb_bank;
			ah->ah_offset[bank] = i;
		}

		rfb[i] = ini_rfb[i].rfb_mode_data[mode];
	}

	
	if (channel->band == IEEE80211_BAND_2GHZ) {

		if (channel->hw_value == AR5K_MODE_11B)
			ee_mode = AR5K_EEPROM_MODE_11B;
		else
			ee_mode = AR5K_EEPROM_MODE_11G;

		if ((ah->ah_radio == AR5K_RF5111) ||
		(ah->ah_radio == AR5K_RF5112))
			obdb = 0;
		else
			obdb = 1;

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_ob[ee_mode][obdb],
						AR5K_RF_OB_2GHZ, true);

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_db[ee_mode][obdb],
						AR5K_RF_DB_2GHZ, true);

	
	} else if ((channel->band == IEEE80211_BAND_5GHZ) ||
			(ah->ah_radio == AR5K_RF5111)) {

		ee_mode = AR5K_EEPROM_MODE_11A;
		obdb =	 channel->center_freq >= 5725 ? 3 :
			(channel->center_freq >= 5500 ? 2 :
			(channel->center_freq >= 5260 ? 1 :
			 (channel->center_freq > 4000 ? 0 : -1)));

		if (obdb < 0)
			return -EINVAL;

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_ob[ee_mode][obdb],
						AR5K_RF_OB_5GHZ, true);

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_db[ee_mode][obdb],
						AR5K_RF_DB_5GHZ, true);
	}

	g_step = &go->go_step[ah->ah_gain.g_step_idx];

	
	if ((ah->ah_bwmode == AR5K_BWMODE_40MHZ) &&
	(ah->ah_radio != AR5K_RF5413))
		ath5k_hw_rfb_op(ah, rf_regs, 1, AR5K_RF_TURBO, false);

	
	if (ah->ah_radio == AR5K_RF5111) {

		
		if (channel->hw_value != AR5K_MODE_11B) {

			AR5K_REG_WRITE_BITS(ah, AR5K_PHY_FRAME_CTL,
					AR5K_PHY_FRAME_CTL_TX_CLIP,
					g_step->gos_param[0]);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[1],
							AR5K_RF_PWD_90, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[2],
							AR5K_RF_PWD_84, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[3],
						AR5K_RF_RFGAIN_SEL, true);

			ah->ah_gain.g_state = AR5K_RFGAIN_ACTIVE;

		}

		

		ath5k_hw_rfb_op(ah, rf_regs, !ee->ee_xpd[ee_mode],
						AR5K_RF_PWD_XPD, true);

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_x_gain[ee_mode],
						AR5K_RF_XPD_GAIN, true);

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_i_gain[ee_mode],
						AR5K_RF_GAIN_I, true);

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_xpd[ee_mode],
						AR5K_RF_PLO_SEL, true);

		
		if (ah->ah_bwmode == AR5K_BWMODE_5MHZ ||
		ah->ah_bwmode == AR5K_BWMODE_10MHZ) {
			u8 wait_i;

			ath5k_hw_rfb_op(ah, rf_regs, 0x1f,
						AR5K_RF_WAIT_S, true);

			wait_i = (ah->ah_bwmode == AR5K_BWMODE_5MHZ) ?
							0x1f : 0x10;

			ath5k_hw_rfb_op(ah, rf_regs, wait_i,
						AR5K_RF_WAIT_I, true);
			ath5k_hw_rfb_op(ah, rf_regs, 3,
						AR5K_RF_MAX_TIME, true);

		}
	}

	if (ah->ah_radio == AR5K_RF5112) {

		
		if (channel->hw_value != AR5K_MODE_11B) {

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[0],
						AR5K_RF_MIXGAIN_OVR, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[1],
						AR5K_RF_PWD_138, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[2],
						AR5K_RF_PWD_137, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[3],
						AR5K_RF_PWD_136, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[4],
						AR5K_RF_PWD_132, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[5],
						AR5K_RF_PWD_131, true);

			ath5k_hw_rfb_op(ah, rf_regs, g_step->gos_param[6],
						AR5K_RF_PWD_130, true);

			ah->ah_gain.g_state = AR5K_RFGAIN_ACTIVE;
		}

		

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_xpd[ee_mode],
						AR5K_RF_XPD_SEL, true);

		if (ah->ah_radio_5ghz_revision < AR5K_SREV_RAD_5112A) {
			
			ath5k_hw_rfb_op(ah, rf_regs,
						ee->ee_x_gain[ee_mode],
						AR5K_RF_XPD_GAIN, true);

		} else {
			u8 *pdg_curve_to_idx = ee->ee_pdc_to_idx[ee_mode];
			if (ee->ee_pd_gains[ee_mode] > 1) {
				ath5k_hw_rfb_op(ah, rf_regs,
						pdg_curve_to_idx[0],
						AR5K_RF_PD_GAIN_LO, true);
				ath5k_hw_rfb_op(ah, rf_regs,
						pdg_curve_to_idx[1],
						AR5K_RF_PD_GAIN_HI, true);
			} else {
				ath5k_hw_rfb_op(ah, rf_regs,
						pdg_curve_to_idx[0],
						AR5K_RF_PD_GAIN_LO, true);
				ath5k_hw_rfb_op(ah, rf_regs,
						pdg_curve_to_idx[0],
						AR5K_RF_PD_GAIN_HI, true);
			}

			
			if (ah->ah_radio == AR5K_RF5112 &&
			    (ah->ah_radio_5ghz_revision & AR5K_SREV_REV) > 0) {
				ath5k_hw_rfb_op(ah, rf_regs, 2,
						AR5K_RF_HIGH_VC_CP, true);

				ath5k_hw_rfb_op(ah, rf_regs, 2,
						AR5K_RF_MID_VC_CP, true);

				ath5k_hw_rfb_op(ah, rf_regs, 2,
						AR5K_RF_LOW_VC_CP, true);

				ath5k_hw_rfb_op(ah, rf_regs, 2,
						AR5K_RF_PUSH_UP, true);
			}

			
			if (ah->ah_phy_revision >= AR5K_SREV_PHY_5212A) {
				ath5k_hw_rfb_op(ah, rf_regs, 1,
						AR5K_RF_PAD2GND, true);

				ath5k_hw_rfb_op(ah, rf_regs, 1,
						AR5K_RF_XB2_LVL, true);

				ath5k_hw_rfb_op(ah, rf_regs, 1,
						AR5K_RF_XB5_LVL, true);

				ath5k_hw_rfb_op(ah, rf_regs, 1,
						AR5K_RF_PWD_167, true);

				ath5k_hw_rfb_op(ah, rf_regs, 1,
						AR5K_RF_PWD_166, true);
			}
		}

		ath5k_hw_rfb_op(ah, rf_regs, ee->ee_i_gain[ee_mode],
						AR5K_RF_GAIN_I, true);

		
		if (ah->ah_bwmode == AR5K_BWMODE_5MHZ ||
		ah->ah_bwmode == AR5K_BWMODE_10MHZ) {
			u8 pd_delay;

			pd_delay = (ah->ah_bwmode == AR5K_BWMODE_5MHZ) ?
							0xf : 0x8;

			ath5k_hw_rfb_op(ah, rf_regs, pd_delay,
						AR5K_RF_PD_PERIOD_A, true);
			ath5k_hw_rfb_op(ah, rf_regs, 0xf,
						AR5K_RF_PD_DELAY_A, true);

		}
	}

	if (ah->ah_radio == AR5K_RF5413 &&
	channel->band == IEEE80211_BAND_2GHZ) {

		ath5k_hw_rfb_op(ah, rf_regs, 1, AR5K_RF_DERBY_CHAN_SEL_MODE,
									true);

		
		if (ah->ah_mac_srev >= AR5K_SREV_AR5424 &&
		ah->ah_mac_srev < AR5K_SREV_AR5413)
			ath5k_hw_rfb_op(ah, rf_regs, ath5k_hw_bitswap(6, 3),
						AR5K_RF_PWD_ICLOBUF_2G, true);

	}

	
	for (i = 0; i < ah->ah_rf_banks_size; i++) {
		AR5K_REG_WAIT(i);
		ath5k_hw_reg_write(ah, rfb[i], ini_rfb[i].rfb_ctrl_register);
	}

	return 0;
}



static u32
ath5k_hw_rf5110_chan2athchan(struct ieee80211_channel *channel)
{
	u32 athchan;

	athchan = (ath5k_hw_bitswap(
			(ieee80211_frequency_to_channel(
				channel->center_freq) - 24) / 2, 5)
				<< 1) | (1 << 6) | 0x1;
	return athchan;
}

static int
ath5k_hw_rf5110_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 data;

	data = ath5k_hw_rf5110_chan2athchan(channel);
	ath5k_hw_reg_write(ah, data, AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, 0, AR5K_RF_BUFFER_CONTROL_0);
	usleep_range(1000, 1500);

	return 0;
}

static int
ath5k_hw_rf5111_chan2athchan(unsigned int ieee,
		struct ath5k_athchan_2ghz *athchan)
{
	int channel;

	
	channel = (int)ieee;

	if (channel <= 13) {
		athchan->a2_athchan = 115 + channel;
		athchan->a2_flags = 0x46;
	} else if (channel == 14) {
		athchan->a2_athchan = 124;
		athchan->a2_flags = 0x44;
	} else if (channel >= 15 && channel <= 26) {
		athchan->a2_athchan = ((channel - 14) * 4) + 132;
		athchan->a2_flags = 0x46;
	} else
		return -EINVAL;

	return 0;
}

static int
ath5k_hw_rf5111_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	struct ath5k_athchan_2ghz ath5k_channel_2ghz;
	unsigned int ath5k_channel =
		ieee80211_frequency_to_channel(channel->center_freq);
	u32 data0, data1, clock;
	int ret;

	data0 = data1 = 0;

	if (channel->band == IEEE80211_BAND_2GHZ) {
		
		ret = ath5k_hw_rf5111_chan2athchan(
			ieee80211_frequency_to_channel(channel->center_freq),
			&ath5k_channel_2ghz);
		if (ret)
			return ret;

		ath5k_channel = ath5k_channel_2ghz.a2_athchan;
		data0 = ((ath5k_hw_bitswap(ath5k_channel_2ghz.a2_flags, 8) & 0xff)
		    << 5) | (1 << 4);
	}

	if (ath5k_channel < 145 || !(ath5k_channel & 1)) {
		clock = 1;
		data1 = ((ath5k_hw_bitswap(ath5k_channel - 24, 8) & 0xff) << 2) |
			(clock << 1) | (1 << 10) | 1;
	} else {
		clock = 0;
		data1 = ((ath5k_hw_bitswap((ath5k_channel - 24) / 2, 8) & 0xff)
			<< 2) | (clock << 1) | (1 << 10) | 1;
	}

	ath5k_hw_reg_write(ah, (data1 & 0xff) | ((data0 & 0xff) << 8),
			AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, ((data1 >> 8) & 0xff) | (data0 & 0xff00),
			AR5K_RF_BUFFER_CONTROL_3);

	return 0;
}

static int
ath5k_hw_rf5112_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 data, data0, data1, data2;
	u16 c;

	data = data0 = data1 = data2 = 0;
	c = channel->center_freq;

	if (c < 4800) {
		if (!((c - 2224) % 5)) {
			
			data0 = ((2 * (c - 704)) - 3040) / 10;
			data1 = 1;
		} else if (!((c - 2192) % 5)) {
			
			data0 = ((2 * (c - 672)) - 3040) / 10;
			data1 = 0;
		} else
			return -EINVAL;

		data0 = ath5k_hw_bitswap((data0 << 2) & 0xff, 8);
	} else if ((c % 5) != 2 || c > 5435) {
		if (!(c % 20) && c >= 5120) {
			data0 = ath5k_hw_bitswap(((c - 4800) / 20 << 2), 8);
			data2 = ath5k_hw_bitswap(3, 2);
		} else if (!(c % 10)) {
			data0 = ath5k_hw_bitswap(((c - 4800) / 10 << 1), 8);
			data2 = ath5k_hw_bitswap(2, 2);
		} else if (!(c % 5)) {
			data0 = ath5k_hw_bitswap((c - 4800) / 5, 8);
			data2 = ath5k_hw_bitswap(1, 2);
		} else
			return -EINVAL;
	} else {
		data0 = ath5k_hw_bitswap((10 * (c - 2 - 4800)) / 25 + 1, 8);
		data2 = ath5k_hw_bitswap(0, 2);
	}

	data = (data0 << 4) | (data1 << 1) | (data2 << 2) | 0x1001;

	ath5k_hw_reg_write(ah, data & 0xff, AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, (data >> 8) & 0x7f, AR5K_RF_BUFFER_CONTROL_5);

	return 0;
}

static int
ath5k_hw_rf2425_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 data, data0, data2;
	u16 c;

	data = data0 = data2 = 0;
	c = channel->center_freq;

	if (c < 4800) {
		data0 = ath5k_hw_bitswap((c - 2272), 8);
		data2 = 0;
	
	} else if ((c % 5) != 2 || c > 5435) {
		if (!(c % 20) && c < 5120)
			data0 = ath5k_hw_bitswap(((c - 4800) / 20 << 2), 8);
		else if (!(c % 10))
			data0 = ath5k_hw_bitswap(((c - 4800) / 10 << 1), 8);
		else if (!(c % 5))
			data0 = ath5k_hw_bitswap((c - 4800) / 5, 8);
		else
			return -EINVAL;
		data2 = ath5k_hw_bitswap(1, 2);
	} else {
		data0 = ath5k_hw_bitswap((10 * (c - 2 - 4800)) / 25 + 1, 8);
		data2 = ath5k_hw_bitswap(0, 2);
	}

	data = (data0 << 4) | data2 << 2 | 0x1001;

	ath5k_hw_reg_write(ah, data & 0xff, AR5K_RF_BUFFER);
	ath5k_hw_reg_write(ah, (data >> 8) & 0x7f, AR5K_RF_BUFFER_CONTROL_5);

	return 0;
}

static int
ath5k_hw_channel(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	int ret;
	if (!ath5k_channel_ok(ah, channel)) {
		ATH5K_ERR(ah,
			"channel frequency (%u MHz) out of supported "
			"band range\n",
			channel->center_freq);
			return -EINVAL;
	}

	switch (ah->ah_radio) {
	case AR5K_RF5110:
		ret = ath5k_hw_rf5110_channel(ah, channel);
		break;
	case AR5K_RF5111:
		ret = ath5k_hw_rf5111_channel(ah, channel);
		break;
	case AR5K_RF2317:
	case AR5K_RF2425:
		ret = ath5k_hw_rf2425_channel(ah, channel);
		break;
	default:
		ret = ath5k_hw_rf5112_channel(ah, channel);
		break;
	}

	if (ret)
		return ret;

	
	if (channel->center_freq == 2484) {
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_CCKTXCTL,
				AR5K_PHY_CCKTXCTL_JAPAN);
	} else {
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_CCKTXCTL,
				AR5K_PHY_CCKTXCTL_WORLD);
	}

	ah->ah_current_channel = channel;

	return 0;
}




static s32
ath5k_hw_read_measured_noise_floor(struct ath5k_hw *ah)
{
	s32 val;

	val = ath5k_hw_reg_read(ah, AR5K_PHY_NF);
	return sign_extend32(AR5K_REG_MS(val, AR5K_PHY_NF_MINCCA_PWR), 8);
}

void
ath5k_hw_init_nfcal_hist(struct ath5k_hw *ah)
{
	int i;

	ah->ah_nfcal_hist.index = 0;
	for (i = 0; i < ATH5K_NF_CAL_HIST_MAX; i++)
		ah->ah_nfcal_hist.nfval[i] = AR5K_TUNE_CCA_MAX_GOOD_VALUE;
}

static void ath5k_hw_update_nfcal_hist(struct ath5k_hw *ah, s16 noise_floor)
{
	struct ath5k_nfcal_hist *hist = &ah->ah_nfcal_hist;
	hist->index = (hist->index + 1) & (ATH5K_NF_CAL_HIST_MAX - 1);
	hist->nfval[hist->index] = noise_floor;
}

static s16
ath5k_hw_get_median_noise_floor(struct ath5k_hw *ah)
{
	s16 sort[ATH5K_NF_CAL_HIST_MAX];
	s16 tmp;
	int i, j;

	memcpy(sort, ah->ah_nfcal_hist.nfval, sizeof(sort));
	for (i = 0; i < ATH5K_NF_CAL_HIST_MAX - 1; i++) {
		for (j = 1; j < ATH5K_NF_CAL_HIST_MAX - i; j++) {
			if (sort[j] > sort[j - 1]) {
				tmp = sort[j];
				sort[j] = sort[j - 1];
				sort[j - 1] = tmp;
			}
		}
	}
	for (i = 0; i < ATH5K_NF_CAL_HIST_MAX; i++) {
		ATH5K_DBG(ah, ATH5K_DEBUG_CALIBRATE,
			"cal %d:%d\n", i, sort[i]);
	}
	return sort[(ATH5K_NF_CAL_HIST_MAX - 1) / 2];
}

void
ath5k_hw_update_noise_floor(struct ath5k_hw *ah)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u32 val;
	s16 nf, threshold;
	u8 ee_mode;

	
	if (ath5k_hw_reg_read(ah, AR5K_PHY_AGCCTL) & AR5K_PHY_AGCCTL_NF) {
		ATH5K_DBG(ah, ATH5K_DEBUG_CALIBRATE,
			"NF did not complete in calibration window\n");

		return;
	}

	ah->ah_cal_mask |= AR5K_CALIBRATION_NF;

	ee_mode = ath5k_eeprom_mode_from_channel(ah->ah_current_channel);

	
	nf = ath5k_hw_read_measured_noise_floor(ah);
	threshold = ee->ee_noise_floor_thr[ee_mode];

	if (nf > threshold) {
		ATH5K_DBG(ah, ATH5K_DEBUG_CALIBRATE,
			"noise floor failure detected; "
			"read %d, threshold %d\n",
			nf, threshold);

		nf = AR5K_TUNE_CCA_MAX_GOOD_VALUE;
	}

	ath5k_hw_update_nfcal_hist(ah, nf);
	nf = ath5k_hw_get_median_noise_floor(ah);

	
	val = ath5k_hw_reg_read(ah, AR5K_PHY_NF) & ~AR5K_PHY_NF_M;
	val |= (nf * 2) & AR5K_PHY_NF_M;
	ath5k_hw_reg_write(ah, val, AR5K_PHY_NF);

	AR5K_REG_MASKED_BITS(ah, AR5K_PHY_AGCCTL, AR5K_PHY_AGCCTL_NF,
		~(AR5K_PHY_AGCCTL_NF_EN | AR5K_PHY_AGCCTL_NF_NOUPDATE));

	ath5k_hw_register_timeout(ah, AR5K_PHY_AGCCTL, AR5K_PHY_AGCCTL_NF,
		0, false);

	val = (val & ~AR5K_PHY_NF_M) | ((-50 * 2) & AR5K_PHY_NF_M);
	ath5k_hw_reg_write(ah, val, AR5K_PHY_NF);
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL,
		AR5K_PHY_AGCCTL_NF_EN |
		AR5K_PHY_AGCCTL_NF_NOUPDATE |
		AR5K_PHY_AGCCTL_NF);

	ah->ah_noise_floor = nf;

	ah->ah_cal_mask &= ~AR5K_CALIBRATION_NF;

	ATH5K_DBG(ah, ATH5K_DEBUG_CALIBRATE,
		"noise floor calibrated: %d\n", nf);
}

static int
ath5k_hw_rf5110_calibrate(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	u32 phy_sig, phy_agc, phy_sat, beacon;
	int ret;

	if (!(ah->ah_cal_mask & AR5K_CALIBRATION_FULL))
		return 0;

	AR5K_REG_ENABLE_BITS(ah, AR5K_DIAG_SW_5210,
		AR5K_DIAG_SW_DIS_TX_5210 | AR5K_DIAG_SW_DIS_RX_5210);
	beacon = ath5k_hw_reg_read(ah, AR5K_BEACON_5210);
	ath5k_hw_reg_write(ah, beacon & ~AR5K_BEACON_ENABLE, AR5K_BEACON_5210);

	usleep_range(2000, 2500);

	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);
	udelay(10);
	ret = ath5k_hw_channel(ah, channel);

	ath5k_hw_reg_write(ah, AR5K_PHY_ACT_ENABLE, AR5K_PHY_ACT);
	usleep_range(1000, 1500);

	AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);

	if (ret)
		return ret;


	
	phy_sig = ath5k_hw_reg_read(ah, AR5K_PHY_SIG);
	phy_agc = ath5k_hw_reg_read(ah, AR5K_PHY_AGCCOARSE);
	phy_sat = ath5k_hw_reg_read(ah, AR5K_PHY_ADCSAT);

	
	ath5k_hw_reg_write(ah, (phy_sig & ~(AR5K_PHY_SIG_FIRPWR)) |
		AR5K_REG_SM(-1, AR5K_PHY_SIG_FIRPWR), AR5K_PHY_SIG);

	ath5k_hw_reg_write(ah, (phy_agc & ~(AR5K_PHY_AGCCOARSE_HI |
			AR5K_PHY_AGCCOARSE_LO)) |
		AR5K_REG_SM(-1, AR5K_PHY_AGCCOARSE_HI) |
		AR5K_REG_SM(-127, AR5K_PHY_AGCCOARSE_LO), AR5K_PHY_AGCCOARSE);

	ath5k_hw_reg_write(ah, (phy_sat & ~(AR5K_PHY_ADCSAT_ICNT |
			AR5K_PHY_ADCSAT_THR)) |
		AR5K_REG_SM(2, AR5K_PHY_ADCSAT_ICNT) |
		AR5K_REG_SM(12, AR5K_PHY_ADCSAT_THR), AR5K_PHY_ADCSAT);

	udelay(20);

	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);
	udelay(10);
	ath5k_hw_reg_write(ah, AR5K_PHY_RFSTG_DISABLE, AR5K_PHY_RFSTG);
	AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_AGC, AR5K_PHY_AGC_DISABLE);

	usleep_range(1000, 1500);

	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL, AR5K_PHY_AGCCTL_CAL);

	ret = ath5k_hw_register_timeout(ah, AR5K_PHY_AGCCTL,
			AR5K_PHY_AGCCTL_CAL, 0, false);

	
	ath5k_hw_reg_write(ah, phy_sig, AR5K_PHY_SIG);
	ath5k_hw_reg_write(ah, phy_agc, AR5K_PHY_AGCCOARSE);
	ath5k_hw_reg_write(ah, phy_sat, AR5K_PHY_ADCSAT);

	if (ret) {
		ATH5K_ERR(ah, "calibration timeout (%uMHz)\n",
				channel->center_freq);
		return ret;
	}

	AR5K_REG_DISABLE_BITS(ah, AR5K_DIAG_SW_5210,
		AR5K_DIAG_SW_DIS_TX_5210 | AR5K_DIAG_SW_DIS_RX_5210);
	ath5k_hw_reg_write(ah, beacon, AR5K_BEACON_5210);

	return 0;
}

static int
ath5k_hw_rf511x_iq_calibrate(struct ath5k_hw *ah)
{
	u32 i_pwr, q_pwr;
	s32 iq_corr, i_coff, i_coffd, q_coff, q_coffd;
	int i;

	
	if (!ah->ah_iq_cal_needed)
		return -EINVAL;
	else if (ath5k_hw_reg_read(ah, AR5K_PHY_IQ) & AR5K_PHY_IQ_RUN) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_CALIBRATE,
				"I/Q calibration still running");
		return -EBUSY;
	}

	

	for (i = 0; i <= 10; i++) {
		iq_corr = ath5k_hw_reg_read(ah, AR5K_PHY_IQRES_CAL_CORR);
		i_pwr = ath5k_hw_reg_read(ah, AR5K_PHY_IQRES_CAL_PWR_I);
		q_pwr = ath5k_hw_reg_read(ah, AR5K_PHY_IQRES_CAL_PWR_Q);
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_CALIBRATE,
			"iq_corr:%x i_pwr:%x q_pwr:%x", iq_corr, i_pwr, q_pwr);
		if (i_pwr && q_pwr)
			break;
	}

	i_coffd = ((i_pwr >> 1) + (q_pwr >> 1)) >> 7;

	if (ah->ah_version == AR5K_AR5211)
		q_coffd = q_pwr >> 6;
	else
		q_coffd = q_pwr >> 7;

	if (i_coffd == 0 || q_coffd < 2)
		return -ECANCELED;

	

	i_coff = (-iq_corr) / i_coffd;
	i_coff = clamp(i_coff, -32, 31); 

	if (ah->ah_version == AR5K_AR5211)
		q_coff = (i_pwr / q_coffd) - 64;
	else
		q_coff = (i_pwr / q_coffd) - 128;
	q_coff = clamp(q_coff, -16, 15); 

	ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_CALIBRATE,
			"new I:%d Q:%d (i_coffd:%x q_coffd:%x)",
			i_coff, q_coff, i_coffd, q_coffd);

	
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_Q_I_COFF, i_coff);
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_Q_Q_COFF, q_coff);
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_ENABLE);

	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ,
			AR5K_PHY_IQ_CAL_NUM_LOG_MAX, 15);
	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_RUN);

	return 0;
}

int
ath5k_hw_phy_calibrate(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	int ret;

	if (ah->ah_radio == AR5K_RF5110)
		return ath5k_hw_rf5110_calibrate(ah, channel);

	ret = ath5k_hw_rf511x_iq_calibrate(ah);
	if (ret) {
		ATH5K_DBG_UNLIMIT(ah, ATH5K_DEBUG_CALIBRATE,
			"No I/Q correction performed (%uMHz)\n",
			channel->center_freq);

		ret = 0;
	}

	if ((ah->ah_cal_mask & AR5K_CALIBRATION_FULL) &&
	    (ah->ah_radio == AR5K_RF5111 ||
	     ah->ah_radio == AR5K_RF5112) &&
	    channel->hw_value != AR5K_MODE_11B)
		ath5k_hw_request_rfgain_probe(ah);

	
	if (!(ah->ah_cal_mask & AR5K_CALIBRATION_NF))
		ath5k_hw_update_noise_floor(ah);

	return ret;
}



static void
ath5k_hw_set_spur_mitigation_filter(struct ath5k_hw *ah,
				struct ieee80211_channel *channel)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u32 mag_mask[4] = {0, 0, 0, 0};
	u32 pilot_mask[2] = {0, 0};
	
	u16 spur_chan_fbin, chan_fbin, symbol_width, spur_detection_window;
	s32 spur_delta_phase, spur_freq_sigma_delta;
	s32 spur_offset, num_symbols_x16;
	u8 num_symbol_offsets, i, freq_band;

	if (channel->band == IEEE80211_BAND_2GHZ) {
		chan_fbin = (channel->center_freq - 2300) * 10;
		freq_band = AR5K_EEPROM_BAND_2GHZ;
	} else {
		chan_fbin = (channel->center_freq - 4900) * 10;
		freq_band = AR5K_EEPROM_BAND_5GHZ;
	}

	spur_chan_fbin = AR5K_EEPROM_NO_SPUR;
	spur_detection_window = AR5K_SPUR_CHAN_WIDTH;
	
	if (ah->ah_bwmode == AR5K_BWMODE_40MHZ)
		spur_detection_window *= 2;

	for (i = 0; i < AR5K_EEPROM_N_SPUR_CHANS; i++) {
		spur_chan_fbin = ee->ee_spur_chans[i][freq_band];

		if (spur_chan_fbin == AR5K_EEPROM_NO_SPUR) {
			spur_chan_fbin &= AR5K_EEPROM_SPUR_CHAN_MASK;
			break;
		}

		if ((chan_fbin - spur_detection_window <=
		(spur_chan_fbin & AR5K_EEPROM_SPUR_CHAN_MASK)) &&
		(chan_fbin + spur_detection_window >=
		(spur_chan_fbin & AR5K_EEPROM_SPUR_CHAN_MASK))) {
			spur_chan_fbin &= AR5K_EEPROM_SPUR_CHAN_MASK;
			break;
		}
	}

	
	if (spur_chan_fbin) {
		spur_offset = spur_chan_fbin - chan_fbin;
		switch (ah->ah_bwmode) {
		case AR5K_BWMODE_40MHZ:
			
			spur_delta_phase = (spur_offset << 16) / 25;
			spur_freq_sigma_delta = (spur_delta_phase >> 10);
			symbol_width = AR5K_SPUR_SYMBOL_WIDTH_BASE_100Hz * 2;
			break;
		case AR5K_BWMODE_10MHZ:
			
			spur_delta_phase = (spur_offset << 18) / 25;
			spur_freq_sigma_delta = (spur_delta_phase >> 10);
			symbol_width = AR5K_SPUR_SYMBOL_WIDTH_BASE_100Hz / 2;
		case AR5K_BWMODE_5MHZ:
			
			spur_delta_phase = (spur_offset << 19) / 25;
			spur_freq_sigma_delta = (spur_delta_phase >> 10);
			symbol_width = AR5K_SPUR_SYMBOL_WIDTH_BASE_100Hz / 4;
		default:
			if (channel->band == IEEE80211_BAND_5GHZ) {
				
				spur_delta_phase = (spur_offset << 17) / 25;
				spur_freq_sigma_delta =
						(spur_delta_phase >> 10);
				symbol_width =
					AR5K_SPUR_SYMBOL_WIDTH_BASE_100Hz;
			} else {
				spur_delta_phase = (spur_offset << 17) / 25;
				spur_freq_sigma_delta =
						(spur_offset << 8) / 55;
				symbol_width =
					AR5K_SPUR_SYMBOL_WIDTH_BASE_100Hz;
			}
			break;
		}

		

		num_symbols_x16 = ((spur_offset * 1000) << 4) / symbol_width;

		
		if (!(num_symbols_x16 & 0xF))
			
			num_symbol_offsets = 3;
		else
			
			num_symbol_offsets = 4;

		for (i = 0; i < num_symbol_offsets; i++) {

			
			s32 curr_sym_off =
				(num_symbols_x16 / 16) + i + 25;

			u8 plt_mag_map =
				(i == 0 || i == (num_symbol_offsets - 1))
								? 1 : 2;

			if (curr_sym_off >= 0 && curr_sym_off <= 32) {
				if (curr_sym_off <= 25)
					pilot_mask[0] |= 1 << curr_sym_off;
				else if (curr_sym_off >= 27)
					pilot_mask[0] |= 1 << (curr_sym_off - 1);
			} else if (curr_sym_off >= 33 && curr_sym_off <= 52)
				pilot_mask[1] |= 1 << (curr_sym_off - 33);

			
			if (curr_sym_off >= -1 && curr_sym_off <= 14)
				mag_mask[0] |=
					plt_mag_map << (curr_sym_off + 1) * 2;
			else if (curr_sym_off >= 15 && curr_sym_off <= 30)
				mag_mask[1] |=
					plt_mag_map << (curr_sym_off - 15) * 2;
			else if (curr_sym_off >= 31 && curr_sym_off <= 46)
				mag_mask[2] |=
					plt_mag_map << (curr_sym_off - 31) * 2;
			else if (curr_sym_off >= 47 && curr_sym_off <= 53)
				mag_mask[3] |=
					plt_mag_map << (curr_sym_off - 47) * 2;

		}

		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_BIN_MASK_CTL,
					AR5K_PHY_BIN_MASK_CTL_RATE, 0xff);
		
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ,
					AR5K_PHY_IQ_PILOT_MASK_EN |
					AR5K_PHY_IQ_CHAN_MASK_EN |
					AR5K_PHY_IQ_SPUR_FILT_EN);

		
		ath5k_hw_reg_write(ah,
				AR5K_REG_SM(spur_delta_phase,
					AR5K_PHY_TIMING_11_SPUR_DELTA_PHASE) |
				AR5K_REG_SM(spur_freq_sigma_delta,
				AR5K_PHY_TIMING_11_SPUR_FREQ_SD) |
				AR5K_PHY_TIMING_11_USE_SPUR_IN_AGC,
				AR5K_PHY_TIMING_11);

		
		ath5k_hw_reg_write(ah, pilot_mask[0], AR5K_PHY_TIMING_7);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_TIMING_8,
					AR5K_PHY_TIMING_8_PILOT_MASK_2,
					pilot_mask[1]);

		ath5k_hw_reg_write(ah, pilot_mask[0], AR5K_PHY_TIMING_9);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_TIMING_10,
					AR5K_PHY_TIMING_10_PILOT_MASK_2,
					pilot_mask[1]);

		
		ath5k_hw_reg_write(ah, mag_mask[0], AR5K_PHY_BIN_MASK_1);
		ath5k_hw_reg_write(ah, mag_mask[1], AR5K_PHY_BIN_MASK_2);
		ath5k_hw_reg_write(ah, mag_mask[2], AR5K_PHY_BIN_MASK_3);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_BIN_MASK_CTL,
					AR5K_PHY_BIN_MASK_CTL_MASK_4,
					mag_mask[3]);

		ath5k_hw_reg_write(ah, mag_mask[0], AR5K_PHY_BIN_MASK2_1);
		ath5k_hw_reg_write(ah, mag_mask[1], AR5K_PHY_BIN_MASK2_2);
		ath5k_hw_reg_write(ah, mag_mask[2], AR5K_PHY_BIN_MASK2_3);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_BIN_MASK2_4,
					AR5K_PHY_BIN_MASK2_4_MASK_4,
					mag_mask[3]);

	} else if (ath5k_hw_reg_read(ah, AR5K_PHY_IQ) &
	AR5K_PHY_IQ_SPUR_FILT_EN) {
		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_BIN_MASK_CTL,
					AR5K_PHY_BIN_MASK_CTL_RATE, 0);
		AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_IQ,
					AR5K_PHY_IQ_PILOT_MASK_EN |
					AR5K_PHY_IQ_CHAN_MASK_EN |
					AR5K_PHY_IQ_SPUR_FILT_EN);
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_TIMING_11);

		
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_TIMING_7);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_TIMING_8,
					AR5K_PHY_TIMING_8_PILOT_MASK_2,
					0);

		ath5k_hw_reg_write(ah, 0, AR5K_PHY_TIMING_9);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_TIMING_10,
					AR5K_PHY_TIMING_10_PILOT_MASK_2,
					0);

		
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BIN_MASK_1);
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BIN_MASK_2);
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BIN_MASK_3);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_BIN_MASK_CTL,
					AR5K_PHY_BIN_MASK_CTL_MASK_4,
					0);

		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BIN_MASK2_1);
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BIN_MASK2_2);
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BIN_MASK2_3);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_BIN_MASK2_4,
					AR5K_PHY_BIN_MASK2_4_MASK_4,
					0);
	}
}




static void
ath5k_hw_set_def_antenna(struct ath5k_hw *ah, u8 ant)
{
	if (ah->ah_version != AR5K_AR5210)
		ath5k_hw_reg_write(ah, ant & 0x7, AR5K_DEFAULT_ANTENNA);
}

static void
ath5k_hw_set_fast_div(struct ath5k_hw *ah, u8 ee_mode, bool enable)
{
	switch (ee_mode) {
	case AR5K_EEPROM_MODE_11G:
	case AR5K_EEPROM_MODE_11A:
		if (enable)
			AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_AGCCTL,
					AR5K_PHY_AGCCTL_OFDM_DIV_DIS);
		else
			AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL,
					AR5K_PHY_AGCCTL_OFDM_DIV_DIS);
		break;
	case AR5K_EEPROM_MODE_11B:
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL,
					AR5K_PHY_AGCCTL_OFDM_DIV_DIS);
		break;
	default:
		return;
	}

	if (enable) {
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_RESTART,
				AR5K_PHY_RESTART_DIV_GC, 4);

		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_FAST_ANT_DIV,
					AR5K_PHY_FAST_ANT_DIV_EN);
	} else {
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_RESTART,
				AR5K_PHY_RESTART_DIV_GC, 0);

		AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_FAST_ANT_DIV,
					AR5K_PHY_FAST_ANT_DIV_EN);
	}
}

void
ath5k_hw_set_antenna_switch(struct ath5k_hw *ah, u8 ee_mode)
{
	u8 ant0, ant1;

	if (ah->ah_ant_mode == AR5K_ANTMODE_FIXED_A)
		ant0 = ant1 = AR5K_ANT_SWTABLE_A;
	else if (ah->ah_ant_mode == AR5K_ANTMODE_FIXED_B)
		ant0 = ant1 = AR5K_ANT_SWTABLE_B;
	else {
		ant0 = AR5K_ANT_SWTABLE_A;
		ant1 = AR5K_ANT_SWTABLE_B;
	}

	
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_ANT_CTL,
			AR5K_PHY_ANT_CTL_SWTABLE_IDLE,
			(ah->ah_ant_ctl[ee_mode][AR5K_ANT_CTL] |
			AR5K_PHY_ANT_CTL_TXRX_EN));

	
	ath5k_hw_reg_write(ah, ah->ah_ant_ctl[ee_mode][ant0],
		AR5K_PHY_ANT_SWITCH_TABLE_0);
	ath5k_hw_reg_write(ah, ah->ah_ant_ctl[ee_mode][ant1],
		AR5K_PHY_ANT_SWITCH_TABLE_1);
}

void
ath5k_hw_set_antenna_mode(struct ath5k_hw *ah, u8 ant_mode)
{
	struct ieee80211_channel *channel = ah->ah_current_channel;
	bool use_def_for_tx, update_def_on_tx, use_def_for_rts, fast_div;
	bool use_def_for_sg;
	int ee_mode;
	u8 def_ant, tx_ant;
	u32 sta_id1 = 0;

	if (channel == NULL) {
		ah->ah_ant_mode = ant_mode;
		return;
	}

	def_ant = ah->ah_def_ant;

	ee_mode = ath5k_eeprom_mode_from_channel(channel);
	if (ee_mode < 0) {
		ATH5K_ERR(ah,
			"invalid channel: %d\n", channel->center_freq);
		return;
	}

	switch (ant_mode) {
	case AR5K_ANTMODE_DEFAULT:
		tx_ant = 0;
		use_def_for_tx = false;
		update_def_on_tx = false;
		use_def_for_rts = false;
		use_def_for_sg = false;
		fast_div = true;
		break;
	case AR5K_ANTMODE_FIXED_A:
		def_ant = 1;
		tx_ant = 1;
		use_def_for_tx = true;
		update_def_on_tx = false;
		use_def_for_rts = true;
		use_def_for_sg = true;
		fast_div = false;
		break;
	case AR5K_ANTMODE_FIXED_B:
		def_ant = 2;
		tx_ant = 2;
		use_def_for_tx = true;
		update_def_on_tx = false;
		use_def_for_rts = true;
		use_def_for_sg = true;
		fast_div = false;
		break;
	case AR5K_ANTMODE_SINGLE_AP:
		def_ant = 1;	
		tx_ant = 0;
		use_def_for_tx = true;
		update_def_on_tx = true;
		use_def_for_rts = true;
		use_def_for_sg = true;
		fast_div = true;
		break;
	case AR5K_ANTMODE_SECTOR_AP:
		tx_ant = 1;	
		use_def_for_tx = false;
		update_def_on_tx = false;
		use_def_for_rts = true;
		use_def_for_sg = false;
		fast_div = false;
		break;
	case AR5K_ANTMODE_SECTOR_STA:
		tx_ant = 1;	
		use_def_for_tx = true;
		update_def_on_tx = false;
		use_def_for_rts = true;
		use_def_for_sg = false;
		fast_div = true;
		break;
	case AR5K_ANTMODE_DEBUG:
		def_ant = 1;
		tx_ant = 2;
		use_def_for_tx = false;
		update_def_on_tx = false;
		use_def_for_rts = false;
		use_def_for_sg = false;
		fast_div = false;
		break;
	default:
		return;
	}

	ah->ah_tx_ant = tx_ant;
	ah->ah_ant_mode = ant_mode;
	ah->ah_def_ant = def_ant;

	sta_id1 |= use_def_for_tx ? AR5K_STA_ID1_DEFAULT_ANTENNA : 0;
	sta_id1 |= update_def_on_tx ? AR5K_STA_ID1_DESC_ANTENNA : 0;
	sta_id1 |= use_def_for_rts ? AR5K_STA_ID1_RTS_DEF_ANTENNA : 0;
	sta_id1 |= use_def_for_sg ? AR5K_STA_ID1_SELFGEN_DEF_ANT : 0;

	AR5K_REG_DISABLE_BITS(ah, AR5K_STA_ID1, AR5K_STA_ID1_ANTENNA_SETTINGS);

	if (sta_id1)
		AR5K_REG_ENABLE_BITS(ah, AR5K_STA_ID1, sta_id1);

	ath5k_hw_set_antenna_switch(ah, ee_mode);
	ath5k_hw_set_fast_div(ah, ee_mode, fast_div);
	ath5k_hw_set_def_antenna(ah, def_ant);
}




static s16
ath5k_get_interpolated_value(s16 target, s16 x_left, s16 x_right,
					s16 y_left, s16 y_right)
{
	s16 ratio, result;

	if ((x_left == x_right) || (y_left == y_right))
		return y_left;

	ratio = ((100 * y_right - 100 * y_left) / (x_right - x_left));

	
	result = y_left + (ratio * (target - x_left) / 100);

	return result;
}

static s16
ath5k_get_linear_pcdac_min(const u8 *stepL, const u8 *stepR,
				const s16 *pwrL, const s16 *pwrR)
{
	s8 tmp;
	s16 min_pwrL, min_pwrR;
	s16 pwr_i;

	
	if (stepL[0] == stepL[1] || stepR[0] == stepR[1])
		return max(pwrL[0], pwrR[0]);

	if (pwrL[0] == pwrL[1])
		min_pwrL = pwrL[0];
	else {
		pwr_i = pwrL[0];
		do {
			pwr_i--;
			tmp = (s8) ath5k_get_interpolated_value(pwr_i,
							pwrL[0], pwrL[1],
							stepL[0], stepL[1]);
		} while (tmp > 1);

		min_pwrL = pwr_i;
	}

	if (pwrR[0] == pwrR[1])
		min_pwrR = pwrR[0];
	else {
		pwr_i = pwrR[0];
		do {
			pwr_i--;
			tmp = (s8) ath5k_get_interpolated_value(pwr_i,
							pwrR[0], pwrR[1],
							stepR[0], stepR[1]);
		} while (tmp > 1);

		min_pwrR = pwr_i;
	}

	
	return max(min_pwrL, min_pwrR);
}

static void
ath5k_create_power_curve(s16 pmin, s16 pmax,
			const s16 *pwr, const u8 *vpd,
			u8 num_points,
			u8 *vpd_table, u8 type)
{
	u8 idx[2] = { 0, 1 };
	s16 pwr_i = 2 * pmin;
	int i;

	if (num_points < 2)
		return;

	if (type == AR5K_PWRTABLE_LINEAR_PCDAC) {
		pwr_i = pmin;
		pmin = 0;
		pmax = 63;
	}

	for (i = 0; (i <= (u16) (pmax - pmin)) &&
	(i < AR5K_EEPROM_POWER_TABLE_SIZE); i++) {

		if ((pwr_i > pwr[idx[1]]) && (idx[1] < num_points - 1)) {
			idx[0]++;
			idx[1]++;
		}

		vpd_table[i] = (u8) ath5k_get_interpolated_value(pwr_i,
						pwr[idx[0]], pwr[idx[1]],
						vpd[idx[0]], vpd[idx[1]]);

		pwr_i += 2;
	}
}

static void
ath5k_get_chan_pcal_surrounding_piers(struct ath5k_hw *ah,
			struct ieee80211_channel *channel,
			struct ath5k_chan_pcal_info **pcinfo_l,
			struct ath5k_chan_pcal_info **pcinfo_r)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	struct ath5k_chan_pcal_info *pcinfo;
	u8 idx_l, idx_r;
	u8 mode, max, i;
	u32 target = channel->center_freq;

	idx_l = 0;
	idx_r = 0;

	switch (channel->hw_value) {
	case AR5K_EEPROM_MODE_11A:
		pcinfo = ee->ee_pwr_cal_a;
		mode = AR5K_EEPROM_MODE_11A;
		break;
	case AR5K_EEPROM_MODE_11B:
		pcinfo = ee->ee_pwr_cal_b;
		mode = AR5K_EEPROM_MODE_11B;
		break;
	case AR5K_EEPROM_MODE_11G:
	default:
		pcinfo = ee->ee_pwr_cal_g;
		mode = AR5K_EEPROM_MODE_11G;
		break;
	}
	max = ee->ee_n_piers[mode] - 1;

	if (target < pcinfo[0].freq) {
		idx_l = idx_r = 0;
		goto done;
	}

	if (target > pcinfo[max].freq) {
		idx_l = idx_r = max;
		goto done;
	}

	for (i = 0; i <= max; i++) {

		if (pcinfo[i].freq == target) {
			idx_l = idx_r = i;
			goto done;
		}

		if (target < pcinfo[i].freq) {
			idx_r = i;
			idx_l = idx_r - 1;
			goto done;
		}
	}

done:
	*pcinfo_l = &pcinfo[idx_l];
	*pcinfo_r = &pcinfo[idx_r];
}

static void
ath5k_get_rate_pcal_data(struct ath5k_hw *ah,
			struct ieee80211_channel *channel,
			struct ath5k_rate_pcal_info *rates)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	struct ath5k_rate_pcal_info *rpinfo;
	u8 idx_l, idx_r;
	u8 mode, max, i;
	u32 target = channel->center_freq;

	idx_l = 0;
	idx_r = 0;

	switch (channel->hw_value) {
	case AR5K_MODE_11A:
		rpinfo = ee->ee_rate_tpwr_a;
		mode = AR5K_EEPROM_MODE_11A;
		break;
	case AR5K_MODE_11B:
		rpinfo = ee->ee_rate_tpwr_b;
		mode = AR5K_EEPROM_MODE_11B;
		break;
	case AR5K_MODE_11G:
	default:
		rpinfo = ee->ee_rate_tpwr_g;
		mode = AR5K_EEPROM_MODE_11G;
		break;
	}
	max = ee->ee_rate_target_pwr_num[mode] - 1;

	if (target < rpinfo[0].freq) {
		idx_l = idx_r = 0;
		goto done;
	}

	if (target > rpinfo[max].freq) {
		idx_l = idx_r = max;
		goto done;
	}

	for (i = 0; i <= max; i++) {

		if (rpinfo[i].freq == target) {
			idx_l = idx_r = i;
			goto done;
		}

		if (target < rpinfo[i].freq) {
			idx_r = i;
			idx_l = idx_r - 1;
			goto done;
		}
	}

done:
	
	rates->freq = target;

	rates->target_power_6to24 =
		ath5k_get_interpolated_value(target, rpinfo[idx_l].freq,
					rpinfo[idx_r].freq,
					rpinfo[idx_l].target_power_6to24,
					rpinfo[idx_r].target_power_6to24);

	rates->target_power_36 =
		ath5k_get_interpolated_value(target, rpinfo[idx_l].freq,
					rpinfo[idx_r].freq,
					rpinfo[idx_l].target_power_36,
					rpinfo[idx_r].target_power_36);

	rates->target_power_48 =
		ath5k_get_interpolated_value(target, rpinfo[idx_l].freq,
					rpinfo[idx_r].freq,
					rpinfo[idx_l].target_power_48,
					rpinfo[idx_r].target_power_48);

	rates->target_power_54 =
		ath5k_get_interpolated_value(target, rpinfo[idx_l].freq,
					rpinfo[idx_r].freq,
					rpinfo[idx_l].target_power_54,
					rpinfo[idx_r].target_power_54);
}

static void
ath5k_get_max_ctl_power(struct ath5k_hw *ah,
			struct ieee80211_channel *channel)
{
	struct ath_regulatory *regulatory = ath5k_hw_regulatory(ah);
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	struct ath5k_edge_power *rep = ee->ee_ctl_pwr;
	u8 *ctl_val = ee->ee_ctl;
	s16 max_chan_pwr = ah->ah_txpower.txp_max_pwr / 4;
	s16 edge_pwr = 0;
	u8 rep_idx;
	u8 i, ctl_mode;
	u8 ctl_idx = 0xFF;
	u32 target = channel->center_freq;

	ctl_mode = ath_regd_get_band_ctl(regulatory, channel->band);

	switch (channel->hw_value) {
	case AR5K_MODE_11A:
		if (ah->ah_bwmode == AR5K_BWMODE_40MHZ)
			ctl_mode |= AR5K_CTL_TURBO;
		else
			ctl_mode |= AR5K_CTL_11A;
		break;
	case AR5K_MODE_11G:
		if (ah->ah_bwmode == AR5K_BWMODE_40MHZ)
			ctl_mode |= AR5K_CTL_TURBOG;
		else
			ctl_mode |= AR5K_CTL_11G;
		break;
	case AR5K_MODE_11B:
		ctl_mode |= AR5K_CTL_11B;
		break;
	default:
		return;
	}

	for (i = 0; i < ee->ee_ctls; i++) {
		if (ctl_val[i] == ctl_mode) {
			ctl_idx = i;
			break;
		}
	}

	if (ctl_idx == 0xFF)
		return;

	rep_idx = ctl_idx * AR5K_EEPROM_N_EDGES;


	for (i = 0; i < AR5K_EEPROM_N_EDGES; i++) {
		rep_idx += i;
		if (target <= rep[rep_idx].freq)
			edge_pwr = (s16) rep[rep_idx].edge;
	}

	if (edge_pwr)
		ah->ah_txpower.txp_max_pwr = 4 * min(edge_pwr, max_chan_pwr);
}




static void
ath5k_fill_pwr_to_pcdac_table(struct ath5k_hw *ah, s16* table_min,
							s16 *table_max)
{
	u8	*pcdac_out = ah->ah_txpower.txp_pd_table;
	u8	*pcdac_tmp = ah->ah_txpower.tmpL[0];
	u8	pcdac_0, pcdac_n, pcdac_i, pwr_idx, i;
	s16	min_pwr, max_pwr;

	
	min_pwr = table_min[0];
	pcdac_0 = pcdac_tmp[0];

	max_pwr = table_max[0];
	pcdac_n = pcdac_tmp[table_max[0] - table_min[0]];

	
	pcdac_i = 0;
	for (i = 0; i < min_pwr; i++)
		pcdac_out[pcdac_i++] = pcdac_0;

	
	pwr_idx = min_pwr;
	for (i = 0; pwr_idx <= max_pwr &&
		    pcdac_i < AR5K_EEPROM_POWER_TABLE_SIZE; i++) {
		pcdac_out[pcdac_i++] = pcdac_tmp[i];
		pwr_idx++;
	}

	
	while (pcdac_i < AR5K_EEPROM_POWER_TABLE_SIZE)
		pcdac_out[pcdac_i++] = pcdac_n;

}

static void
ath5k_combine_linear_pcdac_curves(struct ath5k_hw *ah, s16* table_min,
						s16 *table_max, u8 pdcurves)
{
	u8	*pcdac_out = ah->ah_txpower.txp_pd_table;
	u8	*pcdac_low_pwr;
	u8	*pcdac_high_pwr;
	u8	*pcdac_tmp;
	u8	pwr;
	s16	max_pwr_idx;
	s16	min_pwr_idx;
	s16	mid_pwr_idx = 0;
	u8	edge_flag;
	int	i;

	if (pdcurves > 1) {
		pcdac_low_pwr = ah->ah_txpower.tmpL[1];
		pcdac_high_pwr = ah->ah_txpower.tmpL[0];
		mid_pwr_idx = table_max[1] - table_min[1] - 1;
		max_pwr_idx = (table_max[0] - table_min[0]) / 2;

		if (table_max[0] - table_min[1] > 126)
			min_pwr_idx = table_max[0] - 126;
		else
			min_pwr_idx = table_min[1];

		pcdac_tmp = pcdac_high_pwr;

		edge_flag = 0x40;
	} else {
		pcdac_low_pwr = ah->ah_txpower.tmpL[1]; 
		pcdac_high_pwr = ah->ah_txpower.tmpL[0];
		min_pwr_idx = table_min[0];
		max_pwr_idx = (table_max[0] - table_min[0]) / 2;
		pcdac_tmp = pcdac_high_pwr;
		edge_flag = 0;
	}

	
	ah->ah_txpower.txp_min_idx = min_pwr_idx / 2;

	
	pwr = max_pwr_idx;
	for (i = 63; i >= 0; i--) {
		if (edge_flag == 0x40 &&
		(2 * pwr <= (table_max[1] - table_min[0]) || pwr == 0)) {
			edge_flag = 0x00;
			pcdac_tmp = pcdac_low_pwr;
			pwr = mid_pwr_idx / 2;
		}

		if (pcdac_tmp[pwr] < 1 && (edge_flag == 0x00)) {
			while (i >= 0) {
				pcdac_out[i] = pcdac_out[i + 1];
				i--;
			}
			break;
		}

		pcdac_out[i] = pcdac_tmp[pwr] | edge_flag;

		if (pcdac_out[i] > 126)
			pcdac_out[i] = 126;

		
		pwr--;
	}
}

static void
ath5k_write_pcdac_table(struct ath5k_hw *ah)
{
	u8	*pcdac_out = ah->ah_txpower.txp_pd_table;
	int	i;

	for (i = 0; i < (AR5K_EEPROM_POWER_TABLE_SIZE / 2); i++) {
		ath5k_hw_reg_write(ah,
			(((pcdac_out[2 * i + 0] << 8 | 0xff) & 0xffff) << 0) |
			(((pcdac_out[2 * i + 1] << 8 | 0xff) & 0xffff) << 16),
			AR5K_PHY_PCDAC_TXPOWER(i));
	}
}




static void
ath5k_combine_pwr_to_pdadc_curves(struct ath5k_hw *ah,
			s16 *pwr_min, s16 *pwr_max, u8 pdcurves)
{
	u8 gain_boundaries[AR5K_EEPROM_N_PD_GAINS];
	u8 *pdadc_out = ah->ah_txpower.txp_pd_table;
	u8 *pdadc_tmp;
	s16 pdadc_0;
	u8 pdadc_i, pdadc_n, pwr_step, pdg, max_idx, table_size;
	u8 pd_gain_overlap;

	pd_gain_overlap = (u8) ath5k_hw_reg_read(ah, AR5K_PHY_TPC_RG5) &
		AR5K_PHY_TPC_RG5_PD_GAIN_OVERLAP;

	
	for (pdg = 0, pdadc_i = 0; pdg < pdcurves; pdg++) {
		pdadc_tmp = ah->ah_txpower.tmpL[pdg];

		if (pdg == pdcurves - 1)
			gain_boundaries[pdg] = pwr_max[pdg] + 4;
		else
			gain_boundaries[pdg] =
				(pwr_max[pdg] + pwr_min[pdg + 1]) / 2;

		if (gain_boundaries[pdg] > AR5K_TUNE_MAX_TXPOWER)
			gain_boundaries[pdg] = AR5K_TUNE_MAX_TXPOWER;

		if (pdg == 0)
			pdadc_0 = 0;
		else
			
			pdadc_0 = (gain_boundaries[pdg - 1] - pwr_min[pdg]) -
							pd_gain_overlap;

		
		if ((pdadc_tmp[1] - pdadc_tmp[0]) > 1)
			pwr_step = pdadc_tmp[1] - pdadc_tmp[0];
		else
			pwr_step = 1;

		while ((pdadc_0 < 0) && (pdadc_i < 128)) {
			s16 tmp = pdadc_tmp[0] + pdadc_0 * pwr_step;
			pdadc_out[pdadc_i++] = (tmp < 0) ? 0 : (u8) tmp;
			pdadc_0++;
		}

		
		pdadc_n = gain_boundaries[pdg] + pd_gain_overlap - pwr_min[pdg];
		
		table_size = pwr_max[pdg] - pwr_min[pdg];
		max_idx = (pdadc_n < table_size) ? pdadc_n : table_size;

		
		while (pdadc_0 < max_idx && pdadc_i < 128)
			pdadc_out[pdadc_i++] = pdadc_tmp[pdadc_0++];

		
		if (pdadc_n <= max_idx)
			continue;

		
		if ((pdadc_tmp[table_size - 1] - pdadc_tmp[table_size - 2]) > 1)
			pwr_step = pdadc_tmp[table_size - 1] -
						pdadc_tmp[table_size - 2];
		else
			pwr_step = 1;

		
		while ((pdadc_0 < (s16) pdadc_n) &&
		(pdadc_i < AR5K_EEPROM_POWER_TABLE_SIZE * 2)) {
			s16 tmp = pdadc_tmp[table_size - 1] +
					(pdadc_0 - max_idx) * pwr_step;
			pdadc_out[pdadc_i++] = (tmp > 127) ? 127 : (u8) tmp;
			pdadc_0++;
		}
	}

	while (pdg < AR5K_EEPROM_N_PD_GAINS) {
		gain_boundaries[pdg] = gain_boundaries[pdg - 1];
		pdg++;
	}

	while (pdadc_i < AR5K_EEPROM_POWER_TABLE_SIZE * 2) {
		pdadc_out[pdadc_i] = pdadc_out[pdadc_i - 1];
		pdadc_i++;
	}

	
	ath5k_hw_reg_write(ah,
		AR5K_REG_SM(pd_gain_overlap,
			AR5K_PHY_TPC_RG5_PD_GAIN_OVERLAP) |
		AR5K_REG_SM(gain_boundaries[0],
			AR5K_PHY_TPC_RG5_PD_GAIN_BOUNDARY_1) |
		AR5K_REG_SM(gain_boundaries[1],
			AR5K_PHY_TPC_RG5_PD_GAIN_BOUNDARY_2) |
		AR5K_REG_SM(gain_boundaries[2],
			AR5K_PHY_TPC_RG5_PD_GAIN_BOUNDARY_3) |
		AR5K_REG_SM(gain_boundaries[3],
			AR5K_PHY_TPC_RG5_PD_GAIN_BOUNDARY_4),
		AR5K_PHY_TPC_RG5);

	
	ah->ah_txpower.txp_min_idx = pwr_min[0];

}

static void
ath5k_write_pwr_to_pdadc_table(struct ath5k_hw *ah, u8 ee_mode)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u8 *pdadc_out = ah->ah_txpower.txp_pd_table;
	u8 *pdg_to_idx = ee->ee_pdc_to_idx[ee_mode];
	u8 pdcurves = ee->ee_pd_gains[ee_mode];
	u32 reg;
	u8 i;

	

	
	reg = ath5k_hw_reg_read(ah, AR5K_PHY_TPC_RG1);
	reg &= ~(AR5K_PHY_TPC_RG1_PDGAIN_1 |
		AR5K_PHY_TPC_RG1_PDGAIN_2 |
		AR5K_PHY_TPC_RG1_PDGAIN_3 |
		AR5K_PHY_TPC_RG1_NUM_PD_GAIN);

	reg |= AR5K_REG_SM(pdcurves, AR5K_PHY_TPC_RG1_NUM_PD_GAIN);

	switch (pdcurves) {
	case 3:
		reg |= AR5K_REG_SM(pdg_to_idx[2], AR5K_PHY_TPC_RG1_PDGAIN_3);
		
	case 2:
		reg |= AR5K_REG_SM(pdg_to_idx[1], AR5K_PHY_TPC_RG1_PDGAIN_2);
		
	case 1:
		reg |= AR5K_REG_SM(pdg_to_idx[0], AR5K_PHY_TPC_RG1_PDGAIN_1);
		break;
	}
	ath5k_hw_reg_write(ah, reg, AR5K_PHY_TPC_RG1);

	for (i = 0; i < (AR5K_EEPROM_POWER_TABLE_SIZE / 2); i++) {
		u32 val = get_unaligned_le32(&pdadc_out[4 * i]);
		ath5k_hw_reg_write(ah, val, AR5K_PHY_PDADC_TXPOWER(i));
	}
}



static int
ath5k_setup_channel_powertable(struct ath5k_hw *ah,
			struct ieee80211_channel *channel,
			u8 ee_mode, u8 type)
{
	struct ath5k_pdgain_info *pdg_L, *pdg_R;
	struct ath5k_chan_pcal_info *pcinfo_L;
	struct ath5k_chan_pcal_info *pcinfo_R;
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u8 *pdg_curve_to_idx = ee->ee_pdc_to_idx[ee_mode];
	s16 table_min[AR5K_EEPROM_N_PD_GAINS];
	s16 table_max[AR5K_EEPROM_N_PD_GAINS];
	u8 *tmpL;
	u8 *tmpR;
	u32 target = channel->center_freq;
	int pdg, i;

	
	ath5k_get_chan_pcal_surrounding_piers(ah, channel,
						&pcinfo_L,
						&pcinfo_R);

	for (pdg = 0; pdg < ee->ee_pd_gains[ee_mode]; pdg++) {

		u8 idx = pdg_curve_to_idx[pdg];

		
		pdg_L = &pcinfo_L->pd_curves[idx];
		pdg_R = &pcinfo_R->pd_curves[idx];

		
		tmpL = ah->ah_txpower.tmpL[pdg];
		tmpR = ah->ah_txpower.tmpR[pdg];

		table_min[pdg] = min(pdg_L->pd_pwr[0],
					pdg_R->pd_pwr[0]) / 2;

		table_max[pdg] = max(pdg_L->pd_pwr[pdg_L->pd_points - 1],
				pdg_R->pd_pwr[pdg_R->pd_points - 1]) / 2;

		switch (type) {
		case AR5K_PWRTABLE_LINEAR_PCDAC:
			table_min[pdg] = min(pdg_L->pd_pwr[0],
						pdg_R->pd_pwr[0]);

			table_max[pdg] =
				max(pdg_L->pd_pwr[pdg_L->pd_points - 1],
					pdg_R->pd_pwr[pdg_R->pd_points - 1]);

			if (!(ee->ee_pd_gains[ee_mode] > 1 && pdg == 0)) {

				table_min[pdg] =
					ath5k_get_linear_pcdac_min(pdg_L->pd_step,
								pdg_R->pd_step,
								pdg_L->pd_pwr,
								pdg_R->pd_pwr);

				if (table_max[pdg] - table_min[pdg] > 126)
					table_min[pdg] = table_max[pdg] - 126;
			}

			
		case AR5K_PWRTABLE_PWR_TO_PCDAC:
		case AR5K_PWRTABLE_PWR_TO_PDADC:

			ath5k_create_power_curve(table_min[pdg],
						table_max[pdg],
						pdg_L->pd_pwr,
						pdg_L->pd_step,
						pdg_L->pd_points, tmpL, type);

			if (pcinfo_L == pcinfo_R)
				continue;

			ath5k_create_power_curve(table_min[pdg],
						table_max[pdg],
						pdg_R->pd_pwr,
						pdg_R->pd_step,
						pdg_R->pd_points, tmpR, type);
			break;
		default:
			return -EINVAL;
		}

		for (i = 0; (i < (u16) (table_max[pdg] - table_min[pdg])) &&
		(i < AR5K_EEPROM_POWER_TABLE_SIZE); i++) {
			tmpL[i] = (u8) ath5k_get_interpolated_value(target,
							(s16) pcinfo_L->freq,
							(s16) pcinfo_R->freq,
							(s16) tmpL[i],
							(s16) tmpR[i]);
		}
	}


	ah->ah_txpower.txp_min_pwr = ath5k_get_interpolated_value(target,
					(s16) pcinfo_L->freq,
					(s16) pcinfo_R->freq,
					pcinfo_L->min_pwr, pcinfo_R->min_pwr);

	ah->ah_txpower.txp_max_pwr = ath5k_get_interpolated_value(target,
					(s16) pcinfo_L->freq,
					(s16) pcinfo_R->freq,
					pcinfo_L->max_pwr, pcinfo_R->max_pwr);

	
	switch (type) {
	case AR5K_PWRTABLE_LINEAR_PCDAC:
		ath5k_combine_linear_pcdac_curves(ah, table_min, table_max,
						ee->ee_pd_gains[ee_mode]);

		ah->ah_txpower.txp_offset = 64 - (table_max[0] / 2);
		break;
	case AR5K_PWRTABLE_PWR_TO_PCDAC:
		ath5k_fill_pwr_to_pcdac_table(ah, table_min, table_max);

		
		ah->ah_txpower.txp_min_idx = 0;
		ah->ah_txpower.txp_offset = 0;
		break;
	case AR5K_PWRTABLE_PWR_TO_PDADC:
		ath5k_combine_pwr_to_pdadc_curves(ah, table_min, table_max,
						ee->ee_pd_gains[ee_mode]);

		ah->ah_txpower.txp_offset = table_min[0];
		break;
	default:
		return -EINVAL;
	}

	ah->ah_txpower.txp_setup = true;

	return 0;
}

static void
ath5k_write_channel_powertable(struct ath5k_hw *ah, u8 ee_mode, u8 type)
{
	if (type == AR5K_PWRTABLE_PWR_TO_PDADC)
		ath5k_write_pwr_to_pdadc_table(ah, ee_mode);
	else
		ath5k_write_pcdac_table(ah);
}



static void
ath5k_setup_rate_powertable(struct ath5k_hw *ah, u16 max_pwr,
			struct ath5k_rate_pcal_info *rate_info,
			u8 ee_mode)
{
	unsigned int i;
	u16 *rates;

	max_pwr *= 2;
	max_pwr = min(max_pwr, (u16) ah->ah_txpower.txp_max_pwr) / 2;

	
	rates = ah->ah_txpower.txp_rates_power_table;

	
	for (i = 0; i < 5; i++)
		rates[i] = min(max_pwr, rate_info->target_power_6to24);

	
	rates[5] = min(rates[0], rate_info->target_power_36);
	rates[6] = min(rates[0], rate_info->target_power_48);
	rates[7] = min(rates[0], rate_info->target_power_54);

	
	
	rates[8] = min(rates[0], rate_info->target_power_6to24);
	
	rates[9] = min(rates[0], rate_info->target_power_36);
	
	rates[10] = min(rates[0], rate_info->target_power_36);
	
	rates[11] = min(rates[0], rate_info->target_power_48);
	
	rates[12] = min(rates[0], rate_info->target_power_48);
	
	rates[13] = min(rates[0], rate_info->target_power_54);
	
	rates[14] = min(rates[0], rate_info->target_power_54);

	
	rates[15] = min(rates[0], rate_info->target_power_6to24);

	if ((ee_mode == AR5K_EEPROM_MODE_11G) &&
	(ah->ah_phy_revision < AR5K_SREV_PHY_5212A))
		for (i = 8; i <= 15; i++)
			rates[i] -= ah->ah_txpower.txp_cck_ofdm_gainf_delta;

	for (i = 0; i < 16; i++) {
		rates[i] += ah->ah_txpower.txp_offset;
		
		if (rates[i] > 63)
			rates[i] = 63;
	}

	
	ah->ah_txpower.txp_min_pwr = 2 * rates[7];
	ah->ah_txpower.txp_cur_pwr = 2 * rates[0];
	ah->ah_txpower.txp_ofdm = rates[7];
}


static int
ath5k_hw_txpower(struct ath5k_hw *ah, struct ieee80211_channel *channel,
		 u8 txpower)
{
	struct ath5k_rate_pcal_info rate_info;
	struct ieee80211_channel *curr_channel = ah->ah_current_channel;
	int ee_mode;
	u8 type;
	int ret;

	if (txpower > AR5K_TUNE_MAX_TXPOWER) {
		ATH5K_ERR(ah, "invalid tx power: %u\n", txpower);
		return -EINVAL;
	}

	ee_mode = ath5k_eeprom_mode_from_channel(channel);
	if (ee_mode < 0) {
		ATH5K_ERR(ah,
			"invalid channel: %d\n", channel->center_freq);
		return -EINVAL;
	}

	
	switch (ah->ah_radio) {
	case AR5K_RF5110:
		
		return 0;
	case AR5K_RF5111:
		type = AR5K_PWRTABLE_PWR_TO_PCDAC;
		break;
	case AR5K_RF5112:
		type = AR5K_PWRTABLE_LINEAR_PCDAC;
		break;
	case AR5K_RF2413:
	case AR5K_RF5413:
	case AR5K_RF2316:
	case AR5K_RF2317:
	case AR5K_RF2425:
		type = AR5K_PWRTABLE_PWR_TO_PDADC;
		break;
	default:
		return -EINVAL;
	}

	if (!ah->ah_txpower.txp_setup ||
	    (channel->hw_value != curr_channel->hw_value) ||
	    (channel->center_freq != curr_channel->center_freq)) {
		
		memset(&ah->ah_txpower, 0, sizeof(ah->ah_txpower));
		ah->ah_txpower.txp_tpc = AR5K_TUNE_TPC_TXPOWER;

		
		ret = ath5k_setup_channel_powertable(ah, channel,
							ee_mode, type);
		if (ret)
			return ret;
	}

	
	ath5k_write_channel_powertable(ah, ee_mode, type);

	
	ath5k_get_max_ctl_power(ah, channel);

	

	

	

	ath5k_get_rate_pcal_data(ah, channel, &rate_info);

	
	ath5k_setup_rate_powertable(ah, txpower, &rate_info, ee_mode);

	
	ath5k_hw_reg_write(ah, AR5K_TXPOWER_OFDM(3, 24) |
		AR5K_TXPOWER_OFDM(2, 16) | AR5K_TXPOWER_OFDM(1, 8) |
		AR5K_TXPOWER_OFDM(0, 0), AR5K_PHY_TXPOWER_RATE1);

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_OFDM(7, 24) |
		AR5K_TXPOWER_OFDM(6, 16) | AR5K_TXPOWER_OFDM(5, 8) |
		AR5K_TXPOWER_OFDM(4, 0), AR5K_PHY_TXPOWER_RATE2);

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_CCK(10, 24) |
		AR5K_TXPOWER_CCK(9, 16) | AR5K_TXPOWER_CCK(15, 8) |
		AR5K_TXPOWER_CCK(8, 0), AR5K_PHY_TXPOWER_RATE3);

	ath5k_hw_reg_write(ah, AR5K_TXPOWER_CCK(14, 24) |
		AR5K_TXPOWER_CCK(13, 16) | AR5K_TXPOWER_CCK(12, 8) |
		AR5K_TXPOWER_CCK(11, 0), AR5K_PHY_TXPOWER_RATE4);

	
	if (ah->ah_txpower.txp_tpc) {
		ath5k_hw_reg_write(ah, AR5K_PHY_TXPOWER_RATE_MAX_TPC_ENABLE |
			AR5K_TUNE_MAX_TXPOWER, AR5K_PHY_TXPOWER_RATE_MAX);

		ath5k_hw_reg_write(ah,
			AR5K_REG_MS(AR5K_TUNE_MAX_TXPOWER, AR5K_TPC_ACK) |
			AR5K_REG_MS(AR5K_TUNE_MAX_TXPOWER, AR5K_TPC_CTS) |
			AR5K_REG_MS(AR5K_TUNE_MAX_TXPOWER, AR5K_TPC_CHIRP),
			AR5K_TPC);
	} else {
		ath5k_hw_reg_write(ah, AR5K_PHY_TXPOWER_RATE_MAX |
			AR5K_TUNE_MAX_TXPOWER, AR5K_PHY_TXPOWER_RATE_MAX);
	}

	return 0;
}

int
ath5k_hw_set_txpower_limit(struct ath5k_hw *ah, u8 txpower)
{
	ATH5K_DBG(ah, ATH5K_DEBUG_TXPOWER,
		"changing txpower to %d\n", txpower);

	return ath5k_hw_txpower(ah, ah->ah_current_channel, txpower);
}



int
ath5k_hw_phy_init(struct ath5k_hw *ah, struct ieee80211_channel *channel,
		      u8 mode, bool fast)
{
	struct ieee80211_channel *curr_channel;
	int ret, i;
	u32 phy_tst1;
	ret = 0;

	curr_channel = ah->ah_current_channel;
	if (fast && (channel->hw_value != curr_channel->hw_value))
		return -EINVAL;

	if (fast) {
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_RFBUS_REQ,
				    AR5K_PHY_RFBUS_REQ_REQUEST);
		for (i = 0; i < 100; i++) {
			if (ath5k_hw_reg_read(ah, AR5K_PHY_RFBUS_GRANT))
				break;
			udelay(5);
		}
		
		if (i >= 100)
			return -EIO;

		
		ret = ath5k_hw_channel(ah, channel);
		if (ret)
			return ret;

		ath5k_hw_wait_for_synth(ah, channel);
	}

	ret = ath5k_hw_txpower(ah, channel, ah->ah_txpower.txp_cur_pwr ?
			ah->ah_txpower.txp_cur_pwr / 2 : AR5K_TUNE_MAX_TXPOWER);
	if (ret)
		return ret;

	
	if (ah->ah_version == AR5K_AR5212 &&
		channel->hw_value != AR5K_MODE_11B) {

		ret = ath5k_hw_write_ofdm_timings(ah, channel);
		if (ret)
			return ret;

		if (ah->ah_mac_srev >= AR5K_SREV_AR5424)
			ath5k_hw_set_spur_mitigation_filter(ah,
							    channel);
	}

	if (fast) {
		AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_RFBUS_REQ,
				    AR5K_PHY_RFBUS_REQ_REQUEST);

		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL,
					AR5K_PHY_AGCCTL_NF);

		return ret;
	}

	if (ah->ah_version != AR5K_AR5210) {

		ret = ath5k_hw_rfgain_init(ah, channel->band);
		if (ret)
			return ret;

		usleep_range(1000, 1500);

		ret = ath5k_hw_rfregs_init(ah, channel, mode);
		if (ret)
			return ret;

		if (ah->ah_radio == AR5K_RF5111) {
			if (mode == AR5K_MODE_11B)
				AR5K_REG_ENABLE_BITS(ah, AR5K_TXCFG,
				    AR5K_TXCFG_B_MODE);
			else
				AR5K_REG_DISABLE_BITS(ah, AR5K_TXCFG,
				    AR5K_TXCFG_B_MODE);
		}

	} else if (ah->ah_version == AR5K_AR5210) {
		usleep_range(1000, 1500);
		
		ath5k_hw_reg_write(ah, AR5K_PHY_ACT_DISABLE, AR5K_PHY_ACT);
		usleep_range(1000, 1500);
	}

	
	ret = ath5k_hw_channel(ah, channel);
	if (ret)
		return ret;

	ath5k_hw_reg_write(ah, AR5K_PHY_ACT_ENABLE, AR5K_PHY_ACT);

	ath5k_hw_wait_for_synth(ah, channel);

	phy_tst1 = ath5k_hw_reg_read(ah, AR5K_PHY_TST1);
	ath5k_hw_reg_write(ah, AR5K_PHY_TST1_TXHOLD, AR5K_PHY_TST1);
	for (i = 0; i <= 20; i++) {
		if (!(ath5k_hw_reg_read(ah, AR5K_PHY_ADC_TEST) & 0x10))
			break;
		usleep_range(200, 250);
	}
	ath5k_hw_reg_write(ah, phy_tst1, AR5K_PHY_TST1);

	AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_AGCCTL,
				AR5K_PHY_AGCCTL_CAL | AR5K_PHY_AGCCTL_NF);

	ah->ah_iq_cal_needed = false;
	if (!(mode == AR5K_MODE_11B)) {
		ah->ah_iq_cal_needed = true;
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ,
				AR5K_PHY_IQ_CAL_NUM_LOG_MAX, 15);
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ,
				AR5K_PHY_IQ_RUN);
	}

	if (ath5k_hw_register_timeout(ah, AR5K_PHY_AGCCTL,
			AR5K_PHY_AGCCTL_CAL, 0, false)) {
		ATH5K_ERR(ah, "gain calibration timeout (%uMHz)\n",
			channel->center_freq);
	}

	
	ath5k_hw_set_antenna_mode(ah, ah->ah_ant_mode);

	return ret;
}
