/*
 * oplib.h:  Describes the interface and available routines in the
 *           Linux Prom library.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC_OPLIB_H
#define __SPARC_OPLIB_H

#include <asm/openprom.h>
#include <linux/spinlock.h>
#include <linux/compiler.h>

extern struct linux_romvec *romvec;

enum prom_major_version {
	PROM_V0,      
	PROM_V2,      
	PROM_V3,      
	PROM_P1275,   
};

extern enum prom_major_version prom_vers;
extern unsigned int prom_rev, prom_prev;

extern phandle prom_root_node;

extern struct linux_nodeops *prom_nodeops;


extern void prom_init(struct linux_romvec *rom_ptr);

extern char *prom_getbootargs(void);


extern void prom_reboot(char *boot_command);

extern void prom_feval(char *forth_string);

extern void prom_cmdline(void);

extern void __noreturn prom_halt(void);

typedef void (*sync_func_t)(void);
extern void prom_setsync(sync_func_t func_ptr);

extern unsigned char prom_get_idprom(char *idp_buffer, int idpbuf_size);

extern int prom_version(void);

extern int prom_getrev(void);

extern int prom_getprev(void);

extern void prom_console_write_buf(const char *buf, int len);

extern void prom_printf(const char *fmt, ...);
extern void prom_write(const char *buf, unsigned int len);


extern int prom_startcpu(int cpunode, struct linux_prom_registers *context_table,
			 int context, char *program_counter);


extern void prom_putsegment(int context, unsigned long virt_addr,
			    int physical_segment);

void prom_meminit(void);


extern phandle prom_getchild(phandle parent_node);

extern phandle prom_getsibling(phandle node);

extern int prom_getproplen(phandle thisnode, const char *property);

extern int __must_check prom_getproperty(phandle thisnode, const char *property,
					 char *prop_buffer, int propbuf_size);

extern int prom_getint(phandle node, char *property);

extern int prom_getintdefault(phandle node, char *property, int defval);

extern int prom_getbool(phandle node, char *prop);

extern void prom_getstring(phandle node, char *prop, char *buf, int bufsize);

extern phandle prom_searchsiblings(phandle node_start, char *name);

extern char *prom_nextprop(phandle node, char *prev_property, char *buffer);

extern phandle prom_finddevice(char *name);

extern int prom_setprop(phandle node, const char *prop_name, char *prop_value,
			int value_size);

extern phandle prom_inst2pkg(int);


extern void prom_apply_obio_ranges(struct linux_prom_registers *obioregs, int nregs);

extern void prom_apply_generic_ranges(phandle node, phandle parent,
				      struct linux_prom_registers *sbusregs, int nregs);

void prom_ranges_init(void);

int cpu_find_by_instance(int instance, phandle *prom_node, int *mid);
int cpu_find_by_mid(int mid, phandle *prom_node);
int cpu_get_hwmid(phandle prom_node);

extern spinlock_t prom_lock;

#endif 
