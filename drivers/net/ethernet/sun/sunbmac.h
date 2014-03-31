/* $Id: sunbmac.h,v 1.7 2000/07/11 22:35:22 davem Exp $
 * sunbmac.h: Defines for the Sun "Big MAC" 100baseT ethernet cards.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SUNBMAC_H
#define _SUNBMAC_H

#define GLOB_CTRL	0x00UL	
#define GLOB_STAT	0x04UL	
#define GLOB_PSIZE	0x08UL	
#define GLOB_MSIZE	0x0cUL	
#define GLOB_RSIZE	0x10UL	
#define GLOB_TSIZE	0x14UL	
#define GLOB_REG_SIZE	0x18UL

#define GLOB_CTRL_MMODE       0x40000000 
#define GLOB_CTRL_BMODE       0x10000000 
#define GLOB_CTRL_EPAR        0x00000020 
#define GLOB_CTRL_ACNTRL      0x00000018 
#define GLOB_CTRL_B64         0x00000004 
#define GLOB_CTRL_B32         0x00000002 
#define GLOB_CTRL_B16         0x00000000 
#define GLOB_CTRL_RESET       0x00000001 

#define GLOB_STAT_TX          0x00000008 
#define GLOB_STAT_RX          0x00000004 
#define GLOB_STAT_BM          0x00000002 
#define GLOB_STAT_ER          0x00000001 

#define GLOB_PSIZE_2048       0x00       
#define GLOB_PSIZE_4096       0x01       
#define GLOB_PSIZE_6144       0x10       
#define GLOB_PSIZE_8192       0x11       

#define CREG_CTRL	0x00UL	
#define CREG_STAT	0x04UL	
#define CREG_RXDS	0x08UL	
#define CREG_TXDS	0x0cUL	
#define CREG_RIMASK	0x10UL	
#define CREG_TIMASK	0x14UL	
#define CREG_QMASK	0x18UL	
#define CREG_BMASK	0x1cUL	
#define CREG_RXWBUFPTR	0x20UL	
#define CREG_RXRBUFPTR	0x24UL	
#define CREG_TXWBUFPTR	0x28UL	
#define CREG_TXRBUFPTR	0x2cUL	
#define CREG_CCNT	0x30UL	
#define CREG_REG_SIZE	0x34UL

#define CREG_CTRL_TWAKEUP     0x00000001  

#define CREG_STAT_BERROR      0x80000000  
#define CREG_STAT_TXIRQ       0x00200000  
#define CREG_STAT_TXDERROR    0x00080000  
#define CREG_STAT_TXLERR      0x00040000  
#define CREG_STAT_TXPERR      0x00020000  
#define CREG_STAT_TXSERR      0x00010000  
#define CREG_STAT_RXIRQ       0x00000020  
#define CREG_STAT_RXDROP      0x00000010  
#define CREG_STAT_RXSMALL     0x00000008  
#define CREG_STAT_RXLERR      0x00000004  
#define CREG_STAT_RXPERR      0x00000002  
#define CREG_STAT_RXSERR      0x00000001  

#define CREG_STAT_ERRORS      (CREG_STAT_BERROR|CREG_STAT_TXDERROR|CREG_STAT_TXLERR|   \
                               CREG_STAT_TXPERR|CREG_STAT_TXSERR|CREG_STAT_RXDROP|     \
                               CREG_STAT_RXSMALL|CREG_STAT_RXLERR|CREG_STAT_RXPERR|    \
                               CREG_STAT_RXSERR)

#define CREG_QMASK_TXDERROR   0x00080000  
#define CREG_QMASK_TXLERR     0x00040000  
#define CREG_QMASK_TXPERR     0x00020000  
#define CREG_QMASK_TXSERR     0x00010000  
#define CREG_QMASK_RXDROP     0x00000010  
#define CREG_QMASK_RXBERROR   0x00000008  
#define CREG_QMASK_RXLEERR    0x00000004  
#define CREG_QMASK_RXPERR     0x00000002  
#define CREG_QMASK_RXSERR     0x00000001  

#define BMAC_XIFCFG	0x000UL	
	
#define BMAC_STATUS	0x100UL	
#define BMAC_IMASK	0x104UL	
	
#define BMAC_TXSWRESET	0x208UL	
#define BMAC_TXCFG	0x20cUL	
#define BMAC_IGAP1	0x210UL	
#define BMAC_IGAP2	0x214UL	
#define BMAC_ALIMIT	0x218UL	
#define BMAC_STIME	0x21cUL	
#define BMAC_PLEN	0x220UL	
#define BMAC_PPAT	0x224UL	
#define BMAC_TXDELIM	0x228UL	
#define BMAC_JSIZE	0x22cUL	
#define BMAC_TXPMAX	0x230UL	
#define BMAC_TXPMIN	0x234UL	
#define BMAC_PATTEMPT	0x238UL	
#define BMAC_DTCTR	0x23cUL	
#define BMAC_NCCTR	0x240UL	
#define BMAC_FCCTR	0x244UL	
#define BMAC_EXCTR	0x248UL	
#define BMAC_LTCTR	0x24cUL	
#define BMAC_RSEED	0x250UL	
#define BMAC_TXSMACHINE	0x254UL 
	
#define BMAC_RXSWRESET	0x308UL	
#define BMAC_RXCFG	0x30cUL	
#define BMAC_RXPMAX	0x310UL	
#define BMAC_RXPMIN	0x314UL	
#define BMAC_MACADDR2	0x318UL	
#define BMAC_MACADDR1	0x31cUL	
#define BMAC_MACADDR0	0x320UL	
#define BMAC_FRCTR	0x324UL	
#define BMAC_GLECTR	0x328UL	
#define BMAC_UNALECTR	0x32cUL	
#define BMAC_RCRCECTR	0x330UL	
#define BMAC_RXSMACHINE	0x334UL	
#define BMAC_RXCVALID	0x338UL	
	
#define BMAC_HTABLE3	0x340UL	
#define BMAC_HTABLE2	0x344UL	
#define BMAC_HTABLE1	0x348UL	
#define BMAC_HTABLE0	0x34cUL	
#define BMAC_AFILTER2	0x350UL	
#define BMAC_AFILTER1	0x354UL	
#define BMAC_AFILTER0	0x358UL	
#define BMAC_AFMASK	0x35cUL	
#define BMAC_REG_SIZE	0x360UL

#define BIGMAC_XCFG_ODENABLE   0x00000001 
#define BIGMAC_XCFG_RESV       0x00000002 
#define BIGMAC_XCFG_MLBACK     0x00000004 
#define BIGMAC_XCFG_SMODE      0x00000008 

#define BIGMAC_STAT_GOTFRAME   0x00000001 
#define BIGMAC_STAT_RCNTEXP    0x00000002 
#define BIGMAC_STAT_ACNTEXP    0x00000004 
#define BIGMAC_STAT_CCNTEXP    0x00000008 
#define BIGMAC_STAT_LCNTEXP    0x00000010 
#define BIGMAC_STAT_RFIFOVF    0x00000020 
#define BIGMAC_STAT_CVCNTEXP   0x00000040 
#define BIGMAC_STAT_SENTFRAME  0x00000100 
#define BIGMAC_STAT_TFIFO_UND  0x00000200 
#define BIGMAC_STAT_MAXPKTERR  0x00000400 
#define BIGMAC_STAT_NCNTEXP    0x00000800 
#define BIGMAC_STAT_ECNTEXP    0x00001000 
#define BIGMAC_STAT_LCCNTEXP   0x00002000 
#define BIGMAC_STAT_FCNTEXP    0x00004000 
#define BIGMAC_STAT_DTIMEXP    0x00008000 

#define BIGMAC_IMASK_GOTFRAME  0x00000001 
#define BIGMAC_IMASK_RCNTEXP   0x00000002 
#define BIGMAC_IMASK_ACNTEXP   0x00000004 
#define BIGMAC_IMASK_CCNTEXP   0x00000008 
#define BIGMAC_IMASK_LCNTEXP   0x00000010 
#define BIGMAC_IMASK_RFIFOVF   0x00000020 
#define BIGMAC_IMASK_CVCNTEXP  0x00000040 
#define BIGMAC_IMASK_SENTFRAME 0x00000100 
#define BIGMAC_IMASK_TFIFO_UND 0x00000200 
#define BIGMAC_IMASK_MAXPKTERR 0x00000400 
#define BIGMAC_IMASK_NCNTEXP   0x00000800 
#define BIGMAC_IMASK_ECNTEXP   0x00001000 
#define BIGMAC_IMASK_LCCNTEXP  0x00002000 
#define BIGMAC_IMASK_FCNTEXP   0x00004000 
#define BIGMAC_IMASK_DTIMEXP   0x00008000 

#define BIGMAC_TXCFG_ENABLE    0x00000001 
#define BIGMAC_TXCFG_FIFO      0x00000010 
#define BIGMAC_TXCFG_SMODE     0x00000020 
#define BIGMAC_TXCFG_CIGN      0x00000040 
#define BIGMAC_TXCFG_FCSOFF    0x00000080 
#define BIGMAC_TXCFG_DBACKOFF  0x00000100 
#define BIGMAC_TXCFG_FULLDPLX  0x00000200 

#define BIGMAC_RXCFG_ENABLE    0x00000001 
#define BIGMAC_RXCFG_FIFO      0x0000000e 
#define BIGMAC_RXCFG_PSTRIP    0x00000020 
#define BIGMAC_RXCFG_PMISC     0x00000040 
#define BIGMAC_RXCFG_DERR      0x00000080 
#define BIGMAC_RXCFG_DCRCS     0x00000100 
#define BIGMAC_RXCFG_ME        0x00000200 
#define BIGMAC_RXCFG_PGRP      0x00000400 
#define BIGMAC_RXCFG_HENABLE   0x00000800 
#define BIGMAC_RXCFG_AENABLE   0x00001000 

#define TCVR_TPAL	0x00UL
#define TCVR_MPAL	0x04UL
#define TCVR_REG_SIZE	0x08UL

#define FRAME_WRITE           0x50020000
#define FRAME_READ            0x60020000

#define TCVR_PAL_SERIAL       0x00000001 
#define TCVR_PAL_EXTLBACK     0x00000002 
#define TCVR_PAL_MSENSE       0x00000004 
#define TCVR_PAL_LTENABLE     0x00000008 
#define TCVR_PAL_LTSTATUS     0x00000010 

#define MGMT_PAL_DCLOCK       0x00000001 
#define MGMT_PAL_OENAB        0x00000002 
#define MGMT_PAL_MDIO         0x00000004 
#define MGMT_PAL_TIMEO        0x00000008 
#define MGMT_PAL_EXT_MDIO     MGMT_PAL_MDIO
#define MGMT_PAL_INT_MDIO     MGMT_PAL_TIMEO

#define BIGMAC_PHY_EXTERNAL   0 
#define BIGMAC_PHY_INTERNAL   1 

struct be_rxd {
	u32 rx_flags;
	u32 rx_addr;
};

#define RXD_OWN      0x80000000 
#define RXD_UPDATE   0x10000000 
#define RXD_LENGTH   0x000007ff 

struct be_txd {
	u32 tx_flags;
	u32 tx_addr;
};

#define TXD_OWN      0x80000000 
#define TXD_SOP      0x40000000 
#define TXD_EOP      0x20000000 
#define TXD_UPDATE   0x10000000 
#define TXD_LENGTH   0x000007ff 

#define TX_RING_MAXSIZE   256
#define RX_RING_MAXSIZE   256

#define TX_RING_SIZE      256
#define RX_RING_SIZE      256

#define NEXT_RX(num)       (((num) + 1) & (RX_RING_SIZE - 1))
#define NEXT_TX(num)       (((num) + 1) & (TX_RING_SIZE - 1))
#define PREV_RX(num)       (((num) - 1) & (RX_RING_SIZE - 1))
#define PREV_TX(num)       (((num) - 1) & (TX_RING_SIZE - 1))

#define TX_BUFFS_AVAIL(bp)                                    \
        (((bp)->tx_old <= (bp)->tx_new) ?                     \
	  (bp)->tx_old + (TX_RING_SIZE - 1) - (bp)->tx_new :  \
			    (bp)->tx_old - (bp)->tx_new - 1)


#define RX_COPY_THRESHOLD  256
#define RX_BUF_ALLOC_SIZE  (ETH_FRAME_LEN + (64 * 3))

struct bmac_init_block {
	struct be_rxd be_rxd[RX_RING_MAXSIZE];
	struct be_txd be_txd[TX_RING_MAXSIZE];
};

#define bib_offset(mem, elem) \
((__u32)((unsigned long)(&(((struct bmac_init_block *)0)->mem[elem]))))

enum bigmac_transceiver {
	external = 0,
	internal = 1,
	none     = 2,
};

enum bigmac_timer_state {
	ltrywait = 1,  
	asleep   = 2,  
};

struct bigmac {
	void __iomem	*gregs;	
	void __iomem	*creg;	
	void __iomem	*bregs;	
	void __iomem	*tregs;	
	struct bmac_init_block	*bmac_block;	
	__u32			 bblock_dvma;	

	spinlock_t		lock;

	struct sk_buff		*rx_skbs[RX_RING_SIZE];
	struct sk_buff		*tx_skbs[TX_RING_SIZE];

	int rx_new, tx_new, rx_old, tx_old;

	int board_rev;				

	enum bigmac_transceiver	tcvr_type;
	unsigned int		bigmac_bursts;
	unsigned int		paddr;
	unsigned short		sw_bmsr;         
	unsigned short		sw_bmcr;         
	struct timer_list	bigmac_timer;
	enum bigmac_timer_state	timer_state;
	unsigned int		timer_ticks;

	struct net_device_stats	enet_stats;
	struct platform_device	*qec_op;
	struct platform_device	*bigmac_op;
	struct net_device	*dev;
};

#define ALIGNED_RX_SKB_ADDR(addr) \
        ((((unsigned long)(addr) + (64 - 1)) & ~(64 - 1)) - (unsigned long)(addr))

static inline struct sk_buff *big_mac_alloc_skb(unsigned int length, gfp_t gfp_flags)
{
	struct sk_buff *skb;

	skb = alloc_skb(length + 64, gfp_flags);
	if(skb) {
		int offset = ALIGNED_RX_SKB_ADDR(skb->data);

		if(offset)
			skb_reserve(skb, offset);
	}
	return skb;
}

#endif 
