/*
 * x86 instruction analysis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) IBM Corporation, 2002, 2004, 2009
 */

#include <linux/string.h>
#include <asm/inat.h>
#include <asm/insn.h>

#define validate_next(t, insn, n)	\
	((insn)->next_byte + sizeof(t) + n - (insn)->kaddr <= MAX_INSN_SIZE)

#define __get_next(t, insn)	\
	({ t r = *(t*)insn->next_byte; insn->next_byte += sizeof(t); r; })

#define __peek_nbyte_next(t, insn, n)	\
	({ t r = *(t*)((insn)->next_byte + n); r; })

#define get_next(t, insn)	\
	({ if (unlikely(!validate_next(t, insn, 0))) goto err_out; __get_next(t, insn); })

#define peek_nbyte_next(t, insn, n)	\
	({ if (unlikely(!validate_next(t, insn, n))) goto err_out; __peek_nbyte_next(t, insn, n); })

#define peek_next(t, insn)	peek_nbyte_next(t, insn, 0)

void insn_init(struct insn *insn, const void *kaddr, int x86_64)
{
	memset(insn, 0, sizeof(*insn));
	insn->kaddr = kaddr;
	insn->next_byte = kaddr;
	insn->x86_64 = x86_64 ? 1 : 0;
	insn->opnd_bytes = 4;
	if (x86_64)
		insn->addr_bytes = 8;
	else
		insn->addr_bytes = 4;
}

void insn_get_prefixes(struct insn *insn)
{
	struct insn_field *prefixes = &insn->prefixes;
	insn_attr_t attr;
	insn_byte_t b, lb;
	int i, nb;

	if (prefixes->got)
		return;

	nb = 0;
	lb = 0;
	b = peek_next(insn_byte_t, insn);
	attr = inat_get_opcode_attribute(b);
	while (inat_is_legacy_prefix(attr)) {
		
		for (i = 0; i < nb; i++)
			if (prefixes->bytes[i] == b)
				goto found;
		if (nb == 4)
			
			break;
		prefixes->bytes[nb++] = b;
		if (inat_is_address_size_prefix(attr)) {
			
			if (insn->x86_64)
				insn->addr_bytes ^= 12;
			else
				insn->addr_bytes ^= 6;
		} else if (inat_is_operand_size_prefix(attr)) {
			
			insn->opnd_bytes ^= 6;
		}
found:
		prefixes->nbytes++;
		insn->next_byte++;
		lb = b;
		b = peek_next(insn_byte_t, insn);
		attr = inat_get_opcode_attribute(b);
	}
	
	if (lb && lb != insn->prefixes.bytes[3]) {
		if (unlikely(insn->prefixes.bytes[3])) {
			
			b = insn->prefixes.bytes[3];
			for (i = 0; i < nb; i++)
				if (prefixes->bytes[i] == lb)
					prefixes->bytes[i] = b;
		}
		insn->prefixes.bytes[3] = lb;
	}

	
	if (insn->x86_64) {
		b = peek_next(insn_byte_t, insn);
		attr = inat_get_opcode_attribute(b);
		if (inat_is_rex_prefix(attr)) {
			insn->rex_prefix.value = b;
			insn->rex_prefix.nbytes = 1;
			insn->next_byte++;
			if (X86_REX_W(b))
				
				insn->opnd_bytes = 8;
		}
	}
	insn->rex_prefix.got = 1;

	
	b = peek_next(insn_byte_t, insn);
	attr = inat_get_opcode_attribute(b);
	if (inat_is_vex_prefix(attr)) {
		insn_byte_t b2 = peek_nbyte_next(insn_byte_t, insn, 1);
		if (!insn->x86_64) {
			if (X86_MODRM_MOD(b2) != 3)
				goto vex_end;
		}
		insn->vex_prefix.bytes[0] = b;
		insn->vex_prefix.bytes[1] = b2;
		if (inat_is_vex3_prefix(attr)) {
			b2 = peek_nbyte_next(insn_byte_t, insn, 2);
			insn->vex_prefix.bytes[2] = b2;
			insn->vex_prefix.nbytes = 3;
			insn->next_byte += 3;
			if (insn->x86_64 && X86_VEX_W(b2))
				
				insn->opnd_bytes = 8;
		} else {
			insn->vex_prefix.nbytes = 2;
			insn->next_byte += 2;
		}
	}
vex_end:
	insn->vex_prefix.got = 1;

	prefixes->got = 1;

err_out:
	return;
}

void insn_get_opcode(struct insn *insn)
{
	struct insn_field *opcode = &insn->opcode;
	insn_byte_t op;
	int pfx_id;
	if (opcode->got)
		return;
	if (!insn->prefixes.got)
		insn_get_prefixes(insn);

	
	op = get_next(insn_byte_t, insn);
	opcode->bytes[0] = op;
	opcode->nbytes = 1;

	
	if (insn_is_avx(insn)) {
		insn_byte_t m, p;
		m = insn_vex_m_bits(insn);
		p = insn_vex_p_bits(insn);
		insn->attr = inat_get_avx_attribute(op, m, p);
		if (!inat_accept_vex(insn->attr) && !inat_is_group(insn->attr))
			insn->attr = 0;	
		goto end;	
	}

	insn->attr = inat_get_opcode_attribute(op);
	while (inat_is_escape(insn->attr)) {
		
		op = get_next(insn_byte_t, insn);
		opcode->bytes[opcode->nbytes++] = op;
		pfx_id = insn_last_prefix_id(insn);
		insn->attr = inat_get_escape_attribute(op, pfx_id, insn->attr);
	}
	if (inat_must_vex(insn->attr))
		insn->attr = 0;	
end:
	opcode->got = 1;

err_out:
	return;
}

void insn_get_modrm(struct insn *insn)
{
	struct insn_field *modrm = &insn->modrm;
	insn_byte_t pfx_id, mod;
	if (modrm->got)
		return;
	if (!insn->opcode.got)
		insn_get_opcode(insn);

	if (inat_has_modrm(insn->attr)) {
		mod = get_next(insn_byte_t, insn);
		modrm->value = mod;
		modrm->nbytes = 1;
		if (inat_is_group(insn->attr)) {
			pfx_id = insn_last_prefix_id(insn);
			insn->attr = inat_get_group_attribute(mod, pfx_id,
							      insn->attr);
			if (insn_is_avx(insn) && !inat_accept_vex(insn->attr))
				insn->attr = 0;	
		}
	}

	if (insn->x86_64 && inat_is_force64(insn->attr))
		insn->opnd_bytes = 8;
	modrm->got = 1;

err_out:
	return;
}


int insn_rip_relative(struct insn *insn)
{
	struct insn_field *modrm = &insn->modrm;

	if (!insn->x86_64)
		return 0;
	if (!modrm->got)
		insn_get_modrm(insn);
	return (modrm->nbytes && (modrm->value & 0xc7) == 0x5);
}

void insn_get_sib(struct insn *insn)
{
	insn_byte_t modrm;

	if (insn->sib.got)
		return;
	if (!insn->modrm.got)
		insn_get_modrm(insn);
	if (insn->modrm.nbytes) {
		modrm = (insn_byte_t)insn->modrm.value;
		if (insn->addr_bytes != 2 &&
		    X86_MODRM_MOD(modrm) != 3 && X86_MODRM_RM(modrm) == 4) {
			insn->sib.value = get_next(insn_byte_t, insn);
			insn->sib.nbytes = 1;
		}
	}
	insn->sib.got = 1;

err_out:
	return;
}


void insn_get_displacement(struct insn *insn)
{
	insn_byte_t mod, rm, base;

	if (insn->displacement.got)
		return;
	if (!insn->sib.got)
		insn_get_sib(insn);
	if (insn->modrm.nbytes) {
		mod = X86_MODRM_MOD(insn->modrm.value);
		rm = X86_MODRM_RM(insn->modrm.value);
		base = X86_SIB_BASE(insn->sib.value);
		if (mod == 3)
			goto out;
		if (mod == 1) {
			insn->displacement.value = get_next(char, insn);
			insn->displacement.nbytes = 1;
		} else if (insn->addr_bytes == 2) {
			if ((mod == 0 && rm == 6) || mod == 2) {
				insn->displacement.value =
					 get_next(short, insn);
				insn->displacement.nbytes = 2;
			}
		} else {
			if ((mod == 0 && rm == 5) || mod == 2 ||
			    (mod == 0 && base == 5)) {
				insn->displacement.value = get_next(int, insn);
				insn->displacement.nbytes = 4;
			}
		}
	}
out:
	insn->displacement.got = 1;

err_out:
	return;
}

static int __get_moffset(struct insn *insn)
{
	switch (insn->addr_bytes) {
	case 2:
		insn->moffset1.value = get_next(short, insn);
		insn->moffset1.nbytes = 2;
		break;
	case 4:
		insn->moffset1.value = get_next(int, insn);
		insn->moffset1.nbytes = 4;
		break;
	case 8:
		insn->moffset1.value = get_next(int, insn);
		insn->moffset1.nbytes = 4;
		insn->moffset2.value = get_next(int, insn);
		insn->moffset2.nbytes = 4;
		break;
	default:	
		goto err_out;
	}
	insn->moffset1.got = insn->moffset2.got = 1;

	return 1;

err_out:
	return 0;
}

static int __get_immv32(struct insn *insn)
{
	switch (insn->opnd_bytes) {
	case 2:
		insn->immediate.value = get_next(short, insn);
		insn->immediate.nbytes = 2;
		break;
	case 4:
	case 8:
		insn->immediate.value = get_next(int, insn);
		insn->immediate.nbytes = 4;
		break;
	default:	
		goto err_out;
	}

	return 1;

err_out:
	return 0;
}

static int __get_immv(struct insn *insn)
{
	switch (insn->opnd_bytes) {
	case 2:
		insn->immediate1.value = get_next(short, insn);
		insn->immediate1.nbytes = 2;
		break;
	case 4:
		insn->immediate1.value = get_next(int, insn);
		insn->immediate1.nbytes = 4;
		break;
	case 8:
		insn->immediate1.value = get_next(int, insn);
		insn->immediate1.nbytes = 4;
		insn->immediate2.value = get_next(int, insn);
		insn->immediate2.nbytes = 4;
		break;
	default:	
		goto err_out;
	}
	insn->immediate1.got = insn->immediate2.got = 1;

	return 1;
err_out:
	return 0;
}

static int __get_immptr(struct insn *insn)
{
	switch (insn->opnd_bytes) {
	case 2:
		insn->immediate1.value = get_next(short, insn);
		insn->immediate1.nbytes = 2;
		break;
	case 4:
		insn->immediate1.value = get_next(int, insn);
		insn->immediate1.nbytes = 4;
		break;
	case 8:
		
		return 0;
	default:	
		goto err_out;
	}
	insn->immediate2.value = get_next(unsigned short, insn);
	insn->immediate2.nbytes = 2;
	insn->immediate1.got = insn->immediate2.got = 1;

	return 1;
err_out:
	return 0;
}

void insn_get_immediate(struct insn *insn)
{
	if (insn->immediate.got)
		return;
	if (!insn->displacement.got)
		insn_get_displacement(insn);

	if (inat_has_moffset(insn->attr)) {
		if (!__get_moffset(insn))
			goto err_out;
		goto done;
	}

	if (!inat_has_immediate(insn->attr))
		
		goto done;

	switch (inat_immediate_size(insn->attr)) {
	case INAT_IMM_BYTE:
		insn->immediate.value = get_next(char, insn);
		insn->immediate.nbytes = 1;
		break;
	case INAT_IMM_WORD:
		insn->immediate.value = get_next(short, insn);
		insn->immediate.nbytes = 2;
		break;
	case INAT_IMM_DWORD:
		insn->immediate.value = get_next(int, insn);
		insn->immediate.nbytes = 4;
		break;
	case INAT_IMM_QWORD:
		insn->immediate1.value = get_next(int, insn);
		insn->immediate1.nbytes = 4;
		insn->immediate2.value = get_next(int, insn);
		insn->immediate2.nbytes = 4;
		break;
	case INAT_IMM_PTR:
		if (!__get_immptr(insn))
			goto err_out;
		break;
	case INAT_IMM_VWORD32:
		if (!__get_immv32(insn))
			goto err_out;
		break;
	case INAT_IMM_VWORD:
		if (!__get_immv(insn))
			goto err_out;
		break;
	default:
		
		goto err_out;
	}
	if (inat_has_second_immediate(insn->attr)) {
		insn->immediate2.value = get_next(char, insn);
		insn->immediate2.nbytes = 1;
	}
done:
	insn->immediate.got = 1;

err_out:
	return;
}

void insn_get_length(struct insn *insn)
{
	if (insn->length)
		return;
	if (!insn->immediate.got)
		insn_get_immediate(insn);
	insn->length = (unsigned char)((unsigned long)insn->next_byte
				     - (unsigned long)insn->kaddr);
}
