/**
 * drivers/net/ethernet/micrel/ks8851_mll.c
 * Copyright (c) 2009 Micrel Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/cache.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/io.h>

#define	DRV_NAME	"ks8851_mll"

static u8 KS_DEFAULT_MAC_ADDRESS[] = { 0x00, 0x10, 0xA1, 0x86, 0x95, 0x11 };
#define MAX_RECV_FRAMES			255
#define MAX_BUF_SIZE			2048
#define TX_BUF_SIZE			2000
#define RX_BUF_SIZE			2000

#define KS_CCR				0x08
#define CCR_EEPROM			(1 << 9)
#define CCR_SPI				(1 << 8)
#define CCR_8BIT			(1 << 7)
#define CCR_16BIT			(1 << 6)
#define CCR_32BIT			(1 << 5)
#define CCR_SHARED			(1 << 4)
#define CCR_32PIN			(1 << 0)

#define KS_MARL				0x10
#define KS_MARM				0x12
#define KS_MARH				0x14

#define KS_OBCR				0x20
#define OBCR_ODS_16MA			(1 << 6)

#define KS_EEPCR			0x22
#define EEPCR_EESA			(1 << 4)
#define EEPCR_EESB			(1 << 3)
#define EEPCR_EEDO			(1 << 2)
#define EEPCR_EESCK			(1 << 1)
#define EEPCR_EECS			(1 << 0)

#define KS_MBIR				0x24
#define MBIR_TXMBF			(1 << 12)
#define MBIR_TXMBFA			(1 << 11)
#define MBIR_RXMBF			(1 << 4)
#define MBIR_RXMBFA			(1 << 3)

#define KS_GRR				0x26
#define GRR_QMU				(1 << 1)
#define GRR_GSR				(1 << 0)

#define KS_WFCR				0x2A
#define WFCR_MPRXE			(1 << 7)
#define WFCR_WF3E			(1 << 3)
#define WFCR_WF2E			(1 << 2)
#define WFCR_WF1E			(1 << 1)
#define WFCR_WF0E			(1 << 0)

#define KS_WF0CRC0			0x30
#define KS_WF0CRC1			0x32
#define KS_WF0BM0			0x34
#define KS_WF0BM1			0x36
#define KS_WF0BM2			0x38
#define KS_WF0BM3			0x3A

#define KS_WF1CRC0			0x40
#define KS_WF1CRC1			0x42
#define KS_WF1BM0			0x44
#define KS_WF1BM1			0x46
#define KS_WF1BM2			0x48
#define KS_WF1BM3			0x4A

#define KS_WF2CRC0			0x50
#define KS_WF2CRC1			0x52
#define KS_WF2BM0			0x54
#define KS_WF2BM1			0x56
#define KS_WF2BM2			0x58
#define KS_WF2BM3			0x5A

#define KS_WF3CRC0			0x60
#define KS_WF3CRC1			0x62
#define KS_WF3BM0			0x64
#define KS_WF3BM1			0x66
#define KS_WF3BM2			0x68
#define KS_WF3BM3			0x6A

#define KS_TXCR				0x70
#define TXCR_TCGICMP			(1 << 8)
#define TXCR_TCGUDP			(1 << 7)
#define TXCR_TCGTCP			(1 << 6)
#define TXCR_TCGIP			(1 << 5)
#define TXCR_FTXQ			(1 << 4)
#define TXCR_TXFCE			(1 << 3)
#define TXCR_TXPE			(1 << 2)
#define TXCR_TXCRC			(1 << 1)
#define TXCR_TXE			(1 << 0)

#define KS_TXSR				0x72
#define TXSR_TXLC			(1 << 13)
#define TXSR_TXMC			(1 << 12)
#define TXSR_TXFID_MASK			(0x3f << 0)
#define TXSR_TXFID_SHIFT		(0)
#define TXSR_TXFID_GET(_v)		(((_v) >> 0) & 0x3f)


#define KS_RXCR1			0x74
#define RXCR1_FRXQ			(1 << 15)
#define RXCR1_RXUDPFCC			(1 << 14)
#define RXCR1_RXTCPFCC			(1 << 13)
#define RXCR1_RXIPFCC			(1 << 12)
#define RXCR1_RXPAFMA			(1 << 11)
#define RXCR1_RXFCE			(1 << 10)
#define RXCR1_RXEFE			(1 << 9)
#define RXCR1_RXMAFMA			(1 << 8)
#define RXCR1_RXBE			(1 << 7)
#define RXCR1_RXME			(1 << 6)
#define RXCR1_RXUE			(1 << 5)
#define RXCR1_RXAE			(1 << 4)
#define RXCR1_RXINVF			(1 << 1)
#define RXCR1_RXE			(1 << 0)
#define RXCR1_FILTER_MASK    		(RXCR1_RXINVF | RXCR1_RXAE | \
					 RXCR1_RXMAFMA | RXCR1_RXPAFMA)

#define KS_RXCR2			0x76
#define RXCR2_SRDBL_MASK		(0x7 << 5)
#define RXCR2_SRDBL_SHIFT		(5)
#define RXCR2_SRDBL_4B			(0x0 << 5)
#define RXCR2_SRDBL_8B			(0x1 << 5)
#define RXCR2_SRDBL_16B			(0x2 << 5)
#define RXCR2_SRDBL_32B			(0x3 << 5)
#define RXCR2_IUFFP			(1 << 4)
#define RXCR2_RXIUFCEZ			(1 << 3)
#define RXCR2_UDPLFE			(1 << 2)
#define RXCR2_RXICMPFCC			(1 << 1)
#define RXCR2_RXSAF			(1 << 0)

#define KS_TXMIR			0x78

#define KS_RXFHSR			0x7C
#define RXFSHR_RXFV			(1 << 15)
#define RXFSHR_RXICMPFCS		(1 << 13)
#define RXFSHR_RXIPFCS			(1 << 12)
#define RXFSHR_RXTCPFCS			(1 << 11)
#define RXFSHR_RXUDPFCS			(1 << 10)
#define RXFSHR_RXBF			(1 << 7)
#define RXFSHR_RXMF			(1 << 6)
#define RXFSHR_RXUF			(1 << 5)
#define RXFSHR_RXMR			(1 << 4)
#define RXFSHR_RXFT			(1 << 3)
#define RXFSHR_RXFTL			(1 << 2)
#define RXFSHR_RXRF			(1 << 1)
#define RXFSHR_RXCE			(1 << 0)
#define	RXFSHR_ERR			(RXFSHR_RXCE | RXFSHR_RXRF |\
					RXFSHR_RXFTL | RXFSHR_RXMR |\
					RXFSHR_RXICMPFCS | RXFSHR_RXIPFCS |\
					RXFSHR_RXTCPFCS)
#define KS_RXFHBCR			0x7E
#define RXFHBCR_CNT_MASK		0x0FFF

#define KS_TXQCR			0x80
#define TXQCR_AETFE			(1 << 2)
#define TXQCR_TXQMAM			(1 << 1)
#define TXQCR_METFE			(1 << 0)

#define KS_RXQCR			0x82
#define RXQCR_RXDTTS			(1 << 12)
#define RXQCR_RXDBCTS			(1 << 11)
#define RXQCR_RXFCTS			(1 << 10)
#define RXQCR_RXIPHTOE			(1 << 9)
#define RXQCR_RXDTTE			(1 << 7)
#define RXQCR_RXDBCTE			(1 << 6)
#define RXQCR_RXFCTE			(1 << 5)
#define RXQCR_ADRFE			(1 << 4)
#define RXQCR_SDA			(1 << 3)
#define RXQCR_RRXEF			(1 << 0)
#define RXQCR_CMD_CNTL                	(RXQCR_RXFCTE|RXQCR_ADRFE)

#define KS_TXFDPR			0x84
#define TXFDPR_TXFPAI			(1 << 14)
#define TXFDPR_TXFP_MASK		(0x7ff << 0)
#define TXFDPR_TXFP_SHIFT		(0)

#define KS_RXFDPR			0x86
#define RXFDPR_RXFPAI			(1 << 14)

#define KS_RXDTTR			0x8C
#define KS_RXDBCTR			0x8E

#define KS_IER				0x90
#define KS_ISR				0x92
#define IRQ_LCI				(1 << 15)
#define IRQ_TXI				(1 << 14)
#define IRQ_RXI				(1 << 13)
#define IRQ_RXOI			(1 << 11)
#define IRQ_TXPSI			(1 << 9)
#define IRQ_RXPSI			(1 << 8)
#define IRQ_TXSAI			(1 << 6)
#define IRQ_RXWFDI			(1 << 5)
#define IRQ_RXMPDI			(1 << 4)
#define IRQ_LDI				(1 << 3)
#define IRQ_EDI				(1 << 2)
#define IRQ_SPIBEI			(1 << 1)
#define IRQ_DEDI			(1 << 0)

#define KS_RXFCTR			0x9C
#define RXFCTR_THRESHOLD_MASK     	0x00FF

#define KS_RXFC				0x9D
#define RXFCTR_RXFC_MASK		(0xff << 8)
#define RXFCTR_RXFC_SHIFT		(8)
#define RXFCTR_RXFC_GET(_v)		(((_v) >> 8) & 0xff)
#define RXFCTR_RXFCT_MASK		(0xff << 0)
#define RXFCTR_RXFCT_SHIFT		(0)

#define KS_TXNTFSR			0x9E

#define KS_MAHTR0			0xA0
#define KS_MAHTR1			0xA2
#define KS_MAHTR2			0xA4
#define KS_MAHTR3			0xA6

#define KS_FCLWR			0xB0
#define KS_FCHWR			0xB2
#define KS_FCOWR			0xB4

#define KS_CIDER			0xC0
#define CIDER_ID			0x8870
#define CIDER_REV_MASK			(0x7 << 1)
#define CIDER_REV_SHIFT			(1)
#define CIDER_REV_GET(_v)		(((_v) >> 1) & 0x7)

#define KS_CGCR				0xC6
#define KS_IACR				0xC8
#define IACR_RDEN			(1 << 12)
#define IACR_TSEL_MASK			(0x3 << 10)
#define IACR_TSEL_SHIFT			(10)
#define IACR_TSEL_MIB			(0x3 << 10)
#define IACR_ADDR_MASK			(0x1f << 0)
#define IACR_ADDR_SHIFT			(0)

#define KS_IADLR			0xD0
#define KS_IAHDR			0xD2

#define KS_PMECR			0xD4
#define PMECR_PME_DELAY			(1 << 14)
#define PMECR_PME_POL			(1 << 12)
#define PMECR_WOL_WAKEUP		(1 << 11)
#define PMECR_WOL_MAGICPKT		(1 << 10)
#define PMECR_WOL_LINKUP		(1 << 9)
#define PMECR_WOL_ENERGY		(1 << 8)
#define PMECR_AUTO_WAKE_EN		(1 << 7)
#define PMECR_WAKEUP_NORMAL		(1 << 6)
#define PMECR_WKEVT_MASK		(0xf << 2)
#define PMECR_WKEVT_SHIFT		(2)
#define PMECR_WKEVT_GET(_v)		(((_v) >> 2) & 0xf)
#define PMECR_WKEVT_ENERGY		(0x1 << 2)
#define PMECR_WKEVT_LINK		(0x2 << 2)
#define PMECR_WKEVT_MAGICPKT		(0x4 << 2)
#define PMECR_WKEVT_FRAME		(0x8 << 2)
#define PMECR_PM_MASK			(0x3 << 0)
#define PMECR_PM_SHIFT			(0)
#define PMECR_PM_NORMAL			(0x0 << 0)
#define PMECR_PM_ENERGY			(0x1 << 0)
#define PMECR_PM_SOFTDOWN		(0x2 << 0)
#define PMECR_PM_POWERSAVE		(0x3 << 0)

#define KS_P1MBCR			0xE4
#define P1MBCR_FORCE_FDX		(1 << 8)

#define KS_P1MBSR			0xE6
#define P1MBSR_AN_COMPLETE		(1 << 5)
#define P1MBSR_AN_CAPABLE		(1 << 3)
#define P1MBSR_LINK_UP			(1 << 2)

#define KS_PHY1ILR			0xE8
#define KS_PHY1IHR			0xEA
#define KS_P1ANAR			0xEC
#define KS_P1ANLPR			0xEE

#define KS_P1SCLMD			0xF4
#define P1SCLMD_LEDOFF			(1 << 15)
#define P1SCLMD_TXIDS			(1 << 14)
#define P1SCLMD_RESTARTAN		(1 << 13)
#define P1SCLMD_DISAUTOMDIX		(1 << 10)
#define P1SCLMD_FORCEMDIX		(1 << 9)
#define P1SCLMD_AUTONEGEN		(1 << 7)
#define P1SCLMD_FORCE100		(1 << 6)
#define P1SCLMD_FORCEFDX		(1 << 5)
#define P1SCLMD_ADV_FLOW		(1 << 4)
#define P1SCLMD_ADV_100BT_FDX		(1 << 3)
#define P1SCLMD_ADV_100BT_HDX		(1 << 2)
#define P1SCLMD_ADV_10BT_FDX		(1 << 1)
#define P1SCLMD_ADV_10BT_HDX		(1 << 0)

#define KS_P1CR				0xF6
#define P1CR_HP_MDIX			(1 << 15)
#define P1CR_REV_POL			(1 << 13)
#define P1CR_OP_100M			(1 << 10)
#define P1CR_OP_FDX			(1 << 9)
#define P1CR_OP_MDI			(1 << 7)
#define P1CR_AN_DONE			(1 << 6)
#define P1CR_LINK_GOOD			(1 << 5)
#define P1CR_PNTR_FLOW			(1 << 4)
#define P1CR_PNTR_100BT_FDX		(1 << 3)
#define P1CR_PNTR_100BT_HDX		(1 << 2)
#define P1CR_PNTR_10BT_FDX		(1 << 1)
#define P1CR_PNTR_10BT_HDX		(1 << 0)


#define TXFR_TXIC			(1 << 15)
#define TXFR_TXFID_MASK			(0x3f << 0)
#define TXFR_TXFID_SHIFT		(0)

#define KS_P1SR				0xF8
#define P1SR_HP_MDIX			(1 << 15)
#define P1SR_REV_POL			(1 << 13)
#define P1SR_OP_100M			(1 << 10)
#define P1SR_OP_FDX			(1 << 9)
#define P1SR_OP_MDI			(1 << 7)
#define P1SR_AN_DONE			(1 << 6)
#define P1SR_LINK_GOOD			(1 << 5)
#define P1SR_PNTR_FLOW			(1 << 4)
#define P1SR_PNTR_100BT_FDX		(1 << 3)
#define P1SR_PNTR_100BT_HDX		(1 << 2)
#define P1SR_PNTR_10BT_FDX		(1 << 1)
#define P1SR_PNTR_10BT_HDX		(1 << 0)

#define	ENUM_BUS_NONE			0
#define	ENUM_BUS_8BIT			1
#define	ENUM_BUS_16BIT			2
#define	ENUM_BUS_32BIT			3

#define MAX_MCAST_LST			32
#define HW_MCAST_SIZE			8

union ks_tx_hdr {
	u8      txb[4];
	__le16  txw[2];
};


struct type_frame_head {
	u16	sts;         
	u16	len;         
};

struct ks_net {
	struct net_device	*netdev;
	void __iomem    	*hw_addr;
	void __iomem    	*hw_addr_cmd;
	union ks_tx_hdr		txh ____cacheline_aligned;
	struct mutex      	lock; 
	struct platform_device *pdev;
	struct mii_if_info	mii;
	struct type_frame_head	*frame_head_info;
	spinlock_t		statelock;
	u32			msg_enable;
	u32			frame_cnt;
	int			bus_width;

	u16			rc_rxqcr;
	u16			rc_txcr;
	u16			rc_ier;
	u16			sharedbus;
	u16			cmd_reg_cache;
	u16			cmd_reg_cache_int;
	u16			promiscuous;
	u16			all_mcast;
	u16			mcast_lst_size;
	u8			mcast_lst[MAX_MCAST_LST][ETH_ALEN];
	u8			mcast_bits[HW_MCAST_SIZE];
	u8			mac_addr[6];
	u8                      fid;
	u8			extra_byte;
	u8			enabled;
};

static int msg_enable;

#define BE3             0x8000      
#define BE2             0x4000      
#define BE1             0x2000      
#define BE0             0x1000      


static u8 ks_rdreg8(struct ks_net *ks, int offset)
{
	u16 data;
	u8 shift_bit = offset & 0x03;
	u8 shift_data = (offset & 1) << 3;
	ks->cmd_reg_cache = (u16) offset | (u16)(BE0 << shift_bit);
	iowrite16(ks->cmd_reg_cache, ks->hw_addr_cmd);
	data  = ioread16(ks->hw_addr);
	return (u8)(data >> shift_data);
}


static u16 ks_rdreg16(struct ks_net *ks, int offset)
{
	ks->cmd_reg_cache = (u16)offset | ((BE1 | BE0) << (offset & 0x02));
	iowrite16(ks->cmd_reg_cache, ks->hw_addr_cmd);
	return ioread16(ks->hw_addr);
}

static void ks_wrreg8(struct ks_net *ks, int offset, u8 value)
{
	u8  shift_bit = (offset & 0x03);
	u16 value_write = (u16)(value << ((offset & 1) << 3));
	ks->cmd_reg_cache = (u16)offset | (BE0 << shift_bit);
	iowrite16(ks->cmd_reg_cache, ks->hw_addr_cmd);
	iowrite16(value_write, ks->hw_addr);
}


static void ks_wrreg16(struct ks_net *ks, int offset, u16 value)
{
	ks->cmd_reg_cache = (u16)offset | ((BE1 | BE0) << (offset & 0x02));
	iowrite16(ks->cmd_reg_cache, ks->hw_addr_cmd);
	iowrite16(value, ks->hw_addr);
}

static inline void ks_inblk(struct ks_net *ks, u16 *wptr, u32 len)
{
	len >>= 1;
	while (len--)
		*wptr++ = (u16)ioread16(ks->hw_addr);
}

static inline void ks_outblk(struct ks_net *ks, u16 *wptr, u32 len)
{
	len >>= 1;
	while (len--)
		iowrite16(*wptr++, ks->hw_addr);
}

static void ks_disable_int(struct ks_net *ks)
{
	ks_wrreg16(ks, KS_IER, 0x0000);
}  

static void ks_enable_int(struct ks_net *ks)
{
	ks_wrreg16(ks, KS_IER, ks->rc_ier);
}  

static inline u16 ks_tx_fifo_space(struct ks_net *ks)
{
	return ks_rdreg16(ks, KS_TXMIR) & 0x1fff;
}

static inline void ks_save_cmd_reg(struct ks_net *ks)
{
	ks->cmd_reg_cache_int = ks->cmd_reg_cache;
}

static inline void ks_restore_cmd_reg(struct ks_net *ks)
{
	ks->cmd_reg_cache = ks->cmd_reg_cache_int;
	iowrite16(ks->cmd_reg_cache, ks->hw_addr_cmd);
}

static void ks_set_powermode(struct ks_net *ks, unsigned pwrmode)
{
	unsigned pmecr;

	netif_dbg(ks, hw, ks->netdev, "setting power mode %d\n", pwrmode);

	ks_rdreg16(ks, KS_GRR);
	pmecr = ks_rdreg16(ks, KS_PMECR);
	pmecr &= ~PMECR_PM_MASK;
	pmecr |= pwrmode;

	ks_wrreg16(ks, KS_PMECR, pmecr);
}

static void ks_read_config(struct ks_net *ks)
{
	u16 reg_data = 0;

	
	reg_data = ks_rdreg8(ks, KS_CCR) & 0x00FF;
	reg_data |= ks_rdreg8(ks, KS_CCR+1) << 8;

	
	ks->sharedbus = (reg_data & CCR_SHARED) == CCR_SHARED;


	if (reg_data & CCR_8BIT) {
		ks->bus_width = ENUM_BUS_8BIT;
		ks->extra_byte = 1;
	} else if (reg_data & CCR_16BIT) {
		ks->bus_width = ENUM_BUS_16BIT;
		ks->extra_byte = 2;
	} else {
		ks->bus_width = ENUM_BUS_32BIT;
		ks->extra_byte = 4;
	}
}

static void ks_soft_reset(struct ks_net *ks, unsigned op)
{
	
	ks_wrreg16(ks, KS_IER, 0x0000);
	ks_wrreg16(ks, KS_GRR, op);
	mdelay(10);	
	ks_wrreg16(ks, KS_GRR, 0);
	mdelay(1);	
}


void ks_enable_qmu(struct ks_net *ks)
{
	u16 w;

	w = ks_rdreg16(ks, KS_TXCR);
	
	ks_wrreg16(ks, KS_TXCR, w | TXCR_TXE);


	w = ks_rdreg16(ks, KS_RXQCR);
	ks_wrreg16(ks, KS_RXQCR, w | RXQCR_RXFCTE);

	
	w = ks_rdreg16(ks, KS_RXCR1);
	ks_wrreg16(ks, KS_RXCR1, w | RXCR1_RXE);
	ks->enabled = true;
}  

static void ks_disable_qmu(struct ks_net *ks)
{
	u16	w;

	w = ks_rdreg16(ks, KS_TXCR);

	
	w  &= ~TXCR_TXE;
	ks_wrreg16(ks, KS_TXCR, w);

	
	w = ks_rdreg16(ks, KS_RXCR1);
	w &= ~RXCR1_RXE ;
	ks_wrreg16(ks, KS_RXCR1, w);

	ks->enabled = false;

}  

static inline void ks_read_qmu(struct ks_net *ks, u16 *buf, u32 len)
{
	u32 r =  ks->extra_byte & 0x1 ;
	u32 w = ks->extra_byte - r;

	
	ks_wrreg16(ks, KS_RXFDPR, RXFDPR_RXFPAI);
	ks_wrreg8(ks, KS_RXQCR, (ks->rc_rxqcr | RXQCR_SDA) & 0xff);

	

	
	if (unlikely(r))
		ioread8(ks->hw_addr);
	ks_inblk(ks, buf, w + 2 + 2);

	
	ks_inblk(ks, buf, ALIGN(len, 4));

	
	ks_wrreg8(ks, KS_RXQCR, ks->rc_rxqcr);
}

static void ks_rcv(struct ks_net *ks, struct net_device *netdev)
{
	u32	i;
	struct type_frame_head *frame_hdr = ks->frame_head_info;
	struct sk_buff *skb;

	ks->frame_cnt = ks_rdreg16(ks, KS_RXFCTR) >> 8;

	
	for (i = 0; i < ks->frame_cnt; i++) {
		
		frame_hdr->sts = ks_rdreg16(ks, KS_RXFHSR);
		
		frame_hdr->len = ks_rdreg16(ks, KS_RXFHBCR);
		frame_hdr++;
	}

	frame_hdr = ks->frame_head_info;
	while (ks->frame_cnt--) {
		skb = netdev_alloc_skb(netdev, frame_hdr->len + 16);
		if (likely(skb && (frame_hdr->sts & RXFSHR_RXFV) &&
			(frame_hdr->len < RX_BUF_SIZE) && frame_hdr->len)) {
			skb_reserve(skb, 2);
			
			ks_read_qmu(ks, (u16 *)skb->data, frame_hdr->len);
			skb_put(skb, frame_hdr->len);
			skb->protocol = eth_type_trans(skb, netdev);
			netif_rx(skb);
		} else {
			pr_err("%s: err:skb alloc\n", __func__);
			ks_wrreg16(ks, KS_RXQCR, (ks->rc_rxqcr | RXQCR_RRXEF));
			if (skb)
				dev_kfree_skb_irq(skb);
		}
		frame_hdr++;
	}
}


static void ks_update_link_status(struct net_device *netdev, struct ks_net *ks)
{
	
	u32 link_up_status;
	if (ks_rdreg16(ks, KS_P1SR) & P1SR_LINK_GOOD) {
		netif_carrier_on(netdev);
		link_up_status = true;
	} else {
		netif_carrier_off(netdev);
		link_up_status = false;
	}
	netif_dbg(ks, link, ks->netdev,
		  "%s: %s\n", __func__, link_up_status ? "UP" : "DOWN");
}


static irqreturn_t ks_irq(int irq, void *pw)
{
	struct net_device *netdev = pw;
	struct ks_net *ks = netdev_priv(netdev);
	u16 status;

	
	ks_save_cmd_reg(ks);

	status = ks_rdreg16(ks, KS_ISR);
	if (unlikely(!status)) {
		ks_restore_cmd_reg(ks);
		return IRQ_NONE;
	}

	ks_wrreg16(ks, KS_ISR, status);

	if (likely(status & IRQ_RXI))
		ks_rcv(ks, netdev);

	if (unlikely(status & IRQ_LCI))
		ks_update_link_status(netdev, ks);

	if (unlikely(status & IRQ_TXI))
		netif_wake_queue(netdev);

	if (unlikely(status & IRQ_LDI)) {

		u16 pmecr = ks_rdreg16(ks, KS_PMECR);
		pmecr &= ~PMECR_WKEVT_MASK;
		ks_wrreg16(ks, KS_PMECR, pmecr | PMECR_WKEVT_LINK);
	}

	
	ks_restore_cmd_reg(ks);
	return IRQ_HANDLED;
}


static int ks_net_open(struct net_device *netdev)
{
	struct ks_net *ks = netdev_priv(netdev);
	int err;

#define	KS_INT_FLAGS	(IRQF_DISABLED|IRQF_TRIGGER_LOW)

	netif_dbg(ks, ifup, ks->netdev, "%s - entry\n", __func__);

	
	err = request_irq(netdev->irq, ks_irq, KS_INT_FLAGS, DRV_NAME, netdev);

	if (err) {
		pr_err("Failed to request IRQ: %d: %d\n", netdev->irq, err);
		return err;
	}

	
	ks_set_powermode(ks, PMECR_PM_NORMAL);
	mdelay(1);	

	ks_wrreg16(ks, KS_ISR, 0xffff);
	ks_enable_int(ks);
	ks_enable_qmu(ks);
	netif_start_queue(ks->netdev);

	netif_dbg(ks, ifup, ks->netdev, "network device up\n");

	return 0;
}

static int ks_net_stop(struct net_device *netdev)
{
	struct ks_net *ks = netdev_priv(netdev);

	netif_info(ks, ifdown, netdev, "shutting down\n");

	netif_stop_queue(netdev);

	mutex_lock(&ks->lock);

	
	ks_wrreg16(ks, KS_IER, 0x0000);
	ks_wrreg16(ks, KS_ISR, 0xffff);

	
	ks_disable_qmu(ks);

	
	ks_set_powermode(ks, PMECR_PM_SOFTDOWN);
	free_irq(netdev->irq, netdev);
	mutex_unlock(&ks->lock);
	return 0;
}


static void ks_write_qmu(struct ks_net *ks, u8 *pdata, u16 len)
{
	
	ks->txh.txw[0] = 0;
	ks->txh.txw[1] = cpu_to_le16(len);

	
	ks_wrreg8(ks, KS_RXQCR, (ks->rc_rxqcr | RXQCR_SDA) & 0xff);
	
	ks_outblk(ks, ks->txh.txw, 4);
	
	ks_outblk(ks, (u16 *)pdata, ALIGN(len, 4));
	
	ks_wrreg8(ks, KS_RXQCR, ks->rc_rxqcr);
	
	ks_wrreg16(ks, KS_TXQCR, TXQCR_METFE);
	
	while (ks_rdreg16(ks, KS_TXQCR) & TXQCR_METFE)
		;
}

static int ks_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	int retv = NETDEV_TX_OK;
	struct ks_net *ks = netdev_priv(netdev);

	disable_irq(netdev->irq);
	ks_disable_int(ks);
	spin_lock(&ks->statelock);


	if (likely(ks_tx_fifo_space(ks) >= skb->len + 12)) {
		ks_write_qmu(ks, skb->data, skb->len);
		dev_kfree_skb(skb);
	} else
		retv = NETDEV_TX_BUSY;
	spin_unlock(&ks->statelock);
	ks_enable_int(ks);
	enable_irq(netdev->irq);
	return retv;
}

static void ks_start_rx(struct ks_net *ks)
{
	u16 cntl;

	
	cntl = ks_rdreg16(ks, KS_RXCR1);
	cntl |= RXCR1_RXE ;
	ks_wrreg16(ks, KS_RXCR1, cntl);
}  

static void ks_stop_rx(struct ks_net *ks)
{
	u16 cntl;

	
	cntl = ks_rdreg16(ks, KS_RXCR1);
	cntl &= ~RXCR1_RXE ;
	ks_wrreg16(ks, KS_RXCR1, cntl);

}  

static unsigned long const ethernet_polynomial = 0x04c11db7U;

static unsigned long ether_gen_crc(int length, u8 *data)
{
	long crc = -1;
	while (--length >= 0) {
		u8 current_octet = *data++;
		int bit;

		for (bit = 0; bit < 8; bit++, current_octet >>= 1) {
			crc = (crc << 1) ^
				((crc < 0) ^ (current_octet & 1) ?
			ethernet_polynomial : 0);
		}
	}
	return (unsigned long)crc;
}  


static void ks_set_grpaddr(struct ks_net *ks)
{
	u8	i;
	u32	index, position, value;

	memset(ks->mcast_bits, 0, sizeof(u8) * HW_MCAST_SIZE);

	for (i = 0; i < ks->mcast_lst_size; i++) {
		position = (ether_gen_crc(6, ks->mcast_lst[i]) >> 26) & 0x3f;
		index = position >> 3;
		value = 1 << (position & 7);
		ks->mcast_bits[index] |= (u8)value;
	}

	for (i  = 0; i < HW_MCAST_SIZE; i++) {
		if (i & 1) {
			ks_wrreg16(ks, (u16)((KS_MAHTR0 + i) & ~1),
				(ks->mcast_bits[i] << 8) |
				ks->mcast_bits[i - 1]);
		}
	}
}  


static void ks_clear_mcast(struct ks_net *ks)
{
	u16	i, mcast_size;
	for (i = 0; i < HW_MCAST_SIZE; i++)
		ks->mcast_bits[i] = 0;

	mcast_size = HW_MCAST_SIZE >> 2;
	for (i = 0; i < mcast_size; i++)
		ks_wrreg16(ks, KS_MAHTR0 + (2*i), 0);
}

static void ks_set_promis(struct ks_net *ks, u16 promiscuous_mode)
{
	u16		cntl;
	ks->promiscuous = promiscuous_mode;
	ks_stop_rx(ks);  
	cntl = ks_rdreg16(ks, KS_RXCR1);

	cntl &= ~RXCR1_FILTER_MASK;
	if (promiscuous_mode)
		
		cntl |= RXCR1_RXAE | RXCR1_RXINVF;
	else
		
		cntl |= RXCR1_RXPAFMA;

	ks_wrreg16(ks, KS_RXCR1, cntl);

	if (ks->enabled)
		ks_start_rx(ks);

}  

static void ks_set_mcast(struct ks_net *ks, u16 mcast)
{
	u16	cntl;

	ks->all_mcast = mcast;
	ks_stop_rx(ks);  
	cntl = ks_rdreg16(ks, KS_RXCR1);
	cntl &= ~RXCR1_FILTER_MASK;
	if (mcast)
		
		cntl |= (RXCR1_RXAE | RXCR1_RXMAFMA | RXCR1_RXPAFMA);
	else
		cntl |= RXCR1_RXPAFMA;

	ks_wrreg16(ks, KS_RXCR1, cntl);

	if (ks->enabled)
		ks_start_rx(ks);
}  

static void ks_set_rx_mode(struct net_device *netdev)
{
	struct ks_net *ks = netdev_priv(netdev);
	struct netdev_hw_addr *ha;

	
	if ((netdev->flags & IFF_PROMISC) == IFF_PROMISC)
		ks_set_promis(ks,
			(u16)((netdev->flags & IFF_PROMISC) == IFF_PROMISC));
	
	else if ((netdev->flags & IFF_ALLMULTI) == IFF_ALLMULTI)
		ks_set_mcast(ks,
			(u16)((netdev->flags & IFF_ALLMULTI) == IFF_ALLMULTI));
	else
		ks_set_promis(ks, false);

	if ((netdev->flags & IFF_MULTICAST) && netdev_mc_count(netdev)) {
		if (netdev_mc_count(netdev) <= MAX_MCAST_LST) {
			int i = 0;

			netdev_for_each_mc_addr(ha, netdev) {
				if (i >= MAX_MCAST_LST)
					break;
				memcpy(ks->mcast_lst[i++], ha->addr, ETH_ALEN);
			}
			ks->mcast_lst_size = (u8)i;
			ks_set_grpaddr(ks);
		} else {
			ks->mcast_lst_size = MAX_MCAST_LST;
			ks_set_mcast(ks, true);
		}
	} else {
		ks->mcast_lst_size = 0;
		ks_clear_mcast(ks);
	}
} 

static void ks_set_mac(struct ks_net *ks, u8 *data)
{
	u16 *pw = (u16 *)data;
	u16 w, u;

	ks_stop_rx(ks);  

	u = *pw++;
	w = ((u & 0xFF) << 8) | ((u >> 8) & 0xFF);
	ks_wrreg16(ks, KS_MARH, w);

	u = *pw++;
	w = ((u & 0xFF) << 8) | ((u >> 8) & 0xFF);
	ks_wrreg16(ks, KS_MARM, w);

	u = *pw;
	w = ((u & 0xFF) << 8) | ((u >> 8) & 0xFF);
	ks_wrreg16(ks, KS_MARL, w);

	memcpy(ks->mac_addr, data, 6);

	if (ks->enabled)
		ks_start_rx(ks);
}

static int ks_set_mac_address(struct net_device *netdev, void *paddr)
{
	struct ks_net *ks = netdev_priv(netdev);
	struct sockaddr *addr = paddr;
	u8 *da;

	netdev->addr_assign_type &= ~NET_ADDR_RANDOM;
	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);

	da = (u8 *)netdev->dev_addr;

	ks_set_mac(ks, da);
	return 0;
}

static int ks_net_ioctl(struct net_device *netdev, struct ifreq *req, int cmd)
{
	struct ks_net *ks = netdev_priv(netdev);

	if (!netif_running(netdev))
		return -EINVAL;

	return generic_mii_ioctl(&ks->mii, if_mii(req), cmd, NULL);
}

static const struct net_device_ops ks_netdev_ops = {
	.ndo_open		= ks_net_open,
	.ndo_stop		= ks_net_stop,
	.ndo_do_ioctl		= ks_net_ioctl,
	.ndo_start_xmit		= ks_start_xmit,
	.ndo_set_mac_address	= ks_set_mac_address,
	.ndo_set_rx_mode	= ks_set_rx_mode,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
};


static void ks_get_drvinfo(struct net_device *netdev,
			       struct ethtool_drvinfo *di)
{
	strlcpy(di->driver, DRV_NAME, sizeof(di->driver));
	strlcpy(di->version, "1.00", sizeof(di->version));
	strlcpy(di->bus_info, dev_name(netdev->dev.parent),
		sizeof(di->bus_info));
}

static u32 ks_get_msglevel(struct net_device *netdev)
{
	struct ks_net *ks = netdev_priv(netdev);
	return ks->msg_enable;
}

static void ks_set_msglevel(struct net_device *netdev, u32 to)
{
	struct ks_net *ks = netdev_priv(netdev);
	ks->msg_enable = to;
}

static int ks_get_settings(struct net_device *netdev, struct ethtool_cmd *cmd)
{
	struct ks_net *ks = netdev_priv(netdev);
	return mii_ethtool_gset(&ks->mii, cmd);
}

static int ks_set_settings(struct net_device *netdev, struct ethtool_cmd *cmd)
{
	struct ks_net *ks = netdev_priv(netdev);
	return mii_ethtool_sset(&ks->mii, cmd);
}

static u32 ks_get_link(struct net_device *netdev)
{
	struct ks_net *ks = netdev_priv(netdev);
	return mii_link_ok(&ks->mii);
}

static int ks_nway_reset(struct net_device *netdev)
{
	struct ks_net *ks = netdev_priv(netdev);
	return mii_nway_restart(&ks->mii);
}

static const struct ethtool_ops ks_ethtool_ops = {
	.get_drvinfo	= ks_get_drvinfo,
	.get_msglevel	= ks_get_msglevel,
	.set_msglevel	= ks_set_msglevel,
	.get_settings	= ks_get_settings,
	.set_settings	= ks_set_settings,
	.get_link	= ks_get_link,
	.nway_reset	= ks_nway_reset,
};


static int ks_phy_reg(int reg)
{
	switch (reg) {
	case MII_BMCR:
		return KS_P1MBCR;
	case MII_BMSR:
		return KS_P1MBSR;
	case MII_PHYSID1:
		return KS_PHY1ILR;
	case MII_PHYSID2:
		return KS_PHY1IHR;
	case MII_ADVERTISE:
		return KS_P1ANAR;
	case MII_LPA:
		return KS_P1ANLPR;
	}

	return 0x0;
}

static int ks_phy_read(struct net_device *netdev, int phy_addr, int reg)
{
	struct ks_net *ks = netdev_priv(netdev);
	int ksreg;
	int result;

	ksreg = ks_phy_reg(reg);
	if (!ksreg)
		return 0x0;	

	mutex_lock(&ks->lock);
	result = ks_rdreg16(ks, ksreg);
	mutex_unlock(&ks->lock);

	return result;
}

static void ks_phy_write(struct net_device *netdev,
			     int phy, int reg, int value)
{
	struct ks_net *ks = netdev_priv(netdev);
	int ksreg;

	ksreg = ks_phy_reg(reg);
	if (ksreg) {
		mutex_lock(&ks->lock);
		ks_wrreg16(ks, ksreg, value);
		mutex_unlock(&ks->lock);
	}
}

static int ks_read_selftest(struct ks_net *ks)
{
	unsigned both_done = MBIR_TXMBF | MBIR_RXMBF;
	int ret = 0;
	unsigned rd;

	rd = ks_rdreg16(ks, KS_MBIR);

	if ((rd & both_done) != both_done) {
		netdev_warn(ks->netdev, "Memory selftest not finished\n");
		return 0;
	}

	if (rd & MBIR_TXMBFA) {
		netdev_err(ks->netdev, "TX memory selftest fails\n");
		ret |= 1;
	}

	if (rd & MBIR_RXMBFA) {
		netdev_err(ks->netdev, "RX memory selftest fails\n");
		ret |= 2;
	}

	netdev_info(ks->netdev, "the selftest passes\n");
	return ret;
}

static void ks_setup(struct ks_net *ks)
{
	u16	w;


	
	ks_wrreg16(ks, KS_TXFDPR, TXFDPR_TXFPAI);

	
	ks_wrreg16(ks, KS_RXFDPR, RXFDPR_RXFPAI);

	
	ks_wrreg16(ks, KS_RXFCTR, 1 & RXFCTR_THRESHOLD_MASK);

	
	ks->rc_rxqcr = RXQCR_CMD_CNTL;
	ks_wrreg16(ks, KS_RXQCR, ks->rc_rxqcr);


	w = ks_rdreg16(ks, KS_P1MBCR);
	w &= ~P1MBCR_FORCE_FDX;
	ks_wrreg16(ks, KS_P1MBCR, w);

	w = TXCR_TXFCE | TXCR_TXPE | TXCR_TXCRC | TXCR_TCGIP;
	ks_wrreg16(ks, KS_TXCR, w);

	w = RXCR1_RXFCE | RXCR1_RXBE | RXCR1_RXUE | RXCR1_RXME | RXCR1_RXIPFCC;

	if (ks->promiscuous)         
		w |= (RXCR1_RXAE | RXCR1_RXINVF);
	else if (ks->all_mcast) 
		w |= (RXCR1_RXAE | RXCR1_RXMAFMA | RXCR1_RXPAFMA);
	else                                   
		w |= RXCR1_RXPAFMA;

	ks_wrreg16(ks, KS_RXCR1, w);
}  


static void ks_setup_int(struct ks_net *ks)
{
	ks->rc_ier = 0x00;
	
	ks_wrreg16(ks, KS_ISR, 0xffff);

	
	ks->rc_ier = (IRQ_LCI | IRQ_TXI | IRQ_RXI);
}  

static int ks_hw_init(struct ks_net *ks)
{
#define	MHEADER_SIZE	(sizeof(struct type_frame_head) * MAX_RECV_FRAMES)
	ks->promiscuous = 0;
	ks->all_mcast = 0;
	ks->mcast_lst_size = 0;

	ks->frame_head_info = kmalloc(MHEADER_SIZE, GFP_KERNEL);
	if (!ks->frame_head_info)
		return false;

	ks_set_mac(ks, KS_DEFAULT_MAC_ADDRESS);
	return true;
}


static int __devinit ks8851_probe(struct platform_device *pdev)
{
	int err = -ENOMEM;
	struct resource *io_d, *io_c;
	struct net_device *netdev;
	struct ks_net *ks;
	u16 id, data;

	io_d = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	io_c = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!request_mem_region(io_d->start, resource_size(io_d), DRV_NAME))
		goto err_mem_region;

	if (!request_mem_region(io_c->start, resource_size(io_c), DRV_NAME))
		goto err_mem_region1;

	netdev = alloc_etherdev(sizeof(struct ks_net));
	if (!netdev)
		goto err_alloc_etherdev;

	SET_NETDEV_DEV(netdev, &pdev->dev);

	ks = netdev_priv(netdev);
	ks->netdev = netdev;
	ks->hw_addr = ioremap(io_d->start, resource_size(io_d));

	if (!ks->hw_addr)
		goto err_ioremap;

	ks->hw_addr_cmd = ioremap(io_c->start, resource_size(io_c));
	if (!ks->hw_addr_cmd)
		goto err_ioremap1;

	netdev->irq = platform_get_irq(pdev, 0);

	if ((int)netdev->irq < 0) {
		err = netdev->irq;
		goto err_get_irq;
	}

	ks->pdev = pdev;

	mutex_init(&ks->lock);
	spin_lock_init(&ks->statelock);

	netdev->netdev_ops = &ks_netdev_ops;
	netdev->ethtool_ops = &ks_ethtool_ops;

	
	ks->mii.dev             = netdev;
	ks->mii.phy_id          = 1,
	ks->mii.phy_id_mask     = 1;
	ks->mii.reg_num_mask    = 0xf;
	ks->mii.mdio_read       = ks_phy_read;
	ks->mii.mdio_write      = ks_phy_write;

	netdev_info(netdev, "message enable is %d\n", msg_enable);
	
	ks->msg_enable = netif_msg_init(msg_enable, (NETIF_MSG_DRV |
						     NETIF_MSG_PROBE |
						     NETIF_MSG_LINK));
	ks_read_config(ks);

	
	if ((ks_rdreg16(ks, KS_CIDER) & ~CIDER_REV_MASK) != CIDER_ID) {
		netdev_err(netdev, "failed to read device ID\n");
		err = -ENODEV;
		goto err_register;
	}

	if (ks_read_selftest(ks)) {
		netdev_err(netdev, "failed to read device ID\n");
		err = -ENODEV;
		goto err_register;
	}

	err = register_netdev(netdev);
	if (err)
		goto err_register;

	platform_set_drvdata(pdev, netdev);

	ks_soft_reset(ks, GRR_GSR);
	ks_hw_init(ks);
	ks_disable_qmu(ks);
	ks_setup(ks);
	ks_setup_int(ks);
	memcpy(netdev->dev_addr, ks->mac_addr, 6);

	data = ks_rdreg16(ks, KS_OBCR);
	ks_wrreg16(ks, KS_OBCR, data | OBCR_ODS_16MA);


	random_ether_addr(netdev->dev_addr);
	ks_set_mac(ks, netdev->dev_addr);

	id = ks_rdreg16(ks, KS_CIDER);

	netdev_info(netdev, "Found chip, family: 0x%x, id: 0x%x, rev: 0x%x\n",
		    (id >> 8) & 0xff, (id >> 4) & 0xf, (id >> 1) & 0x7);
	return 0;

err_register:
err_get_irq:
	iounmap(ks->hw_addr_cmd);
err_ioremap1:
	iounmap(ks->hw_addr);
err_ioremap:
	free_netdev(netdev);
err_alloc_etherdev:
	release_mem_region(io_c->start, resource_size(io_c));
err_mem_region1:
	release_mem_region(io_d->start, resource_size(io_d));
err_mem_region:
	return err;
}

static int __devexit ks8851_remove(struct platform_device *pdev)
{
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct ks_net *ks = netdev_priv(netdev);
	struct resource *iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	kfree(ks->frame_head_info);
	unregister_netdev(netdev);
	iounmap(ks->hw_addr);
	free_netdev(netdev);
	release_mem_region(iomem->start, resource_size(iomem));
	platform_set_drvdata(pdev, NULL);
	return 0;

}

static struct platform_driver ks8851_platform_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = ks8851_probe,
	.remove = __devexit_p(ks8851_remove),
};

module_platform_driver(ks8851_platform_driver);

MODULE_DESCRIPTION("KS8851 MLL Network driver");
MODULE_AUTHOR("David Choi <david.choi@micrel.com>");
MODULE_LICENSE("GPL");
module_param_named(message, msg_enable, int, 0);
MODULE_PARM_DESC(message, "Message verbosity level (0=none, 31=all)");

