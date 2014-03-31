/* oplib.h:  Describes the interface and available routines in the
 *           Linux Prom library.
 *
 * Copyright (C) 1995, 2007 David S. Miller (davem@davemloft.net)
 * Copyright (C) 1996 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#ifndef __SPARC64_OPLIB_H
#define __SPARC64_OPLIB_H

#include <asm/openprom.h>

extern char prom_version[];

extern phandle prom_root_node;

extern int prom_stdout;

extern phandle prom_chosen_node;

extern const char prom_peer_name[];
extern const char prom_compatible_name[];
extern const char prom_root_compatible[];
extern const char prom_cpu_compatible[];
extern const char prom_finddev_name[];
extern const char prom_chosen_path[];
extern const char prom_cpu_path[];
extern const char prom_getprop_name[];
extern const char prom_mmu_name[];
extern const char prom_callmethod_name[];
extern const char prom_translate_name[];
extern const char prom_map_name[];
extern const char prom_unmap_name[];
extern int prom_mmu_ihandle_cache;
extern unsigned int prom_boot_mapped_pc;
extern unsigned int prom_boot_mapping_mode;
extern unsigned long prom_boot_mapping_phys_high, prom_boot_mapping_phys_low;

struct linux_mlist_p1275 {
	struct linux_mlist_p1275 *theres_more;
	unsigned long start_adr;
	unsigned long num_bytes;
};

struct linux_mem_p1275 {
	struct linux_mlist_p1275 **p1275_totphys;
	struct linux_mlist_p1275 **p1275_prommap;
	struct linux_mlist_p1275 **p1275_available; 
};


extern void prom_init(void *cif_handler, void *cif_stack);

extern char *prom_getbootargs(void);


extern void prom_reboot(const char *boot_command);

extern void prom_feval(const char *forth_string);

extern void prom_cmdline(void);

extern void prom_halt(void) __attribute__ ((noreturn));

extern void prom_halt_power_off(void) __attribute__ ((noreturn));

extern unsigned char prom_get_idprom(char *idp_buffer, int idpbuf_size);

extern void prom_console_write_buf(const char *buf, int len);

extern void prom_printf(const char *fmt, ...);
extern void prom_write(const char *buf, unsigned int len);

#ifdef CONFIG_SMP
extern void prom_startcpu(int cpunode, unsigned long pc, unsigned long arg);

extern void prom_startcpu_cpuid(int cpuid, unsigned long pc, unsigned long arg);

extern void prom_stopcpu_cpuid(int cpuid);

extern void prom_stopself(void);

extern void prom_idleself(void);

extern void prom_resumecpu(int cpunode);
#endif


extern void prom_sleepself(void);

extern int prom_sleepsystem(void);

extern int prom_wakeupsystem(void);


extern int prom_getunumber(int syndrome_code,
			   unsigned long phys_addr,
			   char *buf, int buflen);

extern int prom_retain(const char *name, unsigned long size,
		       unsigned long align, unsigned long *paddr);

extern long prom_itlb_load(unsigned long index,
			   unsigned long tte_data,
			   unsigned long vaddr);

extern long prom_dtlb_load(unsigned long index,
			   unsigned long tte_data,
			   unsigned long vaddr);

#define PROM_MAP_WRITE	0x0001 
#define PROM_MAP_READ	0x0002 
#define PROM_MAP_EXEC	0x0004 
#define PROM_MAP_LOCKED	0x0010 
#define PROM_MAP_CACHED	0x0020 
#define PROM_MAP_SE	0x0040 
#define PROM_MAP_GLOB	0x0080 
#define PROM_MAP_IE	0x0100 
#define PROM_MAP_DEFAULT (PROM_MAP_WRITE | PROM_MAP_READ | PROM_MAP_EXEC | PROM_MAP_CACHED)

extern int prom_map(int mode, unsigned long size,
		    unsigned long vaddr, unsigned long paddr);
extern void prom_unmap(unsigned long size, unsigned long vaddr);



extern phandle prom_getchild(phandle parent_node);

extern phandle prom_getsibling(phandle node);

extern int prom_getproplen(phandle thisnode, const char *property);

extern int prom_getproperty(phandle thisnode, const char *property,
			    char *prop_buffer, int propbuf_size);

extern int prom_getint(phandle node, const char *property);

extern int prom_getintdefault(phandle node, const char *property, int defval);

extern int prom_getbool(phandle node, const char *prop);

extern void prom_getstring(phandle node, const char *prop, char *buf,
			   int bufsize);

extern int prom_nodematch(phandle thisnode, const char *name);

extern phandle prom_searchsiblings(phandle node_start, const char *name);

extern char *prom_firstprop(phandle node, char *buffer);

extern char *prom_nextprop(phandle node, const char *prev_property, char *buf);

extern int prom_node_has_property(phandle node, const char *property);

extern phandle prom_finddevice(const char *name);

extern int prom_setprop(phandle node, const char *prop_name, char *prop_value,
			int value_size);

extern phandle prom_inst2pkg(int);
extern void prom_sun4v_guest_soft_state(void);

extern int prom_ihandle2path(int handle, char *buffer, int bufsize);

extern void p1275_cmd_direct(unsigned long *);

#endif 
