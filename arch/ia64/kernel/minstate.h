
#include <asm/cache.h>

#include "entry.h"
#include "paravirt_inst.h"

#ifdef CONFIG_VIRT_CPU_ACCOUNTING
#define ACCOUNT_GET_STAMP				\
(pUStk) mov.m r20=ar.itc;
#define ACCOUNT_SYS_ENTER				\
(pUStk) br.call.spnt rp=account_sys_enter		\
	;;
#else
#define ACCOUNT_GET_STAMP
#define ACCOUNT_SYS_ENTER
#endif

.section ".data..patch.rse", "a"
.previous

#define IA64_NATIVE_DO_SAVE_MIN(__COVER,SAVE_IFS,EXTRA,WORKAROUND)				\
	mov r16=IA64_KR(CURRENT);								\
	mov r27=ar.rsc;										\
	mov r20=r1;										\
	mov r25=ar.unat;									\
	MOV_FROM_IPSR(p0,r29);									\
	mov r26=ar.pfs;										\
	MOV_FROM_IIP(r28);									\
	mov r21=ar.fpsr;									\
	__COVER;								\
	;;											\
	adds r16=IA64_TASK_THREAD_ON_USTACK_OFFSET,r16;						\
	;;											\
	ld1 r17=[r16];					\
	st1 [r16]=r0;					\
	adds r1=-IA64_TASK_THREAD_ON_USTACK_OFFSET,r16						\
								\
	;;											\
	invala;											\
	SAVE_IFS;										\
	cmp.eq pKStk,pUStk=r0,r17;				\
	;;											\
(pUStk)	mov ar.rsc=0;			\
	;;											\
(pUStk)	mov.m r24=ar.rnat;									\
(pUStk)	addl r22=IA64_RBS_OFFSET,r1;					\
(pKStk) mov r1=sp;									\
	;;											\
(pUStk) lfetch.fault.excl.nt1 [r22];								\
(pUStk)	addl r1=IA64_STK_OFFSET-IA64_PT_REGS_SIZE,r1;		\
(pUStk)	mov r23=ar.bspstore;							\
	;;											\
(pUStk)	mov ar.bspstore=r22;						\
(pKStk) addl r1=-IA64_PT_REGS_SIZE,r1;				\
	;;											\
(pUStk)	mov r18=ar.bsp;										\
(pUStk)	mov ar.rsc=0x3;				\
	adds r17=2*L1_CACHE_BYTES,r1;				\
	adds r16=PT(CR_IPSR),r1;								\
	;;											\
	lfetch.fault.excl.nt1 [r17],L1_CACHE_BYTES;						\
	st8 [r16]=r29;								\
	;;											\
	lfetch.fault.excl.nt1 [r17];								\
	tbit.nz p15,p0=r29,IA64_PSR_I_BIT;							\
	mov r29=b0										\
	;;											\
	WORKAROUND;										\
	adds r16=PT(R8),r1;					\
	adds r17=PT(R9),r1;					\
(pKStk)	mov r18=r0;							\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r8,16;								\
.mem.offset 8,0; st8.spill [r17]=r9,16;								\
        ;;											\
.mem.offset 0,0; st8.spill [r16]=r10,24;							\
.mem.offset 8,0; st8.spill [r17]=r11,24;							\
        ;;											\
	st8 [r16]=r28,16;							\
	st8 [r17]=r30,16;							\
(pUStk)	sub r18=r18,r22;							\
	mov r8=ar.ccv;										\
	mov r9=ar.csd;										\
	mov r10=ar.ssd;										\
	movl r11=FPSR_DEFAULT;   							\
	;;											\
	st8 [r16]=r25,16;							\
	st8 [r17]=r26,16;							\
	shl r18=r18,16;					\
	;;											\
	st8 [r16]=r27,16;							\
(pUStk)	st8 [r17]=r24,16;							\
(pKStk)	adds r17=16,r17;						\
	;;								\
(pUStk)	st8 [r16]=r23,16;							\
	st8 [r17]=r31,16;							\
(pKStk)	adds r16=16,r16;					\
	;;											\
	st8 [r16]=r29,16;								\
	st8 [r17]=r18,16;					\
	cmp.eq pNonSys,pSys=r0,r0				\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r20,16;					\
.mem.offset 8,0; st8.spill [r17]=r12,16;							\
	adds r12=-16,r1;		\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r13,16;							\
.mem.offset 8,0; st8.spill [r17]=r21,16;					\
	mov r13=IA64_KR(CURRENT);					\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r15,16;							\
.mem.offset 8,0; st8.spill [r17]=r14,16;							\
	;;											\
.mem.offset 0,0; st8.spill [r16]=r2,16;								\
.mem.offset 8,0; st8.spill [r17]=r3,16;								\
	ACCOUNT_GET_STAMP									\
	adds r2=IA64_PT_REGS_R16_OFFSET,r1;							\
	;;											\
	EXTRA;											\
	movl r1=__gp;						\
	;;											\
	ACCOUNT_SYS_ENTER									\
	bsw.1;				\
	;;

#define SAVE_REST				\
.mem.offset 0,0; st8.spill [r2]=r16,16;		\
.mem.offset 8,0; st8.spill [r3]=r17,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r18,16;		\
.mem.offset 8,0; st8.spill [r3]=r19,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r20,16;		\
.mem.offset 8,0; st8.spill [r3]=r21,16;		\
	mov r18=b6;				\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r22,16;		\
.mem.offset 8,0; st8.spill [r3]=r23,16;		\
	mov r19=b7;				\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r24,16;		\
.mem.offset 8,0; st8.spill [r3]=r25,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r26,16;		\
.mem.offset 8,0; st8.spill [r3]=r27,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r28,16;		\
.mem.offset 8,0; st8.spill [r3]=r29,16;		\
	;;					\
.mem.offset 0,0; st8.spill [r2]=r30,16;		\
.mem.offset 8,0; st8.spill [r3]=r31,32;		\
	;;					\
	mov ar.fpsr=r11;		\
	st8 [r2]=r8,8;			\
	adds r24=PT(B6)-PT(F7),r3;		\
	;;					\
	stf.spill [r2]=f6,32;			\
	stf.spill [r3]=f7,32;			\
	;;					\
	stf.spill [r2]=f8,32;			\
	stf.spill [r3]=f9,32;			\
	;;					\
	stf.spill [r2]=f10;			\
	stf.spill [r3]=f11;			\
	adds r25=PT(B7)-PT(F11),r3;		\
	;;					\
	st8 [r24]=r18,16;       	\
	st8 [r25]=r19,16;       	\
	;;					\
	st8 [r24]=r9;        		\
	st8 [r25]=r10;      		\
	;;

#define RSE_WORKAROUND				\
(pUStk) extr.u r17=r18,3,6;			\
(pUStk)	sub r16=r18,r22;			\
[1:](pKStk)	br.cond.sptk.many 1f;		\
	.xdata4 ".data..patch.rse",1b-.		\
	;;					\
	cmp.ge p6,p7 = 33,r17;			\
	;;					\
(p6)	mov r17=0x310;				\
(p7)	mov r17=0x308;				\
	;;					\
	cmp.leu p1,p0=r16,r17;			\
(p1)	br.cond.sptk.many 1f;			\
	dep.z r17=r26,0,62;			\
	movl r16=2f;				\
	;;					\
	mov ar.pfs=r17;				\
	dep r27=r0,r27,16,14;			\
	mov b0=r16;				\
	;;					\
	br.ret.sptk b0;				\
	;;					\
2:						\
	mov ar.rsc=r0				\
	;;					\
	flushrs;				\
	;;					\
	mov ar.bspstore=r22			\
	;;					\
	mov r18=ar.bsp;				\
	;;					\
1:						\
	.pred.rel "mutex", pKStk, pUStk

#define SAVE_MIN_WITH_COVER	DO_SAVE_MIN(COVER, mov r30=cr.ifs, , RSE_WORKAROUND)
#define SAVE_MIN_WITH_COVER_R19	DO_SAVE_MIN(COVER, mov r30=cr.ifs, mov r15=r19, RSE_WORKAROUND)
#define SAVE_MIN			DO_SAVE_MIN(     , mov r30=r0, , )
