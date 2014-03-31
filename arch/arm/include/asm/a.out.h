#ifndef __ARM_A_OUT_H__
#define __ARM_A_OUT_H__

#include <linux/personality.h>
#include <linux/types.h>

struct exec
{
  __u32 a_info;		
  __u32 a_text;		
  __u32 a_data;		
  __u32 a_bss;		
  __u32 a_syms;		
  __u32 a_entry;	
  __u32 a_trsize;	
  __u32 a_drsize;	
};

#define N_TXTADDR(a)	(0x00008000)

#define N_TRSIZE(a)	((a).a_trsize)
#define N_DRSIZE(a)	((a).a_drsize)
#define N_SYMSIZE(a)	((a).a_syms)

#define M_ARM 103

#ifndef LIBRARY_START_TEXT
#define LIBRARY_START_TEXT	(0x00c00000)
#endif

#endif 
