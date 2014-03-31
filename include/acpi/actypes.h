
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

#ifndef __ACTYPES_H__
#define __ACTYPES_H__


#ifndef ACPI_MACHINE_WIDTH
#error ACPI_MACHINE_WIDTH not defined
#endif


#define ACPI_UINT8_MAX                  (UINT8) (~((UINT8)  0))	
#define ACPI_UINT16_MAX                 (UINT16)(~((UINT16) 0))	
#define ACPI_UINT32_MAX                 (UINT32)(~((UINT32) 0))	
#define ACPI_UINT64_MAX                 (UINT64)(~((UINT64) 0))	
#define ACPI_ASCII_MAX                  0x7F



typedef unsigned char BOOLEAN;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef COMPILER_DEPENDENT_UINT64 UINT64;
typedef COMPILER_DEPENDENT_INT64 INT64;


#define acpi_thread_id                  u64


#if ACPI_MACHINE_WIDTH == 64


typedef unsigned int UINT32;
typedef int INT32;


typedef s64 acpi_native_int;

typedef u64 acpi_size;
typedef u64 acpi_io_address;
typedef u64 acpi_physical_address;

#define ACPI_MAX_PTR                    ACPI_UINT64_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT64_MAX

#define ACPI_USE_NATIVE_DIVIDE	

#if defined (__IA64__) || defined (__ia64__)
#define ACPI_MISALIGNMENT_NOT_SUPPORTED
#endif


#elif ACPI_MACHINE_WIDTH == 32


typedef unsigned int UINT32;
typedef int INT32;


typedef s32 acpi_native_int;

typedef u32 acpi_size;
typedef u32 acpi_io_address;
typedef u32 acpi_physical_address;

#define ACPI_MAX_PTR                    ACPI_UINT32_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT32_MAX

#else


#error unknown ACPI_MACHINE_WIDTH
#endif



#ifndef acpi_cpu_flags
#define acpi_cpu_flags			acpi_size
#endif


#ifndef acpi_cache_t
#ifdef ACPI_USE_LOCAL_CACHE
#define acpi_cache_t                    struct acpi_memory_list
#else
#define acpi_cache_t                    void *
#endif
#endif

#if (ACPI_MUTEX_TYPE == ACPI_BINARY_SEMAPHORE)
#define acpi_mutex                      acpi_semaphore
#define acpi_os_create_mutex(out_handle) acpi_os_create_semaphore (1, 1, out_handle)
#define acpi_os_delete_mutex(handle)    (void) acpi_os_delete_semaphore (handle)
#define acpi_os_acquire_mutex(handle,time) acpi_os_wait_semaphore (handle, 1, time)
#define acpi_os_release_mutex(handle)   (void) acpi_os_signal_semaphore (handle, 1)
#endif


#ifndef acpi_spinlock
#define acpi_spinlock                   void *
#endif

#ifndef acpi_semaphore
#define acpi_semaphore                  void *
#endif

#ifndef acpi_mutex
#define acpi_mutex                      void *
#endif



#ifndef acpi_uintptr_t
#define acpi_uintptr_t                  void *
#endif

#ifndef ACPI_PRINTF_LIKE
#define ACPI_PRINTF_LIKE(c)
#endif

#ifndef ACPI_UNUSED_VAR
#define ACPI_UNUSED_VAR
#endif

#ifndef ACPI_EXPORT_SYMBOL
#define ACPI_EXPORT_SYMBOL(symbol)
#endif



#define ACPI_MAX_GPE_BLOCKS             2


#define ACPI_GPE_REGISTER_WIDTH         8
#define ACPI_PM1_REGISTER_WIDTH         16
#define ACPI_PM2_REGISTER_WIDTH         8
#define ACPI_PM_TIMER_WIDTH             32


#define ACPI_NAME_SIZE                  4
#define ACPI_PATH_SEGMENT_LENGTH        5	
#define ACPI_PATH_SEPARATOR             '.'


#define ACPI_OEM_ID_SIZE                6
#define ACPI_OEM_TABLE_ID_SIZE          8


#define PCI_ROOT_HID_STRING             "PNP0A03"
#define PCI_EXPRESS_ROOT_HID_STRING     "PNP0A08"


#define PM_TIMER_FREQUENCY              3579545



#ifdef FALSE
#undef FALSE
#endif
#define FALSE                           (1 == 0)

#ifdef TRUE
#undef TRUE
#endif
#define TRUE                            (1 == 1)

#ifndef NULL
#define NULL                            (void *) 0
#endif

typedef u32 acpi_status;	
typedef u32 acpi_name;		
typedef char *acpi_string;	
typedef void *acpi_handle;	


typedef u8 acpi_owner_id;
#define ACPI_OWNER_ID_MAX               0xFF

#define ACPI_INTEGER_BIT_SIZE           64
#define ACPI_MAX_DECIMAL_DIGITS         20	

#if ACPI_MACHINE_WIDTH == 64
#define ACPI_USE_NATIVE_DIVIDE	
#endif

#define ACPI_MAX64_DECIMAL_DIGITS       20
#define ACPI_MAX32_DECIMAL_DIGITS       10
#define ACPI_MAX16_DECIMAL_DIGITS        5
#define ACPI_MAX8_DECIMAL_DIGITS         3


#define PM_TIMER_FREQUENCY  3579545

#define ACPI_ROOT_OBJECT                ACPI_ADD_PTR (acpi_handle, NULL, ACPI_MAX_PTR)
#define ACPI_WAIT_FOREVER               0xFFFF	
#define ACPI_DO_NOT_WAIT                0

typedef u64 acpi_integer;
#define ACPI_INTEGER_MAX                ACPI_UINT64_MAX



#define ACPI_LOBYTE(integer)            ((u8)   (u16)(integer))
#define ACPI_HIBYTE(integer)            ((u8) (((u16)(integer)) >> 8))
#define ACPI_LOWORD(integer)            ((u16)  (u32)(integer))
#define ACPI_HIWORD(integer)            ((u16)(((u32)(integer)) >> 16))
#define ACPI_LODWORD(integer64)         ((u32)  (u64)(integer64))
#define ACPI_HIDWORD(integer64)         ((u32)(((u64)(integer64)) >> 32))

#define ACPI_SET_BIT(target,bit)        ((target) |= (bit))
#define ACPI_CLEAR_BIT(target,bit)      ((target) &= ~(bit))
#define ACPI_MIN(a,b)                   (((a)<(b))?(a):(b))
#define ACPI_MAX(a,b)                   (((a)>(b))?(a):(b))


#define ACPI_ARRAY_LENGTH(x)            (sizeof(x) / sizeof((x)[0]))


#define ACPI_CAST_PTR(t, p)             ((t *) (acpi_uintptr_t) (p))
#define ACPI_CAST_INDIRECT_PTR(t, p)    ((t **) (acpi_uintptr_t) (p))
#define ACPI_ADD_PTR(t, a, b)           ACPI_CAST_PTR (t, (ACPI_CAST_PTR (u8, (a)) + (acpi_size)(b)))
#define ACPI_PTR_DIFF(a, b)             (acpi_size) (ACPI_CAST_PTR (u8, (a)) - ACPI_CAST_PTR (u8, (b)))


#define ACPI_TO_POINTER(i)              ACPI_ADD_PTR (void, (void *) NULL,(acpi_size) i)
#define ACPI_TO_INTEGER(p)              ACPI_PTR_DIFF (p, (void *) NULL)
#define ACPI_OFFSET(d, f)               (acpi_size) ACPI_PTR_DIFF (&(((d *)0)->f), (void *) NULL)
#define ACPI_PHYSADDR_TO_PTR(i)         ACPI_TO_POINTER(i)
#define ACPI_PTR_TO_PHYSADDR(i)         ACPI_TO_INTEGER(i)

#ifndef ACPI_MISALIGNMENT_NOT_SUPPORTED
#define ACPI_COMPARE_NAME(a,b)          (*ACPI_CAST_PTR (u32, (a)) == *ACPI_CAST_PTR (u32, (b)))
#else
#define ACPI_COMPARE_NAME(a,b)          (!ACPI_STRNCMP (ACPI_CAST_PTR (char, (a)), ACPI_CAST_PTR (char, (b)), ACPI_NAME_SIZE))
#endif


#define ACPI_FULL_INITIALIZATION        0x00
#define ACPI_NO_ADDRESS_SPACE_INIT      0x01
#define ACPI_NO_EVENT_INIT              0x04
#define ACPI_NO_HANDLER_INIT            0x08
#define ACPI_NO_ACPI_ENABLE             0x10
#define ACPI_NO_DEVICE_INIT             0x20
#define ACPI_NO_OBJECT_INIT             0x40

#define ACPI_SUBSYSTEM_INITIALIZE       0x01
#define ACPI_INITIALIZED_OK             0x02

#define ACPI_STATE_UNKNOWN              (u8) 0xFF

#define ACPI_STATE_S0                   (u8) 0
#define ACPI_STATE_S1                   (u8) 1
#define ACPI_STATE_S2                   (u8) 2
#define ACPI_STATE_S3                   (u8) 3
#define ACPI_STATE_S4                   (u8) 4
#define ACPI_STATE_S5                   (u8) 5
#define ACPI_S_STATES_MAX               ACPI_STATE_S5
#define ACPI_S_STATE_COUNT              6

#define ACPI_STATE_D0                   (u8) 0
#define ACPI_STATE_D1                   (u8) 1
#define ACPI_STATE_D2                   (u8) 2
#define ACPI_STATE_D3_HOT               (u8) 3
#define ACPI_STATE_D3                   (u8) 4
#define ACPI_STATE_D3_COLD              ACPI_STATE_D3
#define ACPI_D_STATES_MAX               ACPI_STATE_D3
#define ACPI_D_STATE_COUNT              5

#define ACPI_STATE_C0                   (u8) 0
#define ACPI_STATE_C1                   (u8) 1
#define ACPI_STATE_C2                   (u8) 2
#define ACPI_STATE_C3                   (u8) 3
#define ACPI_C_STATES_MAX               ACPI_STATE_C3
#define ACPI_C_STATE_COUNT              4

#define ACPI_SLEEP_TYPE_MAX             0x7
#define ACPI_SLEEP_TYPE_INVALID         0xFF

#define ACPI_NO_OPTIONAL_METHODS        0x00	
#define ACPI_EXECUTE_GTS                0x01	
#define ACPI_EXECUTE_BFS                0x02	

#define ACPI_NOTIFY_BUS_CHECK           (u8) 0x00
#define ACPI_NOTIFY_DEVICE_CHECK        (u8) 0x01
#define ACPI_NOTIFY_DEVICE_WAKE         (u8) 0x02
#define ACPI_NOTIFY_EJECT_REQUEST       (u8) 0x03
#define ACPI_NOTIFY_DEVICE_CHECK_LIGHT  (u8) 0x04
#define ACPI_NOTIFY_FREQUENCY_MISMATCH  (u8) 0x05
#define ACPI_NOTIFY_BUS_MODE_MISMATCH   (u8) 0x06
#define ACPI_NOTIFY_POWER_FAULT         (u8) 0x07
#define ACPI_NOTIFY_CAPABILITIES_CHECK  (u8) 0x08
#define ACPI_NOTIFY_DEVICE_PLD_CHECK    (u8) 0x09
#define ACPI_NOTIFY_RESERVED            (u8) 0x0A
#define ACPI_NOTIFY_LOCALITY_UPDATE     (u8) 0x0B
#define ACPI_NOTIFY_SHUTDOWN_REQUEST    (u8) 0x0C

#define ACPI_NOTIFY_MAX                 0x0C

typedef u32 acpi_object_type;

#define ACPI_TYPE_ANY                   0x00
#define ACPI_TYPE_INTEGER               0x01	
#define ACPI_TYPE_STRING                0x02
#define ACPI_TYPE_BUFFER                0x03
#define ACPI_TYPE_PACKAGE               0x04	
#define ACPI_TYPE_FIELD_UNIT            0x05
#define ACPI_TYPE_DEVICE                0x06	
#define ACPI_TYPE_EVENT                 0x07
#define ACPI_TYPE_METHOD                0x08	
#define ACPI_TYPE_MUTEX                 0x09
#define ACPI_TYPE_REGION                0x0A
#define ACPI_TYPE_POWER                 0x0B	
#define ACPI_TYPE_PROCESSOR             0x0C	
#define ACPI_TYPE_THERMAL               0x0D	
#define ACPI_TYPE_BUFFER_FIELD          0x0E
#define ACPI_TYPE_DDB_HANDLE            0x0F
#define ACPI_TYPE_DEBUG_OBJECT          0x10

#define ACPI_TYPE_EXTERNAL_MAX          0x10

#define ACPI_TYPE_LOCAL_REGION_FIELD    0x11
#define ACPI_TYPE_LOCAL_BANK_FIELD      0x12
#define ACPI_TYPE_LOCAL_INDEX_FIELD     0x13
#define ACPI_TYPE_LOCAL_REFERENCE       0x14	
#define ACPI_TYPE_LOCAL_ALIAS           0x15
#define ACPI_TYPE_LOCAL_METHOD_ALIAS    0x16
#define ACPI_TYPE_LOCAL_NOTIFY          0x17
#define ACPI_TYPE_LOCAL_ADDRESS_HANDLER 0x18
#define ACPI_TYPE_LOCAL_RESOURCE        0x19
#define ACPI_TYPE_LOCAL_RESOURCE_FIELD  0x1A
#define ACPI_TYPE_LOCAL_SCOPE           0x1B	

#define ACPI_TYPE_NS_NODE_MAX           0x1B	

#define ACPI_TYPE_LOCAL_EXTRA           0x1C
#define ACPI_TYPE_LOCAL_DATA            0x1D

#define ACPI_TYPE_LOCAL_MAX             0x1D


#define ACPI_TYPE_INVALID               0x1E
#define ACPI_TYPE_NOT_FOUND             0xFF

#define ACPI_NUM_NS_TYPES               (ACPI_TYPE_INVALID + 1)

#define ACPI_READ                       0
#define ACPI_WRITE                      1
#define ACPI_IO_MASK                    1

typedef u32 acpi_event_type;

#define ACPI_EVENT_PMTIMER              0
#define ACPI_EVENT_GLOBAL               1
#define ACPI_EVENT_POWER_BUTTON         2
#define ACPI_EVENT_SLEEP_BUTTON         3
#define ACPI_EVENT_RTC                  4
#define ACPI_EVENT_MAX                  4
#define ACPI_NUM_FIXED_EVENTS           ACPI_EVENT_MAX + 1

typedef u32 acpi_event_status;

#define ACPI_EVENT_FLAG_DISABLED        (acpi_event_status) 0x00
#define ACPI_EVENT_FLAG_ENABLED         (acpi_event_status) 0x01
#define ACPI_EVENT_FLAG_WAKE_ENABLED    (acpi_event_status) 0x02
#define ACPI_EVENT_FLAG_SET             (acpi_event_status) 0x04
#define ACPI_EVENT_FLAG_HANDLE		(acpi_event_status) 0x08

#define ACPI_GPE_INVALID                0xFF
#define ACPI_GPE_MAX                    0xFF
#define ACPI_NUM_GPE                    256


#define ACPI_GPE_ENABLE                 0
#define ACPI_GPE_DISABLE                1
#define ACPI_GPE_CONDITIONAL_ENABLE     2

#define ACPI_GPE_DISPATCH_NONE          (u8) 0x00
#define ACPI_GPE_DISPATCH_METHOD        (u8) 0x01
#define ACPI_GPE_DISPATCH_HANDLER       (u8) 0x02
#define ACPI_GPE_DISPATCH_NOTIFY        (u8) 0x03
#define ACPI_GPE_DISPATCH_MASK          (u8) 0x03

#define ACPI_GPE_LEVEL_TRIGGERED        (u8) 0x04
#define ACPI_GPE_EDGE_TRIGGERED         (u8) 0x00
#define ACPI_GPE_XRUPT_TYPE_MASK        (u8) 0x04

#define ACPI_GPE_CAN_WAKE               (u8) 0x08

#define ACPI_NOT_ISR                    0x1
#define ACPI_ISR                        0x0


#define ACPI_SYSTEM_NOTIFY              0x1
#define ACPI_DEVICE_NOTIFY              0x2
#define ACPI_ALL_NOTIFY                 (ACPI_SYSTEM_NOTIFY | ACPI_DEVICE_NOTIFY)
#define ACPI_MAX_NOTIFY_HANDLER_TYPE    0x3

#define ACPI_MAX_SYS_NOTIFY             0x7F
#define ACPI_MAX_DEVICE_SPECIFIC_NOTIFY 0xBF


typedef u8 acpi_adr_space_type;

#define ACPI_ADR_SPACE_SYSTEM_MEMORY    (acpi_adr_space_type) 0
#define ACPI_ADR_SPACE_SYSTEM_IO        (acpi_adr_space_type) 1
#define ACPI_ADR_SPACE_PCI_CONFIG       (acpi_adr_space_type) 2
#define ACPI_ADR_SPACE_EC               (acpi_adr_space_type) 3
#define ACPI_ADR_SPACE_SMBUS            (acpi_adr_space_type) 4
#define ACPI_ADR_SPACE_CMOS             (acpi_adr_space_type) 5
#define ACPI_ADR_SPACE_PCI_BAR_TARGET   (acpi_adr_space_type) 6
#define ACPI_ADR_SPACE_IPMI             (acpi_adr_space_type) 7
#define ACPI_ADR_SPACE_GPIO             (acpi_adr_space_type) 8
#define ACPI_ADR_SPACE_GSBUS            (acpi_adr_space_type) 9

#define ACPI_NUM_PREDEFINED_REGIONS     10

#define ACPI_ADR_SPACE_DATA_TABLE       (acpi_adr_space_type) 0x7E	
#define ACPI_ADR_SPACE_FIXED_HARDWARE   (acpi_adr_space_type) 0x7F


#define ACPI_REG_DISCONNECT             0
#define ACPI_REG_CONNECT                1



#define ACPI_BITREG_TIMER_STATUS                0x00
#define ACPI_BITREG_BUS_MASTER_STATUS           0x01
#define ACPI_BITREG_GLOBAL_LOCK_STATUS          0x02
#define ACPI_BITREG_POWER_BUTTON_STATUS         0x03
#define ACPI_BITREG_SLEEP_BUTTON_STATUS         0x04
#define ACPI_BITREG_RT_CLOCK_STATUS             0x05
#define ACPI_BITREG_WAKE_STATUS                 0x06
#define ACPI_BITREG_PCIEXP_WAKE_STATUS          0x07


#define ACPI_BITREG_TIMER_ENABLE                0x08
#define ACPI_BITREG_GLOBAL_LOCK_ENABLE          0x09
#define ACPI_BITREG_POWER_BUTTON_ENABLE         0x0A
#define ACPI_BITREG_SLEEP_BUTTON_ENABLE         0x0B
#define ACPI_BITREG_RT_CLOCK_ENABLE             0x0C
#define ACPI_BITREG_PCIEXP_WAKE_DISABLE         0x0D


#define ACPI_BITREG_SCI_ENABLE                  0x0E
#define ACPI_BITREG_BUS_MASTER_RLD              0x0F
#define ACPI_BITREG_GLOBAL_LOCK_RELEASE         0x10
#define ACPI_BITREG_SLEEP_TYPE                  0x11
#define ACPI_BITREG_SLEEP_ENABLE                0x12


#define ACPI_BITREG_ARB_DISABLE                 0x13

#define ACPI_BITREG_MAX                         0x13
#define ACPI_NUM_BITREG                         ACPI_BITREG_MAX + 1


#define ACPI_CLEAR_STATUS                       1


#define ACPI_ENABLE_EVENT                       1
#define ACPI_DISABLE_EVENT                      0


typedef acpi_status(*ACPI_SLEEP_FUNCTION) (u8 sleep_state, u8 flags);

struct acpi_sleep_functions {
	ACPI_SLEEP_FUNCTION legacy_function;
	ACPI_SLEEP_FUNCTION extended_function;
};


union acpi_object {
	acpi_object_type type;	
	struct {
		acpi_object_type type;	
		u64 value;	
	} integer;

	struct {
		acpi_object_type type;	
		u32 length;	
		char *pointer;	
	} string;

	struct {
		acpi_object_type type;	
		u32 length;	
		u8 *pointer;	
	} buffer;

	struct {
		acpi_object_type type;	
		u32 count;	
		union acpi_object *elements;	
	} package;

	struct {
		acpi_object_type type;	
		acpi_object_type actual_type;	
		acpi_handle handle;	
	} reference;

	struct {
		acpi_object_type type;	
		u32 proc_id;
		acpi_io_address pblk_address;
		u32 pblk_length;
	} processor;

	struct {
		acpi_object_type type;	
		u32 system_level;
		u32 resource_order;
	} power_resource;
};

struct acpi_object_list {
	u32 count;
	union acpi_object *pointer;
};

#define ACPI_NO_BUFFER              0
#define ACPI_ALLOCATE_BUFFER        (acpi_size) (-1)
#define ACPI_ALLOCATE_LOCAL_BUFFER  (acpi_size) (-2)

struct acpi_buffer {
	acpi_size length;	
	void *pointer;		
};

#define ACPI_FULL_PATHNAME              0
#define ACPI_SINGLE_NAME                1
#define ACPI_NAME_TYPE_MAX              1

struct acpi_predefined_names {
	char *name;
	u8 type;
	char *val;
};

#define ACPI_SYS_MODE_UNKNOWN           0x0000
#define ACPI_SYS_MODE_ACPI              0x0001
#define ACPI_SYS_MODE_LEGACY            0x0002
#define ACPI_SYS_MODES_MASK             0x0003

struct acpi_system_info {
	u32 acpi_ca_version;
	u32 flags;
	u32 timer_resolution;
	u32 reserved1;
	u32 reserved2;
	u32 debug_level;
	u32 debug_layer;
};


#define ACPI_TABLE_EVENT_LOAD           0x0
#define ACPI_TABLE_EVENT_UNLOAD         0x1
#define ACPI_NUM_TABLE_EVENTS           2

typedef u32(ACPI_SYSTEM_XFACE * acpi_osd_handler) (void *context);

typedef void
 (ACPI_SYSTEM_XFACE * acpi_osd_exec_callback) (void *context);

typedef
void (*ACPI_GBL_EVENT_HANDLER) (u32 event_type,
			       acpi_handle device,
			       u32 event_number, void *context);

#define ACPI_EVENT_TYPE_GPE         0
#define ACPI_EVENT_TYPE_FIXED       1

typedef u32(*acpi_event_handler) (void *context);

typedef
u32 (*acpi_gpe_handler) (acpi_handle gpe_device, u32 gpe_number, void *context);

typedef
void (*acpi_notify_handler) (acpi_handle device, u32 value, void *context);

typedef
void (*acpi_object_handler) (acpi_handle object, void *data);

typedef acpi_status(*acpi_init_handler) (acpi_handle object, u32 function);

#define ACPI_INIT_DEVICE_INI        1

typedef
acpi_status(*acpi_exception_handler) (acpi_status aml_status,
				      acpi_name name,
				      u16 opcode,
				      u32 aml_offset, void *context);


typedef
acpi_status(*acpi_tbl_handler) (u32 event, void *table, void *context);


typedef
acpi_status(*acpi_adr_space_handler) (u32 function,
				      acpi_physical_address address,
				      u32 bit_width,
				      u64 *value,
				      void *handler_context,
				      void *region_context);

#define ACPI_DEFAULT_HANDLER            NULL


struct acpi_connection_info {
	u8 *connection;
	u16 length;
	u8 access_length;
};

typedef
acpi_status(*acpi_adr_space_setup) (acpi_handle region_handle,
				    u32 function,
				    void *handler_context,
				    void **region_context);

#define ACPI_REGION_ACTIVATE    0
#define ACPI_REGION_DEACTIVATE  1

typedef
acpi_status(*acpi_walk_callback) (acpi_handle object,
				  u32 nesting_level,
				  void *context, void **return_value);

typedef
u32 (*acpi_interface_handler) (acpi_string interface_name, u32 supported);


#define ACPI_INTERRUPT_NOT_HANDLED      0x00
#define ACPI_INTERRUPT_HANDLED          0x01


#define ACPI_REENABLE_GPE               0x80


#define ACPI_EISAID_STRING_SIZE         8	


#define ACPI_UUID_LENGTH                16


struct acpica_device_id {
	u32 length;		
	char *string;
};

struct acpica_device_id_list {
	u32 count;		
	u32 list_size;		
	struct acpica_device_id ids[1];	
};

struct acpi_device_info {
	u32 info_size;		
	u32 name;		
	acpi_object_type type;	
	u8 param_count;		
	u8 valid;		
	u8 flags;		
	u8 highest_dstates[4];	
	u8 lowest_dstates[5];	
	u32 current_status;	
	u64 address;	
	struct acpica_device_id hardware_id;	
	struct acpica_device_id unique_id;	
	struct acpica_device_id_list compatible_id_list;	
};


#define ACPI_PCI_ROOT_BRIDGE            0x01


#define ACPI_VALID_STA                  0x01
#define ACPI_VALID_ADR                  0x02
#define ACPI_VALID_HID                  0x04
#define ACPI_VALID_UID                  0x08
#define ACPI_VALID_CID                  0x10
#define ACPI_VALID_SXDS                 0x20
#define ACPI_VALID_SXWS                 0x40


#define ACPI_STA_DEVICE_PRESENT         0x01
#define ACPI_STA_DEVICE_ENABLED         0x02
#define ACPI_STA_DEVICE_UI              0x04
#define ACPI_STA_DEVICE_FUNCTIONING     0x08
#define ACPI_STA_DEVICE_OK              0x08	
#define ACPI_STA_BATTERY_PRESENT        0x10


struct acpi_pci_id {
	u16 segment;
	u16 bus;
	u16 device;
	u16 function;
};

struct acpi_mem_space_context {
	u32 length;
	acpi_physical_address address;
	acpi_physical_address mapped_physical_address;
	u8 *mapped_logical_address;
	acpi_size mapped_length;
};

struct acpi_memory_list {
	char *list_name;
	void *list_head;
	u16 object_size;
	u16 max_depth;
	u16 current_depth;
	u16 link_offset;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS

	

	u32 total_allocated;
	u32 total_freed;
	u32 max_occupied;
	u32 total_size;
	u32 current_total_size;
	u32 requests;
	u32 hits;
#endif
};

#endif				
