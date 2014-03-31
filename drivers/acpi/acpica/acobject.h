

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

#ifndef _ACOBJECT_H
#define _ACOBJECT_H


#if ACPI_MACHINE_WIDTH == 64
#pragma pack(8)
#else
#pragma pack(4)
#endif


#define ACPI_OBJECT_COMMON_HEADER \
	union acpi_operand_object       *next_object;       \
	u8                              descriptor_type;    \
	u8                              type;               \
	u16                             reference_count;    \
	u8                              flags;


#define AOPOBJ_AML_CONSTANT         0x01	
#define AOPOBJ_STATIC_POINTER       0x02	
#define AOPOBJ_DATA_VALID           0x04	
#define AOPOBJ_OBJECT_INITIALIZED   0x08	
#define AOPOBJ_SETUP_COMPLETE       0x10	
#define AOPOBJ_INVALID              0x20	


struct acpi_object_common {
ACPI_OBJECT_COMMON_HEADER};

struct acpi_object_integer {
	ACPI_OBJECT_COMMON_HEADER u8 fill[3];	
	u64 value;
};

#define ACPI_COMMON_BUFFER_INFO(_type) \
	_type                           *pointer; \
	u32                             length;

struct acpi_object_string {	
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_BUFFER_INFO(char)	
};

struct acpi_object_buffer {
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_BUFFER_INFO(u8)	
	u32 aml_length;
	u8 *aml_start;
	struct acpi_namespace_node *node;	
};

struct acpi_object_package {
	ACPI_OBJECT_COMMON_HEADER struct acpi_namespace_node *node;	
	union acpi_operand_object **elements;	
	u8 *aml_start;
	u32 aml_length;
	u32 count;		
};


struct acpi_object_event {
	ACPI_OBJECT_COMMON_HEADER acpi_semaphore os_semaphore;	
};

struct acpi_object_mutex {
	ACPI_OBJECT_COMMON_HEADER u8 sync_level;	
	u16 acquisition_depth;	
	acpi_mutex os_mutex;	
	acpi_thread_id thread_id;	
	struct acpi_thread_state *owner_thread;	
	union acpi_operand_object *prev;	
	union acpi_operand_object *next;	
	struct acpi_namespace_node *node;	
	u8 original_sync_level;	
};

struct acpi_object_region {
	ACPI_OBJECT_COMMON_HEADER u8 space_id;
	struct acpi_namespace_node *node;	
	union acpi_operand_object *handler;	
	union acpi_operand_object *next;
	acpi_physical_address address;
	u32 length;
};

struct acpi_object_method {
	ACPI_OBJECT_COMMON_HEADER u8 info_flags;
	u8 param_count;
	u8 sync_level;
	union acpi_operand_object *mutex;
	u8 *aml_start;
	union {
		ACPI_INTERNAL_METHOD implementation;
		union acpi_operand_object *handler;
	} dispatch;

	u32 aml_length;
	u8 thread_count;
	acpi_owner_id owner_id;
};


#define ACPI_METHOD_MODULE_LEVEL        0x01	
#define ACPI_METHOD_INTERNAL_ONLY       0x02	
#define ACPI_METHOD_SERIALIZED          0x04	
#define ACPI_METHOD_SERIALIZED_PENDING  0x08	
#define ACPI_METHOD_MODIFIED_NAMESPACE  0x10	


#define ACPI_COMMON_NOTIFY_INFO \
	union acpi_operand_object       *system_notify;     \
	union acpi_operand_object       *device_notify;     \
	union acpi_operand_object       *handler;	

struct acpi_object_notify_common {	
ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_NOTIFY_INFO};

struct acpi_object_device {
	ACPI_OBJECT_COMMON_HEADER
	    ACPI_COMMON_NOTIFY_INFO struct acpi_gpe_block_info *gpe_block;
};

struct acpi_object_power_resource {
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_NOTIFY_INFO u32 system_level;
	u32 resource_order;
};

struct acpi_object_processor {
	ACPI_OBJECT_COMMON_HEADER
	    
	u8 proc_id;
	u8 length;
	ACPI_COMMON_NOTIFY_INFO acpi_io_address address;
};

struct acpi_object_thermal_zone {
ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_NOTIFY_INFO};


/*
 * Common bitfield for the field objects
 * "Field Datum"  -- a datum from the actual field object
 * "Buffer Datum" -- a datum from a user buffer, read from or to be written to the field
 */
#define ACPI_COMMON_FIELD_INFO \
	u8                              field_flags;        \
	u8                              attribute;          \
	u8                              access_byte_width;  \
	struct acpi_namespace_node      *node;              \
	u32                             bit_length;         \
	u32                             base_byte_offset;   \
	u32                             value;              \
	u8                              start_field_bit_offset;\
	u8                              access_length;	


struct acpi_object_field_common {	
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_FIELD_INFO union acpi_operand_object *region_obj;	
};

struct acpi_object_region_field {
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_FIELD_INFO u16 resource_length;
	union acpi_operand_object *region_obj;	
	u8 *resource_buffer;	
};

struct acpi_object_bank_field {
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_FIELD_INFO union acpi_operand_object *region_obj;	
	union acpi_operand_object *bank_obj;	
};

struct acpi_object_index_field {
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_FIELD_INFO
	union acpi_operand_object *index_obj;	
	union acpi_operand_object *data_obj;	
};


struct acpi_object_buffer_field {
	ACPI_OBJECT_COMMON_HEADER ACPI_COMMON_FIELD_INFO union acpi_operand_object *buffer_obj;	
};


struct acpi_object_notify_handler {
	ACPI_OBJECT_COMMON_HEADER struct acpi_namespace_node *node;	
	u32 handler_type;
	acpi_notify_handler handler;
	void *context;
	struct acpi_object_notify_handler *next;
};

struct acpi_object_addr_handler {
	ACPI_OBJECT_COMMON_HEADER u8 space_id;
	u8 handler_flags;
	acpi_adr_space_handler handler;
	struct acpi_namespace_node *node;	
	void *context;
	acpi_adr_space_setup setup;
	union acpi_operand_object *region_list;	
	union acpi_operand_object *next;
};


#define ACPI_ADDR_HANDLER_DEFAULT_INSTALLED  0x01


struct acpi_object_reference {
	ACPI_OBJECT_COMMON_HEADER u8 class;	
	u8 target_type;		
	u8 reserved;
	void *object;		
	struct acpi_namespace_node *node;	
	union acpi_operand_object **where;	
	u32 value;		
};


typedef enum {
	ACPI_REFCLASS_LOCAL = 0,	
	ACPI_REFCLASS_ARG = 1,	
	ACPI_REFCLASS_REFOF = 2,	
	ACPI_REFCLASS_INDEX = 3,	
	ACPI_REFCLASS_TABLE = 4,	
	ACPI_REFCLASS_NAME = 5,	
	ACPI_REFCLASS_DEBUG = 6,	

	ACPI_REFCLASS_MAX = 6
} ACPI_REFERENCE_CLASSES;

struct acpi_object_extra {
	ACPI_OBJECT_COMMON_HEADER struct acpi_namespace_node *method_REG;	
	struct acpi_namespace_node *scope_node;
	void *region_context;	
	u8 *aml_start;
	u32 aml_length;
};


struct acpi_object_data {
	ACPI_OBJECT_COMMON_HEADER acpi_object_handler handler;
	void *pointer;
};


struct acpi_object_cache_list {
	ACPI_OBJECT_COMMON_HEADER union acpi_operand_object *next;	
};


union acpi_operand_object {
	struct acpi_object_common common;
	struct acpi_object_integer integer;
	struct acpi_object_string string;
	struct acpi_object_buffer buffer;
	struct acpi_object_package package;
	struct acpi_object_event event;
	struct acpi_object_method method;
	struct acpi_object_mutex mutex;
	struct acpi_object_region region;
	struct acpi_object_notify_common common_notify;
	struct acpi_object_device device;
	struct acpi_object_power_resource power_resource;
	struct acpi_object_processor processor;
	struct acpi_object_thermal_zone thermal_zone;
	struct acpi_object_field_common common_field;
	struct acpi_object_region_field field;
	struct acpi_object_buffer_field buffer_field;
	struct acpi_object_bank_field bank_field;
	struct acpi_object_index_field index_field;
	struct acpi_object_notify_handler notify;
	struct acpi_object_addr_handler address_space;
	struct acpi_object_reference reference;
	struct acpi_object_extra extra;
	struct acpi_object_data data;
	struct acpi_object_cache_list cache;

	struct acpi_namespace_node node;
};



#define ACPI_DESC_TYPE_CACHED           0x01	
#define ACPI_DESC_TYPE_STATE            0x02
#define ACPI_DESC_TYPE_STATE_UPDATE     0x03
#define ACPI_DESC_TYPE_STATE_PACKAGE    0x04
#define ACPI_DESC_TYPE_STATE_CONTROL    0x05
#define ACPI_DESC_TYPE_STATE_RPSCOPE    0x06
#define ACPI_DESC_TYPE_STATE_PSCOPE     0x07
#define ACPI_DESC_TYPE_STATE_WSCOPE     0x08
#define ACPI_DESC_TYPE_STATE_RESULT     0x09
#define ACPI_DESC_TYPE_STATE_NOTIFY     0x0A
#define ACPI_DESC_TYPE_STATE_THREAD     0x0B
#define ACPI_DESC_TYPE_WALK             0x0C
#define ACPI_DESC_TYPE_PARSER           0x0D
#define ACPI_DESC_TYPE_OPERAND          0x0E
#define ACPI_DESC_TYPE_NAMED            0x0F
#define ACPI_DESC_TYPE_MAX              0x0F

struct acpi_common_descriptor {
	void *common_pointer;
	u8 descriptor_type;	
};

union acpi_descriptor {
	struct acpi_common_descriptor common;
	union acpi_operand_object object;
	struct acpi_namespace_node node;
	union acpi_parse_object op;
};

#pragma pack()

#endif				
