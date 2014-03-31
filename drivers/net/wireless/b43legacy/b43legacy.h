#ifndef B43legacy_H_
#define B43legacy_H_

#include <linux/hw_random.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/stringify.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/atomic.h>
#include <linux/io.h>

#include <linux/ssb/ssb.h>
#include <linux/ssb/ssb_driver_chipcommon.h>

#include <net/mac80211.h>

#include "debugfs.h"
#include "leds.h"
#include "rfkill.h"
#include "phy.h"


#define B43legacy_IRQWAIT_MAX_RETRIES	20

#define B43legacy_MMIO_DMA0_REASON	0x20
#define B43legacy_MMIO_DMA0_IRQ_MASK	0x24
#define B43legacy_MMIO_DMA1_REASON	0x28
#define B43legacy_MMIO_DMA1_IRQ_MASK	0x2C
#define B43legacy_MMIO_DMA2_REASON	0x30
#define B43legacy_MMIO_DMA2_IRQ_MASK	0x34
#define B43legacy_MMIO_DMA3_REASON	0x38
#define B43legacy_MMIO_DMA3_IRQ_MASK	0x3C
#define B43legacy_MMIO_DMA4_REASON	0x40
#define B43legacy_MMIO_DMA4_IRQ_MASK	0x44
#define B43legacy_MMIO_DMA5_REASON	0x48
#define B43legacy_MMIO_DMA5_IRQ_MASK	0x4C
#define B43legacy_MMIO_MACCTL		0x120	
#define B43legacy_MMIO_MACCMD		0x124	
#define B43legacy_MMIO_GEN_IRQ_REASON	0x128
#define B43legacy_MMIO_GEN_IRQ_MASK	0x12C
#define B43legacy_MMIO_RAM_CONTROL	0x130
#define B43legacy_MMIO_RAM_DATA		0x134
#define B43legacy_MMIO_PS_STATUS		0x140
#define B43legacy_MMIO_RADIO_HWENABLED_HI	0x158
#define B43legacy_MMIO_SHM_CONTROL	0x160
#define B43legacy_MMIO_SHM_DATA		0x164
#define B43legacy_MMIO_SHM_DATA_UNALIGNED	0x166
#define B43legacy_MMIO_XMITSTAT_0		0x170
#define B43legacy_MMIO_XMITSTAT_1		0x174
#define B43legacy_MMIO_REV3PLUS_TSF_LOW	0x180 
#define B43legacy_MMIO_REV3PLUS_TSF_HIGH	0x184 
#define B43legacy_MMIO_TSF_CFP_REP	0x188
#define B43legacy_MMIO_TSF_CFP_START	0x18C
#define B43legacy_MMIO_DMA32_BASE0	0x200
#define B43legacy_MMIO_DMA32_BASE1	0x220
#define B43legacy_MMIO_DMA32_BASE2	0x240
#define B43legacy_MMIO_DMA32_BASE3	0x260
#define B43legacy_MMIO_DMA32_BASE4	0x280
#define B43legacy_MMIO_DMA32_BASE5	0x2A0
#define B43legacy_MMIO_DMA64_BASE0	0x200
#define B43legacy_MMIO_DMA64_BASE1	0x240
#define B43legacy_MMIO_DMA64_BASE2	0x280
#define B43legacy_MMIO_DMA64_BASE3	0x2C0
#define B43legacy_MMIO_DMA64_BASE4	0x300
#define B43legacy_MMIO_DMA64_BASE5	0x340
#define B43legacy_MMIO_PIO1_BASE		0x300
#define B43legacy_MMIO_PIO2_BASE		0x310
#define B43legacy_MMIO_PIO3_BASE		0x320
#define B43legacy_MMIO_PIO4_BASE		0x330

#define B43legacy_MMIO_PHY_VER		0x3E0
#define B43legacy_MMIO_PHY_RADIO		0x3E2
#define B43legacy_MMIO_PHY0		0x3E6
#define B43legacy_MMIO_ANTENNA		0x3E8
#define B43legacy_MMIO_CHANNEL		0x3F0
#define B43legacy_MMIO_CHANNEL_EXT	0x3F4
#define B43legacy_MMIO_RADIO_CONTROL	0x3F6
#define B43legacy_MMIO_RADIO_DATA_HIGH	0x3F8
#define B43legacy_MMIO_RADIO_DATA_LOW	0x3FA
#define B43legacy_MMIO_PHY_CONTROL	0x3FC
#define B43legacy_MMIO_PHY_DATA		0x3FE
#define B43legacy_MMIO_MACFILTER_CONTROL	0x420
#define B43legacy_MMIO_MACFILTER_DATA	0x422
#define B43legacy_MMIO_RCMTA_COUNT	0x43C 
#define B43legacy_MMIO_RADIO_HWENABLED_LO	0x49A
#define B43legacy_MMIO_GPIO_CONTROL	0x49C
#define B43legacy_MMIO_GPIO_MASK		0x49E
#define B43legacy_MMIO_TSF_CFP_PRETBTT	0x612
#define B43legacy_MMIO_TSF_0		0x632 
#define B43legacy_MMIO_TSF_1		0x634 
#define B43legacy_MMIO_TSF_2		0x636 
#define B43legacy_MMIO_TSF_3		0x638 
#define B43legacy_MMIO_RNG		0x65A
#define B43legacy_MMIO_POWERUP_DELAY	0x6A8

#define B43legacy_BFL_PACTRL		0x0002
#define B43legacy_BFL_RSSI		0x0008
#define B43legacy_BFL_EXTLNA		0x1000

#define B43legacy_GPIO_CONTROL		0x6c

#define	B43legacy_SHM_SHARED		0x0001
#define	B43legacy_SHM_WIRELESS		0x0002
#define	B43legacy_SHM_HW		0x0004
#define	B43legacy_SHM_UCODE		0x0300

#define B43legacy_SHM_AUTOINC_R		0x0200 
#define B43legacy_SHM_AUTOINC_W		0x0100 
#define B43legacy_SHM_AUTOINC_RW	(B43legacy_SHM_AUTOINC_R | \
					 B43legacy_SHM_AUTOINC_W)

#define B43legacy_SHM_SH_WLCOREREV	0x0016 
#define B43legacy_SHM_SH_HOSTFLO	0x005E 
#define B43legacy_SHM_SH_HOSTFHI	0x0060 
#define B43legacy_SHM_SH_KEYIDXBLOCK	0x05D4 
#define B43legacy_SHM_SH_DTIMP		0x0012 
#define B43legacy_SHM_SH_BTL0		0x0018 
#define B43legacy_SHM_SH_BTL1		0x001A 
#define B43legacy_SHM_SH_BTSFOFF	0x001C 
#define B43legacy_SHM_SH_TIMPOS		0x001E 
#define B43legacy_SHM_SH_BEACPHYCTL	0x0054 
#define B43legacy_SHM_SH_ACKCTSPHYCTL	0x0022 
#define B43legacy_SHM_SH_PRTLEN		0x004A 
#define B43legacy_SHM_SH_PRMAXTIME	0x0074 
#define B43legacy_SHM_SH_PRPHYCTL	0x0188 
#define B43legacy_SHM_SH_OFDMDIRECT	0x0480 
#define B43legacy_SHM_SH_OFDMBASIC	0x04A0 
#define B43legacy_SHM_SH_CCKDIRECT	0x04C0 
#define B43legacy_SHM_SH_CCKBASIC	0x04E0 
#define B43legacy_SHM_SH_UCODEREV	0x0000 
#define B43legacy_SHM_SH_UCODEPATCH	0x0002 
#define B43legacy_SHM_SH_UCODEDATE	0x0004 
#define B43legacy_SHM_SH_UCODETIME	0x0006 
#define B43legacy_SHM_SH_SPUWKUP	0x0094 
#define B43legacy_SHM_SH_PRETBTT	0x0096 

#define B43legacy_UCODEFLAGS_OFFSET     0x005E

#define B43legacy_MMIO_RADIO_HWENABLED_HI_MASK (1 << 16)
#define B43legacy_MMIO_RADIO_HWENABLED_LO_MASK (1 << 4)

#define B43legacy_HF_SYMW		0x00000002 
#define B43legacy_HF_GDCW		0x00000020 
#define B43legacy_HF_OFDMPABOOST	0x00000040 
#define B43legacy_HF_EDCF		0x00000100 

#define B43legacy_MACFILTER_SELF	0x0000
#define B43legacy_MACFILTER_BSSID	0x0003
#define B43legacy_MACFILTER_MAC		0x0010

#define B43legacy_PHYTYPE_B		0x01
#define B43legacy_PHYTYPE_G		0x02

#define B43legacy_PHY_G_LO_CONTROL	0x0810
#define B43legacy_PHY_ILT_G_CTRL	0x0472
#define B43legacy_PHY_ILT_G_DATA1	0x0473
#define B43legacy_PHY_ILT_G_DATA2	0x0474
#define B43legacy_PHY_G_PCTL		0x0029
#define B43legacy_PHY_RADIO_BITFIELD	0x0401
#define B43legacy_PHY_G_CRS		0x0429
#define B43legacy_PHY_NRSSILT_CTRL	0x0803
#define B43legacy_PHY_NRSSILT_DATA	0x0804

#define B43legacy_RADIOCTL_ID		0x01

#define B43legacy_MACCTL_ENABLED	0x00000001 
#define B43legacy_MACCTL_PSM_RUN	0x00000002 
#define B43legacy_MACCTL_PSM_JMP0	0x00000004 
#define B43legacy_MACCTL_SHM_ENABLED	0x00000100 
#define B43legacy_MACCTL_IHR_ENABLED	0x00000400 
#define B43legacy_MACCTL_BE		0x00010000 
#define B43legacy_MACCTL_INFRA		0x00020000 
#define B43legacy_MACCTL_AP		0x00040000 
#define B43legacy_MACCTL_RADIOLOCK	0x00080000 
#define B43legacy_MACCTL_BEACPROMISC	0x00100000 
#define B43legacy_MACCTL_KEEP_BADPLCP	0x00200000 
#define B43legacy_MACCTL_KEEP_CTL	0x00400000 
#define B43legacy_MACCTL_KEEP_BAD	0x00800000 
#define B43legacy_MACCTL_PROMISC	0x01000000 
#define B43legacy_MACCTL_HWPS		0x02000000 
#define B43legacy_MACCTL_AWAKE		0x04000000 
#define B43legacy_MACCTL_TBTTHOLD	0x10000000 
#define B43legacy_MACCTL_GMODE		0x80000000 

#define B43legacy_MACCMD_BEACON0_VALID	0x00000001 
#define B43legacy_MACCMD_BEACON1_VALID	0x00000002 
#define B43legacy_MACCMD_DFQ_VALID	0x00000004 
#define B43legacy_MACCMD_CCA		0x00000008 
#define B43legacy_MACCMD_BGNOISE	0x00000010 

#define B43legacy_TMSLOW_GMODE		0x20000000 
#define B43legacy_TMSLOW_PLLREFSEL	0x00200000 
#define B43legacy_TMSLOW_MACPHYCLKEN	0x00100000 
#define B43legacy_TMSLOW_PHYRESET	0x00080000 
#define B43legacy_TMSLOW_PHYCLKEN	0x00040000 

#define B43legacy_TMSHIGH_FCLOCK	0x00040000 
#define B43legacy_TMSHIGH_GPHY		0x00010000 

#define B43legacy_UCODEFLAG_AUTODIV       0x0001

#define B43legacy_IRQ_MAC_SUSPENDED	0x00000001
#define B43legacy_IRQ_BEACON		0x00000002
#define B43legacy_IRQ_TBTT_INDI		0x00000004 
#define B43legacy_IRQ_BEACON_TX_OK	0x00000008
#define B43legacy_IRQ_BEACON_CANCEL	0x00000010
#define B43legacy_IRQ_ATIM_END		0x00000020
#define B43legacy_IRQ_PMQ		0x00000040
#define B43legacy_IRQ_PIO_WORKAROUND	0x00000100
#define B43legacy_IRQ_MAC_TXERR		0x00000200
#define B43legacy_IRQ_PHY_TXERR		0x00000800
#define B43legacy_IRQ_PMEVENT		0x00001000
#define B43legacy_IRQ_TIMER0		0x00002000
#define B43legacy_IRQ_TIMER1		0x00004000
#define B43legacy_IRQ_DMA		0x00008000
#define B43legacy_IRQ_TXFIFO_FLUSH_OK	0x00010000
#define B43legacy_IRQ_CCA_MEASURE_OK	0x00020000
#define B43legacy_IRQ_NOISESAMPLE_OK	0x00040000
#define B43legacy_IRQ_UCODE_DEBUG	0x08000000
#define B43legacy_IRQ_RFKILL		0x10000000
#define B43legacy_IRQ_TX_OK		0x20000000
#define B43legacy_IRQ_PHY_G_CHANGED	0x40000000
#define B43legacy_IRQ_TIMEOUT		0x80000000

#define B43legacy_IRQ_ALL		0xFFFFFFFF
#define B43legacy_IRQ_MASKTEMPLATE	(B43legacy_IRQ_MAC_SUSPENDED |	\
					 B43legacy_IRQ_TBTT_INDI |	\
					 B43legacy_IRQ_ATIM_END |	\
					 B43legacy_IRQ_PMQ |		\
					 B43legacy_IRQ_MAC_TXERR |	\
					 B43legacy_IRQ_PHY_TXERR |	\
					 B43legacy_IRQ_DMA |		\
					 B43legacy_IRQ_TXFIFO_FLUSH_OK | \
					 B43legacy_IRQ_NOISESAMPLE_OK | \
					 B43legacy_IRQ_UCODE_DEBUG |	\
					 B43legacy_IRQ_RFKILL |		\
					 B43legacy_IRQ_TX_OK)

#define B43legacy_CCK_RATE_1MB		2
#define B43legacy_CCK_RATE_2MB		4
#define B43legacy_CCK_RATE_5MB		11
#define B43legacy_CCK_RATE_11MB		22
#define B43legacy_OFDM_RATE_6MB		12
#define B43legacy_OFDM_RATE_9MB		18
#define B43legacy_OFDM_RATE_12MB	24
#define B43legacy_OFDM_RATE_18MB	36
#define B43legacy_OFDM_RATE_24MB	48
#define B43legacy_OFDM_RATE_36MB	72
#define B43legacy_OFDM_RATE_48MB	96
#define B43legacy_OFDM_RATE_54MB	108
#define B43legacy_RATE_TO_100KBPS(rate)	(((rate) * 10) / 2)


#define B43legacy_DEFAULT_SHORT_RETRY_LIMIT	7
#define B43legacy_DEFAULT_LONG_RETRY_LIMIT	4

#define B43legacy_PHY_TX_BADNESS_LIMIT		1000

#define B43legacy_SEC_KEYSIZE		16
enum {
	B43legacy_SEC_ALGO_NONE = 0, 
	B43legacy_SEC_ALGO_WEP40,
	B43legacy_SEC_ALGO_TKIP,
	B43legacy_SEC_ALGO_AES,
	B43legacy_SEC_ALGO_WEP104,
	B43legacy_SEC_ALGO_AES_LEGACY,
};

#define B43legacy_CIR_BASE                0xf00
#define B43legacy_CIR_SBTPSFLAG           (B43legacy_CIR_BASE + 0x18)
#define B43legacy_CIR_SBIMSTATE           (B43legacy_CIR_BASE + 0x90)
#define B43legacy_CIR_SBINTVEC            (B43legacy_CIR_BASE + 0x94)
#define B43legacy_CIR_SBTMSTATELOW        (B43legacy_CIR_BASE + 0x98)
#define B43legacy_CIR_SBTMSTATEHIGH       (B43legacy_CIR_BASE + 0x9c)
#define B43legacy_CIR_SBIMCONFIGLOW       (B43legacy_CIR_BASE + 0xa8)
#define B43legacy_CIR_SB_ID_HI            (B43legacy_CIR_BASE + 0xfc)

#define B43legacy_SBTMSTATEHIGH_SERROR		0x00000001
#define B43legacy_SBTMSTATEHIGH_BUSY		0x00000004
#define B43legacy_SBTMSTATEHIGH_TIMEOUT		0x00000020
#define B43legacy_SBTMSTATEHIGH_G_PHY_AVAIL	0x00010000
#define B43legacy_SBTMSTATEHIGH_COREFLAGS	0x1FFF0000
#define B43legacy_SBTMSTATEHIGH_DMA64BIT	0x10000000
#define B43legacy_SBTMSTATEHIGH_GATEDCLK	0x20000000
#define B43legacy_SBTMSTATEHIGH_BISTFAILED	0x40000000
#define B43legacy_SBTMSTATEHIGH_BISTCOMPLETE	0x80000000

#define B43legacy_SBIMSTATE_IB_ERROR		0x20000
#define B43legacy_SBIMSTATE_TIMEOUT		0x40000

#define PFX		KBUILD_MODNAME ": "
#ifdef assert
# undef assert
#endif
#ifdef CONFIG_B43LEGACY_DEBUG
# define B43legacy_WARN_ON(x)	WARN_ON(x)
# define B43legacy_BUG_ON(expr)						\
	do {								\
		if (unlikely((expr))) {					\
			printk(KERN_INFO PFX "Test (%s) failed\n",	\
					      #expr);			\
			BUG_ON(expr);					\
		}							\
	} while (0)
# define B43legacy_DEBUG	1
#else
static inline bool __b43legacy_warn_on_dummy(bool x) { return x; }
# define B43legacy_WARN_ON(x)	__b43legacy_warn_on_dummy(unlikely(!!(x)))
# define B43legacy_BUG_ON(x)	do {  } while (0)
# define B43legacy_DEBUG	0
#endif


struct net_device;
struct pci_dev;
struct b43legacy_dmaring;
struct b43legacy_pioqueue;

#define B43legacy_FW_TYPE_UCODE	'u'
#define B43legacy_FW_TYPE_PCM	'p'
#define B43legacy_FW_TYPE_IV	'i'
struct b43legacy_fw_header {
	
	u8 type;
	
	u8 ver;
	u8 __padding[2];
	__be32 size;
} __packed;

#define B43legacy_IV_OFFSET_MASK	0x7FFF
#define B43legacy_IV_32BIT		0x8000
struct b43legacy_iv {
	__be16 offset_size;
	union {
		__be16 d16;
		__be32 d32;
	} data __packed;
} __packed;

#define B43legacy_PHYMODE(phytype)	(1 << (phytype))
#define B43legacy_PHYMODE_B		B43legacy_PHYMODE	\
					((B43legacy_PHYTYPE_B))
#define B43legacy_PHYMODE_G		B43legacy_PHYMODE	\
					((B43legacy_PHYTYPE_G))

struct b43legacy_lopair {
	s8 low;
	s8 high;
	u8 used:1;
};
#define B43legacy_LO_COUNT	(14*4)

struct b43legacy_phy {
	
	u8 possible_phymodes;
	
	bool gmode;

	
	u8 analog;
	
	u8 type;
	
	u8 rev;

	u16 antenna_diversity;
	u16 savedpctlreg;
	
	u16 radio_manuf;	
	u16 radio_ver;		
	u8 calibrated:1;
	u8 radio_rev;		

	bool dyn_tssi_tbl;	

	
	bool aci_enable;
	bool aci_wlan_automatic;
	bool aci_hw_rssi;

	
	bool radio_on;
	struct {
		bool valid;
		u16 rfover;
		u16 rfoverval;
	} radio_off_context;

	u16 minlowsig[2];
	u16 minlowsigpos[2];

	struct b43legacy_lopair *_lo_pairs;
	
	const s8 *tssi2dbm;
	
	s8 idle_tssi;
	
	int tgt_idle_tssi;
	
	int cur_idle_tssi;

	
	struct b43legacy_txpower_lo_control *lo_control;
	
	s16 max_lb_gain;	
	s16 trsw_rx_gain;	
	s16 lna_lod_gain;	
	s16 lna_gain;		
	s16 pga_gain;		

	u8 power_level;

	
	u16 loopback_gain[2];

	
	
	struct {
		
		u16 rfatt;
		
		u16 bbatt;
		
		u16 txctl1;
		u16 txctl2;
	};
	
	struct {
		u16 txpwr_offset;
	};

	
	int interfmode;
#define B43legacy_INTERFSTACK_SIZE	26
	u32 interfstack[B43legacy_INTERFSTACK_SIZE];

	
	s16 nrssi[2];
	s32 nrssislope;
	
	s8 nrssi_lt[64];

	
	u8 channel;

	u16 lofcal;

	u16 initval;

	
	atomic_t txerr_cnt;

#if B43legacy_DEBUG
	
	bool manual_txpower_control;
	
	bool phy_locked;
#endif 
};

struct b43legacy_dma {
	struct b43legacy_dmaring *tx_ring0;
	struct b43legacy_dmaring *tx_ring1;
	struct b43legacy_dmaring *tx_ring2;
	struct b43legacy_dmaring *tx_ring3;
	struct b43legacy_dmaring *tx_ring4;
	struct b43legacy_dmaring *tx_ring5;

	struct b43legacy_dmaring *rx_ring0;
	struct b43legacy_dmaring *rx_ring3; 

	u32 translation; 
};

struct b43legacy_pio {
	struct b43legacy_pioqueue *queue0;
	struct b43legacy_pioqueue *queue1;
	struct b43legacy_pioqueue *queue2;
	struct b43legacy_pioqueue *queue3;
};

struct b43legacy_noise_calculation {
	u8 channel_at_start;
	bool calculation_running;
	u8 nr_samples;
	s8 samples[8][4];
};

struct b43legacy_stats {
	u8 link_noise;
	
	unsigned long last_tx;
	unsigned long last_rx;
};

struct b43legacy_key {
	void *keyconf;
	bool enabled;
	u8 algorithm;
};

#define B43legacy_QOS_QUEUE_NUM	4

struct b43legacy_wldev;

struct b43legacy_qos_params {
	
	struct ieee80211_tx_queue_params p;
};

struct b43legacy_wl {
	
	struct b43legacy_wldev *current_dev;
	
	struct ieee80211_hw *hw;

	spinlock_t irq_lock;		
	struct mutex mutex;		
	spinlock_t leds_lock;		

	
	struct work_struct firmware_load;


	struct ieee80211_vif *vif;
	
	u8 mac_addr[ETH_ALEN];
	
	u8 bssid[ETH_ALEN];
	
	int if_type;
	
	bool operating;
	
	unsigned int filter_flags;
	
	struct ieee80211_low_level_stats ieee_stats;

#ifdef CONFIG_B43LEGACY_HWRNG
	struct hwrng rng;
	u8 rng_initialized;
	char rng_name[30 + 1];
#endif

	
	struct list_head devlist;
	u8 nr_devs;

	bool radiotap_enabled;
	bool radio_enabled;

	struct sk_buff *current_beacon;
	bool beacon0_uploaded;
	bool beacon1_uploaded;
	bool beacon_templates_virgin; 
	struct work_struct beacon_update_trigger;
	
	struct b43legacy_qos_params qos_params[B43legacy_QOS_QUEUE_NUM];

	
	struct work_struct tx_work;

	
	struct sk_buff_head tx_queue[B43legacy_QOS_QUEUE_NUM];

	
	bool tx_queue_stopped[B43legacy_QOS_QUEUE_NUM];

};

struct b43legacy_firmware {
	
	const struct firmware *ucode;
	
	const struct firmware *pcm;
	
	const struct firmware *initvals;
	
	const struct firmware *initvals_band;
	
	u16 rev;
	
	u16 patch;
};

enum {
	B43legacy_STAT_UNINIT		= 0, 
	B43legacy_STAT_INITIALIZED	= 1, 
	B43legacy_STAT_STARTED	= 2, 
};
#define b43legacy_status(wldev)	atomic_read(&(wldev)->__init_status)
#define b43legacy_set_status(wldev, stat)	do {		\
		atomic_set(&(wldev)->__init_status, (stat));	\
		smp_wmb();					\
					} while (0)


struct b43legacy_wldev {
	struct ssb_device *dev;
	struct b43legacy_wl *wl;

	atomic_t __init_status;
	
	int suspend_init_status;

	bool __using_pio;	
	bool bad_frames_preempt;
	bool dfq_valid;		
	bool short_preamble;	
	bool radio_hw_enable;	

	
	struct b43legacy_phy phy;
	union {
		
		struct b43legacy_dma dma;
		
		struct b43legacy_pio pio;
	};

	
	struct b43legacy_stats stats;

	
	struct b43legacy_led led_tx;
	struct b43legacy_led led_rx;
	struct b43legacy_led led_assoc;
	struct b43legacy_led led_radio;

	
	u32 irq_reason;
	u32 dma_reason[6];
	
	u32 irq_mask;
	
	struct b43legacy_noise_calculation noisecalc;
	
	int mac_suspended;

	
	struct tasklet_struct isr_tasklet;

	
	struct delayed_work periodic_work;
	unsigned int periodic_state;

	struct work_struct restart_work;

	
	u16 ktp; 
	u8 max_nr_keys;
	struct b43legacy_key key[58];

	
	struct b43legacy_firmware fw;

	
	struct list_head list;

	
#ifdef CONFIG_B43LEGACY_DEBUG
	struct b43legacy_dfsentry *dfsentry;
#endif
};


static inline
struct b43legacy_wl *hw_to_b43legacy_wl(struct ieee80211_hw *hw)
{
	return hw->priv;
}

#if defined(CONFIG_B43LEGACY_DMA) && defined(CONFIG_B43LEGACY_PIO)
static inline
int b43legacy_using_pio(struct b43legacy_wldev *dev)
{
	return dev->__using_pio;
}
#elif defined(CONFIG_B43LEGACY_DMA)
static inline
int b43legacy_using_pio(struct b43legacy_wldev *dev)
{
	return 0;
}
#elif defined(CONFIG_B43LEGACY_PIO)
static inline
int b43legacy_using_pio(struct b43legacy_wldev *dev)
{
	return 1;
}
#else
# error "Using neither DMA nor PIO? Confused..."
#endif


static inline
struct b43legacy_wldev *dev_to_b43legacy_wldev(struct device *dev)
{
	struct ssb_device *ssb_dev = dev_to_ssb_dev(dev);
	return ssb_get_drvdata(ssb_dev);
}

static inline
int b43legacy_is_mode(struct b43legacy_wl *wl, int type)
{
	return (wl->operating &&
		wl->if_type == type);
}

static inline
bool is_bcm_board_vendor(struct b43legacy_wldev *dev)
{
	return  (dev->dev->bus->boardinfo.vendor == PCI_VENDOR_ID_BROADCOM);
}

static inline
u16 b43legacy_read16(struct b43legacy_wldev *dev, u16 offset)
{
	return ssb_read16(dev->dev, offset);
}

static inline
void b43legacy_write16(struct b43legacy_wldev *dev, u16 offset, u16 value)
{
	ssb_write16(dev->dev, offset, value);
}

static inline
u32 b43legacy_read32(struct b43legacy_wldev *dev, u16 offset)
{
	return ssb_read32(dev->dev, offset);
}

static inline
void b43legacy_write32(struct b43legacy_wldev *dev, u16 offset, u32 value)
{
	ssb_write32(dev->dev, offset, value);
}

static inline
struct b43legacy_lopair *b43legacy_get_lopair(struct b43legacy_phy *phy,
					      u16 radio_attenuation,
					      u16 baseband_attenuation)
{
	return phy->_lo_pairs + (radio_attenuation
			+ 14 * (baseband_attenuation / 2));
}



__printf(2, 3)
void b43legacyinfo(struct b43legacy_wl *wl, const char *fmt, ...);
__printf(2, 3)
void b43legacyerr(struct b43legacy_wl *wl, const char *fmt, ...);
__printf(2, 3)
void b43legacywarn(struct b43legacy_wl *wl, const char *fmt, ...);
#if B43legacy_DEBUG
__printf(2, 3)
void b43legacydbg(struct b43legacy_wl *wl, const char *fmt, ...);
#else 
# define b43legacydbg(wl, fmt...) do {  } while (0)
#endif 

#define Q52_FMT		"%u.%u"
#define Q52_ARG(q52)	((q52) / 4), (((q52) & 3) * 100 / 4)

#endif 
