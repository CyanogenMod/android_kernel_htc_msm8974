
/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __ACTBL1_H__
#define __ACTBL1_H__


#define ACPI_SIG_BERT           "BERT"	
#define ACPI_SIG_CPEP           "CPEP"	
#define ACPI_SIG_ECDT           "ECDT"	
#define ACPI_SIG_EINJ           "EINJ"	
#define ACPI_SIG_ERST           "ERST"	
#define ACPI_SIG_HEST           "HEST"	
#define ACPI_SIG_MADT           "APIC"	
#define ACPI_SIG_MSCT           "MSCT"	
#define ACPI_SIG_SBST           "SBST"	
#define ACPI_SIG_SLIT           "SLIT"	
#define ACPI_SIG_SRAT           "SRAT"	

#pragma pack(1)




struct acpi_subtable_header {
	u8 type;
	u8 length;
};


struct acpi_whea_header {
	u8 action;
	u8 instruction;
	u8 flags;
	u8 reserved;
	struct acpi_generic_address register_region;
	u64 value;		
	u64 mask;		
};


struct acpi_table_bert {
	struct acpi_table_header header;	
	u32 region_length;	
	u64 address;		
};


struct acpi_bert_region {
	u32 block_status;	
	u32 raw_data_offset;	
	u32 raw_data_length;	
	u32 data_length;	
	u32 error_severity;	
};


#define ACPI_BERT_UNCORRECTABLE             (1)
#define ACPI_BERT_CORRECTABLE               (1<<1)
#define ACPI_BERT_MULTIPLE_UNCORRECTABLE    (1<<2)
#define ACPI_BERT_MULTIPLE_CORRECTABLE      (1<<3)
#define ACPI_BERT_ERROR_ENTRY_COUNT         (0xFF<<4)	


enum acpi_bert_error_severity {
	ACPI_BERT_ERROR_CORRECTABLE = 0,
	ACPI_BERT_ERROR_FATAL = 1,
	ACPI_BERT_ERROR_CORRECTED = 2,
	ACPI_BERT_ERROR_NONE = 3,
	ACPI_BERT_ERROR_RESERVED = 4	
};



struct acpi_table_cpep {
	struct acpi_table_header header;	
	u64 reserved;
};


struct acpi_cpep_polling {
	struct acpi_subtable_header header;
	u8 id;			
	u8 eid;			
	u32 interval;		
};


struct acpi_table_ecdt {
	struct acpi_table_header header;	
	struct acpi_generic_address control;	
	struct acpi_generic_address data;	
	u32 uid;		
	u8 gpe;			
	u8 id[1];		
};


struct acpi_table_einj {
	struct acpi_table_header header;	
	u32 header_length;
	u8 flags;
	u8 reserved[3];
	u32 entries;
};


struct acpi_einj_entry {
	struct acpi_whea_header whea_header;	
};


#define ACPI_EINJ_PRESERVE          (1)


enum acpi_einj_actions {
	ACPI_EINJ_BEGIN_OPERATION = 0,
	ACPI_EINJ_GET_TRIGGER_TABLE = 1,
	ACPI_EINJ_SET_ERROR_TYPE = 2,
	ACPI_EINJ_GET_ERROR_TYPE = 3,
	ACPI_EINJ_END_OPERATION = 4,
	ACPI_EINJ_EXECUTE_OPERATION = 5,
	ACPI_EINJ_CHECK_BUSY_STATUS = 6,
	ACPI_EINJ_GET_COMMAND_STATUS = 7,
	ACPI_EINJ_SET_ERROR_TYPE_WITH_ADDRESS = 8,
	ACPI_EINJ_ACTION_RESERVED = 9,	
	ACPI_EINJ_TRIGGER_ERROR = 0xFF	
};


enum acpi_einj_instructions {
	ACPI_EINJ_READ_REGISTER = 0,
	ACPI_EINJ_READ_REGISTER_VALUE = 1,
	ACPI_EINJ_WRITE_REGISTER = 2,
	ACPI_EINJ_WRITE_REGISTER_VALUE = 3,
	ACPI_EINJ_NOOP = 4,
	ACPI_EINJ_FLUSH_CACHELINE = 5,
	ACPI_EINJ_INSTRUCTION_RESERVED = 6	
};

struct acpi_einj_error_type_with_addr {
	u32 error_type;
	u32 vendor_struct_offset;
	u32 flags;
	u32 apic_id;
	u64 address;
	u64 range;
	u32 pcie_id;
};

struct acpi_einj_vendor {
	u32 length;
	u32 pcie_id;
	u16 vendor_id;
	u16 device_id;
	u8 revision_id;
	u8 reserved[3];
};


struct acpi_einj_trigger {
	u32 header_size;
	u32 revision;
	u32 table_size;
	u32 entry_count;
};


enum acpi_einj_command_status {
	ACPI_EINJ_SUCCESS = 0,
	ACPI_EINJ_FAILURE = 1,
	ACPI_EINJ_INVALID_ACCESS = 2,
	ACPI_EINJ_STATUS_RESERVED = 3	
};


#define ACPI_EINJ_PROCESSOR_CORRECTABLE     (1)
#define ACPI_EINJ_PROCESSOR_UNCORRECTABLE   (1<<1)
#define ACPI_EINJ_PROCESSOR_FATAL           (1<<2)
#define ACPI_EINJ_MEMORY_CORRECTABLE        (1<<3)
#define ACPI_EINJ_MEMORY_UNCORRECTABLE      (1<<4)
#define ACPI_EINJ_MEMORY_FATAL              (1<<5)
#define ACPI_EINJ_PCIX_CORRECTABLE          (1<<6)
#define ACPI_EINJ_PCIX_UNCORRECTABLE        (1<<7)
#define ACPI_EINJ_PCIX_FATAL                (1<<8)
#define ACPI_EINJ_PLATFORM_CORRECTABLE      (1<<9)
#define ACPI_EINJ_PLATFORM_UNCORRECTABLE    (1<<10)
#define ACPI_EINJ_PLATFORM_FATAL            (1<<11)
#define ACPI_EINJ_VENDOR_DEFINED            (1<<31)


struct acpi_table_erst {
	struct acpi_table_header header;	
	u32 header_length;
	u32 reserved;
	u32 entries;
};


struct acpi_erst_entry {
	struct acpi_whea_header whea_header;	
};


#define ACPI_ERST_PRESERVE          (1)


enum acpi_erst_actions {
	ACPI_ERST_BEGIN_WRITE = 0,
	ACPI_ERST_BEGIN_READ = 1,
	ACPI_ERST_BEGIN_CLEAR = 2,
	ACPI_ERST_END = 3,
	ACPI_ERST_SET_RECORD_OFFSET = 4,
	ACPI_ERST_EXECUTE_OPERATION = 5,
	ACPI_ERST_CHECK_BUSY_STATUS = 6,
	ACPI_ERST_GET_COMMAND_STATUS = 7,
	ACPI_ERST_GET_RECORD_ID = 8,
	ACPI_ERST_SET_RECORD_ID = 9,
	ACPI_ERST_GET_RECORD_COUNT = 10,
	ACPI_ERST_BEGIN_DUMMY_WRIITE = 11,
	ACPI_ERST_NOT_USED = 12,
	ACPI_ERST_GET_ERROR_RANGE = 13,
	ACPI_ERST_GET_ERROR_LENGTH = 14,
	ACPI_ERST_GET_ERROR_ATTRIBUTES = 15,
	ACPI_ERST_ACTION_RESERVED = 16	
};


enum acpi_erst_instructions {
	ACPI_ERST_READ_REGISTER = 0,
	ACPI_ERST_READ_REGISTER_VALUE = 1,
	ACPI_ERST_WRITE_REGISTER = 2,
	ACPI_ERST_WRITE_REGISTER_VALUE = 3,
	ACPI_ERST_NOOP = 4,
	ACPI_ERST_LOAD_VAR1 = 5,
	ACPI_ERST_LOAD_VAR2 = 6,
	ACPI_ERST_STORE_VAR1 = 7,
	ACPI_ERST_ADD = 8,
	ACPI_ERST_SUBTRACT = 9,
	ACPI_ERST_ADD_VALUE = 10,
	ACPI_ERST_SUBTRACT_VALUE = 11,
	ACPI_ERST_STALL = 12,
	ACPI_ERST_STALL_WHILE_TRUE = 13,
	ACPI_ERST_SKIP_NEXT_IF_TRUE = 14,
	ACPI_ERST_GOTO = 15,
	ACPI_ERST_SET_SRC_ADDRESS_BASE = 16,
	ACPI_ERST_SET_DST_ADDRESS_BASE = 17,
	ACPI_ERST_MOVE_DATA = 18,
	ACPI_ERST_INSTRUCTION_RESERVED = 19	
};


enum acpi_erst_command_status {
	ACPI_ERST_SUCESS = 0,
	ACPI_ERST_NO_SPACE = 1,
	ACPI_ERST_NOT_AVAILABLE = 2,
	ACPI_ERST_FAILURE = 3,
	ACPI_ERST_RECORD_EMPTY = 4,
	ACPI_ERST_NOT_FOUND = 5,
	ACPI_ERST_STATUS_RESERVED = 6	
};


struct acpi_erst_info {
	u16 signature;		
	u8 data[48];
};


struct acpi_table_hest {
	struct acpi_table_header header;	
	u32 error_source_count;
};


struct acpi_hest_header {
	u16 type;
	u16 source_id;
};


enum acpi_hest_types {
	ACPI_HEST_TYPE_IA32_CHECK = 0,
	ACPI_HEST_TYPE_IA32_CORRECTED_CHECK = 1,
	ACPI_HEST_TYPE_IA32_NMI = 2,
	ACPI_HEST_TYPE_NOT_USED3 = 3,
	ACPI_HEST_TYPE_NOT_USED4 = 4,
	ACPI_HEST_TYPE_NOT_USED5 = 5,
	ACPI_HEST_TYPE_AER_ROOT_PORT = 6,
	ACPI_HEST_TYPE_AER_ENDPOINT = 7,
	ACPI_HEST_TYPE_AER_BRIDGE = 8,
	ACPI_HEST_TYPE_GENERIC_ERROR = 9,
	ACPI_HEST_TYPE_RESERVED = 10	
};


struct acpi_hest_ia_error_bank {
	u8 bank_number;
	u8 clear_status_on_init;
	u8 status_format;
	u8 reserved;
	u32 control_register;
	u64 control_data;
	u32 status_register;
	u32 address_register;
	u32 misc_register;
};


struct acpi_hest_aer_common {
	u16 reserved1;
	u8 flags;
	u8 enabled;
	u32 records_to_preallocate;
	u32 max_sections_per_record;
	u32 bus;
	u16 device;
	u16 function;
	u16 device_control;
	u16 reserved2;
	u32 uncorrectable_mask;
	u32 uncorrectable_severity;
	u32 correctable_mask;
	u32 advanced_capabilities;
};


#define ACPI_HEST_FIRMWARE_FIRST        (1)
#define ACPI_HEST_GLOBAL                (1<<1)


struct acpi_hest_notify {
	u8 type;
	u8 length;
	u16 config_write_enable;
	u32 poll_interval;
	u32 vector;
	u32 polling_threshold_value;
	u32 polling_threshold_window;
	u32 error_threshold_value;
	u32 error_threshold_window;
};


enum acpi_hest_notify_types {
	ACPI_HEST_NOTIFY_POLLED = 0,
	ACPI_HEST_NOTIFY_EXTERNAL = 1,
	ACPI_HEST_NOTIFY_LOCAL = 2,
	ACPI_HEST_NOTIFY_SCI = 3,
	ACPI_HEST_NOTIFY_NMI = 4,
	ACPI_HEST_NOTIFY_RESERVED = 5	
};


#define ACPI_HEST_TYPE                  (1)
#define ACPI_HEST_POLL_INTERVAL         (1<<1)
#define ACPI_HEST_POLL_THRESHOLD_VALUE  (1<<2)
#define ACPI_HEST_POLL_THRESHOLD_WINDOW (1<<3)
#define ACPI_HEST_ERR_THRESHOLD_VALUE   (1<<4)
#define ACPI_HEST_ERR_THRESHOLD_WINDOW  (1<<5)



struct acpi_hest_ia_machine_check {
	struct acpi_hest_header header;
	u16 reserved1;
	u8 flags;
	u8 enabled;
	u32 records_to_preallocate;
	u32 max_sections_per_record;
	u64 global_capability_data;
	u64 global_control_data;
	u8 num_hardware_banks;
	u8 reserved3[7];
};


struct acpi_hest_ia_corrected {
	struct acpi_hest_header header;
	u16 reserved1;
	u8 flags;
	u8 enabled;
	u32 records_to_preallocate;
	u32 max_sections_per_record;
	struct acpi_hest_notify notify;
	u8 num_hardware_banks;
	u8 reserved2[3];
};


struct acpi_hest_ia_nmi {
	struct acpi_hest_header header;
	u32 reserved;
	u32 records_to_preallocate;
	u32 max_sections_per_record;
	u32 max_raw_data_length;
};



struct acpi_hest_aer_root {
	struct acpi_hest_header header;
	struct acpi_hest_aer_common aer;
	u32 root_error_command;
};


struct acpi_hest_aer {
	struct acpi_hest_header header;
	struct acpi_hest_aer_common aer;
};


struct acpi_hest_aer_bridge {
	struct acpi_hest_header header;
	struct acpi_hest_aer_common aer;
	u32 uncorrectable_mask2;
	u32 uncorrectable_severity2;
	u32 advanced_capabilities2;
};


struct acpi_hest_generic {
	struct acpi_hest_header header;
	u16 related_source_id;
	u8 reserved;
	u8 enabled;
	u32 records_to_preallocate;
	u32 max_sections_per_record;
	u32 max_raw_data_length;
	struct acpi_generic_address error_status_address;
	struct acpi_hest_notify notify;
	u32 error_block_length;
};


struct acpi_hest_generic_status {
	u32 block_status;
	u32 raw_data_offset;
	u32 raw_data_length;
	u32 data_length;
	u32 error_severity;
};


#define ACPI_HEST_UNCORRECTABLE             (1)
#define ACPI_HEST_CORRECTABLE               (1<<1)
#define ACPI_HEST_MULTIPLE_UNCORRECTABLE    (1<<2)
#define ACPI_HEST_MULTIPLE_CORRECTABLE      (1<<3)
#define ACPI_HEST_ERROR_ENTRY_COUNT         (0xFF<<4)	


struct acpi_hest_generic_data {
	u8 section_type[16];
	u32 error_severity;
	u16 revision;
	u8 validation_bits;
	u8 flags;
	u32 error_data_length;
	u8 fru_id[16];
	u8 fru_text[20];
};


struct acpi_table_madt {
	struct acpi_table_header header;	
	u32 address;		
	u32 flags;
};


#define ACPI_MADT_PCAT_COMPAT       (1)	


#define ACPI_MADT_DUAL_PIC          0
#define ACPI_MADT_MULTIPLE_APIC     1


enum acpi_madt_type {
	ACPI_MADT_TYPE_LOCAL_APIC = 0,
	ACPI_MADT_TYPE_IO_APIC = 1,
	ACPI_MADT_TYPE_INTERRUPT_OVERRIDE = 2,
	ACPI_MADT_TYPE_NMI_SOURCE = 3,
	ACPI_MADT_TYPE_LOCAL_APIC_NMI = 4,
	ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE = 5,
	ACPI_MADT_TYPE_IO_SAPIC = 6,
	ACPI_MADT_TYPE_LOCAL_SAPIC = 7,
	ACPI_MADT_TYPE_INTERRUPT_SOURCE = 8,
	ACPI_MADT_TYPE_LOCAL_X2APIC = 9,
	ACPI_MADT_TYPE_LOCAL_X2APIC_NMI = 10,
	ACPI_MADT_TYPE_GENERIC_INTERRUPT = 11,
	ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR = 12,
	ACPI_MADT_TYPE_RESERVED = 13	
};



struct acpi_madt_local_apic {
	struct acpi_subtable_header header;
	u8 processor_id;	
	u8 id;			
	u32 lapic_flags;
};


struct acpi_madt_io_apic {
	struct acpi_subtable_header header;
	u8 id;			
	u8 reserved;		
	u32 address;		
	u32 global_irq_base;	
};


struct acpi_madt_interrupt_override {
	struct acpi_subtable_header header;
	u8 bus;			
	u8 source_irq;		
	u32 global_irq;		
	u16 inti_flags;
};


struct acpi_madt_nmi_source {
	struct acpi_subtable_header header;
	u16 inti_flags;
	u32 global_irq;		
};


struct acpi_madt_local_apic_nmi {
	struct acpi_subtable_header header;
	u8 processor_id;	
	u16 inti_flags;
	u8 lint;		
};


struct acpi_madt_local_apic_override {
	struct acpi_subtable_header header;
	u16 reserved;		
	u64 address;		
};


struct acpi_madt_io_sapic {
	struct acpi_subtable_header header;
	u8 id;			
	u8 reserved;		
	u32 global_irq_base;	
	u64 address;		
};


struct acpi_madt_local_sapic {
	struct acpi_subtable_header header;
	u8 processor_id;	
	u8 id;			
	u8 eid;			
	u8 reserved[3];		
	u32 lapic_flags;
	u32 uid;		
	char uid_string[1];	
};


struct acpi_madt_interrupt_source {
	struct acpi_subtable_header header;
	u16 inti_flags;
	u8 type;		
	u8 id;			
	u8 eid;			
	u8 io_sapic_vector;	
	u32 global_irq;		
	u32 flags;		
};


#define ACPI_MADT_CPEI_OVERRIDE     (1)


struct acpi_madt_local_x2apic {
	struct acpi_subtable_header header;
	u16 reserved;		
	u32 local_apic_id;	
	u32 lapic_flags;
	u32 uid;		
};


struct acpi_madt_local_x2apic_nmi {
	struct acpi_subtable_header header;
	u16 inti_flags;
	u32 uid;		
	u8 lint;		
	u8 reserved[3];
};


struct acpi_madt_generic_interrupt {
	struct acpi_subtable_header header;
	u16 reserved;		
	u32 gic_id;
	u32 uid;
	u32 flags;
	u32 parking_version;
	u32 performance_interrupt;
	u64 parked_address;
	u64 base_address;
};


struct acpi_madt_generic_distributor {
	struct acpi_subtable_header header;
	u16 reserved;		
	u32 gic_id;
	u64 base_address;
	u32 global_irq_base;
	u32 reserved2;		
};



#define ACPI_MADT_ENABLED           (1)	


#define ACPI_MADT_POLARITY_MASK     (3)	
#define ACPI_MADT_TRIGGER_MASK      (3<<2)	


#define ACPI_MADT_POLARITY_CONFORMS       0
#define ACPI_MADT_POLARITY_ACTIVE_HIGH    1
#define ACPI_MADT_POLARITY_RESERVED       2
#define ACPI_MADT_POLARITY_ACTIVE_LOW     3

#define ACPI_MADT_TRIGGER_CONFORMS        (0)
#define ACPI_MADT_TRIGGER_EDGE            (1<<2)
#define ACPI_MADT_TRIGGER_RESERVED        (2<<2)
#define ACPI_MADT_TRIGGER_LEVEL           (3<<2)


struct acpi_table_msct {
	struct acpi_table_header header;	
	u32 proximity_offset;	
	u32 max_proximity_domains;	
	u32 max_clock_domains;	
	u64 max_address;	
};


struct acpi_msct_proximity {
	u8 revision;
	u8 length;
	u32 range_start;	
	u32 range_end;		
	u32 processor_capacity;
	u64 memory_capacity;	
};


struct acpi_table_sbst {
	struct acpi_table_header header;	
	u32 warning_level;
	u32 low_level;
	u32 critical_level;
};


struct acpi_table_slit {
	struct acpi_table_header header;	
	u64 locality_count;
	u8 entry[1];		
};


struct acpi_table_srat {
	struct acpi_table_header header;	
	u32 table_revision;	
	u64 reserved;		
};


enum acpi_srat_type {
	ACPI_SRAT_TYPE_CPU_AFFINITY = 0,
	ACPI_SRAT_TYPE_MEMORY_AFFINITY = 1,
	ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY = 2,
	ACPI_SRAT_TYPE_RESERVED = 3	
};



struct acpi_srat_cpu_affinity {
	struct acpi_subtable_header header;
	u8 proximity_domain_lo;
	u8 apic_id;
	u32 flags;
	u8 local_sapic_eid;
	u8 proximity_domain_hi[3];
	u32 reserved;		
};


#define ACPI_SRAT_CPU_USE_AFFINITY  (1)	


struct acpi_srat_mem_affinity {
	struct acpi_subtable_header header;
	u32 proximity_domain;
	u16 reserved;		
	u64 base_address;
	u64 length;
       u32 reserved1;
	u32 flags;
       u64 reserved2;	       
};


#define ACPI_SRAT_MEM_ENABLED       (1)	
#define ACPI_SRAT_MEM_HOT_PLUGGABLE (1<<1)	
#define ACPI_SRAT_MEM_NON_VOLATILE  (1<<2)	


struct acpi_srat_x2apic_cpu_affinity {
	struct acpi_subtable_header header;
	u16 reserved;		
	u32 proximity_domain;
	u32 apic_id;
	u32 flags;
	u32 clock_domain;
	u32 reserved2;
};


#define ACPI_SRAT_CPU_ENABLED       (1)	


#pragma pack()

#endif				
