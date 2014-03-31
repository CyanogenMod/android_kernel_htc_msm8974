#ifndef _ASM_X86_VISWS_LITHIUM_H
#define _ASM_X86_VISWS_LITHIUM_H

#include <asm/fixmap.h>


#define	LI_PCI_A_PHYS		0xfc000000	
#define	LI_PCI_B_PHYS		0xfd000000	

#define LI_PCIA_VADDR   (fix_to_virt(FIX_LI_PCIA))
#define LI_PCIB_VADDR   (fix_to_virt(FIX_LI_PCIB))

#define	LI_PCI_BUSNUM	0x44			
#define LI_PCI_INTEN    0x46

#define	LI_INTA_0	0x0001
#define	LI_INTA_1	0x0002
#define	LI_INTA_2	0x0004
#define	LI_INTA_3	0x0008
#define	LI_INTA_4	0x0010
#define	LI_INTB		0x0020
#define	LI_INTC		0x0040
#define	LI_INTD		0x0080

static inline void li_pcia_write16(unsigned long reg, unsigned short v)
{
	*((volatile unsigned short *)(LI_PCIA_VADDR+reg))=v;
}

static inline unsigned short li_pcia_read16(unsigned long reg)
{
	 return *((volatile unsigned short *)(LI_PCIA_VADDR+reg));
}

static inline void li_pcib_write16(unsigned long reg, unsigned short v)
{
	*((volatile unsigned short *)(LI_PCIB_VADDR+reg))=v;
}

static inline unsigned short li_pcib_read16(unsigned long reg)
{
	return *((volatile unsigned short *)(LI_PCIB_VADDR+reg));
}

#endif 

