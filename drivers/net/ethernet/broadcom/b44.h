#ifndef _B44_H
#define _B44_H

#define	B44_DEVCTRL	0x0000UL 
#define  DEVCTRL_MPM		0x00000040 
#define  DEVCTRL_PFE		0x00000080 
#define  DEVCTRL_IPP		0x00000400 
#define  DEVCTRL_EPR		0x00008000 
#define  DEVCTRL_PME		0x00001000 
#define  DEVCTRL_PMCE		0x00002000 
#define  DEVCTRL_PADDR		0x0007c000 
#define  DEVCTRL_PADDR_SHIFT	18
#define B44_BIST_STAT	0x000CUL 
#define B44_WKUP_LEN	0x0010UL 
#define  WKUP_LEN_P0_MASK	0x0000007f 
#define  WKUP_LEN_D0		0x00000080
#define  WKUP_LEN_P1_MASK	0x00007f00 
#define  WKUP_LEN_P1_SHIFT	8
#define  WKUP_LEN_D1		0x00008000
#define  WKUP_LEN_P2_MASK	0x007f0000 
#define  WKUP_LEN_P2_SHIFT	16
#define  WKUP_LEN_D2		0x00000000
#define  WKUP_LEN_P3_MASK	0x7f000000 
#define  WKUP_LEN_P3_SHIFT	24
#define  WKUP_LEN_D3		0x80000000
#define  WKUP_LEN_DISABLE	0x80808080
#define  WKUP_LEN_ENABLE_TWO	0x80800000
#define  WKUP_LEN_ENABLE_THREE	0x80000000
#define B44_ISTAT	0x0020UL 
#define  ISTAT_LS		0x00000020 
#define  ISTAT_PME		0x00000040 
#define  ISTAT_TO		0x00000080 
#define  ISTAT_DSCE		0x00000400 
#define  ISTAT_DATAE		0x00000800 
#define  ISTAT_DPE		0x00001000 
#define  ISTAT_RDU		0x00002000 
#define  ISTAT_RFO		0x00004000 
#define  ISTAT_TFU		0x00008000 
#define  ISTAT_RX		0x00010000 
#define  ISTAT_TX		0x01000000 
#define  ISTAT_EMAC		0x04000000 
#define  ISTAT_MII_WRITE	0x08000000 
#define  ISTAT_MII_READ		0x10000000 
#define  ISTAT_ERRORS (ISTAT_DSCE|ISTAT_DATAE|ISTAT_DPE|ISTAT_RDU|ISTAT_RFO|ISTAT_TFU)
#define B44_IMASK	0x0024UL 
#define  IMASK_DEF		(ISTAT_ERRORS | ISTAT_TO | ISTAT_RX | ISTAT_TX)
#define B44_GPTIMER	0x0028UL 
#define B44_ADDR_LO	0x0088UL 
#define B44_ADDR_HI	0x008CUL 
#define B44_FILT_ADDR	0x0090UL 
#define B44_FILT_DATA	0x0094UL 
#define B44_TXBURST	0x00A0UL 
#define B44_RXBURST	0x00A4UL 
#define B44_MAC_CTRL	0x00A8UL 
#define  MAC_CTRL_CRC32_ENAB	0x00000001 
#define  MAC_CTRL_PHY_PDOWN	0x00000004 
#define  MAC_CTRL_PHY_EDET	0x00000008 
#define  MAC_CTRL_PHY_LEDCTRL	0x000000e0 
#define  MAC_CTRL_PHY_LEDCTRL_SHIFT 5
#define B44_MAC_FLOW	0x00ACUL 
#define  MAC_FLOW_RX_HI_WATER	0x000000ff 
#define  MAC_FLOW_PAUSE_ENAB	0x00008000 
#define B44_RCV_LAZY	0x0100UL 
#define  RCV_LAZY_TO_MASK	0x00ffffff 
#define  RCV_LAZY_FC_MASK	0xff000000 
#define  RCV_LAZY_FC_SHIFT	24
#define B44_DMATX_CTRL	0x0200UL 
#define  DMATX_CTRL_ENABLE	0x00000001 
#define  DMATX_CTRL_SUSPEND	0x00000002 
#define  DMATX_CTRL_LPBACK	0x00000004 
#define  DMATX_CTRL_FAIRPRIOR	0x00000008 
#define  DMATX_CTRL_FLUSH	0x00000010 
#define B44_DMATX_ADDR	0x0204UL 
#define B44_DMATX_PTR	0x0208UL 
#define B44_DMATX_STAT	0x020CUL 
#define  DMATX_STAT_CDMASK	0x00000fff 
#define  DMATX_STAT_SMASK	0x0000f000 
#define  DMATX_STAT_SDISABLED	0x00000000 
#define  DMATX_STAT_SACTIVE	0x00001000 
#define  DMATX_STAT_SIDLE	0x00002000 
#define  DMATX_STAT_SSTOPPED	0x00003000 
#define  DMATX_STAT_SSUSP	0x00004000 
#define  DMATX_STAT_EMASK	0x000f0000 
#define  DMATX_STAT_ENONE	0x00000000 
#define  DMATX_STAT_EDPE	0x00010000 
#define  DMATX_STAT_EDFU	0x00020000 
#define  DMATX_STAT_EBEBR	0x00030000 
#define  DMATX_STAT_EBEDA	0x00040000 
#define  DMATX_STAT_FLUSHED	0x00100000 
#define B44_DMARX_CTRL	0x0210UL 
#define  DMARX_CTRL_ENABLE	0x00000001 
#define  DMARX_CTRL_ROMASK	0x000000fe 
#define  DMARX_CTRL_ROSHIFT	1 	   
#define B44_DMARX_ADDR	0x0214UL 
#define B44_DMARX_PTR	0x0218UL 
#define B44_DMARX_STAT	0x021CUL 
#define  DMARX_STAT_CDMASK	0x00000fff 
#define  DMARX_STAT_SMASK	0x0000f000 
#define  DMARX_STAT_SDISABLED	0x00000000 
#define  DMARX_STAT_SACTIVE	0x00001000 
#define  DMARX_STAT_SIDLE	0x00002000 
#define  DMARX_STAT_SSTOPPED	0x00003000 
#define  DMARX_STAT_EMASK	0x000f0000 
#define  DMARX_STAT_ENONE	0x00000000 
#define  DMARX_STAT_EDPE	0x00010000 
#define  DMARX_STAT_EDFO	0x00020000 
#define  DMARX_STAT_EBEBW	0x00030000 
#define  DMARX_STAT_EBEDA	0x00040000 
#define B44_DMAFIFO_AD	0x0220UL 
#define  DMAFIFO_AD_OMASK	0x0000ffff 
#define  DMAFIFO_AD_SMASK	0x000f0000 
#define  DMAFIFO_AD_SXDD	0x00000000 
#define  DMAFIFO_AD_SXDP	0x00010000 
#define  DMAFIFO_AD_SRDD	0x00040000 
#define  DMAFIFO_AD_SRDP	0x00050000 
#define  DMAFIFO_AD_SXFD	0x00080000 
#define  DMAFIFO_AD_SXFP	0x00090000 
#define  DMAFIFO_AD_SRFD	0x000c0000 
#define  DMAFIFO_AD_SRFP	0x000c0000 
#define B44_DMAFIFO_LO	0x0224UL 
#define B44_DMAFIFO_HI	0x0228UL 
#define B44_RXCONFIG	0x0400UL 
#define  RXCONFIG_DBCAST	0x00000001 
#define  RXCONFIG_ALLMULTI	0x00000002 
#define  RXCONFIG_NORX_WHILE_TX	0x00000004 
#define  RXCONFIG_PROMISC	0x00000008 
#define  RXCONFIG_LPBACK	0x00000010 
#define  RXCONFIG_FLOW		0x00000020 
#define  RXCONFIG_FLOW_ACCEPT	0x00000040 
#define  RXCONFIG_RFILT		0x00000080 
#define  RXCONFIG_CAM_ABSENT	0x00000100 
#define B44_RXMAXLEN	0x0404UL 
#define B44_TXMAXLEN	0x0408UL 
#define B44_MDIO_CTRL	0x0410UL 
#define  MDIO_CTRL_MAXF_MASK	0x0000007f 
#define  MDIO_CTRL_PREAMBLE	0x00000080 
#define B44_MDIO_DATA	0x0414UL 
#define  MDIO_DATA_DATA		0x0000ffff 
#define  MDIO_DATA_TA_MASK	0x00030000 
#define  MDIO_DATA_TA_SHIFT	16
#define  MDIO_TA_VALID		2
#define  MDIO_DATA_RA_MASK	0x007c0000 
#define  MDIO_DATA_RA_SHIFT	18
#define  MDIO_DATA_PMD_MASK	0x0f800000 
#define  MDIO_DATA_PMD_SHIFT	23
#define  MDIO_DATA_OP_MASK	0x30000000 
#define  MDIO_DATA_OP_SHIFT	28
#define  MDIO_OP_WRITE		1
#define  MDIO_OP_READ		2
#define  MDIO_DATA_SB_MASK	0xc0000000 
#define  MDIO_DATA_SB_SHIFT	30
#define  MDIO_DATA_SB_START	0x40000000 
#define B44_EMAC_IMASK	0x0418UL 
#define B44_EMAC_ISTAT	0x041CUL 
#define  EMAC_INT_MII		0x00000001 
#define  EMAC_INT_MIB		0x00000002 
#define  EMAC_INT_FLOW		0x00000003 
#define B44_CAM_DATA_LO	0x0420UL 
#define B44_CAM_DATA_HI	0x0424UL 
#define  CAM_DATA_HI_VALID	0x00010000 
#define B44_CAM_CTRL	0x0428UL 
#define  CAM_CTRL_ENABLE	0x00000001 
#define  CAM_CTRL_MSEL		0x00000002 
#define  CAM_CTRL_READ		0x00000004 
#define  CAM_CTRL_WRITE		0x00000008 
#define  CAM_CTRL_INDEX_MASK	0x003f0000 
#define  CAM_CTRL_INDEX_SHIFT	16
#define  CAM_CTRL_BUSY		0x80000000 
#define B44_ENET_CTRL	0x042CUL 
#define  ENET_CTRL_ENABLE	0x00000001 
#define  ENET_CTRL_DISABLE	0x00000002 
#define  ENET_CTRL_SRST		0x00000004 
#define  ENET_CTRL_EPSEL	0x00000008 
#define B44_TX_CTRL	0x0430UL 
#define  TX_CTRL_DUPLEX		0x00000001 
#define  TX_CTRL_FMODE		0x00000002 
#define  TX_CTRL_SBENAB		0x00000004 
#define  TX_CTRL_SMALL_SLOT	0x00000008 
#define B44_TX_WMARK	0x0434UL 
#define B44_MIB_CTRL	0x0438UL 
#define  MIB_CTRL_CLR_ON_READ	0x00000001 
#define B44_TX_GOOD_O	0x0500UL 
#define B44_TX_GOOD_P	0x0504UL 
#define B44_TX_O	0x0508UL 
#define B44_TX_P	0x050CUL 
#define B44_TX_BCAST	0x0510UL 
#define B44_TX_MCAST	0x0514UL 
#define B44_TX_64	0x0518UL 
#define B44_TX_65_127	0x051CUL 
#define B44_TX_128_255	0x0520UL 
#define B44_TX_256_511	0x0524UL 
#define B44_TX_512_1023	0x0528UL 
#define B44_TX_1024_MAX	0x052CUL 
#define B44_TX_JABBER	0x0530UL 
#define B44_TX_OSIZE	0x0534UL 
#define B44_TX_FRAG	0x0538UL 
#define B44_TX_URUNS	0x053CUL 
#define B44_TX_TCOLS	0x0540UL 
#define B44_TX_SCOLS	0x0544UL 
#define B44_TX_MCOLS	0x0548UL 
#define B44_TX_ECOLS	0x054CUL 
#define B44_TX_LCOLS	0x0550UL 
#define B44_TX_DEFERED	0x0554UL 
#define B44_TX_CLOST	0x0558UL 
#define B44_TX_PAUSE	0x055CUL 
#define B44_RX_GOOD_O	0x0580UL 
#define B44_RX_GOOD_P	0x0584UL 
#define B44_RX_O	0x0588UL 
#define B44_RX_P	0x058CUL 
#define B44_RX_BCAST	0x0590UL 
#define B44_RX_MCAST	0x0594UL 
#define B44_RX_64	0x0598UL 
#define B44_RX_65_127	0x059CUL 
#define B44_RX_128_255	0x05A0UL 
#define B44_RX_256_511	0x05A4UL 
#define B44_RX_512_1023	0x05A8UL 
#define B44_RX_1024_MAX	0x05ACUL 
#define B44_RX_JABBER	0x05B0UL 
#define B44_RX_OSIZE	0x05B4UL 
#define B44_RX_FRAG	0x05B8UL 
#define B44_RX_MISS	0x05BCUL 
#define B44_RX_CRCA	0x05C0UL 
#define B44_RX_USIZE	0x05C4UL 
#define B44_RX_CRC	0x05C8UL 
#define B44_RX_ALIGN	0x05CCUL 
#define B44_RX_SYM	0x05D0UL 
#define B44_RX_PAUSE	0x05D4UL 
#define B44_RX_NPAUSE	0x05D8UL 

#define B44_MII_AUXCTRL		24	
#define  MII_AUXCTRL_DUPLEX	0x0001  
#define  MII_AUXCTRL_SPEED	0x0002  
#define  MII_AUXCTRL_FORCED	0x0004	
#define B44_MII_ALEDCTRL	26	
#define  MII_ALEDCTRL_ALLMSK	0x7fff
#define B44_MII_TLEDCTRL	27	
#define  MII_TLEDCTRL_ENABLE	0x0040

struct dma_desc {
	__le32	ctrl;
	__le32	addr;
};

#define DMA_TABLE_BYTES		4096

#define DESC_CTRL_LEN	0x00001fff
#define DESC_CTRL_CMASK	0x0ff00000 
#define DESC_CTRL_EOT	0x10000000 
#define DESC_CTRL_IOC	0x20000000 
#define DESC_CTRL_EOF	0x40000000 
#define DESC_CTRL_SOF	0x80000000 

#define RX_COPY_THRESHOLD  	256

struct rx_header {
	__le16	len;
	__le16	flags;
	__le16	pad[12];
};
#define RX_HEADER_LEN	28

#define RX_FLAG_OFIFO	0x00000001 
#define RX_FLAG_CRCERR	0x00000002 
#define RX_FLAG_SERR	0x00000004 
#define RX_FLAG_ODD	0x00000008 
#define RX_FLAG_LARGE	0x00000010 
#define RX_FLAG_MCAST	0x00000020 
#define RX_FLAG_BCAST	0x00000040 
#define RX_FLAG_MISS	0x00000080 
#define RX_FLAG_LAST	0x00000800 
#define RX_FLAG_ERRORS	(RX_FLAG_ODD | RX_FLAG_SERR | RX_FLAG_CRCERR | RX_FLAG_OFIFO)

struct ring_info {
	struct sk_buff		*skb;
	dma_addr_t	mapping;
};

#define B44_MCAST_TABLE_SIZE	32
#define B44_PHY_ADDR_NO_PHY	30
#define B44_MDC_RATIO		5000000

#define	B44_STAT_REG_DECLARE		\
	_B44(tx_good_octets)		\
	_B44(tx_good_pkts)		\
	_B44(tx_octets)			\
	_B44(tx_pkts)			\
	_B44(tx_broadcast_pkts)		\
	_B44(tx_multicast_pkts)		\
	_B44(tx_len_64)			\
	_B44(tx_len_65_to_127)		\
	_B44(tx_len_128_to_255)		\
	_B44(tx_len_256_to_511)		\
	_B44(tx_len_512_to_1023)	\
	_B44(tx_len_1024_to_max)	\
	_B44(tx_jabber_pkts)		\
	_B44(tx_oversize_pkts)		\
	_B44(tx_fragment_pkts)		\
	_B44(tx_underruns)		\
	_B44(tx_total_cols)		\
	_B44(tx_single_cols)		\
	_B44(tx_multiple_cols)		\
	_B44(tx_excessive_cols)		\
	_B44(tx_late_cols)		\
	_B44(tx_defered)		\
	_B44(tx_carrier_lost)		\
	_B44(tx_pause_pkts)		\
	_B44(rx_good_octets)		\
	_B44(rx_good_pkts)		\
	_B44(rx_octets)			\
	_B44(rx_pkts)			\
	_B44(rx_broadcast_pkts)		\
	_B44(rx_multicast_pkts)		\
	_B44(rx_len_64)			\
	_B44(rx_len_65_to_127)		\
	_B44(rx_len_128_to_255)		\
	_B44(rx_len_256_to_511)		\
	_B44(rx_len_512_to_1023)	\
	_B44(rx_len_1024_to_max)	\
	_B44(rx_jabber_pkts)		\
	_B44(rx_oversize_pkts)		\
	_B44(rx_fragment_pkts)		\
	_B44(rx_missed_pkts)		\
	_B44(rx_crc_align_errs)		\
	_B44(rx_undersize)		\
	_B44(rx_crc_errs)		\
	_B44(rx_align_errs)		\
	_B44(rx_symbol_errs)		\
	_B44(rx_pause_pkts)		\
	_B44(rx_nonpause_pkts)

struct b44_hw_stats {
#define _B44(x)	u32 x;
B44_STAT_REG_DECLARE
#undef _B44
};

struct ssb_device;

struct b44 {
	spinlock_t		lock;

	u32			imask, istat;

	struct dma_desc		*rx_ring, *tx_ring;

	u32			tx_prod, tx_cons;
	u32			rx_prod, rx_cons;

	struct ring_info	*rx_buffers;
	struct ring_info	*tx_buffers;

	struct napi_struct	napi;

	u32			dma_offset;
	u32			flags;
#define B44_FLAG_B0_ANDLATER	0x00000001
#define B44_FLAG_BUGGY_TXPTR	0x00000002
#define B44_FLAG_REORDER_BUG	0x00000004
#define B44_FLAG_PAUSE_AUTO	0x00008000
#define B44_FLAG_FULL_DUPLEX	0x00010000
#define B44_FLAG_100_BASE_T	0x00020000
#define B44_FLAG_TX_PAUSE	0x00040000
#define B44_FLAG_RX_PAUSE	0x00080000
#define B44_FLAG_FORCE_LINK	0x00100000
#define B44_FLAG_ADV_10HALF	0x01000000
#define B44_FLAG_ADV_10FULL	0x02000000
#define B44_FLAG_ADV_100HALF	0x04000000
#define B44_FLAG_ADV_100FULL	0x08000000
#define B44_FLAG_INTERNAL_PHY	0x10000000
#define B44_FLAG_RX_RING_HACK	0x20000000
#define B44_FLAG_TX_RING_HACK	0x40000000
#define B44_FLAG_WOL_ENABLE	0x80000000

	u32			msg_enable;

	struct timer_list	timer;

	struct b44_hw_stats	hw_stats;

	struct ssb_device	*sdev;
	struct net_device	*dev;

	dma_addr_t		rx_ring_dma, tx_ring_dma;

	u32			rx_pending;
	u32			tx_pending;
	u8			phy_addr;
	u8			force_copybreak;
	struct mii_if_info	mii_if;
};

#endif 
