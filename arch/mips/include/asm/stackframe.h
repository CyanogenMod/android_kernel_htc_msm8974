/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 95, 96, 99, 2001 Ralf Baechle
 * Copyright (C) 1994, 1995, 1996 Paul M. Antoine.
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2007  Maciej W. Rozycki
 */
#ifndef _ASM_STACKFRAME_H
#define _ASM_STACKFRAME_H

#include <linux/threads.h>

#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/mipsregs.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_MIPS_MT_SMTC
#define STATMASK 0x1e
#elif defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)
#define STATMASK 0x3f
#else
#define STATMASK 0x1f
#endif

#ifdef CONFIG_MIPS_MT_SMTC
#include <asm/mipsmtregs.h>
#endif 

		.macro	SAVE_AT
		.set	push
		.set	noat
		LONG_S	$1, PT_R1(sp)
		.set	pop
		.endm

		.macro	SAVE_TEMP
#ifdef CONFIG_CPU_HAS_SMARTMIPS
		mflhxu	v1
		LONG_S	v1, PT_LO(sp)
		mflhxu	v1
		LONG_S	v1, PT_HI(sp)
		mflhxu	v1
		LONG_S	v1, PT_ACX(sp)
#else
		mfhi	v1
#endif
#ifdef CONFIG_32BIT
		LONG_S	$8, PT_R8(sp)
		LONG_S	$9, PT_R9(sp)
#endif
		LONG_S	$10, PT_R10(sp)
		LONG_S	$11, PT_R11(sp)
		LONG_S	$12, PT_R12(sp)
#ifndef CONFIG_CPU_HAS_SMARTMIPS
		LONG_S	v1, PT_HI(sp)
		mflo	v1
#endif
		LONG_S	$13, PT_R13(sp)
		LONG_S	$14, PT_R14(sp)
		LONG_S	$15, PT_R15(sp)
		LONG_S	$24, PT_R24(sp)
#ifndef CONFIG_CPU_HAS_SMARTMIPS
		LONG_S	v1, PT_LO(sp)
#endif
		.endm

		.macro	SAVE_STATIC
		LONG_S	$16, PT_R16(sp)
		LONG_S	$17, PT_R17(sp)
		LONG_S	$18, PT_R18(sp)
		LONG_S	$19, PT_R19(sp)
		LONG_S	$20, PT_R20(sp)
		LONG_S	$21, PT_R21(sp)
		LONG_S	$22, PT_R22(sp)
		LONG_S	$23, PT_R23(sp)
		LONG_S	$30, PT_R30(sp)
		.endm

#ifdef CONFIG_SMP
#ifdef CONFIG_MIPS_MT_SMTC
#define PTEBASE_SHIFT	19	
#define CPU_ID_REG CP0_TCBIND
#define CPU_ID_MFC0 mfc0
#elif defined(CONFIG_MIPS_PGD_C0_CONTEXT)
#define PTEBASE_SHIFT	48	
#define CPU_ID_REG CP0_XCONTEXT
#define CPU_ID_MFC0 MFC0
#else
#define PTEBASE_SHIFT	23	
#define CPU_ID_REG CP0_CONTEXT
#define CPU_ID_MFC0 MFC0
#endif
		.macro	get_saved_sp	
		CPU_ID_MFC0	k0, CPU_ID_REG
#if defined(CONFIG_32BIT) || defined(KBUILD_64BIT_SYM32)
		lui	k1, %hi(kernelsp)
#else
		lui	k1, %highest(kernelsp)
		daddiu	k1, %higher(kernelsp)
		dsll	k1, 16
		daddiu	k1, %hi(kernelsp)
		dsll	k1, 16
#endif
		LONG_SRL	k0, PTEBASE_SHIFT
		LONG_ADDU	k1, k0
		LONG_L	k1, %lo(kernelsp)(k1)
		.endm

		.macro	set_saved_sp stackp temp temp2
		CPU_ID_MFC0	\temp, CPU_ID_REG
		LONG_SRL	\temp, PTEBASE_SHIFT
		LONG_S	\stackp, kernelsp(\temp)
		.endm
#else
		.macro	get_saved_sp	
#ifdef CONFIG_CPU_JUMP_WORKAROUNDS
		move	k0, ra
		jal	1f
		 nop
1:		jal	1f
		 nop
1:		jal	1f
		 nop
1:		jal	1f
		 nop
1:		move	ra, k0
		li	k0, 3
		mtc0	k0, $22
#endif 
#if defined(CONFIG_32BIT) || defined(KBUILD_64BIT_SYM32)
		lui	k1, %hi(kernelsp)
#else
		lui	k1, %highest(kernelsp)
		daddiu	k1, %higher(kernelsp)
		dsll	k1, k1, 16
		daddiu	k1, %hi(kernelsp)
		dsll	k1, k1, 16
#endif
		LONG_L	k1, %lo(kernelsp)(k1)
		.endm

		.macro	set_saved_sp stackp temp temp2
		LONG_S	\stackp, kernelsp
		.endm
#endif

		.macro	SAVE_SOME
		.set	push
		.set	noat
		.set	reorder
		mfc0	k0, CP0_STATUS
		sll	k0, 3		
		.set	noreorder
		bltz	k0, 8f
		 move	k1, sp
		.set	reorder
		
		get_saved_sp
#ifndef CONFIG_CPU_DADDI_WORKAROUNDS
8:		move	k0, sp
		PTR_SUBU sp, k1, PT_SIZE
#else
		.set	at=k0
8:		PTR_SUBU k1, PT_SIZE
		.set	noat
		move	k0, sp
		move	sp, k1
#endif
		LONG_S	k0, PT_R29(sp)
		LONG_S	$3, PT_R3(sp)
		LONG_S	$0, PT_R0(sp)
		mfc0	v1, CP0_STATUS
		LONG_S	$2, PT_R2(sp)
#ifdef CONFIG_MIPS_MT_SMTC
		.set	mips32
		mfc0	k0, CP0_TCSTATUS
		.set	mips0
		LONG_S	k0, PT_TCSTATUS(sp)
#endif 
		LONG_S	$4, PT_R4(sp)
		LONG_S	$5, PT_R5(sp)
		LONG_S	v1, PT_STATUS(sp)
		mfc0	v1, CP0_CAUSE
		LONG_S	$6, PT_R6(sp)
		LONG_S	$7, PT_R7(sp)
		LONG_S	v1, PT_CAUSE(sp)
		MFC0	v1, CP0_EPC
#ifdef CONFIG_64BIT
		LONG_S	$8, PT_R8(sp)
		LONG_S	$9, PT_R9(sp)
#endif
		LONG_S	$25, PT_R25(sp)
		LONG_S	$28, PT_R28(sp)
		LONG_S	$31, PT_R31(sp)
		LONG_S	v1, PT_EPC(sp)
		ori	$28, sp, _THREAD_MASK
		xori	$28, _THREAD_MASK
#ifdef CONFIG_CPU_CAVIUM_OCTEON
		.set    mips64
		pref    0, 0($28)       
		pref    0, PT_R31(sp)   
		jal     octeon_mult_save
		LONG_L  v1, 0($28)  
			 
		LONG_L  ra, PT_R31(sp)
		pref    0, 0(v1)    
#endif
		.set	pop
		.endm

		.macro	SAVE_ALL
		SAVE_SOME
		SAVE_AT
		SAVE_TEMP
		SAVE_STATIC
		.endm

		.macro	RESTORE_AT
		.set	push
		.set	noat
		LONG_L	$1,  PT_R1(sp)
		.set	pop
		.endm

		.macro	RESTORE_TEMP
#ifdef CONFIG_CPU_HAS_SMARTMIPS
		LONG_L	$24, PT_ACX(sp)
		mtlhx	$24
		LONG_L	$24, PT_HI(sp)
		mtlhx	$24
		LONG_L	$24, PT_LO(sp)
		mtlhx	$24
#else
		LONG_L	$24, PT_LO(sp)
		mtlo	$24
		LONG_L	$24, PT_HI(sp)
		mthi	$24
#endif
#ifdef CONFIG_32BIT
		LONG_L	$8, PT_R8(sp)
		LONG_L	$9, PT_R9(sp)
#endif
		LONG_L	$10, PT_R10(sp)
		LONG_L	$11, PT_R11(sp)
		LONG_L	$12, PT_R12(sp)
		LONG_L	$13, PT_R13(sp)
		LONG_L	$14, PT_R14(sp)
		LONG_L	$15, PT_R15(sp)
		LONG_L	$24, PT_R24(sp)
		.endm

		.macro	RESTORE_STATIC
		LONG_L	$16, PT_R16(sp)
		LONG_L	$17, PT_R17(sp)
		LONG_L	$18, PT_R18(sp)
		LONG_L	$19, PT_R19(sp)
		LONG_L	$20, PT_R20(sp)
		LONG_L	$21, PT_R21(sp)
		LONG_L	$22, PT_R22(sp)
		LONG_L	$23, PT_R23(sp)
		LONG_L	$30, PT_R30(sp)
		.endm

#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)

		.macro	RESTORE_SOME
		.set	push
		.set	reorder
		.set	noat
		mfc0	a0, CP0_STATUS
		li	v1, 0xff00
		ori	a0, STATMASK
		xori	a0, STATMASK
		mtc0	a0, CP0_STATUS
		and	a0, v1
		LONG_L	v0, PT_STATUS(sp)
		nor	v1, $0, v1
		and	v0, v1
		or	v0, a0
		mtc0	v0, CP0_STATUS
		LONG_L	$31, PT_R31(sp)
		LONG_L	$28, PT_R28(sp)
		LONG_L	$25, PT_R25(sp)
		LONG_L	$7,  PT_R7(sp)
		LONG_L	$6,  PT_R6(sp)
		LONG_L	$5,  PT_R5(sp)
		LONG_L	$4,  PT_R4(sp)
		LONG_L	$3,  PT_R3(sp)
		LONG_L	$2,  PT_R2(sp)
		.set	pop
		.endm

		.macro	RESTORE_SP_AND_RET
		.set	push
		.set	noreorder
		LONG_L	k0, PT_EPC(sp)
		LONG_L	sp, PT_R29(sp)
		jr	k0
		 rfe
		.set	pop
		.endm

#else
		.macro	RESTORE_SOME
		.set	push
		.set	reorder
		.set	noat
#ifdef CONFIG_MIPS_MT_SMTC
		.set	mips32r2
		mfc0	v0, CP0_TCSTATUS
		ori	v0, TCSTATUS_IXMT
		mtc0	v0, CP0_TCSTATUS
		_ehb
		DVPE	5				# dvpe a1
		jal	mips_ihb
#endif 
#ifdef CONFIG_CPU_CAVIUM_OCTEON
		
		jal	octeon_mult_restore
#endif
		mfc0	a0, CP0_STATUS
		ori	a0, STATMASK
		xori	a0, STATMASK
		mtc0	a0, CP0_STATUS
		li	v1, 0xff00
		and	a0, v1
		LONG_L	v0, PT_STATUS(sp)
		nor	v1, $0, v1
		and	v0, v1
		or	v0, a0
		mtc0	v0, CP0_STATUS
#ifdef CONFIG_MIPS_MT_SMTC
		LONG_L	v1, PT_TCSTATUS(sp)
		_ehb
		mfc0	a0, CP0_TCSTATUS
		andi	v1, TCSTATUS_IXMT
		bnez	v1, 0f

		mfc0	v0, CP0_TCCONTEXT
		la	a2, IPIQ
		srl	v0, v0, 16
		addu	a2, a2, v0
		LONG_L	v0, 0(a2)
		beqz	v0, 0f
		mfc0	v0, CP0_CAUSE
		ori	v0, v0, C_SW1
		mtc0	v0, CP0_CAUSE
0:
		andi	a1, a1, MVPCONTROL_EVP
		beqz	a1, 1f
		evpe
1:
		
		xori	a0, a0, TCSTATUS_IXMT
		or	a0, a0, v1
		mtc0	a0, CP0_TCSTATUS
		_ehb

		.set	mips0
#endif 
		LONG_L	v1, PT_EPC(sp)
		MTC0	v1, CP0_EPC
		LONG_L	$31, PT_R31(sp)
		LONG_L	$28, PT_R28(sp)
		LONG_L	$25, PT_R25(sp)
#ifdef CONFIG_64BIT
		LONG_L	$8, PT_R8(sp)
		LONG_L	$9, PT_R9(sp)
#endif
		LONG_L	$7,  PT_R7(sp)
		LONG_L	$6,  PT_R6(sp)
		LONG_L	$5,  PT_R5(sp)
		LONG_L	$4,  PT_R4(sp)
		LONG_L	$3,  PT_R3(sp)
		LONG_L	$2,  PT_R2(sp)
		.set	pop
		.endm

		.macro	RESTORE_SP_AND_RET
		LONG_L	sp, PT_R29(sp)
		.set	mips3
		eret
		.set	mips0
		.endm

#endif

		.macro	RESTORE_SP
		LONG_L	sp, PT_R29(sp)
		.endm

		.macro	RESTORE_ALL
		RESTORE_TEMP
		RESTORE_STATIC
		RESTORE_AT
		RESTORE_SOME
		RESTORE_SP
		.endm

		.macro	RESTORE_ALL_AND_RET
		RESTORE_TEMP
		RESTORE_STATIC
		RESTORE_AT
		RESTORE_SOME
		RESTORE_SP_AND_RET
		.endm

		.macro	CLI
#if !defined(CONFIG_MIPS_MT_SMTC)
		mfc0	t0, CP0_STATUS
		li	t1, ST0_CU0 | STATMASK
		or	t0, t1
		xori	t0, STATMASK
		mtc0	t0, CP0_STATUS
#else 
		mfc0	t0, CP0_TCSTATUS
		
		
		li	t1, ST0_CU0 | 0x08001c00
		or	t0, t1
		
		xori	t0, 0x00001800
		mtc0	t0, CP0_TCSTATUS
		_ehb
		
		mfc0	t0, CP0_STATUS
		ori	t0, ST0_EXL | ST0_ERL
		xori	t0, ST0_EXL | ST0_ERL
		mtc0	t0, CP0_STATUS
#endif 
		irq_disable_hazard
		.endm

		.macro	STI
#if !defined(CONFIG_MIPS_MT_SMTC)
		mfc0	t0, CP0_STATUS
		li	t1, ST0_CU0 | STATMASK
		or	t0, t1
		xori	t0, STATMASK & ~1
		mtc0	t0, CP0_STATUS
#else 
		_ehb
		mfc0	t0, CP0_TCSTATUS
		
		
		li	t1, ST0_CU0 | 0x08001c00
		or	t0, t1
		
		xori	t0, 0x00001c00
		mtc0	t0, CP0_TCSTATUS
		_ehb
		
		mfc0	t0, CP0_STATUS
		ori	t0, ST0_EXL
		xori	t0, ST0_EXL
		mtc0	t0, CP0_STATUS
		
#endif 
		irq_enable_hazard
		.endm

		.macro	KMODE
#ifdef CONFIG_MIPS_MT_SMTC

		
		.set	push
		.set	mips32r2
		.set	noreorder
		mfc0	v0, CP0_TCSTATUS
		andi	v1, v0, TCSTATUS_IXMT
		ori	v0, TCSTATUS_IXMT
		mtc0	v0, CP0_TCSTATUS
		_ehb
		DMT	2				# dmt	v0
		move	t0, ra
		jal	mips_ihb
		nop	
		move	ra, t0
#endif 
		mfc0	t0, CP0_STATUS
		li	t1, ST0_CU0 | (STATMASK & ~1)
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)
		andi	t2, t0, ST0_IEP
		srl	t2, 2
		or	t0, t2
#endif
		or	t0, t1
		xori	t0, STATMASK & ~1
		mtc0	t0, CP0_STATUS
#ifdef CONFIG_MIPS_MT_SMTC
		_ehb
		andi	v0, v0, VPECONTROL_TE
		beqz	v0, 2f
		nop	
		emt
2:
		mfc0	v0, CP0_TCSTATUS
		
		ori	v0, TCSTATUS_IXMT
		xori	v0, TCSTATUS_IXMT
		or	v0, v1, v0
		mtc0	v0, CP0_TCSTATUS
		.set pop
#endif 
		irq_disable_hazard
		.endm

#endif 
