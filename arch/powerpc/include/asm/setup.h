#ifndef _ASM_POWERPC_SETUP_H
#define _ASM_POWERPC_SETUP_H

#include <asm-generic/setup.h>

#ifndef __ASSEMBLY__
extern void ppc_printk_progress(char *s, unsigned short hex);

extern unsigned int rtas_data;
extern int mem_init_done;	
extern int init_bootmem_done;	
extern phys_addr_t memory_limit;
extern unsigned long klimit;
extern void *zalloc_maybe_bootmem(size_t size, gfp_t mask);

extern void via_cuda_init(void);
extern void read_rtc_time(void);
extern void pmac_find_display(void);

struct device_node;
extern void note_scsi_host(struct device_node *, void *);

extern unsigned long reloc_offset(void);
extern unsigned long add_reloc_offset(unsigned long);
extern void reloc_got2(unsigned long);

#define PTRRELOC(x)	((typeof(x)) add_reloc_offset((unsigned long)(x)))

#endif 

#endif	
