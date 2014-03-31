/*
	Written/copyright 1999-2001 by Donald Becker.
	Portions copyright (c) 2001,2002 Sun Microsystems (thockin@sun.com)
	Portions copyright 2001,2002 Manfred Spraul (manfred@colorfullife.com)
	Portions copyright 2004 Harald Welte <laforge@gnumonks.org>

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.  License for under other terms may be
	available.  Contact the original author for details.

	The original author may be reached as becker@scyld.com, or at
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Support information and updates available at
	http://www.scyld.com/network/netsemi.html
	[link no longer provides useful info -jgarzik]


	TODO:
	* big endian support with CFG:BEM instead of cpu_to_le32
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/rtnetlink.h>
#include <linux/mii.h>
#include <linux/crc32.h>
#include <linux/bitops.h>
#include <linux/prefetch.h>
#include <asm/processor.h>	
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#define DRV_NAME	"natsemi"
#define DRV_VERSION	"2.1"
#define DRV_RELDATE	"Sept 11, 2006"

#define RX_OFFSET	2



#define NATSEMI_DEF_MSG		(NETIF_MSG_DRV		| \
				 NETIF_MSG_LINK		| \
				 NETIF_MSG_WOL		| \
				 NETIF_MSG_RX_ERR	| \
				 NETIF_MSG_TX_ERR)
static int debug = -1;

static int mtu;

static const int multicast_filter_limit = 100;

static int rx_copybreak;

static int dspcfg_workaround = 1;

#define MAX_UNITS 8		
static int options[MAX_UNITS];
static int full_duplex[MAX_UNITS];


#define TX_RING_SIZE	16
#define TX_QUEUE_LEN	10 
#define RX_RING_SIZE	32

#define TX_TIMEOUT  (2*HZ)

#define NATSEMI_HW_TIMEOUT	400
#define NATSEMI_TIMER_FREQ	5*HZ
#define NATSEMI_PG0_NREGS	64
#define NATSEMI_RFDR_NREGS	8
#define NATSEMI_PG1_NREGS	4
#define NATSEMI_NREGS		(NATSEMI_PG0_NREGS + NATSEMI_RFDR_NREGS + \
				 NATSEMI_PG1_NREGS)
#define NATSEMI_REGS_VER	1 
#define NATSEMI_REGS_SIZE	(NATSEMI_NREGS * sizeof(u32))

#define NATSEMI_HEADERS		22	
#define NATSEMI_PADDING		16	
#define NATSEMI_LONGPKT		1518	
#define NATSEMI_RX_LIMIT	2046	

static const char version[] __devinitconst =
  KERN_INFO DRV_NAME " dp8381x driver, version "
      DRV_VERSION ", " DRV_RELDATE "\n"
  "  originally by Donald Becker <becker@scyld.com>\n"
  "  2.4.x kernel port by Jeff Garzik, Tjeerd Mulder\n";

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("National Semiconductor DP8381x series PCI Ethernet driver");
MODULE_LICENSE("GPL");

module_param(mtu, int, 0);
module_param(debug, int, 0);
module_param(rx_copybreak, int, 0);
module_param(dspcfg_workaround, int, 0);
module_param_array(options, int, NULL, 0);
module_param_array(full_duplex, int, NULL, 0);
MODULE_PARM_DESC(mtu, "DP8381x MTU (all boards)");
MODULE_PARM_DESC(debug, "DP8381x default debug level");
MODULE_PARM_DESC(rx_copybreak,
	"DP8381x copy breakpoint for copy-only-tiny-frames");
MODULE_PARM_DESC(dspcfg_workaround, "DP8381x: control DspCfg workaround");
MODULE_PARM_DESC(options,
	"DP8381x: Bits 0-3: media type, bit 17: full duplex");
MODULE_PARM_DESC(full_duplex, "DP8381x full duplex setting(s) (1)");




#define PHYID_AM79C874	0x0022561b

enum {
	MII_MCTRL	= 0x15,		
	MII_FX_SEL	= 0x0001,	
	MII_EN_SCRM	= 0x0004,	
};

enum {
	NATSEMI_FLAG_IGNORE_PHY		= 0x1,
};

static struct {
	const char *name;
	unsigned long flags;
	unsigned int eeprom_size;
} natsemi_pci_info[] __devinitdata = {
	{ "Aculab E1/T1 PMXc cPCI carrier card", NATSEMI_FLAG_IGNORE_PHY, 128 },
	{ "NatSemi DP8381[56]", 0, 24 },
};

static DEFINE_PCI_DEVICE_TABLE(natsemi_pci_tbl) = {
	{ PCI_VENDOR_ID_NS, 0x0020, 0x12d9,     0x000c,     0, 0, 0 },
	{ PCI_VENDOR_ID_NS, 0x0020, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1 },
	{ }	
};
MODULE_DEVICE_TABLE(pci, natsemi_pci_tbl);

enum register_offsets {
	ChipCmd			= 0x00,
	ChipConfig		= 0x04,
	EECtrl			= 0x08,
	PCIBusCfg		= 0x0C,
	IntrStatus		= 0x10,
	IntrMask		= 0x14,
	IntrEnable		= 0x18,
	IntrHoldoff		= 0x1C, 
	TxRingPtr		= 0x20,
	TxConfig		= 0x24,
	RxRingPtr		= 0x30,
	RxConfig		= 0x34,
	ClkRun			= 0x3C,
	WOLCmd			= 0x40,
	PauseCmd		= 0x44,
	RxFilterAddr		= 0x48,
	RxFilterData		= 0x4C,
	BootRomAddr		= 0x50,
	BootRomData		= 0x54,
	SiliconRev		= 0x58,
	StatsCtrl		= 0x5C,
	StatsData		= 0x60,
	RxPktErrs		= 0x60,
	RxMissed		= 0x68,
	RxCRCErrs		= 0x64,
	BasicControl		= 0x80,
	BasicStatus		= 0x84,
	AnegAdv			= 0x90,
	AnegPeer		= 0x94,
	PhyStatus		= 0xC0,
	MIntrCtrl		= 0xC4,
	MIntrStatus		= 0xC8,
	PhyCtrl			= 0xE4,

	PGSEL			= 0xCC,
	PMDCSR			= 0xE4,
	TSTDAT			= 0xFC,
	DSPCFG			= 0xF4,
	SDCFG			= 0xF8
};
#define PMDCSR_VAL	0x189c	
#define TSTDAT_VAL	0x0
#define DSPCFG_VAL	0x5040
#define SDCFG_VAL	0x008c	
#define DSPCFG_LOCK	0x20	
#define DSPCFG_COEF	0x1000	
#define TSTDAT_FIXED	0xe8	

enum pci_register_offsets {
	PCIPM			= 0x44,
};

enum ChipCmd_bits {
	ChipReset		= 0x100,
	RxReset			= 0x20,
	TxReset			= 0x10,
	RxOff			= 0x08,
	RxOn			= 0x04,
	TxOff			= 0x02,
	TxOn			= 0x01,
};

enum ChipConfig_bits {
	CfgPhyDis		= 0x200,
	CfgPhyRst		= 0x400,
	CfgExtPhy		= 0x1000,
	CfgAnegEnable		= 0x2000,
	CfgAneg100		= 0x4000,
	CfgAnegFull		= 0x8000,
	CfgAnegDone		= 0x8000000,
	CfgFullDuplex		= 0x20000000,
	CfgSpeed100		= 0x40000000,
	CfgLink			= 0x80000000,
};

enum EECtrl_bits {
	EE_ShiftClk		= 0x04,
	EE_DataIn		= 0x01,
	EE_ChipSelect		= 0x08,
	EE_DataOut		= 0x02,
	MII_Data 		= 0x10,
	MII_Write		= 0x20,
	MII_ShiftClk		= 0x40,
};

enum PCIBusCfg_bits {
	EepromReload		= 0x4,
};

enum IntrStatus_bits {
	IntrRxDone		= 0x0001,
	IntrRxIntr		= 0x0002,
	IntrRxErr		= 0x0004,
	IntrRxEarly		= 0x0008,
	IntrRxIdle		= 0x0010,
	IntrRxOverrun		= 0x0020,
	IntrTxDone		= 0x0040,
	IntrTxIntr		= 0x0080,
	IntrTxErr		= 0x0100,
	IntrTxIdle		= 0x0200,
	IntrTxUnderrun		= 0x0400,
	StatsMax		= 0x0800,
	SWInt			= 0x1000,
	WOLPkt			= 0x2000,
	LinkChange		= 0x4000,
	IntrHighBits		= 0x8000,
	RxStatusFIFOOver	= 0x10000,
	IntrPCIErr		= 0xf00000,
	RxResetDone		= 0x1000000,
	TxResetDone		= 0x2000000,
	IntrAbnormalSummary	= 0xCD20,
};

#define DEFAULT_INTR 0x00f1cd65

enum TxConfig_bits {
	TxDrthMask		= 0x3f,
	TxFlthMask		= 0x3f00,
	TxMxdmaMask		= 0x700000,
	TxMxdma_512		= 0x0,
	TxMxdma_4		= 0x100000,
	TxMxdma_8		= 0x200000,
	TxMxdma_16		= 0x300000,
	TxMxdma_32		= 0x400000,
	TxMxdma_64		= 0x500000,
	TxMxdma_128		= 0x600000,
	TxMxdma_256		= 0x700000,
	TxCollRetry		= 0x800000,
	TxAutoPad		= 0x10000000,
	TxMacLoop		= 0x20000000,
	TxHeartIgn		= 0x40000000,
	TxCarrierIgn		= 0x80000000
};

#define TX_FLTH_VAL		((512/32) << 8)
#define TX_DRTH_VAL_START	(64/32)
#define TX_DRTH_VAL_INC		2
#define TX_DRTH_VAL_LIMIT	(1472/32)

enum RxConfig_bits {
	RxDrthMask		= 0x3e,
	RxMxdmaMask		= 0x700000,
	RxMxdma_512		= 0x0,
	RxMxdma_4		= 0x100000,
	RxMxdma_8		= 0x200000,
	RxMxdma_16		= 0x300000,
	RxMxdma_32		= 0x400000,
	RxMxdma_64		= 0x500000,
	RxMxdma_128		= 0x600000,
	RxMxdma_256		= 0x700000,
	RxAcceptLong		= 0x8000000,
	RxAcceptTx		= 0x10000000,
	RxAcceptRunt		= 0x40000000,
	RxAcceptErr		= 0x80000000
};
#define RX_DRTH_VAL		(128/8)

enum ClkRun_bits {
	PMEEnable		= 0x100,
	PMEStatus		= 0x8000,
};

enum WolCmd_bits {
	WakePhy			= 0x1,
	WakeUnicast		= 0x2,
	WakeMulticast		= 0x4,
	WakeBroadcast		= 0x8,
	WakeArp			= 0x10,
	WakePMatch0		= 0x20,
	WakePMatch1		= 0x40,
	WakePMatch2		= 0x80,
	WakePMatch3		= 0x100,
	WakeMagic		= 0x200,
	WakeMagicSecure		= 0x400,
	SecureHack		= 0x100000,
	WokePhy			= 0x400000,
	WokeUnicast		= 0x800000,
	WokeMulticast		= 0x1000000,
	WokeBroadcast		= 0x2000000,
	WokeArp			= 0x4000000,
	WokePMatch0		= 0x8000000,
	WokePMatch1		= 0x10000000,
	WokePMatch2		= 0x20000000,
	WokePMatch3		= 0x40000000,
	WokeMagic		= 0x80000000,
	WakeOptsSummary		= 0x7ff
};

enum RxFilterAddr_bits {
	RFCRAddressMask		= 0x3ff,
	AcceptMulticast		= 0x00200000,
	AcceptMyPhys		= 0x08000000,
	AcceptAllPhys		= 0x10000000,
	AcceptAllMulticast	= 0x20000000,
	AcceptBroadcast		= 0x40000000,
	RxFilterEnable		= 0x80000000
};

enum StatsCtrl_bits {
	StatsWarn		= 0x1,
	StatsFreeze		= 0x2,
	StatsClear		= 0x4,
	StatsStrobe		= 0x8,
};

enum MIntrCtrl_bits {
	MICRIntEn		= 0x2,
};

enum PhyCtrl_bits {
	PhyAddrMask		= 0x1f,
};

#define PHY_ADDR_NONE		32
#define PHY_ADDR_INTERNAL	1

#define SRR_DP83815_C	0x0302
#define SRR_DP83815_D	0x0403
#define SRR_DP83816_A4	0x0504
#define SRR_DP83816_A5	0x0505

struct netdev_desc {
	__le32 next_desc;
	__le32 cmd_status;
	__le32 addr;
	__le32 software_use;
};

enum desc_status_bits {
	DescOwn=0x80000000, DescMore=0x40000000, DescIntr=0x20000000,
	DescNoCRC=0x10000000, DescPktOK=0x08000000,
	DescSizeMask=0xfff,

	DescTxAbort=0x04000000, DescTxFIFO=0x02000000,
	DescTxCarrier=0x01000000, DescTxDefer=0x00800000,
	DescTxExcDefer=0x00400000, DescTxOOWCol=0x00200000,
	DescTxExcColl=0x00100000, DescTxCollCount=0x000f0000,

	DescRxAbort=0x04000000, DescRxOver=0x02000000,
	DescRxDest=0x01800000, DescRxLong=0x00400000,
	DescRxRunt=0x00200000, DescRxInvalid=0x00100000,
	DescRxCRC=0x00080000, DescRxAlign=0x00040000,
	DescRxLoop=0x00020000, DesRxColl=0x00010000,
};

struct netdev_private {
	
	dma_addr_t ring_dma;
	struct netdev_desc *rx_ring;
	struct netdev_desc *tx_ring;
	
	struct sk_buff *rx_skbuff[RX_RING_SIZE];
	dma_addr_t rx_dma[RX_RING_SIZE];
	
	struct sk_buff *tx_skbuff[TX_RING_SIZE];
	dma_addr_t tx_dma[TX_RING_SIZE];
	struct net_device *dev;
	struct napi_struct napi;
	
	struct timer_list timer;
	
	struct pci_dev *pci_dev;
	struct netdev_desc *rx_head_desc;
	
	unsigned int cur_rx, dirty_rx;
	unsigned int cur_tx, dirty_tx;
	
	unsigned int rx_buf_sz;
	int oom;
	
	u32 intr_status;
	
	int hands_off;
	
	int ignore_phy;
	
	int mii;
	int phy_addr_external;
	unsigned int full_duplex;
	
	u32 cur_rx_mode;
	u32 rx_filter[16];
	
	u32 tx_config, rx_config;
	
	u32 SavedClkRun;
	
	u32 srr;
	
	u16 dspcfg;
	int dspcfg_workaround;
	
	u16	speed;		
	u8	duplex;		
	u8	autoneg;	
	
	u16 advertising;
	unsigned int iosize;
	spinlock_t lock;
	u32 msg_enable;
	
	int eeprom_size;
};

static void move_int_phy(struct net_device *dev, int addr);
static int eeprom_read(void __iomem *ioaddr, int location);
static int mdio_read(struct net_device *dev, int reg);
static void mdio_write(struct net_device *dev, int reg, u16 data);
static void init_phy_fixup(struct net_device *dev);
static int miiport_read(struct net_device *dev, int phy_id, int reg);
static void miiport_write(struct net_device *dev, int phy_id, int reg, u16 data);
static int find_mii(struct net_device *dev);
static void natsemi_reset(struct net_device *dev);
static void natsemi_reload_eeprom(struct net_device *dev);
static void natsemi_stop_rxtx(struct net_device *dev);
static int netdev_open(struct net_device *dev);
static void do_cable_magic(struct net_device *dev);
static void undo_cable_magic(struct net_device *dev);
static void check_link(struct net_device *dev);
static void netdev_timer(unsigned long data);
static void dump_ring(struct net_device *dev);
static void ns_tx_timeout(struct net_device *dev);
static int alloc_ring(struct net_device *dev);
static void refill_rx(struct net_device *dev);
static void init_ring(struct net_device *dev);
static void drain_tx(struct net_device *dev);
static void drain_ring(struct net_device *dev);
static void free_ring(struct net_device *dev);
static void reinit_ring(struct net_device *dev);
static void init_registers(struct net_device *dev);
static netdev_tx_t start_tx(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t intr_handler(int irq, void *dev_instance);
static void netdev_error(struct net_device *dev, int intr_status);
static int natsemi_poll(struct napi_struct *napi, int budget);
static void netdev_rx(struct net_device *dev, int *work_done, int work_to_do);
static void netdev_tx_done(struct net_device *dev);
static int natsemi_change_mtu(struct net_device *dev, int new_mtu);
#ifdef CONFIG_NET_POLL_CONTROLLER
static void natsemi_poll_controller(struct net_device *dev);
#endif
static void __set_rx_mode(struct net_device *dev);
static void set_rx_mode(struct net_device *dev);
static void __get_stats(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int netdev_set_wol(struct net_device *dev, u32 newval);
static int netdev_get_wol(struct net_device *dev, u32 *supported, u32 *cur);
static int netdev_set_sopass(struct net_device *dev, u8 *newval);
static int netdev_get_sopass(struct net_device *dev, u8 *data);
static int netdev_get_ecmd(struct net_device *dev, struct ethtool_cmd *ecmd);
static int netdev_set_ecmd(struct net_device *dev, struct ethtool_cmd *ecmd);
static void enable_wol_mode(struct net_device *dev, int enable_intr);
static int netdev_close(struct net_device *dev);
static int netdev_get_regs(struct net_device *dev, u8 *buf);
static int netdev_get_eeprom(struct net_device *dev, u8 *buf);
static const struct ethtool_ops ethtool_ops;

#define NATSEMI_ATTR(_name) \
static ssize_t natsemi_show_##_name(struct device *dev, \
         struct device_attribute *attr, char *buf); \
	 static ssize_t natsemi_set_##_name(struct device *dev, \
		struct device_attribute *attr, \
	        const char *buf, size_t count); \
	 static DEVICE_ATTR(_name, 0644, natsemi_show_##_name, natsemi_set_##_name)

#define NATSEMI_CREATE_FILE(_dev, _name) \
         device_create_file(&_dev->dev, &dev_attr_##_name)
#define NATSEMI_REMOVE_FILE(_dev, _name) \
         device_remove_file(&_dev->dev, &dev_attr_##_name)

NATSEMI_ATTR(dspcfg_workaround);

static ssize_t natsemi_show_dspcfg_workaround(struct device *dev,
				  	      struct device_attribute *attr,
					      char *buf)
{
	struct netdev_private *np = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "%s\n", np->dspcfg_workaround ? "on" : "off");
}

static ssize_t natsemi_set_dspcfg_workaround(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct netdev_private *np = netdev_priv(to_net_dev(dev));
	int new_setting;
	unsigned long flags;

        
        if (!strncmp("on", buf, count - 1) || !strncmp("1", buf, count - 1))
                new_setting = 1;
        else if (!strncmp("off", buf, count - 1) ||
                 !strncmp("0", buf, count - 1))
		new_setting = 0;
	else
                 return count;

	spin_lock_irqsave(&np->lock, flags);

	np->dspcfg_workaround = new_setting;

	spin_unlock_irqrestore(&np->lock, flags);

	return count;
}

static inline void __iomem *ns_ioaddr(struct net_device *dev)
{
	return (void __iomem *) dev->base_addr;
}

static inline void natsemi_irq_enable(struct net_device *dev)
{
	writel(1, ns_ioaddr(dev) + IntrEnable);
	readl(ns_ioaddr(dev) + IntrEnable);
}

static inline void natsemi_irq_disable(struct net_device *dev)
{
	writel(0, ns_ioaddr(dev) + IntrEnable);
	readl(ns_ioaddr(dev) + IntrEnable);
}

static void move_int_phy(struct net_device *dev, int addr)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);
	int target = 31;


	if (target == addr)
		target--;
	if (target == np->phy_addr_external)
		target--;
	writew(target, ioaddr + PhyCtrl);
	readw(ioaddr + PhyCtrl);
	udelay(1);
}

static void __devinit natsemi_init_media (struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	u32 tmp;

	if (np->ignore_phy)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);

	
	tmp            = mdio_read(dev, MII_BMCR);
	np->speed      = (tmp & BMCR_SPEED100)? SPEED_100     : SPEED_10;
	np->duplex     = (tmp & BMCR_FULLDPLX)? DUPLEX_FULL   : DUPLEX_HALF;
	np->autoneg    = (tmp & BMCR_ANENABLE)? AUTONEG_ENABLE: AUTONEG_DISABLE;
	np->advertising= mdio_read(dev, MII_ADVERTISE);

	if ((np->advertising & ADVERTISE_ALL) != ADVERTISE_ALL &&
	    netif_msg_probe(np)) {
		printk(KERN_INFO "natsemi %s: Transceiver default autonegotiation %s "
			"10%s %s duplex.\n",
			pci_name(np->pci_dev),
			(mdio_read(dev, MII_BMCR) & BMCR_ANENABLE)?
			  "enabled, advertise" : "disabled, force",
			(np->advertising &
			  (ADVERTISE_100FULL|ADVERTISE_100HALF))?
			    "0" : "",
			(np->advertising &
			  (ADVERTISE_100FULL|ADVERTISE_10FULL))?
			    "full" : "half");
	}
	if (netif_msg_probe(np))
		printk(KERN_INFO
			"natsemi %s: Transceiver status %#04x advertising %#04x.\n",
			pci_name(np->pci_dev), mdio_read(dev, MII_BMSR),
			np->advertising);

}

static const struct net_device_ops natsemi_netdev_ops = {
	.ndo_open		= netdev_open,
	.ndo_stop		= netdev_close,
	.ndo_start_xmit		= start_tx,
	.ndo_get_stats		= get_stats,
	.ndo_set_rx_mode	= set_rx_mode,
	.ndo_change_mtu		= natsemi_change_mtu,
	.ndo_do_ioctl		= netdev_ioctl,
	.ndo_tx_timeout 	= ns_tx_timeout,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= natsemi_poll_controller,
#endif
};

static int __devinit natsemi_probe1 (struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct net_device *dev;
	struct netdev_private *np;
	int i, option, irq, chip_idx = ent->driver_data;
	static int find_cnt = -1;
	resource_size_t iostart;
	unsigned long iosize;
	void __iomem *ioaddr;
	const int pcibar = 1; 
	int prev_eedata;
	u32 tmp;

#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	i = pci_enable_device(pdev);
	if (i) return i;

	pci_read_config_dword(pdev, PCIPM, &tmp);
	if (tmp & PCI_PM_CTRL_STATE_MASK) {
		
		u32 newtmp = tmp & ~PCI_PM_CTRL_STATE_MASK;
		pci_write_config_dword(pdev, PCIPM, newtmp);
	}

	find_cnt++;
	iostart = pci_resource_start(pdev, pcibar);
	iosize = pci_resource_len(pdev, pcibar);
	irq = pdev->irq;

	pci_set_master(pdev);

	dev = alloc_etherdev(sizeof (struct netdev_private));
	if (!dev)
		return -ENOMEM;
	SET_NETDEV_DEV(dev, &pdev->dev);

	i = pci_request_regions(pdev, DRV_NAME);
	if (i)
		goto err_pci_request_regions;

	ioaddr = ioremap(iostart, iosize);
	if (!ioaddr) {
		i = -ENOMEM;
		goto err_ioremap;
	}

	
	prev_eedata = eeprom_read(ioaddr, 6);
	for (i = 0; i < 3; i++) {
		int eedata = eeprom_read(ioaddr, i + 7);
		dev->dev_addr[i*2] = (eedata << 1) + (prev_eedata >> 15);
		dev->dev_addr[i*2+1] = eedata >> 7;
		prev_eedata = eedata;
	}

	
	memcpy(dev->perm_addr, dev->dev_addr, ETH_ALEN);

	dev->base_addr = (unsigned long __force) ioaddr;
	dev->irq = irq;

	np = netdev_priv(dev);
	netif_napi_add(dev, &np->napi, natsemi_poll, 64);
	np->dev = dev;

	np->pci_dev = pdev;
	pci_set_drvdata(pdev, dev);
	np->iosize = iosize;
	spin_lock_init(&np->lock);
	np->msg_enable = (debug >= 0) ? (1<<debug)-1 : NATSEMI_DEF_MSG;
	np->hands_off = 0;
	np->intr_status = 0;
	np->eeprom_size = natsemi_pci_info[chip_idx].eeprom_size;
	if (natsemi_pci_info[chip_idx].flags & NATSEMI_FLAG_IGNORE_PHY)
		np->ignore_phy = 1;
	else
		np->ignore_phy = 0;
	np->dspcfg_workaround = dspcfg_workaround;

	if (np->ignore_phy || readl(ioaddr + ChipConfig) & CfgExtPhy)
		dev->if_port = PORT_MII;
	else
		dev->if_port = PORT_TP;
	
	natsemi_reload_eeprom(dev);
	natsemi_reset(dev);

	if (dev->if_port != PORT_TP) {
		np->phy_addr_external = find_mii(dev);
		if (!np->ignore_phy && np->phy_addr_external == PHY_ADDR_NONE) {
			dev->if_port = PORT_TP;
			np->phy_addr_external = PHY_ADDR_INTERNAL;
		}
	} else {
		np->phy_addr_external = PHY_ADDR_INTERNAL;
	}

	option = find_cnt < MAX_UNITS ? options[find_cnt] : 0;
	if (dev->mem_start)
		option = dev->mem_start;

	
	if (option) {
		if (option & 0x200)
			np->full_duplex = 1;
		if (option & 15)
			printk(KERN_INFO
				"natsemi %s: ignoring user supplied media type %d",
				pci_name(np->pci_dev), option & 15);
	}
	if (find_cnt < MAX_UNITS  &&  full_duplex[find_cnt])
		np->full_duplex = 1;

	dev->netdev_ops = &natsemi_netdev_ops;
	dev->watchdog_timeo = TX_TIMEOUT;

	SET_ETHTOOL_OPS(dev, &ethtool_ops);

	if (mtu)
		dev->mtu = mtu;

	natsemi_init_media(dev);

	
	np->srr = readl(ioaddr + SiliconRev);
	if (netif_msg_hw(np))
		printk(KERN_INFO "natsemi %s: silicon revision %#04x.\n",
				pci_name(np->pci_dev), np->srr);

	i = register_netdev(dev);
	if (i)
		goto err_register_netdev;

	if (NATSEMI_CREATE_FILE(pdev, dspcfg_workaround))
		goto err_create_file;

	if (netif_msg_drv(np)) {
		printk(KERN_INFO "natsemi %s: %s at %#08llx "
		       "(%s), %pM, IRQ %d",
		       dev->name, natsemi_pci_info[chip_idx].name,
		       (unsigned long long)iostart, pci_name(np->pci_dev),
		       dev->dev_addr, irq);
		if (dev->if_port == PORT_TP)
			printk(", port TP.\n");
		else if (np->ignore_phy)
			printk(", port MII, ignoring PHY\n");
		else
			printk(", port MII, phy ad %d.\n", np->phy_addr_external);
	}
	return 0;

 err_create_file:
 	unregister_netdev(dev);

 err_register_netdev:
	iounmap(ioaddr);

 err_ioremap:
	pci_release_regions(pdev);
	pci_set_drvdata(pdev, NULL);

 err_pci_request_regions:
	free_netdev(dev);
	return i;
}



#define eeprom_delay(ee_addr)	readl(ee_addr)

#define EE_Write0 (EE_ChipSelect)
#define EE_Write1 (EE_ChipSelect | EE_DataIn)

enum EEPROM_Cmds {
	EE_WriteCmd=(5 << 6), EE_ReadCmd=(6 << 6), EE_EraseCmd=(7 << 6),
};

static int eeprom_read(void __iomem *addr, int location)
{
	int i;
	int retval = 0;
	void __iomem *ee_addr = addr + EECtrl;
	int read_cmd = location | EE_ReadCmd;

	writel(EE_Write0, ee_addr);

	
	for (i = 10; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_Write1 : EE_Write0;
		writel(dataval, ee_addr);
		eeprom_delay(ee_addr);
		writel(dataval | EE_ShiftClk, ee_addr);
		eeprom_delay(ee_addr);
	}
	writel(EE_ChipSelect, ee_addr);
	eeprom_delay(ee_addr);

	for (i = 0; i < 16; i++) {
		writel(EE_ChipSelect | EE_ShiftClk, ee_addr);
		eeprom_delay(ee_addr);
		retval |= (readl(ee_addr) & EE_DataOut) ? 1 << i : 0;
		writel(EE_ChipSelect, ee_addr);
		eeprom_delay(ee_addr);
	}

	
	writel(EE_Write0, ee_addr);
	writel(0, ee_addr);
	return retval;
}


#define mii_delay(ioaddr)  readl(ioaddr + EECtrl)

static int mii_getbit (struct net_device *dev)
{
	int data;
	void __iomem *ioaddr = ns_ioaddr(dev);

	writel(MII_ShiftClk, ioaddr + EECtrl);
	data = readl(ioaddr + EECtrl);
	writel(0, ioaddr + EECtrl);
	mii_delay(ioaddr);
	return (data & MII_Data)? 1 : 0;
}

static void mii_send_bits (struct net_device *dev, u32 data, int len)
{
	u32 i;
	void __iomem *ioaddr = ns_ioaddr(dev);

	for (i = (1 << (len-1)); i; i >>= 1)
	{
		u32 mdio_val = MII_Write | ((data & i)? MII_Data : 0);
		writel(mdio_val, ioaddr + EECtrl);
		mii_delay(ioaddr);
		writel(mdio_val | MII_ShiftClk, ioaddr + EECtrl);
		mii_delay(ioaddr);
	}
	writel(0, ioaddr + EECtrl);
	mii_delay(ioaddr);
}

static int miiport_read(struct net_device *dev, int phy_id, int reg)
{
	u32 cmd;
	int i;
	u32 retval = 0;

	
	mii_send_bits (dev, 0xffffffff, 32);
	
	
	cmd = (0x06 << 10) | (phy_id << 5) | reg;
	mii_send_bits (dev, cmd, 14);
	
	if (mii_getbit (dev))
		return 0;
	
	for (i = 0; i < 16; i++) {
		retval <<= 1;
		retval |= mii_getbit (dev);
	}
	
	mii_getbit (dev);
	return retval;
}

static void miiport_write(struct net_device *dev, int phy_id, int reg, u16 data)
{
	u32 cmd;

	
	mii_send_bits (dev, 0xffffffff, 32);
	
	
	cmd = (0x5002 << 16) | (phy_id << 23) | (reg << 18) | data;
	mii_send_bits (dev, cmd, 32);
	
	mii_getbit (dev);
}

static int mdio_read(struct net_device *dev, int reg)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);

	if (dev->if_port == PORT_TP)
		return readw(ioaddr+BasicControl+(reg<<2));
	else
		return miiport_read(dev, np->phy_addr_external, reg);
}

static void mdio_write(struct net_device *dev, int reg, u16 data)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);

	
	if (dev->if_port == PORT_TP)
		writew(data, ioaddr+BasicControl+(reg<<2));
	else
		miiport_write(dev, np->phy_addr_external, reg, data);
}

static void init_phy_fixup(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);
	int i;
	u32 cfg;
	u16 tmp;

	
	tmp = mdio_read(dev, MII_BMCR);
	if (np->autoneg == AUTONEG_ENABLE) {
		
		if ((tmp & BMCR_ANENABLE) == 0 ||
		    np->advertising != mdio_read(dev, MII_ADVERTISE))
		{
			
			tmp |= (BMCR_ANENABLE | BMCR_ANRESTART);
			mdio_write(dev, MII_ADVERTISE, np->advertising);
		}
	} else {
		
		tmp &= ~(BMCR_ANENABLE | BMCR_SPEED100 | BMCR_FULLDPLX);
		if (np->speed == SPEED_100)
			tmp |= BMCR_SPEED100;
		if (np->duplex == DUPLEX_FULL)
			tmp |= BMCR_FULLDPLX;
	}
	mdio_write(dev, MII_BMCR, tmp);
	readl(ioaddr + ChipConfig);
	udelay(1);

	
	np->mii = (mdio_read(dev, MII_PHYSID1) << 16)
				+ mdio_read(dev, MII_PHYSID2);

	
	switch (np->mii) {
	case PHYID_AM79C874:
		
		tmp = mdio_read(dev, MII_MCTRL);
		tmp &= ~(MII_FX_SEL | MII_EN_SCRM);
		if (dev->if_port == PORT_FIBRE)
			tmp |= MII_FX_SEL;
		else
			tmp |= MII_EN_SCRM;
		mdio_write(dev, MII_MCTRL, tmp);
		break;
	default:
		break;
	}
	cfg = readl(ioaddr + ChipConfig);
	if (cfg & CfgExtPhy)
		return;

	for (i=0;i<NATSEMI_HW_TIMEOUT;i++) {

		int dspcfg;
		writew(1, ioaddr + PGSEL);
		writew(PMDCSR_VAL, ioaddr + PMDCSR);
		writew(TSTDAT_VAL, ioaddr + TSTDAT);
		np->dspcfg = (np->srr <= SRR_DP83815_C)?
			DSPCFG_VAL : (DSPCFG_COEF | readw(ioaddr + DSPCFG));
		writew(np->dspcfg, ioaddr + DSPCFG);
		writew(SDCFG_VAL, ioaddr + SDCFG);
		writew(0, ioaddr + PGSEL);
		readl(ioaddr + ChipConfig);
		udelay(10);

		writew(1, ioaddr + PGSEL);
		dspcfg = readw(ioaddr + DSPCFG);
		writew(0, ioaddr + PGSEL);
		if (np->dspcfg == dspcfg)
			break;
	}

	if (netif_msg_link(np)) {
		if (i==NATSEMI_HW_TIMEOUT) {
			printk(KERN_INFO
				"%s: DSPCFG mismatch after retrying for %d usec.\n",
				dev->name, i*10);
		} else {
			printk(KERN_INFO
				"%s: DSPCFG accepted after %d usec.\n",
				dev->name, i*10);
		}
	}
	readw(ioaddr + MIntrStatus);
	writew(MICRIntEn, ioaddr + MIntrCtrl);
}

static int switch_port_external(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);
	u32 cfg;

	cfg = readl(ioaddr + ChipConfig);
	if (cfg & CfgExtPhy)
		return 0;

	if (netif_msg_link(np)) {
		printk(KERN_INFO "%s: switching to external transceiver.\n",
				dev->name);
	}

	
	writel(cfg | (CfgExtPhy | CfgPhyDis), ioaddr + ChipConfig);
	readl(ioaddr + ChipConfig);
	udelay(1);

	

	
	move_int_phy(dev, np->phy_addr_external);
	init_phy_fixup(dev);

	return 1;
}

static int switch_port_internal(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);
	int i;
	u32 cfg;
	u16 bmcr;

	cfg = readl(ioaddr + ChipConfig);
	if (!(cfg &CfgExtPhy))
		return 0;

	if (netif_msg_link(np)) {
		printk(KERN_INFO "%s: switching to internal transceiver.\n",
				dev->name);
	}
	
	cfg = cfg & ~(CfgExtPhy | CfgPhyDis);
	writel(cfg, ioaddr + ChipConfig);
	readl(ioaddr + ChipConfig);
	udelay(1);

	
	bmcr = readw(ioaddr+BasicControl+(MII_BMCR<<2));
	writel(bmcr | BMCR_RESET, ioaddr+BasicControl+(MII_BMCR<<2));
	readl(ioaddr + ChipConfig);
	udelay(10);
	for (i=0;i<NATSEMI_HW_TIMEOUT;i++) {
		bmcr = readw(ioaddr+BasicControl+(MII_BMCR<<2));
		if (!(bmcr & BMCR_RESET))
			break;
		udelay(10);
	}
	if (i==NATSEMI_HW_TIMEOUT && netif_msg_link(np)) {
		printk(KERN_INFO
			"%s: phy reset did not complete in %d usec.\n",
			dev->name, i*10);
	}
	
	init_phy_fixup(dev);

	return 1;
}

static int find_mii(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int tmp;
	int i;
	int did_switch;

	
	did_switch = switch_port_external(dev);

	for (i = 1; i <= 31; i++) {
		move_int_phy(dev, i);
		tmp = miiport_read(dev, i, MII_BMSR);
		if (tmp != 0xffff && tmp != 0x0000) {
			
			np->mii = (mdio_read(dev, MII_PHYSID1) << 16)
					+ mdio_read(dev, MII_PHYSID2);
	 		if (netif_msg_probe(np)) {
				printk(KERN_INFO "natsemi %s: found external phy %08x at address %d.\n",
						pci_name(np->pci_dev), np->mii, i);
			}
			break;
		}
	}
	
	if (did_switch)
		switch_port_internal(dev);
	return i;
}

#define CFG_RESET_SAVE 0xfde000
#define WCSR_RESET_SAVE 0x61f
#define RFCR_RESET_SAVE 0xf8500000

static void natsemi_reset(struct net_device *dev)
{
	int i;
	u32 cfg;
	u32 wcsr;
	u32 rfcr;
	u16 pmatch[3];
	u16 sopass[3];
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);


	
	cfg = readl(ioaddr + ChipConfig) & CFG_RESET_SAVE;
	
	wcsr = readl(ioaddr + WOLCmd) & WCSR_RESET_SAVE;
	
	rfcr = readl(ioaddr + RxFilterAddr) & RFCR_RESET_SAVE;
	
	for (i = 0; i < 3; i++) {
		writel(i*2, ioaddr + RxFilterAddr);
		pmatch[i] = readw(ioaddr + RxFilterData);
	}
	
	for (i = 0; i < 3; i++) {
		writel(0xa+(i*2), ioaddr + RxFilterAddr);
		sopass[i] = readw(ioaddr + RxFilterData);
	}

	
	writel(ChipReset, ioaddr + ChipCmd);
	for (i=0;i<NATSEMI_HW_TIMEOUT;i++) {
		if (!(readl(ioaddr + ChipCmd) & ChipReset))
			break;
		udelay(5);
	}
	if (i==NATSEMI_HW_TIMEOUT) {
		printk(KERN_WARNING "%s: reset did not complete in %d usec.\n",
			dev->name, i*5);
	} else if (netif_msg_hw(np)) {
		printk(KERN_DEBUG "%s: reset completed in %d usec.\n",
			dev->name, i*5);
	}

	
	cfg |= readl(ioaddr + ChipConfig) & ~CFG_RESET_SAVE;
	
	if (dev->if_port == PORT_TP)
		cfg &= ~(CfgExtPhy | CfgPhyDis);
	else
		cfg |= (CfgExtPhy | CfgPhyDis);
	writel(cfg, ioaddr + ChipConfig);
	
	wcsr |= readl(ioaddr + WOLCmd) & ~WCSR_RESET_SAVE;
	writel(wcsr, ioaddr + WOLCmd);
	
	rfcr |= readl(ioaddr + RxFilterAddr) & ~RFCR_RESET_SAVE;
	
	for (i = 0; i < 3; i++) {
		writel(i*2, ioaddr + RxFilterAddr);
		writew(pmatch[i], ioaddr + RxFilterData);
	}
	for (i = 0; i < 3; i++) {
		writel(0xa+(i*2), ioaddr + RxFilterAddr);
		writew(sopass[i], ioaddr + RxFilterData);
	}
	
	writel(rfcr, ioaddr + RxFilterAddr);
}

static void reset_rx(struct net_device *dev)
{
	int i;
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);

	np->intr_status &= ~RxResetDone;

	writel(RxReset, ioaddr + ChipCmd);

	for (i=0;i<NATSEMI_HW_TIMEOUT;i++) {
		np->intr_status |= readl(ioaddr + IntrStatus);
		if (np->intr_status & RxResetDone)
			break;
		udelay(15);
	}
	if (i==NATSEMI_HW_TIMEOUT) {
		printk(KERN_WARNING "%s: RX reset did not complete in %d usec.\n",
		       dev->name, i*15);
	} else if (netif_msg_hw(np)) {
		printk(KERN_WARNING "%s: RX reset took %d usec.\n",
		       dev->name, i*15);
	}
}

static void natsemi_reload_eeprom(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);
	int i;

	writel(EepromReload, ioaddr + PCIBusCfg);
	for (i=0;i<NATSEMI_HW_TIMEOUT;i++) {
		udelay(50);
		if (!(readl(ioaddr + PCIBusCfg) & EepromReload))
			break;
	}
	if (i==NATSEMI_HW_TIMEOUT) {
		printk(KERN_WARNING "natsemi %s: EEPROM did not reload in %d usec.\n",
			pci_name(np->pci_dev), i*50);
	} else if (netif_msg_hw(np)) {
		printk(KERN_DEBUG "natsemi %s: EEPROM reloaded in %d usec.\n",
			pci_name(np->pci_dev), i*50);
	}
}

static void natsemi_stop_rxtx(struct net_device *dev)
{
	void __iomem * ioaddr = ns_ioaddr(dev);
	struct netdev_private *np = netdev_priv(dev);
	int i;

	writel(RxOff | TxOff, ioaddr + ChipCmd);
	for(i=0;i< NATSEMI_HW_TIMEOUT;i++) {
		if ((readl(ioaddr + ChipCmd) & (TxOn|RxOn)) == 0)
			break;
		udelay(5);
	}
	if (i==NATSEMI_HW_TIMEOUT) {
		printk(KERN_WARNING "%s: Tx/Rx process did not stop in %d usec.\n",
			dev->name, i*5);
	} else if (netif_msg_hw(np)) {
		printk(KERN_DEBUG "%s: Tx/Rx process stopped in %d usec.\n",
			dev->name, i*5);
	}
}

static int netdev_open(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	int i;

	
	natsemi_reset(dev);

	i = request_irq(dev->irq, intr_handler, IRQF_SHARED, dev->name, dev);
	if (i) return i;

	if (netif_msg_ifup(np))
		printk(KERN_DEBUG "%s: netdev_open() irq %d.\n",
			dev->name, dev->irq);
	i = alloc_ring(dev);
	if (i < 0) {
		free_irq(dev->irq, dev);
		return i;
	}
	napi_enable(&np->napi);

	init_ring(dev);
	spin_lock_irq(&np->lock);
	init_registers(dev);
	
	for (i = 0; i < 3; i++) {
		u16 mac = (dev->dev_addr[2*i+1]<<8) + dev->dev_addr[2*i];

		writel(i*2, ioaddr + RxFilterAddr);
		writew(mac, ioaddr + RxFilterData);
	}
	writel(np->cur_rx_mode, ioaddr + RxFilterAddr);
	spin_unlock_irq(&np->lock);

	netif_start_queue(dev);

	if (netif_msg_ifup(np))
		printk(KERN_DEBUG "%s: Done netdev_open(), status: %#08x.\n",
			dev->name, (int)readl(ioaddr + ChipCmd));

	
	init_timer(&np->timer);
	np->timer.expires = round_jiffies(jiffies + NATSEMI_TIMER_FREQ);
	np->timer.data = (unsigned long)dev;
	np->timer.function = netdev_timer; 
	add_timer(&np->timer);

	return 0;
}

static void do_cable_magic(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = ns_ioaddr(dev);

	if (dev->if_port != PORT_TP)
		return;

	if (np->srr >= SRR_DP83816_A5)
		return;

	if (readl(ioaddr + ChipConfig) & CfgSpeed100) {
		u16 data;

		writew(1, ioaddr + PGSEL);
		data = readw(ioaddr + TSTDAT) & 0xff;
		if (!(data & 0x80) || ((data >= 0xd8) && (data <= 0xff))) {
			np = netdev_priv(dev);

			
			writew(TSTDAT_FIXED, ioaddr + TSTDAT);
			
			data = readw(ioaddr + DSPCFG);
			np->dspcfg = data | DSPCFG_LOCK;
			writew(np->dspcfg, ioaddr + DSPCFG);
		}
		writew(0, ioaddr + PGSEL);
	}
}

static void undo_cable_magic(struct net_device *dev)
{
	u16 data;
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	if (dev->if_port != PORT_TP)
		return;

	if (np->srr >= SRR_DP83816_A5)
		return;

	writew(1, ioaddr + PGSEL);
	
	data = readw(ioaddr + DSPCFG);
	np->dspcfg = data & ~DSPCFG_LOCK;
	writew(np->dspcfg, ioaddr + DSPCFG);
	writew(0, ioaddr + PGSEL);
}

static void check_link(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	int duplex = np->duplex;
	u16 bmsr;

	
	if (np->ignore_phy)
		goto propagate_state;

	mdio_read(dev, MII_BMSR);
	bmsr = mdio_read(dev, MII_BMSR);

	if (!(bmsr & BMSR_LSTATUS)) {
		if (netif_carrier_ok(dev)) {
			if (netif_msg_link(np))
				printk(KERN_NOTICE "%s: link down.\n",
				       dev->name);
			netif_carrier_off(dev);
			undo_cable_magic(dev);
		}
		return;
	}
	if (!netif_carrier_ok(dev)) {
		if (netif_msg_link(np))
			printk(KERN_NOTICE "%s: link up.\n", dev->name);
		netif_carrier_on(dev);
		do_cable_magic(dev);
	}

	duplex = np->full_duplex;
	if (!duplex) {
		if (bmsr & BMSR_ANEGCOMPLETE) {
			int tmp = mii_nway_result(
				np->advertising & mdio_read(dev, MII_LPA));
			if (tmp == LPA_100FULL || tmp == LPA_10FULL)
				duplex = 1;
		} else if (mdio_read(dev, MII_BMCR) & BMCR_FULLDPLX)
			duplex = 1;
	}

propagate_state:
	
	if (duplex ^ !!(np->rx_config & RxAcceptTx)) {
		if (netif_msg_link(np))
			printk(KERN_INFO
				"%s: Setting %s-duplex based on negotiated "
				"link capability.\n", dev->name,
				duplex ? "full" : "half");
		if (duplex) {
			np->rx_config |= RxAcceptTx;
			np->tx_config |= TxCarrierIgn | TxHeartIgn;
		} else {
			np->rx_config &= ~RxAcceptTx;
			np->tx_config &= ~(TxCarrierIgn | TxHeartIgn);
		}
		writel(np->tx_config, ioaddr + TxConfig);
		writel(np->rx_config, ioaddr + RxConfig);
	}
}

static void init_registers(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	init_phy_fixup(dev);

	
	readl(ioaddr + IntrStatus);

	writel(np->ring_dma, ioaddr + RxRingPtr);
	writel(np->ring_dma + RX_RING_SIZE * sizeof(struct netdev_desc),
		ioaddr + TxRingPtr);


	np->tx_config = TxAutoPad | TxCollRetry | TxMxdma_256 |
				TX_FLTH_VAL | TX_DRTH_VAL_START;
	writel(np->tx_config, ioaddr + TxConfig);

	np->rx_config = RxMxdma_256 | RX_DRTH_VAL;
	
	if (np->rx_buf_sz > NATSEMI_LONGPKT)
		np->rx_config |= RxAcceptLong;

	writel(np->rx_config, ioaddr + RxConfig);

	/* Disable PME:
	 * The PME bit is initialized from the EEPROM contents.
	 * PCI cards probably have PME disabled, but motherboard
	 * implementations may have PME set to enable WakeOnLan.
	 * With PME set the chip will scan incoming packets but
	 * nothing will be written to memory. */
	np->SavedClkRun = readl(ioaddr + ClkRun);
	writel(np->SavedClkRun & ~PMEEnable, ioaddr + ClkRun);
	if (np->SavedClkRun & PMEStatus && netif_msg_wol(np)) {
		printk(KERN_NOTICE "%s: Wake-up event %#08x\n",
			dev->name, readl(ioaddr + WOLCmd));
	}

	check_link(dev);
	__set_rx_mode(dev);

	
	writel(DEFAULT_INTR, ioaddr + IntrMask);
	natsemi_irq_enable(dev);

	writel(RxOn | TxOn, ioaddr + ChipCmd);
	writel(StatsClear, ioaddr + StatsCtrl); 
}

static void netdev_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	int next_tick = NATSEMI_TIMER_FREQ;

	if (netif_msg_timer(np)) {
		printk(KERN_DEBUG "%s: Media selection timer tick.\n",
			dev->name);
	}

	if (dev->if_port == PORT_TP) {
		u16 dspcfg;

		spin_lock_irq(&np->lock);
		
		writew(1, ioaddr+PGSEL);
		dspcfg = readw(ioaddr+DSPCFG);
		writew(0, ioaddr+PGSEL);
		if (np->dspcfg_workaround && dspcfg != np->dspcfg) {
			if (!netif_queue_stopped(dev)) {
				spin_unlock_irq(&np->lock);
				if (netif_msg_drv(np))
					printk(KERN_NOTICE "%s: possible phy reset: "
						"re-initializing\n", dev->name);
				disable_irq(dev->irq);
				spin_lock_irq(&np->lock);
				natsemi_stop_rxtx(dev);
				dump_ring(dev);
				reinit_ring(dev);
				init_registers(dev);
				spin_unlock_irq(&np->lock);
				enable_irq(dev->irq);
			} else {
				
				next_tick = HZ;
				spin_unlock_irq(&np->lock);
			}
		} else {
			
			check_link(dev);
			spin_unlock_irq(&np->lock);
		}
	} else {
		spin_lock_irq(&np->lock);
		check_link(dev);
		spin_unlock_irq(&np->lock);
	}
	if (np->oom) {
		disable_irq(dev->irq);
		np->oom = 0;
		refill_rx(dev);
		enable_irq(dev->irq);
		if (!np->oom) {
			writel(RxOn, ioaddr + ChipCmd);
		} else {
			next_tick = 1;
		}
	}

	if (next_tick > 1)
		mod_timer(&np->timer, round_jiffies(jiffies + next_tick));
	else
		mod_timer(&np->timer, jiffies + next_tick);
}

static void dump_ring(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);

	if (netif_msg_pktdata(np)) {
		int i;
		printk(KERN_DEBUG "  Tx ring at %p:\n", np->tx_ring);
		for (i = 0; i < TX_RING_SIZE; i++) {
			printk(KERN_DEBUG " #%d desc. %#08x %#08x %#08x.\n",
				i, np->tx_ring[i].next_desc,
				np->tx_ring[i].cmd_status,
				np->tx_ring[i].addr);
		}
		printk(KERN_DEBUG "  Rx ring %p:\n", np->rx_ring);
		for (i = 0; i < RX_RING_SIZE; i++) {
			printk(KERN_DEBUG " #%d desc. %#08x %#08x %#08x.\n",
				i, np->rx_ring[i].next_desc,
				np->rx_ring[i].cmd_status,
				np->rx_ring[i].addr);
		}
	}
}

static void ns_tx_timeout(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	disable_irq(dev->irq);
	spin_lock_irq(&np->lock);
	if (!np->hands_off) {
		if (netif_msg_tx_err(np))
			printk(KERN_WARNING
				"%s: Transmit timed out, status %#08x,"
				" resetting...\n",
				dev->name, readl(ioaddr + IntrStatus));
		dump_ring(dev);

		natsemi_reset(dev);
		reinit_ring(dev);
		init_registers(dev);
	} else {
		printk(KERN_WARNING
			"%s: tx_timeout while in hands_off state?\n",
			dev->name);
	}
	spin_unlock_irq(&np->lock);
	enable_irq(dev->irq);

	dev->trans_start = jiffies; 
	dev->stats.tx_errors++;
	netif_wake_queue(dev);
}

static int alloc_ring(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	np->rx_ring = pci_alloc_consistent(np->pci_dev,
		sizeof(struct netdev_desc) * (RX_RING_SIZE+TX_RING_SIZE),
		&np->ring_dma);
	if (!np->rx_ring)
		return -ENOMEM;
	np->tx_ring = &np->rx_ring[RX_RING_SIZE];
	return 0;
}

static void refill_rx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);

	
	for (; np->cur_rx - np->dirty_rx > 0; np->dirty_rx++) {
		struct sk_buff *skb;
		int entry = np->dirty_rx % RX_RING_SIZE;
		if (np->rx_skbuff[entry] == NULL) {
			unsigned int buflen = np->rx_buf_sz+NATSEMI_PADDING;
			skb = netdev_alloc_skb(dev, buflen);
			np->rx_skbuff[entry] = skb;
			if (skb == NULL)
				break; 
			np->rx_dma[entry] = pci_map_single(np->pci_dev,
				skb->data, buflen, PCI_DMA_FROMDEVICE);
			np->rx_ring[entry].addr = cpu_to_le32(np->rx_dma[entry]);
		}
		np->rx_ring[entry].cmd_status = cpu_to_le32(np->rx_buf_sz);
	}
	if (np->cur_rx - np->dirty_rx == RX_RING_SIZE) {
		if (netif_msg_rx_err(np))
			printk(KERN_WARNING "%s: going OOM.\n", dev->name);
		np->oom = 1;
	}
}

static void set_bufsize(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	if (dev->mtu <= ETH_DATA_LEN)
		np->rx_buf_sz = ETH_DATA_LEN + NATSEMI_HEADERS;
	else
		np->rx_buf_sz = dev->mtu + NATSEMI_HEADERS;
}

static void init_ring(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int i;

	
	np->dirty_tx = np->cur_tx = 0;
	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_skbuff[i] = NULL;
		np->tx_ring[i].next_desc = cpu_to_le32(np->ring_dma
			+sizeof(struct netdev_desc)
			*((i+1)%TX_RING_SIZE+RX_RING_SIZE));
		np->tx_ring[i].cmd_status = 0;
	}

	
	np->dirty_rx = 0;
	np->cur_rx = RX_RING_SIZE;
	np->oom = 0;
	set_bufsize(dev);

	np->rx_head_desc = &np->rx_ring[0];

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].next_desc = cpu_to_le32(np->ring_dma
				+sizeof(struct netdev_desc)
				*((i+1)%RX_RING_SIZE));
		np->rx_ring[i].cmd_status = cpu_to_le32(DescOwn);
		np->rx_skbuff[i] = NULL;
	}
	refill_rx(dev);
	dump_ring(dev);
}

static void drain_tx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int i;

	for (i = 0; i < TX_RING_SIZE; i++) {
		if (np->tx_skbuff[i]) {
			pci_unmap_single(np->pci_dev,
				np->tx_dma[i], np->tx_skbuff[i]->len,
				PCI_DMA_TODEVICE);
			dev_kfree_skb(np->tx_skbuff[i]);
			dev->stats.tx_dropped++;
		}
		np->tx_skbuff[i] = NULL;
	}
}

static void drain_rx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	unsigned int buflen = np->rx_buf_sz;
	int i;

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].cmd_status = 0;
		np->rx_ring[i].addr = cpu_to_le32(0xBADF00D0); 
		if (np->rx_skbuff[i]) {
			pci_unmap_single(np->pci_dev, np->rx_dma[i],
				buflen + NATSEMI_PADDING,
				PCI_DMA_FROMDEVICE);
			dev_kfree_skb(np->rx_skbuff[i]);
		}
		np->rx_skbuff[i] = NULL;
	}
}

static void drain_ring(struct net_device *dev)
{
	drain_rx(dev);
	drain_tx(dev);
}

static void free_ring(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	pci_free_consistent(np->pci_dev,
		sizeof(struct netdev_desc) * (RX_RING_SIZE+TX_RING_SIZE),
		np->rx_ring, np->ring_dma);
}

static void reinit_rx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int i;

	
	np->dirty_rx = 0;
	np->cur_rx = RX_RING_SIZE;
	np->rx_head_desc = &np->rx_ring[0];
	
	for (i = 0; i < RX_RING_SIZE; i++)
		np->rx_ring[i].cmd_status = cpu_to_le32(DescOwn);

	refill_rx(dev);
}

static void reinit_ring(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int i;

	
	drain_tx(dev);
	np->dirty_tx = np->cur_tx = 0;
	for (i=0;i<TX_RING_SIZE;i++)
		np->tx_ring[i].cmd_status = 0;

	reinit_rx(dev);
}

static netdev_tx_t start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	unsigned entry;
	unsigned long flags;


	
	entry = np->cur_tx % TX_RING_SIZE;

	np->tx_skbuff[entry] = skb;
	np->tx_dma[entry] = pci_map_single(np->pci_dev,
				skb->data,skb->len, PCI_DMA_TODEVICE);

	np->tx_ring[entry].addr = cpu_to_le32(np->tx_dma[entry]);

	spin_lock_irqsave(&np->lock, flags);

	if (!np->hands_off) {
		np->tx_ring[entry].cmd_status = cpu_to_le32(DescOwn | skb->len);
		wmb();
		np->cur_tx++;
		if (np->cur_tx - np->dirty_tx >= TX_QUEUE_LEN - 1) {
			netdev_tx_done(dev);
			if (np->cur_tx - np->dirty_tx >= TX_QUEUE_LEN - 1)
				netif_stop_queue(dev);
		}
		
		writel(TxOn, ioaddr + ChipCmd);
	} else {
		dev_kfree_skb_irq(skb);
		dev->stats.tx_dropped++;
	}
	spin_unlock_irqrestore(&np->lock, flags);

	if (netif_msg_tx_queued(np)) {
		printk(KERN_DEBUG "%s: Transmit frame #%d queued in slot %d.\n",
			dev->name, np->cur_tx, entry);
	}
	return NETDEV_TX_OK;
}

static void netdev_tx_done(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);

	for (; np->cur_tx - np->dirty_tx > 0; np->dirty_tx++) {
		int entry = np->dirty_tx % TX_RING_SIZE;
		if (np->tx_ring[entry].cmd_status & cpu_to_le32(DescOwn))
			break;
		if (netif_msg_tx_done(np))
			printk(KERN_DEBUG
				"%s: tx frame #%d finished, status %#08x.\n",
					dev->name, np->dirty_tx,
					le32_to_cpu(np->tx_ring[entry].cmd_status));
		if (np->tx_ring[entry].cmd_status & cpu_to_le32(DescPktOK)) {
			dev->stats.tx_packets++;
			dev->stats.tx_bytes += np->tx_skbuff[entry]->len;
		} else { 
			int tx_status =
				le32_to_cpu(np->tx_ring[entry].cmd_status);
			if (tx_status & (DescTxAbort|DescTxExcColl))
				dev->stats.tx_aborted_errors++;
			if (tx_status & DescTxFIFO)
				dev->stats.tx_fifo_errors++;
			if (tx_status & DescTxCarrier)
				dev->stats.tx_carrier_errors++;
			if (tx_status & DescTxOOWCol)
				dev->stats.tx_window_errors++;
			dev->stats.tx_errors++;
		}
		pci_unmap_single(np->pci_dev,np->tx_dma[entry],
					np->tx_skbuff[entry]->len,
					PCI_DMA_TODEVICE);
		
		dev_kfree_skb_irq(np->tx_skbuff[entry]);
		np->tx_skbuff[entry] = NULL;
	}
	if (netif_queue_stopped(dev) &&
	    np->cur_tx - np->dirty_tx < TX_QUEUE_LEN - 4) {
		
		netif_wake_queue(dev);
	}
}

static irqreturn_t intr_handler(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	if (np->hands_off || !readl(ioaddr + IntrEnable))
		return IRQ_NONE;

	np->intr_status = readl(ioaddr + IntrStatus);

	if (!np->intr_status)
		return IRQ_NONE;

	if (netif_msg_intr(np))
		printk(KERN_DEBUG
		       "%s: Interrupt, status %#08x, mask %#08x.\n",
		       dev->name, np->intr_status,
		       readl(ioaddr + IntrMask));

	prefetch(&np->rx_skbuff[np->cur_rx % RX_RING_SIZE]);

	if (napi_schedule_prep(&np->napi)) {
		
		natsemi_irq_disable(dev);
		__napi_schedule(&np->napi);
	} else
		printk(KERN_WARNING
	       	       "%s: Ignoring interrupt, status %#08x, mask %#08x.\n",
		       dev->name, np->intr_status,
		       readl(ioaddr + IntrMask));

	return IRQ_HANDLED;
}

static int natsemi_poll(struct napi_struct *napi, int budget)
{
	struct netdev_private *np = container_of(napi, struct netdev_private, napi);
	struct net_device *dev = np->dev;
	void __iomem * ioaddr = ns_ioaddr(dev);
	int work_done = 0;

	do {
		if (netif_msg_intr(np))
			printk(KERN_DEBUG
			       "%s: Poll, status %#08x, mask %#08x.\n",
			       dev->name, np->intr_status,
			       readl(ioaddr + IntrMask));

		if (np->intr_status &
		    (IntrRxDone | IntrRxIntr | RxStatusFIFOOver |
		     IntrRxErr | IntrRxOverrun)) {
			netdev_rx(dev, &work_done, budget);
		}

		if (np->intr_status &
		    (IntrTxDone | IntrTxIntr | IntrTxIdle | IntrTxErr)) {
			spin_lock(&np->lock);
			netdev_tx_done(dev);
			spin_unlock(&np->lock);
		}

		
		if (np->intr_status & IntrAbnormalSummary)
			netdev_error(dev, np->intr_status);

		if (work_done >= budget)
			return work_done;

		np->intr_status = readl(ioaddr + IntrStatus);
	} while (np->intr_status);

	napi_complete(napi);

	spin_lock(&np->lock);
	if (!np->hands_off)
		natsemi_irq_enable(dev);
	spin_unlock(&np->lock);

	return work_done;
}

static void netdev_rx(struct net_device *dev, int *work_done, int work_to_do)
{
	struct netdev_private *np = netdev_priv(dev);
	int entry = np->cur_rx % RX_RING_SIZE;
	int boguscnt = np->dirty_rx + RX_RING_SIZE - np->cur_rx;
	s32 desc_status = le32_to_cpu(np->rx_head_desc->cmd_status);
	unsigned int buflen = np->rx_buf_sz;
	void __iomem * ioaddr = ns_ioaddr(dev);

	
	while (desc_status < 0) { 
		int pkt_len;
		if (netif_msg_rx_status(np))
			printk(KERN_DEBUG
				"  netdev_rx() entry %d status was %#08x.\n",
				entry, desc_status);
		if (--boguscnt < 0)
			break;

		if (*work_done >= work_to_do)
			break;

		(*work_done)++;

		pkt_len = (desc_status & DescSizeMask) - 4;
		if ((desc_status&(DescMore|DescPktOK|DescRxLong)) != DescPktOK){
			if (desc_status & DescMore) {
				unsigned long flags;

				if (netif_msg_rx_err(np))
					printk(KERN_WARNING
						"%s: Oversized(?) Ethernet "
						"frame spanned multiple "
						"buffers, entry %#08x "
						"status %#08x.\n", dev->name,
						np->cur_rx, desc_status);
				dev->stats.rx_length_errors++;


				spin_lock_irqsave(&np->lock, flags);
				reset_rx(dev);
				reinit_rx(dev);
				writel(np->ring_dma, ioaddr + RxRingPtr);
				check_link(dev);
				spin_unlock_irqrestore(&np->lock, flags);

				break;

			} else {
				
				dev->stats.rx_errors++;
				if (desc_status & (DescRxAbort|DescRxOver))
					dev->stats.rx_over_errors++;
				if (desc_status & (DescRxLong|DescRxRunt))
					dev->stats.rx_length_errors++;
				if (desc_status & (DescRxInvalid|DescRxAlign))
					dev->stats.rx_frame_errors++;
				if (desc_status & DescRxCRC)
					dev->stats.rx_crc_errors++;
			}
		} else if (pkt_len > np->rx_buf_sz) {
		} else {
			struct sk_buff *skb;
			
			if (pkt_len < rx_copybreak &&
			    (skb = netdev_alloc_skb(dev, pkt_len + RX_OFFSET)) != NULL) {
				
				skb_reserve(skb, RX_OFFSET);
				pci_dma_sync_single_for_cpu(np->pci_dev,
					np->rx_dma[entry],
					buflen,
					PCI_DMA_FROMDEVICE);
				skb_copy_to_linear_data(skb,
					np->rx_skbuff[entry]->data, pkt_len);
				skb_put(skb, pkt_len);
				pci_dma_sync_single_for_device(np->pci_dev,
					np->rx_dma[entry],
					buflen,
					PCI_DMA_FROMDEVICE);
			} else {
				pci_unmap_single(np->pci_dev, np->rx_dma[entry],
						 buflen + NATSEMI_PADDING,
						 PCI_DMA_FROMDEVICE);
				skb_put(skb = np->rx_skbuff[entry], pkt_len);
				np->rx_skbuff[entry] = NULL;
			}
			skb->protocol = eth_type_trans(skb, dev);
			netif_receive_skb(skb);
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += pkt_len;
		}
		entry = (++np->cur_rx) % RX_RING_SIZE;
		np->rx_head_desc = &np->rx_ring[entry];
		desc_status = le32_to_cpu(np->rx_head_desc->cmd_status);
	}
	refill_rx(dev);

	
	if (np->oom)
		mod_timer(&np->timer, jiffies + 1);
	else
		writel(RxOn, ioaddr + ChipCmd);
}

static void netdev_error(struct net_device *dev, int intr_status)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	spin_lock(&np->lock);
	if (intr_status & LinkChange) {
		u16 lpa = mdio_read(dev, MII_LPA);
		if (mdio_read(dev, MII_BMCR) & BMCR_ANENABLE &&
		    netif_msg_link(np)) {
			printk(KERN_INFO
				"%s: Autonegotiation advertising"
				" %#04x  partner %#04x.\n", dev->name,
				np->advertising, lpa);
		}

		
		readw(ioaddr + MIntrStatus);
		check_link(dev);
	}
	if (intr_status & StatsMax) {
		__get_stats(dev);
	}
	if (intr_status & IntrTxUnderrun) {
		if ((np->tx_config & TxDrthMask) < TX_DRTH_VAL_LIMIT) {
			np->tx_config += TX_DRTH_VAL_INC;
			if (netif_msg_tx_err(np))
				printk(KERN_NOTICE
					"%s: increased tx threshold, txcfg %#08x.\n",
					dev->name, np->tx_config);
		} else {
			if (netif_msg_tx_err(np))
				printk(KERN_NOTICE
					"%s: tx underrun with maximum tx threshold, txcfg %#08x.\n",
					dev->name, np->tx_config);
		}
		writel(np->tx_config, ioaddr + TxConfig);
	}
	if (intr_status & WOLPkt && netif_msg_wol(np)) {
		int wol_status = readl(ioaddr + WOLCmd);
		printk(KERN_NOTICE "%s: Link wake-up event %#08x\n",
			dev->name, wol_status);
	}
	if (intr_status & RxStatusFIFOOver) {
		if (netif_msg_rx_err(np) && netif_msg_intr(np)) {
			printk(KERN_NOTICE "%s: Rx status FIFO overrun\n",
				dev->name);
		}
		dev->stats.rx_fifo_errors++;
		dev->stats.rx_errors++;
	}
	
	if (intr_status & IntrPCIErr) {
		printk(KERN_NOTICE "%s: PCI error %#08x\n", dev->name,
			intr_status & IntrPCIErr);
		dev->stats.tx_fifo_errors++;
		dev->stats.tx_errors++;
		dev->stats.rx_fifo_errors++;
		dev->stats.rx_errors++;
	}
	spin_unlock(&np->lock);
}

static void __get_stats(struct net_device *dev)
{
	void __iomem * ioaddr = ns_ioaddr(dev);

	
	dev->stats.rx_crc_errors += readl(ioaddr + RxCRCErrs);
	dev->stats.rx_missed_errors += readl(ioaddr + RxMissed);
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);

	
	spin_lock_irq(&np->lock);
	if (netif_running(dev) && !np->hands_off)
		__get_stats(dev);
	spin_unlock_irq(&np->lock);

	return &dev->stats;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void natsemi_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	intr_handler(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

#define HASH_TABLE	0x200
static void __set_rx_mode(struct net_device *dev)
{
	void __iomem * ioaddr = ns_ioaddr(dev);
	struct netdev_private *np = netdev_priv(dev);
	u8 mc_filter[64]; 
	u32 rx_mode;

	if (dev->flags & IFF_PROMISC) { 
		rx_mode = RxFilterEnable | AcceptBroadcast
			| AcceptAllMulticast | AcceptAllPhys | AcceptMyPhys;
	} else if ((netdev_mc_count(dev) > multicast_filter_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		rx_mode = RxFilterEnable | AcceptBroadcast
			| AcceptAllMulticast | AcceptMyPhys;
	} else {
		struct netdev_hw_addr *ha;
		int i;

		memset(mc_filter, 0, sizeof(mc_filter));
		netdev_for_each_mc_addr(ha, dev) {
			int b = (ether_crc(ETH_ALEN, ha->addr) >> 23) & 0x1ff;
			mc_filter[b/8] |= (1 << (b & 0x07));
		}
		rx_mode = RxFilterEnable | AcceptBroadcast
			| AcceptMulticast | AcceptMyPhys;
		for (i = 0; i < 64; i += 2) {
			writel(HASH_TABLE + i, ioaddr + RxFilterAddr);
			writel((mc_filter[i + 1] << 8) + mc_filter[i],
			       ioaddr + RxFilterData);
		}
	}
	writel(rx_mode, ioaddr + RxFilterAddr);
	np->cur_rx_mode = rx_mode;
}

static int natsemi_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 64 || new_mtu > NATSEMI_RX_LIMIT-NATSEMI_HEADERS)
		return -EINVAL;

	dev->mtu = new_mtu;

	
	if (netif_running(dev)) {
		struct netdev_private *np = netdev_priv(dev);
		void __iomem * ioaddr = ns_ioaddr(dev);

		disable_irq(dev->irq);
		spin_lock(&np->lock);
		
		natsemi_stop_rxtx(dev);
		
		drain_rx(dev);
		
		set_bufsize(dev);
		reinit_rx(dev);
		writel(np->ring_dma, ioaddr + RxRingPtr);
		
		writel(RxOn | TxOn, ioaddr + ChipCmd);
		spin_unlock(&np->lock);
		enable_irq(dev->irq);
	}
	return 0;
}

static void set_rx_mode(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	spin_lock_irq(&np->lock);
	if (!np->hands_off)
		__set_rx_mode(dev);
	spin_unlock_irq(&np->lock);
}

static void get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct netdev_private *np = netdev_priv(dev);
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(np->pci_dev), sizeof(info->bus_info));
}

static int get_regs_len(struct net_device *dev)
{
	return NATSEMI_REGS_SIZE;
}

static int get_eeprom_len(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	return np->eeprom_size;
}

static int get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct netdev_private *np = netdev_priv(dev);
	spin_lock_irq(&np->lock);
	netdev_get_ecmd(dev, ecmd);
	spin_unlock_irq(&np->lock);
	return 0;
}

static int set_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct netdev_private *np = netdev_priv(dev);
	int res;
	spin_lock_irq(&np->lock);
	res = netdev_set_ecmd(dev, ecmd);
	spin_unlock_irq(&np->lock);
	return res;
}

static void get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct netdev_private *np = netdev_priv(dev);
	spin_lock_irq(&np->lock);
	netdev_get_wol(dev, &wol->supported, &wol->wolopts);
	netdev_get_sopass(dev, wol->sopass);
	spin_unlock_irq(&np->lock);
}

static int set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct netdev_private *np = netdev_priv(dev);
	int res;
	spin_lock_irq(&np->lock);
	netdev_set_wol(dev, wol->wolopts);
	res = netdev_set_sopass(dev, wol->sopass);
	spin_unlock_irq(&np->lock);
	return res;
}

static void get_regs(struct net_device *dev, struct ethtool_regs *regs, void *buf)
{
	struct netdev_private *np = netdev_priv(dev);
	regs->version = NATSEMI_REGS_VER;
	spin_lock_irq(&np->lock);
	netdev_get_regs(dev, buf);
	spin_unlock_irq(&np->lock);
}

static u32 get_msglevel(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	return np->msg_enable;
}

static void set_msglevel(struct net_device *dev, u32 val)
{
	struct netdev_private *np = netdev_priv(dev);
	np->msg_enable = val;
}

static int nway_reset(struct net_device *dev)
{
	int tmp;
	int r = -EINVAL;
	
	tmp = mdio_read(dev, MII_BMCR);
	if (tmp & BMCR_ANENABLE) {
		tmp |= (BMCR_ANRESTART);
		mdio_write(dev, MII_BMCR, tmp);
		r = 0;
	}
	return r;
}

static u32 get_link(struct net_device *dev)
{
	
	mdio_read(dev, MII_BMSR);
	return (mdio_read(dev, MII_BMSR)&BMSR_LSTATUS) ? 1:0;
}

static int get_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom, u8 *data)
{
	struct netdev_private *np = netdev_priv(dev);
	u8 *eebuf;
	int res;

	eebuf = kmalloc(np->eeprom_size, GFP_KERNEL);
	if (!eebuf)
		return -ENOMEM;

	eeprom->magic = PCI_VENDOR_ID_NS | (PCI_DEVICE_ID_NS_83815<<16);
	spin_lock_irq(&np->lock);
	res = netdev_get_eeprom(dev, eebuf);
	spin_unlock_irq(&np->lock);
	if (!res)
		memcpy(data, eebuf+eeprom->offset, eeprom->len);
	kfree(eebuf);
	return res;
}

static const struct ethtool_ops ethtool_ops = {
	.get_drvinfo = get_drvinfo,
	.get_regs_len = get_regs_len,
	.get_eeprom_len = get_eeprom_len,
	.get_settings = get_settings,
	.set_settings = set_settings,
	.get_wol = get_wol,
	.set_wol = set_wol,
	.get_regs = get_regs,
	.get_msglevel = get_msglevel,
	.set_msglevel = set_msglevel,
	.nway_reset = nway_reset,
	.get_link = get_link,
	.get_eeprom = get_eeprom,
};

static int netdev_set_wol(struct net_device *dev, u32 newval)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	u32 data = readl(ioaddr + WOLCmd) & ~WakeOptsSummary;

	
	if (newval & WAKE_PHY)
		data |= WakePhy;
	if (newval & WAKE_UCAST)
		data |= WakeUnicast;
	if (newval & WAKE_MCAST)
		data |= WakeMulticast;
	if (newval & WAKE_BCAST)
		data |= WakeBroadcast;
	if (newval & WAKE_ARP)
		data |= WakeArp;
	if (newval & WAKE_MAGIC)
		data |= WakeMagic;
	if (np->srr >= SRR_DP83815_D) {
		if (newval & WAKE_MAGICSECURE) {
			data |= WakeMagicSecure;
		}
	}

	writel(data, ioaddr + WOLCmd);

	return 0;
}

static int netdev_get_wol(struct net_device *dev, u32 *supported, u32 *cur)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	u32 regval = readl(ioaddr + WOLCmd);

	*supported = (WAKE_PHY | WAKE_UCAST | WAKE_MCAST | WAKE_BCAST
			| WAKE_ARP | WAKE_MAGIC);

	if (np->srr >= SRR_DP83815_D) {
		
		*supported |= WAKE_MAGICSECURE;
	}
	*cur = 0;

	
	if (regval & WakePhy)
		*cur |= WAKE_PHY;
	if (regval & WakeUnicast)
		*cur |= WAKE_UCAST;
	if (regval & WakeMulticast)
		*cur |= WAKE_MCAST;
	if (regval & WakeBroadcast)
		*cur |= WAKE_BCAST;
	if (regval & WakeArp)
		*cur |= WAKE_ARP;
	if (regval & WakeMagic)
		*cur |= WAKE_MAGIC;
	if (regval & WakeMagicSecure) {
		
		*cur |= WAKE_MAGICSECURE;
	}

	return 0;
}

static int netdev_set_sopass(struct net_device *dev, u8 *newval)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	u16 *sval = (u16 *)newval;
	u32 addr;

	if (np->srr < SRR_DP83815_D) {
		return 0;
	}

	
	addr = readl(ioaddr + RxFilterAddr) & ~RFCRAddressMask;
	addr &= ~RxFilterEnable;
	writel(addr, ioaddr + RxFilterAddr);

	
	writel(addr | 0xa, ioaddr + RxFilterAddr);
	writew(sval[0], ioaddr + RxFilterData);

	writel(addr | 0xc, ioaddr + RxFilterAddr);
	writew(sval[1], ioaddr + RxFilterData);

	writel(addr | 0xe, ioaddr + RxFilterAddr);
	writew(sval[2], ioaddr + RxFilterData);

	
	writel(addr | RxFilterEnable, ioaddr + RxFilterAddr);

	return 0;
}

static int netdev_get_sopass(struct net_device *dev, u8 *data)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);
	u16 *sval = (u16 *)data;
	u32 addr;

	if (np->srr < SRR_DP83815_D) {
		sval[0] = sval[1] = sval[2] = 0;
		return 0;
	}

	
	addr = readl(ioaddr + RxFilterAddr) & ~RFCRAddressMask;

	writel(addr | 0xa, ioaddr + RxFilterAddr);
	sval[0] = readw(ioaddr + RxFilterData);

	writel(addr | 0xc, ioaddr + RxFilterAddr);
	sval[1] = readw(ioaddr + RxFilterData);

	writel(addr | 0xe, ioaddr + RxFilterAddr);
	sval[2] = readw(ioaddr + RxFilterData);

	writel(addr, ioaddr + RxFilterAddr);

	return 0;
}

static int netdev_get_ecmd(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct netdev_private *np = netdev_priv(dev);
	u32 tmp;

	ecmd->port        = dev->if_port;
	ethtool_cmd_speed_set(ecmd, np->speed);
	ecmd->duplex      = np->duplex;
	ecmd->autoneg     = np->autoneg;
	ecmd->advertising = 0;
	if (np->advertising & ADVERTISE_10HALF)
		ecmd->advertising |= ADVERTISED_10baseT_Half;
	if (np->advertising & ADVERTISE_10FULL)
		ecmd->advertising |= ADVERTISED_10baseT_Full;
	if (np->advertising & ADVERTISE_100HALF)
		ecmd->advertising |= ADVERTISED_100baseT_Half;
	if (np->advertising & ADVERTISE_100FULL)
		ecmd->advertising |= ADVERTISED_100baseT_Full;
	ecmd->supported   = (SUPPORTED_Autoneg |
		SUPPORTED_10baseT_Half  | SUPPORTED_10baseT_Full  |
		SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full |
		SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_FIBRE);
	ecmd->phy_address = np->phy_addr_external;

	
	switch (ecmd->port) {
	default:
	case PORT_TP:
		ecmd->advertising |= ADVERTISED_TP;
		ecmd->transceiver = XCVR_INTERNAL;
		break;
	case PORT_MII:
		ecmd->advertising |= ADVERTISED_MII;
		ecmd->transceiver = XCVR_EXTERNAL;
		break;
	case PORT_FIBRE:
		ecmd->advertising |= ADVERTISED_FIBRE;
		ecmd->transceiver = XCVR_EXTERNAL;
		break;
	}

	
	if (ecmd->autoneg == AUTONEG_ENABLE) {
		ecmd->advertising |= ADVERTISED_Autoneg;
		tmp = mii_nway_result(
			np->advertising & mdio_read(dev, MII_LPA));
		if (tmp == LPA_100FULL || tmp == LPA_100HALF)
			ethtool_cmd_speed_set(ecmd, SPEED_100);
		else
			ethtool_cmd_speed_set(ecmd, SPEED_10);
		if (tmp == LPA_100FULL || tmp == LPA_10FULL)
			ecmd->duplex = DUPLEX_FULL;
		else
			ecmd->duplex = DUPLEX_HALF;
	}

	

	return 0;
}

static int netdev_set_ecmd(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct netdev_private *np = netdev_priv(dev);

	if (ecmd->port != PORT_TP && ecmd->port != PORT_MII && ecmd->port != PORT_FIBRE)
		return -EINVAL;
	if (ecmd->transceiver != XCVR_INTERNAL && ecmd->transceiver != XCVR_EXTERNAL)
		return -EINVAL;
	if (ecmd->autoneg == AUTONEG_ENABLE) {
		if ((ecmd->advertising & (ADVERTISED_10baseT_Half |
					  ADVERTISED_10baseT_Full |
					  ADVERTISED_100baseT_Half |
					  ADVERTISED_100baseT_Full)) == 0) {
			return -EINVAL;
		}
	} else if (ecmd->autoneg == AUTONEG_DISABLE) {
		u32 speed = ethtool_cmd_speed(ecmd);
		if (speed != SPEED_10 && speed != SPEED_100)
			return -EINVAL;
		if (ecmd->duplex != DUPLEX_HALF && ecmd->duplex != DUPLEX_FULL)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	if (np->ignore_phy && (ecmd->autoneg == AUTONEG_ENABLE ||
			       ecmd->port == PORT_TP))
		return -EINVAL;


	

	
	dev->if_port          = ecmd->port;
	np->autoneg           = ecmd->autoneg;
	np->phy_addr_external = ecmd->phy_address & PhyAddrMask;
	if (np->autoneg == AUTONEG_ENABLE) {
		
		np->advertising &= ~(ADVERTISE_ALL | ADVERTISE_100BASE4);
		if (ecmd->advertising & ADVERTISED_10baseT_Half)
			np->advertising |= ADVERTISE_10HALF;
		if (ecmd->advertising & ADVERTISED_10baseT_Full)
			np->advertising |= ADVERTISE_10FULL;
		if (ecmd->advertising & ADVERTISED_100baseT_Half)
			np->advertising |= ADVERTISE_100HALF;
		if (ecmd->advertising & ADVERTISED_100baseT_Full)
			np->advertising |= ADVERTISE_100FULL;
	} else {
		np->speed  = ethtool_cmd_speed(ecmd);
		np->duplex = ecmd->duplex;
		
		if (np->duplex == DUPLEX_HALF)
			np->full_duplex = 0;
	}

	
	if (ecmd->port == PORT_TP)
		switch_port_internal(dev);
	else
		switch_port_external(dev);

	
	init_phy_fixup(dev);
	check_link(dev);
	return 0;
}

static int netdev_get_regs(struct net_device *dev, u8 *buf)
{
	int i;
	int j;
	u32 rfcr;
	u32 *rbuf = (u32 *)buf;
	void __iomem * ioaddr = ns_ioaddr(dev);

	
	for (i = 0; i < NATSEMI_PG0_NREGS/2; i++) {
		rbuf[i] = readl(ioaddr + i*4);
	}

	
	for (i = NATSEMI_PG0_NREGS/2; i < NATSEMI_PG0_NREGS; i++)
		rbuf[i] = mdio_read(dev, i & 0x1f);

	
	writew(1, ioaddr + PGSEL);
	rbuf[i++] = readw(ioaddr + PMDCSR);
	rbuf[i++] = readw(ioaddr + TSTDAT);
	rbuf[i++] = readw(ioaddr + DSPCFG);
	rbuf[i++] = readw(ioaddr + SDCFG);
	writew(0, ioaddr + PGSEL);

	
	rfcr = readl(ioaddr + RxFilterAddr);
	for (j = 0; j < NATSEMI_RFDR_NREGS; j++) {
		writel(j*2, ioaddr + RxFilterAddr);
		rbuf[i++] = readw(ioaddr + RxFilterData);
	}
	writel(rfcr, ioaddr + RxFilterAddr);

	
	if (rbuf[4] & rbuf[5]) {
		printk(KERN_WARNING
			"%s: shoot, we dropped an interrupt (%#08x)\n",
			dev->name, rbuf[4] & rbuf[5]);
	}

	return 0;
}

#define SWAP_BITS(x)	( (((x) & 0x0001) << 15) | (((x) & 0x0002) << 13) \
			| (((x) & 0x0004) << 11) | (((x) & 0x0008) << 9)  \
			| (((x) & 0x0010) << 7)  | (((x) & 0x0020) << 5)  \
			| (((x) & 0x0040) << 3)  | (((x) & 0x0080) << 1)  \
			| (((x) & 0x0100) >> 1)  | (((x) & 0x0200) >> 3)  \
			| (((x) & 0x0400) >> 5)  | (((x) & 0x0800) >> 7)  \
			| (((x) & 0x1000) >> 9)  | (((x) & 0x2000) >> 11) \
			| (((x) & 0x4000) >> 13) | (((x) & 0x8000) >> 15) )

static int netdev_get_eeprom(struct net_device *dev, u8 *buf)
{
	int i;
	u16 *ebuf = (u16 *)buf;
	void __iomem * ioaddr = ns_ioaddr(dev);
	struct netdev_private *np = netdev_priv(dev);

	
	for (i = 0; i < np->eeprom_size/2; i++) {
		ebuf[i] = eeprom_read(ioaddr, i);
		ebuf[i] = SWAP_BITS(ebuf[i]);
	}
	return 0;
}

static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct mii_ioctl_data *data = if_mii(rq);
	struct netdev_private *np = netdev_priv(dev);

	switch(cmd) {
	case SIOCGMIIPHY:		
		data->phy_id = np->phy_addr_external;
		

	case SIOCGMIIREG:		
		if (dev->if_port == PORT_TP) {
			if ((data->phy_id & 0x1f) == np->phy_addr_external)
				data->val_out = mdio_read(dev,
							data->reg_num & 0x1f);
			else
				data->val_out = 0;
		} else {
			move_int_phy(dev, data->phy_id & 0x1f);
			data->val_out = miiport_read(dev, data->phy_id & 0x1f,
							data->reg_num & 0x1f);
		}
		return 0;

	case SIOCSMIIREG:		
		if (dev->if_port == PORT_TP) {
			if ((data->phy_id & 0x1f) == np->phy_addr_external) {
 				if ((data->reg_num & 0x1f) == MII_ADVERTISE)
					np->advertising = data->val_in;
				mdio_write(dev, data->reg_num & 0x1f,
							data->val_in);
			}
		} else {
			if ((data->phy_id & 0x1f) == np->phy_addr_external) {
 				if ((data->reg_num & 0x1f) == MII_ADVERTISE)
					np->advertising = data->val_in;
			}
			move_int_phy(dev, data->phy_id & 0x1f);
			miiport_write(dev, data->phy_id & 0x1f,
						data->reg_num & 0x1f,
						data->val_in);
		}
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static void enable_wol_mode(struct net_device *dev, int enable_intr)
{
	void __iomem * ioaddr = ns_ioaddr(dev);
	struct netdev_private *np = netdev_priv(dev);

	if (netif_msg_wol(np))
		printk(KERN_INFO "%s: remaining active for wake-on-lan\n",
			dev->name);

	writel(0, ioaddr + RxRingPtr);

	
	readl(ioaddr + WOLCmd);

	
	writel(np->SavedClkRun | PMEEnable | PMEStatus, ioaddr + ClkRun);

	
	writel(RxOn, ioaddr + ChipCmd);

	if (enable_intr) {
		writel(WOLPkt | LinkChange, ioaddr + IntrMask);
		natsemi_irq_enable(dev);
	}
}

static int netdev_close(struct net_device *dev)
{
	void __iomem * ioaddr = ns_ioaddr(dev);
	struct netdev_private *np = netdev_priv(dev);

	if (netif_msg_ifdown(np))
		printk(KERN_DEBUG
			"%s: Shutting down ethercard, status was %#04x.\n",
			dev->name, (int)readl(ioaddr + ChipCmd));
	if (netif_msg_pktdata(np))
		printk(KERN_DEBUG
			"%s: Queue pointers were Tx %d / %d,  Rx %d / %d.\n",
			dev->name, np->cur_tx, np->dirty_tx,
			np->cur_rx, np->dirty_rx);

	napi_disable(&np->napi);


	del_timer_sync(&np->timer);
	disable_irq(dev->irq);
	spin_lock_irq(&np->lock);
	natsemi_irq_disable(dev);
	np->hands_off = 1;
	spin_unlock_irq(&np->lock);
	enable_irq(dev->irq);

	free_irq(dev->irq, dev);

	spin_lock_irq(&np->lock);
	np->hands_off = 0;
	readl(ioaddr + IntrMask);
	readw(ioaddr + MIntrStatus);

	
	writel(StatsFreeze, ioaddr + StatsCtrl);

	
	natsemi_stop_rxtx(dev);

	__get_stats(dev);
	spin_unlock_irq(&np->lock);

	
	netif_carrier_off(dev);
	netif_stop_queue(dev);

	dump_ring(dev);
	drain_ring(dev);
	free_ring(dev);

	{
		u32 wol = readl(ioaddr + WOLCmd) & WakeOptsSummary;
		if (wol) {
			enable_wol_mode(dev, 0);
		} else {
			
			writel(np->SavedClkRun, ioaddr + ClkRun);
		}
	}
	return 0;
}


static void __devexit natsemi_remove1 (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	NATSEMI_REMOVE_FILE(pdev, dspcfg_workaround);
	unregister_netdev (dev);
	pci_release_regions (pdev);
	iounmap(ioaddr);
	free_netdev (dev);
	pci_set_drvdata(pdev, NULL);
}

#ifdef CONFIG_PM


static int natsemi_suspend (struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata (pdev);
	struct netdev_private *np = netdev_priv(dev);
	void __iomem * ioaddr = ns_ioaddr(dev);

	rtnl_lock();
	if (netif_running (dev)) {
		del_timer_sync(&np->timer);

		disable_irq(dev->irq);
		spin_lock_irq(&np->lock);

		natsemi_irq_disable(dev);
		np->hands_off = 1;
		natsemi_stop_rxtx(dev);
		netif_stop_queue(dev);

		spin_unlock_irq(&np->lock);
		enable_irq(dev->irq);

		napi_disable(&np->napi);

		
		__get_stats(dev);

		
		drain_ring(dev);
		{
			u32 wol = readl(ioaddr + WOLCmd) & WakeOptsSummary;
			
			if (wol) {
				enable_wol_mode(dev, 0);
			} else {
				
				writel(np->SavedClkRun, ioaddr + ClkRun);
			}
		}
	}
	netif_device_detach(dev);
	rtnl_unlock();
	return 0;
}


static int natsemi_resume (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata (pdev);
	struct netdev_private *np = netdev_priv(dev);
	int ret = 0;

	rtnl_lock();
	if (netif_device_present(dev))
		goto out;
	if (netif_running(dev)) {
		BUG_ON(!np->hands_off);
		ret = pci_enable_device(pdev);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"pci_enable_device() failed: %d\n", ret);
			goto out;
		}
	

		napi_enable(&np->napi);

		natsemi_reset(dev);
		init_ring(dev);
		disable_irq(dev->irq);
		spin_lock_irq(&np->lock);
		np->hands_off = 0;
		init_registers(dev);
		netif_device_attach(dev);
		spin_unlock_irq(&np->lock);
		enable_irq(dev->irq);

		mod_timer(&np->timer, round_jiffies(jiffies + 1*HZ));
	}
	netif_device_attach(dev);
out:
	rtnl_unlock();
	return ret;
}

#endif 

static struct pci_driver natsemi_driver = {
	.name		= DRV_NAME,
	.id_table	= natsemi_pci_tbl,
	.probe		= natsemi_probe1,
	.remove		= __devexit_p(natsemi_remove1),
#ifdef CONFIG_PM
	.suspend	= natsemi_suspend,
	.resume		= natsemi_resume,
#endif
};

static int __init natsemi_init_mod (void)
{
#ifdef MODULE
	printk(version);
#endif

	return pci_register_driver(&natsemi_driver);
}

static void __exit natsemi_exit_mod (void)
{
	pci_unregister_driver (&natsemi_driver);
}

module_init(natsemi_init_mod);
module_exit(natsemi_exit_mod);

