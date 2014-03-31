
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/dmar.h>
#include <asm/iommu.h>
#include <asm/machvec.h>
#include <linux/dma-mapping.h>


#ifdef CONFIG_INTEL_IOMMU

#include <linux/kernel.h>

#include <asm/page.h>

dma_addr_t bad_dma_address __read_mostly;
EXPORT_SYMBOL(bad_dma_address);

static int iommu_sac_force __read_mostly;

int no_iommu __read_mostly;
#ifdef CONFIG_IOMMU_DEBUG
int force_iommu __read_mostly = 1;
#else
int force_iommu __read_mostly;
#endif

int iommu_pass_through;
int iommu_group_mf;

struct device fallback_dev = {
	.init_name = "fallback device",
	.coherent_dma_mask = DMA_BIT_MASK(32),
	.dma_mask = &fallback_dev.coherent_dma_mask,
};

extern struct dma_map_ops intel_dma_ops;

static int __init pci_iommu_init(void)
{
	if (iommu_detected)
		intel_iommu_init();

	return 0;
}

fs_initcall(pci_iommu_init);

void pci_iommu_shutdown(void)
{
	return;
}

void __init
iommu_dma_init(void)
{
	return;
}

int iommu_dma_supported(struct device *dev, u64 mask)
{
	if (mask < DMA_BIT_MASK(24))
		return 0;

	if (iommu_sac_force && (mask >= DMA_BIT_MASK(40))) {
		dev_info(dev, "Force SAC with mask %llx\n", mask);
		return 0;
	}

	return 1;
}
EXPORT_SYMBOL(iommu_dma_supported);

void __init pci_iommu_alloc(void)
{
	dma_ops = &intel_dma_ops;

	dma_ops->sync_single_for_cpu = machvec_dma_sync_single;
	dma_ops->sync_sg_for_cpu = machvec_dma_sync_sg;
	dma_ops->sync_single_for_device = machvec_dma_sync_single;
	dma_ops->sync_sg_for_device = machvec_dma_sync_sg;
	dma_ops->dma_supported = iommu_dma_supported;

	detect_intel_iommu();

#ifdef CONFIG_SWIOTLB
	pci_swiotlb_init();
#endif
}

#endif
