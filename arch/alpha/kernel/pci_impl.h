
struct pci_dev;
struct pci_controller;
struct pci_iommu_arena;


#define EISA_DEFAULT_IO_BASE	0x9000	
#define DEFAULT_IO_BASE		0x8000	


#define XL_DEFAULT_MEM_BASE ((16+2)*1024*1024) 

#define APECS_AND_LCA_DEFAULT_MEM_BASE ((16+2)*1024*1024)

#define MCPCIA_DEFAULT_MEM_BASE ((32+2)*1024*1024)
#define T2_DEFAULT_MEM_BASE ((16+1)*1024*1024)

#define DEFAULT_MEM_BASE ((128+16)*1024*1024)

#define CIA_DEFAULT_MEM_BASE ((32+2)*1024*1024)

#define IRONGATE_DEFAULT_MEM_BASE ((256*8-16)*1024*1024)

#define DEFAULT_AGP_APER_SIZE	(64*1024*1024)




#define COMMON_TABLE_LOOKUP						\
({ long _ctl_ = -1; 							\
   if (slot >= min_idsel && slot <= max_idsel && pin < irqs_per_slot)	\
     _ctl_ = irq_tab[slot - min_idsel][pin];				\
   _ctl_; })



struct pci_iommu_arena
{
	spinlock_t lock;
	struct pci_controller *hose;
#define IOMMU_INVALID_PTE 0x2 
#define IOMMU_RESERVED_PTE 0xface
	unsigned long *ptes;
	dma_addr_t dma_base;
	unsigned int size;
	unsigned int next_entry;
	unsigned int align_entry;
};

#if defined(CONFIG_ALPHA_SRM) && \
    (defined(CONFIG_ALPHA_CIA) || defined(CONFIG_ALPHA_LCA))
# define NEED_SRM_SAVE_RESTORE
#else
# undef NEED_SRM_SAVE_RESTORE
#endif

#if defined(CONFIG_ALPHA_GENERIC) || defined(NEED_SRM_SAVE_RESTORE)
# define ALPHA_RESTORE_SRM_SETUP
#else
# undef ALPHA_RESTORE_SRM_SETUP
#endif

#ifdef ALPHA_RESTORE_SRM_SETUP
struct pdev_srm_saved_conf
{
	struct pdev_srm_saved_conf *next;
	struct pci_dev *dev;
};

extern void pci_restore_srm_config(void);
#else
#define pdev_save_srm_config(dev)	do {} while (0)
#define pci_restore_srm_config()	do {} while (0)
#endif

extern struct pci_controller *hose_head, **hose_tail;
extern struct pci_controller *pci_isa_hose;

extern unsigned long alpha_agpgart_size;

extern void common_init_pci(void);
#define common_swizzle pci_common_swizzle
extern struct pci_controller *alloc_pci_controller(void);
extern struct resource *alloc_resource(void);

extern struct pci_iommu_arena *iommu_arena_new_node(int,
						    struct pci_controller *,
					            dma_addr_t, unsigned long,
					            unsigned long);
extern struct pci_iommu_arena *iommu_arena_new(struct pci_controller *,
					       dma_addr_t, unsigned long,
					       unsigned long);
extern const char *const pci_io_names[];
extern const char *const pci_mem_names[];
extern const char pci_hae0_name[];

extern unsigned long size_for_memory(unsigned long max);

extern int iommu_reserve(struct pci_iommu_arena *, long, long);
extern int iommu_release(struct pci_iommu_arena *, long, long);
extern int iommu_bind(struct pci_iommu_arena *, long, long, struct page **);
extern int iommu_unbind(struct pci_iommu_arena *, long, long);


