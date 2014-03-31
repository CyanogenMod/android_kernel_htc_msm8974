#ifndef __ASM_SH_SEGMENT_H
#define __ASM_SH_SEGMENT_H

#ifndef __ASSEMBLY__

typedef struct {
	unsigned long seg;
} mm_segment_t;

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })

#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFFUL)
#ifdef CONFIG_MMU
#define USER_DS		MAKE_MM_SEG(PAGE_OFFSET)
#else
#define USER_DS		KERNEL_DS
#endif

#define segment_eq(a,b)	((a).seg == (b).seg)

#define get_ds()	(KERNEL_DS)

#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#endif 
#endif 
