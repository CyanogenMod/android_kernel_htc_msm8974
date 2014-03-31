/*
 * File:	msi.c
 * Purpose:	PCI Message Signaled Interrupt (MSI)
 *
 * Copyright (C) 2003-2004 Intel
 * Copyright (C) Tom Long Nguyen (tom.l.nguyen@intel.com)
 */

#include <linux/err.h>
#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/msi.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "pci.h"
#include "msi.h"

static int pci_msi_enable = 1;


#ifndef arch_msi_check_device
int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif

#ifndef arch_setup_msi_irqs
# define arch_setup_msi_irqs default_setup_msi_irqs
# define HAVE_DEFAULT_MSI_SETUP_IRQS
#endif

#ifdef HAVE_DEFAULT_MSI_SETUP_IRQS
int default_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;

	if (type == PCI_CAP_ID_MSI && nvec > 1)
		return 1;

	list_for_each_entry(entry, &dev->msi_list, list) {
		ret = arch_setup_msi_irq(dev, entry);
		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

	return 0;
}
#endif

#ifndef arch_teardown_msi_irqs
# define arch_teardown_msi_irqs default_teardown_msi_irqs
# define HAVE_DEFAULT_MSI_TEARDOWN_IRQS
#endif

#ifdef HAVE_DEFAULT_MSI_TEARDOWN_IRQS
void default_teardown_msi_irqs(struct pci_dev *dev)
{
	struct msi_desc *entry;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int i, nvec;
		if (entry->irq == 0)
			continue;
		nvec = 1 << entry->msi_attrib.multiple;
		for (i = 0; i < nvec; i++)
			arch_teardown_msi_irq(entry->irq + i);
	}
}
#endif

#ifndef arch_restore_msi_irqs
# define arch_restore_msi_irqs default_restore_msi_irqs
# define HAVE_DEFAULT_MSI_RESTORE_IRQS
#endif

#ifdef HAVE_DEFAULT_MSI_RESTORE_IRQS
void default_restore_msi_irqs(struct pci_dev *dev, int irq)
{
	struct msi_desc *entry;

	entry = NULL;
	if (dev->msix_enabled) {
		list_for_each_entry(entry, &dev->msi_list, list) {
			if (irq == entry->irq)
				break;
		}
	} else if (dev->msi_enabled)  {
		entry = irq_get_msi_desc(irq);
	}

	if (entry)
		write_msi_msg(irq, &entry->msg);
}
#endif

static void msi_set_enable(struct pci_dev *dev, int pos, int enable)
{
	u16 control;

	BUG_ON(!pos);

	pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &control);
	control &= ~PCI_MSI_FLAGS_ENABLE;
	if (enable)
		control |= PCI_MSI_FLAGS_ENABLE;
	pci_write_config_word(dev, pos + PCI_MSI_FLAGS, control);
}

static void msix_set_enable(struct pci_dev *dev, int enable)
{
	int pos;
	u16 control;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (pos) {
		pci_read_config_word(dev, pos + PCI_MSIX_FLAGS, &control);
		control &= ~PCI_MSIX_FLAGS_ENABLE;
		if (enable)
			control |= PCI_MSIX_FLAGS_ENABLE;
		pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);
	}
}

static inline __attribute_const__ u32 msi_mask(unsigned x)
{
	
	if (x >= 5)
		return 0xffffffff;
	return (1 << (1 << x)) - 1;
}

static inline __attribute_const__ u32 msi_capable_mask(u16 control)
{
	return msi_mask((control >> 1) & 7);
}

static inline __attribute_const__ u32 msi_enabled_mask(u16 control)
{
	return msi_mask((control >> 4) & 7);
}

static u32 __msi_mask_irq(struct msi_desc *desc, u32 mask, u32 flag)
{
	u32 mask_bits = desc->masked;

	if (!desc->msi_attrib.maskbit)
		return 0;

	mask_bits &= ~mask;
	mask_bits |= flag;
	pci_write_config_dword(desc->dev, desc->mask_pos, mask_bits);

	return mask_bits;
}

static void msi_mask_irq(struct msi_desc *desc, u32 mask, u32 flag)
{
	desc->masked = __msi_mask_irq(desc, mask, flag);
}

static u32 __msix_mask_irq(struct msi_desc *desc, u32 flag)
{
	u32 mask_bits = desc->masked;
	unsigned offset = desc->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE +
						PCI_MSIX_ENTRY_VECTOR_CTRL;
	mask_bits &= ~PCI_MSIX_ENTRY_CTRL_MASKBIT;
	if (flag)
		mask_bits |= PCI_MSIX_ENTRY_CTRL_MASKBIT;
	writel(mask_bits, desc->mask_base + offset);

	return mask_bits;
}

static void msix_mask_irq(struct msi_desc *desc, u32 flag)
{
	desc->masked = __msix_mask_irq(desc, flag);
}

static void msi_set_mask_bit(struct irq_data *data, u32 flag)
{
	struct msi_desc *desc = irq_data_get_msi(data);

	if (desc->msi_attrib.is_msix) {
		msix_mask_irq(desc, flag);
		readl(desc->mask_base);		
	} else {
		unsigned offset = data->irq - desc->dev->irq;
		msi_mask_irq(desc, 1 << offset, flag << offset);
	}
}

void mask_msi_irq(struct irq_data *data)
{
	msi_set_mask_bit(data, 1);
}

void unmask_msi_irq(struct irq_data *data)
{
	msi_set_mask_bit(data, 0);
}

void __read_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	BUG_ON(entry->dev->current_state != PCI_D0);

	if (entry->msi_attrib.is_msix) {
		void __iomem *base = entry->mask_base +
			entry->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE;

		msg->address_lo = readl(base + PCI_MSIX_ENTRY_LOWER_ADDR);
		msg->address_hi = readl(base + PCI_MSIX_ENTRY_UPPER_ADDR);
		msg->data = readl(base + PCI_MSIX_ENTRY_DATA);
	} else {
		struct pci_dev *dev = entry->dev;
		int pos = entry->msi_attrib.pos;
		u16 data;

		pci_read_config_dword(dev, msi_lower_address_reg(pos),
					&msg->address_lo);
		if (entry->msi_attrib.is_64) {
			pci_read_config_dword(dev, msi_upper_address_reg(pos),
						&msg->address_hi);
			pci_read_config_word(dev, msi_data_reg(pos, 1), &data);
		} else {
			msg->address_hi = 0;
			pci_read_config_word(dev, msi_data_reg(pos, 0), &data);
		}
		msg->data = data;
	}
}

void read_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__read_msi_msg(entry, msg);
}

void __get_cached_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	BUG_ON(!(entry->msg.address_hi | entry->msg.address_lo |
		 entry->msg.data));

	*msg = entry->msg;
}

void get_cached_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__get_cached_msi_msg(entry, msg);
}

void __write_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	if (entry->dev->current_state != PCI_D0) {
		
	} else if (entry->msi_attrib.is_msix) {
		void __iomem *base;
		base = entry->mask_base +
			entry->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE;

		writel(msg->address_lo, base + PCI_MSIX_ENTRY_LOWER_ADDR);
		writel(msg->address_hi, base + PCI_MSIX_ENTRY_UPPER_ADDR);
		writel(msg->data, base + PCI_MSIX_ENTRY_DATA);
	} else {
		struct pci_dev *dev = entry->dev;
		int pos = entry->msi_attrib.pos;
		u16 msgctl;

		pci_read_config_word(dev, msi_control_reg(pos), &msgctl);
		msgctl &= ~PCI_MSI_FLAGS_QSIZE;
		msgctl |= entry->msi_attrib.multiple << 4;
		pci_write_config_word(dev, msi_control_reg(pos), msgctl);

		pci_write_config_dword(dev, msi_lower_address_reg(pos),
					msg->address_lo);
		if (entry->msi_attrib.is_64) {
			pci_write_config_dword(dev, msi_upper_address_reg(pos),
						msg->address_hi);
			pci_write_config_word(dev, msi_data_reg(pos, 1),
						msg->data);
		} else {
			pci_write_config_word(dev, msi_data_reg(pos, 0),
						msg->data);
		}
	}
	entry->msg = *msg;
}

void write_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__write_msi_msg(entry, msg);
}

static void free_msi_irqs(struct pci_dev *dev)
{
	struct msi_desc *entry, *tmp;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int i, nvec;
		if (!entry->irq)
			continue;
		nvec = 1 << entry->msi_attrib.multiple;
		for (i = 0; i < nvec; i++)
			BUG_ON(irq_has_action(entry->irq + i));
	}

	arch_teardown_msi_irqs(dev);

	list_for_each_entry_safe(entry, tmp, &dev->msi_list, list) {
		if (entry->msi_attrib.is_msix) {
			if (list_is_last(&entry->list, &dev->msi_list))
				iounmap(entry->mask_base);
		}

		if (entry->kobj.parent) {
			kobject_del(&entry->kobj);
			kobject_put(&entry->kobj);
		}

		list_del(&entry->list);
		kfree(entry);
	}
}

static struct msi_desc *alloc_msi_entry(struct pci_dev *dev)
{
	struct msi_desc *desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	INIT_LIST_HEAD(&desc->list);
	desc->dev = dev;

	return desc;
}

static void pci_intx_for_msi(struct pci_dev *dev, int enable)
{
	if (!(dev->dev_flags & PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG))
		pci_intx(dev, enable);
}

static void __pci_restore_msi_state(struct pci_dev *dev)
{
	int pos;
	u16 control;
	struct msi_desc *entry;

	if (!dev->msi_enabled)
		return;

	entry = irq_get_msi_desc(dev->irq);
	pos = entry->msi_attrib.pos;

	pci_intx_for_msi(dev, 0);
	msi_set_enable(dev, pos, 0);
	arch_restore_msi_irqs(dev, dev->irq);

	pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &control);
	msi_mask_irq(entry, msi_capable_mask(control), entry->masked);
	control &= ~PCI_MSI_FLAGS_QSIZE;
	control |= (entry->msi_attrib.multiple << 4) | PCI_MSI_FLAGS_ENABLE;
	pci_write_config_word(dev, pos + PCI_MSI_FLAGS, control);
}

static void __pci_restore_msix_state(struct pci_dev *dev)
{
	int pos;
	struct msi_desc *entry;
	u16 control;

	if (!dev->msix_enabled)
		return;
	BUG_ON(list_empty(&dev->msi_list));
	entry = list_first_entry(&dev->msi_list, struct msi_desc, list);
	pos = entry->msi_attrib.pos;
	pci_read_config_word(dev, pos + PCI_MSIX_FLAGS, &control);

	
	pci_intx_for_msi(dev, 0);
	control |= PCI_MSIX_FLAGS_ENABLE | PCI_MSIX_FLAGS_MASKALL;
	pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);

	list_for_each_entry(entry, &dev->msi_list, list) {
		arch_restore_msi_irqs(dev, entry->irq);
		msix_mask_irq(entry, entry->masked);
	}

	control &= ~PCI_MSIX_FLAGS_MASKALL;
	pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);
}

void pci_restore_msi_state(struct pci_dev *dev)
{
	__pci_restore_msi_state(dev);
	__pci_restore_msix_state(dev);
}
EXPORT_SYMBOL_GPL(pci_restore_msi_state);


#define to_msi_attr(obj) container_of(obj, struct msi_attribute, attr)
#define to_msi_desc(obj) container_of(obj, struct msi_desc, kobj)

struct msi_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct msi_desc *entry, struct msi_attribute *attr,
			char *buf);
	ssize_t (*store)(struct msi_desc *entry, struct msi_attribute *attr,
			 const char *buf, size_t count);
};

static ssize_t show_msi_mode(struct msi_desc *entry, struct msi_attribute *atr,
			     char *buf)
{
	return sprintf(buf, "%s\n", entry->msi_attrib.is_msix ? "msix" : "msi");
}

static ssize_t msi_irq_attr_show(struct kobject *kobj,
				 struct attribute *attr, char *buf)
{
	struct msi_attribute *attribute = to_msi_attr(attr);
	struct msi_desc *entry = to_msi_desc(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(entry, attribute, buf);
}

static const struct sysfs_ops msi_irq_sysfs_ops = {
	.show = msi_irq_attr_show,
};

static struct msi_attribute mode_attribute =
	__ATTR(mode, S_IRUGO, show_msi_mode, NULL);


struct attribute *msi_irq_default_attrs[] = {
	&mode_attribute.attr,
	NULL
};

void msi_kobj_release(struct kobject *kobj)
{
	struct msi_desc *entry = to_msi_desc(kobj);

	pci_dev_put(entry->dev);
}

static struct kobj_type msi_irq_ktype = {
	.release = msi_kobj_release,
	.sysfs_ops = &msi_irq_sysfs_ops,
	.default_attrs = msi_irq_default_attrs,
};

static int populate_msi_sysfs(struct pci_dev *pdev)
{
	struct msi_desc *entry;
	struct kobject *kobj;
	int ret;
	int count = 0;

	pdev->msi_kset = kset_create_and_add("msi_irqs", NULL, &pdev->dev.kobj);
	if (!pdev->msi_kset)
		return -ENOMEM;

	list_for_each_entry(entry, &pdev->msi_list, list) {
		kobj = &entry->kobj;
		kobj->kset = pdev->msi_kset;
		pci_dev_get(pdev);
		ret = kobject_init_and_add(kobj, &msi_irq_ktype, NULL,
				     "%u", entry->irq);
		if (ret)
			goto out_unroll;

		count++;
	}

	return 0;

out_unroll:
	list_for_each_entry(entry, &pdev->msi_list, list) {
		if (!count)
			break;
		kobject_del(&entry->kobj);
		kobject_put(&entry->kobj);
		count--;
	}
	return ret;
}

static int msi_capability_init(struct pci_dev *dev, int nvec)
{
	struct msi_desc *entry;
	int pos, ret;
	u16 control;
	unsigned mask;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
	msi_set_enable(dev, pos, 0);	

	pci_read_config_word(dev, msi_control_reg(pos), &control);
	
	entry = alloc_msi_entry(dev);
	if (!entry)
		return -ENOMEM;

	entry->msi_attrib.is_msix	= 0;
	entry->msi_attrib.is_64		= is_64bit_address(control);
	entry->msi_attrib.entry_nr	= 0;
	entry->msi_attrib.maskbit	= is_mask_bit_support(control);
	entry->msi_attrib.default_irq	= dev->irq;	
	entry->msi_attrib.pos		= pos;

	entry->mask_pos = msi_mask_reg(pos, entry->msi_attrib.is_64);
	
	if (entry->msi_attrib.maskbit)
		pci_read_config_dword(dev, entry->mask_pos, &entry->masked);
	mask = msi_capable_mask(control);
	msi_mask_irq(entry, mask, mask);

	list_add_tail(&entry->list, &dev->msi_list);

	
	ret = arch_setup_msi_irqs(dev, nvec, PCI_CAP_ID_MSI);
	if (ret) {
		msi_mask_irq(entry, mask, ~mask);
		free_msi_irqs(dev);
		return ret;
	}

	ret = populate_msi_sysfs(dev);
	if (ret) {
		msi_mask_irq(entry, mask, ~mask);
		free_msi_irqs(dev);
		return ret;
	}

	
	pci_intx_for_msi(dev, 0);
	msi_set_enable(dev, pos, 1);
	dev->msi_enabled = 1;

	dev->irq = entry->irq;
	return 0;
}

static void __iomem *msix_map_region(struct pci_dev *dev, unsigned pos,
							unsigned nr_entries)
{
	resource_size_t phys_addr;
	u32 table_offset;
	u8 bir;

	pci_read_config_dword(dev, msix_table_offset_reg(pos), &table_offset);
	bir = (u8)(table_offset & PCI_MSIX_FLAGS_BIRMASK);
	table_offset &= ~PCI_MSIX_FLAGS_BIRMASK;
	phys_addr = pci_resource_start(dev, bir) + table_offset;

	return ioremap_nocache(phys_addr, nr_entries * PCI_MSIX_ENTRY_SIZE);
}

static int msix_setup_entries(struct pci_dev *dev, unsigned pos,
				void __iomem *base, struct msix_entry *entries,
				int nvec)
{
	struct msi_desc *entry;
	int i;

	for (i = 0; i < nvec; i++) {
		entry = alloc_msi_entry(dev);
		if (!entry) {
			if (!i)
				iounmap(base);
			else
				free_msi_irqs(dev);
			
			return -ENOMEM;
		}

		entry->msi_attrib.is_msix	= 1;
		entry->msi_attrib.is_64		= 1;
		entry->msi_attrib.entry_nr	= entries[i].entry;
		entry->msi_attrib.default_irq	= dev->irq;
		entry->msi_attrib.pos		= pos;
		entry->mask_base		= base;

		list_add_tail(&entry->list, &dev->msi_list);
	}

	return 0;
}

static void msix_program_entries(struct pci_dev *dev,
					struct msix_entry *entries)
{
	struct msi_desc *entry;
	int i = 0;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int offset = entries[i].entry * PCI_MSIX_ENTRY_SIZE +
						PCI_MSIX_ENTRY_VECTOR_CTRL;

		entries[i].vector = entry->irq;
		irq_set_msi_desc(entry->irq, entry);
		entry->masked = readl(entry->mask_base + offset);
		msix_mask_irq(entry, 1);
		i++;
	}
}

static int msix_capability_init(struct pci_dev *dev,
				struct msix_entry *entries, int nvec)
{
	int pos, ret;
	u16 control;
	void __iomem *base;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	pci_read_config_word(dev, pos + PCI_MSIX_FLAGS, &control);

	
	control &= ~PCI_MSIX_FLAGS_ENABLE;
	pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);

	
	base = msix_map_region(dev, pos, multi_msix_capable(control));
	if (!base)
		return -ENOMEM;

	ret = msix_setup_entries(dev, pos, base, entries, nvec);
	if (ret)
		return ret;

	ret = arch_setup_msi_irqs(dev, nvec, PCI_CAP_ID_MSIX);
	if (ret)
		goto error;

	control |= PCI_MSIX_FLAGS_MASKALL | PCI_MSIX_FLAGS_ENABLE;
	pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);

	msix_program_entries(dev, entries);

	ret = populate_msi_sysfs(dev);
	if (ret) {
		ret = 0;
		goto error;
	}

	
	pci_intx_for_msi(dev, 0);
	dev->msix_enabled = 1;

	control &= ~PCI_MSIX_FLAGS_MASKALL;
	pci_write_config_word(dev, pos + PCI_MSIX_FLAGS, control);

	return 0;

error:
	if (ret < 0) {
		struct msi_desc *entry;
		int avail = 0;

		list_for_each_entry(entry, &dev->msi_list, list) {
			if (entry->irq != 0)
				avail++;
		}
		if (avail != 0)
			ret = avail;
	}

	free_msi_irqs(dev);

	return ret;
}

static int pci_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	struct pci_bus *bus;
	int ret;

	
	if (!pci_msi_enable || !dev || dev->no_msi)
		return -EINVAL;

	if (nvec < 1)
		return -ERANGE;

	for (bus = dev->bus; bus; bus = bus->parent)
		if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
			return -EINVAL;

	ret = arch_msi_check_device(dev, nvec, type);
	if (ret)
		return ret;

	if (!pci_find_capability(dev, type))
		return -EINVAL;

	return 0;
}

int pci_enable_msi_block(struct pci_dev *dev, unsigned int nvec)
{
	int status, pos, maxvec;
	u16 msgctl;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (!pos)
		return -EINVAL;
	pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &msgctl);
	maxvec = 1 << ((msgctl & PCI_MSI_FLAGS_QMASK) >> 1);
	if (nvec > maxvec)
		return maxvec;

	status = pci_msi_check_device(dev, nvec, PCI_CAP_ID_MSI);
	if (status)
		return status;

	WARN_ON(!!dev->msi_enabled);

	
	if (dev->msix_enabled) {
		dev_info(&dev->dev, "can't enable MSI "
			 "(MSI-X already enabled)\n");
		return -EINVAL;
	}

	status = msi_capability_init(dev, nvec);
	return status;
}
EXPORT_SYMBOL(pci_enable_msi_block);

void pci_msi_shutdown(struct pci_dev *dev)
{
	struct msi_desc *desc;
	u32 mask;
	u16 ctrl;
	unsigned pos;

	if (!pci_msi_enable || !dev || !dev->msi_enabled)
		return;

	BUG_ON(list_empty(&dev->msi_list));
	desc = list_first_entry(&dev->msi_list, struct msi_desc, list);
	pos = desc->msi_attrib.pos;

	msi_set_enable(dev, pos, 0);
	pci_intx_for_msi(dev, 1);
	dev->msi_enabled = 0;

	
	pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &ctrl);
	mask = msi_capable_mask(ctrl);
	
	__msi_mask_irq(desc, mask, ~mask);

	
	dev->irq = desc->msi_attrib.default_irq;
}

void pci_disable_msi(struct pci_dev *dev)
{
	if (!pci_msi_enable || !dev || !dev->msi_enabled)
		return;

	pci_msi_shutdown(dev);
	free_msi_irqs(dev);
	kset_unregister(dev->msi_kset);
	dev->msi_kset = NULL;
}
EXPORT_SYMBOL(pci_disable_msi);

int pci_msix_table_size(struct pci_dev *dev)
{
	int pos;
	u16 control;

	pos = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (!pos)
		return 0;

	pci_read_config_word(dev, msi_control_reg(pos), &control);
	return multi_msix_capable(control);
}

int pci_enable_msix(struct pci_dev *dev, struct msix_entry *entries, int nvec)
{
	int status, nr_entries;
	int i, j;

	if (!entries)
		return -EINVAL;

	status = pci_msi_check_device(dev, nvec, PCI_CAP_ID_MSIX);
	if (status)
		return status;

	nr_entries = pci_msix_table_size(dev);
	if (nvec > nr_entries)
		return nr_entries;

	
	for (i = 0; i < nvec; i++) {
		if (entries[i].entry >= nr_entries)
			return -EINVAL;		
		for (j = i + 1; j < nvec; j++) {
			if (entries[i].entry == entries[j].entry)
				return -EINVAL;	
		}
	}
	WARN_ON(!!dev->msix_enabled);

	
	if (dev->msi_enabled) {
		dev_info(&dev->dev, "can't enable MSI-X "
		       "(MSI IRQ already assigned)\n");
		return -EINVAL;
	}
	status = msix_capability_init(dev, entries, nvec);
	return status;
}
EXPORT_SYMBOL(pci_enable_msix);

void pci_msix_shutdown(struct pci_dev *dev)
{
	struct msi_desc *entry;

	if (!pci_msi_enable || !dev || !dev->msix_enabled)
		return;

	
	list_for_each_entry(entry, &dev->msi_list, list) {
		
		__msix_mask_irq(entry, 1);
	}

	msix_set_enable(dev, 0);
	pci_intx_for_msi(dev, 1);
	dev->msix_enabled = 0;
}

void pci_disable_msix(struct pci_dev *dev)
{
	if (!pci_msi_enable || !dev || !dev->msix_enabled)
		return;

	pci_msix_shutdown(dev);
	free_msi_irqs(dev);
	kset_unregister(dev->msi_kset);
	dev->msi_kset = NULL;
}
EXPORT_SYMBOL(pci_disable_msix);

void msi_remove_pci_irq_vectors(struct pci_dev *dev)
{
	if (!pci_msi_enable || !dev)
		return;

	if (dev->msi_enabled || dev->msix_enabled)
		free_msi_irqs(dev);
}

void pci_no_msi(void)
{
	pci_msi_enable = 0;
}

int pci_msi_enabled(void)
{
	return pci_msi_enable;
}
EXPORT_SYMBOL(pci_msi_enabled);

void pci_msi_init_pci_dev(struct pci_dev *dev)
{
	int pos;
	INIT_LIST_HEAD(&dev->msi_list);

	pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (pos)
		msi_set_enable(dev, pos, 0);
	msix_set_enable(dev, 0);
}
