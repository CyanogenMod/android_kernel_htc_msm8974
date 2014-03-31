#ifndef __SPARC_OPENPROM_H
#define __SPARC_OPENPROM_H

/* openprom.h:  Prom structures and defines for access to the OPENBOOT
 *              prom routines and data areas.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */


#ifdef CONFIG_SUN3
#define KADB_DEBUGGER_BEGVM     0x0fee0000    
#define LINUX_OPPROM_BEGVM      0x0fef0000
#define LINUX_OPPROM_ENDVM      0x0ff10000    
#else
#define KADB_DEBUGGER_BEGVM     0xffc00000    
#define	LINUX_OPPROM_BEGVM	0xffd00000
#define	LINUX_OPPROM_ENDVM	0xfff00000
#define	LINUX_OPPROM_MAGIC      0x10010407
#endif

#ifndef __ASSEMBLY__
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
	int (*v2_inst2pkg)(int d);	
	char * (*v2_dumb_mem_alloc)(char *va, unsigned sz);
	void (*v2_dumb_mem_free)(char *va, unsigned sz);

	
	char * (*v2_dumb_mmap)(char *virta, int which_io, unsigned paddr, unsigned sz);
	void (*v2_dumb_munmap)(char *virta, unsigned size);

	int (*v2_dev_open)(char *devpath);
	void (*v2_dev_close)(int d);
	int (*v2_dev_read)(int d, char *buf, int nbytes);
	int (*v2_dev_write)(int d, char *buf, int nbytes);
	int (*v2_dev_seek)(int d, int hi, int lo);

	
	void (*v2_wheee2)(void);
	void (*v2_wheee3)(void);
};

struct linux_mlist_v0 {
	struct linux_mlist_v0 *theres_more;
	char *start_adr;
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

#if defined(CONFIG_SUN3) || defined(CONFIG_SUN3X)
struct linux_romvec {
	char		*pv_initsp;
	int		(*pv_startmon)(void);

	int		*diagberr;

	struct linux_arguments_v0 **pv_v0bootargs;
	unsigned	*pv_sun3mem;

	unsigned char	(*pv_getchar)(void);
	int		(*pv_putchar)(int ch);
	int		(*pv_nbgetchar)(void);
	int		(*pv_nbputchar)(int ch);
	unsigned char	*pv_echo;
	unsigned char	*pv_insource;
	unsigned char	*pv_outsink;

	int		(*pv_getkey)(void);
	int		(*pv_initgetkey)(void);
	unsigned int	*pv_translation;
	unsigned char	*pv_keybid;
	int		*pv_screen_x;
	int		*pv_screen_y;
	struct keybuf	*pv_keybuf;

	char		*pv_monid;


	int		(*pv_fbwritechar)(char);
	int		*pv_fbaddr;
	char		**pv_font;
	int		(*pv_fbwritestr)(char);

	void		(*pv_reboot)(char *bootstr);


	unsigned char	*pv_linebuf;
	unsigned char	**pv_lineptr;
	int		*pv_linesize;
	int		(*pv_getline)(void);
	unsigned char	(*pv_getnextchar)(void);
	unsigned char	(*pv_peeknextchar)(void);
	int		*pv_fbthere;
	int		(*pv_getnum)(void);

	void		(*pv_printf)(const char *fmt, ...);
	int		(*pv_printhex)(void);

	unsigned char	*pv_leds;
	int		(*pv_setleds)(void);


	int		(*pv_nmiaddr)(void);
	int		(*pv_abortentry)(void);
	int		*pv_nmiclock;

	int		*pv_fbtype;


	unsigned	pv_romvers;
	struct globram  *pv_globram;
	char		*pv_kbdzscc;

	int		*pv_keyrinit;
	unsigned char	*pv_keyrtick;
	unsigned	*pv_memoryavail;
	long		*pv_resetaddr;
	long		*pv_resetmap;

	void		(*pv_halt)(void);
	unsigned char	*pv_memorybitmap;

#ifdef CONFIG_SUN3
	void		(*pv_setctxt)(int ctxt, char *va, int pmeg);
	void		(*pv_vector_cmd)(void);
	int		dummy1z;
	int		dummy2z;
	int		dummy3z;
	int		dummy4z;
#endif
};
#else
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
#endif

struct linux_nodeops {
	int (*no_nextnode)(int node);
	int (*no_child)(int node);
	int (*no_proplen)(int node, char *name);
	int (*no_getprop)(int node, char *name, char *val);
	int (*no_setprop)(int node, char *name, char *val, int len);
	char * (*no_nextprop)(int node, char *name);
};

#define PROMREG_MAX     16
#define PROMVADDR_MAX   16
#define PROMINTR_MAX    15

struct linux_prom_registers {
	int which_io;         
	char *phys_addr;      
	int reg_size;         
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

#endif 

#endif 
