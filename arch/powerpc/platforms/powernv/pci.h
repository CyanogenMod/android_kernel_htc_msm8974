#ifndef __POWERNV_PCI_H
#define __POWERNV_PCI_H

struct pci_dn;

enum pnv_phb_type {
	PNV_PHB_P5IOC2,
	PNV_PHB_IODA1,
	PNV_PHB_IODA2,
};

enum pnv_phb_model {
	PNV_PHB_MODEL_UNKNOWN,
	PNV_PHB_MODEL_P5IOC2,
	PNV_PHB_MODEL_P7IOC,
};

#define PNV_PCI_DIAG_BUF_SIZE	4096

struct pnv_ioda_pe {
	struct pci_dev		*pdev;
	struct pci_bus		*pbus;

	unsigned int		rid;

	
	unsigned int		pe_number;

	unsigned int		dma_weight;

	struct pnv_ioda_pe	*bus_pe;

	
	int			tce32_seg;
	int			tce32_segcount;
	struct iommu_table	tce32_table;

	

	int			mve_number;

	
	struct list_head	link;
};

struct pnv_phb {
	struct pci_controller	*hose;
	enum pnv_phb_type	type;
	enum pnv_phb_model	model;
	u64			opal_id;
	void __iomem		*regs;
	spinlock_t		lock;

#ifdef CONFIG_PCI_MSI
	unsigned long		*msi_map;
	unsigned int		msi_base;
	unsigned int		msi_count;
	unsigned int		msi_next;
	unsigned int		msi32_support;
#endif
	int (*msi_setup)(struct pnv_phb *phb, struct pci_dev *dev,
			 unsigned int hwirq, unsigned int is_64,
			 struct msi_msg *msg);
	void (*dma_dev_setup)(struct pnv_phb *phb, struct pci_dev *pdev);
	void (*fixup_phb)(struct pci_controller *hose);
	u32 (*bdfn_to_pe)(struct pnv_phb *phb, struct pci_bus *bus, u32 devfn);

	union {
		struct {
			struct iommu_table iommu_table;
		} p5ioc2;

		struct {
			
			unsigned int		total_pe;
			unsigned int		m32_size;
			unsigned int		m32_segsize;
			unsigned int		m32_pci_base;
			unsigned int		io_size;
			unsigned int		io_segsize;
			unsigned int		io_pci_base;

			
			unsigned long		*pe_alloc;

			
			unsigned int		*m32_segmap;
			unsigned int		*io_segmap;
			struct pnv_ioda_pe	*pe_array;

			unsigned char		pe_rmap[0x10000];

			
			unsigned long		tce32_count;

			unsigned int		dma_weight;
			unsigned int		dma_pe_count;

			struct list_head	pe_list;
		} ioda;
	};

	
	union {
		unsigned char			blob[PNV_PCI_DIAG_BUF_SIZE];
		struct OpalIoP7IOCPhbErrorData	p7ioc;
	} diag;
};

extern struct pci_ops pnv_pci_ops;

extern void pnv_pci_setup_iommu_table(struct iommu_table *tbl,
				      void *tce_mem, u64 tce_size,
				      u64 dma_offset);
extern void pnv_pci_init_p5ioc2_hub(struct device_node *np);
extern void pnv_pci_init_ioda_hub(struct device_node *np);


#endif 
