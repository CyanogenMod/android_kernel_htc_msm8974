#ifndef __PMAC_ZILOG_H__
#define __PMAC_ZILOG_H__

#define MAX_ZS_PORTS	4

#define NUM_ZSREGS    17

struct uart_pmac_port {
	struct uart_port		port;
	struct uart_pmac_port		*mate;

#ifdef CONFIG_PPC_PMAC
	struct macio_dev		*dev;
	struct device_node		*node;
#else
	struct platform_device		*pdev;
#endif

	
	int				port_type;
	u8				curregs[NUM_ZSREGS];

	unsigned int			flags;
#define PMACZILOG_FLAG_IS_CONS		0x00000001
#define PMACZILOG_FLAG_IS_KGDB		0x00000002
#define PMACZILOG_FLAG_MODEM_STATUS	0x00000004
#define PMACZILOG_FLAG_IS_CHANNEL_A	0x00000008
#define PMACZILOG_FLAG_REGS_HELD	0x00000010
#define PMACZILOG_FLAG_TX_STOPPED	0x00000020
#define PMACZILOG_FLAG_TX_ACTIVE	0x00000040
#define PMACZILOG_FLAG_IS_IRDA		0x00000100
#define PMACZILOG_FLAG_IS_INTMODEM	0x00000200
#define PMACZILOG_FLAG_HAS_DMA		0x00000400
#define PMACZILOG_FLAG_RSRC_REQUESTED	0x00000800
#define PMACZILOG_FLAG_IS_OPEN		0x00002000
#define PMACZILOG_FLAG_IS_EXTCLK	0x00008000
#define PMACZILOG_FLAG_BREAK		0x00010000

	unsigned char			parity_mask;
	unsigned char			prev_status;

	volatile u8			__iomem *control_reg;
	volatile u8			__iomem *data_reg;

#ifdef CONFIG_PPC_PMAC
	unsigned int			tx_dma_irq;
	unsigned int			rx_dma_irq;
	volatile struct dbdma_regs	__iomem *tx_dma_regs;
	volatile struct dbdma_regs	__iomem *rx_dma_regs;
#endif

	unsigned char			irq_name[8];

	struct ktermios			termios_cache;
};

#define to_pmz(p) ((struct uart_pmac_port *)(p))

static inline struct uart_pmac_port *pmz_get_port_A(struct uart_pmac_port *uap)
{
	if (uap->flags & PMACZILOG_FLAG_IS_CHANNEL_A)
		return uap;
	return uap->mate;
}

static inline u8 read_zsreg(struct uart_pmac_port *port, u8 reg)
{
	if (reg != 0)
		writeb(reg, port->control_reg);
	return readb(port->control_reg);
}

static inline void write_zsreg(struct uart_pmac_port *port, u8 reg, u8 value)
{
	if (reg != 0)
		writeb(reg, port->control_reg);
	writeb(value, port->control_reg);
}

static inline u8 read_zsdata(struct uart_pmac_port *port)
{
	return readb(port->data_reg);
}

static inline void write_zsdata(struct uart_pmac_port *port, u8 data)
{
	writeb(data, port->data_reg);
}

static inline void zssync(struct uart_pmac_port *port)
{
	(void)readb(port->control_reg);
}

#define BRG_TO_BPS(brg, freq) ((freq) / 2 / ((brg) + 2))
#define BPS_TO_BRG(bps, freq) ((((freq) + (bps)) / (2 * (bps))) - 2)

#define ZS_CLOCK         3686400	


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
#define	R7P	16

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
#define RxN_MASK	0xc0


#define	PAR_ENAB	0x1	
#define	PAR_EVEN	0x2	

#define	SYNC_ENAB	0	
#define	SB1		0x4	
#define	SB15		0x8	
#define	SB2		0xc	
#define SB_MASK		0xc

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
#define	TxENABLE	0x8	
#define	SND_BRK		0x10	
#define	Tx5		0x0	
#define	Tx7		0x20	
#define	Tx6		0x40	
#define	Tx8		0x60	
#define TxN_MASK	0x60
#define	DTR		0x80	



#define	ENEXREAD	0x40	


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



#define	BRENAB	1	
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

#define	EN85C30	1	
#define	ZCIE	2	
#define	ENSTFIFO 4	
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

#define	CHB_Tx_EMPTY	0x00
#define	CHB_EXT_STAT	0x02
#define	CHB_Rx_AVAIL	0x04
#define	CHB_SPECIAL	0x06
#define	CHA_Tx_EMPTY	0x08
#define	CHA_EXT_STAT	0x0a
#define	CHA_Rx_AVAIL	0x0c
#define	CHA_SPECIAL	0x0e
#define	STATUS_MASK	0x06

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




#define ZS_CLEARERR(port)    (write_zsreg(port, 0, ERR_RES))
#define ZS_CLEARFIFO(port)   do { volatile unsigned char garbage; \
				     garbage = read_zsdata(port); \
				     garbage = read_zsdata(port); \
				     garbage = read_zsdata(port); \
				} while(0)

#define ZS_IS_CONS(UP)			((UP)->flags & PMACZILOG_FLAG_IS_CONS)
#define ZS_IS_KGDB(UP)			((UP)->flags & PMACZILOG_FLAG_IS_KGDB)
#define ZS_IS_CHANNEL_A(UP)		((UP)->flags & PMACZILOG_FLAG_IS_CHANNEL_A)
#define ZS_REGS_HELD(UP)		((UP)->flags & PMACZILOG_FLAG_REGS_HELD)
#define ZS_TX_STOPPED(UP)		((UP)->flags & PMACZILOG_FLAG_TX_STOPPED)
#define ZS_TX_ACTIVE(UP)		((UP)->flags & PMACZILOG_FLAG_TX_ACTIVE)
#define ZS_WANTS_MODEM_STATUS(UP)	((UP)->flags & PMACZILOG_FLAG_MODEM_STATUS)
#define ZS_IS_IRDA(UP)			((UP)->flags & PMACZILOG_FLAG_IS_IRDA)
#define ZS_IS_INTMODEM(UP)		((UP)->flags & PMACZILOG_FLAG_IS_INTMODEM)
#define ZS_HAS_DMA(UP)			((UP)->flags & PMACZILOG_FLAG_HAS_DMA)
#define ZS_IS_OPEN(UP)			((UP)->flags & PMACZILOG_FLAG_IS_OPEN)
#define ZS_IS_EXTCLK(UP)		((UP)->flags & PMACZILOG_FLAG_IS_EXTCLK)

#endif 
