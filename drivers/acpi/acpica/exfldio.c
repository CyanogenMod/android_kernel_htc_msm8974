
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
#include "acinterp.h"
#include "amlcode.h"
#include "acevents.h"
#include "acdispat.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exfldio")

static acpi_status
acpi_ex_field_datum_io(union acpi_operand_object *obj_desc,
		       u32 field_datum_byte_offset,
		       u64 *value, u32 read_write);

static u8
acpi_ex_register_overflow(union acpi_operand_object *obj_desc, u64 value);

static acpi_status
acpi_ex_setup_region(union acpi_operand_object *obj_desc,
		     u32 field_datum_byte_offset);

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_setup_region
 *
 * PARAMETERS:  obj_desc                - Field to be read or written
 *              field_datum_byte_offset - Byte offset of this datum within the
 *                                        parent field
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Common processing for acpi_ex_extract_from_field and
 *              acpi_ex_insert_into_field. Initialize the Region if necessary and
 *              validate the request.
 *
 ******************************************************************************/

static acpi_status
acpi_ex_setup_region(union acpi_operand_object *obj_desc,
		     u32 field_datum_byte_offset)
{
	acpi_status status = AE_OK;
	union acpi_operand_object *rgn_desc;
	u8 space_id;

	ACPI_FUNCTION_TRACE_U32(ex_setup_region, field_datum_byte_offset);

	rgn_desc = obj_desc->common_field.region_obj;

	

	if (rgn_desc->common.type != ACPI_TYPE_REGION) {
		ACPI_ERROR((AE_INFO, "Needed Region, found type 0x%X (%s)",
			    rgn_desc->common.type,
			    acpi_ut_get_object_type_name(rgn_desc)));

		return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
	}

	space_id = rgn_desc->region.space_id;

	

	if (!acpi_is_valid_space_id(space_id)) {
		ACPI_ERROR((AE_INFO,
			    "Invalid/unknown Address Space ID: 0x%2.2X",
			    space_id));
		return_ACPI_STATUS(AE_AML_INVALID_SPACE_ID);
	}

	if (!(rgn_desc->common.flags & AOPOBJ_DATA_VALID)) {
		status = acpi_ds_get_region_arguments(rgn_desc);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	

	if (rgn_desc->common.flags & AOPOBJ_INVALID) {
		return_ACPI_STATUS(AE_AML_ILLEGAL_ADDRESS);
	}

	if (space_id == ACPI_ADR_SPACE_SMBUS ||
	    space_id == ACPI_ADR_SPACE_GSBUS ||
	    space_id == ACPI_ADR_SPACE_IPMI) {

		

		return_ACPI_STATUS(AE_OK);
	}
#ifdef ACPI_UNDER_DEVELOPMENT
	if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}
#endif

	if (rgn_desc->region.length <
	    (obj_desc->common_field.base_byte_offset + field_datum_byte_offset +
	     obj_desc->common_field.access_byte_width)) {
		if (acpi_gbl_enable_interpreter_slack) {
			if (ACPI_ROUND_UP(rgn_desc->region.length,
					  obj_desc->common_field.
					  access_byte_width) >=
			    ((acpi_size) obj_desc->common_field.
			     base_byte_offset +
			     obj_desc->common_field.access_byte_width +
			     field_datum_byte_offset)) {
				return_ACPI_STATUS(AE_OK);
			}
		}

		if (rgn_desc->region.length <
		    obj_desc->common_field.access_byte_width) {
			ACPI_ERROR((AE_INFO,
				    "Field [%4.4s] access width (%u bytes) too large for region [%4.4s] (length %u)",
				    acpi_ut_get_node_name(obj_desc->
							  common_field.node),
				    obj_desc->common_field.access_byte_width,
				    acpi_ut_get_node_name(rgn_desc->region.
							  node),
				    rgn_desc->region.length));
		}

		ACPI_ERROR((AE_INFO,
			    "Field [%4.4s] Base+Offset+Width %u+%u+%u is beyond end of region [%4.4s] (length %u)",
			    acpi_ut_get_node_name(obj_desc->common_field.node),
			    obj_desc->common_field.base_byte_offset,
			    field_datum_byte_offset,
			    obj_desc->common_field.access_byte_width,
			    acpi_ut_get_node_name(rgn_desc->region.node),
			    rgn_desc->region.length));

		return_ACPI_STATUS(AE_AML_REGION_LIMIT);
	}

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_access_region(union acpi_operand_object *obj_desc,
		      u32 field_datum_byte_offset, u64 *value, u32 function)
{
	acpi_status status;
	union acpi_operand_object *rgn_desc;
	u32 region_offset;

	ACPI_FUNCTION_TRACE(ex_access_region);

	status = acpi_ex_setup_region(obj_desc, field_datum_byte_offset);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	rgn_desc = obj_desc->common_field.region_obj;
	region_offset =
	    obj_desc->common_field.base_byte_offset + field_datum_byte_offset;

	if ((function & ACPI_IO_MASK) == ACPI_READ) {
		ACPI_DEBUG_PRINT((ACPI_DB_BFIELD, "[READ]"));
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_BFIELD, "[WRITE]"));
	}

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_BFIELD,
			      " Region [%s:%X], Width %X, ByteBase %X, Offset %X at %p\n",
			      acpi_ut_get_region_name(rgn_desc->region.
						      space_id),
			      rgn_desc->region.space_id,
			      obj_desc->common_field.access_byte_width,
			      obj_desc->common_field.base_byte_offset,
			      field_datum_byte_offset, ACPI_CAST_PTR(void,
								     (rgn_desc->
								      region.
								      address +
								      region_offset))));

	

	status = acpi_ev_address_space_dispatch(rgn_desc, obj_desc,
						function, region_offset,
						ACPI_MUL_8(obj_desc->
							   common_field.
							   access_byte_width),
						value);

	if (ACPI_FAILURE(status)) {
		if (status == AE_NOT_IMPLEMENTED) {
			ACPI_ERROR((AE_INFO,
				    "Region %s (ID=%u) not implemented",
				    acpi_ut_get_region_name(rgn_desc->region.
							    space_id),
				    rgn_desc->region.space_id));
		} else if (status == AE_NOT_EXIST) {
			ACPI_ERROR((AE_INFO,
				    "Region %s (ID=%u) has no handler",
				    acpi_ut_get_region_name(rgn_desc->region.
							    space_id),
				    rgn_desc->region.space_id));
		}
	}

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_register_overflow
 *
 * PARAMETERS:  obj_desc                - Register(Field) to be written
 *              Value                   - Value to be stored
 *
 * RETURN:      TRUE if value overflows the field, FALSE otherwise
 *
 * DESCRIPTION: Check if a value is out of range of the field being written.
 *              Used to check if the values written to Index and Bank registers
 *              are out of range.  Normally, the value is simply truncated
 *              to fit the field, but this case is most likely a serious
 *              coding error in the ASL.
 *
 ******************************************************************************/

static u8
acpi_ex_register_overflow(union acpi_operand_object *obj_desc, u64 value)
{
	ACPI_FUNCTION_NAME(ex_register_overflow);

	if (obj_desc->common_field.bit_length >= ACPI_INTEGER_BIT_SIZE) {
		return (FALSE);
	}

	if (value >= ((u64) 1 << obj_desc->common_field.bit_length)) {
		ACPI_ERROR((AE_INFO,
			    "Index value 0x%8.8X%8.8X overflows field width 0x%X",
			    ACPI_FORMAT_UINT64(value),
			    obj_desc->common_field.bit_length));

		return (TRUE);
	}

	

	return (FALSE);
}


static acpi_status
acpi_ex_field_datum_io(union acpi_operand_object *obj_desc,
		       u32 field_datum_byte_offset, u64 *value, u32 read_write)
{
	acpi_status status;
	u64 local_value;

	ACPI_FUNCTION_TRACE_U32(ex_field_datum_io, field_datum_byte_offset);

	if (read_write == ACPI_READ) {
		if (!value) {
			local_value = 0;

			
			value = &local_value;
		}

		

		*value = 0;
	}

	switch (obj_desc->common.type) {
	case ACPI_TYPE_BUFFER_FIELD:
		if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
			status = acpi_ds_get_buffer_field_arguments(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}

		if (read_write == ACPI_READ) {
			ACPI_MEMCPY(value,
				    (obj_desc->buffer_field.buffer_obj)->buffer.
				    pointer +
				    obj_desc->buffer_field.base_byte_offset +
				    field_datum_byte_offset,
				    obj_desc->common_field.access_byte_width);
		} else {
			ACPI_MEMCPY((obj_desc->buffer_field.buffer_obj)->buffer.
				    pointer +
				    obj_desc->buffer_field.base_byte_offset +
				    field_datum_byte_offset, value,
				    obj_desc->common_field.access_byte_width);
		}

		status = AE_OK;
		break;

	case ACPI_TYPE_LOCAL_BANK_FIELD:

		if (acpi_ex_register_overflow(obj_desc->bank_field.bank_obj,
					      (u64) obj_desc->bank_field.
					      value)) {
			return_ACPI_STATUS(AE_AML_REGISTER_LIMIT);
		}

		status =
		    acpi_ex_insert_into_field(obj_desc->bank_field.bank_obj,
					      &obj_desc->bank_field.value,
					      sizeof(obj_desc->bank_field.
						     value));
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}


		

	case ACPI_TYPE_LOCAL_REGION_FIELD:
		status =
		    acpi_ex_access_region(obj_desc, field_datum_byte_offset,
					  value, read_write);
		break;

	case ACPI_TYPE_LOCAL_INDEX_FIELD:

		if (acpi_ex_register_overflow(obj_desc->index_field.index_obj,
					      (u64) obj_desc->index_field.
					      value)) {
			return_ACPI_STATUS(AE_AML_REGISTER_LIMIT);
		}

		

		field_datum_byte_offset += obj_desc->index_field.value;

		ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
				  "Write to Index Register: Value %8.8X\n",
				  field_datum_byte_offset));

		status =
		    acpi_ex_insert_into_field(obj_desc->index_field.index_obj,
					      &field_datum_byte_offset,
					      sizeof(field_datum_byte_offset));
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		if (read_write == ACPI_READ) {

			

			ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
					  "Read from Data Register\n"));

			status =
			    acpi_ex_extract_from_field(obj_desc->index_field.
						       data_obj, value,
						       sizeof(u64));
		} else {
			

			ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
					  "Write to Data Register: Value %8.8X%8.8X\n",
					  ACPI_FORMAT_UINT64(*value)));

			status =
			    acpi_ex_insert_into_field(obj_desc->index_field.
						      data_obj, value,
						      sizeof(u64));
		}
		break;

	default:

		ACPI_ERROR((AE_INFO, "Wrong object type in field I/O %u",
			    obj_desc->common.type));
		status = AE_AML_INTERNAL;
		break;
	}

	if (ACPI_SUCCESS(status)) {
		if (read_write == ACPI_READ) {
			ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
					  "Value Read %8.8X%8.8X, Width %u\n",
					  ACPI_FORMAT_UINT64(*value),
					  obj_desc->common_field.
					  access_byte_width));
		} else {
			ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
					  "Value Written %8.8X%8.8X, Width %u\n",
					  ACPI_FORMAT_UINT64(*value),
					  obj_desc->common_field.
					  access_byte_width));
		}
	}

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_write_with_update_rule
 *
 * PARAMETERS:  obj_desc                - Field to be written
 *              Mask                    - bitmask within field datum
 *              field_value             - Value to write
 *              field_datum_byte_offset - Offset of datum within field
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Apply the field update rule to a field write
 *
 ******************************************************************************/

acpi_status
acpi_ex_write_with_update_rule(union acpi_operand_object *obj_desc,
			       u64 mask,
			       u64 field_value, u32 field_datum_byte_offset)
{
	acpi_status status = AE_OK;
	u64 merged_value;
	u64 current_value;

	ACPI_FUNCTION_TRACE_U32(ex_write_with_update_rule, mask);

	

	merged_value = field_value;

	

	if (mask != ACPI_UINT64_MAX) {

		

		switch (obj_desc->common_field.
			field_flags & AML_FIELD_UPDATE_RULE_MASK) {
		case AML_FIELD_UPDATE_PRESERVE:
			if ((~mask << (ACPI_MUL_8(sizeof(mask)) -
				       ACPI_MUL_8(obj_desc->common_field.
						  access_byte_width))) != 0) {
				status =
				    acpi_ex_field_datum_io(obj_desc,
							   field_datum_byte_offset,
							   &current_value,
							   ACPI_READ);
				if (ACPI_FAILURE(status)) {
					return_ACPI_STATUS(status);
				}

				merged_value |= (current_value & ~mask);
			}
			break;

		case AML_FIELD_UPDATE_WRITE_AS_ONES:

			

			merged_value |= ~mask;
			break;

		case AML_FIELD_UPDATE_WRITE_AS_ZEROS:

			

			merged_value &= mask;
			break;

		default:

			ACPI_ERROR((AE_INFO,
				    "Unknown UpdateRule value: 0x%X",
				    (obj_desc->common_field.
				     field_flags &
				     AML_FIELD_UPDATE_RULE_MASK)));
			return_ACPI_STATUS(AE_AML_OPERAND_VALUE);
		}
	}

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "Mask %8.8X%8.8X, DatumOffset %X, Width %X, Value %8.8X%8.8X, MergedValue %8.8X%8.8X\n",
			  ACPI_FORMAT_UINT64(mask),
			  field_datum_byte_offset,
			  obj_desc->common_field.access_byte_width,
			  ACPI_FORMAT_UINT64(field_value),
			  ACPI_FORMAT_UINT64(merged_value)));

	

	status = acpi_ex_field_datum_io(obj_desc, field_datum_byte_offset,
					&merged_value, ACPI_WRITE);

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_extract_from_field(union acpi_operand_object *obj_desc,
			   void *buffer, u32 buffer_length)
{
	acpi_status status;
	u64 raw_datum;
	u64 merged_datum;
	u32 field_offset = 0;
	u32 buffer_offset = 0;
	u32 buffer_tail_bits;
	u32 datum_count;
	u32 field_datum_count;
	u32 access_bit_width;
	u32 i;

	ACPI_FUNCTION_TRACE(ex_extract_from_field);

	

	if (buffer_length <
	    ACPI_ROUND_BITS_UP_TO_BYTES(obj_desc->common_field.bit_length)) {
		ACPI_ERROR((AE_INFO,
			    "Field size %u (bits) is too large for buffer (%u)",
			    obj_desc->common_field.bit_length, buffer_length));

		return_ACPI_STATUS(AE_BUFFER_OVERFLOW);
	}

	ACPI_MEMSET(buffer, 0, buffer_length);
	access_bit_width = ACPI_MUL_8(obj_desc->common_field.access_byte_width);

	

	if ((obj_desc->common_field.start_field_bit_offset == 0) &&
	    (obj_desc->common_field.bit_length == access_bit_width)) {
		status = acpi_ex_field_datum_io(obj_desc, 0, buffer, ACPI_READ);
		return_ACPI_STATUS(status);
	}


	

	if (obj_desc->common_field.access_byte_width > sizeof(u64)) {
		obj_desc->common_field.access_byte_width = sizeof(u64);
		access_bit_width = sizeof(u64) * 8;
	}

	

	datum_count =
	    ACPI_ROUND_UP_TO(obj_desc->common_field.bit_length,
			     access_bit_width);

	field_datum_count = ACPI_ROUND_UP_TO(obj_desc->common_field.bit_length +
					     obj_desc->common_field.
					     start_field_bit_offset,
					     access_bit_width);

	

	status =
	    acpi_ex_field_datum_io(obj_desc, field_offset, &raw_datum,
				   ACPI_READ);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}
	merged_datum =
	    raw_datum >> obj_desc->common_field.start_field_bit_offset;

	

	for (i = 1; i < field_datum_count; i++) {

		

		field_offset += obj_desc->common_field.access_byte_width;
		status = acpi_ex_field_datum_io(obj_desc, field_offset,
						&raw_datum, ACPI_READ);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		if (access_bit_width -
		    obj_desc->common_field.start_field_bit_offset <
		    ACPI_INTEGER_BIT_SIZE) {
			merged_datum |=
			    raw_datum << (access_bit_width -
					  obj_desc->common_field.
					  start_field_bit_offset);
		}

		if (i == datum_count) {
			break;
		}

		

		ACPI_MEMCPY(((char *)buffer) + buffer_offset, &merged_datum,
			    ACPI_MIN(obj_desc->common_field.access_byte_width,
				     buffer_length - buffer_offset));

		buffer_offset += obj_desc->common_field.access_byte_width;
		merged_datum =
		    raw_datum >> obj_desc->common_field.start_field_bit_offset;
	}

	

	buffer_tail_bits = obj_desc->common_field.bit_length % access_bit_width;
	if (buffer_tail_bits) {
		merged_datum &= ACPI_MASK_BITS_ABOVE(buffer_tail_bits);
	}

	

	ACPI_MEMCPY(((char *)buffer) + buffer_offset, &merged_datum,
		    ACPI_MIN(obj_desc->common_field.access_byte_width,
			     buffer_length - buffer_offset));

	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_insert_into_field
 *
 * PARAMETERS:  obj_desc            - Field to be written
 *              Buffer              - Data to be written
 *              buffer_length       - Length of Buffer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the Buffer contents into the given field
 *
 ******************************************************************************/

acpi_status
acpi_ex_insert_into_field(union acpi_operand_object *obj_desc,
			  void *buffer, u32 buffer_length)
{
	void *new_buffer;
	acpi_status status;
	u64 mask;
	u64 width_mask;
	u64 merged_datum;
	u64 raw_datum = 0;
	u32 field_offset = 0;
	u32 buffer_offset = 0;
	u32 buffer_tail_bits;
	u32 datum_count;
	u32 field_datum_count;
	u32 access_bit_width;
	u32 required_length;
	u32 i;

	ACPI_FUNCTION_TRACE(ex_insert_into_field);

	

	new_buffer = NULL;
	required_length =
	    ACPI_ROUND_BITS_UP_TO_BYTES(obj_desc->common_field.bit_length);
	if (buffer_length < required_length) {

		

		new_buffer = ACPI_ALLOCATE_ZEROED(required_length);
		if (!new_buffer) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		ACPI_MEMCPY((char *)new_buffer, (char *)buffer, buffer_length);
		buffer = new_buffer;
		buffer_length = required_length;
	}


	
	if (obj_desc->common_field.access_byte_width > sizeof(u64)) {
		obj_desc->common_field.access_byte_width = sizeof(u64);
	}

	access_bit_width = ACPI_MUL_8(obj_desc->common_field.access_byte_width);

	if (access_bit_width == ACPI_INTEGER_BIT_SIZE) {
		width_mask = ACPI_UINT64_MAX;
	} else {
		width_mask = ACPI_MASK_BITS_ABOVE(access_bit_width);
	}

	mask = width_mask &
	    ACPI_MASK_BITS_BELOW(obj_desc->common_field.start_field_bit_offset);

	

	datum_count = ACPI_ROUND_UP_TO(obj_desc->common_field.bit_length,
				       access_bit_width);

	field_datum_count = ACPI_ROUND_UP_TO(obj_desc->common_field.bit_length +
					     obj_desc->common_field.
					     start_field_bit_offset,
					     access_bit_width);

	

	ACPI_MEMCPY(&raw_datum, buffer,
		    ACPI_MIN(obj_desc->common_field.access_byte_width,
			     buffer_length - buffer_offset));

	merged_datum =
	    raw_datum << obj_desc->common_field.start_field_bit_offset;

	

	for (i = 1; i < field_datum_count; i++) {

		

		merged_datum &= mask;
		status = acpi_ex_write_with_update_rule(obj_desc, mask,
							merged_datum,
							field_offset);
		if (ACPI_FAILURE(status)) {
			goto exit;
		}

		field_offset += obj_desc->common_field.access_byte_width;

		if ((access_bit_width -
		     obj_desc->common_field.start_field_bit_offset) <
		    ACPI_INTEGER_BIT_SIZE) {
			merged_datum =
			    raw_datum >> (access_bit_width -
					  obj_desc->common_field.
					  start_field_bit_offset);
		} else {
			merged_datum = 0;
		}

		mask = width_mask;

		if (i == datum_count) {
			break;
		}

		

		buffer_offset += obj_desc->common_field.access_byte_width;
		ACPI_MEMCPY(&raw_datum, ((char *)buffer) + buffer_offset,
			    ACPI_MIN(obj_desc->common_field.access_byte_width,
				     buffer_length - buffer_offset));

		merged_datum |=
		    raw_datum << obj_desc->common_field.start_field_bit_offset;
	}

	

	buffer_tail_bits = (obj_desc->common_field.bit_length +
			    obj_desc->common_field.start_field_bit_offset) %
	    access_bit_width;
	if (buffer_tail_bits) {
		mask &= ACPI_MASK_BITS_ABOVE(buffer_tail_bits);
	}

	

	merged_datum &= mask;
	status = acpi_ex_write_with_update_rule(obj_desc,
						mask, merged_datum,
						field_offset);

      exit:
	

	if (new_buffer) {
		ACPI_FREE(new_buffer);
	}
	return_ACPI_STATUS(status);
}
