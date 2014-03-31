/*
 * The VGA aribiter manages VGA space routing and VGA resource decode to
 * allow multiple VGA devices to be used in a system in a safe way.
 *
 * (C) Copyright 2005 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 * (C) Copyright 2007 Paulo R. Zanoni <przanoni@gmail.com>
 * (C) Copyright 2007, 2009 Tiago Vignatti <vignatti@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef LINUX_VGA_H
#define LINUX_VGA_H


#define VGA_RSRC_NONE	       0x00
#define VGA_RSRC_LEGACY_IO     0x01
#define VGA_RSRC_LEGACY_MEM    0x02
#define VGA_RSRC_LEGACY_MASK   (VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM)
#define VGA_RSRC_NORMAL_IO     0x04
#define VGA_RSRC_NORMAL_MEM    0x08

#define VGA_DEFAULT_DEVICE     (NULL)

struct pci_dev;


extern void vga_set_legacy_decoding(struct pci_dev *pdev,
				    unsigned int decodes);


#if defined(CONFIG_VGA_ARB)
extern int vga_get(struct pci_dev *pdev, unsigned int rsrc, int interruptible);
#else
static inline int vga_get(struct pci_dev *pdev, unsigned int rsrc, int interruptible) { return 0; }
#endif


static inline int vga_get_interruptible(struct pci_dev *pdev,
					unsigned int rsrc)
{
       return vga_get(pdev, rsrc, 1);
}


static inline int vga_get_uninterruptible(struct pci_dev *pdev,
					  unsigned int rsrc)
{
       return vga_get(pdev, rsrc, 0);
}


#if defined(CONFIG_VGA_ARB)
extern int vga_tryget(struct pci_dev *pdev, unsigned int rsrc);
#else
static inline int vga_tryget(struct pci_dev *pdev, unsigned int rsrc) { return 0; }
#endif


#if defined(CONFIG_VGA_ARB)
extern void vga_put(struct pci_dev *pdev, unsigned int rsrc);
#else
#define vga_put(pdev, rsrc)
#endif



#ifndef __ARCH_HAS_VGA_DEFAULT_DEVICE
extern struct pci_dev *vga_default_device(void);
#endif


#ifndef __ARCH_HAS_VGA_CONFLICT
static inline int vga_conflicts(struct pci_dev *p1, struct pci_dev *p2)
{
       return 1;
}
#endif

#if defined(CONFIG_VGA_ARB)
int vga_client_register(struct pci_dev *pdev, void *cookie,
			void (*irq_set_state)(void *cookie, bool state),
			unsigned int (*set_vga_decode)(void *cookie, bool state));
#else
static inline int vga_client_register(struct pci_dev *pdev, void *cookie,
				      void (*irq_set_state)(void *cookie, bool state),
				      unsigned int (*set_vga_decode)(void *cookie, bool state))
{
	return 0;
}
#endif

#endif 
