/**

    AudioScience HPI driver
    Copyright (C) 1997-2011  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

\file
Functions for reading DSP code to load into DSP

*/
#ifndef _HPIDSPCD_H_
#define _HPIDSPCD_H_

#include "hpi_internal.h"

struct code_header {
	
	u32 size;
	
	u32 type;
	
	u32 adapter;
	
	u32 version;
	
	u32 checksum;
};

compile_time_assert((sizeof(struct code_header) == 20), code_header_size);

struct dsp_code {
	
	struct code_header header;
	
	u32 block_length;
	
	u32 word_count;

	
	struct dsp_code_private *pvt;
};

short hpi_dsp_code_open(
	
	u32 adapter, void *pci_dev,
	
	struct dsp_code *ps_dsp_code,
	
	u32 *pos_error_code);

void hpi_dsp_code_close(struct dsp_code *ps_dsp_code);

void hpi_dsp_code_rewind(struct dsp_code *ps_dsp_code);

short hpi_dsp_code_read_word(struct dsp_code *ps_dsp_code,
				      
	u32 *pword 
	);

short hpi_dsp_code_read_block(size_t words_requested,
	struct dsp_code *ps_dsp_code,
	
	u32 **ppblock);

#endif
