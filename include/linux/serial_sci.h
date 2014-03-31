#ifndef __LINUX_SERIAL_SCI_H
#define __LINUX_SERIAL_SCI_H

#include <linux/serial_core.h>
#include <linux/sh_dma.h>


#define SCIx_NOT_SUPPORTED	(-1)

enum {
	SCBRR_ALGO_1,		
	SCBRR_ALGO_2,		
	SCBRR_ALGO_3,		
	SCBRR_ALGO_4,		
	SCBRR_ALGO_5,		
};

#define SCSCR_TIE	(1 << 7)
#define SCSCR_RIE	(1 << 6)
#define SCSCR_TE	(1 << 5)
#define SCSCR_RE	(1 << 4)
#define SCSCR_REIE	(1 << 3)	
#define SCSCR_TOIE	(1 << 2)	
#define SCSCR_CKE1	(1 << 1)
#define SCSCR_CKE0	(1 << 0)

#define SCI_TDRE  0x80
#define SCI_RDRF  0x40
#define SCI_ORER  0x20
#define SCI_FER   0x10
#define SCI_PER   0x08
#define SCI_TEND  0x04

#define SCI_DEFAULT_ERROR_MASK (SCI_PER | SCI_FER)

#define SCIF_ER    0x0080
#define SCIF_TEND  0x0040
#define SCIF_TDFE  0x0020
#define SCIF_BRK   0x0010
#define SCIF_FER   0x0008
#define SCIF_PER   0x0004
#define SCIF_RDF   0x0002
#define SCIF_DR    0x0001

#define SCIF_DEFAULT_ERROR_MASK (SCIF_PER | SCIF_FER | SCIF_ER | SCIF_BRK)

#define SCSPTR_RTSIO	(1 << 7)
#define SCSPTR_CTSIO	(1 << 5)

enum {
	SCIx_ERI_IRQ,
	SCIx_RXI_IRQ,
	SCIx_TXI_IRQ,
	SCIx_BRI_IRQ,
	SCIx_NR_IRQS,

	SCIx_MUX_IRQ = SCIx_NR_IRQS,	
};

enum {
	SCIx_SCK,
	SCIx_RXD,
	SCIx_TXD,
	SCIx_CTS,
	SCIx_RTS,

	SCIx_NR_FNS,
};

enum {
	SCIx_PROBE_REGTYPE,

	SCIx_SCI_REGTYPE,
	SCIx_IRDA_REGTYPE,
	SCIx_SCIFA_REGTYPE,
	SCIx_SCIFB_REGTYPE,
	SCIx_SH2_SCIF_FIFODATA_REGTYPE,
	SCIx_SH3_SCIF_REGTYPE,
	SCIx_SH4_SCIF_REGTYPE,
	SCIx_SH4_SCIF_NO_SCSPTR_REGTYPE,
	SCIx_SH4_SCIF_FIFODATA_REGTYPE,
	SCIx_SH7705_SCIF_REGTYPE,

	SCIx_NR_REGTYPES,
};

#define SCIx_IRQ_MUXED(irq)		\
{					\
	[SCIx_ERI_IRQ]	= (irq),	\
	[SCIx_RXI_IRQ]	= (irq),	\
	[SCIx_TXI_IRQ]	= (irq),	\
	[SCIx_BRI_IRQ]	= (irq),	\
}

#define SCIx_IRQ_IS_MUXED(port)			\
	((port)->cfg->irqs[SCIx_ERI_IRQ] ==	\
	 (port)->cfg->irqs[SCIx_RXI_IRQ]) ||	\
	((port)->cfg->irqs[SCIx_ERI_IRQ] &&	\
	 !(port)->cfg->irqs[SCIx_RXI_IRQ])
enum {
	SCSMR, SCBRR, SCSCR, SCxSR,
	SCFCR, SCFDR, SCxTDR, SCxRDR,
	SCLSR, SCTFDR, SCRFDR, SCSPTR,

	SCIx_NR_REGS,
};

struct device;

struct plat_sci_port_ops {
	void (*init_pins)(struct uart_port *, unsigned int cflag);
};

#define SCIx_HAVE_RTSCTS	(1 << 0)

struct plat_sci_port {
	unsigned long	mapbase;		
	unsigned int	irqs[SCIx_NR_IRQS];	
	unsigned int	gpios[SCIx_NR_FNS];	
	unsigned int	type;			
	upf_t		flags;			
	unsigned long	capabilities;		

	unsigned int	scbrr_algo_id;		
	unsigned int	scscr;			

	int		overrun_bit;
	unsigned int	error_mask;

	int		port_reg;
	unsigned char	regshift;
	unsigned char	regtype;

	struct plat_sci_port_ops	*ops;

	unsigned int	dma_slave_tx;
	unsigned int	dma_slave_rx;
};

#endif 
