/* $Id: hysdn_procconf.c,v 1.8.6.4 2001/09/23 22:24:54 kai Exp $
 *
 * Linux driver for HYSDN cards, /proc/net filesystem dir and conf functions.
 *
 * written by Werner Cornelius (werner@titro.de) for Hypercope GmbH
 *
 * Copyright 1999  by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/cred.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <net/net_namespace.h>

#include "hysdn_defs.h"

static DEFINE_MUTEX(hysdn_conf_mutex);

#define INFO_OUT_LEN 80		

#define CONF_STATE_DETECT 0	
#define CONF_STATE_CONF   1	
#define CONF_STATE_POF    2	
#define CONF_LINE_LEN   255	

struct conf_writedata {
	hysdn_card *card;	
	int buf_size;		
	int needed_size;	
	int state;		
	unsigned char conf_line[CONF_LINE_LEN];	
	unsigned short channel;		
	unsigned char *pof_buffer;	
};

static int
process_line(struct conf_writedata *cnf)
{
	unsigned char *cp = cnf->conf_line;
	int i;

	if (cnf->card->debug_flags & LOG_CNF_LINE)
		hysdn_addlog(cnf->card, "conf line: %s", cp);

	if (*cp == '-') {	
		cp++;		

		if (*cp++ != 'c')
			return (0);	
		i = 0;		
		while ((*cp <= '9') && (*cp >= '0'))
			i = i * 10 + *cp++ - '0';	
		if (i > 65535) {
			if (cnf->card->debug_flags & LOG_CNF_MISC)
				hysdn_addlog(cnf->card, "conf channel invalid  %d", i);
			return (-ERR_INV_CHAN);		
		}
		cnf->channel = i & 0xFFFF;	
		return (0);	
	}			
	if (*cp == '*') {	
		if (cnf->card->debug_flags & LOG_CNF_DATA)
			hysdn_addlog(cnf->card, "conf chan=%d %s", cnf->channel, cp);
		return (hysdn_tx_cfgline(cnf->card, cnf->conf_line + 1,
					 cnf->channel));	
	}			
	return (0);
}				


static ssize_t
hysdn_conf_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
	struct conf_writedata *cnf;
	int i;
	unsigned char ch, *cp;

	if (!count)
		return (0);	

	if (!(cnf = file->private_data))
		return (-EFAULT);	

	if (cnf->state == CONF_STATE_DETECT) {	
		if (copy_from_user(&ch, buf, 1))	
			return (-EFAULT);

		if (ch == 0x1A) {
			
			if ((cnf->needed_size = pof_write_open(cnf->card, &cnf->pof_buffer)) <= 0)
				return (cnf->needed_size);	
			cnf->buf_size = 0;	
			cnf->state = CONF_STATE_POF;	
		} else {
			
			cnf->buf_size = 0;	
			cnf->state = CONF_STATE_CONF;	
			if (cnf->card->state != CARD_STATE_RUN)
				return (-ERR_NOT_BOOTED);
			cnf->conf_line[CONF_LINE_LEN - 1] = 0;	
			cnf->channel = 4098;	
		}
	}			
	if (cnf->state == CONF_STATE_POF) {	
		i = cnf->needed_size - cnf->buf_size;	
		if (i <= 0)
			return (-EINVAL);	

		if (i < count)
			count = i;	
		if (copy_from_user(cnf->pof_buffer + cnf->buf_size, buf, count))
			return (-EFAULT);	
		cnf->buf_size += count;

		if (cnf->needed_size == cnf->buf_size) {
			cnf->needed_size = pof_write_buffer(cnf->card, cnf->buf_size);	
			if (cnf->needed_size <= 0) {
				cnf->card->state = CARD_STATE_BOOTERR;	
				return (cnf->needed_size);	
			}
			cnf->buf_size = 0;	
		}
	}
	
	else {			

		if (cnf->card->state != CARD_STATE_RUN) {
			if (cnf->card->debug_flags & LOG_CNF_MISC)
				hysdn_addlog(cnf->card, "cnf write denied -> not booted");
			return (-ERR_NOT_BOOTED);
		}
		i = (CONF_LINE_LEN - 1) - cnf->buf_size;	
		if (i > 0) {
			

			if (count > i)
				count = i;	
			if (copy_from_user(cnf->conf_line + cnf->buf_size, buf, count))
				return (-EFAULT);	

			i = count;	
			cp = cnf->conf_line + cnf->buf_size;
			while (i) {
				
				if ((*cp < ' ') && (*cp != 9))
					break;	
				cp++;
				i--;
			}	

			if (i) {
				
				*cp++ = 0;	
				count -= (i - 1);	
				while ((i) && (*cp < ' ') && (*cp != 9)) {
					i--;	
					count++;	
					cp++;	
				}
				cnf->buf_size = 0;	
				if ((i = process_line(cnf)) < 0)	
					count = i;	
			}
			
			else {
				cnf->buf_size += count;		
				if (cnf->buf_size >= CONF_LINE_LEN - 1) {
					if (cnf->card->debug_flags & LOG_CNF_MISC)
						hysdn_addlog(cnf->card, "cnf line too long %d chars pos %d", cnf->buf_size, count);
					return (-ERR_CONF_LONG);
				}
			}	

		}
		
		else {
			if (cnf->card->debug_flags & LOG_CNF_MISC)
				hysdn_addlog(cnf->card, "cnf line too long");
			return (-ERR_CONF_LONG);
		}
	}			

	return (count);
}				

static ssize_t
hysdn_conf_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
	char *cp;

	if (!(file->f_mode & FMODE_READ))
		return -EPERM;	

	if (!(cp = file->private_data))
		return -EFAULT;	

	return simple_read_from_buffer(buf, count, off, cp, strlen(cp));
}				

static int
hysdn_conf_open(struct inode *ino, struct file *filep)
{
	hysdn_card *card;
	struct proc_dir_entry *pd;
	struct conf_writedata *cnf;
	char *cp, *tmp;

	
	mutex_lock(&hysdn_conf_mutex);
	card = card_root;
	while (card) {
		pd = card->procconf;
		if (pd == PDE(ino))
			break;
		card = card->next;	
	}
	if (!card) {
		mutex_unlock(&hysdn_conf_mutex);
		return (-ENODEV);	
	}
	if (card->debug_flags & (LOG_PROC_OPEN | LOG_PROC_ALL))
		hysdn_addlog(card, "config open for uid=%d gid=%d mode=0x%x",
			     filep->f_cred->fsuid, filep->f_cred->fsgid,
			     filep->f_mode);

	if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_WRITE) {
		

		if (!(cnf = kmalloc(sizeof(struct conf_writedata), GFP_KERNEL))) {
			mutex_unlock(&hysdn_conf_mutex);
			return (-EFAULT);
		}
		cnf->card = card;
		cnf->buf_size = 0;	
		cnf->state = CONF_STATE_DETECT;		
		filep->private_data = cnf;

	} else if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_READ) {
		

		if (!(tmp = kmalloc(INFO_OUT_LEN * 2 + 2, GFP_KERNEL))) {
			mutex_unlock(&hysdn_conf_mutex);
			return (-EFAULT);	
		}
		filep->private_data = tmp;	

		
		sprintf(tmp, "id bus slot type irq iobase dp-mem     b-chans fax-chans state device");
		cp = tmp;	
		while (*cp)
			cp++;
		while (((cp - tmp) % (INFO_OUT_LEN + 1)) != INFO_OUT_LEN)
			*cp++ = ' ';
		*cp++ = '\n';

		
		sprintf(cp, "%d  %3d %4d %4d %3d 0x%04x 0x%08lx %7d %9d %3d   %s",
			card->myid,
			card->bus,
			PCI_SLOT(card->devfn),
			card->brdtype,
			card->irq,
			card->iobase,
			card->membase,
			card->bchans,
			card->faxchans,
			card->state,
			hysdn_net_getname(card));
		while (*cp)
			cp++;
		while (((cp - tmp) % (INFO_OUT_LEN + 1)) != INFO_OUT_LEN)
			*cp++ = ' ';
		*cp++ = '\n';
		*cp = 0;	
	} else {		
		mutex_unlock(&hysdn_conf_mutex);
		return (-EPERM);	
	}
	mutex_unlock(&hysdn_conf_mutex);
	return nonseekable_open(ino, filep);
}				

static int
hysdn_conf_close(struct inode *ino, struct file *filep)
{
	hysdn_card *card;
	struct conf_writedata *cnf;
	int retval = 0;
	struct proc_dir_entry *pd;

	mutex_lock(&hysdn_conf_mutex);
	
	card = card_root;
	while (card) {
		pd = card->procconf;
		if (pd == PDE(ino))
			break;
		card = card->next;	
	}
	if (!card) {
		mutex_unlock(&hysdn_conf_mutex);
		return (-ENODEV);	
	}
	if (card->debug_flags & (LOG_PROC_OPEN | LOG_PROC_ALL))
		hysdn_addlog(card, "config close for uid=%d gid=%d mode=0x%x",
			     filep->f_cred->fsuid, filep->f_cred->fsgid,
			     filep->f_mode);

	if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_WRITE) {
		
		if (filep->private_data) {
			cnf = filep->private_data;

			if (cnf->state == CONF_STATE_POF)
				retval = pof_write_close(cnf->card);	
			kfree(filep->private_data);	

		}		
	} else if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_READ) {
		

		kfree(filep->private_data);	
	}
	mutex_unlock(&hysdn_conf_mutex);
	return (retval);
}				

static const struct file_operations conf_fops =
{
	.owner		= THIS_MODULE,
	.llseek         = no_llseek,
	.read           = hysdn_conf_read,
	.write          = hysdn_conf_write,
	.open           = hysdn_conf_open,
	.release        = hysdn_conf_close,
};

struct proc_dir_entry *hysdn_proc_entry = NULL;

int
hysdn_procconf_init(void)
{
	hysdn_card *card;
	unsigned char conf_name[20];

	hysdn_proc_entry = proc_mkdir(PROC_SUBDIR_NAME, init_net.proc_net);
	if (!hysdn_proc_entry) {
		printk(KERN_ERR "HYSDN: unable to create hysdn subdir\n");
		return (-1);
	}
	card = card_root;	
	while (card) {

		sprintf(conf_name, "%s%d", PROC_CONF_BASENAME, card->myid);
		if ((card->procconf = (void *) proc_create(conf_name,
							   S_IFREG | S_IRUGO | S_IWUSR,
							   hysdn_proc_entry,
							   &conf_fops)) != NULL) {
			hysdn_proclog_init(card);	
		}
		card = card->next;	
	}

	printk(KERN_NOTICE "HYSDN: procfs initialised\n");
	return (0);
}				

void
hysdn_procconf_release(void)
{
	hysdn_card *card;
	unsigned char conf_name[20];

	card = card_root;	
	while (card) {

		sprintf(conf_name, "%s%d", PROC_CONF_BASENAME, card->myid);
		if (card->procconf)
			remove_proc_entry(conf_name, hysdn_proc_entry);

		hysdn_proclog_release(card);	

		card = card->next;	
	}

	remove_proc_entry(PROC_SUBDIR_NAME, init_net.proc_net);
}
