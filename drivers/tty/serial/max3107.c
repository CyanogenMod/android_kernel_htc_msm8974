/*
 *  max3107.c - spi uart protocol driver for Maxim 3107
 *  Based on max3100.c
 *	by Christian Pellegrin <chripell@evolware.org>
 *  and	max3110.c
 *	by Feng Tang <feng.tang@intel.com>
 *
 *  Copyright (C) Aavamobile 2009
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include "max3107.h"

static const struct baud_table brg26_ext[] = {
	{ 300,    MAX3107_BRG26_B300 },
	{ 600,    MAX3107_BRG26_B600 },
	{ 1200,   MAX3107_BRG26_B1200 },
	{ 2400,   MAX3107_BRG26_B2400 },
	{ 4800,   MAX3107_BRG26_B4800 },
	{ 9600,   MAX3107_BRG26_B9600 },
	{ 19200,  MAX3107_BRG26_B19200 },
	{ 57600,  MAX3107_BRG26_B57600 },
	{ 115200, MAX3107_BRG26_B115200 },
	{ 230400, MAX3107_BRG26_B230400 },
	{ 460800, MAX3107_BRG26_B460800 },
	{ 921600, MAX3107_BRG26_B921600 },
	{ 0, 0 }
};

static const struct baud_table brg13_int[] = {
	{ 300,    MAX3107_BRG13_IB300 },
	{ 600,    MAX3107_BRG13_IB600 },
	{ 1200,   MAX3107_BRG13_IB1200 },
	{ 2400,   MAX3107_BRG13_IB2400 },
	{ 4800,   MAX3107_BRG13_IB4800 },
	{ 9600,   MAX3107_BRG13_IB9600 },
	{ 19200,  MAX3107_BRG13_IB19200 },
	{ 57600,  MAX3107_BRG13_IB57600 },
	{ 115200, MAX3107_BRG13_IB115200 },
	{ 230400, MAX3107_BRG13_IB230400 },
	{ 460800, MAX3107_BRG13_IB460800 },
	{ 921600, MAX3107_BRG13_IB921600 },
	{ 0, 0 }
};

static u32 get_new_brg(int baud, struct max3107_port *s)
{
	int i;
	const struct baud_table *baud_tbl = s->baud_tbl;

	for (i = 0; i < 13; i++) {
		if (baud == baud_tbl[i].baud)
			return baud_tbl[i].new_brg;
	}

	return 0;
}

int max3107_rw(struct max3107_port *s, u8 *tx, u8 *rx, int len)
{
	struct spi_message spi_msg;
	struct spi_transfer spi_xfer;

	
	spi_message_init(&spi_msg);

	
	memset(&spi_xfer, 0, sizeof spi_xfer);
	spi_xfer.len = len;
	spi_xfer.tx_buf = tx;
	spi_xfer.rx_buf = rx;
	spi_xfer.speed_hz = MAX3107_SPI_SPEED;

	
	spi_message_add_tail(&spi_xfer, &spi_msg);

#ifdef DBG_TRACE_SPI_DATA
	{
		int i;
		pr_info("tx len %d:\n", spi_xfer.len);
		for (i = 0 ; i < spi_xfer.len && i < 32 ; i++)
			pr_info(" %x", ((u8 *)spi_xfer.tx_buf)[i]);
		pr_info("\n");
	}
#endif

	
	if (spi_sync(s->spi, &spi_msg)) {
		dev_err(&s->spi->dev, "spi_sync failure\n");
		return -EIO;
	}

#ifdef DBG_TRACE_SPI_DATA
	if (spi_xfer.rx_buf) {
		int i;
		pr_info("rx len %d:\n", spi_xfer.len);
		for (i = 0 ; i < spi_xfer.len && i < 32 ; i++)
			pr_info(" %x", ((u8 *)spi_xfer.rx_buf)[i]);
		pr_info("\n");
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(max3107_rw);

static void put_data_to_circ_buf(struct max3107_port *s, unsigned char *data,
					int len)
{
	struct uart_port *port = &s->port;
	struct tty_struct *tty;

	if (!port->state)
		return;

	tty = port->state->port.tty;
	if (!tty)
		return;

	
	tty_insert_flip_string(tty, data, len);
	
	port->icount.rx += len;
}

static void max3107_handlerx(struct max3107_port *s, u16 rxlvl)
{
	int i;
	int j;
	int len;				
	u16 *buf;
	u8 *valid_str;

	if (!s->rx_enabled)
		
		return;

	if (rxlvl == 0) {
		
		return;
	} else if (rxlvl >= MAX3107_RX_FIFO_SIZE) {
		dev_warn(&s->spi->dev, "Possible RX FIFO overrun %d\n", rxlvl);
		
		rxlvl = MAX3107_RX_FIFO_SIZE;
	}
	if ((s->rxbuf == 0) || (s->rxstr == 0)) {
		dev_warn(&s->spi->dev, "Rx buffer/str isn't ready\n");
		return;
	}
	buf = s->rxbuf;
	valid_str = s->rxstr;
	while (rxlvl) {
		pr_debug("rxlvl %d\n", rxlvl);
		
		memset(buf, 0, sizeof(u16) * (MAX3107_RX_FIFO_SIZE + 2));
		len = 0;
		if (s->irqen_reg & MAX3107_IRQ_RXFIFO_BIT) {
			
			pr_debug("Disabling RX INT\n");
			buf[0] = (MAX3107_WRITE_BIT | MAX3107_IRQEN_REG);
			s->irqen_reg &= ~MAX3107_IRQ_RXFIFO_BIT;
			buf[0] |= s->irqen_reg;
			len++;
		}
		len += rxlvl;
		
		buf[len] = MAX3107_RXFIFOLVL_REG;
		len++;

		
		if (max3107_rw(s, (u8 *)buf, (u8 *)buf, len * 2)) {
			dev_err(&s->spi->dev, "SPI transfer for RX h failed\n");
			return;
		}

		
		j = ((len - 1) - rxlvl);
		
		for (i = 0; i < rxlvl; i++, j++)
			valid_str[i] = (u8)buf[j];
		put_data_to_circ_buf(s, valid_str, rxlvl);
		
		rxlvl = (buf[len - 1] & MAX3107_SPI_RX_DATA_MASK);
	}

	if (s->rx_enabled) {
		
		pr_debug("Enabling RX INT\n");
		buf[0] = (MAX3107_WRITE_BIT | MAX3107_IRQEN_REG);
		s->irqen_reg |= MAX3107_IRQ_RXFIFO_BIT;
		buf[0] |= s->irqen_reg;
		if (max3107_rw(s, (u8 *)buf, NULL, 2))
			dev_err(&s->spi->dev, "RX FIFO INT enabling failed\n");
	}

	
	if (s->port.state->port.tty)
		tty_flip_buffer_push(s->port.state->port.tty);
}


static void max3107_handletx(struct max3107_port *s)
{
	struct circ_buf *xmit = &s->port.state->xmit;
	int i;
	unsigned long flags;
	int len;				
	u16 *buf;

	if (!s->tx_fifo_empty)
		
		return;

	if (uart_circ_empty(xmit) || uart_tx_stopped(&s->port))
		
		return;

	if (!s->txbuf) {
		dev_warn(&s->spi->dev, "Txbuf isn't ready\n");
		return;
	}
	buf = s->txbuf;
	
	len = uart_circ_chars_pending(xmit);
	if (len) {
		
		if (len > MAX3107_TX_FIFO_SIZE)
			len = MAX3107_TX_FIFO_SIZE;

		pr_debug("txlen %d\n", len);

		
		s->port.icount.tx += len;

		
		s->tx_fifo_empty = 0;

		i = 0;
		if (s->irqen_reg & MAX3107_IRQ_TXEMPTY_BIT) {
			
			pr_debug("Disabling TE INT\n");
			buf[i] = (MAX3107_WRITE_BIT | MAX3107_IRQEN_REG);
			s->irqen_reg &= ~MAX3107_IRQ_TXEMPTY_BIT;
			buf[i] |= s->irqen_reg;
			i++;
			len++;
		}
		
		spin_lock_irqsave(&s->port.lock, flags);
		for ( ; i < len ; i++) {
			buf[i] = (MAX3107_WRITE_BIT | MAX3107_THR_REG);
			buf[i] |= ((u16)xmit->buf[xmit->tail] &
						MAX3107_SPI_TX_DATA_MASK);
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		}
		spin_unlock_irqrestore(&s->port.lock, flags);
		if (!(s->irqen_reg & MAX3107_IRQ_TXEMPTY_BIT)) {
			
			pr_debug("Enabling TE INT\n");
			buf[i] = (MAX3107_WRITE_BIT | MAX3107_IRQEN_REG);
			s->irqen_reg |= MAX3107_IRQ_TXEMPTY_BIT;
			buf[i] |= s->irqen_reg;
			i++;
			len++;
		}
		if (!s->tx_enabled) {
			
			pr_debug("Enable TX\n");
			buf[i] = (MAX3107_WRITE_BIT | MAX3107_MODE1_REG);
			spin_lock_irqsave(&s->data_lock, flags);
			s->mode1_reg &= ~MAX3107_MODE1_TXDIS_BIT;
			buf[i] |= s->mode1_reg;
			spin_unlock_irqrestore(&s->data_lock, flags);
			s->tx_enabled = 1;
			i++;
			len++;
		}

		
		if (max3107_rw(s, (u8 *)buf, NULL, len*2)) {
			dev_err(&s->spi->dev,
				"SPI transfer TX handling failed\n");
			return;
		}
	}

	
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&s->port);

}

static u16 handle_interrupt(struct max3107_port *s)
{
	u16 buf[4];	
	u8 irq_status;
	u16 rx_level;
	unsigned long flags;

	
	buf[0] = MAX3107_IRQSTS_REG;
	
	buf[1] = MAX3107_STS_IRQSTS_REG;
	
	buf[2] = MAX3107_LSR_IRQSTS_REG;
	
	buf[3] = MAX3107_RXFIFOLVL_REG;

	if (max3107_rw(s, (u8 *)buf, (u8 *)buf, 8)) {
		dev_err(&s->spi->dev,
			"SPI transfer for INTR handling failed\n");
		return 0;
	}

	irq_status = (u8)buf[0];
	pr_debug("IRQSTS %x\n", irq_status);
	rx_level = (buf[3] & MAX3107_SPI_RX_DATA_MASK);

	if (irq_status & MAX3107_IRQ_LSR_BIT) {
		
		if (buf[2] & MAX3107_LSR_RXTO_BIT)
			pr_debug("RX TO INT\n");
	}

	if (irq_status & MAX3107_IRQ_TXEMPTY_BIT) {
		pr_debug("TE INT, disabling TX\n");
		buf[0] = (MAX3107_WRITE_BIT | MAX3107_MODE1_REG);
		spin_lock_irqsave(&s->data_lock, flags);
		s->mode1_reg |= MAX3107_MODE1_TXDIS_BIT;
		buf[0] |= s->mode1_reg;
		spin_unlock_irqrestore(&s->data_lock, flags);
		if (max3107_rw(s, (u8 *)buf, NULL, 2))
			dev_err(&s->spi->dev, "SPI transfer TX dis failed\n");
		s->tx_enabled = 0;
		s->tx_fifo_empty = 1;
	}

	if (irq_status & MAX3107_IRQ_RXFIFO_BIT)
		pr_debug("RFIFO INT\n");

	
	return rx_level;
}

static void max3107_dowork(struct max3107_port *s)
{
	if (!work_pending(&s->work) && !freezing(current) && !s->suspended)
		queue_work(s->workqueue, &s->work);
	else
		dev_warn(&s->spi->dev, "interrup isn't serviced normally!\n");
}

static void max3107_work(struct work_struct *w)
{
	struct max3107_port *s = container_of(w, struct max3107_port, work);
	u16 rxlvl = 0;
	int len;	
	u16 buf[5];	
	unsigned long flags;

	
	buf[0] = MAX3107_RXFIFOLVL_REG;
	if (max3107_rw(s, (u8 *)buf, (u8 *)buf, 2)) {
		dev_err(&s->spi->dev, "SPI transfer RX lev failed\n");
		rxlvl = 0;
	} else {
		rxlvl = (buf[0] & MAX3107_SPI_RX_DATA_MASK);
	}

	do {
		pr_debug("rxlvl %d\n", rxlvl);

		
		max3107_handlerx(s, rxlvl);
		rxlvl = 0;

		if (s->handle_irq) {
			s->handle_irq = 0;
			rxlvl = handle_interrupt(s);
		}

		
		max3107_handletx(s);

		
		len = 0;
		spin_lock_irqsave(&s->data_lock, flags);
		if (s->mode1_commit) {
			pr_debug("mode1_commit\n");
			buf[len] = (MAX3107_WRITE_BIT | MAX3107_MODE1_REG);
			buf[len++] |= s->mode1_reg;
			s->mode1_commit = 0;
		}
		if (s->lcr_commit) {
			pr_debug("lcr_commit\n");
			buf[len] = (MAX3107_WRITE_BIT | MAX3107_LCR_REG);
			buf[len++] |= s->lcr_reg;
			s->lcr_commit = 0;
		}
		if (s->brg_commit) {
			pr_debug("brg_commit\n");
			buf[len] = (MAX3107_WRITE_BIT | MAX3107_BRGDIVMSB_REG);
			buf[len++] |= ((s->brg_cfg >> 16) &
						MAX3107_SPI_TX_DATA_MASK);
			buf[len] = (MAX3107_WRITE_BIT | MAX3107_BRGDIVLSB_REG);
			buf[len++] |= ((s->brg_cfg >> 8) &
						MAX3107_SPI_TX_DATA_MASK);
			buf[len] = (MAX3107_WRITE_BIT | MAX3107_BRGCFG_REG);
			buf[len++] |= ((s->brg_cfg) & 0xff);
			s->brg_commit = 0;
		}
		spin_unlock_irqrestore(&s->data_lock, flags);

		if (len > 0) {
			if (max3107_rw(s, (u8 *)buf, NULL, len * 2))
				dev_err(&s->spi->dev,
					"SPI transfer config failed\n");
		}

		
	} while (rxlvl);

}

static void max3107_set_sleep(struct max3107_port *s, int mode)
{
	u16 buf[1];	
	unsigned long flags;
	pr_debug("enter, mode %d\n", mode);

	buf[0] = (MAX3107_WRITE_BIT | MAX3107_MODE1_REG);
	spin_lock_irqsave(&s->data_lock, flags);
	switch (mode) {
	case MAX3107_DISABLE_FORCED_SLEEP:
			s->mode1_reg &= ~MAX3107_MODE1_FORCESLEEP_BIT;
			break;
	case MAX3107_ENABLE_FORCED_SLEEP:
			s->mode1_reg |= MAX3107_MODE1_FORCESLEEP_BIT;
			break;
	case MAX3107_DISABLE_AUTOSLEEP:
			s->mode1_reg &= ~MAX3107_MODE1_AUTOSLEEP_BIT;
			break;
	case MAX3107_ENABLE_AUTOSLEEP:
			s->mode1_reg |= MAX3107_MODE1_AUTOSLEEP_BIT;
			break;
	default:
		spin_unlock_irqrestore(&s->data_lock, flags);
		dev_warn(&s->spi->dev, "invalid sleep mode\n");
		return;
	}
	buf[0] |= s->mode1_reg;
	spin_unlock_irqrestore(&s->data_lock, flags);

	if (max3107_rw(s, (u8 *)buf, NULL, 2))
		dev_err(&s->spi->dev, "SPI transfer sleep mode failed\n");

	if (mode == MAX3107_DISABLE_AUTOSLEEP ||
			mode == MAX3107_DISABLE_FORCED_SLEEP)
		msleep(MAX3107_WAKEUP_DELAY);
}

static void max3107_register_init(struct max3107_port *s)
{
	u16 buf[11];	

	
	s->baud = 9600;
	
	if (s->ext_clk) {
		s->brg_cfg = MAX3107_BRG26_B9600;
		s->baud_tbl = (struct baud_table *)brg26_ext;
	} else {
		s->brg_cfg = MAX3107_BRG13_IB9600;
		s->baud_tbl = (struct baud_table *)brg13_int;
	}

	if (s->pdata->init)
		s->pdata->init(s);

	buf[0] = (MAX3107_WRITE_BIT | MAX3107_BRGDIVMSB_REG)
		| ((s->brg_cfg >> 16) & MAX3107_SPI_TX_DATA_MASK);
	buf[1] = (MAX3107_WRITE_BIT | MAX3107_BRGDIVLSB_REG)
		| ((s->brg_cfg >> 8) & MAX3107_SPI_TX_DATA_MASK);
	buf[2] = (MAX3107_WRITE_BIT | MAX3107_BRGCFG_REG)
		| ((s->brg_cfg) & 0xff);

	
	s->lcr_reg = MAX3107_LCR_WORD_LEN_8;
	buf[3] = (MAX3107_WRITE_BIT | MAX3107_LCR_REG)
		| s->lcr_reg;

	
	s->mode1_reg = 0;
	
	s->mode1_reg |= MAX3107_MODE1_IRQSEL_BIT;
	
	s->mode1_reg |= MAX3107_MODE1_TXDIS_BIT;
	s->tx_enabled = 0;
	
	s->rx_enabled = 1;
	buf[4] = (MAX3107_WRITE_BIT | MAX3107_MODE1_REG)
		| s->mode1_reg;

	
	buf[5] = (MAX3107_WRITE_BIT | MAX3107_MODE2_REG);
	if (s->loopback) {
		
		buf[5] |= MAX3107_MODE2_LOOPBACK_BIT;
	}
	
	buf[5] |= MAX3107_MODE2_FIFORST_BIT;
	s->tx_fifo_empty = 1;

	
	buf[6] = (MAX3107_WRITE_BIT | MAX3107_FIFOTRIGLVL_REG);
	
	buf[6] |= (MAX3107_FIFOTRIGLVL_RX(16) | MAX3107_FIFOTRIGLVL_TX(0));

	
	buf[7] = (MAX3107_WRITE_BIT | MAX3107_FLOWLVL_REG);
	
	buf[7] |= (MAX3107_FLOWLVL_RES(48) | MAX3107_FLOWLVL_HALT(96));

	
	buf[8] = (MAX3107_WRITE_BIT | MAX3107_FLOWCTRL_REG);
	
	buf[8] |= (MAX3107_FLOWCTRL_AUTOCTS_BIT | MAX3107_FLOWCTRL_AUTORTS_BIT);

	
	buf[9] = (MAX3107_WRITE_BIT | MAX3107_RXTO_REG);
	
	buf[9] |= 0x0030;

	
	buf[10] = (MAX3107_WRITE_BIT | MAX3107_LSR_IRQEN_REG);
	
	buf[10] |= MAX3107_LSR_RXTO_BIT;

	
	if (max3107_rw(s, (u8 *)buf, NULL, 22))
		dev_err(&s->spi->dev, "SPI transfer for init failed\n");

	
	buf[0] = MAX3107_IRQSTS_REG;

	
	
	s->irqen_reg = MAX3107_IRQ_LSR_BIT;
	
	s->irqen_reg |= MAX3107_IRQ_RXFIFO_BIT;
	buf[1] = (MAX3107_WRITE_BIT | MAX3107_IRQEN_REG)
		| s->irqen_reg;

	
	buf[2] = (MAX3107_WRITE_BIT | MAX3107_MODE2_REG);
	if (s->loopback) {
		
		buf[2] |= MAX3107_MODE2_LOOPBACK_BIT;
	}

	
	if (max3107_rw(s, (u8 *)buf, (u8 *)buf, 6))
		dev_err(&s->spi->dev, "SPI transfer for init failed\n");

}

static irqreturn_t max3107_irq(int irqno, void *dev_id)
{
	struct max3107_port *s = dev_id;

	if (irqno != s->spi->irq) {
		
		return IRQ_NONE;
	}

	
	s->handle_irq = 1;

	
	max3107_dowork(s);

	return IRQ_HANDLED;
}

void max3107_hw_susp(struct max3107_port *s, int suspend)
{
	pr_debug("enter, suspend %d\n", suspend);

	if (suspend) {
		s->suspended = 1;
		max3107_set_sleep(s, MAX3107_ENABLE_AUTOSLEEP);
	} else {
		s->suspended = 0;
		max3107_set_sleep(s, MAX3107_DISABLE_AUTOSLEEP);
	}
}
EXPORT_SYMBOL_GPL(max3107_hw_susp);

static void max3107_enable_ms(struct uart_port *port)
{
	
}

static void max3107_start_tx(struct uart_port *port)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);

	
	max3107_dowork(s);
}

static unsigned int max3107_tx_empty(struct uart_port *port)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);

	pr_debug("returning %d\n",
		  (s->tx_fifo_empty && uart_circ_empty(&s->port.state->xmit)));
	return s->tx_fifo_empty && uart_circ_empty(&s->port.state->xmit);
}

static void max3107_stop_rx(struct uart_port *port)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);
	unsigned long flags;

	
	spin_lock_irqsave(&s->data_lock, flags);
	s->mode1_reg |= MAX3107_MODE1_RXDIS_BIT;
	s->mode1_commit = 1;
	spin_unlock_irqrestore(&s->data_lock, flags);
	
	s->rx_enabled = 0;
	
	max3107_dowork(s);
}

static unsigned int max3107_get_mctrl(struct uart_port *port)
{
	return TIOCM_DSR | TIOCM_CAR;
}

static void max3107_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void max3107_set_termios(struct uart_port *port,
				struct ktermios *termios,
				struct ktermios *old)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);
	struct tty_struct *tty;
	int baud;
	u16 new_lcr = 0;
	u32 new_brg = 0;
	unsigned long flags;

	if (!port->state)
		return;

	tty = port->state->port.tty;
	if (!tty)
		return;

	
	
	if ((termios->c_cflag & CSIZE) == CS7)
		new_lcr |= MAX3107_LCR_WORD_LEN_7;
	else
		new_lcr |= MAX3107_LCR_WORD_LEN_8;

	
	if (termios->c_cflag & PARENB) {
		new_lcr |= MAX3107_LCR_PARITY_BIT;
		if (!(termios->c_cflag & PARODD))
			new_lcr |= MAX3107_LCR_EVENPARITY_BIT;
	}

	
	if (termios->c_cflag & CSTOPB) {
		
		new_lcr |= MAX3107_LCR_STOPLEN_BIT;
	}

	
	termios->c_cflag &= ~CMSPAR;

	
	s->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		s->port.ignore_status_mask |= MAX3107_ALL_ERRORS;

	
	s->port.state->port.tty->low_latency = 1;

	
	baud = tty_get_baud_rate(tty);

	spin_lock_irqsave(&s->data_lock, flags);
	new_brg = get_new_brg(baud, s);
	
	if (!new_brg) {
		baud = s->baud;
		new_brg = s->brg_cfg;
	}
	spin_unlock_irqrestore(&s->data_lock, flags);
	tty_termios_encode_baud_rate(termios, baud, baud);
	s->baud = baud;

	
	uart_update_timeout(port, termios->c_cflag, baud);

	spin_lock_irqsave(&s->data_lock, flags);
	if (s->lcr_reg != new_lcr) {
		s->lcr_reg = new_lcr;
		s->lcr_commit = 1;
	}
	if (s->brg_cfg != new_brg) {
		s->brg_cfg = new_brg;
		s->brg_commit = 1;
	}
	spin_unlock_irqrestore(&s->data_lock, flags);

	
	max3107_dowork(s);
}

static void max3107_shutdown(struct uart_port *port)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);

	if (s->suspended && s->pdata->hw_suspend)
		s->pdata->hw_suspend(s, 0);

	
	free_irq(s->spi->irq, s);

	if (s->workqueue) {
		
		flush_workqueue(s->workqueue);
		destroy_workqueue(s->workqueue);
		s->workqueue = NULL;
	}

	
	if (s->pdata->hw_suspend)
		s->pdata->hw_suspend(s, 1);
}

static int max3107_startup(struct uart_port *port)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);

	
	s->workqueue = create_freezable_workqueue("max3107");
	if (!s->workqueue) {
		dev_err(&s->spi->dev, "Workqueue creation failed\n");
		return -EBUSY;
	}
	INIT_WORK(&s->work, max3107_work);

	
	if (request_irq(s->spi->irq, max3107_irq, IRQF_TRIGGER_FALLING,
			"max3107", s)) {
		dev_err(&s->spi->dev, "IRQ reguest failed\n");
		destroy_workqueue(s->workqueue);
		s->workqueue = NULL;
		return -EBUSY;
	}

	
	if (s->pdata->hw_suspend)
		s->pdata->hw_suspend(s, 0);

	
	max3107_register_init(s);

	return 0;
}

static const char *max3107_type(struct uart_port *port)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);
	return s->spi->modalias;
}

static void max3107_release_port(struct uart_port *port)
{
	
}

static int max3107_request_port(struct uart_port *port)
{
	
	return 0;
}

static void max3107_config_port(struct uart_port *port, int flags)
{
	struct max3107_port *s = container_of(port, struct max3107_port, port);
	s->port.type = PORT_MAX3107;
}

static int max3107_verify_port(struct uart_port *port,
				struct serial_struct *ser)
{
	if (ser->type == PORT_UNKNOWN || ser->type == PORT_MAX3107)
		return 0;

	return -EINVAL;
}

static void max3107_stop_tx(struct uart_port *port)
{
	
}

static void max3107_break_ctl(struct uart_port *port, int break_state)
{
	
}


static struct uart_ops max3107_ops = {
	.tx_empty       = max3107_tx_empty,
	.set_mctrl      = max3107_set_mctrl,
	.get_mctrl      = max3107_get_mctrl,
	.stop_tx        = max3107_stop_tx,
	.start_tx       = max3107_start_tx,
	.stop_rx        = max3107_stop_rx,
	.enable_ms      = max3107_enable_ms,
	.break_ctl      = max3107_break_ctl,
	.startup        = max3107_startup,
	.shutdown       = max3107_shutdown,
	.set_termios    = max3107_set_termios,
	.type           = max3107_type,
	.release_port   = max3107_release_port,
	.request_port   = max3107_request_port,
	.config_port    = max3107_config_port,
	.verify_port    = max3107_verify_port,
};

static struct uart_driver max3107_uart_driver = {
	.owner          = THIS_MODULE,
	.driver_name    = "ttyMAX",
	.dev_name       = "ttyMAX",
	.nr             = 1,
};

static int driver_registered = 0;



static struct max3107_plat generic_plat_data = {
	.loopback               = 0,
	.ext_clk                = 1,
	.hw_suspend		= max3107_hw_susp,
	.polled_mode            = 0,
	.poll_time              = 0,
};




int max3107_probe(struct spi_device *spi, struct max3107_plat *pdata)
{
	struct max3107_port *s;
	u16 buf[2];	
	int retval;

	pr_info("enter max3107 probe\n");

	
	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		pr_err("Allocating port structure failed\n");
		return -ENOMEM;
	}

	s->pdata = pdata;

	s->rxbuf = kzalloc(sizeof(u16) * (MAX3107_RX_FIFO_SIZE+2), GFP_KERNEL);
	if (!s->rxbuf) {
		pr_err("Allocating RX buffer failed\n");
		retval = -ENOMEM;
		goto err_free4;
	}
	s->rxstr = kzalloc(sizeof(u8) * MAX3107_RX_FIFO_SIZE, GFP_KERNEL);
	if (!s->rxstr) {
		pr_err("Allocating RX buffer failed\n");
		retval = -ENOMEM;
		goto err_free3;
	}
	s->txbuf = kzalloc(sizeof(u16) * MAX3107_TX_FIFO_SIZE + 3, GFP_KERNEL);
	if (!s->txbuf) {
		pr_err("Allocating TX buffer failed\n");
		retval = -ENOMEM;
		goto err_free2;
	}
	
	spin_lock_init(&s->data_lock);

	
	dev_set_drvdata(&spi->dev, s);
	spi->mode = SPI_MODE_0;
	spi->dev.platform_data = pdata;
	spi->bits_per_word = 16;
	s->ext_clk = pdata->ext_clk;
	s->loopback = pdata->loopback;
	spi_setup(spi);
	s->spi = spi;

	
	buf[0] = MAX3107_REVID_REG;
	if (max3107_rw(s, (u8 *)buf, (u8 *)buf, 2)) {
		dev_err(&s->spi->dev, "SPI transfer for REVID read failed\n");
		retval = -EIO;
		goto err_free1;
	}
	if ((buf[0] & MAX3107_SPI_RX_DATA_MASK) != MAX3107_REVID1 &&
		(buf[0] & MAX3107_SPI_RX_DATA_MASK) != MAX3107_REVID2) {
		dev_err(&s->spi->dev, "REVID %x does not match\n",
				(buf[0] & MAX3107_SPI_RX_DATA_MASK));
		retval = -ENODEV;
		goto err_free1;
	}

	
	buf[0] = (MAX3107_WRITE_BIT | MAX3107_IRQEN_REG | 0x0000);
	buf[0] |= 0x0000;

	
	buf[1] = (MAX3107_WRITE_BIT | MAX3107_CLKSRC_REG);
	if (s->ext_clk) {
		
		buf[1] |= MAX3107_CLKSRC_EXTCLK_BIT;
	}

	
	buf[1] |= MAX3107_CLKSRC_PLLBYP_BIT;

	
	if (max3107_rw(s, (u8 *)buf, NULL, 4)) {
		dev_err(&s->spi->dev, "SPI transfer for init failed\n");
		retval = -EIO;
		goto err_free1;
	}

	
	if (!driver_registered) {
		retval = uart_register_driver(&max3107_uart_driver);
		if (retval) {
			dev_err(&s->spi->dev, "Registering UART driver failed\n");
			goto err_free1;
		}
		driver_registered = 1;
	}

	
	s->port.fifosize = 128;
	s->port.ops = &max3107_ops;
	s->port.line = 0;
	s->port.dev = &spi->dev;
	s->port.uartclk = 9600;
	s->port.flags = UPF_SKIP_TEST | UPF_BOOT_AUTOCONF;
	s->port.irq = s->spi->irq;
	s->port.type = PORT_MAX3107;

	
	retval = uart_add_one_port(&max3107_uart_driver, &s->port);
	if (retval < 0) {
		dev_err(&s->spi->dev, "Adding UART port failed\n");
		goto err_free1;
	}

	if (pdata->configure) {
		retval = pdata->configure(s);
		if (retval < 0)
			goto err_free1;
	}

	
	if (pdata->hw_suspend)
		pdata->hw_suspend(s, 1);

	return 0;

err_free1:
	kfree(s->txbuf);
err_free2:
	kfree(s->rxstr);
err_free3:
	kfree(s->rxbuf);
err_free4:
	kfree(s);
	return retval;
}
EXPORT_SYMBOL_GPL(max3107_probe);

int max3107_remove(struct spi_device *spi)
{
	struct max3107_port *s = dev_get_drvdata(&spi->dev);

	pr_info("enter max3107 remove\n");

	
	if (uart_remove_one_port(&max3107_uart_driver, &s->port))
		dev_warn(&s->spi->dev, "Removing UART port failed\n");


	
	kfree(s->rxbuf);
	kfree(s->rxstr);
	kfree(s->txbuf);

	
	kfree(s);

	return 0;
}
EXPORT_SYMBOL_GPL(max3107_remove);

int max3107_suspend(struct spi_device *spi, pm_message_t state)
{
#ifdef CONFIG_PM
	struct max3107_port *s = dev_get_drvdata(&spi->dev);

	pr_debug("enter suspend\n");

	
	uart_suspend_port(&max3107_uart_driver, &s->port);

	
	if (s->pdata->hw_suspend)
		s->pdata->hw_suspend(s, 1);
#endif	
	return 0;
}
EXPORT_SYMBOL_GPL(max3107_suspend);

int max3107_resume(struct spi_device *spi)
{
#ifdef CONFIG_PM
	struct max3107_port *s = dev_get_drvdata(&spi->dev);

	pr_debug("enter resume\n");

	
	if (s->pdata->hw_suspend)
		s->pdata->hw_suspend(s, 0);

	
	uart_resume_port(&max3107_uart_driver, &s->port);
#endif	
	return 0;
}
EXPORT_SYMBOL_GPL(max3107_resume);

static int max3107_probe_generic(struct spi_device *spi)
{
	return max3107_probe(spi, &generic_plat_data);
}

static struct spi_driver max3107_driver = {
	.driver = {
		.name		= "max3107",
		.owner		= THIS_MODULE,
	},
	.probe		= max3107_probe_generic,
	.remove		= __devexit_p(max3107_remove),
	.suspend	= max3107_suspend,
	.resume		= max3107_resume,
};

static int __init max3107_init(void)
{
	pr_info("enter max3107 init\n");
	return spi_register_driver(&max3107_driver);
}

static void __exit max3107_exit(void)
{
	pr_info("enter max3107 exit\n");
	
	if (driver_registered)
		uart_unregister_driver(&max3107_uart_driver);
	spi_unregister_driver(&max3107_driver);
}

module_init(max3107_init);
module_exit(max3107_exit);

MODULE_DESCRIPTION("MAX3107 driver");
MODULE_AUTHOR("Aavamobile");
MODULE_ALIAS("spi:max3107");
MODULE_LICENSE("GPL v2");
