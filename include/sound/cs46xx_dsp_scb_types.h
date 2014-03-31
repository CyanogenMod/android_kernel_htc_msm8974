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

#ifndef __CS46XX_DSP_SCB_TYPES_H__
#define __CS46XX_DSP_SCB_TYPES_H__

#include <asm/byteorder.h>

#ifndef ___DSP_DUAL_16BIT_ALLOC
#if   defined(__LITTLE_ENDIAN)
#define ___DSP_DUAL_16BIT_ALLOC(a,b) u16 a; u16 b;
#elif defined(__BIG_ENDIAN)
#define ___DSP_DUAL_16BIT_ALLOC(a,b) u16 b; u16 a;
#else
#error Not __LITTLE_ENDIAN and not __BIG_ENDIAN, then what ???
#endif
#endif


struct dsp_basic_dma_req {
	u32 dcw;                 
	u32 dmw;                 
	u32 saw;                 
	u32 daw;                 
};

struct dsp_scatter_gather_ext {
	u32 npaw;                

	u32 npcw;                
	u32 lbaw;                
	u32 nplbaw;              
	u32 sgaw;                
};

struct dsp_volume_control {
	___DSP_DUAL_16BIT_ALLOC(
	   rightTarg,  
	   leftTarg
	)
	___DSP_DUAL_16BIT_ALLOC(
	   rightVol,   
	   leftVol
	)
};

struct dsp_generic_scb {
  
	struct dsp_basic_dma_req  basic_req;  

	struct dsp_scatter_gather_ext sg_ext;  

	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,     
	    sub_list_ptr  
	)
  
	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,  
	    this_spb      
	)

	u32  strm_rs_config; 
               
	u32  strm_buf_ptr; 

	u32  strmPhiIncr;


	struct dsp_volume_control vol_ctrl_t;   
};


struct dsp_spos_control_block {


	___DSP_DUAL_16BIT_ALLOC( 
	
	    hfg_tree_root_ptr,  			    
	/* First 3 dwords are written by the host and read-only on the DSP */
	    hfg_stack_base      
	)

	___DSP_DUAL_16BIT_ALLOC(
	
	    spos_cb_ptr,	 
	    prev_task_tree_ptr   
	)

	___DSP_DUAL_16BIT_ALLOC(
	
	    xxinterval_timer_period,
	
	    HFGSPB_ptr
	)


	___DSP_DUAL_16BIT_ALLOC(
	    xxnum_HFG_ticks_thisInterval,
	
	    xxnum_tntervals
	)


	___DSP_DUAL_16BIT_ALLOC(
	    spurious_int_flag,	 
	    trap_flag            
	)

	___DSP_DUAL_16BIT_ALLOC(
	    unused2,					
	    invalid_IP_flag        
	)

	___DSP_DUAL_16BIT_ALLOC(
	
	    fg_task_tree_hdr_ptr,	  		
	
	    hfg_sync_update_ptr           
	)
  
	___DSP_DUAL_16BIT_ALLOC(
	     begin_foreground_FCNT,  
	
	     last_FCNT_before_sleep  
	)

	___DSP_DUAL_16BIT_ALLOC(
	    unused7,           
	    next_task_treePtr  
	)

	u32 unused5;        

	___DSP_DUAL_16BIT_ALLOC(
	    active_flags,   
	
	    HFG_flags       
	)

	___DSP_DUAL_16BIT_ALLOC(
	    unused9,
	    unused8
	)
                              
	u32 rFE_save_for_invalid_IP;
	u32 r32_save_for_spurious_int;
	u32 r32_save_for_trap;
	u32 r32_save_for_HFG;
};

struct dsp_mix2_ostream_spb
{
	u32 outTripletsPerFrame;

	u32 accumOutTriplets;  
};

struct dsp_timing_master_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,     
	    sub_list_ptr  
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,  
	    this_spb      
	)

	___DSP_DUAL_16BIT_ALLOC(
	
 	    reserved,
	    extra_sample_accum
	)

  
	___DSP_DUAL_16BIT_ALLOC(
	    codec_FIFO_syncd, 
	    codec_FIFO_ptr
	)
  
	___DSP_DUAL_16BIT_ALLOC(
	    frac_samp_accum_qm1,
	    TM_frms_left_in_group
	) 

	___DSP_DUAL_16BIT_ALLOC(
	    frac_samp_correction_qm1,
	    TM_frm_group_length  
	)

	u32 nsamp_per_frm_q15;
};

struct dsp_codec_output_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,       
	    sub_list_ptr    
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,    
	    this_spb        
	)

	u32 strm_rs_config; 

	u32 strm_buf_ptr;   


	/* Init. 0000:0010 for SDout
                 0060:0010 for SDout2
		 0080:0010 for SDout3
	   hi: Base IO address of FIFO to which
	       the left-channel samples are to
	       be written.
	   lo: Displacement for the base IO
	       address for left-channel to obtain
	       the base IO address for the FIFO
	       to which the right-channel samples
	       are to be written.
	*/
	___DSP_DUAL_16BIT_ALLOC(
	    left_chan_base_IO_addr,
	    right_chan_IO_disp
	)


	___DSP_DUAL_16BIT_ALLOC(
	    CO_scale_shift_count, 
	    CO_exp_vol_change_rate
	)

	
	___DSP_DUAL_16BIT_ALLOC(
	    reserved,
	    last_sub_ptr
	)
};

struct dsp_codec_input_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,       
	    sub_list_ptr    
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,    
	    this_spb        
	)

	u32 strm_rs_config; 
	u32 strm_buf_ptr;   


	/* Init. 0000:0010 for SDout
                 0060:0010 for SDout2
		 0080:0010 for SDout3
	   hi: Base IO address of FIFO to which
	       the left-channel samples are to
	       be written.
	   lo: Displacement for the base IO
	       address for left-channel to obtain
	       the base IO address for the FIFO
	       to which the right-channel samples
	       are to be written.
	*/
	___DSP_DUAL_16BIT_ALLOC(
	    rightChanINdisp, 
	    left_chan_base_IN_addr
	)
	___DSP_DUAL_16BIT_ALLOC(
	    scaleShiftCount, 
	    reserver1
	)

	u32  reserved2;
};


struct dsp_pcm_serial_input_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,       
	    sub_list_ptr    
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,    
	    this_spb        
	)

	u32 strm_buf_ptr;   
	u32 strm_rs_config; 
  
	/* Init. Ptr to CODEC input SCB
	   hi: Pointer to the SCB containing the
	       input buffer to which CODEC input
	       samples are written
	   lo: Flag indicating the link to the CODEC
	       input task is to be initialized
	*/
	___DSP_DUAL_16BIT_ALLOC(
	    init_codec_input_link,
	    codec_input_buf_scb
	)

	
	struct dsp_volume_control psi_vol_ctrl;   
  
};

struct dsp_src_task_scb {
	___DSP_DUAL_16BIT_ALLOC(
	    frames_left_in_gof,
	    gofs_left_in_sec
	)

	___DSP_DUAL_16BIT_ALLOC(
	    const2_thirds,
	    num_extra_tnput_samples
	)

	___DSP_DUAL_16BIT_ALLOC(
	    cor_per_gof,
	    correction_per_sec 
	)

	___DSP_DUAL_16BIT_ALLOC(
	    output_buf_producer_ptr,  
	    junk_DMA_MID
	)

	___DSP_DUAL_16BIT_ALLOC(
	    gof_length,  
	    gofs_per_sec
	)

	u32 input_buf_strm_config;

	___DSP_DUAL_16BIT_ALLOC(
	    reserved_for_SRC_use,
	    input_buf_consumer_ptr
	)

	u32 accum_phi;

	___DSP_DUAL_16BIT_ALLOC(
	    exp_src_vol_change_rate,
	    input_buf_producer_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    src_next_scb,
	    src_sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    src_entry_point,
	    src_this_sbp
	)

	u32  src_strm_rs_config;
	u32  src_strm_buf_ptr;
  
	u32   phiIncr6int_26frac;
  
	struct dsp_volume_control src_vol_ctrl;
};

struct dsp_decimate_by_pow2_scb {
	___DSP_DUAL_16BIT_ALLOC(
	    dec2_coef_base_ptr,
	    dec2_coef_increment
	)

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_in_samples_per_out_triplet,
	    dec2_extra_in_samples
	)

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_const2_thirds,
	    dec2_half_num_taps_mp5
	)

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_output_buf_producer_ptr,
	    dec2_junkdma_mid
	)

	u32  dec2_reserved2;

	u32  dec2_input_nuf_strm_config;

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_phi_incr,
	    dec2_input_buf_consumer_ptr
	)

	u32 dec2_reserved3;

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_exp_vol_change_rate,
	    dec2_input_buf_producer_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_next_scb,
	    dec2_sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    dec2_entry_point,
	    dec2_this_spb
	)

	u32  dec2_strm_rs_config;
	u32  dec2_strm_buf_ptr;

	u32  dec2_reserved4;

	struct dsp_volume_control dec2_vol_ctrl; 
};

struct dsp_vari_decimate_scb {
	___DSP_DUAL_16BIT_ALLOC(
	    vdec_frames_left_in_gof,
	    vdec_gofs_left_in_sec
	)

	___DSP_DUAL_16BIT_ALLOC(
	    vdec_const2_thirds,
	    vdec_extra_in_samples
	)

	___DSP_DUAL_16BIT_ALLOC(
	    vdec_cor_per_gof,
	    vdec_correction_per_sec
	)

	___DSP_DUAL_16BIT_ALLOC(
	    vdec_output_buf_producer_ptr,
	    vdec_input_buf_consumer_ptr
	)
	
	___DSP_DUAL_16BIT_ALLOC(
	    vdec_gof_length,
	    vdec_gofs_per_sec
	)

	u32  vdec_input_buf_strm_config;
	u32  vdec_coef_increment;
	

	u32  vdec_accumphi;
	

	___DSP_DUAL_16BIT_ALLOC(
 	    vdec_exp_vol_change_rate,
	    vdec_input_buf_producer_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    vdec_next_scb,
	    vdec_sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    vdec_entry_point,
	    vdec_this_spb
	)

	u32 vdec_strm_rs_config;
	u32 vdec_strm_buf_ptr;

	u32 vdec_phi_incr_6int_26frac;

	struct dsp_volume_control vdec_vol_ctrl;
};


struct dsp_mix2_ostream_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,       
	    sub_list_ptr    
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,    
	    this_spb        
	)

	u32 strm_rs_config; 
	u32 strm_buf_ptr;   


	___DSP_DUAL_16BIT_ALLOC(
	    frames_left_in_group,
	    accum_input_triplets
	)

	___DSP_DUAL_16BIT_ALLOC(
	    frame_group_length,
	    exp_vol_change_rate
	)
  
	___DSP_DUAL_16BIT_ALLOC(
	    const_FFFF,
	    const_zero
	)
};


struct dsp_mix_only_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,       
	    sub_list_ptr    
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,    
	    this_spb        
	)

	u32 strm_rs_config; 
	u32 strm_buf_ptr;   

	u32 reserved;
	struct dsp_volume_control vol_ctrl;
};

struct dsp_async_codec_input_scb {
	u32 io_free2;     
  
	u32 io_current_total;
	u32 io_previous_total;
  
	u16 io_count;
	u16 io_count_limit;
  
	u16 o_fifo_base_addr;            
	u16 ost_mo_format;

	u32  ostrm_rs_config;
	u32  ostrm_buf_ptr;
  
	___DSP_DUAL_16BIT_ALLOC(
	    io_sclks_per_lr_clk,
	    io_io_enable
	)

	u32  io_free4;

	___DSP_DUAL_16BIT_ALLOC(  
	    io_next_scb,
	    io_sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    io_entry_point,
	    io_this_spb
	)

	u32 istrm_rs_config;
	u32 istrm_buf_ptr;

	___DSP_DUAL_16BIT_ALLOC(
	    io_stat_reg_addr,
	    iofifo_pointer
	)

	___DSP_DUAL_16BIT_ALLOC(
	    ififo_base_addr,            
	    ist_mo_format
	)

	u32 i_free;
};


struct dsp_spdifiscb {
	___DSP_DUAL_16BIT_ALLOC(
	    status_ptr,     
	    status_start_ptr
	)

	u32 current_total;
	u32 previous_total;

	___DSP_DUAL_16BIT_ALLOC(
	    count,
	    count_limit
	)

	u32 status_data;

	___DSP_DUAL_16BIT_ALLOC(  
	    status,
	    free4
	)

	u32 free3;

	___DSP_DUAL_16BIT_ALLOC(  
	    free2,
	    bit_count
	)

	u32  temp_status;
  
	___DSP_DUAL_16BIT_ALLOC(
	    next_SCB,
	    sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,
	    this_spb
	)

	u32  strm_rs_config;
	u32  strm_buf_ptr;
  
	___DSP_DUAL_16BIT_ALLOC(
	    stat_reg_addr, 
	    fifo_pointer
	)

	___DSP_DUAL_16BIT_ALLOC(
	    fifo_base_addr, 
	    st_mo_format
	)

	u32  free1;
};


struct dsp_spdifoscb {		 

	u32 free2;     

	u32 free3[4];             

	
	u32 strm_rs_config;
                               
	u32 strm_buf_ptr;

	___DSP_DUAL_16BIT_ALLOC(  
	    status,
	    free5
	)

	u32 free4;

	___DSP_DUAL_16BIT_ALLOC(  
	    next_scb,
	    sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,
	    this_spb
	)

	u32 free6[2];
  
	___DSP_DUAL_16BIT_ALLOC(
	    stat_reg_addr, 
	    fifo_pointer
	)

	___DSP_DUAL_16BIT_ALLOC(
	    fifo_base_addr,
	    st_mo_format
	)

	u32  free1;                                         
};


struct dsp_asynch_fg_rx_scb {
	___DSP_DUAL_16BIT_ALLOC(
	    bot_buf_mask,
	    buf_Mask
	)

	___DSP_DUAL_16BIT_ALLOC(
	    max,
	    min
	)

	___DSP_DUAL_16BIT_ALLOC(
	    old_producer_pointer,
	    hfg_scb_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    delta,
	    adjust_count
	)

	u32 unused2[5];  

	___DSP_DUAL_16BIT_ALLOC(  
	    sibling_ptr,  
	    child_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    code_ptr,
	    this_ptr
	)

	u32 strm_rs_config; 

	u32 strm_buf_ptr;
  
	u32 unused_phi_incr;
  
	___DSP_DUAL_16BIT_ALLOC(
	    right_targ,   
	    left_targ
	)

	___DSP_DUAL_16BIT_ALLOC(
	    right_vol,
	    left_vol
	)
};


struct dsp_asynch_fg_tx_scb {
	___DSP_DUAL_16BIT_ALLOC(
	    not_buf_mask,
	    buf_mask
	)

	___DSP_DUAL_16BIT_ALLOC(
	    max,
	    min
	)

	___DSP_DUAL_16BIT_ALLOC(
	    unused1,
	    hfg_scb_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    delta,
	    adjust_count
	)

	u32 accum_phi;

	___DSP_DUAL_16BIT_ALLOC(
	    unused2,
	    const_one_third
	)

	u32 unused3[3];

	___DSP_DUAL_16BIT_ALLOC(
	    sibling_ptr,
	    child_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    codePtr,
	    this_ptr
	)

	u32 strm_rs_config;

	u32 strm_buf_ptr;

	u32 phi_incr;

	___DSP_DUAL_16BIT_ALLOC(
	    unused_right_targ,
	    unused_left_targ
	)

	___DSP_DUAL_16BIT_ALLOC(
	    unused_right_vol,
	    unused_left_vol
	)
};


struct dsp_output_snoop_scb {
	
	struct dsp_basic_dma_req  basic_req;  
	struct dsp_scatter_gather_ext sg_ext;  
	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,       
	    sub_list_ptr    
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,    
	    this_spb        
	)

	u32 strm_rs_config; 
	u32 strm_buf_ptr;   

	___DSP_DUAL_16BIT_ALLOC(
	    init_snoop_input_link,
	    snoop_child_input_scb
	)

	u32 snoop_input_buf_ptr;

	___DSP_DUAL_16BIT_ALLOC(
	    reserved,
	    input_scb
	)
};

struct dsp_spio_write_scb {
	___DSP_DUAL_16BIT_ALLOC(
	    address1,
	    address2
	)

	u32 data1;

	u32 data2;

	___DSP_DUAL_16BIT_ALLOC(
	    address3,
	    address4
	)

	u32 data3;

	u32 data4;

	___DSP_DUAL_16BIT_ALLOC(
	    unused1,
	    data_ptr
	)

	u32 unused2[2];

	___DSP_DUAL_16BIT_ALLOC(
	    sibling_ptr,
	    child_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,
	    this_ptr
	)

	u32 unused3[5];
};

struct dsp_magic_snoop_task {
	u32 i0;
	u32 i1;

	u32 strm_buf_ptr1;
  
	u16 i2;
	u16 snoop_scb;

	u32 i3;
	u32 i4;
	u32 i5;
	u32 i6;

	u32 i7;

	___DSP_DUAL_16BIT_ALLOC(
	    next_scb,
	    sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	    entry_point,
	    this_ptr
	)

	u32 strm_buf_config;
	u32 strm_buf_ptr2;

	u32 i8;

	struct dsp_volume_control vdec_vol_ctrl;
};


struct dsp_filter_scb {
	___DSP_DUAL_16BIT_ALLOC(
	      a0_right,          
	      a0_left
	)
	___DSP_DUAL_16BIT_ALLOC(
	      a1_right,          
	      a1_left
	)
	___DSP_DUAL_16BIT_ALLOC(
	      a2_right,          
	      a2_left
	)
	___DSP_DUAL_16BIT_ALLOC(
	      output_buf_ptr,    
	      init
	)

	___DSP_DUAL_16BIT_ALLOC(
	      filter_unused3,    
	      filter_unused2
	)

	u32 prev_sample_output1; 
	u32 prev_sample_output2; 
	u32 prev_sample_input1;  
	u32 prev_sample_input2;  

	___DSP_DUAL_16BIT_ALLOC(
	      next_scb_ptr,      
	      sub_list_ptr
	)

	___DSP_DUAL_16BIT_ALLOC(
	      entry_point,       
	      spb_ptr
	)

	u32  strm_rs_config;     
	u32  strm_buf_ptr;       

	___DSP_DUAL_16BIT_ALLOC(
              b0_right,          
	      b0_left
	)
	___DSP_DUAL_16BIT_ALLOC(
              b1_right,          
	      b1_left
	)
	___DSP_DUAL_16BIT_ALLOC(
              b2_right,          
	      b2_left
	)
};
#endif 
