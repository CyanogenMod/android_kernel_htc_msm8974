/*
 * reloc.c
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

#include "header.h"

#if TMS32060
static const char bsssymbol[] = { ".bss" };
#endif

#if TMS32060
#include "reloc_table_c6000.c"
#endif

#if TMS32060
#define R_C60ALIGN     0x76	
#define R_C60FPHEAD    0x77	
#define R_C60NOCMP    0x100	
#endif

rvalue dload_unpack(struct dload_state *dlthis, tgt_au_t * data, int fieldsz,
		    int offset, unsigned sgn)
{
	register rvalue objval;
	register int shift, direction;
	register tgt_au_t *dp = data;

	fieldsz -= 1;	
	
	if (TARGET_BIG_ENDIAN) {
		dp += (fieldsz + offset) >> LOG_TGTAU_BITS;
		direction = -1;
	} else
		direction = 1;
	objval = *dp >> offset;
	shift = TGTAU_BITS - offset;
	while (shift <= fieldsz) {
		dp += direction;
		objval += (rvalue) *dp << shift;
		shift += TGTAU_BITS;
	}

	
	if (sgn == ROP_UNS)
		objval &= (2 << fieldsz) - 1;
	else {
		shift = sizeof(rvalue) * BITS_PER_AU - 1 - fieldsz;
		objval = (objval << shift) >> shift;
	}

	return objval;

}				

static const unsigned char ovf_limit[] = { 1, 2, 2 };

int dload_repack(struct dload_state *dlthis, rvalue val, tgt_au_t * data,
		 int fieldsz, int offset, unsigned sgn)
{
	register urvalue objval, mask;
	register int shift, direction;
	register tgt_au_t *dp = data;

	fieldsz -= 1;	
	
	mask = (2UL << fieldsz) - 1;
	objval = (val & mask);
	
	if (TARGET_BIG_ENDIAN) {
		dp += (fieldsz + offset) >> LOG_TGTAU_BITS;
		direction = -1;
	} else
		direction = 1;

	
	*dp = (*dp & ~(mask << offset)) + (objval << offset);
	shift = TGTAU_BITS - offset;
	
	objval >>= shift;
	mask >>= shift;

	while (mask) {
		dp += direction;
		*dp = (*dp & ~mask) + objval;
		objval >>= TGTAU_BITS;
		mask >>= TGTAU_BITS;
	}

	if (sgn) {
		unsigned tmp = (val >> fieldsz) + (sgn & 0x1);
		if (tmp > ovf_limit[sgn - 1])
			return 1;
	}
	return 0;

}				

#if TMS32060
#define SCALE_BITS 4		
#define SCALE_MASK 0x7		
static const u8 c60_scale[SCALE_MASK + 1] = {
	1, 0, 0, 0, 1, 1, 2, 2
};
#endif

void dload_relocate(struct dload_state *dlthis, tgt_au_t * data,
		    struct reloc_record_t *rp, bool *tramps_generated,
		    bool second_pass)
{
	rvalue val, reloc_amt, orig_val = 0;
	unsigned int fieldsz = 0;
	unsigned int offset = 0;
	unsigned int reloc_info = 0;
	unsigned int reloc_action = 0;
	register int rx = 0;
	rvalue *stackp = NULL;
	int top;
	struct local_symbol *svp = NULL;
#ifdef RFV_SCALE
	unsigned int scale = 0;
#endif
	struct image_packet_t *img_pkt = NULL;

	if (second_pass == false)
		img_pkt = (struct image_packet_t *)((u8 *) data -
						    sizeof(struct
							   image_packet_t));

	rx = HASH_FUNC(rp->TYPE);
	while (rop_map1[rx] != rp->TYPE) {
		rx = HASH_L(rop_map2[rx]);
		if (rx < 0) {
#if TMS32060
			switch (rp->TYPE) {
			case R_C60ALIGN:
			case R_C60NOCMP:
			case R_C60FPHEAD:
				
				break;
			default:
				
				dload_error(dlthis, "Bad coff operator 0x%x",
					    rp->TYPE);
			}
#else
			dload_error(dlthis, "Bad coff operator 0x%x", rp->TYPE);
#endif
			return;
		}
	}
	rx = HASH_I(rop_map2[rx]);
	if ((rx < (sizeof(rop_action) / sizeof(u16)))
	    && (rx < (sizeof(rop_info) / sizeof(u16))) && (rx > 0)) {
		reloc_action = rop_action[rx];
		reloc_info = rop_info[rx];
	} else {
		dload_error(dlthis, "Buffer Overflow - Array Index Out "
			    "of Bounds");
	}

	
	reloc_amt = rp->UVAL;
	if (RFV_SYM(reloc_info)) {	
		if (second_pass == false) {
			if ((u32) rp->SYMNDX < dlthis->dfile_hdr.df_no_syms) {
				
				svp = &dlthis->local_symtab[rp->SYMNDX];
				reloc_amt = (RFV_SYM(reloc_info) == ROP_SYMD) ?
				    svp->delta : svp->value;
			}
			
			else if (rp->SYMNDX == -1) {
				reloc_amt = (RFV_SYM(reloc_info) == ROP_SYMD) ?
				    dlthis->delta_runaddr :
				    dlthis->image_secn->run_addr;
			}
		}
	}
	
	
	val = 0;
	top = RFV_STK(reloc_info);
	if (top) {
		top += dlthis->relstkidx - RSTK_UOP;
		if (top >= STATIC_EXPR_STK_SIZE) {
			dload_error(dlthis,
				    "Expression stack overflow in %s at offset "
				    FMT_UI32, dlthis->image_secn->name,
				    rp->vaddr + dlthis->image_offset);
			return;
		}
		val = dlthis->relstk[dlthis->relstkidx];
		dlthis->relstkidx = top;
		stackp = &dlthis->relstk[top];
	}
	
	if (reloc_info & ROP_RW) {	
		fieldsz = RFV_WIDTH(reloc_action);
		if (fieldsz) {	
			offset = RFV_POSN(reloc_action);
			if (TARGET_BIG_ENDIAN)
				rp->vaddr += RFV_BIGOFF(reloc_info);
		} else {	
			fieldsz = rp->FIELDSZ;
			offset = rp->OFFSET;
			if (TARGET_BIG_ENDIAN)
				rp->vaddr += (rp->WORDSZ - offset - fieldsz)
				    >> LOG_TARGET_AU_BITS;
		}
		data = (tgt_au_t *) ((char *)data + TADDR_TO_HOST(rp->vaddr));
		
#if BITS_PER_AU > TARGET_AU_BITS
		if (TARGET_BIG_ENDIAN) {
			offset += -((rp->vaddr << LOG_TARGET_AU_BITS) +
				    offset + fieldsz) &
			    (BITS_PER_AU - TARGET_AU_BITS);
		} else {
			offset += (rp->vaddr << LOG_TARGET_AU_BITS) &
			    (BITS_PER_AU - 1);
		}
#endif
#ifdef RFV_SCALE
		scale = RFV_SCALE(reloc_info);
#endif
	}
	
	if (reloc_info & ROP_R) {
		
		val = dload_unpack(dlthis, data, fieldsz, offset,
				   RFV_SIGN(reloc_info));
		orig_val = val;

#ifdef RFV_SCALE
		val <<= scale;
#endif
	}
	
	switch (RFV_ACTION(reloc_action)) {	
	case RACT_VAL:
		break;
	case RACT_ASGN:
		val = reloc_amt;
		break;
	case RACT_ADD:
		val += reloc_amt;
		break;
	case RACT_PCR:
		if (rp->SYMNDX == -1)
			reloc_amt = 0;
		val += reloc_amt - dlthis->delta_runaddr;
		break;
	case RACT_ADDISP:
		val += rp->R_DISP + reloc_amt;
		break;
	case RACT_ASGPC:
		val = dlthis->image_secn->run_addr + reloc_amt;
		break;
	case RACT_PLUS:
		if (stackp != NULL)
			val += *stackp;
		break;
	case RACT_SUB:
		if (stackp != NULL)
			val = *stackp - val;
		break;
	case RACT_NEG:
		val = -val;
		break;
	case RACT_MPY:
		if (stackp != NULL)
			val *= *stackp;
		break;
	case RACT_DIV:
		if (stackp != NULL)
			val = *stackp / val;
		break;
	case RACT_MOD:
		if (stackp != NULL)
			val = *stackp % val;
		break;
	case RACT_SR:
		if (val >= sizeof(rvalue) * BITS_PER_AU)
			val = 0;
		else if (stackp != NULL)
			val = (urvalue) *stackp >> val;
		break;
	case RACT_ASR:
		if (val >= sizeof(rvalue) * BITS_PER_AU)
			val = sizeof(rvalue) * BITS_PER_AU - 1;
		else if (stackp != NULL)
			val = *stackp >> val;
		break;
	case RACT_SL:
		if (val >= sizeof(rvalue) * BITS_PER_AU)
			val = 0;
		else if (stackp != NULL)
			val = *stackp << val;
		break;
	case RACT_AND:
		if (stackp != NULL)
			val &= *stackp;
		break;
	case RACT_OR:
		if (stackp != NULL)
			val |= *stackp;
		break;
	case RACT_XOR:
		if (stackp != NULL)
			val ^= *stackp;
		break;
	case RACT_NOT:
		val = ~val;
		break;
#if TMS32060
	case RACT_C6SECT:
		
		if (svp != NULL) {
			if (rp->SYMNDX >= 0)
				if (svp->secnn > 0)
					reloc_amt = dlthis->ldr_sections
					    [svp->secnn - 1].run_addr;
		}
		
	case RACT_C6BASE:
		if (dlthis->bss_run_base == 0) {
			struct dynload_symbol *symp;
			symp = dlthis->mysym->find_matching_symbol
			    (dlthis->mysym, bsssymbol);
			
			if (symp)
				dlthis->bss_run_base = symp->value;
			else
				dload_error(dlthis,
					    "Global BSS base referenced in %s "
					    "offset" FMT_UI32 " but not "
					    "defined",
					    dlthis->image_secn->name,
					    rp->vaddr + dlthis->image_offset);
		}
		reloc_amt -= dlthis->bss_run_base;
		
	case RACT_C6DSPL:
		
		scale = c60_scale[val & SCALE_MASK];
		offset += SCALE_BITS;
		fieldsz -= SCALE_BITS;
		val >>= SCALE_BITS;	
		val <<= scale;
		val += reloc_amt;	
		if (((1 << scale) - 1) & val)
			dload_error(dlthis,
				    "Unaligned reference in %s offset "
				    FMT_UI32, dlthis->image_secn->name,
				    rp->vaddr + dlthis->image_offset);
		break;
#endif
	}			
	
	if (reloc_info & ROP_W) {	
#ifdef RFV_SCALE
		val >>= scale;
#endif
		if (dload_repack(dlthis, val, data, fieldsz, offset,
				 RFV_SIGN(reloc_info))) {
			if ((second_pass == false) &&
			    (dload_tramp_avail(dlthis, rp) == true)) {

				dload_repack(dlthis, orig_val, data, fieldsz,
					     offset, RFV_SIGN(reloc_info));
				if (!dload_tramp_generate(dlthis,
							(dlthis->image_secn -
							 dlthis->ldr_sections),
							 dlthis->image_offset,
							 img_pkt, rp)) {
					dload_error(dlthis,
						    "Failed to "
						    "generate trampoline for "
						    "bit overflow");
					dload_error(dlthis,
						    "Relocation val " FMT_UI32
						    " overflows %d bits in %s "
						    "offset " FMT_UI32, val,
						    fieldsz,
						    dlthis->image_secn->name,
						    dlthis->image_offset +
						    rp->vaddr);
				} else
					*tramps_generated = true;
			} else {
				dload_error(dlthis, "Relocation value "
					    FMT_UI32 " overflows %d bits in %s"
					    " offset " FMT_UI32, val, fieldsz,
					    dlthis->image_secn->name,
					    dlthis->image_offset + rp->vaddr);
			}
		}
	} else if (top)
		*stackp = val;
}				
