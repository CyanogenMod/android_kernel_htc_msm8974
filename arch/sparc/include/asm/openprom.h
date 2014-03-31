#ifndef __SPARC_OPENPROM_H
#define __SPARC_OPENPROM_H

/* openprom.h:  Prom structures and defines for access to the OPENBOOT
 *              prom routines and data areas.
 *
 * Copyright (C) 1995,1996 David S. Miller (davem@caip.rutgers.edu)
 */

#define LINUX_OPPROM_MAGIC      0x10010407

#ifndef __ASSEMBLY__
#include <linux/of.h>

struct linux_dev_v0_funcs {
	int (*v0_devopen)(char *device_str);
	int (*v0_devclose)(int dev_desc);
	int (*v0_rdblkdev)(int dev_desc, int num_blks, int blk_st, char *buf);
	int (*v0_wrblkdev)(int dev_desc, int num_blks, int blk_st, char *buf);
	int (*v0_wrnetdev)(int dev_desc, int num_bytes, char *buf);
	int (*v0_rdnetdev)(int dev_desc, int num_bytes, char *buf);
	int (*v0_rdchardev)(int dev_desc, int num_bytes, int dummy, char *buf);
	int (*v0_wrchardev)(int dev_desc, int num_bytes, int dummy, char *buf);
	int (*v0_seekdev)(int dev_desc, long logical_offst, int from);
};

struct linux_dev_v2_funcs {
	phandle (*v2_inst2pkg)(int d);	
	char * (*v2_dumb_mem_alloc)(char *va, unsigned sz);
	void (*v2_dumb_mem_free)(char *va, unsigned sz);

	
	char * (*v2_dumb_mmap)(char *virta, int which_io, unsigned paddr, unsigned sz);
	void (*v2_dumb_munmap)(char *virta, unsigned size);

	int (*v2_dev_open)(char *devpath);
	void (*v2_dev_close)(int d);
	int (*v2_dev_read)(int d, char *buf, int nbytes);
	int (*v2_dev_write)(int d, const char *buf, int nbytes);
	int (*v2_dev_seek)(int d, int hi, int lo);

	
	void (*v2_wheee2)(void);
	void (*v2_wheee3)(void);
};

struct linux_mlist_v0 {
	struct linux_mlist_v0 *theres_more;
	unsigned int start_adr;
	unsigned num_bytes;
};

struct linux_mem_v0 {
	struct linux_mlist_v0 **v0_totphys;
	struct linux_mlist_v0 **v0_prommap;
	struct linux_mlist_v0 **v0_available; 
};

struct linux_arguments_v0 {
	char *argv[8];
	char args[100];
	char boot_dev[2];
	int boot_dev_ctrl;
	int boot_dev_unit;
	int dev_partition;
	char *kernel_file_name;
	void *aieee1;           
};

struct linux_bootargs_v2 {
	char **bootpath;
	char **bootargs;
	int *fd_stdin;
	int *fd_stdout;
};

struct linux_romvec {
	
	unsigned int pv_magic_cookie;
	unsigned int pv_romvers;
	unsigned int pv_plugin_revision;
	unsigned int pv_printrev;

	
	struct linux_mem_v0 pv_v0mem;

	
	struct linux_nodeops *pv_nodeops;

	char **pv_bootstr;
	struct linux_dev_v0_funcs pv_v0devops;

	char *pv_stdin;
	char *pv_stdout;
#define	PROMDEV_KBD	0		
#define	PROMDEV_SCREEN	0		
#define	PROMDEV_TTYA	1		
#define	PROMDEV_TTYB	2		

	
	int (*pv_getchar)(void);
	void (*pv_putchar)(int ch);

	
	int (*pv_nbgetchar)(void);
	int (*pv_nbputchar)(int ch);

	void (*pv_putstr)(char *str, int len);

	
	void (*pv_reboot)(char *bootstr);
	void (*pv_printf)(__const__ char *fmt, ...);
	void (*pv_abort)(void);
	__volatile__ int *pv_ticks;
	void (*pv_halt)(void);
	void (**pv_synchook)(void);

	
	union {
		void (*v0_eval)(int len, char *str);
		void (*v2_eval)(char *str);
	} pv_fortheval;

	struct linux_arguments_v0 **pv_v0bootargs;

	
	unsigned int (*pv_enaddr)(int d, char *enaddr);

	struct linux_bootargs_v2 pv_v2bootargs;
	struct linux_dev_v2_funcs pv_v2devops;

	int filler[15];

	
	void (*pv_setctxt)(int ctxt, char *va, int pmeg);


	int (*v3_cpustart)(unsigned int whichcpu, int ctxtbl_ptr,
			   int thiscontext, char *prog_counter);

	int (*v3_cpustop)(unsigned int whichcpu);

	int (*v3_cpuidle)(unsigned int whichcpu);

	int (*v3_cpuresume)(unsigned int whichcpu);
};

struct linux_nodeops {
	phandle (*no_nextnode)(phandle node);
	phandle (*no_child)(phandle node);
	int (*no_proplen)(phandle node, const char *name);
	int (*no_getprop)(phandle node, const char *name, char *val);
	int (*no_setprop)(phandle node, const char *name, char *val, int len);
	char * (*no_nextprop)(phandle node, char *name);
};

#if defined(__sparc__) && defined(__arch64__)
#define PROMREG_MAX     24
#define PROMVADDR_MAX   16
#define PROMINTR_MAX    32
#else
#define PROMREG_MAX     16
#define PROMVADDR_MAX   16
#define PROMINTR_MAX    15
#endif

struct linux_prom_registers {
	unsigned int which_io;	
	unsigned int phys_addr;	
	unsigned int reg_size;	
};

struct linux_prom64_registers {
	unsigned long phys_addr;
	unsigned long reg_size;
};

struct linux_prom_irqs {
	int pri;    
	int vector; 
};

struct linux_prom_ranges {
	unsigned int ot_child_space;
	unsigned int ot_child_base;		
	unsigned int ot_parent_space;
	unsigned int ot_parent_base;		
	unsigned int or_size;
};

#if defined(__sparc__) && defined(__arch64__)
struct linux_prom_pci_registers {
	unsigned int phys_hi;
	unsigned int phys_mid;
	unsigned int phys_lo;

	unsigned int size_hi;
	unsigned int size_lo;
};
#else
struct linux_prom_pci_registers {
	unsigned int which_io;  

	unsigned int phys_hi;
	unsigned int phys_lo;

	unsigned int size_hi;
	unsigned int size_lo;
};

#endif

struct linux_prom_pci_ranges {
	unsigned int child_phys_hi;	
	unsigned int child_phys_mid;
	unsigned int child_phys_lo;

	unsigned int parent_phys_hi;
	unsigned int parent_phys_lo;

	unsigned int size_hi;
	unsigned int size_lo;
};

struct linux_prom_pci_intmap {
	unsigned int phys_hi;
	unsigned int phys_mid;
	unsigned int phys_lo;

	unsigned int interrupt;

	int          cnode;
	unsigned int cinterrupt;
};

struct linux_prom_pci_intmask {
	unsigned int phys_hi;
	unsigned int phys_mid;
	unsigned int phys_lo;
	unsigned int interrupt;
};

#endif 

#endif 
