/* $Id: sunqe.h,v 1.13 2000/02/09 11:15:42 davem Exp $
 * sunqe.h: Definitions for the Sun QuadEthernet driver.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SUNQE_H
#define _SUNQE_H

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

#define GLOB_STAT_PER_QE(status, channel) (((status) >> ((channel) * 4)) & 0xf)

#define CREG_CTRL	0x00UL	
#define CREG_STAT	0x04UL	
#define CREG_RXDS	0x08UL	
#define CREG_TXDS	0x0cUL	
#define CREG_RIMASK	0x10UL	
#define CREG_TIMASK	0x14UL	
#define CREG_QMASK	0x18UL	
#define CREG_MMASK	0x1cUL	
#define CREG_RXWBUFPTR	0x20UL	
#define CREG_RXRBUFPTR	0x24UL	
#define CREG_TXWBUFPTR	0x28UL	
#define CREG_TXRBUFPTR	0x2cUL	
#define CREG_CCNT	0x30UL	
#define CREG_PIPG	0x34UL	
#define CREG_REG_SIZE	0x38UL

#define CREG_CTRL_RXOFF       0x00000004  
#define CREG_CTRL_RESET       0x00000002  
#define CREG_CTRL_TWAKEUP     0x00000001  

#define CREG_STAT_EDEFER      0x10000000  
#define CREG_STAT_CLOSS       0x08000000  
#define CREG_STAT_ERETRIES    0x04000000  
#define CREG_STAT_LCOLL       0x02000000  
#define CREG_STAT_FUFLOW      0x01000000  
#define CREG_STAT_JERROR      0x00800000  
#define CREG_STAT_BERROR      0x00400000  
#define CREG_STAT_TXIRQ       0x00200000  
#define CREG_STAT_CCOFLOW     0x00100000  
#define CREG_STAT_TXDERROR    0x00080000  
#define CREG_STAT_TXLERR      0x00040000  
#define CREG_STAT_TXPERR      0x00020000  
#define CREG_STAT_TXSERR      0x00010000  
#define CREG_STAT_RCCOFLOW    0x00001000  
#define CREG_STAT_RUOFLOW     0x00000800  
#define CREG_STAT_MCOFLOW     0x00000400  
#define CREG_STAT_RXFOFLOW    0x00000200  
#define CREG_STAT_RLCOLL      0x00000100  
#define CREG_STAT_FCOFLOW     0x00000080  
#define CREG_STAT_CECOFLOW    0x00000040  
#define CREG_STAT_RXIRQ       0x00000020  
#define CREG_STAT_RXDROP      0x00000010  
#define CREG_STAT_RXSMALL     0x00000008  
#define CREG_STAT_RXLERR      0x00000004  
#define CREG_STAT_RXPERR      0x00000002  
#define CREG_STAT_RXSERR      0x00000001  

#define CREG_STAT_ERRORS      (CREG_STAT_EDEFER|CREG_STAT_CLOSS|CREG_STAT_ERETRIES|     \
			       CREG_STAT_LCOLL|CREG_STAT_FUFLOW|CREG_STAT_JERROR|       \
			       CREG_STAT_BERROR|CREG_STAT_CCOFLOW|CREG_STAT_TXDERROR|   \
			       CREG_STAT_TXLERR|CREG_STAT_TXPERR|CREG_STAT_TXSERR|      \
			       CREG_STAT_RCCOFLOW|CREG_STAT_RUOFLOW|CREG_STAT_MCOFLOW| \
			       CREG_STAT_RXFOFLOW|CREG_STAT_RLCOLL|CREG_STAT_FCOFLOW|   \
			       CREG_STAT_CECOFLOW|CREG_STAT_RXDROP|CREG_STAT_RXSMALL|   \
			       CREG_STAT_RXLERR|CREG_STAT_RXPERR|CREG_STAT_RXSERR)

#define CREG_QMASK_COFLOW     0x00100000  
#define CREG_QMASK_TXDERROR   0x00080000  
#define CREG_QMASK_TXLERR     0x00040000  
#define CREG_QMASK_TXPERR     0x00020000  
#define CREG_QMASK_TXSERR     0x00010000  
#define CREG_QMASK_RXDROP     0x00000010  
#define CREG_QMASK_RXBERROR   0x00000008  
#define CREG_QMASK_RXLEERR    0x00000004  
#define CREG_QMASK_RXPERR     0x00000002  
#define CREG_QMASK_RXSERR     0x00000001  

#define CREG_MMASK_EDEFER     0x10000000  
#define CREG_MMASK_CLOSS      0x08000000  
#define CREG_MMASK_ERETRY     0x04000000  
#define CREG_MMASK_LCOLL      0x02000000  
#define CREG_MMASK_UFLOW      0x01000000  
#define CREG_MMASK_JABBER     0x00800000  
#define CREG_MMASK_BABBLE     0x00400000  
#define CREG_MMASK_OFLOW      0x00000800  
#define CREG_MMASK_RXCOLL     0x00000400  
#define CREG_MMASK_RPKT       0x00000200  
#define CREG_MMASK_MPKT       0x00000100  

#define CREG_PIPG_TENAB       0x00000020  
#define CREG_PIPG_MMODE       0x00000010  
#define CREG_PIPG_WMASK       0x0000000f  

#define MREGS_RXFIFO	0x00UL	
#define MREGS_TXFIFO	0x01UL	
#define MREGS_TXFCNTL	0x02UL	
#define MREGS_TXFSTAT	0x03UL	
#define MREGS_TXRCNT	0x04UL	
#define MREGS_RXFCNTL	0x05UL	
#define MREGS_RXFSTAT	0x06UL	
#define MREGS_FFCNT	0x07UL	
#define MREGS_IREG	0x08UL	
#define MREGS_IMASK	0x09UL	
#define MREGS_POLL	0x0aUL	
#define MREGS_BCONFIG	0x0bUL	
#define MREGS_FCONFIG	0x0cUL	
#define MREGS_MCONFIG	0x0dUL	
#define MREGS_PLSCONFIG	0x0eUL	
#define MREGS_PHYCONFIG	0x0fUL	
#define MREGS_CHIPID1	0x10UL	
#define MREGS_CHIPID2	0x11UL	
#define MREGS_IACONFIG	0x12UL	
	
#define MREGS_FILTER	0x14UL	
#define MREGS_ETHADDR	0x15UL	
	
	
#define MREGS_MPCNT	0x18UL	
	
#define MREGS_RPCNT	0x1aUL	
#define MREGS_RCCNT	0x1bUL	
	
#define MREGS_UTEST	0x1dUL	
#define MREGS_RTEST1	0x1eUL	
#define MREGS_RTEST2	0x1fUL	
#define MREGS_REG_SIZE	0x20UL

#define MREGS_TXFCNTL_DRETRY        0x80 
#define MREGS_TXFCNTL_DFCS          0x08 
#define MREGS_TXFCNTL_AUTOPAD       0x01 

#define MREGS_TXFSTAT_VALID         0x80 
#define MREGS_TXFSTAT_UNDERFLOW     0x40 
#define MREGS_TXFSTAT_LCOLL         0x20 
#define MREGS_TXFSTAT_MRETRY        0x10 
#define MREGS_TXFSTAT_ORETRY        0x08 
#define MREGS_TXFSTAT_PDEFER        0x04 
#define MREGS_TXFSTAT_CLOSS         0x02 
#define MREGS_TXFSTAT_RERROR        0x01 

#define MREGS_TXRCNT_EDEFER         0x80 
#define MREGS_TXRCNT_CMASK          0x0f 

#define MREGS_RXFCNTL_LOWLAT        0x08 
#define MREGS_RXFCNTL_AREJECT       0x04 
#define MREGS_RXFCNTL_AUTOSTRIP     0x01 

#define MREGS_RXFSTAT_OVERFLOW      0x80 
#define MREGS_RXFSTAT_LCOLL         0x40 
#define MREGS_RXFSTAT_FERROR        0x20 
#define MREGS_RXFSTAT_FCSERROR      0x10 
#define MREGS_RXFSTAT_RBCNT         0x0f 

#define MREGS_FFCNT_RX              0xf0 
#define MREGS_FFCNT_TX              0x0f 

#define MREGS_IREG_JABBER           0x80 
#define MREGS_IREG_BABBLE           0x40 
#define MREGS_IREG_COLL             0x20 
#define MREGS_IREG_RCCO             0x10 
#define MREGS_IREG_RPKTCO           0x08 
#define MREGS_IREG_MPKTCO           0x04 
#define MREGS_IREG_RXIRQ            0x02 
#define MREGS_IREG_TXIRQ            0x01 

#define MREGS_IMASK_BABBLE          0x40 
#define MREGS_IMASK_COLL            0x20 
#define MREGS_IMASK_MPKTCO          0x04 
#define MREGS_IMASK_RXIRQ           0x02 
#define MREGS_IMASK_TXIRQ           0x01 

#define MREGS_POLL_TXVALID          0x80 
#define MREGS_POLL_TDTR             0x40 
#define MREGS_POLL_RDTR             0x20 

#define MREGS_BCONFIG_BSWAP         0x40 
#define MREGS_BCONFIG_4TS           0x00 
#define MREGS_BCONFIG_16TS          0x10 
#define MREGS_BCONFIG_64TS          0x20 
#define MREGS_BCONFIG_112TS         0x30 
#define MREGS_BCONFIG_RESET         0x01 

#define MREGS_FCONFIG_TXF8          0x00 
#define MREGS_FCONFIG_TXF32         0x80 
#define MREGS_FCONFIG_TXF16         0x40 
#define MREGS_FCONFIG_RXF64         0x20 
#define MREGS_FCONFIG_RXF32         0x10 
#define MREGS_FCONFIG_RXF16         0x00 
#define MREGS_FCONFIG_TFWU          0x08 
#define MREGS_FCONFIG_RFWU          0x04 
#define MREGS_FCONFIG_TBENAB        0x02 
#define MREGS_FCONFIG_RBENAB        0x01 

#define MREGS_MCONFIG_PROMISC       0x80 
#define MREGS_MCONFIG_TPDDISAB      0x40 
#define MREGS_MCONFIG_MBAENAB       0x20 
#define MREGS_MCONFIG_RPADISAB      0x08 
#define MREGS_MCONFIG_RBDISAB       0x04 
#define MREGS_MCONFIG_TXENAB        0x02 
#define MREGS_MCONFIG_RXENAB        0x01 

#define MREGS_PLSCONFIG_TXMS        0x08 
#define MREGS_PLSCONFIG_GPSI        0x06 
#define MREGS_PLSCONFIG_DAI         0x04 
#define MREGS_PLSCONFIG_TP          0x02 
#define MREGS_PLSCONFIG_AUI         0x00 
#define MREGS_PLSCONFIG_IOENAB      0x01 

#define MREGS_PHYCONFIG_LSTAT       0x80 
#define MREGS_PHYCONFIG_LTESTDIS    0x40 
#define MREGS_PHYCONFIG_RXPOLARITY  0x20 
#define MREGS_PHYCONFIG_APCDISAB    0x10 
#define MREGS_PHYCONFIG_LTENAB      0x08 
#define MREGS_PHYCONFIG_AUTO        0x04 
#define MREGS_PHYCONFIG_RWU         0x02 
#define MREGS_PHYCONFIG_AW          0x01 

#define MREGS_IACONFIG_ACHNGE       0x80 
#define MREGS_IACONFIG_PARESET      0x04 
#define MREGS_IACONFIG_LARESET      0x02 

#define MREGS_UTEST_RTRENAB         0x80 
#define MREGS_UTEST_RTRDISAB        0x40 
#define MREGS_UTEST_RPACCEPT        0x20 
#define MREGS_UTEST_FCOLL           0x10 
#define MREGS_UTEST_FCSENAB         0x08 
#define MREGS_UTEST_INTLOOPM        0x06 
#define MREGS_UTEST_INTLOOP         0x04 
#define MREGS_UTEST_EXTLOOP         0x02 
#define MREGS_UTEST_NOLOOP          0x00 

struct qe_rxd {
	u32 rx_flags;
	u32 rx_addr;
};

#define RXD_OWN      0x80000000 
#define RXD_UPDATE   0x10000000 
#define RXD_LENGTH   0x000007ff 

struct qe_txd {
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

#define TX_RING_SIZE      16
#define RX_RING_SIZE      16

#define NEXT_RX(num)       (((num) + 1) & (RX_RING_MAXSIZE - 1))
#define NEXT_TX(num)       (((num) + 1) & (TX_RING_MAXSIZE - 1))
#define PREV_RX(num)       (((num) - 1) & (RX_RING_MAXSIZE - 1))
#define PREV_TX(num)       (((num) - 1) & (TX_RING_MAXSIZE - 1))

#define TX_BUFFS_AVAIL(qp)                                    \
        (((qp)->tx_old <= (qp)->tx_new) ?                     \
	  (qp)->tx_old + (TX_RING_SIZE - 1) - (qp)->tx_new :  \
			    (qp)->tx_old - (qp)->tx_new - 1)

struct qe_init_block {
	struct qe_rxd qe_rxd[RX_RING_MAXSIZE];
	struct qe_txd qe_txd[TX_RING_MAXSIZE];
};

#define qib_offset(mem, elem) \
((__u32)((unsigned long)(&(((struct qe_init_block *)0)->mem[elem]))))

struct sunqe;

struct sunqec {
	void __iomem		*gregs;		
	struct sunqe		*qes[4];	
	unsigned int            qec_bursts;	
	struct platform_device	*op;		
	struct sunqec		*next_module;	
};

#define PKT_BUF_SZ	1664
#define RXD_PKT_SZ	1664

struct sunqe_buffers {
	u8	tx_buf[TX_RING_SIZE][PKT_BUF_SZ];
	u8	__pad[2];
	u8	rx_buf[RX_RING_SIZE][PKT_BUF_SZ];
};

#define qebuf_offset(mem, elem) \
((__u32)((unsigned long)(&(((struct sunqe_buffers *)0)->mem[elem][0]))))

struct sunqe {
	void __iomem			*qcregs;		
	void __iomem			*mregs;		
	struct qe_init_block      	*qe_block;	
	__u32                      	qblock_dvma;	
	spinlock_t			lock;		
	int                        	rx_new, rx_old;	
	int			   	tx_new, tx_old;	
	struct sunqe_buffers		*buffers;	
	__u32				buffers_dvma;	
	struct sunqec			*parent;
	u8				mconfig;	
	struct platform_device		*op;		
	struct net_device		*dev;		
	int				channel;	
};

#endif 
