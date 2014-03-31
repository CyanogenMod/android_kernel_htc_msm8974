#ifndef _ASM_PARISC_ROPES_H_
#define _ASM_PARISC_ROPES_H_

#include <asm/parisc-device.h>

#ifdef CONFIG_64BIT
#define ZX1_SUPPORT
#endif

#ifdef CONFIG_PROC_FS
#undef SBA_COLLECT_STATS
#endif

#define DELAYED_RESOURCE_CNT	16

#define MAX_IOC		2	
#define ROPES_PER_IOC	8	

struct ioc {
	void __iomem	*ioc_hpa;	
	char		*res_map;	
	u64		*pdir_base;	
	unsigned long	ibase;		
	unsigned long	imask;		
#ifdef ZX1_SUPPORT
	unsigned long	iovp_mask;	
#endif
	unsigned long	*res_hint;	
	spinlock_t	res_lock;
	unsigned int	res_bitshift;	
	unsigned int	res_size;	
#ifdef SBA_HINT_SUPPORT
	unsigned long	hint_mask_pdir; 
	unsigned int	hint_shift_pdir;
#endif
#if DELAYED_RESOURCE_CNT > 0
	int		saved_cnt;
	struct sba_dma_pair {
			dma_addr_t	iova;
			size_t		size;
        } saved[DELAYED_RESOURCE_CNT];
#endif

#ifdef SBA_COLLECT_STATS
#define SBA_SEARCH_SAMPLE	0x100
	unsigned long	avg_search[SBA_SEARCH_SAMPLE];
	unsigned long	avg_idx;	
	unsigned long	used_pages;
	unsigned long	msingle_calls;
	unsigned long	msingle_pages;
	unsigned long	msg_calls;
	unsigned long	msg_pages;
	unsigned long	usingle_calls;
	unsigned long	usingle_pages;
	unsigned long	usg_calls;
	unsigned long	usg_pages;
#endif
        
	unsigned int	pdir_size;	
};

struct sba_device {
	struct sba_device	*next;  
	struct parisc_device	*dev;   
	const char		*name;
	void __iomem		*sba_hpa; 
	spinlock_t		sba_lock;
	unsigned int		flags;  
	unsigned int		hw_rev;  

	struct resource		chip_resv; 
	struct resource		iommu_resv; 

	unsigned int		num_ioc;  
	struct ioc		ioc[MAX_IOC];
};

#define ASTRO_RUNWAY_PORT	0x582
#define IKE_MERCED_PORT		0x803
#define REO_MERCED_PORT		0x804
#define REOG_MERCED_PORT	0x805
#define PLUTO_MCKINLEY_PORT	0x880

static inline int IS_ASTRO(struct parisc_device *d) {
	return d->id.hversion == ASTRO_RUNWAY_PORT;
}

static inline int IS_IKE(struct parisc_device *d) {
	return d->id.hversion == IKE_MERCED_PORT;
}

static inline int IS_PLUTO(struct parisc_device *d) {
	return d->id.hversion == PLUTO_MCKINLEY_PORT;
}

#define PLUTO_IOVA_BASE	(1UL*1024*1024*1024)	
#define PLUTO_IOVA_SIZE	(1UL*1024*1024*1024)	
#define PLUTO_GART_SIZE	(PLUTO_IOVA_SIZE / 2)

#define SBA_PDIR_VALID_BIT	0x8000000000000000ULL

#define SBA_AGPGART_COOKIE	0x0000badbadc0ffeeULL

#define SBA_FUNC_ID	0x0000	
#define SBA_FCLASS	0x0008	

#define SBA_FUNC_SIZE 4096   

#define ASTRO_IOC_OFFSET	(32 * SBA_FUNC_SIZE)
#define PLUTO_IOC_OFFSET	(1 * SBA_FUNC_SIZE)
#define IKE_IOC_OFFSET(p)	((p+2) * SBA_FUNC_SIZE)

#define IOC_CTRL          0x8	
#define IOC_CTRL_TC       (1 << 0) 
#define IOC_CTRL_CE       (1 << 1) 
#define IOC_CTRL_DE       (1 << 2) 
#define IOC_CTRL_RM       (1 << 8) 
#define IOC_CTRL_NC       (1 << 9) 
#define IOC_CTRL_D4       (1 << 11) 
#define IOC_CTRL_DD       (1 << 13) 

#define LMMIO_DIRECT0_BASE  0x300
#define LMMIO_DIRECT0_MASK  0x308
#define LMMIO_DIRECT0_ROUTE 0x310

#define LMMIO_DIST_BASE  0x360
#define LMMIO_DIST_MASK  0x368
#define LMMIO_DIST_ROUTE 0x370

#define IOS_DIST_BASE	0x390
#define IOS_DIST_MASK	0x398
#define IOS_DIST_ROUTE	0x3A0

#define IOS_DIRECT_BASE	0x3C0
#define IOS_DIRECT_MASK	0x3C8
#define IOS_DIRECT_ROUTE 0x3D0

#define ROPE0_CTL	0x200  
#define ROPE1_CTL	0x208
#define ROPE2_CTL	0x210
#define ROPE3_CTL	0x218
#define ROPE4_CTL	0x220
#define ROPE5_CTL	0x228
#define ROPE6_CTL	0x230
#define ROPE7_CTL	0x238

#define IOC_ROPE0_CFG	0x500	
#define   IOC_ROPE_AO	  0x10	

#define HF_ENABLE	0x40

#define IOC_IBASE	0x300	
#define IOC_IMASK	0x308
#define IOC_PCOM	0x310
#define IOC_TCNFG	0x318
#define IOC_PDIR_BASE	0x320

#define IOVP_SIZE	PAGE_SIZE
#define IOVP_SHIFT	PAGE_SHIFT
#define IOVP_MASK	PAGE_MASK

#define SBA_PERF_CFG	0x708	
#define SBA_PERF_MASK1	0x718
#define SBA_PERF_MASK2	0x730

#define SBA_PERF_CNT1	0x200
#define SBA_PERF_CNT2	0x208
#define SBA_PERF_CNT3	0x210

struct lba_device {
	struct pci_hba_data	hba;

	spinlock_t		lba_lock;
	void			*iosapic_obj;

#ifdef CONFIG_64BIT
	void __iomem		*iop_base;	
#endif

	int			flags;		
	int			hw_rev;		
};

#define ELROY_HVERS		0x782
#define MERCURY_HVERS		0x783
#define QUICKSILVER_HVERS	0x784

static inline int IS_ELROY(struct parisc_device *d) {
	return (d->id.hversion == ELROY_HVERS);
}

static inline int IS_MERCURY(struct parisc_device *d) {
	return (d->id.hversion == MERCURY_HVERS);
}

static inline int IS_QUICKSILVER(struct parisc_device *d) {
	return (d->id.hversion == QUICKSILVER_HVERS);
}

static inline int agp_mode_mercury(void __iomem *hpa) {
	u64 bus_mode;

	bus_mode = readl(hpa + 0x0620);
	if (bus_mode & 1)
		return 1;

	return 0;
}

extern void *iosapic_register(unsigned long hpa);
extern int iosapic_fixup_irq(void *obj, struct pci_dev *pcidev);

#define LBA_FUNC_ID	0x0000	
#define LBA_FCLASS	0x0008	
#define LBA_CAPABLE	0x0030	

#define LBA_PCI_CFG_ADDR	0x0040	
#define LBA_PCI_CFG_DATA	0x0048	

#define LBA_PMC_MTLT	0x0050	
#define LBA_FW_SCRATCH	0x0058	
#define LBA_ERROR_ADDR	0x0070	

#define LBA_ARB_MASK	0x0080	
#define LBA_ARB_PRI	0x0088	
#define LBA_ARB_MODE	0x0090	
#define LBA_ARB_MTLT	0x0098	

#define LBA_MOD_ID	0x0100	

#define LBA_STAT_CTL	0x0108	
#define   LBA_BUS_RESET		0x01	
#define   CLEAR_ERRLOG		0x10	
#define   CLEAR_ERRLOG_ENABLE	0x20	
#define   HF_ENABLE	0x40	

#define LBA_LMMIO_BASE	0x0200	
#define LBA_LMMIO_MASK	0x0208

#define LBA_GMMIO_BASE	0x0210	
#define LBA_GMMIO_MASK	0x0218

#define LBA_WLMMIO_BASE	0x0220	
#define LBA_WLMMIO_MASK	0x0228

#define LBA_WGMMIO_BASE	0x0230	
#define LBA_WGMMIO_MASK	0x0238

#define LBA_IOS_BASE	0x0240	
#define LBA_IOS_MASK	0x0248

#define LBA_ELMMIO_BASE	0x0250	
#define LBA_ELMMIO_MASK	0x0258

#define LBA_EIOS_BASE	0x0260	
#define LBA_EIOS_MASK	0x0268

#define LBA_GLOBAL_MASK	0x0270	
#define LBA_DMA_CTL	0x0278	

#define LBA_IBASE	0x0300	
#define LBA_IMASK	0x0308

#define LBA_HINT_CFG	0x0310
#define LBA_HINT_BASE	0x0380	

#define LBA_BUS_MODE	0x0620

#define LBA_ERROR_CONFIG 0x0680
#define     LBA_SMART_MODE 0x20
#define LBA_ERROR_STATUS 0x0688
#define LBA_ROPE_CTL     0x06A0

#define LBA_IOSAPIC_BASE	0x800 

#endif 
