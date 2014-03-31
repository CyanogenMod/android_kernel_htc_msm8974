#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/mach/pci.h>
#include <asm/mach-types.h>

static int irqmap_cats[] __initdata = { IRQ_PCI, IRQ_IN0, IRQ_IN1, IRQ_IN3 };

static int __init cats_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	if (dev->irq >= 255)
		return -1;	

	if (dev->irq >= 128)
		return dev->irq & 0x1f;

	if (dev->irq >= 1 && dev->irq <= 4)
		return irqmap_cats[dev->irq - 1];

	if (dev->irq != 0)
		printk("PCI: device %02x:%02x has unknown irq line %x\n",
		       dev->bus->number, dev->devfn, dev->irq);

	return -1;
}

static struct hw_pci cats_pci __initdata = {
	.swizzle		= NULL,
	.map_irq		= cats_map_irq,
	.nr_controllers		= 1,
	.setup			= dc21285_setup,
	.scan			= dc21285_scan_bus,
	.preinit		= dc21285_preinit,
	.postinit		= dc21285_postinit,
};

static int __init cats_pci_init(void)
{
	if (machine_is_cats())
		pci_common_init(&cats_pci);
	return 0;
}

subsys_initcall(cats_pci_init);
