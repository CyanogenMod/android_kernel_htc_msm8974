#ifndef _ASM_IA64_PAL_H
#define _ASM_IA64_PAL_H

/*
 * Processor Abstraction Layer definitions.
 *
 * This is based on Intel IA-64 Architecture Software Developer's Manual rev 1.0
 * chapter 11 IA-64 Processor Abstraction Layer
 *
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *	Stephane Eranian <eranian@hpl.hp.com>
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 * Copyright (C) 1999 Srinivasa Prasad Thirumalachar <sprasad@sprasad.engr.sgi.com>
 * Copyright (C) 2008 Silicon Graphics, Inc. (SGI)
 *
 * 99/10/01	davidm	Make sure we pass zero for reserved parameters.
 * 00/03/07	davidm	Updated pal_cache_flush() to be in sync with PAL v2.6.
 * 00/03/23     cfleck  Modified processor min-state save area to match updated PAL & SAL info
 * 00/05/24     eranian Updated to latest PAL spec, fix structures bugs, added
 * 00/05/25	eranian Support for stack calls, and static physical calls
 * 00/06/18	eranian Support for stacked physical calls
 * 06/10/26	rja	Support for Intel Itanium Architecture Software Developer's
 *			Manual Rev 2.2 (Jan 2006)
 */

#define PAL_CACHE_FLUSH		1	
#define PAL_CACHE_INFO		2	
#define PAL_CACHE_INIT		3	
#define PAL_CACHE_SUMMARY	4	
#define PAL_MEM_ATTRIB		5	
#define PAL_PTCE_INFO		6	
#define PAL_VM_INFO		7	
#define PAL_VM_SUMMARY		8	
#define PAL_BUS_GET_FEATURES	9	
#define PAL_BUS_SET_FEATURES	10	
#define PAL_DEBUG_INFO		11	
#define PAL_FIXED_ADDR		12	
#define PAL_FREQ_BASE		13	
#define PAL_FREQ_RATIOS		14	
#define PAL_PERF_MON_INFO	15	
#define PAL_PLATFORM_ADDR	16	
#define PAL_PROC_GET_FEATURES	17	
#define PAL_PROC_SET_FEATURES	18	
#define PAL_RSE_INFO		19	
#define PAL_VERSION		20	
#define PAL_MC_CLEAR_LOG	21	
#define PAL_MC_DRAIN		22	
#define PAL_MC_EXPECTED		23	
#define PAL_MC_DYNAMIC_STATE	24	
#define PAL_MC_ERROR_INFO	25	
#define PAL_MC_RESUME		26	
#define PAL_MC_REGISTER_MEM	27	
#define PAL_HALT		28	
#define PAL_HALT_LIGHT		29	
#define PAL_COPY_INFO		30	
#define PAL_CACHE_LINE_INIT	31	
#define PAL_PMI_ENTRYPOINT	32	
#define PAL_ENTER_IA_32_ENV	33	
#define PAL_VM_PAGE_SIZE	34	

#define PAL_MEM_FOR_TEST	37	
#define PAL_CACHE_PROT_INFO	38	
#define PAL_REGISTER_INFO	39	
#define PAL_SHUTDOWN		40	
#define PAL_PREFETCH_VISIBILITY	41	
#define PAL_LOGICAL_TO_PHYSICAL 42	
#define PAL_CACHE_SHARED_INFO	43	
#define PAL_GET_HW_POLICY	48	
#define PAL_SET_HW_POLICY	49	
#define PAL_VP_INFO		50	
#define PAL_MC_HW_TRACKING	51	

#define PAL_COPY_PAL		256	
#define PAL_HALT_INFO		257	
#define PAL_TEST_PROC		258	
#define PAL_CACHE_READ		259	
#define PAL_CACHE_WRITE		260	
#define PAL_VM_TR_READ		261	
#define PAL_GET_PSTATE		262	
#define PAL_SET_PSTATE		263	
#define PAL_BRAND_INFO		274	

#define PAL_GET_PSTATE_TYPE_LASTSET	0
#define PAL_GET_PSTATE_TYPE_AVGANDRESET	1
#define PAL_GET_PSTATE_TYPE_AVGNORESET	2
#define PAL_GET_PSTATE_TYPE_INSTANT	3

#define PAL_MC_ERROR_INJECT	276	

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/fpu.h>


typedef s64				pal_status_t;

#define PAL_STATUS_SUCCESS		0	
#define PAL_STATUS_UNIMPLEMENTED	(-1)	
#define PAL_STATUS_EINVAL		(-2)	
#define PAL_STATUS_ERROR		(-3)	
#define PAL_STATUS_CACHE_INIT_FAIL	(-4)	
#define PAL_STATUS_REQUIRES_MEMORY	(-9)	

typedef u64				pal_cache_level_t;
#define PAL_CACHE_LEVEL_L0		0	
#define PAL_CACHE_LEVEL_L1		1	
#define PAL_CACHE_LEVEL_L2		2	



typedef u64				pal_cache_type_t;
#define PAL_CACHE_TYPE_INSTRUCTION	1	
#define PAL_CACHE_TYPE_DATA		2	
#define PAL_CACHE_TYPE_INSTRUCTION_DATA	3	


#define PAL_CACHE_FLUSH_INVALIDATE	1	
#define PAL_CACHE_FLUSH_CHK_INTRS	2	

typedef int				pal_cache_line_size_t;

typedef u64				pal_cache_line_state_t;
#define PAL_CACHE_LINE_STATE_INVALID	0	
#define PAL_CACHE_LINE_STATE_SHARED	1	
#define PAL_CACHE_LINE_STATE_EXCLUSIVE	2	
#define PAL_CACHE_LINE_STATE_MODIFIED	3	

typedef struct pal_freq_ratio {
	u32 den, num;		
} itc_ratio, proc_ratio;

typedef	union  pal_cache_config_info_1_s {
	struct {
		u64		u		: 1,	
				at		: 2,	
				reserved	: 5,	
				associativity	: 8,	
				line_size	: 8,	
				stride		: 8,	
				store_latency	: 8,	
				load_latency	: 8,	
				store_hints	: 8,	
				load_hints	: 8;	
	} pcci1_bits;
	u64			pcci1_data;
} pal_cache_config_info_1_t;

typedef	union  pal_cache_config_info_2_s {
	struct {
		u32		cache_size;		


		u32		alias_boundary	: 8,	
				tag_ls_bit	: 8,	
				tag_ms_bit	: 8,	
				reserved	: 8;	
	} pcci2_bits;
	u64			pcci2_data;
} pal_cache_config_info_2_t;


typedef struct pal_cache_config_info_s {
	pal_status_t			pcci_status;
	pal_cache_config_info_1_t	pcci_info_1;
	pal_cache_config_info_2_t	pcci_info_2;
	u64				pcci_reserved;
} pal_cache_config_info_t;

#define pcci_ld_hints		pcci_info_1.pcci1_bits.load_hints
#define pcci_st_hints		pcci_info_1.pcci1_bits.store_hints
#define pcci_ld_latency		pcci_info_1.pcci1_bits.load_latency
#define pcci_st_latency		pcci_info_1.pcci1_bits.store_latency
#define pcci_stride		pcci_info_1.pcci1_bits.stride
#define pcci_line_size		pcci_info_1.pcci1_bits.line_size
#define pcci_assoc		pcci_info_1.pcci1_bits.associativity
#define pcci_cache_attr		pcci_info_1.pcci1_bits.at
#define pcci_unified		pcci_info_1.pcci1_bits.u
#define pcci_tag_msb		pcci_info_2.pcci2_bits.tag_ms_bit
#define pcci_tag_lsb		pcci_info_2.pcci2_bits.tag_ls_bit
#define pcci_alias_boundary	pcci_info_2.pcci2_bits.alias_boundary
#define pcci_cache_size		pcci_info_2.pcci2_bits.cache_size




#define PAL_CACHE_ATTR_WT		0	
#define PAL_CACHE_ATTR_WB		1	
#define PAL_CACHE_ATTR_WT_OR_WB		2	



#define PAL_CACHE_HINT_TEMP_1		0	
#define PAL_CACHE_HINT_NTEMP_1		1	
#define PAL_CACHE_HINT_NTEMP_ALL	3	

typedef union pal_cache_protection_element_u {
	u32			pcpi_data;
	struct {
		u32		data_bits	: 8, 

				tagprot_lsb	: 6, 
				tagprot_msb	: 6, 
				prot_bits	: 6, 
				method		: 4, 
				t_d		: 2; 
	} pcp_info;
} pal_cache_protection_element_t;

#define pcpi_cache_prot_part	pcp_info.t_d
#define pcpi_prot_method	pcp_info.method
#define pcpi_prot_bits		pcp_info.prot_bits
#define pcpi_tagprot_msb	pcp_info.tagprot_msb
#define pcpi_tagprot_lsb	pcp_info.tagprot_lsb
#define pcpi_data_bits		pcp_info.data_bits

#define PAL_CACHE_PROT_PART_DATA	0	
#define PAL_CACHE_PROT_PART_TAG		1	
#define PAL_CACHE_PROT_PART_TAG_DATA	2	
#define PAL_CACHE_PROT_PART_DATA_TAG	3	
#define PAL_CACHE_PROT_PART_MAX		6


typedef struct pal_cache_protection_info_s {
	pal_status_t			pcpi_status;
	pal_cache_protection_element_t	pcp_info[PAL_CACHE_PROT_PART_MAX];
} pal_cache_protection_info_t;


#define PAL_CACHE_PROT_METHOD_NONE		0	
#define PAL_CACHE_PROT_METHOD_ODD_PARITY	1	
#define PAL_CACHE_PROT_METHOD_EVEN_PARITY	2	
#define PAL_CACHE_PROT_METHOD_ECC		3	


typedef union pal_cache_line_id_u {
	u64			pclid_data;
	struct {
		u64		cache_type	: 8,	
				level		: 8,	
				way		: 8,	
				part		: 8,	
				reserved	: 32;	
	} pclid_info_read;
	struct {
		u64		cache_type	: 8,	
				level		: 8,	
				way		: 8,	
				part		: 8,	
				mesi		: 8,	
				start		: 8,	
				length		: 8,	
				trigger		: 8;	

	} pclid_info_write;
} pal_cache_line_id_u_t;

#define pclid_read_part		pclid_info_read.part
#define pclid_read_way		pclid_info_read.way
#define pclid_read_level	pclid_info_read.level
#define pclid_read_cache_type	pclid_info_read.cache_type

#define pclid_write_trigger	pclid_info_write.trigger
#define pclid_write_length	pclid_info_write.length
#define pclid_write_start	pclid_info_write.start
#define pclid_write_mesi	pclid_info_write.mesi
#define pclid_write_part	pclid_info_write.part
#define pclid_write_way		pclid_info_write.way
#define pclid_write_level	pclid_info_write.level
#define pclid_write_cache_type	pclid_info_write.cache_type

#define PAL_CACHE_LINE_ID_PART_DATA		0	
#define PAL_CACHE_LINE_ID_PART_TAG		1	
#define PAL_CACHE_LINE_ID_PART_DATA_PROT	2	
#define PAL_CACHE_LINE_ID_PART_TAG_PROT		3	
#define PAL_CACHE_LINE_ID_PART_DATA_TAG_PROT	4	
typedef struct pal_cache_line_info_s {
	pal_status_t		pcli_status;		
	u64			pcli_data;		
	u64			pcli_data_len;		
	pal_cache_line_state_t	pcli_cache_line_state;	

} pal_cache_line_info_t;



typedef u64					pal_mc_pending_events_t;

#define PAL_MC_PENDING_MCA			(1 << 0)
#define PAL_MC_PENDING_INIT			(1 << 1)

typedef u64					pal_mc_info_index_t;

#define PAL_MC_INFO_PROCESSOR			0	
#define PAL_MC_INFO_CACHE_CHECK			1	
#define PAL_MC_INFO_TLB_CHECK			2	
#define PAL_MC_INFO_BUS_CHECK			3	
#define PAL_MC_INFO_REQ_ADDR			4	
#define PAL_MC_INFO_RESP_ADDR			5	
#define PAL_MC_INFO_TARGET_ADDR			6	
#define PAL_MC_INFO_IMPL_DEP			7	

#define PAL_TLB_CHECK_OP_PURGE			8

typedef struct pal_process_state_info_s {
	u64		reserved1	: 2,
			rz		: 1,	

			ra		: 1,	
			me		: 1,	

			mn		: 1,	

			sy		: 1,	


			co		: 1,	
			ci		: 1,	
			us		: 1,	


			hd		: 1,	

			tl		: 1,	
			mi		: 1,	
			pi		: 1,	
			pm		: 1,	

			dy		: 1,	


			in		: 1,	
			rs		: 1,	
			cm		: 1,	
			ex		: 1,	
			cr		: 1,	
			pc		: 1,	
			dr		: 1,	
			tr		: 1,	
			rr		: 1,	
			ar		: 1,	
			br		: 1,	
			pr		: 1,	

			fp		: 1,	
			b1		: 1,	
			b0		: 1,	
			gr		: 1,	
			dsize		: 16,	

			se		: 1,	
			reserved2	: 10,
			cc		: 1,	
			tc		: 1,	
			bc		: 1,	
			rc		: 1,	
			uc		: 1;	

} pal_processor_state_info_t;

typedef struct pal_cache_check_info_s {
	u64		op		: 4,	
			level		: 2,	
			reserved1	: 2,
			dl		: 1,	
			tl		: 1,	
			dc		: 1,	
			ic		: 1,	
			mesi		: 3,	
			mv		: 1,	
			way		: 5,	
			wiv		: 1,	
			reserved2	: 1,
			dp		: 1,	
			reserved3	: 6,
			hlth		: 2,	

			index		: 20,	
			reserved4	: 2,

			is		: 1,	
			iv		: 1,	
			pl		: 2,	
			pv		: 1,	
			mcc		: 1,	
			tv		: 1,	
			rq		: 1,	
			rp		: 1,	
			pi		: 1;	
} pal_cache_check_info_t;

typedef struct pal_tlb_check_info_s {

	u64		tr_slot		: 8,	
			trv		: 1,	
			reserved1	: 1,
			level		: 2,	
			reserved2	: 4,
			dtr		: 1,	
			itr		: 1,	
			dtc		: 1,	
			itc		: 1,	
			op		: 4,	
			reserved3	: 6,
			hlth		: 2,	
			reserved4	: 22,

			is		: 1,	
			iv		: 1,	
			pl		: 2,	
			pv		: 1,	
			mcc		: 1,	
			tv		: 1,	
			rq		: 1,	
			rp		: 1,	
			pi		: 1;	
} pal_tlb_check_info_t;

typedef struct pal_bus_check_info_s {
	u64		size		: 5,	
			ib		: 1,	
			eb		: 1,	
			cc		: 1,	
			type		: 8,	
			sev		: 5,	
			hier		: 2,	
			dp		: 1,	
			bsi		: 8,	
			reserved2	: 22,

			is		: 1,	
			iv		: 1,	
			pl		: 2,	
			pv		: 1,	
			mcc		: 1,	
			tv		: 1,	
			rq		: 1,	
			rp		: 1,	
			pi		: 1;	
} pal_bus_check_info_t;

typedef struct pal_reg_file_check_info_s {
	u64		id		: 4,	
			op		: 4,	
			reg_num		: 7,	
			rnv		: 1,	
			reserved2	: 38,

			is		: 1,	
			iv		: 1,	
			pl		: 2,	
			pv		: 1,	
			mcc		: 1,	
			reserved3	: 3,
			pi		: 1;	
} pal_reg_file_check_info_t;

typedef struct pal_uarch_check_info_s {
	u64		sid		: 5,	
			level		: 3,	
			array_id	: 4,	
			op		: 4,	
			way		: 6,	
			wv		: 1,	
			xv		: 1,	
			reserved1	: 6,
			hlth		: 2,	
			index		: 8,	
			reserved2	: 24,

			is		: 1,	
			iv		: 1,	
			pl		: 2,	
			pv		: 1,	
			mcc		: 1,	
			tv		: 1,	
			rq		: 1,	
			rp		: 1,	
			pi		: 1;	
} pal_uarch_check_info_t;

typedef union pal_mc_error_info_u {
	u64				pmei_data;
	pal_processor_state_info_t	pme_processor;
	pal_cache_check_info_t		pme_cache;
	pal_tlb_check_info_t		pme_tlb;
	pal_bus_check_info_t		pme_bus;
	pal_reg_file_check_info_t	pme_reg_file;
	pal_uarch_check_info_t		pme_uarch;
} pal_mc_error_info_t;

#define pmci_proc_unknown_check			pme_processor.uc
#define pmci_proc_bus_check			pme_processor.bc
#define pmci_proc_tlb_check			pme_processor.tc
#define pmci_proc_cache_check			pme_processor.cc
#define pmci_proc_dynamic_state_size		pme_processor.dsize
#define pmci_proc_gpr_valid			pme_processor.gr
#define pmci_proc_preserved_bank0_gpr_valid	pme_processor.b0
#define pmci_proc_preserved_bank1_gpr_valid	pme_processor.b1
#define pmci_proc_fp_valid			pme_processor.fp
#define pmci_proc_predicate_regs_valid		pme_processor.pr
#define pmci_proc_branch_regs_valid		pme_processor.br
#define pmci_proc_app_regs_valid		pme_processor.ar
#define pmci_proc_region_regs_valid		pme_processor.rr
#define pmci_proc_translation_regs_valid	pme_processor.tr
#define pmci_proc_debug_regs_valid		pme_processor.dr
#define pmci_proc_perf_counters_valid		pme_processor.pc
#define pmci_proc_control_regs_valid		pme_processor.cr
#define pmci_proc_machine_check_expected	pme_processor.ex
#define pmci_proc_machine_check_corrected	pme_processor.cm
#define pmci_proc_rse_valid			pme_processor.rs
#define pmci_proc_machine_check_or_init		pme_processor.in
#define pmci_proc_dynamic_state_valid		pme_processor.dy
#define pmci_proc_operation			pme_processor.op
#define pmci_proc_trap_lost			pme_processor.tl
#define pmci_proc_hardware_damage		pme_processor.hd
#define pmci_proc_uncontained_storage_damage	pme_processor.us
#define pmci_proc_machine_check_isolated	pme_processor.ci
#define pmci_proc_continuable			pme_processor.co
#define pmci_proc_storage_intergrity_synced	pme_processor.sy
#define pmci_proc_min_state_save_area_regd	pme_processor.mn
#define	pmci_proc_distinct_multiple_errors	pme_processor.me
#define pmci_proc_pal_attempted_rendezvous	pme_processor.ra
#define pmci_proc_pal_rendezvous_complete	pme_processor.rz


#define pmci_cache_level			pme_cache.level
#define pmci_cache_line_state			pme_cache.mesi
#define pmci_cache_line_state_valid		pme_cache.mv
#define pmci_cache_line_index			pme_cache.index
#define pmci_cache_instr_cache_fail		pme_cache.ic
#define pmci_cache_data_cache_fail		pme_cache.dc
#define pmci_cache_line_tag_fail		pme_cache.tl
#define pmci_cache_line_data_fail		pme_cache.dl
#define pmci_cache_operation			pme_cache.op
#define pmci_cache_way_valid			pme_cache.wv
#define pmci_cache_target_address_valid		pme_cache.tv
#define pmci_cache_way				pme_cache.way
#define pmci_cache_mc				pme_cache.mc

#define pmci_tlb_instr_translation_cache_fail	pme_tlb.itc
#define pmci_tlb_data_translation_cache_fail	pme_tlb.dtc
#define pmci_tlb_instr_translation_reg_fail	pme_tlb.itr
#define pmci_tlb_data_translation_reg_fail	pme_tlb.dtr
#define pmci_tlb_translation_reg_slot		pme_tlb.tr_slot
#define pmci_tlb_mc				pme_tlb.mc

#define pmci_bus_status_info			pme_bus.bsi
#define pmci_bus_req_address_valid		pme_bus.rq
#define pmci_bus_resp_address_valid		pme_bus.rp
#define pmci_bus_target_address_valid		pme_bus.tv
#define pmci_bus_error_severity			pme_bus.sev
#define pmci_bus_transaction_type		pme_bus.type
#define pmci_bus_cache_cache_transfer		pme_bus.cc
#define pmci_bus_transaction_size		pme_bus.size
#define pmci_bus_internal_error			pme_bus.ib
#define pmci_bus_external_error			pme_bus.eb
#define pmci_bus_mc				pme_bus.mc


typedef struct pal_min_state_area_s {
	u64	pmsa_nat_bits;		
	u64	pmsa_gr[15];		
	u64	pmsa_bank0_gr[16];	
	u64	pmsa_bank1_gr[16];	
	u64	pmsa_pr;		
	u64	pmsa_br0;		
	u64	pmsa_rsc;		
	u64	pmsa_iip;		
	u64	pmsa_ipsr;		
	u64	pmsa_ifs;		
	u64	pmsa_xip;		
	u64	pmsa_xpsr;		
	u64	pmsa_xfs;		
	u64	pmsa_br1;		
	u64	pmsa_reserved[70];	
} pal_min_state_area_t;


struct ia64_pal_retval {
	s64 status;
	u64 v0;
	u64 v1;
	u64 v2;
};

extern struct ia64_pal_retval ia64_pal_call_static (u64, u64, u64, u64);
extern struct ia64_pal_retval ia64_pal_call_stacked (u64, u64, u64, u64);
extern struct ia64_pal_retval ia64_pal_call_phys_static (u64, u64, u64, u64);
extern struct ia64_pal_retval ia64_pal_call_phys_stacked (u64, u64, u64, u64);
extern void ia64_save_scratch_fpregs (struct ia64_fpreg *);
extern void ia64_load_scratch_fpregs (struct ia64_fpreg *);

#define PAL_CALL(iprv,a0,a1,a2,a3) do {			\
	struct ia64_fpreg fr[6];			\
	ia64_save_scratch_fpregs(fr);			\
	iprv = ia64_pal_call_static(a0, a1, a2, a3);	\
	ia64_load_scratch_fpregs(fr);			\
} while (0)

#define PAL_CALL_STK(iprv,a0,a1,a2,a3) do {		\
	struct ia64_fpreg fr[6];			\
	ia64_save_scratch_fpregs(fr);			\
	iprv = ia64_pal_call_stacked(a0, a1, a2, a3);	\
	ia64_load_scratch_fpregs(fr);			\
} while (0)

#define PAL_CALL_PHYS(iprv,a0,a1,a2,a3) do {			\
	struct ia64_fpreg fr[6];				\
	ia64_save_scratch_fpregs(fr);				\
	iprv = ia64_pal_call_phys_static(a0, a1, a2, a3);	\
	ia64_load_scratch_fpregs(fr);				\
} while (0)

#define PAL_CALL_PHYS_STK(iprv,a0,a1,a2,a3) do {		\
	struct ia64_fpreg fr[6];				\
	ia64_save_scratch_fpregs(fr);				\
	iprv = ia64_pal_call_phys_stacked(a0, a1, a2, a3);	\
	ia64_load_scratch_fpregs(fr);				\
} while (0)

typedef int (*ia64_pal_handler) (u64, ...);
extern ia64_pal_handler ia64_pal;
extern void ia64_pal_handler_init (void *);

extern ia64_pal_handler ia64_pal;

extern pal_cache_config_info_t		l0d_cache_config_info;
extern pal_cache_config_info_t		l0i_cache_config_info;
extern pal_cache_config_info_t		l1_cache_config_info;
extern pal_cache_config_info_t		l2_cache_config_info;

extern pal_cache_protection_info_t	l0d_cache_protection_info;
extern pal_cache_protection_info_t	l0i_cache_protection_info;
extern pal_cache_protection_info_t	l1_cache_protection_info;
extern pal_cache_protection_info_t	l2_cache_protection_info;

extern pal_cache_config_info_t		pal_cache_config_info_get(pal_cache_level_t,
								  pal_cache_type_t);

extern pal_cache_protection_info_t	pal_cache_protection_info_get(pal_cache_level_t,
								      pal_cache_type_t);


extern void				pal_error(int);



typedef union pal_bus_features_u {
	u64	pal_bus_features_val;
	struct {
		u64	pbf_reserved1				:	29;
		u64	pbf_req_bus_parking			:	1;
		u64	pbf_bus_lock_mask			:	1;
		u64	pbf_enable_half_xfer_rate		:	1;
		u64	pbf_reserved2				:	20;
		u64	pbf_enable_shared_line_replace		:	1;
		u64	pbf_enable_exclusive_line_replace	:	1;
		u64	pbf_disable_xaction_queueing		:	1;
		u64	pbf_disable_resp_err_check		:	1;
		u64	pbf_disable_berr_check			:	1;
		u64	pbf_disable_bus_req_internal_err_signal	:	1;
		u64	pbf_disable_bus_req_berr_signal		:	1;
		u64	pbf_disable_bus_init_event_check	:	1;
		u64	pbf_disable_bus_init_event_signal	:	1;
		u64	pbf_disable_bus_addr_err_check		:	1;
		u64	pbf_disable_bus_addr_err_signal		:	1;
		u64	pbf_disable_bus_data_err_check		:	1;
	} pal_bus_features_s;
} pal_bus_features_u_t;

extern void pal_bus_features_print (u64);

static inline s64
ia64_pal_bus_get_features (pal_bus_features_u_t *features_avail,
			   pal_bus_features_u_t *features_status,
			   pal_bus_features_u_t *features_control)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS(iprv, PAL_BUS_GET_FEATURES, 0, 0, 0);
	if (features_avail)
		features_avail->pal_bus_features_val = iprv.v0;
	if (features_status)
		features_status->pal_bus_features_val = iprv.v1;
	if (features_control)
		features_control->pal_bus_features_val = iprv.v2;
	return iprv.status;
}

static inline s64
ia64_pal_bus_set_features (pal_bus_features_u_t feature_select)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS(iprv, PAL_BUS_SET_FEATURES, feature_select.pal_bus_features_val, 0, 0);
	return iprv.status;
}

static inline s64
ia64_pal_cache_config_info (u64 cache_level, u64 cache_type, pal_cache_config_info_t *conf)
{
	struct ia64_pal_retval iprv;

	PAL_CALL(iprv, PAL_CACHE_INFO, cache_level, cache_type, 0);

	if (iprv.status == 0) {
		conf->pcci_status                 = iprv.status;
		conf->pcci_info_1.pcci1_data      = iprv.v0;
		conf->pcci_info_2.pcci2_data      = iprv.v1;
		conf->pcci_reserved               = iprv.v2;
	}
	return iprv.status;

}

static inline s64
ia64_pal_cache_prot_info (u64 cache_level, u64 cache_type, pal_cache_protection_info_t *prot)
{
	struct ia64_pal_retval iprv;

	PAL_CALL(iprv, PAL_CACHE_PROT_INFO, cache_level, cache_type, 0);

	if (iprv.status == 0) {
		prot->pcpi_status           = iprv.status;
		prot->pcp_info[0].pcpi_data = iprv.v0 & 0xffffffff;
		prot->pcp_info[1].pcpi_data = iprv.v0 >> 32;
		prot->pcp_info[2].pcpi_data = iprv.v1 & 0xffffffff;
		prot->pcp_info[3].pcpi_data = iprv.v1 >> 32;
		prot->pcp_info[4].pcpi_data = iprv.v2 & 0xffffffff;
		prot->pcp_info[5].pcpi_data = iprv.v2 >> 32;
	}
	return iprv.status;
}

static inline s64
ia64_pal_cache_flush (u64 cache_type, u64 invalidate, u64 *progress, u64 *vector)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_CACHE_FLUSH, cache_type, invalidate, *progress);
	if (vector)
		*vector = iprv.v0;
	*progress = iprv.v1;
	return iprv.status;
}


static inline s64
ia64_pal_cache_init (u64 level, u64 cache_type, u64 rest)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_CACHE_INIT, level, cache_type, rest);
	return iprv.status;
}

static inline s64
ia64_pal_cache_line_init (u64 physical_addr, u64 data_value)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_CACHE_LINE_INIT, physical_addr, data_value, 0);
	return iprv.status;
}


static inline s64
ia64_pal_cache_read (pal_cache_line_id_u_t line_id, u64 physical_addr)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS_STK(iprv, PAL_CACHE_READ, line_id.pclid_data,
				physical_addr, 0);
	return iprv.status;
}

static inline long ia64_pal_cache_summary(unsigned long *cache_levels,
						unsigned long *unique_caches)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_CACHE_SUMMARY, 0, 0, 0);
	if (cache_levels)
		*cache_levels = iprv.v0;
	if (unique_caches)
		*unique_caches = iprv.v1;
	return iprv.status;
}

static inline s64
ia64_pal_cache_write (pal_cache_line_id_u_t line_id, u64 physical_addr, u64 data)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS_STK(iprv, PAL_CACHE_WRITE, line_id.pclid_data,
				physical_addr, data);
	return iprv.status;
}


static inline s64
ia64_pal_copy_info (u64 copy_type, u64 num_procs, u64 num_iopics,
		    u64 *buffer_size, u64 *buffer_align)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_COPY_INFO, copy_type, num_procs, num_iopics);
	if (buffer_size)
		*buffer_size = iprv.v0;
	if (buffer_align)
		*buffer_align = iprv.v1;
	return iprv.status;
}

static inline s64
ia64_pal_copy_pal (u64 target_addr, u64 alloc_size, u64 processor, u64 *pal_proc_offset)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_COPY_PAL, target_addr, alloc_size, processor);
	if (pal_proc_offset)
		*pal_proc_offset = iprv.v0;
	return iprv.status;
}

static inline long ia64_pal_debug_info(unsigned long *inst_regs,
						unsigned long *data_regs)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_DEBUG_INFO, 0, 0, 0);
	if (inst_regs)
		*inst_regs = iprv.v0;
	if (data_regs)
		*data_regs = iprv.v1;

	return iprv.status;
}

#ifdef TBD
static inline s64
ia64_pal_enter_ia32_env (ia32_env1, ia32_env2, ia32_env3)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_ENTER_IA_32_ENV, ia32_env1, ia32_env2, ia32_env3);
	return iprv.status;
}
#endif

static inline s64
ia64_pal_fixed_addr (u64 *global_unique_addr)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_FIXED_ADDR, 0, 0, 0);
	if (global_unique_addr)
		*global_unique_addr = iprv.v0;
	return iprv.status;
}

static inline long ia64_pal_freq_base(unsigned long *platform_base_freq)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_FREQ_BASE, 0, 0, 0);
	if (platform_base_freq)
		*platform_base_freq = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_freq_ratios (struct pal_freq_ratio *proc_ratio, struct pal_freq_ratio *bus_ratio,
		      struct pal_freq_ratio *itc_ratio)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_FREQ_RATIOS, 0, 0, 0);
	if (proc_ratio)
		*(u64 *)proc_ratio = iprv.v0;
	if (bus_ratio)
		*(u64 *)bus_ratio = iprv.v1;
	if (itc_ratio)
		*(u64 *)itc_ratio = iprv.v2;
	return iprv.status;
}

static inline s64
ia64_pal_get_hw_policy (u64 proc_num, u64 *cur_policy, u64 *num_impacted,
			u64 *la)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_GET_HW_POLICY, proc_num, 0, 0);
	if (cur_policy)
		*cur_policy = iprv.v0;
	if (num_impacted)
		*num_impacted = iprv.v1;
	if (la)
		*la = iprv.v2;
	return iprv.status;
}

static inline s64
ia64_pal_halt (u64 halt_state)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_HALT, halt_state, 0, 0);
	return iprv.status;
}

typedef union pal_power_mgmt_info_u {
	u64			ppmi_data;
	struct {
	       u64		exit_latency		: 16,
				entry_latency		: 16,
				power_consumption	: 28,
				im			: 1,
				co			: 1,
				reserved		: 2;
	} pal_power_mgmt_info_s;
} pal_power_mgmt_info_u_t;

static inline s64
ia64_pal_halt_info (pal_power_mgmt_info_u_t *power_buf)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_STK(iprv, PAL_HALT_INFO, (unsigned long) power_buf, 0, 0);
	return iprv.status;
}

static inline s64
ia64_pal_get_pstate (u64 *pstate_index, unsigned long type)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_STK(iprv, PAL_GET_PSTATE, type, 0, 0);
	*pstate_index = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_set_pstate (u64 pstate_index)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_STK(iprv, PAL_SET_PSTATE, pstate_index, 0, 0);
	return iprv.status;
}

static inline s64
ia64_pal_get_brand_info (char *brand_info)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_STK(iprv, PAL_BRAND_INFO, 0, (u64)brand_info, 0);
	return iprv.status;
}

static inline s64
ia64_pal_halt_light (void)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_HALT_LIGHT, 0, 0, 0);
	return iprv.status;
}

/* Clear all the processor error logging   registers and reset the indicator that allows
 * the error logging registers to be written. This procedure also checks the pending
 * machine check bit and pending INIT bit and reports their states.
 */
static inline s64
ia64_pal_mc_clear_log (u64 *pending_vector)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_CLEAR_LOG, 0, 0, 0);
	if (pending_vector)
		*pending_vector = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_mc_drain (void)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_DRAIN, 0, 0, 0);
	return iprv.status;
}

static inline s64
ia64_pal_mc_dynamic_state (u64 info_type, u64 dy_buffer, u64 *size)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_DYNAMIC_STATE, info_type, dy_buffer, 0);
	if (size)
		*size = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_mc_error_info (u64 info_index, u64 type_index, u64 *size, u64 *error_info)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_ERROR_INFO, info_index, type_index, 0);
	if (size)
		*size = iprv.v0;
	if (error_info)
		*error_info = iprv.v1;
	return iprv.status;
}

static inline s64
ia64_pal_mc_error_inject_phys (u64 err_type_info, u64 err_struct_info,
			u64 err_data_buffer, u64 *capabilities, u64 *resources)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS_STK(iprv, PAL_MC_ERROR_INJECT, err_type_info,
			  err_struct_info, err_data_buffer);
	if (capabilities)
		*capabilities= iprv.v0;
	if (resources)
		*resources= iprv.v1;
	return iprv.status;
}

static inline s64
ia64_pal_mc_error_inject_virt (u64 err_type_info, u64 err_struct_info,
			u64 err_data_buffer, u64 *capabilities, u64 *resources)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_STK(iprv, PAL_MC_ERROR_INJECT, err_type_info,
			  err_struct_info, err_data_buffer);
	if (capabilities)
		*capabilities= iprv.v0;
	if (resources)
		*resources= iprv.v1;
	return iprv.status;
}

static inline s64
ia64_pal_mc_expected (u64 expected, u64 *previous)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_EXPECTED, expected, 0, 0);
	if (previous)
		*previous = iprv.v0;
	return iprv.status;
}

typedef union pal_hw_tracking_u {
	u64			pht_data;
	struct {
		u64		itc	:4,	
				dct	:4,	
				itt	:4,	
				ddt	:4,	
				reserved:48;
	} pal_hw_tracking_s;
} pal_hw_tracking_u_t;

static inline s64
ia64_pal_mc_hw_tracking (u64 *status)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_HW_TRACKING, 0, 0, 0);
	if (status)
		*status = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_mc_register_mem (u64 physical_addr, u64 size, u64 *req_size)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_REGISTER_MEM, physical_addr, size, 0);
	if (req_size)
		*req_size = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_mc_resume (u64 set_cmci, u64 save_ptr)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MC_RESUME, set_cmci, save_ptr, 0);
	return iprv.status;
}

static inline s64
ia64_pal_mem_attrib (u64 *mem_attrib)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MEM_ATTRIB, 0, 0, 0);
	if (mem_attrib)
		*mem_attrib = iprv.v0 & 0xff;
	return iprv.status;
}

static inline s64
ia64_pal_mem_for_test (u64 *bytes_needed, u64 *alignment)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_MEM_FOR_TEST, 0, 0, 0);
	if (bytes_needed)
		*bytes_needed = iprv.v0;
	if (alignment)
		*alignment = iprv.v1;
	return iprv.status;
}

typedef union pal_perf_mon_info_u {
	u64			  ppmi_data;
	struct {
	       u64		generic		: 8,
				width		: 8,
				cycles		: 8,
				retired		: 8,
				reserved	: 32;
	} pal_perf_mon_info_s;
} pal_perf_mon_info_u_t;

static inline s64
ia64_pal_perf_mon_info (u64 *pm_buffer, pal_perf_mon_info_u_t *pm_info)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_PERF_MON_INFO, (unsigned long) pm_buffer, 0, 0);
	if (pm_info)
		pm_info->ppmi_data = iprv.v0;
	return iprv.status;
}

static inline s64
ia64_pal_platform_addr (u64 type, u64 physical_addr)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_PLATFORM_ADDR, type, physical_addr, 0);
	return iprv.status;
}

static inline s64
ia64_pal_pmi_entrypoint (u64 sal_pmi_entry_addr)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_PMI_ENTRYPOINT, sal_pmi_entry_addr, 0, 0);
	return iprv.status;
}

struct pal_features_s;
static inline s64
ia64_pal_proc_get_features (u64 *features_avail,
			    u64 *features_status,
			    u64 *features_control,
			    u64 features_set)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS(iprv, PAL_PROC_GET_FEATURES, 0, features_set, 0);
	if (iprv.status == 0) {
		*features_avail   = iprv.v0;
		*features_status  = iprv.v1;
		*features_control = iprv.v2;
	}
	return iprv.status;
}

static inline s64
ia64_pal_proc_set_features (u64 feature_select)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS(iprv, PAL_PROC_SET_FEATURES, feature_select, 0, 0);
	return iprv.status;
}

typedef struct ia64_ptce_info_s {
	unsigned long	base;
	u32		count[2];
	u32		stride[2];
} ia64_ptce_info_t;

static inline s64
ia64_get_ptce (ia64_ptce_info_t *ptce)
{
	struct ia64_pal_retval iprv;

	if (!ptce)
		return -1;

	PAL_CALL(iprv, PAL_PTCE_INFO, 0, 0, 0);
	if (iprv.status == 0) {
		ptce->base = iprv.v0;
		ptce->count[0] = iprv.v1 >> 32;
		ptce->count[1] = iprv.v1 & 0xffffffff;
		ptce->stride[0] = iprv.v2 >> 32;
		ptce->stride[1] = iprv.v2 & 0xffffffff;
	}
	return iprv.status;
}

static inline s64
ia64_pal_register_info (u64 info_request, u64 *reg_info_1, u64 *reg_info_2)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_REGISTER_INFO, info_request, 0, 0);
	if (reg_info_1)
		*reg_info_1 = iprv.v0;
	if (reg_info_2)
		*reg_info_2 = iprv.v1;
	return iprv.status;
}

typedef union pal_hints_u {
	unsigned long		ph_data;
	struct {
	       unsigned long	si		: 1,
				li		: 1,
				reserved	: 62;
	} pal_hints_s;
} pal_hints_u_t;

static inline long ia64_pal_rse_info(unsigned long *num_phys_stacked,
							pal_hints_u_t *hints)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_RSE_INFO, 0, 0, 0);
	if (num_phys_stacked)
		*num_phys_stacked = iprv.v0;
	if (hints)
		hints->ph_data = iprv.v1;
	return iprv.status;
}

static inline s64
ia64_pal_set_hw_policy (u64 policy)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_SET_HW_POLICY, policy, 0, 0);
	return iprv.status;
}

static inline s64
ia64_pal_shutdown (void)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_SHUTDOWN, 0, 0, 0);
	return iprv.status;
}

static inline s64
ia64_pal_test_proc (u64 test_addr, u64 test_size, u64 attributes, u64 *self_test_state)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_TEST_PROC, test_addr, test_size, attributes);
	if (self_test_state)
		*self_test_state = iprv.v0;
	return iprv.status;
}

typedef union  pal_version_u {
	u64	pal_version_val;
	struct {
		u64	pv_pal_b_rev		:	8;
		u64	pv_pal_b_model		:	8;
		u64	pv_reserved1		:	8;
		u64	pv_pal_vendor		:	8;
		u64	pv_pal_a_rev		:	8;
		u64	pv_pal_a_model		:	8;
		u64	pv_reserved2		:	16;
	} pal_version_s;
} pal_version_u_t;


static inline s64
ia64_pal_version (pal_version_u_t *pal_min_version, pal_version_u_t *pal_cur_version)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS(iprv, PAL_VERSION, 0, 0, 0);
	if (pal_min_version)
		pal_min_version->pal_version_val = iprv.v0;

	if (pal_cur_version)
		pal_cur_version->pal_version_val = iprv.v1;

	return iprv.status;
}

typedef union pal_tc_info_u {
	u64			pti_val;
	struct {
	       u64		num_sets	:	8,
				associativity	:	8,
				num_entries	:	16,
				pf		:	1,
				unified		:	1,
				reduce_tr	:	1,
				reserved	:	29;
	} pal_tc_info_s;
} pal_tc_info_u_t;

#define tc_reduce_tr		pal_tc_info_s.reduce_tr
#define tc_unified		pal_tc_info_s.unified
#define tc_pf			pal_tc_info_s.pf
#define tc_num_entries		pal_tc_info_s.num_entries
#define tc_associativity	pal_tc_info_s.associativity
#define tc_num_sets		pal_tc_info_s.num_sets


static inline s64
ia64_pal_vm_info (u64 tc_level, u64 tc_type,  pal_tc_info_u_t *tc_info, u64 *tc_pages)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_VM_INFO, tc_level, tc_type, 0);
	if (tc_info)
		tc_info->pti_val = iprv.v0;
	if (tc_pages)
		*tc_pages = iprv.v1;
	return iprv.status;
}

static inline s64 ia64_pal_vm_page_size(u64 *tr_pages, u64 *vw_pages)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_VM_PAGE_SIZE, 0, 0, 0);
	if (tr_pages)
		*tr_pages = iprv.v0;
	if (vw_pages)
		*vw_pages = iprv.v1;
	return iprv.status;
}

typedef union pal_vm_info_1_u {
	u64			pvi1_val;
	struct {
		u64		vw		: 1,
				phys_add_size	: 7,
				key_size	: 8,
				max_pkr		: 8,
				hash_tag_id	: 8,
				max_dtr_entry	: 8,
				max_itr_entry	: 8,
				max_unique_tcs	: 8,
				num_tc_levels	: 8;
	} pal_vm_info_1_s;
} pal_vm_info_1_u_t;

#define PAL_MAX_PURGES		0xFFFF		

typedef union pal_vm_info_2_u {
	u64			pvi2_val;
	struct {
		u64		impl_va_msb	: 8,
				rid_size	: 8,
				max_purges	: 16,
				reserved	: 32;
	} pal_vm_info_2_s;
} pal_vm_info_2_u_t;

static inline s64
ia64_pal_vm_summary (pal_vm_info_1_u_t *vm_info_1, pal_vm_info_2_u_t *vm_info_2)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_VM_SUMMARY, 0, 0, 0);
	if (vm_info_1)
		vm_info_1->pvi1_val = iprv.v0;
	if (vm_info_2)
		vm_info_2->pvi2_val = iprv.v1;
	return iprv.status;
}

typedef union pal_vp_info_u {
	u64			pvi_val;
	struct {
		u64		index:		48,	
				vmm_id:		16;	
	} pal_vp_info_s;
} pal_vp_info_u_t;

static inline s64
ia64_pal_vp_info (u64 feature_set, u64 vp_buffer, u64 *vp_info, u64 *vmm_id)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_VP_INFO, feature_set, vp_buffer, 0);
	if (vp_info)
		*vp_info = iprv.v0;
	if (vmm_id)
		*vmm_id = iprv.v1;
	return iprv.status;
}

typedef union pal_itr_valid_u {
	u64			piv_val;
	struct {
	       u64		access_rights_valid	: 1,
				priv_level_valid	: 1,
				dirty_bit_valid		: 1,
				mem_attr_valid		: 1,
				reserved		: 60;
	} pal_tr_valid_s;
} pal_tr_valid_u_t;

static inline s64
ia64_pal_tr_read (u64 reg_num, u64 tr_type, u64 *tr_buffer, pal_tr_valid_u_t *tr_valid)
{
	struct ia64_pal_retval iprv;
	PAL_CALL_PHYS_STK(iprv, PAL_VM_TR_READ, reg_num, tr_type,(u64)ia64_tpa(tr_buffer));
	if (tr_valid)
		tr_valid->piv_val = iprv.v0;
	return iprv.status;
}

#define PAL_VISIBILITY_VIRTUAL		0
#define PAL_VISIBILITY_PHYSICAL		1

#define PAL_VISIBILITY_OK		1
#define PAL_VISIBILITY_OK_REMOTE_NEEDED	0
#define PAL_VISIBILITY_INVAL_ARG	-2
#define PAL_VISIBILITY_ERROR		-3

static inline s64
ia64_pal_prefetch_visibility (s64 trans_type)
{
	struct ia64_pal_retval iprv;
	PAL_CALL(iprv, PAL_PREFETCH_VISIBILITY, trans_type, 0, 0);
	return iprv.status;
}

typedef union pal_log_overview_u {
	struct {
		u64	num_log		:16,	
			tpc		:8,	
			reserved3	:8,	
			cpp		:8,	
			reserved2	:8,	
			ppid		:8,	
			reserved1	:8;	
	} overview_bits;
	u64 overview_data;
} pal_log_overview_t;

typedef union pal_proc_n_log_info1_u{
	struct {
		u64	tid		:16,	
			reserved2	:16,	
			cid		:16,	
			reserved1	:16;	
	} ppli1_bits;
	u64	ppli1_data;
} pal_proc_n_log_info1_t;

typedef union pal_proc_n_log_info2_u {
	struct {
		u64	la		:16,	
			reserved	:48;	
	} ppli2_bits;
	u64	ppli2_data;
} pal_proc_n_log_info2_t;

typedef struct pal_logical_to_physical_s
{
	pal_log_overview_t overview;
	pal_proc_n_log_info1_t ppli1;
	pal_proc_n_log_info2_t ppli2;
} pal_logical_to_physical_t;

#define overview_num_log	overview.overview_bits.num_log
#define overview_tpc		overview.overview_bits.tpc
#define overview_cpp		overview.overview_bits.cpp
#define overview_ppid		overview.overview_bits.ppid
#define log1_tid		ppli1.ppli1_bits.tid
#define log1_cid		ppli1.ppli1_bits.cid
#define log2_la			ppli2.ppli2_bits.la

static inline s64
ia64_pal_logical_to_phys(u64 proc_number, pal_logical_to_physical_t *mapping)
{
	struct ia64_pal_retval iprv;

	PAL_CALL(iprv, PAL_LOGICAL_TO_PHYSICAL, proc_number, 0, 0);

	if (iprv.status == PAL_STATUS_SUCCESS)
	{
		mapping->overview.overview_data = iprv.v0;
		mapping->ppli1.ppli1_data = iprv.v1;
		mapping->ppli2.ppli2_data = iprv.v2;
	}

	return iprv.status;
}

typedef struct pal_cache_shared_info_s
{
	u64 num_shared;
	pal_proc_n_log_info1_t ppli1;
	pal_proc_n_log_info2_t ppli2;
} pal_cache_shared_info_t;

static inline s64
ia64_pal_cache_shared_info(u64 level,
		u64 type,
		u64 proc_number,
		pal_cache_shared_info_t *info)
{
	struct ia64_pal_retval iprv;

	PAL_CALL(iprv, PAL_CACHE_SHARED_INFO, level, type, proc_number);

	if (iprv.status == PAL_STATUS_SUCCESS) {
		info->num_shared = iprv.v0;
		info->ppli1.ppli1_data = iprv.v1;
		info->ppli2.ppli2_data = iprv.v2;
	}

	return iprv.status;
}
#endif 

#endif 
