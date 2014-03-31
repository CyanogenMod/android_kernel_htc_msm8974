/* $Id: hysdn_proclog.c,v 1.9.6.3 2001/09/23 22:24:54 kai Exp $
 *
 * Linux driver for HYSDN cards, /proc/net filesystem log functions.
 *
 * Author    Werner Cornelius (werner@titro.de) for Hypercope GmbH
 * Copyright 1999 by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kernel.h>

#include "hysdn_defs.h"

extern struct proc_dir_entry *hysdn_proc_entry;

static DEFINE_MUTEX(hysdn_log_mutex);
static void put_log_buffer(hysdn_card *card, char *cp);

struct log_data {
	struct log_data *next;
	unsigned long usage_cnt;
	void *proc_ctrl;	
	char log_start[2];	
};

struct procdata {
	struct proc_dir_entry *log;	
	char log_name[15];	
	struct log_data *log_head, *log_tail;	
	int if_used;		
	int volatile del_lock;	
	unsigned char logtmp[LOG_MAX_LINELEN];
	wait_queue_head_t rd_queue;
};


void
hysdn_card_errlog(hysdn_card *card, tErrLogEntry *logp, int maxsize)
{
	char buf[ERRLOG_TEXT_SIZE + 40];

	sprintf(buf, "LOG 0x%08lX 0x%08lX : %s\n", logp->ulErrType, logp->ulErrSubtype, logp->ucText);
	put_log_buffer(card, buf);	
}				

void
hysdn_addlog(hysdn_card *card, char *fmt, ...)
{
	struct procdata *pd = card->proclog;
	char *cp;
	va_list args;

	if (!pd)
		return;		

	cp = pd->logtmp;
	cp += sprintf(cp, "HYSDN: card %d ", card->myid);

	va_start(args, fmt);
	cp += vsprintf(cp, fmt, args);
	va_end(args);
	*cp++ = '\n';
	*cp = 0;

	if (card->debug_flags & DEB_OUT_SYSLOG)
		printk(KERN_INFO "%s", pd->logtmp);
	else
		put_log_buffer(card, pd->logtmp);

}				

static void
put_log_buffer(hysdn_card *card, char *cp)
{
	struct log_data *ib;
	struct procdata *pd = card->proclog;
	int i;
	unsigned long flags;

	if (!pd)
		return;
	if (!cp)
		return;
	if (!*cp)
		return;
	if (pd->if_used <= 0)
		return;		

	if (!(ib = kmalloc(sizeof(struct log_data) + strlen(cp), GFP_ATOMIC)))
		return;	
	strcpy(ib->log_start, cp);	
	ib->next = NULL;
	ib->proc_ctrl = pd;	
	spin_lock_irqsave(&card->hysdn_lock, flags);
	ib->usage_cnt = pd->if_used;
	if (!pd->log_head)
		pd->log_head = ib;	
	else
		pd->log_tail->next = ib;	
	pd->log_tail = ib;	
	i = pd->del_lock++;	
	spin_unlock_irqrestore(&card->hysdn_lock, flags);

	
	if (!i)
		while (pd->log_head->next) {
			if ((pd->log_head->usage_cnt <= 0) &&
			    (pd->log_head->next->usage_cnt <= 0)) {
				ib = pd->log_head;
				pd->log_head = pd->log_head->next;
				kfree(ib);
			} else
				break;
		}		
	pd->del_lock--;		
	wake_up_interruptible(&(pd->rd_queue));		
}				



static ssize_t
hysdn_log_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
	int rc;
	unsigned char valbuf[128];
	hysdn_card *card = file->private_data;

	if (count > (sizeof(valbuf) - 1))
		count = sizeof(valbuf) - 1;	
	if (copy_from_user(valbuf, buf, count))
		return (-EFAULT);	

	valbuf[count] = 0;	

	rc = kstrtoul(valbuf, 0, &card->debug_flags);
	if (rc < 0)
		return rc;
	hysdn_addlog(card, "debug set to 0x%lx", card->debug_flags);
	return (count);
}				

static ssize_t
hysdn_log_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
	struct log_data *inf;
	int len;
	struct proc_dir_entry *pde = PDE(file->f_path.dentry->d_inode);
	struct procdata *pd = NULL;
	hysdn_card *card;

	if (!*((struct log_data **) file->private_data)) {
		if (file->f_flags & O_NONBLOCK)
			return (-EAGAIN);

		
		card = card_root;
		while (card) {
			pd = card->proclog;
			if (pd->log == pde)
				break;
			card = card->next;	
		}
		if (card)
			interruptible_sleep_on(&(pd->rd_queue));
		else
			return (-EAGAIN);

	}
	if (!(inf = *((struct log_data **) file->private_data)))
		return (0);

	inf->usage_cnt--;	
	file->private_data = &inf->next;	
	if ((len = strlen(inf->log_start)) <= count) {
		if (copy_to_user(buf, inf->log_start, len))
			return -EFAULT;
		*off += len;
		return (len);
	}
	return (0);
}				

static int
hysdn_log_open(struct inode *ino, struct file *filep)
{
	hysdn_card *card;
	struct procdata *pd = NULL;
	unsigned long flags;

	mutex_lock(&hysdn_log_mutex);
	card = card_root;
	while (card) {
		pd = card->proclog;
		if (pd->log == PDE(ino))
			break;
		card = card->next;	
	}
	if (!card) {
		mutex_unlock(&hysdn_log_mutex);
		return (-ENODEV);	
	}
	filep->private_data = card;	

	if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_WRITE) {
		
	} else if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_READ) {

		
		spin_lock_irqsave(&card->hysdn_lock, flags);
		pd->if_used++;
		if (pd->log_head)
			filep->private_data = &pd->log_tail->next;
		else
			filep->private_data = &pd->log_head;
		spin_unlock_irqrestore(&card->hysdn_lock, flags);
	} else {		
		mutex_unlock(&hysdn_log_mutex);
		return (-EPERM);	
	}
	mutex_unlock(&hysdn_log_mutex);
	return nonseekable_open(ino, filep);
}				

static int
hysdn_log_close(struct inode *ino, struct file *filep)
{
	struct log_data *inf;
	struct procdata *pd;
	hysdn_card *card;
	int retval = 0;

	mutex_lock(&hysdn_log_mutex);
	if ((filep->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_WRITE) {
		/* write only access -> write debug level written */
		retval = 0;	
	} else {
		

		pd = NULL;
		inf = *((struct log_data **) filep->private_data);	
		if (inf)
			pd = (struct procdata *) inf->proc_ctrl;	
		else {
			
			card = card_root;
			while (card) {
				pd = card->proclog;
				if (pd->log == PDE(ino))
					break;
				card = card->next;	
			}
			if (card)
				pd = card->proclog;	
		}
		if (pd)
			pd->if_used--;	

		while (inf) {
			inf->usage_cnt--;	
			inf = inf->next;
		}

		if (pd)
			if (pd->if_used <= 0)	
				while (pd->log_head) {
					inf = pd->log_head;
					pd->log_head = pd->log_head->next;
					kfree(inf);
				}
	}			
	mutex_unlock(&hysdn_log_mutex);

	return (retval);
}				

static unsigned int
hysdn_log_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct proc_dir_entry *pde = PDE(file->f_path.dentry->d_inode);
	hysdn_card *card;
	struct procdata *pd = NULL;

	if ((file->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_WRITE)
		return (mask);	

	
	card = card_root;
	while (card) {
		pd = card->proclog;
		if (pd->log == pde)
			break;
		card = card->next;	
	}
	if (!card)
		return (mask);	

	poll_wait(file, &(pd->rd_queue), wait);

	if (*((struct log_data **) file->private_data))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}				

static const struct file_operations log_fops =
{
	.owner		= THIS_MODULE,
	.llseek         = no_llseek,
	.read           = hysdn_log_read,
	.write          = hysdn_log_write,
	.poll           = hysdn_log_poll,
	.open           = hysdn_log_open,
	.release        = hysdn_log_close,
};


int
hysdn_proclog_init(hysdn_card *card)
{
	struct procdata *pd;

	

	if ((pd = kzalloc(sizeof(struct procdata), GFP_KERNEL)) != NULL) {
		sprintf(pd->log_name, "%s%d", PROC_LOG_BASENAME, card->myid);
		pd->log = proc_create(pd->log_name,
				      S_IFREG | S_IRUGO | S_IWUSR, hysdn_proc_entry,
				      &log_fops);

		init_waitqueue_head(&(pd->rd_queue));

		card->proclog = (void *) pd;	
	}
	return (0);
}				

void
hysdn_proclog_release(hysdn_card *card)
{
	struct procdata *pd;

	if ((pd = (struct procdata *) card->proclog) != NULL) {
		if (pd->log)
			remove_proc_entry(pd->log_name, hysdn_proc_entry);
		kfree(pd);	
		card->proclog = NULL;
	}
}				
