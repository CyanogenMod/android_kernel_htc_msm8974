/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright 2002 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */


#include <linux/export.h>
#include <linux/raid/pq.h>

void raid6_2data_recov(int disks, size_t bytes, int faila, int failb,
		       void **ptrs)
{
	u8 *p, *q, *dp, *dq;
	u8 px, qx, db;
	const u8 *pbmul;	
	const u8 *qmul;		

	p = (u8 *)ptrs[disks-2];
	q = (u8 *)ptrs[disks-1];

	dp = (u8 *)ptrs[faila];
	ptrs[faila] = (void *)raid6_empty_zero_page;
	ptrs[disks-2] = dp;
	dq = (u8 *)ptrs[failb];
	ptrs[failb] = (void *)raid6_empty_zero_page;
	ptrs[disks-1] = dq;

	raid6_call.gen_syndrome(disks, bytes, ptrs);

	
	ptrs[faila]   = dp;
	ptrs[failb]   = dq;
	ptrs[disks-2] = p;
	ptrs[disks-1] = q;

	
	pbmul = raid6_gfmul[raid6_gfexi[failb-faila]];
	qmul  = raid6_gfmul[raid6_gfinv[raid6_gfexp[faila]^raid6_gfexp[failb]]];

	
	while ( bytes-- ) {
		px    = *p ^ *dp;
		qx    = qmul[*q ^ *dq];
		*dq++ = db = pbmul[px] ^ qx; 
		*dp++ = db ^ px; 
		p++; q++;
	}
}
EXPORT_SYMBOL_GPL(raid6_2data_recov);

void raid6_datap_recov(int disks, size_t bytes, int faila, void **ptrs)
{
	u8 *p, *q, *dq;
	const u8 *qmul;		

	p = (u8 *)ptrs[disks-2];
	q = (u8 *)ptrs[disks-1];

	dq = (u8 *)ptrs[faila];
	ptrs[faila] = (void *)raid6_empty_zero_page;
	ptrs[disks-1] = dq;

	raid6_call.gen_syndrome(disks, bytes, ptrs);

	
	ptrs[faila]   = dq;
	ptrs[disks-1] = q;

	
	qmul  = raid6_gfmul[raid6_gfinv[raid6_gfexp[faila]]];

	
	while ( bytes-- ) {
		*p++ ^= *dq = qmul[*q ^ *dq];
		q++; dq++;
	}
}
EXPORT_SYMBOL_GPL(raid6_datap_recov);

#ifndef __KERNEL__

void raid6_dual_recov(int disks, size_t bytes, int faila, int failb, void **ptrs)
{
	if ( faila > failb ) {
		int tmp = faila;
		faila = failb;
		failb = tmp;
	}

	if ( failb == disks-1 ) {
		if ( faila == disks-2 ) {
			
			raid6_call.gen_syndrome(disks, bytes, ptrs);
		} else {
			
		}
	} else {
		if ( failb == disks-2 ) {
			
			raid6_datap_recov(disks, bytes, faila, ptrs);
		} else {
			
			raid6_2data_recov(disks, bytes, faila, failb, ptrs);
		}
	}
}

#endif
