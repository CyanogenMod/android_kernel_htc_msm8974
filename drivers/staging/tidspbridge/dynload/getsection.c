/*
 * getsection.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <dspbridge/getsection.h>
#include "header.h"

static const char readstrm[] = { "Error reading %s from input stream" };
static const char seek[] = { "Set file position to %d failed" };
static const char isiz[] = { "Bad image packet size %d" };
static const char err_checksum[] = { "Checksum failed on %s" };

static const char err_reloc[] = { "dload_get_section unable to read"
	    "sections containing relocation entries"
};

#if BITS_PER_AU > BITS_PER_BYTE
static const char err_alloc[] = { "Syms->dload_allocate( %d ) failed" };
static const char stbl[] = { "Bad string table offset " FMT_UI32 };
#endif


#if BITS_PER_AU > BITS_PER_BYTE
static char *unpack_sec_name(struct dload_state *dlthis, u32 soffset, char *dst)
{
	u8 tmp, *src;

	if (soffset >= dlthis->dfile_hdr.df_scn_name_size) {
		dload_error(dlthis, stbl, soffset);
		return NULL;
	}
	src = (u8 *) dlthis->str_head +
	    (soffset >> (LOG_BITS_PER_AU - LOG_BITS_PER_BYTE));
	if (soffset & 1)
		*dst++ = *src++;	
	do {
		tmp = *src++;
		*dst = (tmp >> BITS_PER_BYTE)
		    if (!(*dst++))
			break;
	} while ((*dst++ = tmp & BYTE_MASK));

	return dst;
}

static void expand_sec_names(struct dload_state *dlthis)
{
	char *xstrings, *curr, *next;
	u32 xsize;
	u16 sec;
	struct ldr_section_info *shp;
	
	xsize = dlthis->dfile_hdr.df_max_str_len * dlthis->dfile_hdr.df_no_scns;
	xstrings = (char *)dlthis->mysym->dload_allocate(dlthis->mysym, xsize);
	if (xstrings == NULL) {
		dload_error(dlthis, err_alloc, xsize);
		return;
	}
	dlthis->xstrings = xstrings;
	
	curr = xstrings;
	for (sec = 0; sec < dlthis->dfile_hdr.df_no_scns; sec++) {
		shp = (struct ldr_section_info *)&dlthis->sect_hdrs[sec];
		next = unpack_sec_name(dlthis, *(u32 *) &shp->name, curr);
		if (next == NULL)
			break;	
		shp->name = curr;
		curr = next;
	}
}

#endif


void *dload_module_open(struct dynamic_loader_stream *module,
				    struct dynamic_loader_sym *syms)
{
	struct dload_state *dlthis;	
	unsigned *dp, sz;
	u32 sec_start;
#if BITS_PER_AU <= BITS_PER_BYTE
	u16 sec;
#endif

	
	if (!module || !syms) {
		if (syms != NULL)
			dload_syms_error(syms, "Required parameter is NULL");

		return NULL;
	}

	dlthis = (struct dload_state *)
	    syms->dload_allocate(syms, sizeof(struct dload_state));
	if (!dlthis) {
		
		dload_syms_error(syms, "Can't allocate module info");
		return NULL;
	}

	
	dp = (unsigned *)dlthis;
	for (sz = sizeof(struct dload_state) / sizeof(unsigned);
	     sz > 0; sz -= 1)
		*dp++ = 0;

	dlthis->strm = module;
	dlthis->mysym = syms;

	
	dload_headers(dlthis);

	if (!dlthis->dload_errcount)
		dload_strings(dlthis, true);

	
	sec_start = sizeof(struct doff_filehdr_t) +
	    sizeof(struct doff_verify_rec_t) +
	    BYTE_TO_HOST(DOFF_ALIGN(dlthis->dfile_hdr.df_strtab_size));

	if (dlthis->strm->set_file_posn(dlthis->strm, sec_start) != 0) {
		dload_error(dlthis, seek, sec_start);
		return NULL;
	}

	if (!dlthis->dload_errcount)
		dload_sections(dlthis);

	if (dlthis->dload_errcount) {
		dload_module_close(dlthis);	
		dlthis = NULL;
		return NULL;
	}
#if BITS_PER_AU > BITS_PER_BYTE
	
	
	
	expand_sec_names(dlthis);
#else
	
	
	for (sec = 0; sec < dlthis->dfile_hdr.df_no_scns; sec++) {
		struct ldr_section_info *shp =
		    (struct ldr_section_info *)&dlthis->sect_hdrs[sec];
		shp->name = dlthis->str_head + *(u32 *) &shp->name;
	}
#endif

	return dlthis;
}

int dload_get_section_info(void *minfo, const char *section_name,
			   const struct ldr_section_info **const section_info)
{
	struct dload_state *dlthis;
	struct ldr_section_info *shp;
	u16 sec;

	dlthis = (struct dload_state *)minfo;
	if (!dlthis)
		return false;

	for (sec = 0; sec < dlthis->dfile_hdr.df_no_scns; sec++) {
		shp = (struct ldr_section_info *)&dlthis->sect_hdrs[sec];
		if (strcmp(section_name, shp->name) == 0) {
			*section_info = shp;
			return true;
		}
	}

	return false;
}

#define IPH_SIZE (sizeof(struct image_packet_t) - sizeof(u32))

int dload_get_section(void *minfo,
		      const struct ldr_section_info *section_info,
		      void *section_data)
{
	struct dload_state *dlthis;
	u32 pos;
	struct doff_scnhdr_t *sptr = NULL;
	s32 nip;
	struct image_packet_t ipacket;
	s32 ipsize;
	u32 checks;
	s8 *dest = (s8 *) section_data;

	dlthis = (struct dload_state *)minfo;
	if (!dlthis)
		return false;
	sptr = (struct doff_scnhdr_t *)section_info;
	if (sptr == NULL)
		return false;

	
	pos = BYTE_TO_HOST(DOFF_ALIGN((u32) sptr->ds_first_pkt_offset));
	if (dlthis->strm->set_file_posn(dlthis->strm, pos) != 0) {
		dload_error(dlthis, seek, pos);
		return false;
	}

	nip = sptr->ds_nipacks;
	while ((nip -= 1) >= 0) {	
		
		if (dlthis->strm->read_buffer(dlthis->strm, &ipacket,
					      IPH_SIZE) != IPH_SIZE) {
			dload_error(dlthis, readstrm, "image packet");
			return false;
		}
		
		if (dlthis->reorder_map)
			dload_reorder(&ipacket, IPH_SIZE, dlthis->reorder_map);

		ipsize = BYTE_TO_HOST(DOFF_ALIGN(ipacket.packet_size));
		if (ipsize > BYTE_TO_HOST(IMAGE_PACKET_SIZE)) {
			dload_error(dlthis, isiz, ipsize);
			return false;
		}
		if (dlthis->strm->read_buffer
		    (dlthis->strm, dest, ipsize) != ipsize) {
			dload_error(dlthis, readstrm, "image packet");
			return false;
		}
		
#if !defined(_BIG_ENDIAN) || (TARGET_AU_BITS > 16)
		if (dlthis->reorder_map)
			dload_reorder(dest, ipsize, dlthis->reorder_map);

		checks = dload_checksum(dest, ipsize);
#else
		if (dlthis->dfile_hdr.df_byte_reshuffle !=
		    TARGET_ORDER(REORDER_MAP(BYTE_RESHUFFLE_VALUE))) {
			
			dload_reorder(dest, ipsize,
				      TARGET_ORDER(dlthis->
						dfile_hdr.df_byte_reshuffle));
		}
#if TARGET_AU_BITS > 8
		checks = dload_reverse_checksum16(dest, ipsize);
#else
		checks = dload_reverse_checksum(dest, ipsize);
#endif
#endif
		checks += dload_checksum(&ipacket, IPH_SIZE);

		if (ipacket.num_relocs != 0) {
			dload_error(dlthis, err_reloc, ipsize);
			return false;
		}

		if (~checks) {
			dload_error(dlthis, err_checksum, "image packet");
			return false;
		}

		
		dest += ipsize;
	}

	return true;
}

void dload_module_close(void *minfo)
{
	struct dload_state *dlthis;

	dlthis = (struct dload_state *)minfo;
	if (!dlthis)
		return;

	if (dlthis->str_head)
		dlthis->mysym->dload_deallocate(dlthis->mysym,
						dlthis->str_head);

	if (dlthis->sect_hdrs)
		dlthis->mysym->dload_deallocate(dlthis->mysym,
						dlthis->sect_hdrs);

#if BITS_PER_AU > BITS_PER_BYTE
	if (dlthis->xstrings)
		dlthis->mysym->dload_deallocate(dlthis->mysym,
						dlthis->xstrings);

#endif

	dlthis->mysym->dload_deallocate(dlthis->mysym, dlthis);
}
