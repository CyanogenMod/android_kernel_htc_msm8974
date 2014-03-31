
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

#include <acpi/acpi.h>
#include "accommon.h"
#include "acresrc.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utresrc")
#if defined(ACPI_DISASSEMBLER) || defined (ACPI_DEBUGGER)
const char *acpi_gbl_bm_decode[] = {
	"NotBusMaster",
	"BusMaster"
};

const char *acpi_gbl_config_decode[] = {
	"0 - Good Configuration",
	"1 - Acceptable Configuration",
	"2 - Suboptimal Configuration",
	"3 - ***Invalid Configuration***",
};

const char *acpi_gbl_consume_decode[] = {
	"ResourceProducer",
	"ResourceConsumer"
};

const char *acpi_gbl_dec_decode[] = {
	"PosDecode",
	"SubDecode"
};

const char *acpi_gbl_he_decode[] = {
	"Level",
	"Edge"
};

const char *acpi_gbl_io_decode[] = {
	"Decode10",
	"Decode16"
};

const char *acpi_gbl_ll_decode[] = {
	"ActiveHigh",
	"ActiveLow"
};

const char *acpi_gbl_max_decode[] = {
	"MaxNotFixed",
	"MaxFixed"
};

const char *acpi_gbl_mem_decode[] = {
	"NonCacheable",
	"Cacheable",
	"WriteCombining",
	"Prefetchable"
};

const char *acpi_gbl_min_decode[] = {
	"MinNotFixed",
	"MinFixed"
};

const char *acpi_gbl_mtp_decode[] = {
	"AddressRangeMemory",
	"AddressRangeReserved",
	"AddressRangeACPI",
	"AddressRangeNVS"
};

const char *acpi_gbl_rng_decode[] = {
	"InvalidRanges",
	"NonISAOnlyRanges",
	"ISAOnlyRanges",
	"EntireRange"
};

const char *acpi_gbl_rw_decode[] = {
	"ReadOnly",
	"ReadWrite"
};

const char *acpi_gbl_shr_decode[] = {
	"Exclusive",
	"Shared"
};

const char *acpi_gbl_siz_decode[] = {
	"Transfer8",
	"Transfer8_16",
	"Transfer16",
	"InvalidSize"
};

const char *acpi_gbl_trs_decode[] = {
	"DenseTranslation",
	"SparseTranslation"
};

const char *acpi_gbl_ttp_decode[] = {
	"TypeStatic",
	"TypeTranslation"
};

const char *acpi_gbl_typ_decode[] = {
	"Compatibility",
	"TypeA",
	"TypeB",
	"TypeF"
};

const char *acpi_gbl_ppc_decode[] = {
	"PullDefault",
	"PullUp",
	"PullDown",
	"PullNone"
};

const char *acpi_gbl_ior_decode[] = {
	"IoRestrictionNone",
	"IoRestrictionInputOnly",
	"IoRestrictionOutputOnly",
	"IoRestrictionNoneAndPreserve"
};

const char *acpi_gbl_dts_decode[] = {
	"Width8bit",
	"Width16bit",
	"Width32bit",
	"Width64bit",
	"Width128bit",
	"Width256bit",
};


const char *acpi_gbl_ct_decode[] = {
	"Interrupt",
	"I/O"
};


const char *acpi_gbl_sbt_decode[] = {
	"/* UNKNOWN serial bus type */",
	"I2C",
	"SPI",
	"UART"
};


const char *acpi_gbl_am_decode[] = {
	"AddressingMode7Bit",
	"AddressingMode10Bit"
};


const char *acpi_gbl_sm_decode[] = {
	"ControllerInitiated",
	"DeviceInitiated"
};


const char *acpi_gbl_wm_decode[] = {
	"FourWireMode",
	"ThreeWireMode"
};


const char *acpi_gbl_cph_decode[] = {
	"ClockPhaseFirst",
	"ClockPhaseSecond"
};


const char *acpi_gbl_cpo_decode[] = {
	"ClockPolarityLow",
	"ClockPolarityHigh"
};


const char *acpi_gbl_dp_decode[] = {
	"PolarityLow",
	"PolarityHigh"
};


const char *acpi_gbl_ed_decode[] = {
	"LittleEndian",
	"BigEndian"
};


const char *acpi_gbl_bpb_decode[] = {
	"DataBitsFive",
	"DataBitsSix",
	"DataBitsSeven",
	"DataBitsEight",
	"DataBitsNine",
	"/* UNKNOWN Bits per byte */",
	"/* UNKNOWN Bits per byte */",
	"/* UNKNOWN Bits per byte */"
};


const char *acpi_gbl_sb_decode[] = {
	"StopBitsNone",
	"StopBitsOne",
	"StopBitsOnePlusHalf",
	"StopBitsTwo"
};


const char *acpi_gbl_fc_decode[] = {
	"FlowControlNone",
	"FlowControlHardware",
	"FlowControlXON",
	"/* UNKNOWN flow control keyword */"
};


const char *acpi_gbl_pt_decode[] = {
	"ParityTypeNone",
	"ParityTypeEven",
	"ParityTypeOdd",
	"ParityTypeMark",
	"ParityTypeSpace",
	"/* UNKNOWN parity keyword */",
	"/* UNKNOWN parity keyword */",
	"/* UNKNOWN parity keyword */"
};

#endif

const u8 acpi_gbl_resource_aml_sizes[] = {
	

	0,
	0,
	0,
	0,
	ACPI_AML_SIZE_SMALL(struct aml_resource_irq),
	ACPI_AML_SIZE_SMALL(struct aml_resource_dma),
	ACPI_AML_SIZE_SMALL(struct aml_resource_start_dependent),
	ACPI_AML_SIZE_SMALL(struct aml_resource_end_dependent),
	ACPI_AML_SIZE_SMALL(struct aml_resource_io),
	ACPI_AML_SIZE_SMALL(struct aml_resource_fixed_io),
	ACPI_AML_SIZE_SMALL(struct aml_resource_fixed_dma),
	0,
	0,
	0,
	ACPI_AML_SIZE_SMALL(struct aml_resource_vendor_small),
	ACPI_AML_SIZE_SMALL(struct aml_resource_end_tag),

	

	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_memory24),
	ACPI_AML_SIZE_LARGE(struct aml_resource_generic_register),
	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_vendor_large),
	ACPI_AML_SIZE_LARGE(struct aml_resource_memory32),
	ACPI_AML_SIZE_LARGE(struct aml_resource_fixed_memory32),
	ACPI_AML_SIZE_LARGE(struct aml_resource_address32),
	ACPI_AML_SIZE_LARGE(struct aml_resource_address16),
	ACPI_AML_SIZE_LARGE(struct aml_resource_extended_irq),
	ACPI_AML_SIZE_LARGE(struct aml_resource_address64),
	ACPI_AML_SIZE_LARGE(struct aml_resource_extended_address64),
	ACPI_AML_SIZE_LARGE(struct aml_resource_gpio),
	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_common_serialbus),
};

const u8 acpi_gbl_resource_aml_serial_bus_sizes[] = {
	0,
	ACPI_AML_SIZE_LARGE(struct aml_resource_i2c_serialbus),
	ACPI_AML_SIZE_LARGE(struct aml_resource_spi_serialbus),
	ACPI_AML_SIZE_LARGE(struct aml_resource_uart_serialbus),
};

static const u8 acpi_gbl_resource_types[] = {
	

	0,
	0,
	0,
	0,
	ACPI_SMALL_VARIABLE_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_SMALL_VARIABLE_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_FIXED_LENGTH,	
	0,
	0,
	0,
	ACPI_VARIABLE_LENGTH,	
	ACPI_FIXED_LENGTH,	

	

	0,
	ACPI_FIXED_LENGTH,	
	ACPI_FIXED_LENGTH,	
	0,
	ACPI_VARIABLE_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_VARIABLE_LENGTH,	
	ACPI_VARIABLE_LENGTH,	
	ACPI_VARIABLE_LENGTH,	
	ACPI_VARIABLE_LENGTH,	
	ACPI_FIXED_LENGTH,	
	ACPI_VARIABLE_LENGTH,	
	0,
	ACPI_VARIABLE_LENGTH	
};

#ifdef ACPI_ASL_COMPILER
#define ACPI_RESOURCE_ERROR(plist)
#else
#define ACPI_RESOURCE_ERROR(plist)  ACPI_ERROR(plist)
#endif


acpi_status
acpi_ut_walk_aml_resources(u8 * aml,
			   acpi_size aml_length,
			   acpi_walk_aml_callback user_function, void **context)
{
	acpi_status status;
	u8 *end_aml;
	u8 resource_index;
	u32 length;
	u32 offset = 0;
	u8 end_tag[2] = { 0x79, 0x00 };

	ACPI_FUNCTION_TRACE(ut_walk_aml_resources);

	

	if (aml_length < sizeof(struct aml_resource_end_tag)) {
		return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
	}

	

	end_aml = aml + aml_length;

	

	while (aml < end_aml) {

		

		status = acpi_ut_validate_resource(aml, &resource_index);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		

		length = acpi_ut_get_descriptor_length(aml);

		

		if (user_function) {
			status =
			    user_function(aml, length, offset, resource_index,
					  context);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}

		

		if (acpi_ut_get_resource_type(aml) ==
		    ACPI_RESOURCE_NAME_END_TAG) {
			if ((aml + 1) >= end_aml) {
				return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
			}

			

			if (!user_function) {
				*context = aml;
			}

			

			return_ACPI_STATUS(AE_OK);
		}

		aml += length;
		offset += length;
	}

	

	if (user_function) {

		

		(void)acpi_ut_validate_resource(end_tag, &resource_index);
		status =
		    user_function(end_tag, 2, offset, resource_index, context);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	return_ACPI_STATUS(AE_AML_NO_RESOURCE_END_TAG);
}


acpi_status acpi_ut_validate_resource(void *aml, u8 * return_index)
{
	union aml_resource *aml_resource;
	u8 resource_type;
	u8 resource_index;
	acpi_rs_length resource_length;
	acpi_rs_length minimum_resource_length;

	ACPI_FUNCTION_ENTRY();

	resource_type = ACPI_GET8(aml);

	if (resource_type & ACPI_RESOURCE_NAME_LARGE) {

		

		if (resource_type > ACPI_RESOURCE_NAME_LARGE_MAX) {
			goto invalid_resource;
		}

		resource_index = (u8) (resource_type - 0x70);
	} else {
		resource_index = (u8)
		    ((resource_type & ACPI_RESOURCE_NAME_SMALL_MASK) >> 3);
	}

	if (!acpi_gbl_resource_types[resource_index]) {
		goto invalid_resource;
	}

	resource_length = acpi_ut_get_resource_length(aml);
	minimum_resource_length = acpi_gbl_resource_aml_sizes[resource_index];

	

	switch (acpi_gbl_resource_types[resource_index]) {
	case ACPI_FIXED_LENGTH:

		

		if (resource_length != minimum_resource_length) {
			goto bad_resource_length;
		}
		break;

	case ACPI_VARIABLE_LENGTH:

		

		if (resource_length < minimum_resource_length) {
			goto bad_resource_length;
		}
		break;

	case ACPI_SMALL_VARIABLE_LENGTH:

		

		if ((resource_length > minimum_resource_length) ||
		    (resource_length < (minimum_resource_length - 1))) {
			goto bad_resource_length;
		}
		break;

	default:

		

		goto invalid_resource;
	}

	aml_resource = ACPI_CAST_PTR(union aml_resource, aml);
	if (resource_type == ACPI_RESOURCE_NAME_SERIAL_BUS) {

		

		if ((aml_resource->common_serial_bus.type == 0) ||
		    (aml_resource->common_serial_bus.type >
		     AML_RESOURCE_MAX_SERIALBUSTYPE)) {
			ACPI_RESOURCE_ERROR((AE_INFO,
					     "Invalid/unsupported SerialBus resource descriptor: BusType 0x%2.2X",
					     aml_resource->common_serial_bus.
					     type));
			return (AE_AML_INVALID_RESOURCE_TYPE);
		}
	}

	

	if (return_index) {
		*return_index = resource_index;
	}

	return (AE_OK);

      invalid_resource:

	ACPI_RESOURCE_ERROR((AE_INFO,
			     "Invalid/unsupported resource descriptor: Type 0x%2.2X",
			     resource_type));
	return (AE_AML_INVALID_RESOURCE_TYPE);

      bad_resource_length:

	ACPI_RESOURCE_ERROR((AE_INFO,
			     "Invalid resource descriptor length: Type "
			     "0x%2.2X, Length 0x%4.4X, MinLength 0x%4.4X",
			     resource_type, resource_length,
			     minimum_resource_length));
	return (AE_AML_BAD_RESOURCE_LENGTH);
}


u8 acpi_ut_get_resource_type(void *aml)
{
	ACPI_FUNCTION_ENTRY();

	if (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_LARGE) {

		

		return (ACPI_GET8(aml));
	} else {
		

		return ((u8) (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_SMALL_MASK));
	}
}


u16 acpi_ut_get_resource_length(void *aml)
{
	acpi_rs_length resource_length;

	ACPI_FUNCTION_ENTRY();

	if (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_LARGE) {

		

		ACPI_MOVE_16_TO_16(&resource_length, ACPI_ADD_PTR(u8, aml, 1));

	} else {
		

		resource_length = (u16) (ACPI_GET8(aml) &
					 ACPI_RESOURCE_NAME_SMALL_LENGTH_MASK);
	}

	return (resource_length);
}


u8 acpi_ut_get_resource_header_length(void *aml)
{
	ACPI_FUNCTION_ENTRY();

	

	if (ACPI_GET8(aml) & ACPI_RESOURCE_NAME_LARGE) {
		return (sizeof(struct aml_resource_large_header));
	} else {
		return (sizeof(struct aml_resource_small_header));
	}
}


u32 acpi_ut_get_descriptor_length(void *aml)
{
	ACPI_FUNCTION_ENTRY();

	return (acpi_ut_get_resource_length(aml) +
		acpi_ut_get_resource_header_length(aml));
}


acpi_status
acpi_ut_get_resource_end_tag(union acpi_operand_object * obj_desc,
			     u8 ** end_tag)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_get_resource_end_tag);

	

	if (!obj_desc->buffer.length) {
		*end_tag = obj_desc->buffer.pointer;
		return_ACPI_STATUS(AE_OK);
	}

	

	status = acpi_ut_walk_aml_resources(obj_desc->buffer.pointer,
					    obj_desc->buffer.length, NULL,
					    (void **)end_tag);

	return_ACPI_STATUS(status);
}
