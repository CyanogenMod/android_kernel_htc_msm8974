/*****************************************************************************
 *                                                                           *
 * File: pm3393.c                                                            *
 * $Revision: 1.16 $                                                         *
 * $Date: 2005/05/14 00:59:32 $                                              *
 * Description:                                                              *
 *  PMC/SIERRA (pm3393) MAC-PHY functionality.                               *
 *  part of the Chelsio 10Gb Ethernet Driver.                                *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License, version 2, as       *
 * published by the Free Software Foundation.                                *
 *                                                                           *
 * You should have received a copy of the GNU General Public License along   *
 * with this program; if not, write to the Free Software Foundation, Inc.,   *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.                 *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED    *
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF      *
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 * http://www.chelsio.com                                                    *
 *                                                                           *
 * Copyright (c) 2003 - 2005 Chelsio Communications, Inc.                    *
 * All rights reserved.                                                      *
 *                                                                           *
 * Maintainers: maintainers@chelsio.com                                      *
 *                                                                           *
 * Authors: Dimitrios Michailidis   <dm@chelsio.com>                         *
 *          Tina Yang               <tainay@chelsio.com>                     *
 *          Felix Marti             <felix@chelsio.com>                      *
 *          Scott Bardone           <sbardone@chelsio.com>                   *
 *          Kurt Ottaway            <kottaway@chelsio.com>                   *
 *          Frank DiMambro          <frank@chelsio.com>                      *
 *                                                                           *
 * History:                                                                  *
 *                                                                           *
 ****************************************************************************/

#include "common.h"
#include "regs.h"
#include "gmac.h"
#include "elmer0.h"
#include "suni1x10gexp_regs.h"

#include <linux/crc32.h>
#include <linux/slab.h>

#define OFFSET(REG_ADDR)    ((REG_ADDR) << 2)

#define MAX_FRAME_SIZE  9600

#define IPG 12
#define TXXG_CONF1_VAL ((IPG << SUNI1x10GEXP_BITOFF_TXXG_IPGT) | \
	SUNI1x10GEXP_BITMSK_TXXG_32BIT_ALIGN | SUNI1x10GEXP_BITMSK_TXXG_CRCEN | \
	SUNI1x10GEXP_BITMSK_TXXG_PADEN)
#define RXXG_CONF1_VAL (SUNI1x10GEXP_BITMSK_RXXG_PUREP | 0x14 | \
	SUNI1x10GEXP_BITMSK_RXXG_FLCHK | SUNI1x10GEXP_BITMSK_RXXG_CRC_STRIP)

#define STATS_TICK_SECS (15 * 60)

enum {                     
	RxOctetsReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_1_LOW,
	RxUnicastFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_4_LOW,
	RxMulticastFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_5_LOW,
	RxBroadcastFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_6_LOW,
	RxPAUSEMACCtrlFramesReceived = SUNI1x10GEXP_REG_MSTAT_COUNTER_8_LOW,
	RxFrameCheckSequenceErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_10_LOW,
	RxFramesLostDueToInternalMACErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_11_LOW,
	RxSymbolErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_12_LOW,
	RxInRangeLengthErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_13_LOW,
	RxFramesTooLongErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_15_LOW,
	RxJabbers = SUNI1x10GEXP_REG_MSTAT_COUNTER_16_LOW,
	RxFragments = SUNI1x10GEXP_REG_MSTAT_COUNTER_17_LOW,
	RxUndersizedFrames =  SUNI1x10GEXP_REG_MSTAT_COUNTER_18_LOW,
	RxJumboFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_25_LOW,
	RxJumboOctetsReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_26_LOW,

	TxOctetsTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_33_LOW,
	TxFramesLostDueToInternalMACTransmissionError = SUNI1x10GEXP_REG_MSTAT_COUNTER_35_LOW,
	TxTransmitSystemError = SUNI1x10GEXP_REG_MSTAT_COUNTER_36_LOW,
	TxUnicastFramesTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_38_LOW,
	TxMulticastFramesTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_40_LOW,
	TxBroadcastFramesTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_42_LOW,
	TxPAUSEMACCtrlFramesTransmitted = SUNI1x10GEXP_REG_MSTAT_COUNTER_43_LOW,
	TxJumboFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_51_LOW,
	TxJumboOctetsReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_52_LOW
};

struct _cmac_instance {
	u8 enabled;
	u8 fc;
	u8 mac_addr[6];
};

static int pmread(struct cmac *cmac, u32 reg, u32 * data32)
{
	t1_tpi_read(cmac->adapter, OFFSET(reg), data32);
	return 0;
}

static int pmwrite(struct cmac *cmac, u32 reg, u32 data32)
{
	t1_tpi_write(cmac->adapter, OFFSET(reg), data32);
	return 0;
}

static int pm3393_reset(struct cmac *cmac)
{
	return 0;
}

static int pm3393_interrupt_enable(struct cmac *cmac)
{
	u32 pl_intr;

	pmwrite(cmac, SUNI1x10GEXP_REG_SERDES_3125_INTERRUPT_ENABLE, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_XRF_INTERRUPT_ENABLE, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_XRF_DIAG_INTERRUPT_ENABLE, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXOAM_INTERRUPT_ENABLE, 0xffff);

	
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_0, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_1, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_2, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_3, 0);

	pmwrite(cmac, SUNI1x10GEXP_REG_IFLX_FIFO_OVERFLOW_ENABLE, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_PL4ODP_INTERRUPT_MASK, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_XTEF_INTERRUPT_ENABLE, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXOAM_INTERRUPT_ENABLE, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_3, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_MASK, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_3, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_PL4IDU_INTERRUPT_MASK, 0xffff);
	pmwrite(cmac, SUNI1x10GEXP_REG_EFLX_FIFO_OVERFLOW_ERROR_ENABLE, 0xffff);

	
	pmwrite(cmac, SUNI1x10GEXP_REG_GLOBAL_INTERRUPT_ENABLE,
		0  );

	
	pl_intr = readl(cmac->adapter->regs + A_PL_ENABLE);
	pl_intr |= F_PL_INTR_EXT;
	writel(pl_intr, cmac->adapter->regs + A_PL_ENABLE);
	return 0;
}

static int pm3393_interrupt_disable(struct cmac *cmac)
{
	u32 elmer;

	
	pmwrite(cmac, SUNI1x10GEXP_REG_SERDES_3125_INTERRUPT_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_XRF_INTERRUPT_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_XRF_DIAG_INTERRUPT_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXOAM_INTERRUPT_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_0, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_1, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_2, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_3, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_IFLX_FIFO_OVERFLOW_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_PL4ODP_INTERRUPT_MASK, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_XTEF_INTERRUPT_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXOAM_INTERRUPT_ENABLE, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_3, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_MASK, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_3, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_PL4IDU_INTERRUPT_MASK, 0);
	pmwrite(cmac, SUNI1x10GEXP_REG_EFLX_FIFO_OVERFLOW_ERROR_ENABLE, 0);

	
	pmwrite(cmac, SUNI1x10GEXP_REG_GLOBAL_INTERRUPT_ENABLE, 0);

	
	t1_tpi_read(cmac->adapter, A_ELMER0_INT_ENABLE, &elmer);
	elmer &= ~ELMER0_GP_BIT1;
	t1_tpi_write(cmac->adapter, A_ELMER0_INT_ENABLE, elmer);

	

	return 0;
}

static int pm3393_interrupt_clear(struct cmac *cmac)
{
	u32 elmer;
	u32 pl_intr;
	u32 val32;

	pmread(cmac, SUNI1x10GEXP_REG_SERDES_3125_INTERRUPT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_XRF_INTERRUPT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_XRF_DIAG_INTERRUPT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_RXOAM_INTERRUPT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_PL4ODP_INTERRUPT, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_XTEF_INTERRUPT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_IFLX_FIFO_OVERFLOW_INTERRUPT, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_TXOAM_INTERRUPT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_RXXG_INTERRUPT, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_TXXG_INTERRUPT, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_PL4IDU_INTERRUPT, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_EFLX_FIFO_OVERFLOW_ERROR_INDICATION,
	       &val32);
	pmread(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_STATUS, &val32);
	pmread(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_CHANGE, &val32);

	pmread(cmac, SUNI1x10GEXP_REG_MASTER_INTERRUPT_STATUS, &val32);

	t1_tpi_read(cmac->adapter, A_ELMER0_INT_CAUSE, &elmer);
	elmer |= ELMER0_GP_BIT1;
	t1_tpi_write(cmac->adapter, A_ELMER0_INT_CAUSE, elmer);

	pl_intr = readl(cmac->adapter->regs + A_PL_CAUSE);
	pl_intr |= F_PL_INTR_EXT;
	writel(pl_intr, cmac->adapter->regs + A_PL_CAUSE);

	return 0;
}

static int pm3393_interrupt_handler(struct cmac *cmac)
{
	u32 master_intr_status;

	
	pmread(cmac, SUNI1x10GEXP_REG_MASTER_INTERRUPT_STATUS,
	       &master_intr_status);
	if (netif_msg_intr(cmac->adapter))
		dev_dbg(&cmac->adapter->pdev->dev, "PM3393 intr cause 0x%x\n",
			master_intr_status);

	
	pm3393_interrupt_clear(cmac);

	return 0;
}

static int pm3393_enable(struct cmac *cmac, int which)
{
	if (which & MAC_DIRECTION_RX)
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_1,
			(RXXG_CONF1_VAL | SUNI1x10GEXP_BITMSK_RXXG_RXEN));

	if (which & MAC_DIRECTION_TX) {
		u32 val = TXXG_CONF1_VAL | SUNI1x10GEXP_BITMSK_TXXG_TXEN0;

		if (cmac->instance->fc & PAUSE_RX)
			val |= SUNI1x10GEXP_BITMSK_TXXG_FCRX;
		if (cmac->instance->fc & PAUSE_TX)
			val |= SUNI1x10GEXP_BITMSK_TXXG_FCTX;
		pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_1, val);
	}

	cmac->instance->enabled |= which;
	return 0;
}

static int pm3393_enable_port(struct cmac *cmac, int which)
{
	
	pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_CONTROL,
		SUNI1x10GEXP_BITMSK_MSTAT_CLEAR);
	udelay(2);
	memset(&cmac->stats, 0, sizeof(struct cmac_statistics));

	pm3393_enable(cmac, which);

	t1_link_changed(cmac->adapter, 0);
	return 0;
}

static int pm3393_disable(struct cmac *cmac, int which)
{
	if (which & MAC_DIRECTION_RX)
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_1, RXXG_CONF1_VAL);
	if (which & MAC_DIRECTION_TX)
		pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_1, TXXG_CONF1_VAL);

	udelay(20);

	cmac->instance->enabled &= ~which;
	return 0;
}

static int pm3393_loopback_enable(struct cmac *cmac)
{
	return 0;
}

static int pm3393_loopback_disable(struct cmac *cmac)
{
	return 0;
}

static int pm3393_set_mtu(struct cmac *cmac, int mtu)
{
	int enabled = cmac->instance->enabled;

	
	mtu += 14 + 4;
	if (mtu > MAX_FRAME_SIZE)
		return -EINVAL;

	
	if (enabled)
		pm3393_disable(cmac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);

	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MAX_FRAME_LENGTH, mtu);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_MAX_FRAME_SIZE, mtu);

	if (enabled)
		pm3393_enable(cmac, enabled);
	return 0;
}

static int pm3393_set_rx_mode(struct cmac *cmac, struct t1_rx_mode *rm)
{
	int enabled = cmac->instance->enabled & MAC_DIRECTION_RX;
	u32 rx_mode;

	
	if (enabled)
		pm3393_disable(cmac, MAC_DIRECTION_RX);

	pmread(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_2, &rx_mode);
	rx_mode &= ~(SUNI1x10GEXP_BITMSK_RXXG_PMODE |
		     SUNI1x10GEXP_BITMSK_RXXG_MHASH_EN);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_2,
		(u16)rx_mode);

	if (t1_rx_mode_promisc(rm)) {
		
		rx_mode |= SUNI1x10GEXP_BITMSK_RXXG_PMODE;
	}
	if (t1_rx_mode_allmulti(rm)) {
		
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_LOW, 0xffff);
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDLOW, 0xffff);
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDHIGH, 0xffff);
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_HIGH, 0xffff);
		rx_mode |= SUNI1x10GEXP_BITMSK_RXXG_MHASH_EN;
	} else if (t1_rx_mode_mc_cnt(rm)) {
		
		struct netdev_hw_addr *ha;
		int bit;
		u16 mc_filter[4] = { 0, };

		netdev_for_each_mc_addr(ha, t1_get_netdev(rm)) {
			
			bit = (ether_crc(ETH_ALEN, ha->addr) >> 23) & 0x3f;
			mc_filter[bit >> 4] |= 1 << (bit & 0xf);
		}
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_LOW, mc_filter[0]);
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDLOW, mc_filter[1]);
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDHIGH, mc_filter[2]);
		pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_HIGH, mc_filter[3]);
		rx_mode |= SUNI1x10GEXP_BITMSK_RXXG_MHASH_EN;
	}

	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_2, (u16)rx_mode);

	if (enabled)
		pm3393_enable(cmac, MAC_DIRECTION_RX);

	return 0;
}

static int pm3393_get_speed_duplex_fc(struct cmac *cmac, int *speed,
				      int *duplex, int *fc)
{
	if (speed)
		*speed = SPEED_10000;
	if (duplex)
		*duplex = DUPLEX_FULL;
	if (fc)
		*fc = cmac->instance->fc;
	return 0;
}

static int pm3393_set_speed_duplex_fc(struct cmac *cmac, int speed, int duplex,
				      int fc)
{
	if (speed >= 0 && speed != SPEED_10000)
		return -1;
	if (duplex >= 0 && duplex != DUPLEX_FULL)
		return -1;
	if (fc & ~(PAUSE_TX | PAUSE_RX))
		return -1;

	if (fc != cmac->instance->fc) {
		cmac->instance->fc = (u8) fc;
		if (cmac->instance->enabled & MAC_DIRECTION_TX)
			pm3393_enable(cmac, MAC_DIRECTION_TX);
	}
	return 0;
}

#define RMON_UPDATE(mac, name, stat_name) \
{ \
	t1_tpi_read((mac)->adapter, OFFSET(name), &val0);     \
	t1_tpi_read((mac)->adapter, OFFSET((name)+1), &val1); \
	t1_tpi_read((mac)->adapter, OFFSET((name)+2), &val2); \
	(mac)->stats.stat_name = (u64)(val0 & 0xffff) | \
				 ((u64)(val1 & 0xffff) << 16) | \
				 ((u64)(val2 & 0xff) << 32) | \
				 ((mac)->stats.stat_name & \
					0xffffff0000000000ULL); \
	if (ro & \
	    (1ULL << ((name - SUNI1x10GEXP_REG_MSTAT_COUNTER_0_LOW) >> 2))) \
		(mac)->stats.stat_name += 1ULL << 40; \
}

static const struct cmac_statistics *pm3393_update_statistics(struct cmac *mac,
							      int flag)
{
	u64	ro;
	u32	val0, val1, val2, val3;

	
	pmwrite(mac, SUNI1x10GEXP_REG_MSTAT_CONTROL,
		SUNI1x10GEXP_BITMSK_MSTAT_SNAP);

	
	pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_0, &val0);
	pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_1, &val1);
	pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_2, &val2);
	pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_3, &val3);
	ro = ((u64)val0 & 0xffff) | (((u64)val1 & 0xffff) << 16) |
		(((u64)val2 & 0xffff) << 32) | (((u64)val3 & 0xffff) << 48);

	
	RMON_UPDATE(mac, RxOctetsReceivedOK, RxOctetsOK);
	RMON_UPDATE(mac, RxUnicastFramesReceivedOK, RxUnicastFramesOK);
	RMON_UPDATE(mac, RxMulticastFramesReceivedOK, RxMulticastFramesOK);
	RMON_UPDATE(mac, RxBroadcastFramesReceivedOK, RxBroadcastFramesOK);
	RMON_UPDATE(mac, RxPAUSEMACCtrlFramesReceived, RxPauseFrames);
	RMON_UPDATE(mac, RxFrameCheckSequenceErrors, RxFCSErrors);
	RMON_UPDATE(mac, RxFramesLostDueToInternalMACErrors,
				RxInternalMACRcvError);
	RMON_UPDATE(mac, RxSymbolErrors, RxSymbolErrors);
	RMON_UPDATE(mac, RxInRangeLengthErrors, RxInRangeLengthErrors);
	RMON_UPDATE(mac, RxFramesTooLongErrors , RxFrameTooLongErrors);
	RMON_UPDATE(mac, RxJabbers, RxJabberErrors);
	RMON_UPDATE(mac, RxFragments, RxRuntErrors);
	RMON_UPDATE(mac, RxUndersizedFrames, RxRuntErrors);
	RMON_UPDATE(mac, RxJumboFramesReceivedOK, RxJumboFramesOK);
	RMON_UPDATE(mac, RxJumboOctetsReceivedOK, RxJumboOctetsOK);

	
	RMON_UPDATE(mac, TxOctetsTransmittedOK, TxOctetsOK);
	RMON_UPDATE(mac, TxFramesLostDueToInternalMACTransmissionError,
				TxInternalMACXmitError);
	RMON_UPDATE(mac, TxTransmitSystemError, TxFCSErrors);
	RMON_UPDATE(mac, TxUnicastFramesTransmittedOK, TxUnicastFramesOK);
	RMON_UPDATE(mac, TxMulticastFramesTransmittedOK, TxMulticastFramesOK);
	RMON_UPDATE(mac, TxBroadcastFramesTransmittedOK, TxBroadcastFramesOK);
	RMON_UPDATE(mac, TxPAUSEMACCtrlFramesTransmitted, TxPauseFrames);
	RMON_UPDATE(mac, TxJumboFramesReceivedOK, TxJumboFramesOK);
	RMON_UPDATE(mac, TxJumboOctetsReceivedOK, TxJumboOctetsOK);

	return &mac->stats;
}

static int pm3393_macaddress_get(struct cmac *cmac, u8 mac_addr[6])
{
	memcpy(mac_addr, cmac->instance->mac_addr, 6);
	return 0;
}

static int pm3393_macaddress_set(struct cmac *cmac, u8 ma[6])
{
	u32 val, lo, mid, hi, enabled = cmac->instance->enabled;


	
	memcpy(cmac->instance->mac_addr, ma, 6);

	lo  = ((u32) ma[1] << 8) | (u32) ma[0];
	mid = ((u32) ma[3] << 8) | (u32) ma[2];
	hi  = ((u32) ma[5] << 8) | (u32) ma[4];

	
	if (enabled)
		pm3393_disable(cmac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);

	
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_SA_15_0, lo);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_SA_31_16, mid);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_SA_47_32, hi);

	
	pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_SA_15_0, lo);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_SA_31_16, mid);
	pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_SA_47_32, hi);

	pmread(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_0, &val);
	val &= 0xff0f;
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_0, val);

	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_EXACT_MATCH_ADDR_1_LOW, lo);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_EXACT_MATCH_ADDR_1_MID, mid);
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_EXACT_MATCH_ADDR_1_HIGH, hi);

	val |= 0x0090;
	pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_0, val);

	if (enabled)
		pm3393_enable(cmac, enabled);
	return 0;
}

static void pm3393_destroy(struct cmac *cmac)
{
	kfree(cmac);
}

static struct cmac_ops pm3393_ops = {
	.destroy                 = pm3393_destroy,
	.reset                   = pm3393_reset,
	.interrupt_enable        = pm3393_interrupt_enable,
	.interrupt_disable       = pm3393_interrupt_disable,
	.interrupt_clear         = pm3393_interrupt_clear,
	.interrupt_handler       = pm3393_interrupt_handler,
	.enable                  = pm3393_enable_port,
	.disable                 = pm3393_disable,
	.loopback_enable         = pm3393_loopback_enable,
	.loopback_disable        = pm3393_loopback_disable,
	.set_mtu                 = pm3393_set_mtu,
	.set_rx_mode             = pm3393_set_rx_mode,
	.get_speed_duplex_fc     = pm3393_get_speed_duplex_fc,
	.set_speed_duplex_fc     = pm3393_set_speed_duplex_fc,
	.statistics_update       = pm3393_update_statistics,
	.macaddress_get          = pm3393_macaddress_get,
	.macaddress_set          = pm3393_macaddress_set
};

static struct cmac *pm3393_mac_create(adapter_t *adapter, int index)
{
	struct cmac *cmac;

	cmac = kzalloc(sizeof(*cmac) + sizeof(cmac_instance), GFP_KERNEL);
	if (!cmac)
		return NULL;

	cmac->ops = &pm3393_ops;
	cmac->instance = (cmac_instance *) (cmac + 1);
	cmac->adapter = adapter;
	cmac->instance->fc = PAUSE_TX | PAUSE_RX;

	t1_tpi_write(adapter, OFFSET(0x0001), 0x00008000);
	t1_tpi_write(adapter, OFFSET(0x0001), 0x00000000);
	t1_tpi_write(adapter, OFFSET(0x2308), 0x00009800);
	t1_tpi_write(adapter, OFFSET(0x2305), 0x00001001);   
	t1_tpi_write(adapter, OFFSET(0x2320), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2321), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2322), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2323), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2324), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2325), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2326), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2327), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2328), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x2329), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x232a), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x232b), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x232c), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x232d), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x232e), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x232f), 0x00008800);
	t1_tpi_write(adapter, OFFSET(0x230d), 0x00009c00);
	t1_tpi_write(adapter, OFFSET(0x2304), 0x00000202);	

	t1_tpi_write(adapter, OFFSET(0x3200), 0x00008080);	
	t1_tpi_write(adapter, OFFSET(0x3210), 0x00000000);	
	t1_tpi_write(adapter, OFFSET(0x3203), 0x00000000);	
	t1_tpi_write(adapter, OFFSET(0x3204), 0x00000040);	
	t1_tpi_write(adapter, OFFSET(0x3205), 0x000002cc);	
	t1_tpi_write(adapter, OFFSET(0x3206), 0x00000199);	
	t1_tpi_write(adapter, OFFSET(0x3207), 0x00000240);	
	t1_tpi_write(adapter, OFFSET(0x3202), 0x00000000);	
	t1_tpi_write(adapter, OFFSET(0x3210), 0x00000001);	
	t1_tpi_write(adapter, OFFSET(0x3208), 0x0000ffff);	
	t1_tpi_write(adapter, OFFSET(0x320a), 0x0000ffff);	
	t1_tpi_write(adapter, OFFSET(0x320c), 0x0000ffff);	
	t1_tpi_write(adapter, OFFSET(0x320e), 0x0000ffff);	

	t1_tpi_write(adapter, OFFSET(0x2200), 0x0000c000);	
	t1_tpi_write(adapter, OFFSET(0x2201), 0x00000000);	
	t1_tpi_write(adapter, OFFSET(0x220e), 0x00000000);	
	t1_tpi_write(adapter, OFFSET(0x220f), 0x00000100);	
	t1_tpi_write(adapter, OFFSET(0x2210), 0x00000c00);	
	t1_tpi_write(adapter, OFFSET(0x2211), 0x00000599);	
	t1_tpi_write(adapter, OFFSET(0x220d), 0x00000000);	
	t1_tpi_write(adapter, OFFSET(0x2201), 0x00000001);	
	t1_tpi_write(adapter, OFFSET(0x2203), 0x0000ffff);	
	t1_tpi_write(adapter, OFFSET(0x2205), 0x0000ffff);	
	t1_tpi_write(adapter, OFFSET(0x2209), 0x0000ffff);	

	t1_tpi_write(adapter, OFFSET(0x2241), 0xfffffffe);	
	t1_tpi_write(adapter, OFFSET(0x2242), 0x0000ffff);	
	t1_tpi_write(adapter, OFFSET(0x2243), 0x00000008);	
	t1_tpi_write(adapter, OFFSET(0x2244), 0x00000008);	
	t1_tpi_write(adapter, OFFSET(0x2245), 0x00000008);	
	t1_tpi_write(adapter, OFFSET(0x2240), 0x00000005);	

	t1_tpi_write(adapter, OFFSET(0x2280), 0x00002103);	
	t1_tpi_write(adapter, OFFSET(0x2284), 0x00000000);	

	t1_tpi_write(adapter, OFFSET(0x3280), 0x00000087);	
	t1_tpi_write(adapter, OFFSET(0x3282), 0x0000001f);	

	t1_tpi_write(adapter, OFFSET(0x3040), 0x0c32);	
	
	t1_tpi_write(adapter, OFFSET(0x304d), 0x8000);
	t1_tpi_write(adapter, OFFSET(0x2040), 0x059c);	
	t1_tpi_write(adapter, OFFSET(0x2049), 0x0001);	
	t1_tpi_write(adapter, OFFSET(0x2070), 0x0000);	

	t1_tpi_write(adapter, OFFSET(0x206e), 0x0000);	
	t1_tpi_write(adapter, OFFSET(0x204a), 0xffff);	
	t1_tpi_write(adapter, OFFSET(0x204b), 0xffff);	
	t1_tpi_write(adapter, OFFSET(0x204c), 0xffff);	
	t1_tpi_write(adapter, OFFSET(0x206e), 0x0009);	

	t1_tpi_write(adapter, OFFSET(0x0003), 0x0000);	
	t1_tpi_write(adapter, OFFSET(0x0100), 0x0ff0);	
	t1_tpi_write(adapter, OFFSET(0x0101), 0x0f0f);	

	return cmac;
}

static int pm3393_mac_reset(adapter_t * adapter)
{
	u32 val;
	u32 x;
	u32 is_pl4_reset_finished;
	u32 is_pl4_outof_lock;
	u32 is_xaui_mabc_pll_locked;
	u32 successful_reset;
	int i;


	successful_reset = 0;
	for (i = 0; i < 3 && !successful_reset; i++) {
		
		t1_tpi_read(adapter, A_ELMER0_GPO, &val);
		val &= ~1;
		t1_tpi_write(adapter, A_ELMER0_GPO, val);

		
		msleep(1);

		
		msleep(1);

		
		msleep(2  );

		
		val |= 1;
		t1_tpi_write(adapter, A_ELMER0_GPO, val);

		
		msleep(15  );

		
		msleep(1);

		

		
		t1_tpi_read(adapter, OFFSET(SUNI1x10GEXP_REG_DEVICE_STATUS), &val);
		is_pl4_reset_finished = (val & SUNI1x10GEXP_BITMSK_TOP_EXPIRED);


		
		x = (SUNI1x10GEXP_BITMSK_TOP_PL4_ID_DOOL
		       |
		     SUNI1x10GEXP_BITMSK_TOP_PL4_ID_ROOL |
		     SUNI1x10GEXP_BITMSK_TOP_PL4_IS_ROOL |
		     SUNI1x10GEXP_BITMSK_TOP_PL4_OUT_ROOL);
		is_pl4_outof_lock = (val & x);

		
		is_xaui_mabc_pll_locked =
		    (val & SUNI1x10GEXP_BITMSK_TOP_SXRA_EXPIRED);

		successful_reset = (is_pl4_reset_finished && !is_pl4_outof_lock
				    && is_xaui_mabc_pll_locked);

		if (netif_msg_hw(adapter))
			dev_dbg(&adapter->pdev->dev,
				"PM3393 HW reset %d: pl4_reset 0x%x, val 0x%x, "
				"is_pl4_outof_lock 0x%x, xaui_locked 0x%x\n",
				i, is_pl4_reset_finished, val,
				is_pl4_outof_lock, is_xaui_mabc_pll_locked);
	}
	return successful_reset ? 0 : 1;
}

const struct gmac t1_pm3393_ops = {
	.stats_update_period = STATS_TICK_SECS,
	.create              = pm3393_mac_create,
	.reset               = pm3393_mac_reset,
};
