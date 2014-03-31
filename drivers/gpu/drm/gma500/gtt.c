/*
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics.com>
 *	    Alan Cox <alan@linux.intel.com>
 */

#include <drm/drmP.h>
#include <linux/shmem_fs.h>
#include "psb_drv.h"



static inline uint32_t psb_gtt_mask_pte(uint32_t pfn, int type)
{
	uint32_t mask = PSB_PTE_VALID;

	if (type & PSB_MMU_CACHED_MEMORY)
		mask |= PSB_PTE_CACHED;
	if (type & PSB_MMU_RO_MEMORY)
		mask |= PSB_PTE_RO;
	if (type & PSB_MMU_WO_MEMORY)
		mask |= PSB_PTE_WO;

	return (pfn << PAGE_SHIFT) | mask;
}

static u32 *psb_gtt_entry(struct drm_device *dev, struct gtt_range *r)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned long offset;

	offset = r->resource.start - dev_priv->gtt_mem->start;

	return dev_priv->gtt_map + (offset >> PAGE_SHIFT);
}

static int psb_gtt_insert(struct drm_device *dev, struct gtt_range *r)
{
	u32 *gtt_slot, pte;
	struct page **pages;
	int i;

	if (r->pages == NULL) {
		WARN_ON(1);
		return -EINVAL;
	}

	WARN_ON(r->stolen);	

	gtt_slot = psb_gtt_entry(dev, r);
	pages = r->pages;

	
	set_pages_array_uc(pages, r->npage);

	
	for (i = r->roll; i < r->npage; i++) {
		pte = psb_gtt_mask_pte(page_to_pfn(r->pages[i]), 0);
		iowrite32(pte, gtt_slot++);
	}
	for (i = 0; i < r->roll; i++) {
		pte = psb_gtt_mask_pte(page_to_pfn(r->pages[i]), 0);
		iowrite32(pte, gtt_slot++);
	}
	
	ioread32(gtt_slot - 1);

	return 0;
}

static void psb_gtt_remove(struct drm_device *dev, struct gtt_range *r)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 *gtt_slot, pte;
	int i;

	WARN_ON(r->stolen);

	gtt_slot = psb_gtt_entry(dev, r);
	pte = psb_gtt_mask_pte(page_to_pfn(dev_priv->scratch_page), 0);

	for (i = 0; i < r->npage; i++)
		iowrite32(pte, gtt_slot++);
	ioread32(gtt_slot - 1);
	set_pages_array_wb(r->pages, r->npage);
}

void psb_gtt_roll(struct drm_device *dev, struct gtt_range *r, int roll)
{
	u32 *gtt_slot, pte;
	int i;

	if (roll >= r->npage) {
		WARN_ON(1);
		return;
	}

	r->roll = roll;

	if (!r->stolen && !r->in_gart)
		return;

	gtt_slot = psb_gtt_entry(dev, r);

	for (i = r->roll; i < r->npage; i++) {
		pte = psb_gtt_mask_pte(page_to_pfn(r->pages[i]), 0);
		iowrite32(pte, gtt_slot++);
	}
	for (i = 0; i < r->roll; i++) {
		pte = psb_gtt_mask_pte(page_to_pfn(r->pages[i]), 0);
		iowrite32(pte, gtt_slot++);
	}
	ioread32(gtt_slot - 1);
}

static int psb_gtt_attach_pages(struct gtt_range *gt)
{
	struct inode *inode;
	struct address_space *mapping;
	int i;
	struct page *p;
	int pages = gt->gem.size / PAGE_SIZE;

	WARN_ON(gt->pages);

	
	inode = gt->gem.filp->f_path.dentry->d_inode;
	mapping = inode->i_mapping;

	gt->pages = kmalloc(pages * sizeof(struct page *), GFP_KERNEL);
	if (gt->pages == NULL)
		return -ENOMEM;
	gt->npage = pages;

	for (i = 0; i < pages; i++) {
		p = shmem_read_mapping_page(mapping, i);
		if (IS_ERR(p))
			goto err;
		gt->pages[i] = p;
	}
	return 0;

err:
	while (i--)
		page_cache_release(gt->pages[i]);
	kfree(gt->pages);
	gt->pages = NULL;
	return PTR_ERR(p);
}

static void psb_gtt_detach_pages(struct gtt_range *gt)
{
	int i;
	for (i = 0; i < gt->npage; i++) {
		
		set_page_dirty(gt->pages[i]);
		page_cache_release(gt->pages[i]);
	}
	kfree(gt->pages);
	gt->pages = NULL;
}

int psb_gtt_pin(struct gtt_range *gt)
{
	int ret = 0;
	struct drm_device *dev = gt->gem.dev;
	struct drm_psb_private *dev_priv = dev->dev_private;

	mutex_lock(&dev_priv->gtt_mutex);

	if (gt->in_gart == 0 && gt->stolen == 0) {
		ret = psb_gtt_attach_pages(gt);
		if (ret < 0)
			goto out;
		ret = psb_gtt_insert(dev, gt);
		if (ret < 0) {
			psb_gtt_detach_pages(gt);
			goto out;
		}
	}
	gt->in_gart++;
out:
	mutex_unlock(&dev_priv->gtt_mutex);
	return ret;
}

void psb_gtt_unpin(struct gtt_range *gt)
{
	struct drm_device *dev = gt->gem.dev;
	struct drm_psb_private *dev_priv = dev->dev_private;

	mutex_lock(&dev_priv->gtt_mutex);

	WARN_ON(!gt->in_gart);

	gt->in_gart--;
	if (gt->in_gart == 0 && gt->stolen == 0) {
		psb_gtt_remove(dev, gt);
		psb_gtt_detach_pages(gt);
	}
	mutex_unlock(&dev_priv->gtt_mutex);
}


struct gtt_range *psb_gtt_alloc_range(struct drm_device *dev, int len,
						const char *name, int backed)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct gtt_range *gt;
	struct resource *r = dev_priv->gtt_mem;
	int ret;
	unsigned long start, end;

	if (backed) {
		
		start = r->start;
		end = r->start + dev_priv->gtt.stolen_size - 1;
	} else {
		
		start = r->start + dev_priv->gtt.stolen_size;
		end = r->end;
	}

	gt = kzalloc(sizeof(struct gtt_range), GFP_KERNEL);
	if (gt == NULL)
		return NULL;
	gt->resource.name = name;
	gt->stolen = backed;
	gt->in_gart = backed;
	gt->roll = 0;
	
	gt->gem.dev = dev;
	ret = allocate_resource(dev_priv->gtt_mem, &gt->resource,
				len, start, end, PAGE_SIZE, NULL, NULL);
	if (ret == 0) {
		gt->offset = gt->resource.start - r->start;
		return gt;
	}
	kfree(gt);
	return NULL;
}

void psb_gtt_free_range(struct drm_device *dev, struct gtt_range *gt)
{
	
	if (gt->mmapping) {
		psb_gtt_unpin(gt);
		gt->mmapping = 0;
	}
	WARN_ON(gt->in_gart && !gt->stolen);
	release_resource(&gt->resource);
	kfree(gt);
}

static void psb_gtt_alloc(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	init_rwsem(&dev_priv->gtt.sem);
}

void psb_gtt_takedown(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	if (dev_priv->gtt_map) {
		iounmap(dev_priv->gtt_map);
		dev_priv->gtt_map = NULL;
	}
	if (dev_priv->gtt_initialized) {
		pci_write_config_word(dev->pdev, PSB_GMCH_CTRL,
				      dev_priv->gmch_ctrl);
		PSB_WVDC32(dev_priv->pge_ctl, PSB_PGETBL_CTL);
		(void) PSB_RVDC32(PSB_PGETBL_CTL);
	}
	if (dev_priv->vram_addr)
		iounmap(dev_priv->gtt_map);
}

int psb_gtt_init(struct drm_device *dev, int resume)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned gtt_pages;
	unsigned long stolen_size, vram_stolen_size;
	unsigned i, num_pages;
	unsigned pfn_base;
	uint32_t vram_pages;
	uint32_t dvmt_mode = 0;
	struct psb_gtt *pg;

	int ret = 0;
	uint32_t pte;

	mutex_init(&dev_priv->gtt_mutex);

	psb_gtt_alloc(dev);
	pg = &dev_priv->gtt;

	
	pci_read_config_word(dev->pdev, PSB_GMCH_CTRL, &dev_priv->gmch_ctrl);
	pci_write_config_word(dev->pdev, PSB_GMCH_CTRL,
			      dev_priv->gmch_ctrl | _PSB_GMCH_ENABLED);

	dev_priv->pge_ctl = PSB_RVDC32(PSB_PGETBL_CTL);
	PSB_WVDC32(dev_priv->pge_ctl | _PSB_PGETBL_ENABLED, PSB_PGETBL_CTL);
	(void) PSB_RVDC32(PSB_PGETBL_CTL);

	
	dev_priv->gtt_initialized = 1;

	pg->gtt_phys_start = dev_priv->pge_ctl & PAGE_MASK;

	pg->mmu_gatt_start = 0xE0000000;

	pg->gtt_start = pci_resource_start(dev->pdev, PSB_GTT_RESOURCE);
	gtt_pages = pci_resource_len(dev->pdev, PSB_GTT_RESOURCE)
								>> PAGE_SHIFT;
	
	if (pg->gtt_start == 0 || gtt_pages == 0) {
		dev_dbg(dev->dev, "GTT PCI BAR not initialized.\n");
		gtt_pages = 64;
		pg->gtt_start = dev_priv->pge_ctl;
	}

	pg->gatt_start = pci_resource_start(dev->pdev, PSB_GATT_RESOURCE);
	pg->gatt_pages = pci_resource_len(dev->pdev, PSB_GATT_RESOURCE)
								>> PAGE_SHIFT;
	dev_priv->gtt_mem = &dev->pdev->resource[PSB_GATT_RESOURCE];

	if (pg->gatt_pages == 0 || pg->gatt_start == 0) {
		static struct resource fudge;	
		dev_dbg(dev->dev, "GATT PCI BAR not initialized.\n");
		pg->gatt_start = 0x40000000;
		pg->gatt_pages = (128 * 1024 * 1024) >> PAGE_SHIFT;
		fudge.start = 0x40000000;
		fudge.end = 0x40000000 + 128 * 1024 * 1024 - 1;
		fudge.name = "fudge";
		fudge.flags = IORESOURCE_MEM;
		dev_priv->gtt_mem = &fudge;
	}

	pci_read_config_dword(dev->pdev, PSB_BSM, &dev_priv->stolen_base);
	vram_stolen_size = pg->gtt_phys_start - dev_priv->stolen_base
								- PAGE_SIZE;

	stolen_size = vram_stolen_size;

	printk(KERN_INFO "Stolen memory information\n");
	printk(KERN_INFO "       base in RAM: 0x%x\n", dev_priv->stolen_base);
	printk(KERN_INFO "       size: %luK, calculated by (GTT RAM base) - (Stolen base), seems wrong\n",
		vram_stolen_size/1024);
	dvmt_mode = (dev_priv->gmch_ctrl >> 4) & 0x7;
	printk(KERN_INFO "      the correct size should be: %dM(dvmt mode=%d)\n",
		(dvmt_mode == 1) ? 1 : (2 << (dvmt_mode - 1)), dvmt_mode);

	if (resume && (gtt_pages != pg->gtt_pages) &&
	    (stolen_size != pg->stolen_size)) {
		dev_err(dev->dev, "GTT resume error.\n");
		ret = -EINVAL;
		goto out_err;
	}

	pg->gtt_pages = gtt_pages;
	pg->stolen_size = stolen_size;
	dev_priv->vram_stolen_size = vram_stolen_size;

	dev_priv->gtt_map = ioremap_nocache(pg->gtt_phys_start,
						gtt_pages << PAGE_SHIFT);
	if (!dev_priv->gtt_map) {
		dev_err(dev->dev, "Failure to map gtt.\n");
		ret = -ENOMEM;
		goto out_err;
	}

	dev_priv->vram_addr = ioremap_wc(dev_priv->stolen_base, stolen_size);
	if (!dev_priv->vram_addr) {
		dev_err(dev->dev, "Failure to map stolen base.\n");
		ret = -ENOMEM;
		goto out_err;
	}


	pfn_base = dev_priv->stolen_base >> PAGE_SHIFT;
	vram_pages = num_pages = vram_stolen_size >> PAGE_SHIFT;
	printk(KERN_INFO"Set up %d stolen pages starting at 0x%08x, GTT offset %dK\n",
		num_pages, pfn_base << PAGE_SHIFT, 0);
	for (i = 0; i < num_pages; ++i) {
		pte = psb_gtt_mask_pte(pfn_base + i, 0);
		iowrite32(pte, dev_priv->gtt_map + i);
	}


	pfn_base = page_to_pfn(dev_priv->scratch_page);
	pte = psb_gtt_mask_pte(pfn_base, 0);
	for (; i < gtt_pages; ++i)
		iowrite32(pte, dev_priv->gtt_map + i);

	(void) ioread32(dev_priv->gtt_map + i - 1);
	return 0;

out_err:
	psb_gtt_takedown(dev);
	return ret;
}
