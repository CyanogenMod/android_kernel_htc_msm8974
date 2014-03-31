#ifndef __PCI_SH4_H
#define __PCI_SH4_H

#if defined(CONFIG_CPU_SUBTYPE_SH7780) || \
    defined(CONFIG_CPU_SUBTYPE_SH7785) || \
    defined(CONFIG_CPU_SUBTYPE_SH7763)
#include "pci-sh7780.h"
#else
#include "pci-sh7751.h"
#endif

#include <asm/io.h>

#define PCI_PROBE_BIOS		1
#define PCI_PROBE_CONF1		2
#define PCI_PROBE_CONF2		4
#define PCI_NO_CHECKS		0x400
#define PCI_ASSIGN_ROMS		0x1000
#define PCI_BIOS_IRQ_SCAN	0x2000

#define SH4_PCICR		0x100		
  #define SH4_PCICR_PREFIX	  0xA5000000	
  #define SH4_PCICR_FTO		  0x00000400	
  #define SH4_PCICR_TRSB	  0x00000200	
  #define SH4_PCICR_BSWP	  0x00000100	
  #define SH4_PCICR_PLUP	  0x00000080	
  #define SH4_PCICR_ARBM	  0x00000040	
  #define SH4_PCICR_MD		  0x00000030	
  #define SH4_PCICR_SERR	  0x00000008	
  #define SH4_PCICR_INTA	  0x00000004	
  #define SH4_PCICR_PRST	  0x00000002	
  #define SH4_PCICR_CFIN	  0x00000001	
#define SH4_PCILSR0		0x104		
#define SH4_PCILSR1		0x108		
#define SH4_PCILAR0		0x10C		
#define SH4_PCILAR1		0x110		
#define SH4_PCIINT		0x114		
  #define SH4_PCIINT_MLCK	  0x00008000	
  #define SH4_PCIINT_TABT	  0x00004000	
  #define SH4_PCIINT_TRET	  0x00000200	
  #define SH4_PCIINT_MFDE	  0x00000100	
  #define SH4_PCIINT_PRTY	  0x00000080	
  #define SH4_PCIINT_SERR	  0x00000040	
  #define SH4_PCIINT_TWDP	  0x00000020	
  #define SH4_PCIINT_TRDP	  0x00000010	
  #define SH4_PCIINT_MTABT	  0x00000008	
  #define SH4_PCIINT_MMABT	  0x00000004	
  #define SH4_PCIINT_MWPD	  0x00000002	
  #define SH4_PCIINT_MRPD	  0x00000001	
#define SH4_PCIINTM		0x118		
  #define SH4_PCIINTM_TTADIM	  BIT(14)	
  #define SH4_PCIINTM_TMTOIM	  BIT(9)	
  #define SH4_PCIINTM_MDEIM	  BIT(8)	
  #define SH4_PCIINTM_APEDIM	  BIT(7)	
  #define SH4_PCIINTM_SDIM	  BIT(6)	
  #define SH4_PCIINTM_DPEITWM	  BIT(5)	
  #define SH4_PCIINTM_PEDITRM	  BIT(4)	
  #define SH4_PCIINTM_TADIMM	  BIT(3)	
  #define SH4_PCIINTM_MADIMM	  BIT(2)	
  #define SH4_PCIINTM_MWPDIM	  BIT(1)	
  #define SH4_PCIINTM_MRDPEIM	  BIT(0)	
#define SH4_PCIALR		0x11C		
#define SH4_PCICLR		0x120		
  #define SH4_PCICLR_MPIO	  0x80000000
  #define SH4_PCICLR_MDMA0	  0x40000000	
  #define SH4_PCICLR_MDMA1	  0x20000000	
  #define SH4_PCICLR_MDMA2	  0x10000000	
  #define SH4_PCICLR_MDMA3	  0x08000000	
  #define SH4_PCICLR_TGT	  0x04000000	
  #define SH4_PCICLR_CMDL	  0x0000000F	
#define SH4_PCIAINT		0x130		
  #define SH4_PCIAINT_MBKN	  0x00002000	
  #define SH4_PCIAINT_TBTO	  0x00001000	
  #define SH4_PCIAINT_MBTO	  0x00000800	
  #define SH4_PCIAINT_TABT	  0x00000008	
  #define SH4_PCIAINT_MABT	  0x00000004	
  #define SH4_PCIAINT_RDPE	  0x00000002	
  #define SH4_PCIAINT_WDPE	  0x00000001	
#define SH4_PCIAINTM            0x134		
#define SH4_PCIBMLR		0x138		
  #define SH4_PCIBMLR_REQ4	  0x00000010	
  #define SH4_PCIBMLR_REQ3	  0x00000008	
  #define SH4_PCIBMLR_REQ2	  0x00000004	
  #define SH4_PCIBMLR_REQ1	  0x00000002	
  #define SH4_PCIBMLR_REQ0	  0x00000001	
#define SH4_PCIDMABT		0x140		
  #define SH4_PCIDMABT_RRBN	  0x00000001	
#define SH4_PCIDPA0		0x180		
#define SH4_PCIDLA0		0x184		
#define SH4_PCIDTC0		0x188		
#define SH4_PCIDCR0		0x18C		
  #define SH4_PCIDCR_ALGN	  0x00000600	
  #define SH4_PCIDCR_MAST	  0x00000100	
  #define SH4_PCIDCR_INTM	  0x00000080	
  #define SH4_PCIDCR_INTS	  0x00000040	
  #define SH4_PCIDCR_LHLD	  0x00000020	
  #define SH4_PCIDCR_PHLD	  0x00000010	
  #define SH4_PCIDCR_IOSEL	  0x00000008	
  #define SH4_PCIDCR_DIR	  0x00000004	
  #define SH4_PCIDCR_STOP	  0x00000002	
  #define SH4_PCIDCR_STRT	  0x00000001	
#define SH4_PCIDPA1		0x190		
#define SH4_PCIDLA1		0x194		
#define SH4_PCIDTC1		0x198		
#define SH4_PCIDCR1		0x19C		
#define SH4_PCIDPA2		0x1A0		
#define SH4_PCIDLA2		0x1A4		
#define SH4_PCIDTC2		0x1A8		
#define SH4_PCIDCR2		0x1AC		
#define SH4_PCIDPA3		0x1B0		
#define SH4_PCIDLA3		0x1B4		
#define SH4_PCIDTC3		0x1B8		
#define SH4_PCIDCR3		0x1BC		
#define SH4_PCIPAR		0x1C0		
  #define SH4_PCIPAR_CFGEN	  0x80000000	
  #define SH4_PCIPAR_BUSNO	  0x00FF0000	
  #define SH4_PCIPAR_DEVNO	  0x0000FF00	
  #define SH4_PCIPAR_REGAD	  0x000000FC	
#define SH4_PCIMBR		0x1C4		
  #define SH4_PCIMBR_MASK	  0xFF000000	
  #define SH4_PCIMBR_LOCK	  0x00000001	
#define SH4_PCIIOBR		0x1C8		
  #define SH4_PCIIOBR_MASK	  0xFFFC0000	
  #define SH4_PCIIOBR_LOCK	  0x00000001	
#define SH4_PCIPINT		0x1CC		
  #define SH4_PCIPINT_D3	  0x00000002	
  #define SH4_PCIPINT_D0	  0x00000001	
#define SH4_PCIPINTM		0x1D0		
#define SH4_PCICLKR		0x1D4		
  #define SH4_PCICLKR_PCSTP	  0x00000002	
  #define SH4_PCICLKR_BCSTP	  0x00000001	
#define SH4_PCIBCR1		0x1E0		
  #define SH4_PCIMBR0		SH4_PCIBCR1
#define SH4_PCIBCR2		0x1E4		
  #define SH4_PCIMBMR0		SH4_PCIBCR2
#define SH4_PCIWCR1		0x1E8		
#define SH4_PCIWCR2		0x1EC		
#define SH4_PCIWCR3		0x1F0		
  #define SH4_PCIMBR2		SH4_PCIWCR3
#define SH4_PCIMCR		0x1F4		
#define SH4_PCIBCR3		0x1f8		
#define SH4_PCIPCTR             0x200		
  #define SH4_PCIPCTR_P2EN	  0x000400000	
  #define SH4_PCIPCTR_P1EN	  0x000200000	
  #define SH4_PCIPCTR_P0EN	  0x000100000	
  #define SH4_PCIPCTR_P2UP	  0x000000020	
  #define SH4_PCIPCTR_P2IO	  0x000000010	
  #define SH4_PCIPCTR_P1UP	  0x000000008	
  #define SH4_PCIPCTR_P1IO	  0x000000004	
  #define SH4_PCIPCTR_P0UP	  0x000000002	
  #define SH4_PCIPCTR_P0IO	  0x000000001	
#define SH4_PCIPDTR		0x204		
  #define SH4_PCIPDTR_PB5	  0x000000020	
  #define SH4_PCIPDTR_PB4	  0x000000010	
  #define SH4_PCIPDTR_PB3	  0x000000008	
  #define SH4_PCIPDTR_PB2	  0x000000004	
  #define SH4_PCIPDTR_PB1	  0x000000002	
  #define SH4_PCIPDTR_PB0	  0x000000001	
#define SH4_PCIPDR		0x220		

extern struct pci_ops sh4_pci_ops;
int pci_fixup_pcic(struct pci_channel *chan);

struct sh4_pci_address_space {
	unsigned long base;
	unsigned long size;
};

struct sh4_pci_address_map {
	struct sh4_pci_address_space window0;
	struct sh4_pci_address_space window1;
};

static inline void pci_write_reg(struct pci_channel *chan,
				 unsigned long val, unsigned long reg)
{
	__raw_writel(val, chan->reg_base + reg);
}

static inline unsigned long pci_read_reg(struct pci_channel *chan,
					 unsigned long reg)
{
	return __raw_readl(chan->reg_base + reg);
}

#endif 
