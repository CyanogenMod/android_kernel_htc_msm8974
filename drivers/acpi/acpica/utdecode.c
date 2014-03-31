
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

#include <linux/export.h>
#include <acpi/acpi.h>
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utdecode")

const char *acpi_format_exception(acpi_status status)
{
	const char *exception = NULL;

	ACPI_FUNCTION_ENTRY();

	exception = acpi_ut_validate_exception(status);
	if (!exception) {

		

		ACPI_ERROR((AE_INFO,
			    "Unknown exception code: 0x%8.8X", status));

		exception = "UNKNOWN_STATUS_CODE";
	}

	return (ACPI_CAST_PTR(const char, exception));
}

ACPI_EXPORT_SYMBOL(acpi_format_exception)

const u8 acpi_gbl_ns_properties[ACPI_NUM_NS_TYPES] = {
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NEWSCOPE,	
	ACPI_NS_NORMAL,		
	ACPI_NS_NEWSCOPE,	
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NEWSCOPE,	
	ACPI_NS_NEWSCOPE,	
	ACPI_NS_NEWSCOPE,	
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,	
	ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,	
	ACPI_NS_NEWSCOPE,	
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL,		
	ACPI_NS_NORMAL		
};



static const char acpi_gbl_hex_to_ascii[] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

char acpi_ut_hex_to_ascii_char(u64 integer, u32 position)
{

	return (acpi_gbl_hex_to_ascii[(integer >> position) & 0xF]);
}



const char *acpi_gbl_region_types[ACPI_NUM_PREDEFINED_REGIONS] = {
	"SystemMemory",
	"SystemIO",
	"PCI_Config",
	"EmbeddedControl",
	"SMBus",
	"SystemCMOS",
	"PCIBARTarget",
	"IPMI",
	"GeneralPurposeIo",
	"GenericSerialBus"
};

char *acpi_ut_get_region_name(u8 space_id)
{

	if (space_id >= ACPI_USER_REGION_BEGIN) {
		return ("UserDefinedRegion");
	} else if (space_id == ACPI_ADR_SPACE_DATA_TABLE) {
		return ("DataTable");
	} else if (space_id == ACPI_ADR_SPACE_FIXED_HARDWARE) {
		return ("FunctionalFixedHW");
	} else if (space_id >= ACPI_NUM_PREDEFINED_REGIONS) {
		return ("InvalidSpaceId");
	}

	return (ACPI_CAST_PTR(char, acpi_gbl_region_types[space_id]));
}



static const char *acpi_gbl_event_types[ACPI_NUM_FIXED_EVENTS] = {
	"PM_Timer",
	"GlobalLock",
	"PowerButton",
	"SleepButton",
	"RealTimeClock",
};

char *acpi_ut_get_event_name(u32 event_id)
{

	if (event_id > ACPI_EVENT_MAX) {
		return ("InvalidEventID");
	}

	return (ACPI_CAST_PTR(char, acpi_gbl_event_types[event_id]));
}


static const char acpi_gbl_bad_type[] = "UNDEFINED";


static const char *acpi_gbl_ns_type_names[] = {
	 "Untyped",
	 "Integer",
	 "String",
	 "Buffer",
	 "Package",
	 "FieldUnit",
	 "Device",
	 "Event",
	 "Method",
	 "Mutex",
	 "Region",
	 "Power",
	 "Processor",
	 "Thermal",
	 "BufferField",
	 "DdbHandle",
	 "DebugObject",
	 "RegionField",
	 "BankField",
	 "IndexField",
	 "Reference",
	 "Alias",
	 "MethodAlias",
	 "Notify",
	 "AddrHandler",
	 "ResourceDesc",
	 "ResourceFld",
	 "Scope",
	 "Extra",
	 "Data",
	 "Invalid"
};

char *acpi_ut_get_type_name(acpi_object_type type)
{

	if (type > ACPI_TYPE_INVALID) {
		return (ACPI_CAST_PTR(char, acpi_gbl_bad_type));
	}

	return (ACPI_CAST_PTR(char, acpi_gbl_ns_type_names[type]));
}

char *acpi_ut_get_object_type_name(union acpi_operand_object *obj_desc)
{

	if (!obj_desc) {
		return ("[NULL Object Descriptor]");
	}

	return (acpi_ut_get_type_name(obj_desc->common.type));
}


char *acpi_ut_get_node_name(void *object)
{
	struct acpi_namespace_node *node = (struct acpi_namespace_node *)object;

	

	if (!object) {
		return ("NULL");
	}

	

	if ((object == ACPI_ROOT_OBJECT) || (object == acpi_gbl_root_node)) {
		return ("\"\\\" ");
	}

	

	if (ACPI_GET_DESCRIPTOR_TYPE(node) != ACPI_DESC_TYPE_NAMED) {
		return ("####");
	}

	acpi_ut_repair_name(node->name.ascii);

	

	return (node->name.ascii);
}



static const char *acpi_gbl_desc_type_names[] = {
	 "Not a Descriptor",
	 "Cached",
	 "State-Generic",
	 "State-Update",
	 "State-Package",
	 "State-Control",
	 "State-RootParseScope",
	 "State-ParseScope",
	 "State-WalkScope",
	 "State-Result",
	 "State-Notify",
	 "State-Thread",
	 "Walk",
	 "Parser",
	 "Operand",
	 "Node"
};

char *acpi_ut_get_descriptor_name(void *object)
{

	if (!object) {
		return ("NULL OBJECT");
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(object) > ACPI_DESC_TYPE_MAX) {
		return ("Not a Descriptor");
	}

	return (ACPI_CAST_PTR(char,
			      acpi_gbl_desc_type_names[ACPI_GET_DESCRIPTOR_TYPE
						       (object)]));

}



static const char *acpi_gbl_ref_class_names[] = {
	 "Local",
	 "Argument",
	 "RefOf",
	 "Index",
	 "DdbHandle",
	 "Named Object",
	 "Debug"
};

const char *acpi_ut_get_reference_name(union acpi_operand_object *object)
{

	if (!object) {
		return ("NULL Object");
	}

	if (ACPI_GET_DESCRIPTOR_TYPE(object) != ACPI_DESC_TYPE_OPERAND) {
		return ("Not an Operand object");
	}

	if (object->common.type != ACPI_TYPE_LOCAL_REFERENCE) {
		return ("Not a Reference object");
	}

	if (object->reference.class > ACPI_REFCLASS_MAX) {
		return ("Unknown Reference class");
	}

	return (acpi_gbl_ref_class_names[object->reference.class]);
}

#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)



static char *acpi_gbl_mutex_names[ACPI_NUM_MUTEX] = {
	"ACPI_MTX_Interpreter",
	"ACPI_MTX_Namespace",
	"ACPI_MTX_Tables",
	"ACPI_MTX_Events",
	"ACPI_MTX_Caches",
	"ACPI_MTX_Memory",
	"ACPI_MTX_CommandComplete",
	"ACPI_MTX_CommandReady"
};

char *acpi_ut_get_mutex_name(u32 mutex_id)
{

	if (mutex_id > ACPI_MAX_MUTEX) {
		return ("Invalid Mutex ID");
	}

	return (acpi_gbl_mutex_names[mutex_id]);
}



static const char *acpi_gbl_notify_value_names[ACPI_NOTIFY_MAX + 1] = {
	 "Bus Check",
	 "Device Check",
	 "Device Wake",
	 "Eject Request",
	 "Device Check Light",
	 "Frequency Mismatch",
	 "Bus Mode Mismatch",
	 "Power Fault",
	 "Capabilities Check",
	 "Device PLD Check",
	 "Reserved",
	 "System Locality Update",
	 "Shutdown Request"
};

const char *acpi_ut_get_notify_name(u32 notify_value)
{

	if (notify_value <= ACPI_NOTIFY_MAX) {
		return (acpi_gbl_notify_value_names[notify_value]);
	} else if (notify_value <= ACPI_MAX_SYS_NOTIFY) {
		return ("Reserved");
	} else if (notify_value <= ACPI_MAX_DEVICE_SPECIFIC_NOTIFY) {
		return ("Device Specific");
	} else {
		return ("Hardware Specific");
	}
}
#endif


u8 acpi_ut_valid_object_type(acpi_object_type type)
{

	if (type > ACPI_TYPE_LOCAL_MAX) {

		

		return (FALSE);
	}

	return (TRUE);
}
