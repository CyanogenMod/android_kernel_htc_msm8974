/*********************************************************************
 *
 * Filename:      discovery.c
 * Version:       0.1
 * Description:   Routines for handling discoveries at the IrLMP layer
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Apr  6 15:33:50 1999
 * Modified at:   Sat Oct  9 17:11:31 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Modified at:   Fri May 28  3:11 CST 1999
 * Modified by:   Horst von Brand <vonbrand@sleipnir.valparaiso.cl>
 *
 *     Copyright (c) 1999 Dag Brattli, All Rights Reserved.
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#include <linux/string.h>
#include <linux/socket.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/export.h>

#include <net/irda/irda.h>
#include <net/irda/irlmp.h>

#include <net/irda/discovery.h>

#include <asm/unaligned.h>

void irlmp_add_discovery(hashbin_t *cachelog, discovery_t *new)
{
	discovery_t *discovery, *node;
	unsigned long flags;

	
	new->firststamp = new->timestamp;

	spin_lock_irqsave(&cachelog->hb_spinlock, flags);

	discovery = (discovery_t *) hashbin_get_first(cachelog);
	while (discovery != NULL ) {
		node = discovery;

		
		discovery = (discovery_t *) hashbin_get_next(cachelog);

		if ((node->data.saddr == new->data.saddr) &&
		    ((node->data.daddr == new->data.daddr) ||
		     (strcmp(node->data.info, new->data.info) == 0)))
		{
			hashbin_remove_this(cachelog, (irda_queue_t *) node);
			
			if (get_unaligned((__u16 *)node->data.hints) == get_unaligned((__u16 *)new->data.hints))
				
				new->firststamp = node->firststamp;
			kfree(node);
		}
	}

	
	hashbin_insert(cachelog, (irda_queue_t *) new, new->data.daddr, NULL);

	spin_unlock_irqrestore(&cachelog->hb_spinlock, flags);
}

void irlmp_add_discovery_log(hashbin_t *cachelog, hashbin_t *log)
{
	discovery_t *discovery;

	IRDA_DEBUG(4, "%s()\n", __func__);

	
	if (log == NULL) {
		
		return;
	}

	discovery = (discovery_t *) hashbin_remove_first(log);
	while (discovery != NULL) {
		irlmp_add_discovery(cachelog, discovery);

		discovery = (discovery_t *) hashbin_remove_first(log);
	}

	
	hashbin_delete(log, (FREE_FUNC) kfree);
}

void irlmp_expire_discoveries(hashbin_t *log, __u32 saddr, int force)
{
	discovery_t *		discovery;
	discovery_t *		curr;
	unsigned long		flags;
	discinfo_t *		buffer = NULL;
	int			n;		
	int			i = 0;		

	IRDA_ASSERT(log != NULL, return;);
	IRDA_DEBUG(4, "%s()\n", __func__);

	spin_lock_irqsave(&log->hb_spinlock, flags);

	discovery = (discovery_t *) hashbin_get_first(log);
	while (discovery != NULL) {
		
		curr = discovery;
		discovery = (discovery_t *) hashbin_get_next(log);

		
		if ((curr->data.saddr == saddr) &&
		    (force ||
		     ((jiffies - curr->timestamp) > DISCOVERY_EXPIRE_TIMEOUT)))
		{
			if(buffer == NULL) {
				
				n = HASHBIN_GET_SIZE(log);
				buffer = kmalloc(n * sizeof(struct irda_device_info), GFP_ATOMIC);
				if (buffer == NULL) {
					spin_unlock_irqrestore(&log->hb_spinlock, flags);
					return;
				}

			}

			
			memcpy(&(buffer[i]), &(curr->data),
			       sizeof(discinfo_t));
			i++;

			
			curr = hashbin_remove_this(log, (irda_queue_t *) curr);
			kfree(curr);
		}
	}

	spin_unlock_irqrestore(&log->hb_spinlock, flags);

	if(buffer == NULL)
		return;

	
	irlmp_discovery_expiry(buffer, i);

	
	kfree(buffer);
}

#if 0
void irlmp_dump_discoveries(hashbin_t *log)
{
	discovery_t *discovery;

	IRDA_ASSERT(log != NULL, return;);

	discovery = (discovery_t *) hashbin_get_first(log);
	while (discovery != NULL) {
		IRDA_DEBUG(0, "Discovery:\n");
		IRDA_DEBUG(0, "  daddr=%08x\n", discovery->data.daddr);
		IRDA_DEBUG(0, "  saddr=%08x\n", discovery->data.saddr);
		IRDA_DEBUG(0, "  nickname=%s\n", discovery->data.info);

		discovery = (discovery_t *) hashbin_get_next(log);
	}
}
#endif

struct irda_device_info *irlmp_copy_discoveries(hashbin_t *log, int *pn,
						__u16 mask, int old_entries)
{
	discovery_t *		discovery;
	unsigned long		flags;
	discinfo_t *		buffer = NULL;
	int			j_timeout = (sysctl_discovery_timeout * HZ);
	int			n;		
	int			i = 0;		

	IRDA_ASSERT(pn != NULL, return NULL;);
	IRDA_ASSERT(log != NULL, return NULL;);

	
	spin_lock_irqsave(&log->hb_spinlock, flags);

	discovery = (discovery_t *) hashbin_get_first(log);
	while (discovery != NULL) {
		if ((get_unaligned((__u16 *)discovery->data.hints) & mask) &&
		    ((old_entries) ||
		     ((jiffies - discovery->firststamp) < j_timeout))) {
			if(buffer == NULL) {
				
				n = HASHBIN_GET_SIZE(log);
				buffer = kmalloc(n * sizeof(struct irda_device_info), GFP_ATOMIC);
				if (buffer == NULL) {
					spin_unlock_irqrestore(&log->hb_spinlock, flags);
					return NULL;
				}

			}

			
			memcpy(&(buffer[i]), &(discovery->data),
			       sizeof(discinfo_t));
			i++;
		}
		discovery = (discovery_t *) hashbin_get_next(log);
	}

	spin_unlock_irqrestore(&log->hb_spinlock, flags);

	
	*pn = i;
	return buffer;
}

#ifdef CONFIG_PROC_FS
static inline discovery_t *discovery_seq_idx(loff_t pos)

{
	discovery_t *discovery;

	for (discovery = (discovery_t *) hashbin_get_first(irlmp->cachelog);
	     discovery != NULL;
	     discovery = (discovery_t *) hashbin_get_next(irlmp->cachelog)) {
		if (pos-- == 0)
			break;
	}

	return discovery;
}

static void *discovery_seq_start(struct seq_file *seq, loff_t *pos)
{
	spin_lock_irq(&irlmp->cachelog->hb_spinlock);
	return *pos ? discovery_seq_idx(*pos - 1) : SEQ_START_TOKEN;
}

static void *discovery_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	++*pos;
	return (v == SEQ_START_TOKEN)
		? (void *) hashbin_get_first(irlmp->cachelog)
		: (void *) hashbin_get_next(irlmp->cachelog);
}

static void discovery_seq_stop(struct seq_file *seq, void *v)
{
	spin_unlock_irq(&irlmp->cachelog->hb_spinlock);
}

static int discovery_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN)
		seq_puts(seq, "IrLMP: Discovery log:\n\n");
	else {
		const discovery_t *discovery = v;

		seq_printf(seq, "nickname: %s, hint: 0x%02x%02x",
			   discovery->data.info,
			   discovery->data.hints[0],
			   discovery->data.hints[1]);
#if 0
		if ( discovery->data.hints[0] & HINT_PNP)
			seq_puts(seq, "PnP Compatible ");
		if ( discovery->data.hints[0] & HINT_PDA)
			seq_puts(seq, "PDA/Palmtop ");
		if ( discovery->data.hints[0] & HINT_COMPUTER)
			seq_puts(seq, "Computer ");
		if ( discovery->data.hints[0] & HINT_PRINTER)
			seq_puts(seq, "Printer ");
		if ( discovery->data.hints[0] & HINT_MODEM)
			seq_puts(seq, "Modem ");
		if ( discovery->data.hints[0] & HINT_FAX)
			seq_puts(seq, "Fax ");
		if ( discovery->data.hints[0] & HINT_LAN)
			seq_puts(seq, "LAN Access ");

		if ( discovery->data.hints[1] & HINT_TELEPHONY)
			seq_puts(seq, "Telephony ");
		if ( discovery->data.hints[1] & HINT_FILE_SERVER)
			seq_puts(seq, "File Server ");
		if ( discovery->data.hints[1] & HINT_COMM)
			seq_puts(seq, "IrCOMM ");
		if ( discovery->data.hints[1] & HINT_OBEX)
			seq_puts(seq, "IrOBEX ");
#endif
		seq_printf(seq,", saddr: 0x%08x, daddr: 0x%08x\n\n",
			       discovery->data.saddr,
			       discovery->data.daddr);

		seq_putc(seq, '\n');
	}
	return 0;
}

static const struct seq_operations discovery_seq_ops = {
	.start  = discovery_seq_start,
	.next   = discovery_seq_next,
	.stop   = discovery_seq_stop,
	.show   = discovery_seq_show,
};

static int discovery_seq_open(struct inode *inode, struct file *file)
{
	IRDA_ASSERT(irlmp != NULL, return -EINVAL;);

	return seq_open(file, &discovery_seq_ops);
}

const struct file_operations discovery_seq_fops = {
	.owner		= THIS_MODULE,
	.open           = discovery_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= seq_release,
};
#endif
