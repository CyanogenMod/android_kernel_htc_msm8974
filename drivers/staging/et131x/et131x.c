/*
 * Agere Systems Inc.
 * 10/100/1000 Base-T Ethernet Driver for the ET1301 and ET131x series MACs
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *   http://www.agere.com
 *
 * Copyright (c) 2011 Mark Einon <mark.einon@gmail.com>
 *
 *------------------------------------------------------------------------------
 *
 * SOFTWARE LICENSE
 *
 * This software is provided subject to the following terms and conditions,
 * which you should read carefully before using the software.  Using this
 * software indicates your acceptance of these terms and conditions.  If you do
 * not agree with these terms and conditions, do not use the software.
 *
 * Copyright © 2005 Agere Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source or binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 * . Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following Disclaimer as comments in the code as
 *    well as in the documentation and/or other materials provided with the
 *    distribution.
 *
 * . Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following Disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * . Neither the name of Agere Systems Inc. nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Disclaimer
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
 * USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
 * RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include <linux/pci.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <linux/phy.h>

#include "et131x.h"

MODULE_AUTHOR("Victor Soriano <vjsoriano@agere.com>");
MODULE_AUTHOR("Mark Einon <mark.einon@gmail.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("10/100/1000 Base-T Ethernet Driver "
		   "for the ET1310 by Agere Systems");

#define MAX_NUM_REGISTER_POLLS          1000
#define MAX_NUM_WRITE_RETRIES           2

#define COUNTER_WRAP_16_BIT 0x10000
#define COUNTER_WRAP_12_BIT 0x1000

#define INTERNAL_MEM_SIZE       0x400	
#define INTERNAL_MEM_RX_OFFSET  0x1FF	

#define INT_MASK_DISABLE            0xffffffff

#define INT_MASK_ENABLE             0xfffebf17
#define INT_MASK_ENABLE_NO_FLOW     0xfffebfd7

#define NIC_MIN_PACKET_SIZE	60

#define NIC_MAX_MCAST_LIST	128

#define ET131X_PACKET_TYPE_DIRECTED		0x0001
#define ET131X_PACKET_TYPE_MULTICAST		0x0002
#define ET131X_PACKET_TYPE_BROADCAST		0x0004
#define ET131X_PACKET_TYPE_PROMISCUOUS		0x0008
#define ET131X_PACKET_TYPE_ALL_MULTICAST	0x0010

#define ET131X_TX_TIMEOUT	(1 * HZ)
#define NIC_SEND_HANG_THRESHOLD	0

#define fMP_DEST_MULTI			0x00000001
#define fMP_DEST_BROAD			0x00000002

#define fMP_ADAPTER_RECV_LOOKASIDE	0x00000004
#define fMP_ADAPTER_INTERRUPT_IN_USE	0x00000008

#define fMP_ADAPTER_LOWER_POWER		0x00200000

#define fMP_ADAPTER_NON_RECOVER_ERROR	0x00800000
#define fMP_ADAPTER_HARDWARE_ERROR	0x04000000

#define fMP_ADAPTER_FAIL_SEND_MASK	0x3ff00000

#define ET1310_PCI_MAC_ADDRESS		0xA4
#define ET1310_PCI_EEPROM_STATUS	0xB2
#define ET1310_PCI_ACK_NACK		0xC0
#define ET1310_PCI_REPLAY		0xC2
#define ET1310_PCI_L0L1LATENCY		0xCF

#define ET131X_PCI_DEVICE_ID_GIG	0xED00	
#define ET131X_PCI_DEVICE_ID_FAST	0xED01	

#define NANO_IN_A_MICRO	1000

#define PARM_RX_NUM_BUFS_DEF    4
#define PARM_RX_TIME_INT_DEF    10
#define PARM_RX_MEM_END_DEF     0x2bc
#define PARM_TX_TIME_INT_DEF    40
#define PARM_TX_NUM_BUFS_DEF    4
#define PARM_DMA_CACHE_DEF      0

#define USE_FBR0 1
#define FBR_CHUNKS 32
#define MAX_DESC_PER_RING_RX         1024

#ifdef USE_FBR0
#define RFD_LOW_WATER_MARK	40
#define NIC_DEFAULT_NUM_RFD	1024
#define NUM_FBRS		2
#else
#define RFD_LOW_WATER_MARK	20
#define NIC_DEFAULT_NUM_RFD	256
#define NUM_FBRS		1
#endif

#define NIC_MIN_NUM_RFD		64
#define NUM_PACKETS_HANDLED	256

#define ALCATEL_MULTICAST_PKT	0x01000000
#define ALCATEL_BROADCAST_PKT	0x02000000

struct fbr_desc {
	u32 addr_lo;
	u32 addr_hi;
	u32 word2;		
};


struct pkt_stat_desc {
	u32 word0;
	u32 word1;
};




struct rx_status_block {
	u32 word0;
	u32 word1;
};

struct fbr_lookup {
	void		*virt[MAX_DESC_PER_RING_RX];
	void		*buffer1[MAX_DESC_PER_RING_RX];
	void		*buffer2[MAX_DESC_PER_RING_RX];
	u32		 bus_high[MAX_DESC_PER_RING_RX];
	u32		 bus_low[MAX_DESC_PER_RING_RX];
	void		*ring_virtaddr;
	dma_addr_t	 ring_physaddr;
	void		*mem_virtaddrs[MAX_DESC_PER_RING_RX / FBR_CHUNKS];
	dma_addr_t	 mem_physaddrs[MAX_DESC_PER_RING_RX / FBR_CHUNKS];
	u64		 real_physaddr;
	u64		 offset;
	u32		 local_full;
	u32		 num_entries;
	u32		 buffsize;
};

struct rx_ring {
	struct fbr_lookup *fbr[NUM_FBRS];
	void *ps_ring_virtaddr;
	dma_addr_t ps_ring_physaddr;
	u32 local_psr_full;
	u32 psr_num_entries;

	struct rx_status_block *rx_status_block;
	dma_addr_t rx_status_bus;

	
	struct list_head recv_list;
	u32 num_ready_recv;

	u32 num_rfd;

	bool unfinished_receives;

	
	struct kmem_cache *recv_lookaside;
};


struct tx_desc {
	u32 addr_hi;
	u32 addr_lo;
	u32 len_vlan;	
	u32 flags;	
};


struct tcb {
	struct tcb *next;	
	u32 flags;		
	u32 count;		
	u32 stale;		
	struct sk_buff *skb;	
	u32 index;		
	u32 index_start;
};

struct tx_ring {
	
	struct tcb *tcb_ring;

	
	struct tcb *tcb_qhead;
	struct tcb *tcb_qtail;

	struct tcb *send_head;
	struct tcb *send_tail;
	int used;

	
	struct tx_desc *tx_desc_ring;
	dma_addr_t tx_desc_ring_pa;

	
	u32 send_idx;

	
	u32 *tx_status;
	dma_addr_t tx_status_pa;

	
	int since_irq;
};

#define NUM_DESC_PER_RING_TX         512    
#define NUM_TCB                      64

#define TX_ERROR_PERIOD             1000

#define LO_MARK_PERCENT_FOR_PSR     15
#define LO_MARK_PERCENT_FOR_RX      15

struct rfd {
	struct list_head list_node;
	struct sk_buff *skb;
	u32 len;	
	u16 bufferindex;
	u8 ringindex;
};

#define FLOW_BOTH	0
#define FLOW_TXONLY	1
#define FLOW_RXONLY	2
#define FLOW_NONE	3

struct ce_stats {
	u32		unicast_pkts_rcvd;
	atomic_t	unicast_pkts_xmtd;
	u32		multicast_pkts_rcvd;
	atomic_t	multicast_pkts_xmtd;
	u32		broadcast_pkts_rcvd;
	atomic_t	broadcast_pkts_xmtd;
	u32		rcvd_pkts_dropped;

	
	u32		tx_underflows;

	u32		tx_collisions;
	u32		tx_excessive_collisions;
	u32		tx_first_collisions;
	u32		tx_late_collisions;
	u32		tx_max_pkt_errs;
	u32		tx_deferred;

	
	u32		rx_overflows;

	u32		rx_length_errs;
	u32		rx_align_errs;
	u32		rx_crc_errs;
	u32		rx_code_violations;
	u32		rx_other_errs;

	u32		synchronous_iterations;
	u32		interrupt_status;
};

struct et131x_adapter {
	struct net_device *netdev;
	struct pci_dev *pdev;
	struct mii_bus *mii_bus;
	struct phy_device *phydev;
	struct work_struct task;

	
	u32 flags;

	
	int link;

	
	u8 rom_addr[ETH_ALEN];
	u8 addr[ETH_ALEN];
	bool has_eeprom;
	u8 eeprom_data[2];

	
	spinlock_t lock;

	spinlock_t tcb_send_qlock;
	spinlock_t tcb_ready_qlock;
	spinlock_t send_hw_lock;

	spinlock_t rcv_lock;
	spinlock_t rcv_pend_lock;
	spinlock_t fbr_lock;

	spinlock_t phy_lock;

	
	u32 packet_filter;

	
	u32 multicast_addr_count;
	u8 multicast_list[NIC_MAX_MCAST_LIST][ETH_ALEN];

	
	struct address_map __iomem *regs;

	
	u8 wanted_flow;		
	u32 registry_jumbo_packet;	

	
	u8 flowcontrol;		

	
	struct timer_list error_timer;

	u8 boot_coma;

	u16 pdown_speed;
	u8 pdown_duplex;

	
	struct tx_ring tx_ring;

	
	struct rx_ring rx_ring;

	
	struct ce_stats stats;

	struct net_device_stats net_stats;
};

static int eeprom_wait_ready(struct pci_dev *pdev, u32 *status)
{
	u32 reg;
	int i;


	for (i = 0; i < MAX_NUM_REGISTER_POLLS; i++) {
		
		if (pci_read_config_dword(pdev, LBCIF_DWORD1_GROUP, &reg))
			return -EIO;

		
		if ((reg & 0x3000) == 0x3000) {
			if (status)
				*status = reg;
			return reg & 0xFF;
		}
	}
	return -ETIMEDOUT;
}


static int eeprom_write(struct et131x_adapter *adapter, u32 addr, u8 data)
{
	struct pci_dev *pdev = adapter->pdev;
	int index = 0;
	int retries;
	int err = 0;
	int i2c_wack = 0;
	int writeok = 0;
	u32 status;
	u32 val = 0;


	err = eeprom_wait_ready(pdev, NULL);
	if (err)
		return err;

	if (pci_write_config_byte(pdev, LBCIF_CONTROL_REGISTER,
			LBCIF_CONTROL_LBCIF_ENABLE | LBCIF_CONTROL_I2C_WRITE))
		return -EIO;

	i2c_wack = 1;

	

	for (retries = 0; retries < MAX_NUM_WRITE_RETRIES; retries++) {
		
		if (pci_write_config_dword(pdev, LBCIF_ADDRESS_REGISTER, addr))
			break;
		if (pci_write_config_byte(pdev, LBCIF_DATA_REGISTER, data))
			break;
		err = eeprom_wait_ready(pdev, &status);
		if (err < 0)
			return 0;

		if ((status & LBCIF_STATUS_GENERAL_ERROR)
			&& adapter->pdev->revision == 0)
			break;

		if (status & LBCIF_STATUS_ACK_ERROR) {
			udelay(10);
			continue;
		}

		writeok = 1;
		break;
	}

	udelay(10);

	while (i2c_wack) {
		if (pci_write_config_byte(pdev, LBCIF_CONTROL_REGISTER,
			LBCIF_CONTROL_LBCIF_ENABLE))
			writeok = 0;

		do {
			pci_write_config_dword(pdev,
					       LBCIF_ADDRESS_REGISTER,
					       addr);
			do {
				pci_read_config_dword(pdev,
					LBCIF_DATA_REGISTER, &val);
			} while ((val & 0x00010000) == 0);
		} while (val & 0x00040000);

		if ((val & 0xFF00) != 0xC000 || index == 10000)
			break;
		index++;
	}
	return writeok ? 0 : -EIO;
}

static int eeprom_read(struct et131x_adapter *adapter, u32 addr, u8 *pdata)
{
	struct pci_dev *pdev = adapter->pdev;
	int err;
	u32 status;


	err = eeprom_wait_ready(pdev, NULL);
	if (err)
		return err;
	if (pci_write_config_byte(pdev, LBCIF_CONTROL_REGISTER,
				  LBCIF_CONTROL_LBCIF_ENABLE))
		return -EIO;
	if (pci_write_config_dword(pdev, LBCIF_ADDRESS_REGISTER, addr))
		return -EIO;
	err = eeprom_wait_ready(pdev, &status);
	if (err < 0)
		return err;
	*pdata = err;
	return (status & LBCIF_STATUS_ACK_ERROR) ? -EIO : 0;
}

static int et131x_init_eeprom(struct et131x_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	u8 eestatus;

	pci_read_config_byte(pdev, ET1310_PCI_EEPROM_STATUS,
				      &eestatus);

	if (pci_read_config_byte(pdev, ET1310_PCI_EEPROM_STATUS, &eestatus)) {
		dev_err(&pdev->dev,
		       "Could not read PCI config space for EEPROM Status\n");
		return -EIO;
	}

	if (eestatus & 0x4C) {
		int write_failed = 0;
		if (pdev->revision == 0x01) {
			int	i;
			static const u8 eedata[4] = { 0xFE, 0x13, 0x10, 0xFF };

			for (i = 0; i < 3; i++)
				if (eeprom_write(adapter, i, eedata[i]) < 0)
					write_failed = 1;
		}
		if (pdev->revision  != 0x01 || write_failed) {
			dev_err(&pdev->dev,
			    "Fatal EEPROM Status Error - 0x%04x\n", eestatus);

			adapter->has_eeprom = 0;
			return -EIO;
		}
	}
	adapter->has_eeprom = 1;

	eeprom_read(adapter, 0x70, &adapter->eeprom_data[0]);
	eeprom_read(adapter, 0x71, &adapter->eeprom_data[1]);

	if (adapter->eeprom_data[0] != 0xcd)
		
		adapter->eeprom_data[1] = 0x00;

	return 0;
}

static void et131x_rx_dma_enable(struct et131x_adapter *adapter)
{
	
	u32 csr =  0x2000;	

	if (adapter->rx_ring.fbr[0]->buffsize == 4096)
		csr |= 0x0800;
	else if (adapter->rx_ring.fbr[0]->buffsize == 8192)
		csr |= 0x1000;
	else if (adapter->rx_ring.fbr[0]->buffsize == 16384)
		csr |= 0x1800;
#ifdef USE_FBR0
	csr |= 0x0400;		
	if (adapter->rx_ring.fbr[1]->buffsize == 256)
		csr |= 0x0100;
	else if (adapter->rx_ring.fbr[1]->buffsize == 512)
		csr |= 0x0200;
	else if (adapter->rx_ring.fbr[1]->buffsize == 1024)
		csr |= 0x0300;
#endif
	writel(csr, &adapter->regs->rxdma.csr);

	csr = readl(&adapter->regs->rxdma.csr);
	if ((csr & 0x00020000) != 0) {
		udelay(5);
		csr = readl(&adapter->regs->rxdma.csr);
		if ((csr & 0x00020000) != 0) {
			dev_err(&adapter->pdev->dev,
			    "RX Dma failed to exit halt state.  CSR 0x%08x\n",
				csr);
		}
	}
}

static void et131x_rx_dma_disable(struct et131x_adapter *adapter)
{
	u32 csr;
	
	writel(0x00002001, &adapter->regs->rxdma.csr);
	csr = readl(&adapter->regs->rxdma.csr);
	if ((csr & 0x00020000) == 0) {	
		udelay(5);
		csr = readl(&adapter->regs->rxdma.csr);
		if ((csr & 0x00020000) == 0)
			dev_err(&adapter->pdev->dev,
			"RX Dma failed to enter halt state. CSR 0x%08x\n",
				csr);
	}
}

static void et131x_tx_dma_enable(struct et131x_adapter *adapter)
{
	writel(ET_TXDMA_SNGL_EPKT|(PARM_DMA_CACHE_DEF << ET_TXDMA_CACHE_SHIFT),
					&adapter->regs->txdma.csr);
}

static inline void add_10bit(u32 *v, int n)
{
	*v = INDEX10(*v + n) | (*v & ET_DMA10_WRAP);
}

static inline void add_12bit(u32 *v, int n)
{
	*v = INDEX12(*v + n) | (*v & ET_DMA12_WRAP);
}

static void et1310_config_mac_regs1(struct et131x_adapter *adapter)
{
	struct mac_regs __iomem *macregs = &adapter->regs->mac;
	u32 station1;
	u32 station2;
	u32 ipg;

	writel(0xC00F0000, &macregs->cfg1);

	
	ipg = 0x38005860;		
	ipg |= 0x50 << 8;		
	writel(ipg, &macregs->ipg);

	
	
	writel(0x00A1F037, &macregs->hfdp);

	
	writel(0, &macregs->if_ctrl);

	
	writel(0x07, &macregs->mii_mgmt_cfg);	

	station2 = (adapter->addr[1] << ET_MAC_STATION_ADDR2_OC2_SHIFT) |
		   (adapter->addr[0] << ET_MAC_STATION_ADDR2_OC1_SHIFT);
	station1 = (adapter->addr[5] << ET_MAC_STATION_ADDR1_OC6_SHIFT) |
		   (adapter->addr[4] << ET_MAC_STATION_ADDR1_OC5_SHIFT) |
		   (adapter->addr[3] << ET_MAC_STATION_ADDR1_OC4_SHIFT) |
		    adapter->addr[2];
	writel(station1, &macregs->station_addr_1);
	writel(station2, &macregs->station_addr_2);

	writel(adapter->registry_jumbo_packet + 4, &macregs->max_fm_len);

	
	writel(0, &macregs->cfg1);
}

static void et1310_config_mac_regs2(struct et131x_adapter *adapter)
{
	int32_t delay = 0;
	struct mac_regs __iomem *mac = &adapter->regs->mac;
	struct phy_device *phydev = adapter->phydev;
	u32 cfg1;
	u32 cfg2;
	u32 ifctrl;
	u32 ctl;

	ctl = readl(&adapter->regs->txmac.ctl);
	cfg1 = readl(&mac->cfg1);
	cfg2 = readl(&mac->cfg2);
	ifctrl = readl(&mac->if_ctrl);

	
	cfg2 &= ~0x300;
	if (phydev && phydev->speed == SPEED_1000) {
		cfg2 |= 0x200;
		
		ifctrl &= ~(1 << 24);
	} else {
		cfg2 |= 0x100;
		ifctrl |= (1 << 24);
	}

	
	cfg1 |= CFG1_RX_ENABLE | CFG1_TX_ENABLE | CFG1_TX_FLOW;
	
	cfg1 &= ~(CFG1_LOOPBACK | CFG1_RX_FLOW);
	if (adapter->flowcontrol == FLOW_RXONLY ||
				adapter->flowcontrol == FLOW_BOTH)
		cfg1 |= CFG1_RX_FLOW;
	writel(cfg1, &mac->cfg1);

	
	cfg2 |= 0x7016;
	cfg2 &= ~0x0021;

	
	if (phydev && phydev->duplex == DUPLEX_FULL)
		cfg2 |= 0x01;

	ifctrl &= ~(1 << 26);
	if (phydev && phydev->duplex == DUPLEX_HALF)
		ifctrl |= (1<<26);	

	writel(ifctrl, &mac->if_ctrl);
	writel(cfg2, &mac->cfg2);

	do {
		udelay(10);
		delay++;
		cfg1 = readl(&mac->cfg1);
	} while ((cfg1 & CFG1_WAIT) != CFG1_WAIT && delay < 100);

	if (delay == 100) {
		dev_warn(&adapter->pdev->dev,
		    "Syncd bits did not respond correctly cfg1 word 0x%08x\n",
			cfg1);
	}

	
	ctl |= 0x09;	
	writel(ctl, &adapter->regs->txmac.ctl);

	
	if (adapter->flags & fMP_ADAPTER_LOWER_POWER) {
		et131x_rx_dma_enable(adapter);
		et131x_tx_dma_enable(adapter);
	}
}

static int et1310_in_phy_coma(struct et131x_adapter *adapter)
{
	u32 pmcsr;

	pmcsr = readl(&adapter->regs->global.pm_csr);

	return ET_PM_PHY_SW_COMA & pmcsr ? 1 : 0;
}

static void et1310_setup_device_for_multicast(struct et131x_adapter *adapter)
{
	struct rxmac_regs __iomem *rxmac = &adapter->regs->rxmac;
	u32 hash1 = 0;
	u32 hash2 = 0;
	u32 hash3 = 0;
	u32 hash4 = 0;
	u32 pm_csr;

	if (adapter->packet_filter & ET131X_PACKET_TYPE_MULTICAST) {
		int i;

		
		for (i = 0; i < adapter->multicast_addr_count; i++) {
			u32 result;

			result = ether_crc(6, adapter->multicast_list[i]);

			result = (result & 0x3F800000) >> 23;

			if (result < 32) {
				hash1 |= (1 << result);
			} else if ((31 < result) && (result < 64)) {
				result -= 32;
				hash2 |= (1 << result);
			} else if ((63 < result) && (result < 96)) {
				result -= 64;
				hash3 |= (1 << result);
			} else {
				result -= 96;
				hash4 |= (1 << result);
			}
		}
	}

	
	pm_csr = readl(&adapter->regs->global.pm_csr);
	if (!et1310_in_phy_coma(adapter)) {
		writel(hash1, &rxmac->multi_hash1);
		writel(hash2, &rxmac->multi_hash2);
		writel(hash3, &rxmac->multi_hash3);
		writel(hash4, &rxmac->multi_hash4);
	}
}

static void et1310_setup_device_for_unicast(struct et131x_adapter *adapter)
{
	struct rxmac_regs __iomem *rxmac = &adapter->regs->rxmac;
	u32 uni_pf1;
	u32 uni_pf2;
	u32 uni_pf3;
	u32 pm_csr;

	uni_pf3 = (adapter->addr[0] << ET_UNI_PF_ADDR2_1_SHIFT) |
		  (adapter->addr[1] << ET_UNI_PF_ADDR2_2_SHIFT) |
		  (adapter->addr[0] << ET_UNI_PF_ADDR1_1_SHIFT) |
		   adapter->addr[1];

	uni_pf2 = (adapter->addr[2] << ET_UNI_PF_ADDR2_3_SHIFT) |
		  (adapter->addr[3] << ET_UNI_PF_ADDR2_4_SHIFT) |
		  (adapter->addr[4] << ET_UNI_PF_ADDR2_5_SHIFT) |
		   adapter->addr[5];

	uni_pf1 = (adapter->addr[2] << ET_UNI_PF_ADDR1_3_SHIFT) |
		  (adapter->addr[3] << ET_UNI_PF_ADDR1_4_SHIFT) |
		  (adapter->addr[4] << ET_UNI_PF_ADDR1_5_SHIFT) |
		   adapter->addr[5];

	pm_csr = readl(&adapter->regs->global.pm_csr);
	if (!et1310_in_phy_coma(adapter)) {
		writel(uni_pf1, &rxmac->uni_pf_addr1);
		writel(uni_pf2, &rxmac->uni_pf_addr2);
		writel(uni_pf3, &rxmac->uni_pf_addr3);
	}
}

static void et1310_config_rxmac_regs(struct et131x_adapter *adapter)
{
	struct rxmac_regs __iomem *rxmac = &adapter->regs->rxmac;
	struct phy_device *phydev = adapter->phydev;
	u32 sa_lo;
	u32 sa_hi = 0;
	u32 pf_ctrl = 0;

	
	writel(0x8, &rxmac->ctrl);

	
	writel(0, &rxmac->crc0);
	writel(0, &rxmac->crc12);
	writel(0, &rxmac->crc34);

	writel(0, &rxmac->mask0_word0);
	writel(0, &rxmac->mask0_word1);
	writel(0, &rxmac->mask0_word2);
	writel(0, &rxmac->mask0_word3);

	writel(0, &rxmac->mask1_word0);
	writel(0, &rxmac->mask1_word1);
	writel(0, &rxmac->mask1_word2);
	writel(0, &rxmac->mask1_word3);

	writel(0, &rxmac->mask2_word0);
	writel(0, &rxmac->mask2_word1);
	writel(0, &rxmac->mask2_word2);
	writel(0, &rxmac->mask2_word3);

	writel(0, &rxmac->mask3_word0);
	writel(0, &rxmac->mask3_word1);
	writel(0, &rxmac->mask3_word2);
	writel(0, &rxmac->mask3_word3);

	writel(0, &rxmac->mask4_word0);
	writel(0, &rxmac->mask4_word1);
	writel(0, &rxmac->mask4_word2);
	writel(0, &rxmac->mask4_word3);

	
	sa_lo = (adapter->addr[2] << ET_WOL_LO_SA3_SHIFT) |
		(adapter->addr[3] << ET_WOL_LO_SA4_SHIFT) |
		(adapter->addr[4] << ET_WOL_LO_SA5_SHIFT) |
		 adapter->addr[5];
	writel(sa_lo, &rxmac->sa_lo);

	sa_hi = (u32) (adapter->addr[0] << ET_WOL_HI_SA1_SHIFT) |
		       adapter->addr[1];
	writel(sa_hi, &rxmac->sa_hi);

	
	writel(0, &rxmac->pf_ctrl);

	
	if (adapter->packet_filter & ET131X_PACKET_TYPE_DIRECTED) {
		et1310_setup_device_for_unicast(adapter);
		pf_ctrl |= 4;	
	} else {
		writel(0, &rxmac->uni_pf_addr1);
		writel(0, &rxmac->uni_pf_addr2);
		writel(0, &rxmac->uni_pf_addr3);
	}

	
	if (!(adapter->packet_filter & ET131X_PACKET_TYPE_ALL_MULTICAST)) {
		pf_ctrl |= 2;	
		et1310_setup_device_for_multicast(adapter);
	}

	
	pf_ctrl |= (NIC_MIN_PACKET_SIZE + 4) << 16;
	pf_ctrl |= 8;	

	if (adapter->registry_jumbo_packet > 8192)
		writel(0x41, &rxmac->mcif_ctrl_max_seg);
	else
		writel(0, &rxmac->mcif_ctrl_max_seg);

	
	writel(0, &rxmac->mcif_water_mark);

	
	writel(0, &rxmac->mif_ctrl);

	
	writel(0, &rxmac->space_avail);

	if (phydev && phydev->speed == SPEED_100)
		writel(0x30038, &rxmac->mif_ctrl);
	else
		writel(0x30030, &rxmac->mif_ctrl);

	writel(pf_ctrl, &rxmac->pf_ctrl);
	writel(0x9, &rxmac->ctrl);
}

static void et1310_config_txmac_regs(struct et131x_adapter *adapter)
{
	struct txmac_regs __iomem *txmac = &adapter->regs->txmac;

	if (adapter->flowcontrol == FLOW_NONE)
		writel(0, &txmac->cf_param);
	else
		writel(0x40, &txmac->cf_param);
}

static void et1310_config_macstat_regs(struct et131x_adapter *adapter)
{
	struct macstat_regs __iomem *macstat =
		&adapter->regs->macstat;

	writel(0, &macstat->txrx_0_64_byte_frames);
	writel(0, &macstat->txrx_65_127_byte_frames);
	writel(0, &macstat->txrx_128_255_byte_frames);
	writel(0, &macstat->txrx_256_511_byte_frames);
	writel(0, &macstat->txrx_512_1023_byte_frames);
	writel(0, &macstat->txrx_1024_1518_byte_frames);
	writel(0, &macstat->txrx_1519_1522_gvln_frames);

	writel(0, &macstat->rx_bytes);
	writel(0, &macstat->rx_packets);
	writel(0, &macstat->rx_fcs_errs);
	writel(0, &macstat->rx_multicast_packets);
	writel(0, &macstat->rx_broadcast_packets);
	writel(0, &macstat->rx_control_frames);
	writel(0, &macstat->rx_pause_frames);
	writel(0, &macstat->rx_unknown_opcodes);
	writel(0, &macstat->rx_align_errs);
	writel(0, &macstat->rx_frame_len_errs);
	writel(0, &macstat->rx_code_errs);
	writel(0, &macstat->rx_carrier_sense_errs);
	writel(0, &macstat->rx_undersize_packets);
	writel(0, &macstat->rx_oversize_packets);
	writel(0, &macstat->rx_fragment_packets);
	writel(0, &macstat->rx_jabbers);
	writel(0, &macstat->rx_drops);

	writel(0, &macstat->tx_bytes);
	writel(0, &macstat->tx_packets);
	writel(0, &macstat->tx_multicast_packets);
	writel(0, &macstat->tx_broadcast_packets);
	writel(0, &macstat->tx_pause_frames);
	writel(0, &macstat->tx_deferred);
	writel(0, &macstat->tx_excessive_deferred);
	writel(0, &macstat->tx_single_collisions);
	writel(0, &macstat->tx_multiple_collisions);
	writel(0, &macstat->tx_late_collisions);
	writel(0, &macstat->tx_excessive_collisions);
	writel(0, &macstat->tx_total_collisions);
	writel(0, &macstat->tx_pause_honored_frames);
	writel(0, &macstat->tx_drops);
	writel(0, &macstat->tx_jabbers);
	writel(0, &macstat->tx_fcs_errs);
	writel(0, &macstat->tx_control_frames);
	writel(0, &macstat->tx_oversize_frames);
	writel(0, &macstat->tx_undersize_frames);
	writel(0, &macstat->tx_fragments);
	writel(0, &macstat->carry_reg1);
	writel(0, &macstat->carry_reg2);

	writel(0xFFFFBE32, &macstat->carry_reg1_mask);
	writel(0xFFFE7E8B, &macstat->carry_reg2_mask);
}

static int et131x_phy_mii_read(struct et131x_adapter *adapter, u8 addr,
	      u8 reg, u16 *value)
{
	struct mac_regs __iomem *mac = &adapter->regs->mac;
	int status = 0;
	u32 delay = 0;
	u32 mii_addr;
	u32 mii_cmd;
	u32 mii_indicator;

	mii_addr = readl(&mac->mii_mgmt_addr);
	mii_cmd = readl(&mac->mii_mgmt_cmd);

	
	writel(0, &mac->mii_mgmt_cmd);

	
	writel(MII_ADDR(addr, reg), &mac->mii_mgmt_addr);

	writel(0x1, &mac->mii_mgmt_cmd);

	do {
		udelay(50);
		delay++;
		mii_indicator = readl(&mac->mii_mgmt_indicator);
	} while ((mii_indicator & MGMT_WAIT) && delay < 50);

	
	if (delay == 50) {
		dev_warn(&adapter->pdev->dev,
			    "reg 0x%08x could not be read\n", reg);
		dev_warn(&adapter->pdev->dev, "status is  0x%08x\n",
			    mii_indicator);

		status = -EIO;
	}

	*value = readl(&mac->mii_mgmt_stat) & 0xFFFF;

	
	writel(0, &mac->mii_mgmt_cmd);

	writel(mii_addr, &mac->mii_mgmt_addr);
	writel(mii_cmd, &mac->mii_mgmt_cmd);

	return status;
}

static int et131x_mii_read(struct et131x_adapter *adapter, u8 reg, u16 *value)
{
	struct phy_device *phydev = adapter->phydev;

	if (!phydev)
		return -EIO;

	return et131x_phy_mii_read(adapter, phydev->addr, reg, value);
}

static int et131x_mii_write(struct et131x_adapter *adapter, u8 reg, u16 value)
{
	struct mac_regs __iomem *mac = &adapter->regs->mac;
	struct phy_device *phydev = adapter->phydev;
	int status = 0;
	u8 addr;
	u32 delay = 0;
	u32 mii_addr;
	u32 mii_cmd;
	u32 mii_indicator;

	if (!phydev)
		return -EIO;

	addr = phydev->addr;

	mii_addr = readl(&mac->mii_mgmt_addr);
	mii_cmd = readl(&mac->mii_mgmt_cmd);

	
	writel(0, &mac->mii_mgmt_cmd);

	
	writel(MII_ADDR(addr, reg), &mac->mii_mgmt_addr);

	
	writel(value, &mac->mii_mgmt_ctrl);

	do {
		udelay(50);
		delay++;
		mii_indicator = readl(&mac->mii_mgmt_indicator);
	} while ((mii_indicator & MGMT_BUSY) && delay < 100);

	
	if (delay == 100) {
		u16 tmp;

		dev_warn(&adapter->pdev->dev,
		    "reg 0x%08x could not be written", reg);
		dev_warn(&adapter->pdev->dev, "status is  0x%08x\n",
			    mii_indicator);
		dev_warn(&adapter->pdev->dev, "command is  0x%08x\n",
			    readl(&mac->mii_mgmt_cmd));

		et131x_mii_read(adapter, reg, &tmp);

		status = -EIO;
	}
	
	writel(0, &mac->mii_mgmt_cmd);

	writel(mii_addr, &mac->mii_mgmt_addr);
	writel(mii_cmd, &mac->mii_mgmt_cmd);

	return status;
}

static void et1310_phy_access_mii_bit(struct et131x_adapter *adapter,
				      u16 action, u16 regnum, u16 bitnum,
				      u8 *value)
{
	u16 reg;
	u16 mask = 0x0001 << bitnum;

	
	et131x_mii_read(adapter, regnum, &reg);

	switch (action) {
	case TRUEPHY_BIT_READ:
		*value = (reg & mask) >> bitnum;
		break;

	case TRUEPHY_BIT_SET:
		et131x_mii_write(adapter, regnum, reg | mask);
		break;

	case TRUEPHY_BIT_CLEAR:
		et131x_mii_write(adapter, regnum, reg & ~mask);
		break;

	default:
		break;
	}
}

static void et1310_config_flow_control(struct et131x_adapter *adapter)
{
	struct phy_device *phydev = adapter->phydev;

	if (phydev->duplex == DUPLEX_HALF) {
		adapter->flowcontrol = FLOW_NONE;
	} else {
		char remote_pause, remote_async_pause;

		et1310_phy_access_mii_bit(adapter,
				TRUEPHY_BIT_READ, 5, 10, &remote_pause);
		et1310_phy_access_mii_bit(adapter,
				TRUEPHY_BIT_READ, 5, 11,
				&remote_async_pause);

		if ((remote_pause == TRUEPHY_BIT_SET) &&
		    (remote_async_pause == TRUEPHY_BIT_SET)) {
			adapter->flowcontrol = adapter->wanted_flow;
		} else if ((remote_pause == TRUEPHY_BIT_SET) &&
			   (remote_async_pause == TRUEPHY_BIT_CLEAR)) {
			if (adapter->wanted_flow == FLOW_BOTH)
				adapter->flowcontrol = FLOW_BOTH;
			else
				adapter->flowcontrol = FLOW_NONE;
		} else if ((remote_pause == TRUEPHY_BIT_CLEAR) &&
			   (remote_async_pause == TRUEPHY_BIT_CLEAR)) {
			adapter->flowcontrol = FLOW_NONE;
		} else {
			if (adapter->wanted_flow == FLOW_BOTH)
				adapter->flowcontrol = FLOW_RXONLY;
			else
				adapter->flowcontrol = FLOW_NONE;
		}
	}
}

static void et1310_update_macstat_host_counters(struct et131x_adapter *adapter)
{
	struct ce_stats *stats = &adapter->stats;
	struct macstat_regs __iomem *macstat =
		&adapter->regs->macstat;

	stats->tx_collisions	       += readl(&macstat->tx_total_collisions);
	stats->tx_first_collisions     += readl(&macstat->tx_single_collisions);
	stats->tx_deferred	       += readl(&macstat->tx_deferred);
	stats->tx_excessive_collisions +=
				readl(&macstat->tx_multiple_collisions);
	stats->tx_late_collisions      += readl(&macstat->tx_late_collisions);
	stats->tx_underflows	       += readl(&macstat->tx_undersize_frames);
	stats->tx_max_pkt_errs	       += readl(&macstat->tx_oversize_frames);

	stats->rx_align_errs        += readl(&macstat->rx_align_errs);
	stats->rx_crc_errs          += readl(&macstat->rx_code_errs);
	stats->rcvd_pkts_dropped    += readl(&macstat->rx_drops);
	stats->rx_overflows         += readl(&macstat->rx_oversize_packets);
	stats->rx_code_violations   += readl(&macstat->rx_fcs_errs);
	stats->rx_length_errs       += readl(&macstat->rx_frame_len_errs);
	stats->rx_other_errs        += readl(&macstat->rx_fragment_packets);
}

static void et1310_handle_macstat_interrupt(struct et131x_adapter *adapter)
{
	u32 carry_reg1;
	u32 carry_reg2;

	carry_reg1 = readl(&adapter->regs->macstat.carry_reg1);
	carry_reg2 = readl(&adapter->regs->macstat.carry_reg2);

	writel(carry_reg1, &adapter->regs->macstat.carry_reg1);
	writel(carry_reg2, &adapter->regs->macstat.carry_reg2);

	if (carry_reg1 & (1 << 14))
		adapter->stats.rx_code_violations	+= COUNTER_WRAP_16_BIT;
	if (carry_reg1 & (1 << 8))
		adapter->stats.rx_align_errs	+= COUNTER_WRAP_12_BIT;
	if (carry_reg1 & (1 << 7))
		adapter->stats.rx_length_errs	+= COUNTER_WRAP_16_BIT;
	if (carry_reg1 & (1 << 2))
		adapter->stats.rx_other_errs	+= COUNTER_WRAP_16_BIT;
	if (carry_reg1 & (1 << 6))
		adapter->stats.rx_crc_errs	+= COUNTER_WRAP_16_BIT;
	if (carry_reg1 & (1 << 3))
		adapter->stats.rx_overflows	+= COUNTER_WRAP_16_BIT;
	if (carry_reg1 & (1 << 0))
		adapter->stats.rcvd_pkts_dropped	+= COUNTER_WRAP_16_BIT;
	if (carry_reg2 & (1 << 16))
		adapter->stats.tx_max_pkt_errs	+= COUNTER_WRAP_12_BIT;
	if (carry_reg2 & (1 << 15))
		adapter->stats.tx_underflows	+= COUNTER_WRAP_12_BIT;
	if (carry_reg2 & (1 << 6))
		adapter->stats.tx_first_collisions += COUNTER_WRAP_12_BIT;
	if (carry_reg2 & (1 << 8))
		adapter->stats.tx_deferred	+= COUNTER_WRAP_12_BIT;
	if (carry_reg2 & (1 << 5))
		adapter->stats.tx_excessive_collisions += COUNTER_WRAP_12_BIT;
	if (carry_reg2 & (1 << 4))
		adapter->stats.tx_late_collisions	+= COUNTER_WRAP_12_BIT;
	if (carry_reg2 & (1 << 2))
		adapter->stats.tx_collisions	+= COUNTER_WRAP_12_BIT;
}

static int et131x_mdio_read(struct mii_bus *bus, int phy_addr, int reg)
{
	struct net_device *netdev = bus->priv;
	struct et131x_adapter *adapter = netdev_priv(netdev);
	u16 value;
	int ret;

	ret = et131x_phy_mii_read(adapter, phy_addr, reg, &value);

	if (ret < 0)
		return ret;
	else
		return value;
}

static int et131x_mdio_write(struct mii_bus *bus, int phy_addr, int reg, u16 value)
{
	struct net_device *netdev = bus->priv;
	struct et131x_adapter *adapter = netdev_priv(netdev);

	return et131x_mii_write(adapter, reg, value);
}

static int et131x_mdio_reset(struct mii_bus *bus)
{
	struct net_device *netdev = bus->priv;
	struct et131x_adapter *adapter = netdev_priv(netdev);

	et131x_mii_write(adapter, MII_BMCR, BMCR_RESET);

	return 0;
}

static void et1310_phy_power_down(struct et131x_adapter *adapter, bool down)
{
	u16 data;

	et131x_mii_read(adapter, MII_BMCR, &data);
	data &= ~BMCR_PDOWN;
	if (down)
		data |= BMCR_PDOWN;
	et131x_mii_write(adapter, MII_BMCR, data);
}

static void et131x_xcvr_init(struct et131x_adapter *adapter)
{
	u16 imr;
	u16 isr;
	u16 lcr2;

	et131x_mii_read(adapter, PHY_INTERRUPT_STATUS, &isr);
	et131x_mii_read(adapter, PHY_INTERRUPT_MASK, &imr);

	imr |= (ET_PHY_INT_MASK_AUTONEGSTAT &
		ET_PHY_INT_MASK_LINKSTAT &
		ET_PHY_INT_MASK_ENABLE);

	et131x_mii_write(adapter, PHY_INTERRUPT_MASK, imr);

	if ((adapter->eeprom_data[1] & 0x4) == 0) {
		et131x_mii_read(adapter, PHY_LED_2, &lcr2);

		lcr2 &= (ET_LED2_LED_100TX & ET_LED2_LED_1000T);
		lcr2 |= (LED_VAL_LINKON_ACTIVE << LED_LINK_SHIFT);

		if ((adapter->eeprom_data[1] & 0x8) == 0)
			lcr2 |= (LED_VAL_1000BT_100BTX << LED_TXRX_SHIFT);
		else
			lcr2 |= (LED_VAL_LINKON << LED_TXRX_SHIFT);

		et131x_mii_write(adapter, PHY_LED_2, lcr2);
	}
}

static void et131x_configure_global_regs(struct et131x_adapter *adapter)
{
	struct global_regs __iomem *regs = &adapter->regs->global;

	writel(0, &regs->rxq_start_addr);
	writel(INTERNAL_MEM_SIZE - 1, &regs->txq_end_addr);

	if (adapter->registry_jumbo_packet < 2048) {
		writel(PARM_RX_MEM_END_DEF, &regs->rxq_end_addr);
		writel(PARM_RX_MEM_END_DEF + 1, &regs->txq_start_addr);
	} else if (adapter->registry_jumbo_packet < 8192) {
		
		writel(INTERNAL_MEM_RX_OFFSET, &regs->rxq_end_addr);
		writel(INTERNAL_MEM_RX_OFFSET + 1, &regs->txq_start_addr);
	} else {
		writel(0x01b3, &regs->rxq_end_addr);
		writel(0x01b4, &regs->txq_start_addr);
	}

	
	writel(0, &regs->loopback);

	
	writel(0, &regs->msi_config);

	writel(0, &regs->watchdog_timer);
}

static void et131x_config_rx_dma_regs(struct et131x_adapter *adapter)
{
	struct rxdma_regs __iomem *rx_dma = &adapter->regs->rxdma;
	struct rx_ring *rx_local = &adapter->rx_ring;
	struct fbr_desc *fbr_entry;
	u32 entry;
	u32 psr_num_des;
	unsigned long flags;

	
	et131x_rx_dma_disable(adapter);

	writel((u32) ((u64)rx_local->rx_status_bus >> 32),
	       &rx_dma->dma_wb_base_hi);
	writel((u32) rx_local->rx_status_bus, &rx_dma->dma_wb_base_lo);

	memset(rx_local->rx_status_block, 0, sizeof(struct rx_status_block));

	writel((u32) ((u64)rx_local->ps_ring_physaddr >> 32),
	       &rx_dma->psr_base_hi);
	writel((u32) rx_local->ps_ring_physaddr, &rx_dma->psr_base_lo);
	writel(rx_local->psr_num_entries - 1, &rx_dma->psr_num_des);
	writel(0, &rx_dma->psr_full_offset);

	psr_num_des = readl(&rx_dma->psr_num_des) & 0xFFF;
	writel((psr_num_des * LO_MARK_PERCENT_FOR_PSR) / 100,
	       &rx_dma->psr_min_des);

	spin_lock_irqsave(&adapter->rcv_lock, flags);

	
	rx_local->local_psr_full = 0;

	
	fbr_entry = (struct fbr_desc *) rx_local->fbr[0]->ring_virtaddr;
	for (entry = 0; entry < rx_local->fbr[0]->num_entries; entry++) {
		fbr_entry->addr_hi = rx_local->fbr[0]->bus_high[entry];
		fbr_entry->addr_lo = rx_local->fbr[0]->bus_low[entry];
		fbr_entry->word2 = entry;
		fbr_entry++;
	}

	writel((u32) (rx_local->fbr[0]->real_physaddr >> 32),
	       &rx_dma->fbr1_base_hi);
	writel((u32) rx_local->fbr[0]->real_physaddr, &rx_dma->fbr1_base_lo);
	writel(rx_local->fbr[0]->num_entries - 1, &rx_dma->fbr1_num_des);
	writel(ET_DMA10_WRAP, &rx_dma->fbr1_full_offset);

	rx_local->fbr[0]->local_full = ET_DMA10_WRAP;
	writel(
	   ((rx_local->fbr[0]->num_entries * LO_MARK_PERCENT_FOR_RX) / 100) - 1,
	   &rx_dma->fbr1_min_des);

#ifdef USE_FBR0
	
	fbr_entry = (struct fbr_desc *) rx_local->fbr[1]->ring_virtaddr;
	for (entry = 0; entry < rx_local->fbr[1]->num_entries; entry++) {
		fbr_entry->addr_hi = rx_local->fbr[1]->bus_high[entry];
		fbr_entry->addr_lo = rx_local->fbr[1]->bus_low[entry];
		fbr_entry->word2 = entry;
		fbr_entry++;
	}

	writel((u32) (rx_local->fbr[1]->real_physaddr >> 32),
	       &rx_dma->fbr0_base_hi);
	writel((u32) rx_local->fbr[1]->real_physaddr, &rx_dma->fbr0_base_lo);
	writel(rx_local->fbr[1]->num_entries - 1, &rx_dma->fbr0_num_des);
	writel(ET_DMA10_WRAP, &rx_dma->fbr0_full_offset);

	rx_local->fbr[1]->local_full = ET_DMA10_WRAP;
	writel(
	   ((rx_local->fbr[1]->num_entries * LO_MARK_PERCENT_FOR_RX) / 100) - 1,
	   &rx_dma->fbr0_min_des);
#endif

	writel(PARM_RX_NUM_BUFS_DEF, &rx_dma->num_pkt_done);

	writel(PARM_RX_TIME_INT_DEF, &rx_dma->max_pkt_time);

	spin_unlock_irqrestore(&adapter->rcv_lock, flags);
}

static void et131x_config_tx_dma_regs(struct et131x_adapter *adapter)
{
	struct txdma_regs __iomem *txdma = &adapter->regs->txdma;

	
	writel((u32) ((u64)adapter->tx_ring.tx_desc_ring_pa >> 32),
	       &txdma->pr_base_hi);
	writel((u32) adapter->tx_ring.tx_desc_ring_pa,
	       &txdma->pr_base_lo);

	
	writel(NUM_DESC_PER_RING_TX - 1, &txdma->pr_num_des);

	
	writel((u32)((u64)adapter->tx_ring.tx_status_pa >> 32),
						&txdma->dma_wb_base_hi);
	writel((u32)adapter->tx_ring.tx_status_pa, &txdma->dma_wb_base_lo);

	*adapter->tx_ring.tx_status = 0;

	writel(0, &txdma->service_request);
	adapter->tx_ring.send_idx = 0;
}

static void et131x_adapter_setup(struct et131x_adapter *adapter)
{
	
	et131x_configure_global_regs(adapter);

	et1310_config_mac_regs1(adapter);

	
	
	writel(ET_MMC_ENABLE, &adapter->regs->mmc.mmc_ctrl);

	et1310_config_rxmac_regs(adapter);
	et1310_config_txmac_regs(adapter);

	et131x_config_rx_dma_regs(adapter);
	et131x_config_tx_dma_regs(adapter);

	et1310_config_macstat_regs(adapter);

	et1310_phy_power_down(adapter, 0);
	et131x_xcvr_init(adapter);
}

static void et131x_soft_reset(struct et131x_adapter *adapter)
{
	
	writel(0xc00f0000, &adapter->regs->mac.cfg1);

	
	writel(0x7F, &adapter->regs->global.sw_reset);
	writel(0x000f0000, &adapter->regs->mac.cfg1);
	writel(0x00000000, &adapter->regs->mac.cfg1);
}

static void et131x_enable_interrupts(struct et131x_adapter *adapter)
{
	u32 mask;

	
	if (adapter->flowcontrol == FLOW_TXONLY ||
			    adapter->flowcontrol == FLOW_BOTH)
		mask = INT_MASK_ENABLE;
	else
		mask = INT_MASK_ENABLE_NO_FLOW;

	writel(mask, &adapter->regs->global.int_mask);
}

static void et131x_disable_interrupts(struct et131x_adapter *adapter)
{
	
	writel(INT_MASK_DISABLE, &adapter->regs->global.int_mask);
}

static void et131x_tx_dma_disable(struct et131x_adapter *adapter)
{
	
	writel(ET_TXDMA_CSR_HALT|ET_TXDMA_SNGL_EPKT,
					&adapter->regs->txdma.csr);
}

static void et131x_enable_txrx(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	
	et131x_rx_dma_enable(adapter);
	et131x_tx_dma_enable(adapter);

	
	if (adapter->flags & fMP_ADAPTER_INTERRUPT_IN_USE)
		et131x_enable_interrupts(adapter);

	
	netif_start_queue(netdev);
}

static void et131x_disable_txrx(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	
	netif_stop_queue(netdev);

	
	et131x_rx_dma_disable(adapter);
	et131x_tx_dma_disable(adapter);

	
	et131x_disable_interrupts(adapter);
}

static void et131x_init_send(struct et131x_adapter *adapter)
{
	struct tcb *tcb;
	u32 ct;
	struct tx_ring *tx_ring;

	
	tx_ring = &adapter->tx_ring;
	tcb = adapter->tx_ring.tcb_ring;

	tx_ring->tcb_qhead = tcb;

	memset(tcb, 0, sizeof(struct tcb) * NUM_TCB);

	
	for (ct = 0; ct++ < NUM_TCB; tcb++)
		tcb->next = tcb + 1;

	
	tcb--;
	tx_ring->tcb_qtail = tcb;
	tcb->next = NULL;
	
	tx_ring->send_head = NULL;
	tx_ring->send_tail = NULL;
}

static void et1310_enable_phy_coma(struct et131x_adapter *adapter)
{
	unsigned long flags;
	u32 pmcsr;

	pmcsr = readl(&adapter->regs->global.pm_csr);


	
	spin_lock_irqsave(&adapter->send_hw_lock, flags);
	adapter->flags |= fMP_ADAPTER_LOWER_POWER;
	spin_unlock_irqrestore(&adapter->send_hw_lock, flags);

	

	et131x_disable_txrx(adapter->netdev);

	
	pmcsr &= ~ET_PMCSR_INIT;
	writel(pmcsr, &adapter->regs->global.pm_csr);

	
	pmcsr |= ET_PM_PHY_SW_COMA;
	writel(pmcsr, &adapter->regs->global.pm_csr);
}

static void et1310_disable_phy_coma(struct et131x_adapter *adapter)
{
	u32 pmcsr;

	pmcsr = readl(&adapter->regs->global.pm_csr);

	
	pmcsr |= ET_PMCSR_INIT;
	pmcsr &= ~ET_PM_PHY_SW_COMA;
	writel(pmcsr, &adapter->regs->global.pm_csr);


	
	et131x_init_send(adapter);

	et131x_soft_reset(adapter);

	
	et131x_adapter_setup(adapter);

	
	adapter->flags &= ~fMP_ADAPTER_LOWER_POWER;

	et131x_enable_txrx(adapter->netdev);
}

static inline u32 bump_free_buff_ring(u32 *free_buff_ring, u32 limit)
{
	u32 tmp_free_buff_ring = *free_buff_ring;
	tmp_free_buff_ring++;
	if ((tmp_free_buff_ring & ET_DMA10_MASK) > limit) {
		tmp_free_buff_ring &= ~ET_DMA10_MASK;
		tmp_free_buff_ring ^= ET_DMA10_WRAP;
	}
	
	tmp_free_buff_ring &= (ET_DMA10_MASK|ET_DMA10_WRAP);
	*free_buff_ring = tmp_free_buff_ring;
	return tmp_free_buff_ring;
}

static void et131x_align_allocated_memory(struct et131x_adapter *adapter,
					  u64 *phys_addr, u64 *offset,
					  u64 mask)
{
	u64 new_addr = *phys_addr & ~mask;

	*offset = 0;

	if (new_addr != *phys_addr) {
		
		new_addr += mask + 1;
		
		*offset = new_addr - *phys_addr;
		
		*phys_addr = new_addr;
	}
}

static int et131x_rx_dma_memory_alloc(struct et131x_adapter *adapter)
{
	u32 i, j;
	u32 bufsize;
	u32 pktstat_ringsize, fbr_chunksize;
	struct rx_ring *rx_ring;

	
	rx_ring = &adapter->rx_ring;

	
#ifdef USE_FBR0
	rx_ring->fbr[1] = kmalloc(sizeof(struct fbr_lookup), GFP_KERNEL);
#endif
	rx_ring->fbr[0] = kmalloc(sizeof(struct fbr_lookup), GFP_KERNEL);


	if (adapter->registry_jumbo_packet < 2048) {
#ifdef USE_FBR0
		rx_ring->fbr[1]->buffsize = 256;
		rx_ring->fbr[1]->num_entries = 512;
#endif
		rx_ring->fbr[0]->buffsize = 2048;
		rx_ring->fbr[0]->num_entries = 512;
	} else if (adapter->registry_jumbo_packet < 4096) {
#ifdef USE_FBR0
		rx_ring->fbr[1]->buffsize = 512;
		rx_ring->fbr[1]->num_entries = 1024;
#endif
		rx_ring->fbr[0]->buffsize = 4096;
		rx_ring->fbr[0]->num_entries = 512;
	} else {
#ifdef USE_FBR0
		rx_ring->fbr[1]->buffsize = 1024;
		rx_ring->fbr[1]->num_entries = 768;
#endif
		rx_ring->fbr[0]->buffsize = 16384;
		rx_ring->fbr[0]->num_entries = 128;
	}

#ifdef USE_FBR0
	adapter->rx_ring.psr_num_entries =
				adapter->rx_ring.fbr[1]->num_entries +
				adapter->rx_ring.fbr[0]->num_entries;
#else
	adapter->rx_ring.psr_num_entries = adapter->rx_ring.fbr[0]->num_entries;
#endif

	
	bufsize = (sizeof(struct fbr_desc) * rx_ring->fbr[0]->num_entries) +
									0xfff;
	rx_ring->fbr[0]->ring_virtaddr = dma_alloc_coherent(&adapter->pdev->dev,
					bufsize,
					&rx_ring->fbr[0]->ring_physaddr,
					GFP_KERNEL);
	if (!rx_ring->fbr[0]->ring_virtaddr) {
		dev_err(&adapter->pdev->dev,
			  "Cannot alloc memory for Free Buffer Ring 1\n");
		return -ENOMEM;
	}

	rx_ring->fbr[0]->real_physaddr = rx_ring->fbr[0]->ring_physaddr;

	
	et131x_align_allocated_memory(adapter,
				      &rx_ring->fbr[0]->real_physaddr,
				      &rx_ring->fbr[0]->offset, 0x0FFF);

	rx_ring->fbr[0]->ring_virtaddr =
			(void *)((u8 *) rx_ring->fbr[0]->ring_virtaddr +
			rx_ring->fbr[0]->offset);

#ifdef USE_FBR0
	
	bufsize = (sizeof(struct fbr_desc) * rx_ring->fbr[1]->num_entries) +
									0xfff;
	rx_ring->fbr[1]->ring_virtaddr = dma_alloc_coherent(&adapter->pdev->dev,
						bufsize,
						&rx_ring->fbr[1]->ring_physaddr,
						GFP_KERNEL);
	if (!rx_ring->fbr[1]->ring_virtaddr) {
		dev_err(&adapter->pdev->dev,
			  "Cannot alloc memory for Free Buffer Ring 0\n");
		return -ENOMEM;
	}

	rx_ring->fbr[1]->real_physaddr = rx_ring->fbr[1]->ring_physaddr;

	
	et131x_align_allocated_memory(adapter,
				      &rx_ring->fbr[1]->real_physaddr,
				      &rx_ring->fbr[1]->offset, 0x0FFF);

	rx_ring->fbr[1]->ring_virtaddr =
			(void *)((u8 *) rx_ring->fbr[1]->ring_virtaddr +
			rx_ring->fbr[1]->offset);
#endif
	for (i = 0; i < (rx_ring->fbr[0]->num_entries / FBR_CHUNKS); i++) {
		u64 fbr1_tmp_physaddr;
		u64 fbr1_offset;
		u32 fbr1_align;

		if (rx_ring->fbr[0]->buffsize > 4096)
			fbr1_align = 4096;
		else
			fbr1_align = rx_ring->fbr[0]->buffsize;

		fbr_chunksize =
		    (FBR_CHUNKS * rx_ring->fbr[0]->buffsize) + fbr1_align - 1;
		rx_ring->fbr[0]->mem_virtaddrs[i] =
		    dma_alloc_coherent(&adapter->pdev->dev, fbr_chunksize,
				       &rx_ring->fbr[0]->mem_physaddrs[i],
				       GFP_KERNEL);

		if (!rx_ring->fbr[0]->mem_virtaddrs[i]) {
			dev_err(&adapter->pdev->dev,
				"Could not alloc memory\n");
			return -ENOMEM;
		}

		
		fbr1_tmp_physaddr = rx_ring->fbr[0]->mem_physaddrs[i];

		et131x_align_allocated_memory(adapter,
					      &fbr1_tmp_physaddr,
					      &fbr1_offset, (fbr1_align - 1));

		for (j = 0; j < FBR_CHUNKS; j++) {
			u32 index = (i * FBR_CHUNKS) + j;

			rx_ring->fbr[0]->virt[index] =
			    (u8 *) rx_ring->fbr[0]->mem_virtaddrs[i] +
			    (j * rx_ring->fbr[0]->buffsize) + fbr1_offset;

			rx_ring->fbr[0]->bus_high[index] =
			    (u32) (fbr1_tmp_physaddr >> 32);
			rx_ring->fbr[0]->bus_low[index] =
			    (u32) fbr1_tmp_physaddr;

			fbr1_tmp_physaddr += rx_ring->fbr[0]->buffsize;

			rx_ring->fbr[0]->buffer1[index] =
			    rx_ring->fbr[0]->virt[index];
			rx_ring->fbr[0]->buffer2[index] =
			    rx_ring->fbr[0]->virt[index] - 4;
		}
	}

#ifdef USE_FBR0
	
	for (i = 0; i < (rx_ring->fbr[1]->num_entries / FBR_CHUNKS); i++) {
		u64 fbr0_tmp_physaddr;
		u64 fbr0_offset;

		fbr_chunksize =
		    ((FBR_CHUNKS + 1) * rx_ring->fbr[1]->buffsize) - 1;
		rx_ring->fbr[1]->mem_virtaddrs[i] =
		    dma_alloc_coherent(&adapter->pdev->dev, fbr_chunksize,
				       &rx_ring->fbr[1]->mem_physaddrs[i],
				       GFP_KERNEL);

		if (!rx_ring->fbr[1]->mem_virtaddrs[i]) {
			dev_err(&adapter->pdev->dev,
				"Could not alloc memory\n");
			return -ENOMEM;
		}

		
		fbr0_tmp_physaddr = rx_ring->fbr[1]->mem_physaddrs[i];

		et131x_align_allocated_memory(adapter,
					      &fbr0_tmp_physaddr,
					      &fbr0_offset,
					      rx_ring->fbr[1]->buffsize - 1);

		for (j = 0; j < FBR_CHUNKS; j++) {
			u32 index = (i * FBR_CHUNKS) + j;

			rx_ring->fbr[1]->virt[index] =
			    (u8 *) rx_ring->fbr[1]->mem_virtaddrs[i] +
			    (j * rx_ring->fbr[1]->buffsize) + fbr0_offset;

			rx_ring->fbr[1]->bus_high[index] =
			    (u32) (fbr0_tmp_physaddr >> 32);
			rx_ring->fbr[1]->bus_low[index] =
			    (u32) fbr0_tmp_physaddr;

			fbr0_tmp_physaddr += rx_ring->fbr[1]->buffsize;

			rx_ring->fbr[1]->buffer1[index] =
			    rx_ring->fbr[1]->virt[index];
			rx_ring->fbr[1]->buffer2[index] =
			    rx_ring->fbr[1]->virt[index] - 4;
		}
	}
#endif

	
	pktstat_ringsize =
	    sizeof(struct pkt_stat_desc) * adapter->rx_ring.psr_num_entries;

	rx_ring->ps_ring_virtaddr = dma_alloc_coherent(&adapter->pdev->dev,
						  pktstat_ringsize,
						  &rx_ring->ps_ring_physaddr,
						  GFP_KERNEL);

	if (!rx_ring->ps_ring_virtaddr) {
		dev_err(&adapter->pdev->dev,
			  "Cannot alloc memory for Packet Status Ring\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "Packet Status Ring %lx\n",
	    (unsigned long) rx_ring->ps_ring_physaddr);


	
	rx_ring->rx_status_block = dma_alloc_coherent(&adapter->pdev->dev,
					    sizeof(struct rx_status_block),
					    &rx_ring->rx_status_bus,
					    GFP_KERNEL);
	if (!rx_ring->rx_status_block) {
		dev_err(&adapter->pdev->dev,
			  "Cannot alloc memory for Status Block\n");
		return -ENOMEM;
	}
	rx_ring->num_rfd = NIC_DEFAULT_NUM_RFD;
	printk(KERN_INFO "PRS %lx\n", (unsigned long)rx_ring->rx_status_bus);

	rx_ring->recv_lookaside = kmem_cache_create(adapter->netdev->name,
						   sizeof(struct rfd),
						   0,
						   SLAB_CACHE_DMA |
						   SLAB_HWCACHE_ALIGN,
						   NULL);

	adapter->flags |= fMP_ADAPTER_RECV_LOOKASIDE;

	INIT_LIST_HEAD(&rx_ring->recv_list);
	return 0;
}

static void et131x_rx_dma_memory_free(struct et131x_adapter *adapter)
{
	u32 index;
	u32 bufsize;
	u32 pktstat_ringsize;
	struct rfd *rfd;
	struct rx_ring *rx_ring;

	
	rx_ring = &adapter->rx_ring;

	
	WARN_ON(rx_ring->num_ready_recv != rx_ring->num_rfd);

	while (!list_empty(&rx_ring->recv_list)) {
		rfd = (struct rfd *) list_entry(rx_ring->recv_list.next,
				struct rfd, list_node);

		list_del(&rfd->list_node);
		rfd->skb = NULL;
		kmem_cache_free(adapter->rx_ring.recv_lookaside, rfd);
	}

	
	if (rx_ring->fbr[0]->ring_virtaddr) {
		
		for (index = 0; index <
		     (rx_ring->fbr[0]->num_entries / FBR_CHUNKS); index++) {
			if (rx_ring->fbr[0]->mem_virtaddrs[index]) {
				u32 fbr1_align;

				if (rx_ring->fbr[0]->buffsize > 4096)
					fbr1_align = 4096;
				else
					fbr1_align = rx_ring->fbr[0]->buffsize;

				bufsize =
				    (rx_ring->fbr[0]->buffsize * FBR_CHUNKS) +
				    fbr1_align - 1;

				dma_free_coherent(&adapter->pdev->dev,
					bufsize,
					rx_ring->fbr[0]->mem_virtaddrs[index],
					rx_ring->fbr[0]->mem_physaddrs[index]);

				rx_ring->fbr[0]->mem_virtaddrs[index] = NULL;
			}
		}

		
		rx_ring->fbr[0]->ring_virtaddr = (void *)((u8 *)
		    rx_ring->fbr[0]->ring_virtaddr - rx_ring->fbr[0]->offset);

		bufsize =
		    (sizeof(struct fbr_desc) * rx_ring->fbr[0]->num_entries) +
									0xfff;

		dma_free_coherent(&adapter->pdev->dev, bufsize,
				    rx_ring->fbr[0]->ring_virtaddr,
				    rx_ring->fbr[0]->ring_physaddr);

		rx_ring->fbr[0]->ring_virtaddr = NULL;
	}

#ifdef USE_FBR0
	
	if (rx_ring->fbr[1]->ring_virtaddr) {
		
		for (index = 0; index <
		     (rx_ring->fbr[1]->num_entries / FBR_CHUNKS); index++) {
			if (rx_ring->fbr[1]->mem_virtaddrs[index]) {
				bufsize =
				    (rx_ring->fbr[1]->buffsize *
				     (FBR_CHUNKS + 1)) - 1;

				dma_free_coherent(&adapter->pdev->dev,
					bufsize,
					rx_ring->fbr[1]->mem_virtaddrs[index],
					rx_ring->fbr[1]->mem_physaddrs[index]);

				rx_ring->fbr[1]->mem_virtaddrs[index] = NULL;
			}
		}

		
		rx_ring->fbr[1]->ring_virtaddr = (void *)((u8 *)
		    rx_ring->fbr[1]->ring_virtaddr - rx_ring->fbr[1]->offset);

		bufsize =
		    (sizeof(struct fbr_desc) * rx_ring->fbr[1]->num_entries) +
									0xfff;

		dma_free_coherent(&adapter->pdev->dev,
				  bufsize,
				  rx_ring->fbr[1]->ring_virtaddr,
				  rx_ring->fbr[1]->ring_physaddr);

		rx_ring->fbr[1]->ring_virtaddr = NULL;
	}
#endif

	
	if (rx_ring->ps_ring_virtaddr) {
		pktstat_ringsize =
		    sizeof(struct pkt_stat_desc) *
		    adapter->rx_ring.psr_num_entries;

		dma_free_coherent(&adapter->pdev->dev, pktstat_ringsize,
				    rx_ring->ps_ring_virtaddr,
				    rx_ring->ps_ring_physaddr);

		rx_ring->ps_ring_virtaddr = NULL;
	}

	
	if (rx_ring->rx_status_block) {
		dma_free_coherent(&adapter->pdev->dev,
			sizeof(struct rx_status_block),
			rx_ring->rx_status_block, rx_ring->rx_status_bus);
		rx_ring->rx_status_block = NULL;
	}

	
	if (adapter->flags & fMP_ADAPTER_RECV_LOOKASIDE) {
		kmem_cache_destroy(rx_ring->recv_lookaside);
		adapter->flags &= ~fMP_ADAPTER_RECV_LOOKASIDE;
	}

	
#ifdef USE_FBR0
	kfree(rx_ring->fbr[1]);
#endif

	kfree(rx_ring->fbr[0]);

	
	rx_ring->num_ready_recv = 0;
}

static int et131x_init_recv(struct et131x_adapter *adapter)
{
	int status = -ENOMEM;
	struct rfd *rfd = NULL;
	u32 rfdct;
	u32 numrfd = 0;
	struct rx_ring *rx_ring;

	
	rx_ring = &adapter->rx_ring;

	
	for (rfdct = 0; rfdct < rx_ring->num_rfd; rfdct++) {
		rfd = kmem_cache_alloc(rx_ring->recv_lookaside,
						     GFP_ATOMIC | GFP_DMA);

		if (!rfd) {
			dev_err(&adapter->pdev->dev,
				  "Couldn't alloc RFD out of kmem_cache\n");
			status = -ENOMEM;
			continue;
		}

		rfd->skb = NULL;

		
		list_add_tail(&rfd->list_node, &rx_ring->recv_list);

		
		rx_ring->num_ready_recv++;
		numrfd++;
	}

	if (numrfd > NIC_MIN_NUM_RFD)
		status = 0;

	rx_ring->num_rfd = numrfd;

	if (status != 0) {
		kmem_cache_free(rx_ring->recv_lookaside, rfd);
		dev_err(&adapter->pdev->dev,
			  "Allocation problems in et131x_init_recv\n");
	}
	return status;
}

static void et131x_set_rx_dma_timer(struct et131x_adapter *adapter)
{
	struct phy_device *phydev = adapter->phydev;

	if (!phydev)
		return;

	if ((phydev->speed == SPEED_100) || (phydev->speed == SPEED_10)) {
		writel(0, &adapter->regs->rxdma.max_pkt_time);
		writel(1, &adapter->regs->rxdma.num_pkt_done);
	}
}

static void nic_return_rfd(struct et131x_adapter *adapter, struct rfd *rfd)
{
	struct rx_ring *rx_local = &adapter->rx_ring;
	struct rxdma_regs __iomem *rx_dma = &adapter->regs->rxdma;
	u16 buff_index = rfd->bufferindex;
	u8 ring_index = rfd->ringindex;
	unsigned long flags;

	if (
#ifdef USE_FBR0
	    (ring_index == 0 && buff_index < rx_local->fbr[1]->num_entries) ||
#endif
	    (ring_index == 1 && buff_index < rx_local->fbr[0]->num_entries)) {
		spin_lock_irqsave(&adapter->fbr_lock, flags);

		if (ring_index == 1) {
			struct fbr_desc *next = (struct fbr_desc *)
					(rx_local->fbr[0]->ring_virtaddr) +
					INDEX10(rx_local->fbr[0]->local_full);

			next->addr_hi = rx_local->fbr[0]->bus_high[buff_index];
			next->addr_lo = rx_local->fbr[0]->bus_low[buff_index];
			next->word2 = buff_index;

			writel(bump_free_buff_ring(
					&rx_local->fbr[0]->local_full,
					rx_local->fbr[0]->num_entries - 1),
					&rx_dma->fbr1_full_offset);
		}
#ifdef USE_FBR0
		else {
			struct fbr_desc *next = (struct fbr_desc *)
				rx_local->fbr[1]->ring_virtaddr +
				    INDEX10(rx_local->fbr[1]->local_full);

			next->addr_hi = rx_local->fbr[1]->bus_high[buff_index];
			next->addr_lo = rx_local->fbr[1]->bus_low[buff_index];
			next->word2 = buff_index;

			writel(bump_free_buff_ring(
					&rx_local->fbr[1]->local_full,
					rx_local->fbr[1]->num_entries - 1),
			       &rx_dma->fbr0_full_offset);
		}
#endif
		spin_unlock_irqrestore(&adapter->fbr_lock, flags);
	} else {
		dev_err(&adapter->pdev->dev,
			  "%s illegal Buffer Index returned\n", __func__);
	}

	spin_lock_irqsave(&adapter->rcv_lock, flags);
	list_add_tail(&rfd->list_node, &rx_local->recv_list);
	rx_local->num_ready_recv++;
	spin_unlock_irqrestore(&adapter->rcv_lock, flags);

	WARN_ON(rx_local->num_ready_recv > rx_local->num_rfd);
}

static struct rfd *nic_rx_pkts(struct et131x_adapter *adapter)
{
	struct rx_ring *rx_local = &adapter->rx_ring;
	struct rx_status_block *status;
	struct pkt_stat_desc *psr;
	struct rfd *rfd;
	u32 i;
	u8 *buf;
	unsigned long flags;
	struct list_head *element;
	u8 ring_index;
	u16 buff_index;
	u32 len;
	u32 word0;
	u32 word1;

	/* RX Status block is written by the DMA engine prior to every
	 * interrupt. It contains the next to be used entry in the Packet
	 * Status Ring, and also the two Free Buffer rings.
	 */
	status = rx_local->rx_status_block;
	word1 = status->word1 >> 16;	

	
	if ((word1 & 0x1FFF) == (rx_local->local_psr_full & 0x1FFF))
		
		return NULL;

	
	psr = (struct pkt_stat_desc *) (rx_local->ps_ring_virtaddr) +
			(rx_local->local_psr_full & 0xFFF);

	len = psr->word1 & 0xFFFF;
	ring_index = (psr->word1 >> 26) & 0x03;
	buff_index = (psr->word1 >> 16) & 0x3FF;
	word0 = psr->word0;

	
	
	add_12bit(&rx_local->local_psr_full, 1);
	if (
	  (rx_local->local_psr_full & 0xFFF) > rx_local->psr_num_entries - 1) {
		
		rx_local->local_psr_full &=  ~0xFFF;
		rx_local->local_psr_full ^= 0x1000;
	}

	writel(rx_local->local_psr_full,
	       &adapter->regs->rxdma.psr_full_offset);

#ifndef USE_FBR0
	if (ring_index != 1)
		return NULL;
#endif

#ifdef USE_FBR0
	if (ring_index > 1 ||
		(ring_index == 0 &&
		buff_index > rx_local->fbr[1]->num_entries - 1) ||
		(ring_index == 1 &&
		buff_index > rx_local->fbr[0]->num_entries - 1))
#else
	if (ring_index != 1 || buff_index > rx_local->fbr[0]->num_entries - 1)
#endif
	{
		
		dev_err(&adapter->pdev->dev,
			  "NICRxPkts PSR Entry %d indicates "
			  "length of %d and/or bad bi(%d)\n",
			  rx_local->local_psr_full & 0xFFF,
			  len, buff_index);
		return NULL;
	}

	
	spin_lock_irqsave(&adapter->rcv_lock, flags);

	rfd = NULL;
	element = rx_local->recv_list.next;
	rfd = (struct rfd *) list_entry(element, struct rfd, list_node);

	if (rfd == NULL) {
		spin_unlock_irqrestore(&adapter->rcv_lock, flags);
		return NULL;
	}

	list_del(&rfd->list_node);
	rx_local->num_ready_recv--;

	spin_unlock_irqrestore(&adapter->rcv_lock, flags);

	rfd->bufferindex = buff_index;
	rfd->ringindex = ring_index;

	if (len < (NIC_MIN_PACKET_SIZE + 4)) {
		adapter->stats.rx_other_errs++;
		len = 0;
	}

	if (len) {
		
		if ((word0 & ALCATEL_MULTICAST_PKT) &&
		    !(word0 & ALCATEL_BROADCAST_PKT)) {
			if ((adapter->packet_filter &
					ET131X_PACKET_TYPE_MULTICAST)
			    && !(adapter->packet_filter &
					ET131X_PACKET_TYPE_PROMISCUOUS)
			    && !(adapter->packet_filter &
					ET131X_PACKET_TYPE_ALL_MULTICAST)) {
				buf = rx_local->fbr[(ring_index == 0 ? 1 : 0)]->
						virt[buff_index];

				for (i = 0; i < adapter->multicast_addr_count;
				     i++) {
					if (buf[0] ==
						adapter->multicast_list[i][0]
					    && buf[1] ==
						adapter->multicast_list[i][1]
					    && buf[2] ==
						adapter->multicast_list[i][2]
					    && buf[3] ==
						adapter->multicast_list[i][3]
					    && buf[4] ==
						adapter->multicast_list[i][4]
					    && buf[5] ==
						adapter->multicast_list[i][5]) {
						break;
					}
				}

				if (i == adapter->multicast_addr_count)
					len = 0;
			}

			if (len > 0)
				adapter->stats.multicast_pkts_rcvd++;
		} else if (word0 & ALCATEL_BROADCAST_PKT)
			adapter->stats.broadcast_pkts_rcvd++;
		else
			adapter->stats.unicast_pkts_rcvd++;
	}

	if (len > 0) {
		struct sk_buff *skb = NULL;

		
		rfd->len = len;

		skb = dev_alloc_skb(rfd->len + 2);
		if (!skb) {
			dev_err(&adapter->pdev->dev,
				  "Couldn't alloc an SKB for Rx\n");
			return NULL;
		}

		adapter->net_stats.rx_bytes += rfd->len;

		memcpy(skb_put(skb, rfd->len),
		    rx_local->fbr[(ring_index == 0 ? 1 : 0)]->virt[buff_index],
		    rfd->len);

		skb->dev = adapter->netdev;
		skb->protocol = eth_type_trans(skb, adapter->netdev);
		skb->ip_summed = CHECKSUM_NONE;

		netif_rx_ni(skb);
	} else {
		rfd->len = 0;
	}

	nic_return_rfd(adapter, rfd);
	return rfd;
}

static void et131x_handle_recv_interrupt(struct et131x_adapter *adapter)
{
	struct rfd *rfd = NULL;
	u32 count = 0;
	bool done = true;

	
	while (count < NUM_PACKETS_HANDLED) {
		if (list_empty(&adapter->rx_ring.recv_list)) {
			WARN_ON(adapter->rx_ring.num_ready_recv != 0);
			done = false;
			break;
		}

		rfd = nic_rx_pkts(adapter);

		if (rfd == NULL)
			break;

		if (!adapter->packet_filter ||
		    !netif_carrier_ok(adapter->netdev) ||
		    rfd->len == 0)
			continue;

		
		adapter->net_stats.rx_packets++;

		
		if (adapter->rx_ring.num_ready_recv < RFD_LOW_WATER_MARK) {
			dev_warn(&adapter->pdev->dev,
				    "RFD's are running out\n");
		}
		count++;
	}

	if (count == NUM_PACKETS_HANDLED || !done) {
		adapter->rx_ring.unfinished_receives = true;
		writel(PARM_TX_TIME_INT_DEF * NANO_IN_A_MICRO,
		       &adapter->regs->global.watchdog_timer);
	} else
		
		adapter->rx_ring.unfinished_receives = false;
}

static int et131x_tx_dma_memory_alloc(struct et131x_adapter *adapter)
{
	int desc_size = 0;
	struct tx_ring *tx_ring = &adapter->tx_ring;

	
	adapter->tx_ring.tcb_ring =
		kcalloc(NUM_TCB, sizeof(struct tcb), GFP_ATOMIC | GFP_DMA);
	if (!adapter->tx_ring.tcb_ring) {
		dev_err(&adapter->pdev->dev, "Cannot alloc memory for TCBs\n");
		return -ENOMEM;
	}

	desc_size = (sizeof(struct tx_desc) * NUM_DESC_PER_RING_TX) + 4096 - 1;
	tx_ring->tx_desc_ring =
	    (struct tx_desc *) dma_alloc_coherent(&adapter->pdev->dev,
						  desc_size,
						  &tx_ring->tx_desc_ring_pa,
						  GFP_KERNEL);
	if (!adapter->tx_ring.tx_desc_ring) {
		dev_err(&adapter->pdev->dev,
			"Cannot alloc memory for Tx Ring\n");
		return -ENOMEM;
	}

	
	tx_ring->tx_status = dma_alloc_coherent(&adapter->pdev->dev,
						    sizeof(u32),
						    &tx_ring->tx_status_pa,
						    GFP_KERNEL);
	if (!adapter->tx_ring.tx_status_pa) {
		dev_err(&adapter->pdev->dev,
				  "Cannot alloc memory for Tx status block\n");
		return -ENOMEM;
	}
	return 0;
}

static void et131x_tx_dma_memory_free(struct et131x_adapter *adapter)
{
	int desc_size = 0;

	if (adapter->tx_ring.tx_desc_ring) {
		
		desc_size = (sizeof(struct tx_desc) * NUM_DESC_PER_RING_TX)
								+ 4096 - 1;
		dma_free_coherent(&adapter->pdev->dev,
				    desc_size,
				    adapter->tx_ring.tx_desc_ring,
				    adapter->tx_ring.tx_desc_ring_pa);
		adapter->tx_ring.tx_desc_ring = NULL;
	}

	
	if (adapter->tx_ring.tx_status) {
		dma_free_coherent(&adapter->pdev->dev,
				    sizeof(u32),
				    adapter->tx_ring.tx_status,
				    adapter->tx_ring.tx_status_pa);

		adapter->tx_ring.tx_status = NULL;
	}
	
	kfree(adapter->tx_ring.tcb_ring);
}

static int nic_send_packet(struct et131x_adapter *adapter, struct tcb *tcb)
{
	u32 i;
	struct tx_desc desc[24];	
	u32 frag = 0;
	u32 thiscopy, remainder;
	struct sk_buff *skb = tcb->skb;
	u32 nr_frags = skb_shinfo(skb)->nr_frags + 1;
	struct skb_frag_struct *frags = &skb_shinfo(skb)->frags[0];
	unsigned long flags;
	struct phy_device *phydev = adapter->phydev;

	if (nr_frags > 23)
		return -EIO;

	memset(desc, 0, sizeof(struct tx_desc) * (nr_frags + 1));

	for (i = 0; i < nr_frags; i++) {
		if (i == 0) {
			if ((skb->len - skb->data_len) <= 1514) {
				desc[frag].addr_hi = 0;
				desc[frag].len_vlan =
					skb->len - skb->data_len;

				desc[frag++].addr_lo =
				    dma_map_single(&adapter->pdev->dev,
						   skb->data,
						   skb->len -
						   skb->data_len,
						   DMA_TO_DEVICE);
			} else {
				desc[frag].addr_hi = 0;
				desc[frag].len_vlan =
				    (skb->len - skb->data_len) / 2;

				desc[frag++].addr_lo =
				    dma_map_single(&adapter->pdev->dev,
						   skb->data,
						   ((skb->len -
						     skb->data_len) / 2),
						   DMA_TO_DEVICE);
				desc[frag].addr_hi = 0;

				desc[frag].len_vlan =
				    (skb->len - skb->data_len) / 2;

				desc[frag++].addr_lo =
				    dma_map_single(&adapter->pdev->dev,
						   skb->data +
						   ((skb->len -
						     skb->data_len) / 2),
						   ((skb->len -
						     skb->data_len) / 2),
						   DMA_TO_DEVICE);
			}
		} else {
			desc[frag].addr_hi = 0;
			desc[frag].len_vlan =
					frags[i - 1].size;

			desc[frag++].addr_lo = skb_frag_dma_map(
							&adapter->pdev->dev,
							&frags[i - 1],
							0,
							frags[i - 1].size,
							DMA_TO_DEVICE);
		}
	}

	if (phydev && phydev->speed == SPEED_1000) {
		if (++adapter->tx_ring.since_irq == PARM_TX_NUM_BUFS_DEF) {
			
			desc[frag - 1].flags = 0x5;
			adapter->tx_ring.since_irq = 0;
		} else { 
			desc[frag - 1].flags = 0x1;
		}
	} else
		desc[frag - 1].flags = 0x5;

	desc[0].flags |= 2;	

	tcb->index_start = adapter->tx_ring.send_idx;
	tcb->stale = 0;

	spin_lock_irqsave(&adapter->send_hw_lock, flags);

	thiscopy = NUM_DESC_PER_RING_TX -
				INDEX10(adapter->tx_ring.send_idx);

	if (thiscopy >= frag) {
		remainder = 0;
		thiscopy = frag;
	} else {
		remainder = frag - thiscopy;
	}

	memcpy(adapter->tx_ring.tx_desc_ring +
	       INDEX10(adapter->tx_ring.send_idx), desc,
	       sizeof(struct tx_desc) * thiscopy);

	add_10bit(&adapter->tx_ring.send_idx, thiscopy);

	if (INDEX10(adapter->tx_ring.send_idx) == 0 ||
		  INDEX10(adapter->tx_ring.send_idx) == NUM_DESC_PER_RING_TX) {
		adapter->tx_ring.send_idx &= ~ET_DMA10_MASK;
		adapter->tx_ring.send_idx ^= ET_DMA10_WRAP;
	}

	if (remainder) {
		memcpy(adapter->tx_ring.tx_desc_ring,
		       desc + thiscopy,
		       sizeof(struct tx_desc) * remainder);

		add_10bit(&adapter->tx_ring.send_idx, remainder);
	}

	if (INDEX10(adapter->tx_ring.send_idx) == 0) {
		if (adapter->tx_ring.send_idx)
			tcb->index = NUM_DESC_PER_RING_TX - 1;
		else
			tcb->index = ET_DMA10_WRAP|(NUM_DESC_PER_RING_TX - 1);
	} else
		tcb->index = adapter->tx_ring.send_idx - 1;

	spin_lock(&adapter->tcb_send_qlock);

	if (adapter->tx_ring.send_tail)
		adapter->tx_ring.send_tail->next = tcb;
	else
		adapter->tx_ring.send_head = tcb;

	adapter->tx_ring.send_tail = tcb;

	WARN_ON(tcb->next != NULL);

	adapter->tx_ring.used++;

	spin_unlock(&adapter->tcb_send_qlock);

	
	writel(adapter->tx_ring.send_idx,
	       &adapter->regs->txdma.service_request);

	if (phydev && phydev->speed == SPEED_1000) {
		writel(PARM_TX_TIME_INT_DEF * NANO_IN_A_MICRO,
		       &adapter->regs->global.watchdog_timer);
	}
	spin_unlock_irqrestore(&adapter->send_hw_lock, flags);

	return 0;
}

static int send_packet(struct sk_buff *skb, struct et131x_adapter *adapter)
{
	int status;
	struct tcb *tcb = NULL;
	u16 *shbufva;
	unsigned long flags;

	
	if (skb->len < ETH_HLEN)
		return -EIO;

	
	spin_lock_irqsave(&adapter->tcb_ready_qlock, flags);

	tcb = adapter->tx_ring.tcb_qhead;

	if (tcb == NULL) {
		spin_unlock_irqrestore(&adapter->tcb_ready_qlock, flags);
		return -ENOMEM;
	}

	adapter->tx_ring.tcb_qhead = tcb->next;

	if (adapter->tx_ring.tcb_qhead == NULL)
		adapter->tx_ring.tcb_qtail = NULL;

	spin_unlock_irqrestore(&adapter->tcb_ready_qlock, flags);

	tcb->skb = skb;

	if (skb->data != NULL && skb->len - skb->data_len >= 6) {
		shbufva = (u16 *) skb->data;

		if ((shbufva[0] == 0xffff) &&
		    (shbufva[1] == 0xffff) && (shbufva[2] == 0xffff)) {
			tcb->flags |= fMP_DEST_BROAD;
		} else if ((shbufva[0] & 0x3) == 0x0001) {
			tcb->flags |=  fMP_DEST_MULTI;
		}
	}

	tcb->next = NULL;

	
	status = nic_send_packet(adapter, tcb);

	if (status != 0) {
		spin_lock_irqsave(&adapter->tcb_ready_qlock, flags);

		if (adapter->tx_ring.tcb_qtail)
			adapter->tx_ring.tcb_qtail->next = tcb;
		else
			
			adapter->tx_ring.tcb_qhead = tcb;

		adapter->tx_ring.tcb_qtail = tcb;
		spin_unlock_irqrestore(&adapter->tcb_ready_qlock, flags);
		return status;
	}
	WARN_ON(adapter->tx_ring.used > NUM_TCB);
	return 0;
}

static int et131x_send_packets(struct sk_buff *skb, struct net_device *netdev)
{
	int status = 0;
	struct et131x_adapter *adapter = netdev_priv(netdev);


	
	if (adapter->tx_ring.used >= NUM_TCB) {
		status = -ENOMEM;
	} else {
		if ((adapter->flags & fMP_ADAPTER_FAIL_SEND_MASK) ||
					!netif_carrier_ok(netdev)) {
			dev_kfree_skb_any(skb);
			skb = NULL;

			adapter->net_stats.tx_dropped++;
		} else {
			status = send_packet(skb, adapter);
			if (status != 0 && status != -ENOMEM) {
				dev_kfree_skb_any(skb);
				skb = NULL;
				adapter->net_stats.tx_dropped++;
			}
		}
	}
	return status;
}

static inline void free_send_packet(struct et131x_adapter *adapter,
						struct tcb *tcb)
{
	unsigned long flags;
	struct tx_desc *desc = NULL;
	struct net_device_stats *stats = &adapter->net_stats;

	if (tcb->flags & fMP_DEST_BROAD)
		atomic_inc(&adapter->stats.broadcast_pkts_xmtd);
	else if (tcb->flags & fMP_DEST_MULTI)
		atomic_inc(&adapter->stats.multicast_pkts_xmtd);
	else
		atomic_inc(&adapter->stats.unicast_pkts_xmtd);

	if (tcb->skb) {
		stats->tx_bytes += tcb->skb->len;

		do {
			desc = (struct tx_desc *)
				    (adapter->tx_ring.tx_desc_ring +
						INDEX10(tcb->index_start));

			dma_unmap_single(&adapter->pdev->dev,
					 desc->addr_lo,
					 desc->len_vlan, DMA_TO_DEVICE);

			add_10bit(&tcb->index_start, 1);
			if (INDEX10(tcb->index_start) >=
							NUM_DESC_PER_RING_TX) {
				tcb->index_start &= ~ET_DMA10_MASK;
				tcb->index_start ^= ET_DMA10_WRAP;
			}
		} while (desc != (adapter->tx_ring.tx_desc_ring +
				INDEX10(tcb->index)));

		dev_kfree_skb_any(tcb->skb);
	}

	memset(tcb, 0, sizeof(struct tcb));

	
	spin_lock_irqsave(&adapter->tcb_ready_qlock, flags);

	adapter->net_stats.tx_packets++;

	if (adapter->tx_ring.tcb_qtail)
		adapter->tx_ring.tcb_qtail->next = tcb;
	else
		
		adapter->tx_ring.tcb_qhead = tcb;

	adapter->tx_ring.tcb_qtail = tcb;

	spin_unlock_irqrestore(&adapter->tcb_ready_qlock, flags);
	WARN_ON(adapter->tx_ring.used < 0);
}

static void et131x_free_busy_send_packets(struct et131x_adapter *adapter)
{
	struct tcb *tcb;
	unsigned long flags;
	u32 freed = 0;

	
	spin_lock_irqsave(&adapter->tcb_send_qlock, flags);

	tcb = adapter->tx_ring.send_head;

	while (tcb != NULL && freed < NUM_TCB) {
		struct tcb *next = tcb->next;

		adapter->tx_ring.send_head = next;

		if (next == NULL)
			adapter->tx_ring.send_tail = NULL;

		adapter->tx_ring.used--;

		spin_unlock_irqrestore(&adapter->tcb_send_qlock, flags);

		freed++;
		free_send_packet(adapter, tcb);

		spin_lock_irqsave(&adapter->tcb_send_qlock, flags);

		tcb = adapter->tx_ring.send_head;
	}

	WARN_ON(freed == NUM_TCB);

	spin_unlock_irqrestore(&adapter->tcb_send_qlock, flags);

	adapter->tx_ring.used = 0;
}

static void et131x_handle_send_interrupt(struct et131x_adapter *adapter)
{
	unsigned long flags;
	u32 serviced;
	struct tcb *tcb;
	u32 index;

	serviced = readl(&adapter->regs->txdma.new_service_complete);
	index = INDEX10(serviced);

	spin_lock_irqsave(&adapter->tcb_send_qlock, flags);

	tcb = adapter->tx_ring.send_head;

	while (tcb &&
	       ((serviced ^ tcb->index) & ET_DMA10_WRAP) &&
	       index < INDEX10(tcb->index)) {
		adapter->tx_ring.used--;
		adapter->tx_ring.send_head = tcb->next;
		if (tcb->next == NULL)
			adapter->tx_ring.send_tail = NULL;

		spin_unlock_irqrestore(&adapter->tcb_send_qlock, flags);
		free_send_packet(adapter, tcb);
		spin_lock_irqsave(&adapter->tcb_send_qlock, flags);

		
		tcb = adapter->tx_ring.send_head;
	}
	while (tcb &&
	       !((serviced ^ tcb->index) & ET_DMA10_WRAP)
	       && index > (tcb->index & ET_DMA10_MASK)) {
		adapter->tx_ring.used--;
		adapter->tx_ring.send_head = tcb->next;
		if (tcb->next == NULL)
			adapter->tx_ring.send_tail = NULL;

		spin_unlock_irqrestore(&adapter->tcb_send_qlock, flags);
		free_send_packet(adapter, tcb);
		spin_lock_irqsave(&adapter->tcb_send_qlock, flags);

		
		tcb = adapter->tx_ring.send_head;
	}

	
	if (adapter->tx_ring.used <= NUM_TCB / 3)
		netif_wake_queue(adapter->netdev);

	spin_unlock_irqrestore(&adapter->tcb_send_qlock, flags);
}

static int et131x_get_settings(struct net_device *netdev,
			       struct ethtool_cmd *cmd)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	return phy_ethtool_gset(adapter->phydev, cmd);
}

static int et131x_set_settings(struct net_device *netdev,
			       struct ethtool_cmd *cmd)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	return phy_ethtool_sset(adapter->phydev, cmd);
}

static int et131x_get_regs_len(struct net_device *netdev)
{
#define ET131X_REGS_LEN 256
	return ET131X_REGS_LEN * sizeof(u32);
}

static void et131x_get_regs(struct net_device *netdev,
			    struct ethtool_regs *regs, void *regs_data)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct address_map __iomem *aregs = adapter->regs;
	u32 *regs_buff = regs_data;
	u32 num = 0;

	memset(regs_data, 0, et131x_get_regs_len(netdev));

	regs->version = (1 << 24) | (adapter->pdev->revision << 16) |
			adapter->pdev->device;

	
	et131x_mii_read(adapter, MII_BMCR, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_BMSR, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_PHYSID1, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_PHYSID2, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_ADVERTISE, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_LPA, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_EXPANSION, (u16 *)&regs_buff[num++]);
	
	et131x_mii_read(adapter, 0x07, (u16 *)&regs_buff[num++]);
	
	et131x_mii_read(adapter, 0x08, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_CTRL1000, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_STAT1000, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, MII_ESTATUS, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_INDEX_REG, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_DATA_REG, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_MPHY_CONTROL_REG,
			(u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_LOOPBACK_CONTROL,
			(u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_LOOPBACK_CONTROL+1,
			(u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_REGISTER_MGMT_CONTROL,
			(u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_CONFIG, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_PHY_CONTROL, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_INTERRUPT_MASK, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_INTERRUPT_STATUS,
			(u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_PHY_STATUS, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_LED_1, (u16 *)&regs_buff[num++]);
	et131x_mii_read(adapter, PHY_LED_2, (u16 *)&regs_buff[num++]);

	
	regs_buff[num++] = readl(&aregs->global.txq_start_addr);
	regs_buff[num++] = readl(&aregs->global.txq_end_addr);
	regs_buff[num++] = readl(&aregs->global.rxq_start_addr);
	regs_buff[num++] = readl(&aregs->global.rxq_end_addr);
	regs_buff[num++] = readl(&aregs->global.pm_csr);
	regs_buff[num++] = adapter->stats.interrupt_status;
	regs_buff[num++] = readl(&aregs->global.int_mask);
	regs_buff[num++] = readl(&aregs->global.int_alias_clr_en);
	regs_buff[num++] = readl(&aregs->global.int_status_alias);
	regs_buff[num++] = readl(&aregs->global.sw_reset);
	regs_buff[num++] = readl(&aregs->global.slv_timer);
	regs_buff[num++] = readl(&aregs->global.msi_config);
	regs_buff[num++] = readl(&aregs->global.loopback);
	regs_buff[num++] = readl(&aregs->global.watchdog_timer);

	
	regs_buff[num++] = readl(&aregs->txdma.csr);
	regs_buff[num++] = readl(&aregs->txdma.pr_base_hi);
	regs_buff[num++] = readl(&aregs->txdma.pr_base_lo);
	regs_buff[num++] = readl(&aregs->txdma.pr_num_des);
	regs_buff[num++] = readl(&aregs->txdma.txq_wr_addr);
	regs_buff[num++] = readl(&aregs->txdma.txq_wr_addr_ext);
	regs_buff[num++] = readl(&aregs->txdma.txq_rd_addr);
	regs_buff[num++] = readl(&aregs->txdma.dma_wb_base_hi);
	regs_buff[num++] = readl(&aregs->txdma.dma_wb_base_lo);
	regs_buff[num++] = readl(&aregs->txdma.service_request);
	regs_buff[num++] = readl(&aregs->txdma.service_complete);
	regs_buff[num++] = readl(&aregs->txdma.cache_rd_index);
	regs_buff[num++] = readl(&aregs->txdma.cache_wr_index);
	regs_buff[num++] = readl(&aregs->txdma.tx_dma_error);
	regs_buff[num++] = readl(&aregs->txdma.desc_abort_cnt);
	regs_buff[num++] = readl(&aregs->txdma.payload_abort_cnt);
	regs_buff[num++] = readl(&aregs->txdma.writeback_abort_cnt);
	regs_buff[num++] = readl(&aregs->txdma.desc_timeout_cnt);
	regs_buff[num++] = readl(&aregs->txdma.payload_timeout_cnt);
	regs_buff[num++] = readl(&aregs->txdma.writeback_timeout_cnt);
	regs_buff[num++] = readl(&aregs->txdma.desc_error_cnt);
	regs_buff[num++] = readl(&aregs->txdma.payload_error_cnt);
	regs_buff[num++] = readl(&aregs->txdma.writeback_error_cnt);
	regs_buff[num++] = readl(&aregs->txdma.dropped_tlp_cnt);
	regs_buff[num++] = readl(&aregs->txdma.new_service_complete);
	regs_buff[num++] = readl(&aregs->txdma.ethernet_packet_cnt);

	
	regs_buff[num++] = readl(&aregs->rxdma.csr);
	regs_buff[num++] = readl(&aregs->rxdma.dma_wb_base_hi);
	regs_buff[num++] = readl(&aregs->rxdma.dma_wb_base_lo);
	regs_buff[num++] = readl(&aregs->rxdma.num_pkt_done);
	regs_buff[num++] = readl(&aregs->rxdma.max_pkt_time);
	regs_buff[num++] = readl(&aregs->rxdma.rxq_rd_addr);
	regs_buff[num++] = readl(&aregs->rxdma.rxq_rd_addr_ext);
	regs_buff[num++] = readl(&aregs->rxdma.rxq_wr_addr);
	regs_buff[num++] = readl(&aregs->rxdma.psr_base_hi);
	regs_buff[num++] = readl(&aregs->rxdma.psr_base_lo);
	regs_buff[num++] = readl(&aregs->rxdma.psr_num_des);
	regs_buff[num++] = readl(&aregs->rxdma.psr_avail_offset);
	regs_buff[num++] = readl(&aregs->rxdma.psr_full_offset);
	regs_buff[num++] = readl(&aregs->rxdma.psr_access_index);
	regs_buff[num++] = readl(&aregs->rxdma.psr_min_des);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_base_lo);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_base_hi);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_num_des);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_avail_offset);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_full_offset);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_rd_index);
	regs_buff[num++] = readl(&aregs->rxdma.fbr0_min_des);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_base_lo);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_base_hi);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_num_des);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_avail_offset);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_full_offset);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_rd_index);
	regs_buff[num++] = readl(&aregs->rxdma.fbr1_min_des);
}

#define ET131X_DRVINFO_LEN 32 
static void et131x_get_drvinfo(struct net_device *netdev,
			       struct ethtool_drvinfo *info)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	strncpy(info->driver, DRIVER_NAME, ET131X_DRVINFO_LEN);
	strncpy(info->version, DRIVER_VERSION, ET131X_DRVINFO_LEN);
	strncpy(info->bus_info, pci_name(adapter->pdev), ET131X_DRVINFO_LEN);
}

static struct ethtool_ops et131x_ethtool_ops = {
	.get_settings	= et131x_get_settings,
	.set_settings	= et131x_set_settings,
	.get_drvinfo	= et131x_get_drvinfo,
	.get_regs_len	= et131x_get_regs_len,
	.get_regs	= et131x_get_regs,
	.get_link = ethtool_op_get_link,
};

static void et131x_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &et131x_ethtool_ops);
}

static void et131x_hwaddr_init(struct et131x_adapter *adapter)
{
	if (adapter->rom_addr[0] == 0x00 &&
	    adapter->rom_addr[1] == 0x00 &&
	    adapter->rom_addr[2] == 0x00 &&
	    adapter->rom_addr[3] == 0x00 &&
	    adapter->rom_addr[4] == 0x00 &&
	    adapter->rom_addr[5] == 0x00) {
		get_random_bytes(&adapter->addr[5], 1);
		memcpy(adapter->rom_addr,
			adapter->addr, ETH_ALEN);
	} else {
		memcpy(adapter->addr,
		       adapter->rom_addr, ETH_ALEN);
	}
}

static int et131x_pci_init(struct et131x_adapter *adapter,
						struct pci_dev *pdev)
{
	int cap = pci_pcie_cap(pdev);
	u16 max_payload;
	u16 ctl;
	int i, rc;

	rc = et131x_init_eeprom(adapter);
	if (rc < 0)
		goto out;

	if (!cap) {
		dev_err(&pdev->dev, "Missing PCIe capabilities\n");
		goto err_out;
	}
		
	if (pci_read_config_word(pdev, cap + PCI_EXP_DEVCAP, &max_payload)) {
		dev_err(&pdev->dev,
		    "Could not read PCI config space for Max Payload Size\n");
		goto err_out;
	}

	
	max_payload &= 0x07;

	if (max_payload < 2) {
		static const u16 acknak[2] = { 0x76, 0xD0 };
		static const u16 replay[2] = { 0x1E0, 0x2ED };

		if (pci_write_config_word(pdev, ET1310_PCI_ACK_NACK,
					       acknak[max_payload])) {
			dev_err(&pdev->dev,
			  "Could not write PCI config space for ACK/NAK\n");
			goto err_out;
		}
		if (pci_write_config_word(pdev, ET1310_PCI_REPLAY,
					       replay[max_payload])) {
			dev_err(&pdev->dev,
			  "Could not write PCI config space for Replay Timer\n");
			goto err_out;
		}
	}

	if (pci_write_config_byte(pdev, ET1310_PCI_L0L1LATENCY, 0x11)) {
		dev_err(&pdev->dev,
		  "Could not write PCI config space for Latency Timers\n");
		goto err_out;
	}

	
	if (pci_read_config_word(pdev, cap + PCI_EXP_DEVCTL, &ctl)) {
		dev_err(&pdev->dev,
			"Could not read PCI config space for Max read size\n");
		goto err_out;
	}

	ctl = (ctl & ~PCI_EXP_DEVCTL_READRQ) | ( 0x04 << 12);

	if (pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL, ctl)) {
		dev_err(&pdev->dev,
		      "Could not write PCI config space for Max read size\n");
		goto err_out;
	}

	if (!adapter->has_eeprom) {
		et131x_hwaddr_init(adapter);
		return 0;
	}

	for (i = 0; i < ETH_ALEN; i++) {
		if (pci_read_config_byte(pdev, ET1310_PCI_MAC_ADDRESS + i,
					adapter->rom_addr + i)) {
			dev_err(&pdev->dev, "Could not read PCI config space for MAC address\n");
			goto err_out;
		}
	}
	memcpy(adapter->addr, adapter->rom_addr, ETH_ALEN);
out:
	return rc;
err_out:
	rc = -EIO;
	goto out;
}

static void et131x_error_timer_handler(unsigned long data)
{
	struct et131x_adapter *adapter = (struct et131x_adapter *) data;
	struct phy_device *phydev = adapter->phydev;

	if (et1310_in_phy_coma(adapter)) {
		et1310_disable_phy_coma(adapter);
		adapter->boot_coma = 20;
	} else {
		et1310_update_macstat_host_counters(adapter);
	}

	if (!phydev->link && adapter->boot_coma < 11)
		adapter->boot_coma++;

	if (adapter->boot_coma == 10) {
		if (!phydev->link) {
			if (!et1310_in_phy_coma(adapter)) {
				et131x_enable_interrupts(adapter);
				et1310_enable_phy_coma(adapter);
			}
		}
	}

	
	mod_timer(&adapter->error_timer, jiffies +
					  TX_ERROR_PERIOD * HZ / 1000);
}

static int et131x_adapter_memory_alloc(struct et131x_adapter *adapter)
{
	int status;

	
	status = et131x_tx_dma_memory_alloc(adapter);
	if (status != 0) {
		dev_err(&adapter->pdev->dev,
			  "et131x_tx_dma_memory_alloc FAILED\n");
		return status;
	}
	
	status = et131x_rx_dma_memory_alloc(adapter);
	if (status != 0) {
		dev_err(&adapter->pdev->dev,
			  "et131x_rx_dma_memory_alloc FAILED\n");
		et131x_tx_dma_memory_free(adapter);
		return status;
	}

	
	status = et131x_init_recv(adapter);
	if (status != 0) {
		dev_err(&adapter->pdev->dev,
			"et131x_init_recv FAILED\n");
		et131x_tx_dma_memory_free(adapter);
		et131x_rx_dma_memory_free(adapter);
	}
	return status;
}

static void et131x_adapter_memory_free(struct et131x_adapter *adapter)
{
	
	et131x_tx_dma_memory_free(adapter);
	et131x_rx_dma_memory_free(adapter);
}

static void et131x_adjust_link(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct  phy_device *phydev = adapter->phydev;

	if (netif_carrier_ok(netdev)) {
		adapter->boot_coma = 20;

		if (phydev && phydev->speed == SPEED_10) {
			u16 register18;

			et131x_mii_read(adapter, PHY_MPHY_CONTROL_REG,
					 &register18);
			et131x_mii_write(adapter, PHY_MPHY_CONTROL_REG,
					 register18 | 0x4);
			et131x_mii_write(adapter, PHY_INDEX_REG,
					 register18 | 0x8402);
			et131x_mii_write(adapter, PHY_DATA_REG,
					 register18 | 511);
			et131x_mii_write(adapter, PHY_MPHY_CONTROL_REG,
					 register18);
		}

		et1310_config_flow_control(adapter);

		if (phydev && phydev->speed == SPEED_1000 &&
				adapter->registry_jumbo_packet > 2048) {
			u16 reg;

			et131x_mii_read(adapter, PHY_CONFIG, &reg);
			reg &= ~ET_PHY_CONFIG_TX_FIFO_DEPTH;
			reg |= ET_PHY_CONFIG_FIFO_DEPTH_32;
			et131x_mii_write(adapter, PHY_CONFIG, reg);
		}

		et131x_set_rx_dma_timer(adapter);
		et1310_config_mac_regs2(adapter);
	}

	if (phydev && phydev->link != adapter->link) {
		if (et1310_in_phy_coma(adapter))
			et1310_disable_phy_coma(adapter);

		if (phydev->link) {
			adapter->boot_coma = 20;
		} else {
			dev_warn(&adapter->pdev->dev,
			    "Link down - cable problem ?\n");
			adapter->boot_coma = 0;

			if (phydev->speed == SPEED_10) {
				u16 register18;

				et131x_mii_read(adapter, PHY_MPHY_CONTROL_REG,
						 &register18);
				et131x_mii_write(adapter, PHY_MPHY_CONTROL_REG,
						 register18 | 0x4);
				et131x_mii_write(adapter, PHY_INDEX_REG,
						 register18 | 0x8402);
				et131x_mii_write(adapter, PHY_DATA_REG,
						 register18 | 511);
				et131x_mii_write(adapter, PHY_MPHY_CONTROL_REG,
						 register18);
			}

			
			et131x_free_busy_send_packets(adapter);

			
			et131x_init_send(adapter);

			et131x_soft_reset(adapter);

			
			et131x_adapter_setup(adapter);

			
			et131x_disable_txrx(netdev);
			et131x_enable_txrx(netdev);
		}

		adapter->link = phydev->link;

		phy_print_status(phydev);
	}
}

static int et131x_mii_probe(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct  phy_device *phydev = NULL;

	phydev = phy_find_first(adapter->mii_bus);
	if (!phydev) {
		dev_err(&adapter->pdev->dev, "no PHY found\n");
		return -ENODEV;
	}

	phydev = phy_connect(netdev, dev_name(&phydev->dev),
			&et131x_adjust_link, 0, PHY_INTERFACE_MODE_MII);

	if (IS_ERR(phydev)) {
		dev_err(&adapter->pdev->dev, "Could not attach to PHY\n");
		return PTR_ERR(phydev);
	}

	phydev->supported &= (SUPPORTED_10baseT_Half
				| SUPPORTED_10baseT_Full
				| SUPPORTED_100baseT_Half
				| SUPPORTED_100baseT_Full
				| SUPPORTED_Autoneg
				| SUPPORTED_MII
				| SUPPORTED_TP);

	if (adapter->pdev->device != ET131X_PCI_DEVICE_ID_FAST)
		phydev->supported |= SUPPORTED_1000baseT_Full;

	phydev->advertising = phydev->supported;
	adapter->phydev = phydev;

	dev_info(&adapter->pdev->dev, "attached PHY driver [%s] "
		 "(mii_bus:phy_addr=%s)\n",
		 phydev->drv->name, dev_name(&phydev->dev));

	return 0;
}

static struct et131x_adapter *et131x_adapter_init(struct net_device *netdev,
		struct pci_dev *pdev)
{
	static const u8 default_mac[] = { 0x00, 0x05, 0x3d, 0x00, 0x02, 0x00 };

	struct et131x_adapter *adapter;

	
	adapter = netdev_priv(netdev);
	adapter->pdev = pci_dev_get(pdev);
	adapter->netdev = netdev;

	
	spin_lock_init(&adapter->lock);
	spin_lock_init(&adapter->tcb_send_qlock);
	spin_lock_init(&adapter->tcb_ready_qlock);
	spin_lock_init(&adapter->send_hw_lock);
	spin_lock_init(&adapter->rcv_lock);
	spin_lock_init(&adapter->rcv_pend_lock);
	spin_lock_init(&adapter->fbr_lock);
	spin_lock_init(&adapter->phy_lock);

	adapter->registry_jumbo_packet = 1514;	

	
	memcpy(adapter->addr, default_mac, ETH_ALEN);

	return adapter;
}

static void __devexit et131x_pci_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct et131x_adapter *adapter = netdev_priv(netdev);

	unregister_netdev(netdev);
	phy_disconnect(adapter->phydev);
	mdiobus_unregister(adapter->mii_bus);
	kfree(adapter->mii_bus->irq);
	mdiobus_free(adapter->mii_bus);

	et131x_adapter_memory_free(adapter);
	iounmap(adapter->regs);
	pci_dev_put(pdev);

	free_netdev(netdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static void et131x_up(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	et131x_enable_txrx(netdev);
	phy_start(adapter->phydev);
}

static void et131x_down(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	
	netdev->trans_start = jiffies;

	phy_stop(adapter->phydev);
	et131x_disable_txrx(netdev);
}

#ifdef CONFIG_PM_SLEEP
static int et131x_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct net_device *netdev = pci_get_drvdata(pdev);

	if (netif_running(netdev)) {
		netif_device_detach(netdev);
		et131x_down(netdev);
		pci_save_state(pdev);
	}

	return 0;
}

static int et131x_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct net_device *netdev = pci_get_drvdata(pdev);

	if (netif_running(netdev)) {
		pci_restore_state(pdev);
		et131x_up(netdev);
		netif_device_attach(netdev);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(et131x_pm_ops, et131x_suspend, et131x_resume);
#define ET131X_PM_OPS (&et131x_pm_ops)
#else
#define ET131X_PM_OPS NULL
#endif

irqreturn_t et131x_isr(int irq, void *dev_id)
{
	bool handled = true;
	struct net_device *netdev = (struct net_device *)dev_id;
	struct et131x_adapter *adapter = NULL;
	u32 status;

	if (!netif_device_present(netdev)) {
		handled = false;
		goto out;
	}

	adapter = netdev_priv(netdev);


	
	et131x_disable_interrupts(adapter);

	status = readl(&adapter->regs->global.int_status);

	if (adapter->flowcontrol == FLOW_TXONLY ||
	    adapter->flowcontrol == FLOW_BOTH) {
		status &= ~INT_MASK_ENABLE;
	} else {
		status &= ~INT_MASK_ENABLE_NO_FLOW;
	}

	
	if (!status) {
		handled = false;
		et131x_enable_interrupts(adapter);
		goto out;
	}

	

	if (status & ET_INTR_WATCHDOG) {
		struct tcb *tcb = adapter->tx_ring.send_head;

		if (tcb)
			if (++tcb->stale > 1)
				status |= ET_INTR_TXDMA_ISR;

		if (adapter->rx_ring.unfinished_receives)
			status |= ET_INTR_RXDMA_XFR_DONE;
		else if (tcb == NULL)
			writel(0, &adapter->regs->global.watchdog_timer);

		status &= ~ET_INTR_WATCHDOG;
	}

	if (status == 0) {
		et131x_enable_interrupts(adapter);
		goto out;
	}

	adapter->stats.interrupt_status = status;

	schedule_work(&adapter->task);
out:
	return IRQ_RETVAL(handled);
}

static void et131x_isr_handler(struct work_struct *work)
{
	struct et131x_adapter *adapter =
		container_of(work, struct et131x_adapter, task);
	u32 status = adapter->stats.interrupt_status;
	struct address_map __iomem *iomem = adapter->regs;

	
	if (status & ET_INTR_TXDMA_ISR)
		et131x_handle_send_interrupt(adapter);

	
	if (status & ET_INTR_RXDMA_XFR_DONE)
		et131x_handle_recv_interrupt(adapter);

	status &= 0xffffffd7;

	if (status) {
		
		if (status & ET_INTR_TXDMA_ERR) {
			u32 txdma_err;

			
			txdma_err = readl(&iomem->txdma.tx_dma_error);

			dev_warn(&adapter->pdev->dev,
				    "TXDMA_ERR interrupt, error = %d\n",
				    txdma_err);
		}

		
		if (status &
		    (ET_INTR_RXDMA_FB_R0_LOW | ET_INTR_RXDMA_FB_R1_LOW)) {

			if (adapter->flowcontrol == FLOW_TXONLY ||
			    adapter->flowcontrol == FLOW_BOTH) {
				u32 pm_csr;

				pm_csr = readl(&iomem->global.pm_csr);
				if (!et1310_in_phy_coma(adapter))
					writel(3, &iomem->txmac.bp_ctrl);
			}
		}

		
		if (status & ET_INTR_RXDMA_STAT_LOW) {

		}

		
		if (status & ET_INTR_RXDMA_ERR) {
			

			dev_warn(&adapter->pdev->dev,
				    "RxDMA_ERR interrupt, error %x\n",
				    readl(&iomem->txmac.tx_test));
		}

		
		if (status & ET_INTR_WOL) {
			dev_err(&adapter->pdev->dev, "WAKE_ON_LAN interrupt\n");
		}

		
		if (status & ET_INTR_TXMAC) {
			u32 err = readl(&iomem->txmac.err);

			dev_warn(&adapter->pdev->dev,
				    "TXMAC interrupt, error 0x%08x\n",
				    err);

		}

		
		if (status & ET_INTR_RXMAC) {

			dev_warn(&adapter->pdev->dev,
			  "RXMAC interrupt, error 0x%08x.  Requesting reset\n",
				    readl(&iomem->rxmac.err_reg));

			dev_warn(&adapter->pdev->dev,
				    "Enable 0x%08x, Diag 0x%08x\n",
				    readl(&iomem->rxmac.ctrl),
				    readl(&iomem->rxmac.rxq_diag));

		}

		
		if (status & ET_INTR_MAC_STAT) {
			et1310_handle_macstat_interrupt(adapter);
		}

		
		if (status & ET_INTR_SLV_TIMEOUT) {
		}
	}
	et131x_enable_interrupts(adapter);
}

static struct net_device_stats *et131x_stats(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct net_device_stats *stats = &adapter->net_stats;
	struct ce_stats *devstat = &adapter->stats;

	stats->rx_errors = devstat->rx_length_errs +
			   devstat->rx_align_errs +
			   devstat->rx_crc_errs +
			   devstat->rx_code_violations +
			   devstat->rx_other_errs;
	stats->tx_errors = devstat->tx_max_pkt_errs;
	stats->multicast = devstat->multicast_pkts_rcvd;
	stats->collisions = devstat->tx_collisions;

	stats->rx_length_errors = devstat->rx_length_errs;
	stats->rx_over_errors = devstat->rx_overflows;
	stats->rx_crc_errors = devstat->rx_crc_errs;

	
	
	
	

	
	
	
	

	
	
	
	
	
	return stats;
}

static int et131x_open(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct pci_dev *pdev = adapter->pdev;
	unsigned int irq = pdev->irq;
	int result;

	
	init_timer(&adapter->error_timer);
	adapter->error_timer.expires = jiffies + TX_ERROR_PERIOD * HZ / 1000;
	adapter->error_timer.function = et131x_error_timer_handler;
	adapter->error_timer.data = (unsigned long)adapter;
	add_timer(&adapter->error_timer);

	result = request_irq(irq, et131x_isr, IRQF_SHARED, netdev->name, netdev);
	if (result) {
		dev_err(&pdev->dev, "could not register IRQ %d\n", irq);
		return result;
	}

	adapter->flags |= fMP_ADAPTER_INTERRUPT_IN_USE;

	et131x_up(netdev);

	return result;
}

static int et131x_close(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	et131x_down(netdev);

	adapter->flags &= ~fMP_ADAPTER_INTERRUPT_IN_USE;
	free_irq(adapter->pdev->irq, netdev);

	
	return del_timer_sync(&adapter->error_timer);
}

static int et131x_ioctl(struct net_device *netdev, struct ifreq *reqbuf,
			int cmd)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);

	if (!adapter->phydev)
		return -EINVAL;

	return phy_mii_ioctl(adapter->phydev, reqbuf, cmd);
}

static int et131x_set_packet_filter(struct et131x_adapter *adapter)
{
	int filter = adapter->packet_filter;
	int status = 0;
	u32 ctrl;
	u32 pf_ctrl;

	ctrl = readl(&adapter->regs->rxmac.ctrl);
	pf_ctrl = readl(&adapter->regs->rxmac.pf_ctrl);

	ctrl |= 0x04;

	if ((filter & ET131X_PACKET_TYPE_PROMISCUOUS) || filter == 0)
		pf_ctrl &= ~7;	
	else {
		if (filter & ET131X_PACKET_TYPE_ALL_MULTICAST)
			pf_ctrl &= ~2;	
		else {
			et1310_setup_device_for_multicast(adapter);
			pf_ctrl |= 2;
			ctrl &= ~0x04;
		}

		
		if (filter & ET131X_PACKET_TYPE_DIRECTED) {
			et1310_setup_device_for_unicast(adapter);
			pf_ctrl |= 4;
			ctrl &= ~0x04;
		}

		
		if (filter & ET131X_PACKET_TYPE_BROADCAST) {
			pf_ctrl |= 1;	
			ctrl &= ~0x04;
		} else
			pf_ctrl &= ~1;

		writel(pf_ctrl, &adapter->regs->rxmac.pf_ctrl);
		writel(ctrl, &adapter->regs->rxmac.ctrl);
	}
	return status;
}

static void et131x_multicast(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	int packet_filter;
	unsigned long flags;
	struct netdev_hw_addr *ha;
	int i;

	spin_lock_irqsave(&adapter->lock, flags);

	packet_filter = adapter->packet_filter;

	packet_filter &= ~ET131X_PACKET_TYPE_MULTICAST;


	if (netdev->flags & IFF_PROMISC)
		adapter->packet_filter |= ET131X_PACKET_TYPE_PROMISCUOUS;
	else
		adapter->packet_filter &= ~ET131X_PACKET_TYPE_PROMISCUOUS;

	if (netdev->flags & IFF_ALLMULTI)
		adapter->packet_filter |= ET131X_PACKET_TYPE_ALL_MULTICAST;

	if (netdev_mc_count(netdev) > NIC_MAX_MCAST_LIST)
		adapter->packet_filter |= ET131X_PACKET_TYPE_ALL_MULTICAST;

	if (netdev_mc_count(netdev) < 1) {
		adapter->packet_filter &= ~ET131X_PACKET_TYPE_ALL_MULTICAST;
		adapter->packet_filter &= ~ET131X_PACKET_TYPE_MULTICAST;
	} else
		adapter->packet_filter |= ET131X_PACKET_TYPE_MULTICAST;

	
	i = 0;
	netdev_for_each_mc_addr(ha, netdev) {
		if (i == NIC_MAX_MCAST_LIST)
			break;
		memcpy(adapter->multicast_list[i++], ha->addr, ETH_ALEN);
	}
	adapter->multicast_addr_count = i;

	if (packet_filter != adapter->packet_filter) {
		
		et131x_set_packet_filter(adapter);
	}
	spin_unlock_irqrestore(&adapter->lock, flags);
}

static int et131x_tx(struct sk_buff *skb, struct net_device *netdev)
{
	int status = 0;
	struct et131x_adapter *adapter = netdev_priv(netdev);

	
	if (adapter->tx_ring.used >= NUM_TCB - 1 &&
	    !netif_queue_stopped(netdev))
		netif_stop_queue(netdev);

	
	netdev->trans_start = jiffies;

	
	status = et131x_send_packets(skb, netdev);

	
	if (status != 0) {
		if (status == -ENOMEM)
			status = NETDEV_TX_BUSY;
		else
			status = NETDEV_TX_OK;
	}
	return status;
}

static void et131x_tx_timeout(struct net_device *netdev)
{
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct tcb *tcb;
	unsigned long flags;

	
	if (~(adapter->flags & fMP_ADAPTER_INTERRUPT_IN_USE))
		return;

	if (adapter->flags & fMP_ADAPTER_NON_RECOVER_ERROR)
		return;

	
	if (adapter->flags & fMP_ADAPTER_HARDWARE_ERROR) {
		dev_err(&adapter->pdev->dev, "hardware error - reset\n");
		return;
	}

	
	spin_lock_irqsave(&adapter->tcb_send_qlock, flags);

	tcb = adapter->tx_ring.send_head;

	if (tcb != NULL) {
		tcb->count++;

		if (tcb->count > NIC_SEND_HANG_THRESHOLD) {
			spin_unlock_irqrestore(&adapter->tcb_send_qlock,
					       flags);

			dev_warn(&adapter->pdev->dev,
				"Send stuck - reset.  tcb->WrIndex %x, flags 0x%08x\n",
				tcb->index,
				tcb->flags);

			adapter->net_stats.tx_errors++;

			
			et131x_disable_txrx(netdev);
			et131x_enable_txrx(netdev);
			return;
		}
	}

	spin_unlock_irqrestore(&adapter->tcb_send_qlock, flags);
}

static int et131x_change_mtu(struct net_device *netdev, int new_mtu)
{
	int result = 0;
	struct et131x_adapter *adapter = netdev_priv(netdev);

	
	if (new_mtu < 64 || new_mtu > 9216)
		return -EINVAL;

	et131x_disable_txrx(netdev);
	et131x_handle_send_interrupt(adapter);
	et131x_handle_recv_interrupt(adapter);

	
	netdev->mtu = new_mtu;

	
	et131x_adapter_memory_free(adapter);

	
	adapter->registry_jumbo_packet = new_mtu + 14;
	et131x_soft_reset(adapter);

	
	result = et131x_adapter_memory_alloc(adapter);
	if (result != 0) {
		dev_warn(&adapter->pdev->dev,
			"Change MTU failed; couldn't re-alloc DMA memory\n");
		return result;
	}

	et131x_init_send(adapter);

	et131x_hwaddr_init(adapter);
	memcpy(netdev->dev_addr, adapter->addr, ETH_ALEN);

	
	et131x_adapter_setup(adapter);

	et131x_enable_txrx(netdev);

	return result;
}

static int et131x_set_mac_addr(struct net_device *netdev, void *new_mac)
{
	int result = 0;
	struct et131x_adapter *adapter = netdev_priv(netdev);
	struct sockaddr *address = new_mac;

	

	if (adapter == NULL)
		return -ENODEV;

	
	if (!is_valid_ether_addr(address->sa_data))
		return -EADDRNOTAVAIL;

	et131x_disable_txrx(netdev);
	et131x_handle_send_interrupt(adapter);
	et131x_handle_recv_interrupt(adapter);

	
	

	memcpy(netdev->dev_addr, address->sa_data, netdev->addr_len);

	printk(KERN_INFO "%s: Setting MAC address to %pM\n",
			netdev->name, netdev->dev_addr);

	
	et131x_adapter_memory_free(adapter);

	et131x_soft_reset(adapter);

	
	result = et131x_adapter_memory_alloc(adapter);
	if (result != 0) {
		dev_err(&adapter->pdev->dev,
			"Change MAC failed; couldn't re-alloc DMA memory\n");
		return result;
	}

	et131x_init_send(adapter);

	et131x_hwaddr_init(adapter);

	
	et131x_adapter_setup(adapter);

	et131x_enable_txrx(netdev);

	return result;
}

static const struct net_device_ops et131x_netdev_ops = {
	.ndo_open		= et131x_open,
	.ndo_stop		= et131x_close,
	.ndo_start_xmit		= et131x_tx,
	.ndo_set_rx_mode	= et131x_multicast,
	.ndo_tx_timeout		= et131x_tx_timeout,
	.ndo_change_mtu		= et131x_change_mtu,
	.ndo_set_mac_address	= et131x_set_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_get_stats		= et131x_stats,
	.ndo_do_ioctl		= et131x_ioctl,
};

static int __devinit et131x_pci_setup(struct pci_dev *pdev,
			       const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct et131x_adapter *adapter;
	int rc;
	int ii;

	rc = pci_enable_device(pdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "pci_enable_device() failed\n");
		goto out;
	}

	
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev, "Can't find PCI device's base address\n");
		rc = -ENODEV;
		goto err_disable;
	}

	rc = pci_request_regions(pdev, DRIVER_NAME);
	if (rc < 0) {
		dev_err(&pdev->dev, "Can't get PCI resources\n");
		goto err_disable;
	}

	pci_set_master(pdev);

	
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		rc = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
		if (rc < 0) {
			dev_err(&pdev->dev,
			  "Unable to obtain 64 bit DMA for consistent allocations\n");
			goto err_release_res;
		}
	} else if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		rc = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (rc < 0) {
			dev_err(&pdev->dev,
			  "Unable to obtain 32 bit DMA for consistent allocations\n");
			goto err_release_res;
		}
	} else {
		dev_err(&pdev->dev, "No usable DMA addressing method\n");
		rc = -EIO;
		goto err_release_res;
	}

	
	netdev = alloc_etherdev(sizeof(struct et131x_adapter));
	if (!netdev) {
		dev_err(&pdev->dev, "Couldn't alloc netdev struct\n");
		rc = -ENOMEM;
		goto err_release_res;
	}

	netdev->watchdog_timeo = ET131X_TX_TIMEOUT;
	netdev->netdev_ops     = &et131x_netdev_ops;

	SET_NETDEV_DEV(netdev, &pdev->dev);
	et131x_set_ethtool_ops(netdev);

	adapter = et131x_adapter_init(netdev, pdev);

	rc = et131x_pci_init(adapter, pdev);
	if (rc < 0)
		goto err_free_dev;

	
	adapter->regs = pci_ioremap_bar(pdev, 0);
	if (!adapter->regs) {
		dev_err(&pdev->dev, "Cannot map device registers\n");
		rc = -ENOMEM;
		goto err_free_dev;
	}

	
	writel(ET_PMCSR_INIT,  &adapter->regs->global.pm_csr);

	
	et131x_soft_reset(adapter);

	
	et131x_disable_interrupts(adapter);

	
	rc = et131x_adapter_memory_alloc(adapter);
	if (rc < 0) {
		dev_err(&pdev->dev, "Could not alloc adapater memory (DMA)\n");
		goto err_iounmap;
	}

	
	et131x_init_send(adapter);

	
	INIT_WORK(&adapter->task, et131x_isr_handler);

	
	memcpy(netdev->dev_addr, adapter->addr, ETH_ALEN);

	
	adapter->boot_coma = 0;
	et1310_disable_phy_coma(adapter);

	rc = -ENOMEM;

	
	adapter->mii_bus = mdiobus_alloc();
	if (!adapter->mii_bus) {
		dev_err(&pdev->dev, "Alloc of mii_bus struct failed\n");
		goto err_mem_free;
	}

	adapter->mii_bus->name = "et131x_eth_mii";
	snprintf(adapter->mii_bus->id, MII_BUS_ID_SIZE, "%x",
		(adapter->pdev->bus->number << 8) | adapter->pdev->devfn);
	adapter->mii_bus->priv = netdev;
	adapter->mii_bus->read = et131x_mdio_read;
	adapter->mii_bus->write = et131x_mdio_write;
	adapter->mii_bus->reset = et131x_mdio_reset;
	adapter->mii_bus->irq = kmalloc(sizeof(int)*PHY_MAX_ADDR, GFP_KERNEL);
	if (!adapter->mii_bus->irq) {
		dev_err(&pdev->dev, "mii_bus irq allocation failed\n");
		goto err_mdio_free;
	}

	for (ii = 0; ii < PHY_MAX_ADDR; ii++)
		adapter->mii_bus->irq[ii] = PHY_POLL;

	rc = mdiobus_register(adapter->mii_bus);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register MII bus\n");
		goto err_mdio_free_irq;
	}

	rc = et131x_mii_probe(netdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to probe MII bus\n");
		goto err_mdio_unregister;
	}

	
	et131x_adapter_setup(adapter);


	
	rc = register_netdev(netdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "register_netdev() failed\n");
		goto err_phy_disconnect;
	}

	pci_set_drvdata(pdev, netdev);
out:
	return rc;

err_phy_disconnect:
	phy_disconnect(adapter->phydev);
err_mdio_unregister:
	mdiobus_unregister(adapter->mii_bus);
err_mdio_free_irq:
	kfree(adapter->mii_bus->irq);
err_mdio_free:
	mdiobus_free(adapter->mii_bus);
err_mem_free:
	et131x_adapter_memory_free(adapter);
err_iounmap:
	iounmap(adapter->regs);
err_free_dev:
	pci_dev_put(pdev);
	free_netdev(netdev);
err_release_res:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	goto out;
}

static DEFINE_PCI_DEVICE_TABLE(et131x_pci_table) = {
	{ PCI_VDEVICE(ATT, ET131X_PCI_DEVICE_ID_GIG), 0UL},
	{ PCI_VDEVICE(ATT, ET131X_PCI_DEVICE_ID_FAST), 0UL},
	{0,}
};
MODULE_DEVICE_TABLE(pci, et131x_pci_table);

static struct pci_driver et131x_driver = {
	.name		= DRIVER_NAME,
	.id_table	= et131x_pci_table,
	.probe		= et131x_pci_setup,
	.remove		= __devexit_p(et131x_pci_remove),
	.driver.pm	= ET131X_PM_OPS,
};

static int __init et131x_init_module(void)
{
	return pci_register_driver(&et131x_driver);
}

static void __exit et131x_cleanup_module(void)
{
	pci_unregister_driver(&et131x_driver);
}

module_init(et131x_init_module);
module_exit(et131x_cleanup_module);

