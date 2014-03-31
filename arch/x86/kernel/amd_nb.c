/*
 * Shared support code for AMD K8 northbridges and derivates.
 * Copyright 2006 Andi Kleen, SUSE Labs. Subject to GPLv2.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <asm/amd_nb.h>

static u32 *flush_words;

const struct pci_device_id amd_nb_misc_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_K8_NB_MISC) },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_10H_NB_MISC) },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_15H_NB_F3) },
	{}
};
EXPORT_SYMBOL(amd_nb_misc_ids);

static struct pci_device_id amd_nb_link_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_15H_NB_F4) },
	{}
};

const struct amd_nb_bus_dev_range amd_nb_bus_dev_ranges[] __initconst = {
	{ 0x00, 0x18, 0x20 },
	{ 0xff, 0x00, 0x20 },
	{ 0xfe, 0x00, 0x20 },
	{ }
};

struct amd_northbridge_info amd_northbridges;
EXPORT_SYMBOL(amd_northbridges);

static struct pci_dev *next_northbridge(struct pci_dev *dev,
					const struct pci_device_id *ids)
{
	do {
		dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev);
		if (!dev)
			break;
	} while (!pci_match_id(ids, dev));
	return dev;
}

int amd_cache_northbridges(void)
{
	u16 i = 0;
	struct amd_northbridge *nb;
	struct pci_dev *misc, *link;

	if (amd_nb_num())
		return 0;

	misc = NULL;
	while ((misc = next_northbridge(misc, amd_nb_misc_ids)) != NULL)
		i++;

	if (i == 0)
		return 0;

	nb = kzalloc(i * sizeof(struct amd_northbridge), GFP_KERNEL);
	if (!nb)
		return -ENOMEM;

	amd_northbridges.nb = nb;
	amd_northbridges.num = i;

	link = misc = NULL;
	for (i = 0; i != amd_nb_num(); i++) {
		node_to_amd_nb(i)->misc = misc =
			next_northbridge(misc, amd_nb_misc_ids);
		node_to_amd_nb(i)->link = link =
			next_northbridge(link, amd_nb_link_ids);
        }

	
	if (boot_cpu_data.x86 == 0xf || boot_cpu_data.x86 == 0x10 ||
	    boot_cpu_data.x86 == 0x15)
		amd_northbridges.flags |= AMD_NB_GART;

	if (boot_cpu_data.x86 == 0x10 &&
	    boot_cpu_data.x86_model >= 0x8 &&
	    (boot_cpu_data.x86_model > 0x9 ||
	     boot_cpu_data.x86_mask >= 0x1))
		amd_northbridges.flags |= AMD_NB_L3_INDEX_DISABLE;

	if (boot_cpu_data.x86 == 0x15)
		amd_northbridges.flags |= AMD_NB_L3_INDEX_DISABLE;

	
	if (boot_cpu_data.x86 == 0x15)
		amd_northbridges.flags |= AMD_NB_L3_PARTITIONING;

	return 0;
}
EXPORT_SYMBOL_GPL(amd_cache_northbridges);

bool __init early_is_amd_nb(u32 device)
{
	const struct pci_device_id *id;
	u32 vendor = device & 0xffff;

	device >>= 16;
	for (id = amd_nb_misc_ids; id->vendor; id++)
		if (vendor == id->vendor && device == id->device)
			return true;
	return false;
}

struct resource *amd_get_mmconfig_range(struct resource *res)
{
	u32 address;
	u64 base, msr;
	unsigned segn_busn_bits;

	if (boot_cpu_data.x86_vendor != X86_VENDOR_AMD)
		return NULL;

	
        if (boot_cpu_data.x86 < 0x10)
		return NULL;

	address = MSR_FAM10H_MMIO_CONF_BASE;
	rdmsrl(address, msr);

	
	if (!(msr & FAM10H_MMIO_CONF_ENABLE))
		return NULL;

	base = msr & (FAM10H_MMIO_CONF_BASE_MASK<<FAM10H_MMIO_CONF_BASE_SHIFT);

	segn_busn_bits = (msr >> FAM10H_MMIO_CONF_BUSRANGE_SHIFT) &
			 FAM10H_MMIO_CONF_BUSRANGE_MASK;

	res->flags = IORESOURCE_MEM;
	res->start = base;
	res->end = base + (1ULL<<(segn_busn_bits + 20)) - 1;
	return res;
}

int amd_get_subcaches(int cpu)
{
	struct pci_dev *link = node_to_amd_nb(amd_get_nb_id(cpu))->link;
	unsigned int mask;
	int cuid;

	if (!amd_nb_has_feature(AMD_NB_L3_PARTITIONING))
		return 0;

	pci_read_config_dword(link, 0x1d4, &mask);

	cuid = cpu_data(cpu).compute_unit_id;
	return (mask >> (4 * cuid)) & 0xf;
}

int amd_set_subcaches(int cpu, int mask)
{
	static unsigned int reset, ban;
	struct amd_northbridge *nb = node_to_amd_nb(amd_get_nb_id(cpu));
	unsigned int reg;
	int cuid;

	if (!amd_nb_has_feature(AMD_NB_L3_PARTITIONING) || mask > 0xf)
		return -EINVAL;

	
	if (reset == 0) {
		pci_read_config_dword(nb->link, 0x1d4, &reset);
		pci_read_config_dword(nb->misc, 0x1b8, &ban);
		ban &= 0x180000;
	}

	
	if (mask != 0xf) {
		pci_read_config_dword(nb->misc, 0x1b8, &reg);
		pci_write_config_dword(nb->misc, 0x1b8, reg & ~0x180000);
	}

	cuid = cpu_data(cpu).compute_unit_id;
	mask <<= 4 * cuid;
	mask |= (0xf ^ (1 << cuid)) << 26;

	pci_write_config_dword(nb->link, 0x1d4, mask);

	
	pci_read_config_dword(nb->link, 0x1d4, &reg);
	if (reg == reset) {
		pci_read_config_dword(nb->misc, 0x1b8, &reg);
		reg &= ~0x180000;
		pci_write_config_dword(nb->misc, 0x1b8, reg | ban);
	}

	return 0;
}

static int amd_cache_gart(void)
{
	u16 i;

       if (!amd_nb_has_feature(AMD_NB_GART))
               return 0;

       flush_words = kmalloc(amd_nb_num() * sizeof(u32), GFP_KERNEL);
       if (!flush_words) {
               amd_northbridges.flags &= ~AMD_NB_GART;
               return -ENOMEM;
       }

       for (i = 0; i != amd_nb_num(); i++)
               pci_read_config_dword(node_to_amd_nb(i)->misc, 0x9c,
                                     &flush_words[i]);

       return 0;
}

void amd_flush_garts(void)
{
	int flushed, i;
	unsigned long flags;
	static DEFINE_SPINLOCK(gart_lock);

	if (!amd_nb_has_feature(AMD_NB_GART))
		return;

	spin_lock_irqsave(&gart_lock, flags);
	flushed = 0;
	for (i = 0; i < amd_nb_num(); i++) {
		pci_write_config_dword(node_to_amd_nb(i)->misc, 0x9c,
				       flush_words[i] | 1);
		flushed++;
	}
	for (i = 0; i < amd_nb_num(); i++) {
		u32 w;
		
		for (;;) {
			pci_read_config_dword(node_to_amd_nb(i)->misc,
					      0x9c, &w);
			if (!(w & 1))
				break;
			cpu_relax();
		}
	}
	spin_unlock_irqrestore(&gart_lock, flags);
	if (!flushed)
		printk("nothing to flush?\n");
}
EXPORT_SYMBOL_GPL(amd_flush_garts);

static __init int init_amd_nbs(void)
{
	int err = 0;

	err = amd_cache_northbridges();

	if (err < 0)
		printk(KERN_NOTICE "AMD NB: Cannot enumerate AMD northbridges.\n");

	if (amd_cache_gart() < 0)
		printk(KERN_NOTICE "AMD NB: Cannot initialize GART flush words, "
		       "GART support disabled.\n");

	return err;
}

fs_initcall(init_amd_nbs);
