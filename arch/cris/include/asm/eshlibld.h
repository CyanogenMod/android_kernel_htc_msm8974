/*!**************************************************************************
*!
*! FILE NAME  : eshlibld.h
*!
*! DESCRIPTION: Prototypes for exported shared library functions
*!
*! FUNCTIONS  : perform_cris_aout_relocations, shlibmod_fork, shlibmod_exit
*! (EXPORTED)
*!
*!---------------------------------------------------------------------------
*!
*! (C) Copyright 1998, 1999 Axis Communications AB, LUND, SWEDEN
*!
*!**************************************************************************/

#ifndef _cris_relocate_h
#define _cris_relocate_h


#include <linux/limits.h>

#undef SANITYCHECK_RELOC

#undef RELOC_DEBUG

#ifndef SHARE_LIB_CORE
# if (defined(__KERNEL__) || !defined(RELOC_DEBUG)) \
     && !defined(CONFIG_SHARE_SHLIB_CORE)
#  define SHARE_LIB_CORE 0
# else
#  define SHARE_LIB_CORE 1
# endif 
#endif 


extern int
perform_cris_aout_relocations(unsigned long text, unsigned long tlength,
			      unsigned long data, unsigned long dlength,
			      unsigned long baddr, unsigned long blength,

			      unsigned char *trel, unsigned long tsrel,
			      unsigned long dsrel,

			      unsigned char *symbols, unsigned long symlength,
			      unsigned char *strings, unsigned long stringlength,

			      char **env, int envc,
			      int euid, int is_suid);


#ifdef RELOC_DEBUG
struct task_reloc_debug {
	struct memdebug *alloclast;
	unsigned long alloc_total;
	unsigned long export_total;
};
#endif 

#if SHARE_LIB_CORE


struct shlibdep;

extern void
shlibmod_exit(struct shlibdep **deps);

extern int
shlibmod_fork(struct shlibdep **deps);

#else  
# define shlibmod_exit(x)
# define shlibmod_fork(x) 1
#endif 

#endif _cris_relocate_h

