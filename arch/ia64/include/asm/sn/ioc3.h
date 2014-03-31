/*
 * Copyright (C) 2005 Silicon Graphics, Inc.
 */
#ifndef IA64_SN_IOC3_H
#define IA64_SN_IOC3_H

struct ioc3_serialregs {
	uint32_t sscr;
	uint32_t stpir;
	uint32_t stcir;
	uint32_t srpir;
	uint32_t srcir;
	uint32_t srtr;
	uint32_t shadow;
};

struct ioc3_uartregs {
	char iu_lcr;
	union {
		char iir;	
		char fcr;	
	} u3;
	union {
		char ier;	
		char dlm;	
	} u2;
	union {
		char rbr;	
		char thr;	
		char dll;	
	} u1;
	char iu_scr;
	char iu_msr;
	char iu_lsr;
	char iu_mcr;
};

#define iu_rbr u1.rbr
#define iu_thr u1.thr
#define iu_dll u1.dll
#define iu_ier u2.ier
#define iu_dlm u2.dlm
#define iu_iir u3.iir
#define iu_fcr u3.fcr

struct ioc3_sioregs {
	char fill[0x170];
	struct ioc3_uartregs uartb;
	struct ioc3_uartregs uarta;
};

struct ioc3 {
	uint32_t pci_id;
	uint32_t pci_scr;
	uint32_t pci_rev;
	uint32_t pci_lat;
	uint32_t pci_addr;
	uint32_t pci_err_addr_l;
	uint32_t pci_err_addr_h;

	uint32_t sio_ir;
	uint32_t sio_ies;
	uint32_t sio_iec;
	uint32_t sio_cr;
	uint32_t int_out;
	uint32_t mcr;
	uint32_t gpcr_s;
	uint32_t gpcr_c;
	uint32_t gpdr;
	uint32_t gppr[9];
	char fill[0x4c];

	
	uint32_t sbbr_h;
	uint32_t sbbr_l;

	struct ioc3_serialregs port_a;
	struct ioc3_serialregs port_b;
	char fill1[0x1ff10];
	
	struct ioc3_sioregs sregs;
};

#define eier	fill1[8]
#define eisr	fill1[4]

#define PCI_LAT			0xc	
#define PCI_SCR_DROP_MODE_EN	0x00008000 
#define UARTA_BASE		0x178
#define UARTB_BASE		0x170


#define RXSB_OVERRUN		0x01	
#define RXSB_PAR_ERR		0x02	
#define RXSB_FRAME_ERR		0x04	
#define RXSB_BREAK		0x08	
#define RXSB_CTS		0x10	
#define RXSB_DCD		0x20	
#define RXSB_MODEM_VALID	0x40	
#define RXSB_DATA_VALID		0x80	

#define TXCB_INT_WHEN_DONE	0x20	
#define TXCB_INVALID		0x00	
#define TXCB_VALID		0x40	
#define TXCB_MCR		0x80	
#define TXCB_DELAY		0xc0	

#define SBBR_L_SIZE		0x00000001	

#define SSCR_RX_THRESHOLD	0x000001ff	
#define SSCR_TX_TIMER_BUSY	0x00010000	
#define SSCR_HFC_EN		0x00020000	
#define SSCR_RX_RING_DCD	0x00040000	
#define SSCR_RX_RING_CTS	0x00080000	
#define SSCR_HIGH_SPD		0x00100000	
#define SSCR_DIAG		0x00200000	
#define SSCR_RX_DRAIN		0x08000000	
#define SSCR_DMA_EN		0x10000000	
#define SSCR_DMA_PAUSE		0x20000000	
#define SSCR_PAUSE_STATE	0x40000000	
#define SSCR_RESET		0x80000000	

#define PROD_CONS_PTR_4K	0x00000ff8	
#define PROD_CONS_PTR_1K	0x000003f8	
#define PROD_CONS_PTR_OFF	3

#define SRCIR_ARM		0x80000000	

#define SHADOW_DR		0x00000001	
#define SHADOW_OE		0x00000002	
#define SHADOW_PE		0x00000004	
#define SHADOW_FE		0x00000008	
#define SHADOW_BI		0x00000010	
#define SHADOW_THRE		0x00000020	
#define SHADOW_TEMT		0x00000040	
#define SHADOW_RFCE		0x00000080	
#define SHADOW_DCTS		0x00010000	
#define SHADOW_DDCD		0x00080000	
#define SHADOW_CTS		0x00100000	
#define SHADOW_DCD		0x00800000	
#define SHADOW_DTR		0x01000000	
#define SHADOW_RTS		0x02000000	
#define SHADOW_OUT1		0x04000000	
#define SHADOW_OUT2		0x08000000	
#define SHADOW_LOOP		0x10000000	

#define SRTR_CNT		0x00000fff	
#define SRTR_CNT_VAL		0x0fff0000	
#define SRTR_CNT_VAL_SHIFT	16
#define SRTR_HZ			16000		

#define SIO_IR_SA_TX_MT		0x00000001	
#define SIO_IR_SA_RX_FULL	0x00000002	
#define SIO_IR_SA_RX_HIGH	0x00000004	
#define SIO_IR_SA_RX_TIMER	0x00000008	
#define SIO_IR_SA_DELTA_DCD	0x00000010	
#define SIO_IR_SA_DELTA_CTS	0x00000020	
#define SIO_IR_SA_INT		0x00000040	
#define SIO_IR_SA_TX_EXPLICIT	0x00000080	
#define SIO_IR_SA_MEMERR	0x00000100	
#define SIO_IR_SB_TX_MT		0x00000200
#define SIO_IR_SB_RX_FULL	0x00000400
#define SIO_IR_SB_RX_HIGH	0x00000800
#define SIO_IR_SB_RX_TIMER	0x00001000
#define SIO_IR_SB_DELTA_DCD	0x00002000
#define SIO_IR_SB_DELTA_CTS	0x00004000
#define SIO_IR_SB_INT		0x00008000
#define SIO_IR_SB_TX_EXPLICIT	0x00010000
#define SIO_IR_SB_MEMERR	0x00020000
#define SIO_IR_PP_INT		0x00040000	
#define SIO_IR_PP_INTA		0x00080000	
#define SIO_IR_PP_INTB		0x00100000	
#define SIO_IR_PP_MEMERR	0x00200000	
#define SIO_IR_KBD_INT		0x00400000	
#define SIO_IR_RT_INT		0x08000000	
#define SIO_IR_GEN_INT1		0x10000000	
#define SIO_IR_GEN_INT_SHIFT	28

#define SIO_IR_SA		(SIO_IR_SA_TX_MT | \
				 SIO_IR_SA_RX_FULL | \
				 SIO_IR_SA_RX_HIGH | \
				 SIO_IR_SA_RX_TIMER | \
				 SIO_IR_SA_DELTA_DCD | \
				 SIO_IR_SA_DELTA_CTS | \
				 SIO_IR_SA_INT | \
				 SIO_IR_SA_TX_EXPLICIT | \
				 SIO_IR_SA_MEMERR)

#define SIO_IR_SB		(SIO_IR_SB_TX_MT | \
				 SIO_IR_SB_RX_FULL | \
				 SIO_IR_SB_RX_HIGH | \
				 SIO_IR_SB_RX_TIMER | \
				 SIO_IR_SB_DELTA_DCD | \
				 SIO_IR_SB_DELTA_CTS | \
				 SIO_IR_SB_INT | \
				 SIO_IR_SB_TX_EXPLICIT | \
				 SIO_IR_SB_MEMERR)

#define SIO_IR_PP		(SIO_IR_PP_INT | SIO_IR_PP_INTA | \
				 SIO_IR_PP_INTB | SIO_IR_PP_MEMERR)
#define SIO_IR_RT		(SIO_IR_RT_INT | SIO_IR_GEN_INT1)

#define SIO_CR_CMD_PULSE_SHIFT 15
#define SIO_CR_SER_A_BASE_SHIFT 1
#define SIO_CR_SER_B_BASE_SHIFT 8
#define SIO_CR_ARB_DIAG		0x00380000	
#define SIO_CR_ARB_DIAG_TXA	0x00000000
#define SIO_CR_ARB_DIAG_RXA	0x00080000
#define SIO_CR_ARB_DIAG_TXB	0x00100000
#define SIO_CR_ARB_DIAG_RXB	0x00180000
#define SIO_CR_ARB_DIAG_PP	0x00200000
#define SIO_CR_ARB_DIAG_IDLE	0x00400000	

#define GPCR_PHY_RESET		0x20	
#define GPCR_UARTB_MODESEL	0x40	
#define GPCR_UARTA_MODESEL	0x80	

#define GPPR_PHY_RESET_PIN	5	
#define GPPR_UARTB_MODESEL_PIN	6	
#define GPPR_UARTA_MODESEL_PIN	7	

#endif 
