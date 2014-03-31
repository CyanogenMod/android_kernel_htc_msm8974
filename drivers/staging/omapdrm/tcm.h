/*
 * tcm.h
 *
 * TILER container manager specification and support functions for TI
 * TILER driver.
 *
 * Author: Lajos Molnar <molnar@ti.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TCM_H
#define TCM_H

struct tcm;

struct tcm_pt {
	u16 x;
	u16 y;
};

struct tcm_area {
	bool is2d;		
	struct tcm    *tcm;	
	struct tcm_pt  p0;
	struct tcm_pt  p1;
};

struct tcm {
	u16 width, height;	
	int lut_id;		

	void *pvt;

	
	s32 (*reserve_2d)(struct tcm *tcm, u16 height, u16 width, u8 align,
			  struct tcm_area *area);
	s32 (*reserve_1d)(struct tcm *tcm, u32 slots, struct tcm_area *area);
	s32 (*free)      (struct tcm *tcm, struct tcm_area *area);
	void (*deinit)   (struct tcm *tcm);
};



struct tcm *sita_init(u16 width, u16 height, struct tcm_pt *attr);


static inline void tcm_deinit(struct tcm *tcm)
{
	if (tcm)
		tcm->deinit(tcm);
}

static inline s32 tcm_reserve_2d(struct tcm *tcm, u16 width, u16 height,
				 u16 align, struct tcm_area *area)
{
	
	s32 res = tcm  == NULL ? -ENODEV :
		(area == NULL || width == 0 || height == 0 ||
		 
		 (align & (align - 1))) ? -EINVAL :
		(height > tcm->height || width > tcm->width) ? -ENOMEM : 0;

	if (!res) {
		area->is2d = true;
		res = tcm->reserve_2d(tcm, height, width, align, area);
		area->tcm = res ? NULL : tcm;
	}

	return res;
}

static inline s32 tcm_reserve_1d(struct tcm *tcm, u32 slots,
				 struct tcm_area *area)
{
	
	s32 res = tcm  == NULL ? -ENODEV :
		(area == NULL || slots == 0) ? -EINVAL :
		slots > (tcm->width * (u32) tcm->height) ? -ENOMEM : 0;

	if (!res) {
		area->is2d = false;
		res = tcm->reserve_1d(tcm, slots, area);
		area->tcm = res ? NULL : tcm;
	}

	return res;
}

static inline s32 tcm_free(struct tcm_area *area)
{
	s32 res = 0; 

	if (area && area->tcm) {
		res = area->tcm->free(area->tcm, area);
		if (res == 0)
			area->tcm = NULL;
	}

	return res;
}


static inline void tcm_slice(struct tcm_area *parent, struct tcm_area *slice)
{
	*slice = *parent;

	
	if (slice->tcm && !slice->is2d &&
		slice->p0.y != slice->p1.y &&
		(slice->p0.x || (slice->p1.x != slice->tcm->width - 1))) {
		
		slice->p1.x = slice->tcm->width - 1;
		slice->p1.y = (slice->p0.x) ? slice->p0.y : slice->p1.y - 1;
		
		parent->p0.x = 0;
		parent->p0.y = slice->p1.y + 1;
	} else {
		
		parent->tcm = NULL;
	}
}

static inline bool tcm_area_is_valid(struct tcm_area *area)
{
	return area && area->tcm &&
		
		area->p1.x < area->tcm->width &&
		area->p1.y < area->tcm->height &&
		area->p0.y <= area->p1.y &&
		
		((!area->is2d &&
		  area->p0.x < area->tcm->width &&
		  area->p0.x + area->p0.y * area->tcm->width <=
		  area->p1.x + area->p1.y * area->tcm->width) ||
		 
		 (area->is2d &&
		  area->p0.x <= area->p1.x));
}

static inline bool __tcm_is_in(struct tcm_pt *p, struct tcm_area *a)
{
	u16 i;

	if (a->is2d) {
		return p->x >= a->p0.x && p->x <= a->p1.x &&
		       p->y >= a->p0.y && p->y <= a->p1.y;
	} else {
		i = p->x + p->y * a->tcm->width;
		return i >= a->p0.x + a->p0.y * a->tcm->width &&
		       i <= a->p1.x + a->p1.y * a->tcm->width;
	}
}

static inline u16 __tcm_area_width(struct tcm_area *area)
{
	return area->p1.x - area->p0.x + 1;
}

static inline u16 __tcm_area_height(struct tcm_area *area)
{
	return area->p1.y - area->p0.y + 1;
}

static inline u16 __tcm_sizeof(struct tcm_area *area)
{
	return area->is2d ?
		__tcm_area_width(area) * __tcm_area_height(area) :
		(area->p1.x - area->p0.x + 1) + (area->p1.y - area->p0.y) *
							area->tcm->width;
}
#define tcm_sizeof(area) __tcm_sizeof(&(area))
#define tcm_awidth(area) __tcm_area_width(&(area))
#define tcm_aheight(area) __tcm_area_height(&(area))
#define tcm_is_in(pt, area) __tcm_is_in(&(pt), &(area))

static inline s32 tcm_1d_limit(struct tcm_area *a, u32 num_pg)
{
	if (__tcm_sizeof(a) < num_pg)
		return -ENOMEM;
	if (!num_pg)
		return -EINVAL;

	a->p1.x = (a->p0.x + num_pg - 1) % a->tcm->width;
	a->p1.y = a->p0.y + ((a->p0.x + num_pg - 1) / a->tcm->width);
	return 0;
}

#define tcm_for_each_slice(var, area, safe) \
	for (safe = area, \
	     tcm_slice(&safe, &var); \
	     var.tcm; tcm_slice(&safe, &var))

#endif
