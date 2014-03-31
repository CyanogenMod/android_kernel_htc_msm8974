/*
 * include/asm-s390/kexec.h
 *
 * (C) Copyright IBM Corp. 2005
 *
 * Author(s): Rolf Adelsberger <adelsberger@de.ibm.com>
 *
 */

#ifndef _S390_KEXEC_H
#define _S390_KEXEC_H

#ifdef __KERNEL__
#include <asm/page.h>
#endif
#include <asm/processor.h>

#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)

#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)

#define KEXEC_CONTROL_MEMORY_LIMIT (1UL<<31)

#define KEXEC_CRASH_CONTROL_MEMORY_LIMIT (-1UL)

#define KEXEC_CONTROL_PAGE_SIZE 4096

#define KEXEC_CRASH_MEM_ALIGN HPAGE_SIZE

#define KEXEC_ARCH KEXEC_ARCH_S390

#define KEXEC_NOTE_BYTES \
	(ALIGN(sizeof(struct elf_note), 4) * 8 + \
	 ALIGN(sizeof("CORE"), 4) * 7 + \
	 ALIGN(sizeof(struct elf_prstatus), 4) + \
	 ALIGN(sizeof(elf_fpregset_t), 4) + \
	 ALIGN(sizeof(u64), 4) + \
	 ALIGN(sizeof(u64), 4) + \
	 ALIGN(sizeof(u32), 4) + \
	 ALIGN(sizeof(u64) * 16, 4) + \
	 ALIGN(sizeof(u32), 4) \
	)

static inline void crash_setup_regs(struct pt_regs *newregs,
					struct pt_regs *oldregs) { }

#endif 
