/*
 * Freescale QUICC Engine UART device driver
 *
 * Author: Timur Tabi <timur@freescale.com>
 *
 * Copyright 2007 Freescale Semiconductor, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * This driver adds support for UART devices via Freescale's QUICC Engine
 * found on some Freescale SOCs.
 *
 * If Soft-UART support is needed but not already present, then this driver
 * will request and upload the "Soft-UART" microcode upon probe.  The
 * filename of the microcode should be fsl_qe_ucode_uart_X_YZ.bin, where "X"
 * is the name of the SOC (e.g. 8323), and YZ is the revision of the SOC,
 * (e.g. "11" for 1.1).
 */

#include <linux/module.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>

#include <linux/fs_uart_pd.h>
#include <asm/ucc_slow.h>

#include <linux/firmware.h>
#include <asm/reg.h>

#define UCC_SLOW_GUMR_H_SUART   	0x00004000      

static int soft_uart;
static int firmware_loaded;


#define SERIAL_QE_MAJOR 204
#define SERIAL_QE_MINOR 46

#define UCC_MAX_UART    4

#define RX_NUM_FIFO     4

#define TX_NUM_FIFO     4

#define RX_BUF_SIZE     32

#define TX_BUF_SIZE     32

#define UCC_WAIT_CLOSING 100

struct ucc_uart_pram {
	struct ucc_slow_pram common;
	u8 res1[8];     	
	__be16 maxidl;  	
	__be16 idlc;    	
	__be16 brkcr;   	
	__be16 parec;   	
	__be16 frmec;   	
	__be16 nosec;   	
	__be16 brkec;   	
	__be16 brkln;   	
	__be16 uaddr[2];	
	__be16 rtemp;   	
	__be16 toseq;   	
	__be16 cchars[8];       
	__be16 rccm;    	
	__be16 rccr;    	
	__be16 rlbc;    	
	__be16 res2;    	
	__be32 res3;    	
	u8 res4;		
	u8 res5[3];     	
	__be32 res6;    	
	__be32 res7;    	
	__be32 res8;    	
	__be32 res9;    	
	__be32 res10;   	
	__be32 res11;   	
	__be32 res12;   	
	__be32 res13;   	
	__be16 supsmr;  	
	__be16 res92;   	
	__be32 rx_state;	
	__be32 rx_cnt;  	
	u8 rx_length;   	
	u8 rx_bitmark;  	
	u8 rx_temp_dlst_qe;     
	u8 res14[0xBC - 0x9F];  
	__be32 dump_ptr;	
	__be32 rx_frame_rem;    
	u8 rx_frame_rem_size;   
	u8 tx_mode;     	
	__be16 tx_state;	
	u8 res15[0xD0 - 0xC8];  
	__be32 resD0;   	
	u8 resD4;       	
	__be16 resD5;   	
} __attribute__ ((packed));

#define UCC_UART_SUPSMR_SL      	0x8000
#define UCC_UART_SUPSMR_RPM_MASK	0x6000
#define UCC_UART_SUPSMR_RPM_ODD 	0x0000
#define UCC_UART_SUPSMR_RPM_LOW 	0x2000
#define UCC_UART_SUPSMR_RPM_EVEN	0x4000
#define UCC_UART_SUPSMR_RPM_HIGH	0x6000
#define UCC_UART_SUPSMR_PEN     	0x1000
#define UCC_UART_SUPSMR_TPM_MASK	0x0C00
#define UCC_UART_SUPSMR_TPM_ODD 	0x0000
#define UCC_UART_SUPSMR_TPM_LOW 	0x0400
#define UCC_UART_SUPSMR_TPM_EVEN	0x0800
#define UCC_UART_SUPSMR_TPM_HIGH	0x0C00
#define UCC_UART_SUPSMR_FRZ     	0x0100
#define UCC_UART_SUPSMR_UM_MASK 	0x00c0
#define UCC_UART_SUPSMR_UM_NORMAL       0x0000
#define UCC_UART_SUPSMR_UM_MAN_MULTI    0x0040
#define UCC_UART_SUPSMR_UM_AUTO_MULTI   0x00c0
#define UCC_UART_SUPSMR_CL_MASK 	0x0030
#define UCC_UART_SUPSMR_CL_8    	0x0030
#define UCC_UART_SUPSMR_CL_7    	0x0020
#define UCC_UART_SUPSMR_CL_6    	0x0010
#define UCC_UART_SUPSMR_CL_5    	0x0000

#define UCC_UART_TX_STATE_AHDLC 	0x00
#define UCC_UART_TX_STATE_UART  	0x01
#define UCC_UART_TX_STATE_X1    	0x00
#define UCC_UART_TX_STATE_X16   	0x80

#define UCC_UART_PRAM_ALIGNMENT 0x100

#define UCC_UART_SIZE_OF_BD     UCC_SLOW_SIZE_OF_BD
#define NUM_CONTROL_CHARS       8

struct uart_qe_port {
	struct uart_port port;
	struct ucc_slow __iomem *uccp;
	struct ucc_uart_pram __iomem *uccup;
	struct ucc_slow_info us_info;
	struct ucc_slow_private *us_private;
	struct device_node *np;
	unsigned int ucc_num;   

	u16 rx_nrfifos;
	u16 rx_fifosize;
	u16 tx_nrfifos;
	u16 tx_fifosize;
	int wait_closing;
	u32 flags;
	struct qe_bd *rx_bd_base;
	struct qe_bd *rx_cur;
	struct qe_bd *tx_bd_base;
	struct qe_bd *tx_cur;
	unsigned char *tx_buf;
	unsigned char *rx_buf;
	void *bd_virt;  	
	dma_addr_t bd_dma_addr; 
	unsigned int bd_size;   
};

static struct uart_driver ucc_uart_driver = {
	.owner  	= THIS_MODULE,
	.driver_name    = "ucc_uart",
	.dev_name       = "ttyQE",
	.major  	= SERIAL_QE_MAJOR,
	.minor  	= SERIAL_QE_MINOR,
	.nr     	= UCC_MAX_UART,
};

static inline dma_addr_t cpu2qe_addr(void *addr, struct uart_qe_port *qe_port)
{
	if (likely((addr >= qe_port->bd_virt)) &&
	    (addr < (qe_port->bd_virt + qe_port->bd_size)))
		return qe_port->bd_dma_addr + (addr - qe_port->bd_virt);

	
	printk(KERN_ERR "%s: addr=%p\n", __func__, addr);
	BUG();
	return 0;
}

static inline void *qe2cpu_addr(dma_addr_t addr, struct uart_qe_port *qe_port)
{
	
	if (likely((addr >= qe_port->bd_dma_addr) &&
		   (addr < (qe_port->bd_dma_addr + qe_port->bd_size))))
		return qe_port->bd_virt + (addr - qe_port->bd_dma_addr);

	
	printk(KERN_ERR "%s: addr=%llx\n", __func__, (u64)addr);
	BUG();
	return NULL;
}

static unsigned int qe_uart_tx_empty(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);
	struct qe_bd *bdp = qe_port->tx_bd_base;

	while (1) {
		if (in_be16(&bdp->status) & BD_SC_READY)
			
			return 0;

		if (in_be16(&bdp->status) & BD_SC_WRAP)
			return 1;

		bdp++;
	};
}

void qe_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int qe_uart_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void qe_uart_stop_tx(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);

	clrbits16(&qe_port->uccp->uccm, UCC_UART_UCCE_TX);
}

static int qe_uart_tx_pump(struct uart_qe_port *qe_port)
{
	struct qe_bd *bdp;
	unsigned char *p;
	unsigned int count;
	struct uart_port *port = &qe_port->port;
	struct circ_buf *xmit = &port->state->xmit;

	bdp = qe_port->rx_cur;

	
	if (port->x_char) {
		
		bdp = qe_port->tx_cur;

		p = qe2cpu_addr(bdp->buf, qe_port);

		*p++ = port->x_char;
		out_be16(&bdp->length, 1);
		setbits16(&bdp->status, BD_SC_READY);
		
		if (in_be16(&bdp->status) & BD_SC_WRAP)
			bdp = qe_port->tx_bd_base;
		else
			bdp++;
		qe_port->tx_cur = bdp;

		port->icount.tx++;
		port->x_char = 0;
		return 1;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		qe_uart_stop_tx(port);
		return 0;
	}

	
	bdp = qe_port->tx_cur;

	while (!(in_be16(&bdp->status) & BD_SC_READY) &&
	       (xmit->tail != xmit->head)) {
		count = 0;
		p = qe2cpu_addr(bdp->buf, qe_port);
		while (count < qe_port->tx_fifosize) {
			*p++ = xmit->buf[xmit->tail];
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
			port->icount.tx++;
			count++;
			if (xmit->head == xmit->tail)
				break;
		}

		out_be16(&bdp->length, count);
		setbits16(&bdp->status, BD_SC_READY);

		
		if (in_be16(&bdp->status) & BD_SC_WRAP)
			bdp = qe_port->tx_bd_base;
		else
			bdp++;
	}
	qe_port->tx_cur = bdp;

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit)) {
		qe_uart_stop_tx(port);
		return 0;
	}

	return 1;
}

static void qe_uart_start_tx(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);

	
	if (in_be16(&qe_port->uccp->uccm) & UCC_UART_UCCE_TX)
		return;

	
	if (qe_uart_tx_pump(qe_port))
		setbits16(&qe_port->uccp->uccm, UCC_UART_UCCE_TX);
}

static void qe_uart_stop_rx(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);

	clrbits16(&qe_port->uccp->uccm, UCC_UART_UCCE_RX);
}

static void qe_uart_enable_ms(struct uart_port *port)
{
}

static void qe_uart_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);

	if (break_state)
		ucc_slow_stop_tx(qe_port->us_private);
	else
		ucc_slow_restart_tx(qe_port->us_private);
}

static void qe_uart_int_rx(struct uart_qe_port *qe_port)
{
	int i;
	unsigned char ch, *cp;
	struct uart_port *port = &qe_port->port;
	struct tty_struct *tty = port->state->port.tty;
	struct qe_bd *bdp;
	u16 status;
	unsigned int flg;

	bdp = qe_port->rx_cur;
	while (1) {
		status = in_be16(&bdp->status);

		
		if (status & BD_SC_EMPTY)
			break;

		
		i = in_be16(&bdp->length);

		if (tty_buffer_request_room(tty, i) < i) {
			dev_dbg(port->dev, "ucc-uart: no room in RX buffer\n");
			return;
		}

		
		cp = qe2cpu_addr(bdp->buf, qe_port);

		
		while (i-- > 0) {
			ch = *cp++;
			port->icount.rx++;
			flg = TTY_NORMAL;

			if (!i && status &
			    (BD_SC_BR | BD_SC_FR | BD_SC_PR | BD_SC_OV))
				goto handle_error;
			if (uart_handle_sysrq_char(port, ch))
				continue;

error_return:
			tty_insert_flip_char(tty, ch, flg);

		}

		
		clrsetbits_be16(&bdp->status, BD_SC_BR | BD_SC_FR | BD_SC_PR |
			BD_SC_OV | BD_SC_ID, BD_SC_EMPTY);
		if (in_be16(&bdp->status) & BD_SC_WRAP)
			bdp = qe_port->rx_bd_base;
		else
			bdp++;

	}

	
	qe_port->rx_cur = bdp;

	
	tty_flip_buffer_push(tty);

	return;

	

handle_error:
	
	if (status & BD_SC_BR)
		port->icount.brk++;
	if (status & BD_SC_PR)
		port->icount.parity++;
	if (status & BD_SC_FR)
		port->icount.frame++;
	if (status & BD_SC_OV)
		port->icount.overrun++;

	
	status &= port->read_status_mask;

	
	if (status & BD_SC_BR)
		flg = TTY_BREAK;
	else if (status & BD_SC_PR)
		flg = TTY_PARITY;
	else if (status & BD_SC_FR)
		flg = TTY_FRAME;

	
	if (status & BD_SC_OV)
		tty_insert_flip_char(tty, 0, TTY_OVERRUN);
#ifdef SUPPORT_SYSRQ
	port->sysrq = 0;
#endif
	goto error_return;
}

static irqreturn_t qe_uart_int(int irq, void *data)
{
	struct uart_qe_port *qe_port = (struct uart_qe_port *) data;
	struct ucc_slow __iomem *uccp = qe_port->uccp;
	u16 events;

	
	events = in_be16(&uccp->ucce);
	out_be16(&uccp->ucce, events);

	if (events & UCC_UART_UCCE_BRKE)
		uart_handle_break(&qe_port->port);

	if (events & UCC_UART_UCCE_RX)
		qe_uart_int_rx(qe_port);

	if (events & UCC_UART_UCCE_TX)
		qe_uart_tx_pump(qe_port);

	return events ? IRQ_HANDLED : IRQ_NONE;
}

static void qe_uart_initbd(struct uart_qe_port *qe_port)
{
	int i;
	void *bd_virt;
	struct qe_bd *bdp;

	bd_virt = qe_port->bd_virt;
	bdp = qe_port->rx_bd_base;
	qe_port->rx_cur = qe_port->rx_bd_base;
	for (i = 0; i < (qe_port->rx_nrfifos - 1); i++) {
		out_be16(&bdp->status, BD_SC_EMPTY | BD_SC_INTRPT);
		out_be32(&bdp->buf, cpu2qe_addr(bd_virt, qe_port));
		out_be16(&bdp->length, 0);
		bd_virt += qe_port->rx_fifosize;
		bdp++;
	}

	
	out_be16(&bdp->status, BD_SC_WRAP | BD_SC_EMPTY | BD_SC_INTRPT);
	out_be32(&bdp->buf, cpu2qe_addr(bd_virt, qe_port));
	out_be16(&bdp->length, 0);

	bd_virt = qe_port->bd_virt +
		L1_CACHE_ALIGN(qe_port->rx_nrfifos * qe_port->rx_fifosize);
	qe_port->tx_cur = qe_port->tx_bd_base;
	bdp = qe_port->tx_bd_base;
	for (i = 0; i < (qe_port->tx_nrfifos - 1); i++) {
		out_be16(&bdp->status, BD_SC_INTRPT);
		out_be32(&bdp->buf, cpu2qe_addr(bd_virt, qe_port));
		out_be16(&bdp->length, 0);
		bd_virt += qe_port->tx_fifosize;
		bdp++;
	}

	
#ifdef LOOPBACK
	setbits16(&qe_port->tx_cur->status, BD_SC_P);
#endif

	out_be16(&bdp->status, BD_SC_WRAP | BD_SC_INTRPT);
	out_be32(&bdp->buf, cpu2qe_addr(bd_virt, qe_port));
	out_be16(&bdp->length, 0);
}

static void qe_uart_init_ucc(struct uart_qe_port *qe_port)
{
	u32 cecr_subblock;
	struct ucc_slow __iomem *uccp = qe_port->uccp;
	struct ucc_uart_pram *uccup = qe_port->uccup;

	unsigned int i;

	
	ucc_slow_disable(qe_port->us_private, COMM_DIR_RX_AND_TX);

	
	out_8(&uccup->common.rbmr, UCC_BMR_GBL | UCC_BMR_BO_BE);
	out_8(&uccup->common.tbmr, UCC_BMR_GBL | UCC_BMR_BO_BE);
	out_be16(&uccup->common.mrblr, qe_port->rx_fifosize);
	out_be16(&uccup->maxidl, 0x10);
	out_be16(&uccup->brkcr, 1);
	out_be16(&uccup->parec, 0);
	out_be16(&uccup->frmec, 0);
	out_be16(&uccup->nosec, 0);
	out_be16(&uccup->brkec, 0);
	out_be16(&uccup->uaddr[0], 0);
	out_be16(&uccup->uaddr[1], 0);
	out_be16(&uccup->toseq, 0);
	for (i = 0; i < 8; i++)
		out_be16(&uccup->cchars[i], 0xC000);
	out_be16(&uccup->rccm, 0xc0ff);

	
	if (soft_uart) {
		
		clrsetbits_be32(&uccp->gumr_l,
			UCC_SLOW_GUMR_L_MODE_MASK | UCC_SLOW_GUMR_L_TDCR_MASK |
			UCC_SLOW_GUMR_L_RDCR_MASK,
			UCC_SLOW_GUMR_L_MODE_UART | UCC_SLOW_GUMR_L_TDCR_1 |
			UCC_SLOW_GUMR_L_RDCR_16);

		clrsetbits_be32(&uccp->gumr_h, UCC_SLOW_GUMR_H_RFW,
			UCC_SLOW_GUMR_H_TRX | UCC_SLOW_GUMR_H_TTX);
	} else {
		clrsetbits_be32(&uccp->gumr_l,
			UCC_SLOW_GUMR_L_MODE_MASK | UCC_SLOW_GUMR_L_TDCR_MASK |
			UCC_SLOW_GUMR_L_RDCR_MASK,
			UCC_SLOW_GUMR_L_MODE_UART | UCC_SLOW_GUMR_L_TDCR_16 |
			UCC_SLOW_GUMR_L_RDCR_16);

		clrsetbits_be32(&uccp->gumr_h,
			UCC_SLOW_GUMR_H_TRX | UCC_SLOW_GUMR_H_TTX,
			UCC_SLOW_GUMR_H_RFW);
	}

#ifdef LOOPBACK
	clrsetbits_be32(&uccp->gumr_l, UCC_SLOW_GUMR_L_DIAG_MASK,
		UCC_SLOW_GUMR_L_DIAG_LOOP);
	clrsetbits_be32(&uccp->gumr_h,
		UCC_SLOW_GUMR_H_CTSP | UCC_SLOW_GUMR_H_RSYN,
		UCC_SLOW_GUMR_H_CDS);
#endif

	
	out_be16(&uccp->uccm, 0);
	out_be16(&uccp->ucce, 0xffff);
	out_be16(&uccp->udsr, 0x7e7e);

	
	out_be16(&uccp->upsmr, 0);

	if (soft_uart) {
		out_be16(&uccup->supsmr, 0x30);
		out_be16(&uccup->res92, 0);
		out_be32(&uccup->rx_state, 0);
		out_be32(&uccup->rx_cnt, 0);
		out_8(&uccup->rx_bitmark, 0);
		out_8(&uccup->rx_length, 10);
		out_be32(&uccup->dump_ptr, 0x4000);
		out_8(&uccup->rx_temp_dlst_qe, 0);
		out_be32(&uccup->rx_frame_rem, 0);
		out_8(&uccup->rx_frame_rem_size, 0);
		
		out_8(&uccup->tx_mode,
			UCC_UART_TX_STATE_UART | UCC_UART_TX_STATE_X1);
		out_be16(&uccup->tx_state, 0);
		out_8(&uccup->resD4, 0);
		out_be16(&uccup->resD5, 0);


		clrsetbits_be32(&uccp->gumr_l,
			UCC_SLOW_GUMR_L_MODE_MASK | UCC_SLOW_GUMR_L_TDCR_MASK |
			UCC_SLOW_GUMR_L_RDCR_MASK,
			UCC_SLOW_GUMR_L_MODE_QMC | UCC_SLOW_GUMR_L_TDCR_16 |
			UCC_SLOW_GUMR_L_RDCR_16);

		clrsetbits_be32(&uccp->gumr_h,
			UCC_SLOW_GUMR_H_RFW | UCC_SLOW_GUMR_H_RSYN,
			UCC_SLOW_GUMR_H_SUART | UCC_SLOW_GUMR_H_TRX |
			UCC_SLOW_GUMR_H_TTX | UCC_SLOW_GUMR_H_TFL);

#ifdef LOOPBACK
		clrsetbits_be32(&uccp->gumr_l, UCC_SLOW_GUMR_L_DIAG_MASK,
				UCC_SLOW_GUMR_L_DIAG_LOOP);
		clrbits32(&uccp->gumr_h, UCC_SLOW_GUMR_H_CTSP |
			  UCC_SLOW_GUMR_H_CDS);
#endif

		cecr_subblock = ucc_slow_get_qe_cr_subblock(qe_port->ucc_num);
		qe_issue_cmd(QE_INIT_TX_RX, cecr_subblock,
			QE_CR_PROTOCOL_UNSPECIFIED, 0);
	} else {
		cecr_subblock = ucc_slow_get_qe_cr_subblock(qe_port->ucc_num);
		qe_issue_cmd(QE_INIT_TX_RX, cecr_subblock,
			QE_CR_PROTOCOL_UART, 0);
	}
}

static int qe_uart_startup(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);
	int ret;

	if (soft_uart && !firmware_loaded) {
		dev_err(port->dev, "Soft-UART firmware not uploaded\n");
		return -ENODEV;
	}

	qe_uart_initbd(qe_port);
	qe_uart_init_ucc(qe_port);

	
	ret = request_irq(port->irq, qe_uart_int, IRQF_SHARED, "ucc-uart",
		qe_port);
	if (ret) {
		dev_err(port->dev, "could not claim IRQ %u\n", port->irq);
		return ret;
	}

	
	setbits16(&qe_port->uccp->uccm, UCC_UART_UCCE_RX);
	ucc_slow_enable(qe_port->us_private, COMM_DIR_RX_AND_TX);

	return 0;
}

static void qe_uart_shutdown(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);
	struct ucc_slow __iomem *uccp = qe_port->uccp;
	unsigned int timeout = 20;

	

	
	while (!qe_uart_tx_empty(port)) {
		if (!--timeout) {
			dev_warn(port->dev, "shutdown timeout\n");
			break;
		}
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(2);
	}

	if (qe_port->wait_closing) {
		
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(qe_port->wait_closing);
	}

	
	ucc_slow_disable(qe_port->us_private, COMM_DIR_RX_AND_TX);
	clrbits16(&uccp->uccm, UCC_UART_UCCE_TX | UCC_UART_UCCE_RX);

	
	ucc_slow_graceful_stop_tx(qe_port->us_private);
	qe_uart_initbd(qe_port);

	free_irq(port->irq, qe_port);
}

static void qe_uart_set_termios(struct uart_port *port,
				struct ktermios *termios, struct ktermios *old)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);
	struct ucc_slow __iomem *uccp = qe_port->uccp;
	unsigned int baud;
	unsigned long flags;
	u16 upsmr = in_be16(&uccp->upsmr);
	struct ucc_uart_pram __iomem *uccup = qe_port->uccup;
	u16 supsmr = in_be16(&uccup->supsmr);
	u8 char_length = 2; 


	
	upsmr &= UCC_UART_UPSMR_CL_MASK;
	supsmr &= UCC_UART_SUPSMR_CL_MASK;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		upsmr |= UCC_UART_UPSMR_CL_5;
		supsmr |= UCC_UART_SUPSMR_CL_5;
		char_length += 5;
		break;
	case CS6:
		upsmr |= UCC_UART_UPSMR_CL_6;
		supsmr |= UCC_UART_SUPSMR_CL_6;
		char_length += 6;
		break;
	case CS7:
		upsmr |= UCC_UART_UPSMR_CL_7;
		supsmr |= UCC_UART_SUPSMR_CL_7;
		char_length += 7;
		break;
	default:	
		upsmr |= UCC_UART_UPSMR_CL_8;
		supsmr |= UCC_UART_SUPSMR_CL_8;
		char_length += 8;
		break;
	}

	
	if (termios->c_cflag & CSTOPB) {
		upsmr |= UCC_UART_UPSMR_SL;
		supsmr |= UCC_UART_SUPSMR_SL;
		char_length++;  
	}

	if (termios->c_cflag & PARENB) {
		upsmr |= UCC_UART_UPSMR_PEN;
		supsmr |= UCC_UART_SUPSMR_PEN;
		char_length++;  

		if (!(termios->c_cflag & PARODD)) {
			upsmr &= ~(UCC_UART_UPSMR_RPM_MASK |
				   UCC_UART_UPSMR_TPM_MASK);
			upsmr |= UCC_UART_UPSMR_RPM_EVEN |
				UCC_UART_UPSMR_TPM_EVEN;
			supsmr &= ~(UCC_UART_SUPSMR_RPM_MASK |
				    UCC_UART_SUPSMR_TPM_MASK);
			supsmr |= UCC_UART_SUPSMR_RPM_EVEN |
				UCC_UART_SUPSMR_TPM_EVEN;
		}
	}

	port->read_status_mask = BD_SC_EMPTY | BD_SC_OV;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= BD_SC_FR | BD_SC_PR;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= BD_SC_BR;

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= BD_SC_PR | BD_SC_FR;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= BD_SC_BR;
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= BD_SC_OV;
	}
	if ((termios->c_cflag & CREAD) == 0)
		port->read_status_mask &= ~BD_SC_EMPTY;

	baud = uart_get_baud_rate(port, termios, old, 0, 115200);

	
	spin_lock_irqsave(&port->lock, flags);

	
	uart_update_timeout(port, termios->c_cflag, baud);

	out_be16(&uccp->upsmr, upsmr);
	if (soft_uart) {
		out_be16(&uccup->supsmr, supsmr);
		out_8(&uccup->rx_length, char_length);

		
		qe_setbrg(qe_port->us_info.rx_clock, baud, 16);
		qe_setbrg(qe_port->us_info.tx_clock, baud, 1);
	} else {
		qe_setbrg(qe_port->us_info.rx_clock, baud, 16);
		qe_setbrg(qe_port->us_info.tx_clock, baud, 16);
	}

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *qe_uart_type(struct uart_port *port)
{
	return "QE";
}

static int qe_uart_request_port(struct uart_port *port)
{
	int ret;
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);
	struct ucc_slow_info *us_info = &qe_port->us_info;
	struct ucc_slow_private *uccs;
	unsigned int rx_size, tx_size;
	void *bd_virt;
	dma_addr_t bd_dma_addr = 0;

	ret = ucc_slow_init(us_info, &uccs);
	if (ret) {
		dev_err(port->dev, "could not initialize UCC%u\n",
		       qe_port->ucc_num);
		return ret;
	}

	qe_port->us_private = uccs;
	qe_port->uccp = uccs->us_regs;
	qe_port->uccup = (struct ucc_uart_pram *) uccs->us_pram;
	qe_port->rx_bd_base = uccs->rx_bd;
	qe_port->tx_bd_base = uccs->tx_bd;


	rx_size = L1_CACHE_ALIGN(qe_port->rx_nrfifos * qe_port->rx_fifosize);
	tx_size = L1_CACHE_ALIGN(qe_port->tx_nrfifos * qe_port->tx_fifosize);

	bd_virt = dma_alloc_coherent(port->dev, rx_size + tx_size, &bd_dma_addr,
		GFP_KERNEL);
	if (!bd_virt) {
		dev_err(port->dev, "could not allocate buffer descriptors\n");
		return -ENOMEM;
	}

	qe_port->bd_virt = bd_virt;
	qe_port->bd_dma_addr = bd_dma_addr;
	qe_port->bd_size = rx_size + tx_size;

	qe_port->rx_buf = bd_virt;
	qe_port->tx_buf = qe_port->rx_buf + rx_size;

	return 0;
}

static void qe_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_CPM;
		qe_uart_request_port(port);
	}
}

static void qe_uart_release_port(struct uart_port *port)
{
	struct uart_qe_port *qe_port =
		container_of(port, struct uart_qe_port, port);
	struct ucc_slow_private *uccs = qe_port->us_private;

	dma_free_coherent(port->dev, qe_port->bd_size, qe_port->bd_virt,
			  qe_port->bd_dma_addr);

	ucc_slow_free(uccs);
}

static int qe_uart_verify_port(struct uart_port *port,
			       struct serial_struct *ser)
{
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_CPM)
		return -EINVAL;

	if (ser->irq < 0 || ser->irq >= nr_irqs)
		return -EINVAL;

	if (ser->baud_base < 9600)
		return -EINVAL;

	return 0;
}
static struct uart_ops qe_uart_pops = {
	.tx_empty       = qe_uart_tx_empty,
	.set_mctrl      = qe_uart_set_mctrl,
	.get_mctrl      = qe_uart_get_mctrl,
	.stop_tx	= qe_uart_stop_tx,
	.start_tx       = qe_uart_start_tx,
	.stop_rx	= qe_uart_stop_rx,
	.enable_ms      = qe_uart_enable_ms,
	.break_ctl      = qe_uart_break_ctl,
	.startup	= qe_uart_startup,
	.shutdown       = qe_uart_shutdown,
	.set_termios    = qe_uart_set_termios,
	.type   	= qe_uart_type,
	.release_port   = qe_uart_release_port,
	.request_port   = qe_uart_request_port,
	.config_port    = qe_uart_config_port,
	.verify_port    = qe_uart_verify_port,
};

static unsigned int soc_info(unsigned int *rev_h, unsigned int *rev_l)
{
	struct device_node *np;
	const char *soc_string;
	unsigned int svr;
	unsigned int soc;

	
	np = of_find_node_by_type(NULL, "cpu");
	if (!np)
		return 0;
	
	soc_string = of_get_property(np, "compatible", NULL);
	if (!soc_string)
		
		soc_string = np->name;

	
	if ((sscanf(soc_string, "PowerPC,%u", &soc) != 1) || !soc)
		return 0;

	
	svr = mfspr(SPRN_SVR);
	*rev_h = (svr >> 4) & 0xf;
	*rev_l = svr & 0xf;

	return soc;
}

static void uart_firmware_cont(const struct firmware *fw, void *context)
{
	struct qe_firmware *firmware;
	struct device *dev = context;
	int ret;

	if (!fw) {
		dev_err(dev, "firmware not found\n");
		return;
	}

	firmware = (struct qe_firmware *) fw->data;

	if (firmware->header.length != fw->size) {
		dev_err(dev, "invalid firmware\n");
		goto out;
	}

	ret = qe_upload_firmware(firmware);
	if (ret) {
		dev_err(dev, "could not load firmware\n");
		goto out;
	}

	firmware_loaded = 1;
 out:
	release_firmware(fw);
}

static int ucc_uart_probe(struct platform_device *ofdev)
{
	struct device_node *np = ofdev->dev.of_node;
	const unsigned int *iprop;      
	const char *sprop;      
	struct uart_qe_port *qe_port = NULL;
	struct resource res;
	int ret;

	if (of_find_property(np, "soft-uart", NULL)) {
		dev_dbg(&ofdev->dev, "using Soft-UART mode\n");
		soft_uart = 1;
	}

	if (soft_uart) {
		struct qe_firmware_info *qe_fw_info;

		qe_fw_info = qe_get_firmware_info();

		
		if (qe_fw_info && strstr(qe_fw_info->id, "Soft-UART")) {
			firmware_loaded = 1;
		} else {
			char filename[32];
			unsigned int soc;
			unsigned int rev_h;
			unsigned int rev_l;

			soc = soc_info(&rev_h, &rev_l);
			if (!soc) {
				dev_err(&ofdev->dev, "unknown CPU model\n");
				return -ENXIO;
			}
			sprintf(filename, "fsl_qe_ucode_uart_%u_%u%u.bin",
				soc, rev_h, rev_l);

			dev_info(&ofdev->dev, "waiting for firmware %s\n",
				filename);

			ret = request_firmware_nowait(THIS_MODULE,
				FW_ACTION_HOTPLUG, filename, &ofdev->dev,
				GFP_KERNEL, &ofdev->dev, uart_firmware_cont);
			if (ret) {
				dev_err(&ofdev->dev,
					"could not load firmware %s\n",
					filename);
				return ret;
			}
		}
	}

	qe_port = kzalloc(sizeof(struct uart_qe_port), GFP_KERNEL);
	if (!qe_port) {
		dev_err(&ofdev->dev, "can't allocate QE port structure\n");
		return -ENOMEM;
	}

	
	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		dev_err(&ofdev->dev, "missing 'reg' property in device tree\n");
		goto out_free;
	}
	if (!res.start) {
		dev_err(&ofdev->dev, "invalid 'reg' property in device tree\n");
		ret = -EINVAL;
		goto out_free;
	}
	qe_port->port.mapbase = res.start;

	
	
	iprop = of_get_property(np, "cell-index", NULL);
	if (!iprop) {
		iprop = of_get_property(np, "device-id", NULL);
		if (!iprop) {
			dev_err(&ofdev->dev, "UCC is unspecified in "
				"device tree\n");
			ret = -EINVAL;
			goto out_free;
		}
	}

	if ((*iprop < 1) || (*iprop > UCC_MAX_NUM)) {
		dev_err(&ofdev->dev, "no support for UCC%u\n", *iprop);
		ret = -ENODEV;
		goto out_free;
	}
	qe_port->ucc_num = *iprop - 1;


	sprop = of_get_property(np, "rx-clock-name", NULL);
	if (!sprop) {
		dev_err(&ofdev->dev, "missing rx-clock-name in device tree\n");
		ret = -ENODEV;
		goto out_free;
	}

	qe_port->us_info.rx_clock = qe_clock_source(sprop);
	if ((qe_port->us_info.rx_clock < QE_BRG1) ||
	    (qe_port->us_info.rx_clock > QE_BRG16)) {
		dev_err(&ofdev->dev, "rx-clock-name must be a BRG for UART\n");
		ret = -ENODEV;
		goto out_free;
	}

#ifdef LOOPBACK
	
	qe_port->us_info.tx_clock = qe_port->us_info.rx_clock;
#else
	sprop = of_get_property(np, "tx-clock-name", NULL);
	if (!sprop) {
		dev_err(&ofdev->dev, "missing tx-clock-name in device tree\n");
		ret = -ENODEV;
		goto out_free;
	}
	qe_port->us_info.tx_clock = qe_clock_source(sprop);
#endif
	if ((qe_port->us_info.tx_clock < QE_BRG1) ||
	    (qe_port->us_info.tx_clock > QE_BRG16)) {
		dev_err(&ofdev->dev, "tx-clock-name must be a BRG for UART\n");
		ret = -ENODEV;
		goto out_free;
	}

	
	iprop = of_get_property(np, "port-number", NULL);
	if (!iprop) {
		dev_err(&ofdev->dev, "missing port-number in device tree\n");
		ret = -EINVAL;
		goto out_free;
	}
	qe_port->port.line = *iprop;
	if (qe_port->port.line >= UCC_MAX_UART) {
		dev_err(&ofdev->dev, "port-number must be 0-%u\n",
			UCC_MAX_UART - 1);
		ret = -EINVAL;
		goto out_free;
	}

	qe_port->port.irq = irq_of_parse_and_map(np, 0);
	if (qe_port->port.irq == 0) {
		dev_err(&ofdev->dev, "could not map IRQ for UCC%u\n",
		       qe_port->ucc_num + 1);
		ret = -EINVAL;
		goto out_free;
	}

	np = of_find_compatible_node(NULL, NULL, "fsl,qe");
	if (!np) {
		np = of_find_node_by_type(NULL, "qe");
		if (!np) {
			dev_err(&ofdev->dev, "could not find 'qe' node\n");
			ret = -EINVAL;
			goto out_free;
		}
	}

	iprop = of_get_property(np, "brg-frequency", NULL);
	if (!iprop) {
		dev_err(&ofdev->dev,
		       "missing brg-frequency in device tree\n");
		ret = -EINVAL;
		goto out_np;
	}

	if (*iprop)
		qe_port->port.uartclk = *iprop;
	else {
		iprop = of_get_property(np, "bus-frequency", NULL);
		if (!iprop) {
			dev_err(&ofdev->dev,
				"missing QE bus-frequency in device tree\n");
			ret = -EINVAL;
			goto out_np;
		}
		if (*iprop)
			qe_port->port.uartclk = *iprop / 2;
		else {
			dev_err(&ofdev->dev,
				"invalid QE bus-frequency in device tree\n");
			ret = -EINVAL;
			goto out_np;
		}
	}

	spin_lock_init(&qe_port->port.lock);
	qe_port->np = np;
	qe_port->port.dev = &ofdev->dev;
	qe_port->port.ops = &qe_uart_pops;
	qe_port->port.iotype = UPIO_MEM;

	qe_port->tx_nrfifos = TX_NUM_FIFO;
	qe_port->tx_fifosize = TX_BUF_SIZE;
	qe_port->rx_nrfifos = RX_NUM_FIFO;
	qe_port->rx_fifosize = RX_BUF_SIZE;

	qe_port->wait_closing = UCC_WAIT_CLOSING;
	qe_port->port.fifosize = 512;
	qe_port->port.flags = UPF_BOOT_AUTOCONF | UPF_IOREMAP;

	qe_port->us_info.ucc_num = qe_port->ucc_num;
	qe_port->us_info.regs = (phys_addr_t) res.start;
	qe_port->us_info.irq = qe_port->port.irq;

	qe_port->us_info.rx_bd_ring_len = qe_port->rx_nrfifos;
	qe_port->us_info.tx_bd_ring_len = qe_port->tx_nrfifos;

	
	qe_port->us_info.init_tx = 1;
	qe_port->us_info.init_rx = 1;

	ret = uart_add_one_port(&ucc_uart_driver, &qe_port->port);
	if (ret) {
		dev_err(&ofdev->dev, "could not add /dev/ttyQE%u\n",
		       qe_port->port.line);
		goto out_np;
	}

	dev_set_drvdata(&ofdev->dev, qe_port);

	dev_info(&ofdev->dev, "UCC%u assigned to /dev/ttyQE%u\n",
		qe_port->ucc_num + 1, qe_port->port.line);

	
	dev_dbg(&ofdev->dev, "mknod command is 'mknod /dev/ttyQE%u c %u %u'\n",
	       qe_port->port.line, SERIAL_QE_MAJOR,
	       SERIAL_QE_MINOR + qe_port->port.line);

	return 0;
out_np:
	of_node_put(np);
out_free:
	kfree(qe_port);
	return ret;
}

static int ucc_uart_remove(struct platform_device *ofdev)
{
	struct uart_qe_port *qe_port = dev_get_drvdata(&ofdev->dev);

	dev_info(&ofdev->dev, "removing /dev/ttyQE%u\n", qe_port->port.line);

	uart_remove_one_port(&ucc_uart_driver, &qe_port->port);

	dev_set_drvdata(&ofdev->dev, NULL);
	kfree(qe_port);

	return 0;
}

static struct of_device_id ucc_uart_match[] = {
	{
		.type = "serial",
		.compatible = "ucc_uart",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ucc_uart_match);

static struct platform_driver ucc_uart_of_driver = {
	.driver = {
		.name = "ucc_uart",
		.owner = THIS_MODULE,
		.of_match_table    = ucc_uart_match,
	},
	.probe  	= ucc_uart_probe,
	.remove 	= ucc_uart_remove,
};

static int __init ucc_uart_init(void)
{
	int ret;

	printk(KERN_INFO "Freescale QUICC Engine UART device driver\n");
#ifdef LOOPBACK
	printk(KERN_INFO "ucc-uart: Using loopback mode\n");
#endif

	ret = uart_register_driver(&ucc_uart_driver);
	if (ret) {
		printk(KERN_ERR "ucc-uart: could not register UART driver\n");
		return ret;
	}

	ret = platform_driver_register(&ucc_uart_of_driver);
	if (ret)
		printk(KERN_ERR
		       "ucc-uart: could not register platform driver\n");

	return ret;
}

static void __exit ucc_uart_exit(void)
{
	printk(KERN_INFO
	       "Freescale QUICC Engine UART device driver unloading\n");

	platform_driver_unregister(&ucc_uart_of_driver);
	uart_unregister_driver(&ucc_uart_driver);
}

module_init(ucc_uart_init);
module_exit(ucc_uart_exit);

MODULE_DESCRIPTION("Freescale QUICC Engine (QE) UART");
MODULE_AUTHOR("Timur Tabi <timur@freescale.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_CHARDEV_MAJOR(SERIAL_QE_MAJOR);

