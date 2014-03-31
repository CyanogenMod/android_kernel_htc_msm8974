
#ifndef _PPA_H
#define _PPA_H

#define   PPA_VERSION   "2.07 (for Linux 2.4.x)"


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

#define   PPA_AUTODETECT        0	
#define   PPA_NIBBLE            1	
#define   PPA_PS2               2	
#define   PPA_EPP_8             3	
#define   PPA_EPP_16            4	
#define   PPA_EPP_32            5	
#define   PPA_UNKNOWN           6	

static char *PPA_MODE_STRING[] =
{
    "Autodetect",
    "SPP",
    "PS/2",
    "EPP 8 bit",
    "EPP 16 bit",
#ifdef CONFIG_SCSI_IZIP_EPP16
    "EPP 16 bit",
#else
    "EPP 32 bit",
#endif
    "Unknown"};

#define PPA_BURST_SIZE	512	
#define PPA_SELECT_TMO  5000	
#define PPA_SPIN_TMO    50000	
#define PPA_RECON_TMO   500	
#define PPA_DEBUG	0	
#define IN_EPP_MODE(x) (x == PPA_EPP_8 || x == PPA_EPP_16 || x == PPA_EPP_32)

#define CONNECT_EPP_MAYBE 1
#define CONNECT_NORMAL  0

#define r_dtr(x)        (unsigned char)inb((x))
#define r_str(x)        (unsigned char)inb((x)+1)
#define r_ctr(x)        (unsigned char)inb((x)+2)
#define r_epp(x)        (unsigned char)inb((x)+4)
#define r_fifo(x)       (unsigned char)inb((x)) 
					
#define r_ecr(x)        (unsigned char)inb((x)+0x2) 

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

static int ppa_engine(ppa_struct *, struct scsi_cmnd *);

#endif				
