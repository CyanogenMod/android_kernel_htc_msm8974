#ifndef _ASM_S390_MODULE_H
#define _ASM_S390_MODULE_H

struct mod_arch_syminfo
{
	unsigned long got_offset;
	unsigned long plt_offset;
	int got_initialized;
	int plt_initialized;
};

struct mod_arch_specific
{
	
	unsigned long got_offset;
	
	unsigned long plt_offset;
	
	unsigned long got_size;
	
	unsigned long plt_size;
	
	int nsyms;
	
	struct mod_arch_syminfo *syminfo;
};

#ifdef __s390x__
#define ElfW(x) Elf64_ ## x
#define ELFW(x) ELF64_ ## x
#else
#define ElfW(x) Elf32_ ## x
#define ELFW(x) ELF32_ ## x
#endif

#define Elf_Addr ElfW(Addr)
#define Elf_Rela ElfW(Rela)
#define Elf_Shdr ElfW(Shdr)
#define Elf_Sym ElfW(Sym)
#define Elf_Ehdr ElfW(Ehdr)
#define ELF_R_SYM ELFW(R_SYM)
#define ELF_R_TYPE ELFW(R_TYPE)
#endif 
