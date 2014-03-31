/* $Id: isdn_divert.c,v 1.6.6.3 2001/09/23 22:24:36 kai Exp $
 *
 * DSS1 main diversion supplementary handling for i4l.
 *
 * Copyright 1999       by Werner Cornelius (werner@isdn4linux.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include "isdn_divert.h"

struct call_struc
{ isdn_ctrl ics; 
	ulong divert_id; 
	unsigned char akt_state; 
	char deflect_dest[35]; 
	struct timer_list timer; 
	char info[90]; 
	struct call_struc *next; 
	struct call_struc *prev;
};


struct deflect_struc
{ struct deflect_struc *next, *prev;
	divert_rule rule; 
};


static struct call_struc *divert_head = NULL; 
static ulong next_id = 1; 
static struct deflect_struc *table_head = NULL;
static struct deflect_struc *table_tail = NULL;
static unsigned char extern_wait_max = 4; 

DEFINE_SPINLOCK(divert_lock);

static void deflect_timer_expire(ulong arg)
{
	unsigned long flags;
	struct call_struc *cs = (struct call_struc *) arg;

	spin_lock_irqsave(&divert_lock, flags);
	del_timer(&cs->timer); 
	spin_unlock_irqrestore(&divert_lock, flags);

	switch (cs->akt_state)
	{ case DEFLECT_PROCEED:
			cs->ics.command = ISDN_CMD_HANGUP; 
			divert_if.ll_cmd(&cs->ics);
			spin_lock_irqsave(&divert_lock, flags);
			cs->akt_state = DEFLECT_AUTODEL; 
			cs->timer.expires = jiffies + (HZ * AUTODEL_TIME);
			add_timer(&cs->timer);
			spin_unlock_irqrestore(&divert_lock, flags);
			break;

	case DEFLECT_ALERT:
		cs->ics.command = ISDN_CMD_REDIR; 
		strlcpy(cs->ics.parm.setup.phone, cs->deflect_dest, sizeof(cs->ics.parm.setup.phone));
		strcpy(cs->ics.parm.setup.eazmsn, "Testtext delayed");
		divert_if.ll_cmd(&cs->ics);
		spin_lock_irqsave(&divert_lock, flags);
		cs->akt_state = DEFLECT_AUTODEL; 
		cs->timer.expires = jiffies + (HZ * AUTODEL_TIME);
		add_timer(&cs->timer);
		spin_unlock_irqrestore(&divert_lock, flags);
		break;

	case DEFLECT_AUTODEL:
	default:
		spin_lock_irqsave(&divert_lock, flags);
		if (cs->prev)
			cs->prev->next = cs->next; 
		else
			divert_head = cs->next;
		if (cs->next)
			cs->next->prev = cs->prev; 
		spin_unlock_irqrestore(&divert_lock, flags);
		kfree(cs);
		return;

	} 
} 


int cf_command(int drvid, int mode,
	       u_char proc, char *msn,
	       u_char service, char *fwd_nr, ulong *procid)
{ unsigned long flags;
	int retval, msnlen;
	int fwd_len;
	char *p, *ielenp, tmp[60];
	struct call_struc *cs;

	if (strchr(msn, '.')) return (-EINVAL); 
	if ((proc & 0x7F) > 2) return (-EINVAL);
	proc &= 3;
	p = tmp;
	*p++ = 0x30; 
	ielenp = p++; 
	*p++ = 0xa; 
	*p++ = 1;   
	*p++ = proc & 0x7F; 
	*p++ = 0xa; 
	*p++ = 1;   
	*p++ = service; 

	if (mode == 1)
	{ if (!*fwd_nr) return (-EINVAL); 
		if (strchr(fwd_nr, '.')) return (-EINVAL); 
		fwd_len = strlen(fwd_nr);
		*p++ = 0x30; 
		*p++ = fwd_len + 2; 
		*p++ = 0x80; 
		*p++ = fwd_len; 
		strcpy(p, fwd_nr); 
		p += fwd_len; 
	} 

	msnlen = strlen(msn);
	*p++ = 0x80; 
	if (msnlen > 1)
	{ *p++ = msnlen; 
		strcpy(p, msn);
		p += msnlen;
	}
	else *p++ = 0;

	*ielenp = p - ielenp - 1; 

	
	if (!(cs = kmalloc(sizeof(struct call_struc), GFP_ATOMIC)))
		return (-ENOMEM); 
	init_timer(&cs->timer);
	cs->info[0] = '\0';
	cs->timer.function = deflect_timer_expire;
	cs->timer.data = (ulong) cs; 
	cs->ics.driver = drvid;
	cs->ics.command = ISDN_CMD_PROT_IO; 
	cs->ics.arg = DSS1_CMD_INVOKE; 
	cs->ics.parm.dss1_io.proc = (mode == 1) ? 7 : (mode == 2) ? 11 : 8; 
	cs->ics.parm.dss1_io.timeout = 4000; 
	cs->ics.parm.dss1_io.datalen = p - tmp; 
	cs->ics.parm.dss1_io.data = tmp; 

	spin_lock_irqsave(&divert_lock, flags);
	cs->ics.parm.dss1_io.ll_id = next_id++; 
	spin_unlock_irqrestore(&divert_lock, flags);
	*procid = cs->ics.parm.dss1_io.ll_id;

	sprintf(cs->info, "%d 0x%lx %s%s 0 %s %02x %d%s%s\n",
		(!mode) ? DIVERT_DEACTIVATE : (mode == 1) ? DIVERT_ACTIVATE : DIVERT_REPORT,
		cs->ics.parm.dss1_io.ll_id,
		(mode != 2) ? "" : "0 ",
		divert_if.drv_to_name(cs->ics.driver),
		msn,
		service & 0xFF,
		proc,
		(mode != 1) ? "" : " 0 ",
		(mode != 1) ? "" : fwd_nr);

	retval = divert_if.ll_cmd(&cs->ics); 

	if (!retval)
	{ cs->prev = NULL;
		spin_lock_irqsave(&divert_lock, flags);
		cs->next = divert_head;
		divert_head = cs;
		spin_unlock_irqrestore(&divert_lock, flags);
	}
	else
		kfree(cs);
	return (retval);
} 


int deflect_extern_action(u_char cmd, ulong callid, char *to_nr)
{ struct call_struc *cs;
	isdn_ctrl ic;
	unsigned long flags;
	int i;

	if ((cmd & 0x7F) > 2) return (-EINVAL); 
	cs = divert_head; 
	while (cs)
	{ if (cs->divert_id == callid) break; 
		cs = cs->next;
	} 
	if (!cs) return (-EINVAL); 

	ic.driver = cs->ics.driver;
	ic.arg = cs->ics.arg;
	i = -EINVAL;
	if (cs->akt_state == DEFLECT_AUTODEL) return (i); 
	switch (cmd & 0x7F)
	{ case 0: 
			del_timer(&cs->timer);
			ic.command = ISDN_CMD_HANGUP;
			i = divert_if.ll_cmd(&ic);
			spin_lock_irqsave(&divert_lock, flags);
			cs->akt_state = DEFLECT_AUTODEL; 
			cs->timer.expires = jiffies + (HZ * AUTODEL_TIME);
			add_timer(&cs->timer);
			spin_unlock_irqrestore(&divert_lock, flags);
			break;

	case 1: 
		if (cs->akt_state == DEFLECT_ALERT) return (0);
		cmd &= 0x7F; 
		del_timer(&cs->timer);
		ic.command = ISDN_CMD_ALERT;
		if ((i = divert_if.ll_cmd(&ic)))
		{
			spin_lock_irqsave(&divert_lock, flags);
			cs->akt_state = DEFLECT_AUTODEL; 
			cs->timer.expires = jiffies + (HZ * AUTODEL_TIME);
			add_timer(&cs->timer);
			spin_unlock_irqrestore(&divert_lock, flags);
		}
		else
			cs->akt_state = DEFLECT_ALERT;
		break;

	case 2: 
		del_timer(&cs->timer);
		strlcpy(cs->ics.parm.setup.phone, to_nr, sizeof(cs->ics.parm.setup.phone));
		strcpy(cs->ics.parm.setup.eazmsn, "Testtext manual");
		ic.command = ISDN_CMD_REDIR;
		if ((i = divert_if.ll_cmd(&ic)))
		{
			spin_lock_irqsave(&divert_lock, flags);
			cs->akt_state = DEFLECT_AUTODEL; 
			cs->timer.expires = jiffies + (HZ * AUTODEL_TIME);
			add_timer(&cs->timer);
			spin_unlock_irqrestore(&divert_lock, flags);
		}
		else
			cs->akt_state = DEFLECT_ALERT;
		break;

	} 
	return (i);
} 

int insertrule(int idx, divert_rule *newrule)
{ struct deflect_struc *ds, *ds1 = NULL;
	unsigned long flags;

	if (!(ds = kmalloc(sizeof(struct deflect_struc),
			   GFP_KERNEL)))
		return (-ENOMEM); 

	ds->rule = *newrule; 

	spin_lock_irqsave(&divert_lock, flags);

	if (idx >= 0)
	{ ds1 = table_head;
		while ((ds1) && (idx > 0))
		{ idx--;
			ds1 = ds1->next;
		}
		if (!ds1) idx = -1;
	}

	if (idx < 0)
	{ ds->prev = table_tail; 
		ds->next = NULL; 
		if (ds->prev)
			ds->prev->next = ds; 
		else
			table_head = ds; 
		table_tail = ds; 
	}
	else
	{ ds->next = ds1; 
		ds->prev = ds1->prev; 
		ds1->prev = ds; 
		if (!ds->prev)
			table_head = ds; 
	}

	spin_unlock_irqrestore(&divert_lock, flags);
	return (0);
} 

int deleterule(int idx)
{ struct deflect_struc *ds, *ds1;
	unsigned long flags;

	if (idx < 0)
	{ spin_lock_irqsave(&divert_lock, flags);
		ds = table_head;
		table_head = NULL;
		table_tail = NULL;
		spin_unlock_irqrestore(&divert_lock, flags);
		while (ds)
		{ ds1 = ds;
			ds = ds->next;
			kfree(ds1);
		}
		return (0);
	}

	spin_lock_irqsave(&divert_lock, flags);
	ds = table_head;

	while ((ds) && (idx > 0))
	{ idx--;
		ds = ds->next;
	}

	if (!ds)
	{
		spin_unlock_irqrestore(&divert_lock, flags);
		return (-EINVAL);
	}

	if (ds->next)
		ds->next->prev = ds->prev; 
	else
		table_tail = ds->prev; 

	if (ds->prev)
		ds->prev->next = ds->next; 
	else
		table_head = ds->next; 

	spin_unlock_irqrestore(&divert_lock, flags);
	kfree(ds);
	return (0);
} 

divert_rule *getruleptr(int idx)
{ struct deflect_struc *ds = table_head;

	if (idx < 0) return (NULL);
	while ((ds) && (idx >= 0))
	{ if (!(idx--))
		{ return (&ds->rule);
			break;
		}
		ds = ds->next;
	}
	return (NULL);
} 

static int isdn_divert_icall(isdn_ctrl *ic)
{ int retval = 0;
	unsigned long flags;
	struct call_struc *cs = NULL;
	struct deflect_struc *dv;
	char *p, *p1;
	u_char accept;

	
	for (dv = table_head; dv; dv = dv->next)
	{ 
		if (((dv->rule.callopt == 1) && (ic->command == ISDN_STAT_ICALLW)) ||
		    ((dv->rule.callopt == 2) && (ic->command == ISDN_STAT_ICALL)))
			continue; 
		if (!(dv->rule.drvid & (1L << ic->driver)))
			continue; 
		if ((dv->rule.si1) && (dv->rule.si1 != ic->parm.setup.si1))
			continue; 
		if ((dv->rule.si2) && (dv->rule.si2 != ic->parm.setup.si2))
			continue; 

		p = dv->rule.my_msn;
		p1 = ic->parm.setup.eazmsn;
		accept = 0;
		while (*p)
		{ 
			if (*p == '-')
			{ accept = 1; 
				break;
			}
			if (*p++ != *p1++)
				break; 
			if ((!*p) && (!*p1))
				accept = 1;
		} 
		if (!accept) continue; 

		if ((strcmp(dv->rule.caller, "0")) || (ic->parm.setup.phone[0]))
		{ p = dv->rule.caller;
			p1 = ic->parm.setup.phone;
			accept = 0;
			while (*p)
			{ 
				if (*p == '-')
				{ accept = 1; 
					break;
				}
				if (*p++ != *p1++)
					break; 
				if ((!*p) && (!*p1))
					accept = 1;
			} 
			if (!accept) continue; 
		}

		switch (dv->rule.action)
		{ case DEFLECT_IGNORE:
				return (0);
				break;

		case DEFLECT_ALERT:
		case DEFLECT_PROCEED:
		case DEFLECT_REPORT:
		case DEFLECT_REJECT:
			if (dv->rule.action == DEFLECT_PROCEED)
				if ((!if_used) || ((!extern_wait_max) && (!dv->rule.waittime)))
					return (0); 
			if (!(cs = kmalloc(sizeof(struct call_struc), GFP_ATOMIC)))
				return (0); 
			init_timer(&cs->timer);
			cs->info[0] = '\0';
			cs->timer.function = deflect_timer_expire;
			cs->timer.data = (ulong) cs; 

			cs->ics = *ic; 
			if (!cs->ics.parm.setup.phone[0]) strcpy(cs->ics.parm.setup.phone, "0");
			if (!cs->ics.parm.setup.eazmsn[0]) strcpy(cs->ics.parm.setup.eazmsn, "0");
			cs->ics.parm.setup.screen = dv->rule.screen;
			if (dv->rule.waittime)
				cs->timer.expires = jiffies + (HZ * dv->rule.waittime);
			else
				if (dv->rule.action == DEFLECT_PROCEED)
					cs->timer.expires = jiffies + (HZ * extern_wait_max);
				else
					cs->timer.expires = 0;
			cs->akt_state = dv->rule.action;
			spin_lock_irqsave(&divert_lock, flags);
			cs->divert_id = next_id++; 
			spin_unlock_irqrestore(&divert_lock, flags);
			cs->prev = NULL;
			if (cs->akt_state == DEFLECT_ALERT)
			{ strcpy(cs->deflect_dest, dv->rule.to_nr);
				if (!cs->timer.expires)
				{ strcpy(ic->parm.setup.eazmsn, "Testtext direct");
					ic->parm.setup.screen = dv->rule.screen;
					strlcpy(ic->parm.setup.phone, dv->rule.to_nr, sizeof(ic->parm.setup.phone));
					cs->akt_state = DEFLECT_AUTODEL; 
					cs->timer.expires = jiffies + (HZ * AUTODEL_TIME);
					retval = 5;
				}
				else
					retval = 1; 
			}
			else
			{ cs->deflect_dest[0] = '\0';
				retval = 4; 
			}
			sprintf(cs->info, "%d 0x%lx %s %s %s %s 0x%x 0x%x %d %d %s\n",
				cs->akt_state,
				cs->divert_id,
				divert_if.drv_to_name(cs->ics.driver),
				(ic->command == ISDN_STAT_ICALLW) ? "1" : "0",
				cs->ics.parm.setup.phone,
				cs->ics.parm.setup.eazmsn,
				cs->ics.parm.setup.si1,
				cs->ics.parm.setup.si2,
				cs->ics.parm.setup.screen,
				dv->rule.waittime,
				cs->deflect_dest);
			if ((dv->rule.action == DEFLECT_REPORT) ||
			    (dv->rule.action == DEFLECT_REJECT))
			{ put_info_buffer(cs->info);
				kfree(cs); 
				return ((dv->rule.action == DEFLECT_REPORT) ? 0 : 2); 
			}
			break;

		default:
			return (0); 
			break;
		} 
		break;
	} 

	if (cs)
	{ cs->prev = NULL;
		spin_lock_irqsave(&divert_lock, flags);
		cs->next = divert_head;
		divert_head = cs;
		if (cs->timer.expires) add_timer(&cs->timer);
		spin_unlock_irqrestore(&divert_lock, flags);

		put_info_buffer(cs->info);
		return (retval);
	}
	else
		return (0);
} 


void deleteprocs(void)
{ struct call_struc *cs, *cs1;
	unsigned long flags;

	spin_lock_irqsave(&divert_lock, flags);
	cs = divert_head;
	divert_head = NULL;
	while (cs)
	{ del_timer(&cs->timer);
		cs1 = cs;
		cs = cs->next;
		kfree(cs1);
	}
	spin_unlock_irqrestore(&divert_lock, flags);
} 

static int put_address(char *st, u_char *p, int len)
{ u_char retval = 0;
	u_char adr_typ = 0; 

	if (len < 2) return (retval);
	if (*p == 0xA1)
	{ retval = *(++p) + 2; 
		if (retval > len) return (0); 
		len = retval - 2; 
		if (len < 3) return (0);
		if ((*(++p) != 0x0A) || (*(++p) != 1)) return (0);
		adr_typ = *(++p);
		len -= 3;
		p++;
		if (len < 2) return (0);
		if (*p++ != 0x12) return (0);
		if (*p > len) return (0); 
		len = *p++;
	}
	else
		if (*p == 0x80)
		{ retval = *(++p) + 2; 
			if (retval > len) return (0);
			len = retval - 2;
			p++;
		}
		else
			return (0); 

	sprintf(st, "%d ", adr_typ);
	st += strlen(st);
	if (!len)
		*st++ = '-';
	else
		while (len--)
			*st++ = *p++;
	*st = '\0';
	return (retval);
} 

static int interrogate_success(isdn_ctrl *ic, struct call_struc *cs)
{ char *src = ic->parm.dss1_io.data;
	int restlen = ic->parm.dss1_io.datalen;
	int cnt = 1;
	u_char n, n1;
	char st[90], *p, *stp;

	if (restlen < 2) return (-100); 
	if (*src++ != 0x30) return (-101);
	if ((n = *src++) > 0x81) return (-102); 
	restlen -= 2; 
	if (n == 0x80)
	{ if (restlen < 2) return (-103);
		if ((*(src + restlen - 1)) || (*(src + restlen - 2))) return (-104);
		restlen -= 2;
	}
	else
		if (n == 0x81)
		{ n = *src++;
			restlen--;
			if (n > restlen) return (-105);
			restlen = n;
		}
		else
			if (n > restlen) return (-106);
			else
				restlen = n; 
	if (restlen < 3) return (-107); 
	if ((*src++ != 2) || (*src++ != 1) || (*src++ != 0x0B)) return (-108);
	restlen -= 3;
	if (restlen < 2) return (-109); 
	if (*src == 0x31)
	{ src++;
		if ((n = *src++) > 0x81) return (-110); 
		restlen -= 2; 
		if (n == 0x80)
		{ if (restlen < 2) return (-111);
			if ((*(src + restlen - 1)) || (*(src + restlen - 2))) return (-112);
			restlen -= 2;
		}
		else
			if (n == 0x81)
			{ n = *src++;
				restlen--;
				if (n > restlen) return (-113);
				restlen = n;
			}
			else
				if (n > restlen) return (-114);
				else
					restlen = n; 
	} 

	while (restlen >= 2)
	{ stp = st;
		sprintf(stp, "%d 0x%lx %d %s ", DIVERT_REPORT, ic->parm.dss1_io.ll_id,
			cnt++, divert_if.drv_to_name(ic->driver));
		stp += strlen(stp);
		if (*src++ != 0x30) return (-115); 
		n = *src++;
		restlen -= 2;
		if (n > restlen) return (-116); 
		restlen -= n;
		p = src; 
		src += n;
		if (!(n1 = put_address(stp, p, n & 0xFF))) continue;
		stp += strlen(stp);
		p += n1;
		n -= n1;
		if (n < 6) continue; 
		if ((*p++ != 0x0A) || (*p++ != 1)) continue;
		sprintf(stp, " 0x%02x ", (*p++) & 0xFF);
		stp += strlen(stp);
		if ((*p++ != 0x0A) || (*p++ != 1)) continue;
		sprintf(stp, "%d ", (*p++) & 0xFF);
		stp += strlen(stp);
		n -= 6;
		if (n > 2)
		{ if (*p++ != 0x30) continue;
			if (*p > (n - 2)) continue;
			n = *p++;
			if (!(n1 = put_address(stp, p, n & 0xFF))) continue;
			stp += strlen(stp);
		}
		sprintf(stp, "\n");
		put_info_buffer(st);
	} 
	if (restlen) return (-117);
	return (0);
} 

static int prot_stat_callback(isdn_ctrl *ic)
{ struct call_struc *cs, *cs1;
	int i;
	unsigned long flags;

	cs = divert_head; 
	cs1 = NULL;
	while (cs)
	{ if (ic->driver == cs->ics.driver)
		{ switch (cs->ics.arg)
			{ case DSS1_CMD_INVOKE:
					if ((cs->ics.parm.dss1_io.ll_id == ic->parm.dss1_io.ll_id) &&
					    (cs->ics.parm.dss1_io.hl_id == ic->parm.dss1_io.hl_id))
					{ switch (ic->arg)
						{  case DSS1_STAT_INVOKE_ERR:
								sprintf(cs->info, "128 0x%lx 0x%x\n",
									ic->parm.dss1_io.ll_id,
									ic->parm.dss1_io.timeout);
								put_info_buffer(cs->info);
								break;

						case DSS1_STAT_INVOKE_RES:
							switch (cs->ics.parm.dss1_io.proc)
							{  case  7:
							case  8:
								put_info_buffer(cs->info);
								break;

							case  11:
								i = interrogate_success(ic, cs);
								if (i)
									sprintf(cs->info, "%d 0x%lx %d\n", DIVERT_REPORT,
										ic->parm.dss1_io.ll_id, i);
								put_info_buffer(cs->info);
								break;

							default:
								printk(KERN_WARNING "dss1_divert: unknown proc %d\n", cs->ics.parm.dss1_io.proc);
								break;
							}


							break;

						default:
							printk(KERN_WARNING "dss1_divert unknown invoke answer %lx\n", ic->arg);
							break;
						}
						cs1 = cs; 
						cs = NULL;
						continue; 
					} 
					break;

			case DSS1_CMD_INVOKE_ABORT:
				printk(KERN_WARNING "dss1_divert unhandled invoke abort\n");
				break;

			default:
				printk(KERN_WARNING "dss1_divert unknown cmd 0x%lx\n", cs->ics.arg);
				break;
			} 
			cs = cs->next;
		} 
	}

	if (!cs1)
	{ printk(KERN_WARNING "dss1_divert unhandled process\n");
		return (0);
	}

	if (cs1->ics.driver == -1)
	{
		spin_lock_irqsave(&divert_lock, flags);
		del_timer(&cs1->timer);
		if (cs1->prev)
			cs1->prev->next = cs1->next; 
		else
			divert_head = cs1->next;
		if (cs1->next)
			cs1->next->prev = cs1->prev; 
		spin_unlock_irqrestore(&divert_lock, flags);
		kfree(cs1);
	}

	return (0);
} 


static int isdn_divert_stat_callback(isdn_ctrl *ic)
{ struct call_struc *cs, *cs1;
	unsigned long flags;
	int retval;

	retval = -1;
	cs = divert_head; 
	while (cs)
	{ if ((ic->driver == cs->ics.driver) && (ic->arg == cs->ics.arg))
		{ switch (ic->command)
			{ case ISDN_STAT_DHUP:
					sprintf(cs->info, "129 0x%lx\n", cs->divert_id);
					del_timer(&cs->timer);
					cs->ics.driver = -1;
					break;

			case ISDN_STAT_CAUSE:
				sprintf(cs->info, "130 0x%lx %s\n", cs->divert_id, ic->parm.num);
				break;

			case ISDN_STAT_REDIR:
				sprintf(cs->info, "131 0x%lx\n", cs->divert_id);
				del_timer(&cs->timer);
				cs->ics.driver = -1;
				break;

			default:
				sprintf(cs->info, "999 0x%lx 0x%x\n", cs->divert_id, (int)(ic->command));
				break;
			}
			put_info_buffer(cs->info);
			retval = 0;
		}
		cs1 = cs;
		cs = cs->next;
		if (cs1->ics.driver == -1)
		{
			spin_lock_irqsave(&divert_lock, flags);
			if (cs1->prev)
				cs1->prev->next = cs1->next; 
			else
				divert_head = cs1->next;
			if (cs1->next)
				cs1->next->prev = cs1->prev; 
			spin_unlock_irqrestore(&divert_lock, flags);
			kfree(cs1);
		}
	}
	return (retval); 
} 


int ll_callback(isdn_ctrl *ic)
{
	switch (ic->command)
	{ case ISDN_STAT_ICALL:
	case ISDN_STAT_ICALLW:
		return (isdn_divert_icall(ic));
		break;

	case ISDN_STAT_PROT:
		if ((ic->arg & 0xFF) == ISDN_PTYPE_EURO)
		{ if (ic->arg != DSS1_STAT_INVOKE_BRD)
				return (prot_stat_callback(ic));
			else
				return (0); 
		}
		else
			return (-1); 

	default:
		return (isdn_divert_stat_callback(ic));
	}
} 
