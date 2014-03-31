/*
 * Copyright (c) 2004-2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2008 Nick Kossifidis <mickflemm@gmail.com>
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

#define	AR5K_EEPROM_PCIE_OFFSET		0x02	
#define	AR5K_EEPROM_PCIE_SERDES_SECTION	0x40	
#define AR5K_EEPROM_MAGIC		0x003d	
#define AR5K_EEPROM_MAGIC_VALUE		0x5aa5	

#define	AR5K_EEPROM_IS_HB63		0x000b	

#define AR5K_EEPROM_RFKILL		0x0f
#define AR5K_EEPROM_RFKILL_GPIO_SEL	0x0000001c
#define AR5K_EEPROM_RFKILL_GPIO_SEL_S	2
#define AR5K_EEPROM_RFKILL_POLARITY	0x00000002
#define AR5K_EEPROM_RFKILL_POLARITY_S	1

#define AR5K_EEPROM_REG_DOMAIN		0x00bf	

#define AR5K_EEPROM_SIZE_LOWER		0x1b 
#define AR5K_EEPROM_SIZE_UPPER		0x1c 
#define AR5K_EEPROM_SIZE_UPPER_MASK	0xfff0
#define AR5K_EEPROM_SIZE_UPPER_SHIFT	4
#define AR5K_EEPROM_SIZE_ENDLOC_SHIFT	12

#define AR5K_EEPROM_CHECKSUM		0x00c0	
#define AR5K_EEPROM_INFO_BASE		0x00c0	
#define AR5K_EEPROM_INFO_MAX		(0x400 - AR5K_EEPROM_INFO_BASE)
#define AR5K_EEPROM_INFO_CKSUM		0xffff
#define AR5K_EEPROM_INFO(_n)		(AR5K_EEPROM_INFO_BASE + (_n))

#define AR5K_EEPROM_VERSION		AR5K_EEPROM_INFO(1)	
#define AR5K_EEPROM_VERSION_3_0		0x3000	
#define AR5K_EEPROM_VERSION_3_1		0x3001	
#define AR5K_EEPROM_VERSION_3_2		0x3002	
#define AR5K_EEPROM_VERSION_3_3		0x3003	
#define AR5K_EEPROM_VERSION_3_4		0x3004	
#define AR5K_EEPROM_VERSION_4_0		0x4000	
#define AR5K_EEPROM_VERSION_4_1		0x4001	
#define AR5K_EEPROM_VERSION_4_2		0x4002	
#define AR5K_EEPROM_VERSION_4_3		0x4003	
#define AR5K_EEPROM_VERSION_4_4		0x4004
#define AR5K_EEPROM_VERSION_4_5		0x4005
#define AR5K_EEPROM_VERSION_4_6		0x4006	
#define AR5K_EEPROM_VERSION_4_7		0x3007	
#define AR5K_EEPROM_VERSION_4_9		0x4009	
#define AR5K_EEPROM_VERSION_5_0		0x5000	
#define AR5K_EEPROM_VERSION_5_1		0x5001	
#define AR5K_EEPROM_VERSION_5_3		0x5003	

#define AR5K_EEPROM_MODE_11A		0
#define AR5K_EEPROM_MODE_11B		1
#define AR5K_EEPROM_MODE_11G		2

#define AR5K_EEPROM_HDR			AR5K_EEPROM_INFO(2)	
#define AR5K_EEPROM_HDR_11A(_v)		(((_v) >> AR5K_EEPROM_MODE_11A) & 0x1)
#define AR5K_EEPROM_HDR_11B(_v)		(((_v) >> AR5K_EEPROM_MODE_11B) & 0x1)
#define AR5K_EEPROM_HDR_11G(_v)		(((_v) >> AR5K_EEPROM_MODE_11G) & 0x1)
#define AR5K_EEPROM_HDR_T_2GHZ_DIS(_v)	(((_v) >> 3) & 0x1)	
#define AR5K_EEPROM_HDR_T_5GHZ_DBM(_v)	(((_v) >> 4) & 0x7f)	
#define AR5K_EEPROM_HDR_DEVICE(_v)	(((_v) >> 11) & 0x7)	
#define AR5K_EEPROM_HDR_RFKILL(_v)	(((_v) >> 14) & 0x1)	
#define AR5K_EEPROM_HDR_T_5GHZ_DIS(_v)	(((_v) >> 15) & 0x1)	

#define AR5K_EEPROM_OFF(_v, _v3_0, _v3_3) \
	(((_v) >= AR5K_EEPROM_VERSION_3_3) ? _v3_3 : _v3_0)

#define AR5K_EEPROM_ANT_GAIN(_v)	AR5K_EEPROM_OFF(_v, 0x00c4, 0x00c3)
#define AR5K_EEPROM_ANT_GAIN_5GHZ(_v)	((s8)(((_v) >> 8) & 0xff))
#define AR5K_EEPROM_ANT_GAIN_2GHZ(_v)	((s8)((_v) & 0xff))

#define AR5K_EEPROM_MISC0		AR5K_EEPROM_INFO(4)
#define AR5K_EEPROM_EARSTART(_v)	((_v) & 0xfff)
#define AR5K_EEPROM_HDR_XR2_DIS(_v)	(((_v) >> 12) & 0x1)
#define AR5K_EEPROM_HDR_XR5_DIS(_v)	(((_v) >> 13) & 0x1)
#define AR5K_EEPROM_EEMAP(_v)		(((_v) >> 14) & 0x3)

#define AR5K_EEPROM_MISC1			AR5K_EEPROM_INFO(5)
#define AR5K_EEPROM_TARGET_PWRSTART(_v)		((_v) & 0xfff)
#define AR5K_EEPROM_HAS32KHZCRYSTAL(_v)		(((_v) >> 14) & 0x1)	
#define AR5K_EEPROM_HAS32KHZCRYSTAL_OLD(_v)	(((_v) >> 15) & 0x1)

#define AR5K_EEPROM_MISC2			AR5K_EEPROM_INFO(6)
#define AR5K_EEPROM_EEP_FILE_VERSION(_v)	(((_v) >> 8) & 0xff)
#define AR5K_EEPROM_EAR_FILE_VERSION(_v)	((_v) & 0xff)

#define AR5K_EEPROM_MISC3		AR5K_EEPROM_INFO(7)
#define AR5K_EEPROM_ART_BUILD_NUM(_v)	(((_v) >> 10) & 0x3f)
#define AR5K_EEPROM_EAR_FILE_ID(_v)	((_v) & 0xff)

#define AR5K_EEPROM_MISC4		AR5K_EEPROM_INFO(8)
#define AR5K_EEPROM_CAL_DATA_START(_v)	(((_v) >> 4) & 0xfff)
#define AR5K_EEPROM_MASK_R0(_v)		(((_v) >> 2) & 0x3)	
#define AR5K_EEPROM_MASK_R1(_v)		((_v) & 0x3)		

#define AR5K_EEPROM_MISC5		AR5K_EEPROM_INFO(9)
#define AR5K_EEPROM_COMP_DIS(_v)	((_v) & 0x1)		
#define AR5K_EEPROM_AES_DIS(_v)		(((_v) >> 1) & 0x1)	
#define AR5K_EEPROM_FF_DIS(_v)		(((_v) >> 2) & 0x1)	
#define AR5K_EEPROM_BURST_DIS(_v)	(((_v) >> 3) & 0x1)	
#define AR5K_EEPROM_MAX_QCU(_v)		(((_v) >> 4) & 0xf)	
#define AR5K_EEPROM_HEAVY_CLIP_EN(_v)	(((_v) >> 8) & 0x1)	
#define AR5K_EEPROM_KEY_CACHE_SIZE(_v)	(((_v) >> 12) & 0xf)	

#define AR5K_EEPROM_MISC6		AR5K_EEPROM_INFO(10)
#define AR5K_EEPROM_TX_CHAIN_DIS	((_v) & 0x7)		
#define AR5K_EEPROM_RX_CHAIN_DIS	(((_v) >> 3) & 0x7)	
#define AR5K_EEPROM_FCC_MID_EN		(((_v) >> 6) & 0x1)	
#define AR5K_EEPROM_JAP_U1EVEN_EN	(((_v) >> 7) & 0x1)	
#define AR5K_EEPROM_JAP_U2_EN		(((_v) >> 8) & 0x1)	
#define AR5K_EEPROM_JAP_MID_EN		(((_v) >> 9) & 0x1)	
#define AR5K_EEPROM_JAP_U1ODD_EN	(((_v) >> 10) & 0x1)	
#define AR5K_EEPROM_JAP_11A_NEW_EN	(((_v) >> 11) & 0x1)	

#define AR5K_EEPROM_MODES_11A(_v)	AR5K_EEPROM_OFF(_v, 0x00c5, 0x00d4)
#define AR5K_EEPROM_MODES_11B(_v)	AR5K_EEPROM_OFF(_v, 0x00d0, 0x00f2)
#define AR5K_EEPROM_MODES_11G(_v)	AR5K_EEPROM_OFF(_v, 0x00da, 0x010d)
#define AR5K_EEPROM_CTL(_v)		AR5K_EEPROM_OFF(_v, 0x00e4, 0x0128)	
#define AR5K_EEPROM_GROUPS_START(_v)	AR5K_EEPROM_OFF(_v, 0x0100, 0x0150)	
#define AR5K_EEPROM_GROUP1_OFFSET	0x0
#define AR5K_EEPROM_GROUP2_OFFSET	0x5
#define AR5K_EEPROM_GROUP3_OFFSET	0x37
#define AR5K_EEPROM_GROUP4_OFFSET	0x46
#define AR5K_EEPROM_GROUP5_OFFSET	0x55
#define AR5K_EEPROM_GROUP6_OFFSET	0x65
#define AR5K_EEPROM_GROUP7_OFFSET	0x69
#define AR5K_EEPROM_GROUP8_OFFSET	0x6f

#define AR5K_EEPROM_TARGET_PWR_OFF_11A(_v)	AR5K_EEPROM_OFF(_v, AR5K_EEPROM_GROUPS_START(_v) + \
								AR5K_EEPROM_GROUP5_OFFSET, 0x0000)
#define AR5K_EEPROM_TARGET_PWR_OFF_11B(_v)	AR5K_EEPROM_OFF(_v, AR5K_EEPROM_GROUPS_START(_v) + \
								AR5K_EEPROM_GROUP6_OFFSET, 0x0010)
#define AR5K_EEPROM_TARGET_PWR_OFF_11G(_v)	AR5K_EEPROM_OFF(_v, AR5K_EEPROM_GROUPS_START(_v) + \
								AR5K_EEPROM_GROUP7_OFFSET, 0x0014)

#define AR5K_EEPROM_OBDB0_2GHZ		0x00ec
#define AR5K_EEPROM_OBDB1_2GHZ		0x00ed

#define AR5K_EEPROM_PROTECT		0x003f	
#define AR5K_EEPROM_PROTECT_RD_0_31	0x0001	
#define AR5K_EEPROM_PROTECT_WR_0_31	0x0002	
#define AR5K_EEPROM_PROTECT_RD_32_63	0x0004	
#define AR5K_EEPROM_PROTECT_WR_32_63	0x0008
#define AR5K_EEPROM_PROTECT_RD_64_127	0x0010	
#define AR5K_EEPROM_PROTECT_WR_64_127	0x0020
#define AR5K_EEPROM_PROTECT_RD_128_191	0x0040	
#define AR5K_EEPROM_PROTECT_WR_128_191	0x0080
#define AR5K_EEPROM_PROTECT_RD_192_207	0x0100	
#define AR5K_EEPROM_PROTECT_WR_192_207	0x0200
#define AR5K_EEPROM_PROTECT_RD_208_223	0x0400	
#define AR5K_EEPROM_PROTECT_WR_208_223	0x0800
#define AR5K_EEPROM_PROTECT_RD_224_239	0x1000	
#define AR5K_EEPROM_PROTECT_WR_224_239	0x2000
#define AR5K_EEPROM_PROTECT_RD_240_255	0x4000	
#define AR5K_EEPROM_PROTECT_WR_240_255	0x8000

#define AR5K_EEPROM_EEP_SCALE		100
#define AR5K_EEPROM_EEP_DELTA		10
#define AR5K_EEPROM_N_MODES		3
#define AR5K_EEPROM_N_5GHZ_CHAN		10
#define AR5K_EEPROM_N_2GHZ_CHAN		3
#define AR5K_EEPROM_N_2GHZ_CHAN_2413	4
#define	AR5K_EEPROM_N_2GHZ_CHAN_MAX	4
#define AR5K_EEPROM_MAX_CHAN		10
#define AR5K_EEPROM_N_PWR_POINTS_5111	11
#define AR5K_EEPROM_N_PCDAC		11
#define AR5K_EEPROM_N_PHASE_CAL		5
#define AR5K_EEPROM_N_TEST_FREQ		8
#define AR5K_EEPROM_N_EDGES		8
#define AR5K_EEPROM_N_INTERCEPTS	11
#define AR5K_EEPROM_FREQ_M(_v)		AR5K_EEPROM_OFF(_v, 0x7f, 0xff)
#define AR5K_EEPROM_PCDAC_M		0x3f
#define AR5K_EEPROM_PCDAC_START		1
#define AR5K_EEPROM_PCDAC_STOP		63
#define AR5K_EEPROM_PCDAC_STEP		1
#define AR5K_EEPROM_NON_EDGE_M		0x40
#define AR5K_EEPROM_CHANNEL_POWER	8
#define AR5K_EEPROM_N_OBDB		4
#define AR5K_EEPROM_OBDB_DIS		0xffff
#define AR5K_EEPROM_CHANNEL_DIS		0xff
#define AR5K_EEPROM_SCALE_OC_DELTA(_x)	(((_x) * 2) / 10)
#define AR5K_EEPROM_N_CTLS(_v)		AR5K_EEPROM_OFF(_v, 16, 32)
#define AR5K_EEPROM_MAX_CTLS		32
#define AR5K_EEPROM_N_PD_CURVES		4
#define AR5K_EEPROM_N_XPD0_POINTS	4
#define AR5K_EEPROM_N_XPD3_POINTS	3
#define AR5K_EEPROM_N_PD_GAINS		4
#define AR5K_EEPROM_N_PD_POINTS		5
#define AR5K_EEPROM_N_INTERCEPT_10_2GHZ	35
#define AR5K_EEPROM_N_INTERCEPT_10_5GHZ	55
#define AR5K_EEPROM_POWER_M		0x3f
#define AR5K_EEPROM_POWER_MIN		0
#define AR5K_EEPROM_POWER_MAX		3150
#define AR5K_EEPROM_POWER_STEP		50
#define AR5K_EEPROM_POWER_TABLE_SIZE	64
#define AR5K_EEPROM_N_POWER_LOC_11B	4
#define AR5K_EEPROM_N_POWER_LOC_11G	6
#define AR5K_EEPROM_I_GAIN		10
#define AR5K_EEPROM_CCK_OFDM_DELTA	15
#define AR5K_EEPROM_N_IQ_CAL		2
enum ath5k_eeprom_freq_bands {
	AR5K_EEPROM_BAND_5GHZ = 0,
	AR5K_EEPROM_BAND_2GHZ = 1,
	AR5K_EEPROM_N_FREQ_BANDS,
};
#define	AR5K_EEPROM_N_SPUR_CHANS	5
#define	AR5K_EEPROM_5413_SPUR_CHAN_1	1640
#define	AR5K_EEPROM_5413_SPUR_CHAN_2	1200
#define	AR5K_EEPROM_SPUR_CHAN_MASK	0x3FFF
#define	AR5K_EEPROM_NO_SPUR		0x8000
#define	AR5K_SPUR_CHAN_WIDTH			87
#define	AR5K_SPUR_SYMBOL_WIDTH_BASE_100Hz	3125
#define	AR5K_SPUR_SYMBOL_WIDTH_TURBO_100Hz	6250

#define AR5K_EEPROM_READ(_o, _v) do {			\
	if (!ath5k_hw_nvram_read(ah, (_o), &(_v)))	\
		return -EIO;				\
} while (0)

#define AR5K_EEPROM_READ_HDR(_o, _v)					\
	AR5K_EEPROM_READ(_o, ah->ah_capabilities.cap_eeprom._v);	\

enum ath5k_ant_table {
	AR5K_ANT_CTL		= 0,	
	AR5K_ANT_SWTABLE_A	= 1,	
	AR5K_ANT_SWTABLE_B	= 2,	
	AR5K_ANT_MAX,
};

enum ath5k_ctl_mode {
	AR5K_CTL_11A = 0,
	AR5K_CTL_11B = 1,
	AR5K_CTL_11G = 2,
	AR5K_CTL_TURBO = 3,
	AR5K_CTL_TURBOG = 4,
	AR5K_CTL_2GHT20 = 5,
	AR5K_CTL_5GHT20 = 6,
	AR5K_CTL_2GHT40 = 7,
	AR5K_CTL_5GHT40 = 8,
	AR5K_CTL_MODE_M = 15,
};

struct ath5k_chan_pcal_info_rf5111 {
	u8 pwr[AR5K_EEPROM_N_PWR_POINTS_5111];
	u8 pcdac[AR5K_EEPROM_N_PWR_POINTS_5111];
	
	u8 pcdac_min;
	
	u8 pcdac_max;
};

struct ath5k_chan_pcal_info_rf5112 {
	s8 pwr_x0[AR5K_EEPROM_N_XPD0_POINTS];
	s8 pwr_x3[AR5K_EEPROM_N_XPD3_POINTS];
	u8 pcdac_x0[AR5K_EEPROM_N_XPD0_POINTS];
	u8 pcdac_x3[AR5K_EEPROM_N_XPD3_POINTS];
};

struct ath5k_chan_pcal_info_rf2413 {
	
	s8 pwr_i[AR5K_EEPROM_N_PD_GAINS];
	u8 pddac_i[AR5K_EEPROM_N_PD_GAINS];
	s8 pwr[AR5K_EEPROM_N_PD_GAINS]
		[AR5K_EEPROM_N_PD_POINTS];
	u8 pddac[AR5K_EEPROM_N_PD_GAINS]
		[AR5K_EEPROM_N_PD_POINTS];
};

enum ath5k_powertable_type {
	AR5K_PWRTABLE_PWR_TO_PCDAC = 0,
	AR5K_PWRTABLE_LINEAR_PCDAC = 1,
	AR5K_PWRTABLE_PWR_TO_PDADC = 2,
};

struct ath5k_pdgain_info {
	u8 pd_points;
	u8 *pd_step;
	s16 *pd_pwr;
};

struct ath5k_chan_pcal_info {
	
	u16	freq;
	
	s16	max_pwr;
	s16	min_pwr;
	union {
		struct ath5k_chan_pcal_info_rf5111 rf5111_info;
		struct ath5k_chan_pcal_info_rf5112 rf5112_info;
		struct ath5k_chan_pcal_info_rf2413 rf2413_info;
	};
	struct ath5k_pdgain_info *pd_curves;
};

struct ath5k_rate_pcal_info {
	u16	freq; 
	u16	target_power_6to24;
	u16	target_power_36;
	u16	target_power_48;
	u16	target_power_54;
};

struct ath5k_edge_power {
	u16 freq;
	u16 edge; 
	bool flag;
};

struct ath5k_eeprom_info {

	
	u16	ee_magic;
	u16	ee_protect;
	u16	ee_regdomain;
	u16	ee_version;
	u16	ee_header;
	u16	ee_ant_gain;
	u8	ee_rfkill_pin;
	bool	ee_rfkill_pol;
	bool	ee_is_hb63;
	bool	ee_serdes;
	u16	ee_misc0;
	u16	ee_misc1;
	u16	ee_misc2;
	u16	ee_misc3;
	u16	ee_misc4;
	u16	ee_misc5;
	u16	ee_misc6;
	u16	ee_cck_ofdm_gain_delta;
	u16	ee_cck_ofdm_power_delta;
	u16	ee_scaled_cck_delta;

	
	u16	ee_i_cal[AR5K_EEPROM_N_MODES];
	u16	ee_q_cal[AR5K_EEPROM_N_MODES];
	u16	ee_fixed_bias[AR5K_EEPROM_N_MODES];
	u16	ee_turbo_max_power[AR5K_EEPROM_N_MODES];
	u16	ee_xr_power[AR5K_EEPROM_N_MODES];
	u16	ee_switch_settling[AR5K_EEPROM_N_MODES];
	u16	ee_atn_tx_rx[AR5K_EEPROM_N_MODES];
	u16	ee_ant_control[AR5K_EEPROM_N_MODES][AR5K_EEPROM_N_PCDAC];
	u16	ee_ob[AR5K_EEPROM_N_MODES][AR5K_EEPROM_N_OBDB];
	u16	ee_db[AR5K_EEPROM_N_MODES][AR5K_EEPROM_N_OBDB];
	u16	ee_tx_end2xlna_enable[AR5K_EEPROM_N_MODES];
	u16	ee_tx_end2xpa_disable[AR5K_EEPROM_N_MODES];
	u16	ee_tx_frm2xpa_enable[AR5K_EEPROM_N_MODES];
	u16	ee_thr_62[AR5K_EEPROM_N_MODES];
	u16	ee_xlna_gain[AR5K_EEPROM_N_MODES];
	u16	ee_xpd[AR5K_EEPROM_N_MODES];
	u16	ee_x_gain[AR5K_EEPROM_N_MODES];
	u16	ee_i_gain[AR5K_EEPROM_N_MODES];
	u16	ee_margin_tx_rx[AR5K_EEPROM_N_MODES];
	u16	ee_switch_settling_turbo[AR5K_EEPROM_N_MODES];
	u16	ee_margin_tx_rx_turbo[AR5K_EEPROM_N_MODES];
	u16	ee_atn_tx_rx_turbo[AR5K_EEPROM_N_MODES];

	
	u16	ee_false_detect[AR5K_EEPROM_N_MODES];

	
	u8	ee_pd_gains[AR5K_EEPROM_N_MODES];
	
	u8	ee_pdc_to_idx[AR5K_EEPROM_N_MODES][AR5K_EEPROM_N_PD_GAINS];

	u8	ee_n_piers[AR5K_EEPROM_N_MODES];
	struct ath5k_chan_pcal_info	ee_pwr_cal_a[AR5K_EEPROM_N_5GHZ_CHAN];
	struct ath5k_chan_pcal_info	ee_pwr_cal_b[AR5K_EEPROM_N_2GHZ_CHAN_MAX];
	struct ath5k_chan_pcal_info	ee_pwr_cal_g[AR5K_EEPROM_N_2GHZ_CHAN_MAX];

	
	u8	ee_rate_target_pwr_num[AR5K_EEPROM_N_MODES];
	struct ath5k_rate_pcal_info	ee_rate_tpwr_a[AR5K_EEPROM_N_5GHZ_CHAN];
	struct ath5k_rate_pcal_info	ee_rate_tpwr_b[AR5K_EEPROM_N_2GHZ_CHAN_MAX];
	struct ath5k_rate_pcal_info	ee_rate_tpwr_g[AR5K_EEPROM_N_2GHZ_CHAN_MAX];

	
	u8	ee_ctls;
	u8	ee_ctl[AR5K_EEPROM_MAX_CTLS];
	struct ath5k_edge_power ee_ctl_pwr[AR5K_EEPROM_N_EDGES * AR5K_EEPROM_MAX_CTLS];

	
	s16	ee_noise_floor_thr[AR5K_EEPROM_N_MODES];
	s8	ee_adc_desired_size[AR5K_EEPROM_N_MODES];
	s8	ee_pga_desired_size[AR5K_EEPROM_N_MODES];
	s8	ee_adc_desired_size_turbo[AR5K_EEPROM_N_MODES];
	s8	ee_pga_desired_size_turbo[AR5K_EEPROM_N_MODES];
	s8	ee_pd_gain_overlap;

	
	u16	ee_spur_chans[AR5K_EEPROM_N_SPUR_CHANS][AR5K_EEPROM_N_FREQ_BANDS];

	
	u32	ee_antenna[AR5K_EEPROM_N_MODES][AR5K_ANT_MAX];
};

int
ath5k_eeprom_mode_from_channel(struct ieee80211_channel *channel);
