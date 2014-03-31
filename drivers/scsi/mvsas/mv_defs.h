/*
 * Marvell 88SE64xx/88SE94xx const head file
 *
 * Copyright 2007 Red Hat, Inc.
 * Copyright 2008 Marvell. <kewei@marvell.com>
 * Copyright 2009-2011 Marvell. <yuxiangl@marvell.com>
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
*/

#ifndef _MV_DEFS_H_
#define _MV_DEFS_H_

#define PCI_DEVICE_ID_ARECA_1300	0x1300
#define PCI_DEVICE_ID_ARECA_1320	0x1320

enum chip_flavors {
	chip_6320,
	chip_6440,
	chip_6485,
	chip_9480,
	chip_9180,
	chip_9445,
	chip_9485,
	chip_1300,
	chip_1320
};

enum driver_configuration {
	MVS_TX_RING_SZ		= 1024,	
	MVS_RX_RING_SZ		= 1024, 
	MVS_SOC_SLOTS		= 64,
	MVS_SOC_TX_RING_SZ	= MVS_SOC_SLOTS * 2,
	MVS_SOC_RX_RING_SZ	= MVS_SOC_SLOTS * 2,

	MVS_SLOT_BUF_SZ		= 8192, 
	MVS_SSP_CMD_SZ		= 64,	
	MVS_ATA_CMD_SZ		= 96,	
	MVS_OAF_SZ		= 64,	
	MVS_QUEUE_SIZE		= 64,	
	MVS_SOC_CAN_QUEUE	= MVS_SOC_SLOTS - 2,
};

enum hardware_details {
	MVS_MAX_PHYS		= 8,	
	MVS_MAX_PORTS		= 8,	
	MVS_SOC_PHYS		= 4,	
	MVS_SOC_PORTS		= 4,	
	MVS_MAX_DEVICES	= 1024,	
};

enum peripheral_registers {
	SPI_CTL			= 0x10,	
	SPI_CMD			= 0x14,	
	SPI_DATA		= 0x18, 
};

enum peripheral_register_bits {
	TWSI_RDY		= (1U << 7),	
	TWSI_RD			= (1U << 4),	

	SPI_ADDR_MASK		= 0x3ffff,	
};

enum hw_register_bits {
	
	INT_EN			= (1U << 1),	
	HBA_RST			= (1U << 0),	

	
	INT_XOR			= (1U << 4),	
	INT_SAS_SATA		= (1U << 0),	

				
	SATA_TARGET		= (1U << 16),	
	MODE_AUTO_DET_PORT7 = (1U << 15),	
	MODE_AUTO_DET_PORT6 = (1U << 14),
	MODE_AUTO_DET_PORT5 = (1U << 13),
	MODE_AUTO_DET_PORT4 = (1U << 12),
	MODE_AUTO_DET_PORT3 = (1U << 11),
	MODE_AUTO_DET_PORT2 = (1U << 10),
	MODE_AUTO_DET_PORT1 = (1U << 9),
	MODE_AUTO_DET_PORT0 = (1U << 8),
	MODE_AUTO_DET_EN    =	MODE_AUTO_DET_PORT0 | MODE_AUTO_DET_PORT1 |
				MODE_AUTO_DET_PORT2 | MODE_AUTO_DET_PORT3 |
				MODE_AUTO_DET_PORT4 | MODE_AUTO_DET_PORT5 |
				MODE_AUTO_DET_PORT6 | MODE_AUTO_DET_PORT7,
	MODE_SAS_PORT7_MASK = (1U << 7),  
	MODE_SAS_PORT6_MASK = (1U << 6),
	MODE_SAS_PORT5_MASK = (1U << 5),
	MODE_SAS_PORT4_MASK = (1U << 4),
	MODE_SAS_PORT3_MASK = (1U << 3),
	MODE_SAS_PORT2_MASK = (1U << 2),
	MODE_SAS_PORT1_MASK = (1U << 1),
	MODE_SAS_PORT0_MASK = (1U << 0),
	MODE_SAS_SATA	=	MODE_SAS_PORT0_MASK | MODE_SAS_PORT1_MASK |
				MODE_SAS_PORT2_MASK | MODE_SAS_PORT3_MASK |
				MODE_SAS_PORT4_MASK | MODE_SAS_PORT5_MASK |
				MODE_SAS_PORT6_MASK | MODE_SAS_PORT7_MASK,


	
	TX_EN			= (1U << 16),	
	TX_RING_SZ_MASK		= 0xfff,	

	
	RX_EN			= (1U << 16),	
	RX_RING_SZ_MASK		= 0xfff,	

	
	COAL_EN			= (1U << 16),	

	
	CINT_I2C		= (1U << 31),	
	CINT_SW0		= (1U << 30),	
	CINT_SW1		= (1U << 29),	
	CINT_PRD_BC		= (1U << 28),	
	CINT_DMA_PCIE		= (1U << 27),	
	CINT_MEM		= (1U << 26),	
	CINT_I2C_SLAVE		= (1U << 25),	
	CINT_NON_SPEC_NCQ_ERROR	= (1U << 25),	
	CINT_SRS		= (1U << 3),	
	CINT_CI_STOP		= (1U << 1),	
	CINT_DONE		= (1U << 0),	

						
	CINT_PORT_STOPPED	= (1U << 16),	
	CINT_PORT		= (1U << 8),	
	CINT_PORT_MASK_OFFSET	= 8,
	CINT_PORT_MASK		= (0xFF << CINT_PORT_MASK_OFFSET),
	CINT_PHY_MASK_OFFSET	= 4,
	CINT_PHY_MASK		= (0x0F << CINT_PHY_MASK_OFFSET),

	
	TXQ_CMD_SHIFT		= 29,
	TXQ_CMD_SSP		= 1,		
	TXQ_CMD_SMP		= 2,		
	TXQ_CMD_STP		= 3,		
	TXQ_CMD_SSP_FREE_LIST	= 4,		
	TXQ_CMD_SLOT_RESET	= 7,		
	TXQ_MODE_I		= (1U << 28),	
	TXQ_MODE_TARGET 	= 0,
	TXQ_MODE_INITIATOR	= 1,
	TXQ_PRIO_HI		= (1U << 27),	
	TXQ_PRI_NORMAL		= 0,
	TXQ_PRI_HIGH		= 1,
	TXQ_SRS_SHIFT		= 20,		
	TXQ_SRS_MASK		= 0x7f,
	TXQ_PHY_SHIFT		= 12,		
	TXQ_PHY_MASK		= 0xff,
	TXQ_SLOT_MASK		= 0xfff,	

	
	RXQ_GOOD		= (1U << 23),	
	RXQ_SLOT_RESET		= (1U << 21),	
	RXQ_CMD_RX		= (1U << 20),	
	RXQ_ATTN		= (1U << 19),	
	RXQ_RSP			= (1U << 18),	
	RXQ_ERR			= (1U << 17),	
	RXQ_DONE		= (1U << 16),	
	RXQ_SLOT_MASK		= 0xfff,	

	
	MCH_PRD_LEN_SHIFT	= 16,		
	MCH_SSP_FR_TYPE_SHIFT	= 13,		

						
	MCH_SSP_FR_CMD		= 0x0,		

						
	MCH_SSP_FR_TASK		= 0x1,		

						
	MCH_SSP_FR_XFER_RDY	= 0x4,		
	MCH_SSP_FR_RESP		= 0x5,		
	MCH_SSP_FR_READ		= 0x6,		
	MCH_SSP_FR_READ_RESP	= 0x7,		

	MCH_SSP_MODE_PASSTHRU	= 1,
	MCH_SSP_MODE_NORMAL	= 0,
	MCH_PASSTHRU		= (1U << 12),	
	MCH_FBURST		= (1U << 11),	
	MCH_CHK_LEN		= (1U << 10),	
	MCH_RETRY		= (1U << 9),	
	MCH_PROTECTION		= (1U << 8),	
	MCH_RESET		= (1U << 7),	
	MCH_FPDMA		= (1U << 6),	
	MCH_ATAPI		= (1U << 5),	
	MCH_BIST		= (1U << 4),	
	MCH_PMP_MASK		= 0xf,		

	CCTL_RST		= (1U << 5),	

						
	CCTL_ENDIAN_DATA	= (1U << 3),	
	CCTL_ENDIAN_RSP		= (1U << 2),	
	CCTL_ENDIAN_OPEN	= (1U << 1),	
	CCTL_ENDIAN_CMD		= (1U << 0),	

	
	PHY_SSP_RST		= (1U << 3),	
	PHY_BCAST_CHG		= (1U << 2),	
	PHY_RST_HARD		= (1U << 1),	
	PHY_RST			= (1U << 0),	
	PHY_READY_MASK		= (1U << 20),

	
	PHYEV_DEC_ERR		= (1U << 24),	
	PHYEV_DCDR_ERR		= (1U << 23),	
	PHYEV_CRC_ERR		= (1U << 22),	
	PHYEV_UNASSOC_FIS	= (1U << 19),	
	PHYEV_AN		= (1U << 18),	
	PHYEV_BIST_ACT		= (1U << 17),	
	PHYEV_SIG_FIS		= (1U << 16),	
	PHYEV_POOF		= (1U << 12),	
	PHYEV_IU_BIG		= (1U << 11),	
	PHYEV_IU_SMALL		= (1U << 10),	
	PHYEV_UNK_TAG		= (1U << 9),	
	PHYEV_BROAD_CH		= (1U << 8),	
	PHYEV_COMWAKE		= (1U << 7),	
	PHYEV_PORT_SEL		= (1U << 6),	
	PHYEV_HARD_RST		= (1U << 5),	
	PHYEV_ID_TMOUT		= (1U << 4),	
	PHYEV_ID_FAIL		= (1U << 3),	
	PHYEV_ID_DONE		= (1U << 2),	
	PHYEV_HARD_RST_DONE	= (1U << 1),	
	PHYEV_RDY_CH		= (1U << 0),	

	
	PCS_EN_SATA_REG_SHIFT	= (16),		
	PCS_EN_PORT_XMT_SHIFT	= (12),		
	PCS_EN_PORT_XMT_SHIFT2	= (8),		
	PCS_SATA_RETRY		= (1U << 8),	
	PCS_RSP_RX_EN		= (1U << 7),	
	PCS_SATA_RETRY_2	= (1U << 6),	
	PCS_SELF_CLEAR		= (1U << 5),	
	PCS_FIS_RX_EN		= (1U << 4),	
	PCS_CMD_STOP_ERR	= (1U << 3),	
	PCS_CMD_RST		= (1U << 1),	
	PCS_CMD_EN		= (1U << 0),	

	
	PORT_DEV_SSP_TRGT	= (1U << 19),
	PORT_DEV_SMP_TRGT	= (1U << 18),
	PORT_DEV_STP_TRGT	= (1U << 17),
	PORT_DEV_SSP_INIT	= (1U << 11),
	PORT_DEV_SMP_INIT	= (1U << 10),
	PORT_DEV_STP_INIT	= (1U << 9),
	PORT_PHY_ID_MASK	= (0xFFU << 24),
	PORT_SSP_TRGT_MASK	= (0x1U << 19),
	PORT_SSP_INIT_MASK	= (0x1U << 11),
	PORT_DEV_TRGT_MASK	= (0x7U << 17),
	PORT_DEV_INIT_MASK	= (0x7U << 9),
	PORT_DEV_TYPE_MASK	= (0x7U << 0),

	
	PHY_RDY			= (1U << 2),
	PHY_DW_SYNC		= (1U << 1),
	PHY_OOB_DTCTD		= (1U << 0),

	
	
	PHY_MODE6_LATECLK	= (1U << 29),	
	PHY_MODE6_DTL_SPEED	= (1U << 27),	
	PHY_MODE6_FC_ORDER	= (1U << 26),	
	PHY_MODE6_MUCNT_EN	= (1U << 24),	
	PHY_MODE6_SEL_MUCNT_LEN	= (1U << 22),	
	PHY_MODE6_SELMUPI	= (1U << 20),	
	PHY_MODE6_SELMUPF	= (1U << 18),	
	PHY_MODE6_SELMUFF	= (1U << 16),	
	PHY_MODE6_SELMUFI	= (1U << 14),	
	PHY_MODE6_FREEZE_LOOP	= (1U << 12),	
	PHY_MODE6_INT_RXFOFFS	= (1U << 3),	
	PHY_MODE6_FRC_RXFOFFS	= (1U << 2),	
	PHY_MODE6_STAU_0D8	= (1U << 1),	
	PHY_MODE6_RXSAT_DIS	= (1U << 0),	
};

enum sas_sata_config_port_regs {
	PHYR_IDENTIFY		= 0x00,	
	PHYR_ADDR_LO		= 0x04,	
	PHYR_ADDR_HI		= 0x08,	
	PHYR_ATT_DEV_INFO	= 0x0C,	
	PHYR_ATT_ADDR_LO	= 0x10,	
	PHYR_ATT_ADDR_HI	= 0x14,	
	PHYR_SATA_CTL		= 0x18,	
	PHYR_PHY_STAT		= 0x1C,	
	PHYR_SATA_SIG0	= 0x20,	
	PHYR_SATA_SIG1	= 0x24,	
	PHYR_SATA_SIG2	= 0x28,	
	PHYR_SATA_SIG3	= 0x2c,	
	PHYR_R_ERR_COUNT	= 0x30, 
	PHYR_CRC_ERR_COUNT	= 0x34, 
	PHYR_WIDE_PORT	= 0x38,	
	PHYR_CURRENT0		= 0x80,	
	PHYR_CURRENT1		= 0x84,	
	PHYR_CURRENT2		= 0x88,	
	CONFIG_ID_FRAME0       = 0x100, 
	CONFIG_ID_FRAME1       = 0x104, 
	CONFIG_ID_FRAME2       = 0x108, 
	CONFIG_ID_FRAME3       = 0x10c, 
	CONFIG_ID_FRAME4       = 0x110, 
	CONFIG_ID_FRAME5       = 0x114, 
	CONFIG_ID_FRAME6       = 0x118, 
	CONFIG_ATT_ID_FRAME0   = 0x11c, 
	CONFIG_ATT_ID_FRAME1   = 0x120, 
	CONFIG_ATT_ID_FRAME2   = 0x124, 
	CONFIG_ATT_ID_FRAME3   = 0x128, 
	CONFIG_ATT_ID_FRAME4   = 0x12c, 
	CONFIG_ATT_ID_FRAME5   = 0x130, 
	CONFIG_ATT_ID_FRAME6   = 0x134, 
};

enum sas_cmd_port_registers {
	CMD_CMRST_OOB_DET	= 0x100, 
	CMD_CMWK_OOB_DET	= 0x104, 
	CMD_CMSAS_OOB_DET	= 0x108, 
	CMD_BRST_OOB_DET	= 0x10c, 
	CMD_OOB_SPACE	= 0x110, 
	CMD_OOB_BURST	= 0x114, 
	CMD_PHY_TIMER		= 0x118, 
	CMD_PHY_CONFIG0	= 0x11c, 
	CMD_PHY_CONFIG1	= 0x120, 
	CMD_SAS_CTL0		= 0x124, 
	CMD_SAS_CTL1		= 0x128, 
	CMD_SAS_CTL2		= 0x12c, 
	CMD_SAS_CTL3		= 0x130, 
	CMD_ID_TEST		= 0x134, 
	CMD_PL_TIMER		= 0x138, 
	CMD_WD_TIMER		= 0x13c, 
	CMD_PORT_SEL_COUNT	= 0x140, 
	CMD_APP_MEM_CTL	= 0x144, 
	CMD_XOR_MEM_CTL	= 0x148, 
	CMD_DMA_MEM_CTL	= 0x14c, 
	CMD_PORT_MEM_CTL0	= 0x150, 
	CMD_PORT_MEM_CTL1	= 0x154, 
	CMD_SATA_PORT_MEM_CTL0	= 0x158, 
	CMD_SATA_PORT_MEM_CTL1	= 0x15c, 
	CMD_XOR_MEM_BIST_CTL	= 0x160, 
	CMD_XOR_MEM_BIST_STAT	= 0x164, 
	CMD_DMA_MEM_BIST_CTL	= 0x168, 
	CMD_DMA_MEM_BIST_STAT	= 0x16c, 
	CMD_PORT_MEM_BIST_CTL	= 0x170, 
	CMD_PORT_MEM_BIST_STAT0 = 0x174, 
	CMD_PORT_MEM_BIST_STAT1 = 0x178, 
	CMD_STP_MEM_BIST_CTL	= 0x17c, 
	CMD_STP_MEM_BIST_STAT0	= 0x180, 
	CMD_STP_MEM_BIST_STAT1	= 0x184, 
	CMD_RESET_COUNT		= 0x188, 
	CMD_MONTR_DATA_SEL	= 0x18C, 
	CMD_PLL_PHY_CONFIG	= 0x190, 
	CMD_PHY_CTL		= 0x194, 
	CMD_PHY_TEST_COUNT0	= 0x198, 
	CMD_PHY_TEST_COUNT1	= 0x19C, 
	CMD_PHY_TEST_COUNT2	= 0x1A0, 
	CMD_APP_ERR_CONFIG	= 0x1A4, 
	CMD_PND_FIFO_CTL0	= 0x1A8, 
	CMD_HOST_CTL		= 0x1AC, 
	CMD_HOST_WR_DATA	= 0x1B0, 
	CMD_HOST_RD_DATA	= 0x1B4, 
	CMD_PHY_MODE_21		= 0x1B8, 
	CMD_SL_MODE0		= 0x1BC, 
	CMD_SL_MODE1		= 0x1C0, 
	CMD_PND_FIFO_CTL1	= 0x1C4, 
	CMD_PORT_LAYER_TIMER1	= 0x1E0, 
	CMD_LINK_TIMER		= 0x1E4, 
};

enum mvs_info_flags {
	MVF_PHY_PWR_FIX	= (1U << 1),	
	MVF_FLAG_SOC		= (1U << 2),	
};

enum mvs_event_flags {
	PHY_PLUG_EVENT		= (3U),
	PHY_PLUG_IN		= (1U << 0),	
	PHY_PLUG_OUT		= (1U << 1),	
	EXP_BRCT_CHG		= (1U << 2),	
};

enum mvs_port_type {
	PORT_TGT_MASK	=  (1U << 5),
	PORT_INIT_PORT	=  (1U << 4),
	PORT_TGT_PORT	=  (1U << 3),
	PORT_INIT_TGT_PORT = (PORT_INIT_PORT | PORT_TGT_PORT),
	PORT_TYPE_SAS	=  (1U << 1),
	PORT_TYPE_SATA	=  (1U << 0),
};

enum ct_format {
	
	SSP_F_H		=  0x00,
	SSP_F_IU	=  0x18,
	SSP_F_MAX	=  0x4D,
	
	STP_CMD_FIS	=  0x00,
	STP_ATAPI_CMD	=  0x40,
	STP_F_MAX	=  0x10,
	
	SMP_F_T		=  0x00,
	SMP_F_DEP	=  0x01,
	SMP_F_MAX	=  0x101,
};

enum status_buffer {
	SB_EIR_OFF	=  0x00,	
	SB_RFB_OFF	=  0x08,	
	SB_RFB_MAX	=  0x400,	
};

enum error_info_rec {
	CMD_ISS_STPD	= (1U << 31),	
	CMD_PI_ERR	= (1U << 30),	
	RSP_OVER	= (1U << 29),	
	RETRY_LIM	= (1U << 28),	
	UNK_FIS 	= (1U << 27),	
	DMA_TERM	= (1U << 26),	
	SYNC_ERR	= (1U << 25),	
	TFILE_ERR	= (1U << 24),	
	R_ERR		= (1U << 23),	
	RD_OFS		= (1U << 20),	
	XFER_RDY_OFS	= (1U << 19),	
	UNEXP_XFER_RDY	= (1U << 18),	
	DATA_OVER_UNDER = (1U << 16),	
	INTERLOCK	= (1U << 15),	
	NAK		= (1U << 14),	
	ACK_NAK_TO	= (1U << 13),	
	CXN_CLOSED	= (1U << 12),	
	OPEN_TO 	= (1U << 11),	
	PATH_BLOCKED	= (1U << 10),	
	NO_DEST 	= (1U << 9),	
	STP_RES_BSY	= (1U << 8),	
	BREAK		= (1U << 7),	
	BAD_DEST	= (1U << 6),	
	BAD_PROTO	= (1U << 5),	
	BAD_RATE	= (1U << 4),	
	WRONG_DEST	= (1U << 3),	
	CREDIT_TO	= (1U << 2),	
	WDOG_TO 	= (1U << 1),	
	BUF_PAR 	= (1U << 0),	
};

enum error_info_rec_2 {
	SLOT_BSY_ERR	= (1U << 31),	
	GRD_CHK_ERR	= (1U << 14),	
	APP_CHK_ERR	= (1U << 13),	
	REF_CHK_ERR	= (1U << 12),	
	USR_BLK_NM	= (1U << 0),	
};

enum pci_cfg_register_bits {
	PCTL_PWR_OFF	= (0xFU << 24),
	PCTL_COM_ON	= (0xFU << 20),
	PCTL_LINK_RST	= (0xFU << 16),
	PCTL_LINK_OFFS	= (16),
	PCTL_PHY_DSBL	= (0xFU << 12),
	PCTL_PHY_DSBL_OFFS	= (12),
	PRD_REQ_SIZE	= (0x4000),
	PRD_REQ_MASK	= (0x00007000),
	PLS_NEG_LINK_WD		= (0x3FU << 4),
	PLS_NEG_LINK_WD_OFFS	= 4,
	PLS_LINK_SPD		= (0x0FU << 0),
	PLS_LINK_SPD_OFFS	= 0,
};

enum open_frame_protocol {
	PROTOCOL_SMP	= 0x0,
	PROTOCOL_SSP	= 0x1,
	PROTOCOL_STP	= 0x2,
};

enum datapres_field {
	NO_DATA		= 0,
	RESPONSE_DATA	= 1,
	SENSE_DATA	= 2,
};

struct mvs_tmf_task{
	u8 tmf;
	u16 tag_of_task_to_be_managed;
};
#endif
