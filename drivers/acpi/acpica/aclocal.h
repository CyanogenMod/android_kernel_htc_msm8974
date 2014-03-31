
/*
 * Copyright (C) 2000 - 2012, Intel Corp.
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

#ifndef __ACLOCAL_H__
#define __ACLOCAL_H__


#define ACPI_SERIALIZED                 0xFF

typedef u32 acpi_mutex_handle;
#define ACPI_GLOBAL_LOCK                (acpi_semaphore) (-1)


#define AML_NUM_OPCODES                 0x81


struct acpi_walk_state;
struct acpi_obj_mutex;
union acpi_parse_object;


#define ACPI_MTX_INTERPRETER            0	
#define ACPI_MTX_NAMESPACE              1	
#define ACPI_MTX_TABLES                 2	
#define ACPI_MTX_EVENTS                 3	
#define ACPI_MTX_CACHES                 4	
#define ACPI_MTX_MEMORY                 5	
#define ACPI_MTX_DEBUG_CMD_COMPLETE     6	
#define ACPI_MTX_DEBUG_CMD_READY        7	

#define ACPI_MAX_MUTEX                  7
#define ACPI_NUM_MUTEX                  ACPI_MAX_MUTEX+1


struct acpi_rw_lock {
	acpi_mutex writer_mutex;
	acpi_mutex reader_mutex;
	u32 num_readers;
};

#define ACPI_LOCK_GPES                  0
#define ACPI_LOCK_HARDWARE              1

#define ACPI_MAX_LOCK                   1
#define ACPI_NUM_LOCK                   ACPI_MAX_LOCK+1


#define ACPI_MUTEX_NOT_ACQUIRED         (acpi_thread_id) 0


struct acpi_mutex_info {
	acpi_mutex mutex;
	u32 use_count;
	acpi_thread_id thread_id;
};


#define ACPI_MTX_DO_NOT_LOCK            0
#define ACPI_MTX_LOCK                   1


#define ACPI_FIELD_BYTE_GRANULARITY     1
#define ACPI_FIELD_WORD_GRANULARITY     2
#define ACPI_FIELD_DWORD_GRANULARITY    4
#define ACPI_FIELD_QWORD_GRANULARITY    8

#define ACPI_ENTRY_NOT_FOUND            NULL



typedef enum {
	ACPI_IMODE_LOAD_PASS1 = 0x01,
	ACPI_IMODE_LOAD_PASS2 = 0x02,
	ACPI_IMODE_EXECUTE = 0x03
} acpi_interpreter_mode;

struct acpi_namespace_node {
	union acpi_operand_object *object;	
	u8 descriptor_type;	
	u8 type;		
	u8 flags;		
	acpi_owner_id owner_id;	
	union acpi_name_union name;	
	struct acpi_namespace_node *parent;	
	struct acpi_namespace_node *child;	
	struct acpi_namespace_node *peer;	

#ifdef ACPI_LARGE_NAMESPACE_NODE
	union acpi_parse_object *op;
	u32 value;
	u32 length;
#endif
};


#define ANOBJ_RESERVED                  0x01	
#define ANOBJ_TEMPORARY                 0x02	
#define ANOBJ_METHOD_ARG                0x04	
#define ANOBJ_METHOD_LOCAL              0x08	
#define ANOBJ_SUBTREE_HAS_INI           0x10	
#define ANOBJ_EVALUATED                 0x20	
#define ANOBJ_ALLOCATED_BUFFER          0x40	

#define ANOBJ_IS_EXTERNAL               0x08	
#define ANOBJ_METHOD_NO_RETVAL          0x10	
#define ANOBJ_METHOD_SOME_NO_RETVAL     0x20	
#define ANOBJ_IS_BIT_OFFSET             0x40	
#define ANOBJ_IS_REFERENCED             0x80	


struct acpi_table_list {
	struct acpi_table_desc *tables;	
	u32 current_table_count;	
	u32 max_table_count;	
	u8 flags;
};


#define ACPI_ROOT_ORIGIN_UNKNOWN        (0)	
#define ACPI_ROOT_ORIGIN_ALLOCATED      (1)
#define ACPI_ROOT_ALLOW_RESIZE          (2)


#define ACPI_TABLE_INDEX_DSDT           (0)
#define ACPI_TABLE_INDEX_FACS           (1)

struct acpi_find_context {
	char *search_for;
	acpi_handle *list;
	u32 *count;
};

struct acpi_ns_search_data {
	struct acpi_namespace_node *node;
};


#define ACPI_COPY_TYPE_SIMPLE           0
#define ACPI_COPY_TYPE_PACKAGE          1


struct acpi_namestring_info {
	const char *external_name;
	const char *next_external_char;
	char *internal_name;
	u32 length;
	u32 num_segments;
	u32 num_carats;
	u8 fully_qualified;
};


struct acpi_create_field_info {
	struct acpi_namespace_node *region_node;
	struct acpi_namespace_node *field_node;
	struct acpi_namespace_node *register_node;
	struct acpi_namespace_node *data_register_node;
	struct acpi_namespace_node *connection_node;
	u8 *resource_buffer;
	u32 bank_value;
	u32 field_bit_position;
	u32 field_bit_length;
	u16 resource_length;
	u8 field_flags;
	u8 attribute;
	u8 field_type;
	u8 access_length;
};

typedef
acpi_status(*ACPI_INTERNAL_METHOD) (struct acpi_walk_state * walk_state);

#define ACPI_BTYPE_ANY                  0x00000000
#define ACPI_BTYPE_INTEGER              0x00000001
#define ACPI_BTYPE_STRING               0x00000002
#define ACPI_BTYPE_BUFFER               0x00000004
#define ACPI_BTYPE_PACKAGE              0x00000008
#define ACPI_BTYPE_FIELD_UNIT           0x00000010
#define ACPI_BTYPE_DEVICE               0x00000020
#define ACPI_BTYPE_EVENT                0x00000040
#define ACPI_BTYPE_METHOD               0x00000080
#define ACPI_BTYPE_MUTEX                0x00000100
#define ACPI_BTYPE_REGION               0x00000200
#define ACPI_BTYPE_POWER                0x00000400
#define ACPI_BTYPE_PROCESSOR            0x00000800
#define ACPI_BTYPE_THERMAL              0x00001000
#define ACPI_BTYPE_BUFFER_FIELD         0x00002000
#define ACPI_BTYPE_DDB_HANDLE           0x00004000
#define ACPI_BTYPE_DEBUG_OBJECT         0x00008000
#define ACPI_BTYPE_REFERENCE            0x00010000
#define ACPI_BTYPE_RESOURCE             0x00020000

#define ACPI_BTYPE_COMPUTE_DATA         (ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER)

#define ACPI_BTYPE_DATA                 (ACPI_BTYPE_COMPUTE_DATA  | ACPI_BTYPE_PACKAGE)
#define ACPI_BTYPE_DATA_REFERENCE       (ACPI_BTYPE_DATA | ACPI_BTYPE_REFERENCE | ACPI_BTYPE_DDB_HANDLE)
#define ACPI_BTYPE_DEVICE_OBJECTS       (ACPI_BTYPE_DEVICE | ACPI_BTYPE_THERMAL | ACPI_BTYPE_PROCESSOR)
#define ACPI_BTYPE_OBJECTS_AND_REFS     0x0001FFFF	
#define ACPI_BTYPE_ALL_OBJECTS          0x0000FFFF

struct acpi_name_info {
	char name[ACPI_NAME_SIZE];
	u8 param_count;
	u8 expected_btypes;
};


struct acpi_package_info {
	u8 type;
	u8 object_type1;
	u8 count1;
	u8 object_type2;
	u8 count2;
	u8 reserved;
};


struct acpi_package_info2 {
	u8 type;
	u8 count;
	u8 object_type[4];
};


struct acpi_package_info3 {
	u8 type;
	u8 count;
	u8 object_type[2];
	u8 tail_object_type;
	u8 reserved;
};

union acpi_predefined_info {
	struct acpi_name_info info;
	struct acpi_package_info ret_info;
	struct acpi_package_info2 ret_info2;
	struct acpi_package_info3 ret_info3;
};


struct acpi_predefined_data {
	char *pathname;
	const union acpi_predefined_info *predefined;
	union acpi_operand_object *parent_package;
	struct acpi_namespace_node *node;
	u32 flags;
	u8 node_flags;
};


#define ACPI_OBJECT_REPAIRED    1
#define ACPI_OBJECT_WRAPPED     2

#define ACPI_RTYPE_ANY                  0x00
#define ACPI_RTYPE_NONE                 0x01
#define ACPI_RTYPE_INTEGER              0x02
#define ACPI_RTYPE_STRING               0x04
#define ACPI_RTYPE_BUFFER               0x08
#define ACPI_RTYPE_PACKAGE              0x10
#define ACPI_RTYPE_REFERENCE            0x20
#define ACPI_RTYPE_ALL                  0x3F

#define ACPI_NUM_RTYPES                 5	



struct acpi_gpe_handler_info {
	acpi_gpe_handler address;	
	void *context;		
	struct acpi_namespace_node *method_node;	
	u8 original_flags;      
	u8 originally_enabled;  
};

struct acpi_gpe_notify_object {
	struct acpi_namespace_node *node;
	struct acpi_gpe_notify_object *next;
};

union acpi_gpe_dispatch_info {
	struct acpi_namespace_node *method_node;	
	struct acpi_gpe_handler_info *handler;  
	struct acpi_gpe_notify_object device;   
};

struct acpi_gpe_event_info {
	union acpi_gpe_dispatch_info dispatch;	
	struct acpi_gpe_register_info *register_info;	
	u8 flags;		
	u8 gpe_number;		
	u8 runtime_count;	
};


struct acpi_gpe_register_info {
	struct acpi_generic_address status_address;	
	struct acpi_generic_address enable_address;	
	u8 enable_for_wake;	
	u8 enable_for_run;	
	u8 base_gpe_number;	
};

struct acpi_gpe_block_info {
	struct acpi_namespace_node *node;
	struct acpi_gpe_block_info *previous;
	struct acpi_gpe_block_info *next;
	struct acpi_gpe_xrupt_info *xrupt_block;	
	struct acpi_gpe_register_info *register_info;	
	struct acpi_gpe_event_info *event_info;	
	struct acpi_generic_address block_address;	
	u32 register_count;	
	u16 gpe_count;		
	u8 block_base_number;	
	u8 initialized;         
};


struct acpi_gpe_xrupt_info {
	struct acpi_gpe_xrupt_info *previous;
	struct acpi_gpe_xrupt_info *next;
	struct acpi_gpe_block_info *gpe_block_list_head;	
	u32 interrupt_number;	
};

struct acpi_gpe_walk_info {
	struct acpi_namespace_node *gpe_device;
	struct acpi_gpe_block_info *gpe_block;
	u16 count;
	acpi_owner_id owner_id;
	u8 execute_by_owner_id;
};

struct acpi_gpe_device_info {
	u32 index;
	u32 next_block_base_index;
	acpi_status status;
	struct acpi_namespace_node *gpe_device;
};

typedef acpi_status(*acpi_gpe_callback) (struct acpi_gpe_xrupt_info *gpe_xrupt_info,
		struct acpi_gpe_block_info *gpe_block, void *context);


struct acpi_fixed_event_handler {
	acpi_event_handler handler;	
	void *context;		
};

struct acpi_fixed_event_info {
	u8 status_register_id;
	u8 enable_register_id;
	u16 status_bit_mask;
	u16 enable_bit_mask;
};


struct acpi_field_info {
	u8 skip_field;
	u8 field_flag;
	u32 pkg_length;
};


#define ACPI_CONTROL_NORMAL                  0xC0
#define ACPI_CONTROL_CONDITIONAL_EXECUTING   0xC1
#define ACPI_CONTROL_PREDICATE_EXECUTING     0xC2
#define ACPI_CONTROL_PREDICATE_FALSE         0xC3
#define ACPI_CONTROL_PREDICATE_TRUE          0xC4

#define ACPI_STATE_COMMON \
	void                            *next; \
	u8                              descriptor_type; \
	u8                              flags; \
	u16                             value; \
	u16                             state;

	

struct acpi_common_state {
ACPI_STATE_COMMON};

struct acpi_update_state {
	ACPI_STATE_COMMON union acpi_operand_object *object;
};

struct acpi_pkg_state {
	ACPI_STATE_COMMON u16 index;
	union acpi_operand_object *source_object;
	union acpi_operand_object *dest_object;
	struct acpi_walk_state *walk_state;
	void *this_target_obj;
	u32 num_packages;
};

struct acpi_control_state {
	ACPI_STATE_COMMON u16 opcode;
	union acpi_parse_object *predicate_op;
	u8 *aml_predicate_start;	
	u8 *package_end;	
	u32 loop_count;		
};

struct acpi_scope_state {
	ACPI_STATE_COMMON struct acpi_namespace_node *node;
};

struct acpi_pscope_state {
	ACPI_STATE_COMMON u32 arg_count;	
	union acpi_parse_object *op;	
	u8 *arg_end;		
	u8 *pkg_end;		
	u32 arg_list;		
};

struct acpi_thread_state {
	ACPI_STATE_COMMON u8 current_sync_level;	
	struct acpi_walk_state *walk_state_list;	
	union acpi_operand_object *acquired_mutex_list;	
	acpi_thread_id thread_id;	
};

struct acpi_result_values {
	ACPI_STATE_COMMON
	    union acpi_operand_object *obj_desc[ACPI_RESULTS_FRAME_OBJ_NUM];
};

typedef
acpi_status(*acpi_parse_downwards) (struct acpi_walk_state * walk_state,
				    union acpi_parse_object ** out_op);

typedef acpi_status(*acpi_parse_upwards) (struct acpi_walk_state * walk_state);

struct acpi_notify_info {
	ACPI_STATE_COMMON struct acpi_namespace_node *node;
	union acpi_operand_object *handler_obj;
};


union acpi_generic_state {
	struct acpi_common_state common;
	struct acpi_control_state control;
	struct acpi_update_state update;
	struct acpi_scope_state scope;
	struct acpi_pscope_state parse_scope;
	struct acpi_pkg_state pkg;
	struct acpi_thread_state thread;
	struct acpi_result_values results;
	struct acpi_notify_info notify;
};


typedef acpi_status(*ACPI_EXECUTE_OP) (struct acpi_walk_state * walk_state);


struct acpi_address_range {
	struct acpi_address_range *next;
	struct acpi_namespace_node *region_node;
	acpi_physical_address start_address;
	acpi_physical_address end_address;
};


struct acpi_opcode_info {
#if defined(ACPI_DISASSEMBLER) || defined(ACPI_DEBUG_OUTPUT)
	char *name;		
#endif
	u32 parse_args;		
	u32 runtime_args;	
	u16 flags;		
	u8 object_type;		
	u8 class;		
	u8 type;		
};

union acpi_parse_value {
	u64 integer;		
	u32 size;		
	char *string;		
	u8 *buffer;		
	char *name;		
	union acpi_parse_object *arg;	
};

#ifdef ACPI_DISASSEMBLER
#define ACPI_DISASM_ONLY_MEMBERS(a)     a;
#else
#define ACPI_DISASM_ONLY_MEMBERS(a)
#endif

#define ACPI_PARSE_COMMON \
	union acpi_parse_object         *parent;        \
	u8                              descriptor_type; \
	u8                              flags;          \
	u16                             aml_opcode;     \
	u32                             aml_offset;     \
	union acpi_parse_object         *next;          \
	struct acpi_namespace_node      *node;          \
	union acpi_parse_value          value;          \
	u8                              arg_list_length; \
	ACPI_DISASM_ONLY_MEMBERS (\
	u8                              disasm_flags;   \
	u8                              disasm_opcode;  \
	char                            aml_op_name[16])	

#define ACPI_DASM_BUFFER                0x00
#define ACPI_DASM_RESOURCE              0x01
#define ACPI_DASM_STRING                0x02
#define ACPI_DASM_UNICODE               0x03
#define ACPI_DASM_EISAID                0x04
#define ACPI_DASM_MATCHOP               0x05
#define ACPI_DASM_LNOT_PREFIX           0x06
#define ACPI_DASM_LNOT_SUFFIX           0x07
#define ACPI_DASM_IGNORE                0x08

struct acpi_parse_obj_common {
ACPI_PARSE_COMMON};

struct acpi_parse_obj_named {
	ACPI_PARSE_COMMON u8 *path;
	u8 *data;		
	u32 length;		
	u32 name;		
};


#define ACPI_MAX_PARSEOP_NAME   20

struct acpi_parse_obj_asl {
	ACPI_PARSE_COMMON union acpi_parse_object *child;
	union acpi_parse_object *parent_method;
	char *filename;
	char *external_name;
	char *namepath;
	char name_seg[4];
	u32 extra_value;
	u32 column;
	u32 line_number;
	u32 logical_line_number;
	u32 logical_byte_offset;
	u32 end_line;
	u32 end_logical_line;
	u32 acpi_btype;
	u32 aml_length;
	u32 aml_subtree_length;
	u32 final_aml_length;
	u32 final_aml_offset;
	u32 compile_flags;
	u16 parse_opcode;
	u8 aml_opcode_length;
	u8 aml_pkg_len_bytes;
	u8 extra;
	char parse_op_name[ACPI_MAX_PARSEOP_NAME];
};

union acpi_parse_object {
	struct acpi_parse_obj_common common;
	struct acpi_parse_obj_named named;
	struct acpi_parse_obj_asl asl;
};

struct acpi_parse_state {
	u8 *aml_start;		
	u8 *aml;		
	u8 *aml_end;		
	u8 *pkg_start;		
	u8 *pkg_end;		
	union acpi_parse_object *start_op;	
	struct acpi_namespace_node *start_node;
	union acpi_generic_state *scope;	
	union acpi_parse_object *start_scope;
	u32 aml_size;
};


#define ACPI_PARSEOP_GENERIC            0x01
#define ACPI_PARSEOP_NAMED              0x02
#define ACPI_PARSEOP_DEFERRED           0x04
#define ACPI_PARSEOP_BYTELIST           0x08
#define ACPI_PARSEOP_IN_STACK           0x10
#define ACPI_PARSEOP_TARGET             0x20
#define ACPI_PARSEOP_IN_CACHE           0x80


#define ACPI_PARSEOP_IGNORE             0x01
#define ACPI_PARSEOP_PARAMLIST          0x02
#define ACPI_PARSEOP_EMPTY_TERMLIST     0x04
#define ACPI_PARSEOP_SPECIAL            0x10


struct acpi_bit_register_info {
	u8 parent_register;
	u8 bit_position;
	u16 access_bit_mask;
};

#define ACPI_PM1_STATUS_PRESERVED_BITS          0x0800	


#define ACPI_PM1_CONTROL_WRITEONLY_BITS         0x2004	


#define ACPI_PM1_CONTROL_IGNORED_BITS           0x0200	
#define ACPI_PM1_CONTROL_RESERVED_BITS          0xC1F8	
#define ACPI_PM1_CONTROL_PRESERVED_BITS \
	       (ACPI_PM1_CONTROL_IGNORED_BITS | ACPI_PM1_CONTROL_RESERVED_BITS)

#define ACPI_PM2_CONTROL_PRESERVED_BITS         0xFFFFFFFE	

#define ACPI_REGISTER_PM1_STATUS                0x01
#define ACPI_REGISTER_PM1_ENABLE                0x02
#define ACPI_REGISTER_PM1_CONTROL               0x03
#define ACPI_REGISTER_PM2_CONTROL               0x04
#define ACPI_REGISTER_PM_TIMER                  0x05
#define ACPI_REGISTER_PROCESSOR_BLOCK           0x06
#define ACPI_REGISTER_SMI_COMMAND_BLOCK         0x07


#define ACPI_BITMASK_TIMER_STATUS               0x0001
#define ACPI_BITMASK_BUS_MASTER_STATUS          0x0010
#define ACPI_BITMASK_GLOBAL_LOCK_STATUS         0x0020
#define ACPI_BITMASK_POWER_BUTTON_STATUS        0x0100
#define ACPI_BITMASK_SLEEP_BUTTON_STATUS        0x0200
#define ACPI_BITMASK_RT_CLOCK_STATUS            0x0400
#define ACPI_BITMASK_PCIEXP_WAKE_STATUS         0x4000	
#define ACPI_BITMASK_WAKE_STATUS                0x8000

#define ACPI_BITMASK_ALL_FIXED_STATUS           (\
	ACPI_BITMASK_TIMER_STATUS          | \
	ACPI_BITMASK_BUS_MASTER_STATUS     | \
	ACPI_BITMASK_GLOBAL_LOCK_STATUS    | \
	ACPI_BITMASK_POWER_BUTTON_STATUS   | \
	ACPI_BITMASK_SLEEP_BUTTON_STATUS   | \
	ACPI_BITMASK_RT_CLOCK_STATUS       | \
	ACPI_BITMASK_PCIEXP_WAKE_STATUS    | \
	ACPI_BITMASK_WAKE_STATUS)

#define ACPI_BITMASK_TIMER_ENABLE               0x0001
#define ACPI_BITMASK_GLOBAL_LOCK_ENABLE         0x0020
#define ACPI_BITMASK_POWER_BUTTON_ENABLE        0x0100
#define ACPI_BITMASK_SLEEP_BUTTON_ENABLE        0x0200
#define ACPI_BITMASK_RT_CLOCK_ENABLE            0x0400
#define ACPI_BITMASK_PCIEXP_WAKE_DISABLE        0x4000	

#define ACPI_BITMASK_SCI_ENABLE                 0x0001
#define ACPI_BITMASK_BUS_MASTER_RLD             0x0002
#define ACPI_BITMASK_GLOBAL_LOCK_RELEASE        0x0004
#define ACPI_BITMASK_SLEEP_TYPE                 0x1C00
#define ACPI_BITMASK_SLEEP_ENABLE               0x2000

#define ACPI_BITMASK_ARB_DISABLE                0x0001


#define ACPI_BITPOSITION_TIMER_STATUS           0x00
#define ACPI_BITPOSITION_BUS_MASTER_STATUS      0x04
#define ACPI_BITPOSITION_GLOBAL_LOCK_STATUS     0x05
#define ACPI_BITPOSITION_POWER_BUTTON_STATUS    0x08
#define ACPI_BITPOSITION_SLEEP_BUTTON_STATUS    0x09
#define ACPI_BITPOSITION_RT_CLOCK_STATUS        0x0A
#define ACPI_BITPOSITION_PCIEXP_WAKE_STATUS     0x0E	
#define ACPI_BITPOSITION_WAKE_STATUS            0x0F

#define ACPI_BITPOSITION_TIMER_ENABLE           0x00
#define ACPI_BITPOSITION_GLOBAL_LOCK_ENABLE     0x05
#define ACPI_BITPOSITION_POWER_BUTTON_ENABLE    0x08
#define ACPI_BITPOSITION_SLEEP_BUTTON_ENABLE    0x09
#define ACPI_BITPOSITION_RT_CLOCK_ENABLE        0x0A
#define ACPI_BITPOSITION_PCIEXP_WAKE_DISABLE    0x0E	

#define ACPI_BITPOSITION_SCI_ENABLE             0x00
#define ACPI_BITPOSITION_BUS_MASTER_RLD         0x01
#define ACPI_BITPOSITION_GLOBAL_LOCK_RELEASE    0x02
#define ACPI_BITPOSITION_SLEEP_TYPE             0x0A
#define ACPI_BITPOSITION_SLEEP_ENABLE           0x0D

#define ACPI_BITPOSITION_ARB_DISABLE            0x00


#define ACPI_OSI_WIN_2000               0x01
#define ACPI_OSI_WIN_XP                 0x02
#define ACPI_OSI_WIN_XP_SP1             0x03
#define ACPI_OSI_WINSRV_2003            0x04
#define ACPI_OSI_WIN_XP_SP2             0x05
#define ACPI_OSI_WINSRV_2003_SP1        0x06
#define ACPI_OSI_WIN_VISTA              0x07
#define ACPI_OSI_WINSRV_2008            0x08
#define ACPI_OSI_WIN_VISTA_SP1          0x09
#define ACPI_OSI_WIN_VISTA_SP2          0x0A
#define ACPI_OSI_WIN_7                  0x0B

#define ACPI_ALWAYS_ILLEGAL             0x00

struct acpi_interface_info {
	char *name;
	struct acpi_interface_info *next;
	u8 flags;
	u8 value;
};

#define ACPI_OSI_INVALID                0x01
#define ACPI_OSI_DYNAMIC                0x02

struct acpi_port_info {
	char *name;
	u16 start;
	u16 end;
	u8 osi_dependency;
};



#define ACPI_ADDRESS_TYPE_MEMORY_RANGE          0
#define ACPI_ADDRESS_TYPE_IO_RANGE              1
#define ACPI_ADDRESS_TYPE_BUS_NUMBER_RANGE      2


#define ACPI_RESOURCE_NAME_LARGE                0x80
#define ACPI_RESOURCE_NAME_SMALL                0x00

#define ACPI_RESOURCE_NAME_SMALL_MASK           0x78	
#define ACPI_RESOURCE_NAME_SMALL_LENGTH_MASK    0x07	
#define ACPI_RESOURCE_NAME_LARGE_MASK           0x7F	

#define ACPI_RESOURCE_NAME_IRQ                  0x20
#define ACPI_RESOURCE_NAME_DMA                  0x28
#define ACPI_RESOURCE_NAME_START_DEPENDENT      0x30
#define ACPI_RESOURCE_NAME_END_DEPENDENT        0x38
#define ACPI_RESOURCE_NAME_IO                   0x40
#define ACPI_RESOURCE_NAME_FIXED_IO             0x48
#define ACPI_RESOURCE_NAME_FIXED_DMA            0x50
#define ACPI_RESOURCE_NAME_RESERVED_S2          0x58
#define ACPI_RESOURCE_NAME_RESERVED_S3          0x60
#define ACPI_RESOURCE_NAME_RESERVED_S4          0x68
#define ACPI_RESOURCE_NAME_VENDOR_SMALL         0x70
#define ACPI_RESOURCE_NAME_END_TAG              0x78

#define ACPI_RESOURCE_NAME_MEMORY24             0x81
#define ACPI_RESOURCE_NAME_GENERIC_REGISTER     0x82
#define ACPI_RESOURCE_NAME_RESERVED_L1          0x83
#define ACPI_RESOURCE_NAME_VENDOR_LARGE         0x84
#define ACPI_RESOURCE_NAME_MEMORY32             0x85
#define ACPI_RESOURCE_NAME_FIXED_MEMORY32       0x86
#define ACPI_RESOURCE_NAME_ADDRESS32            0x87
#define ACPI_RESOURCE_NAME_ADDRESS16            0x88
#define ACPI_RESOURCE_NAME_EXTENDED_IRQ         0x89
#define ACPI_RESOURCE_NAME_ADDRESS64            0x8A
#define ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64   0x8B
#define ACPI_RESOURCE_NAME_GPIO                 0x8C
#define ACPI_RESOURCE_NAME_SERIAL_BUS           0x8E
#define ACPI_RESOURCE_NAME_LARGE_MAX            0x8E


#define ACPI_ASCII_ZERO                 0x30


struct acpi_db_method_info {
	acpi_handle main_thread_gate;
	acpi_handle thread_complete_gate;
	acpi_thread_id *threads;
	u32 num_threads;
	u32 num_created;
	u32 num_completed;

	char *name;
	u32 flags;
	u32 num_loops;
	char pathname[128];
	char **args;

	char init_args;
	char *arguments[4];
	char num_threads_str[11];
	char id_of_thread_str[11];
	char index_of_thread_str[11];
};

struct acpi_integrity_info {
	u32 nodes;
	u32 objects;
};

#define ACPI_DB_REDIRECTABLE_OUTPUT     0x01
#define ACPI_DB_CONSOLE_OUTPUT          0x02
#define ACPI_DB_DUPLICATE_OUTPUT        0x03



#define ACPI_MEM_MALLOC                 0
#define ACPI_MEM_CALLOC                 1
#define ACPI_MAX_MODULE_NAME            16

#define ACPI_COMMON_DEBUG_MEM_HEADER \
	struct acpi_debug_mem_block     *previous; \
	struct acpi_debug_mem_block     *next; \
	u32                             size; \
	u32                             component; \
	u32                             line; \
	char                            module[ACPI_MAX_MODULE_NAME]; \
	u8                              alloc_type;

struct acpi_debug_mem_header {
ACPI_COMMON_DEBUG_MEM_HEADER};

struct acpi_debug_mem_block {
	ACPI_COMMON_DEBUG_MEM_HEADER u64 user_space;
};

#define ACPI_MEM_LIST_GLOBAL            0
#define ACPI_MEM_LIST_NSNODE            1
#define ACPI_MEM_LIST_MAX               1
#define ACPI_NUM_MEM_LISTS              2

#endif				
