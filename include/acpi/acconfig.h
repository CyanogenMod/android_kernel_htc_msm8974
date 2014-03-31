
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

#ifndef _ACCONFIG_H
#define _ACCONFIG_H



#define ACPI_OS_NAME                    "Microsoft Windows NT"


#define ACPI_MAX_STATE_CACHE_DEPTH      96	
#define ACPI_MAX_PARSE_CACHE_DEPTH      96	
#define ACPI_MAX_EXTPARSE_CACHE_DEPTH   96	
#define ACPI_MAX_OBJECT_CACHE_DEPTH     96	
#define ACPI_MAX_NAMESPACE_CACHE_DEPTH  96	

#define ACPI_CHECKSUM_ABORT             FALSE

#define ACPI_REDUCED_HARDWARE           FALSE



#define ACPI_CA_SUPPORT_LEVEL           5


#define ACPI_MAX_SEMAPHORE_COUNT        256


#define ACPI_MAX_REFERENCE_COUNT        0x1000


#define ACPI_DEFAULT_PAGE_SIZE          4096	


#define ACPI_NUM_OWNERID_MASKS          8


#define ACPI_ROOT_TABLE_SIZE_INCREMENT  4


#define ACPI_MAX_LOOP_ITERATIONS        0xFFFF


#define ACPI_MAX_SLEEP                  2000	


#define ACPI_ADDRESS_RANGE_MAX          2



#define ACPI_MAX_GPE_BLOCKS             2
#define ACPI_GPE_REGISTER_WIDTH         8


#define ACPI_METHOD_NUM_LOCALS          8
#define ACPI_METHOD_MAX_LOCAL           7

#define ACPI_METHOD_NUM_ARGS            7
#define ACPI_METHOD_MAX_ARG             6


#define ACPI_DEVICE_ID_LENGTH           0x09
#define ACPI_MAX_CID_LENGTH             48
#define ACPI_UUID_LENGTH                16

#define ACPI_OBJ_NUM_OPERANDS           8
#define ACPI_OBJ_MAX_OPERAND            7


#define ACPI_RESULTS_FRAME_OBJ_NUM      8

#define ACPI_RESULTS_OBJ_NUM_MAX        255


#define ACPI_NAME_SIZE                  4
#define ACPI_PATH_SEGMENT_LENGTH        5	
#define ACPI_PATH_SEPARATOR             '.'


#define ACPI_OEM_ID_SIZE                6
#define ACPI_OEM_TABLE_ID_SIZE          8


#define ACPI_EBDA_PTR_LOCATION          0x0000040E	
#define ACPI_EBDA_PTR_LENGTH            2
#define ACPI_EBDA_WINDOW_SIZE           1024
#define ACPI_HI_RSDP_WINDOW_BASE        0x000E0000	
#define ACPI_HI_RSDP_WINDOW_SIZE        0x00020000
#define ACPI_RSDP_SCAN_STEP             16


#define ACPI_USER_REGION_BEGIN          0x80


#define ACPI_MAX_ADDRESS_SPACE          255


#define ACPI_MAX_MATCH_OPCODE           5


#define ACPI_RSDP_CHECKSUM_LENGTH       20
#define ACPI_RSDP_XCHECKSUM_LENGTH      36


#define ACPI_SMBUS_BUFFER_SIZE          34
#define ACPI_GSBUS_BUFFER_SIZE          34
#define ACPI_IPMI_BUFFER_SIZE           66


#define ACPI_NUM_sx_d_METHODS           4
#define ACPI_NUM_sx_w_METHODS           5


#define ACPI_DEBUGGER_MAX_ARGS          8	

#define ACPI_DEBUGGER_COMMAND_PROMPT    '-'
#define ACPI_DEBUGGER_EXECUTE_PROMPT    '%'

#endif				
