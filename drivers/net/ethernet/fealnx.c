/*
	Written 1998-2000 by Donald Becker.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Support information and updates available at
	http://www.scyld.com/network/pci-skeleton.html

	Linux kernel updates:

	Version 2.51, Nov 17, 2001 (jgarzik):
	- Add ethtool support
	- Replace some MII-related magic numbers with constants

*/

#define DRV_NAME	"fealnx"
#define DRV_VERSION	"2.52"
#define DRV_RELDATE	"Sep-11-2006"

static int debug;		
static int max_interrupt_work = 20;

static int multicast_filter_limit = 32;

static int rx_copybreak;

#define MAX_UNITS 8		
static int options[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static int full_duplex[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1 };

#define TX_RING_SIZE    6
#define RX_RING_SIZE    12
#define TX_TOTAL_SIZE	TX_RING_SIZE*sizeof(struct fealnx_desc)
#define RX_TOTAL_SIZE	RX_RING_SIZE*sizeof(struct fealnx_desc)

#define TX_TIMEOUT      (2*HZ)

#define PKT_BUF_SZ      1536	


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#include <asm/processor.h>	
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

static const char version[] __devinitconst =
	KERN_INFO DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE "\n";


/* This driver was written to use PCI memory space, however some x86 systems
   work only with I/O space accesses. */
#ifndef __alpha__
#define USE_IO_OPS
#endif


#define RUN_AT(x) (jiffies + (x))

MODULE_AUTHOR("Myson or whoever");
MODULE_DESCRIPTION("Myson MTD-8xx 100/10M Ethernet PCI Adapter Driver");
MODULE_LICENSE("GPL");
module_param(max_interrupt_work, int, 0);
module_param(debug, int, 0);
module_param(rx_copybreak, int, 0);
module_param(multicast_filter_limit, int, 0);
module_param_array(options, int, NULL, 0);
module_param_array(full_duplex, int, NULL, 0);
MODULE_PARM_DESC(max_interrupt_work, "fealnx maximum events handled per interrupt");
MODULE_PARM_DESC(debug, "fealnx enable debugging (0-1)");
MODULE_PARM_DESC(rx_copybreak, "fealnx copy breakpoint for copy-only-tiny-frames");
MODULE_PARM_DESC(multicast_filter_limit, "fealnx maximum number of filtered multicast addresses");
MODULE_PARM_DESC(options, "fealnx: Bits 0-3: media type, bit 17: full duplex");
MODULE_PARM_DESC(full_duplex, "fealnx full duplex setting(s) (1)");

enum {
	MIN_REGION_SIZE		= 136,
};

enum chip_capability_flags {
	HAS_MII_XCVR,
	HAS_CHIP_XCVR,
};

enum phy_type_flags {
	MysonPHY = 1,
	AhdocPHY = 2,
	SeeqPHY = 3,
	MarvellPHY = 4,
	Myson981 = 5,
	LevelOnePHY = 6,
	OtherPHY = 10,
};

struct chip_info {
	char *chip_name;
	int flags;
};

static const struct chip_info skel_netdrv_tbl[] __devinitdata = {
 	{ "100/10M Ethernet PCI Adapter",	HAS_MII_XCVR },
	{ "100/10M Ethernet PCI Adapter",	HAS_CHIP_XCVR },
	{ "1000/100/10M Ethernet PCI Adapter",	HAS_MII_XCVR },
};

enum fealnx_offsets {
	PAR0 = 0x0,		
	PAR1 = 0x04,		
	MAR0 = 0x08,		
	MAR1 = 0x0C,		
	FAR0 = 0x10,		
	FAR1 = 0x14,		
	TCRRCR = 0x18,		
	BCR = 0x1C,		
	TXPDR = 0x20,		
	RXPDR = 0x24,		
	RXCWP = 0x28,		
	TXLBA = 0x2C,		
	RXLBA = 0x30,		
	ISR = 0x34,		
	IMR = 0x38,		
	FTH = 0x3C,		
	MANAGEMENT = 0x40,	
	TALLY = 0x44,		
	TSR = 0x48,		
	BMCRSR = 0x4c,		
	PHYIDENTIFIER = 0x50,	
	ANARANLPAR = 0x54,	
	ANEROCR = 0x58,		
	BPREMRPSR = 0x5c,	
};

enum intr_status_bits {
	RFCON = 0x00020000,	
	RFCOFF = 0x00010000,	
	LSCStatus = 0x00008000,	
	ANCStatus = 0x00004000,	
	FBE = 0x00002000,	
	FBEMask = 0x00001800,	
	ParityErr = 0x00000000,	
	TargetErr = 0x00001000,	
	MasterErr = 0x00000800,	
	TUNF = 0x00000400,	
	ROVF = 0x00000200,	
	ETI = 0x00000100,	
	ERI = 0x00000080,	
	CNTOVF = 0x00000040,	
	RBU = 0x00000020,	
	TBU = 0x00000010,	
	TI = 0x00000008,	
	RI = 0x00000004,	
	RxErr = 0x00000002,	
};

enum rx_mode_bits {
	CR_W_ENH	= 0x02000000,	
	CR_W_FD		= 0x00100000,	
	CR_W_PS10	= 0x00080000,	
	CR_W_TXEN	= 0x00040000,	
	CR_W_PS1000	= 0x00010000,	
     
	CR_W_RXMODEMASK	= 0x000000e0,
	CR_W_PROM	= 0x00000080,	
	CR_W_AB		= 0x00000040,	
	CR_W_AM		= 0x00000020,	
	CR_W_ARP	= 0x00000008,	
	CR_W_ALP	= 0x00000004,	
	CR_W_SEP	= 0x00000002,	
	CR_W_RXEN	= 0x00000001,	

	CR_R_TXSTOP	= 0x04000000,	
	CR_R_FD		= 0x00100000,	
	CR_R_PS10	= 0x00080000,	
	CR_R_RXSTOP	= 0x00008000,	
};

struct fealnx_desc {
	s32 status;
	s32 control;
	u32 buffer;
	u32 next_desc;
	struct fealnx_desc *next_desc_logical;
	struct sk_buff *skbuff;
	u32 reserved1;
	u32 reserved2;
};

enum rx_desc_status_bits {
	RXOWN = 0x80000000,	
	FLNGMASK = 0x0fff0000,	
	FLNGShift = 16,
	MARSTATUS = 0x00004000,	
	BARSTATUS = 0x00002000,	
	PHYSTATUS = 0x00001000,	
	RXFSD = 0x00000800,	
	RXLSD = 0x00000400,	
	ErrorSummary = 0x80,	
	RUNT = 0x40,		
	LONG = 0x20,		
	FAE = 0x10,		
	CRC = 0x08,		
	RXER = 0x04,		
};

enum rx_desc_control_bits {
	RXIC = 0x00800000,	
	RBSShift = 0,
};

enum tx_desc_status_bits {
	TXOWN = 0x80000000,	
	JABTO = 0x00004000,	
	CSL = 0x00002000,	
	LC = 0x00001000,	
	EC = 0x00000800,	
	UDF = 0x00000400,	
	DFR = 0x00000200,	
	HF = 0x00000100,	
	NCRMask = 0x000000ff,	
	NCRShift = 0,
};

enum tx_desc_control_bits {
	TXIC = 0x80000000,	
	ETIControl = 0x40000000,	
	TXLD = 0x20000000,	
	TXFD = 0x10000000,	
	CRCEnable = 0x08000000,	
	PADEnable = 0x04000000,	
	RetryTxLC = 0x02000000,	
	PKTSMask = 0x3ff800,	
	PKTSShift = 11,
	TBSMask = 0x000007ff,	
	TBSShift = 0,
};

#define MASK_MIIR_MII_READ       0x00000000
#define MASK_MIIR_MII_WRITE      0x00000008
#define MASK_MIIR_MII_MDO        0x00000004
#define MASK_MIIR_MII_MDI        0x00000002
#define MASK_MIIR_MII_MDC        0x00000001

#define OP_READ             0x6000	
#define OP_WRITE            0x5002	

#define MysonPHYID      0xd0000302
#define MysonPHYID0     0x0302
#define StatusRegister  18
#define SPEED100        0x0400	
#define FULLMODE        0x0800	

#define SeeqPHYID0      0x0016

#define MIIRegister18   18
#define SPD_DET_100     0x80
#define DPLX_DET_FULL   0x40

#define AhdocPHYID0     0x0022

#define DiagnosticReg   18
#define DPLX_FULL       0x0800
#define Speed_100       0x0400

#define MarvellPHYID0           0x0141
#define LevelOnePHYID0		0x0013

#define MII1000BaseTControlReg  9
#define MII1000BaseTStatusReg   10
#define SpecificReg		17

#define PHYAbletoPerform1000FullDuplex  0x0200
#define PHYAbletoPerform1000HalfDuplex  0x0100
#define PHY1000AbilityMask              0x300

#define SpeedMask       0x0c000
#define Speed_1000M     0x08000
#define Speed_100M      0x4000
#define Speed_10M       0
#define Full_Duplex     0x2000

#define LXT1000_100M    0x08000
#define LXT1000_1000M   0x0c000
#define LXT1000_Full    0x200

#define LinkIsUp2	0x00040000

#define LinkIsUp        0x0004


struct netdev_private {
	
	struct fealnx_desc *rx_ring;
	struct fealnx_desc *tx_ring;

	dma_addr_t rx_ring_dma;
	dma_addr_t tx_ring_dma;

	spinlock_t lock;

	
	struct timer_list timer;

	
	struct timer_list reset_timer;
	int reset_timer_armed;
	unsigned long crvalue_sv;
	unsigned long imrvalue_sv;

	
	int flags;
	struct pci_dev *pci_dev;
	unsigned long crvalue;
	unsigned long bcrvalue;
	unsigned long imrvalue;
	struct fealnx_desc *cur_rx;
	struct fealnx_desc *lack_rxbuf;
	int really_rx_count;
	struct fealnx_desc *cur_tx;
	struct fealnx_desc *cur_tx_copy;
	int really_tx_count;
	int free_tx_count;
	unsigned int rx_buf_sz;	

	
	unsigned int linkok;
	unsigned int line_speed;
	unsigned int duplexmode;
	unsigned int default_port:4;	
	unsigned int PHYType;

	
	int mii_cnt;		
	unsigned char phys[2];	
	struct mii_if_info mii;
	void __iomem *mem;
};


static int mdio_read(struct net_device *dev, int phy_id, int location);
static void mdio_write(struct net_device *dev, int phy_id, int location, int value);
static int netdev_open(struct net_device *dev);
static void getlinktype(struct net_device *dev);
static void getlinkstatus(struct net_device *dev);
static void netdev_timer(unsigned long data);
static void reset_timer(unsigned long data);
static void fealnx_tx_timeout(struct net_device *dev);
static void init_ring(struct net_device *dev);
static netdev_tx_t start_tx(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t intr_handler(int irq, void *dev_instance);
static int netdev_rx(struct net_device *dev);
static void set_rx_mode(struct net_device *dev);
static void __set_rx_mode(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static const struct ethtool_ops netdev_ethtool_ops;
static int netdev_close(struct net_device *dev);
static void reset_rx_descriptors(struct net_device *dev);
static void reset_tx_descriptors(struct net_device *dev);

static void stop_nic_rx(void __iomem *ioaddr, long crvalue)
{
	int delay = 0x1000;
	iowrite32(crvalue & ~(CR_W_RXEN), ioaddr + TCRRCR);
	while (--delay) {
		if ( (ioread32(ioaddr + TCRRCR) & CR_R_RXSTOP) == CR_R_RXSTOP)
			break;
	}
}


static void stop_nic_rxtx(void __iomem *ioaddr, long crvalue)
{
	int delay = 0x1000;
	iowrite32(crvalue & ~(CR_W_RXEN+CR_W_TXEN), ioaddr + TCRRCR);
	while (--delay) {
		if ( (ioread32(ioaddr + TCRRCR) & (CR_R_RXSTOP+CR_R_TXSTOP))
					    == (CR_R_RXSTOP+CR_R_TXSTOP) )
			break;
	}
}

static const struct net_device_ops netdev_ops = {
	.ndo_open		= netdev_open,
	.ndo_stop		= netdev_close,
	.ndo_start_xmit		= start_tx,
	.ndo_get_stats 		= get_stats,
	.ndo_set_rx_mode	= set_rx_mode,
	.ndo_do_ioctl		= mii_ioctl,
	.ndo_tx_timeout		= fealnx_tx_timeout,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static int __devinit fealnx_init_one(struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	struct netdev_private *np;
	int i, option, err, irq;
	static int card_idx = -1;
	char boardname[12];
	void __iomem *ioaddr;
	unsigned long len;
	unsigned int chip_id = ent->driver_data;
	struct net_device *dev;
	void *ring_space;
	dma_addr_t ring_dma;
#ifdef USE_IO_OPS
	int bar = 0;
#else
	int bar = 1;
#endif

#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	card_idx++;
	sprintf(boardname, "fealnx%d", card_idx);

	option = card_idx < MAX_UNITS ? options[card_idx] : 0;

	i = pci_enable_device(pdev);
	if (i) return i;
	pci_set_master(pdev);

	len = pci_resource_len(pdev, bar);
	if (len < MIN_REGION_SIZE) {
		dev_err(&pdev->dev,
			   "region size %ld too small, aborting\n", len);
		return -ENODEV;
	}

	i = pci_request_regions(pdev, boardname);
	if (i)
		return i;

	irq = pdev->irq;

	ioaddr = pci_iomap(pdev, bar, len);
	if (!ioaddr) {
		err = -ENOMEM;
		goto err_out_res;
	}

	dev = alloc_etherdev(sizeof(struct netdev_private));
	if (!dev) {
		err = -ENOMEM;
		goto err_out_unmap;
	}
	SET_NETDEV_DEV(dev, &pdev->dev);

	
	for (i = 0; i < 6; ++i)
		dev->dev_addr[i] = ioread8(ioaddr + PAR0 + i);

	
	iowrite32(0x00000001, ioaddr + BCR);

	dev->base_addr = (unsigned long)ioaddr;
	dev->irq = irq;

	
	np = netdev_priv(dev);
	np->mem = ioaddr;
	spin_lock_init(&np->lock);
	np->pci_dev = pdev;
	np->flags = skel_netdrv_tbl[chip_id].flags;
	pci_set_drvdata(pdev, dev);
	np->mii.dev = dev;
	np->mii.mdio_read = mdio_read;
	np->mii.mdio_write = mdio_write;
	np->mii.phy_id_mask = 0x1f;
	np->mii.reg_num_mask = 0x1f;

	ring_space = pci_alloc_consistent(pdev, RX_TOTAL_SIZE, &ring_dma);
	if (!ring_space) {
		err = -ENOMEM;
		goto err_out_free_dev;
	}
	np->rx_ring = ring_space;
	np->rx_ring_dma = ring_dma;

	ring_space = pci_alloc_consistent(pdev, TX_TOTAL_SIZE, &ring_dma);
	if (!ring_space) {
		err = -ENOMEM;
		goto err_out_free_rx;
	}
	np->tx_ring = ring_space;
	np->tx_ring_dma = ring_dma;

	
	if (np->flags == HAS_MII_XCVR) {
		int phy, phy_idx = 0;

		for (phy = 1; phy < 32 && phy_idx < ARRAY_SIZE(np->phys);
			       phy++) {
			int mii_status = mdio_read(dev, phy, 1);

			if (mii_status != 0xffff && mii_status != 0x0000) {
				np->phys[phy_idx++] = phy;
				dev_info(&pdev->dev,
				       "MII PHY found at address %d, status "
				       "0x%4.4x.\n", phy, mii_status);
				
				{
					unsigned int data;

					data = mdio_read(dev, np->phys[0], 2);
					if (data == SeeqPHYID0)
						np->PHYType = SeeqPHY;
					else if (data == AhdocPHYID0)
						np->PHYType = AhdocPHY;
					else if (data == MarvellPHYID0)
						np->PHYType = MarvellPHY;
					else if (data == MysonPHYID0)
						np->PHYType = Myson981;
					else if (data == LevelOnePHYID0)
						np->PHYType = LevelOnePHY;
					else
						np->PHYType = OtherPHY;
				}
			}
		}

		np->mii_cnt = phy_idx;
		if (phy_idx == 0)
			dev_warn(&pdev->dev,
				"MII PHY not found -- this device may "
			       "not operate correctly.\n");
	} else {
		np->phys[0] = 32;
		
		if (ioread32(ioaddr + PHYIDENTIFIER) == MysonPHYID)
			np->PHYType = MysonPHY;
		else
			np->PHYType = OtherPHY;
	}
	np->mii.phy_id = np->phys[0];

	if (dev->mem_start)
		option = dev->mem_start;

	
	if (option > 0) {
		if (option & 0x200)
			np->mii.full_duplex = 1;
		np->default_port = option & 15;
	}

	if (card_idx < MAX_UNITS && full_duplex[card_idx] > 0)
		np->mii.full_duplex = full_duplex[card_idx];

	if (np->mii.full_duplex) {
		dev_info(&pdev->dev, "Media type forced to Full Duplex.\n");
		if ((np->PHYType == MarvellPHY) || (np->PHYType == LevelOnePHY)) {
			unsigned int data;

			data = mdio_read(dev, np->phys[0], 9);
			data = (data & 0xfcff) | 0x0200;
			mdio_write(dev, np->phys[0], 9, data);
		}
		if (np->flags == HAS_MII_XCVR)
			mdio_write(dev, np->phys[0], MII_ADVERTISE, ADVERTISE_FULL);
		else
			iowrite32(ADVERTISE_FULL, ioaddr + ANARANLPAR);
		np->mii.force_media = 1;
	}

	dev->netdev_ops = &netdev_ops;
	dev->ethtool_ops = &netdev_ethtool_ops;
	dev->watchdog_timeo = TX_TIMEOUT;

	err = register_netdev(dev);
	if (err)
		goto err_out_free_tx;

	printk(KERN_INFO "%s: %s at %p, %pM, IRQ %d.\n",
	       dev->name, skel_netdrv_tbl[chip_id].chip_name, ioaddr,
	       dev->dev_addr, irq);

	return 0;

err_out_free_tx:
	pci_free_consistent(pdev, TX_TOTAL_SIZE, np->tx_ring, np->tx_ring_dma);
err_out_free_rx:
	pci_free_consistent(pdev, RX_TOTAL_SIZE, np->rx_ring, np->rx_ring_dma);
err_out_free_dev:
	free_netdev(dev);
err_out_unmap:
	pci_iounmap(pdev, ioaddr);
err_out_res:
	pci_release_regions(pdev);
	return err;
}


static void __devexit fealnx_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (dev) {
		struct netdev_private *np = netdev_priv(dev);

		pci_free_consistent(pdev, TX_TOTAL_SIZE, np->tx_ring,
			np->tx_ring_dma);
		pci_free_consistent(pdev, RX_TOTAL_SIZE, np->rx_ring,
			np->rx_ring_dma);
		unregister_netdev(dev);
		pci_iounmap(pdev, np->mem);
		free_netdev(dev);
		pci_release_regions(pdev);
		pci_set_drvdata(pdev, NULL);
	} else
		printk(KERN_ERR "fealnx: remove for unknown device\n");
}


static ulong m80x_send_cmd_to_phy(void __iomem *miiport, int opcode, int phyad, int regad)
{
	ulong miir;
	int i;
	unsigned int mask, data;

	
	miir = (ulong) ioread32(miiport);
	miir &= 0xfffffff0;

	miir |= MASK_MIIR_MII_WRITE + MASK_MIIR_MII_MDO;

	
	for (i = 0; i < 32; i++) {
		
		miir &= ~MASK_MIIR_MII_MDC;
		iowrite32(miir, miiport);

		
		miir |= MASK_MIIR_MII_MDC;
		iowrite32(miir, miiport);
	}

	
	data = opcode | (phyad << 7) | (regad << 2);

	
	mask = 0x8000;
	while (mask) {
		
		miir &= ~(MASK_MIIR_MII_MDC + MASK_MIIR_MII_MDO);
		if (mask & data)
			miir |= MASK_MIIR_MII_MDO;

		iowrite32(miir, miiport);
		
		miir |= MASK_MIIR_MII_MDC;
		iowrite32(miir, miiport);
		udelay(30);

		
		mask >>= 1;
		if (mask == 0x2 && opcode == OP_READ)
			miir &= ~MASK_MIIR_MII_WRITE;
	}
	return miir;
}


static int mdio_read(struct net_device *dev, int phyad, int regad)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *miiport = np->mem + MANAGEMENT;
	ulong miir;
	unsigned int mask, data;

	miir = m80x_send_cmd_to_phy(miiport, OP_READ, phyad, regad);

	
	mask = 0x8000;
	data = 0;
	while (mask) {
		
		miir &= ~MASK_MIIR_MII_MDC;
		iowrite32(miir, miiport);

		
		miir = ioread32(miiport);
		if (miir & MASK_MIIR_MII_MDI)
			data |= mask;

		
		miir |= MASK_MIIR_MII_MDC;
		iowrite32(miir, miiport);
		udelay(30);

		
		mask >>= 1;
	}

	
	miir &= ~MASK_MIIR_MII_MDC;
	iowrite32(miir, miiport);

	return data & 0xffff;
}


static void mdio_write(struct net_device *dev, int phyad, int regad, int data)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *miiport = np->mem + MANAGEMENT;
	ulong miir;
	unsigned int mask;

	miir = m80x_send_cmd_to_phy(miiport, OP_WRITE, phyad, regad);

	
	mask = 0x8000;
	while (mask) {
		
		miir &= ~(MASK_MIIR_MII_MDC + MASK_MIIR_MII_MDO);
		if (mask & data)
			miir |= MASK_MIIR_MII_MDO;
		iowrite32(miir, miiport);

		
		miir |= MASK_MIIR_MII_MDC;
		iowrite32(miir, miiport);

		
		mask >>= 1;
	}

	
	miir &= ~MASK_MIIR_MII_MDC;
	iowrite32(miir, miiport);
}


static int netdev_open(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	int i;

	iowrite32(0x00000001, ioaddr + BCR);	

	if (request_irq(dev->irq, intr_handler, IRQF_SHARED, dev->name, dev))
		return -EAGAIN;

	for (i = 0; i < 3; i++)
		iowrite16(((unsigned short*)dev->dev_addr)[i],
				ioaddr + PAR0 + i*2);

	init_ring(dev);

	iowrite32(np->rx_ring_dma, ioaddr + RXLBA);
	iowrite32(np->tx_ring_dma, ioaddr + TXLBA);

	

	np->bcrvalue = 0x10;	
#ifdef __BIG_ENDIAN
	np->bcrvalue |= 0x04;	
#endif

#if defined(__i386__) && !defined(MODULE)
	if (boot_cpu_data.x86 <= 4)
		np->crvalue = 0xa00;
	else
#endif
		np->crvalue = 0xe00;	


	np->imrvalue = TUNF | CNTOVF | RBU | TI | RI;
	if (np->pci_dev->device == 0x891) {
		np->bcrvalue |= 0x200;	
		np->crvalue |= CR_W_ENH;	
		np->imrvalue |= ETI;
	}
	iowrite32(np->bcrvalue, ioaddr + BCR);

	if (dev->if_port == 0)
		dev->if_port = np->default_port;

	iowrite32(0, ioaddr + RXPDR);
	np->crvalue |= 0x00e40001;	
	np->mii.full_duplex = np->mii.force_media;
	getlinkstatus(dev);
	if (np->linkok)
		getlinktype(dev);
	__set_rx_mode(dev);

	netif_start_queue(dev);

	
	iowrite32(FBE | TUNF | CNTOVF | RBU | TI | RI, ioaddr + ISR);
	iowrite32(np->imrvalue, ioaddr + IMR);

	if (debug)
		printk(KERN_DEBUG "%s: Done netdev_open().\n", dev->name);

	
	init_timer(&np->timer);
	np->timer.expires = RUN_AT(3 * HZ);
	np->timer.data = (unsigned long) dev;
	np->timer.function = netdev_timer;

	
	add_timer(&np->timer);

	init_timer(&np->reset_timer);
	np->reset_timer.data = (unsigned long) dev;
	np->reset_timer.function = reset_timer;
	np->reset_timer_armed = 0;

	return 0;
}


static void getlinkstatus(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	unsigned int i, DelayTime = 0x1000;

	np->linkok = 0;

	if (np->PHYType == MysonPHY) {
		for (i = 0; i < DelayTime; ++i) {
			if (ioread32(np->mem + BMCRSR) & LinkIsUp2) {
				np->linkok = 1;
				return;
			}
			udelay(100);
		}
	} else {
		for (i = 0; i < DelayTime; ++i) {
			if (mdio_read(dev, np->phys[0], MII_BMSR) & BMSR_LSTATUS) {
				np->linkok = 1;
				return;
			}
			udelay(100);
		}
	}
}


static void getlinktype(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);

	if (np->PHYType == MysonPHY) {	
		if (ioread32(np->mem + TCRRCR) & CR_R_FD)
			np->duplexmode = 2;	
		else
			np->duplexmode = 1;	
		if (ioread32(np->mem + TCRRCR) & CR_R_PS10)
			np->line_speed = 1;	
		else
			np->line_speed = 2;	
	} else {
		if (np->PHYType == SeeqPHY) {	
			unsigned int data;

			data = mdio_read(dev, np->phys[0], MIIRegister18);
			if (data & SPD_DET_100)
				np->line_speed = 2;	
			else
				np->line_speed = 1;	
			if (data & DPLX_DET_FULL)
				np->duplexmode = 2;	
			else
				np->duplexmode = 1;	
		} else if (np->PHYType == AhdocPHY) {
			unsigned int data;

			data = mdio_read(dev, np->phys[0], DiagnosticReg);
			if (data & Speed_100)
				np->line_speed = 2;	
			else
				np->line_speed = 1;	
			if (data & DPLX_FULL)
				np->duplexmode = 2;	
			else
				np->duplexmode = 1;	
		}
		else if (np->PHYType == MarvellPHY) {
			unsigned int data;

			data = mdio_read(dev, np->phys[0], SpecificReg);
			if (data & Full_Duplex)
				np->duplexmode = 2;	
			else
				np->duplexmode = 1;	
			data &= SpeedMask;
			if (data == Speed_1000M)
				np->line_speed = 3;	
			else if (data == Speed_100M)
				np->line_speed = 2;	
			else
				np->line_speed = 1;	
		}
		else if (np->PHYType == Myson981) {
			unsigned int data;

			data = mdio_read(dev, np->phys[0], StatusRegister);

			if (data & SPEED100)
				np->line_speed = 2;
			else
				np->line_speed = 1;

			if (data & FULLMODE)
				np->duplexmode = 2;
			else
				np->duplexmode = 1;
		}
		else if (np->PHYType == LevelOnePHY) {
			unsigned int data;

			data = mdio_read(dev, np->phys[0], SpecificReg);
			if (data & LXT1000_Full)
				np->duplexmode = 2;	
			else
				np->duplexmode = 1;	
			data &= SpeedMask;
			if (data == LXT1000_1000M)
				np->line_speed = 3;	
			else if (data == LXT1000_100M)
				np->line_speed = 2;	
			else
				np->line_speed = 1;	
		}
		np->crvalue &= (~CR_W_PS10) & (~CR_W_FD) & (~CR_W_PS1000);
		if (np->line_speed == 1)
			np->crvalue |= CR_W_PS10;
		else if (np->line_speed == 3)
			np->crvalue |= CR_W_PS1000;
		if (np->duplexmode == 2)
			np->crvalue |= CR_W_FD;
	}
}


static void allocate_rx_buffers(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);

	
	while (np->really_rx_count != RX_RING_SIZE) {
		struct sk_buff *skb;

		skb = netdev_alloc_skb(dev, np->rx_buf_sz);
		if (skb == NULL)
			break;	

		while (np->lack_rxbuf->skbuff)
			np->lack_rxbuf = np->lack_rxbuf->next_desc_logical;

		np->lack_rxbuf->skbuff = skb;
		np->lack_rxbuf->buffer = pci_map_single(np->pci_dev, skb->data,
			np->rx_buf_sz, PCI_DMA_FROMDEVICE);
		np->lack_rxbuf->status = RXOWN;
		++np->really_rx_count;
	}
}


static void netdev_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	int old_crvalue = np->crvalue;
	unsigned int old_linkok = np->linkok;
	unsigned long flags;

	if (debug)
		printk(KERN_DEBUG "%s: Media selection timer tick, status %8.8x "
		       "config %8.8x.\n", dev->name, ioread32(ioaddr + ISR),
		       ioread32(ioaddr + TCRRCR));

	spin_lock_irqsave(&np->lock, flags);

	if (np->flags == HAS_MII_XCVR) {
		getlinkstatus(dev);
		if ((old_linkok == 0) && (np->linkok == 1)) {	
			getlinktype(dev);
			if (np->crvalue != old_crvalue) {
				stop_nic_rxtx(ioaddr, np->crvalue);
				iowrite32(np->crvalue, ioaddr + TCRRCR);
			}
		}
	}

	allocate_rx_buffers(dev);

	spin_unlock_irqrestore(&np->lock, flags);

	np->timer.expires = RUN_AT(10 * HZ);
	add_timer(&np->timer);
}


static void reset_and_disable_rxtx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	int delay=51;

	
	stop_nic_rxtx(ioaddr, 0);

	
	iowrite32(0, ioaddr + IMR);

	
	iowrite32(0x00000001, ioaddr + BCR);

	while (--delay) {
		ioread32(ioaddr + BCR);
		rmb();
	}
}


static void enable_rxtx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;

	reset_rx_descriptors(dev);

	iowrite32(np->tx_ring_dma + ((char*)np->cur_tx - (char*)np->tx_ring),
		ioaddr + TXLBA);
	iowrite32(np->rx_ring_dma + ((char*)np->cur_rx - (char*)np->rx_ring),
		ioaddr + RXLBA);

	iowrite32(np->bcrvalue, ioaddr + BCR);

	iowrite32(0, ioaddr + RXPDR);
	__set_rx_mode(dev); 

	
	iowrite32(FBE | TUNF | CNTOVF | RBU | TI | RI, ioaddr + ISR);
	iowrite32(np->imrvalue, ioaddr + IMR);

	iowrite32(0, ioaddr + TXPDR);
}


static void reset_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	struct netdev_private *np = netdev_priv(dev);
	unsigned long flags;

	printk(KERN_WARNING "%s: resetting tx and rx machinery\n", dev->name);

	spin_lock_irqsave(&np->lock, flags);
	np->crvalue = np->crvalue_sv;
	np->imrvalue = np->imrvalue_sv;

	reset_and_disable_rxtx(dev);
	enable_rxtx(dev);
	netif_start_queue(dev); 

	np->reset_timer_armed = 0;

	spin_unlock_irqrestore(&np->lock, flags);
}


static void fealnx_tx_timeout(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	unsigned long flags;
	int i;

	printk(KERN_WARNING
	       "%s: Transmit timed out, status %8.8x, resetting...\n",
	       dev->name, ioread32(ioaddr + ISR));

	{
		printk(KERN_DEBUG "  Rx ring %p: ", np->rx_ring);
		for (i = 0; i < RX_RING_SIZE; i++)
			printk(KERN_CONT " %8.8x",
			       (unsigned int) np->rx_ring[i].status);
		printk(KERN_CONT "\n");
		printk(KERN_DEBUG "  Tx ring %p: ", np->tx_ring);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(KERN_CONT " %4.4x", np->tx_ring[i].status);
		printk(KERN_CONT "\n");
	}

	spin_lock_irqsave(&np->lock, flags);

	reset_and_disable_rxtx(dev);
	reset_tx_descriptors(dev);
	enable_rxtx(dev);

	spin_unlock_irqrestore(&np->lock, flags);

	dev->trans_start = jiffies; 
	dev->stats.tx_errors++;
	netif_wake_queue(dev); 
}


static void init_ring(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	int i;

	
	np->rx_buf_sz = (dev->mtu <= 1500 ? PKT_BUF_SZ : dev->mtu + 32);
	np->cur_rx = &np->rx_ring[0];
	np->lack_rxbuf = np->rx_ring;
	np->really_rx_count = 0;

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].status = 0;
		np->rx_ring[i].control = np->rx_buf_sz << RBSShift;
		np->rx_ring[i].next_desc = np->rx_ring_dma +
			(i + 1)*sizeof(struct fealnx_desc);
		np->rx_ring[i].next_desc_logical = &np->rx_ring[i + 1];
		np->rx_ring[i].skbuff = NULL;
	}

	
	np->rx_ring[i - 1].next_desc = np->rx_ring_dma;
	np->rx_ring[i - 1].next_desc_logical = np->rx_ring;

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = netdev_alloc_skb(dev, np->rx_buf_sz);

		if (skb == NULL) {
			np->lack_rxbuf = &np->rx_ring[i];
			break;
		}

		++np->really_rx_count;
		np->rx_ring[i].skbuff = skb;
		np->rx_ring[i].buffer = pci_map_single(np->pci_dev, skb->data,
			np->rx_buf_sz, PCI_DMA_FROMDEVICE);
		np->rx_ring[i].status = RXOWN;
		np->rx_ring[i].control |= RXIC;
	}

	
	np->cur_tx = &np->tx_ring[0];
	np->cur_tx_copy = &np->tx_ring[0];
	np->really_tx_count = 0;
	np->free_tx_count = TX_RING_SIZE;

	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_ring[i].status = 0;
		
		np->tx_ring[i].next_desc = np->tx_ring_dma +
			(i + 1)*sizeof(struct fealnx_desc);
		np->tx_ring[i].next_desc_logical = &np->tx_ring[i + 1];
		np->tx_ring[i].skbuff = NULL;
	}

	
	np->tx_ring[i - 1].next_desc = np->tx_ring_dma;
	np->tx_ring[i - 1].next_desc_logical = &np->tx_ring[0];
}


static netdev_tx_t start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&np->lock, flags);

	np->cur_tx_copy->skbuff = skb;

#define one_buffer
#define BPT 1022
#if defined(one_buffer)
	np->cur_tx_copy->buffer = pci_map_single(np->pci_dev, skb->data,
		skb->len, PCI_DMA_TODEVICE);
	np->cur_tx_copy->control = TXIC | TXLD | TXFD | CRCEnable | PADEnable;
	np->cur_tx_copy->control |= (skb->len << PKTSShift);	
	np->cur_tx_copy->control |= (skb->len << TBSShift);	
	if (np->pci_dev->device == 0x891)
		np->cur_tx_copy->control |= ETIControl | RetryTxLC;
	np->cur_tx_copy->status = TXOWN;
	np->cur_tx_copy = np->cur_tx_copy->next_desc_logical;
	--np->free_tx_count;
#elif defined(two_buffer)
	if (skb->len > BPT) {
		struct fealnx_desc *next;

		
		np->cur_tx_copy->buffer = pci_map_single(np->pci_dev, skb->data,
			BPT, PCI_DMA_TODEVICE);
		np->cur_tx_copy->control = TXIC | TXFD | CRCEnable | PADEnable;
		np->cur_tx_copy->control |= (skb->len << PKTSShift);	
		np->cur_tx_copy->control |= (BPT << TBSShift);	

		
		next = np->cur_tx_copy->next_desc_logical;
		next->skbuff = skb;
		next->control = TXIC | TXLD | CRCEnable | PADEnable;
		next->control |= (skb->len << PKTSShift);	
		next->control |= ((skb->len - BPT) << TBSShift);	
		if (np->pci_dev->device == 0x891)
			np->cur_tx_copy->control |= ETIControl | RetryTxLC;
		next->buffer = pci_map_single(ep->pci_dev, skb->data + BPT,
                                skb->len - BPT, PCI_DMA_TODEVICE);

		next->status = TXOWN;
		np->cur_tx_copy->status = TXOWN;

		np->cur_tx_copy = next->next_desc_logical;
		np->free_tx_count -= 2;
	} else {
		np->cur_tx_copy->buffer = pci_map_single(np->pci_dev, skb->data,
			skb->len, PCI_DMA_TODEVICE);
		np->cur_tx_copy->control = TXIC | TXLD | TXFD | CRCEnable | PADEnable;
		np->cur_tx_copy->control |= (skb->len << PKTSShift);	
		np->cur_tx_copy->control |= (skb->len << TBSShift);	
		if (np->pci_dev->device == 0x891)
			np->cur_tx_copy->control |= ETIControl | RetryTxLC;
		np->cur_tx_copy->status = TXOWN;
		np->cur_tx_copy = np->cur_tx_copy->next_desc_logical;
		--np->free_tx_count;
	}
#endif

	if (np->free_tx_count < 2)
		netif_stop_queue(dev);
	++np->really_tx_count;
	iowrite32(0, np->mem + TXPDR);

	spin_unlock_irqrestore(&np->lock, flags);
	return NETDEV_TX_OK;
}


static void reset_tx_descriptors(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	struct fealnx_desc *cur;
	int i;

	
	np->cur_tx = &np->tx_ring[0];
	np->cur_tx_copy = &np->tx_ring[0];
	np->really_tx_count = 0;
	np->free_tx_count = TX_RING_SIZE;

	for (i = 0; i < TX_RING_SIZE; i++) {
		cur = &np->tx_ring[i];
		if (cur->skbuff) {
			pci_unmap_single(np->pci_dev, cur->buffer,
				cur->skbuff->len, PCI_DMA_TODEVICE);
			dev_kfree_skb_any(cur->skbuff);
			cur->skbuff = NULL;
		}
		cur->status = 0;
		cur->control = 0;	
		
		cur->next_desc = np->tx_ring_dma +
			(i + 1)*sizeof(struct fealnx_desc);
		cur->next_desc_logical = &np->tx_ring[i + 1];
	}
	
	np->tx_ring[TX_RING_SIZE - 1].next_desc = np->tx_ring_dma;
	np->tx_ring[TX_RING_SIZE - 1].next_desc_logical = &np->tx_ring[0];
}


static void reset_rx_descriptors(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	struct fealnx_desc *cur = np->cur_rx;
	int i;

	allocate_rx_buffers(dev);

	for (i = 0; i < RX_RING_SIZE; i++) {
		if (cur->skbuff)
			cur->status = RXOWN;
		cur = cur->next_desc_logical;
	}

	iowrite32(np->rx_ring_dma + ((char*)np->cur_rx - (char*)np->rx_ring),
		np->mem + RXLBA);
}


static irqreturn_t intr_handler(int irq, void *dev_instance)
{
	struct net_device *dev = (struct net_device *) dev_instance;
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	long boguscnt = max_interrupt_work;
	unsigned int num_tx = 0;
	int handled = 0;

	spin_lock(&np->lock);

	iowrite32(0, ioaddr + IMR);

	do {
		u32 intr_status = ioread32(ioaddr + ISR);

		
		iowrite32(intr_status, ioaddr + ISR);

		if (debug)
			printk(KERN_DEBUG "%s: Interrupt, status %4.4x.\n", dev->name,
			       intr_status);

		if (!(intr_status & np->imrvalue))
			break;

		handled = 1;


		if (intr_status & TUNF)
			iowrite32(0, ioaddr + TXPDR);

		if (intr_status & CNTOVF) {
			
			dev->stats.rx_missed_errors +=
				ioread32(ioaddr + TALLY) & 0x7fff;

			
			dev->stats.rx_crc_errors +=
			    (ioread32(ioaddr + TALLY) & 0x7fff0000) >> 16;
		}

		if (intr_status & (RI | RBU)) {
			if (intr_status & RI)
				netdev_rx(dev);
			else {
				stop_nic_rx(ioaddr, np->crvalue);
				reset_rx_descriptors(dev);
				iowrite32(np->crvalue, ioaddr + TCRRCR);
			}
		}

		while (np->really_tx_count) {
			long tx_status = np->cur_tx->status;
			long tx_control = np->cur_tx->control;

			if (!(tx_control & TXLD)) {	
				struct fealnx_desc *next;

				next = np->cur_tx->next_desc_logical;
				tx_status = next->status;
				tx_control = next->control;
			}

			if (tx_status & TXOWN)
				break;

			if (!(np->crvalue & CR_W_ENH)) {
				if (tx_status & (CSL | LC | EC | UDF | HF)) {
					dev->stats.tx_errors++;
					if (tx_status & EC)
						dev->stats.tx_aborted_errors++;
					if (tx_status & CSL)
						dev->stats.tx_carrier_errors++;
					if (tx_status & LC)
						dev->stats.tx_window_errors++;
					if (tx_status & UDF)
						dev->stats.tx_fifo_errors++;
					if ((tx_status & HF) && np->mii.full_duplex == 0)
						dev->stats.tx_heartbeat_errors++;

				} else {
					dev->stats.tx_bytes +=
					    ((tx_control & PKTSMask) >> PKTSShift);

					dev->stats.collisions +=
					    ((tx_status & NCRMask) >> NCRShift);
					dev->stats.tx_packets++;
				}
			} else {
				dev->stats.tx_bytes +=
				    ((tx_control & PKTSMask) >> PKTSShift);
				dev->stats.tx_packets++;
			}

			
			pci_unmap_single(np->pci_dev, np->cur_tx->buffer,
				np->cur_tx->skbuff->len, PCI_DMA_TODEVICE);
			dev_kfree_skb_irq(np->cur_tx->skbuff);
			np->cur_tx->skbuff = NULL;
			--np->really_tx_count;
			if (np->cur_tx->control & TXLD) {
				np->cur_tx = np->cur_tx->next_desc_logical;
				++np->free_tx_count;
			} else {
				np->cur_tx = np->cur_tx->next_desc_logical;
				np->cur_tx = np->cur_tx->next_desc_logical;
				np->free_tx_count += 2;
			}
			num_tx++;
		}		

		if (num_tx && np->free_tx_count >= 2)
			netif_wake_queue(dev);

		
		if (np->crvalue & CR_W_ENH) {
			long data;

			data = ioread32(ioaddr + TSR);
			dev->stats.tx_errors += (data & 0xff000000) >> 24;
			dev->stats.tx_aborted_errors +=
				(data & 0xff000000) >> 24;
			dev->stats.tx_window_errors +=
				(data & 0x00ff0000) >> 16;
			dev->stats.collisions += (data & 0x0000ffff);
		}

		if (--boguscnt < 0) {
			printk(KERN_WARNING "%s: Too much work at interrupt, "
			       "status=0x%4.4x.\n", dev->name, intr_status);
			if (!np->reset_timer_armed) {
				np->reset_timer_armed = 1;
				np->reset_timer.expires = RUN_AT(HZ/2);
				add_timer(&np->reset_timer);
				stop_nic_rxtx(ioaddr, 0);
				netif_stop_queue(dev);
				
				
				np->crvalue_sv = np->crvalue;
				np->imrvalue_sv = np->imrvalue;
				np->crvalue &= ~(CR_W_TXEN | CR_W_RXEN); 
				np->imrvalue = 0;
			}

			break;
		}
	} while (1);

	
	
	dev->stats.rx_missed_errors += ioread32(ioaddr + TALLY) & 0x7fff;

	
	dev->stats.rx_crc_errors +=
		(ioread32(ioaddr + TALLY) & 0x7fff0000) >> 16;

	if (debug)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
		       dev->name, ioread32(ioaddr + ISR));

	iowrite32(np->imrvalue, ioaddr + IMR);

	spin_unlock(&np->lock);

	return IRQ_RETVAL(handled);
}


static int netdev_rx(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;

	
	while (!(np->cur_rx->status & RXOWN) && np->cur_rx->skbuff) {
		s32 rx_status = np->cur_rx->status;

		if (np->really_rx_count == 0)
			break;

		if (debug)
			printk(KERN_DEBUG "  netdev_rx() status was %8.8x.\n", rx_status);

		if ((!((rx_status & RXFSD) && (rx_status & RXLSD))) ||
		    (rx_status & ErrorSummary)) {
			if (rx_status & ErrorSummary) {	
				if (debug)
					printk(KERN_DEBUG
					       "%s: Receive error, Rx status %8.8x.\n",
					       dev->name, rx_status);

				dev->stats.rx_errors++;	
				if (rx_status & (LONG | RUNT))
					dev->stats.rx_length_errors++;
				if (rx_status & RXER)
					dev->stats.rx_frame_errors++;
				if (rx_status & CRC)
					dev->stats.rx_crc_errors++;
			} else {
				int need_to_reset = 0;
				int desno = 0;

				if (rx_status & RXFSD) {	
					struct fealnx_desc *cur;

					
					cur = np->cur_rx;
					while (desno <= np->really_rx_count) {
						++desno;
						if ((!(cur->status & RXOWN)) &&
						    (cur->status & RXLSD))
							break;
						
						cur = cur->next_desc_logical;
					}
					if (desno > np->really_rx_count)
						need_to_reset = 1;
				} else	
					need_to_reset = 1;

				if (need_to_reset == 0) {
					int i;

					dev->stats.rx_length_errors++;

					
					for (i = 0; i < desno; ++i) {
						if (!np->cur_rx->skbuff) {
							printk(KERN_DEBUG
								"%s: I'm scared\n", dev->name);
							break;
						}
						np->cur_rx->status = RXOWN;
						np->cur_rx = np->cur_rx->next_desc_logical;
					}
					continue;
				} else {        
					stop_nic_rx(ioaddr, np->crvalue);
					reset_rx_descriptors(dev);
					iowrite32(np->crvalue, ioaddr + TCRRCR);
				}
				break;	
			}
		} else {	

			struct sk_buff *skb;
			
			short pkt_len = ((rx_status & FLNGMASK) >> FLNGShift) - 4;

#ifndef final_version
			if (debug)
				printk(KERN_DEBUG "  netdev_rx() normal Rx pkt length %d"
				       " status %x.\n", pkt_len, rx_status);
#endif

			if (pkt_len < rx_copybreak &&
			    (skb = netdev_alloc_skb(dev, pkt_len + 2)) != NULL) {
				skb_reserve(skb, 2);	
				pci_dma_sync_single_for_cpu(np->pci_dev,
							    np->cur_rx->buffer,
							    np->rx_buf_sz,
							    PCI_DMA_FROMDEVICE);
				

#if ! defined(__alpha__)
				skb_copy_to_linear_data(skb,
					np->cur_rx->skbuff->data, pkt_len);
				skb_put(skb, pkt_len);
#else
				memcpy(skb_put(skb, pkt_len),
					np->cur_rx->skbuff->data, pkt_len);
#endif
				pci_dma_sync_single_for_device(np->pci_dev,
							       np->cur_rx->buffer,
							       np->rx_buf_sz,
							       PCI_DMA_FROMDEVICE);
			} else {
				pci_unmap_single(np->pci_dev,
						 np->cur_rx->buffer,
						 np->rx_buf_sz,
						 PCI_DMA_FROMDEVICE);
				skb_put(skb = np->cur_rx->skbuff, pkt_len);
				np->cur_rx->skbuff = NULL;
				--np->really_rx_count;
			}
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += pkt_len;
		}

		np->cur_rx = np->cur_rx->next_desc_logical;
	}			

	
	allocate_rx_buffers(dev);

	return 0;
}


static struct net_device_stats *get_stats(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;

	
	if (netif_running(dev)) {
		dev->stats.rx_missed_errors +=
			ioread32(ioaddr + TALLY) & 0x7fff;
		dev->stats.rx_crc_errors +=
			(ioread32(ioaddr + TALLY) & 0x7fff0000) >> 16;
	}

	return &dev->stats;
}


static void set_rx_mode(struct net_device *dev)
{
	spinlock_t *lp = &((struct netdev_private *)netdev_priv(dev))->lock;
	unsigned long flags;
	spin_lock_irqsave(lp, flags);
	__set_rx_mode(dev);
	spin_unlock_irqrestore(lp, flags);
}


static void __set_rx_mode(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	u32 mc_filter[2];	
	u32 rx_mode;

	if (dev->flags & IFF_PROMISC) {	
		memset(mc_filter, 0xff, sizeof(mc_filter));
		rx_mode = CR_W_PROM | CR_W_AB | CR_W_AM;
	} else if ((netdev_mc_count(dev) > multicast_filter_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		
		memset(mc_filter, 0xff, sizeof(mc_filter));
		rx_mode = CR_W_AB | CR_W_AM;
	} else {
		struct netdev_hw_addr *ha;

		memset(mc_filter, 0, sizeof(mc_filter));
		netdev_for_each_mc_addr(ha, dev) {
			unsigned int bit;
			bit = (ether_crc(ETH_ALEN, ha->addr) >> 26) ^ 0x3F;
			mc_filter[bit >> 5] |= (1 << bit);
		}
		rx_mode = CR_W_AB | CR_W_AM;
	}

	stop_nic_rxtx(ioaddr, np->crvalue);

	iowrite32(mc_filter[0], ioaddr + MAR0);
	iowrite32(mc_filter[1], ioaddr + MAR1);
	np->crvalue &= ~CR_W_RXMODEMASK;
	np->crvalue |= rx_mode;
	iowrite32(np->crvalue, ioaddr + TCRRCR);
}

static void netdev_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct netdev_private *np = netdev_priv(dev);

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(np->pci_dev), sizeof(info->bus_info));
}

static int netdev_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct netdev_private *np = netdev_priv(dev);
	int rc;

	spin_lock_irq(&np->lock);
	rc = mii_ethtool_gset(&np->mii, cmd);
	spin_unlock_irq(&np->lock);

	return rc;
}

static int netdev_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct netdev_private *np = netdev_priv(dev);
	int rc;

	spin_lock_irq(&np->lock);
	rc = mii_ethtool_sset(&np->mii, cmd);
	spin_unlock_irq(&np->lock);

	return rc;
}

static int netdev_nway_reset(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	return mii_nway_restart(&np->mii);
}

static u32 netdev_get_link(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	return mii_link_ok(&np->mii);
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	return debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 value)
{
	debug = value;
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_settings		= netdev_get_settings,
	.set_settings		= netdev_set_settings,
	.nway_reset		= netdev_nway_reset,
	.get_link		= netdev_get_link,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
};

static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct netdev_private *np = netdev_priv(dev);
	int rc;

	if (!netif_running(dev))
		return -EINVAL;

	spin_lock_irq(&np->lock);
	rc = generic_mii_ioctl(&np->mii, if_mii(rq), cmd, NULL);
	spin_unlock_irq(&np->lock);

	return rc;
}


static int netdev_close(struct net_device *dev)
{
	struct netdev_private *np = netdev_priv(dev);
	void __iomem *ioaddr = np->mem;
	int i;

	netif_stop_queue(dev);

	
	iowrite32(0x0000, ioaddr + IMR);

	
	stop_nic_rxtx(ioaddr, 0);

	del_timer_sync(&np->timer);
	del_timer_sync(&np->reset_timer);

	free_irq(dev->irq, dev);

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = np->rx_ring[i].skbuff;

		np->rx_ring[i].status = 0;
		if (skb) {
			pci_unmap_single(np->pci_dev, np->rx_ring[i].buffer,
				np->rx_buf_sz, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(skb);
			np->rx_ring[i].skbuff = NULL;
		}
	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct sk_buff *skb = np->tx_ring[i].skbuff;

		if (skb) {
			pci_unmap_single(np->pci_dev, np->tx_ring[i].buffer,
				skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
			np->tx_ring[i].skbuff = NULL;
		}
	}

	return 0;
}

static DEFINE_PCI_DEVICE_TABLE(fealnx_pci_tbl) = {
	{0x1516, 0x0800, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0x1516, 0x0803, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{0x1516, 0x0891, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
	{} 
};
MODULE_DEVICE_TABLE(pci, fealnx_pci_tbl);


static struct pci_driver fealnx_driver = {
	.name		= "fealnx",
	.id_table	= fealnx_pci_tbl,
	.probe		= fealnx_init_one,
	.remove		= __devexit_p(fealnx_remove_one),
};

static int __init fealnx_init(void)
{
#ifdef MODULE
	printk(version);
#endif

	return pci_register_driver(&fealnx_driver);
}

static void __exit fealnx_exit(void)
{
	pci_unregister_driver(&fealnx_driver);
}

module_init(fealnx_init);
module_exit(fealnx_exit);
