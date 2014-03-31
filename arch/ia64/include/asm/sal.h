#ifndef _ASM_IA64_SAL_H
#define _ASM_IA64_SAL_H

/*
 * System Abstraction Layer definitions.
 *
 * This is based on version 2.5 of the manual "IA-64 System
 * Abstraction Layer".
 *
 * Copyright (C) 2001 Intel
 * Copyright (C) 2002 Jenna Hall <jenna.s.hall@intel.com>
 * Copyright (C) 2001 Fred Lewis <frederick.v.lewis@intel.com>
 * Copyright (C) 1998, 1999, 2001, 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 Srinivasa Prasad Thirumalachar <sprasad@sprasad.engr.sgi.com>
 *
 * 02/01/04 J. Hall Updated Error Record Structures to conform to July 2001
 *		    revision of the SAL spec.
 * 01/01/03 fvlewis Updated Error Record Structures to conform with Nov. 2000
 *                  revision of the SAL spec.
 * 99/09/29 davidm	Updated for SAL 2.6.
 * 00/03/29 cfleck      Updated SAL Error Logging info for processor (SAL 2.6)
 *                      (plus examples of platform error info structures from smariset @ Intel)
 */

#define IA64_SAL_PLATFORM_FEATURE_BUS_LOCK_BIT		0
#define IA64_SAL_PLATFORM_FEATURE_IRQ_REDIR_HINT_BIT	1
#define IA64_SAL_PLATFORM_FEATURE_IPI_REDIR_HINT_BIT	2
#define IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT_BIT	 	3

#define IA64_SAL_PLATFORM_FEATURE_BUS_LOCK	  (1<<IA64_SAL_PLATFORM_FEATURE_BUS_LOCK_BIT)
#define IA64_SAL_PLATFORM_FEATURE_IRQ_REDIR_HINT (1<<IA64_SAL_PLATFORM_FEATURE_IRQ_REDIR_HINT_BIT)
#define IA64_SAL_PLATFORM_FEATURE_IPI_REDIR_HINT (1<<IA64_SAL_PLATFORM_FEATURE_IPI_REDIR_HINT_BIT)
#define IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT	  (1<<IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT_BIT)

#ifndef __ASSEMBLY__

#include <linux/bcd.h>
#include <linux/spinlock.h>
#include <linux/efi.h>

#include <asm/pal.h>
#include <asm/fpu.h>

extern spinlock_t sal_lock;

#define __IA64_FW_CALL(entry,result,a0,a1,a2,a3,a4,a5,a6,a7)	\
	result = (*entry)(a0,a1,a2,a3,a4,a5,a6,a7)

# define IA64_FW_CALL(entry,result,args...) do {		\
	unsigned long __ia64_sc_flags;				\
	struct ia64_fpreg __ia64_sc_fr[6];			\
	ia64_save_scratch_fpregs(__ia64_sc_fr);			\
	spin_lock_irqsave(&sal_lock, __ia64_sc_flags);		\
	__IA64_FW_CALL(entry, result, args);			\
	spin_unlock_irqrestore(&sal_lock, __ia64_sc_flags);	\
	ia64_load_scratch_fpregs(__ia64_sc_fr);			\
} while (0)

# define SAL_CALL(result,args...)			\
	IA64_FW_CALL(ia64_sal, result, args);

# define SAL_CALL_NOLOCK(result,args...) do {		\
	unsigned long __ia64_scn_flags;			\
	struct ia64_fpreg __ia64_scn_fr[6];		\
	ia64_save_scratch_fpregs(__ia64_scn_fr);	\
	local_irq_save(__ia64_scn_flags);		\
	__IA64_FW_CALL(ia64_sal, result, args);		\
	local_irq_restore(__ia64_scn_flags);		\
	ia64_load_scratch_fpregs(__ia64_scn_fr);	\
} while (0)

# define SAL_CALL_REENTRANT(result,args...) do {	\
	struct ia64_fpreg __ia64_scs_fr[6];		\
	ia64_save_scratch_fpregs(__ia64_scs_fr);	\
	preempt_disable();				\
	__IA64_FW_CALL(ia64_sal, result, args);		\
	preempt_enable();				\
	ia64_load_scratch_fpregs(__ia64_scs_fr);	\
} while (0)

#define SAL_SET_VECTORS			0x01000000
#define SAL_GET_STATE_INFO		0x01000001
#define SAL_GET_STATE_INFO_SIZE		0x01000002
#define SAL_CLEAR_STATE_INFO		0x01000003
#define SAL_MC_RENDEZ			0x01000004
#define SAL_MC_SET_PARAMS		0x01000005
#define SAL_REGISTER_PHYSICAL_ADDR	0x01000006

#define SAL_CACHE_FLUSH			0x01000008
#define SAL_CACHE_INIT			0x01000009
#define SAL_PCI_CONFIG_READ		0x01000010
#define SAL_PCI_CONFIG_WRITE		0x01000011
#define SAL_FREQ_BASE			0x01000012
#define SAL_PHYSICAL_ID_INFO		0x01000013

#define SAL_UPDATE_PAL			0x01000020

struct ia64_sal_retval {
	long status;
	unsigned long v0;
	unsigned long v1;
	unsigned long v2;
};

typedef struct ia64_sal_retval (*ia64_sal_handler) (u64, ...);

enum {
	SAL_FREQ_BASE_PLATFORM = 0,
	SAL_FREQ_BASE_INTERVAL_TIMER = 1,
	SAL_FREQ_BASE_REALTIME_CLOCK = 2
};

struct ia64_sal_systab {
	u8 signature[4];	
	u32 size;		
	u8 sal_rev_minor;
	u8 sal_rev_major;
	u16 entry_count;	
	u8 checksum;
	u8 reserved1[7];
	u8 sal_a_rev_minor;
	u8 sal_a_rev_major;
	u8 sal_b_rev_minor;
	u8 sal_b_rev_major;
	
	u8 oem_id[32];
	u8 product_id[32];	
	u8 reserved2[8];
};

enum sal_systab_entry_type {
	SAL_DESC_ENTRY_POINT = 0,
	SAL_DESC_MEMORY = 1,
	SAL_DESC_PLATFORM_FEATURE = 2,
	SAL_DESC_TR = 3,
	SAL_DESC_PTC = 4,
	SAL_DESC_AP_WAKEUP = 5
};

#define SAL_DESC_SIZE(type)	"\060\040\020\040\020\020"[(unsigned) type]

typedef struct ia64_sal_desc_entry_point {
	u8 type;
	u8 reserved1[7];
	u64 pal_proc;
	u64 sal_proc;
	u64 gp;
	u8 reserved2[16];
}ia64_sal_desc_entry_point_t;

typedef struct ia64_sal_desc_memory {
	u8 type;
	u8 used_by_sal;	
	u8 mem_attr;		
	u8 access_rights;	
	u8 mem_attr_mask;	
	u8 reserved1;
	u8 mem_type;		
	u8 mem_usage;		
	u64 addr;		
	u32 length;	
	u32 reserved2;
	u8 oem_reserved[8];
} ia64_sal_desc_memory_t;

typedef struct ia64_sal_desc_platform_feature {
	u8 type;
	u8 feature_mask;
	u8 reserved1[14];
} ia64_sal_desc_platform_feature_t;

typedef struct ia64_sal_desc_tr {
	u8 type;
	u8 tr_type;		
	u8 regnum;		
	u8 reserved1[5];
	u64 addr;		
	u64 page_size;		
	u8 reserved2[8];
} ia64_sal_desc_tr_t;

typedef struct ia64_sal_desc_ptc {
	u8 type;
	u8 reserved1[3];
	u32 num_domains;	
	u64 domain_info;	
} ia64_sal_desc_ptc_t;

typedef struct ia64_sal_ptc_domain_info {
	u64 proc_count;		
	u64 proc_list;		
} ia64_sal_ptc_domain_info_t;

typedef struct ia64_sal_ptc_domain_proc_entry {
	u64 id  : 8;		
	u64 eid : 8;		
} ia64_sal_ptc_domain_proc_entry_t;


#define IA64_SAL_AP_EXTERNAL_INT 0

typedef struct ia64_sal_desc_ap_wakeup {
	u8 type;
	u8 mechanism;		
	u8 reserved1[6];
	u64 vector;		
} ia64_sal_desc_ap_wakeup_t ;

extern ia64_sal_handler ia64_sal;
extern struct ia64_sal_desc_ptc *ia64_ptc_domain_info;

extern unsigned short sal_revision;	
extern unsigned short sal_version;	
#define SAL_VERSION_CODE(major, minor) ((bin2bcd(major) << 8) | bin2bcd(minor))

extern const char *ia64_sal_strerror (long status);
extern void ia64_sal_init (struct ia64_sal_systab *sal_systab);

enum {
	SAL_INFO_TYPE_MCA  = 0,		
        SAL_INFO_TYPE_INIT = 1,		
        SAL_INFO_TYPE_CMC  = 2,		
        SAL_INFO_TYPE_CPE  = 3		
};

enum {
	SAL_MC_PARAM_RENDEZ_INT    = 1,	
	SAL_MC_PARAM_RENDEZ_WAKEUP = 2,	
	SAL_MC_PARAM_CPE_INT	   = 3	
};

enum {
	SAL_MC_PARAM_MECHANISM_INT = 1,	
	SAL_MC_PARAM_MECHANISM_MEM = 2	
};

enum {
	SAL_VECTOR_OS_MCA	  = 0,
	SAL_VECTOR_OS_INIT	  = 1,
	SAL_VECTOR_OS_BOOT_RENDEZ = 2
};

#define	SAL_MC_PARAM_RZ_ALWAYS		0x1
#define	SAL_MC_PARAM_BINIT_ESCALATE	0x10


#define SAL_PROC_DEV_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf1, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_MEM_DEV_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf2, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_SEL_DEV_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf3, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_PCI_BUS_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf4, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_SMBIOS_DEV_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf5, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_PCI_COMP_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf6, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_SPECIFIC_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf7, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_HOST_CTLR_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf8, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define SAL_PLAT_BUS_ERR_SECT_GUID  \
    EFI_GUID(0xe429faf9, 0x3cb7, 0x11d4, 0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81)
#define PROCESSOR_ABSTRACTION_LAYER_OVERWRITE_GUID \
    EFI_GUID(0x6cb0a200, 0x893a, 0x11da, 0x96, 0xd2, 0x0, 0x10, 0x83, 0xff, \
		0xca, 0x4d)

#define MAX_CACHE_ERRORS	6
#define MAX_TLB_ERRORS		6
#define MAX_BUS_ERRORS		1

typedef struct sal_log_revision {
	u8 minor;		
	u8 major;		
} sal_log_revision_t;

typedef struct sal_log_timestamp {
	u8 slh_second;		
	u8 slh_minute;		
	u8 slh_hour;		
	u8 slh_reserved;
	u8 slh_day;		
	u8 slh_month;		
	u8 slh_year;		
	u8 slh_century;		
} sal_log_timestamp_t;

typedef struct sal_log_record_header {
	u64 id;				
	sal_log_revision_t revision;	
	u8 severity;			
	u8 validation_bits;		
	u32 len;			
	sal_log_timestamp_t timestamp;	
	efi_guid_t platform_guid;	
} sal_log_record_header_t;

#define sal_log_severity_recoverable	0
#define sal_log_severity_fatal		1
#define sal_log_severity_corrected	2

#define ERI_NOT_VALID		0x0	
#define ERI_NOT_ACCESSIBLE	0x30	
#define ERI_CONTAINMENT_WARN	0x22	
#define ERI_UNCORRECTED_ERROR	0x20	
#define ERI_COMPONENT_RESET	0x24	
#define ERI_CORR_ERROR_LOG	0x21	
#define ERI_CORR_ERROR_THRESH	0x29	

typedef struct sal_log_sec_header {
    efi_guid_t guid;			
    sal_log_revision_t revision;	
    u8 error_recovery_info;		
    u8 reserved;
    u32 len;				
} sal_log_section_hdr_t;

typedef struct sal_log_mod_error_info {
	struct {
		u64 check_info              : 1,
		    requestor_identifier    : 1,
		    responder_identifier    : 1,
		    target_identifier       : 1,
		    precise_ip              : 1,
		    reserved                : 59;
	} valid;
	u64 check_info;
	u64 requestor_identifier;
	u64 responder_identifier;
	u64 target_identifier;
	u64 precise_ip;
} sal_log_mod_error_info_t;

typedef struct sal_processor_static_info {
	struct {
		u64 minstate        : 1,
		    br              : 1,
		    cr              : 1,
		    ar              : 1,
		    rr              : 1,
		    fr              : 1,
		    reserved        : 58;
	} valid;
	pal_min_state_area_t min_state_area;
	u64 br[8];
	u64 cr[128];
	u64 ar[128];
	u64 rr[8];
	struct ia64_fpreg __attribute__ ((packed)) fr[128];
} sal_processor_static_info_t;

struct sal_cpuid_info {
	u64 regs[5];
	u64 reserved;
};

typedef struct sal_log_processor_info {
	sal_log_section_hdr_t header;
	struct {
		u64 proc_error_map      : 1,
		    proc_state_param    : 1,
		    proc_cr_lid         : 1,
		    psi_static_struct   : 1,
		    num_cache_check     : 4,
		    num_tlb_check       : 4,
		    num_bus_check       : 4,
		    num_reg_file_check  : 4,
		    num_ms_check        : 4,
		    cpuid_info          : 1,
		    reserved1           : 39;
	} valid;
	u64 proc_error_map;
	u64 proc_state_parameter;
	u64 proc_cr_lid;
	sal_log_mod_error_info_t info[0];
} sal_log_processor_info_t;

#define SAL_LPI_PSI_INFO(l)									\
({	sal_log_processor_info_t *_l = (l);							\
	((sal_processor_static_info_t *)							\
	 ((char *) _l->info + ((_l->valid.num_cache_check + _l->valid.num_tlb_check		\
				+ _l->valid.num_bus_check + _l->valid.num_reg_file_check	\
				+ _l->valid.num_ms_check) * sizeof(sal_log_mod_error_info_t)	\
			       + sizeof(struct sal_cpuid_info))));				\
})


typedef struct sal_log_mem_dev_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 error_status    : 1,
		    physical_addr   : 1,
		    addr_mask       : 1,
		    node            : 1,
		    card            : 1,
		    module          : 1,
		    bank            : 1,
		    device          : 1,
		    row             : 1,
		    column          : 1,
		    bit_position    : 1,
		    requestor_id    : 1,
		    responder_id    : 1,
		    target_id       : 1,
		    bus_spec_data   : 1,
		    oem_id          : 1,
		    oem_data        : 1,
		    reserved        : 47;
	} valid;
	u64 error_status;
	u64 physical_addr;
	u64 addr_mask;
	u16 node;
	u16 card;
	u16 module;
	u16 bank;
	u16 device;
	u16 row;
	u16 column;
	u16 bit_position;
	u64 requestor_id;
	u64 responder_id;
	u64 target_id;
	u64 bus_spec_data;
	u8 oem_id[16];
	u8 oem_data[1];			
} sal_log_mem_dev_err_info_t;

typedef struct sal_log_sel_dev_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 record_id       : 1,
		    record_type     : 1,
		    generator_id    : 1,
		    evm_rev         : 1,
		    sensor_type     : 1,
		    sensor_num      : 1,
		    event_dir       : 1,
		    event_data1     : 1,
		    event_data2     : 1,
		    event_data3     : 1,
		    reserved        : 54;
	} valid;
	u16 record_id;
	u8 record_type;
	u8 timestamp[4];
	u16 generator_id;
	u8 evm_rev;
	u8 sensor_type;
	u8 sensor_num;
	u8 event_dir;
	u8 event_data1;
	u8 event_data2;
	u8 event_data3;
} sal_log_sel_dev_err_info_t;

typedef struct sal_log_pci_bus_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 err_status      : 1,
		    err_type        : 1,
		    bus_id          : 1,
		    bus_address     : 1,
		    bus_data        : 1,
		    bus_cmd         : 1,
		    requestor_id    : 1,
		    responder_id    : 1,
		    target_id       : 1,
		    oem_data        : 1,
		    reserved        : 54;
	} valid;
	u64 err_status;
	u16 err_type;
	u16 bus_id;
	u32 reserved;
	u64 bus_address;
	u64 bus_data;
	u64 bus_cmd;
	u64 requestor_id;
	u64 responder_id;
	u64 target_id;
	u8 oem_data[1];			
} sal_log_pci_bus_err_info_t;

typedef struct sal_log_smbios_dev_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 event_type      : 1,
		    length          : 1,
		    time_stamp      : 1,
		    data            : 1,
		    reserved1       : 60;
	} valid;
	u8 event_type;
	u8 length;
	u8 time_stamp[6];
	u8 data[1];			
} sal_log_smbios_dev_err_info_t;

typedef struct sal_log_pci_comp_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 err_status      : 1,
		    comp_info       : 1,
		    num_mem_regs    : 1,
		    num_io_regs     : 1,
		    reg_data_pairs  : 1,
		    oem_data        : 1,
		    reserved        : 58;
	} valid;
	u64 err_status;
	struct {
		u16 vendor_id;
		u16 device_id;
		u8 class_code[3];
		u8 func_num;
		u8 dev_num;
		u8 bus_num;
		u8 seg_num;
		u8 reserved[5];
	} comp_info;
	u32 num_mem_regs;
	u32 num_io_regs;
	u64 reg_data_pairs[1];
	u8 oem_data[1];			
} sal_log_pci_comp_err_info_t;

typedef struct sal_log_plat_specific_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 err_status      : 1,
		    guid            : 1,
		    oem_data        : 1,
		    reserved        : 61;
	} valid;
	u64 err_status;
	efi_guid_t guid;
	u8 oem_data[1];			
} sal_log_plat_specific_err_info_t;

typedef struct sal_log_host_ctlr_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 err_status      : 1,
		    requestor_id    : 1,
		    responder_id    : 1,
		    target_id       : 1,
		    bus_spec_data   : 1,
		    oem_data        : 1,
		    reserved        : 58;
	} valid;
	u64 err_status;
	u64 requestor_id;
	u64 responder_id;
	u64 target_id;
	u64 bus_spec_data;
	u8 oem_data[1];			
} sal_log_host_ctlr_err_info_t;

typedef struct sal_log_plat_bus_err_info {
	sal_log_section_hdr_t header;
	struct {
		u64 err_status      : 1,
		    requestor_id    : 1,
		    responder_id    : 1,
		    target_id       : 1,
		    bus_spec_data   : 1,
		    oem_data        : 1,
		    reserved        : 58;
	} valid;
	u64 err_status;
	u64 requestor_id;
	u64 responder_id;
	u64 target_id;
	u64 bus_spec_data;
	u8 oem_data[1];			
} sal_log_plat_bus_err_info_t;

typedef union sal_log_platform_err_info {
	sal_log_mem_dev_err_info_t mem_dev_err;
	sal_log_sel_dev_err_info_t sel_dev_err;
	sal_log_pci_bus_err_info_t pci_bus_err;
	sal_log_smbios_dev_err_info_t smbios_dev_err;
	sal_log_pci_comp_err_info_t pci_comp_err;
	sal_log_plat_specific_err_info_t plat_specific_err;
	sal_log_host_ctlr_err_info_t host_ctlr_err;
	sal_log_plat_bus_err_info_t plat_bus_err;
} sal_log_platform_err_info_t;

typedef struct err_rec {
	sal_log_record_header_t sal_elog_header;
	sal_log_processor_info_t proc_err;
	sal_log_platform_err_info_t plat_err;
	u8 oem_data_pad[1024];
} ia64_err_rec_t;


extern s64 ia64_sal_cache_flush (u64 cache_type);
extern void __init check_sal_cache_flush (void);

static inline s64
ia64_sal_cache_init (void)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_CACHE_INIT, 0, 0, 0, 0, 0, 0, 0);
	return isrv.status;
}

static inline s64
ia64_sal_clear_state_info (u64 sal_info_type)
{
	struct ia64_sal_retval isrv;
	SAL_CALL_REENTRANT(isrv, SAL_CLEAR_STATE_INFO, sal_info_type, 0,
	              0, 0, 0, 0, 0);
	return isrv.status;
}


static inline u64
ia64_sal_get_state_info (u64 sal_info_type, u64 *sal_info)
{
	struct ia64_sal_retval isrv;
	SAL_CALL_REENTRANT(isrv, SAL_GET_STATE_INFO, sal_info_type, 0,
	              sal_info, 0, 0, 0, 0);
	if (isrv.status)
		return 0;

	return isrv.v0;
}

static inline u64
ia64_sal_get_state_info_size (u64 sal_info_type)
{
	struct ia64_sal_retval isrv;
	SAL_CALL_REENTRANT(isrv, SAL_GET_STATE_INFO_SIZE, sal_info_type, 0,
	              0, 0, 0, 0, 0);
	if (isrv.status)
		return 0;
	return isrv.v0;
}

static inline s64
ia64_sal_mc_rendez (void)
{
	struct ia64_sal_retval isrv;
	SAL_CALL_NOLOCK(isrv, SAL_MC_RENDEZ, 0, 0, 0, 0, 0, 0, 0);
	return isrv.status;
}

static inline struct ia64_sal_retval
ia64_sal_mc_set_params (u64 param_type, u64 i_or_m, u64 i_or_m_val, u64 timeout, u64 rz_always)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_MC_SET_PARAMS, param_type, i_or_m, i_or_m_val,
		 timeout, rz_always, 0, 0);
	return isrv;
}

static inline s64
ia64_sal_pci_config_read (u64 pci_config_addr, int type, u64 size, u64 *value)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_PCI_CONFIG_READ, pci_config_addr, size, type, 0, 0, 0, 0);
	if (value)
		*value = isrv.v0;
	return isrv.status;
}

static inline s64
ia64_sal_pci_config_write (u64 pci_config_addr, int type, u64 size, u64 value)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_PCI_CONFIG_WRITE, pci_config_addr, size, value,
	         type, 0, 0, 0);
	return isrv.status;
}

static inline s64
ia64_sal_register_physical_addr (u64 phys_entry, u64 phys_addr)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_REGISTER_PHYSICAL_ADDR, phys_entry, phys_addr,
	         0, 0, 0, 0, 0);
	return isrv.status;
}

static inline s64
ia64_sal_set_vectors (u64 vector_type,
		      u64 handler_addr1, u64 gp1, u64 handler_len1,
		      u64 handler_addr2, u64 gp2, u64 handler_len2)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_SET_VECTORS, vector_type,
			handler_addr1, gp1, handler_len1,
			handler_addr2, gp2, handler_len2);

	return isrv.status;
}

static inline s64
ia64_sal_update_pal (u64 param_buf, u64 scratch_buf, u64 scratch_buf_size,
		     u64 *error_code, u64 *scratch_buf_size_needed)
{
	struct ia64_sal_retval isrv;
	SAL_CALL(isrv, SAL_UPDATE_PAL, param_buf, scratch_buf, scratch_buf_size,
	         0, 0, 0, 0);
	if (error_code)
		*error_code = isrv.v0;
	if (scratch_buf_size_needed)
		*scratch_buf_size_needed = isrv.v1;
	return isrv.status;
}

static inline s64
ia64_sal_physical_id_info(u16 *splid)
{
	struct ia64_sal_retval isrv;

	if (sal_revision < SAL_VERSION_CODE(3,2))
		return -1;

	SAL_CALL(isrv, SAL_PHYSICAL_ID_INFO, 0, 0, 0, 0, 0, 0, 0);
	if (splid)
		*splid = isrv.v0;
	return isrv.status;
}

extern unsigned long sal_platform_features;

extern int (*salinfo_platform_oemdata)(const u8 *, u8 **, u64 *);

struct sal_ret_values {
	long r8; long r9; long r10; long r11;
};

#define IA64_SAL_OEMFUNC_MIN		0x02000000
#define IA64_SAL_OEMFUNC_MAX		0x03ffffff

extern int ia64_sal_oemcall(struct ia64_sal_retval *, u64, u64, u64, u64, u64,
			    u64, u64, u64);
extern int ia64_sal_oemcall_nolock(struct ia64_sal_retval *, u64, u64, u64,
				   u64, u64, u64, u64, u64);
extern int ia64_sal_oemcall_reentrant(struct ia64_sal_retval *, u64, u64, u64,
				      u64, u64, u64, u64, u64);
extern long
ia64_sal_freq_base (unsigned long which, unsigned long *ticks_per_second,
		    unsigned long *drift_info);
#ifdef CONFIG_HOTPLUG_CPU
struct sal_to_os_boot {
	u64 rr[8];		
	u64 br[6];		
	u64 gr1;		
	u64 gr12;		
	u64 gr13;		
	u64 fpsr;
	u64 pfs;
	u64 rnat;
	u64 unat;
	u64 bspstore;
	u64 dcr;		
	u64 iva;
	u64 pta;
	u64 itv;
	u64 pmv;
	u64 cmcv;
	u64 lrr[2];
	u64 gr[4];
	u64 pr;			
	u64 lc;			
	struct ia64_fpreg fp[20];
};

extern struct sal_to_os_boot sal_boot_rendez_state[NR_CPUS];

extern void ia64_jump_to_sal(struct sal_to_os_boot *);
#endif

extern void ia64_sal_handler_init(void *entry_point, void *gpval);

#define PALO_MAX_TLB_PURGES	0xFFFF
#define PALO_SIG	"PALO"

struct palo_table {
	u8  signature[4];	
	u32 length;
	u8  minor_revision;
	u8  major_revision;
	u8  checksum;
	u8  reserved1[5];
	u16 max_tlb_purges;
	u8  reserved2[6];
};

#define NPTCG_FROM_PAL			0
#define NPTCG_FROM_PALO			1
#define NPTCG_FROM_KERNEL_PARAMETER	2

#endif 

#endif 
