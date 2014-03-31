/* fd_mcs.c -- Future Domain MCS 600/700 (or IBM OEM) driver
 *
 * FutureDomain MCS-600/700 v0.2 03/11/1998 by ZP Gu (zpg@castle.net)
 *
 * This driver is cloned from fdomain.* to specifically support
 * the Future Domain MCS 600/700 MCA SCSI adapters. Some PS/2s
 * also equipped with IBM Fast SCSI Adapter/A which is an OEM
 * of MCS 700.
 *
 * This driver also supports Reply SB16/SCSI card (the SCSI part).
 *
 * What makes this driver different is that this driver is MCA only
 * and it supports multiple adapters in the same system, IRQ 
 * sharing, some driver statistics, and maps highest SCSI id to sda.
 * All cards are auto-detected.
 *
 * Assumptions: TMC-1800/18C50/18C30, BIOS >= 3.4
 *
 * LILO command-line options:
 *   fd_mcs=<FIFO_COUNT>[,<FIFO_SIZE>]
 *
 * ********************************************************
 * Please see Copyrights/Comments in fdomain.* for credits.
 * Following is from fdomain.c for acknowledgement:
 *
 * Created: Sun May  3 18:53:19 1992 by faith@cs.unc.edu
 * Revised: Wed Oct  2 11:10:55 1996 by r.faith@ieee.org
 * Author: Rickard E. Faith, faith@cs.unc.edu
 * Copyright 1992, 1993, 1994, 1995, 1996 Rickard E. Faith
 *
 * $Id: fdomain.c,v 5.45 1996/10/02 15:13:06 root Exp $

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.

 **************************************************************************

 NOTES ON USER DEFINABLE OPTIONS:

 DEBUG: This turns on the printing of various debug information.

 ENABLE_PARITY: This turns on SCSI parity checking.  With the current
 driver, all attached devices must support SCSI parity.  If none of your
 devices support parity, then you can probably get the driver to work by
 turning this option off.  I have no way of testing this, however, and it
 would appear that no one ever uses this option.

 FIFO_COUNT: The host adapter has an 8K cache (host adapters based on the
 18C30 chip have a 2k cache).  When this many 512 byte blocks are filled by
 the SCSI device, an interrupt will be raised.  Therefore, this could be as
 low as 0, or as high as 16.  Note, however, that values which are too high
 or too low seem to prevent any interrupts from occurring, and thereby lock
 up the machine.  I have found that 2 is a good number, but throughput may
 be increased by changing this value to values which are close to 2.
 Please let me know if you try any different values.
 [*****Now a runtime option*****]

 RESELECTION: This is no longer an option, since I gave up trying to
 implement it in version 4.x of this driver.  It did not improve
 performance at all and made the driver unstable (because I never found one
 of the two race conditions which were introduced by the multiple
 outstanding command code).  The instability seems a very high price to pay
 just so that you don't have to wait for the tape to rewind.  If you want
 this feature implemented, send me patches.  I'll be happy to send a copy
 of my (broken) driver to anyone who would like to see a copy.

 **************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/mca.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <scsi/scsicam.h>
#include <linux/mca-legacy.h>

#include <asm/io.h>

#include "scsi.h"
#include <scsi/scsi_host.h>

#define DRIVER_VERSION "v0.2 by ZP Gu<zpg@castle.net>"


#define DEBUG            0	
#define ENABLE_PARITY    1	


#if DEBUG
#define EVERY_ACCESS     0	
#define ERRORS_ONLY      1	
#define DEBUG_MESSAGES   1	
#define DEBUG_ABORT      1	
#define DEBUG_RESET      1	
#define DEBUG_RACE       1	
#else
#define EVERY_ACCESS     0	
#define ERRORS_ONLY      0
#define DEBUG_MESSAGES   0
#define DEBUG_ABORT      0
#define DEBUG_RESET      0
#define DEBUG_RACE       0
#endif

#if EVERY_ACCESS
#undef ERRORS_ONLY
#define ERRORS_ONLY      0
#endif

#if ENABLE_PARITY
#define PARITY_MASK      0x08
#else
#define PARITY_MASK      0x00
#endif

enum chip_type {
	unknown = 0x00,
	tmc1800 = 0x01,
	tmc18c50 = 0x02,
	tmc18c30 = 0x03,
};

enum {
	in_arbitration = 0x02,
	in_selection = 0x04,
	in_other = 0x08,
	disconnect = 0x10,
	aborted = 0x20,
	sent_ident = 0x40,
};

enum in_port_type {
	Read_SCSI_Data = 0,
	SCSI_Status = 1,
	TMC_Status = 2,
	FIFO_Status = 3,	
	Interrupt_Cond = 4,	
	LSB_ID_Code = 5,
	MSB_ID_Code = 6,
	Read_Loopback = 7,
	SCSI_Data_NoACK = 8,
	Interrupt_Status = 9,
	Configuration1 = 10,
	Configuration2 = 11,	
	Read_FIFO = 12,
	FIFO_Data_Count = 14
};

enum out_port_type {
	Write_SCSI_Data = 0,
	SCSI_Cntl = 1,
	Interrupt_Cntl = 2,
	SCSI_Mode_Cntl = 3,
	TMC_Cntl = 4,
	Memory_Cntl = 5,	
	Write_Loopback = 7,
	IO_Control = 11,	
	Write_FIFO = 12
};

struct fd_hostdata {
	unsigned long _bios_base;
	int _bios_major;
	int _bios_minor;
	volatile int _in_command;
	Scsi_Cmnd *_current_SC;
	enum chip_type _chip;
	int _adapter_mask;
	int _fifo_count;	

	char _adapter_name[64];
#if DEBUG_RACE
	volatile int _in_interrupt_flag;
#endif

	int _SCSI_Mode_Cntl_port;
	int _FIFO_Data_Count_port;
	int _Interrupt_Cntl_port;
	int _Interrupt_Status_port;
	int _Interrupt_Cond_port;
	int _Read_FIFO_port;
	int _Read_SCSI_Data_port;
	int _SCSI_Cntl_port;
	int _SCSI_Data_NoACK_port;
	int _SCSI_Status_port;
	int _TMC_Cntl_port;
	int _TMC_Status_port;
	int _Write_FIFO_port;
	int _Write_SCSI_Data_port;

	int _FIFO_Size;		
	
	int _Bytes_Read;
	int _Bytes_Written;
	int _INTR_Processed;
};

#define FD_MAX_HOSTS 3		

#define HOSTDATA(shpnt) ((struct fd_hostdata *) shpnt->hostdata)
#define bios_base             (HOSTDATA(shpnt)->_bios_base)
#define bios_major            (HOSTDATA(shpnt)->_bios_major)
#define bios_minor            (HOSTDATA(shpnt)->_bios_minor)
#define in_command            (HOSTDATA(shpnt)->_in_command)
#define current_SC            (HOSTDATA(shpnt)->_current_SC)
#define chip                  (HOSTDATA(shpnt)->_chip)
#define adapter_mask          (HOSTDATA(shpnt)->_adapter_mask)
#define FIFO_COUNT            (HOSTDATA(shpnt)->_fifo_count)
#define adapter_name          (HOSTDATA(shpnt)->_adapter_name)
#if DEBUG_RACE
#define in_interrupt_flag     (HOSTDATA(shpnt)->_in_interrupt_flag)
#endif
#define SCSI_Mode_Cntl_port   (HOSTDATA(shpnt)->_SCSI_Mode_Cntl_port)
#define FIFO_Data_Count_port  (HOSTDATA(shpnt)->_FIFO_Data_Count_port)
#define Interrupt_Cntl_port   (HOSTDATA(shpnt)->_Interrupt_Cntl_port)
#define Interrupt_Status_port (HOSTDATA(shpnt)->_Interrupt_Status_port)
#define Interrupt_Cond_port   (HOSTDATA(shpnt)->_Interrupt_Cond_port)
#define Read_FIFO_port        (HOSTDATA(shpnt)->_Read_FIFO_port)
#define Read_SCSI_Data_port   (HOSTDATA(shpnt)->_Read_SCSI_Data_port)
#define SCSI_Cntl_port        (HOSTDATA(shpnt)->_SCSI_Cntl_port)
#define SCSI_Data_NoACK_port  (HOSTDATA(shpnt)->_SCSI_Data_NoACK_port)
#define SCSI_Status_port      (HOSTDATA(shpnt)->_SCSI_Status_port)
#define TMC_Cntl_port         (HOSTDATA(shpnt)->_TMC_Cntl_port)
#define TMC_Status_port       (HOSTDATA(shpnt)->_TMC_Status_port)
#define Write_FIFO_port       (HOSTDATA(shpnt)->_Write_FIFO_port)
#define Write_SCSI_Data_port  (HOSTDATA(shpnt)->_Write_SCSI_Data_port)
#define FIFO_Size             (HOSTDATA(shpnt)->_FIFO_Size)
#define Bytes_Read            (HOSTDATA(shpnt)->_Bytes_Read)
#define Bytes_Written         (HOSTDATA(shpnt)->_Bytes_Written)
#define INTR_Processed        (HOSTDATA(shpnt)->_INTR_Processed)

struct fd_mcs_adapters_struct {
	char *name;
	int id;
	enum chip_type fd_chip;
	int fifo_size;
	int fifo_count;
};

#define REPLY_ID 0x5137

static struct fd_mcs_adapters_struct fd_mcs_adapters[] = {
	{"Future Domain SCSI Adapter MCS-700(18C50)",
	 0x60e9,
	 tmc18c50,
	 0x2000,
	 4},
	{"Future Domain SCSI Adapter MCS-600/700(TMC-1800)",
	 0x6127,
	 tmc1800,
	 0x2000,
	 4},
	{"Reply Sound Blaster/SCSI Adapter",
	 REPLY_ID,
	 tmc18c30,
	 0x800,
	 2},
};

#define FD_BRDS ARRAY_SIZE(fd_mcs_adapters)

static irqreturn_t fd_mcs_intr(int irq, void *dev_id);

static unsigned long addresses[] = { 0xc8000, 0xca000, 0xce000, 0xde000 };
static unsigned short ports[] = { 0x140, 0x150, 0x160, 0x170 };
static unsigned short interrupts[] = { 3, 5, 10, 11, 12, 14, 15, 0 };

static int found = 0;
static struct Scsi_Host *hosts[FD_MAX_HOSTS + 1] = { NULL };

static int user_fifo_count = 0;
static int user_fifo_size = 0;

#ifndef MODULE
static int __init fd_mcs_setup(char *str)
{
	static int done_setup = 0;
	int ints[3];

	get_options(str, 3, ints);
	if (done_setup++ || ints[0] < 1 || ints[0] > 2 || ints[1] < 1 || ints[1] > 16) {
		printk("fd_mcs: usage: fd_mcs=FIFO_COUNT, FIFO_SIZE\n");
		return 0;
	}

	user_fifo_count = ints[0] >= 1 ? ints[1] : 0;
	user_fifo_size = ints[0] >= 2 ? ints[2] : 0;
	return 1;
}

__setup("fd_mcs=", fd_mcs_setup);
#endif 

static void print_banner(struct Scsi_Host *shpnt)
{
	printk("scsi%d <fd_mcs>: ", shpnt->host_no);

	if (bios_base) {
		printk("BIOS at 0x%lX", bios_base);
	} else {
		printk("No BIOS");
	}

	printk(", HostID %d, %s Chip, IRQ %d, IO 0x%lX\n", shpnt->this_id, chip == tmc18c50 ? "TMC-18C50" : (chip == tmc18c30 ? "TMC-18C30" : (chip == tmc1800 ? "TMC-1800" : "Unknown")), shpnt->irq, shpnt->io_port);
}


static void do_pause(unsigned amount)
{				
	do {
		mdelay(10);
	} while (--amount);
}

static void fd_mcs_make_bus_idle(struct Scsi_Host *shpnt)
{
	outb(0, SCSI_Cntl_port);
	outb(0, SCSI_Mode_Cntl_port);
	if (chip == tmc18c50 || chip == tmc18c30)
		outb(0x21 | PARITY_MASK, TMC_Cntl_port);	
	else
		outb(0x01 | PARITY_MASK, TMC_Cntl_port);
}

static int fd_mcs_detect(struct scsi_host_template * tpnt)
{
	int loop;
	struct Scsi_Host *shpnt;

	
	int slot;
	u_char pos2, pos3, pos4;
	int id, port, irq;
	unsigned long bios;

	
	if (!MCA_bus)
		return 0;

	
	id = 7;

	for (loop = 0; loop < FD_BRDS; loop++) {
		slot = 0;
		while (MCA_NOTFOUND != (slot = mca_find_adapter(fd_mcs_adapters[loop].id, slot))) {


			printk(KERN_INFO "scsi  <fd_mcs>: %s at slot %d\n", fd_mcs_adapters[loop].name, slot + 1);

			pos2 = mca_read_stored_pos(slot, 2);
			pos3 = mca_read_stored_pos(slot, 3);
			pos4 = mca_read_stored_pos(slot, 4);

			
			slot++;

			if (fd_mcs_adapters[loop].id == REPLY_ID) {	
				static int reply_irq[] = { 10, 11, 14, 15 };

				bios = 0;	

				if (pos2 & 0x2)
					port = ports[pos4 & 0x3];
				else
					continue;

				
				irq = reply_irq[((pos4 >> 2) & 0x1) + 2 * ((pos4 >> 4) & 0x1)];
			} else {
				bios = addresses[pos2 >> 6];
				port = ports[(pos2 >> 4) & 0x03];
				irq = interrupts[(pos2 >> 1) & 0x07];
			}

			if (irq) {
				
				mca_set_adapter_name(slot - 1, fd_mcs_adapters[loop].name);

				
				if (request_irq(irq, fd_mcs_intr, IRQF_SHARED, "fd_mcs", hosts)) {
					printk(KERN_ERR "fd_mcs: interrupt is not available, skipping...\n");
					continue;
				}

				
				if (request_region(port, 0x10, "fd_mcs")) {
					printk(KERN_ERR "fd_mcs: I/O region is already in use, skipping...\n");
					continue;
				}
				
				if (!(shpnt = scsi_register(tpnt, sizeof(struct fd_hostdata)))) {
					printk(KERN_ERR "fd_mcs: scsi_register() failed\n");
					release_region(port, 0x10);
					free_irq(irq, hosts);
					continue;
				}


				
				strcpy(adapter_name, fd_mcs_adapters[loop].name);

				
				chip = fd_mcs_adapters[loop].fd_chip;
				
				FIFO_COUNT = user_fifo_count ? user_fifo_count : fd_mcs_adapters[loop].fifo_count;
				FIFO_Size = user_fifo_size ? user_fifo_size : fd_mcs_adapters[loop].fifo_size;

#ifdef NOT_USED
				
				outb(0x80, port + IO_Control);
				if ((inb(port + Configuration2) & 0x80) == 0x80) {
					outb(0x00, port + IO_Control);
					if ((inb(port + Configuration2) & 0x80) == 0x00) {
						chip = tmc18c30;
						FIFO_Size = 0x800;	

						printk("FIRST: chip=%s, fifo_size=0x%x\n", (chip == tmc18c30) ? "tmc18c30" : "tmc18c50", FIFO_Size);
					}
				}


				if (inb(port + Configuration2) & 0x02) {
					chip = tmc18c30;
					FIFO_Size = 0x800;	

					printk("SECOND: chip=%s, fifo_size=0x%x\n", (chip == tmc18c30) ? "tmc18c30" : "tmc18c50", FIFO_Size);
				}
				
#endif

				
				
				shpnt->reverse_ordering = 1;


				
				hosts[found++] = shpnt;

				shpnt->this_id = id;
				shpnt->irq = irq;
				shpnt->io_port = port;
				shpnt->n_io_port = 0x10;

				
				bios_base = bios;
				adapter_mask = (1 << id);

				
				SCSI_Mode_Cntl_port = port + SCSI_Mode_Cntl;
				FIFO_Data_Count_port = port + FIFO_Data_Count;
				Interrupt_Cntl_port = port + Interrupt_Cntl;
				Interrupt_Status_port = port + Interrupt_Status;
				Interrupt_Cond_port = port + Interrupt_Cond;
				Read_FIFO_port = port + Read_FIFO;
				Read_SCSI_Data_port = port + Read_SCSI_Data;
				SCSI_Cntl_port = port + SCSI_Cntl;
				SCSI_Data_NoACK_port = port + SCSI_Data_NoACK;
				SCSI_Status_port = port + SCSI_Status;
				TMC_Cntl_port = port + TMC_Cntl;
				TMC_Status_port = port + TMC_Status;
				Write_FIFO_port = port + Write_FIFO;
				Write_SCSI_Data_port = port + Write_SCSI_Data;

				Bytes_Read = 0;
				Bytes_Written = 0;
				INTR_Processed = 0;

				
				print_banner(shpnt);

				
				outb(1, SCSI_Cntl_port);
				do_pause(2);
				outb(0, SCSI_Cntl_port);
				do_pause(115);
				outb(0, SCSI_Mode_Cntl_port);
				outb(PARITY_MASK, TMC_Cntl_port);
				
			}
		}

		if (found == FD_MAX_HOSTS) {
			printk("fd_mcs: detecting reached max=%d host adapters.\n", FD_MAX_HOSTS);
			break;
		}
	}

	return found;
}

static const char *fd_mcs_info(struct Scsi_Host *shpnt)
{
	return adapter_name;
}

static int TOTAL_INTR = 0;

/*
 * inout : decides on the direction of the dataflow and the meaning of the 
 *         variables
 * buffer: If inout==FALSE data is being written to it else read from it
 * *start: If inout==FALSE start of the valid data in the buffer
 * offset: If inout==FALSE offset from the beginning of the imaginary file 
 *         from which we start writing into the buffer
 * length: If inout==FALSE max number of bytes to be written into the buffer 
 *         else number of bytes in the buffer
 */
static int fd_mcs_proc_info(struct Scsi_Host *shpnt, char *buffer, char **start, off_t offset, int length, int inout)
{
	int len = 0;

	if (inout)
		return (-ENOSYS);

	*start = buffer + offset;

	len += sprintf(buffer + len, "Future Domain MCS-600/700 Driver %s\n", DRIVER_VERSION);
	len += sprintf(buffer + len, "HOST #%d: %s\n", shpnt->host_no, adapter_name);
	len += sprintf(buffer + len, "FIFO Size=0x%x, FIFO Count=%d\n", FIFO_Size, FIFO_COUNT);
	len += sprintf(buffer + len, "DriverCalls=%d, Interrupts=%d, BytesRead=%d, BytesWrite=%d\n\n", TOTAL_INTR, INTR_Processed, Bytes_Read, Bytes_Written);

	if ((len -= offset) <= 0)
		return 0;
	if (len > length)
		len = length;
	return len;
}

static int fd_mcs_select(struct Scsi_Host *shpnt, int target)
{
	int status;
	unsigned long timeout;

	outb(0x82, SCSI_Cntl_port);	
	outb(adapter_mask | (1 << target), SCSI_Data_NoACK_port);

	
	outb(PARITY_MASK, TMC_Cntl_port);

	timeout = 350;		

	do {
		status = inb(SCSI_Status_port);	
		if (status & 1) {	
			
			outb(0x80, SCSI_Cntl_port);
			return 0;
		}
		udelay(1000);	
	} while (--timeout);

	
	fd_mcs_make_bus_idle(shpnt);
#if EVERY_ACCESS
	if (!target)
		printk("Selection failed\n");
#endif
#if ERRORS_ONLY
	if (!target) {
		static int flag = 0;

		if (!flag)	
			++flag;
		else
			printk("fd_mcs: Selection failed\n");
	}
#endif
	return 1;
}

static void my_done(struct Scsi_Host *shpnt, int error)
{
	if (in_command) {
		in_command = 0;
		outb(0x00, Interrupt_Cntl_port);
		fd_mcs_make_bus_idle(shpnt);
		current_SC->result = error;
		current_SC->scsi_done(current_SC);
	} else {
		panic("fd_mcs: my_done() called outside of command\n");
	}
#if DEBUG_RACE
	in_interrupt_flag = 0;
#endif
}

static irqreturn_t fd_mcs_intr(int irq, void *dev_id)
{
	unsigned long flags;
	int status;
	int done = 0;
	unsigned data_count, tmp_count;

	int i = 0;
	struct Scsi_Host *shpnt;

	TOTAL_INTR++;

	
	while ((shpnt = hosts[i++])) {
		if ((inb(TMC_Status_port)) & 1)
			break;
	}

	
	if (!shpnt) {
		return IRQ_NONE;
	}

	INTR_Processed++;

	outb(0x00, Interrupt_Cntl_port);

	
	if (current_SC->SCp.phase & aborted) {
#if DEBUG_ABORT
		printk("Interrupt after abort, ignoring\n");
#endif
		
	}
#if DEBUG_RACE
	++in_interrupt_flag;
#endif

	if (current_SC->SCp.phase & in_arbitration) {
		status = inb(TMC_Status_port);	
		if (!(status & 0x02)) {
#if EVERY_ACCESS
			printk(" AFAIL ");
#endif
			spin_lock_irqsave(shpnt->host_lock, flags);
			my_done(shpnt, DID_BUS_BUSY << 16);
			spin_unlock_irqrestore(shpnt->host_lock, flags);
			return IRQ_HANDLED;
		}
		current_SC->SCp.phase = in_selection;

		outb(0x40 | FIFO_COUNT, Interrupt_Cntl_port);

		outb(0x82, SCSI_Cntl_port);	
		outb(adapter_mask | (1 << scmd_id(current_SC)), SCSI_Data_NoACK_port);

		
		outb(0x10 | PARITY_MASK, TMC_Cntl_port);
#if DEBUG_RACE
		in_interrupt_flag = 0;
#endif
		return IRQ_HANDLED;
	} else if (current_SC->SCp.phase & in_selection) {
		status = inb(SCSI_Status_port);
		if (!(status & 0x01)) {
			
			if (fd_mcs_select(shpnt, scmd_id(current_SC))) {
#if EVERY_ACCESS
				printk(" SFAIL ");
#endif
				spin_lock_irqsave(shpnt->host_lock, flags);
				my_done(shpnt, DID_NO_CONNECT << 16);
				spin_unlock_irqrestore(shpnt->host_lock, flags);
				return IRQ_HANDLED;
			} else {
#if EVERY_ACCESS
				printk(" AltSel ");
#endif
				
				outb(0x10 | PARITY_MASK, TMC_Cntl_port);
			}
		}
		current_SC->SCp.phase = in_other;
		outb(0x90 | FIFO_COUNT, Interrupt_Cntl_port);
		outb(0x80, SCSI_Cntl_port);
#if DEBUG_RACE
		in_interrupt_flag = 0;
#endif
		return IRQ_HANDLED;
	}

	

	status = inb(SCSI_Status_port);

	if (status & 0x10) {	

		switch (status & 0x0e) {

		case 0x08:	
			outb(current_SC->cmnd[current_SC->SCp.sent_command++], Write_SCSI_Data_port);
#if EVERY_ACCESS
			printk("CMD = %x,", current_SC->cmnd[current_SC->SCp.sent_command - 1]);
#endif
			break;
		case 0x00:	
			if (chip != tmc1800 && !current_SC->SCp.have_data_in) {
				current_SC->SCp.have_data_in = -1;
				outb(0xd0 | PARITY_MASK, TMC_Cntl_port);
			}
			break;
		case 0x04:	
			if (chip != tmc1800 && !current_SC->SCp.have_data_in) {
				current_SC->SCp.have_data_in = 1;
				outb(0x90 | PARITY_MASK, TMC_Cntl_port);
			}
			break;
		case 0x0c:	
			current_SC->SCp.Status = inb(Read_SCSI_Data_port);
#if EVERY_ACCESS
			printk("Status = %x, ", current_SC->SCp.Status);
#endif
#if ERRORS_ONLY
			if (current_SC->SCp.Status && current_SC->SCp.Status != 2 && current_SC->SCp.Status != 8) {
				printk("ERROR fd_mcs: target = %d, command = %x, status = %x\n", current_SC->device->id, current_SC->cmnd[0], current_SC->SCp.Status);
			}
#endif
			break;
		case 0x0a:	
			outb(MESSAGE_REJECT, Write_SCSI_Data_port);	
			break;
		case 0x0e:	
			current_SC->SCp.Message = inb(Read_SCSI_Data_port);
#if EVERY_ACCESS
			printk("Message = %x, ", current_SC->SCp.Message);
#endif
			if (!current_SC->SCp.Message)
				++done;
#if DEBUG_MESSAGES || EVERY_ACCESS
			if (current_SC->SCp.Message) {
				printk("fd_mcs: message = %x\n", current_SC->SCp.Message);
			}
#endif
			break;
		}
	}

	if (chip == tmc1800 && !current_SC->SCp.have_data_in && (current_SC->SCp.sent_command >= current_SC->cmd_len)) {

		switch (current_SC->cmnd[0]) {
		case CHANGE_DEFINITION:
		case COMPARE:
		case COPY:
		case COPY_VERIFY:
		case LOG_SELECT:
		case MODE_SELECT:
		case MODE_SELECT_10:
		case SEND_DIAGNOSTIC:
		case WRITE_BUFFER:

		case FORMAT_UNIT:
		case REASSIGN_BLOCKS:
		case RESERVE:
		case SEARCH_EQUAL:
		case SEARCH_HIGH:
		case SEARCH_LOW:
		case WRITE_6:
		case WRITE_10:
		case WRITE_VERIFY:
		case 0x3f:
		case 0x41:

		case 0xb1:
		case 0xb0:
		case 0xb2:
		case 0xaa:
		case 0xae:

		case 0x24:

		case 0x38:
		case 0x3d:

		case 0xb6:

		case 0xea:	

			current_SC->SCp.have_data_in = -1;
			outb(0xd0 | PARITY_MASK, TMC_Cntl_port);
			break;

		case 0x00:
		default:

			current_SC->SCp.have_data_in = 1;
			outb(0x90 | PARITY_MASK, TMC_Cntl_port);
			break;
		}
	}

	if (current_SC->SCp.have_data_in == -1) {	
		while ((data_count = FIFO_Size - inw(FIFO_Data_Count_port)) > 512) {
#if EVERY_ACCESS
			printk("DC=%d, ", data_count);
#endif
			if (data_count > current_SC->SCp.this_residual)
				data_count = current_SC->SCp.this_residual;
			if (data_count > 0) {
#if EVERY_ACCESS
				printk("%d OUT, ", data_count);
#endif
				if (data_count == 1) {
					Bytes_Written++;

					outb(*current_SC->SCp.ptr++, Write_FIFO_port);
					--current_SC->SCp.this_residual;
				} else {
					data_count >>= 1;
					tmp_count = data_count << 1;
					outsw(Write_FIFO_port, current_SC->SCp.ptr, data_count);
					current_SC->SCp.ptr += tmp_count;
					Bytes_Written += tmp_count;
					current_SC->SCp.this_residual -= tmp_count;
				}
			}
			if (!current_SC->SCp.this_residual) {
				if (current_SC->SCp.buffers_residual) {
					--current_SC->SCp.buffers_residual;
					++current_SC->SCp.buffer;
					current_SC->SCp.ptr = sg_virt(current_SC->SCp.buffer);
					current_SC->SCp.this_residual = current_SC->SCp.buffer->length;
				} else
					break;
			}
		}
	} else if (current_SC->SCp.have_data_in == 1) {	
		while ((data_count = inw(FIFO_Data_Count_port)) > 0) {
#if EVERY_ACCESS
			printk("DC=%d, ", data_count);
#endif
			if (data_count > current_SC->SCp.this_residual)
				data_count = current_SC->SCp.this_residual;
			if (data_count) {
#if EVERY_ACCESS
				printk("%d IN, ", data_count);
#endif
				if (data_count == 1) {
					Bytes_Read++;
					*current_SC->SCp.ptr++ = inb(Read_FIFO_port);
					--current_SC->SCp.this_residual;
				} else {
					data_count >>= 1;	
					tmp_count = data_count << 1;
					insw(Read_FIFO_port, current_SC->SCp.ptr, data_count);
					current_SC->SCp.ptr += tmp_count;
					Bytes_Read += tmp_count;
					current_SC->SCp.this_residual -= tmp_count;
				}
			}
			if (!current_SC->SCp.this_residual && current_SC->SCp.buffers_residual) {
				--current_SC->SCp.buffers_residual;
				++current_SC->SCp.buffer;
				current_SC->SCp.ptr = sg_virt(current_SC->SCp.buffer);
				current_SC->SCp.this_residual = current_SC->SCp.buffer->length;
			}
		}
	}

	if (done) {
#if EVERY_ACCESS
		printk(" ** IN DONE %d ** ", current_SC->SCp.have_data_in);
#endif

#if EVERY_ACCESS
		printk("BEFORE MY_DONE. . .");
#endif
		spin_lock_irqsave(shpnt->host_lock, flags);
		my_done(shpnt, (current_SC->SCp.Status & 0xff)
			| ((current_SC->SCp.Message & 0xff) << 8) | (DID_OK << 16));
		spin_unlock_irqrestore(shpnt->host_lock, flags);
#if EVERY_ACCESS
		printk("RETURNING.\n");
#endif

	} else {
		if (current_SC->SCp.phase & disconnect) {
			outb(0xd0 | FIFO_COUNT, Interrupt_Cntl_port);
			outb(0x00, SCSI_Cntl_port);
		} else {
			outb(0x90 | FIFO_COUNT, Interrupt_Cntl_port);
		}
	}
#if DEBUG_RACE
	in_interrupt_flag = 0;
#endif
	return IRQ_HANDLED;
}

static int fd_mcs_release(struct Scsi_Host *shpnt)
{
	int i, this_host, irq_usage;

	release_region(shpnt->io_port, shpnt->n_io_port);

	this_host = -1;
	irq_usage = 0;
	for (i = 0; i < found; i++) {
		if (shpnt == hosts[i])
			this_host = i;
		if (shpnt->irq == hosts[i]->irq)
			irq_usage++;
	}

	
	if (1 == irq_usage)
		free_irq(shpnt->irq, hosts);

	found--;

	for (i = this_host; i < found; i++)
		hosts[i] = hosts[i + 1];

	hosts[found] = NULL;

	return 0;
}

static int fd_mcs_queue_lck(Scsi_Cmnd * SCpnt, void (*done) (Scsi_Cmnd *))
{
	struct Scsi_Host *shpnt = SCpnt->device->host;

	if (in_command) {
		panic("fd_mcs: fd_mcs_queue() NOT REENTRANT!\n");
	}
#if EVERY_ACCESS
	printk("queue: target = %d cmnd = 0x%02x pieces = %d size = %u\n",
		SCpnt->target, *(unsigned char *) SCpnt->cmnd,
		scsi_sg_count(SCpnt), scsi_bufflen(SCpnt));
#endif

	fd_mcs_make_bus_idle(shpnt);

	SCpnt->scsi_done = done;	
	current_SC = SCpnt;

	

	if (scsi_bufflen(current_SC)) {
		current_SC->SCp.buffer = scsi_sglist(current_SC);
		current_SC->SCp.ptr = sg_virt(current_SC->SCp.buffer);
		current_SC->SCp.this_residual = current_SC->SCp.buffer->length;
		current_SC->SCp.buffers_residual = scsi_sg_count(current_SC) - 1;
	} else {
		current_SC->SCp.ptr = NULL;
		current_SC->SCp.this_residual = 0;
		current_SC->SCp.buffer = NULL;
		current_SC->SCp.buffers_residual = 0;
	}


	current_SC->SCp.Status = 0;
	current_SC->SCp.Message = 0;
	current_SC->SCp.have_data_in = 0;
	current_SC->SCp.sent_command = 0;
	current_SC->SCp.phase = in_arbitration;

	
	outb(0x00, Interrupt_Cntl_port);
	outb(0x00, SCSI_Cntl_port);	
	outb(adapter_mask, SCSI_Data_NoACK_port);	
	in_command = 1;
	outb(0x20, Interrupt_Cntl_port);
	outb(0x14 | PARITY_MASK, TMC_Cntl_port);	

	return 0;
}

static DEF_SCSI_QCMD(fd_mcs_queue)

#if DEBUG_ABORT || DEBUG_RESET
static void fd_mcs_print_info(Scsi_Cmnd * SCpnt)
{
	unsigned int imr;
	unsigned int irr;
	unsigned int isr;
	struct Scsi_Host *shpnt = SCpnt->host;

	if (!SCpnt || !SCpnt->host) {
		printk("fd_mcs: cannot provide detailed information\n");
	}

	printk("%s\n", fd_mcs_info(SCpnt->host));
	print_banner(SCpnt->host);
	switch (SCpnt->SCp.phase) {
	case in_arbitration:
		printk("arbitration ");
		break;
	case in_selection:
		printk("selection ");
		break;
	case in_other:
		printk("other ");
		break;
	default:
		printk("unknown ");
		break;
	}

	printk("(%d), target = %d cmnd = 0x%02x pieces = %d size = %u\n",
		SCpnt->SCp.phase, SCpnt->device->id, *(unsigned char *) SCpnt->cmnd,
		scsi_sg_count(SCpnt), scsi_bufflen(SCpnt));
	printk("sent_command = %d, have_data_in = %d, timeout = %d\n", SCpnt->SCp.sent_command, SCpnt->SCp.have_data_in, SCpnt->timeout);
#if DEBUG_RACE
	printk("in_interrupt_flag = %d\n", in_interrupt_flag);
#endif

	imr = (inb(0x0a1) << 8) + inb(0x21);
	outb(0x0a, 0xa0);
	irr = inb(0xa0) << 8;
	outb(0x0a, 0x20);
	irr += inb(0x20);
	outb(0x0b, 0xa0);
	isr = inb(0xa0) << 8;
	outb(0x0b, 0x20);
	isr += inb(0x20);

	
	printk("IMR = 0x%04x", imr);
	if (imr & (1 << shpnt->irq))
		printk(" (masked)");
	printk(", IRR = 0x%04x, ISR = 0x%04x\n", irr, isr);

	printk("SCSI Status      = 0x%02x\n", inb(SCSI_Status_port));
	printk("TMC Status       = 0x%02x", inb(TMC_Status_port));
	if (inb(TMC_Status_port) & 1)
		printk(" (interrupt)");
	printk("\n");
	printk("Interrupt Status = 0x%02x", inb(Interrupt_Status_port));
	if (inb(Interrupt_Status_port) & 0x08)
		printk(" (enabled)");
	printk("\n");
	if (chip == tmc18c50 || chip == tmc18c30) {
		printk("FIFO Status      = 0x%02x\n", inb(shpnt->io_port + FIFO_Status));
		printk("Int. Condition   = 0x%02x\n", inb(shpnt->io_port + Interrupt_Cond));
	}
	printk("Configuration 1  = 0x%02x\n", inb(shpnt->io_port + Configuration1));
	if (chip == tmc18c50 || chip == tmc18c30)
		printk("Configuration 2  = 0x%02x\n", inb(shpnt->io_port + Configuration2));
}
#endif

static int fd_mcs_abort(Scsi_Cmnd * SCpnt)
{
	struct Scsi_Host *shpnt = SCpnt->device->host;

	unsigned long flags;
#if EVERY_ACCESS || ERRORS_ONLY || DEBUG_ABORT
	printk("fd_mcs: abort ");
#endif

	spin_lock_irqsave(shpnt->host_lock, flags);
	if (!in_command) {
#if EVERY_ACCESS || ERRORS_ONLY
		printk(" (not in command)\n");
#endif
		spin_unlock_irqrestore(shpnt->host_lock, flags);
		return FAILED;
	} else
		printk("\n");

#if DEBUG_ABORT
	fd_mcs_print_info(SCpnt);
#endif

	fd_mcs_make_bus_idle(shpnt);

	current_SC->SCp.phase |= aborted;

	current_SC->result = DID_ABORT << 16;

	
	my_done(shpnt, DID_ABORT << 16);

	spin_unlock_irqrestore(shpnt->host_lock, flags);
	return SUCCESS;
}

static int fd_mcs_bus_reset(Scsi_Cmnd * SCpnt) {
	struct Scsi_Host *shpnt = SCpnt->device->host;
	unsigned long flags;

#if DEBUG_RESET
	static int called_once = 0;
#endif

#if ERRORS_ONLY
	if (SCpnt)
		printk("fd_mcs: SCSI Bus Reset\n");
#endif

#if DEBUG_RESET
	if (called_once)
		fd_mcs_print_info(current_SC);
	called_once = 1;
#endif

	spin_lock_irqsave(shpnt->host_lock, flags);

	outb(1, SCSI_Cntl_port);
	do_pause(2);
	outb(0, SCSI_Cntl_port);
	do_pause(115);
	outb(0, SCSI_Mode_Cntl_port);
	outb(PARITY_MASK, TMC_Cntl_port);

	spin_unlock_irqrestore(shpnt->host_lock, flags);

		return SUCCESS;
}

#include <scsi/scsi_ioctl.h>

static int fd_mcs_biosparam(struct scsi_device * disk, struct block_device *bdev,
			    sector_t capacity, int *info_array) 
{
	unsigned char *p = scsi_bios_ptable(bdev);
	int size = capacity;

	
	

	if (p && p[65] == 0xaa && p[64] == 0x55	
	    && p[4]) {	

		info_array[0] = p[5] + 1;	
		info_array[1] = p[6] & 0x3f;	
	} else {
		if ((unsigned int) size >= 0x7e0000U) 
		{
			info_array[0] = 0xff;	
			info_array[1] = 0x3f;	
		} else if ((unsigned int) size >= 0x200000U) {
			info_array[0] = 0x80;	
			info_array[1] = 0x3f;	
		} else {
			info_array[0] = 0x40;	
			info_array[1] = 0x20;	
		}
	}
	
	info_array[2] = (unsigned int) size / (info_array[0] * info_array[1]);
	kfree(p);
	return 0;
}

static struct scsi_host_template driver_template = {
	.proc_name			= "fd_mcs",
	.proc_info			= fd_mcs_proc_info,
	.detect				= fd_mcs_detect,
	.release			= fd_mcs_release,
	.info				= fd_mcs_info,
	.queuecommand   		= fd_mcs_queue, 
	.eh_abort_handler		= fd_mcs_abort,
	.eh_bus_reset_handler		= fd_mcs_bus_reset,
	.bios_param     		= fd_mcs_biosparam,
	.can_queue      		= 1,
	.this_id        		= 7,
	.sg_tablesize   		= 64,
	.cmd_per_lun    		= 1,
	.use_clustering 		= DISABLE_CLUSTERING,
};
#include "scsi_module.c"

MODULE_LICENSE("GPL");
