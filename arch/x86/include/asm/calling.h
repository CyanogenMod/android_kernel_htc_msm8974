
#include "dwarf2.h"


#define R15		  (0)
#define R14		  (8)
#define R13		 (16)
#define R12		 (24)
#define RBP		 (32)
#define RBX		 (40)

#define R11		 (48)
#define R10		 (56)
#define R9		 (64)
#define R8		 (72)
#define RAX		 (80)
#define RCX		 (88)
#define RDX		 (96)
#define RSI		(104)
#define RDI		(112)
#define ORIG_RAX	(120)       

#define RIP		(128)
#define CS		(136)
#define EFLAGS		(144)
#define RSP		(152)
#define SS		(160)

#define ARGOFFSET	R11
#define SWFRAME		ORIG_RAX

	.macro SAVE_ARGS addskip=0, save_rcx=1, save_r891011=1
	subq  $9*8+\addskip, %rsp
	CFI_ADJUST_CFA_OFFSET	9*8+\addskip
	movq_cfi rdi, 8*8
	movq_cfi rsi, 7*8
	movq_cfi rdx, 6*8

	.if \save_rcx
	movq_cfi rcx, 5*8
	.endif

	movq_cfi rax, 4*8

	.if \save_r891011
	movq_cfi r8,  3*8
	movq_cfi r9,  2*8
	movq_cfi r10, 1*8
	movq_cfi r11, 0*8
	.endif

	.endm

#define ARG_SKIP	(9*8)

	.macro RESTORE_ARGS rstor_rax=1, addskip=0, rstor_rcx=1, rstor_r11=1, \
			    rstor_r8910=1, rstor_rdx=1
	.if \rstor_r11
	movq_cfi_restore 0*8, r11
	.endif

	.if \rstor_r8910
	movq_cfi_restore 1*8, r10
	movq_cfi_restore 2*8, r9
	movq_cfi_restore 3*8, r8
	.endif

	.if \rstor_rax
	movq_cfi_restore 4*8, rax
	.endif

	.if \rstor_rcx
	movq_cfi_restore 5*8, rcx
	.endif

	.if \rstor_rdx
	movq_cfi_restore 6*8, rdx
	.endif

	movq_cfi_restore 7*8, rsi
	movq_cfi_restore 8*8, rdi

	.if ARG_SKIP+\addskip > 0
	addq $ARG_SKIP+\addskip, %rsp
	CFI_ADJUST_CFA_OFFSET	-(ARG_SKIP+\addskip)
	.endif
	.endm

	.macro LOAD_ARGS offset, skiprax=0
	movq \offset(%rsp),    %r11
	movq \offset+8(%rsp),  %r10
	movq \offset+16(%rsp), %r9
	movq \offset+24(%rsp), %r8
	movq \offset+40(%rsp), %rcx
	movq \offset+48(%rsp), %rdx
	movq \offset+56(%rsp), %rsi
	movq \offset+64(%rsp), %rdi
	.if \skiprax
	.else
	movq \offset+72(%rsp), %rax
	.endif
	.endm

#define REST_SKIP	(6*8)

	.macro SAVE_REST
	subq $REST_SKIP, %rsp
	CFI_ADJUST_CFA_OFFSET	REST_SKIP
	movq_cfi rbx, 5*8
	movq_cfi rbp, 4*8
	movq_cfi r12, 3*8
	movq_cfi r13, 2*8
	movq_cfi r14, 1*8
	movq_cfi r15, 0*8
	.endm

	.macro RESTORE_REST
	movq_cfi_restore 0*8, r15
	movq_cfi_restore 1*8, r14
	movq_cfi_restore 2*8, r13
	movq_cfi_restore 3*8, r12
	movq_cfi_restore 4*8, rbp
	movq_cfi_restore 5*8, rbx
	addq $REST_SKIP, %rsp
	CFI_ADJUST_CFA_OFFSET	-(REST_SKIP)
	.endm

	.macro SAVE_ALL
	SAVE_ARGS
	SAVE_REST
	.endm

	.macro RESTORE_ALL addskip=0
	RESTORE_REST
	RESTORE_ARGS 1, \addskip
	.endm

	.macro icebp
	.byte 0xf1
	.endm
