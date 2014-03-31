
#ifndef _DRM_INTEL_GTT_H
#define	_DRM_INTEL_GTT_H

const struct intel_gtt {
	
	unsigned int stolen_size;
	
	unsigned int gtt_total_entries;
	unsigned int gtt_mappable_entries;
	
	unsigned int needs_dmar : 1;
	
	unsigned int do_idle_maps : 1;
	
	dma_addr_t scratch_page_dma;
	
	u32 __iomem *gtt;
} *intel_gtt_get(void);

void intel_gtt_chipset_flush(void);
void intel_gtt_unmap_memory(struct scatterlist *sg_list, int num_sg);
void intel_gtt_clear_range(unsigned int first_entry, unsigned int num_entries);
int intel_gtt_map_memory(struct page **pages, unsigned int num_entries,
			 struct scatterlist **sg_list, int *num_sg);
void intel_gtt_insert_sg_entries(struct scatterlist *sg_list,
				 unsigned int sg_len,
				 unsigned int pg_start,
				 unsigned int flags);
void intel_gtt_insert_pages(unsigned int first_entry, unsigned int num_entries,
			    struct page **pages, unsigned int flags);

#define AGP_DCACHE_MEMORY	1
#define AGP_PHYS_MEMORY		2

#define AGP_USER_CACHED_MEMORY_LLC_MLC (AGP_USER_TYPES + 2)
#define AGP_USER_UNCACHED_MEMORY (AGP_USER_TYPES + 4)

#define AGP_USER_CACHED_MEMORY_GFDT (1 << 3)

#ifdef CONFIG_INTEL_IOMMU
extern int intel_iommu_gfx_mapped;
#endif

#endif
