/* $Id: sunhme.h,v 1.33 2001/08/03 06:23:04 davem Exp $
 * sunhme.h: Definitions for Sparc HME/BigMac 10/100baseT ethernet driver.
 *           Also known as the "Happy Meal".
 *
 * Copyright (C) 1996, 1999 David S. Miller (davem@redhat.com)
 */

#ifndef _SUNHME_H
#define _SUNHME_H

#include <linux/pci.h>

#define GREG_SWRESET	0x000UL	
#define GREG_CFG	0x004UL	
#define GREG_STAT	0x108UL	
#define GREG_IMASK	0x10cUL	
#define GREG_REG_SIZE	0x110UL

#define GREG_RESET_ETX         0x01
#define GREG_RESET_ERX         0x02
#define GREG_RESET_ALL         0x03

#define GREG_CFG_BURSTMSK      0x03
#define GREG_CFG_BURST16       0x00
#define GREG_CFG_BURST32       0x01
#define GREG_CFG_BURST64       0x02
#define GREG_CFG_64BIT         0x04
#define GREG_CFG_PARITY        0x08
#define GREG_CFG_RESV          0x10

#define GREG_STAT_GOTFRAME     0x00000001 
#define GREG_STAT_RCNTEXP      0x00000002 
#define GREG_STAT_ACNTEXP      0x00000004 
#define GREG_STAT_CCNTEXP      0x00000008 
#define GREG_STAT_LCNTEXP      0x00000010 
#define GREG_STAT_RFIFOVF      0x00000020 
#define GREG_STAT_CVCNTEXP     0x00000040 
#define GREG_STAT_STSTERR      0x00000080 
#define GREG_STAT_SENTFRAME    0x00000100 
#define GREG_STAT_TFIFO_UND    0x00000200 
#define GREG_STAT_MAXPKTERR    0x00000400 
#define GREG_STAT_NCNTEXP      0x00000800 
#define GREG_STAT_ECNTEXP      0x00001000 
#define GREG_STAT_LCCNTEXP     0x00002000 
#define GREG_STAT_FCNTEXP      0x00004000 
#define GREG_STAT_DTIMEXP      0x00008000 
#define GREG_STAT_RXTOHOST     0x00010000 
#define GREG_STAT_NORXD        0x00020000 
#define GREG_STAT_RXERR        0x00040000 
#define GREG_STAT_RXLATERR     0x00080000 
#define GREG_STAT_RXPERR       0x00100000 
#define GREG_STAT_RXTERR       0x00200000 
#define GREG_STAT_EOPERR       0x00400000 
#define GREG_STAT_MIFIRQ       0x00800000 
#define GREG_STAT_HOSTTOTX     0x01000000 
#define GREG_STAT_TXALL        0x02000000 
#define GREG_STAT_TXEACK       0x04000000 
#define GREG_STAT_TXLERR       0x08000000 
#define GREG_STAT_TXPERR       0x10000000 
#define GREG_STAT_TXTERR       0x20000000 
#define GREG_STAT_SLVERR       0x40000000 
#define GREG_STAT_SLVPERR      0x80000000 

#define GREG_STAT_ERRORS       0xfc7efefc

#define GREG_IMASK_GOTFRAME    0x00000001 
#define GREG_IMASK_RCNTEXP     0x00000002 
#define GREG_IMASK_ACNTEXP     0x00000004 
#define GREG_IMASK_CCNTEXP     0x00000008 
#define GREG_IMASK_LCNTEXP     0x00000010 
#define GREG_IMASK_RFIFOVF     0x00000020 
#define GREG_IMASK_CVCNTEXP    0x00000040 
#define GREG_IMASK_STSTERR     0x00000080 
#define GREG_IMASK_SENTFRAME   0x00000100 
#define GREG_IMASK_TFIFO_UND   0x00000200 
#define GREG_IMASK_MAXPKTERR   0x00000400 
#define GREG_IMASK_NCNTEXP     0x00000800 
#define GREG_IMASK_ECNTEXP     0x00001000 
#define GREG_IMASK_LCCNTEXP    0x00002000 
#define GREG_IMASK_FCNTEXP     0x00004000 
#define GREG_IMASK_DTIMEXP     0x00008000 
#define GREG_IMASK_RXTOHOST    0x00010000 
#define GREG_IMASK_NORXD       0x00020000 
#define GREG_IMASK_RXERR       0x00040000 
#define GREG_IMASK_RXLATERR    0x00080000 
#define GREG_IMASK_RXPERR      0x00100000 
#define GREG_IMASK_RXTERR      0x00200000 
#define GREG_IMASK_EOPERR      0x00400000 
#define GREG_IMASK_MIFIRQ      0x00800000 
#define GREG_IMASK_HOSTTOTX    0x01000000 
#define GREG_IMASK_TXALL       0x02000000 
#define GREG_IMASK_TXEACK      0x04000000 
#define GREG_IMASK_TXLERR      0x08000000 
#define GREG_IMASK_TXPERR      0x10000000 
#define GREG_IMASK_TXTERR      0x20000000 
#define GREG_IMASK_SLVERR      0x40000000 
#define GREG_IMASK_SLVPERR     0x80000000 

#define ETX_PENDING	0x00UL	
#define ETX_CFG		0x04UL	
#define ETX_RING	0x08UL	
#define ETX_BBASE	0x0cUL	
#define ETX_BDISP	0x10UL	
#define ETX_FIFOWPTR	0x14UL	
#define ETX_FIFOSWPTR	0x18UL	
#define ETX_FIFORPTR	0x1cUL	
#define ETX_FIFOSRPTR	0x20UL	
#define ETX_FIFOPCNT	0x24UL	
#define ETX_SMACHINE	0x28UL	
#define ETX_RSIZE	0x2cUL	
#define ETX_BPTR	0x30UL	
#define ETX_REG_SIZE	0x34UL

#define ETX_TP_DMAWAKEUP         0x00000001 

#define ETX_CFG_DMAENABLE        0x00000001 
#define ETX_CFG_FIFOTHRESH       0x000003fe 
#define ETX_CFG_IRQDAFTER        0x00000400 
#define ETX_CFG_IRQDBEFORE       0x00000000 

#define ETX_RSIZE_SHIFT          4

#define ERX_CFG		0x00UL	
#define ERX_RING	0x04UL	
#define ERX_BPTR	0x08UL	
#define ERX_FIFOWPTR	0x0cUL	
#define ERX_FIFOSWPTR	0x10UL	
#define ERX_FIFORPTR	0x14UL	
#define ERX_FIFOSRPTR	0x18UL	
#define ERX_SMACHINE	0x1cUL	
#define ERX_REG_SIZE	0x20UL

#define ERX_CFG_DMAENABLE    0x00000001 
#define ERX_CFG_RESV1        0x00000006 
#define ERX_CFG_BYTEOFFSET   0x00000038 
#define ERX_CFG_RESV2        0x000001c0 
#define ERX_CFG_SIZE32       0x00000000 
#define ERX_CFG_SIZE64       0x00000200 
#define ERX_CFG_SIZE128      0x00000400 
#define ERX_CFG_SIZE256      0x00000600 
#define ERX_CFG_RESV3        0x0000f800 
#define ERX_CFG_CSUMSTART    0x007f0000 

#define BMAC_XIFCFG	0x0000UL	
	
#define BMAC_TXSWRESET	0x208UL	
#define BMAC_TXCFG	0x20cUL	
#define BMAC_IGAP1	0x210UL	
#define BMAC_IGAP2	0x214UL	
#define BMAC_ALIMIT	0x218UL	
#define BMAC_STIME	0x21cUL	
#define BMAC_PLEN	0x220UL	
#define BMAC_PPAT	0x224UL	
#define BMAC_TXSDELIM	0x228UL	
#define BMAC_JSIZE	0x22cUL	
#define BMAC_TXMAX	0x230UL	
#define BMAC_TXMIN	0x234UL	
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
#define BMAC_RXMAX	0x310UL	
#define BMAC_RXMIN	0x314UL	
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

#define BIGMAC_XCFG_ODENABLE  0x00000001 
#define BIGMAC_XCFG_XLBACK    0x00000002 
#define BIGMAC_XCFG_MLBACK    0x00000004 
#define BIGMAC_XCFG_MIIDISAB  0x00000008 
#define BIGMAC_XCFG_SQENABLE  0x00000010 
#define BIGMAC_XCFG_SQETWIN   0x000003e0 
#define BIGMAC_XCFG_LANCE     0x00000010 
#define BIGMAC_XCFG_LIPG0     0x000003e0 

#define BIGMAC_TXCFG_ENABLE   0x00000001 
#define BIGMAC_TXCFG_SMODE    0x00000020 
#define BIGMAC_TXCFG_CIGN     0x00000040 
#define BIGMAC_TXCFG_FCSOFF   0x00000080 
#define BIGMAC_TXCFG_DBACKOFF 0x00000100 
#define BIGMAC_TXCFG_FULLDPLX 0x00000200 
#define BIGMAC_TXCFG_DGIVEUP  0x00000400 

#define BIGMAC_RXCFG_ENABLE   0x00000001 
#define BIGMAC_RXCFG_PSTRIP   0x00000020 
#define BIGMAC_RXCFG_PMISC    0x00000040 
#define BIGMAC_RXCFG_DERR     0x00000080 
#define BIGMAC_RXCFG_DCRCS    0x00000100 
#define BIGMAC_RXCFG_REJME    0x00000200 
#define BIGMAC_RXCFG_PGRP     0x00000400 
#define BIGMAC_RXCFG_HENABLE  0x00000800 
#define BIGMAC_RXCFG_AENABLE  0x00001000 

#define TCVR_BBCLOCK	0x00UL	
#define TCVR_BBDATA	0x04UL	
#define TCVR_BBOENAB	0x08UL	
#define TCVR_FRAME	0x0cUL	
#define TCVR_CFG	0x10UL	
#define TCVR_IMASK	0x14UL	
#define TCVR_STATUS	0x18UL	
#define TCVR_SMACHINE	0x1cUL	
#define TCVR_REG_SIZE	0x20UL

#define FRAME_WRITE           0x50020000
#define FRAME_READ            0x60020000

#define TCV_CFG_PSELECT       0x00000001 
#define TCV_CFG_PENABLE       0x00000002 
#define TCV_CFG_BENABLE       0x00000004 
#define TCV_CFG_PREGADDR      0x000000f8 
#define TCV_CFG_MDIO0         0x00000100 
#define TCV_CFG_MDIO1         0x00000200 
#define TCV_CFG_PDADDR        0x00007c00 

#define TCV_PADDR_ETX         0          
#define TCV_PADDR_ITX         1          

#define TCV_STAT_BASIC        0xffff0000 
#define TCV_STAT_NORMAL       0x0000ffff 


#define DP83840_CSCONFIG        0x17        

#define CSCONFIG_RESV1          0x0001  
#define CSCONFIG_LED4           0x0002  
#define CSCONFIG_LED1           0x0004  
#define CSCONFIG_RESV2          0x0008  
#define CSCONFIG_TCVDISAB       0x0010  
#define CSCONFIG_DFBYPASS       0x0020  
#define CSCONFIG_GLFORCE        0x0040  
#define CSCONFIG_CLKTRISTATE    0x0080  
#define CSCONFIG_RESV3          0x0700  
#define CSCONFIG_ENCODE         0x0800  
#define CSCONFIG_RENABLE        0x1000  
#define CSCONFIG_TCDISABLE      0x2000  
#define CSCONFIG_RESV4          0x4000  
#define CSCONFIG_NDISABLE       0x8000  

typedef u32 __bitwise__ hme32;

struct happy_meal_rxd {
	hme32 rx_flags;
	hme32 rx_addr;
};

#define RXFLAG_OWN         0x80000000 
#define RXFLAG_OVERFLOW    0x40000000 
#define RXFLAG_SIZE        0x3fff0000 
#define RXFLAG_CSUM        0x0000ffff 

struct happy_meal_txd {
	hme32 tx_flags;
	hme32 tx_addr;
};

#define TXFLAG_OWN         0x80000000 
#define TXFLAG_SOP         0x40000000 
#define TXFLAG_EOP         0x20000000 
#define TXFLAG_CSENABLE    0x10000000 
#define TXFLAG_CSLOCATION  0x0ff00000 
#define TXFLAG_CSBUFBEGIN  0x000fc000 
#define TXFLAG_SIZE        0x00003fff 

#define TX_RING_SIZE       32         
#define RX_RING_SIZE       32         

#if (TX_RING_SIZE < 16 || TX_RING_SIZE > 256 || (TX_RING_SIZE % 16) != 0)
#error TX_RING_SIZE holds illegal value
#endif

#define TX_RING_MAXSIZE    256
#define RX_RING_MAXSIZE    256

#if (RX_RING_SIZE == 32)
#define ERX_CFG_DEFAULT(off) (ERX_CFG_DMAENABLE|((off)<<3)|ERX_CFG_SIZE32|((14/2)<<16))
#else
#if (RX_RING_SIZE == 64)
#define ERX_CFG_DEFAULT(off) (ERX_CFG_DMAENABLE|((off)<<3)|ERX_CFG_SIZE64|((14/2)<<16))
#else
#if (RX_RING_SIZE == 128)
#define ERX_CFG_DEFAULT(off) (ERX_CFG_DMAENABLE|((off)<<3)|ERX_CFG_SIZE128|((14/2)<<16))
#else
#if (RX_RING_SIZE == 256)
#define ERX_CFG_DEFAULT(off) (ERX_CFG_DMAENABLE|((off)<<3)|ERX_CFG_SIZE256|((14/2)<<16))
#else
#error RX_RING_SIZE holds illegal value
#endif
#endif
#endif
#endif

#define NEXT_RX(num)       (((num) + 1) & (RX_RING_SIZE - 1))
#define NEXT_TX(num)       (((num) + 1) & (TX_RING_SIZE - 1))
#define PREV_RX(num)       (((num) - 1) & (RX_RING_SIZE - 1))
#define PREV_TX(num)       (((num) - 1) & (TX_RING_SIZE - 1))

#define TX_BUFFS_AVAIL(hp)                                    \
        (((hp)->tx_old <= (hp)->tx_new) ?                     \
	  (hp)->tx_old + (TX_RING_SIZE - 1) - (hp)->tx_new :  \
			    (hp)->tx_old - (hp)->tx_new - 1)

#define RX_OFFSET          2
#define RX_BUF_ALLOC_SIZE  (1546 + RX_OFFSET + 64)

#define RX_COPY_THRESHOLD  256

struct hmeal_init_block {
	struct happy_meal_rxd happy_meal_rxd[RX_RING_MAXSIZE];
	struct happy_meal_txd happy_meal_txd[TX_RING_MAXSIZE];
};

#define hblock_offset(mem, elem) \
((__u32)((unsigned long)(&(((struct hmeal_init_block *)0)->mem[elem]))))

enum happy_transceiver {
	external = 0,
	internal = 1,
	none     = 2,
};

enum happy_timer_state {
	arbwait  = 0,  
	lupwait  = 1,  
	ltrywait = 2,  
	asleep   = 3,  
};

struct quattro;

struct happy_meal {
	void __iomem	*gregs;			
	struct hmeal_init_block  *happy_block;	

#if defined(CONFIG_SBUS) && defined(CONFIG_PCI)
	u32 (*read_desc32)(hme32 *);
	void (*write_txd)(struct happy_meal_txd *, u32, u32);
	void (*write_rxd)(struct happy_meal_rxd *, u32, u32);
#endif

	
	void			  *happy_dev;
	struct device		  *dma_dev;

	spinlock_t		  happy_lock;

	struct sk_buff           *rx_skbs[RX_RING_SIZE];
	struct sk_buff           *tx_skbs[TX_RING_SIZE];

	int rx_new, tx_new, rx_old, tx_old;

	struct net_device_stats	  net_stats;      

#if defined(CONFIG_SBUS) && defined(CONFIG_PCI)
	u32 (*read32)(void __iomem *);
	void (*write32)(void __iomem *, u32);
#endif

	void __iomem	*etxregs;        
	void __iomem	*erxregs;        
	void __iomem	*bigmacregs;     
	void __iomem	*tcvregs;        

	dma_addr_t                hblock_dvma;    
	unsigned int              happy_flags;    
	enum happy_transceiver    tcvr_type;      
	unsigned int              happy_bursts;   
	unsigned int              paddr;          
	unsigned short            hm_revision;    
	unsigned short            sw_bmcr;        
	unsigned short            sw_bmsr;        
	unsigned short            sw_physid1;     
	unsigned short            sw_physid2;     
	unsigned short            sw_advertise;   
	unsigned short            sw_lpa;         
	unsigned short            sw_expansion;   
	unsigned short            sw_csconfig;    
	unsigned int              auto_speed;     
        unsigned int              forced_speed;   
	unsigned int              poll_data;      
	unsigned int              poll_flag;      
	unsigned int              linkcheck;      
	unsigned int              lnkup;          
	unsigned int              lnkdown;        
	unsigned int              lnkcnt;         
	struct timer_list         happy_timer;    
	enum happy_timer_state    timer_state;    
	unsigned int              timer_ticks;    

	struct net_device	 *dev;		
	struct quattro		 *qfe_parent;	
	int			  qfe_ent;	
};

#define HFLAG_POLL                0x00000001      
#define HFLAG_FENABLE             0x00000002      
#define HFLAG_LANCE               0x00000004      
#define HFLAG_RXENABLE            0x00000008      
#define HFLAG_AUTO                0x00000010      
#define HFLAG_FULL                0x00000020      
#define HFLAG_MACFULL             0x00000040      
#define HFLAG_POLLENABLE          0x00000080      
#define HFLAG_RXCV                0x00000100      
#define HFLAG_INIT                0x00000200      
#define HFLAG_LINKUP              0x00000400      
#define HFLAG_PCI                 0x00000800      
#define HFLAG_QUATTRO		  0x00001000      

#define HFLAG_20_21  (HFLAG_POLLENABLE | HFLAG_FENABLE)
#define HFLAG_NOT_A0 (HFLAG_POLLENABLE | HFLAG_FENABLE | HFLAG_LANCE | HFLAG_RXCV)

struct quattro {
	struct net_device	*happy_meals[4];

	
	void			*quattro_dev;

	struct quattro		*next;

	
#ifdef CONFIG_SBUS
	struct linux_prom_ranges  ranges[8];
#endif
	int			  nranges;
};

#define ALIGNED_RX_SKB_ADDR(addr) \
        ((((unsigned long)(addr) + (64UL - 1UL)) & ~(64UL - 1UL)) - (unsigned long)(addr))
#define happy_meal_alloc_skb(__length, __gfp_flags) \
({	struct sk_buff *__skb; \
	__skb = alloc_skb((__length) + 64, (__gfp_flags)); \
	if(__skb) { \
		int __offset = (int) ALIGNED_RX_SKB_ADDR(__skb->data); \
		if(__offset) \
			skb_reserve(__skb, __offset); \
	} \
	__skb; \
})

#endif 
