/*
 * hd64572.h	Description of the Hitachi HD64572 (SCA-II), valid for 
 * 		CPU modes 0 & 2.
 *
 * Author:	Ivan Passos <ivan@cyclades.com>
 *
 * Copyright:   (c) 2000-2001 Cyclades Corp.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * $Log: hd64572.h,v $
 * Revision 3.1  2001/06/15 12:41:10  regina
 * upping major version number
 *
 * Revision 1.1.1.1  2001/06/13 20:24:49  daniela
 * PC300 initial CVS version (3.4.0-pre1)
 *
 * Revision 1.0 2000/01/25 ivan
 * Initial version.
 *
 */

#ifndef __HD64572_H
#define __HD64572_H

#define	ILAR	0x00

#define PABR0L	0x20	
#define PABR0H	0x21	
#define PABR1L	0x22	
#define PABR1H	0x23	
#define WCRL	0x24	
#define WCRM	0x25	
#define WCRH	0x26	

#define IVR	0x60	
#define IMVR	0x64	
#define ITCR	0x68	
#define ISR0	0x6c	
#define ISR1	0x70	
#define IER0	0x74	
#define IER1	0x78	

#define	M_REG(reg, chan)	(reg + 0x80*chan)		
#define	DRX_REG(reg, chan)	(reg + 0x40*chan)		
#define	DTX_REG(reg, chan)	(reg + 0x20*(2*chan + 1))	
#define	TRX_REG(reg, chan)	(reg + 0x20*chan)		
#define	TTX_REG(reg, chan)	(reg + 0x10*(2*chan + 1))	
#define	ST_REG(reg, chan)	(reg + 0x80*chan)		
#define IR0_DRX(val, chan)	((val)<<(8*(chan)))		
#define IR0_DTX(val, chan)	((val)<<(4*(2*chan + 1)))	
#define IR0_M(val, chan)	((val)<<(8*(chan)))		

#define MSCI0_OFFSET 0x00
#define MSCI1_OFFSET 0x80

#define MD0	0x138	
#define MD1	0x139	
#define MD2	0x13a	
#define MD3	0x13b	
#define CTL	0x130	
#define RXS	0x13c	
#define TXS	0x13d	
#define EXS	0x13e	
#define TMCT	0x144	
#define TMCR	0x145	
#define CMD	0x128	
#define ST0	0x118	
#define ST1	0x119	
#define ST2	0x11a	
#define ST3	0x11b	
#define ST4	0x11c	
#define FST	0x11d	
#define IE0	0x120	
#define IE1	0x121	
#define IE2	0x122	
#define IE4	0x124	
#define FIE	0x125	
#define SA0	0x140	
#define SA1	0x141	
#define IDL	0x142	
#define TRBL	0x100	 
#define TRBK	0x101	 
#define TRBJ	0x102	 
#define TRBH	0x103	 
#define TRC0	0x148	 
#define TRC1	0x149	 
#define RRC	0x14a	 
#define CST0	0x108	 
#define CST1	0x109	 
#define CST2	0x10a	 
#define CST3	0x10b	 
#define GPO	0x131	
#define TFS	0x14b	
#define TFN	0x143	
#define TBN	0x110	
#define RBN	0x111	
#define TNR0	0x150	
#define TNR1	0x151	
#define TCR	0x152	
#define RNR	0x154	
#define RCR	0x156	

#define TIMER0RX_OFFSET 0x00
#define TIMER0TX_OFFSET 0x10
#define TIMER1RX_OFFSET 0x20
#define TIMER1TX_OFFSET 0x30

#define TCNTL	0x200	
#define TCNTH	0x201	
#define TCONRL	0x204	
#define TCONRH	0x205	
#define TCSR	0x206	
#define TEPR	0x207	

#define PCR		0x40		
#define DRR		0x44		
#define DMER		0x07		
#define BTCR		0x08		
#define BOLR		0x0c		
#define DSR_RX(chan)	(0x48 + 2*chan)	
#define DSR_TX(chan)	(0x49 + 2*chan)	
#define DIR_RX(chan)	(0x4c + 2*chan)	
#define DIR_TX(chan)	(0x4d + 2*chan)	
#define FCT_RX(chan)	(0x50 + 2*chan)	
#define FCT_TX(chan)	(0x51 + 2*chan)	
#define DMR_RX(chan)	(0x54 + 2*chan)	
#define DMR_TX(chan)	(0x55 + 2*chan)	
#define DCR_RX(chan)	(0x58 + 2*chan)	
#define DCR_TX(chan)	(0x59 + 2*chan)	

#define DMAC0RX_OFFSET 0x00
#define DMAC0TX_OFFSET 0x20
#define DMAC1RX_OFFSET 0x40
#define DMAC1TX_OFFSET 0x60

#define DARL	0x80	
#define DARH	0x81	
#define DARB	0x82	
#define DARBH	0x83	
#define SARL	0x80	
#define SARH	0x81	
#define SARB	0x82	
#define DARBH	0x83	
#define BARL	0x80	
#define BARH	0x81	
#define BARB	0x82	
#define BARBH	0x83	
#define CDAL	0x84	
#define CDAH	0x85	
#define CDAB	0x86	
#define CDABH	0x87	
#define EDAL	0x88	
#define EDAH	0x89	
#define EDAB	0x8a	
#define EDABH	0x8b	
#define BFLL	0x90	
#define BFLH	0x91	
#define BCRL	0x8c	
#define BCRH	0x8d	

typedef struct {
	unsigned long	next;		
	unsigned long	ptbuf;		
	unsigned short	len;		
	unsigned char	status;		
	unsigned char	filler[5];	 
} pcsca_bd_t;

typedef struct {
	u32 cp;			
	u32 bp;			
	u16 len;		
	u8 stat;		
	u8 unused;		
}pkt_desc;


#define DST_EOT		0x01	
#define DST_OSB		0x02	
#define DST_CRC		0x04	
#define DST_OVR		0x08	
#define DST_UDR		0x08	
#define DST_RBIT	0x10	
#define DST_ABT		0x20	
#define DST_SHRT	0x40	
#define DST_EOM		0x80	


#define ST_TX_EOM     0x80	
#define ST_TX_UNDRRUN 0x08
#define ST_TX_OWNRSHP 0x02
#define ST_TX_EOT     0x01	

#define ST_RX_EOM     0x80	
#define ST_RX_SHORT   0x40	
#define ST_RX_ABORT   0x20	
#define ST_RX_RESBIT  0x10	
#define ST_RX_OVERRUN 0x08	
#define ST_RX_CRC     0x04	
#define ST_RX_OWNRSHP 0x02

#define ST_ERROR_MASK 0x7C

#define CMCR	0x158	
#define TECNTL	0x160	
#define TECNTM	0x161	
#define TECNTH	0x162	
#define TECCR	0x163	
#define URCNTL	0x164	
#define URCNTH	0x165	
#define URCCR	0x167	
#define RECNTL	0x168	
#define RECNTM	0x169	
#define RECNTH	0x16a	
#define RECCR	0x16b	
#define ORCNTL	0x16c	
#define ORCNTH	0x16d	
#define ORCCR	0x16f	
#define CECNTL	0x170	
#define CECNTH	0x171	
#define CECCR	0x173	
#define ABCNTL	0x174	
#define ABCNTH	0x175	
#define ABCCR	0x177	
#define SHCNTL	0x178	
#define SHCNTH	0x179	
#define SHCCR	0x17b	
#define RSCNTL	0x17c	
#define RSCNTH	0x17d	
#define RSCCR	0x17f	


#define IR0_DMIC	0x00000001
#define IR0_DMIB	0x00000002
#define IR0_DMIA	0x00000004
#define IR0_EFT		0x00000008
#define IR0_DMAREQ	0x00010000
#define IR0_TXINT	0x00020000
#define IR0_RXINTB	0x00040000
#define IR0_RXINTA	0x00080000
#define IR0_TXRDY	0x00100000
#define IR0_RXRDY	0x00200000

#define MD0_CRC16_0	0x00
#define MD0_CRC16_1	0x01
#define MD0_CRC32	0x02
#define MD0_CRC_CCITT	0x03
#define MD0_CRCC0	0x04
#define MD0_CRCC1	0x08
#define MD0_AUTO_ENA	0x10
#define MD0_ASYNC	0x00
#define MD0_BY_MSYNC	0x20
#define MD0_BY_BISYNC	0x40
#define MD0_BY_EXT	0x60
#define MD0_BIT_SYNC	0x80
#define MD0_TRANSP	0xc0

#define MD0_HDLC        0x80	

#define MD0_CRC_NONE	0x00
#define MD0_CRC_16_0	0x04
#define MD0_CRC_16	0x05
#define MD0_CRC_ITU32	0x06
#define MD0_CRC_ITU	0x07

#define MD1_NOADDR	0x00
#define MD1_SADDR1	0x40
#define MD1_SADDR2	0x80
#define MD1_DADDR	0xc0

#define MD2_NRZI_IEEE	0x40
#define MD2_MANCHESTER	0x80
#define MD2_FM_MARK	0xA0
#define MD2_FM_SPACE	0xC0
#define MD2_LOOPBACK	0x03	

#define MD2_F_DUPLEX	0x00
#define MD2_AUTO_ECHO	0x01
#define MD2_LOOP_HI_Z	0x02
#define MD2_LOOP_MIR	0x03
#define MD2_ADPLL_X8	0x00
#define MD2_ADPLL_X16	0x08
#define MD2_ADPLL_X32	0x10
#define MD2_NRZ		0x00
#define MD2_NRZI	0x20
#define MD2_NRZ_IEEE	0x40
#define MD2_MANCH	0x00
#define MD2_FM1		0x20
#define MD2_FM0		0x40
#define MD2_FM		0x80

#define CTL_RTS		0x01
#define CTL_DTR		0x02
#define CTL_SYN		0x04
#define CTL_IDLC	0x10
#define CTL_UDRNC	0x20
#define CTL_URSKP	0x40
#define CTL_URCT	0x80

#define CTL_NORTS	0x01
#define CTL_NODTR	0x02
#define CTL_IDLE	0x10

#define	RXS_BR0		0x01
#define	RXS_BR1		0x02
#define	RXS_BR2		0x04
#define	RXS_BR3		0x08
#define	RXS_ECLK	0x00
#define	RXS_ECLK_NS	0x20
#define	RXS_IBRG	0x40
#define	RXS_PLL1	0x50
#define	RXS_PLL2	0x60
#define	RXS_PLL3	0x70
#define	RXS_DRTXC	0x80

#define	TXS_BR0		0x01
#define	TXS_BR1		0x02
#define	TXS_BR2		0x04
#define	TXS_BR3		0x08
#define	TXS_ECLK	0x00
#define	TXS_IBRG	0x40
#define	TXS_RCLK	0x60
#define	TXS_DTRXC	0x80

#define	EXS_RES0	0x01
#define	EXS_RES1	0x02
#define	EXS_RES2	0x04
#define	EXS_TES0	0x10
#define	EXS_TES1	0x20
#define	EXS_TES2	0x40

#define CLK_BRG_MASK	0x0F
#define CLK_PIN_OUT	0x80
#define CLK_LINE    	0x00	
#define CLK_BRG     	0x40	
#define CLK_TX_RXCLK	0x60	

#define CMD_RX_RST	0x11
#define CMD_RX_ENA	0x12
#define CMD_RX_DIS	0x13
#define CMD_RX_CRC_INIT	0x14
#define CMD_RX_MSG_REJ	0x15
#define CMD_RX_MP_SRCH	0x16
#define CMD_RX_CRC_EXC	0x17
#define CMD_RX_CRC_FRC	0x18
#define CMD_TX_RST	0x01
#define CMD_TX_ENA	0x02
#define CMD_TX_DISA	0x03
#define CMD_TX_CRC_INIT	0x04
#define CMD_TX_CRC_EXC	0x05
#define CMD_TX_EOM	0x06
#define CMD_TX_ABORT	0x07
#define CMD_TX_MP_ON	0x08
#define CMD_TX_BUF_CLR	0x09
#define CMD_TX_DISB	0x0b
#define CMD_CH_RST	0x21
#define CMD_SRCH_MODE	0x31
#define CMD_NOP		0x00

#define CMD_RESET	0x21
#define CMD_TX_ENABLE	0x02
#define CMD_RX_ENABLE	0x12

#define ST0_RXRDY	0x01
#define ST0_TXRDY	0x02
#define ST0_RXINTB	0x20
#define ST0_RXINTA	0x40
#define ST0_TXINT	0x80

#define ST1_IDLE	0x01
#define ST1_ABORT	0x02
#define ST1_CDCD	0x04
#define ST1_CCTS	0x08
#define ST1_SYN_FLAG	0x10
#define ST1_CLMD	0x20
#define ST1_TXIDLE	0x40
#define ST1_UDRN	0x80

#define ST2_CRCE	0x04
#define ST2_ONRN	0x08
#define ST2_RBIT	0x10
#define ST2_ABORT	0x20
#define ST2_SHORT	0x40
#define ST2_EOM		0x80

#define ST3_RX_ENA	0x01
#define ST3_TX_ENA	0x02
#define ST3_DCD		0x04
#define ST3_CTS		0x08
#define ST3_SRCH_MODE	0x10
#define ST3_SLOOP	0x20
#define ST3_GPI		0x80

#define ST4_RDNR	0x01
#define ST4_RDCR	0x02
#define ST4_TDNR	0x04
#define ST4_TDCR	0x08
#define ST4_OCLM	0x20
#define ST4_CFT		0x40
#define ST4_CGPI	0x80

#define FST_CRCEF	0x04
#define FST_OVRNF	0x08
#define FST_RBIF	0x10
#define FST_ABTF	0x20
#define FST_SHRTF	0x40
#define FST_EOMF	0x80

#define IE0_RXRDY	0x01
#define IE0_TXRDY	0x02
#define IE0_RXINTB	0x20
#define IE0_RXINTA	0x40
#define IE0_TXINT	0x80
#define IE0_UDRN	0x00008000 
#define IE0_CDCD	0x00000400 

#define IE1_IDLD	0x01
#define IE1_ABTD	0x02
#define IE1_CDCD	0x04
#define IE1_CCTS	0x08
#define IE1_SYNCD	0x10
#define IE1_CLMD	0x20
#define IE1_IDL		0x40
#define IE1_UDRN	0x80

#define IE2_CRCE	0x04
#define IE2_OVRN	0x08
#define IE2_RBIT	0x10
#define IE2_ABT		0x20
#define IE2_SHRT	0x40
#define IE2_EOM		0x80

#define IE4_RDNR	0x01
#define IE4_RDCR	0x02
#define IE4_TDNR	0x04
#define IE4_TDCR	0x08
#define IE4_OCLM	0x20
#define IE4_CFT		0x40
#define IE4_CGPI	0x80

#define FIE_CRCEF	0x04
#define FIE_OVRNF	0x08
#define FIE_RBIF	0x10
#define FIE_ABTF	0x20
#define FIE_SHRTF	0x40
#define FIE_EOMF	0x80

#define DSR_DWE		0x01
#define DSR_DE		0x02
#define DSR_REF		0x04
#define DSR_UDRF	0x04
#define DSR_COA		0x08
#define DSR_COF		0x10
#define DSR_BOF		0x20
#define DSR_EOM		0x40
#define DSR_EOT		0x80

#define DIR_REF		0x04
#define DIR_UDRF	0x04
#define DIR_COA		0x08
#define DIR_COF		0x10
#define DIR_BOF		0x20
#define DIR_EOM		0x40
#define DIR_EOT		0x80

#define DIR_REFE	0x04
#define DIR_UDRFE	0x04
#define DIR_COAE	0x08
#define DIR_COFE	0x10
#define DIR_BOFE	0x20
#define DIR_EOME	0x40
#define DIR_EOTE	0x80

#define DMR_CNTE	0x02
#define DMR_NF		0x04
#define DMR_SEOME	0x08
#define DMR_TMOD	0x10

#define DMER_DME        0x80	

#define DCR_SW_ABT	0x01
#define DCR_FCT_CLR	0x02

#define DCR_ABORT	0x01
#define DCR_CLEAR_EOF	0x02

#define PCR_COTE	0x80
#define PCR_PR0		0x01
#define PCR_PR1		0x02
#define PCR_PR2		0x04
#define PCR_CCC		0x08
#define PCR_BRC		0x10
#define PCR_OSB		0x40
#define PCR_BURST	0x80

#endif 
