/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This software may be redistributed and/or modified under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * File: via-velocity.h
 *
 * Purpose: Header file to define driver's private structures.
 *
 * Author: Chuang Liang-Shing, AJ Jiang
 *
 * Date: Jan 24, 2003
 */


#ifndef VELOCITY_H
#define VELOCITY_H

#define VELOCITY_TX_CSUM_SUPPORT

#define VELOCITY_NAME          "via-velocity"
#define VELOCITY_FULL_DRV_NAM  "VIA Networking Velocity Family Gigabit Ethernet Adapter Driver"
#define VELOCITY_VERSION       "1.15"

#define VELOCITY_IO_SIZE	256
#define VELOCITY_NAPI_WEIGHT	64

#define PKT_BUF_SZ          1540

#define MAX_UNITS           8
#define OPTION_DEFAULT      { [0 ... MAX_UNITS-1] = -1}

#define REV_ID_VT6110       (0)

#define BYTE_REG_BITS_ON(x,p)       do { writeb(readb((p))|(x),(p));} while (0)
#define WORD_REG_BITS_ON(x,p)       do { writew(readw((p))|(x),(p));} while (0)
#define DWORD_REG_BITS_ON(x,p)      do { writel(readl((p))|(x),(p));} while (0)

#define BYTE_REG_BITS_IS_ON(x,p)    (readb((p)) & (x))
#define WORD_REG_BITS_IS_ON(x,p)    (readw((p)) & (x))
#define DWORD_REG_BITS_IS_ON(x,p)   (readl((p)) & (x))

#define BYTE_REG_BITS_OFF(x,p)      do { writeb(readb((p)) & (~(x)),(p));} while (0)
#define WORD_REG_BITS_OFF(x,p)      do { writew(readw((p)) & (~(x)),(p));} while (0)
#define DWORD_REG_BITS_OFF(x,p)     do { writel(readl((p)) & (~(x)),(p));} while (0)

#define BYTE_REG_BITS_SET(x,m,p)    do { writeb( (readb((p)) & (~(m))) |(x),(p));} while (0)
#define WORD_REG_BITS_SET(x,m,p)    do { writew( (readw((p)) & (~(m))) |(x),(p));} while (0)
#define DWORD_REG_BITS_SET(x,m,p)   do { writel( (readl((p)) & (~(m)))|(x),(p));}  while (0)

#define VAR_USED(p)     do {(p)=(p);} while (0)



#define B_OWNED_BY_CHIP     1
#define B_OWNED_BY_HOST     0


#define RSR_DETAG	cpu_to_le16(0x0080)
#define RSR_SNTAG	cpu_to_le16(0x0040)
#define RSR_RXER	cpu_to_le16(0x0020)
#define RSR_RL		cpu_to_le16(0x0010)
#define RSR_CE		cpu_to_le16(0x0008)
#define RSR_FAE		cpu_to_le16(0x0004)
#define RSR_CRC		cpu_to_le16(0x0002)
#define RSR_VIDM	cpu_to_le16(0x0001)


#define RSR_RXOK	cpu_to_le16(0x8000) 
#define RSR_PFT		cpu_to_le16(0x4000) 
#define RSR_MAR		cpu_to_le16(0x2000) 
#define RSR_BAR		cpu_to_le16(0x1000) 
#define RSR_PHY		cpu_to_le16(0x0800) 
#define RSR_VTAG	cpu_to_le16(0x0400) 
#define RSR_STP		cpu_to_le16(0x0200) 
#define RSR_EDP		cpu_to_le16(0x0100) 


#define CSM_IPOK            0x40	
#define CSM_TUPOK           0x20	
#define CSM_FRAG            0x10	
#define CSM_IPKT            0x04	
#define CSM_TCPKT           0x02	
#define CSM_UDPKT           0x01	


#define TSR0_ABT	cpu_to_le16(0x0080) 
#define TSR0_OWT	cpu_to_le16(0x0040) 
#define TSR0_OWC	cpu_to_le16(0x0020) 
#define TSR0_COLS	cpu_to_le16(0x0010) 
#define TSR0_NCR3	cpu_to_le16(0x0008) 
#define TSR0_NCR2	cpu_to_le16(0x0004) 
#define TSR0_NCR1	cpu_to_le16(0x0002) 
#define TSR0_NCR0	cpu_to_le16(0x0001) 
#define TSR0_TERR	cpu_to_le16(0x8000) 
#define TSR0_FDX	cpu_to_le16(0x4000) 
#define TSR0_GMII	cpu_to_le16(0x2000) 
#define TSR0_LNKFL	cpu_to_le16(0x1000) 
#define TSR0_SHDN	cpu_to_le16(0x0400) 
#define TSR0_CRS	cpu_to_le16(0x0200) 
#define TSR0_CDH	cpu_to_le16(0x0100) 

#define TCR0_TIC            0x80	
#define TCR0_PIC            0x40	
#define TCR0_VETAG          0x20	
#define TCR0_IPCK           0x10	
#define TCR0_UDPCK          0x08	
#define TCR0_TCPCK          0x04	
#define TCR0_JMBO           0x02	
#define TCR0_CRC            0x01	

#define TCPLS_NORMAL        3
#define TCPLS_START         2
#define TCPLS_END           1
#define TCPLS_MED           0


#define CB_RX_BUF_SIZE     2048UL	
					

#define CB_MAX_RD_NUM       512	
#define CB_MAX_TD_NUM       256	

#define CB_INIT_RD_NUM_3119 128	
#define CB_INIT_TD_NUM_3119 64	

#define CB_INIT_RD_NUM      128	
#define CB_INIT_TD_NUM      64	

#define CB_TD_RING_NUM      4	
#define CB_MAX_SEG_PER_PKT  7	



#define CB_MAX_TX_ABORT_RETRY   3


struct rdesc0 {
	__le16 RSR;		
	__le16 len;		
};

struct rdesc1 {
	__le16 PQTAG;
	u8 CSM;
	u8 IPKT;
};

enum {
	RX_INTEN = cpu_to_le16(0x8000)
};

struct rx_desc {
	struct rdesc0 rdesc0;
	struct rdesc1 rdesc1;
	__le32 pa_low;		
	__le16 pa_high;		
	__le16 size;		
} __packed;


struct tdesc0 {
	__le16 TSR;		
	__le16 len;		
};

struct tdesc1 {
	__le16 vlan;
	u8 TCR;
	u8 cmd;			
} __packed;

enum {
	TD_QUEUE = cpu_to_le16(0x8000)
};

struct td_buf {
	__le32 pa_low;
	__le16 pa_high;
	__le16 size;		
} __packed;

struct tx_desc {
	struct tdesc0 tdesc0;
	struct tdesc1 tdesc1;
	struct td_buf td_buf[7];
};

struct velocity_rd_info {
	struct sk_buff *skb;
	dma_addr_t skb_dma;
};


struct velocity_td_info {
	struct sk_buff *skb;
	int nskb_dma;
	dma_addr_t skb_dma[7];
};

enum  velocity_owner {
	OWNED_BY_HOST = 0,
	OWNED_BY_NIC = cpu_to_le16(0x8000)
};




#define MCAM_SIZE           64
#define VCAM_SIZE           64
#define TX_QUEUE_NO         4

#define MAX_HW_MIB_COUNTER  32
#define VELOCITY_MIN_MTU    (64)
#define VELOCITY_MAX_MTU    (9000)


#define MAC_REG_PAR         0x00	
#define MAC_REG_RCR         0x06
#define MAC_REG_TCR         0x07
#define MAC_REG_CR0_SET     0x08
#define MAC_REG_CR1_SET     0x09
#define MAC_REG_CR2_SET     0x0A
#define MAC_REG_CR3_SET     0x0B
#define MAC_REG_CR0_CLR     0x0C
#define MAC_REG_CR1_CLR     0x0D
#define MAC_REG_CR2_CLR     0x0E
#define MAC_REG_CR3_CLR     0x0F
#define MAC_REG_MAR         0x10
#define MAC_REG_CAM         0x10
#define MAC_REG_DEC_BASE_HI 0x18
#define MAC_REG_DBF_BASE_HI 0x1C
#define MAC_REG_ISR_CTL     0x20
#define MAC_REG_ISR_HOTMR   0x20
#define MAC_REG_ISR_TSUPTHR 0x20
#define MAC_REG_ISR_RSUPTHR 0x20
#define MAC_REG_ISR_CTL1    0x21
#define MAC_REG_TXE_SR      0x22
#define MAC_REG_RXE_SR      0x23
#define MAC_REG_ISR         0x24
#define MAC_REG_ISR0        0x24
#define MAC_REG_ISR1        0x25
#define MAC_REG_ISR2        0x26
#define MAC_REG_ISR3        0x27
#define MAC_REG_IMR         0x28
#define MAC_REG_IMR0        0x28
#define MAC_REG_IMR1        0x29
#define MAC_REG_IMR2        0x2A
#define MAC_REG_IMR3        0x2B
#define MAC_REG_TDCSR_SET   0x30
#define MAC_REG_RDCSR_SET   0x32
#define MAC_REG_TDCSR_CLR   0x34
#define MAC_REG_RDCSR_CLR   0x36
#define MAC_REG_RDBASE_LO   0x38
#define MAC_REG_RDINDX      0x3C
#define MAC_REG_TDBASE_LO   0x40
#define MAC_REG_RDCSIZE     0x50
#define MAC_REG_TDCSIZE     0x52
#define MAC_REG_TDINDX      0x54
#define MAC_REG_TDIDX0      0x54
#define MAC_REG_TDIDX1      0x56
#define MAC_REG_TDIDX2      0x58
#define MAC_REG_TDIDX3      0x5A
#define MAC_REG_PAUSE_TIMER 0x5C
#define MAC_REG_RBRDU       0x5E
#define MAC_REG_FIFO_TEST0  0x60
#define MAC_REG_FIFO_TEST1  0x64
#define MAC_REG_CAMADDR     0x68
#define MAC_REG_CAMCR       0x69
#define MAC_REG_GFTEST      0x6A
#define MAC_REG_FTSTCMD     0x6B
#define MAC_REG_MIICFG      0x6C
#define MAC_REG_MIISR       0x6D
#define MAC_REG_PHYSR0      0x6E
#define MAC_REG_PHYSR1      0x6F
#define MAC_REG_MIICR       0x70
#define MAC_REG_MIIADR      0x71
#define MAC_REG_MIIDATA     0x72
#define MAC_REG_SOFT_TIMER0 0x74
#define MAC_REG_SOFT_TIMER1 0x76
#define MAC_REG_CFGA        0x78
#define MAC_REG_CFGB        0x79
#define MAC_REG_CFGC        0x7A
#define MAC_REG_CFGD        0x7B
#define MAC_REG_DCFG0       0x7C
#define MAC_REG_DCFG1       0x7D
#define MAC_REG_MCFG0       0x7E
#define MAC_REG_MCFG1       0x7F

#define MAC_REG_TBIST       0x80
#define MAC_REG_RBIST       0x81
#define MAC_REG_PMCC        0x82
#define MAC_REG_STICKHW     0x83
#define MAC_REG_MIBCR       0x84
#define MAC_REG_EERSV       0x85
#define MAC_REG_REVID       0x86
#define MAC_REG_MIBREAD     0x88
#define MAC_REG_BPMA        0x8C
#define MAC_REG_EEWR_DATA   0x8C
#define MAC_REG_BPMD_WR     0x8F
#define MAC_REG_BPCMD       0x90
#define MAC_REG_BPMD_RD     0x91
#define MAC_REG_EECHKSUM    0x92
#define MAC_REG_EECSR       0x93
#define MAC_REG_EERD_DATA   0x94
#define MAC_REG_EADDR       0x96
#define MAC_REG_EMBCMD      0x97
#define MAC_REG_JMPSR0      0x98
#define MAC_REG_JMPSR1      0x99
#define MAC_REG_JMPSR2      0x9A
#define MAC_REG_JMPSR3      0x9B
#define MAC_REG_CHIPGSR     0x9C
#define MAC_REG_TESTCFG     0x9D
#define MAC_REG_DEBUG       0x9E
#define MAC_REG_CHIPGCR     0x9F	
#define MAC_REG_WOLCR0_SET  0xA0
#define MAC_REG_WOLCR1_SET  0xA1
#define MAC_REG_PWCFG_SET   0xA2
#define MAC_REG_WOLCFG_SET  0xA3
#define MAC_REG_WOLCR0_CLR  0xA4
#define MAC_REG_WOLCR1_CLR  0xA5
#define MAC_REG_PWCFG_CLR   0xA6
#define MAC_REG_WOLCFG_CLR  0xA7
#define MAC_REG_WOLSR0_SET  0xA8
#define MAC_REG_WOLSR1_SET  0xA9
#define MAC_REG_WOLSR0_CLR  0xAC
#define MAC_REG_WOLSR1_CLR  0xAD
#define MAC_REG_PATRN_CRC0  0xB0
#define MAC_REG_PATRN_CRC1  0xB2
#define MAC_REG_PATRN_CRC2  0xB4
#define MAC_REG_PATRN_CRC3  0xB6
#define MAC_REG_PATRN_CRC4  0xB8
#define MAC_REG_PATRN_CRC5  0xBA
#define MAC_REG_PATRN_CRC6  0xBC
#define MAC_REG_PATRN_CRC7  0xBE
#define MAC_REG_BYTEMSK0_0  0xC0
#define MAC_REG_BYTEMSK0_1  0xC4
#define MAC_REG_BYTEMSK0_2  0xC8
#define MAC_REG_BYTEMSK0_3  0xCC
#define MAC_REG_BYTEMSK1_0  0xD0
#define MAC_REG_BYTEMSK1_1  0xD4
#define MAC_REG_BYTEMSK1_2  0xD8
#define MAC_REG_BYTEMSK1_3  0xDC
#define MAC_REG_BYTEMSK2_0  0xE0
#define MAC_REG_BYTEMSK2_1  0xE4
#define MAC_REG_BYTEMSK2_2  0xE8
#define MAC_REG_BYTEMSK2_3  0xEC
#define MAC_REG_BYTEMSK3_0  0xF0
#define MAC_REG_BYTEMSK3_1  0xF4
#define MAC_REG_BYTEMSK3_2  0xF8
#define MAC_REG_BYTEMSK3_3  0xFC


#define RCR_AS              0x80
#define RCR_AP              0x40
#define RCR_AL              0x20
#define RCR_PROM            0x10
#define RCR_AB              0x08
#define RCR_AM              0x04
#define RCR_AR              0x02
#define RCR_SEP             0x01


#define TCR_TB2BDIS         0x80
#define TCR_COLTMC1         0x08
#define TCR_COLTMC0         0x04
#define TCR_LB1             0x02	
#define TCR_LB0             0x01	


#define CR0_TXON            0x00000008UL
#define CR0_RXON            0x00000004UL
#define CR0_STOP            0x00000002UL	
#define CR0_STRT            0x00000001UL	
#define CR0_SFRST           0x00008000UL	
#define CR0_TM1EN           0x00004000UL
#define CR0_TM0EN           0x00002000UL
#define CR0_DPOLL           0x00000800UL	
#define CR0_DISAU           0x00000100UL
#define CR0_XONEN           0x00800000UL
#define CR0_FDXTFCEN        0x00400000UL	
#define CR0_FDXRFCEN        0x00200000UL	
#define CR0_HDXFCEN         0x00100000UL	
#define CR0_XHITH1          0x00080000UL	
#define CR0_XHITH0          0x00040000UL	
#define CR0_XLTH1           0x00020000UL	
#define CR0_XLTH0           0x00010000UL	
#define CR0_GSPRST          0x80000000UL
#define CR0_FORSRST         0x40000000UL
#define CR0_FPHYRST         0x20000000UL
#define CR0_DIAG            0x10000000UL
#define CR0_INTPCTL         0x04000000UL
#define CR0_GINTMSK1        0x02000000UL
#define CR0_GINTMSK0        0x01000000UL


#define CR1_SFRST           0x80	
#define CR1_TM1EN           0x40
#define CR1_TM0EN           0x20
#define CR1_DPOLL           0x08	
#define CR1_DISAU           0x01


#define CR2_XONEN           0x80
#define CR2_FDXTFCEN        0x40	
#define CR2_FDXRFCEN        0x20	
#define CR2_HDXFCEN         0x10	
#define CR2_XHITH1          0x08	
#define CR2_XHITH0          0x04	
#define CR2_XLTH1           0x02	
#define CR2_XLTH0           0x01	


#define CR3_GSPRST          0x80
#define CR3_FORSRST         0x40
#define CR3_FPHYRST         0x20
#define CR3_DIAG            0x10
#define CR3_INTPCTL         0x04
#define CR3_GINTMSK1        0x02
#define CR3_GINTMSK0        0x01

#define ISRCTL_UDPINT       0x8000
#define ISRCTL_TSUPDIS      0x4000
#define ISRCTL_RSUPDIS      0x2000
#define ISRCTL_PMSK1        0x1000
#define ISRCTL_PMSK0        0x0800
#define ISRCTL_INTPD        0x0400
#define ISRCTL_HCRLD        0x0200
#define ISRCTL_SCRLD        0x0100


#define ISRCTL1_UDPINT      0x80
#define ISRCTL1_TSUPDIS     0x40
#define ISRCTL1_RSUPDIS     0x20
#define ISRCTL1_PMSK1       0x10
#define ISRCTL1_PMSK0       0x08
#define ISRCTL1_INTPD       0x04
#define ISRCTL1_HCRLD       0x02
#define ISRCTL1_SCRLD       0x01


#define TXESR_TFDBS         0x08
#define TXESR_TDWBS         0x04
#define TXESR_TDRBS         0x02
#define TXESR_TDSTR         0x01


#define RXESR_RFDBS         0x08
#define RXESR_RDWBS         0x04
#define RXESR_RDRBS         0x02
#define RXESR_RDSTR         0x01


#define ISR_ISR3            0x80000000UL
#define ISR_ISR2            0x40000000UL
#define ISR_ISR1            0x20000000UL
#define ISR_ISR0            0x10000000UL
#define ISR_TXSTLI          0x02000000UL
#define ISR_RXSTLI          0x01000000UL
#define ISR_HFLD            0x00800000UL
#define ISR_UDPI            0x00400000UL
#define ISR_MIBFI           0x00200000UL
#define ISR_SHDNI           0x00100000UL
#define ISR_PHYI            0x00080000UL
#define ISR_PWEI            0x00040000UL
#define ISR_TMR1I           0x00020000UL
#define ISR_TMR0I           0x00010000UL
#define ISR_SRCI            0x00008000UL
#define ISR_LSTPEI          0x00004000UL
#define ISR_LSTEI           0x00002000UL
#define ISR_OVFI            0x00001000UL
#define ISR_FLONI           0x00000800UL
#define ISR_RACEI           0x00000400UL
#define ISR_TXWB1I          0x00000200UL
#define ISR_TXWB0I          0x00000100UL
#define ISR_PTX3I           0x00000080UL
#define ISR_PTX2I           0x00000040UL
#define ISR_PTX1I           0x00000020UL
#define ISR_PTX0I           0x00000010UL
#define ISR_PTXI            0x00000008UL
#define ISR_PRXI            0x00000004UL
#define ISR_PPTXI           0x00000002UL
#define ISR_PPRXI           0x00000001UL


#define IMR_TXSTLM          0x02000000UL
#define IMR_UDPIM           0x00400000UL
#define IMR_MIBFIM          0x00200000UL
#define IMR_SHDNIM          0x00100000UL
#define IMR_PHYIM           0x00080000UL
#define IMR_PWEIM           0x00040000UL
#define IMR_TMR1IM          0x00020000UL
#define IMR_TMR0IM          0x00010000UL

#define IMR_SRCIM           0x00008000UL
#define IMR_LSTPEIM         0x00004000UL
#define IMR_LSTEIM          0x00002000UL
#define IMR_OVFIM           0x00001000UL
#define IMR_FLONIM          0x00000800UL
#define IMR_RACEIM          0x00000400UL
#define IMR_TXWB1IM         0x00000200UL
#define IMR_TXWB0IM         0x00000100UL

#define IMR_PTX3IM          0x00000080UL
#define IMR_PTX2IM          0x00000040UL
#define IMR_PTX1IM          0x00000020UL
#define IMR_PTX0IM          0x00000010UL
#define IMR_PTXIM           0x00000008UL
#define IMR_PRXIM           0x00000004UL
#define IMR_PPTXIM          0x00000002UL
#define IMR_PPRXIM          0x00000001UL


#define INT_MASK_DEF        (IMR_PPTXIM|IMR_PPRXIM|IMR_PTXIM|IMR_PRXIM|\
                            IMR_PWEIM|IMR_TXWB0IM|IMR_TXWB1IM|IMR_FLONIM|\
                            IMR_OVFIM|IMR_LSTEIM|IMR_LSTPEIM|IMR_SRCIM|IMR_MIBFIM|\
                            IMR_SHDNIM|IMR_TMR1IM|IMR_TMR0IM|IMR_TXSTLM)


#define TRDCSR_DEAD         0x0008
#define TRDCSR_WAK          0x0004
#define TRDCSR_ACT          0x0002
#define TRDCSR_RUN	    0x0001


#define CAMADDR_CAMEN       0x80
#define CAMADDR_VCAMSL      0x40


#define CAMCR_PS1           0x80
#define CAMCR_PS0           0x40
#define CAMCR_AITRPKT       0x20
#define CAMCR_AITR16        0x10
#define CAMCR_CAMRD         0x08
#define CAMCR_CAMWR         0x04
#define CAMCR_PS_CAM_MASK   0x40
#define CAMCR_PS_CAM_DATA   0x80
#define CAMCR_PS_MAR        0x00


#define MIICFG_MPO1         0x80
#define MIICFG_MPO0         0x40
#define MIICFG_MFDC         0x20


#define MIISR_MIDLE         0x80


#define PHYSR0_PHYRST       0x80
#define PHYSR0_LINKGD       0x40
#define PHYSR0_FDPX         0x10
#define PHYSR0_SPDG         0x08
#define PHYSR0_SPD10        0x04
#define PHYSR0_RXFLC        0x02
#define PHYSR0_TXFLC        0x01


#define PHYSR1_PHYTBI       0x01


#define MIICR_MAUTO         0x80
#define MIICR_RCMD          0x40
#define MIICR_WCMD          0x20
#define MIICR_MDPM          0x10
#define MIICR_MOUT          0x08
#define MIICR_MDO           0x04
#define MIICR_MDI           0x02
#define MIICR_MDC           0x01


#define MIIADR_SWMPL        0x80


#define CFGA_PMHCTG         0x08
#define CFGA_GPIO1PD        0x04
#define CFGA_ABSHDN         0x02
#define CFGA_PACPI          0x01


#define CFGB_GTCKOPT        0x80
#define CFGB_MIIOPT         0x40
#define CFGB_CRSEOPT        0x20
#define CFGB_OFSET          0x10
#define CFGB_CRANDOM        0x08
#define CFGB_CAP            0x04
#define CFGB_MBA            0x02
#define CFGB_BAKOPT         0x01


#define CFGC_EELOAD         0x80
#define CFGC_BROPT          0x40
#define CFGC_DLYEN          0x20
#define CFGC_DTSEL          0x10
#define CFGC_BTSEL          0x08
#define CFGC_BPS2           0x04	
#define CFGC_BPS1           0x02	
#define CFGC_BPS0           0x01	


#define CFGD_IODIS          0x80
#define CFGD_MSLVDACEN      0x40
#define CFGD_CFGDACEN       0x20
#define CFGD_PCI64EN        0x10
#define CFGD_HTMRL4         0x08


#define DCFG_XMWI           0x8000
#define DCFG_XMRM           0x4000
#define DCFG_XMRL           0x2000
#define DCFG_PERDIS         0x1000
#define DCFG_MRWAIT         0x0400
#define DCFG_MWWAIT         0x0200
#define DCFG_LATMEN         0x0100


#define MCFG_RXARB          0x0080
#define MCFG_RFT1           0x0020
#define MCFG_RFT0           0x0010
#define MCFG_LOWTHOPT       0x0008
#define MCFG_PQEN           0x0004
#define MCFG_RTGOPT         0x0002
#define MCFG_VIDFR          0x0001


#define MCFG_TXARB          0x8000
#define MCFG_TXQBK1         0x0800
#define MCFG_TXQBK0         0x0400
#define MCFG_TXQNOBK        0x0200
#define MCFG_SNAPOPT        0x0100


#define PMCC_DSI            0x80
#define PMCC_D2_DIS         0x40
#define PMCC_D1_DIS         0x20
#define PMCC_D3C_EN         0x10
#define PMCC_D3H_EN         0x08
#define PMCC_D2_EN          0x04
#define PMCC_D1_EN          0x02
#define PMCC_D0_EN          0x01


#define STICKHW_SWPTAG      0x10
#define STICKHW_WOLSR       0x08
#define STICKHW_WOLEN       0x04
#define STICKHW_DS1         0x02	
#define STICKHW_DS0         0x01	


#define MIBCR_MIBISTOK      0x80
#define MIBCR_MIBISTGO      0x40
#define MIBCR_MIBINC        0x20
#define MIBCR_MIBHI         0x10
#define MIBCR_MIBFRZ        0x08
#define MIBCR_MIBFLSH       0x04
#define MIBCR_MPTRINI       0x02
#define MIBCR_MIBCLR        0x01


#define EERSV_BOOT_RPL      ((u8) 0x01)	 

#define EERSV_BOOT_MASK     ((u8) 0x06)
#define EERSV_BOOT_INT19    ((u8) 0x00)
#define EERSV_BOOT_INT18    ((u8) 0x02)
#define EERSV_BOOT_LOCAL    ((u8) 0x04)
#define EERSV_BOOT_BEV      ((u8) 0x06)



#define BPCMD_BPDNE         0x80
#define BPCMD_EBPWR         0x02
#define BPCMD_EBPRD         0x01


#define EECSR_EMBP          0x40	
#define EECSR_RELOAD        0x20	
#define EECSR_DPM           0x10	
#define EECSR_ECS           0x08	
#define EECSR_ECK           0x04	
#define EECSR_EDI           0x02	
#define EECSR_EDO           0x01	


#define EMBCMD_EDONE        0x80
#define EMBCMD_EWDIS        0x08
#define EMBCMD_EWEN         0x04
#define EMBCMD_EWR          0x02
#define EMBCMD_ERD          0x01


#define TESTCFG_HBDIS       0x80


#define CHIPGCR_FCGMII      0x80	
#define CHIPGCR_FCFDX       0x40	
#define CHIPGCR_FCRESV      0x20
#define CHIPGCR_FCMODE      0x10	
#define CHIPGCR_LPSOPT      0x08
#define CHIPGCR_TM1US       0x04
#define CHIPGCR_TM0US       0x02
#define CHIPGCR_PHYINTEN    0x01


#define WOLCR_MSWOLEN7      0x0080	
#define WOLCR_MSWOLEN6      0x0040
#define WOLCR_MSWOLEN5      0x0020
#define WOLCR_MSWOLEN4      0x0010
#define WOLCR_MSWOLEN3      0x0008
#define WOLCR_MSWOLEN2      0x0004
#define WOLCR_MSWOLEN1      0x0002
#define WOLCR_MSWOLEN0      0x0001
#define WOLCR_ARP_EN        0x0001


#define WOLCR_LINKOFF_EN      0x0800	
#define WOLCR_LINKON_EN       0x0400	
#define WOLCR_MAGIC_EN        0x0200	
#define WOLCR_UNICAST_EN      0x0100	



#define PWCFG_PHYPWOPT          0x80	
#define PWCFG_PCISTICK          0x40	
#define PWCFG_WOLTYPE           0x20	
#define PWCFG_LEGCY_WOL         0x10
#define PWCFG_PMCSR_PME_SR      0x08
#define PWCFG_PMCSR_PME_EN      0x04	
#define PWCFG_LEGACY_WOLSR      0x02	
#define PWCFG_LEGACY_WOLEN      0x01	


#define WOLCFG_PMEOVR           0x80	
#define WOLCFG_SAM              0x20	
#define WOLCFG_SAB              0x10	
#define WOLCFG_SMIIACC          0x08	
#define WOLCFG_SGENWH           0x02
#define WOLCFG_PHYINTEN         0x01	

#define WOLSR_LINKOFF_INT      0x0800
#define WOLSR_LINKON_INT       0x0400
#define WOLSR_MAGIC_INT        0x0200
#define WOLSR_UNICAST_INT      0x0100


#define PKT_TYPE_NONE               0x0000	
#define PKT_TYPE_DIRECTED           0x0001	
#define PKT_TYPE_MULTICAST          0x0002
#define PKT_TYPE_ALL_MULTICAST      0x0004
#define PKT_TYPE_BROADCAST          0x0008
#define PKT_TYPE_PROMISCUOUS        0x0020
#define PKT_TYPE_LONG               0x2000	
#define PKT_TYPE_RUNT               0x4000
#define PKT_TYPE_ERROR              0x8000	


#define MAC_LB_NONE         0x00
#define MAC_LB_INTERNAL     0x01
#define MAC_LB_EXTERNAL     0x02


#if defined(_SIM)
#define IMR_MASK_VALUE      0x0033FF0FUL	

#else
#define IMR_MASK_VALUE      0x0013FB0FUL	
#endif


#define REV_ID_VT3119_A0	0x00
#define REV_ID_VT3119_A1	0x01
#define REV_ID_VT3216_A0	0x10


#define W_MAX_TIMEOUT       0x0FFFU



struct mac_regs {
	volatile u8 PAR[6];		
	volatile u8 RCR;
	volatile u8 TCR;

	volatile __le32 CR0Set;		
	volatile __le32 CR0Clr;		

	volatile u8 MARCAM[8];		

	volatile __le32 DecBaseHi;	
	volatile __le16 DbfBaseHi;	
	volatile __le16 reserved_1E;

	volatile __le16 ISRCTL;		
	volatile u8 TXESR;
	volatile u8 RXESR;

	volatile __le32 ISR;		
	volatile __le32 IMR;

	volatile __le32 TDStatusPort;	

	volatile __le16 TDCSRSet;	
	volatile u8 RDCSRSet;
	volatile u8 reserved_33;
	volatile __le16 TDCSRClr;
	volatile u8 RDCSRClr;
	volatile u8 reserved_37;

	volatile __le32 RDBaseLo;	
	volatile __le16 RDIdx;		
	volatile u8 TQETMR;		
	volatile u8 RQETMR;		

	volatile __le32 TDBaseLo[4];	

	volatile __le16 RDCSize;	
	volatile __le16 TDCSize;	
	volatile __le16 TDIdx[4];	
	volatile __le16 tx_pause_timer;	
	volatile __le16 RBRDU;		

	volatile __le32 FIFOTest0;	
	volatile __le32 FIFOTest1;	

	volatile u8 CAMADDR;		
	volatile u8 CAMCR;		
	volatile u8 GFTEST;		
	volatile u8 FTSTCMD;		

	volatile u8 MIICFG;		
	volatile u8 MIISR;
	volatile u8 PHYSR0;
	volatile u8 PHYSR1;
	volatile u8 MIICR;
	volatile u8 MIIADR;
	volatile __le16 MIIDATA;

	volatile __le16 SoftTimer0;	
	volatile __le16 SoftTimer1;

	volatile u8 CFGA;		
	volatile u8 CFGB;
	volatile u8 CFGC;
	volatile u8 CFGD;

	volatile __le16 DCFG;		
	volatile __le16 MCFG;

	volatile u8 TBIST;		
	volatile u8 RBIST;
	volatile u8 PMCPORT;
	volatile u8 STICKHW;

	volatile u8 MIBCR;		
	volatile u8 reserved_85;
	volatile u8 rev_id;
	volatile u8 PORSTS;

	volatile __le32 MIBData;	

	volatile __le16 EEWrData;

	volatile u8 reserved_8E;
	volatile u8 BPMDWr;
	volatile u8 BPCMD;
	volatile u8 BPMDRd;

	volatile u8 EECHKSUM;		
	volatile u8 EECSR;

	volatile __le16 EERdData;	
	volatile u8 EADDR;
	volatile u8 EMBCMD;


	volatile u8 JMPSR0;		
	volatile u8 JMPSR1;
	volatile u8 JMPSR2;
	volatile u8 JMPSR3;
	volatile u8 CHIPGSR;		
	volatile u8 TESTCFG;
	volatile u8 DEBUG;
	volatile u8 CHIPGCR;

	volatile __le16 WOLCRSet;	
	volatile u8 PWCFGSet;
	volatile u8 WOLCFGSet;

	volatile __le16 WOLCRClr;	
	volatile u8 PWCFGCLR;
	volatile u8 WOLCFGClr;

	volatile __le16 WOLSRSet;	
	volatile __le16 reserved_AA;

	volatile __le16 WOLSRClr;	
	volatile __le16 reserved_AE;

	volatile __le16 PatternCRC[8];	
	volatile __le32 ByteMask[4][4];	
};


enum hw_mib {
	HW_MIB_ifRxAllPkts = 0,
	HW_MIB_ifRxOkPkts,
	HW_MIB_ifTxOkPkts,
	HW_MIB_ifRxErrorPkts,
	HW_MIB_ifRxRuntOkPkt,
	HW_MIB_ifRxRuntErrPkt,
	HW_MIB_ifRx64Pkts,
	HW_MIB_ifTx64Pkts,
	HW_MIB_ifRx65To127Pkts,
	HW_MIB_ifTx65To127Pkts,
	HW_MIB_ifRx128To255Pkts,
	HW_MIB_ifTx128To255Pkts,
	HW_MIB_ifRx256To511Pkts,
	HW_MIB_ifTx256To511Pkts,
	HW_MIB_ifRx512To1023Pkts,
	HW_MIB_ifTx512To1023Pkts,
	HW_MIB_ifRx1024To1518Pkts,
	HW_MIB_ifTx1024To1518Pkts,
	HW_MIB_ifTxEtherCollisions,
	HW_MIB_ifRxPktCRCE,
	HW_MIB_ifRxJumboPkts,
	HW_MIB_ifTxJumboPkts,
	HW_MIB_ifRxMacControlFrames,
	HW_MIB_ifTxMacControlFrames,
	HW_MIB_ifRxPktFAE,
	HW_MIB_ifRxLongOkPkt,
	HW_MIB_ifRxLongPktErrPkt,
	HW_MIB_ifTXSQEErrors,
	HW_MIB_ifRxNobuf,
	HW_MIB_ifRxSymbolErrors,
	HW_MIB_ifInRangeLengthErrors,
	HW_MIB_ifLateCollisions,
	HW_MIB_SIZE
};

enum chip_type {
	CHIP_TYPE_VT6110 = 1,
};

struct velocity_info_tbl {
	enum chip_type chip_id;
	const char *name;
	int txqueue;
	u32 flags;
};

#define mac_hw_mibs_init(regs) {\
	BYTE_REG_BITS_ON(MIBCR_MIBFRZ,&((regs)->MIBCR));\
	BYTE_REG_BITS_ON(MIBCR_MIBCLR,&((regs)->MIBCR));\
	do {}\
		while (BYTE_REG_BITS_IS_ON(MIBCR_MIBCLR,&((regs)->MIBCR)));\
	BYTE_REG_BITS_OFF(MIBCR_MIBFRZ,&((regs)->MIBCR));\
}

#define mac_read_isr(regs)  		readl(&((regs)->ISR))
#define mac_write_isr(regs, x)  	writel((x),&((regs)->ISR))
#define mac_clear_isr(regs) 		writel(0xffffffffL,&((regs)->ISR))

#define mac_write_int_mask(mask, regs) 	writel((mask),&((regs)->IMR));
#define mac_disable_int(regs)       	writel(CR0_GINTMSK1,&((regs)->CR0Clr))
#define mac_enable_int(regs)    	writel(CR0_GINTMSK1,&((regs)->CR0Set))

#define mac_set_dma_length(regs, n) {\
	BYTE_REG_BITS_SET((n),0x07,&((regs)->DCFG));\
}

#define mac_set_rx_thresh(regs, n) {\
	BYTE_REG_BITS_SET((n),(MCFG_RFT0|MCFG_RFT1),&((regs)->MCFG));\
}

#define mac_rx_queue_run(regs) {\
	writeb(TRDCSR_RUN, &((regs)->RDCSRSet));\
}

#define mac_rx_queue_wake(regs) {\
	writeb(TRDCSR_WAK, &((regs)->RDCSRSet));\
}

#define mac_tx_queue_run(regs, n) {\
	writew(TRDCSR_RUN<<((n)*4),&((regs)->TDCSRSet));\
}

#define mac_tx_queue_wake(regs, n) {\
	writew(TRDCSR_WAK<<(n*4),&((regs)->TDCSRSet));\
}

static inline void mac_eeprom_reload(struct mac_regs __iomem * regs) {
	int i=0;

	BYTE_REG_BITS_ON(EECSR_RELOAD,&(regs->EECSR));
	do {
		udelay(10);
		if (i++>0x1000)
			break;
	} while (BYTE_REG_BITS_IS_ON(EECSR_RELOAD,&(regs->EECSR)));
}


typedef u8 MCAM_ADDR[ETH_ALEN];

struct arp_packet {
	u8 dest_mac[ETH_ALEN];
	u8 src_mac[ETH_ALEN];
	__be16 type;
	__be16 ar_hrd;
	__be16 ar_pro;
	u8 ar_hln;
	u8 ar_pln;
	__be16 ar_op;
	u8 ar_sha[ETH_ALEN];
	u8 ar_sip[4];
	u8 ar_tha[ETH_ALEN];
	u8 ar_tip[4];
} __packed;

struct _magic_packet {
	u8 dest_mac[6];
	u8 src_mac[6];
	__be16 type;
	u8 MAC[16][6];
	u8 password[6];
} __packed;


struct velocity_context {
	u8 mac_reg[256];
	MCAM_ADDR cam_addr[MCAM_SIZE];
	u16 vcam[VCAM_SIZE];
	u32 cammask[2];
	u32 patcrc[2];
	u32 pattern[8];
};


#define MII_REG_PSCR        0x10	


#define TCSR_ECHODIS        0x2000	
#define AUXCR_MDPPS         0x0004	

#define PLED_LALBE			0x0004	

#define PSCR_ACRSTX         0x0800	

#define PHYID_CICADA_CS8201 0x000FC410UL
#define PHYID_VT3216_32BIT  0x000FC610UL
#define PHYID_VT3216_64BIT  0x000FC600UL
#define PHYID_MARVELL_1000  0x01410C50UL
#define PHYID_MARVELL_1000S 0x01410C40UL

#define PHYID_REV_ID_MASK   0x0000000FUL

#define PHYID_GET_PHY_ID(i)         ((i) & ~PHYID_REV_ID_MASK)

#define MII_REG_BITS_ON(x,i,p) do {\
    u16 w;\
    velocity_mii_read((p),(i),&(w));\
    (w)|=(x);\
    velocity_mii_write((p),(i),(w));\
} while (0)

#define MII_REG_BITS_OFF(x,i,p) do {\
    u16 w;\
    velocity_mii_read((p),(i),&(w));\
    (w)&=(~(x));\
    velocity_mii_write((p),(i),(w));\
} while (0)

#define MII_REG_BITS_IS_ON(x,i,p) ({\
    u16 w;\
    velocity_mii_read((p),(i),&(w));\
    ((int) ((w) & (x)));})

#define MII_GET_PHY_ID(p) ({\
    u32 id;\
    velocity_mii_read((p),MII_PHYSID2,(u16 *) &id);\
    velocity_mii_read((p),MII_PHYSID1,((u16 *) &id)+1);\
    (id);})



enum velocity_msg_level {
	MSG_LEVEL_ERR = 0,	
	MSG_LEVEL_NOTICE = 1,	
	MSG_LEVEL_INFO = 2,	
	MSG_LEVEL_VERBOSE = 3,	
	MSG_LEVEL_DEBUG = 4	
};

#ifdef VELOCITY_DEBUG
#define ASSERT(x) { \
	if (!(x)) { \
		printk(KERN_ERR "assertion %s failed: file %s line %d\n", #x,\
			__func__, __LINE__);\
		BUG(); \
	}\
}
#define VELOCITY_DBG(p,args...) printk(p, ##args)
#else
#define ASSERT(x)
#define VELOCITY_DBG(x)
#endif

#define VELOCITY_PRT(l, p, args...) do {if (l<=msglevel) printk( p ,##args);} while (0)

#define VELOCITY_PRT_CAMMASK(p,t) {\
	int i;\
	if ((t)==VELOCITY_MULTICAST_CAM) {\
        	for (i=0;i<(MCAM_SIZE/8);i++)\
			printk("%02X",(p)->mCAMmask[i]);\
	}\
	else {\
		for (i=0;i<(VCAM_SIZE/8);i++)\
			printk("%02X",(p)->vCAMmask[i]);\
	}\
	printk("\n");\
}



#define     VELOCITY_WOL_MAGIC             0x00000000UL
#define     VELOCITY_WOL_PHY               0x00000001UL
#define     VELOCITY_WOL_ARP               0x00000002UL
#define     VELOCITY_WOL_UCAST             0x00000004UL
#define     VELOCITY_WOL_BCAST             0x00000010UL
#define     VELOCITY_WOL_MCAST             0x00000020UL
#define     VELOCITY_WOL_MAGIC_SEC         0x00000040UL


#define     VELOCITY_FLAGS_TAGGING         0x00000001UL
#define     VELOCITY_FLAGS_RX_CSUM         0x00000004UL
#define     VELOCITY_FLAGS_IP_ALIGN        0x00000008UL
#define     VELOCITY_FLAGS_VAL_PKT_LEN     0x00000010UL

#define     VELOCITY_FLAGS_FLOW_CTRL       0x01000000UL


#define     VELOCITY_FLAGS_OPENED          0x00010000UL
#define     VELOCITY_FLAGS_VMNS_CONNECTED  0x00020000UL
#define     VELOCITY_FLAGS_VMNS_COMMITTED  0x00040000UL
#define     VELOCITY_FLAGS_WOL_ENABLED     0x00080000UL


#define     VELOCITY_LINK_FAIL             0x00000001UL
#define     VELOCITY_SPEED_10              0x00000002UL
#define     VELOCITY_SPEED_100             0x00000004UL
#define     VELOCITY_SPEED_1000            0x00000008UL
#define     VELOCITY_DUPLEX_FULL           0x00000010UL
#define     VELOCITY_AUTONEG_ENABLE        0x00000020UL
#define     VELOCITY_FORCED_BY_EEPROM      0x00000040UL


#define     VELOCITY_LINK_CHANGE           0x00000001UL

enum speed_opt {
	SPD_DPX_AUTO = 0,
	SPD_DPX_100_HALF = 1,
	SPD_DPX_100_FULL = 2,
	SPD_DPX_10_HALF = 3,
	SPD_DPX_10_FULL = 4,
	SPD_DPX_1000_FULL = 5
};

enum velocity_init_type {
	VELOCITY_INIT_COLD = 0,
	VELOCITY_INIT_RESET,
	VELOCITY_INIT_WOL
};

enum velocity_flow_cntl_type {
	FLOW_CNTL_DEFAULT = 1,
	FLOW_CNTL_TX,
	FLOW_CNTL_RX,
	FLOW_CNTL_TX_RX,
	FLOW_CNTL_DISABLE,
};

struct velocity_opt {
	int numrx;			
	int numtx;			
	enum speed_opt spd_dpx;		

	int DMA_length;			
	int rx_thresh;			
	int flow_cntl;
	int wol_opts;			
	int td_int_count;
	int int_works;
	int rx_bandwidth_hi;
	int rx_bandwidth_lo;
	int rx_bandwidth_en;
	int rxqueue_timer;
	int txqueue_timer;
	int tx_intsup;
	int rx_intsup;
	u32 flags;
};

#define AVAIL_TD(p,q)   ((p)->options.numtx-((p)->tx.used[(q)]))

#define GET_RD_BY_IDX(vptr, idx)   (vptr->rd_ring[idx])

struct velocity_info {
	struct pci_dev *pdev;
	struct net_device *dev;

	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	u8 ip_addr[4];
	enum chip_type chip_id;

	struct mac_regs __iomem * mac_regs;
	unsigned long memaddr;
	unsigned long ioaddr;

	struct tx_info {
		int numq;

		
		int used[TX_QUEUE_NO];
		int curr[TX_QUEUE_NO];
		int tail[TX_QUEUE_NO];
		struct tx_desc *rings[TX_QUEUE_NO];
		struct velocity_td_info *infos[TX_QUEUE_NO];
		dma_addr_t pool_dma[TX_QUEUE_NO];
	} tx;

	struct rx_info {
		int buf_sz;

		int dirty;
		int curr;
		u32 filled;
		struct rx_desc *ring;
		struct velocity_rd_info *info;	
		dma_addr_t pool_dma;
	} rx;

	u32 mib_counter[MAX_HW_MIB_COUNTER];
	struct velocity_opt options;

	u32 int_mask;

	u32 flags;

	u32 mii_status;
	u32 phy_id;
	int multicast_limit;

	u8 vCAMmask[(VCAM_SIZE / 8)];
	u8 mCAMmask[(MCAM_SIZE / 8)];

	spinlock_t lock;

	int wol_opts;
	u8 wol_passwd[6];

	struct velocity_context context;

	u32 ticks;

	u8 rev_id;

	struct napi_struct napi;
};


static inline int velocity_get_ip(struct velocity_info *vptr)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	int res = -ENOENT;

	rcu_read_lock();
	in_dev = __in_dev_get_rcu(vptr->dev);
	if (in_dev != NULL) {
		ifa = (struct in_ifaddr *) in_dev->ifa_list;
		if (ifa != NULL) {
			memcpy(vptr->ip_addr, &ifa->ifa_address, 4);
			res = 0;
		}
	}
	rcu_read_unlock();
	return res;
}


static inline void velocity_update_hw_mibs(struct velocity_info *vptr)
{
	u32 tmp;
	int i;
	BYTE_REG_BITS_ON(MIBCR_MIBFLSH, &(vptr->mac_regs->MIBCR));

	while (BYTE_REG_BITS_IS_ON(MIBCR_MIBFLSH, &(vptr->mac_regs->MIBCR)));

	BYTE_REG_BITS_ON(MIBCR_MPTRINI, &(vptr->mac_regs->MIBCR));
	for (i = 0; i < HW_MIB_SIZE; i++) {
		tmp = readl(&(vptr->mac_regs->MIBData)) & 0x00FFFFFFUL;
		vptr->mib_counter[i] += tmp;
	}
}


static inline void init_flow_control_register(struct velocity_info *vptr)
{
	struct mac_regs __iomem * regs = vptr->mac_regs;

	writel((CR0_XONEN | CR0_XHITH1 | CR0_XLTH1 | CR0_XLTH0), &regs->CR0Set);
	writel((CR0_FDXTFCEN | CR0_FDXRFCEN | CR0_HDXFCEN | CR0_XHITH0), &regs->CR0Clr);

	
	writew(0xFFFF, &regs->tx_pause_timer);

	
	writew(vptr->options.numrx, &regs->RBRDU);
}


#endif
