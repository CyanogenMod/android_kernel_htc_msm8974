
#ifndef XILINX_LL_TEMAC_H
#define XILINX_LL_TEMAC_H

#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/spinlock.h>

#ifdef CONFIG_PPC_DCR
#include <asm/dcr.h>
#include <asm/dcr-regs.h>
#endif

#define XTE_HDR_SIZE			14      
#define XTE_TRL_SIZE			4       
#define XTE_JUMBO_MTU			9000
#define XTE_MAX_JUMBO_FRAME_SIZE	(XTE_JUMBO_MTU + XTE_HDR_SIZE + XTE_TRL_SIZE)


#define XTE_OPTION_PROMISC                      (1 << 0)
#define XTE_OPTION_JUMBO                        (1 << 1)
#define XTE_OPTION_VLAN                         (1 << 2)
#define XTE_OPTION_FLOW_CONTROL                 (1 << 4)
#define XTE_OPTION_FCS_STRIP                    (1 << 5)
#define XTE_OPTION_FCS_INSERT                   (1 << 6)
#define XTE_OPTION_LENTYPE_ERR                  (1 << 7)
#define XTE_OPTION_TXEN                         (1 << 11)
#define XTE_OPTION_RXEN                         (1 << 12)

#define XTE_OPTION_DEFAULTS                     \
	(XTE_OPTION_TXEN |                          \
	 XTE_OPTION_FLOW_CONTROL |                  \
	 XTE_OPTION_RXEN)


#define TX_NXTDESC_PTR      0x00            
#define TX_CURBUF_ADDR      0x01            
#define TX_CURBUF_LENGTH    0x02            
#define TX_CURDESC_PTR      0x03            
#define TX_TAILDESC_PTR     0x04            
#define TX_CHNL_CTRL        0x05            
#define CHNL_CTRL_IRQ_IOE       (1 << 9)
#define CHNL_CTRL_IRQ_EN        (1 << 7)
#define CHNL_CTRL_IRQ_ERR_EN    (1 << 2)
#define CHNL_CTRL_IRQ_DLY_EN    (1 << 1)
#define CHNL_CTRL_IRQ_COAL_EN   (1 << 0)
#define TX_IRQ_REG          0x06            
#define TX_CHNL_STS         0x07            

#define RX_NXTDESC_PTR      0x08            
#define RX_CURBUF_ADDR      0x09            
#define RX_CURBUF_LENGTH    0x0a            
#define RX_CURDESC_PTR      0x0b            
#define RX_TAILDESC_PTR     0x0c            
#define RX_CHNL_CTRL        0x0d            
#define RX_IRQ_REG          0x0e            
#define IRQ_COAL        (1 << 0)
#define IRQ_DLY         (1 << 1)
#define IRQ_ERR         (1 << 2)
#define IRQ_DMAERR      (1 << 7)            
#define RX_CHNL_STS         0x0f        
#define CHNL_STS_ENGBUSY    (1 << 1)
#define CHNL_STS_EOP        (1 << 2)
#define CHNL_STS_SOP        (1 << 3)
#define CHNL_STS_CMPLT      (1 << 4)
#define CHNL_STS_SOE        (1 << 5)
#define CHNL_STS_IOE        (1 << 6)
#define CHNL_STS_ERR        (1 << 7)

#define CHNL_STS_BSYWR      (1 << 16)
#define CHNL_STS_CURPERR    (1 << 17)
#define CHNL_STS_NXTPERR    (1 << 18)
#define CHNL_STS_ADDRERR    (1 << 19)
#define CHNL_STS_CMPERR     (1 << 20)
#define CHNL_STS_TAILERR    (1 << 21)

#define DMA_CONTROL_REG             0x10            
#define DMA_CONTROL_RST                 (1 << 0)
#define DMA_TAIL_ENABLE                 (1 << 2)


#define XTE_RAF0_OFFSET              0x00
#define RAF0_RST                        (1 << 0)
#define RAF0_MCSTREJ                    (1 << 1)
#define RAF0_BCSTREJ                    (1 << 2)
#define XTE_TPF0_OFFSET              0x04
#define XTE_IFGP0_OFFSET             0x08
#define XTE_ISR0_OFFSET              0x0c
#define ISR0_HARDACSCMPLT               (1 << 0)
#define ISR0_AUTONEG                    (1 << 1)
#define ISR0_RXCMPLT                    (1 << 2)
#define ISR0_RXREJ                      (1 << 3)
#define ISR0_RXFIFOOVR                  (1 << 4)
#define ISR0_TXCMPLT                    (1 << 5)
#define ISR0_RXDCMLCK                   (1 << 6)

#define XTE_IPR0_OFFSET              0x10
#define XTE_IER0_OFFSET              0x14

#define XTE_MSW0_OFFSET              0x20
#define XTE_LSW0_OFFSET              0x24
#define XTE_CTL0_OFFSET              0x28
#define XTE_RDY0_OFFSET              0x2c

#define XTE_RSE_MIIM_RR_MASK      0x0002
#define XTE_RSE_MIIM_WR_MASK      0x0004
#define XTE_RSE_CFG_RR_MASK       0x0020
#define XTE_RSE_CFG_WR_MASK       0x0040
#define XTE_RDY0_HARD_ACS_RDY_MASK  (0x10000)


#define	XTE_RXC0_OFFSET			0x00000200 
#define	XTE_RXC1_OFFSET			0x00000240 
#define XTE_RXC1_RXRST_MASK		(1 << 31)  
#define XTE_RXC1_RXJMBO_MASK		(1 << 30)  
#define XTE_RXC1_RXFCS_MASK		(1 << 29)  
#define XTE_RXC1_RXEN_MASK		(1 << 28)  
#define XTE_RXC1_RXVLAN_MASK		(1 << 27)  
#define XTE_RXC1_RXHD_MASK		(1 << 26)  
#define XTE_RXC1_RXLT_MASK		(1 << 25)  

#define XTE_TXC_OFFSET			0x00000280 
#define XTE_TXC_TXRST_MASK		(1 << 31)  
#define XTE_TXC_TXJMBO_MASK		(1 << 30)  
#define XTE_TXC_TXFCS_MASK		(1 << 29)  
#define XTE_TXC_TXEN_MASK		(1 << 28)  
#define XTE_TXC_TXVLAN_MASK		(1 << 27)  
#define XTE_TXC_TXHD_MASK		(1 << 26)  

#define XTE_FCC_OFFSET			0x000002C0 
#define XTE_FCC_RXFLO_MASK		(1 << 29)  
#define XTE_FCC_TXFLO_MASK		(1 << 30)  

#define XTE_EMCFG_OFFSET		0x00000300 
#define XTE_EMCFG_LINKSPD_MASK		0xC0000000 
#define XTE_EMCFG_HOSTEN_MASK		(1 << 26)  
#define XTE_EMCFG_LINKSPD_10		0x00000000 
#define XTE_EMCFG_LINKSPD_100		(1 << 30)  
#define XTE_EMCFG_LINKSPD_1000		(1 << 31)  

#define XTE_GMIC_OFFSET			0x00000320 
#define XTE_MC_OFFSET			0x00000340 
#define XTE_UAW0_OFFSET			0x00000380 
#define XTE_UAW1_OFFSET			0x00000384 

#define XTE_MAW0_OFFSET			0x00000388 
#define XTE_MAW1_OFFSET			0x0000038C 
#define XTE_AFM_OFFSET			0x00000390 
#define XTE_AFM_EPPRM_MASK		(1 << 31)  

#define XTE_TIS_OFFSET			0x000003A0
#define TIS_FRIS			(1 << 0)
#define TIS_MRIS			(1 << 1)
#define TIS_MWIS			(1 << 2)
#define TIS_ARIS			(1 << 3)
#define TIS_AWIS			(1 << 4)
#define TIS_CRIS			(1 << 5)
#define TIS_CWIS			(1 << 6)

#define XTE_TIE_OFFSET			0x000003A4 

#define XTE_MGTDR_OFFSET		0x000003B0 
#define XTE_MIIMAI_OFFSET		0x000003B4 

#define CNTLREG_WRITE_ENABLE_MASK   0x8000
#define CNTLREG_EMAC1SEL_MASK       0x0400
#define CNTLREG_ADDRESSCODE_MASK    0x03ff


#define STS_CTRL_APP0_ERR         (1 << 31)
#define STS_CTRL_APP0_IRQONEND    (1 << 30)
#define STS_CTRL_APP0_STOPONEND   (1 << 29)
#define STS_CTRL_APP0_CMPLT       (1 << 28)
#define STS_CTRL_APP0_SOP         (1 << 27)
#define STS_CTRL_APP0_EOP         (1 << 26)
#define STS_CTRL_APP0_ENGBUSY     (1 << 25)
#define STS_CTRL_APP0_ENGRST      (1 << 24)

#define TX_CONTROL_CALC_CSUM_MASK   1

#define MULTICAST_CAM_TABLE_NUM 4

#define TEMAC_FEATURE_RX_CSUM  (1 << 0)
#define TEMAC_FEATURE_TX_CSUM  (1 << 1)


struct cdmac_bd {
	u32 next;	
	u32 phys;
	u32 len;
	u32 app0;
	u32 app1;	
	u32 app2;	
	u32 app3;
	u32 app4;	
};

struct temac_local {
	struct net_device *ndev;
	struct device *dev;

	
	struct phy_device *phy_dev;	
	struct device_node *phy_node;

	
	struct mii_bus *mii_bus;	
	int mdio_irqs[PHY_MAX_ADDR];	

	
	void __iomem *regs;
	void __iomem *sdma_regs;
#ifdef CONFIG_PPC_DCR
	dcr_host_t sdma_dcrs;
#endif
	u32 (*dma_in)(struct temac_local *, int);
	void (*dma_out)(struct temac_local *, int, u32);

	int tx_irq;
	int rx_irq;
	int emac_num;

	struct sk_buff **rx_skb;
	spinlock_t rx_lock;
	struct mutex indirect_mutex;
	u32 options;			
	int last_link;
	unsigned int temac_features;

	
	struct cdmac_bd *tx_bd_v;
	dma_addr_t tx_bd_p;
	struct cdmac_bd *rx_bd_v;
	dma_addr_t rx_bd_p;
	int tx_bd_ci;
	int tx_bd_next;
	int tx_bd_tail;
	int rx_bd_ci;
};

u32 temac_ior(struct temac_local *lp, int offset);
void temac_iow(struct temac_local *lp, int offset, u32 value);
int temac_indirect_busywait(struct temac_local *lp);
u32 temac_indirect_in32(struct temac_local *lp, int reg);
void temac_indirect_out32(struct temac_local *lp, int reg, u32 value);


int temac_mdio_setup(struct temac_local *lp, struct device_node *np);
void temac_mdio_teardown(struct temac_local *lp);

#endif 
