/*
 * DaVinci Ethernet Medium Access Controller
 *
 * DaVinci EMAC is based upon CPPI 3.0 TI DMA engine
 *
 * Copyright (C) 2009 Texas Instruments.
 *
 * ---------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ---------------------------------------------------------------------------
 * History:
 * 0-5 A number of folks worked on this driver in bits and pieces but the major
 *     contribution came from Suraj Iyer and Anant Gole
 * 6.0 Anant Gole - rewrote the driver as per Linux conventions
 * 6.1 Chaithrika U S - added support for Gigabit and RMII features,
 *     PHY layer usage
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/phy.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/davinci_emac.h>

#include <asm/irq.h>
#include <asm/page.h>

#include "davinci_cpdma.h"

static int debug_level;
module_param(debug_level, int, 0);
MODULE_PARM_DESC(debug_level, "DaVinci EMAC debug level (NETIF_MSG bits)");

#define DAVINCI_EMAC_DEBUG	(NETIF_MSG_DRV | \
				NETIF_MSG_PROBE | \
				NETIF_MSG_LINK | \
				NETIF_MSG_TIMER | \
				NETIF_MSG_IFDOWN | \
				NETIF_MSG_IFUP | \
				NETIF_MSG_RX_ERR | \
				NETIF_MSG_TX_ERR | \
				NETIF_MSG_TX_QUEUED | \
				NETIF_MSG_INTR | \
				NETIF_MSG_TX_DONE | \
				NETIF_MSG_RX_STATUS | \
				NETIF_MSG_PKTDATA | \
				NETIF_MSG_HW | \
				NETIF_MSG_WOL)

#define EMAC_MAJOR_VERSION	6
#define EMAC_MINOR_VERSION	1
#define EMAC_MODULE_VERSION	"6.1"
MODULE_VERSION(EMAC_MODULE_VERSION);
static const char emac_version_string[] = "TI DaVinci EMAC Linux v6.1";

#define EMAC_DEF_PASS_CRC		(0) 
#define EMAC_DEF_QOS_EN			(0) 
#define EMAC_DEF_NO_BUFF_CHAIN		(0) 
#define EMAC_DEF_MACCTRL_FRAME_EN	(0) 
#define EMAC_DEF_SHORT_FRAME_EN		(0) 
#define EMAC_DEF_ERROR_FRAME_EN		(0) 
#define EMAC_DEF_PROM_EN		(0) 
#define EMAC_DEF_PROM_CH		(0) 
#define EMAC_DEF_BCAST_EN		(1) 
#define EMAC_DEF_BCAST_CH		(0) 
#define EMAC_DEF_MCAST_EN		(1) 
#define EMAC_DEF_MCAST_CH		(0) 

#define EMAC_DEF_TXPRIO_FIXED		(1) 
#define EMAC_DEF_TXPACING_EN		(0) 

#define EMAC_DEF_BUFFER_OFFSET		(0) 
#define EMAC_DEF_MIN_ETHPKTSIZE		(60) 
#define EMAC_DEF_MAX_FRAME_SIZE		(1500 + 14 + 4 + 4)
#define EMAC_DEF_TX_CH			(0) 
#define EMAC_DEF_RX_CH			(0) 
#define EMAC_DEF_RX_NUM_DESC		(128)
#define EMAC_DEF_TX_NUM_DESC		(128)
#define EMAC_DEF_MAX_TX_CH		(1) 
#define EMAC_DEF_MAX_RX_CH		(1) 
#define EMAC_POLL_WEIGHT		(64) 

#define EMAC_DEF_TX_MAX_SERVICE		(32) 
#define EMAC_DEF_RX_MAX_SERVICE		(64) 

#define EMAC_ALL_MULTI_REG_VALUE	(0xFFFFFFFF)
#define EMAC_NUM_MULTICAST_BITS		(64)
#define EMAC_TX_CONTROL_TX_ENABLE_VAL	(0x1)
#define EMAC_RX_CONTROL_RX_ENABLE_VAL	(0x1)
#define EMAC_MAC_HOST_ERR_INTMASK_VAL	(0x2)
#define EMAC_RX_UNICAST_CLEAR_ALL	(0xFF)
#define EMAC_INT_MASK_CLEAR		(0xFF)

#define EMAC_RXMBP_PASSCRC_MASK		BIT(30)
#define EMAC_RXMBP_QOSEN_MASK		BIT(29)
#define EMAC_RXMBP_NOCHAIN_MASK		BIT(28)
#define EMAC_RXMBP_CMFEN_MASK		BIT(24)
#define EMAC_RXMBP_CSFEN_MASK		BIT(23)
#define EMAC_RXMBP_CEFEN_MASK		BIT(22)
#define EMAC_RXMBP_CAFEN_MASK		BIT(21)
#define EMAC_RXMBP_PROMCH_SHIFT		(16)
#define EMAC_RXMBP_PROMCH_MASK		(0x7 << 16)
#define EMAC_RXMBP_BROADEN_MASK		BIT(13)
#define EMAC_RXMBP_BROADCH_SHIFT	(8)
#define EMAC_RXMBP_BROADCH_MASK		(0x7 << 8)
#define EMAC_RXMBP_MULTIEN_MASK		BIT(5)
#define EMAC_RXMBP_MULTICH_SHIFT	(0)
#define EMAC_RXMBP_MULTICH_MASK		(0x7)
#define EMAC_RXMBP_CHMASK		(0x7)

# define EMAC_MBP_RXPROMISC		(0x00200000)
# define EMAC_MBP_PROMISCCH(ch)		(((ch) & 0x7) << 16)
# define EMAC_MBP_RXBCAST		(0x00002000)
# define EMAC_MBP_BCASTCHAN(ch)		(((ch) & 0x7) << 8)
# define EMAC_MBP_RXMCAST		(0x00000020)
# define EMAC_MBP_MCASTCHAN(ch)		((ch) & 0x7)

#define EMAC_MACCONTROL_TXPTYPE		BIT(9)
#define EMAC_MACCONTROL_TXPACEEN	BIT(6)
#define EMAC_MACCONTROL_GMIIEN		BIT(5)
#define EMAC_MACCONTROL_GIGABITEN	BIT(7)
#define EMAC_MACCONTROL_FULLDUPLEXEN	BIT(0)
#define EMAC_MACCONTROL_RMIISPEED_MASK	BIT(15)

#define EMAC_DM646X_MACCONTORL_GIG	BIT(7)
#define EMAC_DM646X_MACCONTORL_GIGFORCE	BIT(17)

#define EMAC_MACSTATUS_TXERRCODE_MASK	(0xF00000)
#define EMAC_MACSTATUS_TXERRCODE_SHIFT	(20)
#define EMAC_MACSTATUS_TXERRCH_MASK	(0x7)
#define EMAC_MACSTATUS_TXERRCH_SHIFT	(16)
#define EMAC_MACSTATUS_RXERRCODE_MASK	(0xF000)
#define EMAC_MACSTATUS_RXERRCODE_SHIFT	(12)
#define EMAC_MACSTATUS_RXERRCH_MASK	(0x7)
#define EMAC_MACSTATUS_RXERRCH_SHIFT	(8)

#define EMAC_RX_MAX_LEN_MASK		(0xFFFF)
#define EMAC_RX_BUFFER_OFFSET_MASK	(0xFFFF)

#define EMAC_DM644X_MAC_IN_VECTOR_HOST_INT	BIT(17)
#define EMAC_DM644X_MAC_IN_VECTOR_STATPEND_INT	BIT(16)
#define EMAC_DM644X_MAC_IN_VECTOR_RX_INT_VEC	BIT(8)
#define EMAC_DM644X_MAC_IN_VECTOR_TX_INT_VEC	BIT(0)

#define EMAC_DM646X_MAC_IN_VECTOR_RX_INT_VEC	BIT(EMAC_DEF_RX_CH)
#define EMAC_DM646X_MAC_IN_VECTOR_TX_INT_VEC	BIT(16 + EMAC_DEF_TX_CH)
#define EMAC_DM646X_MAC_IN_VECTOR_HOST_INT	BIT(26)
#define EMAC_DM646X_MAC_IN_VECTOR_STATPEND_INT	BIT(27)

#define EMAC_CPPI_SOP_BIT		BIT(31)
#define EMAC_CPPI_EOP_BIT		BIT(30)
#define EMAC_CPPI_OWNERSHIP_BIT		BIT(29)
#define EMAC_CPPI_EOQ_BIT		BIT(28)
#define EMAC_CPPI_TEARDOWN_COMPLETE_BIT BIT(27)
#define EMAC_CPPI_PASS_CRC_BIT		BIT(26)
#define EMAC_RX_BD_BUF_SIZE		(0xFFFF)
#define EMAC_BD_LENGTH_FOR_CACHE	(16) 
#define EMAC_RX_BD_PKT_LENGTH_MASK	(0xFFFF)

#define EMAC_MAX_TXRX_CHANNELS		 (8)  
#define EMAC_DEF_MAX_MULTICAST_ADDRESSES (64) 

#define EMAC_MACINVECTOR	0x90

#define EMAC_DM646X_MACEOIVECTOR	0x94

#define EMAC_MACINTSTATRAW	0xB0
#define EMAC_MACINTSTATMASKED	0xB4
#define EMAC_MACINTMASKSET	0xB8
#define EMAC_MACINTMASKCLEAR	0xBC

#define EMAC_RXMBPENABLE	0x100
#define EMAC_RXUNICASTSET	0x104
#define EMAC_RXUNICASTCLEAR	0x108
#define EMAC_RXMAXLEN		0x10C
#define EMAC_RXBUFFEROFFSET	0x110
#define EMAC_RXFILTERLOWTHRESH	0x114

#define EMAC_MACCONTROL		0x160
#define EMAC_MACSTATUS		0x164
#define EMAC_EMCONTROL		0x168
#define EMAC_FIFOCONTROL	0x16C
#define EMAC_MACCONFIG		0x170
#define EMAC_SOFTRESET		0x174
#define EMAC_MACSRCADDRLO	0x1D0
#define EMAC_MACSRCADDRHI	0x1D4
#define EMAC_MACHASH1		0x1D8
#define EMAC_MACHASH2		0x1DC
#define EMAC_MACADDRLO		0x500
#define EMAC_MACADDRHI		0x504
#define EMAC_MACINDEX		0x508

#define EMAC_RXGOODFRAMES	0x200
#define EMAC_RXBCASTFRAMES	0x204
#define EMAC_RXMCASTFRAMES	0x208
#define EMAC_RXPAUSEFRAMES	0x20C
#define EMAC_RXCRCERRORS	0x210
#define EMAC_RXALIGNCODEERRORS	0x214
#define EMAC_RXOVERSIZED	0x218
#define EMAC_RXJABBER		0x21C
#define EMAC_RXUNDERSIZED	0x220
#define EMAC_RXFRAGMENTS	0x224
#define EMAC_RXFILTERED		0x228
#define EMAC_RXQOSFILTERED	0x22C
#define EMAC_RXOCTETS		0x230
#define EMAC_TXGOODFRAMES	0x234
#define EMAC_TXBCASTFRAMES	0x238
#define EMAC_TXMCASTFRAMES	0x23C
#define EMAC_TXPAUSEFRAMES	0x240
#define EMAC_TXDEFERRED		0x244
#define EMAC_TXCOLLISION	0x248
#define EMAC_TXSINGLECOLL	0x24C
#define EMAC_TXMULTICOLL	0x250
#define EMAC_TXEXCESSIVECOLL	0x254
#define EMAC_TXLATECOLL		0x258
#define EMAC_TXUNDERRUN		0x25C
#define EMAC_TXCARRIERSENSE	0x260
#define EMAC_TXOCTETS		0x264
#define EMAC_NETOCTETS		0x280
#define EMAC_RXSOFOVERRUNS	0x284
#define EMAC_RXMOFOVERRUNS	0x288
#define EMAC_RXDMAOVERRUNS	0x28C

#define EMAC_CTRL_EWCTL		(0x4)
#define EMAC_CTRL_EWINTTCNT	(0x8)

#define EMAC_DM644X_EWINTCNT_MASK	0x1FFFF
#define EMAC_DM644X_INTMIN_INTVL	0x1
#define EMAC_DM644X_INTMAX_INTVL	(EMAC_DM644X_EWINTCNT_MASK)

#define EMAC_DM646X_CMINTCTRL	0x0C
#define EMAC_DM646X_CMRXINTEN	0x14
#define EMAC_DM646X_CMTXINTEN	0x18
#define EMAC_DM646X_CMRXINTMAX	0x70
#define EMAC_DM646X_CMTXINTMAX	0x74

#define EMAC_DM646X_INTPACEEN		(0x3 << 16)
#define EMAC_DM646X_INTPRESCALE_MASK	(0x7FF << 0)
#define EMAC_DM646X_CMINTMAX_CNT	63
#define EMAC_DM646X_CMINTMIN_CNT	2
#define EMAC_DM646X_CMINTMAX_INTVL	(1000 / EMAC_DM646X_CMINTMIN_CNT)
#define EMAC_DM646X_CMINTMIN_INTVL	((1000 / EMAC_DM646X_CMINTMAX_CNT) + 1)


#define EMAC_DM646X_MAC_EOI_C0_RXEN	(0x01)
#define EMAC_DM646X_MAC_EOI_C0_TXEN	(0x02)

#define EMAC_STATS_CLR_MASK    (0xFFFFFFFF)

struct emac_priv {
	u32 msg_enable;
	struct net_device *ndev;
	struct platform_device *pdev;
	struct napi_struct napi;
	char mac_addr[6];
	void __iomem *remap_addr;
	u32 emac_base_phys;
	void __iomem *emac_base;
	void __iomem *ctrl_base;
	struct cpdma_ctlr *dma;
	struct cpdma_chan *txchan;
	struct cpdma_chan *rxchan;
	u32 link; 
	u32 speed; 
	u32 duplex; 
	u32 rx_buf_size;
	u32 isr_count;
	u32 coal_intvl;
	u32 bus_freq_mhz;
	u8 rmii_en;
	u8 version;
	u32 mac_hash1;
	u32 mac_hash2;
	u32 multicast_hash_cnt[EMAC_NUM_MULTICAST_BITS];
	u32 rx_addr_type;
	atomic_t cur_tx;
	const char *phy_id;
	struct phy_device *phydev;
	spinlock_t lock;
	
	void (*int_enable) (void);
	void (*int_disable) (void);
};

static struct clk *emac_clk;
static unsigned long emac_bus_frequency;

static char *emac_txhost_errcodes[16] = {
	"No error", "SOP error", "Ownership bit not set in SOP buffer",
	"Zero Next Buffer Descriptor Pointer Without EOP",
	"Zero Buffer Pointer", "Zero Buffer Length", "Packet Length Error",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved"
};

static char *emac_rxhost_errcodes[16] = {
	"No error", "Reserved", "Ownership bit not set in input buffer",
	"Reserved", "Zero Buffer Pointer", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
	"Reserved", "Reserved", "Reserved", "Reserved"
};

#define emac_read(reg)		  ioread32(priv->emac_base + (reg))
#define emac_write(reg, val)      iowrite32(val, priv->emac_base + (reg))

#define emac_ctrl_read(reg)	  ioread32((priv->ctrl_base + (reg)))
#define emac_ctrl_write(reg, val) iowrite32(val, (priv->ctrl_base + (reg)))

static void emac_dump_regs(struct emac_priv *priv)
{
	struct device *emac_dev = &priv->ndev->dev;

	
	dev_info(emac_dev, "EMAC Basic registers\n");
	if (priv->version == EMAC_VERSION_1) {
		dev_info(emac_dev, "EMAC: EWCTL: %08X, EWINTTCNT: %08X\n",
			emac_ctrl_read(EMAC_CTRL_EWCTL),
			emac_ctrl_read(EMAC_CTRL_EWINTTCNT));
	}
	dev_info(emac_dev, "EMAC: EmuControl:%08X, FifoControl: %08X\n",
		emac_read(EMAC_EMCONTROL), emac_read(EMAC_FIFOCONTROL));
	dev_info(emac_dev, "EMAC: MBPEnable:%08X, RXUnicastSet: %08X, "\
		"RXMaxLen=%08X\n", emac_read(EMAC_RXMBPENABLE),
		emac_read(EMAC_RXUNICASTSET), emac_read(EMAC_RXMAXLEN));
	dev_info(emac_dev, "EMAC: MacControl:%08X, MacStatus: %08X, "\
		"MacConfig=%08X\n", emac_read(EMAC_MACCONTROL),
		emac_read(EMAC_MACSTATUS), emac_read(EMAC_MACCONFIG));
	dev_info(emac_dev, "EMAC Statistics\n");
	dev_info(emac_dev, "EMAC: rx_good_frames:%d\n",
		emac_read(EMAC_RXGOODFRAMES));
	dev_info(emac_dev, "EMAC: rx_broadcast_frames:%d\n",
		emac_read(EMAC_RXBCASTFRAMES));
	dev_info(emac_dev, "EMAC: rx_multicast_frames:%d\n",
		emac_read(EMAC_RXMCASTFRAMES));
	dev_info(emac_dev, "EMAC: rx_pause_frames:%d\n",
		emac_read(EMAC_RXPAUSEFRAMES));
	dev_info(emac_dev, "EMAC: rx_crcerrors:%d\n",
		emac_read(EMAC_RXCRCERRORS));
	dev_info(emac_dev, "EMAC: rx_align_code_errors:%d\n",
		emac_read(EMAC_RXALIGNCODEERRORS));
	dev_info(emac_dev, "EMAC: rx_oversized_frames:%d\n",
		emac_read(EMAC_RXOVERSIZED));
	dev_info(emac_dev, "EMAC: rx_jabber_frames:%d\n",
		emac_read(EMAC_RXJABBER));
	dev_info(emac_dev, "EMAC: rx_undersized_frames:%d\n",
		emac_read(EMAC_RXUNDERSIZED));
	dev_info(emac_dev, "EMAC: rx_fragments:%d\n",
		emac_read(EMAC_RXFRAGMENTS));
	dev_info(emac_dev, "EMAC: rx_filtered_frames:%d\n",
		emac_read(EMAC_RXFILTERED));
	dev_info(emac_dev, "EMAC: rx_qos_filtered_frames:%d\n",
		emac_read(EMAC_RXQOSFILTERED));
	dev_info(emac_dev, "EMAC: rx_octets:%d\n",
		emac_read(EMAC_RXOCTETS));
	dev_info(emac_dev, "EMAC: tx_goodframes:%d\n",
		emac_read(EMAC_TXGOODFRAMES));
	dev_info(emac_dev, "EMAC: tx_bcastframes:%d\n",
		emac_read(EMAC_TXBCASTFRAMES));
	dev_info(emac_dev, "EMAC: tx_mcastframes:%d\n",
		emac_read(EMAC_TXMCASTFRAMES));
	dev_info(emac_dev, "EMAC: tx_pause_frames:%d\n",
		emac_read(EMAC_TXPAUSEFRAMES));
	dev_info(emac_dev, "EMAC: tx_deferred_frames:%d\n",
		emac_read(EMAC_TXDEFERRED));
	dev_info(emac_dev, "EMAC: tx_collision_frames:%d\n",
		emac_read(EMAC_TXCOLLISION));
	dev_info(emac_dev, "EMAC: tx_single_coll_frames:%d\n",
		emac_read(EMAC_TXSINGLECOLL));
	dev_info(emac_dev, "EMAC: tx_mult_coll_frames:%d\n",
		emac_read(EMAC_TXMULTICOLL));
	dev_info(emac_dev, "EMAC: tx_excessive_collisions:%d\n",
		emac_read(EMAC_TXEXCESSIVECOLL));
	dev_info(emac_dev, "EMAC: tx_late_collisions:%d\n",
		emac_read(EMAC_TXLATECOLL));
	dev_info(emac_dev, "EMAC: tx_underrun:%d\n",
		emac_read(EMAC_TXUNDERRUN));
	dev_info(emac_dev, "EMAC: tx_carrier_sense_errors:%d\n",
		emac_read(EMAC_TXCARRIERSENSE));
	dev_info(emac_dev, "EMAC: tx_octets:%d\n",
		emac_read(EMAC_TXOCTETS));
	dev_info(emac_dev, "EMAC: net_octets:%d\n",
		emac_read(EMAC_NETOCTETS));
	dev_info(emac_dev, "EMAC: rx_sof_overruns:%d\n",
		emac_read(EMAC_RXSOFOVERRUNS));
	dev_info(emac_dev, "EMAC: rx_mof_overruns:%d\n",
		emac_read(EMAC_RXMOFOVERRUNS));
	dev_info(emac_dev, "EMAC: rx_dma_overruns:%d\n",
		emac_read(EMAC_RXDMAOVERRUNS));

	cpdma_ctlr_dump(priv->dma);
}

static void emac_get_drvinfo(struct net_device *ndev,
			     struct ethtool_drvinfo *info)
{
	strcpy(info->driver, emac_version_string);
	strcpy(info->version, EMAC_MODULE_VERSION);
}

static int emac_get_settings(struct net_device *ndev,
			     struct ethtool_cmd *ecmd)
{
	struct emac_priv *priv = netdev_priv(ndev);
	if (priv->phydev)
		return phy_ethtool_gset(priv->phydev, ecmd);
	else
		return -EOPNOTSUPP;

}

static int emac_set_settings(struct net_device *ndev, struct ethtool_cmd *ecmd)
{
	struct emac_priv *priv = netdev_priv(ndev);
	if (priv->phydev)
		return phy_ethtool_sset(priv->phydev, ecmd);
	else
		return -EOPNOTSUPP;

}

static int emac_get_coalesce(struct net_device *ndev,
				struct ethtool_coalesce *coal)
{
	struct emac_priv *priv = netdev_priv(ndev);

	coal->rx_coalesce_usecs = priv->coal_intvl;
	return 0;

}

static int emac_set_coalesce(struct net_device *ndev,
				struct ethtool_coalesce *coal)
{
	struct emac_priv *priv = netdev_priv(ndev);
	u32 int_ctrl, num_interrupts = 0;
	u32 prescale = 0, addnl_dvdr = 1, coal_intvl = 0;

	if (!coal->rx_coalesce_usecs)
		return -EINVAL;

	coal_intvl = coal->rx_coalesce_usecs;

	switch (priv->version) {
	case EMAC_VERSION_2:
		int_ctrl =  emac_ctrl_read(EMAC_DM646X_CMINTCTRL);
		prescale = priv->bus_freq_mhz * 4;

		if (coal_intvl < EMAC_DM646X_CMINTMIN_INTVL)
			coal_intvl = EMAC_DM646X_CMINTMIN_INTVL;

		if (coal_intvl > EMAC_DM646X_CMINTMAX_INTVL) {
			addnl_dvdr = EMAC_DM646X_INTPRESCALE_MASK / prescale;

			if (addnl_dvdr > 1) {
				prescale *= addnl_dvdr;
				if (coal_intvl > (EMAC_DM646X_CMINTMAX_INTVL
							* addnl_dvdr))
					coal_intvl = (EMAC_DM646X_CMINTMAX_INTVL
							* addnl_dvdr);
			} else {
				addnl_dvdr = 1;
				coal_intvl = EMAC_DM646X_CMINTMAX_INTVL;
			}
		}

		num_interrupts = (1000 * addnl_dvdr) / coal_intvl;

		int_ctrl |= EMAC_DM646X_INTPACEEN;
		int_ctrl &= (~EMAC_DM646X_INTPRESCALE_MASK);
		int_ctrl |= (prescale & EMAC_DM646X_INTPRESCALE_MASK);
		emac_ctrl_write(EMAC_DM646X_CMINTCTRL, int_ctrl);

		emac_ctrl_write(EMAC_DM646X_CMRXINTMAX, num_interrupts);
		emac_ctrl_write(EMAC_DM646X_CMTXINTMAX, num_interrupts);

		break;
	default:
		int_ctrl = emac_ctrl_read(EMAC_CTRL_EWINTTCNT);
		int_ctrl &= (~EMAC_DM644X_EWINTCNT_MASK);
		prescale = coal_intvl * priv->bus_freq_mhz;
		if (prescale > EMAC_DM644X_EWINTCNT_MASK) {
			prescale = EMAC_DM644X_EWINTCNT_MASK;
			coal_intvl = prescale / priv->bus_freq_mhz;
		}
		emac_ctrl_write(EMAC_CTRL_EWINTTCNT, (int_ctrl | prescale));

		break;
	}

	printk(KERN_INFO"Set coalesce to %d usecs.\n", coal_intvl);
	priv->coal_intvl = coal_intvl;

	return 0;

}


static const struct ethtool_ops ethtool_ops = {
	.get_drvinfo = emac_get_drvinfo,
	.get_settings = emac_get_settings,
	.set_settings = emac_set_settings,
	.get_link = ethtool_op_get_link,
	.get_coalesce = emac_get_coalesce,
	.set_coalesce =  emac_set_coalesce,
};

static void emac_update_phystatus(struct emac_priv *priv)
{
	u32 mac_control;
	u32 new_duplex;
	u32 cur_duplex;
	struct net_device *ndev = priv->ndev;

	mac_control = emac_read(EMAC_MACCONTROL);
	cur_duplex = (mac_control & EMAC_MACCONTROL_FULLDUPLEXEN) ?
			DUPLEX_FULL : DUPLEX_HALF;
	if (priv->phydev)
		new_duplex = priv->phydev->duplex;
	else
		new_duplex = DUPLEX_FULL;

	
	if ((priv->link) && (new_duplex != cur_duplex)) {
		priv->duplex = new_duplex;
		if (DUPLEX_FULL == priv->duplex)
			mac_control |= (EMAC_MACCONTROL_FULLDUPLEXEN);
		else
			mac_control &= ~(EMAC_MACCONTROL_FULLDUPLEXEN);
	}

	if (priv->speed == SPEED_1000 && (priv->version == EMAC_VERSION_2)) {
		mac_control = emac_read(EMAC_MACCONTROL);
		mac_control |= (EMAC_DM646X_MACCONTORL_GIG |
				EMAC_DM646X_MACCONTORL_GIGFORCE);
	} else {
		
		mac_control &= ~(EMAC_DM646X_MACCONTORL_GIGFORCE |
					EMAC_DM646X_MACCONTORL_GIG);

		if (priv->rmii_en && (priv->speed == SPEED_100))
			mac_control |= EMAC_MACCONTROL_RMIISPEED_MASK;
		else
			mac_control &= ~EMAC_MACCONTROL_RMIISPEED_MASK;
	}

	
	emac_write(EMAC_MACCONTROL, mac_control);

	if (priv->link) {
		
		if (!netif_carrier_ok(ndev))
			netif_carrier_on(ndev);
	
		if (netif_running(ndev) && netif_queue_stopped(ndev))
			netif_wake_queue(ndev);
	} else {
		
		if (netif_carrier_ok(ndev))
			netif_carrier_off(ndev);
		if (!netif_queue_stopped(ndev))
			netif_stop_queue(ndev);
	}
}

static u32 hash_get(u8 *addr)
{
	u32 hash;
	u8 tmpval;
	int cnt;
	hash = 0;

	for (cnt = 0; cnt < 2; cnt++) {
		tmpval = *addr++;
		hash ^= (tmpval >> 2) ^ (tmpval << 4);
		tmpval = *addr++;
		hash ^= (tmpval >> 4) ^ (tmpval << 2);
		tmpval = *addr++;
		hash ^= (tmpval >> 6) ^ (tmpval);
	}

	return hash & 0x3F;
}

static int hash_add(struct emac_priv *priv, u8 *mac_addr)
{
	struct device *emac_dev = &priv->ndev->dev;
	u32 rc = 0;
	u32 hash_bit;
	u32 hash_value = hash_get(mac_addr);

	if (hash_value >= EMAC_NUM_MULTICAST_BITS) {
		if (netif_msg_drv(priv)) {
			dev_err(emac_dev, "DaVinci EMAC: hash_add(): Invalid "\
				"Hash %08x, should not be greater than %08x",
				hash_value, (EMAC_NUM_MULTICAST_BITS - 1));
		}
		return -1;
	}

	
	if (priv->multicast_hash_cnt[hash_value] == 0) {
		rc = 1; 
		if (hash_value < 32) {
			hash_bit = BIT(hash_value);
			priv->mac_hash1 |= hash_bit;
		} else {
			hash_bit = BIT((hash_value - 32));
			priv->mac_hash2 |= hash_bit;
		}
	}

	
	++priv->multicast_hash_cnt[hash_value];

	return rc;
}

static int hash_del(struct emac_priv *priv, u8 *mac_addr)
{
	u32 hash_value;
	u32 hash_bit;

	hash_value = hash_get(mac_addr);
	if (priv->multicast_hash_cnt[hash_value] > 0) {
		
		--priv->multicast_hash_cnt[hash_value];
	}

	if (priv->multicast_hash_cnt[hash_value] > 0)
		return 0;

	if (hash_value < 32) {
		hash_bit = BIT(hash_value);
		priv->mac_hash1 &= ~hash_bit;
	} else {
		hash_bit = BIT((hash_value - 32));
		priv->mac_hash2 &= ~hash_bit;
	}

	
	return 1;
}

#define EMAC_MULTICAST_ADD	0
#define EMAC_MULTICAST_DEL	1
#define EMAC_ALL_MULTI_SET	2
#define EMAC_ALL_MULTI_CLR	3

static void emac_add_mcast(struct emac_priv *priv, u32 action, u8 *mac_addr)
{
	struct device *emac_dev = &priv->ndev->dev;
	int update = -1;

	switch (action) {
	case EMAC_MULTICAST_ADD:
		update = hash_add(priv, mac_addr);
		break;
	case EMAC_MULTICAST_DEL:
		update = hash_del(priv, mac_addr);
		break;
	case EMAC_ALL_MULTI_SET:
		update = 1;
		priv->mac_hash1 = EMAC_ALL_MULTI_REG_VALUE;
		priv->mac_hash2 = EMAC_ALL_MULTI_REG_VALUE;
		break;
	case EMAC_ALL_MULTI_CLR:
		update = 1;
		priv->mac_hash1 = 0;
		priv->mac_hash2 = 0;
		memset(&(priv->multicast_hash_cnt[0]), 0,
		sizeof(priv->multicast_hash_cnt[0]) *
		       EMAC_NUM_MULTICAST_BITS);
		break;
	default:
		if (netif_msg_drv(priv))
			dev_err(emac_dev, "DaVinci EMAC: add_mcast"\
				": bad operation %d", action);
		break;
	}

	
	if (update > 0) {
		emac_write(EMAC_MACHASH1, priv->mac_hash1);
		emac_write(EMAC_MACHASH2, priv->mac_hash2);
	}
}

static void emac_dev_mcast_set(struct net_device *ndev)
{
	u32 mbp_enable;
	struct emac_priv *priv = netdev_priv(ndev);

	mbp_enable = emac_read(EMAC_RXMBPENABLE);
	if (ndev->flags & IFF_PROMISC) {
		mbp_enable &= (~EMAC_MBP_PROMISCCH(EMAC_DEF_PROM_CH));
		mbp_enable |= (EMAC_MBP_RXPROMISC);
	} else {
		mbp_enable = (mbp_enable & ~EMAC_MBP_RXPROMISC);
		if ((ndev->flags & IFF_ALLMULTI) ||
		    netdev_mc_count(ndev) > EMAC_DEF_MAX_MULTICAST_ADDRESSES) {
			mbp_enable = (mbp_enable | EMAC_MBP_RXMCAST);
			emac_add_mcast(priv, EMAC_ALL_MULTI_SET, NULL);
		}
		if (!netdev_mc_empty(ndev)) {
			struct netdev_hw_addr *ha;

			mbp_enable = (mbp_enable | EMAC_MBP_RXMCAST);
			emac_add_mcast(priv, EMAC_ALL_MULTI_CLR, NULL);
			
			netdev_for_each_mc_addr(ha, ndev) {
				emac_add_mcast(priv, EMAC_MULTICAST_ADD,
					       (u8 *) ha->addr);
			}
		} else {
			mbp_enable = (mbp_enable & ~EMAC_MBP_RXMCAST);
			emac_add_mcast(priv, EMAC_ALL_MULTI_CLR, NULL);
		}
	}
	
	emac_write(EMAC_RXMBPENABLE, mbp_enable);
}


static void emac_int_disable(struct emac_priv *priv)
{
	if (priv->version == EMAC_VERSION_2) {
		unsigned long flags;

		local_irq_save(flags);

		emac_ctrl_write(EMAC_DM646X_CMRXINTEN, 0x0);
		emac_ctrl_write(EMAC_DM646X_CMTXINTEN, 0x0);
		
		if (priv->int_disable)
			priv->int_disable();

		local_irq_restore(flags);

	} else {
		
		emac_ctrl_write(EMAC_CTRL_EWCTL, 0x0);
	}
}

static void emac_int_enable(struct emac_priv *priv)
{
	if (priv->version == EMAC_VERSION_2) {
		if (priv->int_enable)
			priv->int_enable();

		emac_ctrl_write(EMAC_DM646X_CMRXINTEN, 0xff);
		emac_ctrl_write(EMAC_DM646X_CMTXINTEN, 0xff);


		

		
		emac_write(EMAC_DM646X_MACEOIVECTOR,
			EMAC_DM646X_MAC_EOI_C0_RXEN);

		
		emac_write(EMAC_DM646X_MACEOIVECTOR,
			EMAC_DM646X_MAC_EOI_C0_TXEN);

	} else {
		
		emac_ctrl_write(EMAC_CTRL_EWCTL, 0x1);
	}
}

static irqreturn_t emac_irq(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct emac_priv *priv = netdev_priv(ndev);

	++priv->isr_count;
	if (likely(netif_running(priv->ndev))) {
		emac_int_disable(priv);
		napi_schedule(&priv->napi);
	} else {
		
	}
	return IRQ_HANDLED;
}

static struct sk_buff *emac_rx_alloc(struct emac_priv *priv)
{
	struct sk_buff *skb = netdev_alloc_skb(priv->ndev, priv->rx_buf_size);
	if (WARN_ON(!skb))
		return NULL;
	skb_reserve(skb, NET_IP_ALIGN);
	return skb;
}

static void emac_rx_handler(void *token, int len, int status)
{
	struct sk_buff		*skb = token;
	struct net_device	*ndev = skb->dev;
	struct emac_priv	*priv = netdev_priv(ndev);
	struct device		*emac_dev = &ndev->dev;
	int			ret;

	
	if (unlikely(!netif_running(ndev))) {
		dev_kfree_skb_any(skb);
		return;
	}

	
	if (status < 0) {
		ndev->stats.rx_errors++;
		goto recycle;
	}

	
	skb_put(skb, len);
	skb->protocol = eth_type_trans(skb, ndev);
	netif_receive_skb(skb);
	ndev->stats.rx_bytes += len;
	ndev->stats.rx_packets++;

	
	skb = emac_rx_alloc(priv);
	if (!skb) {
		if (netif_msg_rx_err(priv) && net_ratelimit())
			dev_err(emac_dev, "failed rx buffer alloc\n");
		return;
	}

recycle:
	ret = cpdma_chan_submit(priv->rxchan, skb, skb->data,
			skb_tailroom(skb), GFP_KERNEL);

	WARN_ON(ret == -ENOMEM);
	if (unlikely(ret < 0))
		dev_kfree_skb_any(skb);
}

static void emac_tx_handler(void *token, int len, int status)
{
	struct sk_buff		*skb = token;
	struct net_device	*ndev = skb->dev;
	struct emac_priv	*priv = netdev_priv(ndev);

	atomic_dec(&priv->cur_tx);

	if (unlikely(netif_queue_stopped(ndev)))
		netif_start_queue(ndev);
	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += len;
	dev_kfree_skb_any(skb);
}

static int emac_dev_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct device *emac_dev = &ndev->dev;
	int ret_code;
	struct emac_priv *priv = netdev_priv(ndev);

	
	if (unlikely(!priv->link)) {
		if (netif_msg_tx_err(priv) && net_ratelimit())
			dev_err(emac_dev, "DaVinci EMAC: No link to transmit");
		goto fail_tx;
	}

	ret_code = skb_padto(skb, EMAC_DEF_MIN_ETHPKTSIZE);
	if (unlikely(ret_code < 0)) {
		if (netif_msg_tx_err(priv) && net_ratelimit())
			dev_err(emac_dev, "DaVinci EMAC: packet pad failed");
		goto fail_tx;
	}

	skb_tx_timestamp(skb);

	ret_code = cpdma_chan_submit(priv->txchan, skb, skb->data, skb->len,
				     GFP_KERNEL);
	if (unlikely(ret_code != 0)) {
		if (netif_msg_tx_err(priv) && net_ratelimit())
			dev_err(emac_dev, "DaVinci EMAC: desc submit failed");
		goto fail_tx;
	}

	if (atomic_inc_return(&priv->cur_tx) >= EMAC_DEF_TX_NUM_DESC)
		netif_stop_queue(ndev);

	return NETDEV_TX_OK;

fail_tx:
	ndev->stats.tx_dropped++;
	netif_stop_queue(ndev);
	return NETDEV_TX_BUSY;
}

static void emac_dev_tx_timeout(struct net_device *ndev)
{
	struct emac_priv *priv = netdev_priv(ndev);
	struct device *emac_dev = &ndev->dev;

	if (netif_msg_tx_err(priv))
		dev_err(emac_dev, "DaVinci EMAC: xmit timeout, restarting TX");

	emac_dump_regs(priv);

	ndev->stats.tx_errors++;
	emac_int_disable(priv);
	cpdma_chan_stop(priv->txchan);
	cpdma_chan_start(priv->txchan);
	emac_int_enable(priv);
}

static void emac_set_type0addr(struct emac_priv *priv, u32 ch, char *mac_addr)
{
	u32 val;
	val = ((mac_addr[5] << 8) | (mac_addr[4]));
	emac_write(EMAC_MACSRCADDRLO, val);

	val = ((mac_addr[3] << 24) | (mac_addr[2] << 16) | \
	       (mac_addr[1] << 8) | (mac_addr[0]));
	emac_write(EMAC_MACSRCADDRHI, val);
	val = emac_read(EMAC_RXUNICASTSET);
	val |= BIT(ch);
	emac_write(EMAC_RXUNICASTSET, val);
	val = emac_read(EMAC_RXUNICASTCLEAR);
	val &= ~BIT(ch);
	emac_write(EMAC_RXUNICASTCLEAR, val);
}

static void emac_set_type1addr(struct emac_priv *priv, u32 ch, char *mac_addr)
{
	u32 val;
	emac_write(EMAC_MACINDEX, ch);
	val = ((mac_addr[5] << 8) | mac_addr[4]);
	emac_write(EMAC_MACADDRLO, val);
	val = ((mac_addr[3] << 24) | (mac_addr[2] << 16) | \
	       (mac_addr[1] << 8) | (mac_addr[0]));
	emac_write(EMAC_MACADDRHI, val);
	emac_set_type0addr(priv, ch, mac_addr);
}

static void emac_set_type2addr(struct emac_priv *priv, u32 ch,
			       char *mac_addr, int index, int match)
{
	u32 val;
	emac_write(EMAC_MACINDEX, index);
	val = ((mac_addr[3] << 24) | (mac_addr[2] << 16) | \
	       (mac_addr[1] << 8) | (mac_addr[0]));
	emac_write(EMAC_MACADDRHI, val);
	val = ((mac_addr[5] << 8) | mac_addr[4] | ((ch & 0x7) << 16) | \
	       (match << 19) | BIT(20));
	emac_write(EMAC_MACADDRLO, val);
	emac_set_type0addr(priv, ch, mac_addr);
}

static void emac_setmac(struct emac_priv *priv, u32 ch, char *mac_addr)
{
	struct device *emac_dev = &priv->ndev->dev;

	if (priv->rx_addr_type == 0) {
		emac_set_type0addr(priv, ch, mac_addr);
	} else if (priv->rx_addr_type == 1) {
		u32 cnt;
		for (cnt = 0; cnt < EMAC_MAX_TXRX_CHANNELS; cnt++)
			emac_set_type1addr(priv, ch, mac_addr);
	} else if (priv->rx_addr_type == 2) {
		emac_set_type2addr(priv, ch, mac_addr, ch, 1);
		emac_set_type0addr(priv, ch, mac_addr);
	} else {
		if (netif_msg_drv(priv))
			dev_err(emac_dev, "DaVinci EMAC: Wrong addressing\n");
	}
}

static int emac_dev_setmac_addr(struct net_device *ndev, void *addr)
{
	struct emac_priv *priv = netdev_priv(ndev);
	struct device *emac_dev = &priv->ndev->dev;
	struct sockaddr *sa = addr;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	
	memcpy(priv->mac_addr, sa->sa_data, ndev->addr_len);
	memcpy(ndev->dev_addr, sa->sa_data, ndev->addr_len);
	ndev->addr_assign_type &= ~NET_ADDR_RANDOM;

	
	if (netif_running(ndev)) {
		emac_setmac(priv, EMAC_DEF_RX_CH, priv->mac_addr);
	}

	if (netif_msg_drv(priv))
		dev_notice(emac_dev, "DaVinci EMAC: emac_dev_setmac_addr %pM\n",
					priv->mac_addr);

	return 0;
}

static int emac_hw_enable(struct emac_priv *priv)
{
	u32 val, mbp_enable, mac_control;

	
	emac_write(EMAC_SOFTRESET, 1);
	while (emac_read(EMAC_SOFTRESET))
		cpu_relax();

	
	emac_int_disable(priv);

	
	mac_control =
		(((EMAC_DEF_TXPRIO_FIXED) ? (EMAC_MACCONTROL_TXPTYPE) : 0x0) |
		((priv->speed == 1000) ? EMAC_MACCONTROL_GIGABITEN : 0x0) |
		((EMAC_DEF_TXPACING_EN) ? (EMAC_MACCONTROL_TXPACEEN) : 0x0) |
		((priv->duplex == DUPLEX_FULL) ? 0x1 : 0));
	emac_write(EMAC_MACCONTROL, mac_control);

	mbp_enable =
		(((EMAC_DEF_PASS_CRC) ? (EMAC_RXMBP_PASSCRC_MASK) : 0x0) |
		((EMAC_DEF_QOS_EN) ? (EMAC_RXMBP_QOSEN_MASK) : 0x0) |
		 ((EMAC_DEF_NO_BUFF_CHAIN) ? (EMAC_RXMBP_NOCHAIN_MASK) : 0x0) |
		 ((EMAC_DEF_MACCTRL_FRAME_EN) ? (EMAC_RXMBP_CMFEN_MASK) : 0x0) |
		 ((EMAC_DEF_SHORT_FRAME_EN) ? (EMAC_RXMBP_CSFEN_MASK) : 0x0) |
		 ((EMAC_DEF_ERROR_FRAME_EN) ? (EMAC_RXMBP_CEFEN_MASK) : 0x0) |
		 ((EMAC_DEF_PROM_EN) ? (EMAC_RXMBP_CAFEN_MASK) : 0x0) |
		 ((EMAC_DEF_PROM_CH & EMAC_RXMBP_CHMASK) << \
			EMAC_RXMBP_PROMCH_SHIFT) |
		 ((EMAC_DEF_BCAST_EN) ? (EMAC_RXMBP_BROADEN_MASK) : 0x0) |
		 ((EMAC_DEF_BCAST_CH & EMAC_RXMBP_CHMASK) << \
			EMAC_RXMBP_BROADCH_SHIFT) |
		 ((EMAC_DEF_MCAST_EN) ? (EMAC_RXMBP_MULTIEN_MASK) : 0x0) |
		 ((EMAC_DEF_MCAST_CH & EMAC_RXMBP_CHMASK) << \
			EMAC_RXMBP_MULTICH_SHIFT));
	emac_write(EMAC_RXMBPENABLE, mbp_enable);
	emac_write(EMAC_RXMAXLEN, (EMAC_DEF_MAX_FRAME_SIZE &
				   EMAC_RX_MAX_LEN_MASK));
	emac_write(EMAC_RXBUFFEROFFSET, (EMAC_DEF_BUFFER_OFFSET &
					 EMAC_RX_BUFFER_OFFSET_MASK));
	emac_write(EMAC_RXFILTERLOWTHRESH, 0);
	emac_write(EMAC_RXUNICASTCLEAR, EMAC_RX_UNICAST_CLEAR_ALL);
	priv->rx_addr_type = (emac_read(EMAC_MACCONFIG) >> 8) & 0xFF;

	emac_write(EMAC_MACINTMASKSET, EMAC_MAC_HOST_ERR_INTMASK_VAL);

	emac_setmac(priv, EMAC_DEF_RX_CH, priv->mac_addr);

	
	val = emac_read(EMAC_MACCONTROL);
	val |= (EMAC_MACCONTROL_GMIIEN);
	emac_write(EMAC_MACCONTROL, val);

	
	napi_enable(&priv->napi);
	emac_int_enable(priv);
	return 0;

}

static int emac_poll(struct napi_struct *napi, int budget)
{
	unsigned int mask;
	struct emac_priv *priv = container_of(napi, struct emac_priv, napi);
	struct net_device *ndev = priv->ndev;
	struct device *emac_dev = &ndev->dev;
	u32 status = 0;
	u32 num_tx_pkts = 0, num_rx_pkts = 0;

	
	status = emac_read(EMAC_MACINVECTOR);

	mask = EMAC_DM644X_MAC_IN_VECTOR_TX_INT_VEC;

	if (priv->version == EMAC_VERSION_2)
		mask = EMAC_DM646X_MAC_IN_VECTOR_TX_INT_VEC;

	if (status & mask) {
		num_tx_pkts = cpdma_chan_process(priv->txchan,
					      EMAC_DEF_TX_MAX_SERVICE);
	} 

	mask = EMAC_DM644X_MAC_IN_VECTOR_RX_INT_VEC;

	if (priv->version == EMAC_VERSION_2)
		mask = EMAC_DM646X_MAC_IN_VECTOR_RX_INT_VEC;

	if (status & mask) {
		num_rx_pkts = cpdma_chan_process(priv->rxchan, budget);
	} 

	mask = EMAC_DM644X_MAC_IN_VECTOR_HOST_INT;
	if (priv->version == EMAC_VERSION_2)
		mask = EMAC_DM646X_MAC_IN_VECTOR_HOST_INT;

	if (unlikely(status & mask)) {
		u32 ch, cause;
		dev_err(emac_dev, "DaVinci EMAC: Fatal Hardware Error\n");
		netif_stop_queue(ndev);
		napi_disable(&priv->napi);

		status = emac_read(EMAC_MACSTATUS);
		cause = ((status & EMAC_MACSTATUS_TXERRCODE_MASK) >>
			 EMAC_MACSTATUS_TXERRCODE_SHIFT);
		if (cause) {
			ch = ((status & EMAC_MACSTATUS_TXERRCH_MASK) >>
			      EMAC_MACSTATUS_TXERRCH_SHIFT);
			if (net_ratelimit()) {
				dev_err(emac_dev, "TX Host error %s on ch=%d\n",
					&emac_txhost_errcodes[cause][0], ch);
			}
		}
		cause = ((status & EMAC_MACSTATUS_RXERRCODE_MASK) >>
			 EMAC_MACSTATUS_RXERRCODE_SHIFT);
		if (cause) {
			ch = ((status & EMAC_MACSTATUS_RXERRCH_MASK) >>
			      EMAC_MACSTATUS_RXERRCH_SHIFT);
			if (netif_msg_hw(priv) && net_ratelimit())
				dev_err(emac_dev, "RX Host error %s on ch=%d\n",
					&emac_rxhost_errcodes[cause][0], ch);
		}
	} else if (num_rx_pkts < budget) {
		napi_complete(napi);
		emac_int_enable(priv);
	}

	return num_rx_pkts;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
void emac_poll_controller(struct net_device *ndev)
{
	struct emac_priv *priv = netdev_priv(ndev);

	emac_int_disable(priv);
	emac_irq(ndev->irq, ndev);
	emac_int_enable(priv);
}
#endif

static void emac_adjust_link(struct net_device *ndev)
{
	struct emac_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev = priv->phydev;
	unsigned long flags;
	int new_state = 0;

	spin_lock_irqsave(&priv->lock, flags);

	if (phydev->link) {
		
		if (phydev->duplex != priv->duplex) {
			new_state = 1;
			priv->duplex = phydev->duplex;
		}
		if (phydev->speed != priv->speed) {
			new_state = 1;
			priv->speed = phydev->speed;
		}
		if (!priv->link) {
			new_state = 1;
			priv->link = 1;
		}

	} else if (priv->link) {
		new_state = 1;
		priv->link = 0;
		priv->speed = 0;
		priv->duplex = ~0;
	}
	if (new_state) {
		emac_update_phystatus(priv);
		phy_print_status(priv->phydev);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
}


static int emac_devioctl(struct net_device *ndev, struct ifreq *ifrq, int cmd)
{
	struct emac_priv *priv = netdev_priv(ndev);

	if (!(netif_running(ndev)))
		return -EINVAL;

	

	return phy_mii_ioctl(priv->phydev, ifrq, cmd);
}

static int match_first_device(struct device *dev, void *data)
{
	return !strncmp(dev_name(dev), "davinci_mdio", 12);
}

static int emac_dev_open(struct net_device *ndev)
{
	struct device *emac_dev = &ndev->dev;
	u32 cnt;
	struct resource *res;
	int q, m, ret;
	int i = 0;
	int k = 0;
	struct emac_priv *priv = netdev_priv(ndev);

	netif_carrier_off(ndev);
	for (cnt = 0; cnt < ETH_ALEN; cnt++)
		ndev->dev_addr[cnt] = priv->mac_addr[cnt];

	
	priv->rx_buf_size = EMAC_DEF_MAX_FRAME_SIZE + NET_IP_ALIGN;

	priv->mac_hash1 = 0;
	priv->mac_hash2 = 0;
	emac_write(EMAC_MACHASH1, 0);
	emac_write(EMAC_MACHASH2, 0);

	for (i = 0; i < EMAC_DEF_RX_NUM_DESC; i++) {
		struct sk_buff *skb = emac_rx_alloc(priv);

		if (!skb)
			break;

		ret = cpdma_chan_submit(priv->rxchan, skb, skb->data,
					skb_tailroom(skb), GFP_KERNEL);
		if (WARN_ON(ret < 0))
			break;
	}

	

	while ((res = platform_get_resource(priv->pdev, IORESOURCE_IRQ, k))) {
		for (i = res->start; i <= res->end; i++) {
			if (request_irq(i, emac_irq, IRQF_DISABLED,
					ndev->name, ndev))
				goto rollback;
		}
		k++;
	}

	
	emac_hw_enable(priv);

	
	if (priv->coal_intvl != 0) {
		struct ethtool_coalesce coal;

		coal.rx_coalesce_usecs = (priv->coal_intvl << 4);
		emac_set_coalesce(ndev, &coal);
	}

	cpdma_ctlr_start(priv->dma);

	priv->phydev = NULL;
	
	if (!priv->phy_id) {
		struct device *phy;

		phy = bus_find_device(&mdio_bus_type, NULL, NULL,
				      match_first_device);
		if (phy)
			priv->phy_id = dev_name(phy);
	}

	if (priv->phy_id && *priv->phy_id) {
		priv->phydev = phy_connect(ndev, priv->phy_id,
					   &emac_adjust_link, 0,
					   PHY_INTERFACE_MODE_MII);

		if (IS_ERR(priv->phydev)) {
			dev_err(emac_dev, "could not connect to phy %s\n",
				priv->phy_id);
			ret = PTR_ERR(priv->phydev);
			priv->phydev = NULL;
			return ret;
		}

		priv->link = 0;
		priv->speed = 0;
		priv->duplex = ~0;

		dev_info(emac_dev, "attached PHY driver [%s] "
			"(mii_bus:phy_addr=%s, id=%x)\n",
			priv->phydev->drv->name, dev_name(&priv->phydev->dev),
			priv->phydev->phy_id);
	} else {
		
		dev_notice(emac_dev, "no phy, defaulting to 100/full\n");
		priv->link = 1;
		priv->speed = SPEED_100;
		priv->duplex = DUPLEX_FULL;
		emac_update_phystatus(priv);
	}

	if (!netif_running(ndev)) 
		emac_dump_regs(priv);

	if (netif_msg_drv(priv))
		dev_notice(emac_dev, "DaVinci EMAC: Opened %s\n", ndev->name);

	if (priv->phydev)
		phy_start(priv->phydev);

	return 0;

rollback:

	dev_err(emac_dev, "DaVinci EMAC: request_irq() failed");

	for (q = k; k >= 0; k--) {
		for (m = i; m >= res->start; m--)
			free_irq(m, ndev);
		res = platform_get_resource(priv->pdev, IORESOURCE_IRQ, k-1);
		m = res->end;
	}
	return -EBUSY;
}

static int emac_dev_stop(struct net_device *ndev)
{
	struct resource *res;
	int i = 0;
	int irq_num;
	struct emac_priv *priv = netdev_priv(ndev);
	struct device *emac_dev = &ndev->dev;

	
	netif_stop_queue(ndev);
	napi_disable(&priv->napi);

	netif_carrier_off(ndev);
	emac_int_disable(priv);
	cpdma_ctlr_stop(priv->dma);
	emac_write(EMAC_SOFTRESET, 1);

	if (priv->phydev)
		phy_disconnect(priv->phydev);

	
	while ((res = platform_get_resource(priv->pdev, IORESOURCE_IRQ, i))) {
		for (irq_num = res->start; irq_num <= res->end; irq_num++)
			free_irq(irq_num, priv->ndev);
		i++;
	}

	if (netif_msg_drv(priv))
		dev_notice(emac_dev, "DaVinci EMAC: %s stopped\n", ndev->name);

	return 0;
}

static struct net_device_stats *emac_dev_getnetstats(struct net_device *ndev)
{
	struct emac_priv *priv = netdev_priv(ndev);
	u32 mac_control;
	u32 stats_clear_mask;

	

	mac_control = emac_read(EMAC_MACCONTROL);

	if (mac_control & EMAC_MACCONTROL_GMIIEN)
		stats_clear_mask = EMAC_STATS_CLR_MASK;
	else
		stats_clear_mask = 0;

	ndev->stats.multicast += emac_read(EMAC_RXMCASTFRAMES);
	emac_write(EMAC_RXMCASTFRAMES, stats_clear_mask);

	ndev->stats.collisions += (emac_read(EMAC_TXCOLLISION) +
					   emac_read(EMAC_TXSINGLECOLL) +
					   emac_read(EMAC_TXMULTICOLL));
	emac_write(EMAC_TXCOLLISION, stats_clear_mask);
	emac_write(EMAC_TXSINGLECOLL, stats_clear_mask);
	emac_write(EMAC_TXMULTICOLL, stats_clear_mask);

	ndev->stats.rx_length_errors += (emac_read(EMAC_RXOVERSIZED) +
						emac_read(EMAC_RXJABBER) +
						emac_read(EMAC_RXUNDERSIZED));
	emac_write(EMAC_RXOVERSIZED, stats_clear_mask);
	emac_write(EMAC_RXJABBER, stats_clear_mask);
	emac_write(EMAC_RXUNDERSIZED, stats_clear_mask);

	ndev->stats.rx_over_errors += (emac_read(EMAC_RXSOFOVERRUNS) +
					       emac_read(EMAC_RXMOFOVERRUNS));
	emac_write(EMAC_RXSOFOVERRUNS, stats_clear_mask);
	emac_write(EMAC_RXMOFOVERRUNS, stats_clear_mask);

	ndev->stats.rx_fifo_errors += emac_read(EMAC_RXDMAOVERRUNS);
	emac_write(EMAC_RXDMAOVERRUNS, stats_clear_mask);

	ndev->stats.tx_carrier_errors +=
		emac_read(EMAC_TXCARRIERSENSE);
	emac_write(EMAC_TXCARRIERSENSE, stats_clear_mask);

	ndev->stats.tx_fifo_errors += emac_read(EMAC_TXUNDERRUN);
	emac_write(EMAC_TXUNDERRUN, stats_clear_mask);

	return &ndev->stats;
}

static const struct net_device_ops emac_netdev_ops = {
	.ndo_open		= emac_dev_open,
	.ndo_stop		= emac_dev_stop,
	.ndo_start_xmit		= emac_dev_xmit,
	.ndo_set_rx_mode	= emac_dev_mcast_set,
	.ndo_set_mac_address	= emac_dev_setmac_addr,
	.ndo_do_ioctl		= emac_devioctl,
	.ndo_tx_timeout		= emac_dev_tx_timeout,
	.ndo_get_stats		= emac_dev_getnetstats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= emac_poll_controller,
#endif
};

static int __devinit davinci_emac_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	struct net_device *ndev;
	struct emac_priv *priv;
	unsigned long size, hw_ram_addr;
	struct emac_platform_data *pdata;
	struct device *emac_dev;
	struct cpdma_params dma_params;

	
	emac_clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(emac_clk)) {
		dev_err(&pdev->dev, "failed to get EMAC clock\n");
		return -EBUSY;
	}
	emac_bus_frequency = clk_get_rate(emac_clk);
	

	ndev = alloc_etherdev(sizeof(struct emac_priv));
	if (!ndev) {
		rc = -ENOMEM;
		goto free_clk;
	}

	platform_set_drvdata(pdev, ndev);
	priv = netdev_priv(ndev);
	priv->pdev = pdev;
	priv->ndev = ndev;
	priv->msg_enable = netif_msg_init(debug_level, DAVINCI_EMAC_DEBUG);

	spin_lock_init(&priv->lock);

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		rc = -ENODEV;
		goto probe_quit;
	}

	
	memcpy(priv->mac_addr, pdata->mac_addr, 6);
	priv->phy_id = pdata->phy_id;
	priv->rmii_en = pdata->rmii_en;
	priv->version = pdata->version;
	priv->int_enable = pdata->interrupt_enable;
	priv->int_disable = pdata->interrupt_disable;

	priv->coal_intvl = 0;
	priv->bus_freq_mhz = (u32)(emac_bus_frequency / 1000000);

	emac_dev = &ndev->dev;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,"error getting res\n");
		rc = -ENOENT;
		goto probe_quit;
	}

	priv->emac_base_phys = res->start + pdata->ctrl_reg_offset;
	size = resource_size(res);
	if (!request_mem_region(res->start, size, ndev->name)) {
		dev_err(&pdev->dev, "failed request_mem_region() for regs\n");
		rc = -ENXIO;
		goto probe_quit;
	}

	priv->remap_addr = ioremap(res->start, size);
	if (!priv->remap_addr) {
		dev_err(&pdev->dev, "unable to map IO\n");
		rc = -ENOMEM;
		release_mem_region(res->start, size);
		goto probe_quit;
	}
	priv->emac_base = priv->remap_addr + pdata->ctrl_reg_offset;
	ndev->base_addr = (unsigned long)priv->remap_addr;

	priv->ctrl_base = priv->remap_addr + pdata->ctrl_mod_reg_offset;

	hw_ram_addr = pdata->hw_ram_addr;
	if (!hw_ram_addr)
		hw_ram_addr = (u32 __force)res->start + pdata->ctrl_ram_offset;

	memset(&dma_params, 0, sizeof(dma_params));
	dma_params.dev			= emac_dev;
	dma_params.dmaregs		= priv->emac_base;
	dma_params.rxthresh		= priv->emac_base + 0x120;
	dma_params.rxfree		= priv->emac_base + 0x140;
	dma_params.txhdp		= priv->emac_base + 0x600;
	dma_params.rxhdp		= priv->emac_base + 0x620;
	dma_params.txcp			= priv->emac_base + 0x640;
	dma_params.rxcp			= priv->emac_base + 0x660;
	dma_params.num_chan		= EMAC_MAX_TXRX_CHANNELS;
	dma_params.min_packet_size	= EMAC_DEF_MIN_ETHPKTSIZE;
	dma_params.desc_hw_addr		= hw_ram_addr;
	dma_params.desc_mem_size	= pdata->ctrl_ram_size;
	dma_params.desc_align		= 16;

	dma_params.desc_mem_phys = pdata->no_bd_ram ? 0 :
			(u32 __force)res->start + pdata->ctrl_ram_offset;

	priv->dma = cpdma_ctlr_create(&dma_params);
	if (!priv->dma) {
		dev_err(&pdev->dev, "error initializing DMA\n");
		rc = -ENOMEM;
		goto no_dma;
	}

	priv->txchan = cpdma_chan_create(priv->dma, tx_chan_num(EMAC_DEF_TX_CH),
				       emac_tx_handler);
	priv->rxchan = cpdma_chan_create(priv->dma, rx_chan_num(EMAC_DEF_RX_CH),
				       emac_rx_handler);
	if (WARN_ON(!priv->txchan || !priv->rxchan)) {
		rc = -ENOMEM;
		goto no_irq_res;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "error getting irq res\n");
		rc = -ENOENT;
		goto no_irq_res;
	}
	ndev->irq = res->start;

	if (!is_valid_ether_addr(priv->mac_addr)) {
		
		eth_hw_addr_random(ndev);
		memcpy(priv->mac_addr, ndev->dev_addr, ndev->addr_len);
		dev_warn(&pdev->dev, "using random MAC addr: %pM\n",
							priv->mac_addr);
	}

	ndev->netdev_ops = &emac_netdev_ops;
	SET_ETHTOOL_OPS(ndev, &ethtool_ops);
	netif_napi_add(ndev, &priv->napi, emac_poll, EMAC_POLL_WEIGHT);

	clk_enable(emac_clk);

	
	SET_NETDEV_DEV(ndev, &pdev->dev);
	rc = register_netdev(ndev);
	if (rc) {
		dev_err(&pdev->dev, "error in register_netdev\n");
		rc = -ENODEV;
		goto netdev_reg_err;
	}


	if (netif_msg_probe(priv)) {
		dev_notice(emac_dev, "DaVinci EMAC Probe found device "\
			   "(regs: %p, irq: %d)\n",
			   (void *)priv->emac_base_phys, ndev->irq);
	}
	return 0;

netdev_reg_err:
	clk_disable(emac_clk);
no_irq_res:
	if (priv->txchan)
		cpdma_chan_destroy(priv->txchan);
	if (priv->rxchan)
		cpdma_chan_destroy(priv->rxchan);
	cpdma_ctlr_destroy(priv->dma);
no_dma:
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));
	iounmap(priv->remap_addr);

probe_quit:
	free_netdev(ndev);
free_clk:
	clk_put(emac_clk);
	return rc;
}

static int __devexit davinci_emac_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct emac_priv *priv = netdev_priv(ndev);

	dev_notice(&ndev->dev, "DaVinci EMAC: davinci_emac_remove()\n");

	platform_set_drvdata(pdev, NULL);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (priv->txchan)
		cpdma_chan_destroy(priv->txchan);
	if (priv->rxchan)
		cpdma_chan_destroy(priv->rxchan);
	cpdma_ctlr_destroy(priv->dma);

	release_mem_region(res->start, resource_size(res));

	unregister_netdev(ndev);
	iounmap(priv->remap_addr);
	free_netdev(ndev);

	clk_disable(emac_clk);
	clk_put(emac_clk);

	return 0;
}

static int davinci_emac_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = platform_get_drvdata(pdev);

	if (netif_running(ndev))
		emac_dev_stop(ndev);

	clk_disable(emac_clk);

	return 0;
}

static int davinci_emac_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *ndev = platform_get_drvdata(pdev);

	clk_enable(emac_clk);

	if (netif_running(ndev))
		emac_dev_open(ndev);

	return 0;
}

static const struct dev_pm_ops davinci_emac_pm_ops = {
	.suspend	= davinci_emac_suspend,
	.resume		= davinci_emac_resume,
};

static struct platform_driver davinci_emac_driver = {
	.driver = {
		.name	 = "davinci_emac",
		.owner	 = THIS_MODULE,
		.pm	 = &davinci_emac_pm_ops,
	},
	.probe = davinci_emac_probe,
	.remove = __devexit_p(davinci_emac_remove),
};

static int __init davinci_emac_init(void)
{
	return platform_driver_register(&davinci_emac_driver);
}
late_initcall(davinci_emac_init);

static void __exit davinci_emac_exit(void)
{
	platform_driver_unregister(&davinci_emac_driver);
}
module_exit(davinci_emac_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DaVinci EMAC Maintainer: Anant Gole <anantgole@ti.com>");
MODULE_AUTHOR("DaVinci EMAC Maintainer: Chaithrika U S <chaithrika@ti.com>");
MODULE_DESCRIPTION("DaVinci EMAC Ethernet driver");
