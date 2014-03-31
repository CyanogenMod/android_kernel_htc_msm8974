/*
 * Copyright (c) 2004-2007 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2007 Nick Kossifidis <mickflemm@gmail.com>
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
 */

#ifndef _ATH5K_H
#define _ATH5K_H

#define CHAN_DEBUG	0

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/average.h>
#include <linux/leds.h>
#include <net/mac80211.h>

#include "desc.h"

#include "eeprom.h"
#include "debug.h"
#include "../ath.h"
#include "ani.h"

#define PCI_DEVICE_ID_ATHEROS_AR5210		0x0007 
#define PCI_DEVICE_ID_ATHEROS_AR5311		0x0011 
#define PCI_DEVICE_ID_ATHEROS_AR5211		0x0012 
#define PCI_DEVICE_ID_ATHEROS_AR5212		0x0013 
#define PCI_DEVICE_ID_3COM_3CRDAG675		0x0013 
#define PCI_DEVICE_ID_3COM_2_3CRPAG175		0x0013 
#define PCI_DEVICE_ID_ATHEROS_AR5210_AP		0x0207 
#define PCI_DEVICE_ID_ATHEROS_AR5212_IBM	0x1014 
#define PCI_DEVICE_ID_ATHEROS_AR5210_DEFAULT	0x1107 
#define PCI_DEVICE_ID_ATHEROS_AR5212_DEFAULT	0x1113 
#define PCI_DEVICE_ID_ATHEROS_AR5211_DEFAULT	0x1112 
#define PCI_DEVICE_ID_ATHEROS_AR5212_FPGA	0xf013 
#define PCI_DEVICE_ID_ATHEROS_AR5211_LEGACY	0xff12 
#define PCI_DEVICE_ID_ATHEROS_AR5211_FPGA11B	0xf11b 
#define PCI_DEVICE_ID_ATHEROS_AR5312_REV2	0x0052 
#define PCI_DEVICE_ID_ATHEROS_AR5312_REV7	0x0057 
#define PCI_DEVICE_ID_ATHEROS_AR5312_REV8	0x0058 
#define PCI_DEVICE_ID_ATHEROS_AR5212_0014	0x0014 
#define PCI_DEVICE_ID_ATHEROS_AR5212_0015	0x0015 
#define PCI_DEVICE_ID_ATHEROS_AR5212_0016	0x0016 
#define PCI_DEVICE_ID_ATHEROS_AR5212_0017	0x0017 
#define PCI_DEVICE_ID_ATHEROS_AR5212_0018	0x0018 
#define PCI_DEVICE_ID_ATHEROS_AR5212_0019	0x0019 
#define PCI_DEVICE_ID_ATHEROS_AR2413		0x001a 
#define PCI_DEVICE_ID_ATHEROS_AR5413		0x001b 
#define PCI_DEVICE_ID_ATHEROS_AR5424		0x001c 
#define PCI_DEVICE_ID_ATHEROS_AR5416		0x0023 
#define PCI_DEVICE_ID_ATHEROS_AR5418		0x0024 


#define ATH5K_PRINTF(fmt, ...) \
	printk(KERN_WARNING "%s: " fmt, __func__, ##__VA_ARGS__)

#define ATH5K_PRINTK(_sc, _level, _fmt, ...) \
	printk(_level "ath5k %s: " _fmt, \
		((_sc) && (_sc)->hw) ? wiphy_name((_sc)->hw->wiphy) : "", \
		##__VA_ARGS__)

#define ATH5K_PRINTK_LIMIT(_sc, _level, _fmt, ...) do { \
	if (net_ratelimit()) \
		ATH5K_PRINTK(_sc, _level, _fmt, ##__VA_ARGS__); \
	} while (0)

#define ATH5K_INFO(_sc, _fmt, ...) \
	ATH5K_PRINTK(_sc, KERN_INFO, _fmt, ##__VA_ARGS__)

#define ATH5K_WARN(_sc, _fmt, ...) \
	ATH5K_PRINTK_LIMIT(_sc, KERN_WARNING, _fmt, ##__VA_ARGS__)

#define ATH5K_ERR(_sc, _fmt, ...) \
	ATH5K_PRINTK_LIMIT(_sc, KERN_ERR, _fmt, ##__VA_ARGS__)



#define AR5K_REG_SM(_val, _flags)					\
	(((_val) << _flags##_S) & (_flags))

#define AR5K_REG_MS(_val, _flags)					\
	(((_val) & (_flags)) >> _flags##_S)

#define AR5K_REG_WRITE_BITS(ah, _reg, _flags, _val)			\
	ath5k_hw_reg_write(ah, (ath5k_hw_reg_read(ah, _reg) & ~(_flags)) | \
	    (((_val) << _flags##_S) & (_flags)), _reg)

#define AR5K_REG_MASKED_BITS(ah, _reg, _flags, _mask)			\
	ath5k_hw_reg_write(ah, (ath5k_hw_reg_read(ah, _reg) &		\
			(_mask)) | (_flags), _reg)

#define AR5K_REG_ENABLE_BITS(ah, _reg, _flags)				\
	ath5k_hw_reg_write(ah, ath5k_hw_reg_read(ah, _reg) | (_flags), _reg)

#define AR5K_REG_DISABLE_BITS(ah, _reg, _flags)			\
	ath5k_hw_reg_write(ah, ath5k_hw_reg_read(ah, _reg) & ~(_flags), _reg)

#define AR5K_REG_READ_Q(ah, _reg, _queue)				\
	(ath5k_hw_reg_read(ah, _reg) & (1 << _queue))			\

#define AR5K_REG_WRITE_Q(ah, _reg, _queue)				\
	ath5k_hw_reg_write(ah, (1 << _queue), _reg)

#define AR5K_Q_ENABLE_BITS(_reg, _queue) do {				\
	_reg |= 1 << _queue;						\
} while (0)

#define AR5K_Q_DISABLE_BITS(_reg, _queue) do {				\
	_reg &= ~(1 << _queue);						\
} while (0)

#define AR5K_REG_WAIT(_i) do {						\
	if (_i % 64)							\
		udelay(1);						\
} while (0)

#define AR5K_TUNE_DMA_BEACON_RESP		2
#define AR5K_TUNE_SW_BEACON_RESP		10
#define AR5K_TUNE_ADDITIONAL_SWBA_BACKOFF	0
#define AR5K_TUNE_MIN_TX_FIFO_THRES		1
#define AR5K_TUNE_MAX_TX_FIFO_THRES	((IEEE80211_MAX_FRAME_LEN / 64) + 1)
#define AR5K_TUNE_REGISTER_TIMEOUT		20000
#define AR5K_TUNE_RSSI_THRES			129
#define AR5K_TUNE_BMISS_THRES			7
#define AR5K_TUNE_REGISTER_DWELL_TIME		20000
#define AR5K_TUNE_BEACON_INTERVAL		100
#define AR5K_TUNE_AIFS				2
#define AR5K_TUNE_AIFS_11B			2
#define AR5K_TUNE_AIFS_XR			0
#define AR5K_TUNE_CWMIN				15
#define AR5K_TUNE_CWMIN_11B			31
#define AR5K_TUNE_CWMIN_XR			3
#define AR5K_TUNE_CWMAX				1023
#define AR5K_TUNE_CWMAX_11B			1023
#define AR5K_TUNE_CWMAX_XR			7
#define AR5K_TUNE_NOISE_FLOOR			-72
#define AR5K_TUNE_CCA_MAX_GOOD_VALUE		-95
#define AR5K_TUNE_MAX_TXPOWER			63
#define AR5K_TUNE_DEFAULT_TXPOWER		25
#define AR5K_TUNE_TPC_TXPOWER			false
#define ATH5K_TUNE_CALIBRATION_INTERVAL_FULL    60000   
#define	ATH5K_TUNE_CALIBRATION_INTERVAL_SHORT	10000	
#define ATH5K_TUNE_CALIBRATION_INTERVAL_ANI	1000	
#define ATH5K_TX_COMPLETE_POLL_INT		3000	

#define AR5K_INIT_CARR_SENSE_EN			1

#if defined(__BIG_ENDIAN)
#define AR5K_INIT_CFG	(		\
	AR5K_CFG_SWTD | AR5K_CFG_SWRD	\
)
#else
#define AR5K_INIT_CFG	0x00000000
#endif

#define	AR5K_INIT_CYCRSSI_THR1			2

#define AR5K_INIT_RETRY_SHORT			7
#define AR5K_INIT_RETRY_LONG			4

#define AR5K_INIT_SLOT_TIME_TURBO		6
#define AR5K_INIT_SLOT_TIME_DEFAULT		9
#define	AR5K_INIT_SLOT_TIME_HALF_RATE		13
#define	AR5K_INIT_SLOT_TIME_QUARTER_RATE	21
#define	AR5K_INIT_SLOT_TIME_B			20
#define AR5K_SLOT_TIME_MAX			0xffff

#define	AR5K_INIT_SIFS_TURBO			6
#define	AR5K_INIT_SIFS_DEFAULT_BG		10
#define	AR5K_INIT_SIFS_DEFAULT_A		16
#define	AR5K_INIT_SIFS_HALF_RATE		32
#define AR5K_INIT_SIFS_QUARTER_RATE		64

#define	AR5K_INIT_OFDM_PREAMPLE_TIME		20
#define	AR5K_INIT_OFDM_PREAMBLE_TIME_MIN	14
#define	AR5K_INIT_OFDM_SYMBOL_TIME		4
#define	AR5K_INIT_OFDM_PLCP_BITS		22

#define AR5K_INIT_RX_LAT_MAX			63
#define	AR5K_INIT_TX_LAT_A			54
#define	AR5K_INIT_TX_LAT_BG			384
#define	AR5K_INIT_TX_LAT_MIN			32
#define AR5K_INIT_TX_LATENCY_5210		54
#define	AR5K_INIT_RX_LATENCY_5210		29

#define AR5K_INIT_TXF2TXD_START_DEFAULT		14
#define AR5K_INIT_TXF2TXD_START_DELAY_10MHZ	12
#define AR5K_INIT_TXF2TXD_START_DELAY_5MHZ	13

#define	AR5K_SWITCH_SETTLING			5760
#define	AR5K_SWITCH_SETTLING_TURBO		7168

#define	AR5K_AGC_SETTLING			28
#define	AR5K_AGC_SETTLING_TURBO			37




enum ath5k_version {
	AR5K_AR5210	= 0,
	AR5K_AR5211	= 1,
	AR5K_AR5212	= 2,
};

enum ath5k_radio {
	AR5K_RF5110	= 0,
	AR5K_RF5111	= 1,
	AR5K_RF5112	= 2,
	AR5K_RF2413	= 3,
	AR5K_RF5413	= 4,
	AR5K_RF2316	= 5,
	AR5K_RF2317	= 6,
	AR5K_RF2425	= 7,
};


#define AR5K_SREV_UNKNOWN	0xffff

#define AR5K_SREV_AR5210	0x00 
#define AR5K_SREV_AR5311	0x10 
#define AR5K_SREV_AR5311A	0x20 
#define AR5K_SREV_AR5311B	0x30 
#define AR5K_SREV_AR5211	0x40 
#define AR5K_SREV_AR5212	0x50 
#define AR5K_SREV_AR5312_R2	0x52 
#define AR5K_SREV_AR5212_V4	0x54 
#define AR5K_SREV_AR5213	0x55 
#define AR5K_SREV_AR5312_R7	0x57 
#define AR5K_SREV_AR2313_R8	0x58 
#define AR5K_SREV_AR5213A	0x59 
#define AR5K_SREV_AR2413	0x78 
#define AR5K_SREV_AR2414	0x70 
#define AR5K_SREV_AR2315_R6	0x86 
#define AR5K_SREV_AR2315_R7	0x87 
#define AR5K_SREV_AR5424	0x90 
#define AR5K_SREV_AR2317_R1	0x90 
#define AR5K_SREV_AR2317_R2	0x91 
#define AR5K_SREV_AR5413	0xa4 
#define AR5K_SREV_AR5414	0xa0 
#define AR5K_SREV_AR2415	0xb0 
#define AR5K_SREV_AR5416	0xc0 
#define AR5K_SREV_AR5418	0xca 
#define AR5K_SREV_AR2425	0xe0 
#define AR5K_SREV_AR2417	0xf0 

#define AR5K_SREV_RAD_5110	0x00
#define AR5K_SREV_RAD_5111	0x10
#define AR5K_SREV_RAD_5111A	0x15
#define AR5K_SREV_RAD_2111	0x20
#define AR5K_SREV_RAD_5112	0x30
#define AR5K_SREV_RAD_5112A	0x35
#define	AR5K_SREV_RAD_5112B	0x36
#define AR5K_SREV_RAD_2112	0x40
#define AR5K_SREV_RAD_2112A	0x45
#define	AR5K_SREV_RAD_2112B	0x46
#define AR5K_SREV_RAD_2413	0x50
#define AR5K_SREV_RAD_5413	0x60
#define AR5K_SREV_RAD_2316	0x70 
#define AR5K_SREV_RAD_2317	0x80
#define AR5K_SREV_RAD_5424	0xa0 
#define AR5K_SREV_RAD_2425	0xa2
#define AR5K_SREV_RAD_5133	0xc0

#define AR5K_SREV_PHY_5211	0x30
#define AR5K_SREV_PHY_5212	0x41
#define	AR5K_SREV_PHY_5212A	0x42
#define AR5K_SREV_PHY_5212B	0x43
#define AR5K_SREV_PHY_2413	0x45
#define AR5K_SREV_PHY_5413	0x61
#define AR5K_SREV_PHY_2425	0x70






enum ath5k_driver_mode {
	AR5K_MODE_11A		=	0,
	AR5K_MODE_11B		=	1,
	AR5K_MODE_11G		=	2,
	AR5K_MODE_MAX		=	3
};

enum ath5k_ant_mode {
	AR5K_ANTMODE_DEFAULT	= 0,
	AR5K_ANTMODE_FIXED_A	= 1,
	AR5K_ANTMODE_FIXED_B	= 2,
	AR5K_ANTMODE_SINGLE_AP	= 3,
	AR5K_ANTMODE_SECTOR_AP	= 4,
	AR5K_ANTMODE_SECTOR_STA	= 5,
	AR5K_ANTMODE_DEBUG	= 6,
	AR5K_ANTMODE_MAX,
};

enum ath5k_bw_mode {
	AR5K_BWMODE_DEFAULT	= 0,
	AR5K_BWMODE_5MHZ	= 1,
	AR5K_BWMODE_10MHZ	= 2,
	AR5K_BWMODE_40MHZ	= 3
};




struct ath5k_tx_status {
	u16	ts_seqnum;
	u16	ts_tstamp;
	u8	ts_status;
	u8	ts_final_idx;
	u8	ts_final_retry;
	s8	ts_rssi;
	u8	ts_shortretry;
	u8	ts_virtcol;
	u8	ts_antenna;
};

#define AR5K_TXSTAT_ALTRATE	0x80
#define AR5K_TXERR_XRETRY	0x01
#define AR5K_TXERR_FILT		0x02
#define AR5K_TXERR_FIFO		0x04

enum ath5k_tx_queue {
	AR5K_TX_QUEUE_INACTIVE = 0,
	AR5K_TX_QUEUE_DATA,
	AR5K_TX_QUEUE_BEACON,
	AR5K_TX_QUEUE_CAB,
	AR5K_TX_QUEUE_UAPSD,
};

#define	AR5K_NUM_TX_QUEUES		10
#define	AR5K_NUM_TX_QUEUES_NOQCU	2

enum ath5k_tx_queue_subtype {
	AR5K_WME_AC_BK = 0,
	AR5K_WME_AC_BE,
	AR5K_WME_AC_VI,
	AR5K_WME_AC_VO,
};

enum ath5k_tx_queue_id {
	AR5K_TX_QUEUE_ID_NOQCU_DATA	= 0,
	AR5K_TX_QUEUE_ID_NOQCU_BEACON	= 1,
	AR5K_TX_QUEUE_ID_DATA_MIN	= 0,
	AR5K_TX_QUEUE_ID_DATA_MAX	= 3,
	AR5K_TX_QUEUE_ID_UAPSD		= 7,
	AR5K_TX_QUEUE_ID_CAB		= 8,
	AR5K_TX_QUEUE_ID_BEACON		= 9,
};

#define AR5K_TXQ_FLAG_TXOKINT_ENABLE		0x0001	
#define AR5K_TXQ_FLAG_TXERRINT_ENABLE		0x0002	
#define AR5K_TXQ_FLAG_TXEOLINT_ENABLE		0x0004	
#define AR5K_TXQ_FLAG_TXDESCINT_ENABLE		0x0008	
#define AR5K_TXQ_FLAG_TXURNINT_ENABLE		0x0010	
#define AR5K_TXQ_FLAG_CBRORNINT_ENABLE		0x0020	
#define AR5K_TXQ_FLAG_CBRURNINT_ENABLE		0x0040	
#define AR5K_TXQ_FLAG_QTRIGINT_ENABLE		0x0080	
#define AR5K_TXQ_FLAG_TXNOFRMINT_ENABLE		0x0100	
#define AR5K_TXQ_FLAG_BACKOFF_DISABLE		0x0200	
#define AR5K_TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE	0x0300	
#define AR5K_TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE	0x0800	
#define AR5K_TXQ_FLAG_POST_FR_BKOFF_DIS		0x1000	
#define AR5K_TXQ_FLAG_COMPRESSION_ENABLE	0x2000	

struct ath5k_txq {
	unsigned int		qnum;
	u32			*link;
	struct list_head	q;
	spinlock_t		lock;
	bool			setup;
	int			txq_len;
	int			txq_max;
	bool			txq_poll_mark;
	unsigned int		txq_stuck;
};

struct ath5k_txq_info {
	enum ath5k_tx_queue tqi_type;
	enum ath5k_tx_queue_subtype tqi_subtype;
	u16	tqi_flags;
	u8	tqi_aifs;
	u16	tqi_cw_min;
	u16	tqi_cw_max;
	u32	tqi_cbr_period;
	u32	tqi_cbr_overflow_limit;
	u32	tqi_burst_time;
	u32	tqi_ready_time;
};

enum ath5k_pkt_type {
	AR5K_PKT_TYPE_NORMAL		= 0,
	AR5K_PKT_TYPE_ATIM		= 1,
	AR5K_PKT_TYPE_PSPOLL		= 2,
	AR5K_PKT_TYPE_BEACON		= 3,
	AR5K_PKT_TYPE_PROBE_RESP	= 4,
	AR5K_PKT_TYPE_PIFS		= 5,
};

#define AR5K_TXPOWER_OFDM(_r, _v)	(			\
	((0 & 1) << ((_v) + 6)) |				\
	(((ah->ah_txpower.txp_rates_power_table[(_r)]) & 0x3f) << (_v))	\
)

#define AR5K_TXPOWER_CCK(_r, _v)	(			\
	(ah->ah_txpower.txp_rates_power_table[(_r)] & 0x3f) << (_v)	\
)




struct ath5k_rx_status {
	u16	rs_datalen;
	u16	rs_tstamp;
	u8	rs_status;
	u8	rs_phyerr;
	s8	rs_rssi;
	u8	rs_keyix;
	u8	rs_rate;
	u8	rs_antenna;
	u8	rs_more;
};

#define AR5K_RXERR_CRC		0x01
#define AR5K_RXERR_PHY		0x02
#define AR5K_RXERR_FIFO		0x04
#define AR5K_RXERR_DECRYPT	0x08
#define AR5K_RXERR_MIC		0x10
#define AR5K_RXKEYIX_INVALID	((u8) -1)
#define AR5K_TXKEYIX_INVALID	((u32) -1)



#define AR5K_BEACON_PERIOD	0x0000ffff
#define AR5K_BEACON_ENA		0x00800000 
#define AR5K_BEACON_RESET_TSF	0x01000000 


#define TSF_TO_TU(_tsf) (u32)((_tsf) >> 10)




enum ath5k_rfgain {
	AR5K_RFGAIN_INACTIVE = 0,
	AR5K_RFGAIN_ACTIVE,
	AR5K_RFGAIN_READ_REQUESTED,
	AR5K_RFGAIN_NEED_CHANGE,
};

struct ath5k_gain {
	u8			g_step_idx;
	u8			g_current;
	u8			g_target;
	u8			g_low;
	u8			g_high;
	u8			g_f_corr;
	u8			g_state;
};




#define AR5K_SLOT_TIME_9	396
#define AR5K_SLOT_TIME_20	880
#define AR5K_SLOT_TIME_MAX	0xffff

struct ath5k_athchan_2ghz {
	u32	a2_flags;
	u16	a2_athchan;
};

enum ath5k_dmasize {
	AR5K_DMASIZE_4B	= 0,
	AR5K_DMASIZE_8B,
	AR5K_DMASIZE_16B,
	AR5K_DMASIZE_32B,
	AR5K_DMASIZE_64B,
	AR5K_DMASIZE_128B,
	AR5K_DMASIZE_256B,
	AR5K_DMASIZE_512B
};




#define AR5K_MAX_RATES 32

#define ATH5K_RATE_CODE_1M	0x1B
#define ATH5K_RATE_CODE_2M	0x1A
#define ATH5K_RATE_CODE_5_5M	0x19
#define ATH5K_RATE_CODE_11M	0x18
#define ATH5K_RATE_CODE_6M	0x0B
#define ATH5K_RATE_CODE_9M	0x0F
#define ATH5K_RATE_CODE_12M	0x0A
#define ATH5K_RATE_CODE_18M	0x0E
#define ATH5K_RATE_CODE_24M	0x09
#define ATH5K_RATE_CODE_36M	0x0D
#define ATH5K_RATE_CODE_48M	0x08
#define ATH5K_RATE_CODE_54M	0x0C

#define AR5K_SET_SHORT_PREAMBLE 0x04


#define AR5K_KEYCACHE_SIZE	8
extern bool ath5k_modparam_nohwcrypt;


#define	AR5K_RSSI_EP_MULTIPLIER	(1 << 7)

#define AR5K_ASSERT_ENTRY(_e, _s) do {		\
	if (_e >= _s)				\
		return false;			\
} while (0)


enum ath5k_int {
	AR5K_INT_RXOK	= 0x00000001,
	AR5K_INT_RXDESC	= 0x00000002,
	AR5K_INT_RXERR	= 0x00000004,
	AR5K_INT_RXNOFRM = 0x00000008,
	AR5K_INT_RXEOL	= 0x00000010,
	AR5K_INT_RXORN	= 0x00000020,
	AR5K_INT_TXOK	= 0x00000040,
	AR5K_INT_TXDESC	= 0x00000080,
	AR5K_INT_TXERR	= 0x00000100,
	AR5K_INT_TXNOFRM = 0x00000200,
	AR5K_INT_TXEOL	= 0x00000400,
	AR5K_INT_TXURN	= 0x00000800,
	AR5K_INT_MIB	= 0x00001000,
	AR5K_INT_SWI	= 0x00002000,
	AR5K_INT_RXPHY	= 0x00004000,
	AR5K_INT_RXKCM	= 0x00008000,
	AR5K_INT_SWBA	= 0x00010000,
	AR5K_INT_BRSSI	= 0x00020000,
	AR5K_INT_BMISS	= 0x00040000,
	AR5K_INT_FATAL	= 0x00080000, 
	AR5K_INT_BNR	= 0x00100000, 
	AR5K_INT_TIM	= 0x00200000, 
	AR5K_INT_DTIM	= 0x00400000, 
	AR5K_INT_DTIM_SYNC =	0x00800000, 
	AR5K_INT_GPIO	=	0x01000000,
	AR5K_INT_BCN_TIMEOUT =	0x02000000, 
	AR5K_INT_CAB_TIMEOUT =	0x04000000, 
	AR5K_INT_QCBRORN =	0x08000000, 
	AR5K_INT_QCBRURN =	0x10000000, 
	AR5K_INT_QTRIG	=	0x20000000, 
	AR5K_INT_GLOBAL =	0x80000000,

	AR5K_INT_TX_ALL = AR5K_INT_TXOK
		| AR5K_INT_TXDESC
		| AR5K_INT_TXERR
		| AR5K_INT_TXNOFRM
		| AR5K_INT_TXEOL
		| AR5K_INT_TXURN,

	AR5K_INT_RX_ALL = AR5K_INT_RXOK
		| AR5K_INT_RXDESC
		| AR5K_INT_RXERR
		| AR5K_INT_RXNOFRM
		| AR5K_INT_RXEOL
		| AR5K_INT_RXORN,

	AR5K_INT_COMMON  = AR5K_INT_RXOK
		| AR5K_INT_RXDESC
		| AR5K_INT_RXERR
		| AR5K_INT_RXNOFRM
		| AR5K_INT_RXEOL
		| AR5K_INT_RXORN
		| AR5K_INT_TXOK
		| AR5K_INT_TXDESC
		| AR5K_INT_TXERR
		| AR5K_INT_TXNOFRM
		| AR5K_INT_TXEOL
		| AR5K_INT_TXURN
		| AR5K_INT_MIB
		| AR5K_INT_SWI
		| AR5K_INT_RXPHY
		| AR5K_INT_RXKCM
		| AR5K_INT_SWBA
		| AR5K_INT_BRSSI
		| AR5K_INT_BMISS
		| AR5K_INT_GPIO
		| AR5K_INT_GLOBAL,

	AR5K_INT_NOCARD	= 0xffffffff
};

enum ath5k_calibration_mask {
	AR5K_CALIBRATION_FULL = 0x01,
	AR5K_CALIBRATION_SHORT = 0x02,
	AR5K_CALIBRATION_NF = 0x04,
	AR5K_CALIBRATION_ANI = 0x08,
};

enum ath5k_power_mode {
	AR5K_PM_UNDEFINED = 0,
	AR5K_PM_AUTO,
	AR5K_PM_AWAKE,
	AR5K_PM_FULL_SLEEP,
	AR5K_PM_NETWORK_SLEEP,
};

#define AR5K_LED_INIT	0 
#define AR5K_LED_SCAN	1 
#define AR5K_LED_AUTH	2 
#define AR5K_LED_ASSOC	3 
#define AR5K_LED_RUN	4 

#define AR5K_SOFTLED_PIN	0
#define AR5K_SOFTLED_ON		0
#define AR5K_SOFTLED_OFF	1


struct ath5k_capabilities {
	DECLARE_BITMAP(cap_mode, AR5K_MODE_MAX);

	struct {
		u16	range_2ghz_min;
		u16	range_2ghz_max;
		u16	range_5ghz_min;
		u16	range_5ghz_max;
	} cap_range;

	struct ath5k_eeprom_info	cap_eeprom;

	struct {
		u8	q_tx_num;
	} cap_queues;

	bool cap_has_phyerr_counters;
	bool cap_has_mrr_support;
	bool cap_needs_2GHz_ovr;
};

#define ATH5K_NF_CAL_HIST_MAX	8
struct ath5k_nfcal_hist {
	s16 index;				
	s16 nfval[ATH5K_NF_CAL_HIST_MAX];	
};

#define ATH5K_LED_MAX_NAME_LEN 31

struct ath5k_led {
	char name[ATH5K_LED_MAX_NAME_LEN + 1];	
	struct ath5k_hw *ah;			
	struct led_classdev led_dev;		
};

struct ath5k_rfkill {
	
	u16 gpio;
	
	bool polarity;
	
	struct tasklet_struct toggleq;
};

struct ath5k_statistics {
	
	unsigned int antenna_rx[5];	
	unsigned int antenna_tx[5];	

	
	unsigned int rx_all_count;	
	unsigned int tx_all_count;	
	unsigned int rx_bytes_count;	
	unsigned int tx_bytes_count;	
	unsigned int rxerr_crc;
	unsigned int rxerr_phy;
	unsigned int rxerr_phy_code[32];
	unsigned int rxerr_fifo;
	unsigned int rxerr_decrypt;
	unsigned int rxerr_mic;
	unsigned int rxerr_proc;
	unsigned int rxerr_jumbo;
	unsigned int txerr_retry;
	unsigned int txerr_fifo;
	unsigned int txerr_filt;

	
	unsigned int ack_fail;
	unsigned int rts_fail;
	unsigned int rts_ok;
	unsigned int fcs_error;
	unsigned int beacons;

	unsigned int mib_intr;
	unsigned int rxorn_intr;
	unsigned int rxeol_intr;
};


#define AR5K_MAX_GPIO		10
#define AR5K_MAX_RF_BANKS	8

#if CHAN_DEBUG
#define ATH_CHAN_MAX	(26 + 26 + 26 + 200 + 200)
#else
#define ATH_CHAN_MAX	(14 + 14 + 14 + 252 + 20)
#endif

#define	ATH_RXBUF	40		
#define	ATH_TXBUF	200		
#define ATH_BCBUF	4		
#define ATH5K_TXQ_LEN_MAX	(ATH_TXBUF / 4)		
#define ATH5K_TXQ_LEN_LOW	(ATH5K_TXQ_LEN_MAX / 2)	

struct ath5k_hw {
	struct ath_common       common;

	struct pci_dev		*pdev;
	struct device		*dev;		
	int irq;
	u16 devid;
	void __iomem		*iobase;	
	struct mutex		lock;		
	struct ieee80211_hw	*hw;		
	struct ieee80211_supported_band sbands[IEEE80211_NUM_BANDS];
	struct ieee80211_channel channels[ATH_CHAN_MAX];
	struct ieee80211_rate	rates[IEEE80211_NUM_BANDS][AR5K_MAX_RATES];
	s8			rate_idx[IEEE80211_NUM_BANDS][AR5K_MAX_RATES];
	enum nl80211_iftype	opmode;

#ifdef CONFIG_ATH5K_DEBUG
	struct ath5k_dbg_info	debug;		
#endif 

	struct ath5k_buf	*bufptr;	
	struct ath5k_desc	*desc;		
	dma_addr_t		desc_daddr;	
	size_t			desc_len;	

	DECLARE_BITMAP(status, 4);
#define ATH_STAT_INVALID	0		
#define ATH_STAT_PROMISC	1
#define ATH_STAT_LEDSOFT	2		
#define ATH_STAT_STARTED	3		

	unsigned int		filter_flags;	
	struct ieee80211_channel *curchan;	

	u16			nvifs;

	enum ath5k_int		imask;		

	spinlock_t		irqlock;
	bool			rx_pending;	
	bool			tx_pending;	

	u8			bssidmask[ETH_ALEN];

	unsigned int		led_pin,	
				led_on;		

	struct work_struct	reset_work;	
	struct work_struct	calib_work;	

	struct list_head	rxbuf;		
	spinlock_t		rxbuflock;
	u32			*rxlink;	
	struct tasklet_struct	rxtq;		
	struct ath5k_led	rx_led;		

	struct list_head	txbuf;		
	spinlock_t		txbuflock;
	unsigned int		txbuf_len;	
	struct ath5k_txq	txqs[AR5K_NUM_TX_QUEUES];	
	struct tasklet_struct	txtq;		
	struct ath5k_led	tx_led;		

	struct ath5k_rfkill	rf_kill;

	spinlock_t		block;		
	struct tasklet_struct	beacontq;	
	struct list_head	bcbuf;		
	struct ieee80211_vif	*bslot[ATH_BCBUF];
	u16			num_ap_vifs;
	u16			num_adhoc_vifs;
	u16			num_mesh_vifs;
	unsigned int		bhalq,		
				bmisscount,	
				bintval,	
				bsent;
	unsigned int		nexttbtt;	
	struct ath5k_txq	*cabq;		

	int			power_level;	
	bool			assoc;		
	bool			enable_beacon;	

	struct ath5k_statistics	stats;

	struct ath5k_ani_state	ani_state;
	struct tasklet_struct	ani_tasklet;	

	struct delayed_work	tx_complete_work;

	struct survey_info	survey;		

	enum ath5k_int		ah_imr;

	struct ieee80211_channel *ah_current_channel;
	bool			ah_iq_cal_needed;
	bool			ah_single_chip;

	enum ath5k_version	ah_version;
	enum ath5k_radio	ah_radio;
	u32			ah_mac_srev;
	u16			ah_mac_version;
	u16			ah_phy_revision;
	u16			ah_radio_5ghz_revision;
	u16			ah_radio_2ghz_revision;

#define ah_modes		ah_capabilities.cap_mode
#define ah_ee_version		ah_capabilities.cap_eeprom.ee_version

	u8			ah_retry_long;
	u8			ah_retry_short;

	u32			ah_use_32khz_clock;

	u8			ah_coverage_class;
	bool			ah_ack_bitrate_high;
	u8			ah_bwmode;
	bool			ah_short_slot;

	
	u32			ah_ant_ctl[AR5K_EEPROM_N_MODES][AR5K_ANT_MAX];
	u8			ah_ant_mode;
	u8			ah_tx_ant;
	u8			ah_def_ant;

	struct ath5k_capabilities ah_capabilities;

	struct ath5k_txq_info	ah_txq[AR5K_NUM_TX_QUEUES];
	u32			ah_txq_status;
	u32			ah_txq_imr_txok;
	u32			ah_txq_imr_txerr;
	u32			ah_txq_imr_txurn;
	u32			ah_txq_imr_txdesc;
	u32			ah_txq_imr_txeol;
	u32			ah_txq_imr_cbrorn;
	u32			ah_txq_imr_cbrurn;
	u32			ah_txq_imr_qtrig;
	u32			ah_txq_imr_nofrm;

	u32			ah_txq_isr_txok_all;
	u32			ah_txq_isr_txurn;
	u32			ah_txq_isr_qcborn;
	u32			ah_txq_isr_qcburn;
	u32			ah_txq_isr_qtrig;

	u32			*ah_rf_banks;
	size_t			ah_rf_banks_size;
	size_t			ah_rf_regs_count;
	struct ath5k_gain	ah_gain;
	u8			ah_offset[AR5K_MAX_RF_BANKS];


	struct {
		
		u8		tmpL[AR5K_EEPROM_N_PD_GAINS]
					[AR5K_EEPROM_POWER_TABLE_SIZE];
		u8		tmpR[AR5K_EEPROM_N_PD_GAINS]
					[AR5K_EEPROM_POWER_TABLE_SIZE];
		u8		txp_pd_table[AR5K_EEPROM_POWER_TABLE_SIZE * 2];
		u16		txp_rates_power_table[AR5K_MAX_RATES];
		u8		txp_min_idx;
		bool		txp_tpc;
		
		s16		txp_min_pwr;
		s16		txp_max_pwr;
		s16		txp_cur_pwr;
		
		s16		txp_offset;
		s16		txp_ofdm;
		s16		txp_cck_ofdm_gainf_delta;
		
		s16		txp_cck_ofdm_pwr_delta;
		bool		txp_setup;
	} ah_txpower;

	struct ath5k_nfcal_hist ah_nfcal_hist;

	
	struct ewma		ah_beacon_rssi_avg;

	
	s32			ah_noise_floor;

	
	unsigned long		ah_cal_next_full;
	unsigned long		ah_cal_next_short;
	unsigned long		ah_cal_next_ani;

	
	u8			ah_cal_mask;

	int (*ah_setup_tx_desc)(struct ath5k_hw *, struct ath5k_desc *,
		unsigned int, unsigned int, int, enum ath5k_pkt_type,
		unsigned int, unsigned int, unsigned int, unsigned int,
		unsigned int, unsigned int, unsigned int, unsigned int);
	int (*ah_proc_tx_desc)(struct ath5k_hw *, struct ath5k_desc *,
		struct ath5k_tx_status *);
	int (*ah_proc_rx_desc)(struct ath5k_hw *, struct ath5k_desc *,
		struct ath5k_rx_status *);
};

struct ath_bus_ops {
	enum ath_bus_type ath_bus_type;
	void (*read_cachesize)(struct ath_common *common, int *csz);
	bool (*eeprom_read)(struct ath_common *common, u32 off, u16 *data);
	int (*eeprom_read_mac)(struct ath5k_hw *ah, u8 *mac);
};

extern const struct ieee80211_ops ath5k_hw_ops;

int ath5k_hw_init(struct ath5k_hw *ah);
void ath5k_hw_deinit(struct ath5k_hw *ah);

int ath5k_sysfs_register(struct ath5k_hw *ah);
void ath5k_sysfs_unregister(struct ath5k_hw *ah);

int ath5k_hw_read_srev(struct ath5k_hw *ah);

int ath5k_init_leds(struct ath5k_hw *ah);
void ath5k_led_enable(struct ath5k_hw *ah);
void ath5k_led_off(struct ath5k_hw *ah);
void ath5k_unregister_leds(struct ath5k_hw *ah);


int ath5k_hw_nic_wakeup(struct ath5k_hw *ah, struct ieee80211_channel *channel);
int ath5k_hw_on_hold(struct ath5k_hw *ah);
int ath5k_hw_reset(struct ath5k_hw *ah, enum nl80211_iftype op_mode,
	   struct ieee80211_channel *channel, bool fast, bool skip_pcu);
int ath5k_hw_register_timeout(struct ath5k_hw *ah, u32 reg, u32 flag, u32 val,
			      bool is_set);


unsigned int ath5k_hw_htoclock(struct ath5k_hw *ah, unsigned int usec);
unsigned int ath5k_hw_clocktoh(struct ath5k_hw *ah, unsigned int clock);
void ath5k_hw_set_clockrate(struct ath5k_hw *ah);


void ath5k_hw_start_rx_dma(struct ath5k_hw *ah);
u32 ath5k_hw_get_rxdp(struct ath5k_hw *ah);
int ath5k_hw_set_rxdp(struct ath5k_hw *ah, u32 phys_addr);
int ath5k_hw_start_tx_dma(struct ath5k_hw *ah, unsigned int queue);
int ath5k_hw_stop_beacon_queue(struct ath5k_hw *ah, unsigned int queue);
u32 ath5k_hw_get_txdp(struct ath5k_hw *ah, unsigned int queue);
int ath5k_hw_set_txdp(struct ath5k_hw *ah, unsigned int queue,
				u32 phys_addr);
int ath5k_hw_update_tx_triglevel(struct ath5k_hw *ah, bool increase);
bool ath5k_hw_is_intr_pending(struct ath5k_hw *ah);
int ath5k_hw_get_isr(struct ath5k_hw *ah, enum ath5k_int *interrupt_mask);
enum ath5k_int ath5k_hw_set_imr(struct ath5k_hw *ah, enum ath5k_int new_mask);
void ath5k_hw_update_mib_counters(struct ath5k_hw *ah);
void ath5k_hw_dma_init(struct ath5k_hw *ah);
int ath5k_hw_dma_stop(struct ath5k_hw *ah);

int ath5k_eeprom_init(struct ath5k_hw *ah);
void ath5k_eeprom_detach(struct ath5k_hw *ah);


int ath5k_hw_get_frame_duration(struct ath5k_hw *ah,
		int len, struct ieee80211_rate *rate, bool shortpre);
unsigned int ath5k_hw_get_default_slottime(struct ath5k_hw *ah);
unsigned int ath5k_hw_get_default_sifs(struct ath5k_hw *ah);
int ath5k_hw_set_opmode(struct ath5k_hw *ah, enum nl80211_iftype opmode);
void ath5k_hw_set_coverage_class(struct ath5k_hw *ah, u8 coverage_class);
int ath5k_hw_set_lladdr(struct ath5k_hw *ah, const u8 *mac);
void ath5k_hw_set_bssid(struct ath5k_hw *ah);
void ath5k_hw_set_bssid_mask(struct ath5k_hw *ah, const u8 *mask);
void ath5k_hw_set_mcast_filter(struct ath5k_hw *ah, u32 filter0, u32 filter1);
u32 ath5k_hw_get_rx_filter(struct ath5k_hw *ah);
void ath5k_hw_set_rx_filter(struct ath5k_hw *ah, u32 filter);
void ath5k_hw_start_rx_pcu(struct ath5k_hw *ah);
void ath5k_hw_stop_rx_pcu(struct ath5k_hw *ah);
u64 ath5k_hw_get_tsf64(struct ath5k_hw *ah);
void ath5k_hw_set_tsf64(struct ath5k_hw *ah, u64 tsf64);
void ath5k_hw_reset_tsf(struct ath5k_hw *ah);
void ath5k_hw_init_beacon_timers(struct ath5k_hw *ah, u32 next_beacon,
							u32 interval);
bool ath5k_hw_check_beacon_timers(struct ath5k_hw *ah, int intval);
void ath5k_hw_pcu_init(struct ath5k_hw *ah, enum nl80211_iftype op_mode);

int ath5k_hw_get_tx_queueprops(struct ath5k_hw *ah, int queue,
			       struct ath5k_txq_info *queue_info);
int ath5k_hw_set_tx_queueprops(struct ath5k_hw *ah, int queue,
			       const struct ath5k_txq_info *queue_info);
int ath5k_hw_setup_tx_queue(struct ath5k_hw *ah,
			    enum ath5k_tx_queue queue_type,
			    struct ath5k_txq_info *queue_info);
void ath5k_hw_set_tx_retry_limits(struct ath5k_hw *ah,
				  unsigned int queue);
u32 ath5k_hw_num_tx_pending(struct ath5k_hw *ah, unsigned int queue);
void ath5k_hw_release_tx_queue(struct ath5k_hw *ah, unsigned int queue);
int ath5k_hw_reset_tx_queue(struct ath5k_hw *ah, unsigned int queue);
int ath5k_hw_set_ifs_intervals(struct ath5k_hw *ah, unsigned int slot_time);
int ath5k_hw_init_queues(struct ath5k_hw *ah);

int ath5k_hw_init_desc_functions(struct ath5k_hw *ah);
int ath5k_hw_setup_rx_desc(struct ath5k_hw *ah, struct ath5k_desc *desc,
			   u32 size, unsigned int flags);
int ath5k_hw_setup_mrr_tx_desc(struct ath5k_hw *ah, struct ath5k_desc *desc,
	unsigned int tx_rate1, u_int tx_tries1, u_int tx_rate2,
	u_int tx_tries2, unsigned int tx_rate3, u_int tx_tries3);


void ath5k_hw_set_ledstate(struct ath5k_hw *ah, unsigned int state);
int ath5k_hw_set_gpio_input(struct ath5k_hw *ah, u32 gpio);
int ath5k_hw_set_gpio_output(struct ath5k_hw *ah, u32 gpio);
u32 ath5k_hw_get_gpio(struct ath5k_hw *ah, u32 gpio);
int ath5k_hw_set_gpio(struct ath5k_hw *ah, u32 gpio, u32 val);
void ath5k_hw_set_gpio_intr(struct ath5k_hw *ah, unsigned int gpio,
			    u32 interrupt_level);


void ath5k_rfkill_hw_start(struct ath5k_hw *ah);
void ath5k_rfkill_hw_stop(struct ath5k_hw *ah);


int ath5k_hw_set_capabilities(struct ath5k_hw *ah);
int ath5k_hw_enable_pspoll(struct ath5k_hw *ah, u8 *bssid, u16 assoc_id);
int ath5k_hw_disable_pspoll(struct ath5k_hw *ah);


int ath5k_hw_write_initvals(struct ath5k_hw *ah, u8 mode, bool change_channel);


u16 ath5k_hw_radio_revision(struct ath5k_hw *ah, enum ieee80211_band band);
int ath5k_hw_phy_disable(struct ath5k_hw *ah);
enum ath5k_rfgain ath5k_hw_gainf_calibrate(struct ath5k_hw *ah);
int ath5k_hw_rfgain_opt_init(struct ath5k_hw *ah);
bool ath5k_channel_ok(struct ath5k_hw *ah, struct ieee80211_channel *channel);
void ath5k_hw_init_nfcal_hist(struct ath5k_hw *ah);
int ath5k_hw_phy_calibrate(struct ath5k_hw *ah,
			   struct ieee80211_channel *channel);
void ath5k_hw_update_noise_floor(struct ath5k_hw *ah);
bool ath5k_hw_chan_has_spur_noise(struct ath5k_hw *ah,
				  struct ieee80211_channel *channel);
void ath5k_hw_set_antenna_mode(struct ath5k_hw *ah, u8 ant_mode);
void ath5k_hw_set_antenna_switch(struct ath5k_hw *ah, u8 ee_mode);
int ath5k_hw_set_txpower_limit(struct ath5k_hw *ah, u8 txpower);
int ath5k_hw_phy_init(struct ath5k_hw *ah, struct ieee80211_channel *channel,
				u8 mode, bool fast);


static inline struct ath_common *ath5k_hw_common(struct ath5k_hw *ah)
{
	return &ah->common;
}

static inline struct ath_regulatory *ath5k_hw_regulatory(struct ath5k_hw *ah)
{
	return &(ath5k_hw_common(ah)->regulatory);
}

#ifdef CONFIG_ATHEROS_AR231X
#define AR5K_AR2315_PCI_BASE	((void __iomem *)0xb0100000)

static inline void __iomem *ath5k_ahb_reg(struct ath5k_hw *ah, u16 reg)
{
	if (unlikely((reg >= 0x4000) && (reg < 0x5000) &&
	    (ah->ah_mac_srev >= AR5K_SREV_AR2315_R6)))
		return AR5K_AR2315_PCI_BASE + reg;

	return ah->iobase + reg;
}

static inline u32 ath5k_hw_reg_read(struct ath5k_hw *ah, u16 reg)
{
	return ioread32(ath5k_ahb_reg(ah, reg));
}

static inline void ath5k_hw_reg_write(struct ath5k_hw *ah, u32 val, u16 reg)
{
	iowrite32(val, ath5k_ahb_reg(ah, reg));
}

#else

static inline u32 ath5k_hw_reg_read(struct ath5k_hw *ah, u16 reg)
{
	return ioread32(ah->iobase + reg);
}

static inline void ath5k_hw_reg_write(struct ath5k_hw *ah, u32 val, u16 reg)
{
	iowrite32(val, ah->iobase + reg);
}

#endif

static inline enum ath_bus_type ath5k_get_bus_type(struct ath5k_hw *ah)
{
	return ath5k_hw_common(ah)->bus_ops->ath_bus_type;
}

static inline void ath5k_read_cachesize(struct ath_common *common, int *csz)
{
	common->bus_ops->read_cachesize(common, csz);
}

static inline bool ath5k_hw_nvram_read(struct ath5k_hw *ah, u32 off, u16 *data)
{
	struct ath_common *common = ath5k_hw_common(ah);
	return common->bus_ops->eeprom_read(common, off, data);
}

static inline u32 ath5k_hw_bitswap(u32 val, unsigned int bits)
{
	u32 retval = 0, bit, i;

	for (i = 0; i < bits; i++) {
		bit = (val >> i) & 1;
		retval = (retval << 1) | bit;
	}

	return retval;
}

#endif
