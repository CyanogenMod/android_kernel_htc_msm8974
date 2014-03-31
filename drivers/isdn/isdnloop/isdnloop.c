/* $Id: isdnloop.c,v 1.11.6.7 2001/11/11 19:54:31 kai Exp $
 *
 * ISDN low-level module implementing a dummy loop driver.
 *
 * Copyright 1997 by Fritz Elfert (fritz@isdn4linux.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include "isdnloop.h"

static char *revision = "$Revision: 1.11.6.7 $";
static char *isdnloop_id = "loop0";

MODULE_DESCRIPTION("ISDN4Linux: Pseudo Driver that simulates an ISDN card");
MODULE_AUTHOR("Fritz Elfert");
MODULE_LICENSE("GPL");
module_param(isdnloop_id, charp, 0);
MODULE_PARM_DESC(isdnloop_id, "ID-String of first card");

static int isdnloop_addcard(char *);

static void
isdnloop_free_queue(isdnloop_card *card, int channel)
{
	struct sk_buff_head *queue = &card->bqueue[channel];

	skb_queue_purge(queue);
	card->sndcount[channel] = 0;
}

static void
isdnloop_bchan_send(isdnloop_card *card, int ch)
{
	isdnloop_card *rcard = card->rcard[ch];
	int rch = card->rch[ch], len, ack;
	struct sk_buff *skb;
	isdn_ctrl cmd;

	while (card->sndcount[ch]) {
		if ((skb = skb_dequeue(&card->bqueue[ch]))) {
			len = skb->len;
			card->sndcount[ch] -= len;
			ack = *(skb->head); 
			cmd.driver = card->myid;
			cmd.arg = ch;
			if (rcard) {
				rcard->interface.rcvcallb_skb(rcard->myid, rch, skb);
			} else {
				printk(KERN_WARNING "isdnloop: no rcard, skb dropped\n");
				dev_kfree_skb(skb);

			};
			cmd.command = ISDN_STAT_BSENT;
			cmd.parm.length = len;
			card->interface.statcallb(&cmd);
		} else
			card->sndcount[ch] = 0;
	}
}

static void
isdnloop_pollbchan(unsigned long data)
{
	isdnloop_card *card = (isdnloop_card *) data;
	unsigned long flags;

	if (card->flags & ISDNLOOP_FLAGS_B1ACTIVE)
		isdnloop_bchan_send(card, 0);
	if (card->flags & ISDNLOOP_FLAGS_B2ACTIVE)
		isdnloop_bchan_send(card, 1);
	if (card->flags & (ISDNLOOP_FLAGS_B1ACTIVE | ISDNLOOP_FLAGS_B2ACTIVE)) {
		
		spin_lock_irqsave(&card->isdnloop_lock, flags);
		card->rb_timer.expires = jiffies + ISDNLOOP_TIMER_BCREAD;
		add_timer(&card->rb_timer);
		card->flags |= ISDNLOOP_FLAGS_RBTIMER;
		spin_unlock_irqrestore(&card->isdnloop_lock, flags);
	} else
		card->flags &= ~ISDNLOOP_FLAGS_RBTIMER;
}

static void
isdnloop_parse_setup(char *setup, isdn_ctrl *cmd)
{
	char *t = setup;
	char *s = strchr(t, ',');

	*s++ = '\0';
	strlcpy(cmd->parm.setup.phone, t, sizeof(cmd->parm.setup.phone));
	s = strchr(t = s, ',');
	*s++ = '\0';
	if (!strlen(t))
		cmd->parm.setup.si1 = 0;
	else
		cmd->parm.setup.si1 = simple_strtoul(t, NULL, 10);
	s = strchr(t = s, ',');
	*s++ = '\0';
	if (!strlen(t))
		cmd->parm.setup.si2 = 0;
	else
		cmd->parm.setup.si2 =
			simple_strtoul(t, NULL, 10);
	strlcpy(cmd->parm.setup.eazmsn, s, sizeof(cmd->parm.setup.eazmsn));
	cmd->parm.setup.plan = 0;
	cmd->parm.setup.screen = 0;
}

typedef struct isdnloop_stat {
	char *statstr;
	int command;
	int action;
} isdnloop_stat;
static isdnloop_stat isdnloop_stat_table[] =
{
	{"BCON_",          ISDN_STAT_BCONN, 1}, 
	{"BDIS_",          ISDN_STAT_BHUP,  2}, 
	{"DCON_",          ISDN_STAT_DCONN, 0}, 
	{"DDIS_",          ISDN_STAT_DHUP,  0}, 
	{"DCAL_I",         ISDN_STAT_ICALL, 3}, 
	{"DSCA_I",         ISDN_STAT_ICALL, 3}, 
	{"FCALL",          ISDN_STAT_ICALL, 4}, 
	{"CIF",            ISDN_STAT_CINF,  5}, 
	{"AOC",            ISDN_STAT_CINF,  6}, 
	{"CAU",            ISDN_STAT_CAUSE, 7}, 
	{"TEI OK",         ISDN_STAT_RUN,   0}, 
	{"E_L1: ACT FAIL", ISDN_STAT_BHUP,  8}, 
	{"E_L2: DATA LIN", ISDN_STAT_BHUP,  8}, 
	{"E_L1: ACTIVATION FAILED",
	 ISDN_STAT_BHUP,  8},         
	{NULL, 0, -1}
};


static void
isdnloop_parse_status(u_char *status, int channel, isdnloop_card *card)
{
	isdnloop_stat *s = isdnloop_stat_table;
	int action = -1;
	isdn_ctrl cmd;

	while (s->statstr) {
		if (!strncmp(status, s->statstr, strlen(s->statstr))) {
			cmd.command = s->command;
			action = s->action;
			break;
		}
		s++;
	}
	if (action == -1)
		return;
	cmd.driver = card->myid;
	cmd.arg = channel;
	switch (action) {
	case 1:
		
		card->flags |= (channel) ?
			ISDNLOOP_FLAGS_B2ACTIVE : ISDNLOOP_FLAGS_B1ACTIVE;
		break;
	case 2:
		
		card->flags &= ~((channel) ?
				 ISDNLOOP_FLAGS_B2ACTIVE : ISDNLOOP_FLAGS_B1ACTIVE);
		isdnloop_free_queue(card, channel);
		break;
	case 3:
		
		isdnloop_parse_setup(status + 6, &cmd);
		break;
	case 4:
		
		sprintf(cmd.parm.setup.phone, "LEASED%d", card->myid);
		sprintf(cmd.parm.setup.eazmsn, "%d", channel + 1);
		cmd.parm.setup.si1 = 7;
		cmd.parm.setup.si2 = 0;
		cmd.parm.setup.plan = 0;
		cmd.parm.setup.screen = 0;
		break;
	case 5:
		
		strlcpy(cmd.parm.num, status + 3, sizeof(cmd.parm.num));
		break;
	case 6:
		
		snprintf(cmd.parm.num, sizeof(cmd.parm.num), "%d",
			 (int) simple_strtoul(status + 7, NULL, 16));
		break;
	case 7:
		
		status += 3;
		if (strlen(status) == 4)
			snprintf(cmd.parm.num, sizeof(cmd.parm.num), "%s%c%c",
				 status + 2, *status, *(status + 1));
		else
			strlcpy(cmd.parm.num, status + 1, sizeof(cmd.parm.num));
		break;
	case 8:
		
		card->flags &= ~ISDNLOOP_FLAGS_B1ACTIVE;
		isdnloop_free_queue(card, 0);
		cmd.arg = 0;
		cmd.driver = card->myid;
		card->interface.statcallb(&cmd);
		cmd.command = ISDN_STAT_DHUP;
		cmd.arg = 0;
		cmd.driver = card->myid;
		card->interface.statcallb(&cmd);
		cmd.command = ISDN_STAT_BHUP;
		card->flags &= ~ISDNLOOP_FLAGS_B2ACTIVE;
		isdnloop_free_queue(card, 1);
		cmd.arg = 1;
		cmd.driver = card->myid;
		card->interface.statcallb(&cmd);
		cmd.command = ISDN_STAT_DHUP;
		cmd.arg = 1;
		cmd.driver = card->myid;
		break;
	}
	card->interface.statcallb(&cmd);
}

static void
isdnloop_putmsg(isdnloop_card *card, unsigned char c)
{
	ulong flags;

	spin_lock_irqsave(&card->isdnloop_lock, flags);
	*card->msg_buf_write++ = (c == 0xff) ? '\n' : c;
	if (card->msg_buf_write == card->msg_buf_read) {
		if (++card->msg_buf_read > card->msg_buf_end)
			card->msg_buf_read = card->msg_buf;
	}
	if (card->msg_buf_write > card->msg_buf_end)
		card->msg_buf_write = card->msg_buf;
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
}

static void
isdnloop_polldchan(unsigned long data)
{
	isdnloop_card *card = (isdnloop_card *) data;
	struct sk_buff *skb;
	int avail;
	int left;
	u_char c;
	int ch;
	unsigned long flags;
	u_char *p;
	isdn_ctrl cmd;

	if ((skb = skb_dequeue(&card->dqueue)))
		avail = skb->len;
	else
		avail = 0;
	for (left = avail; left > 0; left--) {
		c = *skb->data;
		skb_pull(skb, 1);
		isdnloop_putmsg(card, c);
		card->imsg[card->iptr] = c;
		if (card->iptr < 59)
			card->iptr++;
		if (!skb->len) {
			avail++;
			isdnloop_putmsg(card, '\n');
			card->imsg[card->iptr] = 0;
			card->iptr = 0;
			if (card->imsg[0] == '0' && card->imsg[1] >= '0' &&
			    card->imsg[1] <= '2' && card->imsg[2] == ';') {
				ch = (card->imsg[1] - '0') - 1;
				p = &card->imsg[3];
				isdnloop_parse_status(p, ch, card);
			} else {
				p = card->imsg;
				if (!strncmp(p, "DRV1.", 5)) {
					printk(KERN_INFO "isdnloop: (%s) %s\n", CID, p);
					if (!strncmp(p + 7, "TC", 2)) {
						card->ptype = ISDN_PTYPE_1TR6;
						card->interface.features |= ISDN_FEATURE_P_1TR6;
						printk(KERN_INFO
						       "isdnloop: (%s) 1TR6-Protocol loaded and running\n", CID);
					}
					if (!strncmp(p + 7, "EC", 2)) {
						card->ptype = ISDN_PTYPE_EURO;
						card->interface.features |= ISDN_FEATURE_P_EURO;
						printk(KERN_INFO
						       "isdnloop: (%s) Euro-Protocol loaded and running\n", CID);
					}
					continue;

				}
			}
		}
	}
	if (avail) {
		cmd.command = ISDN_STAT_STAVAIL;
		cmd.driver = card->myid;
		cmd.arg = avail;
		card->interface.statcallb(&cmd);
	}
	if (card->flags & (ISDNLOOP_FLAGS_B1ACTIVE | ISDNLOOP_FLAGS_B2ACTIVE))
		if (!(card->flags & ISDNLOOP_FLAGS_RBTIMER)) {
			
			card->flags |= ISDNLOOP_FLAGS_RBTIMER;
			spin_lock_irqsave(&card->isdnloop_lock, flags);
			del_timer(&card->rb_timer);
			card->rb_timer.function = isdnloop_pollbchan;
			card->rb_timer.data = (unsigned long) card;
			card->rb_timer.expires = jiffies + ISDNLOOP_TIMER_BCREAD;
			add_timer(&card->rb_timer);
			spin_unlock_irqrestore(&card->isdnloop_lock, flags);
		}
	
	spin_lock_irqsave(&card->isdnloop_lock, flags);
	card->st_timer.expires = jiffies + ISDNLOOP_TIMER_DCREAD;
	add_timer(&card->st_timer);
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
}

static int
isdnloop_sendbuf(int channel, struct sk_buff *skb, isdnloop_card *card)
{
	int len = skb->len;
	unsigned long flags;
	struct sk_buff *nskb;

	if (len > 4000) {
		printk(KERN_WARNING
		       "isdnloop: Send packet too large\n");
		return -EINVAL;
	}
	if (len) {
		if (!(card->flags & (channel) ? ISDNLOOP_FLAGS_B2ACTIVE : ISDNLOOP_FLAGS_B1ACTIVE))
			return 0;
		if (card->sndcount[channel] > ISDNLOOP_MAX_SQUEUE)
			return 0;
		spin_lock_irqsave(&card->isdnloop_lock, flags);
		nskb = dev_alloc_skb(skb->len);
		if (nskb) {
			skb_copy_from_linear_data(skb,
						  skb_put(nskb, len), len);
			skb_queue_tail(&card->bqueue[channel], nskb);
			dev_kfree_skb(skb);
		} else
			len = 0;
		card->sndcount[channel] += len;
		spin_unlock_irqrestore(&card->isdnloop_lock, flags);
	}
	return len;
}

static int
isdnloop_readstatus(u_char __user *buf, int len, isdnloop_card *card)
{
	int count;
	u_char __user *p;

	for (p = buf, count = 0; count < len; p++, count++) {
		if (card->msg_buf_read == card->msg_buf_write)
			return count;
		if (put_user(*card->msg_buf_read++, p))
			return -EFAULT;
		if (card->msg_buf_read > card->msg_buf_end)
			card->msg_buf_read = card->msg_buf;
	}
	return count;
}

static int
isdnloop_fake(isdnloop_card *card, char *s, int ch)
{
	struct sk_buff *skb;
	int len = strlen(s) + ((ch >= 0) ? 3 : 0);

	if (!(skb = dev_alloc_skb(len))) {
		printk(KERN_WARNING "isdnloop: Out of memory in isdnloop_fake\n");
		return 1;
	}
	if (ch >= 0)
		sprintf(skb_put(skb, 3), "%02d;", ch);
	memcpy(skb_put(skb, strlen(s)), s, strlen(s));
	skb_queue_tail(&card->dqueue, skb);
	return 0;
}
static isdnloop_stat isdnloop_cmd_table[] =
{
	{"BCON_R",         0,  1},	
	{"BCON_I",         0, 17},	
	{"BDIS_R",         0,  2},	
	{"DDIS_R",         0,  3},	
	{"DCON_R",         0, 16},	
	{"DSCA_R",         0,  4},	
	{"DCAL_R",         0,  5},	
	{"EAZC",           0,  6},	
	{"EAZ",            0,  7},	
	{"SEEAZ",          0,  8},	
	{"MSN",            0,  9},	
	{"MSALL",          0, 10},	
	{"SETSIL",         0, 11},	
	{"SEESIL",         0, 12},	
	{"SILC",           0, 13},	
	{"LOCK",           0, -1},	
	{"UNLOCK",         0, -1},	
	{"FV2ON",          1, 14},	
	{"FV2OFF",         1, 15},	
	{NULL, 0, -1}
};


static void
isdnloop_fake_err(isdnloop_card *card)
{
	char buf[60];

	sprintf(buf, "E%s", card->omsg);
	isdnloop_fake(card, buf, -1);
	isdnloop_fake(card, "NAK", -1);
}

static u_char ctable_eu[] =
{0x00, 0x11, 0x01, 0x12};
static u_char ctable_1t[] =
{0x00, 0x3b, 0x01, 0x3a};

static char *
isdnloop_unicause(isdnloop_card *card, int loc, int cau)
{
	static char buf[6];

	switch (card->ptype) {
	case ISDN_PTYPE_EURO:
		sprintf(buf, "E%02X%02X", (loc) ? 4 : 2, ctable_eu[cau]);
		break;
	case ISDN_PTYPE_1TR6:
		sprintf(buf, "%02X44", ctable_1t[cau]);
		break;
	default:
		return ("0000");
	}
	return (buf);
}

static void
isdnloop_atimeout(isdnloop_card *card, int ch)
{
	unsigned long flags;
	char buf[60];

	spin_lock_irqsave(&card->isdnloop_lock, flags);
	if (card->rcard) {
		isdnloop_fake(card->rcard[ch], "DDIS_I", card->rch[ch] + 1);
		card->rcard[ch]->rcard[card->rch[ch]] = NULL;
		card->rcard[ch] = NULL;
	}
	isdnloop_fake(card, "DDIS_I", ch + 1);
	
	sprintf(buf, "CAU%s", isdnloop_unicause(card, 1, 3));
	isdnloop_fake(card, buf, ch + 1);
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
}

static void
isdnloop_atimeout0(unsigned long data)
{
	isdnloop_card *card = (isdnloop_card *) data;
	isdnloop_atimeout(card, 0);
}

static void
isdnloop_atimeout1(unsigned long data)
{
	isdnloop_card *card = (isdnloop_card *) data;
	isdnloop_atimeout(card, 1);
}

static void
isdnloop_start_ctimer(isdnloop_card *card, int ch)
{
	unsigned long flags;

	spin_lock_irqsave(&card->isdnloop_lock, flags);
	init_timer(&card->c_timer[ch]);
	card->c_timer[ch].expires = jiffies + ISDNLOOP_TIMER_ALERTWAIT;
	if (ch)
		card->c_timer[ch].function = isdnloop_atimeout1;
	else
		card->c_timer[ch].function = isdnloop_atimeout0;
	card->c_timer[ch].data = (unsigned long) card;
	add_timer(&card->c_timer[ch]);
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
}

static void
isdnloop_kill_ctimer(isdnloop_card *card, int ch)
{
	unsigned long flags;

	spin_lock_irqsave(&card->isdnloop_lock, flags);
	del_timer(&card->c_timer[ch]);
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
}

static u_char si2bit[] =
{0, 1, 0, 0, 0, 2, 0, 4, 0, 0};
static u_char bit2si[] =
{1, 5, 7};

static int
isdnloop_try_call(isdnloop_card *card, char *p, int lch, isdn_ctrl *cmd)
{
	isdnloop_card *cc = cards;
	unsigned long flags;
	int ch;
	int num_match;
	int i;
	char *e;
	char nbuf[32];

	isdnloop_parse_setup(p, cmd);
	while (cc) {
		for (ch = 0; ch < 2; ch++) {
			
			if ((cc == card) && (ch == lch))
				continue;
			num_match = 0;
			switch (cc->ptype) {
			case ISDN_PTYPE_EURO:
				for (i = 0; i < 3; i++)
					if (!(strcmp(cc->s0num[i], cmd->parm.setup.phone)))
						num_match = 1;
				break;
			case ISDN_PTYPE_1TR6:
				e = cc->eazlist[ch];
				while (*e) {
					sprintf(nbuf, "%s%c", cc->s0num[0], *e);
					if (!(strcmp(nbuf, cmd->parm.setup.phone)))
						num_match = 1;
					e++;
				}
			}
			if (num_match) {
				spin_lock_irqsave(&card->isdnloop_lock, flags);
				
				if (!(cc->rcard[ch])) {
					
					if (!(si2bit[cmd->parm.setup.si1] & cc->sil[ch])) {
						spin_unlock_irqrestore(&card->isdnloop_lock, flags);
						return 3;
					}
					
					cc->rcard[ch] = card;
					cc->rch[ch] = lch;
					card->rcard[lch] = cc;
					card->rch[lch] = ch;
					spin_unlock_irqrestore(&card->isdnloop_lock, flags);
					return 0;
				} else {
					spin_unlock_irqrestore(&card->isdnloop_lock, flags);
					
					if (ch == 1)
						return 1;
				}
			}
		}
		cc = cc->next;
	}
	return 2;
}

static char *
isdnloop_vstphone(isdnloop_card *card, char *phone, int caller)
{
	int i;
	static char nphone[30];

	if (!card) {
		printk("BUG!!!\n");
		return "";
	}
	switch (card->ptype) {
	case ISDN_PTYPE_EURO:
		if (caller) {
			for (i = 0; i < 2; i++)
				if (!(strcmp(card->s0num[i], phone)))
					return (phone);
			return (card->s0num[0]);
		}
		return (phone);
		break;
	case ISDN_PTYPE_1TR6:
		if (caller) {
			sprintf(nphone, "%s%c", card->s0num[0], phone[0]);
			return (nphone);
		} else
			return (&phone[strlen(phone) - 1]);
		break;
	}
	return "";
}

static void
isdnloop_parse_cmd(isdnloop_card *card)
{
	char *p = card->omsg;
	isdn_ctrl cmd;
	char buf[60];
	isdnloop_stat *s = isdnloop_cmd_table;
	int action = -1;
	int i;
	int ch;

	if ((card->omsg[0] != '0') && (card->omsg[2] != ';')) {
		isdnloop_fake_err(card);
		return;
	}
	ch = card->omsg[1] - '0';
	if ((ch < 0) || (ch > 2)) {
		isdnloop_fake_err(card);
		return;
	}
	p += 3;
	while (s->statstr) {
		if (!strncmp(p, s->statstr, strlen(s->statstr))) {
			action = s->action;
			if (s->command && (ch != 0)) {
				isdnloop_fake_err(card);
				return;
			}
			break;
		}
		s++;
	}
	if (action == -1)
		return;
	switch (action) {
	case 1:
		
		if (card->rcard[ch - 1]) {
			isdnloop_fake(card->rcard[ch - 1], "BCON_I",
				      card->rch[ch - 1] + 1);
			isdnloop_fake(card, "BCON_C", ch);
		}
		break;
	case 17:
		
		if (card->rcard[ch - 1]) {
			isdnloop_fake(card->rcard[ch - 1], "BCON_C",
				      card->rch[ch - 1] + 1);
		}
		break;
	case 2:
		
		isdnloop_fake(card, "BDIS_C", ch);
		if (card->rcard[ch - 1]) {
			isdnloop_fake(card->rcard[ch - 1], "BDIS_I",
				      card->rch[ch - 1] + 1);
		}
		break;
	case 16:
		
		isdnloop_kill_ctimer(card, ch - 1);
		if (card->rcard[ch - 1]) {
			isdnloop_kill_ctimer(card->rcard[ch - 1], card->rch[ch - 1]);
			isdnloop_fake(card->rcard[ch - 1], "DCON_C",
				      card->rch[ch - 1] + 1);
			isdnloop_fake(card, "DCON_C", ch);
		}
		break;
	case 3:
		
		isdnloop_kill_ctimer(card, ch - 1);
		if (card->rcard[ch - 1]) {
			isdnloop_kill_ctimer(card->rcard[ch - 1], card->rch[ch - 1]);
			isdnloop_fake(card->rcard[ch - 1], "DDIS_I",
				      card->rch[ch - 1] + 1);
			card->rcard[ch - 1] = NULL;
		}
		isdnloop_fake(card, "DDIS_C", ch);
		break;
	case 4:
		
		if (card->ptype != ISDN_PTYPE_1TR6) {
			isdnloop_fake_err(card);
			return;
		}
		
	case 5:
		
		p += 6;
		switch (isdnloop_try_call(card, p, ch - 1, &cmd)) {
		case 0:
			
			sprintf(buf, "D%s_I%s,%02d,%02d,%s",
				(action == 4) ? "SCA" : "CAL",
				isdnloop_vstphone(card, cmd.parm.setup.eazmsn, 1),
				cmd.parm.setup.si1,
				cmd.parm.setup.si2,
				isdnloop_vstphone(card->rcard[ch - 1],
						  cmd.parm.setup.phone, 0));
			isdnloop_fake(card->rcard[ch - 1], buf, card->rch[ch - 1] + 1);
			
		case 3:
			
			isdnloop_start_ctimer(card, ch - 1);
			break;
		case 1:
			
			isdnloop_fake(card, "DDIS_I", ch);
			sprintf(buf, "CAU%s", isdnloop_unicause(card, 1, 1));
			isdnloop_fake(card, buf, ch);
			break;
		case 2:
			
			isdnloop_fake(card, "DDIS_I", ch);
			sprintf(buf, "CAU%s", isdnloop_unicause(card, 1, 2));
			isdnloop_fake(card, buf, ch);
			break;
		}
		break;
	case 6:
		
		card->eazlist[ch - 1][0] = '\0';
		break;
	case 7:
		
		p += 3;
		strcpy(card->eazlist[ch - 1], p);
		break;
	case 8:
		
		sprintf(buf, "EAZ-LIST: %s", card->eazlist[ch - 1]);
		isdnloop_fake(card, buf, ch + 1);
		break;
	case 9:
		
		break;
	case 10:
		
		break;
	case 11:
		
		p += 6;
		i = 0;
		while (strchr("0157", *p)) {
			if (i)
				card->sil[ch - 1] |= si2bit[*p - '0'];
			i = (*p++ == '0');
		}
		if (*p)
			isdnloop_fake_err(card);
		break;
	case 12:
		
		sprintf(buf, "SIN-LIST: ");
		p = buf + 10;
		for (i = 0; i < 3; i++)
			if (card->sil[ch - 1] & (1 << i))
				p += sprintf(p, "%02d", bit2si[i]);
		isdnloop_fake(card, buf, ch + 1);
		break;
	case 13:
		
		card->sil[ch - 1] = 0;
		break;
	case 14:
		
		break;
	case 15:
		
		break;
	}
}

static int
isdnloop_writecmd(const u_char *buf, int len, int user, isdnloop_card *card)
{
	int xcount = 0;
	int ocount = 1;
	isdn_ctrl cmd;

	while (len) {
		int count = len;
		u_char *p;
		u_char msg[0x100];

		if (count > 255)
			count = 255;
		if (user) {
			if (copy_from_user(msg, buf, count))
				return -EFAULT;
		} else
			memcpy(msg, buf, count);
		isdnloop_putmsg(card, '>');
		for (p = msg; count > 0; count--, p++) {
			len--;
			xcount++;
			isdnloop_putmsg(card, *p);
			card->omsg[card->optr] = *p;
			if (*p == '\n') {
				card->omsg[card->optr] = '\0';
				card->optr = 0;
				isdnloop_parse_cmd(card);
				if (len) {
					isdnloop_putmsg(card, '>');
					ocount++;
				}
			} else {
				if (card->optr < 59)
					card->optr++;
			}
			ocount++;
		}
	}
	cmd.command = ISDN_STAT_STAVAIL;
	cmd.driver = card->myid;
	cmd.arg = ocount;
	card->interface.statcallb(&cmd);
	return xcount;
}

static void
isdnloop_stopcard(isdnloop_card *card)
{
	unsigned long flags;
	isdn_ctrl cmd;

	spin_lock_irqsave(&card->isdnloop_lock, flags);
	if (card->flags & ISDNLOOP_FLAGS_RUNNING) {
		card->flags &= ~ISDNLOOP_FLAGS_RUNNING;
		del_timer(&card->st_timer);
		del_timer(&card->rb_timer);
		del_timer(&card->c_timer[0]);
		del_timer(&card->c_timer[1]);
		cmd.command = ISDN_STAT_STOP;
		cmd.driver = card->myid;
		card->interface.statcallb(&cmd);
	}
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
}

static void
isdnloop_stopallcards(void)
{
	isdnloop_card *p = cards;

	while (p) {
		isdnloop_stopcard(p);
		p = p->next;
	}
}

static int
isdnloop_start(isdnloop_card *card, isdnloop_sdef *sdefp)
{
	unsigned long flags;
	isdnloop_sdef sdef;
	int i;

	if (card->flags & ISDNLOOP_FLAGS_RUNNING)
		return -EBUSY;
	if (copy_from_user((char *) &sdef, (char *) sdefp, sizeof(sdef)))
		return -EFAULT;
	spin_lock_irqsave(&card->isdnloop_lock, flags);
	switch (sdef.ptype) {
	case ISDN_PTYPE_EURO:
		if (isdnloop_fake(card, "DRV1.23EC-Q.931-CAPI-CNS-BASIS-20.02.96",
				  -1)) {
			spin_unlock_irqrestore(&card->isdnloop_lock, flags);
			return -ENOMEM;
		}
		card->sil[0] = card->sil[1] = 4;
		if (isdnloop_fake(card, "TEI OK", 0)) {
			spin_unlock_irqrestore(&card->isdnloop_lock, flags);
			return -ENOMEM;
		}
		for (i = 0; i < 3; i++)
			strcpy(card->s0num[i], sdef.num[i]);
		break;
	case ISDN_PTYPE_1TR6:
		if (isdnloop_fake(card, "DRV1.04TC-1TR6-CAPI-CNS-BASIS-29.11.95",
				  -1)) {
			spin_unlock_irqrestore(&card->isdnloop_lock, flags);
			return -ENOMEM;
		}
		card->sil[0] = card->sil[1] = 4;
		if (isdnloop_fake(card, "TEI OK", 0)) {
			spin_unlock_irqrestore(&card->isdnloop_lock, flags);
			return -ENOMEM;
		}
		strcpy(card->s0num[0], sdef.num[0]);
		card->s0num[1][0] = '\0';
		card->s0num[2][0] = '\0';
		break;
	default:
		spin_unlock_irqrestore(&card->isdnloop_lock, flags);
		printk(KERN_WARNING "isdnloop: Illegal D-channel protocol %d\n",
		       sdef.ptype);
		return -EINVAL;
	}
	init_timer(&card->st_timer);
	card->st_timer.expires = jiffies + ISDNLOOP_TIMER_DCREAD;
	card->st_timer.function = isdnloop_polldchan;
	card->st_timer.data = (unsigned long) card;
	add_timer(&card->st_timer);
	card->flags |= ISDNLOOP_FLAGS_RUNNING;
	spin_unlock_irqrestore(&card->isdnloop_lock, flags);
	return 0;
}

static int
isdnloop_command(isdn_ctrl *c, isdnloop_card *card)
{
	ulong a;
	int i;
	char cbuf[60];
	isdn_ctrl cmd;
	isdnloop_cdef cdef;

	switch (c->command) {
	case ISDN_CMD_IOCTL:
		memcpy(&a, c->parm.num, sizeof(ulong));
		switch (c->arg) {
		case ISDNLOOP_IOCTL_DEBUGVAR:
			return (ulong) card;
		case ISDNLOOP_IOCTL_STARTUP:
			if (!access_ok(VERIFY_READ, (void *) a, sizeof(isdnloop_sdef)))
				return -EFAULT;
			return (isdnloop_start(card, (isdnloop_sdef *) a));
			break;
		case ISDNLOOP_IOCTL_ADDCARD:
			if (copy_from_user((char *)&cdef,
					   (char *)a,
					   sizeof(cdef)))
				return -EFAULT;
			return (isdnloop_addcard(cdef.id1));
			break;
		case ISDNLOOP_IOCTL_LEASEDCFG:
			if (a) {
				if (!card->leased) {
					card->leased = 1;
					while (card->ptype == ISDN_PTYPE_UNKNOWN)
						schedule_timeout_interruptible(10);
					schedule_timeout_interruptible(10);
					sprintf(cbuf, "00;FV2ON\n01;EAZ1\n02;EAZ2\n");
					i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
					printk(KERN_INFO
					       "isdnloop: (%s) Leased-line mode enabled\n",
					       CID);
					cmd.command = ISDN_STAT_RUN;
					cmd.driver = card->myid;
					cmd.arg = 0;
					card->interface.statcallb(&cmd);
				}
			} else {
				if (card->leased) {
					card->leased = 0;
					sprintf(cbuf, "00;FV2OFF\n");
					i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
					printk(KERN_INFO
					       "isdnloop: (%s) Leased-line mode disabled\n",
					       CID);
					cmd.command = ISDN_STAT_RUN;
					cmd.driver = card->myid;
					cmd.arg = 0;
					card->interface.statcallb(&cmd);
				}
			}
			return 0;
		default:
			return -EINVAL;
		}
		break;
	case ISDN_CMD_DIAL:
		if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
			return -ENODEV;
		if (card->leased)
			break;
		if ((c->arg & 255) < ISDNLOOP_BCH) {
			char *p;
			char dial[50];
			char dcode[4];

			a = c->arg;
			p = c->parm.setup.phone;
			if (*p == 's' || *p == 'S') {
				
				p++;
				strcpy(dcode, "SCA");
			} else
				
				strcpy(dcode, "CAL");
			strcpy(dial, p);
			sprintf(cbuf, "%02d;D%s_R%s,%02d,%02d,%s\n", (int) (a + 1),
				dcode, dial, c->parm.setup.si1,
				c->parm.setup.si2, c->parm.setup.eazmsn);
			i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
		}
		break;
	case ISDN_CMD_ACCEPTD:
		if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
			return -ENODEV;
		if (c->arg < ISDNLOOP_BCH) {
			a = c->arg + 1;
			cbuf[0] = 0;
			switch (card->l2_proto[a - 1]) {
			case ISDN_PROTO_L2_X75I:
				sprintf(cbuf, "%02d;BX75\n", (int) a);
				break;
#ifdef CONFIG_ISDN_X25
			case ISDN_PROTO_L2_X25DTE:
				sprintf(cbuf, "%02d;BX2T\n", (int) a);
				break;
			case ISDN_PROTO_L2_X25DCE:
				sprintf(cbuf, "%02d;BX2C\n", (int) a);
				break;
#endif
			case ISDN_PROTO_L2_HDLC:
				sprintf(cbuf, "%02d;BTRA\n", (int) a);
				break;
			}
			if (strlen(cbuf))
				i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
			sprintf(cbuf, "%02d;DCON_R\n", (int) a);
			i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
		}
		break;
	case ISDN_CMD_ACCEPTB:
		if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
			return -ENODEV;
		if (c->arg < ISDNLOOP_BCH) {
			a = c->arg + 1;
			switch (card->l2_proto[a - 1]) {
			case ISDN_PROTO_L2_X75I:
				sprintf(cbuf, "%02d;BCON_R,BX75\n", (int) a);
				break;
#ifdef CONFIG_ISDN_X25
			case ISDN_PROTO_L2_X25DTE:
				sprintf(cbuf, "%02d;BCON_R,BX2T\n", (int) a);
				break;
			case ISDN_PROTO_L2_X25DCE:
				sprintf(cbuf, "%02d;BCON_R,BX2C\n", (int) a);
				break;
#endif
			case ISDN_PROTO_L2_HDLC:
				sprintf(cbuf, "%02d;BCON_R,BTRA\n", (int) a);
				break;
			default:
				sprintf(cbuf, "%02d;BCON_R\n", (int) a);
			}
			printk(KERN_DEBUG "isdnloop writecmd '%s'\n", cbuf);
			i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
			break;
		case ISDN_CMD_HANGUP:
			if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
				return -ENODEV;
			if (c->arg < ISDNLOOP_BCH) {
				a = c->arg + 1;
				sprintf(cbuf, "%02d;BDIS_R\n%02d;DDIS_R\n", (int) a, (int) a);
				i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
			}
			break;
		case ISDN_CMD_SETEAZ:
			if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
				return -ENODEV;
			if (card->leased)
				break;
			if (c->arg < ISDNLOOP_BCH) {
				a = c->arg + 1;
				if (card->ptype == ISDN_PTYPE_EURO) {
					sprintf(cbuf, "%02d;MS%s%s\n", (int) a,
						c->parm.num[0] ? "N" : "ALL", c->parm.num);
				} else
					sprintf(cbuf, "%02d;EAZ%s\n", (int) a,
						c->parm.num[0] ? c->parm.num : (u_char *) "0123456789");
				i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
			}
			break;
		case ISDN_CMD_CLREAZ:
			if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
				return -ENODEV;
			if (card->leased)
				break;
			if (c->arg < ISDNLOOP_BCH) {
				a = c->arg + 1;
				if (card->ptype == ISDN_PTYPE_EURO)
					sprintf(cbuf, "%02d;MSNC\n", (int) a);
				else
					sprintf(cbuf, "%02d;EAZC\n", (int) a);
				i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
			}
			break;
		case ISDN_CMD_SETL2:
			if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
				return -ENODEV;
			if ((c->arg & 255) < ISDNLOOP_BCH) {
				a = c->arg;
				switch (a >> 8) {
				case ISDN_PROTO_L2_X75I:
					sprintf(cbuf, "%02d;BX75\n", (int) (a & 255) + 1);
					break;
#ifdef CONFIG_ISDN_X25
				case ISDN_PROTO_L2_X25DTE:
					sprintf(cbuf, "%02d;BX2T\n", (int) (a & 255) + 1);
					break;
				case ISDN_PROTO_L2_X25DCE:
					sprintf(cbuf, "%02d;BX2C\n", (int) (a & 255) + 1);
					break;
#endif
				case ISDN_PROTO_L2_HDLC:
					sprintf(cbuf, "%02d;BTRA\n", (int) (a & 255) + 1);
					break;
				case ISDN_PROTO_L2_TRANS:
					sprintf(cbuf, "%02d;BTRA\n", (int) (a & 255) + 1);
					break;
				default:
					return -EINVAL;
				}
				i = isdnloop_writecmd(cbuf, strlen(cbuf), 0, card);
				card->l2_proto[a & 255] = (a >> 8);
			}
			break;
		case ISDN_CMD_SETL3:
			if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
				return -ENODEV;
			return 0;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

static inline isdnloop_card *
isdnloop_findcard(int driverid)
{
	isdnloop_card *p = cards;

	while (p) {
		if (p->myid == driverid)
			return p;
		p = p->next;
	}
	return (isdnloop_card *) 0;
}

static int
if_command(isdn_ctrl *c)
{
	isdnloop_card *card = isdnloop_findcard(c->driver);

	if (card)
		return (isdnloop_command(c, card));
	printk(KERN_ERR
	       "isdnloop: if_command called with invalid driverId!\n");
	return -ENODEV;
}

static int
if_writecmd(const u_char __user *buf, int len, int id, int channel)
{
	isdnloop_card *card = isdnloop_findcard(id);

	if (card) {
		if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
			return -ENODEV;
		return (isdnloop_writecmd(buf, len, 1, card));
	}
	printk(KERN_ERR
	       "isdnloop: if_writecmd called with invalid driverId!\n");
	return -ENODEV;
}

static int
if_readstatus(u_char __user *buf, int len, int id, int channel)
{
	isdnloop_card *card = isdnloop_findcard(id);

	if (card) {
		if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
			return -ENODEV;
		return (isdnloop_readstatus(buf, len, card));
	}
	printk(KERN_ERR
	       "isdnloop: if_readstatus called with invalid driverId!\n");
	return -ENODEV;
}

static int
if_sendbuf(int id, int channel, int ack, struct sk_buff *skb)
{
	isdnloop_card *card = isdnloop_findcard(id);

	if (card) {
		if (!(card->flags & ISDNLOOP_FLAGS_RUNNING))
			return -ENODEV;
		
		*(skb->head) = ack;
		return (isdnloop_sendbuf(channel, skb, card));
	}
	printk(KERN_ERR
	       "isdnloop: if_sendbuf called with invalid driverId!\n");
	return -ENODEV;
}

static isdnloop_card *
isdnloop_initcard(char *id)
{
	isdnloop_card *card;
	int i;

	if (!(card = kzalloc(sizeof(isdnloop_card), GFP_KERNEL))) {
		printk(KERN_WARNING
		       "isdnloop: (%s) Could not allocate card-struct.\n", id);
		return (isdnloop_card *) 0;
	}
	card->interface.owner = THIS_MODULE;
	card->interface.channels = ISDNLOOP_BCH;
	card->interface.hl_hdrlen  = 1; 
	card->interface.maxbufsize = 4000;
	card->interface.command = if_command;
	card->interface.writebuf_skb = if_sendbuf;
	card->interface.writecmd = if_writecmd;
	card->interface.readstat = if_readstatus;
	card->interface.features = ISDN_FEATURE_L2_X75I |
#ifdef CONFIG_ISDN_X25
		ISDN_FEATURE_L2_X25DTE |
		ISDN_FEATURE_L2_X25DCE |
#endif
		ISDN_FEATURE_L2_HDLC |
		ISDN_FEATURE_L3_TRANS |
		ISDN_FEATURE_P_UNKNOWN;
	card->ptype = ISDN_PTYPE_UNKNOWN;
	strlcpy(card->interface.id, id, sizeof(card->interface.id));
	card->msg_buf_write = card->msg_buf;
	card->msg_buf_read = card->msg_buf;
	card->msg_buf_end = &card->msg_buf[sizeof(card->msg_buf) - 1];
	for (i = 0; i < ISDNLOOP_BCH; i++) {
		card->l2_proto[i] = ISDN_PROTO_L2_X75I;
		skb_queue_head_init(&card->bqueue[i]);
	}
	skb_queue_head_init(&card->dqueue);
	spin_lock_init(&card->isdnloop_lock);
	card->next = cards;
	cards = card;
	if (!register_isdn(&card->interface)) {
		cards = cards->next;
		printk(KERN_WARNING
		       "isdnloop: Unable to register %s\n", id);
		kfree(card);
		return (isdnloop_card *) 0;
	}
	card->myid = card->interface.channels;
	return card;
}

static int
isdnloop_addcard(char *id1)
{
	isdnloop_card *card;

	if (!(card = isdnloop_initcard(id1))) {
		return -EIO;
	}
	printk(KERN_INFO
	       "isdnloop: (%s) virtual card added\n",
	       card->interface.id);
	return 0;
}

static int __init
isdnloop_init(void)
{
	char *p;
	char rev[10];

	if ((p = strchr(revision, ':'))) {
		strcpy(rev, p + 1);
		p = strchr(rev, '$');
		*p = 0;
	} else
		strcpy(rev, " ??? ");
	printk(KERN_NOTICE "isdnloop-ISDN-driver Rev%s\n", rev);

	if (isdnloop_id)
		return (isdnloop_addcard(isdnloop_id));

	return 0;
}

static void __exit
isdnloop_exit(void)
{
	isdn_ctrl cmd;
	isdnloop_card *card = cards;
	isdnloop_card *last;
	int i;

	isdnloop_stopallcards();
	while (card) {
		cmd.command = ISDN_STAT_UNLOAD;
		cmd.driver = card->myid;
		card->interface.statcallb(&cmd);
		for (i = 0; i < ISDNLOOP_BCH; i++)
			isdnloop_free_queue(card, i);
		card = card->next;
	}
	card = cards;
	while (card) {
		last = card;
		skb_queue_purge(&card->dqueue);
		card = card->next;
		kfree(last);
	}
	printk(KERN_NOTICE "isdnloop-ISDN-driver unloaded\n");
}

module_init(isdnloop_init);
module_exit(isdnloop_exit);
