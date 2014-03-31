#ifndef _IP22_ZILOG_H
#define _IP22_ZILOG_H

#include <asm/byteorder.h>

struct zilog_channel {
#ifdef __BIG_ENDIAN
	volatile unsigned char unused0[3];
	volatile unsigned char control;
	volatile unsigned char unused1[3];
	volatile unsigned char data;
#else 
	volatile unsigned char control;
	volatile unsigned char unused0[3];
	volatile unsigned char data;
	volatile unsigned char unused1[3];
#endif
};

struct zilog_layout {
	struct zilog_channel channelB;
	struct zilog_channel channelA;
};

#define NUM_ZSREGS    16

#define BRG_TO_BPS(brg, freq) ((freq) / 2 / ((brg) + 2))
#define BPS_TO_BRG(bps, freq) ((((freq) + (bps)) / (2 * (bps))) - 2)


#define	FLAG	0x7e

#define	R0	0		
#define	R1	1
#define	R2	2
#define	R3	3
#define	R4	4
#define	R5	5
#define	R6	6
#define	R7	7
#define	R8	8
#define	R9	9
#define	R10	10
#define	R11	11
#define	R12	12
#define	R13	13
#define	R14	14
#define	R15	15

#define	NULLCODE	0	
#define	POINT_HIGH	0x8	
#define	RES_EXT_INT	0x10	
#define	SEND_ABORT	0x18	
#define	RES_RxINT_FC	0x20	
#define	RES_Tx_P	0x28	
#define	ERR_RES		0x30	
#define	RES_H_IUS	0x38	

#define	RES_Rx_CRC	0x40	
#define	RES_Tx_CRC	0x80	
#define	RES_EOM_L	0xC0	


#define	EXT_INT_ENAB	0x1	
#define	TxINT_ENAB	0x2	
#define	PAR_SPEC	0x4	

#define	RxINT_DISAB	0	
#define	RxINT_FCERR	0x8	
#define	INT_ALL_Rx	0x10	
#define	INT_ERR_Rx	0x18	
#define RxINT_MASK	0x18

#define	WT_RDY_RT	0x20	
#define	WT_FN_RDYFN	0x40	
#define	WT_RDY_ENAB	0x80	



#define	RxENAB  	0x1	
#define	SYNC_L_INH	0x2	
#define	ADD_SM		0x4	
#define	RxCRC_ENAB	0x8	
#define	ENT_HM		0x10	
#define	AUTO_ENAB	0x20	
#define	Rx5		0x0	
#define	Rx7		0x40	
#define	Rx6		0x80	
#define	Rx8		0xc0	
#define RxN_MASK	0xc0


#define	PAR_ENAB	0x1	
#define	PAR_EVEN	0x2	

#define	SYNC_ENAB	0	
#define	SB1		0x4	
#define	SB15		0x8	
#define	SB2		0xc	

#define	MONSYNC		0	
#define	BISYNC		0x10	
#define	SDLC		0x20	
#define	EXTSYNC		0x30	

#define	X1CLK		0x0	
#define	X16CLK		0x40	
#define	X32CLK		0x80	
#define	X64CLK		0xC0	
#define XCLK_MASK	0xC0


#define	TxCRC_ENAB	0x1	
#define	RTS		0x2	
#define	SDLC_CRC	0x4	
#define	TxENAB		0x8	
#define	SND_BRK		0x10	
#define	Tx5		0x0	
#define	Tx7		0x20	
#define	Tx6		0x40	
#define	Tx8		0x60	
#define TxN_MASK	0x60
#define	DTR		0x80	




#define	VIS	1	
#define	NV	2	
#define	DLC	4	
#define	MIE	8	
#define	STATHI	0x10	
#define	NORESET	0	
#define	CHRB	0x40	
#define	CHRA	0x80	
#define	FHWRES	0xc0	

#define	BIT6	1	
#define	LOOPMODE 2	
#define	ABUNDER	4	
#define	MARKIDLE 8	
#define	GAOP	0x10	
#define	NRZ	0	
#define	NRZI	0x20	
#define	FM1	0x40	
#define	FM0	0x60	
#define	CRCPS	0x80	

#define	TRxCXT	0	
#define	TRxCTC	1	
#define	TRxCBR	2	
#define	TRxCDP	3	
#define	TRxCOI	4	
#define	TCRTxCP	0	
#define	TCTRxCP	8	
#define	TCBR	0x10	
#define	TCDPLL	0x18	
#define	RCRTxCP	0	
#define	RCTRxCP	0x20	
#define	RCBR	0x40	
#define	RCDPLL	0x60	
#define	RTxCX	0x80	



#define	BRENAB 	1	
#define	BRSRC	2	
#define	DTRREQ	4	
#define	AUTOECHO 8	
#define	LOOPBAK	0x10	
#define	SEARCH	0x20	
#define	RMC	0x40	
#define	DISDPLL	0x60	
#define	SSBR	0x80	
#define	SSRTxC	0xa0	
#define	SFMM	0xc0	
#define	SNRZI	0xe0	

#define	ZCIE	2	
#define	DCDIE	8	
#define	SYNCIE	0x10	
#define	CTSIE	0x20	
#define	TxUIE	0x40	
#define	BRKIE	0x80	


#define	Rx_CH_AV	0x1	
#define	ZCOUNT		0x2	
#define	Tx_BUF_EMP	0x4	
#define	DCD		0x8	
#define	SYNC		0x10	
#define	CTS		0x20	
#define	TxEOM		0x40	
#define	BRK_ABRT	0x80	

#define	ALL_SNT		0x1	
#define	RES3		0x8	
#define	RES4		0x4	
#define	RES5		0xc	
#define	RES6		0x2	
#define	RES7		0xa	
#define	RES8		0x6	
#define	RES18		0xe	
#define	RES28		0x0	
#define	PAR_ERR		0x10	
#define	Rx_OVR		0x20	
#define	CRC_ERR		0x40	
#define	END_FR		0x80	

#define CHB_Tx_EMPTY	0x00
#define CHB_EXT_STAT	0x02
#define CHB_Rx_AVAIL	0x04
#define CHB_SPECIAL	0x06
#define CHA_Tx_EMPTY	0x08
#define CHA_EXT_STAT	0x0a
#define CHA_Rx_AVAIL	0x0c
#define CHA_SPECIAL	0x0e
#define STATUS_MASK	0x0e

#define	CHBEXT	0x1		
#define	CHBTxIP	0x2		
#define	CHBRxIP	0x4		
#define	CHAEXT	0x8		
#define	CHATxIP	0x10		
#define	CHARxIP	0x20		


#define	ONLOOP	2		
#define	LOOPSEND 0x10		
#define	CLK2MIS	0x40		
#define	CLK1MIS	0x80		




#define ZS_CLEARERR(channel)    do { writeb(ERR_RES, &channel->control); \
				     udelay(5); } while(0)

#define ZS_CLEARSTAT(channel)   do { writeb(RES_EXT_INT, &channel->control); \
				     udelay(5); } while(0)

#define ZS_CLEARFIFO(channel)   do { readb(&channel->data); \
				     udelay(2); \
				     readb(&channel->data); \
				     udelay(2); \
				     readb(&channel->data); \
				     udelay(2); } while(0)

#endif 
