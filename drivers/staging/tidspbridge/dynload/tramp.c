/*
 * tramp.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "header.h"

#if TMS32060
#include "tramp_table_c6000.c"
#endif

#define MAX_RELOS_PER_PASS	4

static int priv_tramp_sect_tgt_alloc(struct dload_state *dlthis)
{
	int ret_val = 0;
	struct ldr_section_info *sect_info;

	sect_info = &dlthis->ldr_sections[dlthis->allocated_secn_count];

	sect_info->name = dlthis->tramp.final_string_table;
	sect_info->size = dlthis->tramp.tramp_sect_next_addr;
	sect_info->context = 0;
	sect_info->type =
	    (4 << 8) | DLOAD_TEXT | DS_ALLOCATE_MASK | DS_DOWNLOAD_MASK;
	sect_info->page = 0;
	sect_info->run_addr = 0;
	sect_info->load_addr = 0;
	ret_val = dlthis->myalloc->dload_allocate(dlthis->myalloc,
						  sect_info,
						  ds_alignment
						  (sect_info->type));

	if (ret_val == 0)
		dload_error(dlthis, "Failed to allocate target memory for"
			    " trampoline");

	return ret_val;
}

static u8 priv_h2a(u8 value)
{
	if (value > 0xF)
		return 0xFF;

	if (value <= 9)
		value += 0x30;
	else
		value += 0x37;

	return value;
}

static void priv_tramp_sym_gen_name(u32 value, char *dst)
{
	u32 i;
	char *prefix = TRAMP_SYM_PREFIX;
	char *dst_local = dst;
	u8 tmp;

	
	for (i = 0; i < (TRAMP_SYM_PREFIX_LEN + TRAMP_SYM_HEX_ASCII_LEN); i++)
		*(dst_local + i) = 0;

	
	for (i = 0; i < strlen(TRAMP_SYM_PREFIX); i++) {
		*dst_local = *(prefix + i);
		dst_local++;
	}

	
	for (i = 0; i < sizeof(value); i++) {
#ifndef _BIG_ENDIAN
		tmp = *(((u8 *) &value) + (sizeof(value) - 1) - i);
		*dst_local = priv_h2a((tmp & 0xF0) >> 4);
		dst_local++;
		*dst_local = priv_h2a(tmp & 0x0F);
		dst_local++;
#else
		tmp = *(((u8 *) &value) + i);
		*dst_local = priv_h2a((tmp & 0xF0) >> 4);
		dst_local++;
		*dst_local = priv_h2a(tmp & 0x0F);
		dst_local++;
#endif
	}

	
	*dst_local = 0;
}

static struct tramp_string *priv_tramp_string_create(struct dload_state *dlthis,
						     u32 str_len, char *str)
{
	struct tramp_string *new_string = NULL;
	u32 i;

	
	new_string =
	    (struct tramp_string *)dlthis->mysym->dload_allocate(dlthis->mysym,
								 (sizeof
								  (struct
								   tramp_string)
								  + str_len +
								  1));
	if (new_string != NULL) {
		for (i = 0; i < (sizeof(struct tramp_string) + str_len + 1);
		     i++)
			*((u8 *) new_string + i) = 0;

		new_string->index = dlthis->tramp.tramp_string_next_index;
		dlthis->tramp.tramp_string_next_index++;
		dlthis->tramp.tramp_string_size += str_len + 1;

		new_string->next = NULL;
		if (dlthis->tramp.string_head == NULL)
			dlthis->tramp.string_head = new_string;
		else
			dlthis->tramp.string_tail->next = new_string;

		dlthis->tramp.string_tail = new_string;

		
		for (i = 0; i < str_len; i++)
			new_string->str[i] = str[i];
	}

	return new_string;
}

static struct tramp_string *priv_tramp_string_find(struct dload_state *dlthis,
						   char *str)
{
	struct tramp_string *cur_str = NULL;
	struct tramp_string *ret_val = NULL;
	u32 i;
	u32 str_len = strlen(str);

	for (cur_str = dlthis->tramp.string_head;
	     (ret_val == NULL) && (cur_str != NULL); cur_str = cur_str->next) {
		if (str_len != strlen(cur_str->str))
			continue;

		
		for (i = 0; i < str_len; i++) {
			if (str[i] != cur_str->str[i])
				break;
		}

		if (i == str_len)
			ret_val = cur_str;
	}

	return ret_val;
}

static int priv_string_tbl_finalize(struct dload_state *dlthis)
{
	int ret_val = 0;
	struct tramp_string *cur_string;
	char *cur_loc;
	char *tmp;

	dlthis->tramp.final_string_table =
	    (char *)dlthis->mysym->dload_allocate(dlthis->mysym,
						  dlthis->tramp.
						  tramp_string_size);
	if (dlthis->tramp.final_string_table != NULL) {
		cur_loc = dlthis->tramp.final_string_table;
		cur_string = dlthis->tramp.string_head;
		while (cur_string != NULL) {
			
			dlthis->tramp.string_head = cur_string->next;
			if (dlthis->tramp.string_tail == cur_string)
				dlthis->tramp.string_tail = NULL;

			
			for (tmp = cur_string->str;
			     *tmp != '\0'; tmp++, cur_loc++)
				*cur_loc = *tmp;

			*cur_loc = '\0';
			cur_loc++;

			
			dlthis->mysym->dload_deallocate(dlthis->mysym,
							cur_string);

			
			cur_string = dlthis->tramp.string_head;
		}

		
		ret_val = 1;
	} else
		dload_error(dlthis, "Failed to allocate trampoline "
			    "string table");

	return ret_val;
}

static u32 priv_tramp_sect_alloc(struct dload_state *dlthis, u32 tramp_size)
{
	u32 ret_val;

	if (dlthis->tramp.tramp_sect_next_addr == 0) {
		dload_syms_error(dlthis->mysym, "*** WARNING ***  created "
				 "dynamic TRAMPOLINE section for module %s",
				 dlthis->str_head);
	}

	
	ret_val = dlthis->tramp.tramp_sect_next_addr;
	dlthis->tramp.tramp_sect_next_addr += tramp_size;
	return ret_val;
}

static struct tramp_sym *priv_tramp_sym_create(struct dload_state *dlthis,
					       u32 str_index,
					       struct local_symbol *tmp_sym)
{
	struct tramp_sym *new_sym = NULL;
	u32 i;

	
	new_sym =
	    (struct tramp_sym *)dlthis->mysym->dload_allocate(dlthis->mysym,
					      sizeof(struct tramp_sym));
	if (new_sym != NULL) {
		for (i = 0; i != sizeof(struct tramp_sym); i++)
			*((char *)new_sym + i) = 0;

		new_sym->index = dlthis->tramp.tramp_sym_next_index;
		dlthis->tramp.tramp_sym_next_index++;

		new_sym->sym_info = *tmp_sym;
		new_sym->str_index = str_index;

		
		new_sym->next = NULL;
		if (dlthis->tramp.symbol_head == NULL)
			dlthis->tramp.symbol_head = new_sym;
		else
			dlthis->tramp.symbol_tail->next = new_sym;

		dlthis->tramp.symbol_tail = new_sym;
	}

	return new_sym;
}

static struct tramp_sym *priv_tramp_sym_get(struct dload_state *dlthis,
					    u32 string_index)
{
	struct tramp_sym *sym_found = NULL;

	
	for (sym_found = dlthis->tramp.symbol_head;
	     sym_found != NULL; sym_found = sym_found->next) {
		if (sym_found->str_index == string_index)
			break;
	}

	return sym_found;
}

static struct tramp_sym *priv_tramp_sym_find(struct dload_state *dlthis,
					     char *string)
{
	struct tramp_sym *sym_found = NULL;
	struct tramp_string *str_found = NULL;

	str_found = priv_tramp_string_find(dlthis, string);
	if (str_found != NULL)
		sym_found = priv_tramp_sym_get(dlthis, str_found->index);

	return sym_found;
}

static int priv_tramp_sym_finalize(struct dload_state *dlthis)
{
	int ret_val = 0;
	struct tramp_sym *cur_sym;
	struct ldr_section_info *tramp_sect =
	    &dlthis->ldr_sections[dlthis->allocated_secn_count];
	struct local_symbol *new_sym;

	dlthis->tramp.final_sym_table =
	    (struct local_symbol *)dlthis->mysym->dload_allocate(dlthis->mysym,
				 (sizeof(struct local_symbol) * dlthis->tramp.
						  tramp_sym_next_index));
	if (dlthis->tramp.final_sym_table != NULL) {
		new_sym = dlthis->tramp.final_sym_table;
		cur_sym = dlthis->tramp.symbol_head;
		while (cur_sym != NULL) {
			
			dlthis->tramp.symbol_head = cur_sym->next;
			if (cur_sym == dlthis->tramp.symbol_tail)
				dlthis->tramp.symbol_tail = NULL;

			
			*new_sym = cur_sym->sym_info;

			if (new_sym->secnn < 0) {
				new_sym->value += tramp_sect->load_addr;
				new_sym->delta = new_sym->value;
			}

			
			dlthis->mysym->dload_deallocate(dlthis->mysym, cur_sym);

			
			cur_sym = dlthis->tramp.symbol_head;
			new_sym++;
		}

		ret_val = 1;
	} else
		dload_error(dlthis, "Failed to alloc trampoline sym table");

	return ret_val;
}

static int priv_tgt_img_gen(struct dload_state *dlthis, u32 base,
			    u32 gen_index, struct tramp_sym *new_ext_sym)
{
	struct tramp_img_pkt *new_img_pkt = NULL;
	u32 i;
	u32 pkt_size = tramp_img_pkt_size_get();
	u8 *gen_tbl_entry;
	u8 *pkt_data;
	struct reloc_record_t *cur_relo;
	int ret_val = 0;

	
	new_img_pkt =
	    (struct tramp_img_pkt *)dlthis->mysym->dload_allocate(dlthis->mysym,
								  pkt_size);
	if (new_img_pkt != NULL) {
		
		new_img_pkt->base = base;

		
		pkt_data = (u8 *) &new_img_pkt->hdr;
		gen_tbl_entry = (u8 *) &tramp_gen_info[gen_index];
		for (i = 0; i < pkt_size; i++) {
			*pkt_data = *gen_tbl_entry;
			pkt_data++;
			gen_tbl_entry++;
		}

		
		cur_relo =
		    (struct reloc_record_t *)((u8 *) &new_img_pkt->hdr +
					      new_img_pkt->hdr.relo_offset);
		for (i = 0; i < new_img_pkt->hdr.num_relos; i++)
			cur_relo[i].SYMNDX = new_ext_sym->index;

		
		new_img_pkt->next = dlthis->tramp.tramp_pkts;
		dlthis->tramp.tramp_pkts = new_img_pkt;

		ret_val = 1;
	}

	return ret_val;
}

static int priv_pkt_relo(struct dload_state *dlthis, tgt_au_t * data,
			 struct reloc_record_t *rp[], u32 relo_count)
{
	int ret_val = 1;
	u32 i;
	bool tmp;

	for (i = 0; i < relo_count; i++)
		dload_relocate(dlthis, data, rp[i], &tmp, true);

	return ret_val;
}

/*
 * Function:	priv_tramp_pkt_finalize
 * Description: Walk the list of all trampoline packets and finalize them.
 *	  Each trampoline image packet will be relocated now that the
 *	  trampoline section has been allocated on the target.  Once
 *	  all of the relocations are done the trampoline image data
 *	  is written into target memory and the trampoline packet
 *	  is freed: it is no longer needed after this point.
 */
static int priv_tramp_pkt_finalize(struct dload_state *dlthis)
{
	int ret_val = 1;
	struct tramp_img_pkt *cur_pkt = NULL;
	struct reloc_record_t *relos[MAX_RELOS_PER_PASS];
	u32 relos_done;
	u32 i;
	struct reloc_record_t *cur_relo;
	struct ldr_section_info *sect_info =
	    &dlthis->ldr_sections[dlthis->allocated_secn_count];

	cur_pkt = dlthis->tramp.tramp_pkts;
	while ((ret_val != 0) && (cur_pkt != NULL)) {
		
		dlthis->tramp.tramp_pkts = cur_pkt->next;

		
		dlthis->image_secn = sect_info;
		dlthis->image_offset = cur_pkt->base;
		dlthis->delta_runaddr = sect_info->run_addr;

		
		relos_done = 0;
		cur_relo = (struct reloc_record_t *)((u8 *) &cur_pkt->hdr +
						     cur_pkt->hdr.relo_offset);
		while (relos_done < cur_pkt->hdr.num_relos) {
#ifdef ENABLE_TRAMP_DEBUG
			dload_syms_error(dlthis->mysym,
					 "===> Trampoline %x branches to %x",
					 sect_info->run_addr +
					 dlthis->image_offset,
					 dlthis->
					 tramp.final_sym_table[cur_relo->
							       SYMNDX].value);
#endif

			for (i = 0;
			     ((i < MAX_RELOS_PER_PASS) &&
			      ((i + relos_done) < cur_pkt->hdr.num_relos)); i++)
				relos[i] = cur_relo + i;

			
			ret_val = priv_pkt_relo(dlthis,
						(tgt_au_t *) &cur_pkt->payload,
						relos, i);
			if (ret_val == 0) {
				dload_error(dlthis,
					    "Relocation of trampoline pkt at %x"
					    " failed", cur_pkt->base +
					    sect_info->run_addr);
				break;
			}

			relos_done += i;
			cur_relo += i;
		}

		
		if (ret_val != 0) {
			ret_val = dlthis->myio->writemem(dlthis->myio,
							 &cur_pkt->payload,
							 sect_info->load_addr +
							 cur_pkt->base,
							 sect_info,
							 BYTE_TO_HOST
							 (cur_pkt->hdr.
							  tramp_code_size));
			if (ret_val == 0) {
				dload_error(dlthis,
					    "Write to " FMT_UI32 " failed",
					    sect_info->load_addr +
					    cur_pkt->base);
			}

			
			dlthis->mysym->dload_deallocate(dlthis->mysym, cur_pkt);

			
			cur_pkt = dlthis->tramp.tramp_pkts;
		}
	}

	return ret_val;
}

static int priv_dup_pkt_finalize(struct dload_state *dlthis)
{
	int ret_val = 1;
	struct tramp_img_dup_pkt *cur_pkt;
	struct tramp_img_dup_relo *cur_relo;
	struct reloc_record_t *relos[MAX_RELOS_PER_PASS];
	struct doff_scnhdr_t *sect_hdr = NULL;
	s32 i;

	cur_pkt = dlthis->tramp.dup_pkts;
	while ((ret_val != 0) && (cur_pkt != NULL)) {
		dlthis->tramp.dup_pkts = cur_pkt->next;

		
		dlthis->image_secn = &dlthis->ldr_sections[cur_pkt->secnn];
		dlthis->image_offset = cur_pkt->offset;

		i = (s32) (dlthis->image_secn - dlthis->ldr_sections);
		sect_hdr = dlthis->sect_hdrs + i;
		dlthis->delta_runaddr = sect_hdr->ds_paddr;

		
		cur_relo = cur_pkt->relo_chain;
		while (cur_relo != NULL) {
			
			for (i = 0; (i < MAX_RELOS_PER_PASS)
			     && (cur_relo != NULL);
			     i++, cur_relo = cur_relo->next) {
				relos[i] = &cur_relo->relo;
				cur_pkt->relo_chain = cur_relo->next;
			}

			
			ret_val = priv_pkt_relo(dlthis,
						cur_pkt->img_pkt.img_data,
						relos, i);
			if (ret_val == 0) {
				dload_error(dlthis,
					    "Relocation of dup pkt at %x"
					    " failed", cur_pkt->offset +
					    dlthis->image_secn->run_addr);
				break;
			}

			
			while (i > 0) {
				dlthis->mysym->dload_deallocate(dlthis->mysym,
						GET_CONTAINER
						(relos[i - 1],
						 struct tramp_img_dup_relo,
						 relo));
				i--;
			}

		}

		if (ret_val != 0) {
			ret_val = dlthis->myio->writemem(dlthis->myio,
							 cur_pkt->img_pkt.
							 img_data,
							 dlthis->image_secn->
							 load_addr +
							 cur_pkt->offset,
							 dlthis->image_secn,
							 BYTE_TO_HOST
							 (cur_pkt->img_pkt.
							  packet_size));
			if (ret_val == 0) {
				dload_error(dlthis,
					    "Write to " FMT_UI32 " failed",
					    dlthis->image_secn->load_addr +
					    cur_pkt->offset);
			}

			dlthis->mysym->dload_deallocate(dlthis->mysym, cur_pkt);

			
			cur_pkt = dlthis->tramp.dup_pkts;
		}
	}

	return ret_val;
}

static struct tramp_img_dup_pkt *priv_dup_find(struct dload_state *dlthis,
					       s16 secnn, u32 image_offset)
{
	struct tramp_img_dup_pkt *cur_pkt = NULL;

	for (cur_pkt = dlthis->tramp.dup_pkts;
	     cur_pkt != NULL; cur_pkt = cur_pkt->next) {
		if ((cur_pkt->secnn == secnn) &&
		    (cur_pkt->offset == image_offset)) {
			
			break;
		}
	}

	return cur_pkt;
}

static int priv_img_pkt_dup(struct dload_state *dlthis,
			    s16 secnn, u32 image_offset,
			    struct image_packet_t *ipacket,
			    struct reloc_record_t *rp,
			    struct tramp_sym *new_tramp_sym)
{
	struct tramp_img_dup_pkt *dup_pkt = NULL;
	u32 new_dup_size;
	s32 i;
	int ret_val = 0;
	struct tramp_img_dup_relo *dup_relo = NULL;

	dup_pkt = priv_dup_find(dlthis, secnn, image_offset);

	if (dup_pkt == NULL) {
		new_dup_size = sizeof(struct tramp_img_dup_pkt) +
		    ipacket->packet_size;

		dup_pkt = (struct tramp_img_dup_pkt *)
		    dlthis->mysym->dload_allocate(dlthis->mysym, new_dup_size);
		if (dup_pkt != NULL) {
			
			dup_pkt->secnn = secnn;
			dup_pkt->offset = image_offset;
			dup_pkt->relo_chain = NULL;

			
			dup_pkt->img_pkt = *ipacket;
			dup_pkt->img_pkt.img_data = (u8 *) (dup_pkt + 1);
			for (i = 0; i < ipacket->packet_size; i++)
				*(dup_pkt->img_pkt.img_data + i) =
				    *(ipacket->img_data + i);

			
			dup_pkt->next = dlthis->tramp.dup_pkts;
			dlthis->tramp.dup_pkts = dup_pkt;
		} else
			dload_error(dlthis, "Failed to create dup packet!");
	} else {
		for (i = 0; i < dup_pkt->img_pkt.packet_size; i++)
			*(dup_pkt->img_pkt.img_data + i) =
			    *(ipacket->img_data + i);
	}

	if (dup_pkt != NULL) {
		dup_relo = dlthis->mysym->dload_allocate(dlthis->mysym,
					 sizeof(struct tramp_img_dup_relo));
		if (dup_relo != NULL) {
			dup_relo->relo = *rp;
			dup_relo->relo.SYMNDX = new_tramp_sym->index;

			dup_relo->next = dup_pkt->relo_chain;
			dup_pkt->relo_chain = dup_relo;

			ret_val = 1;
		} else
			dload_error(dlthis, "Unable to alloc dup relo");
	}

	return ret_val;
}

bool dload_tramp_avail(struct dload_state *dlthis, struct reloc_record_t *rp)
{
	bool ret_val = false;
	u16 map_index;
	u16 gen_index;

	
	map_index = HASH_FUNC(rp->TYPE);
	gen_index = tramp_map[map_index];
	if (gen_index != TRAMP_NO_GEN_AVAIL)
		ret_val = true;

	return ret_val;
}

int dload_tramp_generate(struct dload_state *dlthis, s16 secnn,
			 u32 image_offset, struct image_packet_t *ipacket,
			 struct reloc_record_t *rp)
{
	u16 map_index;
	u16 gen_index;
	int ret_val = 1;
	char tramp_sym_str[TRAMP_SYM_PREFIX_LEN + TRAMP_SYM_HEX_ASCII_LEN];
	struct local_symbol *ref_sym;
	struct tramp_sym *new_tramp_sym;
	struct tramp_sym *new_ext_sym;
	struct tramp_string *new_tramp_str;
	u32 new_tramp_base;
	struct local_symbol tmp_sym;
	struct local_symbol ext_tmp_sym;

	
	map_index = HASH_FUNC(rp->TYPE);
	gen_index = tramp_map[map_index];
	if (gen_index != TRAMP_NO_GEN_AVAIL) {
		if (dlthis->tramp.string_head == NULL) {
			priv_tramp_string_create(dlthis,
						 strlen(TRAMP_SECT_NAME),
						 TRAMP_SECT_NAME);
		}
#ifdef ENABLE_TRAMP_DEBUG
		dload_syms_error(dlthis->mysym,
				 "Trampoline at img loc %x, references %x",
				 dlthis->ldr_sections[secnn].run_addr +
				 image_offset + rp->vaddr,
				 dlthis->local_symtab[rp->SYMNDX].value);
#endif

		if (rp->SYMNDX == -1) {
			ext_tmp_sym.value =
			    dlthis->ldr_sections[secnn].run_addr;
			ext_tmp_sym.delta = dlthis->sect_hdrs[secnn].ds_paddr;
			ref_sym = &ext_tmp_sym;
		} else
			ref_sym = &(dlthis->local_symtab[rp->SYMNDX]);

		priv_tramp_sym_gen_name(ref_sym->value, tramp_sym_str);
		new_tramp_sym = priv_tramp_sym_find(dlthis, tramp_sym_str);
		if (new_tramp_sym == NULL) {
			new_tramp_str = priv_tramp_string_create(dlthis,
								strlen
								(tramp_sym_str),
								 tramp_sym_str);
			if (new_tramp_str == NULL) {
				dload_error(dlthis, "Failed to create new "
					    "trampoline string\n");
				ret_val = 0;
			} else {
				new_tramp_base = priv_tramp_sect_alloc(dlthis,
						       tramp_size_get());

				tmp_sym.value = new_tramp_base;
				tmp_sym.delta = 0;
				tmp_sym.secnn = -1;
				tmp_sym.sclass = 0;
				new_tramp_sym = priv_tramp_sym_create(dlthis,
							      new_tramp_str->
							      index,
							      &tmp_sym);

				new_ext_sym = priv_tramp_sym_create(dlthis, -1,
								    ref_sym);

				if ((new_tramp_sym != NULL) &&
				    (new_ext_sym != NULL)) {
					ret_val = priv_tgt_img_gen(dlthis,
								 new_tramp_base,
								 gen_index,
								 new_ext_sym);

					if (ret_val != 1) {
						dload_error(dlthis, "Failed to "
							    "create img pkt for"
							    " trampoline\n");
					}
				} else {
					dload_error(dlthis, "Failed to create "
						    "new tramp syms "
						    "(%8.8X, %8.8X)\n",
						    new_tramp_sym, new_ext_sym);
					ret_val = 0;
				}
			}
		}

		if (ret_val == 1) {
			ret_val = priv_img_pkt_dup(dlthis, secnn, image_offset,
						   ipacket, rp, new_tramp_sym);
			if (ret_val != 1) {
				dload_error(dlthis, "Failed to create dup of "
					    "original img pkt\n");
			}
		}
	}

	return ret_val;
}

/*
 * Function:	dload_tramp_pkt_update
 * Description: Update the duplicate copy of this image packet, which the
 *	  trampoline layer is already tracking.  This is call is critical
 *	  to make if trampolines were generated anywhere within the
 *	  packet and first pass relo continued on the remainder.  The
 *	  trampoline layer needs the updates image data so when 2nd
 *	  pass relo is done during finalize the image packet can be
 *	  written to the target since all relo is done.
 */
int dload_tramp_pkt_udpate(struct dload_state *dlthis, s16 secnn,
			   u32 image_offset, struct image_packet_t *ipacket)
{
	struct tramp_img_dup_pkt *dup_pkt = NULL;
	s32 i;
	int ret_val = 0;

	dup_pkt = priv_dup_find(dlthis, secnn, image_offset);
	if (dup_pkt != NULL) {
		for (i = 0; i < dup_pkt->img_pkt.packet_size; i++)
			*(dup_pkt->img_pkt.img_data + i) =
			    *(ipacket->img_data + i);

		ret_val = 1;
	} else {
		dload_error(dlthis,
			    "Unable to find existing DUP pkt for %x, offset %x",
			    secnn, image_offset);

	}

	return ret_val;
}

int dload_tramp_finalize(struct dload_state *dlthis)
{
	int ret_val = 1;

	if (dlthis->tramp.tramp_sect_next_addr != 0) {
		ret_val = priv_string_tbl_finalize(dlthis);

		if (ret_val != 0)
			ret_val = priv_tramp_sect_tgt_alloc(dlthis);

		if (ret_val != 0)
			ret_val = priv_tramp_sym_finalize(dlthis);

		if (ret_val != 0)
			ret_val = priv_tramp_pkt_finalize(dlthis);

		
		if (ret_val != 0)
			ret_val = priv_dup_pkt_finalize(dlthis);
	}

	return ret_val;
}

/*
 * Function:	dload_tramp_cleanup
 * Description: Release all temporary resources used in the trampoline layer.
 *	  Note that the target memory which may have been allocated and
 *	  written to store the trampolines is NOT RELEASED HERE since it
 *	  is potentially still in use.  It is automatically released
 *	  when the module is unloaded.
 */
void dload_tramp_cleanup(struct dload_state *dlthis)
{
	struct tramp_info *tramp = &dlthis->tramp;
	struct tramp_sym *cur_sym;
	struct tramp_string *cur_string;
	struct tramp_img_pkt *cur_tramp_pkt;
	struct tramp_img_dup_pkt *cur_dup_pkt;
	struct tramp_img_dup_relo *cur_dup_relo;

	
	if (tramp->tramp_sect_next_addr == 0)
		return;

	
	for (cur_sym = tramp->symbol_head;
	     cur_sym != NULL; cur_sym = tramp->symbol_head) {
		tramp->symbol_head = cur_sym->next;
		if (tramp->symbol_tail == cur_sym)
			tramp->symbol_tail = NULL;

		dlthis->mysym->dload_deallocate(dlthis->mysym, cur_sym);
	}

	if (tramp->final_sym_table != NULL)
		dlthis->mysym->dload_deallocate(dlthis->mysym,
						tramp->final_sym_table);

	for (cur_string = tramp->string_head;
	     cur_string != NULL; cur_string = tramp->string_head) {
		tramp->string_head = cur_string->next;
		if (tramp->string_tail == cur_string)
			tramp->string_tail = NULL;

		dlthis->mysym->dload_deallocate(dlthis->mysym, cur_string);
	}

	if (tramp->final_string_table != NULL)
		dlthis->mysym->dload_deallocate(dlthis->mysym,
						tramp->final_string_table);

	for (cur_tramp_pkt = tramp->tramp_pkts;
	     cur_tramp_pkt != NULL; cur_tramp_pkt = tramp->tramp_pkts) {
		tramp->tramp_pkts = cur_tramp_pkt->next;
		dlthis->mysym->dload_deallocate(dlthis->mysym, cur_tramp_pkt);
	}

	for (cur_dup_pkt = tramp->dup_pkts;
	     cur_dup_pkt != NULL; cur_dup_pkt = tramp->dup_pkts) {
		tramp->dup_pkts = cur_dup_pkt->next;

		for (cur_dup_relo = cur_dup_pkt->relo_chain;
		     cur_dup_relo != NULL;
		     cur_dup_relo = cur_dup_pkt->relo_chain) {
			cur_dup_pkt->relo_chain = cur_dup_relo->next;
			dlthis->mysym->dload_deallocate(dlthis->mysym,
							cur_dup_relo);
		}

		dlthis->mysym->dload_deallocate(dlthis->mysym, cur_dup_pkt);
	}
}
