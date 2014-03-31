#ifndef B43_RADIO_2055_H_
#define B43_RADIO_2055_H_

#include <linux/types.h>

#include "tables_nphy.h"

#define B2055_GEN_SPARE			0x00 
#define B2055_SP_PINPD			0x02 
#define B2055_C1_SP_RSSI		0x03 
#define B2055_C1_SP_PDMISC		0x04 
#define B2055_C2_SP_RSSI		0x05 
#define B2055_C2_SP_PDMISC		0x06 
#define B2055_C1_SP_RXGC1		0x07 
#define B2055_C1_SP_RXGC2		0x08 
#define B2055_C2_SP_RXGC1		0x09 
#define B2055_C2_SP_RXGC2		0x0A 
#define B2055_C1_SP_LPFBWSEL		0x0B 
#define B2055_C2_SP_LPFBWSEL		0x0C 
#define B2055_C1_SP_TXGC1		0x0D 
#define B2055_C1_SP_TXGC2		0x0E 
#define B2055_C2_SP_TXGC1		0x0F 
#define B2055_C2_SP_TXGC2		0x10 
#define B2055_MASTER1			0x11 
#define B2055_MASTER2			0x12 
#define B2055_PD_LGEN			0x13 
#define B2055_PD_PLLTS			0x14 
#define B2055_C1_PD_LGBUF		0x15 
#define B2055_C1_PD_TX			0x16 
#define B2055_C1_PD_RXTX		0x17 
#define B2055_C1_PD_RSSIMISC		0x18 
#define B2055_C2_PD_LGBUF		0x19 
#define B2055_C2_PD_TX			0x1A 
#define B2055_C2_PD_RXTX		0x1B 
#define B2055_C2_PD_RSSIMISC		0x1C 
#define B2055_PWRDET_LGEN		0x1D 
#define B2055_C1_PWRDET_LGBUF		0x1E 
#define B2055_C1_PWRDET_RXTX		0x1F 
#define B2055_C2_PWRDET_LGBUF		0x20 
#define B2055_C2_PWRDET_RXTX		0x21 
#define B2055_RRCCAL_CS			0x22 
#define B2055_RRCCAL_NOPTSEL		0x23 
#define B2055_CAL_MISC			0x24 
#define B2055_CAL_COUT			0x25 
#define B2055_CAL_COUT2			0x26 
#define B2055_CAL_CVARCTL		0x27 
#define B2055_CAL_RVARCTL		0x28 
#define B2055_CAL_LPOCTL		0x29 
#define B2055_CAL_TS			0x2A 
#define B2055_CAL_RCCALRTS		0x2B 
#define B2055_CAL_RCALRTS		0x2C 
#define B2055_PADDRV			0x2D 
#define B2055_XOCTL1			0x2E 
#define B2055_XOCTL2			0x2F 
#define B2055_XOREGUL			0x30 
#define B2055_XOMISC			0x31 
#define B2055_PLL_LFC1			0x32 
#define B2055_PLL_CALVTH		0x33 
#define B2055_PLL_LFC2			0x34 
#define B2055_PLL_REF			0x35 
#define B2055_PLL_LFR1			0x36 
#define B2055_PLL_PFDCP			0x37 
#define B2055_PLL_IDAC_CPOPAMP		0x38 
#define B2055_PLL_CPREG			0x39 
#define B2055_PLL_RCAL			0x3A 
#define B2055_RF_PLLMOD0		0x3B 
#define B2055_RF_PLLMOD1		0x3C 
#define B2055_RF_MMDIDAC1		0x3D 
#define B2055_RF_MMDIDAC0		0x3E 
#define B2055_RF_MMDSP			0x3F 
#define B2055_VCO_CAL1			0x40 
#define B2055_VCO_CAL2			0x41 
#define B2055_VCO_CAL3			0x42 
#define B2055_VCO_CAL4			0x43 
#define B2055_VCO_CAL5			0x44 
#define B2055_VCO_CAL6			0x45 
#define B2055_VCO_CAL7			0x46 
#define B2055_VCO_CAL8			0x47 
#define B2055_VCO_CAL9			0x48 
#define B2055_VCO_CAL10			0x49 
#define B2055_VCO_CAL11			0x4A 
#define B2055_VCO_CAL12			0x4B 
#define B2055_VCO_CAL13			0x4C 
#define B2055_VCO_CAL14			0x4D 
#define B2055_VCO_CAL15			0x4E 
#define B2055_VCO_CAL16			0x4F 
#define B2055_VCO_KVCO			0x50 
#define B2055_VCO_CAPTAIL		0x51 
#define B2055_VCO_IDACVCO		0x52 
#define B2055_VCO_REG			0x53 
#define B2055_PLL_RFVTH			0x54 
#define B2055_LGBUF_CENBUF		0x55 
#define B2055_LGEN_TUNE1		0x56 
#define B2055_LGEN_TUNE2		0x57 
#define B2055_LGEN_IDAC1		0x58 
#define B2055_LGEN_IDAC2		0x59 
#define B2055_LGEN_BIASC		0x5A 
#define B2055_LGEN_BIASIDAC		0x5B 
#define B2055_LGEN_RCAL			0x5C 
#define B2055_LGEN_DIV			0x5D 
#define B2055_LGEN_SPARE2		0x5E 
#define B2055_C1_LGBUF_ATUNE		0x5F 
#define B2055_C1_LGBUF_GTUNE		0x60 
#define B2055_C1_LGBUF_DIV		0x61 
#define B2055_C1_LGBUF_AIDAC		0x62 
#define B2055_C1_LGBUF_GIDAC		0x63 
#define B2055_C1_LGBUF_IDACFO		0x64 
#define B2055_C1_LGBUF_SPARE		0x65 
#define B2055_C1_RX_RFSPC1		0x66 
#define B2055_C1_RX_RFR1		0x67 
#define B2055_C1_RX_RFR2		0x68 
#define B2055_C1_RX_RFRCAL		0x69 
#define B2055_C1_RX_BB_BLCMP		0x6A 
#define B2055_C1_RX_BB_LPF		0x6B 
#define B2055_C1_RX_BB_MIDACHP		0x6C 
#define B2055_C1_RX_BB_VGA1IDAC		0x6D 
#define B2055_C1_RX_BB_VGA2IDAC		0x6E 
#define B2055_C1_RX_BB_VGA3IDAC		0x6F 
#define B2055_C1_RX_BB_BUFOCTL		0x70 
#define B2055_C1_RX_BB_RCCALCTL		0x71 
#define B2055_C1_RX_BB_RSSICTL1		0x72 
#define B2055_C1_RX_BB_RSSICTL2		0x73 
#define B2055_C1_RX_BB_RSSICTL3		0x74 
#define B2055_C1_RX_BB_RSSICTL4		0x75 
#define B2055_C1_RX_BB_RSSICTL5		0x76 
#define B2055_C1_RX_BB_REG		0x77 
#define B2055_C1_RX_BB_SPARE1		0x78 
#define B2055_C1_RX_TXBBRCAL		0x79 
#define B2055_C1_TX_RF_SPGA		0x7A 
#define B2055_C1_TX_RF_SPAD		0x7B 
#define B2055_C1_TX_RF_CNTPGA1		0x7C 
#define B2055_C1_TX_RF_CNTPAD1		0x7D 
#define B2055_C1_TX_RF_PGAIDAC		0x7E 
#define B2055_C1_TX_PGAPADTN		0x7F 
#define B2055_C1_TX_PADIDAC1		0x80 
#define B2055_C1_TX_PADIDAC2		0x81 
#define B2055_C1_TX_MXBGTRIM		0x82 
#define B2055_C1_TX_RF_RCAL		0x83 
#define B2055_C1_TX_RF_PADTSSI1		0x84 
#define B2055_C1_TX_RF_PADTSSI2		0x85 
#define B2055_C1_TX_RF_SPARE		0x86 
#define B2055_C1_TX_RF_IQCAL1		0x87 
#define B2055_C1_TX_RF_IQCAL2		0x88 
#define B2055_C1_TXBB_RCCAL		0x89 
#define B2055_C1_TXBB_LPF1		0x8A 
#define B2055_C1_TX_VOSCNCL		0x8B 
#define B2055_C1_TX_LPF_MXGMIDAC	0x8C 
#define B2055_C1_TX_BB_MXGM		0x8D 
#define B2055_C2_LGBUF_ATUNE		0x8E 
#define B2055_C2_LGBUF_GTUNE		0x8F 
#define B2055_C2_LGBUF_DIV		0x90 
#define B2055_C2_LGBUF_AIDAC		0x91 
#define B2055_C2_LGBUF_GIDAC		0x92 
#define B2055_C2_LGBUF_IDACFO		0x93 
#define B2055_C2_LGBUF_SPARE		0x94 
#define B2055_C2_RX_RFSPC1		0x95 
#define B2055_C2_RX_RFR1		0x96 
#define B2055_C2_RX_RFR2		0x97 
#define B2055_C2_RX_RFRCAL		0x98 
#define B2055_C2_RX_BB_BLCMP		0x99 
#define B2055_C2_RX_BB_LPF		0x9A 
#define B2055_C2_RX_BB_MIDACHP		0x9B 
#define B2055_C2_RX_BB_VGA1IDAC		0x9C 
#define B2055_C2_RX_BB_VGA2IDAC		0x9D 
#define B2055_C2_RX_BB_VGA3IDAC		0x9E 
#define B2055_C2_RX_BB_BUFOCTL		0x9F 
#define B2055_C2_RX_BB_RCCALCTL		0xA0 
#define B2055_C2_RX_BB_RSSICTL1		0xA1 
#define B2055_C2_RX_BB_RSSICTL2		0xA2 
#define B2055_C2_RX_BB_RSSICTL3		0xA3 
#define B2055_C2_RX_BB_RSSICTL4		0xA4 
#define B2055_C2_RX_BB_RSSICTL5		0xA5 
#define B2055_C2_RX_BB_REG		0xA6 
#define B2055_C2_RX_BB_SPARE1		0xA7 
#define B2055_C2_RX_TXBBRCAL		0xA8 
#define B2055_C2_TX_RF_SPGA		0xA9 
#define B2055_C2_TX_RF_SPAD		0xAA 
#define B2055_C2_TX_RF_CNTPGA1		0xAB 
#define B2055_C2_TX_RF_CNTPAD1		0xAC 
#define B2055_C2_TX_RF_PGAIDAC		0xAD 
#define B2055_C2_TX_PGAPADTN		0xAE 
#define B2055_C2_TX_PADIDAC1		0xAF 
#define B2055_C2_TX_PADIDAC2		0xB0 
#define B2055_C2_TX_MXBGTRIM		0xB1 
#define B2055_C2_TX_RF_RCAL		0xB2 
#define B2055_C2_TX_RF_PADTSSI1		0xB3 
#define B2055_C2_TX_RF_PADTSSI2		0xB4 
#define B2055_C2_TX_RF_SPARE		0xB5 
#define B2055_C2_TX_RF_IQCAL1		0xB6 
#define B2055_C2_TX_RF_IQCAL2		0xB7 
#define B2055_C2_TXBB_RCCAL		0xB8 
#define B2055_C2_TXBB_LPF1		0xB9 
#define B2055_C2_TX_VOSCNCL		0xBA 
#define B2055_C2_TX_LPF_MXGMIDAC	0xBB 
#define B2055_C2_TX_BB_MXGM		0xBC 
#define B2055_PRG_GCHP21		0xBD 
#define B2055_PRG_GCHP22		0xBE 
#define B2055_PRG_GCHP23		0xBF 
#define B2055_PRG_GCHP24		0xC0 
#define B2055_PRG_GCHP25		0xC1 
#define B2055_PRG_GCHP26		0xC2 
#define B2055_PRG_GCHP27		0xC3 
#define B2055_PRG_GCHP28		0xC4 
#define B2055_PRG_GCHP29		0xC5 
#define B2055_PRG_GCHP30		0xC6 
#define B2055_C1_LNA_GAINBST		0xCD 
#define B2055_C1_B0NB_RSSIVCM		0xD2 
#define B2055_C1_GENSPARE2		0xD6 
#define B2055_C2_LNA_GAINBST		0xD9 
#define B2055_C2_B0NB_RSSIVCM		0xDE 
#define B2055_C2_GENSPARE2		0xE2 

struct b43_nphy_channeltab_entry_rev2 {
	
	u8 channel;
	
	u16 freq;
	
	u16 unk2;
	
	u8 radio_pll_ref;
	u8 radio_rf_pllmod0;
	u8 radio_rf_pllmod1;
	u8 radio_vco_captail;
	u8 radio_vco_cal1;
	u8 radio_vco_cal2;
	u8 radio_pll_lfc1;
	u8 radio_pll_lfr1;
	u8 radio_pll_lfc2;
	u8 radio_lgbuf_cenbuf;
	u8 radio_lgen_tune1;
	u8 radio_lgen_tune2;
	u8 radio_c1_lgbuf_atune;
	u8 radio_c1_lgbuf_gtune;
	u8 radio_c1_rx_rfr1;
	u8 radio_c1_tx_pgapadtn;
	u8 radio_c1_tx_mxbgtrim;
	u8 radio_c2_lgbuf_atune;
	u8 radio_c2_lgbuf_gtune;
	u8 radio_c2_rx_rfr1;
	u8 radio_c2_tx_pgapadtn;
	u8 radio_c2_tx_mxbgtrim;
	
	struct b43_phy_n_sfo_cfg phy_regs;
};

void b2055_upload_inittab(struct b43_wldev *dev,
			  bool ghz5, bool ignore_uploadflag);

const struct b43_nphy_channeltab_entry_rev2 *
b43_nphy_get_chantabent_rev2(struct b43_wldev *dev, u8 channel);

#endif 
