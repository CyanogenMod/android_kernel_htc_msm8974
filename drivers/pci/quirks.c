/*
 *  This file contains work-arounds for many known PCI hardware
 *  bugs.  Devices present only on certain architectures (host
 *  bridges et cetera) should be handled in arch-specific code.
 *
 *  Note: any quirks for hotpluggable devices must _NOT_ be declared __init.
 *
 *  Copyright (c) 1999 Martin Mares <mj@ucw.cz>
 *
 *  Init/reset quirks for USB host controllers should be in the
 *  USB quirks file, where their drivers can access reuse it.
 *
 *  The bridge optimization stuff has been removed. If you really
 *  have a silly BIOS which is unable to set your host bridge right,
 *  use the PowerTweak utility (see http://powertweak.sourceforge.net).
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/kallsyms.h>
#include <linux/dmi.h>
#include <linux/pci-aspm.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <asm/dma.h>	
#include "pci.h"

static void __devinit quirk_mmio_always_on(struct pci_dev *dev)
{
	dev->mmio_always_on = 1;
}
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_ANY_ID, PCI_ANY_ID,
				PCI_CLASS_BRIDGE_HOST, 8, quirk_mmio_always_on);

static void __devinit quirk_mellanox_tavor(struct pci_dev *dev)
{
	dev->broken_parity_status = 1;	
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_MELLANOX,PCI_DEVICE_ID_MELLANOX_TAVOR,quirk_mellanox_tavor);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_MELLANOX,PCI_DEVICE_ID_MELLANOX_TAVOR_BRIDGE,quirk_mellanox_tavor);

static void quirk_passive_release(struct pci_dev *dev)
{
	struct pci_dev *d = NULL;
	unsigned char dlc;

	while ((d = pci_get_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0, d))) {
		pci_read_config_byte(d, 0x82, &dlc);
		if (!(dlc & 1<<1)) {
			dev_info(&d->dev, "PIIX3: Enabling Passive Release\n");
			dlc |= 1<<1;
			pci_write_config_byte(d, 0x82, dlc);
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_passive_release);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82441,	quirk_passive_release);

    
static void __devinit quirk_isa_dma_hangs(struct pci_dev *dev)
{
	if (!isa_dma_bridge_buggy) {
		isa_dma_bridge_buggy=1;
		dev_info(&dev->dev, "Activating ISA DMA hang workarounds\n");
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_0,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C596,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82371SB_0,  quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M1533, 	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NEC,	PCI_DEVICE_ID_NEC_CBUS_1,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NEC,	PCI_DEVICE_ID_NEC_CBUS_2,	quirk_isa_dma_hangs);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NEC,	PCI_DEVICE_ID_NEC_CBUS_3,	quirk_isa_dma_hangs);

static void __devinit quirk_tigerpoint_bm_sts(struct pci_dev *dev)
{
	u32 pmbase;
	u16 pm1a;

	pci_read_config_dword(dev, 0x40, &pmbase);
	pmbase = pmbase & 0xff80;
	pm1a = inw(pmbase);

	if (pm1a & 0x10) {
		dev_info(&dev->dev, FW_BUG "TigerPoint LPC.BM_STS cleared\n");
		outw(0x10, pmbase);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_TGP_LPC, quirk_tigerpoint_bm_sts);

static void __devinit quirk_nopcipci(struct pci_dev *dev)
{
	if ((pci_pci_problems & PCIPCI_FAIL)==0) {
		dev_info(&dev->dev, "Disabling direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_FAIL;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_5597,		quirk_nopcipci);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_496,		quirk_nopcipci);

static void __devinit quirk_nopciamd(struct pci_dev *dev)
{
	u8 rev;
	pci_read_config_byte(dev, 0x08, &rev);
	if (rev == 0x13) {
		
		dev_info(&dev->dev, "Chipset erratum: Disabling direct PCI/AGP transfers\n");
		pci_pci_problems |= PCIAGP_FAIL;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8151_0,	quirk_nopciamd);

static void __devinit quirk_triton(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_TRITON)==0) {
		dev_info(&dev->dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_TRITON;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82437, 	quirk_triton);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82437VX, 	quirk_triton);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82439, 	quirk_triton);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82439TX, 	quirk_triton);

static void quirk_vialatency(struct pci_dev *dev)
{
	struct pci_dev *p;
	u8 busarb;
	   
	p = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686, NULL);
	if (p!=NULL) {
		
		
		if (p->revision < 0x40 || p->revision > 0x42)
			goto exit;
	} else {
		p = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8231, NULL);
		if (p==NULL)	
			goto exit;
		
		if (p->revision < 0x10 || p->revision > 0x12)
			goto exit;
	}
	

	pci_read_config_byte(dev, 0x76, &busarb);
	busarb &= ~(1<<5);
	busarb |= (1<<4);
	pci_write_config_byte(dev, 0x76, busarb);
	dev_info(&dev->dev, "Applying VIA southbridge workaround\n");
exit:
	pci_dev_put(p);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8363_0,	quirk_vialatency);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8371_1,	quirk_vialatency);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8361,		quirk_vialatency);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8363_0,	quirk_vialatency);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8371_1,	quirk_vialatency);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8361,		quirk_vialatency);

static void __devinit quirk_viaetbf(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_VIAETBF)==0) {
		dev_info(&dev->dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_VIAETBF;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C597_0,	quirk_viaetbf);

static void __devinit quirk_vsfx(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_VSFX)==0) {
		dev_info(&dev->dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_VSFX;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C576,	quirk_vsfx);

	
static void __init quirk_alimagik(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_ALIMAGIK)==0) {
		dev_info(&dev->dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_ALIMAGIK|PCIPCI_TRITON;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, 	PCI_DEVICE_ID_AL_M1647, 	quirk_alimagik);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, 	PCI_DEVICE_ID_AL_M1651, 	quirk_alimagik);

static void __devinit quirk_natoma(struct pci_dev *dev)
{
	if ((pci_pci_problems&PCIPCI_NATOMA)==0) {
		dev_info(&dev->dev, "Limiting direct PCI/PCI transfers\n");
		pci_pci_problems |= PCIPCI_NATOMA;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82441, 	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443LX_0, 	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443LX_1, 	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443BX_0, 	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443BX_1, 	quirk_natoma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 	PCI_DEVICE_ID_INTEL_82443BX_2, 	quirk_natoma);

static void __devinit quirk_citrine(struct pci_dev *dev)
{
	dev->cfg_size = 0xA0;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_IBM,	PCI_DEVICE_ID_IBM_CITRINE,	quirk_citrine);

static void __devinit quirk_s3_64M(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[0];

	if ((r->start & 0x3ffffff) || r->end != r->start + 0x3ffffff) {
		r->start = 0;
		r->end = 0x3ffffff;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_S3,	PCI_DEVICE_ID_S3_868,		quirk_s3_64M);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_S3,	PCI_DEVICE_ID_S3_968,		quirk_s3_64M);

static void __devinit quirk_cs5536_vsa(struct pci_dev *dev)
{
	if (pci_resource_len(dev, 0) != 8) {
		struct resource *res = &dev->resource[0];
		res->end = res->start + 8 - 1;
		dev_info(&dev->dev, "CS5536 ISA bridge bug detected "
				"(incorrect header); workaround applied.\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_ISA, quirk_cs5536_vsa);

static void __devinit quirk_io_region(struct pci_dev *dev, unsigned region,
	unsigned size, int nr, const char *name)
{
	region &= ~(size-1);
	if (region) {
		struct pci_bus_region bus_region;
		struct resource *res = dev->resource + nr;

		res->name = pci_name(dev);
		res->start = region;
		res->end = region + size - 1;
		res->flags = IORESOURCE_IO;

		
		bus_region.start = res->start;
		bus_region.end = res->end;
		pcibios_bus_to_resource(dev, res, &bus_region);

		if (pci_claim_resource(dev, nr) == 0)
			dev_info(&dev->dev, "quirk: %pR claimed by %s\n",
				 res, name);
	}
}	

static void __devinit quirk_ati_exploding_mce(struct pci_dev *dev)
{
	dev_info(&dev->dev, "ATI Northbridge, reserving I/O ports 0x3b0 to 0x3bb\n");
	
	request_region(0x3b0, 0x0C, "RadeonIGP");
	request_region(0x3d3, 0x01, "RadeonIGP");
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI,	PCI_DEVICE_ID_ATI_RS100,   quirk_ati_exploding_mce);

static void __devinit quirk_ali7101_acpi(struct pci_dev *dev)
{
	u16 region;

	pci_read_config_word(dev, 0xE0, &region);
	quirk_io_region(dev, region, 64, PCI_BRIDGE_RESOURCES, "ali7101 ACPI");
	pci_read_config_word(dev, 0xE2, &region);
	quirk_io_region(dev, region, 32, PCI_BRIDGE_RESOURCES+1, "ali7101 SMB");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AL,	PCI_DEVICE_ID_AL_M7101,		quirk_ali7101_acpi);

static void piix4_io_quirk(struct pci_dev *dev, const char *name, unsigned int port, unsigned int enable)
{
	u32 devres;
	u32 mask, size, base;

	pci_read_config_dword(dev, port, &devres);
	if ((devres & enable) != enable)
		return;
	mask = (devres >> 16) & 15;
	base = devres & 0xffff;
	size = 16;
	for (;;) {
		unsigned bit = size >> 1;
		if ((bit & mask) == bit)
			break;
		size = bit;
	}
	base &= -size;
	dev_info(&dev->dev, "%s PIO at %04x-%04x\n", name, base, base + size - 1);
}

static void piix4_mem_quirk(struct pci_dev *dev, const char *name, unsigned int port, unsigned int enable)
{
	u32 devres;
	u32 mask, size, base;

	pci_read_config_dword(dev, port, &devres);
	if ((devres & enable) != enable)
		return;
	base = devres & 0xffff0000;
	mask = (devres & 0x3f) << 16;
	size = 128 << 16;
	for (;;) {
		unsigned bit = size >> 1;
		if ((bit & mask) == bit)
			break;
		size = bit;
	}
	base &= -size;
	dev_info(&dev->dev, "%s MMIO at %04x-%04x\n", name, base, base + size - 1);
}

static void __devinit quirk_piix4_acpi(struct pci_dev *dev)
{
	u32 region, res_a;

	pci_read_config_dword(dev, 0x40, &region);
	quirk_io_region(dev, region, 64, PCI_BRIDGE_RESOURCES, "PIIX4 ACPI");
	pci_read_config_dword(dev, 0x90, &region);
	quirk_io_region(dev, region, 16, PCI_BRIDGE_RESOURCES+1, "PIIX4 SMB");

	
	pci_read_config_dword(dev, 0x5c, &res_a);

	piix4_io_quirk(dev, "PIIX4 devres B", 0x60, 3 << 21);
	piix4_io_quirk(dev, "PIIX4 devres C", 0x64, 3 << 21);

	

	
	if (res_a & (1 << 29)) {
		piix4_io_quirk(dev, "PIIX4 devres E", 0x68, 1 << 20);
		piix4_mem_quirk(dev, "PIIX4 devres F", 0x6c, 1 << 7);
	}
	
	if (res_a & (1 << 30)) {
		piix4_io_quirk(dev, "PIIX4 devres G", 0x70, 1 << 20);
		piix4_mem_quirk(dev, "PIIX4 devres H", 0x74, 1 << 7);
	}
	piix4_io_quirk(dev, "PIIX4 devres I", 0x78, 1 << 20);
	piix4_io_quirk(dev, "PIIX4 devres J", 0x7c, 1 << 20);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82371AB_3,	quirk_piix4_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82443MX_3,	quirk_piix4_acpi);

#define ICH_PMBASE	0x40
#define ICH_ACPI_CNTL	0x44
#define  ICH4_ACPI_EN	0x10
#define  ICH6_ACPI_EN	0x80
#define ICH4_GPIOBASE	0x58
#define ICH4_GPIO_CNTL	0x5c
#define  ICH4_GPIO_EN	0x10
#define ICH6_GPIOBASE	0x48
#define ICH6_GPIO_CNTL	0x4c
#define  ICH6_GPIO_EN	0x10

static void __devinit quirk_ich4_lpc_acpi(struct pci_dev *dev)
{
	u32 region;
	u8 enable;


	pci_read_config_byte(dev, ICH_ACPI_CNTL, &enable);
	if (enable & ICH4_ACPI_EN) {
		pci_read_config_dword(dev, ICH_PMBASE, &region);
		region &= PCI_BASE_ADDRESS_IO_MASK;
		if (region >= PCIBIOS_MIN_IO)
			quirk_io_region(dev, region, 128, PCI_BRIDGE_RESOURCES,
					"ICH4 ACPI/GPIO/TCO");
	}

	pci_read_config_byte(dev, ICH4_GPIO_CNTL, &enable);
	if (enable & ICH4_GPIO_EN) {
		pci_read_config_dword(dev, ICH4_GPIOBASE, &region);
		region &= PCI_BASE_ADDRESS_IO_MASK;
		if (region >= PCIBIOS_MIN_IO)
			quirk_io_region(dev, region, 64,
					PCI_BRIDGE_RESOURCES + 1, "ICH4 GPIO");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801AA_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801AB_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801BA_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801BA_10,	quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801CA_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801CA_12,	quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801DB_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801DB_12,	quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_82801EB_0,		quirk_ich4_lpc_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,    PCI_DEVICE_ID_INTEL_ESB_1,		quirk_ich4_lpc_acpi);

static void __devinit ich6_lpc_acpi_gpio(struct pci_dev *dev)
{
	u32 region;
	u8 enable;

	pci_read_config_byte(dev, ICH_ACPI_CNTL, &enable);
	if (enable & ICH6_ACPI_EN) {
		pci_read_config_dword(dev, ICH_PMBASE, &region);
		region &= PCI_BASE_ADDRESS_IO_MASK;
		if (region >= PCIBIOS_MIN_IO)
			quirk_io_region(dev, region, 128, PCI_BRIDGE_RESOURCES,
					"ICH6 ACPI/GPIO/TCO");
	}

	pci_read_config_byte(dev, ICH6_GPIO_CNTL, &enable);
	if (enable & ICH6_GPIO_EN) {
		pci_read_config_dword(dev, ICH6_GPIOBASE, &region);
		region &= PCI_BASE_ADDRESS_IO_MASK;
		if (region >= PCIBIOS_MIN_IO)
			quirk_io_region(dev, region, 64,
					PCI_BRIDGE_RESOURCES + 1, "ICH6 GPIO");
	}
}

static void __devinit ich6_lpc_generic_decode(struct pci_dev *dev, unsigned reg, const char *name, int dynsize)
{
	u32 val;
	u32 size, base;

	pci_read_config_dword(dev, reg, &val);

	
	if (!(val & 1))
		return;
	base = val & 0xfffc;
	if (dynsize) {
		size = 16;
	} else {
		size = 128;
	}
	base &= ~(size-1);

	
	dev_info(&dev->dev, "%s PIO at %04x-%04x\n", name, base, base+size-1);
}

static void __devinit quirk_ich6_lpc(struct pci_dev *dev)
{
	
	ich6_lpc_acpi_gpio(dev);

	
	ich6_lpc_generic_decode(dev, 0x84, "LPC Generic IO decode 1", 0);
	ich6_lpc_generic_decode(dev, 0x88, "LPC Generic IO decode 2", 1);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_0, quirk_ich6_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1, quirk_ich6_lpc);

static void __devinit ich7_lpc_generic_decode(struct pci_dev *dev, unsigned reg, const char *name)
{
	u32 val;
	u32 mask, base;

	pci_read_config_dword(dev, reg, &val);

	
	if (!(val & 1))
		return;

	base = val & 0xfffc;
	mask = (val >> 16) & 0xfc;
	mask |= 3;

	
	dev_info(&dev->dev, "%s PIO at %04x (mask %04x)\n", name, base, mask);
}

static void __devinit quirk_ich7_lpc(struct pci_dev *dev)
{
	
	ich6_lpc_acpi_gpio(dev);

	
	ich7_lpc_generic_decode(dev, 0x84, "ICH7 LPC Generic IO decode 1");
	ich7_lpc_generic_decode(dev, 0x88, "ICH7 LPC Generic IO decode 2");
	ich7_lpc_generic_decode(dev, 0x8c, "ICH7 LPC Generic IO decode 3");
	ich7_lpc_generic_decode(dev, 0x90, "ICH7 LPC Generic IO decode 4");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH7_0, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH7_1, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH7_31, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_0, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_2, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_3, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_1, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH8_4, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_2, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_4, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_7, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH9_8, quirk_ich7_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,   PCI_DEVICE_ID_INTEL_ICH10_1, quirk_ich7_lpc);

static void __devinit quirk_vt82c586_acpi(struct pci_dev *dev)
{
	u32 region;

	if (dev->revision & 0x10) {
		pci_read_config_dword(dev, 0x48, &region);
		region &= PCI_BASE_ADDRESS_IO_MASK;
		quirk_io_region(dev, region, 256, PCI_BRIDGE_RESOURCES, "vt82c586 ACPI");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_3,	quirk_vt82c586_acpi);

static void __devinit quirk_vt82c686_acpi(struct pci_dev *dev)
{
	u16 hm;
	u32 smb;

	quirk_vt82c586_acpi(dev);

	pci_read_config_word(dev, 0x70, &hm);
	hm &= PCI_BASE_ADDRESS_IO_MASK;
	quirk_io_region(dev, hm, 128, PCI_BRIDGE_RESOURCES + 1, "vt82c686 HW-mon");

	pci_read_config_dword(dev, 0x90, &smb);
	smb &= PCI_BASE_ADDRESS_IO_MASK;
	quirk_io_region(dev, smb, 16, PCI_BRIDGE_RESOURCES + 2, "vt82c686 SMB");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_4,	quirk_vt82c686_acpi);

static void __devinit quirk_vt8235_acpi(struct pci_dev *dev)
{
	u16 pm, smb;

	pci_read_config_word(dev, 0x88, &pm);
	pm &= PCI_BASE_ADDRESS_IO_MASK;
	quirk_io_region(dev, pm, 128, PCI_BRIDGE_RESOURCES, "vt8235 PM");

	pci_read_config_word(dev, 0xd0, &smb);
	smb &= PCI_BASE_ADDRESS_IO_MASK;
	quirk_io_region(dev, smb, 16, PCI_BRIDGE_RESOURCES + 1, "vt8235 SMB");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8235,	quirk_vt8235_acpi);

static void __devinit quirk_xio2000a(struct pci_dev *dev)
{
	struct pci_dev *pdev;
	u16 command;

	dev_warn(&dev->dev, "TI XIO2000a quirk detected; "
		"secondary bus fast back-to-back transfers disabled\n");
	list_for_each_entry(pdev, &dev->subordinate->devices, bus_list) {
		pci_read_config_word(pdev, PCI_COMMAND, &command);
		if (command & PCI_COMMAND_FAST_BACK)
			pci_write_config_word(pdev, PCI_COMMAND, command & ~PCI_COMMAND_FAST_BACK);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_TI, PCI_DEVICE_ID_TI_XIO2000A,
			quirk_xio2000a);

#ifdef CONFIG_X86_IO_APIC 

#include <asm/io_apic.h>

static void quirk_via_ioapic(struct pci_dev *dev)
{
	u8 tmp;
	
	if (nr_ioapics < 1)
		tmp = 0;    
	else
		tmp = 0x1f; 
		
	dev_info(&dev->dev, "%sbling VIA external APIC routing\n",
	       tmp == 0 ? "Disa" : "Ena");

	
	pci_write_config_byte (dev, 0x58, tmp);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_ioapic);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_ioapic);

static void quirk_via_vt8237_bypass_apic_deassert(struct pci_dev *dev)
{
	u8 misc_control2;
#define BYPASS_APIC_DEASSERT 8

	pci_read_config_byte(dev, 0x5B, &misc_control2);
	if (!(misc_control2 & BYPASS_APIC_DEASSERT)) {
		dev_info(&dev->dev, "Bypassing VIA 8237 APIC De-Assert Message\n");
		pci_write_config_byte(dev, 0x5B, misc_control2|BYPASS_APIC_DEASSERT);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237,		quirk_via_vt8237_bypass_apic_deassert);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237,		quirk_via_vt8237_bypass_apic_deassert);

static void __devinit quirk_amd_ioapic(struct pci_dev *dev)
{
	if (dev->revision >= 0x02) {
		dev_warn(&dev->dev, "I/O APIC: AMD Erratum #22 may be present. In the event of instability try\n");
		dev_warn(&dev->dev, "        : booting with the \"noapic\" option\n");
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_VIPER_7410,	quirk_amd_ioapic);

static void __init quirk_ioapic_rmw(struct pci_dev *dev)
{
	if (dev->devfn == 0 && dev->bus->number == 0)
		sis_apic_bug = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SI,	PCI_ANY_ID,			quirk_ioapic_rmw);
#endif 

static void __init quirk_amd_8131_mmrbc(struct pci_dev *dev)
{
	if (dev->subordinate && dev->revision <= 0x12) {
		dev_info(&dev->dev, "AMD8131 rev %x detected; "
			"disabling PCI-X MMRBC\n", dev->revision);
		dev->subordinate->bus_flags |= PCI_BUS_FLAGS_NO_MMRBC;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8131_BRIDGE, quirk_amd_8131_mmrbc);

static void __devinit quirk_via_acpi(struct pci_dev *d)
{
	u8 irq;
	pci_read_config_byte(d, 0x42, &irq);
	irq &= 0xf;
	if (irq && (irq != 2))
		d->irq = irq;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C586_3,	quirk_via_acpi);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686_4,	quirk_via_acpi);



static int via_vlink_dev_lo = -1, via_vlink_dev_hi = 18;

static void quirk_via_bridge(struct pci_dev *dev)
{
	
	switch (dev->device) {
	case PCI_DEVICE_ID_VIA_82C686:
		via_vlink_dev_lo = PCI_SLOT(dev->devfn);
		via_vlink_dev_hi = PCI_SLOT(dev->devfn);
		break;
	case PCI_DEVICE_ID_VIA_8237:
	case PCI_DEVICE_ID_VIA_8237A:
		via_vlink_dev_lo = 15;
		break;
	case PCI_DEVICE_ID_VIA_8235:
		via_vlink_dev_lo = 16;
		break;
	case PCI_DEVICE_ID_VIA_8231:
	case PCI_DEVICE_ID_VIA_8233_0:
	case PCI_DEVICE_ID_VIA_8233A:
	case PCI_DEVICE_ID_VIA_8233C_0:
		via_vlink_dev_lo = 17;
		break;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C686,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8231,		quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8233_0,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8233A,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8233C_0,	quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8235,		quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237,		quirk_via_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237A,	quirk_via_bridge);

/**
 *	quirk_via_vlink		-	VIA VLink IRQ number update
 *	@dev: PCI device
 *
 *	If the device we are dealing with is on a PIC IRQ we need to
 *	ensure that the IRQ line register which usually is not relevant
 *	for PCI cards, is actually written so that interrupts get sent
 *	to the right place.
 *	We only do this on systems where a VIA south bridge was detected,
 *	and only for VIA devices on the motherboard (see quirk_via_bridge
 *	above).
 */

static void quirk_via_vlink(struct pci_dev *dev)
{
	u8 irq, new_irq;

	
	if (via_vlink_dev_lo == -1)
		return;

	new_irq = dev->irq;

	
	if (!new_irq || new_irq > 15)
		return;

	
	if (dev->bus->number != 0 || PCI_SLOT(dev->devfn) > via_vlink_dev_hi ||
	    PCI_SLOT(dev->devfn) < via_vlink_dev_lo)
		return;


	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	if (new_irq != irq) {
		dev_info(&dev->dev, "VIA VLink IRQ fixup, from %d to %d\n",
			irq, new_irq);
		udelay(15);	
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, new_irq);
	}
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_VIA, PCI_ANY_ID, quirk_via_vlink);

static void __devinit quirk_vt82c598_id(struct pci_dev *dev)
{
	pci_write_config_byte(dev, 0xfc, 0);
	pci_read_config_word(dev, PCI_DEVICE_ID, &dev->device);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_82C597_0,	quirk_vt82c598_id);

static void quirk_cardbus_legacy(struct pci_dev *dev)
{
	pci_write_config_dword(dev, PCI_CB_LEGACY_MODE_BASE, 0);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_ANY_ID, PCI_ANY_ID,
			PCI_CLASS_BRIDGE_CARDBUS, 8, quirk_cardbus_legacy);
DECLARE_PCI_FIXUP_CLASS_RESUME_EARLY(PCI_ANY_ID, PCI_ANY_ID,
			PCI_CLASS_BRIDGE_CARDBUS, 8, quirk_cardbus_legacy);

static void quirk_amd_ordering(struct pci_dev *dev)
{
	u32 pcic;
	pci_read_config_dword(dev, 0x4C, &pcic);
	if ((pcic&6)!=6) {
		pcic |= 6;
		dev_warn(&dev->dev, "BIOS failed to enable PCI standards compliance; fixing this error\n");
		pci_write_config_dword(dev, 0x4C, pcic);
		pci_read_config_dword(dev, 0x84, &pcic);
		pcic |= (1<<23);	
		pci_write_config_dword(dev, 0x84, pcic);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_FE_GATE_700C, quirk_amd_ordering);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_FE_GATE_700C, quirk_amd_ordering);

static void __devinit quirk_dunord ( struct pci_dev * dev )
{
	struct resource *r = &dev->resource [1];
	r->start = 0;
	r->end = 0xffffff;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_DUNORD,	PCI_DEVICE_ID_DUNORD_I3000,	quirk_dunord);

static void __devinit quirk_transparent_bridge(struct pci_dev *dev)
{
	dev->transparent = 1;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82380FB,	quirk_transparent_bridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TOSHIBA,	0x605,	quirk_transparent_bridge);

static void quirk_mediagx_master(struct pci_dev *dev)
{
	u8 reg;
	pci_read_config_byte(dev, 0x41, &reg);
	if (reg & 2) {
		reg &= ~2;
		dev_info(&dev->dev, "Fixup for MediaGX/Geode Slave Disconnect Boundary (0x41=0x%02x)\n", reg);
                pci_write_config_byte(dev, 0x41, reg);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_CYRIX,	PCI_DEVICE_ID_CYRIX_PCI_MASTER, quirk_mediagx_master);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_CYRIX,	PCI_DEVICE_ID_CYRIX_PCI_MASTER, quirk_mediagx_master);

static void quirk_disable_pxb(struct pci_dev *pdev)
{
	u16 config;
	
	if (pdev->revision != 0x04)		
		return;
	pci_read_config_word(pdev, 0x40, &config);
	if (config & (1<<6)) {
		config &= ~(1<<6);
		pci_write_config_word(pdev, 0x40, config);
		dev_info(&pdev->dev, "C0 revision 450NX. Disabling PCI restreaming\n");
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82454NX,	quirk_disable_pxb);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82454NX,	quirk_disable_pxb);

static void __devinit quirk_amd_ide_mode(struct pci_dev *pdev)
{
	
	u8 tmp;

	pci_read_config_byte(pdev, PCI_CLASS_DEVICE, &tmp);
	if (tmp == 0x01) {
		pci_read_config_byte(pdev, 0x40, &tmp);
		pci_write_config_byte(pdev, 0x40, tmp|1);
		pci_write_config_byte(pdev, 0x9, 1);
		pci_write_config_byte(pdev, 0xa, 6);
		pci_write_config_byte(pdev, 0x40, tmp);

		pdev->class = PCI_CLASS_STORAGE_SATA_AHCI;
		dev_info(&pdev->dev, "set SATA to AHCI mode\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP600_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP600_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP700_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_IXP700_SATA, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_HUDSON2_SATA_IDE, quirk_amd_ide_mode);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_HUDSON2_SATA_IDE, quirk_amd_ide_mode);

static void __devinit quirk_svwks_csb5ide(struct pci_dev *pdev)
{
	u8 prog;
	pci_read_config_byte(pdev, PCI_CLASS_PROG, &prog);
	if (prog & 5) {
		prog &= ~5;
		pdev->class &= ~5;
		pci_write_config_byte(pdev, PCI_CLASS_PROG, prog);
		
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_CSB5IDE, quirk_svwks_csb5ide);

static void __init quirk_ide_samemode(struct pci_dev *pdev)
{
	u8 prog;

	pci_read_config_byte(pdev, PCI_CLASS_PROG, &prog);

	if (((prog & 1) && !(prog & 4)) || ((prog & 4) && !(prog & 1))) {
		dev_info(&pdev->dev, "IDE mode mismatch; forcing legacy mode\n");
		prog &= ~5;
		pdev->class &= ~5;
		pci_write_config_byte(pdev, PCI_CLASS_PROG, prog);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801CA_10, quirk_ide_samemode);


static void __devinit quirk_no_ata_d3(struct pci_dev *pdev)
{
	pdev->dev_flags |= PCI_DEV_FLAGS_NO_D3;
}
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_SERVERWORKS, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_ATI, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_AL, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_VIA, PCI_ANY_ID,
				PCI_CLASS_STORAGE_IDE, 8, quirk_no_ata_d3);

static void __init quirk_eisa_bridge(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_EISA << 8;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82375,	quirk_eisa_bridge);


static int asus_hides_smbus;

static void __init asus_hides_smbus_hostbridge(struct pci_dev *dev)
{
	if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_ASUSTEK)) {
		if (dev->device == PCI_DEVICE_ID_INTEL_82845_HB)
			switch(dev->subsystem_device) {
			case 0x8025: 
			case 0x8070: 
			case 0x8088: 
			case 0x1626: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82845G_HB)
			switch(dev->subsystem_device) {
			case 0x80b1: 
			case 0x80b2: 
			case 0x8093: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82850_HB)
			switch(dev->subsystem_device) {
			case 0x8030: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_7205_0)
			switch (dev->subsystem_device) {
			case 0x8070: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_E7501_MCH)
			switch (dev->subsystem_device) {
			case 0x80c9: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82855GM_HB)
			switch (dev->subsystem_device) {
			case 0x1751: 
			case 0x1821: 
			case 0x1897: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch (dev->subsystem_device) {
			case 0x184b: 
			case 0x186a: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82865_HB)
			switch (dev->subsystem_device) {
			case 0x80f2: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82915GM_HB)
			switch (dev->subsystem_device) {
			case 0x1882: 
			case 0x1977: 
				asus_hides_smbus = 1;
			}
	} else if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_HP)) {
		if (dev->device ==  PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch(dev->subsystem_device) {
			case 0x088C: 
			case 0x0890: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82865_HB)
			switch (dev->subsystem_device) {
			case 0x12bc: 
			case 0x12bd: 
			case 0x006a: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82875_HB)
			switch (dev->subsystem_device) {
			case 0x12bf: 
				asus_hides_smbus = 1;
			}
       } else if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_SAMSUNG)) {
               if (dev->device ==  PCI_DEVICE_ID_INTEL_82855PM_HB)
                       switch(dev->subsystem_device) {
                       case 0xC00C: 
                               asus_hides_smbus = 1;
                       }
	} else if (unlikely(dev->subsystem_vendor == PCI_VENDOR_ID_COMPAQ)) {
		if (dev->device == PCI_DEVICE_ID_INTEL_82855PM_HB)
			switch(dev->subsystem_device) {
			case 0x0058: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82810_IG3)
			switch(dev->subsystem_device) {
			case 0xB16C: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82801DB_2)
			switch(dev->subsystem_device) {
			case 0x00b8: 
			case 0x00b9: 
			case 0x00ba: 
				asus_hides_smbus = 1;
			}
		else if (dev->device == PCI_DEVICE_ID_INTEL_82815_CGC)
			switch (dev->subsystem_device) {
			case 0x001A: 
				asus_hides_smbus = 1;
			}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82845_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82845G_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82850_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82865_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82875_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_7205_0,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7501_MCH,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82855PM_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82855GM_HB,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82915GM_HB, asus_hides_smbus_hostbridge);

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82810_IG3,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_2,	asus_hides_smbus_hostbridge);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82815_CGC,	asus_hides_smbus_hostbridge);

static void asus_hides_smbus_lpc(struct pci_dev *dev)
{
	u16 val;
	
	if (likely(!asus_hides_smbus))
		return;

	pci_read_config_word(dev, 0xF2, &val);
	if (val & 0x8) {
		pci_write_config_word(dev, 0xF2, val & (~0x8));
		pci_read_config_word(dev, 0xF2, &val);
		if (val & 0x8)
			dev_info(&dev->dev, "i801 SMBus device continues to play 'hide and seek'! 0x%x\n", val);
		else
			dev_info(&dev->dev, "Enabled i801 SMBus device\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801AA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801BA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801EB_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801AA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801BA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_0,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801CA_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801DB_12,	asus_hides_smbus_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_82801EB_0,	asus_hides_smbus_lpc);

static void __iomem *asus_rcba_base;
static void asus_hides_smbus_lpc_ich6_suspend(struct pci_dev *dev)
{
	u32 rcba;

	if (likely(!asus_hides_smbus))
		return;
	WARN_ON(asus_rcba_base);

	pci_read_config_dword(dev, 0xF0, &rcba);
	
	asus_rcba_base = ioremap_nocache(rcba & 0xFFFFC000, 0x4000);
	if (asus_rcba_base == NULL)
		return;
}

static void asus_hides_smbus_lpc_ich6_resume_early(struct pci_dev *dev)
{
	u32 val;

	if (likely(!asus_hides_smbus || !asus_rcba_base))
		return;
	
	val = readl(asus_rcba_base + 0x3418);
	writel(val & 0xFFFFFFF7, asus_rcba_base + 0x3418); 
}

static void asus_hides_smbus_lpc_ich6_resume(struct pci_dev *dev)
{
	if (likely(!asus_hides_smbus || !asus_rcba_base))
		return;
	iounmap(asus_rcba_base);
	asus_rcba_base = NULL;
	dev_info(&dev->dev, "Enabled ICH6/i801 SMBus device\n");
}

static void asus_hides_smbus_lpc_ich6(struct pci_dev *dev)
{
	asus_hides_smbus_lpc_ich6_suspend(dev);
	asus_hides_smbus_lpc_ich6_resume_early(dev);
	asus_hides_smbus_lpc_ich6_resume(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6);
DECLARE_PCI_FIXUP_SUSPEND(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6_suspend);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6_resume);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ICH6_1,	asus_hides_smbus_lpc_ich6_resume_early);

static void quirk_sis_96x_smbus(struct pci_dev *dev)
{
	u8 val = 0;
	pci_read_config_byte(dev, 0x77, &val);
	if (val & 0x10) {
		dev_info(&dev->dev, "Enabling SiS 96x SMBus\n");
		pci_write_config_byte(dev, 0x77, val & ~0x10);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_961,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_962,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_963,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_LPC,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_961,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_962,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_963,		quirk_sis_96x_smbus);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_LPC,		quirk_sis_96x_smbus);

#define SIS_DETECT_REGISTER 0x40

static void quirk_sis_503(struct pci_dev *dev)
{
	u8 reg;
	u16 devid;

	pci_read_config_byte(dev, SIS_DETECT_REGISTER, &reg);
	pci_write_config_byte(dev, SIS_DETECT_REGISTER, reg | (1 << 6));
	pci_read_config_word(dev, PCI_DEVICE_ID, &devid);
	if (((devid & 0xfff0) != 0x0960) && (devid != 0x0018)) {
		pci_write_config_byte(dev, SIS_DETECT_REGISTER, reg);
		return;
	}

	dev->device = devid;
	quirk_sis_96x_smbus(dev);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_503,		quirk_sis_503);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_SI,	PCI_DEVICE_ID_SI_503,		quirk_sis_503);


static void asus_hides_ac97_lpc(struct pci_dev *dev)
{
	u8 val;
	int asus_hides_ac97 = 0;

	if (likely(dev->subsystem_vendor == PCI_VENDOR_ID_ASUSTEK)) {
		if (dev->device == PCI_DEVICE_ID_VIA_8237)
			asus_hides_ac97 = 1;
	}

	if (!asus_hides_ac97)
		return;

	pci_read_config_byte(dev, 0x50, &val);
	if (val & 0xc0) {
		pci_write_config_byte(dev, 0x50, val & (~0xc0));
		pci_read_config_byte(dev, 0x50, &val);
		if (val & 0xc0)
			dev_info(&dev->dev, "Onboard AC97/MC97 devices continue to play 'hide and seek'! 0x%x\n", val);
		else
			dev_info(&dev->dev, "Enabled onboard AC97/MC97 devices\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237, asus_hides_ac97_lpc);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_VIA,	PCI_DEVICE_ID_VIA_8237, asus_hides_ac97_lpc);

#if defined(CONFIG_ATA) || defined(CONFIG_ATA_MODULE)

static void quirk_jmicron_ata(struct pci_dev *pdev)
{
	u32 conf1, conf5, class;
	u8 hdr;

	
	if (PCI_FUNC(pdev->devfn))
		return;

	pci_read_config_dword(pdev, 0x40, &conf1);
	pci_read_config_dword(pdev, 0x80, &conf5);

	conf1 &= ~0x00CFF302; 
	conf5 &= ~(1 << 24);  

	switch (pdev->device) {
	case PCI_DEVICE_ID_JMICRON_JMB360: 
	case PCI_DEVICE_ID_JMICRON_JMB362: 
	case PCI_DEVICE_ID_JMICRON_JMB364: 
		
		conf1 |= 0x0002A100; 
		break;

	case PCI_DEVICE_ID_JMICRON_JMB365:
	case PCI_DEVICE_ID_JMICRON_JMB366:
		
		conf5 |= (1 << 24);
		
	case PCI_DEVICE_ID_JMICRON_JMB361:
	case PCI_DEVICE_ID_JMICRON_JMB363:
	case PCI_DEVICE_ID_JMICRON_JMB369:
		
		
		conf1 |= 0x00C2A1B3; 
		break;

	case PCI_DEVICE_ID_JMICRON_JMB368:
		
		conf1 |= 0x00C00000; 
		break;
	}

	pci_write_config_dword(pdev, 0x40, conf1);
	pci_write_config_dword(pdev, 0x80, conf5);

	
	pci_read_config_byte(pdev, PCI_HEADER_TYPE, &hdr);
	pdev->hdr_type = hdr & 0x7f;
	pdev->multifunction = !!(hdr & 0x80);

	pci_read_config_dword(pdev, PCI_CLASS_REVISION, &class);
	pdev->class = class >> 8;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB360, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB361, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB362, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB363, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB364, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB365, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB366, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB368, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB369, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB360, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB361, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB362, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB363, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB364, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB365, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB366, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB368, quirk_jmicron_ata);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_JMICRON, PCI_DEVICE_ID_JMICRON_JMB369, quirk_jmicron_ata);

#endif

#ifdef CONFIG_X86_IO_APIC
static void __init quirk_alder_ioapic(struct pci_dev *pdev)
{
	int i;

	if ((pdev->class >> 8) != 0xff00)
		return;

	if (pci_resource_start(pdev, 0) && pci_resource_len(pdev, 0))
		insert_resource(&iomem_resource, &pdev->resource[0]);

	for (i=1; i < 6; i++) {
		memset(&pdev->resource[i], 0, sizeof(pdev->resource[i]));
	}

}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_EESSC,	quirk_alder_ioapic);
#endif

static void __devinit quirk_pcie_mch(struct pci_dev *pdev)
{
	pci_msi_off(pdev);
	pdev->no_msi = 1;
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7520_MCH,	quirk_pcie_mch);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7320_MCH,	quirk_pcie_mch);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_E7525_MCH,	quirk_pcie_mch);


static void __devinit quirk_pcie_pxh(struct pci_dev *dev)
{
	pci_msi_off(dev);
	dev->no_msi = 1;
	dev_warn(&dev->dev, "PXH quirk detected; SHPC device MSI disabled\n");
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHD_0,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHD_1,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_0,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_1,	quirk_pcie_pxh);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHV,	quirk_pcie_pxh);

static void quirk_intel_pcie_pm(struct pci_dev * dev)
{
	pci_pm_d3_delay = 120;
	dev->no_d1d2 = 1;
}

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e2, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e3, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e4, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e5, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e6, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25e7, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25f7, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25f8, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25f9, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x25fa, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2601, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2602, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2603, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2604, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2605, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2606, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2607, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2608, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x2609, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x260a, quirk_intel_pcie_pm);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x260b, quirk_intel_pcie_pm);

#ifdef CONFIG_X86_IO_APIC
static void quirk_reroute_to_boot_interrupts_intel(struct pci_dev *dev)
{
	if (noioapicquirk || noioapicreroute)
		return;

	dev->irq_reroute_variant = INTEL_IRQ_REROUTE_VARIANT;
	dev_info(&dev->dev, "rerouting interrupts for [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ESB2_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHV,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80333_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_ESB2_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXH_1,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_PXHV,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_0,	quirk_reroute_to_boot_interrupts_intel);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,	PCI_DEVICE_ID_INTEL_80332_1,	quirk_reroute_to_boot_interrupts_intel);


#define INTEL_6300_IOAPIC_ABAR		0x40
#define INTEL_6300_DISABLE_BOOT_IRQ	(1<<14)

static void quirk_disable_intel_boot_interrupt(struct pci_dev *dev)
{
	u16 pci_config_word;

	if (noioapicquirk)
		return;

	pci_read_config_word(dev, INTEL_6300_IOAPIC_ABAR, &pci_config_word);
	pci_config_word |= INTEL_6300_DISABLE_BOOT_IRQ;
	pci_write_config_word(dev, INTEL_6300_IOAPIC_ABAR, pci_config_word);

	dev_info(&dev->dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,   PCI_DEVICE_ID_INTEL_ESB_10, 	quirk_disable_intel_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL,   PCI_DEVICE_ID_INTEL_ESB_10, 	quirk_disable_intel_boot_interrupt);

#define BC_HT1000_FEATURE_REG		0x64
#define BC_HT1000_PIC_REGS_ENABLE	(1<<0)
#define BC_HT1000_MAP_IDX		0xC00
#define BC_HT1000_MAP_DATA		0xC01

static void quirk_disable_broadcom_boot_interrupt(struct pci_dev *dev)
{
	u32 pci_config_dword;
	u8 irq;

	if (noioapicquirk)
		return;

	pci_read_config_dword(dev, BC_HT1000_FEATURE_REG, &pci_config_dword);
	pci_write_config_dword(dev, BC_HT1000_FEATURE_REG, pci_config_dword |
			BC_HT1000_PIC_REGS_ENABLE);

	for (irq = 0x10; irq < 0x10 + 32; irq++) {
		outb(irq, BC_HT1000_MAP_IDX);
		outb(0x00, BC_HT1000_MAP_DATA);
	}

	pci_write_config_dword(dev, BC_HT1000_FEATURE_REG, pci_config_dword);

	dev_info(&dev->dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SERVERWORKS,   PCI_DEVICE_ID_SERVERWORKS_HT1000SB, 	quirk_disable_broadcom_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_SERVERWORKS,   PCI_DEVICE_ID_SERVERWORKS_HT1000SB, 	quirk_disable_broadcom_boot_interrupt);

#define AMD_813X_MISC			0x40
#define AMD_813X_NOIOAMODE		(1<<0)
#define AMD_813X_REV_B1			0x12
#define AMD_813X_REV_B2			0x13

static void quirk_disable_amd_813x_boot_interrupt(struct pci_dev *dev)
{
	u32 pci_config_dword;

	if (noioapicquirk)
		return;
	if ((dev->revision == AMD_813X_REV_B1) ||
	    (dev->revision == AMD_813X_REV_B2))
		return;

	pci_read_config_dword(dev, AMD_813X_MISC, &pci_config_dword);
	pci_config_dword &= ~AMD_813X_NOIOAMODE;
	pci_write_config_dword(dev, AMD_813X_MISC, pci_config_dword);

	dev_info(&dev->dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8131_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8131_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8132_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AMD,	PCI_DEVICE_ID_AMD_8132_BRIDGE,	quirk_disable_amd_813x_boot_interrupt);

#define AMD_8111_PCI_IRQ_ROUTING	0x56

static void quirk_disable_amd_8111_boot_interrupt(struct pci_dev *dev)
{
	u16 pci_config_word;

	if (noioapicquirk)
		return;

	pci_read_config_word(dev, AMD_8111_PCI_IRQ_ROUTING, &pci_config_word);
	if (!pci_config_word) {
		dev_info(&dev->dev, "boot interrupts on device [%04x:%04x] "
			 "already disabled\n", dev->vendor, dev->device);
		return;
	}
	pci_write_config_word(dev, AMD_8111_PCI_IRQ_ROUTING, 0);
	dev_info(&dev->dev, "disabled boot interrupts on device [%04x:%04x]\n",
		 dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD,   PCI_DEVICE_ID_AMD_8111_SMBUS, 	quirk_disable_amd_8111_boot_interrupt);
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_AMD,   PCI_DEVICE_ID_AMD_8111_SMBUS, 	quirk_disable_amd_8111_boot_interrupt);
#endif 

static void __init quirk_tc86c001_ide(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[0];

	if (r->start & 0x8) {
		r->start = 0;
		r->end = 0xf;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_TOSHIBA_2,
			 PCI_DEVICE_ID_TOSHIBA_TC86C001_IDE,
			 quirk_tc86c001_ide);

static void __devinit quirk_netmos(struct pci_dev *dev)
{
	unsigned int num_parallel = (dev->subsystem_device & 0xf0) >> 4;
	unsigned int num_serial = dev->subsystem_device & 0xf;

	switch (dev->device) {
	case PCI_DEVICE_ID_NETMOS_9835:
		
		if (dev->subsystem_vendor == PCI_VENDOR_ID_IBM &&
				dev->subsystem_device == 0x0299)
			return;
	case PCI_DEVICE_ID_NETMOS_9735:
	case PCI_DEVICE_ID_NETMOS_9745:
	case PCI_DEVICE_ID_NETMOS_9845:
	case PCI_DEVICE_ID_NETMOS_9855:
		if (num_parallel) {
			dev_info(&dev->dev, "Netmos %04x (%u parallel, "
				"%u serial); changing class SERIAL to OTHER "
				"(use parport_serial)\n",
				dev->device, num_parallel, num_serial);
			dev->class = (PCI_CLASS_COMMUNICATION_OTHER << 8) |
			    (dev->class & 0xff);
		}
	}
}
DECLARE_PCI_FIXUP_CLASS_HEADER(PCI_VENDOR_ID_NETMOS, PCI_ANY_ID,
			 PCI_CLASS_COMMUNICATION_SERIAL, 8, quirk_netmos);

static void __devinit quirk_e100_interrupt(struct pci_dev *dev)
{
	u16 command, pmcsr;
	u8 __iomem *csr;
	u8 cmd_hi;
	int pm;

	switch (dev->device) {
	
	case 0x1029:
	case 0x1030 ... 0x1034:
	case 0x1038 ... 0x103E:
	case 0x1050 ... 0x1057:
	case 0x1059:
	case 0x1064 ... 0x106B:
	case 0x1091 ... 0x1095:
	case 0x1209:
	case 0x1229:
	case 0x2449:
	case 0x2459:
	case 0x245D:
	case 0x27DC:
		break;
	default:
		return;
	}

	pci_read_config_word(dev, PCI_COMMAND, &command);

	if (!(command & PCI_COMMAND_MEMORY) || !pci_resource_start(dev, 0))
		return;

	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	if (pm) {
		pci_read_config_word(dev, pm + PCI_PM_CTRL, &pmcsr);
		if ((pmcsr & PCI_PM_CTRL_STATE_MASK) != PCI_D0)
			return;
	}

	
	csr = ioremap(pci_resource_start(dev, 0), 8);
	if (!csr) {
		dev_warn(&dev->dev, "Can't map e100 registers\n");
		return;
	}

	cmd_hi = readb(csr + 3);
	if (cmd_hi == 0) {
		dev_warn(&dev->dev, "Firmware left e100 interrupts enabled; "
			"disabling\n");
		writeb(1, csr + 3);
	}

	iounmap(csr);
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_INTEL, PCI_ANY_ID,
			PCI_CLASS_NETWORK_ETHERNET, 8, quirk_e100_interrupt);

static void __devinit quirk_disable_aspm_l0s(struct pci_dev *dev)
{
	dev_info(&dev->dev, "Disabling L0s\n");
	pci_disable_link_state(dev, PCIE_LINK_STATE_L0S);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10a7, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10a9, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10b6, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10c6, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10c7, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10c8, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10d6, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10db, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10dd, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10e1, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10ec, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10f1, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x10f4, quirk_disable_aspm_l0s);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1508, quirk_disable_aspm_l0s);

static void __devinit fixup_rev1_53c810(struct pci_dev* dev)
{

	if (dev->class == PCI_CLASS_NOT_DEFINED) {
		dev_info(&dev->dev, "NCR 53c810 rev 1 detected; setting PCI class\n");
		dev->class = PCI_CLASS_STORAGE_SCSI;
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_NCR, PCI_DEVICE_ID_NCR_53C810, fixup_rev1_53c810);

static void __devinit quirk_p64h2_1k_io(struct pci_dev *dev)
{
	u16 en1k;
	u8 io_base_lo, io_limit_lo;
	unsigned long base, limit;
	struct resource *res = dev->resource + PCI_BRIDGE_RESOURCES;

	pci_read_config_word(dev, 0x40, &en1k);

	if (en1k & 0x200) {
		dev_info(&dev->dev, "Enable I/O Space to 1KB granularity\n");

		pci_read_config_byte(dev, PCI_IO_BASE, &io_base_lo);
		pci_read_config_byte(dev, PCI_IO_LIMIT, &io_limit_lo);
		base = (io_base_lo & (PCI_IO_RANGE_MASK | 0x0c)) << 8;
		limit = (io_limit_lo & (PCI_IO_RANGE_MASK | 0x0c)) << 8;

		if (base <= limit) {
			res->start = base;
			res->end = limit + 0x3ff;
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL,	0x1460,		quirk_p64h2_1k_io);

/* Fix the IOBL_ADR for 1k I/O space granularity on the Intel P64H2
 * The IOBL_ADR gets re-written to 4k boundaries in pci_setup_bridge()
 * in drivers/pci/setup-bus.c
 */
static void __devinit quirk_p64h2_1k_io_fix_iobl(struct pci_dev *dev)
{
	u16 en1k, iobl_adr, iobl_adr_1k;
	struct resource *res = dev->resource + PCI_BRIDGE_RESOURCES;

	pci_read_config_word(dev, 0x40, &en1k);

	if (en1k & 0x200) {
		pci_read_config_word(dev, PCI_IO_BASE, &iobl_adr);

		iobl_adr_1k = iobl_adr | (res->start >> 8) | (res->end & 0xfc00);

		if (iobl_adr != iobl_adr_1k) {
			dev_info(&dev->dev, "Fixing P64H2 IOBL_ADR from 0x%x to 0x%x for 1KB granularity\n",
				iobl_adr,iobl_adr_1k);
			pci_write_config_word(dev, PCI_IO_BASE, iobl_adr_1k);
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL,	0x1460,		quirk_p64h2_1k_io_fix_iobl);

static void quirk_nvidia_ck804_pcie_aer_ext_cap(struct pci_dev *dev)
{
	uint8_t b;
	if (pci_read_config_byte(dev, 0xf41, &b) == 0) {
		if (!(b & 0x20)) {
			pci_write_config_byte(dev, 0xf41, b | 0x20);
			dev_info(&dev->dev,
			       "Linking AER extended capability\n");
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA,  PCI_DEVICE_ID_NVIDIA_CK804_PCIE,
			quirk_nvidia_ck804_pcie_aer_ext_cap);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_NVIDIA,  PCI_DEVICE_ID_NVIDIA_CK804_PCIE,
			quirk_nvidia_ck804_pcie_aer_ext_cap);

static void __devinit quirk_via_cx700_pci_parking_caching(struct pci_dev *dev)
{

	
	struct pci_dev *p = pci_get_device(PCI_VENDOR_ID_VIA,
		PCI_DEVICE_ID_VIA_8235_USB_2, NULL);
	uint8_t b;

	p = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8235_USB_2, p);
	if (!p)
		return;
	pci_dev_put(p);

	if (pci_read_config_byte(dev, 0x76, &b) == 0) {
		if (b & 0x40) {
			
			pci_write_config_byte(dev, 0x76, b ^ 0x40);

			dev_info(&dev->dev,
				"Disabling VIA CX700 PCI parking\n");
		}
	}

	if (pci_read_config_byte(dev, 0x72, &b) == 0) {
		if (b != 0) {
			
			pci_write_config_byte(dev, 0x72, 0x0);

			
			pci_write_config_byte(dev, 0x75, 0x1);

			
			pci_write_config_byte(dev, 0x77, 0x0);

			dev_info(&dev->dev,
				"Disabling VIA CX700 PCI caching\n");
		}
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, 0x324e, quirk_via_cx700_pci_parking_caching);

static void __devinit quirk_brcm_570x_limit_vpd(struct pci_dev *dev)
{
	if ((dev->device == PCI_DEVICE_ID_NX2_5706) ||
	    (dev->device == PCI_DEVICE_ID_NX2_5706S) ||
	    (dev->device == PCI_DEVICE_ID_NX2_5708) ||
	    (dev->device == PCI_DEVICE_ID_NX2_5708S) ||
	    ((dev->device == PCI_DEVICE_ID_NX2_5709) &&
	     (dev->revision & 0xf0) == 0x0)) {
		if (dev->vpd)
			dev->vpd->len = 0x80;
	}
}

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_NX2_5706,
			quirk_brcm_570x_limit_vpd);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_NX2_5706S,
			quirk_brcm_570x_limit_vpd);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_NX2_5708,
			quirk_brcm_570x_limit_vpd);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_NX2_5708S,
			quirk_brcm_570x_limit_vpd);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_NX2_5709,
			quirk_brcm_570x_limit_vpd);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_NX2_5709S,
			quirk_brcm_570x_limit_vpd);

static void __devinit quirk_brcm_5719_limit_mrrs(struct pci_dev *dev)
{
	u32 rev;

	pci_read_config_dword(dev, 0xf4, &rev);

	
	if (rev == 0x05719000) {
		int readrq = pcie_get_readrq(dev);
		if (readrq > 2048)
			pcie_set_readrq(dev, 2048);
	}
}

DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_BROADCOM,
			 PCI_DEVICE_ID_TIGON3_5719,
			 quirk_brcm_5719_limit_mrrs);

static void __devinit quirk_unhide_mch_dev6(struct pci_dev *dev)
{
	u8 reg;

	if (pci_read_config_byte(dev, 0xF4, &reg) == 0 && !(reg & 0x02)) {
		dev_info(&dev->dev, "Enabling MCH 'Overflow' Device\n");
		pci_write_config_byte(dev, 0xF4, reg | 0x02);
	}
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82865_HB,
			quirk_unhide_mch_dev6);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82875_HB,
			quirk_unhide_mch_dev6);

#ifdef CONFIG_TILE
static void __devinit quirk_tile_plx_gen1(struct pci_dev *dev)
{
	if (tile_plx_gen1) {
		pci_write_config_dword(dev, 0x98, 0x1);
		mdelay(50);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PLX, 0x8624, quirk_tile_plx_gen1);
#endif 

#ifdef CONFIG_PCI_MSI
static void __init quirk_disable_all_msi(struct pci_dev *dev)
{
	pci_no_msi();
	dev_warn(&dev->dev, "MSI quirk detected; MSI disabled\n");
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_GCNB_LE, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RS400_200, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_RS480, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VT3336, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VT3351, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_VT3364, quirk_disable_all_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8380_0, quirk_disable_all_msi);

static void __devinit quirk_disable_msi(struct pci_dev *dev)
{
	if (dev->subordinate) {
		dev_warn(&dev->dev, "MSI quirk detected; "
			"subordinate MSI disabled\n");
		dev->subordinate->bus_flags |= PCI_BUS_FLAGS_NO_MSI;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8131_BRIDGE, quirk_disable_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_VIA, 0xa238, quirk_disable_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x5a3f, quirk_disable_msi);

static void __devinit quirk_amd_780_apc_msi(struct pci_dev *host_bridge)
{
	struct pci_dev *apc_bridge;

	apc_bridge = pci_get_slot(host_bridge->bus, PCI_DEVFN(1, 0));
	if (apc_bridge) {
		if (apc_bridge->device == 0x9602)
			quirk_disable_msi(apc_bridge);
		pci_dev_put(apc_bridge);
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x9600, quirk_amd_780_apc_msi);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AMD, 0x9601, quirk_amd_780_apc_msi);

static int __devinit msi_ht_cap_enabled(struct pci_dev *dev)
{
	int pos, ttl = 48;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0)
		{
			dev_info(&dev->dev, "Found %s HT MSI Mapping\n",
				flags & HT_MSI_FLAGS_ENABLE ?
				"enabled" : "disabled");
			return (flags & HT_MSI_FLAGS_ENABLE) != 0;
		}

		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}
	return 0;
}

static void __devinit quirk_msi_ht_cap(struct pci_dev *dev)
{
	if (dev->subordinate && !msi_ht_cap_enabled(dev)) {
		dev_warn(&dev->dev, "MSI quirk detected; "
			"subordinate MSI disabled\n");
		dev->subordinate->bus_flags |= PCI_BUS_FLAGS_NO_MSI;
	}
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_HT2000_PCIE,
			quirk_msi_ht_cap);

static void __devinit quirk_nvidia_ck804_msi_ht_cap(struct pci_dev *dev)
{
	struct pci_dev *pdev;

	if (!dev->subordinate)
		return;

	pdev = pci_get_slot(dev->bus, 0);
	if (!pdev)
		return;
	if (!msi_ht_cap_enabled(dev) && !msi_ht_cap_enabled(pdev)) {
		dev_warn(&dev->dev, "MSI quirk detected; "
			"subordinate MSI disabled\n");
		dev->subordinate->bus_flags |= PCI_BUS_FLAGS_NO_MSI;
	}
	pci_dev_put(pdev);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_CK804_PCIE,
			quirk_nvidia_ck804_msi_ht_cap);

static void __devinit ht_enable_msi_mapping(struct pci_dev *dev)
{
	int pos, ttl = 48;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			dev_info(&dev->dev, "Enabling HT MSI Mapping\n");

			pci_write_config_byte(dev, pos + HT_MSI_FLAGS,
					      flags | HT_MSI_FLAGS_ENABLE);
		}
		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SERVERWORKS,
			 PCI_DEVICE_ID_SERVERWORKS_HT1000_PXB,
			 ht_enable_msi_mapping);

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8132_BRIDGE,
			 ht_enable_msi_mapping);

static void __devinit nvenet_msi_disable(struct pci_dev *dev)
{
	const char *board_name = dmi_get_system_info(DMI_BOARD_NAME);

	if (board_name &&
	    (strstr(board_name, "P5N32-SLI PREMIUM") ||
	     strstr(board_name, "P5N32-E SLI"))) {
		dev_info(&dev->dev,
			 "Disabling msi for MCP55 NIC on P5N32-SLI\n");
		dev->no_msi = 1;
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA,
			PCI_DEVICE_ID_NVIDIA_NVENET_15,
			nvenet_msi_disable);

static void __devinit nvbridge_check_legacy_irq_routing(struct pci_dev *dev)
{
	u32 cfg;

	if (!pci_find_capability(dev, PCI_CAP_ID_HT))
		return;

	pci_read_config_dword(dev, 0x74, &cfg);

	if (cfg & ((1 << 2) | (1 << 15))) {
		printk(KERN_INFO "Rewriting irq routing register on MCP55\n");
		cfg &= ~((1 << 2) | (1 << 15));
		pci_write_config_dword(dev, 0x74, cfg);
	}
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA,
			PCI_DEVICE_ID_NVIDIA_MCP55_BRIDGE_V0,
			nvbridge_check_legacy_irq_routing);

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_NVIDIA,
			PCI_DEVICE_ID_NVIDIA_MCP55_BRIDGE_V4,
			nvbridge_check_legacy_irq_routing);

static int __devinit ht_check_msi_mapping(struct pci_dev *dev)
{
	int pos, ttl = 48;
	int found = 0;

	
	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (found < 1)
			found = 1;
		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			if (flags & HT_MSI_FLAGS_ENABLE) {
				if (found < 2) {
					found = 2;
					break;
				}
			}
		}
		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}

	return found;
}

static int __devinit host_bridge_with_leaf(struct pci_dev *host_bridge)
{
	struct pci_dev *dev;
	int pos;
	int i, dev_no;
	int found = 0;

	dev_no = host_bridge->devfn >> 3;
	for (i = dev_no + 1; i < 0x20; i++) {
		dev = pci_get_slot(host_bridge->bus, PCI_DEVFN(i, 0));
		if (!dev)
			continue;

		
		pos = pci_find_ht_capability(dev, HT_CAPTYPE_SLAVE);
		if (pos != 0) {
			pci_dev_put(dev);
			break;
		}

		if (ht_check_msi_mapping(dev)) {
			found = 1;
			pci_dev_put(dev);
			break;
		}
		pci_dev_put(dev);
	}

	return found;
}

#define PCI_HT_CAP_SLAVE_CTRL0     4    
#define PCI_HT_CAP_SLAVE_CTRL1     8    

static int __devinit is_end_of_ht_chain(struct pci_dev *dev)
{
	int pos, ctrl_off;
	int end = 0;
	u16 flags, ctrl;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_SLAVE);

	if (!pos)
		goto out;

	pci_read_config_word(dev, pos + PCI_CAP_FLAGS, &flags);

	ctrl_off = ((flags >> 10) & 1) ?
			PCI_HT_CAP_SLAVE_CTRL0 : PCI_HT_CAP_SLAVE_CTRL1;
	pci_read_config_word(dev, pos + ctrl_off, &ctrl);

	if (ctrl & (1 << 6))
		end = 1;

out:
	return end;
}

static void __devinit nv_ht_enable_msi_mapping(struct pci_dev *dev)
{
	struct pci_dev *host_bridge;
	int pos;
	int i, dev_no;
	int found = 0;

	dev_no = dev->devfn >> 3;
	for (i = dev_no; i >= 0; i--) {
		host_bridge = pci_get_slot(dev->bus, PCI_DEVFN(i, 0));
		if (!host_bridge)
			continue;

		pos = pci_find_ht_capability(host_bridge, HT_CAPTYPE_SLAVE);
		if (pos != 0) {
			found = 1;
			break;
		}
		pci_dev_put(host_bridge);
	}

	if (!found)
		return;

	
	if (host_bridge == dev && is_end_of_ht_chain(host_bridge) &&
	    host_bridge_with_leaf(host_bridge))
		goto out;

	
	if (msi_ht_cap_enabled(host_bridge))
		goto out;

	ht_enable_msi_mapping(dev);

out:
	pci_dev_put(host_bridge);
}

static void __devinit ht_disable_msi_mapping(struct pci_dev *dev)
{
	int pos, ttl = 48;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_MSI_MAPPING);
	while (pos && ttl--) {
		u8 flags;

		if (pci_read_config_byte(dev, pos + HT_MSI_FLAGS,
					 &flags) == 0) {
			dev_info(&dev->dev, "Disabling HT MSI Mapping\n");

			pci_write_config_byte(dev, pos + HT_MSI_FLAGS,
					      flags & ~HT_MSI_FLAGS_ENABLE);
		}
		pos = pci_find_next_ht_capability(dev, pos,
						  HT_CAPTYPE_MSI_MAPPING);
	}
}

static void __devinit __nv_msi_ht_cap_quirk(struct pci_dev *dev, int all)
{
	struct pci_dev *host_bridge;
	int pos;
	int found;

	if (!pci_msi_enabled())
		return;

	
	found = ht_check_msi_mapping(dev);

	
	if (found == 0)
		return;

	host_bridge = pci_get_bus_and_slot(0, PCI_DEVFN(0, 0));
	if (host_bridge == NULL) {
		dev_warn(&dev->dev,
			 "nv_msi_ht_cap_quirk didn't locate host bridge\n");
		return;
	}

	pos = pci_find_ht_capability(host_bridge, HT_CAPTYPE_SLAVE);
	if (pos != 0) {
		
		if (found == 1) {
			
			if (all)
				ht_enable_msi_mapping(dev);
			else
				nv_ht_enable_msi_mapping(dev);
		}
		return;
	}

	
	if (found == 1)
		return;

	
	ht_disable_msi_mapping(dev);
}

static void __devinit nv_msi_ht_cap_quirk_all(struct pci_dev *dev)
{
	return __nv_msi_ht_cap_quirk(dev, 1);
}

static void __devinit nv_msi_ht_cap_quirk_leaf(struct pci_dev *dev)
{
	return __nv_msi_ht_cap_quirk(dev, 0);
}

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID, nv_msi_ht_cap_quirk_leaf);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID, nv_msi_ht_cap_quirk_leaf);

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_AL, PCI_ANY_ID, nv_msi_ht_cap_quirk_all);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_AL, PCI_ANY_ID, nv_msi_ht_cap_quirk_all);

static void __devinit quirk_msi_intx_disable_bug(struct pci_dev *dev)
{
	dev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
}
static void __devinit quirk_msi_intx_disable_ati_bug(struct pci_dev *dev)
{
	struct pci_dev *p;

	p = pci_get_device(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_SBX00_SMBUS,
			   NULL);
	if (!p)
		return;

	if ((p->revision < 0x3B) && (p->revision >= 0x30))
		dev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
	pci_dev_put(p);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5780,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5780S,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5714,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5714S,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5715,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_BROADCOM,
			PCI_DEVICE_ID_TIGON3_5715S,
			quirk_msi_intx_disable_bug);

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4390,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4391,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4392,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4393,
			quirk_msi_intx_disable_ati_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4394,
			quirk_msi_intx_disable_ati_bug);

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4373,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4374,
			quirk_msi_intx_disable_bug);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_ATI, 0x4375,
			quirk_msi_intx_disable_bug);

#endif 

static void __devinit quirk_hotplug_bridge(struct pci_dev *dev)
{
	dev->is_hotplug_bridge = 1;
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_HINT, 0x0020, quirk_hotplug_bridge);

/*
 * This is a quirk for the Ricoh MMC controller found as a part of
 * some mulifunction chips.

 * This is very similar and based on the ricoh_mmc driver written by
 * Philip Langdale. Thank you for these magic sequences.
 *
 * These chips implement the four main memory card controllers (SD, MMC, MS, xD)
 * and one or both of cardbus or firewire.
 *
 * It happens that they implement SD and MMC
 * support as separate controllers (and PCI functions). The linux SDHCI
 * driver supports MMC cards but the chip detects MMC cards in hardware
 * and directs them to the MMC controller - so the SDHCI driver never sees
 * them.
 *
 * To get around this, we must disable the useless MMC controller.
 * At that point, the SDHCI controller will start seeing them
 * It seems to be the case that the relevant PCI registers to deactivate the
 * MMC controller live on PCI function 0, which might be the cardbus controller
 * or the firewire controller, depending on the particular chip in question
 *
 * This has to be done early, because as soon as we disable the MMC controller
 * other pci functions shift up one level, e.g. function #2 becomes function
 * #1, and this will confuse the pci core.
 */

#ifdef CONFIG_MMC_RICOH_MMC
static void ricoh_mmc_fixup_rl5c476(struct pci_dev *dev)
{
	
	u8 write_enable;
	u8 write_target;
	u8 disable;

	
	if (PCI_FUNC(dev->devfn))
		return;

	pci_read_config_byte(dev, 0xB7, &disable);
	if (disable & 0x02)
		return;

	pci_read_config_byte(dev, 0x8E, &write_enable);
	pci_write_config_byte(dev, 0x8E, 0xAA);
	pci_read_config_byte(dev, 0x8D, &write_target);
	pci_write_config_byte(dev, 0x8D, 0xB7);
	pci_write_config_byte(dev, 0xB7, disable | 0x02);
	pci_write_config_byte(dev, 0x8E, write_enable);
	pci_write_config_byte(dev, 0x8D, write_target);

	dev_notice(&dev->dev, "proprietary Ricoh MMC controller disabled (via cardbus function)\n");
	dev_notice(&dev->dev, "MMC cards are now supported by standard SDHCI controller\n");
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_RL5C476, ricoh_mmc_fixup_rl5c476);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_RL5C476, ricoh_mmc_fixup_rl5c476);

static void ricoh_mmc_fixup_r5c832(struct pci_dev *dev)
{
	
	u8 write_enable;
	u8 disable;

	
	if (PCI_FUNC(dev->devfn))
		return;
	if (dev->device == PCI_DEVICE_ID_RICOH_R5CE823) {
		pci_write_config_byte(dev, 0xf9, 0xfc);
		pci_write_config_byte(dev, 0x150, 0x10);
		pci_write_config_byte(dev, 0xf9, 0x00);
		pci_write_config_byte(dev, 0xfc, 0x01);
		pci_write_config_byte(dev, 0xe1, 0x32);
		pci_write_config_byte(dev, 0xfc, 0x00);

		dev_notice(&dev->dev, "MMC controller base frequency changed to 50Mhz.\n");
	}

	pci_read_config_byte(dev, 0xCB, &disable);

	if (disable & 0x02)
		return;

	pci_read_config_byte(dev, 0xCA, &write_enable);
	pci_write_config_byte(dev, 0xCA, 0x57);
	pci_write_config_byte(dev, 0xCB, disable | 0x02);
	pci_write_config_byte(dev, 0xCA, write_enable);

	dev_notice(&dev->dev, "proprietary Ricoh MMC controller disabled (via firewire function)\n");
	dev_notice(&dev->dev, "MMC cards are now supported by standard SDHCI controller\n");

}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5C832, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5C832, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5CE823, ricoh_mmc_fixup_r5c832);
DECLARE_PCI_FIXUP_RESUME_EARLY(PCI_VENDOR_ID_RICOH, PCI_DEVICE_ID_RICOH_R5CE823, ricoh_mmc_fixup_r5c832);
#endif 

#ifdef CONFIG_DMAR_TABLE
#define VTUNCERRMSK_REG	0x1ac
#define VTD_MSK_SPEC_ERRORS	(1 << 31)
static void vtd_mask_spec_errors(struct pci_dev *dev)
{
	u32 word;

	pci_read_config_dword(dev, VTUNCERRMSK_REG, &word);
	pci_write_config_dword(dev, VTUNCERRMSK_REG, word | VTD_MSK_SPEC_ERRORS);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x342e, vtd_mask_spec_errors);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x3c28, vtd_mask_spec_errors);
#endif

static void __devinit fixup_ti816x_class(struct pci_dev* dev)
{
	
	dev_info(&dev->dev, "Setting PCI class for 816x PCIe device\n");
	dev->class = PCI_CLASS_MULTIMEDIA_VIDEO;
}
DECLARE_PCI_FIXUP_CLASS_EARLY(PCI_VENDOR_ID_TI, 0xb800,
				 PCI_CLASS_NOT_DEFINED, 0, fixup_ti816x_class);

static void __devinit fixup_mpss_256(struct pci_dev *dev)
{
	dev->pcie_mpss = 1; 
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SOLARFLARE,
			 PCI_DEVICE_ID_SOLARFLARE_SFC4000A_0, fixup_mpss_256);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SOLARFLARE,
			 PCI_DEVICE_ID_SOLARFLARE_SFC4000A_1, fixup_mpss_256);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_SOLARFLARE,
			 PCI_DEVICE_ID_SOLARFLARE_SFC4000B, fixup_mpss_256);

static void __devinit quirk_intel_mc_errata(struct pci_dev *dev)
{
	int err;
	u16 rcc;

	if (pcie_bus_config == PCIE_BUS_TUNE_OFF)
		return;

	err = pci_read_config_word(dev, 0x48, &rcc);
	if (err) {
		dev_err(&dev->dev, "Error attempting to read the read "
			"completion coalescing register.\n");
		return;
	}

	if (!(rcc & (1 << 10)))
		return;

	rcc &= ~(1 << 10);

	err = pci_write_config_word(dev, 0x48, rcc);
	if (err) {
		dev_err(&dev->dev, "Error attempting to write the read "
			"completion coalescing register.\n");
		return;
	}

	pr_info_once("Read completion coalescing disabled due to hardware "
		     "errata relating to 256B MPS.\n");
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25c0, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25d0, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25d4, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25d8, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e2, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e3, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e4, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e5, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e6, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25e7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25f7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25f8, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25f9, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x25fa, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65c0, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e2, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e3, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e4, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e5, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e6, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65e7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65f7, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65f8, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65f9, quirk_intel_mc_errata);
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_INTEL, 0x65fa, quirk_intel_mc_errata);


static void do_one_fixup_debug(void (*fn)(struct pci_dev *dev), struct pci_dev *dev)
{
	ktime_t calltime, delta, rettime;
	unsigned long long duration;

	printk(KERN_DEBUG "calling  %pF @ %i for %s\n",
			fn, task_pid_nr(current), dev_name(&dev->dev));
	calltime = ktime_get();
	fn(dev);
	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	duration = (unsigned long long) ktime_to_ns(delta) >> 10;
	printk(KERN_DEBUG "pci fixup %pF returned after %lld usecs for %s\n",
			fn, duration, dev_name(&dev->dev));
}

#define I915_DEIER_REG 0x4400c
static void __devinit disable_igfx_irq(struct pci_dev *dev)
{
	void __iomem *regs = pci_iomap(dev, 0, 0);
	if (regs == NULL) {
		dev_warn(&dev->dev, "igfx quirk: Can't iomap PCI device\n");
		return;
	}

	
	if (readl(regs + I915_DEIER_REG) != 0) {
		dev_warn(&dev->dev, "BIOS left Intel GPU interrupts enabled; "
			"disabling\n");

		writel(0, regs + I915_DEIER_REG);
	}

	pci_iounmap(dev, regs);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x0102, disable_igfx_irq);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x010a, disable_igfx_irq);

static void pci_do_fixups(struct pci_dev *dev, struct pci_fixup *f,
			  struct pci_fixup *end)
{
	for (; f < end; f++)
		if ((f->class == (u32) (dev->class >> f->class_shift) ||
		     f->class == (u32) PCI_ANY_ID) &&
		    (f->vendor == dev->vendor ||
		     f->vendor == (u16) PCI_ANY_ID) &&
		    (f->device == dev->device ||
		     f->device == (u16) PCI_ANY_ID)) {
			dev_dbg(&dev->dev, "calling %pF\n", f->hook);
			if (initcall_debug)
				do_one_fixup_debug(f->hook, dev);
			else
				f->hook(dev);
		}
}

extern struct pci_fixup __start_pci_fixups_early[];
extern struct pci_fixup __end_pci_fixups_early[];
extern struct pci_fixup __start_pci_fixups_header[];
extern struct pci_fixup __end_pci_fixups_header[];
extern struct pci_fixup __start_pci_fixups_final[];
extern struct pci_fixup __end_pci_fixups_final[];
extern struct pci_fixup __start_pci_fixups_enable[];
extern struct pci_fixup __end_pci_fixups_enable[];
extern struct pci_fixup __start_pci_fixups_resume[];
extern struct pci_fixup __end_pci_fixups_resume[];
extern struct pci_fixup __start_pci_fixups_resume_early[];
extern struct pci_fixup __end_pci_fixups_resume_early[];
extern struct pci_fixup __start_pci_fixups_suspend[];
extern struct pci_fixup __end_pci_fixups_suspend[];


void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev)
{
	struct pci_fixup *start, *end;

	switch(pass) {
	case pci_fixup_early:
		start = __start_pci_fixups_early;
		end = __end_pci_fixups_early;
		break;

	case pci_fixup_header:
		start = __start_pci_fixups_header;
		end = __end_pci_fixups_header;
		break;

	case pci_fixup_final:
		start = __start_pci_fixups_final;
		end = __end_pci_fixups_final;
		break;

	case pci_fixup_enable:
		start = __start_pci_fixups_enable;
		end = __end_pci_fixups_enable;
		break;

	case pci_fixup_resume:
		start = __start_pci_fixups_resume;
		end = __end_pci_fixups_resume;
		break;

	case pci_fixup_resume_early:
		start = __start_pci_fixups_resume_early;
		end = __end_pci_fixups_resume_early;
		break;

	case pci_fixup_suspend:
		start = __start_pci_fixups_suspend;
		end = __end_pci_fixups_suspend;
		break;

	default:
		
		return;
	}
	pci_do_fixups(dev, start, end);
}
EXPORT_SYMBOL(pci_fixup_device);

static int __init pci_apply_final_quirks(void)
{
	struct pci_dev *dev = NULL;
	u8 cls = 0;
	u8 tmp;

	if (pci_cache_line_size)
		printk(KERN_DEBUG "PCI: CLS %u bytes\n",
		       pci_cache_line_size << 2);

	for_each_pci_dev(dev) {
		pci_fixup_device(pci_fixup_final, dev);
		if (!pci_cache_line_size) {
			pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &tmp);
			if (!cls)
				cls = tmp;
			if (!tmp || cls == tmp)
				continue;

			printk(KERN_DEBUG "PCI: CLS mismatch (%u != %u), "
			       "using %u bytes\n", cls << 2, tmp << 2,
			       pci_dfl_cache_line_size << 2);
			pci_cache_line_size = pci_dfl_cache_line_size;
		}
	}
	if (!pci_cache_line_size) {
		printk(KERN_DEBUG "PCI: CLS %u bytes, default %u\n",
		       cls << 2, pci_dfl_cache_line_size << 2);
		pci_cache_line_size = cls ? cls : pci_dfl_cache_line_size;
	}

	return 0;
}

fs_initcall_sync(pci_apply_final_quirks);

static int reset_intel_generic_dev(struct pci_dev *dev, int probe)
{
	int pos;

	
	if (dev->class == PCI_CLASS_SERIAL_USB) {
		pos = pci_find_capability(dev, PCI_CAP_ID_VNDR);
		if (!pos)
			return -ENOTTY;

		if (probe)
			return 0;

		pci_write_config_byte(dev, pos + 0x4, 1);
		msleep(100);

		return 0;
	} else {
		return -ENOTTY;
	}
}

static int reset_intel_82599_sfp_virtfn(struct pci_dev *dev, int probe)
{
	int pos;

	pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (!pos)
		return -ENOTTY;

	if (probe)
		return 0;

	pci_write_config_word(dev, pos + PCI_EXP_DEVCTL,
				PCI_EXP_DEVCTL_BCR_FLR);
	msleep(100);

	return 0;
}

#define PCI_DEVICE_ID_INTEL_82599_SFP_VF   0x10ed

static const struct pci_dev_reset_methods pci_dev_reset_methods[] = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82599_SFP_VF,
		 reset_intel_82599_sfp_virtfn },
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID,
		reset_intel_generic_dev },
	{ 0 }
};

int pci_dev_specific_reset(struct pci_dev *dev, int probe)
{
	const struct pci_dev_reset_methods *i;

	for (i = pci_dev_reset_methods; i->reset; i++) {
		if ((i->vendor == dev->vendor ||
		     i->vendor == (u16)PCI_ANY_ID) &&
		    (i->device == dev->device ||
		     i->device == (u16)PCI_ANY_ID))
			return i->reset(dev, probe);
	}

	return -ENOTTY;
}
