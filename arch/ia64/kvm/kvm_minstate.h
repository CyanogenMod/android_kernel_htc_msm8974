/*
 *  kvm_minstate.h: min save macros
 *  Copyright (c) 2007, Intel Corporation.
 *
 *  Xuefei Xu (Anthony Xu) (Anthony.xu@intel.com)
 *  Xiantao Zhang (xiantao.zhang@intel.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 */


#include <asm/asmmacro.h>
#include <asm/types.h>
#include <asm/kregs.h>
#include <asm/kvm_host.h>

#include "asm-offsets.h"

#define KVM_MINSTATE_START_SAVE_MIN	     					\
	mov ar.rsc = 0;\
	;;									\
	mov.m r28 = ar.rnat;                                  			\
	addl r22 = VMM_RBS_OFFSET,r1;            	\
	;;									\
	lfetch.fault.excl.nt1 [r22];						\
	addl r1 = KVM_STK_OFFSET-VMM_PT_REGS_SIZE, r1;  \
	mov r23 = ar.bspstore;			          \
	;;									\
	mov ar.bspstore = r22;				\
	;;									\
	mov r18 = ar.bsp;							\
	mov ar.rsc = 0x3;     



#define KVM_MINSTATE_END_SAVE_MIN						\
	bsw.1;          \
	;;


#define PAL_VSA_SYNC_READ						\
					\
{.mii;									\
	add r25 = VMM_VPD_BASE_OFFSET, r21;				\
	nop 0x0;							\
	mov r24=ip;							\
	;;								\
}									\
{.mmb									\
	add r24=0x20, r24;						\
	ld8 r25 = [r25];      			\
	br.cond.sptk kvm_vps_sync_read;			\
	;;								\
};									\


#define KVM_MINSTATE_GET_CURRENT(reg)   mov reg=r21



#define PT(f) (VMM_PT_REGS_##f##_OFFSET)

#define KVM_DO_SAVE_MIN(COVER,SAVE_IFS,EXTRA)			\
	KVM_MINSTATE_GET_CURRENT(r16);  	\
	mov r27 = ar.rsc;         			\
	mov r20 = r1;         				\
	mov r25 = ar.unat;        			\
	mov r29 = cr.ipsr;        			\
	mov r26 = ar.pfs;         			\
	mov r18 = cr.isr;         				\
	COVER;              		\
	;;							\
	tbit.z p0,p15 = r29,IA64_PSR_I_BIT;			\
	mov r1 = r16;						\
					\
				\
	;;							\
	invala;             				\
	SAVE_IFS;						\
	;;							\
	KVM_MINSTATE_START_SAVE_MIN				\
	adds r17 = 2*L1_CACHE_BYTES,r1;	\
	adds r16 = PT(CR_IPSR),r1;				\
	;;							\
	lfetch.fault.excl.nt1 [r17],L1_CACHE_BYTES;		\
	st8 [r16] = r29;      		\
	;;							\
	lfetch.fault.excl.nt1 [r17];				\
	tbit.nz p15,p0 = r29,IA64_PSR_I_BIT;			\
	mov r29 = b0						\
	;;							\
	adds r16 = PT(R8),r1; \
	adds r17 = PT(R9),r1; \
	;;							\
.mem.offset 0,0; st8.spill [r16] = r8,16;			\
.mem.offset 8,0; st8.spill [r17] = r9,16;			\
	;;							\
.mem.offset 0,0; st8.spill [r16] = r10,24;			\
.mem.offset 8,0; st8.spill [r17] = r11,24;			\
	;;							\
	mov r9 = cr.iip;         			\
	mov r10 = ar.fpsr;        			\
	;;							\
	st8 [r16] = r9,16;    			\
	st8 [r17] = r30,16;   			\
	sub r18 = r18,r22;    		\
	;;							\
	st8 [r16] = r25,16;   		\
	st8 [r17] = r26,16;    		\
	shl r18 = r18,16;     \
	;;							\
	st8 [r16] = r27,16;   			\
	st8 [r17] = r28,16;   		\
	;;          		\
	st8 [r16] = r23,16;   		\
	st8 [r17] = r31,16;   		\
	;;							\
	st8 [r16] = r29,16;   			\
	st8 [r17] = r18,16;   \
	;;							\
.mem.offset 0,0; st8.spill [r16] = r20,16;  \
.mem.offset 8,0; st8.spill [r17] = r12,16;			\
	adds r12 = -16,r1;      \
	;;							\
.mem.offset 0,0; st8.spill [r16] = r13,16;			\
.mem.offset 8,0; st8.spill [r17] = r10,16;	\
	mov r13 = r21;   		\
	;;							\
.mem.offset 0,0; st8.spill [r16] = r15,16;			\
.mem.offset 8,0; st8.spill [r17] = r14,16;			\
	;;							\
.mem.offset 0,0; st8.spill [r16] = r2,16;			\
.mem.offset 8,0; st8.spill [r17] = r3,16;			\
	adds r2 = VMM_PT_REGS_R16_OFFSET,r1;			\
	 ;;							\
	adds r16 = VMM_VCPU_IIPA_OFFSET,r13;			\
	adds r17 = VMM_VCPU_ISR_OFFSET,r13;			\
	mov r26 = cr.iipa;					\
	mov r27 = cr.isr;					\
	;;							\
	st8 [r16] = r26;					\
	st8 [r17] = r27;					\
	;;							\
	EXTRA;							\
	mov r8 = ar.ccv;					\
	mov r9 = ar.csd;					\
	mov r10 = ar.ssd;					\
	movl r11 = FPSR_DEFAULT;   			\
	adds r17 = VMM_VCPU_GP_OFFSET,r13;			\
	;;							\
	ld8 r1 = [r17];	\
	;;							\
	PAL_VSA_SYNC_READ					\
	KVM_MINSTATE_END_SAVE_MIN

#define KVM_SAVE_REST				\
.mem.offset 0,0; st8.spill [r2] = r16,16;	\
.mem.offset 8,0; st8.spill [r3] = r17,16;	\
	;;				\
.mem.offset 0,0; st8.spill [r2] = r18,16;	\
.mem.offset 8,0; st8.spill [r3] = r19,16;	\
	;;				\
.mem.offset 0,0; st8.spill [r2] = r20,16;	\
.mem.offset 8,0; st8.spill [r3] = r21,16;	\
	mov r18=b6;			\
	;;				\
.mem.offset 0,0; st8.spill [r2] = r22,16;	\
.mem.offset 8,0; st8.spill [r3] = r23,16;	\
	mov r19 = b7;				\
	;;					\
.mem.offset 0,0; st8.spill [r2] = r24,16;	\
.mem.offset 8,0; st8.spill [r3] = r25,16;	\
	;;					\
.mem.offset 0,0; st8.spill [r2] = r26,16;	\
.mem.offset 8,0; st8.spill [r3] = r27,16;	\
	;;					\
.mem.offset 0,0; st8.spill [r2] = r28,16;	\
.mem.offset 8,0; st8.spill [r3] = r29,16;	\
	;;					\
.mem.offset 0,0; st8.spill [r2] = r30,16;	\
.mem.offset 8,0; st8.spill [r3] = r31,32;	\
	;;					\
	mov ar.fpsr = r11;			\
	st8 [r2] = r8,8;			\
	adds r24 = PT(B6)-PT(F7),r3;		\
	adds r25 = PT(B7)-PT(F7),r3;		\
	;;					\
	st8 [r24] = r18,16;       	\
	st8 [r25] = r19,16;       	\
	adds r2 = PT(R4)-PT(F6),r2;		\
	adds r3 = PT(R5)-PT(F7),r3;		\
	;;					\
	st8 [r24] = r9;			\
	st8 [r25] = r10;		\
	;;					\
	mov r18 = ar.unat;			\
	adds r19 = PT(EML_UNAT)-PT(R4),r2;	\
	;;					\
	st8 [r19] = r18;  	\


#define KVM_SAVE_EXTRA				\
.mem.offset 0,0; st8.spill [r2] = r4,16;	\
.mem.offset 8,0; st8.spill [r3] = r5,16;	\
	;;					\
.mem.offset 0,0; st8.spill [r2] = r6,16;	\
.mem.offset 8,0; st8.spill [r3] = r7;		\
	;;					\
	mov r26 = ar.unat;			\
	;;					\
	st8 [r2] = r26; 		\

#define KVM_SAVE_MIN_WITH_COVER		KVM_DO_SAVE_MIN(cover, mov r30 = cr.ifs,)
#define KVM_SAVE_MIN_WITH_COVER_R19	KVM_DO_SAVE_MIN(cover, mov r30 = cr.ifs, mov r15 = r19)
#define KVM_SAVE_MIN			KVM_DO_SAVE_MIN(     , mov r30 = r0, )
