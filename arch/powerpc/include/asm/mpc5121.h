/*
 * MPC5121 Prototypes and definitions
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.
 */

#ifndef __ASM_POWERPC_MPC5121_H__
#define __ASM_POWERPC_MPC5121_H__

struct mpc512x_reset_module {
	u32	rcwlr;	
	u32	rcwhr;	
	u32	reserved1;
	u32	reserved2;
	u32	rsr;	
	u32	rmr;	
	u32	rpr;	
	u32	rcr;	
	u32	rcer;	
};

struct mpc512x_ccm {
	u32	spmr;	
	u32	sccr1;	
	u32	sccr2;	
	u32	scfr1;	
	u32	scfr2;	
	u32	scfr2s;	
	u32	bcr;	
	u32	p0ccr;	
	u32	p1ccr;	
	u32	p2ccr;	
	u32	p3ccr;	
	u32	p4ccr;	
	u32	p5ccr;	
	u32	p6ccr;	
	u32	p7ccr;	
	u32	p8ccr;	
	u32	p9ccr;	
	u32	p10ccr;	
	u32	p11ccr;	
	u32	spccr;	
	u32	cccr;	
	u32	dccr;	
	u32	m1ccr;	
	u32	m2ccr;	
	u32	m3ccr;	
	u32	m4ccr;	
	u8	res[0x98]; 
};
#endif 
