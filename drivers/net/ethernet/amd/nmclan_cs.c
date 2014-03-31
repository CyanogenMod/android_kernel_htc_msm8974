/* ----------------------------------------------------------------------------
Linux PCMCIA ethernet adapter driver for the New Media Ethernet LAN.
  nmclan_cs.c,v 0.16 1995/07/01 06:42:17 rpao Exp rpao

  The Ethernet LAN uses the Advanced Micro Devices (AMD) Am79C940 Media
  Access Controller for Ethernet (MACE).  It is essentially the Am2150
  PCMCIA Ethernet card contained in the Am2150 Demo Kit.

Written by Roger C. Pao <rpao@paonet.org>
  Copyright 1995 Roger C. Pao
  Linux 2.5 cleanups Copyright Red Hat 2003

  This software may be used and distributed according to the terms of
  the GNU General Public License.

Ported to Linux 1.3.* network driver environment by
  Matti Aarnio <mea@utu.fi>

References

  Am2150 Technical Reference Manual, Revision 1.0, August 17, 1993
  Am79C940 (MACE) Data Sheet, 1994
  Am79C90 (C-LANCE) Data Sheet, 1994
  Linux PCMCIA Programmer's Guide v1.17
  /usr/src/linux/net/inet/dev.c, Linux kernel 1.2.8

  Eric Mears, New Media Corporation
  Tom Pollard, New Media Corporation
  Dean Siasoyco, New Media Corporation
  Ken Lesniak, Silicon Graphics, Inc. <lesniak@boston.sgi.com>
  Donald Becker <becker@scyld.com>
  David Hinds <dahinds@users.sourceforge.net>

  The Linux client driver is based on the 3c589_cs.c client driver by
  David Hinds.

  The Linux network driver outline is based on the 3c589_cs.c driver,
  the 8390.c driver, and the example skeleton.c kernel code, which are
  by Donald Becker.

  The Am2150 network driver hardware interface code is based on the
  OS/9000 driver for the New Media Ethernet LAN by Eric Mears.

  Special thanks for testing and help in debugging this driver goes
  to Ken Lesniak.

-------------------------------------------------------------------------------
Driver Notes and Issues
-------------------------------------------------------------------------------

1. Developed on a Dell 320SLi
   PCMCIA Card Services 2.6.2
   Linux dell 1.2.10 #1 Thu Jun 29 20:23:41 PDT 1995 i386

2. rc.pcmcia may require loading pcmcia_core with io_speed=300:
   'insmod pcmcia_core.o io_speed=300'.
   This will avoid problems with fast systems which causes rx_framecnt
   to return random values.

3. If hot extraction does not work for you, use 'ifconfig eth0 down'
   before extraction.

4. There is a bad slow-down problem in this driver.

5. Future: Multicast processing.  In the meantime, do _not_ compile your
   kernel with multicast ip enabled.

-------------------------------------------------------------------------------
History
-------------------------------------------------------------------------------
Log: nmclan_cs.c,v
 * 2.5.75-ac1 2003/07/11 Alan Cox <alan@lxorguk.ukuu.org.uk>
 * Fixed hang on card eject as we probe it
 * Cleaned up to use new style locking.
 *
 * Revision 0.16  1995/07/01  06:42:17  rpao
 * Bug fix: nmclan_reset() called CardServices incorrectly.
 *
 * Revision 0.15  1995/05/24  08:09:47  rpao
 * Re-implement MULTI_TX dev->tbusy handling.
 *
 * Revision 0.14  1995/05/23  03:19:30  rpao
 * Added, in nmclan_config(), "tuple.Attributes = 0;".
 * Modified MACE ID check to ignore chip revision level.
 * Avoid tx_free_frames race condition between _start_xmit and _interrupt.
 *
 * Revision 0.13  1995/05/18  05:56:34  rpao
 * Statistics changes.
 * Bug fix: nmclan_reset did not enable TX and RX: call restore_multicast_list.
 * Bug fix: mace_interrupt checks ~MACE_IMR_DEFAULT.  Fixes driver lockup.
 *
 * Revision 0.12  1995/05/14  00:12:23  rpao
 * Statistics overhaul.
 *

95/05/13 rpao	V0.10a
		Bug fix: MACE statistics counters used wrong I/O ports.
		Bug fix: mace_interrupt() needed to allow statistics to be
		processed without RX or TX interrupts pending.
95/05/11 rpao	V0.10
		Multiple transmit request processing.
		Modified statistics to use MACE counters where possible.
95/05/10 rpao	V0.09 Bug fix: Must use IO_DATA_PATH_WIDTH_AUTO.
		*Released
95/05/10 rpao	V0.08
		Bug fix: Make all non-exported functions private by using
		static keyword.
		Bug fix: Test IntrCnt _before_ reading MACE_IR.
95/05/10 rpao	V0.07 Statistics.
95/05/09 rpao	V0.06 Fix rx_framecnt problem by addition of PCIC wait states.

---------------------------------------------------------------------------- */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define DRV_NAME	"nmclan_cs"
#define DRV_VERSION	"0.16"



#define MULTI_TX			0
#define RESET_ON_TIMEOUT		1
#define TX_INTERRUPTABLE		1
#define RESET_XILINX			0


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/bitops.h>

#include <pcmcia/cisreg.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#include <asm/uaccess.h>
#include <asm/io.h>


#define MACE_LADRF_LEN			8
					

#define MACE_MAX_IR_ITERATIONS		10
#define MACE_MAX_RX_ITERATIONS		12

#define AM2150_MAX_TX_FRAMES		4
#define AM2150_MAX_RX_FRAMES		12

#define AM2150_RCV			0x00
#define AM2150_XMT			0x04
#define AM2150_XMT_SKIP			0x09
#define AM2150_RCV_NEXT			0x0A
#define AM2150_RCV_FRAME_COUNT		0x0B
#define AM2150_MACE_BANK		0x0C
#define AM2150_MACE_BASE		0x10

#define MACE_RCVFIFO			0
#define MACE_XMTFIFO			1
#define MACE_XMTFC			2
#define MACE_XMTFS			3
#define MACE_XMTRC			4
#define MACE_RCVFC			5
#define MACE_RCVFS			6
#define MACE_FIFOFC			7
#define MACE_IR				8
#define MACE_IMR			9
#define MACE_PR				10
#define MACE_BIUCC			11
#define MACE_FIFOCC			12
#define MACE_MACCC			13
#define MACE_PLSCC			14
#define MACE_PHYCC			15
#define MACE_CHIPIDL			16
#define MACE_CHIPIDH			17
#define MACE_IAC			18
#define MACE_LADRF			20
#define MACE_PADR			21
#define MACE_MPC			24
#define MACE_RNTPC			26
#define MACE_RCVCC			27
#define MACE_UTR			29
#define MACE_RTR1			30
#define MACE_RTR2			31

#define MACE_XMTRC_EXDEF		0x80
#define MACE_XMTRC_XMTRC		0x0F

#define MACE_XMTFS_XMTSV		0x80
#define MACE_XMTFS_UFLO			0x40
#define MACE_XMTFS_LCOL			0x20
#define MACE_XMTFS_MORE			0x10
#define MACE_XMTFS_ONE			0x08
#define MACE_XMTFS_DEFER		0x04
#define MACE_XMTFS_LCAR			0x02
#define MACE_XMTFS_RTRY			0x01

#define MACE_RCVFS_RCVSTS		0xF000
#define MACE_RCVFS_OFLO			0x8000
#define MACE_RCVFS_CLSN			0x4000
#define MACE_RCVFS_FRAM			0x2000
#define MACE_RCVFS_FCS			0x1000

#define MACE_FIFOFC_RCVFC		0xF0
#define MACE_FIFOFC_XMTFC		0x0F

#define MACE_IR_JAB			0x80
#define MACE_IR_BABL			0x40
#define MACE_IR_CERR			0x20
#define MACE_IR_RCVCCO			0x10
#define MACE_IR_RNTPCO			0x08
#define MACE_IR_MPCO			0x04
#define MACE_IR_RCVINT			0x02
#define MACE_IR_XMTINT			0x01

#define MACE_MACCC_PROM			0x80
#define MACE_MACCC_DXMT2PD		0x40
#define MACE_MACCC_EMBA			0x20
#define MACE_MACCC_RESERVED		0x10
#define MACE_MACCC_DRCVPA		0x08
#define MACE_MACCC_DRCVBC		0x04
#define MACE_MACCC_ENXMT		0x02
#define MACE_MACCC_ENRCV		0x01

#define MACE_PHYCC_LNKFL		0x80
#define MACE_PHYCC_DLNKTST		0x40
#define MACE_PHYCC_REVPOL		0x20
#define MACE_PHYCC_DAPC			0x10
#define MACE_PHYCC_LRT			0x08
#define MACE_PHYCC_ASEL			0x04
#define MACE_PHYCC_RWAKE		0x02
#define MACE_PHYCC_AWAKE		0x01

#define MACE_IAC_ADDRCHG		0x80
#define MACE_IAC_PHYADDR		0x04
#define MACE_IAC_LOGADDR		0x02

#define MACE_UTR_RTRE			0x80
#define MACE_UTR_RTRD			0x40
#define MACE_UTR_RPA			0x20
#define MACE_UTR_FCOLL			0x10
#define MACE_UTR_RCVFCSE		0x08
#define MACE_UTR_LOOP_INCL_MENDEC	0x06
#define MACE_UTR_LOOP_NO_MENDEC		0x04
#define MACE_UTR_LOOP_EXTERNAL		0x02
#define MACE_UTR_LOOP_NONE		0x00
#define MACE_UTR_RESERVED		0x01

#define MACEBANK(win_num) outb((win_num), ioaddr + AM2150_MACE_BANK)

#define MACE_IMR_DEFAULT \
  (0xFF - \
    ( \
      MACE_IR_CERR | \
      MACE_IR_RCVCCO | \
      MACE_IR_RNTPCO | \
      MACE_IR_MPCO | \
      MACE_IR_RCVINT | \
      MACE_IR_XMTINT \
    ) \
  )
#undef MACE_IMR_DEFAULT
#define MACE_IMR_DEFAULT 0x00 

#define TX_TIMEOUT		((400*HZ)/1000)


typedef struct _mace_statistics {
    
    int xmtsv;
    int uflo;
    int lcol;
    int more;
    int one;
    int defer;
    int lcar;
    int rtry;

    
    int exdef;
    int xmtrc;

    
    int oflo;
    int clsn;
    int fram;
    int fcs;

    
    int rfs_rntpc;

    
    int rfs_rcvcc;

    
    int jab;
    int babl;
    int cerr;
    int rcvcco;
    int rntpco;
    int mpco;

    
    int mpc;

    
    int rntpc;

    
    int rcvcc;
} mace_statistics;

typedef struct _mace_private {
	struct pcmcia_device	*p_dev;
    struct net_device_stats linux_stats; 
    mace_statistics mace_stats; 

    
    int multicast_ladrf[MACE_LADRF_LEN]; 
    int multicast_num_addrs;

    char tx_free_frames; 
    char tx_irq_disabled; 
    
    spinlock_t bank_lock; 
} mace_private;


static const char *if_names[]={
    "Auto", "10baseT", "BNC",
};


MODULE_DESCRIPTION("New Media PCMCIA ethernet driver");
MODULE_LICENSE("GPL");

#define INT_MODULE_PARM(n, v) static int n = v; module_param(n, int, 0)

INT_MODULE_PARM(if_port, 0);



static int nmclan_config(struct pcmcia_device *link);
static void nmclan_release(struct pcmcia_device *link);

static void nmclan_reset(struct net_device *dev);
static int mace_config(struct net_device *dev, struct ifmap *map);
static int mace_open(struct net_device *dev);
static int mace_close(struct net_device *dev);
static netdev_tx_t mace_start_xmit(struct sk_buff *skb,
					 struct net_device *dev);
static void mace_tx_timeout(struct net_device *dev);
static irqreturn_t mace_interrupt(int irq, void *dev_id);
static struct net_device_stats *mace_get_stats(struct net_device *dev);
static int mace_rx(struct net_device *dev, unsigned char RxCnt);
static void restore_multicast_list(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static const struct ethtool_ops netdev_ethtool_ops;


static void nmclan_detach(struct pcmcia_device *p_dev);

static const struct net_device_ops mace_netdev_ops = {
	.ndo_open		= mace_open,
	.ndo_stop		= mace_close,
	.ndo_start_xmit		= mace_start_xmit,
	.ndo_tx_timeout		= mace_tx_timeout,
	.ndo_set_config		= mace_config,
	.ndo_get_stats		= mace_get_stats,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static int nmclan_probe(struct pcmcia_device *link)
{
    mace_private *lp;
    struct net_device *dev;

    dev_dbg(&link->dev, "nmclan_attach()\n");

    
    dev = alloc_etherdev(sizeof(mace_private));
    if (!dev)
	    return -ENOMEM;
    lp = netdev_priv(dev);
    lp->p_dev = link;
    link->priv = dev;
    
    spin_lock_init(&lp->bank_lock);
    link->resource[0]->end = 32;
    link->resource[0]->flags |= IO_DATA_PATH_WIDTH_AUTO;
    link->config_flags |= CONF_ENABLE_IRQ;
    link->config_index = 1;
    link->config_regs = PRESENT_OPTION;

    lp->tx_free_frames=AM2150_MAX_TX_FRAMES;

    dev->netdev_ops = &mace_netdev_ops;
    SET_ETHTOOL_OPS(dev, &netdev_ethtool_ops);
    dev->watchdog_timeo = TX_TIMEOUT;

    return nmclan_config(link);
} 

static void nmclan_detach(struct pcmcia_device *link)
{
    struct net_device *dev = link->priv;

    dev_dbg(&link->dev, "nmclan_detach\n");

    unregister_netdev(dev);

    nmclan_release(link);

    free_netdev(dev);
} 

static int mace_read(mace_private *lp, unsigned int ioaddr, int reg)
{
  int data = 0xFF;
  unsigned long flags;

  switch (reg >> 4) {
    case 0: 
      data = inb(ioaddr + AM2150_MACE_BASE + reg);
      break;
    case 1: 
      spin_lock_irqsave(&lp->bank_lock, flags);
      MACEBANK(1);
      data = inb(ioaddr + AM2150_MACE_BASE + (reg & 0x0F));
      MACEBANK(0);
      spin_unlock_irqrestore(&lp->bank_lock, flags);
      break;
  }
  return data & 0xFF;
} 

static void mace_write(mace_private *lp, unsigned int ioaddr, int reg,
		       int data)
{
  unsigned long flags;

  switch (reg >> 4) {
    case 0: 
      outb(data & 0xFF, ioaddr + AM2150_MACE_BASE + reg);
      break;
    case 1: 
      spin_lock_irqsave(&lp->bank_lock, flags);
      MACEBANK(1);
      outb(data & 0xFF, ioaddr + AM2150_MACE_BASE + (reg & 0x0F));
      MACEBANK(0);
      spin_unlock_irqrestore(&lp->bank_lock, flags);
      break;
  }
} 

static int mace_init(mace_private *lp, unsigned int ioaddr, char *enet_addr)
{
  int i;
  int ct = 0;

  
  mace_write(lp, ioaddr, MACE_BIUCC, 1);
  while (mace_read(lp, ioaddr, MACE_BIUCC) & 0x01) {
    ;
    if(++ct > 500)
    {
	pr_err("reset failed, card removed?\n");
    	return -1;
    }
    udelay(1);
  }
  mace_write(lp, ioaddr, MACE_BIUCC, 0);

  
  mace_write(lp, ioaddr, MACE_FIFOCC, 0x0F);

  mace_write(lp,ioaddr, MACE_RCVFC, 0); 
  mace_write(lp, ioaddr, MACE_IMR, 0xFF); 

  switch (if_port) {
    case 1:
      mace_write(lp, ioaddr, MACE_PLSCC, 0x02);
      break;
    case 2:
      mace_write(lp, ioaddr, MACE_PLSCC, 0x00);
      break;
    default:
      mace_write(lp, ioaddr, MACE_PHYCC,  4);
      break;
  }

  mace_write(lp, ioaddr, MACE_IAC, MACE_IAC_ADDRCHG | MACE_IAC_PHYADDR);
  
  ct = 0;
  while (mace_read(lp, ioaddr, MACE_IAC) & MACE_IAC_ADDRCHG)
  {
  	if(++ ct > 500)
  	{
		pr_err("ADDRCHG timeout, card removed?\n");
  		return -1;
  	}
  }
  
  for (i = 0; i < ETH_ALEN; i++)
    mace_write(lp, ioaddr, MACE_PADR, enet_addr[i]);

  /* MAC Configuration Control Register should be written last */
  
  
  mace_write(lp, ioaddr, MACE_MACCC, 0x00);
  return 0;
} 

static int nmclan_config(struct pcmcia_device *link)
{
  struct net_device *dev = link->priv;
  mace_private *lp = netdev_priv(dev);
  u8 *buf;
  size_t len;
  int i, ret;
  unsigned int ioaddr;

  dev_dbg(&link->dev, "nmclan_config\n");

  link->io_lines = 5;
  ret = pcmcia_request_io(link);
  if (ret)
	  goto failed;
  ret = pcmcia_request_exclusive_irq(link, mace_interrupt);
  if (ret)
	  goto failed;
  ret = pcmcia_enable_device(link);
  if (ret)
	  goto failed;

  dev->irq = link->irq;
  dev->base_addr = link->resource[0]->start;

  ioaddr = dev->base_addr;

  
  len = pcmcia_get_tuple(link, 0x80, &buf);
  if (!buf || len < ETH_ALEN) {
	  kfree(buf);
	  goto failed;
  }
  memcpy(dev->dev_addr, buf, ETH_ALEN);
  kfree(buf);

  
  {
    char sig[2];

    sig[0] = mace_read(lp, ioaddr, MACE_CHIPIDL);
    sig[1] = mace_read(lp, ioaddr, MACE_CHIPIDH);
    if ((sig[0] == 0x40) && ((sig[1] & 0x0F) == 0x09)) {
      dev_dbg(&link->dev, "nmclan_cs configured: mace id=%x %x\n",
	    sig[0], sig[1]);
    } else {
      pr_notice("mace id not found: %x %x should be 0x40 0x?9\n",
		sig[0], sig[1]);
      return -ENODEV;
    }
  }

  if(mace_init(lp, ioaddr, dev->dev_addr) == -1)
  	goto failed;

  
  if (if_port <= 2)
    dev->if_port = if_port;
  else
    pr_notice("invalid if_port requested\n");

  SET_NETDEV_DEV(dev, &link->dev);

  i = register_netdev(dev);
  if (i != 0) {
    pr_notice("register_netdev() failed\n");
    goto failed;
  }

  netdev_info(dev, "nmclan: port %#3lx, irq %d, %s port, hw_addr %pM\n",
	      dev->base_addr, dev->irq, if_names[dev->if_port], dev->dev_addr);
  return 0;

failed:
	nmclan_release(link);
	return -ENODEV;
} 

static void nmclan_release(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "nmclan_release\n");
	pcmcia_disable_device(link);
}

static int nmclan_suspend(struct pcmcia_device *link)
{
	struct net_device *dev = link->priv;

	if (link->open)
		netif_device_detach(dev);

	return 0;
}

static int nmclan_resume(struct pcmcia_device *link)
{
	struct net_device *dev = link->priv;

	if (link->open) {
		nmclan_reset(dev);
		netif_device_attach(dev);
	}

	return 0;
}


static void nmclan_reset(struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);

#if RESET_XILINX
  struct pcmcia_device *link = &lp->link;
  u8 OrigCorValue;

  
  pcmcia_read_config_byte(link, CISREG_COR, &OrigCorValue);

  
  dev_dbg(&link->dev, "nmclan_reset: OrigCorValue=0x%x, resetting...\n",
	OrigCorValue);
  pcmcia_write_config_byte(link, CISREG_COR, COR_SOFT_RESET);
  

  
  pcmcia_write_config_byte(link, CISREG_COR,
			  (COR_LEVEL_REQ | (OrigCorValue & COR_CONFIG_MASK)));
  
  lp->tx_free_frames=AM2150_MAX_TX_FRAMES;

#endif 

  
  lp->tx_free_frames=AM2150_MAX_TX_FRAMES;

  
  mace_init(lp, dev->base_addr, dev->dev_addr);
  mace_write(lp, dev->base_addr, MACE_IMR, MACE_IMR_DEFAULT);

  
  restore_multicast_list(dev);
} 

static int mace_config(struct net_device *dev, struct ifmap *map)
{
  if ((map->port != (u_char)(-1)) && (map->port != dev->if_port)) {
    if (map->port <= 2) {
      dev->if_port = map->port;
      netdev_info(dev, "switched to %s port\n", if_names[dev->if_port]);
    } else
      return -EINVAL;
  }
  return 0;
} 

static int mace_open(struct net_device *dev)
{
  unsigned int ioaddr = dev->base_addr;
  mace_private *lp = netdev_priv(dev);
  struct pcmcia_device *link = lp->p_dev;

  if (!pcmcia_dev_present(link))
    return -ENODEV;

  link->open++;

  MACEBANK(0);

  netif_start_queue(dev);
  nmclan_reset(dev);

  return 0; 
} 

static int mace_close(struct net_device *dev)
{
  unsigned int ioaddr = dev->base_addr;
  mace_private *lp = netdev_priv(dev);
  struct pcmcia_device *link = lp->p_dev;

  dev_dbg(&link->dev, "%s: shutting down ethercard.\n", dev->name);

  
  outb(0xFF, ioaddr + AM2150_MACE_BASE + MACE_IMR);

  link->open--;
  netif_stop_queue(dev);

  return 0;
} 

static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	snprintf(info->bus_info, sizeof(info->bus_info),
		"PCMCIA 0x%lx", dev->base_addr);
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
};


static void mace_tx_timeout(struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);
  struct pcmcia_device *link = lp->p_dev;

  netdev_notice(dev, "transmit timed out -- ");
#if RESET_ON_TIMEOUT
  pr_cont("resetting card\n");
  pcmcia_reset_card(link->socket);
#else 
  pr_cont("NOT resetting card\n");
#endif 
  dev->trans_start = jiffies; 
  netif_wake_queue(dev);
}

static netdev_tx_t mace_start_xmit(struct sk_buff *skb,
					 struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);
  unsigned int ioaddr = dev->base_addr;

  netif_stop_queue(dev);

  pr_debug("%s: mace_start_xmit(length = %ld) called.\n",
	dev->name, (long)skb->len);

#if (!TX_INTERRUPTABLE)
  
  outb(MACE_IMR_DEFAULT | MACE_IR_XMTINT,
    ioaddr + AM2150_MACE_BASE + MACE_IMR);
  lp->tx_irq_disabled=1;
#endif 

  {

    lp->linux_stats.tx_bytes += skb->len;
    lp->tx_free_frames--;

    /* WARNING: Write the _exact_ number of bytes written in the header! */
    
    outw(skb->len, ioaddr + AM2150_XMT);
    
    outsw(ioaddr + AM2150_XMT, skb->data, skb->len >> 1);
    if (skb->len & 1) {
      
      outb(skb->data[skb->len-1], ioaddr + AM2150_XMT);
    }

#if MULTI_TX
    if (lp->tx_free_frames > 0)
      netif_start_queue(dev);
#endif 
  }

#if (!TX_INTERRUPTABLE)
  
  lp->tx_irq_disabled=0;
  outb(MACE_IMR_DEFAULT, ioaddr + AM2150_MACE_BASE + MACE_IMR);
#endif 

  dev_kfree_skb(skb);

  return NETDEV_TX_OK;
} 

static irqreturn_t mace_interrupt(int irq, void *dev_id)
{
  struct net_device *dev = (struct net_device *) dev_id;
  mace_private *lp = netdev_priv(dev);
  unsigned int ioaddr;
  int status;
  int IntrCnt = MACE_MAX_IR_ITERATIONS;

  if (dev == NULL) {
    pr_debug("mace_interrupt(): irq 0x%X for unknown device.\n",
	  irq);
    return IRQ_NONE;
  }

  ioaddr = dev->base_addr;

  if (lp->tx_irq_disabled) {
    const char *msg;
    if (lp->tx_irq_disabled)
      msg = "Interrupt with tx_irq_disabled";
    else
      msg = "Re-entering the interrupt handler";
    netdev_notice(dev, "%s [isr=%02X, imr=%02X]\n",
		  msg,
		  inb(ioaddr + AM2150_MACE_BASE + MACE_IR),
		  inb(ioaddr + AM2150_MACE_BASE + MACE_IMR));
    
    return IRQ_NONE;
  }

  if (!netif_device_present(dev)) {
    netdev_dbg(dev, "interrupt from dead card\n");
    return IRQ_NONE;
  }

  do {
    
    status = inb(ioaddr + AM2150_MACE_BASE + MACE_IR);

    pr_debug("mace_interrupt: irq 0x%X status 0x%X.\n", irq, status);

    if (status & MACE_IR_RCVINT) {
      mace_rx(dev, MACE_MAX_RX_ITERATIONS);
    }

    if (status & MACE_IR_XMTINT) {
      unsigned char fifofc;
      unsigned char xmtrc;
      unsigned char xmtfs;

      fifofc = inb(ioaddr + AM2150_MACE_BASE + MACE_FIFOFC);
      if ((fifofc & MACE_FIFOFC_XMTFC)==0) {
	lp->linux_stats.tx_errors++;
	outb(0xFF, ioaddr + AM2150_XMT_SKIP);
      }

      
      xmtrc = inb(ioaddr + AM2150_MACE_BASE + MACE_XMTRC);
      if (xmtrc & MACE_XMTRC_EXDEF) lp->mace_stats.exdef++;
      lp->mace_stats.xmtrc += (xmtrc & MACE_XMTRC_XMTRC);

      if (
        (xmtfs = inb(ioaddr + AM2150_MACE_BASE + MACE_XMTFS)) &
        MACE_XMTFS_XMTSV 
      ) {
	lp->mace_stats.xmtsv++;

	if (xmtfs & ~MACE_XMTFS_XMTSV) {
	  if (xmtfs & MACE_XMTFS_UFLO) {
	    lp->mace_stats.uflo++;
	  }
	  if (xmtfs & MACE_XMTFS_LCOL) {
	    
	    lp->mace_stats.lcol++;
	  }
	  if (xmtfs & MACE_XMTFS_MORE) {
	    
	    lp->mace_stats.more++;
	  }
	  if (xmtfs & MACE_XMTFS_ONE) {
	    
	    lp->mace_stats.one++;
	  }
	  if (xmtfs & MACE_XMTFS_DEFER) {
	    
	    lp->mace_stats.defer++;
	  }
	  if (xmtfs & MACE_XMTFS_LCAR) {
	    
	    lp->mace_stats.lcar++;
	  }
	  if (xmtfs & MACE_XMTFS_RTRY) {
	    
	    lp->mace_stats.rtry++;
	  }
        } 

      } 

      lp->linux_stats.tx_packets++;
      lp->tx_free_frames++;
      netif_wake_queue(dev);
    } 

    if (status & ~MACE_IMR_DEFAULT & ~MACE_IR_RCVINT & ~MACE_IR_XMTINT) {
      if (status & MACE_IR_JAB) {
        
        lp->mace_stats.jab++;
      }
      if (status & MACE_IR_BABL) {
        
        lp->mace_stats.babl++;
      }
      if (status & MACE_IR_CERR) {
        lp->mace_stats.cerr++;
      }
      if (status & MACE_IR_RCVCCO) {
        
        lp->mace_stats.rcvcco++;
      }
      if (status & MACE_IR_RNTPCO) {
        
        lp->mace_stats.rntpco++;
      }
      if (status & MACE_IR_MPCO) {
        
        lp->mace_stats.mpco++;
      }
    } 

  } while ((status & ~MACE_IMR_DEFAULT) && (--IntrCnt));

  return IRQ_HANDLED;
} 

static int mace_rx(struct net_device *dev, unsigned char RxCnt)
{
  mace_private *lp = netdev_priv(dev);
  unsigned int ioaddr = dev->base_addr;
  unsigned char rx_framecnt;
  unsigned short rx_status;

  while (
    ((rx_framecnt = inb(ioaddr + AM2150_RCV_FRAME_COUNT)) > 0) &&
    (rx_framecnt <= 12) && 
    (RxCnt--)
  ) {
    rx_status = inw(ioaddr + AM2150_RCV);

    pr_debug("%s: in mace_rx(), framecnt 0x%X, rx_status"
	  " 0x%X.\n", dev->name, rx_framecnt, rx_status);

    if (rx_status & MACE_RCVFS_RCVSTS) { 
      lp->linux_stats.rx_errors++;
      if (rx_status & MACE_RCVFS_OFLO) {
        lp->mace_stats.oflo++;
      }
      if (rx_status & MACE_RCVFS_CLSN) {
        lp->mace_stats.clsn++;
      }
      if (rx_status & MACE_RCVFS_FRAM) {
	lp->mace_stats.fram++;
      }
      if (rx_status & MACE_RCVFS_FCS) {
        lp->mace_stats.fcs++;
      }
    } else {
      short pkt_len = (rx_status & ~MACE_RCVFS_RCVSTS) - 4;
        
      struct sk_buff *skb;

      lp->mace_stats.rfs_rntpc += inb(ioaddr + AM2150_RCV);
        
      lp->mace_stats.rfs_rcvcc += inb(ioaddr + AM2150_RCV);
        

      pr_debug("    receiving packet size 0x%X rx_status"
	    " 0x%X.\n", pkt_len, rx_status);

      skb = netdev_alloc_skb(dev, pkt_len + 2);

      if (skb != NULL) {
	skb_reserve(skb, 2);
	insw(ioaddr + AM2150_RCV, skb_put(skb, pkt_len), pkt_len>>1);
	if (pkt_len & 1)
	    *(skb_tail_pointer(skb) - 1) = inb(ioaddr + AM2150_RCV);
	skb->protocol = eth_type_trans(skb, dev);
	
	netif_rx(skb); 

	lp->linux_stats.rx_packets++;
	lp->linux_stats.rx_bytes += pkt_len;
	outb(0xFF, ioaddr + AM2150_RCV_NEXT); 
	continue;
      } else {
	pr_debug("%s: couldn't allocate a sk_buff of size"
	      " %d.\n", dev->name, pkt_len);
	lp->linux_stats.rx_dropped++;
      }
    }
    outb(0xFF, ioaddr + AM2150_RCV_NEXT); 
  } 

  return 0;
} 

static void pr_linux_stats(struct net_device_stats *pstats)
{
  pr_debug("pr_linux_stats\n");
  pr_debug(" rx_packets=%-7ld        tx_packets=%ld\n",
	(long)pstats->rx_packets, (long)pstats->tx_packets);
  pr_debug(" rx_errors=%-7ld         tx_errors=%ld\n",
	(long)pstats->rx_errors, (long)pstats->tx_errors);
  pr_debug(" rx_dropped=%-7ld        tx_dropped=%ld\n",
	(long)pstats->rx_dropped, (long)pstats->tx_dropped);
  pr_debug(" multicast=%-7ld         collisions=%ld\n",
	(long)pstats->multicast, (long)pstats->collisions);

  pr_debug(" rx_length_errors=%-7ld  rx_over_errors=%ld\n",
	(long)pstats->rx_length_errors, (long)pstats->rx_over_errors);
  pr_debug(" rx_crc_errors=%-7ld     rx_frame_errors=%ld\n",
	(long)pstats->rx_crc_errors, (long)pstats->rx_frame_errors);
  pr_debug(" rx_fifo_errors=%-7ld    rx_missed_errors=%ld\n",
	(long)pstats->rx_fifo_errors, (long)pstats->rx_missed_errors);

  pr_debug(" tx_aborted_errors=%-7ld tx_carrier_errors=%ld\n",
	(long)pstats->tx_aborted_errors, (long)pstats->tx_carrier_errors);
  pr_debug(" tx_fifo_errors=%-7ld    tx_heartbeat_errors=%ld\n",
	(long)pstats->tx_fifo_errors, (long)pstats->tx_heartbeat_errors);
  pr_debug(" tx_window_errors=%ld\n",
	(long)pstats->tx_window_errors);
} 

static void pr_mace_stats(mace_statistics *pstats)
{
  pr_debug("pr_mace_stats\n");

  pr_debug(" xmtsv=%-7d             uflo=%d\n",
	pstats->xmtsv, pstats->uflo);
  pr_debug(" lcol=%-7d              more=%d\n",
	pstats->lcol, pstats->more);
  pr_debug(" one=%-7d               defer=%d\n",
	pstats->one, pstats->defer);
  pr_debug(" lcar=%-7d              rtry=%d\n",
	pstats->lcar, pstats->rtry);

  
  pr_debug(" exdef=%-7d             xmtrc=%d\n",
	pstats->exdef, pstats->xmtrc);

  
  pr_debug(" oflo=%-7d              clsn=%d\n",
	pstats->oflo, pstats->clsn);
  pr_debug(" fram=%-7d              fcs=%d\n",
	pstats->fram, pstats->fcs);

  
  
  pr_debug(" rfs_rntpc=%-7d         rfs_rcvcc=%d\n",
	pstats->rfs_rntpc, pstats->rfs_rcvcc);

  
  pr_debug(" jab=%-7d               babl=%d\n",
	pstats->jab, pstats->babl);
  pr_debug(" cerr=%-7d              rcvcco=%d\n",
	pstats->cerr, pstats->rcvcco);
  pr_debug(" rntpco=%-7d            mpco=%d\n",
	pstats->rntpco, pstats->mpco);

  
  pr_debug(" mpc=%d\n", pstats->mpc);

  
  pr_debug(" rntpc=%d\n", pstats->rntpc);

  
  pr_debug(" rcvcc=%d\n", pstats->rcvcc);

} 

static void update_stats(unsigned int ioaddr, struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);

  lp->mace_stats.rcvcc += mace_read(lp, ioaddr, MACE_RCVCC);
  lp->mace_stats.rntpc += mace_read(lp, ioaddr, MACE_RNTPC);
  lp->mace_stats.mpc += mace_read(lp, ioaddr, MACE_MPC);


  
  lp->linux_stats.collisions = 
    lp->mace_stats.rcvcco * 256 + lp->mace_stats.rcvcc;

  
  lp->linux_stats.rx_length_errors = 
    lp->mace_stats.rntpco * 256 + lp->mace_stats.rntpc;
  
  lp->linux_stats.rx_crc_errors = lp->mace_stats.fcs;
  lp->linux_stats.rx_frame_errors = lp->mace_stats.fram;
  lp->linux_stats.rx_fifo_errors = lp->mace_stats.oflo;
  lp->linux_stats.rx_missed_errors = 
    lp->mace_stats.mpco * 256 + lp->mace_stats.mpc;

  
  lp->linux_stats.tx_aborted_errors = lp->mace_stats.rtry;
  lp->linux_stats.tx_carrier_errors = lp->mace_stats.lcar;
    
  lp->linux_stats.tx_fifo_errors = lp->mace_stats.uflo;
  lp->linux_stats.tx_heartbeat_errors = lp->mace_stats.cerr;
  
} 

static struct net_device_stats *mace_get_stats(struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);

  update_stats(dev->base_addr, dev);

  pr_debug("%s: updating the statistics.\n", dev->name);
  pr_linux_stats(&lp->linux_stats);
  pr_mace_stats(&lp->mace_stats);

  return &lp->linux_stats;
} 


#ifdef BROKEN_MULTICAST

static void updateCRC(int *CRC, int bit)
{
  static const int poly[]={
    1,1,1,0, 1,1,0,1,
    1,0,1,1, 1,0,0,0,
    1,0,0,0, 0,0,1,1,
    0,0,1,0, 0,0,0,0
  }; 

  int j;

  
  for (j = 32; j > 0; j--)
    CRC[j] = CRC[j-1];
  CRC[0] = 0;

  
  if (bit ^ CRC[32])
    for (j = 0; j < 32; j++)
      CRC[j] ^= poly[j];
} 

static void BuildLAF(int *ladrf, int *adr)
{
  int CRC[33]={1}; 

  int i, byte; 
  int hashcode; 

  CRC[32]=0;

  for (byte = 0; byte < 6; byte++)
    for (i = 0; i < 8; i++)
      updateCRC(CRC, (adr[byte] >> i) & 1);

  hashcode = 0;
  for (i = 0; i < 6; i++)
    hashcode = (hashcode << 1) + CRC[i];

  byte = hashcode >> 3;
  ladrf[byte] |= (1 << (hashcode & 7));

#ifdef PCMCIA_DEBUG
  if (0)
    printk(KERN_DEBUG "    adr =%pM\n", adr);
  printk(KERN_DEBUG "    hashcode = %d(decimal), ladrf[0:63] =", hashcode);
  for (i = 0; i < 8; i++)
    pr_cont(" %02X", ladrf[i]);
  pr_cont("\n");
#endif
} 

static void restore_multicast_list(struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);
  int num_addrs = lp->multicast_num_addrs;
  int *ladrf = lp->multicast_ladrf;
  unsigned int ioaddr = dev->base_addr;
  int i;

  pr_debug("%s: restoring Rx mode to %d addresses.\n",
	dev->name, num_addrs);

  if (num_addrs > 0) {

    pr_debug("Attempt to restore multicast list detected.\n");

    mace_write(lp, ioaddr, MACE_IAC, MACE_IAC_ADDRCHG | MACE_IAC_LOGADDR);
    
    while (mace_read(lp, ioaddr, MACE_IAC) & MACE_IAC_ADDRCHG)
      ;
    
    for (i = 0; i < MACE_LADRF_LEN; i++)
      mace_write(lp, ioaddr, MACE_LADRF, ladrf[i]);

    mace_write(lp, ioaddr, MACE_UTR, MACE_UTR_RCVFCSE | MACE_UTR_LOOP_EXTERNAL);
    mace_write(lp, ioaddr, MACE_MACCC, MACE_MACCC_ENXMT | MACE_MACCC_ENRCV);

  } else if (num_addrs < 0) {

    
    mace_write(lp, ioaddr, MACE_UTR, MACE_UTR_LOOP_EXTERNAL);
    mace_write(lp, ioaddr, MACE_MACCC,
      MACE_MACCC_PROM | MACE_MACCC_ENXMT | MACE_MACCC_ENRCV
    );

  } else {

    
    mace_write(lp, ioaddr, MACE_UTR, MACE_UTR_LOOP_EXTERNAL);
    mace_write(lp, ioaddr, MACE_MACCC, MACE_MACCC_ENXMT | MACE_MACCC_ENRCV);

  }
} 


static void set_multicast_list(struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);
  int adr[ETH_ALEN] = {0}; 
  struct netdev_hw_addr *ha;

#ifdef PCMCIA_DEBUG
  {
    static int old;
    if (netdev_mc_count(dev) != old) {
      old = netdev_mc_count(dev);
      pr_debug("%s: setting Rx mode to %d addresses.\n",
	    dev->name, old);
    }
  }
#endif

  
  lp->multicast_num_addrs = netdev_mc_count(dev);

  
  if (num_addrs > 0) {
    
    memset(lp->multicast_ladrf, 0, MACE_LADRF_LEN);
    netdev_for_each_mc_addr(ha, dev) {
      memcpy(adr, ha->addr, ETH_ALEN);
      BuildLAF(lp->multicast_ladrf, adr);
    }
  }

  restore_multicast_list(dev);

} 

#endif 

static void restore_multicast_list(struct net_device *dev)
{
  unsigned int ioaddr = dev->base_addr;
  mace_private *lp = netdev_priv(dev);

  pr_debug("%s: restoring Rx mode to %d addresses.\n", dev->name,
	lp->multicast_num_addrs);

  if (dev->flags & IFF_PROMISC) {
    
    mace_write(lp,ioaddr, MACE_UTR, MACE_UTR_LOOP_EXTERNAL);
    mace_write(lp, ioaddr, MACE_MACCC,
      MACE_MACCC_PROM | MACE_MACCC_ENXMT | MACE_MACCC_ENRCV
    );
  } else {
    
    mace_write(lp, ioaddr, MACE_UTR, MACE_UTR_LOOP_EXTERNAL);
    mace_write(lp, ioaddr, MACE_MACCC, MACE_MACCC_ENXMT | MACE_MACCC_ENRCV);
  }
} 

static void set_multicast_list(struct net_device *dev)
{
  mace_private *lp = netdev_priv(dev);

#ifdef PCMCIA_DEBUG
  {
    static int old;
    if (netdev_mc_count(dev) != old) {
      old = netdev_mc_count(dev);
      pr_debug("%s: setting Rx mode to %d addresses.\n",
	    dev->name, old);
    }
  }
#endif

  lp->multicast_num_addrs = netdev_mc_count(dev);
  restore_multicast_list(dev);

} 

static const struct pcmcia_device_id nmclan_ids[] = {
	PCMCIA_DEVICE_PROD_ID12("New Media Corporation", "Ethernet", 0x085a850b, 0x00b2e941),
	PCMCIA_DEVICE_PROD_ID12("Portable Add-ons", "Ethernet+", 0xebf1d60, 0xad673aaf),
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, nmclan_ids);

static struct pcmcia_driver nmclan_cs_driver = {
	.owner		= THIS_MODULE,
	.name		= "nmclan_cs",
	.probe		= nmclan_probe,
	.remove		= nmclan_detach,
	.id_table       = nmclan_ids,
	.suspend	= nmclan_suspend,
	.resume		= nmclan_resume,
};

static int __init init_nmclan_cs(void)
{
	return pcmcia_register_driver(&nmclan_cs_driver);
}

static void __exit exit_nmclan_cs(void)
{
	pcmcia_unregister_driver(&nmclan_cs_driver);
}

module_init(init_nmclan_cs);
module_exit(exit_nmclan_cs);
