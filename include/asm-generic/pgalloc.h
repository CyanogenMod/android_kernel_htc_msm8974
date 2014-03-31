#ifndef __ASM_GENERIC_PGALLOC_H
#define __ASM_GENERIC_PGALLOC_H
#ifdef CONFIG_MMU
#error need to implement an architecture specific asm/pgalloc.h
#endif

#define check_pgt_cache()          do { } while (0)

#endif 
