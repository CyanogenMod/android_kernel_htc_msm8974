/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Stefan Nilsson <stefan.xk.nilsson@stericsson.com> for ST-Ericsson.
 * Author: Martin Persson <martin.persson@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL), version 2.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/completion.h>
#include <mach/mbox-db5500.h>

#define MBOX_NAME "mbox"

#define MBOX_FIFO_DATA        0x000
#define MBOX_FIFO_ADD         0x004
#define MBOX_FIFO_REMOVE      0x008
#define MBOX_FIFO_THRES_FREE  0x00C
#define MBOX_FIFO_THRES_OCCUP 0x010
#define MBOX_FIFO_STATUS      0x014

#define MBOX_DISABLE_IRQ 0x4
#define MBOX_ENABLE_IRQ  0x0
#define MBOX_LATCH 1

static struct list_head mboxs = LIST_HEAD_INIT(mboxs);

static struct mbox *get_mbox_with_id(u8 id)
{
	u8 i;
	struct list_head *pos = &mboxs;
	for (i = 0; i <= id; i++)
		pos = pos->next;

	return (struct mbox *) list_entry(pos, struct mbox, list);
}

int mbox_send(struct mbox *mbox, u32 mbox_msg, bool block)
{
	int res = 0;

	spin_lock(&mbox->lock);

	dev_dbg(&(mbox->pdev->dev),
		"About to buffer 0x%X to mailbox 0x%X."
		" ri = %d, wi = %d\n",
		mbox_msg, (u32)mbox, mbox->read_index,
		mbox->write_index);

	
	while (((mbox->write_index + 1) % MBOX_BUF_SIZE) == mbox->read_index) {
		if (!block) {
			dev_dbg(&(mbox->pdev->dev),
			"Buffer full in non-blocking call! "
			"Returning -ENOMEM!\n");
			res = -ENOMEM;
			goto exit;
		}
		spin_unlock(&mbox->lock);
		dev_dbg(&(mbox->pdev->dev),
			"Buffer full in blocking call! Sleeping...\n");
		mbox->client_blocked = 1;
		wait_for_completion(&mbox->buffer_available);
		dev_dbg(&(mbox->pdev->dev),
			"Blocking send was woken up! Trying again...\n");
		spin_lock(&mbox->lock);
	}

	mbox->buffer[mbox->write_index] = mbox_msg;
	mbox->write_index = (mbox->write_index + 1) % MBOX_BUF_SIZE;

	writel(MBOX_ENABLE_IRQ, mbox->virtbase_peer + MBOX_FIFO_THRES_FREE);

exit:
	spin_unlock(&mbox->lock);
	return res;
}
EXPORT_SYMBOL(mbox_send);

#if defined(CONFIG_DEBUG_FS)
static ssize_t mbox_write_fifo(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	unsigned long mbox_mess;
	unsigned long nbr_sends;
	unsigned long i;
	char int_buf[16];
	char *token;
	char *val;

	struct mbox *mbox = (struct mbox *) dev->platform_data;

	strncpy((char *) &int_buf, buf, sizeof(int_buf));
	token = (char *) &int_buf;

	
	val = strsep(&token, " ");
	if ((val == NULL) || (strict_strtoul(val, 16, &mbox_mess) != 0))
		mbox_mess = 0xDEADBEEF;

	val = strsep(&token, " ");
	if ((val == NULL) || (strict_strtoul(val, 10, &nbr_sends) != 0))
		nbr_sends = 1;

	dev_dbg(dev, "Will write 0x%lX %ld times using data struct at 0x%X\n",
		mbox_mess, nbr_sends, (u32) mbox);

	for (i = 0; i < nbr_sends; i++)
		mbox_send(mbox, mbox_mess, true);

	return count;
}

static ssize_t mbox_read_fifo(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int mbox_value;
	struct mbox *mbox = (struct mbox *) dev->platform_data;

	if ((readl(mbox->virtbase_local + MBOX_FIFO_STATUS) & 0x7) <= 0)
		return sprintf(buf, "Mailbox is empty\n");

	mbox_value = readl(mbox->virtbase_local + MBOX_FIFO_DATA);
	writel(MBOX_LATCH, (mbox->virtbase_local + MBOX_FIFO_REMOVE));

	return sprintf(buf, "0x%X\n", mbox_value);
}

static DEVICE_ATTR(fifo, S_IWUSR | S_IRUGO, mbox_read_fifo, mbox_write_fifo);

static int mbox_show(struct seq_file *s, void *data)
{
	struct list_head *pos;
	u8 mbox_index = 0;

	list_for_each(pos, &mboxs) {
		struct mbox *m =
			(struct mbox *) list_entry(pos, struct mbox, list);
		if (m == NULL) {
			seq_printf(s,
				   "Unable to retrieve mailbox %d\n",
				   mbox_index);
			continue;
		}

		spin_lock(&m->lock);
		if ((m->virtbase_peer == NULL) || (m->virtbase_local == NULL)) {
			seq_printf(s, "MAILBOX %d not setup or corrupt\n",
				   mbox_index);
			spin_unlock(&m->lock);
			continue;
		}

		seq_printf(s,
		"===========================\n"
		" MAILBOX %d\n"
		" PEER MAILBOX DUMP\n"
		"---------------------------\n"
		"FIFO:                 0x%X (%d)\n"
		"Free     Threshold:   0x%.2X (%d)\n"
		"Occupied Threshold:   0x%.2X (%d)\n"
		"Status:               0x%.2X (%d)\n"
		"   Free spaces  (ot):    %d (%d)\n"
		"   Occup spaces (ot):    %d (%d)\n"
		"===========================\n"
		" LOCAL MAILBOX DUMP\n"
		"---------------------------\n"
		"FIFO:                 0x%.X (%d)\n"
		"Free     Threshold:   0x%.2X (%d)\n"
		"Occupied Threshold:   0x%.2X (%d)\n"
		"Status:               0x%.2X (%d)\n"
		"   Free spaces  (ot):    %d (%d)\n"
		"   Occup spaces (ot):    %d (%d)\n"
		"===========================\n"
		"write_index: %d\n"
		"read_index : %d\n"
		"===========================\n"
		"\n",
		mbox_index,
		readl(m->virtbase_peer + MBOX_FIFO_DATA),
		readl(m->virtbase_peer + MBOX_FIFO_DATA),
		readl(m->virtbase_peer + MBOX_FIFO_THRES_FREE),
		readl(m->virtbase_peer + MBOX_FIFO_THRES_FREE),
		readl(m->virtbase_peer + MBOX_FIFO_THRES_OCCUP),
		readl(m->virtbase_peer + MBOX_FIFO_THRES_OCCUP),
		readl(m->virtbase_peer + MBOX_FIFO_STATUS),
		readl(m->virtbase_peer + MBOX_FIFO_STATUS),
		(readl(m->virtbase_peer + MBOX_FIFO_STATUS) >> 4) & 0x7,
		(readl(m->virtbase_peer + MBOX_FIFO_STATUS) >> 7) & 0x1,
		(readl(m->virtbase_peer + MBOX_FIFO_STATUS) >> 0) & 0x7,
		(readl(m->virtbase_peer + MBOX_FIFO_STATUS) >> 3) & 0x1,
		readl(m->virtbase_local + MBOX_FIFO_DATA),
		readl(m->virtbase_local + MBOX_FIFO_DATA),
		readl(m->virtbase_local + MBOX_FIFO_THRES_FREE),
		readl(m->virtbase_local + MBOX_FIFO_THRES_FREE),
		readl(m->virtbase_local + MBOX_FIFO_THRES_OCCUP),
		readl(m->virtbase_local + MBOX_FIFO_THRES_OCCUP),
		readl(m->virtbase_local + MBOX_FIFO_STATUS),
		readl(m->virtbase_local + MBOX_FIFO_STATUS),
		(readl(m->virtbase_local + MBOX_FIFO_STATUS) >> 4) & 0x7,
		(readl(m->virtbase_local + MBOX_FIFO_STATUS) >> 7) & 0x1,
		(readl(m->virtbase_local + MBOX_FIFO_STATUS) >> 0) & 0x7,
		(readl(m->virtbase_local + MBOX_FIFO_STATUS) >> 3) & 0x1,
		m->write_index, m->read_index);
		mbox_index++;
		spin_unlock(&m->lock);
	}

	return 0;
}

static int mbox_open(struct inode *inode, struct file *file)
{
	return single_open(file, mbox_show, NULL);
}

static const struct file_operations mbox_operations = {
	.owner = THIS_MODULE,
	.open = mbox_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static irqreturn_t mbox_irq(int irq, void *arg)
{
	u32 mbox_value;
	int nbr_occup;
	int nbr_free;
	struct mbox *mbox = (struct mbox *) arg;

	spin_lock(&mbox->lock);

	dev_dbg(&(mbox->pdev->dev),
		"mbox IRQ [%d] received. ri = %d, wi = %d\n",
		irq, mbox->read_index, mbox->write_index);

	if (mbox->read_index != mbox->write_index) {
		nbr_free = (readl(mbox->virtbase_local + MBOX_FIFO_STATUS)
			    >> 4) & 0x7;
		dev_dbg(&(mbox->pdev->dev),
			"Status indicates %d empty spaces in the FIFO!\n",
			nbr_free);

		while ((nbr_free > 0) &&
		       (mbox->read_index != mbox->write_index)) {
			
			writel(mbox->buffer[mbox->read_index],
			       (mbox->virtbase_peer + MBOX_FIFO_DATA));
			writel(MBOX_LATCH,
			       (mbox->virtbase_peer + MBOX_FIFO_ADD));
			dev_dbg(&(mbox->pdev->dev),
				"Wrote message 0x%X to addr 0x%X\n",
				mbox->buffer[mbox->read_index],
				(u32) (mbox->virtbase_peer + MBOX_FIFO_DATA));

			nbr_free--;
			mbox->read_index =
				(mbox->read_index + 1) % MBOX_BUF_SIZE;
		}

		if (mbox->read_index != mbox->write_index) {
			dev_dbg(&(mbox->pdev->dev),
				"Still have messages to send, but FIFO full. "
				"Request IRQ again!\n");
			writel(MBOX_ENABLE_IRQ,
			       mbox->virtbase_peer + MBOX_FIFO_THRES_FREE);
		} else {
			dev_dbg(&(mbox->pdev->dev),
				"No more messages to send. "
				"Do not request IRQ again!\n");
			writel(MBOX_DISABLE_IRQ,
			       mbox->virtbase_peer + MBOX_FIFO_THRES_FREE);
		}

		if (mbox->client_blocked &&
		    (((mbox->write_index + 1) % MBOX_BUF_SIZE)
		     != mbox->read_index)) {
			dev_dbg(&(mbox->pdev->dev),
				"Waking up blocked client\n");
			complete(&mbox->buffer_available);
			mbox->client_blocked = 0;
		}
	}

	
	nbr_occup = readl(mbox->virtbase_local + MBOX_FIFO_STATUS) & 0x7;
	if (nbr_occup == 0)
		goto exit;

	if (mbox->cb == NULL) {
		dev_dbg(&(mbox->pdev->dev), "No receive callback registered, "
			"leaving %d incoming messages in fifo!\n", nbr_occup);
		goto exit;
	}

	
	mbox_value = readl(mbox->virtbase_local + MBOX_FIFO_DATA);
	writel(MBOX_LATCH, (mbox->virtbase_local + MBOX_FIFO_REMOVE));

	
	dev_dbg(&(mbox->pdev->dev), "Calling callback for message 0x%X!\n",
		mbox_value);
	mbox->cb(mbox_value, mbox->client_data);

exit:
	dev_dbg(&(mbox->pdev->dev), "Exit mbox IRQ. ri = %d, wi = %d\n",
		mbox->read_index, mbox->write_index);
	spin_unlock(&mbox->lock);

	return IRQ_HANDLED;
}

struct mbox *mbox_setup(u8 mbox_id, mbox_recv_cb_t *mbox_cb, void *priv)
{
	struct resource *resource;
	int irq;
	int res;
	struct mbox *mbox;

	mbox = get_mbox_with_id(mbox_id);
	if (mbox == NULL) {
		dev_err(&(mbox->pdev->dev), "Incorrect mailbox id: %d!\n",
			mbox_id);
		goto exit;
	}

	if (mbox->allocated) {
		dev_err(&(mbox->pdev->dev), "Mailbox number %d is busy!\n",
			mbox_id);
		mbox = NULL;
		goto exit;
	}
	mbox->allocated = true;

	dev_dbg(&(mbox->pdev->dev), "Initiating mailbox number %d: 0x%X...\n",
		mbox_id, (u32)mbox);

	mbox->client_data = priv;
	mbox->cb = mbox_cb;

	
	resource = platform_get_resource_byname(mbox->pdev,
						IORESOURCE_MEM,
						"mbox_peer");
	if (resource == NULL) {
		dev_err(&(mbox->pdev->dev),
			"Unable to retrieve mbox peer resource\n");
		mbox = NULL;
		goto exit;
	}
	dev_dbg(&(mbox->pdev->dev),
		"Resource name: %s start: 0x%X, end: 0x%X\n",
		resource->name, resource->start, resource->end);
	mbox->virtbase_peer = ioremap(resource->start, resource_size(resource));
	if (!mbox->virtbase_peer) {
		dev_err(&(mbox->pdev->dev), "Unable to ioremap peer mbox\n");
		mbox = NULL;
		goto exit;
	}
	dev_dbg(&(mbox->pdev->dev),
		"ioremapped peer physical: (0x%X-0x%X) to virtual: 0x%X\n",
		resource->start, resource->end, (u32) mbox->virtbase_peer);

	
	resource = platform_get_resource_byname(mbox->pdev,
						IORESOURCE_MEM,
						"mbox_local");
	if (resource == NULL) {
		dev_err(&(mbox->pdev->dev),
			"Unable to retrieve mbox local resource\n");
		mbox = NULL;
		goto exit;
	}
	dev_dbg(&(mbox->pdev->dev),
		"Resource name: %s start: 0x%X, end: 0x%X\n",
		resource->name, resource->start, resource->end);
	mbox->virtbase_local = ioremap(resource->start, resource_size(resource));
	if (!mbox->virtbase_local) {
		dev_err(&(mbox->pdev->dev), "Unable to ioremap local mbox\n");
		mbox = NULL;
		goto exit;
	}
	dev_dbg(&(mbox->pdev->dev),
		"ioremapped local physical: (0x%X-0x%X) to virtual: 0x%X\n",
		resource->start, resource->end, (u32) mbox->virtbase_peer);

	init_completion(&mbox->buffer_available);
	mbox->client_blocked = 0;

	
	irq = platform_get_irq_byname(mbox->pdev, "mbox_irq");
	if (irq < 0) {
		dev_err(&(mbox->pdev->dev),
			"Unable to retrieve mbox irq resource\n");
		mbox = NULL;
		goto exit;
	}

	dev_dbg(&(mbox->pdev->dev), "Allocating irq %d...\n", irq);
	res = request_irq(irq, mbox_irq, 0, mbox->name, (void *) mbox);
	if (res < 0) {
		dev_err(&(mbox->pdev->dev),
			"Unable to allocate mbox irq %d\n", irq);
		mbox = NULL;
		goto exit;
	}

	
	writel(MBOX_DISABLE_IRQ, mbox->virtbase_peer + MBOX_FIFO_THRES_FREE);

	if (mbox_cb != NULL)
		writel(MBOX_ENABLE_IRQ,
		       mbox->virtbase_local + MBOX_FIFO_THRES_OCCUP);
	else
		writel(MBOX_DISABLE_IRQ,
		       mbox->virtbase_local + MBOX_FIFO_THRES_OCCUP);

#if defined(CONFIG_DEBUG_FS)
	res = device_create_file(&(mbox->pdev->dev), &dev_attr_fifo);
	if (res != 0)
		dev_warn(&(mbox->pdev->dev),
			 "Unable to create mbox sysfs entry");

	(void) debugfs_create_file("mbox", S_IFREG | S_IRUGO, NULL,
				   NULL, &mbox_operations);
#endif

	dev_info(&(mbox->pdev->dev),
		 "Mailbox driver with index %d initiated!\n", mbox_id);

exit:
	return mbox;
}
EXPORT_SYMBOL(mbox_setup);


int __init mbox_probe(struct platform_device *pdev)
{
	struct mbox local_mbox;
	struct mbox *mbox;
	int res = 0;
	dev_dbg(&(pdev->dev), "Probing mailbox (pdev = 0x%X)...\n", (u32) pdev);

	memset(&local_mbox, 0x0, sizeof(struct mbox));

	
	res = platform_device_add_data(pdev,
				       (void *) &local_mbox,
				       sizeof(struct mbox));
	if (res != 0) {
		dev_err(&(pdev->dev),
			"Unable to allocate driver platform data!\n");
		goto exit;
	}

	mbox = (struct mbox *) pdev->dev.platform_data;
	mbox->pdev = pdev;
	mbox->write_index = 0;
	mbox->read_index = 0;

	INIT_LIST_HEAD(&(mbox->list));
	list_add_tail(&(mbox->list), &mboxs);

	sprintf(mbox->name, "%s", MBOX_NAME);
	spin_lock_init(&mbox->lock);

	dev_info(&(pdev->dev), "Mailbox driver loaded\n");

exit:
	return res;
}

static struct platform_driver mbox_driver = {
	.driver = {
		.name = MBOX_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init mbox_init(void)
{
	return platform_driver_probe(&mbox_driver, mbox_probe);
}

module_init(mbox_init);

void __exit mbox_exit(void)
{
	platform_driver_unregister(&mbox_driver);
}

module_exit(mbox_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MBOX driver");
