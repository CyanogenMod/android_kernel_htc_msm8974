#ifndef __M68K_A_OUT_H__
#define __M68K_A_OUT_H__

struct exec
{
  unsigned long a_info;		
  unsigned a_text;		
  unsigned a_data;		
  unsigned a_bss;		
  unsigned a_syms;		
  unsigned a_entry;		
  unsigned a_trsize;		
  unsigned a_drsize;		
};

#define N_TRSIZE(a)	((a).a_trsize)
#define N_DRSIZE(a)	((a).a_drsize)
#define N_SYMSIZE(a)	((a).a_syms)

#endif 
