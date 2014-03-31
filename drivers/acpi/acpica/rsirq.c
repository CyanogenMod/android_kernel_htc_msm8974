
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

#define _COMPONENT          ACPI_RESOURCES
ACPI_MODULE_NAME("rsirq")

struct acpi_rsconvert_info acpi_rs_get_irq[8] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_IRQ,
	 ACPI_RS_SIZE(struct acpi_resource_irq),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_get_irq)},

	

	{ACPI_RSC_BITMASK16, ACPI_RS_OFFSET(data.irq.interrupts[0]),
	 AML_OFFSET(irq.irq_mask),
	 ACPI_RS_OFFSET(data.irq.interrupt_count)},

	

	{ACPI_RSC_SET8, ACPI_RS_OFFSET(data.irq.triggering),
	 ACPI_EDGE_SENSITIVE,
	 1},

	

	{ACPI_RSC_2BITFLAG, ACPI_RS_OFFSET(data.irq.descriptor_length),
	 AML_OFFSET(irq.descriptor_type),
	 0},

	

	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_AML_LENGTH, 0, 3},

	

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.irq.triggering),
	 AML_OFFSET(irq.flags),
	 0},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.irq.polarity),
	 AML_OFFSET(irq.flags),
	 3},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.irq.sharable),
	 AML_OFFSET(irq.flags),
	 4}
};


struct acpi_rsconvert_info acpi_rs_set_irq[13] = {
	

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_IRQ,
	 sizeof(struct aml_resource_irq),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_set_irq)},

	

	{ACPI_RSC_BITMASK16, ACPI_RS_OFFSET(data.irq.interrupts[0]),
	 AML_OFFSET(irq.irq_mask),
	 ACPI_RS_OFFSET(data.irq.interrupt_count)},

	

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.irq.triggering),
	 AML_OFFSET(irq.flags),
	 0},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.irq.polarity),
	 AML_OFFSET(irq.flags),
	 3},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.irq.sharable),
	 AML_OFFSET(irq.flags),
	 4},

	{ACPI_RSC_EXIT_EQ, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.irq.descriptor_length),
	 3},

	

	{ACPI_RSC_LENGTH, 0, 0, sizeof(struct aml_resource_irq_noflags)},

	{ACPI_RSC_EXIT_EQ, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.irq.descriptor_length),
	 2},

	

	{ACPI_RSC_LENGTH, 0, 0, sizeof(struct aml_resource_irq)},

	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.irq.triggering),
	 ACPI_EDGE_SENSITIVE},

	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.irq.polarity),
	 ACPI_ACTIVE_HIGH},

	{ACPI_RSC_EXIT_NE, ACPI_RSC_COMPARE_VALUE,
	 ACPI_RS_OFFSET(data.irq.sharable),
	 ACPI_EXCLUSIVE},

	

	{ACPI_RSC_LENGTH, 0, 0, sizeof(struct aml_resource_irq_noflags)}
};


struct acpi_rsconvert_info acpi_rs_convert_ext_irq[9] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_EXTENDED_IRQ,
	 ACPI_RS_SIZE(struct acpi_resource_extended_irq),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_ext_irq)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_EXTENDED_IRQ,
	 sizeof(struct aml_resource_extended_irq),
	 0},

	

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.extended_irq.producer_consumer),
	 AML_OFFSET(extended_irq.flags),
	 0},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.extended_irq.triggering),
	 AML_OFFSET(extended_irq.flags),
	 1},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.extended_irq.polarity),
	 AML_OFFSET(extended_irq.flags),
	 2},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.extended_irq.sharable),
	 AML_OFFSET(extended_irq.flags),
	 3},

	

	{ACPI_RSC_COUNT, ACPI_RS_OFFSET(data.extended_irq.interrupt_count),
	 AML_OFFSET(extended_irq.interrupt_count),
	 sizeof(u32)}
	,

	

	{ACPI_RSC_MOVE32, ACPI_RS_OFFSET(data.extended_irq.interrupts[0]),
	 AML_OFFSET(extended_irq.interrupts[0]),
	 0}
	,

	

	{ACPI_RSC_SOURCEX, ACPI_RS_OFFSET(data.extended_irq.resource_source),
	 ACPI_RS_OFFSET(data.extended_irq.interrupts[0]),
	 sizeof(struct aml_resource_extended_irq)}
};


struct acpi_rsconvert_info acpi_rs_convert_dma[6] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_DMA,
	 ACPI_RS_SIZE(struct acpi_resource_dma),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_dma)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_DMA,
	 sizeof(struct aml_resource_dma),
	 0},

	

	{ACPI_RSC_2BITFLAG, ACPI_RS_OFFSET(data.dma.transfer),
	 AML_OFFSET(dma.flags),
	 0},

	{ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET(data.dma.bus_master),
	 AML_OFFSET(dma.flags),
	 2},

	{ACPI_RSC_2BITFLAG, ACPI_RS_OFFSET(data.dma.type),
	 AML_OFFSET(dma.flags),
	 5},

	

	{ACPI_RSC_BITMASK, ACPI_RS_OFFSET(data.dma.channels[0]),
	 AML_OFFSET(dma.dma_channel_mask),
	 ACPI_RS_OFFSET(data.dma.channel_count)}
};


struct acpi_rsconvert_info acpi_rs_convert_fixed_dma[4] = {
	{ACPI_RSC_INITGET, ACPI_RESOURCE_TYPE_FIXED_DMA,
	 ACPI_RS_SIZE(struct acpi_resource_fixed_dma),
	 ACPI_RSC_TABLE_SIZE(acpi_rs_convert_fixed_dma)},

	{ACPI_RSC_INITSET, ACPI_RESOURCE_NAME_FIXED_DMA,
	 sizeof(struct aml_resource_fixed_dma),
	 0},


	{ACPI_RSC_MOVE16, ACPI_RS_OFFSET(data.fixed_dma.request_lines),
	 AML_OFFSET(fixed_dma.request_lines),
	 2},

	{ACPI_RSC_MOVE8, ACPI_RS_OFFSET(data.fixed_dma.width),
	 AML_OFFSET(fixed_dma.width),
	 1},

};
