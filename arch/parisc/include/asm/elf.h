#ifndef __ASMPARISC_ELF_H
#define __ASMPARISC_ELF_H


#include <asm/ptrace.h>

#define EM_PARISC 15



#define EF_PARISC_TRAPNIL	0x00010000 
#define EF_PARISC_EXT		0x00020000 
#define EF_PARISC_LSB		0x00040000 
#define EF_PARISC_WIDE		0x00080000 
#define EF_PARISC_NO_KABP	0x00100000 
#define EF_PARISC_LAZYSWAP	0x00400000 
#define EF_PARISC_ARCH		0x0000ffff 


#define EFA_PARISC_1_0		    0x020b 
#define EFA_PARISC_1_1		    0x0210 
#define EFA_PARISC_2_0		    0x0214 


#define SHN_PARISC_ANSI_COMMON	0xff00	   
#define SHN_PARISC_HUGE_COMMON	0xff01	   


#define SHT_PARISC_EXT		0x70000000 
#define SHT_PARISC_UNWIND	0x70000001 
#define SHT_PARISC_DOC		0x70000002 


#define SHF_PARISC_SHORT	0x20000000 
#define SHF_PARISC_HUGE		0x40000000 
#define SHF_PARISC_SBP		0x80000000 


#define STT_PARISC_MILLICODE	13	

#define STT_HP_OPAQUE		(STT_LOOS + 0x1)
#define STT_HP_STUB		(STT_LOOS + 0x2)


#define R_PARISC_NONE		0	
#define R_PARISC_DIR32		1	
#define R_PARISC_DIR21L		2	
#define R_PARISC_DIR17R		3	
#define R_PARISC_DIR17F		4	
#define R_PARISC_DIR14R		6	
#define R_PARISC_PCREL32	9	
#define R_PARISC_PCREL21L	10	
#define R_PARISC_PCREL17R	11	
#define R_PARISC_PCREL17F	12	
#define R_PARISC_PCREL14R	14	
#define R_PARISC_DPREL21L	18	
#define R_PARISC_DPREL14R	22	
#define R_PARISC_GPREL21L	26	
#define R_PARISC_GPREL14R	30	
#define R_PARISC_LTOFF21L	34	
#define R_PARISC_LTOFF14R	38	
#define R_PARISC_SECREL32	41	
#define R_PARISC_SEGBASE	48	
#define R_PARISC_SEGREL32	49	
#define R_PARISC_PLTOFF21L	50	
#define R_PARISC_PLTOFF14R	54	
#define R_PARISC_LTOFF_FPTR32	57	
#define R_PARISC_LTOFF_FPTR21L	58	
#define R_PARISC_LTOFF_FPTR14R	62	
#define R_PARISC_FPTR64		64	
#define R_PARISC_PLABEL32	65	
#define R_PARISC_PCREL64	72	
#define R_PARISC_PCREL22F	74	
#define R_PARISC_PCREL14WR	75	
#define R_PARISC_PCREL14DR	76	
#define R_PARISC_PCREL16F	77	
#define R_PARISC_PCREL16WF	78	
#define R_PARISC_PCREL16DF	79	
#define R_PARISC_DIR64		80	
#define R_PARISC_DIR14WR	83	
#define R_PARISC_DIR14DR	84	
#define R_PARISC_DIR16F		85	
#define R_PARISC_DIR16WF	86	
#define R_PARISC_DIR16DF	87	
#define R_PARISC_GPREL64	88	
#define R_PARISC_GPREL14WR	91	
#define R_PARISC_GPREL14DR	92	
#define R_PARISC_GPREL16F	93	
#define R_PARISC_GPREL16WF	94	
#define R_PARISC_GPREL16DF	95	
#define R_PARISC_LTOFF64	96	
#define R_PARISC_LTOFF14WR	99	
#define R_PARISC_LTOFF14DR	100	
#define R_PARISC_LTOFF16F	101	
#define R_PARISC_LTOFF16WF	102	
#define R_PARISC_LTOFF16DF	103	
#define R_PARISC_SECREL64	104	
#define R_PARISC_SEGREL64	112	
#define R_PARISC_PLTOFF14WR	115	
#define R_PARISC_PLTOFF14DR	116	
#define R_PARISC_PLTOFF16F	117	
#define R_PARISC_PLTOFF16WF	118	
#define R_PARISC_PLTOFF16DF	119	
#define R_PARISC_LTOFF_FPTR64	120	
#define R_PARISC_LTOFF_FPTR14WR	123	
#define R_PARISC_LTOFF_FPTR14DR	124	
#define R_PARISC_LTOFF_FPTR16F	125	
#define R_PARISC_LTOFF_FPTR16WF	126	
#define R_PARISC_LTOFF_FPTR16DF	127	
#define R_PARISC_LORESERVE	128
#define R_PARISC_COPY		128	
#define R_PARISC_IPLT		129	
#define R_PARISC_EPLT		130	
#define R_PARISC_TPREL32	153	
#define R_PARISC_TPREL21L	154	
#define R_PARISC_TPREL14R	158	
#define R_PARISC_LTOFF_TP21L	162	
#define R_PARISC_LTOFF_TP14R	166	
#define R_PARISC_LTOFF_TP14F	167	
#define R_PARISC_TPREL64	216	
#define R_PARISC_TPREL14WR	219	
#define R_PARISC_TPREL14DR	220	
#define R_PARISC_TPREL16F	221	
#define R_PARISC_TPREL16WF	222	
#define R_PARISC_TPREL16DF	223	
#define R_PARISC_LTOFF_TP64	224	
#define R_PARISC_LTOFF_TP14WR	227	
#define R_PARISC_LTOFF_TP14DR	228	
#define R_PARISC_LTOFF_TP16F	229	
#define R_PARISC_LTOFF_TP16WF	230	
#define R_PARISC_LTOFF_TP16DF	231	
#define R_PARISC_HIRESERVE	255

#define PA_PLABEL_FDESC		0x02	


typedef struct elf32_fdesc {
	__u32	addr;
	__u32	gp;
} Elf32_Fdesc;

typedef struct elf64_fdesc {
	__u64	dummy[2]; 
	__u64	addr;
	__u64	gp;
} Elf64_Fdesc;

#ifdef __KERNEL__

#ifdef CONFIG_64BIT
#define Elf_Fdesc	Elf64_Fdesc
#else
#define Elf_Fdesc	Elf32_Fdesc
#endif 

#endif 


#define PT_HP_TLS		(PT_LOOS + 0x0)
#define PT_HP_CORE_NONE		(PT_LOOS + 0x1)
#define PT_HP_CORE_VERSION	(PT_LOOS + 0x2)
#define PT_HP_CORE_KERNEL	(PT_LOOS + 0x3)
#define PT_HP_CORE_COMM		(PT_LOOS + 0x4)
#define PT_HP_CORE_PROC		(PT_LOOS + 0x5)
#define PT_HP_CORE_LOADABLE	(PT_LOOS + 0x6)
#define PT_HP_CORE_STACK	(PT_LOOS + 0x7)
#define PT_HP_CORE_SHM		(PT_LOOS + 0x8)
#define PT_HP_CORE_MMF		(PT_LOOS + 0x9)
#define PT_HP_PARALLEL		(PT_LOOS + 0x10)
#define PT_HP_FASTBIND		(PT_LOOS + 0x11)
#define PT_HP_OPT_ANNOT		(PT_LOOS + 0x12)
#define PT_HP_HSL_ANNOT		(PT_LOOS + 0x13)
#define PT_HP_STACK		(PT_LOOS + 0x14)

#define PT_PARISC_ARCHEXT	0x70000000
#define PT_PARISC_UNWIND	0x70000001


#define PF_PARISC_SBP		0x08000000

#define PF_HP_PAGE_SIZE		0x00100000
#define PF_HP_FAR_SHARED	0x00200000
#define PF_HP_NEAR_SHARED	0x00400000
#define PF_HP_CODE		0x01000000
#define PF_HP_MODIFY		0x02000000
#define PF_HP_LAZYSWAP		0x04000000
#define PF_HP_SBP		0x08000000

#ifndef ELF_CLASS

#ifdef CONFIG_64BIT
#define ELF_CLASS   ELFCLASS64
#else
#define ELF_CLASS	ELFCLASS32
#endif

typedef unsigned long elf_greg_t;


#define ELF_PLATFORM  ("PARISC\0")

#define SET_PERSONALITY(ex) \
	current->personality = PER_LINUX; \
	current->thread.map_base = DEFAULT_MAP_BASE; \
	current->thread.task_size = DEFAULT_TASK_SIZE \


#define ELF_CORE_COPY_REGS(dst, pt)	\
	memset(dst, 0, sizeof(dst));	 \
	memcpy(dst + 0, pt->gr, 32 * sizeof(elf_greg_t)); \
	memcpy(dst + 32, pt->sr, 8 * sizeof(elf_greg_t)); \
	memcpy(dst + 40, pt->iaoq, 2 * sizeof(elf_greg_t)); \
	memcpy(dst + 42, pt->iasq, 2 * sizeof(elf_greg_t)); \
	dst[44] = pt->sar;   dst[45] = pt->iir; \
	dst[46] = pt->isr;   dst[47] = pt->ior; \
	dst[48] = mfctl(22); dst[49] = mfctl(0); \
	dst[50] = mfctl(24); dst[51] = mfctl(25); \
	dst[52] = mfctl(26); dst[53] = mfctl(27); \
	dst[54] = mfctl(28); dst[55] = mfctl(29); \
	dst[56] = mfctl(30); dst[57] = mfctl(31); \
	dst[58] = mfctl( 8); dst[59] = mfctl( 9); \
	dst[60] = mfctl(12); dst[61] = mfctl(13); \
	dst[62] = mfctl(10); dst[63] = mfctl(15);

#endif 

#define ELF_NGREG 80	
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

#define ELF_NFPREG 32
typedef double elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

struct task_struct;

extern int dump_task_fpu (struct task_struct *, elf_fpregset_t *);
#define ELF_CORE_COPY_FPREGS(tsk, elf_fpregs) dump_task_fpu(tsk, elf_fpregs)

struct pt_regs;	


#define elf_check_arch(x) ((x)->e_machine == EM_PARISC && (x)->e_ident[EI_CLASS] == ELF_CLASS)

#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_PARISC
#define ELF_OSABI 	ELFOSABI_LINUX

#define ELF_PLAT_INIT(_r, load_addr)       _r->gr[23] = 0

#define ELF_EXEC_PAGESIZE	4096


#define ELF_ET_DYN_BASE         (TASK_UNMAPPED_BASE + 0x01000000)


#define ELF_HWCAP	0

#endif
