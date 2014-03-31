
#define KMSG_COMPONENT "dasd"

#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/blkpg.h>

#include <asm/uaccess.h>

#define PRINTK_HEADER "dasd_gendisk:"

#include "dasd_int.h"

int dasd_gendisk_alloc(struct dasd_block *block)
{
	struct gendisk *gdp;
	struct dasd_device *base;
	int len;

	
	base = block->base;
	if (base->devindex >= DASD_PER_MAJOR)
		return -EBUSY;

	gdp = alloc_disk(1 << DASD_PARTN_BITS);
	if (!gdp)
		return -ENOMEM;

	
	gdp->major = DASD_MAJOR;
	gdp->first_minor = base->devindex << DASD_PARTN_BITS;
	gdp->fops = &dasd_device_operations;
	gdp->driverfs_dev = &base->cdev->dev;

	len = sprintf(gdp->disk_name, "dasd");
	if (base->devindex > 25) {
		if (base->devindex > 701) {
			if (base->devindex > 18277)
			        len += sprintf(gdp->disk_name + len, "%c",
					       'a'+(((base->devindex-18278)
						     /17576)%26));
			len += sprintf(gdp->disk_name + len, "%c",
				       'a'+(((base->devindex-702)/676)%26));
		}
		len += sprintf(gdp->disk_name + len, "%c",
			       'a'+(((base->devindex-26)/26)%26));
	}
	len += sprintf(gdp->disk_name + len, "%c", 'a'+(base->devindex%26));

	if (base->features & DASD_FEATURE_READONLY ||
	    test_bit(DASD_FLAG_DEVICE_RO, &base->flags))
		set_disk_ro(gdp, 1);
	dasd_add_link_to_gendisk(gdp, base);
	gdp->queue = block->request_queue;
	block->gdp = gdp;
	set_capacity(block->gdp, 0);
	add_disk(block->gdp);
	return 0;
}

void dasd_gendisk_free(struct dasd_block *block)
{
	if (block->gdp) {
		del_gendisk(block->gdp);
		block->gdp->queue = NULL;
		block->gdp->private_data = NULL;
		put_disk(block->gdp);
		block->gdp = NULL;
	}
}

int dasd_scan_partitions(struct dasd_block *block)
{
	struct block_device *bdev;

	bdev = bdget_disk(block->gdp, 0);
	if (!bdev || blkdev_get(bdev, FMODE_READ, NULL) < 0)
		return -ENODEV;
	ioctl_by_bdev(bdev, BLKRRPART, 0);
	block->bdev = bdev;
	return 0;
}

void dasd_destroy_partitions(struct dasd_block *block)
{
	
	struct blkpg_partition bpart;
	struct blkpg_ioctl_arg barg;
	struct block_device *bdev;

	bdev = block->bdev;
	block->bdev = NULL;

	memset(&bpart, 0, sizeof(struct blkpg_partition));
	memset(&barg, 0, sizeof(struct blkpg_ioctl_arg));
	barg.data = (void __force __user *) &bpart;
	barg.op = BLKPG_DEL_PARTITION;
	for (bpart.pno = block->gdp->minors - 1; bpart.pno > 0; bpart.pno--)
		ioctl_by_bdev(bdev, BLKPG, (unsigned long) &barg);

	invalidate_partition(block->gdp, 0);
	
	blkdev_put(bdev, FMODE_READ);
	set_capacity(block->gdp, 0);
}

int dasd_gendisk_init(void)
{
	int rc;

	
	rc = register_blkdev(DASD_MAJOR, "dasd");
	if (rc != 0) {
		pr_warning("Registering the device driver with major number "
			   "%d failed\n", DASD_MAJOR);
		return rc;
	}
	return 0;
}

void dasd_gendisk_exit(void)
{
	unregister_blkdev(DASD_MAJOR, "dasd");
}
