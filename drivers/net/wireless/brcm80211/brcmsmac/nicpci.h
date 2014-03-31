/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_BRCM_NICPCI_H_
#define	_BRCM_NICPCI_H_

#include "types.h"

#define PCI_SZPCR		256

#define PCI_BAR0_WIN		0x80
#define PCI_SPROM_CONTROL	0x88
#define PCI_INT_MASK		0x94
#define  PCI_SBIM_SHIFT		8
#define PCI_BAR0_WIN2		0xac
#define PCI_GPIO_IN		0xb0
#define PCI_GPIO_OUT		0xb4
#define PCI_GPIO_OUTEN		0xb8

#define PCI_BAR0_SPROM_OFFSET	(4 * 1024)
#define PCI_BAR0_PCIREGS_OFFSET	(6 * 1024)
#define PCI_BAR0_PCISBR_OFFSET	(4 * 1024)
#define PCI_BAR0_WINSZ		(16 * 1024)
#define PCI_16KB0_PCIREGS_OFFSET (8 * 1024)
#define PCI_16KB0_CCREGS_OFFSET	(12 * 1024)

struct sbpciregs;
struct sbpcieregs;

extern struct pcicore_info *pcicore_init(struct si_pub *sih,
					 struct bcma_device *core);
extern void pcicore_deinit(struct pcicore_info *pch);
extern void pcicore_attach(struct pcicore_info *pch, int state);
extern void pcicore_hwup(struct pcicore_info *pch);
extern void pcicore_up(struct pcicore_info *pch, int state);
extern void pcicore_sleep(struct pcicore_info *pch);
extern void pcicore_down(struct pcicore_info *pch, int state);
extern u8 pcicore_find_pci_capability(struct pci_dev *dev, u8 req_cap_id,
				      unsigned char *buf, u32 *buflen);
extern void pcicore_fixcfg(struct pcicore_info *pch);
extern void pcicore_pci_setup(struct pcicore_info *pch);

#endif 
