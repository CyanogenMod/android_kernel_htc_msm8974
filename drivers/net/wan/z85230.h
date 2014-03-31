/*
 *	Description of Z8530 Z85C30 and Z85230 communications chips
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Alan Cox <alan@lxorguk.ukuu.org.uk>
 */

#ifndef _Z8530_H
#define _Z8530_H

#include <linux/tty.h>
#include <linux/interrupt.h>

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

#define RPRIME	16		

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

#define	WT_RDY_RT	0x20	
#define	WT_FN_RDYFN	0x40	
#define	WT_RDY_ENAB	0x80	



#define	RxENABLE	0x1	
#define	SYNC_L_INH	0x2	
#define	ADD_SM		0x4	
#define	RxCRC_ENAB	0x8	
#define	ENT_HM		0x10	
#define	AUTO_ENAB	0x20	
#define	Rx5		0x0	
#define	Rx7		0x40	
#define	Rx6		0x80	
#define	Rx8		0xc0	


#define	PAR_ENA		0x1	
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


#define	TxCRC_ENAB	0x1	
#define	RTS		0x2	
#define	SDLC_CRC	0x4	
#define	TxENAB		0x8	
#define	SND_BRK		0x10	
#define	Tx5		0x0	
#define	Tx7		0x20	
#define	Tx6		0x40	
#define	Tx8		0x60	
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



#define	BRENABL	1	
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

#define PRIME	1	
#define	ZCIE	2	
#define FIFOE	4	
#define	DCDIE	8	
#define	SYNCIE	0x10	
#define	CTSIE	0x20	
#define	TxUIE	0x40	
#define	BRKIE	0x80	


#define	Rx_CH_AV	0x1	
#define	ZCOUNT		0x2	
#define	Tx_BUF_EMP	0x4	
#define	DCD		0x8	
#define	SYNC_HUNT	0x10	
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






struct z8530_channel;
 
struct z8530_irqhandler
{
	void (*rx)(struct z8530_channel *);
	void (*tx)(struct z8530_channel *);
	void (*status)(struct z8530_channel *);
};


struct z8530_channel
{
	struct		z8530_irqhandler *irqs;		
	u16		count;		
	u16		max;		
	u16		mtu;		
	u8		*dptr;		
	struct sk_buff	*skb;		
	struct sk_buff	*skb2;		
	u8		status;		
	u8		dcdcheck;	
	u8		sync;		

	u8		regs[32];	
	u8		pendregs[32];	
	
	struct sk_buff 	*tx_skb;	
	struct sk_buff  *tx_next_skb;	
	u8		*tx_ptr;	
	u8		*tx_next_ptr;	
	u8		*tx_dma_buf[2];	
	u8		tx_dma_used;	
	u16		txcount;	
	
	void		(*rx_function)(struct z8530_channel *, struct sk_buff *);
	
	
	u8		rxdma;		
	u8		txdma;		
	u8		rxdma_on;	
	u8		txdma_on;
	u8		dma_num;	
	u8		dma_ready;	
	u8		dma_tx;		
	u8		*rx_buf[2];	
	
	 
	struct z8530_dev *dev;		
	unsigned long	ctrlio;		
	unsigned long	dataio;

	
#define Z8530_PORT_SLEEP	0x80000000
#define Z8530_PORT_OF(x)	((x)&0xFFFF)

	u32		rx_overrun;		
	u32		rx_crc_err;


	void		*private;	
	struct net_device	*netdevice;	


	struct tty_struct 	*tty;		
	int			line;		
	wait_queue_head_t	open_wait;	
	wait_queue_head_t	close_wait;	
	unsigned long		event;		
	int			fdcount;    	
	int			blocked_open;	
	int			x_char;		
	unsigned char 		*xmit_buf;	
	int			xmit_head;	
	int			xmit_tail;
	int			xmit_cnt;
	int			flags;	
	int			timeout;
	int			xmit_fifo_size;	

	int			close_delay;	
	unsigned short		closing_wait;


	unsigned char		clk_divisor;  
	int			zs_baud;

	int			magic;
	int			baud_base;		
	int			custom_divisor;


	unsigned char		tx_active; 
	unsigned char		tx_stopped; 

	spinlock_t		*lock;	  
};


struct z8530_dev
{
	char *name;	
	struct z8530_channel chanA;	
	struct z8530_channel chanB;	
	int type;
#define Z8530	0		
#define Z85C30	1	
#define Z85230	2	
	int irq;	
	int active;	
	spinlock_t lock;
};


 
extern u8 z8530_dead_port[];
extern u8 z8530_hdlc_kilostream_85230[];
extern u8 z8530_hdlc_kilostream[];
extern irqreturn_t z8530_interrupt(int, void *);
extern void z8530_describe(struct z8530_dev *, char *mapping, unsigned long io);
extern int z8530_init(struct z8530_dev *);
extern int z8530_shutdown(struct z8530_dev *);
extern int z8530_sync_open(struct net_device *, struct z8530_channel *);
extern int z8530_sync_close(struct net_device *, struct z8530_channel *);
extern int z8530_sync_dma_open(struct net_device *, struct z8530_channel *);
extern int z8530_sync_dma_close(struct net_device *, struct z8530_channel *);
extern int z8530_sync_txdma_open(struct net_device *, struct z8530_channel *);
extern int z8530_sync_txdma_close(struct net_device *, struct z8530_channel *);
extern int z8530_channel_load(struct z8530_channel *, u8 *);
extern netdev_tx_t z8530_queue_xmit(struct z8530_channel *c,
					  struct sk_buff *skb);
extern void z8530_null_rx(struct z8530_channel *c, struct sk_buff *skb);


 
extern struct z8530_irqhandler z8530_sync, z8530_async, z8530_nop;


#define SERIAL_MAGIC 0x5301


#define SERIAL_XMIT_SIZE 4096
#define WAKEUP_CHARS	256

#define RS_EVENT_WRITE_WAKEUP	0

#define ZILOG_INITIALIZED	0x80000000 
#define ZILOG_CALLOUT_ACTIVE	0x40000000 
#define ZILOG_NORMAL_ACTIVE	0x20000000 
#define ZILOG_BOOT_AUTOCONF	0x10000000 
#define ZILOG_CLOSING		0x08000000 
#define ZILOG_CTS_FLOW		0x04000000 
#define ZILOG_CHECK_CD		0x02000000 

#endif 
