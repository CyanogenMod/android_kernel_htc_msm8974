/*
 * Single-step support.
 *
 * Copyright (C) 2004 Paul Mackerras <paulus@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/prefetch.h>
#include <asm/sstep.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/cputable.h>

extern char system_call_common[];

#ifdef CONFIG_PPC64
#define MSR_MASK	0xffffffff87c0ffffUL
#else
#define MSR_MASK	0x87c0ffff
#endif

#define XER_SO		0x80000000U
#define XER_OV		0x40000000U
#define XER_CA		0x20000000U

#ifdef CONFIG_PPC_FPU
extern int do_lfs(int rn, unsigned long ea);
extern int do_lfd(int rn, unsigned long ea);
extern int do_stfs(int rn, unsigned long ea);
extern int do_stfd(int rn, unsigned long ea);
extern int do_lvx(int rn, unsigned long ea);
extern int do_stvx(int rn, unsigned long ea);
extern int do_lxvd2x(int rn, unsigned long ea);
extern int do_stxvd2x(int rn, unsigned long ea);
#endif

static unsigned long truncate_if_32bit(unsigned long msr, unsigned long val)
{
#ifdef __powerpc64__
	if ((msr & MSR_64BIT) == 0)
		val &= 0xffffffffUL;
#endif
	return val;
}

static int __kprobes branch_taken(unsigned int instr, struct pt_regs *regs)
{
	unsigned int bo = (instr >> 21) & 0x1f;
	unsigned int bi;

	if ((bo & 4) == 0) {
		
		--regs->ctr;
		if (((bo >> 1) & 1) ^ (regs->ctr == 0))
			return 0;
	}
	if ((bo & 0x10) == 0) {
		
		bi = (instr >> 16) & 0x1f;
		if (((regs->ccr >> (31 - bi)) & 1) != ((bo >> 3) & 1))
			return 0;
	}
	return 1;
}


static long __kprobes address_ok(struct pt_regs *regs, unsigned long ea, int nb)
{
	if (!user_mode(regs))
		return 1;
	return __access_ok(ea, nb, USER_DS);
}

static unsigned long __kprobes dform_ea(unsigned int instr, struct pt_regs *regs)
{
	int ra;
	unsigned long ea;

	ra = (instr >> 16) & 0x1f;
	ea = (signed short) instr;		
	if (ra) {
		ea += regs->gpr[ra];
		if (instr & 0x04000000)		
			regs->gpr[ra] = ea;
	}

	return truncate_if_32bit(regs->msr, ea);
}

#ifdef __powerpc64__
static unsigned long __kprobes dsform_ea(unsigned int instr, struct pt_regs *regs)
{
	int ra;
	unsigned long ea;

	ra = (instr >> 16) & 0x1f;
	ea = (signed short) (instr & ~3);	
	if (ra) {
		ea += regs->gpr[ra];
		if ((instr & 3) == 1)		
			regs->gpr[ra] = ea;
	}

	return truncate_if_32bit(regs->msr, ea);
}
#endif 

static unsigned long __kprobes xform_ea(unsigned int instr, struct pt_regs *regs,
				     int do_update)
{
	int ra, rb;
	unsigned long ea;

	ra = (instr >> 16) & 0x1f;
	rb = (instr >> 11) & 0x1f;
	ea = regs->gpr[rb];
	if (ra) {
		ea += regs->gpr[ra];
		if (do_update)		
			regs->gpr[ra] = ea;
	}

	return truncate_if_32bit(regs->msr, ea);
}

static inline unsigned long max_align(unsigned long x)
{
	x |= sizeof(unsigned long);
	return x & -x;		
}


static inline unsigned long byterev_2(unsigned long x)
{
	return ((x >> 8) & 0xff) | ((x & 0xff) << 8);
}

static inline unsigned long byterev_4(unsigned long x)
{
	return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) |
		((x & 0xff00) << 8) | ((x & 0xff) << 24);
}

#ifdef __powerpc64__
static inline unsigned long byterev_8(unsigned long x)
{
	return (byterev_4(x) << 32) | byterev_4(x >> 32);
}
#endif

static int __kprobes read_mem_aligned(unsigned long *dest, unsigned long ea,
				      int nb)
{
	int err = 0;
	unsigned long x = 0;

	switch (nb) {
	case 1:
		err = __get_user(x, (unsigned char __user *) ea);
		break;
	case 2:
		err = __get_user(x, (unsigned short __user *) ea);
		break;
	case 4:
		err = __get_user(x, (unsigned int __user *) ea);
		break;
#ifdef __powerpc64__
	case 8:
		err = __get_user(x, (unsigned long __user *) ea);
		break;
#endif
	}
	if (!err)
		*dest = x;
	return err;
}

static int __kprobes read_mem_unaligned(unsigned long *dest, unsigned long ea,
					int nb, struct pt_regs *regs)
{
	int err;
	unsigned long x, b, c;

	
	x = 0;
	for (; nb > 0; nb -= c) {
		c = max_align(ea);
		if (c > nb)
			c = max_align(nb);
		err = read_mem_aligned(&b, ea, c);
		if (err)
			return err;
		x = (x << (8 * c)) + b;
		ea += c;
	}
	*dest = x;
	return 0;
}

static int __kprobes read_mem(unsigned long *dest, unsigned long ea, int nb,
			      struct pt_regs *regs)
{
	if (!address_ok(regs, ea, nb))
		return -EFAULT;
	if ((ea & (nb - 1)) == 0)
		return read_mem_aligned(dest, ea, nb);
	return read_mem_unaligned(dest, ea, nb, regs);
}

static int __kprobes write_mem_aligned(unsigned long val, unsigned long ea,
				       int nb)
{
	int err = 0;

	switch (nb) {
	case 1:
		err = __put_user(val, (unsigned char __user *) ea);
		break;
	case 2:
		err = __put_user(val, (unsigned short __user *) ea);
		break;
	case 4:
		err = __put_user(val, (unsigned int __user *) ea);
		break;
#ifdef __powerpc64__
	case 8:
		err = __put_user(val, (unsigned long __user *) ea);
		break;
#endif
	}
	return err;
}

static int __kprobes write_mem_unaligned(unsigned long val, unsigned long ea,
					 int nb, struct pt_regs *regs)
{
	int err;
	unsigned long c;

	
	for (; nb > 0; nb -= c) {
		c = max_align(ea);
		if (c > nb)
			c = max_align(nb);
		err = write_mem_aligned(val >> (nb - c) * 8, ea, c);
		if (err)
			return err;
		++ea;
	}
	return 0;
}

static int __kprobes write_mem(unsigned long val, unsigned long ea, int nb,
			       struct pt_regs *regs)
{
	if (!address_ok(regs, ea, nb))
		return -EFAULT;
	if ((ea & (nb - 1)) == 0)
		return write_mem_aligned(val, ea, nb);
	return write_mem_unaligned(val, ea, nb, regs);
}

#ifdef CONFIG_PPC_FPU
static int __kprobes do_fp_load(int rn, int (*func)(int, unsigned long),
				unsigned long ea, int nb,
				struct pt_regs *regs)
{
	int err;
	unsigned long val[sizeof(double) / sizeof(long)];
	unsigned long ptr;

	if (!address_ok(regs, ea, nb))
		return -EFAULT;
	if ((ea & 3) == 0)
		return (*func)(rn, ea);
	ptr = (unsigned long) &val[0];
	if (sizeof(unsigned long) == 8 || nb == 4) {
		err = read_mem_unaligned(&val[0], ea, nb, regs);
		ptr += sizeof(unsigned long) - nb;
	} else {
		
		err = read_mem_unaligned(&val[0], ea, 4, regs);
		if (!err)
			err = read_mem_unaligned(&val[1], ea + 4, 4, regs);
	}
	if (err)
		return err;
	return (*func)(rn, ptr);
}

static int __kprobes do_fp_store(int rn, int (*func)(int, unsigned long),
				 unsigned long ea, int nb,
				 struct pt_regs *regs)
{
	int err;
	unsigned long val[sizeof(double) / sizeof(long)];
	unsigned long ptr;

	if (!address_ok(regs, ea, nb))
		return -EFAULT;
	if ((ea & 3) == 0)
		return (*func)(rn, ea);
	ptr = (unsigned long) &val[0];
	if (sizeof(unsigned long) == 8 || nb == 4) {
		ptr += sizeof(unsigned long) - nb;
		err = (*func)(rn, ptr);
		if (err)
			return err;
		err = write_mem_unaligned(val[0], ea, nb, regs);
	} else {
		
		err = (*func)(rn, ptr);
		if (err)
			return err;
		err = write_mem_unaligned(val[0], ea, 4, regs);
		if (!err)
			err = write_mem_unaligned(val[1], ea + 4, 4, regs);
	}
	return err;
}
#endif

#ifdef CONFIG_ALTIVEC
static int __kprobes do_vec_load(int rn, int (*func)(int, unsigned long),
				 unsigned long ea, struct pt_regs *regs)
{
	if (!address_ok(regs, ea & ~0xfUL, 16))
		return -EFAULT;
	return (*func)(rn, ea);
}

static int __kprobes do_vec_store(int rn, int (*func)(int, unsigned long),
				  unsigned long ea, struct pt_regs *regs)
{
	if (!address_ok(regs, ea & ~0xfUL, 16))
		return -EFAULT;
	return (*func)(rn, ea);
}
#endif 

#ifdef CONFIG_VSX
static int __kprobes do_vsx_load(int rn, int (*func)(int, unsigned long),
				 unsigned long ea, struct pt_regs *regs)
{
	int err;
	unsigned long val[2];

	if (!address_ok(regs, ea, 16))
		return -EFAULT;
	if ((ea & 3) == 0)
		return (*func)(rn, ea);
	err = read_mem_unaligned(&val[0], ea, 8, regs);
	if (!err)
		err = read_mem_unaligned(&val[1], ea + 8, 8, regs);
	if (!err)
		err = (*func)(rn, (unsigned long) &val[0]);
	return err;
}

static int __kprobes do_vsx_store(int rn, int (*func)(int, unsigned long),
				 unsigned long ea, struct pt_regs *regs)
{
	int err;
	unsigned long val[2];

	if (!address_ok(regs, ea, 16))
		return -EFAULT;
	if ((ea & 3) == 0)
		return (*func)(rn, ea);
	err = (*func)(rn, (unsigned long) &val[0]);
	if (err)
		return err;
	err = write_mem_unaligned(val[0], ea, 8, regs);
	if (!err)
		err = write_mem_unaligned(val[1], ea + 8, 8, regs);
	return err;
}
#endif 

#define __put_user_asmx(x, addr, err, op, cr)		\
	__asm__ __volatile__(				\
		"1:	" op " %2,0,%3\n"		\
		"	mfcr	%1\n"			\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	li	%0,%4\n"		\
		"	b	2b\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
			PPC_LONG_ALIGN "\n"		\
			PPC_LONG "1b,3b\n"		\
		".previous"				\
		: "=r" (err), "=r" (cr)			\
		: "r" (x), "r" (addr), "i" (-EFAULT), "0" (err))

#define __get_user_asmx(x, addr, err, op)		\
	__asm__ __volatile__(				\
		"1:	"op" %1,0,%2\n"			\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	li	%0,%3\n"		\
		"	b	2b\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
			PPC_LONG_ALIGN "\n"		\
			PPC_LONG "1b,3b\n"		\
		".previous"				\
		: "=r" (err), "=r" (x)			\
		: "r" (addr), "i" (-EFAULT), "0" (err))

#define __cacheop_user_asmx(addr, err, op)		\
	__asm__ __volatile__(				\
		"1:	"op" 0,%1\n"			\
		"2:\n"					\
		".section .fixup,\"ax\"\n"		\
		"3:	li	%0,%3\n"		\
		"	b	2b\n"			\
		".previous\n"				\
		".section __ex_table,\"a\"\n"		\
			PPC_LONG_ALIGN "\n"		\
			PPC_LONG "1b,3b\n"		\
		".previous"				\
		: "=r" (err)				\
		: "r" (addr), "i" (-EFAULT), "0" (err))

static void __kprobes set_cr0(struct pt_regs *regs, int rd)
{
	long val = regs->gpr[rd];

	regs->ccr = (regs->ccr & 0x0fffffff) | ((regs->xer >> 3) & 0x10000000);
#ifdef __powerpc64__
	if (!(regs->msr & MSR_64BIT))
		val = (int) val;
#endif
	if (val < 0)
		regs->ccr |= 0x80000000;
	else if (val > 0)
		regs->ccr |= 0x40000000;
	else
		regs->ccr |= 0x20000000;
}

static void __kprobes add_with_carry(struct pt_regs *regs, int rd,
				     unsigned long val1, unsigned long val2,
				     unsigned long carry_in)
{
	unsigned long val = val1 + val2;

	if (carry_in)
		++val;
	regs->gpr[rd] = val;
#ifdef __powerpc64__
	if (!(regs->msr & MSR_64BIT)) {
		val = (unsigned int) val;
		val1 = (unsigned int) val1;
	}
#endif
	if (val < val1 || (carry_in && val == val1))
		regs->xer |= XER_CA;
	else
		regs->xer &= ~XER_CA;
}

static void __kprobes do_cmp_signed(struct pt_regs *regs, long v1, long v2,
				    int crfld)
{
	unsigned int crval, shift;

	crval = (regs->xer >> 31) & 1;		
	if (v1 < v2)
		crval |= 8;
	else if (v1 > v2)
		crval |= 4;
	else
		crval |= 2;
	shift = (7 - crfld) * 4;
	regs->ccr = (regs->ccr & ~(0xf << shift)) | (crval << shift);
}

static void __kprobes do_cmp_unsigned(struct pt_regs *regs, unsigned long v1,
				      unsigned long v2, int crfld)
{
	unsigned int crval, shift;

	crval = (regs->xer >> 31) & 1;		
	if (v1 < v2)
		crval |= 8;
	else if (v1 > v2)
		crval |= 4;
	else
		crval |= 2;
	shift = (7 - crfld) * 4;
	regs->ccr = (regs->ccr & ~(0xf << shift)) | (crval << shift);
}

#define MASK32(mb, me)	((0xffffffffUL >> (mb)) + \
			 ((signed long)-0x80000000L >> (me)) + ((me) >= (mb)))
#ifdef __powerpc64__
#define MASK64_L(mb)	(~0UL >> (mb))
#define MASK64_R(me)	((signed long)-0x8000000000000000L >> (me))
#define MASK64(mb, me)	(MASK64_L(mb) + MASK64_R(me) + ((me) >= (mb)))
#define DATA32(x)	(((x) & 0xffffffffUL) | (((x) & 0xffffffffUL) << 32))
#else
#define DATA32(x)	(x)
#endif
#define ROTATE(x, n)	((n) ? (((x) << (n)) | ((x) >> (8 * sizeof(long) - (n)))) : (x))

int __kprobes emulate_step(struct pt_regs *regs, unsigned int instr)
{
	unsigned int opcode, ra, rb, rd, spr, u;
	unsigned long int imm;
	unsigned long int val, val2;
	unsigned long int ea;
	unsigned int cr, mb, me, sh;
	int err;
	unsigned long old_ra;
	long ival;

	opcode = instr >> 26;
	switch (opcode) {
	case 16:	
		imm = (signed short)(instr & 0xfffc);
		if ((instr & 2) == 0)
			imm += regs->nip;
		regs->nip += 4;
		regs->nip = truncate_if_32bit(regs->msr, regs->nip);
		if (instr & 1)
			regs->link = regs->nip;
		if (branch_taken(instr, regs))
			regs->nip = imm;
		return 1;
#ifdef CONFIG_PPC64
	case 17:	
		if (regs->gpr[0] == 0x1ebe &&
		    cpu_has_feature(CPU_FTR_REAL_LE)) {
			regs->msr ^= MSR_LE;
			goto instr_done;
		}
		regs->gpr[9] = regs->gpr[13];
		regs->gpr[10] = MSR_KERNEL;
		regs->gpr[11] = regs->nip + 4;
		regs->gpr[12] = regs->msr & MSR_MASK;
		regs->gpr[13] = (unsigned long) get_paca();
		regs->nip = (unsigned long) &system_call_common;
		regs->msr = MSR_KERNEL;
		return 1;
#endif
	case 18:	
		imm = instr & 0x03fffffc;
		if (imm & 0x02000000)
			imm -= 0x04000000;
		if ((instr & 2) == 0)
			imm += regs->nip;
		if (instr & 1)
			regs->link = truncate_if_32bit(regs->msr, regs->nip + 4);
		imm = truncate_if_32bit(regs->msr, imm);
		regs->nip = imm;
		return 1;
	case 19:
		switch ((instr >> 1) & 0x3ff) {
		case 16:	
		case 528:	
			imm = (instr & 0x400)? regs->ctr: regs->link;
			regs->nip = truncate_if_32bit(regs->msr, regs->nip + 4);
			imm = truncate_if_32bit(regs->msr, imm);
			if (instr & 1)
				regs->link = regs->nip;
			if (branch_taken(instr, regs))
				regs->nip = imm;
			return 1;

		case 18:	
			return -1;

		case 150:	
			isync();
			goto instr_done;

		case 33:	
		case 129:	
		case 193:	
		case 225:	
		case 257:	
		case 289:	
		case 417:	
		case 449:	
			ra = (instr >> 16) & 0x1f;
			rb = (instr >> 11) & 0x1f;
			rd = (instr >> 21) & 0x1f;
			ra = (regs->ccr >> (31 - ra)) & 1;
			rb = (regs->ccr >> (31 - rb)) & 1;
			val = (instr >> (6 + ra * 2 + rb)) & 1;
			regs->ccr = (regs->ccr & ~(1UL << (31 - rd))) |
				(val << (31 - rd));
			goto instr_done;
		}
		break;
	case 31:
		switch ((instr >> 1) & 0x3ff) {
		case 598:	
#ifdef __powerpc64__
			switch ((instr >> 21) & 3) {
			case 1:		
				asm volatile("lwsync" : : : "memory");
				goto instr_done;
			case 2:		
				asm volatile("ptesync" : : : "memory");
				goto instr_done;
			}
#endif
			mb();
			goto instr_done;

		case 854:	
			eieio();
			goto instr_done;
		}
		break;
	}

	
	if (!FULL_REGS(regs))
		return 0;

	rd = (instr >> 21) & 0x1f;
	ra = (instr >> 16) & 0x1f;
	rb = (instr >> 11) & 0x1f;

	switch (opcode) {
	case 7:		
		regs->gpr[rd] = regs->gpr[ra] * (short) instr;
		goto instr_done;

	case 8:		
		imm = (short) instr;
		add_with_carry(regs, rd, ~regs->gpr[ra], imm, 1);
		goto instr_done;

	case 10:	
		imm = (unsigned short) instr;
		val = regs->gpr[ra];
#ifdef __powerpc64__
		if ((rd & 1) == 0)
			val = (unsigned int) val;
#endif
		do_cmp_unsigned(regs, val, imm, rd >> 2);
		goto instr_done;

	case 11:	
		imm = (short) instr;
		val = regs->gpr[ra];
#ifdef __powerpc64__
		if ((rd & 1) == 0)
			val = (int) val;
#endif
		do_cmp_signed(regs, val, imm, rd >> 2);
		goto instr_done;

	case 12:	
		imm = (short) instr;
		add_with_carry(regs, rd, regs->gpr[ra], imm, 0);
		goto instr_done;

	case 13:	
		imm = (short) instr;
		add_with_carry(regs, rd, regs->gpr[ra], imm, 0);
		set_cr0(regs, rd);
		goto instr_done;

	case 14:	
		imm = (short) instr;
		if (ra)
			imm += regs->gpr[ra];
		regs->gpr[rd] = imm;
		goto instr_done;

	case 15:	
		imm = ((short) instr) << 16;
		if (ra)
			imm += regs->gpr[ra];
		regs->gpr[rd] = imm;
		goto instr_done;

	case 20:	
		mb = (instr >> 6) & 0x1f;
		me = (instr >> 1) & 0x1f;
		val = DATA32(regs->gpr[rd]);
		imm = MASK32(mb, me);
		regs->gpr[ra] = (regs->gpr[ra] & ~imm) | (ROTATE(val, rb) & imm);
		goto logical_done;

	case 21:	
		mb = (instr >> 6) & 0x1f;
		me = (instr >> 1) & 0x1f;
		val = DATA32(regs->gpr[rd]);
		regs->gpr[ra] = ROTATE(val, rb) & MASK32(mb, me);
		goto logical_done;

	case 23:	
		mb = (instr >> 6) & 0x1f;
		me = (instr >> 1) & 0x1f;
		rb = regs->gpr[rb] & 0x1f;
		val = DATA32(regs->gpr[rd]);
		regs->gpr[ra] = ROTATE(val, rb) & MASK32(mb, me);
		goto logical_done;

	case 24:	
		imm = (unsigned short) instr;
		regs->gpr[ra] = regs->gpr[rd] | imm;
		goto instr_done;

	case 25:	
		imm = (unsigned short) instr;
		regs->gpr[ra] = regs->gpr[rd] | (imm << 16);
		goto instr_done;

	case 26:	
		imm = (unsigned short) instr;
		regs->gpr[ra] = regs->gpr[rd] ^ imm;
		goto instr_done;

	case 27:	
		imm = (unsigned short) instr;
		regs->gpr[ra] = regs->gpr[rd] ^ (imm << 16);
		goto instr_done;

	case 28:	
		imm = (unsigned short) instr;
		regs->gpr[ra] = regs->gpr[rd] & imm;
		set_cr0(regs, ra);
		goto instr_done;

	case 29:	
		imm = (unsigned short) instr;
		regs->gpr[ra] = regs->gpr[rd] & (imm << 16);
		set_cr0(regs, ra);
		goto instr_done;

#ifdef __powerpc64__
	case 30:	
		mb = ((instr >> 6) & 0x1f) | (instr & 0x20);
		val = regs->gpr[rd];
		if ((instr & 0x10) == 0) {
			sh = rb | ((instr & 2) << 4);
			val = ROTATE(val, sh);
			switch ((instr >> 2) & 3) {
			case 0:		
				regs->gpr[ra] = val & MASK64_L(mb);
				goto logical_done;
			case 1:		
				regs->gpr[ra] = val & MASK64_R(mb);
				goto logical_done;
			case 2:		
				regs->gpr[ra] = val & MASK64(mb, 63 - sh);
				goto logical_done;
			case 3:		
				imm = MASK64(mb, 63 - sh);
				regs->gpr[ra] = (regs->gpr[ra] & ~imm) |
					(val & imm);
				goto logical_done;
			}
		} else {
			sh = regs->gpr[rb] & 0x3f;
			val = ROTATE(val, sh);
			switch ((instr >> 1) & 7) {
			case 0:		
				regs->gpr[ra] = val & MASK64_L(mb);
				goto logical_done;
			case 1:		
				regs->gpr[ra] = val & MASK64_R(mb);
				goto logical_done;
			}
		}
#endif

	case 31:
		switch ((instr >> 1) & 0x3ff) {
		case 83:	
			if (regs->msr & MSR_PR)
				break;
			regs->gpr[rd] = regs->msr & MSR_MASK;
			goto instr_done;
		case 146:	
			if (regs->msr & MSR_PR)
				break;
			imm = regs->gpr[rd];
			if ((imm & MSR_RI) == 0)
				
				return -1;
			regs->msr = imm;
			goto instr_done;
#ifdef CONFIG_PPC64
		case 178:	
			
			
			if (regs->msr & MSR_PR)
				break;
			imm = (instr & 0x10000)? 0x8002: 0xefffffffffffefffUL;
			imm = (regs->msr & MSR_MASK & ~imm)
				| (regs->gpr[rd] & imm);
			if ((imm & MSR_RI) == 0)
				
				return -1;
			regs->msr = imm;
			goto instr_done;
#endif
		case 19:	
			regs->gpr[rd] = regs->ccr;
			regs->gpr[rd] &= 0xffffffffUL;
			goto instr_done;

		case 144:	
			imm = 0xf0000000UL;
			val = regs->gpr[rd];
			for (sh = 0; sh < 8; ++sh) {
				if (instr & (0x80000 >> sh))
					regs->ccr = (regs->ccr & ~imm) |
						(val & imm);
				imm >>= 4;
			}
			goto instr_done;

		case 339:	
			spr = (instr >> 11) & 0x3ff;
			switch (spr) {
			case 0x20:	
				regs->gpr[rd] = regs->xer;
				regs->gpr[rd] &= 0xffffffffUL;
				goto instr_done;
			case 0x100:	
				regs->gpr[rd] = regs->link;
				goto instr_done;
			case 0x120:	
				regs->gpr[rd] = regs->ctr;
				goto instr_done;
			}
			break;

		case 467:	
			spr = (instr >> 11) & 0x3ff;
			switch (spr) {
			case 0x20:	
				regs->xer = (regs->gpr[rd] & 0xffffffffUL);
				goto instr_done;
			case 0x100:	
				regs->link = regs->gpr[rd];
				goto instr_done;
			case 0x120:	
				regs->ctr = regs->gpr[rd];
				goto instr_done;
			}
			break;

		case 0:	
			val = regs->gpr[ra];
			val2 = regs->gpr[rb];
#ifdef __powerpc64__
			if ((rd & 1) == 0) {
				
				val = (int) val;
				val2 = (int) val2;
			}
#endif
			do_cmp_signed(regs, val, val2, rd >> 2);
			goto instr_done;

		case 32:	
			val = regs->gpr[ra];
			val2 = regs->gpr[rb];
#ifdef __powerpc64__
			if ((rd & 1) == 0) {
				
				val = (unsigned int) val;
				val2 = (unsigned int) val2;
			}
#endif
			do_cmp_unsigned(regs, val, val2, rd >> 2);
			goto instr_done;

		case 8:	
			add_with_carry(regs, rd, ~regs->gpr[ra],
				       regs->gpr[rb], 1);
			goto arith_done;
#ifdef __powerpc64__
		case 9:	
			asm("mulhdu %0,%1,%2" : "=r" (regs->gpr[rd]) :
			    "r" (regs->gpr[ra]), "r" (regs->gpr[rb]));
			goto arith_done;
#endif
		case 10:	
			add_with_carry(regs, rd, regs->gpr[ra],
				       regs->gpr[rb], 0);
			goto arith_done;

		case 11:	
			asm("mulhwu %0,%1,%2" : "=r" (regs->gpr[rd]) :
			    "r" (regs->gpr[ra]), "r" (regs->gpr[rb]));
			goto arith_done;

		case 40:	
			regs->gpr[rd] = regs->gpr[rb] - regs->gpr[ra];
			goto arith_done;
#ifdef __powerpc64__
		case 73:	
			asm("mulhd %0,%1,%2" : "=r" (regs->gpr[rd]) :
			    "r" (regs->gpr[ra]), "r" (regs->gpr[rb]));
			goto arith_done;
#endif
		case 75:	
			asm("mulhw %0,%1,%2" : "=r" (regs->gpr[rd]) :
			    "r" (regs->gpr[ra]), "r" (regs->gpr[rb]));
			goto arith_done;

		case 104:	
			regs->gpr[rd] = -regs->gpr[ra];
			goto arith_done;

		case 136:	
			add_with_carry(regs, rd, ~regs->gpr[ra], regs->gpr[rb],
				       regs->xer & XER_CA);
			goto arith_done;

		case 138:	
			add_with_carry(regs, rd, regs->gpr[ra], regs->gpr[rb],
				       regs->xer & XER_CA);
			goto arith_done;

		case 200:	
			add_with_carry(regs, rd, ~regs->gpr[ra], 0L,
				       regs->xer & XER_CA);
			goto arith_done;

		case 202:	
			add_with_carry(regs, rd, regs->gpr[ra], 0L,
				       regs->xer & XER_CA);
			goto arith_done;

		case 232:	
			add_with_carry(regs, rd, ~regs->gpr[ra], -1L,
				       regs->xer & XER_CA);
			goto arith_done;
#ifdef __powerpc64__
		case 233:	
			regs->gpr[rd] = regs->gpr[ra] * regs->gpr[rb];
			goto arith_done;
#endif
		case 234:	
			add_with_carry(regs, rd, regs->gpr[ra], -1L,
				       regs->xer & XER_CA);
			goto arith_done;

		case 235:	
			regs->gpr[rd] = (unsigned int) regs->gpr[ra] *
				(unsigned int) regs->gpr[rb];
			goto arith_done;

		case 266:	
			regs->gpr[rd] = regs->gpr[ra] + regs->gpr[rb];
			goto arith_done;
#ifdef __powerpc64__
		case 457:	
			regs->gpr[rd] = regs->gpr[ra] / regs->gpr[rb];
			goto arith_done;
#endif
		case 459:	
			regs->gpr[rd] = (unsigned int) regs->gpr[ra] /
				(unsigned int) regs->gpr[rb];
			goto arith_done;
#ifdef __powerpc64__
		case 489:	
			regs->gpr[rd] = (long int) regs->gpr[ra] /
				(long int) regs->gpr[rb];
			goto arith_done;
#endif
		case 491:	
			regs->gpr[rd] = (int) regs->gpr[ra] /
				(int) regs->gpr[rb];
			goto arith_done;


		case 26:	
			asm("cntlzw %0,%1" : "=r" (regs->gpr[ra]) :
			    "r" (regs->gpr[rd]));
			goto logical_done;
#ifdef __powerpc64__
		case 58:	
			asm("cntlzd %0,%1" : "=r" (regs->gpr[ra]) :
			    "r" (regs->gpr[rd]));
			goto logical_done;
#endif
		case 28:	
			regs->gpr[ra] = regs->gpr[rd] & regs->gpr[rb];
			goto logical_done;

		case 60:	
			regs->gpr[ra] = regs->gpr[rd] & ~regs->gpr[rb];
			goto logical_done;

		case 124:	
			regs->gpr[ra] = ~(regs->gpr[rd] | regs->gpr[rb]);
			goto logical_done;

		case 284:	
			regs->gpr[ra] = ~(regs->gpr[rd] ^ regs->gpr[rb]);
			goto logical_done;

		case 316:	
			regs->gpr[ra] = regs->gpr[rd] ^ regs->gpr[rb];
			goto logical_done;

		case 412:	
			regs->gpr[ra] = regs->gpr[rd] | ~regs->gpr[rb];
			goto logical_done;

		case 444:	
			regs->gpr[ra] = regs->gpr[rd] | regs->gpr[rb];
			goto logical_done;

		case 476:	
			regs->gpr[ra] = ~(regs->gpr[rd] & regs->gpr[rb]);
			goto logical_done;

		case 922:	
			regs->gpr[ra] = (signed short) regs->gpr[rd];
			goto logical_done;

		case 954:	
			regs->gpr[ra] = (signed char) regs->gpr[rd];
			goto logical_done;
#ifdef __powerpc64__
		case 986:	
			regs->gpr[ra] = (signed int) regs->gpr[rd];
			goto logical_done;
#endif

		case 24:	
			sh = regs->gpr[rb] & 0x3f;
			if (sh < 32)
				regs->gpr[ra] = (regs->gpr[rd] << sh) & 0xffffffffUL;
			else
				regs->gpr[ra] = 0;
			goto logical_done;

		case 536:	
			sh = regs->gpr[rb] & 0x3f;
			if (sh < 32)
				regs->gpr[ra] = (regs->gpr[rd] & 0xffffffffUL) >> sh;
			else
				regs->gpr[ra] = 0;
			goto logical_done;

		case 792:	
			sh = regs->gpr[rb] & 0x3f;
			ival = (signed int) regs->gpr[rd];
			regs->gpr[ra] = ival >> (sh < 32 ? sh : 31);
			if (ival < 0 && (sh >= 32 || (ival & ((1 << sh) - 1)) != 0))
				regs->xer |= XER_CA;
			else
				regs->xer &= ~XER_CA;
			goto logical_done;

		case 824:	
			sh = rb;
			ival = (signed int) regs->gpr[rd];
			regs->gpr[ra] = ival >> sh;
			if (ival < 0 && (ival & ((1 << sh) - 1)) != 0)
				regs->xer |= XER_CA;
			else
				regs->xer &= ~XER_CA;
			goto logical_done;

#ifdef __powerpc64__
		case 27:	
			sh = regs->gpr[rd] & 0x7f;
			if (sh < 64)
				regs->gpr[ra] = regs->gpr[rd] << sh;
			else
				regs->gpr[ra] = 0;
			goto logical_done;

		case 539:	
			sh = regs->gpr[rb] & 0x7f;
			if (sh < 64)
				regs->gpr[ra] = regs->gpr[rd] >> sh;
			else
				regs->gpr[ra] = 0;
			goto logical_done;

		case 794:	
			sh = regs->gpr[rb] & 0x7f;
			ival = (signed long int) regs->gpr[rd];
			regs->gpr[ra] = ival >> (sh < 64 ? sh : 63);
			if (ival < 0 && (sh >= 64 || (ival & ((1 << sh) - 1)) != 0))
				regs->xer |= XER_CA;
			else
				regs->xer &= ~XER_CA;
			goto logical_done;

		case 826:	
		case 827:	
			sh = rb | ((instr & 2) << 4);
			ival = (signed long int) regs->gpr[rd];
			regs->gpr[ra] = ival >> sh;
			if (ival < 0 && (ival & ((1 << sh) - 1)) != 0)
				regs->xer |= XER_CA;
			else
				regs->xer &= ~XER_CA;
			goto logical_done;
#endif 

		case 54:	
			ea = xform_ea(instr, regs, 0);
			if (!address_ok(regs, ea, 8))
				return 0;
			err = 0;
			__cacheop_user_asmx(ea, err, "dcbst");
			if (err)
				return 0;
			goto instr_done;

		case 86:	
			ea = xform_ea(instr, regs, 0);
			if (!address_ok(regs, ea, 8))
				return 0;
			err = 0;
			__cacheop_user_asmx(ea, err, "dcbf");
			if (err)
				return 0;
			goto instr_done;

		case 246:	
			if (rd == 0) {
				ea = xform_ea(instr, regs, 0);
				prefetchw((void *) ea);
			}
			goto instr_done;

		case 278:	
			if (rd == 0) {
				ea = xform_ea(instr, regs, 0);
				prefetch((void *) ea);
			}
			goto instr_done;

		}
		break;
	}

	if (regs->msr & MSR_LE)
		return 0;

	old_ra = regs->gpr[ra];

	switch (opcode) {
	case 31:
		u = instr & 0x40;
		switch ((instr >> 1) & 0x3ff) {
		case 20:	
			ea = xform_ea(instr, regs, 0);
			if (ea & 3)
				break;		
			err = -EFAULT;
			if (!address_ok(regs, ea, 4))
				goto ldst_done;
			err = 0;
			__get_user_asmx(val, ea, err, "lwarx");
			if (!err)
				regs->gpr[rd] = val;
			goto ldst_done;

		case 150:	
			ea = xform_ea(instr, regs, 0);
			if (ea & 3)
				break;		
			err = -EFAULT;
			if (!address_ok(regs, ea, 4))
				goto ldst_done;
			err = 0;
			__put_user_asmx(regs->gpr[rd], ea, err, "stwcx.", cr);
			if (!err)
				regs->ccr = (regs->ccr & 0x0fffffff) |
					(cr & 0xe0000000) |
					((regs->xer >> 3) & 0x10000000);
			goto ldst_done;

#ifdef __powerpc64__
		case 84:	
			ea = xform_ea(instr, regs, 0);
			if (ea & 7)
				break;		
			err = -EFAULT;
			if (!address_ok(regs, ea, 8))
				goto ldst_done;
			err = 0;
			__get_user_asmx(val, ea, err, "ldarx");
			if (!err)
				regs->gpr[rd] = val;
			goto ldst_done;

		case 214:	
			ea = xform_ea(instr, regs, 0);
			if (ea & 7)
				break;		
			err = -EFAULT;
			if (!address_ok(regs, ea, 8))
				goto ldst_done;
			err = 0;
			__put_user_asmx(regs->gpr[rd], ea, err, "stdcx.", cr);
			if (!err)
				regs->ccr = (regs->ccr & 0x0fffffff) |
					(cr & 0xe0000000) |
					((regs->xer >> 3) & 0x10000000);
			goto ldst_done;

		case 21:	
		case 53:	
			err = read_mem(&regs->gpr[rd], xform_ea(instr, regs, u),
				       8, regs);
			goto ldst_done;
#endif

		case 23:	
		case 55:	
			err = read_mem(&regs->gpr[rd], xform_ea(instr, regs, u),
				       4, regs);
			goto ldst_done;

		case 87:	
		case 119:	
			err = read_mem(&regs->gpr[rd], xform_ea(instr, regs, u),
				       1, regs);
			goto ldst_done;

#ifdef CONFIG_ALTIVEC
		case 103:	
		case 359:	
			if (!(regs->msr & MSR_VEC))
				break;
			ea = xform_ea(instr, regs, 0);
			err = do_vec_load(rd, do_lvx, ea, regs);
			goto ldst_done;

		case 231:	
		case 487:	
			if (!(regs->msr & MSR_VEC))
				break;
			ea = xform_ea(instr, regs, 0);
			err = do_vec_store(rd, do_stvx, ea, regs);
			goto ldst_done;
#endif 

#ifdef __powerpc64__
		case 149:	
		case 181:	
			val = regs->gpr[rd];
			err = write_mem(val, xform_ea(instr, regs, u), 8, regs);
			goto ldst_done;
#endif

		case 151:	
		case 183:	
			val = regs->gpr[rd];
			err = write_mem(val, xform_ea(instr, regs, u), 4, regs);
			goto ldst_done;

		case 215:	
		case 247:	
			val = regs->gpr[rd];
			err = write_mem(val, xform_ea(instr, regs, u), 1, regs);
			goto ldst_done;

		case 279:	
		case 311:	
			err = read_mem(&regs->gpr[rd], xform_ea(instr, regs, u),
				       2, regs);
			goto ldst_done;

#ifdef __powerpc64__
		case 341:	
		case 373:	
			err = read_mem(&regs->gpr[rd], xform_ea(instr, regs, u),
				       4, regs);
			if (!err)
				regs->gpr[rd] = (signed int) regs->gpr[rd];
			goto ldst_done;
#endif

		case 343:	
		case 375:	
			err = read_mem(&regs->gpr[rd], xform_ea(instr, regs, u),
				       2, regs);
			if (!err)
				regs->gpr[rd] = (signed short) regs->gpr[rd];
			goto ldst_done;

		case 407:	
		case 439:	
			val = regs->gpr[rd];
			err = write_mem(val, xform_ea(instr, regs, u), 2, regs);
			goto ldst_done;

#ifdef __powerpc64__
		case 532:	
			err = read_mem(&val, xform_ea(instr, regs, 0), 8, regs);
			if (!err)
				regs->gpr[rd] = byterev_8(val);
			goto ldst_done;

#endif

		case 534:	
			err = read_mem(&val, xform_ea(instr, regs, 0), 4, regs);
			if (!err)
				regs->gpr[rd] = byterev_4(val);
			goto ldst_done;

#ifdef CONFIG_PPC_CPU
		case 535:	
		case 567:	
			if (!(regs->msr & MSR_FP))
				break;
			ea = xform_ea(instr, regs, u);
			err = do_fp_load(rd, do_lfs, ea, 4, regs);
			goto ldst_done;

		case 599:	
		case 631:	
			if (!(regs->msr & MSR_FP))
				break;
			ea = xform_ea(instr, regs, u);
			err = do_fp_load(rd, do_lfd, ea, 8, regs);
			goto ldst_done;

		case 663:	
		case 695:	
			if (!(regs->msr & MSR_FP))
				break;
			ea = xform_ea(instr, regs, u);
			err = do_fp_store(rd, do_stfs, ea, 4, regs);
			goto ldst_done;

		case 727:	
		case 759:	
			if (!(regs->msr & MSR_FP))
				break;
			ea = xform_ea(instr, regs, u);
			err = do_fp_store(rd, do_stfd, ea, 8, regs);
			goto ldst_done;
#endif

#ifdef __powerpc64__
		case 660:	
			val = byterev_8(regs->gpr[rd]);
			err = write_mem(val, xform_ea(instr, regs, 0), 8, regs);
			goto ldst_done;

#endif
		case 662:	
			val = byterev_4(regs->gpr[rd]);
			err = write_mem(val, xform_ea(instr, regs, 0), 4, regs);
			goto ldst_done;

		case 790:	
			err = read_mem(&val, xform_ea(instr, regs, 0), 2, regs);
			if (!err)
				regs->gpr[rd] = byterev_2(val);
			goto ldst_done;

		case 918:	
			val = byterev_2(regs->gpr[rd]);
			err = write_mem(val, xform_ea(instr, regs, 0), 2, regs);
			goto ldst_done;

#ifdef CONFIG_VSX
		case 844:	
		case 876:	
			if (!(regs->msr & MSR_VSX))
				break;
			rd |= (instr & 1) << 5;
			ea = xform_ea(instr, regs, u);
			err = do_vsx_load(rd, do_lxvd2x, ea, regs);
			goto ldst_done;

		case 972:	
		case 1004:	
			if (!(regs->msr & MSR_VSX))
				break;
			rd |= (instr & 1) << 5;
			ea = xform_ea(instr, regs, u);
			err = do_vsx_store(rd, do_stxvd2x, ea, regs);
			goto ldst_done;

#endif 
		}
		break;

	case 32:	
	case 33:	
		err = read_mem(&regs->gpr[rd], dform_ea(instr, regs), 4, regs);
		goto ldst_done;

	case 34:	
	case 35:	
		err = read_mem(&regs->gpr[rd], dform_ea(instr, regs), 1, regs);
		goto ldst_done;

	case 36:	
	case 37:	
		val = regs->gpr[rd];
		err = write_mem(val, dform_ea(instr, regs), 4, regs);
		goto ldst_done;

	case 38:	
	case 39:	
		val = regs->gpr[rd];
		err = write_mem(val, dform_ea(instr, regs), 1, regs);
		goto ldst_done;

	case 40:	
	case 41:	
		err = read_mem(&regs->gpr[rd], dform_ea(instr, regs), 2, regs);
		goto ldst_done;

	case 42:	
	case 43:	
		err = read_mem(&regs->gpr[rd], dform_ea(instr, regs), 2, regs);
		if (!err)
			regs->gpr[rd] = (signed short) regs->gpr[rd];
		goto ldst_done;

	case 44:	
	case 45:	
		val = regs->gpr[rd];
		err = write_mem(val, dform_ea(instr, regs), 2, regs);
		goto ldst_done;

	case 46:	
		ra = (instr >> 16) & 0x1f;
		if (ra >= rd)
			break;		
		ea = dform_ea(instr, regs);
		do {
			err = read_mem(&regs->gpr[rd], ea, 4, regs);
			if (err)
				return 0;
			ea += 4;
		} while (++rd < 32);
		goto instr_done;

	case 47:	
		ea = dform_ea(instr, regs);
		do {
			err = write_mem(regs->gpr[rd], ea, 4, regs);
			if (err)
				return 0;
			ea += 4;
		} while (++rd < 32);
		goto instr_done;

#ifdef CONFIG_PPC_FPU
	case 48:	
	case 49:	
		if (!(regs->msr & MSR_FP))
			break;
		ea = dform_ea(instr, regs);
		err = do_fp_load(rd, do_lfs, ea, 4, regs);
		goto ldst_done;

	case 50:	
	case 51:	
		if (!(regs->msr & MSR_FP))
			break;
		ea = dform_ea(instr, regs);
		err = do_fp_load(rd, do_lfd, ea, 8, regs);
		goto ldst_done;

	case 52:	
	case 53:	
		if (!(regs->msr & MSR_FP))
			break;
		ea = dform_ea(instr, regs);
		err = do_fp_store(rd, do_stfs, ea, 4, regs);
		goto ldst_done;

	case 54:	
	case 55:	
		if (!(regs->msr & MSR_FP))
			break;
		ea = dform_ea(instr, regs);
		err = do_fp_store(rd, do_stfd, ea, 8, regs);
		goto ldst_done;
#endif

#ifdef __powerpc64__
	case 58:	
		switch (instr & 3) {
		case 0:		
			err = read_mem(&regs->gpr[rd], dsform_ea(instr, regs),
				       8, regs);
			goto ldst_done;
		case 1:		
			err = read_mem(&regs->gpr[rd], dsform_ea(instr, regs),
				       8, regs);
			goto ldst_done;
		case 2:		
			err = read_mem(&regs->gpr[rd], dsform_ea(instr, regs),
				       4, regs);
			if (!err)
				regs->gpr[rd] = (signed int) regs->gpr[rd];
			goto ldst_done;
		}
		break;

	case 62:	
		val = regs->gpr[rd];
		switch (instr & 3) {
		case 0:		
			err = write_mem(val, dsform_ea(instr, regs), 8, regs);
			goto ldst_done;
		case 1:		
			err = write_mem(val, dsform_ea(instr, regs), 8, regs);
			goto ldst_done;
		}
		break;
#endif 

	}
	err = -EINVAL;

 ldst_done:
	if (err) {
		regs->gpr[ra] = old_ra;
		return 0;	
	}
 instr_done:
	regs->nip = truncate_if_32bit(regs->msr, regs->nip + 4);
	return 1;

 logical_done:
	if (instr & 1)
		set_cr0(regs, ra);
	goto instr_done;

 arith_done:
	if (instr & 1)
		set_cr0(regs, rd);
	goto instr_done;
}
