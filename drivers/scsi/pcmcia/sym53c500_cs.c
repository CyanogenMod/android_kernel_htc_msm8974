/*
*  sym53c500_cs.c	Bob Tracy (rct@frus.com)
*
*  A rewrite of the pcmcia-cs add-on driver for newer (circa 1997)
*  New Media Bus Toaster PCMCIA SCSI cards using the Symbios Logic
*  53c500 controller: intended for use with 2.6 and later kernels.
*  The pcmcia-cs add-on version of this driver is not supported
*  beyond 2.4.  It consisted of three files with history/copyright
*  information as follows:
*
*  SYM53C500.h
*	Bob Tracy (rct@frus.com)
*	Original by Tom Corner (tcorner@via.at).
*	Adapted from NCR53c406a.h which is Copyrighted (C) 1994
*	Normunds Saumanis (normunds@rx.tech.swh.lv)
*
*  SYM53C500.c
*	Bob Tracy (rct@frus.com)
*	Original driver by Tom Corner (tcorner@via.at) was adapted
*	from NCR53c406a.c which is Copyrighted (C) 1994, 1995, 1996 
*	Normunds Saumanis (normunds@fi.ibm.com)
*
*  sym53c500.c
*	Bob Tracy (rct@frus.com)
*	Original by Tom Corner (tcorner@via.at) was adapted from a
*	driver for the Qlogic SCSI card written by
*	David Hinds (dhinds@allegro.stanford.edu).
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
*/

#define SYM53C500_DEBUG 0
#define VERBOSE_SYM53C500_DEBUG 0

#define USE_FAST_PIO 1


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

#include <scsi/scsi_ioctl.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>

#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>
#include <pcmcia/ciscode.h>



#define SYNC_MODE 0 		

#define C1_IMG   0x07		
#define C2_IMG   0x48		
#define C3_IMG   0x20		
#define C4_IMG   0x04		
#define C5_IMG   0xa4		
#define C7_IMG   0x80		


#define TC_LSB		0x00		
#define TC_MSB		0x01		
#define SCSI_FIFO	0x02		
#define CMD_REG		0x03		
#define STAT_REG	0x04		
#define DEST_ID		0x04		
#define INT_REG		0x05		
#define SRTIMOUT	0x05		
#define SEQ_REG		0x06		
#define SYNCPRD		0x06		
#define FIFO_FLAGS	0x07		
#define SYNCOFF		0x07		
#define CONFIG1		0x08		
#define CLKCONV		0x09		
		
#define CONFIG2		0x0B		
#define CONFIG3		0x0C		
#define CONFIG4		0x0D		
#define TC_HIGH		0x0E		
		

		
		
		
#define PIO_FIFO	0x04		
		
		
		
#define PIO_STATUS	0x08		
		
		
#define PIO_FLAG	0x0B		
#define CONFIG5		0x09		
		
		
#define CONFIG7		0x0d

#define REG0(x)		(outb(C4_IMG, (x) + CONFIG4))
#define REG1(x)		outb(C7_IMG, (x) + CONFIG7); outb(C5_IMG, (x) + CONFIG5)

#if SYM53C500_DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

#if VERBOSE_SYM53C500_DEBUG
#define VDEB(x) x
#else
#define VDEB(x)
#endif

#define LOAD_DMA_COUNT(x, count) \
  outb(count & 0xff, (x) + TC_LSB); \
  outb((count >> 8) & 0xff, (x) + TC_MSB); \
  outb((count >> 16) & 0xff, (x) + TC_HIGH);

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


struct scsi_info_t {
	struct pcmcia_device	*p_dev;
	struct Scsi_Host *host;
	unsigned short manf_id;
};

struct sym53c500_data {
	struct scsi_cmnd *current_SC;
	int fast_pio;
};

enum Phase {
    idle,
    data_out,
    data_in,
    command_ph,
    status_ph,
    message_out,
    message_in
};


static void
chip_init(int io_port)
{
	REG1(io_port);
	outb(0x01, io_port + PIO_STATUS);
	outb(0x00, io_port + PIO_FLAG);

	outb(C4_IMG, io_port + CONFIG4);	
	outb(C3_IMG, io_port + CONFIG3);
	outb(C2_IMG, io_port + CONFIG2);
	outb(C1_IMG, io_port + CONFIG1);

	outb(0x05, io_port + CLKCONV);	
	outb(0x9C, io_port + SRTIMOUT);	
	outb(0x05, io_port + SYNCPRD);	
	outb(SYNC_MODE, io_port + SYNCOFF);	  
}

static void
SYM53C500_int_host_reset(int io_port)
{
	outb(C4_IMG, io_port + CONFIG4);	
	outb(CHIP_RESET, io_port + CMD_REG);
	outb(SCSI_NOP, io_port + CMD_REG);	
	outb(SCSI_RESET, io_port + CMD_REG);
	chip_init(io_port);
}

static __inline__ int
SYM53C500_pio_read(int fast_pio, int base, unsigned char *request, unsigned int reqlen)
{
	int i;
	int len;	

	REG1(base);
	while (reqlen) {
		i = inb(base + PIO_STATUS);
		
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
				insl(base + PIO_FIFO, request, len >> 2);
				request += len & 0xfc; 
				reqlen -= len & 0xfc; 
			} else {
				while (len--) {
					*request++ = inb(base + PIO_FIFO);
					reqlen--;
				}
			} 
		}
	}
	return 0;
}

static __inline__ int
SYM53C500_pio_write(int fast_pio, int base, unsigned char *request, unsigned int reqlen)
{
	int i = 0;
	int len;	

	REG1(base);
	while (reqlen && !(i & 0x40)) {
		i = inb(base + PIO_STATUS);
		
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
				outsl(base + PIO_FIFO, request, len >> 2);
				request += len & 0xfc;
				reqlen -= len & 0xfc;
			} else {
				while (len--) {
					outb(*request++, base + PIO_FIFO);
					reqlen--;
				}
			}
		}
	}
	return 0;
}

static irqreturn_t
SYM53C500_intr(int irq, void *dev_id)
{
	unsigned long flags;
	struct Scsi_Host *dev = dev_id;
	DEB(unsigned char fifo_size;)
	DEB(unsigned char seq_reg;)
	unsigned char status, int_reg;
	unsigned char pio_status;
	int port_base = dev->io_port;
	struct sym53c500_data *data =
	    (struct sym53c500_data *)dev->hostdata;
	struct scsi_cmnd *curSC = data->current_SC;
	int fast_pio = data->fast_pio;

	spin_lock_irqsave(dev->host_lock, flags);

	VDEB(printk("SYM53C500_intr called\n"));

	REG1(port_base);
	pio_status = inb(port_base + PIO_STATUS);
	REG0(port_base);
	status = inb(port_base + STAT_REG);
	DEB(seq_reg = inb(port_base + SEQ_REG));
	int_reg = inb(port_base + INT_REG);
	DEB(fifo_size = inb(port_base + FIFO_FLAGS) & 0x1f);

#if SYM53C500_DEBUG
	printk("status=%02x, seq_reg=%02x, int_reg=%02x, fifo_size=%02x", 
	    status, seq_reg, int_reg, fifo_size);
	printk(", pio=%02x\n", pio_status);
#endif 

	if (int_reg & 0x80) {	
		DEB(printk("SYM53C500: reset intr received\n"));
		curSC->result = DID_RESET << 16;
		goto idle_out;
	}

	if (pio_status & 0x80) {
		printk("SYM53C500: Warning: PIO error!\n");
		curSC->result = DID_ERROR << 16;
		goto idle_out;
	}

	if (status & 0x20) {		
		printk("SYM53C500: Warning: parity error!\n");
		curSC->result = DID_PARITY << 16;
		goto idle_out;
	}

	if (status & 0x40) {		
		printk("SYM53C500: Warning: gross error!\n");
		curSC->result = DID_ERROR << 16;
		goto idle_out;
	}

	if (int_reg & 0x20) {		
		DEB(printk("SYM53C500: disconnect intr received\n"));
		if (curSC->SCp.phase != message_in) {	
			curSC->result = DID_NO_CONNECT << 16;
		} else {	
			curSC->result = (curSC->SCp.Status & 0xff)
			    | ((curSC->SCp.Message & 0xff) << 8) | (DID_OK << 16);
		}
		goto idle_out;
	}

	switch (status & 0x07) {	
	case 0x00:			
		if (int_reg & 0x10) {	
			struct scatterlist *sg;
			int i;

			curSC->SCp.phase = data_out;
			VDEB(printk("SYM53C500: Data-Out phase\n"));
			outb(FLUSH_FIFO, port_base + CMD_REG);
			LOAD_DMA_COUNT(port_base, scsi_bufflen(curSC));	
			outb(TRANSFER_INFO | DMA_OP, port_base + CMD_REG);

			scsi_for_each_sg(curSC, sg, scsi_sg_count(curSC), i) {
				SYM53C500_pio_write(fast_pio, port_base,
				    sg_virt(sg), sg->length);
			}
			REG0(port_base);
		}
		break;

	case 0x01:		
		if (int_reg & 0x10) {	
			struct scatterlist *sg;
			int i;

			curSC->SCp.phase = data_in;
			VDEB(printk("SYM53C500: Data-In phase\n"));
			outb(FLUSH_FIFO, port_base + CMD_REG);
			LOAD_DMA_COUNT(port_base, scsi_bufflen(curSC));	
			outb(TRANSFER_INFO | DMA_OP, port_base + CMD_REG);

			scsi_for_each_sg(curSC, sg, scsi_sg_count(curSC), i) {
				SYM53C500_pio_read(fast_pio, port_base,
					sg_virt(sg), sg->length);
			}
			REG0(port_base);
		}
		break;

	case 0x02:		
		curSC->SCp.phase = command_ph;
		printk("SYM53C500: Warning: Unknown interrupt occurred in command phase!\n");
		break;

	case 0x03:		
		curSC->SCp.phase = status_ph;
		VDEB(printk("SYM53C500: Status phase\n"));
		outb(FLUSH_FIFO, port_base + CMD_REG);
		outb(INIT_CMD_COMPLETE, port_base + CMD_REG);
		break;

	case 0x04:		
	case 0x05:		
		printk("SYM53C500: WARNING: Reserved phase!!!\n");
		break;

	case 0x06:		
		DEB(printk("SYM53C500: Message-Out phase\n"));
		curSC->SCp.phase = message_out;
		outb(SET_ATN, port_base + CMD_REG);	
		outb(MSG_ACCEPT, port_base + CMD_REG);
		break;

	case 0x07:		
		VDEB(printk("SYM53C500: Message-In phase\n"));
		curSC->SCp.phase = message_in;

		curSC->SCp.Status = inb(port_base + SCSI_FIFO);
		curSC->SCp.Message = inb(port_base + SCSI_FIFO);

		VDEB(printk("SCSI FIFO size=%d\n", inb(port_base + FIFO_FLAGS) & 0x1f));
		DEB(printk("Status = %02x  Message = %02x\n", curSC->SCp.Status, curSC->SCp.Message));

		if (curSC->SCp.Message == SAVE_POINTERS || curSC->SCp.Message == DISCONNECT) {
			outb(SET_ATN, port_base + CMD_REG);	
			DEB(printk("Discarding SAVE_POINTERS message\n"));
		}
		outb(MSG_ACCEPT, port_base + CMD_REG);
		break;
	}
out:
	spin_unlock_irqrestore(dev->host_lock, flags);
	return IRQ_HANDLED;

idle_out:
	curSC->SCp.phase = idle;
	curSC->scsi_done(curSC);
	goto out;
}

static void
SYM53C500_release(struct pcmcia_device *link)
{
	struct scsi_info_t *info = link->priv;
	struct Scsi_Host *shost = info->host;

	dev_dbg(&link->dev, "SYM53C500_release\n");

	scsi_remove_host(shost);

	if (shost->irq)
		free_irq(shost->irq, shost);
	if (shost->io_port && shost->n_io_port)
		release_region(shost->io_port, shost->n_io_port);

	pcmcia_disable_device(link);

	scsi_host_put(shost);
} 

static const char*
SYM53C500_info(struct Scsi_Host *SChost)
{
	static char info_msg[256];
	struct sym53c500_data *data =
	    (struct sym53c500_data *)SChost->hostdata;

	DEB(printk("SYM53C500_info called\n"));
	(void)snprintf(info_msg, sizeof(info_msg),
	    "SYM53C500 at 0x%lx, IRQ %d, %s PIO mode.", 
	    SChost->io_port, SChost->irq, data->fast_pio ? "fast" : "slow");
	return (info_msg);
}

static int 
SYM53C500_queue_lck(struct scsi_cmnd *SCpnt, void (*done)(struct scsi_cmnd *))
{
	int i;
	int port_base = SCpnt->device->host->io_port;
	struct sym53c500_data *data =
	    (struct sym53c500_data *)SCpnt->device->host->hostdata;

	VDEB(printk("SYM53C500_queue called\n"));

	DEB(printk("cmd=%02x, cmd_len=%02x, target=%02x, lun=%02x, bufflen=%d\n", 
	    SCpnt->cmnd[0], SCpnt->cmd_len, SCpnt->device->id, 
	    SCpnt->device->lun,  scsi_bufflen(SCpnt)));

	VDEB(for (i = 0; i < SCpnt->cmd_len; i++)
	    printk("cmd[%d]=%02x  ", i, SCpnt->cmnd[i]));
	VDEB(printk("\n"));

	data->current_SC = SCpnt;
	data->current_SC->scsi_done = done;
	data->current_SC->SCp.phase = command_ph;
	data->current_SC->SCp.Status = 0;
	data->current_SC->SCp.Message = 0;

	
	REG0(port_base);
	outb(scmd_id(SCpnt), port_base + DEST_ID);	
	outb(FLUSH_FIFO, port_base + CMD_REG);	

	for (i = 0; i < SCpnt->cmd_len; i++) {
		outb(SCpnt->cmnd[i], port_base + SCSI_FIFO);
	}
	outb(SELECT_NO_ATN, port_base + CMD_REG);

	return 0;
}

static DEF_SCSI_QCMD(SYM53C500_queue)

static int 
SYM53C500_host_reset(struct scsi_cmnd *SCpnt)
{
	int port_base = SCpnt->device->host->io_port;

	DEB(printk("SYM53C500_host_reset called\n"));
	spin_lock_irq(SCpnt->device->host->host_lock);
	SYM53C500_int_host_reset(port_base);
	spin_unlock_irq(SCpnt->device->host->host_lock);

	return SUCCESS;
}

static int 
SYM53C500_biosparm(struct scsi_device *disk,
    struct block_device *dev,
    sector_t capacity, int *info_array)
{
	int size;

	DEB(printk("SYM53C500_biosparm called\n"));

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

static ssize_t
SYM53C500_show_pio(struct device *dev, struct device_attribute *attr,
		   char *buf)
{
	struct Scsi_Host *SHp = class_to_shost(dev);
	struct sym53c500_data *data =
	    (struct sym53c500_data *)SHp->hostdata;

	return snprintf(buf, 4, "%d\n", data->fast_pio);
}

static ssize_t
SYM53C500_store_pio(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	int pio;
	struct Scsi_Host *SHp = class_to_shost(dev);
	struct sym53c500_data *data =
	    (struct sym53c500_data *)SHp->hostdata;

	pio = simple_strtoul(buf, NULL, 0);
	if (pio == 0 || pio == 1) {
		data->fast_pio = pio;
		return count;
	}
	else
		return -EINVAL;
}

static struct device_attribute SYM53C500_pio_attr = {
	.attr = {
		.name = "fast_pio",
		.mode = (S_IRUGO | S_IWUSR),
	},
	.show = SYM53C500_show_pio,
	.store = SYM53C500_store_pio,
};

static struct device_attribute *SYM53C500_shost_attrs[] = {
	&SYM53C500_pio_attr,
	NULL,
};

static struct scsi_host_template sym53c500_driver_template = {
     .module			= THIS_MODULE,
     .name			= "SYM53C500",
     .info			= SYM53C500_info,
     .queuecommand		= SYM53C500_queue,
     .eh_host_reset_handler	= SYM53C500_host_reset,
     .bios_param		= SYM53C500_biosparm,
     .proc_name			= "SYM53C500",
     .can_queue			= 1,
     .this_id			= 7,
     .sg_tablesize		= 32,
     .cmd_per_lun		= 1,
     .use_clustering		= ENABLE_CLUSTERING,
     .shost_attrs		= SYM53C500_shost_attrs
};

static int SYM53C500_config_check(struct pcmcia_device *p_dev, void *priv_data)
{
	p_dev->io_lines = 10;
	p_dev->resource[0]->flags &= ~IO_DATA_PATH_WIDTH;
	p_dev->resource[0]->flags |= IO_DATA_PATH_WIDTH_AUTO;

	if (p_dev->resource[0]->start == 0)
		return -ENODEV;

	return pcmcia_request_io(p_dev);
}

static int
SYM53C500_config(struct pcmcia_device *link)
{
	struct scsi_info_t *info = link->priv;
	int ret;
	int irq_level, port_base;
	struct Scsi_Host *host;
	struct scsi_host_template *tpnt = &sym53c500_driver_template;
	struct sym53c500_data *data;

	dev_dbg(&link->dev, "SYM53C500_config\n");

	info->manf_id = link->manf_id;

	ret = pcmcia_loop_config(link, SYM53C500_config_check, NULL);
	if (ret)
		goto failed;

	if (!link->irq)
		goto failed;

	ret = pcmcia_enable_device(link);
	if (ret)
		goto failed;

	if ((info->manf_id == MANFID_MACNICA) ||
	    (info->manf_id == MANFID_PIONEER) ||
	    (info->manf_id == 0x0098)) {
		
		outb(0xb4, link->resource[0]->start + 0xd);
		outb(0x24, link->resource[0]->start + 0x9);
		outb(0x04, link->resource[0]->start + 0xd);
	}

	port_base = link->resource[0]->start;
	irq_level = link->irq;

	DEB(printk("SYM53C500: port_base=0x%x, irq=%d, fast_pio=%d\n",
	    port_base, irq_level, USE_FAST_PIO);)

	chip_init(port_base);

	host = scsi_host_alloc(tpnt, sizeof(struct sym53c500_data));
	if (!host) {
		printk("SYM53C500: Unable to register host, giving up.\n");
		goto err_release;
	}

	data = (struct sym53c500_data *)host->hostdata;

	if (irq_level > 0) {
		if (request_irq(irq_level, SYM53C500_intr, IRQF_SHARED, "SYM53C500", host)) {
			printk("SYM53C500: unable to allocate IRQ %d\n", irq_level);
			goto err_free_scsi;
		}
		DEB(printk("SYM53C500: allocated IRQ %d\n", irq_level));
	} else if (irq_level == 0) {
		DEB(printk("SYM53C500: No interrupts detected\n"));
		goto err_free_scsi;
	} else {
		DEB(printk("SYM53C500: Shouldn't get here!\n"));
		goto err_free_scsi;
	}

	host->unique_id = port_base;
	host->irq = irq_level;
	host->io_port = port_base;
	host->n_io_port = 0x10;
	host->dma_channel = -1;

	data->fast_pio = USE_FAST_PIO;

	info->host = host;

	if (scsi_add_host(host, NULL))
		goto err_free_irq;

	scsi_scan_host(host);

	return 0;

err_free_irq:
	free_irq(irq_level, host);
err_free_scsi:
	scsi_host_put(host);
err_release:
	release_region(port_base, 0x10);
	printk(KERN_INFO "sym53c500_cs: no SCSI devices found\n");
	return -ENODEV;

failed:
	SYM53C500_release(link);
	return -ENODEV;
} 

static int sym53c500_resume(struct pcmcia_device *link)
{
	struct scsi_info_t *info = link->priv;

	
	if ((info->manf_id == MANFID_MACNICA) ||
	    (info->manf_id == MANFID_PIONEER) ||
	    (info->manf_id == 0x0098)) {
		outb(0x80, link->resource[0]->start + 0xd);
		outb(0x24, link->resource[0]->start + 0x9);
		outb(0x04, link->resource[0]->start + 0xd);
	}
	SYM53C500_int_host_reset(link->resource[0]->start);

	return 0;
}

static void
SYM53C500_detach(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "SYM53C500_detach\n");

	SYM53C500_release(link);

	kfree(link->priv);
	link->priv = NULL;
} 

static int
SYM53C500_probe(struct pcmcia_device *link)
{
	struct scsi_info_t *info;

	dev_dbg(&link->dev, "SYM53C500_attach()\n");

	
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->p_dev = link;
	link->priv = info;
	link->config_flags |= CONF_ENABLE_IRQ | CONF_AUTO_SET_IO;

	return SYM53C500_config(link);
} 

MODULE_AUTHOR("Bob Tracy <rct@frus.com>");
MODULE_DESCRIPTION("SYM53C500 PCMCIA SCSI driver");
MODULE_LICENSE("GPL");

static const struct pcmcia_device_id sym53c500_ids[] = {
	PCMCIA_DEVICE_PROD_ID12("BASICS by New Media Corporation", "SCSI Sym53C500", 0x23c78a9d, 0x0099e7f7),
	PCMCIA_DEVICE_PROD_ID12("New Media Corporation", "SCSI Bus Toaster Sym53C500", 0x085a850b, 0x45432eb8),
	PCMCIA_DEVICE_PROD_ID2("SCSI9000", 0x21648f44),
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, sym53c500_ids);

static struct pcmcia_driver sym53c500_cs_driver = {
	.owner		= THIS_MODULE,
	.name		= "sym53c500_cs",
	.probe		= SYM53C500_probe,
	.remove		= SYM53C500_detach,
	.id_table       = sym53c500_ids,
	.resume		= sym53c500_resume,
};

static int __init
init_sym53c500_cs(void)
{
	return pcmcia_register_driver(&sym53c500_cs_driver);
}

static void __exit
exit_sym53c500_cs(void)
{
	pcmcia_unregister_driver(&sym53c500_cs_driver);
}

module_init(init_sym53c500_cs);
module_exit(exit_sym53c500_cs);
