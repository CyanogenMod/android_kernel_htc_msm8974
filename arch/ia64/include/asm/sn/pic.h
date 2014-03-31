/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_PCI_PIC_H
#define _ASM_IA64_SN_PCI_PIC_H


#define PIC_ATE_TARGETID_SHFT           8
#define PIC_HOST_INTR_ADDR              0x0000FFFFFFFFFFFFUL
#define PIC_PCI64_ATTR_TARG_SHFT        60




struct pic {

    

    
    u64		p_wid_id;			
    u64		p_wid_stat;			
    u64		p_wid_err_upper;		
    u64		p_wid_err_lower;		
    #define p_wid_err p_wid_err_lower
    u64		p_wid_control;			
    u64		p_wid_req_timeout;		
    u64		p_wid_int_upper;		
    u64		p_wid_int_lower;		
    #define p_wid_int p_wid_int_lower
    u64		p_wid_err_cmdword;		
    u64		p_wid_llp;			
    u64		p_wid_tflush;			

    
    u64		p_wid_aux_err;			
    u64		p_wid_resp_upper;		
    u64		p_wid_resp_lower;		
    #define p_wid_resp p_wid_resp_lower
    u64		p_wid_tst_pin_ctrl;		
    u64		p_wid_addr_lkerr;		

    
    u64		p_dir_map;			
    u64		_pad_000088;			

    
    u64		p_map_fault;			
    u64		_pad_000098;			

    
    u64		p_arb;				
    u64		_pad_0000A8;			

    
    u64		p_ate_parity_err;		
    u64		_pad_0000B8;			

    
    u64		p_bus_timeout;			
    u64		p_pci_cfg;			
    u64		p_pci_err_upper;		
    u64		p_pci_err_lower;		
    #define p_pci_err p_pci_err_lower
    u64		_pad_0000E0[4];			

    
    u64		p_int_status;			
    u64		p_int_enable;			
    u64		p_int_rst_stat;			
    u64		p_int_mode;			
    u64		p_int_device;			
    u64		p_int_host_err;			
    u64		p_int_addr[8];			
    u64		p_err_int_view;			
    u64		p_mult_int;			
    u64		p_force_always[8];		
    u64		p_force_pin[8];			

    
    u64		p_device[4];			
    u64		_pad_000220[4];			
    u64		p_wr_req_buf[4];		
    u64		_pad_000260[4];			
    u64		p_rrb_map[2];			
    #define p_even_resp p_rrb_map[0]			
    #define p_odd_resp  p_rrb_map[1]			
    u64		p_resp_status;			
    u64		p_resp_clear;			

    u64		_pad_0002A0[12];		

    
    struct {
	u64	upper;				
	u64	lower;				
    } p_buf_addr_match[16];

    
    struct {
	u64	flush_w_touch;			
	u64	flush_wo_touch;			
	u64	inflight;			
	u64	prefetch;			
	u64	total_pci_retry;		
	u64	max_pci_retry;			
	u64	max_latency;			
	u64	clear_all;			
    } p_buf_count[8];


    
    u64		p_pcix_bus_err_addr;		
    u64		p_pcix_bus_err_attr;		
    u64		p_pcix_bus_err_data;		
    u64		p_pcix_pio_split_addr;		
    u64		p_pcix_pio_split_attr;		
    u64		p_pcix_dma_req_err_attr;	
    u64		p_pcix_dma_req_err_addr;	
    u64		p_pcix_timeout;			

    u64		_pad_000640[120];		

    
    struct {
	u64	p_buf_addr;			
	u64	p_buf_attr;			
    } p_pcix_read_buf_64[16];

    struct {
	u64	p_buf_addr;			
	u64	p_buf_attr;			
	u64	p_buf_valid;			
	u64	__pad1;				
    } p_pcix_write_buf_64[8];

    

    char		_pad_000c00[0x010000 - 0x000c00];

    
    u64		p_int_ate_ram[1024];		

    
    u64		p_int_ate_ram_mp[1024];		

    char		_pad_014000[0x18000 - 0x014000];

    
    u64		p_wr_req_lower[256];		
    u64		p_wr_req_upper[256];		
    u64		p_wr_req_parity[256];		

    char		_pad_019800[0x20000 - 0x019800];

    
    union {
	u8		c[0x1000 / 1];			
	u16	s[0x1000 / 2];			
	u32	l[0x1000 / 4];			
	u64	d[0x1000 / 8];			
	union {
	    u8	c[0x100 / 1];
	    u16	s[0x100 / 2];
	    u32	l[0x100 / 4];
	    u64	d[0x100 / 8];
	} f[8];
    } p_type0_cfg_dev[8];				

    
    union {
	u8		c[0x1000 / 1];			
	u16	s[0x1000 / 2];			
	u32	l[0x1000 / 4];			
	u64	d[0x1000 / 8];			
	union {
	    u8	c[0x100 / 1];
	    u16	s[0x100 / 2];
	    u32	l[0x100 / 4];
	    u64	d[0x100 / 8];
	} f[8];
    } p_type1_cfg;					

    char		_pad_029000[0x030000-0x029000];

    
    union {
	u8		c[8 / 1];
	u16	s[8 / 2];
	u32	l[8 / 4];
	u64	d[8 / 8];
    } p_pci_iack;					

    char		_pad_030007[0x040000-0x030008];

    
    union {
	u8		c[8 / 1];
	u16	s[8 / 2];
	u32	l[8 / 4];
	u64	d[8 / 8];
    } p_pcix_cycle;					
};

#endif                          
