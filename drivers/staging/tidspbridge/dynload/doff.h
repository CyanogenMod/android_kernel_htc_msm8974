/*
 * doff.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Structures & definitions used for dynamically loaded modules file format.
 * This format is a reformatted version of COFF. It optimizes the layout for
 * the dynamic loader.
 *
 * .dof files, when viewed as a sequence of 32-bit integers, look the same
 * on big-endian and little-endian machines.
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

#ifndef _DOFF_H
#define _DOFF_H


#define BYTE_RESHUFFLE_VALUE 0x00010203

struct doff_filehdr_t {

	
	u32 df_strtab_size;

	
	u32 df_entrypt;

	u32 df_byte_reshuffle;

	
	
	u32 df_scn_name_size;

#ifndef _BIG_ENDIAN
	
	u16 df_no_syms;

	
	
	u16 df_max_str_len;

	
	u16 df_no_scns;

	
	u16 df_target_scns;

	
	u16 df_doff_version;

	
	u16 df_target_id;

	
	u16 df_flags;

	
	
	s16 df_entry_secn;
#else
	
	u16 df_max_str_len;

	
	u16 df_no_syms;

	
	u16 df_target_scns;

	
	u16 df_no_scns;

	
	u16 df_target_id;

	
	u16 df_doff_version;

	
	
	s16 df_entry_secn;

	
	u16 df_flags;
#endif
	
	u32 df_checksum;

};

#define  DF_LITTLE   0x100
#define  DF_BIG      0x200
#define  DF_BYTE_ORDER (DF_LITTLE | DF_BIG)

#define TMS470_ID   0x97
#define LEAD_ID     0x98
#define TMS32060_ID 0x99
#define LEAD3_ID    0x9c

#if TMS32060
#define TARGET_ID   TMS32060_ID
#endif

struct doff_verify_rec_t {

	
	u32 dv_timdat;

	
	u32 dv_scn_rec_checksum;

	
	u32 dv_str_tab_checksum;

	
	u32 dv_sym_tab_checksum;

	
	u32 dv_verify_rec_checksum;

};



struct doff_scnhdr_t {

	s32 ds_offset;		
	s32 ds_paddr;		
	s32 ds_vaddr;		
	s32 ds_size;		
#ifndef _BIG_ENDIAN
	u16 ds_page;		
	u16 ds_flags;		
#else
	u16 ds_flags;		
	u16 ds_page;		
#endif
	u32 ds_first_pkt_offset;
	
	

	s32 ds_nipacks;		

};

struct doff_syment_t {

	s32 dn_offset;		
	s32 dn_value;		
#ifndef _BIG_ENDIAN
	s16 dn_scnum;		
	s16 dn_sclass;		
#else
	s16 dn_sclass;		
	s16 dn_scnum;		
#endif

};

#define  DN_UNDEF  0		
#define  DN_ABS    (-1)		
#define DN_EXT     2
#define DN_STATLAB 20
#define DN_EXTLAB  21

#define IMAGE_PACKET_SIZE 1024

struct image_packet_t {

	s32 num_relocs;		
	

	s32 packet_size;	
	
	
	
	
	
	
	
	
	

	s32 img_chksum;		
	
	

	u8 *img_data;		

};

struct reloc_record_t {

	s32 vaddr;

	

	union {
		struct {
#ifndef _BIG_ENDIAN
			u8 _offset;	
			u8 _fieldsz;	
			u8 _wordsz;	
			u8 _dum1;
			u16 _dum2;
			u16 _type;
#else
			unsigned _dum1:8;
			unsigned _wordsz:8;	
			unsigned _fieldsz:8;	
			unsigned _offset:8;	
			u16 _type;
			u16 _dum2;
#endif
		} _r_field;

		struct {
			u32 _spc;	
#ifndef _BIG_ENDIAN
			u16 _dum;
			u16 _type;	
#else
			u16 _type;	
			u16 _dum;
#endif
		} _r_spc;

		struct {
			u32 _uval;	
#ifndef _BIG_ENDIAN
			u16 _dum;
			u16 _type;	
#else
			u16 _type;	
			u16 _dum;
#endif
		} _r_uval;

		struct {
			s32 _symndx;	
#ifndef _BIG_ENDIAN
			u16 _disp;	
			u16 _type;	
#else
			u16 _type;	
			u16 _disp;	
#endif
		} _r_sym;
	} _u_reloc;

};

#ifndef TYPE
#define TYPE      _u_reloc._r_sym._type
#define UVAL      _u_reloc._r_uval._uval
#define SYMNDX    _u_reloc._r_sym._symndx
#define OFFSET    _u_reloc._r_field._offset
#define FIELDSZ   _u_reloc._r_field._fieldsz
#define WORDSZ    _u_reloc._r_field._wordsz
#define R_DISP      _u_reloc._r_sym._disp
#endif


#define         DOFF0                       0

#define         DOFF_ALIGN(addr)            (((addr) + 3) & ~3UL)


#define DS_SECTION_TYPE_MASK	0xF
#define DS_ALLOCATE_MASK            0x10
#define DS_DOWNLOAD_MASK            0x20
#define DS_ALIGNMENT_SHIFT	8

static inline bool dload_check_type(struct doff_scnhdr_t *sptr, u32 flag)
{
	return (sptr->ds_flags & DS_SECTION_TYPE_MASK) == flag;
}
static inline bool ds_needs_allocation(struct doff_scnhdr_t *sptr)
{
	return sptr->ds_flags & DS_ALLOCATE_MASK;
}

static inline bool ds_needs_download(struct doff_scnhdr_t *sptr)
{
	return sptr->ds_flags & DS_DOWNLOAD_MASK;
}

static inline int ds_alignment(u16 ds_flags)
{
	return 1 << ((ds_flags >> DS_ALIGNMENT_SHIFT) & DS_SECTION_TYPE_MASK);
}


#endif 
