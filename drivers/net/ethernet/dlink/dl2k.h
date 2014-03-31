/*
    Copyright (c) 2001, 2002 by D-Link Corporation
    Written by Edward Peng.<edward_peng@dlink.com.tw>
    Created 03-May-2001, base on Linux' sundance.c.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef __DL2K_H__
#define __DL2K_H__

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
#include <linux/crc32.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/bitops.h>
#include <asm/processor.h>	
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#define TX_RING_SIZE	256
#define TX_QUEUE_LEN	(TX_RING_SIZE - 1) 
#define RX_RING_SIZE 	256
#define TX_TOTAL_SIZE	TX_RING_SIZE*sizeof(struct netdev_desc)
#define RX_TOTAL_SIZE	RX_RING_SIZE*sizeof(struct netdev_desc)

/* This driver was written to use PCI memory space, however x86-oriented
   hardware often uses I/O space accesses. */
#ifndef MEM_MAPPING
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb inb
#define readw inw
#define readl inl
#define writeb outb
#define writew outw
#define writel outl
#endif

enum dl2x_offsets {
	
	DMACtrl = 0x00,
	RxDMAStatus = 0x08,
	TFDListPtr0 = 0x10,
	TFDListPtr1 = 0x14,
	TxDMABurstThresh = 0x18,
	TxDMAUrgentThresh = 0x19,
	TxDMAPollPeriod = 0x1a,
	RFDListPtr0 = 0x1c,
	RFDListPtr1 = 0x20,
	RxDMABurstThresh = 0x24,
	RxDMAUrgentThresh = 0x25,
	RxDMAPollPeriod = 0x26,
	RxDMAIntCtrl = 0x28,
	DebugCtrl = 0x2c,
	ASICCtrl = 0x30,
	FifoCtrl = 0x38,
	RxEarlyThresh = 0x3a,
	FlowOffThresh = 0x3c,
	FlowOnThresh = 0x3e,
	TxStartThresh = 0x44,
	EepromData = 0x48,
	EepromCtrl = 0x4a,
	ExpromAddr = 0x4c,
	Exprodata = 0x50,
	WakeEvent = 0x51,
	CountDown = 0x54,
	IntStatusAck = 0x5a,
	IntEnable = 0x5c,
	IntStatus = 0x5e,
	TxStatus = 0x60,
	MACCtrl = 0x6c,
	VLANTag = 0x70,
	PhyCtrl = 0x76,
	StationAddr0 = 0x78,
	StationAddr1 = 0x7a,
	StationAddr2 = 0x7c,
	VLANId = 0x80,
	MaxFrameSize = 0x86,
	ReceiveMode = 0x88,
	HashTable0 = 0x8c,
	HashTable1 = 0x90,
	RmonStatMask = 0x98,
	StatMask = 0x9c,
	RxJumboFrames = 0xbc,
	TCPCheckSumErrors = 0xc0,
	IPCheckSumErrors = 0xc2,
	UDPCheckSumErrors = 0xc4,
	TxJumboFrames = 0xf4,
	
	OctetRcvOk = 0xa8,
	McstOctetRcvOk = 0xac,
	BcstOctetRcvOk = 0xb0,
	FramesRcvOk = 0xb4,
	McstFramesRcvdOk = 0xb8,
	BcstFramesRcvdOk = 0xbe,
	MacControlFramesRcvd = 0xc6,
	FrameTooLongErrors = 0xc8,
	InRangeLengthErrors = 0xca,
	FramesCheckSeqErrors = 0xcc,
	FramesLostRxErrors = 0xce,
	OctetXmtOk = 0xd0,
	McstOctetXmtOk = 0xd4,
	BcstOctetXmtOk = 0xd8,
	FramesXmtOk = 0xdc,
	McstFramesXmtdOk = 0xe0,
	FramesWDeferredXmt = 0xe4,
	LateCollisions = 0xe8,
	MultiColFrames = 0xec,
	SingleColFrames = 0xf0,
	BcstFramesXmtdOk = 0xf6,
	CarrierSenseErrors = 0xf8,
	MacControlFramesXmtd = 0xfa,
	FramesAbortXSColls = 0xfc,
	FramesWEXDeferal = 0xfe,
	
	EtherStatsCollisions = 0x100,
	EtherStatsOctetsTransmit = 0x104,
	EtherStatsPktsTransmit = 0x108,
	EtherStatsPkts64OctetTransmit = 0x10c,
	EtherStats65to127OctetsTransmit = 0x110,
	EtherStatsPkts128to255OctetsTransmit = 0x114,
	EtherStatsPkts256to511OctetsTransmit = 0x118,
	EtherStatsPkts512to1023OctetsTransmit = 0x11c,
	EtherStatsPkts1024to1518OctetsTransmit = 0x120,
	EtherStatsCRCAlignErrors = 0x124,
	EtherStatsUndersizePkts = 0x128,
	EtherStatsFragments = 0x12c,
	EtherStatsJabbers = 0x130,
	EtherStatsOctets = 0x134,
	EtherStatsPkts = 0x138,
	EtherStats64Octets = 0x13c,
	EtherStatsPkts65to127Octets = 0x140,
	EtherStatsPkts128to255Octets = 0x144,
	EtherStatsPkts256to511Octets = 0x148,
	EtherStatsPkts512to1023Octets = 0x14c,
	EtherStatsPkts1024to1518Octets = 0x150,
};

enum IntStatus_bits {
	InterruptStatus = 0x0001,
	HostError = 0x0002,
	MACCtrlFrame = 0x0008,
	TxComplete = 0x0004,
	RxComplete = 0x0010,
	RxEarly = 0x0020,
	IntRequested = 0x0040,
	UpdateStats = 0x0080,
	LinkEvent = 0x0100,
	TxDMAComplete = 0x0200,
	RxDMAComplete = 0x0400,
	RFDListEnd = 0x0800,
	RxDMAPriority = 0x1000,
};

enum ReceiveMode_bits {
	ReceiveUnicast = 0x0001,
	ReceiveMulticast = 0x0002,
	ReceiveBroadcast = 0x0004,
	ReceiveAllFrames = 0x0008,
	ReceiveMulticastHash = 0x0010,
	ReceiveIPMulticast = 0x0020,
	ReceiveVLANMatch = 0x0100,
	ReceiveVLANHash = 0x0200,
};
enum MACCtrl_bits {
	DuplexSelect = 0x20,
	TxFlowControlEnable = 0x80,
	RxFlowControlEnable = 0x0100,
	RcvFCS = 0x200,
	AutoVLANtagging = 0x1000,
	AutoVLANuntagging = 0x2000,
	StatsEnable = 0x00200000,
	StatsDisable = 0x00400000,
	StatsEnabled = 0x00800000,
	TxEnable = 0x01000000,
	TxDisable = 0x02000000,
	TxEnabled = 0x04000000,
	RxEnable = 0x08000000,
	RxDisable = 0x10000000,
	RxEnabled = 0x20000000,
};

enum ASICCtrl_LoWord_bits {
	PhyMedia = 0x0080,
};

enum ASICCtrl_HiWord_bits {
	GlobalReset = 0x0001,
	RxReset = 0x0002,
	TxReset = 0x0004,
	DMAReset = 0x0008,
	FIFOReset = 0x0010,
	NetworkReset = 0x0020,
	HostReset = 0x0040,
	ResetBusy = 0x0400,
};

enum TFC_bits {
	DwordAlign = 0x00000000,
	WordAlignDisable = 0x00030000,
	WordAlign = 0x00020000,
	TCPChecksumEnable = 0x00040000,
	UDPChecksumEnable = 0x00080000,
	IPChecksumEnable = 0x00100000,
	FCSAppendDisable = 0x00200000,
	TxIndicate = 0x00400000,
	TxDMAIndicate = 0x00800000,
	FragCountShift = 24,
	VLANTagInsert = 0x0000000010000000,
	TFDDone = 0x80000000,
	VIDShift = 32,
	UsePriorityShift = 48,
};

enum RFS_bits {
	RxFIFOOverrun = 0x00010000,
	RxRuntFrame = 0x00020000,
	RxAlignmentError = 0x00040000,
	RxFCSError = 0x00080000,
	RxOverSizedFrame = 0x00100000,
	RxLengthError = 0x00200000,
	VLANDetected = 0x00400000,
	TCPDetected = 0x00800000,
	TCPError = 0x01000000,
	UDPDetected = 0x02000000,
	UDPError = 0x04000000,
	IPDetected = 0x08000000,
	IPError = 0x10000000,
	FrameStart = 0x20000000,
	FrameEnd = 0x40000000,
	RFDDone = 0x80000000,
	TCIShift = 32,
	RFS_Errors = 0x003f0000,
};

#define MII_RESET_TIME_OUT		10000
enum _mii_reg {
	MII_PHY_SCR = 16,
};

enum _pcs_reg {
	PCS_BMCR = 0,
	PCS_BMSR = 1,
	PCS_ANAR = 4,
	PCS_ANLPAR = 5,
	PCS_ANER = 6,
	PCS_ANNPT = 7,
	PCS_ANLPRNP = 8,
	PCS_ESR = 15,
};

enum _mii_esr {
	MII_ESR_1000BX_FD = 0x8000,
	MII_ESR_1000BX_HD = 0x4000,
	MII_ESR_1000BT_FD = 0x2000,
	MII_ESR_1000BT_HD = 0x1000,
};
#if 0
typedef union t_MII_PHY_SCR {
	u16 image;
	struct {
		u16 disable_jabber:1;	
		u16 polarity_reversal:1;	
		u16 SEQ_test:1;	
		u16 _bit_3:1;	
		u16 disable_CLK125:1;	
		u16 mdi_crossover_mode:2;	
		u16 enable_ext_dist:1;	
		u16 _bit_8_9:2;	
		u16 force_link:1;	
		u16 assert_CRS:1;	
		u16 rcv_fifo_depth:2;	
		u16 xmit_fifo_depth:2;	
	} bits;
} PHY_SCR_t, *PPHY_SCR_t;
#endif

typedef enum t_MII_ADMIN_STATUS {
	adm_reset,
	adm_operational,
	adm_loopback,
	adm_power_down,
	adm_isolate
} MII_ADMIN_t, *PMII_ADMIN_t;

enum _pcs_anar {
	PCS_ANAR_NEXT_PAGE = 0x8000,
	PCS_ANAR_REMOTE_FAULT = 0x3000,
	PCS_ANAR_ASYMMETRIC = 0x0100,
	PCS_ANAR_PAUSE = 0x0080,
	PCS_ANAR_HALF_DUPLEX = 0x0040,
	PCS_ANAR_FULL_DUPLEX = 0x0020,
};
enum _pcs_anlpar {
	PCS_ANLPAR_NEXT_PAGE = PCS_ANAR_NEXT_PAGE,
	PCS_ANLPAR_REMOTE_FAULT = PCS_ANAR_REMOTE_FAULT,
	PCS_ANLPAR_ASYMMETRIC = PCS_ANAR_ASYMMETRIC,
	PCS_ANLPAR_PAUSE = PCS_ANAR_PAUSE,
	PCS_ANLPAR_HALF_DUPLEX = PCS_ANAR_HALF_DUPLEX,
	PCS_ANLPAR_FULL_DUPLEX = PCS_ANAR_FULL_DUPLEX,
};

typedef struct t_SROM {
	u16 config_param;	
	u16 asic_ctrl;		
	u16 sub_vendor_id;	
	u16 sub_system_id;	
	u16 reserved1[12];	
	u8 mac_addr[6];		
	u8 reserved2[10];	
	u8 sib[204];		
	u32 crc;		
} SROM_t, *PSROM_t;

struct ioctl_data {
	char signature[10];
	int cmd;
	int len;
	char *data;
};

struct netdev_desc {
	__le64 next_desc;
	__le64 status;
	__le64 fraginfo;
};

#define PRIV_ALIGN	15	
struct netdev_private {
	
	struct netdev_desc *rx_ring;
	struct netdev_desc *tx_ring;
	struct sk_buff *rx_skbuff[RX_RING_SIZE];
	struct sk_buff *tx_skbuff[TX_RING_SIZE];
	dma_addr_t tx_ring_dma;
	dma_addr_t rx_ring_dma;
	struct pci_dev *pdev;
	spinlock_t tx_lock;
	spinlock_t rx_lock;
	struct net_device_stats stats;
	unsigned int rx_buf_sz;		
	unsigned int speed;		
	unsigned int vlan;		
	unsigned int chip_id;		
	unsigned int rx_coalesce; 	
	unsigned int rx_timeout; 	
	unsigned int tx_coalesce;	
	unsigned int full_duplex:1;	
	unsigned int an_enable:2;	
	unsigned int jumbo:1;		
	unsigned int coalesce:1;	
	unsigned int tx_flow:1;		
	unsigned int rx_flow:1;		
	unsigned int phy_media:1;	
	unsigned int link_status:1;	
	struct netdev_desc *last_tx;	
	unsigned long cur_rx, old_rx;	
	unsigned long cur_tx, old_tx;
	struct timer_list timer;
	int wake_polarity;
	char name[256];		
	u8 duplex_polarity;
	u16 mcast_filter[4];
	u16 advertising;	
	u16 negotiate;		
	int phy_addr;		
};


static DEFINE_PCI_DEVICE_TABLE(rio_pci_tbl) = {
	{0x1186, 0x4000, PCI_ANY_ID, PCI_ANY_ID, },
	{0x13f0, 0x1021, PCI_ANY_ID, PCI_ANY_ID, },
	{ }
};
MODULE_DEVICE_TABLE (pci, rio_pci_tbl);
#define TX_TIMEOUT  (4*HZ)
#define PACKET_SIZE		1536
#define MAX_JUMBO		8000
#define RIO_IO_SIZE             340
#define DEFAULT_RXC		5
#define DEFAULT_RXT		750
#define DEFAULT_TXC		1
#define MAX_TXC			8
#endif				
