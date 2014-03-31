/*
 * dload_internal.h
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

#ifndef _DLOAD_INTERNAL_
#define _DLOAD_INTERNAL_

#include <linux/types.h>


typedef s32 rvalue;

typedef u32 urvalue;

#define REASONABLE_SECTION_LIMIT 100

#define DLOAD_FILL_BSS 0

#define REORDER_MAP(rawmap) ((rawmap) ^ 0x3030303)
#if defined(_BIG_ENDIAN)
#define HOST_BYTE_ORDER(cookedmap) ((cookedmap) ^ 0x3030303)
#else
#define HOST_BYTE_ORDER(cookedmap) (cookedmap)
#endif
#if !defined(_BIG_ENDIAN) || TARGET_AU_BITS > 16
#define TARGET_ORDER(cookedmap) (cookedmap)
#elif TARGET_AU_BITS > 8
#define TARGET_ORDER(cookedmap) ((cookedmap) ^ 0x2020202)
#else
#define TARGET_ORDER(cookedmap) ((cookedmap) ^ 0x3030303)
#endif

struct my_handle;

struct dbg_mirror_root {
	
	u32 dbthis;
	struct my_handle *next;	
	u16 changes;		
	u16 refcount;		
};

struct dbg_mirror_list {
	u32 dbthis;
	struct my_handle *next, *prev;
	struct dbg_mirror_root *root;
	u16 dbsiz;
	u32 context;	
};

#define VARIABLE_SIZE 1
struct my_handle {
	struct dbg_mirror_list dm;	
	
	u16 secn_count;
	struct ldr_section_info secns[VARIABLE_SIZE];
};
#define MY_HANDLE_SIZE (sizeof(struct my_handle) -\
			sizeof(struct ldr_section_info))

struct local_symbol {
	s32 value;		
	s32 delta;		
	s16 secnn;		
	s16 sclass;		
};

#define TRAMP_NO_GEN_AVAIL              65535
#define TRAMP_SYM_PREFIX                "__$dbTR__"
#define TRAMP_SECT_NAME                 ".dbTR"
#define TRAMP_SYM_PREFIX_LEN            9
#define TRAMP_SYM_HEX_ASCII_LEN         9

#define GET_CONTAINER(ptr, type, field) ((type *)((unsigned long)ptr -\
				(unsigned long)(&((type *)0)->field)))
#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)       ((unsigned long)(&((type *)0)->field))
#endif

struct tramp_gen_code_hdr {
	u32 tramp_code_size;	
	u32 num_relos;
	u32 relo_offset;	
};

struct tramp_img_pkt {
	struct tramp_img_pkt *next;	
	u32 base;
	struct tramp_gen_code_hdr hdr;
	u8 payload[VARIABLE_SIZE];
};

struct tramp_img_dup_relo {
	struct tramp_img_dup_relo *next;
	struct reloc_record_t relo;
};

struct tramp_img_dup_pkt {
	struct tramp_img_dup_pkt *next;	
	s16 secnn;
	u32 offset;
	struct image_packet_t img_pkt;
	struct tramp_img_dup_relo *relo_chain;

	
};

struct tramp_sym {
	struct tramp_sym *next;	
	u32 index;
	u32 str_index;
	struct local_symbol sym_info;
};

struct tramp_string {
	struct tramp_string *next;	
	u32 index;
	char str[VARIABLE_SIZE];	
};

struct tramp_info {
	u32 tramp_sect_next_addr;
	struct ldr_section_info sect_info;

	struct tramp_sym *symbol_head;
	struct tramp_sym *symbol_tail;
	u32 tramp_sym_next_index;
	struct local_symbol *final_sym_table;

	struct tramp_string *string_head;
	struct tramp_string *string_tail;
	u32 tramp_string_next_index;
	u32 tramp_string_size;
	char *final_string_table;

	struct tramp_img_pkt *tramp_pkts;
	struct tramp_img_dup_pkt *dup_pkts;
};

enum cinit_mode {
	CI_COUNT = 0,		
	CI_ADDRESS,		
#if CINIT_ALIGN < CINIT_ADDRESS	
	CI_PARTADDRESS,		
#endif
	CI_COPY,		
	CI_DONE			
};

struct dload_state {
	struct dynamic_loader_stream *strm;	
	struct dynamic_loader_sym *mysym;	
	
	struct dynamic_loader_allocate *myalloc;
	struct dynamic_loader_initialize *myio;	
	unsigned myoptions;	

	char *str_head;		
#if BITS_PER_AU > BITS_PER_BYTE
	char *str_temp;		
	
	unsigned temp_len;	
	char *xstrings;		
	
#endif
	
	unsigned debug_string_size;
	
	struct doff_scnhdr_t *sect_hdrs;	
	struct ldr_section_info *ldr_sections;
#if TMS32060
	
	ldr_addr bss_run_base;
#endif
	struct local_symbol *local_symtab;	

	
	struct ldr_section_info *image_secn;
	
	ldr_addr delta_runaddr;
	ldr_addr image_offset;	
	enum cinit_mode cinit_state;	
	int cinit_count;	
	ldr_addr cinit_addr;	
	s16 cinit_page;		
	
	struct my_handle *myhandle;
	unsigned dload_errcount;	
	
	unsigned allocated_secn_count;
#ifndef TARGET_ENDIANNESS
	int big_e_target;	
#endif
	
	u32 reorder_map;
	struct doff_filehdr_t dfile_hdr;	
	struct doff_verify_rec_t verify;	

	struct tramp_info tramp;	

	int relstkidx;		
	
	rvalue relstk[STATIC_EXPR_STK_SIZE];

};

#ifdef TARGET_ENDIANNESS
#define TARGET_BIG_ENDIAN TARGET_ENDIANNESS
#else
#define TARGET_BIG_ENDIAN (dlthis->big_e_target)
#endif

extern void dload_error(struct dload_state *dlthis, const char *errtxt, ...);
extern void dload_syms_error(struct dynamic_loader_sym *syms,
			     const char *errtxt, ...);
extern void dload_headers(struct dload_state *dlthis);
extern void dload_strings(struct dload_state *dlthis, bool sec_names_only);
extern void dload_sections(struct dload_state *dlthis);
extern void dload_reorder(void *data, int dsiz, u32 map);
extern u32 dload_checksum(void *data, unsigned siz);

#if HOST_ENDIANNESS
extern uint32_t dload_reverse_checksum(void *data, unsigned siz);
#if (TARGET_AU_BITS > 8) && (TARGET_AU_BITS < 32)
extern uint32_t dload_reverse_checksum16(void *data, unsigned siz);
#endif
#endif

extern void dload_relocate(struct dload_state *dlthis, tgt_au_t * data,
			   struct reloc_record_t *rp, bool * tramps_generated,
			   bool second_pass);

extern rvalue dload_unpack(struct dload_state *dlthis, tgt_au_t * data,
			   int fieldsz, int offset, unsigned sgn);

extern int dload_repack(struct dload_state *dlthis, rvalue val, tgt_au_t * data,
			int fieldsz, int offset, unsigned sgn);

extern bool dload_tramp_avail(struct dload_state *dlthis,
			      struct reloc_record_t *rp);

int dload_tramp_generate(struct dload_state *dlthis, s16 secnn,
			 u32 image_offset, struct image_packet_t *ipacket,
			 struct reloc_record_t *rp);

extern int dload_tramp_pkt_udpate(struct dload_state *dlthis,
				  s16 secnn, u32 image_offset,
				  struct image_packet_t *ipacket);

extern int dload_tramp_finalize(struct dload_state *dlthis);

extern void dload_tramp_cleanup(struct dload_state *dlthis);

#endif 
