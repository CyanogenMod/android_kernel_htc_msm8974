#ifndef __ASM_MIPS_GT64120_H
#define __ASM_MIPS_GT64120_H


#define WRPPMC_SDRAM_SCS0_BASE	0x00000000
#define WRPPMC_SDRAM_SCS0_SIZE	0x04000000

#define WRPPMC_UART16550_BASE	0x1C800000
#define WRPPMC_UART16550_CLOCK	3686400 

#define WRPPMC_LED_BASE		0x1C000000
#define WRPPMC_MBOX_BASE	0x1F000000

#define WRPPMC_BOOTROM_BASE	0x1FC00000
#define WRPPMC_BOOTROM_SIZE	0x00400000 

#define WRPPMC_MIPS_TIMER_IRQ	7 
#define WRPPMC_UART16550_IRQ	6
#define WRPPMC_PCI_INTA_IRQ	3

#define GT_PCI_MEM_BASE	0x13000000UL
#define GT_PCI_MEM_SIZE	0x02000000UL
#define GT_PCI_IO_BASE	0x11000000UL
#define GT_PCI_IO_SIZE	0x02000000UL

#define GT_TIMER	4
#define GT_INTA		2
#define GT_INTD		5

#ifndef __ASSEMBLY__

extern unsigned long gt64120_base;

#define GT64120_BASE	(gt64120_base)

#undef WRPPMC_EARLY_DEBUG

#ifdef WRPPMC_EARLY_DEBUG
extern void wrppmc_led_on(int mask);
extern void wrppmc_led_off(int mask);
extern void wrppmc_early_printk(const char *fmt, ...);
#else
#define wrppmc_early_printk(fmt, ...) do {} while (0)
#endif 

#endif 
#endif 
