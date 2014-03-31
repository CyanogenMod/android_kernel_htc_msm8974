#include <linux/prefetch.h>

 
static inline unsigned int
iommu_fill_pdir(struct ioc *ioc, struct scatterlist *startsg, int nents, 
		unsigned long hint,
		void (*iommu_io_pdir_entry)(u64 *, space_t, unsigned long,
					    unsigned long))
{
	struct scatterlist *dma_sg = startsg;	
	unsigned int n_mappings = 0;
	unsigned long dma_offset = 0, dma_len = 0;
	u64 *pdirp = NULL;

	 dma_sg--;

	while (nents-- > 0) {
		unsigned long vaddr;
		long size;

		DBG_RUN_SG(" %d : %08lx/%05x %08lx/%05x\n", nents,
			   (unsigned long)sg_dma_address(startsg), cnt,
			   sg_virt_addr(startsg), startsg->length
		);


		
		if (sg_dma_address(startsg) & PIDE_FLAG) {
			u32 pide = sg_dma_address(startsg) & ~PIDE_FLAG;

			BUG_ON(pdirp && (dma_len != sg_dma_len(dma_sg)));

			dma_sg++;

			dma_len = sg_dma_len(startsg);
			sg_dma_len(startsg) = 0;
			dma_offset = (unsigned long) pide & ~IOVP_MASK;
			n_mappings++;
#if defined(ZX1_SUPPORT)
			
			sg_dma_address(dma_sg) = pide | ioc->ibase;
#else
			sg_dma_address(dma_sg) = pide;
#endif
			pdirp = &(ioc->pdir_base[pide >> IOVP_SHIFT]);
			prefetchw(pdirp);
		}
		
		BUG_ON(pdirp == NULL);
		
		vaddr = sg_virt_addr(startsg);
		sg_dma_len(dma_sg) += startsg->length;
		size = startsg->length + dma_offset;
		dma_offset = 0;
#ifdef IOMMU_MAP_STATS
		ioc->msg_pages += startsg->length >> IOVP_SHIFT;
#endif
		do {
			iommu_io_pdir_entry(pdirp, KERNEL_SPACE, 
					    vaddr, hint);
			vaddr += IOVP_SIZE;
			size -= IOVP_SIZE;
			pdirp++;
		} while(unlikely(size > 0));
		startsg++;
	}
	return(n_mappings);
}



static inline unsigned int
iommu_coalesce_chunks(struct ioc *ioc, struct device *dev,
		struct scatterlist *startsg, int nents,
		int (*iommu_alloc_range)(struct ioc *, struct device *, size_t))
{
	struct scatterlist *contig_sg;	   
	unsigned long dma_offset, dma_len; 
	unsigned int n_mappings = 0;
	unsigned int max_seg_size = dma_get_max_seg_size(dev);

	while (nents > 0) {

		contig_sg = startsg;
		dma_len = startsg->length;
		dma_offset = sg_virt_addr(startsg) & ~IOVP_MASK;

		
		sg_dma_address(startsg) = 0;
		sg_dma_len(startsg) = 0;

		while(--nents > 0) {
			unsigned long prevstartsg_end, startsg_end;

			prevstartsg_end = sg_virt_addr(startsg) +
				startsg->length;

			startsg++;
			startsg_end = sg_virt_addr(startsg) + 
				startsg->length;

			
			sg_dma_address(startsg) = 0;
			sg_dma_len(startsg) = 0;

   
			if(unlikely(ALIGN(dma_len + dma_offset + startsg->length,
					    IOVP_SIZE) > DMA_CHUNK_SIZE))
				break;

			if (startsg->length + dma_len > max_seg_size)
				break;

			if (unlikely(((prevstartsg_end | sg_virt_addr(startsg)) & ~PAGE_MASK) != 0))
				break;
			
			dma_len += startsg->length;
		}

		sg_dma_len(contig_sg) = dma_len;
		dma_len = ALIGN(dma_len + dma_offset, IOVP_SIZE);
		sg_dma_address(contig_sg) =
			PIDE_FLAG 
			| (iommu_alloc_range(ioc, dev, dma_len) << IOVP_SHIFT)
			| dma_offset;
		n_mappings++;
	}

	return n_mappings;
}

