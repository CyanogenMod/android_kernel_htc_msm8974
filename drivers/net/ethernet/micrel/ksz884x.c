/**
 * drivers/net/ethernet/micrel/ksx884x.c - Micrel KSZ8841/2 PCI Ethernet driver
 *
 * Copyright (c) 2009-2010 Micrel, Inc.
 * 	Tristram Ha <Tristram.Ha@micrel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/mii.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/sched.h>
#include <linux/slab.h>



#define KS_DMA_TX_CTRL			0x0000
#define DMA_TX_ENABLE			0x00000001
#define DMA_TX_CRC_ENABLE		0x00000002
#define DMA_TX_PAD_ENABLE		0x00000004
#define DMA_TX_LOOPBACK			0x00000100
#define DMA_TX_FLOW_ENABLE		0x00000200
#define DMA_TX_CSUM_IP			0x00010000
#define DMA_TX_CSUM_TCP			0x00020000
#define DMA_TX_CSUM_UDP			0x00040000
#define DMA_TX_BURST_SIZE		0x3F000000

#define KS_DMA_RX_CTRL			0x0004
#define DMA_RX_ENABLE			0x00000001
#define KS884X_DMA_RX_MULTICAST		0x00000002
#define DMA_RX_PROMISCUOUS		0x00000004
#define DMA_RX_ERROR			0x00000008
#define DMA_RX_UNICAST			0x00000010
#define DMA_RX_ALL_MULTICAST		0x00000020
#define DMA_RX_BROADCAST		0x00000040
#define DMA_RX_FLOW_ENABLE		0x00000200
#define DMA_RX_CSUM_IP			0x00010000
#define DMA_RX_CSUM_TCP			0x00020000
#define DMA_RX_CSUM_UDP			0x00040000
#define DMA_RX_BURST_SIZE		0x3F000000

#define DMA_BURST_SHIFT			24
#define DMA_BURST_DEFAULT		8

#define KS_DMA_TX_START			0x0008
#define KS_DMA_RX_START			0x000C
#define DMA_START			0x00000001

#define KS_DMA_TX_ADDR			0x0010
#define KS_DMA_RX_ADDR			0x0014

#define DMA_ADDR_LIST_MASK		0xFFFFFFFC
#define DMA_ADDR_LIST_SHIFT		2

#define KS884X_MULTICAST_0_OFFSET	0x0020
#define KS884X_MULTICAST_1_OFFSET	0x0021
#define KS884X_MULTICAST_2_OFFSET	0x0022
#define KS884x_MULTICAST_3_OFFSET	0x0023
#define KS884X_MULTICAST_4_OFFSET	0x0024
#define KS884X_MULTICAST_5_OFFSET	0x0025
#define KS884X_MULTICAST_6_OFFSET	0x0026
#define KS884X_MULTICAST_7_OFFSET	0x0027


#define KS884X_INTERRUPTS_ENABLE	0x0028
#define KS884X_INTERRUPTS_STATUS	0x002C

#define KS884X_INT_RX_STOPPED		0x02000000
#define KS884X_INT_TX_STOPPED		0x04000000
#define KS884X_INT_RX_OVERRUN		0x08000000
#define KS884X_INT_TX_EMPTY		0x10000000
#define KS884X_INT_RX			0x20000000
#define KS884X_INT_TX			0x40000000
#define KS884X_INT_PHY			0x80000000

#define KS884X_INT_RX_MASK		\
	(KS884X_INT_RX | KS884X_INT_RX_OVERRUN)
#define KS884X_INT_TX_MASK		\
	(KS884X_INT_TX | KS884X_INT_TX_EMPTY)
#define KS884X_INT_MASK	(KS884X_INT_RX | KS884X_INT_TX | KS884X_INT_PHY)


#define KS_ADD_ADDR_0_LO		0x0080
#define KS_ADD_ADDR_0_HI		0x0084
#define KS_ADD_ADDR_1_LO		0x0088
#define KS_ADD_ADDR_1_HI		0x008C
#define KS_ADD_ADDR_2_LO		0x0090
#define KS_ADD_ADDR_2_HI		0x0094
#define KS_ADD_ADDR_3_LO		0x0098
#define KS_ADD_ADDR_3_HI		0x009C
#define KS_ADD_ADDR_4_LO		0x00A0
#define KS_ADD_ADDR_4_HI		0x00A4
#define KS_ADD_ADDR_5_LO		0x00A8
#define KS_ADD_ADDR_5_HI		0x00AC
#define KS_ADD_ADDR_6_LO		0x00B0
#define KS_ADD_ADDR_6_HI		0x00B4
#define KS_ADD_ADDR_7_LO		0x00B8
#define KS_ADD_ADDR_7_HI		0x00BC
#define KS_ADD_ADDR_8_LO		0x00C0
#define KS_ADD_ADDR_8_HI		0x00C4
#define KS_ADD_ADDR_9_LO		0x00C8
#define KS_ADD_ADDR_9_HI		0x00CC
#define KS_ADD_ADDR_A_LO		0x00D0
#define KS_ADD_ADDR_A_HI		0x00D4
#define KS_ADD_ADDR_B_LO		0x00D8
#define KS_ADD_ADDR_B_HI		0x00DC
#define KS_ADD_ADDR_C_LO		0x00E0
#define KS_ADD_ADDR_C_HI		0x00E4
#define KS_ADD_ADDR_D_LO		0x00E8
#define KS_ADD_ADDR_D_HI		0x00EC
#define KS_ADD_ADDR_E_LO		0x00F0
#define KS_ADD_ADDR_E_HI		0x00F4
#define KS_ADD_ADDR_F_LO		0x00F8
#define KS_ADD_ADDR_F_HI		0x00FC

#define ADD_ADDR_HI_MASK		0x0000FFFF
#define ADD_ADDR_ENABLE			0x80000000
#define ADD_ADDR_INCR			8


#define KS884X_ADDR_0_OFFSET		0x0200
#define KS884X_ADDR_1_OFFSET		0x0201
#define KS884X_ADDR_2_OFFSET		0x0202
#define KS884X_ADDR_3_OFFSET		0x0203
#define KS884X_ADDR_4_OFFSET		0x0204
#define KS884X_ADDR_5_OFFSET		0x0205

#define KS884X_BUS_CTRL_OFFSET		0x0210

#define BUS_SPEED_125_MHZ		0x0000
#define BUS_SPEED_62_5_MHZ		0x0001
#define BUS_SPEED_41_66_MHZ		0x0002
#define BUS_SPEED_25_MHZ		0x0003

#define KS884X_EEPROM_CTRL_OFFSET	0x0212

#define EEPROM_CHIP_SELECT		0x0001
#define EEPROM_SERIAL_CLOCK		0x0002
#define EEPROM_DATA_OUT			0x0004
#define EEPROM_DATA_IN			0x0008
#define EEPROM_ACCESS_ENABLE		0x0010

#define KS884X_MEM_INFO_OFFSET		0x0214

#define RX_MEM_TEST_FAILED		0x0008
#define RX_MEM_TEST_FINISHED		0x0010
#define TX_MEM_TEST_FAILED		0x0800
#define TX_MEM_TEST_FINISHED		0x1000

#define KS884X_GLOBAL_CTRL_OFFSET	0x0216
#define GLOBAL_SOFTWARE_RESET		0x0001

#define KS8841_POWER_MANAGE_OFFSET	0x0218

#define KS8841_WOL_CTRL_OFFSET		0x021A
#define KS8841_WOL_MAGIC_ENABLE		0x0080
#define KS8841_WOL_FRAME3_ENABLE	0x0008
#define KS8841_WOL_FRAME2_ENABLE	0x0004
#define KS8841_WOL_FRAME1_ENABLE	0x0002
#define KS8841_WOL_FRAME0_ENABLE	0x0001

#define KS8841_WOL_FRAME_CRC_OFFSET	0x0220
#define KS8841_WOL_FRAME_BYTE0_OFFSET	0x0224
#define KS8841_WOL_FRAME_BYTE2_OFFSET	0x0228

#define KS884X_IACR_P			0x04A0
#define KS884X_IACR_OFFSET		KS884X_IACR_P

#define KS884X_IADR1_P			0x04A2
#define KS884X_IADR2_P			0x04A4
#define KS884X_IADR3_P			0x04A6
#define KS884X_IADR4_P			0x04A8
#define KS884X_IADR5_P			0x04AA

#define KS884X_ACC_CTRL_SEL_OFFSET	KS884X_IACR_P
#define KS884X_ACC_CTRL_INDEX_OFFSET	(KS884X_ACC_CTRL_SEL_OFFSET + 1)

#define KS884X_ACC_DATA_0_OFFSET	KS884X_IADR4_P
#define KS884X_ACC_DATA_1_OFFSET	(KS884X_ACC_DATA_0_OFFSET + 1)
#define KS884X_ACC_DATA_2_OFFSET	KS884X_IADR5_P
#define KS884X_ACC_DATA_3_OFFSET	(KS884X_ACC_DATA_2_OFFSET + 1)
#define KS884X_ACC_DATA_4_OFFSET	KS884X_IADR2_P
#define KS884X_ACC_DATA_5_OFFSET	(KS884X_ACC_DATA_4_OFFSET + 1)
#define KS884X_ACC_DATA_6_OFFSET	KS884X_IADR3_P
#define KS884X_ACC_DATA_7_OFFSET	(KS884X_ACC_DATA_6_OFFSET + 1)
#define KS884X_ACC_DATA_8_OFFSET	KS884X_IADR1_P

#define KS884X_P1MBCR_P			0x04D0
#define KS884X_P1MBSR_P			0x04D2
#define KS884X_PHY1ILR_P		0x04D4
#define KS884X_PHY1IHR_P		0x04D6
#define KS884X_P1ANAR_P			0x04D8
#define KS884X_P1ANLPR_P		0x04DA

#define KS884X_P2MBCR_P			0x04E0
#define KS884X_P2MBSR_P			0x04E2
#define KS884X_PHY2ILR_P		0x04E4
#define KS884X_PHY2IHR_P		0x04E6
#define KS884X_P2ANAR_P			0x04E8
#define KS884X_P2ANLPR_P		0x04EA

#define KS884X_PHY_1_CTRL_OFFSET	KS884X_P1MBCR_P
#define PHY_CTRL_INTERVAL		(KS884X_P2MBCR_P - KS884X_P1MBCR_P)

#define KS884X_PHY_CTRL_OFFSET		0x00

#define PHY_REG_CTRL			0

#define PHY_RESET			0x8000
#define PHY_LOOPBACK			0x4000
#define PHY_SPEED_100MBIT		0x2000
#define PHY_AUTO_NEG_ENABLE		0x1000
#define PHY_POWER_DOWN			0x0800
#define PHY_MII_DISABLE			0x0400
#define PHY_AUTO_NEG_RESTART		0x0200
#define PHY_FULL_DUPLEX			0x0100
#define PHY_COLLISION_TEST		0x0080
#define PHY_HP_MDIX			0x0020
#define PHY_FORCE_MDIX			0x0010
#define PHY_AUTO_MDIX_DISABLE		0x0008
#define PHY_REMOTE_FAULT_DISABLE	0x0004
#define PHY_TRANSMIT_DISABLE		0x0002
#define PHY_LED_DISABLE			0x0001

#define KS884X_PHY_STATUS_OFFSET	0x02

#define PHY_REG_STATUS			1

#define PHY_100BT4_CAPABLE		0x8000
#define PHY_100BTX_FD_CAPABLE		0x4000
#define PHY_100BTX_CAPABLE		0x2000
#define PHY_10BT_FD_CAPABLE		0x1000
#define PHY_10BT_CAPABLE		0x0800
#define PHY_MII_SUPPRESS_CAPABLE	0x0040
#define PHY_AUTO_NEG_ACKNOWLEDGE	0x0020
#define PHY_REMOTE_FAULT		0x0010
#define PHY_AUTO_NEG_CAPABLE		0x0008
#define PHY_LINK_STATUS			0x0004
#define PHY_JABBER_DETECT		0x0002
#define PHY_EXTENDED_CAPABILITY		0x0001

#define KS884X_PHY_ID_1_OFFSET		0x04
#define KS884X_PHY_ID_2_OFFSET		0x06

#define PHY_REG_ID_1			2
#define PHY_REG_ID_2			3

#define KS884X_PHY_AUTO_NEG_OFFSET	0x08

#define PHY_REG_AUTO_NEGOTIATION	4

#define PHY_AUTO_NEG_NEXT_PAGE		0x8000
#define PHY_AUTO_NEG_REMOTE_FAULT	0x2000
#define PHY_AUTO_NEG_ASYM_PAUSE		0x0800
#define PHY_AUTO_NEG_SYM_PAUSE		0x0400
#define PHY_AUTO_NEG_100BT4		0x0200
#define PHY_AUTO_NEG_100BTX_FD		0x0100
#define PHY_AUTO_NEG_100BTX		0x0080
#define PHY_AUTO_NEG_10BT_FD		0x0040
#define PHY_AUTO_NEG_10BT		0x0020
#define PHY_AUTO_NEG_SELECTOR		0x001F
#define PHY_AUTO_NEG_802_3		0x0001

#define PHY_AUTO_NEG_PAUSE  (PHY_AUTO_NEG_SYM_PAUSE | PHY_AUTO_NEG_ASYM_PAUSE)

#define KS884X_PHY_REMOTE_CAP_OFFSET	0x0A

#define PHY_REG_REMOTE_CAPABILITY	5

#define PHY_REMOTE_NEXT_PAGE		0x8000
#define PHY_REMOTE_ACKNOWLEDGE		0x4000
#define PHY_REMOTE_REMOTE_FAULT		0x2000
#define PHY_REMOTE_SYM_PAUSE		0x0400
#define PHY_REMOTE_100BTX_FD		0x0100
#define PHY_REMOTE_100BTX		0x0080
#define PHY_REMOTE_10BT_FD		0x0040
#define PHY_REMOTE_10BT			0x0020

#define KS884X_P1VCT_P			0x04F0
#define KS884X_P1PHYCTRL_P		0x04F2

#define KS884X_P2VCT_P			0x04F4
#define KS884X_P2PHYCTRL_P		0x04F6

#define KS884X_PHY_SPECIAL_OFFSET	KS884X_P1VCT_P
#define PHY_SPECIAL_INTERVAL		(KS884X_P2VCT_P - KS884X_P1VCT_P)

#define KS884X_PHY_LINK_MD_OFFSET	0x00

#define PHY_START_CABLE_DIAG		0x8000
#define PHY_CABLE_DIAG_RESULT		0x6000
#define PHY_CABLE_STAT_NORMAL		0x0000
#define PHY_CABLE_STAT_OPEN		0x2000
#define PHY_CABLE_STAT_SHORT		0x4000
#define PHY_CABLE_STAT_FAILED		0x6000
#define PHY_CABLE_10M_SHORT		0x1000
#define PHY_CABLE_FAULT_COUNTER		0x01FF

#define KS884X_PHY_PHY_CTRL_OFFSET	0x02

#define PHY_STAT_REVERSED_POLARITY	0x0020
#define PHY_STAT_MDIX			0x0010
#define PHY_FORCE_LINK			0x0008
#define PHY_POWER_SAVING_DISABLE	0x0004
#define PHY_REMOTE_LOOPBACK		0x0002

#define KS884X_SIDER_P			0x0400
#define KS884X_CHIP_ID_OFFSET		KS884X_SIDER_P
#define KS884X_FAMILY_ID_OFFSET		(KS884X_CHIP_ID_OFFSET + 1)

#define REG_FAMILY_ID			0x88

#define REG_CHIP_ID_41			0x8810
#define REG_CHIP_ID_42			0x8800

#define KS884X_CHIP_ID_MASK_41		0xFF10
#define KS884X_CHIP_ID_MASK		0xFFF0
#define KS884X_CHIP_ID_SHIFT		4
#define KS884X_REVISION_MASK		0x000E
#define KS884X_REVISION_SHIFT		1
#define KS8842_START			0x0001

#define CHIP_IP_41_M			0x8810
#define CHIP_IP_42_M			0x8800
#define CHIP_IP_61_M			0x8890
#define CHIP_IP_62_M			0x8880

#define CHIP_IP_41_P			0x8850
#define CHIP_IP_42_P			0x8840
#define CHIP_IP_61_P			0x88D0
#define CHIP_IP_62_P			0x88C0

#define KS8842_SGCR1_P			0x0402
#define KS8842_SWITCH_CTRL_1_OFFSET	KS8842_SGCR1_P

#define SWITCH_PASS_ALL			0x8000
#define SWITCH_TX_FLOW_CTRL		0x2000
#define SWITCH_RX_FLOW_CTRL		0x1000
#define SWITCH_CHECK_LENGTH		0x0800
#define SWITCH_AGING_ENABLE		0x0400
#define SWITCH_FAST_AGING		0x0200
#define SWITCH_AGGR_BACKOFF		0x0100
#define SWITCH_PASS_PAUSE		0x0008
#define SWITCH_LINK_AUTO_AGING		0x0001

#define KS8842_SGCR2_P			0x0404
#define KS8842_SWITCH_CTRL_2_OFFSET	KS8842_SGCR2_P

#define SWITCH_VLAN_ENABLE		0x8000
#define SWITCH_IGMP_SNOOP		0x4000
#define IPV6_MLD_SNOOP_ENABLE		0x2000
#define IPV6_MLD_SNOOP_OPTION		0x1000
#define PRIORITY_SCHEME_SELECT		0x0800
#define SWITCH_MIRROR_RX_TX		0x0100
#define UNICAST_VLAN_BOUNDARY		0x0080
#define MULTICAST_STORM_DISABLE		0x0040
#define SWITCH_BACK_PRESSURE		0x0020
#define FAIR_FLOW_CTRL			0x0010
#define NO_EXC_COLLISION_DROP		0x0008
#define SWITCH_HUGE_PACKET		0x0004
#define SWITCH_LEGAL_PACKET		0x0002
#define SWITCH_BUF_RESERVE		0x0001

#define KS8842_SGCR3_P			0x0406
#define KS8842_SWITCH_CTRL_3_OFFSET	KS8842_SGCR3_P

#define BROADCAST_STORM_RATE_LO		0xFF00
#define SWITCH_REPEATER			0x0080
#define SWITCH_HALF_DUPLEX		0x0040
#define SWITCH_FLOW_CTRL		0x0020
#define SWITCH_10_MBIT			0x0010
#define SWITCH_REPLACE_NULL_VID		0x0008
#define BROADCAST_STORM_RATE_HI		0x0007

#define BROADCAST_STORM_RATE		0x07FF

#define KS8842_SGCR4_P			0x0408

#define KS8842_SGCR5_P			0x040A
#define KS8842_SWITCH_CTRL_5_OFFSET	KS8842_SGCR5_P

#define LED_MODE			0x8200
#define LED_SPEED_DUPLEX_ACT		0x0000
#define LED_SPEED_DUPLEX_LINK_ACT	0x8000
#define LED_DUPLEX_10_100		0x0200

#define KS8842_SGCR6_P			0x0410
#define KS8842_SWITCH_CTRL_6_OFFSET	KS8842_SGCR6_P

#define KS8842_PRIORITY_MASK		3
#define KS8842_PRIORITY_SHIFT		2

#define KS8842_SGCR7_P			0x0412
#define KS8842_SWITCH_CTRL_7_OFFSET	KS8842_SGCR7_P

#define SWITCH_UNK_DEF_PORT_ENABLE	0x0008
#define SWITCH_UNK_DEF_PORT_3		0x0004
#define SWITCH_UNK_DEF_PORT_2		0x0002
#define SWITCH_UNK_DEF_PORT_1		0x0001

#define KS8842_MACAR1_P			0x0470
#define KS8842_MACAR2_P			0x0472
#define KS8842_MACAR3_P			0x0474
#define KS8842_MAC_ADDR_1_OFFSET	KS8842_MACAR1_P
#define KS8842_MAC_ADDR_0_OFFSET	(KS8842_MAC_ADDR_1_OFFSET + 1)
#define KS8842_MAC_ADDR_3_OFFSET	KS8842_MACAR2_P
#define KS8842_MAC_ADDR_2_OFFSET	(KS8842_MAC_ADDR_3_OFFSET + 1)
#define KS8842_MAC_ADDR_5_OFFSET	KS8842_MACAR3_P
#define KS8842_MAC_ADDR_4_OFFSET	(KS8842_MAC_ADDR_5_OFFSET + 1)

#define KS8842_TOSR1_P			0x0480
#define KS8842_TOSR2_P			0x0482
#define KS8842_TOSR3_P			0x0484
#define KS8842_TOSR4_P			0x0486
#define KS8842_TOSR5_P			0x0488
#define KS8842_TOSR6_P			0x048A
#define KS8842_TOSR7_P			0x0490
#define KS8842_TOSR8_P			0x0492
#define KS8842_TOS_1_OFFSET		KS8842_TOSR1_P
#define KS8842_TOS_2_OFFSET		KS8842_TOSR2_P
#define KS8842_TOS_3_OFFSET		KS8842_TOSR3_P
#define KS8842_TOS_4_OFFSET		KS8842_TOSR4_P
#define KS8842_TOS_5_OFFSET		KS8842_TOSR5_P
#define KS8842_TOS_6_OFFSET		KS8842_TOSR6_P

#define KS8842_TOS_7_OFFSET		KS8842_TOSR7_P
#define KS8842_TOS_8_OFFSET		KS8842_TOSR8_P

#define KS8842_P1CR1_P			0x0500
#define KS8842_P1CR2_P			0x0502
#define KS8842_P1VIDR_P			0x0504
#define KS8842_P1CR3_P			0x0506
#define KS8842_P1IRCR_P			0x0508
#define KS8842_P1ERCR_P			0x050A
#define KS884X_P1SCSLMD_P		0x0510
#define KS884X_P1CR4_P			0x0512
#define KS884X_P1SR_P			0x0514

#define KS8842_P2CR1_P			0x0520
#define KS8842_P2CR2_P			0x0522
#define KS8842_P2VIDR_P			0x0524
#define KS8842_P2CR3_P			0x0526
#define KS8842_P2IRCR_P			0x0528
#define KS8842_P2ERCR_P			0x052A
#define KS884X_P2SCSLMD_P		0x0530
#define KS884X_P2CR4_P			0x0532
#define KS884X_P2SR_P			0x0534

#define KS8842_P3CR1_P			0x0540
#define KS8842_P3CR2_P			0x0542
#define KS8842_P3VIDR_P			0x0544
#define KS8842_P3CR3_P			0x0546
#define KS8842_P3IRCR_P			0x0548
#define KS8842_P3ERCR_P			0x054A

#define KS8842_PORT_1_CTRL_1		KS8842_P1CR1_P
#define KS8842_PORT_2_CTRL_1		KS8842_P2CR1_P
#define KS8842_PORT_3_CTRL_1		KS8842_P3CR1_P

#define PORT_CTRL_ADDR(port, addr)		\
	(addr = KS8842_PORT_1_CTRL_1 + (port) *	\
		(KS8842_PORT_2_CTRL_1 - KS8842_PORT_1_CTRL_1))

#define KS8842_PORT_CTRL_1_OFFSET	0x00

#define PORT_BROADCAST_STORM		0x0080
#define PORT_DIFFSERV_ENABLE		0x0040
#define PORT_802_1P_ENABLE		0x0020
#define PORT_BASED_PRIORITY_MASK	0x0018
#define PORT_BASED_PRIORITY_BASE	0x0003
#define PORT_BASED_PRIORITY_SHIFT	3
#define PORT_BASED_PRIORITY_0		0x0000
#define PORT_BASED_PRIORITY_1		0x0008
#define PORT_BASED_PRIORITY_2		0x0010
#define PORT_BASED_PRIORITY_3		0x0018
#define PORT_INSERT_TAG			0x0004
#define PORT_REMOVE_TAG			0x0002
#define PORT_PRIO_QUEUE_ENABLE		0x0001

#define KS8842_PORT_CTRL_2_OFFSET	0x02

#define PORT_INGRESS_VLAN_FILTER	0x4000
#define PORT_DISCARD_NON_VID		0x2000
#define PORT_FORCE_FLOW_CTRL		0x1000
#define PORT_BACK_PRESSURE		0x0800
#define PORT_TX_ENABLE			0x0400
#define PORT_RX_ENABLE			0x0200
#define PORT_LEARN_DISABLE		0x0100
#define PORT_MIRROR_SNIFFER		0x0080
#define PORT_MIRROR_RX			0x0040
#define PORT_MIRROR_TX			0x0020
#define PORT_USER_PRIORITY_CEILING	0x0008
#define PORT_VLAN_MEMBERSHIP		0x0007

#define KS8842_PORT_CTRL_VID_OFFSET	0x04

#define PORT_DEFAULT_VID		0x0001

#define KS8842_PORT_CTRL_3_OFFSET	0x06

#define PORT_INGRESS_LIMIT_MODE		0x000C
#define PORT_INGRESS_ALL		0x0000
#define PORT_INGRESS_UNICAST		0x0004
#define PORT_INGRESS_MULTICAST		0x0008
#define PORT_INGRESS_BROADCAST		0x000C
#define PORT_COUNT_IFG			0x0002
#define PORT_COUNT_PREAMBLE		0x0001

#define KS8842_PORT_IN_RATE_OFFSET	0x08
#define KS8842_PORT_OUT_RATE_OFFSET	0x0A

#define PORT_PRIORITY_RATE		0x0F
#define PORT_PRIORITY_RATE_SHIFT	4

#define KS884X_PORT_LINK_MD		0x10

#define PORT_CABLE_10M_SHORT		0x8000
#define PORT_CABLE_DIAG_RESULT		0x6000
#define PORT_CABLE_STAT_NORMAL		0x0000
#define PORT_CABLE_STAT_OPEN		0x2000
#define PORT_CABLE_STAT_SHORT		0x4000
#define PORT_CABLE_STAT_FAILED		0x6000
#define PORT_START_CABLE_DIAG		0x1000
#define PORT_FORCE_LINK			0x0800
#define PORT_POWER_SAVING_DISABLE	0x0400
#define PORT_PHY_REMOTE_LOOPBACK	0x0200
#define PORT_CABLE_FAULT_COUNTER	0x01FF

#define KS884X_PORT_CTRL_4_OFFSET	0x12

#define PORT_LED_OFF			0x8000
#define PORT_TX_DISABLE			0x4000
#define PORT_AUTO_NEG_RESTART		0x2000
#define PORT_REMOTE_FAULT_DISABLE	0x1000
#define PORT_POWER_DOWN			0x0800
#define PORT_AUTO_MDIX_DISABLE		0x0400
#define PORT_FORCE_MDIX			0x0200
#define PORT_LOOPBACK			0x0100
#define PORT_AUTO_NEG_ENABLE		0x0080
#define PORT_FORCE_100_MBIT		0x0040
#define PORT_FORCE_FULL_DUPLEX		0x0020
#define PORT_AUTO_NEG_SYM_PAUSE		0x0010
#define PORT_AUTO_NEG_100BTX_FD		0x0008
#define PORT_AUTO_NEG_100BTX		0x0004
#define PORT_AUTO_NEG_10BT_FD		0x0002
#define PORT_AUTO_NEG_10BT		0x0001

#define KS884X_PORT_STATUS_OFFSET	0x14

#define PORT_HP_MDIX			0x8000
#define PORT_REVERSED_POLARITY		0x2000
#define PORT_RX_FLOW_CTRL		0x0800
#define PORT_TX_FLOW_CTRL		0x1000
#define PORT_STATUS_SPEED_100MBIT	0x0400
#define PORT_STATUS_FULL_DUPLEX		0x0200
#define PORT_REMOTE_FAULT		0x0100
#define PORT_MDIX_STATUS		0x0080
#define PORT_AUTO_NEG_COMPLETE		0x0040
#define PORT_STATUS_LINK_GOOD		0x0020
#define PORT_REMOTE_SYM_PAUSE		0x0010
#define PORT_REMOTE_100BTX_FD		0x0008
#define PORT_REMOTE_100BTX		0x0004
#define PORT_REMOTE_10BT_FD		0x0002
#define PORT_REMOTE_10BT		0x0001


#define STATIC_MAC_TABLE_ADDR		0x0000FFFF
#define STATIC_MAC_TABLE_FWD_PORTS	0x00070000
#define STATIC_MAC_TABLE_VALID		0x00080000
#define STATIC_MAC_TABLE_OVERRIDE	0x00100000
#define STATIC_MAC_TABLE_USE_FID	0x00200000
#define STATIC_MAC_TABLE_FID		0x03C00000

#define STATIC_MAC_FWD_PORTS_SHIFT	16
#define STATIC_MAC_FID_SHIFT		22


#define VLAN_TABLE_VID			0x00000FFF
#define VLAN_TABLE_FID			0x0000F000
#define VLAN_TABLE_MEMBERSHIP		0x00070000
#define VLAN_TABLE_VALID		0x00080000

#define VLAN_TABLE_FID_SHIFT		12
#define VLAN_TABLE_MEMBERSHIP_SHIFT	16


#define DYNAMIC_MAC_TABLE_ADDR		0x0000FFFF
#define DYNAMIC_MAC_TABLE_FID		0x000F0000
#define DYNAMIC_MAC_TABLE_SRC_PORT	0x00300000
#define DYNAMIC_MAC_TABLE_TIMESTAMP	0x00C00000
#define DYNAMIC_MAC_TABLE_ENTRIES	0xFF000000

#define DYNAMIC_MAC_TABLE_ENTRIES_H	0x03
#define DYNAMIC_MAC_TABLE_MAC_EMPTY	0x04
#define DYNAMIC_MAC_TABLE_RESERVED	0x78
#define DYNAMIC_MAC_TABLE_NOT_READY	0x80

#define DYNAMIC_MAC_FID_SHIFT		16
#define DYNAMIC_MAC_SRC_PORT_SHIFT	20
#define DYNAMIC_MAC_TIMESTAMP_SHIFT	22
#define DYNAMIC_MAC_ENTRIES_SHIFT	24
#define DYNAMIC_MAC_ENTRIES_H_SHIFT	8


#define MIB_COUNTER_VALUE		0x3FFFFFFF
#define MIB_COUNTER_VALID		0x40000000
#define MIB_COUNTER_OVERFLOW		0x80000000

#define MIB_PACKET_DROPPED		0x0000FFFF

#define KS_MIB_PACKET_DROPPED_TX_0	0x100
#define KS_MIB_PACKET_DROPPED_TX_1	0x101
#define KS_MIB_PACKET_DROPPED_TX	0x102
#define KS_MIB_PACKET_DROPPED_RX_0	0x103
#define KS_MIB_PACKET_DROPPED_RX_1	0x104
#define KS_MIB_PACKET_DROPPED_RX	0x105

#define SET_DEFAULT_LED			LED_SPEED_DUPLEX_ACT

#define MAC_ADDR_ORDER(i)		(ETH_ALEN - 1 - (i))

#define MAX_ETHERNET_BODY_SIZE		1500
#define ETHERNET_HEADER_SIZE		(14 + VLAN_HLEN)

#define MAX_ETHERNET_PACKET_SIZE	\
	(MAX_ETHERNET_BODY_SIZE + ETHERNET_HEADER_SIZE)

#define REGULAR_RX_BUF_SIZE		(MAX_ETHERNET_PACKET_SIZE + 4)
#define MAX_RX_BUF_SIZE			(1912 + 4)

#define ADDITIONAL_ENTRIES		16
#define MAX_MULTICAST_LIST		32

#define HW_MULTICAST_SIZE		8

#define HW_TO_DEV_PORT(port)		(port - 1)

enum {
	media_connected,
	media_disconnected
};

enum {
	OID_COUNTER_UNKOWN,

	OID_COUNTER_FIRST,

	
	OID_COUNTER_XMIT_ERROR,

	
	OID_COUNTER_RCV_ERROR,

	OID_COUNTER_LAST
};


#define DESC_ALIGNMENT			16
#define BUFFER_ALIGNMENT		8

#define NUM_OF_RX_DESC			64
#define NUM_OF_TX_DESC			64

#define KS_DESC_RX_FRAME_LEN		0x000007FF
#define KS_DESC_RX_FRAME_TYPE		0x00008000
#define KS_DESC_RX_ERROR_CRC		0x00010000
#define KS_DESC_RX_ERROR_RUNT		0x00020000
#define KS_DESC_RX_ERROR_TOO_LONG	0x00040000
#define KS_DESC_RX_ERROR_PHY		0x00080000
#define KS884X_DESC_RX_PORT_MASK	0x00300000
#define KS_DESC_RX_MULTICAST		0x01000000
#define KS_DESC_RX_ERROR		0x02000000
#define KS_DESC_RX_ERROR_CSUM_UDP	0x04000000
#define KS_DESC_RX_ERROR_CSUM_TCP	0x08000000
#define KS_DESC_RX_ERROR_CSUM_IP	0x10000000
#define KS_DESC_RX_LAST			0x20000000
#define KS_DESC_RX_FIRST		0x40000000
#define KS_DESC_RX_ERROR_COND		\
	(KS_DESC_RX_ERROR_CRC |		\
	KS_DESC_RX_ERROR_RUNT |		\
	KS_DESC_RX_ERROR_PHY |		\
	KS_DESC_RX_ERROR_TOO_LONG)

#define KS_DESC_HW_OWNED		0x80000000

#define KS_DESC_BUF_SIZE		0x000007FF
#define KS884X_DESC_TX_PORT_MASK	0x00300000
#define KS_DESC_END_OF_RING		0x02000000
#define KS_DESC_TX_CSUM_GEN_UDP		0x04000000
#define KS_DESC_TX_CSUM_GEN_TCP		0x08000000
#define KS_DESC_TX_CSUM_GEN_IP		0x10000000
#define KS_DESC_TX_LAST			0x20000000
#define KS_DESC_TX_FIRST		0x40000000
#define KS_DESC_TX_INTERRUPT		0x80000000

#define KS_DESC_PORT_SHIFT		20

#define KS_DESC_RX_MASK			(KS_DESC_BUF_SIZE)

#define KS_DESC_TX_MASK			\
	(KS_DESC_TX_INTERRUPT |		\
	KS_DESC_TX_FIRST |		\
	KS_DESC_TX_LAST |		\
	KS_DESC_TX_CSUM_GEN_IP |	\
	KS_DESC_TX_CSUM_GEN_TCP |	\
	KS_DESC_TX_CSUM_GEN_UDP |	\
	KS_DESC_BUF_SIZE)

struct ksz_desc_rx_stat {
#ifdef __BIG_ENDIAN_BITFIELD
	u32 hw_owned:1;
	u32 first_desc:1;
	u32 last_desc:1;
	u32 csum_err_ip:1;
	u32 csum_err_tcp:1;
	u32 csum_err_udp:1;
	u32 error:1;
	u32 multicast:1;
	u32 src_port:4;
	u32 err_phy:1;
	u32 err_too_long:1;
	u32 err_runt:1;
	u32 err_crc:1;
	u32 frame_type:1;
	u32 reserved1:4;
	u32 frame_len:11;
#else
	u32 frame_len:11;
	u32 reserved1:4;
	u32 frame_type:1;
	u32 err_crc:1;
	u32 err_runt:1;
	u32 err_too_long:1;
	u32 err_phy:1;
	u32 src_port:4;
	u32 multicast:1;
	u32 error:1;
	u32 csum_err_udp:1;
	u32 csum_err_tcp:1;
	u32 csum_err_ip:1;
	u32 last_desc:1;
	u32 first_desc:1;
	u32 hw_owned:1;
#endif
};

struct ksz_desc_tx_stat {
#ifdef __BIG_ENDIAN_BITFIELD
	u32 hw_owned:1;
	u32 reserved1:31;
#else
	u32 reserved1:31;
	u32 hw_owned:1;
#endif
};

struct ksz_desc_rx_buf {
#ifdef __BIG_ENDIAN_BITFIELD
	u32 reserved4:6;
	u32 end_of_ring:1;
	u32 reserved3:14;
	u32 buf_size:11;
#else
	u32 buf_size:11;
	u32 reserved3:14;
	u32 end_of_ring:1;
	u32 reserved4:6;
#endif
};

struct ksz_desc_tx_buf {
#ifdef __BIG_ENDIAN_BITFIELD
	u32 intr:1;
	u32 first_seg:1;
	u32 last_seg:1;
	u32 csum_gen_ip:1;
	u32 csum_gen_tcp:1;
	u32 csum_gen_udp:1;
	u32 end_of_ring:1;
	u32 reserved4:1;
	u32 dest_port:4;
	u32 reserved3:9;
	u32 buf_size:11;
#else
	u32 buf_size:11;
	u32 reserved3:9;
	u32 dest_port:4;
	u32 reserved4:1;
	u32 end_of_ring:1;
	u32 csum_gen_udp:1;
	u32 csum_gen_tcp:1;
	u32 csum_gen_ip:1;
	u32 last_seg:1;
	u32 first_seg:1;
	u32 intr:1;
#endif
};

union desc_stat {
	struct ksz_desc_rx_stat rx;
	struct ksz_desc_tx_stat tx;
	u32 data;
};

union desc_buf {
	struct ksz_desc_rx_buf rx;
	struct ksz_desc_tx_buf tx;
	u32 data;
};

struct ksz_hw_desc {
	union desc_stat ctrl;
	union desc_buf buf;
	u32 addr;
	u32 next;
};

struct ksz_sw_desc {
	union desc_stat ctrl;
	union desc_buf buf;
	u32 buf_size;
};

struct ksz_dma_buf {
	struct sk_buff *skb;
	dma_addr_t dma;
	int len;
};

struct ksz_desc {
	struct ksz_hw_desc *phw;
	struct ksz_sw_desc sw;
	struct ksz_dma_buf dma_buf;
};

#define DMA_BUFFER(desc)  ((struct ksz_dma_buf *)(&(desc)->dma_buf))

struct ksz_desc_info {
	struct ksz_desc *ring;
	struct ksz_desc *cur;
	struct ksz_hw_desc *ring_virt;
	u32 ring_phys;
	int size;
	int alloc;
	int avail;
	int last;
	int next;
	int mask;
};


enum {
	TABLE_STATIC_MAC = 0,
	TABLE_VLAN,
	TABLE_DYNAMIC_MAC,
	TABLE_MIB
};

#define LEARNED_MAC_TABLE_ENTRIES	1024
#define STATIC_MAC_TABLE_ENTRIES	8

struct ksz_mac_table {
	u8 mac_addr[ETH_ALEN];
	u16 vid;
	u8 fid;
	u8 ports;
	u8 override:1;
	u8 use_fid:1;
	u8 valid:1;
};

#define VLAN_TABLE_ENTRIES		16

struct ksz_vlan_table {
	u16 vid;
	u8 fid;
	u8 member;
};

#define DIFFSERV_ENTRIES		64
#define PRIO_802_1P_ENTRIES		8
#define PRIO_QUEUES			4

#define SWITCH_PORT_NUM			2
#define TOTAL_PORT_NUM			(SWITCH_PORT_NUM + 1)
#define HOST_MASK			(1 << SWITCH_PORT_NUM)
#define PORT_MASK			7

#define MAIN_PORT			0
#define OTHER_PORT			1
#define HOST_PORT			SWITCH_PORT_NUM

#define PORT_COUNTER_NUM		0x20
#define TOTAL_PORT_COUNTER_NUM		(PORT_COUNTER_NUM + 2)

#define MIB_COUNTER_RX_LO_PRIORITY	0x00
#define MIB_COUNTER_RX_HI_PRIORITY	0x01
#define MIB_COUNTER_RX_UNDERSIZE	0x02
#define MIB_COUNTER_RX_FRAGMENT		0x03
#define MIB_COUNTER_RX_OVERSIZE		0x04
#define MIB_COUNTER_RX_JABBER		0x05
#define MIB_COUNTER_RX_SYMBOL_ERR	0x06
#define MIB_COUNTER_RX_CRC_ERR		0x07
#define MIB_COUNTER_RX_ALIGNMENT_ERR	0x08
#define MIB_COUNTER_RX_CTRL_8808	0x09
#define MIB_COUNTER_RX_PAUSE		0x0A
#define MIB_COUNTER_RX_BROADCAST	0x0B
#define MIB_COUNTER_RX_MULTICAST	0x0C
#define MIB_COUNTER_RX_UNICAST		0x0D
#define MIB_COUNTER_RX_OCTET_64		0x0E
#define MIB_COUNTER_RX_OCTET_65_127	0x0F
#define MIB_COUNTER_RX_OCTET_128_255	0x10
#define MIB_COUNTER_RX_OCTET_256_511	0x11
#define MIB_COUNTER_RX_OCTET_512_1023	0x12
#define MIB_COUNTER_RX_OCTET_1024_1522	0x13
#define MIB_COUNTER_TX_LO_PRIORITY	0x14
#define MIB_COUNTER_TX_HI_PRIORITY	0x15
#define MIB_COUNTER_TX_LATE_COLLISION	0x16
#define MIB_COUNTER_TX_PAUSE		0x17
#define MIB_COUNTER_TX_BROADCAST	0x18
#define MIB_COUNTER_TX_MULTICAST	0x19
#define MIB_COUNTER_TX_UNICAST		0x1A
#define MIB_COUNTER_TX_DEFERRED		0x1B
#define MIB_COUNTER_TX_TOTAL_COLLISION	0x1C
#define MIB_COUNTER_TX_EXCESS_COLLISION	0x1D
#define MIB_COUNTER_TX_SINGLE_COLLISION	0x1E
#define MIB_COUNTER_TX_MULTI_COLLISION	0x1F

#define MIB_COUNTER_RX_DROPPED_PACKET	0x20
#define MIB_COUNTER_TX_DROPPED_PACKET	0x21

struct ksz_port_mib {
	u8 cnt_ptr;
	u8 link_down;
	u8 state;
	u8 mib_start;

	u64 counter[TOTAL_PORT_COUNTER_NUM];
	u32 dropped[2];
};

struct ksz_port_cfg {
	u16 vid;
	u8 member;
	u8 port_prio;
	u32 rx_rate[PRIO_QUEUES];
	u32 tx_rate[PRIO_QUEUES];
	int stp_state;
};

struct ksz_switch {
	struct ksz_mac_table mac_table[STATIC_MAC_TABLE_ENTRIES];
	struct ksz_vlan_table vlan_table[VLAN_TABLE_ENTRIES];
	struct ksz_port_cfg port_cfg[TOTAL_PORT_NUM];

	u8 diffserv[DIFFSERV_ENTRIES];
	u8 p_802_1p[PRIO_802_1P_ENTRIES];

	u8 br_addr[ETH_ALEN];
	u8 other_addr[ETH_ALEN];

	u8 broad_per;
	u8 member;
};

#define TX_RATE_UNIT			10000

struct ksz_port_info {
	uint state;
	uint tx_rate;
	u8 duplex;
	u8 advertised;
	u8 partner;
	u8 port_id;
	void *pdev;
};

#define MAX_TX_HELD_SIZE		52000

#define LINK_INT_WORKING		(1 << 0)
#define SMALL_PACKET_TX_BUG		(1 << 1)
#define HALF_DUPLEX_SIGNAL_BUG		(1 << 2)
#define RX_HUGE_FRAME			(1 << 4)
#define STP_SUPPORT			(1 << 8)

#define PAUSE_FLOW_CTRL			(1 << 0)
#define FAST_AGING			(1 << 1)

struct ksz_hw {
	void __iomem *io;

	struct ksz_switch *ksz_switch;
	struct ksz_port_info port_info[SWITCH_PORT_NUM];
	struct ksz_port_mib port_mib[TOTAL_PORT_NUM];
	int dev_count;
	int dst_ports;
	int id;
	int mib_cnt;
	int mib_port_cnt;

	u32 tx_cfg;
	u32 rx_cfg;
	u32 intr_mask;
	u32 intr_set;
	uint intr_blocked;

	struct ksz_desc_info rx_desc_info;
	struct ksz_desc_info tx_desc_info;

	int tx_int_cnt;
	int tx_int_mask;
	int tx_size;

	u8 perm_addr[ETH_ALEN];
	u8 override_addr[ETH_ALEN];
	u8 address[ADDITIONAL_ENTRIES][ETH_ALEN];
	u8 addr_list_size;
	u8 mac_override;
	u8 promiscuous;
	u8 all_multi;
	u8 multi_list[MAX_MULTICAST_LIST][ETH_ALEN];
	u8 multi_bits[HW_MULTICAST_SIZE];
	u8 multi_list_size;

	u8 enabled;
	u8 rx_stop;
	u8 reserved2[1];

	uint features;
	uint overrides;

	void *parent;
};

enum {
	PHY_NO_FLOW_CTRL,
	PHY_FLOW_CTRL,
	PHY_TX_ONLY,
	PHY_RX_ONLY
};

struct ksz_port {
	u8 duplex;
	u8 speed;
	u8 force_link;
	u8 flow_ctrl;

	int first_port;
	int mib_port_cnt;
	int port_cnt;
	u64 counter[OID_COUNTER_LAST];

	struct ksz_hw *hw;
	struct ksz_port_info *linked;
};

struct ksz_timer_info {
	struct timer_list timer;
	int cnt;
	int max;
	int period;
};

struct ksz_shared_mem {
	dma_addr_t dma_addr;
	uint alloc_size;
	uint phys;
	u8 *alloc_virt;
	u8 *virt;
};

struct ksz_counter_info {
	wait_queue_head_t counter;
	unsigned long time;
	int read;
};

struct dev_info {
	struct net_device *dev;
	struct pci_dev *pdev;

	struct ksz_hw hw;
	struct ksz_shared_mem desc_pool;

	spinlock_t hwlock;
	struct mutex lock;

	int (*dev_rcv)(struct dev_info *);

	struct sk_buff *last_skb;
	int skb_index;
	int skb_len;

	struct work_struct mib_read;
	struct ksz_timer_info mib_timer_info;
	struct ksz_counter_info counter[TOTAL_PORT_NUM];

	int mtu;
	int opened;

	struct tasklet_struct rx_tasklet;
	struct tasklet_struct tx_tasklet;

	int wol_enable;
	int wol_support;
	unsigned long pme_wait;
};

struct dev_priv {
	struct dev_info *adapter;
	struct ksz_port port;
	struct ksz_timer_info monitor_timer_info;

	struct semaphore proc_sem;
	int id;

	struct mii_if_info mii_if;
	u32 advertising;

	u32 msg_enable;
	int media_state;
	int multicast;
	int promiscuous;
};

#define DRV_NAME		"KSZ884X PCI"
#define DEVICE_NAME		"KSZ884x PCI"
#define DRV_VERSION		"1.0.0"
#define DRV_RELDATE		"Feb 8, 2010"

static char version[] __devinitdata =
	"Micrel " DEVICE_NAME " " DRV_VERSION " (" DRV_RELDATE ")";

static u8 DEFAULT_MAC_ADDRESS[] = { 0x00, 0x10, 0xA1, 0x88, 0x42, 0x01 };


static inline void hw_ack_intr(struct ksz_hw *hw, uint interrupt)
{
	writel(interrupt, hw->io + KS884X_INTERRUPTS_STATUS);
}

static inline void hw_dis_intr(struct ksz_hw *hw)
{
	hw->intr_blocked = hw->intr_mask;
	writel(0, hw->io + KS884X_INTERRUPTS_ENABLE);
	hw->intr_set = readl(hw->io + KS884X_INTERRUPTS_ENABLE);
}

static inline void hw_set_intr(struct ksz_hw *hw, uint interrupt)
{
	hw->intr_set = interrupt;
	writel(interrupt, hw->io + KS884X_INTERRUPTS_ENABLE);
}

static inline void hw_ena_intr(struct ksz_hw *hw)
{
	hw->intr_blocked = 0;
	hw_set_intr(hw, hw->intr_mask);
}

static inline void hw_dis_intr_bit(struct ksz_hw *hw, uint bit)
{
	hw->intr_mask &= ~(bit);
}

static inline void hw_turn_off_intr(struct ksz_hw *hw, uint interrupt)
{
	u32 read_intr;

	read_intr = readl(hw->io + KS884X_INTERRUPTS_ENABLE);
	hw->intr_set = read_intr & ~interrupt;
	writel(hw->intr_set, hw->io + KS884X_INTERRUPTS_ENABLE);
	hw_dis_intr_bit(hw, interrupt);
}

static void hw_turn_on_intr(struct ksz_hw *hw, u32 bit)
{
	hw->intr_mask |= bit;

	if (!hw->intr_blocked)
		hw_set_intr(hw, hw->intr_mask);
}

static inline void hw_ena_intr_bit(struct ksz_hw *hw, uint interrupt)
{
	u32 read_intr;

	read_intr = readl(hw->io + KS884X_INTERRUPTS_ENABLE);
	hw->intr_set = read_intr | interrupt;
	writel(hw->intr_set, hw->io + KS884X_INTERRUPTS_ENABLE);
}

static inline void hw_read_intr(struct ksz_hw *hw, uint *status)
{
	*status = readl(hw->io + KS884X_INTERRUPTS_STATUS);
	*status = *status & hw->intr_set;
}

static inline void hw_restore_intr(struct ksz_hw *hw, uint interrupt)
{
	if (interrupt)
		hw_ena_intr(hw);
}

static uint hw_block_intr(struct ksz_hw *hw)
{
	uint interrupt = 0;

	if (!hw->intr_blocked) {
		hw_dis_intr(hw);
		interrupt = hw->intr_blocked;
	}
	return interrupt;
}


static inline void reset_desc(struct ksz_desc *desc, union desc_stat status)
{
	status.rx.hw_owned = 0;
	desc->phw->ctrl.data = cpu_to_le32(status.data);
}

static inline void release_desc(struct ksz_desc *desc)
{
	desc->sw.ctrl.tx.hw_owned = 1;
	if (desc->sw.buf_size != desc->sw.buf.data) {
		desc->sw.buf_size = desc->sw.buf.data;
		desc->phw->buf.data = cpu_to_le32(desc->sw.buf.data);
	}
	desc->phw->ctrl.data = cpu_to_le32(desc->sw.ctrl.data);
}

static void get_rx_pkt(struct ksz_desc_info *info, struct ksz_desc **desc)
{
	*desc = &info->ring[info->last];
	info->last++;
	info->last &= info->mask;
	info->avail--;
	(*desc)->sw.buf.data &= ~KS_DESC_RX_MASK;
}

static inline void set_rx_buf(struct ksz_desc *desc, u32 addr)
{
	desc->phw->addr = cpu_to_le32(addr);
}

static inline void set_rx_len(struct ksz_desc *desc, u32 len)
{
	desc->sw.buf.rx.buf_size = len;
}

static inline void get_tx_pkt(struct ksz_desc_info *info,
	struct ksz_desc **desc)
{
	*desc = &info->ring[info->next];
	info->next++;
	info->next &= info->mask;
	info->avail--;
	(*desc)->sw.buf.data &= ~KS_DESC_TX_MASK;
}

static inline void set_tx_buf(struct ksz_desc *desc, u32 addr)
{
	desc->phw->addr = cpu_to_le32(addr);
}

static inline void set_tx_len(struct ksz_desc *desc, u32 len)
{
	desc->sw.buf.tx.buf_size = len;
}


#define TABLE_READ			0x10
#define TABLE_SEL_SHIFT			2

#define HW_DELAY(hw, reg)			\
	do {					\
		u16 dummy;			\
		dummy = readw(hw->io + reg);	\
	} while (0)

static void sw_r_table(struct ksz_hw *hw, int table, u16 addr, u32 *data)
{
	u16 ctrl_addr;
	uint interrupt;

	ctrl_addr = (((table << TABLE_SEL_SHIFT) | TABLE_READ) << 8) | addr;

	interrupt = hw_block_intr(hw);

	writew(ctrl_addr, hw->io + KS884X_IACR_OFFSET);
	HW_DELAY(hw, KS884X_IACR_OFFSET);
	*data = readl(hw->io + KS884X_ACC_DATA_0_OFFSET);

	hw_restore_intr(hw, interrupt);
}

/**
 * sw_w_table_64 - write 8 bytes of data to the switch table
 * @hw:		The hardware instance.
 * @table:	The table selector.
 * @addr:	The address of the table entry.
 * @data_hi:	The high part of data to be written (bit63 ~ bit32).
 * @data_lo:	The low part of data to be written (bit31 ~ bit0).
 *
 * This routine writes 8 bytes of data to the table of the switch.
 * Hardware interrupts are disabled to minimize corruption of written data.
 */
static void sw_w_table_64(struct ksz_hw *hw, int table, u16 addr, u32 data_hi,
	u32 data_lo)
{
	u16 ctrl_addr;
	uint interrupt;

	ctrl_addr = ((table << TABLE_SEL_SHIFT) << 8) | addr;

	interrupt = hw_block_intr(hw);

	writel(data_hi, hw->io + KS884X_ACC_DATA_4_OFFSET);
	writel(data_lo, hw->io + KS884X_ACC_DATA_0_OFFSET);

	writew(ctrl_addr, hw->io + KS884X_IACR_OFFSET);
	HW_DELAY(hw, KS884X_IACR_OFFSET);

	hw_restore_intr(hw, interrupt);
}

static void sw_w_sta_mac_table(struct ksz_hw *hw, u16 addr, u8 *mac_addr,
	u8 ports, int override, int valid, int use_fid, u8 fid)
{
	u32 data_hi;
	u32 data_lo;

	data_lo = ((u32) mac_addr[2] << 24) |
		((u32) mac_addr[3] << 16) |
		((u32) mac_addr[4] << 8) | mac_addr[5];
	data_hi = ((u32) mac_addr[0] << 8) | mac_addr[1];
	data_hi |= (u32) ports << STATIC_MAC_FWD_PORTS_SHIFT;

	if (override)
		data_hi |= STATIC_MAC_TABLE_OVERRIDE;
	if (use_fid) {
		data_hi |= STATIC_MAC_TABLE_USE_FID;
		data_hi |= (u32) fid << STATIC_MAC_FID_SHIFT;
	}
	if (valid)
		data_hi |= STATIC_MAC_TABLE_VALID;

	sw_w_table_64(hw, TABLE_STATIC_MAC, addr, data_hi, data_lo);
}

static int sw_r_vlan_table(struct ksz_hw *hw, u16 addr, u16 *vid, u8 *fid,
	u8 *member)
{
	u32 data;

	sw_r_table(hw, TABLE_VLAN, addr, &data);
	if (data & VLAN_TABLE_VALID) {
		*vid = (u16)(data & VLAN_TABLE_VID);
		*fid = (u8)((data & VLAN_TABLE_FID) >> VLAN_TABLE_FID_SHIFT);
		*member = (u8)((data & VLAN_TABLE_MEMBERSHIP) >>
			VLAN_TABLE_MEMBERSHIP_SHIFT);
		return 0;
	}
	return -1;
}

static void port_r_mib_cnt(struct ksz_hw *hw, int port, u16 addr, u64 *cnt)
{
	u32 data;
	u16 ctrl_addr;
	uint interrupt;
	int timeout;

	ctrl_addr = addr + PORT_COUNTER_NUM * port;

	interrupt = hw_block_intr(hw);

	ctrl_addr |= (((TABLE_MIB << TABLE_SEL_SHIFT) | TABLE_READ) << 8);
	writew(ctrl_addr, hw->io + KS884X_IACR_OFFSET);
	HW_DELAY(hw, KS884X_IACR_OFFSET);

	for (timeout = 100; timeout > 0; timeout--) {
		data = readl(hw->io + KS884X_ACC_DATA_0_OFFSET);

		if (data & MIB_COUNTER_VALID) {
			if (data & MIB_COUNTER_OVERFLOW)
				*cnt += MIB_COUNTER_VALUE + 1;
			*cnt += data & MIB_COUNTER_VALUE;
			break;
		}
	}

	hw_restore_intr(hw, interrupt);
}

static void port_r_mib_pkt(struct ksz_hw *hw, int port, u32 *last, u64 *cnt)
{
	u32 cur;
	u32 data;
	u16 ctrl_addr;
	uint interrupt;
	int index;

	index = KS_MIB_PACKET_DROPPED_RX_0 + port;
	do {
		interrupt = hw_block_intr(hw);

		ctrl_addr = (u16) index;
		ctrl_addr |= (((TABLE_MIB << TABLE_SEL_SHIFT) | TABLE_READ)
			<< 8);
		writew(ctrl_addr, hw->io + KS884X_IACR_OFFSET);
		HW_DELAY(hw, KS884X_IACR_OFFSET);
		data = readl(hw->io + KS884X_ACC_DATA_0_OFFSET);

		hw_restore_intr(hw, interrupt);

		data &= MIB_PACKET_DROPPED;
		cur = *last;
		if (data != cur) {
			*last = data;
			if (data < cur)
				data += MIB_PACKET_DROPPED + 1;
			data -= cur;
			*cnt += data;
		}
		++last;
		++cnt;
		index -= KS_MIB_PACKET_DROPPED_TX -
			KS_MIB_PACKET_DROPPED_TX_0 + 1;
	} while (index >= KS_MIB_PACKET_DROPPED_TX_0 + port);
}

static int port_r_cnt(struct ksz_hw *hw, int port)
{
	struct ksz_port_mib *mib = &hw->port_mib[port];

	if (mib->mib_start < PORT_COUNTER_NUM)
		while (mib->cnt_ptr < PORT_COUNTER_NUM) {
			port_r_mib_cnt(hw, port, mib->cnt_ptr,
				&mib->counter[mib->cnt_ptr]);
			++mib->cnt_ptr;
		}
	if (hw->mib_cnt > PORT_COUNTER_NUM)
		port_r_mib_pkt(hw, port, mib->dropped,
			&mib->counter[PORT_COUNTER_NUM]);
	mib->cnt_ptr = 0;
	return 0;
}

static void port_init_cnt(struct ksz_hw *hw, int port)
{
	struct ksz_port_mib *mib = &hw->port_mib[port];

	mib->cnt_ptr = 0;
	if (mib->mib_start < PORT_COUNTER_NUM)
		do {
			port_r_mib_cnt(hw, port, mib->cnt_ptr,
				&mib->counter[mib->cnt_ptr]);
			++mib->cnt_ptr;
		} while (mib->cnt_ptr < PORT_COUNTER_NUM);
	if (hw->mib_cnt > PORT_COUNTER_NUM)
		port_r_mib_pkt(hw, port, mib->dropped,
			&mib->counter[PORT_COUNTER_NUM]);
	memset((void *) mib->counter, 0, sizeof(u64) * TOTAL_PORT_COUNTER_NUM);
	mib->cnt_ptr = 0;
}


static int port_chk(struct ksz_hw *hw, int port, int offset, u16 bits)
{
	u32 addr;
	u16 data;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	data = readw(hw->io + addr);
	return (data & bits) == bits;
}

static void port_cfg(struct ksz_hw *hw, int port, int offset, u16 bits,
	int set)
{
	u32 addr;
	u16 data;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	data = readw(hw->io + addr);
	if (set)
		data |= bits;
	else
		data &= ~bits;
	writew(data, hw->io + addr);
}

static int port_chk_shift(struct ksz_hw *hw, int port, u32 addr, int shift)
{
	u16 data;
	u16 bit = 1 << port;

	data = readw(hw->io + addr);
	data >>= shift;
	return (data & bit) == bit;
}

static void port_cfg_shift(struct ksz_hw *hw, int port, u32 addr, int shift,
	int set)
{
	u16 data;
	u16 bits = 1 << port;

	data = readw(hw->io + addr);
	bits <<= shift;
	if (set)
		data |= bits;
	else
		data &= ~bits;
	writew(data, hw->io + addr);
}

static void port_r8(struct ksz_hw *hw, int port, int offset, u8 *data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	*data = readb(hw->io + addr);
}

static void port_r16(struct ksz_hw *hw, int port, int offset, u16 *data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	*data = readw(hw->io + addr);
}

static void port_w16(struct ksz_hw *hw, int port, int offset, u16 data)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += offset;
	writew(data, hw->io + addr);
}

static int sw_chk(struct ksz_hw *hw, u32 addr, u16 bits)
{
	u16 data;

	data = readw(hw->io + addr);
	return (data & bits) == bits;
}

static void sw_cfg(struct ksz_hw *hw, u32 addr, u16 bits, int set)
{
	u16 data;

	data = readw(hw->io + addr);
	if (set)
		data |= bits;
	else
		data &= ~bits;
	writew(data, hw->io + addr);
}


static inline void port_cfg_broad_storm(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_BROADCAST_STORM, set);
}

static inline int port_chk_broad_storm(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_BROADCAST_STORM);
}

#define BROADCAST_STORM_PROTECTION_RATE	10

#define BROADCAST_STORM_VALUE		9969

static void sw_cfg_broad_storm(struct ksz_hw *hw, u8 percent)
{
	u16 data;
	u32 value = ((u32) BROADCAST_STORM_VALUE * (u32) percent / 100);

	if (value > BROADCAST_STORM_RATE)
		value = BROADCAST_STORM_RATE;

	data = readw(hw->io + KS8842_SWITCH_CTRL_3_OFFSET);
	data &= ~(BROADCAST_STORM_RATE_LO | BROADCAST_STORM_RATE_HI);
	data |= ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
	writew(data, hw->io + KS8842_SWITCH_CTRL_3_OFFSET);
}

static void sw_get_broad_storm(struct ksz_hw *hw, u8 *percent)
{
	int num;
	u16 data;

	data = readw(hw->io + KS8842_SWITCH_CTRL_3_OFFSET);
	num = (data & BROADCAST_STORM_RATE_HI);
	num <<= 8;
	num |= (data & BROADCAST_STORM_RATE_LO) >> 8;
	num = (num * 100 + BROADCAST_STORM_VALUE / 2) / BROADCAST_STORM_VALUE;
	*percent = (u8) num;
}

static void sw_dis_broad_storm(struct ksz_hw *hw, int port)
{
	port_cfg_broad_storm(hw, port, 0);
}

static void sw_ena_broad_storm(struct ksz_hw *hw, int port)
{
	sw_cfg_broad_storm(hw, hw->ksz_switch->broad_per);
	port_cfg_broad_storm(hw, port, 1);
}

static void sw_init_broad_storm(struct ksz_hw *hw)
{
	int port;

	hw->ksz_switch->broad_per = 1;
	sw_cfg_broad_storm(hw, hw->ksz_switch->broad_per);
	for (port = 0; port < TOTAL_PORT_NUM; port++)
		sw_dis_broad_storm(hw, port);
	sw_cfg(hw, KS8842_SWITCH_CTRL_2_OFFSET, MULTICAST_STORM_DISABLE, 1);
}

static void hw_cfg_broad_storm(struct ksz_hw *hw, u8 percent)
{
	if (percent > 100)
		percent = 100;

	sw_cfg_broad_storm(hw, percent);
	sw_get_broad_storm(hw, &percent);
	hw->ksz_switch->broad_per = percent;
}

static void sw_dis_prio_rate(struct ksz_hw *hw, int port)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += KS8842_PORT_IN_RATE_OFFSET;
	writel(0, hw->io + addr);
}

static void sw_init_prio_rate(struct ksz_hw *hw)
{
	int port;
	int prio;
	struct ksz_switch *sw = hw->ksz_switch;

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		for (prio = 0; prio < PRIO_QUEUES; prio++) {
			sw->port_cfg[port].rx_rate[prio] =
			sw->port_cfg[port].tx_rate[prio] = 0;
		}
		sw_dis_prio_rate(hw, port);
	}
}


static inline void port_cfg_back_pressure(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_BACK_PRESSURE, set);
}

static inline void port_cfg_force_flow_ctrl(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_FORCE_FLOW_CTRL, set);
}

static inline int port_chk_back_pressure(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_BACK_PRESSURE);
}

static inline int port_chk_force_flow_ctrl(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_FORCE_FLOW_CTRL);
}


static inline void port_cfg_dis_learn(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_LEARN_DISABLE, set);
}

static inline void port_cfg_rx(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_RX_ENABLE, set);
}

static inline void port_cfg_tx(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_TX_ENABLE, set);
}

static inline void sw_cfg_fast_aging(struct ksz_hw *hw, int set)
{
	sw_cfg(hw, KS8842_SWITCH_CTRL_1_OFFSET, SWITCH_FAST_AGING, set);
}

static inline void sw_flush_dyn_mac_table(struct ksz_hw *hw)
{
	if (!(hw->overrides & FAST_AGING)) {
		sw_cfg_fast_aging(hw, 1);
		mdelay(1);
		sw_cfg_fast_aging(hw, 0);
	}
}


static inline void port_cfg_ins_tag(struct ksz_hw *hw, int p, int insert)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_INSERT_TAG, insert);
}

static inline void port_cfg_rmv_tag(struct ksz_hw *hw, int p, int remove)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_REMOVE_TAG, remove);
}

static inline int port_chk_ins_tag(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_INSERT_TAG);
}

static inline int port_chk_rmv_tag(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_REMOVE_TAG);
}

static inline void port_cfg_dis_non_vid(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_DISCARD_NON_VID, set);
}

static inline void port_cfg_in_filter(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_INGRESS_VLAN_FILTER, set);
}

static inline int port_chk_dis_non_vid(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_DISCARD_NON_VID);
}

static inline int port_chk_in_filter(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_INGRESS_VLAN_FILTER);
}


static inline void port_cfg_mirror_sniffer(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_MIRROR_SNIFFER, set);
}

static inline void port_cfg_mirror_rx(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_MIRROR_RX, set);
}

static inline void port_cfg_mirror_tx(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_MIRROR_TX, set);
}

static inline void sw_cfg_mirror_rx_tx(struct ksz_hw *hw, int set)
{
	sw_cfg(hw, KS8842_SWITCH_CTRL_2_OFFSET, SWITCH_MIRROR_RX_TX, set);
}

static void sw_init_mirror(struct ksz_hw *hw)
{
	int port;

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		port_cfg_mirror_sniffer(hw, port, 0);
		port_cfg_mirror_rx(hw, port, 0);
		port_cfg_mirror_tx(hw, port, 0);
	}
	sw_cfg_mirror_rx_tx(hw, 0);
}

static inline void sw_cfg_unk_def_deliver(struct ksz_hw *hw, int set)
{
	sw_cfg(hw, KS8842_SWITCH_CTRL_7_OFFSET,
		SWITCH_UNK_DEF_PORT_ENABLE, set);
}

static inline int sw_cfg_chk_unk_def_deliver(struct ksz_hw *hw)
{
	return sw_chk(hw, KS8842_SWITCH_CTRL_7_OFFSET,
		SWITCH_UNK_DEF_PORT_ENABLE);
}

static inline void sw_cfg_unk_def_port(struct ksz_hw *hw, int port, int set)
{
	port_cfg_shift(hw, port, KS8842_SWITCH_CTRL_7_OFFSET, 0, set);
}

static inline int sw_chk_unk_def_port(struct ksz_hw *hw, int port)
{
	return port_chk_shift(hw, port, KS8842_SWITCH_CTRL_7_OFFSET, 0);
}


static inline void port_cfg_diffserv(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_DIFFSERV_ENABLE, set);
}

static inline void port_cfg_802_1p(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_802_1P_ENABLE, set);
}

static inline void port_cfg_replace_vid(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_USER_PRIORITY_CEILING, set);
}

static inline void port_cfg_prio(struct ksz_hw *hw, int p, int set)
{
	port_cfg(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_PRIO_QUEUE_ENABLE, set);
}

static inline int port_chk_diffserv(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_DIFFSERV_ENABLE);
}

static inline int port_chk_802_1p(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_802_1P_ENABLE);
}

static inline int port_chk_replace_vid(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_2_OFFSET, PORT_USER_PRIORITY_CEILING);
}

static inline int port_chk_prio(struct ksz_hw *hw, int p)
{
	return port_chk(hw, p,
		KS8842_PORT_CTRL_1_OFFSET, PORT_PRIO_QUEUE_ENABLE);
}

static void sw_dis_diffserv(struct ksz_hw *hw, int port)
{
	port_cfg_diffserv(hw, port, 0);
}

static void sw_dis_802_1p(struct ksz_hw *hw, int port)
{
	port_cfg_802_1p(hw, port, 0);
}

static void sw_cfg_replace_null_vid(struct ksz_hw *hw, int set)
{
	sw_cfg(hw, KS8842_SWITCH_CTRL_3_OFFSET, SWITCH_REPLACE_NULL_VID, set);
}

static void sw_cfg_replace_vid(struct ksz_hw *hw, int port, int set)
{
	port_cfg_replace_vid(hw, port, set);
}

static void sw_cfg_port_based(struct ksz_hw *hw, int port, u8 prio)
{
	u16 data;

	if (prio > PORT_BASED_PRIORITY_BASE)
		prio = PORT_BASED_PRIORITY_BASE;

	hw->ksz_switch->port_cfg[port].port_prio = prio;

	port_r16(hw, port, KS8842_PORT_CTRL_1_OFFSET, &data);
	data &= ~PORT_BASED_PRIORITY_MASK;
	data |= prio << PORT_BASED_PRIORITY_SHIFT;
	port_w16(hw, port, KS8842_PORT_CTRL_1_OFFSET, data);
}

static void sw_dis_multi_queue(struct ksz_hw *hw, int port)
{
	port_cfg_prio(hw, port, 0);
}

static void sw_init_prio(struct ksz_hw *hw)
{
	int port;
	int tos;
	struct ksz_switch *sw = hw->ksz_switch;

	sw->p_802_1p[0] = 0;
	sw->p_802_1p[1] = 0;
	sw->p_802_1p[2] = 1;
	sw->p_802_1p[3] = 1;
	sw->p_802_1p[4] = 2;
	sw->p_802_1p[5] = 2;
	sw->p_802_1p[6] = 3;
	sw->p_802_1p[7] = 3;

	for (tos = 0; tos < DIFFSERV_ENTRIES; tos++)
		sw->diffserv[tos] = 0;

	
	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		sw_dis_multi_queue(hw, port);
		sw_dis_diffserv(hw, port);
		sw_dis_802_1p(hw, port);
		sw_cfg_replace_vid(hw, port, 0);

		sw->port_cfg[port].port_prio = 0;
		sw_cfg_port_based(hw, port, sw->port_cfg[port].port_prio);
	}
	sw_cfg_replace_null_vid(hw, 0);
}

static void port_get_def_vid(struct ksz_hw *hw, int port, u16 *vid)
{
	u32 addr;

	PORT_CTRL_ADDR(port, addr);
	addr += KS8842_PORT_CTRL_VID_OFFSET;
	*vid = readw(hw->io + addr);
}

static void sw_init_vlan(struct ksz_hw *hw)
{
	int port;
	int entry;
	struct ksz_switch *sw = hw->ksz_switch;

	
	for (entry = 0; entry < VLAN_TABLE_ENTRIES; entry++) {
		sw_r_vlan_table(hw, entry,
			&sw->vlan_table[entry].vid,
			&sw->vlan_table[entry].fid,
			&sw->vlan_table[entry].member);
	}

	for (port = 0; port < TOTAL_PORT_NUM; port++) {
		port_get_def_vid(hw, port, &sw->port_cfg[port].vid);
		sw->port_cfg[port].member = PORT_MASK;
	}
}

static void sw_cfg_port_base_vlan(struct ksz_hw *hw, int port, u8 member)
{
	u32 addr;
	u8 data;

	PORT_CTRL_ADDR(port, addr);
	addr += KS8842_PORT_CTRL_2_OFFSET;

	data = readb(hw->io + addr);
	data &= ~PORT_VLAN_MEMBERSHIP;
	data |= (member & PORT_MASK);
	writeb(data, hw->io + addr);

	hw->ksz_switch->port_cfg[port].member = member;
}

static inline void sw_get_addr(struct ksz_hw *hw, u8 *mac_addr)
{
	int i;

	for (i = 0; i < 6; i += 2) {
		mac_addr[i] = readb(hw->io + KS8842_MAC_ADDR_0_OFFSET + i);
		mac_addr[1 + i] = readb(hw->io + KS8842_MAC_ADDR_1_OFFSET + i);
	}
}

static void sw_set_addr(struct ksz_hw *hw, u8 *mac_addr)
{
	int i;

	for (i = 0; i < 6; i += 2) {
		writeb(mac_addr[i], hw->io + KS8842_MAC_ADDR_0_OFFSET + i);
		writeb(mac_addr[1 + i], hw->io + KS8842_MAC_ADDR_1_OFFSET + i);
	}
}

static void sw_set_global_ctrl(struct ksz_hw *hw)
{
	u16 data;

	
	data = readw(hw->io + KS8842_SWITCH_CTRL_3_OFFSET);
	data |= SWITCH_FLOW_CTRL;
	writew(data, hw->io + KS8842_SWITCH_CTRL_3_OFFSET);

	data = readw(hw->io + KS8842_SWITCH_CTRL_1_OFFSET);

	
	data |= SWITCH_AGGR_BACKOFF;

	
	data |= SWITCH_AGING_ENABLE;
	data |= SWITCH_LINK_AUTO_AGING;

	if (hw->overrides & FAST_AGING)
		data |= SWITCH_FAST_AGING;
	else
		data &= ~SWITCH_FAST_AGING;
	writew(data, hw->io + KS8842_SWITCH_CTRL_1_OFFSET);

	data = readw(hw->io + KS8842_SWITCH_CTRL_2_OFFSET);

	
	data |= NO_EXC_COLLISION_DROP;
	writew(data, hw->io + KS8842_SWITCH_CTRL_2_OFFSET);
}

enum {
	STP_STATE_DISABLED = 0,
	STP_STATE_LISTENING,
	STP_STATE_LEARNING,
	STP_STATE_FORWARDING,
	STP_STATE_BLOCKED,
	STP_STATE_SIMPLE
};

static void port_set_stp_state(struct ksz_hw *hw, int port, int state)
{
	u16 data;

	port_r16(hw, port, KS8842_PORT_CTRL_2_OFFSET, &data);
	switch (state) {
	case STP_STATE_DISABLED:
		data &= ~(PORT_TX_ENABLE | PORT_RX_ENABLE);
		data |= PORT_LEARN_DISABLE;
		break;
	case STP_STATE_LISTENING:
		data &= ~PORT_TX_ENABLE;
		data |= PORT_RX_ENABLE;
		data |= PORT_LEARN_DISABLE;
		break;
	case STP_STATE_LEARNING:
		data &= ~PORT_TX_ENABLE;
		data |= PORT_RX_ENABLE;
		data &= ~PORT_LEARN_DISABLE;
		break;
	case STP_STATE_FORWARDING:
		data |= (PORT_TX_ENABLE | PORT_RX_ENABLE);
		data &= ~PORT_LEARN_DISABLE;
		break;
	case STP_STATE_BLOCKED:
		data &= ~(PORT_TX_ENABLE | PORT_RX_ENABLE);
		data |= PORT_LEARN_DISABLE;
		break;
	case STP_STATE_SIMPLE:
		data |= (PORT_TX_ENABLE | PORT_RX_ENABLE);
		data |= PORT_LEARN_DISABLE;
		break;
	}
	port_w16(hw, port, KS8842_PORT_CTRL_2_OFFSET, data);
	hw->ksz_switch->port_cfg[port].stp_state = state;
}

#define STP_ENTRY			0
#define BROADCAST_ENTRY			1
#define BRIDGE_ADDR_ENTRY		2
#define IPV6_ADDR_ENTRY			3

static void sw_clr_sta_mac_table(struct ksz_hw *hw)
{
	struct ksz_mac_table *entry;
	int i;

	for (i = 0; i < STATIC_MAC_TABLE_ENTRIES; i++) {
		entry = &hw->ksz_switch->mac_table[i];
		sw_w_sta_mac_table(hw, i,
			entry->mac_addr, entry->ports,
			entry->override, 0,
			entry->use_fid, entry->fid);
	}
}

static void sw_init_stp(struct ksz_hw *hw)
{
	struct ksz_mac_table *entry;

	entry = &hw->ksz_switch->mac_table[STP_ENTRY];
	entry->mac_addr[0] = 0x01;
	entry->mac_addr[1] = 0x80;
	entry->mac_addr[2] = 0xC2;
	entry->mac_addr[3] = 0x00;
	entry->mac_addr[4] = 0x00;
	entry->mac_addr[5] = 0x00;
	entry->ports = HOST_MASK;
	entry->override = 1;
	entry->valid = 1;
	sw_w_sta_mac_table(hw, STP_ENTRY,
		entry->mac_addr, entry->ports,
		entry->override, entry->valid,
		entry->use_fid, entry->fid);
}

static void sw_block_addr(struct ksz_hw *hw)
{
	struct ksz_mac_table *entry;
	int i;

	for (i = BROADCAST_ENTRY; i <= IPV6_ADDR_ENTRY; i++) {
		entry = &hw->ksz_switch->mac_table[i];
		entry->valid = 0;
		sw_w_sta_mac_table(hw, i,
			entry->mac_addr, entry->ports,
			entry->override, entry->valid,
			entry->use_fid, entry->fid);
	}
}

#define PHY_LINK_SUPPORT		\
	(PHY_AUTO_NEG_ASYM_PAUSE |	\
	PHY_AUTO_NEG_SYM_PAUSE |	\
	PHY_AUTO_NEG_100BT4 |		\
	PHY_AUTO_NEG_100BTX_FD |	\
	PHY_AUTO_NEG_100BTX |		\
	PHY_AUTO_NEG_10BT_FD |		\
	PHY_AUTO_NEG_10BT)

static inline void hw_r_phy_ctrl(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_CTRL_OFFSET);
}

static inline void hw_w_phy_ctrl(struct ksz_hw *hw, int phy, u16 data)
{
	writew(data, hw->io + phy + KS884X_PHY_CTRL_OFFSET);
}

static inline void hw_r_phy_link_stat(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_STATUS_OFFSET);
}

static inline void hw_r_phy_auto_neg(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_AUTO_NEG_OFFSET);
}

static inline void hw_w_phy_auto_neg(struct ksz_hw *hw, int phy, u16 data)
{
	writew(data, hw->io + phy + KS884X_PHY_AUTO_NEG_OFFSET);
}

static inline void hw_r_phy_rem_cap(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_REMOTE_CAP_OFFSET);
}

static inline void hw_r_phy_crossover(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_CTRL_OFFSET);
}

static inline void hw_w_phy_crossover(struct ksz_hw *hw, int phy, u16 data)
{
	writew(data, hw->io + phy + KS884X_PHY_CTRL_OFFSET);
}

static inline void hw_r_phy_polarity(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_PHY_CTRL_OFFSET);
}

static inline void hw_w_phy_polarity(struct ksz_hw *hw, int phy, u16 data)
{
	writew(data, hw->io + phy + KS884X_PHY_PHY_CTRL_OFFSET);
}

static inline void hw_r_phy_link_md(struct ksz_hw *hw, int phy, u16 *data)
{
	*data = readw(hw->io + phy + KS884X_PHY_LINK_MD_OFFSET);
}

static inline void hw_w_phy_link_md(struct ksz_hw *hw, int phy, u16 data)
{
	writew(data, hw->io + phy + KS884X_PHY_LINK_MD_OFFSET);
}

static void hw_r_phy(struct ksz_hw *hw, int port, u16 reg, u16 *val)
{
	int phy;

	phy = KS884X_PHY_1_CTRL_OFFSET + port * PHY_CTRL_INTERVAL + reg;
	*val = readw(hw->io + phy);
}

static void hw_w_phy(struct ksz_hw *hw, int port, u16 reg, u16 val)
{
	int phy;

	phy = KS884X_PHY_1_CTRL_OFFSET + port * PHY_CTRL_INTERVAL + reg;
	writew(val, hw->io + phy);
}


#define AT93C_CODE			0
#define AT93C_WR_OFF			0x00
#define AT93C_WR_ALL			0x10
#define AT93C_ER_ALL			0x20
#define AT93C_WR_ON			0x30

#define AT93C_WRITE			1
#define AT93C_READ			2
#define AT93C_ERASE			3

#define EEPROM_DELAY			4

static inline void drop_gpio(struct ksz_hw *hw, u8 gpio)
{
	u16 data;

	data = readw(hw->io + KS884X_EEPROM_CTRL_OFFSET);
	data &= ~gpio;
	writew(data, hw->io + KS884X_EEPROM_CTRL_OFFSET);
}

static inline void raise_gpio(struct ksz_hw *hw, u8 gpio)
{
	u16 data;

	data = readw(hw->io + KS884X_EEPROM_CTRL_OFFSET);
	data |= gpio;
	writew(data, hw->io + KS884X_EEPROM_CTRL_OFFSET);
}

static inline u8 state_gpio(struct ksz_hw *hw, u8 gpio)
{
	u16 data;

	data = readw(hw->io + KS884X_EEPROM_CTRL_OFFSET);
	return (u8)(data & gpio);
}

static void eeprom_clk(struct ksz_hw *hw)
{
	raise_gpio(hw, EEPROM_SERIAL_CLOCK);
	udelay(EEPROM_DELAY);
	drop_gpio(hw, EEPROM_SERIAL_CLOCK);
	udelay(EEPROM_DELAY);
}

static u16 spi_r(struct ksz_hw *hw)
{
	int i;
	u16 temp = 0;

	for (i = 15; i >= 0; i--) {
		raise_gpio(hw, EEPROM_SERIAL_CLOCK);
		udelay(EEPROM_DELAY);

		temp |= (state_gpio(hw, EEPROM_DATA_IN)) ? 1 << i : 0;

		drop_gpio(hw, EEPROM_SERIAL_CLOCK);
		udelay(EEPROM_DELAY);
	}
	return temp;
}

static void spi_w(struct ksz_hw *hw, u16 data)
{
	int i;

	for (i = 15; i >= 0; i--) {
		(data & (0x01 << i)) ? raise_gpio(hw, EEPROM_DATA_OUT) :
			drop_gpio(hw, EEPROM_DATA_OUT);
		eeprom_clk(hw);
	}
}

static void spi_reg(struct ksz_hw *hw, u8 data, u8 reg)
{
	int i;

	
	raise_gpio(hw, EEPROM_DATA_OUT);
	eeprom_clk(hw);

	
	for (i = 1; i >= 0; i--) {
		(data & (0x01 << i)) ? raise_gpio(hw, EEPROM_DATA_OUT) :
			drop_gpio(hw, EEPROM_DATA_OUT);
		eeprom_clk(hw);
	}

	
	for (i = 5; i >= 0; i--) {
		(reg & (0x01 << i)) ? raise_gpio(hw, EEPROM_DATA_OUT) :
			drop_gpio(hw, EEPROM_DATA_OUT);
		eeprom_clk(hw);
	}
}

#define EEPROM_DATA_RESERVED		0
#define EEPROM_DATA_MAC_ADDR_0		1
#define EEPROM_DATA_MAC_ADDR_1		2
#define EEPROM_DATA_MAC_ADDR_2		3
#define EEPROM_DATA_SUBSYS_ID		4
#define EEPROM_DATA_SUBSYS_VEN_ID	5
#define EEPROM_DATA_PM_CAP		6

#define EEPROM_DATA_OTHER_MAC_ADDR	9

static u16 eeprom_read(struct ksz_hw *hw, u8 reg)
{
	u16 data;

	raise_gpio(hw, EEPROM_ACCESS_ENABLE | EEPROM_CHIP_SELECT);

	spi_reg(hw, AT93C_READ, reg);
	data = spi_r(hw);

	drop_gpio(hw, EEPROM_ACCESS_ENABLE | EEPROM_CHIP_SELECT);

	return data;
}

static void eeprom_write(struct ksz_hw *hw, u8 reg, u16 data)
{
	int timeout;

	raise_gpio(hw, EEPROM_ACCESS_ENABLE | EEPROM_CHIP_SELECT);

	
	spi_reg(hw, AT93C_CODE, AT93C_WR_ON);
	drop_gpio(hw, EEPROM_CHIP_SELECT);
	udelay(1);

	
	raise_gpio(hw, EEPROM_CHIP_SELECT);
	spi_reg(hw, AT93C_ERASE, reg);
	drop_gpio(hw, EEPROM_CHIP_SELECT);
	udelay(1);

	
	raise_gpio(hw, EEPROM_CHIP_SELECT);
	timeout = 8;
	mdelay(2);
	do {
		mdelay(1);
	} while (!state_gpio(hw, EEPROM_DATA_IN) && --timeout);
	drop_gpio(hw, EEPROM_CHIP_SELECT);
	udelay(1);

	
	raise_gpio(hw, EEPROM_CHIP_SELECT);
	spi_reg(hw, AT93C_WRITE, reg);
	spi_w(hw, data);
	drop_gpio(hw, EEPROM_CHIP_SELECT);
	udelay(1);

	
	raise_gpio(hw, EEPROM_CHIP_SELECT);
	timeout = 8;
	mdelay(2);
	do {
		mdelay(1);
	} while (!state_gpio(hw, EEPROM_DATA_IN) && --timeout);
	drop_gpio(hw, EEPROM_CHIP_SELECT);
	udelay(1);

	
	raise_gpio(hw, EEPROM_CHIP_SELECT);
	spi_reg(hw, AT93C_CODE, AT93C_WR_OFF);

	drop_gpio(hw, EEPROM_ACCESS_ENABLE | EEPROM_CHIP_SELECT);
}


static u16 advertised_flow_ctrl(struct ksz_port *port, u16 ctrl)
{
	ctrl &= ~PORT_AUTO_NEG_SYM_PAUSE;
	switch (port->flow_ctrl) {
	case PHY_FLOW_CTRL:
		ctrl |= PORT_AUTO_NEG_SYM_PAUSE;
		break;
	
	case PHY_TX_ONLY:
	case PHY_RX_ONLY:
	default:
		break;
	}
	return ctrl;
}

static void set_flow_ctrl(struct ksz_hw *hw, int rx, int tx)
{
	u32 rx_cfg;
	u32 tx_cfg;

	rx_cfg = hw->rx_cfg;
	tx_cfg = hw->tx_cfg;
	if (rx)
		hw->rx_cfg |= DMA_RX_FLOW_ENABLE;
	else
		hw->rx_cfg &= ~DMA_RX_FLOW_ENABLE;
	if (tx)
		hw->tx_cfg |= DMA_TX_FLOW_ENABLE;
	else
		hw->tx_cfg &= ~DMA_TX_FLOW_ENABLE;
	if (hw->enabled) {
		if (rx_cfg != hw->rx_cfg)
			writel(hw->rx_cfg, hw->io + KS_DMA_RX_CTRL);
		if (tx_cfg != hw->tx_cfg)
			writel(hw->tx_cfg, hw->io + KS_DMA_TX_CTRL);
	}
}

static void determine_flow_ctrl(struct ksz_hw *hw, struct ksz_port *port,
	u16 local, u16 remote)
{
	int rx;
	int tx;

	if (hw->overrides & PAUSE_FLOW_CTRL)
		return;

	rx = tx = 0;
	if (port->force_link)
		rx = tx = 1;
	if (remote & PHY_AUTO_NEG_SYM_PAUSE) {
		if (local & PHY_AUTO_NEG_SYM_PAUSE) {
			rx = tx = 1;
		} else if ((remote & PHY_AUTO_NEG_ASYM_PAUSE) &&
				(local & PHY_AUTO_NEG_PAUSE) ==
				PHY_AUTO_NEG_ASYM_PAUSE) {
			tx = 1;
		}
	} else if (remote & PHY_AUTO_NEG_ASYM_PAUSE) {
		if ((local & PHY_AUTO_NEG_PAUSE) == PHY_AUTO_NEG_PAUSE)
			rx = 1;
	}
	if (!hw->ksz_switch)
		set_flow_ctrl(hw, rx, tx);
}

static inline void port_cfg_change(struct ksz_hw *hw, struct ksz_port *port,
	struct ksz_port_info *info, u16 link_status)
{
	if ((hw->features & HALF_DUPLEX_SIGNAL_BUG) &&
			!(hw->overrides & PAUSE_FLOW_CTRL)) {
		u32 cfg = hw->tx_cfg;

		
		if (1 == info->duplex)
			hw->tx_cfg &= ~DMA_TX_FLOW_ENABLE;
		if (hw->enabled && cfg != hw->tx_cfg)
			writel(hw->tx_cfg, hw->io + KS_DMA_TX_CTRL);
	}
}

static void port_get_link_speed(struct ksz_port *port)
{
	uint interrupt;
	struct ksz_port_info *info;
	struct ksz_port_info *linked = NULL;
	struct ksz_hw *hw = port->hw;
	u16 data;
	u16 status;
	u8 local;
	u8 remote;
	int i;
	int p;
	int change = 0;

	interrupt = hw_block_intr(hw);

	for (i = 0, p = port->first_port; i < port->port_cnt; i++, p++) {
		info = &hw->port_info[p];
		port_r16(hw, p, KS884X_PORT_CTRL_4_OFFSET, &data);
		port_r16(hw, p, KS884X_PORT_STATUS_OFFSET, &status);

		remote = status & (PORT_AUTO_NEG_COMPLETE |
			PORT_STATUS_LINK_GOOD);
		local = (u8) data;

		
		if (local == info->advertised && remote == info->partner)
			continue;

		info->advertised = local;
		info->partner = remote;
		if (status & PORT_STATUS_LINK_GOOD) {

			
			if (!linked)
				linked = info;

			info->tx_rate = 10 * TX_RATE_UNIT;
			if (status & PORT_STATUS_SPEED_100MBIT)
				info->tx_rate = 100 * TX_RATE_UNIT;

			info->duplex = 1;
			if (status & PORT_STATUS_FULL_DUPLEX)
				info->duplex = 2;

			if (media_connected != info->state) {
				hw_r_phy(hw, p, KS884X_PHY_AUTO_NEG_OFFSET,
					&data);
				hw_r_phy(hw, p, KS884X_PHY_REMOTE_CAP_OFFSET,
					&status);
				determine_flow_ctrl(hw, port, data, status);
				if (hw->ksz_switch) {
					port_cfg_back_pressure(hw, p,
						(1 == info->duplex));
				}
				change |= 1 << i;
				port_cfg_change(hw, port, info, status);
			}
			info->state = media_connected;
		} else {
			if (media_disconnected != info->state) {
				change |= 1 << i;

				
				hw->port_mib[p].link_down = 1;
			}
			info->state = media_disconnected;
		}
		hw->port_mib[p].state = (u8) info->state;
	}

	if (linked && media_disconnected == port->linked->state)
		port->linked = linked;

	hw_restore_intr(hw, interrupt);
}

#define PHY_RESET_TIMEOUT		10

static void port_set_link_speed(struct ksz_port *port)
{
	struct ksz_port_info *info;
	struct ksz_hw *hw = port->hw;
	u16 data;
	u16 cfg;
	u8 status;
	int i;
	int p;

	for (i = 0, p = port->first_port; i < port->port_cnt; i++, p++) {
		info = &hw->port_info[p];

		port_r16(hw, p, KS884X_PORT_CTRL_4_OFFSET, &data);
		port_r8(hw, p, KS884X_PORT_STATUS_OFFSET, &status);

		cfg = 0;
		if (status & PORT_STATUS_LINK_GOOD)
			cfg = data;

		data |= PORT_AUTO_NEG_ENABLE;
		data = advertised_flow_ctrl(port, data);

		data |= PORT_AUTO_NEG_100BTX_FD | PORT_AUTO_NEG_100BTX |
			PORT_AUTO_NEG_10BT_FD | PORT_AUTO_NEG_10BT;

		
		if (port->speed || port->duplex) {
			if (10 == port->speed)
				data &= ~(PORT_AUTO_NEG_100BTX_FD |
					PORT_AUTO_NEG_100BTX);
			else if (100 == port->speed)
				data &= ~(PORT_AUTO_NEG_10BT_FD |
					PORT_AUTO_NEG_10BT);
			if (1 == port->duplex)
				data &= ~(PORT_AUTO_NEG_100BTX_FD |
					PORT_AUTO_NEG_10BT_FD);
			else if (2 == port->duplex)
				data &= ~(PORT_AUTO_NEG_100BTX |
					PORT_AUTO_NEG_10BT);
		}
		if (data != cfg) {
			data |= PORT_AUTO_NEG_RESTART;
			port_w16(hw, p, KS884X_PORT_CTRL_4_OFFSET, data);
		}
	}
}

static void port_force_link_speed(struct ksz_port *port)
{
	struct ksz_hw *hw = port->hw;
	u16 data;
	int i;
	int phy;
	int p;

	for (i = 0, p = port->first_port; i < port->port_cnt; i++, p++) {
		phy = KS884X_PHY_1_CTRL_OFFSET + p * PHY_CTRL_INTERVAL;
		hw_r_phy_ctrl(hw, phy, &data);

		data &= ~PHY_AUTO_NEG_ENABLE;

		if (10 == port->speed)
			data &= ~PHY_SPEED_100MBIT;
		else if (100 == port->speed)
			data |= PHY_SPEED_100MBIT;
		if (1 == port->duplex)
			data &= ~PHY_FULL_DUPLEX;
		else if (2 == port->duplex)
			data |= PHY_FULL_DUPLEX;
		hw_w_phy_ctrl(hw, phy, data);
	}
}

static void port_set_power_saving(struct ksz_port *port, int enable)
{
	struct ksz_hw *hw = port->hw;
	int i;
	int p;

	for (i = 0, p = port->first_port; i < port->port_cnt; i++, p++)
		port_cfg(hw, p,
			KS884X_PORT_CTRL_4_OFFSET, PORT_POWER_DOWN, enable);
}


static int hw_chk_wol_pme_status(struct ksz_hw *hw)
{
	struct dev_info *hw_priv = container_of(hw, struct dev_info, hw);
	struct pci_dev *pdev = hw_priv->pdev;
	u16 data;

	if (!pdev->pm_cap)
		return 0;
	pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, &data);
	return (data & PCI_PM_CTRL_PME_STATUS) == PCI_PM_CTRL_PME_STATUS;
}

static void hw_clr_wol_pme_status(struct ksz_hw *hw)
{
	struct dev_info *hw_priv = container_of(hw, struct dev_info, hw);
	struct pci_dev *pdev = hw_priv->pdev;
	u16 data;

	if (!pdev->pm_cap)
		return;

	
	pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, &data);
	data |= PCI_PM_CTRL_PME_STATUS;
	pci_write_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, data);
}

static void hw_cfg_wol_pme(struct ksz_hw *hw, int set)
{
	struct dev_info *hw_priv = container_of(hw, struct dev_info, hw);
	struct pci_dev *pdev = hw_priv->pdev;
	u16 data;

	if (!pdev->pm_cap)
		return;
	pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, &data);
	data &= ~PCI_PM_CTRL_STATE_MASK;
	if (set)
		data |= PCI_PM_CTRL_PME_ENABLE | PCI_D3hot;
	else
		data &= ~PCI_PM_CTRL_PME_ENABLE;
	pci_write_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, data);
}

static void hw_cfg_wol(struct ksz_hw *hw, u16 frame, int set)
{
	u16 data;

	data = readw(hw->io + KS8841_WOL_CTRL_OFFSET);
	if (set)
		data |= frame;
	else
		data &= ~frame;
	writew(data, hw->io + KS8841_WOL_CTRL_OFFSET);
}

static void hw_set_wol_frame(struct ksz_hw *hw, int i, uint mask_size,
	const u8 *mask, uint frame_size, const u8 *pattern)
{
	int bits;
	int from;
	int len;
	int to;
	u32 crc;
	u8 data[64];
	u8 val = 0;

	if (frame_size > mask_size * 8)
		frame_size = mask_size * 8;
	if (frame_size > 64)
		frame_size = 64;

	i *= 0x10;
	writel(0, hw->io + KS8841_WOL_FRAME_BYTE0_OFFSET + i);
	writel(0, hw->io + KS8841_WOL_FRAME_BYTE2_OFFSET + i);

	bits = len = from = to = 0;
	do {
		if (bits) {
			if ((val & 1))
				data[to++] = pattern[from];
			val >>= 1;
			++from;
			--bits;
		} else {
			val = mask[len];
			writeb(val, hw->io + KS8841_WOL_FRAME_BYTE0_OFFSET + i
				+ len);
			++len;
			if (val)
				bits = 8;
			else
				from += 8;
		}
	} while (from < (int) frame_size);
	if (val) {
		bits = mask[len - 1];
		val <<= (from % 8);
		bits &= ~val;
		writeb(bits, hw->io + KS8841_WOL_FRAME_BYTE0_OFFSET + i + len -
			1);
	}
	crc = ether_crc(to, data);
	writel(crc, hw->io + KS8841_WOL_FRAME_CRC_OFFSET + i);
}

static void hw_add_wol_arp(struct ksz_hw *hw, const u8 *ip_addr)
{
	static const u8 mask[6] = { 0x3F, 0xF0, 0x3F, 0x00, 0xC0, 0x03 };
	u8 pattern[42] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x06,
		0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00 };

	memcpy(&pattern[38], ip_addr, 4);
	hw_set_wol_frame(hw, 3, 6, mask, 42, pattern);
}

static void hw_add_wol_bcast(struct ksz_hw *hw)
{
	static const u8 mask[] = { 0x3F };
	static const u8 pattern[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	hw_set_wol_frame(hw, 2, 1, mask, ETH_ALEN, pattern);
}

static void hw_add_wol_mcast(struct ksz_hw *hw)
{
	static const u8 mask[] = { 0x3F };
	u8 pattern[] = { 0x33, 0x33, 0xFF, 0x00, 0x00, 0x00 };

	memcpy(&pattern[3], &hw->override_addr[3], 3);
	hw_set_wol_frame(hw, 1, 1, mask, 6, pattern);
}

static void hw_add_wol_ucast(struct ksz_hw *hw)
{
	static const u8 mask[] = { 0x3F };

	hw_set_wol_frame(hw, 0, 1, mask, ETH_ALEN, hw->override_addr);
}

static void hw_enable_wol(struct ksz_hw *hw, u32 wol_enable, const u8 *net_addr)
{
	hw_cfg_wol(hw, KS8841_WOL_MAGIC_ENABLE, (wol_enable & WAKE_MAGIC));
	hw_cfg_wol(hw, KS8841_WOL_FRAME0_ENABLE, (wol_enable & WAKE_UCAST));
	hw_add_wol_ucast(hw);
	hw_cfg_wol(hw, KS8841_WOL_FRAME1_ENABLE, (wol_enable & WAKE_MCAST));
	hw_add_wol_mcast(hw);
	hw_cfg_wol(hw, KS8841_WOL_FRAME2_ENABLE, (wol_enable & WAKE_BCAST));
	hw_cfg_wol(hw, KS8841_WOL_FRAME3_ENABLE, (wol_enable & WAKE_ARP));
	hw_add_wol_arp(hw, net_addr);
}

static int hw_init(struct ksz_hw *hw)
{
	int rc = 0;
	u16 data;
	u16 revision;

	
	writew(BUS_SPEED_125_MHZ, hw->io + KS884X_BUS_CTRL_OFFSET);

	
	data = readw(hw->io + KS884X_CHIP_ID_OFFSET);

	revision = (data & KS884X_REVISION_MASK) >> KS884X_REVISION_SHIFT;
	data &= KS884X_CHIP_ID_MASK_41;
	if (REG_CHIP_ID_41 == data)
		rc = 1;
	else if (REG_CHIP_ID_42 == data)
		rc = 2;
	else
		return 0;

	
	if (revision <= 1) {
		hw->features |= SMALL_PACKET_TX_BUG;
		if (1 == rc)
			hw->features |= HALF_DUPLEX_SIGNAL_BUG;
	}
	return rc;
}

static void hw_reset(struct ksz_hw *hw)
{
	writew(GLOBAL_SOFTWARE_RESET, hw->io + KS884X_GLOBAL_CTRL_OFFSET);

	
	mdelay(10);

	
	writew(0, hw->io + KS884X_GLOBAL_CTRL_OFFSET);
}

static void hw_setup(struct ksz_hw *hw)
{
#if SET_DEFAULT_LED
	u16 data;

	
	data = readw(hw->io + KS8842_SWITCH_CTRL_5_OFFSET);
	data &= ~LED_MODE;
	data |= SET_DEFAULT_LED;
	writew(data, hw->io + KS8842_SWITCH_CTRL_5_OFFSET);
#endif

	
	hw->tx_cfg = (DMA_TX_PAD_ENABLE | DMA_TX_CRC_ENABLE |
		(DMA_BURST_DEFAULT << DMA_BURST_SHIFT) | DMA_TX_ENABLE);

	
	hw->rx_cfg = (DMA_RX_BROADCAST | DMA_RX_UNICAST |
		(DMA_BURST_DEFAULT << DMA_BURST_SHIFT) | DMA_RX_ENABLE);
	hw->rx_cfg |= KS884X_DMA_RX_MULTICAST;

	
	hw->rx_cfg |= (DMA_RX_CSUM_TCP | DMA_RX_CSUM_IP);

	if (hw->all_multi)
		hw->rx_cfg |= DMA_RX_ALL_MULTICAST;
	if (hw->promiscuous)
		hw->rx_cfg |= DMA_RX_PROMISCUOUS;
}

static void hw_setup_intr(struct ksz_hw *hw)
{
	hw->intr_mask = KS884X_INT_MASK | KS884X_INT_RX_OVERRUN;
}

static void ksz_check_desc_num(struct ksz_desc_info *info)
{
#define MIN_DESC_SHIFT  2

	int alloc = info->alloc;
	int shift;

	shift = 0;
	while (!(alloc & 1)) {
		shift++;
		alloc >>= 1;
	}
	if (alloc != 1 || shift < MIN_DESC_SHIFT) {
		pr_alert("Hardware descriptor numbers not right!\n");
		while (alloc) {
			shift++;
			alloc >>= 1;
		}
		if (shift < MIN_DESC_SHIFT)
			shift = MIN_DESC_SHIFT;
		alloc = 1 << shift;
		info->alloc = alloc;
	}
	info->mask = info->alloc - 1;
}

static void hw_init_desc(struct ksz_desc_info *desc_info, int transmit)
{
	int i;
	u32 phys = desc_info->ring_phys;
	struct ksz_hw_desc *desc = desc_info->ring_virt;
	struct ksz_desc *cur = desc_info->ring;
	struct ksz_desc *previous = NULL;

	for (i = 0; i < desc_info->alloc; i++) {
		cur->phw = desc++;
		phys += desc_info->size;
		previous = cur++;
		previous->phw->next = cpu_to_le32(phys);
	}
	previous->phw->next = cpu_to_le32(desc_info->ring_phys);
	previous->sw.buf.rx.end_of_ring = 1;
	previous->phw->buf.data = cpu_to_le32(previous->sw.buf.data);

	desc_info->avail = desc_info->alloc;
	desc_info->last = desc_info->next = 0;

	desc_info->cur = desc_info->ring;
}

static void hw_set_desc_base(struct ksz_hw *hw, u32 tx_addr, u32 rx_addr)
{
	
	writel(tx_addr, hw->io + KS_DMA_TX_ADDR);
	writel(rx_addr, hw->io + KS_DMA_RX_ADDR);
}

static void hw_reset_pkts(struct ksz_desc_info *info)
{
	info->cur = info->ring;
	info->avail = info->alloc;
	info->last = info->next = 0;
}

static inline void hw_resume_rx(struct ksz_hw *hw)
{
	writel(DMA_START, hw->io + KS_DMA_RX_START);
}

static void hw_start_rx(struct ksz_hw *hw)
{
	writel(hw->rx_cfg, hw->io + KS_DMA_RX_CTRL);

	
	hw->intr_mask |= KS884X_INT_RX_STOPPED;

	writel(DMA_START, hw->io + KS_DMA_RX_START);
	hw_ack_intr(hw, KS884X_INT_RX_STOPPED);
	hw->rx_stop++;

	
	if (0 == hw->rx_stop)
		hw->rx_stop = 2;
}

static void hw_stop_rx(struct ksz_hw *hw)
{
	hw->rx_stop = 0;
	hw_turn_off_intr(hw, KS884X_INT_RX_STOPPED);
	writel((hw->rx_cfg & ~DMA_RX_ENABLE), hw->io + KS_DMA_RX_CTRL);
}

static void hw_start_tx(struct ksz_hw *hw)
{
	writel(hw->tx_cfg, hw->io + KS_DMA_TX_CTRL);
}

static void hw_stop_tx(struct ksz_hw *hw)
{
	writel((hw->tx_cfg & ~DMA_TX_ENABLE), hw->io + KS_DMA_TX_CTRL);
}

static void hw_disable(struct ksz_hw *hw)
{
	hw_stop_rx(hw);
	hw_stop_tx(hw);
	hw->enabled = 0;
}

static void hw_enable(struct ksz_hw *hw)
{
	hw_start_tx(hw);
	hw_start_rx(hw);
	hw->enabled = 1;
}

static int hw_alloc_pkt(struct ksz_hw *hw, int length, int physical)
{
	
	if (hw->tx_desc_info.avail <= 1)
		return 0;

	
	get_tx_pkt(&hw->tx_desc_info, &hw->tx_desc_info.cur);
	hw->tx_desc_info.cur->sw.buf.tx.first_seg = 1;

	
	++hw->tx_int_cnt;
	hw->tx_size += length;

	
	if (hw->tx_size >= MAX_TX_HELD_SIZE)
		hw->tx_int_cnt = hw->tx_int_mask + 1;

	if (physical > hw->tx_desc_info.avail)
		return 1;

	return hw->tx_desc_info.avail;
}

static void hw_send_pkt(struct ksz_hw *hw)
{
	struct ksz_desc *cur = hw->tx_desc_info.cur;

	cur->sw.buf.tx.last_seg = 1;

	
	if (hw->tx_int_cnt > hw->tx_int_mask) {
		cur->sw.buf.tx.intr = 1;
		hw->tx_int_cnt = 0;
		hw->tx_size = 0;
	}

	
	cur->sw.buf.tx.dest_port = hw->dst_ports;

	release_desc(cur);

	writel(0, hw->io + KS_DMA_TX_START);
}

static int empty_addr(u8 *addr)
{
	u32 *addr1 = (u32 *) addr;
	u16 *addr2 = (u16 *) &addr[4];

	return 0 == *addr1 && 0 == *addr2;
}

static void hw_set_addr(struct ksz_hw *hw)
{
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		writeb(hw->override_addr[MAC_ADDR_ORDER(i)],
			hw->io + KS884X_ADDR_0_OFFSET + i);

	sw_set_addr(hw, hw->override_addr);
}

static void hw_read_addr(struct ksz_hw *hw)
{
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		hw->perm_addr[MAC_ADDR_ORDER(i)] = readb(hw->io +
			KS884X_ADDR_0_OFFSET + i);

	if (!hw->mac_override) {
		memcpy(hw->override_addr, hw->perm_addr, ETH_ALEN);
		if (empty_addr(hw->override_addr)) {
			memcpy(hw->perm_addr, DEFAULT_MAC_ADDRESS, ETH_ALEN);
			memcpy(hw->override_addr, DEFAULT_MAC_ADDRESS,
			       ETH_ALEN);
			hw->override_addr[5] += hw->id;
			hw_set_addr(hw);
		}
	}
}

static void hw_ena_add_addr(struct ksz_hw *hw, int index, u8 *mac_addr)
{
	int i;
	u32 mac_addr_lo;
	u32 mac_addr_hi;

	mac_addr_hi = 0;
	for (i = 0; i < 2; i++) {
		mac_addr_hi <<= 8;
		mac_addr_hi |= mac_addr[i];
	}
	mac_addr_hi |= ADD_ADDR_ENABLE;
	mac_addr_lo = 0;
	for (i = 2; i < 6; i++) {
		mac_addr_lo <<= 8;
		mac_addr_lo |= mac_addr[i];
	}
	index *= ADD_ADDR_INCR;

	writel(mac_addr_lo, hw->io + index + KS_ADD_ADDR_0_LO);
	writel(mac_addr_hi, hw->io + index + KS_ADD_ADDR_0_HI);
}

static void hw_set_add_addr(struct ksz_hw *hw)
{
	int i;

	for (i = 0; i < ADDITIONAL_ENTRIES; i++) {
		if (empty_addr(hw->address[i]))
			writel(0, hw->io + ADD_ADDR_INCR * i +
				KS_ADD_ADDR_0_HI);
		else
			hw_ena_add_addr(hw, i, hw->address[i]);
	}
}

static int hw_add_addr(struct ksz_hw *hw, u8 *mac_addr)
{
	int i;
	int j = ADDITIONAL_ENTRIES;

	if (!memcmp(hw->override_addr, mac_addr, ETH_ALEN))
		return 0;
	for (i = 0; i < hw->addr_list_size; i++) {
		if (!memcmp(hw->address[i], mac_addr, ETH_ALEN))
			return 0;
		if (ADDITIONAL_ENTRIES == j && empty_addr(hw->address[i]))
			j = i;
	}
	if (j < ADDITIONAL_ENTRIES) {
		memcpy(hw->address[j], mac_addr, ETH_ALEN);
		hw_ena_add_addr(hw, j, hw->address[j]);
		return 0;
	}
	return -1;
}

static int hw_del_addr(struct ksz_hw *hw, u8 *mac_addr)
{
	int i;

	for (i = 0; i < hw->addr_list_size; i++) {
		if (!memcmp(hw->address[i], mac_addr, ETH_ALEN)) {
			memset(hw->address[i], 0, ETH_ALEN);
			writel(0, hw->io + ADD_ADDR_INCR * i +
				KS_ADD_ADDR_0_HI);
			return 0;
		}
	}
	return -1;
}

static void hw_clr_multicast(struct ksz_hw *hw)
{
	int i;

	for (i = 0; i < HW_MULTICAST_SIZE; i++) {
		hw->multi_bits[i] = 0;

		writeb(0, hw->io + KS884X_MULTICAST_0_OFFSET + i);
	}
}

static void hw_set_grp_addr(struct ksz_hw *hw)
{
	int i;
	int index;
	int position;
	int value;

	memset(hw->multi_bits, 0, sizeof(u8) * HW_MULTICAST_SIZE);

	for (i = 0; i < hw->multi_list_size; i++) {
		position = (ether_crc(6, hw->multi_list[i]) >> 26) & 0x3f;
		index = position >> 3;
		value = 1 << (position & 7);
		hw->multi_bits[index] |= (u8) value;
	}

	for (i = 0; i < HW_MULTICAST_SIZE; i++)
		writeb(hw->multi_bits[i], hw->io + KS884X_MULTICAST_0_OFFSET +
			i);
}

static void hw_set_multicast(struct ksz_hw *hw, u8 multicast)
{
	
	hw_stop_rx(hw);

	if (multicast)
		hw->rx_cfg |= DMA_RX_ALL_MULTICAST;
	else
		hw->rx_cfg &= ~DMA_RX_ALL_MULTICAST;

	if (hw->enabled)
		hw_start_rx(hw);
}

static void hw_set_promiscuous(struct ksz_hw *hw, u8 prom)
{
	
	hw_stop_rx(hw);

	if (prom)
		hw->rx_cfg |= DMA_RX_PROMISCUOUS;
	else
		hw->rx_cfg &= ~DMA_RX_PROMISCUOUS;

	if (hw->enabled)
		hw_start_rx(hw);
}

static void sw_enable(struct ksz_hw *hw, int enable)
{
	int port;

	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		if (hw->dev_count > 1) {
			
			sw_cfg_port_base_vlan(hw, port,
				HOST_MASK | (1 << port));
			port_set_stp_state(hw, port, STP_STATE_DISABLED);
		} else {
			sw_cfg_port_base_vlan(hw, port, PORT_MASK);
			port_set_stp_state(hw, port, STP_STATE_FORWARDING);
		}
	}
	if (hw->dev_count > 1)
		port_set_stp_state(hw, SWITCH_PORT_NUM, STP_STATE_SIMPLE);
	else
		port_set_stp_state(hw, SWITCH_PORT_NUM, STP_STATE_FORWARDING);

	if (enable)
		enable = KS8842_START;
	writew(enable, hw->io + KS884X_CHIP_ID_OFFSET);
}

static void sw_setup(struct ksz_hw *hw)
{
	int port;

	sw_set_global_ctrl(hw);

	
	sw_init_broad_storm(hw);
	hw_cfg_broad_storm(hw, BROADCAST_STORM_PROTECTION_RATE);
	for (port = 0; port < SWITCH_PORT_NUM; port++)
		sw_ena_broad_storm(hw, port);

	sw_init_prio(hw);

	sw_init_mirror(hw);

	sw_init_prio_rate(hw);

	sw_init_vlan(hw);

	if (hw->features & STP_SUPPORT)
		sw_init_stp(hw);
	if (!sw_chk(hw, KS8842_SWITCH_CTRL_1_OFFSET,
			SWITCH_TX_FLOW_CTRL | SWITCH_RX_FLOW_CTRL))
		hw->overrides |= PAUSE_FLOW_CTRL;
	sw_enable(hw, 1);
}

static void ksz_start_timer(struct ksz_timer_info *info, int time)
{
	info->cnt = 0;
	info->timer.expires = jiffies + time;
	add_timer(&info->timer);

	
	info->max = -1;
}

static void ksz_stop_timer(struct ksz_timer_info *info)
{
	if (info->max) {
		info->max = 0;
		del_timer_sync(&info->timer);
	}
}

static void ksz_init_timer(struct ksz_timer_info *info, int period,
	void (*function)(unsigned long), void *data)
{
	info->max = 0;
	info->period = period;
	init_timer(&info->timer);
	info->timer.function = function;
	info->timer.data = (unsigned long) data;
}

static void ksz_update_timer(struct ksz_timer_info *info)
{
	++info->cnt;
	if (info->max > 0) {
		if (info->cnt < info->max) {
			info->timer.expires = jiffies + info->period;
			add_timer(&info->timer);
		} else
			info->max = 0;
	} else if (info->max < 0) {
		info->timer.expires = jiffies + info->period;
		add_timer(&info->timer);
	}
}

static int ksz_alloc_soft_desc(struct ksz_desc_info *desc_info, int transmit)
{
	desc_info->ring = kzalloc(sizeof(struct ksz_desc) * desc_info->alloc,
				  GFP_KERNEL);
	if (!desc_info->ring)
		return 1;
	hw_init_desc(desc_info, transmit);
	return 0;
}

static int ksz_alloc_desc(struct dev_info *adapter)
{
	struct ksz_hw *hw = &adapter->hw;
	int offset;

	
	adapter->desc_pool.alloc_size =
		hw->rx_desc_info.size * hw->rx_desc_info.alloc +
		hw->tx_desc_info.size * hw->tx_desc_info.alloc +
		DESC_ALIGNMENT;

	adapter->desc_pool.alloc_virt =
		pci_alloc_consistent(
			adapter->pdev, adapter->desc_pool.alloc_size,
			&adapter->desc_pool.dma_addr);
	if (adapter->desc_pool.alloc_virt == NULL) {
		adapter->desc_pool.alloc_size = 0;
		return 1;
	}
	memset(adapter->desc_pool.alloc_virt, 0, adapter->desc_pool.alloc_size);

	
	offset = (((ulong) adapter->desc_pool.alloc_virt % DESC_ALIGNMENT) ?
		(DESC_ALIGNMENT -
		((ulong) adapter->desc_pool.alloc_virt % DESC_ALIGNMENT)) : 0);
	adapter->desc_pool.virt = adapter->desc_pool.alloc_virt + offset;
	adapter->desc_pool.phys = adapter->desc_pool.dma_addr + offset;

	
	hw->rx_desc_info.ring_virt = (struct ksz_hw_desc *)
		adapter->desc_pool.virt;
	hw->rx_desc_info.ring_phys = adapter->desc_pool.phys;
	offset = hw->rx_desc_info.alloc * hw->rx_desc_info.size;
	hw->tx_desc_info.ring_virt = (struct ksz_hw_desc *)
		(adapter->desc_pool.virt + offset);
	hw->tx_desc_info.ring_phys = adapter->desc_pool.phys + offset;

	if (ksz_alloc_soft_desc(&hw->rx_desc_info, 0))
		return 1;
	if (ksz_alloc_soft_desc(&hw->tx_desc_info, 1))
		return 1;

	return 0;
}

static void free_dma_buf(struct dev_info *adapter, struct ksz_dma_buf *dma_buf,
	int direction)
{
	pci_unmap_single(adapter->pdev, dma_buf->dma, dma_buf->len, direction);
	dev_kfree_skb(dma_buf->skb);
	dma_buf->skb = NULL;
	dma_buf->dma = 0;
}

static void ksz_init_rx_buffers(struct dev_info *adapter)
{
	int i;
	struct ksz_desc *desc;
	struct ksz_dma_buf *dma_buf;
	struct ksz_hw *hw = &adapter->hw;
	struct ksz_desc_info *info = &hw->rx_desc_info;

	for (i = 0; i < hw->rx_desc_info.alloc; i++) {
		get_rx_pkt(info, &desc);

		dma_buf = DMA_BUFFER(desc);
		if (dma_buf->skb && dma_buf->len != adapter->mtu)
			free_dma_buf(adapter, dma_buf, PCI_DMA_FROMDEVICE);
		dma_buf->len = adapter->mtu;
		if (!dma_buf->skb)
			dma_buf->skb = alloc_skb(dma_buf->len, GFP_ATOMIC);
		if (dma_buf->skb && !dma_buf->dma) {
			dma_buf->skb->dev = adapter->dev;
			dma_buf->dma = pci_map_single(
				adapter->pdev,
				skb_tail_pointer(dma_buf->skb),
				dma_buf->len,
				PCI_DMA_FROMDEVICE);
		}

		
		set_rx_buf(desc, dma_buf->dma);
		set_rx_len(desc, dma_buf->len);
		release_desc(desc);
	}
}

static int ksz_alloc_mem(struct dev_info *adapter)
{
	struct ksz_hw *hw = &adapter->hw;

	
	hw->rx_desc_info.alloc = NUM_OF_RX_DESC;
	hw->tx_desc_info.alloc = NUM_OF_TX_DESC;

	
	hw->tx_int_cnt = 0;
	hw->tx_int_mask = NUM_OF_TX_DESC / 4;
	if (hw->tx_int_mask > 8)
		hw->tx_int_mask = 8;
	while (hw->tx_int_mask) {
		hw->tx_int_cnt++;
		hw->tx_int_mask >>= 1;
	}
	if (hw->tx_int_cnt) {
		hw->tx_int_mask = (1 << (hw->tx_int_cnt - 1)) - 1;
		hw->tx_int_cnt = 0;
	}

	
	hw->rx_desc_info.size =
		(((sizeof(struct ksz_hw_desc) + DESC_ALIGNMENT - 1) /
		DESC_ALIGNMENT) * DESC_ALIGNMENT);
	hw->tx_desc_info.size =
		(((sizeof(struct ksz_hw_desc) + DESC_ALIGNMENT - 1) /
		DESC_ALIGNMENT) * DESC_ALIGNMENT);
	if (hw->rx_desc_info.size != sizeof(struct ksz_hw_desc))
		pr_alert("Hardware descriptor size not right!\n");
	ksz_check_desc_num(&hw->rx_desc_info);
	ksz_check_desc_num(&hw->tx_desc_info);

	
	if (ksz_alloc_desc(adapter))
		return 1;

	return 0;
}

static void ksz_free_desc(struct dev_info *adapter)
{
	struct ksz_hw *hw = &adapter->hw;

	
	hw->rx_desc_info.ring_virt = NULL;
	hw->tx_desc_info.ring_virt = NULL;
	hw->rx_desc_info.ring_phys = 0;
	hw->tx_desc_info.ring_phys = 0;

	
	if (adapter->desc_pool.alloc_virt)
		pci_free_consistent(
			adapter->pdev,
			adapter->desc_pool.alloc_size,
			adapter->desc_pool.alloc_virt,
			adapter->desc_pool.dma_addr);

	
	adapter->desc_pool.alloc_size = 0;
	adapter->desc_pool.alloc_virt = NULL;

	kfree(hw->rx_desc_info.ring);
	hw->rx_desc_info.ring = NULL;
	kfree(hw->tx_desc_info.ring);
	hw->tx_desc_info.ring = NULL;
}

static void ksz_free_buffers(struct dev_info *adapter,
	struct ksz_desc_info *desc_info, int direction)
{
	int i;
	struct ksz_dma_buf *dma_buf;
	struct ksz_desc *desc = desc_info->ring;

	for (i = 0; i < desc_info->alloc; i++) {
		dma_buf = DMA_BUFFER(desc);
		if (dma_buf->skb)
			free_dma_buf(adapter, dma_buf, direction);
		desc++;
	}
}

static void ksz_free_mem(struct dev_info *adapter)
{
	
	ksz_free_buffers(adapter, &adapter->hw.tx_desc_info,
		PCI_DMA_TODEVICE);

	
	ksz_free_buffers(adapter, &adapter->hw.rx_desc_info,
		PCI_DMA_FROMDEVICE);

	
	ksz_free_desc(adapter);
}

static void get_mib_counters(struct ksz_hw *hw, int first, int cnt,
	u64 *counter)
{
	int i;
	int mib;
	int port;
	struct ksz_port_mib *port_mib;

	memset(counter, 0, sizeof(u64) * TOTAL_PORT_COUNTER_NUM);
	for (i = 0, port = first; i < cnt; i++, port++) {
		port_mib = &hw->port_mib[port];
		for (mib = port_mib->mib_start; mib < hw->mib_cnt; mib++)
			counter[mib] += port_mib->counter[mib];
	}
}

static void send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct ksz_desc *desc;
	struct ksz_desc *first;
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_desc_info *info = &hw->tx_desc_info;
	struct ksz_dma_buf *dma_buf;
	int len;
	int last_frag = skb_shinfo(skb)->nr_frags;

	if (hw->dev_count > 1)
		hw->dst_ports = 1 << priv->port.first_port;

	
	len = skb->len;

	
	first = info->cur;
	desc = first;

	dma_buf = DMA_BUFFER(desc);
	if (last_frag) {
		int frag;
		skb_frag_t *this_frag;

		dma_buf->len = skb_headlen(skb);

		dma_buf->dma = pci_map_single(
			hw_priv->pdev, skb->data, dma_buf->len,
			PCI_DMA_TODEVICE);
		set_tx_buf(desc, dma_buf->dma);
		set_tx_len(desc, dma_buf->len);

		frag = 0;
		do {
			this_frag = &skb_shinfo(skb)->frags[frag];

			
			get_tx_pkt(info, &desc);

			
			++hw->tx_int_cnt;

			dma_buf = DMA_BUFFER(desc);
			dma_buf->len = skb_frag_size(this_frag);

			dma_buf->dma = pci_map_single(
				hw_priv->pdev,
				skb_frag_address(this_frag),
				dma_buf->len,
				PCI_DMA_TODEVICE);
			set_tx_buf(desc, dma_buf->dma);
			set_tx_len(desc, dma_buf->len);

			frag++;
			if (frag == last_frag)
				break;

			
			release_desc(desc);
		} while (1);

		
		info->cur = desc;

		
		release_desc(first);
	} else {
		dma_buf->len = len;

		dma_buf->dma = pci_map_single(
			hw_priv->pdev, skb->data, dma_buf->len,
			PCI_DMA_TODEVICE);
		set_tx_buf(desc, dma_buf->dma);
		set_tx_len(desc, dma_buf->len);
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		(desc)->sw.buf.tx.csum_gen_tcp = 1;
		(desc)->sw.buf.tx.csum_gen_udp = 1;
	}

	dma_buf->skb = skb;

	hw_send_pkt(hw);

	
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += len;
}

static void transmit_cleanup(struct dev_info *hw_priv, int normal)
{
	int last;
	union desc_stat status;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_desc_info *info = &hw->tx_desc_info;
	struct ksz_desc *desc;
	struct ksz_dma_buf *dma_buf;
	struct net_device *dev = NULL;

	spin_lock(&hw_priv->hwlock);
	last = info->last;

	while (info->avail < info->alloc) {
		
		desc = &info->ring[last];
		status.data = le32_to_cpu(desc->phw->ctrl.data);
		if (status.tx.hw_owned) {
			if (normal)
				break;
			else
				reset_desc(desc, status);
		}

		dma_buf = DMA_BUFFER(desc);
		pci_unmap_single(
			hw_priv->pdev, dma_buf->dma, dma_buf->len,
			PCI_DMA_TODEVICE);

		
		if (dma_buf->skb) {
			dev = dma_buf->skb->dev;

			
			dev_kfree_skb_irq(dma_buf->skb);
			dma_buf->skb = NULL;
		}

		
		last++;
		last &= info->mask;
		info->avail++;
	}
	info->last = last;
	spin_unlock(&hw_priv->hwlock);

	
	if (dev)
		dev->trans_start = jiffies;
}

static void tx_done(struct dev_info *hw_priv)
{
	struct ksz_hw *hw = &hw_priv->hw;
	int port;

	transmit_cleanup(hw_priv, 1);

	for (port = 0; port < hw->dev_count; port++) {
		struct net_device *dev = hw->port_info[port].pdev;

		if (netif_running(dev) && netif_queue_stopped(dev))
			netif_wake_queue(dev);
	}
}

static inline void copy_old_skb(struct sk_buff *old, struct sk_buff *skb)
{
	skb->dev = old->dev;
	skb->protocol = old->protocol;
	skb->ip_summed = old->ip_summed;
	skb->csum = old->csum;
	skb_set_network_header(skb, ETH_HLEN);

	dev_kfree_skb(old);
}

static netdev_tx_t netdev_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	int left;
	int num = 1;
	int rc = 0;

	if (hw->features & SMALL_PACKET_TX_BUG) {
		struct sk_buff *org_skb = skb;

		if (skb->len <= 48) {
			if (skb_end_pointer(skb) - skb->data >= 50) {
				memset(&skb->data[skb->len], 0, 50 - skb->len);
				skb->len = 50;
			} else {
				skb = netdev_alloc_skb(dev, 50);
				if (!skb)
					return NETDEV_TX_BUSY;
				memcpy(skb->data, org_skb->data, org_skb->len);
				memset(&skb->data[org_skb->len], 0,
					50 - org_skb->len);
				skb->len = 50;
				copy_old_skb(org_skb, skb);
			}
		}
	}

	spin_lock_irq(&hw_priv->hwlock);

	num = skb_shinfo(skb)->nr_frags + 1;
	left = hw_alloc_pkt(hw, skb->len, num);
	if (left) {
		if (left < num ||
				((CHECKSUM_PARTIAL == skb->ip_summed) &&
				(ETH_P_IPV6 == htons(skb->protocol)))) {
			struct sk_buff *org_skb = skb;

			skb = netdev_alloc_skb(dev, org_skb->len);
			if (!skb) {
				rc = NETDEV_TX_BUSY;
				goto unlock;
			}
			skb_copy_and_csum_dev(org_skb, skb->data);
			org_skb->ip_summed = CHECKSUM_NONE;
			skb->len = org_skb->len;
			copy_old_skb(org_skb, skb);
		}
		send_packet(skb, dev);
		if (left <= num)
			netif_stop_queue(dev);
	} else {
		
		netif_stop_queue(dev);
		rc = NETDEV_TX_BUSY;
	}
unlock:
	spin_unlock_irq(&hw_priv->hwlock);

	return rc;
}

static void netdev_tx_timeout(struct net_device *dev)
{
	static unsigned long last_reset;

	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	int port;

	if (hw->dev_count > 1) {
		if (jiffies - last_reset <= dev->watchdog_timeo)
			hw_priv = NULL;
	}

	last_reset = jiffies;
	if (hw_priv) {
		hw_dis_intr(hw);
		hw_disable(hw);

		transmit_cleanup(hw_priv, 0);
		hw_reset_pkts(&hw->rx_desc_info);
		hw_reset_pkts(&hw->tx_desc_info);
		ksz_init_rx_buffers(hw_priv);

		hw_reset(hw);

		hw_set_desc_base(hw,
			hw->tx_desc_info.ring_phys,
			hw->rx_desc_info.ring_phys);
		hw_set_addr(hw);
		if (hw->all_multi)
			hw_set_multicast(hw, hw->all_multi);
		else if (hw->multi_list_size)
			hw_set_grp_addr(hw);

		if (hw->dev_count > 1) {
			hw_set_add_addr(hw);
			for (port = 0; port < SWITCH_PORT_NUM; port++) {
				struct net_device *port_dev;

				port_set_stp_state(hw, port,
					STP_STATE_DISABLED);

				port_dev = hw->port_info[port].pdev;
				if (netif_running(port_dev))
					port_set_stp_state(hw, port,
						STP_STATE_SIMPLE);
			}
		}

		hw_enable(hw);
		hw_ena_intr(hw);
	}

	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}

static inline void csum_verified(struct sk_buff *skb)
{
	unsigned short protocol;
	struct iphdr *iph;

	protocol = skb->protocol;
	skb_reset_network_header(skb);
	iph = (struct iphdr *) skb_network_header(skb);
	if (protocol == htons(ETH_P_8021Q)) {
		protocol = iph->tot_len;
		skb_set_network_header(skb, VLAN_HLEN);
		iph = (struct iphdr *) skb_network_header(skb);
	}
	if (protocol == htons(ETH_P_IP)) {
		if (iph->protocol == IPPROTO_TCP)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
	}
}

static inline int rx_proc(struct net_device *dev, struct ksz_hw* hw,
	struct ksz_desc *desc, union desc_stat status)
{
	int packet_len;
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_dma_buf *dma_buf;
	struct sk_buff *skb;
	int rx_status;

	
	packet_len = status.rx.frame_len - 4;

	dma_buf = DMA_BUFFER(desc);
	pci_dma_sync_single_for_cpu(
		hw_priv->pdev, dma_buf->dma, packet_len + 4,
		PCI_DMA_FROMDEVICE);

	do {
		
		skb = netdev_alloc_skb(dev, packet_len + 2);
		if (!skb) {
			dev->stats.rx_dropped++;
			return -ENOMEM;
		}

		skb_reserve(skb, 2);

		memcpy(skb_put(skb, packet_len),
			dma_buf->skb->data, packet_len);
	} while (0);

	skb->protocol = eth_type_trans(skb, dev);

	if (hw->rx_cfg & (DMA_RX_CSUM_UDP | DMA_RX_CSUM_TCP))
		csum_verified(skb);

	
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += packet_len;

	
	rx_status = netif_rx(skb);

	return 0;
}

static int dev_rcv_packets(struct dev_info *hw_priv)
{
	int next;
	union desc_stat status;
	struct ksz_hw *hw = &hw_priv->hw;
	struct net_device *dev = hw->port_info[0].pdev;
	struct ksz_desc_info *info = &hw->rx_desc_info;
	int left = info->alloc;
	struct ksz_desc *desc;
	int received = 0;

	next = info->next;
	while (left--) {
		
		desc = &info->ring[next];
		status.data = le32_to_cpu(desc->phw->ctrl.data);
		if (status.rx.hw_owned)
			break;

		
		if (status.rx.last_desc && status.rx.first_desc) {
			if (rx_proc(dev, hw, desc, status))
				goto release_packet;
			received++;
		}

release_packet:
		release_desc(desc);
		next++;
		next &= info->mask;
	}
	info->next = next;

	return received;
}

static int port_rcv_packets(struct dev_info *hw_priv)
{
	int next;
	union desc_stat status;
	struct ksz_hw *hw = &hw_priv->hw;
	struct net_device *dev = hw->port_info[0].pdev;
	struct ksz_desc_info *info = &hw->rx_desc_info;
	int left = info->alloc;
	struct ksz_desc *desc;
	int received = 0;

	next = info->next;
	while (left--) {
		
		desc = &info->ring[next];
		status.data = le32_to_cpu(desc->phw->ctrl.data);
		if (status.rx.hw_owned)
			break;

		if (hw->dev_count > 1) {
			
			int p = HW_TO_DEV_PORT(status.rx.src_port);

			dev = hw->port_info[p].pdev;
			if (!netif_running(dev))
				goto release_packet;
		}

		
		if (status.rx.last_desc && status.rx.first_desc) {
			if (rx_proc(dev, hw, desc, status))
				goto release_packet;
			received++;
		}

release_packet:
		release_desc(desc);
		next++;
		next &= info->mask;
	}
	info->next = next;

	return received;
}

static int dev_rcv_special(struct dev_info *hw_priv)
{
	int next;
	union desc_stat status;
	struct ksz_hw *hw = &hw_priv->hw;
	struct net_device *dev = hw->port_info[0].pdev;
	struct ksz_desc_info *info = &hw->rx_desc_info;
	int left = info->alloc;
	struct ksz_desc *desc;
	int received = 0;

	next = info->next;
	while (left--) {
		
		desc = &info->ring[next];
		status.data = le32_to_cpu(desc->phw->ctrl.data);
		if (status.rx.hw_owned)
			break;

		if (hw->dev_count > 1) {
			
			int p = HW_TO_DEV_PORT(status.rx.src_port);

			dev = hw->port_info[p].pdev;
			if (!netif_running(dev))
				goto release_packet;
		}

		
		if (status.rx.last_desc && status.rx.first_desc) {
			if (!status.rx.error || (status.data &
					KS_DESC_RX_ERROR_COND) ==
					KS_DESC_RX_ERROR_TOO_LONG) {
				if (rx_proc(dev, hw, desc, status))
					goto release_packet;
				received++;
			} else {
				struct dev_priv *priv = netdev_priv(dev);

				
				priv->port.counter[OID_COUNTER_RCV_ERROR]++;
			}
		}

release_packet:
		release_desc(desc);
		next++;
		next &= info->mask;
	}
	info->next = next;

	return received;
}

static void rx_proc_task(unsigned long data)
{
	struct dev_info *hw_priv = (struct dev_info *) data;
	struct ksz_hw *hw = &hw_priv->hw;

	if (!hw->enabled)
		return;
	if (unlikely(!hw_priv->dev_rcv(hw_priv))) {

		
		hw_resume_rx(hw);

		
		spin_lock_irq(&hw_priv->hwlock);
		hw_turn_on_intr(hw, KS884X_INT_RX_MASK);
		spin_unlock_irq(&hw_priv->hwlock);
	} else {
		hw_ack_intr(hw, KS884X_INT_RX);
		tasklet_schedule(&hw_priv->rx_tasklet);
	}
}

static void tx_proc_task(unsigned long data)
{
	struct dev_info *hw_priv = (struct dev_info *) data;
	struct ksz_hw *hw = &hw_priv->hw;

	hw_ack_intr(hw, KS884X_INT_TX_MASK);

	tx_done(hw_priv);

	
	spin_lock_irq(&hw_priv->hwlock);
	hw_turn_on_intr(hw, KS884X_INT_TX);
	spin_unlock_irq(&hw_priv->hwlock);
}

static inline void handle_rx_stop(struct ksz_hw *hw)
{
	
	if (0 == hw->rx_stop)
		hw->intr_mask &= ~KS884X_INT_RX_STOPPED;
	else if (hw->rx_stop > 1) {
		if (hw->enabled && (hw->rx_cfg & DMA_RX_ENABLE)) {
			hw_start_rx(hw);
		} else {
			hw->intr_mask &= ~KS884X_INT_RX_STOPPED;
			hw->rx_stop = 0;
		}
	} else
		
		hw->rx_stop++;
}

static irqreturn_t netdev_intr(int irq, void *dev_id)
{
	uint int_enable = 0;
	struct net_device *dev = (struct net_device *) dev_id;
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;

	hw_read_intr(hw, &int_enable);

	
	if (!int_enable)
		return IRQ_NONE;

	do {
		hw_ack_intr(hw, int_enable);
		int_enable &= hw->intr_mask;

		if (unlikely(int_enable & KS884X_INT_TX_MASK)) {
			hw_dis_intr_bit(hw, KS884X_INT_TX_MASK);
			tasklet_schedule(&hw_priv->tx_tasklet);
		}

		if (likely(int_enable & KS884X_INT_RX)) {
			hw_dis_intr_bit(hw, KS884X_INT_RX);
			tasklet_schedule(&hw_priv->rx_tasklet);
		}

		if (unlikely(int_enable & KS884X_INT_RX_OVERRUN)) {
			dev->stats.rx_fifo_errors++;
			hw_resume_rx(hw);
		}

		if (unlikely(int_enable & KS884X_INT_PHY)) {
			struct ksz_port *port = &priv->port;

			hw->features |= LINK_INT_WORKING;
			port_get_link_speed(port);
		}

		if (unlikely(int_enable & KS884X_INT_RX_STOPPED)) {
			handle_rx_stop(hw);
			break;
		}

		if (unlikely(int_enable & KS884X_INT_TX_STOPPED)) {
			u32 data;

			hw->intr_mask &= ~KS884X_INT_TX_STOPPED;
			pr_info("Tx stopped\n");
			data = readl(hw->io + KS_DMA_TX_CTRL);
			if (!(data & DMA_TX_ENABLE))
				pr_info("Tx disabled\n");
			break;
		}
	} while (0);

	hw_ena_intr(hw);

	return IRQ_HANDLED;
}


static unsigned long next_jiffies;

#ifdef CONFIG_NET_POLL_CONTROLLER
static void netdev_netpoll(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;

	hw_dis_intr(&hw_priv->hw);
	netdev_intr(dev->irq, dev);
}
#endif

static void bridge_change(struct ksz_hw *hw)
{
	int port;
	u8  member;
	struct ksz_switch *sw = hw->ksz_switch;

	
	if (!sw->member) {
		port_set_stp_state(hw, SWITCH_PORT_NUM, STP_STATE_SIMPLE);
		sw_block_addr(hw);
	}
	for (port = 0; port < SWITCH_PORT_NUM; port++) {
		if (STP_STATE_FORWARDING == sw->port_cfg[port].stp_state)
			member = HOST_MASK | sw->member;
		else
			member = HOST_MASK | (1 << port);
		if (member != sw->port_cfg[port].member)
			sw_cfg_port_base_vlan(hw, port, member);
	}
}

static int netdev_close(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_port *port = &priv->port;
	struct ksz_hw *hw = &hw_priv->hw;
	int pi;

	netif_stop_queue(dev);

	ksz_stop_timer(&priv->monitor_timer_info);

	
	if (hw->dev_count > 1) {
		port_set_stp_state(hw, port->first_port, STP_STATE_DISABLED);

		
		if (hw->features & STP_SUPPORT) {
			pi = 1 << port->first_port;
			if (hw->ksz_switch->member & pi) {
				hw->ksz_switch->member &= ~pi;
				bridge_change(hw);
			}
		}
	}
	if (port->first_port > 0)
		hw_del_addr(hw, dev->dev_addr);
	if (!hw_priv->wol_enable)
		port_set_power_saving(port, true);

	if (priv->multicast)
		--hw->all_multi;
	if (priv->promiscuous)
		--hw->promiscuous;

	hw_priv->opened--;
	if (!(hw_priv->opened)) {
		ksz_stop_timer(&hw_priv->mib_timer_info);
		flush_work(&hw_priv->mib_read);

		hw_dis_intr(hw);
		hw_disable(hw);
		hw_clr_multicast(hw);

		
		msleep(2000 / HZ);

		tasklet_disable(&hw_priv->rx_tasklet);
		tasklet_disable(&hw_priv->tx_tasklet);
		free_irq(dev->irq, hw_priv->dev);

		transmit_cleanup(hw_priv, 0);
		hw_reset_pkts(&hw->rx_desc_info);
		hw_reset_pkts(&hw->tx_desc_info);

		
		if (hw->features & STP_SUPPORT)
			sw_clr_sta_mac_table(hw);
	}

	return 0;
}

static void hw_cfg_huge_frame(struct dev_info *hw_priv, struct ksz_hw *hw)
{
	if (hw->ksz_switch) {
		u32 data;

		data = readw(hw->io + KS8842_SWITCH_CTRL_2_OFFSET);
		if (hw->features & RX_HUGE_FRAME)
			data |= SWITCH_HUGE_PACKET;
		else
			data &= ~SWITCH_HUGE_PACKET;
		writew(data, hw->io + KS8842_SWITCH_CTRL_2_OFFSET);
	}
	if (hw->features & RX_HUGE_FRAME) {
		hw->rx_cfg |= DMA_RX_ERROR;
		hw_priv->dev_rcv = dev_rcv_special;
	} else {
		hw->rx_cfg &= ~DMA_RX_ERROR;
		if (hw->dev_count > 1)
			hw_priv->dev_rcv = port_rcv_packets;
		else
			hw_priv->dev_rcv = dev_rcv_packets;
	}
}

static int prepare_hardware(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	int rc = 0;

	
	hw_priv->dev = dev;
	rc = request_irq(dev->irq, netdev_intr, IRQF_SHARED, dev->name, dev);
	if (rc)
		return rc;
	tasklet_enable(&hw_priv->rx_tasklet);
	tasklet_enable(&hw_priv->tx_tasklet);

	hw->promiscuous = 0;
	hw->all_multi = 0;
	hw->multi_list_size = 0;

	hw_reset(hw);

	hw_set_desc_base(hw,
		hw->tx_desc_info.ring_phys, hw->rx_desc_info.ring_phys);
	hw_set_addr(hw);
	hw_cfg_huge_frame(hw_priv, hw);
	ksz_init_rx_buffers(hw_priv);
	return 0;
}

static void set_media_state(struct net_device *dev, int media_state)
{
	struct dev_priv *priv = netdev_priv(dev);

	if (media_state == priv->media_state)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);
	netif_info(priv, link, dev, "link %s\n",
		   media_state == priv->media_state ? "on" : "off");
}

static int netdev_open(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_port *port = &priv->port;
	int i;
	int p;
	int rc = 0;

	priv->multicast = 0;
	priv->promiscuous = 0;

	
	memset(&dev->stats, 0, sizeof(struct net_device_stats));
	memset((void *) port->counter, 0,
		(sizeof(u64) * OID_COUNTER_LAST));

	if (!(hw_priv->opened)) {
		rc = prepare_hardware(dev);
		if (rc)
			return rc;
		for (i = 0; i < hw->mib_port_cnt; i++) {
			if (next_jiffies < jiffies)
				next_jiffies = jiffies + HZ * 2;
			else
				next_jiffies += HZ * 1;
			hw_priv->counter[i].time = next_jiffies;
			hw->port_mib[i].state = media_disconnected;
			port_init_cnt(hw, i);
		}
		if (hw->ksz_switch)
			hw->port_mib[HOST_PORT].state = media_connected;
		else {
			hw_add_wol_bcast(hw);
			hw_cfg_wol_pme(hw, 0);
			hw_clr_wol_pme_status(&hw_priv->hw);
		}
	}
	port_set_power_saving(port, false);

	for (i = 0, p = port->first_port; i < port->port_cnt; i++, p++) {
		hw->port_info[p].partner = 0xFF;
		hw->port_info[p].state = media_disconnected;
	}

	
	if (hw->dev_count > 1) {
		port_set_stp_state(hw, port->first_port, STP_STATE_SIMPLE);
		if (port->first_port > 0)
			hw_add_addr(hw, dev->dev_addr);
	}

	port_get_link_speed(port);
	if (port->force_link)
		port_force_link_speed(port);
	else
		port_set_link_speed(port);

	if (!(hw_priv->opened)) {
		hw_setup_intr(hw);
		hw_enable(hw);
		hw_ena_intr(hw);

		if (hw->mib_port_cnt)
			ksz_start_timer(&hw_priv->mib_timer_info,
				hw_priv->mib_timer_info.period);
	}

	hw_priv->opened++;

	ksz_start_timer(&priv->monitor_timer_info,
		priv->monitor_timer_info.period);

	priv->media_state = port->linked->state;

	set_media_state(dev, media_connected);
	netif_start_queue(dev);

	return 0;
}


static struct net_device_stats *netdev_query_statistics(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct ksz_port *port = &priv->port;
	struct ksz_hw *hw = &priv->adapter->hw;
	struct ksz_port_mib *mib;
	int i;
	int p;

	dev->stats.rx_errors = port->counter[OID_COUNTER_RCV_ERROR];
	dev->stats.tx_errors = port->counter[OID_COUNTER_XMIT_ERROR];

	
	dev->stats.multicast = 0;
	dev->stats.collisions = 0;
	dev->stats.rx_length_errors = 0;
	dev->stats.rx_crc_errors = 0;
	dev->stats.rx_frame_errors = 0;
	dev->stats.tx_window_errors = 0;

	for (i = 0, p = port->first_port; i < port->mib_port_cnt; i++, p++) {
		mib = &hw->port_mib[p];

		dev->stats.multicast += (unsigned long)
			mib->counter[MIB_COUNTER_RX_MULTICAST];

		dev->stats.collisions += (unsigned long)
			mib->counter[MIB_COUNTER_TX_TOTAL_COLLISION];

		dev->stats.rx_length_errors += (unsigned long)(
			mib->counter[MIB_COUNTER_RX_UNDERSIZE] +
			mib->counter[MIB_COUNTER_RX_FRAGMENT] +
			mib->counter[MIB_COUNTER_RX_OVERSIZE] +
			mib->counter[MIB_COUNTER_RX_JABBER]);
		dev->stats.rx_crc_errors += (unsigned long)
			mib->counter[MIB_COUNTER_RX_CRC_ERR];
		dev->stats.rx_frame_errors += (unsigned long)(
			mib->counter[MIB_COUNTER_RX_ALIGNMENT_ERR] +
			mib->counter[MIB_COUNTER_RX_SYMBOL_ERR]);

		dev->stats.tx_window_errors += (unsigned long)
			mib->counter[MIB_COUNTER_TX_LATE_COLLISION];
	}

	return &dev->stats;
}

static int netdev_set_mac_address(struct net_device *dev, void *addr)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct sockaddr *mac = addr;
	uint interrupt;

	if (priv->port.first_port > 0)
		hw_del_addr(hw, dev->dev_addr);
	else {
		hw->mac_override = 1;
		memcpy(hw->override_addr, mac->sa_data, ETH_ALEN);
	}

	memcpy(dev->dev_addr, mac->sa_data, ETH_ALEN);

	interrupt = hw_block_intr(hw);

	if (priv->port.first_port > 0)
		hw_add_addr(hw, dev->dev_addr);
	else
		hw_set_addr(hw);
	hw_restore_intr(hw, interrupt);

	return 0;
}

static void dev_set_promiscuous(struct net_device *dev, struct dev_priv *priv,
	struct ksz_hw *hw, int promiscuous)
{
	if (promiscuous != priv->promiscuous) {
		u8 prev_state = hw->promiscuous;

		if (promiscuous)
			++hw->promiscuous;
		else
			--hw->promiscuous;
		priv->promiscuous = promiscuous;

		
		if (hw->promiscuous <= 1 && prev_state <= 1)
			hw_set_promiscuous(hw, hw->promiscuous);

		if ((hw->features & STP_SUPPORT) && !promiscuous &&
		    (dev->priv_flags & IFF_BRIDGE_PORT)) {
			struct ksz_switch *sw = hw->ksz_switch;
			int port = priv->port.first_port;

			port_set_stp_state(hw, port, STP_STATE_DISABLED);
			port = 1 << port;
			if (sw->member & port) {
				sw->member &= ~port;
				bridge_change(hw);
			}
		}
	}
}

static void dev_set_multicast(struct dev_priv *priv, struct ksz_hw *hw,
	int multicast)
{
	if (multicast != priv->multicast) {
		u8 all_multi = hw->all_multi;

		if (multicast)
			++hw->all_multi;
		else
			--hw->all_multi;
		priv->multicast = multicast;

		
		if (hw->all_multi <= 1 && all_multi <= 1)
			hw_set_multicast(hw, hw->all_multi);
	}
}

static void netdev_set_rx_mode(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct netdev_hw_addr *ha;
	int multicast = (dev->flags & IFF_ALLMULTI);

	dev_set_promiscuous(dev, priv, hw, (dev->flags & IFF_PROMISC));

	if (hw_priv->hw.dev_count > 1)
		multicast |= (dev->flags & IFF_MULTICAST);
	dev_set_multicast(priv, hw, multicast);

	
	if (hw_priv->hw.dev_count > 1)
		return;

	if ((dev->flags & IFF_MULTICAST) && !netdev_mc_empty(dev)) {
		int i = 0;

		
		if (netdev_mc_count(dev) > MAX_MULTICAST_LIST) {
			if (MAX_MULTICAST_LIST != hw->multi_list_size) {
				hw->multi_list_size = MAX_MULTICAST_LIST;
				++hw->all_multi;
				hw_set_multicast(hw, hw->all_multi);
			}
			return;
		}

		netdev_for_each_mc_addr(ha, dev) {
			if (i >= MAX_MULTICAST_LIST)
				break;
			memcpy(hw->multi_list[i++], ha->addr, ETH_ALEN);
		}
		hw->multi_list_size = (u8) i;
		hw_set_grp_addr(hw);
	} else {
		if (MAX_MULTICAST_LIST == hw->multi_list_size) {
			--hw->all_multi;
			hw_set_multicast(hw, hw->all_multi);
		}
		hw->multi_list_size = 0;
		hw_clr_multicast(hw);
	}
}

static int netdev_change_mtu(struct net_device *dev, int new_mtu)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	int hw_mtu;

	if (netif_running(dev))
		return -EBUSY;

	
	if (hw->dev_count > 1)
		if (dev != hw_priv->dev)
			return 0;
	if (new_mtu < 60)
		return -EINVAL;

	if (dev->mtu != new_mtu) {
		hw_mtu = new_mtu + ETHERNET_HEADER_SIZE + 4;
		if (hw_mtu > MAX_RX_BUF_SIZE)
			return -EINVAL;
		if (hw_mtu > REGULAR_RX_BUF_SIZE) {
			hw->features |= RX_HUGE_FRAME;
			hw_mtu = MAX_RX_BUF_SIZE;
		} else {
			hw->features &= ~RX_HUGE_FRAME;
			hw_mtu = REGULAR_RX_BUF_SIZE;
		}
		hw_mtu = (hw_mtu + 3) & ~3;
		hw_priv->mtu = hw_mtu;
		dev->mtu = new_mtu;
	}
	return 0;
}

static int netdev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_port *port = &priv->port;
	int rc;
	int result = 0;
	struct mii_ioctl_data *data = if_mii(ifr);

	if (down_interruptible(&priv->proc_sem))
		return -ERESTARTSYS;

	
	rc = 0;
	switch (cmd) {
	
	case SIOCGMIIPHY:
		data->phy_id = priv->id;

		

	
	case SIOCGMIIREG:
		if (data->phy_id != priv->id || data->reg_num >= 6)
			result = -EIO;
		else
			hw_r_phy(hw, port->linked->port_id, data->reg_num,
				&data->val_out);
		break;

	
	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN))
			result = -EPERM;
		else if (data->phy_id != priv->id || data->reg_num >= 6)
			result = -EIO;
		else
			hw_w_phy(hw, port->linked->port_id, data->reg_num,
				data->val_in);
		break;

	default:
		result = -EOPNOTSUPP;
	}

	up(&priv->proc_sem);

	return result;
}


static int mdio_read(struct net_device *dev, int phy_id, int reg_num)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct ksz_port *port = &priv->port;
	struct ksz_hw *hw = port->hw;
	u16 val_out;

	hw_r_phy(hw, port->linked->port_id, reg_num << 1, &val_out);
	return val_out;
}

static void mdio_write(struct net_device *dev, int phy_id, int reg_num, int val)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct ksz_port *port = &priv->port;
	struct ksz_hw *hw = port->hw;
	int i;
	int pi;

	for (i = 0, pi = port->first_port; i < port->port_cnt; i++, pi++)
		hw_w_phy(hw, pi, reg_num << 1, val);
}


#define EEPROM_SIZE			0x40

static u16 eeprom_data[EEPROM_SIZE] = { 0 };

#define ADVERTISED_ALL			\
	(ADVERTISED_10baseT_Half |	\
	ADVERTISED_10baseT_Full |	\
	ADVERTISED_100baseT_Half |	\
	ADVERTISED_100baseT_Full)


static int netdev_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;

	mutex_lock(&hw_priv->lock);
	mii_ethtool_gset(&priv->mii_if, cmd);
	cmd->advertising |= SUPPORTED_TP;
	mutex_unlock(&hw_priv->lock);

	
	priv->advertising = cmd->advertising;
	return 0;
}

static int netdev_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_port *port = &priv->port;
	u32 speed = ethtool_cmd_speed(cmd);
	int rc;

	if (cmd->autoneg && priv->advertising == cmd->advertising) {
		cmd->advertising |= ADVERTISED_ALL;
		if (10 == speed)
			cmd->advertising &=
				~(ADVERTISED_100baseT_Full |
				ADVERTISED_100baseT_Half);
		else if (100 == speed)
			cmd->advertising &=
				~(ADVERTISED_10baseT_Full |
				ADVERTISED_10baseT_Half);
		if (0 == cmd->duplex)
			cmd->advertising &=
				~(ADVERTISED_100baseT_Full |
				ADVERTISED_10baseT_Full);
		else if (1 == cmd->duplex)
			cmd->advertising &=
				~(ADVERTISED_100baseT_Half |
				ADVERTISED_10baseT_Half);
	}
	mutex_lock(&hw_priv->lock);
	if (cmd->autoneg &&
			(cmd->advertising & ADVERTISED_ALL) ==
			ADVERTISED_ALL) {
		port->duplex = 0;
		port->speed = 0;
		port->force_link = 0;
	} else {
		port->duplex = cmd->duplex + 1;
		if (1000 != speed)
			port->speed = speed;
		if (cmd->autoneg)
			port->force_link = 0;
		else
			port->force_link = 1;
	}
	rc = mii_ethtool_sset(&priv->mii_if, cmd);
	mutex_unlock(&hw_priv->lock);
	return rc;
}

static int netdev_nway_reset(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	int rc;

	mutex_lock(&hw_priv->lock);
	rc = mii_nway_restart(&priv->mii_if);
	mutex_unlock(&hw_priv->lock);
	return rc;
}

static u32 netdev_get_link(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);
	int rc;

	rc = mii_link_ok(&priv->mii_if);
	return rc;
}

static void netdev_get_drvinfo(struct net_device *dev,
	struct ethtool_drvinfo *info)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, pci_name(hw_priv->pdev),
		sizeof(info->bus_info));
}

static struct hw_regs {
	int start;
	int end;
} hw_regs_range[] = {
	{ KS_DMA_TX_CTRL,	KS884X_INTERRUPTS_STATUS },
	{ KS_ADD_ADDR_0_LO,	KS_ADD_ADDR_F_HI },
	{ KS884X_ADDR_0_OFFSET,	KS8841_WOL_FRAME_BYTE2_OFFSET },
	{ KS884X_SIDER_P,	KS8842_SGCR7_P },
	{ KS8842_MACAR1_P,	KS8842_TOSR8_P },
	{ KS884X_P1MBCR_P,	KS8842_P3ERCR_P },
	{ 0, 0 }
};

static int netdev_get_regs_len(struct net_device *dev)
{
	struct hw_regs *range = hw_regs_range;
	int regs_len = 0x10 * sizeof(u32);

	while (range->end > range->start) {
		regs_len += (range->end - range->start + 3) / 4 * 4;
		range++;
	}
	return regs_len;
}

static void netdev_get_regs(struct net_device *dev, struct ethtool_regs *regs,
	void *ptr)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	int *buf = (int *) ptr;
	struct hw_regs *range = hw_regs_range;
	int len;

	mutex_lock(&hw_priv->lock);
	regs->version = 0;
	for (len = 0; len < 0x40; len += 4) {
		pci_read_config_dword(hw_priv->pdev, len, buf);
		buf++;
	}
	while (range->end > range->start) {
		for (len = range->start; len < range->end; len += 4) {
			*buf = readl(hw->io + len);
			buf++;
		}
		range++;
	}
	mutex_unlock(&hw_priv->lock);
}

#define WOL_SUPPORT			\
	(WAKE_PHY | WAKE_MAGIC |	\
	WAKE_UCAST | WAKE_MCAST |	\
	WAKE_BCAST | WAKE_ARP)

static void netdev_get_wol(struct net_device *dev,
	struct ethtool_wolinfo *wol)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;

	wol->supported = hw_priv->wol_support;
	wol->wolopts = hw_priv->wol_enable;
	memset(&wol->sopass, 0, sizeof(wol->sopass));
}

static int netdev_set_wol(struct net_device *dev,
	struct ethtool_wolinfo *wol)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;

	
	static const u8 net_addr[] = { 192, 168, 1, 1 };

	if (wol->wolopts & ~hw_priv->wol_support)
		return -EINVAL;

	hw_priv->wol_enable = wol->wolopts;

	
	if (wol->wolopts)
		hw_priv->wol_enable |= WAKE_PHY;
	hw_enable_wol(&hw_priv->hw, hw_priv->wol_enable, net_addr);
	return 0;
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);

	return priv->msg_enable;
}

static void netdev_set_msglevel(struct net_device *dev, u32 value)
{
	struct dev_priv *priv = netdev_priv(dev);

	priv->msg_enable = value;
}

static int netdev_get_eeprom_len(struct net_device *dev)
{
	return EEPROM_SIZE * 2;
}

#define EEPROM_MAGIC			0x10A18842

static int netdev_get_eeprom(struct net_device *dev,
	struct ethtool_eeprom *eeprom, u8 *data)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	u8 *eeprom_byte = (u8 *) eeprom_data;
	int i;
	int len;

	len = (eeprom->offset + eeprom->len + 1) / 2;
	for (i = eeprom->offset / 2; i < len; i++)
		eeprom_data[i] = eeprom_read(&hw_priv->hw, i);
	eeprom->magic = EEPROM_MAGIC;
	memcpy(data, &eeprom_byte[eeprom->offset], eeprom->len);

	return 0;
}

static int netdev_set_eeprom(struct net_device *dev,
	struct ethtool_eeprom *eeprom, u8 *data)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	u16 eeprom_word[EEPROM_SIZE];
	u8 *eeprom_byte = (u8 *) eeprom_word;
	int i;
	int len;

	if (eeprom->magic != EEPROM_MAGIC)
		return -EINVAL;

	len = (eeprom->offset + eeprom->len + 1) / 2;
	for (i = eeprom->offset / 2; i < len; i++)
		eeprom_data[i] = eeprom_read(&hw_priv->hw, i);
	memcpy(eeprom_word, eeprom_data, EEPROM_SIZE * 2);
	memcpy(&eeprom_byte[eeprom->offset], data, eeprom->len);
	for (i = 0; i < EEPROM_SIZE; i++)
		if (eeprom_word[i] != eeprom_data[i]) {
			eeprom_data[i] = eeprom_word[i];
			eeprom_write(&hw_priv->hw, i, eeprom_data[i]);
	}

	return 0;
}

static void netdev_get_pauseparam(struct net_device *dev,
	struct ethtool_pauseparam *pause)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;

	pause->autoneg = (hw->overrides & PAUSE_FLOW_CTRL) ? 0 : 1;
	if (!hw->ksz_switch) {
		pause->rx_pause =
			(hw->rx_cfg & DMA_RX_FLOW_ENABLE) ? 1 : 0;
		pause->tx_pause =
			(hw->tx_cfg & DMA_TX_FLOW_ENABLE) ? 1 : 0;
	} else {
		pause->rx_pause =
			(sw_chk(hw, KS8842_SWITCH_CTRL_1_OFFSET,
				SWITCH_RX_FLOW_CTRL)) ? 1 : 0;
		pause->tx_pause =
			(sw_chk(hw, KS8842_SWITCH_CTRL_1_OFFSET,
				SWITCH_TX_FLOW_CTRL)) ? 1 : 0;
	}
}

static int netdev_set_pauseparam(struct net_device *dev,
	struct ethtool_pauseparam *pause)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_port *port = &priv->port;

	mutex_lock(&hw_priv->lock);
	if (pause->autoneg) {
		if (!pause->rx_pause && !pause->tx_pause)
			port->flow_ctrl = PHY_NO_FLOW_CTRL;
		else
			port->flow_ctrl = PHY_FLOW_CTRL;
		hw->overrides &= ~PAUSE_FLOW_CTRL;
		port->force_link = 0;
		if (hw->ksz_switch) {
			sw_cfg(hw, KS8842_SWITCH_CTRL_1_OFFSET,
				SWITCH_RX_FLOW_CTRL, 1);
			sw_cfg(hw, KS8842_SWITCH_CTRL_1_OFFSET,
				SWITCH_TX_FLOW_CTRL, 1);
		}
		port_set_link_speed(port);
	} else {
		hw->overrides |= PAUSE_FLOW_CTRL;
		if (hw->ksz_switch) {
			sw_cfg(hw, KS8842_SWITCH_CTRL_1_OFFSET,
				SWITCH_RX_FLOW_CTRL, pause->rx_pause);
			sw_cfg(hw, KS8842_SWITCH_CTRL_1_OFFSET,
				SWITCH_TX_FLOW_CTRL, pause->tx_pause);
		} else
			set_flow_ctrl(hw, pause->rx_pause, pause->tx_pause);
	}
	mutex_unlock(&hw_priv->lock);

	return 0;
}

static void netdev_get_ringparam(struct net_device *dev,
	struct ethtool_ringparam *ring)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;

	ring->tx_max_pending = (1 << 9);
	ring->tx_pending = hw->tx_desc_info.alloc;
	ring->rx_max_pending = (1 << 9);
	ring->rx_pending = hw->rx_desc_info.alloc;
}

#define STATS_LEN			(TOTAL_PORT_COUNTER_NUM)

static struct {
	char string[ETH_GSTRING_LEN];
} ethtool_stats_keys[STATS_LEN] = {
	{ "rx_lo_priority_octets" },
	{ "rx_hi_priority_octets" },
	{ "rx_undersize_packets" },
	{ "rx_fragments" },
	{ "rx_oversize_packets" },
	{ "rx_jabbers" },
	{ "rx_symbol_errors" },
	{ "rx_crc_errors" },
	{ "rx_align_errors" },
	{ "rx_mac_ctrl_packets" },
	{ "rx_pause_packets" },
	{ "rx_bcast_packets" },
	{ "rx_mcast_packets" },
	{ "rx_ucast_packets" },
	{ "rx_64_or_less_octet_packets" },
	{ "rx_65_to_127_octet_packets" },
	{ "rx_128_to_255_octet_packets" },
	{ "rx_256_to_511_octet_packets" },
	{ "rx_512_to_1023_octet_packets" },
	{ "rx_1024_to_1522_octet_packets" },

	{ "tx_lo_priority_octets" },
	{ "tx_hi_priority_octets" },
	{ "tx_late_collisions" },
	{ "tx_pause_packets" },
	{ "tx_bcast_packets" },
	{ "tx_mcast_packets" },
	{ "tx_ucast_packets" },
	{ "tx_deferred" },
	{ "tx_total_collisions" },
	{ "tx_excessive_collisions" },
	{ "tx_single_collisions" },
	{ "tx_mult_collisions" },

	{ "rx_discards" },
	{ "tx_discards" },
};

static void netdev_get_strings(struct net_device *dev, u32 stringset, u8 *buf)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;

	if (ETH_SS_STATS == stringset)
		memcpy(buf, &ethtool_stats_keys,
			ETH_GSTRING_LEN * hw->mib_cnt);
}

static int netdev_get_sset_count(struct net_device *dev, int sset)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;

	switch (sset) {
	case ETH_SS_STATS:
		return hw->mib_cnt;
	default:
		return -EOPNOTSUPP;
	}
}

static void netdev_get_ethtool_stats(struct net_device *dev,
	struct ethtool_stats *stats, u64 *data)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_port *port = &priv->port;
	int n_stats = stats->n_stats;
	int i;
	int n;
	int p;
	int rc;
	u64 counter[TOTAL_PORT_COUNTER_NUM];

	mutex_lock(&hw_priv->lock);
	n = SWITCH_PORT_NUM;
	for (i = 0, p = port->first_port; i < port->mib_port_cnt; i++, p++) {
		if (media_connected == hw->port_mib[p].state) {
			hw_priv->counter[p].read = 1;

			
			if (n == SWITCH_PORT_NUM)
				n = p;
		}
	}
	mutex_unlock(&hw_priv->lock);

	if (n < SWITCH_PORT_NUM)
		schedule_work(&hw_priv->mib_read);

	if (1 == port->mib_port_cnt && n < SWITCH_PORT_NUM) {
		p = n;
		rc = wait_event_interruptible_timeout(
			hw_priv->counter[p].counter,
			2 == hw_priv->counter[p].read,
			HZ * 1);
	} else
		for (i = 0, p = n; i < port->mib_port_cnt - n; i++, p++) {
			if (0 == i) {
				rc = wait_event_interruptible_timeout(
					hw_priv->counter[p].counter,
					2 == hw_priv->counter[p].read,
					HZ * 2);
			} else if (hw->port_mib[p].cnt_ptr) {
				rc = wait_event_interruptible_timeout(
					hw_priv->counter[p].counter,
					2 == hw_priv->counter[p].read,
					HZ * 1);
			}
		}

	get_mib_counters(hw, port->first_port, port->mib_port_cnt, counter);
	n = hw->mib_cnt;
	if (n > n_stats)
		n = n_stats;
	n_stats -= n;
	for (i = 0; i < n; i++)
		*data++ = counter[i];
}

static int netdev_set_features(struct net_device *dev,
	netdev_features_t features)
{
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;

	mutex_lock(&hw_priv->lock);

	
	if (features & NETIF_F_RXCSUM)
		hw->rx_cfg |= DMA_RX_CSUM_TCP | DMA_RX_CSUM_IP;
	else
		hw->rx_cfg &= ~(DMA_RX_CSUM_TCP | DMA_RX_CSUM_IP);

	if (hw->enabled)
		writel(hw->rx_cfg, hw->io + KS_DMA_RX_CTRL);

	mutex_unlock(&hw_priv->lock);

	return 0;
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_settings		= netdev_get_settings,
	.set_settings		= netdev_set_settings,
	.nway_reset		= netdev_nway_reset,
	.get_link		= netdev_get_link,
	.get_drvinfo		= netdev_get_drvinfo,
	.get_regs_len		= netdev_get_regs_len,
	.get_regs		= netdev_get_regs,
	.get_wol		= netdev_get_wol,
	.set_wol		= netdev_set_wol,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
	.get_eeprom_len		= netdev_get_eeprom_len,
	.get_eeprom		= netdev_get_eeprom,
	.set_eeprom		= netdev_set_eeprom,
	.get_pauseparam		= netdev_get_pauseparam,
	.set_pauseparam		= netdev_set_pauseparam,
	.get_ringparam		= netdev_get_ringparam,
	.get_strings		= netdev_get_strings,
	.get_sset_count		= netdev_get_sset_count,
	.get_ethtool_stats	= netdev_get_ethtool_stats,
};


static void update_link(struct net_device *dev, struct dev_priv *priv,
	struct ksz_port *port)
{
	if (priv->media_state != port->linked->state) {
		priv->media_state = port->linked->state;
		if (netif_running(dev))
			set_media_state(dev, media_connected);
	}
}

static void mib_read_work(struct work_struct *work)
{
	struct dev_info *hw_priv =
		container_of(work, struct dev_info, mib_read);
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_port_mib *mib;
	int i;

	next_jiffies = jiffies;
	for (i = 0; i < hw->mib_port_cnt; i++) {
		mib = &hw->port_mib[i];

		
		if (mib->cnt_ptr || 1 == hw_priv->counter[i].read) {

			
			if (port_r_cnt(hw, i))
				break;
			hw_priv->counter[i].read = 0;

			
			if (0 == mib->cnt_ptr) {
				hw_priv->counter[i].read = 2;
				wake_up_interruptible(
					&hw_priv->counter[i].counter);
			}
		} else if (jiffies >= hw_priv->counter[i].time) {
			
			if (media_connected == mib->state)
				hw_priv->counter[i].read = 1;
			next_jiffies += HZ * 1 * hw->mib_port_cnt;
			hw_priv->counter[i].time = next_jiffies;

		
		} else if (mib->link_down) {
			mib->link_down = 0;

			
			hw_priv->counter[i].read = 1;
		}
	}
}

static void mib_monitor(unsigned long ptr)
{
	struct dev_info *hw_priv = (struct dev_info *) ptr;

	mib_read_work(&hw_priv->mib_read);

	
	if (hw_priv->pme_wait) {
		if (hw_priv->pme_wait <= jiffies) {
			hw_clr_wol_pme_status(&hw_priv->hw);
			hw_priv->pme_wait = 0;
		}
	} else if (hw_chk_wol_pme_status(&hw_priv->hw)) {

		
		hw_priv->pme_wait = jiffies + HZ * 2;
	}

	ksz_update_timer(&hw_priv->mib_timer_info);
}

static void dev_monitor(unsigned long ptr)
{
	struct net_device *dev = (struct net_device *) ptr;
	struct dev_priv *priv = netdev_priv(dev);
	struct dev_info *hw_priv = priv->adapter;
	struct ksz_hw *hw = &hw_priv->hw;
	struct ksz_port *port = &priv->port;

	if (!(hw->features & LINK_INT_WORKING))
		port_get_link_speed(port);
	update_link(dev, priv, port);

	ksz_update_timer(&priv->monitor_timer_info);
}



static int msg_enable;

static char *macaddr = ":";
static char *mac1addr = ":";

static int multi_dev;

static int stp;

static int fast_aging;

static int __init netdev_init(struct net_device *dev)
{
	struct dev_priv *priv = netdev_priv(dev);

	
	ksz_init_timer(&priv->monitor_timer_info, 500 * HZ / 1000,
		dev_monitor, dev);

	
	dev->watchdog_timeo = HZ / 2;

	dev->hw_features = NETIF_F_IP_CSUM | NETIF_F_SG | NETIF_F_RXCSUM;

	dev->hw_features |= NETIF_F_IPV6_CSUM;

	dev->features |= dev->hw_features;

	sema_init(&priv->proc_sem, 1);

	priv->mii_if.phy_id_mask = 0x1;
	priv->mii_if.reg_num_mask = 0x7;
	priv->mii_if.dev = dev;
	priv->mii_if.mdio_read = mdio_read;
	priv->mii_if.mdio_write = mdio_write;
	priv->mii_if.phy_id = priv->port.first_port + 1;

	priv->msg_enable = netif_msg_init(msg_enable,
		(NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK));

	return 0;
}

static const struct net_device_ops netdev_ops = {
	.ndo_init		= netdev_init,
	.ndo_open		= netdev_open,
	.ndo_stop		= netdev_close,
	.ndo_get_stats		= netdev_query_statistics,
	.ndo_start_xmit		= netdev_tx,
	.ndo_tx_timeout		= netdev_tx_timeout,
	.ndo_change_mtu		= netdev_change_mtu,
	.ndo_set_features	= netdev_set_features,
	.ndo_set_mac_address	= netdev_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_do_ioctl		= netdev_ioctl,
	.ndo_set_rx_mode	= netdev_set_rx_mode,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= netdev_netpoll,
#endif
};

static void netdev_free(struct net_device *dev)
{
	if (dev->watchdog_timeo)
		unregister_netdev(dev);

	free_netdev(dev);
}

struct platform_info {
	struct dev_info dev_info;
	struct net_device *netdev[SWITCH_PORT_NUM];
};

static int net_device_present;

static void get_mac_addr(struct dev_info *hw_priv, u8 *macaddr, int port)
{
	int i;
	int j;
	int got_num;
	int num;

	i = j = num = got_num = 0;
	while (j < ETH_ALEN) {
		if (macaddr[i]) {
			int digit;

			got_num = 1;
			digit = hex_to_bin(macaddr[i]);
			if (digit >= 0)
				num = num * 16 + digit;
			else if (':' == macaddr[i])
				got_num = 2;
			else
				break;
		} else if (got_num)
			got_num = 2;
		else
			break;
		if (2 == got_num) {
			if (MAIN_PORT == port) {
				hw_priv->hw.override_addr[j++] = (u8) num;
				hw_priv->hw.override_addr[5] +=
					hw_priv->hw.id;
			} else {
				hw_priv->hw.ksz_switch->other_addr[j++] =
					(u8) num;
				hw_priv->hw.ksz_switch->other_addr[5] +=
					hw_priv->hw.id;
			}
			num = got_num = 0;
		}
		i++;
	}
	if (ETH_ALEN == j) {
		if (MAIN_PORT == port)
			hw_priv->hw.mac_override = 1;
	}
}

#define KS884X_DMA_MASK			(~0x0UL)

static void read_other_addr(struct ksz_hw *hw)
{
	int i;
	u16 data[3];
	struct ksz_switch *sw = hw->ksz_switch;

	for (i = 0; i < 3; i++)
		data[i] = eeprom_read(hw, i + EEPROM_DATA_OTHER_MAC_ADDR);
	if ((data[0] || data[1] || data[2]) && data[0] != 0xffff) {
		sw->other_addr[5] = (u8) data[0];
		sw->other_addr[4] = (u8)(data[0] >> 8);
		sw->other_addr[3] = (u8) data[1];
		sw->other_addr[2] = (u8)(data[1] >> 8);
		sw->other_addr[1] = (u8) data[2];
		sw->other_addr[0] = (u8)(data[2] >> 8);
	}
}

#ifndef PCI_VENDOR_ID_MICREL_KS
#define PCI_VENDOR_ID_MICREL_KS		0x16c6
#endif

static int __devinit pcidev_init(struct pci_dev *pdev,
	const struct pci_device_id *id)
{
	struct net_device *dev;
	struct dev_priv *priv;
	struct dev_info *hw_priv;
	struct ksz_hw *hw;
	struct platform_info *info;
	struct ksz_port *port;
	unsigned long reg_base;
	unsigned long reg_len;
	int cnt;
	int i;
	int mib_port_count;
	int pi;
	int port_count;
	int result;
	char banner[sizeof(version)];
	struct ksz_switch *sw = NULL;

	result = pci_enable_device(pdev);
	if (result)
		return result;

	result = -ENODEV;

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) ||
			pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)))
		return result;

	reg_base = pci_resource_start(pdev, 0);
	reg_len = pci_resource_len(pdev, 0);
	if ((pci_resource_flags(pdev, 0) & IORESOURCE_IO) != 0)
		return result;

	if (!request_mem_region(reg_base, reg_len, DRV_NAME))
		return result;
	pci_set_master(pdev);

	result = -ENOMEM;

	info = kzalloc(sizeof(struct platform_info), GFP_KERNEL);
	if (!info)
		goto pcidev_init_dev_err;

	hw_priv = &info->dev_info;
	hw_priv->pdev = pdev;

	hw = &hw_priv->hw;

	hw->io = ioremap(reg_base, reg_len);
	if (!hw->io)
		goto pcidev_init_io_err;

	cnt = hw_init(hw);
	if (!cnt) {
		if (msg_enable & NETIF_MSG_PROBE)
			pr_alert("chip not detected\n");
		result = -ENODEV;
		goto pcidev_init_alloc_err;
	}

	snprintf(banner, sizeof(banner), "%s", version);
	banner[13] = cnt + '0';		
	dev_info(&hw_priv->pdev->dev, "%s\n", banner);
	dev_dbg(&hw_priv->pdev->dev, "Mem = %p; IRQ = %d\n", hw->io, pdev->irq);

	
	hw->dev_count = 1;
	port_count = 1;
	mib_port_count = 1;
	hw->addr_list_size = 0;
	hw->mib_cnt = PORT_COUNTER_NUM;
	hw->mib_port_cnt = 1;

	
	if (2 == cnt) {
		if (fast_aging)
			hw->overrides |= FAST_AGING;

		hw->mib_cnt = TOTAL_PORT_COUNTER_NUM;

		
		if (multi_dev) {
			hw->dev_count = SWITCH_PORT_NUM;
			hw->addr_list_size = SWITCH_PORT_NUM - 1;
		}

		
		if (1 == hw->dev_count) {
			port_count = SWITCH_PORT_NUM;
			mib_port_count = SWITCH_PORT_NUM;
		}
		hw->mib_port_cnt = TOTAL_PORT_NUM;
		hw->ksz_switch = kzalloc(sizeof(struct ksz_switch), GFP_KERNEL);
		if (!hw->ksz_switch)
			goto pcidev_init_alloc_err;

		sw = hw->ksz_switch;
	}
	for (i = 0; i < hw->mib_port_cnt; i++)
		hw->port_mib[i].mib_start = 0;

	hw->parent = hw_priv;

	
	hw_priv->mtu = (REGULAR_RX_BUF_SIZE + 3) & ~3;

	if (ksz_alloc_mem(hw_priv))
		goto pcidev_init_mem_err;

	hw_priv->hw.id = net_device_present;

	spin_lock_init(&hw_priv->hwlock);
	mutex_init(&hw_priv->lock);

	
	tasklet_init(&hw_priv->rx_tasklet, rx_proc_task,
		(unsigned long) hw_priv);
	tasklet_init(&hw_priv->tx_tasklet, tx_proc_task,
		(unsigned long) hw_priv);

	
	tasklet_disable(&hw_priv->rx_tasklet);
	tasklet_disable(&hw_priv->tx_tasklet);

	for (i = 0; i < TOTAL_PORT_NUM; i++)
		init_waitqueue_head(&hw_priv->counter[i].counter);

	if (macaddr[0] != ':')
		get_mac_addr(hw_priv, macaddr, MAIN_PORT);

	
	hw_read_addr(hw);

	
	if (hw->dev_count > 1) {
		memcpy(sw->other_addr, hw->override_addr, ETH_ALEN);
		read_other_addr(hw);
		if (mac1addr[0] != ':')
			get_mac_addr(hw_priv, mac1addr, OTHER_PORT);
	}

	hw_setup(hw);
	if (hw->ksz_switch)
		sw_setup(hw);
	else {
		hw_priv->wol_support = WOL_SUPPORT;
		hw_priv->wol_enable = 0;
	}

	INIT_WORK(&hw_priv->mib_read, mib_read_work);

	
	ksz_init_timer(&hw_priv->mib_timer_info, 500 * HZ / 1000,
		mib_monitor, hw_priv);

	for (i = 0; i < hw->dev_count; i++) {
		dev = alloc_etherdev(sizeof(struct dev_priv));
		if (!dev)
			goto pcidev_init_reg_err;
		info->netdev[i] = dev;

		priv = netdev_priv(dev);
		priv->adapter = hw_priv;
		priv->id = net_device_present++;

		port = &priv->port;
		port->port_cnt = port_count;
		port->mib_port_cnt = mib_port_count;
		port->first_port = i;
		port->flow_ctrl = PHY_FLOW_CTRL;

		port->hw = hw;
		port->linked = &hw->port_info[port->first_port];

		for (cnt = 0, pi = i; cnt < port_count; cnt++, pi++) {
			hw->port_info[pi].port_id = pi;
			hw->port_info[pi].pdev = dev;
			hw->port_info[pi].state = media_disconnected;
		}

		dev->mem_start = (unsigned long) hw->io;
		dev->mem_end = dev->mem_start + reg_len - 1;
		dev->irq = pdev->irq;
		if (MAIN_PORT == i)
			memcpy(dev->dev_addr, hw_priv->hw.override_addr,
			       ETH_ALEN);
		else {
			memcpy(dev->dev_addr, sw->other_addr, ETH_ALEN);
			if (!memcmp(sw->other_addr, hw->override_addr,
				    ETH_ALEN))
				dev->dev_addr[5] += port->first_port;
		}

		dev->netdev_ops = &netdev_ops;
		SET_ETHTOOL_OPS(dev, &netdev_ethtool_ops);
		if (register_netdev(dev))
			goto pcidev_init_reg_err;
		port_set_power_saving(port, true);
	}

	pci_dev_get(hw_priv->pdev);
	pci_set_drvdata(pdev, info);
	return 0;

pcidev_init_reg_err:
	for (i = 0; i < hw->dev_count; i++) {
		if (info->netdev[i]) {
			netdev_free(info->netdev[i]);
			info->netdev[i] = NULL;
		}
	}

pcidev_init_mem_err:
	ksz_free_mem(hw_priv);
	kfree(hw->ksz_switch);

pcidev_init_alloc_err:
	iounmap(hw->io);

pcidev_init_io_err:
	kfree(info);

pcidev_init_dev_err:
	release_mem_region(reg_base, reg_len);

	return result;
}

static void pcidev_exit(struct pci_dev *pdev)
{
	int i;
	struct platform_info *info = pci_get_drvdata(pdev);
	struct dev_info *hw_priv = &info->dev_info;

	pci_set_drvdata(pdev, NULL);

	release_mem_region(pci_resource_start(pdev, 0),
		pci_resource_len(pdev, 0));
	for (i = 0; i < hw_priv->hw.dev_count; i++) {
		if (info->netdev[i])
			netdev_free(info->netdev[i]);
	}
	if (hw_priv->hw.io)
		iounmap(hw_priv->hw.io);
	ksz_free_mem(hw_priv);
	kfree(hw_priv->hw.ksz_switch);
	pci_dev_put(hw_priv->pdev);
	kfree(info);
}

#ifdef CONFIG_PM
static int pcidev_resume(struct pci_dev *pdev)
{
	int i;
	struct platform_info *info = pci_get_drvdata(pdev);
	struct dev_info *hw_priv = &info->dev_info;
	struct ksz_hw *hw = &hw_priv->hw;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	pci_enable_wake(pdev, PCI_D0, 0);

	if (hw_priv->wol_enable)
		hw_cfg_wol_pme(hw, 0);
	for (i = 0; i < hw->dev_count; i++) {
		if (info->netdev[i]) {
			struct net_device *dev = info->netdev[i];

			if (netif_running(dev)) {
				netdev_open(dev);
				netif_device_attach(dev);
			}
		}
	}
	return 0;
}

static int pcidev_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int i;
	struct platform_info *info = pci_get_drvdata(pdev);
	struct dev_info *hw_priv = &info->dev_info;
	struct ksz_hw *hw = &hw_priv->hw;

	
	static const u8 net_addr[] = { 192, 168, 1, 1 };

	for (i = 0; i < hw->dev_count; i++) {
		if (info->netdev[i]) {
			struct net_device *dev = info->netdev[i];

			if (netif_running(dev)) {
				netif_device_detach(dev);
				netdev_close(dev);
			}
		}
	}
	if (hw_priv->wol_enable) {
		hw_enable_wol(hw, hw_priv->wol_enable, net_addr);
		hw_cfg_wol_pme(hw, 1);
	}

	pci_save_state(pdev);
	pci_enable_wake(pdev, pci_choose_state(pdev, state), 1);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
	return 0;
}
#endif

static char pcidev_name[] = "ksz884xp";

static struct pci_device_id pcidev_table[] = {
	{ PCI_VENDOR_ID_MICREL_KS, 0x8841,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MICREL_KS, 0x8842,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pcidev_table);

static struct pci_driver pci_device_driver = {
#ifdef CONFIG_PM
	.suspend	= pcidev_suspend,
	.resume		= pcidev_resume,
#endif
	.name		= pcidev_name,
	.id_table	= pcidev_table,
	.probe		= pcidev_init,
	.remove		= pcidev_exit
};

static int __init ksz884x_init_module(void)
{
	return pci_register_driver(&pci_device_driver);
}

static void __exit ksz884x_cleanup_module(void)
{
	pci_unregister_driver(&pci_device_driver);
}

module_init(ksz884x_init_module);
module_exit(ksz884x_cleanup_module);

MODULE_DESCRIPTION("KSZ8841/2 PCI network driver");
MODULE_AUTHOR("Tristram Ha <Tristram.Ha@micrel.com>");
MODULE_LICENSE("GPL");

module_param_named(message, msg_enable, int, 0);
MODULE_PARM_DESC(message, "Message verbosity level (0=none, 31=all)");

module_param(macaddr, charp, 0);
module_param(mac1addr, charp, 0);
module_param(fast_aging, int, 0);
module_param(multi_dev, int, 0);
module_param(stp, int, 0);
MODULE_PARM_DESC(macaddr, "MAC address");
MODULE_PARM_DESC(mac1addr, "Second MAC address");
MODULE_PARM_DESC(fast_aging, "Fast aging");
MODULE_PARM_DESC(multi_dev, "Multiple device interfaces");
MODULE_PARM_DESC(stp, "STP support");
