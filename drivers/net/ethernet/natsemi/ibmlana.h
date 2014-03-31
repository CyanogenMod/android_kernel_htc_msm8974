#ifndef _IBM_LANA_INCLUDE_
#define _IBM_LANA_INCLUDE_

#ifdef _IBM_LANA_DRIVER_


#define PKTSIZE 1524


#define TXBUFCNT 4

#define IBM_LANA_ID 0xffe0


typedef enum {
	Media_10BaseT, Media_10Base5,
	Media_Unknown, Media_10Base2, Media_Count
} ibmlana_medium;


typedef struct {
	unsigned int slot;		
	int realirq;			
	ibmlana_medium medium;		
	u32 	tdastart, txbufstart,	
		rrastart, rxbufstart, rdastart, rxbufcnt, txusedcnt;
	int 	nextrxdescr,		
		lastrxdescr,		
		nexttxdescr,		
		currtxdescr,		
		txused[TXBUFCNT];	
	void __iomem *base;
	spinlock_t lock;
} ibmlana_priv;


#define IBM_LANA_IORANGE 0xa0


#define SONIC_CMDREG     0x00
#define CMDREG_HTX       0x0001	
#define CMDREG_TXP       0x0002	
#define CMDREG_RXDIS     0x0004	
#define CMDREG_RXEN      0x0008	
#define CMDREG_STP       0x0010	
#define CMDREG_ST        0x0020	
#define CMDREG_RST       0x0080	
#define CMDREG_RRRA      0x0100	
#define CMDREG_LCAM      0x0200	


#define SONIC_DCREG      0x02
#define DCREG_EXBUS      0x8000	
#define DCREG_LBR        0x2000	
#define DCREG_PO1        0x1000	
#define DCREG_PO0        0x0800
#define DCREG_SBUS       0x0400	
#define DCREG_USR1       0x0200	
#define DCREG_USR0       0x0100
#define DCREG_WC0        0x0000	
#define DCREG_WC1        0x0040
#define DCREG_WC2        0x0080
#define DCREG_WC3        0x00c0
#define DCREG_DW16       0x0000	
#define DCREG_DW32       0x0020	
#define DCREG_BMS        0x0010	
#define DCREG_RFT4       0x0000	
#define DCREG_RFT8       0x0004
#define DCREG_RFT16      0x0008
#define DCREG_RFT24      0x000c
#define DCREG_TFT8       0x0000	
#define DCREG_TFT16      0x0001
#define DCREG_TFT24      0x0002
#define DCREG_TFT28      0x0003


#define SONIC_RCREG      0x04
#define RCREG_ERR        0x8000	
#define RCREG_RNT        0x4000	
#define RCREG_BRD        0x2000	
#define RCREG_PRO        0x1000	
#define RCREG_AMC        0x0800	
#define RCREG_LB_NONE    0x0000	
#define RCREG_LB_MAC     0x0200	
#define RCREG_LB_ENDEC   0x0400	
#define RCREG_LB_XVR     0x0600	
#define RCREG_MC         0x0100	
#define RCREG_BC         0x0080	
#define RCREG_LPKT       0x0040	
#define RCREG_CRS        0x0020	
#define RCREG_COL        0x0010	
#define RCREG_CRCR       0x0008	
#define RCREG_FAER       0x0004	
#define RCREG_LBK        0x0002	
#define RCREG_PRX        0x0001	


#define SONIC_TCREG      0x06
#define TCREG_PINT       0x8000	
#define TCREG_POWC       0x4000	
#define TCREG_CRCI       0x2000	
#define TCREG_EXDIS      0x1000	
#define TCREG_EXD        0x0400	
#define TCREG_DEF        0x0200	
#define TCREG_NCRS       0x0100	
#define TCREG_CRSL       0x0080	
#define TCREG_EXC        0x0040	
#define TCREG_OWC        0x0020	
#define TCREG_PMB        0x0008	
#define TCREG_FU         0x0004	
#define TCREG_BCM        0x0002	
#define TCREG_PTX        0x0001	


#define SONIC_IMREG      0x08
#define IMREG_BREN       0x4000	
#define IMREG_HBLEN      0x2000	
#define IMREG_LCDEN      0x1000	
#define IMREG_PINTEN     0x0800	
#define IMREG_PRXEN      0x0400	
#define IMREG_PTXEN      0x0200	
#define IMREG_TXEREN     0x0100	
#define IMREG_TCEN       0x0080	
#define IMREG_RDEEN      0x0040	
#define IMREG_RBEEN      0x0020	
#define IMREG_RBAEEN     0x0010	
#define IMREG_CRCEN      0x0008	
#define IMREG_FAEEN      0x0004	
#define IMREG_MPEN       0x0002	
#define IMREG_RFOEN      0x0001	


#define SONIC_ISREG      0x0a
#define ISREG_BR         0x4000	
#define ISREG_HBL        0x2000	
#define ISREG_LCD        0x1000	
#define ISREG_PINT       0x0800	
#define ISREG_PKTRX      0x0400	
#define ISREG_TXDN       0x0200	
#define ISREG_TXER       0x0100	
#define ISREG_TC         0x0080	
#define ISREG_RDE        0x0040	
#define ISREG_RBE        0x0020	
#define ISREG_RBAE       0x0010	
#define ISREG_CRC        0x0008	
#define ISREG_FAE        0x0004	
#define ISREG_MP         0x0002	
#define ISREG_RFO        0x0001	

#define SONIC_UTDA       0x0c	
#define SONIC_CTDA       0x0e

#define SONIC_URDA       0x1a	
#define SONIC_CRDA       0x1c

#define SONIC_CRBA0      0x1e	
#define SONIC_CRBA1      0x20

#define SONIC_RBWC0      0x22	
#define SONIC_RBWC1      0x24

#define SONIC_EOBC       0x26	

#define SONIC_URRA       0x28	

#define SONIC_RSA        0x2a	

#define SONIC_REA        0x2c	

#define SONIC_RRP        0x2e	

#define SONIC_RWP        0x30	

#define SONIC_CAMEPTR    0x42	

#define SONIC_CAMADDR2   0x44	
#define SONIC_CAMADDR1   0x46
#define SONIC_CAMADDR0   0x48

#define SONIC_CAMPTR     0x4c	

#define SONIC_CAMCNT     0x4e	


#define SONIC_DCREG2     0x7e
#define DCREG2_EXPO3     0x8000	
#define DCREG2_EXPO2     0x4000
#define DCREG2_EXPO1     0x2000
#define DCREG2_EXPO0     0x1000
#define DCREG2_HD        0x0800	
#define DCREG2_JD        0x0200	
#define DCREG2_AUTO      0x0100	
#define DCREG2_XWRAP     0x0040	
#define DCREG2_PH        0x0010	
#define DCREG2_PCM       0x0004	
#define DCREG2_PCNM      0x0002	
#define DCREG2_RJCM      0x0001	


#define BCMREG           0x80
#define BCMREG_RAMEN     0x80	
#define BCMREG_IPEND     0x40	
#define BCMREG_RESET     0x08	
#define BCMREG_16BIT     0x04	
#define BCMREG_RAMWIN    0x02	
#define BCMREG_IEN       0x01	


#define MACADDRPROM      0x92


typedef struct {
	u32 index;		
	u32 addr0;		
	u32 addr1;
	u32 addr2;
} camentry_t;


typedef struct {
	u32 startlo;		
	u32 starthi;
	u32 cntlo;		
	u32 cnthi;
} rra_t;


typedef struct {
	u32 status;		
	u32 length;		
	u32 startlo;		
	u32 starthi;
	u32 seqno;		
	u32 link;		
	
	u32 inuse;		
} rda_t;


typedef struct {
	u32 status;		
	u32 config;		
	u32 length;		
	u32 fragcount;		
	u32 startlo;		
	u32 starthi;
	u32 fraglength;		
	
	
	u32 link;		
	
} tda_t;

#endif				

#endif	
