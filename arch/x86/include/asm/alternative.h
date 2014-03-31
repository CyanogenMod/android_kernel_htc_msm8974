#ifndef _ASM_X86_ALTERNATIVE_H
#define _ASM_X86_ALTERNATIVE_H

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/stringify.h>
#include <asm/asm.h>


#ifdef CONFIG_SMP
#define LOCK_PREFIX_HERE \
		".section .smp_locks,\"a\"\n"	\
		".balign 4\n"			\
		".long 671f - .\n" 	\
		".previous\n"			\
		"671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "

#else 
#define LOCK_PREFIX_HERE ""
#define LOCK_PREFIX ""
#endif

struct alt_instr {
	s32 instr_offset;	
	s32 repl_offset;	
	u16 cpuid;		
	u8  instrlen;		
	u8  replacementlen;	
};

extern void alternative_instructions(void);
extern void apply_alternatives(struct alt_instr *start, struct alt_instr *end);

struct module;

#ifdef CONFIG_SMP
extern void alternatives_smp_module_add(struct module *mod, char *name,
					void *locks, void *locks_end,
					void *text, void *text_end);
extern void alternatives_smp_module_del(struct module *mod);
extern void alternatives_smp_switch(int smp);
extern int alternatives_text_reserved(void *start, void *end);
extern bool skip_smp_alternatives;
#else
static inline void alternatives_smp_module_add(struct module *mod, char *name,
					       void *locks, void *locks_end,
					       void *text, void *text_end) {}
static inline void alternatives_smp_module_del(struct module *mod) {}
static inline void alternatives_smp_switch(int smp) {}
static inline int alternatives_text_reserved(void *start, void *end)
{
	return 0;
}
#endif	

#define ALTERNATIVE(oldinstr, newinstr, feature)			\
									\
      "661:\n\t" oldinstr "\n662:\n"					\
      ".section .altinstructions,\"a\"\n"				\
      "	 .long 661b - .\n"				\
      "	 .long 663f - .\n"				\
      "	 .word " __stringify(feature) "\n"		\
      "	 .byte 662b-661b\n"				\
      "	 .byte 664f-663f\n"				\
      ".previous\n"							\
      ".section .discard,\"aw\",@progbits\n"				\
      "	 .byte 0xff + (664f-663f) - (662b-661b)\n" 	\
      ".previous\n"							\
      ".section .altinstr_replacement, \"ax\"\n"			\
      "663:\n\t" newinstr "\n664:\n"			\
      ".previous"

#include <asm/cpufeature.h>

#define alternative(oldinstr, newinstr, feature)			\
	asm volatile (ALTERNATIVE(oldinstr, newinstr, feature) : : : "memory")

#define alternative_input(oldinstr, newinstr, feature, input...)	\
	asm volatile (ALTERNATIVE(oldinstr, newinstr, feature)		\
		: : "i" (0), ## input)

#define alternative_io(oldinstr, newinstr, feature, output, input...)	\
	asm volatile (ALTERNATIVE(oldinstr, newinstr, feature)		\
		: output : "i" (0), ## input)

#define alternative_call(oldfunc, newfunc, feature, output, input...)	\
	asm volatile (ALTERNATIVE("call %P[old]", "call %P[new]", feature) \
		: output : [old] "i" (oldfunc), [new] "i" (newfunc), ## input)

#define ASM_OUTPUT2(a...) a

#define ASM_NO_INPUT_CLOBBER(clbr...) "i" (0) : clbr

struct paravirt_patch_site;
#ifdef CONFIG_PARAVIRT
void apply_paravirt(struct paravirt_patch_site *start,
		    struct paravirt_patch_site *end);
#else
static inline void apply_paravirt(struct paravirt_patch_site *start,
				  struct paravirt_patch_site *end)
{}
#define __parainstructions	NULL
#define __parainstructions_end	NULL
#endif

extern void *text_poke_early(void *addr, const void *opcode, size_t len);

struct text_poke_param {
	void *addr;
	const void *opcode;
	size_t len;
};

extern void *text_poke(void *addr, const void *opcode, size_t len);
extern void *text_poke_smp(void *addr, const void *opcode, size_t len);
extern void text_poke_smp_batch(struct text_poke_param *params, int n);

#endif 
