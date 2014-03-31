/* $Id: aha1542.c,v 1.1 1992/07/24 06:27:38 root Exp root $
 *  linux/kernel/aha1542.c
 *
 *  Copyright (C) 1992  Tommy Thorn
 *  Copyright (C) 1993, 1994, 1995 Eric Youngdale
 *
 *  Modified by Eric Youngdale
 *        Use request_irq and request_dma to help prevent unexpected conflicts
 *        Set up on-board DMA controller, such that we do not have to
 *        have the bios enabled to use the aha1542.
 *  Modified by David Gentzel
 *        Don't call request_dma if dma mask is 0 (for BusLogic BT-445S VL-Bus
 *        controller).
 *  Modified by Matti Aarnio
 *        Accept parameters from LILO cmd-line. -- 1-Oct-94
 *  Modified by Mike McLagan <mike.mclagan@linux.org>
 *        Recognise extended mode on AHA1542CP, different bit than 1542CF
 *        1-Jan-97
 *  Modified by Bjorn L. Thordarson and Einar Thor Einarsson
 *        Recognize that DMA0 is valid DMA channel -- 13-Jul-98
 *  Modified by Chris Faulhaber <jedgar@fxp.org>
 *        Added module command-line options
 *        19-Jul-99
 *  Modified by Adam Fritzler
 *        Added proper detection of the AHA-1640 (MCA version of AHA-1540)
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/isapnp.h>
#include <linux/blkdev.h>
#include <linux/mca.h>
#include <linux/mca-legacy.h>
#include <linux/slab.h>

#include <asm/dma.h>
#include <asm/io.h>

#include "scsi.h"
#include <scsi/scsi_host.h>
#include "aha1542.h"

#define SCSI_BUF_PA(address)	isa_virt_to_bus(address)
#define SCSI_SG_PA(sgent)	(isa_page_to_bus(sg_page((sgent))) + (sgent)->offset)

#include<linux/stat.h>

#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif



#define MAXBOARDS 4		


static unsigned int bases[MAXBOARDS] __initdata = {0x330, 0x334, 0, 0};


static int setup_called[MAXBOARDS];
static int setup_buson[MAXBOARDS];
static int setup_busoff[MAXBOARDS];
static int setup_dmaspeed[MAXBOARDS] __initdata = { -1, -1, -1, -1 };


#if defined(MODULE)
static bool isapnp = 0;
static int aha1542[] = {0x330, 11, 4, -1};
module_param_array(aha1542, int, NULL, 0);
module_param(isapnp, bool, 0);

static struct isapnp_device_id id_table[] __initdata = {
	{
		ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('A', 'D', 'P'), ISAPNP_FUNCTION(0x1542),
		0
	},
	{0}
};

MODULE_DEVICE_TABLE(isapnp, id_table);

#else
static int isapnp = 1;
#endif

#define BIOS_TRANSLATION_1632 0	
#define BIOS_TRANSLATION_6432 1	
#define BIOS_TRANSLATION_25563 2	

struct aha1542_hostdata {
	
	int bios_translation;	
	int aha1542_last_mbi_used;
	int aha1542_last_mbo_used;
	Scsi_Cmnd *SCint[AHA1542_MAILBOXES];
	struct mailbox mb[2 * AHA1542_MAILBOXES];
	struct ccb ccb[AHA1542_MAILBOXES];
};

#define HOSTDATA(host) ((struct aha1542_hostdata *) &host->hostdata)

static DEFINE_SPINLOCK(aha1542_lock);



#define WAITnexttimeout 3000000

static void setup_mailboxes(int base_io, struct Scsi_Host *shpnt);
static int aha1542_restart(struct Scsi_Host *shost);
static void aha1542_intr_handle(struct Scsi_Host *shost);

#define aha1542_intr_reset(base)  outb(IRST, CONTROL(base))

#define WAIT(port, mask, allof, noneof)					\
 { register int WAITbits;						\
   register int WAITtimeout = WAITnexttimeout;				\
   while (1) {								\
     WAITbits = inb(port) & (mask);					\
     if ((WAITbits & (allof)) == (allof) && ((WAITbits & (noneof)) == 0)) \
       break;                                                         	\
     if (--WAITtimeout == 0) goto fail;					\
   }									\
 }

#define WAITd(port, mask, allof, noneof, timeout)			\
 { register int WAITbits;						\
   register int WAITtimeout = timeout;					\
   while (1) {								\
     WAITbits = inb(port) & (mask);					\
     if ((WAITbits & (allof)) == (allof) && ((WAITbits & (noneof)) == 0)) \
       break;                                                         	\
     mdelay(1);							\
     if (--WAITtimeout == 0) goto fail;					\
   }									\
 }

static void aha1542_stat(void)
{
}

static int aha1542_out(unsigned int base, unchar * cmdp, int len)
{
	unsigned long flags = 0;
	int got_lock;

	if (len == 1) {
		got_lock = 0;
		while (1 == 1) {
			WAIT(STATUS(base), CDF, 0, CDF);
			spin_lock_irqsave(&aha1542_lock, flags);
			if (inb(STATUS(base)) & CDF) {
				spin_unlock_irqrestore(&aha1542_lock, flags);
				continue;
			}
			outb(*cmdp, DATA(base));
			spin_unlock_irqrestore(&aha1542_lock, flags);
			return 0;
		}
	} else {
		spin_lock_irqsave(&aha1542_lock, flags);
		got_lock = 1;
		while (len--) {
			WAIT(STATUS(base), CDF, 0, CDF);
			outb(*cmdp++, DATA(base));
		}
		spin_unlock_irqrestore(&aha1542_lock, flags);
	}
	return 0;
fail:
	if (got_lock)
		spin_unlock_irqrestore(&aha1542_lock, flags);
	printk(KERN_ERR "aha1542_out failed(%d): ", len + 1);
	aha1542_stat();
	return 1;
}


static int __init aha1542_in(unsigned int base, unchar * cmdp, int len)
{
	unsigned long flags;

	spin_lock_irqsave(&aha1542_lock, flags);
	while (len--) {
		WAIT(STATUS(base), DF, DF, 0);
		*cmdp++ = inb(DATA(base));
	}
	spin_unlock_irqrestore(&aha1542_lock, flags);
	return 0;
fail:
	spin_unlock_irqrestore(&aha1542_lock, flags);
	printk(KERN_ERR "aha1542_in failed(%d): ", len + 1);
	aha1542_stat();
	return 1;
}

static int __init aha1542_in1(unsigned int base, unchar * cmdp, int len)
{
	unsigned long flags;

	spin_lock_irqsave(&aha1542_lock, flags);
	while (len--) {
		WAITd(STATUS(base), DF, DF, 0, 100);
		*cmdp++ = inb(DATA(base));
	}
	spin_unlock_irqrestore(&aha1542_lock, flags);
	return 0;
fail:
	spin_unlock_irqrestore(&aha1542_lock, flags);
	return 1;
}

static int makecode(unsigned hosterr, unsigned scsierr)
{
	switch (hosterr) {
	case 0x0:
	case 0xa:		
	case 0xb:		
		hosterr = 0;
		break;

	case 0x11:		
		hosterr = DID_TIME_OUT;
		break;

	case 0x12:		

	case 0x13:		

	case 0x15:		

	case 0x16:		

	case 0x17:		
	case 0x18:		

	case 0x19:		

	case 0x1a:		
		DEB(printk("Aha1542: %x %x\n", hosterr, scsierr));
		hosterr = DID_ERROR;	
		break;

	case 0x14:		
		hosterr = DID_RESET;
		break;
	default:
		printk(KERN_ERR "aha1542: makecode: unknown hoststatus %x\n", hosterr);
		break;
	}
	return scsierr | (hosterr << 16);
}

static int __init aha1542_test_port(int bse, struct Scsi_Host *shpnt)
{
	unchar inquiry_cmd[] = {CMD_INQUIRY};
	unchar inquiry_result[4];
	unchar *cmdp;
	int len;
	volatile int debug = 0;

	
	if (inb(STATUS(bse)) == 0xff)
		return 0;

	

	

	
	aha1542_intr_reset(bse);	

	outb(SRST | IRST  , CONTROL(bse));

	mdelay(20);		

	debug = 1;
	
	WAIT(STATUS(bse), STATMASK, INIT | IDLE, STST | DIAGF | INVDCMD | DF | CDF);

	debug = 2;
	
	if (inb(INTRFLAGS(bse)) & INTRMASK)
		goto fail;



	aha1542_out(bse, inquiry_cmd, 1);

	debug = 3;
	len = 4;
	cmdp = &inquiry_result[0];

	while (len--) {
		WAIT(STATUS(bse), DF, DF, 0);
		*cmdp++ = inb(DATA(bse));
	}

	debug = 8;
	
	if (inb(STATUS(bse)) & DF)
		goto fail;

	debug = 9;
	
	WAIT(INTRFLAGS(bse), HACC, HACC, 0);
	

	debug = 10;
	
	outb(IRST, CONTROL(bse));

	debug = 11;

	return debug;		
fail:
	return 0;		
}

static irqreturn_t do_aha1542_intr_handle(int dummy, void *dev_id)
{
	unsigned long flags;
	struct Scsi_Host *shost = dev_id;

	spin_lock_irqsave(shost->host_lock, flags);
	aha1542_intr_handle(shost);
	spin_unlock_irqrestore(shost->host_lock, flags);
	return IRQ_HANDLED;
}

static void aha1542_intr_handle(struct Scsi_Host *shost)
{
	void (*my_done) (Scsi_Cmnd *) = NULL;
	int errstatus, mbi, mbo, mbistatus;
	int number_serviced;
	unsigned long flags;
	Scsi_Cmnd *SCtmp;
	int flag;
	int needs_restart;
	struct mailbox *mb;
	struct ccb *ccb;

	mb = HOSTDATA(shost)->mb;
	ccb = HOSTDATA(shost)->ccb;

#ifdef DEBUG
	{
		flag = inb(INTRFLAGS(shost->io_port));
		printk(KERN_DEBUG "aha1542_intr_handle: ");
		if (!(flag & ANYINTR))
			printk("no interrupt?");
		if (flag & MBIF)
			printk("MBIF ");
		if (flag & MBOA)
			printk("MBOF ");
		if (flag & HACC)
			printk("HACC ");
		if (flag & SCRD)
			printk("SCRD ");
		printk("status %02x\n", inb(STATUS(shost->io_port)));
	};
#endif
	number_serviced = 0;
	needs_restart = 0;

	while (1 == 1) {
		flag = inb(INTRFLAGS(shost->io_port));

		if (flag & ~MBIF) {
			if (flag & MBOA)
				printk("MBOF ");
			if (flag & HACC)
				printk("HACC ");
			if (flag & SCRD) {
				needs_restart = 1;
				printk("SCRD ");
			}
		}
		aha1542_intr_reset(shost->io_port);

		spin_lock_irqsave(&aha1542_lock, flags);
		mbi = HOSTDATA(shost)->aha1542_last_mbi_used + 1;
		if (mbi >= 2 * AHA1542_MAILBOXES)
			mbi = AHA1542_MAILBOXES;

		do {
			if (mb[mbi].status != 0)
				break;
			mbi++;
			if (mbi >= 2 * AHA1542_MAILBOXES)
				mbi = AHA1542_MAILBOXES;
		} while (mbi != HOSTDATA(shost)->aha1542_last_mbi_used);

		if (mb[mbi].status == 0) {
			spin_unlock_irqrestore(&aha1542_lock, flags);
			
			if (!number_serviced && !needs_restart)
				printk(KERN_WARNING "aha1542.c: interrupt received, but no mail.\n");
			if (needs_restart)
				aha1542_restart(shost);
			return;
		};

		mbo = (scsi2int(mb[mbi].ccbptr) - (SCSI_BUF_PA(&ccb[0]))) / sizeof(struct ccb);
		mbistatus = mb[mbi].status;
		mb[mbi].status = 0;
		HOSTDATA(shost)->aha1542_last_mbi_used = mbi;
		spin_unlock_irqrestore(&aha1542_lock, flags);

#ifdef DEBUG
		{
			if (ccb[mbo].tarstat | ccb[mbo].hastat)
				printk(KERN_DEBUG "aha1542_command: returning %x (status %d)\n",
				       ccb[mbo].tarstat + ((int) ccb[mbo].hastat << 16), mb[mbi].status);
		};
#endif

		if (mbistatus == 3)
			continue;	

#ifdef DEBUG
		printk(KERN_DEBUG "...done %d %d\n", mbo, mbi);
#endif

		SCtmp = HOSTDATA(shost)->SCint[mbo];

		if (!SCtmp || !SCtmp->scsi_done) {
			printk(KERN_WARNING "aha1542_intr_handle: Unexpected interrupt\n");
			printk(KERN_WARNING "tarstat=%x, hastat=%x idlun=%x ccb#=%d \n", ccb[mbo].tarstat,
			       ccb[mbo].hastat, ccb[mbo].idlun, mbo);
			return;
		}
		my_done = SCtmp->scsi_done;
		kfree(SCtmp->host_scribble);
		SCtmp->host_scribble = NULL;
		if (ccb[mbo].tarstat == 2)
			memcpy(SCtmp->sense_buffer, &ccb[mbo].cdb[ccb[mbo].cdblen],
			       SCSI_SENSE_BUFFERSIZE);


		

		
		if (mbistatus != 1)
			
			errstatus = makecode(ccb[mbo].hastat, ccb[mbo].tarstat);
		else
			errstatus = 0;

#ifdef DEBUG
		if (errstatus)
			printk(KERN_DEBUG "(aha1542 error:%x %x %x) ", errstatus,
			       ccb[mbo].hastat, ccb[mbo].tarstat);
#endif

		if (ccb[mbo].tarstat == 2) {
#ifdef DEBUG
			int i;
#endif
			DEB(printk("aha1542_intr_handle: sense:"));
#ifdef DEBUG
			for (i = 0; i < 12; i++)
				printk("%02x ", ccb[mbo].cdb[ccb[mbo].cdblen + i]);
			printk("\n");
#endif
		}
		DEB(if (errstatus) printk("aha1542_intr_handle: returning %6x\n", errstatus));
		SCtmp->result = errstatus;
		HOSTDATA(shost)->SCint[mbo] = NULL;	
		my_done(SCtmp);
		number_serviced++;
	};
}

static int aha1542_queuecommand_lck(Scsi_Cmnd * SCpnt, void (*done) (Scsi_Cmnd *))
{
	unchar ahacmd = CMD_START_SCSI;
	unchar direction;
	unchar *cmd = (unchar *) SCpnt->cmnd;
	unchar target = SCpnt->device->id;
	unchar lun = SCpnt->device->lun;
	unsigned long flags;
	int bufflen = scsi_bufflen(SCpnt);
	int mbo;
	struct mailbox *mb;
	struct ccb *ccb;

	DEB(int i);

	mb = HOSTDATA(SCpnt->device->host)->mb;
	ccb = HOSTDATA(SCpnt->device->host)->ccb;

	DEB(if (target > 1) {
	    SCpnt->result = DID_TIME_OUT << 16;
	    done(SCpnt); return 0;
	    }
	);

	if (*cmd == REQUEST_SENSE) {
		
#if 0
		if (bufflen != SCSI_SENSE_BUFFERSIZE)
			printk(KERN_CRIT "aha1542: Wrong buffer length supplied "
			       "for request sense (%d)\n", bufflen);
#endif
		SCpnt->result = 0;
		done(SCpnt);
		return 0;
	}
#ifdef DEBUG
	if (*cmd == READ_10 || *cmd == WRITE_10)
		i = xscsi2int(cmd + 2);
	else if (*cmd == READ_6 || *cmd == WRITE_6)
		i = scsi2int(cmd + 2);
	else
		i = -1;
	if (done)
		printk(KERN_DEBUG "aha1542_queuecommand: dev %d cmd %02x pos %d len %d ", target, *cmd, i, bufflen);
	else
		printk(KERN_DEBUG "aha1542_command: dev %d cmd %02x pos %d len %d ", target, *cmd, i, bufflen);
	aha1542_stat();
	printk(KERN_DEBUG "aha1542_queuecommand: dumping scsi cmd:");
	for (i = 0; i < SCpnt->cmd_len; i++)
		printk("%02x ", cmd[i]);
	printk("\n");
	if (*cmd == WRITE_10 || *cmd == WRITE_6)
		return 0;	
#endif

	spin_lock_irqsave(&aha1542_lock, flags);
	mbo = HOSTDATA(SCpnt->device->host)->aha1542_last_mbo_used + 1;
	if (mbo >= AHA1542_MAILBOXES)
		mbo = 0;

	do {
		if (mb[mbo].status == 0 && HOSTDATA(SCpnt->device->host)->SCint[mbo] == NULL)
			break;
		mbo++;
		if (mbo >= AHA1542_MAILBOXES)
			mbo = 0;
	} while (mbo != HOSTDATA(SCpnt->device->host)->aha1542_last_mbo_used);

	if (mb[mbo].status || HOSTDATA(SCpnt->device->host)->SCint[mbo])
		panic("Unable to find empty mailbox for aha1542.\n");

	HOSTDATA(SCpnt->device->host)->SCint[mbo] = SCpnt;	

	HOSTDATA(SCpnt->device->host)->aha1542_last_mbo_used = mbo;
	spin_unlock_irqrestore(&aha1542_lock, flags);

#ifdef DEBUG
	printk(KERN_DEBUG "Sending command (%d %x)...", mbo, done);
#endif

	any2scsi(mb[mbo].ccbptr, SCSI_BUF_PA(&ccb[mbo]));	

	memset(&ccb[mbo], 0, sizeof(struct ccb));

	ccb[mbo].cdblen = SCpnt->cmd_len;

	direction = 0;
	if (*cmd == READ_10 || *cmd == READ_6)
		direction = 8;
	else if (*cmd == WRITE_10 || *cmd == WRITE_6)
		direction = 16;

	memcpy(ccb[mbo].cdb, cmd, ccb[mbo].cdblen);

	if (bufflen) {
		struct scatterlist *sg;
		struct chain *cptr;
#ifdef DEBUG
		unsigned char *ptr;
#endif
		int i, sg_count = scsi_sg_count(SCpnt);
		ccb[mbo].op = 2;	
		SCpnt->host_scribble = kmalloc(sizeof(*cptr)*sg_count,
		                                         GFP_KERNEL | GFP_DMA);
		cptr = (struct chain *) SCpnt->host_scribble;
		if (cptr == NULL) {
			
			HOSTDATA(SCpnt->device->host)->SCint[mbo] = NULL;
			return SCSI_MLQUEUE_HOST_BUSY;
		}
		scsi_for_each_sg(SCpnt, sg, sg_count, i) {
			any2scsi(cptr[i].dataptr, SCSI_SG_PA(sg));
			any2scsi(cptr[i].datalen, sg->length);
		};
		any2scsi(ccb[mbo].datalen, sg_count * sizeof(struct chain));
		any2scsi(ccb[mbo].dataptr, SCSI_BUF_PA(cptr));
#ifdef DEBUG
		printk("cptr %x: ", cptr);
		ptr = (unsigned char *) cptr;
		for (i = 0; i < 18; i++)
			printk("%02x ", ptr[i]);
#endif
	} else {
		ccb[mbo].op = 0;	
		SCpnt->host_scribble = NULL;
		any2scsi(ccb[mbo].datalen, 0);
		any2scsi(ccb[mbo].dataptr, 0);
	};
	ccb[mbo].idlun = (target & 7) << 5 | direction | (lun & 7);	
	ccb[mbo].rsalen = 16;
	ccb[mbo].linkptr[0] = ccb[mbo].linkptr[1] = ccb[mbo].linkptr[2] = 0;
	ccb[mbo].commlinkid = 0;

#ifdef DEBUG
	{
		int i;
		printk(KERN_DEBUG "aha1542_command: sending.. ");
		for (i = 0; i < sizeof(ccb[mbo]) - 10; i++)
			printk("%02x ", ((unchar *) & ccb[mbo])[i]);
	};
#endif

	if (done) {
		DEB(printk("aha1542_queuecommand: now waiting for interrupt ");
		    aha1542_stat());
		SCpnt->scsi_done = done;
		mb[mbo].status = 1;
		aha1542_out(SCpnt->device->host->io_port, &ahacmd, 1);	
		DEB(aha1542_stat());
	} else
		printk("aha1542_queuecommand: done can't be NULL\n");

	return 0;
}

static DEF_SCSI_QCMD(aha1542_queuecommand)

static void setup_mailboxes(int bse, struct Scsi_Host *shpnt)
{
	int i;
	struct mailbox *mb;
	struct ccb *ccb;

	unchar cmd[5] = { CMD_MBINIT, AHA1542_MAILBOXES, 0, 0, 0};

	mb = HOSTDATA(shpnt)->mb;
	ccb = HOSTDATA(shpnt)->ccb;

	for (i = 0; i < AHA1542_MAILBOXES; i++) {
		mb[i].status = mb[AHA1542_MAILBOXES + i].status = 0;
		any2scsi(mb[i].ccbptr, SCSI_BUF_PA(&ccb[i]));
	};
	aha1542_intr_reset(bse);	
	any2scsi((cmd + 2), SCSI_BUF_PA(mb));
	aha1542_out(bse, cmd, 5);
	WAIT(INTRFLAGS(bse), INTRMASK, HACC, 0);
	while (0) {
fail:
		printk(KERN_ERR "aha1542_detect: failed setting up mailboxes\n");
	}
	aha1542_intr_reset(bse);
}

static int __init aha1542_getconfig(int base_io, unsigned char *irq_level, unsigned char *dma_chan, unsigned char *scsi_id)
{
	unchar inquiry_cmd[] = {CMD_RETCONF};
	unchar inquiry_result[3];
	int i;
	i = inb(STATUS(base_io));
	if (i & DF) {
		i = inb(DATA(base_io));
	};
	aha1542_out(base_io, inquiry_cmd, 1);
	aha1542_in(base_io, inquiry_result, 3);
	WAIT(INTRFLAGS(base_io), INTRMASK, HACC, 0);
	while (0) {
fail:
		printk(KERN_ERR "aha1542_detect: query board settings\n");
	}
	aha1542_intr_reset(base_io);
	switch (inquiry_result[0]) {
	case 0x80:
		*dma_chan = 7;
		break;
	case 0x40:
		*dma_chan = 6;
		break;
	case 0x20:
		*dma_chan = 5;
		break;
	case 0x01:
		*dma_chan = 0;
		break;
	case 0:
		*dma_chan = 0xFF;
		break;
	default:
		printk(KERN_ERR "Unable to determine Adaptec DMA priority.  Disabling board\n");
		return -1;
	};
	switch (inquiry_result[1]) {
	case 0x40:
		*irq_level = 15;
		break;
	case 0x20:
		*irq_level = 14;
		break;
	case 0x8:
		*irq_level = 12;
		break;
	case 0x4:
		*irq_level = 11;
		break;
	case 0x2:
		*irq_level = 10;
		break;
	case 0x1:
		*irq_level = 9;
		break;
	default:
		printk(KERN_ERR "Unable to determine Adaptec IRQ level.  Disabling board\n");
		return -1;
	};
	*scsi_id = inquiry_result[2] & 7;
	return 0;
}


static int __init aha1542_mbenable(int base)
{
	static unchar mbenable_cmd[3];
	static unchar mbenable_result[2];
	int retval;

	retval = BIOS_TRANSLATION_6432;

	mbenable_cmd[0] = CMD_EXTBIOS;
	aha1542_out(base, mbenable_cmd, 1);
	if (aha1542_in1(base, mbenable_result, 2))
		return retval;
	WAITd(INTRFLAGS(base), INTRMASK, HACC, 0, 100);
	aha1542_intr_reset(base);

	if ((mbenable_result[0] & 0x08) || mbenable_result[1]) {
		mbenable_cmd[0] = CMD_MBENABLE;
		mbenable_cmd[1] = 0;
		mbenable_cmd[2] = mbenable_result[1];

		if ((mbenable_result[0] & 0x08) && (mbenable_result[1] & 0x03))
			retval = BIOS_TRANSLATION_25563;

		aha1542_out(base, mbenable_cmd, 3);
		WAIT(INTRFLAGS(base), INTRMASK, HACC, 0);
	};
	while (0) {
fail:
		printk(KERN_ERR "aha1542_mbenable: Mailbox init failed\n");
	}
	aha1542_intr_reset(base);
	return retval;
}

static int __init aha1542_query(int base_io, int *transl)
{
	unchar inquiry_cmd[] = {CMD_INQUIRY};
	unchar inquiry_result[4];
	int i;
	i = inb(STATUS(base_io));
	if (i & DF) {
		i = inb(DATA(base_io));
	};
	aha1542_out(base_io, inquiry_cmd, 1);
	aha1542_in(base_io, inquiry_result, 4);
	WAIT(INTRFLAGS(base_io), INTRMASK, HACC, 0);
	while (0) {
fail:
		printk(KERN_ERR "aha1542_detect: query card type\n");
	}
	aha1542_intr_reset(base_io);

	*transl = BIOS_TRANSLATION_6432;	


	if (inquiry_result[0] == 0x43) {
		printk(KERN_INFO "aha1542.c: Emulation mode not supported for AHA 174N hardware.\n");
		return 1;
	};


	*transl = aha1542_mbenable(base_io);

	return 0;
}

#ifndef MODULE
static char *setup_str[MAXBOARDS] __initdata;
static int setup_idx = 0;

static void __init aha1542_setup(char *str, int *ints)
{
	const char *ahausage = "aha1542: usage: aha1542=<PORTBASE>[,<BUSON>,<BUSOFF>[,<DMASPEED>]]\n";
	int setup_portbase;

	if (setup_idx >= MAXBOARDS) {
		printk(KERN_ERR "aha1542: aha1542_setup called too many times! Bad LILO params ?\n");
		printk(KERN_ERR "   Entryline 1: %s\n", setup_str[0]);
		printk(KERN_ERR "   Entryline 2: %s\n", setup_str[1]);
		printk(KERN_ERR "   This line:   %s\n", str);
		return;
	}
	if (ints[0] < 1 || ints[0] > 4) {
		printk(KERN_ERR "aha1542: %s\n", str);
		printk(ahausage);
		printk(KERN_ERR "aha1542: Wrong parameters may cause system malfunction.. We try anyway..\n");
	}
	setup_called[setup_idx] = ints[0];
	setup_str[setup_idx] = str;

	setup_portbase = ints[0] >= 1 ? ints[1] : 0;	
	setup_buson[setup_idx] = ints[0] >= 2 ? ints[2] : 7;
	setup_busoff[setup_idx] = ints[0] >= 3 ? ints[3] : 5;
	if (ints[0] >= 4) 
	{
		int atbt = -1;
		switch (ints[4]) {
		case 5:
			atbt = 0x00;
			break;
		case 6:
			atbt = 0x04;
			break;
		case 7:
			atbt = 0x01;
			break;
		case 8:
			atbt = 0x02;
			break;
		case 10:
			atbt = 0x03;
			break;
		default:
			printk(KERN_ERR "aha1542: %s\n", str);
			printk(ahausage);
			printk(KERN_ERR "aha1542: Valid values for DMASPEED are 5-8, 10 MB/s.  Using jumper defaults.\n");
			break;
		}
		setup_dmaspeed[setup_idx] = atbt;
	}
	if (setup_portbase != 0)
		bases[setup_idx] = setup_portbase;

	++setup_idx;
}

static int __init do_setup(char *str)
{
	int ints[5];

	int count=setup_idx;

	get_options(str, ARRAY_SIZE(ints), ints);
	aha1542_setup(str,ints);

	return count<setup_idx;
}

__setup("aha1542=",do_setup);
#endif

static int __init aha1542_detect(struct scsi_host_template * tpnt)
{
	unsigned char dma_chan;
	unsigned char irq_level;
	unsigned char scsi_id;
	unsigned long flags;
	unsigned int base_io;
	int trans;
	struct Scsi_Host *shpnt = NULL;
	int count = 0;
	int indx;

	DEB(printk("aha1542_detect: \n"));

	tpnt->proc_name = "aha1542";

#ifdef MODULE
	bases[0] = aha1542[0];
	setup_buson[0] = aha1542[1];
	setup_busoff[0] = aha1542[2];
	{
		int atbt = -1;
		switch (aha1542[3]) {
		case 5:
			atbt = 0x00;
			break;
		case 6:
			atbt = 0x04;
			break;
		case 7:
			atbt = 0x01;
			break;
		case 8:
			atbt = 0x02;
			break;
		case 10:
			atbt = 0x03;
			break;
		};
		setup_dmaspeed[0] = atbt;
	}
#endif

#ifdef CONFIG_MCA_LEGACY
	if(MCA_bus) {
		int slot = 0;
		int pos = 0;

		for (indx = 0; (slot != MCA_NOTFOUND) && (indx < ARRAY_SIZE(bases)); indx++) {

			if (bases[indx])
				continue;

			
			slot = mca_find_unused_adapter(0x0f1f, slot);
			if (slot == MCA_NOTFOUND)
				break;

			
			pos = mca_read_stored_pos(slot, 3);

			
			if (pos & 0x80) {
				if (pos & 0x02) {
					if (pos & 0x01)
						bases[indx] = 0x334;
					else
						bases[indx] = 0x234;
				} else {
					if (pos & 0x01)
						bases[indx] = 0x134;
				}
			} else {
				if (pos & 0x02) {
					if (pos & 0x01)
						bases[indx] = 0x330;
					else
						bases[indx] = 0x230;
				} else {
					if (pos & 0x01)
						bases[indx] = 0x130;
				}
			}

			printk(KERN_INFO "Found an AHA-1640 in MCA slot %d, I/O 0x%04x\n", slot, bases[indx]);

			mca_set_adapter_name(slot, "Adapter AHA-1640");
			mca_set_adapter_procfn(slot, NULL, NULL);
			mca_mark_as_used(slot);

			
			slot++;
		}

	}
#endif


	if(isapnp)
	{
		struct pnp_dev *pdev = NULL;
		for(indx = 0; indx < ARRAY_SIZE(bases); indx++) {
			if(bases[indx])
				continue;
			pdev = pnp_find_dev(NULL, ISAPNP_VENDOR('A', 'D', 'P'), 
				ISAPNP_FUNCTION(0x1542), pdev);
			if(pdev==NULL)
				break;

			if(pnp_device_attach(pdev)<0)
				continue;

			if(pnp_activate_dev(pdev)<0) {
				pnp_device_detach(pdev);
				continue;
			}

			if(!pnp_port_valid(pdev, 0)) {
				pnp_device_detach(pdev);
				continue;
			}

			bases[indx] = pnp_port_start(pdev, 0);


			printk(KERN_INFO "ISAPnP found an AHA1535 at I/O 0x%03X\n", bases[indx]);
		}
	}
	for (indx = 0; indx < ARRAY_SIZE(bases); indx++)
		if (bases[indx] != 0 && request_region(bases[indx], 4, "aha1542")) {
			shpnt = scsi_register(tpnt,
					sizeof(struct aha1542_hostdata));

			if(shpnt==NULL) {
				release_region(bases[indx], 4);
				continue;
			}
			if (!aha1542_test_port(bases[indx], shpnt))
				goto unregister;

			base_io = bases[indx];

			
			{
				unchar oncmd[] = {CMD_BUSON_TIME, 7};
				unchar offcmd[] = {CMD_BUSOFF_TIME, 5};

				if (setup_called[indx]) {
					oncmd[1] = setup_buson[indx];
					offcmd[1] = setup_busoff[indx];
				}
				aha1542_intr_reset(base_io);
				aha1542_out(base_io, oncmd, 2);
				WAIT(INTRFLAGS(base_io), INTRMASK, HACC, 0);
				aha1542_intr_reset(base_io);
				aha1542_out(base_io, offcmd, 2);
				WAIT(INTRFLAGS(base_io), INTRMASK, HACC, 0);
				if (setup_dmaspeed[indx] >= 0) {
					unchar dmacmd[] = {CMD_DMASPEED, 0};
					dmacmd[1] = setup_dmaspeed[indx];
					aha1542_intr_reset(base_io);
					aha1542_out(base_io, dmacmd, 2);
					WAIT(INTRFLAGS(base_io), INTRMASK, HACC, 0);
				}
				while (0) {
fail:
					printk(KERN_ERR "aha1542_detect: setting bus on/off-time failed\n");
				}
				aha1542_intr_reset(base_io);
			}
			if (aha1542_query(base_io, &trans))
				goto unregister;

			if (aha1542_getconfig(base_io, &irq_level, &dma_chan, &scsi_id) == -1)
				goto unregister;

			printk(KERN_INFO "Configuring Adaptec (SCSI-ID %d) at IO:%x, IRQ %d", scsi_id, base_io, irq_level);
			if (dma_chan != 0xFF)
				printk(", DMA priority %d", dma_chan);
			printk("\n");

			DEB(aha1542_stat());
			setup_mailboxes(base_io, shpnt);

			DEB(aha1542_stat());

			DEB(printk("aha1542_detect: enable interrupt channel %d\n", irq_level));
			spin_lock_irqsave(&aha1542_lock, flags);
			if (request_irq(irq_level, do_aha1542_intr_handle, 0,
					"aha1542", shpnt)) {
				printk(KERN_ERR "Unable to allocate IRQ for adaptec controller.\n");
				spin_unlock_irqrestore(&aha1542_lock, flags);
				goto unregister;
			}
			if (dma_chan != 0xFF) {
				if (request_dma(dma_chan, "aha1542")) {
					printk(KERN_ERR "Unable to allocate DMA channel for Adaptec.\n");
					free_irq(irq_level, shpnt);
					spin_unlock_irqrestore(&aha1542_lock, flags);
					goto unregister;
				}
				if (dma_chan == 0 || dma_chan >= 5) {
					set_dma_mode(dma_chan, DMA_MODE_CASCADE);
					enable_dma(dma_chan);
				}
			}

			shpnt->this_id = scsi_id;
			shpnt->unique_id = base_io;
			shpnt->io_port = base_io;
			shpnt->n_io_port = 4;	
			shpnt->dma_channel = dma_chan;
			shpnt->irq = irq_level;
			HOSTDATA(shpnt)->bios_translation = trans;
			if (trans == BIOS_TRANSLATION_25563)
				printk(KERN_INFO "aha1542.c: Using extended bios translation\n");
			HOSTDATA(shpnt)->aha1542_last_mbi_used = (2 * AHA1542_MAILBOXES - 1);
			HOSTDATA(shpnt)->aha1542_last_mbo_used = (AHA1542_MAILBOXES - 1);
			memset(HOSTDATA(shpnt)->SCint, 0, sizeof(HOSTDATA(shpnt)->SCint));
			spin_unlock_irqrestore(&aha1542_lock, flags);
#if 0
			DEB(printk(" *** READ CAPACITY ***\n"));

			{
				unchar buf[8];
				static unchar cmd[] = { READ_CAPACITY, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				int i;

				for (i = 0; i < sizeof(buf); ++i)
					buf[i] = 0x87;
				for (i = 0; i < 2; ++i)
					if (!aha1542_command(i, cmd, buf, sizeof(buf))) {
						printk(KERN_DEBUG "aha_detect: LU %d sector_size %d device_size %d\n",
						       i, xscsi2int(buf + 4), xscsi2int(buf));
					}
			}

			DEB(printk(" *** NOW RUNNING MY OWN TEST *** \n"));

			for (i = 0; i < 4; ++i) {
				unsigned char cmd[10];
				static buffer[512];

				cmd[0] = READ_10;
				cmd[1] = 0;
				xany2scsi(cmd + 2, i);
				cmd[6] = 0;
				cmd[7] = 0;
				cmd[8] = 1;
				cmd[9] = 0;
				aha1542_command(0, cmd, buffer, 512);
			}
#endif
			count++;
			continue;
unregister:
			release_region(bases[indx], 4);
			scsi_unregister(shpnt);
			continue;

		};

	return count;
}

static int aha1542_release(struct Scsi_Host *shost)
{
	if (shost->irq)
		free_irq(shost->irq, shost);
	if (shost->dma_channel != 0xff)
		free_dma(shost->dma_channel);
	if (shost->io_port && shost->n_io_port)
		release_region(shost->io_port, shost->n_io_port);
	scsi_unregister(shost);
	return 0;
}

static int aha1542_restart(struct Scsi_Host *shost)
{
	int i;
	int count = 0;
#if 0
	unchar ahacmd = CMD_START_SCSI;
#endif

	for (i = 0; i < AHA1542_MAILBOXES; i++)
		if (HOSTDATA(shost)->SCint[i] &&
		    !(HOSTDATA(shost)->SCint[i]->device->soft_reset)) {
#if 0
			HOSTDATA(shost)->mb[i].status = 1;	
#endif
			count++;
		}
	printk(KERN_DEBUG "Potential to restart %d stalled commands...\n", count);
#if 0
	
	if (count)
		aha1542_out(shost->io_port, &ahacmd, 1);
#endif
	return 0;
}

static int aha1542_dev_reset(Scsi_Cmnd * SCpnt)
{
	unsigned long flags;
	struct mailbox *mb;
	unchar target = SCpnt->device->id;
	unchar lun = SCpnt->device->lun;
	int mbo;
	struct ccb *ccb;
	unchar ahacmd = CMD_START_SCSI;

	ccb = HOSTDATA(SCpnt->device->host)->ccb;
	mb = HOSTDATA(SCpnt->device->host)->mb;

	spin_lock_irqsave(&aha1542_lock, flags);
	mbo = HOSTDATA(SCpnt->device->host)->aha1542_last_mbo_used + 1;
	if (mbo >= AHA1542_MAILBOXES)
		mbo = 0;

	do {
		if (mb[mbo].status == 0 && HOSTDATA(SCpnt->device->host)->SCint[mbo] == NULL)
			break;
		mbo++;
		if (mbo >= AHA1542_MAILBOXES)
			mbo = 0;
	} while (mbo != HOSTDATA(SCpnt->device->host)->aha1542_last_mbo_used);

	if (mb[mbo].status || HOSTDATA(SCpnt->device->host)->SCint[mbo])
		panic("Unable to find empty mailbox for aha1542.\n");

	HOSTDATA(SCpnt->device->host)->SCint[mbo] = SCpnt;	

	HOSTDATA(SCpnt->device->host)->aha1542_last_mbo_used = mbo;
	spin_unlock_irqrestore(&aha1542_lock, flags);

	any2scsi(mb[mbo].ccbptr, SCSI_BUF_PA(&ccb[mbo]));	

	memset(&ccb[mbo], 0, sizeof(struct ccb));

	ccb[mbo].op = 0x81;	

	ccb[mbo].idlun = (target & 7) << 5 | (lun & 7);		

	ccb[mbo].linkptr[0] = ccb[mbo].linkptr[1] = ccb[mbo].linkptr[2] = 0;
	ccb[mbo].commlinkid = 0;

	aha1542_out(SCpnt->device->host->io_port, &ahacmd, 1);

	scmd_printk(KERN_WARNING, SCpnt,
		"Trying device reset for target\n");

	return SUCCESS;


#ifdef ERIC_neverdef
	printk(KERN_WARNING "Sent BUS DEVICE RESET to target %d\n", SCpnt->target);

	for (i = 0; i < AHA1542_MAILBOXES; i++) {
		if (HOSTDATA(SCpnt->host)->SCint[i] &&
		    HOSTDATA(SCpnt->host)->SCint[i]->target == SCpnt->target) {
			Scsi_Cmnd *SCtmp;
			SCtmp = HOSTDATA(SCpnt->host)->SCint[i];
			kfree(SCtmp->host_scribble);
			SCtmp->host_scribble = NULL;
			HOSTDATA(SCpnt->host)->SCint[i] = NULL;
			HOSTDATA(SCpnt->host)->mb[i].status = 0;
		}
	}
	return SUCCESS;

	return FAILED;
#endif				
}

static int aha1542_bus_reset(Scsi_Cmnd * SCpnt)
{
	int i;

	outb(SCRST, CONTROL(SCpnt->device->host->io_port));

	ssleep(4);

	spin_lock_irq(SCpnt->device->host->host_lock);

	WAIT(STATUS(SCpnt->device->host->io_port),
	     STATMASK, INIT | IDLE, STST | DIAGF | INVDCMD | DF | CDF);

	printk(KERN_WARNING "Sent BUS RESET to scsi host %d\n", SCpnt->device->host->host_no);

	for (i = 0; i < AHA1542_MAILBOXES; i++) {
		if (HOSTDATA(SCpnt->device->host)->SCint[i] != NULL) {
			Scsi_Cmnd *SCtmp;
			SCtmp = HOSTDATA(SCpnt->device->host)->SCint[i];


			if (SCtmp->device->soft_reset) {
				continue;
			}
			kfree(SCtmp->host_scribble);
			SCtmp->host_scribble = NULL;
			HOSTDATA(SCpnt->device->host)->SCint[i] = NULL;
			HOSTDATA(SCpnt->device->host)->mb[i].status = 0;
		}
	}

	spin_unlock_irq(SCpnt->device->host->host_lock);
	return SUCCESS;

fail:
	spin_unlock_irq(SCpnt->device->host->host_lock);
	return FAILED;
}

static int aha1542_host_reset(Scsi_Cmnd * SCpnt)
{
	int i;

	outb(HRST | SCRST, CONTROL(SCpnt->device->host->io_port));

	ssleep(4);
	spin_lock_irq(SCpnt->device->host->host_lock);

	WAIT(STATUS(SCpnt->device->host->io_port),
	     STATMASK, INIT | IDLE, STST | DIAGF | INVDCMD | DF | CDF);

	setup_mailboxes(SCpnt->device->host->io_port, SCpnt->device->host);

	printk(KERN_WARNING "Sent BUS RESET to scsi host %d\n", SCpnt->device->host->host_no);

	for (i = 0; i < AHA1542_MAILBOXES; i++) {
		if (HOSTDATA(SCpnt->device->host)->SCint[i] != NULL) {
			Scsi_Cmnd *SCtmp;
			SCtmp = HOSTDATA(SCpnt->device->host)->SCint[i];

			if (SCtmp->device->soft_reset) {
				continue;
			}
			kfree(SCtmp->host_scribble);
			SCtmp->host_scribble = NULL;
			HOSTDATA(SCpnt->device->host)->SCint[i] = NULL;
			HOSTDATA(SCpnt->device->host)->mb[i].status = 0;
		}
	}

	spin_unlock_irq(SCpnt->device->host->host_lock);
	return SUCCESS;

fail:
	spin_unlock_irq(SCpnt->device->host->host_lock);
	return FAILED;
}

#if 0
static int aha1542_old_abort(Scsi_Cmnd * SCpnt)
{
#if 0
	unchar ahacmd = CMD_START_SCSI;
	unsigned long flags;
	struct mailbox *mb;
	int mbi, mbo, i;

	printk(KERN_DEBUG "In aha1542_abort: %x %x\n",
	       inb(STATUS(SCpnt->host->io_port)),
	       inb(INTRFLAGS(SCpnt->host->io_port)));

	spin_lock_irqsave(&aha1542_lock, flags);
	mb = HOSTDATA(SCpnt->host)->mb;
	mbi = HOSTDATA(SCpnt->host)->aha1542_last_mbi_used + 1;
	if (mbi >= 2 * AHA1542_MAILBOXES)
		mbi = AHA1542_MAILBOXES;

	do {
		if (mb[mbi].status != 0)
			break;
		mbi++;
		if (mbi >= 2 * AHA1542_MAILBOXES)
			mbi = AHA1542_MAILBOXES;
	} while (mbi != HOSTDATA(SCpnt->host)->aha1542_last_mbi_used);
	spin_unlock_irqrestore(&aha1542_lock, flags);

	if (mb[mbi].status) {
		printk(KERN_ERR "Lost interrupt discovered on irq %d - attempting to recover\n",
		       SCpnt->host->irq);
		aha1542_intr_handle(SCpnt->host, NULL);
		return 0;
	}

	for (i = 0; i < AHA1542_MAILBOXES; i++)
		if (HOSTDATA(SCpnt->host)->SCint[i]) {
			if (HOSTDATA(SCpnt->host)->SCint[i] == SCpnt) {
				printk(KERN_ERR "Timed out command pending for %s\n",
				       SCpnt->request->rq_disk ?
				       SCpnt->request->rq_disk->disk_name : "?"
				       );
				if (HOSTDATA(SCpnt->host)->mb[i].status) {
					printk(KERN_ERR "OGMB still full - restarting\n");
					aha1542_out(SCpnt->host->io_port, &ahacmd, 1);
				};
			} else
				printk(KERN_ERR "Other pending command %s\n",
				       SCpnt->request->rq_disk ?
				       SCpnt->request->rq_disk->disk_name : "?"
				       );
		}
#endif

	DEB(printk("aha1542_abort\n"));
#if 0
	spin_lock_irqsave(&aha1542_lock, flags);
	for (mbo = 0; mbo < AHA1542_MAILBOXES; mbo++) {
		if (SCpnt == HOSTDATA(SCpnt->host)->SCint[mbo]) {
			mb[mbo].status = 2;	
			aha1542_out(SCpnt->host->io_port, &ahacmd, 1);	
			spin_unlock_irqrestore(&aha1542_lock, flags);
			break;
		}
	}
	if (AHA1542_MAILBOXES == mbo)
		spin_unlock_irqrestore(&aha1542_lock, flags);
#endif
	return SCSI_ABORT_SNOOZE;
}


static int aha1542_old_reset(Scsi_Cmnd * SCpnt, unsigned int reset_flags)
{
	unchar ahacmd = CMD_START_SCSI;
	int i;

	if (reset_flags & SCSI_RESET_SUGGEST_BUS_RESET) {
		outb(HRST | SCRST, CONTROL(SCpnt->host->io_port));

		WAIT(STATUS(SCpnt->host->io_port),
		STATMASK, INIT | IDLE, STST | DIAGF | INVDCMD | DF | CDF);

		setup_mailboxes(SCpnt->host->io_port, SCpnt->host);

		printk(KERN_WARNING "Sent BUS RESET to scsi host %d\n", SCpnt->host->host_no);

		for (i = 0; i < AHA1542_MAILBOXES; i++)
			if (HOSTDATA(SCpnt->host)->SCint[i] != NULL) {
				Scsi_Cmnd *SCtmp;
				SCtmp = HOSTDATA(SCpnt->host)->SCint[i];
				SCtmp->result = DID_RESET << 16;
				kfree(SCtmp->host_scribble);
				SCtmp->host_scribble = NULL;
				printk(KERN_WARNING "Sending DID_RESET for target %d\n", SCpnt->target);
				SCtmp->scsi_done(SCpnt);

				HOSTDATA(SCpnt->host)->SCint[i] = NULL;
				HOSTDATA(SCpnt->host)->mb[i].status = 0;
			}
		return (SCSI_RESET_SUCCESS | SCSI_RESET_BUS_RESET);
fail:
		printk(KERN_CRIT "aha1542.c: Unable to perform hard reset.\n");
		printk(KERN_CRIT "Power cycle machine to reset\n");
		return (SCSI_RESET_ERROR | SCSI_RESET_BUS_RESET);


	} else {
		
		
		for (i = 0; i < AHA1542_MAILBOXES; i++)
			if (HOSTDATA(SCpnt->host)->SCint[i] == SCpnt) {
				HOSTDATA(SCpnt->host)->ccb[i].op = 0x81;	
				
				aha1542_out(SCpnt->host->io_port, &ahacmd, 1);

				printk(KERN_WARNING "Sent BUS DEVICE RESET to target %d\n", SCpnt->target);

				for (i = 0; i < AHA1542_MAILBOXES; i++)
					if (HOSTDATA(SCpnt->host)->SCint[i] &&
					    HOSTDATA(SCpnt->host)->SCint[i]->target == SCpnt->target) {
						Scsi_Cmnd *SCtmp;
						SCtmp = HOSTDATA(SCpnt->host)->SCint[i];
						SCtmp->result = DID_RESET << 16;
						kfree(SCtmp->host_scribble);
						SCtmp->host_scribble = NULL;
						printk(KERN_WARNING "Sending DID_RESET for target %d\n", SCpnt->target);
						SCtmp->scsi_done(SCpnt);

						HOSTDATA(SCpnt->host)->SCint[i] = NULL;
						HOSTDATA(SCpnt->host)->mb[i].status = 0;
					}
				return SCSI_RESET_SUCCESS;
			}
	}
	return SCSI_RESET_PUNT;
}
#endif    

static int aha1542_biosparam(struct scsi_device *sdev,
		struct block_device *bdev, sector_t capacity, int *ip)
{
	int translation_algorithm;
	int size = capacity;

	translation_algorithm = HOSTDATA(sdev->host)->bios_translation;

	if ((size >> 11) > 1024 && translation_algorithm == BIOS_TRANSLATION_25563) {
		
		ip[0] = 255;
		ip[1] = 63;
		ip[2] = size / 255 / 63;
	} else {
		ip[0] = 64;
		ip[1] = 32;
		ip[2] = size >> 11;
	}

	return 0;
}
MODULE_LICENSE("GPL");


static struct scsi_host_template driver_template = {
	.proc_name		= "aha1542",
	.name			= "Adaptec 1542",
	.detect			= aha1542_detect,
	.release		= aha1542_release,
	.queuecommand		= aha1542_queuecommand,
	.eh_device_reset_handler= aha1542_dev_reset,
	.eh_bus_reset_handler	= aha1542_bus_reset,
	.eh_host_reset_handler	= aha1542_host_reset,
	.bios_param		= aha1542_biosparam,
	.can_queue		= AHA1542_MAILBOXES, 
	.this_id		= 7,
	.sg_tablesize		= AHA1542_SCATTER,
	.cmd_per_lun		= AHA1542_CMDLUN,
	.unchecked_isa_dma	= 1, 
	.use_clustering		= ENABLE_CLUSTERING,
};
#include "scsi_module.c"
