/*    Kernel dynamically loadable module help for PARISC.
 *
 *    The best reference for this stuff is probably the Processor-
 *    Specific ELF Supplement for PA-RISC:
 *        http://ftp.parisc-linux.org/docs/arch/elf-pa-hp.pdf
 *
 *    Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *    Copyright (C) 2003 Randolph Chung <tausq at debian . org>
 *    Copyright (C) 2008 Helge Deller <deller@gmx.de>
 *
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *    Notes:
 *    - PLT stub handling
 *      On 32bit (and sometimes 64bit) and with big kernel modules like xfs or
 *      ipv6 the relocation types R_PARISC_PCREL17F and R_PARISC_PCREL22F may
 *      fail to reach their PLT stub if we only create one big stub array for
 *      all sections at the beginning of the core or init section.
 *      Instead we now insert individual PLT stub entries directly in front of
 *      of the code sections where the stubs are actually called.
 *      This reduces the distance between the PCREL location and the stub entry
 *      so that the relocations can be fulfilled.
 *      While calculating the final layout of the kernel module in memory, the
 *      kernel module loader calls arch_mod_section_prepend() to request the
 *      to be reserved amount of memory in front of each individual section.
 *
 *    - SEGREL32 handling
 *      We are not doing SEGREL32 handling correctly. According to the ABI, we
 *      should do a value offset, like this:
 *			if (in_init(me, (void *)val))
 *				val -= (uint32_t)me->module_init;
 *			else
 *				val -= (uint32_t)me->module_core;
 *	However, SEGREL32 is used only for PARISC unwind entries, and we want
 *	those entries to have an absolute address, and not just an offset.
 *
 *	The unwind table mechanism has the ability to specify an offset for 
 *	the unwind table; however, because we split off the init functions into
 *	a different piece of memory, it is not possible to do this using a 
 *	single offset. Instead, we use the above hack for now.
 */

#include <linux/moduleloader.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/pgtable.h>
#include <asm/unwind.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(fmt...)
#endif

#define RELOC_REACHABLE(val, bits) \
	(( ( !((val) & (1<<((bits)-1))) && ((val)>>(bits)) != 0 )  ||	\
	     ( ((val) & (1<<((bits)-1))) && ((val)>>(bits)) != (((__typeof__(val))(~0))>>((bits)+2)))) ? \
	0 : 1)

#define CHECK_RELOC(val, bits) \
	if (!RELOC_REACHABLE(val, bits)) { \
		printk(KERN_ERR "module %s relocation of symbol %s is out of range (0x%lx in %d bits)\n", \
		me->name, strtab + sym->st_name, (unsigned long)val, bits); \
		return -ENOEXEC;			\
	}

#define MAX_GOTS	4095

static inline int in_init(struct module *me, void *loc)
{
	return (loc >= me->module_init &&
		loc <= (me->module_init + me->init_size));
}

static inline int in_core(struct module *me, void *loc)
{
	return (loc >= me->module_core &&
		loc <= (me->module_core + me->core_size));
}

static inline int in_local(struct module *me, void *loc)
{
	return in_init(me, loc) || in_core(me, loc);
}

#ifndef CONFIG_64BIT
struct got_entry {
	Elf32_Addr addr;
};

struct stub_entry {
	Elf32_Word insns[2]; 
};
#else
struct got_entry {
	Elf64_Addr addr;
};

struct stub_entry {
	Elf64_Word insns[4]; 
};
#endif

#define rnd(x)			(((x)+0x1000)&~0x1fff)
#define fsel(v,a)		((v)+(a))
#define lsel(v,a)		(((v)+(a))>>11)
#define rsel(v,a)		(((v)+(a))&0x7ff)
#define lrsel(v,a)		(((v)+rnd(a))>>11)
#define rrsel(v,a)		((((v)+rnd(a))&0x7ff)+((a)-rnd(a)))

#define mask(x,sz)		((x) & ~((1<<(sz))-1))


static inline int sign_unext(int x, int len)
{
	int len_ones;

	len_ones = (1 << len) - 1;
	return x & len_ones;
}

static inline int low_sign_unext(int x, int len)
{
	int sign, temp;

	sign = (x >> (len-1)) & 1;
	temp = sign_unext(x, len-1);
	return (temp << 1) | sign;
}

static inline int reassemble_14(int as14)
{
	return (((as14 & 0x1fff) << 1) |
		((as14 & 0x2000) >> 13));
}

static inline int reassemble_16a(int as16)
{
	int s, t;

	
	t = (as16 << 1) & 0xffff;
	s = (as16 & 0x8000);
	return (t ^ s ^ (s >> 1)) | (s >> 15);
}


static inline int reassemble_17(int as17)
{
	return (((as17 & 0x10000) >> 16) |
		((as17 & 0x0f800) << 5) |
		((as17 & 0x00400) >> 8) |
		((as17 & 0x003ff) << 3));
}

static inline int reassemble_21(int as21)
{
	return (((as21 & 0x100000) >> 20) |
		((as21 & 0x0ffe00) >> 8) |
		((as21 & 0x000180) << 7) |
		((as21 & 0x00007c) << 14) |
		((as21 & 0x000003) << 12));
}

static inline int reassemble_22(int as22)
{
	return (((as22 & 0x200000) >> 21) |
		((as22 & 0x1f0000) << 5) |
		((as22 & 0x00f800) << 5) |
		((as22 & 0x000400) >> 8) |
		((as22 & 0x0003ff) << 3));
}

void *module_alloc(unsigned long size)
{
	if (size == 0)
		return NULL;
	return __vmalloc_node_range(size, 1, VMALLOC_START, VMALLOC_END,
				    GFP_KERNEL | __GFP_HIGHMEM,
				    PAGE_KERNEL_RWX, -1,
				    __builtin_return_address(0));
}

#ifndef CONFIG_64BIT
static inline unsigned long count_gots(const Elf_Rela *rela, unsigned long n)
{
	return 0;
}

static inline unsigned long count_fdescs(const Elf_Rela *rela, unsigned long n)
{
	return 0;
}

static inline unsigned long count_stubs(const Elf_Rela *rela, unsigned long n)
{
	unsigned long cnt = 0;

	for (; n > 0; n--, rela++)
	{
		switch (ELF32_R_TYPE(rela->r_info)) {
			case R_PARISC_PCREL17F:
			case R_PARISC_PCREL22F:
				cnt++;
		}
	}

	return cnt;
}
#else
static inline unsigned long count_gots(const Elf_Rela *rela, unsigned long n)
{
	unsigned long cnt = 0;

	for (; n > 0; n--, rela++)
	{
		switch (ELF64_R_TYPE(rela->r_info)) {
			case R_PARISC_LTOFF21L:
			case R_PARISC_LTOFF14R:
			case R_PARISC_PCREL22F:
				cnt++;
		}
	}

	return cnt;
}

static inline unsigned long count_fdescs(const Elf_Rela *rela, unsigned long n)
{
	unsigned long cnt = 0;

	for (; n > 0; n--, rela++)
	{
		switch (ELF64_R_TYPE(rela->r_info)) {
			case R_PARISC_FPTR64:
				cnt++;
		}
	}

	return cnt;
}

static inline unsigned long count_stubs(const Elf_Rela *rela, unsigned long n)
{
	unsigned long cnt = 0;

	for (; n > 0; n--, rela++)
	{
		switch (ELF64_R_TYPE(rela->r_info)) {
			case R_PARISC_PCREL22F:
				cnt++;
		}
	}

	return cnt;
}
#endif


void module_free(struct module *mod, void *module_region)
{
	kfree(mod->arch.section);
	mod->arch.section = NULL;

	vfree(module_region);
}

unsigned int arch_mod_section_prepend(struct module *mod,
				      unsigned int section)
{
	return (mod->arch.section[section].stub_entries + 1)
		* sizeof(struct stub_entry);
}

#define CONST 
int module_frob_arch_sections(CONST Elf_Ehdr *hdr,
			      CONST Elf_Shdr *sechdrs,
			      CONST char *secstrings,
			      struct module *me)
{
	unsigned long gots = 0, fdescs = 0, len;
	unsigned int i;

	len = hdr->e_shnum * sizeof(me->arch.section[0]);
	me->arch.section = kzalloc(len, GFP_KERNEL);
	if (!me->arch.section)
		return -ENOMEM;

	for (i = 1; i < hdr->e_shnum; i++) {
		const Elf_Rela *rels = (void *)sechdrs[i].sh_addr;
		unsigned long nrels = sechdrs[i].sh_size / sizeof(*rels);
		unsigned int count, s;

		if (strncmp(secstrings + sechdrs[i].sh_name,
			    ".PARISC.unwind", 14) == 0)
			me->arch.unwind_section = i;

		if (sechdrs[i].sh_type != SHT_RELA)
			continue;

		gots += count_gots(rels, nrels);
		fdescs += count_fdescs(rels, nrels);

		count = count_stubs(rels, nrels);
		if (!count)
			continue;

		
		
		s = sechdrs[i].sh_info;

		
		WARN_ON(me->arch.section[s].stub_entries);

		
		me->arch.section[s].stub_entries += count;
	}

	
	me->core_size = ALIGN(me->core_size, 16);
	me->arch.got_offset = me->core_size;
	me->core_size += gots * sizeof(struct got_entry);

	me->core_size = ALIGN(me->core_size, 16);
	me->arch.fdesc_offset = me->core_size;
	me->core_size += fdescs * sizeof(Elf_Fdesc);

	me->arch.got_max = gots;
	me->arch.fdesc_max = fdescs;

	return 0;
}

#ifdef CONFIG_64BIT
static Elf64_Word get_got(struct module *me, unsigned long value, long addend)
{
	unsigned int i;
	struct got_entry *got;

	value += addend;

	BUG_ON(value == 0);

	got = me->module_core + me->arch.got_offset;
	for (i = 0; got[i].addr; i++)
		if (got[i].addr == value)
			goto out;

	BUG_ON(++me->arch.got_count > me->arch.got_max);

	got[i].addr = value;
 out:
	DEBUGP("GOT ENTRY %d[%x] val %lx\n", i, i*sizeof(struct got_entry),
	       value);
	return i * sizeof(struct got_entry);
}
#endif 

#ifdef CONFIG_64BIT
static Elf_Addr get_fdesc(struct module *me, unsigned long value)
{
	Elf_Fdesc *fdesc = me->module_core + me->arch.fdesc_offset;

	if (!value) {
		printk(KERN_ERR "%s: zero OPD requested!\n", me->name);
		return 0;
	}

	
	while (fdesc->addr) {
		if (fdesc->addr == value)
			return (Elf_Addr)fdesc;
		fdesc++;
	}

	BUG_ON(++me->arch.fdesc_count > me->arch.fdesc_max);

	
	fdesc->addr = value;
	fdesc->gp = (Elf_Addr)me->module_core + me->arch.got_offset;
	return (Elf_Addr)fdesc;
}
#endif 

enum elf_stub_type {
	ELF_STUB_GOT,
	ELF_STUB_MILLI,
	ELF_STUB_DIRECT,
};

static Elf_Addr get_stub(struct module *me, unsigned long value, long addend,
	enum elf_stub_type stub_type, Elf_Addr loc0, unsigned int targetsec)
{
	struct stub_entry *stub;
	int __maybe_unused d;

	
	if (!me->arch.section[targetsec].stub_offset) {
		loc0 -= (me->arch.section[targetsec].stub_entries + 1) *
				sizeof(struct stub_entry);
		
		loc0 = ALIGN(loc0, sizeof(struct stub_entry));
		me->arch.section[targetsec].stub_offset = loc0;
	}

	
	stub = (void *) me->arch.section[targetsec].stub_offset;
	me->arch.section[targetsec].stub_offset += sizeof(struct stub_entry);

	
	BUG_ON(0 == me->arch.section[targetsec].stub_entries--);


#ifndef CONFIG_64BIT
	

	stub->insns[0] = 0x20200000;	
	stub->insns[1] = 0xe0202002;	

	stub->insns[0] |= reassemble_21(lrsel(value, addend));
	stub->insns[1] |= reassemble_17(rrsel(value, addend) / 4);

#else
	switch (stub_type) {
	case ELF_STUB_GOT:
		d = get_got(me, value, addend);
		if (d <= 15) {
			
			stub->insns[0] = 0x0f6010db; 
			stub->insns[0] |= low_sign_unext(d, 5) << 16;
		} else {
			
			stub->insns[0] = 0x537b0000; 
			stub->insns[0] |= reassemble_16a(d);
		}
		stub->insns[1] = 0x53610020;	
		stub->insns[2] = 0xe820d000;	
		stub->insns[3] = 0x537b0030;	
		break;
	case ELF_STUB_MILLI:
		stub->insns[0] = 0x20200000;	
		stub->insns[1] = 0x34210000;	
		stub->insns[2] = 0x50210020;	
		stub->insns[3] = 0xe820d002;	

		stub->insns[0] |= reassemble_21(lrsel(value, addend));
		stub->insns[1] |= reassemble_14(rrsel(value, addend));
		break;
	case ELF_STUB_DIRECT:
		stub->insns[0] = 0x20200000;    
		stub->insns[1] = 0x34210000;    
		stub->insns[2] = 0xe820d002;    

		stub->insns[0] |= reassemble_21(lrsel(value, addend));
		stub->insns[1] |= reassemble_14(rrsel(value, addend));
		break;
	}

#endif

	return (Elf_Addr)stub;
}

#ifndef CONFIG_64BIT
int apply_relocate_add(Elf_Shdr *sechdrs,
		       const char *strtab,
		       unsigned int symindex,
		       unsigned int relsec,
		       struct module *me)
{
	int i;
	Elf32_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	Elf32_Word *loc;
	Elf32_Addr val;
	Elf32_Sword addend;
	Elf32_Addr dot;
	Elf_Addr loc0;
	unsigned int targetsec = sechdrs[relsec].sh_info;
	
	register unsigned long dp asm ("r27");

	DEBUGP("Applying relocate section %u to %u\n", relsec,
	       targetsec);
	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		
		loc = (void *)sechdrs[targetsec].sh_addr
		      + rel[i].r_offset;
		
		loc0 = sechdrs[targetsec].sh_addr;
		
		sym = (Elf32_Sym *)sechdrs[symindex].sh_addr
			+ ELF32_R_SYM(rel[i].r_info);
		if (!sym->st_value) {
			printk(KERN_WARNING "%s: Unknown symbol %s\n",
			       me->name, strtab + sym->st_name);
			return -ENOENT;
		}
		
		dot =  (Elf32_Addr)loc & ~0x03;

		val = sym->st_value;
		addend = rel[i].r_addend;

#if 0
#define r(t) ELF32_R_TYPE(rel[i].r_info)==t ? #t :
		DEBUGP("Symbol %s loc 0x%x val 0x%x addend 0x%x: %s\n",
			strtab + sym->st_name,
			(uint32_t)loc, val, addend,
			r(R_PARISC_PLABEL32)
			r(R_PARISC_DIR32)
			r(R_PARISC_DIR21L)
			r(R_PARISC_DIR14R)
			r(R_PARISC_SEGREL32)
			r(R_PARISC_DPREL21L)
			r(R_PARISC_DPREL14R)
			r(R_PARISC_PCREL17F)
			r(R_PARISC_PCREL22F)
			"UNKNOWN");
#undef r
#endif

		switch (ELF32_R_TYPE(rel[i].r_info)) {
		case R_PARISC_PLABEL32:
			
			
			*loc = fsel(val, addend);
			break;
		case R_PARISC_DIR32:
			
			*loc = fsel(val, addend);
			break;
		case R_PARISC_DIR21L:
			
			val = lrsel(val, addend);
			*loc = mask(*loc, 21) | reassemble_21(val);
			break;
		case R_PARISC_DIR14R:
			
			val = rrsel(val, addend);
			*loc = mask(*loc, 14) | reassemble_14(val);
			break;
		case R_PARISC_SEGREL32:
			
			*loc = fsel(val, addend); 
			break;
		case R_PARISC_DPREL21L:
			
			val = lrsel(val - dp, addend);
			*loc = mask(*loc, 21) | reassemble_21(val);
			break;
		case R_PARISC_DPREL14R:
			
			val = rrsel(val - dp, addend);
			*loc = mask(*loc, 14) | reassemble_14(val);
			break;
		case R_PARISC_PCREL17F:
			
			
			val += addend;
			val = (val - dot - 8)/4;
			if (!RELOC_REACHABLE(val, 17)) {
				val = get_stub(me, sym->st_value, addend,
					ELF_STUB_DIRECT, loc0, targetsec);
				val = (val - dot - 8)/4;
				CHECK_RELOC(val, 17);
			}
			*loc = (*loc & ~0x1f1ffd) | reassemble_17(val);
			break;
		case R_PARISC_PCREL22F:
			
			
			val += addend;
			val = (val - dot - 8)/4;
			if (!RELOC_REACHABLE(val, 22)) {
				val = get_stub(me, sym->st_value, addend,
					ELF_STUB_DIRECT, loc0, targetsec);
				val = (val - dot - 8)/4;
				CHECK_RELOC(val, 22);
			}
			*loc = (*loc & ~0x3ff1ffd) | reassemble_22(val);
			break;

		default:
			printk(KERN_ERR "module %s: Unknown relocation: %u\n",
			       me->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
	}

	return 0;
}

#else
int apply_relocate_add(Elf_Shdr *sechdrs,
		       const char *strtab,
		       unsigned int symindex,
		       unsigned int relsec,
		       struct module *me)
{
	int i;
	Elf64_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf64_Sym *sym;
	Elf64_Word *loc;
	Elf64_Xword *loc64;
	Elf64_Addr val;
	Elf64_Sxword addend;
	Elf64_Addr dot;
	Elf_Addr loc0;
	unsigned int targetsec = sechdrs[relsec].sh_info;

	DEBUGP("Applying relocate section %u to %u\n", relsec,
	       targetsec);
	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		
		loc = (void *)sechdrs[targetsec].sh_addr
		      + rel[i].r_offset;
		
		loc0 = sechdrs[targetsec].sh_addr;
		
		sym = (Elf64_Sym *)sechdrs[symindex].sh_addr
			+ ELF64_R_SYM(rel[i].r_info);
		if (!sym->st_value) {
			printk(KERN_WARNING "%s: Unknown symbol %s\n",
			       me->name, strtab + sym->st_name);
			return -ENOENT;
		}
		
		dot = (Elf64_Addr)loc & ~0x03;
		loc64 = (Elf64_Xword *)loc;

		val = sym->st_value;
		addend = rel[i].r_addend;

#if 0
#define r(t) ELF64_R_TYPE(rel[i].r_info)==t ? #t :
		printk("Symbol %s loc %p val 0x%Lx addend 0x%Lx: %s\n",
			strtab + sym->st_name,
			loc, val, addend,
			r(R_PARISC_LTOFF14R)
			r(R_PARISC_LTOFF21L)
			r(R_PARISC_PCREL22F)
			r(R_PARISC_DIR64)
			r(R_PARISC_SEGREL32)
			r(R_PARISC_FPTR64)
			"UNKNOWN");
#undef r
#endif

		switch (ELF64_R_TYPE(rel[i].r_info)) {
		case R_PARISC_LTOFF21L:
			
			val = get_got(me, val, addend);
			DEBUGP("LTOFF21L Symbol %s loc %p val %lx\n",
			       strtab + sym->st_name,
			       loc, val);
			val = lrsel(val, 0);
			*loc = mask(*loc, 21) | reassemble_21(val);
			break;
		case R_PARISC_LTOFF14R:
			
			
			val = get_got(me, val, addend);
			val = rrsel(val, 0);
			DEBUGP("LTOFF14R Symbol %s loc %p val %lx\n",
			       strtab + sym->st_name,
			       loc, val);
			*loc = mask(*loc, 14) | reassemble_14(val);
			break;
		case R_PARISC_PCREL22F:
			
			DEBUGP("PCREL22F Symbol %s loc %p val %lx\n",
			       strtab + sym->st_name,
			       loc, val);
			val += addend;
			
			if (in_local(me, (void *)val)) {
				val = (val - dot - 8)/4;
				if (!RELOC_REACHABLE(val, 22)) {
					val = get_stub(me, sym->st_value,
						addend, ELF_STUB_DIRECT,
						loc0, targetsec);
				} else {
					
					val = sym->st_value;
					val += addend;
				}
			} else {
				val = sym->st_value;
				if (strncmp(strtab + sym->st_name, "$$", 2)
				    == 0)
					val = get_stub(me, val, addend, ELF_STUB_MILLI,
						       loc0, targetsec);
				else
					val = get_stub(me, val, addend, ELF_STUB_GOT,
						       loc0, targetsec);
			}
			DEBUGP("STUB FOR %s loc %lx, val %lx+%lx at %lx\n", 
			       strtab + sym->st_name, loc, sym->st_value,
			       addend, val);
			val = (val - dot - 8)/4;
			CHECK_RELOC(val, 22);
			*loc = (*loc & ~0x3ff1ffd) | reassemble_22(val);
			break;
		case R_PARISC_DIR64:
			
			*loc64 = val + addend;
			break;
		case R_PARISC_SEGREL32:
			
			*loc = fsel(val, addend); 
			break;
		case R_PARISC_FPTR64:
			
			if(in_local(me, (void *)(val + addend))) {
				*loc64 = get_fdesc(me, val+addend);
				DEBUGP("FDESC for %s at %p points to %lx\n",
				       strtab + sym->st_name, *loc64,
				       ((Elf_Fdesc *)*loc64)->addr);
			} else {
				DEBUGP("Non local FPTR64 Symbol %s loc %p val %lx\n",
				       strtab + sym->st_name,
				       loc, val);
				*loc64 = val + addend;
			}
			break;

		default:
			printk(KERN_ERR "module %s: Unknown relocation: %Lu\n",
			       me->name, ELF64_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}
#endif

static void
register_unwind_table(struct module *me,
		      const Elf_Shdr *sechdrs)
{
	unsigned char *table, *end;
	unsigned long gp;

	if (!me->arch.unwind_section)
		return;

	table = (unsigned char *)sechdrs[me->arch.unwind_section].sh_addr;
	end = table + sechdrs[me->arch.unwind_section].sh_size;
	gp = (Elf_Addr)me->module_core + me->arch.got_offset;

	DEBUGP("register_unwind_table(), sect = %d at 0x%p - 0x%p (gp=0x%lx)\n",
	       me->arch.unwind_section, table, end, gp);
	me->arch.unwind = unwind_table_add(me->name, 0, gp, table, end);
}

static void
deregister_unwind_table(struct module *me)
{
	if (me->arch.unwind)
		unwind_table_remove(me->arch.unwind);
}

int module_finalize(const Elf_Ehdr *hdr,
		    const Elf_Shdr *sechdrs,
		    struct module *me)
{
	int i;
	unsigned long nsyms;
	const char *strtab = NULL;
	Elf_Sym *newptr, *oldptr;
	Elf_Shdr *symhdr = NULL;
#ifdef DEBUG
	Elf_Fdesc *entry;
	u32 *addr;

	entry = (Elf_Fdesc *)me->init;
	printk("FINALIZE, ->init FPTR is %p, GP %lx ADDR %lx\n", entry,
	       entry->gp, entry->addr);
	addr = (u32 *)entry->addr;
	printk("INSNS: %x %x %x %x\n",
	       addr[0], addr[1], addr[2], addr[3]);
	printk("got entries used %ld, gots max %ld\n"
	       "fdescs used %ld, fdescs max %ld\n",
	       me->arch.got_count, me->arch.got_max,
	       me->arch.fdesc_count, me->arch.fdesc_max);
#endif

	register_unwind_table(me, sechdrs);

	for (i = 1; i < hdr->e_shnum; i++) {
		if(sechdrs[i].sh_type == SHT_SYMTAB
		   && (sechdrs[i].sh_flags & SHF_ALLOC)) {
			int strindex = sechdrs[i].sh_link;
			symhdr = (Elf_Shdr *)&sechdrs[i];
			strtab = (char *)sechdrs[strindex].sh_addr;
			break;
		}
	}

	DEBUGP("module %s: strtab %p, symhdr %p\n",
	       me->name, strtab, symhdr);

	if(me->arch.got_count > MAX_GOTS) {
		printk(KERN_ERR "%s: Global Offset Table overflow (used %ld, allowed %d)\n",
				me->name, me->arch.got_count, MAX_GOTS);
		return -EINVAL;
	}

	kfree(me->arch.section);
	me->arch.section = NULL;

	
	if(symhdr == NULL)
		return 0;

	oldptr = (void *)symhdr->sh_addr;
	newptr = oldptr + 1;	
	nsyms = symhdr->sh_size / sizeof(Elf_Sym);
	DEBUGP("OLD num_symtab %lu\n", nsyms);

	for (i = 1; i < nsyms; i++) {
		oldptr++;	
		if(strncmp(strtab + oldptr->st_name,
			      ".L", 2) == 0)
			continue;

		if(newptr != oldptr)
			*newptr++ = *oldptr;
		else
			newptr++;

	}
	nsyms = newptr - (Elf_Sym *)symhdr->sh_addr;
	DEBUGP("NEW num_symtab %lu\n", nsyms);
	symhdr->sh_size = nsyms * sizeof(Elf_Sym);
	return 0;
}

void module_arch_cleanup(struct module *mod)
{
	deregister_unwind_table(mod);
}
