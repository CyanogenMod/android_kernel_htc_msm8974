/* Low-level parallel port routines for built-in port on SGI IP32
 *
 * Author: Arnaud Giersch <arnaud.giersch@free.fr>
 *
 * Based on parport_pc.c by
 *	Phil Blundell, Tim Waugh, Jose Renau, David Campbell,
 *	Andrea Arcangeli, et al.
 *
 * Thanks to Ilya A. Volynets-Evenbakh for his help.
 *
 * Copyright (C) 2005, 2006 Arnaud Giersch.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */




#if !defined(DEBUG_PARPORT_IP32)
#	define DEBUG_PARPORT_IP32  0	
#endif


#if DEBUG_PARPORT_IP32 == 1
#	warning DEBUG_PARPORT_IP32 == 1
#elif DEBUG_PARPORT_IP32 == 2
#	warning DEBUG_PARPORT_IP32 == 2
#elif DEBUG_PARPORT_IP32 >= 3
#	warning DEBUG_PARPORT_IP32 >= 3
#	if !defined(DEBUG)
#		define DEBUG 
#	endif
#endif

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/parport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/ip32/ip32_ints.h>
#include <asm/ip32/mace.h>


#if DEBUG_PARPORT_IP32 >= 1
#	define DEFAULT_VERBOSE_PROBING	1
#else
#	define DEFAULT_VERBOSE_PROBING	0
#endif

#define PPIP32 "parport_ip32: "

#define PARPORT_IP32_ENABLE_IRQ	(1U << 0)
#define PARPORT_IP32_ENABLE_DMA	(1U << 1)
#define PARPORT_IP32_ENABLE_SPP	(1U << 2)
#define PARPORT_IP32_ENABLE_EPP	(1U << 3)
#define PARPORT_IP32_ENABLE_ECP	(1U << 4)
static unsigned int features =	~0U;
static bool verbose_probing =	DEFAULT_VERBOSE_PROBING;

static struct parport *this_port = NULL;

#define FIFO_NFAULT_TIMEOUT	100	
#define FIFO_POLLING_INTERVAL	50	


struct parport_ip32_regs {
	void __iomem *data;
	void __iomem *dsr;
	void __iomem *dcr;
	void __iomem *eppAddr;
	void __iomem *eppData0;
	void __iomem *eppData1;
	void __iomem *eppData2;
	void __iomem *eppData3;
	void __iomem *ecpAFifo;
	void __iomem *fifo;
	void __iomem *cnfgA;
	void __iomem *cnfgB;
	void __iomem *ecr;
};

#define DSR_nBUSY		(1U << 7)	
#define DSR_nACK		(1U << 6)	
#define DSR_PERROR		(1U << 5)	
#define DSR_SELECT		(1U << 4)	
#define DSR_nFAULT		(1U << 3)	
#define DSR_nPRINT		(1U << 2)	
#define DSR_TIMEOUT		(1U << 0)	

#define DCR_DIR			(1U << 5)	
#define DCR_IRQ			(1U << 4)	
#define DCR_SELECT		(1U << 3)	
#define DCR_nINIT		(1U << 2)	
#define DCR_AUTOFD		(1U << 1)	
#define DCR_STROBE		(1U << 0)	

#define CNFGA_IRQ		(1U << 7)
#define CNFGA_ID_MASK		((1U << 6) | (1U << 5) | (1U << 4))
#define CNFGA_ID_SHIFT		4
#define CNFGA_ID_16		(00U << CNFGA_ID_SHIFT)
#define CNFGA_ID_8		(01U << CNFGA_ID_SHIFT)
#define CNFGA_ID_32		(02U << CNFGA_ID_SHIFT)
#define CNFGA_nBYTEINTRANS	(1U << 2)
#define CNFGA_PWORDLEFT		((1U << 1) | (1U << 0))

#define CNFGB_COMPRESS		(1U << 7)
#define CNFGB_INTRVAL		(1U << 6)
#define CNFGB_IRQ_MASK		((1U << 5) | (1U << 4) | (1U << 3))
#define CNFGB_IRQ_SHIFT		3
#define CNFGB_DMA_MASK		((1U << 2) | (1U << 1) | (1U << 0))
#define CNFGB_DMA_SHIFT		0

#define ECR_MODE_MASK		((1U << 7) | (1U << 6) | (1U << 5))
#define ECR_MODE_SHIFT		5
#define ECR_MODE_SPP		(00U << ECR_MODE_SHIFT)
#define ECR_MODE_PS2		(01U << ECR_MODE_SHIFT)
#define ECR_MODE_PPF		(02U << ECR_MODE_SHIFT)
#define ECR_MODE_ECP		(03U << ECR_MODE_SHIFT)
#define ECR_MODE_EPP		(04U << ECR_MODE_SHIFT)
#define ECR_MODE_TST		(06U << ECR_MODE_SHIFT)
#define ECR_MODE_CFG		(07U << ECR_MODE_SHIFT)
#define ECR_nERRINTR		(1U << 4)
#define ECR_DMAEN		(1U << 3)
#define ECR_SERVINTR		(1U << 2)
#define ECR_F_FULL		(1U << 1)
#define ECR_F_EMPTY		(1U << 0)


enum parport_ip32_irq_mode { PARPORT_IP32_IRQ_FWD, PARPORT_IP32_IRQ_HERE };

struct parport_ip32_private {
	struct parport_ip32_regs	regs;
	unsigned int			dcr_cache;
	unsigned int			dcr_writable;
	unsigned int			pword;
	unsigned int			fifo_depth;
	unsigned int			readIntrThreshold;
	unsigned int			writeIntrThreshold;
	enum parport_ip32_irq_mode	irq_mode;
	struct completion		irq_complete;
};


#if DEBUG_PARPORT_IP32 >= 1
#	define pr_debug1(...)	printk(KERN_DEBUG __VA_ARGS__)
#else 
#	define pr_debug1(...)	do { } while (0)
#endif

#define __pr_trace(pr, p, fmt, ...)					\
	pr("%s: %s" fmt "\n",						\
	   ({ const struct parport *__p = (p);				\
		   __p ? __p->name : "parport_ip32"; }),		\
	   __func__ , ##__VA_ARGS__)
#define pr_trace(p, fmt, ...)	__pr_trace(pr_debug, p, fmt , ##__VA_ARGS__)
#define pr_trace1(p, fmt, ...)	__pr_trace(pr_debug1, p, fmt , ##__VA_ARGS__)

#define __pr_probe(...)							\
	do { if (verbose_probing) printk(__VA_ARGS__); } while (0)
#define pr_probe(p, fmt, ...)						\
	__pr_probe(KERN_INFO PPIP32 "0x%lx: " fmt, (p)->base , ##__VA_ARGS__)

#if DEBUG_PARPORT_IP32 >= 2
static void parport_ip32_dump_state(struct parport *p, char *str,
				    unsigned int show_ecp_config)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	unsigned int i;

	printk(KERN_DEBUG PPIP32 "%s: state (%s):\n", p->name, str);
	{
		static const char ecr_modes[8][4] = {"SPP", "PS2", "PPF",
						     "ECP", "EPP", "???",
						     "TST", "CFG"};
		unsigned int ecr = readb(priv->regs.ecr);
		printk(KERN_DEBUG PPIP32 "    ecr=0x%02x", ecr);
		printk(" %s",
		       ecr_modes[(ecr & ECR_MODE_MASK) >> ECR_MODE_SHIFT]);
		if (ecr & ECR_nERRINTR)
			printk(",nErrIntrEn");
		if (ecr & ECR_DMAEN)
			printk(",dmaEn");
		if (ecr & ECR_SERVINTR)
			printk(",serviceIntr");
		if (ecr & ECR_F_FULL)
			printk(",f_full");
		if (ecr & ECR_F_EMPTY)
			printk(",f_empty");
		printk("\n");
	}
	if (show_ecp_config) {
		unsigned int oecr, cnfgA, cnfgB;
		oecr = readb(priv->regs.ecr);
		writeb(ECR_MODE_PS2, priv->regs.ecr);
		writeb(ECR_MODE_CFG, priv->regs.ecr);
		cnfgA = readb(priv->regs.cnfgA);
		cnfgB = readb(priv->regs.cnfgB);
		writeb(ECR_MODE_PS2, priv->regs.ecr);
		writeb(oecr, priv->regs.ecr);
		printk(KERN_DEBUG PPIP32 "    cnfgA=0x%02x", cnfgA);
		printk(" ISA-%s", (cnfgA & CNFGA_IRQ) ? "Level" : "Pulses");
		switch (cnfgA & CNFGA_ID_MASK) {
		case CNFGA_ID_8:
			printk(",8 bits");
			break;
		case CNFGA_ID_16:
			printk(",16 bits");
			break;
		case CNFGA_ID_32:
			printk(",32 bits");
			break;
		default:
			printk(",unknown ID");
			break;
		}
		if (!(cnfgA & CNFGA_nBYTEINTRANS))
			printk(",ByteInTrans");
		if ((cnfgA & CNFGA_ID_MASK) != CNFGA_ID_8)
			printk(",%d byte%s left", cnfgA & CNFGA_PWORDLEFT,
			       ((cnfgA & CNFGA_PWORDLEFT) > 1) ? "s" : "");
		printk("\n");
		printk(KERN_DEBUG PPIP32 "    cnfgB=0x%02x", cnfgB);
		printk(" irq=%u,dma=%u",
		       (cnfgB & CNFGB_IRQ_MASK) >> CNFGB_IRQ_SHIFT,
		       (cnfgB & CNFGB_DMA_MASK) >> CNFGB_DMA_SHIFT);
		printk(",intrValue=%d", !!(cnfgB & CNFGB_INTRVAL));
		if (cnfgB & CNFGB_COMPRESS)
			printk(",compress");
		printk("\n");
	}
	for (i = 0; i < 2; i++) {
		unsigned int dcr = i ? priv->dcr_cache : readb(priv->regs.dcr);
		printk(KERN_DEBUG PPIP32 "    dcr(%s)=0x%02x",
		       i ? "soft" : "hard", dcr);
		printk(" %s", (dcr & DCR_DIR) ? "rev" : "fwd");
		if (dcr & DCR_IRQ)
			printk(",ackIntEn");
		if (!(dcr & DCR_SELECT))
			printk(",nSelectIn");
		if (dcr & DCR_nINIT)
			printk(",nInit");
		if (!(dcr & DCR_AUTOFD))
			printk(",nAutoFD");
		if (!(dcr & DCR_STROBE))
			printk(",nStrobe");
		printk("\n");
	}
#define sep (f++ ? ',' : ' ')
	{
		unsigned int f = 0;
		unsigned int dsr = readb(priv->regs.dsr);
		printk(KERN_DEBUG PPIP32 "    dsr=0x%02x", dsr);
		if (!(dsr & DSR_nBUSY))
			printk("%cBusy", sep);
		if (dsr & DSR_nACK)
			printk("%cnAck", sep);
		if (dsr & DSR_PERROR)
			printk("%cPError", sep);
		if (dsr & DSR_SELECT)
			printk("%cSelect", sep);
		if (dsr & DSR_nFAULT)
			printk("%cnFault", sep);
		if (!(dsr & DSR_nPRINT))
			printk("%c(Print)", sep);
		if (dsr & DSR_TIMEOUT)
			printk("%cTimeout", sep);
		printk("\n");
	}
#undef sep
}
#else 
#define parport_ip32_dump_state(...)	do { } while (0)
#endif

#if DEBUG_PARPORT_IP32 >= 1
#define CHECK_EXTRA_BITS(p, b, m)					\
	do {								\
		unsigned int __b = (b), __m = (m);			\
		if (__b & ~__m)						\
			pr_debug1(PPIP32 "%s: extra bits in %s(%s): "	\
				  "0x%02x/0x%02x\n",			\
				  (p)->name, __func__, #b, __b, __m);	\
	} while (0)
#else 
#define CHECK_EXTRA_BITS(...)	do { } while (0)
#endif


struct parport_ip32_dma_data {
	enum dma_data_direction		dir;
	dma_addr_t			buf;
	dma_addr_t			next;
	size_t				len;
	size_t				left;
	unsigned int			ctx;
	unsigned int			irq_on;
	spinlock_t			lock;
};
static struct parport_ip32_dma_data parport_ip32_dma;

static void parport_ip32_dma_setup_context(unsigned int limit)
{
	unsigned long flags;

	spin_lock_irqsave(&parport_ip32_dma.lock, flags);
	if (parport_ip32_dma.left > 0) {
		volatile u64 __iomem *ctxreg = (parport_ip32_dma.ctx == 0) ?
			&mace->perif.ctrl.parport.context_a :
			&mace->perif.ctrl.parport.context_b;
		u64 count;
		u64 ctxval;
		if (parport_ip32_dma.left <= limit) {
			count = parport_ip32_dma.left;
			ctxval = MACEPAR_CONTEXT_LASTFLAG;
		} else {
			count = limit;
			ctxval = 0;
		}

		pr_trace(NULL,
			 "(%u): 0x%04x:0x%04x, %u -> %u%s",
			 limit,
			 (unsigned int)parport_ip32_dma.buf,
			 (unsigned int)parport_ip32_dma.next,
			 (unsigned int)count,
			 parport_ip32_dma.ctx, ctxval ? "*" : "");

		ctxval |= parport_ip32_dma.next &
			MACEPAR_CONTEXT_BASEADDR_MASK;
		ctxval |= ((count - 1) << MACEPAR_CONTEXT_DATALEN_SHIFT) &
			MACEPAR_CONTEXT_DATALEN_MASK;
		writeq(ctxval, ctxreg);
		parport_ip32_dma.next += count;
		parport_ip32_dma.left -= count;
		parport_ip32_dma.ctx ^= 1U;
	}
	if (parport_ip32_dma.left == 0 && parport_ip32_dma.irq_on) {
		pr_debug(PPIP32 "IRQ off (ctx)\n");
		disable_irq_nosync(MACEISA_PAR_CTXA_IRQ);
		disable_irq_nosync(MACEISA_PAR_CTXB_IRQ);
		parport_ip32_dma.irq_on = 0;
	}
	spin_unlock_irqrestore(&parport_ip32_dma.lock, flags);
}

static irqreturn_t parport_ip32_dma_interrupt(int irq, void *dev_id)
{
	if (parport_ip32_dma.left)
		pr_trace(NULL, "(%d): ctx=%d", irq, parport_ip32_dma.ctx);
	parport_ip32_dma_setup_context(MACEPAR_CONTEXT_DATA_BOUND);
	return IRQ_HANDLED;
}

#if DEBUG_PARPORT_IP32
static irqreturn_t parport_ip32_merr_interrupt(int irq, void *dev_id)
{
	pr_trace1(NULL, "(%d)", irq);
	return IRQ_HANDLED;
}
#endif

static int parport_ip32_dma_start(enum dma_data_direction dir,
				  void *addr, size_t count)
{
	unsigned int limit;
	u64 ctrl;

	pr_trace(NULL, "(%d, %lu)", dir, (unsigned long)count);

	BUG_ON(dir != DMA_TO_DEVICE);

	
	ctrl = MACEPAR_CTLSTAT_RESET;
	writeq(ctrl, &mace->perif.ctrl.parport.cntlstat);

	
	if (!parport_ip32_dma.irq_on) {
		WARN_ON(1);
		enable_irq(MACEISA_PAR_CTXA_IRQ);
		enable_irq(MACEISA_PAR_CTXB_IRQ);
		parport_ip32_dma.irq_on = 1;
	}

	
	parport_ip32_dma.dir = dir;
	parport_ip32_dma.buf = dma_map_single(NULL, addr, count, dir);
	parport_ip32_dma.len = count;
	parport_ip32_dma.next = parport_ip32_dma.buf;
	parport_ip32_dma.left = parport_ip32_dma.len;
	parport_ip32_dma.ctx = 0;

	
	ctrl = (dir == DMA_TO_DEVICE) ? 0 : MACEPAR_CTLSTAT_DIRECTION;
	writeq(ctrl, &mace->perif.ctrl.parport.cntlstat);
	
	limit = MACEPAR_CONTEXT_DATA_BOUND -
		(parport_ip32_dma.next & (MACEPAR_CONTEXT_DATA_BOUND - 1));
	parport_ip32_dma_setup_context(limit);
	parport_ip32_dma_setup_context(MACEPAR_CONTEXT_DATA_BOUND);

	
	ctrl |= MACEPAR_CTLSTAT_ENABLE;
	writeq(ctrl, &mace->perif.ctrl.parport.cntlstat);

	return 0;
}

static void parport_ip32_dma_stop(void)
{
	u64 ctx_a;
	u64 ctx_b;
	u64 ctrl;
	u64 diag;
	size_t res[2];	

	pr_trace(NULL, "()");

	
	spin_lock_irq(&parport_ip32_dma.lock);
	if (parport_ip32_dma.irq_on) {
		pr_debug(PPIP32 "IRQ off (stop)\n");
		disable_irq_nosync(MACEISA_PAR_CTXA_IRQ);
		disable_irq_nosync(MACEISA_PAR_CTXB_IRQ);
		parport_ip32_dma.irq_on = 0;
	}
	spin_unlock_irq(&parport_ip32_dma.lock);
	synchronize_irq(MACEISA_PAR_CTXA_IRQ);
	synchronize_irq(MACEISA_PAR_CTXB_IRQ);

	
	ctrl = readq(&mace->perif.ctrl.parport.cntlstat);
	ctrl &= ~MACEPAR_CTLSTAT_ENABLE;
	writeq(ctrl, &mace->perif.ctrl.parport.cntlstat);

	
	ctx_a = readq(&mace->perif.ctrl.parport.context_a);
	ctx_b = readq(&mace->perif.ctrl.parport.context_b);
	ctrl = readq(&mace->perif.ctrl.parport.cntlstat);
	diag = readq(&mace->perif.ctrl.parport.diagnostic);
	res[0] = (ctrl & MACEPAR_CTLSTAT_CTXA_VALID) ?
		1 + ((ctx_a & MACEPAR_CONTEXT_DATALEN_MASK) >>
		     MACEPAR_CONTEXT_DATALEN_SHIFT) :
		0;
	res[1] = (ctrl & MACEPAR_CTLSTAT_CTXB_VALID) ?
		1 + ((ctx_b & MACEPAR_CONTEXT_DATALEN_MASK) >>
		     MACEPAR_CONTEXT_DATALEN_SHIFT) :
		0;
	if (diag & MACEPAR_DIAG_DMACTIVE)
		res[(diag & MACEPAR_DIAG_CTXINUSE) != 0] =
			1 + ((diag & MACEPAR_DIAG_CTRMASK) >>
			     MACEPAR_DIAG_CTRSHIFT);
	parport_ip32_dma.left += res[0] + res[1];

	
	ctrl = MACEPAR_CTLSTAT_RESET;
	writeq(ctrl, &mace->perif.ctrl.parport.cntlstat);
	pr_debug(PPIP32 "IRQ on (stop)\n");
	enable_irq(MACEISA_PAR_CTXA_IRQ);
	enable_irq(MACEISA_PAR_CTXB_IRQ);
	parport_ip32_dma.irq_on = 1;

	dma_unmap_single(NULL, parport_ip32_dma.buf, parport_ip32_dma.len,
			 parport_ip32_dma.dir);
}

static inline size_t parport_ip32_dma_get_residue(void)
{
	return parport_ip32_dma.left;
}

static int parport_ip32_dma_register(void)
{
	int err;

	spin_lock_init(&parport_ip32_dma.lock);
	parport_ip32_dma.irq_on = 1;

	
	writeq(MACEPAR_CTLSTAT_RESET, &mace->perif.ctrl.parport.cntlstat);

	
	err = request_irq(MACEISA_PAR_CTXA_IRQ, parport_ip32_dma_interrupt,
			  0, "parport_ip32", NULL);
	if (err)
		goto fail_a;
	err = request_irq(MACEISA_PAR_CTXB_IRQ, parport_ip32_dma_interrupt,
			  0, "parport_ip32", NULL);
	if (err)
		goto fail_b;
#if DEBUG_PARPORT_IP32
	
	err = request_irq(MACEISA_PAR_MERR_IRQ, parport_ip32_merr_interrupt,
			  0, "parport_ip32", NULL);
	if (err)
		goto fail_merr;
#endif
	return 0;

#if DEBUG_PARPORT_IP32
fail_merr:
	free_irq(MACEISA_PAR_CTXB_IRQ, NULL);
#endif
fail_b:
	free_irq(MACEISA_PAR_CTXA_IRQ, NULL);
fail_a:
	return err;
}

static void parport_ip32_dma_unregister(void)
{
#if DEBUG_PARPORT_IP32
	free_irq(MACEISA_PAR_MERR_IRQ, NULL);
#endif
	free_irq(MACEISA_PAR_CTXB_IRQ, NULL);
	free_irq(MACEISA_PAR_CTXA_IRQ, NULL);
}


static inline void parport_ip32_wakeup(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	complete(&priv->irq_complete);
}

static irqreturn_t parport_ip32_interrupt(int irq, void *dev_id)
{
	struct parport * const p = dev_id;
	struct parport_ip32_private * const priv = p->physport->private_data;
	enum parport_ip32_irq_mode irq_mode = priv->irq_mode;

	switch (irq_mode) {
	case PARPORT_IP32_IRQ_FWD:
		return parport_irq_handler(irq, dev_id);

	case PARPORT_IP32_IRQ_HERE:
		parport_ip32_wakeup(p);
		break;
	}

	return IRQ_HANDLED;
}


static inline unsigned int parport_ip32_read_econtrol(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return readb(priv->regs.ecr);
}

static inline void parport_ip32_write_econtrol(struct parport *p,
					       unsigned int c)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	writeb(c, priv->regs.ecr);
}

static inline void parport_ip32_frob_econtrol(struct parport *p,
					      unsigned int mask,
					      unsigned int val)
{
	unsigned int c;
	c = (parport_ip32_read_econtrol(p) & ~mask) ^ val;
	parport_ip32_write_econtrol(p, c);
}

static void parport_ip32_set_mode(struct parport *p, unsigned int mode)
{
	unsigned int omode;

	mode &= ECR_MODE_MASK;
	omode = parport_ip32_read_econtrol(p) & ECR_MODE_MASK;

	if (!(mode == ECR_MODE_SPP || mode == ECR_MODE_PS2
	      || omode == ECR_MODE_SPP || omode == ECR_MODE_PS2)) {
		
		unsigned int ecr = ECR_MODE_PS2 | ECR_nERRINTR | ECR_SERVINTR;
		parport_ip32_write_econtrol(p, ecr);
	}
	parport_ip32_write_econtrol(p, mode | ECR_nERRINTR | ECR_SERVINTR);
}


static inline unsigned char parport_ip32_read_data(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return readb(priv->regs.data);
}

static inline void parport_ip32_write_data(struct parport *p, unsigned char d)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	writeb(d, priv->regs.data);
}

static inline unsigned char parport_ip32_read_status(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return readb(priv->regs.dsr);
}

static inline unsigned int __parport_ip32_read_control(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return priv->dcr_cache; 
}

static inline void __parport_ip32_write_control(struct parport *p,
						unsigned int c)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	CHECK_EXTRA_BITS(p, c, priv->dcr_writable);
	c &= priv->dcr_writable; 
	writeb(c, priv->regs.dcr);
	priv->dcr_cache = c;		
}

static inline void __parport_ip32_frob_control(struct parport *p,
					       unsigned int mask,
					       unsigned int val)
{
	unsigned int c;
	c = (__parport_ip32_read_control(p) & ~mask) ^ val;
	__parport_ip32_write_control(p, c);
}

static inline unsigned char parport_ip32_read_control(struct parport *p)
{
	const unsigned int rm =
		DCR_STROBE | DCR_AUTOFD | DCR_nINIT | DCR_SELECT;
	return __parport_ip32_read_control(p) & rm;
}

static inline void parport_ip32_write_control(struct parport *p,
					      unsigned char c)
{
	const unsigned int wm =
		DCR_STROBE | DCR_AUTOFD | DCR_nINIT | DCR_SELECT;
	CHECK_EXTRA_BITS(p, c, wm);
	__parport_ip32_frob_control(p, wm, c & wm);
}

static inline unsigned char parport_ip32_frob_control(struct parport *p,
						      unsigned char mask,
						      unsigned char val)
{
	const unsigned int wm =
		DCR_STROBE | DCR_AUTOFD | DCR_nINIT | DCR_SELECT;
	CHECK_EXTRA_BITS(p, mask, wm);
	CHECK_EXTRA_BITS(p, val, wm);
	__parport_ip32_frob_control(p, mask & wm, val & wm);
	return parport_ip32_read_control(p);
}

static inline void parport_ip32_disable_irq(struct parport *p)
{
	__parport_ip32_frob_control(p, DCR_IRQ, 0);
}

static inline void parport_ip32_enable_irq(struct parport *p)
{
	__parport_ip32_frob_control(p, DCR_IRQ, DCR_IRQ);
}

static inline void parport_ip32_data_forward(struct parport *p)
{
	__parport_ip32_frob_control(p, DCR_DIR, 0);
}

static inline void parport_ip32_data_reverse(struct parport *p)
{
	__parport_ip32_frob_control(p, DCR_DIR, DCR_DIR);
}

static void parport_ip32_init_state(struct pardevice *dev,
				    struct parport_state *s)
{
	s->u.ip32.dcr = DCR_SELECT | DCR_nINIT;
	s->u.ip32.ecr = ECR_MODE_PS2 | ECR_nERRINTR | ECR_SERVINTR;
}

static void parport_ip32_save_state(struct parport *p,
				    struct parport_state *s)
{
	s->u.ip32.dcr = __parport_ip32_read_control(p);
	s->u.ip32.ecr = parport_ip32_read_econtrol(p);
}

static void parport_ip32_restore_state(struct parport *p,
				       struct parport_state *s)
{
	parport_ip32_set_mode(p, s->u.ip32.ecr & ECR_MODE_MASK);
	parport_ip32_write_econtrol(p, s->u.ip32.ecr);
	__parport_ip32_write_control(p, s->u.ip32.dcr);
}


static unsigned int parport_ip32_clear_epp_timeout(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	unsigned int cleared;

	if (!(parport_ip32_read_status(p) & DSR_TIMEOUT))
		cleared = 1;
	else {
		unsigned int r;
		
		parport_ip32_read_status(p);
		r = parport_ip32_read_status(p);
		
		writeb(r | DSR_TIMEOUT, priv->regs.dsr);
		
		writeb(r & ~DSR_TIMEOUT, priv->regs.dsr);

		r = parport_ip32_read_status(p);
		cleared = !(r & DSR_TIMEOUT);
	}

	pr_trace(p, "(): %s", cleared ? "cleared" : "failed");
	return cleared;
}

static size_t parport_ip32_epp_read(void __iomem *eppreg,
				    struct parport *p, void *buf,
				    size_t len, int flags)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	size_t got;
	parport_ip32_set_mode(p, ECR_MODE_EPP);
	parport_ip32_data_reverse(p);
	parport_ip32_write_control(p, DCR_nINIT);
	if ((flags & PARPORT_EPP_FAST) && (len > 1)) {
		readsb(eppreg, buf, len);
		if (readb(priv->regs.dsr) & DSR_TIMEOUT) {
			parport_ip32_clear_epp_timeout(p);
			return -EIO;
		}
		got = len;
	} else {
		u8 *bufp = buf;
		for (got = 0; got < len; got++) {
			*bufp++ = readb(eppreg);
			if (readb(priv->regs.dsr) & DSR_TIMEOUT) {
				parport_ip32_clear_epp_timeout(p);
				break;
			}
		}
	}
	parport_ip32_data_forward(p);
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	return got;
}

static size_t parport_ip32_epp_write(void __iomem *eppreg,
				     struct parport *p, const void *buf,
				     size_t len, int flags)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	size_t written;
	parport_ip32_set_mode(p, ECR_MODE_EPP);
	parport_ip32_data_forward(p);
	parport_ip32_write_control(p, DCR_nINIT);
	if ((flags & PARPORT_EPP_FAST) && (len > 1)) {
		writesb(eppreg, buf, len);
		if (readb(priv->regs.dsr) & DSR_TIMEOUT) {
			parport_ip32_clear_epp_timeout(p);
			return -EIO;
		}
		written = len;
	} else {
		const u8 *bufp = buf;
		for (written = 0; written < len; written++) {
			writeb(*bufp++, eppreg);
			if (readb(priv->regs.dsr) & DSR_TIMEOUT) {
				parport_ip32_clear_epp_timeout(p);
				break;
			}
		}
	}
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	return written;
}

static size_t parport_ip32_epp_read_data(struct parport *p, void *buf,
					 size_t len, int flags)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return parport_ip32_epp_read(priv->regs.eppData0, p, buf, len, flags);
}

static size_t parport_ip32_epp_write_data(struct parport *p, const void *buf,
					  size_t len, int flags)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return parport_ip32_epp_write(priv->regs.eppData0, p, buf, len, flags);
}

static size_t parport_ip32_epp_read_addr(struct parport *p, void *buf,
					 size_t len, int flags)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return parport_ip32_epp_read(priv->regs.eppAddr, p, buf, len, flags);
}

static size_t parport_ip32_epp_write_addr(struct parport *p, const void *buf,
					  size_t len, int flags)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	return parport_ip32_epp_write(priv->regs.eppAddr, p, buf, len, flags);
}


static unsigned int parport_ip32_fifo_wait_break(struct parport *p,
						 unsigned long expire)
{
	cond_resched();
	if (time_after(jiffies, expire)) {
		pr_debug1(PPIP32 "%s: FIFO write timed out\n", p->name);
		return 1;
	}
	if (signal_pending(current)) {
		pr_debug1(PPIP32 "%s: Signal pending\n", p->name);
		return 1;
	}
	if (!(parport_ip32_read_status(p) & DSR_nFAULT)) {
		pr_debug1(PPIP32 "%s: nFault asserted low\n", p->name);
		return 1;
	}
	return 0;
}

/**
 * parport_ip32_fwp_wait_polling - wait for FIFO to empty (polling)
 * @p:		pointer to &struct parport
 *
 * Returns the number of bytes that can safely be written in the FIFO.  A
 * return value of zero means that the calling function should terminate as
 * fast as possible.
 */
static unsigned int parport_ip32_fwp_wait_polling(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	struct parport * const physport = p->physport;
	unsigned long expire;
	unsigned int count;
	unsigned int ecr;

	expire = jiffies + physport->cad->timeout;
	count = 0;
	while (1) {
		if (parport_ip32_fifo_wait_break(p, expire))
			break;

		ecr = parport_ip32_read_econtrol(p);
		if (ecr & ECR_F_EMPTY) {
			
			count = priv->fifo_depth;
			break;
		}

		
		udelay(FIFO_POLLING_INTERVAL);
	} 

	return count;
}

/**
 * parport_ip32_fwp_wait_interrupt - wait for FIFO to empty (interrupt-driven)
 * @p:		pointer to &struct parport
 *
 * Returns the number of bytes that can safely be written in the FIFO.  A
 * return value of zero means that the calling function should terminate as
 * fast as possible.
 */
static unsigned int parport_ip32_fwp_wait_interrupt(struct parport *p)
{
	static unsigned int lost_interrupt = 0;
	struct parport_ip32_private * const priv = p->physport->private_data;
	struct parport * const physport = p->physport;
	unsigned long nfault_timeout;
	unsigned long expire;
	unsigned int count;
	unsigned int ecr;

	nfault_timeout = min((unsigned long)physport->cad->timeout,
			     msecs_to_jiffies(FIFO_NFAULT_TIMEOUT));
	expire = jiffies + physport->cad->timeout;
	count = 0;
	while (1) {
		if (parport_ip32_fifo_wait_break(p, expire))
			break;

		
		INIT_COMPLETION(priv->irq_complete);

		
		parport_ip32_frob_econtrol(p, ECR_SERVINTR, 0);

		ecr = parport_ip32_read_econtrol(p);
		if (!(ecr & ECR_F_EMPTY)) {
			wait_for_completion_interruptible_timeout(
				&priv->irq_complete, nfault_timeout);
			ecr = parport_ip32_read_econtrol(p);
			if ((ecr & ECR_F_EMPTY) && !(ecr & ECR_SERVINTR)
			    && !lost_interrupt) {
				printk(KERN_WARNING PPIP32
				       "%s: lost interrupt in %s\n",
				       p->name, __func__);
				lost_interrupt = 1;
			}
		}

		
		parport_ip32_frob_econtrol(p, ECR_SERVINTR, ECR_SERVINTR);

		
		if (ecr & ECR_F_EMPTY) {
			
			count = priv->fifo_depth;
			break;
		} else if (ecr & ECR_SERVINTR) {
			count = priv->writeIntrThreshold;
			break;
		}
	} 

	return count;
}

/**
 * parport_ip32_fifo_write_block_pio - write a block of data (PIO mode)
 * @p:		pointer to &struct parport
 * @buf:	buffer of data to write
 * @len:	length of buffer @buf
 *
 * Uses PIO to write the contents of the buffer @buf into the parallel port
 * FIFO.  Returns the number of bytes that were actually written.  It can work
 * with or without the help of interrupts.  The parallel port must be
 * correctly initialized before calling parport_ip32_fifo_write_block_pio().
 */
static size_t parport_ip32_fifo_write_block_pio(struct parport *p,
						const void *buf, size_t len)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	const u8 *bufp = buf;
	size_t left = len;

	priv->irq_mode = PARPORT_IP32_IRQ_HERE;

	while (left > 0) {
		unsigned int count;

		count = (p->irq == PARPORT_IRQ_NONE) ?
			parport_ip32_fwp_wait_polling(p) :
			parport_ip32_fwp_wait_interrupt(p);
		if (count == 0)
			break;	
		if (count > left)
			count = left;
		if (count == 1) {
			writeb(*bufp, priv->regs.fifo);
			bufp++, left--;
		} else {
			writesb(priv->regs.fifo, bufp, count);
			bufp += count, left -= count;
		}
	}

	priv->irq_mode = PARPORT_IP32_IRQ_FWD;

	return len - left;
}

/**
 * parport_ip32_fifo_write_block_dma - write a block of data (DMA mode)
 * @p:		pointer to &struct parport
 * @buf:	buffer of data to write
 * @len:	length of buffer @buf
 *
 * Uses DMA to write the contents of the buffer @buf into the parallel port
 * FIFO.  Returns the number of bytes that were actually written.  The
 * parallel port must be correctly initialized before calling
 * parport_ip32_fifo_write_block_dma().
 */
static size_t parport_ip32_fifo_write_block_dma(struct parport *p,
						const void *buf, size_t len)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	struct parport * const physport = p->physport;
	unsigned long nfault_timeout;
	unsigned long expire;
	size_t written;
	unsigned int ecr;

	priv->irq_mode = PARPORT_IP32_IRQ_HERE;

	parport_ip32_dma_start(DMA_TO_DEVICE, (void *)buf, len);
	INIT_COMPLETION(priv->irq_complete);
	parport_ip32_frob_econtrol(p, ECR_DMAEN | ECR_SERVINTR, ECR_DMAEN);

	nfault_timeout = min((unsigned long)physport->cad->timeout,
			     msecs_to_jiffies(FIFO_NFAULT_TIMEOUT));
	expire = jiffies + physport->cad->timeout;
	while (1) {
		if (parport_ip32_fifo_wait_break(p, expire))
			break;
		wait_for_completion_interruptible_timeout(&priv->irq_complete,
							  nfault_timeout);
		ecr = parport_ip32_read_econtrol(p);
		if (ecr & ECR_SERVINTR)
			break;	
	}
	parport_ip32_dma_stop();
	written = len - parport_ip32_dma_get_residue();

	priv->irq_mode = PARPORT_IP32_IRQ_FWD;

	return written;
}

/**
 * parport_ip32_fifo_write_block - write a block of data
 * @p:		pointer to &struct parport
 * @buf:	buffer of data to write
 * @len:	length of buffer @buf
 *
 * Uses PIO or DMA to write the contents of the buffer @buf into the parallel
 * p FIFO.  Returns the number of bytes that were actually written.
 */
static size_t parport_ip32_fifo_write_block(struct parport *p,
					    const void *buf, size_t len)
{
	size_t written = 0;
	if (len)
		written = (p->modes & PARPORT_MODE_DMA) ?
			parport_ip32_fifo_write_block_dma(p, buf, len) :
			parport_ip32_fifo_write_block_pio(p, buf, len);
	return written;
}

static unsigned int parport_ip32_drain_fifo(struct parport *p,
					    unsigned long timeout)
{
	unsigned long expire = jiffies + timeout;
	unsigned int polling_interval;
	unsigned int counter;

	
	for (counter = 0; counter < 40; counter++) {
		if (parport_ip32_read_econtrol(p) & ECR_F_EMPTY)
			break;
		if (time_after(jiffies, expire))
			break;
		if (signal_pending(current))
			break;
		udelay(5);
	}
	polling_interval = 1; 
	while (!(parport_ip32_read_econtrol(p) & ECR_F_EMPTY)) {
		if (time_after_eq(jiffies, expire))
			break;
		msleep_interruptible(polling_interval);
		if (signal_pending(current))
			break;
		if (polling_interval < 128)
			polling_interval *= 2;
	}

	return !!(parport_ip32_read_econtrol(p) & ECR_F_EMPTY);
}

static unsigned int parport_ip32_get_fifo_residue(struct parport *p,
						  unsigned int mode)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	unsigned int residue;
	unsigned int cnfga;

	if (parport_ip32_read_econtrol(p) & ECR_F_EMPTY)
		residue = 0;
	else {
		pr_debug1(PPIP32 "%s: FIFO is stuck\n", p->name);

		parport_ip32_frob_control(p, DCR_STROBE, 0);

		
		for (residue = priv->fifo_depth; residue > 0; residue--) {
			if (parport_ip32_read_econtrol(p) & ECR_F_FULL)
				break;
			writeb(0x00, priv->regs.fifo);
		}
	}
	if (residue)
		pr_debug1(PPIP32 "%s: %d PWord%s left in FIFO\n",
			  p->name, residue,
			  (residue == 1) ? " was" : "s were");

	
	parport_ip32_set_mode(p, ECR_MODE_PS2);

	
	if (mode == ECR_MODE_ECP) {
		parport_ip32_data_reverse(p);
		parport_ip32_frob_control(p, DCR_nINIT, 0);
		if (parport_wait_peripheral(p, DSR_PERROR, 0))
			pr_debug1(PPIP32 "%s: PEerror timeout 1 in %s\n",
				  p->name, __func__);
		parport_ip32_frob_control(p, DCR_STROBE, DCR_STROBE);
		parport_ip32_frob_control(p, DCR_nINIT, DCR_nINIT);
		if (parport_wait_peripheral(p, DSR_PERROR, DSR_PERROR))
			pr_debug1(PPIP32 "%s: PEerror timeout 2 in %s\n",
				  p->name, __func__);
	}

	
	parport_ip32_set_mode(p, ECR_MODE_CFG);
	cnfga = readb(priv->regs.cnfgA);
	if (!(cnfga & CNFGA_nBYTEINTRANS)) {
		pr_debug1(PPIP32 "%s: cnfgA contains 0x%02x\n",
			  p->name, cnfga);
		pr_debug1(PPIP32 "%s: Accounting for extra byte\n",
			  p->name);
		residue++;
	}


	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	parport_ip32_data_forward(p);

	return residue;
}

static size_t parport_ip32_compat_write_data(struct parport *p,
					     const void *buf, size_t len,
					     int flags)
{
	static unsigned int ready_before = 1;
	struct parport_ip32_private * const priv = p->physport->private_data;
	struct parport * const physport = p->physport;
	size_t written = 0;

	if (physport->cad->timeout <= PARPORT_INACTIVITY_O_NONBLOCK)
		return parport_ieee1284_write_compat(p, buf, len, flags);

	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	parport_ip32_write_control(p, DCR_SELECT | DCR_nINIT);
	parport_ip32_data_forward(p);
	parport_ip32_disable_irq(p);
	parport_ip32_set_mode(p, ECR_MODE_PPF);
	physport->ieee1284.phase = IEEE1284_PH_FWD_DATA;

	
	if (parport_wait_peripheral(p, DSR_nBUSY | DSR_nFAULT,
				       DSR_nBUSY | DSR_nFAULT)) {
		
		if (ready_before)
			printk(KERN_INFO PPIP32 "%s: not ready in %s\n",
			       p->name, __func__);
		ready_before = 0;
		goto stop;
	}
	ready_before = 1;

	written = parport_ip32_fifo_write_block(p, buf, len);

	
	parport_ip32_drain_fifo(p, physport->cad->timeout * priv->fifo_depth);

	
	written -= parport_ip32_get_fifo_residue(p, ECR_MODE_PPF);

	
	if (parport_wait_peripheral(p, DSR_nBUSY, DSR_nBUSY))
		printk(KERN_DEBUG PPIP32 "%s: BUSY timeout in %s\n",
		       p->name, __func__);

stop:
	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	physport->ieee1284.phase = IEEE1284_PH_FWD_IDLE;

	return written;
}


static size_t parport_ip32_ecp_write_data(struct parport *p,
					  const void *buf, size_t len,
					  int flags)
{
	static unsigned int ready_before = 1;
	struct parport_ip32_private * const priv = p->physport->private_data;
	struct parport * const physport = p->physport;
	size_t written = 0;

	if (physport->cad->timeout <= PARPORT_INACTIVITY_O_NONBLOCK)
		return parport_ieee1284_ecp_write_data(p, buf, len, flags);

	
	if (physport->ieee1284.phase != IEEE1284_PH_FWD_IDLE) {
		
		parport_ip32_frob_control(p, DCR_nINIT | DCR_AUTOFD,
					     DCR_nINIT | DCR_AUTOFD);

		
		if (parport_wait_peripheral(p, DSR_PERROR, DSR_PERROR)) {
			printk(KERN_DEBUG PPIP32 "%s: PError timeout in %s",
			       p->name, __func__);
			physport->ieee1284.phase = IEEE1284_PH_ECP_DIR_UNKNOWN;
			return 0;
		}
	}

	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	parport_ip32_write_control(p, DCR_SELECT | DCR_nINIT);
	parport_ip32_data_forward(p);
	parport_ip32_disable_irq(p);
	parport_ip32_set_mode(p, ECR_MODE_ECP);
	physport->ieee1284.phase = IEEE1284_PH_FWD_DATA;

	
	if (parport_wait_peripheral(p, DSR_nBUSY | DSR_nFAULT,
				       DSR_nBUSY | DSR_nFAULT)) {
		
		if (ready_before)
			printk(KERN_INFO PPIP32 "%s: not ready in %s\n",
			       p->name, __func__);
		ready_before = 0;
		goto stop;
	}
	ready_before = 1;

	written = parport_ip32_fifo_write_block(p, buf, len);

	
	parport_ip32_drain_fifo(p, physport->cad->timeout * priv->fifo_depth);

	
	written -= parport_ip32_get_fifo_residue(p, ECR_MODE_ECP);

	
	if (parport_wait_peripheral(p, DSR_nBUSY, DSR_nBUSY))
		printk(KERN_DEBUG PPIP32 "%s: BUSY timeout in %s\n",
		       p->name, __func__);

stop:
	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	physport->ieee1284.phase = IEEE1284_PH_FWD_IDLE;

	return written;
}



static __initdata struct parport_operations parport_ip32_ops = {
	.write_data		= parport_ip32_write_data,
	.read_data		= parport_ip32_read_data,

	.write_control		= parport_ip32_write_control,
	.read_control		= parport_ip32_read_control,
	.frob_control		= parport_ip32_frob_control,

	.read_status		= parport_ip32_read_status,

	.enable_irq		= parport_ip32_enable_irq,
	.disable_irq		= parport_ip32_disable_irq,

	.data_forward		= parport_ip32_data_forward,
	.data_reverse		= parport_ip32_data_reverse,

	.init_state		= parport_ip32_init_state,
	.save_state		= parport_ip32_save_state,
	.restore_state		= parport_ip32_restore_state,

	.epp_write_data		= parport_ieee1284_epp_write_data,
	.epp_read_data		= parport_ieee1284_epp_read_data,
	.epp_write_addr		= parport_ieee1284_epp_write_addr,
	.epp_read_addr		= parport_ieee1284_epp_read_addr,

	.ecp_write_data		= parport_ieee1284_ecp_write_data,
	.ecp_read_data		= parport_ieee1284_ecp_read_data,
	.ecp_write_addr		= parport_ieee1284_ecp_write_addr,

	.compat_write_data	= parport_ieee1284_write_compat,
	.nibble_read_data	= parport_ieee1284_read_nibble,
	.byte_read_data		= parport_ieee1284_read_byte,

	.owner			= THIS_MODULE,
};


static __init unsigned int parport_ip32_ecp_supported(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	unsigned int ecr;

	ecr = ECR_MODE_PS2 | ECR_nERRINTR | ECR_SERVINTR;
	writeb(ecr, priv->regs.ecr);
	if (readb(priv->regs.ecr) != (ecr | ECR_F_EMPTY))
		goto fail;

	pr_probe(p, "Found working ECR register\n");
	parport_ip32_set_mode(p, ECR_MODE_SPP);
	parport_ip32_write_control(p, DCR_SELECT | DCR_nINIT);
	return 1;

fail:
	pr_probe(p, "ECR register not found\n");
	return 0;
}

static __init unsigned int parport_ip32_fifo_supported(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	unsigned int configa, configb;
	unsigned int pword;
	unsigned int i;

	
	parport_ip32_set_mode(p, ECR_MODE_CFG);
	configa = readb(priv->regs.cnfgA);
	configb = readb(priv->regs.cnfgB);

	
	switch (configa & CNFGA_ID_MASK) {
	case CNFGA_ID_8:
		pword = 1;
		break;
	case CNFGA_ID_16:
		pword = 2;
		break;
	case CNFGA_ID_32:
		pword = 4;
		break;
	default:
		pr_probe(p, "Unknown implementation ID: 0x%0x\n",
			 (configa & CNFGA_ID_MASK) >> CNFGA_ID_SHIFT);
		goto fail;
		break;
	}
	if (pword != 1) {
		pr_probe(p, "Unsupported PWord size: %u\n", pword);
		goto fail;
	}
	priv->pword = pword;
	pr_probe(p, "PWord is %u bits\n", 8 * priv->pword);

	
	writeb(configb | CNFGB_COMPRESS, priv->regs.cnfgB);
	if (readb(priv->regs.cnfgB) & CNFGB_COMPRESS)
		pr_probe(p, "Hardware compression detected (unsupported)\n");
	writeb(configb & ~CNFGB_COMPRESS, priv->regs.cnfgB);

	
	parport_ip32_set_mode(p, ECR_MODE_TST);

	
	if (!(readb(priv->regs.ecr) & ECR_F_EMPTY)) {
		pr_probe(p, "FIFO not reset\n");
		goto fail;
	}

	
	priv->fifo_depth = 0;
	for (i = 0; i < 1024; i++) {
		if (readb(priv->regs.ecr) & ECR_F_FULL) {
			
			priv->fifo_depth = i;
			break;
		}
		writeb((u8)i, priv->regs.fifo);
	}
	if (i >= 1024) {
		pr_probe(p, "Can't fill FIFO\n");
		goto fail;
	}
	if (!priv->fifo_depth) {
		pr_probe(p, "Can't get FIFO depth\n");
		goto fail;
	}
	pr_probe(p, "FIFO is %u PWords deep\n", priv->fifo_depth);

	
	parport_ip32_frob_econtrol(p, ECR_SERVINTR, 0);

	priv->writeIntrThreshold = 0;
	for (i = 0; i < priv->fifo_depth; i++) {
		if (readb(priv->regs.fifo) != (u8)i) {
			pr_probe(p, "Invalid data in FIFO\n");
			goto fail;
		}
		if (!priv->writeIntrThreshold
		    && readb(priv->regs.ecr) & ECR_SERVINTR)
			
			priv->writeIntrThreshold = i + 1;
		if (i + 1 < priv->fifo_depth
		    && readb(priv->regs.ecr) & ECR_F_EMPTY) {
			
			pr_probe(p, "Data lost in FIFO\n");
			goto fail;
		}
	}
	if (!priv->writeIntrThreshold) {
		pr_probe(p, "Can't get writeIntrThreshold\n");
		goto fail;
	}
	pr_probe(p, "writeIntrThreshold is %u\n", priv->writeIntrThreshold);

	
	if (!(readb(priv->regs.ecr) & ECR_F_EMPTY)) {
		pr_probe(p, "Can't empty FIFO\n");
		goto fail;
	}

	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	
	parport_ip32_data_reverse(p);
	
	parport_ip32_set_mode(p, ECR_MODE_TST);
	
	parport_ip32_frob_econtrol(p, ECR_SERVINTR, 0);

	priv->readIntrThreshold = 0;
	for (i = 0; i < priv->fifo_depth; i++) {
		writeb(0xaa, priv->regs.fifo);
		if (readb(priv->regs.ecr) & ECR_SERVINTR) {
			
			priv->readIntrThreshold = i + 1;
			break;
		}
	}
	if (!priv->readIntrThreshold) {
		pr_probe(p, "Can't get readIntrThreshold\n");
		goto fail;
	}
	pr_probe(p, "readIntrThreshold is %u\n", priv->readIntrThreshold);

	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	parport_ip32_data_forward(p);
	parport_ip32_set_mode(p, ECR_MODE_SPP);
	return 1;

fail:
	priv->fifo_depth = 0;
	parport_ip32_set_mode(p, ECR_MODE_SPP);
	return 0;
}


static void __init
parport_ip32_make_isa_registers(struct parport_ip32_regs *regs,
				void __iomem *base, void __iomem *base_hi,
				unsigned int regshift)
{
#define r_base(offset)    ((u8 __iomem *)base    + ((offset) << regshift))
#define r_base_hi(offset) ((u8 __iomem *)base_hi + ((offset) << regshift))
	*regs = (struct parport_ip32_regs){
		.data		= r_base(0),
		.dsr		= r_base(1),
		.dcr		= r_base(2),
		.eppAddr	= r_base(3),
		.eppData0	= r_base(4),
		.eppData1	= r_base(5),
		.eppData2	= r_base(6),
		.eppData3	= r_base(7),
		.ecpAFifo	= r_base(0),
		.fifo		= r_base_hi(0),
		.cnfgA		= r_base_hi(0),
		.cnfgB		= r_base_hi(1),
		.ecr		= r_base_hi(2)
	};
#undef r_base_hi
#undef r_base
}

static __init struct parport *parport_ip32_probe_port(void)
{
	struct parport_ip32_regs regs;
	struct parport_ip32_private *priv = NULL;
	struct parport_operations *ops = NULL;
	struct parport *p = NULL;
	int err;

	parport_ip32_make_isa_registers(&regs, &mace->isa.parallel,
					&mace->isa.ecp1284, 8 );

	ops = kmalloc(sizeof(struct parport_operations), GFP_KERNEL);
	priv = kmalloc(sizeof(struct parport_ip32_private), GFP_KERNEL);
	p = parport_register_port(0, PARPORT_IRQ_NONE, PARPORT_DMA_NONE, ops);
	if (ops == NULL || priv == NULL || p == NULL) {
		err = -ENOMEM;
		goto fail;
	}
	p->base = MACE_BASE + offsetof(struct sgi_mace, isa.parallel);
	p->base_hi = MACE_BASE + offsetof(struct sgi_mace, isa.ecp1284);
	p->private_data = priv;

	*ops = parport_ip32_ops;
	*priv = (struct parport_ip32_private){
		.regs			= regs,
		.dcr_writable		= DCR_DIR | DCR_SELECT | DCR_nINIT |
					  DCR_AUTOFD | DCR_STROBE,
		.irq_mode		= PARPORT_IP32_IRQ_FWD,
	};
	init_completion(&priv->irq_complete);

	
	if (!parport_ip32_ecp_supported(p)) {
		err = -ENODEV;
		goto fail;
	}
	parport_ip32_dump_state(p, "begin init", 0);

	p->modes = PARPORT_MODE_PCSPP | PARPORT_MODE_SAFEININT;
	p->modes |= PARPORT_MODE_TRISTATE;

	if (!parport_ip32_fifo_supported(p)) {
		printk(KERN_WARNING PPIP32
		       "%s: error: FIFO disabled\n", p->name);
		
		features &= ~PARPORT_IP32_ENABLE_SPP;
		features &= ~PARPORT_IP32_ENABLE_ECP;
		
		features &= ~PARPORT_IP32_ENABLE_DMA;
	}

	
	if (features & PARPORT_IP32_ENABLE_IRQ) {
		int irq = MACEISA_PARALLEL_IRQ;
		if (request_irq(irq, parport_ip32_interrupt, 0, p->name, p)) {
			printk(KERN_WARNING PPIP32
			       "%s: error: IRQ disabled\n", p->name);
			
			features &= ~PARPORT_IP32_ENABLE_DMA;
		} else {
			pr_probe(p, "Interrupt support enabled\n");
			p->irq = irq;
			priv->dcr_writable |= DCR_IRQ;
		}
	}

	
	if (features & PARPORT_IP32_ENABLE_DMA) {
		if (parport_ip32_dma_register())
			printk(KERN_WARNING PPIP32
			       "%s: error: DMA disabled\n", p->name);
		else {
			pr_probe(p, "DMA support enabled\n");
			p->dma = 0; 
			p->modes |= PARPORT_MODE_DMA;
		}
	}

	if (features & PARPORT_IP32_ENABLE_SPP) {
		
		p->ops->compat_write_data = parport_ip32_compat_write_data;
		p->modes |= PARPORT_MODE_COMPAT;
		pr_probe(p, "Hardware support for SPP mode enabled\n");
	}
	if (features & PARPORT_IP32_ENABLE_EPP) {
		
		p->ops->epp_read_data = parport_ip32_epp_read_data;
		p->ops->epp_write_data = parport_ip32_epp_write_data;
		p->ops->epp_read_addr = parport_ip32_epp_read_addr;
		p->ops->epp_write_addr = parport_ip32_epp_write_addr;
		p->modes |= PARPORT_MODE_EPP;
		pr_probe(p, "Hardware support for EPP mode enabled\n");
	}
	if (features & PARPORT_IP32_ENABLE_ECP) {
		
		p->ops->ecp_write_data = parport_ip32_ecp_write_data;
		
		p->modes |= PARPORT_MODE_ECP;
		pr_probe(p, "Hardware support for ECP mode enabled\n");
	}

	
	parport_ip32_set_mode(p, ECR_MODE_PS2);
	parport_ip32_write_control(p, DCR_SELECT | DCR_nINIT);
	parport_ip32_data_forward(p);
	parport_ip32_disable_irq(p);
	parport_ip32_write_data(p, 0x00);
	parport_ip32_dump_state(p, "end init", 0);

	
	printk(KERN_INFO "%s: SGI IP32 at 0x%lx (0x%lx)",
	       p->name, p->base, p->base_hi);
	if (p->irq != PARPORT_IRQ_NONE)
		printk(", irq %d", p->irq);
	printk(" [");
#define printmode(x)	if (p->modes & PARPORT_MODE_##x)		\
				printk("%s%s", f++ ? "," : "", #x)
	{
		unsigned int f = 0;
		printmode(PCSPP);
		printmode(TRISTATE);
		printmode(COMPAT);
		printmode(EPP);
		printmode(ECP);
		printmode(DMA);
	}
#undef printmode
	printk("]\n");

	parport_announce_port(p);
	return p;

fail:
	if (p)
		parport_put_port(p);
	kfree(priv);
	kfree(ops);
	return ERR_PTR(err);
}

static __exit void parport_ip32_unregister_port(struct parport *p)
{
	struct parport_ip32_private * const priv = p->physport->private_data;
	struct parport_operations *ops = p->ops;

	parport_remove_port(p);
	if (p->modes & PARPORT_MODE_DMA)
		parport_ip32_dma_unregister();
	if (p->irq != PARPORT_IRQ_NONE)
		free_irq(p->irq, p);
	parport_put_port(p);
	kfree(priv);
	kfree(ops);
}

static int __init parport_ip32_init(void)
{
	pr_info(PPIP32 "SGI IP32 built-in parallel port driver v0.6\n");
	this_port = parport_ip32_probe_port();
	return IS_ERR(this_port) ? PTR_ERR(this_port) : 0;
}

static void __exit parport_ip32_exit(void)
{
	parport_ip32_unregister_port(this_port);
}


MODULE_AUTHOR("Arnaud Giersch <arnaud.giersch@free.fr>");
MODULE_DESCRIPTION("SGI IP32 built-in parallel port driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.6");		

module_init(parport_ip32_init);
module_exit(parport_ip32_exit);

module_param(verbose_probing, bool, S_IRUGO);
MODULE_PARM_DESC(verbose_probing, "Log chit-chat during initialization");

module_param(features, uint, S_IRUGO);
MODULE_PARM_DESC(features,
		 "Bit mask of features to enable"
		 ", bit 0: IRQ support"
		 ", bit 1: DMA support"
		 ", bit 2: hardware SPP mode"
		 ", bit 3: hardware EPP mode"
		 ", bit 4: hardware ECP mode");

