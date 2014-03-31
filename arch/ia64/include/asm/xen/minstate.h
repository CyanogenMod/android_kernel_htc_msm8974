
#ifdef CONFIG_VIRT_CPU_ACCOUNTING
#define XEN_ACCOUNT_GET_STAMP		\
	MOV_FROM_ITC(pUStk, p6, r20, r2);
#else
#define XEN_ACCOUNT_GET_STAMP
#endif

#define XEN_DO_SAVE_MIN(__COVER,SAVE_IFS,EXTRA,WORKAROUND)					\
	mov r16=IA64_KR(CURRENT);								\
	mov r27=ar.rsc;										\
	mov r20=r1;										\
	mov r25=ar.unat;									\
	MOV_FROM_IPSR(p0,r29);									\
	MOV_FROM_IIP(r28);									\
	mov r21=ar.fpsr;									\
	mov r26=ar.pfs;										\
	__COVER;								\
	adds r16=IA64_TASK_THREAD_ON_USTACK_OFFSET,r16;						\
	;;											\
	ld1 r17=[r16];					\
	st1 [r16]=r0;					\
	adds r1=-IA64_TASK_THREAD_ON_USTACK_OFFSET,r16						\
								\
	;;											\
	invala;											\
	 					\
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
	movl r8=XSI_PRECOVER_IFS;								\
.mem.offset 8,0; st8.spill [r17]=r11,24;							\
        ;;											\
						\
					\
	ld8 r30=[r8];										\
(pUStk)	sub r18=r18,r22;							\
	st8 [r16]=r28,16;							\
	;;											\
	st8 [r17]=r30,16;							\
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
	XEN_ACCOUNT_GET_STAMP									\
	adds r2=IA64_PT_REGS_R16_OFFSET,r1;							\
	;;											\
	EXTRA;											\
	movl r1=__gp;						\
	;;											\
	ACCOUNT_SYS_ENTER									\
	BSW_1(r3,r14);			\
	;;
