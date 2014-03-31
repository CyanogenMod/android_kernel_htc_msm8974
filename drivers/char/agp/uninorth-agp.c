#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/agp_backend.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <asm/uninorth.h>
#include <asm/pci-bridge.h>
#include <asm/prom.h>
#include <asm/pmac_feature.h>
#include "agp.h"

static int uninorth_rev;
static int is_u3;
static u32 scratch_value;

#define DEFAULT_APERTURE_SIZE 256
#define DEFAULT_APERTURE_STRING "256"
static char *aperture = NULL;

static int uninorth_fetch_size(void)
{
	int i, size = 0;
	struct aper_size_info_32 *values =
	    A_SIZE_32(agp_bridge->driver->aperture_sizes);

	if (aperture) {
		char *save = aperture;

		size = memparse(aperture, &aperture) >> 20;
		aperture = save;

		for (i = 0; i < agp_bridge->driver->num_aperture_sizes; i++)
			if (size == values[i].size)
				break;

		if (i == agp_bridge->driver->num_aperture_sizes) {
			dev_err(&agp_bridge->dev->dev, "invalid aperture size, "
				"using default\n");
			size = 0;
			aperture = NULL;
		}
	}

	if (!size) {
		for (i = 0; i < agp_bridge->driver->num_aperture_sizes; i++)
			if (values[i].size == DEFAULT_APERTURE_SIZE)
				break;
	}

	agp_bridge->previous_size =
	    agp_bridge->current_size = (void *)(values + i);
	agp_bridge->aperture_size_idx = i;
	return values[i].size;
}

static void uninorth_tlbflush(struct agp_memory *mem)
{
	u32 ctrl = UNI_N_CFG_GART_ENABLE;

	if (is_u3)
		ctrl |= U3_N_CFG_GART_PERFRD;
	pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL,
			       ctrl | UNI_N_CFG_GART_INVAL);
	pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL, ctrl);

	if (!mem && uninorth_rev <= 0x30) {
		pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL,
				       ctrl | UNI_N_CFG_GART_2xRESET);
		pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL,
				       ctrl);
	}
}

static void uninorth_cleanup(void)
{
	u32 tmp;

	pci_read_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL, &tmp);
	if (!(tmp & UNI_N_CFG_GART_ENABLE))
		return;
	tmp |= UNI_N_CFG_GART_INVAL;
	pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL, tmp);
	pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL, 0);

	if (uninorth_rev <= 0x30) {
		pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL,
				       UNI_N_CFG_GART_2xRESET);
		pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_GART_CTRL,
				       0);
	}
}

static int uninorth_configure(void)
{
	struct aper_size_info_32 *current_size;

	current_size = A_SIZE_32(agp_bridge->current_size);

	dev_info(&agp_bridge->dev->dev, "configuring for size idx: %d\n",
		 current_size->size_value);

	
	pci_write_config_dword(agp_bridge->dev,
		UNI_N_CFG_GART_BASE,
		(agp_bridge->gatt_bus_addr & 0xfffff000)
			| current_size->size_value);

	agp_bridge->gart_bus_addr = 0;
#ifdef CONFIG_PPC64
	
	
	pci_write_config_dword(agp_bridge->dev, UNI_N_CFG_AGP_BASE,
			       (agp_bridge->gatt_bus_addr >> 32) & 0xf);
#else
	pci_write_config_dword(agp_bridge->dev,
		UNI_N_CFG_AGP_BASE, agp_bridge->gart_bus_addr);
#endif

	if (is_u3) {
		pci_write_config_dword(agp_bridge->dev,
				       UNI_N_CFG_GART_DUMMY_PAGE,
				       page_to_phys(agp_bridge->scratch_page_page) >> 12);
	}

	return 0;
}

static int uninorth_insert_memory(struct agp_memory *mem, off_t pg_start, int type)
{
	int i, num_entries;
	void *temp;
	u32 *gp;
	int mask_type;

	if (type != mem->type)
		return -EINVAL;

	mask_type = agp_bridge->driver->agp_type_to_mask_type(agp_bridge, type);
	if (mask_type != 0) {
		
		return -EINVAL;
	}

	if (mem->page_count == 0)
		return 0;

	temp = agp_bridge->current_size;
	num_entries = A_SIZE_32(temp)->num_entries;

	if ((pg_start + mem->page_count) > num_entries)
		return -EINVAL;

	gp = (u32 *) &agp_bridge->gatt_table[pg_start];
	for (i = 0; i < mem->page_count; ++i) {
		if (gp[i] != scratch_value) {
			dev_info(&agp_bridge->dev->dev,
				 "uninorth_insert_memory: entry 0x%x occupied (%x)\n",
				 i, gp[i]);
			return -EBUSY;
		}
	}

	for (i = 0; i < mem->page_count; i++) {
		if (is_u3)
			gp[i] = (page_to_phys(mem->pages[i]) >> PAGE_SHIFT) | 0x80000000UL;
		else
			gp[i] =	cpu_to_le32((page_to_phys(mem->pages[i]) & 0xFFFFF000UL) |
					    0x1UL);
		flush_dcache_range((unsigned long)__va(page_to_phys(mem->pages[i])),
				   (unsigned long)__va(page_to_phys(mem->pages[i]))+0x1000);
	}
	mb();
	uninorth_tlbflush(mem);

	return 0;
}

int uninorth_remove_memory(struct agp_memory *mem, off_t pg_start, int type)
{
	size_t i;
	u32 *gp;
	int mask_type;

	if (type != mem->type)
		return -EINVAL;

	mask_type = agp_bridge->driver->agp_type_to_mask_type(agp_bridge, type);
	if (mask_type != 0) {
		
		return -EINVAL;
	}

	if (mem->page_count == 0)
		return 0;

	gp = (u32 *) &agp_bridge->gatt_table[pg_start];
	for (i = 0; i < mem->page_count; ++i) {
		gp[i] = scratch_value;
	}
	mb();
	uninorth_tlbflush(mem);

	return 0;
}

static void uninorth_agp_enable(struct agp_bridge_data *bridge, u32 mode)
{
	u32 command, scratch, status;
	int timeout;

	pci_read_config_dword(bridge->dev,
			      bridge->capndx + PCI_AGP_STATUS,
			      &status);

	command = agp_collect_device_status(bridge, mode, status);
	command |= PCI_AGP_COMMAND_AGP;

	if (uninorth_rev == 0x21) {
		command &= ~AGPSTAT2_4X;
	}

	if ((uninorth_rev >= 0x30) && (uninorth_rev <= 0x33)) {
		if ((command >> AGPSTAT_RQ_DEPTH_SHIFT) > 7)
			command = (command & ~AGPSTAT_RQ_DEPTH)
				| (7 << AGPSTAT_RQ_DEPTH_SHIFT);
	}

	uninorth_tlbflush(NULL);

	timeout = 0;
	do {
		pci_write_config_dword(bridge->dev,
				       bridge->capndx + PCI_AGP_COMMAND,
				       command);
		pci_read_config_dword(bridge->dev,
				      bridge->capndx + PCI_AGP_COMMAND,
				       &scratch);
	} while ((scratch & PCI_AGP_COMMAND_AGP) == 0 && ++timeout < 1000);
	if ((scratch & PCI_AGP_COMMAND_AGP) == 0)
		dev_err(&bridge->dev->dev, "can't write UniNorth AGP "
			"command register\n");

	if (uninorth_rev >= 0x30) {
		
		agp_device_command(command, (status & AGPSTAT_MODE_3_0) != 0);
	} else {
		
		agp_device_command(command, false);
	}

	uninorth_tlbflush(NULL);
}

#ifdef CONFIG_PM
static int agp_uninorth_suspend(struct pci_dev *pdev)
{
	struct agp_bridge_data *bridge;
	u32 cmd;
	u8 agp;
	struct pci_dev *device = NULL;

	bridge = agp_find_bridge(pdev);
	if (bridge == NULL)
		return -ENODEV;

	
	if (bridge->dev_private_data)
		return 0;

	
	for_each_pci_dev(device) {
		
		if (device == pdev)
			continue;
		if (device->bus != pdev->bus)
			continue;
		agp = pci_find_capability(device, PCI_CAP_ID_AGP);
		if (!agp)
			continue;
		pci_read_config_dword(device, agp + PCI_AGP_COMMAND, &cmd);
		if (!(cmd & PCI_AGP_COMMAND_AGP))
			continue;
		dev_info(&pdev->dev, "disabling AGP on device %s\n",
			 pci_name(device));
		cmd &= ~PCI_AGP_COMMAND_AGP;
		pci_write_config_dword(device, agp + PCI_AGP_COMMAND, cmd);
	}

	
	agp = pci_find_capability(pdev, PCI_CAP_ID_AGP);
	pci_read_config_dword(pdev, agp + PCI_AGP_COMMAND, &cmd);
	bridge->dev_private_data = (void *)(long)cmd;
	if (cmd & PCI_AGP_COMMAND_AGP) {
		dev_info(&pdev->dev, "disabling AGP on bridge\n");
		cmd &= ~PCI_AGP_COMMAND_AGP;
		pci_write_config_dword(pdev, agp + PCI_AGP_COMMAND, cmd);
	}
	
	uninorth_cleanup();

	return 0;
}

static int agp_uninorth_resume(struct pci_dev *pdev)
{
	struct agp_bridge_data *bridge;
	u32 command;

	bridge = agp_find_bridge(pdev);
	if (bridge == NULL)
		return -ENODEV;

	command = (long)bridge->dev_private_data;
	bridge->dev_private_data = NULL;
	if (!(command & PCI_AGP_COMMAND_AGP))
		return 0;

	uninorth_agp_enable(bridge, command);

	return 0;
}
#endif 

static int uninorth_create_gatt_table(struct agp_bridge_data *bridge)
{
	char *table;
	char *table_end;
	int size;
	int page_order;
	int num_entries;
	int i;
	void *temp;
	struct page *page;
	struct page **pages;

	
	if (bridge->driver->size_type == LVL2_APER_SIZE)
		return -EINVAL;

	table = NULL;
	i = bridge->aperture_size_idx;
	temp = bridge->current_size;
	size = page_order = num_entries = 0;

	do {
		size = A_SIZE_32(temp)->size;
		page_order = A_SIZE_32(temp)->page_order;
		num_entries = A_SIZE_32(temp)->num_entries;

		table = (char *) __get_free_pages(GFP_KERNEL, page_order);

		if (table == NULL) {
			i++;
			bridge->current_size = A_IDX32(bridge);
		} else {
			bridge->aperture_size_idx = i;
		}
	} while (!table && (i < bridge->driver->num_aperture_sizes));

	if (table == NULL)
		return -ENOMEM;

	pages = kmalloc((1 << page_order) * sizeof(struct page*), GFP_KERNEL);
	if (pages == NULL)
		goto enomem;

	table_end = table + ((PAGE_SIZE * (1 << page_order)) - 1);

	for (page = virt_to_page(table), i = 0; page <= virt_to_page(table_end);
	     page++, i++) {
		SetPageReserved(page);
		pages[i] = page;
	}

	bridge->gatt_table_real = (u32 *) table;
	
	flush_dcache_range((unsigned long)table,
			   (unsigned long)table_end + 1);
	bridge->gatt_table = vmap(pages, (1 << page_order), 0, PAGE_KERNEL_NCG);

	if (bridge->gatt_table == NULL)
		goto enomem;

	bridge->gatt_bus_addr = virt_to_phys(table);

	if (is_u3)
		scratch_value = (page_to_phys(agp_bridge->scratch_page_page) >> PAGE_SHIFT) | 0x80000000UL;
	else
		scratch_value =	cpu_to_le32((page_to_phys(agp_bridge->scratch_page_page) & 0xFFFFF000UL) |
				0x1UL);
	for (i = 0; i < num_entries; i++)
		bridge->gatt_table[i] = scratch_value;

	return 0;

enomem:
	kfree(pages);
	if (table)
		free_pages((unsigned long)table, page_order);
	return -ENOMEM;
}

static int uninorth_free_gatt_table(struct agp_bridge_data *bridge)
{
	int page_order;
	char *table, *table_end;
	void *temp;
	struct page *page;

	temp = bridge->current_size;
	page_order = A_SIZE_32(temp)->page_order;


	vunmap(bridge->gatt_table);
	table = (char *) bridge->gatt_table_real;
	table_end = table + ((PAGE_SIZE * (1 << page_order)) - 1);

	for (page = virt_to_page(table); page <= virt_to_page(table_end); page++)
		ClearPageReserved(page);

	free_pages((unsigned long) bridge->gatt_table_real, page_order);

	return 0;
}

void null_cache_flush(void)
{
	mb();
}


static const struct aper_size_info_32 uninorth_sizes[] =
{
	{256, 65536, 6, 64},
	{128, 32768, 5, 32},
	{64, 16384, 4, 16},
	{32, 8192, 3, 8},
	{16, 4096, 2, 4},
	{8, 2048, 1, 2},
	{4, 1024, 0, 1}
};

static const struct aper_size_info_32 u3_sizes[] =
{
	{512, 131072, 7, 128},
	{256, 65536, 6, 64},
	{128, 32768, 5, 32},
	{64, 16384, 4, 16},
	{32, 8192, 3, 8},
	{16, 4096, 2, 4},
	{8, 2048, 1, 2},
	{4, 1024, 0, 1}
};

const struct agp_bridge_driver uninorth_agp_driver = {
	.owner			= THIS_MODULE,
	.aperture_sizes		= (void *)uninorth_sizes,
	.size_type		= U32_APER_SIZE,
	.num_aperture_sizes	= ARRAY_SIZE(uninorth_sizes),
	.configure		= uninorth_configure,
	.fetch_size		= uninorth_fetch_size,
	.cleanup		= uninorth_cleanup,
	.tlb_flush		= uninorth_tlbflush,
	.mask_memory		= agp_generic_mask_memory,
	.masks			= NULL,
	.cache_flush		= null_cache_flush,
	.agp_enable		= uninorth_agp_enable,
	.create_gatt_table	= uninorth_create_gatt_table,
	.free_gatt_table	= uninorth_free_gatt_table,
	.insert_memory		= uninorth_insert_memory,
	.remove_memory		= uninorth_remove_memory,
	.alloc_by_type		= agp_generic_alloc_by_type,
	.free_by_type		= agp_generic_free_by_type,
	.agp_alloc_page		= agp_generic_alloc_page,
	.agp_alloc_pages	= agp_generic_alloc_pages,
	.agp_destroy_page	= agp_generic_destroy_page,
	.agp_destroy_pages	= agp_generic_destroy_pages,
	.agp_type_to_mask_type  = agp_generic_type_to_mask_type,
	.cant_use_aperture	= true,
	.needs_scratch_page	= true,
};

const struct agp_bridge_driver u3_agp_driver = {
	.owner			= THIS_MODULE,
	.aperture_sizes		= (void *)u3_sizes,
	.size_type		= U32_APER_SIZE,
	.num_aperture_sizes	= ARRAY_SIZE(u3_sizes),
	.configure		= uninorth_configure,
	.fetch_size		= uninorth_fetch_size,
	.cleanup		= uninorth_cleanup,
	.tlb_flush		= uninorth_tlbflush,
	.mask_memory		= agp_generic_mask_memory,
	.masks			= NULL,
	.cache_flush		= null_cache_flush,
	.agp_enable		= uninorth_agp_enable,
	.create_gatt_table	= uninorth_create_gatt_table,
	.free_gatt_table	= uninorth_free_gatt_table,
	.insert_memory		= uninorth_insert_memory,
	.remove_memory		= uninorth_remove_memory,
	.alloc_by_type		= agp_generic_alloc_by_type,
	.free_by_type		= agp_generic_free_by_type,
	.agp_alloc_page		= agp_generic_alloc_page,
	.agp_alloc_pages	= agp_generic_alloc_pages,
	.agp_destroy_page	= agp_generic_destroy_page,
	.agp_destroy_pages	= agp_generic_destroy_pages,
	.agp_type_to_mask_type  = agp_generic_type_to_mask_type,
	.cant_use_aperture	= true,
	.needs_scratch_page	= true,
};

static struct agp_device_ids uninorth_agp_device_ids[] __devinitdata = {
	{
		.device_id	= PCI_DEVICE_ID_APPLE_UNI_N_AGP,
		.chipset_name	= "UniNorth",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_UNI_N_AGP_P,
		.chipset_name	= "UniNorth/Pangea",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_UNI_N_AGP15,
		.chipset_name	= "UniNorth 1.5",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_UNI_N_AGP2,
		.chipset_name	= "UniNorth 2",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_U3_AGP,
		.chipset_name	= "U3",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_U3L_AGP,
		.chipset_name	= "U3L",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_U3H_AGP,
		.chipset_name	= "U3H",
	},
	{
		.device_id	= PCI_DEVICE_ID_APPLE_IPID2_AGP,
		.chipset_name	= "UniNorth/Intrepid2",
	},
};

static int __devinit agp_uninorth_probe(struct pci_dev *pdev,
					const struct pci_device_id *ent)
{
	struct agp_device_ids *devs = uninorth_agp_device_ids;
	struct agp_bridge_data *bridge;
	struct device_node *uninorth_node;
	u8 cap_ptr;
	int j;

	cap_ptr = pci_find_capability(pdev, PCI_CAP_ID_AGP);
	if (cap_ptr == 0)
		return -ENODEV;

	
	for (j = 0; devs[j].chipset_name != NULL; ++j) {
		if (pdev->device == devs[j].device_id) {
			dev_info(&pdev->dev, "Apple %s chipset\n",
				 devs[j].chipset_name);
			goto found;
		}
	}

	dev_err(&pdev->dev, "unsupported Apple chipset [%04x/%04x]\n",
		pdev->vendor, pdev->device);
	return -ENODEV;

 found:
	
	uninorth_rev = 0;
	is_u3 = 0;
	
	uninorth_node = of_find_node_by_name(NULL, "uni-n");
	
	if (uninorth_node == NULL) {
		is_u3 = 1;
		uninorth_node = of_find_node_by_name(NULL, "u3");
	}
	if (uninorth_node) {
		const int *revprop = of_get_property(uninorth_node,
				"device-rev", NULL);
		if (revprop != NULL)
			uninorth_rev = *revprop & 0x3f;
		of_node_put(uninorth_node);
	}

#ifdef CONFIG_PM
	
	pmac_register_agp_pm(pdev, agp_uninorth_suspend, agp_uninorth_resume);
#endif

	
	bridge = agp_alloc_bridge();
	if (!bridge)
		return -ENOMEM;

	if (is_u3)
		bridge->driver = &u3_agp_driver;
	else
		bridge->driver = &uninorth_agp_driver;

	bridge->dev = pdev;
	bridge->capndx = cap_ptr;
	bridge->flags = AGP_ERRATA_FASTWRITES;

	
	pci_read_config_dword(pdev, cap_ptr+PCI_AGP_STATUS, &bridge->mode);

	pci_set_drvdata(pdev, bridge);
	return agp_add_bridge(bridge);
}

static void __devexit agp_uninorth_remove(struct pci_dev *pdev)
{
	struct agp_bridge_data *bridge = pci_get_drvdata(pdev);

#ifdef CONFIG_PM
	
	pmac_register_agp_pm(pdev, NULL, NULL);
#endif

	agp_remove_bridge(bridge);
	agp_put_bridge(bridge);
}

static struct pci_device_id agp_uninorth_pci_table[] = {
	{
	.class		= (PCI_CLASS_BRIDGE_HOST << 8),
	.class_mask	= ~0,
	.vendor		= PCI_VENDOR_ID_APPLE,
	.device		= PCI_ANY_ID,
	.subvendor	= PCI_ANY_ID,
	.subdevice	= PCI_ANY_ID,
	},
	{ }
};

MODULE_DEVICE_TABLE(pci, agp_uninorth_pci_table);

static struct pci_driver agp_uninorth_pci_driver = {
	.name		= "agpgart-uninorth",
	.id_table	= agp_uninorth_pci_table,
	.probe		= agp_uninorth_probe,
	.remove		= agp_uninorth_remove,
};

static int __init agp_uninorth_init(void)
{
	if (agp_off)
		return -EINVAL;
	return pci_register_driver(&agp_uninorth_pci_driver);
}

static void __exit agp_uninorth_cleanup(void)
{
	pci_unregister_driver(&agp_uninorth_pci_driver);
}

module_init(agp_uninorth_init);
module_exit(agp_uninorth_cleanup);

module_param(aperture, charp, 0);
MODULE_PARM_DESC(aperture,
		 "Aperture size, must be power of two between 4MB and an\n"
		 "\t\tupper limit specific to the UniNorth revision.\n"
		 "\t\tDefault: " DEFAULT_APERTURE_STRING "M");

MODULE_AUTHOR("Ben Herrenschmidt & Paul Mackerras");
MODULE_LICENSE("GPL");
