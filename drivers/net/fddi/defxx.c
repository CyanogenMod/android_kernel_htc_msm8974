/*
 * File Name:
 *   defxx.c
 *
 * Copyright Information:
 *   Copyright Digital Equipment Corporation 1996.
 *
 *   This software may be used and distributed according to the terms of
 *   the GNU General Public License, incorporated herein by reference.
 *
 * Abstract:
 *   A Linux device driver supporting the Digital Equipment Corporation
 *   FDDI TURBOchannel, EISA and PCI controller families.  Supported
 *   adapters include:
 *
 *		DEC FDDIcontroller/TURBOchannel (DEFTA)
 *		DEC FDDIcontroller/EISA         (DEFEA)
 *		DEC FDDIcontroller/PCI          (DEFPA)
 *
 * The original author:
 *   LVS	Lawrence V. Stefani <lstefani@yahoo.com>
 *
 * Maintainers:
 *   macro	Maciej W. Rozycki <macro@linux-mips.org>
 *
 * Credits:
 *   I'd like to thank Patricia Cross for helping me get started with
 *   Linux, David Davies for a lot of help upgrading and configuring
 *   my development system and for answering many OS and driver
 *   development questions, and Alan Cox for recommendations and
 *   integration help on getting FDDI support into Linux.  LVS
 *
 * Driver Architecture:
 *   The driver architecture is largely based on previous driver work
 *   for other operating systems.  The upper edge interface and
 *   functions were largely taken from existing Linux device drivers
 *   such as David Davies' DE4X5.C driver and Donald Becker's TULIP.C
 *   driver.
 *
 *   Adapter Probe -
 *		The driver scans for supported EISA adapters by reading the
 *		SLOT ID register for each EISA slot and making a match
 *		against the expected value.
 *
 *   Bus-Specific Initialization -
 *		This driver currently supports both EISA and PCI controller
 *		families.  While the custom DMA chip and FDDI logic is similar
 *		or identical, the bus logic is very different.  After
 *		initialization, the	only bus-specific differences is in how the
 *		driver enables and disables interrupts.  Other than that, the
 *		run-time critical code behaves the same on both families.
 *		It's important to note that both adapter families are configured
 *		to I/O map, rather than memory map, the adapter registers.
 *
 *   Driver Open/Close -
 *		In the driver open routine, the driver ISR (interrupt service
 *		routine) is registered and the adapter is brought to an
 *		operational state.  In the driver close routine, the opposite
 *		occurs; the driver ISR is deregistered and the adapter is
 *		brought to a safe, but closed state.  Users may use consecutive
 *		commands to bring the adapter up and down as in the following
 *		example:
 *					ifconfig fddi0 up
 *					ifconfig fddi0 down
 *					ifconfig fddi0 up
 *
 *   Driver Shutdown -
 *		Apparently, there is no shutdown or halt routine support under
 *		Linux.  This routine would be called during "reboot" or
 *		"shutdown" to allow the driver to place the adapter in a safe
 *		state before a warm reboot occurs.  To be really safe, the user
 *		should close the adapter before shutdown (eg. ifconfig fddi0 down)
 *		to ensure that the adapter DMA engine is taken off-line.  However,
 *		the current driver code anticipates this problem and always issues
 *		a soft reset of the adapter	at the beginning of driver initialization.
 *		A future driver enhancement in this area may occur in 2.1.X where
 *		Alan indicated that a shutdown handler may be implemented.
 *
 *   Interrupt Service Routine -
 *		The driver supports shared interrupts, so the ISR is registered for
 *		each board with the appropriate flag and the pointer to that board's
 *		device structure.  This provides the context during interrupt
 *		processing to support shared interrupts and multiple boards.
 *
 *		Interrupt enabling/disabling can occur at many levels.  At the host
 *		end, you can disable system interrupts, or disable interrupts at the
 *		PIC (on Intel systems).  Across the bus, both EISA and PCI adapters
 *		have a bus-logic chip interrupt enable/disable as well as a DMA
 *		controller interrupt enable/disable.
 *
 *		The driver currently enables and disables adapter interrupts at the
 *		bus-logic chip and assumes that Linux will take care of clearing or
 *		acknowledging any host-based interrupt chips.
 *
 *   Control Functions -
 *		Control functions are those used to support functions such as adding
 *		or deleting multicast addresses, enabling or disabling packet
 *		reception filters, or other custom/proprietary commands.  Presently,
 *		the driver supports the "get statistics", "set multicast list", and
 *		"set mac address" functions defined by Linux.  A list of possible
 *		enhancements include:
 *
 *				- Custom ioctl interface for executing port interface commands
 *				- Custom ioctl interface for adding unicast addresses to
 *				  adapter CAM (to support bridge functions).
 *				- Custom ioctl interface for supporting firmware upgrades.
 *
 *   Hardware (port interface) Support Routines -
 *		The driver function names that start with "dfx_hw_" represent
 *		low-level port interface routines that are called frequently.  They
 *		include issuing a DMA or port control command to the adapter,
 *		resetting the adapter, or reading the adapter state.  Since the
 *		driver initialization and run-time code must make calls into the
 *		port interface, these routines were written to be as generic and
 *		usable as possible.
 *
 *   Receive Path -
 *		The adapter DMA engine supports a 256 entry receive descriptor block
 *		of which up to 255 entries can be used at any given time.  The
 *		architecture is a standard producer, consumer, completion model in
 *		which the driver "produces" receive buffers to the adapter, the
 *		adapter "consumes" the receive buffers by DMAing incoming packet data,
 *		and the driver "completes" the receive buffers by servicing the
 *		incoming packet, then "produces" a new buffer and starts the cycle
 *		again.  Receive buffers can be fragmented in up to 16 fragments
 *		(descriptor	entries).  For simplicity, this driver posts
 *		single-fragment receive buffers of 4608 bytes, then allocates a
 *		sk_buff, copies the data, then reposts the buffer.  To reduce CPU
 *		utilization, a better approach would be to pass up the receive
 *		buffer (no extra copy) then allocate and post a replacement buffer.
 *		This is a performance enhancement that should be looked into at
 *		some point.
 *
 *   Transmit Path -
 *		Like the receive path, the adapter DMA engine supports a 256 entry
 *		transmit descriptor block of which up to 255 entries can be used at
 *		any	given time.  Transmit buffers can be fragmented	in up to 255
 *		fragments (descriptor entries).  This driver always posts one
 *		fragment per transmit packet request.
 *
 *		The fragment contains the entire packet from FC to end of data.
 *		Before posting the buffer to the adapter, the driver sets a three-byte
 *		packet request header (PRH) which is required by the Motorola MAC chip
 *		used on the adapters.  The PRH tells the MAC the type of token to
 *		receive/send, whether or not to generate and append the CRC, whether
 *		synchronous or asynchronous framing is used, etc.  Since the PRH
 *		definition is not necessarily consistent across all FDDI chipsets,
 *		the driver, rather than the common FDDI packet handler routines,
 *		sets these bytes.
 *
 *		To reduce the amount of descriptor fetches needed per transmit request,
 *		the driver takes advantage of the fact that there are at least three
 *		bytes available before the skb->data field on the outgoing transmit
 *		request.  This is guaranteed by having fddi_setup() in net_init.c set
 *		dev->hard_header_len to 24 bytes.  21 bytes accounts for the largest
 *		header in an 802.2 SNAP frame.  The other 3 bytes are the extra "pad"
 *		bytes which we'll use to store the PRH.
 *
 *		There's a subtle advantage to adding these pad bytes to the
 *		hard_header_len, it ensures that the data portion of the packet for
 *		an 802.2 SNAP frame is longword aligned.  Other FDDI driver
 *		implementations may not need the extra padding and can start copying
 *		or DMAing directly from the FC byte which starts at skb->data.  Should
 *		another driver implementation need ADDITIONAL padding, the net_init.c
 *		module should be updated and dev->hard_header_len should be increased.
 *		NOTE: To maintain the alignment on the data portion of the packet,
 *		dev->hard_header_len should always be evenly divisible by 4 and at
 *		least 24 bytes in size.
 *
 * Modification History:
 *		Date		Name	Description
 *		16-Aug-96	LVS		Created.
 *		20-Aug-96	LVS		Updated dfx_probe so that version information
 *							string is only displayed if 1 or more cards are
 *							found.  Changed dfx_rcv_queue_process to copy
 *							3 NULL bytes before FC to ensure that data is
 *							longword aligned in receive buffer.
 *		09-Sep-96	LVS		Updated dfx_ctl_set_multicast_list to enable
 *							LLC group promiscuous mode if multicast list
 *							is too large.  LLC individual/group promiscuous
 *							mode is now disabled if IFF_PROMISC flag not set.
 *							dfx_xmt_queue_pkt no longer checks for NULL skb
 *							on Alan Cox recommendation.  Added node address
 *							override support.
 *		12-Sep-96	LVS		Reset current address to factory address during
 *							device open.  Updated transmit path to post a
 *							single fragment which includes PRH->end of data.
 *		Mar 2000	AC		Did various cleanups for 2.3.x
 *		Jun 2000	jgarzik		PCI and resource alloc cleanups
 *		Jul 2000	tjeerd		Much cleanup and some bug fixes
 *		Sep 2000	tjeerd		Fix leak on unload, cosmetic code cleanup
 *		Feb 2001			Skb allocation fixes
 *		Feb 2001	davej		PCI enable cleanups.
 *		04 Aug 2003	macro		Converted to the DMA API.
 *		14 Aug 2004	macro		Fix device names reported.
 *		14 Jun 2005	macro		Use irqreturn_t.
 *		23 Oct 2006	macro		Big-endian host support.
 *		14 Dec 2006	macro		TURBOchannel support.
 */

#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/eisa.h>
#include <linux/errno.h>
#include <linux/fddidevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tc.h>

#include <asm/byteorder.h>
#include <asm/io.h>

#include "defxx.h"

#define DRV_NAME "defxx"
#define DRV_VERSION "v1.10"
#define DRV_RELDATE "2006/12/14"

static char version[] __devinitdata =
	DRV_NAME ": " DRV_VERSION " " DRV_RELDATE
	"  Lawrence V. Stefani and others\n";

#define DYNAMIC_BUFFERS 1

#define SKBUFF_RX_COPYBREAK 200
#define NEW_SKB_SIZE (PI_RCV_DATA_K_SIZE_MAX+128)

#ifdef CONFIG_PCI
#define DFX_BUS_PCI(dev) (dev->bus == &pci_bus_type)
#else
#define DFX_BUS_PCI(dev) 0
#endif

#ifdef CONFIG_EISA
#define DFX_BUS_EISA(dev) (dev->bus == &eisa_bus_type)
#else
#define DFX_BUS_EISA(dev) 0
#endif

#ifdef CONFIG_TC
#define DFX_BUS_TC(dev) (dev->bus == &tc_bus_type)
#else
#define DFX_BUS_TC(dev) 0
#endif

#ifdef CONFIG_DEFXX_MMIO
#define DFX_MMIO 1
#else
#define DFX_MMIO 0
#endif


static void		dfx_bus_init(struct net_device *dev);
static void		dfx_bus_uninit(struct net_device *dev);
static void		dfx_bus_config_check(DFX_board_t *bp);

static int		dfx_driver_init(struct net_device *dev,
					const char *print_name,
					resource_size_t bar_start);
static int		dfx_adap_init(DFX_board_t *bp, int get_buffers);

static int		dfx_open(struct net_device *dev);
static int		dfx_close(struct net_device *dev);

static void		dfx_int_pr_halt_id(DFX_board_t *bp);
static void		dfx_int_type_0_process(DFX_board_t *bp);
static void		dfx_int_common(struct net_device *dev);
static irqreturn_t	dfx_interrupt(int irq, void *dev_id);

static struct		net_device_stats *dfx_ctl_get_stats(struct net_device *dev);
static void		dfx_ctl_set_multicast_list(struct net_device *dev);
static int		dfx_ctl_set_mac_address(struct net_device *dev, void *addr);
static int		dfx_ctl_update_cam(DFX_board_t *bp);
static int		dfx_ctl_update_filters(DFX_board_t *bp);

static int		dfx_hw_dma_cmd_req(DFX_board_t *bp);
static int		dfx_hw_port_ctrl_req(DFX_board_t *bp, PI_UINT32	command, PI_UINT32 data_a, PI_UINT32 data_b, PI_UINT32 *host_data);
static void		dfx_hw_adap_reset(DFX_board_t *bp, PI_UINT32 type);
static int		dfx_hw_adap_state_rd(DFX_board_t *bp);
static int		dfx_hw_dma_uninit(DFX_board_t *bp, PI_UINT32 type);

static int		dfx_rcv_init(DFX_board_t *bp, int get_buffers);
static void		dfx_rcv_queue_process(DFX_board_t *bp);
static void		dfx_rcv_flush(DFX_board_t *bp);

static netdev_tx_t dfx_xmt_queue_pkt(struct sk_buff *skb,
				     struct net_device *dev);
static int		dfx_xmt_done(DFX_board_t *bp);
static void		dfx_xmt_flush(DFX_board_t *bp);


static struct pci_driver dfx_pci_driver;
static struct eisa_driver dfx_eisa_driver;
static struct tc_driver dfx_tc_driver;



static inline void dfx_writel(DFX_board_t *bp, int offset, u32 data)
{
	writel(data, bp->base.mem + offset);
	mb();
}

static inline void dfx_outl(DFX_board_t *bp, int offset, u32 data)
{
	outl(data, bp->base.port + offset);
}

static void dfx_port_write_long(DFX_board_t *bp, int offset, u32 data)
{
	struct device __maybe_unused *bdev = bp->bus_dev;
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;

	if (dfx_use_mmio)
		dfx_writel(bp, offset, data);
	else
		dfx_outl(bp, offset, data);
}


static inline void dfx_readl(DFX_board_t *bp, int offset, u32 *data)
{
	mb();
	*data = readl(bp->base.mem + offset);
}

static inline void dfx_inl(DFX_board_t *bp, int offset, u32 *data)
{
	*data = inl(bp->base.port + offset);
}

static void dfx_port_read_long(DFX_board_t *bp, int offset, u32 *data)
{
	struct device __maybe_unused *bdev = bp->bus_dev;
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;

	if (dfx_use_mmio)
		dfx_readl(bp, offset, data);
	else
		dfx_inl(bp, offset, data);
}


static void dfx_get_bars(struct device *bdev,
			 resource_size_t *bar_start, resource_size_t *bar_len)
{
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_eisa = DFX_BUS_EISA(bdev);
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;

	if (dfx_bus_pci) {
		int num = dfx_use_mmio ? 0 : 1;

		*bar_start = pci_resource_start(to_pci_dev(bdev), num);
		*bar_len = pci_resource_len(to_pci_dev(bdev), num);
	}
	if (dfx_bus_eisa) {
		unsigned long base_addr = to_eisa_device(bdev)->base_addr;
		resource_size_t bar;

		if (dfx_use_mmio) {
			bar = inb(base_addr + PI_ESIC_K_MEM_ADD_CMP_2);
			bar <<= 8;
			bar |= inb(base_addr + PI_ESIC_K_MEM_ADD_CMP_1);
			bar <<= 8;
			bar |= inb(base_addr + PI_ESIC_K_MEM_ADD_CMP_0);
			bar <<= 16;
			*bar_start = bar;
			bar = inb(base_addr + PI_ESIC_K_MEM_ADD_MASK_2);
			bar <<= 8;
			bar |= inb(base_addr + PI_ESIC_K_MEM_ADD_MASK_1);
			bar <<= 8;
			bar |= inb(base_addr + PI_ESIC_K_MEM_ADD_MASK_0);
			bar <<= 16;
			*bar_len = (bar | PI_MEM_ADD_MASK_M) + 1;
		} else {
			*bar_start = base_addr;
			*bar_len = PI_ESIC_K_CSR_IO_LEN;
		}
	}
	if (dfx_bus_tc) {
		*bar_start = to_tc_dev(bdev)->resource.start +
			     PI_TC_K_CSR_OFFSET;
		*bar_len = PI_TC_K_CSR_LEN;
	}
}

static const struct net_device_ops dfx_netdev_ops = {
	.ndo_open		= dfx_open,
	.ndo_stop		= dfx_close,
	.ndo_start_xmit		= dfx_xmt_queue_pkt,
	.ndo_get_stats		= dfx_ctl_get_stats,
	.ndo_set_rx_mode	= dfx_ctl_set_multicast_list,
	.ndo_set_mac_address	= dfx_ctl_set_mac_address,
};

static int __devinit dfx_register(struct device *bdev)
{
	static int version_disp;
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;
	const char *print_name = dev_name(bdev);
	struct net_device *dev;
	DFX_board_t	  *bp;			
	resource_size_t bar_start = 0;		
	resource_size_t bar_len = 0;		
	int alloc_size;				
	struct resource *region;
	int err = 0;

	if (!version_disp) {	
		version_disp = 1;	
		printk(version);	
	}

	dev = alloc_fddidev(sizeof(*bp));
	if (!dev) {
		printk(KERN_ERR "%s: Unable to allocate fddidev, aborting\n",
		       print_name);
		return -ENOMEM;
	}

	
	if (dfx_bus_pci && pci_enable_device(to_pci_dev(bdev))) {
		printk(KERN_ERR "%s: Cannot enable PCI device, aborting\n",
		       print_name);
		goto err_out;
	}

	SET_NETDEV_DEV(dev, bdev);

	bp = netdev_priv(dev);
	bp->bus_dev = bdev;
	dev_set_drvdata(bdev, dev);

	dfx_get_bars(bdev, &bar_start, &bar_len);

	if (dfx_use_mmio)
		region = request_mem_region(bar_start, bar_len, print_name);
	else
		region = request_region(bar_start, bar_len, print_name);
	if (!region) {
		printk(KERN_ERR "%s: Cannot reserve I/O resource "
		       "0x%lx @ 0x%lx, aborting\n",
		       print_name, (long)bar_len, (long)bar_start);
		err = -EBUSY;
		goto err_out_disable;
	}

	
	if (dfx_use_mmio) {
		bp->base.mem = ioremap_nocache(bar_start, bar_len);
		if (!bp->base.mem) {
			printk(KERN_ERR "%s: Cannot map MMIO\n", print_name);
			err = -ENOMEM;
			goto err_out_region;
		}
	} else {
		bp->base.port = bar_start;
		dev->base_addr = bar_start;
	}

	
	dev->netdev_ops			= &dfx_netdev_ops;

	if (dfx_bus_pci)
		pci_set_master(to_pci_dev(bdev));

	if (dfx_driver_init(dev, print_name, bar_start) != DFX_K_SUCCESS) {
		err = -ENODEV;
		goto err_out_unmap;
	}

	err = register_netdev(dev);
	if (err)
		goto err_out_kfree;

	printk("%s: registered as %s\n", print_name, dev->name);
	return 0;

err_out_kfree:
	alloc_size = sizeof(PI_DESCR_BLOCK) +
		     PI_CMD_REQ_K_SIZE_MAX + PI_CMD_RSP_K_SIZE_MAX +
#ifndef DYNAMIC_BUFFERS
		     (bp->rcv_bufs_to_post * PI_RCV_DATA_K_SIZE_MAX) +
#endif
		     sizeof(PI_CONSUMER_BLOCK) +
		     (PI_ALIGN_K_DESC_BLK - 1);
	if (bp->kmalloced)
		dma_free_coherent(bdev, alloc_size,
				  bp->kmalloced, bp->kmalloced_dma);

err_out_unmap:
	if (dfx_use_mmio)
		iounmap(bp->base.mem);

err_out_region:
	if (dfx_use_mmio)
		release_mem_region(bar_start, bar_len);
	else
		release_region(bar_start, bar_len);

err_out_disable:
	if (dfx_bus_pci)
		pci_disable_device(to_pci_dev(bdev));

err_out:
	free_netdev(dev);
	return err;
}



static void __devinit dfx_bus_init(struct net_device *dev)
{
	DFX_board_t *bp = netdev_priv(dev);
	struct device *bdev = bp->bus_dev;
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_eisa = DFX_BUS_EISA(bdev);
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;
	u8 val;

	DBG_printk("In dfx_bus_init...\n");

	
	bp->dev = dev;

	

	if (dfx_bus_tc)
		dev->irq = to_tc_dev(bdev)->interrupt;
	if (dfx_bus_eisa) {
		unsigned long base_addr = to_eisa_device(bdev)->base_addr;

		
		val = inb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0);
		val &= PI_CONFIG_STAT_0_M_IRQ;
		val >>= PI_CONFIG_STAT_0_V_IRQ;

		switch (val) {
		case PI_CONFIG_STAT_0_IRQ_K_9:
			dev->irq = 9;
			break;

		case PI_CONFIG_STAT_0_IRQ_K_10:
			dev->irq = 10;
			break;

		case PI_CONFIG_STAT_0_IRQ_K_11:
			dev->irq = 11;
			break;

		case PI_CONFIG_STAT_0_IRQ_K_15:
			dev->irq = 15;
			break;
		}


		
		val = ((bp->base.port >> 12) << PI_IO_CMP_V_SLOT);
		outb(base_addr + PI_ESIC_K_IO_ADD_CMP_0_1, val);
		outb(base_addr + PI_ESIC_K_IO_ADD_CMP_0_0, 0);
		outb(base_addr + PI_ESIC_K_IO_ADD_CMP_1_1, val);
		outb(base_addr + PI_ESIC_K_IO_ADD_CMP_1_0, 0);
		val = PI_ESIC_K_CSR_IO_LEN - 1;
		outb(base_addr + PI_ESIC_K_IO_ADD_MASK_0_1, (val >> 8) & 0xff);
		outb(base_addr + PI_ESIC_K_IO_ADD_MASK_0_0, val & 0xff);
		outb(base_addr + PI_ESIC_K_IO_ADD_MASK_1_1, (val >> 8) & 0xff);
		outb(base_addr + PI_ESIC_K_IO_ADD_MASK_1_0, val & 0xff);

		
		val = PI_FUNCTION_CNTRL_M_IOCS1 | PI_FUNCTION_CNTRL_M_IOCS0;
		if (dfx_use_mmio)
			val |= PI_FUNCTION_CNTRL_M_MEMCS0;
		outb(base_addr + PI_ESIC_K_FUNCTION_CNTRL, val);

		val = PI_SLOT_CNTRL_M_ENB;
		outb(base_addr + PI_ESIC_K_SLOT_CNTRL, val);

		val = inb(base_addr + PI_DEFEA_K_BURST_HOLDOFF);
		if (dfx_use_mmio)
			val |= PI_BURST_HOLDOFF_V_MEM_MAP;
		else
			val &= ~PI_BURST_HOLDOFF_V_MEM_MAP;
		outb(base_addr + PI_DEFEA_K_BURST_HOLDOFF, val);

		
		val = inb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0);
		val |= PI_CONFIG_STAT_0_M_INT_ENB;
		outb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0, val);
	}
	if (dfx_bus_pci) {
		struct pci_dev *pdev = to_pci_dev(bdev);

		

		dev->irq = pdev->irq;

		

		pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &val);
		if (val < PFI_K_LAT_TIMER_MIN) {
			val = PFI_K_LAT_TIMER_DEF;
			pci_write_config_byte(pdev, PCI_LATENCY_TIMER, val);
		}

		
		val = PFI_MODE_M_PDQ_INT_ENB | PFI_MODE_M_DMA_ENB;
		dfx_port_write_long(bp, PFI_K_REG_MODE_CTRL, val);
	}
}


static void __devexit dfx_bus_uninit(struct net_device *dev)
{
	DFX_board_t *bp = netdev_priv(dev);
	struct device *bdev = bp->bus_dev;
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_eisa = DFX_BUS_EISA(bdev);
	u8 val;

	DBG_printk("In dfx_bus_uninit...\n");

	

	if (dfx_bus_eisa) {
		unsigned long base_addr = to_eisa_device(bdev)->base_addr;

		
		val = inb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0);
		val &= ~PI_CONFIG_STAT_0_M_INT_ENB;
		outb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0, val);
	}
	if (dfx_bus_pci) {
		
		dfx_port_write_long(bp, PFI_K_REG_MODE_CTRL, 0);
	}
}



static void __devinit dfx_bus_config_check(DFX_board_t *bp)
{
	struct device __maybe_unused *bdev = bp->bus_dev;
	int dfx_bus_eisa = DFX_BUS_EISA(bdev);
	int	status;				
	u32	host_data;			

	DBG_printk("In dfx_bus_config_check...\n");

	

	if (dfx_bus_eisa) {
		if (to_eisa_device(bdev)->id.driver_data == DEFEA_PROD_ID_2) {
			status = dfx_hw_port_ctrl_req(bp,
											PI_PCTRL_M_SUB_CMD,
											PI_SUB_CMD_K_PDQ_REV_GET,
											0,
											&host_data);
			if ((status != DFX_K_SUCCESS) || (host_data == 2))
				{

				

				switch (bp->burst_size)
					{
					case PI_PDATA_B_DMA_BURST_SIZE_32:
					case PI_PDATA_B_DMA_BURST_SIZE_16:
						bp->burst_size = PI_PDATA_B_DMA_BURST_SIZE_8;
						break;

					default:
						break;
					}

				

				bp->full_duplex_enb = PI_SNMP_K_FALSE;
				}
			}
		}
	}



static int __devinit dfx_driver_init(struct net_device *dev,
				     const char *print_name,
				     resource_size_t bar_start)
{
	DFX_board_t *bp = netdev_priv(dev);
	struct device *bdev = bp->bus_dev;
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_eisa = DFX_BUS_EISA(bdev);
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;
	int alloc_size;			
	char *top_v, *curr_v;		
	dma_addr_t top_p, curr_p;	
	u32 data;			
	__le32 le32;
	char *board_name = NULL;

	DBG_printk("In dfx_driver_init...\n");

	

	dfx_bus_init(dev);


	bp->full_duplex_enb		= PI_SNMP_K_FALSE;
	bp->req_ttrt			= 8 * 12500;		
	bp->burst_size			= PI_PDATA_B_DMA_BURST_SIZE_DEF;
	bp->rcv_bufs_to_post	= RCV_BUFS_DEF;


	dfx_bus_config_check(bp);

	

	dfx_port_write_long(bp, PI_PDQ_K_REG_HOST_INT_ENB, PI_HOST_INT_K_DISABLE_ALL_INTS);

	

	(void) dfx_hw_dma_uninit(bp, PI_PDATA_A_RESET_M_SKIP_ST);

	

	if (dfx_hw_port_ctrl_req(bp, PI_PCTRL_M_MLA, PI_PDATA_A_MLA_K_LO, 0,
				 &data) != DFX_K_SUCCESS) {
		printk("%s: Could not read adapter factory MAC address!\n",
		       print_name);
		return DFX_K_FAILURE;
	}
	le32 = cpu_to_le32(data);
	memcpy(&bp->factory_mac_addr[0], &le32, sizeof(u32));

	if (dfx_hw_port_ctrl_req(bp, PI_PCTRL_M_MLA, PI_PDATA_A_MLA_K_HI, 0,
				 &data) != DFX_K_SUCCESS) {
		printk("%s: Could not read adapter factory MAC address!\n",
		       print_name);
		return DFX_K_FAILURE;
	}
	le32 = cpu_to_le32(data);
	memcpy(&bp->factory_mac_addr[4], &le32, sizeof(u16));


	memcpy(dev->dev_addr, bp->factory_mac_addr, FDDI_K_ALEN);
	if (dfx_bus_tc)
		board_name = "DEFTA";
	if (dfx_bus_eisa)
		board_name = "DEFEA";
	if (dfx_bus_pci)
		board_name = "DEFPA";
	pr_info("%s: %s at %saddr = 0x%llx, IRQ = %d, Hardware addr = %pMF\n",
		print_name, board_name, dfx_use_mmio ? "" : "I/O ",
		(long long)bar_start, dev->irq, dev->dev_addr);

	/*
	 * Get memory for descriptor block, consumer block, and other buffers
	 * that need to be DMA read or written to by the adapter.
	 */

	alloc_size = sizeof(PI_DESCR_BLOCK) +
					PI_CMD_REQ_K_SIZE_MAX +
					PI_CMD_RSP_K_SIZE_MAX +
#ifndef DYNAMIC_BUFFERS
					(bp->rcv_bufs_to_post * PI_RCV_DATA_K_SIZE_MAX) +
#endif
					sizeof(PI_CONSUMER_BLOCK) +
					(PI_ALIGN_K_DESC_BLK - 1);
	bp->kmalloced = top_v = dma_alloc_coherent(bp->bus_dev, alloc_size,
						   &bp->kmalloced_dma,
						   GFP_ATOMIC);
	if (top_v == NULL) {
		printk("%s: Could not allocate memory for host buffers "
		       "and structures!\n", print_name);
		return DFX_K_FAILURE;
	}
	memset(top_v, 0, alloc_size);	
	top_p = bp->kmalloced_dma;	


	curr_p = ALIGN(top_p, PI_ALIGN_K_DESC_BLK);
	curr_v = top_v + (curr_p - top_p);

	

	bp->descr_block_virt = (PI_DESCR_BLOCK *) curr_v;
	bp->descr_block_phys = curr_p;
	curr_v += sizeof(PI_DESCR_BLOCK);
	curr_p += sizeof(PI_DESCR_BLOCK);

	

	bp->cmd_req_virt = (PI_DMA_CMD_REQ *) curr_v;
	bp->cmd_req_phys = curr_p;
	curr_v += PI_CMD_REQ_K_SIZE_MAX;
	curr_p += PI_CMD_REQ_K_SIZE_MAX;

	

	bp->cmd_rsp_virt = (PI_DMA_CMD_RSP *) curr_v;
	bp->cmd_rsp_phys = curr_p;
	curr_v += PI_CMD_RSP_K_SIZE_MAX;
	curr_p += PI_CMD_RSP_K_SIZE_MAX;

	

	bp->rcv_block_virt = curr_v;
	bp->rcv_block_phys = curr_p;

#ifndef DYNAMIC_BUFFERS
	curr_v += (bp->rcv_bufs_to_post * PI_RCV_DATA_K_SIZE_MAX);
	curr_p += (bp->rcv_bufs_to_post * PI_RCV_DATA_K_SIZE_MAX);
#endif

	

	bp->cons_block_virt = (PI_CONSUMER_BLOCK *) curr_v;
	bp->cons_block_phys = curr_p;

	

	DBG_printk("%s: Descriptor block virt = %0lX, phys = %0X\n",
		   print_name,
		   (long)bp->descr_block_virt, bp->descr_block_phys);
	DBG_printk("%s: Command Request buffer virt = %0lX, phys = %0X\n",
		   print_name, (long)bp->cmd_req_virt, bp->cmd_req_phys);
	DBG_printk("%s: Command Response buffer virt = %0lX, phys = %0X\n",
		   print_name, (long)bp->cmd_rsp_virt, bp->cmd_rsp_phys);
	DBG_printk("%s: Receive buffer block virt = %0lX, phys = %0X\n",
		   print_name, (long)bp->rcv_block_virt, bp->rcv_block_phys);
	DBG_printk("%s: Consumer block virt = %0lX, phys = %0X\n",
		   print_name, (long)bp->cons_block_virt, bp->cons_block_phys);

	return DFX_K_SUCCESS;
}



static int dfx_adap_init(DFX_board_t *bp, int get_buffers)
	{
	DBG_printk("In dfx_adap_init...\n");

	

	dfx_port_write_long(bp, PI_PDQ_K_REG_HOST_INT_ENB, PI_HOST_INT_K_DISABLE_ALL_INTS);

	

	if (dfx_hw_dma_uninit(bp, bp->reset_type) != DFX_K_SUCCESS)
		{
		printk("%s: Could not uninitialize/reset adapter!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}


	dfx_port_write_long(bp, PI_PDQ_K_REG_TYPE_0_STATUS, PI_HOST_INT_K_ACK_ALL_TYPE_0);


	bp->cmd_req_reg.lword	= 0;
	bp->cmd_rsp_reg.lword	= 0;
	bp->rcv_xmt_reg.lword	= 0;

	

	memset(bp->cons_block_virt, 0, sizeof(PI_CONSUMER_BLOCK));

	

	if (dfx_hw_port_ctrl_req(bp,
							PI_PCTRL_M_SUB_CMD,
							PI_SUB_CMD_K_BURST_SIZE_SET,
							bp->burst_size,
							NULL) != DFX_K_SUCCESS)
		{
		printk("%s: Could not set adapter burst size!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}


	if (dfx_hw_port_ctrl_req(bp,
							PI_PCTRL_M_CONS_BLOCK,
							bp->cons_block_phys,
							0,
							NULL) != DFX_K_SUCCESS)
		{
		printk("%s: Could not set consumer block address!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}

	if (dfx_hw_port_ctrl_req(bp, PI_PCTRL_M_INIT,
				 (u32)(bp->descr_block_phys |
				       PI_PDATA_A_INIT_M_BSWAP_INIT),
				 0, NULL) != DFX_K_SUCCESS) {
		printk("%s: Could not set descriptor block address!\n",
		       bp->dev->name);
		return DFX_K_FAILURE;
	}

	

	bp->cmd_req_virt->cmd_type = PI_CMD_K_CHARS_SET;
	bp->cmd_req_virt->char_set.item[0].item_code	= PI_ITEM_K_FLUSH_TIME;
	bp->cmd_req_virt->char_set.item[0].value		= 3;	
	bp->cmd_req_virt->char_set.item[0].item_index	= 0;
	bp->cmd_req_virt->char_set.item[1].item_code	= PI_ITEM_K_EOL;
	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		{
		printk("%s: DMA command request failed!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}

	

	bp->cmd_req_virt->cmd_type = PI_CMD_K_SNMP_SET;
	bp->cmd_req_virt->snmp_set.item[0].item_code	= PI_ITEM_K_FDX_ENB_DIS;
	bp->cmd_req_virt->snmp_set.item[0].value		= bp->full_duplex_enb;
	bp->cmd_req_virt->snmp_set.item[0].item_index	= 0;
	bp->cmd_req_virt->snmp_set.item[1].item_code	= PI_ITEM_K_MAC_T_REQ;
	bp->cmd_req_virt->snmp_set.item[1].value		= bp->req_ttrt;
	bp->cmd_req_virt->snmp_set.item[1].item_index	= 0;
	bp->cmd_req_virt->snmp_set.item[2].item_code	= PI_ITEM_K_EOL;
	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		{
		printk("%s: DMA command request failed!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}

	

	if (dfx_ctl_update_cam(bp) != DFX_K_SUCCESS)
		{
		printk("%s: Adapter CAM update failed!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}

	

	if (dfx_ctl_update_filters(bp) != DFX_K_SUCCESS)
		{
		printk("%s: Adapter filters update failed!\n", bp->dev->name);
		return DFX_K_FAILURE;
		}


	if (get_buffers)
		dfx_rcv_flush(bp);

	

	if (dfx_rcv_init(bp, get_buffers))
	        {
		printk("%s: Receive buffer allocation failed\n", bp->dev->name);
		if (get_buffers)
			dfx_rcv_flush(bp);
		return DFX_K_FAILURE;
		}

	

	bp->cmd_req_virt->cmd_type = PI_CMD_K_START;
	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		{
		printk("%s: Start command failed\n", bp->dev->name);
		if (get_buffers)
			dfx_rcv_flush(bp);
		return DFX_K_FAILURE;
		}

	

	dfx_port_write_long(bp, PI_PDQ_K_REG_HOST_INT_ENB, PI_HOST_INT_K_ENABLE_DEF_INTS);
	return DFX_K_SUCCESS;
	}



static int dfx_open(struct net_device *dev)
{
	DFX_board_t *bp = netdev_priv(dev);
	int ret;

	DBG_printk("In dfx_open...\n");

	

	ret = request_irq(dev->irq, dfx_interrupt, IRQF_SHARED, dev->name,
			  dev);
	if (ret) {
		printk(KERN_ERR "%s: Requested IRQ %d is busy\n", dev->name, dev->irq);
		return ret;
	}


	memcpy(dev->dev_addr, bp->factory_mac_addr, FDDI_K_ALEN);

	

	memset(bp->uc_table, 0, sizeof(bp->uc_table));
	memset(bp->mc_table, 0, sizeof(bp->mc_table));
	bp->uc_count = 0;
	bp->mc_count = 0;

	

	bp->ind_group_prom	= PI_FSTATE_K_BLOCK;
	bp->group_prom		= PI_FSTATE_K_BLOCK;

	spin_lock_init(&bp->lock);

	

	bp->reset_type = PI_PDATA_A_RESET_M_SKIP_ST;	
	if (dfx_adap_init(bp, 1) != DFX_K_SUCCESS)
	{
		printk(KERN_ERR "%s: Adapter open failed!\n", dev->name);
		free_irq(dev->irq, dev);
		return -EAGAIN;
	}

	
	netif_start_queue(dev);
	return 0;
}



static int dfx_close(struct net_device *dev)
{
	DFX_board_t *bp = netdev_priv(dev);

	DBG_printk("In dfx_close...\n");

	

	dfx_port_write_long(bp, PI_PDQ_K_REG_HOST_INT_ENB, PI_HOST_INT_K_DISABLE_ALL_INTS);

	

	(void) dfx_hw_dma_uninit(bp, PI_PDATA_A_RESET_M_SKIP_ST);


	dfx_xmt_flush(bp);


	bp->cmd_req_reg.lword	= 0;
	bp->cmd_rsp_reg.lword	= 0;
	bp->rcv_xmt_reg.lword	= 0;

	

	memset(bp->cons_block_virt, 0, sizeof(PI_CONSUMER_BLOCK));

	

	dfx_rcv_flush(bp);

	

	netif_stop_queue(dev);

	

	free_irq(dev->irq, dev);

	return 0;
}



static void dfx_int_pr_halt_id(DFX_board_t	*bp)
	{
	PI_UINT32	port_status;			
	PI_UINT32	halt_id;				

	

	dfx_port_read_long(bp, PI_PDQ_K_REG_PORT_STATUS, &port_status);

	

	halt_id = (port_status & PI_PSTATUS_M_HALT_ID) >> PI_PSTATUS_V_HALT_ID;
	switch (halt_id)
		{
		case PI_HALT_ID_K_SELFTEST_TIMEOUT:
			printk("%s: Halt ID: Selftest Timeout\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_PARITY_ERROR:
			printk("%s: Halt ID: Host Bus Parity Error\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_HOST_DIR_HALT:
			printk("%s: Halt ID: Host-Directed Halt\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_SW_FAULT:
			printk("%s: Halt ID: Adapter Software Fault\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_HW_FAULT:
			printk("%s: Halt ID: Adapter Hardware Fault\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_PC_TRACE:
			printk("%s: Halt ID: FDDI Network PC Trace Path Test\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_DMA_ERROR:
			printk("%s: Halt ID: Adapter DMA Error\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_IMAGE_CRC_ERROR:
			printk("%s: Halt ID: Firmware Image CRC Error\n", bp->dev->name);
			break;

		case PI_HALT_ID_K_BUS_EXCEPTION:
			printk("%s: Halt ID: 68000 Bus Exception\n", bp->dev->name);
			break;

		default:
			printk("%s: Halt ID: Unknown (code = %X)\n", bp->dev->name, halt_id);
			break;
		}
	}



static void dfx_int_type_0_process(DFX_board_t	*bp)

	{
	PI_UINT32	type_0_status;		
	PI_UINT32	state;				


	dfx_port_read_long(bp, PI_PDQ_K_REG_TYPE_0_STATUS, &type_0_status);
	dfx_port_write_long(bp, PI_PDQ_K_REG_TYPE_0_STATUS, type_0_status);

	

	if (type_0_status & (PI_TYPE_0_STAT_M_NXM |
							PI_TYPE_0_STAT_M_PM_PAR_ERR |
							PI_TYPE_0_STAT_M_BUS_PAR_ERR))
		{
		

		if (type_0_status & PI_TYPE_0_STAT_M_NXM)
			printk("%s: Non-Existent Memory Access Error\n", bp->dev->name);

		

		if (type_0_status & PI_TYPE_0_STAT_M_PM_PAR_ERR)
			printk("%s: Packet Memory Parity Error\n", bp->dev->name);

		

		if (type_0_status & PI_TYPE_0_STAT_M_BUS_PAR_ERR)
			printk("%s: Host Bus Parity Error\n", bp->dev->name);

		

		bp->link_available = PI_K_FALSE;	
		bp->reset_type = 0;					
		printk("%s: Resetting adapter...\n", bp->dev->name);
		if (dfx_adap_init(bp, 0) != DFX_K_SUCCESS)
			{
			printk("%s: Adapter reset failed!  Disabling adapter interrupts.\n", bp->dev->name);
			dfx_port_write_long(bp, PI_PDQ_K_REG_HOST_INT_ENB, PI_HOST_INT_K_DISABLE_ALL_INTS);
			return;
			}
		printk("%s: Adapter reset successful!\n", bp->dev->name);
		return;
		}

	

	if (type_0_status & PI_TYPE_0_STAT_M_XMT_FLUSH)
		{
		

		bp->link_available = PI_K_FALSE;		
		dfx_xmt_flush(bp);						
		(void) dfx_hw_port_ctrl_req(bp,
									PI_PCTRL_M_XMT_DATA_FLUSH_DONE,
									0,
									0,
									NULL);
		}

	

	if (type_0_status & PI_TYPE_0_STAT_M_STATE_CHANGE)
		{
		

		state = dfx_hw_adap_state_rd(bp);	
		if (state == PI_STATE_K_HALTED)
			{

			printk("%s: Controller has transitioned to HALTED state!\n", bp->dev->name);
			dfx_int_pr_halt_id(bp);			

			

			bp->link_available = PI_K_FALSE;	
			bp->reset_type = 0;					
			printk("%s: Resetting adapter...\n", bp->dev->name);
			if (dfx_adap_init(bp, 0) != DFX_K_SUCCESS)
				{
				printk("%s: Adapter reset failed!  Disabling adapter interrupts.\n", bp->dev->name);
				dfx_port_write_long(bp, PI_PDQ_K_REG_HOST_INT_ENB, PI_HOST_INT_K_DISABLE_ALL_INTS);
				return;
				}
			printk("%s: Adapter reset successful!\n", bp->dev->name);
			}
		else if (state == PI_STATE_K_LINK_AVAIL)
			{
			bp->link_available = PI_K_TRUE;		
			}
		}
	}



static void dfx_int_common(struct net_device *dev)
{
	DFX_board_t *bp = netdev_priv(dev);
	PI_UINT32	port_status;		

	

	if(dfx_xmt_done(bp))				
		netif_wake_queue(dev);

	

	dfx_rcv_queue_process(bp);		


	dfx_port_write_long(bp, PI_PDQ_K_REG_TYPE_2_PROD, bp->rcv_xmt_reg.lword);

	

	dfx_port_read_long(bp, PI_PDQ_K_REG_PORT_STATUS, &port_status);

	

	if (port_status & PI_PSTATUS_M_TYPE_0_PENDING)
		dfx_int_type_0_process(bp);	
	}



static irqreturn_t dfx_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	DFX_board_t *bp = netdev_priv(dev);
	struct device *bdev = bp->bus_dev;
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_eisa = DFX_BUS_EISA(bdev);
	int dfx_bus_tc = DFX_BUS_TC(bdev);

	

	if (dfx_bus_pci) {
		u32 status;

		dfx_port_read_long(bp, PFI_K_REG_STATUS, &status);
		if (!(status & PFI_STATUS_M_PDQ_INT))
			return IRQ_NONE;

		spin_lock(&bp->lock);

		
		dfx_port_write_long(bp, PFI_K_REG_MODE_CTRL,
				    PFI_MODE_M_DMA_ENB);

		
		dfx_int_common(dev);

		
		dfx_port_write_long(bp, PFI_K_REG_STATUS,
				    PFI_STATUS_M_PDQ_INT);
		dfx_port_write_long(bp, PFI_K_REG_MODE_CTRL,
				    (PFI_MODE_M_PDQ_INT_ENB |
				     PFI_MODE_M_DMA_ENB));

		spin_unlock(&bp->lock);
	}
	if (dfx_bus_eisa) {
		unsigned long base_addr = to_eisa_device(bdev)->base_addr;
		u8 status;

		status = inb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0);
		if (!(status & PI_CONFIG_STAT_0_M_PEND))
			return IRQ_NONE;

		spin_lock(&bp->lock);

		
		status &= ~PI_CONFIG_STAT_0_M_INT_ENB;
		outb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0, status);

		
		dfx_int_common(dev);

		
		status = inb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0);
		status |= PI_CONFIG_STAT_0_M_INT_ENB;
		outb(base_addr + PI_ESIC_K_IO_CONFIG_STAT_0, status);

		spin_unlock(&bp->lock);
	}
	if (dfx_bus_tc) {
		u32 status;

		dfx_port_read_long(bp, PI_PDQ_K_REG_PORT_STATUS, &status);
		if (!(status & (PI_PSTATUS_M_RCV_DATA_PENDING |
				PI_PSTATUS_M_XMT_DATA_PENDING |
				PI_PSTATUS_M_SMT_HOST_PENDING |
				PI_PSTATUS_M_UNSOL_PENDING |
				PI_PSTATUS_M_CMD_RSP_PENDING |
				PI_PSTATUS_M_CMD_REQ_PENDING |
				PI_PSTATUS_M_TYPE_0_PENDING)))
			return IRQ_NONE;

		spin_lock(&bp->lock);

		
		dfx_int_common(dev);

		spin_unlock(&bp->lock);
	}

	return IRQ_HANDLED;
}



static struct net_device_stats *dfx_ctl_get_stats(struct net_device *dev)
	{
	DFX_board_t *bp = netdev_priv(dev);

	

	bp->stats.gen.rx_packets = bp->rcv_total_frames;
	bp->stats.gen.tx_packets = bp->xmt_total_frames;
	bp->stats.gen.rx_bytes   = bp->rcv_total_bytes;
	bp->stats.gen.tx_bytes   = bp->xmt_total_bytes;
	bp->stats.gen.rx_errors  = bp->rcv_crc_errors +
				   bp->rcv_frame_status_errors +
				   bp->rcv_length_errors;
	bp->stats.gen.tx_errors  = bp->xmt_length_errors;
	bp->stats.gen.rx_dropped = bp->rcv_discards;
	bp->stats.gen.tx_dropped = bp->xmt_discards;
	bp->stats.gen.multicast  = bp->rcv_multicast_frames;
	bp->stats.gen.collisions = 0;		

	

	bp->cmd_req_virt->cmd_type = PI_CMD_K_SMT_MIB_GET;
	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		return (struct net_device_stats *)&bp->stats;

	

	memcpy(bp->stats.smt_station_id, &bp->cmd_rsp_virt->smt_mib_get.smt_station_id, sizeof(bp->cmd_rsp_virt->smt_mib_get.smt_station_id));
	bp->stats.smt_op_version_id					= bp->cmd_rsp_virt->smt_mib_get.smt_op_version_id;
	bp->stats.smt_hi_version_id					= bp->cmd_rsp_virt->smt_mib_get.smt_hi_version_id;
	bp->stats.smt_lo_version_id					= bp->cmd_rsp_virt->smt_mib_get.smt_lo_version_id;
	memcpy(bp->stats.smt_user_data, &bp->cmd_rsp_virt->smt_mib_get.smt_user_data, sizeof(bp->cmd_rsp_virt->smt_mib_get.smt_user_data));
	bp->stats.smt_mib_version_id				= bp->cmd_rsp_virt->smt_mib_get.smt_mib_version_id;
	bp->stats.smt_mac_cts						= bp->cmd_rsp_virt->smt_mib_get.smt_mac_ct;
	bp->stats.smt_non_master_cts				= bp->cmd_rsp_virt->smt_mib_get.smt_non_master_ct;
	bp->stats.smt_master_cts					= bp->cmd_rsp_virt->smt_mib_get.smt_master_ct;
	bp->stats.smt_available_paths				= bp->cmd_rsp_virt->smt_mib_get.smt_available_paths;
	bp->stats.smt_config_capabilities			= bp->cmd_rsp_virt->smt_mib_get.smt_config_capabilities;
	bp->stats.smt_config_policy					= bp->cmd_rsp_virt->smt_mib_get.smt_config_policy;
	bp->stats.smt_connection_policy				= bp->cmd_rsp_virt->smt_mib_get.smt_connection_policy;
	bp->stats.smt_t_notify						= bp->cmd_rsp_virt->smt_mib_get.smt_t_notify;
	bp->stats.smt_stat_rpt_policy				= bp->cmd_rsp_virt->smt_mib_get.smt_stat_rpt_policy;
	bp->stats.smt_trace_max_expiration			= bp->cmd_rsp_virt->smt_mib_get.smt_trace_max_expiration;
	bp->stats.smt_bypass_present				= bp->cmd_rsp_virt->smt_mib_get.smt_bypass_present;
	bp->stats.smt_ecm_state						= bp->cmd_rsp_virt->smt_mib_get.smt_ecm_state;
	bp->stats.smt_cf_state						= bp->cmd_rsp_virt->smt_mib_get.smt_cf_state;
	bp->stats.smt_remote_disconnect_flag		= bp->cmd_rsp_virt->smt_mib_get.smt_remote_disconnect_flag;
	bp->stats.smt_station_status				= bp->cmd_rsp_virt->smt_mib_get.smt_station_status;
	bp->stats.smt_peer_wrap_flag				= bp->cmd_rsp_virt->smt_mib_get.smt_peer_wrap_flag;
	bp->stats.smt_time_stamp					= bp->cmd_rsp_virt->smt_mib_get.smt_msg_time_stamp.ls;
	bp->stats.smt_transition_time_stamp			= bp->cmd_rsp_virt->smt_mib_get.smt_transition_time_stamp.ls;
	bp->stats.mac_frame_status_functions		= bp->cmd_rsp_virt->smt_mib_get.mac_frame_status_functions;
	bp->stats.mac_t_max_capability				= bp->cmd_rsp_virt->smt_mib_get.mac_t_max_capability;
	bp->stats.mac_tvx_capability				= bp->cmd_rsp_virt->smt_mib_get.mac_tvx_capability;
	bp->stats.mac_available_paths				= bp->cmd_rsp_virt->smt_mib_get.mac_available_paths;
	bp->stats.mac_current_path					= bp->cmd_rsp_virt->smt_mib_get.mac_current_path;
	memcpy(bp->stats.mac_upstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_upstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_downstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_downstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_old_upstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_old_upstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_old_downstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_old_downstream_nbr, FDDI_K_ALEN);
	bp->stats.mac_dup_address_test				= bp->cmd_rsp_virt->smt_mib_get.mac_dup_address_test;
	bp->stats.mac_requested_paths				= bp->cmd_rsp_virt->smt_mib_get.mac_requested_paths;
	bp->stats.mac_downstream_port_type			= bp->cmd_rsp_virt->smt_mib_get.mac_downstream_port_type;
	memcpy(bp->stats.mac_smt_address, &bp->cmd_rsp_virt->smt_mib_get.mac_smt_address, FDDI_K_ALEN);
	bp->stats.mac_t_req							= bp->cmd_rsp_virt->smt_mib_get.mac_t_req;
	bp->stats.mac_t_neg							= bp->cmd_rsp_virt->smt_mib_get.mac_t_neg;
	bp->stats.mac_t_max							= bp->cmd_rsp_virt->smt_mib_get.mac_t_max;
	bp->stats.mac_tvx_value						= bp->cmd_rsp_virt->smt_mib_get.mac_tvx_value;
	bp->stats.mac_frame_error_threshold			= bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_threshold;
	bp->stats.mac_frame_error_ratio				= bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_ratio;
	bp->stats.mac_rmt_state						= bp->cmd_rsp_virt->smt_mib_get.mac_rmt_state;
	bp->stats.mac_da_flag						= bp->cmd_rsp_virt->smt_mib_get.mac_da_flag;
	bp->stats.mac_una_da_flag					= bp->cmd_rsp_virt->smt_mib_get.mac_unda_flag;
	bp->stats.mac_frame_error_flag				= bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_flag;
	bp->stats.mac_ma_unitdata_available			= bp->cmd_rsp_virt->smt_mib_get.mac_ma_unitdata_available;
	bp->stats.mac_hardware_present				= bp->cmd_rsp_virt->smt_mib_get.mac_hardware_present;
	bp->stats.mac_ma_unitdata_enable			= bp->cmd_rsp_virt->smt_mib_get.mac_ma_unitdata_enable;
	bp->stats.path_tvx_lower_bound				= bp->cmd_rsp_virt->smt_mib_get.path_tvx_lower_bound;
	bp->stats.path_t_max_lower_bound			= bp->cmd_rsp_virt->smt_mib_get.path_t_max_lower_bound;
	bp->stats.path_max_t_req					= bp->cmd_rsp_virt->smt_mib_get.path_max_t_req;
	memcpy(bp->stats.path_configuration, &bp->cmd_rsp_virt->smt_mib_get.path_configuration, sizeof(bp->cmd_rsp_virt->smt_mib_get.path_configuration));
	bp->stats.port_my_type[0]					= bp->cmd_rsp_virt->smt_mib_get.port_my_type[0];
	bp->stats.port_my_type[1]					= bp->cmd_rsp_virt->smt_mib_get.port_my_type[1];
	bp->stats.port_neighbor_type[0]				= bp->cmd_rsp_virt->smt_mib_get.port_neighbor_type[0];
	bp->stats.port_neighbor_type[1]				= bp->cmd_rsp_virt->smt_mib_get.port_neighbor_type[1];
	bp->stats.port_connection_policies[0]		= bp->cmd_rsp_virt->smt_mib_get.port_connection_policies[0];
	bp->stats.port_connection_policies[1]		= bp->cmd_rsp_virt->smt_mib_get.port_connection_policies[1];
	bp->stats.port_mac_indicated[0]				= bp->cmd_rsp_virt->smt_mib_get.port_mac_indicated[0];
	bp->stats.port_mac_indicated[1]				= bp->cmd_rsp_virt->smt_mib_get.port_mac_indicated[1];
	bp->stats.port_current_path[0]				= bp->cmd_rsp_virt->smt_mib_get.port_current_path[0];
	bp->stats.port_current_path[1]				= bp->cmd_rsp_virt->smt_mib_get.port_current_path[1];
	memcpy(&bp->stats.port_requested_paths[0*3], &bp->cmd_rsp_virt->smt_mib_get.port_requested_paths[0], 3);
	memcpy(&bp->stats.port_requested_paths[1*3], &bp->cmd_rsp_virt->smt_mib_get.port_requested_paths[1], 3);
	bp->stats.port_mac_placement[0]				= bp->cmd_rsp_virt->smt_mib_get.port_mac_placement[0];
	bp->stats.port_mac_placement[1]				= bp->cmd_rsp_virt->smt_mib_get.port_mac_placement[1];
	bp->stats.port_available_paths[0]			= bp->cmd_rsp_virt->smt_mib_get.port_available_paths[0];
	bp->stats.port_available_paths[1]			= bp->cmd_rsp_virt->smt_mib_get.port_available_paths[1];
	bp->stats.port_pmd_class[0]					= bp->cmd_rsp_virt->smt_mib_get.port_pmd_class[0];
	bp->stats.port_pmd_class[1]					= bp->cmd_rsp_virt->smt_mib_get.port_pmd_class[1];
	bp->stats.port_connection_capabilities[0]	= bp->cmd_rsp_virt->smt_mib_get.port_connection_capabilities[0];
	bp->stats.port_connection_capabilities[1]	= bp->cmd_rsp_virt->smt_mib_get.port_connection_capabilities[1];
	bp->stats.port_bs_flag[0]					= bp->cmd_rsp_virt->smt_mib_get.port_bs_flag[0];
	bp->stats.port_bs_flag[1]					= bp->cmd_rsp_virt->smt_mib_get.port_bs_flag[1];
	bp->stats.port_ler_estimate[0]				= bp->cmd_rsp_virt->smt_mib_get.port_ler_estimate[0];
	bp->stats.port_ler_estimate[1]				= bp->cmd_rsp_virt->smt_mib_get.port_ler_estimate[1];
	bp->stats.port_ler_cutoff[0]				= bp->cmd_rsp_virt->smt_mib_get.port_ler_cutoff[0];
	bp->stats.port_ler_cutoff[1]				= bp->cmd_rsp_virt->smt_mib_get.port_ler_cutoff[1];
	bp->stats.port_ler_alarm[0]					= bp->cmd_rsp_virt->smt_mib_get.port_ler_alarm[0];
	bp->stats.port_ler_alarm[1]					= bp->cmd_rsp_virt->smt_mib_get.port_ler_alarm[1];
	bp->stats.port_connect_state[0]				= bp->cmd_rsp_virt->smt_mib_get.port_connect_state[0];
	bp->stats.port_connect_state[1]				= bp->cmd_rsp_virt->smt_mib_get.port_connect_state[1];
	bp->stats.port_pcm_state[0]					= bp->cmd_rsp_virt->smt_mib_get.port_pcm_state[0];
	bp->stats.port_pcm_state[1]					= bp->cmd_rsp_virt->smt_mib_get.port_pcm_state[1];
	bp->stats.port_pc_withhold[0]				= bp->cmd_rsp_virt->smt_mib_get.port_pc_withhold[0];
	bp->stats.port_pc_withhold[1]				= bp->cmd_rsp_virt->smt_mib_get.port_pc_withhold[1];
	bp->stats.port_ler_flag[0]					= bp->cmd_rsp_virt->smt_mib_get.port_ler_flag[0];
	bp->stats.port_ler_flag[1]					= bp->cmd_rsp_virt->smt_mib_get.port_ler_flag[1];
	bp->stats.port_hardware_present[0]			= bp->cmd_rsp_virt->smt_mib_get.port_hardware_present[0];
	bp->stats.port_hardware_present[1]			= bp->cmd_rsp_virt->smt_mib_get.port_hardware_present[1];

	

	bp->cmd_req_virt->cmd_type = PI_CMD_K_CNTRS_GET;
	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		return (struct net_device_stats *)&bp->stats;

	

	bp->stats.mac_frame_cts				= bp->cmd_rsp_virt->cntrs_get.cntrs.frame_cnt.ls;
	bp->stats.mac_copied_cts			= bp->cmd_rsp_virt->cntrs_get.cntrs.copied_cnt.ls;
	bp->stats.mac_transmit_cts			= bp->cmd_rsp_virt->cntrs_get.cntrs.transmit_cnt.ls;
	bp->stats.mac_error_cts				= bp->cmd_rsp_virt->cntrs_get.cntrs.error_cnt.ls;
	bp->stats.mac_lost_cts				= bp->cmd_rsp_virt->cntrs_get.cntrs.lost_cnt.ls;
	bp->stats.port_lct_fail_cts[0]		= bp->cmd_rsp_virt->cntrs_get.cntrs.lct_rejects[0].ls;
	bp->stats.port_lct_fail_cts[1]		= bp->cmd_rsp_virt->cntrs_get.cntrs.lct_rejects[1].ls;
	bp->stats.port_lem_reject_cts[0]	= bp->cmd_rsp_virt->cntrs_get.cntrs.lem_rejects[0].ls;
	bp->stats.port_lem_reject_cts[1]	= bp->cmd_rsp_virt->cntrs_get.cntrs.lem_rejects[1].ls;
	bp->stats.port_lem_cts[0]			= bp->cmd_rsp_virt->cntrs_get.cntrs.link_errors[0].ls;
	bp->stats.port_lem_cts[1]			= bp->cmd_rsp_virt->cntrs_get.cntrs.link_errors[1].ls;

	return (struct net_device_stats *)&bp->stats;
	}



static void dfx_ctl_set_multicast_list(struct net_device *dev)
{
	DFX_board_t *bp = netdev_priv(dev);
	int					i;			
	struct netdev_hw_addr *ha;

	

	if (dev->flags & IFF_PROMISC)
		bp->ind_group_prom = PI_FSTATE_K_PASS;		

	

	else
		{
		bp->ind_group_prom = PI_FSTATE_K_BLOCK;		

		if (netdev_mc_count(dev) > (PI_CMD_ADDR_FILTER_K_SIZE - bp->uc_count))
			{
			bp->group_prom	= PI_FSTATE_K_PASS;		
			bp->mc_count	= 0;					
			}
		else
			{
			bp->group_prom	= PI_FSTATE_K_BLOCK;	
			bp->mc_count	= netdev_mc_count(dev);		
			}

		

		i = 0;
		netdev_for_each_mc_addr(ha, dev)
			memcpy(&bp->mc_table[i++ * FDDI_K_ALEN],
			       ha->addr, FDDI_K_ALEN);

		if (dfx_ctl_update_cam(bp) != DFX_K_SUCCESS)
			{
			DBG_printk("%s: Could not update multicast address table!\n", dev->name);
			}
		else
			{
			DBG_printk("%s: Multicast address table updated!  Added %d addresses.\n", dev->name, bp->mc_count);
			}
		}

	

	if (dfx_ctl_update_filters(bp) != DFX_K_SUCCESS)
		{
		DBG_printk("%s: Could not update adapter filters!\n", dev->name);
		}
	else
		{
		DBG_printk("%s: Adapter filters updated!\n", dev->name);
		}
	}



static int dfx_ctl_set_mac_address(struct net_device *dev, void *addr)
	{
	struct sockaddr	*p_sockaddr = (struct sockaddr *)addr;
	DFX_board_t *bp = netdev_priv(dev);

	

	memcpy(dev->dev_addr, p_sockaddr->sa_data, FDDI_K_ALEN);	
	memcpy(&bp->uc_table[0], p_sockaddr->sa_data, FDDI_K_ALEN);	
	bp->uc_count = 1;


	if ((bp->uc_count + bp->mc_count) > PI_CMD_ADDR_FILTER_K_SIZE)
		{
		bp->group_prom	= PI_FSTATE_K_PASS;		
		bp->mc_count	= 0;					

		

		if (dfx_ctl_update_filters(bp) != DFX_K_SUCCESS)
			{
			DBG_printk("%s: Could not update adapter filters!\n", dev->name);
			}
		else
			{
			DBG_printk("%s: Adapter filters updated!\n", dev->name);
			}
		}

	

	if (dfx_ctl_update_cam(bp) != DFX_K_SUCCESS)
		{
		DBG_printk("%s: Could not set new MAC address!\n", dev->name);
		}
	else
		{
		DBG_printk("%s: Adapter CAM updated with new MAC address\n", dev->name);
		}
	return 0;			
	}



static int dfx_ctl_update_cam(DFX_board_t *bp)
	{
	int			i;				
	PI_LAN_ADDR	*p_addr;		


	memset(bp->cmd_req_virt, 0, PI_CMD_REQ_K_SIZE_MAX);	
	bp->cmd_req_virt->cmd_type = PI_CMD_K_ADDR_FILTER_SET;
	p_addr = &bp->cmd_req_virt->addr_filter_set.entry[0];

	

	for (i=0; i < (int)bp->uc_count; i++)
		{
		if (i < PI_CMD_ADDR_FILTER_K_SIZE)
			{
			memcpy(p_addr, &bp->uc_table[i*FDDI_K_ALEN], FDDI_K_ALEN);
			p_addr++;			
			}
		}

	

	for (i=0; i < (int)bp->mc_count; i++)
		{
		if ((i + bp->uc_count) < PI_CMD_ADDR_FILTER_K_SIZE)
			{
			memcpy(p_addr, &bp->mc_table[i*FDDI_K_ALEN], FDDI_K_ALEN);
			p_addr++;			
			}
		}

	

	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		return DFX_K_FAILURE;
	return DFX_K_SUCCESS;
	}



static int dfx_ctl_update_filters(DFX_board_t *bp)
	{
	int	i = 0;					

	

	bp->cmd_req_virt->cmd_type = PI_CMD_K_FILTERS_SET;

	

	bp->cmd_req_virt->filter_set.item[i].item_code	= PI_ITEM_K_BROADCAST;
	bp->cmd_req_virt->filter_set.item[i++].value	= PI_FSTATE_K_PASS;

	

	bp->cmd_req_virt->filter_set.item[i].item_code	= PI_ITEM_K_IND_GROUP_PROM;
	bp->cmd_req_virt->filter_set.item[i++].value	= bp->ind_group_prom;

	

	bp->cmd_req_virt->filter_set.item[i].item_code	= PI_ITEM_K_GROUP_PROM;
	bp->cmd_req_virt->filter_set.item[i++].value	= bp->group_prom;

	

	bp->cmd_req_virt->filter_set.item[i].item_code	= PI_ITEM_K_EOL;

	

	if (dfx_hw_dma_cmd_req(bp) != DFX_K_SUCCESS)
		return DFX_K_FAILURE;
	return DFX_K_SUCCESS;
	}



static int dfx_hw_dma_cmd_req(DFX_board_t *bp)
	{
	int status;			
	int timeout_cnt;	

	

	status = dfx_hw_adap_state_rd(bp);
	if ((status == PI_STATE_K_RESET)		||
		(status == PI_STATE_K_HALTED)		||
		(status == PI_STATE_K_DMA_UNAVAIL)	||
		(status == PI_STATE_K_UPGRADE))
		return DFX_K_OUTSTATE;

	

	bp->descr_block_virt->cmd_rsp[bp->cmd_rsp_reg.index.prod].long_0 = (u32) (PI_RCV_DESCR_M_SOP |
			((PI_CMD_RSP_K_SIZE_MAX / PI_ALIGN_K_CMD_RSP_BUFF) << PI_RCV_DESCR_V_SEG_LEN));
	bp->descr_block_virt->cmd_rsp[bp->cmd_rsp_reg.index.prod].long_1 = bp->cmd_rsp_phys;

	

	bp->cmd_rsp_reg.index.prod += 1;
	bp->cmd_rsp_reg.index.prod &= PI_CMD_RSP_K_NUM_ENTRIES-1;
	dfx_port_write_long(bp, PI_PDQ_K_REG_CMD_RSP_PROD, bp->cmd_rsp_reg.lword);

	

	bp->descr_block_virt->cmd_req[bp->cmd_req_reg.index.prod].long_0 = (u32) (PI_XMT_DESCR_M_SOP |
			PI_XMT_DESCR_M_EOP | (PI_CMD_REQ_K_SIZE_MAX << PI_XMT_DESCR_V_SEG_LEN));
	bp->descr_block_virt->cmd_req[bp->cmd_req_reg.index.prod].long_1 = bp->cmd_req_phys;

	

	bp->cmd_req_reg.index.prod += 1;
	bp->cmd_req_reg.index.prod &= PI_CMD_REQ_K_NUM_ENTRIES-1;
	dfx_port_write_long(bp, PI_PDQ_K_REG_CMD_REQ_PROD, bp->cmd_req_reg.lword);


	for (timeout_cnt = 20000; timeout_cnt > 0; timeout_cnt--)
		{
		if (bp->cmd_req_reg.index.prod == (u8)(bp->cons_block_virt->cmd_req))
			break;
		udelay(100);			
		}
	if (timeout_cnt == 0)
		return DFX_K_HW_TIMEOUT;

	

	bp->cmd_req_reg.index.comp += 1;
	bp->cmd_req_reg.index.comp &= PI_CMD_REQ_K_NUM_ENTRIES-1;
	dfx_port_write_long(bp, PI_PDQ_K_REG_CMD_REQ_PROD, bp->cmd_req_reg.lword);


	for (timeout_cnt = 20000; timeout_cnt > 0; timeout_cnt--)
		{
		if (bp->cmd_rsp_reg.index.prod == (u8)(bp->cons_block_virt->cmd_rsp))
			break;
		udelay(100);			
		}
	if (timeout_cnt == 0)
		return DFX_K_HW_TIMEOUT;

	

	bp->cmd_rsp_reg.index.comp += 1;
	bp->cmd_rsp_reg.index.comp &= PI_CMD_RSP_K_NUM_ENTRIES-1;
	dfx_port_write_long(bp, PI_PDQ_K_REG_CMD_RSP_PROD, bp->cmd_rsp_reg.lword);
	return DFX_K_SUCCESS;
	}



static int dfx_hw_port_ctrl_req(
	DFX_board_t	*bp,
	PI_UINT32	command,
	PI_UINT32	data_a,
	PI_UINT32	data_b,
	PI_UINT32	*host_data
	)

	{
	PI_UINT32	port_cmd;		
	int			timeout_cnt;	

	

	port_cmd = (PI_UINT32) (command | PI_PCTRL_M_CMD_ERROR);

	

	dfx_port_write_long(bp, PI_PDQ_K_REG_PORT_DATA_A, data_a);
	dfx_port_write_long(bp, PI_PDQ_K_REG_PORT_DATA_B, data_b);
	dfx_port_write_long(bp, PI_PDQ_K_REG_PORT_CTRL, port_cmd);

	

	if (command == PI_PCTRL_M_BLAST_FLASH)
		timeout_cnt = 600000;	
	else
		timeout_cnt = 20000;	

	for (; timeout_cnt > 0; timeout_cnt--)
		{
		dfx_port_read_long(bp, PI_PDQ_K_REG_PORT_CTRL, &port_cmd);
		if (!(port_cmd & PI_PCTRL_M_CMD_ERROR))
			break;
		udelay(100);			
		}
	if (timeout_cnt == 0)
		return DFX_K_HW_TIMEOUT;


	if (host_data != NULL)
		dfx_port_read_long(bp, PI_PDQ_K_REG_HOST_DATA, host_data);
	return DFX_K_SUCCESS;
	}



static void dfx_hw_adap_reset(
	DFX_board_t	*bp,
	PI_UINT32	type
	)

	{
	

	dfx_port_write_long(bp, PI_PDQ_K_REG_PORT_DATA_A, type);	
	dfx_port_write_long(bp, PI_PDQ_K_REG_PORT_RESET, PI_RESET_M_ASSERT_RESET);

	

	udelay(20);

	

	dfx_port_write_long(bp, PI_PDQ_K_REG_PORT_RESET, 0);
	}



static int dfx_hw_adap_state_rd(DFX_board_t *bp)
	{
	PI_UINT32 port_status;		

	dfx_port_read_long(bp, PI_PDQ_K_REG_PORT_STATUS, &port_status);
	return (port_status & PI_PSTATUS_M_STATE) >> PI_PSTATUS_V_STATE;
	}



static int dfx_hw_dma_uninit(DFX_board_t *bp, PI_UINT32 type)
	{
	int timeout_cnt;	

	

	dfx_hw_adap_reset(bp, type);

	

	for (timeout_cnt = 100000; timeout_cnt > 0; timeout_cnt--)
		{
		if (dfx_hw_adap_state_rd(bp) == PI_STATE_K_DMA_UNAVAIL)
			break;
		udelay(100);					
		}
	if (timeout_cnt == 0)
		return DFX_K_HW_TIMEOUT;
	return DFX_K_SUCCESS;
	}


static void my_skb_align(struct sk_buff *skb, int n)
{
	unsigned long x = (unsigned long)skb->data;
	unsigned long v;

	v = ALIGN(x, n);	

	skb_reserve(skb, v - x);
}



static int dfx_rcv_init(DFX_board_t *bp, int get_buffers)
	{
	int	i, j;					


	if (get_buffers) {
#ifdef DYNAMIC_BUFFERS
	for (i = 0; i < (int)(bp->rcv_bufs_to_post); i++)
		for (j = 0; (i + j) < (int)PI_RCV_DATA_K_NUM_ENTRIES; j += bp->rcv_bufs_to_post)
		{
			struct sk_buff *newskb = __netdev_alloc_skb(bp->dev, NEW_SKB_SIZE, GFP_NOIO);
			if (!newskb)
				return -ENOMEM;
			bp->descr_block_virt->rcv_data[i+j].long_0 = (u32) (PI_RCV_DESCR_M_SOP |
				((PI_RCV_DATA_K_SIZE_MAX / PI_ALIGN_K_RCV_DATA_BUFF) << PI_RCV_DESCR_V_SEG_LEN));

			my_skb_align(newskb, 128);
			bp->descr_block_virt->rcv_data[i + j].long_1 =
				(u32)dma_map_single(bp->bus_dev, newskb->data,
						    NEW_SKB_SIZE,
						    DMA_FROM_DEVICE);
			bp->p_rcv_buff_va[i+j] = (char *) newskb;
		}
#else
	for (i=0; i < (int)(bp->rcv_bufs_to_post); i++)
		for (j=0; (i + j) < (int)PI_RCV_DATA_K_NUM_ENTRIES; j += bp->rcv_bufs_to_post)
			{
			bp->descr_block_virt->rcv_data[i+j].long_0 = (u32) (PI_RCV_DESCR_M_SOP |
				((PI_RCV_DATA_K_SIZE_MAX / PI_ALIGN_K_RCV_DATA_BUFF) << PI_RCV_DESCR_V_SEG_LEN));
			bp->descr_block_virt->rcv_data[i+j].long_1 = (u32) (bp->rcv_block_phys + (i * PI_RCV_DATA_K_SIZE_MAX));
			bp->p_rcv_buff_va[i+j] = (char *) (bp->rcv_block_virt + (i * PI_RCV_DATA_K_SIZE_MAX));
			}
#endif
	}

	

	bp->rcv_xmt_reg.index.rcv_prod = bp->rcv_bufs_to_post;
	dfx_port_write_long(bp, PI_PDQ_K_REG_TYPE_2_PROD, bp->rcv_xmt_reg.lword);
	return 0;
	}



static void dfx_rcv_queue_process(
	DFX_board_t *bp
	)

	{
	PI_TYPE_2_CONSUMER	*p_type_2_cons;		
	char				*p_buff;			
	u32					descr, pkt_len;		
	struct sk_buff		*skb;				

	

	p_type_2_cons = (PI_TYPE_2_CONSUMER *)(&bp->cons_block_virt->xmt_rcv_data);
	while (bp->rcv_xmt_reg.index.rcv_comp != p_type_2_cons->index.rcv_cons)
		{
		

		int entry;

		entry = bp->rcv_xmt_reg.index.rcv_comp;
#ifdef DYNAMIC_BUFFERS
		p_buff = (char *) (((struct sk_buff *)bp->p_rcv_buff_va[entry])->data);
#else
		p_buff = (char *) bp->p_rcv_buff_va[entry];
#endif
		memcpy(&descr, p_buff + RCV_BUFF_K_DESCR, sizeof(u32));

		if (descr & PI_FMC_DESCR_M_RCC_FLUSH)
			{
			if (descr & PI_FMC_DESCR_M_RCC_CRC)
				bp->rcv_crc_errors++;
			else
				bp->rcv_frame_status_errors++;
			}
		else
		{
			int rx_in_place = 0;

			

			pkt_len = (u32)((descr & PI_FMC_DESCR_M_LEN) >> PI_FMC_DESCR_V_LEN);
			pkt_len -= 4;				
			if (!IN_RANGE(pkt_len, FDDI_K_LLC_ZLEN, FDDI_K_LLC_LEN))
				bp->rcv_length_errors++;
			else{
#ifdef DYNAMIC_BUFFERS
				if (pkt_len > SKBUFF_RX_COPYBREAK) {
					struct sk_buff *newskb;

					newskb = dev_alloc_skb(NEW_SKB_SIZE);
					if (newskb){
						rx_in_place = 1;

						my_skb_align(newskb, 128);
						skb = (struct sk_buff *)bp->p_rcv_buff_va[entry];
						dma_unmap_single(bp->bus_dev,
							bp->descr_block_virt->rcv_data[entry].long_1,
							NEW_SKB_SIZE,
							DMA_FROM_DEVICE);
						skb_reserve(skb, RCV_BUFF_K_PADDING);
						bp->p_rcv_buff_va[entry] = (char *)newskb;
						bp->descr_block_virt->rcv_data[entry].long_1 =
							(u32)dma_map_single(bp->bus_dev,
								newskb->data,
								NEW_SKB_SIZE,
								DMA_FROM_DEVICE);
					} else
						skb = NULL;
				} else
#endif
					skb = dev_alloc_skb(pkt_len+3);	
				if (skb == NULL)
					{
					printk("%s: Could not allocate receive buffer.  Dropping packet.\n", bp->dev->name);
					bp->rcv_discards++;
					break;
					}
				else {
#ifndef DYNAMIC_BUFFERS
					if (! rx_in_place)
#endif
					{
						

						skb_copy_to_linear_data(skb,
							       p_buff + RCV_BUFF_K_PADDING,
							       pkt_len + 3);
					}

					skb_reserve(skb,3);		
					skb_put(skb, pkt_len);		
					skb->protocol = fddi_type_trans(skb, bp->dev);
					bp->rcv_total_bytes += skb->len;
					netif_rx(skb);

					
					bp->rcv_total_frames++;
					if (*(p_buff + RCV_BUFF_K_DA) & 0x01)
						bp->rcv_multicast_frames++;
				}
			}
			}


		bp->rcv_xmt_reg.index.rcv_prod += 1;
		bp->rcv_xmt_reg.index.rcv_comp += 1;
		}
	}



static netdev_tx_t dfx_xmt_queue_pkt(struct sk_buff *skb,
				     struct net_device *dev)
	{
	DFX_board_t		*bp = netdev_priv(dev);
	u8			prod;				
	PI_XMT_DESCR		*p_xmt_descr;		
	XMT_DRIVER_DESCR	*p_xmt_drv_descr;	
	unsigned long		flags;

	netif_stop_queue(dev);


	if (!IN_RANGE(skb->len, FDDI_K_LLC_ZLEN, FDDI_K_LLC_LEN))
	{
		printk("%s: Invalid packet length - %u bytes\n",
			dev->name, skb->len);
		bp->xmt_length_errors++;		
		netif_wake_queue(dev);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;			
	}

	if (bp->link_available == PI_K_FALSE)
		{
		if (dfx_hw_adap_state_rd(bp) == PI_STATE_K_LINK_AVAIL)	
			bp->link_available = PI_K_TRUE;		
		else
			{
			bp->xmt_discards++;					
			dev_kfree_skb(skb);		
			netif_wake_queue(dev);
			return NETDEV_TX_OK;		
			}
		}

	spin_lock_irqsave(&bp->lock, flags);

	

	prod		= bp->rcv_xmt_reg.index.xmt_prod;
	p_xmt_descr = &(bp->descr_block_virt->xmt_data[prod]);


	p_xmt_drv_descr = &(bp->xmt_drv_descr_blk[prod++]);	

	

	skb_push(skb,3);
	skb->data[0] = DFX_PRH0_BYTE;	
	skb->data[1] = DFX_PRH1_BYTE;	
	skb->data[2] = DFX_PRH2_BYTE;	


	p_xmt_descr->long_0	= (u32) (PI_XMT_DESCR_M_SOP | PI_XMT_DESCR_M_EOP | ((skb->len) << PI_XMT_DESCR_V_SEG_LEN));
	p_xmt_descr->long_1 = (u32)dma_map_single(bp->bus_dev, skb->data,
						  skb->len, DMA_TO_DEVICE);


	if (prod == bp->rcv_xmt_reg.index.xmt_comp)
	{
		skb_pull(skb,3);
		spin_unlock_irqrestore(&bp->lock, flags);
		return NETDEV_TX_BUSY;	
	}


	p_xmt_drv_descr->p_skb = skb;

	

	bp->rcv_xmt_reg.index.xmt_prod = prod;
	dfx_port_write_long(bp, PI_PDQ_K_REG_TYPE_2_PROD, bp->rcv_xmt_reg.lword);
	spin_unlock_irqrestore(&bp->lock, flags);
	netif_wake_queue(dev);
	return NETDEV_TX_OK;	
	}



static int dfx_xmt_done(DFX_board_t *bp)
	{
	XMT_DRIVER_DESCR	*p_xmt_drv_descr;	
	PI_TYPE_2_CONSUMER	*p_type_2_cons;		
	u8			comp;			
	int 			freed = 0;		

	

	p_type_2_cons = (PI_TYPE_2_CONSUMER *)(&bp->cons_block_virt->xmt_rcv_data);
	while (bp->rcv_xmt_reg.index.xmt_comp != p_type_2_cons->index.xmt_cons)
		{
		

		p_xmt_drv_descr = &(bp->xmt_drv_descr_blk[bp->rcv_xmt_reg.index.xmt_comp]);

		

		bp->xmt_total_frames++;
		bp->xmt_total_bytes += p_xmt_drv_descr->p_skb->len;

		
		comp = bp->rcv_xmt_reg.index.xmt_comp;
		dma_unmap_single(bp->bus_dev,
				 bp->descr_block_virt->xmt_data[comp].long_1,
				 p_xmt_drv_descr->p_skb->len,
				 DMA_TO_DEVICE);
		dev_kfree_skb_irq(p_xmt_drv_descr->p_skb);


		bp->rcv_xmt_reg.index.xmt_comp += 1;
		freed++;
		}
	return freed;
	}


#ifdef DYNAMIC_BUFFERS
static void dfx_rcv_flush( DFX_board_t *bp )
	{
	int i, j;

	for (i = 0; i < (int)(bp->rcv_bufs_to_post); i++)
		for (j = 0; (i + j) < (int)PI_RCV_DATA_K_NUM_ENTRIES; j += bp->rcv_bufs_to_post)
		{
			struct sk_buff *skb;
			skb = (struct sk_buff *)bp->p_rcv_buff_va[i+j];
			if (skb)
				dev_kfree_skb(skb);
			bp->p_rcv_buff_va[i+j] = NULL;
		}

	}
#else
static inline void dfx_rcv_flush( DFX_board_t *bp )
{
}
#endif 


static void dfx_xmt_flush( DFX_board_t *bp )
	{
	u32			prod_cons;		
	XMT_DRIVER_DESCR	*p_xmt_drv_descr;	
	u8			comp;			

	

	while (bp->rcv_xmt_reg.index.xmt_comp != bp->rcv_xmt_reg.index.xmt_prod)
		{
		

		p_xmt_drv_descr = &(bp->xmt_drv_descr_blk[bp->rcv_xmt_reg.index.xmt_comp]);

		
		comp = bp->rcv_xmt_reg.index.xmt_comp;
		dma_unmap_single(bp->bus_dev,
				 bp->descr_block_virt->xmt_data[comp].long_1,
				 p_xmt_drv_descr->p_skb->len,
				 DMA_TO_DEVICE);
		dev_kfree_skb(p_xmt_drv_descr->p_skb);

		

		bp->xmt_discards++;


		bp->rcv_xmt_reg.index.xmt_comp += 1;
		}

	

	prod_cons = (u32)(bp->cons_block_virt->xmt_rcv_data & ~PI_CONS_M_XMT_INDEX);
	prod_cons |= (u32)(bp->rcv_xmt_reg.index.xmt_prod << PI_CONS_V_XMT_INDEX);
	bp->cons_block_virt->xmt_rcv_data = prod_cons;
	}

static void __devexit dfx_unregister(struct device *bdev)
{
	struct net_device *dev = dev_get_drvdata(bdev);
	DFX_board_t *bp = netdev_priv(dev);
	int dfx_bus_pci = DFX_BUS_PCI(bdev);
	int dfx_bus_tc = DFX_BUS_TC(bdev);
	int dfx_use_mmio = DFX_MMIO || dfx_bus_tc;
	resource_size_t bar_start = 0;		
	resource_size_t bar_len = 0;		
	int		alloc_size;		

	unregister_netdev(dev);

	alloc_size = sizeof(PI_DESCR_BLOCK) +
		     PI_CMD_REQ_K_SIZE_MAX + PI_CMD_RSP_K_SIZE_MAX +
#ifndef DYNAMIC_BUFFERS
		     (bp->rcv_bufs_to_post * PI_RCV_DATA_K_SIZE_MAX) +
#endif
		     sizeof(PI_CONSUMER_BLOCK) +
		     (PI_ALIGN_K_DESC_BLK - 1);
	if (bp->kmalloced)
		dma_free_coherent(bdev, alloc_size,
				  bp->kmalloced, bp->kmalloced_dma);

	dfx_bus_uninit(dev);

	dfx_get_bars(bdev, &bar_start, &bar_len);
	if (dfx_use_mmio) {
		iounmap(bp->base.mem);
		release_mem_region(bar_start, bar_len);
	} else
		release_region(bar_start, bar_len);

	if (dfx_bus_pci)
		pci_disable_device(to_pci_dev(bdev));

	free_netdev(dev);
}


static int __devinit __maybe_unused dfx_dev_register(struct device *);
static int __devexit __maybe_unused dfx_dev_unregister(struct device *);

#ifdef CONFIG_PCI
static int __devinit dfx_pci_register(struct pci_dev *,
				      const struct pci_device_id *);
static void __devexit dfx_pci_unregister(struct pci_dev *);

static DEFINE_PCI_DEVICE_TABLE(dfx_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_FDDI) },
	{ }
};
MODULE_DEVICE_TABLE(pci, dfx_pci_table);

static struct pci_driver dfx_pci_driver = {
	.name		= "defxx",
	.id_table	= dfx_pci_table,
	.probe		= dfx_pci_register,
	.remove		= __devexit_p(dfx_pci_unregister),
};

static __devinit int dfx_pci_register(struct pci_dev *pdev,
				      const struct pci_device_id *ent)
{
	return dfx_register(&pdev->dev);
}

static void __devexit dfx_pci_unregister(struct pci_dev *pdev)
{
	dfx_unregister(&pdev->dev);
}
#endif 

#ifdef CONFIG_EISA
static struct eisa_device_id dfx_eisa_table[] = {
        { "DEC3001", DEFEA_PROD_ID_1 },
        { "DEC3002", DEFEA_PROD_ID_2 },
        { "DEC3003", DEFEA_PROD_ID_3 },
        { "DEC3004", DEFEA_PROD_ID_4 },
        { }
};
MODULE_DEVICE_TABLE(eisa, dfx_eisa_table);

static struct eisa_driver dfx_eisa_driver = {
	.id_table	= dfx_eisa_table,
	.driver		= {
		.name	= "defxx",
		.bus	= &eisa_bus_type,
		.probe	= dfx_dev_register,
		.remove	= __devexit_p(dfx_dev_unregister),
	},
};
#endif 

#ifdef CONFIG_TC
static struct tc_device_id const dfx_tc_table[] = {
	{ "DEC     ", "PMAF-FA " },
	{ "DEC     ", "PMAF-FD " },
	{ "DEC     ", "PMAF-FS " },
	{ "DEC     ", "PMAF-FU " },
	{ }
};
MODULE_DEVICE_TABLE(tc, dfx_tc_table);

static struct tc_driver dfx_tc_driver = {
	.id_table	= dfx_tc_table,
	.driver		= {
		.name	= "defxx",
		.bus	= &tc_bus_type,
		.probe	= dfx_dev_register,
		.remove	= __devexit_p(dfx_dev_unregister),
	},
};
#endif 

static int __devinit __maybe_unused dfx_dev_register(struct device *dev)
{
	int status;

	status = dfx_register(dev);
	if (!status)
		get_device(dev);
	return status;
}

static int __devexit __maybe_unused dfx_dev_unregister(struct device *dev)
{
	put_device(dev);
	dfx_unregister(dev);
	return 0;
}


static int __devinit dfx_init(void)
{
	int status;

	status = pci_register_driver(&dfx_pci_driver);
	if (!status)
		status = eisa_driver_register(&dfx_eisa_driver);
	if (!status)
		status = tc_register_driver(&dfx_tc_driver);
	return status;
}

static void __devexit dfx_cleanup(void)
{
	tc_unregister_driver(&dfx_tc_driver);
	eisa_driver_unregister(&dfx_eisa_driver);
	pci_unregister_driver(&dfx_pci_driver);
}

module_init(dfx_init);
module_exit(dfx_cleanup);
MODULE_AUTHOR("Lawrence V. Stefani");
MODULE_DESCRIPTION("DEC FDDIcontroller TC/EISA/PCI (DEFTA/DEFEA/DEFPA) driver "
		   DRV_VERSION " " DRV_RELDATE);
MODULE_LICENSE("GPL");
