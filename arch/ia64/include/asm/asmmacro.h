#ifndef _ASM_IA64_ASMMACRO_H
#define _ASM_IA64_ASMMACRO_H

/*
 * Copyright (C) 2000-2001, 2003-2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */


#define ENTRY(name)				\
	.align 32;				\
	.proc name;				\
name:

#define ENTRY_MIN_ALIGN(name)			\
	.align 16;				\
	.proc name;				\
name:

#define GLOBAL_ENTRY(name)			\
	.global name;				\
	ENTRY(name)

#define END(name)				\
	.endp name


#define ASM_UNW_PRLG_RP			0x8
#define ASM_UNW_PRLG_PFS		0x4
#define ASM_UNW_PRLG_PSP		0x2
#define ASM_UNW_PRLG_PR			0x1
#define ASM_UNW_PRLG_GRSAVE(ninputs)	(32+(ninputs))


	.section "__ex_table", "a"		
	.previous

# define EX(y,x...)				\
	.xdata4 "__ex_table", 99f-., y-.;	\
  [99:]	x
# define EXCLR(y,x...)				\
	.xdata4 "__ex_table", 99f-., y-.+4;	\
  [99:]	x


	.section "__mca_table", "a"		
	.previous

# define MCA_RECOVER_RANGE(y)			\
	.xdata4 "__mca_table", y-., 99f-.;	\
  [99:]

	.section ".data..patch.vtop", "a"	
	.previous

#define	LOAD_PHYSICAL(pr, reg, obj)		\
[1:](pr)movl reg = obj;				\
	.xdata4 ".data..patch.vtop", 1b-.

#define DO_MCKINLEY_E9_WORKAROUND

#ifdef DO_MCKINLEY_E9_WORKAROUND
	.section ".data..patch.mckinley_e9", "a"
	.previous
# define FSYS_RETURN					\
	.xdata4 ".data..patch.mckinley_e9", 1f-.;	\
1:{ .mib;						\
	nop.m 0;					\
	mov r16=ar.pfs;					\
	br.call.sptk.many b7=2f;;			\
  };							\
2:{ .mib;						\
	nop.m 0;					\
	mov ar.pfs=r16;					\
	br.ret.sptk.many b6;;				\
  }
#else
# define FSYS_RETURN	br.ret.sptk.many b6
#endif

	.section ".data..patch.phys_stack_reg", "a"
	.previous
#define LOAD_PHYS_STACK_REG_SIZE(reg)			\
[1:]	adds reg=IA64_NUM_PHYS_STACK_REG*8+8,r0;	\
	.xdata4 ".data..patch.phys_stack_reg", 1b-.

#ifdef HAVE_WORKING_TEXT_ALIGN
# define TEXT_ALIGN(n)	.align n
#else
# define TEXT_ALIGN(n)
#endif

#ifdef HAVE_SERIALIZE_DIRECTIVE
# define dv_serialize_data		.serialize.data
# define dv_serialize_instruction	.serialize.instruction
#else
# define dv_serialize_data
# define dv_serialize_instruction
#endif

#endif 
