
#define CS0		0x08	
#define ICEN		0x04	
#define DS0		0x02	
#define DS1		0x01	

#define WDIR		0x20	
#define RDIR		0x00	
#define PS2WDIR		0x00	
#define PS2RDIR		0x20

#define IRQEN		0x10	
#define SELECTIN	0x08	
#define INITP		0x04	
#define AUTOFEED	0x02	
#define STROBE		0x01	

#define RESET		0x08
#define NIS0		0x20	
#define NCTL0		0x10

#define W_DUMMY		0x00	
#define W_CR		0x20	
#define W_NPR		0x40	
#define W_TBR		0x60	
#define W_RSA		0x80	

#define EMPTY		0x80	
#define INTLEVEL	0x40	
#define TXBF1		0x20	
#define TXBF0		0x10	
#define READY		0x08	

#define	W_RSA1		0xa0	
#define	W_RSA0		0xa1	
#define	W_NPRF		0xa2	
#define	W_DFR		0xa3	
#define	W_CPR		0xa4	
#define	W_SPR		0xa5	
#define	W_EPR		0xa6	
#define	W_SCR		0xa7	
#define	W_TCR		0xa8	
#define	W_EIP		0xa9	
#define	W_PAR0		0xaa	
#define	W_PAR1		0xab	
#define	W_PAR2		0xac	
#define	W_PAR3		0xad	
#define	W_PAR4		0xae	
#define	W_PAR5		0xaf	

#define	R_STS		0xc0	
#define	R_CPR		0xc1	
#define	R_BPR		0xc2	
#define	R_TDR		0xc3	

#define EEDI		0x80	
#define TXSUC		0x40	
#define T16		0x20	
#define TS1		0x40	
#define TS0		0x20	
#define RXGOOD		0x10	
#define RXCRC		0x08	
#define RXSHORT		0x04	
#define COLS		0x02	
#define LNKS		0x01	

#define CLEAR		0x10	
#define NOPER		0x08	
#define RNOP		0x08
#define RRA		0x06	
#define RRN		0x04	
#define RW1		0x02	
#define RW0		0x00	
#define TXEN		0x01	

#define TESTON		0x80	
#define SLEEP		0x40	
#if 0
#define FASTMODE	0x04	
#define BYTEMODE	0x02	
#else
#define FASTMODE	0x20	
#define BYTEMODE	0x10	
#endif
#define NIBBLEMODE	0x00	
#define IRQINV		0x08	
#define IRQNML		0x00	
#define INTON		0x04
#define AUTOFFSET	0x02	
#define AUTOTX		0x01	

#define JABBER		0x80	
#define TXSUCINT	0x40	
#define T16INT		0x20	
#define RXERRPKT	0x10	
#define EXTERNALB2	0x0C	
#define EXTERNALB1	0x08	
#define INTERNALB	0x04	
#define NMLOPERATE	0x00	
#define RXPBM		0x03	
#define RXPB		0x02	
#define RXALL		0x01	
#define RXOFF		0x00	
