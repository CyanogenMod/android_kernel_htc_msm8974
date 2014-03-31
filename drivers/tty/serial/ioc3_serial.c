/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/circ_buf.h>
#include <linux/serial_reg.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/serial_core.h>
#include <linux/ioc3.h>
#include <linux/slab.h>


#define LOGICAL_PORTS		2	
#define PORTS_PER_CARD		2
#define LOGICAL_PORTS_PER_CARD (PORTS_PER_CARD * LOGICAL_PORTS)
#define MAX_CARDS		8
#define MAX_LOGICAL_PORTS	(LOGICAL_PORTS_PER_CARD * MAX_CARDS)

#define GET_PORT_FROM_SIO_IR(_x)	(_x & SIO_IR_SA) ? 0 : 1


#define GET_PHYSICAL_PORT(_x)	((_x) >> 1)
#define GET_LOGICAL_PORT(_x)	((_x) & 1)
#define IS_PHYSICAL_PORT(_x)	!((_x) & 1)
#define IS_RS232(_x)		!((_x) & 1)

static unsigned int Num_of_ioc3_cards;
static unsigned int Submodule_slot;

#define DPRINT_CONFIG(_x...)	;
#define NOT_PROGRESS()	;

#define MAX_CHARS		256
#define FIFO_SIZE		(MAX_CHARS-1)	

#define DEVICE_NAME		"ttySIOC"
#define DEVICE_MAJOR		204
#define DEVICE_MINOR		116

#define NCS_BREAK		0x1
#define NCS_PARITY		0x2
#define NCS_FRAMING		0x4
#define NCS_OVERRUN		0x8

#define MIN_BAUD_SUPPORTED	1200
#define MAX_BAUD_SUPPORTED	115200

#define PROTO_RS232		0
#define PROTO_RS422		1

#define N_DATA_READY		0x01
#define N_OUTPUT_LOWAT		0x02
#define N_BREAK			0x04
#define N_PARITY_ERROR		0x08
#define N_FRAMING_ERROR		0x10
#define N_OVERRUN_ERROR		0x20
#define N_DDCD			0x40
#define N_DCTS			0x80

#define N_ALL_INPUT		(N_DATA_READY | N_BREAK			   \
					| N_PARITY_ERROR | N_FRAMING_ERROR \
					| N_OVERRUN_ERROR | N_DDCD | N_DCTS)

#define N_ALL_OUTPUT		N_OUTPUT_LOWAT

#define N_ALL_ERRORS		(N_PARITY_ERROR | N_FRAMING_ERROR \
						| N_OVERRUN_ERROR)

#define N_ALL			(N_DATA_READY | N_OUTPUT_LOWAT | N_BREAK    \
					| N_PARITY_ERROR | N_FRAMING_ERROR  \
					| N_OVERRUN_ERROR | N_DDCD | N_DCTS)

#define SER_CLK_SPEED(prediv)	((22000000 << 1) / prediv)
#define SER_DIVISOR(x, clk)	(((clk) + (x) * 8) / ((x) * 16))
#define DIVISOR_TO_BAUD(div, clk) ((clk) / 16 / (div))

#define LCR_MASK_BITS_CHAR	(UART_LCR_WLEN5 | UART_LCR_WLEN6 \
					| UART_LCR_WLEN7 | UART_LCR_WLEN8)
#define LCR_MASK_STOP_BITS	(UART_LCR_STOP)

#define PENDING(_a, _p)		(readl(&(_p)->vma->sio_ir) & (_a)->ic_enable)

#define RING_BUF_SIZE		4096
#define BUF_SIZE_BIT		SBBR_L_SIZE
#define PROD_CONS_MASK		PROD_CONS_PTR_4K

#define TOTAL_RING_BUF_SIZE	(RING_BUF_SIZE * 4)

struct ioc3_card {
	struct {
		
		struct uart_port icp_uart_port[LOGICAL_PORTS];
		
		struct ioc3_port *icp_port;
	} ic_port[PORTS_PER_CARD];
	
	uint32_t ic_enable;
};

struct ioc3_port {
	
	struct uart_port *ip_port;
	struct ioc3_card *ip_card;
	struct ioc3_driver_data *ip_idd;
	struct ioc3_submodule *ip_is;

	
	struct ioc3_serialregs __iomem *ip_serial_regs;
	struct ioc3_uartregs __iomem *ip_uart_regs;

	
	dma_addr_t ip_dma_ringbuf;
	
	struct ring_buffer *ip_cpu_ringbuf;

	
	struct ring *ip_inring;
	struct ring *ip_outring;

	
	struct port_hooks *ip_hooks;

	spinlock_t ip_lock;

	
	int ip_baud;
	int ip_tx_lowat;
	int ip_rx_timeout;

	
	int ip_notify;

	uint32_t ip_sscr;
	uint32_t ip_tx_prod;
	uint32_t ip_rx_cons;
	unsigned char ip_flags;
};

#define TX_LOWAT_LATENCY      1000
#define TX_LOWAT_HZ          (1000000 / TX_LOWAT_LATENCY)
#define TX_LOWAT_CHARS(baud) (baud / 10 / TX_LOWAT_HZ)

#define INPUT_HIGH		0x01
#define DCD_ON			0x02
	
#define LOWAT_WRITTEN		0x04
#define READ_ABORTED		0x08
#define INPUT_ENABLE		0x10

struct port_hooks {
	uint32_t intr_delta_dcd;
	uint32_t intr_delta_cts;
	uint32_t intr_tx_mt;
	uint32_t intr_rx_timer;
	uint32_t intr_rx_high;
	uint32_t intr_tx_explicit;
	uint32_t intr_clear;
	uint32_t intr_all;
	char rs422_select_pin;
};

static struct port_hooks hooks_array[PORTS_PER_CARD] = {
	
	{
	.intr_delta_dcd = SIO_IR_SA_DELTA_DCD,
	.intr_delta_cts = SIO_IR_SA_DELTA_CTS,
	.intr_tx_mt = SIO_IR_SA_TX_MT,
	.intr_rx_timer = SIO_IR_SA_RX_TIMER,
	.intr_rx_high = SIO_IR_SA_RX_HIGH,
	.intr_tx_explicit = SIO_IR_SA_TX_EXPLICIT,
	.intr_clear = (SIO_IR_SA_TX_MT | SIO_IR_SA_RX_FULL
				| SIO_IR_SA_RX_HIGH
				| SIO_IR_SA_RX_TIMER
				| SIO_IR_SA_DELTA_DCD
				| SIO_IR_SA_DELTA_CTS
				| SIO_IR_SA_INT
				| SIO_IR_SA_TX_EXPLICIT
				| SIO_IR_SA_MEMERR),
	.intr_all =  SIO_IR_SA,
	.rs422_select_pin = GPPR_UARTA_MODESEL_PIN,
	 },

	
	{
	.intr_delta_dcd = SIO_IR_SB_DELTA_DCD,
	.intr_delta_cts = SIO_IR_SB_DELTA_CTS,
	.intr_tx_mt = SIO_IR_SB_TX_MT,
	.intr_rx_timer = SIO_IR_SB_RX_TIMER,
	.intr_rx_high = SIO_IR_SB_RX_HIGH,
	.intr_tx_explicit = SIO_IR_SB_TX_EXPLICIT,
	.intr_clear = (SIO_IR_SB_TX_MT | SIO_IR_SB_RX_FULL
				| SIO_IR_SB_RX_HIGH
				| SIO_IR_SB_RX_TIMER
				| SIO_IR_SB_DELTA_DCD
				| SIO_IR_SB_DELTA_CTS
				| SIO_IR_SB_INT
				| SIO_IR_SB_TX_EXPLICIT
				| SIO_IR_SB_MEMERR),
	.intr_all = SIO_IR_SB,
	.rs422_select_pin = GPPR_UARTB_MODESEL_PIN,
	 }
};

struct ring_entry {
	union {
		struct {
			uint32_t alldata;
			uint32_t allsc;
		} all;
		struct {
			char data[4];	
			char sc[4];	
		} s;
	} u;
};

#define RING_ANY_VALID \
	((uint32_t)(RXSB_MODEM_VALID | RXSB_DATA_VALID) * 0x01010101)

#define ring_sc		u.s.sc
#define ring_data	u.s.data
#define ring_allsc	u.all.allsc

#define ENTRIES_PER_RING (RING_BUF_SIZE / (int) sizeof(struct ring_entry))

struct ring {
	struct ring_entry entries[ENTRIES_PER_RING];
};

struct ring_buffer {
	struct ring TX_A;
	struct ring RX_A;
	struct ring TX_B;
	struct ring RX_B;
};

#define RING(_p, _wh)	&(((struct ring_buffer *)((_p)->ip_cpu_ringbuf))->_wh)

#define MAXITER		10000000


static int set_baud(struct ioc3_port *port, int baud)
{
	int divisor;
	int actual_baud;
	int diff;
	int lcr, prediv;
	struct ioc3_uartregs __iomem *uart;

	for (prediv = 6; prediv < 64; prediv++) {
		divisor = SER_DIVISOR(baud, SER_CLK_SPEED(prediv));
		if (!divisor)
			continue;	
		actual_baud = DIVISOR_TO_BAUD(divisor, SER_CLK_SPEED(prediv));

		diff = actual_baud - baud;
		if (diff < 0)
			diff = -diff;

		
		if (diff * 100 <= actual_baud)
			break;
	}

	if (prediv == 64) {
		NOT_PROGRESS();
		return 1;
	}

	uart = port->ip_uart_regs;
	lcr = readb(&uart->iu_lcr);

	writeb(lcr | UART_LCR_DLAB, &uart->iu_lcr);
	writeb((unsigned char)divisor, &uart->iu_dll);
	writeb((unsigned char)(divisor >> 8), &uart->iu_dlm);
	writeb((unsigned char)prediv, &uart->iu_scr);
	writeb((unsigned char)lcr, &uart->iu_lcr);

	return 0;
}

static struct ioc3_port *get_ioc3_port(struct uart_port *the_port)
{
	struct ioc3_driver_data *idd = dev_get_drvdata(the_port->dev);
	struct ioc3_card *card_ptr = idd->data[Submodule_slot];
	int ii, jj;

	if (!card_ptr) {
		NOT_PROGRESS();
		return NULL;
	}
	for (ii = 0; ii < PORTS_PER_CARD; ii++) {
		for (jj = 0; jj < LOGICAL_PORTS; jj++) {
			if (the_port == &card_ptr->ic_port[ii].icp_uart_port[jj])
				return card_ptr->ic_port[ii].icp_port;
		}
	}
	NOT_PROGRESS();
	return NULL;
}

static int inline port_init(struct ioc3_port *port)
{
	uint32_t sio_cr;
	struct port_hooks *hooks = port->ip_hooks;
	struct ioc3_uartregs __iomem *uart;
	int reset_loop_counter = 0xfffff;
	struct ioc3_driver_data *idd = port->ip_idd;

	
	writel(SSCR_RESET, &port->ip_serial_regs->sscr);

	
	do {
		sio_cr = readl(&idd->vma->sio_cr);
		if (reset_loop_counter-- <= 0) {
			printk(KERN_WARNING
			       "IOC3 unable to come out of reset"
				" scr 0x%x\n", sio_cr);
			return -1;
		}
	} while (!(sio_cr & SIO_CR_ARB_DIAG_IDLE) &&
	       (((sio_cr &= SIO_CR_ARB_DIAG) == SIO_CR_ARB_DIAG_TXA)
		|| sio_cr == SIO_CR_ARB_DIAG_TXB
		|| sio_cr == SIO_CR_ARB_DIAG_RXA
		|| sio_cr == SIO_CR_ARB_DIAG_RXB));

	
	writel(0, &port->ip_serial_regs->sscr);

	port->ip_tx_prod = readl(&port->ip_serial_regs->stcir) & PROD_CONS_MASK;
	writel(port->ip_tx_prod, &port->ip_serial_regs->stpir);
	port->ip_rx_cons = readl(&port->ip_serial_regs->srpir) & PROD_CONS_MASK;
	writel(port->ip_rx_cons | SRCIR_ARM, &port->ip_serial_regs->srcir);

	
	uart = port->ip_uart_regs;
	writeb(0, &uart->iu_lcr);
	writeb(0, &uart->iu_ier);

	
	set_baud(port, port->ip_baud);

	
	writeb(UART_LCR_WLEN8 | 0, &uart->iu_lcr);
	

	
	writeb(UART_FCR_ENABLE_FIFO, &uart->iu_fcr);
	
	writeb(UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT,
	       &uart->iu_fcr);

	
	writeb(0, &uart->iu_mcr);

	
	writel(0, &port->ip_serial_regs->shadow);

	
	if (port->ip_hooks == &hooks_array[0]) {
		unsigned long ring_pci_addr;
		uint32_t __iomem *sbbr_l, *sbbr_h;

		sbbr_l = &idd->vma->sbbr_l;
		sbbr_h = &idd->vma->sbbr_h;
		ring_pci_addr = (unsigned long __iomem)port->ip_dma_ringbuf;
		DPRINT_CONFIG(("%s: ring_pci_addr 0x%p\n",
			       __func__, (void *)ring_pci_addr));

		writel((unsigned int)((uint64_t) ring_pci_addr >> 32), sbbr_h);
		writel((unsigned int)ring_pci_addr | BUF_SIZE_BIT, sbbr_l);
	}

	
	writel(SRTR_HZ / 100, &port->ip_serial_regs->srtr);

	
	
	port->ip_sscr = (ENTRIES_PER_RING * 3 / 4);

	port->ip_sscr |= SSCR_HIGH_SPD;

	writel(port->ip_sscr, &port->ip_serial_regs->sscr);

	
	port->ip_card->ic_enable &= ~hooks->intr_clear;
	ioc3_disable(port->ip_is, idd, hooks->intr_clear);
	ioc3_ack(port->ip_is, idd, hooks->intr_clear);
	return 0;
}

static void enable_intrs(struct ioc3_port *port, uint32_t mask)
{
	if ((port->ip_card->ic_enable & mask) != mask) {
		port->ip_card->ic_enable |= mask;
		ioc3_enable(port->ip_is, port->ip_idd, mask);
	}
}

static inline int local_open(struct ioc3_port *port)
{
	int spiniter = 0;

	port->ip_flags = INPUT_ENABLE;

	
	if (port->ip_sscr & SSCR_DMA_EN) {
		writel(port->ip_sscr | SSCR_DMA_PAUSE,
		       &port->ip_serial_regs->sscr);
		while ((readl(&port->ip_serial_regs->sscr)
			& SSCR_PAUSE_STATE) == 0) {
			spiniter++;
			if (spiniter > MAXITER) {
				NOT_PROGRESS();
				return -1;
			}
		}
	}

	writeb(UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR,
	       &port->ip_uart_regs->iu_fcr);

	writeb(UART_LCR_WLEN8, &port->ip_uart_regs->iu_lcr);
	

	port->ip_sscr &= ~SSCR_RX_THRESHOLD;
	port->ip_sscr |= 1;	

	writel(port->ip_sscr, &port->ip_serial_regs->sscr);
	port->ip_tx_lowat = 1;
	return 0;
}

static inline int set_rx_timeout(struct ioc3_port *port, int timeout)
{
	int threshold;

	port->ip_rx_timeout = timeout;

	threshold = timeout * port->ip_baud / 4000;
	if (threshold == 0)
		threshold = 1;	

	if ((unsigned)threshold > (unsigned)SSCR_RX_THRESHOLD)
		return 1;

	port->ip_sscr &= ~SSCR_RX_THRESHOLD;
	port->ip_sscr |= threshold;
	writel(port->ip_sscr, &port->ip_serial_regs->sscr);

	timeout = timeout * SRTR_HZ / 100;
	if (timeout > SRTR_CNT)
		timeout = SRTR_CNT;
	writel(timeout, &port->ip_serial_regs->srtr);
	return 0;
}

static inline int
config_port(struct ioc3_port *port,
	    int baud, int byte_size, int stop_bits, int parenb, int parodd)
{
	char lcr, sizebits;
	int spiniter = 0;

	DPRINT_CONFIG(("%s: line %d baud %d byte_size %d stop %d parenb %d "
			"parodd %d\n",
		       __func__, ((struct uart_port *)port->ip_port)->line,
			baud, byte_size, stop_bits, parenb, parodd));

	if (set_baud(port, baud))
		return 1;

	switch (byte_size) {
	case 5:
		sizebits = UART_LCR_WLEN5;
		break;
	case 6:
		sizebits = UART_LCR_WLEN6;
		break;
	case 7:
		sizebits = UART_LCR_WLEN7;
		break;
	case 8:
		sizebits = UART_LCR_WLEN8;
		break;
	default:
		return 1;
	}

	
	if (port->ip_sscr & SSCR_DMA_EN) {
		writel(port->ip_sscr | SSCR_DMA_PAUSE,
		       &port->ip_serial_regs->sscr);
		while ((readl(&port->ip_serial_regs->sscr)
			& SSCR_PAUSE_STATE) == 0) {
			spiniter++;
			if (spiniter > MAXITER)
				return -1;
		}
	}

	
	lcr = readb(&port->ip_uart_regs->iu_lcr);
	lcr &= ~(LCR_MASK_BITS_CHAR | UART_LCR_EPAR |
		 UART_LCR_PARITY | LCR_MASK_STOP_BITS);

	
	lcr |= sizebits;

	
	if (parenb) {
		lcr |= UART_LCR_PARITY;
		if (!parodd)
			lcr |= UART_LCR_EPAR;
	}

	
	if (stop_bits)
		lcr |= UART_LCR_STOP  ;

	writeb(lcr, &port->ip_uart_regs->iu_lcr);

	
	if (port->ip_sscr & SSCR_DMA_EN) {
		writel(port->ip_sscr, &port->ip_serial_regs->sscr);
	}
	port->ip_baud = baud;

	port->ip_tx_lowat = (TX_LOWAT_CHARS(baud) + 3) / 4;
	if (port->ip_tx_lowat == 0)
		port->ip_tx_lowat = 1;

	set_rx_timeout(port, 2);
	return 0;
}

/**
 * do_write - Write bytes to the port.  Returns the number of bytes
 *			actually written. Called from transmit_chars
 * @port: port to use
 * @buf: the stuff to write
 * @len: how many bytes in 'buf'
 */
static inline int do_write(struct ioc3_port *port, char *buf, int len)
{
	int prod_ptr, cons_ptr, total = 0;
	struct ring *outring;
	struct ring_entry *entry;
	struct port_hooks *hooks = port->ip_hooks;

	BUG_ON(!(len >= 0));

	prod_ptr = port->ip_tx_prod;
	cons_ptr = readl(&port->ip_serial_regs->stcir) & PROD_CONS_MASK;
	outring = port->ip_outring;

	cons_ptr = (cons_ptr - (int)sizeof(struct ring_entry)) & PROD_CONS_MASK;

	
	while ((prod_ptr != cons_ptr) && (len > 0)) {
		int xx;

		
		entry = (struct ring_entry *)((caddr_t) outring + prod_ptr);

		
		entry->ring_allsc = 0;

		
		for (xx = 0; (xx < 4) && (len > 0); xx++) {
			entry->ring_data[xx] = *buf++;
			entry->ring_sc[xx] = TXCB_VALID;
			len--;
			total++;
		}

		if (!(port->ip_flags & LOWAT_WRITTEN) &&
		    ((cons_ptr - prod_ptr) & PROD_CONS_MASK)
		    <= port->ip_tx_lowat * (int)sizeof(struct ring_entry)) {
			port->ip_flags |= LOWAT_WRITTEN;
			entry->ring_sc[0] |= TXCB_INT_WHEN_DONE;
		}

		
		prod_ptr += sizeof(struct ring_entry);
		prod_ptr &= PROD_CONS_MASK;
	}

	
	if (total > 0 && !(port->ip_sscr & SSCR_DMA_EN)) {
		port->ip_sscr |= SSCR_DMA_EN;
		writel(port->ip_sscr, &port->ip_serial_regs->sscr);
	}

	if (!uart_tx_stopped(port->ip_port)) {
		writel(prod_ptr, &port->ip_serial_regs->stpir);

		if (total > 0)
			enable_intrs(port, hooks->intr_tx_mt);
	}
	port->ip_tx_prod = prod_ptr;

	return total;
}

static inline void disable_intrs(struct ioc3_port *port, uint32_t mask)
{
	if (port->ip_card->ic_enable & mask) {
		ioc3_disable(port->ip_is, port->ip_idd, mask);
		port->ip_card->ic_enable &= ~mask;
	}
}

static int set_notification(struct ioc3_port *port, int mask, int set_on)
{
	struct port_hooks *hooks = port->ip_hooks;
	uint32_t intrbits, sscrbits;

	BUG_ON(!mask);

	intrbits = sscrbits = 0;

	if (mask & N_DATA_READY)
		intrbits |= (hooks->intr_rx_timer | hooks->intr_rx_high);
	if (mask & N_OUTPUT_LOWAT)
		intrbits |= hooks->intr_tx_explicit;
	if (mask & N_DDCD) {
		intrbits |= hooks->intr_delta_dcd;
		sscrbits |= SSCR_RX_RING_DCD;
	}
	if (mask & N_DCTS)
		intrbits |= hooks->intr_delta_cts;

	if (set_on) {
		enable_intrs(port, intrbits);
		port->ip_notify |= mask;
		port->ip_sscr |= sscrbits;
	} else {
		disable_intrs(port, intrbits);
		port->ip_notify &= ~mask;
		port->ip_sscr &= ~sscrbits;
	}

	if (port->ip_notify & (N_DATA_READY | N_DDCD))
		port->ip_sscr |= SSCR_DMA_EN;
	else if (!(port->ip_card->ic_enable & hooks->intr_tx_mt))
		port->ip_sscr &= ~SSCR_DMA_EN;

	writel(port->ip_sscr, &port->ip_serial_regs->sscr);
	return 0;
}

static inline int set_mcr(struct uart_port *the_port,
			  int mask1, int mask2)
{
	struct ioc3_port *port = get_ioc3_port(the_port);
	uint32_t shadow;
	int spiniter = 0;
	char mcr;

	if (!port)
		return -1;

	
	if (port->ip_sscr & SSCR_DMA_EN) {
		writel(port->ip_sscr | SSCR_DMA_PAUSE,
		       &port->ip_serial_regs->sscr);
		while ((readl(&port->ip_serial_regs->sscr)
			& SSCR_PAUSE_STATE) == 0) {
			spiniter++;
			if (spiniter > MAXITER)
				return -1;
		}
	}
	shadow = readl(&port->ip_serial_regs->shadow);
	mcr = (shadow & 0xff000000) >> 24;

	
	mcr |= mask1;
	shadow |= mask2;
	writeb(mcr, &port->ip_uart_regs->iu_mcr);
	writel(shadow, &port->ip_serial_regs->shadow);

	
	if (port->ip_sscr & SSCR_DMA_EN) {
		writel(port->ip_sscr, &port->ip_serial_regs->sscr);
	}
	return 0;
}

static int ioc3_set_proto(struct ioc3_port *port, int proto)
{
	struct port_hooks *hooks = port->ip_hooks;

	switch (proto) {
	default:
	case PROTO_RS232:
		
		DPRINT_CONFIG(("%s: rs232\n", __func__));
		writel(0, (&port->ip_idd->vma->gppr[0]
					+ hooks->rs422_select_pin));
		break;

	case PROTO_RS422:
		
		DPRINT_CONFIG(("%s: rs422\n", __func__));
		writel(1, (&port->ip_idd->vma->gppr[0]
					+ hooks->rs422_select_pin));
		break;
	}
	return 0;
}

static void transmit_chars(struct uart_port *the_port)
{
	int xmit_count, tail, head;
	int result;
	char *start;
	struct tty_struct *tty;
	struct ioc3_port *port = get_ioc3_port(the_port);
	struct uart_state *state;

	if (!the_port)
		return;
	if (!port)
		return;

	state = the_port->state;
	tty = state->port.tty;

	if (uart_circ_empty(&state->xmit) || uart_tx_stopped(the_port)) {
		
		set_notification(port, N_ALL_OUTPUT, 0);
		return;
	}

	head = state->xmit.head;
	tail = state->xmit.tail;
	start = (char *)&state->xmit.buf[tail];

	
	xmit_count = (head < tail) ? (UART_XMIT_SIZE - tail) : (head - tail);
	if (xmit_count > 0) {
		result = do_write(port, start, xmit_count);
		if (result > 0) {
			
			xmit_count -= result;
			the_port->icount.tx += result;
			
			tail += result;
			tail &= UART_XMIT_SIZE - 1;
			state->xmit.tail = tail;
			start = (char *)&state->xmit.buf[tail];
		}
	}
	if (uart_circ_chars_pending(&state->xmit) < WAKEUP_CHARS)
		uart_write_wakeup(the_port);

	if (uart_circ_empty(&state->xmit)) {
		set_notification(port, N_OUTPUT_LOWAT, 0);
	} else {
		set_notification(port, N_OUTPUT_LOWAT, 1);
	}
}

static void
ioc3_change_speed(struct uart_port *the_port,
		  struct ktermios *new_termios, struct ktermios *old_termios)
{
	struct ioc3_port *port = get_ioc3_port(the_port);
	unsigned int cflag, iflag;
	int baud;
	int new_parity = 0, new_parity_enable = 0, new_stop = 0, new_data = 8;
	struct uart_state *state = the_port->state;

	cflag = new_termios->c_cflag;
	iflag = new_termios->c_iflag;

	switch (cflag & CSIZE) {
	case CS5:
		new_data = 5;
		break;
	case CS6:
		new_data = 6;
		break;
	case CS7:
		new_data = 7;
		break;
	case CS8:
		new_data = 8;
		break;
	default:
		
		new_data = 5;
		break;
	}
	if (cflag & CSTOPB) {
		new_stop = 1;
	}
	if (cflag & PARENB) {
		new_parity_enable = 1;
		if (cflag & PARODD)
			new_parity = 1;
	}
	baud = uart_get_baud_rate(the_port, new_termios, old_termios,
				  MIN_BAUD_SUPPORTED, MAX_BAUD_SUPPORTED);
	DPRINT_CONFIG(("%s: returned baud %d for line %d\n", __func__, baud,
				the_port->line));

	if (!the_port->fifosize)
		the_port->fifosize = FIFO_SIZE;
	uart_update_timeout(the_port, cflag, baud);

	the_port->ignore_status_mask = N_ALL_INPUT;

	state->port.tty->low_latency = 1;

	if (iflag & IGNPAR)
		the_port->ignore_status_mask &= ~(N_PARITY_ERROR
						  | N_FRAMING_ERROR);
	if (iflag & IGNBRK) {
		the_port->ignore_status_mask &= ~N_BREAK;
		if (iflag & IGNPAR)
			the_port->ignore_status_mask &= ~N_OVERRUN_ERROR;
	}
	if (!(cflag & CREAD)) {
		
		the_port->ignore_status_mask &= ~N_DATA_READY;
	}

	if (cflag & CRTSCTS) {
		
		port->ip_sscr |= SSCR_HFC_EN;
	}
	else {
		
		port->ip_sscr &= ~SSCR_HFC_EN;
	}
	writel(port->ip_sscr, &port->ip_serial_regs->sscr);

	
	DPRINT_CONFIG(("%s : port 0x%p line %d cflag 0%o "
		       "config_port(baud %d data %d stop %d penable %d "
			" parity %d), notification 0x%x\n",
		       __func__, (void *)port, the_port->line, cflag, baud,
		       new_data, new_stop, new_parity_enable, new_parity,
		       the_port->ignore_status_mask));

	if ((config_port(port, baud,	
			 new_data,	
			 new_stop,	
			 new_parity_enable,	
			 new_parity)) >= 0) {	
		set_notification(port, the_port->ignore_status_mask, 1);
	}
}

static inline int ic3_startup_local(struct uart_port *the_port)
{
	struct ioc3_port *port;

	if (!the_port) {
		NOT_PROGRESS();
		return -1;
	}

	port = get_ioc3_port(the_port);
	if (!port) {
		NOT_PROGRESS();
		return -1;
	}

	local_open(port);

	
	ioc3_set_proto(port, IS_RS232(the_port->line) ? PROTO_RS232 :
							PROTO_RS422);
	return 0;
}

static void ioc3_cb_output_lowat(struct ioc3_port *port)
{
	unsigned long pflags;

	
	if (port->ip_port) {
		spin_lock_irqsave(&port->ip_port->lock, pflags);
		transmit_chars(port->ip_port);
		spin_unlock_irqrestore(&port->ip_port->lock, pflags);
	}
}

static void ioc3_cb_post_ncs(struct uart_port *the_port, int ncs)
{
	struct uart_icount *icount;

	icount = &the_port->icount;

	if (ncs & NCS_BREAK)
		icount->brk++;
	if (ncs & NCS_FRAMING)
		icount->frame++;
	if (ncs & NCS_OVERRUN)
		icount->overrun++;
	if (ncs & NCS_PARITY)
		icount->parity++;
}


static inline int do_read(struct uart_port *the_port, char *buf, int len)
{
	int prod_ptr, cons_ptr, total;
	struct ioc3_port *port = get_ioc3_port(the_port);
	struct ring *inring;
	struct ring_entry *entry;
	struct port_hooks *hooks = port->ip_hooks;
	int byte_num;
	char *sc;
	int loop_counter;

	BUG_ON(!(len >= 0));
	BUG_ON(!port);

	

	writel(port->ip_rx_cons | SRCIR_ARM, &port->ip_serial_regs->srcir);

	prod_ptr = readl(&port->ip_serial_regs->srpir) & PROD_CONS_MASK;
	cons_ptr = port->ip_rx_cons;

	if (prod_ptr == cons_ptr) {
		int reset_dma = 0;

		

		
		if (!(port->ip_sscr & SSCR_DMA_EN)) {
			port->ip_sscr |= SSCR_DMA_EN;
			reset_dma = 1;
		}

		writel(port->ip_sscr | SSCR_RX_DRAIN,
		       &port->ip_serial_regs->sscr);
		prod_ptr = readl(&port->ip_serial_regs->srpir) & PROD_CONS_MASK;

		if (prod_ptr == cons_ptr) {
			loop_counter = 0;
			while (readl(&port->ip_serial_regs->sscr) &
			       SSCR_RX_DRAIN) {
				loop_counter++;
				if (loop_counter > MAXITER)
					return -1;
			}

			prod_ptr = readl(&port->ip_serial_regs->srpir)
			    & PROD_CONS_MASK;
		}
		if (reset_dma) {
			port->ip_sscr &= ~SSCR_DMA_EN;
			writel(port->ip_sscr, &port->ip_serial_regs->sscr);
		}
	}
	inring = port->ip_inring;
	port->ip_flags &= ~READ_ABORTED;

	total = 0;
	loop_counter = 0xfffff;	

	
	while ((prod_ptr != cons_ptr) && (len > 0)) {
		entry = (struct ring_entry *)((caddr_t) inring + cons_ptr);

		if (loop_counter-- <= 0) {
			printk(KERN_WARNING "IOC3 serial: "
			       "possible hang condition/"
			       "port stuck on read (line %d).\n",
				the_port->line);
			break;
		}

		if ((entry->ring_allsc & RING_ANY_VALID) == 0) {
			port->ip_flags |= READ_ABORTED;
			len = 0;
			break;
		}

		
		for (byte_num = 0; byte_num < 4 && len > 0; byte_num++) {
			sc = &(entry->ring_sc[byte_num]);

			
			if ((*sc & RXSB_MODEM_VALID)
			    && (port->ip_notify & N_DDCD)) {
				
				if ((port->ip_flags & DCD_ON)
				    && !(*sc & RXSB_DCD)) {
					if (total > 0) {
						len = 0;
						break;
					}
					port->ip_flags &= ~DCD_ON;

					*sc &= ~RXSB_MODEM_VALID;


					if ((entry->ring_allsc & RING_ANY_VALID)
					    == 0) {
						cons_ptr += (int)sizeof
						    (struct ring_entry);
						cons_ptr &= PROD_CONS_MASK;
					}
					writel(cons_ptr,
					       &port->ip_serial_regs->srcir);
					port->ip_rx_cons = cons_ptr;

					
					if ((port->ip_notify & N_DDCD)
					    && port->ip_port) {
						uart_handle_dcd_change
							(port->ip_port, 0);
						wake_up_interruptible
						    (&the_port->state->
						     port.delta_msr_wait);
					}

					return 0;
				}
			}
			if (*sc & RXSB_MODEM_VALID) {
				
				if ((*sc & RXSB_OVERRUN)
				    && (port->ip_notify & N_OVERRUN_ERROR)) {
					ioc3_cb_post_ncs(the_port, NCS_OVERRUN);
				}
				
				*sc &= ~RXSB_MODEM_VALID;
			}

			
			if ((*sc & RXSB_DATA_VALID) &&
			    ((*sc & (RXSB_PAR_ERR
				     | RXSB_FRAME_ERR | RXSB_BREAK))
			     && (port->ip_notify & (N_PARITY_ERROR
						    | N_FRAMING_ERROR
						    | N_BREAK)))) {
				if (total > 0) {
					len = 0;
					break;
				} else {
					if ((*sc & RXSB_PAR_ERR) &&
					    (port->
					     ip_notify & N_PARITY_ERROR)) {
						ioc3_cb_post_ncs(the_port,
								 NCS_PARITY);
					}
					if ((*sc & RXSB_FRAME_ERR) &&
					    (port->
					     ip_notify & N_FRAMING_ERROR)) {
						ioc3_cb_post_ncs(the_port,
								 NCS_FRAMING);
					}
					if ((*sc & RXSB_BREAK)
					    && (port->ip_notify & N_BREAK)) {
						ioc3_cb_post_ncs
						    (the_port, NCS_BREAK);
					}
					len = 1;
				}
			}
			if (*sc & RXSB_DATA_VALID) {
				*sc &= ~RXSB_DATA_VALID;
				*buf = entry->ring_data[byte_num];
				buf++;
				len--;
				total++;
			}
		}

		if ((entry->ring_allsc & RING_ANY_VALID) == 0) {
			cons_ptr += (int)sizeof(struct ring_entry);
			cons_ptr &= PROD_CONS_MASK;
		}
	}

	
	writel(cons_ptr, &port->ip_serial_regs->srcir);
	port->ip_rx_cons = cons_ptr;

	if ((port->ip_flags & INPUT_HIGH) && (((prod_ptr - cons_ptr)
					       & PROD_CONS_MASK) <
					      ((port->
						ip_sscr &
						SSCR_RX_THRESHOLD)
					       << PROD_CONS_PTR_OFF))) {
		port->ip_flags &= ~INPUT_HIGH;
		enable_intrs(port, hooks->intr_rx_high);
	}
	return total;
}

static int receive_chars(struct uart_port *the_port)
{
	struct tty_struct *tty;
	unsigned char ch[MAX_CHARS];
	int read_count = 0, read_room, flip = 0;
	struct uart_state *state = the_port->state;
	struct ioc3_port *port = get_ioc3_port(the_port);
	unsigned long pflags;

	
	if (!state)
		return 0;
	if (!state->port.tty)
		return 0;

	if (!(port->ip_flags & INPUT_ENABLE))
		return 0;

	spin_lock_irqsave(&the_port->lock, pflags);
	tty = state->port.tty;

	read_count = do_read(the_port, ch, MAX_CHARS);
	if (read_count > 0) {
		flip = 1;
		read_room = tty_insert_flip_string(tty, ch, read_count);
		the_port->icount.rx += read_count;
	}
	spin_unlock_irqrestore(&the_port->lock, pflags);

	if (flip)
		tty_flip_buffer_push(tty);

	return read_count;
}


static int inline
ioc3uart_intr_one(struct ioc3_submodule *is,
			struct ioc3_driver_data *idd,
			unsigned int pending)
{
	int port_num = GET_PORT_FROM_SIO_IR(pending);
	struct port_hooks *hooks;
	unsigned int rx_high_rd_aborted = 0;
	unsigned long flags;
	struct uart_port *the_port;
	struct ioc3_port *port;
	int loop_counter;
	struct ioc3_card *card_ptr;
	unsigned int sio_ir;

	card_ptr = idd->data[is->id];
	port = card_ptr->ic_port[port_num].icp_port;
	hooks = port->ip_hooks;


	sio_ir = pending & ~(hooks->intr_tx_mt);
	spin_lock_irqsave(&port->ip_lock, flags);

	loop_counter = MAXITER;	

	do {
		uint32_t shadow;

		if (loop_counter-- <= 0) {
			printk(KERN_WARNING "IOC3 serial: "
			       "possible hang condition/"
			       "port stuck on interrupt (line %d).\n",
				((struct uart_port *)port->ip_port)->line);
			break;
		}
		
		if (sio_ir & hooks->intr_delta_dcd) {
			ioc3_ack(is, idd, hooks->intr_delta_dcd);
			shadow = readl(&port->ip_serial_regs->shadow);

			if ((port->ip_notify & N_DDCD)
			    && (shadow & SHADOW_DCD)
			    && (port->ip_port)) {
				the_port = port->ip_port;
				uart_handle_dcd_change(the_port,
						shadow & SHADOW_DCD);
				wake_up_interruptible
				    (&the_port->state->port.delta_msr_wait);
			} else if ((port->ip_notify & N_DDCD)
				   && !(shadow & SHADOW_DCD)) {
				
				uart_handle_dcd_change(port->ip_port,
						shadow & SHADOW_DCD);
				port->ip_flags |= DCD_ON;
			}
		}

		
		if (sio_ir & hooks->intr_delta_cts) {
			ioc3_ack(is, idd, hooks->intr_delta_cts);
			shadow = readl(&port->ip_serial_regs->shadow);

			if ((port->ip_notify & N_DCTS) && (port->ip_port)) {
				the_port = port->ip_port;
				uart_handle_cts_change(the_port, shadow
						& SHADOW_CTS);
				wake_up_interruptible
				    (&the_port->state->port.delta_msr_wait);
			}
		}

		if (sio_ir & hooks->intr_rx_timer) {
			ioc3_ack(is, idd, hooks->intr_rx_timer);
			if ((port->ip_notify & N_DATA_READY)
						&& (port->ip_port)) {
				receive_chars(port->ip_port);
			}
		}

		
		else if (sio_ir & hooks->intr_rx_high) {
			
			if ((port->ip_notify & N_DATA_READY) && port->ip_port) {
				receive_chars(port->ip_port);
			}

			if ((sio_ir = PENDING(card_ptr, idd))
					& hooks->intr_rx_high) {
				if (port->ip_flags & READ_ABORTED) {
					rx_high_rd_aborted++;
				}
				else {
					card_ptr->ic_enable &= ~hooks->intr_rx_high;
					port->ip_flags |= INPUT_HIGH;
				}
			}
		}

		if (sio_ir & hooks->intr_tx_explicit) {
			port->ip_flags &= ~LOWAT_WRITTEN;
			ioc3_ack(is, idd, hooks->intr_tx_explicit);
			if (port->ip_notify & N_OUTPUT_LOWAT)
				ioc3_cb_output_lowat(port);
		}

		
		else if (sio_ir & hooks->intr_tx_mt) {
			if (port->ip_notify & N_OUTPUT_LOWAT) {
				ioc3_cb_output_lowat(port);

				sio_ir = PENDING(card_ptr, idd);
			}

			if (sio_ir & hooks->intr_tx_mt) {
				if (!(port->ip_notify
				      & (N_DATA_READY | N_DDCD))) {
					BUG_ON(!(port->ip_sscr
						 & SSCR_DMA_EN));
					port->ip_sscr &= ~SSCR_DMA_EN;
					writel(port->ip_sscr,
					       &port->ip_serial_regs->sscr);
				}
				
				card_ptr->ic_enable &= ~hooks->intr_tx_mt;
			}
		}
		sio_ir = PENDING(card_ptr, idd);


		if (rx_high_rd_aborted && (sio_ir == hooks->intr_rx_high)) {
			sio_ir &= ~hooks->intr_rx_high;
		}
	} while (sio_ir & hooks->intr_all);

	spin_unlock_irqrestore(&port->ip_lock, flags);
	ioc3_enable(is, idd, card_ptr->ic_enable);
	return 0;
}


static int ioc3uart_intr(struct ioc3_submodule *is,
			struct ioc3_driver_data *idd,
			unsigned int pending)
{
	int ret = 0;


	if (pending & SIO_IR_SA)
		ret |= ioc3uart_intr_one(is, idd, pending & SIO_IR_SA);
	if (pending & SIO_IR_SB)
		ret |= ioc3uart_intr_one(is, idd, pending & SIO_IR_SB);

	return ret;
}

static const char *ic3_type(struct uart_port *the_port)
{
	if (IS_RS232(the_port->line))
		return "SGI IOC3 Serial [rs232]";
	else
		return "SGI IOC3 Serial [rs422]";
}

static unsigned int ic3_tx_empty(struct uart_port *the_port)
{
	unsigned int ret = 0;
	struct ioc3_port *port = get_ioc3_port(the_port);

	if (readl(&port->ip_serial_regs->shadow) & SHADOW_TEMT)
		ret = TIOCSER_TEMT;
	return ret;
}

static void ic3_stop_tx(struct uart_port *the_port)
{
	struct ioc3_port *port = get_ioc3_port(the_port);

	if (port)
		set_notification(port, N_OUTPUT_LOWAT, 0);
}

static void ic3_stop_rx(struct uart_port *the_port)
{
	struct ioc3_port *port = get_ioc3_port(the_port);

	if (port)
		port->ip_flags &= ~INPUT_ENABLE;
}

static void null_void_function(struct uart_port *the_port)
{
}

static void ic3_shutdown(struct uart_port *the_port)
{
	unsigned long port_flags;
	struct ioc3_port *port;
	struct uart_state *state;

	port = get_ioc3_port(the_port);
	if (!port)
		return;

	state = the_port->state;
	wake_up_interruptible(&state->port.delta_msr_wait);

	spin_lock_irqsave(&the_port->lock, port_flags);
	set_notification(port, N_ALL, 0);
	spin_unlock_irqrestore(&the_port->lock, port_flags);
}

static void ic3_set_mctrl(struct uart_port *the_port, unsigned int mctrl)
{
	unsigned char mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	set_mcr(the_port, mcr, SHADOW_DTR);
}

static unsigned int ic3_get_mctrl(struct uart_port *the_port)
{
	struct ioc3_port *port = get_ioc3_port(the_port);
	uint32_t shadow;
	unsigned int ret = 0;

	if (!port)
		return 0;

	shadow = readl(&port->ip_serial_regs->shadow);
	if (shadow & SHADOW_DCD)
		ret |= TIOCM_CD;
	if (shadow & SHADOW_DR)
		ret |= TIOCM_DSR;
	if (shadow & SHADOW_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void ic3_start_tx(struct uart_port *the_port)
{
	struct ioc3_port *port = get_ioc3_port(the_port);

	if (port) {
		set_notification(port, N_OUTPUT_LOWAT, 1);
		enable_intrs(port, port->ip_hooks->intr_tx_mt);
	}
}

static void ic3_break_ctl(struct uart_port *the_port, int break_state)
{
}

static int ic3_startup(struct uart_port *the_port)
{
	int retval;
	struct ioc3_port *port;
	struct ioc3_card *card_ptr;
	unsigned long port_flags;

	if (!the_port) {
		NOT_PROGRESS();
		return -ENODEV;
	}
	port = get_ioc3_port(the_port);
	if (!port) {
		NOT_PROGRESS();
		return -ENODEV;
	}
	card_ptr = port->ip_card;
	port->ip_port = the_port;

	if (!card_ptr) {
		NOT_PROGRESS();
		return -ENODEV;
	}

	
	spin_lock_irqsave(&the_port->lock, port_flags);
	retval = ic3_startup_local(the_port);
	spin_unlock_irqrestore(&the_port->lock, port_flags);
	return retval;
}

static void
ic3_set_termios(struct uart_port *the_port,
		struct ktermios *termios, struct ktermios *old_termios)
{
	unsigned long port_flags;

	spin_lock_irqsave(&the_port->lock, port_flags);
	ioc3_change_speed(the_port, termios, old_termios);
	spin_unlock_irqrestore(&the_port->lock, port_flags);
}

static int ic3_request_port(struct uart_port *port)
{
	return 0;
}

static struct uart_ops ioc3_ops = {
	.tx_empty = ic3_tx_empty,
	.set_mctrl = ic3_set_mctrl,
	.get_mctrl = ic3_get_mctrl,
	.stop_tx = ic3_stop_tx,
	.start_tx = ic3_start_tx,
	.stop_rx = ic3_stop_rx,
	.enable_ms = null_void_function,
	.break_ctl = ic3_break_ctl,
	.startup = ic3_startup,
	.shutdown = ic3_shutdown,
	.set_termios = ic3_set_termios,
	.type = ic3_type,
	.release_port = null_void_function,
	.request_port = ic3_request_port,
};


static struct uart_driver ioc3_uart = {
	.owner = THIS_MODULE,
	.driver_name = "ioc3_serial",
	.dev_name = DEVICE_NAME,
	.major = DEVICE_MAJOR,
	.minor = DEVICE_MINOR,
	.nr = MAX_LOGICAL_PORTS
};

static inline int ioc3_serial_core_attach( struct ioc3_submodule *is,
				struct ioc3_driver_data *idd)
{
	struct ioc3_port *port;
	struct uart_port *the_port;
	struct ioc3_card *card_ptr = idd->data[is->id];
	int ii, phys_port;
	struct pci_dev *pdev = idd->pdev;

	DPRINT_CONFIG(("%s: attach pdev 0x%p - card_ptr 0x%p\n",
		       __func__, pdev, (void *)card_ptr));

	if (!card_ptr)
		return -ENODEV;

	
	for (ii = 0; ii < LOGICAL_PORTS_PER_CARD; ii++) {
		phys_port = GET_PHYSICAL_PORT(ii);
		the_port = &card_ptr->ic_port[phys_port].
				icp_uart_port[GET_LOGICAL_PORT(ii)];
		port = card_ptr->ic_port[phys_port].icp_port;
		port->ip_port = the_port;

		DPRINT_CONFIG(("%s: attach the_port 0x%p / port 0x%p [%d/%d]\n",
			__func__, (void *)the_port, (void *)port,
				phys_port, ii));

		
		the_port->membase = (unsigned char __iomem *)1;
		the_port->iobase = (pdev->bus->number << 16) |  ii;
		the_port->line = (Num_of_ioc3_cards << 2) | ii;
		the_port->mapbase = 1;
		the_port->type = PORT_16550A;
		the_port->fifosize = FIFO_SIZE;
		the_port->ops = &ioc3_ops;
		the_port->irq = idd->irq_io;
		the_port->dev = &pdev->dev;

		if (uart_add_one_port(&ioc3_uart, the_port) < 0) {
			printk(KERN_WARNING
		          "%s: unable to add port %d bus %d\n",
			       __func__, the_port->line, pdev->bus->number);
		} else {
			DPRINT_CONFIG(("IOC3 serial port %d irq %d bus %d\n",
		          the_port->line, the_port->irq, pdev->bus->number));
		}

		
		if (IS_PHYSICAL_PORT(ii))
			ioc3_set_proto(port, PROTO_RS232);
	}
	return 0;
}


static int ioc3uart_remove(struct ioc3_submodule *is,
			struct ioc3_driver_data *idd)
{
	struct ioc3_card *card_ptr = idd->data[is->id];
	struct uart_port *the_port;
	struct ioc3_port *port;
	int ii;

	if (card_ptr) {
		for (ii = 0; ii < LOGICAL_PORTS_PER_CARD; ii++) {
			the_port = &card_ptr->ic_port[GET_PHYSICAL_PORT(ii)].
					icp_uart_port[GET_LOGICAL_PORT(ii)];
			if (the_port)
				uart_remove_one_port(&ioc3_uart, the_port);
			port = card_ptr->ic_port[GET_PHYSICAL_PORT(ii)].icp_port;
			if (port && IS_PHYSICAL_PORT(ii)
					&& (GET_PHYSICAL_PORT(ii) == 0)) {
				pci_free_consistent(port->ip_idd->pdev,
					TOTAL_RING_BUF_SIZE,
					(void *)port->ip_cpu_ringbuf,
					port->ip_dma_ringbuf);
				kfree(port);
				card_ptr->ic_port[GET_PHYSICAL_PORT(ii)].
							icp_port = NULL;
			}
		}
		kfree(card_ptr);
		idd->data[is->id] = NULL;
	}
	return 0;
}


static int __devinit
ioc3uart_probe(struct ioc3_submodule *is, struct ioc3_driver_data *idd)
{
	struct pci_dev *pdev = idd->pdev;
	struct ioc3_card *card_ptr;
	int ret = 0;
	struct ioc3_port *port;
	struct ioc3_port *ports[PORTS_PER_CARD];
	int phys_port;
	int cnt;

	DPRINT_CONFIG(("%s (0x%p, 0x%p)\n", __func__, is, idd));

	card_ptr = kzalloc(sizeof(struct ioc3_card), GFP_KERNEL);
	if (!card_ptr) {
		printk(KERN_WARNING "ioc3_attach_one"
		       ": unable to get memory for the IOC3\n");
		return -ENOMEM;
	}
	idd->data[is->id] = card_ptr;
	Submodule_slot = is->id;

	writel(((UARTA_BASE >> 3) << SIO_CR_SER_A_BASE_SHIFT) |
		((UARTB_BASE >> 3) << SIO_CR_SER_B_BASE_SHIFT) |
		(0xf << SIO_CR_CMD_PULSE_SHIFT), &idd->vma->sio_cr);

	pci_write_config_dword(pdev, PCI_LAT, 0xff00);

	
	ioc3_gpcr_set(idd, GPCR_UARTA_MODESEL | GPCR_UARTB_MODESEL);

	
	for (phys_port = 0; phys_port < PORTS_PER_CARD; phys_port++) {
		port = kzalloc(sizeof(struct ioc3_port), GFP_KERNEL);
		if (!port) {
			printk(KERN_WARNING
			       "IOC3 serial memory not available for port\n");
			ret = -ENOMEM;
			goto out4;
		}
		spin_lock_init(&port->ip_lock);

		ports[phys_port] = port;

		
		card_ptr->ic_port[phys_port].icp_port = port;
		port->ip_is = is;
		port->ip_idd = idd;
		port->ip_baud = 9600;
		port->ip_card = card_ptr;
		port->ip_hooks = &hooks_array[phys_port];

		
		if (phys_port == 0) {
			port->ip_serial_regs = &idd->vma->port_a;
			port->ip_uart_regs = &idd->vma->sregs.uarta;

			DPRINT_CONFIG(("%s : Port A ip_serial_regs 0x%p "
				       "ip_uart_regs 0x%p\n",
				       __func__,
				       (void *)port->ip_serial_regs,
				       (void *)port->ip_uart_regs));

			
			port->ip_cpu_ringbuf = pci_alloc_consistent(pdev,
				TOTAL_RING_BUF_SIZE, &port->ip_dma_ringbuf);

			BUG_ON(!((((int64_t) port->ip_dma_ringbuf) &
				  (TOTAL_RING_BUF_SIZE - 1)) == 0));
			port->ip_inring = RING(port, RX_A);
			port->ip_outring = RING(port, TX_A);
			DPRINT_CONFIG(("%s : Port A ip_cpu_ringbuf 0x%p "
				       "ip_dma_ringbuf 0x%p, ip_inring 0x%p "
					"ip_outring 0x%p\n",
				       __func__,
				       (void *)port->ip_cpu_ringbuf,
				       (void *)port->ip_dma_ringbuf,
				       (void *)port->ip_inring,
				       (void *)port->ip_outring));
		}
		else {
			port->ip_serial_regs = &idd->vma->port_b;
			port->ip_uart_regs = &idd->vma->sregs.uartb;

			DPRINT_CONFIG(("%s : Port B ip_serial_regs 0x%p "
				       "ip_uart_regs 0x%p\n",
				       __func__,
				       (void *)port->ip_serial_regs,
				       (void *)port->ip_uart_regs));

			
			port->ip_dma_ringbuf =
			    ports[phys_port - 1]->ip_dma_ringbuf;
			port->ip_cpu_ringbuf =
			    ports[phys_port - 1]->ip_cpu_ringbuf;
			port->ip_inring = RING(port, RX_B);
			port->ip_outring = RING(port, TX_B);
			DPRINT_CONFIG(("%s : Port B ip_cpu_ringbuf 0x%p "
				       "ip_dma_ringbuf 0x%p, ip_inring 0x%p "
					"ip_outring 0x%p\n",
				       __func__,
				       (void *)port->ip_cpu_ringbuf,
				       (void *)port->ip_dma_ringbuf,
				       (void *)port->ip_inring,
				       (void *)port->ip_outring));
		}

		DPRINT_CONFIG(("%s : port %d [addr 0x%p] card_ptr 0x%p",
			       __func__,
			       phys_port, (void *)port, (void *)card_ptr));
		DPRINT_CONFIG((" ip_serial_regs 0x%p ip_uart_regs 0x%p\n",
			       (void *)port->ip_serial_regs,
			       (void *)port->ip_uart_regs));

		
		port_init(port);

		DPRINT_CONFIG(("%s: phys_port %d port 0x%p inring 0x%p "
			       "outring 0x%p\n",
			       __func__,
			       phys_port, (void *)port,
			       (void *)port->ip_inring,
			       (void *)port->ip_outring));

	}

	

	if ((ret = ioc3_serial_core_attach(is, idd)))
		goto out4;

	Num_of_ioc3_cards++;

	return ret;

	
out4:
	for (cnt = 0; cnt < phys_port; cnt++)
		kfree(ports[cnt]);

	kfree(card_ptr);
	return ret;
}

static struct ioc3_submodule ioc3uart_ops = {
	.name = "IOC3uart",
	.probe = ioc3uart_probe,
	.remove = ioc3uart_remove,
	
	.irq_mask = SIO_IR_SA | SIO_IR_SB,
	.intr = ioc3uart_intr,
	.owner = THIS_MODULE,
};

static int __init ioc3uart_init(void)
{
	int ret;

	
	if ((ret = uart_register_driver(&ioc3_uart)) < 0) {
		printk(KERN_WARNING
		       "%s: Couldn't register IOC3 uart serial driver\n",
		       __func__);
		return ret;
	}
	ret = ioc3_register_submodule(&ioc3uart_ops);
	if (ret)
		uart_unregister_driver(&ioc3_uart);
	return ret;
}

static void __exit ioc3uart_exit(void)
{
	ioc3_unregister_submodule(&ioc3uart_ops);
	uart_unregister_driver(&ioc3_uart);
}

module_init(ioc3uart_init);
module_exit(ioc3uart_exit);

MODULE_AUTHOR("Pat Gefre - Silicon Graphics Inc. (SGI) <pfg@sgi.com>");
MODULE_DESCRIPTION("Serial PCI driver module for SGI IOC3 card");
MODULE_LICENSE("GPL");
