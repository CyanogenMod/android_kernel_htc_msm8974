/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003-2005 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_PCI_TIOCP_H
#define _ASM_IA64_SN_PCI_TIOCP_H

#define TIOCP_HOST_INTR_ADDR            0x003FFFFFFFFFFFFFUL
#define TIOCP_PCI64_CMDTYPE_MEM         (0x1ull << 60)
#define TIOCP_PCI64_CMDTYPE_MSI         (0x3ull << 60)



struct tiocp{

    

    
    u64		cp_id;				
    u64		cp_stat;			
    u64		cp_err_upper;			
    u64		cp_err_lower;			
    #define cp_err cp_err_lower
    u64		cp_control;			
    u64		cp_req_timeout;			
    u64		cp_intr_upper;			
    u64		cp_intr_lower;			
    #define cp_intr cp_intr_lower
    u64		cp_err_cmdword;			
    u64		_pad_000048;			
    u64		cp_tflush;			

    
    u64		cp_aux_err;			
    u64		cp_resp_upper;			
    u64		cp_resp_lower;			
    #define cp_resp cp_resp_lower
    u64		cp_tst_pin_ctrl;		
    u64		cp_addr_lkerr;			

    
    u64		cp_dir_map;			
    u64		_pad_000088;			

    
    u64		cp_map_fault;			
    u64		_pad_000098;			

    
    u64		cp_arb;				
    u64		_pad_0000A8;			

    
    u64		cp_ate_parity_err;		
    u64		_pad_0000B8;			

    
    u64		cp_bus_timeout;			
    u64		cp_pci_cfg;			
    u64		cp_pci_err_upper;		
    u64		cp_pci_err_lower;		
    #define cp_pci_err cp_pci_err_lower
    u64		_pad_0000E0[4];			

    
    u64		cp_int_status;			
    u64		cp_int_enable;			
    u64		cp_int_rst_stat;		
    u64		cp_int_mode;			
    u64		cp_int_device;			
    u64		cp_int_host_err;		
    u64		cp_int_addr[8];			
    u64		cp_err_int_view;		
    u64		cp_mult_int;			
    u64		cp_force_always[8];		
    u64		cp_force_pin[8];		

    
    u64		cp_device[4];			
    u64		_pad_000220[4];			
    u64		cp_wr_req_buf[4];		
    u64		_pad_000260[4];			
    u64		cp_rrb_map[2];			
    #define cp_even_resp cp_rrb_map[0]			
    #define cp_odd_resp  cp_rrb_map[1]			
    u64		cp_resp_status;			
    u64		cp_resp_clear;			

    u64		_pad_0002A0[12];		

    
    struct {
	u64	upper;				
	u64	lower;				
    } cp_buf_addr_match[16];

    
    struct {
	u64	flush_w_touch;			
	u64	flush_wo_touch;			
	u64	inflight;			
	u64	prefetch;			
	u64	total_pci_retry;		
	u64	max_pci_retry;			
	u64	max_latency;			
	u64	clear_all;			
    } cp_buf_count[8];


    
    u64		cp_pcix_bus_err_addr;		
    u64		cp_pcix_bus_err_attr;		
    u64		cp_pcix_bus_err_data;		
    u64		cp_pcix_pio_split_addr;		
    u64		cp_pcix_pio_split_attr;		
    u64		cp_pcix_dma_req_err_attr;	
    u64		cp_pcix_dma_req_err_addr;	
    u64		cp_pcix_timeout;		

    u64		_pad_000640[24];		

    
    u64		cp_ct_debug_ctl;		
    u64		cp_br_debug_ctl;		
    u64		cp_mux3_debug_ctl;		
    u64		cp_mux4_debug_ctl;		
    u64		cp_mux5_debug_ctl;		
    u64		cp_mux6_debug_ctl;		
    u64		cp_mux7_debug_ctl;		

    u64		_pad_000738[89];		

    
    struct {
	u64	cp_buf_addr;			
	u64	cp_buf_attr;			
    } cp_pcix_read_buf_64[16];

    struct {
	u64	cp_buf_addr;			
	u64	cp_buf_attr;			
	u64	cp_buf_valid;			
	u64	__pad1;				
    } cp_pcix_write_buf_64[8];

    

    char	_pad_000c00[0x010000 - 0x000c00];

    
    u64		cp_int_ate_ram[1024];		

    char	_pad_012000[0x14000 - 0x012000];

    
    u64		cp_int_ate_ram_mp[1024];	

    char	_pad_016000[0x18000 - 0x016000];

    
    u64		cp_wr_req_lower[256];		
    u64		cp_wr_req_upper[256];		
    u64		cp_wr_req_parity[256];		

    char	_pad_019800[0x1C000 - 0x019800];

    
    u64		cp_rd_resp_lower[512];		
    u64		cp_rd_resp_upper[512];		
    u64		cp_rd_resp_parity[512];		

    char	_pad_01F000[0x20000 - 0x01F000];

    
    char	_pad_020000[0x021000 - 0x20000];

    
    union {
	u8	c[0x1000 / 1];			
	u16	s[0x1000 / 2];			
	u32	l[0x1000 / 4];			
	u64	d[0x1000 / 8];			
	union {
	    u8	c[0x100 / 1];
	    u16	s[0x100 / 2];
	    u32	l[0x100 / 4];
	    u64	d[0x100 / 8];
	} f[8];
    } cp_type0_cfg_dev[7];				

    
    union {
	u8	c[0x1000 / 1];			
	u16	s[0x1000 / 2];			
	u32	l[0x1000 / 4];			
	u64	d[0x1000 / 8];			
	union {
	    u8	c[0x100 / 1];
	    u16	s[0x100 / 2];
	    u32	l[0x100 / 4];
	    u64	d[0x100 / 8];
	} f[8];
    } cp_type1_cfg;					

    char		_pad_029000[0x030000-0x029000];

    
    union {
	u8	c[8 / 1];
	u16	s[8 / 2];
	u32	l[8 / 4];
	u64	d[8 / 8];
    } cp_pci_iack;					

    char		_pad_030007[0x040000-0x030008];

    
    union {
	u8	c[8 / 1];
	u16	s[8 / 2];
	u32	l[8 / 4];
	u64	d[8 / 8];
    } cp_pcix_cycle;					

    char		_pad_040007[0x200000-0x040008];

    
    union {
	u8	c[0x100000 / 1];
	u16	s[0x100000 / 2];
	u32	l[0x100000 / 4];
	u64	d[0x100000 / 8];
    } cp_devio_raw[6];					

    #define cp_devio(n)  cp_devio_raw[((n)<2)?(n*2):(n+2)]

    char		_pad_800000[0xA00000-0x800000];

    
    union {
	u8	c[0x100000 / 1];
	u16	s[0x100000 / 2];
	u32	l[0x100000 / 4];
	u64	d[0x100000 / 8];
    } cp_devio_raw_flush[6];				

    #define cp_devio_flush(n)  cp_devio_raw_flush[((n)<2)?(n*2):(n+2)]

};

#endif 	
