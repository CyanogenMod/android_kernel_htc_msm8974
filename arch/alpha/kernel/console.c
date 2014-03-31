
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/vt.h>
#include <asm/vga.h>
#include <asm/machvec.h>

#include "pci_impl.h"

#ifdef CONFIG_VGA_HOSE

struct pci_controller *pci_vga_hose;
static struct resource alpha_vga = {
	.name	= "alpha-vga+",
	.start	= 0x3C0,
	.end	= 0x3DF
};

static struct pci_controller * __init 
default_vga_hose_select(struct pci_controller *h1, struct pci_controller *h2)
{
	if (h2->index < h1->index)
		return h2;

	return h1;
}

void __init 
locate_and_init_vga(void *(*sel_func)(void *, void *))
{
	struct pci_controller *hose = NULL;
	struct pci_dev *dev = NULL;

	
	if (!sel_func) sel_func = (void *)default_vga_hose_select;

	
	for(dev=NULL; (dev=pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, dev));) {
		if (!hose)
			hose = dev->sysdata;
		else
			hose = sel_func(hose, dev->sysdata);
	}

	
	if (!hose || (conswitchp == &vga_con && pci_vga_hose == hose))
		return;

	
	alpha_vga.start += hose->io_space->start;
	alpha_vga.end += hose->io_space->start;
	request_resource(hose->io_space, &alpha_vga);

	
	pci_vga_hose = hose;
	take_over_console(&vga_con, 0, MAX_NR_CONSOLES-1, 1);
}

void __init
find_console_vga_hose(void)
{
	u64 *pu64 = (u64 *)((u64)hwrpb + hwrpb->ctbt_offset);

	if (pu64[7] == 3) {	
		struct pci_controller *hose;
		int h = (pu64[30] >> 24) & 0xff;	

		for (hose = hose_head; hose; hose = hose->next) {
			if (hose->index == h) break;
		}

		if (hose) {
			printk("Console graphics on hose %d\n", h);
			pci_vga_hose = hose;
		}
	}
}

#endif
