/* $Id: hysdn_boot.c,v 1.4.6.4 2001/09/23 22:24:54 kai Exp $
 *
 * Linux driver for HYSDN cards
 * specific routines for booting and pof handling
 *
 * Author    Werner Cornelius (werner@titro.de) for Hypercope GmbH
 * Copyright 1999 by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "hysdn_defs.h"
#include "hysdn_pof.h"

#define POF_READ_FILE_HEAD  0
#define POF_READ_TAG_HEAD   1
#define POF_READ_TAG_DATA   2

struct boot_data {
	unsigned short Cryptor;	
	unsigned short Nrecs;	
	unsigned char pof_state;
	unsigned char is_crypted;
	int BufSize;		
	int last_error;		
	unsigned short pof_recid;
	unsigned long pof_reclen;
	unsigned long pof_recoffset;
	union {
		unsigned char BootBuf[BOOT_BUF_SIZE];
		tPofRecHdr PofRecHdr;	
		tPofFileHdr PofFileHdr;		
		tPofTimeStamp PofTime;	
	} buf;
};

static void
StartDecryption(struct boot_data *boot)
{
	boot->Cryptor = CRYPT_STARTTERM;
}				


static void
DecryptBuf(struct boot_data *boot, int cnt)
{
	unsigned char *bufp = boot->buf.BootBuf;

	while (cnt--) {
		boot->Cryptor = (boot->Cryptor >> 1) ^ ((boot->Cryptor & 1U) ? CRYPT_FEEDTERM : 0);
		*bufp++ ^= (unsigned char)boot->Cryptor;
	}
}				

static int
pof_handle_data(hysdn_card *card, int datlen)
{
	struct boot_data *boot = card->boot;	
	long l;
	unsigned char *imgp;
	int img_len;

	
	switch (boot->pof_recid) {

	case TAG_TIMESTMP:
		if (card->debug_flags & LOG_POF_RECORD)
			hysdn_addlog(card, "POF created %s", boot->buf.PofTime.DateTimeText);
		break;

	case TAG_CBOOTDTA:
		DecryptBuf(boot, datlen);	
	case TAG_BOOTDTA:
		if (card->debug_flags & LOG_POF_RECORD)
			hysdn_addlog(card, "POF got %s len=%d offs=0x%lx",
				     (boot->pof_recid == TAG_CBOOTDTA) ? "CBOOTDATA" : "BOOTDTA",
				     datlen, boot->pof_recoffset);

		if (boot->pof_reclen != POF_BOOT_LOADER_TOTAL_SIZE) {
			boot->last_error = EPOF_BAD_IMG_SIZE;	
			return (boot->last_error);
		}
		imgp = boot->buf.BootBuf;	
		img_len = datlen;	

		l = POF_BOOT_LOADER_OFF_IN_PAGE -
			(boot->pof_recoffset & (POF_BOOT_LOADER_PAGE_SIZE - 1));
		if (l > 0) {
			
			imgp += l;	
			img_len -= l;	
		}
		
		
		
		
		
		

		if (img_len > 0) {
			
			if ((boot->last_error =
			     card->writebootimg(card, imgp,
						(boot->pof_recoffset > POF_BOOT_LOADER_PAGE_SIZE) ? 2 : 0)) < 0)
				return (boot->last_error);
		}
		break;	

	case TAG_CABSDATA:
		DecryptBuf(boot, datlen);	
	case TAG_ABSDATA:
		if (card->debug_flags & LOG_POF_RECORD)
			hysdn_addlog(card, "POF got %s len=%d offs=0x%lx",
				     (boot->pof_recid == TAG_CABSDATA) ? "CABSDATA" : "ABSDATA",
				     datlen, boot->pof_recoffset);

		if ((boot->last_error = card->writebootseq(card, boot->buf.BootBuf, datlen)) < 0)
			return (boot->last_error);	

		if (boot->pof_recoffset + datlen >= boot->pof_reclen)
			return (card->waitpofready(card));	

		break;	

	default:
		if (card->debug_flags & LOG_POF_RECORD)
			hysdn_addlog(card, "POF got data(id=0x%lx) len=%d offs=0x%lx", boot->pof_recid,
				     datlen, boot->pof_recoffset);

		break;	
	}			

	return (0);
}				


int
pof_write_buffer(hysdn_card *card, int datlen)
{
	struct boot_data *boot = card->boot;	

	if (!boot)
		return (-EFAULT);	
	if (boot->last_error < 0)
		return (boot->last_error);	

	if (card->debug_flags & LOG_POF_WRITE)
		hysdn_addlog(card, "POF write: got %d bytes ", datlen);

	switch (boot->pof_state) {
	case POF_READ_FILE_HEAD:
		if (card->debug_flags & LOG_POF_WRITE)
			hysdn_addlog(card, "POF write: checking file header");

		if (datlen != sizeof(tPofFileHdr)) {
			boot->last_error = -EPOF_INTERNAL;
			break;
		}
		if (boot->buf.PofFileHdr.Magic != TAGFILEMAGIC) {
			boot->last_error = -EPOF_BAD_MAGIC;
			break;
		}
		
		boot->Nrecs = (unsigned short)(boot->buf.PofFileHdr.N_PofRecs);	
		boot->pof_state = POF_READ_TAG_HEAD;	
		boot->last_error = sizeof(tPofRecHdr);	
		break;

	case POF_READ_TAG_HEAD:
		if (card->debug_flags & LOG_POF_WRITE)
			hysdn_addlog(card, "POF write: checking tag header");

		if (datlen != sizeof(tPofRecHdr)) {
			boot->last_error = -EPOF_INTERNAL;
			break;
		}
		boot->pof_recid = boot->buf.PofRecHdr.PofRecId;		
		boot->pof_reclen = boot->buf.PofRecHdr.PofRecDataLen;	
		boot->pof_recoffset = 0;	

		if (card->debug_flags & LOG_POF_RECORD)
			hysdn_addlog(card, "POF: got record id=0x%lx length=%ld ",
				     boot->pof_recid, boot->pof_reclen);

		boot->pof_state = POF_READ_TAG_DATA;	
		if (boot->pof_reclen < BOOT_BUF_SIZE)
			boot->last_error = boot->pof_reclen;	
		else
			boot->last_error = BOOT_BUF_SIZE;	

		if (!boot->last_error) {	
			boot->pof_state = POF_READ_TAG_HEAD;	
			boot->last_error = sizeof(tPofRecHdr);	
		}
		break;

	case POF_READ_TAG_DATA:
		if (card->debug_flags & LOG_POF_WRITE)
			hysdn_addlog(card, "POF write: getting tag data");

		if (datlen != boot->last_error) {
			boot->last_error = -EPOF_INTERNAL;
			break;
		}
		if ((boot->last_error = pof_handle_data(card, datlen)) < 0)
			return (boot->last_error);	
		boot->pof_recoffset += datlen;
		if (boot->pof_recoffset >= boot->pof_reclen) {
			boot->pof_state = POF_READ_TAG_HEAD;	
			boot->last_error = sizeof(tPofRecHdr);	
		} else {
			if (boot->pof_reclen - boot->pof_recoffset < BOOT_BUF_SIZE)
				boot->last_error = boot->pof_reclen - boot->pof_recoffset;	
			else
				boot->last_error = BOOT_BUF_SIZE;	
		}
		break;

	default:
		boot->last_error = -EPOF_INTERNAL;	
		break;
	}			

	return (boot->last_error);
}				


int
pof_write_open(hysdn_card *card, unsigned char **bufp)
{
	struct boot_data *boot;	

	if (card->boot) {
		if (card->debug_flags & LOG_POF_OPEN)
			hysdn_addlog(card, "POF open: already opened for boot");
		return (-ERR_ALREADY_BOOT);	
	}
	
	if (!(boot = kzalloc(sizeof(struct boot_data), GFP_KERNEL))) {
		if (card->debug_flags & LOG_MEM_ERR)
			hysdn_addlog(card, "POF open: unable to allocate mem");
		return (-EFAULT);
	}
	card->boot = boot;
	card->state = CARD_STATE_BOOTING;

	card->stopcard(card);	
	if (card->testram(card)) {
		if (card->debug_flags & LOG_POF_OPEN)
			hysdn_addlog(card, "POF open: DPRAM test failure");
		boot->last_error = -ERR_BOARD_DPRAM;
		card->state = CARD_STATE_BOOTERR;	
		return (boot->last_error);
	}
	boot->BufSize = 0;	
	boot->pof_state = POF_READ_FILE_HEAD;	
	StartDecryption(boot);	

	if (card->debug_flags & LOG_POF_OPEN)
		hysdn_addlog(card, "POF open: success");

	*bufp = boot->buf.BootBuf;	
	return (sizeof(tPofFileHdr));
}				

int
pof_write_close(hysdn_card *card)
{
	struct boot_data *boot = card->boot;	

	if (!boot)
		return (-EFAULT);	

	card->boot = NULL;	
	kfree(boot);

	if (card->state == CARD_STATE_RUN)
		card->set_errlog_state(card, 1);	

	if (card->debug_flags & LOG_POF_OPEN)
		hysdn_addlog(card, "POF close: success");

	return (0);
}				

int
EvalSysrTokData(hysdn_card *card, unsigned char *cp, int len)
{
	u_char *p;
	u_char crc;

	if (card->debug_flags & LOG_POF_RECORD)
		hysdn_addlog(card, "SysReady Token data length %d", len);

	if (len < 2) {
		hysdn_addlog(card, "SysReady Token Data to short");
		return (1);
	}
	for (p = cp, crc = 0; p < (cp + len - 2); p++)
		if ((crc & 0x80))
			crc = (((u_char) (crc << 1)) + 1) + *p;
		else
			crc = ((u_char) (crc << 1)) + *p;
	crc = ~crc;
	if (crc != *(cp + len - 1)) {
		hysdn_addlog(card, "SysReady Token Data invalid CRC");
		return (1);
	}
	len--;			
	while (len > 0) {

		if (*cp == SYSR_TOK_END)
			return (0);	

		if (len < (*(cp + 1) + 2)) {
			hysdn_addlog(card, "token 0x%x invalid length %d", *cp, *(cp + 1));
			return (1);
		}
		switch (*cp) {
		case SYSR_TOK_B_CHAN:	
			if (*(cp + 1) != 1)
				return (1);	
			card->bchans = *(cp + 2);
			break;

		case SYSR_TOK_FAX_CHAN:	
			if (*(cp + 1) != 1)
				return (1);	
			card->faxchans = *(cp + 2);
			break;

		case SYSR_TOK_MAC_ADDR:	
			if (*(cp + 1) != 6)
				return (1);	
			memcpy(card->mac_addr, cp + 2, 6);
			break;

		default:
			hysdn_addlog(card, "unknown token 0x%02x length %d", *cp, *(cp + 1));
			break;
		}
		len -= (*(cp + 1) + 2);		
		cp += (*(cp + 1) + 2);	
	}

	hysdn_addlog(card, "no end token found");
	return (1);
}				
