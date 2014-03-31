/*
	Written 1998-2000 by Donald Becker.
	Updates 2000 by Keith Underwood.

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

	This driver is for the Packet Engines GNIC-II PCI Gigabit Ethernet
	adapter.

	Support and updates available at
	http://www.scyld.com/network/hamachi.html
	[link no longer provides useful info -jgarzik]
	or
	http://www.parl.clemson.edu/~keithu/hamachi.html

*/

#define DRV_NAME	"hamachi"
#define DRV_VERSION	"2.1"
#define DRV_RELDATE	"Sept 11, 2006"



static int debug = 1;		
#define final_version
#define hamachi_debug debug
static int max_interrupt_work = 40;
static int mtu;
static int max_rx_latency = 0x11;
static int max_rx_gap = 0x05;
static int min_rx_pkt = 0x18;
static int max_tx_latency = 0x00;
static int max_tx_gap = 0x00;
static int min_tx_pkt = 0x30;

static int rx_copybreak;

static int force32;


#define MAX_UNITS 8				
static int options[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int rx_params[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int tx_params[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};


#define TX_RING_SIZE	64
#define RX_RING_SIZE	512
#define TX_TOTAL_SIZE	TX_RING_SIZE*sizeof(struct hamachi_desc)
#define RX_TOTAL_SIZE	RX_RING_SIZE*sizeof(struct hamachi_desc)



#define RX_CHECKSUM

#define TX_TIMEOUT  (5*HZ)

#include <linux/capability.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#include <asm/uaccess.h>
#include <asm/processor.h>	
#include <asm/io.h>
#include <asm/unaligned.h>
#include <asm/cache.h>

static const char version[] __devinitconst =
KERN_INFO DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE "  Written by Donald Becker\n"
"   Some modifications by Eric kasten <kasten@nscl.msu.edu>\n"
"   Further modifications by Keith Underwood <keithu@parl.clemson.edu>\n";


#ifndef IP_MF
  #define IP_MF 0x2000   
#endif

#ifndef IP_OFFSET
  #ifdef IPOPT_OFFSET
    #define IP_OFFSET IPOPT_OFFSET
  #else
    #define IP_OFFSET 2
  #endif
#endif

#define RUN_AT(x) (jiffies + (x))

#ifndef ADDRLEN
#define ADDRLEN 32
#endif

#if ADDRLEN == 64
#define cpu_to_leXX(addr)	cpu_to_le64(addr)
#define leXX_to_cpu(addr)	le64_to_cpu(addr)
#else
#define cpu_to_leXX(addr)	cpu_to_le32(addr)
#define leXX_to_cpu(addr)	le32_to_cpu(addr)
#endif


/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the Packet Engines "Hamachi"
Gigabit Ethernet chip.  The only PCA currently supported is the GNIC-II 64-bit
66Mhz PCI card.

II. Board-specific settings

No jumpers exist on the board.  The chip supports software correction of
various motherboard wiring errors, however this driver does not support
that feature.

III. Driver operation

IIIa. Ring buffers

The Hamachi uses a typical descriptor based bus-master architecture.
The descriptor list is similar to that used by the Digital Tulip.
This driver uses two statically allocated fixed-size descriptor lists
formed into rings by a branch from the final descriptor to the beginning of
the list.  The ring sizes are set at compile time by RX/TX_RING_SIZE.

This driver uses a zero-copy receive and transmit scheme similar my other
network drivers.
The driver allocates full frame size skbuffs for the Rx ring buffers at
open() time and passes the skb->data field to the Hamachi as receive data
buffers.  When an incoming frame is less than RX_COPYBREAK bytes long,
a fresh skbuff is allocated and the frame is copied to the new skbuff.
When the incoming frame is larger, the skbuff is passed directly up the
protocol stack and replaced by a newly allocated skbuff.

The RX_COPYBREAK value is chosen to trade-off the memory wasted by
using a full-sized skbuff for small frames vs. the copying costs of larger
frames.  Gigabit cards are typically used on generously configured machines
and the underfilled buffers have negligible impact compared to the benefit of
a single allocation size, so the default value of zero results in never
copying packets.

IIIb/c. Transmit/Receive Structure

The Rx and Tx descriptor structure are straight-forward, with no historical
baggage that must be explained.  Unlike the awkward DBDMA structure, there
are no unused fields or option bits that had only one allowable setting.

Two details should be noted about the descriptors: The chip supports both 32
bit and 64 bit address structures, and the length field is overwritten on
the receive descriptors.  The descriptor length is set in the control word
for each channel. The development driver uses 32 bit addresses only, however
64 bit addresses may be enabled for 64 bit architectures e.g. the Alpha.

IIId. Synchronization

This driver is very similar to my other network drivers.
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and other software.

The send packet thread has partial control over the Tx ring and 'dev->tbusy'
flag.  It sets the tbusy flag whenever it's queuing a Tx packet. If the next
queue slot is empty, it clears the tbusy flag when finished otherwise it sets
the 'hmp->tx_full' flag.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  After reaping the stats, it marks the Tx queue entry as
empty by incrementing the dirty_tx mark. Iff the 'hmp->tx_full' flag is set, it
clears both the tx_full and tbusy flags.

IV. Notes

Thanks to Kim Stearns of Packet Engines for providing a pair of GNIC-II boards.

IVb. References

Hamachi Engineering Design Specification, 5/15/97
(Note: This version was marked "Confidential".)

IVc. Errata

None noted.

V.  Recent Changes

01/15/1999 EPK  Enlargement of the TX and RX ring sizes.  This appears
    to help avoid some stall conditions -- this needs further research.

01/15/1999 EPK  Creation of the hamachi_tx function.  This function cleans
    the Tx ring and is called from hamachi_start_xmit (this used to be
    called from hamachi_interrupt but it tends to delay execution of the
    interrupt handler and thus reduce bandwidth by reducing the latency
    between hamachi_rx()'s).  Notably, some modification has been made so
    that the cleaning loop checks only to make sure that the DescOwn bit
    isn't set in the status flag since the card is not required
    to set the entire flag to zero after processing.

01/15/1999 EPK In the hamachi_start_tx function, the Tx ring full flag is
    checked before attempting to add a buffer to the ring.  If the ring is full
    an attempt is made to free any dirty buffers and thus find space for
    the new buffer or the function returns non-zero which should case the
    scheduler to reschedule the buffer later.

01/15/1999 EPK Some adjustments were made to the chip initialization.
    End-to-end flow control should now be fully active and the interrupt
    algorithm vars have been changed.  These could probably use further tuning.

01/15/1999 EPK Added the max_{rx,tx}_latency options.  These are used to
    set the rx and tx latencies for the Hamachi interrupts. If you're having
    problems with network stalls, try setting these to higher values.
    Valid values are 0x00 through 0xff.

01/15/1999 EPK In general, the overall bandwidth has increased and
    latencies are better (sometimes by a factor of 2).  Stalls are rare at
    this point, however there still appears to be a bug somewhere between the
    hardware and driver.  TCP checksum errors under load also appear to be
    eliminated at this point.

01/18/1999 EPK Ensured that the DescEndRing bit was being set on both the
    Rx and Tx rings.  This appears to have been affecting whether a particular
    peer-to-peer connection would hang under high load.  I believe the Rx
    rings was typically getting set correctly, but the Tx ring wasn't getting
    the DescEndRing bit set during initialization. ??? Does this mean the
    hamachi card is using the DescEndRing in processing even if a particular
    slot isn't in use -- hypothetically, the card might be searching the
    entire Tx ring for slots with the DescOwn bit set and then processing
    them.  If the DescEndRing bit isn't set, then it might just wander off
    through memory until it hits a chunk of data with that bit set
    and then looping back.

02/09/1999 EPK Added Michel Mueller's TxDMA Interrupt and Tx-timeout
    problem (TxCmd and RxCmd need only to be set when idle or stopped.

02/09/1999 EPK Added code to check/reset dev->tbusy in hamachi_interrupt.
    (Michel Mueller pointed out the ``permanently busy'' potential
    problem here).

02/22/1999 EPK Added Pete Wyckoff's ioctl to control the Tx/Rx latencies.

02/23/1999 EPK Verified that the interrupt status field bits for Tx were
    incorrectly defined and corrected (as per Michel Mueller).

02/23/1999 EPK Corrected the Tx full check to check that at least 4 slots
    were available before reseting the tbusy and tx_full flags
    (as per Michel Mueller).

03/11/1999 EPK Added Pete Wyckoff's hardware checksumming support.

12/31/1999 KDU Cleaned up assorted things and added Don's code to force
32 bit.

02/20/2000 KDU Some of the control was just plain odd.  Cleaned up the
hamachi_start_xmit() and hamachi_interrupt() code.  There is still some
re-structuring I would like to do.

03/01/2000 KDU Experimenting with a WIDE range of interrupt mitigation
parameters on a dual P3-450 setup yielded the new default interrupt
mitigation parameters.  Tx should interrupt VERY infrequently due to
Eric's scheme.  Rx should be more often...

03/13/2000 KDU Added a patch to make the Rx Checksum code interact
nicely with non-linux machines.

03/13/2000 KDU Experimented with some of the configuration values:

	-It seems that enabling PCI performance commands for descriptors
	(changing RxDMACtrl and TxDMACtrl lower nibble from 5 to D) has minimal
	performance impact for any of my tests. (ttcp, netpipe, netperf)  I will
	leave them that way until I hear further feedback.

	-Increasing the PCI_LATENCY_TIMER to 130
	(2 + (burst size of 128 * (0 wait states + 1))) seems to slightly
	degrade performance.  Leaving default at 64 pending further information.

03/14/2000 KDU Further tuning:

	-adjusted boguscnt in hamachi_rx() to depend on interrupt
	mitigation parameters chosen.

	-Selected a set of interrupt parameters based on some extensive testing.
	These may change with more testing.

TO DO:

-Consider borrowing from the acenic driver code to check PCI_COMMAND for
PCI_COMMAND_INVALIDATE.  Set maximum burst size to cache line size in
that case.

-fix the reset procedure.  It doesn't quite work.
*/

#define PKT_BUF_SZ		1536

#define MAX_FRAME_SIZE  1518


static void hamachi_timer(unsigned long data);

enum capability_flags {CanHaveMII=1, };
static const struct chip_info {
	u16	vendor_id, device_id, device_id_mask, pad;
	const char *name;
	void (*media_timer)(unsigned long data);
	int flags;
} chip_tbl[] = {
	{0x1318, 0x0911, 0xffff, 0, "Hamachi GNIC-II", hamachi_timer, 0},
	{0,},
};

enum hamachi_offsets {
	TxDMACtrl=0x00, TxCmd=0x04, TxStatus=0x06, TxPtr=0x08, TxCurPtr=0x10,
	RxDMACtrl=0x20, RxCmd=0x24, RxStatus=0x26, RxPtr=0x28, RxCurPtr=0x30,
	PCIClkMeas=0x060, MiscStatus=0x066, ChipRev=0x68, ChipReset=0x06B,
	LEDCtrl=0x06C, VirtualJumpers=0x06D, GPIO=0x6E,
	TxChecksum=0x074, RxChecksum=0x076,
	TxIntrCtrl=0x078, RxIntrCtrl=0x07C,
	InterruptEnable=0x080, InterruptClear=0x084, IntrStatus=0x088,
	EventStatus=0x08C,
	MACCnfg=0x0A0, FrameGap0=0x0A2, FrameGap1=0x0A4,
	
	MACCnfg2=0x0B0, RxDepth=0x0B8, FlowCtrl=0x0BC, MaxFrameSize=0x0CE,
	AddrMode=0x0D0, StationAddr=0x0D2,
	
	ANCtrl=0x0E0, ANStatus=0x0E2, ANXchngCtrl=0x0E4, ANAdvertise=0x0E8,
	ANLinkPartnerAbility=0x0EA,
	EECmdStatus=0x0F0, EEData=0x0F1, EEAddr=0x0F2,
	FIFOcfg=0x0F8,
};

enum MII_offsets {
	MII_Cmd=0xA6, MII_Addr=0xA8, MII_Wr_Data=0xAA, MII_Rd_Data=0xAC,
	MII_Status=0xAE,
};

enum intr_status_bits {
	IntrRxDone=0x01, IntrRxPCIFault=0x02, IntrRxPCIErr=0x04,
	IntrTxDone=0x100, IntrTxPCIFault=0x200, IntrTxPCIErr=0x400,
	LinkChange=0x10000, NegotiationChange=0x20000, StatsMax=0x40000, };

struct hamachi_desc {
	__le32 status_n_length;
#if ADDRLEN == 64
	u32 pad;
	__le64 addr;
#else
	__le32 addr;
#endif
};

enum desc_status_bits {
	DescOwn=0x80000000, DescEndPacket=0x40000000, DescEndRing=0x20000000,
	DescIntr=0x10000000,
};

#define PRIV_ALIGN	15  			
#define MII_CNT		4
struct hamachi_private {
	struct hamachi_desc *rx_ring;
	struct hamachi_desc *tx_ring;
	struct sk_buff* rx_skbuff[RX_RING_SIZE];
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	dma_addr_t tx_ring_dma;
	dma_addr_t rx_ring_dma;
	struct timer_list timer;		
	
	spinlock_t lock;
	int chip_id;
	unsigned int cur_rx, dirty_rx;		
	unsigned int cur_tx, dirty_tx;
	unsigned int rx_buf_sz;			
	unsigned int tx_full:1;			
	unsigned int duplex_lock:1;
	unsigned int default_port:4;		
	
	int mii_cnt;								
	struct mii_if_info mii_if;		
	unsigned char phys[MII_CNT];		
	u32 rx_int_var, tx_int_var;	
	u32 option;							
	struct pci_dev *pci_dev;
	void __iomem *base;
};

MODULE_AUTHOR("Donald Becker <becker@scyld.com>, Eric Kasten <kasten@nscl.msu.edu>, Keith Underwood <keithu@parl.clemson.edu>");
MODULE_DESCRIPTION("Packet Engines 'Hamachi' GNIC-II Gigabit Ethernet driver");
MODULE_LICENSE("GPL");

module_param(max_interrupt_work, int, 0);
module_param(mtu, int, 0);
module_param(debug, int, 0);
module_param(min_rx_pkt, int, 0);
module_param(max_rx_gap, int, 0);
module_param(max_rx_latency, int, 0);
module_param(min_tx_pkt, int, 0);
module_param(max_tx_gap, int, 0);
module_param(max_tx_latency, int, 0);
module_param(rx_copybreak, int, 0);
module_param_array(rx_params, int, NULL, 0);
module_param_array(tx_params, int, NULL, 0);
module_param_array(options, int, NULL, 0);
module_param_array(full_duplex, int, NULL, 0);
module_param(force32, int, 0);
MODULE_PARM_DESC(max_interrupt_work, "GNIC-II maximum events handled per interrupt");
MODULE_PARM_DESC(mtu, "GNIC-II MTU (all boards)");
MODULE_PARM_DESC(debug, "GNIC-II debug level (0-7)");
MODULE_PARM_DESC(min_rx_pkt, "GNIC-II minimum Rx packets processed between interrupts");
MODULE_PARM_DESC(max_rx_gap, "GNIC-II maximum Rx inter-packet gap in 8.192 microsecond units");
MODULE_PARM_DESC(max_rx_latency, "GNIC-II time between Rx interrupts in 8.192 microsecond units");
MODULE_PARM_DESC(min_tx_pkt, "GNIC-II minimum Tx packets processed between interrupts");
MODULE_PARM_DESC(max_tx_gap, "GNIC-II maximum Tx inter-packet gap in 8.192 microsecond units");
MODULE_PARM_DESC(max_tx_latency, "GNIC-II time between Tx interrupts in 8.192 microsecond units");
MODULE_PARM_DESC(rx_copybreak, "GNIC-II copy breakpoint for copy-only-tiny-frames");
MODULE_PARM_DESC(rx_params, "GNIC-II min_rx_pkt+max_rx_gap+max_rx_latency");
MODULE_PARM_DESC(tx_params, "GNIC-II min_tx_pkt+max_tx_gap+max_tx_latency");
MODULE_PARM_DESC(options, "GNIC-II Bits 0-3: media type, bits 4-6: as force32, bit 7: half duplex, bit 9 full duplex");
MODULE_PARM_DESC(full_duplex, "GNIC-II full duplex setting(s) (1)");
MODULE_PARM_DESC(force32, "GNIC-II: Bit 0: 32 bit PCI, bit 1: disable parity, bit 2: 64 bit PCI (all boards)");

static int read_eeprom(void __iomem *ioaddr, int location);
static int mdio_read(struct net_device *dev, int phy_id, int location);
static void mdio_write(struct net_device *dev, int phy_id, int location, int value);
static int hamachi_open(struct net_device *dev);
static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void hamachi_timer(unsigned long data);
static void hamachi_tx_timeout(struct net_device *dev);
static void hamachi_init_ring(struct net_device *dev);
static netdev_tx_t hamachi_start_xmit(struct sk_buff *skb,
				      struct net_device *dev);
static irqreturn_t hamachi_interrupt(int irq, void *dev_instance);
static int hamachi_rx(struct net_device *dev);
static inline int hamachi_tx(struct net_device *dev);
static void hamachi_error(struct net_device *dev, int intr_status);
static int hamachi_close(struct net_device *dev);
static struct net_device_stats *hamachi_get_stats(struct net_device *dev);
static void set_rx_mode(struct net_device *dev);
static const struct ethtool_ops ethtool_ops;
static const struct ethtool_ops ethtool_ops_no_mii;

static const struct net_device_ops hamachi_netdev_ops = {
	.ndo_open		= hamachi_open,
	.ndo_stop		= hamachi_close,
	.ndo_start_xmit		= hamachi_start_xmit,
	.ndo_get_stats		= hamachi_get_stats,
	.ndo_set_rx_mode	= set_rx_mode,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_tx_timeout		= hamachi_tx_timeout,
	.ndo_do_ioctl		= netdev_ioctl,
};


static int __devinit hamachi_init_one (struct pci_dev *pdev,
				    const struct pci_device_id *ent)
{
	struct hamachi_private *hmp;
	int option, i, rx_int_var, tx_int_var, boguscnt;
	int chip_id = ent->driver_data;
	int irq;
	void __iomem *ioaddr;
	unsigned long base;
	static int card_idx;
	struct net_device *dev;
	void *ring_space;
	dma_addr_t ring_dma;
	int ret = -ENOMEM;

#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	if (pci_enable_device(pdev)) {
		ret = -EIO;
		goto err_out;
	}

	base = pci_resource_start(pdev, 0);
#ifdef __alpha__				
	base |= (pci_resource_start(pdev, 1) << 32);
#endif

	pci_set_master(pdev);

	i = pci_request_regions(pdev, DRV_NAME);
	if (i)
		return i;

	irq = pdev->irq;
	ioaddr = ioremap(base, 0x400);
	if (!ioaddr)
		goto err_out_release;

	dev = alloc_etherdev(sizeof(struct hamachi_private));
	if (!dev)
		goto err_out_iounmap;

	SET_NETDEV_DEV(dev, &pdev->dev);

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = 1 ? read_eeprom(ioaddr, 4 + i)
			: readb(ioaddr + StationAddr + i);

#if ! defined(final_version)
	if (hamachi_debug > 4)
		for (i = 0; i < 0x10; i++)
			printk("%2.2x%s",
				   read_eeprom(ioaddr, i), i % 16 != 15 ? " " : "\n");
#endif

	hmp = netdev_priv(dev);
	spin_lock_init(&hmp->lock);

	hmp->mii_if.dev = dev;
	hmp->mii_if.mdio_read = mdio_read;
	hmp->mii_if.mdio_write = mdio_write;
	hmp->mii_if.phy_id_mask = 0x1f;
	hmp->mii_if.reg_num_mask = 0x1f;

	ring_space = pci_alloc_consistent(pdev, TX_TOTAL_SIZE, &ring_dma);
	if (!ring_space)
		goto err_out_cleardev;
	hmp->tx_ring = ring_space;
	hmp->tx_ring_dma = ring_dma;

	ring_space = pci_alloc_consistent(pdev, RX_TOTAL_SIZE, &ring_dma);
	if (!ring_space)
		goto err_out_unmap_tx;
	hmp->rx_ring = ring_space;
	hmp->rx_ring_dma = ring_dma;

	
	option = card_idx < MAX_UNITS ? options[card_idx] : 0;
	if (dev->mem_start)
		option = dev->mem_start;

	
	force32 = force32 ? force32 :
		((option  >= 0) ? ((option & 0x00000070) >> 4) : 0 );
	if (force32)
		writeb(force32, ioaddr + VirtualJumpers);

	
	writeb(0x01, ioaddr + ChipReset);

	udelay(10);
	i = readb(ioaddr + PCIClkMeas);
	for (boguscnt = 0; (!(i & 0x080)) && boguscnt < 1000; boguscnt++){
		udelay(10);
		i = readb(ioaddr + PCIClkMeas);
	}

	hmp->base = ioaddr;
	dev->base_addr = (unsigned long)ioaddr;
	dev->irq = irq;
	pci_set_drvdata(pdev, dev);

	hmp->chip_id = chip_id;
	hmp->pci_dev = pdev;

	
	if (option > 0) {
		hmp->option = option;
		if (option & 0x200)
			hmp->mii_if.full_duplex = 1;
		else if (option & 0x080)
			hmp->mii_if.full_duplex = 0;
		hmp->default_port = option & 15;
		if (hmp->default_port)
			hmp->mii_if.force_media = 1;
	}
	if (card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		hmp->mii_if.full_duplex = 1;

	
	if (hmp->mii_if.full_duplex || (option & 0x080))
		hmp->duplex_lock = 1;

	
	max_rx_latency = max_rx_latency & 0x00ff;
	max_rx_gap = max_rx_gap & 0x00ff;
	min_rx_pkt = min_rx_pkt & 0x00ff;
	max_tx_latency = max_tx_latency & 0x00ff;
	max_tx_gap = max_tx_gap & 0x00ff;
	min_tx_pkt = min_tx_pkt & 0x00ff;

	rx_int_var = card_idx < MAX_UNITS ? rx_params[card_idx] : -1;
	tx_int_var = card_idx < MAX_UNITS ? tx_params[card_idx] : -1;
	hmp->rx_int_var = rx_int_var >= 0 ? rx_int_var :
		(min_rx_pkt << 16 | max_rx_gap << 8 | max_rx_latency);
	hmp->tx_int_var = tx_int_var >= 0 ? tx_int_var :
		(min_tx_pkt << 16 | max_tx_gap << 8 | max_tx_latency);


	
	dev->netdev_ops = &hamachi_netdev_ops;
	if (chip_tbl[hmp->chip_id].flags & CanHaveMII)
		SET_ETHTOOL_OPS(dev, &ethtool_ops);
	else
		SET_ETHTOOL_OPS(dev, &ethtool_ops_no_mii);
	dev->watchdog_timeo = TX_TIMEOUT;
	if (mtu)
		dev->mtu = mtu;

	i = register_netdev(dev);
	if (i) {
		ret = i;
		goto err_out_unmap_rx;
	}

	printk(KERN_INFO "%s: %s type %x at %p, %pM, IRQ %d.\n",
		   dev->name, chip_tbl[chip_id].name, readl(ioaddr + ChipRev),
		   ioaddr, dev->dev_addr, irq);
	i = readb(ioaddr + PCIClkMeas);
	printk(KERN_INFO "%s:  %d-bit %d Mhz PCI bus (%d), Virtual Jumpers "
		   "%2.2x, LPA %4.4x.\n",
		   dev->name, readw(ioaddr + MiscStatus) & 1 ? 64 : 32,
		   i ? 2000/(i&0x7f) : 0, i&0x7f, (int)readb(ioaddr + VirtualJumpers),
		   readw(ioaddr + ANLinkPartnerAbility));

	if (chip_tbl[hmp->chip_id].flags & CanHaveMII) {
		int phy, phy_idx = 0;
		for (phy = 0; phy < 32 && phy_idx < MII_CNT; phy++) {
			int mii_status = mdio_read(dev, phy, MII_BMSR);
			if (mii_status != 0xffff  &&
				mii_status != 0x0000) {
				hmp->phys[phy_idx++] = phy;
				hmp->mii_if.advertising = mdio_read(dev, phy, MII_ADVERTISE);
				printk(KERN_INFO "%s: MII PHY found at address %d, status "
					   "0x%4.4x advertising %4.4x.\n",
					   dev->name, phy, mii_status, hmp->mii_if.advertising);
			}
		}
		hmp->mii_cnt = phy_idx;
		if (hmp->mii_cnt > 0)
			hmp->mii_if.phy_id = hmp->phys[0];
		else
			memset(&hmp->mii_if, 0, sizeof(hmp->mii_if));
	}
	
	writew(0x0400, ioaddr + ANXchngCtrl);	
	writew(0x08e0, ioaddr + ANAdvertise);	
	writew(0x1000, ioaddr + ANCtrl);			

	card_idx++;
	return 0;

err_out_unmap_rx:
	pci_free_consistent(pdev, RX_TOTAL_SIZE, hmp->rx_ring,
		hmp->rx_ring_dma);
err_out_unmap_tx:
	pci_free_consistent(pdev, TX_TOTAL_SIZE, hmp->tx_ring,
		hmp->tx_ring_dma);
err_out_cleardev:
	free_netdev (dev);
err_out_iounmap:
	iounmap(ioaddr);
err_out_release:
	pci_release_regions(pdev);
err_out:
	return ret;
}

static int __devinit read_eeprom(void __iomem *ioaddr, int location)
{
	int bogus_cnt = 1000;

	
	while ((readb(ioaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0);
	writew(location, ioaddr + EEAddr);
	writeb(0x02, ioaddr + EECmdStatus);
	bogus_cnt = 1000;
	while ((readb(ioaddr + EECmdStatus) & 0x40)  && --bogus_cnt > 0);
	if (hamachi_debug > 5)
		printk("   EEPROM status is %2.2x after %d ticks.\n",
			   (int)readb(ioaddr + EECmdStatus), 1000- bogus_cnt);
	return readb(ioaddr + EEData);
}


static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;
	int i;

	
	for (i = 10000; i >= 0; i--)
		if ((readw(ioaddr + MII_Status) & 1) == 0)
			break;
	writew((phy_id<<8) + location, ioaddr + MII_Addr);
	writew(0x0001, ioaddr + MII_Cmd);
	for (i = 10000; i >= 0; i--)
		if ((readw(ioaddr + MII_Status) & 1) == 0)
			break;
	return readw(ioaddr + MII_Rd_Data);
}

static void mdio_write(struct net_device *dev, int phy_id, int location, int value)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;
	int i;

	
	for (i = 10000; i >= 0; i--)
		if ((readw(ioaddr + MII_Status) & 1) == 0)
			break;
	writew((phy_id<<8) + location, ioaddr + MII_Addr);
	writew(value, ioaddr + MII_Wr_Data);

	
	for (i = 10000; i >= 0; i--)
		if ((readw(ioaddr + MII_Status) & 1) == 0)
			break;
}


static int hamachi_open(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;
	int i;
	u32 rx_int_var, tx_int_var;
	u16 fifo_info;

	i = request_irq(dev->irq, hamachi_interrupt, IRQF_SHARED, dev->name, dev);
	if (i)
		return i;

	if (hamachi_debug > 1)
		printk(KERN_DEBUG "%s: hamachi_open() irq %d.\n",
			   dev->name, dev->irq);

	hamachi_init_ring(dev);

#if ADDRLEN == 64
	
	writel(hmp->rx_ring_dma, ioaddr + RxPtr);
	writel(hmp->rx_ring_dma >> 32, ioaddr + RxPtr + 4);
	writel(hmp->tx_ring_dma, ioaddr + TxPtr);
	writel(hmp->tx_ring_dma >> 32, ioaddr + TxPtr + 4);
#else
	writel(hmp->rx_ring_dma, ioaddr + RxPtr);
	writel(hmp->tx_ring_dma, ioaddr + TxPtr);
#endif

	for (i = 0; i < 6; i++)
		writeb(dev->dev_addr[i], ioaddr + StationAddr + i);


	
	fifo_info = (readw(ioaddr + GPIO) & 0x00C0) >> 6;
	switch (fifo_info){
		case 0 :
			
			writew(0x0000, ioaddr + FIFOcfg);
			break;
		case 1 :
			
			writew(0x0028, ioaddr + FIFOcfg);
			break;
		case 2 :
			
			writew(0x004C, ioaddr + FIFOcfg);
			break;
		case 3 :
			
			writew(0x006C, ioaddr + FIFOcfg);
			break;
		default :
			printk(KERN_WARNING "%s:  Unsupported external memory config!\n",
				dev->name);
			
			writew(0x0000, ioaddr + FIFOcfg);
			break;
	}

	if (dev->if_port == 0)
		dev->if_port = hmp->default_port;


	
	
	if (hmp->duplex_lock != 1)
		hmp->mii_if.full_duplex = 1;

	
	writew(0x0001, ioaddr + RxChecksum);
	writew(0x0000, ioaddr + TxChecksum);
	writew(0x8000, ioaddr + MACCnfg); 
	writew(0x215F, ioaddr + MACCnfg);
	writew(0x000C, ioaddr + FrameGap0);
	
	writew(0x1018, ioaddr + FrameGap1);
	
	writew(0x0780, ioaddr + MACCnfg2); 
	
	writel(0x0030FFFF, ioaddr + FlowCtrl);
	writew(MAX_FRAME_SIZE, ioaddr + MaxFrameSize); 	

	
	writew(0x0400, ioaddr + ANXchngCtrl);	
	
	writeb(0x03, ioaddr + LEDCtrl);


	rx_int_var = hmp->rx_int_var;
	tx_int_var = hmp->tx_int_var;

	if (hamachi_debug > 1) {
		printk("max_tx_latency: %d, max_tx_gap: %d, min_tx_pkt: %d\n",
			tx_int_var & 0x00ff, (tx_int_var & 0x00ff00) >> 8,
			(tx_int_var & 0x00ff0000) >> 16);
		printk("max_rx_latency: %d, max_rx_gap: %d, min_rx_pkt: %d\n",
			rx_int_var & 0x00ff, (rx_int_var & 0x00ff00) >> 8,
			(rx_int_var & 0x00ff0000) >> 16);
		printk("rx_int_var: %x, tx_int_var: %x\n", rx_int_var, tx_int_var);
	}

	writel(tx_int_var, ioaddr + TxIntrCtrl);
	writel(rx_int_var, ioaddr + RxIntrCtrl);

	set_rx_mode(dev);

	netif_start_queue(dev);

	
	writel(0x80878787, ioaddr + InterruptEnable);
	writew(0x0000, ioaddr + EventStatus);	

	
	
#if ADDRLEN == 64
	writew(0x005D, ioaddr + RxDMACtrl); 		
	writew(0x005D, ioaddr + TxDMACtrl);
#else
	writew(0x001D, ioaddr + RxDMACtrl);
	writew(0x001D, ioaddr + TxDMACtrl);
#endif
	writew(0x0001, ioaddr + RxCmd);

	if (hamachi_debug > 2) {
		printk(KERN_DEBUG "%s: Done hamachi_open(), status: Rx %x Tx %x.\n",
			   dev->name, readw(ioaddr + RxStatus), readw(ioaddr + TxStatus));
	}
	
	init_timer(&hmp->timer);
	hmp->timer.expires = RUN_AT((24*HZ)/10);			
	hmp->timer.data = (unsigned long)dev;
	hmp->timer.function = hamachi_timer;				
	add_timer(&hmp->timer);

	return 0;
}

static inline int hamachi_tx(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);

	for (; hmp->cur_tx - hmp->dirty_tx > 0; hmp->dirty_tx++) {
		int entry = hmp->dirty_tx % TX_RING_SIZE;
		struct sk_buff *skb;

		if (hmp->tx_ring[entry].status_n_length & cpu_to_le32(DescOwn))
			break;
		
		skb = hmp->tx_skbuff[entry];
		if (skb) {
			pci_unmap_single(hmp->pci_dev,
				leXX_to_cpu(hmp->tx_ring[entry].addr),
				skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
			hmp->tx_skbuff[entry] = NULL;
		}
		hmp->tx_ring[entry].status_n_length = 0;
		if (entry >= TX_RING_SIZE-1)
			hmp->tx_ring[TX_RING_SIZE-1].status_n_length |=
				cpu_to_le32(DescEndRing);
		dev->stats.tx_packets++;
	}

	return 0;
}

static void hamachi_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;
	int next_tick = 10*HZ;

	if (hamachi_debug > 2) {
		printk(KERN_INFO "%s: Hamachi Autonegotiation status %4.4x, LPA "
			   "%4.4x.\n", dev->name, readw(ioaddr + ANStatus),
			   readw(ioaddr + ANLinkPartnerAbility));
		printk(KERN_INFO "%s: Autonegotiation regs %4.4x %4.4x %4.4x "
		       "%4.4x %4.4x %4.4x.\n", dev->name,
		       readw(ioaddr + 0x0e0),
		       readw(ioaddr + 0x0e2),
		       readw(ioaddr + 0x0e4),
		       readw(ioaddr + 0x0e6),
		       readw(ioaddr + 0x0e8),
		       readw(ioaddr + 0x0eA));
	}
	
	hmp->timer.expires = RUN_AT(next_tick);
	add_timer(&hmp->timer);
}

static void hamachi_tx_timeout(struct net_device *dev)
{
	int i;
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;

	printk(KERN_WARNING "%s: Hamachi transmit timed out, status %8.8x,"
		   " resetting...\n", dev->name, (int)readw(ioaddr + TxStatus));

	{
		printk(KERN_DEBUG "  Rx ring %p: ", hmp->rx_ring);
		for (i = 0; i < RX_RING_SIZE; i++)
			printk(KERN_CONT " %8.8x",
			       le32_to_cpu(hmp->rx_ring[i].status_n_length));
		printk(KERN_CONT "\n");
		printk(KERN_DEBUG"  Tx ring %p: ", hmp->tx_ring);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(KERN_CONT " %4.4x",
			       le32_to_cpu(hmp->tx_ring[i].status_n_length));
		printk(KERN_CONT "\n");
	}

	dev->if_port = 0;

	for (i = 0; i < RX_RING_SIZE; i++)
		hmp->rx_ring[i].status_n_length &= cpu_to_le32(~DescOwn);

	for (i = 0; i < TX_RING_SIZE; i++){
		struct sk_buff *skb;

		if (i >= TX_RING_SIZE - 1)
			hmp->tx_ring[i].status_n_length =
				cpu_to_le32(DescEndRing) |
				(hmp->tx_ring[i].status_n_length &
				 cpu_to_le32(0x0000ffff));
		else
			hmp->tx_ring[i].status_n_length &= cpu_to_le32(0x0000ffff);
		skb = hmp->tx_skbuff[i];
		if (skb){
			pci_unmap_single(hmp->pci_dev, leXX_to_cpu(hmp->tx_ring[i].addr),
				skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
			hmp->tx_skbuff[i] = NULL;
		}
	}

	udelay(60); 
	writew(0x0002, ioaddr + RxCmd); 

	writeb(0x01, ioaddr + ChipReset);  

	hmp->tx_full = 0;
	hmp->cur_rx = hmp->cur_tx = 0;
	hmp->dirty_rx = hmp->dirty_tx = 0;
	for (i = 0; i < RX_RING_SIZE; i++){
		struct sk_buff *skb = hmp->rx_skbuff[i];

		if (skb){
			pci_unmap_single(hmp->pci_dev,
				leXX_to_cpu(hmp->rx_ring[i].addr),
				hmp->rx_buf_sz, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(skb);
			hmp->rx_skbuff[i] = NULL;
		}
	}
	
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb;

		skb = netdev_alloc_skb_ip_align(dev, hmp->rx_buf_sz);
		hmp->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;

                hmp->rx_ring[i].addr = cpu_to_leXX(pci_map_single(hmp->pci_dev,
			skb->data, hmp->rx_buf_sz, PCI_DMA_FROMDEVICE));
		hmp->rx_ring[i].status_n_length = cpu_to_le32(DescOwn |
			DescEndPacket | DescIntr | (hmp->rx_buf_sz - 2));
	}
	hmp->dirty_rx = (unsigned int)(i - RX_RING_SIZE);
	
	hmp->rx_ring[RX_RING_SIZE-1].status_n_length |= cpu_to_le32(DescEndRing);

	
	dev->trans_start = jiffies; 
	dev->stats.tx_errors++;

	
	writew(0x0002, ioaddr + TxCmd); 
	writew(0x0001, ioaddr + TxCmd); 
	writew(0x0001, ioaddr + RxCmd); 

	netif_wake_queue(dev);
}


static void hamachi_init_ring(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	int i;

	hmp->tx_full = 0;
	hmp->cur_rx = hmp->cur_tx = 0;
	hmp->dirty_rx = hmp->dirty_tx = 0;

	hmp->rx_buf_sz = (dev->mtu <= 1492 ? PKT_BUF_SZ :
		(((dev->mtu+26+7) & ~7) + 16));

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		hmp->rx_ring[i].status_n_length = 0;
		hmp->rx_skbuff[i] = NULL;
	}
	
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = netdev_alloc_skb(dev, hmp->rx_buf_sz + 2);
		hmp->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;
		skb_reserve(skb, 2); 
                hmp->rx_ring[i].addr = cpu_to_leXX(pci_map_single(hmp->pci_dev,
			skb->data, hmp->rx_buf_sz, PCI_DMA_FROMDEVICE));
		
		hmp->rx_ring[i].status_n_length = cpu_to_le32(DescOwn |
			DescEndPacket | DescIntr | (hmp->rx_buf_sz -2));
	}
	hmp->dirty_rx = (unsigned int)(i - RX_RING_SIZE);
	hmp->rx_ring[RX_RING_SIZE-1].status_n_length |= cpu_to_le32(DescEndRing);

	for (i = 0; i < TX_RING_SIZE; i++) {
		hmp->tx_skbuff[i] = NULL;
		hmp->tx_ring[i].status_n_length = 0;
	}
	
	hmp->tx_ring[TX_RING_SIZE-1].status_n_length |= cpu_to_le32(DescEndRing);
}


static netdev_tx_t hamachi_start_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	unsigned entry;
	u16 status;

	if (hmp->tx_full) {
		
		printk(KERN_WARNING "%s: Hamachi transmit queue full at slot %d.\n",dev->name, hmp->cur_tx);

		
		
		status=readw(hmp->base + TxStatus);
		if( !(status & 0x0001) || (status & 0x0002))
			writew(0x0001, hmp->base + TxCmd);
		return NETDEV_TX_BUSY;
	}


	
	entry = hmp->cur_tx % TX_RING_SIZE;

	hmp->tx_skbuff[entry] = skb;

        hmp->tx_ring[entry].addr = cpu_to_leXX(pci_map_single(hmp->pci_dev,
		skb->data, skb->len, PCI_DMA_TODEVICE));

	if (entry >= TX_RING_SIZE-1)		 
		hmp->tx_ring[entry].status_n_length = cpu_to_le32(DescOwn |
			DescEndPacket | DescEndRing | DescIntr | skb->len);
	else
		hmp->tx_ring[entry].status_n_length = cpu_to_le32(DescOwn |
			DescEndPacket | DescIntr | skb->len);
	hmp->cur_tx++;

	

	
	
	status=readw(hmp->base + TxStatus);
	if( !(status & 0x0001) || (status & 0x0002))
		writew(0x0001, hmp->base + TxCmd);

	
	hamachi_tx(dev);

	if ((hmp->cur_tx - hmp->dirty_tx) < (TX_RING_SIZE - 4))
		netif_wake_queue(dev);  
	else {
		hmp->tx_full = 1;
		netif_stop_queue(dev);
	}

	if (hamachi_debug > 4) {
		printk(KERN_DEBUG "%s: Hamachi transmit frame #%d queued in slot %d.\n",
			   dev->name, hmp->cur_tx, entry);
	}
	return NETDEV_TX_OK;
}

static irqreturn_t hamachi_interrupt(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;
	long boguscnt = max_interrupt_work;
	int handled = 0;

#ifndef final_version			
	if (dev == NULL) {
		printk (KERN_ERR "hamachi_interrupt(): irq %d for unknown device.\n", irq);
		return IRQ_NONE;
	}
#endif

	spin_lock(&hmp->lock);

	do {
		u32 intr_status = readl(ioaddr + InterruptClear);

		if (hamachi_debug > 4)
			printk(KERN_DEBUG "%s: Hamachi interrupt, status %4.4x.\n",
				   dev->name, intr_status);

		if (intr_status == 0)
			break;

		handled = 1;

		if (intr_status & IntrRxDone)
			hamachi_rx(dev);

		if (intr_status & IntrTxDone){
			if (hmp->tx_full){
				for (; hmp->cur_tx - hmp->dirty_tx > 0; hmp->dirty_tx++){
					int entry = hmp->dirty_tx % TX_RING_SIZE;
					struct sk_buff *skb;

					if (hmp->tx_ring[entry].status_n_length & cpu_to_le32(DescOwn))
						break;
					skb = hmp->tx_skbuff[entry];
					
					if (skb){
						pci_unmap_single(hmp->pci_dev,
							leXX_to_cpu(hmp->tx_ring[entry].addr),
							skb->len,
							PCI_DMA_TODEVICE);
						dev_kfree_skb_irq(skb);
						hmp->tx_skbuff[entry] = NULL;
					}
					hmp->tx_ring[entry].status_n_length = 0;
					if (entry >= TX_RING_SIZE-1)
						hmp->tx_ring[TX_RING_SIZE-1].status_n_length |=
							cpu_to_le32(DescEndRing);
					dev->stats.tx_packets++;
				}
				if (hmp->cur_tx - hmp->dirty_tx < TX_RING_SIZE - 4){
					
					hmp->tx_full = 0;
					netif_wake_queue(dev);
				}
			} else {
				netif_wake_queue(dev);
			}
		}


		
		if (intr_status &
			(IntrTxPCIFault | IntrTxPCIErr | IntrRxPCIFault | IntrRxPCIErr |
			 LinkChange | NegotiationChange | StatsMax))
			hamachi_error(dev, intr_status);

		if (--boguscnt < 0) {
			printk(KERN_WARNING "%s: Too much work at interrupt, status=0x%4.4x.\n",
				   dev->name, intr_status);
			break;
		}
	} while (1);

	if (hamachi_debug > 3)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#4.4x.\n",
			   dev->name, readl(ioaddr + IntrStatus));

#ifndef final_version
	
	{
		static int stopit = 10;
		if (dev->start == 0  &&  --stopit < 0) {
			printk(KERN_ERR "%s: Emergency stop, looping startup interrupt.\n",
				   dev->name);
			free_irq(irq, dev);
		}
	}
#endif

	spin_unlock(&hmp->lock);
	return IRQ_RETVAL(handled);
}

static int hamachi_rx(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	int entry = hmp->cur_rx % RX_RING_SIZE;
	int boguscnt = (hmp->dirty_rx + RX_RING_SIZE) - hmp->cur_rx;

	if (hamachi_debug > 4) {
		printk(KERN_DEBUG " In hamachi_rx(), entry %d status %4.4x.\n",
			   entry, hmp->rx_ring[entry].status_n_length);
	}

	
	while (1) {
		struct hamachi_desc *desc = &(hmp->rx_ring[entry]);
		u32 desc_status = le32_to_cpu(desc->status_n_length);
		u16 data_size = desc_status;	
		u8 *buf_addr;
		s32 frame_status;

		if (desc_status & DescOwn)
			break;
		pci_dma_sync_single_for_cpu(hmp->pci_dev,
					    leXX_to_cpu(desc->addr),
					    hmp->rx_buf_sz,
					    PCI_DMA_FROMDEVICE);
		buf_addr = (u8 *) hmp->rx_skbuff[entry]->data;
		frame_status = get_unaligned_le32(&(buf_addr[data_size - 12]));
		if (hamachi_debug > 4)
			printk(KERN_DEBUG "  hamachi_rx() status was %8.8x.\n",
				frame_status);
		if (--boguscnt < 0)
			break;
		if ( ! (desc_status & DescEndPacket)) {
			printk(KERN_WARNING "%s: Oversized Ethernet frame spanned "
				   "multiple buffers, entry %#x length %d status %4.4x!\n",
				   dev->name, hmp->cur_rx, data_size, desc_status);
			printk(KERN_WARNING "%s: Oversized Ethernet frame %p vs %p.\n",
				   dev->name, desc, &hmp->rx_ring[hmp->cur_rx % RX_RING_SIZE]);
			printk(KERN_WARNING "%s: Oversized Ethernet frame -- next status %x/%x last status %x.\n",
				   dev->name,
				   le32_to_cpu(hmp->rx_ring[(hmp->cur_rx+1) % RX_RING_SIZE].status_n_length) & 0xffff0000,
				   le32_to_cpu(hmp->rx_ring[(hmp->cur_rx+1) % RX_RING_SIZE].status_n_length) & 0x0000ffff,
				   le32_to_cpu(hmp->rx_ring[(hmp->cur_rx-1) % RX_RING_SIZE].status_n_length));
			dev->stats.rx_length_errors++;
		} 
		if (frame_status & 0x00380000) {
			
			if (hamachi_debug > 2)
				printk(KERN_DEBUG "  hamachi_rx() Rx error was %8.8x.\n",
					   frame_status);
			dev->stats.rx_errors++;
			if (frame_status & 0x00600000)
				dev->stats.rx_length_errors++;
			if (frame_status & 0x00080000)
				dev->stats.rx_frame_errors++;
			if (frame_status & 0x00100000)
				dev->stats.rx_crc_errors++;
			if (frame_status < 0)
				dev->stats.rx_dropped++;
		} else {
			struct sk_buff *skb;
			
			u16 pkt_len = (frame_status & 0x07ff) - 4;
#ifdef RX_CHECKSUM
			u32 pfck = *(u32 *) &buf_addr[data_size - 8];
#endif


#ifndef final_version
			if (hamachi_debug > 4)
				printk(KERN_DEBUG "  hamachi_rx() normal Rx pkt length %d"
					   " of %d, bogus_cnt %d.\n",
					   pkt_len, data_size, boguscnt);
			if (hamachi_debug > 5)
				printk(KERN_DEBUG"%s:  rx status %8.8x %8.8x %8.8x %8.8x %8.8x.\n",
					   dev->name,
					   *(s32*)&(buf_addr[data_size - 20]),
					   *(s32*)&(buf_addr[data_size - 16]),
					   *(s32*)&(buf_addr[data_size - 12]),
					   *(s32*)&(buf_addr[data_size - 8]),
					   *(s32*)&(buf_addr[data_size - 4]));
#endif
			if (pkt_len < rx_copybreak &&
			    (skb = netdev_alloc_skb(dev, pkt_len + 2)) != NULL) {
#ifdef RX_CHECKSUM
				printk(KERN_ERR "%s: rx_copybreak non-zero "
				  "not good with RX_CHECKSUM\n", dev->name);
#endif
				skb_reserve(skb, 2);	
				pci_dma_sync_single_for_cpu(hmp->pci_dev,
							    leXX_to_cpu(hmp->rx_ring[entry].addr),
							    hmp->rx_buf_sz,
							    PCI_DMA_FROMDEVICE);
				
#if 1 || USE_IP_COPYSUM
				skb_copy_to_linear_data(skb,
					hmp->rx_skbuff[entry]->data, pkt_len);
				skb_put(skb, pkt_len);
#else
				memcpy(skb_put(skb, pkt_len), hmp->rx_ring_dma
					+ entry*sizeof(*desc), pkt_len);
#endif
				pci_dma_sync_single_for_device(hmp->pci_dev,
							       leXX_to_cpu(hmp->rx_ring[entry].addr),
							       hmp->rx_buf_sz,
							       PCI_DMA_FROMDEVICE);
			} else {
				pci_unmap_single(hmp->pci_dev,
						 leXX_to_cpu(hmp->rx_ring[entry].addr),
						 hmp->rx_buf_sz, PCI_DMA_FROMDEVICE);
				skb_put(skb = hmp->rx_skbuff[entry], pkt_len);
				hmp->rx_skbuff[entry] = NULL;
			}
			skb->protocol = eth_type_trans(skb, dev);


#ifdef RX_CHECKSUM
			
			if (pfck>>24 == 0x91 || pfck>>24 == 0x51) {
				struct iphdr *ih = (struct iphdr *) skb->data;
				if (ntohs(ih->tot_len) >= 46){
					
					if (!(ih->frag_off & cpu_to_be16(IP_MF|IP_OFFSET))) {
						u32 inv = *(u32 *) &buf_addr[data_size - 16];
						u32 *p = (u32 *) &buf_addr[data_size - 20];
						register u32 crc, p_r, p_r1;

						if (inv & 4) {
							inv &= ~4;
							--p;
						}
						p_r = *p;
						p_r1 = *(p-1);
						switch (inv) {
							case 0:
								crc = (p_r & 0xffff) + (p_r >> 16);
								break;
							case 1:
								crc = (p_r >> 16) + (p_r & 0xffff)
									+ (p_r1 >> 16 & 0xff00);
								break;
							case 2:
								crc = p_r + (p_r1 >> 16);
								break;
							case 3:
								crc = p_r + (p_r1 & 0xff00) + (p_r1 >> 16);
								break;
							default:	 crc = 0;
						}
						if (crc & 0xffff0000) {
							crc &= 0xffff;
							++crc;
						}
						
						skb->csum = ntohs(pfck & 0xffff);
						if (skb->csum > crc)
							skb->csum -= crc;
						else
							skb->csum += (~crc & 0xffff);
						skb->ip_summed = CHECKSUM_COMPLETE;
					}
				}
			}
#endif  

			netif_rx(skb);
			dev->stats.rx_packets++;
		}
		entry = (++hmp->cur_rx) % RX_RING_SIZE;
	}

	
	for (; hmp->cur_rx - hmp->dirty_rx > 0; hmp->dirty_rx++) {
		struct hamachi_desc *desc;

		entry = hmp->dirty_rx % RX_RING_SIZE;
		desc = &(hmp->rx_ring[entry]);
		if (hmp->rx_skbuff[entry] == NULL) {
			struct sk_buff *skb = netdev_alloc_skb(dev, hmp->rx_buf_sz + 2);

			hmp->rx_skbuff[entry] = skb;
			if (skb == NULL)
				break;		
			skb_reserve(skb, 2);	
                	desc->addr = cpu_to_leXX(pci_map_single(hmp->pci_dev,
				skb->data, hmp->rx_buf_sz, PCI_DMA_FROMDEVICE));
		}
		desc->status_n_length = cpu_to_le32(hmp->rx_buf_sz);
		if (entry >= RX_RING_SIZE-1)
			desc->status_n_length |= cpu_to_le32(DescOwn |
				DescEndPacket | DescEndRing | DescIntr);
		else
			desc->status_n_length |= cpu_to_le32(DescOwn |
				DescEndPacket | DescIntr);
	}

	
	
	if (readw(hmp->base + RxStatus) & 0x0002)
		writew(0x0001, hmp->base + RxCmd);

	return 0;
}

static void hamachi_error(struct net_device *dev, int intr_status)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;

	if (intr_status & (LinkChange|NegotiationChange)) {
		if (hamachi_debug > 1)
			printk(KERN_INFO "%s: Link changed: AutoNegotiation Ctrl"
				   " %4.4x, Status %4.4x %4.4x Intr status %4.4x.\n",
				   dev->name, readw(ioaddr + 0x0E0), readw(ioaddr + 0x0E2),
				   readw(ioaddr + ANLinkPartnerAbility),
				   readl(ioaddr + IntrStatus));
		if (readw(ioaddr + ANStatus) & 0x20)
			writeb(0x01, ioaddr + LEDCtrl);
		else
			writeb(0x03, ioaddr + LEDCtrl);
	}
	if (intr_status & StatsMax) {
		hamachi_get_stats(dev);
		
		readl(ioaddr + 0x370);
		readl(ioaddr + 0x3F0);
	}
	if ((intr_status & ~(LinkChange|StatsMax|NegotiationChange|IntrRxDone|IntrTxDone)) &&
	    hamachi_debug)
		printk(KERN_ERR "%s: Something Wicked happened! %4.4x.\n",
		       dev->name, intr_status);
	
	if (intr_status & (IntrTxPCIErr | IntrTxPCIFault))
		dev->stats.tx_fifo_errors++;
	if (intr_status & (IntrRxPCIErr | IntrRxPCIFault))
		dev->stats.rx_fifo_errors++;
}

static int hamachi_close(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;
	struct sk_buff *skb;
	int i;

	netif_stop_queue(dev);

	if (hamachi_debug > 1) {
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was Tx %4.4x Rx %4.4x Int %2.2x.\n",
			   dev->name, readw(ioaddr + TxStatus),
			   readw(ioaddr + RxStatus), readl(ioaddr + IntrStatus));
		printk(KERN_DEBUG "%s: Queue pointers were Tx %d / %d,  Rx %d / %d.\n",
			   dev->name, hmp->cur_tx, hmp->dirty_tx, hmp->cur_rx, hmp->dirty_rx);
	}

	
	writel(0x0000, ioaddr + InterruptEnable);

	
	writel(2, ioaddr + RxCmd);
	writew(2, ioaddr + TxCmd);

#ifdef __i386__
	if (hamachi_debug > 2) {
		printk(KERN_DEBUG "  Tx ring at %8.8x:\n",
			   (int)hmp->tx_ring_dma);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(KERN_DEBUG " %c #%d desc. %8.8x %8.8x.\n",
				   readl(ioaddr + TxCurPtr) == (long)&hmp->tx_ring[i] ? '>' : ' ',
				   i, hmp->tx_ring[i].status_n_length, hmp->tx_ring[i].addr);
		printk(KERN_DEBUG "  Rx ring %8.8x:\n",
			   (int)hmp->rx_ring_dma);
		for (i = 0; i < RX_RING_SIZE; i++) {
			printk(KERN_DEBUG " %c #%d desc. %4.4x %8.8x\n",
				   readl(ioaddr + RxCurPtr) == (long)&hmp->rx_ring[i] ? '>' : ' ',
				   i, hmp->rx_ring[i].status_n_length, hmp->rx_ring[i].addr);
			if (hamachi_debug > 6) {
				if (*(u8*)hmp->rx_skbuff[i]->data != 0x69) {
					u16 *addr = (u16 *)
						hmp->rx_skbuff[i]->data;
					int j;
					printk(KERN_DEBUG "Addr: ");
					for (j = 0; j < 0x50; j++)
						printk(" %4.4x", addr[j]);
					printk("\n");
				}
			}
		}
	}
#endif 

	free_irq(dev->irq, dev);

	del_timer_sync(&hmp->timer);

	
	for (i = 0; i < RX_RING_SIZE; i++) {
		skb = hmp->rx_skbuff[i];
		hmp->rx_ring[i].status_n_length = 0;
		if (skb) {
			pci_unmap_single(hmp->pci_dev,
				leXX_to_cpu(hmp->rx_ring[i].addr),
				hmp->rx_buf_sz, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(skb);
			hmp->rx_skbuff[i] = NULL;
		}
		hmp->rx_ring[i].addr = cpu_to_leXX(0xBADF00D0); 
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		skb = hmp->tx_skbuff[i];
		if (skb) {
			pci_unmap_single(hmp->pci_dev,
				leXX_to_cpu(hmp->tx_ring[i].addr),
				skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
			hmp->tx_skbuff[i] = NULL;
		}
	}

	writeb(0x00, ioaddr + LEDCtrl);

	return 0;
}

static struct net_device_stats *hamachi_get_stats(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;

	

	
	dev->stats.rx_bytes = readl(ioaddr + 0x330);
	
	dev->stats.tx_bytes = readl(ioaddr + 0x3B0);
	
	dev->stats.multicast = readl(ioaddr + 0x320);

	
	dev->stats.rx_length_errors = readl(ioaddr + 0x368);
	
	dev->stats.rx_over_errors = readl(ioaddr + 0x35C);
	
	dev->stats.rx_crc_errors = readl(ioaddr + 0x360);
	
	dev->stats.rx_frame_errors = readl(ioaddr + 0x364);
	
	dev->stats.rx_missed_errors = readl(ioaddr + 0x36C);

	return &dev->stats;
}

static void set_rx_mode(struct net_device *dev)
{
	struct hamachi_private *hmp = netdev_priv(dev);
	void __iomem *ioaddr = hmp->base;

	if (dev->flags & IFF_PROMISC) {			
		writew(0x000F, ioaddr + AddrMode);
	} else if ((netdev_mc_count(dev) > 63) || (dev->flags & IFF_ALLMULTI)) {
		
		writew(0x000B, ioaddr + AddrMode);
	} else if (!netdev_mc_empty(dev)) { 
		struct netdev_hw_addr *ha;
		int i = 0;

		netdev_for_each_mc_addr(ha, dev) {
			writel(*(u32 *)(ha->addr), ioaddr + 0x100 + i*8);
			writel(0x20000 | (*(u16 *)&ha->addr[4]),
				   ioaddr + 0x104 + i*8);
			i++;
		}
		
		for (; i < 64; i++)
			writel(0, ioaddr + 0x104 + i*8);
		writew(0x0003, ioaddr + AddrMode);
	} else {					
		writew(0x0001, ioaddr + AddrMode);
	}
}

static int check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EINVAL;
	return 0;
}

static void hamachi_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct hamachi_private *np = netdev_priv(dev);
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->bus_info, pci_name(np->pci_dev));
}

static int hamachi_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct hamachi_private *np = netdev_priv(dev);
	spin_lock_irq(&np->lock);
	mii_ethtool_gset(&np->mii_if, ecmd);
	spin_unlock_irq(&np->lock);
	return 0;
}

static int hamachi_set_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	struct hamachi_private *np = netdev_priv(dev);
	int res;
	spin_lock_irq(&np->lock);
	res = mii_ethtool_sset(&np->mii_if, ecmd);
	spin_unlock_irq(&np->lock);
	return res;
}

static int hamachi_nway_reset(struct net_device *dev)
{
	struct hamachi_private *np = netdev_priv(dev);
	return mii_nway_restart(&np->mii_if);
}

static u32 hamachi_get_link(struct net_device *dev)
{
	struct hamachi_private *np = netdev_priv(dev);
	return mii_link_ok(&np->mii_if);
}

static const struct ethtool_ops ethtool_ops = {
	.begin = check_if_running,
	.get_drvinfo = hamachi_get_drvinfo,
	.get_settings = hamachi_get_settings,
	.set_settings = hamachi_set_settings,
	.nway_reset = hamachi_nway_reset,
	.get_link = hamachi_get_link,
};

static const struct ethtool_ops ethtool_ops_no_mii = {
	.begin = check_if_running,
	.get_drvinfo = hamachi_get_drvinfo,
};

static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct hamachi_private *np = netdev_priv(dev);
	struct mii_ioctl_data *data = if_mii(rq);
	int rc;

	if (!netif_running(dev))
		return -EINVAL;

	if (cmd == (SIOCDEVPRIVATE+3)) { 
		u32 *d = (u32 *)&rq->ifr_ifru;
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		writel(d[0], np->base + TxIntrCtrl);
		writel(d[1], np->base + RxIntrCtrl);
		printk(KERN_NOTICE "%s: tx %08x, rx %08x intr\n", dev->name,
		  (u32) readl(np->base + TxIntrCtrl),
		  (u32) readl(np->base + RxIntrCtrl));
		rc = 0;
	}

	else {
		spin_lock_irq(&np->lock);
		rc = generic_mii_ioctl(&np->mii_if, data, cmd, NULL);
		spin_unlock_irq(&np->lock);
	}

	return rc;
}


static void __devexit hamachi_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (dev) {
		struct hamachi_private *hmp = netdev_priv(dev);

		pci_free_consistent(pdev, RX_TOTAL_SIZE, hmp->rx_ring,
			hmp->rx_ring_dma);
		pci_free_consistent(pdev, TX_TOTAL_SIZE, hmp->tx_ring,
			hmp->tx_ring_dma);
		unregister_netdev(dev);
		iounmap(hmp->base);
		free_netdev(dev);
		pci_release_regions(pdev);
		pci_set_drvdata(pdev, NULL);
	}
}

static DEFINE_PCI_DEVICE_TABLE(hamachi_pci_tbl) = {
	{ 0x1318, 0x0911, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, hamachi_pci_tbl);

static struct pci_driver hamachi_driver = {
	.name		= DRV_NAME,
	.id_table	= hamachi_pci_tbl,
	.probe		= hamachi_init_one,
	.remove		= __devexit_p(hamachi_remove_one),
};

static int __init hamachi_init (void)
{
#ifdef MODULE
	printk(version);
#endif
	return pci_register_driver(&hamachi_driver);
}

static void __exit hamachi_exit (void)
{
	pci_unregister_driver(&hamachi_driver);
}


module_init(hamachi_init);
module_exit(hamachi_exit);
