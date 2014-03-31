/*
 * Remote Processor Framework
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Ohad Ben-Cohen <ohad@wizery.com>
 * Brian Swetland <swetland@google.com>
 * Mark Grosen <mgrosen@ti.com>
 * Fernando Guzman Lugo <fernando.lugo@ti.com>
 * Suman Anna <s-anna@ti.com>
 * Robert Tivy <rtivy@ti.com>
 * Armando Uribe De Leon <x0095078@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/remoteproc.h>
#include <linux/iommu.h>
#include <linux/klist.h>
#include <linux/elf.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <asm/byteorder.h>

#include "remoteproc_internal.h"

static void klist_rproc_get(struct klist_node *n);
static void klist_rproc_put(struct klist_node *n);

static DEFINE_KLIST(rprocs, klist_rproc_get, klist_rproc_put);

typedef int (*rproc_handle_resources_t)(struct rproc *rproc,
				struct resource_table *table, int len);
typedef int (*rproc_handle_resource_t)(struct rproc *rproc, void *, int avail);

static int rproc_iommu_fault(struct iommu_domain *domain, struct device *dev,
		unsigned long iova, int flags, void *token)
{
	dev_err(dev, "iommu fault: da 0x%lx flags 0x%x\n", iova, flags);

	return -ENOSYS;
}

static int rproc_enable_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain;
	struct device *dev = rproc->dev;
	int ret;

	if (!iommu_present(dev->bus)) {
		dev_dbg(dev, "iommu not found\n");
		return 0;
	}

	domain = iommu_domain_alloc(dev->bus);
	if (!domain) {
		dev_err(dev, "can't alloc iommu domain\n");
		return -ENOMEM;
	}

	iommu_set_fault_handler(domain, rproc_iommu_fault, rproc);

	ret = iommu_attach_device(domain, dev);
	if (ret) {
		dev_err(dev, "can't attach iommu device: %d\n", ret);
		goto free_domain;
	}

	rproc->domain = domain;

	return 0;

free_domain:
	iommu_domain_free(domain);
	return ret;
}

static void rproc_disable_iommu(struct rproc *rproc)
{
	struct iommu_domain *domain = rproc->domain;
	struct device *dev = rproc->dev;

	if (!domain)
		return;

	iommu_detach_device(domain, dev);
	iommu_domain_free(domain);

	return;
}

static void *rproc_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct rproc_mem_entry *carveout;
	void *ptr = NULL;

	list_for_each_entry(carveout, &rproc->carveouts, node) {
		int offset = da - carveout->da;

		
		if (offset < 0)
			continue;

		
		if (offset + len > carveout->len)
			continue;

		ptr = carveout->va + offset;

		break;
	}

	return ptr;
}

static int
rproc_load_segments(struct rproc *rproc, const u8 *elf_data, size_t len)
{
	struct device *dev = rproc->dev;
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		dev_dbg(dev, "phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
					phdr->p_type, da, memsz, filesz);

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%x memsz 0x%x\n",
							filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > len) {
			dev_err(dev, "truncated fw: need 0x%x avail 0x%x\n",
					offset + filesz, len);
			ret = -EINVAL;
			break;
		}

		
		ptr = rproc_da_to_va(rproc, da, memsz);
		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%x mem 0x%x\n", da, memsz);
			ret = -EINVAL;
			break;
		}

		
		if (phdr->p_filesz)
			memcpy(ptr, elf_data + phdr->p_offset, filesz);

		if (memsz > filesz)
			memset(ptr + filesz, 0, memsz - filesz);
	}

	return ret;
}

static int
__rproc_handle_vring(struct rproc_vdev *rvdev, struct fw_rsc_vdev *rsc, int i)
{
	struct rproc *rproc = rvdev->rproc;
	struct device *dev = rproc->dev;
	struct fw_rsc_vdev_vring *vring = &rsc->vring[i];
	dma_addr_t dma;
	void *va;
	int ret, size, notifyid;

	dev_dbg(dev, "vdev rsc: vring%d: da %x, qsz %d, align %d\n",
				i, vring->da, vring->num, vring->align);

	
	if (vring->reserved) {
		dev_err(dev, "vring rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	
	if (!vring->num || !vring->align) {
		dev_err(dev, "invalid qsz (%d) or alignment (%d)\n",
						vring->num, vring->align);
		return -EINVAL;
	}

	
	size = PAGE_ALIGN(vring_size(vring->num, vring->align));

	if (!idr_pre_get(&rproc->notifyids, GFP_KERNEL)) {
		dev_err(dev, "idr_pre_get failed\n");
		return -ENOMEM;
	}

	va = dma_alloc_coherent(dev, size, &dma, GFP_KERNEL);
	if (!va) {
		dev_err(dev, "dma_alloc_coherent failed\n");
		return -EINVAL;
	}

	
	
	ret = idr_get_new(&rproc->notifyids, &rvdev->vring[i], &notifyid);
	if (ret) {
		dev_err(dev, "idr_get_new failed: %d\n", ret);
		dma_free_coherent(dev, size, va, dma);
		return ret;
	}

	
	
	vring->da = dma;
	vring->notifyid = notifyid;

	dev_dbg(dev, "vring%d: va %p dma %x size %x idr %d\n", i, va,
					dma, size, notifyid);

	rvdev->vring[i].len = vring->num;
	rvdev->vring[i].align = vring->align;
	rvdev->vring[i].va = va;
	rvdev->vring[i].dma = dma;
	rvdev->vring[i].notifyid = notifyid;
	rvdev->vring[i].rvdev = rvdev;

	return 0;
}

static void __rproc_free_vrings(struct rproc_vdev *rvdev, int i)
{
	struct rproc *rproc = rvdev->rproc;

	for (i--; i >= 0; i--) {
		struct rproc_vring *rvring = &rvdev->vring[i];
		int size = PAGE_ALIGN(vring_size(rvring->len, rvring->align));

		dma_free_coherent(rproc->dev, size, rvring->va, rvring->dma);
		idr_remove(&rproc->notifyids, rvring->notifyid);
	}
}

static int rproc_handle_vdev(struct rproc *rproc, struct fw_rsc_vdev *rsc,
								int avail)
{
	struct device *dev = rproc->dev;
	struct rproc_vdev *rvdev;
	int i, ret;

	
	if (sizeof(*rsc) + rsc->num_of_vrings * sizeof(struct fw_rsc_vdev_vring)
			+ rsc->config_len > avail) {
		dev_err(rproc->dev, "vdev rsc is truncated\n");
		return -EINVAL;
	}

	
	if (rsc->reserved[0] || rsc->reserved[1]) {
		dev_err(dev, "vdev rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	dev_dbg(dev, "vdev rsc: id %d, dfeatures %x, cfg len %d, %d vrings\n",
		rsc->id, rsc->dfeatures, rsc->config_len, rsc->num_of_vrings);

	
	if (rsc->num_of_vrings > ARRAY_SIZE(rvdev->vring)) {
		dev_err(dev, "too many vrings: %d\n", rsc->num_of_vrings);
		return -EINVAL;
	}

	rvdev = kzalloc(sizeof(struct rproc_vdev), GFP_KERNEL);
	if (!rvdev)
		return -ENOMEM;

	rvdev->rproc = rproc;

	
	for (i = 0; i < rsc->num_of_vrings; i++) {
		ret = __rproc_handle_vring(rvdev, rsc, i);
		if (ret)
			goto free_vrings;
	}

	
	rvdev->dfeatures = rsc->dfeatures;

	list_add_tail(&rvdev->node, &rproc->rvdevs);

	
	ret = rproc_add_virtio_dev(rvdev, rsc->id);
	if (ret)
		goto free_vrings;

	return 0;

free_vrings:
	__rproc_free_vrings(rvdev, i);
	kfree(rvdev);
	return ret;
}

static int rproc_handle_trace(struct rproc *rproc, struct fw_rsc_trace *rsc,
								int avail)
{
	struct rproc_mem_entry *trace;
	struct device *dev = rproc->dev;
	void *ptr;
	char name[15];

	if (sizeof(*rsc) > avail) {
		dev_err(rproc->dev, "trace rsc is truncated\n");
		return -EINVAL;
	}

	
	if (rsc->reserved) {
		dev_err(dev, "trace rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	
	ptr = rproc_da_to_va(rproc, rsc->da, rsc->len);
	if (!ptr) {
		dev_err(dev, "erroneous trace resource entry\n");
		return -EINVAL;
	}

	trace = kzalloc(sizeof(*trace), GFP_KERNEL);
	if (!trace) {
		dev_err(dev, "kzalloc trace failed\n");
		return -ENOMEM;
	}

	
	trace->len = rsc->len;
	trace->va = ptr;

	
	snprintf(name, sizeof(name), "trace%d", rproc->num_traces);

	
	trace->priv = rproc_create_trace_file(name, rproc, trace);
	if (!trace->priv) {
		trace->va = NULL;
		kfree(trace);
		return -EINVAL;
	}

	list_add_tail(&trace->node, &rproc->traces);

	rproc->num_traces++;

	dev_dbg(dev, "%s added: va %p, da 0x%x, len 0x%x\n", name, ptr,
						rsc->da, rsc->len);

	return 0;
}

static int rproc_handle_devmem(struct rproc *rproc, struct fw_rsc_devmem *rsc,
								int avail)
{
	struct rproc_mem_entry *mapping;
	int ret;

	
	if (!rproc->domain)
		return -EINVAL;

	if (sizeof(*rsc) > avail) {
		dev_err(rproc->dev, "devmem rsc is truncated\n");
		return -EINVAL;
	}

	
	if (rsc->reserved) {
		dev_err(rproc->dev, "devmem rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping) {
		dev_err(rproc->dev, "kzalloc mapping failed\n");
		return -ENOMEM;
	}

	ret = iommu_map(rproc->domain, rsc->da, rsc->pa, rsc->len, rsc->flags);
	if (ret) {
		dev_err(rproc->dev, "failed to map devmem: %d\n", ret);
		goto out;
	}

	mapping->da = rsc->da;
	mapping->len = rsc->len;
	list_add_tail(&mapping->node, &rproc->mappings);

	dev_dbg(rproc->dev, "mapped devmem pa 0x%x, da 0x%x, len 0x%x\n",
					rsc->pa, rsc->da, rsc->len);

	return 0;

out:
	kfree(mapping);
	return ret;
}

static int rproc_handle_carveout(struct rproc *rproc,
				struct fw_rsc_carveout *rsc, int avail)
{
	struct rproc_mem_entry *carveout, *mapping;
	struct device *dev = rproc->dev;
	dma_addr_t dma;
	void *va;
	int ret;

	if (sizeof(*rsc) > avail) {
		dev_err(rproc->dev, "carveout rsc is truncated\n");
		return -EINVAL;
	}

	
	if (rsc->reserved) {
		dev_err(dev, "carveout rsc has non zero reserved bytes\n");
		return -EINVAL;
	}

	dev_dbg(dev, "carveout rsc: da %x, pa %x, len %x, flags %x\n",
			rsc->da, rsc->pa, rsc->len, rsc->flags);

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping) {
		dev_err(dev, "kzalloc mapping failed\n");
		return -ENOMEM;
	}

	carveout = kzalloc(sizeof(*carveout), GFP_KERNEL);
	if (!carveout) {
		dev_err(dev, "kzalloc carveout failed\n");
		ret = -ENOMEM;
		goto free_mapping;
	}

	va = dma_alloc_coherent(dev, rsc->len, &dma, GFP_KERNEL);
	if (!va) {
		dev_err(dev, "failed to dma alloc carveout: %d\n", rsc->len);
		ret = -ENOMEM;
		goto free_carv;
	}

	dev_dbg(dev, "carveout va %p, dma %x, len 0x%x\n", va, dma, rsc->len);

	if (rproc->domain) {
		ret = iommu_map(rproc->domain, rsc->da, dma, rsc->len,
								rsc->flags);
		if (ret) {
			dev_err(dev, "iommu_map failed: %d\n", ret);
			goto dma_free;
		}

		mapping->da = rsc->da;
		mapping->len = rsc->len;
		list_add_tail(&mapping->node, &rproc->mappings);

		dev_dbg(dev, "carveout mapped 0x%x to 0x%x\n", rsc->da, dma);

		rsc->pa = dma;
	}

	carveout->va = va;
	carveout->len = rsc->len;
	carveout->dma = dma;
	carveout->da = rsc->da;

	list_add_tail(&carveout->node, &rproc->carveouts);

	return 0;

dma_free:
	dma_free_coherent(dev, rsc->len, va, dma);
free_carv:
	kfree(carveout);
free_mapping:
	kfree(mapping);
	return ret;
}

static rproc_handle_resource_t rproc_handle_rsc[] = {
	[RSC_CARVEOUT] = (rproc_handle_resource_t)rproc_handle_carveout,
	[RSC_DEVMEM] = (rproc_handle_resource_t)rproc_handle_devmem,
	[RSC_TRACE] = (rproc_handle_resource_t)rproc_handle_trace,
	[RSC_VDEV] = NULL, 
};

static int
rproc_handle_boot_rsc(struct rproc *rproc, struct resource_table *table, int len)
{
	struct device *dev = rproc->dev;
	rproc_handle_resource_t handler;
	int ret = 0, i;

	for (i = 0; i < table->num; i++) {
		int offset = table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)table + offset;
		int avail = len - offset - sizeof(*hdr);
		void *rsc = (void *)hdr + sizeof(*hdr);

		
		if (avail < 0) {
			dev_err(dev, "rsc table is truncated\n");
			return -EINVAL;
		}

		dev_dbg(dev, "rsc: type %d\n", hdr->type);

		if (hdr->type >= RSC_LAST) {
			dev_warn(dev, "unsupported resource %d\n", hdr->type);
			continue;
		}

		handler = rproc_handle_rsc[hdr->type];
		if (!handler)
			continue;

		ret = handler(rproc, rsc, avail);
		if (ret)
			break;
	}

	return ret;
}

static int
rproc_handle_virtio_rsc(struct rproc *rproc, struct resource_table *table, int len)
{
	struct device *dev = rproc->dev;
	int ret = 0, i;

	for (i = 0; i < table->num; i++) {
		int offset = table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)table + offset;
		int avail = len - offset - sizeof(*hdr);
		struct fw_rsc_vdev *vrsc;

		
		if (avail < 0) {
			dev_err(dev, "rsc table is truncated\n");
			return -EINVAL;
		}

		dev_dbg(dev, "%s: rsc type %d\n", __func__, hdr->type);

		if (hdr->type != RSC_VDEV)
			continue;

		vrsc = (struct fw_rsc_vdev *)hdr->data;

		ret = rproc_handle_vdev(rproc, vrsc, avail);
		if (ret)
			break;
	}

	return ret;
}

static struct resource_table *
rproc_find_rsc_table(struct rproc *rproc, const u8 *elf_data, size_t len,
							int *tablesz)
{
	struct elf32_hdr *ehdr;
	struct elf32_shdr *shdr;
	const char *name_table;
	struct device *dev = rproc->dev;
	struct resource_table *table = NULL;
	int i;

	ehdr = (struct elf32_hdr *)elf_data;
	shdr = (struct elf32_shdr *)(elf_data + ehdr->e_shoff);
	name_table = elf_data + shdr[ehdr->e_shstrndx].sh_offset;

	
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		int size = shdr->sh_size;
		int offset = shdr->sh_offset;

		if (strcmp(name_table + shdr->sh_name, ".resource_table"))
			continue;

		table = (struct resource_table *)(elf_data + offset);

		
		if (offset + size > len) {
			dev_err(dev, "resource table truncated\n");
			return NULL;
		}

		
		if (sizeof(struct resource_table) > size) {
			dev_err(dev, "header-less resource table\n");
			return NULL;
		}

		
		if (table->ver != 1) {
			dev_err(dev, "unsupported fw ver: %d\n", table->ver);
			return NULL;
		}

		
		if (table->reserved[0] || table->reserved[1]) {
			dev_err(dev, "non zero reserved bytes\n");
			return NULL;
		}

		
		if (table->num * sizeof(table->offset[0]) +
				sizeof(struct resource_table) > size) {
			dev_err(dev, "resource table incomplete\n");
			return NULL;
		}

		*tablesz = shdr->sh_size;
		break;
	}

	return table;
}

static void rproc_resource_cleanup(struct rproc *rproc)
{
	struct rproc_mem_entry *entry, *tmp;
	struct device *dev = rproc->dev;

	
	list_for_each_entry_safe(entry, tmp, &rproc->traces, node) {
		rproc_remove_trace_file(entry->priv);
		rproc->num_traces--;
		list_del(&entry->node);
		kfree(entry);
	}

	
	list_for_each_entry_safe(entry, tmp, &rproc->carveouts, node) {
		dma_free_coherent(dev, entry->len, entry->va, entry->dma);
		list_del(&entry->node);
		kfree(entry);
	}

	
	list_for_each_entry_safe(entry, tmp, &rproc->mappings, node) {
		size_t unmapped;

		unmapped = iommu_unmap(rproc->domain, entry->da, entry->len);
		if (unmapped != entry->len) {
			
			dev_err(dev, "failed to unmap %u/%u\n", entry->len,
								unmapped);
		}

		list_del(&entry->node);
		kfree(entry);
	}
}

static int rproc_fw_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
	const char *name = rproc->firmware;
	struct device *dev = rproc->dev;
	struct elf32_hdr *ehdr;
	char class;

	if (!fw) {
		dev_err(dev, "failed to load %s\n", name);
		return -EINVAL;
	}

	if (fw->size < sizeof(struct elf32_hdr)) {
		dev_err(dev, "Image is too small\n");
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;

	
	class = ehdr->e_ident[EI_CLASS];
	if (class != ELFCLASS32) {
		dev_err(dev, "Unsupported class: %d\n", class);
		return -EINVAL;
	}

	
# ifdef __LITTLE_ENDIAN
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
# else 
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
# endif
		dev_err(dev, "Unsupported firmware endianess\n");
		return -EINVAL;
	}

	if (fw->size < ehdr->e_shoff + sizeof(struct elf32_shdr)) {
		dev_err(dev, "Image is too small\n");
		return -EINVAL;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(dev, "Image is corrupted (bad magic)\n");
		return -EINVAL;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(dev, "No loadable segments\n");
		return -EINVAL;
	}

	if (ehdr->e_phoff > fw->size) {
		dev_err(dev, "Firmware size is too small\n");
		return -EINVAL;
	}

	return 0;
}

static int rproc_fw_boot(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = rproc->dev;
	const char *name = rproc->firmware;
	struct elf32_hdr *ehdr;
	struct resource_table *table;
	int ret, tablesz;

	ret = rproc_fw_sanity_check(rproc, fw);
	if (ret)
		return ret;

	ehdr = (struct elf32_hdr *)fw->data;

	dev_info(dev, "Booting fw image %s, size %d\n", name, fw->size);

	ret = rproc_enable_iommu(rproc);
	if (ret) {
		dev_err(dev, "can't enable iommu: %d\n", ret);
		return ret;
	}

	rproc->bootaddr = ehdr->e_entry;

	
	table = rproc_find_rsc_table(rproc, fw->data, fw->size, &tablesz);
	if (!table)
		goto clean_up;

	
	ret = rproc_handle_boot_rsc(rproc, table, tablesz);
	if (ret) {
		dev_err(dev, "Failed to process resources: %d\n", ret);
		goto clean_up;
	}

	
	ret = rproc_load_segments(rproc, fw->data, fw->size);
	if (ret) {
		dev_err(dev, "Failed to load program segments: %d\n", ret);
		goto clean_up;
	}

	
	ret = rproc->ops->start(rproc);
	if (ret) {
		dev_err(dev, "can't start rproc %s: %d\n", rproc->name, ret);
		goto clean_up;
	}

	rproc->state = RPROC_RUNNING;

	dev_info(dev, "remote processor %s is now up\n", rproc->name);

	return 0;

clean_up:
	rproc_resource_cleanup(rproc);
	rproc_disable_iommu(rproc);
	return ret;
}

static void rproc_fw_config_virtio(const struct firmware *fw, void *context)
{
	struct rproc *rproc = context;
	struct resource_table *table;
	int ret, tablesz;

	if (rproc_fw_sanity_check(rproc, fw) < 0)
		goto out;

	
	table = rproc_find_rsc_table(rproc, fw->data, fw->size, &tablesz);
	if (!table)
		goto out;

	
	ret = rproc_handle_virtio_rsc(rproc, table, tablesz);
	if (ret)
		goto out;

out:
	if (fw)
		release_firmware(fw);
	
	complete_all(&rproc->firmware_loading_complete);
}

int rproc_boot(struct rproc *rproc)
{
	const struct firmware *firmware_p;
	struct device *dev;
	int ret;

	if (!rproc) {
		pr_err("invalid rproc handle\n");
		return -EINVAL;
	}

	dev = rproc->dev;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return ret;
	}

	
	if (!rproc->firmware) {
		dev_err(dev, "%s: no firmware to load\n", __func__);
		ret = -EINVAL;
		goto unlock_mutex;
	}

	
	if (!try_module_get(dev->driver->owner)) {
		dev_err(dev, "%s: can't get owner\n", __func__);
		ret = -EINVAL;
		goto unlock_mutex;
	}

	
	if (atomic_inc_return(&rproc->power) > 1) {
		ret = 0;
		goto unlock_mutex;
	}

	dev_info(dev, "powering up %s\n", rproc->name);

	
	ret = request_firmware(&firmware_p, rproc->firmware, dev);
	if (ret < 0) {
		dev_err(dev, "request_firmware failed: %d\n", ret);
		goto downref_rproc;
	}

	ret = rproc_fw_boot(rproc, firmware_p);

	release_firmware(firmware_p);

downref_rproc:
	if (ret) {
		module_put(dev->driver->owner);
		atomic_dec(&rproc->power);
	}
unlock_mutex:
	mutex_unlock(&rproc->lock);
	return ret;
}
EXPORT_SYMBOL(rproc_boot);

void rproc_shutdown(struct rproc *rproc)
{
	struct device *dev = rproc->dev;
	int ret;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret) {
		dev_err(dev, "can't lock rproc %s: %d\n", rproc->name, ret);
		return;
	}

	
	if (!atomic_dec_and_test(&rproc->power))
		goto out;

	
	ret = rproc->ops->stop(rproc);
	if (ret) {
		atomic_inc(&rproc->power);
		dev_err(dev, "can't stop rproc: %d\n", ret);
		goto out;
	}

	
	rproc_resource_cleanup(rproc);

	rproc_disable_iommu(rproc);

	rproc->state = RPROC_OFFLINE;

	dev_info(dev, "stopped remote processor %s\n", rproc->name);

out:
	mutex_unlock(&rproc->lock);
	if (!ret)
		module_put(dev->driver->owner);
}
EXPORT_SYMBOL(rproc_shutdown);

void rproc_release(struct kref *kref)
{
	struct rproc *rproc = container_of(kref, struct rproc, refcount);
	struct rproc_vdev *rvdev, *rvtmp;

	dev_info(rproc->dev, "removing %s\n", rproc->name);

	rproc_delete_debug_dir(rproc);

	
	list_for_each_entry_safe(rvdev, rvtmp, &rproc->rvdevs, node) {
		__rproc_free_vrings(rvdev, RVDEV_NUM_VRINGS);
		list_del(&rvdev->node);
	}

	rproc_free(rproc);
}

static void klist_rproc_get(struct klist_node *n)
{
	struct rproc *rproc = container_of(n, struct rproc, node);

	kref_get(&rproc->refcount);
}

static void klist_rproc_put(struct klist_node *n)
{
	struct rproc *rproc = container_of(n, struct rproc, node);

	kref_put(&rproc->refcount, rproc_release);
}

static struct rproc *next_rproc(struct klist_iter *i)
{
	struct klist_node *n;

	n = klist_next(i);
	if (!n)
		return NULL;

	return container_of(n, struct rproc, node);
}

struct rproc *rproc_get_by_name(const char *name)
{
	struct rproc *rproc;
	struct klist_iter i;
	int ret;

	
	klist_iter_init(&rprocs, &i);
	while ((rproc = next_rproc(&i)) != NULL)
		if (!strcmp(rproc->name, name)) {
			kref_get(&rproc->refcount);
			break;
		}
	klist_iter_exit(&i);

	
	if (!rproc) {
		pr_err("can't find remote processor %s\n", name);
		return NULL;
	}

	ret = rproc_boot(rproc);
	if (ret < 0) {
		kref_put(&rproc->refcount, rproc_release);
		return NULL;
	}

	return rproc;
}
EXPORT_SYMBOL(rproc_get_by_name);

void rproc_put(struct rproc *rproc)
{
	
	rproc_shutdown(rproc);

	
	kref_put(&rproc->refcount, rproc_release);
}
EXPORT_SYMBOL(rproc_put);

int rproc_register(struct rproc *rproc)
{
	struct device *dev = rproc->dev;
	int ret = 0;

	
	klist_add_tail(&rproc->node, &rprocs);

	dev_info(rproc->dev, "%s is available\n", rproc->name);

	dev_info(dev, "Note: remoteproc is still under development and considered experimental.\n");
	dev_info(dev, "THE BINARY FORMAT IS NOT YET FINALIZED, and backward compatibility isn't yet guaranteed.\n");

	
	rproc_create_debug_dir(rproc);

	
	init_completion(&rproc->firmware_loading_complete);

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					rproc->firmware, dev, GFP_KERNEL,
					rproc, rproc_fw_config_virtio);
	if (ret < 0) {
		dev_err(dev, "request_firmware_nowait failed: %d\n", ret);
		complete_all(&rproc->firmware_loading_complete);
		klist_remove(&rproc->node);
	}

	return ret;
}
EXPORT_SYMBOL(rproc_register);

struct rproc *rproc_alloc(struct device *dev, const char *name,
				const struct rproc_ops *ops,
				const char *firmware, int len)
{
	struct rproc *rproc;

	if (!dev || !name || !ops)
		return NULL;

	rproc = kzalloc(sizeof(struct rproc) + len, GFP_KERNEL);
	if (!rproc) {
		dev_err(dev, "%s: kzalloc failed\n", __func__);
		return NULL;
	}

	rproc->dev = dev;
	rproc->name = name;
	rproc->ops = ops;
	rproc->firmware = firmware;
	rproc->priv = &rproc[1];

	atomic_set(&rproc->power, 0);

	kref_init(&rproc->refcount);

	mutex_init(&rproc->lock);

	idr_init(&rproc->notifyids);

	INIT_LIST_HEAD(&rproc->carveouts);
	INIT_LIST_HEAD(&rproc->mappings);
	INIT_LIST_HEAD(&rproc->traces);
	INIT_LIST_HEAD(&rproc->rvdevs);

	rproc->state = RPROC_OFFLINE;

	return rproc;
}
EXPORT_SYMBOL(rproc_alloc);

void rproc_free(struct rproc *rproc)
{
	idr_remove_all(&rproc->notifyids);
	idr_destroy(&rproc->notifyids);

	kfree(rproc);
}
EXPORT_SYMBOL(rproc_free);

int rproc_unregister(struct rproc *rproc)
{
	struct rproc_vdev *rvdev;

	if (!rproc)
		return -EINVAL;

	
	wait_for_completion(&rproc->firmware_loading_complete);

	
	list_for_each_entry(rvdev, &rproc->rvdevs, node)
		rproc_remove_virtio_dev(rvdev);

	
	klist_del(&rproc->node);

	
	kref_put(&rproc->refcount, rproc_release);

	return 0;
}
EXPORT_SYMBOL(rproc_unregister);

static int __init remoteproc_init(void)
{
	rproc_init_debugfs();
	return 0;
}
module_init(remoteproc_init);

static void __exit remoteproc_exit(void)
{
	rproc_exit_debugfs();
}
module_exit(remoteproc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Generic Remote Processor Framework");
