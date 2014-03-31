#ifndef _ASM_X86_BOOT_H
#define _ASM_X86_BOOT_H

#define NORMAL_VGA	0xffff		
#define EXTENDED_VGA	0xfffe		
#define ASK_VGA		0xfffd		

#ifdef __KERNEL__

#include <asm/pgtable_types.h>

#define LOAD_PHYSICAL_ADDR ((CONFIG_PHYSICAL_START \
				+ (CONFIG_PHYSICAL_ALIGN - 1)) \
				& ~(CONFIG_PHYSICAL_ALIGN - 1))

#ifdef CONFIG_X86_64
#define MIN_KERNEL_ALIGN_LG2	PMD_SHIFT
#else
#define MIN_KERNEL_ALIGN_LG2	(PAGE_SHIFT + THREAD_ORDER)
#endif
#define MIN_KERNEL_ALIGN	(_AC(1, UL) << MIN_KERNEL_ALIGN_LG2)

#if (CONFIG_PHYSICAL_ALIGN & (CONFIG_PHYSICAL_ALIGN-1)) || \
	(CONFIG_PHYSICAL_ALIGN < MIN_KERNEL_ALIGN)
#error "Invalid value for CONFIG_PHYSICAL_ALIGN"
#endif

#ifdef CONFIG_KERNEL_BZIP2
#define BOOT_HEAP_SIZE             0x400000
#else 

#define BOOT_HEAP_SIZE	0x8000

#endif 

#ifdef CONFIG_X86_64
#define BOOT_STACK_SIZE	0x4000
#else
#define BOOT_STACK_SIZE	0x1000
#endif

#endif 

#endif 
