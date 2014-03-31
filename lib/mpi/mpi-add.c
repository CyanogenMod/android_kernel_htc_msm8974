/* mpi-add.c  -  MPI functions
 *	Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
 *	Copyright (C) 1994, 1996 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 * Note: This code is heavily based on the GNU MP Library.
 *	 Actually it's the same code with only minor changes in the
 *	 way the data is stored; this is to support the abstraction
 *	 of an optional secure memory allocation which may be used
 *	 to avoid revealing of sensitive data due to paging etc.
 *	 The GNU MP Library itself is published under the LGPL;
 *	 however I decided to publish this code under the plain GPL.
 */

#include "mpi-internal.h"

int mpi_add_ui(MPI w, const MPI u, unsigned long v)
{
	mpi_ptr_t wp, up;
	mpi_size_t usize, wsize;
	int usign, wsign;

	usize = u->nlimbs;
	usign = u->sign;
	wsign = 0;

	
	wsize = usize + 1;
	if (w->alloced < wsize)
		if (mpi_resize(w, wsize) < 0)
			return -ENOMEM;

	
	up = u->d;
	wp = w->d;

	if (!usize) {		
		wp[0] = v;
		wsize = v ? 1 : 0;
	} else if (!usign) {	
		mpi_limb_t cy;
		cy = mpihelp_add_1(wp, up, usize, v);
		wp[usize] = cy;
		wsize = usize + cy;
	} else {		
		if (usize == 1 && up[0] < v) {
			wp[0] = v - up[0];
			wsize = 1;
		} else {
			mpihelp_sub_1(wp, up, usize, v);
			
			wsize = usize - (wp[usize - 1] == 0);
			wsign = 1;
		}
	}

	w->nlimbs = wsize;
	w->sign = wsign;
	return 0;
}

int mpi_add(MPI w, MPI u, MPI v)
{
	mpi_ptr_t wp, up, vp;
	mpi_size_t usize, vsize, wsize;
	int usign, vsign, wsign;

	if (u->nlimbs < v->nlimbs) {	
		usize = v->nlimbs;
		usign = v->sign;
		vsize = u->nlimbs;
		vsign = u->sign;
		wsize = usize + 1;
		if (RESIZE_IF_NEEDED(w, wsize) < 0)
			return -ENOMEM;
		
		up = v->d;
		vp = u->d;
	} else {
		usize = u->nlimbs;
		usign = u->sign;
		vsize = v->nlimbs;
		vsign = v->sign;
		wsize = usize + 1;
		if (RESIZE_IF_NEEDED(w, wsize) < 0)
			return -ENOMEM;
		
		up = u->d;
		vp = v->d;
	}
	wp = w->d;
	wsign = 0;

	if (!vsize) {		
		MPN_COPY(wp, up, usize);
		wsize = usize;
		wsign = usign;
	} else if (usign != vsign) {	
		
		if (usize != vsize) {
			mpihelp_sub(wp, up, usize, vp, vsize);
			wsize = usize;
			MPN_NORMALIZE(wp, wsize);
			wsign = usign;
		} else if (mpihelp_cmp(up, vp, usize) < 0) {
			mpihelp_sub_n(wp, vp, up, usize);
			wsize = usize;
			MPN_NORMALIZE(wp, wsize);
			if (!usign)
				wsign = 1;
		} else {
			mpihelp_sub_n(wp, up, vp, usize);
			wsize = usize;
			MPN_NORMALIZE(wp, wsize);
			if (usign)
				wsign = 1;
		}
	} else {		
		mpi_limb_t cy = mpihelp_add(wp, up, usize, vp, vsize);
		wp[usize] = cy;
		wsize = usize + cy;
		if (usign)
			wsign = 1;
	}

	w->nlimbs = wsize;
	w->sign = wsign;
	return 0;
}

int mpi_sub_ui(MPI w, MPI u, unsigned long v)
{
	mpi_ptr_t wp, up;
	mpi_size_t usize, wsize;
	int usign, wsign;

	usize = u->nlimbs;
	usign = u->sign;
	wsign = 0;

	
	wsize = usize + 1;
	if (w->alloced < wsize)
		if (mpi_resize(w, wsize) < 0)
			return -ENOMEM;

	
	up = u->d;
	wp = w->d;

	if (!usize) {		
		wp[0] = v;
		wsize = v ? 1 : 0;
		wsign = 1;
	} else if (usign) {	
		mpi_limb_t cy;
		cy = mpihelp_add_1(wp, up, usize, v);
		wp[usize] = cy;
		wsize = usize + cy;
	} else {		
		if (usize == 1 && up[0] < v) {
			wp[0] = v - up[0];
			wsize = 1;
			wsign = 1;
		} else {
			mpihelp_sub_1(wp, up, usize, v);
			
			wsize = usize - (wp[usize - 1] == 0);
		}
	}

	w->nlimbs = wsize;
	w->sign = wsign;
	return 0;
}

int mpi_sub(MPI w, MPI u, MPI v)
{
	int rc;

	if (w == v) {
		MPI vv;
		if (mpi_copy(&vv, v) < 0)
			return -ENOMEM;
		vv->sign = !vv->sign;
		rc = mpi_add(w, u, vv);
		mpi_free(vv);
	} else {
		
		v->sign = !v->sign;
		rc = mpi_add(w, u, v);
		v->sign = !v->sign;
	}
	return rc;
}

int mpi_addm(MPI w, MPI u, MPI v, MPI m)
{
	if (mpi_add(w, u, v) < 0 || mpi_fdiv_r(w, w, m) < 0)
		return -ENOMEM;
	return 0;
}

int mpi_subm(MPI w, MPI u, MPI v, MPI m)
{
	if (mpi_sub(w, u, v) < 0 || mpi_fdiv_r(w, w, m) < 0)
		return -ENOMEM;
	return 0;
}
