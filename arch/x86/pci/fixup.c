
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <asm/pci_x86.h>

static void __devinit pci_fixup_i450nx(struct pci_dev *d)
{
	int pxb, reg;
	u8 busno, suba, subb;

	dev_warn(&d->dev, "Searching for i450NX host bridges\n");
	reg = 0xd0;
	for(pxb = 0; pxb < 2; pxb++) {
		pci_read_config_byte(d, reg++, &busno);
		pci_read_config_byte(d, reg++, &suba);
		pci_read_config_byte(d, reg++, &subb);
		dev_dbg(&d->dev, "i450NX PXB %d: %02x/%02x/%02x\n", pxb, busno,
			suba, subb);
		if (busno)
			pci_scan_bus_with_sysdata(busno);	
		if (suba < subb)
			pci_scan_bus_with_sysdata(suba+1);	
	}
	pcibios_last_bus = -1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82451NX, pci_fixup_i450nx);

static void __devinit pci_fixup_i450gx(struct pci_dev *d)
{
	u8 busno;
	pci_read_config_byte(d, 0x4a, &busno);
	dev_info(&d->dev, "i440KX/GX host bridge; secondary bus %02x\n", busno);
	pci_scan_bus_with_sysdata(busno);
	pcibios_last_bus = -1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82454GX, pci_fixup_i450gx);

static void __devinit  pci_fixup_umc_ide(struct pci_dev *d)
{
	int i;

	dev_warn(&d->dev, "Fixing base address flags\n");
	for(i = 0; i < 4; i++)
		d->resource[i].flags |= PCI_BASE_ADDRESS_SPACE_IO;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_UMC, PCI_DEVICE_ID_UMC_UM8886BF, pci_fixup_umc_ide);

static void __devinit  pci_fixup_ncr53c810(struct pci_dev *d)
{
	if (!d->class) {
		dev_warn(&d->dev, "Fixing NCR 53C810 class code\n");
		d->class = PCI_CLASS_STORAGE_SCSI << 8;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NCR, PCI_DEVICE_ID_NCR_53C810, pci_fixup_ncr53c810);

static void __devinit  pci_fixup_latency(struct pci_dev *d)
{
	dev_dbg(&d->dev, "Setting max latency to 32\n");
	pcibios_max_latency = 32;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_5597, pci_fixup_latency);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_5598, pci_fixup_latency);

static void __devinit pci_fixup_piix4_acpi(struct pci_dev *d)
{
	d->irq = 9;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_3, pci_fixup_piix4_acpi);


#define VIA_8363_KL133_REVISION_ID 0x81
#define VIA_8363_KM133_REVISION_ID 0x84

static void pci_fixup_via_northbridge_bug(struct pci_dev *d)
{
	u8 v;
	int where = 0x55;
	int mask = 0x1f; 

	if (d->device == PCI_DEVICE_ID_VIA_8367_0) {
		pci_write_config_byte(d, PCI_LATENCY_TIMER, 0);

		where = 0x95; 
	} else if (d->device == PCI_DEVICE_ID_VIA_8363_0 &&
			(d->revision == VIA_8363_KL133_REVISION_ID ||
			d->revision == VIA_8363_KM133_REVISION_ID)) {
			mask = 0x3f; 
	}

	pci_read_config_byte(d, where, &v);
	if (v & ~mask) {
		dev_warn(&d->dev, "Disabling VIA memory write queue (PCI ID %04x, rev %02x): [%02x] %02x & %02x -> %02x\n", \
			d->device, d->revision, where, v, mask, v & mask);
		v &= mask;
		pci_write_config_byte(d, where, v);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8363_0, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8622, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8361, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8367_0, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8363_0, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8622, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8361, pci_fixup_via_northbridge_bug);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8367_0, pci_fixup_via_northbridge_bug);

static void __devinit pci_fixup_transparent_bridge(struct pci_dev *dev)
{
	if ((dev->device & 0xff00) == 0x2400)
		dev->transparent = 1;
}
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_INTEL, PCI_ANY_ID,
			 PCI_CLASS_BRIDGE_PCI, 8, pci_fixup_transparent_bridge);

static void pci_fixup_nforce2(struct pci_dev *dev)
{
	u32 val;

	pci_read_config_dword(dev, 0x6c, &val);

	if ((val & 0x00FF0000) != 0x00010000) {
		dev_warn(&dev->dev, "nForce2 C1 Halt Disconnect fixup\n");
		pci_write_config_dword(dev, 0x6c, (val & 0xFF00FFFF) | 0x00010000);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_NFORCE2, pci_fixup_nforce2);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_NFORCE2, pci_fixup_nforce2);

#define MAX_PCIEROOT	6
static int quirk_aspm_offset[MAX_PCIEROOT << 3];

#define GET_INDEX(a, b) ((((a) - PCI_DEVICE_ID_INTEL_MCH_PA) << 3) + ((b) & 7))

static int quirk_pcie_aspm_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
	return raw_pci_read(pci_domain_nr(bus), bus->number,
						devfn, where, size, value);
}

static int quirk_pcie_aspm_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 value)
{
	u8 offset;

	offset = quirk_aspm_offset[GET_INDEX(bus->self->device, devfn)];

	if ((offset) && (where == offset))
		value = value & 0xfffffffc;

	return raw_pci_write(pci_domain_nr(bus), bus->number,
						devfn, where, size, value);
}

static struct pci_ops quirk_pcie_aspm_ops = {
	.read = quirk_pcie_aspm_read,
	.write = quirk_pcie_aspm_write,
};

static void pcie_rootport_aspm_quirk(struct pci_dev *pdev)
{
	int cap_base, i;
	struct pci_bus  *pbus;
	struct pci_dev *dev;

	if ((pbus = pdev->subordinate) == NULL)
		return;

	if ((pdev->device < PCI_DEVICE_ID_INTEL_MCH_PA) ||
	    (pdev->device > PCI_DEVICE_ID_INTEL_MCH_PC1))
		return;

	if (list_empty(&pbus->devices)) {
		for (i = GET_INDEX(pdev->device, 0); i <= GET_INDEX(pdev->device, 7); ++i)
			quirk_aspm_offset[i] = 0;

		pbus->ops = pbus->parent->ops;
	} else {
		list_for_each_entry(dev, &pbus->devices, bus_list) {
			
			cap_base = pci_find_capability(dev, PCI_CAP_ID_EXP);
			quirk_aspm_offset[GET_INDEX(pdev->device, dev->devfn)] = cap_base + 0x10;
		}
		pbus->ops = &quirk_pcie_aspm_ops;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PA,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PA1,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PB,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PB1,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PC,	pcie_rootport_aspm_quirk);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_MCH_PC1,	pcie_rootport_aspm_quirk);


static void __devinit pci_fixup_video(struct pci_dev *pdev)
{
	struct pci_dev *bridge;
	struct pci_bus *bus;
	u16 config;

	
	bus = pdev->bus;
	while (bus) {
		bridge = bus->self;

		if (bridge
		    && ((bridge->hdr_type == PCI_HEADER_TYPE_BRIDGE)
		       || (bridge->hdr_type == PCI_HEADER_TYPE_CARDBUS))) {
			pci_read_config_word(bridge, PCI_BRIDGE_CONTROL,
						&config);
			if (!(config & PCI_BRIDGE_CTL_VGA))
				return;
		}
		bus = bus->parent;
	}
	pci_read_config_word(pdev, PCI_COMMAND, &config);
	if (config & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) {
		pdev->resource[PCI_ROM_RESOURCE].flags |= IORESOURCE_ROM_SHADOW;
		dev_printk(KERN_DEBUG, &pdev->dev, "Boot video device\n");
	}
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_ANY_ID, PCI_ANY_ID,
				PCI_CLASS_DISPLAY_VGA, 8, pci_fixup_video);


static const struct dmi_system_id __devinitconst msi_k8t_dmi_table[] = {
	{
		.ident = "MSI-K8T-Neo2Fir",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "MSI"),
			DMI_MATCH(DMI_PRODUCT_NAME, "MS-6702E"),
		},
	},
	{}
};

static void __devinit pci_fixup_msi_k8t_onboard_sound(struct pci_dev *dev)
{
	unsigned char val;
	if (!dmi_check_system(msi_k8t_dmi_table))
		return; 

	pci_read_config_byte(dev, 0x50, &val);
	if (val & 0x40) {
		pci_write_config_byte(dev, 0x50, val & (~0x40));

		
		pci_read_config_byte(dev, 0x50, &val);
		if (val & 0x40)
			dev_info(&dev->dev, "Detected MSI K8T Neo2-FIR; "
					"can't enable onboard soundcard!\n");
		else
			dev_info(&dev->dev, "Detected MSI K8T Neo2-FIR; "
					"enabled onboard soundcard\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8237,
		pci_fixup_msi_k8t_onboard_sound);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8237,
		pci_fixup_msi_k8t_onboard_sound);

static u16 toshiba_line_size;

static const struct dmi_system_id __devinitconst toshiba_ohci1394_dmi_table[] = {
	{
		.ident = "Toshiba PS5 based laptop",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "PS5"),
		},
	},
	{
		.ident = "Toshiba PSM4 based laptop",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "PSM4"),
		},
	},
	{
		.ident = "Toshiba A40 based laptop",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "PSA40U"),
		},
	},
	{ }
};

static void __devinit pci_pre_fixup_toshiba_ohci1394(struct pci_dev *dev)
{
	if (!dmi_check_system(toshiba_ohci1394_dmi_table))
		return; 

	dev->current_state = PCI_D3cold;
	pci_read_config_word(dev, PCI_CACHE_LINE_SIZE, &toshiba_line_size);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TI, 0x8032,
			 pci_pre_fixup_toshiba_ohci1394);

static void __devinit pci_post_fixup_toshiba_ohci1394(struct pci_dev *dev)
{
	if (!dmi_check_system(toshiba_ohci1394_dmi_table))
		return; 

	
	pci_write_config_word(dev, PCI_CACHE_LINE_SIZE, toshiba_line_size);
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, (u8 *)&dev->irq);
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_0,
			       pci_resource_start(dev, 0));
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_1,
			       pci_resource_start(dev, 1));
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_TI, 0x8032,
			 pci_post_fixup_toshiba_ohci1394);


static void pci_early_fixup_cyrix_5530(struct pci_dev *dev)
{
	u8 r;
	
	pci_read_config_byte(dev, 0x42, &r);
	r &= 0xfd;
	pci_write_config_byte(dev, 0x42, r);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5530_LEGACY,
			pci_early_fixup_cyrix_5530);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5530_LEGACY,
			pci_early_fixup_cyrix_5530);

static void __devinit pci_siemens_interrupt_controller(struct pci_dev *dev)
{
	dev->resource[0].flags |= IORESOURCE_PCI_FIXED;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SIEMENS, 0x0015,
			  pci_siemens_interrupt_controller);

static void sb600_disable_hpet_bar(struct pci_dev *dev)
{
	u8 val;


	pci_read_config_byte(dev, 0x08, &val);

	if (val < 0x2F) {
		outb(0x55, 0xCD6);
		val = inb(0xCD7);

		
		outb(0x55, 0xCD6);
		outb(val | 0x80, 0xCD7);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_ATI, 0x4385, sb600_disable_hpet_bar);
