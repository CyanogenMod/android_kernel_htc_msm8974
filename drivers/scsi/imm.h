

#ifndef _IMM_H
#define _IMM_H

#define   IMM_VERSION   "2.05 (for Linux 2.4.0)"


#include  <linux/stddef.h>
#include  <linux/module.h>
#include  <linux/kernel.h>
#include  <linux/ioport.h>
#include  <linux/delay.h>
#include  <linux/proc_fs.h>
#include  <linux/stat.h>
#include  <linux/blkdev.h>
#include  <linux/sched.h>
#include  <linux/interrupt.h>

#include  <asm/io.h>
#include  <scsi/scsi_host.h>

#define   IMM_AUTODETECT        0	
#define   IMM_NIBBLE            1	
#define   IMM_PS2               2	
#define   IMM_EPP_8             3	
#define   IMM_EPP_16            4	
#define   IMM_EPP_32            5	
#define   IMM_UNKNOWN           6	

static char *IMM_MODE_STRING[] =
{
	[IMM_AUTODETECT] = "Autodetect",
	[IMM_NIBBLE]	 = "SPP",
	[IMM_PS2]	 = "PS/2",
	[IMM_EPP_8]	 = "EPP 8 bit",
	[IMM_EPP_16]	 = "EPP 16 bit",
#ifdef CONFIG_SCSI_IZIP_EPP16
	[IMM_EPP_32]	 = "EPP 16 bit",
#else
	[IMM_EPP_32]	 = "EPP 32 bit",
#endif
	[IMM_UNKNOWN]	 = "Unknown",
};

#define IMM_BURST_SIZE	512	
#define IMM_SELECT_TMO  500	
#define IMM_SPIN_TMO    5000	
#define IMM_DEBUG	0	
#define IN_EPP_MODE(x) (x == IMM_EPP_8 || x == IMM_EPP_16 || x == IMM_EPP_32)

#define CONNECT_EPP_MAYBE 1
#define CONNECT_NORMAL  0

#define r_dtr(x)        (unsigned char)inb((x))
#define r_str(x)        (unsigned char)inb((x)+1)
#define r_ctr(x)        (unsigned char)inb((x)+2)
#define r_epp(x)        (unsigned char)inb((x)+4)
#define r_fifo(x)       (unsigned char)inb((x))   
					
#define r_ecr(x)        (unsigned char)inb((x)+2) 

#define w_dtr(x,y)      outb(y, (x))
#define w_str(x,y)      outb(y, (x)+1)
#define w_epp(x,y)      outb(y, (x)+4)
#define w_fifo(x,y)     outb(y, (x))     
#define w_ecr(x,y)      outb(y, (x)+0x2) 

#ifdef CONFIG_SCSI_IZIP_SLOW_CTR
#define w_ctr(x,y)      outb_p(y, (x)+2)
#else
#define w_ctr(x,y)      outb(y, (x)+2)
#endif

static int imm_engine(imm_struct *, struct scsi_cmnd *);

#endif				
