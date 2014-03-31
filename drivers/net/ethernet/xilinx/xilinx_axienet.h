/*
 * Definitions for Xilinx Axi Ethernet device driver.
 *
 * Copyright (c) 2009 Secret Lab Technologies, Ltd.
 * Copyright (c) 2010 - 2012 Xilinx, Inc. All rights reserved.
 */

#ifndef XILINX_AXIENET_H
#define XILINX_AXIENET_H

#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#define XAE_HDR_SIZE			14 
#define XAE_HDR_VLAN_SIZE		18 
#define XAE_TRL_SIZE			 4 
#define XAE_MTU			      1500 
#define XAE_JUMBO_MTU		      9000 

#define XAE_MAX_FRAME_SIZE	 (XAE_MTU + XAE_HDR_SIZE + XAE_TRL_SIZE)
#define XAE_MAX_VLAN_FRAME_SIZE  (XAE_MTU + XAE_HDR_VLAN_SIZE + XAE_TRL_SIZE)
#define XAE_MAX_JUMBO_FRAME_SIZE (XAE_JUMBO_MTU + XAE_HDR_SIZE + XAE_TRL_SIZE)


#define XAE_OPTION_PROMISC			(1 << 0)

#define XAE_OPTION_JUMBO			(1 << 1)

#define XAE_OPTION_VLAN				(1 << 2)

#define XAE_OPTION_FLOW_CONTROL			(1 << 4)

#define XAE_OPTION_FCS_STRIP			(1 << 5)

#define XAE_OPTION_FCS_INSERT			(1 << 6)

#define XAE_OPTION_LENTYPE_ERR			(1 << 7)

#define XAE_OPTION_TXEN				(1 << 11)

#define XAE_OPTION_RXEN				(1 << 12)

#define XAE_OPTION_DEFAULTS				   \
				(XAE_OPTION_TXEN |	   \
				 XAE_OPTION_FLOW_CONTROL | \
				 XAE_OPTION_RXEN)


#define XAXIDMA_TX_CR_OFFSET	0x00000000 
#define XAXIDMA_TX_SR_OFFSET	0x00000004 
#define XAXIDMA_TX_CDESC_OFFSET	0x00000008 
#define XAXIDMA_TX_TDESC_OFFSET	0x00000010 

#define XAXIDMA_RX_CR_OFFSET	0x00000030 
#define XAXIDMA_RX_SR_OFFSET	0x00000034 
#define XAXIDMA_RX_CDESC_OFFSET	0x00000038 
#define XAXIDMA_RX_TDESC_OFFSET	0x00000040 

#define XAXIDMA_CR_RUNSTOP_MASK	0x00000001 
#define XAXIDMA_CR_RESET_MASK	0x00000004 

#define XAXIDMA_BD_NDESC_OFFSET		0x00 
#define XAXIDMA_BD_BUFA_OFFSET		0x08 
#define XAXIDMA_BD_CTRL_LEN_OFFSET	0x18 
#define XAXIDMA_BD_STS_OFFSET		0x1C 
#define XAXIDMA_BD_USR0_OFFSET		0x20 
#define XAXIDMA_BD_USR1_OFFSET		0x24 
#define XAXIDMA_BD_USR2_OFFSET		0x28 
#define XAXIDMA_BD_USR3_OFFSET		0x2C 
#define XAXIDMA_BD_USR4_OFFSET		0x30 
#define XAXIDMA_BD_ID_OFFSET		0x34 
#define XAXIDMA_BD_HAS_STSCNTRL_OFFSET	0x38 
#define XAXIDMA_BD_HAS_DRE_OFFSET	0x3C 

#define XAXIDMA_BD_HAS_DRE_SHIFT	8 
#define XAXIDMA_BD_HAS_DRE_MASK		0xF00 
#define XAXIDMA_BD_WORDLEN_MASK		0xFF 

#define XAXIDMA_BD_CTRL_LENGTH_MASK	0x007FFFFF 
#define XAXIDMA_BD_CTRL_TXSOF_MASK	0x08000000 
#define XAXIDMA_BD_CTRL_TXEOF_MASK	0x04000000 
#define XAXIDMA_BD_CTRL_ALL_MASK	0x0C000000 

#define XAXIDMA_DELAY_MASK		0xFF000000 
#define XAXIDMA_COALESCE_MASK		0x00FF0000 

#define XAXIDMA_DELAY_SHIFT		24
#define XAXIDMA_COALESCE_SHIFT		16

#define XAXIDMA_IRQ_IOC_MASK		0x00001000 
#define XAXIDMA_IRQ_DELAY_MASK		0x00002000 
#define XAXIDMA_IRQ_ERROR_MASK		0x00004000 
#define XAXIDMA_IRQ_ALL_MASK		0x00007000 

#define XAXIDMA_DFT_TX_THRESHOLD	24
#define XAXIDMA_DFT_TX_WAITBOUND	254
#define XAXIDMA_DFT_RX_THRESHOLD	24
#define XAXIDMA_DFT_RX_WAITBOUND	254

#define XAXIDMA_BD_CTRL_TXSOF_MASK	0x08000000 
#define XAXIDMA_BD_CTRL_TXEOF_MASK	0x04000000 
#define XAXIDMA_BD_CTRL_ALL_MASK	0x0C000000 

#define XAXIDMA_BD_STS_ACTUAL_LEN_MASK	0x007FFFFF 
#define XAXIDMA_BD_STS_COMPLETE_MASK	0x80000000 
#define XAXIDMA_BD_STS_DEC_ERR_MASK	0x40000000 
#define XAXIDMA_BD_STS_SLV_ERR_MASK	0x20000000 
#define XAXIDMA_BD_STS_INT_ERR_MASK	0x10000000 
#define XAXIDMA_BD_STS_ALL_ERR_MASK	0x70000000 
#define XAXIDMA_BD_STS_RXSOF_MASK	0x08000000 
#define XAXIDMA_BD_STS_RXEOF_MASK	0x04000000 
#define XAXIDMA_BD_STS_ALL_MASK		0xFC000000 

#define XAXIDMA_BD_MINIMUM_ALIGNMENT	0x40

#define XAE_RAF_OFFSET		0x00000000 
#define XAE_TPF_OFFSET		0x00000004 
#define XAE_IFGP_OFFSET		0x00000008 
#define XAE_IS_OFFSET		0x0000000C 
#define XAE_IP_OFFSET		0x00000010 
#define XAE_IE_OFFSET		0x00000014 
#define XAE_TTAG_OFFSET		0x00000018 
#define XAE_RTAG_OFFSET		0x0000001C 
#define XAE_UAWL_OFFSET		0x00000020 
#define XAE_UAWU_OFFSET		0x00000024 
#define XAE_TPID0_OFFSET	0x00000028 
#define XAE_TPID1_OFFSET	0x0000002C 
#define XAE_PPST_OFFSET		0x00000030 
#define XAE_RCW0_OFFSET		0x00000400 
#define XAE_RCW1_OFFSET		0x00000404 
#define XAE_TC_OFFSET		0x00000408 
#define XAE_FCC_OFFSET		0x0000040C 
#define XAE_EMMC_OFFSET		0x00000410 
#define XAE_PHYC_OFFSET		0x00000414 
#define XAE_MDIO_MC_OFFSET	0x00000500 
#define XAE_MDIO_MCR_OFFSET	0x00000504 
#define XAE_MDIO_MWD_OFFSET	0x00000508 
#define XAE_MDIO_MRD_OFFSET	0x0000050C 
#define XAE_MDIO_MIS_OFFSET	0x00000600 
#define XAE_MDIO_MIP_OFFSET	0x00000620 
#define XAE_MDIO_MIE_OFFSET	0x00000640 
#define XAE_MDIO_MIC_OFFSET	0x00000660 
#define XAE_UAW0_OFFSET		0x00000700 
#define XAE_UAW1_OFFSET		0x00000704 
#define XAE_FMI_OFFSET		0x00000708 
#define XAE_AF0_OFFSET		0x00000710 
#define XAE_AF1_OFFSET		0x00000714 

#define XAE_TX_VLAN_DATA_OFFSET 0x00004000 
#define XAE_RX_VLAN_DATA_OFFSET 0x00008000 
#define XAE_MCAST_TABLE_OFFSET	0x00020000 

#define XAE_RAF_MCSTREJ_MASK		0x00000002 
#define XAE_RAF_BCSTREJ_MASK		0x00000004 
#define XAE_RAF_TXVTAGMODE_MASK		0x00000018 
#define XAE_RAF_RXVTAGMODE_MASK		0x00000060 
#define XAE_RAF_TXVSTRPMODE_MASK	0x00000180 
#define XAE_RAF_RXVSTRPMODE_MASK	0x00000600 
#define XAE_RAF_NEWFNCENBL_MASK		0x00000800 
#define XAE_RAF_EMULTIFLTRENBL_MASK	0x00001000 
#define XAE_RAF_STATSRST_MASK		0x00002000 
#define XAE_RAF_RXBADFRMEN_MASK		0x00004000 
#define XAE_RAF_TXVTAGMODE_SHIFT	3 
#define XAE_RAF_RXVTAGMODE_SHIFT	5 
#define XAE_RAF_TXVSTRPMODE_SHIFT	7 
#define XAE_RAF_RXVSTRPMODE_SHIFT	9 

#define XAE_TPF_TPFV_MASK		0x0000FFFF 
#define XAE_IFGP0_IFGP_MASK		0x0000007F 

#define XAE_INT_HARDACSCMPLT_MASK	0x00000001 
#define XAE_INT_AUTONEG_MASK		0x00000002 
#define XAE_INT_RXCMPIT_MASK		0x00000004 
#define XAE_INT_RXRJECT_MASK		0x00000008 
#define XAE_INT_RXFIFOOVR_MASK		0x00000010 
#define XAE_INT_TXCMPIT_MASK		0x00000020 
#define XAE_INT_RXDCMLOCK_MASK		0x00000040 
#define XAE_INT_MGTRDY_MASK		0x00000080 
#define XAE_INT_PHYRSTCMPLT_MASK	0x00000100 
#define XAE_INT_ALL_MASK		0x0000003F 

#define XAE_INT_RECV_ERROR_MASK				\
	(XAE_INT_RXRJECT_MASK | XAE_INT_RXFIFOOVR_MASK) 

#define XAE_TPID_0_MASK		0x0000FFFF 
#define XAE_TPID_1_MASK		0xFFFF0000 

#define XAE_TPID_2_MASK		0x0000FFFF 
#define XAE_TPID_3_MASK		0xFFFF0000 

#define XAE_RCW1_RST_MASK	0x80000000 
#define XAE_RCW1_JUM_MASK	0x40000000 
#define XAE_RCW1_FCS_MASK	0x20000000 
#define XAE_RCW1_RX_MASK	0x10000000 
#define XAE_RCW1_VLAN_MASK	0x08000000 
#define XAE_RCW1_LT_DIS_MASK	0x02000000 
#define XAE_RCW1_CL_DIS_MASK	0x01000000 
#define XAE_RCW1_PAUSEADDR_MASK 0x0000FFFF 

#define XAE_TC_RST_MASK		0x80000000 
#define XAE_TC_JUM_MASK		0x40000000 
#define XAE_TC_FCS_MASK		0x20000000 
#define XAE_TC_TX_MASK		0x10000000 
#define XAE_TC_VLAN_MASK	0x08000000 
#define XAE_TC_IFG_MASK		0x02000000 

#define XAE_FCC_FCRX_MASK	0x20000000 
#define XAE_FCC_FCTX_MASK	0x40000000 

#define XAE_EMMC_LINKSPEED_MASK	0xC0000000 
#define XAE_EMMC_RGMII_MASK	0x20000000 
#define XAE_EMMC_SGMII_MASK	0x10000000 
#define XAE_EMMC_GPCS_MASK	0x08000000 
#define XAE_EMMC_HOST_MASK	0x04000000 
#define XAE_EMMC_TX16BIT	0x02000000 
#define XAE_EMMC_RX16BIT	0x01000000 
#define XAE_EMMC_LINKSPD_10	0x00000000 
#define XAE_EMMC_LINKSPD_100	0x40000000 
#define XAE_EMMC_LINKSPD_1000	0x80000000 

#define XAE_PHYC_SGMIILINKSPEED_MASK	0xC0000000 
#define XAE_PHYC_RGMIILINKSPEED_MASK	0x0000000C 
#define XAE_PHYC_RGMIIHD_MASK		0x00000002 
#define XAE_PHYC_RGMIILINK_MASK		0x00000001 
#define XAE_PHYC_RGLINKSPD_10		0x00000000 
#define XAE_PHYC_RGLINKSPD_100		0x00000004 
#define XAE_PHYC_RGLINKSPD_1000		0x00000008 
#define XAE_PHYC_SGLINKSPD_10		0x00000000 
#define XAE_PHYC_SGLINKSPD_100		0x40000000 
#define XAE_PHYC_SGLINKSPD_1000		0x80000000 

#define XAE_MDIO_MC_MDIOEN_MASK		0x00000040 
#define XAE_MDIO_MC_CLOCK_DIVIDE_MAX	0x3F	   

#define XAE_MDIO_MCR_PHYAD_MASK		0x1F000000 
#define XAE_MDIO_MCR_PHYAD_SHIFT	24	   
#define XAE_MDIO_MCR_REGAD_MASK		0x001F0000 
#define XAE_MDIO_MCR_REGAD_SHIFT	16	   
#define XAE_MDIO_MCR_OP_MASK		0x0000C000 
#define XAE_MDIO_MCR_OP_SHIFT		13	   
#define XAE_MDIO_MCR_OP_READ_MASK	0x00008000 
#define XAE_MDIO_MCR_OP_WRITE_MASK	0x00004000 
#define XAE_MDIO_MCR_INITIATE_MASK	0x00000800 
#define XAE_MDIO_MCR_READY_MASK		0x00000080 

#define XAE_MDIO_INT_MIIM_RDY_MASK	0x00000001 

#define XAE_UAW1_UNICASTADDR_MASK	0x0000FFFF 

#define XAE_FMI_PM_MASK			0x80000000 
#define XAE_FMI_IND_MASK		0x00000003 

#define XAE_MDIO_DIV_DFT		29 

#define XAE_PHY_TYPE_MII		0
#define XAE_PHY_TYPE_GMII		1
#define XAE_PHY_TYPE_RGMII_1_3		2
#define XAE_PHY_TYPE_RGMII_2_0		3
#define XAE_PHY_TYPE_SGMII		4
#define XAE_PHY_TYPE_1000BASE_X		5

#define XAE_MULTICAST_CAM_TABLE_NUM	4 

#define XAE_FEATURE_PARTIAL_RX_CSUM	(1 << 0)
#define XAE_FEATURE_PARTIAL_TX_CSUM	(1 << 1)
#define XAE_FEATURE_FULL_RX_CSUM	(1 << 2)
#define XAE_FEATURE_FULL_TX_CSUM	(1 << 3)

#define XAE_NO_CSUM_OFFLOAD		0

#define XAE_FULL_CSUM_STATUS_MASK	0x00000038
#define XAE_IP_UDP_CSUM_VALIDATED	0x00000003
#define XAE_IP_TCP_CSUM_VALIDATED	0x00000002

#define DELAY_OF_ONE_MILLISEC		1000

struct axidma_bd {
	u32 next;	
	u32 reserved1;
	u32 phys;
	u32 reserved2;
	u32 reserved3;
	u32 reserved4;
	u32 cntrl;
	u32 status;
	u32 app0;
	u32 app1;	
	u32 app2;	
	u32 app3;
	u32 app4;
	u32 sw_id_offset;
	u32 reserved5;
	u32 reserved6;
};

struct axienet_local {
	struct net_device *ndev;
	struct device *dev;

	
	struct phy_device *phy_dev;	
	struct device_node *phy_node;

	
	struct mii_bus *mii_bus;	
	int mdio_irqs[PHY_MAX_ADDR];	

	
	void __iomem *regs;
	void __iomem *dma_regs;

	struct tasklet_struct dma_err_tasklet;

	int tx_irq;
	int rx_irq;
	u32 temac_type;
	u32 phy_type;

	u32 options;			
	u32 last_link;
	u32 features;

	
	struct axidma_bd *tx_bd_v;
	dma_addr_t tx_bd_p;
	struct axidma_bd *rx_bd_v;
	dma_addr_t rx_bd_p;
	u32 tx_bd_ci;
	u32 tx_bd_tail;
	u32 rx_bd_ci;

	u32 max_frm_size;
	u32 jumbo_support;

	int csum_offload_on_tx_path;
	int csum_offload_on_rx_path;

	u32 coalesce_count_rx;
	u32 coalesce_count_tx;
};

/**
 * struct axiethernet_option - Used to set axi ethernet hardware options
 * @opt:	Option to be set.
 * @reg:	Register offset to be written for setting the option
 * @m_or:	Mask to be ORed for setting the option in the register
 */
struct axienet_option {
	u32 opt;
	u32 reg;
	u32 m_or;
};

static inline u32 axienet_ior(struct axienet_local *lp, off_t offset)
{
	return in_be32(lp->regs + offset);
}

/**
 * axienet_iow - Memory mapped Axi Ethernet register write
 * @lp:         Pointer to axienet local structure
 * @offset:     Address offset from the base address of Axi Ethernet core
 * @value:      Value to be written into the Axi Ethernet register
 *
 * This function writes the desired value into the corresponding Axi Ethernet
 * register.
 */
static inline void axienet_iow(struct axienet_local *lp, off_t offset,
			       u32 value)
{
	out_be32((lp->regs + offset), value);
}

int axienet_mdio_setup(struct axienet_local *lp, struct device_node *np);
int axienet_mdio_wait_until_ready(struct axienet_local *lp);
void axienet_mdio_teardown(struct axienet_local *lp);

#endif 
