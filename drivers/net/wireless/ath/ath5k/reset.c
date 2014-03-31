/*
 * Copyright (c) 2004-2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2008 Nick Kossifidis <mickflemm@gmail.com>
 * Copyright (c) 2007-2008 Luis Rodriguez <mcgrof@winlab.rutgers.edu>
 * Copyright (c) 2007-2008 Pavel Roskin <proski@gnu.org>
 * Copyright (c) 2007-2008 Jiri Slaby <jirislaby@gmail.com>
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


#include <asm/unaligned.h>

#include <linux/pci.h>		
#include <linux/log2.h>
#include <linux/platform_device.h>
#include "ath5k.h"
#include "reg.h"
#include "debug.h"





int
ath5k_hw_register_timeout(struct ath5k_hw *ah, u32 reg, u32 flag, u32 val,
			      bool is_set)
{
	int i;
	u32 data;

	for (i = AR5K_TUNE_REGISTER_TIMEOUT; i > 0; i--) {
		data = ath5k_hw_reg_read(ah, reg);
		if (is_set && (data & flag))
			break;
		else if ((data & flag) == val)
			break;
		udelay(15);
	}

	return (i <= 0) ? -EAGAIN : 0;
}



unsigned int
ath5k_hw_htoclock(struct ath5k_hw *ah, unsigned int usec)
{
	struct ath_common *common = ath5k_hw_common(ah);
	return usec * common->clockrate;
}

unsigned int
ath5k_hw_clocktoh(struct ath5k_hw *ah, unsigned int clock)
{
	struct ath_common *common = ath5k_hw_common(ah);
	return clock / common->clockrate;
}

static void
ath5k_hw_init_core_clock(struct ath5k_hw *ah)
{
	struct ieee80211_channel *channel = ah->ah_current_channel;
	struct ath_common *common = ath5k_hw_common(ah);
	u32 usec_reg, txlat, rxlat, usec, clock, sclock, txf2txs;

	switch (channel->hw_value) {
	case AR5K_MODE_11A:
		clock = 40;
		break;
	case AR5K_MODE_11B:
		clock = 22;
		break;
	case AR5K_MODE_11G:
	default:
		clock = 44;
		break;
	}

	switch (ah->ah_bwmode) {
	case AR5K_BWMODE_40MHZ:
		clock *= 2;
		break;
	case AR5K_BWMODE_10MHZ:
		clock /= 2;
		break;
	case AR5K_BWMODE_5MHZ:
		clock /= 4;
		break;
	default:
		break;
	}

	common->clockrate = clock;

	
	usec = clock - 1;
	usec = AR5K_REG_SM(usec, AR5K_USEC_1);

	
	if (ah->ah_version != AR5K_AR5210)
		AR5K_REG_WRITE_BITS(ah, AR5K_DCU_GBL_IFS_MISC,
					AR5K_DCU_GBL_IFS_MISC_USEC_DUR,
					clock);

	
	if ((ah->ah_radio == AR5K_RF5112) ||
	    (ah->ah_radio == AR5K_RF2413) ||
	    (ah->ah_radio == AR5K_RF5413) ||
	    (ah->ah_radio == AR5K_RF2316) ||
	    (ah->ah_radio == AR5K_RF2317))
		
		sclock = 40 - 1;
	else
		sclock = 32 - 1;
	sclock = AR5K_REG_SM(sclock, AR5K_USEC_32);

	usec_reg = ath5k_hw_reg_read(ah, AR5K_USEC_5211);
	txlat = AR5K_REG_MS(usec_reg, AR5K_USEC_TX_LATENCY_5211);
	rxlat = AR5K_REG_MS(usec_reg, AR5K_USEC_RX_LATENCY_5211);

	txf2txs = AR5K_INIT_TXF2TXD_START_DEFAULT;

	if (ah->ah_version == AR5K_AR5210) {
		
		txlat = AR5K_INIT_TX_LATENCY_5210;
		rxlat = AR5K_INIT_RX_LATENCY_5210;
	}

	if (ah->ah_mac_srev < AR5K_SREV_AR5211) {
		txlat = AR5K_REG_SM(txlat, AR5K_USEC_TX_LATENCY_5210);
		rxlat = AR5K_REG_SM(rxlat, AR5K_USEC_RX_LATENCY_5210);
	} else
	switch (ah->ah_bwmode) {
	case AR5K_BWMODE_10MHZ:
		txlat = AR5K_REG_SM(txlat * 2,
				AR5K_USEC_TX_LATENCY_5211);
		rxlat = AR5K_REG_SM(AR5K_INIT_RX_LAT_MAX,
				AR5K_USEC_RX_LATENCY_5211);
		txf2txs = AR5K_INIT_TXF2TXD_START_DELAY_10MHZ;
		break;
	case AR5K_BWMODE_5MHZ:
		txlat = AR5K_REG_SM(txlat * 4,
				AR5K_USEC_TX_LATENCY_5211);
		rxlat = AR5K_REG_SM(AR5K_INIT_RX_LAT_MAX,
				AR5K_USEC_RX_LATENCY_5211);
		txf2txs = AR5K_INIT_TXF2TXD_START_DELAY_5MHZ;
		break;
	case AR5K_BWMODE_40MHZ:
		txlat = AR5K_INIT_TX_LAT_MIN;
		rxlat = AR5K_REG_SM(rxlat / 2,
				AR5K_USEC_RX_LATENCY_5211);
		txf2txs = AR5K_INIT_TXF2TXD_START_DEFAULT;
		break;
	default:
		break;
	}

	usec_reg = (usec | sclock | txlat | rxlat);
	ath5k_hw_reg_write(ah, usec_reg, AR5K_USEC);

	
	if (ah->ah_radio == AR5K_RF5112) {
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_RF_CTL2,
					AR5K_PHY_RF_CTL2_TXF2TXD_START,
					txf2txs);
	}
}

static void
ath5k_hw_set_sleep_clock(struct ath5k_hw *ah, bool enable)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	u32 scal, spending, sclock;

	if ((AR5K_EEPROM_HAS32KHZCRYSTAL(ee->ee_misc1) ||
	AR5K_EEPROM_HAS32KHZCRYSTAL_OLD(ee->ee_misc1)) &&
	enable) {

		
		AR5K_REG_WRITE_BITS(ah, AR5K_USEC_5211, AR5K_USEC_32, 1);
		
		AR5K_REG_WRITE_BITS(ah, AR5K_TSF_PARM, AR5K_TSF_PARM_INC, 61);

		ath5k_hw_reg_write(ah, 0x1f, AR5K_PHY_SCR);

		if ((ah->ah_radio == AR5K_RF5112) ||
		(ah->ah_radio == AR5K_RF5413) ||
		(ah->ah_radio == AR5K_RF2316) ||
		(ah->ah_mac_version == (AR5K_SREV_AR2417 >> 4)))
			spending = 0x14;
		else
			spending = 0x18;
		ath5k_hw_reg_write(ah, spending, AR5K_PHY_SPENDING);

		if ((ah->ah_radio == AR5K_RF5112) ||
		(ah->ah_radio == AR5K_RF5413) ||
		(ah->ah_mac_version == (AR5K_SREV_AR2417 >> 4))) {
			ath5k_hw_reg_write(ah, 0x26, AR5K_PHY_SLMT);
			ath5k_hw_reg_write(ah, 0x0d, AR5K_PHY_SCAL);
			ath5k_hw_reg_write(ah, 0x07, AR5K_PHY_SCLOCK);
			ath5k_hw_reg_write(ah, 0x3f, AR5K_PHY_SDELAY);
			AR5K_REG_WRITE_BITS(ah, AR5K_PCICFG,
				AR5K_PCICFG_SLEEP_CLOCK_RATE, 0x02);
		} else {
			ath5k_hw_reg_write(ah, 0x0a, AR5K_PHY_SLMT);
			ath5k_hw_reg_write(ah, 0x0c, AR5K_PHY_SCAL);
			ath5k_hw_reg_write(ah, 0x03, AR5K_PHY_SCLOCK);
			ath5k_hw_reg_write(ah, 0x20, AR5K_PHY_SDELAY);
			AR5K_REG_WRITE_BITS(ah, AR5K_PCICFG,
				AR5K_PCICFG_SLEEP_CLOCK_RATE, 0x03);
		}

		
		AR5K_REG_ENABLE_BITS(ah, AR5K_PCICFG,
				AR5K_PCICFG_SLEEP_CLOCK_EN);

	} else {

		AR5K_REG_DISABLE_BITS(ah, AR5K_PCICFG,
				AR5K_PCICFG_SLEEP_CLOCK_EN);

		AR5K_REG_WRITE_BITS(ah, AR5K_PCICFG,
				AR5K_PCICFG_SLEEP_CLOCK_RATE, 0);

		
		ath5k_hw_reg_write(ah, 0x1f, AR5K_PHY_SCR);
		ath5k_hw_reg_write(ah, AR5K_PHY_SLMT_32MHZ, AR5K_PHY_SLMT);

		if (ah->ah_mac_version == (AR5K_SREV_AR2417 >> 4))
			scal = AR5K_PHY_SCAL_32MHZ_2417;
		else if (ee->ee_is_hb63)
			scal = AR5K_PHY_SCAL_32MHZ_HB63;
		else
			scal = AR5K_PHY_SCAL_32MHZ;
		ath5k_hw_reg_write(ah, scal, AR5K_PHY_SCAL);

		ath5k_hw_reg_write(ah, AR5K_PHY_SCLOCK_32MHZ, AR5K_PHY_SCLOCK);
		ath5k_hw_reg_write(ah, AR5K_PHY_SDELAY_32MHZ, AR5K_PHY_SDELAY);

		if ((ah->ah_radio == AR5K_RF5112) ||
		(ah->ah_radio == AR5K_RF5413) ||
		(ah->ah_radio == AR5K_RF2316) ||
		(ah->ah_mac_version == (AR5K_SREV_AR2417 >> 4)))
			spending = 0x14;
		else
			spending = 0x18;
		ath5k_hw_reg_write(ah, spending, AR5K_PHY_SPENDING);

		
		AR5K_REG_WRITE_BITS(ah, AR5K_TSF_PARM, AR5K_TSF_PARM_INC, 1);

		if ((ah->ah_radio == AR5K_RF5112) ||
			(ah->ah_radio == AR5K_RF5413) ||
			(ah->ah_radio == AR5K_RF2316) ||
			(ah->ah_radio == AR5K_RF2317))
			sclock = 40 - 1;
		else
			sclock = 32 - 1;
		AR5K_REG_WRITE_BITS(ah, AR5K_USEC_5211, AR5K_USEC_32, sclock);
	}
}



static int
ath5k_hw_nic_reset(struct ath5k_hw *ah, u32 val)
{
	int ret;
	u32 mask = val ? val : ~0U;

	
	ath5k_hw_reg_read(ah, AR5K_RXDP);

	ath5k_hw_reg_write(ah, val, AR5K_RESET_CTL);

	
	usleep_range(15, 20);

	if (ah->ah_version == AR5K_AR5210) {
		val &= AR5K_RESET_CTL_PCU | AR5K_RESET_CTL_DMA
			| AR5K_RESET_CTL_MAC | AR5K_RESET_CTL_PHY;
		mask &= AR5K_RESET_CTL_PCU | AR5K_RESET_CTL_DMA
			| AR5K_RESET_CTL_MAC | AR5K_RESET_CTL_PHY;
	} else {
		val &= AR5K_RESET_CTL_PCU | AR5K_RESET_CTL_BASEBAND;
		mask &= AR5K_RESET_CTL_PCU | AR5K_RESET_CTL_BASEBAND;
	}

	ret = ath5k_hw_register_timeout(ah, AR5K_RESET_CTL, mask, val, false);

	if ((val & AR5K_RESET_CTL_PCU) == 0)
		ath5k_hw_reg_write(ah, AR5K_INIT_CFG, AR5K_CFG);

	return ret;
}

static int
ath5k_hw_wisoc_reset(struct ath5k_hw *ah, u32 flags)
{
	u32 mask = flags ? flags : ~0U;
	u32 __iomem *reg;
	u32 regval;
	u32 val = 0;

	
	if (ah->devid >= AR5K_SREV_AR2315_R6) {
		reg = (u32 __iomem *) AR5K_AR2315_RESET;
		if (mask & AR5K_RESET_CTL_PCU)
			val |= AR5K_AR2315_RESET_WMAC;
		if (mask & AR5K_RESET_CTL_BASEBAND)
			val |= AR5K_AR2315_RESET_BB_WARM;
	} else {
		reg = (u32 __iomem *) AR5K_AR5312_RESET;
		if (to_platform_device(ah->dev)->id == 0) {
			if (mask & AR5K_RESET_CTL_PCU)
				val |= AR5K_AR5312_RESET_WMAC0;
			if (mask & AR5K_RESET_CTL_BASEBAND)
				val |= AR5K_AR5312_RESET_BB0_COLD |
				       AR5K_AR5312_RESET_BB0_WARM;
		} else {
			if (mask & AR5K_RESET_CTL_PCU)
				val |= AR5K_AR5312_RESET_WMAC1;
			if (mask & AR5K_RESET_CTL_BASEBAND)
				val |= AR5K_AR5312_RESET_BB1_COLD |
				       AR5K_AR5312_RESET_BB1_WARM;
		}
	}

	
	regval = ioread32(reg);
	iowrite32(regval | val, reg);
	regval = ioread32(reg);
	usleep_range(100, 150);

	
	iowrite32(regval & ~val, reg);
	regval = ioread32(reg);

	if ((flags & AR5K_RESET_CTL_PCU) == 0)
		ath5k_hw_reg_write(ah, AR5K_INIT_CFG, AR5K_CFG);

	return 0;
}

static int
ath5k_hw_set_power_mode(struct ath5k_hw *ah, enum ath5k_power_mode mode,
			      bool set_chip, u16 sleep_duration)
{
	unsigned int i;
	u32 staid, data;

	staid = ath5k_hw_reg_read(ah, AR5K_STA_ID1);

	switch (mode) {
	case AR5K_PM_AUTO:
		staid &= ~AR5K_STA_ID1_DEFAULT_ANTENNA;
		
	case AR5K_PM_NETWORK_SLEEP:
		if (set_chip)
			ath5k_hw_reg_write(ah,
				AR5K_SLEEP_CTL_SLE_ALLOW |
				sleep_duration,
				AR5K_SLEEP_CTL);

		staid |= AR5K_STA_ID1_PWR_SV;
		break;

	case AR5K_PM_FULL_SLEEP:
		if (set_chip)
			ath5k_hw_reg_write(ah, AR5K_SLEEP_CTL_SLE_SLP,
				AR5K_SLEEP_CTL);

		staid |= AR5K_STA_ID1_PWR_SV;
		break;

	case AR5K_PM_AWAKE:

		staid &= ~AR5K_STA_ID1_PWR_SV;

		if (!set_chip)
			goto commit;

		data = ath5k_hw_reg_read(ah, AR5K_SLEEP_CTL);

		if (data & 0xffc00000)
			data = 0;
		else
			
			data = data & ~AR5K_SLEEP_CTL_SLE;

		ath5k_hw_reg_write(ah, data | AR5K_SLEEP_CTL_SLE_WAKE,
							AR5K_SLEEP_CTL);
		usleep_range(15, 20);

		for (i = 200; i > 0; i--) {
			
			if ((ath5k_hw_reg_read(ah, AR5K_PCICFG) &
					AR5K_PCICFG_SPWR_DN) == 0)
				break;

			
			usleep_range(50, 75);
			ath5k_hw_reg_write(ah, data | AR5K_SLEEP_CTL_SLE_WAKE,
							AR5K_SLEEP_CTL);
		}

		
		if (i == 0)
			return -EIO;

		break;

	default:
		return -EINVAL;
	}

commit:
	ath5k_hw_reg_write(ah, staid, AR5K_STA_ID1);

	return 0;
}

int
ath5k_hw_on_hold(struct ath5k_hw *ah)
{
	struct pci_dev *pdev = ah->pdev;
	u32 bus_flags;
	int ret;

	if (ath5k_get_bus_type(ah) == ATH_AHB)
		return 0;

	
	ret = ath5k_hw_set_power_mode(ah, AR5K_PM_AWAKE, true, 0);
	if (ret) {
		ATH5K_ERR(ah, "failed to wakeup the MAC Chip\n");
		return ret;
	}

	bus_flags = (pdev && pci_is_pcie(pdev)) ? 0 : AR5K_RESET_CTL_PCI;

	if (ah->ah_version == AR5K_AR5210) {
		ret = ath5k_hw_nic_reset(ah, AR5K_RESET_CTL_PCU |
			AR5K_RESET_CTL_MAC | AR5K_RESET_CTL_DMA |
			AR5K_RESET_CTL_PHY | AR5K_RESET_CTL_PCI);
			usleep_range(2000, 2500);
	} else {
		ret = ath5k_hw_nic_reset(ah, AR5K_RESET_CTL_PCU |
			AR5K_RESET_CTL_BASEBAND | bus_flags);
	}

	if (ret) {
		ATH5K_ERR(ah, "failed to put device on warm reset\n");
		return -EIO;
	}

	
	ret = ath5k_hw_set_power_mode(ah, AR5K_PM_AWAKE, true, 0);
	if (ret) {
		ATH5K_ERR(ah, "failed to put device on hold\n");
		return ret;
	}

	return ret;
}

int
ath5k_hw_nic_wakeup(struct ath5k_hw *ah, struct ieee80211_channel *channel)
{
	struct pci_dev *pdev = ah->pdev;
	u32 turbo, mode, clock, bus_flags;
	int ret;

	turbo = 0;
	mode = 0;
	clock = 0;

	if ((ath5k_get_bus_type(ah) != ATH_AHB) || channel) {
		
		ret = ath5k_hw_set_power_mode(ah, AR5K_PM_AWAKE, true, 0);
		if (ret) {
			ATH5K_ERR(ah, "failed to wakeup the MAC Chip\n");
			return ret;
		}
	}

	bus_flags = (pdev && pci_is_pcie(pdev)) ? 0 : AR5K_RESET_CTL_PCI;

	if (ah->ah_version == AR5K_AR5210) {
		ret = ath5k_hw_nic_reset(ah, AR5K_RESET_CTL_PCU |
			AR5K_RESET_CTL_MAC | AR5K_RESET_CTL_DMA |
			AR5K_RESET_CTL_PHY | AR5K_RESET_CTL_PCI);
			usleep_range(2000, 2500);
	} else {
		if (ath5k_get_bus_type(ah) == ATH_AHB)
			ret = ath5k_hw_wisoc_reset(ah, AR5K_RESET_CTL_PCU |
				AR5K_RESET_CTL_BASEBAND);
		else
			ret = ath5k_hw_nic_reset(ah, AR5K_RESET_CTL_PCU |
				AR5K_RESET_CTL_BASEBAND | bus_flags);
	}

	if (ret) {
		ATH5K_ERR(ah, "failed to reset the MAC Chip\n");
		return -EIO;
	}

	
	ret = ath5k_hw_set_power_mode(ah, AR5K_PM_AWAKE, true, 0);
	if (ret) {
		ATH5K_ERR(ah, "failed to resume the MAC Chip\n");
		return ret;
	}

	if (ath5k_get_bus_type(ah) == ATH_AHB)
		ret = ath5k_hw_wisoc_reset(ah, 0);
	else
		ret = ath5k_hw_nic_reset(ah, 0);

	if (ret) {
		ATH5K_ERR(ah, "failed to warm reset the MAC Chip\n");
		return -EIO;
	}

	if (!channel)
		return 0;

	if (ah->ah_version != AR5K_AR5210) {

		if (ah->ah_radio >= AR5K_RF5112) {
			mode = AR5K_PHY_MODE_RAD_RF5112;
			clock = AR5K_PHY_PLL_RF5112;
		} else {
			mode = AR5K_PHY_MODE_RAD_RF5111;	
			clock = AR5K_PHY_PLL_RF5111;		
		}

		if (channel->band == IEEE80211_BAND_2GHZ) {
			mode |= AR5K_PHY_MODE_FREQ_2GHZ;
			clock |= AR5K_PHY_PLL_44MHZ;

			if (channel->hw_value == AR5K_MODE_11B) {
				mode |= AR5K_PHY_MODE_MOD_CCK;
			} else {
				if (ah->ah_version == AR5K_AR5211)
					mode |= AR5K_PHY_MODE_MOD_OFDM;
				else
					mode |= AR5K_PHY_MODE_MOD_DYN;
			}
		} else if (channel->band == IEEE80211_BAND_5GHZ) {
			mode |= (AR5K_PHY_MODE_FREQ_5GHZ |
				 AR5K_PHY_MODE_MOD_OFDM);

			
			if (ah->ah_radio == AR5K_RF5413)
				clock = AR5K_PHY_PLL_40MHZ_5413;
			else
				clock |= AR5K_PHY_PLL_40MHZ;
		} else {
			ATH5K_ERR(ah, "invalid radio frequency mode\n");
			return -EINVAL;
		}

		
		if (ah->ah_bwmode == AR5K_BWMODE_40MHZ) {
			turbo = AR5K_PHY_TURBO_MODE |
				(ah->ah_radio == AR5K_RF2425) ? 0 :
				AR5K_PHY_TURBO_SHORT;
		} else if (ah->ah_bwmode != AR5K_BWMODE_DEFAULT) {
			if (ah->ah_radio == AR5K_RF5413) {
				mode |= (ah->ah_bwmode == AR5K_BWMODE_10MHZ) ?
					AR5K_PHY_MODE_HALF_RATE :
					AR5K_PHY_MODE_QUARTER_RATE;
			} else if (ah->ah_version == AR5K_AR5212) {
				clock |= (ah->ah_bwmode == AR5K_BWMODE_10MHZ) ?
					AR5K_PHY_PLL_HALF_RATE :
					AR5K_PHY_PLL_QUARTER_RATE;
			}
		}

	} else { 

		
		if (ah->ah_bwmode == AR5K_BWMODE_40MHZ)
			ath5k_hw_reg_write(ah, AR5K_PHY_TURBO_MODE,
					AR5K_PHY_TURBO);
	}

	if (ah->ah_version != AR5K_AR5210) {

		
		if (ath5k_hw_reg_read(ah, AR5K_PHY_PLL) != clock) {
			ath5k_hw_reg_write(ah, clock, AR5K_PHY_PLL);
			usleep_range(300, 350);
		}

		
		ath5k_hw_reg_write(ah, mode, AR5K_PHY_MODE);
		ath5k_hw_reg_write(ah, turbo, AR5K_PHY_TURBO);
	}

	return 0;
}



static void
ath5k_hw_tweak_initval_settings(struct ath5k_hw *ah,
				struct ieee80211_channel *channel)
{
	if (ah->ah_version == AR5K_AR5212 &&
	    ah->ah_phy_revision >= AR5K_SREV_PHY_5212A) {

		
		ath5k_hw_reg_write(ah,
				(AR5K_REG_SM(2,
				AR5K_PHY_ADC_CTL_INBUFGAIN_OFF) |
				AR5K_REG_SM(2,
				AR5K_PHY_ADC_CTL_INBUFGAIN_ON) |
				AR5K_PHY_ADC_CTL_PWD_DAC_OFF |
				AR5K_PHY_ADC_CTL_PWD_ADC_OFF),
				AR5K_PHY_ADC_CTL);



		
		AR5K_REG_DISABLE_BITS(ah, AR5K_PHY_DAG_CCK_CTL,
				AR5K_PHY_DAG_CCK_CTL_EN_RSSI_THR);

		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_DAG_CCK_CTL,
			AR5K_PHY_DAG_CCK_CTL_RSSI_THR, 2);

		
		ath5k_hw_reg_write(ah, 0x0000000f, AR5K_SEQ_MASK);
	}

	
	if (ah->ah_phy_revision >= AR5K_SREV_PHY_5212B)
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_BLUETOOTH);

	
	if (ah->ah_phy_revision > AR5K_SREV_PHY_5212B)
		AR5K_REG_DISABLE_BITS(ah, AR5K_TXCFG,
				AR5K_TXCFG_DCU_DBL_BUF_DIS);

	
	if ((ah->ah_radio == AR5K_RF5413) ||
		(ah->ah_radio == AR5K_RF2317) ||
		(ah->ah_mac_version == (AR5K_SREV_AR2417 >> 4))) {
		u32 fast_adc = true;

		if (channel->center_freq == 2462 ||
		channel->center_freq == 2467)
			fast_adc = 0;

		
		if (ath5k_hw_reg_read(ah, AR5K_PHY_FAST_ADC) != fast_adc)
				ath5k_hw_reg_write(ah, fast_adc,
						AR5K_PHY_FAST_ADC);
	}

	
	if (ah->ah_radio == AR5K_RF5112 &&
			ah->ah_radio_5ghz_revision <
			AR5K_SREV_RAD_5112A) {
		u32 data;
		ath5k_hw_reg_write(ah, AR5K_PHY_CCKTXCTL_WORLD,
				AR5K_PHY_CCKTXCTL);
		if (channel->band == IEEE80211_BAND_5GHZ)
			data = 0xffb81020;
		else
			data = 0xffb80d20;
		ath5k_hw_reg_write(ah, data, AR5K_PHY_FRAME_CTL);
	}

	if (ah->ah_mac_srev < AR5K_SREV_AR5211) {
		
		ath5k_hw_reg_write(ah, 0, AR5K_QCUDCU_CLKGT);
		
		ath5k_hw_reg_write(ah, AR5K_PHY_SCAL_32MHZ_5311,
						AR5K_PHY_SCAL);
		
		AR5K_REG_ENABLE_BITS(ah, AR5K_DIAG_SW_5211,
					AR5K_DIAG_SW_ECO_ENABLE);
	}

	if (ah->ah_bwmode) {
		if (ah->ah_bwmode == AR5K_BWMODE_40MHZ) {

			AR5K_REG_WRITE_BITS(ah, AR5K_PHY_SETTLING,
						AR5K_PHY_SETTLING_AGC,
						AR5K_AGC_SETTLING_TURBO);

			if (ah->ah_version == AR5K_AR5212)
				AR5K_REG_WRITE_BITS(ah, AR5K_PHY_SETTLING,
						AR5K_PHY_SETTLING_SWITCH,
						AR5K_SWITCH_SETTLING_TURBO);

			if (ah->ah_version == AR5K_AR5210) {
				
				ath5k_hw_reg_write(ah,
					(AR5K_PHY_FRAME_CTL_INI |
					AR5K_PHY_TURBO_MODE |
					AR5K_PHY_TURBO_SHORT | 0x2020),
					AR5K_PHY_FRAME_CTL_5210);
			}
		
		} else if ((ah->ah_mac_srev >= AR5K_SREV_AR5424) &&
		(ah->ah_mac_srev <= AR5K_SREV_AR5414)) {
			AR5K_REG_WRITE_BITS(ah, AR5K_PHY_FRAME_CTL_5211,
						AR5K_PHY_FRAME_CTL_WIN_LEN,
						3);
		}
	} else if (ah->ah_version == AR5K_AR5210) {
		
		ath5k_hw_reg_write(ah, (AR5K_PHY_FRAME_CTL_INI | 0x1020),
						AR5K_PHY_FRAME_CTL_5210);
	}
}

static void
ath5k_hw_commit_eeprom_settings(struct ath5k_hw *ah,
		struct ieee80211_channel *channel)
{
	struct ath5k_eeprom_info *ee = &ah->ah_capabilities.cap_eeprom;
	s16 cck_ofdm_pwr_delta;
	u8 ee_mode;

	
	if (ah->ah_version == AR5K_AR5210)
		return;

	ee_mode = ath5k_eeprom_mode_from_channel(channel);

	
	if (channel->center_freq == 2484)
		cck_ofdm_pwr_delta =
			((ee->ee_cck_ofdm_power_delta -
			ee->ee_scaled_cck_delta) * 2) / 10;
	else
		cck_ofdm_pwr_delta =
			(ee->ee_cck_ofdm_power_delta * 2) / 10;

	if (ah->ah_phy_revision >= AR5K_SREV_PHY_5212A) {
		if (channel->hw_value == AR5K_MODE_11G)
			ath5k_hw_reg_write(ah,
			AR5K_REG_SM((ee->ee_cck_ofdm_gain_delta * -1),
				AR5K_PHY_TX_PWR_ADJ_CCK_GAIN_DELTA) |
			AR5K_REG_SM((cck_ofdm_pwr_delta * -1),
				AR5K_PHY_TX_PWR_ADJ_CCK_PCDAC_INDEX),
				AR5K_PHY_TX_PWR_ADJ);
		else
			ath5k_hw_reg_write(ah, 0, AR5K_PHY_TX_PWR_ADJ);
	} else {
		ah->ah_txpower.txp_cck_ofdm_pwr_delta = cck_ofdm_pwr_delta;
		ah->ah_txpower.txp_cck_ofdm_gainf_delta =
						ee->ee_cck_ofdm_gain_delta;
	}

	ath5k_hw_set_antenna_switch(ah, ee_mode);

	
	ath5k_hw_reg_write(ah,
		AR5K_PHY_NF_SVAL(ee->ee_noise_floor_thr[ee_mode]),
		AR5K_PHY_NFTHRES);

	if ((ah->ah_bwmode == AR5K_BWMODE_40MHZ) &&
	(ah->ah_ee_version >= AR5K_EEPROM_VERSION_5_0)) {
		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_SETTLING,
				AR5K_PHY_SETTLING_SWITCH,
				ee->ee_switch_settling_turbo[ee_mode]);

		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_GAIN,
				AR5K_PHY_GAIN_TXRX_ATTEN,
				ee->ee_atn_tx_rx_turbo[ee_mode]);

		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_DESIRED_SIZE,
				AR5K_PHY_DESIRED_SIZE_ADC,
				ee->ee_adc_desired_size_turbo[ee_mode]);

		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_DESIRED_SIZE,
				AR5K_PHY_DESIRED_SIZE_PGA,
				ee->ee_pga_desired_size_turbo[ee_mode]);

		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_GAIN_2GHZ,
				AR5K_PHY_GAIN_2GHZ_MARGIN_TXRX,
				ee->ee_margin_tx_rx_turbo[ee_mode]);

	} else {
		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_SETTLING,
				AR5K_PHY_SETTLING_SWITCH,
				ee->ee_switch_settling[ee_mode]);

		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_GAIN,
				AR5K_PHY_GAIN_TXRX_ATTEN,
				ee->ee_atn_tx_rx[ee_mode]);

		
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_DESIRED_SIZE,
				AR5K_PHY_DESIRED_SIZE_ADC,
				ee->ee_adc_desired_size[ee_mode]);

		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_DESIRED_SIZE,
				AR5K_PHY_DESIRED_SIZE_PGA,
				ee->ee_pga_desired_size[ee_mode]);

		
		if (ah->ah_ee_version >= AR5K_EEPROM_VERSION_4_1)
			AR5K_REG_WRITE_BITS(ah, AR5K_PHY_GAIN_2GHZ,
				AR5K_PHY_GAIN_2GHZ_MARGIN_TXRX,
				ee->ee_margin_tx_rx[ee_mode]);
	}

	
	ath5k_hw_reg_write(ah,
		(ee->ee_tx_end2xpa_disable[ee_mode] << 24) |
		(ee->ee_tx_end2xpa_disable[ee_mode] << 16) |
		(ee->ee_tx_frm2xpa_enable[ee_mode] << 8) |
		(ee->ee_tx_frm2xpa_enable[ee_mode]), AR5K_PHY_RF_CTL4);

	
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_RF_CTL3,
			AR5K_PHY_RF_CTL3_TXE2XLNA_ON,
			ee->ee_tx_end2xlna_enable[ee_mode]);

	
	AR5K_REG_WRITE_BITS(ah, AR5K_PHY_NF,
			AR5K_PHY_NF_THRESH62,
			ee->ee_thr_62[ee_mode]);

	if (ath5k_hw_chan_has_spur_noise(ah, channel))
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_OFDM_SELFCORR,
				AR5K_PHY_OFDM_SELFCORR_CYPWR_THR1,
				AR5K_INIT_CYCRSSI_THR1 +
				ee->ee_false_detect[ee_mode]);
	else
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_OFDM_SELFCORR,
				AR5K_PHY_OFDM_SELFCORR_CYPWR_THR1,
				AR5K_INIT_CYCRSSI_THR1);

	
	
	if (ah->ah_ee_version >= AR5K_EEPROM_VERSION_4_0) {
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_Q_I_COFF,
			    ee->ee_i_cal[ee_mode]);
		AR5K_REG_WRITE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_Q_Q_COFF,
			    ee->ee_q_cal[ee_mode]);
		AR5K_REG_ENABLE_BITS(ah, AR5K_PHY_IQ, AR5K_PHY_IQ_CORR_ENABLE);
	}

	
	if (ah->ah_ee_version >= AR5K_EEPROM_VERSION_5_1)
		ath5k_hw_reg_write(ah, 0, AR5K_PHY_HEAVY_CLIP_ENABLE);
}



int
ath5k_hw_reset(struct ath5k_hw *ah, enum nl80211_iftype op_mode,
		struct ieee80211_channel *channel, bool fast, bool skip_pcu)
{
	u32 s_seq[10], s_led[3], tsf_up, tsf_lo;
	u8 mode;
	int i, ret;

	tsf_up = 0;
	tsf_lo = 0;
	mode = 0;

	if (fast && (ah->ah_radio != AR5K_RF2413) &&
	(ah->ah_radio != AR5K_RF5413))
		fast = false;

	if (ah->ah_version == AR5K_AR5212)
		ath5k_hw_set_sleep_clock(ah, false);

	ath5k_hw_stop_rx_pcu(ah);

	ret = ath5k_hw_dma_stop(ah);

	if (ret && fast) {
		ATH5K_DBG(ah, ATH5K_DEBUG_RESET,
			"DMA didn't stop, falling back to normal reset\n");
		fast = false;
		ret = 0;
	}

	mode = channel->hw_value;
	switch (mode) {
	case AR5K_MODE_11A:
		break;
	case AR5K_MODE_11G:
		if (ah->ah_version <= AR5K_AR5211) {
			ATH5K_ERR(ah,
				"G mode not available on 5210/5211");
			return -EINVAL;
		}
		break;
	case AR5K_MODE_11B:
		if (ah->ah_version < AR5K_AR5211) {
			ATH5K_ERR(ah,
				"B mode not available on 5210");
			return -EINVAL;
		}
		break;
	default:
		ATH5K_ERR(ah,
			"invalid channel: %d\n", channel->center_freq);
		return -EINVAL;
	}

	if (fast) {
		ret = ath5k_hw_phy_init(ah, channel, mode, true);
		if (ret) {
			ATH5K_DBG(ah, ATH5K_DEBUG_RESET,
				"fast chan change failed, falling back to normal reset\n");
			ret = 0;
		} else {
			ATH5K_DBG(ah, ATH5K_DEBUG_RESET,
				"fast chan change successful\n");
			return 0;
		}
	}

	if (ah->ah_version != AR5K_AR5210) {
		if (ah->ah_mac_srev < AR5K_SREV_AR5211) {

			for (i = 0; i < 10; i++)
				s_seq[i] = ath5k_hw_reg_read(ah,
					AR5K_QUEUE_DCU_SEQNUM(i));

		} else {
			s_seq[0] = ath5k_hw_reg_read(ah,
					AR5K_QUEUE_DCU_SEQNUM(0));
		}

		if (ah->ah_version == AR5K_AR5211) {
			tsf_up = ath5k_hw_reg_read(ah, AR5K_TSF_U32);
			tsf_lo = ath5k_hw_reg_read(ah, AR5K_TSF_L32);
		}
	}


	
	s_led[0] = ath5k_hw_reg_read(ah, AR5K_PCICFG) &
					AR5K_PCICFG_LEDSTATE;
	s_led[1] = ath5k_hw_reg_read(ah, AR5K_GPIOCR);
	s_led[2] = ath5k_hw_reg_read(ah, AR5K_GPIODO);


	if (ah->ah_version == AR5K_AR5212 &&
	(ah->ah_radio <= AR5K_RF5112)) {
		if (!fast && ah->ah_rf_banks != NULL)
				ath5k_hw_gainf_calibrate(ah);
	}

	
	ret = ath5k_hw_nic_wakeup(ah, channel);
	if (ret)
		return ret;

	
	if (ah->ah_mac_srev >= AR5K_SREV_AR5211)
		ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_5GHZ, AR5K_PHY(0));
	else
		ath5k_hw_reg_write(ah, AR5K_PHY_SHIFT_5GHZ | 0x40,
							AR5K_PHY(0));

	
	ret = ath5k_hw_write_initvals(ah, mode, skip_pcu);
	if (ret)
		return ret;

	
	ath5k_hw_init_core_clock(ah);

	ath5k_hw_tweak_initval_settings(ah, channel);

	
	ath5k_hw_commit_eeprom_settings(ah, channel);



	
	if (ah->ah_version != AR5K_AR5210) {
		if (ah->ah_mac_srev < AR5K_SREV_AR5211) {
			for (i = 0; i < 10; i++)
				ath5k_hw_reg_write(ah, s_seq[i],
					AR5K_QUEUE_DCU_SEQNUM(i));
		} else {
			ath5k_hw_reg_write(ah, s_seq[0],
				AR5K_QUEUE_DCU_SEQNUM(0));
		}

		if (ah->ah_version == AR5K_AR5211) {
			ath5k_hw_reg_write(ah, tsf_up, AR5K_TSF_U32);
			ath5k_hw_reg_write(ah, tsf_lo, AR5K_TSF_L32);
		}
	}

	
	AR5K_REG_ENABLE_BITS(ah, AR5K_PCICFG, s_led[0]);

	
	ath5k_hw_reg_write(ah, s_led[1], AR5K_GPIOCR);
	ath5k_hw_reg_write(ah, s_led[2], AR5K_GPIODO);

	ath5k_hw_pcu_init(ah, op_mode);

	ret = ath5k_hw_phy_init(ah, channel, mode, false);
	if (ret) {
		ATH5K_ERR(ah,
			"failed to initialize PHY (%i) !\n", ret);
		return ret;
	}

	ret = ath5k_hw_init_queues(ah);
	if (ret)
		return ret;


	ath5k_hw_dma_init(ah);


	if (ah->ah_use_32khz_clock && ah->ah_version == AR5K_AR5212 &&
	    op_mode != NL80211_IFTYPE_AP)
		ath5k_hw_set_sleep_clock(ah, true);

	AR5K_REG_DISABLE_BITS(ah, AR5K_BEACON, AR5K_BEACON_ENABLE);
	ath5k_hw_reset_tsf(ah);
	return 0;
}
