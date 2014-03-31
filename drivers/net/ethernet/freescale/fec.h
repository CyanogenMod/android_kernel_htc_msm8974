
/*
 *	fec.h  --  Fast Ethernet Controller for Motorola ColdFire SoC
 *		   processors.
 *
 *	(C) Copyright 2000-2005, Greg Ungerer (gerg@snapgear.com)
 *	(C) Copyright 2000-2001, Lineo (www.lineo.com)
 */

#ifndef FEC_H
#define	FEC_H

#if defined(CONFIG_M523x) || defined(CONFIG_M527x) || defined(CONFIG_M528x) || \
    defined(CONFIG_M520x) || defined(CONFIG_M532x) || \
    defined(CONFIG_ARCH_MXC) || defined(CONFIG_SOC_IMX28)
#define FEC_IEVENT		0x004 
#define FEC_IMASK		0x008 
#define FEC_R_DES_ACTIVE	0x010 
#define FEC_X_DES_ACTIVE	0x014 
#define FEC_ECNTRL		0x024 
#define FEC_MII_DATA		0x040 
#define FEC_MII_SPEED		0x044 
#define FEC_MIB_CTRLSTAT	0x064 
#define FEC_R_CNTRL		0x084 
#define FEC_X_CNTRL		0x0c4 
#define FEC_ADDR_LOW		0x0e4 
#define FEC_ADDR_HIGH		0x0e8 
#define FEC_OPD			0x0ec 
#define FEC_HASH_TABLE_HIGH	0x118 
#define FEC_HASH_TABLE_LOW	0x11c 
#define FEC_GRP_HASH_TABLE_HIGH	0x120 
#define FEC_GRP_HASH_TABLE_LOW	0x124 
#define FEC_X_WMRK		0x144 
#define FEC_R_BOUND		0x14c 
#define FEC_R_FSTART		0x150 
#define FEC_R_DES_START		0x180 
#define FEC_X_DES_START		0x184 
#define FEC_R_BUFF_SIZE		0x188 
#define FEC_MIIGSK_CFGR		0x300 
#define FEC_MIIGSK_ENR		0x308 

#define BM_MIIGSK_CFGR_MII		0x00
#define BM_MIIGSK_CFGR_RMII		0x01
#define BM_MIIGSK_CFGR_FRCONT_10M	0x40

#else

#define FEC_ECNTRL		0x000 
#define FEC_IEVENT		0x004 
#define FEC_IMASK		0x008 
#define FEC_IVEC		0x00c 
#define FEC_R_DES_ACTIVE	0x010 
#define FEC_X_DES_ACTIVE	0x014 
#define FEC_MII_DATA		0x040 
#define FEC_MII_SPEED		0x044 
#define FEC_R_BOUND		0x08c 
#define FEC_R_FSTART		0x090 
#define FEC_X_WMRK		0x0a4 
#define FEC_X_FSTART		0x0ac 
#define FEC_R_CNTRL		0x104 
#define FEC_MAX_FRM_LEN		0x108 
#define FEC_X_CNTRL		0x144 
#define FEC_ADDR_LOW		0x3c0 
#define FEC_ADDR_HIGH		0x3c4 
#define FEC_GRP_HASH_TABLE_HIGH	0x3c8 
#define FEC_GRP_HASH_TABLE_LOW	0x3cc 
#define FEC_R_DES_START		0x3d0 
#define FEC_X_DES_START		0x3d4 
#define FEC_R_BUFF_SIZE		0x3d8 
#define FEC_FIFO_RAM		0x400 

#endif 


#if defined(CONFIG_ARCH_MXC) || defined(CONFIG_SOC_IMX28)
struct bufdesc {
	unsigned short cbd_datlen;	
	unsigned short cbd_sc;	
	unsigned long cbd_bufaddr;	
};
#else
struct bufdesc {
	unsigned short	cbd_sc;			
	unsigned short	cbd_datlen;		
	unsigned long	cbd_bufaddr;		
};
#endif

/*
 *	The following definitions courtesy of commproc.h, which where
 *	Copyright (c) 1997 Dan Malek (dmalek@jlc.net).
 */
#define BD_SC_EMPTY     ((ushort)0x8000)        
#define BD_SC_READY     ((ushort)0x8000)        
#define BD_SC_WRAP      ((ushort)0x2000)        
#define BD_SC_INTRPT    ((ushort)0x1000)        
#define BD_SC_CM        ((ushort)0x0200)        
#define BD_SC_ID        ((ushort)0x0100)        
#define BD_SC_P         ((ushort)0x0100)        
#define BD_SC_BR        ((ushort)0x0020)        
#define BD_SC_FR        ((ushort)0x0010)        
#define BD_SC_PR        ((ushort)0x0008)        
#define BD_SC_OV        ((ushort)0x0002)        
#define BD_SC_CD        ((ushort)0x0001)        

#define BD_ENET_RX_EMPTY        ((ushort)0x8000)
#define BD_ENET_RX_WRAP         ((ushort)0x2000)
#define BD_ENET_RX_INTR         ((ushort)0x1000)
#define BD_ENET_RX_LAST         ((ushort)0x0800)
#define BD_ENET_RX_FIRST        ((ushort)0x0400)
#define BD_ENET_RX_MISS         ((ushort)0x0100)
#define BD_ENET_RX_LG           ((ushort)0x0020)
#define BD_ENET_RX_NO           ((ushort)0x0010)
#define BD_ENET_RX_SH           ((ushort)0x0008)
#define BD_ENET_RX_CR           ((ushort)0x0004)
#define BD_ENET_RX_OV           ((ushort)0x0002)
#define BD_ENET_RX_CL           ((ushort)0x0001)
#define BD_ENET_RX_STATS        ((ushort)0x013f)        

#define BD_ENET_TX_READY        ((ushort)0x8000)
#define BD_ENET_TX_PAD          ((ushort)0x4000)
#define BD_ENET_TX_WRAP         ((ushort)0x2000)
#define BD_ENET_TX_INTR         ((ushort)0x1000)
#define BD_ENET_TX_LAST         ((ushort)0x0800)
#define BD_ENET_TX_TC           ((ushort)0x0400)
#define BD_ENET_TX_DEF          ((ushort)0x0200)
#define BD_ENET_TX_HB           ((ushort)0x0100)
#define BD_ENET_TX_LC           ((ushort)0x0080)
#define BD_ENET_TX_RL           ((ushort)0x0040)
#define BD_ENET_TX_RCMASK       ((ushort)0x003c)
#define BD_ENET_TX_UN           ((ushort)0x0002)
#define BD_ENET_TX_CSL          ((ushort)0x0001)
#define BD_ENET_TX_STATS        ((ushort)0x03ff)        


#endif 
