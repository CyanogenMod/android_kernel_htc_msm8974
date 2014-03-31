#ifndef __ZBOOT_MACRO_H
#define __ZBOOT_MACRO_H

.macro	LIST comment
.endm

.macro ED, addr, data
	LDR	r0, 1f
	LDR	r1, 2f
	STR	r1, [r0]
	B	3f
1 :	.long	\addr
2 :	.long	\data
3 :
.endm

.macro EW, addr, data
	LDR	r0, 1f
	LDR	r1, 2f
	STRH	r1, [r0]
	B	3f
1 :	.long	\addr
2 :	.long	\data
3 :
.endm

.macro EB, addr, data
	LDR	r0, 1f
	LDR	r1, 2f
	STRB	r1, [r0]
	B	3f
1 :	.long	\addr
2 :	.long	\data
3 :
.endm

.macro  WAIT, time, reg
	LDR	r1, 1f
	LDR	r0, 2f
	STR	r0, [r1]
10 :
	LDR	r0, [r1]
	CMP	r0, #0x00000000
	BNE	10b
	NOP
	B	3f
1 :	.long	\reg
2 :	.long	\time * 100
3 :
.endm

.macro  DD, start, end
	LDR	r1, 1f
	B	2f
1 :	.long	\start
2 :
.endm

#endif 
