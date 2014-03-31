#ifndef _ASM_X86_VISWS_COBALT_H
#define _ASM_X86_VISWS_COBALT_H

#include <asm/fixmap.h>

 

#define CO_CPU_NUM_PHYS 0x1e00
#define CO_CPU_TAB_PHYS (CO_CPU_NUM_PHYS + 2)

#define CO_CPU_MAX 4

#define	CO_CPU_PHYS		0xc2000000
#define	CO_APIC_PHYS		0xc4000000

#define	CO_CPU_VADDR		(fix_to_virt(FIX_CO_CPU))
#define	CO_APIC_VADDR		(fix_to_virt(FIX_CO_APIC))

#define	CO_CPU_REV		0x08
#define	CO_CPU_CTRL		0x10
#define	CO_CPU_STAT		0x20
#define	CO_CPU_TIMEVAL		0x30

#define	CO_CTRL_TIMERUN		0x04		
#define	CO_CTRL_TIMEMASK	0x08		

#define	CO_STAT_TIMEINTR	0x02	

#define	CO_TIME_HZ		100000000	

#define	CO_APIC_HI(n)		(((n) * 0x10) + 4)
#define	CO_APIC_LO(n)		((n) * 0x10)
#define	CO_APIC_ID		0x0ffc

#define	CO_APIC_ENABLE		0x00000100

#define	CO_APIC_MASK		0x00010000	
#define	CO_APIC_LEVEL		0x00008000	

#define	CO_APIC_IDE0		4
#define CO_APIC_IDE1		2		

#define	CO_APIC_8259		12		

#define	CO_APIC_PCIA_BASE0	0 	
#define	CO_APIC_PCIA_BASE123	5 	

#define	CO_APIC_PIIX4_USB	7		

#define	CO_APIC_PCIB_BASE0	8 
#define	CO_APIC_PCIB_BASE123	13 	

#define	CO_APIC_VIDOUT0		16
#define	CO_APIC_VIDOUT1		17
#define	CO_APIC_VIDIN0		18
#define	CO_APIC_VIDIN1		19

#define	CO_APIC_LI_AUDIO	22

#define	CO_APIC_AS		24
#define	CO_APIC_RE		25

#define CO_APIC_CPU		28		
#define	CO_APIC_NMI		29
#define	CO_APIC_LAST		CO_APIC_NMI

#define	CO_IRQ_APIC0	16			
#define	IS_CO_APIC(irq)	((irq) >= CO_IRQ_APIC0)
#define	CO_IRQ(apic)	(CO_IRQ_APIC0 + (apic))	
#define	CO_APIC(irq)	((irq) - CO_IRQ_APIC0)	
#define CO_IRQ_IDE0	14			
#define CO_IRQ_IDE1	15			
#define	CO_IRQ_8259	CO_IRQ(CO_APIC_8259)

#ifdef CONFIG_X86_VISWS_APIC
static inline void co_cpu_write(unsigned long reg, unsigned long v)
{
	*((volatile unsigned long *)(CO_CPU_VADDR+reg))=v;
}

static inline unsigned long co_cpu_read(unsigned long reg)
{
	return *((volatile unsigned long *)(CO_CPU_VADDR+reg));
}            
             
static inline void co_apic_write(unsigned long reg, unsigned long v)
{
	*((volatile unsigned long *)(CO_APIC_VADDR+reg))=v;
}            
             
static inline unsigned long co_apic_read(unsigned long reg)
{
	return *((volatile unsigned long *)(CO_APIC_VADDR+reg));
}
#endif

extern char visws_board_type;

#define	VISWS_320	0
#define	VISWS_540	1

extern char visws_board_rev;

extern int pci_visws_init(void);

#endif 
