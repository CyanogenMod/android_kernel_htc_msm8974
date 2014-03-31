/*
 *  The driver for the Cirrus Logic's Sound Fusion CS46XX based soundcards
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *
 * NOTE: comments are copy/paste from cwcemb80.lst 
 * provided by Tom Woller at Cirrus (my only
 * documentation about the SP OS running inside
 * the DSP) 
 */

#ifndef __CS46XX_DSP_TASK_TYPES_H__
#define __CS46XX_DSP_TASK_TYPES_H__

#include "cs46xx_dsp_scb_types.h"


#define		HFG_FIRST_EXECUTE_MODE			0x0001
#define		HFG_FIRST_EXECUTE_MODE_BIT		0
#define		HFG_CONTEXT_SWITCH_MODE			0x0002
#define		HFG_CONTEXT_SWITCH_MODE_BIT		1

#define MAX_FG_STACK_SIZE 	32			
#define MAX_MG_STACK_SIZE 	16
#define MAX_BG_STACK_SIZE 	9
#define MAX_HFG_STACK_SIZE	4

#define SLEEP_ACTIVE_INCREMENT		0		
#define STANDARD_ACTIVE_INCREMENT	1		
#define SUSPEND_ACTIVE_INCREMENT	2		

#define HOSTFLAGS_DISABLE_BG_SLEEP  0       

struct dsp_hf_save_area {
	u32	r10_save;
	u32	r54_save;
	u32	r98_save;

	___DSP_DUAL_16BIT_ALLOC(
	    status_save,
	    ind_save
	)

	___DSP_DUAL_16BIT_ALLOC(
	    rci1_save,
	    rci0_save
	)

	u32	r32_save;
	u32	r76_save;
	u32	rsd2_save;

       	___DSP_DUAL_16BIT_ALLOC(
	      rsi2_save,	  
	      rsa2Save
	)
	
};


struct dsp_tree_link {
	___DSP_DUAL_16BIT_ALLOC(
	
	    next_scb,
	
	    sub_ptr
	)
  
	___DSP_DUAL_16BIT_ALLOC(
	
	    entry_point, 
	
	    this_spb
	)
};


struct dsp_task_tree_data {
	___DSP_DUAL_16BIT_ALLOC(
	
	    tock_count_limit,
	
	    tock_count
	)

	___DSP_DUAL_16BIT_ALLOC(
	    active_tncrement,		
	
	    active_count
	)

        ___DSP_DUAL_16BIT_ALLOC(
	
	    active_bit,	    
	
	    active_task_flags_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    mem_upd_ptr,
	
	    link_upd_ptr
	)
  
	___DSP_DUAL_16BIT_ALLOC(
	
	    save_area,
	
	    data_stack_base_ptr
	)

};


struct dsp_interval_timer_data
{
	
	___DSP_DUAL_16BIT_ALLOC(
	     interval_timer_period,
	     itd_unused
	)

	
	___DSP_DUAL_16BIT_ALLOC(
	     num_FG_ticks_this_interval,        
	     num_intervals
	)
};


struct dsp_task_tree_context_block {
	___DSP_DUAL_16BIT_ALLOC(
	     stack1,
	     stack0
	)
	___DSP_DUAL_16BIT_ALLOC(
	     stack3,
	     stack2
	)
	___DSP_DUAL_16BIT_ALLOC(
	     stack5,
	     stack4
	)
	___DSP_DUAL_16BIT_ALLOC(
	     stack7,
	     stack6
	)
	___DSP_DUAL_16BIT_ALLOC(
	     stack9,
	     stack8
	)

	u32	  saverfe;					

	___DSP_DUAL_16BIT_ALLOC(
             reserved1,	
  	     stack_size
	)
	u32		saverba;	  
	u32		saverdc;
	u32		savers_config_23; 
	u32		savers_DMA23;	  
	u32		saversa0;
	u32		saversi0;
	u32		saversa1;
	u32		saversi1;
	u32		saversa3;
	u32		saversd0;
	u32		saversd1;
	u32		saversd3;
	u32		savers_config01;
	u32		savers_DMA01;
	u32		saveacc0hl;
	u32		saveacc1hl;
	u32		saveacc0xacc1x;
	u32		saveacc2hl;
	u32		saveacc3hl;
	u32		saveacc2xacc3x;
	u32		saveaux0hl;
	u32		saveaux1hl;
	u32		saveaux0xaux1x;
	u32		saveaux2hl;
	u32		saveaux3hl;
	u32		saveaux2xaux3x;
	u32		savershouthl;
	u32		savershoutxmacmode;
};
                

struct dsp_task_tree_control_block {
	struct dsp_hf_save_area			context;
	struct dsp_tree_link			links;
	struct dsp_task_tree_data		data;
	struct dsp_task_tree_context_block	context_blk;
	struct dsp_interval_timer_data		int_timer;
};


#endif 
