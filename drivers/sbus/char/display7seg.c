/* display7seg.c - Driver implementation for the 7-segment display
 *                 present on Sun Microsystems CP1400 and CP1500
 *
 * Copyright (c) 2000 Eric Brower (ebrower@usa.net)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>		
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/atomic.h>
#include <asm/uaccess.h>		
#include <asm/io.h>

#include <asm/display7seg.h>

#define D7S_MINOR	193
#define DRIVER_NAME	"d7s"
#define PFX		DRIVER_NAME ": "

static DEFINE_MUTEX(d7s_mutex);
static int sol_compat = 0;		

module_param(sol_compat, int, 0);
MODULE_PARM_DESC(sol_compat, 
		 "Disables documented functionality omitted from Solaris driver");

MODULE_AUTHOR("Eric Brower <ebrower@usa.net>");
MODULE_DESCRIPTION("7-Segment Display driver for Sun Microsystems CP1400/1500");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("d7s");

struct d7s {
	void __iomem	*regs;
	bool		flipped;
};
struct d7s *d7s_device;

static atomic_t d7s_users = ATOMIC_INIT(0);

static int d7s_open(struct inode *inode, struct file *f)
{
	if (D7S_MINOR != iminor(inode))
		return -ENODEV;
	atomic_inc(&d7s_users);
	return 0;
}

static int d7s_release(struct inode *inode, struct file *f)
{
	if (atomic_dec_and_test(&d7s_users) && !sol_compat) {
		struct d7s *p = d7s_device;
		u8 regval = 0;

		regval = readb(p->regs);
		if (p->flipped)
			regval |= D7S_FLIP;
		else
			regval &= ~D7S_FLIP;
		writeb(regval, p->regs);
	}

	return 0;
}

static long d7s_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct d7s *p = d7s_device;
	u8 regs = readb(p->regs);
	int error = 0;
	u8 ireg = 0;

	if (D7S_MINOR != iminor(file->f_path.dentry->d_inode))
		return -ENODEV;

	mutex_lock(&d7s_mutex);
	switch (cmd) {
	case D7SIOCWR:
		if (get_user(ireg, (int __user *) arg)) {
			error = -EFAULT;
			break;
		}
		if (sol_compat) {
			if (regs & D7S_FLIP)
				ireg |= D7S_FLIP;
			else
				ireg &= ~D7S_FLIP;
		}
		writeb(ireg, p->regs);
		break;

	case D7SIOCRD:
		if (put_user(regs, (int __user *) arg)) {
			error = -EFAULT;
			break;
		}
		break;

	case D7SIOCTM:
		
		if (regs & D7S_FLIP)
			regs &= ~D7S_FLIP;
		else
			regs |= D7S_FLIP;
		writeb(regs, p->regs);
		break;
	};
	mutex_unlock(&d7s_mutex);

	return error;
}

static const struct file_operations d7s_fops = {
	.owner =		THIS_MODULE,
	.unlocked_ioctl =	d7s_ioctl,
	.compat_ioctl =		d7s_ioctl,
	.open =			d7s_open,
	.release =		d7s_release,
	.llseek = noop_llseek,
};

static struct miscdevice d7s_miscdev = {
	.minor		= D7S_MINOR,
	.name		= DRIVER_NAME,
	.fops		= &d7s_fops
};

static int __devinit d7s_probe(struct platform_device *op)
{
	struct device_node *opts;
	int err = -EINVAL;
	struct d7s *p;
	u8 regs;

	if (d7s_device)
		goto out;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	err = -ENOMEM;
	if (!p)
		goto out;

	p->regs = of_ioremap(&op->resource[0], 0, sizeof(u8), "d7s");
	if (!p->regs) {
		printk(KERN_ERR PFX "Cannot map chip registers\n");
		goto out_free;
	}

	err = misc_register(&d7s_miscdev);
	if (err) {
		printk(KERN_ERR PFX "Unable to acquire miscdevice minor %i\n",
		       D7S_MINOR);
		goto out_iounmap;
	}

	regs = readb(p->regs);
	opts = of_find_node_by_path("/options");
	if (opts &&
	    of_get_property(opts, "d7s-flipped?", NULL))
		p->flipped = true;

	if (p->flipped)
		regs |= D7S_FLIP;
	else
		regs &= ~D7S_FLIP;

	writeb(regs,  p->regs);

	printk(KERN_INFO PFX "7-Segment Display%s at [%s:0x%llx] %s\n",
	       op->dev.of_node->full_name,
	       (regs & D7S_FLIP) ? " (FLIPPED)" : "",
	       op->resource[0].start,
	       sol_compat ? "in sol_compat mode" : "");

	dev_set_drvdata(&op->dev, p);
	d7s_device = p;
	err = 0;

out:
	return err;

out_iounmap:
	of_iounmap(&op->resource[0], p->regs, sizeof(u8));

out_free:
	kfree(p);
	goto out;
}

static int __devexit d7s_remove(struct platform_device *op)
{
	struct d7s *p = dev_get_drvdata(&op->dev);
	u8 regs = readb(p->regs);

	
	if (sol_compat) {
		if (p->flipped)
			regs |= D7S_FLIP;
		else
			regs &= ~D7S_FLIP;
		writeb(regs, p->regs);
	}

	misc_deregister(&d7s_miscdev);
	of_iounmap(&op->resource[0], p->regs, sizeof(u8));
	kfree(p);

	return 0;
}

static const struct of_device_id d7s_match[] = {
	{
		.name = "display7seg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, d7s_match);

static struct platform_driver d7s_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = d7s_match,
	},
	.probe		= d7s_probe,
	.remove		= __devexit_p(d7s_remove),
};

module_platform_driver(d7s_driver);
