#ifndef _ASM_IA64_SN_TIO_TIOCA_H
#define _ASM_IA64_SN_TIO_TIOCA_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2003-2005 Silicon Graphics, Inc. All rights reserved.
 */


#define TIOCA_PART_NUM	0xE020
#define TIOCA_MFGR_NUM	0x24
#define TIOCA_REV_A	0x1


struct tioca {
	u64	ca_id;				
	u64	ca_control1;			
	u64	ca_control2;			
	u64	ca_status1;			
	u64	ca_status2;			
	u64	ca_gart_aperature;		
	u64	ca_gfx_detach;			
	u64	ca_inta_dest_addr;		
	u64	ca_intb_dest_addr;		
	u64	ca_err_int_dest_addr;		
	u64	ca_int_status;			
	u64	ca_int_status_alias;		
	u64	ca_mult_error;			
	u64	ca_mult_error_alias;		
	u64	ca_first_error;			
	u64	ca_int_mask;			
	u64	ca_crm_pkterr_type;		
	u64	ca_crm_pkterr_type_alias;	
	u64	ca_crm_ct_error_detail_1;	
	u64	ca_crm_ct_error_detail_2;	
	u64	ca_crm_tnumto;			
	u64	ca_gart_err;			
	u64	ca_pcierr_type;			
	u64	ca_pcierr_addr;			

	u64	ca_pad_0000C0[3];		

	u64	ca_pci_rd_buf_flush;		
	u64	ca_pci_dma_addr_extn;		
	u64	ca_agp_dma_addr_extn;		
	u64	ca_force_inta;			
	u64	ca_force_intb;			
	u64	ca_debug_vector_sel;		
	u64	ca_debug_mux_core_sel;		
	u64	ca_debug_mux_pci_sel;		
	u64	ca_debug_domain_sel;		

	u64	ca_pad_000120[28];		

	u64	ca_gart_ptr_table;		
	u64	ca_gart_tlb_addr[8];		
};


#define CA_SYS_BIG_END			(1ull << 0)
#define CA_DMA_AGP_SWAP			(1ull << 1)
#define CA_DMA_PCI_SWAP			(1ull << 2)
#define CA_PIO_IO_SWAP			(1ull << 3)
#define CA_PIO_MEM_SWAP			(1ull << 4)
#define CA_GFX_WR_SWAP			(1ull << 5)
#define CA_AGP_FW_ENABLE		(1ull << 6)
#define CA_AGP_CAL_CYCLE		(0x7ull << 7)
#define CA_AGP_CAL_CYCLE_SHFT		7
#define CA_AGP_CAL_PRSCL_BYP		(1ull << 10)
#define CA_AGP_INIT_CAL_ENB		(1ull << 11)
#define CA_INJ_ADDR_PERR		(1ull << 12)
#define CA_INJ_DATA_PERR		(1ull << 13)
	
#define CA_PCIM_IO_NBE_AD		(0x7ull << 16)
#define CA_PCIM_IO_NBE_AD_SHFT		16
#define CA_PCIM_FAST_BTB_ENB		(1ull << 19)
	
#define CA_PIO_ADDR_OFFSET		(0xffull << 24)
#define CA_PIO_ADDR_OFFSET_SHFT		24
	
#define CA_AGPDMA_OP_COMBDELAY		(0x1full << 36)
#define CA_AGPDMA_OP_COMBDELAY_SHFT	36
	
#define CA_AGPDMA_OP_ENB_COMBDELAY	(1ull << 42)
#define	CA_PCI_INT_LPCNT		(0xffull << 44)
#define CA_PCI_INT_LPCNT_SHFT		44
	

#define CA_AGP_LATENCY_TO		(0xffull << 0)
#define CA_AGP_LATENCY_TO_SHFT		0
#define CA_PCI_LATENCY_TO		(0xffull << 8)
#define CA_PCI_LATENCY_TO_SHFT		8
#define CA_PCI_MAX_RETRY		(0x3ffull << 16)
#define CA_PCI_MAX_RETRY_SHFT		16
	
#define CA_RT_INT_EN			(0x3ull << 28)
#define CA_RT_INT_EN_SHFT			28
#define CA_MSI_INT_ENB			(1ull << 30)
#define CA_PCI_ARB_ERR_ENB		(1ull << 31)
#define CA_GART_MEM_PARAM		(0x3ull << 32)
#define CA_GART_MEM_PARAM_SHFT		32
#define CA_GART_RD_PREFETCH_ENB		(1ull << 34)
#define CA_GART_WR_PREFETCH_ENB		(1ull << 35)
#define CA_GART_FLUSH_TLB		(1ull << 36)
	
#define CA_CRM_TNUMTO_PERIOD		(0x1fffull << 40)
#define CA_CRM_TNUMTO_PERIOD_SHFT	40
	
#define CA_CRM_TNUMTO_ENB		(1ull << 56)
#define CA_CRM_PRESCALER_BYP		(1ull << 57)
	
#define CA_CRM_MAX_CREDIT		(0x7ull << 60)
#define CA_CRM_MAX_CREDIT_SHFT		60
	

#define CA_CORELET_ID			(0x3ull << 0)
#define CA_CORELET_ID_SHFT		0
#define CA_INTA_N			(1ull << 2)
#define CA_INTB_N			(1ull << 3)
#define CA_CRM_CREDIT_AVAIL		(0x7ull << 4)
#define CA_CRM_CREDIT_AVAIL_SHFT	4
	
#define CA_CRM_SPACE_AVAIL		(0x7full << 8)
#define CA_CRM_SPACE_AVAIL_SHFT		8
	
#define CA_GART_TLB_VAL			(0xffull << 16)
#define CA_GART_TLB_VAL_SHFT		16
	

#define CA_GFX_CREDIT_AVAIL		(0xffull << 0)
#define CA_GFX_CREDIT_AVAIL_SHFT	0
#define CA_GFX_OPQ_AVAIL		(0xffull << 8)
#define CA_GFX_OPQ_AVAIL_SHFT		8
#define CA_GFX_WRBUFF_AVAIL		(0xffull << 16)
#define CA_GFX_WRBUFF_AVAIL_SHFT	16
#define CA_ADMA_OPQ_AVAIL		(0xffull << 24)
#define CA_ADMA_OPQ_AVAIL_SHFT		24
#define CA_ADMA_WRBUFF_AVAIL		(0xffull << 32)
#define CA_ADMA_WRBUFF_AVAIL_SHFT	32
#define CA_ADMA_RDBUFF_AVAIL		(0x7full << 40)
#define CA_ADMA_RDBUFF_AVAIL_SHFT	40
#define CA_PCI_PIO_OP_STAT		(1ull << 47)
#define CA_PDMA_OPQ_AVAIL		(0xfull << 48)
#define CA_PDMA_OPQ_AVAIL_SHFT		48
#define CA_PDMA_WRBUFF_AVAIL		(0xfull << 52)
#define CA_PDMA_WRBUFF_AVAIL_SHFT	52
#define CA_PDMA_RDBUFF_AVAIL		(0x3ull << 56)
#define CA_PDMA_RDBUFF_AVAIL_SHFT	56
	

#define CA_GART_AP_ENB_AGP		(1ull << 0)
#define CA_GART_PAGE_SIZE		(1ull << 1)
#define CA_GART_AP_ENB_PCI		(1ull << 2)
	
#define CA_GART_AP_SIZE			(0x3ffull << 12)
#define CA_GART_AP_SIZE_SHFT		12
#define CA_GART_AP_BASE			(0x3ffffffffffull << 22)
#define CA_GART_AP_BASE_SHFT		22

	
#define CA_INT_DEST_ADDR		(0x7ffffffffffffull << 3)
#define CA_INT_DEST_ADDR_SHFT		3
	
#define CA_INT_DEST_VECT		(0xffull << 56)
#define CA_INT_DEST_VECT_SHFT		56

#define CA_PCI_ERR			(1ull << 0)
	
#define CA_GART_FETCH_ERR		(1ull << 4)
#define CA_GFX_WR_OVFLW			(1ull << 5)
#define CA_PIO_REQ_OVFLW		(1ull << 6)
#define CA_CRM_PKTERR			(1ull << 7)
#define CA_CRM_DVERR			(1ull << 8)
#define CA_TNUMTO			(1ull << 9)
#define CA_CXM_RSP_CRED_OVFLW		(1ull << 10)
#define CA_CXM_REQ_CRED_OVFLW		(1ull << 11)
#define CA_PIO_INVALID_ADDR		(1ull << 12)
#define CA_PCI_ARB_TO			(1ull << 13)
#define CA_AGP_REQ_OFLOW		(1ull << 14)
#define CA_SBA_TYPE1_ERR		(1ull << 15)
	
#define CA_INTA				(1ull << 17)
#define CA_INTB				(1ull << 18)
#define CA_MULT_INTA			(1ull << 19)
#define CA_MULT_INTB			(1ull << 20)
#define CA_GFX_CREDIT_OVFLW		(1ull << 21)
	

#define CA_CRM_PKTERR_SBERR_HDR		(1ull << 0)
#define CA_CRM_PKTERR_DIDN		(1ull << 1)
#define CA_CRM_PKTERR_PACTYPE		(1ull << 2)
#define CA_CRM_PKTERR_INV_TNUM		(1ull << 3)
#define CA_CRM_PKTERR_ADDR_RNG		(1ull << 4)
#define CA_CRM_PKTERR_ADDR_ALGN		(1ull << 5)
#define CA_CRM_PKTERR_HDR_PARAM		(1ull << 6)
#define CA_CRM_PKTERR_CW_ERR		(1ull << 7)
#define CA_CRM_PKTERR_SBERR_NH		(1ull << 8)
#define CA_CRM_PKTERR_EARLY_TERM	(1ull << 9)
#define CA_CRM_PKTERR_EARLY_TAIL	(1ull << 10)
#define CA_CRM_PKTERR_MSSNG_TAIL	(1ull << 11)
#define CA_CRM_PKTERR_MSSNG_HDR		(1ull << 12)
	
#define CA_FIRST_CRM_PKTERR_SBERR_HDR	(1ull << 16)
#define CA_FIRST_CRM_PKTERR_DIDN	(1ull << 17)
#define CA_FIRST_CRM_PKTERR_PACTYPE	(1ull << 18)
#define CA_FIRST_CRM_PKTERR_INV_TNUM	(1ull << 19)
#define CA_FIRST_CRM_PKTERR_ADDR_RNG	(1ull << 20)
#define CA_FIRST_CRM_PKTERR_ADDR_ALGN	(1ull << 21)
#define CA_FIRST_CRM_PKTERR_HDR_PARAM	(1ull << 22)
#define CA_FIRST_CRM_PKTERR_CW_ERR	(1ull << 23)
#define CA_FIRST_CRM_PKTERR_SBERR_NH	(1ull << 24)
#define CA_FIRST_CRM_PKTERR_EARLY_TERM	(1ull << 25)
#define CA_FIRST_CRM_PKTERR_EARLY_TAIL	(1ull << 26)
#define CA_FIRST_CRM_PKTERR_MSSNG_TAIL	(1ull << 27)
#define CA_FIRST_CRM_PKTERR_MSSNG_HDR	(1ull << 28)
	

#define CA_PKT_TYPE			(0xfull << 0)
#define CA_PKT_TYPE_SHFT		0
#define CA_SRC_ID			(0x3ull << 4)
#define CA_SRC_ID_SHFT			4
#define CA_DATA_SZ			(0x3ull << 6)
#define CA_DATA_SZ_SHFT			6
#define CA_TNUM				(0xffull << 8)
#define CA_TNUM_SHFT			8
#define CA_DW_DATA_EN			(0xffull << 16)
#define CA_DW_DATA_EN_SHFT		16
#define CA_GFX_CRED			(0xffull << 24)
#define CA_GFX_CRED_SHFT		24
#define CA_MEM_RD_PARAM			(0x3ull << 32)
#define CA_MEM_RD_PARAM_SHFT		32
#define CA_PIO_OP			(1ull << 34)
#define CA_CW_ERR			(1ull << 35)
	
#define CA_VALID			(1ull << 63)

	
#define CA_PKT_ADDR			(0x1fffffffffffffull << 3)
#define CA_PKT_ADDR_SHFT		3
	

#define CA_CRM_TNUMTO_VAL		(0xffull << 0)
#define CA_CRM_TNUMTO_VAL_SHFT		0
#define CA_CRM_TNUMTO_WR		(1ull << 8)
	

#define CA_GART_ERR_SOURCE		(0x3ull << 0)
#define CA_GART_ERR_SOURCE_SHFT		0
	
#define CA_GART_ERR_ADDR		(0xfffffffffull << 4)
#define CA_GART_ERR_ADDR_SHFT		4
	

#define CA_PCIERR_DATA			(0xffffffffull << 0)
#define CA_PCIERR_DATA_SHFT		0
#define CA_PCIERR_ENB			(0xfull << 32)
#define CA_PCIERR_ENB_SHFT		32
#define CA_PCIERR_CMD			(0xfull << 36)
#define CA_PCIERR_CMD_SHFT		36
#define CA_PCIERR_A64			(1ull << 40)
#define CA_PCIERR_SLV_SERR		(1ull << 41)
#define CA_PCIERR_SLV_WR_PERR		(1ull << 42)
#define CA_PCIERR_SLV_RD_PERR		(1ull << 43)
#define CA_PCIERR_MST_SERR		(1ull << 44)
#define CA_PCIERR_MST_WR_PERR		(1ull << 45)
#define CA_PCIERR_MST_RD_PERR		(1ull << 46)
#define CA_PCIERR_MST_MABT		(1ull << 47)
#define CA_PCIERR_MST_TABT		(1ull << 48)
#define CA_PCIERR_MST_RETRY_TOUT	(1ull << 49)

#define CA_PCIERR_TYPES \
	(CA_PCIERR_A64|CA_PCIERR_SLV_SERR| \
	 CA_PCIERR_SLV_WR_PERR|CA_PCIERR_SLV_RD_PERR| \
	 CA_PCIERR_MST_SERR|CA_PCIERR_MST_WR_PERR|CA_PCIERR_MST_RD_PERR| \
	 CA_PCIERR_MST_MABT|CA_PCIERR_MST_TABT|CA_PCIERR_MST_RETRY_TOUT)

	

#define CA_UPPER_NODE_OFFSET		(0x3full << 0)
#define CA_UPPER_NODE_OFFSET_SHFT	0
	
#define CA_CHIPLET_ID			(0x3ull << 8)
#define CA_CHIPLET_ID_SHFT		8
	
#define CA_PCI_DMA_NODE_ID		(0xffffull << 12)
#define CA_PCI_DMA_NODE_ID_SHFT		12
	
#define CA_PCI_DMA_PIO_MEM_TYPE		(1ull << 28)
	


	
#define CA_AGP_DMA_NODE_ID		(0xffffull << 20)
#define CA_AGP_DMA_NODE_ID_SHFT		20
	
#define CA_AGP_DMA_PIO_MEM_TYPE		(1ull << 28)
	

#define CA_DEBUG_MN_VSEL		(0xfull << 0)
#define CA_DEBUG_MN_VSEL_SHFT		0
#define CA_DEBUG_PP_VSEL		(0xfull << 4)
#define CA_DEBUG_PP_VSEL_SHFT		4
#define CA_DEBUG_GW_VSEL		(0xfull << 8)
#define CA_DEBUG_GW_VSEL_SHFT		8
#define CA_DEBUG_GT_VSEL		(0xfull << 12)
#define CA_DEBUG_GT_VSEL_SHFT		12
#define CA_DEBUG_PD_VSEL		(0xfull << 16)
#define CA_DEBUG_PD_VSEL_SHFT		16
#define CA_DEBUG_AD_VSEL		(0xfull << 20)
#define CA_DEBUG_AD_VSEL_SHFT		20
#define CA_DEBUG_CX_VSEL		(0xfull << 24)
#define CA_DEBUG_CX_VSEL_SHFT		24
#define CA_DEBUG_CR_VSEL		(0xfull << 28)
#define CA_DEBUG_CR_VSEL_SHFT		28
#define CA_DEBUG_BA_VSEL		(0xfull << 32)
#define CA_DEBUG_BA_VSEL_SHFT		32
#define CA_DEBUG_PE_VSEL		(0xfull << 36)
#define CA_DEBUG_PE_VSEL_SHFT		36
#define CA_DEBUG_BO_VSEL		(0xfull << 40)
#define CA_DEBUG_BO_VSEL_SHFT		40
#define CA_DEBUG_BI_VSEL		(0xfull << 44)
#define CA_DEBUG_BI_VSEL_SHFT		44
#define CA_DEBUG_AS_VSEL		(0xfull << 48)
#define CA_DEBUG_AS_VSEL_SHFT		48
#define CA_DEBUG_PS_VSEL		(0xfull << 52)
#define CA_DEBUG_PS_VSEL_SHFT		52
#define CA_DEBUG_PM_VSEL		(0xfull << 56)
#define CA_DEBUG_PM_VSEL_SHFT		56
	

#define CA_DEBUG_MSEL0			(0x7ull << 0)
#define CA_DEBUG_MSEL0_SHFT		0
	
#define CA_DEBUG_NSEL0			(0x7ull << 4)
#define CA_DEBUG_NSEL0_SHFT		4
	
#define CA_DEBUG_MSEL1			(0x7ull << 8)
#define CA_DEBUG_MSEL1_SHFT		8
	
#define CA_DEBUG_NSEL1			(0x7ull << 12)
#define CA_DEBUG_NSEL1_SHFT		12
	
#define CA_DEBUG_MSEL2			(0x7ull << 16)
#define CA_DEBUG_MSEL2_SHFT		16
	
#define CA_DEBUG_NSEL2			(0x7ull << 20)
#define CA_DEBUG_NSEL2_SHFT		20
	
#define CA_DEBUG_MSEL3			(0x7ull << 24)
#define CA_DEBUG_MSEL3_SHFT		24
	
#define CA_DEBUG_NSEL3			(0x7ull << 28)
#define CA_DEBUG_NSEL3_SHFT		28
	
#define CA_DEBUG_MSEL4			(0x7ull << 32)
#define CA_DEBUG_MSEL4_SHFT		32
	
#define CA_DEBUG_NSEL4			(0x7ull << 36)
#define CA_DEBUG_NSEL4_SHFT		36
	
#define CA_DEBUG_MSEL5			(0x7ull << 40)
#define CA_DEBUG_MSEL5_SHFT		40
	
#define CA_DEBUG_NSEL5			(0x7ull << 44)
#define CA_DEBUG_NSEL5_SHFT		44
	
#define CA_DEBUG_MSEL6			(0x7ull << 48)
#define CA_DEBUG_MSEL6_SHFT		48
	
#define CA_DEBUG_NSEL6			(0x7ull << 52)
#define CA_DEBUG_NSEL6_SHFT		52
	
#define CA_DEBUG_MSEL7			(0x7ull << 56)
#define CA_DEBUG_MSEL7_SHFT		56
	
#define CA_DEBUG_NSEL7			(0x7ull << 60)
#define CA_DEBUG_NSEL7_SHFT		60
	


#define CA_DEBUG_DOMAIN_L		(1ull << 0)
#define CA_DEBUG_DOMAIN_H		(1ull << 1)
	

#define CA_GART_PTR_VAL			(1ull << 0)
	
#define CA_GART_PTR_ADDR		(0xfffffffffffull << 12)
#define CA_GART_PTR_ADDR_SHFT		12
	

#define CA_GART_TLB_ADDR		(0xffffffffffffffull << 0)
#define CA_GART_TLB_ADDR_SHFT		0
	
#define CA_GART_TLB_ENTRY_VAL		(1ull << 63)


#define CA_PIO_ADMIN			0x00000000
#define CA_PIO_ADMIN_LEN		0x00010000

#define CA_PIO_GFX			0x00010000
#define CA_PIO_GFX_LEN			0x00010000

#define CA_PIO_AGP_DMAWRITE		0x00020000
#define CA_PIO_AGP_DMAWRITE_LEN		0x00010000

#define CA_PIO_AGP_DMAREAD		0x00030000
#define CA_PIO_AGP_DMAREAD_LEN		0x00010000

#define CA_PIO_PCI_TYPE0_CONFIG		0x01000000
#define CA_PIO_PCI_TYPE0_CONFIG_LEN	0x01000000

#define CA_PIO_PCI_TYPE1_CONFIG		0x02000000
#define CA_PIO_PCI_TYPE1_CONFIG_LEN	0x01000000

#define CA_PIO_PCI_IO			0x03000000
#define CA_PIO_PCI_IO_LEN		0x05000000

#define CA_PIO_PCI_MEM_OFFSET		0x08000000
#define CA_PIO_PCI_MEM_OFFSET_LEN	0x08000000

#define CA_PIO_PCI_MEM			0x40000000
#define CA_PIO_PCI_MEM_LEN		0xc0000000





#define CA_PCI32_DIRECT_BASE	0xC0000000UL	
#define CA_PCI32_DIRECT_SIZE	0x00000000UL	

#define CA_PCI32_MAPPED_BASE	0xC0000000UL
#define CA_PCI32_MAPPED_SIZE	0x40000000UL	

#define CA_AGP_MAPPED_BASE	0x80000000UL
#define CA_AGP_MAPPED_SIZE	0x40000000UL	

#define CA_AGP_DIRECT_BASE	0x40000000UL	
#define CA_AGP_DIRECT_SIZE	0x40000000UL

#define CA_APERATURE_BASE	(CA_AGP_MAPPED_BASE)
#define CA_APERATURE_SIZE	(CA_AGP_MAPPED_SIZE+CA_PCI32_MAPPED_SIZE)

#endif  
