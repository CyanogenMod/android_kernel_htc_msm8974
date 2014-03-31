#ifndef __ROMIMAGE_MACRO_H
#define __ROMIMAGE_MACRO_H

.macro	LIST comment
.endm

.macro  ED, addr, data
	mov.l 1f, r1
	mov.l 2f, r0
	mov.l r0, @r1
	bra 3f
	 nop
	.align 2
1 :	.long \addr
2 :	.long \data
3 :
.endm

.macro  EW, addr, data
	mov.l 1f, r1
	mov.l 2f, r0
	mov.w r0, @r1
	bra 3f
	 nop
	.align 2
1 :	.long \addr
2 :	.long \data
3 :
.endm

.macro  EB, addr, data
	mov.l 1f, r1
	mov.l 2f, r0
	mov.b r0, @r1
	bra 3f
	 nop
	.align 2
1 :	.long \addr
2 :	.long \data
3 :
.endm

.macro  WAIT, time
	mov.l  2f, r3
1 :
	nop
	tst     r3, r3
	bf/s    1b
	dt      r3
	bra	3f
	 nop
	.align 2
2 :	.long \time * 100
3 :
.endm

.macro  DD, addr, addr2, nr
	mov.l 1f, r1
	mov.l @r1, r0
	bra 2f
	 nop
	.align 2
1 :	.long \addr
2 :
.endm

#endif 
