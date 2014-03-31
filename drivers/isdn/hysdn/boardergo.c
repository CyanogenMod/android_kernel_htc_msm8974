/* $Id: boardergo.c,v 1.5.6.7 2001/11/06 21:58:19 kai Exp $
 *
 * Linux driver for HYSDN cards, specific routines for ergo type boards.
 *
 * Author    Werner Cornelius (werner@titro.de) for Hypercope GmbH
 * Copyright 1999 by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * As all Linux supported cards Champ2, Ergo and Metro2/4 use the same
 * DPRAM interface and layout with only minor differences all related
 * stuff is done here, not in separate modules.
 *
 */

#include <linux/signal.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "hysdn_defs.h"
#include "boardergo.h"

#define byteout(addr, val) outb(val, addr)
#define bytein(addr) inb(addr)

static irqreturn_t
ergo_interrupt(int intno, void *dev_id)
{
	hysdn_card *card = dev_id;	
	tErgDpram *dpr;
	unsigned long flags;
	unsigned char volatile b;

	if (!card)
		return IRQ_NONE;		
	if (!card->irq_enabled)
		return IRQ_NONE;		

	spin_lock_irqsave(&card->hysdn_lock, flags); 

	if (!(bytein(card->iobase + PCI9050_INTR_REG) & PCI9050_INTR_REG_STAT1)) {
		spin_unlock_irqrestore(&card->hysdn_lock, flags);	
		return IRQ_NONE;		
	}
	
	dpr = card->dpram;
	b = dpr->ToPcInt;	
	b |= dpr->ToPcIntMetro;	
	b |= dpr->ToHyInt;	

	
	if (!card->hw_lock)
		schedule_work(&card->irq_queue);
	spin_unlock_irqrestore(&card->hysdn_lock, flags);
	return IRQ_HANDLED;
}				

static void
ergo_irq_bh(struct work_struct *ugli_api)
{
	hysdn_card *card = container_of(ugli_api, hysdn_card, irq_queue);
	tErgDpram *dpr;
	int again;
	unsigned long flags;

	if (card->state != CARD_STATE_RUN)
		return;		

	dpr = card->dpram;	

	spin_lock_irqsave(&card->hysdn_lock, flags);
	if (card->hw_lock) {
		spin_unlock_irqrestore(&card->hysdn_lock, flags);	
		return;
	}
	card->hw_lock = 1;	

	do {
		again = 0;	

		if (!dpr->ToHyFlag) {
			

			if (hysdn_sched_tx(card, dpr->ToHyBuf, &dpr->ToHySize, &dpr->ToHyChannel,
					   ERG_TO_HY_BUF_SIZE)) {
				dpr->ToHyFlag = 1;	
				again = 1;	
			}
		}		
		if (dpr->ToPcFlag) {
			

			if (hysdn_sched_rx(card, dpr->ToPcBuf, dpr->ToPcSize, dpr->ToPcChannel)) {
				dpr->ToPcFlag = 0;	
				again = 1;	
			}
		}		
		if (again) {
			dpr->ToHyInt = 1;
			dpr->ToPcInt = 1;	
		} else
			card->hw_lock = 0;	
	} while (again);	

	spin_unlock_irqrestore(&card->hysdn_lock, flags);
}				


static void
ergo_stopcard(hysdn_card *card)
{
	unsigned long flags;
	unsigned char val;

	hysdn_net_release(card);	
#ifdef CONFIG_HYSDN_CAPI
	hycapi_capi_stop(card);
#endif 
	spin_lock_irqsave(&card->hysdn_lock, flags);
	val = bytein(card->iobase + PCI9050_INTR_REG);	
	val &= ~(PCI9050_INTR_REG_ENPCI | PCI9050_INTR_REG_EN1);	
	byteout(card->iobase + PCI9050_INTR_REG, val);
	card->irq_enabled = 0;
	byteout(card->iobase + PCI9050_USER_IO, PCI9050_E1_RESET);	
	card->state = CARD_STATE_UNUSED;
	card->err_log_state = ERRLOG_STATE_OFF;		

	spin_unlock_irqrestore(&card->hysdn_lock, flags);
}				

static void
ergo_set_errlog_state(hysdn_card *card, int on)
{
	unsigned long flags;

	if (card->state != CARD_STATE_RUN) {
		card->err_log_state = ERRLOG_STATE_OFF;		
		return;
	}
	spin_lock_irqsave(&card->hysdn_lock, flags);

	if (((card->err_log_state == ERRLOG_STATE_OFF) && !on) ||
	    ((card->err_log_state == ERRLOG_STATE_ON) && on)) {
		spin_unlock_irqrestore(&card->hysdn_lock, flags);
		return;		
	}
	if (on)
		card->err_log_state = ERRLOG_STATE_START;	
	else
		card->err_log_state = ERRLOG_STATE_STOP;	

	spin_unlock_irqrestore(&card->hysdn_lock, flags);
	schedule_work(&card->irq_queue);
}				

static const char TestText[36] = "This Message is filler, why read it";

static int
ergo_testram(hysdn_card *card)
{
	tErgDpram *dpr = card->dpram;

	memset(dpr->TrapTable, 0, sizeof(dpr->TrapTable));	
	dpr->ToHyInt = 1;	

	memcpy(&dpr->ToHyBuf[ERG_TO_HY_BUF_SIZE - sizeof(TestText)], TestText,
	       sizeof(TestText));
	if (memcmp(&dpr->ToHyBuf[ERG_TO_HY_BUF_SIZE - sizeof(TestText)], TestText,
		   sizeof(TestText)))
		return (-1);

	memcpy(&dpr->ToPcBuf[ERG_TO_PC_BUF_SIZE - sizeof(TestText)], TestText,
	       sizeof(TestText));
	if (memcmp(&dpr->ToPcBuf[ERG_TO_PC_BUF_SIZE - sizeof(TestText)], TestText,
		   sizeof(TestText)))
		return (-1);

	return (0);
}				

/* this is done in two steps. First the 1024 hi-words are written (offs=0),  */
/* then the 1024 lo-bytes are written. The remaining DPRAM is cleared, the   */
static int
ergo_writebootimg(struct HYSDN_CARD *card, unsigned char *buf,
		  unsigned long offs)
{
	unsigned char *dst;
	tErgDpram *dpram;
	int cnt = (BOOT_IMG_SIZE >> 2);		

	if (card->debug_flags & LOG_POF_CARD)
		hysdn_addlog(card, "ERGO: write bootldr offs=0x%lx ", offs);

	dst = card->dpram;	
	dst += (offs + ERG_DPRAM_FILL_SIZE);	
	while (cnt--) {
		*dst++ = *(buf + 1);	
		*dst++ = *buf;	
		dst += 2;	
		buf += 2;	
	}

	/* if low words (offs = 2) have been written, clear the rest of the DPRAM, */
	
	if (offs) {
		memset(card->dpram, 0, ERG_DPRAM_FILL_SIZE);	
		dpram = card->dpram;	
		dpram->ToHyNoDpramErrLog = 0xFF;	
		while (!dpram->ToHyNoDpramErrLog);	

		byteout(card->iobase + PCI9050_USER_IO, PCI9050_E1_RUN);	
		

		msleep_interruptible(20);		

		if (((tDpramBootSpooler *) card->dpram)->Len != DPRAM_SPOOLER_DATA_SIZE) {
			if (card->debug_flags & LOG_POF_CARD)
				hysdn_addlog(card, "ERGO: write bootldr no answer");
			return (-ERR_BOOTIMG_FAIL);
		}
	}			
	return (0);		
}				

static int
ergo_writebootseq(struct HYSDN_CARD *card, unsigned char *buf, int len)
{
	tDpramBootSpooler *sp = (tDpramBootSpooler *) card->dpram;
	unsigned char *dst;
	unsigned char buflen;
	int nr_write;
	unsigned char tmp_rdptr;
	unsigned char wr_mirror;
	int i;

	if (card->debug_flags & LOG_POF_CARD)
		hysdn_addlog(card, "ERGO: write boot seq len=%d ", len);

	dst = sp->Data;		
	buflen = sp->Len;	
	wr_mirror = sp->WrPtr;	

	/* try until all bytes written or error */
	i = 0x1000;		
	while (len) {

		
		do {
			tmp_rdptr = sp->RdPtr;	
			i--;	
		} while (i && (tmp_rdptr != sp->RdPtr));	

		if (!i) {
			if (card->debug_flags & LOG_POF_CARD)
				hysdn_addlog(card, "ERGO: write boot seq timeout");
			return (-ERR_BOOTSEQ_FAIL);	
		}
		if ((nr_write = tmp_rdptr - wr_mirror - 1) < 0)
			nr_write += buflen;	

		if (!nr_write)
			continue;	

		if (nr_write > len)
			nr_write = len;		
		i = 0x1000;	

		
		len -= nr_write;	
		while (nr_write--) {
			*(dst + wr_mirror) = *buf++;	
			if (++wr_mirror >= buflen)
				wr_mirror = 0;
			sp->WrPtr = wr_mirror;	
		}		

	}			
	return (0);
}				

static int
ergo_waitpofready(struct HYSDN_CARD *card)
{
	tErgDpram *dpr = card->dpram;	
	int timecnt = 10000 / 50;	
	unsigned long flags;
	int msg_size;
	int i;

	if (card->debug_flags & LOG_POF_CARD)
		hysdn_addlog(card, "ERGO: waiting for pof ready");
	while (timecnt--) {
		

		if (dpr->ToPcFlag) {
			

			if ((dpr->ToPcChannel != CHAN_SYSTEM) ||
			    (dpr->ToPcSize < MIN_RDY_MSG_SIZE) ||
			    (dpr->ToPcSize > MAX_RDY_MSG_SIZE) ||
			    ((*(unsigned long *) dpr->ToPcBuf) != RDY_MAGIC))
				break;	

			
			msg_size = dpr->ToPcSize - RDY_MAGIC_SIZE;
			if (msg_size > 0)
				if (EvalSysrTokData(card, dpr->ToPcBuf + RDY_MAGIC_SIZE, msg_size))
					break;

			if (card->debug_flags & LOG_POF_RECORD)
				hysdn_addlog(card, "ERGO: pof boot success");
			spin_lock_irqsave(&card->hysdn_lock, flags);

			card->state = CARD_STATE_RUN;	
			
			byteout(card->iobase + PCI9050_INTR_REG,
				bytein(card->iobase + PCI9050_INTR_REG) |
				(PCI9050_INTR_REG_ENPCI | PCI9050_INTR_REG_EN1));
			card->irq_enabled = 1;	

			dpr->ToPcFlag = 0;	
			dpr->ToHyInt = 1;
			dpr->ToPcInt = 1;	

			spin_unlock_irqrestore(&card->hysdn_lock, flags);
			if ((hynet_enable & (1 << card->myid))
			    && (i = hysdn_net_create(card)))
			{
				ergo_stopcard(card);
				card->state = CARD_STATE_BOOTERR;
				return (i);
			}
#ifdef CONFIG_HYSDN_CAPI
			if ((i = hycapi_capi_create(card))) {
				printk(KERN_WARNING "HYSDN: failed to create capi-interface.\n");
			}
#endif 
			return (0);	
		}		
		msleep_interruptible(50);		
	}			

	if (card->debug_flags & LOG_POF_CARD)
		hysdn_addlog(card, "ERGO: pof boot ready timeout");
	return (-ERR_POF_TIMEOUT);
}				



static void
ergo_releasehardware(hysdn_card *card)
{
	ergo_stopcard(card);	
	free_irq(card->irq, card);	
	release_region(card->iobase + PCI9050_INTR_REG, 1);	
	release_region(card->iobase + PCI9050_USER_IO, 1);
	iounmap(card->dpram);
	card->dpram = NULL;	
}				


int
ergo_inithardware(hysdn_card *card)
{
	if (!request_region(card->iobase + PCI9050_INTR_REG, 1, "HYSDN"))
		return (-1);
	if (!request_region(card->iobase + PCI9050_USER_IO, 1, "HYSDN")) {
		release_region(card->iobase + PCI9050_INTR_REG, 1);
		return (-1);	
	}
	card->memend = card->membase + ERG_DPRAM_PAGE_SIZE - 1;
	if (!(card->dpram = ioremap(card->membase, ERG_DPRAM_PAGE_SIZE))) {
		release_region(card->iobase + PCI9050_INTR_REG, 1);
		release_region(card->iobase + PCI9050_USER_IO, 1);
		return (-1);
	}

	ergo_stopcard(card);	
	if (request_irq(card->irq, ergo_interrupt, IRQF_SHARED, "HYSDN", card)) {
		ergo_releasehardware(card); 
		return (-1);
	}
	
	card->stopcard = ergo_stopcard;
	card->releasehardware = ergo_releasehardware;
	card->testram = ergo_testram;
	card->writebootimg = ergo_writebootimg;
	card->writebootseq = ergo_writebootseq;
	card->waitpofready = ergo_waitpofready;
	card->set_errlog_state = ergo_set_errlog_state;
	INIT_WORK(&card->irq_queue, ergo_irq_bh);
	spin_lock_init(&card->hysdn_lock);

	return (0);
}				
