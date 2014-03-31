/*
 * Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *
 * Floating-point emulation code
 *  Copyright (C) 2001 Hewlett-Packard (Paul Bame) <bame@debian.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define FPUDEBUG 0

#include "float.h"
#include <linux/bug.h>
#include <linux/kernel.h>
#include <asm/processor.h>

#define COPR_INST 0x30000000

#define extru(r,pos,len)	(((r) >> (31-(pos))) & (( 1 << (len)) - 1))
#define fpmajorpos 5
#define fpr1pos	10
#define fpr2pos 15
#define fptpos	31
#define fpsubpos 18
#define fpclass1subpos 16
#define fpclasspos 22
#define fpfmtpos 20
#define fpdfpos 18
#define fpnulpos 26
#define fpxr1pos 24
#define fpxr2pos 19
#define fpxtpos 25
#define fpxpos 23
#define fp0efmtpos 20
#define fprm1pos 10
#define fprm2pos 15
#define fptmpos 31
#define fprapos 25
#define fptapos 20
#define fpmultifmt 26
     
     
#define fpraupos 18
#define fpxrm2pos 19
     
#define fpralpos 23
#define fpxrm1pos 24
     
#define fpfusedsubop 26
     

#define fpzeroreg (32*sizeof(double)/sizeof(u_int))

#define get_major(op) extru(op,fpmajorpos,6)
#define get_class(op) extru(op,fpclasspos,2)
#define get_subop(op) extru(op,fpsubpos,3)
#define get_subop1_PA1_1(op) extru(op,fpclass1subpos,2)	
#define get_subop1_PA2_0(op) extru(op,fpclass1subpos,3)	

#define MAJOR_0C_EXCP	0x09
#define MAJOR_0E_EXCP	0x0b
#define MAJOR_06_EXCP	0x03
#define MAJOR_26_EXCP	0x23
#define MAJOR_2E_EXCP	0x2b
#define PA83_UNIMP_EXCP	0x01


#define FPU_TYPE_FLAG_POS (EM_FPU_TYPE_OFFSET>>2)
#define TIMEX_ROLEX_FPU_MASK (TIMEX_EXTEN_FLAG|ROLEX_EXTEN_FLAG)

#define _PROTOTYPES
#if defined(_PROTOTYPES) || defined(_lint)
static u_int decode_0c(u_int, u_int, u_int, u_int *);
static u_int decode_0e(u_int, u_int, u_int, u_int *);
static u_int decode_06(u_int, u_int *);
static u_int decode_26(u_int, u_int *);
static u_int decode_2e(u_int, u_int *);
static void update_status_cbit(u_int *, u_int, u_int, u_int);
#else 
static u_int decode_0c();
static u_int decode_0e();
static u_int decode_06();
static u_int decode_26();
static u_int decode_2e();
static void update_status_cbit();
#endif 

#define VASSERT(x)

static void parisc_linux_get_fpu_type(u_int fpregs[])
{
 
	if (boot_cpu_data.cpu_type == pcxs)
		fpregs[FPU_TYPE_FLAG_POS] = TIMEX_EXTEN_FLAG;
	else if (boot_cpu_data.cpu_type == pcxt ||
	         boot_cpu_data.cpu_type == pcxt_)
		fpregs[FPU_TYPE_FLAG_POS] = ROLEX_EXTEN_FLAG;
	else if (boot_cpu_data.cpu_type >= pcxu)
		fpregs[FPU_TYPE_FLAG_POS] = PA2_0_FPU_FLAG;
}

u_int
fpudispatch(u_int ir, u_int excp_code, u_int holder, u_int fpregs[])
{
	u_int class, subop;
	u_int fpu_type_flags;

	
	VASSERT(sizeof(int) == 4);

	parisc_linux_get_fpu_type(fpregs);

	fpu_type_flags=fpregs[FPU_TYPE_FLAG_POS];  

	class = get_class(ir);
	if (class == 1) {
		if  (fpu_type_flags & PA2_0_FPU_FLAG)
			subop = get_subop1_PA2_0(ir);
		else
			subop = get_subop1_PA1_1(ir);
	}
	else
		subop = get_subop(ir);

	if (FPUDEBUG) printk("class %d subop %d\n", class, subop);

	switch (excp_code) {
		case MAJOR_0C_EXCP:
		case PA83_UNIMP_EXCP:
			return(decode_0c(ir,class,subop,fpregs));
		case MAJOR_0E_EXCP:
			return(decode_0e(ir,class,subop,fpregs));
		case MAJOR_06_EXCP:
			return(decode_06(ir,fpregs));
		case MAJOR_26_EXCP:
			return(decode_26(ir,fpregs));
		case MAJOR_2E_EXCP:
			return(decode_2e(ir,fpregs));
		default:
			return(UNIMPLEMENTEDEXCEPTION);
	}
}

u_int
emfpudispatch(u_int ir, u_int dummy1, u_int dummy2, u_int fpregs[])
{
	u_int class, subop, major;
	u_int fpu_type_flags;

	
	VASSERT(sizeof(int) == 4);

	fpu_type_flags=fpregs[FPU_TYPE_FLAG_POS];  

	major = get_major(ir);
	class = get_class(ir);
	if (class == 1) {
		if  (fpu_type_flags & PA2_0_FPU_FLAG)
			subop = get_subop1_PA2_0(ir);
		else
			subop = get_subop1_PA1_1(ir);
	}
	else
		subop = get_subop(ir);
	switch (major) {
		case 0x0C:
			return(decode_0c(ir,class,subop,fpregs));
		case 0x0E:
			return(decode_0e(ir,class,subop,fpregs));
		case 0x06:
			return(decode_06(ir,fpregs));
		case 0x26:
			return(decode_26(ir,fpregs));
		case 0x2E:
			return(decode_2e(ir,fpregs));
		default:
			return(PA83_UNIMP_EXCP);
	}
}
	

static u_int
decode_0c(u_int ir, u_int class, u_int subop, u_int fpregs[])
{
	u_int r1,r2,t;		 
	u_int fmt;		
	u_int  df;		
	u_int *status;
	u_int retval, local_status;
	u_int fpu_type_flags;

	if (ir == COPR_INST) {
		fpregs[0] = EMULATION_VERSION << 11;
		return(NOEXCEPTION);
	}
	status = &fpregs[0];	
	local_status = fpregs[0]; 
	r1 = extru(ir,fpr1pos,5) * sizeof(double)/sizeof(u_int);
	if (r1 == 0)		
		r1 = fpzeroreg;
	t = extru(ir,fptpos,5) * sizeof(double)/sizeof(u_int);
	if (t == 0 && class != 2)	
		return(MAJOR_0C_EXCP);
	fmt = extru(ir,fpfmtpos,2);	

	switch (class) {
	    case 0:
		switch (subop) {
			case 0:	
			case 1:
				return(MAJOR_0C_EXCP);
			case 2:	
				switch (fmt) {
				    case 2: 
					return(MAJOR_0C_EXCP);
				    case 3: 
					t &= ~3;  
					r1 &= ~3;
					fpregs[t+3] = fpregs[r1+3];
					fpregs[t+2] = fpregs[r1+2];
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					fpregs[t] = fpregs[r1];
					return(NOEXCEPTION);
				}
			case 3: 
				switch (fmt) {
				    case 2: 
					return(MAJOR_0C_EXCP);
				    case 3: 
					t &= ~3;  
					r1 &= ~3;
					fpregs[t+3] = fpregs[r1+3];
					fpregs[t+2] = fpregs[r1+2];
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					
					fpregs[t] = fpregs[r1] & 0x7fffffff;
					return(NOEXCEPTION);
				}
			case 6: 
				switch (fmt) {
				    case 2: 
					return(MAJOR_0C_EXCP);
				    case 3: 
					t &= ~3;  
					r1 &= ~3;
					fpregs[t+3] = fpregs[r1+3];
					fpregs[t+2] = fpregs[r1+2];
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					
					fpregs[t] = fpregs[r1] ^ 0x80000000;
					return(NOEXCEPTION);
				}
			case 7: 
				switch (fmt) {
				    case 2: 
					return(MAJOR_0C_EXCP);
				    case 3: 
					t &= ~3;  
					r1 &= ~3;
					fpregs[t+3] = fpregs[r1+3];
					fpregs[t+2] = fpregs[r1+2];
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					
					fpregs[t] = fpregs[r1] | 0x80000000;
					return(NOEXCEPTION);
				}
			case 4: 
				switch (fmt) {
				    case 0:
					return(sgl_fsqrt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1:
					return(dbl_fsqrt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2:
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 5: 
				switch (fmt) {
				    case 0:
					return(sgl_frnd(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1:
					return(dbl_frnd(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2:
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
		} 

	case 1: 
		df = extru(ir,fpdfpos,2); 
		if ((df & 2) || (fmt & 2)) {
			return(MAJOR_0C_EXCP);
		}
		fmt = (fmt << 1) | df;
		switch (subop) {
			case 0: 
				switch(fmt) {
				    case 0: 
					return(MAJOR_0C_EXCP);
				    case 1: 
					return(sgl_to_dbl_fcnvff(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvff(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 1: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 2: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 3: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 5: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 6: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 7: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 4: 
				return(MAJOR_0C_EXCP);
		} 

	case 2: 
		fpu_type_flags=fpregs[FPU_TYPE_FLAG_POS];
		r2 = extru(ir, fpr2pos, 5) * sizeof(double)/sizeof(u_int);
		if (r2 == 0)
			r2 = fpzeroreg;
		if  (fpu_type_flags & PA2_0_FPU_FLAG) {
			
			if (extru(ir, fpnulpos, 1)) {  
				switch (fmt) {
				    case 0:
					BUG();
				    case 1:
				    case 2:
				    case 3:
					return(MAJOR_0C_EXCP);
				}
			} else {  
				switch (fmt) {
				    case 0:
					retval = sgl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				    case 1:
					retval = dbl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			}
		}  
		else {	
		    switch (subop) {
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				return(MAJOR_0C_EXCP);
			case 0: 
				switch (fmt) {
				    case 0:
					retval = sgl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				    case 1:
					retval = dbl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 1: 
				switch (fmt) {
				    case 0:
					BUG();
				    case 1:
				    case 2:
				    case 3:
					return(MAJOR_0C_EXCP);
				}
		    } 
		} 
	case 3: 
		r2 = extru(ir,fpr2pos,5) * sizeof(double)/sizeof(u_int);
		if (r2 == 0)
			r2 = fpzeroreg;
		switch (subop) {
			case 5:
			case 6:
			case 7:
				return(MAJOR_0C_EXCP);
			
			case 0: 
				switch (fmt) {
				    case 0:
					return(sgl_fadd(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fadd(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 1: 
				switch (fmt) {
				    case 0:
					return(sgl_fsub(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fsub(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 2: 
				switch (fmt) {
				    case 0:
					return(sgl_fmpy(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fmpy(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 3: 
				switch (fmt) {
				    case 0:
					return(sgl_fdiv(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fdiv(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
			case 4: 
				switch (fmt) {
				    case 0:
					return(sgl_frem(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_frem(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 2: 
				    case 3: 
					return(MAJOR_0C_EXCP);
				}
		} 
	} 

	
	return(MAJOR_0C_EXCP);
}

static u_int
decode_0e(ir,class,subop,fpregs)
u_int ir,class,subop;
u_int fpregs[];
{
	u_int r1,r2,t;		
	u_int fmt;		
	u_int df;		
	u_int *status;
	u_int retval, local_status;
	u_int fpu_type_flags;

	status = &fpregs[0];
	local_status = fpregs[0];
	r1 = ((extru(ir,fpr1pos,5)<<1)|(extru(ir,fpxr1pos,1)));
	if (r1 == 0)
		r1 = fpzeroreg;
	t = ((extru(ir,fptpos,5)<<1)|(extru(ir,fpxtpos,1)));
	if (t == 0 && class != 2)
		return(MAJOR_0E_EXCP);
	if (class < 2)		
		fmt = extru(ir,fpfmtpos,2);
	else 			
		fmt = extru(ir,fp0efmtpos,1);
	if (fmt == DBL) {
		r1 &= ~1;
		if (class != 1)
			t &= ~1;
	}

	switch (class) {
	    case 0:
		switch (subop) {
			case 0: 
			case 1:
				return(MAJOR_0E_EXCP);
			case 2: 
				switch (fmt) {
				    case 2:
				    case 3:
					return(MAJOR_0E_EXCP);
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					fpregs[t] = fpregs[r1];
					return(NOEXCEPTION);
				}
			case 3: 
				switch (fmt) {
				    case 2:
				    case 3:
					return(MAJOR_0E_EXCP);
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					fpregs[t] = fpregs[r1] & 0x7fffffff;
					return(NOEXCEPTION);
				}
			case 6: 
				switch (fmt) {
				    case 2:
				    case 3:
					return(MAJOR_0E_EXCP);
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					fpregs[t] = fpregs[r1] ^ 0x80000000;
					return(NOEXCEPTION);
				}
			case 7: 
				switch (fmt) {
				    case 2:
				    case 3:
					return(MAJOR_0E_EXCP);
				    case 1: 
					fpregs[t+1] = fpregs[r1+1];
				    case 0: 
					fpregs[t] = fpregs[r1] | 0x80000000;
					return(NOEXCEPTION);
				}
			case 4: 
				switch (fmt) {
				    case 0:
					return(sgl_fsqrt(&fpregs[r1],0,
						&fpregs[t], status));
				    case 1:
					return(dbl_fsqrt(&fpregs[r1],0,
						&fpregs[t], status));
				    case 2:
				    case 3:
					return(MAJOR_0E_EXCP);
				}
			case 5: 
				switch (fmt) {
				    case 0:
					return(sgl_frnd(&fpregs[r1],0,
						&fpregs[t], status));
				    case 1:
					return(dbl_frnd(&fpregs[r1],0,
						&fpregs[t], status));
				    case 2:
				    case 3:
					return(MAJOR_0E_EXCP);
				}
		} 
	
	case 1: 
		df = extru(ir,fpdfpos,2); 
		if (df == DBL) {
			t &= ~1;
		}
		if ((df & 2) || (fmt & 2))
			return(MAJOR_0E_EXCP);
		
		fmt = (fmt << 1) | df;
		switch (subop) {
			case 0: 
				switch(fmt) {
				    case 0: 
					return(MAJOR_0E_EXCP);
				    case 1: 
					return(sgl_to_dbl_fcnvff(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvff(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(MAJOR_0E_EXCP);
				}
			case 1: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvxf(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 2: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfx(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 3: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfxt(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 5: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvuf(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 6: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfu(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 7: 
				switch(fmt) {
				    case 0: 
					return(sgl_to_sgl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				    case 1: 
					return(sgl_to_dbl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				    case 2: 
					return(dbl_to_sgl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				    case 3: 
					return(dbl_to_dbl_fcnvfut(&fpregs[r1],0,
						&fpregs[t],status));
				}
			case 4: 
				return(MAJOR_0C_EXCP);
		} 
	case 2: 
		if (fmt == DBL)
			r2 = (extru(ir,fpr2pos,5)<<1);
		else
			r2 = ((extru(ir,fpr2pos,5)<<1)|(extru(ir,fpxr2pos,1)));
		fpu_type_flags=fpregs[FPU_TYPE_FLAG_POS];
		if (r2 == 0)
			r2 = fpzeroreg;
		if  (fpu_type_flags & PA2_0_FPU_FLAG) {
			
			if (extru(ir, fpnulpos, 1)) {  
				
				return(MAJOR_0E_EXCP);
			} else {  
			switch (fmt) {
				    case 0:
					retval = sgl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				    case 1:
					retval = dbl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				}
			}
		}  
		else {  
		    switch (subop) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				return(MAJOR_0E_EXCP);
			case 0: 
				switch (fmt) {
				    case 0:
					retval = sgl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				    case 1:
					retval = dbl_fcmp(&fpregs[r1],
						&fpregs[r2],extru(ir,fptpos,5),
						&local_status);
					update_status_cbit(status,local_status,
						fpu_type_flags, subop);
					return(retval);
				}
		    } 
		} 
	case 3: 
		if (fmt == DBL)
			r2 = (extru(ir,fpr2pos,5)<<1);
		else
			r2 = ((extru(ir,fpr2pos,5)<<1)|(extru(ir,fpxr2pos,1)));
		if (r2 == 0)
			r2 = fpzeroreg;
		switch (subop) {
			case 5:
			case 6:
			case 7:
				return(MAJOR_0E_EXCP);
			
			case 0: 
				switch (fmt) {
				    case 0:
					return(sgl_fadd(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fadd(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				}
			case 1: 
				switch (fmt) {
				    case 0:
					return(sgl_fsub(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fsub(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				}
			case 2: 
				if (extru(ir,fpxpos,1)) {
				    switch (fmt) {
					case 0:
					    if (t & 1)
						return(MAJOR_0E_EXCP);
					    BUG();
					    return(NOEXCEPTION);
					case 1:
						return(MAJOR_0E_EXCP);
				    }
				}
				else { 
				    switch (fmt) {
				        case 0:
					    return(sgl_fmpy(&fpregs[r1],
					       &fpregs[r2],&fpregs[t],status));
				        case 1:
					    return(dbl_fmpy(&fpregs[r1],
					       &fpregs[r2],&fpregs[t],status));
				    }
				}
			case 3: 
				switch (fmt) {
				    case 0:
					return(sgl_fdiv(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_fdiv(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				}
			case 4: 
				switch (fmt) {
				    case 0:
					return(sgl_frem(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				    case 1:
					return(dbl_frem(&fpregs[r1],&fpregs[r2],
						&fpregs[t],status));
				}
		} 
	} 

	
	return(MAJOR_0E_EXCP);
}


static u_int
decode_06(ir,fpregs)
u_int ir;
u_int fpregs[];
{
	u_int rm1, rm2, tm, ra, ta; 
	u_int fmt;
	u_int error = 0;
	u_int status;
	u_int fpu_type_flags;
	union {
		double dbl;
		float flt;
		struct { u_int i1; u_int i2; } ints;
	} mtmp, atmp;


	status = fpregs[0];		
	fpu_type_flags=fpregs[FPU_TYPE_FLAG_POS];  
	fmt = extru(ir, fpmultifmt, 1);	
	if (fmt == 0) { 
		rm1 = extru(ir, fprm1pos, 5) * sizeof(double)/sizeof(u_int);
		if (rm1 == 0)
			rm1 = fpzeroreg;
		rm2 = extru(ir, fprm2pos, 5) * sizeof(double)/sizeof(u_int);
		if (rm2 == 0)
			rm2 = fpzeroreg;
		tm = extru(ir, fptmpos, 5) * sizeof(double)/sizeof(u_int);
		if (tm == 0)
			return(MAJOR_06_EXCP);
		ra = extru(ir, fprapos, 5) * sizeof(double)/sizeof(u_int);
		ta = extru(ir, fptapos, 5) * sizeof(double)/sizeof(u_int);
		if (ta == 0)
			return(MAJOR_06_EXCP);

		if  (fpu_type_flags & TIMEX_ROLEX_FPU_MASK)  {

			if (ra == 0) {
			 	
				if (dbl_fmpy(&fpregs[rm1],&fpregs[rm2],
					&mtmp.ints.i1,&status))
					error = 1;
				if (dbl_to_sgl_fcnvfxt(&fpregs[ta],
					&atmp.ints.i1,&atmp.ints.i1,&status))
					error = 1;
				}
			else {

			if (dbl_fmpy(&fpregs[rm1],&fpregs[rm2],&mtmp.ints.i1,
					&status))
				error = 1;
			if (dbl_fadd(&fpregs[ta], &fpregs[ra], &atmp.ints.i1,
					&status))
				error = 1;
				}
			}

		else

			{
			if (ra == 0)
				ra = fpzeroreg;

			if (dbl_fmpy(&fpregs[rm1],&fpregs[rm2],&mtmp.ints.i1,
					&status))
				error = 1;
			if (dbl_fadd(&fpregs[ta], &fpregs[ra], &atmp.ints.i1,
					&status))
				error = 1;

			}

		if (error)
			return(MAJOR_06_EXCP);
		else {
			
			fpregs[tm] = mtmp.ints.i1;
			fpregs[tm+1] = mtmp.ints.i2;
			fpregs[ta] = atmp.ints.i1;
			fpregs[ta+1] = atmp.ints.i2;
			fpregs[0] = status;
			return(NOEXCEPTION);
		}
	}
	else { 
		rm1 = (extru(ir,fprm1pos,4) | 0x10 ) << 1;	
		rm1 |= extru(ir,fprm1pos-4,1);	

		rm2 = (extru(ir,fprm2pos,4) | 0x10 ) << 1;	
		rm2 |= extru(ir,fprm2pos-4,1);	

		tm = (extru(ir,fptmpos,4) | 0x10 ) << 1;	
		tm |= extru(ir,fptmpos-4,1);	

		ra = (extru(ir,fprapos,4) | 0x10 ) << 1;	
		ra |= extru(ir,fprapos-4,1);	

		ta = (extru(ir,fptapos,4) | 0x10 ) << 1;	
		ta |= extru(ir,fptapos-4,1);	
		
		if (ra == 0x20 &&(fpu_type_flags & TIMEX_ROLEX_FPU_MASK)) {
			if (sgl_fmpy(&fpregs[rm1],&fpregs[rm2],&mtmp.ints.i1,
					&status))
				error = 1;
			if (sgl_to_sgl_fcnvfxt(&fpregs[ta],&atmp.ints.i1,
				&atmp.ints.i1,&status))
				error = 1;
		}
		else {
			if (sgl_fmpy(&fpregs[rm1],&fpregs[rm2],&mtmp.ints.i1,
					&status))
				error = 1;
			if (sgl_fadd(&fpregs[ta], &fpregs[ra], &atmp.ints.i1,
					&status))
				error = 1;
		}
		if (error)
			return(MAJOR_06_EXCP);
		else {
			
			fpregs[tm] = mtmp.ints.i1;
			fpregs[ta] = atmp.ints.i1;
			fpregs[0] = status;
			return(NOEXCEPTION);
		}
	}
}

static u_int
decode_26(ir,fpregs)
u_int ir;
u_int fpregs[];
{
	u_int rm1, rm2, tm, ra, ta; 
	u_int fmt;
	u_int error = 0;
	u_int status;
	union {
		double dbl;
		float flt;
		struct { u_int i1; u_int i2; } ints;
	} mtmp, atmp;


	status = fpregs[0];
	fmt = extru(ir, fpmultifmt, 1);	
	if (fmt == 0) { 
		rm1 = extru(ir, fprm1pos, 5) * sizeof(double)/sizeof(u_int);
		if (rm1 == 0)
			rm1 = fpzeroreg;
		rm2 = extru(ir, fprm2pos, 5) * sizeof(double)/sizeof(u_int);
		if (rm2 == 0)
			rm2 = fpzeroreg;
		tm = extru(ir, fptmpos, 5) * sizeof(double)/sizeof(u_int);
		if (tm == 0)
			return(MAJOR_26_EXCP);
		ra = extru(ir, fprapos, 5) * sizeof(double)/sizeof(u_int);
		if (ra == 0)
			return(MAJOR_26_EXCP);
		ta = extru(ir, fptapos, 5) * sizeof(double)/sizeof(u_int);
		if (ta == 0)
			return(MAJOR_26_EXCP);
		
		if (dbl_fmpy(&fpregs[rm1],&fpregs[rm2],&mtmp.ints.i1,&status))
			error = 1;
		if (dbl_fsub(&fpregs[ta], &fpregs[ra], &atmp.ints.i1,&status))
			error = 1;
		if (error)
			return(MAJOR_26_EXCP);
		else {
			
			fpregs[tm] = mtmp.ints.i1;
			fpregs[tm+1] = mtmp.ints.i2;
			fpregs[ta] = atmp.ints.i1;
			fpregs[ta+1] = atmp.ints.i2;
			fpregs[0] = status;
			return(NOEXCEPTION);
		}
	}
	else { 
		rm1 = (extru(ir,fprm1pos,4) | 0x10 ) << 1;	
		rm1 |= extru(ir,fprm1pos-4,1);	

		rm2 = (extru(ir,fprm2pos,4) | 0x10 ) << 1;	
		rm2 |= extru(ir,fprm2pos-4,1);	

		tm = (extru(ir,fptmpos,4) | 0x10 ) << 1;	
		tm |= extru(ir,fptmpos-4,1);	

		ra = (extru(ir,fprapos,4) | 0x10 ) << 1;	
		ra |= extru(ir,fprapos-4,1);	

		ta = (extru(ir,fptapos,4) | 0x10 ) << 1;	
		ta |= extru(ir,fptapos-4,1);	
		
		if (sgl_fmpy(&fpregs[rm1],&fpregs[rm2],&mtmp.ints.i1,&status))
			error = 1;
		if (sgl_fsub(&fpregs[ta], &fpregs[ra], &atmp.ints.i1,&status))
			error = 1;
		if (error)
			return(MAJOR_26_EXCP);
		else {
			
			fpregs[tm] = mtmp.ints.i1;
			fpregs[ta] = atmp.ints.i1;
			fpregs[0] = status;
			return(NOEXCEPTION);
		}
	}

}

static u_int
decode_2e(ir,fpregs)
u_int ir;
u_int fpregs[];
{
	u_int rm1, rm2, ra, t; 
	u_int fmt;

	fmt = extru(ir,fpfmtpos,1);	
	if (fmt == DBL) { 
		rm1 = extru(ir,fprm1pos,5) * sizeof(double)/sizeof(u_int);
		if (rm1 == 0)
			rm1 = fpzeroreg;
		rm2 = extru(ir,fprm2pos,5) * sizeof(double)/sizeof(u_int);
		if (rm2 == 0)
			rm2 = fpzeroreg;
		ra = ((extru(ir,fpraupos,3)<<2)|(extru(ir,fpralpos,3)>>1)) *
		     sizeof(double)/sizeof(u_int);
		if (ra == 0)
			ra = fpzeroreg;
		t = extru(ir,fptpos,5) * sizeof(double)/sizeof(u_int);
		if (t == 0)
			return(MAJOR_2E_EXCP);

		if (extru(ir,fpfusedsubop,1)) { 
			return(dbl_fmpynfadd(&fpregs[rm1], &fpregs[rm2],
					&fpregs[ra], &fpregs[0], &fpregs[t]));
		} else {
			return(dbl_fmpyfadd(&fpregs[rm1], &fpregs[rm2],
					&fpregs[ra], &fpregs[0], &fpregs[t]));
		}
	} 
	else { 
		rm1 = (extru(ir,fprm1pos,5)<<1)|(extru(ir,fpxrm1pos,1));
		if (rm1 == 0)
			rm1 = fpzeroreg;
		rm2 = (extru(ir,fprm2pos,5)<<1)|(extru(ir,fpxrm2pos,1));
		if (rm2 == 0)
			rm2 = fpzeroreg;
		ra = (extru(ir,fpraupos,3)<<3)|extru(ir,fpralpos,3);
		if (ra == 0)
			ra = fpzeroreg;
		t = ((extru(ir,fptpos,5)<<1)|(extru(ir,fpxtpos,1)));
		if (t == 0)
			return(MAJOR_2E_EXCP);

		if (extru(ir,fpfusedsubop,1)) { 
			return(sgl_fmpynfadd(&fpregs[rm1], &fpregs[rm2],
					&fpregs[ra], &fpregs[0], &fpregs[t]));
		} else {
			return(sgl_fmpyfadd(&fpregs[rm1], &fpregs[rm2],
					&fpregs[ra], &fpregs[0], &fpregs[t]));
		}
	} 
}

static void
update_status_cbit(status, new_status, fpu_type, y_field)
u_int *status, new_status;
u_int fpu_type;
u_int y_field;
{
	if ((fpu_type & TIMEX_EXTEN_FLAG) || 
	    (fpu_type & ROLEX_EXTEN_FLAG) ||
	    (fpu_type & PA2_0_FPU_FLAG)) {
		if (y_field == 0) {
			*status = ((*status & 0x04000000) >> 5) | 
				  ((*status & 0x003ff000) >> 1) | 
				  (new_status & 0xffc007ff); 
		} else {
			*status = (*status & 0x04000000) |     
				  ((new_status & 0x04000000) >> (y_field+4)) |
				  (new_status & ~0x04000000 &  
				   ~(0x04000000 >> (y_field+4)));
		}
	}
	
	else {
		*status = new_status;
	}
}
