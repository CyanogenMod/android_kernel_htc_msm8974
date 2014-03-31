/* ----------------------------------------------------------------------- *
 *
 *  Copyright (C) 2009 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version
 *  2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 *
 *  H. Peter Anvin <hpa@linux.intel.com>
 *
 * ----------------------------------------------------------------------- */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <tools/le_byteshift.h>

int main(int argc, char *argv[])
{
	uint32_t olen;
	long ilen;
	unsigned long offs;
	FILE *f;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s compressed_file\n", argv[0]);
		return 1;
	}

	

	f = fopen(argv[1], "r");
	if (!f) {
		perror(argv[1]);
		return 1;
	}


	if (fseek(f, -4L, SEEK_END)) {
		perror(argv[1]);
	}

	if (fread(&olen, sizeof(olen), 1, f) != 1) {
		perror(argv[1]);
		return 1;
	}

	ilen = ftell(f);
	olen = get_unaligned_le32(&olen);
	fclose(f);


	offs = (olen > ilen) ? olen - ilen : 0;
	offs += olen >> 12;	
	offs += 64*1024 + 128;	
	offs = (offs+4095) & ~4095; 

	printf(".section \".rodata..compressed\",\"a\",@progbits\n");
	printf(".globl z_input_len\n");
	printf("z_input_len = %lu\n", ilen);
	printf(".globl z_output_len\n");
	printf("z_output_len = %lu\n", (unsigned long)olen);
	printf(".globl z_extract_offset\n");
	printf("z_extract_offset = 0x%lx\n", offs);
	
	printf(".globl z_extract_offset_negative\n");
	printf("z_extract_offset_negative = -0x%lx\n", offs);

	printf(".globl input_data, input_data_end\n");
	printf("input_data:\n");
	printf(".incbin \"%s\"\n", argv[1]);
	printf("input_data_end:\n");

	return 0;
}
