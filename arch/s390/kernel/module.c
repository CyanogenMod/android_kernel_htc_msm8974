/*
 *  arch/s390/kernel/module.c - Kernel module help for s390.
 *
 *  S390 version
 *    Copyright (C) 2002, 2003 IBM Deutschland Entwicklung GmbH,
 *			       IBM Corporation
 *    Author(s): Arnd Bergmann (arndb@de.ibm.com)
 *		 Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  based on i386 version
 *    Copyright (C) 2001 Rusty Russell.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/module.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/moduleloader.h>
#include <linux/bug.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(fmt , ...)
#endif

#ifndef CONFIG_64BIT
#define PLT_ENTRY_SIZE 12
#else 
#define PLT_ENTRY_SIZE 20
#endif 

void module_free(struct module *mod, void *module_region)
{
	if (mod) {
		vfree(mod->arch.syminfo);
		mod->arch.syminfo = NULL;
	}
	vfree(module_region);
}

static void
check_rela(Elf_Rela *rela, struct module *me)
{
	struct mod_arch_syminfo *info;

	info = me->arch.syminfo + ELF_R_SYM (rela->r_info);
	switch (ELF_R_TYPE (rela->r_info)) {
	case R_390_GOT12:	
	case R_390_GOT16:	
	case R_390_GOT20:	
	case R_390_GOT32:	
	case R_390_GOT64:	
	case R_390_GOTENT:	
	case R_390_GOTPLT12:	
	case R_390_GOTPLT16:	
	case R_390_GOTPLT20:	
	case R_390_GOTPLT32:	
	case R_390_GOTPLT64:	
	case R_390_GOTPLTENT:	
		if (info->got_offset == -1UL) {
			info->got_offset = me->arch.got_size;
			me->arch.got_size += sizeof(void*);
		}
		break;
	case R_390_PLT16DBL:	
	case R_390_PLT32DBL:	
	case R_390_PLT32:	
	case R_390_PLT64:	
	case R_390_PLTOFF16:	
	case R_390_PLTOFF32:	
	case R_390_PLTOFF64:	
		if (info->plt_offset == -1UL) {
			info->plt_offset = me->arch.plt_size;
			me->arch.plt_size += PLT_ENTRY_SIZE;
		}
		break;
	case R_390_COPY:
	case R_390_GLOB_DAT:
	case R_390_JMP_SLOT:
	case R_390_RELATIVE:
		break;
	}
}

int
module_frob_arch_sections(Elf_Ehdr *hdr, Elf_Shdr *sechdrs,
			  char *secstrings, struct module *me)
{
	Elf_Shdr *symtab;
	Elf_Sym *symbols;
	Elf_Rela *rela;
	char *strings;
	int nrela, i, j;

	
	symtab = NULL;
	for (i = 0; i < hdr->e_shnum; i++)
		switch (sechdrs[i].sh_type) {
		case SHT_SYMTAB:
			symtab = sechdrs + i;
			break;
		}
	if (!symtab) {
		printk(KERN_ERR "module %s: no symbol table\n", me->name);
		return -ENOEXEC;
	}

	
	me->arch.nsyms = symtab->sh_size / sizeof(Elf_Sym);
	me->arch.syminfo = vmalloc(me->arch.nsyms *
				   sizeof(struct mod_arch_syminfo));
	if (!me->arch.syminfo)
		return -ENOMEM;
	symbols = (void *) hdr + symtab->sh_offset;
	strings = (void *) hdr + sechdrs[symtab->sh_link].sh_offset;
	for (i = 0; i < me->arch.nsyms; i++) {
		if (symbols[i].st_shndx == SHN_UNDEF &&
		    strcmp(strings + symbols[i].st_name,
			   "_GLOBAL_OFFSET_TABLE_") == 0)
			
			symbols[i].st_shndx = SHN_ABS;
		me->arch.syminfo[i].got_offset = -1UL;
		me->arch.syminfo[i].plt_offset = -1UL;
		me->arch.syminfo[i].got_initialized = 0;
		me->arch.syminfo[i].plt_initialized = 0;
	}

	
	me->arch.got_size = me->arch.plt_size = 0;
	for (i = 0; i < hdr->e_shnum; i++) {
		if (sechdrs[i].sh_type != SHT_RELA)
			continue;
		nrela = sechdrs[i].sh_size / sizeof(Elf_Rela);
		rela = (void *) hdr + sechdrs[i].sh_offset;
		for (j = 0; j < nrela; j++)
			check_rela(rela + j, me);
	}

	me->core_size = ALIGN(me->core_size, 4);
	me->arch.got_offset = me->core_size;
	me->core_size += me->arch.got_size;
	me->arch.plt_offset = me->core_size;
	me->core_size += me->arch.plt_size;
	return 0;
}

static int
apply_rela(Elf_Rela *rela, Elf_Addr base, Elf_Sym *symtab, 
	   struct module *me)
{
	struct mod_arch_syminfo *info;
	Elf_Addr loc, val;
	int r_type, r_sym;

	
	loc = base + rela->r_offset;
	r_sym = ELF_R_SYM(rela->r_info);
	r_type = ELF_R_TYPE(rela->r_info);
	info = me->arch.syminfo + r_sym;
	val = symtab[r_sym].st_value;

	switch (r_type) {
	case R_390_8:		
	case R_390_12:		
	case R_390_16:		
	case R_390_20:		
	case R_390_32:		
	case R_390_64:		
		val += rela->r_addend;
		if (r_type == R_390_8)
			*(unsigned char *) loc = val;
		else if (r_type == R_390_12)
			*(unsigned short *) loc = (val & 0xfff) |
				(*(unsigned short *) loc & 0xf000);
		else if (r_type == R_390_16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_20)
			*(unsigned int *) loc =
				(*(unsigned int *) loc & 0xf00000ff) |
				(val & 0xfff) << 16 | (val & 0xff000) >> 4;
		else if (r_type == R_390_32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_64)
			*(unsigned long *) loc = val;
		break;
	case R_390_PC16:	
	case R_390_PC16DBL:	
	case R_390_PC32DBL:	
	case R_390_PC32:	
	case R_390_PC64:	
		val += rela->r_addend - loc;
		if (r_type == R_390_PC16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_PC16DBL)
			*(unsigned short *) loc = val >> 1;
		else if (r_type == R_390_PC32DBL)
			*(unsigned int *) loc = val >> 1;
		else if (r_type == R_390_PC32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_PC64)
			*(unsigned long *) loc = val;
		break;
	case R_390_GOT12:	
	case R_390_GOT16:	
	case R_390_GOT20:	
	case R_390_GOT32:	
	case R_390_GOT64:	
	case R_390_GOTENT:	
	case R_390_GOTPLT12:	
	case R_390_GOTPLT20:	
	case R_390_GOTPLT16:	
	case R_390_GOTPLT32:	
	case R_390_GOTPLT64:	
	case R_390_GOTPLTENT:	
		if (info->got_initialized == 0) {
			Elf_Addr *gotent;

			gotent = me->module_core + me->arch.got_offset +
				info->got_offset;
			*gotent = val;
			info->got_initialized = 1;
		}
		val = info->got_offset + rela->r_addend;
		if (r_type == R_390_GOT12 ||
		    r_type == R_390_GOTPLT12)
			*(unsigned short *) loc = (val & 0xfff) |
				(*(unsigned short *) loc & 0xf000);
		else if (r_type == R_390_GOT16 ||
			 r_type == R_390_GOTPLT16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_GOT20 ||
			 r_type == R_390_GOTPLT20)
			*(unsigned int *) loc =
				(*(unsigned int *) loc & 0xf00000ff) |
				(val & 0xfff) << 16 | (val & 0xff000) >> 4;
		else if (r_type == R_390_GOT32 ||
			 r_type == R_390_GOTPLT32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_GOTENT ||
			 r_type == R_390_GOTPLTENT)
			*(unsigned int *) loc =
				(val + (Elf_Addr) me->module_core - loc) >> 1;
		else if (r_type == R_390_GOT64 ||
			 r_type == R_390_GOTPLT64)
			*(unsigned long *) loc = val;
		break;
	case R_390_PLT16DBL:	
	case R_390_PLT32DBL:	
	case R_390_PLT32:	
	case R_390_PLT64:	
	case R_390_PLTOFF16:	
	case R_390_PLTOFF32:	
	case R_390_PLTOFF64:	
		if (info->plt_initialized == 0) {
			unsigned int *ip;
			ip = me->module_core + me->arch.plt_offset +
				info->plt_offset;
#ifndef CONFIG_64BIT
			ip[0] = 0x0d105810; 
			ip[1] = 0x100607f1;
			ip[2] = val;
#else 
			ip[0] = 0x0d10e310; 
			ip[1] = 0x100a0004;
			ip[2] = 0x07f10000;
			ip[3] = (unsigned int) (val >> 32);
			ip[4] = (unsigned int) val;
#endif 
			info->plt_initialized = 1;
		}
		if (r_type == R_390_PLTOFF16 ||
		    r_type == R_390_PLTOFF32 ||
		    r_type == R_390_PLTOFF64)
			val = me->arch.plt_offset - me->arch.got_offset +
				info->plt_offset + rela->r_addend;
		else {
			if (!((r_type == R_390_PLT16DBL &&
			       val - loc + 0xffffUL < 0x1ffffeUL) ||
			      (r_type == R_390_PLT32DBL &&
			       val - loc + 0xffffffffULL < 0x1fffffffeULL)))
				val = (Elf_Addr) me->module_core +
					me->arch.plt_offset +
					info->plt_offset;
			val += rela->r_addend - loc;
		}
		if (r_type == R_390_PLT16DBL)
			*(unsigned short *) loc = val >> 1;
		else if (r_type == R_390_PLTOFF16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_PLT32DBL)
			*(unsigned int *) loc = val >> 1;
		else if (r_type == R_390_PLT32 ||
			 r_type == R_390_PLTOFF32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_PLT64 ||
			 r_type == R_390_PLTOFF64)
			*(unsigned long *) loc = val;
		break;
	case R_390_GOTOFF16:	
	case R_390_GOTOFF32:	
	case R_390_GOTOFF64:	
		val = val + rela->r_addend -
			((Elf_Addr) me->module_core + me->arch.got_offset);
		if (r_type == R_390_GOTOFF16)
			*(unsigned short *) loc = val;
		else if (r_type == R_390_GOTOFF32)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_GOTOFF64)
			*(unsigned long *) loc = val;
		break;
	case R_390_GOTPC:	
	case R_390_GOTPCDBL:	
		val = (Elf_Addr) me->module_core + me->arch.got_offset +
			rela->r_addend - loc;
		if (r_type == R_390_GOTPC)
			*(unsigned int *) loc = val;
		else if (r_type == R_390_GOTPCDBL)
			*(unsigned int *) loc = val >> 1;
		break;
	case R_390_COPY:
	case R_390_GLOB_DAT:	
	case R_390_JMP_SLOT:	
	case R_390_RELATIVE:	
		break;
	default:
		printk(KERN_ERR "module %s: Unknown relocation: %u\n",
		       me->name, r_type);
		return -ENOEXEC;
	}
	return 0;
}

int
apply_relocate_add(Elf_Shdr *sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec,
		   struct module *me)
{
	Elf_Addr base;
	Elf_Sym *symtab;
	Elf_Rela *rela;
	unsigned long i, n;
	int rc;

	DEBUGP("Applying relocate section %u to %u\n",
	       relsec, sechdrs[relsec].sh_info);
	base = sechdrs[sechdrs[relsec].sh_info].sh_addr;
	symtab = (Elf_Sym *) sechdrs[symindex].sh_addr;
	rela = (Elf_Rela *) sechdrs[relsec].sh_addr;
	n = sechdrs[relsec].sh_size / sizeof(Elf_Rela);

	for (i = 0; i < n; i++, rela++) {
		rc = apply_rela(rela, base, symtab, me);
		if (rc)
			return rc;
	}
	return 0;
}

int module_finalize(const Elf_Ehdr *hdr,
		    const Elf_Shdr *sechdrs,
		    struct module *me)
{
	vfree(me->arch.syminfo);
	me->arch.syminfo = NULL;
	return 0;
}
