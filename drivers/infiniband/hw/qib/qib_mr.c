/*
 * Copyright (c) 2006, 2007, 2008, 2009 QLogic Corporation. All rights reserved.
 * Copyright (c) 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <rdma/ib_umem.h>
#include <rdma/ib_smi.h>

#include "qib.h"

struct qib_fmr {
	struct ib_fmr ibfmr;
	struct qib_mregion mr;        
};

static inline struct qib_fmr *to_ifmr(struct ib_fmr *ibfmr)
{
	return container_of(ibfmr, struct qib_fmr, ibfmr);
}

struct ib_mr *qib_get_dma_mr(struct ib_pd *pd, int acc)
{
	struct qib_ibdev *dev = to_idev(pd->device);
	struct qib_mr *mr;
	struct ib_mr *ret;
	unsigned long flags;

	if (to_ipd(pd)->user) {
		ret = ERR_PTR(-EPERM);
		goto bail;
	}

	mr = kzalloc(sizeof *mr, GFP_KERNEL);
	if (!mr) {
		ret = ERR_PTR(-ENOMEM);
		goto bail;
	}

	mr->mr.access_flags = acc;
	atomic_set(&mr->mr.refcount, 0);

	spin_lock_irqsave(&dev->lk_table.lock, flags);
	if (!dev->dma_mr)
		dev->dma_mr = &mr->mr;
	spin_unlock_irqrestore(&dev->lk_table.lock, flags);

	ret = &mr->ibmr;

bail:
	return ret;
}

static struct qib_mr *alloc_mr(int count, struct qib_lkey_table *lk_table)
{
	struct qib_mr *mr;
	int m, i = 0;

	
	m = (count + QIB_SEGSZ - 1) / QIB_SEGSZ;
	mr = kmalloc(sizeof *mr + m * sizeof mr->mr.map[0], GFP_KERNEL);
	if (!mr)
		goto done;

	
	for (; i < m; i++) {
		mr->mr.map[i] = kmalloc(sizeof *mr->mr.map[0], GFP_KERNEL);
		if (!mr->mr.map[i])
			goto bail;
	}
	mr->mr.mapsz = m;
	mr->mr.page_shift = 0;
	mr->mr.max_segs = count;

	if (!qib_alloc_lkey(lk_table, &mr->mr))
		goto bail;
	mr->ibmr.lkey = mr->mr.lkey;
	mr->ibmr.rkey = mr->mr.lkey;

	atomic_set(&mr->mr.refcount, 0);
	goto done;

bail:
	while (i)
		kfree(mr->mr.map[--i]);
	kfree(mr);
	mr = NULL;

done:
	return mr;
}

struct ib_mr *qib_reg_phys_mr(struct ib_pd *pd,
			      struct ib_phys_buf *buffer_list,
			      int num_phys_buf, int acc, u64 *iova_start)
{
	struct qib_mr *mr;
	int n, m, i;
	struct ib_mr *ret;

	mr = alloc_mr(num_phys_buf, &to_idev(pd->device)->lk_table);
	if (mr == NULL) {
		ret = ERR_PTR(-ENOMEM);
		goto bail;
	}

	mr->mr.pd = pd;
	mr->mr.user_base = *iova_start;
	mr->mr.iova = *iova_start;
	mr->mr.length = 0;
	mr->mr.offset = 0;
	mr->mr.access_flags = acc;
	mr->umem = NULL;

	m = 0;
	n = 0;
	for (i = 0; i < num_phys_buf; i++) {
		mr->mr.map[m]->segs[n].vaddr = (void *) buffer_list[i].addr;
		mr->mr.map[m]->segs[n].length = buffer_list[i].size;
		mr->mr.length += buffer_list[i].size;
		n++;
		if (n == QIB_SEGSZ) {
			m++;
			n = 0;
		}
	}

	ret = &mr->ibmr;

bail:
	return ret;
}

struct ib_mr *qib_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
			      u64 virt_addr, int mr_access_flags,
			      struct ib_udata *udata)
{
	struct qib_mr *mr;
	struct ib_umem *umem;
	struct ib_umem_chunk *chunk;
	int n, m, i;
	struct ib_mr *ret;

	if (length == 0) {
		ret = ERR_PTR(-EINVAL);
		goto bail;
	}

	umem = ib_umem_get(pd->uobject->context, start, length,
			   mr_access_flags, 0);
	if (IS_ERR(umem))
		return (void *) umem;

	n = 0;
	list_for_each_entry(chunk, &umem->chunk_list, list)
		n += chunk->nents;

	mr = alloc_mr(n, &to_idev(pd->device)->lk_table);
	if (!mr) {
		ret = ERR_PTR(-ENOMEM);
		ib_umem_release(umem);
		goto bail;
	}

	mr->mr.pd = pd;
	mr->mr.user_base = start;
	mr->mr.iova = virt_addr;
	mr->mr.length = length;
	mr->mr.offset = umem->offset;
	mr->mr.access_flags = mr_access_flags;
	mr->umem = umem;

	if (is_power_of_2(umem->page_size))
		mr->mr.page_shift = ilog2(umem->page_size);
	m = 0;
	n = 0;
	list_for_each_entry(chunk, &umem->chunk_list, list) {
		for (i = 0; i < chunk->nents; i++) {
			void *vaddr;

			vaddr = page_address(sg_page(&chunk->page_list[i]));
			if (!vaddr) {
				ret = ERR_PTR(-EINVAL);
				goto bail;
			}
			mr->mr.map[m]->segs[n].vaddr = vaddr;
			mr->mr.map[m]->segs[n].length = umem->page_size;
			n++;
			if (n == QIB_SEGSZ) {
				m++;
				n = 0;
			}
		}
	}
	ret = &mr->ibmr;

bail:
	return ret;
}

int qib_dereg_mr(struct ib_mr *ibmr)
{
	struct qib_mr *mr = to_imr(ibmr);
	struct qib_ibdev *dev = to_idev(ibmr->device);
	int ret;
	int i;

	ret = qib_free_lkey(dev, &mr->mr);
	if (ret)
		return ret;

	i = mr->mr.mapsz;
	while (i)
		kfree(mr->mr.map[--i]);
	if (mr->umem)
		ib_umem_release(mr->umem);
	kfree(mr);
	return 0;
}

struct ib_mr *qib_alloc_fast_reg_mr(struct ib_pd *pd, int max_page_list_len)
{
	struct qib_mr *mr;

	mr = alloc_mr(max_page_list_len, &to_idev(pd->device)->lk_table);
	if (mr == NULL)
		return ERR_PTR(-ENOMEM);

	mr->mr.pd = pd;
	mr->mr.user_base = 0;
	mr->mr.iova = 0;
	mr->mr.length = 0;
	mr->mr.offset = 0;
	mr->mr.access_flags = 0;
	mr->umem = NULL;

	return &mr->ibmr;
}

struct ib_fast_reg_page_list *
qib_alloc_fast_reg_page_list(struct ib_device *ibdev, int page_list_len)
{
	unsigned size = page_list_len * sizeof(u64);
	struct ib_fast_reg_page_list *pl;

	if (size > PAGE_SIZE)
		return ERR_PTR(-EINVAL);

	pl = kmalloc(sizeof *pl, GFP_KERNEL);
	if (!pl)
		return ERR_PTR(-ENOMEM);

	pl->page_list = kmalloc(size, GFP_KERNEL);
	if (!pl->page_list)
		goto err_free;

	return pl;

err_free:
	kfree(pl);
	return ERR_PTR(-ENOMEM);
}

void qib_free_fast_reg_page_list(struct ib_fast_reg_page_list *pl)
{
	kfree(pl->page_list);
	kfree(pl);
}

struct ib_fmr *qib_alloc_fmr(struct ib_pd *pd, int mr_access_flags,
			     struct ib_fmr_attr *fmr_attr)
{
	struct qib_fmr *fmr;
	int m, i = 0;
	struct ib_fmr *ret;

	
	m = (fmr_attr->max_pages + QIB_SEGSZ - 1) / QIB_SEGSZ;
	fmr = kmalloc(sizeof *fmr + m * sizeof fmr->mr.map[0], GFP_KERNEL);
	if (!fmr)
		goto bail;

	
	for (; i < m; i++) {
		fmr->mr.map[i] = kmalloc(sizeof *fmr->mr.map[0],
					 GFP_KERNEL);
		if (!fmr->mr.map[i])
			goto bail;
	}
	fmr->mr.mapsz = m;

	if (!qib_alloc_lkey(&to_idev(pd->device)->lk_table, &fmr->mr))
		goto bail;
	fmr->ibfmr.rkey = fmr->mr.lkey;
	fmr->ibfmr.lkey = fmr->mr.lkey;
	fmr->mr.pd = pd;
	fmr->mr.user_base = 0;
	fmr->mr.iova = 0;
	fmr->mr.length = 0;
	fmr->mr.offset = 0;
	fmr->mr.access_flags = mr_access_flags;
	fmr->mr.max_segs = fmr_attr->max_pages;
	fmr->mr.page_shift = fmr_attr->page_shift;

	atomic_set(&fmr->mr.refcount, 0);
	ret = &fmr->ibfmr;
	goto done;

bail:
	while (i)
		kfree(fmr->mr.map[--i]);
	kfree(fmr);
	ret = ERR_PTR(-ENOMEM);

done:
	return ret;
}


int qib_map_phys_fmr(struct ib_fmr *ibfmr, u64 *page_list,
		     int list_len, u64 iova)
{
	struct qib_fmr *fmr = to_ifmr(ibfmr);
	struct qib_lkey_table *rkt;
	unsigned long flags;
	int m, n, i;
	u32 ps;
	int ret;

	if (atomic_read(&fmr->mr.refcount))
		return -EBUSY;

	if (list_len > fmr->mr.max_segs) {
		ret = -EINVAL;
		goto bail;
	}
	rkt = &to_idev(ibfmr->device)->lk_table;
	spin_lock_irqsave(&rkt->lock, flags);
	fmr->mr.user_base = iova;
	fmr->mr.iova = iova;
	ps = 1 << fmr->mr.page_shift;
	fmr->mr.length = list_len * ps;
	m = 0;
	n = 0;
	for (i = 0; i < list_len; i++) {
		fmr->mr.map[m]->segs[n].vaddr = (void *) page_list[i];
		fmr->mr.map[m]->segs[n].length = ps;
		if (++n == QIB_SEGSZ) {
			m++;
			n = 0;
		}
	}
	spin_unlock_irqrestore(&rkt->lock, flags);
	ret = 0;

bail:
	return ret;
}

int qib_unmap_fmr(struct list_head *fmr_list)
{
	struct qib_fmr *fmr;
	struct qib_lkey_table *rkt;
	unsigned long flags;

	list_for_each_entry(fmr, fmr_list, ibfmr.list) {
		rkt = &to_idev(fmr->ibfmr.device)->lk_table;
		spin_lock_irqsave(&rkt->lock, flags);
		fmr->mr.user_base = 0;
		fmr->mr.iova = 0;
		fmr->mr.length = 0;
		spin_unlock_irqrestore(&rkt->lock, flags);
	}
	return 0;
}

int qib_dealloc_fmr(struct ib_fmr *ibfmr)
{
	struct qib_fmr *fmr = to_ifmr(ibfmr);
	int ret;
	int i;

	ret = qib_free_lkey(to_idev(ibfmr->device), &fmr->mr);
	if (ret)
		return ret;

	i = fmr->mr.mapsz;
	while (i)
		kfree(fmr->mr.map[--i]);
	kfree(fmr);
	return 0;
}
