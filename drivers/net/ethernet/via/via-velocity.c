/*
 * This code is derived from the VIA reference driver (copyright message
 * below) provided to Red Hat by VIA Networking Technologies, Inc. for
 * addition to the Linux kernel.
 *
 * The code has been merged into one source file, cleaned up to follow
 * Linux coding style,  ported to the Linux 2.6 kernel tree and cleaned
 * for 64bit hardware platforms.
 *
 * TODO
 *	rx_copybreak/alignment
 *	More testing
 *
 * The changes are (c) Copyright 2004, Red Hat Inc. <alan@lxorguk.ukuu.org.uk>
 * Additional fixes and clean up: Francois Romieu
 *
 * This source has not been verified for use in safety critical systems.
 *
 * Please direct queries about the revamped driver to the linux-kernel
 * list not VIA.
 *
 * Original code:
 *
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This software may be redistributed and/or modified under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * Author: Chuang Liang-Shing, AJ Jiang
 *
 * Date: Jan 24, 2003
 *
 * MODULE_LICENSE("GPL");
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/if.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/inetdevice.h>
#include <linux/reboot.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/in.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/crc-ccitt.h>
#include <linux/crc32.h>

#include "via-velocity.h"


static int velocity_nics;
static int msglevel = MSG_LEVEL_INFO;

static void mac_get_cam_mask(struct mac_regs __iomem *regs, u8 *mask)
{
	int i;

	
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_MASK, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	writeb(0, &regs->CAMADDR);

	
	for (i = 0; i < 8; i++)
		*mask++ = readb(&(regs->MARCAM[i]));

	
	writeb(0, &regs->CAMADDR);

	
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

static void mac_set_cam_mask(struct mac_regs __iomem *regs, u8 *mask)
{
	int i;
	
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_MASK, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	writeb(CAMADDR_CAMEN, &regs->CAMADDR);

	for (i = 0; i < 8; i++)
		writeb(*mask++, &(regs->MARCAM[i]));

	
	writeb(0, &regs->CAMADDR);

	
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

static void mac_set_vlan_cam_mask(struct mac_regs __iomem *regs, u8 *mask)
{
	int i;
	
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_MASK, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	writeb(CAMADDR_CAMEN | CAMADDR_VCAMSL, &regs->CAMADDR);

	for (i = 0; i < 8; i++)
		writeb(*mask++, &(regs->MARCAM[i]));

	
	writeb(0, &regs->CAMADDR);

	
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

static void mac_set_cam(struct mac_regs __iomem *regs, int idx, const u8 *addr)
{
	int i;

	
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_DATA, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	idx &= (64 - 1);

	writeb(CAMADDR_CAMEN | idx, &regs->CAMADDR);

	for (i = 0; i < 6; i++)
		writeb(*addr++, &(regs->MARCAM[i]));

	BYTE_REG_BITS_ON(CAMCR_CAMWR, &regs->CAMCR);

	udelay(10);

	writeb(0, &regs->CAMADDR);

	
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}

static void mac_set_vlan_cam(struct mac_regs __iomem *regs, int idx,
			     const u8 *addr)
{

	
	BYTE_REG_BITS_SET(CAMCR_PS_CAM_DATA, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);

	idx &= (64 - 1);

	writeb(CAMADDR_CAMEN | CAMADDR_VCAMSL | idx, &regs->CAMADDR);
	writew(*((u16 *) addr), &regs->MARCAM[0]);

	BYTE_REG_BITS_ON(CAMCR_CAMWR, &regs->CAMCR);

	udelay(10);

	writeb(0, &regs->CAMADDR);

	
	BYTE_REG_BITS_SET(CAMCR_PS_MAR, CAMCR_PS1 | CAMCR_PS0, &regs->CAMCR);
}


static void mac_wol_reset(struct mac_regs __iomem *regs)
{

	
	BYTE_REG_BITS_OFF(STICKHW_SWPTAG, &regs->STICKHW);
	
	BYTE_REG_BITS_OFF((STICKHW_DS1 | STICKHW_DS0), &regs->STICKHW);

	BYTE_REG_BITS_OFF(CHIPGCR_FCGMII, &regs->CHIPGCR);
	BYTE_REG_BITS_OFF(CHIPGCR_FCMODE, &regs->CHIPGCR);
	
	writeb(WOLCFG_PMEOVR, &regs->WOLCFGClr);
	
	writew(0xFFFF, &regs->WOLCRClr);
	
	writew(0xFFFF, &regs->WOLSRClr);
}

static const struct ethtool_ops velocity_ethtool_ops;


MODULE_AUTHOR("VIA Networking Technologies, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VIA Networking Velocity Family Gigabit Ethernet Adapter Driver");

#define VELOCITY_PARAM(N, D) \
	static int N[MAX_UNITS] = OPTION_DEFAULT;\
	module_param_array(N, int, NULL, 0); \
	MODULE_PARM_DESC(N, D);

#define RX_DESC_MIN     64
#define RX_DESC_MAX     255
#define RX_DESC_DEF     64
VELOCITY_PARAM(RxDescriptors, "Number of receive descriptors");

#define TX_DESC_MIN     16
#define TX_DESC_MAX     256
#define TX_DESC_DEF     64
VELOCITY_PARAM(TxDescriptors, "Number of transmit descriptors");

#define RX_THRESH_MIN   0
#define RX_THRESH_MAX   3
#define RX_THRESH_DEF   0
VELOCITY_PARAM(rx_thresh, "Receive fifo threshold");

#define DMA_LENGTH_MIN  0
#define DMA_LENGTH_MAX  7
#define DMA_LENGTH_DEF  6

VELOCITY_PARAM(DMA_length, "DMA length");

#define IP_ALIG_DEF     0
VELOCITY_PARAM(IP_byte_align, "Enable IP header dword aligned");

#define FLOW_CNTL_DEF   1
#define FLOW_CNTL_MIN   1
#define FLOW_CNTL_MAX   5

VELOCITY_PARAM(flow_control, "Enable flow control ability");

#define MED_LNK_DEF 0
#define MED_LNK_MIN 0
#define MED_LNK_MAX 5
VELOCITY_PARAM(speed_duplex, "Setting the speed and duplex mode");

#define VAL_PKT_LEN_DEF     0
VELOCITY_PARAM(ValPktLen, "Receiving or Drop invalid 802.3 frame");

#define WOL_OPT_DEF     0
#define WOL_OPT_MIN     0
#define WOL_OPT_MAX     7
VELOCITY_PARAM(wol_opts, "Wake On Lan options");

static int rx_copybreak = 200;
module_param(rx_copybreak, int, 0644);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");

static struct velocity_info_tbl chip_info_table[] = {
	{CHIP_TYPE_VT6110, "VIA Networking Velocity Family Gigabit Ethernet Adapter", 1, 0x00FFFFFFUL},
	{ }
};

static DEFINE_PCI_DEVICE_TABLE(velocity_id_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_612X) },
	{ }
};

MODULE_DEVICE_TABLE(pci, velocity_id_table);

static const char __devinit *get_chip_name(enum chip_type chip_id)
{
	int i;
	for (i = 0; chip_info_table[i].name != NULL; i++)
		if (chip_info_table[i].chip_id == chip_id)
			break;
	return chip_info_table[i].name;
}

static void __devexit velocity_remove1(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct velocity_info *vptr = netdev_priv(dev);

	unregister_netdev(dev);
	iounmap(vptr->mac_regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	free_netdev(dev);

	velocity_nics--;
}

static void __devinit velocity_set_int_opt(int *opt, int val, int min, int max, int def, char *name, const char *devname)
{
	if (val == -1)
		*opt = def;
	else if (val < min || val > max) {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: the value of parameter %s is invalid, the valid range is (%d-%d)\n",
					devname, name, min, max);
		*opt = def;
	} else {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_INFO "%s: set value of parameter %s to %d\n",
					devname, name, val);
		*opt = val;
	}
}

static void __devinit velocity_set_bool_opt(u32 *opt, int val, int def, u32 flag, char *name, const char *devname)
{
	(*opt) &= (~flag);
	if (val == -1)
		*opt |= (def ? flag : 0);
	else if (val < 0 || val > 1) {
		printk(KERN_NOTICE "%s: the value of parameter %s is invalid, the valid range is (0-1)\n",
			devname, name);
		*opt |= (def ? flag : 0);
	} else {
		printk(KERN_INFO "%s: set parameter %s to %s\n",
			devname, name, val ? "TRUE" : "FALSE");
		*opt |= (val ? flag : 0);
	}
}

static void __devinit velocity_get_options(struct velocity_opt *opts, int index, const char *devname)
{

	velocity_set_int_opt(&opts->rx_thresh, rx_thresh[index], RX_THRESH_MIN, RX_THRESH_MAX, RX_THRESH_DEF, "rx_thresh", devname);
	velocity_set_int_opt(&opts->DMA_length, DMA_length[index], DMA_LENGTH_MIN, DMA_LENGTH_MAX, DMA_LENGTH_DEF, "DMA_length", devname);
	velocity_set_int_opt(&opts->numrx, RxDescriptors[index], RX_DESC_MIN, RX_DESC_MAX, RX_DESC_DEF, "RxDescriptors", devname);
	velocity_set_int_opt(&opts->numtx, TxDescriptors[index], TX_DESC_MIN, TX_DESC_MAX, TX_DESC_DEF, "TxDescriptors", devname);

	velocity_set_int_opt(&opts->flow_cntl, flow_control[index], FLOW_CNTL_MIN, FLOW_CNTL_MAX, FLOW_CNTL_DEF, "flow_control", devname);
	velocity_set_bool_opt(&opts->flags, IP_byte_align[index], IP_ALIG_DEF, VELOCITY_FLAGS_IP_ALIGN, "IP_byte_align", devname);
	velocity_set_bool_opt(&opts->flags, ValPktLen[index], VAL_PKT_LEN_DEF, VELOCITY_FLAGS_VAL_PKT_LEN, "ValPktLen", devname);
	velocity_set_int_opt((int *) &opts->spd_dpx, speed_duplex[index], MED_LNK_MIN, MED_LNK_MAX, MED_LNK_DEF, "Media link mode", devname);
	velocity_set_int_opt((int *) &opts->wol_opts, wol_opts[index], WOL_OPT_MIN, WOL_OPT_MAX, WOL_OPT_DEF, "Wake On Lan options", devname);
	opts->numrx = (opts->numrx & ~3);
}

static void velocity_init_cam_filter(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	unsigned int vid, i = 0;

	
	WORD_REG_BITS_SET(MCFG_PQEN, MCFG_RTGOPT, &regs->MCFG);
	WORD_REG_BITS_ON(MCFG_VIDFR, &regs->MCFG);

	
	memset(vptr->vCAMmask, 0, sizeof(u8) * 8);
	memset(vptr->mCAMmask, 0, sizeof(u8) * 8);
	mac_set_vlan_cam_mask(regs, vptr->vCAMmask);
	mac_set_cam_mask(regs, vptr->mCAMmask);

	
	for_each_set_bit(vid, vptr->active_vlans, VLAN_N_VID) {
		mac_set_vlan_cam(regs, i, (u8 *) &vid);
		vptr->vCAMmask[i / 8] |= 0x1 << (i % 8);
		if (++i >= VCAM_SIZE)
			break;
	}
	mac_set_vlan_cam_mask(regs, vptr->vCAMmask);
}

static int velocity_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct velocity_info *vptr = netdev_priv(dev);

	spin_lock_irq(&vptr->lock);
	set_bit(vid, vptr->active_vlans);
	velocity_init_cam_filter(vptr);
	spin_unlock_irq(&vptr->lock);
	return 0;
}

static int velocity_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct velocity_info *vptr = netdev_priv(dev);

	spin_lock_irq(&vptr->lock);
	clear_bit(vid, vptr->active_vlans);
	velocity_init_cam_filter(vptr);
	spin_unlock_irq(&vptr->lock);
	return 0;
}

static void velocity_init_rx_ring_indexes(struct velocity_info *vptr)
{
	vptr->rx.dirty = vptr->rx.filled = vptr->rx.curr = 0;
}

static void velocity_rx_reset(struct velocity_info *vptr)
{

	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i;

	velocity_init_rx_ring_indexes(vptr);

	for (i = 0; i < vptr->options.numrx; ++i)
		vptr->rx.ring[i].rdesc0.len |= OWNED_BY_NIC;

	writew(vptr->options.numrx, &regs->RBRDU);
	writel(vptr->rx.pool_dma, &regs->RDBaseLo);
	writew(0, &regs->RDIdx);
	writew(vptr->options.numrx - 1, &regs->RDCSize);
}

static u32 velocity_get_opt_media_mode(struct velocity_info *vptr)
{
	u32 status = 0;

	switch (vptr->options.spd_dpx) {
	case SPD_DPX_AUTO:
		status = VELOCITY_AUTONEG_ENABLE;
		break;
	case SPD_DPX_100_FULL:
		status = VELOCITY_SPEED_100 | VELOCITY_DUPLEX_FULL;
		break;
	case SPD_DPX_10_FULL:
		status = VELOCITY_SPEED_10 | VELOCITY_DUPLEX_FULL;
		break;
	case SPD_DPX_100_HALF:
		status = VELOCITY_SPEED_100;
		break;
	case SPD_DPX_10_HALF:
		status = VELOCITY_SPEED_10;
		break;
	case SPD_DPX_1000_FULL:
		status = VELOCITY_SPEED_1000 | VELOCITY_DUPLEX_FULL;
		break;
	}
	vptr->mii_status = status;
	return status;
}

static void safe_disable_mii_autopoll(struct mac_regs __iomem *regs)
{
	u16 ww;

	
	writeb(0, &regs->MIICR);
	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		udelay(1);
		if (BYTE_REG_BITS_IS_ON(MIISR_MIDLE, &regs->MIISR))
			break;
	}
}

static void enable_mii_autopoll(struct mac_regs __iomem *regs)
{
	int ii;

	writeb(0, &(regs->MIICR));
	writeb(MIIADR_SWMPL, &regs->MIIADR);

	for (ii = 0; ii < W_MAX_TIMEOUT; ii++) {
		udelay(1);
		if (BYTE_REG_BITS_IS_ON(MIISR_MIDLE, &regs->MIISR))
			break;
	}

	writeb(MIICR_MAUTO, &regs->MIICR);

	for (ii = 0; ii < W_MAX_TIMEOUT; ii++) {
		udelay(1);
		if (!BYTE_REG_BITS_IS_ON(MIISR_MIDLE, &regs->MIISR))
			break;
	}

}

static int velocity_mii_read(struct mac_regs __iomem *regs, u8 index, u16 *data)
{
	u16 ww;

	safe_disable_mii_autopoll(regs);

	writeb(index, &regs->MIIADR);

	BYTE_REG_BITS_ON(MIICR_RCMD, &regs->MIICR);

	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		if (!(readb(&regs->MIICR) & MIICR_RCMD))
			break;
	}

	*data = readw(&regs->MIIDATA);

	enable_mii_autopoll(regs);
	if (ww == W_MAX_TIMEOUT)
		return -ETIMEDOUT;
	return 0;
}

static u32 mii_check_media_mode(struct mac_regs __iomem *regs)
{
	u32 status = 0;
	u16 ANAR;

	if (!MII_REG_BITS_IS_ON(BMSR_LSTATUS, MII_BMSR, regs))
		status |= VELOCITY_LINK_FAIL;

	if (MII_REG_BITS_IS_ON(ADVERTISE_1000FULL, MII_CTRL1000, regs))
		status |= VELOCITY_SPEED_1000 | VELOCITY_DUPLEX_FULL;
	else if (MII_REG_BITS_IS_ON(ADVERTISE_1000HALF, MII_CTRL1000, regs))
		status |= (VELOCITY_SPEED_1000);
	else {
		velocity_mii_read(regs, MII_ADVERTISE, &ANAR);
		if (ANAR & ADVERTISE_100FULL)
			status |= (VELOCITY_SPEED_100 | VELOCITY_DUPLEX_FULL);
		else if (ANAR & ADVERTISE_100HALF)
			status |= VELOCITY_SPEED_100;
		else if (ANAR & ADVERTISE_10FULL)
			status |= (VELOCITY_SPEED_10 | VELOCITY_DUPLEX_FULL);
		else
			status |= (VELOCITY_SPEED_10);
	}

	if (MII_REG_BITS_IS_ON(BMCR_ANENABLE, MII_BMCR, regs)) {
		velocity_mii_read(regs, MII_ADVERTISE, &ANAR);
		if ((ANAR & (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF))
		    == (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF)) {
			if (MII_REG_BITS_IS_ON(ADVERTISE_1000HALF | ADVERTISE_1000FULL, MII_CTRL1000, regs))
				status |= VELOCITY_AUTONEG_ENABLE;
		}
	}

	return status;
}

static int velocity_mii_write(struct mac_regs __iomem *regs, u8 mii_addr, u16 data)
{
	u16 ww;

	safe_disable_mii_autopoll(regs);

	
	writeb(mii_addr, &regs->MIIADR);
	
	writew(data, &regs->MIIDATA);

	
	BYTE_REG_BITS_ON(MIICR_WCMD, &regs->MIICR);

	
	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		udelay(5);
		if (!(readb(&regs->MIICR) & MIICR_WCMD))
			break;
	}
	enable_mii_autopoll(regs);

	if (ww == W_MAX_TIMEOUT)
		return -ETIMEDOUT;
	return 0;
}

static void set_mii_flow_control(struct velocity_info *vptr)
{
	
	switch (vptr->options.flow_cntl) {
	case FLOW_CNTL_TX:
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_ON(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;

	case FLOW_CNTL_RX:
		MII_REG_BITS_ON(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_ON(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;

	case FLOW_CNTL_TX_RX:
		MII_REG_BITS_ON(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;

	case FLOW_CNTL_DISABLE:
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_CAP, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_OFF(ADVERTISE_PAUSE_ASYM, MII_ADVERTISE, vptr->mac_regs);
		break;
	default:
		break;
	}
}

static void mii_set_auto_on(struct velocity_info *vptr)
{
	if (MII_REG_BITS_IS_ON(BMCR_ANENABLE, MII_BMCR, vptr->mac_regs))
		MII_REG_BITS_ON(BMCR_ANRESTART, MII_BMCR, vptr->mac_regs);
	else
		MII_REG_BITS_ON(BMCR_ANENABLE, MII_BMCR, vptr->mac_regs);
}

static u32 check_connection_type(struct mac_regs __iomem *regs)
{
	u32 status = 0;
	u8 PHYSR0;
	u16 ANAR;
	PHYSR0 = readb(&regs->PHYSR0);


	if (PHYSR0 & PHYSR0_FDPX)
		status |= VELOCITY_DUPLEX_FULL;

	if (PHYSR0 & PHYSR0_SPDG)
		status |= VELOCITY_SPEED_1000;
	else if (PHYSR0 & PHYSR0_SPD10)
		status |= VELOCITY_SPEED_10;
	else
		status |= VELOCITY_SPEED_100;

	if (MII_REG_BITS_IS_ON(BMCR_ANENABLE, MII_BMCR, regs)) {
		velocity_mii_read(regs, MII_ADVERTISE, &ANAR);
		if ((ANAR & (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF))
		    == (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF)) {
			if (MII_REG_BITS_IS_ON(ADVERTISE_1000HALF | ADVERTISE_1000FULL, MII_CTRL1000, regs))
				status |= VELOCITY_AUTONEG_ENABLE;
		}
	}

	return status;
}

static int velocity_set_media_mode(struct velocity_info *vptr, u32 mii_status)
{
	u32 curr_status;
	struct mac_regs __iomem *regs = vptr->mac_regs;

	vptr->mii_status = mii_check_media_mode(vptr->mac_regs);
	curr_status = vptr->mii_status & (~VELOCITY_LINK_FAIL);

	
	set_mii_flow_control(vptr);


	if (PHYID_GET_PHY_ID(vptr->phy_id) == PHYID_CICADA_CS8201)
		MII_REG_BITS_ON(AUXCR_MDPPS, MII_NCONFIG, vptr->mac_regs);

	if (mii_status & VELOCITY_AUTONEG_ENABLE) {
		VELOCITY_PRT(MSG_LEVEL_INFO, "Velocity is AUTO mode\n");
		
		BYTE_REG_BITS_OFF(CHIPGCR_FCMODE, &regs->CHIPGCR);
		
		MII_REG_BITS_ON(ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF, MII_ADVERTISE, vptr->mac_regs);
		MII_REG_BITS_ON(ADVERTISE_1000FULL | ADVERTISE_1000HALF, MII_CTRL1000, vptr->mac_regs);
		MII_REG_BITS_ON(BMCR_SPEED1000, MII_BMCR, vptr->mac_regs);

		
		mii_set_auto_on(vptr);
	} else {
		u16 CTRL1000;
		u16 ANAR;
		u8 CHIPGCR;


		
		BYTE_REG_BITS_ON(CHIPGCR_FCMODE, &regs->CHIPGCR);

		CHIPGCR = readb(&regs->CHIPGCR);

		if (mii_status & VELOCITY_SPEED_1000)
			CHIPGCR |= CHIPGCR_FCGMII;
		else
			CHIPGCR &= ~CHIPGCR_FCGMII;

		if (mii_status & VELOCITY_DUPLEX_FULL) {
			CHIPGCR |= CHIPGCR_FCFDX;
			writeb(CHIPGCR, &regs->CHIPGCR);
			VELOCITY_PRT(MSG_LEVEL_INFO, "set Velocity to forced full mode\n");
			if (vptr->rev_id < REV_ID_VT3216_A0)
				BYTE_REG_BITS_OFF(TCR_TB2BDIS, &regs->TCR);
		} else {
			CHIPGCR &= ~CHIPGCR_FCFDX;
			VELOCITY_PRT(MSG_LEVEL_INFO, "set Velocity to forced half mode\n");
			writeb(CHIPGCR, &regs->CHIPGCR);
			if (vptr->rev_id < REV_ID_VT3216_A0)
				BYTE_REG_BITS_ON(TCR_TB2BDIS, &regs->TCR);
		}

		velocity_mii_read(vptr->mac_regs, MII_CTRL1000, &CTRL1000);
		CTRL1000 &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);
		if ((mii_status & VELOCITY_SPEED_1000) &&
		    (mii_status & VELOCITY_DUPLEX_FULL)) {
			CTRL1000 |= ADVERTISE_1000FULL;
		}
		velocity_mii_write(vptr->mac_regs, MII_CTRL1000, CTRL1000);

		if (!(mii_status & VELOCITY_DUPLEX_FULL) && (mii_status & VELOCITY_SPEED_10))
			BYTE_REG_BITS_OFF(TESTCFG_HBDIS, &regs->TESTCFG);
		else
			BYTE_REG_BITS_ON(TESTCFG_HBDIS, &regs->TESTCFG);

		
		velocity_mii_read(vptr->mac_regs, MII_ADVERTISE, &ANAR);
		ANAR &= (~(ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10FULL | ADVERTISE_10HALF));
		if (mii_status & VELOCITY_SPEED_100) {
			if (mii_status & VELOCITY_DUPLEX_FULL)
				ANAR |= ADVERTISE_100FULL;
			else
				ANAR |= ADVERTISE_100HALF;
		} else if (mii_status & VELOCITY_SPEED_10) {
			if (mii_status & VELOCITY_DUPLEX_FULL)
				ANAR |= ADVERTISE_10FULL;
			else
				ANAR |= ADVERTISE_10HALF;
		}
		velocity_mii_write(vptr->mac_regs, MII_ADVERTISE, ANAR);
		
		mii_set_auto_on(vptr);
		
	}
	
	
	return VELOCITY_LINK_CHANGE;
}

static void velocity_print_link_status(struct velocity_info *vptr)
{

	if (vptr->mii_status & VELOCITY_LINK_FAIL) {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: failed to detect cable link\n", vptr->dev->name);
	} else if (vptr->options.spd_dpx == SPD_DPX_AUTO) {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: Link auto-negotiation", vptr->dev->name);

		if (vptr->mii_status & VELOCITY_SPEED_1000)
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 1000M bps");
		else if (vptr->mii_status & VELOCITY_SPEED_100)
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 100M bps");
		else
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 10M bps");

		if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
			VELOCITY_PRT(MSG_LEVEL_INFO, " full duplex\n");
		else
			VELOCITY_PRT(MSG_LEVEL_INFO, " half duplex\n");
	} else {
		VELOCITY_PRT(MSG_LEVEL_INFO, KERN_NOTICE "%s: Link forced", vptr->dev->name);
		switch (vptr->options.spd_dpx) {
		case SPD_DPX_1000_FULL:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 1000M bps full duplex\n");
			break;
		case SPD_DPX_100_HALF:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 100M bps half duplex\n");
			break;
		case SPD_DPX_100_FULL:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 100M bps full duplex\n");
			break;
		case SPD_DPX_10_HALF:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 10M bps half duplex\n");
			break;
		case SPD_DPX_10_FULL:
			VELOCITY_PRT(MSG_LEVEL_INFO, " speed 10M bps full duplex\n");
			break;
		default:
			break;
		}
	}
}

static void enable_flow_control_ability(struct velocity_info *vptr)
{

	struct mac_regs __iomem *regs = vptr->mac_regs;

	switch (vptr->options.flow_cntl) {

	case FLOW_CNTL_DEFAULT:
		if (BYTE_REG_BITS_IS_ON(PHYSR0_RXFLC, &regs->PHYSR0))
			writel(CR0_FDXRFCEN, &regs->CR0Set);
		else
			writel(CR0_FDXRFCEN, &regs->CR0Clr);

		if (BYTE_REG_BITS_IS_ON(PHYSR0_TXFLC, &regs->PHYSR0))
			writel(CR0_FDXTFCEN, &regs->CR0Set);
		else
			writel(CR0_FDXTFCEN, &regs->CR0Clr);
		break;

	case FLOW_CNTL_TX:
		writel(CR0_FDXTFCEN, &regs->CR0Set);
		writel(CR0_FDXRFCEN, &regs->CR0Clr);
		break;

	case FLOW_CNTL_RX:
		writel(CR0_FDXRFCEN, &regs->CR0Set);
		writel(CR0_FDXTFCEN, &regs->CR0Clr);
		break;

	case FLOW_CNTL_TX_RX:
		writel(CR0_FDXTFCEN, &regs->CR0Set);
		writel(CR0_FDXRFCEN, &regs->CR0Set);
		break;

	case FLOW_CNTL_DISABLE:
		writel(CR0_FDXRFCEN, &regs->CR0Clr);
		writel(CR0_FDXTFCEN, &regs->CR0Clr);
		break;

	default:
		break;
	}

}

static int velocity_soft_reset(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i = 0;

	writel(CR0_SFRST, &regs->CR0Set);

	for (i = 0; i < W_MAX_TIMEOUT; i++) {
		udelay(5);
		if (!DWORD_REG_BITS_IS_ON(CR0_SFRST, &regs->CR0Set))
			break;
	}

	if (i == W_MAX_TIMEOUT) {
		writel(CR0_FORSRST, &regs->CR0Set);
		
		
		mdelay(2);
	}
	return 0;
}

static void velocity_set_multi(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u8 rx_mode;
	int i;
	struct netdev_hw_addr *ha;

	if (dev->flags & IFF_PROMISC) {	
		writel(0xffffffff, &regs->MARCAM[0]);
		writel(0xffffffff, &regs->MARCAM[4]);
		rx_mode = (RCR_AM | RCR_AB | RCR_PROM);
	} else if ((netdev_mc_count(dev) > vptr->multicast_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		writel(0xffffffff, &regs->MARCAM[0]);
		writel(0xffffffff, &regs->MARCAM[4]);
		rx_mode = (RCR_AM | RCR_AB);
	} else {
		int offset = MCAM_SIZE - vptr->multicast_limit;
		mac_get_cam_mask(regs, vptr->mCAMmask);

		i = 0;
		netdev_for_each_mc_addr(ha, dev) {
			mac_set_cam(regs, i + offset, ha->addr);
			vptr->mCAMmask[(offset + i) / 8] |= 1 << ((offset + i) & 7);
			i++;
		}

		mac_set_cam_mask(regs, vptr->mCAMmask);
		rx_mode = RCR_AM | RCR_AB | RCR_AP;
	}
	if (dev->mtu > 1500)
		rx_mode |= RCR_AL;

	BYTE_REG_BITS_ON(rx_mode, &regs->RCR);

}


static void mii_init(struct velocity_info *vptr, u32 mii_status)
{
	u16 BMCR;

	switch (PHYID_GET_PHY_ID(vptr->phy_id)) {
	case PHYID_CICADA_CS8201:
		MII_REG_BITS_OFF((ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP), MII_ADVERTISE, vptr->mac_regs);
		if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
			MII_REG_BITS_ON(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		else
			MII_REG_BITS_OFF(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		MII_REG_BITS_ON(PLED_LALBE, MII_TPISTATUS, vptr->mac_regs);
		break;
	case PHYID_VT3216_32BIT:
	case PHYID_VT3216_64BIT:
		MII_REG_BITS_ON((ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP), MII_ADVERTISE, vptr->mac_regs);
		if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
			MII_REG_BITS_ON(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		else
			MII_REG_BITS_OFF(TCSR_ECHODIS, MII_SREVISION, vptr->mac_regs);
		break;

	case PHYID_MARVELL_1000:
	case PHYID_MARVELL_1000S:
		MII_REG_BITS_ON(PSCR_ACRSTX, MII_REG_PSCR, vptr->mac_regs);
		MII_REG_BITS_ON((ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP), MII_ADVERTISE, vptr->mac_regs);
		break;
	default:
		;
	}
	velocity_mii_read(vptr->mac_regs, MII_BMCR, &BMCR);
	if (BMCR & BMCR_ISOLATE) {
		BMCR &= ~BMCR_ISOLATE;
		velocity_mii_write(vptr->mac_regs, MII_BMCR, BMCR);
	}
}

static void setup_queue_timers(struct velocity_info *vptr)
{
	
	if (vptr->rev_id >= REV_ID_VT3216_A0) {
		u8 txqueue_timer = 0;
		u8 rxqueue_timer = 0;

		if (vptr->mii_status & (VELOCITY_SPEED_1000 |
				VELOCITY_SPEED_100)) {
			txqueue_timer = vptr->options.txqueue_timer;
			rxqueue_timer = vptr->options.rxqueue_timer;
		}

		writeb(txqueue_timer, &vptr->mac_regs->TQETMR);
		writeb(rxqueue_timer, &vptr->mac_regs->RQETMR);
	}
}

static void setup_adaptive_interrupts(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u16 tx_intsup = vptr->options.tx_intsup;
	u16 rx_intsup = vptr->options.rx_intsup;

	
	vptr->int_mask = INT_MASK_DEF;

	
	writeb(CAMCR_PS0, &regs->CAMCR);
	if (tx_intsup != 0) {
		vptr->int_mask &= ~(ISR_PTXI | ISR_PTX0I | ISR_PTX1I |
				ISR_PTX2I | ISR_PTX3I);
		writew(tx_intsup, &regs->ISRCTL);
	} else
		writew(ISRCTL_TSUPDIS, &regs->ISRCTL);

	
	writeb(CAMCR_PS1, &regs->CAMCR);
	if (rx_intsup != 0) {
		vptr->int_mask &= ~ISR_PRXI;
		writew(rx_intsup, &regs->ISRCTL);
	} else
		writew(ISRCTL_RSUPDIS, &regs->ISRCTL);

	
	writeb(0, &regs->CAMCR);
}

static void velocity_init_registers(struct velocity_info *vptr,
				    enum velocity_init_type type)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i, mii_status;

	mac_wol_reset(regs);

	switch (type) {
	case VELOCITY_INIT_RESET:
	case VELOCITY_INIT_WOL:

		netif_stop_queue(vptr->dev);

		velocity_rx_reset(vptr);
		mac_rx_queue_run(regs);
		mac_rx_queue_wake(regs);

		mii_status = velocity_get_opt_media_mode(vptr);
		if (velocity_set_media_mode(vptr, mii_status) != VELOCITY_LINK_CHANGE) {
			velocity_print_link_status(vptr);
			if (!(vptr->mii_status & VELOCITY_LINK_FAIL))
				netif_wake_queue(vptr->dev);
		}

		enable_flow_control_ability(vptr);

		mac_clear_isr(regs);
		writel(CR0_STOP, &regs->CR0Clr);
		writel((CR0_DPOLL | CR0_TXON | CR0_RXON | CR0_STRT),
							&regs->CR0Set);

		break;

	case VELOCITY_INIT_COLD:
	default:
		velocity_soft_reset(vptr);
		mdelay(5);

		mac_eeprom_reload(regs);
		for (i = 0; i < 6; i++)
			writeb(vptr->dev->dev_addr[i], &(regs->PAR[i]));

		BYTE_REG_BITS_OFF(CFGA_PACPI, &(regs->CFGA));
		mac_set_rx_thresh(regs, vptr->options.rx_thresh);
		mac_set_dma_length(regs, vptr->options.DMA_length);

		writeb(WOLCFG_SAM | WOLCFG_SAB, &regs->WOLCFGSet);
		BYTE_REG_BITS_SET(CFGB_OFSET, (CFGB_CRANDOM | CFGB_CAP | CFGB_MBA | CFGB_BAKOPT), &regs->CFGB);

		velocity_init_cam_filter(vptr);

		velocity_set_multi(vptr->dev);

		enable_mii_autopoll(regs);

		setup_adaptive_interrupts(vptr);

		writel(vptr->rx.pool_dma, &regs->RDBaseLo);
		writew(vptr->options.numrx - 1, &regs->RDCSize);
		mac_rx_queue_run(regs);
		mac_rx_queue_wake(regs);

		writew(vptr->options.numtx - 1, &regs->TDCSize);

		for (i = 0; i < vptr->tx.numq; i++) {
			writel(vptr->tx.pool_dma[i], &regs->TDBaseLo[i]);
			mac_tx_queue_run(regs, i);
		}

		init_flow_control_register(vptr);

		writel(CR0_STOP, &regs->CR0Clr);
		writel((CR0_DPOLL | CR0_TXON | CR0_RXON | CR0_STRT), &regs->CR0Set);

		mii_status = velocity_get_opt_media_mode(vptr);
		netif_stop_queue(vptr->dev);

		mii_init(vptr, mii_status);

		if (velocity_set_media_mode(vptr, mii_status) != VELOCITY_LINK_CHANGE) {
			velocity_print_link_status(vptr);
			if (!(vptr->mii_status & VELOCITY_LINK_FAIL))
				netif_wake_queue(vptr->dev);
		}

		enable_flow_control_ability(vptr);
		mac_hw_mibs_init(regs);
		mac_write_int_mask(vptr->int_mask, regs);
		mac_clear_isr(regs);

	}
}

static void velocity_give_many_rx_descs(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int avail, dirty, unusable;

	if (vptr->rx.filled < 4)
		return;

	wmb();

	unusable = vptr->rx.filled & 0x0003;
	dirty = vptr->rx.dirty - unusable;
	for (avail = vptr->rx.filled & 0xfffc; avail; avail--) {
		dirty = (dirty > 0) ? dirty - 1 : vptr->options.numrx - 1;
		vptr->rx.ring[dirty].rdesc0.len |= OWNED_BY_NIC;
	}

	writew(vptr->rx.filled & 0xfffc, &regs->RBRDU);
	vptr->rx.filled = unusable;
}

static int velocity_init_dma_rings(struct velocity_info *vptr)
{
	struct velocity_opt *opt = &vptr->options;
	const unsigned int rx_ring_size = opt->numrx * sizeof(struct rx_desc);
	const unsigned int tx_ring_size = opt->numtx * sizeof(struct tx_desc);
	struct pci_dev *pdev = vptr->pdev;
	dma_addr_t pool_dma;
	void *pool;
	unsigned int i;

	pool = pci_alloc_consistent(pdev, tx_ring_size * vptr->tx.numq +
				    rx_ring_size, &pool_dma);
	if (!pool) {
		dev_err(&pdev->dev, "%s : DMA memory allocation failed.\n",
			vptr->dev->name);
		return -ENOMEM;
	}

	vptr->rx.ring = pool;
	vptr->rx.pool_dma = pool_dma;

	pool += rx_ring_size;
	pool_dma += rx_ring_size;

	for (i = 0; i < vptr->tx.numq; i++) {
		vptr->tx.rings[i] = pool;
		vptr->tx.pool_dma[i] = pool_dma;
		pool += tx_ring_size;
		pool_dma += tx_ring_size;
	}

	return 0;
}

static void velocity_set_rxbufsize(struct velocity_info *vptr, int mtu)
{
	vptr->rx.buf_sz = (mtu <= ETH_DATA_LEN) ? PKT_BUF_SZ : mtu + 32;
}

static int velocity_alloc_rx_buf(struct velocity_info *vptr, int idx)
{
	struct rx_desc *rd = &(vptr->rx.ring[idx]);
	struct velocity_rd_info *rd_info = &(vptr->rx.info[idx]);

	rd_info->skb = netdev_alloc_skb(vptr->dev, vptr->rx.buf_sz + 64);
	if (rd_info->skb == NULL)
		return -ENOMEM;

	skb_reserve(rd_info->skb,
			64 - ((unsigned long) rd_info->skb->data & 63));
	rd_info->skb_dma = pci_map_single(vptr->pdev, rd_info->skb->data,
					vptr->rx.buf_sz, PCI_DMA_FROMDEVICE);


	*((u32 *) & (rd->rdesc0)) = 0;
	rd->size = cpu_to_le16(vptr->rx.buf_sz) | RX_INTEN;
	rd->pa_low = cpu_to_le32(rd_info->skb_dma);
	rd->pa_high = 0;
	return 0;
}


static int velocity_rx_refill(struct velocity_info *vptr)
{
	int dirty = vptr->rx.dirty, done = 0;

	do {
		struct rx_desc *rd = vptr->rx.ring + dirty;

		
		if (rd->rdesc0.len & OWNED_BY_NIC)
			break;

		if (!vptr->rx.info[dirty].skb) {
			if (velocity_alloc_rx_buf(vptr, dirty) < 0)
				break;
		}
		done++;
		dirty = (dirty < vptr->options.numrx - 1) ? dirty + 1 : 0;
	} while (dirty != vptr->rx.curr);

	if (done) {
		vptr->rx.dirty = dirty;
		vptr->rx.filled += done;
	}

	return done;
}

static void velocity_free_rd_ring(struct velocity_info *vptr)
{
	int i;

	if (vptr->rx.info == NULL)
		return;

	for (i = 0; i < vptr->options.numrx; i++) {
		struct velocity_rd_info *rd_info = &(vptr->rx.info[i]);
		struct rx_desc *rd = vptr->rx.ring + i;

		memset(rd, 0, sizeof(*rd));

		if (!rd_info->skb)
			continue;
		pci_unmap_single(vptr->pdev, rd_info->skb_dma, vptr->rx.buf_sz,
				 PCI_DMA_FROMDEVICE);
		rd_info->skb_dma = 0;

		dev_kfree_skb(rd_info->skb);
		rd_info->skb = NULL;
	}

	kfree(vptr->rx.info);
	vptr->rx.info = NULL;
}

static int velocity_init_rd_ring(struct velocity_info *vptr)
{
	int ret = -ENOMEM;

	vptr->rx.info = kcalloc(vptr->options.numrx,
				sizeof(struct velocity_rd_info), GFP_KERNEL);
	if (!vptr->rx.info)
		goto out;

	velocity_init_rx_ring_indexes(vptr);

	if (velocity_rx_refill(vptr) != vptr->options.numrx) {
		VELOCITY_PRT(MSG_LEVEL_ERR, KERN_ERR
			"%s: failed to allocate RX buffer.\n", vptr->dev->name);
		velocity_free_rd_ring(vptr);
		goto out;
	}

	ret = 0;
out:
	return ret;
}

static int velocity_init_td_ring(struct velocity_info *vptr)
{
	int j;

	
	for (j = 0; j < vptr->tx.numq; j++) {

		vptr->tx.infos[j] = kcalloc(vptr->options.numtx,
					    sizeof(struct velocity_td_info),
					    GFP_KERNEL);
		if (!vptr->tx.infos[j])	{
			while (--j >= 0)
				kfree(vptr->tx.infos[j]);
			return -ENOMEM;
		}

		vptr->tx.tail[j] = vptr->tx.curr[j] = vptr->tx.used[j] = 0;
	}
	return 0;
}

static void velocity_free_dma_rings(struct velocity_info *vptr)
{
	const int size = vptr->options.numrx * sizeof(struct rx_desc) +
		vptr->options.numtx * sizeof(struct tx_desc) * vptr->tx.numq;

	pci_free_consistent(vptr->pdev, size, vptr->rx.ring, vptr->rx.pool_dma);
}

static int velocity_init_rings(struct velocity_info *vptr, int mtu)
{
	int ret;

	velocity_set_rxbufsize(vptr, mtu);

	ret = velocity_init_dma_rings(vptr);
	if (ret < 0)
		goto out;

	ret = velocity_init_rd_ring(vptr);
	if (ret < 0)
		goto err_free_dma_rings_0;

	ret = velocity_init_td_ring(vptr);
	if (ret < 0)
		goto err_free_rd_ring_1;
out:
	return ret;

err_free_rd_ring_1:
	velocity_free_rd_ring(vptr);
err_free_dma_rings_0:
	velocity_free_dma_rings(vptr);
	goto out;
}

static void velocity_free_tx_buf(struct velocity_info *vptr,
		struct velocity_td_info *tdinfo, struct tx_desc *td)
{
	struct sk_buff *skb = tdinfo->skb;

	if (tdinfo->skb_dma) {
		int i;

		for (i = 0; i < tdinfo->nskb_dma; i++) {
			size_t pktlen = max_t(size_t, skb->len, ETH_ZLEN);

			
			if (skb_shinfo(skb)->nr_frags > 0)
				pktlen = max_t(size_t, pktlen,
						td->td_buf[i].size & ~TD_QUEUE);

			pci_unmap_single(vptr->pdev, tdinfo->skb_dma[i],
					le16_to_cpu(pktlen), PCI_DMA_TODEVICE);
		}
	}
	dev_kfree_skb_irq(skb);
	tdinfo->skb = NULL;
}

static void velocity_free_td_ring_entry(struct velocity_info *vptr,
							 int q, int n)
{
	struct velocity_td_info *td_info = &(vptr->tx.infos[q][n]);
	int i;

	if (td_info == NULL)
		return;

	if (td_info->skb) {
		for (i = 0; i < td_info->nskb_dma; i++) {
			if (td_info->skb_dma[i]) {
				pci_unmap_single(vptr->pdev, td_info->skb_dma[i],
					td_info->skb->len, PCI_DMA_TODEVICE);
				td_info->skb_dma[i] = 0;
			}
		}
		dev_kfree_skb(td_info->skb);
		td_info->skb = NULL;
	}
}

static void velocity_free_td_ring(struct velocity_info *vptr)
{
	int i, j;

	for (j = 0; j < vptr->tx.numq; j++) {
		if (vptr->tx.infos[j] == NULL)
			continue;
		for (i = 0; i < vptr->options.numtx; i++)
			velocity_free_td_ring_entry(vptr, j, i);

		kfree(vptr->tx.infos[j]);
		vptr->tx.infos[j] = NULL;
	}
}

static void velocity_free_rings(struct velocity_info *vptr)
{
	velocity_free_td_ring(vptr);
	velocity_free_rd_ring(vptr);
	velocity_free_dma_rings(vptr);
}

static void velocity_error(struct velocity_info *vptr, int status)
{

	if (status & ISR_TXSTLI) {
		struct mac_regs __iomem *regs = vptr->mac_regs;

		printk(KERN_ERR "TD structure error TDindex=%hx\n", readw(&regs->TDIdx[0]));
		BYTE_REG_BITS_ON(TXESR_TDSTR, &regs->TXESR);
		writew(TRDCSR_RUN, &regs->TDCSRClr);
		netif_stop_queue(vptr->dev);

	}

	if (status & ISR_SRCI) {
		struct mac_regs __iomem *regs = vptr->mac_regs;
		int linked;

		if (vptr->options.spd_dpx == SPD_DPX_AUTO) {
			vptr->mii_status = check_connection_type(regs);

			if (vptr->rev_id < REV_ID_VT3216_A0) {
				if (vptr->mii_status & VELOCITY_DUPLEX_FULL)
					BYTE_REG_BITS_ON(TCR_TB2BDIS, &regs->TCR);
				else
					BYTE_REG_BITS_OFF(TCR_TB2BDIS, &regs->TCR);
			}
			if (!(vptr->mii_status & VELOCITY_DUPLEX_FULL) && (vptr->mii_status & VELOCITY_SPEED_10))
				BYTE_REG_BITS_OFF(TESTCFG_HBDIS, &regs->TESTCFG);
			else
				BYTE_REG_BITS_ON(TESTCFG_HBDIS, &regs->TESTCFG);

			setup_queue_timers(vptr);
		}
		linked = readb(&regs->PHYSR0) & PHYSR0_LINKGD;

		if (linked) {
			vptr->mii_status &= ~VELOCITY_LINK_FAIL;
			netif_carrier_on(vptr->dev);
		} else {
			vptr->mii_status |= VELOCITY_LINK_FAIL;
			netif_carrier_off(vptr->dev);
		}

		velocity_print_link_status(vptr);
		enable_flow_control_ability(vptr);


		enable_mii_autopoll(regs);

		if (vptr->mii_status & VELOCITY_LINK_FAIL)
			netif_stop_queue(vptr->dev);
		else
			netif_wake_queue(vptr->dev);

	}
	if (status & ISR_MIBFI)
		velocity_update_hw_mibs(vptr);
	if (status & ISR_LSTEI)
		mac_rx_queue_wake(vptr->mac_regs);
}

static int velocity_tx_srv(struct velocity_info *vptr)
{
	struct tx_desc *td;
	int qnum;
	int full = 0;
	int idx;
	int works = 0;
	struct velocity_td_info *tdinfo;
	struct net_device_stats *stats = &vptr->dev->stats;

	for (qnum = 0; qnum < vptr->tx.numq; qnum++) {
		for (idx = vptr->tx.tail[qnum]; vptr->tx.used[qnum] > 0;
			idx = (idx + 1) % vptr->options.numtx) {

			td = &(vptr->tx.rings[qnum][idx]);
			tdinfo = &(vptr->tx.infos[qnum][idx]);

			if (td->tdesc0.len & OWNED_BY_NIC)
				break;

			if ((works++ > 15))
				break;

			if (td->tdesc0.TSR & TSR0_TERR) {
				stats->tx_errors++;
				stats->tx_dropped++;
				if (td->tdesc0.TSR & TSR0_CDH)
					stats->tx_heartbeat_errors++;
				if (td->tdesc0.TSR & TSR0_CRS)
					stats->tx_carrier_errors++;
				if (td->tdesc0.TSR & TSR0_ABT)
					stats->tx_aborted_errors++;
				if (td->tdesc0.TSR & TSR0_OWC)
					stats->tx_window_errors++;
			} else {
				stats->tx_packets++;
				stats->tx_bytes += tdinfo->skb->len;
			}
			velocity_free_tx_buf(vptr, tdinfo, td);
			vptr->tx.used[qnum]--;
		}
		vptr->tx.tail[qnum] = idx;

		if (AVAIL_TD(vptr, qnum) < 1)
			full = 1;
	}
	if (netif_queue_stopped(vptr->dev) && (full == 0) &&
	    (!(vptr->mii_status & VELOCITY_LINK_FAIL))) {
		netif_wake_queue(vptr->dev);
	}
	return works;
}

static inline void velocity_rx_csum(struct rx_desc *rd, struct sk_buff *skb)
{
	skb_checksum_none_assert(skb);

	if (rd->rdesc1.CSM & CSM_IPKT) {
		if (rd->rdesc1.CSM & CSM_IPOK) {
			if ((rd->rdesc1.CSM & CSM_TCPKT) ||
					(rd->rdesc1.CSM & CSM_UDPKT)) {
				if (!(rd->rdesc1.CSM & CSM_TUPOK))
					return;
			}
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		}
	}
}

static int velocity_rx_copy(struct sk_buff **rx_skb, int pkt_size,
			    struct velocity_info *vptr)
{
	int ret = -1;
	if (pkt_size < rx_copybreak) {
		struct sk_buff *new_skb;

		new_skb = netdev_alloc_skb_ip_align(vptr->dev, pkt_size);
		if (new_skb) {
			new_skb->ip_summed = rx_skb[0]->ip_summed;
			skb_copy_from_linear_data(*rx_skb, new_skb->data, pkt_size);
			*rx_skb = new_skb;
			ret = 0;
		}

	}
	return ret;
}

static inline void velocity_iph_realign(struct velocity_info *vptr,
					struct sk_buff *skb, int pkt_size)
{
	if (vptr->flags & VELOCITY_FLAGS_IP_ALIGN) {
		memmove(skb->data + 2, skb->data, pkt_size);
		skb_reserve(skb, 2);
	}
}

static int velocity_receive_frame(struct velocity_info *vptr, int idx)
{
	void (*pci_action)(struct pci_dev *, dma_addr_t, size_t, int);
	struct net_device_stats *stats = &vptr->dev->stats;
	struct velocity_rd_info *rd_info = &(vptr->rx.info[idx]);
	struct rx_desc *rd = &(vptr->rx.ring[idx]);
	int pkt_len = le16_to_cpu(rd->rdesc0.len) & 0x3fff;
	struct sk_buff *skb;

	if (rd->rdesc0.RSR & (RSR_STP | RSR_EDP)) {
		VELOCITY_PRT(MSG_LEVEL_VERBOSE, KERN_ERR " %s : the received frame span multple RDs.\n", vptr->dev->name);
		stats->rx_length_errors++;
		return -EINVAL;
	}

	if (rd->rdesc0.RSR & RSR_MAR)
		stats->multicast++;

	skb = rd_info->skb;

	pci_dma_sync_single_for_cpu(vptr->pdev, rd_info->skb_dma,
				    vptr->rx.buf_sz, PCI_DMA_FROMDEVICE);


	if (vptr->flags & VELOCITY_FLAGS_VAL_PKT_LEN) {
		if (rd->rdesc0.RSR & RSR_RL) {
			stats->rx_length_errors++;
			return -EINVAL;
		}
	}

	pci_action = pci_dma_sync_single_for_device;

	velocity_rx_csum(rd, skb);

	if (velocity_rx_copy(&skb, pkt_len, vptr) < 0) {
		velocity_iph_realign(vptr, skb, pkt_len);
		pci_action = pci_unmap_single;
		rd_info->skb = NULL;
	}

	pci_action(vptr->pdev, rd_info->skb_dma, vptr->rx.buf_sz,
		   PCI_DMA_FROMDEVICE);

	skb_put(skb, pkt_len - 4);
	skb->protocol = eth_type_trans(skb, vptr->dev);

	if (rd->rdesc0.RSR & RSR_DETAG) {
		u16 vid = swab16(le16_to_cpu(rd->rdesc1.PQTAG));

		__vlan_hwaccel_put_tag(skb, vid);
	}
	netif_rx(skb);

	stats->rx_bytes += pkt_len;
	stats->rx_packets++;

	return 0;
}

static int velocity_rx_srv(struct velocity_info *vptr, int budget_left)
{
	struct net_device_stats *stats = &vptr->dev->stats;
	int rd_curr = vptr->rx.curr;
	int works = 0;

	while (works < budget_left) {
		struct rx_desc *rd = vptr->rx.ring + rd_curr;

		if (!vptr->rx.info[rd_curr].skb)
			break;

		if (rd->rdesc0.len & OWNED_BY_NIC)
			break;

		rmb();

		if (rd->rdesc0.RSR & (RSR_RXOK | RSR_CE | RSR_RL)) {
			if (velocity_receive_frame(vptr, rd_curr) < 0)
				stats->rx_dropped++;
		} else {
			if (rd->rdesc0.RSR & RSR_CRC)
				stats->rx_crc_errors++;
			if (rd->rdesc0.RSR & RSR_FAE)
				stats->rx_frame_errors++;

			stats->rx_dropped++;
		}

		rd->size |= RX_INTEN;

		rd_curr++;
		if (rd_curr >= vptr->options.numrx)
			rd_curr = 0;
		works++;
	}

	vptr->rx.curr = rd_curr;

	if ((works > 0) && (velocity_rx_refill(vptr) > 0))
		velocity_give_many_rx_descs(vptr);

	VAR_USED(stats);
	return works;
}

static int velocity_poll(struct napi_struct *napi, int budget)
{
	struct velocity_info *vptr = container_of(napi,
			struct velocity_info, napi);
	unsigned int rx_done;
	unsigned long flags;

	spin_lock_irqsave(&vptr->lock, flags);
	rx_done = velocity_rx_srv(vptr, budget / 2);
	velocity_tx_srv(vptr);
	rx_done += velocity_rx_srv(vptr, budget - rx_done);
	velocity_tx_srv(vptr);

	
	if (rx_done < budget) {
		napi_complete(napi);
		mac_enable_int(vptr->mac_regs);
	}
	spin_unlock_irqrestore(&vptr->lock, flags);

	return rx_done;
}

static irqreturn_t velocity_intr(int irq, void *dev_instance)
{
	struct net_device *dev = dev_instance;
	struct velocity_info *vptr = netdev_priv(dev);
	u32 isr_status;

	spin_lock(&vptr->lock);
	isr_status = mac_read_isr(vptr->mac_regs);

	
	if (isr_status == 0) {
		spin_unlock(&vptr->lock);
		return IRQ_NONE;
	}

	
	mac_write_isr(vptr->mac_regs, isr_status);

	if (likely(napi_schedule_prep(&vptr->napi))) {
		mac_disable_int(vptr->mac_regs);
		__napi_schedule(&vptr->napi);
	}

	if (isr_status & (~(ISR_PRXI | ISR_PPRXI | ISR_PTXI | ISR_PPTXI)))
		velocity_error(vptr, isr_status);

	spin_unlock(&vptr->lock);

	return IRQ_HANDLED;
}

static int velocity_open(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int ret;

	ret = velocity_init_rings(vptr, dev->mtu);
	if (ret < 0)
		goto out;

	
	pci_set_power_state(vptr->pdev, PCI_D0);

	velocity_init_registers(vptr, VELOCITY_INIT_COLD);

	ret = request_irq(vptr->pdev->irq, velocity_intr, IRQF_SHARED,
			  dev->name, dev);
	if (ret < 0) {
		
		pci_set_power_state(vptr->pdev, PCI_D3hot);
		velocity_free_rings(vptr);
		goto out;
	}

	velocity_give_many_rx_descs(vptr);

	mac_enable_int(vptr->mac_regs);
	netif_start_queue(dev);
	napi_enable(&vptr->napi);
	vptr->flags |= VELOCITY_FLAGS_OPENED;
out:
	return ret;
}

static void velocity_shutdown(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	mac_disable_int(regs);
	writel(CR0_STOP, &regs->CR0Set);
	writew(0xFFFF, &regs->TDCSRClr);
	writeb(0xFF, &regs->RDCSRClr);
	safe_disable_mii_autopoll(regs);
	mac_clear_isr(regs);
}

static int velocity_change_mtu(struct net_device *dev, int new_mtu)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int ret = 0;

	if ((new_mtu < VELOCITY_MIN_MTU) || new_mtu > (VELOCITY_MAX_MTU)) {
		VELOCITY_PRT(MSG_LEVEL_ERR, KERN_NOTICE "%s: Invalid MTU.\n",
				vptr->dev->name);
		ret = -EINVAL;
		goto out_0;
	}

	if (!netif_running(dev)) {
		dev->mtu = new_mtu;
		goto out_0;
	}

	if (dev->mtu != new_mtu) {
		struct velocity_info *tmp_vptr;
		unsigned long flags;
		struct rx_info rx;
		struct tx_info tx;

		tmp_vptr = kzalloc(sizeof(*tmp_vptr), GFP_KERNEL);
		if (!tmp_vptr) {
			ret = -ENOMEM;
			goto out_0;
		}

		tmp_vptr->dev = dev;
		tmp_vptr->pdev = vptr->pdev;
		tmp_vptr->options = vptr->options;
		tmp_vptr->tx.numq = vptr->tx.numq;

		ret = velocity_init_rings(tmp_vptr, new_mtu);
		if (ret < 0)
			goto out_free_tmp_vptr_1;

		spin_lock_irqsave(&vptr->lock, flags);

		netif_stop_queue(dev);
		velocity_shutdown(vptr);

		rx = vptr->rx;
		tx = vptr->tx;

		vptr->rx = tmp_vptr->rx;
		vptr->tx = tmp_vptr->tx;

		tmp_vptr->rx = rx;
		tmp_vptr->tx = tx;

		dev->mtu = new_mtu;

		velocity_init_registers(vptr, VELOCITY_INIT_COLD);

		velocity_give_many_rx_descs(vptr);

		mac_enable_int(vptr->mac_regs);
		netif_start_queue(dev);

		spin_unlock_irqrestore(&vptr->lock, flags);

		velocity_free_rings(tmp_vptr);

out_free_tmp_vptr_1:
		kfree(tmp_vptr);
	}
out_0:
	return ret;
}

static int velocity_mii_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	unsigned long flags;
	struct mii_ioctl_data *miidata = if_mii(ifr);
	int err;

	switch (cmd) {
	case SIOCGMIIPHY:
		miidata->phy_id = readb(&regs->MIIADR) & 0x1f;
		break;
	case SIOCGMIIREG:
		if (velocity_mii_read(vptr->mac_regs, miidata->reg_num & 0x1f, &(miidata->val_out)) < 0)
			return -ETIMEDOUT;
		break;
	case SIOCSMIIREG:
		spin_lock_irqsave(&vptr->lock, flags);
		err = velocity_mii_write(vptr->mac_regs, miidata->reg_num & 0x1f, miidata->val_in);
		spin_unlock_irqrestore(&vptr->lock, flags);
		check_connection_type(vptr->mac_regs);
		if (err)
			return err;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int velocity_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int ret;


	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D0);

	switch (cmd) {
	case SIOCGMIIPHY:	
	case SIOCGMIIREG:	
	case SIOCSMIIREG:	
		ret = velocity_mii_ioctl(dev, rq, cmd);
		break;

	default:
		ret = -EOPNOTSUPP;
	}
	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D3hot);


	return ret;
}

static struct net_device_stats *velocity_get_stats(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);

	
	if (!netif_running(dev))
		return &dev->stats;

	spin_lock_irq(&vptr->lock);
	velocity_update_hw_mibs(vptr);
	spin_unlock_irq(&vptr->lock);

	dev->stats.rx_packets = vptr->mib_counter[HW_MIB_ifRxAllPkts];
	dev->stats.rx_errors = vptr->mib_counter[HW_MIB_ifRxErrorPkts];
	dev->stats.rx_length_errors = vptr->mib_counter[HW_MIB_ifInRangeLengthErrors];

	dev->stats.collisions = vptr->mib_counter[HW_MIB_ifTxEtherCollisions];
	
	dev->stats.rx_crc_errors = vptr->mib_counter[HW_MIB_ifRxPktCRCE];

	

	return &dev->stats;
}

static int velocity_close(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);

	napi_disable(&vptr->napi);
	netif_stop_queue(dev);
	velocity_shutdown(vptr);

	if (vptr->flags & VELOCITY_FLAGS_WOL_ENABLED)
		velocity_get_ip(vptr);
	if (dev->irq != 0)
		free_irq(dev->irq, dev);

	velocity_free_rings(vptr);

	vptr->flags &= (~VELOCITY_FLAGS_OPENED);
	return 0;
}

static netdev_tx_t velocity_xmit(struct sk_buff *skb,
				 struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int qnum = 0;
	struct tx_desc *td_ptr;
	struct velocity_td_info *tdinfo;
	unsigned long flags;
	int pktlen;
	int index, prev;
	int i = 0;

	if (skb_padto(skb, ETH_ZLEN))
		goto out;

	if (skb_shinfo(skb)->nr_frags > 6 && __skb_linearize(skb)) {
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	pktlen = skb_shinfo(skb)->nr_frags == 0 ?
			max_t(unsigned int, skb->len, ETH_ZLEN) :
				skb_headlen(skb);

	spin_lock_irqsave(&vptr->lock, flags);

	index = vptr->tx.curr[qnum];
	td_ptr = &(vptr->tx.rings[qnum][index]);
	tdinfo = &(vptr->tx.infos[qnum][index]);

	td_ptr->tdesc1.TCR = TCR0_TIC;
	td_ptr->td_buf[0].size &= ~TD_QUEUE;

	tdinfo->skb = skb;
	tdinfo->skb_dma[0] = pci_map_single(vptr->pdev, skb->data, pktlen, PCI_DMA_TODEVICE);
	td_ptr->tdesc0.len = cpu_to_le16(pktlen);
	td_ptr->td_buf[0].pa_low = cpu_to_le32(tdinfo->skb_dma[0]);
	td_ptr->td_buf[0].pa_high = 0;
	td_ptr->td_buf[0].size = cpu_to_le16(pktlen);

	
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		tdinfo->skb_dma[i + 1] = skb_frag_dma_map(&vptr->pdev->dev,
							  frag, 0,
							  skb_frag_size(frag),
							  DMA_TO_DEVICE);

		td_ptr->td_buf[i + 1].pa_low = cpu_to_le32(tdinfo->skb_dma[i + 1]);
		td_ptr->td_buf[i + 1].pa_high = 0;
		td_ptr->td_buf[i + 1].size = cpu_to_le16(skb_frag_size(frag));
	}
	tdinfo->nskb_dma = i + 1;

	td_ptr->tdesc1.cmd = TCPLS_NORMAL + (tdinfo->nskb_dma + 1) * 16;

	if (vlan_tx_tag_present(skb)) {
		td_ptr->tdesc1.vlan = cpu_to_le16(vlan_tx_tag_get(skb));
		td_ptr->tdesc1.TCR |= TCR0_VETAG;
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		const struct iphdr *ip = ip_hdr(skb);
		if (ip->protocol == IPPROTO_TCP)
			td_ptr->tdesc1.TCR |= TCR0_TCPCK;
		else if (ip->protocol == IPPROTO_UDP)
			td_ptr->tdesc1.TCR |= (TCR0_UDPCK);
		td_ptr->tdesc1.TCR |= TCR0_IPCK;
	}

	prev = index - 1;
	if (prev < 0)
		prev = vptr->options.numtx - 1;
	td_ptr->tdesc0.len |= OWNED_BY_NIC;
	vptr->tx.used[qnum]++;
	vptr->tx.curr[qnum] = (index + 1) % vptr->options.numtx;

	if (AVAIL_TD(vptr, qnum) < 1)
		netif_stop_queue(dev);

	td_ptr = &(vptr->tx.rings[qnum][prev]);
	td_ptr->td_buf[0].size |= TD_QUEUE;
	mac_tx_queue_wake(vptr->mac_regs, qnum);

	spin_unlock_irqrestore(&vptr->lock, flags);
out:
	return NETDEV_TX_OK;
}

static const struct net_device_ops velocity_netdev_ops = {
	.ndo_open		= velocity_open,
	.ndo_stop		= velocity_close,
	.ndo_start_xmit		= velocity_xmit,
	.ndo_get_stats		= velocity_get_stats,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_set_rx_mode	= velocity_set_multi,
	.ndo_change_mtu		= velocity_change_mtu,
	.ndo_do_ioctl		= velocity_ioctl,
	.ndo_vlan_rx_add_vid	= velocity_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= velocity_vlan_rx_kill_vid,
};

static void __devinit velocity_init_info(struct pci_dev *pdev,
					 struct velocity_info *vptr,
					 const struct velocity_info_tbl *info)
{
	memset(vptr, 0, sizeof(struct velocity_info));

	vptr->pdev = pdev;
	vptr->chip_id = info->chip_id;
	vptr->tx.numq = info->txqueue;
	vptr->multicast_limit = MCAM_SIZE;
	spin_lock_init(&vptr->lock);
}

static int __devinit velocity_get_pci_info(struct velocity_info *vptr, struct pci_dev *pdev)
{
	vptr->rev_id = pdev->revision;

	pci_set_master(pdev);

	vptr->ioaddr = pci_resource_start(pdev, 0);
	vptr->memaddr = pci_resource_start(pdev, 1);

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_IO)) {
		dev_err(&pdev->dev,
			   "region #0 is not an I/O resource, aborting.\n");
		return -EINVAL;
	}

	if ((pci_resource_flags(pdev, 1) & IORESOURCE_IO)) {
		dev_err(&pdev->dev,
			   "region #1 is an I/O resource, aborting.\n");
		return -EINVAL;
	}

	if (pci_resource_len(pdev, 1) < VELOCITY_IO_SIZE) {
		dev_err(&pdev->dev, "region #1 is too small.\n");
		return -EINVAL;
	}
	vptr->pdev = pdev;

	return 0;
}

static void __devinit velocity_print_info(struct velocity_info *vptr)
{
	struct net_device *dev = vptr->dev;

	printk(KERN_INFO "%s: %s\n", dev->name, get_chip_name(vptr->chip_id));
	printk(KERN_INFO "%s: Ethernet Address: %pM\n",
		dev->name, dev->dev_addr);
}

static u32 velocity_get_link(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	return BYTE_REG_BITS_IS_ON(PHYSR0_LINKGD, &regs->PHYSR0) ? 1 : 0;
}

static int __devinit velocity_found1(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	static int first = 1;
	struct net_device *dev;
	int i;
	const char *drv_string;
	const struct velocity_info_tbl *info = &chip_info_table[ent->driver_data];
	struct velocity_info *vptr;
	struct mac_regs __iomem *regs;
	int ret = -ENOMEM;

	if (velocity_nics >= MAX_UNITS) {
		dev_notice(&pdev->dev, "already found %d NICs.\n",
			   velocity_nics);
		return -ENODEV;
	}

	dev = alloc_etherdev(sizeof(struct velocity_info));
	if (!dev)
		goto out;

	

	SET_NETDEV_DEV(dev, &pdev->dev);
	vptr = netdev_priv(dev);


	if (first) {
		printk(KERN_INFO "%s Ver. %s\n",
			VELOCITY_FULL_DRV_NAM, VELOCITY_VERSION);
		printk(KERN_INFO "Copyright (c) 2002, 2003 VIA Networking Technologies, Inc.\n");
		printk(KERN_INFO "Copyright (c) 2004 Red Hat Inc.\n");
		first = 0;
	}

	velocity_init_info(pdev, vptr, info);

	vptr->dev = dev;

	ret = pci_enable_device(pdev);
	if (ret < 0)
		goto err_free_dev;

	dev->irq = pdev->irq;

	ret = velocity_get_pci_info(vptr, pdev);
	if (ret < 0) {
		
		goto err_disable;
	}

	ret = pci_request_regions(pdev, VELOCITY_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "No PCI resources.\n");
		goto err_disable;
	}

	regs = ioremap(vptr->memaddr, VELOCITY_IO_SIZE);
	if (regs == NULL) {
		ret = -EIO;
		goto err_release_res;
	}

	vptr->mac_regs = regs;

	mac_wol_reset(regs);

	dev->base_addr = vptr->ioaddr;

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = readb(&regs->PAR[i]);


	drv_string = dev_driver_string(&pdev->dev);

	velocity_get_options(&vptr->options, velocity_nics, drv_string);


	vptr->options.flags &= info->flags;


	vptr->flags = vptr->options.flags | (info->flags & 0xFF000000UL);

	vptr->wol_opts = vptr->options.wol_opts;
	vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;

	vptr->phy_id = MII_GET_PHY_ID(vptr->mac_regs);

	dev->irq = pdev->irq;
	dev->netdev_ops = &velocity_netdev_ops;
	dev->ethtool_ops = &velocity_ethtool_ops;
	netif_napi_add(dev, &vptr->napi, velocity_poll, VELOCITY_NAPI_WEIGHT);

	dev->hw_features = NETIF_F_IP_CSUM | NETIF_F_SG | NETIF_F_HW_VLAN_TX;
	dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_FILTER |
		NETIF_F_HW_VLAN_RX | NETIF_F_IP_CSUM;

	ret = register_netdev(dev);
	if (ret < 0)
		goto err_iounmap;

	if (!velocity_get_link(dev)) {
		netif_carrier_off(dev);
		vptr->mii_status |= VELOCITY_LINK_FAIL;
	}

	velocity_print_info(vptr);
	pci_set_drvdata(pdev, dev);

	

	pci_set_power_state(pdev, PCI_D3hot);
	velocity_nics++;
out:
	return ret;

err_iounmap:
	iounmap(regs);
err_release_res:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
err_free_dev:
	free_netdev(dev);
	goto out;
}

#ifdef CONFIG_PM
static u16 wol_calc_crc(int size, u8 *pattern, u8 *mask_pattern)
{
	u16 crc = 0xFFFF;
	u8 mask;
	int i, j;

	for (i = 0; i < size; i++) {
		mask = mask_pattern[i];

		
		if (mask == 0x00)
			continue;

		for (j = 0; j < 8; j++) {
			if ((mask & 0x01) == 0) {
				mask >>= 1;
				continue;
			}
			mask >>= 1;
			crc = crc_ccitt(crc, &(pattern[i * 8 + j]), 1);
		}
	}
	
	crc = ~crc;
	return bitrev32(crc) >> 16;
}

static int velocity_set_wol(struct velocity_info *vptr)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	enum speed_opt spd_dpx = vptr->options.spd_dpx;
	static u8 buf[256];
	int i;

	static u32 mask_pattern[2][4] = {
		{0x00203000, 0x000003C0, 0x00000000, 0x0000000}, 
		{0xfffff000, 0xffffffff, 0xffffffff, 0x000ffff}	 
	};

	writew(0xFFFF, &regs->WOLCRClr);
	writeb(WOLCFG_SAB | WOLCFG_SAM, &regs->WOLCFGSet);
	writew(WOLCR_MAGIC_EN, &regs->WOLCRSet);


	if (vptr->wol_opts & VELOCITY_WOL_UCAST)
		writew(WOLCR_UNICAST_EN, &regs->WOLCRSet);

	if (vptr->wol_opts & VELOCITY_WOL_ARP) {
		struct arp_packet *arp = (struct arp_packet *) buf;
		u16 crc;
		memset(buf, 0, sizeof(struct arp_packet) + 7);

		for (i = 0; i < 4; i++)
			writel(mask_pattern[0][i], &regs->ByteMask[0][i]);

		arp->type = htons(ETH_P_ARP);
		arp->ar_op = htons(1);

		memcpy(arp->ar_tip, vptr->ip_addr, 4);

		crc = wol_calc_crc((sizeof(struct arp_packet) + 7) / 8, buf,
				(u8 *) & mask_pattern[0][0]);

		writew(crc, &regs->PatternCRC[0]);
		writew(WOLCR_ARP_EN, &regs->WOLCRSet);
	}

	BYTE_REG_BITS_ON(PWCFG_WOLTYPE, &regs->PWCFGSet);
	BYTE_REG_BITS_ON(PWCFG_LEGACY_WOLEN, &regs->PWCFGSet);

	writew(0x0FFF, &regs->WOLSRClr);

	if (spd_dpx == SPD_DPX_1000_FULL)
		goto mac_done;

	if (spd_dpx != SPD_DPX_AUTO)
		goto advertise_done;

	if (vptr->mii_status & VELOCITY_AUTONEG_ENABLE) {
		if (PHYID_GET_PHY_ID(vptr->phy_id) == PHYID_CICADA_CS8201)
			MII_REG_BITS_ON(AUXCR_MDPPS, MII_NCONFIG, vptr->mac_regs);

		MII_REG_BITS_OFF(ADVERTISE_1000FULL | ADVERTISE_1000HALF, MII_CTRL1000, vptr->mac_regs);
	}

	if (vptr->mii_status & VELOCITY_SPEED_1000)
		MII_REG_BITS_ON(BMCR_ANRESTART, MII_BMCR, vptr->mac_regs);

advertise_done:
	BYTE_REG_BITS_ON(CHIPGCR_FCMODE, &regs->CHIPGCR);

	{
		u8 GCR;
		GCR = readb(&regs->CHIPGCR);
		GCR = (GCR & ~CHIPGCR_FCGMII) | CHIPGCR_FCFDX;
		writeb(GCR, &regs->CHIPGCR);
	}

mac_done:
	BYTE_REG_BITS_OFF(ISR_PWEI, &regs->ISR);
	
	BYTE_REG_BITS_ON(STICKHW_SWPTAG, &regs->STICKHW);
	
	BYTE_REG_BITS_ON((STICKHW_DS1 | STICKHW_DS0), &regs->STICKHW);

	return 0;
}

static void velocity_save_context(struct velocity_info *vptr, struct velocity_context *context)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u16 i;
	u8 __iomem *ptr = (u8 __iomem *)regs;

	for (i = MAC_REG_PAR; i < MAC_REG_CR0_CLR; i += 4)
		*((u32 *) (context->mac_reg + i)) = readl(ptr + i);

	for (i = MAC_REG_MAR; i < MAC_REG_TDCSR_CLR; i += 4)
		*((u32 *) (context->mac_reg + i)) = readl(ptr + i);

	for (i = MAC_REG_RDBASE_LO; i < MAC_REG_FIFO_TEST0; i += 4)
		*((u32 *) (context->mac_reg + i)) = readl(ptr + i);

}

static int velocity_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct velocity_info *vptr = netdev_priv(dev);
	unsigned long flags;

	if (!netif_running(vptr->dev))
		return 0;

	netif_device_detach(vptr->dev);

	spin_lock_irqsave(&vptr->lock, flags);
	pci_save_state(pdev);

	if (vptr->flags & VELOCITY_FLAGS_WOL_ENABLED) {
		velocity_get_ip(vptr);
		velocity_save_context(vptr, &vptr->context);
		velocity_shutdown(vptr);
		velocity_set_wol(vptr);
		pci_enable_wake(pdev, PCI_D3hot, 1);
		pci_set_power_state(pdev, PCI_D3hot);
	} else {
		velocity_save_context(vptr, &vptr->context);
		velocity_shutdown(vptr);
		pci_disable_device(pdev);
		pci_set_power_state(pdev, pci_choose_state(pdev, state));
	}

	spin_unlock_irqrestore(&vptr->lock, flags);
	return 0;
}

static void velocity_restore_context(struct velocity_info *vptr, struct velocity_context *context)
{
	struct mac_regs __iomem *regs = vptr->mac_regs;
	int i;
	u8 __iomem *ptr = (u8 __iomem *)regs;

	for (i = MAC_REG_PAR; i < MAC_REG_CR0_SET; i += 4)
		writel(*((u32 *) (context->mac_reg + i)), ptr + i);

	
	for (i = MAC_REG_CR1_SET; i < MAC_REG_CR0_CLR; i++) {
		
		writeb(~(*((u8 *) (context->mac_reg + i))), ptr + i + 4);
		
		writeb(*((u8 *) (context->mac_reg + i)), ptr + i);
	}

	for (i = MAC_REG_MAR; i < MAC_REG_IMR; i += 4)
		writel(*((u32 *) (context->mac_reg + i)), ptr + i);

	for (i = MAC_REG_RDBASE_LO; i < MAC_REG_FIFO_TEST0; i += 4)
		writel(*((u32 *) (context->mac_reg + i)), ptr + i);

	for (i = MAC_REG_TDCSR_SET; i <= MAC_REG_RDCSR_SET; i++)
		writeb(*((u8 *) (context->mac_reg + i)), ptr + i);
}

static int velocity_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct velocity_info *vptr = netdev_priv(dev);
	unsigned long flags;
	int i;

	if (!netif_running(vptr->dev))
		return 0;

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, 0, 0);
	pci_restore_state(pdev);

	mac_wol_reset(vptr->mac_regs);

	spin_lock_irqsave(&vptr->lock, flags);
	velocity_restore_context(vptr, &vptr->context);
	velocity_init_registers(vptr, VELOCITY_INIT_WOL);
	mac_disable_int(vptr->mac_regs);

	velocity_tx_srv(vptr);

	for (i = 0; i < vptr->tx.numq; i++) {
		if (vptr->tx.used[i])
			mac_tx_queue_wake(vptr->mac_regs, i);
	}

	mac_enable_int(vptr->mac_regs);
	spin_unlock_irqrestore(&vptr->lock, flags);
	netif_device_attach(vptr->dev);

	return 0;
}
#endif

static struct pci_driver velocity_driver = {
	.name		= VELOCITY_NAME,
	.id_table	= velocity_id_table,
	.probe		= velocity_found1,
	.remove		= __devexit_p(velocity_remove1),
#ifdef CONFIG_PM
	.suspend	= velocity_suspend,
	.resume		= velocity_resume,
#endif
};


static int velocity_ethtool_up(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D0);
	return 0;
}

static void velocity_ethtool_down(struct net_device *dev)
{
	struct velocity_info *vptr = netdev_priv(dev);
	if (!netif_running(dev))
		pci_set_power_state(vptr->pdev, PCI_D3hot);
}

static int velocity_get_settings(struct net_device *dev,
				 struct ethtool_cmd *cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	struct mac_regs __iomem *regs = vptr->mac_regs;
	u32 status;
	status = check_connection_type(vptr->mac_regs);

	cmd->supported = SUPPORTED_TP |
			SUPPORTED_Autoneg |
			SUPPORTED_10baseT_Half |
			SUPPORTED_10baseT_Full |
			SUPPORTED_100baseT_Half |
			SUPPORTED_100baseT_Full |
			SUPPORTED_1000baseT_Half |
			SUPPORTED_1000baseT_Full;

	cmd->advertising = ADVERTISED_TP | ADVERTISED_Autoneg;
	if (vptr->options.spd_dpx == SPD_DPX_AUTO) {
		cmd->advertising |=
			ADVERTISED_10baseT_Half |
			ADVERTISED_10baseT_Full |
			ADVERTISED_100baseT_Half |
			ADVERTISED_100baseT_Full |
			ADVERTISED_1000baseT_Half |
			ADVERTISED_1000baseT_Full;
	} else {
		switch (vptr->options.spd_dpx) {
		case SPD_DPX_1000_FULL:
			cmd->advertising |= ADVERTISED_1000baseT_Full;
			break;
		case SPD_DPX_100_HALF:
			cmd->advertising |= ADVERTISED_100baseT_Half;
			break;
		case SPD_DPX_100_FULL:
			cmd->advertising |= ADVERTISED_100baseT_Full;
			break;
		case SPD_DPX_10_HALF:
			cmd->advertising |= ADVERTISED_10baseT_Half;
			break;
		case SPD_DPX_10_FULL:
			cmd->advertising |= ADVERTISED_10baseT_Full;
			break;
		default:
			break;
		}
	}

	if (status & VELOCITY_SPEED_1000)
		ethtool_cmd_speed_set(cmd, SPEED_1000);
	else if (status & VELOCITY_SPEED_100)
		ethtool_cmd_speed_set(cmd, SPEED_100);
	else
		ethtool_cmd_speed_set(cmd, SPEED_10);

	cmd->autoneg = (status & VELOCITY_AUTONEG_ENABLE) ? AUTONEG_ENABLE : AUTONEG_DISABLE;
	cmd->port = PORT_TP;
	cmd->transceiver = XCVR_INTERNAL;
	cmd->phy_address = readb(&regs->MIIADR) & 0x1F;

	if (status & VELOCITY_DUPLEX_FULL)
		cmd->duplex = DUPLEX_FULL;
	else
		cmd->duplex = DUPLEX_HALF;

	return 0;
}

static int velocity_set_settings(struct net_device *dev,
				 struct ethtool_cmd *cmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	u32 speed = ethtool_cmd_speed(cmd);
	u32 curr_status;
	u32 new_status = 0;
	int ret = 0;

	curr_status = check_connection_type(vptr->mac_regs);
	curr_status &= (~VELOCITY_LINK_FAIL);

	new_status |= ((cmd->autoneg) ? VELOCITY_AUTONEG_ENABLE : 0);
	new_status |= ((speed == SPEED_1000) ? VELOCITY_SPEED_1000 : 0);
	new_status |= ((speed == SPEED_100) ? VELOCITY_SPEED_100 : 0);
	new_status |= ((speed == SPEED_10) ? VELOCITY_SPEED_10 : 0);
	new_status |= ((cmd->duplex == DUPLEX_FULL) ? VELOCITY_DUPLEX_FULL : 0);

	if ((new_status & VELOCITY_AUTONEG_ENABLE) &&
	    (new_status != (curr_status | VELOCITY_AUTONEG_ENABLE))) {
		ret = -EINVAL;
	} else {
		enum speed_opt spd_dpx;

		if (new_status & VELOCITY_AUTONEG_ENABLE)
			spd_dpx = SPD_DPX_AUTO;
		else if ((new_status & VELOCITY_SPEED_1000) &&
			 (new_status & VELOCITY_DUPLEX_FULL)) {
			spd_dpx = SPD_DPX_1000_FULL;
		} else if (new_status & VELOCITY_SPEED_100)
			spd_dpx = (new_status & VELOCITY_DUPLEX_FULL) ?
				SPD_DPX_100_FULL : SPD_DPX_100_HALF;
		else if (new_status & VELOCITY_SPEED_10)
			spd_dpx = (new_status & VELOCITY_DUPLEX_FULL) ?
				SPD_DPX_10_FULL : SPD_DPX_10_HALF;
		else
			return -EOPNOTSUPP;

		vptr->options.spd_dpx = spd_dpx;

		velocity_set_media_mode(vptr, new_status);
	}

	return ret;
}

static void velocity_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct velocity_info *vptr = netdev_priv(dev);
	strlcpy(info->driver, VELOCITY_NAME, sizeof(info->driver));
	strlcpy(info->version, VELOCITY_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(vptr->pdev), sizeof(info->bus_info));
}

static void velocity_ethtool_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct velocity_info *vptr = netdev_priv(dev);
	wol->supported = WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_ARP;
	wol->wolopts |= WAKE_MAGIC;
	if (vptr->wol_opts & VELOCITY_WOL_UCAST)
		wol->wolopts |= WAKE_UCAST;
	if (vptr->wol_opts & VELOCITY_WOL_ARP)
		wol->wolopts |= WAKE_ARP;
	memcpy(&wol->sopass, vptr->wol_passwd, 6);
}

static int velocity_ethtool_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct velocity_info *vptr = netdev_priv(dev);

	if (!(wol->wolopts & (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_ARP)))
		return -EFAULT;
	vptr->wol_opts = VELOCITY_WOL_MAGIC;


	if (wol->wolopts & WAKE_MAGIC) {
		vptr->wol_opts |= VELOCITY_WOL_MAGIC;
		vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;
	}
	if (wol->wolopts & WAKE_UCAST) {
		vptr->wol_opts |= VELOCITY_WOL_UCAST;
		vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;
	}
	if (wol->wolopts & WAKE_ARP) {
		vptr->wol_opts |= VELOCITY_WOL_ARP;
		vptr->flags |= VELOCITY_FLAGS_WOL_ENABLED;
	}
	memcpy(vptr->wol_passwd, wol->sopass, 6);
	return 0;
}

static u32 velocity_get_msglevel(struct net_device *dev)
{
	return msglevel;
}

static void velocity_set_msglevel(struct net_device *dev, u32 value)
{
	 msglevel = value;
}

static int get_pending_timer_val(int val)
{
	int mult_bits = val >> 6;
	int mult = 1;

	switch (mult_bits)
	{
	case 1:
		mult = 4; break;
	case 2:
		mult = 16; break;
	case 3:
		mult = 64; break;
	case 0:
	default:
		break;
	}

	return (val & 0x3f) * mult;
}

static void set_pending_timer_val(int *val, u32 us)
{
	u8 mult = 0;
	u8 shift = 0;

	if (us >= 0x3f) {
		mult = 1; 
		shift = 2;
	}
	if (us >= 0x3f * 4) {
		mult = 2; 
		shift = 4;
	}
	if (us >= 0x3f * 16) {
		mult = 3; 
		shift = 6;
	}

	*val = (mult << 6) | ((us >> shift) & 0x3f);
}


static int velocity_get_coalesce(struct net_device *dev,
		struct ethtool_coalesce *ecmd)
{
	struct velocity_info *vptr = netdev_priv(dev);

	ecmd->tx_max_coalesced_frames = vptr->options.tx_intsup;
	ecmd->rx_max_coalesced_frames = vptr->options.rx_intsup;

	ecmd->rx_coalesce_usecs = get_pending_timer_val(vptr->options.rxqueue_timer);
	ecmd->tx_coalesce_usecs = get_pending_timer_val(vptr->options.txqueue_timer);

	return 0;
}

static int velocity_set_coalesce(struct net_device *dev,
		struct ethtool_coalesce *ecmd)
{
	struct velocity_info *vptr = netdev_priv(dev);
	int max_us = 0x3f * 64;
	unsigned long flags;

	
	if (ecmd->tx_coalesce_usecs > max_us)
		return -EINVAL;
	if (ecmd->rx_coalesce_usecs > max_us)
		return -EINVAL;

	if (ecmd->tx_max_coalesced_frames > 0xff)
		return -EINVAL;
	if (ecmd->rx_max_coalesced_frames > 0xff)
		return -EINVAL;

	vptr->options.rx_intsup = ecmd->rx_max_coalesced_frames;
	vptr->options.tx_intsup = ecmd->tx_max_coalesced_frames;

	set_pending_timer_val(&vptr->options.rxqueue_timer,
			ecmd->rx_coalesce_usecs);
	set_pending_timer_val(&vptr->options.txqueue_timer,
			ecmd->tx_coalesce_usecs);

	
	spin_lock_irqsave(&vptr->lock, flags);
	mac_disable_int(vptr->mac_regs);
	setup_adaptive_interrupts(vptr);
	setup_queue_timers(vptr);

	mac_write_int_mask(vptr->int_mask, vptr->mac_regs);
	mac_clear_isr(vptr->mac_regs);
	mac_enable_int(vptr->mac_regs);
	spin_unlock_irqrestore(&vptr->lock, flags);

	return 0;
}

static const char velocity_gstrings[][ETH_GSTRING_LEN] = {
	"rx_all",
	"rx_ok",
	"tx_ok",
	"rx_error",
	"rx_runt_ok",
	"rx_runt_err",
	"rx_64",
	"tx_64",
	"rx_65_to_127",
	"tx_65_to_127",
	"rx_128_to_255",
	"tx_128_to_255",
	"rx_256_to_511",
	"tx_256_to_511",
	"rx_512_to_1023",
	"tx_512_to_1023",
	"rx_1024_to_1518",
	"tx_1024_to_1518",
	"tx_ether_collisions",
	"rx_crc_errors",
	"rx_jumbo",
	"tx_jumbo",
	"rx_mac_control_frames",
	"tx_mac_control_frames",
	"rx_frame_alignement_errors",
	"rx_long_ok",
	"rx_long_err",
	"tx_sqe_errors",
	"rx_no_buf",
	"rx_symbol_errors",
	"in_range_length_errors",
	"late_collisions"
};

static void velocity_get_strings(struct net_device *dev, u32 sset, u8 *data)
{
	switch (sset) {
	case ETH_SS_STATS:
		memcpy(data, *velocity_gstrings, sizeof(velocity_gstrings));
		break;
	}
}

static int velocity_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(velocity_gstrings);
	default:
		return -EOPNOTSUPP;
	}
}

static void velocity_get_ethtool_stats(struct net_device *dev,
				       struct ethtool_stats *stats, u64 *data)
{
	if (netif_running(dev)) {
		struct velocity_info *vptr = netdev_priv(dev);
		u32 *p = vptr->mib_counter;
		int i;

		spin_lock_irq(&vptr->lock);
		velocity_update_hw_mibs(vptr);
		spin_unlock_irq(&vptr->lock);

		for (i = 0; i < ARRAY_SIZE(velocity_gstrings); i++)
			*data++ = *p++;
	}
}

static const struct ethtool_ops velocity_ethtool_ops = {
	.get_settings		= velocity_get_settings,
	.set_settings		= velocity_set_settings,
	.get_drvinfo		= velocity_get_drvinfo,
	.get_wol		= velocity_ethtool_get_wol,
	.set_wol		= velocity_ethtool_set_wol,
	.get_msglevel		= velocity_get_msglevel,
	.set_msglevel		= velocity_set_msglevel,
	.get_link		= velocity_get_link,
	.get_strings		= velocity_get_strings,
	.get_sset_count		= velocity_get_sset_count,
	.get_ethtool_stats	= velocity_get_ethtool_stats,
	.get_coalesce		= velocity_get_coalesce,
	.set_coalesce		= velocity_set_coalesce,
	.begin			= velocity_ethtool_up,
	.complete		= velocity_ethtool_down
};

#if defined(CONFIG_PM) && defined(CONFIG_INET)
static int velocity_netdev_event(struct notifier_block *nb, unsigned long notification, void *ptr)
{
	struct in_ifaddr *ifa = ptr;
	struct net_device *dev = ifa->ifa_dev->dev;

	if (dev_net(dev) == &init_net &&
	    dev->netdev_ops == &velocity_netdev_ops)
		velocity_get_ip(netdev_priv(dev));

	return NOTIFY_DONE;
}

static struct notifier_block velocity_inetaddr_notifier = {
	.notifier_call	= velocity_netdev_event,
};

static void velocity_register_notifier(void)
{
	register_inetaddr_notifier(&velocity_inetaddr_notifier);
}

static void velocity_unregister_notifier(void)
{
	unregister_inetaddr_notifier(&velocity_inetaddr_notifier);
}

#else

#define velocity_register_notifier()	do {} while (0)
#define velocity_unregister_notifier()	do {} while (0)

#endif	

static int __init velocity_init_module(void)
{
	int ret;

	velocity_register_notifier();
	ret = pci_register_driver(&velocity_driver);
	if (ret < 0)
		velocity_unregister_notifier();
	return ret;
}

static void __exit velocity_cleanup_module(void)
{
	velocity_unregister_notifier();
	pci_unregister_driver(&velocity_driver);
}

module_init(velocity_init_module);
module_exit(velocity_cleanup_module);
