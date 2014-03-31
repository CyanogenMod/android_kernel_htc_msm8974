/*
 * linux/include/arm/hardware/it8152.h
 *
 * Copyright Compulab Ltd., 2006,2007
 * Mike Rapoport <mike@compulab.co.il>
 *
 * ITE 8152 companion chip register definitions
 */

#ifndef __ASM_HARDWARE_IT8152_H
#define __ASM_HARDWARE_IT8152_H

#include <mach/irqs.h>

extern void __iomem *it8152_base_address;

#define IT8152_IO_BASE			(it8152_base_address + 0x03e00000)
#define IT8152_CFGREG_BASE		(it8152_base_address + 0x03f00000)

#define __REG_IT8152(x)			(it8152_base_address + (x))

#define IT8152_PCI_CFG_ADDR		__REG_IT8152(0x3f00800)
#define IT8152_PCI_CFG_DATA		__REG_IT8152(0x3f00804)

#define IT8152_INTC_LDCNIRR		__REG_IT8152(0x3f00300)
#define IT8152_INTC_LDPNIRR		__REG_IT8152(0x3f00304)
#define IT8152_INTC_LDCNIMR		__REG_IT8152(0x3f00308)
#define IT8152_INTC_LDPNIMR		__REG_IT8152(0x3f0030C)
#define IT8152_INTC_LDNITR		__REG_IT8152(0x3f00310)
#define IT8152_INTC_LDNIAR		__REG_IT8152(0x3f00314)
#define IT8152_INTC_LPCNIRR		__REG_IT8152(0x3f00320)
#define IT8152_INTC_LPPNIRR		__REG_IT8152(0x3f00324)
#define IT8152_INTC_LPCNIMR		__REG_IT8152(0x3f00328)
#define IT8152_INTC_LPPNIMR		__REG_IT8152(0x3f0032C)
#define IT8152_INTC_LPNITR		__REG_IT8152(0x3f00330)
#define IT8152_INTC_LPNIAR		__REG_IT8152(0x3f00334)
#define IT8152_INTC_PDCNIRR		__REG_IT8152(0x3f00340)
#define IT8152_INTC_PDPNIRR		__REG_IT8152(0x3f00344)
#define IT8152_INTC_PDCNIMR		__REG_IT8152(0x3f00348)
#define IT8152_INTC_PDPNIMR		__REG_IT8152(0x3f0034C)
#define IT8152_INTC_PDNITR		__REG_IT8152(0x3f00350)
#define IT8152_INTC_PDNIAR		__REG_IT8152(0x3f00354)
#define IT8152_INTC_INTC_TYPER		__REG_IT8152(0x3f003FC)

#define IT8152_GPIO_GPDR		__REG_IT8152(0x3f00500)

#define IT8152_IRQ(x)   (IRQ_BOARD_START + (x))
#define IT8152_LAST_IRQ	(IRQ_BOARD_START + 40)

#define IT8152_LD_IRQ_COUNT     9
#define IT8152_LP_IRQ_COUNT     16
#define IT8152_PD_IRQ_COUNT     15

#define IT8152_PD_IRQ(i)        IT8152_IRQ(i)
#define IT8152_LP_IRQ(i)        (IT8152_IRQ(i) + IT8152_PD_IRQ_COUNT)
#define IT8152_LD_IRQ(i)        (IT8152_IRQ(i) + IT8152_PD_IRQ_COUNT + IT8152_LP_IRQ_COUNT)

#define IT8152_PCISERR		IT8152_PD_IRQ(14)
#define IT8152_H2PTADR		IT8152_PD_IRQ(13)
#define IT8152_H2PMAR		IT8152_PD_IRQ(12)
#define IT8152_PCI_INTD		IT8152_PD_IRQ(11)
#define IT8152_PCI_INTC		IT8152_PD_IRQ(10)
#define IT8152_PCI_INTB		IT8152_PD_IRQ(9)
#define IT8152_PCI_INTA		IT8152_PD_IRQ(8)
#define IT8152_CDMA_INT		IT8152_PD_IRQ(2)
#define IT8152_USB_INT		IT8152_PD_IRQ(1)
#define IT8152_AUDIO_INT	IT8152_PD_IRQ(0)

struct pci_dev;
struct pci_sys_data;

extern void it8152_irq_demux(unsigned int irq, struct irq_desc *desc);
extern void it8152_init_irq(void);
extern int it8152_pci_map_irq(const struct pci_dev *dev, u8 slot, u8 pin);
extern int it8152_pci_setup(int nr, struct pci_sys_data *sys);
extern struct pci_bus *it8152_pci_scan_bus(int nr, struct pci_sys_data *sys);

#endif 
