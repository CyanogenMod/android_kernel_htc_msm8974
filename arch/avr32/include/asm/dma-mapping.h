#ifndef __ASM_AVR32_DMA_MAPPING_H
#define __ASM_AVR32_DMA_MAPPING_H

#include <linux/mm.h>
#include <linux/device.h>
#include <linux/scatterlist.h>
#include <asm/processor.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

extern void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
	int direction);

static inline int dma_supported(struct device *dev, u64 mask)
{
	
	return 1;
}

static inline int dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;
	return 0;
}

static inline int dma_mapping_error(struct device *dev, dma_addr_t addr)
{
	return 0;
}

extern void *dma_alloc_coherent(struct device *dev, size_t size,
				dma_addr_t *handle, gfp_t gfp);

extern void dma_free_coherent(struct device *dev, size_t size,
			      void *cpu_addr, dma_addr_t handle);

extern void *dma_alloc_writecombine(struct device *dev, size_t size,
				    dma_addr_t *handle, gfp_t gfp);

extern void dma_free_writecombine(struct device *dev, size_t size,
				  void *cpu_addr, dma_addr_t handle);

/**
 * dma_map_single - map a single buffer for streaming DMA
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @cpu_addr: CPU direct mapped address of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * Ensure that any data held in the cache is appropriately discarded
 * or written back.
 *
 * The device owns this memory once this call has completed.  The CPU
 * can regain ownership by calling dma_unmap_single() or dma_sync_single().
 */
static inline dma_addr_t
dma_map_single(struct device *dev, void *cpu_addr, size_t size,
	       enum dma_data_direction direction)
{
	dma_cache_sync(dev, cpu_addr, size, direction);
	return virt_to_bus(cpu_addr);
}

static inline void
dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size,
		 enum dma_data_direction direction)
{

}

/**
 * dma_map_page - map a portion of a page for streaming DMA
 * @dev: valid struct device pointer, or NULL for ISA and EISA-like devices
 * @page: page that buffer resides in
 * @offset: offset into page for start of buffer
 * @size: size of buffer to map
 * @dir: DMA transfer direction
 *
 * Ensure that any data held in the cache is appropriately discarded
 * or written back.
 *
 * The device owns this memory once this call has completed.  The CPU
 * can regain ownership by calling dma_unmap_page() or dma_sync_single().
 */
static inline dma_addr_t
dma_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size,
	     enum dma_data_direction direction)
{
	return dma_map_single(dev, page_address(page) + offset,
			      size, direction);
}

static inline void
dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
	       enum dma_data_direction direction)
{
	dma_unmap_single(dev, dma_address, size, direction);
}

static inline int
dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
	   enum dma_data_direction direction)
{
	int i;

	for (i = 0; i < nents; i++) {
		char *virt;

		sg[i].dma_address = page_to_bus(sg_page(&sg[i])) + sg[i].offset;
		virt = sg_virt(&sg[i]);
		dma_cache_sync(dev, virt, sg[i].length, direction);
	}

	return nents;
}

static inline void
dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nhwentries,
	     enum dma_data_direction direction)
{

}

static inline void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
			size_t size, enum dma_data_direction direction)
{
}

static inline void
dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle,
			   size_t size, enum dma_data_direction direction)
{
	dma_cache_sync(dev, bus_to_virt(dma_handle), size, direction);
}

static inline void
dma_sync_single_range_for_cpu(struct device *dev, dma_addr_t dma_handle,
			      unsigned long offset, size_t size,
			      enum dma_data_direction direction)
{
	
	dma_sync_single_for_cpu(dev, dma_handle, offset+size, direction);
}

static inline void
dma_sync_single_range_for_device(struct device *dev, dma_addr_t dma_handle,
				 unsigned long offset, size_t size,
				 enum dma_data_direction direction)
{
	
	dma_sync_single_for_device(dev, dma_handle, offset+size, direction);
}

static inline void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
		    int nents, enum dma_data_direction direction)
{
}

static inline void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
		       int nents, enum dma_data_direction direction)
{
	int i;

	for (i = 0; i < nents; i++) {
		dma_cache_sync(dev, sg_virt(&sg[i]), sg[i].length, direction);
	}
}


#define dma_alloc_noncoherent(d, s, h, f) dma_alloc_coherent(d, s, h, f)
#define dma_free_noncoherent(d, s, v, h) dma_free_coherent(d, s, v, h)

#endif 
