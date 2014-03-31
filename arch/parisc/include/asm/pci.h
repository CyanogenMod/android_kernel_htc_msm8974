#ifndef __ASM_PARISC_PCI_H
#define __ASM_PARISC_PCI_H

#include <asm/scatterlist.h>



#define PCI_MAX_BUSSES	256


#define pci_post_reset_delay 50


struct pci_hba_data {
	void __iomem   *base_addr;	
	const struct parisc_device *dev; 
	struct pci_bus *hba_bus;	
	int		hba_num;	
	struct resource bus_num;	
	struct resource io_space;	
	struct resource lmmio_space;	
	struct resource elmmio_space;	
	struct resource gmmio_space;	

	#define DINO_MAX_LMMIO_RESOURCES	3

	unsigned long   lmmio_space_offset;  
	void *          iommu;          
	

	#define HBA_NAME_SIZE 16
	char io_name[HBA_NAME_SIZE];
	char lmmio_name[HBA_NAME_SIZE];
	char elmmio_name[HBA_NAME_SIZE];
	char gmmio_name[HBA_NAME_SIZE];
};

#define HBA_DATA(d)		((struct pci_hba_data *) (d))

#define HBA_PORT_SPACE_BITS	16

#define HBA_PORT_BASE(h)	((h) << HBA_PORT_SPACE_BITS)
#define HBA_PORT_SPACE_SIZE	(1UL << HBA_PORT_SPACE_BITS)

#define PCI_PORT_HBA(a)		((a) >> HBA_PORT_SPACE_BITS)
#define PCI_PORT_ADDR(a)	((a) & (HBA_PORT_SPACE_SIZE - 1))

#ifdef CONFIG_64BIT
#define PCI_F_EXTEND		0xffffffff00000000UL
#else	
#define PCI_F_EXTEND		0UL
#endif 

struct pci_bus;
struct pci_dev;

#ifdef CONFIG_PA20
#define PCI_DMA_BUS_IS_PHYS	0
#define parisc_has_iommu()	do { } while (0)
#else

#if defined(CONFIG_IOMMU_CCIO) || defined(CONFIG_IOMMU_SBA)
extern int parisc_bus_is_phys; 	
#define PCI_DMA_BUS_IS_PHYS	parisc_bus_is_phys
#define parisc_has_iommu()	do { parisc_bus_is_phys = 0; } while (0)
#else
#define PCI_DMA_BUS_IS_PHYS	1
#define parisc_has_iommu()	do { } while (0)
#endif

#endif	


struct pci_port_ops {
	  u8 (*inb)  (struct pci_hba_data *hba, u16 port);
	 u16 (*inw)  (struct pci_hba_data *hba, u16 port);
	 u32 (*inl)  (struct pci_hba_data *hba, u16 port);
	void (*outb) (struct pci_hba_data *hba, u16 port,  u8 data);
	void (*outw) (struct pci_hba_data *hba, u16 port, u16 data);
	void (*outl) (struct pci_hba_data *hba, u16 port, u32 data);
};


struct pci_bios_ops {
	void (*init)(void);
	void (*fixup_bus)(struct pci_bus *bus);
};

extern struct pci_port_ops *pci_port;
extern struct pci_bios_ops *pci_bios;

#ifdef CONFIG_PCI
extern void pcibios_register_hba(struct pci_hba_data *);
extern void pcibios_set_master(struct pci_dev *);
#else
static inline void pcibios_register_hba(struct pci_hba_data *x)
{
}
#endif

#define pcibios_assign_all_busses()     (1)

#define PCIBIOS_MIN_IO          0x10
#define PCIBIOS_MIN_MEM         0x1000 

#include <asm-generic/pci-dma-compat.h>

#ifdef CONFIG_PCI
static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	unsigned long cacheline_size;
	u8 byte;

	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &byte);
	if (byte == 0)
		cacheline_size = 1024;
	else
		cacheline_size = (int) byte * 4;

	*strat = PCI_DMA_BURST_MULTIPLE;
	*strategy_parameter = cacheline_size;
}
#endif

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	
}

static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return channel ? 15 : 14;
}

#endif 
