/*
 * oplib.h:  Describes the interface and available routines in the
 *           Linux Prom library.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC_OPLIB_H
#define __SPARC_OPLIB_H

#include <asm/openprom.h>

extern struct linux_romvec *romvec;

enum prom_major_version {
	PROM_V0,      
	PROM_V2,      
	PROM_V3,      
	PROM_P1275,   
};

extern enum prom_major_version prom_vers;
extern unsigned int prom_rev, prom_prev;

extern int prom_root_node;

extern struct linux_nodeops *prom_nodeops;


extern void prom_init(struct linux_romvec *rom_ptr);

extern char *prom_getbootargs(void);


extern char *prom_mapio(char *virt_hint, int io_space, unsigned int phys_addr, unsigned int num_bytes);
extern void prom_unmapio(char *virt_addr, unsigned int num_bytes);


extern int prom_devopen(char *device_string);

extern int prom_devclose(int device_handle);

extern void prom_seek(int device_handle, unsigned int seek_hival,
		      unsigned int seek_lowval);


extern struct linux_mem_v0 *prom_meminfo(void);


extern void prom_reboot(char *boot_command);

extern void prom_feval(char *forth_string);

extern void prom_cmdline(void);

extern void prom_halt(void);

typedef void (*sync_func_t)(void);
extern void prom_setsync(sync_func_t func_ptr);

extern unsigned char prom_get_idprom(char *idp_buffer, int idpbuf_size);

extern int prom_version(void);

extern int prom_getrev(void);

extern int prom_getprev(void);


extern int prom_nbgetchar(void);

extern int prom_nbputchar(char character);

extern char prom_getchar(void);

extern void prom_putchar(char character);

void prom_printf(char *fmt, ...);


enum prom_input_device {
	PROMDEV_IKBD,			
	PROMDEV_ITTYA,			
	PROMDEV_ITTYB,			
	PROMDEV_I_UNK,
};

extern enum prom_input_device prom_query_input_device(void);


enum prom_output_device {
	PROMDEV_OSCREEN,		
	PROMDEV_OTTYA,			
	PROMDEV_OTTYB,			
	PROMDEV_O_UNK,
};

extern enum prom_output_device prom_query_output_device(void);


extern int prom_startcpu(int cpunode, struct linux_prom_registers *context_table,
			 int context, char *program_counter);

extern int prom_stopcpu(int cpunode);

extern int prom_idlecpu(int cpunode);

extern int prom_restartcpu(int cpunode);


extern char *prom_alloc(char *virt_hint, unsigned int size);

extern void prom_free(char *virt_addr, unsigned int size);


extern void prom_putsegment(int context, unsigned long virt_addr,
			    int physical_segment);


extern int prom_getchild(int parent_node);

extern int prom_getsibling(int node);

extern int prom_getproplen(int thisnode, char *property);

extern int prom_getproperty(int thisnode, char *property,
			    char *prop_buffer, int propbuf_size);

extern int prom_getint(int node, char *property);

extern int prom_getintdefault(int node, char *property, int defval);

extern int prom_getbool(int node, char *prop);

extern void prom_getstring(int node, char *prop, char *buf, int bufsize);

extern int prom_nodematch(int thisnode, char *name);

extern int prom_searchsiblings(int node_start, char *name);

extern char *prom_firstprop(int node);

extern char *prom_nextprop(int node, char *prev_property);

extern int prom_node_has_property(int node, char *property);

extern int prom_setprop(int node, char *prop_name, char *prop_value,
			int value_size);

extern int prom_pathtoinode(char *path);
extern int prom_inst2pkg(int);


extern void prom_adjust_regs(struct linux_prom_registers *regp, int nregs,
			     struct linux_prom_ranges *rangep, int nranges);

extern void prom_adjust_ranges(struct linux_prom_ranges *cranges, int ncranges,
			       struct linux_prom_ranges *pranges, int npranges);

extern void prom_apply_obio_ranges(struct linux_prom_registers *obioregs, int nregs);

extern void prom_apply_generic_ranges(int node, int parent,
				      struct linux_prom_registers *sbusregs, int nregs);


#endif 
