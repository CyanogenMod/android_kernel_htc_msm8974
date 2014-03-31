/*
 *  Driver for Atmel AT91 / AT32 Serial ports
 *  Copyright (C) 2003 Rick Bronson
 *
 *  Based on drivers/char/serial_sa1100.c, by Deep Blue Solutions Ltd.
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  DMA support added by Chip Coldwell.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/tty_flip.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/atmel_pdc.h>
#include <linux/atmel_serial.h>
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/ioctls.h>

#include <asm/mach/serial_at91.h>
#include <mach/board.h>

#ifdef CONFIG_ARM
#include <mach/cpu.h>
#include <asm/gpio.h>
#endif

#define PDC_BUFFER_SIZE		512
#define PDC_RX_TIMEOUT		(3 * 10)		

#if defined(CONFIG_SERIAL_ATMEL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

static void atmel_start_rx(struct uart_port *port);
static void atmel_stop_rx(struct uart_port *port);

#ifdef CONFIG_SERIAL_ATMEL_TTYAT

#define SERIAL_ATMEL_MAJOR	204
#define MINOR_START		154
#define ATMEL_DEVICENAME	"ttyAT"

#else

#define SERIAL_ATMEL_MAJOR	TTY_MAJOR
#define MINOR_START		64
#define ATMEL_DEVICENAME	"ttyS"

#endif

#define ATMEL_ISR_PASS_LIMIT	256

#define UART_PUT_CR(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_CR)
#define UART_GET_MR(port)	__raw_readl((port)->membase + ATMEL_US_MR)
#define UART_PUT_MR(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_MR)
#define UART_PUT_IER(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_IER)
#define UART_PUT_IDR(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_IDR)
#define UART_GET_IMR(port)	__raw_readl((port)->membase + ATMEL_US_IMR)
#define UART_GET_CSR(port)	__raw_readl((port)->membase + ATMEL_US_CSR)
#define UART_GET_CHAR(port)	__raw_readl((port)->membase + ATMEL_US_RHR)
#define UART_PUT_CHAR(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_THR)
#define UART_GET_BRGR(port)	__raw_readl((port)->membase + ATMEL_US_BRGR)
#define UART_PUT_BRGR(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_BRGR)
#define UART_PUT_RTOR(port,v)	__raw_writel(v, (port)->membase + ATMEL_US_RTOR)
#define UART_PUT_TTGR(port, v)	__raw_writel(v, (port)->membase + ATMEL_US_TTGR)

 
#define UART_PUT_PTCR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_PTCR)
#define UART_GET_PTSR(port)	__raw_readl((port)->membase + ATMEL_PDC_PTSR)

#define UART_PUT_RPR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_RPR)
#define UART_GET_RPR(port)	__raw_readl((port)->membase + ATMEL_PDC_RPR)
#define UART_PUT_RCR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_RCR)
#define UART_PUT_RNPR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_RNPR)
#define UART_PUT_RNCR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_RNCR)

#define UART_PUT_TPR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_TPR)
#define UART_PUT_TCR(port,v)	__raw_writel(v, (port)->membase + ATMEL_PDC_TCR)
#define UART_GET_TCR(port)	__raw_readl((port)->membase + ATMEL_PDC_TCR)

static int (*atmel_open_hook)(struct uart_port *);
static void (*atmel_close_hook)(struct uart_port *);

struct atmel_dma_buffer {
	unsigned char	*buf;
	dma_addr_t	dma_addr;
	unsigned int	dma_size;
	unsigned int	ofs;
};

struct atmel_uart_char {
	u16		status;
	u16		ch;
};

#define ATMEL_SERIAL_RINGSIZE 1024

struct atmel_uart_port {
	struct uart_port	uart;		
	struct clk		*clk;		
	int			may_wakeup;	
	u32			backup_imr;	
	int			break_active;	

	short			use_dma_rx;	
	short			pdc_rx_idx;	
	struct atmel_dma_buffer	pdc_rx[2];	

	short			use_dma_tx;	
	struct atmel_dma_buffer	pdc_tx;		

	struct tasklet_struct	tasklet;
	unsigned int		irq_status;
	unsigned int		irq_status_prev;

	struct circ_buf		rx_ring;

	struct serial_rs485	rs485;		
	unsigned int		tx_done_mask;
};

static struct atmel_uart_port atmel_ports[ATMEL_MAX_UART];
static unsigned long atmel_ports_in_use;

#ifdef SUPPORT_SYSRQ
static struct console atmel_console;
#endif

#if defined(CONFIG_OF)
static const struct of_device_id atmel_serial_dt_ids[] = {
	{ .compatible = "atmel,at91rm9200-usart" },
	{ .compatible = "atmel,at91sam9260-usart" },
	{  }
};

MODULE_DEVICE_TABLE(of, atmel_serial_dt_ids);
#endif

static inline struct atmel_uart_port *
to_atmel_uart_port(struct uart_port *uart)
{
	return container_of(uart, struct atmel_uart_port, uart);
}

#ifdef CONFIG_SERIAL_ATMEL_PDC
static bool atmel_use_dma_rx(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	return atmel_port->use_dma_rx;
}

static bool atmel_use_dma_tx(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	return atmel_port->use_dma_tx;
}
#else
static bool atmel_use_dma_rx(struct uart_port *port)
{
	return false;
}

static bool atmel_use_dma_tx(struct uart_port *port)
{
	return false;
}
#endif

void atmel_config_rs485(struct uart_port *port, struct serial_rs485 *rs485conf)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	unsigned int mode;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);

	
	UART_PUT_IDR(port, atmel_port->tx_done_mask);

	mode = UART_GET_MR(port);

	
	mode &= ~ATMEL_US_USMODE;

	atmel_port->rs485 = *rs485conf;

	if (rs485conf->flags & SER_RS485_ENABLED) {
		dev_dbg(port->dev, "Setting UART to RS485\n");
		atmel_port->tx_done_mask = ATMEL_US_TXEMPTY;
		if ((rs485conf->delay_rts_after_send) > 0)
			UART_PUT_TTGR(port, rs485conf->delay_rts_after_send);
		mode |= ATMEL_US_USMODE_RS485;
	} else {
		dev_dbg(port->dev, "Setting UART to RS232\n");
		if (atmel_use_dma_tx(port))
			atmel_port->tx_done_mask = ATMEL_US_ENDTX |
				ATMEL_US_TXBUFE;
		else
			atmel_port->tx_done_mask = ATMEL_US_TXRDY;
	}
	UART_PUT_MR(port, mode);

	
	UART_PUT_IER(port, atmel_port->tx_done_mask);

	spin_unlock_irqrestore(&port->lock, flags);

}

static u_int atmel_tx_empty(struct uart_port *port)
{
	return (UART_GET_CSR(port) & ATMEL_US_TXEMPTY) ? TIOCSER_TEMT : 0;
}

static void atmel_set_mctrl(struct uart_port *port, u_int mctrl)
{
	unsigned int control = 0;
	unsigned int mode;
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

#ifdef CONFIG_ARCH_AT91RM9200
	if (cpu_is_at91rm9200()) {
		if (port->mapbase == AT91RM9200_BASE_US0) {
			if (mctrl & TIOCM_RTS)
				at91_set_gpio_value(AT91_PIN_PA21, 0);
			else
				at91_set_gpio_value(AT91_PIN_PA21, 1);
		}
	}
#endif

	if (mctrl & TIOCM_RTS)
		control |= ATMEL_US_RTSEN;
	else
		control |= ATMEL_US_RTSDIS;

	if (mctrl & TIOCM_DTR)
		control |= ATMEL_US_DTREN;
	else
		control |= ATMEL_US_DTRDIS;

	UART_PUT_CR(port, control);

	
	mode = UART_GET_MR(port) & ~ATMEL_US_CHMODE;
	if (mctrl & TIOCM_LOOP)
		mode |= ATMEL_US_CHMODE_LOC_LOOP;
	else
		mode |= ATMEL_US_CHMODE_NORMAL;

	
	mode &= ~ATMEL_US_USMODE;

	if (atmel_port->rs485.flags & SER_RS485_ENABLED) {
		dev_dbg(port->dev, "Setting UART to RS485\n");
		if ((atmel_port->rs485.delay_rts_after_send) > 0)
			UART_PUT_TTGR(port,
					atmel_port->rs485.delay_rts_after_send);
		mode |= ATMEL_US_USMODE_RS485;
	} else {
		dev_dbg(port->dev, "Setting UART to RS232\n");
	}
	UART_PUT_MR(port, mode);
}

static u_int atmel_get_mctrl(struct uart_port *port)
{
	unsigned int status, ret = 0;

	status = UART_GET_CSR(port);

	if (!(status & ATMEL_US_DCD))
		ret |= TIOCM_CD;
	if (!(status & ATMEL_US_CTS))
		ret |= TIOCM_CTS;
	if (!(status & ATMEL_US_DSR))
		ret |= TIOCM_DSR;
	if (!(status & ATMEL_US_RI))
		ret |= TIOCM_RI;

	return ret;
}

static void atmel_stop_tx(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (atmel_use_dma_tx(port)) {
		
		UART_PUT_PTCR(port, ATMEL_PDC_TXTDIS);
	}
	
	UART_PUT_IDR(port, atmel_port->tx_done_mask);

	if ((atmel_port->rs485.flags & SER_RS485_ENABLED) &&
	    !(atmel_port->rs485.flags & SER_RS485_RX_DURING_TX))
		atmel_start_rx(port);
}

static void atmel_start_tx(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (atmel_use_dma_tx(port)) {
		if (UART_GET_PTSR(port) & ATMEL_PDC_TXTEN)
			return;

		if ((atmel_port->rs485.flags & SER_RS485_ENABLED) &&
		    !(atmel_port->rs485.flags & SER_RS485_RX_DURING_TX))
			atmel_stop_rx(port);

		
		UART_PUT_PTCR(port, ATMEL_PDC_TXTEN);
	}
	
	UART_PUT_IER(port, atmel_port->tx_done_mask);
}

static void atmel_start_rx(struct uart_port *port)
{
	UART_PUT_CR(port, ATMEL_US_RSTSTA);  

	UART_PUT_CR(port, ATMEL_US_RXEN);

	if (atmel_use_dma_rx(port)) {
		
		UART_PUT_IER(port, ATMEL_US_ENDRX | ATMEL_US_TIMEOUT |
			port->read_status_mask);
		UART_PUT_PTCR(port, ATMEL_PDC_RXTEN);
	} else {
		UART_PUT_IER(port, ATMEL_US_RXRDY);
	}
}

static void atmel_stop_rx(struct uart_port *port)
{
	UART_PUT_CR(port, ATMEL_US_RXDIS);

	if (atmel_use_dma_rx(port)) {
		
		UART_PUT_PTCR(port, ATMEL_PDC_RXTDIS);
		UART_PUT_IDR(port, ATMEL_US_ENDRX | ATMEL_US_TIMEOUT |
			port->read_status_mask);
	} else {
		UART_PUT_IDR(port, ATMEL_US_RXRDY);
	}
}

static void atmel_enable_ms(struct uart_port *port)
{
	UART_PUT_IER(port, ATMEL_US_RIIC | ATMEL_US_DSRIC
			| ATMEL_US_DCDIC | ATMEL_US_CTSIC);
}

static void atmel_break_ctl(struct uart_port *port, int break_state)
{
	if (break_state != 0)
		UART_PUT_CR(port, ATMEL_US_STTBRK);	
	else
		UART_PUT_CR(port, ATMEL_US_STPBRK);	
}

static void
atmel_buffer_rx_char(struct uart_port *port, unsigned int status,
		     unsigned int ch)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	struct circ_buf *ring = &atmel_port->rx_ring;
	struct atmel_uart_char *c;

	if (!CIRC_SPACE(ring->head, ring->tail, ATMEL_SERIAL_RINGSIZE))
		
		return;

	c = &((struct atmel_uart_char *)ring->buf)[ring->head];
	c->status	= status;
	c->ch		= ch;

	
	smp_wmb();

	ring->head = (ring->head + 1) & (ATMEL_SERIAL_RINGSIZE - 1);
}

static void atmel_pdc_rxerr(struct uart_port *port, unsigned int status)
{
	
	UART_PUT_CR(port, ATMEL_US_RSTSTA);

	if (status & ATMEL_US_RXBRK) {
		
		status &= ~(ATMEL_US_PARE | ATMEL_US_FRAME);
		port->icount.brk++;
	}
	if (status & ATMEL_US_PARE)
		port->icount.parity++;
	if (status & ATMEL_US_FRAME)
		port->icount.frame++;
	if (status & ATMEL_US_OVRE)
		port->icount.overrun++;
}

static void atmel_rx_chars(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	unsigned int status, ch;

	status = UART_GET_CSR(port);
	while (status & ATMEL_US_RXRDY) {
		ch = UART_GET_CHAR(port);

		if (unlikely(status & (ATMEL_US_PARE | ATMEL_US_FRAME
				       | ATMEL_US_OVRE | ATMEL_US_RXBRK)
			     || atmel_port->break_active)) {

			
			UART_PUT_CR(port, ATMEL_US_RSTSTA);

			if (status & ATMEL_US_RXBRK
			    && !atmel_port->break_active) {
				atmel_port->break_active = 1;
				UART_PUT_IER(port, ATMEL_US_RXBRK);
			} else {
				UART_PUT_IDR(port, ATMEL_US_RXBRK);
				status &= ~ATMEL_US_RXBRK;
				atmel_port->break_active = 0;
			}
		}

		atmel_buffer_rx_char(port, status, ch);
		status = UART_GET_CSR(port);
	}

	tasklet_schedule(&atmel_port->tasklet);
}

static void atmel_tx_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (port->x_char && UART_GET_CSR(port) & atmel_port->tx_done_mask) {
		UART_PUT_CHAR(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
		return;

	while (UART_GET_CSR(port) & atmel_port->tx_done_mask) {
		UART_PUT_CHAR(port, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (!uart_circ_empty(xmit))
		
		UART_PUT_IER(port, atmel_port->tx_done_mask);
}

static void
atmel_handle_receive(struct uart_port *port, unsigned int pending)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (atmel_use_dma_rx(port)) {
		if (pending & (ATMEL_US_ENDRX | ATMEL_US_TIMEOUT)) {
			UART_PUT_IDR(port, (ATMEL_US_ENDRX
						| ATMEL_US_TIMEOUT));
			tasklet_schedule(&atmel_port->tasklet);
		}

		if (pending & (ATMEL_US_RXBRK | ATMEL_US_OVRE |
				ATMEL_US_FRAME | ATMEL_US_PARE))
			atmel_pdc_rxerr(port, pending);
	}

	
	if (pending & ATMEL_US_RXRDY)
		atmel_rx_chars(port);
	else if (pending & ATMEL_US_RXBRK) {
		UART_PUT_CR(port, ATMEL_US_RSTSTA);
		UART_PUT_IDR(port, ATMEL_US_RXBRK);
		atmel_port->break_active = 0;
	}
}

static void
atmel_handle_transmit(struct uart_port *port, unsigned int pending)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (pending & atmel_port->tx_done_mask) {
		
		UART_PUT_IDR(port, atmel_port->tx_done_mask);
		tasklet_schedule(&atmel_port->tasklet);
	}
}

static void
atmel_handle_status(struct uart_port *port, unsigned int pending,
		    unsigned int status)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (pending & (ATMEL_US_RIIC | ATMEL_US_DSRIC | ATMEL_US_DCDIC
				| ATMEL_US_CTSIC)) {
		atmel_port->irq_status = status;
		tasklet_schedule(&atmel_port->tasklet);
	}
}

static irqreturn_t atmel_interrupt(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	unsigned int status, pending, pass_counter = 0;

	do {
		status = UART_GET_CSR(port);
		pending = status & UART_GET_IMR(port);
		if (!pending)
			break;

		atmel_handle_receive(port, pending);
		atmel_handle_status(port, pending, status);
		atmel_handle_transmit(port, pending);
	} while (pass_counter++ < ATMEL_ISR_PASS_LIMIT);

	return pass_counter ? IRQ_HANDLED : IRQ_NONE;
}

static void atmel_tx_dma(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	struct circ_buf *xmit = &port->state->xmit;
	struct atmel_dma_buffer *pdc = &atmel_port->pdc_tx;
	int count;

	
	if (UART_GET_TCR(port))
		return;

	xmit->tail += pdc->ofs;
	xmit->tail &= UART_XMIT_SIZE - 1;

	port->icount.tx += pdc->ofs;
	pdc->ofs = 0;

	

	
	UART_PUT_PTCR(port, ATMEL_PDC_TXTDIS);

	if (!uart_circ_empty(xmit) && !uart_tx_stopped(port)) {
		dma_sync_single_for_device(port->dev,
					   pdc->dma_addr,
					   pdc->dma_size,
					   DMA_TO_DEVICE);

		count = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		pdc->ofs = count;

		UART_PUT_TPR(port, pdc->dma_addr + xmit->tail);
		UART_PUT_TCR(port, count);
		
		UART_PUT_PTCR(port, ATMEL_PDC_TXTEN);
		
		UART_PUT_IER(port, atmel_port->tx_done_mask);
	} else {
		if ((atmel_port->rs485.flags & SER_RS485_ENABLED) &&
		    !(atmel_port->rs485.flags & SER_RS485_RX_DURING_TX)) {
			
			atmel_start_rx(port);
		}
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);
}

static void atmel_rx_from_ring(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	struct circ_buf *ring = &atmel_port->rx_ring;
	unsigned int flg;
	unsigned int status;

	while (ring->head != ring->tail) {
		struct atmel_uart_char c;

		
		smp_rmb();

		c = ((struct atmel_uart_char *)ring->buf)[ring->tail];

		ring->tail = (ring->tail + 1) & (ATMEL_SERIAL_RINGSIZE - 1);

		port->icount.rx++;
		status = c.status;
		flg = TTY_NORMAL;

		if (unlikely(status & (ATMEL_US_PARE | ATMEL_US_FRAME
				       | ATMEL_US_OVRE | ATMEL_US_RXBRK))) {
			if (status & ATMEL_US_RXBRK) {
				
				status &= ~(ATMEL_US_PARE | ATMEL_US_FRAME);

				port->icount.brk++;
				if (uart_handle_break(port))
					continue;
			}
			if (status & ATMEL_US_PARE)
				port->icount.parity++;
			if (status & ATMEL_US_FRAME)
				port->icount.frame++;
			if (status & ATMEL_US_OVRE)
				port->icount.overrun++;

			status &= port->read_status_mask;

			if (status & ATMEL_US_RXBRK)
				flg = TTY_BREAK;
			else if (status & ATMEL_US_PARE)
				flg = TTY_PARITY;
			else if (status & ATMEL_US_FRAME)
				flg = TTY_FRAME;
		}


		if (uart_handle_sysrq_char(port, c.ch))
			continue;

		uart_insert_char(port, status, ATMEL_US_OVRE, c.ch, flg);
	}

	spin_unlock(&port->lock);
	tty_flip_buffer_push(port->state->port.tty);
	spin_lock(&port->lock);
}

static void atmel_rx_from_dma(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	struct tty_struct *tty = port->state->port.tty;
	struct atmel_dma_buffer *pdc;
	int rx_idx = atmel_port->pdc_rx_idx;
	unsigned int head;
	unsigned int tail;
	unsigned int count;

	do {
		
		UART_PUT_CR(port, ATMEL_US_STTTO);

		pdc = &atmel_port->pdc_rx[rx_idx];
		head = UART_GET_RPR(port) - pdc->dma_addr;
		tail = pdc->ofs;

		head = min(head, pdc->dma_size);

		if (likely(head != tail)) {
			dma_sync_single_for_cpu(port->dev, pdc->dma_addr,
					pdc->dma_size, DMA_FROM_DEVICE);

			count = head - tail;

			tty_insert_flip_string(tty, pdc->buf + pdc->ofs, count);

			dma_sync_single_for_device(port->dev, pdc->dma_addr,
					pdc->dma_size, DMA_FROM_DEVICE);

			port->icount.rx += count;
			pdc->ofs = head;
		}

		if (head >= pdc->dma_size) {
			pdc->ofs = 0;
			UART_PUT_RNPR(port, pdc->dma_addr);
			UART_PUT_RNCR(port, pdc->dma_size);

			rx_idx = !rx_idx;
			atmel_port->pdc_rx_idx = rx_idx;
		}
	} while (head >= pdc->dma_size);

	spin_unlock(&port->lock);
	tty_flip_buffer_push(tty);
	spin_lock(&port->lock);

	UART_PUT_IER(port, ATMEL_US_ENDRX | ATMEL_US_TIMEOUT);
}

static void atmel_tasklet_func(unsigned long data)
{
	struct uart_port *port = (struct uart_port *)data;
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	unsigned int status;
	unsigned int status_change;

	
	spin_lock(&port->lock);

	if (atmel_use_dma_tx(port))
		atmel_tx_dma(port);
	else
		atmel_tx_chars(port);

	status = atmel_port->irq_status;
	status_change = status ^ atmel_port->irq_status_prev;

	if (status_change & (ATMEL_US_RI | ATMEL_US_DSR
				| ATMEL_US_DCD | ATMEL_US_CTS)) {
		
		if (status_change & ATMEL_US_RI)
			port->icount.rng++;
		if (status_change & ATMEL_US_DSR)
			port->icount.dsr++;
		if (status_change & ATMEL_US_DCD)
			uart_handle_dcd_change(port, !(status & ATMEL_US_DCD));
		if (status_change & ATMEL_US_CTS)
			uart_handle_cts_change(port, !(status & ATMEL_US_CTS));

		wake_up_interruptible(&port->state->port.delta_msr_wait);

		atmel_port->irq_status_prev = status;
	}

	if (atmel_use_dma_rx(port))
		atmel_rx_from_dma(port);
	else
		atmel_rx_from_ring(port);

	spin_unlock(&port->lock);
}

static int atmel_startup(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	struct tty_struct *tty = port->state->port.tty;
	int retval;

	UART_PUT_IDR(port, -1);

	retval = request_irq(port->irq, atmel_interrupt, IRQF_SHARED,
			tty ? tty->name : "atmel_serial", port);
	if (retval) {
		printk("atmel_serial: atmel_startup - Can't get irq\n");
		return retval;
	}

	if (atmel_use_dma_rx(port)) {
		int i;

		for (i = 0; i < 2; i++) {
			struct atmel_dma_buffer *pdc = &atmel_port->pdc_rx[i];

			pdc->buf = kmalloc(PDC_BUFFER_SIZE, GFP_KERNEL);
			if (pdc->buf == NULL) {
				if (i != 0) {
					dma_unmap_single(port->dev,
						atmel_port->pdc_rx[0].dma_addr,
						PDC_BUFFER_SIZE,
						DMA_FROM_DEVICE);
					kfree(atmel_port->pdc_rx[0].buf);
				}
				free_irq(port->irq, port);
				return -ENOMEM;
			}
			pdc->dma_addr = dma_map_single(port->dev,
						       pdc->buf,
						       PDC_BUFFER_SIZE,
						       DMA_FROM_DEVICE);
			pdc->dma_size = PDC_BUFFER_SIZE;
			pdc->ofs = 0;
		}

		atmel_port->pdc_rx_idx = 0;

		UART_PUT_RPR(port, atmel_port->pdc_rx[0].dma_addr);
		UART_PUT_RCR(port, PDC_BUFFER_SIZE);

		UART_PUT_RNPR(port, atmel_port->pdc_rx[1].dma_addr);
		UART_PUT_RNCR(port, PDC_BUFFER_SIZE);
	}
	if (atmel_use_dma_tx(port)) {
		struct atmel_dma_buffer *pdc = &atmel_port->pdc_tx;
		struct circ_buf *xmit = &port->state->xmit;

		pdc->buf = xmit->buf;
		pdc->dma_addr = dma_map_single(port->dev,
					       pdc->buf,
					       UART_XMIT_SIZE,
					       DMA_TO_DEVICE);
		pdc->dma_size = UART_XMIT_SIZE;
		pdc->ofs = 0;
	}

	if (atmel_open_hook) {
		retval = atmel_open_hook(port);
		if (retval) {
			free_irq(port->irq, port);
			return retval;
		}
	}

	
	atmel_port->irq_status_prev = UART_GET_CSR(port);
	atmel_port->irq_status = atmel_port->irq_status_prev;

	UART_PUT_CR(port, ATMEL_US_RSTSTA | ATMEL_US_RSTRX);
	
	UART_PUT_CR(port, ATMEL_US_TXEN | ATMEL_US_RXEN);

	if (atmel_use_dma_rx(port)) {
		
		UART_PUT_RTOR(port, PDC_RX_TIMEOUT);
		UART_PUT_CR(port, ATMEL_US_STTTO);

		UART_PUT_IER(port, ATMEL_US_ENDRX | ATMEL_US_TIMEOUT);
		
		UART_PUT_PTCR(port, ATMEL_PDC_RXTEN);
	} else {
		
		UART_PUT_IER(port, ATMEL_US_RXRDY);
	}

	return 0;
}

static void atmel_shutdown(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	atmel_stop_rx(port);
	atmel_stop_tx(port);

	if (atmel_use_dma_rx(port)) {
		int i;

		for (i = 0; i < 2; i++) {
			struct atmel_dma_buffer *pdc = &atmel_port->pdc_rx[i];

			dma_unmap_single(port->dev,
					 pdc->dma_addr,
					 pdc->dma_size,
					 DMA_FROM_DEVICE);
			kfree(pdc->buf);
		}
	}
	if (atmel_use_dma_tx(port)) {
		struct atmel_dma_buffer *pdc = &atmel_port->pdc_tx;

		dma_unmap_single(port->dev,
				 pdc->dma_addr,
				 pdc->dma_size,
				 DMA_TO_DEVICE);
	}

	UART_PUT_CR(port, ATMEL_US_RSTSTA);
	UART_PUT_IDR(port, -1);

	free_irq(port->irq, port);

	if (atmel_close_hook)
		atmel_close_hook(port);
}

static void atmel_flush_buffer(struct uart_port *port)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (atmel_use_dma_tx(port)) {
		UART_PUT_TCR(port, 0);
		atmel_port->pdc_tx.ofs = 0;
	}
}

static void atmel_serial_pm(struct uart_port *port, unsigned int state,
			    unsigned int oldstate)
{
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	switch (state) {
	case 0:
		clk_enable(atmel_port->clk);

		
		UART_PUT_IER(port, atmel_port->backup_imr);
		break;
	case 3:
		
		atmel_port->backup_imr = UART_GET_IMR(port);
		UART_PUT_IDR(port, -1);

		clk_disable(atmel_port->clk);
		break;
	default:
		printk(KERN_ERR "atmel_serial: unknown pm %d\n", state);
	}
}

static void atmel_set_termios(struct uart_port *port, struct ktermios *termios,
			      struct ktermios *old)
{
	unsigned long flags;
	unsigned int mode, imr, quot, baud;
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	
	mode = UART_GET_MR(port) & ~(ATMEL_US_USCLKS | ATMEL_US_CHRL
					| ATMEL_US_NBSTOP | ATMEL_US_PAR
					| ATMEL_US_USMODE);

	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk / 16);
	quot = uart_get_divisor(port, baud);

	if (quot > 65535) {	
		quot /= 8;
		mode |= ATMEL_US_USCLKS_MCK_DIV8;
	}

	
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		mode |= ATMEL_US_CHRL_5;
		break;
	case CS6:
		mode |= ATMEL_US_CHRL_6;
		break;
	case CS7:
		mode |= ATMEL_US_CHRL_7;
		break;
	default:
		mode |= ATMEL_US_CHRL_8;
		break;
	}

	
	if (termios->c_cflag & CSTOPB)
		mode |= ATMEL_US_NBSTOP_2;

	
	if (termios->c_cflag & PARENB) {
		
		if (termios->c_cflag & CMSPAR) {
			if (termios->c_cflag & PARODD)
				mode |= ATMEL_US_PAR_MARK;
			else
				mode |= ATMEL_US_PAR_SPACE;
		} else if (termios->c_cflag & PARODD)
			mode |= ATMEL_US_PAR_ODD;
		else
			mode |= ATMEL_US_PAR_EVEN;
	} else
		mode |= ATMEL_US_PAR_NONE;

	
	if (termios->c_cflag & CRTSCTS)
		mode |= ATMEL_US_USMODE_HWHS;
	else
		mode |= ATMEL_US_USMODE_NORMAL;

	spin_lock_irqsave(&port->lock, flags);

	port->read_status_mask = ATMEL_US_OVRE;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= (ATMEL_US_FRAME | ATMEL_US_PARE);
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= ATMEL_US_RXBRK;

	if (atmel_use_dma_rx(port))
		
		UART_PUT_IER(port, port->read_status_mask);

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= (ATMEL_US_FRAME | ATMEL_US_PARE);
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= ATMEL_US_RXBRK;
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= ATMEL_US_OVRE;
	}
	

	
	uart_update_timeout(port, termios->c_cflag, baud);

	imr = UART_GET_IMR(port);
	UART_PUT_IDR(port, -1);

	
	UART_PUT_CR(port, ATMEL_US_TXDIS | ATMEL_US_RXDIS);

	
	mode &= ~ATMEL_US_USMODE;

	if (atmel_port->rs485.flags & SER_RS485_ENABLED) {
		dev_dbg(port->dev, "Setting UART to RS485\n");
		if ((atmel_port->rs485.delay_rts_after_send) > 0)
			UART_PUT_TTGR(port,
					atmel_port->rs485.delay_rts_after_send);
		mode |= ATMEL_US_USMODE_RS485;
	} else {
		dev_dbg(port->dev, "Setting UART to RS232\n");
	}

	
	UART_PUT_MR(port, mode);

	
	UART_PUT_BRGR(port, quot);
	UART_PUT_CR(port, ATMEL_US_RSTSTA | ATMEL_US_RSTRX);
	UART_PUT_CR(port, ATMEL_US_TXEN | ATMEL_US_RXEN);

	
	UART_PUT_IER(port, imr);

	
	if (UART_ENABLE_MS(port, termios->c_cflag))
		port->ops->enable_ms(port);

	spin_unlock_irqrestore(&port->lock, flags);
}

static void atmel_set_ldisc(struct uart_port *port, int new)
{
	if (new == N_PPS) {
		port->flags |= UPF_HARDPPS_CD;
		atmel_enable_ms(port);
	} else {
		port->flags &= ~UPF_HARDPPS_CD;
	}
}

static const char *atmel_type(struct uart_port *port)
{
	return (port->type == PORT_ATMEL) ? "ATMEL_SERIAL" : NULL;
}

static void atmel_release_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	release_mem_region(port->mapbase, size);

	if (port->flags & UPF_IOREMAP) {
		iounmap(port->membase);
		port->membase = NULL;
	}
}

static int atmel_request_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(port->mapbase, size, "atmel_serial"))
		return -EBUSY;

	if (port->flags & UPF_IOREMAP) {
		port->membase = ioremap(port->mapbase, size);
		if (port->membase == NULL) {
			release_mem_region(port->mapbase, size);
			return -ENOMEM;
		}
	}

	return 0;
}

static void atmel_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_ATMEL;
		atmel_request_port(port);
	}
}

static int atmel_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_ATMEL)
		ret = -EINVAL;
	if (port->irq != ser->irq)
		ret = -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		ret = -EINVAL;
	if (port->uartclk / 16 != ser->baud_base)
		ret = -EINVAL;
	if ((void *)port->mapbase != ser->iomem_base)
		ret = -EINVAL;
	if (port->iobase != ser->port)
		ret = -EINVAL;
	if (ser->hub6 != 0)
		ret = -EINVAL;
	return ret;
}

#ifdef CONFIG_CONSOLE_POLL
static int atmel_poll_get_char(struct uart_port *port)
{
	while (!(UART_GET_CSR(port) & ATMEL_US_RXRDY))
		cpu_relax();

	return UART_GET_CHAR(port);
}

static void atmel_poll_put_char(struct uart_port *port, unsigned char ch)
{
	while (!(UART_GET_CSR(port) & ATMEL_US_TXRDY))
		cpu_relax();

	UART_PUT_CHAR(port, ch);
}
#endif

static int
atmel_ioctl(struct uart_port *port, unsigned int cmd, unsigned long arg)
{
	struct serial_rs485 rs485conf;

	switch (cmd) {
	case TIOCSRS485:
		if (copy_from_user(&rs485conf, (struct serial_rs485 *) arg,
					sizeof(rs485conf)))
			return -EFAULT;

		atmel_config_rs485(port, &rs485conf);
		break;

	case TIOCGRS485:
		if (copy_to_user((struct serial_rs485 *) arg,
					&(to_atmel_uart_port(port)->rs485),
					sizeof(rs485conf)))
			return -EFAULT;
		break;

	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}



static struct uart_ops atmel_pops = {
	.tx_empty	= atmel_tx_empty,
	.set_mctrl	= atmel_set_mctrl,
	.get_mctrl	= atmel_get_mctrl,
	.stop_tx	= atmel_stop_tx,
	.start_tx	= atmel_start_tx,
	.stop_rx	= atmel_stop_rx,
	.enable_ms	= atmel_enable_ms,
	.break_ctl	= atmel_break_ctl,
	.startup	= atmel_startup,
	.shutdown	= atmel_shutdown,
	.flush_buffer	= atmel_flush_buffer,
	.set_termios	= atmel_set_termios,
	.set_ldisc	= atmel_set_ldisc,
	.type		= atmel_type,
	.release_port	= atmel_release_port,
	.request_port	= atmel_request_port,
	.config_port	= atmel_config_port,
	.verify_port	= atmel_verify_port,
	.pm		= atmel_serial_pm,
	.ioctl		= atmel_ioctl,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char	= atmel_poll_get_char,
	.poll_put_char	= atmel_poll_put_char,
#endif
};

static void __devinit atmel_of_init_port(struct atmel_uart_port *atmel_port,
					 struct device_node *np)
{
	u32 rs485_delay[2];

	
	if (of_get_property(np, "atmel,use-dma-rx", NULL))
		atmel_port->use_dma_rx	= 1;
	else
		atmel_port->use_dma_rx	= 0;
	if (of_get_property(np, "atmel,use-dma-tx", NULL))
		atmel_port->use_dma_tx	= 1;
	else
		atmel_port->use_dma_tx	= 0;

	
	if (of_property_read_u32_array(np, "rs485-rts-delay",
					    rs485_delay, 2) == 0) {
		struct serial_rs485 *rs485conf = &atmel_port->rs485;

		rs485conf->delay_rts_before_send = rs485_delay[0];
		rs485conf->delay_rts_after_send = rs485_delay[1];
		rs485conf->flags = 0;

		if (of_get_property(np, "rs485-rx-during-tx", NULL))
			rs485conf->flags |= SER_RS485_RX_DURING_TX;

		if (of_get_property(np, "linux,rs485-enabled-at-boot-time", NULL))
			rs485conf->flags |= SER_RS485_ENABLED;
	}
}

static void __devinit atmel_init_port(struct atmel_uart_port *atmel_port,
				      struct platform_device *pdev)
{
	struct uart_port *port = &atmel_port->uart;
	struct atmel_uart_data *pdata = pdev->dev.platform_data;

	if (pdev->dev.of_node) {
		atmel_of_init_port(atmel_port, pdev->dev.of_node);
	} else {
		atmel_port->use_dma_rx	= pdata->use_dma_rx;
		atmel_port->use_dma_tx	= pdata->use_dma_tx;
		atmel_port->rs485	= pdata->rs485;
	}

	port->iotype		= UPIO_MEM;
	port->flags		= UPF_BOOT_AUTOCONF;
	port->ops		= &atmel_pops;
	port->fifosize		= 1;
	port->dev		= &pdev->dev;
	port->mapbase	= pdev->resource[0].start;
	port->irq	= pdev->resource[1].start;

	tasklet_init(&atmel_port->tasklet, atmel_tasklet_func,
			(unsigned long)port);

	memset(&atmel_port->rx_ring, 0, sizeof(atmel_port->rx_ring));

	if (pdata && pdata->regs) {
		
		port->membase = pdata->regs;
	} else {
		port->flags	|= UPF_IOREMAP;
		port->membase	= NULL;
	}

	
	if (!atmel_port->clk) {
		atmel_port->clk = clk_get(&pdev->dev, "usart");
		clk_enable(atmel_port->clk);
		port->uartclk = clk_get_rate(atmel_port->clk);
		clk_disable(atmel_port->clk);
		
	}

	
	if (atmel_port->rs485.flags & SER_RS485_ENABLED)
		atmel_port->tx_done_mask = ATMEL_US_TXEMPTY;
	else if (atmel_use_dma_tx(port)) {
		port->fifosize = PDC_BUFFER_SIZE;
		atmel_port->tx_done_mask = ATMEL_US_ENDTX | ATMEL_US_TXBUFE;
	} else {
		atmel_port->tx_done_mask = ATMEL_US_TXRDY;
	}
}

void __init atmel_register_uart_fns(struct atmel_port_fns *fns)
{
	if (fns->enable_ms)
		atmel_pops.enable_ms = fns->enable_ms;
	if (fns->get_mctrl)
		atmel_pops.get_mctrl = fns->get_mctrl;
	if (fns->set_mctrl)
		atmel_pops.set_mctrl = fns->set_mctrl;
	atmel_open_hook		= fns->open;
	atmel_close_hook	= fns->close;
	atmel_pops.pm		= fns->pm;
	atmel_pops.set_wake	= fns->set_wake;
}

struct platform_device *atmel_default_console_device;	

#ifdef CONFIG_SERIAL_ATMEL_CONSOLE
static void atmel_console_putchar(struct uart_port *port, int ch)
{
	while (!(UART_GET_CSR(port) & ATMEL_US_TXRDY))
		cpu_relax();
	UART_PUT_CHAR(port, ch);
}

static void atmel_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = &atmel_ports[co->index].uart;
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	unsigned int status, imr;
	unsigned int pdc_tx;

	imr = UART_GET_IMR(port);
	UART_PUT_IDR(port, ATMEL_US_RXRDY | atmel_port->tx_done_mask);

	
	pdc_tx = UART_GET_PTSR(port) & ATMEL_PDC_TXTEN;
	UART_PUT_PTCR(port, ATMEL_PDC_TXTDIS);

	uart_console_write(port, s, count, atmel_console_putchar);

	do {
		status = UART_GET_CSR(port);
	} while (!(status & ATMEL_US_TXRDY));

	
	if (pdc_tx)
		UART_PUT_PTCR(port, ATMEL_PDC_TXTEN);

	
	UART_PUT_IER(port, imr);
}

static void __init atmel_console_get_options(struct uart_port *port, int *baud,
					     int *parity, int *bits)
{
	unsigned int mr, quot;

	quot = UART_GET_BRGR(port) & ATMEL_US_CD;
	if (!quot)
		return;

	mr = UART_GET_MR(port) & ATMEL_US_CHRL;
	if (mr == ATMEL_US_CHRL_8)
		*bits = 8;
	else
		*bits = 7;

	mr = UART_GET_MR(port) & ATMEL_US_PAR;
	if (mr == ATMEL_US_PAR_EVEN)
		*parity = 'e';
	else if (mr == ATMEL_US_PAR_ODD)
		*parity = 'o';

	*baud = port->uartclk / (16 * (quot - 1));
}

static int __init atmel_console_setup(struct console *co, char *options)
{
	struct uart_port *port = &atmel_ports[co->index].uart;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (port->membase == NULL) {
		
		return -ENODEV;
	}

	clk_enable(atmel_ports[co->index].clk);

	UART_PUT_IDR(port, -1);
	UART_PUT_CR(port, ATMEL_US_RSTSTA | ATMEL_US_RSTRX);
	UART_PUT_CR(port, ATMEL_US_TXEN | ATMEL_US_RXEN);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		atmel_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver atmel_uart;

static struct console atmel_console = {
	.name		= ATMEL_DEVICENAME,
	.write		= atmel_console_write,
	.device		= uart_console_device,
	.setup		= atmel_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &atmel_uart,
};

#define ATMEL_CONSOLE_DEVICE	(&atmel_console)

static int __init atmel_console_init(void)
{
	if (atmel_default_console_device) {
		struct atmel_uart_data *pdata =
			atmel_default_console_device->dev.platform_data;
		int id = pdata->num;
		struct atmel_uart_port *port = &atmel_ports[id];

		port->backup_imr = 0;
		port->uart.line = id;

		add_preferred_console(ATMEL_DEVICENAME, id, NULL);
		atmel_init_port(port, atmel_default_console_device);
		register_console(&atmel_console);
	}

	return 0;
}

console_initcall(atmel_console_init);

static int __init atmel_late_console_init(void)
{
	if (atmel_default_console_device
	    && !(atmel_console.flags & CON_ENABLED))
		register_console(&atmel_console);

	return 0;
}

core_initcall(atmel_late_console_init);

static inline bool atmel_is_console_port(struct uart_port *port)
{
	return port->cons && port->cons->index == port->line;
}

#else
#define ATMEL_CONSOLE_DEVICE	NULL

static inline bool atmel_is_console_port(struct uart_port *port)
{
	return false;
}
#endif

static struct uart_driver atmel_uart = {
	.owner		= THIS_MODULE,
	.driver_name	= "atmel_serial",
	.dev_name	= ATMEL_DEVICENAME,
	.major		= SERIAL_ATMEL_MAJOR,
	.minor		= MINOR_START,
	.nr		= ATMEL_MAX_UART,
	.cons		= ATMEL_CONSOLE_DEVICE,
};

#ifdef CONFIG_PM
static bool atmel_serial_clk_will_stop(void)
{
#ifdef CONFIG_ARCH_AT91
	return at91_suspend_entering_slow_clock();
#else
	return false;
#endif
}

static int atmel_serial_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	if (atmel_is_console_port(port) && console_suspend_enabled) {
		
		while (!(UART_GET_CSR(port) & ATMEL_US_TXEMPTY))
			cpu_relax();
	}

	
	atmel_port->may_wakeup = device_may_wakeup(&pdev->dev);
	if (atmel_serial_clk_will_stop())
		device_set_wakeup_enable(&pdev->dev, 0);

	uart_suspend_port(&atmel_uart, port);

	return 0;
}

static int atmel_serial_resume(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);

	uart_resume_port(&atmel_uart, port);
	device_set_wakeup_enable(&pdev->dev, atmel_port->may_wakeup);

	return 0;
}
#else
#define atmel_serial_suspend NULL
#define atmel_serial_resume NULL
#endif

static int __devinit atmel_serial_probe(struct platform_device *pdev)
{
	struct atmel_uart_port *port;
	struct device_node *np = pdev->dev.of_node;
	struct atmel_uart_data *pdata = pdev->dev.platform_data;
	void *data;
	int ret = -ENODEV;

	BUILD_BUG_ON(ATMEL_SERIAL_RINGSIZE & (ATMEL_SERIAL_RINGSIZE - 1));

	if (np)
		ret = of_alias_get_id(np, "serial");
	else
		if (pdata)
			ret = pdata->num;

	if (ret < 0)
		ret = find_first_zero_bit(&atmel_ports_in_use,
				sizeof(atmel_ports_in_use));

	if (ret > ATMEL_MAX_UART) {
		ret = -ENODEV;
		goto err;
	}

	if (test_and_set_bit(ret, &atmel_ports_in_use)) {
		
		ret = -EBUSY;
		goto err;
	}

	port = &atmel_ports[ret];
	port->backup_imr = 0;
	port->uart.line = ret;

	atmel_init_port(port, pdev);

	if (!atmel_use_dma_rx(&port->uart)) {
		ret = -ENOMEM;
		data = kmalloc(sizeof(struct atmel_uart_char)
				* ATMEL_SERIAL_RINGSIZE, GFP_KERNEL);
		if (!data)
			goto err_alloc_ring;
		port->rx_ring.buf = data;
	}

	ret = uart_add_one_port(&atmel_uart, &port->uart);
	if (ret)
		goto err_add_port;

#ifdef CONFIG_SERIAL_ATMEL_CONSOLE
	if (atmel_is_console_port(&port->uart)
			&& ATMEL_CONSOLE_DEVICE->flags & CON_ENABLED) {
		clk_disable(port->clk);
	}
#endif

	device_init_wakeup(&pdev->dev, 1);
	platform_set_drvdata(pdev, port);

	if (port->rs485.flags & SER_RS485_ENABLED) {
		UART_PUT_MR(&port->uart, ATMEL_US_USMODE_NORMAL);
		UART_PUT_CR(&port->uart, ATMEL_US_RTSEN);
	}

	return 0;

err_add_port:
	kfree(port->rx_ring.buf);
	port->rx_ring.buf = NULL;
err_alloc_ring:
	if (!atmel_is_console_port(&port->uart)) {
		clk_put(port->clk);
		port->clk = NULL;
	}
err:
	return ret;
}

static int __devexit atmel_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);
	struct atmel_uart_port *atmel_port = to_atmel_uart_port(port);
	int ret = 0;

	device_init_wakeup(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);

	ret = uart_remove_one_port(&atmel_uart, port);

	tasklet_kill(&atmel_port->tasklet);
	kfree(atmel_port->rx_ring.buf);

	

	clear_bit(port->line, &atmel_ports_in_use);

	clk_put(atmel_port->clk);

	return ret;
}

static struct platform_driver atmel_serial_driver = {
	.probe		= atmel_serial_probe,
	.remove		= __devexit_p(atmel_serial_remove),
	.suspend	= atmel_serial_suspend,
	.resume		= atmel_serial_resume,
	.driver		= {
		.name	= "atmel_usart",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(atmel_serial_dt_ids),
	},
};

static int __init atmel_serial_init(void)
{
	int ret;

	ret = uart_register_driver(&atmel_uart);
	if (ret)
		return ret;

	ret = platform_driver_register(&atmel_serial_driver);
	if (ret)
		uart_unregister_driver(&atmel_uart);

	return ret;
}

static void __exit atmel_serial_exit(void)
{
	platform_driver_unregister(&atmel_serial_driver);
	uart_unregister_driver(&atmel_uart);
}

module_init(atmel_serial_init);
module_exit(atmel_serial_exit);

MODULE_AUTHOR("Rick Bronson");
MODULE_DESCRIPTION("Atmel AT91 / AT32 serial port driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:atmel_usart");
