/*
 * linux/kernel/dma.c: A DMA channel allocator. Inspired by linux/kernel/irq.c.
 *
 * Written by Hennus Bergman, 1992.
 *
 * 1994/12/26: Changes by Alex Nash to fix a minor bug in /proc/dma.
 *   In the previous version the reported device could end up being wrong,
 *   if a device requested a DMA channel that was already in use.
 *   [It also happened to remove the sizeof(char *) == sizeof(int)
 *   assumption introduced because of those /proc/dma patches. -- Hennus]
 */
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <asm/dma.h>





DEFINE_SPINLOCK(dma_spin_lock);


#ifdef MAX_DMA_CHANNELS



struct dma_chan {
	int  lock;
	const char *device_id;
};

static struct dma_chan dma_chan_busy[MAX_DMA_CHANNELS] = {
	[4] = { 1, "cascade" },
};


int request_dma(unsigned int dmanr, const char * device_id)
{
	if (dmanr >= MAX_DMA_CHANNELS)
		return -EINVAL;

	if (xchg(&dma_chan_busy[dmanr].lock, 1) != 0)
		return -EBUSY;

	dma_chan_busy[dmanr].device_id = device_id;

	
	return 0;
} 

void free_dma(unsigned int dmanr)
{
	if (dmanr >= MAX_DMA_CHANNELS) {
		printk(KERN_WARNING "Trying to free DMA%d\n", dmanr);
		return;
	}

	if (xchg(&dma_chan_busy[dmanr].lock, 0) == 0) {
		printk(KERN_WARNING "Trying to free free DMA%d\n", dmanr);
		return;
	}

} 

#else

int request_dma(unsigned int dmanr, const char *device_id)
{
	return -EINVAL;
}

void free_dma(unsigned int dmanr)
{
}

#endif

#ifdef CONFIG_PROC_FS

#ifdef MAX_DMA_CHANNELS
static int proc_dma_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0 ; i < MAX_DMA_CHANNELS ; i++) {
		if (dma_chan_busy[i].lock) {
			seq_printf(m, "%2d: %s\n", i,
				   dma_chan_busy[i].device_id);
		}
	}
	return 0;
}
#else
static int proc_dma_show(struct seq_file *m, void *v)
{
	seq_puts(m, "No DMA\n");
	return 0;
}
#endif 

static int proc_dma_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dma_show, NULL);
}

static const struct file_operations proc_dma_operations = {
	.open		= proc_dma_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_dma_init(void)
{
	proc_create("dma", 0, NULL, &proc_dma_operations);
	return 0;
}

__initcall(proc_dma_init);
#endif

EXPORT_SYMBOL(request_dma);
EXPORT_SYMBOL(free_dma);
EXPORT_SYMBOL(dma_spin_lock);
