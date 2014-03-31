/*
 * pci_dn.c
 *
 * Copyright (C) 2001 Todd Inglett, IBM Corporation
 *
 * PCI manipulation via device_nodes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/gfp.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/ppc-pci.h>
#include <asm/firmware.h>

void * __devinit update_dn_pci_info(struct device_node *dn, void *data)
{
	struct pci_controller *phb = data;
	const int *type =
		of_get_property(dn, "ibm,pci-config-space-type", NULL);
	const u32 *regs;
	struct pci_dn *pdn;

	pdn = zalloc_maybe_bootmem(sizeof(*pdn), GFP_KERNEL);
	if (pdn == NULL)
		return NULL;
	dn->data = pdn;
	pdn->node = dn;
	pdn->phb = phb;
#ifdef CONFIG_PPC_POWERNV
	pdn->pe_number = IODA_INVALID_PE;
#endif
	regs = of_get_property(dn, "reg", NULL);
	if (regs) {
		
		pdn->busno = (regs[0] >> 16) & 0xff;
		pdn->devfn = (regs[0] >> 8) & 0xff;
	}

	pdn->pci_ext_config_space = (type && *type == 1);
	return NULL;
}

void *traverse_pci_devices(struct device_node *start, traverse_func pre,
		void *data)
{
	struct device_node *dn, *nextdn;
	void *ret;

	
	for (dn = start->child; dn; dn = nextdn) {
		const u32 *classp;
		u32 class;

		nextdn = NULL;
		classp = of_get_property(dn, "class-code", NULL);
		class = classp ? *classp : 0;

		if (pre && ((ret = pre(dn, data)) != NULL))
			return ret;

		
		if (dn->child && ((class >> 8) == PCI_CLASS_BRIDGE_PCI ||
				  (class >> 8) == PCI_CLASS_BRIDGE_CARDBUS))
			
			nextdn = dn->child;
		else if (dn->sibling)
			
			nextdn = dn->sibling;
		if (!nextdn) {
			
			do {
				dn = dn->parent;
				if (dn == start)
					return NULL;
			} while (dn->sibling == NULL);
			nextdn = dn->sibling;
		}
	}
	return NULL;
}

void __devinit pci_devs_phb_init_dynamic(struct pci_controller *phb)
{
	struct device_node *dn = phb->dn;
	struct pci_dn *pdn;

	
	update_dn_pci_info(dn, phb);
	pdn = dn->data;
	if (pdn) {
		pdn->devfn = pdn->busno = -1;
		pdn->phb = phb;
	}

	
	traverse_pci_devices(dn, update_dn_pci_info, phb);
}

void __init pci_devs_phb_init(void)
{
	struct pci_controller *phb, *tmp;

	
	list_for_each_entry_safe(phb, tmp, &hose_list, list_node)
		pci_devs_phb_init_dynamic(phb);
}
