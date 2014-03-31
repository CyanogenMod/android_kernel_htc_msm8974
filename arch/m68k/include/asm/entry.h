#ifndef __M68K_ENTRY_H
#define __M68K_ENTRY_H

#include <asm/setup.h>
#include <asm/page.h>
#ifdef __ASSEMBLY__
#include <asm/thread_info.h>
#endif


#if defined(MACH_ATARI_ONLY)
	
#define ALLOWINT	(~0x400)
#define	MAX_NOINT_IPL	3
#else
	
#define ALLOWINT	(~0x700)
#define	MAX_NOINT_IPL	0
#endif 

#ifdef __ASSEMBLY__
#define SWITCH_STACK_SIZE	(6*4+4)	

#ifdef CONFIG_COLDFIRE
#ifdef CONFIG_COLDFIRE_SW_A7
.globl sw_usp
.globl sw_ksp

.macro SAVE_ALL_SYS
	move	#0x2700,%sr		
	btst	#5,%sp@(2)		
	bnes	6f			
	movel	%sp,sw_usp		
	addql	#8,sw_usp		
	movel	sw_ksp,%sp		
	subql	#8,%sp			
	clrl	%sp@-			
	movel	%d0,%sp@-		
	movel	%d0,%sp@-		
	lea	%sp@(-32),%sp		
	moveml	%d1-%d5/%a0-%a2,%sp@
	movel	sw_usp,%a0		
	movel	%a0@-,%sp@(PT_OFF_PC)	
	movel	%a0@-,%sp@(PT_OFF_FORMATVEC)
	bra	7f
	6:
	clrl	%sp@-			
	movel	%d0,%sp@-		
	movel	%d0,%sp@-		
	lea	%sp@(-32),%sp		
	moveml	%d1-%d5/%a0-%a2,%sp@
	7:
.endm

.macro SAVE_ALL_INT
	SAVE_ALL_SYS
	moveq	#-1,%d0			
	movel	%d0,%sp@(PT_OFF_ORIG_D0)
.endm

.macro RESTORE_USER
	move	#0x2700,%sr		
	movel	sw_usp,%a0		
	movel	%sp@(PT_OFF_PC),%a0@-	
	movel	%sp@(PT_OFF_FORMATVEC),%a0@-
	moveml	%sp@,%d1-%d5/%a0-%a2
	lea	%sp@(32),%sp		
	movel	%sp@+,%d0
	addql	#4,%sp			
	addl	%sp@+,%sp		
	addql	#8,%sp			
	movel	%sp,sw_ksp		
	subql	#8,sw_usp		
	movel	sw_usp,%sp		
	rte
.endm

.macro RDUSP
	movel	sw_usp,%a3
.endm

.macro WRUSP
	movel	%a3,sw_usp
.endm

#else 
.macro SAVE_ALL_SYS
	move	#0x2700,%sr		
	clrl	%sp@-			
	movel	%d0,%sp@-		
	movel	%d0,%sp@-		
	lea	%sp@(-32),%sp		
	moveml	%d1-%d5/%a0-%a2,%sp@
.endm

.macro SAVE_ALL_INT
	move	#0x2700,%sr		
	clrl	%sp@-			
	pea	-1:w			
	movel	%d0,%sp@-		
	lea	%sp@(-32),%sp		
	moveml	%d1-%d5/%a0-%a2,%sp@
.endm

.macro RESTORE_USER
	moveml	%sp@,%d1-%d5/%a0-%a2
	lea	%sp@(32),%sp		
	movel	%sp@+,%d0
	addql	#4,%sp			
	addl	%sp@+,%sp		
	rte
.endm

.macro RDUSP
	
	.word	0x4e6b
.endm

.macro WRUSP
	
	.word	0x4e63
.endm

#endif 

.macro SAVE_SWITCH_STACK
	lea	%sp@(-24),%sp		
	moveml	%a3-%a6/%d6-%d7,%sp@
.endm

.macro RESTORE_SWITCH_STACK
	moveml	%sp@,%a3-%a6/%d6-%d7
	lea	%sp@(24),%sp		
.endm

#else 


.macro SAVE_ALL_INT
	clrl	%sp@-			
	pea	-1:w			
	movel	%d0,%sp@-		
	moveml	%d1-%d5/%a0-%a2,%sp@-
.endm

.macro SAVE_ALL_SYS
	clrl	%sp@-			
	movel	%d0,%sp@-		
	movel	%d0,%sp@-		
	moveml	%d1-%d5/%a0-%a2,%sp@-
.endm

.macro RESTORE_ALL
	moveml	%sp@+,%a0-%a2/%d1-%d5
	movel	%sp@+,%d0
	addql	#4,%sp			
	addl	%sp@+,%sp		
	rte
.endm


.macro SAVE_SWITCH_STACK
	moveml	%a3-%a6/%d6-%d7,%sp@-
.endm

.macro RESTORE_SWITCH_STACK
	moveml	%sp@+,%a3-%a6/%d6-%d7
.endm

#endif 

#ifdef CONFIG_MMU

#define curptr a2

#define GET_CURRENT(tmp) get_current tmp
.macro get_current reg=%d0
	movel	%sp,\reg
	andl	#-THREAD_SIZE,\reg
	movel	\reg,%curptr
	movel	%curptr@,%curptr
.endm

#else

#define GET_CURRENT(tmp)

#endif 

#else 

#define STR(X) STR1(X)
#define STR1(X) #X

#define SAVE_ALL_INT				\
	"clrl	%%sp@-;"    	\
	"pea	-1:w;"	    	\
	"movel	%%d0,%%sp@-;" 		\
	"moveml	%%d1-%%d5/%%a0-%%a2,%%sp@-"

#define GET_CURRENT(tmp) \
	"movel	%%sp,"#tmp"\n\t" \
	"andw	#-"STR(THREAD_SIZE)","#tmp"\n\t" \
	"movel	"#tmp",%%a2\n\t" \
	"movel	%%a2@,%%a2"

#endif

#endif 
