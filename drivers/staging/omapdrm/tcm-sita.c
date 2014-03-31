/*
 * tcm-sita.c
 *
 * SImple Tiler Allocator (SiTA): 2D and 1D allocation(reservation) algorithm
 *
 * Authors: Ravi Ramachandra <r.ramachandra@ti.com>,
 *          Lajos Molnar <molnar@ti.com>
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "tcm-sita.h"

#define ALIGN_DOWN(value, align) ((value) & ~((align) - 1))

static s32 CR_L2R_T2B = CR_BIAS_HORIZONTAL;
static s32 CR_R2L_T2B = CR_DIAGONAL_BALANCE;

static s32 sita_reserve_2d(struct tcm *tcm, u16 h, u16 w, u8 align,
			   struct tcm_area *area);
static s32 sita_reserve_1d(struct tcm *tcm, u32 slots, struct tcm_area *area);
static s32 sita_free(struct tcm *tcm, struct tcm_area *area);
static void sita_deinit(struct tcm *tcm);

static s32 scan_areas_and_find_fit(struct tcm *tcm, u16 w, u16 h, u16 align,
				   struct tcm_area *area);

static s32 scan_l2r_t2b(struct tcm *tcm, u16 w, u16 h, u16 align,
			struct tcm_area *field, struct tcm_area *area);

static s32 scan_r2l_t2b(struct tcm *tcm, u16 w, u16 h, u16 align,
			struct tcm_area *field, struct tcm_area *area);

static s32 scan_r2l_b2t_one_dim(struct tcm *tcm, u32 num_slots,
			struct tcm_area *field, struct tcm_area *area);

static s32 is_area_free(struct tcm_area ***map, u16 x0, u16 y0, u16 w, u16 h);

static s32 update_candidate(struct tcm *tcm, u16 x0, u16 y0, u16 w, u16 h,
			    struct tcm_area *field, s32 criteria,
			    struct score *best);

static void get_nearness_factor(struct tcm_area *field,
				struct tcm_area *candidate,
				struct nearness_factor *nf);

static void get_neighbor_stats(struct tcm *tcm, struct tcm_area *area,
			       struct neighbor_stats *stat);

static void fill_area(struct tcm *tcm,
				struct tcm_area *area, struct tcm_area *parent);



struct tcm *sita_init(u16 width, u16 height, struct tcm_pt *attr)
{
	struct tcm *tcm;
	struct sita_pvt *pvt;
	struct tcm_area area = {0};
	s32 i;

	if (width == 0 || height == 0)
		return NULL;

	tcm = kmalloc(sizeof(*tcm), GFP_KERNEL);
	pvt = kmalloc(sizeof(*pvt), GFP_KERNEL);
	if (!tcm || !pvt)
		goto error;

	memset(tcm, 0, sizeof(*tcm));
	memset(pvt, 0, sizeof(*pvt));

	
	tcm->height = height;
	tcm->width = width;
	tcm->reserve_2d = sita_reserve_2d;
	tcm->reserve_1d = sita_reserve_1d;
	tcm->free = sita_free;
	tcm->deinit = sita_deinit;
	tcm->pvt = (void *)pvt;

	spin_lock_init(&(pvt->lock));

	
	pvt->map = kmalloc(sizeof(*pvt->map) * tcm->width, GFP_KERNEL);
	if (!pvt->map)
		goto error;

	for (i = 0; i < tcm->width; i++) {
		pvt->map[i] =
			kmalloc(sizeof(**pvt->map) * tcm->height,
								GFP_KERNEL);
		if (pvt->map[i] == NULL) {
			while (i--)
				kfree(pvt->map[i]);
			kfree(pvt->map);
			goto error;
		}
	}

	if (attr && attr->x <= tcm->width && attr->y <= tcm->height) {
		pvt->div_pt.x = attr->x;
		pvt->div_pt.y = attr->y;

	} else {
		
		
		pvt->div_pt.x = (tcm->width * 3) / 4;
		pvt->div_pt.y = (tcm->height * 3) / 4;
	}

	spin_lock(&(pvt->lock));
	assign(&area, 0, 0, width - 1, height - 1);
	fill_area(tcm, &area, NULL);
	spin_unlock(&(pvt->lock));
	return tcm;

error:
	kfree(tcm);
	kfree(pvt);
	return NULL;
}

static void sita_deinit(struct tcm *tcm)
{
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	struct tcm_area area = {0};
	s32 i;

	area.p1.x = tcm->width - 1;
	area.p1.y = tcm->height - 1;

	spin_lock(&(pvt->lock));
	fill_area(tcm, &area, NULL);
	spin_unlock(&(pvt->lock));

	for (i = 0; i < tcm->height; i++)
		kfree(pvt->map[i]);
	kfree(pvt->map);
	kfree(pvt);
}

static s32 sita_reserve_1d(struct tcm *tcm, u32 num_slots,
			   struct tcm_area *area)
{
	s32 ret;
	struct tcm_area field = {0};
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	spin_lock(&(pvt->lock));

	
	assign(&field, tcm->width - 1, tcm->height - 1, 0, 0);

	ret = scan_r2l_b2t_one_dim(tcm, num_slots, &field, area);
	if (!ret)
		
		fill_area(tcm, area, area);

	spin_unlock(&(pvt->lock));
	return ret;
}

static s32 sita_reserve_2d(struct tcm *tcm, u16 h, u16 w, u8 align,
			   struct tcm_area *area)
{
	s32 ret;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	
	if (align > 64)
		return -EINVAL;

	
	align = align <= 1 ? 1 : align <= 32 ? 32 : 64;

	spin_lock(&(pvt->lock));
	ret = scan_areas_and_find_fit(tcm, w, h, align, area);
	if (!ret)
		
		fill_area(tcm, area, area);

	spin_unlock(&(pvt->lock));
	return ret;
}

static s32 sita_free(struct tcm *tcm, struct tcm_area *area)
{
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	spin_lock(&(pvt->lock));

	
	WARN_ON(pvt->map[area->p0.x][area->p0.y] != area ||
		pvt->map[area->p1.x][area->p1.y] != area);

	
	fill_area(tcm, area, NULL);

	spin_unlock(&(pvt->lock));

	return 0;
}


static s32 scan_r2l_t2b(struct tcm *tcm, u16 w, u16 h, u16 align,
			struct tcm_area *field, struct tcm_area *area)
{
	s32 x, y;
	s16 start_x, end_x, start_y, end_y, found_x = -1;
	struct tcm_area ***map = ((struct sita_pvt *)tcm->pvt)->map;
	struct score best = {{0}, {0}, {0}, 0};

	start_x = field->p0.x;
	end_x = field->p1.x;
	start_y = field->p0.y;
	end_y = field->p1.y;

	
	if (field->p0.x < field->p1.x ||
	    field->p1.y < field->p0.y)
		return -EINVAL;

	
	if (w > LEN(start_x, end_x) || h > LEN(end_y, start_y))
		return -ENOSPC;

	
	start_x = ALIGN_DOWN(start_x - w + 1, align); 
	end_y = end_y - h + 1;

	
	if (start_x < end_x)
		return -ENOSPC;

	
	for (y = start_y; y <= end_y; y++) {
		for (x = start_x; x >= end_x; x -= align) {
			if (is_area_free(map, x, y, w, h)) {
				found_x = x;

				
				if (update_candidate(tcm, x, y, w, h, field,
							CR_R2L_T2B, &best))
					goto done;

				
				end_x = x + 1;
				break;
			} else if (map[x][y] && map[x][y]->is2d) {
				
				x = ALIGN(map[x][y]->p0.x - w + 1, align);
			}
		}

		
		if (found_x == start_x)
			break;
	}

	if (!best.a.tcm)
		return -ENOSPC;
done:
	assign(area, best.a.p0.x, best.a.p0.y, best.a.p1.x, best.a.p1.y);
	return 0;
}

static s32 scan_l2r_t2b(struct tcm *tcm, u16 w, u16 h, u16 align,
			struct tcm_area *field, struct tcm_area *area)
{
	s32 x, y;
	s16 start_x, end_x, start_y, end_y, found_x = -1;
	struct tcm_area ***map = ((struct sita_pvt *)tcm->pvt)->map;
	struct score best = {{0}, {0}, {0}, 0};

	start_x = field->p0.x;
	end_x = field->p1.x;
	start_y = field->p0.y;
	end_y = field->p1.y;

	
	if (field->p1.x < field->p0.x ||
	    field->p1.y < field->p0.y)
		return -EINVAL;

	
	if (w > LEN(end_x, start_x) || h > LEN(end_y, start_y))
		return -ENOSPC;

	start_x = ALIGN(start_x, align);

	
	if (w > LEN(end_x, start_x))
		return -ENOSPC;

	
	end_x = end_x - w + 1; 
	end_y = end_y - h + 1;

	
	for (y = start_y; y <= end_y; y++) {
		for (x = start_x; x <= end_x; x += align) {
			if (is_area_free(map, x, y, w, h)) {
				found_x = x;

				
				if (update_candidate(tcm, x, y, w, h, field,
							CR_L2R_T2B, &best))
					goto done;
				
				end_x = x - 1;

				break;
			} else if (map[x][y] && map[x][y]->is2d) {
				
				x = ALIGN_DOWN(map[x][y]->p1.x, align);
			}
		}

		
		if (found_x == start_x)
			break;
	}

	if (!best.a.tcm)
		return -ENOSPC;
done:
	assign(area, best.a.p0.x, best.a.p0.y, best.a.p1.x, best.a.p1.y);
	return 0;
}

static s32 scan_r2l_b2t_one_dim(struct tcm *tcm, u32 num_slots,
				struct tcm_area *field, struct tcm_area *area)
{
	s32 found = 0;
	s16 x, y;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	struct tcm_area *p;

	
	if (field->p0.y < field->p1.y)
		return -EINVAL;

	if (tcm->width != field->p0.x - field->p1.x + 1)
		return -EINVAL;

	
	if (num_slots > tcm->width * LEN(field->p0.y, field->p1.y))
		return -ENOSPC;

	x = field->p0.x;
	y = field->p0.y;

	
	while (found < num_slots) {
		if (y < 0)
			return -ENOSPC;

		
		if (found == 0) {
			area->p1.x = x;
			area->p1.y = y;
		}

		
		p = pvt->map[x][y];
		if (p) {
			
			x = p->p0.x;
			if (!p->is2d)
				y = p->p0.y;

			
			found = 0;
		} else {
			
			found++;
			if (found == num_slots)
				break;
		}

		
		if (x == 0)
			y--;
		x = (x ? : tcm->width) - 1;

	}

	
	area->p0.x = x;
	area->p0.y = y;
	return 0;
}

static s32 scan_areas_and_find_fit(struct tcm *tcm, u16 w, u16 h, u16 align,
				   struct tcm_area *area)
{
	s32 ret = 0;
	struct tcm_area field = {0};
	u16 boundary_x, boundary_y;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	if (align > 1) {
		
		boundary_x = pvt->div_pt.x - 1;
		boundary_y = pvt->div_pt.y - 1;

		
		if (w > pvt->div_pt.x)
			boundary_x = tcm->width - 1;
		if (h > pvt->div_pt.y)
			boundary_y = tcm->height - 1;

		assign(&field, 0, 0, boundary_x, boundary_y);
		ret = scan_l2r_t2b(tcm, w, h, align, &field, area);

		
		if (ret != 0 && (boundary_x != tcm->width - 1 ||
				 boundary_y != tcm->height - 1)) {
			
			assign(&field, 0, 0, tcm->width - 1, tcm->height - 1);
			ret = scan_l2r_t2b(tcm, w, h, align, &field, area);
		}
	} else if (align == 1) {
		
		boundary_x = pvt->div_pt.x;
		boundary_y = pvt->div_pt.y - 1;

		
		if (w > (tcm->width - pvt->div_pt.x))
			boundary_x = 0;
		if (h > pvt->div_pt.y)
			boundary_y = tcm->height - 1;

		assign(&field, tcm->width - 1, 0, boundary_x, boundary_y);
		ret = scan_r2l_t2b(tcm, w, h, align, &field, area);

		
		if (ret != 0 && (boundary_x != 0 ||
				 boundary_y != tcm->height - 1)) {
			
			assign(&field, tcm->width - 1, 0, 0, tcm->height - 1);
			ret = scan_r2l_t2b(tcm, w, h, align, &field,
					   area);
		}
	}

	return ret;
}

static s32 is_area_free(struct tcm_area ***map, u16 x0, u16 y0, u16 w, u16 h)
{
	u16 x = 0, y = 0;
	for (y = y0; y < y0 + h; y++) {
		for (x = x0; x < x0 + w; x++) {
			if (map[x][y])
				return false;
		}
	}
	return true;
}

static void fill_area(struct tcm *tcm, struct tcm_area *area,
			struct tcm_area *parent)
{
	s32 x, y;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	struct tcm_area a, a_;

	
	area->tcm = tcm;

	tcm_for_each_slice(a, *area, a_) {
		for (x = a.p0.x; x <= a.p1.x; ++x)
			for (y = a.p0.y; y <= a.p1.y; ++y)
				pvt->map[x][y] = parent;

	}
}

static s32 update_candidate(struct tcm *tcm, u16 x0, u16 y0, u16 w, u16 h,
			    struct tcm_area *field, s32 criteria,
			    struct score *best)
{
	struct score me;	

	bool first = criteria & CR_BIAS_HORIZONTAL;

	assign(&me.a, x0, y0, x0 + w - 1, y0 + h - 1);

	
	if (!first) {
		get_neighbor_stats(tcm, &me.a, &me.n);
		me.neighs = me.n.edge + me.n.busy;
		get_nearness_factor(field, &me.a, &me.f);
	}

	
	if (!best->a.tcm)
		goto better;

	BUG_ON(first);

	
	if ((criteria & CR_DIAGONAL_BALANCE) &&
		best->neighs <= me.neighs &&
		(best->neighs < me.neighs ||
		 
		 best->n.busy < me.n.busy ||
		 (best->n.busy == me.n.busy &&
		  
		  best->f.x + best->f.y > me.f.x + me.f.y)))
		goto better;

	
	return 0;

better:
	
	memcpy(best, &me, sizeof(me));
	best->a.tcm = tcm;
	return first;
}

static void get_nearness_factor(struct tcm_area *field, struct tcm_area *area,
				struct nearness_factor *nf)
{
	nf->x = (s32)(area->p0.x - field->p0.x) * 1000 /
		(field->p1.x - field->p0.x);
	nf->y = (s32)(area->p0.y - field->p0.y) * 1000 /
		(field->p1.y - field->p0.y);
}

static void get_neighbor_stats(struct tcm *tcm, struct tcm_area *area,
			 struct neighbor_stats *stat)
{
	s16 x = 0, y = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	
	memset(stat, 0, sizeof(*stat));

	
	for (x = area->p0.x; x <= area->p1.x; x++) {
		if (area->p0.y == 0)
			stat->edge++;
		else if (pvt->map[x][area->p0.y - 1])
			stat->busy++;

		if (area->p1.y == tcm->height - 1)
			stat->edge++;
		else if (pvt->map[x][area->p1.y + 1])
			stat->busy++;
	}

	
	for (y = area->p0.y; y <= area->p1.y; ++y) {
		if (area->p0.x == 0)
			stat->edge++;
		else if (pvt->map[area->p0.x - 1][y])
			stat->busy++;

		if (area->p1.x == tcm->width - 1)
			stat->edge++;
		else if (pvt->map[area->p1.x + 1][y])
			stat->busy++;
	}
}
