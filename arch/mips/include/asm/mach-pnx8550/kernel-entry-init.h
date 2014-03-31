/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005 Embedded Alley Solutions, Inc
 */
#ifndef __ASM_MACH_KERNEL_ENTRY_INIT_H
#define __ASM_MACH_KERNEL_ENTRY_INIT_H

#include <asm/cacheops.h>
#include <asm/addrspace.h>

#define CO_CONFIGPR_VALID  0x3F1F41FF    
#define HAZARD_CP0 nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;
#define CACHE_OPC      0xBC000000  
#define ICACHE_LINE_SIZE        32      
#define DCACHE_LINE_SIZE        32      

#define ICACHE_SET_COUNT        256     
#define DCACHE_SET_COUNT        128     

#define ICACHE_SET_SIZE         (ICACHE_SET_COUNT * ICACHE_LINE_SIZE)
#define DCACHE_SET_SIZE         (DCACHE_SET_COUNT * DCACHE_LINE_SIZE)

	.macro	kernel_entry_setup
	.set	push
	.set	noreorder
cache_begin:	li	t0, (1<<28)
	mtc0	t0, CP0_STATUS 
	HAZARD_CP0

	mtc0 	zero, CP0_CAUSE
	HAZARD_CP0


	
	mfc0 	t0, CP0_CONFIG, 7
	HAZARD_CP0

	and	t0, ~((1<<19) | (1<<20))     
	mtc0	t0, CP0_CONFIG, 7
	HAZARD_CP0

	

	init_icache
	nop
	init_dcache
	nop

	cachePr4450ICReset
	nop

	cachePr4450DCReset
	nop

	
	mfc0	t0, CP0_CONFIG, 7
	HAZARD_CP0

	
	or      t0, (1<<19)

	
	

	
	

	and	t0, CO_CONFIGPR_VALID

	
	mtc0	t0, CP0_CONFIG, 7
	HAZARD_CP0
cache_end:
	
	lui    t0, 0x1BE0
	addi   t0, t0, 0x3
	mtc0   $8, $22, 4
	nop

	
	lui    t0, 0x1000
	addi   t0, t0, 0xf
	mtc0   $8, $22, 5
	nop


	
	lui    t0, 0x1C00
	addi   t0, t0, 0xb
	mtc0   $8, $22, 6
	nop

	
	lui    t0, 0x0
	addi   t0, t0, 0x0
	mtc0   $8, $22, 7
	nop

	
	mfc0	t0, CP0_CONFIG
	HAZARD_CP0
	and	t0, t0, 0xFFFFFFF8
	or	t0, t0, 3
	mtc0	t0, CP0_CONFIG
	HAZARD_CP0
	.set	pop
	.endm

	.macro	init_icache
	.set	push
	.set	noreorder

	
	mfc0	t3, CP0_CONFIG, 1
	HAZARD_CP0

	

	srl   t1, t3, 19   
	andi  t1, t1, 0x7  
	beq   t1, zero, pr4450_instr_cache_invalidated 
	nop
	addiu t0, t1, 1
	ori   t1, zero, 1
	sllv  t1, t1, t0

	
	srl   t2, t3, 22  
	andi  t2, t2, 0x7 
	addiu t0, t2, 6
	ori   t2, zero, 1
	sllv  t2, t2, t0

	
	srl   t3, t3, 16  
	andi  t3, t3, 0x7 
	addiu t3, t3, 1

	
	multu t2, t3             
	mflo  t2
	addiu t2, t2, -1

	move  t0, zero
pr4450_next_instruction_cache_set:
	cache  Index_Invalidate_I, 0(t0)
	addu  t0, t0, t1         
	bne   t2, zero, pr4450_next_instruction_cache_set
	addiu t2, t2, -1   
pr4450_instr_cache_invalidated:
	.set	pop
	.endm

	.macro	init_dcache
	.set	push
	.set	noreorder
	move t1, zero

	
	mtc0	zero, CP0_TAGLO, 0
	HAZARD_CP0

	mtc0	zero, CP0_TAGHI, 0
	HAZARD_CP0

	
	or       t2, zero, (128*4)-1  
	
2:
	cache Index_Store_Tag_D, 0(t1)
	addiu    t2, t2, -1
	bne      t2, zero, 2b
	addiu    t1, t1, 32        
	.set pop
	.endm

	.macro	cachePr4450ICReset
	.set	push
	.set	noreorder

	
	
	mfc0    t0, CP0_STATUS      
	HAZARD_CP0

	mtc0    zero, CP0_STATUS   
	HAZARD_CP0

	or      t1, zero, zero              
	ori     t2, zero, (256 - 1) 

	icache_invd_loop:
	
	.word   CACHE_OPC | (9 << 21) | (Index_Invalidate_I << 16) | \
		(0 * ICACHE_SET_SIZE)  
	.word   CACHE_OPC | (9 << 21) | (Index_Invalidate_I << 16) | \
		(1 * ICACHE_SET_SIZE)  

	addiu   t1, t1, ICACHE_LINE_SIZE    
	bne     t2, zero, icache_invd_loop 
	addiu   t2, t2, -1        

	
	
	
	
	la      t1, KSEG0            
	lw      zero, 0x0000(t1)      

	mtc0    t0, CP0_STATUS        
	HAZARD_CP0
	.set	pop
	.endm

	.macro	cachePr4450DCReset
	.set	push
	.set	noreorder
	mfc0    t0, CP0_STATUS           
	HAZARD_CP0
	mtc0    zero, CP0_STATUS         
	HAZARD_CP0

	
	or      t1, zero, zero              
	ori     t2, zero, (DCACHE_SET_COUNT - 1) 

	dcache_wbinvd_loop:
	
	.word   CACHE_OPC | (9 << 21) | (Index_Writeback_Inv_D << 16) | \
		(0 * DCACHE_SET_SIZE)  
	.word   CACHE_OPC | (9 << 21) | (Index_Writeback_Inv_D << 16) | \
		(1 * DCACHE_SET_SIZE)  
	.word   CACHE_OPC | (9 << 21) | (Index_Writeback_Inv_D << 16) | \
		(2 * DCACHE_SET_SIZE)  
	.word   CACHE_OPC | (9 << 21) | (Index_Writeback_Inv_D << 16) | \
		(3 * DCACHE_SET_SIZE)  

	addiu   t1, t1, DCACHE_LINE_SIZE  
	bne     t2, zero, dcache_wbinvd_loop 
	addiu   t2, t2, -1          

	la      t1, KSEG0            
	lw      zero, 0x0000(t1)      

	mtc0    t0, CP0_STATUS       
	HAZARD_CP0
	.set	pop
	.endm

#endif 
