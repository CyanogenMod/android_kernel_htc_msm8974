/*
 *  Written by Martin Kolinek, February 1996
 *
 * Changes:
 *
 *	Chris Beauregard July 28th, 1996
 *	- Fixed up integrated SCSI detection
 *
 *	Chris Beauregard August 3rd, 1996
 *	- Made mca_info local
 *	- Made integrated registers accessible through standard function calls
 *	- Added name field
 *	- More sanity checking
 *
 *	Chris Beauregard August 9th, 1996
 *	- Rewrote /proc/mca
 *
 *	Chris Beauregard January 7th, 1997
 *	- Added basic NMI-processing
 *	- Added more information to mca_info structure
 *
 *	David Weinehall October 12th, 1998
 *	- Made a lot of cleaning up in the source
 *	- Added use of save_flags / restore_flags
 *	- Added the 'driver_loaded' flag in MCA_adapter
 *	- Added an alternative implemention of ZP Gu's mca_find_unused_adapter
 *
 *	David Weinehall March 24th, 1999
 *	- Fixed the output of 'Driver Installed' in /proc/mca/pos
 *	- Made the Integrated Video & SCSI show up even if they have id 0000
 *
 *	Alexander Viro November 9th, 1999
 *	- Switched to regular procfs methods
 *
 *	Alfred Arnold & David Weinehall August 23rd, 2000
 *	- Added support for Planar POS-registers
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mca.h>
#include <linux/kprobes.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <linux/init.h>

static unsigned char which_scsi;

int MCA_bus;
EXPORT_SYMBOL(MCA_bus);

static DEFINE_SPINLOCK(mca_lock);


static void mca_configure_adapter_status(struct mca_device *mca_dev)
{
	mca_dev->status = MCA_ADAPTER_NONE;

	mca_dev->pos_id = mca_dev->pos[0]
		+ (mca_dev->pos[1] << 8);

	if (!mca_dev->pos_id && mca_dev->slot < MCA_MAX_SLOT_NR) {


		mca_dev->status = MCA_ADAPTER_ERROR;

		return;
	} else if (mca_dev->pos_id != 0xffff) {


		mca_dev->status = MCA_ADAPTER_NORMAL;
	}

	if ((mca_dev->pos_id == 0xffff ||
	    mca_dev->pos_id == 0x0000) && mca_dev->slot >= MCA_MAX_SLOT_NR) {
		int j;

		for (j = 2; j < 8; j++) {
			if (mca_dev->pos[j] != 0xff) {
				mca_dev->status = MCA_ADAPTER_NORMAL;
				break;
			}
		}
	}

	if (!(mca_dev->pos[2] & MCA_ENABLED)) {

		

		mca_dev->status = MCA_ADAPTER_DISABLED;
	}
} 


static struct resource mca_standard_resources[] = {
	{ .start = 0x60, .end = 0x60, .name = "system control port B (MCA)" },
	{ .start = 0x90, .end = 0x90, .name = "arbitration (MCA)" },
	{ .start = 0x91, .end = 0x91, .name = "card Select Feedback (MCA)" },
	{ .start = 0x92, .end = 0x92, .name = "system Control port A (MCA)" },
	{ .start = 0x94, .end = 0x94, .name = "system board setup (MCA)" },
	{ .start = 0x96, .end = 0x97, .name = "POS (MCA)" },
	{ .start = 0x100, .end = 0x107, .name = "POS (MCA)" }
};

#define MCA_STANDARD_RESOURCES	ARRAY_SIZE(mca_standard_resources)

static int mca_read_and_store_pos(unsigned char *pos)
{
	int j;
	int found = 0;

	for (j = 0; j < 8; j++) {
		pos[j] = inb_p(MCA_POS_REG(j));
		if (pos[j] != 0xff) {

			found = 1;
		}
	}
	return found;
}

static unsigned char mca_pc_read_pos(struct mca_device *mca_dev, int reg)
{
	unsigned char byte;
	unsigned long flags;

	if (reg < 0 || reg >= 8)
		return 0;

	spin_lock_irqsave(&mca_lock, flags);
	if (mca_dev->pos_register) {
		

		outb_p(0, MCA_ADAPTER_SETUP_REG);
		outb_p(mca_dev->pos_register, MCA_MOTHERBOARD_SETUP_REG);

		byte = inb_p(MCA_POS_REG(reg));
		outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);
	} else {

		

		outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);

		

		outb_p(0x8|(mca_dev->slot & 0xf), MCA_ADAPTER_SETUP_REG);
		byte = inb_p(MCA_POS_REG(reg));
		outb_p(0, MCA_ADAPTER_SETUP_REG);
	}
	spin_unlock_irqrestore(&mca_lock, flags);

	mca_dev->pos[reg] = byte;

	return byte;
}

static void mca_pc_write_pos(struct mca_device *mca_dev, int reg,
			     unsigned char byte)
{
	unsigned long flags;

	if (reg < 0 || reg >= 8)
		return;

	spin_lock_irqsave(&mca_lock, flags);

	

	outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);

	

	outb_p(0x8|(mca_dev->slot&0xf), MCA_ADAPTER_SETUP_REG);
	outb_p(byte, MCA_POS_REG(reg));
	outb_p(0, MCA_ADAPTER_SETUP_REG);

	spin_unlock_irqrestore(&mca_lock, flags);

	

	mca_dev->pos[reg] = byte;

}

static int mca_dummy_transform_irq(struct mca_device *mca_dev, int irq)
{
	return irq;
}

static int mca_dummy_transform_ioport(struct mca_device *mca_dev, int port)
{
	return port;
}

static void *mca_dummy_transform_memory(struct mca_device *mca_dev, void *mem)
{
	return mem;
}


static int __init mca_init(void)
{
	unsigned int i, j;
	struct mca_device *mca_dev;
	unsigned char pos[8];
	short mca_builtin_scsi_ports[] = {0xf7, 0xfd, 0x00};
	struct mca_bus *bus;


	

	if (mca_system_init()) {
		printk(KERN_ERR "MCA bus system initialisation failed\n");
		return -ENODEV;
	}

	if (!MCA_bus)
		return -ENODEV;

	printk(KERN_INFO "Micro Channel bus detected.\n");

	
	bus = mca_attach_bus(MCA_PRIMARY_BUS);
	if (!bus)
		goto out_nomem;
	bus->default_dma_mask = 0xffffffffLL;
	bus->f.mca_write_pos = mca_pc_write_pos;
	bus->f.mca_read_pos = mca_pc_read_pos;
	bus->f.mca_transform_irq = mca_dummy_transform_irq;
	bus->f.mca_transform_ioport = mca_dummy_transform_ioport;
	bus->f.mca_transform_memory = mca_dummy_transform_memory;

	
	mca_dev = kzalloc(sizeof(struct mca_device), GFP_KERNEL);
	if (unlikely(!mca_dev))
		goto out_nomem;

	spin_lock_irq(&mca_lock);

	

	outb_p(0, MCA_ADAPTER_SETUP_REG);

	

	mca_dev->pos_register = 0x7f;
	outb_p(mca_dev->pos_register, MCA_MOTHERBOARD_SETUP_REG);
	mca_dev->name[0] = 0;
	mca_read_and_store_pos(mca_dev->pos);
	mca_configure_adapter_status(mca_dev);
	
	mca_dev->pos_id = MCA_MOTHERBOARD_POS;
	mca_dev->slot = MCA_MOTHERBOARD;
	mca_register_device(MCA_PRIMARY_BUS, mca_dev);

	mca_dev = kzalloc(sizeof(struct mca_device), GFP_ATOMIC);
	if (unlikely(!mca_dev))
		goto out_unlock_nomem;


	mca_dev->pos_register = 0xdf;
	outb_p(mca_dev->pos_register, MCA_MOTHERBOARD_SETUP_REG);
	mca_dev->name[0] = 0;
	mca_read_and_store_pos(mca_dev->pos);
	mca_configure_adapter_status(mca_dev);
	
	mca_dev->pos_id = MCA_INTEGVIDEO_POS;
	mca_dev->slot = MCA_INTEGVIDEO;
	mca_register_device(MCA_PRIMARY_BUS, mca_dev);


	for (i = 0; (which_scsi = mca_builtin_scsi_ports[i]) != 0; i++) {
		outb_p(which_scsi, MCA_MOTHERBOARD_SETUP_REG);
		if (mca_read_and_store_pos(pos))
			break;
	}
	if (which_scsi) {
		
		mca_dev = kzalloc(sizeof(struct mca_device), GFP_ATOMIC);
		if (unlikely(!mca_dev))
			goto out_unlock_nomem;

		for (j = 0; j < 8; j++)
			mca_dev->pos[j] = pos[j];

		mca_configure_adapter_status(mca_dev);
		
		mca_dev->pos_id = MCA_INTEGSCSI_POS;
		mca_dev->slot = MCA_INTEGSCSI;
		mca_dev->pos_register = which_scsi;
		mca_register_device(MCA_PRIMARY_BUS, mca_dev);
	}

	

	outb_p(0xff, MCA_MOTHERBOARD_SETUP_REG);


	for (i = 0; i < MCA_MAX_SLOT_NR; i++) {
		outb_p(0x8|(i&0xf), MCA_ADAPTER_SETUP_REG);
		if (!mca_read_and_store_pos(pos))
			continue;

		mca_dev = kzalloc(sizeof(struct mca_device), GFP_ATOMIC);
		if (unlikely(!mca_dev))
			goto out_unlock_nomem;

		for (j = 0; j < 8; j++)
			mca_dev->pos[j] = pos[j];

		mca_dev->driver_loaded = 0;
		mca_dev->slot = i;
		mca_dev->pos_register = 0;
		mca_configure_adapter_status(mca_dev);
		mca_register_device(MCA_PRIMARY_BUS, mca_dev);
	}
	outb_p(0, MCA_ADAPTER_SETUP_REG);

	
	spin_unlock_irq(&mca_lock);

	for (i = 0; i < MCA_STANDARD_RESOURCES; i++)
		request_resource(&ioport_resource, mca_standard_resources + i);

	mca_do_proc_init();

	return 0;

 out_unlock_nomem:
	spin_unlock_irq(&mca_lock);
 out_nomem:
	printk(KERN_EMERG "Failed memory allocation in MCA setup!\n");
	return -ENOMEM;
}

subsys_initcall(mca_init);


static __kprobes void
mca_handle_nmi_device(struct mca_device *mca_dev, int check_flag)
{
	int slot = mca_dev->slot;

	if (slot == MCA_INTEGSCSI) {
		printk(KERN_CRIT "NMI: caused by MCA integrated SCSI adapter (%s)\n",
			mca_dev->name);
	} else if (slot == MCA_INTEGVIDEO) {
		printk(KERN_CRIT "NMI: caused by MCA integrated video adapter (%s)\n",
			mca_dev->name);
	} else if (slot == MCA_MOTHERBOARD) {
		printk(KERN_CRIT "NMI: caused by motherboard (%s)\n",
			mca_dev->name);
	}

	

	if (check_flag) {
		unsigned char pos6, pos7;

		pos6 = mca_device_read_pos(mca_dev, 6);
		pos7 = mca_device_read_pos(mca_dev, 7);

		printk(KERN_CRIT "NMI: POS 6 = 0x%x, POS 7 = 0x%x\n", pos6, pos7);
	}

} 


static int __kprobes mca_handle_nmi_callback(struct device *dev, void *data)
{
	struct mca_device *mca_dev = to_mca_device(dev);
	unsigned char pos5;

	pos5 = mca_device_read_pos(mca_dev, 5);

	if (!(pos5 & 0x80)) {
		mca_handle_nmi_device(mca_dev, !(pos5 & 0x40));
		return 1;
	}
	return 0;
}

void __kprobes mca_handle_nmi(void)
{
	bus_for_each_dev(&mca_bus_type, NULL, NULL, mca_handle_nmi_callback);
}
