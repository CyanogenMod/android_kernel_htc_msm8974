/* 
 *  NCR53c406.c
 *  Low-level SCSI driver for NCR53c406a chip.
 *  Copyright (C) 1994, 1995, 1996 Normunds Saumanis (normunds@fi.ibm.com)
 * 
 *  LILO command line usage: ncr53c406a=<PORTBASE>[,<IRQ>[,<FASTPIO>]]
 *  Specify IRQ = 0 for non-interrupt driven mode.
 *  FASTPIO = 1 for fast pio mode, 0 for slow mode.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2, or (at your option) any
 *  later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 */

#define NCR53C406A_DEBUG 0
#define VERBOSE_NCR53C406A_DEBUG 0

#define USE_PIO 1

#define USE_BIOS 0
				
				
				
#define DMA_CHAN  5		

#define USE_FAST_PIO 1


#include <linux/module.h>

#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include "scsi.h"
#include <scsi/scsi_host.h>


#define WATCHDOG 5000000

#define SYNC_MODE 0		

#ifdef DEBUG
#undef NCR53C406A_DEBUG
#define NCR53C406A_DEBUG 1
#endif

#if USE_PIO
#define USE_DMA 0
#else
#define USE_DMA 1
#endif

#define C1_IMG   0x07		
#define C2_IMG   0x48		
#if USE_DMA
#define C3_IMG   0x21		
#else
#define C3_IMG   0x20		
#endif
#define C4_IMG   0x04		
#define C5_IMG   0xb6		

#define REG0 (outb(C4_IMG, CONFIG4))
#define REG1 (outb(C5_IMG, CONFIG5))

#if NCR53C406A_DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

#if VERBOSE_NCR53C406A_DEBUG
#define VDEB(x) x
#else
#define VDEB(x)
#endif

#define LOAD_DMA_COUNT(count) \
  outb(count & 0xff, TC_LSB); \
  outb((count >> 8) & 0xff, TC_MSB); \
  outb((count >> 16) & 0xff, TC_HIGH);

#define DMA_OP               0x80

#define SCSI_NOP             0x00
#define FLUSH_FIFO           0x01
#define CHIP_RESET           0x02
#define SCSI_RESET           0x03
#define RESELECT             0x40
#define SELECT_NO_ATN        0x41
#define SELECT_ATN           0x42
#define SELECT_ATN_STOP      0x43
#define ENABLE_SEL           0x44
#define DISABLE_SEL          0x45
#define SELECT_ATN3          0x46
#define RESELECT3            0x47
#define TRANSFER_INFO        0x10
#define INIT_CMD_COMPLETE    0x11
#define MSG_ACCEPT           0x12
#define TRANSFER_PAD         0x18
#define SET_ATN              0x1a
#define RESET_ATN            0x1b
#define SEND_MSG             0x20
#define SEND_STATUS          0x21
#define SEND_DATA            0x22
#define DISCONN_SEQ          0x23
#define TERMINATE_SEQ        0x24
#define TARG_CMD_COMPLETE    0x25
#define DISCONN              0x27
#define RECV_MSG             0x28
#define RECV_CMD             0x29
#define RECV_DATA            0x2a
#define RECV_CMD_SEQ         0x2b
#define TARGET_ABORT_DMA     0x04


#if NCR53C406A_DEBUG
#define rtrc(i) {inb(0x3da);outb(0x31,0x3c0);outb((i),0x3c0);}
#else
#define rtrc(i) {}
#endif

enum Phase {
	idle,
	data_out,
	data_in,
	command_ph,
	status_ph,
	message_out,
	message_in
};

static void NCR53c406a_intr(void *);
static irqreturn_t do_NCR53c406a_intr(int, void *);
static void chip_init(void);
static void calc_port_addr(void);
#ifndef IRQ_LEV
static int irq_probe(void);
#endif


#if USE_BIOS
static void *bios_base;
#endif

#ifdef PORT_BASE
static int port_base = PORT_BASE;
#else
static int port_base;
#endif

#ifdef IRQ_LEV
static int irq_level = IRQ_LEV;
#else
static int irq_level = -1;	
#endif

#if USE_DMA
static int dma_chan;
#endif

#if USE_PIO
static int fast_pio = USE_FAST_PIO;
#endif

static Scsi_Cmnd *current_SC;
static char info_msg[256];


#if USE_BIOS
static void *addresses[] = {
	(void *) 0xd8000,
	(void *) 0xc8000
};
#define ADDRESS_COUNT ARRAY_SIZE(addresses)
#endif				

static unsigned short ports[] = { 0x230, 0x330, 0x280, 0x290, 0x330, 0x340, 0x300, 0x310, 0x348, 0x350 };
#define PORT_COUNT ARRAY_SIZE(ports)

#ifndef MODULE
static unsigned short intrs[] = { 10, 11, 12, 15 };
#define INTR_COUNT ARRAY_SIZE(intrs)
#endif 

#if USE_BIOS
struct signature {
	char *signature;
	int sig_offset;
	int sig_length;
} signatures[] __initdata = {
	
	
	{
"Copyright (C) Acculogic, Inc.\r\n2.8M Diskette Extension Bios ver 4.04.03 03/01/1993", 61, 82},};

#define SIGNATURE_COUNT ARRAY_SIZE(signatures)
#endif				


static int TC_LSB;		
static int TC_MSB;		
static int SCSI_FIFO;		
static int CMD_REG;		
static int STAT_REG;		
static int DEST_ID;		
static int INT_REG;		
static int SRTIMOUT;		
static int SEQ_REG;		
static int SYNCPRD;		
static int FIFO_FLAGS;		
static int SYNCOFF;		
static int CONFIG1;		
static int CLKCONV;		
				
static int CONFIG2;		
static int CONFIG3;		
static int CONFIG4;		
static int TC_HIGH;		
				

				
				
				
static int PIO_FIFO;		
				
				
				
static int PIO_STATUS;		
				
				
static int PIO_FLAG;		
static int CONFIG5;		
				
				


#if USE_DMA
static __inline__ int NCR53c406a_dma_setup(unsigned char *ptr, unsigned int count, unsigned char mode)
{
	unsigned limit;
	unsigned long flags = 0;

	VDEB(printk("dma: before count=%d   ", count));
	if (dma_chan <= 3) {
		if (count > 65536)
			count = 65536;
		limit = 65536 - (((unsigned) ptr) & 0xFFFF);
	} else {
		if (count > (65536 << 1))
			count = (65536 << 1);
		limit = (65536 << 1) - (((unsigned) ptr) & 0x1FFFF);
	}

	if (count > limit)
		count = limit;

	VDEB(printk("after count=%d\n", count));
	if ((count & 1) || (((unsigned) ptr) & 1))
		panic("NCR53c406a: attempted unaligned DMA transfer\n");

	flags = claim_dma_lock();
	disable_dma(dma_chan);
	clear_dma_ff(dma_chan);
	set_dma_addr(dma_chan, (long) ptr);
	set_dma_count(dma_chan, count);
	set_dma_mode(dma_chan, mode);
	enable_dma(dma_chan);
	release_dma_lock(flags);

	return count;
}

static __inline__ int NCR53c406a_dma_write(unsigned char *src, unsigned int count)
{
	return NCR53c406a_dma_setup(src, count, DMA_MODE_WRITE);
}

static __inline__ int NCR53c406a_dma_read(unsigned char *src, unsigned int count)
{
	return NCR53c406a_dma_setup(src, count, DMA_MODE_READ);
}

static __inline__ int NCR53c406a_dma_residual(void)
{
	register int tmp;
	unsigned long flags;

	flags = claim_dma_lock();
	clear_dma_ff(dma_chan);
	tmp = get_dma_residue(dma_chan);
	release_dma_lock(flags);

	return tmp;
}
#endif				

#if USE_PIO
static __inline__ int NCR53c406a_pio_read(unsigned char *request, unsigned int reqlen)
{
	int i;
	int len;		

	REG1;
	while (reqlen) {
		i = inb(PIO_STATUS);
		
		if (i & 0x80)
			return 0;

		switch (i & 0x1e) {
		default:
		case 0x10:
			len = 0;
			break;
		case 0x0:
			len = 1;
			break;
		case 0x8:
			len = 42;
			break;
		case 0xc:
			len = 84;
			break;
		case 0xe:
			len = 128;
			break;
		}

		if ((i & 0x40) && len == 0) {	
			return 0;
		}

		if (len) {
			if (len > reqlen)
				len = reqlen;

			if (fast_pio && len > 3) {
				insl(PIO_FIFO, request, len >> 2);
				request += len & 0xfc;
				reqlen -= len & 0xfc;
			} else {
				while (len--) {
					*request++ = inb(PIO_FIFO);
					reqlen--;
				}
			}
		}
	}
	return 0;
}

static __inline__ int NCR53c406a_pio_write(unsigned char *request, unsigned int reqlen)
{
	int i = 0;
	int len;		

	REG1;
	while (reqlen && !(i & 0x40)) {
		i = inb(PIO_STATUS);
		
		if (i & 0x80)	
			return 0;

		switch (i & 0x1e) {
		case 0x10:
			len = 128;
			break;
		case 0x0:
			len = 84;
			break;
		case 0x8:
			len = 42;
			break;
		case 0xc:
			len = 1;
			break;
		default:
		case 0xe:
			len = 0;
			break;
		}

		if (len) {
			if (len > reqlen)
				len = reqlen;

			if (fast_pio && len > 3) {
				outsl(PIO_FIFO, request, len >> 2);
				request += len & 0xfc;
				reqlen -= len & 0xfc;
			} else {
				while (len--) {
					outb(*request++, PIO_FIFO);
					reqlen--;
				}
			}
		}
	}
	return 0;
}
#endif				

static int __init NCR53c406a_detect(struct scsi_host_template * tpnt)
{
	int present = 0;
	struct Scsi_Host *shpnt = NULL;
#ifndef PORT_BASE
	int i;
#endif

#if USE_BIOS
	int ii, jj;
	bios_base = 0;
	
	for (ii = 0; ii < ADDRESS_COUNT && !bios_base; ii++)
		for (jj = 0; (jj < SIGNATURE_COUNT) && !bios_base; jj++)
			if (!memcmp((void *) addresses[ii] + signatures[jj].sig_offset, (void *) signatures[jj].signature, (int) signatures[jj].sig_length))
				bios_base = addresses[ii];

	if (!bios_base) {
		printk("NCR53c406a: BIOS signature not found\n");
		return 0;
	}

	DEB(printk("NCR53c406a BIOS found at 0x%x\n", (unsigned int) bios_base);
	    );
#endif				

#ifdef PORT_BASE
	if (!request_region(port_base, 0x10, "NCR53c406a"))	
		port_base = 0;

#else				
	if (port_base) {	
		if (!request_region(port_base, 0x10, "NCR53c406a"))
			port_base = 0;
	} else {
		for (i = 0; i < PORT_COUNT && !port_base; i++) {
			if (!request_region(ports[i], 0x10, "NCR53c406a")) {
				DEB(printk("NCR53c406a: port 0x%x in use\n", ports[i]));
			} else {
				VDEB(printk("NCR53c406a: port 0x%x available\n", ports[i]));
				outb(C5_IMG, ports[i] + 0x0d);	
				if ((inb(ports[i] + 0x0e) ^ inb(ports[i] + 0x0e)) == 7 && (inb(ports[i] + 0x0e) ^ inb(ports[i] + 0x0e)) == 7 && (inb(ports[i] + 0x0e) & 0xf8) == 0x58) {
					port_base = ports[i];
					VDEB(printk("NCR53c406a: Sig register valid\n"));
					VDEB(printk("port_base=0x%x\n", port_base));
					break;
				}
				release_region(ports[i], 0x10);
			}
		}
	}
#endif				

	if (!port_base) {	
		printk("NCR53c406a: no available ports found\n");
		return 0;
	}

	DEB(printk("NCR53c406a detected\n"));

	calc_port_addr();
	chip_init();

#ifndef IRQ_LEV
	if (irq_level < 0) {	
		irq_level = irq_probe();
		if (irq_level < 0) {	
			printk("NCR53c406a: IRQ problem, irq_level=%d, giving up\n", irq_level);
			goto err_release;
		}
	}
#endif

	DEB(printk("NCR53c406a: using port_base 0x%x\n", port_base));

	present = 1;
	tpnt->proc_name = "NCR53c406a";

	shpnt = scsi_register(tpnt, 0);
	if (!shpnt) {
		printk("NCR53c406a: Unable to register host, giving up.\n");
		goto err_release;
	}

	if (irq_level > 0) {
		if (request_irq(irq_level, do_NCR53c406a_intr, 0, "NCR53c406a", shpnt)) {
			printk("NCR53c406a: unable to allocate IRQ %d\n", irq_level);
			goto err_free_scsi;
		}
		tpnt->can_queue = 1;
		DEB(printk("NCR53c406a: allocated IRQ %d\n", irq_level));
	} else if (irq_level == 0) {
		tpnt->can_queue = 0;
		DEB(printk("NCR53c406a: No interrupts detected\n"));
		printk("NCR53c406a driver no longer supports polling interface\n");
		printk("Please email linux-scsi@vger.kernel.org\n");
                        
#if USE_DMA
		printk("NCR53c406a: No interrupts found and DMA mode defined. Giving up.\n");
#endif				
		goto err_free_scsi;
	} else {
		DEB(printk("NCR53c406a: Shouldn't get here!\n"));
		goto err_free_scsi;
	}

#if USE_DMA
	dma_chan = DMA_CHAN;
	if (request_dma(dma_chan, "NCR53c406a") != 0) {
		printk("NCR53c406a: unable to allocate DMA channel %d\n", dma_chan);
		goto err_free_irq;
	}

	DEB(printk("Allocated DMA channel %d\n", dma_chan));
#endif				

	shpnt->irq = irq_level;
	shpnt->io_port = port_base;
	shpnt->n_io_port = 0x10;
#if USE_DMA
	shpnt->dma = dma_chan;
#endif

#if USE_DMA
	sprintf(info_msg, "NCR53c406a at 0x%x, IRQ %d, DMA channel %d.", port_base, irq_level, dma_chan);
#else
	sprintf(info_msg, "NCR53c406a at 0x%x, IRQ %d, %s PIO mode.", port_base, irq_level, fast_pio ? "fast" : "slow");
#endif

	return (present);

#if USE_DMA
      err_free_irq:
	if (irq_level)
		free_irq(irq_level, shpnt);
#endif
      err_free_scsi:
	scsi_unregister(shpnt);
      err_release:
	release_region(port_base, 0x10);
	return 0;
}

static int NCR53c406a_release(struct Scsi_Host *shost)
{
	if (shost->irq)
		free_irq(shost->irq, NULL);
#ifdef USE_DMA
	if (shost->dma_channel != 0xff)
		free_dma(shost->dma_channel);
#endif
	if (shost->io_port && shost->n_io_port)
		release_region(shost->io_port, shost->n_io_port);

	scsi_unregister(shost);
	return 0;
}

#ifndef MODULE
static int __init NCR53c406a_setup(char *str)
{
	static size_t setup_idx = 0;
	size_t i;
	int ints[4];

	DEB(printk("NCR53c406a: Setup called\n");
	    );

	if (setup_idx >= PORT_COUNT - 1) {
		printk("NCR53c406a: Setup called too many times.  Bad LILO params?\n");
		return 0;
	}
	get_options(str, 4, ints);
	if (ints[0] < 1 || ints[0] > 3) {
		printk("NCR53c406a: Malformed command line\n");
		printk("NCR53c406a: Usage: ncr53c406a=<PORTBASE>[,<IRQ>[,<FASTPIO>]]\n");
		return 0;
	}
	for (i = 0; i < PORT_COUNT && !port_base; i++)
		if (ports[i] == ints[1]) {
			port_base = ints[1];
			DEB(printk("NCR53c406a: Specified port_base 0x%x\n", port_base);
			    )
		}
	if (!port_base) {
		printk("NCR53c406a: Invalid PORTBASE 0x%x specified\n", ints[1]);
		return 0;
	}

	if (ints[0] > 1) {
		if (ints[2] == 0) {
			irq_level = 0;
			DEB(printk("NCR53c406a: Specified irq %d\n", irq_level);
			    )
		} else
			for (i = 0; i < INTR_COUNT && irq_level < 0; i++)
				if (intrs[i] == ints[2]) {
					irq_level = ints[2];
					DEB(printk("NCR53c406a: Specified irq %d\n", port_base);
					    )
				}
		if (irq_level < 0)
			printk("NCR53c406a: Invalid IRQ %d specified\n", ints[2]);
	}

	if (ints[0] > 2)
		fast_pio = ints[3];

	DEB(printk("NCR53c406a: port_base=0x%x, irq=%d, fast_pio=%d\n", port_base, irq_level, fast_pio);)
	return 1;
}

__setup("ncr53c406a=", NCR53c406a_setup);

#endif 

static const char *NCR53c406a_info(struct Scsi_Host *SChost)
{
	DEB(printk("NCR53c406a_info called\n"));
	return (info_msg);
}

#if 0
static void wait_intr(void)
{
	unsigned long i = jiffies + WATCHDOG;

	while (time_after(i, jiffies) && !(inb(STAT_REG) & 0xe0)) {	
		cpu_relax();
		barrier();
	}

	if (time_before_eq(i, jiffies)) {	
		rtrc(0);
		current_SC->result = DID_TIME_OUT << 16;
		current_SC->SCp.phase = idle;
		current_SC->scsi_done(current_SC);
		return;
	}

	NCR53c406a_intr(NULL);
}
#endif

static int NCR53c406a_queue_lck(Scsi_Cmnd * SCpnt, void (*done) (Scsi_Cmnd *))
{
	int i;

	VDEB(printk("NCR53c406a_queue called\n"));
	DEB(printk("cmd=%02x, cmd_len=%02x, target=%02x, lun=%02x, bufflen=%d\n", SCpnt->cmnd[0], SCpnt->cmd_len, SCpnt->target, SCpnt->lun, scsi_bufflen(SCpnt)));

#if 0
	VDEB(for (i = 0; i < SCpnt->cmd_len; i++)
	     printk("cmd[%d]=%02x  ", i, SCpnt->cmnd[i]));
	VDEB(printk("\n"));
#endif

	current_SC = SCpnt;
	current_SC->scsi_done = done;
	current_SC->SCp.phase = command_ph;
	current_SC->SCp.Status = 0;
	current_SC->SCp.Message = 0;

	
	REG0;
	outb(scmd_id(SCpnt), DEST_ID);	
	outb(FLUSH_FIFO, CMD_REG);	

	for (i = 0; i < SCpnt->cmd_len; i++) {
		outb(SCpnt->cmnd[i], SCSI_FIFO);
	}
	outb(SELECT_NO_ATN, CMD_REG);

	rtrc(1);
	return 0;
}

static DEF_SCSI_QCMD(NCR53c406a_queue)

static int NCR53c406a_host_reset(Scsi_Cmnd * SCpnt)
{
	DEB(printk("NCR53c406a_reset called\n"));

	spin_lock_irq(SCpnt->device->host->host_lock);

	outb(C4_IMG, CONFIG4);	
	outb(CHIP_RESET, CMD_REG);
	outb(SCSI_NOP, CMD_REG);	
	outb(SCSI_RESET, CMD_REG);
	chip_init();

	rtrc(2);

	spin_unlock_irq(SCpnt->device->host->host_lock);

	return SUCCESS;
}

static int NCR53c406a_biosparm(struct scsi_device *disk,
                               struct block_device *dev,
			       sector_t capacity, int *info_array)
{
	int size;

	DEB(printk("NCR53c406a_biosparm called\n"));

	size = capacity;
	info_array[0] = 64;	
	info_array[1] = 32;	
	info_array[2] = size >> 11;	
	if (info_array[2] > 1024) {	
		info_array[0] = 255;
		info_array[1] = 63;
		info_array[2] = size / (255 * 63);
	}
	return 0;
}

static irqreturn_t do_NCR53c406a_intr(int unused, void *dev_id)
{
	unsigned long flags;
	struct Scsi_Host *dev = dev_id;

	spin_lock_irqsave(dev->host_lock, flags);
	NCR53c406a_intr(dev_id);
	spin_unlock_irqrestore(dev->host_lock, flags);
	return IRQ_HANDLED;
}

static void NCR53c406a_intr(void *dev_id)
{
	DEB(unsigned char fifo_size;
	    )
	    DEB(unsigned char seq_reg;
	    )
	unsigned char status, int_reg;
#if USE_PIO
	unsigned char pio_status;
	struct scatterlist *sg;
        int i;
#endif

	VDEB(printk("NCR53c406a_intr called\n"));

#if USE_PIO
	REG1;
	pio_status = inb(PIO_STATUS);
#endif
	REG0;
	status = inb(STAT_REG);
	DEB(seq_reg = inb(SEQ_REG));
	int_reg = inb(INT_REG);
	DEB(fifo_size = inb(FIFO_FLAGS) & 0x1f);

#if NCR53C406A_DEBUG
	printk("status=%02x, seq_reg=%02x, int_reg=%02x, fifo_size=%02x", status, seq_reg, int_reg, fifo_size);
#if (USE_DMA)
	printk("\n");
#else
	printk(", pio=%02x\n", pio_status);
#endif				
#endif				

	if (int_reg & 0x80) {	
		rtrc(3);
		DEB(printk("NCR53c406a: reset intr received\n"));
		current_SC->SCp.phase = idle;
		current_SC->result = DID_RESET << 16;
		current_SC->scsi_done(current_SC);
		return;
	}
#if USE_PIO
	if (pio_status & 0x80) {
		printk("NCR53C406A: Warning: PIO error!\n");
		current_SC->SCp.phase = idle;
		current_SC->result = DID_ERROR << 16;
		current_SC->scsi_done(current_SC);
		return;
	}
#endif				

	if (status & 0x20) {	
		printk("NCR53c406a: Warning: parity error!\n");
		current_SC->SCp.phase = idle;
		current_SC->result = DID_PARITY << 16;
		current_SC->scsi_done(current_SC);
		return;
	}

	if (status & 0x40) {	
		printk("NCR53c406a: Warning: gross error!\n");
		current_SC->SCp.phase = idle;
		current_SC->result = DID_ERROR << 16;
		current_SC->scsi_done(current_SC);
		return;
	}

	if (int_reg & 0x20) {	
		DEB(printk("NCR53c406a: disconnect intr received\n"));
		if (current_SC->SCp.phase != message_in) {	
			current_SC->result = DID_NO_CONNECT << 16;
		} else {	
			current_SC->result = (current_SC->SCp.Status & 0xff)
			    | ((current_SC->SCp.Message & 0xff) << 8) | (DID_OK << 16);
		}

		rtrc(0);
		current_SC->SCp.phase = idle;
		current_SC->scsi_done(current_SC);
		return;
	}

	switch (status & 0x07) {	
	case 0x00:		
		if (int_reg & 0x10) {	
			rtrc(5);
			current_SC->SCp.phase = data_out;
			VDEB(printk("NCR53c406a: Data-Out phase\n"));
			outb(FLUSH_FIFO, CMD_REG);
			LOAD_DMA_COUNT(scsi_bufflen(current_SC));	
#if USE_DMA			
			NCR53c406a_dma_write(scsi_sglist(current_SC),
                                             scsdi_bufflen(current_SC));

#endif				
			outb(TRANSFER_INFO | DMA_OP, CMD_REG);
#if USE_PIO
                        scsi_for_each_sg(current_SC, sg, scsi_sg_count(current_SC), i) {
                                NCR53c406a_pio_write(sg_virt(sg), sg->length);
                        }
			REG0;
#endif				
		}
		break;

	case 0x01:		
		if (int_reg & 0x10) {	
			rtrc(6);
			current_SC->SCp.phase = data_in;
			VDEB(printk("NCR53c406a: Data-In phase\n"));
			outb(FLUSH_FIFO, CMD_REG);
			LOAD_DMA_COUNT(scsi_bufflen(current_SC));	
#if USE_DMA			
			NCR53c406a_dma_read(scsi_sglist(current_SC),
                                            scsdi_bufflen(current_SC));
#endif				
			outb(TRANSFER_INFO | DMA_OP, CMD_REG);
#if USE_PIO
                        scsi_for_each_sg(current_SC, sg, scsi_sg_count(current_SC), i) {
                                NCR53c406a_pio_read(sg_virt(sg), sg->length);
                        }
			REG0;
#endif				
		}
		break;

	case 0x02:		
		current_SC->SCp.phase = command_ph;
		printk("NCR53c406a: Warning: Unknown interrupt occurred in command phase!\n");
		break;

	case 0x03:		
		rtrc(7);
		current_SC->SCp.phase = status_ph;
		VDEB(printk("NCR53c406a: Status phase\n"));
		outb(FLUSH_FIFO, CMD_REG);
		outb(INIT_CMD_COMPLETE, CMD_REG);
		break;

	case 0x04:		
	case 0x05:		
		printk("NCR53c406a: WARNING: Reserved phase!!!\n");
		break;

	case 0x06:		
		DEB(printk("NCR53c406a: Message-Out phase\n"));
		current_SC->SCp.phase = message_out;
		outb(SET_ATN, CMD_REG);	
		outb(MSG_ACCEPT, CMD_REG);
		break;

	case 0x07:		
		rtrc(4);
		VDEB(printk("NCR53c406a: Message-In phase\n"));
		current_SC->SCp.phase = message_in;

		current_SC->SCp.Status = inb(SCSI_FIFO);
		current_SC->SCp.Message = inb(SCSI_FIFO);

		VDEB(printk("SCSI FIFO size=%d\n", inb(FIFO_FLAGS) & 0x1f));
		DEB(printk("Status = %02x  Message = %02x\n", current_SC->SCp.Status, current_SC->SCp.Message));

		if (current_SC->SCp.Message == SAVE_POINTERS || current_SC->SCp.Message == DISCONNECT) {
			outb(SET_ATN, CMD_REG);	
			DEB(printk("Discarding SAVE_POINTERS message\n"));
		}
		outb(MSG_ACCEPT, CMD_REG);
		break;
	}
}

#ifndef IRQ_LEV
static int irq_probe(void)
{
	int irqs, irq;
	unsigned long i;

	inb(INT_REG);		
	irqs = probe_irq_on();

	
	REG0;
	outb(0xff, CMD_REG);

	
	i = jiffies + WATCHDOG;
	while (time_after(i, jiffies) && !(inb(STAT_REG) & 0x80))
		barrier();
	if (time_before_eq(i, jiffies)) {	
		probe_irq_off(irqs);
		return -1;
	}

	irq = probe_irq_off(irqs);

	
	outb(CHIP_RESET, CMD_REG);
	outb(SCSI_NOP, CMD_REG);
	chip_init();

	return irq;
}
#endif				

static void chip_init(void)
{
	REG1;
#if USE_DMA
	outb(0x00, PIO_STATUS);
#else				
	outb(0x01, PIO_STATUS);
#endif
	outb(0x00, PIO_FLAG);

	outb(C4_IMG, CONFIG4);	
	outb(C3_IMG, CONFIG3);
	outb(C2_IMG, CONFIG2);
	outb(C1_IMG, CONFIG1);

	outb(0x05, CLKCONV);	
	outb(0x9C, SRTIMOUT);	
	outb(0x05, SYNCPRD);	
	outb(SYNC_MODE, SYNCOFF);	
}

static void __init calc_port_addr(void)
{
	
	TC_LSB = (port_base + 0x00);
	TC_MSB = (port_base + 0x01);
	SCSI_FIFO = (port_base + 0x02);
	CMD_REG = (port_base + 0x03);
	STAT_REG = (port_base + 0x04);
	DEST_ID = (port_base + 0x04);
	INT_REG = (port_base + 0x05);
	SRTIMOUT = (port_base + 0x05);
	SEQ_REG = (port_base + 0x06);
	SYNCPRD = (port_base + 0x06);
	FIFO_FLAGS = (port_base + 0x07);
	SYNCOFF = (port_base + 0x07);
	CONFIG1 = (port_base + 0x08);
	CLKCONV = (port_base + 0x09);
	
	CONFIG2 = (port_base + 0x0B);
	CONFIG3 = (port_base + 0x0C);
	CONFIG4 = (port_base + 0x0D);
	TC_HIGH = (port_base + 0x0E);
	

	
	
	
	
	PIO_FIFO = (port_base + 0x04);
	
	
	
	PIO_STATUS = (port_base + 0x08);
	
	
	PIO_FLAG = (port_base + 0x0B);
	CONFIG5 = (port_base + 0x0D);
	
	
}

MODULE_LICENSE("GPL");


static struct scsi_host_template driver_template =
{
     .proc_name         	= "NCR53c406a"		,        
     .name              	= "NCR53c406a"		,             
     .detect            	= NCR53c406a_detect	,           
     .release            	= NCR53c406a_release,
     .info              	= NCR53c406a_info		,             
     .queuecommand      	= NCR53c406a_queue	,     
     .eh_host_reset_handler     = NCR53c406a_host_reset	,            
     .bios_param        	= NCR53c406a_biosparm	,         
     .can_queue         	= 1			,        
     .this_id           	= 7			,
     .sg_tablesize      	= 32			 , 
     .cmd_per_lun       	= 1			, 
     .unchecked_isa_dma 	= 1			,
     .use_clustering    	= ENABLE_CLUSTERING,
};

#include "scsi_module.c"

