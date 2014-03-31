/* mpi-div.c  -  MPI functions
 *	Copyright (C) 1994, 1996 Free Software Foundation, Inc.
 *	Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
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

#include <linux/string.h>
#include "mpi-internal.h"
#include "longlong.h"

int mpi_fdiv_r(MPI rem, MPI dividend, MPI divisor)
{
	int rc = -ENOMEM;
	int divisor_sign = divisor->sign;
	MPI temp_divisor = NULL;

	if (rem == divisor) {
		if (mpi_copy(&temp_divisor, divisor) < 0)
			goto nomem;
		divisor = temp_divisor;
	}

	if (mpi_tdiv_qr(NULL, rem, dividend, divisor) < 0)
		goto nomem;
	if (((divisor_sign ? 1 : 0) ^ (dividend->sign ? 1 : 0)) && rem->nlimbs)
		if (mpi_add(rem, rem, divisor) < 0)
			goto nomem;

	rc = 0;

nomem:
	if (temp_divisor)
		mpi_free(temp_divisor);
	return rc;
}


ulong mpi_fdiv_r_ui(MPI rem, MPI dividend, ulong divisor)
{
	mpi_limb_t rlimb;

	rlimb = mpihelp_mod_1(dividend->d, dividend->nlimbs, divisor);
	if (rlimb && dividend->sign)
		rlimb = divisor - rlimb;

	if (rem) {
		rem->d[0] = rlimb;
		rem->nlimbs = rlimb ? 1 : 0;
	}
	return rlimb;
}

int mpi_fdiv_q(MPI quot, MPI dividend, MPI divisor)
{
	MPI tmp = mpi_alloc(mpi_get_nlimbs(quot));
	if (!tmp)
		return -ENOMEM;
	mpi_fdiv_qr(quot, tmp, dividend, divisor);
	mpi_free(tmp);
	return 0;
}

int mpi_fdiv_qr(MPI quot, MPI rem, MPI dividend, MPI divisor)
{
	int divisor_sign = divisor->sign;
	MPI temp_divisor = NULL;

	if (quot == divisor || rem == divisor) {
		if (mpi_copy(&temp_divisor, divisor) < 0)
			return -ENOMEM;
		divisor = temp_divisor;
	}

	if (mpi_tdiv_qr(quot, rem, dividend, divisor) < 0)
		goto nomem;

	if ((divisor_sign ^ dividend->sign) && rem->nlimbs) {
		if (mpi_sub_ui(quot, quot, 1) < 0)
			goto nomem;
		if (mpi_add(rem, rem, divisor) < 0)
			goto nomem;
	}

	if (temp_divisor)
		mpi_free(temp_divisor);

	return 0;

nomem:
	mpi_free(temp_divisor);
	return -ENOMEM;
}


int mpi_tdiv_r(MPI rem, MPI num, MPI den)
{
	return mpi_tdiv_qr(NULL, rem, num, den);
}

int mpi_tdiv_qr(MPI quot, MPI rem, MPI num, MPI den)
{
	int rc = -ENOMEM;
	mpi_ptr_t np, dp;
	mpi_ptr_t qp, rp;
	mpi_size_t nsize = num->nlimbs;
	mpi_size_t dsize = den->nlimbs;
	mpi_size_t qsize, rsize;
	mpi_size_t sign_remainder = num->sign;
	mpi_size_t sign_quotient = num->sign ^ den->sign;
	unsigned normalization_steps;
	mpi_limb_t q_limb;
	mpi_ptr_t marker[5];
	int markidx = 0;

	if (!dsize)
		return -EINVAL;

	memset(marker, 0, sizeof(marker));

	rsize = nsize + 1;
	if (mpi_resize(rem, rsize) < 0)
		goto nomem;

	qsize = rsize - dsize;	
	if (qsize <= 0) {
		if (num != rem) {
			rem->nlimbs = num->nlimbs;
			rem->sign = num->sign;
			MPN_COPY(rem->d, num->d, nsize);
		}
		if (quot) {
			quot->nlimbs = 0;
			quot->sign = 0;
		}
		return 0;
	}

	if (quot)
		if (mpi_resize(quot, qsize) < 0)
			goto nomem;

	
	np = num->d;
	dp = den->d;
	rp = rem->d;

	
	if (dsize == 1) {
		mpi_limb_t rlimb;
		if (quot) {
			qp = quot->d;
			rlimb = mpihelp_divmod_1(qp, np, nsize, dp[0]);
			qsize -= qp[qsize - 1] == 0;
			quot->nlimbs = qsize;
			quot->sign = sign_quotient;
		} else
			rlimb = mpihelp_mod_1(np, nsize, dp[0]);
		rp[0] = rlimb;
		rsize = rlimb != 0 ? 1 : 0;
		rem->nlimbs = rsize;
		rem->sign = sign_remainder;
		return 0;
	}

	if (quot) {
		qp = quot->d;
		/* Make sure QP and NP point to different objects.  Otherwise the
		 * numerator would be gradually overwritten by the quotient limbs.  */
		if (qp == np) {	
			np = marker[markidx++] = mpi_alloc_limb_space(nsize);
			if (!np)
				goto nomem;
			MPN_COPY(np, qp, nsize);
		}
	} else			
		qp = rp + dsize;

	count_leading_zeros(normalization_steps, dp[dsize - 1]);

	if (normalization_steps) {
		mpi_ptr_t tp;
		mpi_limb_t nlimb;

		tp = marker[markidx++] = mpi_alloc_limb_space(dsize);
		if (!tp)
			goto nomem;
		mpihelp_lshift(tp, dp, dsize, normalization_steps);
		dp = tp;

		nlimb = mpihelp_lshift(rp, np, nsize, normalization_steps);
		if (nlimb) {
			rp[nsize] = nlimb;
			rsize = nsize + 1;
		} else
			rsize = nsize;
	} else {
		if (dp == rp || (quot && (dp == qp))) {
			mpi_ptr_t tp;

			tp = marker[markidx++] = mpi_alloc_limb_space(dsize);
			if (!tp)
				goto nomem;
			MPN_COPY(tp, dp, dsize);
			dp = tp;
		}

		
		if (rp != np)
			MPN_COPY(rp, np, nsize);

		rsize = nsize;
	}

	q_limb = mpihelp_divrem(qp, 0, rp, rsize, dp, dsize);

	if (quot) {
		qsize = rsize - dsize;
		if (q_limb) {
			qp[qsize] = q_limb;
			qsize += 1;
		}

		quot->nlimbs = qsize;
		quot->sign = sign_quotient;
	}

	rsize = dsize;
	MPN_NORMALIZE(rp, rsize);

	if (normalization_steps && rsize) {
		mpihelp_rshift(rp, rp, rsize, normalization_steps);
		rsize -= rp[rsize - 1] == 0 ? 1 : 0;
	}

	rem->nlimbs = rsize;
	rem->sign = sign_remainder;

	rc = 0;
nomem:
	while (markidx)
		mpi_free_limb_space(marker[--markidx]);
	return rc;
}

int mpi_tdiv_q_2exp(MPI w, MPI u, unsigned count)
{
	mpi_size_t usize, wsize;
	mpi_size_t limb_cnt;

	usize = u->nlimbs;
	limb_cnt = count / BITS_PER_MPI_LIMB;
	wsize = usize - limb_cnt;
	if (limb_cnt >= usize)
		w->nlimbs = 0;
	else {
		mpi_ptr_t wp;
		mpi_ptr_t up;

		if (RESIZE_IF_NEEDED(w, wsize) < 0)
			return -ENOMEM;
		wp = w->d;
		up = u->d;

		count %= BITS_PER_MPI_LIMB;
		if (count) {
			mpihelp_rshift(wp, up + limb_cnt, wsize, count);
			wsize -= !wp[wsize - 1];
		} else {
			MPN_COPY_INCR(wp, up + limb_cnt, wsize);
		}

		w->nlimbs = wsize;
	}
	return 0;
}

int mpi_divisible_ui(MPI dividend, ulong divisor)
{
	return !mpihelp_mod_1(dividend->d, dividend->nlimbs, divisor);
}
