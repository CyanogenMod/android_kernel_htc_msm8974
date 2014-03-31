/*
 * Architecture-specific unaligned trap handling.
 *
 * Copyright (C) 1999-2002, 2004 Hewlett-Packard Co
 *	Stephane Eranian <eranian@hpl.hp.com>
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 2002/12/09   Fix rotating register handling (off-by-1 error, missing fr-rotation).  Fix
 *		get_rse_reg() to not leak kernel bits to user-level (reading an out-of-frame
 *		stacked register returns an undefined value; it does NOT trigger a
 *		"rsvd register fault").
 * 2001/10/11	Fix unaligned access to rotating registers in s/w pipelined loops.
 * 2001/08/13	Correct size of extended floats (float_fsz) from 16 to 10 bytes.
 * 2001/01/17	Add support emulation of unaligned kernel accesses.
 */
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/ratelimit.h>

#include <asm/intrinsics.h>
#include <asm/processor.h>
#include <asm/rse.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

extern int die_if_kernel(char *str, struct pt_regs *regs, long err);

#undef DEBUG_UNALIGNED_TRAP

#ifdef DEBUG_UNALIGNED_TRAP
# define DPRINT(a...)	do { printk("%s %u: ", __func__, __LINE__); printk (a); } while (0)
# define DDUMP(str,vp,len)	dump(str, vp, len)

static void
dump (const char *str, void *vp, size_t len)
{
	unsigned char *cp = vp;
	int i;

	printk("%s", str);
	for (i = 0; i < len; ++i)
		printk (" %02x", *cp++);
	printk("\n");
}
#else
# define DPRINT(a...)
# define DDUMP(str,vp,len)
#endif

#define IA64_FIRST_STACKED_GR	32
#define IA64_FIRST_ROTATING_FR	32
#define SIGN_EXT9		0xffffffffffffff00ul

int no_unaligned_warning;
int unaligned_dump_stack;

#define IA64_OPCODE_MASK	0x1ef
#define IA64_OPCODE_SHIFT	32

#define LD_OP            0x080
#define LDS_OP           0x081
#define LDA_OP           0x082
#define LDSA_OP          0x083
#define LDBIAS_OP        0x084
#define LDACQ_OP         0x085
#define LDCCLR_OP        0x088
#define LDCNC_OP         0x089
#define LDCCLRACQ_OP     0x08a
#define ST_OP            0x08c
#define STREL_OP         0x08d


#define LD_IMM_OP            0x0a0
#define LDS_IMM_OP           0x0a1
#define LDA_IMM_OP           0x0a2
#define LDSA_IMM_OP          0x0a3
#define LDBIAS_IMM_OP        0x0a4
#define LDACQ_IMM_OP         0x0a5
#define LDCCLR_IMM_OP        0x0a8
#define LDCNC_IMM_OP         0x0a9
#define LDCCLRACQ_IMM_OP     0x0aa
#define ST_IMM_OP            0x0ac
#define STREL_IMM_OP         0x0ad

#define LDF_OP           0x0c0
#define LDFS_OP          0x0c1
#define LDFA_OP          0x0c2
#define LDFSA_OP         0x0c3
#define LDFCCLR_OP       0x0c8
#define LDFCNC_OP        0x0c9
#define STF_OP           0x0cc


#define LDF_IMM_OP       0x0e0
#define LDFS_IMM_OP      0x0e1
#define LDFA_IMM_OP      0x0e2
#define LDFSA_IMM_OP     0x0e3
#define LDFCCLR_IMM_OP   0x0e8
#define LDFCNC_IMM_OP    0x0e9
#define STF_IMM_OP       0x0ec

typedef struct {
	unsigned long	 qp:6;	
	unsigned long    r1:7;	
	unsigned long   imm:7;	
	unsigned long    r3:7;	
	unsigned long     x:1;  
	unsigned long  hint:2;	
	unsigned long x6_sz:2;	
	unsigned long x6_op:4;	
	unsigned long     m:1;	
	unsigned long    op:4;	
	unsigned long   pad:23; 
} load_store_t;


typedef enum {
	UPD_IMMEDIATE,	
	UPD_REG		
} update_t;


#define RPO(x)	((size_t) &((struct pt_regs *)0)->x)
#define RSO(x)	((size_t) &((struct switch_stack *)0)->x)

#define RPT(x)		(RPO(x) << 1)
#define RSW(x)		(1| RSO(x)<<1)

#define GR_OFFS(x)	(gr_info[x]>>1)
#define GR_IN_SW(x)	(gr_info[x] & 0x1)

#define FR_OFFS(x)	(fr_info[x]>>1)
#define FR_IN_SW(x)	(fr_info[x] & 0x1)

static u16 gr_info[32]={
	0,			

	RPT(r1), RPT(r2), RPT(r3),

	RSW(r4), RSW(r5), RSW(r6), RSW(r7),

	RPT(r8), RPT(r9), RPT(r10), RPT(r11),
	RPT(r12), RPT(r13), RPT(r14), RPT(r15),

	RPT(r16), RPT(r17), RPT(r18), RPT(r19),
	RPT(r20), RPT(r21), RPT(r22), RPT(r23),
	RPT(r24), RPT(r25), RPT(r26), RPT(r27),
	RPT(r28), RPT(r29), RPT(r30), RPT(r31)
};

static u16 fr_info[32]={
	0,			
	0,			

	RSW(f2), RSW(f3), RSW(f4), RSW(f5),

	RPT(f6), RPT(f7), RPT(f8), RPT(f9),
	RPT(f10), RPT(f11),

	RSW(f12), RSW(f13), RSW(f14),
	RSW(f15), RSW(f16), RSW(f17), RSW(f18), RSW(f19),
	RSW(f20), RSW(f21), RSW(f22), RSW(f23), RSW(f24),
	RSW(f25), RSW(f26), RSW(f27), RSW(f28), RSW(f29),
	RSW(f30), RSW(f31)
};

static void
invala_gr (int regno)
{
#	define F(reg)	case reg: ia64_invala_gr(reg); break

	switch (regno) {
		F(  0); F(  1); F(  2); F(  3); F(  4); F(  5); F(  6); F(  7);
		F(  8); F(  9); F( 10); F( 11); F( 12); F( 13); F( 14); F( 15);
		F( 16); F( 17); F( 18); F( 19); F( 20); F( 21); F( 22); F( 23);
		F( 24); F( 25); F( 26); F( 27); F( 28); F( 29); F( 30); F( 31);
		F( 32); F( 33); F( 34); F( 35); F( 36); F( 37); F( 38); F( 39);
		F( 40); F( 41); F( 42); F( 43); F( 44); F( 45); F( 46); F( 47);
		F( 48); F( 49); F( 50); F( 51); F( 52); F( 53); F( 54); F( 55);
		F( 56); F( 57); F( 58); F( 59); F( 60); F( 61); F( 62); F( 63);
		F( 64); F( 65); F( 66); F( 67); F( 68); F( 69); F( 70); F( 71);
		F( 72); F( 73); F( 74); F( 75); F( 76); F( 77); F( 78); F( 79);
		F( 80); F( 81); F( 82); F( 83); F( 84); F( 85); F( 86); F( 87);
		F( 88); F( 89); F( 90); F( 91); F( 92); F( 93); F( 94); F( 95);
		F( 96); F( 97); F( 98); F( 99); F(100); F(101); F(102); F(103);
		F(104); F(105); F(106); F(107); F(108); F(109); F(110); F(111);
		F(112); F(113); F(114); F(115); F(116); F(117); F(118); F(119);
		F(120); F(121); F(122); F(123); F(124); F(125); F(126); F(127);
	}
#	undef F
}

static void
invala_fr (int regno)
{
#	define F(reg)	case reg: ia64_invala_fr(reg); break

	switch (regno) {
		F(  0); F(  1); F(  2); F(  3); F(  4); F(  5); F(  6); F(  7);
		F(  8); F(  9); F( 10); F( 11); F( 12); F( 13); F( 14); F( 15);
		F( 16); F( 17); F( 18); F( 19); F( 20); F( 21); F( 22); F( 23);
		F( 24); F( 25); F( 26); F( 27); F( 28); F( 29); F( 30); F( 31);
		F( 32); F( 33); F( 34); F( 35); F( 36); F( 37); F( 38); F( 39);
		F( 40); F( 41); F( 42); F( 43); F( 44); F( 45); F( 46); F( 47);
		F( 48); F( 49); F( 50); F( 51); F( 52); F( 53); F( 54); F( 55);
		F( 56); F( 57); F( 58); F( 59); F( 60); F( 61); F( 62); F( 63);
		F( 64); F( 65); F( 66); F( 67); F( 68); F( 69); F( 70); F( 71);
		F( 72); F( 73); F( 74); F( 75); F( 76); F( 77); F( 78); F( 79);
		F( 80); F( 81); F( 82); F( 83); F( 84); F( 85); F( 86); F( 87);
		F( 88); F( 89); F( 90); F( 91); F( 92); F( 93); F( 94); F( 95);
		F( 96); F( 97); F( 98); F( 99); F(100); F(101); F(102); F(103);
		F(104); F(105); F(106); F(107); F(108); F(109); F(110); F(111);
		F(112); F(113); F(114); F(115); F(116); F(117); F(118); F(119);
		F(120); F(121); F(122); F(123); F(124); F(125); F(126); F(127);
	}
#	undef F
}

static inline unsigned long
rotate_reg (unsigned long sor, unsigned long rrb, unsigned long reg)
{
	reg += rrb;
	if (reg >= sor)
		reg -= sor;
	return reg;
}

static void
set_rse_reg (struct pt_regs *regs, unsigned long r1, unsigned long val, int nat)
{
	struct switch_stack *sw = (struct switch_stack *) regs - 1;
	unsigned long *bsp, *bspstore, *addr, *rnat_addr, *ubs_end;
	unsigned long *kbs = (void *) current + IA64_RBS_OFFSET;
	unsigned long rnats, nat_mask;
	unsigned long on_kbs;
	long sof = (regs->cr_ifs) & 0x7f;
	long sor = 8 * ((regs->cr_ifs >> 14) & 0xf);
	long rrb_gr = (regs->cr_ifs >> 18) & 0x7f;
	long ridx = r1 - 32;

	if (ridx >= sof) {
		
		DPRINT("ignoring write to r%lu; only %lu registers are allocated!\n", r1, sof);
		return;
	}

	if (ridx < sor)
		ridx = rotate_reg(sor, rrb_gr, ridx);

	DPRINT("r%lu, sw.bspstore=%lx pt.bspstore=%lx sof=%ld sol=%ld ridx=%ld\n",
	       r1, sw->ar_bspstore, regs->ar_bspstore, sof, (regs->cr_ifs >> 7) & 0x7f, ridx);

	on_kbs = ia64_rse_num_regs(kbs, (unsigned long *) sw->ar_bspstore);
	addr = ia64_rse_skip_regs((unsigned long *) sw->ar_bspstore, -sof + ridx);
	if (addr >= kbs) {
		
		rnat_addr = ia64_rse_rnat_addr(addr);
		if ((unsigned long) rnat_addr >= sw->ar_bspstore)
			rnat_addr = &sw->ar_rnat;
		nat_mask = 1UL << ia64_rse_slot_num(addr);

		*addr = val;
		if (nat)
			*rnat_addr |=  nat_mask;
		else
			*rnat_addr &= ~nat_mask;
		return;
	}

	if (!user_stack(current, regs)) {
		DPRINT("ignoring kernel write to r%lu; register isn't on the kernel RBS!", r1);
		return;
	}

	bspstore = (unsigned long *)regs->ar_bspstore;
	ubs_end = ia64_rse_skip_regs(bspstore, on_kbs);
	bsp     = ia64_rse_skip_regs(ubs_end, -sof);
	addr    = ia64_rse_skip_regs(bsp, ridx);

	DPRINT("ubs_end=%p bsp=%p addr=%p\n", (void *) ubs_end, (void *) bsp, (void *) addr);

	ia64_poke(current, sw, (unsigned long) ubs_end, (unsigned long) addr, val);

	rnat_addr = ia64_rse_rnat_addr(addr);

	ia64_peek(current, sw, (unsigned long) ubs_end, (unsigned long) rnat_addr, &rnats);
	DPRINT("rnat @%p = 0x%lx nat=%d old nat=%ld\n",
	       (void *) rnat_addr, rnats, nat, (rnats >> ia64_rse_slot_num(addr)) & 1);

	nat_mask = 1UL << ia64_rse_slot_num(addr);
	if (nat)
		rnats |=  nat_mask;
	else
		rnats &= ~nat_mask;
	ia64_poke(current, sw, (unsigned long) ubs_end, (unsigned long) rnat_addr, rnats);

	DPRINT("rnat changed to @%p = 0x%lx\n", (void *) rnat_addr, rnats);
}


static void
get_rse_reg (struct pt_regs *regs, unsigned long r1, unsigned long *val, int *nat)
{
	struct switch_stack *sw = (struct switch_stack *) regs - 1;
	unsigned long *bsp, *addr, *rnat_addr, *ubs_end, *bspstore;
	unsigned long *kbs = (void *) current + IA64_RBS_OFFSET;
	unsigned long rnats, nat_mask;
	unsigned long on_kbs;
	long sof = (regs->cr_ifs) & 0x7f;
	long sor = 8 * ((regs->cr_ifs >> 14) & 0xf);
	long rrb_gr = (regs->cr_ifs >> 18) & 0x7f;
	long ridx = r1 - 32;

	if (ridx >= sof) {
		
		DPRINT("ignoring read from r%lu; only %lu registers are allocated!\n", r1, sof);
		goto fail;
	}

	if (ridx < sor)
		ridx = rotate_reg(sor, rrb_gr, ridx);

	DPRINT("r%lu, sw.bspstore=%lx pt.bspstore=%lx sof=%ld sol=%ld ridx=%ld\n",
	       r1, sw->ar_bspstore, regs->ar_bspstore, sof, (regs->cr_ifs >> 7) & 0x7f, ridx);

	on_kbs = ia64_rse_num_regs(kbs, (unsigned long *) sw->ar_bspstore);
	addr = ia64_rse_skip_regs((unsigned long *) sw->ar_bspstore, -sof + ridx);
	if (addr >= kbs) {
		
		*val = *addr;
		if (nat) {
			rnat_addr = ia64_rse_rnat_addr(addr);
			if ((unsigned long) rnat_addr >= sw->ar_bspstore)
				rnat_addr = &sw->ar_rnat;
			nat_mask = 1UL << ia64_rse_slot_num(addr);
			*nat = (*rnat_addr & nat_mask) != 0;
		}
		return;
	}

	if (!user_stack(current, regs)) {
		DPRINT("ignoring kernel read of r%lu; register isn't on the RBS!", r1);
		goto fail;
	}

	bspstore = (unsigned long *)regs->ar_bspstore;
	ubs_end = ia64_rse_skip_regs(bspstore, on_kbs);
	bsp     = ia64_rse_skip_regs(ubs_end, -sof);
	addr    = ia64_rse_skip_regs(bsp, ridx);

	DPRINT("ubs_end=%p bsp=%p addr=%p\n", (void *) ubs_end, (void *) bsp, (void *) addr);

	ia64_peek(current, sw, (unsigned long) ubs_end, (unsigned long) addr, val);

	if (nat) {
		rnat_addr = ia64_rse_rnat_addr(addr);
		nat_mask = 1UL << ia64_rse_slot_num(addr);

		DPRINT("rnat @%p = 0x%lx\n", (void *) rnat_addr, rnats);

		ia64_peek(current, sw, (unsigned long) ubs_end, (unsigned long) rnat_addr, &rnats);
		*nat = (rnats & nat_mask) != 0;
	}
	return;

  fail:
	*val = 0;
	if (nat)
		*nat = 0;
	return;
}


static void
setreg (unsigned long regnum, unsigned long val, int nat, struct pt_regs *regs)
{
	struct switch_stack *sw = (struct switch_stack *) regs - 1;
	unsigned long addr;
	unsigned long bitmask;
	unsigned long *unat;

	if (regnum >= IA64_FIRST_STACKED_GR) {
		set_rse_reg(regs, regnum, val, nat);
		return;
	}


	if (GR_IN_SW(regnum)) {
		addr = (unsigned long)sw;
		unat = &sw->ar_unat;
	} else {
		addr = (unsigned long)regs;
		unat = &sw->caller_unat;
	}
	DPRINT("tmp_base=%lx switch_stack=%s offset=%d\n",
	       addr, unat==&sw->ar_unat ? "yes":"no", GR_OFFS(regnum));
	addr += GR_OFFS(regnum);

	*(unsigned long *)addr = val;

	bitmask   = 1UL << (addr >> 3 & 0x3f);
	DPRINT("*0x%lx=0x%lx NaT=%d prev_unat @%p=%lx\n", addr, val, nat, (void *) unat, *unat);
	if (nat) {
		*unat |= bitmask;
	} else {
		*unat &= ~bitmask;
	}
	DPRINT("*0x%lx=0x%lx NaT=%d new unat: %p=%lx\n", addr, val, nat, (void *) unat,*unat);
}

static inline unsigned long
fph_index (struct pt_regs *regs, long regnum)
{
	unsigned long rrb_fr = (regs->cr_ifs >> 25) & 0x7f;
	return rotate_reg(96, rrb_fr, (regnum - IA64_FIRST_ROTATING_FR));
}

static void
setfpreg (unsigned long regnum, struct ia64_fpreg *fpval, struct pt_regs *regs)
{
	struct switch_stack *sw = (struct switch_stack *)regs - 1;
	unsigned long addr;

	if (regnum >= IA64_FIRST_ROTATING_FR) {
		ia64_sync_fph(current);
		current->thread.fph[fph_index(regs, regnum)] = *fpval;
	} else {
		if (FR_IN_SW(regnum)) {
			addr = (unsigned long)sw;
		} else {
			addr = (unsigned long)regs;
		}

		DPRINT("tmp_base=%lx offset=%d\n", addr, FR_OFFS(regnum));

		addr += FR_OFFS(regnum);
		*(struct ia64_fpreg *)addr = *fpval;

		regs->cr_ipsr |= IA64_PSR_MFL;
	}
}

static inline void
float_spill_f0 (struct ia64_fpreg *final)
{
	ia64_stf_spill(final, 0);
}

static inline void
float_spill_f1 (struct ia64_fpreg *final)
{
	ia64_stf_spill(final, 1);
}

static void
getfpreg (unsigned long regnum, struct ia64_fpreg *fpval, struct pt_regs *regs)
{
	struct switch_stack *sw = (struct switch_stack *) regs - 1;
	unsigned long addr;

	if (regnum >= IA64_FIRST_ROTATING_FR) {
		ia64_flush_fph(current);
		*fpval = current->thread.fph[fph_index(regs, regnum)];
	} else {
		switch(regnum) {
		case 0:
			float_spill_f0(fpval);
			break;
		case 1:
			float_spill_f1(fpval);
			break;
		default:
			addr =  FR_IN_SW(regnum) ? (unsigned long)sw
						 : (unsigned long)regs;

			DPRINT("is_sw=%d tmp_base=%lx offset=0x%x\n",
			       FR_IN_SW(regnum), addr, FR_OFFS(regnum));

			addr  += FR_OFFS(regnum);
			*fpval = *(struct ia64_fpreg *)addr;
		}
	}
}


static void
getreg (unsigned long regnum, unsigned long *val, int *nat, struct pt_regs *regs)
{
	struct switch_stack *sw = (struct switch_stack *) regs - 1;
	unsigned long addr, *unat;

	if (regnum >= IA64_FIRST_STACKED_GR) {
		get_rse_reg(regs, regnum, val, nat);
		return;
	}

	if (regnum == 0) {
		*val = 0;
		if (nat)
			*nat = 0;
		return;
	}

	if (GR_IN_SW(regnum)) {
		addr = (unsigned long)sw;
		unat = &sw->ar_unat;
	} else {
		addr = (unsigned long)regs;
		unat = &sw->caller_unat;
	}

	DPRINT("addr_base=%lx offset=0x%x\n", addr,  GR_OFFS(regnum));

	addr += GR_OFFS(regnum);

	*val  = *(unsigned long *)addr;

	if (nat)
		*nat  = (*unat >> (addr >> 3 & 0x3f)) & 0x1UL;
}

static void
emulate_load_updates (update_t type, load_store_t ld, struct pt_regs *regs, unsigned long ifa)
{
	if (ld.x6_op == 1 || ld.x6_op == 3) {
		printk(KERN_ERR "%s: register update on speculative load, error\n", __func__);
		if (die_if_kernel("unaligned reference on speculative load with register update\n",
				  regs, 30))
			return;
	}


	if (type == UPD_IMMEDIATE) {
		unsigned long imm;

		imm = ld.x << 7 | ld.imm;

		if (ld.m) imm |= SIGN_EXT9;

		ifa += imm;

		setreg(ld.r3, ifa, 0, regs);

		DPRINT("ld.x=%d ld.m=%d imm=%ld r3=0x%lx\n", ld.x, ld.m, imm, ifa);

	} else if (ld.m) {
		unsigned long r2;
		int nat_r2;

		getreg(ld.imm, &r2, &nat_r2, regs);

		ifa += r2;

		setreg(ld.r3, ifa, nat_r2, regs);

		DPRINT("imm=%d r2=%ld r3=0x%lx nat_r2=%d\n",ld.imm, r2, ifa, nat_r2);
	}
}


static int
emulate_load_int (unsigned long ifa, load_store_t ld, struct pt_regs *regs)
{
	unsigned int len = 1 << ld.x6_sz;
	unsigned long val = 0;



	if (len != 2 && len != 4 && len != 8) {
		DPRINT("unknown size: x6=%d\n", ld.x6_sz);
		return -1;
	}
	
	if (copy_from_user(&val, (void __user *) ifa, len))
		return -1;
	setreg(ld.r1, val, 0, regs);

	if (ld.op == 0x5 || ld.m)
		emulate_load_updates(ld.op == 0x5 ? UPD_IMMEDIATE: UPD_REG, ld, regs, ifa);


	if (ld.x6_op == 0x5 || ld.x6_op == 0xa)
		mb();

	if (ld.x6_op == 0x2)
		invala_gr(ld.r1);

	return 0;
}

static int
emulate_store_int (unsigned long ifa, load_store_t ld, struct pt_regs *regs)
{
	unsigned long r2;
	unsigned int len = 1 << ld.x6_sz;

	getreg(ld.imm, &r2, NULL, regs);

	DPRINT("st%d [%lx]=%lx\n", len, ifa, r2);

	if (len != 2 && len != 4 && len != 8) {
		DPRINT("unknown size: x6=%d\n", ld.x6_sz);
		return -1;
	}

	
	if (copy_to_user((void __user *) ifa, &r2, len))
		return -1;

	if (ld.op == 0x5) {
		unsigned long imm;

		imm = ld.x << 7 | ld.r1;
		if (ld.m) imm |= SIGN_EXT9;
		ifa += imm;

		DPRINT("imm=%lx r3=%lx\n", imm, ifa);

		setreg(ld.r3, ifa, 0, regs);
	}
	ia64_invala();

	if (ld.x6_op == 0xd)
		mb();

	return 0;
}

static const unsigned char float_fsz[4]={
	10, 
	8,  
	4,  
	8   
};

static inline void
mem2float_extended (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldfe(6, init);
	ia64_stop();
	ia64_stf_spill(final, 6);
}

static inline void
mem2float_integer (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldf8(6, init);
	ia64_stop();
	ia64_stf_spill(final, 6);
}

static inline void
mem2float_single (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldfs(6, init);
	ia64_stop();
	ia64_stf_spill(final, 6);
}

static inline void
mem2float_double (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldfd(6, init);
	ia64_stop();
	ia64_stf_spill(final, 6);
}

static inline void
float2mem_extended (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldf_fill(6, init);
	ia64_stop();
	ia64_stfe(final, 6);
}

static inline void
float2mem_integer (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldf_fill(6, init);
	ia64_stop();
	ia64_stf8(final, 6);
}

static inline void
float2mem_single (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldf_fill(6, init);
	ia64_stop();
	ia64_stfs(final, 6);
}

static inline void
float2mem_double (struct ia64_fpreg *init, struct ia64_fpreg *final)
{
	ia64_ldf_fill(6, init);
	ia64_stop();
	ia64_stfd(final, 6);
}

static int
emulate_load_floatpair (unsigned long ifa, load_store_t ld, struct pt_regs *regs)
{
	struct ia64_fpreg fpr_init[2];
	struct ia64_fpreg fpr_final[2];
	unsigned long len = float_fsz[ld.x6_sz];


	memset(&fpr_init, 0, sizeof(fpr_init));
	memset(&fpr_final, 0, sizeof(fpr_final));

	if (ld.x6_op != 0x2) {
		if (copy_from_user(&fpr_init[0], (void __user *) ifa, len)
		    || copy_from_user(&fpr_init[1], (void __user *) (ifa + len), len))
			return -1;

		DPRINT("ld.r1=%d ld.imm=%d x6_sz=%d\n", ld.r1, ld.imm, ld.x6_sz);
		DDUMP("frp_init =", &fpr_init, 2*len);
		switch( ld.x6_sz ) {
			case 0:
				mem2float_extended(&fpr_init[0], &fpr_final[0]);
				mem2float_extended(&fpr_init[1], &fpr_final[1]);
				break;
			case 1:
				mem2float_integer(&fpr_init[0], &fpr_final[0]);
				mem2float_integer(&fpr_init[1], &fpr_final[1]);
				break;
			case 2:
				mem2float_single(&fpr_init[0], &fpr_final[0]);
				mem2float_single(&fpr_init[1], &fpr_final[1]);
				break;
			case 3:
				mem2float_double(&fpr_init[0], &fpr_final[0]);
				mem2float_double(&fpr_init[1], &fpr_final[1]);
				break;
		}
		DDUMP("fpr_final =", &fpr_final, 2*len);
		setfpreg(ld.r1, &fpr_final[0], regs);
		setfpreg(ld.imm, &fpr_final[1], regs);
	}

	if (ld.m) {
		ifa += len<<1;

		if (ld.x6_op == 1 || ld.x6_op == 3)
			printk(KERN_ERR "%s: register update on speculative load pair, error\n",
			       __func__);

		setreg(ld.r3, ifa, 0, regs);
	}

	if (ld.x6_op == 0x2) {
		invala_fr(ld.r1);
		invala_fr(ld.imm);
	}
	return 0;
}


static int
emulate_load_float (unsigned long ifa, load_store_t ld, struct pt_regs *regs)
{
	struct ia64_fpreg fpr_init;
	struct ia64_fpreg fpr_final;
	unsigned long len = float_fsz[ld.x6_sz];


	memset(&fpr_init,0, sizeof(fpr_init));
	memset(&fpr_final,0, sizeof(fpr_final));

	if (ld.x6_op != 0x2) {
		if (copy_from_user(&fpr_init, (void __user *) ifa, len))
			return -1;

		DPRINT("ld.r1=%d x6_sz=%d\n", ld.r1, ld.x6_sz);
		DDUMP("fpr_init =", &fpr_init, len);
		switch( ld.x6_sz ) {
			case 0:
				mem2float_extended(&fpr_init, &fpr_final);
				break;
			case 1:
				mem2float_integer(&fpr_init, &fpr_final);
				break;
			case 2:
				mem2float_single(&fpr_init, &fpr_final);
				break;
			case 3:
				mem2float_double(&fpr_init, &fpr_final);
				break;
		}
		DDUMP("fpr_final =", &fpr_final, len);
		setfpreg(ld.r1, &fpr_final, regs);
	}

	if (ld.op == 0x7 || ld.m)
		emulate_load_updates(ld.op == 0x7 ? UPD_IMMEDIATE: UPD_REG, ld, regs, ifa);

	if (ld.x6_op == 0x2)
		invala_fr(ld.r1);

	return 0;
}


static int
emulate_store_float (unsigned long ifa, load_store_t ld, struct pt_regs *regs)
{
	struct ia64_fpreg fpr_init;
	struct ia64_fpreg fpr_final;
	unsigned long len = float_fsz[ld.x6_sz];

	memset(&fpr_init,0, sizeof(fpr_init));
	memset(&fpr_final,0, sizeof(fpr_final));

	getfpreg(ld.imm, &fpr_init, regs);
	switch( ld.x6_sz ) {
		case 0:
			float2mem_extended(&fpr_init, &fpr_final);
			break;
		case 1:
			float2mem_integer(&fpr_init, &fpr_final);
			break;
		case 2:
			float2mem_single(&fpr_init, &fpr_final);
			break;
		case 3:
			float2mem_double(&fpr_init, &fpr_final);
			break;
	}
	DPRINT("ld.r1=%d x6_sz=%d\n", ld.r1, ld.x6_sz);
	DDUMP("fpr_init =", &fpr_init, len);
	DDUMP("fpr_final =", &fpr_final, len);

	if (copy_to_user((void __user *) ifa, &fpr_final, len))
		return -1;

	if (ld.op == 0x7) {
		unsigned long imm;

		imm = ld.x << 7 | ld.r1;
		if (ld.m)
			imm |= SIGN_EXT9;
		ifa += imm;

		DPRINT("imm=%lx r3=%lx\n", imm, ifa);

		setreg(ld.r3, ifa, 0, regs);
	}
	ia64_invala();

	return 0;
}

static DEFINE_RATELIMIT_STATE(logging_rate_limit, 5 * HZ, 5);

void
ia64_handle_unaligned (unsigned long ifa, struct pt_regs *regs)
{
	struct ia64_psr *ipsr = ia64_psr(regs);
	mm_segment_t old_fs = get_fs();
	unsigned long bundle[2];
	unsigned long opcode;
	struct siginfo si;
	const struct exception_table_entry *eh = NULL;
	union {
		unsigned long l;
		load_store_t insn;
	} u;
	int ret = -1;

	if (ia64_psr(regs)->be) {
		
		if (die_if_kernel("big-endian unaligned accesses are not supported", regs, 0))
			return;
		goto force_sigbus;
	}

	if (!user_mode(regs))
		eh = search_exception_tables(regs->cr_iip + ia64_psr(regs)->ri);
	if (user_mode(regs) || eh) {
		if ((current->thread.flags & IA64_THREAD_UAC_SIGBUS) != 0)
			goto force_sigbus;

		if (!no_unaligned_warning &&
		    !(current->thread.flags & IA64_THREAD_UAC_NOPRINT) &&
		    __ratelimit(&logging_rate_limit))
		{
			char buf[200];	
			size_t len;

			len = sprintf(buf, "%s(%d): unaligned access to 0x%016lx, "
				      "ip=0x%016lx\n\r", current->comm,
				      task_pid_nr(current),
				      ifa, regs->cr_iip + ipsr->ri);
			if (user_mode(regs))
				tty_write_message(current->signal->tty, buf);
			buf[len-1] = '\0';	
			
			printk(KERN_WARNING "%s", buf);
		} else {
			if (no_unaligned_warning) {
				printk_once(KERN_WARNING "%s(%d) encountered an "
				       "unaligned exception which required\n"
				       "kernel assistance, which degrades "
				       "the performance of the application.\n"
				       "Unaligned exception warnings have "
				       "been disabled by the system "
				       "administrator\n"
				       "echo 0 > /proc/sys/kernel/ignore-"
				       "unaligned-usertrap to re-enable\n",
				       current->comm, task_pid_nr(current));
			}
		}
	} else {
		if (__ratelimit(&logging_rate_limit)) {
			printk(KERN_WARNING "kernel unaligned access to 0x%016lx, ip=0x%016lx\n",
			       ifa, regs->cr_iip + ipsr->ri);
			if (unaligned_dump_stack)
				dump_stack();
		}
		set_fs(KERNEL_DS);
	}

	DPRINT("iip=%lx ifa=%lx isr=%lx (ei=%d, sp=%d)\n",
	       regs->cr_iip, ifa, regs->cr_ipsr, ipsr->ri, ipsr->it);

	if (__copy_from_user(bundle, (void __user *) regs->cr_iip, 16))
		goto failure;

	switch (ipsr->ri) {
	      case 0: u.l = (bundle[0] >>  5); break;
	      case 1: u.l = (bundle[0] >> 46) | (bundle[1] << 18); break;
	      case 2: u.l = (bundle[1] >> 23); break;
	}
	opcode = (u.l >> IA64_OPCODE_SHIFT) & IA64_OPCODE_MASK;

	DPRINT("opcode=%lx ld.qp=%d ld.r1=%d ld.imm=%d ld.r3=%d ld.x=%d ld.hint=%d "
	       "ld.x6=0x%x ld.m=%d ld.op=%d\n", opcode, u.insn.qp, u.insn.r1, u.insn.imm,
	       u.insn.r3, u.insn.x, u.insn.hint, u.insn.x6_sz, u.insn.m, u.insn.op);

	switch (opcode) {
	      case LDS_OP:
	      case LDSA_OP:
		if (u.insn.x)
			
			goto failure;
		
	      case LDS_IMM_OP:
	      case LDSA_IMM_OP:
	      case LDFS_OP:
	      case LDFSA_OP:
	      case LDFS_IMM_OP:
		DPRINT("forcing PSR_ED\n");
		regs->cr_ipsr |= IA64_PSR_ED;
		goto done;

	      case LD_OP:
	      case LDA_OP:
	      case LDBIAS_OP:
	      case LDACQ_OP:
	      case LDCCLR_OP:
	      case LDCNC_OP:
	      case LDCCLRACQ_OP:
		if (u.insn.x)
			
			goto failure;
		
	      case LD_IMM_OP:
	      case LDA_IMM_OP:
	      case LDBIAS_IMM_OP:
	      case LDACQ_IMM_OP:
	      case LDCCLR_IMM_OP:
	      case LDCNC_IMM_OP:
	      case LDCCLRACQ_IMM_OP:
		ret = emulate_load_int(ifa, u.insn, regs);
		break;

	      case ST_OP:
	      case STREL_OP:
		if (u.insn.x)
			
			goto failure;
		
	      case ST_IMM_OP:
	      case STREL_IMM_OP:
		ret = emulate_store_int(ifa, u.insn, regs);
		break;

	      case LDF_OP:
	      case LDFA_OP:
	      case LDFCCLR_OP:
	      case LDFCNC_OP:
		if (u.insn.x)
			ret = emulate_load_floatpair(ifa, u.insn, regs);
		else
			ret = emulate_load_float(ifa, u.insn, regs);
		break;

	      case LDF_IMM_OP:
	      case LDFA_IMM_OP:
	      case LDFCCLR_IMM_OP:
	      case LDFCNC_IMM_OP:
		ret = emulate_load_float(ifa, u.insn, regs);
		break;

	      case STF_OP:
	      case STF_IMM_OP:
		ret = emulate_store_float(ifa, u.insn, regs);
		break;

	      default:
		goto failure;
	}
	DPRINT("ret=%d\n", ret);
	if (ret)
		goto failure;

	if (ipsr->ri == 2)
		regs->cr_iip += 16;
	ipsr->ri = (ipsr->ri + 1) & 0x3;

	DPRINT("ipsr->ri=%d iip=%lx\n", ipsr->ri, regs->cr_iip);
  done:
	set_fs(old_fs);		
	return;

  failure:
	
	if (!user_mode(regs)) {
		if (eh) {
			ia64_handle_exception(regs, eh);
			goto done;
		}
		if (die_if_kernel("error during unaligned kernel access\n", regs, ret))
			return;
		
	}
  force_sigbus:
	si.si_signo = SIGBUS;
	si.si_errno = 0;
	si.si_code = BUS_ADRALN;
	si.si_addr = (void __user *) ifa;
	si.si_flags = 0;
	si.si_isr = 0;
	si.si_imm = 0;
	force_sig_info(SIGBUS, &si, current);
	goto done;
}
