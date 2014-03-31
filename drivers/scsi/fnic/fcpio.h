/*
 * Copyright 2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _FCPIO_H_
#define _FCPIO_H_

#include <linux/if_ether.h>


#define FCPIO_HOST_EXCH_RANGE_START         0x1000
#define FCPIO_HOST_EXCH_RANGE_END           0x1fff
#define FCPIO_HOST_SEQ_ID_RANGE_START       0x80
#define FCPIO_HOST_SEQ_ID_RANGE_END         0xff

enum fcpio_type {
	FCPIO_ICMND_16 = 0x1,
	FCPIO_ICMND_32,
	FCPIO_ICMND_CMPL,
	FCPIO_ITMF,
	FCPIO_ITMF_CMPL,

	FCPIO_TCMND_16 = 0x11,
	FCPIO_TCMND_32,
	FCPIO_TDATA,
	FCPIO_TXRDY,
	FCPIO_TRSP,
	FCPIO_TDRSP_CMPL,
	FCPIO_TTMF,
	FCPIO_TTMF_ACK,
	FCPIO_TABORT,
	FCPIO_TABORT_CMPL,

	FCPIO_ACK = 0x20,
	FCPIO_RESET,
	FCPIO_RESET_CMPL,
	FCPIO_FLOGI_REG,
	FCPIO_FLOGI_REG_CMPL,
	FCPIO_ECHO,
	FCPIO_ECHO_CMPL,
	FCPIO_LUNMAP_CHNG,
	FCPIO_LUNMAP_REQ,
	FCPIO_LUNMAP_REQ_CMPL,
	FCPIO_FLOGI_FIP_REG,
	FCPIO_FLOGI_FIP_REG_CMPL,
};

enum fcpio_status {
	FCPIO_SUCCESS = 0,              

	FCPIO_INVALID_HEADER,    
	FCPIO_OUT_OF_RESOURCE,   
	FCPIO_INVALID_PARAM,     
	FCPIO_REQ_NOT_SUPPORTED, 
	FCPIO_IO_NOT_FOUND,      

	FCPIO_ABORTED = 0x41,     
	FCPIO_TIMEOUT,            
	FCPIO_SGL_INVALID,        
	FCPIO_MSS_INVALID,        
	FCPIO_DATA_CNT_MISMATCH,  
	FCPIO_FW_ERR,             
	FCPIO_ITMF_REJECTED,      
	FCPIO_ITMF_FAILED,        
	FCPIO_ITMF_INCORRECT_LUN, 
	FCPIO_CMND_REJECTED,      
	FCPIO_NO_PATH_AVAIL,      
	FCPIO_PATH_FAILED,        
	FCPIO_LUNMAP_CHNG_PEND,   
};

struct fcpio_tag {
	union {
		u32 req_id;
		struct {
			u16 rx_id;
			u16 ox_id;
		} ex_id;
	} u;
};

static inline void
fcpio_tag_id_enc(struct fcpio_tag *tag, u32 id)
{
	tag->u.req_id = id;
}

static inline void
fcpio_tag_id_dec(struct fcpio_tag *tag, u32 *id)
{
	*id = tag->u.req_id;
}

static inline void
fcpio_tag_exid_enc(struct fcpio_tag *tag, u16 ox_id, u16 rx_id)
{
	tag->u.ex_id.rx_id = rx_id;
	tag->u.ex_id.ox_id = ox_id;
}

static inline void
fcpio_tag_exid_dec(struct fcpio_tag *tag, u16 *ox_id, u16 *rx_id)
{
	*rx_id = tag->u.ex_id.rx_id;
	*ox_id = tag->u.ex_id.ox_id;
}

struct fcpio_header {
	u8            type;           
	u8            status;         
	u16           _resvd;         
	struct fcpio_tag    tag;      
};

static inline void
fcpio_header_enc(struct fcpio_header *hdr,
		 u8 type, u8 status,
		 struct fcpio_tag tag)
{
	hdr->type = type;
	hdr->status = status;
	hdr->_resvd = 0;
	hdr->tag = tag;
}

static inline void
fcpio_header_dec(struct fcpio_header *hdr,
		 u8 *type, u8 *status,
		 struct fcpio_tag *tag)
{
	*type = hdr->type;
	*status = hdr->status;
	*tag = hdr->tag;
}

#define CDB_16      16
#define CDB_32      32
#define LUN_ADDRESS 8

struct fcpio_icmnd_16 {
	u32	  lunmap_id;		
	u8	  special_req_flags;	
	u8	  _resvd0[3];	        
	u32	  sgl_cnt;		
	u32	  sense_len;		
	u64	  sgl_addr;		
	u64	  sense_addr;		
	u8	  crn;			
	u8	  pri_ta;		
	u8	  _resvd1;		
	u8	  flags;		
	u8	  scsi_cdb[CDB_16];	
	u32	  data_len;		
	u8	  lun[LUN_ADDRESS];	
	u8	  _resvd2;		
	u8	  d_id[3];		
	u16	  mss;			
	u16	  _resvd3;		
	u32	  r_a_tov;		
	u32	  e_d_tov;	        
};

#define FCPIO_ICMND_SRFLAG_RETRY 0x01   

#define FCPIO_ICMND_PTA_SIMPLE      0   
#define FCPIO_ICMND_PTA_HEADQ       1   
#define FCPIO_ICMND_PTA_ORDERED     2   
#define FCPIO_ICMND_PTA_ACA         4   
#define FCPIO_ICMND_PRI_SHIFT       3   

#define FCPIO_ICMND_RDDATA      0x02    
#define FCPIO_ICMND_WRDATA      0x01    

struct fcpio_icmnd_32 {
	u32   lunmap_id;              
	u8    special_req_flags;      
	u8    _resvd0[3];             
	u32   sgl_cnt;                
	u32   sense_len;              
	u64   sgl_addr;               
	u64   sense_addr;             
	u8    crn;                    
	u8    pri_ta;                 
	u8    _resvd1;                
	u8    flags;                  
	u8    scsi_cdb[CDB_32];       
	u32   data_len;               
	u8    lun[LUN_ADDRESS];       
	u8    _resvd2;                
	u8    d_id[3];		      
	u16   mss;                    
	u16   _resvd3;                
	u32   r_a_tov;                
	u32   e_d_tov;                
};

struct fcpio_itmf {
	u32   lunmap_id;              
	u32   tm_req;                 
	u32   t_tag;                  
	u32   _resvd;                 
	u8    lun[LUN_ADDRESS];       
	u8    _resvd1;                
	u8    d_id[3];		      
	u32   r_a_tov;                
	u32   e_d_tov;                
};

enum fcpio_itmf_tm_req_type {
	FCPIO_ITMF_ABT_TASK_TERM = 0x01,    
	FCPIO_ITMF_ABT_TASK,                
	FCPIO_ITMF_ABT_TASK_SET,            
	FCPIO_ITMF_CLR_TASK_SET,            
	FCPIO_ITMF_LUN_RESET,               
	FCPIO_ITMF_CLR_ACA,                 
};

struct fcpio_tdata {
	u16   rx_id;                  
	u16   flags;                  
	u32   rel_offset;             
	u32   sgl_cnt;                
	u32   data_len;               
	u64   sgl_addr;               
};

#define FCPIO_TDATA_SCSI_RSP    0x01    

struct fcpio_txrdy {
	u16   rx_id;                  
	u16   _resvd0;                
	u32   rel_offset;             
	u32   sgl_cnt;                
	u32   data_len;               
	u64   sgl_addr;               
};

struct fcpio_trsp {
	u16   rx_id;                  
	u16   _resvd0;                
	u32   sense_len;              
	u64   sense_addr;             
	u16   _resvd1;                
	u8    flags;                  
	u8    scsi_status;            
	u32   residual;               
};

#define FCPIO_TRSP_RESID_UNDER  0x08   
#define FCPIO_TRSP_RESID_OVER   0x04   

struct fcpio_ttmf_ack {
	u16   rx_id;                  
	u16   _resvd0;                
	u32   tmf_status;             
};

struct fcpio_tabort {
	u16   rx_id;                  
};

struct fcpio_reset {
	u32   _resvd;
};

enum fcpio_flogi_reg_format_type {
	FCPIO_FLOGI_REG_DEF_DEST = 0,    
	FCPIO_FLOGI_REG_GW_DEST,         
};

struct fcpio_flogi_reg {
	u8 format;
	u8 s_id[3];			
	u8 gateway_mac[ETH_ALEN];	
	u16 _resvd;
	u32 r_a_tov;			
	u32 e_d_tov;			
};

struct fcpio_echo {
	u32 _resvd;
};

struct fcpio_lunmap_req {
	u64 addr;                     
	u32 len;                      
};

struct fcpio_flogi_fip_reg {
	u8    _resvd0;
	u8     s_id[3];               
	u8     fcf_mac[ETH_ALEN];     
	u16   _resvd1;
	u32   r_a_tov;                
	u32   e_d_tov;                
	u8    ha_mac[ETH_ALEN];       
	u16   _resvd2;
};

#define FCPIO_HOST_REQ_LEN      128     

struct fcpio_host_req {
	struct fcpio_header hdr;

	union {
		u8 buf[FCPIO_HOST_REQ_LEN - sizeof(struct fcpio_header)];

		struct fcpio_icmnd_16               icmnd_16;
		struct fcpio_icmnd_32               icmnd_32;
		struct fcpio_itmf                   itmf;

		struct fcpio_tdata                  tdata;
		struct fcpio_txrdy                  txrdy;
		struct fcpio_trsp                   trsp;
		struct fcpio_ttmf_ack               ttmf_ack;
		struct fcpio_tabort                 tabort;

		struct fcpio_reset                  reset;
		struct fcpio_flogi_reg              flogi_reg;
		struct fcpio_echo                   echo;
		struct fcpio_lunmap_req             lunmap_req;
		struct fcpio_flogi_fip_reg          flogi_fip_reg;
	} u;
};

struct fcpio_icmnd_cmpl {
	u8    _resvd0[6];             
	u8    flags;                  
	u8    scsi_status;            
	u32   residual;               
	u32   sense_len;              
};

#define FCPIO_ICMND_CMPL_RESID_UNDER    0x08    
#define FCPIO_ICMND_CMPL_RESID_OVER     0x04    

struct fcpio_itmf_cmpl {
	u32    _resvd;                
};

struct fcpio_tcmnd_16 {
	u8    lun[LUN_ADDRESS];       
	u8    crn;                    
	u8    pri_ta;                 
	u8    _resvd2;                
	u8    flags;                  
	u8    scsi_cdb[CDB_16];       
	u32   data_len;               
	u8    _resvd1;                
	u8    s_id[3];		      
};

#define FCPIO_TCMND_PTA_SIMPLE      0   
#define FCPIO_TCMND_PTA_HEADQ       1   
#define FCPIO_TCMND_PTA_ORDERED     2   
#define FCPIO_TCMND_PTA_ACA         4   
#define FCPIO_TCMND_PRI_SHIFT       3   

#define FCPIO_TCMND_RDDATA      0x02    
#define FCPIO_TCMND_WRDATA      0x01    

struct fcpio_tcmnd_32 {
	u8    lun[LUN_ADDRESS];       
	u8    crn;                    
	u8    pri_ta;                 
	u8    _resvd2;                
	u8    flags;                  
	u8    scsi_cdb[CDB_32];       
	u32   data_len;               
	u8    _resvd0;                
	u8    s_id[3];		      
};

struct fcpio_tdrsp_cmpl {
	u16   rx_id;                  
	u16   _resvd0;                
};

struct fcpio_ttmf {
	u8    _resvd0;                
	u8    s_id[3];		      
	u8    lun[LUN_ADDRESS];       
	u8    crn;                    
	u8    _resvd2[3];             
	u32   tmf_type;               
};

#define FCPIO_TTMF_CLR_ACA      0x40    
#define FCPIO_TTMF_LUN_RESET    0x10    
#define FCPIO_TTMF_CLR_TASK_SET 0x04    
#define FCPIO_TTMF_ABT_TASK_SET 0x02    
#define FCPIO_TTMF_ABT_TASK     0x01    

struct fcpio_tabort_cmpl {
	u16   rx_id;                  
	u16   _resvd0;                
};

struct fcpio_ack {
	u16  request_out;             
	u16  _resvd;
};

struct fcpio_reset_cmpl {
	u16   vnic_id;
};

struct fcpio_flogi_reg_cmpl {
	u32 _resvd;
};

struct fcpio_echo_cmpl {
	u32 _resvd;
};

struct fcpio_lunmap_chng {
	u32 _resvd;
};

struct fcpio_lunmap_req_cmpl {
	u32 _resvd;
};

#define FCPIO_FW_REQ_LEN        64      
struct fcpio_fw_req {
	struct fcpio_header hdr;

	union {
		u8 buf[FCPIO_FW_REQ_LEN - sizeof(struct fcpio_header)];

		struct fcpio_icmnd_cmpl         icmnd_cmpl;
		struct fcpio_itmf_cmpl          itmf_cmpl;

		struct fcpio_tcmnd_16           tcmnd_16;
		struct fcpio_tcmnd_32           tcmnd_32;

		struct fcpio_tdrsp_cmpl         tdrsp_cmpl;
		struct fcpio_ttmf               ttmf;
		struct fcpio_tabort_cmpl        tabort_cmpl;

		struct fcpio_ack                ack;

		struct fcpio_reset_cmpl         reset_cmpl;
		struct fcpio_flogi_reg_cmpl     flogi_reg_cmpl;
		struct fcpio_echo_cmpl          echo_cmpl;
		struct fcpio_lunmap_chng        lunmap_chng;
		struct fcpio_lunmap_req_cmpl    lunmap_req_cmpl;
	} u;
};

static inline void fcpio_color_enc(struct fcpio_fw_req *fw_req, u8 color)
{
	u8 *c = ((u8 *) fw_req) + sizeof(struct fcpio_fw_req) - 1;

	if (color)
		*c |= 0x80;
	else
		*c &= ~0x80;
}

static inline void fcpio_color_dec(struct fcpio_fw_req *fw_req, u8 *color)
{
	u8 *c = ((u8 *) fw_req) + sizeof(struct fcpio_fw_req) - 1;

	*color = *c >> 7;

	/*
	 * Make sure color bit is read from desc *before* other fields
	 * are read from desc.  Hardware guarantees color bit is last
	 * bit (byte) written.  Adding the rmb() prevents the compiler
	 * and/or CPU from reordering the reads which would potentially
	 * result in reading stale values.
	 */

	rmb();

}

#define FCPIO_LUNMAP_TABLE_SIZE     256
#define FCPIO_FLAGS_LUNMAP_VALID    0x80
#define FCPIO_FLAGS_BOOT            0x01
struct fcpio_lunmap_entry {
	u8    bus;
	u8    target;
	u8    lun;
	u8    path_cnt;
	u16   flags;
	u16   update_cnt;
};

struct fcpio_lunmap_tbl {
	u32                   update_cnt;
	struct fcpio_lunmap_entry   lunmaps[FCPIO_LUNMAP_TABLE_SIZE];
};

#endif 
