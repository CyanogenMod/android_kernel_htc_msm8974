/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2004 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/sn/types.h>
#include <asm/sn/module.h>
#include <asm/sn/l1.h>

char brick_types[MAX_BRICK_TYPES + 1] = "cri.xdpn%#=vo^kjbf890123456789...";
void
format_module_id(char *buffer, moduleid_t m, int fmt)
{
	int rack, position;
	unsigned char brickchar;

	rack = MODULE_GET_RACK(m);
	brickchar = MODULE_GET_BTCHAR(m);

	switch (brickchar) 
	{
	case L1_BRICKTYPE_GA:
	case L1_BRICKTYPE_OPUS_TIO:
		brickchar = L1_BRICKTYPE_C;
		break;

	case L1_BRICKTYPE_PX:
	case L1_BRICKTYPE_PE:
	case L1_BRICKTYPE_PA:
	case L1_BRICKTYPE_SA: 
		brickchar = L1_BRICKTYPE_P;
		break;

	case L1_BRICKTYPE_IX:
	case L1_BRICKTYPE_IA:

		brickchar = L1_BRICKTYPE_I;
		break;
	}

	position = MODULE_GET_BPOS(m);

	if ((fmt == MODULE_FORMAT_BRIEF) || (fmt == MODULE_FORMAT_LCD)) {
		

		
		*buffer++ = '0' + RACK_GET_CLASS(rack);
		*buffer++ = '0' + RACK_GET_GROUP(rack);
		*buffer++ = '0' + RACK_GET_NUM(rack);

		
		*buffer++ = brickchar;
	}
	else if (fmt == MODULE_FORMAT_LONG) {
		

		strcpy(buffer, "rack" "/");  buffer += strlen(buffer);

		*buffer++ = '0' + RACK_GET_CLASS(rack);
		*buffer++ = '0' + RACK_GET_GROUP(rack);
		*buffer++ = '0' + RACK_GET_NUM(rack);

		strcpy(buffer, "/" "bay" "/");  buffer += strlen(buffer);
	}

	
	if (position < 10)
		*buffer++ = '0';
	sprintf(buffer, "%d", position);
}
